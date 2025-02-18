/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_P0_DSP_HW_P0_IMGLOADER_H__
#define __HW_P0_DSP_HW_P0_IMGLOADER_H__

#include "dsp-imgloader.h"

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
int dsp_hw_p0_imgloader_register_ops(void);
#else
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-dummy-imgloader.h"

static inline int dsp_hw_p0_imgloader_register_ops(void)
{
	dsp_check();
	return dsp_hw_dummy_imgloader_register_ops(DSP_DEVICE_ID_P0);
}
#endif

#endif
