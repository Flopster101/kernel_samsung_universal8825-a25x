// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <dqe_cal.h>
#include <decon_cal.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_dsim.h>

static int dpu_dqe_log_level = 6;
module_param(dpu_dqe_log_level, int, 0600);
MODULE_PARM_DESC(dpu_dqe_log_level, "log level for dpu dqe [default : 6]");

#define DQE_DRIVER_NAME "exynos-drmdqe"
#define dqe_info(dqe, fmt, ...)	\
dpu_pr_info(DQE_DRIVER_NAME, (dqe)->id, dpu_dqe_log_level, fmt, ##__VA_ARGS__)

#define dqe_warn(dqe, fmt, ...)	\
dpu_pr_warn(DQE_DRIVER_NAME, (dqe)->id, dpu_dqe_log_level, fmt, ##__VA_ARGS__)

#define dqe_err(dqe, fmt, ...)	\
dpu_pr_err(DQE_DRIVER_NAME, (dqe)->id, dpu_dqe_log_level, fmt, ##__VA_ARGS__)

#define dqe_debug(dqe, fmt, ...)\
dpu_pr_debug(DQE_DRIVER_NAME, (dqe)->id, dpu_dqe_log_level, fmt, ##__VA_ARGS__)

enum dqe_reset_type {
	DQE_RESET_GLOBAL = 0,	// sets when SFR need to be updated
	DQE_RESET_CGC,			// sets when CGC LUT need to be updated
};

enum dqe_reset_ops {
	DQE_RESET_FULL = 0,	// sets when require to reset all
	DQE_RESET_OPT,		// sets when optimized updates are required
};

enum dqe_update_flag {
	DQE_UPDATED_NONE	= 0,
	DQE_UPDATED_CONTEXT	= 1 << 0,
	DQE_UPDATED_CONFIG	= 1 << 1,
};

enum dqe_bpc_type {
	DQE_BPC_TYPE_10	= 0,
	DQE_BPC_TYPE_8,
	DQE_BPC_TYPE_MAX,
};

#define DQE_MODE_MAIN 0
#define DQE_MODE_PRESET 1 // start from 1
#define DQE_PRESET_ATTR_NUM 4
#define DQE_PRESET_CM_SHIFT 16
#define DQE_PRESET_RI_SHIFT 0
#define DQE_PRESET_ALL_MASK 0xFFFF

// saved LUTs of each mode type
struct dqe_ctx_int {
	u32 mode_attr[DQE_PRESET_ATTR_NUM];

	u32 disp_dither[DQE_DISP_DITHER_LUT_MAX];
	u32 cgc_dither[DQE_CGC_DITHER_LUT_MAX];

	int gamma_matrix[DQE_GAMMA_MATRIX_LUT_MAX];
	int degamma_lut_ext;
	u32 degamma_lut[DQE_BPC_TYPE_MAX][DQE_DEGAMMA_LUT_MAX];
	int regamma_lut_ext;
	u32 regamma_lut[DQE_BPC_TYPE_MAX][DQE_REGAMMA_LUT_MAX];

	u32 cgc17_encoded[3][17][17][5];
	int cgc17_enc_rgb;
	int cgc17_enc_idx;
	u32 cgc17_con[DQE_CGC_CON_LUT_MAX];
	u32 cgc17_lut[3][DQE_CGC_LUT_MAX];

	int hsc48_lcg_idx;
	u32 hsc48_lcg[3][DQE_HSC_LUT_LSC_GAIN_MAX];
	u32 hsc48_lut[DQE_HSC_LUT_MAX];

	u32 atc_lut[DQE_ATC_LUT_MAX];

	u32 scl_lut[DQE_SCL_LUT_MAX];
	u32 scl_input[DQE_SCL_COEF_MAX+1]; // en + coef sets

	u32 de_lut[DQE_DE_LUT_MAX];
};

#define DQE_COLORMODE_MAGIC 0xDA
enum dqe_colormode_id {
	DQE_COLORMODE_ID_CGC17_ENC = 1,
	DQE_COLORMODE_ID_CGC17_CON,
	DQE_COLORMODE_ID_CGC_DITHER,
	DQE_COLORMODE_ID_DEGAMMA,
	DQE_COLORMODE_ID_GAMMA,
	DQE_COLORMODE_ID_GAMMA_MATRIX,
	DQE_COLORMODE_ID_HSC48_LCG,
	DQE_COLORMODE_ID_HSC,
	DQE_COLORMODE_ID_SCL,
	DQE_COLORMODE_ID_MAX,
};

struct dqe_colormode_global_header {
	u8 magic;
	u8 seq_num;
	u16 total_size;
	u16 header_size;
	u16 num_data;
	u32 reserved;
};

struct dqe_colormode_data_header {
	u8 magic;
	u8 id;
	u16 total_size;
	u16 header_size;
	u8 attr[4];
	u16 reserved;
};

static struct dqe_ctx_int *dqe_lut;
static u32 mode_idx = DQE_MODE_MAIN;
static char dqe_str_result[1024];

// 2 luts in 1 reg
#define R2L_I(_v, _i)	(((_v) << 1) | ((_i) & 0x1))
#define R2L_D(_v, _i)	(((_v) >> (0 + (16 * ((_i) & 0x1)))) & 0x1fff)

/* 8-tap Filter Coefficient */
static s16 ps_h_c_8t[1][16][8] = {
	{	/* Ratio <= 65536 (~8:8) Selection = 0 */
		{   0,	 0,   0, 512,	0,   0,   0,   0 },
		{  -2,	 8, -25, 509,  30,  -9,   2,  -1 },
		{  -4,	14, -46, 499,  64, -19,   5,  -1 },
		{  -5,	20, -62, 482, 101, -30,   8,  -2 },
		{  -6,	23, -73, 458, 142, -41,  12,  -3 },
		{  -6,	25, -80, 429, 185, -53,  15,  -3 },
		{  -6,	26, -83, 395, 228, -63,  19,  -4 },
		{  -6,	25, -82, 357, 273, -71,  21,  -5 },
		{  -5,	23, -78, 316, 316, -78,  23,  -5 },
		{  -5,	21, -71, 273, 357, -82,  25,  -6 },
		{  -4,	19, -63, 228, 395, -83,  26,  -6 },
		{  -3,	15, -53, 185, 429, -80,  25,  -6 },
		{  -3,	12, -41, 142, 458, -73,  23,  -6 },
		{  -2,	 8, -30, 101, 482, -62,  20,  -5 },
		{  -1,	 5, -19,  64, 499, -46,  14,  -4 },
		{  -1,	 2,  -9,  30, 509, -25,   8,  -2 }
	},
};

/* 4-tap Filter Coefficient */
static s16 ps_v_c_4t[1][16][4] = {
	{	/* Ratio <= 65536 (~8:8) Selection = 0 */
		{   0, 512,   0,   0 },
		{ -15, 508,  20,  -1 },
		{ -25, 495,  45,  -3 },
		{ -31, 473,  75,  -5 },
		{ -33, 443, 110,  -8 },
		{ -33, 408, 148, -11 },
		{ -31, 367, 190, -14 },
		{ -27, 324, 234, -19 },
		{ -23, 279, 279, -23 },
		{ -19, 234, 324, -27 },
		{ -14, 190, 367, -31 },
		{ -11, 148, 408, -33 },
		{  -8, 110, 443, -33 },
		{  -5,  75, 473, -31 },
		{  -3,  45, 495, -25 },
		{  -1,  20, 508, -15 }
	},
};

static int dqe_conv_str2lut(const char *buffer, u32 *lut,
				int size, bool printHex);
static int dqe_conv_str2cgc(const char *buffer, struct dqe_ctx_int *lut);
static int dqe_dec_cgc17(struct dqe_ctx_int *lut, int mask, int wr_mode);
static void dqe_set_degamma_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced);
static void dqe_set_gamma_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced);
static ssize_t dqe_conv_lut2str(u32 *lut, int size, char *out);
static struct device_attribute *dqe_attrs[];

static inline bool IS_DQE_UPDATE_REQUIRED(struct exynos_dqe *dqe)
{
	return atomic_add_unless(&dqe->update_cnt, -1, 0);
}

// update SFR only when CGC LUT changed if sram retention is enabled
static inline bool IS_DQE_CGCUPDATE_REQUIRED(struct exynos_dqe *dqe)
{
	return !dqe->sram_retention ||
		atomic_add_unless(&dqe->cgc_update_cnt, -1, 0);
}

// maximum 2 updates required for SRAM Set0/1
#define MAX_RESET_CNT 2
static void dqe_reset_update_cnt(struct exynos_dqe *dqe,
				enum dqe_reset_type type, enum dqe_reset_ops ops)
{
	switch (type) {
	case DQE_RESET_GLOBAL:
		// at least 1 update required, no change if higher
		if (ops == DQE_RESET_OPT)
			atomic_cmpxchg(&dqe->update_cnt, 0, 1);
		else
			atomic_set(&dqe->update_cnt, MAX_RESET_CNT);
		break;
	case DQE_RESET_CGC:
		// restore cnt as SRAM indicator also would be reset on dpu off
		if (ops == DQE_RESET_OPT)
			atomic_cmpxchg(&dqe->cgc_update_cnt, 1, MAX_RESET_CNT);
		else
			atomic_set(&dqe->cgc_update_cnt, MAX_RESET_CNT);
		break;
	default:
		dqe_err(dqe, "%s invalid parameter\n", __func__);
		break;
	}
	dqe_debug(dqe, "%d updates reqired\n", atomic_read(&dqe->update_cnt));
}

#define DQE_CTRL_OFF 0x00
#define DQE_CTRL_ON 0x01
#define DQE_CTRL_AUTO 0x02
#define DQE_CTRL_FORCEDOFF 0x10

#define DQE_CTRL_ONOFF_MASK 0x0F
#define DQE_CTRL_FORCEDOFF_MASK 0xF0

static inline void dqe_ctrl_forcedoff(u32 *value, int forcedOff)
{
	if (!value)
		return;

	*value &= ~DQE_CTRL_FORCEDOFF_MASK;
	if (forcedOff > 0)
		*value |= DQE_CTRL_FORCEDOFF;
}

static inline void dqe_ctrl_onoff(u32 *value, u32 onOff, bool clrForced)
{
	if (!value)
		return;

	*value &= ~DQE_CTRL_ONOFF_MASK;
	if (clrForced)
		*value &= ~DQE_CTRL_FORCEDOFF_MASK;
	*value |= (onOff & DQE_CTRL_ONOFF_MASK);
}

static inline bool IS_DQE_ON(u32 *value)
{
	if (!value || (*value & DQE_CTRL_FORCEDOFF_MASK))
		return false;
	return ((*value & DQE_CTRL_ONOFF_MASK) == DQE_CTRL_ON);
}

static inline bool IS_DQE_AUTO(u32 *value)
{
	if (!value || (*value & DQE_CTRL_FORCEDOFF_MASK))
		return false;
	return ((*value & DQE_CTRL_ONOFF_MASK) == DQE_CTRL_AUTO);
}

static inline bool IS_DQE_FORCEDOFF(u32 *value)
{
	if (!value)
		return false;
	return ((*value & DQE_CTRL_FORCEDOFF_MASK) == DQE_CTRL_FORCEDOFF);
}

static char *dqe_print_onoff(u32 *value)
{
	if (IS_DQE_AUTO(value))
		return "A";
	if (IS_DQE_FORCEDOFF(value))
		return "0*";
	if (IS_DQE_ON(value))
		return "1";
	return "0";
}

static int dqe_alloc_cgc_dma(struct exynos_dqe *dqe, size_t size,
				struct device *dev)
{
	struct dma_buf *buf;
	struct dma_heap *dma_heap;
	void *vaddr;
	struct dma_buf_attachment *attachment;
	struct sg_table *sg_table;
	dma_addr_t dma_addr;

	dqe_debug(dqe, "%s +\n", __func__);

	size = PAGE_ALIGN(size);

	dqe_debug(dqe, "want %u bytes\n", size);
	dma_heap = dma_heap_find("system-uncached");
	if (IS_ERR_OR_NULL(dma_heap)) {
		dqe_err(dqe, "dma_heap_find() failed [%x]\n", dma_heap);
		return PTR_ERR(dma_heap);
	}

	buf = dma_heap_buffer_alloc(dma_heap, (size_t)size, 0, 0);
	dma_heap_put(dma_heap);
	if (IS_ERR_OR_NULL(buf)) {
		dqe_err(dqe, "dma_buf allocation is failed [%x]\n", buf);
		return PTR_ERR(buf);
	}

	vaddr = dma_buf_vmap(buf);
	if (IS_ERR_OR_NULL(vaddr)) {
		dqe_err(dqe, "failed to vmap buffer [%x]\n", vaddr);
		return -EINVAL;
	}

	/* mapping buffer for translating to DVA */
	attachment = dma_buf_attach(buf, dev);
	if (IS_ERR_OR_NULL(attachment)) {
		dqe_err(dqe, "failed to attach dma_buf [%x]\n", attachment);
		return -EINVAL;
	}

	sg_table = dma_buf_map_attachment(attachment, DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(sg_table)) {
		dqe_err(dqe, "failed to map attachment [%x]\n", sg_table);
		return -EINVAL;
	}

	dma_addr = sg_dma_address(sg_table->sgl);
	if (IS_ERR_VALUE(dma_addr)) {
		dqe_err(dqe, "failed to map iovmm [%x]\n", dma_addr);
		return -EINVAL;
	}

	dqe->ctx.cgc_lut = vaddr;
	dqe->ctx.cgc_dma = dma_addr;
	dqe_debug(dqe, "cgc lut vaddr %p paddr 0x%llx\n", vaddr, dma_addr);

	return 0;
}

static void dqe_load_context(struct exynos_dqe *dqe)
{
	int i, rgb;
	u32 val;
	u32 id = dqe->id;

	dqe_debug(dqe, "%s\n", __func__);

	dqe->ctx.version = dqe_reg_get_version(id);

	for (i = 0; i < DQE_GAMMA_MATRIX_REG_MAX; i++)
		dqe->ctx.gamma_matrix[i] = dqe_reg_get_lut(id, DQE_REG_GAMMA_MATRIX, i, 0);

	for (i = 0; i < DQE_DEGAMMA_REG_MAX; i++)
		dqe->ctx.degamma_lut[i] = dqe_reg_get_lut(id, DQE_REG_DEGAMMA, i, 0);

	for (i = 0; i < DQE_CGC_CON_REG_MAX; i++)
		dqe->ctx.cgc_con[i] = dqe_reg_get_lut(id, DQE_REG_CGC_CON, i, 0);

	for (i = 0; i < DQE_CGC_REG_MAX; i++) {
		for (rgb = 0; rgb < 3; rgb++) {
			val = dqe_reg_get_lut(id, DQE_REG_CGC, i, rgb); // 2 LUTs in 1 REG
			dqe->ctx.cgc_lut[2*i+1][rgb] = R2L_D(val, 2*i+1);
			dqe->ctx.cgc_lut[2*i][rgb] = R2L_D(val, 2*i);
		}
	}

	for (i = 0; i < DQE_REGAMMA_REG_MAX; i++)
		dqe->ctx.regamma_lut[i] = dqe_reg_get_lut(id, DQE_REG_REGAMMA, i, 0);

	dqe->ctx.disp_dither = dqe_reg_get_lut(id, DQE_REG_DISP_DITHER, 0, 0);
	dqe->ctx.cgc_dither = dqe_reg_get_lut(id, DQE_REG_CGC_DITHER, 0, 0);

	for (i = 0; i < DQE_HSC_REG_CTRL_MAX; i++)
		dqe->ctx.hsc_con[i] = dqe_reg_get_lut(id, DQE_REG_HSC, i, 0);
	for (i = 0; i < DQE_HSC_REG_POLY_MAX; i++)
		dqe->ctx.hsc_poly[i] = dqe_reg_get_lut(id, DQE_REG_HSC, i, 1);
	for (i = 0; i < DQE_HSC_REG_GAIN_MAX; i++)
		dqe->ctx.hsc_gain[i] = dqe_reg_get_lut(id, DQE_REG_HSC, i, 2);

	for (i = 0; i < DQE_ATC_REG_MAX; i++)
		dqe->ctx.atc[i] = dqe_reg_get_lut(id, DQE_REG_ATC, i, 0);

	for (i = 0; i < DQE_SCL_REG_MAX; i++)
		dqe->ctx.scl[i] = dqe_reg_get_lut(id, DQE_REG_SCL, i, 0);

	for (i = 0; i < DQE_DE_REG_MAX; i++)
		dqe->ctx.de[i] = dqe_reg_get_lut(id, DQE_REG_DE, i, 0);

	for (i = 0; i < DQE_LPD_REG_MAX; i++)
		dqe->ctx.lpd[i] = dqe_reg_get_lut(id, DQE_REG_LPD, i, 0);

	for (i = 0; i < DQE_REGAMMA_REG_MAX; i++)
		dqe_debug(dqe, "%08x ", dqe->ctx.regamma_lut[i]);

	for (i = 0; i < DQE_DEGAMMA_REG_MAX; i++)
		dqe_debug(dqe, "%08x ", dqe->ctx.degamma_lut[i]);

	dqe_ctrl_onoff(&dqe->ctx.disp_dither_on, DQE_CTRL_AUTO, true);
}

static void dqe_init_context(struct exynos_dqe *dqe, struct device *dev)
{
	int i, j, k, mode_type, ret, bpc;
	struct dqe_ctx_int *lut;

	dqe_debug(dqe, "%s\n", __func__);

	memset(&dqe->colormode, 0, sizeof(dqe->colormode));
	dqe->colormode.dqe_fd = -1;
	dqe->colormode.seq_num = (u8)-1;

	memset(&dqe->ctx, 0, sizeof(dqe->ctx));
	// changed array to pointer as exynos_drm_dqe.h header is commonly used
	dqe->ctx.gamma_matrix = kzalloc(DQE_GAMMA_MATRIX_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.degamma_lut = kzalloc(DQE_DEGAMMA_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.regamma_lut = kzalloc(DQE_REGAMMA_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.cgc_con = kzalloc(DQE_CGC_CON_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.hsc_con = kzalloc(DQE_HSC_REG_CTRL_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.hsc_poly = kzalloc(DQE_HSC_REG_POLY_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.hsc_gain = kzalloc(DQE_HSC_REG_GAIN_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.atc = kzalloc(DQE_ATC_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.scl = kzalloc(DQE_SCL_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.de = kzalloc(DQE_DE_REG_MAX*sizeof(u32), GFP_KERNEL);
	dqe->ctx.lpd = kzalloc(DQE_LPD_REG_MAX*sizeof(u32), GFP_KERNEL);

	if (dqe->regs.edma_base_regs) {
		ret = dqe_alloc_cgc_dma(dqe, DQE_CGC_LUT_MAX*3*sizeof(u16), dev);
		if (ret < 0) {
			dqe_err(dqe, "failed to alloc cgc lut\n");
			return;
		}
	} else {
		dqe->ctx.cgc_lut = kzalloc(DQE_CGC_LUT_MAX*3*sizeof(u16), GFP_KERNEL);
	}
	dqe_load_context(dqe);

	for (mode_type = DQE_MODE_MAIN; mode_type < dqe->num_lut; mode_type++) {
		lut = &dqe_lut[mode_type];

#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
		for (bpc = 0; bpc < DQE_BPC_TYPE_MAX; bpc++) {
			lut->degamma_lut[bpc][0] = 0;
			for (i = 1; i < DQE_DEGAMMA_LUT_MAX; i++)
				lut->degamma_lut[bpc][i] = ((i - 1) % DQE_DEGAMMA_LUT_CNT) * 64;

			lut->regamma_lut[bpc][0] = 0;
			for (i = 1; i < DQE_REGAMMA_LUT_MAX; i++)
				lut->regamma_lut[bpc][i] = ((i - 1) % DQE_REGAMMA_LUT_CNT) * 64;
		}
#else
		for (bpc = 0; bpc < DQE_BPC_TYPE_MAX; bpc++) {
			lut->degamma_lut[bpc][0] = 0;
			for (i = 1; i < DQE_DEGAMMA_LUT_MAX; i++) {
				if ((((i - 1) / DQE_DEGAMMA_LUT_CNT) % 2) == 0) // x 10bit range
					lut->degamma_lut[bpc][i] = ((i - 1) % DQE_DEGAMMA_LUT_CNT) * 32;
				else // y 12bit range
					lut->degamma_lut[bpc][i] = ((i - 1) % DQE_DEGAMMA_LUT_CNT) * 128;
			}
			for (i = DQE_DEGAMMA_LUT_CNT; i < DQE_DEGAMMA_LUT_MAX; i += DQE_DEGAMMA_LUT_CNT)
				lut->degamma_lut[bpc][i] -= lut->degamma_lut[bpc][i-1];

			lut->regamma_lut[bpc][0] = 0;
			for (i = 1; i < DQE_REGAMMA_LUT_MAX; i++) // x,y 12bit range
				lut->regamma_lut[bpc][i] = ((i - 1) % DQE_REGAMMA_LUT_CNT) * 128;
			for (i = DQE_REGAMMA_LUT_CNT; i < DQE_REGAMMA_LUT_MAX; i += DQE_REGAMMA_LUT_CNT)
				lut->regamma_lut[bpc][i] -= lut->regamma_lut[bpc][i-1];
		}
#endif
		for (i = 0; i < 17; i++)
			for (j = 0; j < 17; j++)
				for (k = 0; k < 17; k++) {
					int pos = i * 17 * 17 + j * 17 + k;

					lut->cgc17_lut[0][pos] = i * 256;
					lut->cgc17_lut[1][pos] = j * 256;
					lut->cgc17_lut[2][pos] = k * 256;
		}
	}
}

static int dqe_restore_context(struct exynos_dqe *dqe)
{
	int i, opt;
	u32 id = dqe->id;
	u64 time_s, time_c;
	bool isCgcLutUpdated = false;

	if (!dqe->cfg.in_bpc || !dqe->cfg.width || !dqe->cfg.height) // no received bpc info yet
		return -1;

	mutex_lock(&dqe->lock);

	time_s = ktime_to_us(ktime_get());

	dqe_reg_init(dqe->id, dqe->cfg.width, dqe->cfg.height);

	/* DQE_DISP_DITHER */
	if (IS_DQE_AUTO(&dqe->ctx.disp_dither_on)) { // Automatic by BPC
		if (dqe->cfg.in_bpc >= 10 && dqe->cfg.out_bpc == 8)
			dqe_reg_set_lut_on(id, DQE_REG_DISP_DITHER, 1);
		else
			dqe_reg_set_lut_on(id, DQE_REG_DISP_DITHER, 0);
	} else {
		if (IS_DQE_ON(&dqe->ctx.disp_dither_on)) {
			dqe_reg_set_lut(id, DQE_REG_DISP_DITHER, 0, dqe->ctx.disp_dither, 0);
			dqe_reg_set_lut_on(id, DQE_REG_DISP_DITHER, 1);
		} else {
			dqe_reg_set_lut_on(id, DQE_REG_DISP_DITHER, 0);
		}
	}

	/* DQE_CGC_DITHER */
	if (IS_DQE_ON(&dqe->ctx.cgc_dither_on)) {
		dqe_reg_set_lut(id, DQE_REG_CGC_DITHER, 0, dqe->ctx.cgc_dither, 0);
		dqe_reg_set_lut_on(id, DQE_REG_CGC_DITHER, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_CGC_DITHER, 0);
		if (IS_DQE_ON(&dqe->ctx.regamma_on) ||
			IS_DQE_ON(&dqe->ctx.degamma_on) || IS_DQE_ON(&dqe->ctx.cgc_on))
			dqe_err(dqe, "CGC Dither must be on\n");
	}

	/* DQE_GAMMA_MATRIX */
	if (IS_DQE_ON(&dqe->ctx.gamma_matrix_on)) {
		for (i = 1; i < DQE_GAMMA_MATRIX_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_GAMMA_MATRIX, i, dqe->ctx.gamma_matrix[i], 0);
		dqe_reg_set_lut_on(id, DQE_REG_GAMMA_MATRIX, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_GAMMA_MATRIX, 0);
	}

	/* DQE_INFO_DEGAMMA */
	if (IS_DQE_ON(&dqe->ctx.degamma_on)) {
		for (i = 1; i < DQE_DEGAMMA_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_DEGAMMA, i, dqe->ctx.degamma_lut[i], 0);
		dqe_reg_set_lut_on(id, DQE_REG_DEGAMMA, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_DEGAMMA, 0);
	}

	/* DQE_INFO_CGC & DQE_CGC_LUT */
	if (IS_DQE_ON(&dqe->ctx.cgc_on)) {
		if (IS_DQE_CGCUPDATE_REQUIRED(dqe)) {
			if (dqe->regs.edma_base_regs) {
				dqe_reg_set_lut_on(id, DQE_REG_CGC_DMA, 1);
				edma_reg_set_base_addr(id, dqe->ctx.cgc_dma);
				edma_reg_set_start(id, 1);
				dqe_reg_wait_cgc_dmareq(id);
			} else { // if no edma defined, write thru APB directly.
				for (i = 0; i < DQE_CGC_REG_MAX; i++) {
					for (opt = DQE_REG_CGC_R; opt < DQE_REG_CGC_MAX; opt++) {
						u32 val_h, val_l;

						val_h = CGC_LUT_H(dqe->ctx.cgc_lut[2*i+1][opt]);
						val_l = CGC_LUT_L(dqe->ctx.cgc_lut[2*i][opt]);
						dqe_reg_set_lut(id, DQE_REG_CGC, i, (val_h | val_l), opt);
					}
				}
			}
			isCgcLutUpdated = true;
		}
		for (i = 0; i < DQE_CGC_CON_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_CGC_CON, i, dqe->ctx.cgc_con[i], 0);
		dqe_reg_set_lut_on(id, DQE_REG_CGC_CON, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_CGC_CON, 0);
	}

	/* DQE_REGAMMA */
	if (IS_DQE_ON(&dqe->ctx.regamma_on)) {
		for (i = 1; i < DQE_REGAMMA_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_REGAMMA, i, dqe->ctx.regamma_lut[i], 0);
		dqe_reg_set_lut_on(id, DQE_REG_REGAMMA, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_REGAMMA, 0);
	}

	/* DQE_HSC */
	if (IS_DQE_ON(&dqe->ctx.hsc_on)) {
		for (i = 1; i < DQE_HSC_REG_CTRL_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_HSC, i, dqe->ctx.hsc_con[i], DQE_REG_HSC_CON);
		for (i = 0; i < DQE_HSC_REG_POLY_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_HSC, i, dqe->ctx.hsc_poly[i], DQE_REG_HSC_POLY);
		for (i = 0; i < DQE_HSC_REG_GAIN_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_HSC, i, dqe->ctx.hsc_gain[i], DQE_REG_HSC_GAIN);
		dqe_reg_set_lut(id, DQE_REG_HSC, 0, dqe->ctx.hsc_con[0], DQE_REG_HSC_CON);
		dqe_reg_set_lut_on(id, DQE_REG_HSC, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_HSC, 0);
	}

	/* DQE_ATC */
	for (i = 0; i < DQE_ATC_REG_MAX; i++) {
		if (dqe->ctx.atc_dim_off && i == 8)
			dqe_reg_set_lut(id, DQE_REG_ATC, i, 0, 0);
		else
			dqe_reg_set_lut(id, DQE_REG_ATC, i, dqe->ctx.atc[i], 0);
	}

	if (IS_DQE_ON(&dqe->ctx.atc_on)) {
		dqe_reg_set_atc_partial_ibsi(id, dqe->cfg.width, dqe->cfg.height);
		dqe_reg_set_lut_on(id, DQE_REG_ATC, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_ATC, 0);
	}

	/* DQE_DE */
	if (IS_DQE_ON(&dqe->ctx.de_on)) {
		for (i = 0; i < DQE_DE_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_DE, i, dqe->ctx.de[i], 0);
		dqe_reg_set_lut_on(id, DQE_REG_DE, 1);
	} else {
		dqe_reg_set_lut_on(id, DQE_REG_DE, 0);
	}

	/* POST_SCL */
	if (IS_DQE_ON(&dqe->ctx.scl_on)) {
		for (i = 0; i < DQE_SCL_REG_MAX; i++)
			dqe_reg_set_lut(id, DQE_REG_SCL, i, dqe->ctx.scl[i], 0);
	}

	time_c = (ktime_to_us(ktime_get())-time_s);
	dqe_debug(dqe, "update%d-%d,%d %lld.%03lldms di %s/%s gm %s dg %s cgc%s %s rg %s hsc %s atc %s scl %s\n",
		!IS_ENABLED(CONFIG_SOC_S5E9925_EVT0), atomic_read(&dqe->update_cnt),
		dqe->cfg.in_bpc, time_c/USEC_PER_MSEC, time_c%USEC_PER_MSEC,
		dqe_print_onoff(&dqe->ctx.disp_dither_on),
		dqe_print_onoff(&dqe->ctx.cgc_dither_on),
		dqe_print_onoff(&dqe->ctx.gamma_matrix_on),
		dqe_print_onoff(&dqe->ctx.degamma_on),
		isCgcLutUpdated?"+":"", dqe_print_onoff(&dqe->ctx.cgc_on),
		dqe_print_onoff(&dqe->ctx.regamma_on),
		dqe_print_onoff(&dqe->ctx.hsc_on),
		dqe_print_onoff(&dqe->ctx.atc_on),
		dqe_print_onoff(&dqe->ctx.scl_on));

	DPU_EVENT_LOG(DPU_EVT_DQE_RESTORE, dqe->decon->crtc, NULL);

	mutex_unlock(&dqe->lock);

	return 0;
}

static char *dqe_acquire_colormode_ctx(struct exynos_dqe *dqe)
{
	int i, ctx_no, size;
	char *ctx;

	if (!dqe->colormode.dma_buf || !dqe->colormode.dma_vbuf)
		return NULL;

	size = dqe->colormode.dma_buf->size;
	if (dqe->colormode.ctx_size != size) {
		for (i = 0; i < MAX_DQE_COLORMODE_CTX; i++) {
			if (dqe->colormode.ctx[i])
				kfree(dqe->colormode.ctx[i]);

			dqe->colormode.ctx[i] = kzalloc(size, GFP_KERNEL);
		}
		dqe_info(dqe, "ctx realloc %d -> %d\n", dqe->colormode.ctx_size, size);
	}
	dqe->colormode.ctx_size = size;

	ctx_no = (atomic_inc_return(&dqe->colormode.ctx_no) & INT_MAX) % MAX_DQE_COLORMODE_CTX;
	ctx = dqe->colormode.ctx[ctx_no];
	if (ctx)
		memcpy(ctx, (void *)dqe->colormode.dma_vbuf, size);
	return ctx;
}

static int dqe_alloc_colormode_dma(struct exynos_dqe *dqe,
					struct drm_crtc_state *crtc_state)
{
	struct dma_buf *buf = NULL;
	void *vaddr = NULL;
	int64_t dqe_fd;
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	dqe_fd = exynos_crtc_state->dqe_fd;
	if (dqe_fd <= 0) {
		dqe_debug(dqe, "not allocated dqe fd %d\n", dqe_fd);
		goto error;
	}

	buf = dma_buf_get(dqe_fd);
	if (IS_ERR_OR_NULL(buf)) {
		if ((dqe_fd == dqe->colormode.dqe_fd) && IS_ERR(buf)) {
			dqe_debug(dqe, "bad fd [%ld] but continued with old vbuf\n", PTR_ERR(buf));
			goto done;
		}

		dqe_warn(dqe, "failed to get dma buf [%x] of fd [%lld]\n", buf, dqe_fd);
		goto error;
	}

	if ((dqe_fd == dqe->colormode.dqe_fd) && (buf == dqe->colormode.dma_buf)) {
		dma_buf_put(buf);
		goto done;
	}

	// release old buf
	if (dqe->colormode.dma_buf) {
		if (dqe->colormode.dma_vbuf)
			dma_buf_vunmap(dqe->colormode.dma_buf, dqe->colormode.dma_vbuf);
		dma_buf_put(dqe->colormode.dma_buf);
	}

	vaddr = dma_buf_vmap(buf);
	if (IS_ERR_OR_NULL(vaddr)) {
		dqe_err(dqe, "failed to vmap buffer [%x]\n", vaddr);
		goto error;
	}

	dqe->colormode.dqe_fd = dqe_fd;
	dqe->colormode.dma_buf = buf;
	dqe->colormode.dma_vbuf = vaddr;

done:
	exynos_crtc_state->dqe_colormode_ctx = dqe_acquire_colormode_ctx(dqe);
	return 0;

error:
	if (!IS_ERR_OR_NULL(buf)) {
		if (!IS_ERR_OR_NULL(vaddr))
			dma_buf_vunmap(buf, vaddr);
		dma_buf_put(buf);
	}
	exynos_crtc_state->dqe_colormode_ctx = NULL;
	return -1;
}

static int dqe_prepare_context(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	int ret;

	ret = dqe_alloc_colormode_dma(dqe, crtc_state);
	if (ret < 0)
		return -1;
	return 0;
}

// update LUTs from libcolor & HWC
static u32 dqe_update_colormode(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	struct dqe_ctx_int *ctx_i;
	u32 updated = 0;
	int ret;
	const struct dqe_colormode_global_header *hdr;
	const struct dqe_colormode_data_header *data_h;
	const char *data;
	u32 *lut;
	u16 size, len, count = 0, count_cgc = 0;
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	if (!exynos_crtc_state->dqe_colormode_ctx)
		return DQE_UPDATED_NONE;

	hdr = (struct dqe_colormode_global_header *)exynos_crtc_state->dqe_colormode_ctx;
	if (!hdr) {
		dqe_err(dqe, "no allocated virtual buffer\n");
		goto error;
	}

	if (hdr->magic != DQE_COLORMODE_MAGIC) {
		dqe_err(dqe, "invalid header magic %x\n", hdr->magic);
		goto error;
	}

	if (!hdr->total_size || !hdr->header_size ||
			(hdr->total_size < hdr->header_size) ||
			(hdr->header_size < sizeof(*hdr))) {
		dqe_err(dqe, "invalid size: total %d, header %d\n",
					hdr->total_size, hdr->header_size);
		goto error;
	}

	if (hdr->seq_num == dqe->colormode.seq_num)
		goto done;

	dqe->colormode.seq_num = hdr->seq_num;

	size = hdr->header_size;
	while (size < hdr->total_size) {
		data_h = (struct dqe_colormode_data_header *)((char *)hdr + size);
		if (data_h->magic != DQE_COLORMODE_MAGIC) {
			dqe_err(dqe, "invalid data magic %x\n", data_h->magic);
			goto error;
		}

		if (!data_h->total_size || !data_h->header_size ||
				(data_h->total_size < data_h->header_size) ||
				(data_h->header_size < sizeof(*data_h))) {
			dqe_err(dqe, "invalid data size: total %d, header %d\n",
					data_h->total_size, data_h->header_size);
			goto error;
		}

		data = (const char *)((char *)data_h + data_h->header_size);

		mutex_lock(&dqe->lock);
		ctx_i = &dqe_lut[DQE_MODE_MAIN];
		if (data_h->id == DQE_COLORMODE_ID_CGC17_ENC) {
			if (data_h->attr[0] > 2 || data_h->attr[1] > 16) {
				dqe_err(dqe, "invalid CGC attr %d/%d\n", data_h->attr[0], data_h->attr[1]);
				goto error_locked;
			}

			ctx_i->cgc17_enc_rgb = data_h->attr[0];
			ctx_i->cgc17_enc_idx = data_h->attr[1];
			ret = dqe_conv_str2cgc(data, ctx_i);
			if (ret < 0)
				goto error_locked;

			count_cgc++;

			// decode cgc lut when received the last encoded one
			if (count_cgc == 3*17) {
				ret = dqe_dec_cgc17(ctx_i, 0x7, true);
				if (ret < 0)
					goto error_locked;
				dqe_reset_update_cnt(dqe, DQE_RESET_CGC, DQE_RESET_FULL);
				updated |= (1<<DQE_REG_CGC);
			}
		}	else {
			switch (data_h->id) {
			case DQE_COLORMODE_ID_CGC17_CON:
				lut = ctx_i->cgc17_con;
				len = ARRAY_SIZE(ctx_i->cgc17_con);
				updated |= (1<<DQE_REG_CGC_CON);
				break;
			case DQE_COLORMODE_ID_DEGAMMA:
				if (data_h->attr[0] == 8) {
					ctx_i->degamma_lut_ext = 1;
					lut = ctx_i->degamma_lut[DQE_BPC_TYPE_8];
				} else if (data_h->attr[0] == 10) {
					lut = ctx_i->degamma_lut[DQE_BPC_TYPE_10];
				} else {
					ctx_i->degamma_lut_ext = 0;
					lut = ctx_i->degamma_lut[DQE_BPC_TYPE_10];
				}
				len = ARRAY_SIZE(ctx_i->degamma_lut[0]);
				updated |= (1<<DQE_REG_DEGAMMA);
				break;
			case DQE_COLORMODE_ID_GAMMA:
				if (data_h->attr[0] == 8) {
					ctx_i->regamma_lut_ext = 1;
					lut = ctx_i->regamma_lut[DQE_BPC_TYPE_8];
				} else if (data_h->attr[0] == 10) {
					lut = ctx_i->regamma_lut[DQE_BPC_TYPE_10];
				} else {
					ctx_i->regamma_lut_ext = 0;
					lut = ctx_i->regamma_lut[DQE_BPC_TYPE_10];
				}
				len = ARRAY_SIZE(ctx_i->regamma_lut[0]);
				updated |= (1<<DQE_REG_REGAMMA);
				break;
			case DQE_COLORMODE_ID_CGC_DITHER:
				lut = ctx_i->cgc_dither;
				len = ARRAY_SIZE(ctx_i->cgc_dither);
				updated |= (1<<DQE_REG_CGC_DITHER);
				break;
			case DQE_COLORMODE_ID_GAMMA_MATRIX:
				lut = ctx_i->gamma_matrix;
				len = ARRAY_SIZE(ctx_i->gamma_matrix);
				updated |= (1<<DQE_REG_GAMMA_MATRIX);
				break;
			case DQE_COLORMODE_ID_HSC48_LCG:
				if (data_h->attr[0] >= ARRAY_SIZE(ctx_i->hsc48_lcg)) {
					dqe_err(dqe, "invalid HSC attr %d\n", data_h->attr[0]);
					goto error_locked;
				}

				lut = ctx_i->hsc48_lcg[data_h->attr[0]];
				len = ARRAY_SIZE(ctx_i->hsc48_lcg[0]);
				break;
			case DQE_COLORMODE_ID_HSC:
				lut = ctx_i->hsc48_lut;
				len = DQE_HSC_LUT_CTRL_MAX+DQE_HSC_LUT_POLY_MAX;
				updated |= (1<<DQE_REG_HSC);
				break;
			case DQE_COLORMODE_ID_SCL:
				lut = ctx_i->scl_input;
				len = ARRAY_SIZE(ctx_i->scl_input);
				updated |= (1<<DQE_REG_SCL);
				break;
			default:
				lut = NULL;
				dqe_debug(dqe, "undefined id %d but continued\n", data_h->id);
				break;
			}

			if (lut) {
				ret = dqe_conv_str2lut(data, lut, len, false);
				if (ret < 0) {
					dqe_err(dqe, "str2lut error id %d, len %d data %s\n", data_h->id, len, data);
					goto error_locked;
				}
				count++;
			}
		}
		mutex_unlock(&dqe->lock);

		size += data_h->total_size;
	}

	if (count_cgc && count_cgc != 3*17)
		dqe_warn(dqe, "invalid cgc17_enc count\n", count_cgc);
	if (hdr->num_data != (count+count_cgc))
		dqe_warn(dqe, "number of data %d vs %d mismatch\n", count+count_cgc, hdr->num_data);

	if (likely(updated)) {
		dqe_debug(dqe, "%s context updated (%x)\n", __func__, updated);
		DPU_EVENT_LOG(DPU_EVT_DQE_COLORMODE, dqe->decon->crtc, NULL);
	}
	updated = DQE_UPDATED_CONTEXT;
done:
	return updated;

error_locked:
	mutex_unlock(&dqe->lock);
error:
	dqe_err(dqe, "%s parsing error\n", __func__);
	return DQE_UPDATED_NONE;
}

static inline u32 dqe_get_mode_type(struct exynos_dqe *dqe,
					struct drm_crtc_state *crtc_state)
{
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);
	u32 mode_type, idx, attr_cm, attr_ri, color_mode, render_intent;
	u32 *attr;

	color_mode = exynos_crtc_state->color_mode;
	render_intent = exynos_crtc_state->render_intent;

	for (mode_type = DQE_MODE_PRESET; mode_type < dqe->num_lut; mode_type++) {
		attr = &dqe_lut[mode_type].mode_attr[0];
		for (idx = 0; idx < DQE_PRESET_ATTR_NUM; idx++) {
			if (attr[idx] == (u32)-1)
				continue;

			attr_cm = (attr[idx] >> DQE_PRESET_CM_SHIFT) & DQE_PRESET_ALL_MASK;
			attr_ri = (attr[idx] >> DQE_PRESET_RI_SHIFT) & DQE_PRESET_ALL_MASK;

			dqe_debug(dqe, "Preset[%d] attr (%x,%x) for (CM %d, RI %d)\n",
							mode_type, attr_cm, attr_ri, color_mode, render_intent);
			if (((attr_cm == DQE_PRESET_ALL_MASK) && (attr_ri == render_intent)) ||
				((attr_cm == color_mode) && (attr_ri == DQE_PRESET_ALL_MASK)) ||
				((attr_cm == color_mode) && (attr_ri == render_intent)))
				break;
		}

		if (idx != DQE_PRESET_ATTR_NUM)
			break;
	}

	if (mode_type == dqe->num_lut)
		mode_type = DQE_MODE_MAIN;
	else
		dqe_debug(dqe, "found preset[%d] for CM %d, RI %d\n",
					mode_type, color_mode, render_intent);
	return mode_type;
}

// update LUTs from preset LUT
static u32 dqe_update_preset(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	struct dqe_ctx_int *ctx_from;
	struct dqe_ctx_int *ctx_to;
	u32 updated = 0;
	u32 mode_type = dqe_get_mode_type(dqe, crtc_state);

	if (mode_type >= dqe->num_lut || mode_type == DQE_MODE_MAIN)
		return DQE_UPDATED_NONE;

	mutex_lock(&dqe->lock);
	ctx_from = &dqe_lut[mode_type];
	ctx_to = &dqe_lut[DQE_MODE_MAIN];
	if (ctx_from->disp_dither[0]) {
		memcpy(ctx_to->disp_dither, ctx_from->disp_dither, sizeof(ctx_from->disp_dither));
		updated |= (1<<DQE_REG_DISP_DITHER);
	}
	if (ctx_from->cgc_dither[0]) {
		memcpy(ctx_to->cgc_dither, ctx_from->cgc_dither, sizeof(ctx_from->cgc_dither));
		updated |= (1<<DQE_REG_CGC_DITHER);
	}
	if (ctx_from->gamma_matrix[0]) {
		memcpy(ctx_to->gamma_matrix, ctx_from->gamma_matrix, sizeof(ctx_from->gamma_matrix));
		updated |= (1<<DQE_REG_GAMMA_MATRIX);
	}
	if (ctx_from->degamma_lut[0][0]) {
		memcpy(ctx_to->degamma_lut, ctx_from->degamma_lut, sizeof(ctx_from->degamma_lut));
		ctx_to->degamma_lut_ext = ctx_from->degamma_lut_ext;
		updated |= (1<<DQE_REG_DEGAMMA);
	}
	if (ctx_from->regamma_lut[0][0]) {
		memcpy(ctx_to->regamma_lut, ctx_from->regamma_lut, sizeof(ctx_from->regamma_lut));
		ctx_to->regamma_lut_ext = ctx_from->regamma_lut_ext;
		updated |= (1<<DQE_REG_REGAMMA);
	}
	if (ctx_from->cgc17_con[0]) {
		memcpy(ctx_to->cgc17_con, ctx_from->cgc17_con, sizeof(ctx_from->cgc17_con));
		memcpy(ctx_to->cgc17_lut, ctx_from->cgc17_lut, sizeof(ctx_from->cgc17_lut));
		updated |= (1<<DQE_REG_CGC);
		dqe_reset_update_cnt(dqe, DQE_RESET_CGC, DQE_RESET_FULL);
	}
	if (ctx_from->hsc48_lut[0]) {
		memcpy(ctx_to->hsc48_lut, ctx_from->hsc48_lut, sizeof(ctx_from->hsc48_lut));
		memcpy(ctx_to->hsc48_lcg, ctx_from->hsc48_lcg, sizeof(ctx_from->hsc48_lcg));
		updated |= (1<<DQE_REG_HSC);
	}
	if (ctx_from->atc_lut[0]) {
		memcpy(ctx_to->atc_lut, ctx_from->atc_lut, sizeof(ctx_from->atc_lut));
		updated |= (1<<DQE_REG_ATC);
	}
	if (ctx_from->scl_input[0]) {
		memcpy(ctx_to->scl_input, ctx_from->scl_input, sizeof(ctx_from->scl_input));
		memcpy(ctx_to->scl_lut, ctx_from->scl_lut, sizeof(ctx_from->scl_lut));
		updated |= (1<<DQE_REG_SCL);
	}
	if (ctx_from->de_lut[0]) {
		memcpy(ctx_to->de_lut, ctx_from->de_lut, sizeof(ctx_from->de_lut));
		updated |= (1<<DQE_REG_DE);
	}
	mutex_unlock(&dqe->lock);

	if (unlikely(updated)) {
		dqe_debug(dqe, "%s context updated (%x)\n", __func__, updated);
		DPU_EVENT_LOG(DPU_EVT_DQE_PRESET, dqe->decon->crtc, NULL);
		updated = DQE_UPDATED_CONTEXT;
	}
	return updated;
}

static u32 dqe_update_context(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	u32 updated = DQE_UPDATED_NONE;

	updated |= dqe_update_colormode(dqe, crtc_state);
	if (unlikely(updated)) // copy sub LUTs to Main when colormode LUT changes
		updated |= dqe_update_preset(dqe, crtc_state);
	return updated;
}

static u32 dqe_update_config(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	u32 updated = DQE_UPDATED_NONE;
	u32 width, height;
	struct decon_device *decon = dqe->decon;
	struct decon_config *cfg = &decon->config;
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	if (decon->crtc->partial && exynos_crtc_state->partial) {
		width = drm_rect_width(&exynos_crtc_state->partial_region);
		height = drm_rect_height(&exynos_crtc_state->partial_region);
	} else {
		width = crtc_state->adjusted_mode.hdisplay;
		height = crtc_state->adjusted_mode.vdisplay;
	}

	if (unlikely(dqe->cfg.width != width) ||
		unlikely(dqe->cfg.height != height) ||
		unlikely(dqe->cfg.in_bpc != cfg->in_bpc) ||
		unlikely(dqe->cfg.out_bpc != cfg->out_bpc)) {

		dqe->cfg.width = width;
		dqe->cfg.height = height;
		dqe->cfg.in_bpc = cfg->in_bpc;
		dqe->cfg.out_bpc = cfg->out_bpc;
		dqe_debug(dqe, "%s: w x h (%d x %d), bpc (%d/%d)\n",
				__func__, dqe->cfg.width, dqe->cfg.height,
				dqe->cfg.in_bpc, dqe->cfg.out_bpc);
		DPU_EVENT_LOG(DPU_EVT_DQE_CONFIG, dqe->decon->crtc, NULL);
		updated = DQE_UPDATED_CONFIG;
	}

	return updated;
}

static inline void dqe_get_bpc_info(struct exynos_dqe *dqe,
					u32 *shift, u32 *idx, u32 isExt)
{
	if (isExt) { // if external LUT table exits, uses the separated LUT table.
		if (shift)
			*shift = 0;
		if (idx)
			*idx = (dqe->cfg.in_bpc == 8) ? DQE_BPC_TYPE_8 : DQE_BPC_TYPE_10;
	} else { // if no exist, down-scale of 10bit LUT table
		if (shift)
			*shift = (dqe->cfg.in_bpc == 8) ? 2 : 0; // 8bit: 0~1023, 10bit: 0~4095;
		if (idx)
			*idx = DQE_BPC_TYPE_10;
	}
}

static int dqe_remap_regs(struct exynos_dqe *dqe, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int i;
	u32 id = dqe->id;

	i = of_property_match_string(np, "reg-names", "dqe");
	if (i < 0) {
		dqe_err(dqe, "failed to find dqe regs\n");
		goto err;
	}

	dqe->regs.dqe_base_regs = of_iomap(np, i);
	if (!dqe->regs.dqe_base_regs) {
		dqe_err(dqe, "failed to remap dqe registers\n");
		goto err;
	}

	dqe_regs_desc_init(id, dqe->regs.dqe_base_regs, "dqe", REGS_DQE);

	i = of_property_match_string(np, "reg-names", "edma");
	if (i < 0) {
		dqe_warn(dqe, "failed to find edma regs but not an error\n");
		dqe->regs.edma_base_regs = NULL;
		return 0; // return 0 instead of err
	}

	dqe->regs.edma_base_regs = of_iomap(np, i);
	if (!dqe->regs.edma_base_regs) {
		dqe_err(dqe, "failed to remap edma registers\n");
		goto err;
	}

	dqe_regs_desc_init(id, dqe->regs.edma_base_regs, "edma", REGS_EDMA);

	return 0;

err:
	if (dqe->regs.dqe_base_regs)
		iounmap(dqe->regs.dqe_base_regs);
	if (dqe->regs.edma_base_regs)
		iounmap(dqe->regs.edma_base_regs);

	return -EINVAL;
}

static int dqe_parse_preset(struct exynos_dqe *dqe, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int i, j, offset = 0, size;
	u32 cnt;

	dqe->num_lut = 1; // 1 LUT required as default
	dqe_lut = NULL;

	if (of_property_read_u32(np, "dqe_preset_cnt", &cnt) || cnt == 0)
		goto done;

	if (!of_get_property(np, "dqe_preset", &size))
		goto done;

	if ((cnt * sizeof(u32) * DQE_PRESET_ATTR_NUM) != size) {
		dqe_err(dqe, "invalid dt size %d, preset_cnt %d\n", size, cnt);
		goto err;
	}

	dqe->num_lut += cnt; // add preset LUTs
	dqe_lut = kcalloc(dqe->num_lut, sizeof(*dqe_lut), GFP_KERNEL);
	if (!dqe_lut)
		goto err;

	for (i = DQE_MODE_PRESET; i < dqe->num_lut; i++) {
		for (j = 0; j < DQE_PRESET_ATTR_NUM; j++) {
			if (of_property_read_u32_index(np, "dqe_preset", offset++,
							&dqe_lut[i].mode_attr[j]))
				goto err;
			dqe_info(dqe, "Preset[%d] Attr[%d]: %x\n", i, j,
							dqe_lut[i].mode_attr[j]);
		}
	}

done:
	if (!dqe_lut)
		dqe_lut = kcalloc(dqe->num_lut, sizeof(*dqe_lut), GFP_KERNEL);
	dqe_info(dqe, "Allocated %d internal LUTs\n", dqe->num_lut);
	return 0;

err:
	return -EINVAL;
}

static int dqe_parse_others(struct exynos_dqe *dqe, struct device *dev)
{
	struct device_node *np = dev->of_node;

	dqe->sram_retention = of_property_read_bool(np, "sram-retention");
	dqe_info(dqe, "SRAM retention %d\n", dqe->sram_retention);
	return 0;
}

static void dqe_enable_irqs(struct exynos_dqe *dqe)
{
	if (dqe->regs.edma_base_regs) {
		edma_reg_clear_irq_all(dqe->id);
		edma_reg_set_irq_mask_all(dqe->id, 1);
		edma_reg_set_irq_enable(dqe->id, 1);

		if ((dqe->irq_edma) >= 0)
			enable_irq(dqe->irq_edma);
	}
}

static void dqe_disable_irqs(struct exynos_dqe *dqe)
{
	if (dqe->regs.edma_base_regs) {
		if ((dqe->irq_edma) >= 0)
			disable_irq(dqe->irq_edma);

		edma_reg_clear_irq_all(dqe->id);
		edma_reg_set_irq_enable(dqe->id, 0);
	}
}

static irqreturn_t dqe_irq_handler(int irq, void *dev_data)
{
	struct exynos_dqe *dqe = dev_data;
	u32 irqs;

	spin_lock(&dqe->slock);
	if (dqe->regs.edma_base_regs == NULL)
		goto irq_end;
	if (dqe->state != DQE_STATE_ENABLE)
		goto irq_end;

	irqs = edma_reg_get_interrupt_and_clear(dqe->id);
	dqe_debug(dqe, "%s: irq_sts_reg = %x\n", __func__, irqs);

irq_end:
	spin_unlock(&dqe->slock);
	return IRQ_HANDLED;
}

static int dqe_register_irqs(struct exynos_dqe *dqe, struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct platform_device *pdev;
	int ret = 0;

	pdev = container_of(dev, struct platform_device, dev);

	if (dqe->regs.edma_base_regs) {
		/* EDMA */
		dqe->irq_edma = of_irq_get_byname(np, "edma");
		dqe_info(dqe, "edma irq no = %lld\n", dqe->irq_edma);
		if ((dqe->irq_edma) < 0) {
			dqe_err(dqe, "failed to find EDMA irq\n");
			return -1;
		}

		ret = devm_request_irq(dev, dqe->irq_edma, dqe_irq_handler,
				0, pdev->name, dqe);
		if (ret) {
			dqe_err(dqe, "failed to install EDMA irq\n");
			return ret;
		}
		disable_irq(dqe->irq_edma);
	}
	return ret;
}

static int dqe_init_resources(struct exynos_dqe *dqe, struct device *dev)
{
	int i;
	int ret = 0;

	ret = dqe_remap_regs(dqe, dev);
	if (ret) {
		dqe_err(dqe, "failed to remap regs\n");
		goto err;
	}

	ret = dqe_register_irqs(dqe, dev);
	if (ret) {
		dqe_err(dqe, "failed to register irqs\n");
		goto err;
	}

	ret = dqe_parse_preset(dqe, dev);
	if (ret) {
		dqe_err(dqe, "failed to parse preset info\n");
		goto err;
	}

	ret = dqe_parse_others(dqe, dev);
	if (ret) {
		dqe_err(dqe, "failed to parse dt info\n");
		goto err;
	}

	dqe->cls = class_create(THIS_MODULE, "dqe");
	if (IS_ERR_OR_NULL(dqe->cls)) {
		dqe_err(dqe, "failed to create dqe class\n");
		goto err;
	}

	dqe->dev = device_create(dqe->cls, NULL, 0, &dqe, "dqe");
	if (IS_ERR_OR_NULL(dqe->dev)) {
		dqe_err(dqe, "failed to create dqe device\n");
		goto err;
	}

	for (i = 0; dqe_attrs[i] != NULL; i++) {
		ret = device_create_file(dqe->dev, dqe_attrs[i]);
		if (ret) {
			dqe_err(dqe, "failed to create file: %s\n",
				(*dqe_attrs[i]).attr.name);
			goto err;
		}
	}

	return ret;
err:
	if (dqe->cls)
		class_destroy(dqe->cls);
	return -1;
}

/*============== CONFIGURE LUTs ===================*/
static void dqe_dec_cgc17_dpcm(int *encoded, int *ret)
{
	int ii, idx;
	int offset, maxPos;
	int dpcm[16] = {0,};
	int pos = 0;
	int mode = (encoded[0] >> 30) & 0x3;
	int shift = 0;
	int min_pos = 0;

	for (ii = 0; ii < 17; ii++)
		ret[ii] = -1;

	if (mode == 0) {
		ret[0] = (encoded[0] >> 17) & 0x1fff;
		maxPos = encoded[0] & 0xf;
		ret[maxPos + 1] = (encoded[0] >> 4) & 0x1fff;

		for (ii = 1; ii < 5; ii++) {
			dpcm[pos++] = ((encoded[ii] >> 24) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 16) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 8) & 0xff);
			dpcm[pos++] = ((encoded[ii]) & 0xff);
		}

		offset = dpcm[maxPos];
		if (offset > 128)
			offset -= 256;

		for (idx = 1, pos = 0; idx < 17; idx++, pos++) {
			if (ret[idx] == -1)
				ret[idx] = ret[idx - 1] + dpcm[pos] + offset;
		}
	} else {
		shift = (mode == 2) ? 0 : 1;
		min_pos = (encoded[0] >> 12) & 0x1f;
		ret[min_pos] = (encoded[0] >> 17) & 0x1fff;

		for (ii = 1; ii < 5; ii++) {
			dpcm[pos++] = ((encoded[ii] >> 24) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 16) & 0xff);
			dpcm[pos++] = ((encoded[ii] >> 8) & 0xff);
			dpcm[pos++] = ((encoded[ii]) & 0xff);
		}

		pos = 0;
		for (ii = 0; ii < 17; ii++) {
			if (ii != min_pos)
				ret[ii] = ret[min_pos] + (dpcm[pos++] << shift);
		}
	}
}

static int dqe_dec_cgc17(struct dqe_ctx_int *lut, int mask, int wr_mode)
{
	int ii;
	int r_pos, g_pos, b_pos;
	int decoded[17] = {0,};
	int row = ARRAY_SIZE(lut->cgc17_encoded[0]);
	int col = ARRAY_SIZE(lut->cgc17_encoded[0][0]);
	int size = ARRAY_SIZE(decoded);

	if (mask < 0x1 || mask > 0x7) {
		pr_err("%s invalid mask %x\n", __func__, mask);
		return -EINVAL;
	}

	if (mask & 0x1) {
		for (g_pos = 0; g_pos < row; g_pos++) {
			for (r_pos = 0; r_pos < col; r_pos++) {
				dqe_dec_cgc17_dpcm((int *)lut->cgc17_encoded[0][g_pos][r_pos], decoded);

				if (!wr_mode) {
					dqe_conv_lut2str((u32 *)decoded, size, NULL);
				} else {
					for (ii = 0; ii < size; ii++)
						lut->cgc17_lut[0][r_pos * row * col + g_pos * row + ii] = (u32)decoded[ii];
				}
			}
		}
	}

	if (mask & 0x2) {
		for (r_pos = 0; r_pos < row; r_pos++) {
			for (g_pos = 0; g_pos < col; g_pos++) {
				dqe_dec_cgc17_dpcm((int *)lut->cgc17_encoded[1][r_pos][g_pos], decoded);

				if (!wr_mode) {
					dqe_conv_lut2str((u32 *)decoded, size, NULL);
				} else {
					for (ii = 0; ii < size; ii++)
						lut->cgc17_lut[1][r_pos * row * col + g_pos * row + ii] = (u32)decoded[ii];
				}
			}
		}
	}

	if (mask & 0x4) {
		for (g_pos = 0; g_pos < row; g_pos++) {
			for (b_pos = 0; b_pos < col; b_pos++) {
				dqe_dec_cgc17_dpcm((int *)lut->cgc17_encoded[2][g_pos][b_pos], decoded);

				if (!wr_mode) {
					dqe_conv_lut2str((u32 *)decoded, size, NULL);
				} else {
					for (ii = 0; ii < size; ii++)
						lut->cgc17_lut[2][ii * row * col + g_pos * row + b_pos] = (u32)decoded[ii];
				}
			}
		}
	}

	return 0;
}

static void dqe_set_cgc17_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	int i, rgb;

	dqe_debug(dqe, "%s\n", __func__);

	/* DQE0_CGC_LUT*/
	for (i = 0; i < DQE_CGC_LUT_MAX; i++)
		for (rgb = 0; rgb < 3; rgb++)
			dqe->ctx.cgc_lut[i][rgb] = (u16)lut->cgc17_lut[rgb][i]; // diff align between SFR and DMA

	/* DQE0_CGC_MC_R0*/
	dqe->ctx.cgc_con[0] = (
		CGC_MC_ON_R(lut->cgc17_con[1]) | CGC_MC_INVERSE_R(lut->cgc17_con[2]) |
		CGC_MC_GAIN_R(lut->cgc17_con[3])
	);

	/* DQE0_CGC_MC_R1*/
	dqe->ctx.cgc_con[1] = (
		CGC_MC_ON_R(lut->cgc17_con[4]) | CGC_MC_INVERSE_R(lut->cgc17_con[5]) |
		CGC_MC_GAIN_R(lut->cgc17_con[6])
	);

	/* DQE0_CGC_MC_R2*/
	dqe->ctx.cgc_con[2] = (
		CGC_MC_ON_R(lut->cgc17_con[7]) | CGC_MC_INVERSE_R(lut->cgc17_con[8]) |
		CGC_MC_GAIN_R(lut->cgc17_con[9])
	);

	dqe_ctrl_onoff(&dqe->ctx.cgc_on, lut->cgc17_con[0] ? DQE_CTRL_ON : DQE_CTRL_OFF, clrForced);
	if (lut->cgc17_con[0]) {
		dqe_ctrl_onoff(&dqe->ctx.degamma_on, DQE_CTRL_ON, clrForced);
		dqe_ctrl_onoff(&dqe->ctx.regamma_on, DQE_CTRL_ON, clrForced);
	}
}

static void dqe_set_disp_dither(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	u32 *disp_dither = lut->disp_dither;

	dqe_debug(dqe, "%s\n", __func__);

	/* DQE0_DISP_DITHER*/
	dqe->ctx.disp_dither = (
		DISP_DITHER_EN(disp_dither[0]) | DISP_DITHER_MODE(disp_dither[1]) |
		DISP_DITHER_FRAME_CON(disp_dither[2]) | DISP_DITHER_FRAME_OFFSET(disp_dither[3]) |
		DISP_DITHER_TABLE_SEL(disp_dither[4], 0) | DISP_DITHER_TABLE_SEL(disp_dither[5], 1) |
		DISP_DITHER_TABLE_SEL(disp_dither[6], 2)
	);

	dqe_ctrl_onoff(&dqe->ctx.disp_dither_on, disp_dither[0] ? DQE_CTRL_ON : DQE_CTRL_OFF, clrForced);
}

static void dqe_set_cgc_dither(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	u32 *cgc_dither = lut->cgc_dither;

	dqe_debug(dqe, "%s\n", __func__);

	/* DQE0_CGC_DITHER*/
#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0) // uses same XML format with EVT1 for just test purpose
	dqe->ctx.cgc_dither = (
		CGC_DITHER_EN(cgc_dither[0]) | CGC_DITHER_MODE(cgc_dither[1]) |
		CGC_DITHER_FRAME_CON(cgc_dither[2]) | CGC_DITHER_TABLE_SEL(cgc_dither[3], 0) |
		CGC_DITHER_TABLE_SEL(cgc_dither[4], 1) | CGC_DITHER_TABLE_SEL(cgc_dither[5], 2) |
		CGC_DITHER_FRAME_OFFSET(cgc_dither[7])
	);
#else
	dqe->ctx.cgc_dither = (
		CGC_DITHER_EN(cgc_dither[0]) | CGC_DITHER_MODE(cgc_dither[1]) |
		CGC_DITHER_FRAME_CON(cgc_dither[2]) | CGC_DITHER_TABLE_SEL(cgc_dither[3], 0) |
		CGC_DITHER_TABLE_SEL(cgc_dither[4], 1) | CGC_DITHER_TABLE_SEL(cgc_dither[5], 2) |
		CGC_DITHER_BIT(cgc_dither[6]) | CGC_DITHER_FRAME_OFFSET(cgc_dither[7])
	);
#endif

	dqe_ctrl_onoff(&dqe->ctx.cgc_dither_on, cgc_dither[0] ? DQE_CTRL_ON : DQE_CTRL_OFF, clrForced);
}

/*
 * If Gamma Matrix is transferred with this pattern,
 *
 * |A B C D|
 * |E F G H|
 * |I J K L|
 * |M N O P|
 *
 * Coeff and Offset will be applied as follows.
 *
 * |Rout|   |A E I||Rin|   |M|
 * |Gout| = |B F J||Gin| + |N|
 * |Bout|   |C G K||Bin|   |O|
 *
 */
static void dqe_set_gamma_matrix(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	int i;
	u32 gamma_mat_lut[17] = {0,};
	int *gamma_matrix = lut->gamma_matrix;
	int bound, tmp;
	u32 shift;

	dqe_debug(dqe, "%s\n", __func__);

	dqe_get_bpc_info(dqe, &shift, NULL, 0);

	gamma_mat_lut[0] = gamma_matrix[0];

	/* Coefficient */
	for (i = 0; i < 12; i++)
		gamma_mat_lut[i + 1] = (gamma_matrix[i + 1] >> 5);
	/* Offset */
	bound = 1023 >> shift; // 10bit: -1023~1023, 8bit: -255~255
	for (i = 12; i < 16; i++) {
		tmp = (gamma_matrix[i + 1] >> (6+shift));
		if (tmp > bound)
			tmp = bound;
		if (tmp < -bound)
			tmp = -bound;
		gamma_mat_lut[i + 1] = tmp;
	}
	dqe->ctx.gamma_matrix[0] = GAMMA_MATRIX_EN(gamma_mat_lut[0]);

	/* GAMMA_MATRIX_COEFF */
	dqe->ctx.gamma_matrix[1] = (
		GAMMA_MATRIX_COEFF_H(gamma_mat_lut[5]) | GAMMA_MATRIX_COEFF_L(gamma_mat_lut[1])
	);

	dqe->ctx.gamma_matrix[2] = (
		GAMMA_MATRIX_COEFF_H(gamma_mat_lut[2]) | GAMMA_MATRIX_COEFF_L(gamma_mat_lut[9])
	);

	dqe->ctx.gamma_matrix[3] = (
		GAMMA_MATRIX_COEFF_H(gamma_mat_lut[10]) | GAMMA_MATRIX_COEFF_L(gamma_mat_lut[6])
	);

	dqe->ctx.gamma_matrix[4] = (
		GAMMA_MATRIX_COEFF_H(gamma_mat_lut[7]) | GAMMA_MATRIX_COEFF_L(gamma_mat_lut[3])
	);

	dqe->ctx.gamma_matrix[5] = GAMMA_MATRIX_COEFF_L(gamma_mat_lut[11]);

	/* GAMMA_MATRIX_OFFSET */
	dqe->ctx.gamma_matrix[6] = (
		GAMMA_MATRIX_OFFSET_1(gamma_mat_lut[14]) | GAMMA_MATRIX_OFFSET_0(gamma_mat_lut[13])
	);

	dqe->ctx.gamma_matrix[7] = GAMMA_MATRIX_OFFSET_2(gamma_mat_lut[15]);

	dqe_ctrl_onoff(&dqe->ctx.gamma_matrix_on, gamma_matrix[0] ? DQE_CTRL_ON : DQE_CTRL_OFF, clrForced);
}

static void dqe_set_gamma_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	int i, idx;
	u32 bpc, shift;
	u32 lut_l, lut_h;
	u32 *luts;
	u32 *regamma_lut;

	dqe_get_bpc_info(dqe, &shift, &bpc, lut->regamma_lut_ext);
	regamma_lut = lut->regamma_lut[bpc];

	dqe->ctx.regamma_lut[0] = REGAMMA_EN(regamma_lut[0]);
	luts = &regamma_lut[1];

	for (i = 1, idx = 0; i < DQE_REGAMMA_REG_MAX; i++) {
		lut_h = 0;
		lut_l = REGAMMA_LUT_L(luts[idx]>>shift);
		idx++;

		if (((DQE_REGAMMA_LUT_CNT%2) == 0) || ((idx % DQE_REGAMMA_LUT_CNT) != 0)) {
			lut_h = REGAMMA_LUT_H(luts[idx]>>shift);
			idx++;
		}

		dqe->ctx.regamma_lut[i] = (lut_h | lut_l);
	}

	dqe_ctrl_onoff(&dqe->ctx.regamma_on, regamma_lut[0] ? DQE_CTRL_ON : DQE_CTRL_OFF, clrForced);
}

static void dqe_set_degamma_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	int i, idx;
	u32 bpc, shift;
	u32 lut_l, lut_h;
	u32 *luts;
	u32 *degamma_lut;

	dqe_get_bpc_info(dqe, &shift, &bpc, lut->degamma_lut_ext);
	degamma_lut = lut->degamma_lut[bpc];

	dqe->ctx.degamma_lut[0] = DEGAMMA_EN(degamma_lut[0]);
	luts = &degamma_lut[1];

	for (i = 1, idx = 0; i < DQE_DEGAMMA_REG_MAX; i++) {
		lut_h = 0;
		lut_l = DEGAMMA_LUT_L(luts[idx]>>shift);
		idx++;

		if (((DQE_DEGAMMA_LUT_CNT%2) == 0) || ((idx % DQE_DEGAMMA_LUT_CNT) != 0)) {
			lut_h = DEGAMMA_LUT_H(luts[idx]>>shift);
			idx++;
		}

		dqe->ctx.degamma_lut[i] = (lut_h | lut_l);
	}

	dqe_ctrl_onoff(&dqe->ctx.degamma_on, degamma_lut[0] ? DQE_CTRL_ON : DQE_CTRL_OFF, clrForced);
}

static_assert(DQE_HSC_REG_CTRL_MAX > 19 && DQE_HSC_LUT_CTRL_MAX > (56-0));
static_assert(DQE_HSC_REG_POLY_MAX > 9 && DQE_HSC_LUT_POLY_MAX > (74-57));
static void dqe_set_hsc48_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	int i, j, k;
	u32 shift;
	u32 *hsc48_lut = lut->hsc48_lut;

	dqe_get_bpc_info(dqe, &shift, NULL, 0);

	/* DQE0_HSC_CONTROL */
	dqe->ctx.hsc_con[0] = (
		 HSC_EN(hsc48_lut[0]) | HSC_PARTIAL_UPDATE_METHOD(hsc48_lut[1])
	);

	/* DQE0_HSC_LOCAL_CONTROL */
	dqe->ctx.hsc_con[1] = (
		HSC_LSC_ON(hsc48_lut[2]) | HSC_LSC_GROUPMODE(hsc48_lut[3]) |
		HSC_LHC_ON(hsc48_lut[4]) | HSC_LHC_GROUPMODE(hsc48_lut[5]) |
		HSC_LBC_ON(hsc48_lut[6]) | HSC_LBC_GROUPMODE(hsc48_lut[7])
	);

	/* DQE0_HSC_GLOBAL_CONTROL_1 */
	dqe->ctx.hsc_con[2] = (
		HSC_GHC_ON(hsc48_lut[8]) | HSC_GHC_GAIN(hsc48_lut[9]>>shift)
	);

	/* DQE0_HSC_GLOBAL_CONTROL_0 */
	dqe->ctx.hsc_con[3] = (
		HSC_GSC_ON(hsc48_lut[10]) | HSC_GSC_GAIN(hsc48_lut[11]>>shift)
	);

	/* DQE0_HSC_GLOBAL_CONTROL_2 */
	dqe->ctx.hsc_con[4] = (
		HSC_GBC_ON(hsc48_lut[12]) | HSC_GBC_GAIN(hsc48_lut[13]>>shift)
	);

	/* DQE0_HSC_CONTROL_ALPHA_SAT */
	dqe->ctx.hsc_con[5] = (
		HSC_ALPHA_SAT_ON(hsc48_lut[14]) | HSC_ALPHA_SAT_SCALE(hsc48_lut[15]) |
		HSC_ALPHA_SAT_SHIFT1(hsc48_lut[16]>>shift) | HSC_ALPHA_SAT_SHIFT2(hsc48_lut[17])
	);

	/* DQE0_HSC_CONTROL_ALPHA_BRI */
	dqe->ctx.hsc_con[6] = (
		HSC_ALPHA_BRI_ON(hsc48_lut[18]) | HSC_ALPHA_BRI_SCALE(hsc48_lut[19]) |
		HSC_ALPHA_BRI_SHIFT1(hsc48_lut[20]>>shift) | HSC_ALPHA_BRI_SHIFT2(hsc48_lut[21])
	);

	/* DQE0_HSC_CONTROL_MC1_R0 */
	dqe->ctx.hsc_con[7] = (
		HSC_MC_ON_R0(hsc48_lut[22]) | HSC_MC_BC_HUE_R0(hsc48_lut[23]) |
		HSC_MC_BC_SAT_R0(hsc48_lut[24]) | HSC_MC_SAT_GAIN_R0(hsc48_lut[25]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC2_R0 */
	dqe->ctx.hsc_con[8] = (
		HSC_MC_HUE_GAIN_R0(hsc48_lut[26]>>shift) | HSC_MC_BRI_GAIN_R0(hsc48_lut[27]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC3_R0 */
	dqe->ctx.hsc_con[9] = (
		HSC_MC_S1_R0(hsc48_lut[28]>>shift) | HSC_MC_S2_R0(hsc48_lut[29]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC4_R0 */
	dqe->ctx.hsc_con[10] = (
		HSC_MC_H1_R0(hsc48_lut[30]>>shift) | HSC_MC_H2_R0(hsc48_lut[31]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC1_R1 */
	dqe->ctx.hsc_con[11] = (
		HSC_MC_ON_R1(hsc48_lut[32]) | HSC_MC_BC_HUE_R1(hsc48_lut[33]) |
		HSC_MC_BC_SAT_R1(hsc48_lut[34]) | HSC_MC_SAT_GAIN_R1(hsc48_lut[35]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC2_R1 */
	dqe->ctx.hsc_con[12] = (
		HSC_MC_HUE_GAIN_R1(hsc48_lut[36]>>shift) | HSC_MC_BRI_GAIN_R1(hsc48_lut[37]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC3_R1 */
	dqe->ctx.hsc_con[13] = (
		HSC_MC_S1_R1(hsc48_lut[38]>>shift) | HSC_MC_S2_R1(hsc48_lut[39]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC4_R1 */
	dqe->ctx.hsc_con[14] = (
		HSC_MC_H1_R1(hsc48_lut[40]>>shift) | HSC_MC_H2_R1(hsc48_lut[41]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC1_R2 */
	dqe->ctx.hsc_con[15] = (
		HSC_MC_ON_R2(hsc48_lut[42]) | HSC_MC_BC_HUE_R2(hsc48_lut[43]) |
		HSC_MC_BC_SAT_R2(hsc48_lut[44]) | HSC_MC_SAT_GAIN_R2(hsc48_lut[45]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC2_R2 */
	dqe->ctx.hsc_con[16] = (
		HSC_MC_HUE_GAIN_R2(hsc48_lut[46]>>shift) | HSC_MC_BRI_GAIN_R2(hsc48_lut[47]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC3_R2 */
	dqe->ctx.hsc_con[17] = (
		HSC_MC_S1_R2(hsc48_lut[48]>>shift) | HSC_MC_S2_R2(hsc48_lut[49]>>shift)
	);

	/* DQE0_HSC_CONTROL_MC4_R2 */
	dqe->ctx.hsc_con[18] = (
		HSC_MC_H1_R2(hsc48_lut[50]>>shift) | HSC_MC_H2_R2(hsc48_lut[51]>>shift)
	);

	/* DQE0_HSC_CONTROL_YCOMP */
	dqe->ctx.hsc_con[19] = (
		HSC_YCOMP_ON(hsc48_lut[52]) | HSC_YCOMP_DITH_ON(hsc48_lut[53]) |
		HSC_BLEND_ON(hsc48_lut[54]) | HSC_YCOMP_GAIN(hsc48_lut[55]) |
		HSC_BLEND_MANUAL_GAIN(hsc48_lut[56])
	);

	/* DQE0_HSC_POLY_S0 -- DQE0_HSC_POLY_S4 */
	dqe->ctx.hsc_poly[0] = (HSC_POLY_CURVE_S_L(hsc48_lut[57]>>shift) | HSC_POLY_CURVE_S_H(hsc48_lut[58]>>shift));
	dqe->ctx.hsc_poly[1] = (HSC_POLY_CURVE_S_L(hsc48_lut[59]>>shift) | HSC_POLY_CURVE_S_H(hsc48_lut[60]>>shift));
	dqe->ctx.hsc_poly[2] = (HSC_POLY_CURVE_S_L(hsc48_lut[61]>>shift) | HSC_POLY_CURVE_S_H(hsc48_lut[62]>>shift));
	dqe->ctx.hsc_poly[3] = (HSC_POLY_CURVE_S_L(hsc48_lut[63]>>shift) | HSC_POLY_CURVE_S_H(hsc48_lut[64]>>shift));
	dqe->ctx.hsc_poly[4] = (HSC_POLY_CURVE_S_L(hsc48_lut[65]>>shift));

	/* DQE0_HSC_POLY_B0 -- DQE0_HSC_POLY_B4 */
	dqe->ctx.hsc_poly[5] = (HSC_POLY_CURVE_B_L(hsc48_lut[66]>>shift) | HSC_POLY_CURVE_B_H(hsc48_lut[67]>>shift));
	dqe->ctx.hsc_poly[6] = (HSC_POLY_CURVE_B_L(hsc48_lut[68]>>shift) | HSC_POLY_CURVE_B_H(hsc48_lut[69]>>shift));
	dqe->ctx.hsc_poly[7] = (HSC_POLY_CURVE_B_L(hsc48_lut[70]>>shift) | HSC_POLY_CURVE_B_H(hsc48_lut[71]>>shift));
	dqe->ctx.hsc_poly[8] = (HSC_POLY_CURVE_B_L(hsc48_lut[72]>>shift) | HSC_POLY_CURVE_B_H(hsc48_lut[73]>>shift));
	dqe->ctx.hsc_poly[9] = (HSC_POLY_CURVE_B_L(hsc48_lut[74]>>shift));

	k = (DQE_HSC_LUT_CTRL_MAX+DQE_HSC_LUT_POLY_MAX);
	for (i = 0; i < (int)ARRAY_SIZE(lut->hsc48_lcg); i++)
		for (j = 0; j < DQE_HSC_LUT_LSC_GAIN_MAX; j++)
			hsc48_lut[k++] = lut->hsc48_lcg[i][j];

	/* DQE0_HSC_LSC_GAIN_P1_0 -- DQE1_HSC_LBC_GAIN_P3_23 */
	for (i = 0, j = (DQE_HSC_LUT_CTRL_MAX+DQE_HSC_LUT_POLY_MAX); i < DQE_HSC_REG_GAIN_MAX; i++) {
		dqe->ctx.hsc_gain[i] = (
			HSC_LSC_GAIN_P1_L(hsc48_lut[j]>>shift) | HSC_LSC_GAIN_P1_H(hsc48_lut[j + 1]>>shift)
		);
		j += 2;
	}

	dqe_ctrl_onoff(&dqe->ctx.hsc_on, hsc48_lut[0] ? DQE_CTRL_ON : DQE_CTRL_OFF, clrForced);
}

struct dqe_aps_range {
	u32 max;
	u32 min;
};

enum dqe_aps_type {
	DQE_APS_TDR_MAX = 0,
	DQE_APS_TDR_MIN,
	DQE_APS_DSTEP,
	DQE_APS_MAX,
};

static inline u32 dqe_get_aps_range(struct exynos_dqe *dqe, u32 value,
				enum dqe_aps_type type)
{
	struct dqe_aps_range aps_range[DQE_APS_MAX][DQE_BPC_TYPE_MAX] = {
						{{1022, 824}, {254, 206}},
						{{581, 1}, {145, 1}},
						{{63, 1}, {15, 1}}};
	enum dqe_bpc_type bpc = (dqe->cfg.in_bpc == 8) ? DQE_BPC_TYPE_8 : DQE_BPC_TYPE_10;

	if (value == 0) // if input is zero, just return zero
		return 0;
	// if input is non-zero, check the range
	if (bpc == DQE_BPC_TYPE_8)
		bpc = DQE_BPC_TYPE_10; // TDR_MAX/MIN/DSTEP need to use same value for 8bit and 10bit
	if (value < aps_range[type][bpc].min)
		return aps_range[type][bpc].min;
	if (value > aps_range[type][bpc].max)
		return aps_range[type][bpc].max;
	return value;
}

static void dqe_set_aps_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	u32 *atc_lut = lut->atc_lut;
	u32 tdr_min, tdr_max, dstep, shift;

	dqe_get_bpc_info(dqe, &shift, NULL, 0);

	/* DQE0_ATC_CONTROL */
	dqe->ctx.atc[0] = (
		 DQE_ATC_EN(atc_lut[0]) | DQE_ATC_PARTIAL_UPDATE_METHOD(atc_lut[1]) |
		 DQE_ATC_PIXMAP_EN(atc_lut[2])
	);

	/* DQE0_ATC_GAIN */
	dqe->ctx.atc[1] = (
		ATC_LT(atc_lut[3]) | ATC_NS(atc_lut[4]) | ATC_ST(atc_lut[5])
	);

	/* DQE0_ATC_WEIGHT */
	dqe->ctx.atc[2] = (
		ATC_PL_W1(atc_lut[6]) | ATC_PL_W2(atc_lut[7]) |
		ATC_LA_W_ON(atc_lut[8]) | ATC_LA_W(atc_lut[9])
	);

	/* DQE0_ATC_CTMODE */
	dqe->ctx.atc[3] = (
		ATC_CTMODE(atc_lut[10])
	);

	/* DQE0_ATC_PPEN */
	dqe->ctx.atc[4] = (
		ATC_PP_EN(atc_lut[11])
	);

	/* DQE0_ATC_TDRMINMAX */
	tdr_min = dqe_get_aps_range(dqe, atc_lut[12], DQE_APS_TDR_MIN);
	tdr_max = dqe_get_aps_range(dqe, atc_lut[13], DQE_APS_TDR_MAX);
	dqe->ctx.atc[5] = (
		ATC_TDR_MIN(tdr_min) | ATC_TDR_MAX(tdr_max)
	);

	/* DQE0_ATC_AMBIENT_LIGHT */
	dqe->ctx.atc[6] = (
		ATC_AMBIENT_LIGHT(atc_lut[14])
	);

	/* DQE0_ATC_BACK_LIGHT */
	dqe->ctx.atc[7] = (
		ATC_BACK_LIGHT(atc_lut[15])
	);

	/* DQE0_ATC_DSTEP */
	dstep = dqe_get_aps_range(dqe, atc_lut[16], DQE_APS_DSTEP);
	dqe->ctx.atc[8] = (
		ATC_DSTEP(dstep)
	);

	/* DQE0_ATC_SCALE_MODE */
	dqe->ctx.atc[9] = (
		ATC_SCALE_MODE(atc_lut[17])
	);

	/* DQE0_ATC_THRESHOLD */
	dqe->ctx.atc[10] = (
		ATC_THRESHOLD_1(atc_lut[18]) | ATC_THRESHOLD_2(atc_lut[19]) |
		ATC_THRESHOLD_3(atc_lut[20])
	);

	/* DQE0_ATC_GAIN_LIMIT */
	dqe->ctx.atc[11] = (
		ATC_GAIN_LIMIT(atc_lut[21]>>shift) | ATC_LT_CALC_AB_SHIFT(atc_lut[22])
	);

	dqe_ctrl_onoff(&dqe->ctx.atc_on, atc_lut[0] ? DQE_CTRL_ON : DQE_CTRL_OFF, clrForced);
}

static void dqe_set_scl_lut(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, bool clrForced)
{
	int i, j, cnt, idx = 0;
	u32 *scl_lut = lut->scl_lut;
	u32 *scl_input = lut->scl_input;

	for (cnt = 0; cnt < DQE_SCL_COEF_CNT; cnt++) {
		for (j = 0; j < DQE_SCL_VCOEF_MAX; j++)
			for (i = 0; i < DQE_SCL_COEF_SET; i++)
				scl_lut[idx++] = ps_v_c_4t[0][i][j];

		for (j = 0; j < DQE_SCL_HCOEF_MAX; j++)
			for (i = 0; i < DQE_SCL_COEF_SET; i++)
				scl_lut[idx++] = ps_h_c_8t[0][i][j];
	}

	if (scl_input[0]) {
		for (cnt = 0, idx = 0; cnt < DQE_SCL_COEF_CNT; cnt++)
			for (j = 1; j <= DQE_SCL_COEF_MAX; j++, idx += DQE_SCL_COEF_SET)
				scl_lut[idx] = scl_input[j];
	}

	for (i = 0; i < DQE_SCL_REG_MAX; i++)
		dqe->ctx.scl[i] = SCL_COEF(scl_lut[i]) & SCL_COEF_MASK;

	dqe_ctrl_onoff(&dqe->ctx.scl_on, scl_input[0] ? DQE_CTRL_ON : DQE_CTRL_OFF, clrForced);
}

static void dqe_set_de_lut(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, bool clrForced)
{
	u32 *de_lut = lut->de_lut;
	/* DQE0_DE_CONTROL */
	dqe->ctx.de[0] = (
		 DE_EN(de_lut[0]) | DE_ROI_EN(de_lut[1]) | DE_ROI_IN(de_lut[2]) |
		 DE_SMOOTH_EN(de_lut[3]) | DE_FILTER_TYPE(de_lut[4]) | DE_BRATIO_SMOOTH(de_lut[5])
	);

	/* DQE0_DE_ROI_START */
	dqe->ctx.de[1] = (
		DE_ROI_START_X(de_lut[6]) | DE_ROI_START_Y(de_lut[7])
	);

	/* DQE0_DE_ROI_END */
	dqe->ctx.de[2] = (
		DE_ROI_END_X(de_lut[8]) | DE_ROI_END_Y(de_lut[9])
	);

	/* DQE0_DE_FLATNESS */
	dqe->ctx.de[3] = (
		DE_FLAT_EDGE_TH(de_lut[10]) | DE_EDGE_BALANCE_TH(de_lut[11])
	);

	/* DQE0_DE_CLIPPING */
	dqe->ctx.de[4] = (
		DE_PLUS_CLIP(de_lut[12]) | DE_MINUS_CLIP(de_lut[13])
	);

	/* DQE0_DE_GAIN */
	dqe->ctx.de[5] = (
		DE_GAIN(de_lut[14]) | DE_LUMA_DEGAIN_TH(de_lut[15]) | DE_FLAT_DEGAIN_FLAG(de_lut[16]) |
		DE_FLAT_DEGAIN_SHIFT(de_lut[17]) | DF_MAX_LUMINANCE(de_lut[18])
	);

	dqe_ctrl_onoff(&dqe->ctx.de_on, de_lut[0] ? DQE_CTRL_ON : DQE_CTRL_OFF, clrForced);
}

static void dqe_set_all_luts(struct exynos_dqe *dqe)
{
	struct dqe_ctx_int *lut;

	mutex_lock(&dqe->lock);
	lut = &dqe_lut[DQE_MODE_MAIN];
	dqe_set_cgc_dither(dqe, lut, false);
	dqe_set_gamma_matrix(dqe, lut, false);
	dqe_set_gamma_lut(dqe, lut, false);
	dqe_set_degamma_lut(dqe, lut, false);
	dqe_set_cgc17_lut(dqe, lut, false);
	dqe_set_hsc48_lut(dqe, lut, false);
	dqe_set_aps_lut(dqe, lut, false);
	dqe_set_scl_lut(dqe, lut, false);
	dqe_set_de_lut(dqe, lut, false);
	mutex_unlock(&dqe->lock);
}

static void dqe_save_lpd(struct exynos_dqe *dqe)
{
	int i;

	for (i = 0; i < DQE_LPD_REG_MAX; i++)
		dqe->ctx.lpd[i] = dqe_reg_get_lut(dqe->id, DQE_REG_LPD, i, 0);
}

static void dqe_restore_lpd(struct exynos_dqe *dqe)
{
	int i;
	u32 id = dqe->id;

	dqe_reg_set_lut_on(id, DQE_REG_LPD, 1);
	for (i = 0; i < DQE_LPD_REG_MAX; i++)
		dqe_reg_set_lut(id, DQE_REG_LPD, i, dqe->ctx.lpd[i], 0);
}
/*============== CONFIGURE LUTs END ===================*/

/*============== CREATE SYSFS ===================*/
#define DQE_CREATE_SYSFS_FUNC(name) \
	static ssize_t decon_dqe_ ## name ## _show(struct device *dev, \
				struct device_attribute *attr, char *buf) \
	{ \
		int ret = 0; \
		struct exynos_dqe *dqe = dev_get_drvdata(dev); \
		if (dqe == NULL || buf == NULL || dqe_lut == NULL) \
			return -1;\
		mutex_lock(&dqe->lock); \
		dqe_debug(dqe, "%s +\n", __func__); \
		ret = dqe_ ## name ## _show(dqe, &dqe_lut[mode_idx], buf); \
		if (ret < 0) \
			dqe_err(dqe, "%s err -\n", __func__); \
		else \
			dqe_debug(dqe, "%s -\n", __func__); \
		mutex_unlock(&dqe->lock); \
		return ret; \
	} \
	static ssize_t decon_dqe_ ## name ## _store(struct device *dev, \
		struct device_attribute *attr, const char *buffer, size_t count) \
	{ \
		int ret = 0; \
		struct exynos_dqe *dqe = dev_get_drvdata(dev); \
		if (dqe == NULL || count <= 0 || buffer == NULL || dqe_lut == NULL) \
			return -1; \
		mutex_lock(&dqe->lock); \
		dqe_debug(dqe, "%s +\n", __func__); \
		ret = dqe_ ## name ## _store(dqe, &dqe_lut[mode_idx], buffer, count); \
		if (ret < 0) \
			dqe_err(dqe, "%s err -\n", __func__); \
		else \
			dqe_debug(dqe, "%s -\n", __func__); \
		mutex_unlock(&dqe->lock); \
		return ret; \
	} \
	static DEVICE_ATTR(name, 0660, \
					decon_dqe_ ## name ## _show, \
					decon_dqe_ ## name ## _store)

#define DQE_CREATE_SYSFS_FUNC_SHOW(name) \
	static ssize_t decon_dqe_ ## name ## _show(struct device *dev, \
				struct device_attribute *attr, char *buf) \
	{ \
		int ret = 0; \
		struct exynos_dqe *dqe = dev_get_drvdata(dev); \
		if (dqe == NULL || buf == NULL || dqe_lut == NULL) \
			return -1;\
		mutex_lock(&dqe->lock); \
		dqe_debug(dqe, "%s +\n", __func__); \
		ret = dqe_ ## name ## _show(dqe, &dqe_lut[mode_idx], buf); \
		if (ret < 0) \
			dqe_err(dqe, "%s err -\n", __func__); \
		else \
			dqe_debug(dqe, "%s -\n", __func__); \
		mutex_unlock(&dqe->lock); \
		return ret; \
	} \
	static DEVICE_ATTR(name, 0440, \
					decon_dqe_ ## name ## _show, NULL)

static ssize_t dqe_conv_lut2str(u32 *lut, int size, char *out)
{
	int i;
	char str[10] = {0,};
	ssize_t ret = 0;

	dqe_str_result[0] = '\0';
	for (i = 0 ; i < size; i++) {
		snprintf(str, 9, "%d,", lut[i]);
		strcat(dqe_str_result, str);
	}
	dqe_str_result[strlen(dqe_str_result)-1] = '\0';

	if (out)
		ret = sprintf(out, "%s\n", dqe_str_result);
	else
		pr_info("%s %s\n", __func__, dqe_str_result);
	return ret;
}

static ssize_t dqe_conv_cgc2str(struct dqe_ctx_int *lut, char *out)
{
	int i, j;
	char str[10] = {0,};
	int rgb = lut->cgc17_enc_rgb;
	int idx = lut->cgc17_enc_idx;
	int row = ARRAY_SIZE(lut->cgc17_encoded[0][0]);
	int col = ARRAY_SIZE(lut->cgc17_encoded[0][0][0]);

	dqe_str_result[0] = '\0';
	for (i = 0 ; i < row; i++)
		for (j = 0; j < col; j++) {
			snprintf(str, 9, "%08x", lut->cgc17_encoded[rgb][idx][i][j]);
			strcat(dqe_str_result, str);
		}
	dqe_str_result[strlen(dqe_str_result)] = '\0';
	return snprintf(out, PAGE_SIZE, "%s\n", dqe_str_result);
}

static inline int dqe_conv_str2uint(char *head, u32 *buf)
{
	int ret = 0;
	int tmp;

	ret = kstrtou32(head, 0, buf);
	if (ret < 0) {
		if (ret == -ERANGE) {
			pr_err("%s kstrtou32 error (%d)\n", __func__, ret);
		} else {
			ret = kstrtos32(head, 0, &tmp);
			if (ret == 0)
				*buf = (u32)tmp;
			else
				pr_err("%s kstrtos32 error (%d)\n", __func__, ret);
		}
	}

	return ret;
}

static int dqe_conv_str2lut(const char *buffer, u32 *lut, int size, bool printHex)
{
	int idx, k;
	int ret = 0;
	char *ptr = NULL;
	char *head;

	head = (char *)buffer;
	if  (*head == 0) {
		pr_err("%s buffer is null.\n", __func__);
		return -1;
	}

	pr_debug("%s %s\n", __func__, head);

	for (idx = 0; idx < size-1; idx++) {
		ptr = strchr(head, ',');
		if (ptr == NULL) {
			pr_err("%s not found comma.(%d)\n", __func__, idx);
			ret = -EINVAL;
			goto done;
		}
		*ptr = 0;
		ret = dqe_conv_str2uint(head, &lut[idx]);
		if (ret < 0)
			goto done;
		head = ptr + 1;
	}

	k = 0;
	if (*(head + k) == '-')
		k++;
	while (*(head + k) >= '0' && *(head + k) <= '9')
		k++;
	*(head + k) = 0;

	ret = dqe_conv_str2uint(head, &lut[idx]);
	if (ret < 0)
		goto done;

	for (idx = 0; idx < size; idx++) {
		if (printHex)
			pr_debug("%d ", lut[idx]);
		else
			pr_debug("%04x ", lut[idx]);
	}
done:
	return ret;
}

static int dqe_conv_str2cgc(const char *buffer, struct dqe_ctx_int *lut)
{
	int i, j, ret;
	char str[41] = {0,};
	char hex[9] = {0,};
	char *head = NULL;
	char *ptr = NULL;
	unsigned long encoded;
	int row = ARRAY_SIZE(lut->cgc17_encoded[0][0]);
	int col = ARRAY_SIZE(lut->cgc17_encoded[0][0][0]);
	int rgb = lut->cgc17_enc_rgb;
	int idx = lut->cgc17_enc_idx;

	head = (char *)buffer;
	if  (*head == 0) {
		pr_err("%s buffer is null\n", __func__);
		return -1;
	}

	pr_debug("%s %s\n", __func__, head);
	if (strlen(head) < 680 || strlen(head) > 681) {
		pr_err("%s strlen error. (%d)\n", __func__, strlen(head));
		return -EINVAL;
	}

	if (rgb < 0 || rgb > 2 || idx < 0 || idx > 16) {
		pr_err("%s invalid rgb %d idx %d\n", __func__, rgb, idx);
		return -EINVAL;
	}

	for (i = 0 ; i < row; i++) {
		strncpy(str, head, 40);
		ptr = str;
		for (j = 0; j < col; j++) {
			strncpy(hex, ptr, 8);
			ret = kstrtoul(hex, 16, &encoded);
			if (ret < 0)
				return ret;
			lut->cgc17_encoded[rgb][idx][i][j] = (u32)encoded;
			ptr += 8;
		}

		head += 40;
	}

	return 0;
}

static ssize_t dqe_mode_idx_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", mode_idx);
}

static ssize_t dqe_mode_idx_store(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int val;

	ret = kstrtouint(buffer, 0, &val);
	if (ret < 0) {
		dqe_err(dqe, "Failed at kstrtouint function: %d\n", ret);
		return ret;
	}

	if (val >= dqe->num_lut) {
		dqe_err(dqe, "index range error: %d\n", val);
		return -EINVAL;
	}

	mode_idx = val;
	return count;
}

DQE_CREATE_SYSFS_FUNC(mode_idx);

static ssize_t dqe_disp_dither_show(struct exynos_dqe *dqe,
					struct dqe_ctx_int *lut, char *buf)
{
	return dqe_conv_lut2str(lut->disp_dither, ARRAY_SIZE(lut->disp_dither), buf);
}

static ssize_t dqe_disp_dither_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	ret = dqe_conv_str2lut(buffer, lut->disp_dither, ARRAY_SIZE(lut->disp_dither), true);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) // do not update dqe_ctx_reg for preset LUTs
		dqe_set_disp_dither(dqe, lut, true);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(disp_dither);

static ssize_t dqe_cgc_dither_show(struct exynos_dqe *dqe,
					struct dqe_ctx_int *lut, char *buf)
{
	return dqe_conv_lut2str(lut->cgc_dither, ARRAY_SIZE(lut->cgc_dither), buf);
}

static ssize_t dqe_cgc_dither_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	ret = dqe_conv_str2lut(buffer, lut->cgc_dither, ARRAY_SIZE(lut->cgc_dither), true);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) // do not update dqe_ctx_reg for preset LUTs
		dqe_set_cgc_dither(dqe, lut, true);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(cgc_dither);

static ssize_t dqe_cgc17_idx_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d\n", lut->cgc17_enc_rgb, lut->cgc17_enc_idx);
}

static ssize_t dqe_cgc17_idx_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret = 0;
	int val1, val2;

	ret = sscanf(buffer, "%d %d", &val1, &val2);
	if (ret != 2) {
		dqe_err(dqe, "Failed at sscanf function: %d\n", ret);
		ret = -EINVAL;
		return ret;
	}

	if (val1 < 0 || val1 > 2 || val2 < 0 || val2 > 16) {
		dqe_err(dqe, "index range error: %d %d\n", val1, val2);
		ret = -EINVAL;
		return ret;
	}

	lut->cgc17_enc_rgb = val1;
	lut->cgc17_enc_idx = val2;
	dqe_debug(dqe, "%s: %d %d\n", __func__, lut->cgc17_enc_rgb, lut->cgc17_enc_idx);
	return count;
}

DQE_CREATE_SYSFS_FUNC(cgc17_idx);

static ssize_t dqe_cgc17_enc_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return dqe_conv_cgc2str(lut, buf);
}

static ssize_t dqe_cgc17_enc_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	ret = dqe_conv_str2cgc(buffer, lut);
	if (ret < 0)
		return ret;
	return count;
}

DQE_CREATE_SYSFS_FUNC(cgc17_enc);

static ssize_t dqe_cgc17_dec_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	int ret;

	ret = dqe_dec_cgc17(lut, 0x7, false);
	return snprintf(buf, PAGE_SIZE, "dump=%d\n", ret);
}

static ssize_t dqe_cgc17_dec_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		return ret;

	ret = dqe_dec_cgc17(lut, value, true);
	if (ret < 0)
		return ret;
	return count;
}

DQE_CREATE_SYSFS_FUNC(cgc17_dec);

static ssize_t dqe_cgc17_con_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return dqe_conv_lut2str(lut->cgc17_con, ARRAY_SIZE(lut->cgc17_con), buf);
}

static ssize_t dqe_cgc17_con_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	ret = dqe_conv_str2lut(buffer, lut->cgc17_con, ARRAY_SIZE(lut->cgc17_con), true);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) { // do not update dqe_ctx_reg for preset LUTs
		dqe_set_cgc17_lut(dqe, lut, true);
		dqe_reset_update_cnt(dqe, DQE_RESET_CGC, DQE_RESET_FULL);
	}
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_FULL);
	return count;
}

DQE_CREATE_SYSFS_FUNC(cgc17_con);

static ssize_t dqe_gamma_matrix_show(struct exynos_dqe *dqe,
					struct dqe_ctx_int *lut, char *buf)
{
	return dqe_conv_lut2str(lut->gamma_matrix, ARRAY_SIZE(lut->gamma_matrix), buf);
}

static ssize_t dqe_gamma_matrix_store(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	ret = dqe_conv_str2lut(buffer, lut->gamma_matrix, ARRAY_SIZE(lut->gamma_matrix), false);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) // do not update dqe_ctx_reg for preset LUTs
		dqe_set_gamma_matrix(dqe, lut, true);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(gamma_matrix);

static ssize_t dqe_gamma_ext_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", lut->regamma_lut_ext);
}

static ssize_t dqe_gamma_ext_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret = 0;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0) {
		dqe_err(dqe, "Failed at kstrtouint function: %d\n", ret);
		return ret;
	}

	if (value > 1) {
		dqe_err(dqe, "index range error: %d\n", value);
		ret = -EINVAL;
		return ret;
	}

	lut->regamma_lut_ext = value;
	dqe_info(dqe, "%s regamma extension %d\n", __func__, lut->regamma_lut_ext);
	return count;
}

DQE_CREATE_SYSFS_FUNC(gamma_ext);

static ssize_t dqe_gamma_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	u32 bpc = (lut->regamma_lut_ext) ? DQE_BPC_TYPE_8 : DQE_BPC_TYPE_10;

	return dqe_conv_lut2str(lut->regamma_lut[bpc], ARRAY_SIZE(lut->regamma_lut[0]), buf);
}

static ssize_t dqe_gamma_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	u32 bpc = (lut->regamma_lut_ext) ? DQE_BPC_TYPE_8 : DQE_BPC_TYPE_10;

	ret = dqe_conv_str2lut(buffer, lut->regamma_lut[bpc], ARRAY_SIZE(lut->regamma_lut[0]), false);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) // do not update dqe_ctx_reg for preset LUTs
		dqe_set_gamma_lut(dqe, lut, true);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(gamma);

static ssize_t dqe_degamma_ext_show(struct exynos_dqe *dqe,
					struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", lut->degamma_lut_ext);
}

static ssize_t dqe_degamma_ext_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret = 0;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0) {
		dqe_err(dqe, "Failed at kstrtouint function: %d\n", ret);
		return ret;
	}

	if (value > 1) {
		dqe_err(dqe, "index range error: %d\n", value);
		ret = -EINVAL;
		return ret;
	}

	lut->degamma_lut_ext = value;
	dqe_info(dqe, "%s degamma extension %d\n", __func__, lut->degamma_lut_ext);
	return count;
}

DQE_CREATE_SYSFS_FUNC(degamma_ext);

static ssize_t dqe_degamma_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	u32 bpc = (lut->degamma_lut_ext) ? DQE_BPC_TYPE_8 : DQE_BPC_TYPE_10;

	return dqe_conv_lut2str(lut->degamma_lut[bpc], ARRAY_SIZE(lut->degamma_lut[0]), buf);
}

