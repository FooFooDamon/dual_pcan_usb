/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Sysfs mechanism.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#ifndef __CHARDEV_SYSFS_H__
#define __CHARDEV_SYSFS_H__

#ifdef __cplusplus
extern "C" {
#endif

struct class;
struct class_attribute;

const struct class_attribute* pcan_class_attributes(void);

struct attribute;

const struct attribute** pcan_device_attributes(void);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef __CHARDEV_SYSFS_H__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-12-18, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 */

