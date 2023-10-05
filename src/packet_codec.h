/* SPDX-License-Identifier: GPL-2.0 */

/*
 * PCAN-USB packet coder and decoder.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#ifndef __PACKET_CODEC_H__
#define __PACKET_CODEC_H__

#include <linux/types.h> /* For size_t, u8, etc. */
#include <linux/ktime.h> /* For ktime_t. */

struct net_device;
struct can_frame;
struct urb;

/* time reference */
typedef struct pcan_time_ref
{
    ktime_t tv_host_0, tv_host;
    u32 ts_dev_1, ts_dev_2;
    u64 ts_total;
    u32 tick_count;
} pcan_time_ref_t;

int pcan_encode_frame_to_buf(const struct net_device *dev, const struct can_frame *frame, u8 *obuf, size_t *size);

int pcan_decode_and_handle_urb(const struct urb *urb, struct net_device *dev);

#endif /* #ifndef __PACKET_CODEC_H__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-09-20, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-10-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Change license to GPL-2.0.
 */

