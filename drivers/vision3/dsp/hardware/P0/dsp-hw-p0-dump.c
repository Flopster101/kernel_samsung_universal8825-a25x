// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-dump.h"
#include "dsp-hw-common-init.h"
#include "dsp-kernel.h"
#include "dsp-hw-p0-system.h"
#include "dsp-hw-p0-ctrl.h"
#include "dsp-hw-p0-mailbox.h"
#include "dsp-hw-common-dump.h"
#include "dsp-hw-p0-dump.h"

static const char *const p0_dump_value_name[DSP_P0_DUMP_VALUE_END] = {
	"DSP_P0_DUMP_VALUE_MBOX_ERROR",
	"DSP_P0_DUMP_VALUE_MBOX_DEBUG",
	"DSP_P0_DUMP_VALUE_DL_LOG_ON",
	"DSP_P0_DUMP_VALUE_DL_LOG_RESERVED",
	"DSP_P0_DUMP_VALUE_DNCC_NS",
	"DSP_P0_DUMP_VALUE_CPU_SS",
	"DSP_P0_DUMP_VALUE_DSP0_CTRL",
	"DSP_P0_DUMP_VALUE_DSP1_CTRL",
	"DSP_P0_DUMP_VALUE_DHCP",
	"DSP_P0_DUMP_VALUE_USERDEFINED",
	"DSP_P0_DUMP_VALUE_FW_INFO",
	"DSP_P0_DUMP_VALUE_PC",
	"DSP_P0_DUMP_VALUE_TASK_COUNT",
};

static void dsp_hw_p0_dump_print_status_user(struct dsp_hw_dump *dump,
		struct seq_file *file)
{
	unsigned int idx;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
	seq_printf(file, "current dump_value : 0x%x / default : 0x%x\n",
			dump_value, dump->default_value);

	for (idx = 0; idx < DSP_P0_DUMP_VALUE_END; idx++) {
		seq_printf(file, "[%2d][%s] - %s\n", idx,
				dump_value & BIT(idx) ? "*" : " ",
				p0_dump_value_name[idx]);
	}

	dsp_leave();
}

static void dsp_hw_p0_dump_ctrl(struct dsp_hw_dump *dump)
{
	int idx;
	int start, end;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
	dsp_dump_print_value();
	if (dump_value & BIT(DSP_P0_DUMP_VALUE_DNCC_NS)) {
		start = DSP_P0_DNCC_NS_VERSION;
		end = DSP_P0_DNCC_NS_MBOX_FR_CC_TO_GNPU1_3;
		dsp_notice("DNC_CTRL_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_CPU_SS)) {
		start = DSP_P0_DNC_CPU_REMAPS0_NS;
		end = DSP_P0_DNC_CPU_ALIVE_CTRL;
		dsp_notice("CPU_SS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_DSP0_CTRL)) {
		start = DSP_P0_DSP0_CTRL_BUSACTREQ;
		end = DSP_P0_DSP0_CTRL_IVP1_MSG_TH1_$;
		dsp_notice("DSP0_CTRL register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_DSP1_CTRL)) {
		start = DSP_P0_DSP1_CTRL_BUSACTREQ;
		end = DSP_P0_DSP1_CTRL_IVP1_MSG_TH1_$;
		dsp_notice("DSP1_CTRL register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_reg_print(idx);
	}

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_DHCP))
		dsp_ctrl_dhcp_dump();

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_USERDEFINED))
		dsp_ctrl_userdefined_dump();

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_FW_INFO))
		dsp_ctrl_fw_info_dump();

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_PC))
		dsp_ctrl_pc_dump();

	dsp_leave();
}

static void dsp_hw_p0_dump_ctrl_user(struct dsp_hw_dump *dump,
		struct seq_file *file)
{
	int idx;
	int start, end;
	unsigned int dump_value = dump->dump_value;

