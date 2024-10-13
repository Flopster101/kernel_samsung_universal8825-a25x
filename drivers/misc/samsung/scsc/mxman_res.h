/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef _MAXWELL_RES_MANAGER_H
#define _MAXWELL_RES_MANAGER_H

#include "mxmgmt_transport.h"

#ifdef CONFIG_SOC_S5E8825
#include "mifpmuman.h"
#endif

struct fwhdr_if;

int mxman_res_mem_map(struct mxman *mxman, void **start_dram, size_t *size_dram);
int mxman_res_mem_unmap(struct mxman *mxman, void *start_dram);
int mxman_res_fw_init(struct mxman *mxman, struct fwhdr_if **fw_wlan, struct fwhdr_if **fw_wpan, void *start_dram,
		      size_t size_dram);
int mxman_res_mappings_allocator_init(struct mxman *mxman, void *start_dram);
int mxman_res_mappings_allocator_deinit(struct mxman *mxman);
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
int mxman_res_mappings_logger_init(struct mxman *mxman, void *start_dram);
int mxman_res_mappings_logger_deinit(struct mxman *mxman);
#endif
#ifdef CONFIG_SOC_S5E8825
int mxman_res_pmu_init(struct mxman *mxman, mifpmuisr_handler handler);
#else
int mxman_res_pmu_init(struct mxman *mxman);
#endif
int mxman_res_pmu_boot(struct mxman *mxman, enum scsc_subsystem sub);
int mxman_res_pmu_reset(struct mxman *mxman, enum scsc_subsystem sub);
int mxman_res_pmu_deinit(struct mxman *mxman);
int mxman_res_init_common(struct mxman *mxman);
int mxman_res_deinit_common(struct mxman *mxman);
int mxman_res_init_subsystem(struct mxman *mxman, enum scsc_subsystem sub, void *data, size_t data_sz,
			     mxmgmt_channel_handler handler);
int mxman_res_post_init_subsystem(struct mxman *mxman, enum scsc_subsystem sub);
int mxman_res_reset(struct mxman *mxman, bool reset);
int mxman_res_deinit_subsystem(struct mxman *mxman, enum scsc_subsystem sub);
#endif
