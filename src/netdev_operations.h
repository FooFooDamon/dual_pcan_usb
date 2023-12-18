/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Netdev operations of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#ifndef __NETDEV_OPERATIONS_H__
#define __NETDEV_OPERATIONS_H__

struct can_clock;
struct can_bittiming_const;
struct net_device;
enum can_mode;

const struct can_clock* get_fixed_can_clock(void);

const struct can_bittiming_const* get_can_bittiming_const(void);

int pcan_net_set_can_bittiming(struct net_device *netdev);

void pcan_net_wake_up(struct net_device *netdev);

int pcan_net_dev_open(struct net_device *netdev);

/* NOTE: This function might sleep, DO NOT use it in an interrupt context. */
void pcan_net_dev_close(struct net_device *netdev);

int pcan_net_set_can_mode(struct net_device *netdev, enum can_mode mode);

void pcan_net_set_ops(struct net_device *netdev);

#endif /* #ifndef __NETDEV_OPERATIONS_H__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-09-16, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-09-25, Man Hung-Coeng <udc577@126.com>:
 *  01. Rename netdev_wake_up() to pcan_net_wake_up(),
 *      netdev_set_can_mode() to pcan_net_set_can_mode().
 *  02. Add pcan_net_set_ops().
 *
 * >>> 2023-10-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Add pcan_net_set_can_bittiming().
 *  02. Change license to GPL-2.0.
 *
 * >>> 2023-10-08, Man Hung-Coeng <udc577@126.com>:
 *  01. Add pcan_net_dev_open().
 *
 * >>> 2023-11-30, Man Hung-Coeng <udc577@126.com>:
 *  01. Add pcan_net_dev_close().
 *
 * >>> 2023-12-18, Man Hung-Coeng <udc577@126.com>:
 *  01. Rename this file from netdev_interfaces.h to netdev_operations.h.
 */

