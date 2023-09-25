/*
 * Implementation of PCAN-USB packet coder and decoder.
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

#include "packet_codec.h"

#include <linux/ktime.h>
#include <linux/netdevice.h>
#include <linux/can/dev.h>

#include "common.h"
#include "klogging.h"
#include "can_commands.h"
#include "usb_driver.h"

#define PCAN_USB_MSG_HEADER_LEN		        2

/* PCAN-USB USB message record status/len field */
#define PCAN_USB_STATUSLEN_TIMESTAMP	    (1 << 7)
#define PCAN_USB_STATUSLEN_INTERNAL	        (1 << 6)
#define PCAN_USB_STATUSLEN_EXT_ID	        (1 << 5)
#define PCAN_USB_STATUSLEN_RTR		        (1 << 4)
#define PCAN_USB_STATUSLEN_DLC		        (0xf)

/* PCAN-USB error flags */
#define PCAN_USB_ERROR_TXFULL		        0x01
#define PCAN_USB_ERROR_RXQOVR		        0x02
#define PCAN_USB_ERROR_BUS_LIGHT	        0x04
#define PCAN_USB_ERROR_BUS_HEAVY	        0x08
#define PCAN_USB_ERROR_BUS_OFF		        0x10
#define PCAN_USB_ERROR_RXQEMPTY		        0x20
#define PCAN_USB_ERROR_QOVR		            0x40
#define PCAN_USB_ERROR_TXQFULL		        0x80

/*
 * tick duration = 42.666 us
 * => (tick_number * 42666) / 1000
 * => (tick_number * x) / (1000 * 1.024 * 1024)
 * ~=> (tick_number * 44739243) >> 20
 * accuracy = 10^-7
 */
#define PCAN_USB_TS_DIV_SHIFTER		        20
#define PCAN_USB_TS_US_PER_TICK		        44739243
#define PCAN_USB_TS_USED_BITS               16
#define PCAN_USB_TS_CALIBRATION             24575

/* PCAN-USB messages record types */
#define PCAN_USB_REC_ERROR		            1
#define PCAN_USB_REC_ANALOG		            2
#define PCAN_USB_REC_BUSLOAD	            3
#define PCAN_USB_REC_TS			            4
#define PCAN_USB_REC_BUSEVT		            5

typedef struct msg_context
{
    u16 ts16;
    u8 prev_ts8;
    const u8 *ptr;
    const u8 *end;
    u8 rec_cnt;
    u8 rec_idx;
    u8 rec_data_idx;
    struct net_device *netdev;
} msg_context_t;

int pcan_encode_frame_to_buf(const struct net_device *dev, const struct can_frame *frame, u8 *obuf, size_t *size)
{
    const struct net_device_stats *stats = &dev->stats;

    /* header */
    *obuf++ = 2;
    *obuf++ = 1;

    /* status/len */
    *obuf = frame->can_dlc;
    if (frame->can_id & CAN_RTR_FLAG)
        *obuf |= PCAN_USB_STATUSLEN_RTR;

    /* can id */
    if (frame->can_id & CAN_EFF_FLAG)
    {
        __le32 tmp32 = cpu_to_le32((frame->can_id & CAN_ERR_MASK) << 3);

        *obuf |= PCAN_USB_STATUSLEN_EXT_ID;
        memcpy(++obuf, &tmp32, sizeof(tmp32));
        obuf += sizeof(tmp32);
    }
    else
    {
        __le16 tmp16 = cpu_to_le16((frame->can_id & CAN_ERR_MASK) << 5);

        memcpy(++obuf, &tmp16, sizeof(tmp16));
        obuf += sizeof(tmp16);
    }

    /* can data */
    if (!(frame->can_id & CAN_RTR_FLAG))
    {
        memcpy(obuf, frame->data, frame->can_dlc);
        obuf += frame->can_dlc;
    }

    /* TODO: Need to update value of size? */
    obuf[*size - 1] = (u8)(stats->tx_packets & 0xff); /* TODO: What does it mean? */

    return 0;
}

