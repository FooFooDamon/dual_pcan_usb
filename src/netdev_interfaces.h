/*
 * Netdev interfaces of PCAN-USB.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef __NETDEV_INTERFACES_H__
#define __NETDEV_INTERFACES_H__

struct can_clock;
struct can_bittiming_const;
struct net_device;
enum can_mode;

const struct can_clock* get_fixed_can_clock(void);

const struct can_bittiming_const* get_can_bittiming_const(void);

int pcan_net_set_can_bittiming(struct net_device *netdev);

void pcan_net_wake_up(struct net_device *netdev);

int pcan_net_set_can_mode(struct net_device *netdev, enum can_mode mode);

void pcan_net_set_ops(struct net_device *netdev);

#endif /* #ifndef __NETDEV_INTERFACES_H__ */

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
 */

