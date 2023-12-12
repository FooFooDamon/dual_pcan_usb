// SPDX-License-Identifier: GPL-2.0

/*
 * Implementation of chardev interfaces of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#include "chardev_interfaces.h"

#include <linux/poll.h>

#include "versions.h"
#include "common.h"
#include "klogging.h"
#include "can_commands.h"
#include "chardev_group.h"
#include "chardev_ioctl.h"
#include "usb_driver.h"
#include "evol_kernel.h"

int pcan_chardev_initialize(pcan_chardev_t *dev)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)container_of(dev, usb_forwarder_t, char_dev);

    if (IS_ERR(dev->device = CHRDEV_GRP_MAKE_ITEM(DEV_NAME, forwarder)))
    {
        int err = PTR_ERR(dev->device);

        pr_err_v("Failed to create chardev: %d\n", err);

        return err;
    }

    atomic_set(&forwarder->char_dev.open_count, 0);

    spin_lock_init(&dev->lock);

    init_waitqueue_head(&dev->wait_queue_rd);
    init_waitqueue_head(&dev->wait_queue_wr);

    return 0;
}

void pcan_chardev_finalize(pcan_chardev_t *dev)
{
    CHRDEV_GRP_UNMAKE_ITEM(dev->device, NULL);
}

static void usb_write_bulk_callback(struct urb *urb)
{
    /* TODO */
}

static int pcan_chardev_open(struct inode *inode, struct file *file)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)CHRDEV_GRP_FIND_ITEM_PRIVDATA_BY_INODE(inode);
    int open_count = forwarder ? atomic_inc_return(&forwarder->char_dev.open_count) : 2;
    u16 dev_revision = forwarder ? (le16_to_cpu(forwarder->usb_dev->descriptor.bcdDevice) >> 8) : 0;
    s16 stage = PCAN_USB_STAGE_DISCONNECTED;
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

    if (file->f_flags & O_NONBLOCK)
        dev_notice_v(forwarder->char_dev.device, "Non-blocking mode enabled!\n");

    atomic_set(&forwarder->char_dev.rx_write_idx, 0);
    atomic_set(&forwarder->char_dev.rx_unread_cnt, 0);
    forwarder->char_dev.rx_packets = 0;
    atomic_set(&forwarder->char_dev.active_tx_urbs, 0);

    for (i = PCAN_USB_MAX_TX_URBS; i < PCAN_USB_MAX_TX_URBS * 2; ++i)
    {
        forwarder->tx_contexts[i].urb->complete = usb_write_bulk_callback;
    }

    file->private_data = forwarder;

    if ((stage = atomic_inc_return(&forwarder->stage)) > PCAN_USB_STAGE_ONE_STARTED)
        return 0;

    memset(&forwarder->time_ref, 0, sizeof(forwarder->time_ref));

    err = (dev_revision > 3) ? pcan_cmd_set_silent(forwarder, forwarder->can.ctrlmode & CAN_CTRLMODE_LISTENONLY) : 0;
    if (err || (err = pcan_cmd_set_ext_vcc(forwarder, /* is_on = */0))
        || (err = usbdrv_reset_bus(forwarder, /* is_on = */1)))
    {
        atomic_dec(&forwarder->stage);
        atomic_dec(&forwarder->char_dev.open_count);
    }

    return err;
}

static int pcan_chardev_release(struct inode *inode, struct file *file)
{
    usb_forwarder_t *forwarder = (usb_forwarder_t *)CHRDEV_GRP_FIND_ITEM_PRIVDATA_BY_INODE(inode);
    int err = /* forwarder ? */0/* : -ENODEV*/;

    if (NULL == forwarder)
        pr_err_v("Can not find forwarder, minor = %u\n", MINOR(inode->i_rdev));
    else
    {
        file->private_data = NULL;
        atomic_dec(&forwarder->char_dev.open_count);
        if (atomic_dec_return(&forwarder->stage) < PCAN_USB_STAGE_ONE_STARTED)
            /* err = */usbdrv_reset_bus(forwarder, /* is_on = */0);
    }

    return err;
}

static inline usb_forwarder_t* get_usb_forwarder_from_file(struct file *file)
{
    void *priv_data = file->private_data;

    return (usb_forwarder_t *)(likely(priv_data) ? priv_data : CHRDEV_GRP_FIND_ITEM_PRIVDATA_BY_INODE(file->f_inode));
}

#define CHRDEV_OP_PRECHECK(fwd, filp, err)              do { \
    if (unlikely(atomic_read(&(fwd)->stage) < PCAN_USB_STAGE_ONE_STARTED)) { \
        (filp)->private_data = NULL; \
        return err; \
    } \
} while (0)