static ssize_t dqe_degamma_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	u32 bpc = (lut->degamma_lut_ext) ? DQE_BPC_TYPE_8 : DQE_BPC_TYPE_10;

	ret = dqe_conv_str2lut(buffer, lut->degamma_lut[bpc], ARRAY_SIZE(lut->degamma_lut[0]), false);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) // do not update dqe_ctx_reg for preset LUTs
		dqe_set_degamma_lut(dqe, lut, true);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(degamma);

static ssize_t dqe_hsc48_idx_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", lut->hsc48_lcg_idx);
}

static ssize_t dqe_hsc48_idx_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret = 0;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0) {
		dqe_err(dqe, "Failed at kstrtouint function: %d\n", ret);
		return ret;
	}

	if (value > 2) {
		dqe_err(dqe, "index range error: %d\n", value);
		ret = -EINVAL;
		return ret;
	}

	lut->hsc48_lcg_idx = value;
	dqe_info(dqe, "%s lcg idx %d\n", __func__, lut->hsc48_lcg_idx);
	return count;
}

DQE_CREATE_SYSFS_FUNC(hsc48_idx);

static ssize_t dqe_hsc48_lcg_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	int idx = lut->hsc48_lcg_idx;

	return dqe_conv_lut2str(lut->hsc48_lcg[idx], ARRAY_SIZE(lut->hsc48_lcg[0]), buf);
}

