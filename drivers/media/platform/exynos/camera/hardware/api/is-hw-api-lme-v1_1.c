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

#include "sfr/is-sfr-lme-v1_1.h"
#include "is-hw-api-lme.h"

struct is_reg * lme_hw_get_reg_struct(void)
{
	return (struct is_reg *)lme_regs;
}

unsigned int lme_hw_get_reg_cnt(void)
{
	return LME_REG_CNT;
}
