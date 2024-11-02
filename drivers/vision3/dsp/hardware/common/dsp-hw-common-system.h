/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_SYSTEM_H__
#define __HW_COMMON_DSP_HW_COMMON_SYSTEM_H__

#include "dsp-system.h"

int dsp_hw_common_system_request_control(struct dsp_system *sys,
		unsigned int id, union dsp_control *cmd);
int dsp_hw_common_system_execute_task(struct dsp_system *sys,
		struct dsp_task *task);
void dsp_hw_common_system_iovmm_fault_dump(struct dsp_system *sys);
int dsp_hw_common_system_power_active(struct dsp_system *sys);
int dsp_hw_common_system_set_boot_qos(struct dsp_system *sys, int val);
int dsp_hw_common_system_runtime_resume(struct dsp_system *sys);
int dsp_hw_common_system_runtime_suspend(struct dsp_system *sys);
int dsp_hw_common_system_resume(struct dsp_system *sys);
int dsp_hw_common_system_suspend(struct dsp_system *sys);
int dsp_hw_common_system_npu_start(struct dsp_system *sys, bool boot,
		dma_addr_t fw_iova);

int dsp_hw_common_system_start(struct dsp_system *sys);
int dsp_hw_common_system_stop(struct dsp_system *sys);
int dsp_hw_common_system_open(struct dsp_system *sys);
int dsp_hw_common_system_close(struct dsp_system *sys);
int dsp_hw_common_system_probe(struct dsp_system *sys, void *dspdev,
		timeout_handler_t handler);
void dsp_hw_common_system_remove(struct dsp_system *sys);

int dsp_hw_common_system_set_ops(struct dsp_system *sys, const void *ops);

#endif
