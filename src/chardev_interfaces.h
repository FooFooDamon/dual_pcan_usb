/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Chardev interfaces of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#ifndef __CHARDEV_INTERFACES_H__
#define __CHARDEV_INTERFACES_H__

/*#include <linux/can.h>*/
#include <linux/cdev.h>

typedef struct pcan_chardev
{
#if 0
    struct can_frame rx_frames[16];
    atomic_t wr_idx; /* index of frame item to write */
    atomic_t rd_idx; /* index of frame item to read */
#endif
    struct device *device;
    atomic_t open_count;
    /* spinlock_t lock; */
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
 */

