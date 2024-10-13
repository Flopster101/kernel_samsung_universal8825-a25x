// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 * Pablo v9.1 specific hw configuration
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
	ID_3AA_0 = 0, /* LME/ORBMCH/DMA/3AA0 */
	ID_3AA_1 = 1, /* LME/ORBMCH/DMA/3AA1 */
	ID_3AA_2 = 2, /* LME/ORBMCH/DMA/3AA2 */
	ID_3AA_3 = 3, /* LME/ORBMCH/DMA/3AA2 */
	ID_ISP_0 = 4, /* MCFP/DNS/ITP0 */
	ID_YPP	 = 5, /* YUVPP */
	ID_LME_0 = 6, /* LME */
	ID_LME_1 = 7, /* Not used */
	ID_ISP_1 = 8, /* Not used */
	ID_CLH_0 = 9, /* Not used */
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
	INTR_HWIP7 = 6,
	INTR_HWIP8 = 7,
	INTR_HWIP9 = 8,
	INTR_HWIP10 = 9,
	INTR_HWIP_MAX
};

enum internal_dma_map {
	ID_DMA_3AAISP = 0,
	ID_DMA_MEDRC = 1,
	ID_DMA_ORBMCH = 2,
	ID_DMA_TNR = 3,
	ID_DMA_CLAHE = 4,
	ID_DMA_MAX
};

enum cacheable_dma_map {
	/* 0 ~ 4: reserved for DDK internal_dma_map */
	ID_DBUF_LMEC = 5, /* LME HW output */
	ID_DBUF_MAX
};

#endif /* IS_HW_CONFIG_H */

