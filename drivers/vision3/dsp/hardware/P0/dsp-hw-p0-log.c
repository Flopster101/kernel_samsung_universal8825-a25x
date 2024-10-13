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
#include "dsp-hw-p0-memory.h"
#include "dsp-hw-p0-system.h"
#include "dsp-hw-common-log.h"
#include "dsp-hw-p0-log.h"

static int dsp_hw_p0_log_start(struct dsp_hw_log *log)
{
	struct dsp_system *sys;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	sys = log->sys;

	pmem = &sys->memory.priv_mem[DSP_P0_PRIV_MEM_DHCP];
	log->queue = pmem->kvaddr + DSP_P0_DHCP_IDX(DSP_P0_DHCP_LOG_QUEUE);

	dsp_hw_common_log_start(log);
	dsp_leave();
	return 0;
}

static const struct dsp_hw_log_ops p0_hw_log_ops = {
	.flush		= dsp_hw_common_log_flush,
	.start		= dsp_hw_p0_log_start,
	.stop		= dsp_hw_common_log_stop,

	.open		= dsp_hw_common_log_open,
	.close		= dsp_hw_common_log_close,
	.probe		= dsp_hw_common_log_probe,
	.remove		= dsp_hw_common_log_remove,
};

int dsp_hw_p0_log_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_HW_LOG,
			&p0_hw_log_ops,
			sizeof(p0_hw_log_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
