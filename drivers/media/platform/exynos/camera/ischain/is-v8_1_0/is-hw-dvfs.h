// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_DVFS_H
#define IS_HW_DVFS_H

#include "is-dvfs-config.h"
/* Tick count to get some time margin for DVFS scenario transition while streaming. */
#define IS_DVFS_CAPTURE_TICK (KEEP_FRAME_TICK_DEFAULT + 3)
#define IS_DVFS_DUAL_CAPTURE_TICK (2 * IS_DVFS_CAPTURE_TICK)

/* for assign staic / dynamic scenario check logic data */
int is_hw_dvfs_init(void *dvfs_data);
#endif /* IS_HW_DVFS_H */
