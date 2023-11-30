// SPDX-License-Identifier: GPL-2.0

/*
 * Implementation of USB submodule of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#include "usb_driver.h"

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/can/dev.h>
#include <linux/timer.h>

#include "common.h"
#include "klogging.h"
#include "can_commands.h"
#include "netdev_interfaces.h"
#include "chardev_interfaces.h"
#include "chardev_group.h"
#include "evol_kernel.h"

#define PCAN_USB_MSG_TIMEOUT_MS         1000

#define DEFAULT_TX_QUEUE_LEN            256
#define DEFAULT_RESTART_MSECS           1000
#define DEFAULT_NET_UP_FLAG             1

static u32 bitrate = DEFAULT_BIT_RATE;
module_param(bitrate, uint, 0644);
MODULE_PARM_DESC(bitrate, " initial nominal bitrate (default: " __stringify(DEFAULT_BIT_RATE) ")");

static u16 txqueuelen = DEFAULT_TX_QUEUE_LEN;
module_param(txqueuelen, ushort, 0644);
MODULE_PARM_DESC(txqueuelen, " transmit queue length of netdev (default: " __stringify(DEFAULT_TX_QUEUE_LEN) ")");

static u16 restart_ms = DEFAULT_RESTART_MSECS;
module_param(restart_ms, ushort, 0644);
MODULE_PARM_DESC(restart_ms, " restart timeout in milliseconds from bus-off state (default: "
    __stringify(DEFAULT_RESTART_MSECS) ")");

static bool net_up = DEFAULT_NET_UP_FLAG;
module_param(net_up, bool, 0644);
MODULE_PARM_DESC(net_up, " whether to bring up network interface right after the cable is plugged in (default: "
    __stringify(DEFAULT_NET_UP_FLAG) ")");

static struct usb_device_id s_usb_ids[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) }
    , {}
};
MODULE_DEVICE_TABLE(usb, s_usb_ids);

static int pcan_usb_plugin(struct usb_interface *interface, const struct usb_device_id *id);
static void pcan_usb_plugout(struct usb_interface *interface);

static struct usb_driver s_driver = {
    .name = __DRVNAME__
    , .id_table = s_usb_ids
    , .probe = pcan_usb_plugin
    , .disconnect = pcan_usb_plugout
};

int usbdrv_register(void)
{
    int ret;

    if (IS_ERR(CHRDEV_GRP_CREATE(__DRVNAME__, DEV_MINOR_BASE, 8, get_file_operations())))
        ret = PTR_ERR(THIS_CHRDEV_GRP);
    else
    {
        if ((ret = usb_register(&s_driver)) < 0)
        {
            pr_err_v("usb_register() failed: %d\n", ret);
            CHRDEV_GRP_DESTROY(NULL);
        }
    }

    return ret;
}

void usbdrv_unregister(void)
{
    usb_deregister(&s_driver);
    CHRDEV_GRP_DESTROY(NULL);
}

int usbdrv_bulk_msg_send(usb_forwarder_t *forwarder, void *data, int len)
{
    return usb_bulk_msg(
        forwarder->usb_dev, usb_sndbulkpipe(forwarder->usb_dev, PCAN_USB_EP_CMDOUT),
        data, len, /* actual_length = */NULL, PCAN_USB_MSG_TIMEOUT_MS
    );
}

int usbdrv_bulk_msg_recv(usb_forwarder_t *forwarder, void *data, int len)
{
    return usb_bulk_msg(
        forwarder->usb_dev, usb_rcvbulkpipe(forwarder->usb_dev, PCAN_USB_EP_CMDIN),
        data, len, /* actual_length = */NULL, PCAN_USB_MSG_TIMEOUT_MS
    );
}

int usbdrv_reset_bus(usb_forwarder_t *forwarder, unsigned char is_on)
{
    int err = pcan_cmd_set_bus(forwarder, is_on);

    pr_notice_v("CAN bus %s, err = %d\n", (is_on ? "ON" : "OFF"), err);

    if (err)
        return err;

    if (is_on)
    {
        /* Need some time to finish initialization. */
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(msecs_to_jiffies(PCAN_USB_STARTUP_TIMEOUT_MS));
    }
    else
        err = pcan_init_sja1000(forwarder);

    return err;
}

