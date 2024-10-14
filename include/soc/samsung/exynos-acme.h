/*
 * Copyright (c) 2021 Park Choonghoon,
 * Samsung Electronics Co., Ltd <choong.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARM_EXYNOS_ACME_H
#define __ARM_EXYNOS_ACME_H __FILE__

#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME)
extern int exynos_cpufreq_register_notifier(struct notifier_block *nb, unsigned int list);
extern int exynos_cpufreq_unregister_notifier(struct notifier_block *nb, unsigned int list);
#else
static inline int exynos_cpufreq_register_notifier(struct notifier_block *nb, unsigned int list)
{
	return cpufreq_register_notifier(nb, list);
}
static inline int exynos_cpufreq_unregister_notifier(struct notifier_block *nb, unsigned int list)
{
	return cpufreq_unregister_notifier(nb, list);
}
#endif

#endif /* __ARM_EXYNOS_ACME_H */
