/*
 * Kernel logging facilities.
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

#ifndef __KLOGGING_H__
#define __KLOGGING_H__

#include <linux/printk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __pr_xx_v(name, fmt, ...)	\
	pr_##name(DEV_NAME ": " __FILE__ ":%d %s(): " fmt, \
		__LINE__, __func__, ##__VA_ARGS__)

#define pr_emerg_v(fmt, ...)		__pr_xx_v(emerg, fmt, ##__VA_ARGS__)
#define pr_alert_v(fmt, ...)		__pr_xx_v(alert, fmt, ##__VA_ARGS__)
#define pr_crit_v(fmt, ...)			__pr_xx_v(crit, fmt, ##__VA_ARGS__)
#define pr_err_v(fmt, ...)			__pr_xx_v(err, fmt, ##__VA_ARGS__)
#define pr_warn_v(fmt, ...)			__pr_xx_v(warn, fmt, ##__VA_ARGS__)
#define pr_notice_v(fmt, ...)		__pr_xx_v(notice, fmt, ##__VA_ARGS__)
#define pr_info_v(fmt, ...)			__pr_xx_v(info, fmt, ##__VA_ARGS__)
#define pr_cont_v(fmt, ...)			__pr_xx_v(cont, fmt, ##__VA_ARGS__)
#define pr_devel_v(fmt, ...)		__pr_xx_v(devel, fmt, ##__VA_ARGS__)
#define pr_debug_v(fmt, ...)		__pr_xx_v(debug, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* #ifndef __KLOGGING_H__ */

/*
 * ================
 *   CHANGE LOG
 * ================
 *
 * >>> 2023-07-19, Man Hung-Coeng <udc577@126.com>:
 *  01. Create.
 */