	dsp_enter();
	if (dump_value & BIT(DSP_P0_DUMP_VALUE_DNCC_NS)) {
		start = DSP_P0_DNCC_NS_VERSION;
		end = DSP_P0_DNCC_NS_MBOX_FR_CC_TO_GNPU1_3;
		seq_printf(file, "DNC_CTRL_NS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_CPU_SS)) {
		start = DSP_P0_DNC_CPU_REMAPS0_NS;
		end = DSP_P0_DNC_CPU_ALIVE_CTRL;
		seq_printf(file, "CPU_SS register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_DSP0_CTRL)) {
		start = DSP_P0_DSP0_CTRL_BUSACTREQ;
		end = DSP_P0_DSP0_CTRL_IVP1_MSG_TH1_$;
		seq_printf(file, "DSP0_CTRL register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_DSP1_CTRL)) {
		start = DSP_P0_DSP1_CTRL_BUSACTREQ;
		end = DSP_P0_DSP1_CTRL_IVP1_MSG_TH1_$;
		seq_printf(file, "DSP1_CTRL register dump (count:%d)\n",
				end - start + 1);
		for (idx = start; idx <= end; ++idx)
			dsp_ctrl_user_reg_print(file, idx);
	}

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_DHCP))
		dsp_ctrl_user_dhcp_dump(file);

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_USERDEFINED))
		dsp_ctrl_user_userdefined_dump(file);

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_FW_INFO))
		dsp_ctrl_user_fw_info_dump(file);

	if (dump_value & BIT(DSP_P0_DUMP_VALUE_PC))
		dsp_ctrl_user_pc_dump(file);

	dsp_leave();
}

static void dsp_hw_p0_dump_mailbox_pool_error(struct dsp_hw_dump *dump,
		void *pool)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_P0_DUMP_VALUE_MBOX_ERROR))
		dsp_mailbox_dump_pool(pool);
	dsp_leave();
}

static void dsp_hw_p0_dump_mailbox_pool_debug(struct dsp_hw_dump *dump,
		void *pool)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_P0_DUMP_VALUE_MBOX_DEBUG))
		dsp_mailbox_dump_pool(pool);
	dsp_leave();
}

static void dsp_hw_p0_dump_task_count(struct dsp_hw_dump *dump)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_P0_DUMP_VALUE_TASK_COUNT))
		dsp_task_manager_dump_count();
	dsp_leave();
}

static void dsp_hw_p0_dump_kernel(struct dsp_hw_dump *dump)
{
	dsp_enter();
	if (dump->dump_value & BIT(DSP_P0_DUMP_VALUE_DL_LOG_ON))
		dsp_kernel_dump();
	dsp_leave();
}

static int dsp_hw_p0_dump_probe(struct dsp_hw_dump *dump, void *sys)
{
	int ret;

	dsp_enter();
	dump->dump_count = DSP_P0_DUMP_VALUE_END;
	dump->default_value = DSP_P0_DUMP_DEFAULT_VALUE;

	ret = dsp_hw_common_dump_probe(dump, sys);
	if (ret) {
		dsp_err("Failed to probe common dump(%d)\n", ret);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static const struct dsp_hw_dump_ops p0_dump_ops = {
	.set_value		= dsp_hw_common_dump_set_value,
	.print_value		= dsp_hw_common_dump_print_value,
	.print_status_user	= dsp_hw_p0_dump_print_status_user,
	.ctrl			= dsp_hw_p0_dump_ctrl,
	.ctrl_user		= dsp_hw_p0_dump_ctrl_user,

	.mailbox_pool_error	= dsp_hw_p0_dump_mailbox_pool_error,
	.mailbox_pool_debug	= dsp_hw_p0_dump_mailbox_pool_debug,
	.task_count		= dsp_hw_p0_dump_task_count,
	.kernel			= dsp_hw_p0_dump_kernel,

	.probe			= dsp_hw_p0_dump_probe,
	.remove			= dsp_hw_common_dump_remove,
};

int dsp_hw_p0_dump_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_DUMP,
			&p0_dump_ops, sizeof(p0_dump_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
