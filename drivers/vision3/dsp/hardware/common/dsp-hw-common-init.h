/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_INIT_H__
#define __HW_COMMON_DSP_HW_COMMON_INIT_H__

#include <linux/types.h>

enum dsp_hw_ops_id {
	DSP_HW_OPS_SYSTEM,
	DSP_HW_OPS_PM,
	DSP_HW_OPS_CLK,
	DSP_HW_OPS_BUS,
	DSP_HW_OPS_LLC,
	DSP_HW_OPS_MEMORY,
	DSP_HW_OPS_INTERFACE,
	DSP_HW_OPS_CTRL,
	DSP_HW_OPS_MAILBOX,
	DSP_HW_OPS_GOVERNOR,
	DSP_HW_OPS_IMGLOADER,
	DSP_HW_OPS_SYSEVENT,
	DSP_HW_OPS_MEMLOGGER,
	DSP_HW_OPS_HW_DEBUG,
	DSP_HW_OPS_HW_LOG,
	DSP_HW_OPS_DUMP,
	DSP_HW_OPS_COUNT,
};

struct dsp_hw_ops_list {
	const void *ops[DSP_HW_OPS_COUNT];
};

void dsp_hw_common_dump_ops_list(unsigned int dev_id);
void dsp_hw_common_dump_ops_list_all(void);

int dsp_hw_common_set_ops(unsigned int dev_id, void *data);
int dsp_hw_common_register_ops(unsigned int dev_id, unsigned int ops_id,
		const void *ops, size_t ops_size);
int dsp_hw_common_init(void);

#endif
