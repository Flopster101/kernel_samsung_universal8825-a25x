// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-common-system.h"
#include "dsp-hw-common-pm.h"
#include "dsp-hw-common-clk.h"
#include "dsp-hw-common-bus.h"
#include "dsp-hw-common-llc.h"
#include "dsp-hw-common-memory.h"
#include "dsp-hw-common-interface.h"
#include "dsp-hw-common-ctrl.h"
#include "dsp-hw-common-mailbox.h"
#include "dsp-hw-common-governor.h"
#include "dsp-hw-common-imgloader.h"
#include "dsp-hw-common-sysevent.h"
#include "dsp-hw-common-memlogger.h"
#include "dsp-hw-common-log.h"
#include "dsp-hw-common-debug.h"
#include "dsp-hw-common-dump.h"
#include "dsp-hw-common-init.h"

static const char *const ops_name_list[DSP_HW_OPS_COUNT] = {
	"HW_OPS_SYSTEM",
	"HW_OPS_PM",
	"HW_OPS_CLK",
	"HW_OPS_BUS",
	"HW_OPS_LLC",
	"HW_OPS_MEMORY",
	"HW_OPS_INTERFACE",
	"HW_OPS_CTRL",
	"HW_OPS_MBOX",
	"HW_OPS_GOVERNOR",
	"HW_OPS_IMGLOADER",
	"HW_OPS_SYSEVENT",
	"HW_OPS_MEMLOGGER",
	"HW_OPS_HW_LOG",
	"HW_OPS_HW_DEBUG",
	"HW_OPS_DUMP",
};

static struct dsp_hw_ops_list ops_list[DSP_DEVICE_ID_END];

