/*
 * CAN command interfaces.
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

#ifndef __CAN_COMMANDS_H__
#define __CAN_COMMANDS_H__

#include <linux/types.h> /* For u8, u32, etc. */

typedef struct pcan_cmd_holder
{
    u8 functionality;
    u8 number;
    /*int timeout_ms;*/
    void *args;
    void *result;
} pcan_cmd_holder_t;

struct usb_forwarder;

int pcan_oneway_command(struct usb_forwarder *forwarder, pcan_cmd_holder_t *cmd_holder);

#define pcan_command_get            pcan_oneway_command

int pcan_responsive_command(struct usb_forwarder *forwarder, pcan_cmd_holder_t *cmd_holder);

#define pcan_command_set            pcan_responsive_command

int pcan_set_sja1000(struct usb_forwarder *forwarder, u8 mode);

#define SJA1000_MODE_NORMAL         0x00
#define SJA1000_MODE_INIT           0x01

#define pcan_init_sja1000(fwd)      pcan_set_sja1000(fwd, SJA1000_MODE_INIT)

int pcan_set_bus(struct usb_forwarder *forwarder, u8 is_on);

int pcan_set_silent(struct usb_forwarder *forwarder, u8 is_on);

int pcan_set_ext_vcc(struct usb_forwarder *forwarder, u8 is_on);

struct can_bittiming;

int pcan_set_bittiming(struct usb_forwarder *forwarder, struct can_bittiming *bt);

int pcan_get_serial_number(struct usb_forwarder *forwarder, u32 *serial_number);

int pcan_get_device_id(struct usb_forwarder *forwarder, u32 *device_id);

#endif /* #ifndef __CAN_COMMANDS_H__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-09-11, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 */

