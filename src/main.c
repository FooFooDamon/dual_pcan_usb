// SPDX-License-Identifier: GPL-2.0

/*
 * Entry point of driver.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#include <linux/version.h> /* For LINUX_VERSION_CODE. */
#include <linux/module.h>

#include "versions.h"
#include "common.h"
#include "klogging.h"
#include "usb_driver.h"

static __init int pcan_init(void)
{
    int ret = usbdrv_register();

    if (0 == ret)
        pr_notice("Initialized %s-%s.%s for Linux-%#x.\n", DEV_NAME, DRIVER_VERSION, __VER__, LINUX_VERSION_CODE);

    return ret;
}

static __exit void pcan_exit(void)
{
    usbdrv_unregister();
    pr_notice("Destroyed %s-%s.%s.\n", DEV_NAME, DRIVER_VERSION, __VER__);
}

module_init(pcan_init);
module_exit(pcan_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Man Hung-Coeng <udc577@126.com>");

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-07-19, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-09-03, Man Hung-Coeng <udc577@126.com>:
 *  01. Implement registration and deregistration of USB driver.
 *
 * >>> 2023-10-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Change license to GPL-2.0.
 */

