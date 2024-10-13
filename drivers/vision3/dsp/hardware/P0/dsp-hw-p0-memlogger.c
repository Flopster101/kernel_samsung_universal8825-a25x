// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-p0-system.h"
#include "dsp-hw-common-memlogger.h"
#include "dsp-hw-p0-memlogger.h"

static const struct dsp_memlogger_ops p0_memlogger_ops = {
	.probe		= dsp_hw_common_memlogger_probe,
	.remove		= dsp_hw_common_memlogger_remove,
};

int dsp_hw_p0_memlogger_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_MEMLOGGER,
			&p0_memlogger_ops,
			sizeof(p0_memlogger_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
