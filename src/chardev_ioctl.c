// SPDX-License-Identifier: GPL-2.0

/*
 * Implementation of chardev ioctl.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#include "chardev_ioctl.h"

#include "versions.h"
#include "common.h"
#include "klogging.h"
#include "usb_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline u8 get_msgtype_from_canid(u32 can_id)
{
    if (unlikely(can_id & CAN_ERR_FLAG))
        return MSGTYPE_STATUS; /* FIXME: Not sure. */

    if (can_id & CAN_RTR_FLAG)
        return MSGTYPE_RTR;

    if (can_id & ~CAN_SFF_MASK)
        return MSGTYPE_EXTENDED; /* Definitely is. */

    return MSGTYPE_STANDARD; /* Might be. */
}

#define IOCTL_HANDLE_FUNC(name)                 ioctl_##name

#define DECLARE_IOCTL_HANDLE_FUNC(name)         \
    static int IOCTL_HANDLE_FUNC(name)(struct file *file, usb_forwarder_t *forwarder, void __user *arg)

DECLARE_IOCTL_HANDLE_FUNC(init)
{
    dev_warn_v(forwarder->char_dev.device, "FIXME: Implement this request in future!\n");

    return 0;
}

DECLARE_IOCTL_HANDLE_FUNC(write_msg)
{
    dev_err_ratelimited_v(forwarder->char_dev.device, "Request not supported yet!\n");

    return -EOPNOTSUPP;
}

DECLARE_IOCTL_HANDLE_FUNC(read_msg)
{
    pcan_chardev_t *dev = &forwarder->char_dev;
    unsigned long lock_flags;
    pcan_ioctl_rd_msg_t msg;
    int err = 0;

    if (file->f_flags & O_NONBLOCK)
    {
        if (atomic_read(&dev->rx_unread_cnt) <= 0)
            return -EAGAIN;
    }
    else
    {
        err = wait_event_interruptible(dev->wait_queue_rd,
            atomic_read(&dev->rx_unread_cnt) > 0 || atomic_read(&forwarder->stage) < PCAN_USB_STAGE_ONE_STARTED);

        if (err)
            return err;

        if (unlikely(atomic_read(&forwarder->stage) < PCAN_USB_STAGE_ONE_STARTED)) /* Has been plugged out. */
            return -ENODEV;

        if (unlikely(atomic_read(&dev->rx_unread_cnt) <= 0))
            return signal_pending(current) ? -ERESTARTSYS/*-EINTR*/ : -EAGAIN;
    }

    spin_lock_irqsave(&dev->lock, lock_flags);
    {
        int unread_msgs = atomic_read(&dev->rx_unread_cnt);
        int read_index = pcan_chardev_calc_rx_read_index(atomic_read(&dev->rx_write_idx), unread_msgs);
        struct can_frame *frame = &dev->rx_msgs[read_index].frame;
        s64 hardware_timestamp = ktime_to_ns(dev->rx_msgs[read_index].hwtstamp);

        if (unlikely(unread_msgs <= 0))
            err = -EAGAIN;
        else
        {
            msg.msg.id = frame->can_id;
            msg.msg.type = get_msgtype_from_canid(msg.msg.id); /* FIXME: Or fetch it from value passed by ioctl_init()? */
            msg.msg.len = frame->can_dlc;
            memcpy(&msg.msg.data, frame->data, msg.msg.len);
#if BITS_PER_LONG >= 64
            msg.time_msecs = hardware_timestamp / 1000000;
            msg.remainder_usecs = hardware_timestamp / 1000 - msg.time_msecs * 1000;
#else /* 64-bit divisions above will cause an error of "__aeabi_ldivmod undefined" on 32-bit ARM platforms. */
            {
                u32 remainder_nsecs = do_div(hardware_timestamp, 1000000);

                msg.time_msecs = hardware_timestamp;
                msg.remainder_usecs = remainder_nsecs / 1000;
            }
#endif

            atomic_dec(&dev->rx_unread_cnt);
        }
    }
    spin_unlock_irqrestore(&dev->lock, lock_flags);

    return unlikely(err) ? err : (__copy_to_user(arg, &msg, sizeof(msg)) ? -EFAULT : 0);
}

DECLARE_IOCTL_HANDLE_FUNC(get_status)
{
    dev_warn_ratelimited_v(forwarder->char_dev.device, "FIXME: Implement this request in future!\n");

    return 0;
}

