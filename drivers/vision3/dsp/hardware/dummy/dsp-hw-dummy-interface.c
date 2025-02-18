// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-dummy-interface.h"

int dsp_hw_dummy_interface_start(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_interface_stop(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_leave();
	return 0;
}
