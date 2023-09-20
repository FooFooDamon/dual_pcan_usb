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

#include "packet_codec.h"

#define PCAN_USB_STATE_CONNECTED            ((u8)0x01)
#define PCAN_USB_STATE_STARTED              ((u8)0x02)

typedef struct usb_forwarder
{
    struct can_priv can; /* NOTE: MUST be 1st field, see implementation of alloc_candev(). */
    struct usb_device *usb_dev;
    struct usb_interface *usb_intf;
    struct net_device *net_dev;
    u8 *cmd_buf;
    struct timer_list restart_timer;
    struct pcan_time_ref time_ref;
    u8 state;
} usb_forwarder_t;

int usbdrv_register(void);

void usbdrv_unregister(void);

int usbdrv_bulk_msg_send(usb_forwarder_t *forwarder, void *data, int len);

int usbdrv_bulk_msg_recv(usb_forwarder_t *forwarder, void *data, int len);

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
 */

