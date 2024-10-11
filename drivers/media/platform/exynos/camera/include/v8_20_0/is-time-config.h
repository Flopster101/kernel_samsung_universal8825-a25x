// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 * Pablo v8.20 specific hw configuration
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_TIME_CONFIG_H
#define IS_TIME_CONFIG_H

struct is_monitor {
	unsigned long long	time;
	bool			check;
};

enum is_time_shot {
	TMS_Q,
	TMS_SHOT1,
	TMS_SHOT2,
	TMS_SDONE,
	TMS_DQ,
	TMS_END,
};

#endif /* IS_TIME_CONFIG_H */