static ssize_t dqe_hsc48_lcg_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	int idx = lut->hsc48_lcg_idx;

	ret = dqe_conv_str2lut(buffer, lut->hsc48_lcg[idx], ARRAY_SIZE(lut->hsc48_lcg[0]), true);
	if (ret < 0)
		return ret;
	return count;
}

DQE_CREATE_SYSFS_FUNC(hsc48_lcg);

static ssize_t dqe_hsc_show(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, char *buf)
{
	return dqe_conv_lut2str(lut->hsc48_lut, DQE_HSC_LUT_CTRL_MAX+DQE_HSC_LUT_POLY_MAX, buf);
}

static ssize_t dqe_hsc_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	ret = dqe_conv_str2lut(buffer, lut->hsc48_lut, DQE_HSC_LUT_CTRL_MAX+DQE_HSC_LUT_POLY_MAX, true);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) // do not update dqe_ctx_reg for preset LUTs
		dqe_set_hsc48_lut(dqe, lut, true);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(hsc);

static ssize_t dqe_aps_show(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, char *buf)
{
	return dqe_conv_lut2str(lut->atc_lut, ARRAY_SIZE(lut->atc_lut), buf);
}

static ssize_t dqe_aps_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	ret = dqe_conv_str2lut(buffer, lut->atc_lut, ARRAY_SIZE(lut->atc_lut), false);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) // do not update dqe_ctx_reg for preset LUTs
		dqe_set_aps_lut(dqe, lut, true);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(aps);

