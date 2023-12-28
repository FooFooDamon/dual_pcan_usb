// SPDX-License-Identifier: GPL-2.0

/*
 * Implementation of chardev operations of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#include "chardev_operations.h"

#include <linux/module.h>
#include <linux/poll.h>
#include <linux/highmem.h> /* For kmap() series. */

#include "versions.h"
#include "common.h"
#include "klogging.h"
#include "can_commands.h"
#include "chardev_group.h"
#include "chardev_ioctl.h"
#include "usb_driver.h"
#include "evol_kernel.h"

#define DEFAULT_TIMEZONE                8
#define DEFAULT_MAP_UMEM_FLAG           0

static s16 timezone = DEFAULT_TIMEZONE;
module_param(timezone, short, 0644);
MODULE_PARM_DESC(timezone, " time zone (default: " __stringify(DEFAULT_TIMEZONE) ")");

static bool map_umem = DEFAULT_MAP_UMEM_FLAG;
module_param(map_umem, bool, 0644);
MODULE_PARM_DESC(map_umem, " whether to map user-space memory (default: " __stringify(DEFAULT_MAP_UMEM_FLAG) ")");

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
    dev->rd_user_buf = NULL;
    if (map_umem)
        dev->rd_mapped_addr = NULL;
    else
    {
        dev->rd_kernel_buf = kmalloc(PCAN_CHRDEV_MAX_BYTES_PER_READ * PCAN_CHRDEV_MAX_RX_BUF_COUNT + 1, GFP_KERNEL);
        if (NULL == dev->rd_kernel_buf)
        {
            CHRDEV_GRP_UNMAKE_ITEM(dev->device, NULL);

            return -ENOMEM;
        }
    }

    return 0;
}

static void unmap_user_readbuf_if_needed(pcan_chardev_t *dev)
{
    if (map_umem && dev->rd_mapped_addr)
    {
        struct page *user_page = kmap_to_page(dev->rd_mapped_addr);

        kunmap(user_page);
        put_page(user_page);
        dev->rd_mapped_addr = NULL;
    }
    dev->rd_user_buf = NULL;
}

void pcan_chardev_finalize(pcan_chardev_t *dev)
{
    unmap_user_readbuf_if_needed(dev);
    if (!map_umem && NULL != dev->rd_kernel_buf)
    {
        kfree(dev->rd_kernel_buf);
        dev->rd_kernel_buf = NULL;
    }

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
    ktime_get_real_ts64(&forwarder->bus_up_time);

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
        unmap_user_readbuf_if_needed(&forwarder->char_dev);
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

/* FIXME: Might be buggy ... Test it carefully with pcanusb_test.elf and cat! */
static ssize_t pcan_chardev_read(struct file *file, char __user *buf, size_t count, loff_t *off)
{
    usb_forwarder_t *forwarder = get_usb_forwarder_from_file(file);
    pcan_chardev_t *dev = likely(forwarder) ? &forwarder->char_dev : NULL;
    unsigned long lock_flags;
    int err = likely(dev) ? ((count > PCAN_CHRDEV_MAX_BYTES_PER_READ) ? 0 : -EINVAL) : -ENODEV;

    if (err)
        return err;

    CHRDEV_OP_PRECHECK(forwarder, file, -ENODEV);

    atomic_inc(&forwarder->pending_ops);

    if (file->f_flags & O_NONBLOCK)
    {
        if (atomic_read(&dev->rx_unread_cnt) <= 0)
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

        if (unlikely(atomic_read(&dev->rx_unread_cnt) <= 0))
        {
            err = signal_pending(current) ? -ERESTARTSYS/*-EINTR*/ : -EAGAIN;
            goto lbl_read_end;
        }
    }

    if (map_umem && buf != dev->rd_user_buf)
    {
        struct page *user_page = NULL;

        if (unlikely(!evol_access_ok(buf, count)))
        {
            err = -EINVAL;
            dev_err_ratelimited_v(dev->device, "access_ok() returned false\n");
            goto lbl_read_end;
        }

        unmap_user_readbuf_if_needed(dev);

        err = get_user_pages_fast((unsigned long)buf, 1, FOLL_WRITE, &user_page);
        if (err < 0)
            goto lbl_read_end;

        dev->rd_mapped_addr = (char *)kmap(user_page);
        dev->rd_user_buf = buf;
        dev_notice_ratelimited_v(dev->device, "Mapped read buffer: 0x%p -> 0x%p\n", buf, dev->rd_mapped_addr);
    }

    spin_lock_irqsave(&dev->lock, lock_flags);
    {
        char *buf_start = map_umem ? dev->rd_mapped_addr : dev->rd_kernel_buf;
        char *ptr = buf_start;
        int unread_msgs = atomic_read(&dev->rx_unread_cnt);
        int read_index = pcan_chardev_calc_rx_read_index(atomic_read(&dev->rx_write_idx), unread_msgs);
        int msgs_to_read = count / PCAN_CHRDEV_MAX_BYTES_PER_READ + ((count % PCAN_CHRDEV_MAX_BYTES_PER_READ) ? 0 : -1);
        int i;

        if (unlikely(unread_msgs <= 0))
            err = -EAGAIN;
        else
        {
            if (msgs_to_read > unread_msgs)
                msgs_to_read = unread_msgs;

            for (i = 0; i < msgs_to_read; ++i)
            {
                struct timespec64 tspec = forwarder->bus_up_time;
                struct tm when;
                pcan_chardev_msg_t *rx_msg = &dev->rx_msgs[read_index];
                struct can_frame *f = &rx_msg->frame;
                typeof(f->can_dlc) j;

                timespec64_add_ns(&tspec, ktime_to_ns(ktime_sub(rx_msg->hwtstamp, forwarder->time_ref.tv_host_0)));
                evol_time_to_tm(tspec.tv_sec, 60 * 60 * timezone, &when);
                ptr += sprintf(ptr, "(%04ld-%02d-%02d %02d:%02d:%02d.%06ld)  %s  %08X  [%d] ",
                    when.tm_year + 1900, when.tm_mon + 1, when.tm_mday, when.tm_hour, when.tm_min, when.tm_sec,
                    tspec.tv_nsec / 1000, dev_name(dev->device), (f->can_id & CAN_EFF_MASK), f->can_dlc);
                for (j = 0; j < f->can_dlc; ++j)
                {
                    ptr += sprintf(ptr, " %02X", f->data[j]);
                }
                *ptr++ = '\n';

                read_index = (read_index + 1) % PCAN_CHRDEV_MAX_RX_BUF_COUNT;
            }

            atomic_sub(msgs_to_read, &dev->rx_unread_cnt);
            err = ptr - buf_start;
            ptr[err] = '\0';
        }
    }
    spin_unlock_irqrestore(&dev->lock, lock_flags);

    if (!map_umem && err > 0)
        err -= copy_to_user(buf, dev->rd_kernel_buf, err);

lbl_read_end:

    atomic_dec(&forwarder->pending_ops);

    return err;
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
 *
 * >>> 2023-12-18, Man Hung-Coeng <udc577@126.com>:
 *  01. Rename this file from chardev_interfaces.c to chardev_operations.c.
 *
 * >>> 2023-12-23, Man Hung-Coeng <udc577@126.com>:
 *  01. Mark the CAN bus active time point in open function.
 *
 * >>> 2023-12-28, Man Hung-Coeng <udc577@126.com>:
 *  01. Re-implement the read function with character stream format.
 */

