// SPDX-License-Identifier: GPL-2.0

/*
 * Implementation of chardev interfaces of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#include "chardev_interfaces.h"

#include <linux/poll.h>

#include "common.h"
#include "klogging.h"
#include "chardev_group.h"
#include "usb_driver.h"

static int pcan_chardev_open(struct inode *inode, struct file *file)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)CHRDEV_GRP_FIND_ITEM_PRIVDATA_BY_INODE(inode);
    int open_count = forwarder ? atomic_inc_return(&forwarder->char_dev.open_count) : 2;

    if (NULL == forwarder)
        return -ENODEV;

    if (open_count > 1)
    {
        dev_err_v(forwarder->char_dev.device, "Device has been opened %d times.\n", open_count);
        atomic_dec(&forwarder->char_dev.open_count);
        return -EMFILE;
    }

    file->private_data = forwarder;

    return 0;
}

static int pcan_chardev_release(struct inode *inode, struct file *file)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)CHRDEV_GRP_FIND_ITEM_PRIVDATA_BY_INODE(inode);

    if (NULL == forwarder)
        pr_err_v("Can not find forwarder, minor = %u\n", MINOR(inode->i_rdev));
    else
    {
        atomic_dec(&forwarder->char_dev.open_count);
        file->private_data = NULL;
    }

    return 0;
}

static unsigned int pcan_chardev_poll(struct file *file, poll_table *wait)
{
    return -EOPNOTSUPP;
}

static ssize_t pcan_chardev_read(struct file *file, char __user *buf, size_t count, loff_t *off)
{
    return 0;
}

static ssize_t pcan_chardev_write(struct file *file, const char __user *buf, size_t count, loff_t *off)
{
    return -EOPNOTSUPP;
}

static long pcan_chardev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return -EOPNOTSUPP;
}

static const struct file_operations S_FILE_OPS = {
    .owner = THIS_MODULE,
    .open = pcan_chardev_open,
    .release = pcan_chardev_release,
    .poll = pcan_chardev_poll,
    .read = pcan_chardev_read,
    .write = pcan_chardev_write,
    .unlocked_ioctl = pcan_chardev_ioctl,
};

const struct file_operations* get_file_operations(void)
{
    return &S_FILE_OPS;
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-11-08, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 */