static ssize_t dqe_aps_onoff_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "aps_onoff = %s\n",
					dqe_print_onoff(&dqe->ctx.atc_on));
}

static ssize_t dqe_aps_onoff_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		return ret;
	dqe_ctrl_forcedoff(&dqe->ctx.atc_on, (value == 0) ? 1 : 0);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(aps_onoff);

static ssize_t dqe_aps_lux_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "lux %d\n", dqe->ctx.atc_lux);
}

static ssize_t dqe_aps_lux_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		return ret;
	dqe->ctx.atc_lux = value;
	dqe_info(dqe, "%s: lux %d\n", __func__, dqe->ctx.atc_lux);
	return count;
}

DQE_CREATE_SYSFS_FUNC(aps_lux);

static ssize_t dqe_aps_dim_off_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "aps_dim_off=%d\n", dqe->ctx.atc_dim_off);
}

static ssize_t dqe_aps_dim_off_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		return ret;
	dqe->ctx.atc_dim_off = value;
	dqe_info(dqe, "%s: %d\n", __func__, value);
	return count;
}

DQE_CREATE_SYSFS_FUNC(aps_dim_off);

static ssize_t dqe_scl_show(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, char *buf)
{
	return dqe_conv_lut2str(lut->scl_input, ARRAY_SIZE(lut->scl_input), buf);
}

