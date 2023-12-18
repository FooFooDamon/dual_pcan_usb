/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Common stuff used by both driver and app.
 *
 * Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
 * All rights reserved.
*/

#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef fallthrough
#if __has_attribute(__fallthrough__)
#define fallthrough             __attribute__((__fallthrough__))
#else
#define fallthrough             do {} while (0)  /* fallthrough */
#endif
#endif

#define DEV_NAME                "pcanusb"
#define DEV_MINOR_BASE          32
#define DEV_TYPE                "usb"

#define VENDOR_ID               0x0c72
#define PRODUCT_ID              0x000c
#define PRODUCT_TYPE            11

#define DEFAULT_BIT_RATE        1000000

#ifdef __cplusplus
}
#endif

#endif /* #ifndef __COMMON_H__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-07-19, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 *
 * >>> 2023-09-03, Man Hung-Coeng <udc577@126.com>:
 *  01. Add DEV_MINOR_BASE, VENDOR_ID, PRODUCT_ID.
 *
 * >>> 2023-10-05, Man Hung-Coeng <udc577@126.com>:
 *  01. Change license to GPL-2.0.
 *
 * >>> 2023-10-08, Man Hung-Coeng <udc577@126.com>:
 *  01. Add definition of fallthrough.
 *
 * >>> 2023-11-10, Man Hung-Coeng <udc577@126.com>:
 *  01. Change the value of macro DEV_NAME from "dual_pcan_usb" to "pcanusb".
 *  02. Add a new macro DEFAULT_BIT_RATE.
 *
 * >>> 2023-12-18, Man Hung-Coeng <udc577@126.com>:
 *  01. Add DEV_TYPE and PRODUCT_TYPE macros.
 */