DECLARE_IOCTL_HANDLE_FUNC(get_diagnostic_info)
{
    pcan_chardev_t *dev = &forwarder->char_dev;
    pcan_ioctl_diag_t diag = {
        .hardware_type = PRODUCT_TYPE,
        .base = dev->serial_number,
        .irq_level = dev->device_id,
        .read_count = dev->rx_packets,
        /* TODO: Use other fields in future. */
        .open_paths = atomic_read(&dev->open_count),
        .version = { DRIVER_VERSION "-" __VER__ },
    };

    return __copy_to_user(arg, &diag, sizeof(diag)) ? -EFAULT : 0;
}

DECLARE_IOCTL_HANDLE_FUNC(btr0btr1)
{
    dev_warn_ratelimited_v(forwarder->char_dev.device, "FIXME: Implement this request in future!\n");

    return 0;
}

DECLARE_IOCTL_HANDLE_FUNC(get_extra_status)
{
    pcan_chardev_t *dev = &forwarder->char_dev;
    pcan_ioctl_extra_status_t ext_status = {
        .pending_reads = atomic_read(&dev->rx_unread_cnt)
        /* TODO: Use other fields in future. */
    };

    return __copy_to_user(arg, &ext_status, sizeof(ext_status)) ? -EFAULT : 0;
}

DECLARE_IOCTL_HANDLE_FUNC(set_filter)
{
    dev_err_v(forwarder->char_dev.device, "Request not supported yet!\n");

    return -EOPNOTSUPP;
}

DECLARE_IOCTL_HANDLE_FUNC(extra_params)
{
    pcan_chardev_t *dev = &forwarder->char_dev;
    pcan_ioctl_extra_params_t ext_params;

    if (unlikely(__copy_from_user(&ext_params, arg, sizeof(ext_params))))
    {
        dev_err_v(dev->device, "__copy_from_user() failed\n");

        return -EFAULT;
    }

    memset(ext_params.func_value.device_data, 0, sizeof(ext_params.func_value.device_data));

    switch (ext_params.sub_function & 0xff)
    {
    case SF_GET_SERIALNUMBER:
        ext_params.func_value.serial_num = dev->serial_number;
        break;

    case SF_GET_HCDEVICENO:
        ext_params.func_value.device_num = dev->device_id;
        break;

    case SF_GET_DEVDATA:
        strcpy((char *)ext_params.func_value.device_data, __DRVNAME__);
        break;

    default:
        dev_err_v(dev->device, "Unknown sub_function %d\n", ext_params.sub_function);
        break;
    }

    return __copy_to_user(arg, &ext_params, sizeof(ext_params)) ? -EFAULT : 0;
}

/* Old handlers. DEPRECATED! */
const ioctl_handler_t G_IOCTL_HANDLERS[] = {
    { "INIT", IOCTL_HANDLE_FUNC(init) },
    { "WRITE_MSG", IOCTL_HANDLE_FUNC(write_msg) },
    { "READ_MSG", IOCTL_HANDLE_FUNC(read_msg) },
    { "GET_STATUS", IOCTL_HANDLE_FUNC(get_status) },
    { "GET_DIAGNOSIS", IOCTL_HANDLE_FUNC(get_diagnostic_info) },
    { "BTR0BTR1", IOCTL_HANDLE_FUNC(btr0btr1) },
    { "GET_EXT_STATUS", IOCTL_HANDLE_FUNC(get_extra_status) },
    { "SET_FILTER", IOCTL_HANDLE_FUNC(set_filter) },
    { "EXT_PARAMS", IOCTL_HANDLE_FUNC(extra_params) },
};

#define PRINT_IOCTL_INIT_PARAMS(params, dev)        do { \
    pcan_bittiming_t *nbt = &params.nominal; \
    pcan_bittiming_t *dbt = &params.data; \
\
    dev_notice_v(dev->device, "flags = 0x%08x, clock_hz = %u\n", params.flags, params.clock_hz); \
    dev_notice_v(dev->device, "nominal bittiming: brp = %u, tseg1 = %u, tseg2 = %u, sjw = %u, tsam = %u," \
        " bitrate = %u, sample_point = %u, tq = %u, bitrate_real = %u\n", \
        nbt->brp, nbt->tseg1, nbt->tseg2, nbt->sjw, nbt->tsam, \
        nbt->bitrate, nbt->sample_point, nbt->tq, nbt->bitrate_real); \
    dev_notice_v(dev->device, "data bittiming: brp = %u, tseg1 = %u, tseg2 = %u, sjw = %u, tsam = %u," \
        " bitrate = %u, sample_point = %u, tq = %u, bitrate_real = %u\n", \
        dbt->brp, dbt->tseg1, dbt->tseg2, dbt->sjw, dbt->tsam, \
        dbt->bitrate, dbt->sample_point, dbt->tq, dbt->bitrate_real); \
} while (0)