static ssize_t dqe_scl_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	ret = dqe_conv_str2lut(buffer, lut->scl_input, ARRAY_SIZE(lut->scl_input), true);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) // do not update dqe_ctx_reg for preset LUTs
		dqe_set_scl_lut(dqe, lut, true);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(scl);

static ssize_t dqe_de_show(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, char *buf)
{
	return dqe_conv_lut2str(lut->de_lut, ARRAY_SIZE(lut->de_lut), buf);
}

static ssize_t dqe_de_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;

	ret = dqe_conv_str2lut(buffer, lut->de_lut, ARRAY_SIZE(lut->de_lut), false);
	if (ret < 0)
		return ret;
	if (mode_idx == DQE_MODE_MAIN) // do not update dqe_ctx_reg for preset LUTs
		dqe_set_de_lut(dqe, lut, true);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_OPT);
	return count;
}

DQE_CREATE_SYSFS_FUNC(de);

static ssize_t dqe_color_mode_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	int val = 0;

	return snprintf(buf, PAGE_SIZE, "color_mode = %d\n", val);
}

static ssize_t dqe_color_mode_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	unsigned int value = 0;

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		return ret;
	dqe_info(dqe, "%s: color mode %d\n", __func__, value);
	return count;
}

DQE_CREATE_SYSFS_FUNC(color_mode);

