/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Version codes.
 *
 * Copyright (c) 2023-2024 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#ifndef __VERSIONS_H__
#define __VERSIONS_H__

#include "__ver__.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRV_VER_MAJOR
#define DRV_VER_MAJOR           0
#endif

#ifndef DRV_VER_MINOR
#define DRV_VER_MINOR           8
#endif

#ifndef DRV_VER_RELEASE
#define DRV_VER_RELEASE         1
#endif

#ifndef DRIVER_VERSION
#define DRIVER_VERSION          __stringify(DRV_VER_MAJOR) "." __stringify(DRV_VER_MINOR) "." __stringify(DRV_VER_RELEASE)
#endif

#ifndef APP_VERSION
#define APP_VERSION             "0.2.0"
#endif

#ifdef __cplusplus
}
#endif

#endif /* #ifndef __VERSIONS_H__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-07-19, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-10-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Reset DRIVER_VERSION and APP_VERSION to values
 *      which show the current progress of project.
 *  02. Change license to GPL-2.0.
 *
 * >>> 2023-10-08, Man Hung-Coeng <udc577@126.com>:
 *  01. Use the __VER__ macro from the 3rd-party header file.
 *
 * >>> 2023-12-12, Man Hung-Coeng <udc577@126.com>:
 *  01. Add DRV_VER_* macros for separate parts of DRIVER_VERSION,
 *      and update its value to 0.8.0.
 *
 * >>> 2023-12-28, Man Hung-Coeng <udc577@126.com>:
 *  01. Update APP_VERSION to 0.2.0.
 *
 * >>> 2024-06-23, Man Hung-Coeng <udc577@126.com>:
 *  01. Driver v0.8.1.
 */

