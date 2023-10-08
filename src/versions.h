/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Version codes.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#ifndef __VERSIONS_H__
#define __VERSIONS_H__

#include "__ver__.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRIVER_VERSION
#define DRIVER_VERSION          "0.5.0"
#endif

#ifndef APP_VERSION
#define APP_VERSION             "0.1.0"
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
 */