enum dqe_off_ctrl {
	DQE_OFF_CTRL_ALL = 0,
	DQE_OFF_CTRL_CGC,
	DQE_OFF_CTRL_DEGAMMA,
	DQE_OFF_CTRL_REGAMMA,
	DQE_OFF_CTRL_GAMMA_MATRIX,
	DQE_OFF_CTRL_HSC,
	DQE_OFF_CTRL_ATC,
	DQE_OFF_CTRL_CGC_DITHER,
	DQE_OFF_CTRL_DISP_DITHER,
	DQE_OFF_CTRL_MAX
};

static const char *dqe_off_ctrl_name[DQE_OFF_CTRL_MAX] = {
	"ALL",
	"CGC",
	"DEGAMMA",
	"REGAMMA",
	"GAMMA_MATRIX",
	"HSC",
	"ATC",
	"CGC DITH",
	"DISP DITH",
};

static ssize_t dqe_off_ctrl_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	char str[24] = {0,};
	int i;
	u32 value;

	dqe_str_result[0] = '\0';
	for (i = 0 ; i < DQE_OFF_CTRL_MAX; i++) {
		switch (i) {
		case DQE_OFF_CTRL_CGC:
			value = dqe->ctx.cgc_on;
			break;
		case DQE_OFF_CTRL_DEGAMMA:
			value = dqe->ctx.degamma_on;
			break;
		case DQE_OFF_CTRL_REGAMMA:
			value = dqe->ctx.regamma_on;
			break;
		case DQE_OFF_CTRL_GAMMA_MATRIX:
			value = dqe->ctx.gamma_matrix_on;
			break;
		case DQE_OFF_CTRL_HSC:
			value = dqe->ctx.hsc_on;
			break;
		case DQE_OFF_CTRL_ATC:
			value = dqe->ctx.atc_on;
			break;
		case DQE_OFF_CTRL_CGC_DITHER:
			value = dqe->ctx.cgc_dither_on;
			break;
		case DQE_OFF_CTRL_DISP_DITHER:
			value = dqe->ctx.disp_dither_on;
			break;
		case DQE_OFF_CTRL_ALL:
		default:
			value = (u32)-1;
			break;
		}
		if (value == (u32)-1)
			snprintf(str, 24, "%s(%d),", dqe_off_ctrl_name[i], i);
		else
			snprintf(str, 24, "%s(%d):%s,", dqe_off_ctrl_name[i], i, dqe_print_onoff(&value));
		strcat(dqe_str_result, str);
	}
	dqe_str_result[strlen(dqe_str_result)-1] = '\0';
	return snprintf(buf, PAGE_SIZE, "%s\n", dqe_str_result);
}

