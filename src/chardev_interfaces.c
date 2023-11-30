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
#include "can_commands.h"
#include "chardev_group.h"
#include "usb_driver.h"

static void usb_write_bulk_callback(struct urb *urb)
{
    /* TODO */
}

static int pcan_chardev_open(struct inode *inode, struct file *file)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)CHRDEV_GRP_FIND_ITEM_PRIVDATA_BY_INODE(inode);
    int open_count = forwarder ? atomic_inc_return(&forwarder->char_dev.open_count) : 2;
    u16 dev_revision = forwarder ? (le16_to_cpu(forwarder->usb_dev->descriptor.bcdDevice) >> 8) : 0;
    int err = forwarder ? 0 : -ENODEV;
    int i;

    if (err)
        return err;

    if (open_count > 1)
    {
        dev_err_v(forwarder->char_dev.device, "Device has been opened %d times.\n", open_count);
        atomic_dec(&forwarder->char_dev.open_count);
        return -EMFILE;
    }

    atomic_set(&forwarder->char_dev.rx_write_idx, 0);
    atomic_set(&forwarder->char_dev.rx_unread_cnt, 0);
    atomic_set(&forwarder->char_dev.active_tx_urbs, 0);

    for (i = PCAN_USB_MAX_TX_URBS; i < PCAN_USB_MAX_TX_URBS * 2; ++i)
    {
        forwarder->tx_contexts[i].urb->complete = usb_write_bulk_callback;
    }

    /* FIXME: Needed or not: memset(&forwarder->time_ref, 0, sizeof(forwarder->time_ref)); */
    err = (dev_revision > 3) ? pcan_cmd_set_silent(forwarder, forwarder->can.ctrlmode & CAN_CTRLMODE_LISTENONLY) : 0;
    if (err || (err = pcan_cmd_set_ext_vcc(forwarder, /* is_on = */0)))
    {
        atomic_dec(&forwarder->char_dev.open_count);
        return err;
    }

    err = (atomic_inc_return(&forwarder->stage) > PCAN_USB_STAGE_ONE_STARTED) ? 0
        : usbdrv_reset_bus(forwarder, /* is_on = */1);
    if (err)
    {
        atomic_dec(&forwarder->stage);
        atomic_dec(&forwarder->char_dev.open_count);
    }
    else
        file->private_data = forwarder;

    return err;
}

static int pcan_chardev_release(struct inode *inode, struct file *file)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)CHRDEV_GRP_FIND_ITEM_PRIVDATA_BY_INODE(inode);
    int err = forwarder ? 0 : -ENODEV;

    if (NULL == forwarder)
        pr_err_v("Can not find forwarder, minor = %u\n", MINOR(inode->i_rdev));
    else
    {
        file->private_data = NULL;
        atomic_dec(&forwarder->char_dev.open_count);
        if (atomic_dec_return(&forwarder->stage) < PCAN_USB_STAGE_ONE_STARTED)
            err = usbdrv_reset_bus(forwarder, /* is_on = */0);
    }

    return err;
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
 *
 * >>> 2023-11-30, Man Hung-Coeng <udc577@126.com>:
 *  01. Add mutually-exclusive control to open and close functions
 *      to avoid some status and resource conflicts with netdev.
 */

