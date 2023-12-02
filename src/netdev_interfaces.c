// SPDX-License-Identifier: GPL-2.0

/*
 * Implementation of netdev interfaces of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#include "netdev_interfaces.h"

#include <linux/version.h>
#include <linux/rtnetlink.h> /* For rtnl_lock() and rtnl_unlock() in old versions. */
#include <linux/netdevice.h>
#include <linux/can/dev.h>

#include "common.h"
#include "klogging.h"
#include "can_commands.h"
#include "packet_codec.h"
#include "usb_driver.h"
#include "evol_kernel.h"

#define PCAN_USB_CRYSTAL_HZ             16000000

const struct can_clock* get_fixed_can_clock(void)
{
    static const struct can_clock S_CAN_CLOCK = {
        .freq = PCAN_USB_CRYSTAL_HZ / 2
    };

    return &S_CAN_CLOCK;
}

const struct can_bittiming_const* get_can_bittiming_const(void)
{
    static const struct can_bittiming_const S_CAN_BITTIMING_CONST = {
        .name = "pcan_usb"
        , .tseg1_min = 1
        , .tseg1_max = 16
        , .tseg2_min = 1
        , .tseg2_max = 8
        , .sjw_max = 4
        , .brp_min = 1
        , .brp_max = 64
        , .brp_inc = 1
    };

    return &S_CAN_BITTIMING_CONST;
}

int pcan_net_set_can_bittiming(struct net_device *netdev)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)netdev_priv(netdev);
    int err = pcan_cmd_set_bittiming(forwarder, &forwarder->can.bittiming);

    if (err)
        netdev_err_v(netdev, "couldn't set bitrate (err %d)\n", err);

    return err;
}

void pcan_net_wake_up(struct net_device *netdev)
{
    struct can_priv *can = (struct can_priv *)netdev_priv(netdev);

    can->state = CAN_STATE_ERROR_ACTIVE;
    netif_wake_queue(netdev);
}

int pcan_net_dev_open(struct net_device *netdev)
{
    int err;

    rtnl_lock();
    err = evol_netdev_open(netdev, NULL);
    rtnl_unlock();

    return err;
}

void pcan_net_dev_close(struct net_device *netdev)
{
    rtnl_lock();
    dev_close(netdev); /* NOTE: This function might sleep, DO NOT use it in an interrupt context. */
    rtnl_unlock();
}

static void activate_timer_and_free_urb(struct urb *urb)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)urb->context;

    mod_timer(&forwarder->restart_timer, jiffies + msecs_to_jiffies(PCAN_USB_STARTUP_TIMEOUT_MS));

    usbdrv_default_completion(urb);
}

int pcan_net_set_can_mode(struct net_device *netdev, enum can_mode mode)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)netdev_priv(netdev);
    int err = 0;

    netdev_notice_v(netdev, "mode = %d\n", mode);

    switch (mode)
    {
    case CAN_MODE_START:
        if (timer_pending(&forwarder->restart_timer))
            return -EBUSY;

        err = pcan_cmd_set_bus_async(forwarder, /* is_on = */1, activate_timer_and_free_urb, forwarder);

        break;

    default:
        return -EOPNOTSUPP;
    }

    return err;
}

static void usb_write_bulk_callback(struct urb *urb)
{
    pcan_tx_urb_context_t *ctx = (pcan_tx_urb_context_t *)urb->context;
    usb_forwarder_t *forwarder = ctx ? ctx->forwarder : NULL;
    struct net_device *netdev = forwarder ? forwarder->net_dev : NULL;
    int tx_bytes = 0;

    if (NULL == ctx)
        return;

    atomic_dec(&forwarder->active_tx_urbs);

    if (!netif_device_present(netdev))
        return;

    switch (urb->status)
    {
    case 0:
        /* prevent tx timeout */
        evol_netif_trans_update(netdev);
        break;

    case -EPROTO:
    case -ENOENT:
    case -ECONNRESET:
    case -ESHUTDOWN:
    case -ENODEV:
        break;

    default:
        netdev_err_ratelimited_v(netdev, "Tx urb aborted (%d)\n", urb->status);
        break;
    }

    /* should always release echo skb and corresponding context */
    tx_bytes = evol_can_get_echo_skb(netdev, ctx->echo_index - 1, NULL);
    ctx->echo_index = 0;

    if (!urb->status)
    {
        /* transmission complete */
        atomic_inc(&forwarder->shared_tx_counter);
        ++netdev->stats.tx_packets;
        netdev->stats.tx_bytes += tx_bytes;

        /* do wakeup tx queue in case of success only */
        netif_wake_queue(netdev);
    }
}