static ssize_t dqe_off_ctrl_store(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, const char *buffer, size_t count)
{
	int ret;
	int type, forced = 0;

	ret = sscanf(buffer, "%d %d", &type, &forced);
	if (ret < 1) {
		dqe_err(dqe, "Failed at sscanf function: %d\n", ret);
		ret = -EINVAL;
		return ret;
	}

	if (ret == 1)
		forced = 1; // if arg is 1, set forced off

	switch (type) {
	case DQE_OFF_CTRL_ALL:
		dqe_ctrl_forcedoff(&dqe->ctx.gamma_matrix_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.degamma_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.cgc_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.regamma_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.hsc_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.atc_on, forced);
		dqe_ctrl_forcedoff(&dqe->ctx.cgc_dither_on, forced);
		break;
	case DQE_OFF_CTRL_CGC:
		dqe_ctrl_forcedoff(&dqe->ctx.cgc_on, forced);
		break;
	case DQE_OFF_CTRL_DEGAMMA:
		dqe_ctrl_forcedoff(&dqe->ctx.degamma_on, forced);
		break;
	case DQE_OFF_CTRL_REGAMMA:
		dqe_ctrl_forcedoff(&dqe->ctx.regamma_on, forced);
		break;
	case DQE_OFF_CTRL_GAMMA_MATRIX:
		dqe_ctrl_forcedoff(&dqe->ctx.gamma_matrix_on, forced);
		break;
	case DQE_OFF_CTRL_HSC:
		dqe_ctrl_forcedoff(&dqe->ctx.hsc_on, forced);
		break;
	case DQE_OFF_CTRL_ATC:
		dqe_ctrl_forcedoff(&dqe->ctx.atc_on, forced);
		break;
	case DQE_OFF_CTRL_CGC_DITHER:
		dqe_ctrl_forcedoff(&dqe->ctx.cgc_dither_on, forced);
		break;
	case DQE_OFF_CTRL_DISP_DITHER:
		dqe_ctrl_forcedoff(&dqe->ctx.disp_dither_on, forced);
		break;
	default:
		break;
	}

	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_FULL);
	return count;
}

