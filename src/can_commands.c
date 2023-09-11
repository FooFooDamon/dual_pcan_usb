/*
 * Implementation of CAN command interfaces.
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

#include "can_commands.h"

#include <linux/usb.h>

#include "common.h"
#include "klogging.h"
#include "usb_driver.h"

enum pcan_cmd_arg_index
{
    PCAN_CMD_ARG_INDEX_FUNC =       0
    , PCAN_CMD_ARG_INDEX_NUM =      1
    , PCAN_CMD_ARG_INDEX_ARG =      2
};
#define PCAN_CMD_ARGS_LEN           14
#define PCAN_CMD_TOTAL_LEN          (PCAN_CMD_ARG_INDEX_ARG + PCAN_CMD_ARGS_LEN)

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
    buf[PCAN_CMD_ARG_INDEX_FUNC] = cmd_holder->functionality;
    buf[PCAN_CMD_ARG_INDEX_NUM] = cmd_holder->number;
    if (NULL != cmd_holder->args)
        memcpy(buf + PCAN_CMD_ARG_INDEX_ARG, cmd_holder->args, PCAN_CMD_ARGS_LEN);

    if ((err = usbdrv_bulk_msg_send(forwarder, buf, PCAN_CMD_TOTAL_LEN)) < 0)
        pr_err_v("sending cmd f=0x%x n=0x%x failure: %d\n", cmd_holder->functionality, cmd_holder->number, err);

CMD_END:

    /* TODO: Reference counting or something. */

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
        pr_err_v("waiting reply f=0x%x n=0x%x failure: %d\n", cmd_holder->functionality, cmd_holder->number, err);
    else if (NULL != cmd_holder->result)
        memcpy(cmd_holder->result, buf + PCAN_CMD_ARG_INDEX_ARG, PCAN_CMD_ARGS_LEN);
    else
        ; /* Nothing. */

CMD_END:

    /* TODO: Reference counting or something. */

    return err;
}

#define __PCAN_ONEWAY_SET_SINGLE_ARG(fwd, f, n, idx, arg)   \
    u8 args[PCAN_CMD_ARGS_LEN] = { \
        [idx] = arg, \
    }; \
    pcan_cmd_holder_t cmd_holder = { \
        .functionality = f \
        , .number = n \
        , .args = args \
    }; \
    return pcan_oneway_command(fwd, &cmd_holder)

int pcan_set_sja1000(struct usb_forwarder *forwarder, u8 mode)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG(forwarder, 9, 2, 1, mode);
}

int pcan_set_bus(struct usb_forwarder *forwarder, u8 is_on)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG(forwarder, 3, 2, 0, !!is_on);
}

int pcan_set_silent(struct usb_forwarder *forwarder, u8 is_on)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG(forwarder, 3, 3, 0, !!is_on);
}

int pcan_set_ext_vcc(struct usb_forwarder *forwarder, u8 is_on)
{
    __PCAN_ONEWAY_SET_SINGLE_ARG(forwarder, 10, 2, 0, !!is_on);
}

struct can_bittiming;

int pcan_set_bittiming(struct usb_forwarder *forwarder, struct can_bittiming *bt)
{
    u8 args[PCAN_CMD_ARGS_LEN] = { 0 };
    pcan_cmd_holder_t cmd_holder = {
        .functionality = 1
        , .number = 2
        , .args = args
    };
    u8 btr0 = ((bt->brp - 1) & 0x3f) | (((bt->sjw - 1) & 0x3) << 6);
    u8 btr1 = ((bt->prop_seg + bt->phase_seg1 - 1) & 0xf) | (((bt->phase_seg2 - 1) & 0x7) << 4);

    /* TODO: Reference counting or something. */

    if (forwarder->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES)
        btr1 |= 0x80;

    dev_notice(&forwarder->usb_dev->dev, "setting BTR0=0x%02x BTR1=0x%02x\n", btr0, btr1);

    args[0] = btr1;
    args[1] = btr0;

    return pcan_oneway_command(forwarder, &cmd_holder);
}

int pcan_get_serial_number(struct usb_forwarder *forwarder, u32 *serial_number)
{
    u8 result[PCAN_CMD_ARGS_LEN] = { 0 };
    pcan_cmd_holder_t cmd_holder = {
        .functionality = 6
        , .number = 1
        , .result = result
    };
    int err = pcan_responsive_command(forwarder, &cmd_holder);

    if (err)
        pr_err_v("getting serial failure: %d\n", err);
    else
    {
        __le32 tmp32;

        memcpy(&tmp32, result, sizeof(tmp32));
        *serial_number = le32_to_cpu(tmp32);
    }

    return err;
}

int pcan_get_device_id(struct usb_forwarder *forwarder, u32 *device_id)
{
    u8 result[PCAN_CMD_ARGS_LEN] = { 0 };
    pcan_cmd_holder_t cmd_holder = {
        .functionality = 4
        , .number = 1
        , .result = result
    };
    int err = pcan_responsive_command(forwarder, &cmd_holder);

    if (err)
        pr_err_v("getting device id failure: %d\n", err);
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
 */
