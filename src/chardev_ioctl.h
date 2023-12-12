/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Definitions of chardev ioctl.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#ifndef __CHARDEV_IOCTL_H__
#define __CHARDEV_IOCTL_H__

#include <linux/version.h> /* For LINUX_VERSION_CODE. */
#include <linux/types.h>
#include <linux/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* type agument of ioctl() commands */
#define PCAN_MAGIC_NUMBER               'z'

/******************************************************************************
 * Old definitions for ioctl().
 *****************************************************************************/

#define MSGTYPE_STANDARD                0x00 /* standard frame */
#define MSGTYPE_RTR                     0x01 /* remote frame */
#define MSGTYPE_EXTENDED                0x02 /* extended frame */
#define MSGTYPE_SELFRECEIVE             0x04 /* self-received message */
#define MSGTYPE_SINGLESHOT              0x08 /* single-shot message */
#define MSGTYPE_STATUS                  0x80 /* status message */

typedef struct pcan_ioctl_init
{
    __u16 btr0btr1;
    __u8 msg_type;                      /* See MSGTYPE_* above. */
    __u8 is_listen_only;
} pcan_ioctl_init_t;

typedef struct pcan_ioctl_wr_msg
{
    __u32 id;
    __u8 type;                          /* See MSGTYPE_* above. */
    __u8 len;
    __u8 data[8];
} pcan_ioctl_wr_msg_t;

typedef struct pcan_ioctl_rd_msg
{
    pcan_ioctl_wr_msg_t msg;
    __u32 time_msecs;
    __u16 remainder_usecs;
} pcan_ioctl_rd_msg_t;

typedef struct pcan_ioctl_status
{
    __u16 error_flag;
    int last_error;
} pcan_ioctl_status_t;

typedef struct pcan_ioctl_diag
{
	__u16 hardware_type;                /* fixed to 11 */
	__u32 base;                         /* base address or port of this device */
	__u16 irq_level;                    /* irq level of this device */
	__u32 read_count;                   /* counts all reads to this device from start */
	__u32 write_count;                  /* counts all writes */
	__u32 irq_count;                    /* counts all interrupts */
	__u32 error_count;                  /* counts all errors */
	__u16 error_flag;                   /* gathers all errors */
	int last_error;                     /* the last local error for this device */
	int open_paths;                     /* number of open paths for this device */
	char version[64];
} pcan_ioctl_diag_t;

typedef struct pcan_ioctl_btr0btr1
{
    __u32 bitrate;
    __u16 btr0btr1;
} pcan_ioctl_btr0btr1_t;

typedef struct pcan_ioctl_extra_status
{
	__u16 error_flag;
	int last_error;
    int pending_reads;
    int pending_writes;
} pcan_ioctl_extra_status_t;

typedef struct pcan_ioctl_msg_filter
{
    __u32 from_id;
    __u32 to_id;
    __u8 msg_type;                      /* MSGTYPE_* */
} pcan_ioctl_msg_filter_t;

#define PCAN_SF_SET(f)                  (int )((f) << 1)
#define PCAN_SF_GET(f)                  (PCAN_SF_SET(f) - 1)

/* 32-bit func compatibles */
#define PCAN_SF_SERIALNUMBER            1
#define PCAN_SF_DEVICENO                2
#define PCAN_SF_FWVERSION               3
#define PCAN_SF_MAX32                   63

/* 64 bytes func */
#define PCAN_SF_ADAPTERNAME             65
#define PCAN_SF_PARTNUM                 66
#define PCAN_SF_MAX                     127

#define SF_GET_SERIALNUMBER             PCAN_SF_GET(PCAN_SF_SERIALNUMBER)
#define SF_GET_HCDEVICENO               PCAN_SF_GET(PCAN_SF_DEVICENO)
#define SF_SET_HCDEVICENO               PCAN_SF_SET(PCAN_SF_DEVICENO)
#define SF_GET_FWVERSION                PCAN_SF_GET(PCAN_SF_FWVERSION)
#define SF_GET_ADAPTERNAME              PCAN_SF_GET(PCAN_SF_ADAPTERNAME)
#define SF_GET_PARTNUM                  PCAN_SF_GET(PCAN_SF_PARTNUM)

