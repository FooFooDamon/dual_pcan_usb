/*
 * Implementation of netdev interfaces of PCAN-USB.
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

#include "netdev_interfaces.h"

#include <linux/netdevice.h>
#include <linux/can/dev.h>

#define PCAN_USB_CRYSTAL_HZ             16000000

const struct can_clock* get_fixed_can_clock(void)
{
    static const struct can_clock S_CAN_CLOCK = {
        .freq = PCAN_USB_CRYSTAL_HZ / 2
    };

    return &S_CAN_CLOCK;
}

const struct can_bittiming_const* get_can_bittiming_const(void)
{
    static const struct can_bittiming_const S_CAN_BITTIMING_CONST = {
        .name = "pcan_usb"
        , .tseg1_min = 1
        , .tseg1_max = 16
        , .tseg2_min = 1
        , .tseg2_max = 8
        , .sjw_max = 4
        , .brp_min = 1
        , .brp_max = 64
        , .brp_inc = 1
    };

    return &S_CAN_BITTIMING_CONST;
}

void netdev_wake_up(struct net_device *netdev)
{
    struct can_priv *can = (struct can_priv *)netdev_priv(netdev);

    can->state = CAN_STATE_ERROR_ACTIVE;
    netif_wake_queue(netdev);
}

int netdev_set_can_mode(struct net_device *netdev, enum can_mode mode)
{
    int err = 0;

    switch (mode)
    {
    case CAN_MODE_START:
        netdev_wake_up(netdev); /* TODO: Need to set CAN bus on? */
        break;

    default:
        return -EOPNOTSUPP;
    }

    return err;
}

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-09-16, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 */

