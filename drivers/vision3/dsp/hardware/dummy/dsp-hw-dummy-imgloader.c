// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-dummy-imgloader.h"

int dsp_hw_dummy_imgloader_boot(struct dsp_imgloader *imgloader)
{
	dsp_check();
	return 0;
}

int dsp_hw_dummy_imgloader_shutdown(struct dsp_imgloader *imgloader)
{
	dsp_check();
	return 0;
}

int dsp_hw_dummy_imgloader_probe(struct dsp_imgloader *imgloader, void *sys)
{
	dsp_check();
	return 0;
}

void dsp_hw_dummy_imgloader_remove(struct dsp_imgloader *imgloader)
{
	dsp_check();
}

static const struct dsp_imgloader_ops dummy_imgloader_ops = {
	.boot		= dsp_hw_dummy_imgloader_boot,
	.shutdown	= dsp_hw_dummy_imgloader_shutdown,

	.probe		= dsp_hw_dummy_imgloader_probe,
	.remove		= dsp_hw_dummy_imgloader_remove,
};

int dsp_hw_dummy_imgloader_register_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(dev_id, DSP_HW_OPS_IMGLOADER,
			&dummy_imgloader_ops,
			sizeof(dummy_imgloader_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	dsp_err("Failed to register dummy imgloader ops(%d)\n", ret);
	return ret;
}