static int start_can_interface(struct net_device *netdev)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)netdev_priv(netdev);
    struct usb_device *usb_dev = forwarder->usb_dev;
    int stage = atomic_inc_return(&forwarder->stage);
    u16 dev_revision = le16_to_cpu(usb_dev->descriptor.bcdDevice) >> 8;
    int err = 0;
    int i;

    for (i = 0; i < PCAN_USB_MAX_TX_URBS; ++i)
    {
        forwarder->tx_contexts[i].urb->complete = usb_write_bulk_callback;
    }

    if (stage > PCAN_USB_STAGE_ONE_STARTED)
        goto lbl_start_ok;

    /* FIXME: Needed or not: memset(&forwarder->time_ref, 0, sizeof(forwarder->time_ref)); */
    err = (dev_revision > 3) ? pcan_cmd_set_silent(forwarder, forwarder->can.ctrlmode & CAN_CTRLMODE_LISTENONLY) : 0;
    if (err || (err = pcan_cmd_set_ext_vcc(forwarder, /* is_on = */0)))
        goto lbl_start_failed;

    err = usbdrv_reset_bus(forwarder, /* is_on = */1);
    if (err)
        goto lbl_start_failed;

lbl_start_ok:

    forwarder->can.state = CAN_STATE_ERROR_ACTIVE;

    return 0;

lbl_start_failed:

    atomic_dec(&forwarder->stage);

    if (-ENODEV == err)
        netif_device_detach(netdev);

    return err;
}

static int pcan_net_open(struct net_device *netdev)
{
    int err = open_candev(netdev);

    if (err)
        return err;

    err = start_can_interface(netdev);
    if (err)
    {
        netdev_err_v(netdev, "couldn't start device: %d\n", err);
        close_candev(netdev);
        return err;
    }

    netif_start_queue(netdev);

    return 0;
}

static int pcan_net_stop(struct net_device *netdev)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)netdev_priv(netdev);
    int stage = atomic_dec_return(&forwarder->stage);

    netif_stop_queue(netdev);

    close_candev(netdev);
    forwarder->can.state = CAN_STATE_STOPPED;

    return (stage < PCAN_USB_STAGE_ONE_STARTED) ? usbdrv_reset_bus(forwarder, /* is_on = */0) : 0;
}

