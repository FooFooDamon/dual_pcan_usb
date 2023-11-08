// SPDX-License-Identifier: GPL-2.0

/*
 * Implementation of CAN command interfaces of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#include "can_commands.h"

#include "common.h"
#include "klogging.h"
#include "usb_driver.h"

#define PCAN_CMD_ARGS_LEN           14
#define PCAN_CMD_TOTAL_LEN          (PCAN_CMD_ARG_INDEX_ARG + PCAN_CMD_ARGS_LEN)

static void pcan_fill_command_buffer(u8 functionality, u8 number, const void *args_ptr, u8 args_len, void *buf)
{
    char *p = (char *)buf;

    if (args_len > PCAN_CMD_ARGS_LEN)
        args_len = PCAN_CMD_ARGS_LEN;

    memset(p, 0, args_len + PCAN_CMD_ARG_INDEX_ARG);
    p[PCAN_CMD_ARG_INDEX_FUNC] = functionality;
    p[PCAN_CMD_ARG_INDEX_NUM] = number;
    if (NULL != args_ptr)
        memcpy(p + PCAN_CMD_ARG_INDEX_ARG, args_ptr, args_len);
}

int pcan_oneway_command(struct usb_forwarder *forwarder, pcan_cmd_holder_t *cmd_holder)
{
    u8 *buf = forwarder->cmd_buf; /* TODO: Should be allocated at each call? */
    int err = 0;

    /* TODO: Reference counting or something. */

    if (!(forwarder->state & PCAN_USB_STATE_CONNECTED))
    {
        err = -EPERM;
        goto CMD_END;
    }

    /* TODO: Is a lock needed for the only cmd_buf? */

    pcan_fill_command_buffer(cmd_holder->functionality, cmd_holder->number, cmd_holder->args, PCAN_CMD_ARGS_LEN, buf);

    if ((err = usbdrv_bulk_msg_send(forwarder, buf, PCAN_CMD_TOTAL_LEN)) < 0)
    {
        dev_err_v(&forwarder->usb_dev->dev, "sending cmd f=0x%x n=0x%x failure: %d\n",
            cmd_holder->functionality, cmd_holder->number, err);
    }

CMD_END:

    /* TODO: Reference counting or something. */

    return err;
}

int pcan_oneway_command_async(struct usb_forwarder *forwarder, pcan_cmd_holder_t *cmd_holder)
{
    struct urb *urb = usb_alloc_urb(0, GFP_ATOMIC);
    u8 *buf = urb ? kmalloc(PCAN_USB_MAX_CMD_LEN, GFP_ATOMIC) : NULL;
    int err = buf ? 0 : -ENOMEM;

    if (!err)
    {
        pcan_fill_command_buffer(cmd_holder->functionality, cmd_holder->number,
            cmd_holder->args, PCAN_CMD_ARGS_LEN, buf);

        usb_fill_bulk_urb(urb, forwarder->usb_dev, usb_sndbulkpipe(forwarder->usb_dev, PCAN_USB_EP_CMDOUT),
            buf, PCAN_CMD_TOTAL_LEN, cmd_holder->complete_func, cmd_holder->context);

        if (!(err = usb_submit_urb(urb, GFP_ATOMIC)))
            return 0;
    }

    if (!urb)
        usb_free_urb(urb);

    if (!buf)
        kfree(buf);

    return err;
}

int pcan_responsive_command(struct usb_forwarder *forwarder, pcan_cmd_holder_t *cmd_holder)
{
    u8 *buf = forwarder->cmd_buf; /* TODO: Should be allocated at each call? */
    int err = 0;

    /* TODO: Reference counting or something. */

    if (!(forwarder->state & PCAN_USB_STATE_CONNECTED))
    {
        err = -EPERM;
        goto CMD_END;
    }

    cmd_holder->args = NULL;
    if ((err = pcan_oneway_command(forwarder, cmd_holder)) < 0)
        goto CMD_END;

    if ((err = usbdrv_bulk_msg_recv(forwarder, buf, PCAN_CMD_TOTAL_LEN)) < 0)
    {
        dev_err_v(&forwarder->usb_dev->dev, "waiting reply f=0x%x n=0x%x failure: %d\n",
            cmd_holder->functionality, cmd_holder->number, err);
    }
    else if (NULL != cmd_holder->result)
        memcpy(cmd_holder->result, buf + PCAN_CMD_ARG_INDEX_ARG, PCAN_CMD_ARGS_LEN);
    else
        ; /* Nothing. */

CMD_END:

    /* TODO: Reference counting or something. */

    return err;
}

