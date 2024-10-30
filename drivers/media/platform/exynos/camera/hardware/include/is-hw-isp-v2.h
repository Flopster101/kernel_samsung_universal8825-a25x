/* SPDX-License-Identifier: GPL-2.0
 *
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_ISP_H
#define IS_HW_ISP_H

#include "is-hw.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "api/is-hw-api-type.h"
#include "api/is-hw-api-itp.h"
#include "api/is-hw-api-dns.h"
#include "api/is-hw-api-mcfp-v10_20.h"

enum is_hw_isp_tnr_mode {
	TNR_MODE_PREPARE,
	TNR_MODE_FIRST,
	TNR_MODE_NORMAL,
	TNR_MODE_FUSION,
};

enum is_hw_isp_irq_src {
	ISP_DNS_INTR0,
	ISP_DNS_INTR1,
	ISP_MCFP_INTR0,
	ISP_MCFP_INTR1,
	ISP_INTR_MAX,
};

enum is_hw_isp_reg_idx {
	ISP_REG_ITP,
	ISP_REG_MCFP0,
	ISP_REG_DNS,
	ISP_REG_MCFP1,
	ISP_REG_MAX,
};

enum is_hw_isp_debug {
	ISP_DEBUG_MCFP0_DUMP,
	ISP_DEBUG_MCFP1_DUMP,
	ISP_DEBUG_DNS_DUMP,
	ISP_DEBUG_ITP_DUMP,
	ISP_DEBUG_DTP,
	ISP_DEBUG_TNR_BYPASS,
	ISP_DEBUG_SET_REG,
	ISP_DEBUG_DUMP_FORMAT,	/* 0: sw dump  1: dump for simulation */
	ISP_DEBUG_DRC_BYPASS,
};

struct is_hw_isp_iq {
	struct cr_set	*regs;
	u32		size;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

struct is_hw_isp_count {
	atomic_t		fs;
	atomic_t		fe;
	atomic_t		max_cnt;
};

struct is_hw_isp {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct lib_interface_func	*lib_func;
	struct isp_param_set		param_set[IS_STREAM_COUNT];
	u32				input_dva_tnr_ref[IS_STREAM_COUNT][MAX_STRIPE_REGION_NUM];
	struct is_common_dma		mcfp_rdma[MCFP_RDMA_MAX];
	struct is_common_dma		mcfp_wdma[MCFP_WDMA_MAX];
	struct is_common_dma		dns_dma[DNS_DMA_MAX];
	struct is_hw_isp_count		count;
	u32				irq_state[ISP_INTR_MAX];
	unsigned long			state;
	atomic_t			strip_index;

	bool				hw_fro_en;

	struct is_isp_config		iq_config[IS_STREAM_COUNT];
	struct is_hw_isp_iq		iq_set[IS_STREAM_COUNT][ISP_REG_MAX];
	struct is_hw_isp_iq		cur_hw_iq_set[COREX_MAX][ISP_REG_MAX];
};

int is_hw_isp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
#endif