static netdev_tx_t pcan_net_start_transmit(struct sk_buff *skb, struct net_device *netdev)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)netdev_priv(netdev);
    pcan_tx_urb_context_t *ctx = NULL;
    struct net_device_stats *stats = &netdev->stats;
    struct can_frame *frame = (struct can_frame *)skb->data;
    struct urb *urb;
    u8 *obuf;
    int i;
    int err;
    size_t size = PCAN_USB_TX_BUFFER_SIZE;

    if (forwarder->can.ctrlmode & CAN_CTRLMODE_LISTENONLY)
    {
        /* FIXME: No netdev_*_once() in old versions. */
        /*netdev_info_once(netdev, "interface in listen only mode, dropping skb\n");*/

        kfree_skb(skb);
        ++stats->tx_dropped;

        return NETDEV_TX_OK;
    }

    if (can_dropped_invalid_skb(netdev, skb))
        return NETDEV_TX_OK;

    for (i = 0; i < PCAN_USB_MAX_TX_URBS; ++i)
    {
        if (!forwarder->tx_contexts[i].echo_index)
        {
            ctx = forwarder->tx_contexts + i;
            break;
        }
    }
    if (!ctx)
        return NETDEV_TX_BUSY; /* should not occur except during restart */

    urb = ctx->urb;
    obuf = urb->transfer_buffer;

    err = pcan_encode_frame_to_buf(netdev, frame, obuf, &size);
    if (err)
    {
        netdev_err_ratelimited_v(netdev, "packet dropped\n");

        dev_kfree_skb(skb);
        ++stats->tx_dropped;

        return NETDEV_TX_OK;
    }

    ctx->echo_index = i + 1;

    usb_anchor_urb(urb, &forwarder->anchor_tx_submitted);
    evol_can_put_echo_skb(skb, netdev, ctx->echo_index - 1, 0);
    atomic_inc(&forwarder->active_tx_urbs);

    err = usb_submit_urb(urb, GFP_ATOMIC);
    if (err)
    {
        evol_can_free_echo_skb(netdev, ctx->echo_index - 1, NULL);

        usb_unanchor_urb(urb);

        /* FIXME: this context is not used in fact */
        ctx->echo_index = 0;

        atomic_dec(&forwarder->active_tx_urbs);

        switch (err)
        {
        case -ENODEV:
            netif_device_detach(netdev); /* FIXME: pcan_net_dev_close() ?? */
            break;

        default:
            netdev_warn_ratelimited_v(netdev, "tx urb submitting failed err=%d\n", err);
            fallthrough;

        case -ENOENT:
            ++stats->tx_dropped; /* cable unplugged */
        }
    }
    else
    {
        evol_netif_trans_update(netdev);

        /* slow down tx path */
        if (atomic_read(&forwarder->active_tx_urbs) >= PCAN_USB_MAX_TX_URBS)
            netif_stop_queue(netdev);
    }

    return NETDEV_TX_OK;
}

void pcan_net_set_ops(struct net_device *netdev)
{
    static const struct net_device_ops S_NET_OPS = {
        .ndo_open = pcan_net_open
        , .ndo_stop = pcan_net_stop
        , .ndo_start_xmit = pcan_net_start_transmit
        , .ndo_change_mtu = can_change_mtu
    };

    netdev->netdev_ops = &S_NET_OPS;
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-09-16, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-09-25, Man Hung-Coeng <udc577@126.com>:
 *  01. Rename netdev_wake_up() to pcan_net_wake_up(),
 *      netdev_set_can_mode() to pcan_net_set_can_mode().
 *  02. Add pcan_net_set_ops() and related net device op callbacks.
 *
 * >>> 2023-09-29, Man Hung-Coeng <udc577@126.com>:
 *  01. Use logging APIs of 3rd-party klogging.h.
 *
 * >>> 2023-10-01, Man Hung-Coeng <udc577@126.com>:
 *  01. Set CAN bus on within pcan_net_set_can_mode().
 *  02. Set CAN silent mode and external VCC within start_can_interface().
 *
 * >>> 2023-10-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Add pcan_net_set_can_bittiming().
 *  02. Change license to GPL-2.0.
 *
 * >>> 2023-10-08, Man Hung-Coeng <udc577@126.com>:
 *  01. Fix some -Wunused-result and -Wimplicit-fallthrough warnings.
 *  02. Add pcan_net_dev_open().
 *
 * >>> 2023-10-11, Man Hung-Coeng <udc577@126.com>:
 *  01. Include a 3rd-party header file evol_kernel.h and use wrappers in it
 *      to replace interfaces/definitions which vary from version to version.
 *
 * >>> 2023-10-25, Man Hung-Coeng <udc577@126.com>:
 *  01. Optimize out the field data_len of struct pcan_tx_urb_context.
 *  02. Drop packets in pcan_net_start_transmit() when in listen-only mode.
 *
 * >>> 2023-11-08, Man Hung-Coeng <udc577@126.com>:
 *  01. Cancel the re-definition of __FILE__.
 *
 * >>> 2023-11-10, Man Hung-Coeng <udc577@126.com>:
 *  01. Replace DEV_NAME with __DRVNAME__.
 *
 * >>> 2023-11-30, Man Hung-Coeng <udc577@126.com>:
 *  01. Remove usb_read_bulk_callback().
 *  02. Remove some resource allocations in start_can_interface()
 *      and reclamations in pcan_net_stop().
 *  03. Add some logics to work with chardev interface.
 *
 * >>> 2023-12-02, Man Hung-Coeng <udc577@126.com>:
 *  01. Fix the wrong working flow of turning on CAN bus in open function.
 */

