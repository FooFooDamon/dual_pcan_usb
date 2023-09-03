/*
 * Common stuff used by both driver and app.
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

#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
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
 */

