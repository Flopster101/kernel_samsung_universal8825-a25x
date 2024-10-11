/* SPDX-License-Identifier: GPL-2.0
 *
 * Samsung Exynos SoC series Pablo driver
 *
 * MCFP HW control APIs
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_DNS_H
#define IS_HW_API_DNS_H

#include "is-hw-common-dma.h"
#include "is-hw-api-type.h"

enum dns_event_type {
	DNS_EVENT_FRAME_START,
	DNS_EVENT_FRAME_END,
	DNS_EVENT_ERR,
};

enum dns_input_type {
	DNS_INPUT_OTF = 0,
	DNS_INPUT_DMA = 1,
	DNS_INPUT_STRGEN = 2,
	DNS_INPUT_MAX
};

enum dns_dma_id {
	DNS_RDMA_BAYER = 0,
	DNS_RDMA_DRC0 = 1,
	DNS_RDMA_DRC1 = 2,
	DNS_RDMA_DRC2 = 3,
	DNS_RDMA_MAX,
	DNS_DMA_MAX = DNS_RDMA_MAX,
};
struct dns_control_config {
	enum dns_input_type input_type;
	bool dirty_bayer_sel_dns;
	bool rdma_dirty_bayer_en;
	bool ww_lc_en;
	bool hqdns_tnr_input_en;
};

struct dns_cinfifo_config {
	bool enable;
	u32 width;
	u32 height;
};

/* CONTROL */
int dns_hw_s_reset(void __iomem *base);
int dns_hw_s_control_init(void __iomem *base, u32 set_id);
int dns_hw_s_control_config(void __iomem *base, u32 set_id, struct dns_control_config *config);
void dns_hw_s_control_debug(void __iomem *base, u32 set_id);
int dns_hw_s_enable(void __iomem *base, bool one_shot, bool fro_en);
int dns_hw_s_disable(void __iomem *base);

/* CONTINT */
int dns_hw_s_int_init(void __iomem *base);
u32 dns_hw_g_int_status(void __iomem *base, u32 irq_id, bool clear, bool fro_en);
u32 dns_hw_is_occurred(u32 status, u32 irq_id, enum dns_event_type type);

/* CINFIFO */
int dns_hw_s_cinfifo_init(void __iomem *base, u32 set_id);
int dns_hw_s_cinfifo_config(void __iomem *base, u32 set_id, struct dns_cinfifo_config *config);

/* DMA */
int dns_hw_create_dma(void __iomem *base, enum dns_dma_id dma_id, struct is_common_dma *dma);
int dns_hw_s_rdma(u32 set_id, struct is_common_dma *dma, struct param_dma_input *dma_input,
	struct param_stripe_input *stripe_input, u32 *addr, u32 num_buffers);

/* MODULE */
int dns_hw_s_module_bypass(void __iomem *base, u32 set_id);
int dns_hw_bypass_drcdstr(void __iomem *base, u32 set_id);
int dns_hw_s_module_size(void __iomem *base, u32 set_id,
	struct is_hw_size_config *config, struct is_rectangle *drc_grid);
void dns_hw_s_module_format(void __iomem *base, u32 set_id, u32 bayer_order);
void dns_hw_enable_dtp(void __iomem *base, u32 set_id, bool en);

/* FRO */
int dns_hw_s_fro(void __iomem *base, u32 set_id, u32 num_buffers);

/* TUNE */
void dns_hw_s_crc(void __iomem *base, u32 seed);

/* DUMP */
void dns_hw_dump(void __iomem *base);

/* INFO */
u32 dns_hw_g_reg_cnt(void);

#endif
