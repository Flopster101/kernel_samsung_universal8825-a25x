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
#include "dsp-hw-common-imgloader.h"
#include "dsp-hw-p0-imgloader.h"

static const struct dsp_imgloader_ops p0_imgloader_ops = {
	.boot		= dsp_hw_common_imgloader_boot,
	.shutdown	= dsp_hw_common_imgloader_shutdown,

	.probe		= dsp_hw_common_imgloader_probe,
	.remove		= dsp_hw_common_imgloader_remove,
};

int dsp_hw_p0_imgloader_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_IMGLOADER,
			&p0_imgloader_ops,
			sizeof(p0_imgloader_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
