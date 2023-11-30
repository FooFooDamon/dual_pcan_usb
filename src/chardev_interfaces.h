/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Chardev interfaces of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#ifndef __CHARDEV_INTERFACES_H__
#define __CHARDEV_INTERFACES_H__

#include <linux/can.h>
#include <linux/cdev.h>

#define PCAN_CHRDEV_MAX_RX_BUF_COUNT            8

typedef struct pcan_chardev_msg
{
    ktime_t hwtstamp; /* hardware timestamp */
    struct can_frame frame;
} pcan_chardev_msg_t;

typedef struct pcan_chardev
{
    pcan_chardev_msg_t rx_msgs[PCAN_CHRDEV_MAX_RX_BUF_COUNT * 2];
    atomic_t rx_write_idx; /* index of message item to write */
    atomic_t rx_unread_cnt; /* count of unread items */
    struct device *device;
    atomic_t open_count;
    atomic_t active_tx_urbs;
    spinlock_t lock;
} pcan_chardev_t;

const struct file_operations* get_file_operations(void);

#endif /* #ifndef __CHARDEV_INTERFACES_H__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-11-08, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-11-30, Man Hung-Coeng <udc577@126.com>:
 *  01. Add a spin lock, a transmit URB counter
 *      and several receive message-related fields.
 */

