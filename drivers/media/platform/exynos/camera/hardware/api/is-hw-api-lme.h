// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef IS_HW_API_LME_H
#define IS_HW_API_LME_H

struct is_reg * lme_hw_get_reg_struct(void);
unsigned int lme_hw_get_reg_cnt(void);

#endif /* IS_HW_API_LME_H */