void compute_kernel_time(const pcan_time_ref_t *time_ref, u32 timestamp, ktime_t *kernel_time)
{
    if (ktime_to_ns(time_ref->tv_host) > 0)
    {
        u64 delta_us = timestamp - time_ref->ts_dev_2;

        if (timestamp < time_ref->ts_dev_2)
            delta_us &= (1 << PCAN_USB_TS_USED_BITS) - 1;

        delta_us += time_ref->ts_total;

        delta_us *= PCAN_USB_TS_US_PER_TICK;
        delta_us >>= PCAN_USB_TS_DIV_SHIFTER;

        *kernel_time = ktime_add_us(time_ref->tv_host_0, delta_us);
    }
    else
        *kernel_time = ktime_get();
}

static int decode_timestamp_in_context(u8 is_first_packet, msg_context_t *ctx)
{
    if (is_first_packet)
    {
        __le16 tmp16;

        if (ctx->ptr + sizeof(tmp16) > ctx->end)
            return -EINVAL;

        memcpy(&tmp16, ctx->ptr, sizeof(tmp16));
        ctx->ptr += sizeof(tmp16);

        ctx->ts16 = le16_to_cpu(tmp16);
        ctx->prev_ts8 = ctx->ts16 & 0x00ff;
    }
    else
    {
        u8 ts8;

        if (ctx->ptr + sizeof(ts8) > ctx->end)
            return -EINVAL;

        ts8 = *ctx->ptr++;

        if (ts8 < ctx->prev_ts8)
            ctx->ts16 += 0x100;

        ctx->ts16 &= 0xff00;
        ctx->ts16 |= ts8;
        ctx->prev_ts8 = ts8;
    }

    return 0;
}

static int decode_error(msg_context_t *ctx, u8 number, u8 status_len)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)netdev_priv(ctx->netdev);
    enum can_state new_state = forwarder->can.state;
    struct can_frame *frame = NULL;
    struct sk_buff *skb = NULL;

    /* ignore this error until 1st ts received */
    if (number == PCAN_USB_ERROR_QOVR && !forwarder->time_ref.tick_count)
        return 0;

    switch (forwarder->can.state)
    {
    case CAN_STATE_ERROR_ACTIVE:
        if (number & PCAN_USB_ERROR_BUS_LIGHT)
        {
            new_state = CAN_STATE_ERROR_WARNING;
            break;
        }
        /* FIXME: fall through? */

    case CAN_STATE_ERROR_WARNING:
        if (number & PCAN_USB_ERROR_BUS_HEAVY)
        {
            new_state = CAN_STATE_ERROR_PASSIVE;
            break;
        }
        if (number & PCAN_USB_ERROR_BUS_OFF)
        {
            new_state = CAN_STATE_BUS_OFF;
            break;
        }
        if (number & (PCAN_USB_ERROR_RXQOVR | PCAN_USB_ERROR_QOVR))
        {
            /* trick to bypass next comparison and process other errors */
            new_state = CAN_STATE_MAX;
            break;
        }
        if ((number & PCAN_USB_ERROR_BUS_LIGHT) == 0)
        {
            /* no error (back to active state) */
            forwarder->can.state = CAN_STATE_ERROR_ACTIVE;
            return 0;
        }
        break;

    case CAN_STATE_ERROR_PASSIVE:
        if (number & PCAN_USB_ERROR_BUS_OFF)
        {
            new_state = CAN_STATE_BUS_OFF;
            break;
        }
        if (number & PCAN_USB_ERROR_BUS_LIGHT)
        {
            new_state = CAN_STATE_ERROR_WARNING;
            break;
        }
        if (number & (PCAN_USB_ERROR_RXQOVR | PCAN_USB_ERROR_QOVR))
        {
            /* trick to bypass next comparison and process other errors */
            new_state = CAN_STATE_MAX;
            break;
        }
        if ((number & PCAN_USB_ERROR_BUS_HEAVY) == 0)
        {
            /* no error (back to active state) */
            forwarder->can.state = CAN_STATE_ERROR_ACTIVE;
            return 0;
        }
        break;

    default:
        return 0; /* do nothing waiting for restart */
    }

    /* donot post any error if current state didn't change */
    if (forwarder->can.state == new_state)
        return 0;

    skb = alloc_can_skb(ctx->netdev, &frame);
    if (!skb)
        return -ENOMEM;

    switch (new_state)
    {
    case CAN_STATE_BUS_OFF:
        frame->can_id |= CAN_ERR_BUSOFF;
        ++forwarder->can.can_stats.bus_off;
        can_bus_off(ctx->netdev);
        break;

    case CAN_STATE_ERROR_PASSIVE:
        frame->can_id |= CAN_ERR_CRTL;
        frame->data[1] |= (CAN_ERR_CRTL_TX_PASSIVE | CAN_ERR_CRTL_RX_PASSIVE);
        ++forwarder->can.can_stats.error_passive;
        break;

    case CAN_STATE_ERROR_WARNING:
        frame->can_id |= CAN_ERR_CRTL;
        frame->data[1] |= (CAN_ERR_CRTL_TX_WARNING | CAN_ERR_CRTL_RX_WARNING);
        ++forwarder->can.can_stats.error_warning;
        break;

    default:
        /* CAN_STATE_MAX (trick to handle other errors) */
        frame->can_id |= CAN_ERR_CRTL;
        frame->data[1] |= CAN_ERR_CRTL_RX_OVERFLOW;
        ++ctx->netdev->stats.rx_over_errors;
        ++ctx->netdev->stats.rx_errors;
        new_state = forwarder->can.state;
        break;
    }

    forwarder->can.state = new_state;

    if (status_len & PCAN_USB_STATUSLEN_TIMESTAMP)
        compute_kernel_time(&(forwarder->time_ref), ctx->ts16, &(skb_hwtstamps(skb)->hwtstamp));

    netif_rx(skb);

    ++ctx->netdev->stats.rx_packets;
    ctx->netdev->stats.rx_bytes += frame->can_dlc;

    return 0;
}