DQE_CREATE_SYSFS_FUNC(off_ctrl);

static ssize_t dqe_xml_show(struct exynos_dqe *dqe,
			struct dqe_ctx_int *lut, char *buf)
{
	if (strlen(dqe->xml_suffix) <= 0)
		return 0;
	sprintf(dqe_str_result, "%s.xml", dqe->xml_suffix);
	pr_info("%s xml_path: %s\n", __func__, dqe_str_result);

	return sprintf(buf, "%s\n", dqe_str_result);
}

DQE_CREATE_SYSFS_FUNC_SHOW(xml);

static ssize_t dqe_dqe_ver_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%08X\n", dqe->ctx.version);
}

DQE_CREATE_SYSFS_FUNC_SHOW(dqe_ver);

static ssize_t dqe_dim_status_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	const struct decon_device *decon = dqe->decon;

	return snprintf(buf, PAGE_SIZE, "%d\n", decon->dimming);
}

DQE_CREATE_SYSFS_FUNC_SHOW(dim_status);

static ssize_t dqe_dump_show(struct exynos_dqe *dqe,
				struct dqe_ctx_int *lut, char *buf)
{
	int val = 0;
	struct decon_device *decon = dqe->decon;

	hibernation_block_exit(decon->crtc->hibernation);
	__dqe_dump(dqe->id, &dqe->regs, true);
	hibernation_unblock(decon->crtc->hibernation);
	return snprintf(buf, PAGE_SIZE, "dump = %d\n", val);
}

DQE_CREATE_SYSFS_FUNC_SHOW(dump);

static struct device_attribute *dqe_attrs[] = {
	&dev_attr_mode_idx,
	&dev_attr_disp_dither,
	&dev_attr_cgc_dither,
	&dev_attr_cgc17_idx,
	&dev_attr_cgc17_enc,
	&dev_attr_cgc17_dec,
	&dev_attr_cgc17_con,
	&dev_attr_gamma_matrix,
	&dev_attr_gamma_ext,
	&dev_attr_gamma,
	&dev_attr_degamma_ext,
	&dev_attr_degamma,
	&dev_attr_hsc48_idx,
	&dev_attr_hsc48_lcg,
	&dev_attr_hsc,
	&dev_attr_aps,
	&dev_attr_aps_onoff,
	&dev_attr_aps_lux,
	&dev_attr_aps_dim_off,
	&dev_attr_scl,
	&dev_attr_de,
	&dev_attr_color_mode,
	&dev_attr_off_ctrl,
	&dev_attr_xml,
	&dev_attr_dqe_ver,
	&dev_attr_dim_status,
	&dev_attr_dump,
	NULL,
};
/*============== CREATE SYSFS END ===================*/
static void __exynos_dqe_dump(struct exynos_dqe *dqe)
{
	__dqe_dump(dqe->id, &dqe->regs, false);
}

static void __exynos_dqe_prepare(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	dqe_debug(dqe, "%s +\n", __func__);
	dqe_prepare_context(dqe, crtc_state);
	dqe_debug(dqe, "%s -\n", __func__);
}

static void __exynos_dqe_update(struct exynos_dqe *dqe,
				struct drm_crtc_state *crtc_state)
{
	u32 updated = DQE_UPDATED_NONE;

	dqe_debug(dqe, "%s +\n", __func__);

	updated |= dqe_update_context(dqe, crtc_state);
	updated |= dqe_update_config(dqe, crtc_state);

	if (unlikely(updated)) {
		dqe_set_all_luts(dqe);
		dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_FULL);
	}

	if (IS_DQE_UPDATE_REQUIRED(dqe)) {
		dqe_restore_context(dqe);
		decon_reg_update_req_dqe(dqe->id);
		decon_reg_update_req_dqe_cgc(dqe->id);
	}

	dqe_debug(dqe, "%s -\n", __func__);
}

static void __exynos_dqe_enable(struct exynos_dqe *dqe)
{
	dqe_debug(dqe, "%s +\n", __func__);
	dqe_restore_lpd(dqe);
	dqe_reset_update_cnt(dqe, DQE_RESET_GLOBAL, DQE_RESET_FULL);
	dqe_reset_update_cnt(dqe, DQE_RESET_CGC, DQE_RESET_OPT);
	dqe_enable_irqs(dqe);
	dqe->state = DQE_STATE_ENABLE;
	dqe_debug(dqe, "%s -\n", __func__);
}

static void __exynos_dqe_disable(struct exynos_dqe *dqe)
{
	dqe_debug(dqe, "%s +\n", __func__);
	dqe_save_lpd(dqe);
	dqe_disable_irqs(dqe);
	dqe->state = DQE_STATE_DISABLE;
	dqe_debug(dqe, "%s -\n", __func__);
}

static void __exynos_dqe_suspend(struct exynos_dqe *dqe)
{
	dqe_debug(dqe, "%s +\n", __func__);
	dqe_debug(dqe, "%s -\n", __func__);
}

static void __exynos_dqe_resume(struct exynos_dqe *dqe)
{
	dqe_debug(dqe, "%s +\n", __func__);
	// SRAM retention reset on sleep mif down
	dqe_reset_update_cnt(dqe, DQE_RESET_CGC, DQE_RESET_FULL);
	dqe_debug(dqe, "%s -\n", __func__);
}

static const struct exynos_dqe_funcs dqe_funcs = {
	.dump = __exynos_dqe_dump,
	.prepare = __exynos_dqe_prepare,
	.update = __exynos_dqe_update,
	.enable = __exynos_dqe_enable,
	.disable = __exynos_dqe_disable,
	.suspend = __exynos_dqe_suspend,
	.resume = __exynos_dqe_resume,
};

void exynos_dqe_dump(struct exynos_dqe *dqe)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->dump(dqe);
}

void exynos_dqe_prepare(struct exynos_dqe *dqe,
			struct drm_crtc_state *crtc_state)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false || !crtc_state)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->prepare(dqe, crtc_state);
}

void exynos_dqe_update(struct exynos_dqe *dqe,
			struct drm_crtc_state *crtc_state)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false || !crtc_state)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->update(dqe, crtc_state);
}

void exynos_dqe_enable(struct exynos_dqe *dqe)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->enable(dqe);
}

void exynos_dqe_disable(struct exynos_dqe *dqe)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->disable(dqe);
}

void exynos_dqe_suspend(struct exynos_dqe *dqe)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->suspend(dqe);
}

void exynos_dqe_resume(struct exynos_dqe *dqe)
{
	const struct exynos_dqe_funcs *funcs;

	if (!dqe || dqe->initialized == false)
		return;

	funcs = dqe->funcs;
	if (funcs)
		funcs->resume(dqe);
}

struct exynos_dqe *exynos_dqe_register(struct decon_device *decon)
{
	struct exynos_dqe *dqe;
	int ret;

	if (!decon) {
		pr_err("%s decon is NULL\n", __func__);
		return NULL;
	}

	if (decon->id > 0 || !(decon->config.out_type & DECON_OUT_DSI)) {
		pr_warn("%s decon%d no need dqe interface\n", __func__, decon->id);
		return NULL;
	}

	dqe = devm_kzalloc(decon->dev, sizeof(struct exynos_dqe), GFP_KERNEL);
	if (!dqe)
		return NULL;

	dqe->decon = decon;
	dqe->id = decon->id;

	ret = dqe_init_resources(dqe, decon->dev);
	if (ret) {
		dqe_err(dqe, "failed to init resources\n");
		return NULL;
	}

	spin_lock_init(&dqe->slock);
	mutex_init(&dqe->lock);
	atomic_set(&dqe->update_cnt, 0);
	atomic_set(&dqe->cgc_update_cnt, 0);
	dev_set_drvdata(dqe->dev, dqe);

	memset(&dqe->cfg, 0, sizeof(dqe->cfg));
	dqe->xml_suffix[0] = '\0';
	dqe_init_context(dqe, decon->dev);

	dqe->funcs = &dqe_funcs;
	dqe->state = DQE_STATE_DISABLE;
	dqe->initialized = true;

	dqe_info(dqe, "Display Quality Enhancer is supported\n");

	return dqe;
}