#define __PCAN_ONEWAY_SET_SINGLE_ARG(fwd, name, idx, arg)   \
    u8 args[PCAN_CMD_ARGS_LEN] = { \
        [idx] = arg, \
    }; \
    pcan_cmd_holder_t cmd_holder = CMD_HOLDER_OF_SET_##name(args); \
    return pcan_oneway_command(fwd, &cmd_holder)

#define __PCAN_ONEWAY_SET_SINGLE_ARG_ASYNC(fwd, name, idx, arg, comp_fn, ctx)   \
    u8 args[PCAN_CMD_ARGS_LEN] = { \
        [idx] = arg, \
    }; \
    pcan_cmd_holder_t cmd_holder = CMD_HOLDER_OF_SET_##name(args, .complete_func = comp_fn, .context = ctx); \
    return pcan_oneway_command_async(fwd, &cmd_holder)

int pcan_cmd_set_sja1000(struct usb_forwarder *forwarder, u8 mode)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG(forwarder, SAJ1000, 1, mode);
}

int pcan_cmd_set_sja1000_async(struct usb_forwarder *forwarder, u8 mode, void *complete_func, void *context)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG_ASYNC(forwarder, SAJ1000, 1, mode, complete_func, context);
}

int pcan_cmd_set_bus(struct usb_forwarder *forwarder, u8 is_on)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG(forwarder, BUS, 0, !!is_on);
}

int pcan_cmd_set_bus_async(struct usb_forwarder *forwarder, u8 is_on, void *complete_func, void *context)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG_ASYNC(forwarder, BUS, 0, !!is_on, complete_func, context);
}

int pcan_cmd_set_silent(struct usb_forwarder *forwarder, u8 is_on)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG(forwarder, SILENT, 0, !!is_on);
}

int pcan_cmd_set_silent_async(struct usb_forwarder *forwarder, u8 is_on, void *complete_func, void *context)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG_ASYNC(forwarder, SILENT, 0, !!is_on, complete_func, context);
}

int pcan_cmd_set_ext_vcc(struct usb_forwarder *forwarder, u8 is_on)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG(forwarder, EXT_VCC, 0, !!is_on);
}

int pcan_cmd_set_ext_vcc_async(struct usb_forwarder *forwarder, u8 is_on, void *complete_func, void *context)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG_ASYNC(forwarder, EXT_VCC, 0, !!is_on, complete_func, context);
}

static inline int __pcan_cmd_set_btr0btr1(struct usb_forwarder *forwarder, u8 btr0, u8 btr1,
    void *complete_func, void *context, int (*command_func)(usb_forwarder_t *, pcan_cmd_holder_t *))
{
    u8 args[PCAN_CMD_ARGS_LEN] = {
        [0] = (btr1 | ((forwarder->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES) ? 0x80 : 0)),
        [1] = btr0,
    };
    pcan_cmd_holder_t cmd_holder = CMD_HOLDER_OF_SET_BTR0BTR1(args, .complete_func = complete_func, .context = context);

    /* TODO: Reference counting or something. */

    return command_func(forwarder, &cmd_holder);
}

int pcan_cmd_set_btr0btr1(struct usb_forwarder *forwarder, u8 btr0, u8 btr1)
{
    return __pcan_cmd_set_btr0btr1(forwarder, btr0, btr1, NULL, NULL, pcan_oneway_command);
}

int pcan_cmd_set_btr0btr1_async(struct usb_forwarder *forwarder, u8 btr0, u8 btr1, void *complete_func, void *context)
{
    return __pcan_cmd_set_btr0btr1(forwarder, btr0, btr1, complete_func, context, pcan_oneway_command_async);
}

static inline int __pcan_cmd_set_bitrate(struct usb_forwarder *forwarder, u32 bitrate,
    void *complete_func, void *context, int (*command_func)(usb_forwarder_t *, pcan_cmd_holder_t *))
{
    u8 btr0, btr1;

#define CASE_BITRATE(_bitrate, _btr0, _btr1)    case _bitrate: btr0 = _btr0; btr1 = _btr1; break;

    switch (bitrate)
    {
    CASE_BITRATE(1000000, 0x00, 0x14);
    CASE_BITRATE(500000, 0x00, 0x1C);
    CASE_BITRATE(250000, 0x01, 0x1C);
    CASE_BITRATE(125000, 0x03, 0x1C);
    CASE_BITRATE(100000, 0x43, 0x2F);
    CASE_BITRATE(50000, 0x47, 0x2F);
    CASE_BITRATE(20000, 0x53, 0x2F);
    CASE_BITRATE(10000, 0x67, 0x2F);
    CASE_BITRATE(5000, 0x7F, 0x7F);

    default:
        pr_err_v("Invalid bitrate value: %u\n", bitrate);
        return -EINVAL;
    }

    pr_notice_v("setting bitrate = %u (that is: BTR0=0x%02x, BTR1=0x%02x)\n", bitrate, btr0, btr1);

    return __pcan_cmd_set_btr0btr1(forwarder, btr0, btr1, complete_func, context, command_func);
}

