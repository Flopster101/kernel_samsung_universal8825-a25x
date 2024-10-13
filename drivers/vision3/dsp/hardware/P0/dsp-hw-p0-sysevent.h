/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_P0_DSP_HW_P0_SYSEVENT_H__
#define __HW_P0_DSP_HW_P0_SYSEVENT_H__

#include "dsp-sysevent.h"

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
int dsp_hw_p0_sysevent_register_ops(void);
#else
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-dummy-sysevent.h"

static inline int dsp_hw_p0_sysevent_register_ops(void)
{
	dsp_check();
	return dsp_hw_dummy_sysevent_register_ops(DSP_DEVICE_ID_P0);
}
#endif

#endif