DECLARE_IOCTL_HANDLE_FUNC(fd_set_init)
{
    pcan_chardev_t *dev = &forwarder->char_dev;
    pcanfd_ioctl_init_t params;

    if (unlikely(__copy_from_user(&params, arg, sizeof(params))))
    {
        dev_err_v(dev->device, "__copy_from_user() failed\n");

        return -EFAULT;
    }

    PRINT_IOCTL_INIT_PARAMS(params, dev);

    dev->ioctl_init_flags = params.flags;

    dev_warn_v(dev->device, "Request is not supported! Set these parameters via network interface,"
        " or just ignore this warning!\n");

    return 0;
}

DECLARE_IOCTL_HANDLE_FUNC(fd_get_init)
{
    struct can_priv *can = &forwarder->can;
    struct can_bittiming *nbt = &can->bittiming;
    struct can_bittiming *dbt = &can->data_bittiming;
    pcan_chardev_t *dev = &forwarder->char_dev;
    pcanfd_ioctl_init_t params = {
        .flags = dev->ioctl_init_flags,
        .clock_hz = can->clock.freq,
        .nominal = {
            .brp = nbt->brp,
            .tseg1 = nbt->phase_seg1,
            .tseg2 = nbt->phase_seg2,
            .sjw = nbt->sjw,

            .bitrate = nbt->bitrate,
            .sample_point = nbt->sample_point,
            .tq = nbt->tq,
            .bitrate_real = nbt->bitrate,
        },
        .data = {
            .brp = dbt->brp,
            .tseg1 = dbt->phase_seg1,
            .tseg2 = dbt->phase_seg2,
            .sjw = dbt->sjw,

            .bitrate = dbt->bitrate,
            .sample_point = dbt->sample_point,
            .tq = dbt->tq,
            .bitrate_real = dbt->bitrate,
        },
    };

    dev_notice_v(dev->device, "Fetched (most) parameters from netdev[%s].\n", netdev_name(forwarder->net_dev));

    PRINT_IOCTL_INIT_PARAMS(params, dev);

    return __copy_to_user(arg, &params, sizeof(params)) ? -EFAULT : 0;
}

DECLARE_IOCTL_HANDLE_FUNC(fd_get_state)
{
    pcan_chardev_t *dev = &forwarder->char_dev;
    pcanfd_ioctl_state_t state = {
        .ver_major = DRV_VER_MAJOR,
        .ver_minor = DRV_VER_MINOR,
        .ver_subminor = DRV_VER_RELEASE,
        .bus_state = PCANFD_ERROR_ACTIVE, /* TODO: More possibilities in future. */
        .device_id = dev->device_id,
        .open_counter = atomic_read(&dev->open_count),
        .hw_type = PRODUCT_TYPE,
        .channel_number = MINOR(file->f_inode->i_rdev) - DEV_MINOR_BASE,
        .can_status = 0, /* TODO: More possibilities in future. */
        .bus_load = 0xffff, /* FIXME: 0xffff means "not given". Maybe give it in future. */
        .rx_max_msgs = PCAN_CHRDEV_MAX_RX_BUF_COUNT,
        .rx_pending_msgs = atomic_read(&dev->rx_unread_cnt),
    };

    return __copy_to_user(arg, &state, sizeof(state)) ? -EFAULT : 0;
}

DECLARE_IOCTL_HANDLE_FUNC(fd_send_msg)
{
    dev_err_ratelimited_v(forwarder->char_dev.device, "Request not supported yet!\n");

    return -EOPNOTSUPP;
}

DECLARE_IOCTL_HANDLE_FUNC(fd_recv_msg)
{
    dev_err_ratelimited_v(forwarder->char_dev.device, "Request not supported yet!\n");

    return -EOPNOTSUPP;
}

DECLARE_IOCTL_HANDLE_FUNC(fd_send_msgs)
{
    dev_err_ratelimited_v(forwarder->char_dev.device, "Request not supported yet!\n");

    return -EOPNOTSUPP;
}

