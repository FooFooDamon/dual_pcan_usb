/*
 * Entry point of driver.
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
 */