static void update_time_reference(u32 ts_now, pcan_time_ref_t *time_ref)
{
    time_ref->ts_dev_2 = ts_now;

    /* should wait at least two passes before computing */
    if (ktime_to_ns(time_ref->tv_host) > 0)
    {
        u32 delta_ts = time_ref->ts_dev_2 - time_ref->ts_dev_1;

        if (time_ref->ts_dev_2 < time_ref->ts_dev_1)
            delta_ts &= (1 << PCAN_USB_TS_USED_BITS) - 1;

        time_ref->ts_total += delta_ts;
    }
}

static void set_time_reference(u32 ts_now, pcan_time_ref_t *time_ref)
{
    if (ktime_to_ns(time_ref->tv_host_0) == 0)
    {
        /* use monotonic clock to correctly compute further deltas */
        time_ref->tv_host_0 = ktime_get();
        time_ref->tv_host = ktime_set(0, 0);
    }
    else
    {
        /*
         * delta_us should not be >= 2^32 => delta_s should be < 4294
         * handle 32-bits wrapping here: if count of s. reaches 4200,
         * reset counters and change time base
         */
        if (ktime_to_ns(time_ref->tv_host))
        {
            ktime_t delta = ktime_sub(time_ref->tv_host, time_ref->tv_host_0);

            if (ktime_to_ns(delta) > (4200ull * NSEC_PER_SEC))
            {
                time_ref->tv_host_0 = time_ref->tv_host;
                time_ref->ts_total = 0;
            }

            time_ref->tv_host = ktime_get();
            ++time_ref->tick_count;
        }
    }

    time_ref->ts_dev_1 = time_ref->ts_dev_2;
    update_time_reference(ts_now, time_ref);
}

static int update_timestamp_in_context(msg_context_t *ctx)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)netdev_priv(ctx->netdev);
    __le16 tmp16;

    if (ctx->ptr + sizeof(tmp16) > ctx->end)
        return -EINVAL;

    memcpy(&tmp16, ctx->ptr, sizeof(tmp16));

    ctx->ts16 = le16_to_cpu(tmp16);

    if (ctx->rec_idx > 0)
        update_time_reference(ctx->ts16, &forwarder->time_ref);
    else
        set_time_reference(ctx->ts16, &forwarder->time_ref);

    return 0;
}