static inline void pcan_dump_mem(const char *prompt, void *ptr, int len)
{
    pr_info_v("dumping %s (%d bytes):\n", (prompt ? prompt : "memory"), len);
    print_hex_dump(KERN_INFO, __DRVNAME__ " ", DUMP_PREFIX_NONE, 16, 1, ptr, len, false);
}

static void usb_read_bulk_callback(struct urb *urb)
{
    struct net_device *netdev = (struct net_device *)urb->context;
    usb_forwarder_t *forwarder = netdev ? (usb_forwarder_t *)netdev_priv(netdev) : NULL;
    int stage = forwarder ? atomic_read(&forwarder->stage) : PCAN_USB_STAGE_DISCONNECTED;
    int err = 0;

    if (unlikely(NULL == forwarder) || stage < PCAN_USB_STAGE_ONE_STARTED)
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

    if (urb->actual_length > 0)
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
        netif_device_detach(netdev); /* FIXME: pcan_net_dev_close() ?? */
    else
        netdev_err_v(netdev, "failed resubmitting read bulk urb: %d\n", err);
}

int usbdrv_alloc_urbs(usb_forwarder_t *forwarder)
{
    struct usb_device *usb_dev = forwarder->usb_dev;
    int err = 0;
    const int MAX_TX_URBS = sizeof(forwarder->tx_contexts) / sizeof(forwarder->tx_contexts[0]);
    int i;

    /* allocate rx urbs and submit them */
    for (i = 0; i < PCAN_USB_MAX_RX_URBS; ++i)
    {
        struct urb *urb = usb_alloc_urb(0, GFP_KERNEL);
        u8 *buf = urb ? kmalloc(PCAN_USB_RX_BUFFER_SIZE, GFP_KERNEL) : NULL;

        if (NULL == urb)
        {
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
            buf, PCAN_USB_RX_BUFFER_SIZE, usb_read_bulk_callback, forwarder->net_dev);

        urb->transfer_flags |= URB_FREE_BUFFER; /* ask last usb_free_urb() to also kfree() transfer_buffer */
        usb_anchor_urb(urb, &forwarder->anchor_rx_submitted);

        err = usb_submit_urb(urb, GFP_KERNEL);
        usb_free_urb(urb); /* drop reference, USB core will take care of freeing it */
        if (err)
        {
            usb_unanchor_urb(urb);
            kfree(buf); /* FIXME: Needed or not? */
            break;
        }
    } /* for (i : PCAN_USB_MAX_RX_URBS) */

    if (i < PCAN_USB_MAX_RX_URBS)
    {
        pr_err_v("Not all Rx USBs are allocated, expected %d, allocated %d, last err = %d\n",
            PCAN_USB_MAX_RX_URBS, i, err);
        goto lbl_free_rx_urbs;
    }

    /* allocate tx urbs */
    for (i = 0; i < MAX_TX_URBS; ++i)
    {
        pcan_tx_urb_context_t *ctx = forwarder->tx_contexts + i;
        struct urb *urb = usb_alloc_urb(0, GFP_KERNEL);
        u8 *buf = urb ? kmalloc(PCAN_USB_TX_BUFFER_SIZE, GFP_KERNEL) : NULL;

        if (NULL == urb)
        {
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
        ctx->echo_index = 0;

        /*
         * This just makes an association between urb and buf,
         * the actual complete callback will be specified in xx_open().
         */
        usb_fill_bulk_urb(urb, usb_dev, usb_sndbulkpipe(usb_dev, PCAN_USB_EP_MSGOUT),
            buf, PCAN_USB_TX_BUFFER_SIZE, NULL, ctx);

        urb->transfer_flags |= URB_FREE_BUFFER; /* ask last usb_free_urb() to also kfree() transfer_buffer */
    } /* for (i : MAX_TX_URBS) */

    if (i < MAX_TX_URBS)
    {
        pr_err_v("Not all Tx USBs are allocated, expected %d, allocated %d, last err = %d\n",
            MAX_TX_URBS, i, err);
        goto lbl_free_tx_urbs;
    }

    return 0;

lbl_free_tx_urbs:

    for (i = 0; i < MAX_TX_URBS; ++i)
    {
        usb_free_urb(forwarder->tx_contexts[i].urb);
        forwarder->tx_contexts[i].urb = NULL;
    }

lbl_free_rx_urbs:

    usb_kill_anchored_urbs(&forwarder->anchor_rx_submitted);

    return err;
}

void usbdrv_unlink_all_urbs(usb_forwarder_t *forwarder)
{
    const int MAX_TX_URBS = sizeof(forwarder->tx_contexts) / sizeof(forwarder->tx_contexts[0]);
    int i;

    /* free all Rx (submitted) urbs */
    usb_kill_anchored_urbs(&forwarder->anchor_rx_submitted);

    /* free unsubmitted Tx urbs first */
    for (i = 0; i < MAX_TX_URBS; ++i)
    {
        pcan_tx_urb_context_t *ctx = &forwarder->tx_contexts[i];

        if (!ctx->urb || ctx->echo_index)
        {
            /*
             * this urb is already released or submitted,
             * let usb core free by itself
             */
            continue;
        }

        usb_free_urb(ctx->urb);
        ctx->urb = NULL;
    }

    /* then free all submitted Tx urbs */
    usb_kill_anchored_urbs(&forwarder->anchor_tx_submitted);
    atomic_set(&forwarder->active_tx_urbs, 0);
}

static inline int check_endpoints(const struct usb_interface *interface)
{
    struct usb_host_interface *intf = interface->cur_altsetting;
    int i;

    for (i = 0; i < intf->desc.bNumEndpoints; ++i)
    {
        struct usb_endpoint_descriptor *endpoint = &intf->endpoint[i].desc;

        switch (endpoint->bEndpointAddress)
        {
        case PCAN_USB_EP_CMDOUT:
        case PCAN_USB_EP_CMDIN:
        case PCAN_USB_EP_MSGOUT:
        case PCAN_USB_EP_MSGIN:
            break;

        default:
            return -ENODEV;
        }
    }

    return 0;
}

static void network_up_callback(timer_cb_arg_t arg)
{
#ifdef setup_timer
    usb_forwarder_t *forwarder = (usb_forwarder_t *)arg;
#else
    usb_forwarder_t *forwarder = container_of(arg, usb_forwarder_t, restart_timer);
#endif

    pcan_net_wake_up(forwarder->net_dev);
}

static int check_device_info(usb_forwarder_t *forwarder)
{
    u32 serial_number = 0;
    u32 device_id = 0;
    int err = pcan_cmd_get_serial_number(forwarder, &serial_number);

    if (err < 0)
        return err;
    else
        dev_notice_v(&forwarder->usb_dev->dev, "Got serial number: 0x%08X\n", serial_number);

    if (!(err = pcan_cmd_get_device_id(forwarder, &device_id)))
        dev_notice_v(&forwarder->usb_dev->dev, "Got device id: %u\n", device_id);

    return err;
}

static void destroy_usb_forwarder(struct work_struct *work_info);

static int pcan_usb_plugin(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct net_device *netdev = NULL;
    usb_forwarder_t *forwarder = NULL;
    int err = check_endpoints(interface);

    if (err)
        return err;

    if (NULL == (netdev = alloc_candev(sizeof(usb_forwarder_t), PCAN_USB_MAX_TX_URBS)))
    {
        dev_err_v(&interface->dev, "alloc_candev() failed\n");
        return -ENOMEM;
    }
    SET_NETDEV_DEV(netdev,  &interface->dev); /* Set netdev's parent device. */
    netdev->flags |= IFF_ECHO; /* Support local echo. */
    netdev->tx_queue_len = txqueuelen;
    pcan_net_set_ops(netdev);

    forwarder = netdev_priv(netdev);
    memset(((char *)forwarder) + sizeof(struct can_priv), 0, sizeof(*forwarder) - sizeof(struct can_priv));
    if (NULL == (forwarder->cmd_buf = kmalloc(PCAN_USB_MAX_CMD_LEN, GFP_KERNEL)))
    {
        err = -ENOMEM;
        dev_err_v(&interface->dev, "kmalloc() for cmd_buf failed\n");
        goto lbl_release_res;
    }
    forwarder->net_dev = netdev;
    forwarder->usb_dev = interface_to_usbdev(interface);
    forwarder->usb_intf = interface;
    atomic_set(&forwarder->stage, PCAN_USB_STAGE_CONNECTED);
    atomic_set(&forwarder->pending_cmds, 0);
    INIT_DELAYED_WORK(&forwarder->destroy_work, destroy_usb_forwarder);

    init_usb_anchor(&forwarder->anchor_rx_submitted);
    init_usb_anchor(&forwarder->anchor_tx_submitted);
    atomic_set(&forwarder->active_tx_urbs, 0);
    spin_lock_init(&forwarder->char_dev.lock);
    if ((err = usbdrv_alloc_urbs(forwarder)) < 0)
        goto lbl_release_res;

    evol_setup_timer(&forwarder->restart_timer, network_up_callback, forwarder);

    forwarder->can.clock = *get_fixed_can_clock();
    forwarder->can.bittiming_const = get_can_bittiming_const();
    forwarder->can.do_set_bittiming = pcan_net_set_can_bittiming;
    forwarder->can.do_set_mode = pcan_net_set_can_mode;
    forwarder->can.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES | CAN_CTRLMODE_LISTENONLY;
    forwarder->can.restart_ms = restart_ms;
    forwarder->can.bittiming.bitrate = bitrate;

    if ((err = register_candev(netdev)) < 0)
    {
        dev_err_v(&interface->dev, "couldn't register CAN device: %d\n", err);
        goto lbl_release_res;
    }

    forwarder->char_dev.device = CHRDEV_GRP_MAKE_ITEM(DEV_NAME, forwarder);
    if (IS_ERR(forwarder->char_dev.device))
    {
        err = PTR_ERR(forwarder->char_dev.device);
        dev_err_v(&interface->dev, "Failed to create chardev: %d\n", err);
        goto lbl_unreg_can;
    }

    if ((err = check_device_info(forwarder)) < 0)
        goto lbl_unreg_chardev;

    if ((err = usbdrv_reset_bus(forwarder, /* is_on = */0)) < 0)
        goto lbl_unreg_chardev;

    pcan_cmd_set_bitrate(forwarder, bitrate);
    if (net_up)
        pcan_net_dev_open(netdev);

    usb_set_intfdata(interface, forwarder);

    dev_notice_v(&interface->dev, "New PCAN-USB device plugged in\n");

    return 0;

lbl_unreg_chardev:

    CHRDEV_GRP_UNMAKE_ITEM(forwarder->char_dev.device, NULL);

lbl_unreg_can:

    unregister_candev(netdev);

lbl_release_res:

    if (NULL != forwarder->cmd_buf)
        kfree(forwarder->cmd_buf);

    free_candev(netdev);

    return err;
}

static void destroy_usb_forwarder(struct work_struct *work_info)
{
    struct delayed_work *work = (struct delayed_work *)container_of(work_info, struct delayed_work, work);
    usb_forwarder_t *forwarder = (usb_forwarder_t *)container_of(work, usb_forwarder_t, destroy_work);

    if (atomic_read(&forwarder->pending_cmds) > 0)
    {
        schedule_delayed_work(work, msecs_to_jiffies(PCAN_USB_END_CHECK_INTERVAL_MS));

        return;
    }

    kfree(forwarder->cmd_buf);
    pr_notice_v("PCAN-USB[%s|%s] destroyed\n", netdev_name(forwarder->net_dev), dev_name(forwarder->char_dev.device));
    free_candev(forwarder->net_dev);
}

static void pcan_usb_plugout(struct usb_interface *interface)
{
    usb_forwarder_t *forwarder = usb_get_intfdata(interface);

    if (NULL != forwarder)
    {
        CHRDEV_GRP_UNMAKE_ITEM(forwarder->char_dev.device, NULL);
        unregister_candev(forwarder->net_dev);
        atomic_dec(&forwarder->stage);
        usb_set_intfdata(interface, NULL);
        usbdrv_unlink_all_urbs(forwarder);
        schedule_delayed_work(&forwarder->destroy_work, msecs_to_jiffies(PCAN_USB_END_CHECK_INTERVAL_MS));
        dev_notice_v(&interface->dev, "PCAN-USB device plugged out\n");
    }
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-09-03, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-09-11, Man Hung-Coeng <udc577@126.com>:
 *  01. Remove definition of struct usb_forwarder_t.
 *  02. Add usbdrv_bulk_msg_{send,recv}() and pcan_usb_reset_bus().
 *  03. Add device state management, CAN command buffer management
 *      and device info checking.
 *
 * >>> 2023-09-16, Man Hung-Coeng <udc577@126.com>:
 *  01. Do CAN private settings.
 *
 * >>> 2023-09-25, Man Hung-Coeng <udc577@126.com>:
 *  01. Remove macro PCAN_USB_EP_* and PCAN_USB_MAX_TX_URBS.
 *  02. Rename pcan_usb_reset_bus() to usbdrv_reset_bus().
 *  03. Add global function usbdrv_unlink_all_urbs(),
 *      and add initialization of RX/TX URBs in pcan_usb_plugin().
 *  04. Set net device op callbacks in pcan_usb_plugin().
 *
 * >>> 2023-09-29, Man Hung-Coeng <udc577@126.com>:
 *  01. Use logging APIs of 3rd-party klogging.h.
 *
 * >>> 2023-10-01, Man Hung-Coeng <udc577@126.com>:
 *  01. Remove macro PCAN_USB_STARTUP_TIMEOUT_MS and PCAN_USB_MAX_CMD_LEN.
 *  02. Rename function pcan_usb_restart_callback() to network_up_callback().
 *  03. Set CAN bus off within pcan_usb_plugin().
 *
 * >>> 2023-10-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Implement netdev registration and deregistration,
 *      and do some initializations according to module parameters.
 *  02. Change license to GPL-2.0.
 *
 * >>> 2023-10-08, Man Hung-Coeng <udc577@126.com>:
 *  01. Add a new module parameter to control whether to bring up network
 *      interface right after the cable is plugged in,
 *      and define all the module parameter variables as static variables.
 *
 * >>> 2023-10-11, Man Hung-Coeng <udc577@126.com>:
 *  01. Include a 3rd-party header file evol_kernel.h and use wrappers in it
 *      to replace interfaces/definitions which vary from version to version.
 *
 * >>> 2023-10-25, Man Hung-Coeng <udc577@126.com>:
 *  01. Correct the mistake of zeroing CAN private data in pcan_usb_plugin(),
 *      which results in a null delayed work pointer and thus causes a bunch of
 *      warning messages while closing the candev in pcan_usb_plugout().
 *
 * >>> 2023-11-08, Man Hung-Coeng <udc577@126.com>:
 *  01. Cancel the re-definition of __FILE__.
 *  02. Implement skeleton of character device interface.
 *
 * >>> 2023-11-10, Man Hung-Coeng <udc577@126.com>:
 *  01. Replace DEV_NAME with __DRVNAME__.
 *  02. Remove macro DEFAULT_BIT_RATE.
 *
 * >>> 2023-11-30, Man Hung-Coeng <udc577@126.com>:
 *  01. Add usbdrv_alloc_urbs() for initializing resources needed by chardev
 *      and netdev in the same place.
 *  02. Introduce delayed work mechanism to destroy the forwarder instance.
 */

