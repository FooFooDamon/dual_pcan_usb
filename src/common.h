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

#define DEV_NAME                "dual_pcan_usb"
#define DEV_MINOR_BASE          32
#define VENDOR_ID               0x0c72
#define PRODUCT_ID              0x000c

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
 */

