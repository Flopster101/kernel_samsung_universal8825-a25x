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
#include "dsp-hw-common-sysevent.h"
#include "dsp-hw-p0-sysevent.h"

static const struct dsp_sysevent_ops p0_sysevent_ops = {
	.get		= dsp_hw_common_sysevent_get,
	.put		= dsp_hw_common_sysevent_put,

	.probe		= dsp_hw_common_sysevent_probe,
	.remove		= dsp_hw_common_sysevent_remove,
};

int dsp_hw_p0_sysevent_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_SYSEVENT,
			&p0_sysevent_ops,
			sizeof(p0_sysevent_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