static unsigned int pcan_chardev_poll(struct file *file, poll_table *wait)
{
    usb_forwarder_t *forwarder = get_usb_forwarder_from_file(file);
    pcan_chardev_t *dev = likely(forwarder) ? &forwarder->char_dev : NULL;
    unsigned int mask = likely(dev) ? 0 : POLLERR;

    if (mask)
        return mask;

    CHRDEV_OP_PRECHECK(forwarder, file, POLLERR);

    /* atomic_inc(&forwarder->pending_ops); */ /* Not needed. */

    poll_wait(file, &dev->wait_queue_rd, wait);

    if (atomic_read(&dev->rx_unread_cnt) > 0)
        mask |= (POLLIN | POLLRDNORM);

    poll_wait(file, &dev->wait_queue_wr, wait);

    if (atomic_read(&dev->active_tx_urbs) < PCAN_USB_MAX_TX_URBS)
        mask |= (POLLOUT | POLLWRNORM);

    /* atomic_dec(&forwarder->pending_ops); */ /* Not needed. */

    return mask;
}

static ssize_t pcan_chardev_read(struct file *file, char __user *buf, size_t count, loff_t *off)
{
#ifndef INNER_TEST
    return -EOPNOTSUPP; /* The read logic of pcanview is not good. It's not worthwhile to support it. */
#else
    usb_forwarder_t *forwarder = get_usb_forwarder_from_file(file);
    pcan_chardev_t *dev = likely(forwarder) ? &forwarder->char_dev : NULL;
    unsigned long lock_flags;
    int unread_msgs = 0;
    int err = likely(dev) ? ((count >= sizeof(pcan_chardev_msg_t)) ? 0 : -EINVAL) : -ENODEV;

    if (err)
        return err;

    CHRDEV_OP_PRECHECK(forwarder, file, -ENODEV);

    atomic_inc(&forwarder->pending_ops);

    if (file->f_flags & O_NONBLOCK)
    {
        if ((unread_msgs = atomic_read(&dev->rx_unread_cnt)) <= 0)
        {
            err = -EAGAIN;
            goto lbl_read_end;
        }
    }
    else
    {
        err = wait_event_interruptible(dev->wait_queue_rd,
            atomic_read(&dev->rx_unread_cnt) > 0 || atomic_read(&forwarder->stage) < PCAN_USB_STAGE_ONE_STARTED);
        if (err)
            goto lbl_read_end;

        if (unlikely(atomic_read(&forwarder->stage) < PCAN_USB_STAGE_ONE_STARTED)) /* Has been plugged out. */
        {
            err = -ENODEV;
            goto lbl_read_end;
        }

        if (unlikely((unread_msgs = atomic_read(&dev->rx_unread_cnt)) <= 0))
        {
            err = signal_pending(current) ? -ERESTARTSYS/*-EINTR*/ : -EAGAIN;
            goto lbl_read_end;
        }
    }

    spin_lock_irqsave(&dev->lock, lock_flags);
    {
        int read_index = pcan_chardev_calc_rx_read_index(atomic_read(&dev->rx_write_idx), unread_msgs);
        int msgs_to_read = count / sizeof(pcan_chardev_msg_t);
        int forward_msgs = PCAN_CHRDEV_MAX_RX_BUF_COUNT - read_index;
        int rewind_msgs;

        if (msgs_to_read > unread_msgs)
            msgs_to_read = unread_msgs;

        rewind_msgs = (msgs_to_read > forward_msgs) ? (msgs_to_read - forward_msgs) : 0;
        if (rewind_msgs)
        {
            memcpy(&dev->rx_msgs[PCAN_CHRDEV_MAX_RX_BUF_COUNT], &dev->rx_msgs[read_index],
                sizeof(pcan_chardev_msg_t) * forward_msgs);
            memcpy(&dev->rx_msgs[PCAN_CHRDEV_MAX_RX_BUF_COUNT + forward_msgs], &dev->rx_msgs[0],
                sizeof(pcan_chardev_msg_t) * rewind_msgs);
        }
        else
        {
            memcpy(&dev->rx_msgs[PCAN_CHRDEV_MAX_RX_BUF_COUNT], &dev->rx_msgs[read_index],
                sizeof(pcan_chardev_msg_t) * msgs_to_read);
        }

        atomic_sub(msgs_to_read, &dev->rx_unread_cnt);
        unread_msgs = msgs_to_read;
    }
    spin_unlock_irqrestore(&dev->lock, lock_flags);

    /* NOTE: copy_to_user() might sleep, don't call it with spin lock held. */
    err = count - copy_to_user(buf, &dev->rx_msgs[PCAN_CHRDEV_MAX_RX_BUF_COUNT],
        sizeof(pcan_chardev_msg_t) * unread_msgs);

lbl_read_end:

    atomic_dec(&forwarder->pending_ops);

    return err;
#endif
}