#define PCAN_SF_DEVDATA                 (PCAN_SF_MAX - 1)
#define SF_GET_DEVDATA                  PCAN_SF_GET(PCAN_SF_DEVDATA)
#define SF_SET_DEVDATA                  PCAN_SF_SET(PCAN_SF_DEVDATA)

typedef struct pcan_ioctl_extra_params
{
    int sub_function;
    union
    {
        __u32 serial_num;
        __u8 device_num;
        __u8 device_data[64];
    } func_value;
} pcan_ioctl_extra_params_t;

#define PCAN_IOCTL_SEQ_START            0x80

#define PCAN_IOCTL_INIT                 _IOWR(PCAN_MAGIC_NUMBER, PCAN_IOCTL_SEQ_START, pcan_ioctl_init_t)
#define PCAN_IOCTL_WRITE_MSG            _IOW(PCAN_MAGIC_NUMBER, PCAN_IOCTL_SEQ_START + 1, pcan_ioctl_wr_msg_t)
#define PCAN_IOCTL_READ_MSG             _IOR(PCAN_MAGIC_NUMBER, PCAN_IOCTL_SEQ_START + 2, pcan_ioctl_rd_msg_t)
#define PCAN_IOCTL_GET_STATUS           _IOR(PCAN_MAGIC_NUMBER, PCAN_IOCTL_SEQ_START + 3, pcan_ioctl_status_t)
#define PCAN_IOCTL_GET_DIAGNOSIS        _IOR(PCAN_MAGIC_NUMBER, PCAN_IOCTL_SEQ_START + 4, pcan_ioctl_diag_t)
#define PCAN_IOCTL_BTR0BTR1             _IOWR(PCAN_MAGIC_NUMBER, PCAN_IOCTL_SEQ_START + 5, pcan_ioctl_btr0btr1_t)
#define PCAN_IOCTL_GET_EXT_STATUS       _IOR(PCAN_MAGIC_NUMBER, PCAN_IOCTL_SEQ_START + 6, pcan_ioctl_extra_status_t)
#define PCAN_IOCTL_SET_FILTER           _IOW(PCAN_MAGIC_NUMBER, PCAN_IOCTL_SEQ_START + 7, pcan_ioctl_msg_filter_t)
#define PCAN_IOCTL_EXT_PARAMS           _IOWR(PCAN_MAGIC_NUMBER, PCAN_IOCTL_SEQ_START + 8, pcan_ioctl_extra_params_t)

/******************************************************************************
 * New definitions for ioctl().
 *****************************************************************************/

/*
 * if "bitrate" is not 0, then it is used,
 * if "bitrate" is 0, then all other 4 (??) fields are used instead if all are not 0
 */
typedef struct pcan_bittiming {
    __u32 brp;
    __u32 tseg1;
    __u32 tseg2;
    __u32 sjw;
    __u32 tsam;                         /* triple sampling */

    __u32 bitrate;                      /* bps */
    __u32 sample_point;                 /* in 1/100 th of % (8750 = 87.50%) */
    __u32 tq;                           /* Time quantum in ns. */
    __u32 bitrate_real;                 /* info only */
} pcan_bittiming_t;

typedef struct pcanfd_ioctl_init {
    __u32 flags;
    __u32 clock_hz;
    struct pcan_bittiming nominal;
    struct pcan_bittiming data;
} pcanfd_ioctl_init_t;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
/* struct timeval is deprecated
 * (see https://www.kernel.org/doc/html/latest/core-api/timekeeping.html)
 *
 * see also: include/uapi/asm-generic/posix_types.h
 */
struct timeval {
    __kernel_old_time_t	tv_sec;         /* seconds */
    __kernel_suseconds_t tv_usec;       /* microseconds */
};
#else
#include <linux/time.h>
#endif

enum pcanfd_status {

    PCANFD_UNKNOWN,

    /* flags & PCANFD_ERROR_BUS.
     * event id. is the state of the Bus
     */
    PCANFD_ERROR_ACTIVE,
    PCANFD_ERROR_WARNING,
    PCANFD_ERROR_PASSIVE,               /* receive only state */
    PCANFD_ERROR_BUSOFF,                /* switched off from the bus */

    /* flags & PCANFD_ERROR_CTRLR|PCANFD_ERROR_INTERNAL.
     * event id. is one of the following error:
     */
    PCANFD_RX_EMPTY,
    PCANFD_RX_OVERFLOW,
    PCANFD_RESERVED_1,
    PCANFD_TX_OVERFLOW,

