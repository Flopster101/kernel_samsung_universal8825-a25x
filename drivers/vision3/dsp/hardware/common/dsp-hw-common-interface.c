// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-common-interface.h"

int dsp_hw_common_interface_set_ops(struct dsp_interface *itf, const void *ops)
{
	dsp_enter();
	itf->ops = ops;
	dsp_leave();
	return 0;
}
