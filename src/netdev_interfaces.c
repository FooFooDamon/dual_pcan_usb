/*
 * Implementation of netdev interfaces of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include "netdev_interfaces.h"

#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/can/dev.h>

#include "common.h"
#include "klogging.h"
#include "can_commands.h"
#include "packet_codec.h"
#include "usb_driver.h"

#define __FILE__                        "netdev_interfaces.c"

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

static inline void pcan_dump_mem(char *prompt, void *ptr, int len)
{
    pr_info("dumping %s (%d bytes):\n", (prompt ? prompt : "memory"), len);
    print_hex_dump(KERN_INFO, DEV_NAME " ", DUMP_PREFIX_NONE, 16, 1, ptr, len, false);
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

static void usb_read_bulk_callback(struct urb *urb)
{
    struct net_device *netdev = (struct net_device *)urb->context;
    usb_forwarder_t *forwarder = netdev ? (usb_forwarder_t *)netdev_priv(netdev) : NULL;
    int err = 0;

    if (NULL == forwarder || !netif_device_present(netdev))
        return;

    switch (urb->status)
    {
    case 0:
        break;

    case -EILSEQ:
    case -ENOENT:
    case -ECONNRESET:
    case -ESHUTDOWN:
        return;

    default:
        netdev_err_ratelimited_v(netdev, "Rx urb aborted (%d)\n", urb->status);
        goto resubmit_urb;
    }

    if (urb->actual_length > 0 && (forwarder->state & PCAN_USB_STATE_STARTED))
    {
        err = pcan_decode_and_handle_urb(urb, netdev);
        if (err)
            pcan_dump_mem("received usb message", urb->transfer_buffer, urb->transfer_buffer_length);
    }

resubmit_urb:

    usb_fill_bulk_urb(urb, forwarder->usb_dev, usb_rcvbulkpipe(forwarder->usb_dev, PCAN_USB_EP_MSGIN),
        urb->transfer_buffer, PCAN_USB_RX_BUFFER_SIZE, usb_read_bulk_callback, netdev);

    usb_anchor_urb(urb, &forwarder->anchor_rx_submitted);

    err = usb_submit_urb(urb, GFP_ATOMIC);
    if (!err)
        return;

    usb_unanchor_urb(urb);

    if (-ENODEV == err)
        netif_device_detach(netdev);
    else
        netdev_err_v(netdev, "failed resubmitting read bulk urb: %d\n", err);
}

static void usb_write_bulk_callback(struct urb *urb)
{
    pcan_tx_urb_context_t *ctx = (pcan_tx_urb_context_t *)urb->context;
    usb_forwarder_t *forwarder = ctx ? ctx->forwarder : NULL;
    struct net_device *netdev = forwarder ? forwarder->net_dev : NULL;

    if (NULL == ctx)
        return;

    atomic_dec(&forwarder->active_tx_urbs);

    if (!netif_device_present(netdev))
        return;

    switch (urb->status)
    {
    case 0:
        /* transmission complete */
        ++netdev->stats.tx_packets;
        netdev->stats.tx_bytes += ctx->data_len;
        /* prevent tx timeout */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 1, 15)
        netdev->trans_start = jiffies;
#else
        netif_trans_update(netdev);
#endif
        break;

    default:
        netdev_err_ratelimited_v(netdev, "Tx urb aborted (%d)\n", urb->status);
        /* FIXME: fallthrough */

    case -EPROTO:
    case -ENOENT:
    case -ECONNRESET:
    case -ESHUTDOWN:
        break;
    }

    /* should always release echo skb and corresponding context */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 1, 15)
    can_get_echo_skb(netdev, ctx->echo_index);
#else
    can_get_echo_skb(netdev, ctx->echo_index, NULL);
#endif
    ctx->echo_index = PCAN_USB_MAX_TX_URBS;

    /* do wakeup tx queue in case of success only */
    if (!urb->status)
        netif_wake_queue(netdev);
}