    PCANFD_RESERVED_2,
    PCANFD_BUS_LOAD,

    PCANFD_STATUS_COUNT
};

typedef struct pcanfd_ioctl_state {
    __u16	ver_major, ver_minor, ver_subminor;

    struct timeval tv_init;             /* time the device was initialized */

    enum pcanfd_status bus_state;       /* CAN bus state */

    __u32 device_id;                    /* device id. 0xffffffff is unused */

    __u32 open_counter;                 /* open() counter */
    __u32 filters_counter;              /* count of message filters */

    __u16 hw_type;                      /* pcan hareware type, fixed to 11 (or 18 ??) */
    __u16 channel_number;               /* channel number for the device */

    __u16 can_status;                   /* same as wCANStatus but NOT CLEARED */
    __u16 bus_load;                     /* bus load value, ffff if not given */

    __u32 tx_max_msgs;                  /* Tx fifo size in count of msgs */
    __u32 tx_pending_msgs;              /* msgs waiting to be sent */
    __u32 rx_max_msgs;                  /* Rx fifo size in count of msgs */
    __u32 rx_pending_msgs;              /* msgs waiting to be read */
    __u32 tx_frames_counter;            /* Tx frames written on device */
    __u32 rx_frames_counter;            /* Rx frames read on device */
    __u32 tx_error_counter;             /* CAN Tx errors counter */
    __u32 rx_error_counter;             /* CAN Rx errors counter */

    __u64 host_time_ns;                 /* host time in nanoseconds as it was */
    __u64 hw_time_ns;                   /* when hw_time_ns has been received */
} pcanfd_ioctl_state_t;

struct pcan_timeval
{
    struct timeval tv;                  /* host base time */
    __u64 tv_us;                        /* hw base time */
    __u64 ts_us;                        /* event hw time */
    __u32 ts_mode;                      /* cooking mode */
    long clock_drift;                   /* clock drift */
};

/* CAN-FD message types */
#define PCANFD_TYPE_NOP                 0
#define PCANFD_TYPE_CAN20_MSG           1
#define PCANFD_TYPE_CANFD_MSG           2
#define PCANFD_TYPE_STATUS              3
#define PCANFD_TYPE_ERROR_MSG           4

/* [PCANFD_TYPE_CAN20_MSG]
 * [PCANFD_TYPE_CANFD_MSG]
 *
 * flags bits definition (lowest byte is backward compatible with old MSGTYPE)
 */
#define PCANFD_MSG_STD                  0x00000000
#define PCANFD_MSG_RTR                  0x00000001
#define PCANFD_MSG_EXT                  0x00000002
#define PCANFD_MSG_SLF                  0x00000004
#define PCANFD_MSG_SNG                  0x00000008
#define PCANFD_MSG_ECHO                 0x00000010

/* [PCANFD_TYPE_STATUS]
 *
 * flags bits definition: indicate the kind of error/status:
 */
#define PCANFD_ERROR_BUS                0x00000080  /* Bus status */
#define PCANFD_ERROR_PROTOCOL           0x00000100  /* Protocol error */
#define PCANFD_ERROR_CTRLR              0x00000200  /* Controller error */
#define PCANFD_ERROR_INTERNAL           0x00000400  /* Internal error */

/* [PCANFD_TYPE_ERROR_MSG]
 *
 * flags bits definition: indicate direction
 */
#define PCANFD_ERRMSG_RX                0x00001000  /* err frame received */
#define PCANFD_ERRMSG_GEN               0x00002000  /* triggered by err generator */

/* indexes describing content of ctrlr_data array */
enum
{
    PCANFD_RXERRCNT,
    PCANFD_ECHOID = PCANFD_RXERRCNT,    /* PCANFD_MSG_ECHO set */
    PCANFD_TXERRCNT,
    PCANFD_BUSLOAD_UNIT,
    PCANFD_BUSLOAD_DEC,
    PCANFD_MAXCTRLRDATALEN
};

