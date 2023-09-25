/*
 * Structures and declarations of USB submodule of PCAN-USB.
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

#ifndef __USB_DRIVER_H__
#define __USB_DRIVER_H__

#include <linux/skbuff.h> /* Fix compilation errors due to some inline functions of old <linux/can/dev.h>. */
#include <linux/netdevice.h> /* Same as above. */
#include <linux/can/dev.h> /* struct can_priv */
#include <linux/usb.h> /* struct urb, usb_* */

#include "packet_codec.h"

#define PCAN_USB_STATE_CONNECTED            ((u8)0x01)
#define PCAN_USB_STATE_STARTED              ((u8)0x02)

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
    u8 data_len;
} pcan_tx_urb_context_t;

typedef struct usb_forwarder
{
    struct can_priv can; /* NOTE: MUST be 1st field, see implementation of alloc_candev(). */
    struct usb_device *usb_dev;
    struct usb_interface *usb_intf;
    struct net_device *net_dev;
    u8 *cmd_buf;
    struct usb_anchor anchor_rx_submitted;
    struct usb_anchor anchor_tx_submitted;
    atomic_t active_tx_urbs;
    pcan_tx_urb_context_t tx_contexts[PCAN_USB_MAX_TX_URBS];
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
 */

