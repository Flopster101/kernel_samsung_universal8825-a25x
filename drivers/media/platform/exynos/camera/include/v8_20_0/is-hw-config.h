// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 * Pablo v8.20 specific hw configuration
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_CONFIG_H
#define IS_HW_CONFIG_H

enum taaisp_chain_id {
	ID_3AA_0 = 0,	/* MEIP/DMA/3AA0 */
	ID_3AA_1 = 1,	/* MEIP/DMA/3AA1 */
	ID_ISP_0 = 2,	/* TNR/DNS/ITP0 */
	ID_ISP_1 = 3,	/* not used */
	ID_TPU_0 = 4,	/* not used */
	ID_TPU_1 = 5,	/* not used */
	ID_DCP	 = 6,	/* not used */
	ID_3AA_2 = 7,	/* not used */
	ID_CLH_0 = 8,	/* not used */
	ID_3AA_3 = 9,	/* not used */
	ID_YPP   = 10,	/* not used */
	ID_3AAISP_MAX
};

/* the number of interrupt source at each IP */
enum hwip_interrupt_map {
	INTR_HWIP1 = 0,
	INTR_HWIP2 = 1,
	INTR_HWIP3 = 2,
	INTR_HWIP4 = 3,
	INTR_HWIP5 = 4,
	INTR_HWIP6 = 5,
	INTR_HWIP_MAX
};

enum cacheable_dma_map {
	ID_DBUF_MAX
};

#endif /* IS_HW_CONFIG_H */
