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

#ifndef IS_HW_API_3AA_H
#define IS_HW_API_3AA_H

#include "is-hw-common-dma.h"
#include "is-type.h"

enum taa_event_type {
	TAA_EVENT_FRAME_START,
	TAA_EVENT_FRAME_LINE,
	TAA_EVENT_FRAME_COREX,
	TAA_EVENT_FRAME_END,
	TAA_EVENT_FRAME_CDAF,
	TAA_EVENT_ERR,
	TAA_EVENT_WDMA_ABORT_DONE,
	TAA_EVENT_WDMA_FRAME_DROP,
	TAA_EVENT_WDMA_CFG_ERR,
	TAA_EVENT_WDMA_FIFOFULL_ERR,
	TAA_EVENT_WDMA_FSTART_IN_FLUSH_ERR,
};

enum taa_zsl_input_type {
	TAA_ZSL_INPUT_BCROP1,
	TAA_ZSL_INPUT_RECON
};

enum taa_dma_id {
	TAA_WDMA_THSTAT_PRE,
	TAA_WDMA_RGBYHIST,
	TAA_WDMA_THSTAT,
	TAA_WDMA_DRC,
	TAA_WDMA_FDPIG,
	TAA_WDMA_ORBDS0,
	TAA_WDMA_ORBDS1,
	TAA_WDMA_STRP,
	TAA_WDMA_ZSL,
	TAA_WDMA_MAX,
	TAA_DMA_MAX = TAA_WDMA_MAX,
};

enum taa_internal_buf_id {
	TAA_INTERNAL_BUF_CDAF,
	TAA_INTERNAL_BUF_THUMB_PRE,
	TAA_INTERNAL_BUF_THUMB,
	TAA_INTERNAL_BUF_RGBYHIST,
	TAA_INTERNAL_BUF_MAX
};

static const char * const taa_internal_buf_name[TAA_INTERNAL_BUF_MAX] = {
	"CAF",
	"TH0",
	"TH1",
	"HST",
};

struct taa_control_config {
	enum taa_zsl_input_type zsl_input_type;
	bool zsl_en;
	bool strp_en;
	u32 int_row_cord;
};

struct taa_cinfifo_config {
	bool enable;
	bool bit14_mode;
	u32 width;
	u32 height;
};

struct taa_buf_info {
	u32 w;
	u32 h;
	u32 bpp;
	u32 num;
};

struct taa_size_config {
	struct is_rectangle sensor_calibrated;
	u32 sensor_center_x;
	u32 sensor_center_y;
	u32 sensor_binning_x;
	u32 sensor_binning_y;
	struct is_crop sensor_crop;
	struct is_crop bcrop0;
	struct is_crop bcrop1;
	struct is_rectangle bds;
	struct is_crop bcrop2;

	u32 fdpig_en;
	u32 fdpig_crop_en;
	struct is_crop fdpig_crop;
	struct is_rectangle fdpig;
};

struct taa_format_config {
	u32 order;
};

/* CONTEXT 0 only */
int taa_hw_s_c0_reset(void __iomem *base);
int taa_hw_s_c0_control_init(void __iomem *base);
int taa_hw_s_c0_lic(void __iomem *base, u32 num_set, u32 *offset_seta, u32 *offset_setb);

/* CONTROL */
int taa_hw_s_control_init(void __iomem *base);
int taa_hw_s_control_config(void __iomem *base, struct taa_control_config *config);
int taa_hw_s_global_enable(void __iomem *base, bool enable, bool fro_en);

/* CONTINT */
void taa_hw_enable_int(void __iomem *base);
void taa_hw_disable_int(void __iomem *base);
void taa_hw_enable_csis_int(void __iomem *base);
void taa_hw_disable_csis_int(void __iomem *base);
u32 taa_hw_g_int_status(void __iomem *base, u32 irq_id, bool clear, bool fro_en);
u32 taa_hw_g_csis_int_status(void __iomem *base, u32 irq_id, bool clear);
void taa_hw_wait_isr_clear(void __iomem *base, void __iomem *zsl_base,
	void __iomem *strp_base, bool fro_en);
u32 taa_hw_occurred(u32 status, u32 irq_id, enum taa_event_type type);
void taa_hw_print_error(u32 irq_id, ulong err_state);

/* COREX */
int taa_hw_s_corex_init(void __iomem *base);
int taa_hw_enable_corex_trigger(void __iomem *base);
int taa_hw_disable_corex_trigger(void __iomem *base);
int taa_hw_enable_corex(void __iomem *base);
bool taa_hw_is_corex(u32 cr_offset);

/* CINFIFO */
int taa_hw_s_cinfifo_init(void __iomem *base);
int taa_hw_s_cinfifo_config(void __iomem *base, struct taa_cinfifo_config *config);

/* DMA */
int taa_hw_g_buf_info(enum taa_internal_buf_id buf_id, struct taa_buf_info *info);
int taa_hw_g_stat_param(enum taa_dma_id dma_id, u32 w, u32 h,
	struct param_dma_output *dma_output);
int taa_hw_create_dma(void __iomem *base, enum taa_dma_id dma_id, struct is_common_dma *dma);
int taa_hw_s_wdma(struct is_common_dma *dma, void __iomem *base,
	struct param_dma_output *dma_output, u32 *addr, u32 num_buffers);
int taa_hw_abort_wdma(struct is_common_dma *dma, void __iomem *base);

/* CDAF */
int taa_hw_g_cdaf_stat(void __iomem *base, u32 *buf, u32 *size);

/* MODULE */
int taa_hw_s_module_init(void __iomem *base);
int taa_hw_enable_dtp(void __iomem *base, bool enable);
int taa_hw_bypass_drc(void __iomem *base);
int taa_hw_s_module_bypass(void __iomem *base);
int taa_hw_s_module_size(void __iomem *base, struct taa_size_config *config);
int taa_hw_s_module_format(void __iomem *base, struct taa_format_config *config);

/* FRO */
int taa_hw_s_fro(void __iomem *base, u32 num_buffers);
void taa_hw_s_fro_reset(void __iomem *base, void __iomem *zsl_base,
		void __iomem *strp_base);

/* TUNE */
void taa_hw_s_crc(void __iomem *base, u32 crc_seed);

/* DUMP */
void taa_hw_dump(void __iomem *base);
void taa_hw_dump_zsl(void __iomem *base);
void taa_hw_dump_strp(void __iomem *base);

/* INFO */
u32 taa_hw_g_reg_cnt(void);

#endif
