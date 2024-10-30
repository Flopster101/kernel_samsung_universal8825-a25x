// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-p0-system.h"
#include "dsp-hw-common-clk.h"
#include "dsp-hw-p0-clk.h"

static struct dsp_clk_format p0_clk_array[] = {
#ifndef ENABLE_DSP_VELOCE
	//TODO bring-up check
	//{ NULL, "GATE_IP_DNC_QCH" },
	//{ NULL, "GATE_IP_SDMA_QCH" },
	//{ NULL, "GATE_IP_DSP_QCH" },
#endif
};

static int dsp_hw_p0_clk_probe(struct dsp_clk *clk, void *sys)
{
	int ret;

	dsp_enter();
	clk->array = p0_clk_array;
	clk->array_size = ARRAY_SIZE(p0_clk_array);

	ret = dsp_hw_common_clk_probe(clk, sys);
	if (ret) {
		dsp_err("Failed to probe common clk(%d)\n", ret);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static const struct dsp_clk_ops p0_clk_ops = {
	.dump		= dsp_hw_common_clk_dump,
	.user_dump	= dsp_hw_common_clk_user_dump,
	.enable		= dsp_hw_common_clk_enable,
	.disable	= dsp_hw_common_clk_disable,

	.open		= dsp_hw_common_clk_open,
	.close		= dsp_hw_common_clk_close,
	.probe		= dsp_hw_p0_clk_probe,
	.remove		= dsp_hw_common_clk_remove,
};

int dsp_hw_p0_clk_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_CLK,
			&p0_clk_ops, sizeof(p0_clk_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