int pcan_cmd_set_bitrate(struct usb_forwarder *forwarder, u32 bitrate)
{
    return __pcan_cmd_set_bitrate(forwarder, bitrate, NULL, NULL, pcan_oneway_command);
}

int pcan_cmd_set_bitrate_async(struct usb_forwarder *forwarder, u32 bitrate, void *complete_func, void *context)
{
    return __pcan_cmd_set_bitrate(forwarder, bitrate, complete_func, context, pcan_oneway_command_async);
}

static inline int __pcan_cmd_set_bittiming(struct usb_forwarder *forwarder, struct can_bittiming *bt,
    void *complete_func, void *context, int (*command_func)(usb_forwarder_t *, pcan_cmd_holder_t *))
{
    u8 btr0 = ((bt->brp - 1) & 0x3f) | (((bt->sjw - 1) & 0x3) << 6);
    u8 btr1 = ((bt->prop_seg + bt->phase_seg1 - 1) & 0xf) | (((bt->phase_seg2 - 1) & 0x7) << 4);

    netdev_notice_v(forwarder->net_dev, "setting BTR0=0x%02x BTR1=0x%02x\n", btr0, btr1);

    return __pcan_cmd_set_btr0btr1(forwarder, btr0, btr1, complete_func, context, command_func);
}

int pcan_cmd_set_bittiming(struct usb_forwarder *forwarder, struct can_bittiming *bt)
{
    return __pcan_cmd_set_bittiming(forwarder, bt, NULL, NULL, pcan_oneway_command);
}

int pcan_cmd_set_bittiming_async(struct usb_forwarder *forwarder, struct can_bittiming *bt, void *complete_func, void *context)
{
    return __pcan_cmd_set_bittiming(forwarder, bt, complete_func, context, pcan_oneway_command_async);
}

int pcan_cmd_get_serial_number(struct usb_forwarder *forwarder, u32 *serial_number)
{
    u8 result[PCAN_CMD_ARGS_LEN] = { 0 };
    pcan_cmd_holder_t cmd_holder = CMD_HOLDER_OF_GET_SERIAL_NUMBER(result);
    int err = pcan_responsive_command(forwarder, &cmd_holder);

    if (err)
        dev_err_v(&forwarder->usb_dev->dev, "getting serial number failure: %d\n", err);
    else
    {
        __le32 tmp32;

        memcpy(&tmp32, result, sizeof(tmp32));
        *serial_number = le32_to_cpu(tmp32);
    }

    return err;
}

int pcan_cmd_get_device_id(struct usb_forwarder *forwarder, u32 *device_id)
{
    u8 result[PCAN_CMD_ARGS_LEN] = { 0 };
    pcan_cmd_holder_t cmd_holder = CMD_HOLDER_OF_GET_DEVICE_ID(result);
    int err = pcan_responsive_command(forwarder, &cmd_holder);

    if (err)
        dev_err_v(&forwarder->usb_dev->dev, "getting device id failure: %d\n", err);
    else
        *device_id = result[0];

    return err;
}

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
 *  01. Put definition of enum pcan_cmd_arg_index into header file.
 *
 * >>> 2023-09-29, Man Hung-Coeng <udc577@126.com>:
 *  01. Use logging APIs of 3rd-party klogging.h.
 *
 * >>> 2023-10-01, Man Hung-Coeng <udc577@126.com>:
 *  01. Delete pcan_fill_cmdbuf_for_*().
 *  02. Add pcan_oneway_command_async() and pcan_cmd_set_*_async().
 *
 * >>> 2023-10-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Add pcan_cmd_set_{btr0btr1,bitrate}[_async]().
 *  02. Change license to GPL-2.0.
 *
 * >>> 2023-11-08, Man Hung-Coeng <udc577@126.com>:
 *  01. Cancel the re-definition of __FILE__.
 */