static int decode_status_and_error(msg_context_t *ctx, u8 status_len)
{
    u8 rec_len = status_len & PCAN_USB_STATUSLEN_DLC;
    u8 functionality, number;
    int err = 0;

    if (ctx->ptr + PCAN_CMD_ARG_INDEX_ARG > ctx->end)
        return -EINVAL;

    functionality = ctx->ptr[PCAN_CMD_ARG_INDEX_FUNC];
    number = ctx->ptr[PCAN_CMD_ARG_INDEX_NUM];
    ctx->ptr += PCAN_CMD_ARG_INDEX_ARG;

    if (status_len & PCAN_USB_STATUSLEN_TIMESTAMP)
        err = decode_timestamp_in_context(!ctx->rec_idx, ctx);

    if (err)
        return err;

    switch (functionality)
    {
    case PCAN_USB_REC_ERROR:
        err = decode_error(ctx, number, status_len);
        if (err)
            return err;
        break;

    case PCAN_USB_REC_ANALOG:
    case PCAN_USB_REC_BUSLOAD:
        rec_len = (PCAN_USB_REC_ANALOG == functionality) ? 2 : 1;
        break;

    case PCAN_USB_REC_TS:
        err = update_timestamp_in_context(ctx);
        if (err)
            return err;
        break;

    case PCAN_USB_REC_BUSEVT: /* error frame/bus event */
        if (number & PCAN_USB_ERROR_TXQFULL)
            netdev_dbg(ctx->netdev, "device Tx queue full)\n");
        break;

    default:
        pr_err_v("unexpected functionality %u\n", functionality);
        break;
    }

    if (ctx->ptr + rec_len > ctx->end)
        return -EINVAL;

    ctx->ptr += rec_len;

    return 0;
}

static int decode_data(msg_context_t *ctx, u8 status_len)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)netdev_priv(ctx->netdev);
    u8 rec_len = status_len & PCAN_USB_STATUSLEN_DLC;
    struct can_frame *frame = NULL;
    struct sk_buff *skb = alloc_can_skb(ctx->netdev, &frame);

    if (!skb)
        return -ENOMEM;

    if (status_len & PCAN_USB_STATUSLEN_EXT_ID)
    {
        __le32 tmp32;

        if (ctx->ptr + sizeof(tmp32) > ctx->end)
            goto decode_failed;

        memcpy(&tmp32, ctx->ptr, sizeof(tmp32));
        ctx->ptr += sizeof(tmp32);

        frame->can_id = (le32_to_cpu(tmp32) >> 3) | CAN_EFF_FLAG;
    }
    else
    {
        __le16 tmp16;

        if (ctx->ptr + sizeof(tmp16) > ctx->end)
            goto decode_failed;

        memcpy(&tmp16, ctx->ptr, sizeof(tmp16));
        ctx->ptr += sizeof(tmp16);

        frame->can_id = (le16_to_cpu(tmp16) >> 5);
    }

    frame->can_dlc = min_t(u8, rec_len, CAN_MAX_DLC);

    if (decode_timestamp_in_context(!ctx->rec_data_idx, ctx))
        goto decode_failed;

    memset(frame->data, 0, sizeof(frame->data));
    if (status_len & PCAN_USB_STATUSLEN_RTR)
        frame->can_id |= CAN_RTR_FLAG;
    else
    {
        if (ctx->ptr + rec_len > ctx->end)
            goto decode_failed;

        memcpy(frame->data, ctx->ptr, frame->can_dlc);
        ctx->ptr += rec_len;
    }

    compute_kernel_time(&(forwarder->time_ref), ctx->ts16, &(skb_hwtstamps(skb)->hwtstamp));

    netif_rx(skb);

    ++ctx->netdev->stats.rx_packets;
    ctx->netdev->stats.rx_bytes += frame->can_dlc;

    return 0;

decode_failed:

    dev_kfree_skb(skb);

    return -EINVAL;
}

static int decode_incoming_buf(const u8 *ibuf, u32 size, struct net_device *dev)
{
    msg_context_t ctx = {
        .rec_cnt = ibuf[1]
        , .ptr = ibuf + PCAN_USB_MSG_HEADER_LEN
        , .end = ibuf + size
        , .netdev = dev
    };
    int err = 0;

    for (err = 0; ctx.rec_idx < ctx.rec_cnt && !err; ++ctx.rec_idx)
    {
        u8 status_len = *ctx.ptr++;

        if (status_len & PCAN_USB_STATUSLEN_INTERNAL)
            err = decode_status_and_error(&ctx, status_len);
        else
        {
            err = decode_data(&ctx, status_len);
            ++ctx.rec_data_idx;
        }
    }

    return err;
}

int pcan_decode_and_handle_urb(const struct urb *urb, struct net_device *dev)
{
    if (urb->actual_length > PCAN_USB_MSG_HEADER_LEN)
        return decode_incoming_buf(urb->transfer_buffer, urb->actual_length, dev);
    else if (urb->actual_length == 0)
        return 0;
    else
    {
        pr_err_v("usb message length error (%u)\n", urb->actual_length);
        return -EINVAL;
    }
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-09-20, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 */