DECLARE_IOCTL_HANDLE_FUNC(fd_recv_msgs)
{
    pcan_chardev_t *dev = &forwarder->char_dev;
    pcanfd_ioctl_msgs_t *msgp = dev->ioctl_rxmsgs;
    unsigned long lock_flags;
    int err = 0;

    if (file->f_flags & O_NONBLOCK)
    {
        if (atomic_read(&dev->rx_unread_cnt) <= 0)
            return -EAGAIN;
    }
    else
    {
        err = wait_event_interruptible(dev->wait_queue_rd,
            atomic_read(&dev->rx_unread_cnt) > 0 || atomic_read(&forwarder->stage) < PCAN_USB_STAGE_ONE_STARTED);

        if (err)
            return err;

        if (unlikely(atomic_read(&forwarder->stage) < PCAN_USB_STAGE_ONE_STARTED)) /* Has been plugged out. */
            return -ENODEV;

        if (unlikely(atomic_read(&dev->rx_unread_cnt) <= 0))
            return signal_pending(current) ? -ERESTARTSYS/*-EINTR*/ : -EAGAIN;
    }

    if (unlikely(__get_user(msgp->count, (typeof(msgp->count) *)arg)))
    {
        dev_err_v(dev->device, "__get_user() failed\n");

        return -EFAULT;
    }

    if (unlikely(0 == msgp->count))
        return -EINVAL;

    spin_lock_irqsave(&dev->lock, lock_flags);
    {
        int unread_msgs = atomic_read(&dev->rx_unread_cnt);
        int read_index = pcan_chardev_calc_rx_read_index(atomic_read(&dev->rx_write_idx), unread_msgs);
        typeof(msgp->count) i;

        if (unlikely(unread_msgs <= 0))
            err = -EAGAIN;
        else
        {
            if (msgp->count > (u32)unread_msgs)
                msgp->count = unread_msgs;

            for (i = 0; i < msgp->count; ++i)
            {
                pcan_chardev_msg_t *rx_msg = &dev->rx_msgs[read_index];
                struct can_frame *f = &rx_msg->frame;
                pcanfd_ioctl_msg_t *m = &msgp->list[i];
                struct timespec64 tspec = forwarder->bus_up_time;

                m->id = f->can_id & CAN_EFF_MASK; /* FIXME: It should have been okay even if not using CAN_EFF_MASK. */
                m->data_len = f->can_dlc;
                memcpy(m->data, f->data, m->data_len);
                m->type = PCANFD_TYPE_CAN20_MSG; /* FIXME: More possibilities in future. */
                m->flags = get_msgtype_from_canid(f->can_id); /* FIXME: Also decided by the type above. */
                m->flags |= PCANFD_TIMESTAMP | PCANFD_HWTIMESTAMP;
                timespec64_add_ns(&tspec, ktime_to_ns(ktime_sub(rx_msg->hwtstamp, forwarder->time_ref.tv_host_0)));
                m->timestamp.tv_sec = tspec.tv_sec;
                m->timestamp.tv_usec = tspec.tv_nsec / 1000;
                /* TODO: ctrlr_data */

                read_index = (read_index + 1) % PCAN_CHRDEV_MAX_RX_BUF_COUNT;
            }

            atomic_sub(msgp->count, &dev->rx_unread_cnt);
        }
    }
    spin_unlock_irqrestore(&dev->lock, lock_flags);

    return unlikely(err) ? err : (unlikely(copy_to_user(arg, msgp, SIZE_OF_PCANFD_IOCTL_MSGS(msgp->count))) ? -EFAULT : 0);
}

static inline const char* pcanfd_option_name(int index)
{
    static const char *S_OPT_NAMES[] = {
        "PCANFD_OPT_CHANNEL_FEATURES",
        "PCANFD_OPT_DEVICE_ID",
        "PCANFD_OPT_AVAILABLE_CLOCKS",
        "PCANFD_OPT_BITTIMING_RANGES",
        "PCANFD_OPT_DBITTIMING_RANGES",
        "PCANFD_OPT_ALLOWED_MSGS",
        "PCANFD_OPT_ACC_FILTER_11B",
        "PCANFD_OPT_ACC_FILTER_29B",
        "PCANFD_OPT_IFRAME_DELAYUS",
        "PCANFD_OPT_HWTIMESTAMP_MODE",
        "PCANFD_OPT_DRV_VERSION",
        "PCANFD_OPT_FW_VERSION",
        "PCANFD_IO_DIGITAL_CFG",
        "PCANFD_IO_DIGITAL_VAL",
        "PCANFD_IO_DIGITAL_SET",
        "PCANFD_IO_DIGITAL_CLR",
        "PCANFD_IO_ANALOG_VAL",
        "PCANFD_OPT_MASS_STORAGE_MODE",
        "PCANFD_OPT_FLASH_LED",
        "PCANFD_OPT_DRV_CLK_REF",
        "PCANFD_OPT_LINGER",
        "PCANFD_OPT_SELF_ACK",
        "PCANFD_OPT_BRS_IGNORE",
    };

    return (index >=0 && index < PCANFD_OPT_MAX) ? S_OPT_NAMES[index] : "UNKNOWN_OPTION";
}

