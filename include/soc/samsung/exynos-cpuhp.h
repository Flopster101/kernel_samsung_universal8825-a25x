/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU Hotplug CONTROL support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_CPU_HP_H
#define __EXYNOS_CPU_HP_H __FILE__

#define CPUHP_REQ_NAME_LEN	16

struct cpuhp_request {
	struct list_head	list;
	bool			active;
	struct cpumask		mask;

	char			name[CPUHP_REQ_NAME_LEN];	/* for debugging */
};

#if IS_ENABLED(CONFIG_EXYNOS_CPUHP)
extern int exynos_cpuhp_add_request(struct cpuhp_request *req);
extern int exynos_cpuhp_remove_request(struct cpuhp_request *req);
extern int exynos_cpuhp_update_request(struct cpuhp_request *req, const struct cpumask *mask);
#else
static inline int exynos_cpuhp_add_request(struct cpuhp_request *req)
{ return -1; }
static inline int exynos_cpuhp_remove_request(struct cpuhp_request *req)
{ return -1; }
static inline int exynos_cpuhp_update_request(struct cpuhp_request *req, const struct cpumask *mask)
{ return -1; }
#endif

#endif /* __EXYNOS_CPU_HP_H */