static ssize_t pcan_chardev_write(struct file *file, const char __user *buf, size_t count, loff_t *off)
{
    return -EOPNOTSUPP; /* TODO */
}

static long pcan_chardev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    usb_forwarder_t *forwarder = get_usb_forwarder_from_file(file);
    pcan_chardev_t *dev = likely(forwarder) ? &forwarder->char_dev : NULL;
    const ioctl_handler_t *handler = NULL;
    int err = likely(dev) ? 0 : -ENODEV;

    if (err)
        return err;

    CHRDEV_OP_PRECHECK(forwarder, file, -ENODEV);

    atomic_inc(&forwarder->pending_ops);

    switch (cmd)
    {
    /* Old commands. DEPRECATED! */
    case PCAN_IOCTL_INIT:
    case PCAN_IOCTL_WRITE_MSG:
    case PCAN_IOCTL_READ_MSG:
    case PCAN_IOCTL_GET_STATUS:
    case PCAN_IOCTL_GET_DIAGNOSIS:
    case PCAN_IOCTL_BTR0BTR1:
    case PCAN_IOCTL_GET_EXT_STATUS:
    case PCAN_IOCTL_SET_FILTER:
    case PCAN_IOCTL_EXT_PARAMS:
        handler = &G_IOCTL_HANDLERS[_IOC_NR(cmd) - PCAN_IOCTL_SEQ_START];
        if (PCAN_IOCTL_WRITE_MSG != cmd && PCAN_IOCTL_READ_MSG != cmd)
        {
            dev_notice_ratelimited_v(dev->device, "cmd[%s|0x%08x]: direction = %u, type = %u, number = %u, size = %u\n",
                handler->name, cmd, _IOC_DIR(cmd), _IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd)); \
        }
        err = likely(evol_access_ok(argp, _IOC_SIZE(cmd))) ? handler->func(file, forwarder, argp) : -EINVAL;
        break;

    /* New commands. */
    case PCANFD_IOCTL_SET_INIT:
    case PCANFD_IOCTL_GET_INIT:
    case PCANFD_IOCTL_GET_STATE:
    case PCANFD_IOCTL_SEND_MSG:
    case PCANFD_IOCTL_RECV_MSG:
    case PCANFD_IOCTL_SEND_MSGS:
    case PCANFD_IOCTL_RECV_MSGS:
    case PCANFD_IOCTL_GET_OPTION:
    case PCANFD_IOCTL_SET_OPTION:
    case PCANFD_IOCTL_RESET:
        handler = &G_FD_IOCTL_HANDLERS[_IOC_NR(cmd) - PCANFD_IOCTL_SEQ_START];
        if (PCANFD_IOCTL_SEND_MSG != cmd && PCANFD_IOCTL_RECV_MSG != cmd &&
            PCANFD_IOCTL_SEND_MSGS != cmd && PCANFD_IOCTL_RECV_MSGS != cmd)
        {
            dev_notice_ratelimited_v(dev->device, "cmd[%s|0x%08x]: direction = %u, type = %u, number = %u, size = %u\n",
                handler->name, cmd, _IOC_DIR(cmd), _IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd)); \
        }
        err = likely(evol_access_ok(argp, _IOC_SIZE(cmd))) ? handler->func(file, forwarder, argp) : -EINVAL;
        break;

    default:
        dev_err_ratelimited_v(dev->device, "unknown cmd: 0x%08x (direction = %u, type = %u, number = %u, size = %u)\n",
            cmd, _IOC_DIR(cmd), _IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd));
        err = -EBADRQC/*-EINVAL*/;
        break;
    } /* switch (cmd) */

    atomic_dec(&forwarder->pending_ops);

    return err;
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
 *
 * >>> 2023-12-02, Man Hung-Coeng <udc577@126.com>:
 *  01. Fix the wrong working flow of turning on CAN bus in open function.
 *
 * >>> 2023-12-12, Man Hung-Coeng <udc577@126.com>:
 *  01. Add pcan_chardev_initialize() and pcan_chardev_finalize().
 *  02. Implement pcan_chardev_poll(), *_read() and *_ioctl().
 */