typedef struct pcanfd_ioctl_msg
{
    __u16 type;                         /* PCANFD_TYPE_* */
    __u16 data_len;                     /* true length (not the DLC) */
    __u32 id;                           /* CAN / STATUS / ERROR Id. */
    __u32 flags;                        /* see flag bits definition of [PCANFD_TYPE_*] above */
    struct timeval timestamp;           /* timestamp of the event */
    __u8 ctrlr_data[PCANFD_MAXCTRLRDATALEN];
    __u8 data[64] __attribute__((aligned(8)));
} pcanfd_ioctl_msg_t;

typedef struct pcanfd_ioctl_msgs
{
    __u32 count;
    struct pcanfd_ioctl_msg list[0];
} pcanfd_ioctl_msgs_t;

#define SIZE_OF_PCANFD_IOCTL_MSGS(count)    (sizeof(pcanfd_ioctl_msgs_t) + sizeof(pcanfd_ioctl_msg_t) * (count))

/* PCANFD_OPT_CHANNEL_FEATURES option:
 * features of a channel
 */
#define PCANFD_FEATURE_HWTIMESTAMP      0x00000008
#define PCANFD_FEATURE_DEVICEID         0x00000010

/* PCANFD_OPT_ALLOWED_MSGS option:
 * bitmask of allowed message an application is able to receive
 */
#define PCANFD_ALLOWED_MSG_CAN          0x00000001
#define PCANFD_ALLOWED_MSG_RTR          0x00000002
#define PCANFD_ALLOWED_MSG_EXT          0x00000004
#define PCANFD_ALLOWED_MSG_STATUS       0x00000010
#define PCANFD_ALLOWED_MSG_ERROR        0x00000100
#define PCANFD_ALLOWED_MSG_ALL          0xffffffff
#define PCANFD_ALLOWED_MSG_NONE         0x00000000

/* PCANFD_OPT_HWTIMESTAMP_MODE option:
 * 0    off (host timestamp at the time the event has been saved)
 * 1    on (host time base + device raw time offset)
 * 2    on (host time base + device time offset handling clock drift)
 * 3    on (device timestamp in struct timeval format)
 * 4    reserved
 * 5    same as 1 + ts is generated at SOF rather than at EOF (if hw allows it)
 * 6    same as 2 + ts is generated at SOF rather than at EOF (if hw allows it)
 * 7    same as 3 + ts is generated at SOF rather than at EOF (if hw allows it)
 */
enum
{
	PCANFD_OPT_HWTIMESTAMP_OFF,
	PCANFD_OPT_HWTIMESTAMP_ON,
	PCANFD_OPT_HWTIMESTAMP_COOKED,
	PCANFD_OPT_HWTIMESTAMP_RAW,
	PCANFD_OPT_HWTIMESTAMP_RESERVED_4,
	PCANFD_OPT_HWTIMESTAMP_SOF_ON,
	PCANFD_OPT_HWTIMESTAMP_SOF_COOKED,
	PCANFD_OPT_HWTIMESTAMP_SOF_RAW,

	PCANFD_OPT_HWTIMESTAMP_MAX
};

enum
{
    PCANFD_OPT_CHANNEL_FEATURES,        /* see PCANFD_FEATURE_* */
    PCANFD_OPT_DEVICE_ID,
    PCANFD_OPT_AVAILABLE_CLOCKS,        /* supersedes */
    PCANFD_OPT_BITTIMING_RANGES,        /* corresponding */
    PCANFD_OPT_DBITTIMING_RANGES,       /* ioctl() below */
    PCANFD_OPT_ALLOWED_MSGS,            /* see PCANFD_ALLOWED_MSG_* above */
    PCANFD_OPT_ACC_FILTER_11B,
    PCANFD_OPT_ACC_FILTER_29B,
    PCANFD_OPT_IFRAME_DELAYUS,
    PCANFD_OPT_HWTIMESTAMP_MODE,        /* see PCANFD_OPT_HWTIMESTAMP_* */
    PCANFD_OPT_DRV_VERSION,
    PCANFD_OPT_FW_VERSION,
    PCANFD_IO_DIGITAL_CFG,              /* output mode 1: output active */
    PCANFD_IO_DIGITAL_VAL,              /* digital I/O 32-bit value */
    PCANFD_IO_DIGITAL_SET,              /* multiple dig I/O pins to 1=High */
    PCANFD_IO_DIGITAL_CLR,              /* clr multiple dig I/O pins to 0 */
    PCANFD_IO_ANALOG_VAL,               /* get single analog input pin value */
    PCANFD_OPT_MASS_STORAGE_MODE,       /* all USB FD and some USB devices */
    PCANFD_OPT_FLASH_LED,
    PCANFD_OPT_DRV_CLK_REF,
    PCANFD_OPT_LINGER,
    PCANFD_OPT_SELF_ACK,                /* send ACK to self-written frames */
    PCANFD_OPT_BRS_IGNORE,              /* ignore BRS frames (no error frames sent) */