static int start_can_interface(struct net_device *netdev)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)netdev_priv(netdev);
    struct usb_device *usb_dev = forwarder->usb_dev;
    u16 dev_revision = le16_to_cpu(usb_dev->descriptor.bcdDevice) >> 8;
    int err = 0;
    int i;

    /* allocate rx urbs and submit them */
    for (i = 0; i < PCAN_USB_MAX_RX_URBS; ++i)
    {
        struct urb *urb = usb_alloc_urb(0, GFP_KERNEL);
        u8 *buf = urb ? kmalloc(PCAN_USB_RX_BUFFER_SIZE, GFP_KERNEL) : NULL;

        if (NULL == urb)
        {
            netdev_err_v(netdev, "No memory left for URBs\n");
            err = -ENOMEM;
            break;
        }

        if (NULL == buf)
        {
            usb_free_urb(urb);
            err = -ENOMEM;
            break;
        }

        usb_fill_bulk_urb(urb, usb_dev, usb_rcvbulkpipe(usb_dev, PCAN_USB_EP_MSGIN),
            buf, PCAN_USB_RX_BUFFER_SIZE, usb_read_bulk_callback, netdev);

        /* ask last usb_free_urb() to also kfree() transfer_buffer */
        urb->transfer_flags |= URB_FREE_BUFFER;
        usb_anchor_urb(urb, &forwarder->anchor_rx_submitted);

        err = usb_submit_urb(urb, GFP_KERNEL);
        if (err)
        {
            if (-ENODEV == err)
                netif_device_detach(netdev);

            usb_unanchor_urb(urb);
            kfree(buf);
            usb_free_urb(urb);

            break;
        }

        /* drop reference, USB core will take care of freeing it */
        usb_free_urb(urb);
    }

    if (0 == i)
    {
        netdev_err_v(netdev, "couldn't setup any rx URB\n");
        return err;
    }

    if (i < PCAN_USB_MAX_RX_URBS)
        netdev_warn_v(netdev, "rx performance may be slow\n");

    /* allocate tx urbs */
    for (i = 0; i < PCAN_USB_MAX_TX_URBS; ++i)
    {
        pcan_tx_urb_context_t *ctx = forwarder->tx_contexts + i;
        struct urb *urb = usb_alloc_urb(0, GFP_KERNEL);
        u8 *buf = urb ? kmalloc(PCAN_USB_TX_BUFFER_SIZE, GFP_KERNEL) : NULL;

        if (NULL == urb)
        {
            netdev_err_v(netdev, "No memory left for URBs\n");
            err = -ENOMEM;
            break;
        }

        if (NULL == buf)
        {
            usb_free_urb(urb);
            err = -ENOMEM;
            break;
        }

        ctx->forwarder = forwarder;
        ctx->urb = urb;

        usb_fill_bulk_urb(urb, usb_dev, usb_sndbulkpipe(usb_dev, PCAN_USB_EP_MSGOUT),
            buf, PCAN_USB_TX_BUFFER_SIZE, usb_write_bulk_callback, ctx);

        /* ask last usb_free_urb() to also kfree() transfer_buffer */
        urb->transfer_flags |= URB_FREE_BUFFER;
    }

    if (0 == i)
    {
        netdev_err_v(netdev, "couldn't setup any tx URB\n");
        goto lbl_kill_urbs;
    }

    if (i < PCAN_USB_MAX_TX_URBS)
        netdev_warn_v(netdev, "tx performance may be slow\n");

    /* TODO: Needed or not: memset(&forwarder->time_ref, 0, sizeof(forwarder->time_ref)); */
    err = (dev_revision > 3) ? pcan_cmd_set_silent(forwarder, forwarder->can.ctrlmode & CAN_CTRLMODE_LISTENONLY) : 0;
    if (err || (err = pcan_cmd_set_ext_vcc(forwarder, /* is_on = */0)))
        goto lbl_start_failed;

    forwarder->state |= PCAN_USB_STATE_STARTED;

    err = usbdrv_reset_bus(forwarder, /* is_on = */1);
    if (err)
        goto lbl_start_failed;

    forwarder->can.state = CAN_STATE_ERROR_ACTIVE;

    return 0;

lbl_start_failed:

    if (-ENODEV == err)
        netif_device_detach(netdev);

    netdev_warn_v(netdev, "couldn't submit control: %d\n", err);

    for (i = 0; i < PCAN_USB_MAX_TX_URBS; ++i)
    {
        usb_free_urb(forwarder->tx_contexts[i].urb);
        forwarder->tx_contexts[i].urb = NULL;
    }

lbl_kill_urbs:

    usb_kill_anchored_urbs(&forwarder->anchor_rx_submitted);

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

    forwarder->state &= ~PCAN_USB_STATE_STARTED;
    netif_stop_queue(netdev);

    usbdrv_unlink_all_urbs(forwarder);

    close_candev(netdev);
    forwarder->can.state = CAN_STATE_STOPPED;

    return usbdrv_reset_bus(forwarder, /* is_on = */0)/* FIXME: Needed or not? */;
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

    if (can_dropped_invalid_skb(netdev, skb))
        return NETDEV_TX_OK;

    for (i = 0; i < PCAN_USB_MAX_TX_URBS; ++i)
    {
        if (PCAN_USB_MAX_TX_URBS == forwarder->tx_contexts[i].echo_index)
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

    ctx->echo_index = i;
    ctx->data_len = frame->can_dlc;

    usb_anchor_urb(urb, &forwarder->anchor_tx_submitted);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 1, 15)
    can_put_echo_skb(skb, netdev, ctx->echo_index);
#else
    can_put_echo_skb(skb, netdev, ctx->echo_index, 0);
#endif
    atomic_inc(&forwarder->active_tx_urbs);

    err = usb_submit_urb(urb, GFP_ATOMIC);
    if (err)
    {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 1, 15)
        can_free_echo_skb(netdev, ctx->echo_index);
#else
        can_free_echo_skb(netdev, ctx->echo_index, NULL);
#endif

        usb_unanchor_urb(urb);

        /* FIXME: this context is not used in fact */
        ctx->echo_index = PCAN_USB_MAX_TX_URBS;

        atomic_dec(&forwarder->active_tx_urbs);

        switch (err)
        {
        case -ENODEV:
            netif_device_detach(netdev);
            break;

        default:
            netdev_warn_ratelimited_v(netdev, "tx urb submitting failed err=%d\n", err);
            /* FIXME: fallthrough */
            /* __attribute__((fallthrough)); */

        case -ENOENT:
            ++stats->tx_dropped; /* cable unplugged */
        }
    }
    else
    {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 1, 15)
        netdev->trans_start = jiffies;
#else
        netif_trans_update(netdev);
#endif

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
 */

