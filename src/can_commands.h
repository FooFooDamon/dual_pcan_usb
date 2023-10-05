/* SPDX-License-Identifier: GPL-2.0 */

/*
 * CAN command interfaces of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#ifndef __CAN_COMMANDS_H__
#define __CAN_COMMANDS_H__

#include <linux/types.h> /* For u8, u32, etc. */

#define PCAN_USB_MAX_CMD_LEN        32

enum pcan_cmd_arg_index
{
    PCAN_CMD_ARG_INDEX_FUNC =       0
    , PCAN_CMD_ARG_INDEX_NUM =      1
    , PCAN_CMD_ARG_INDEX_ARG =      2
};

typedef struct pcan_cmd_holder
{
    u8 functionality;
    u8 number;
    /*int timeout_ms;*/
    void *args;
    void *result;
    void *complete_func; /* actual type is usb_complete_t */
    void *context;
} pcan_cmd_holder_t;

struct usb_forwarder;

int pcan_oneway_command(struct usb_forwarder *forwarder, pcan_cmd_holder_t *cmd_holder);

int pcan_oneway_command_async(struct usb_forwarder *forwarder, pcan_cmd_holder_t *cmd_holder);

#define pcan_command_get            pcan_oneway_command

int pcan_responsive_command(struct usb_forwarder *forwarder, pcan_cmd_holder_t *cmd_holder);

#define pcan_command_set            pcan_responsive_command

#define CMD_HOLDER_OF_SET_SAJ1000(_args, ...)           { .functionality = 9, .number = 2, .args = _args, ##__VA_ARGS__ }
int pcan_cmd_set_sja1000(struct usb_forwarder *forwarder, u8 mode);
int pcan_cmd_set_sja1000_async(struct usb_forwarder *forwarder, u8 mode, void *complete_func, void *context);

#define SJA1000_MODE_NORMAL         0x00
#define SJA1000_MODE_INIT           0x01

#define pcan_init_sja1000(fwd)      pcan_cmd_set_sja1000(fwd, SJA1000_MODE_INIT)

#define CMD_HOLDER_OF_SET_BUS(_args, ...)               { .functionality = 3, .number = 2, .args = _args, ##__VA_ARGS__ }
int pcan_cmd_set_bus(struct usb_forwarder *forwarder, u8 is_on);
int pcan_cmd_set_bus_async(struct usb_forwarder *forwarder, u8 is_on, void *complete_func, void *context);

#define CMD_HOLDER_OF_SET_SILENT(_args, ...)            { .functionality = 3, .number = 3, .args = _args, ##__VA_ARGS__ }
int pcan_cmd_set_silent(struct usb_forwarder *forwarder, u8 is_on);
int pcan_cmd_set_silent_async(struct usb_forwarder *forwarder, u8 is_on, void *complete_func, void *context);

#define CMD_HOLDER_OF_SET_EXT_VCC(_args, ...)           { .functionality = 10, .number = 2, .args = _args, ##__VA_ARGS__ }
int pcan_cmd_set_ext_vcc(struct usb_forwarder *forwarder, u8 is_on);
int pcan_cmd_set_ext_vcc_async(struct usb_forwarder *forwarder, u8 is_on, void *complete_func, void *context);

struct can_bittiming;

#define CMD_HOLDER_OF_SET_BTR0BTR1(_args, ...)          { .functionality = 1, .number = 2, .args = _args, ##__VA_ARGS__ }
int pcan_cmd_set_btr0btr1(struct usb_forwarder *forwarder, u8 btr0, u8 btr1);
int pcan_cmd_set_btr0btr1_async(struct usb_forwarder *forwarder, u8 btr0, u8 btr1, void *complete_func, void *context);
#define CMD_HOLDER_OF_SET_BITRATE                       CMD_HOLDER_OF_SET_BTR0BTR1
int pcan_cmd_set_bitrate(struct usb_forwarder *forwarder, u32 bitrate);
int pcan_cmd_set_bitrate_async(struct usb_forwarder *forwarder, u32 bitrate, void *complete_func, void *context);
#define CMD_HOLDER_OF_SET_BITTIMING                     CMD_HOLDER_OF_SET_BTR0BTR1
int pcan_cmd_set_bittiming(struct usb_forwarder *forwarder, struct can_bittiming *bt);
int pcan_cmd_set_bittiming_async(struct usb_forwarder *forwarder, struct can_bittiming *bt, void *complete_func, void *context);

#define CMD_HOLDER_OF_GET_SERIAL_NUMBER(_result, ...)   { .functionality = 6, .number = 1, .result = _result, ##__VA_ARGS__ }
int pcan_cmd_get_serial_number(struct usb_forwarder *forwarder, u32 *serial_number);

#define CMD_HOLDER_OF_GET_DEVICE_ID(_result, ...)       { .functionality = 4, .number = 1, .result = _result, ##__VA_ARGS__ }
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
 *
 * >>> 2023-09-20, Man Hung-Coeng <udc577@126.com>:
 *  01. Make definition of enum pcan_cmd_arg_index public.
 *
 * >>> 2023-10-01, Man Hung-Coeng <udc577@126.com>:
 *  01. Delete pcan_fill_*().
 *  02. Add function pcan_oneway_command_async() and pcan_cmd_set_*_async().
 *  03. Add macro CMD_HOLDER_OF_*().
 *
 * >>> 2023-10-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Add macro CMD_HOLDER_OF_SET_{BTR0BTR1,BITRATE}().
 *  02. Add function pcan_cmd_set_{btr0btr1,bitrate}[_async]().
 *  03. Change license to GPL-2.0.
 */

