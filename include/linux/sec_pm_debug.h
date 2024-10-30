/*
 * sec_pm_debug.h - SEC Power Management Debug Support
 *
 *  Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __LINUX_SEC_PM_DEBUG_H
#define __LINUX_SEC_PM_DEBUG_H __FILE__

#define WS_LOG_PERIOD	10
#define MAX_WAKE_SOURCES_LEN 256

#if IS_ENABLED(CONFIG_REGULATOR_S2MPU13)
extern int main_pmic_init_debug_sysfs(struct device *sec_pm_dev);
#else
static inline int main_pmic_init_debug_sysfs(struct device *sec_pm_dev) { return 0; }
#endif

extern void pm_get_active_wakeup_sources(char *pending_wakeup_source, size_t max);
static void wake_sources_print_acquired_work(struct work_struct *work);

#endif /* __LINUX_SEC_PM_DEBUG_H */