DECLARE_IOCTL_HANDLE_FUNC(fd_get_option)
{
    pcan_chardev_t *dev = &forwarder->char_dev;
    pcanfd_ioctl_option_t opt;
    u32 u32_val = 0;

    if (unlikely(__copy_from_user(&opt, arg, sizeof(opt))))
    {
        dev_err_v(dev->device, "__copy_from_user() failed\n");

        return -EFAULT;
    }

    dev_notice_v(dev->device, "name = %d(%s), size = %d\n", opt.name, pcanfd_option_name(opt.name), opt.size);

    switch (opt.name)
    {
    case PCANFD_OPT_CHANNEL_FEATURES:
        u32_val = PCANFD_FEATURE_HWTIMESTAMP | PCANFD_FEATURE_DEVICEID;
        break;

    case PCANFD_OPT_HWTIMESTAMP_MODE:
        u32_val = PCANFD_OPT_HWTIMESTAMP_RAW;
        break;

    default:
        dev_err_v(dev->device, "Not supported!\n");
    }

    switch (opt.name)
    {
    case PCANFD_OPT_CHANNEL_FEATURES:
    case PCANFD_OPT_HWTIMESTAMP_MODE:
#if 0
        return copy_to_user(opt.value, &u32_val, sizeof(u32_val)) ? -EFAULT : 0;
#else
        return put_user(u32_val, (u32 *)opt.value);
#endif

    default:
        return -EOPNOTSUPP;
    }
}

DECLARE_IOCTL_HANDLE_FUNC(fd_set_option)
{
    pcan_chardev_t *dev = &forwarder->char_dev;
    pcanfd_ioctl_option_t opt;

    if (unlikely(__copy_from_user(&opt, arg, sizeof(opt))))
    {
        dev_err_v(dev->device, "__copy_from_user() failed\n");

        return -EFAULT;
    }

    dev_notice_v(dev->device, "name = %d(%s), size = %d\n", opt.name, pcanfd_option_name(opt.name), opt.size);

    dev_warn_ratelimited_v(dev->device, "FIXME: Implement this request in future!\n");

    return 0;
}

DECLARE_IOCTL_HANDLE_FUNC(fd_reset)
{
    dev_warn_ratelimited_v(forwarder->char_dev.device, "FIXME: Implement this request in future!\n");

    return 0;
}

/* New handlers. */
const ioctl_handler_t G_FD_IOCTL_HANDLERS[] = {
    { "FD_SET_INIT", IOCTL_HANDLE_FUNC(fd_set_init) },
    { "FD_GET_INIT", IOCTL_HANDLE_FUNC(fd_get_init) },
    { "FD_GET_STATE", IOCTL_HANDLE_FUNC(fd_get_state) },
    { "FD_ADD_FILTERS", NULL },
    { "FD_GET_FILTERS", NULL },
    { "FD_SEND_MSG", IOCTL_HANDLE_FUNC(fd_send_msg) },
    { "FD_RECV_MSG", IOCTL_HANDLE_FUNC(fd_recv_msg) },
    { "FD_SEND_MSGS", IOCTL_HANDLE_FUNC(fd_send_msgs) },
    { "FD_RECV_MSGS", IOCTL_HANDLE_FUNC(fd_recv_msgs) },
    { "FD_GET_AVAILABLE_CLOCKS", NULL },
    { "FD_GET_BITTIMING_RANGES", NULL },
    { "FD_GET_OPTION", IOCTL_HANDLE_FUNC(fd_get_option) },
    { "FD_SET_OPTION", IOCTL_HANDLE_FUNC(fd_set_option) },
    { "FD_RESET", IOCTL_HANDLE_FUNC(fd_reset) },
};

#ifdef __cplusplus
}
#endif

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-12-12, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-12-23, Man Hung-Coeng <udc577@126.com>:
 *  01. Implement the timestamp calculation for ioctl message reception.
 *
 * >>> 2023-12-28, Man Hung-Coeng <udc577@126.com>:
 *  01. Optimize the logic of fetching the counter of unread messages,
 *      which can avoid missing some messages due to the old value of counter.
 */

