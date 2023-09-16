/*
 * CAN command interfaces of PCAN-USB.
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

void pcan_fill_command_buffer(u8 functionality, u8 number, const void *args_ptr, u8 args_len, void *buf);

int pcan_oneway_command(struct usb_forwarder *forwarder, pcan_cmd_holder_t *cmd_holder);

#define pcan_command_get            pcan_oneway_command

int pcan_responsive_command(struct usb_forwarder *forwarder, pcan_cmd_holder_t *cmd_holder);

#define pcan_command_set            pcan_responsive_command

void pcan_fill_cmdbuf_for_setting_sja1000(u8 mode, void *buf);
int pcan_cmd_set_sja1000(struct usb_forwarder *forwarder, u8 mode);

#define SJA1000_MODE_NORMAL         0x00
#define SJA1000_MODE_INIT           0x01

#define pcan_init_sja1000(fwd)      pcan_cmd_set_sja1000(fwd, SJA1000_MODE_INIT)

void pcan_fill_cmdbuf_for_setting_bus(u8 is_on, void *buf);
int pcan_cmd_set_bus(struct usb_forwarder *forwarder, u8 is_on);

void pcan_fill_cmdbuf_for_setting_silent(u8 is_on, void *buf);
int pcan_cmd_set_silent(struct usb_forwarder *forwarder, u8 is_on);

void pcan_fill_cmdbuf_for_setting_ext_vcc(u8 is_on, void *buf);
int pcan_cmd_set_ext_vcc(struct usb_forwarder *forwarder, u8 is_on);

struct can_bittiming;

int pcan_cmd_set_bittiming(struct usb_forwarder *forwarder, struct can_bittiming *bt);

void pcan_fill_cmdbuf_for_getting_serial_number(void *buf);
int pcan_cmd_get_serial_number(struct usb_forwarder *forwarder, u32 *serial_number);

void pcan_fill_cmdbuf_for_getting_device_id(void *buf);
int pcan_cmd_get_device_id(struct usb_forwarder *forwarder, u32 *device_id);

#endif /* #ifndef __CAN_COMMANDS_H__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-09-11, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-09-16, Man Hung-Coeng <udc577@126.com>:
 *  01. Rename pcan_{set,get}_*() to pcan_cmd_{set,get}_*().
 *  02. Add pcan_fill_*().
 */