    PCANFD_OPT_DEFERRED_FRM,            /* internal use only */

    PCANFD_OPT_MAX
};

typedef struct pcanfd_ioctl_option
{
    int size;
    int name;                           /* see PCANFD_OPT_* above */
    void *value;
} pcanfd_ioctl_option_t;

#define PCANFD_IOCTL_SEQ_START          0x90

enum
{
    PCANFD_SEQ_SET_INIT = PCANFD_IOCTL_SEQ_START,
    PCANFD_SEQ_GET_INIT,
    PCANFD_SEQ_GET_STATE,
    PCANFD_SEQ_ADD_FILTERS,
    PCANFD_SEQ_GET_FILTERS,
    PCANFD_SEQ_SEND_MSG,
    PCANFD_SEQ_RECV_MSG,
    PCANFD_SEQ_SEND_MSGS,
    PCANFD_SEQ_RECV_MSGS,
    PCANFD_SEQ_GET_AVAILABLE_CLOCKS,    /* deprecated, use related */
    PCANFD_SEQ_GET_BITTIMING_RANGES,    /* use options above instead */
    PCANFD_SEQ_GET_OPTION,
    PCANFD_SEQ_SET_OPTION,
    PCANFD_SEQ_RESET,
};

#define PCANFD_IOCTL_SET_INIT           _IOW(PCAN_MAGIC_NUMBER, PCANFD_SEQ_SET_INIT, pcanfd_ioctl_init_t)
#define PCANFD_IOCTL_GET_INIT           _IOR(PCAN_MAGIC_NUMBER, PCANFD_SEQ_GET_INIT, pcanfd_ioctl_init_t)
#define PCANFD_IOCTL_GET_STATE          _IOR(PCAN_MAGIC_NUMBER, PCANFD_SEQ_GET_STATE, pcanfd_ioctl_state_t)
#define PCANFD_IOCTL_SEND_MSG           _IOW(PCAN_MAGIC_NUMBER, PCANFD_SEQ_SEND_MSG, pcanfd_ioctl_msg_t)
#define PCANFD_IOCTL_RECV_MSG           _IOR(PCAN_MAGIC_NUMBER, PCANFD_SEQ_RECV_MSG, pcanfd_ioctl_msg_t)
#define PCANFD_IOCTL_SEND_MSGS          _IOWR(PCAN_MAGIC_NUMBER, PCANFD_SEQ_SEND_MSGS, pcanfd_ioctl_msgs_t)
#define PCANFD_IOCTL_RECV_MSGS          _IOWR(PCAN_MAGIC_NUMBER, PCANFD_SEQ_RECV_MSGS, pcanfd_ioctl_msgs_t)
#define PCANFD_IOCTL_GET_OPTION         _IOWR(PCAN_MAGIC_NUMBER, PCANFD_SEQ_GET_OPTION, pcanfd_ioctl_option_t)
#define PCANFD_IOCTL_SET_OPTION         _IOW(PCAN_MAGIC_NUMBER, PCANFD_SEQ_SET_OPTION, pcanfd_ioctl_option_t)
#define PCANFD_IOCTL_RESET              _IOW(PCAN_MAGIC_NUMBER, PCANFD_SEQ_RESET, unsigned long)

/******************************************************************************
 * Definitions for upper layer.
 *****************************************************************************/

struct file;
struct usb_forwarder;

typedef struct ioctl_handler
{
    const char *name;
    int (*func)(struct file *, struct usb_forwarder *, void __user *);
} ioctl_handler_t;

extern const ioctl_handler_t G_IOCTL_HANDLERS[];
extern const ioctl_handler_t G_FD_IOCTL_HANDLERS[];

#ifdef __cplusplus
}
#endif

#endif /* #ifndef __CHARDEV_IOCTL_H__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-12-12, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 */