static int __dsp_hw_common_check_dev_id(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	if (dev_id >= ARRAY_SIZE(ops_list)) {
		ret = -EINVAL;
		dsp_err("dev_id(%u/%zu) is invalid\n",
				dev_id, ARRAY_SIZE(ops_list));
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_common_check_ops_id(unsigned int ops_id)
{
	int ret;

	dsp_enter();
	if (ops_id >= ARRAY_SIZE(ops_list[0].ops)) {
		ret = -EINVAL;
		dsp_err("ops_id(%u/%zu) is invalid\n",
				ops_id, ARRAY_SIZE(ops_list[0].ops));
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_hw_common_dump_ops_list(unsigned int dev_id)
{
	int ret;
	size_t idx;

	dsp_enter();
	ret = __dsp_hw_common_check_dev_id(dev_id);
	if (ret)
		return;

	for (idx = 0; idx < ARRAY_SIZE(ops_list[dev_id].ops); ++idx) {
		ret = __dsp_hw_common_check_ops_id(idx);
		if (ret)
			continue;

		if (ops_list[dev_id].ops[idx])
			dsp_info("[%u] ops[%zu/%s] registered\n",
					dev_id, idx, ops_name_list[idx]);
		else
			dsp_warn("[%u] ops[%zu/%s] none\n",
					dev_id, idx, ops_name_list[idx]);
	}
	dsp_leave();
}

void dsp_hw_common_dump_ops_list_all(void)
{
	size_t idx;

	dsp_enter();
	for (idx = 0; idx < ARRAY_SIZE(ops_list); ++idx)
		dsp_hw_common_dump_ops_list(idx);
	dsp_leave();
}

static int __dsp_hw_common_check_ops(unsigned int dev_id)
{
	int ret;
	size_t idx;

	dsp_enter();
	ret = __dsp_hw_common_check_dev_id(dev_id);
	if (ret)
		goto p_err;

	ret = 0;
	for (idx = 0; idx < ARRAY_SIZE(ops_list[dev_id].ops); ++idx) {
		if (!ops_list[dev_id].ops[idx]) {
			ret = -EINVAL;
			dsp_err("[%u] ops[%zu/%s] is not registered\n",
					dev_id, idx, ops_name_list[idx]);
		}
	}
	if (ret) {
		dsp_err("[%u] ops_list is not completed\n", dev_id);
		dsp_hw_common_dump_ops_list(dev_id);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_set_ops(unsigned int dev_id, void *data)
{
	int ret;
	struct dsp_system *sys = data;

	dsp_enter();
	ret = __dsp_hw_common_check_ops(dev_id);
	if (ret)
		goto p_err;

	dsp_hw_common_dump_ops_list(dev_id);

	ret = dsp_hw_common_system_set_ops(sys,
			ops_list[dev_id].ops[DSP_HW_OPS_SYSTEM]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_pm_set_ops(&sys->pm,
			ops_list[dev_id].ops[DSP_HW_OPS_PM]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_clk_set_ops(&sys->clk,
			ops_list[dev_id].ops[DSP_HW_OPS_CLK]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_bus_set_ops(&sys->bus,
			ops_list[dev_id].ops[DSP_HW_OPS_BUS]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_llc_set_ops(&sys->llc,
			ops_list[dev_id].ops[DSP_HW_OPS_LLC]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_memory_set_ops(&sys->memory,
			ops_list[dev_id].ops[DSP_HW_OPS_MEMORY]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_interface_set_ops(&sys->interface,
			ops_list[dev_id].ops[DSP_HW_OPS_INTERFACE]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_ctrl_set_ops(&sys->ctrl,
			ops_list[dev_id].ops[DSP_HW_OPS_CTRL]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_mailbox_set_ops(&sys->mailbox,
			ops_list[dev_id].ops[DSP_HW_OPS_MAILBOX]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_governor_set_ops(&sys->governor,
			ops_list[dev_id].ops[DSP_HW_OPS_GOVERNOR]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_imgloader_set_ops(&sys->imgloader,
			ops_list[dev_id].ops[DSP_HW_OPS_IMGLOADER]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_sysevent_set_ops(&sys->sysevent,
			ops_list[dev_id].ops[DSP_HW_OPS_SYSEVENT]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_memlogger_set_ops(&sys->memlogger,
			ops_list[dev_id].ops[DSP_HW_OPS_MEMLOGGER]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_log_set_ops(&sys->log,
			ops_list[dev_id].ops[DSP_HW_OPS_HW_LOG]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_debug_set_ops(&sys->debug,
			ops_list[dev_id].ops[DSP_HW_OPS_HW_DEBUG]);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_dump_set_ops(&sys->dump,
			ops_list[dev_id].ops[DSP_HW_OPS_DUMP]);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_register_ops(unsigned int dev_id, unsigned int ops_id,
		const void *ops, size_t ops_count)
{
	int ret;
	size_t idx;
	void **check;

	dsp_enter();
	ret = __dsp_hw_common_check_dev_id(dev_id);
	if (ret)
		goto p_err;

	ret = __dsp_hw_common_check_ops_id(ops_id);
	if (ret)
		goto p_err;

	if (ops_list[dev_id].ops[ops_id]) {
		ret = -EINVAL;
		dsp_warn("[%u] ops[%u/%s] is already registered\n",
				dev_id, ops_id, ops_name_list[ops_id]);
		dsp_hw_common_dump_ops_list(dev_id);
		goto p_err;
	}

	ret = 0;
	check = (void **)ops;
	for (idx = 0; idx < ops_count; ++idx) {
		if (!check[idx]) {
			ret = -EFAULT;
			dsp_err("[%u] ops[%u/%s] has NULL ops(%zu)\n",
					dev_id, ops_id, ops_name_list[ops_id],
					idx);
		}
	}
	if (ret) {
		dsp_err("There should not be NULL ops\n");
		goto p_err;
	}

	ops_list[dev_id].ops[ops_id] = ops;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_init(void)
{
	int ret, idx;

	dsp_enter();
	if (ARRAY_SIZE(ops_list) != DSP_DEVICE_ID_END) {
		ret = -EINVAL;
		dsp_err("ops_list(%zu/%u) is unstable\n",
				ARRAY_SIZE(ops_list), DSP_DEVICE_ID_END);
		goto p_err;
	}

	if (ARRAY_SIZE(ops_list[0].ops) != DSP_HW_OPS_COUNT) {
		ret = -EINVAL;
		dsp_err("ops_count(%zu/%u) is unstable\n",
				ARRAY_SIZE(ops_list[0].ops), DSP_HW_OPS_COUNT);
		goto p_err;
	}

	if (ARRAY_SIZE(ops_name_list) != ARRAY_SIZE(ops_list[0].ops)) {
		ret = -EINVAL;
		dsp_err("ops_name_list(%zu/%zu) is invalid\n",
				ARRAY_SIZE(ops_name_list),
				ARRAY_SIZE(ops_list[0].ops));
		goto p_err;
	}

	ret = 0;
	for (idx = 0; idx < ARRAY_SIZE(ops_name_list); ++idx) {
		if (!ops_name_list[idx]) {
			ret = -EINVAL;
			dsp_err("ops_name_list(%d) is NULL\n", idx);
		}
	}
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
