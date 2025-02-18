/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/pm_qos.h>

struct ucc_req {
	char			name[20];
	int			active;
	int			value;
	struct list_head	list;
};

#define RELEASE -1

enum ufc_ctrl_type {
	PM_QOS_MIN_LIMIT = 0,
	PM_QOS_MIN_LIMIT_WO_BOOST,
	PM_QOS_MAX_LIMIT,
	PM_QOS_OVER_LIMIT,
	PM_QOS_LITTLE_MAX_LIMIT,
	PM_QOS_LITTLE_MIN_LIMIT,
	TYPE_END,
};

enum ufc_user_type {
	USERSPACE,
	UFC_INPUT,
	FINGER,
	ARGOS,
	USER_END,
};

#if IS_ENABLED(CONFIG_EXYNOS_UFCC)
extern void ucc_add_request(struct ucc_req *req, int value);
extern void ucc_update_request(struct ucc_req *req, int value);
extern void ucc_remove_request(struct ucc_req *req);

extern int ufc_update_request(enum ufc_user_type user, enum ufc_ctrl_type type,
		s32 new_freq);

#else
static inline void ucc_add_request(struct ucc_req *req, int value) { }
static inline void ucc_update_request(struct ucc_req *req, int value) { }
static inline void ucc_remove_request(struct ucc_req *req) { }

static inline int ufc_update_request(enum ufc_user_type user, enum ufc_ctrl_type type,
		s32 new_freq) { return 0; }
#endif

struct ufc_table_info {
	int			ctrl_type;
	int			mode;
	u32			**ufc_table;
	struct list_head	list;

	struct kobject		kobj;
};


struct ufc_domain {
	struct cpumask		cpus;

	unsigned int		table_col_idx;
	unsigned int		min_freq;
	unsigned int		max_freq;
	unsigned int		hold_freq;
	unsigned int		little_min_timeout;

	struct freq_qos_request	user_min_qos_req;
	struct freq_qos_request	user_max_qos_req;
	struct freq_qos_request	user_min_qos_wo_boost_req;
	struct freq_qos_request	user_lit_max_qos_req;
	struct freq_qos_request	user_lit_min_qos_req;
	struct freq_qos_request	hold_lit_max_qos_req;

	int			allow_disable_cpus;

	struct delayed_work	work;
	struct list_head	list;
};
