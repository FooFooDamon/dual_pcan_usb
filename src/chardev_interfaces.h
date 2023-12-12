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

struct pcanfd_ioctl_msgs;

typedef struct pcan_chardev_msg
{
    ktime_t hwtstamp; /* hardware timestamp */
    struct can_frame frame;
} pcan_chardev_msg_t;

typedef struct pcan_chardev
{
    struct pcanfd_ioctl_msgs *ioctl_rxmsgs;
    pcan_chardev_msg_t rx_msgs[PCAN_CHRDEV_MAX_RX_BUF_COUNT * 2];
    atomic_t rx_write_idx; /* index of message item to write */
    atomic_t rx_unread_cnt; /* count of unread items */
    u64 rx_packets; /* or atomic64_t*/
    struct device *device;
    atomic_t open_count;
    atomic_t active_tx_urbs;
    spinlock_t lock;
    wait_queue_head_t wait_queue_rd; /* wait queue for reading */
    wait_queue_head_t wait_queue_wr; /* wait queue for writing */
    u32 serial_number;
    u32 device_id;
    u32 ioctl_init_flags;
} pcan_chardev_t;

static inline int pcan_chardev_calc_rx_read_index(int write_index, int unread_msgs)
{
    return (write_index + PCAN_CHRDEV_MAX_RX_BUF_COUNT - unread_msgs) % PCAN_CHRDEV_MAX_RX_BUF_COUNT;
}

int pcan_chardev_initialize(pcan_chardev_t *dev);

void pcan_chardev_finalize(pcan_chardev_t *dev);

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
 *
 * >>> 2023-12-12, Man Hung-Coeng <udc577@126.com>:
 *  01. Add pcan_chardev_initialize() and pcan_chardev_finalize().
 *  02. Add some fields to struct pcan_chardev for ioctl() implementation.
 */

