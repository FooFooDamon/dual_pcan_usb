/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Structures and declarations of USB submodule of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#ifndef __USB_DRIVER_H__
#define __USB_DRIVER_H__

#include <linux/skbuff.h> /* Fix compilation errors due to some inline functions of old <linux/can/dev.h>. */
#include <linux/netdevice.h> /* Same as above. */
#include <linux/can/dev.h> /* struct can_priv */
#include <linux/usb.h> /* struct urb, usb_* */

#include "chardev_interfaces.h" /* struct pcan_chardev */
#include "packet_codec.h" /* struct pcan_time_ref */

#define PCAN_USB_STATE_CONNECTED            ((u8)0x01)
#define PCAN_USB_STATE_STARTED              ((u8)0x02)

#define PCAN_USB_STARTUP_TIMEOUT_MS         10

#define PCAN_USB_MAX_TX_URBS                10
#define PCAN_USB_MAX_RX_URBS                4

/* PCAN-USB rx/tx buffers size */
#define PCAN_USB_RX_BUFFER_SIZE             64
#define PCAN_USB_TX_BUFFER_SIZE             64

#define PCAN_USB_EP_CMDOUT                  1
#define PCAN_USB_EP_CMDIN                   (PCAN_USB_EP_CMDOUT | USB_DIR_IN)
#define PCAN_USB_EP_MSGOUT                  2
#define PCAN_USB_EP_MSGIN                   (PCAN_USB_EP_MSGOUT | USB_DIR_IN)

struct usb_forwarder;

typedef struct pcan_tx_urb_context
{
    struct urb *urb;
    struct usb_forwarder *forwarder;
    u32 echo_index; /* FIXME: What is this field for? */
} pcan_tx_urb_context_t;

typedef struct usb_forwarder
{
    struct can_priv can; /* NOTE: MUST be 1st field, see implementation of alloc_candev(). */
    struct net_device *net_dev;
    struct pcan_chardev char_dev;
    struct usb_device *usb_dev;
    struct usb_interface *usb_intf;
    u8 *cmd_buf;
    struct usb_anchor anchor_rx_submitted;
    struct usb_anchor anchor_tx_submitted;
    pcan_tx_urb_context_t tx_contexts[PCAN_USB_MAX_TX_URBS];
    atomic_t active_tx_urbs;
    atomic_t flags;
    struct timer_list restart_timer;
    struct pcan_time_ref time_ref;
    u8 state;
} usb_forwarder_t;

int usbdrv_register(void);

void usbdrv_unregister(void);

int usbdrv_bulk_msg_send(usb_forwarder_t *forwarder, void *data, int len);

int usbdrv_bulk_msg_recv(usb_forwarder_t *forwarder, void *data, int len);

int usbdrv_reset_bus(usb_forwarder_t *forwarder, unsigned char is_on);

void usbdrv_unlink_all_urbs(usb_forwarder_t *forwarder);

static inline void usbdrv_default_completion(struct urb *urb)
{
    if (!(urb->transfer_flags & URB_FREE_BUFFER))
        kfree(urb->transfer_buffer);

    usb_free_urb(urb);
}

#endif /* #ifndef __USB_DRIVER_H__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-09-03, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-09-11, Man Hung-Coeng <udc577@126.com>:
 *  01. Make struct usb_forwarder a public structure,
 *      and add some CAN-setting-related fields to it.
 *  02. Add usbdrv_bulk_msg_{send,recv}().
 *
 * >>> 2023-09-20, Man Hung-Coeng <udc577@126.com>:
 *  01. Add a new field time_ref to struct usb_forwarder.
 *
 * >>> 2023-09-25, Man Hung-Coeng <udc577@126.com>:
 *  01. Add URB-related fields (of usb_forwarder_t), structures and macros.
 *  02. Add function usbdrv_reset_bus() and usbdrv_unlink_all_urbs().
 *
 * >>> 2023-10-01, Man Hung-Coeng <udc577@126.com>:
 *  01. Add macro PCAN_USB_STARTUP_TIMEOUT_MS.
 *  02. Add inline function usbdrv_default_completion().
 *
 * >>> 2023-10-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Change license to GPL-2.0.
 *
 * >>> 2023-10-25, Man Hung-Coeng <udc577@126.com>:
 *  01. Delete the field data_len from struct pcan_tx_urb_context.
 *
 * >>> 2023-11-08, Man Hung-Coeng <udc577@126.com>:
 *  01. Re-order some fields of struct forwarder, and add a new one char_dev.
 */

