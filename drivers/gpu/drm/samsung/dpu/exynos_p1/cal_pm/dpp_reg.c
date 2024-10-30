// SPDX-License-Identifier: GPL-2.0-only
/*
 * dpu30/cal_9925/dpp_regs.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Jaehoe Yang <jaehoe.yang@samsung.com>
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * Register access functions for Samsung EXYNOS Display Pre-Processor driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/iopoll.h>

#include <exynos_dpp_coef.h>
#include <exynos_hdr_lut.h>
#include <exynos_drm_format.h>
#include <exynos_drm_dpp.h>
#include <cal_config.h>
#include <dpp_cal.h>
#include <regs-dpp.h>

#define DPP_SC_RATIO_MAX	((1 << 20) * 8 / 8)
#define DPP_SC_RATIO_7_8	((1 << 20) * 8 / 7)
#define DPP_SC_RATIO_6_8	((1 << 20) * 8 / 6)
#define DPP_SC_RATIO_5_8	((1 << 20) * 8 / 5)
#define DPP_SC_RATIO_4_8	((1 << 20) * 8 / 4)
#define DPP_SC_RATIO_3_8	((1 << 20) * 8 / 3)

static struct cal_regs_desc regs_dpp[REGS_DPP_TYPE_MAX][REGS_DPP_ID_MAX];
static struct dpp_dpuf_req_rsc cur_req_rsc[MAX_DPUF_CNT];

#define dpp_regs_desc(id)			(&regs_dpp[REGS_DPP][id])
#define dpp_read(id, offset)			\
	cal_read(dpp_regs_desc(id), offset)
#define dpp_write(id, offset, val)		\
	cal_write(dpp_regs_desc(id), offset, val)
#define dpp_read_mask(id, offset, mask)	\
	cal_read_mask(dpp_regs_desc(id), offset, mask)
#define dpp_write_mask(id, offset, val, mask)	\
	cal_write_mask(dpp_regs_desc(id), offset, val, mask)

#define dma_regs_desc(id)			(&regs_dpp[REGS_DMA][id])
#define dma_read(id, offset)			\
	cal_read(dma_regs_desc(id), offset)
#define dma_write(id, offset, val)		\
	cal_write(dma_regs_desc(id), offset, val)
#define dma_read_mask(id, offset, mask)	\
	cal_read_mask(dma_regs_desc(id), offset, mask)
#define dma_write_mask(id, offset, val, mask)	\
	cal_write_mask(dma_regs_desc(id), offset, val, mask)

/* SRAMCON_Lx */
#define srcl_regs_desc(id)			(&regs_dpp[REGS_SRAMC][id])
#define srcl_read(id, offset)			\
	cal_read(srcl_regs_desc(id), offset)
#define srcl_write(id, offset, val)		\
	cal_write(srcl_regs_desc(id), offset, val)
#define srcl_read_mask(id, offset, mask)	\
	cal_read_mask(srcl_regs_desc(id), offset, mask)
#define srcl_write_mask(id, offset, val, mask)	\
	cal_write_mask(srcl_regs_desc(id), offset, val, mask)

#define votf_regs_desc(id)			(&regs_dpp[REGS_VOTF][id])
#define votf_read(id, offset)			\
	cal_read(votf_regs_desc(id), offset)
#define votf_write(id, offset, val)		\
	cal_write(votf_regs_desc(id), offset, val)
#define votf_read_mask(id, offset, mask)	\
	cal_read_mask(votf_regs_desc(id), offset, mask)
#define votf_write_mask(id, offset, val, mask)	\
	cal_write_mask(votf_regs_desc(id), offset, val, mask)

/* SCL_COEF */
#define coef_regs_desc(id)			(&regs_dpp[REGS_SCL_COEF][id])
#define coef_read(id, offset)			\
	cal_read(coef_regs_desc(id), offset)
#define coef_write(id, offset, val)		\
	cal_write(coef_regs_desc(id), offset, val)

/* HDR_COMMON */
#define hdr_comm_regs_desc(id)			(&regs_dpp[REGS_HDR_COMM][id])
#define hdr_comm_read(id, offset)			\
	cal_read(hdr_comm_regs_desc(id), offset)
#define hdr_comm_write(id, offset, val)		\
	cal_write(hdr_comm_regs_desc(id), offset, val)
#define hdr_comm_read_mask(id, offset, mask)	\
	cal_read_mask(hdr_comm_regs_desc(id), offset, mask)
#define hdr_comm_write_mask(id, offset, val, mask)	\
	cal_write_mask(hdr_comm_regs_desc(id), offset, val, mask)

#define DPP_IDLE_WAIT_TIMEOUT	(50 * 1000) /* 50ms */

void dpp_regs_desc_init(void __iomem *regs, const char *name,
		enum dpp_regs_type type, unsigned int id)
{
	cal_regs_desc_check(type, id, REGS_DPP_TYPE_MAX, REGS_DPP_ID_MAX);
	cal_regs_desc_set(regs_dpp, regs, name, type, id);
}

/****************** SRAMCON_Layer CAL functions ******************/
static void sramc_reg_set_pslave_err(u32 id, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	mask = SRAMC_PSLVERR_EN;
	srcl_write_mask(id, SRAMC_L_COM_PSLVERR_CON, val, mask);
}

static void sramc_reg_set_mode_enable(u32 id, u32 mode)
{
	srcl_write(id, SRAMC_L_COM_MODE_REG, mode);
}

static void sramc_reg_set_dst_hpos(u32 id, u32 top, u32 bottom)
{
	srcl_write(id, SRAMC_L_COM_DST_POSITION_REG,
			SRAMC_DST_BOTTOM(bottom) | SRAMC_DST_TOP(top));
}

static void sramc_reg_set_rsc_config(u32 id, struct dpp_params_info *p)
{
	u32 format = 0;
	bool scl_en = false, scl_alpha_en = false;
	bool comp_en = false, rot_en = false;
	u32 mode;
	const struct dpu_fmt *fmt = dpu_find_fmt_info(p->format);

	sramc_reg_set_dst_hpos(id, p->dst.y, (p->dst.y + p->dst.h - 1));

	if (fmt->cs == DPU_COLORSPACE_RGB) {
		if (fmt->bpc == 8 || fmt->bpc == 10)
			format = SRAMC_FMT_RGB32BIT;
		else
			format = SRAMC_FMT_RGB16BIT;
	} else {
		if (fmt->bpc == 10)
			format = SRAMC_FMT_YUV10BIT;
		else
			format = SRAMC_FMT_YUV08BIT;
	}

	/* check scaling */
	if (p->is_scale)
		scl_en = true;
	if (scl_en) {
		if (fmt->cs == DPU_COLORSPACE_RGB && fmt->len_alpha > 0)
			scl_alpha_en = true;
	}

	/* check rotation */
	if (p->rot >= DPP_ROT_90)
		rot_en = true;

	/* check buffer compression */
	if (p->comp_type != COMP_TYPE_NONE)
		comp_en = true;

	mode = SRAMC_SCL_ALPHA_ENABLE(scl_alpha_en) |
		SRAMC_SCL_ENABLE(scl_en) | SRAMC_COMP_ENABLE(comp_en) |
		SRAMC_ROT_ENABLE(rot_en) | SRAMC_FORMAT(format);
	sramc_reg_set_mode_enable(id, mode);
}


/****************** IDMA CAL functions ******************/
static void idma_reg_set_irq_mask_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, RDMA_IRQ, val, IDMA_ALL_IRQ_MASK);
}

void idma_reg_set_irq_enable(u32 id)
{
	dma_write_mask(id, RDMA_IRQ, ~0, IDMA_IRQ_ENABLE);
}

void idma_reg_set_irq_disable(u32 id)
{
	dma_write_mask(id, RDMA_IRQ, 0, IDMA_IRQ_ENABLE);
}

static void idma_reg_set_in_qos_lut(u32 id, u32 lut_id, u32 qos_t)
{
	u32 reg_id;

	if (lut_id == 0)
		reg_id = RDMA_QOS_LUT_LOW;
	else
		reg_id = RDMA_QOS_LUT_HIGH;
	dma_write(id, reg_id, qos_t);
}

static void idma_reg_set_sram_clk_gate_en(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, RDMA_DYNAMIC_GATING_EN, val, IDMA_SRAM_CG_EN);
}

static void idma_reg_set_dynamic_gating_en_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, RDMA_DYNAMIC_GATING_EN, val, IDMA_DG_EN_ALL);
}

static void idma_reg_clear_irq(u32 id, u32 irq)
{
	dma_write_mask(id, RDMA_IRQ, ~0, irq);
}

static void idma_reg_set_sw_reset(u32 id)
{
	dma_write_mask(id, RDMA_ENABLE, ~0, IDMA_SRESET);
}

static void idma_reg_set_assigned_mo(u32 id, u32 mo)
{
	dma_write_mask(id, RDMA_ENABLE, IDMA_ASSIGNED_MO(mo),
			IDMA_ASSIGNED_MO_MASK);
}

static int idma_reg_wait_sw_reset_status(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dma_regs_desc(id)->regs + RDMA_ENABLE,
			val, !(val & IDMA_SRESET), 10, 2000); /* timeout 2ms */
	if (ret) {
		cal_log_err(id, "[idma] timeout sw-reset\n");
		return ret;
	}

	return 0;
}

static int idma_reg_wait_idle_status(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dma_regs_desc(id)->regs + RDMA_ENABLE,
			val, !(val & IDMA_OP_STATUS), 10, DPP_IDLE_WAIT_TIMEOUT);
	if (ret) {
		cal_log_err(id, "[idma] timeout op-status-idle\n");
		return ret;
	}

	return 0;
}

static void idma_reg_set_coordinates(u32 id, struct decon_frame *src)
{
	dma_write(id, RDMA_SRC_OFFSET,
			IDMA_SRC_OFFSET_Y(src->y) | IDMA_SRC_OFFSET_X(src->x));
	dma_write(id, RDMA_SRC_WIDTH, IDMA_SRC_WIDTH(src->f_w));
	dma_write(id, RDMA_SRC_HEIGHT, IDMA_SRC_HEIGHT(src->f_h));
	dma_write(id, RDMA_IMG_SIZE,
			IDMA_IMG_HEIGHT(src->h) | IDMA_IMG_WIDTH(src->w));
}

static void idma_reg_set_frame_alpha(u32 id, u32 alpha)
{
	dma_write_mask(id, RDMA_IN_CTRL_0, IDMA_ALPHA(alpha),
			IDMA_ALPHA_MASK);
}

static void idma_reg_set_ic_max(u32 id, u32 ic_max)
{
	dma_write_mask(id, RDMA_IN_CTRL_0, IDMA_IC_MAX(ic_max),
			IDMA_IC_MAX_MASK);
}

static void idma_reg_set_rotation(u32 id, u32 rot)
{
	dma_write_mask(id, RDMA_IN_CTRL_0, IDMA_ROT(rot), IDMA_ROT_MASK);
}

static void idma_reg_set_block_mode(u32 id, bool en, int x, int y, u32 w, u32 h)
{
	if (!en) {
		dma_write_mask(id, RDMA_IN_CTRL_0, 0, IDMA_BLOCK_EN);
		return;
	}

	dma_write(id, RDMA_BLOCK_OFFSET,
			IDMA_BLK_OFFSET_Y(y) | IDMA_BLK_OFFSET_X(x));
	dma_write(id, RDMA_BLOCK_SIZE, IDMA_BLK_HEIGHT(h) | IDMA_BLK_WIDTH(w));
	dma_write_mask(id, RDMA_IN_CTRL_0, ~0, IDMA_BLOCK_EN);

	cal_log_debug(id, "block x(%d) y(%d) w(%d) h(%d)\n", x, y, w, h);
}

static void idma_reg_set_format(u32 id, u32 fmt)
{
	dma_write_mask(id, RDMA_IN_CTRL_0, IDMA_IMG_FORMAT(fmt),
			IDMA_IMG_FORMAT_MASK);
}

#if defined(DMA_BIST)
static void idma_reg_set_test_pattern(u32 id, u32 pat_id, const u32 *pat_dat)
{
	u32 map_tlb[16] = {
		0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12,
	};
	u32 new_id;

	/* 0=AXI, 3=PAT */
	dma_write_mask(id, RDMA_IN_REQ_DEST, ~0, IDMA_REQ_DEST_SEL_MASK);

	new_id = map_tlb[id];
	if (pat_id == 0) {
		dma_write(new_id, GLB_TEST_PATTERN0_0, pat_dat[0]);
		dma_write(new_id, GLB_TEST_PATTERN0_1, pat_dat[1]);
		dma_write(new_id, GLB_TEST_PATTERN0_2, pat_dat[2]);
		dma_write(new_id, GLB_TEST_PATTERN0_3, pat_dat[3]);
	} else {
		dma_write(new_id, GLB_TEST_PATTERN1_0, pat_dat[4]);
		dma_write(new_id, GLB_TEST_PATTERN1_1, pat_dat[5]);
		dma_write(new_id, GLB_TEST_PATTERN1_2, pat_dat[6]);
		dma_write(new_id, GLB_TEST_PATTERN1_3, pat_dat[7]);
	}
}
#endif

static void idma_reg_set_pslave_err(u32 id, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	mask = IDMA_PSLVERR_CTRL;
	dma_write_mask(id, RDMA_PSLV_ERR_CTRL, val, mask);
}

static void idma_reg_set_comp(u32 id, enum dpp_comp_type comp_type,
		u32 rcv_num, u32 blk_size)
{
	const u32 mask = IDMA_SBWC_EN | IDMA_SAJC_EN | IDMA_SBWC_LOSSY;
	u32 val = 0;

	if (comp_type == COMP_TYPE_SBWC)
		val = IDMA_SBWC_EN;
	else if (comp_type == COMP_TYPE_SBWCL)
		val = IDMA_SBWC_EN | IDMA_SBWC_LOSSY;
	else if (comp_type == COMP_TYPE_SAJC)
		val = IDMA_SAJC_EN;

	dma_write_mask(id, RDMA_IN_CTRL_0, val, mask);
#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
	dma_write_mask(id, RDMA_RECOVERY_CTRL, val ? ~0 : 0, IDMA_RECOVERY_EN);
#else /* recovery is not needed due to hang-free */
	dma_write_mask(id, RDMA_RECOVERY_CTRL, 0, IDMA_RECOVERY_EN);
#endif
	dma_write_mask(id, RDMA_RECOVERY_CTRL, IDMA_RECOVERY_NUM(rcv_num),
				IDMA_RECOVERY_NUM_MASK);

	if (comp_type == COMP_TYPE_SAJC) {
		val = IDMA_INDEPENDENT_BLK(blk_size);
		/* if need, implement 2ppc mode */
		//val = IDMA_INDEPENDENT_BLK(blk_size | IDMA_SAJC_PPC_MODE);
		dma_write(id, RDMA_SAJC_PARAM, val);
	} else {
		dma_write_mask(id, RDMA_SBWC_PARAM,
				IDMA_CHM_BLK_BYTENUM(blk_size) |
				IDMA_LUM_BLK_BYTENUM(blk_size),
				IDMA_CHM_BLK_BYTENUM_MASK |
				IDMA_LUM_BLK_BYTENUM_MASK);
	}
}

static void idma_reg_set_deadlock(u32 id, u32 en, u32 dl_num)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, RDMA_DEADLOCK_CTRL, val, IDMA_DEADLOCK_NUM_EN);
	dma_write_mask(id, RDMA_DEADLOCK_CTRL, IDMA_DEADLOCK_NUM(dl_num),
				IDMA_DEADLOCK_NUM_MASK);
}

static void idma_reg_print_irqs_msg(u32 id, u32 irqs)
{
	u32 cfg_err;

	if (irqs & IDMA_LB_CONFLICT_IRQ)
		cal_log_err(id, "IDMA LineBuffer conflict irq occur\n");

	if (irqs & IDMA_FBC_ERR_IRQ)
		cal_log_err(id, "IDMA FBC error irq occur\n");

	if (irqs & IDMA_READ_SLAVE_ERR_IRQ)
		cal_log_err(id, "IDMA read slave error irq occur\n");

	if (irqs & IDMA_DEADLOCK_IRQ)
		cal_log_err(id, "IDMA deadlock irq occur\n");

	if (irqs & IDMA_CONFIG_ERR_IRQ) {
		cfg_err = dma_read(id, RDMA_CONFIG_ERR_STATUS);
		cal_log_err(id, "IDMA cfg err irq occur(0x%x)\n", cfg_err);
	}

	if (irqs & IDMA_VOTF_ERR_IRQ)
		cal_log_err(id, "IDMA votf err irq occur\n");
}

/****************** ODMA CAL functions ******************/
static void odma_reg_set_irq_mask_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, WDMA_IRQ, val, ODMA_ALL_IRQ_MASK);
}

static void odma_reg_set_irq_enable(u32 id)
{
	dma_write_mask(id, WDMA_IRQ, ~0, ODMA_IRQ_ENABLE);
}

static void odma_reg_set_in_qos_lut(u32 id, u32 lut_id, u32 qos_t)
{
	u32 reg_id;

	if (lut_id == 0)
		reg_id = WDMA_QOS_LUT_LOW;
	else
		reg_id = WDMA_QOS_LUT_HIGH;
	dma_write(id, reg_id, qos_t);
}

static void odma_reg_set_dynamic_gating_en_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, WDMA_DYNAMIC_GATING_EN, val, ODMA_DG_EN_ALL);
}

static void odma_reg_set_frame_alpha(u32 id, u32 alpha)
{
	dma_write_mask(id, WDMA_OUT_CTRL_0, ODMA_ALPHA(alpha),
			ODMA_ALPHA_MASK);
}

static void odma_reg_clear_irq(u32 id, u32 irq)
{
	dma_write_mask(id, WDMA_IRQ, ~0, irq);
}

static void odma_reg_set_sw_reset(u32 id)
{
	dma_write_mask(id, WDMA_ENABLE, ~0, ODMA_SRESET);
}

static int odma_reg_wait_sw_reset_status(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dma_regs_desc(id)->regs + WDMA_ENABLE,
			val, !(val & ODMA_SRESET), 10, 2000); /* timeout 2ms */
	if (ret) {
		cal_log_err(id, "[odma%d] timeout sw-reset\n", id);
		return ret;
	}

	return 0;
}

static int odma_reg_wait_idle_status(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dma_regs_desc(id)->regs + WDMA_ENABLE,
			val, !(val & ODMA_OP_STATUS), 10, DPP_IDLE_WAIT_TIMEOUT);
	if (ret) {
		cal_log_err(id, "[odma] timeout op-status-idle\n");
		return ret;
	}

	return 0;
}

static void odma_reg_set_coordinates(u32 id, struct decon_frame *dst)
{
	dma_write(id, WDMA_DST_OFFSET,
			ODMA_DST_OFFSET_Y(dst->y) | ODMA_DST_OFFSET_X(dst->x));
	dma_write(id, WDMA_DST_WIDTH, ODMA_DST_WIDTH(dst->f_w));
	dma_write(id, WDMA_DST_HEIGHT, ODMA_DST_HEIGHT(dst->f_h));
	dma_write(id, WDMA_IMG_SIZE,
			ODMA_IMG_HEIGHT(dst->h) | ODMA_IMG_WIDTH(dst->w));
}

static void odma_reg_set_ic_max(u32 id, u32 ic_max)
{
	dma_write_mask(id, WDMA_OUT_CTRL_0, ODMA_IC_MAX(ic_max),
			ODMA_IC_MAX_MASK);
}

static void odma_reg_set_format(u32 id, u32 fmt)
{
	dma_write_mask(id, WDMA_OUT_CTRL_0, ODMA_IMG_FORMAT(fmt),
			ODMA_IMG_FORMAT_MASK);
}

static void odma_reg_set_pslave_err(u32 id, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	mask = ODMA_PSLVERR_CTRL;
	dma_write_mask(id, WDMA_PSLV_ERR_CTRL, val, mask);
}

static void odma_reg_print_irqs_msg(u32 id, u32 irqs)
{
	u32 cfg_err;

	if (irqs & ODMA_WRITE_SLAVE_ERR_IRQ)
		cal_log_err(id, "ODMA write slave error irq occur\n");

	if (irqs & ODMA_DEADLOCK_IRQ)
		cal_log_err(id, "ODMA deadlock error irq occur\n");

	if (irqs & ODMA_CONFIG_ERR_IRQ) {
		cfg_err = dma_read(id, WDMA_CONFIG_ERR_STATUS);
		cal_log_err(id, "ODMA cfg err irq occur(0x%x)\n", cfg_err);
	}
}

static inline void votf_reg_set_ring_clock_enable(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	votf_write(id, C2S_RING_CLK_ENABLE, val);
}

static inline void votf_reg_set_ring_enable(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	votf_write(id, C2S_RING_ENABLE, val);
}

static inline void votf_reg_set_local_ip(u32 id, u32 ip_addr)
{
	u32 val = ip_addr >> 16;

	votf_write(id, C2S_LOCAL_IP, val);
}

static inline void votf_reg_set_immediate_mode(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	votf_write(id, C2S_REGISTER_MODE, val);
}

static inline void votf_reg_set_a(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	votf_write(id, C2S_SEL_REGISTER, val);
}

static inline void votf_reg_set_tws_enable(u32 id, u32 tws_id, u32 en)
{
	u32 val = en ? ~0 : 0;

	votf_write(id, C2S_TWS_ENABLE(tws_id), val);
}

static inline void votf_reg_set_tws_limit(u32 id, u32 tws_id, u32 limit)
{
	votf_write(id, C2S_TWS_LIMIT(tws_id), limit);
}

static inline void votf_reg_set_tsw_dest(u32 id, u32 tws_id, u32 dst_ip, u32 dst_id)
{
	votf_write(id, C2S_TWS_DEST(tws_id),
			C2S_TSW_DST_ID(dst_id) | C2S_TWS_DST_IP(dst_ip >> 16));
}

static inline void votf_reg_set_tws_lines_in_token(u32 id, u32 tws_id, u32 en)
{
	votf_write(id, C2S_TWS_LINES_IN_TOKEN(tws_id), en);
}

static void votf_reg_set_tws(u32 id, u32 tws_id, u32 dest_base_addr)
{
	votf_reg_set_tws_enable(id, tws_id, 1);
	votf_reg_set_tws_limit(id, tws_id, 0xff);
	votf_reg_set_tsw_dest(id, tws_id, dest_base_addr, 2); // DST : MFC TRS2
	votf_reg_set_tws_lines_in_token(id, tws_id, 1);
}

static void votf_reg_set_mode(u32 id, u32 tws_id, u32 mode)
{
	votf_write(id, VOTF_CONNECT_MODE + (tws_id * 4), (tws_id << 4) | mode);
}

static void odma_reg_set_votf(u32 id, u32 en, u32 votf_idx)
{
	u32 val = en ? ODMA_VOTF_ENABLE : ODMA_VOTF_DISABLE;

	dma_write_mask(id, WDMA_VOTF_EN, (votf_idx << 8) | val,
			ODMA_VOTF_MST_IDX_MASK | ODMA_VOTF_EN_MASK);
}

/****************** DPP CAL functions ******************/
static void dpp_reg_set_irq_mask_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dpp_write_mask(id, DPP_COM_IRQ_MASK, val, DPP_ALL_IRQ_MASK);
}

void dpp_reg_set_irq_enable(u32 id)
{
	dpp_write_mask(id, DPP_COM_IRQ_CON, ~0, DPP_IRQ_EN);
}

void dpp_reg_set_irq_disable(u32 id)
{
	dpp_write_mask(id, DPP_COM_IRQ_CON, 0, DPP_IRQ_EN);
}

static void dpp_reg_set_linecnt(u32 id, u32 en)
{
	if ((id != REGS_DPP8_ID) && (id != REGS_DPP16_ID))
		return;

	if (en)
		dpp_write_mask(id, DPP_WB_LC_CON,
				DPP_LC_MODE(0) | DPP_LC_EN(1),
				DPP_LC_MODE_MASK | DPP_LC_EN_MASK);
	else
		dpp_write_mask(id, DPP_WB_LC_CON, DPP_LC_EN(0),
				DPP_LC_EN_MASK);
}

static void dpp_reg_clear_irq(u32 id, u32 irq)
{
	dpp_write_mask(id, DPP_COM_IRQ_STATUS, ~0, irq);
}

static void dpp_reg_set_sw_reset(u32 id)
{
	dpp_write_mask(id, DPP_COM_SWRST_CON, ~0, DPP_SRESET);
}

static int dpp_reg_wait_sw_reset_status(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(
			dpp_regs_desc(id)->regs + DPP_COM_SWRST_CON, val,
			!(val & DPP_SRESET), 10, 2000); /* timeout 2ms */
	if (ret) {
		cal_log_err(id, "[dpp%d] timeout sw-reset\n", id);
		return ret;
	}

	return 0;
}

static int dpp_reg_wait_idle_status(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(
			dpp_regs_desc(id)->regs + DPP_COM_OP_STATUS, val,
			!(val & DPP_OP_STATUS), 10, DPP_IDLE_WAIT_TIMEOUT);
	if (ret) {
		cal_log_err(id, "[dpp%d] timeout op-status-idle\n");
		return ret;
	}

	return 0;
}

static void dpp_reg_set_pslave_err(u32 id, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	mask = DPP_PSLVERR_EN;
	dma_write_mask(id, DPP_COM_PSLVERR_CON, val, mask);
}

static void dpp_reg_set_uv_offset(u32 id, u32 off_x, u32 off_y)
{
	u32 val, mask;

	val = DPP_UV_OFFSET_Y(off_y) | DPP_UV_OFFSET_X(off_x);
	mask = DPP_UV_OFFSET_Y_MASK | DPP_UV_OFFSET_X_MASK;
	dpp_write_mask(id, DPP_COM_SUB_CON, val, mask);
}

static void
dpp_reg_set_csc_coef(u32 id, u32 std, u32 range, const unsigned long attr)
{
	u32 val, mask;
	u32 csc_id = DPP_CSC_IDX_BT601_625;
	u32 c00, c01, c02;
	u32 c10, c11, c12;
	u32 c20, c21, c22;
	const u16 (*csc_arr)[3][3];

	switch (std) {
	case EXYNOS_STANDARD_BT601_625:
		csc_id = DPP_CSC_IDX_BT601_625;
		break;
	case EXYNOS_STANDARD_BT601_625_UNADJUSTED:
		csc_id = DPP_CSC_IDX_BT601_625_UNADJUSTED;
		break;
	case EXYNOS_STANDARD_BT601_525:
		csc_id = DPP_CSC_IDX_BT601_525;
		break;
	case EXYNOS_STANDARD_BT601_525_UNADJUSTED:
		csc_id = DPP_CSC_IDX_BT601_525_UNADJUSTED;
		break;
	case EXYNOS_STANDARD_BT2020_CONSTANT_LUMINANCE:
		csc_id = DPP_CSC_IDX_BT2020_CONSTANT_LUMINANCE;
		break;
	case EXYNOS_STANDARD_BT470M:
		csc_id = DPP_CSC_IDX_BT470M;
		break;
	case EXYNOS_STANDARD_FILM:
		csc_id = DPP_CSC_IDX_FILM;
		break;
	case EXYNOS_STANDARD_ADOBE_RGB:
		csc_id = DPP_CSC_IDX_ADOBE_RGB;
		break;
	case EXYNOS_STANDARD_BT709:
		csc_id = DPP_CSC_IDX_BT709;
		break;
	case EXYNOS_STANDARD_BT2020:
		csc_id = DPP_CSC_IDX_BT2020;
		break;
	case EXYNOS_STANDARD_DCI_P3:
		csc_id = DPP_CSC_IDX_DCI_P3;
		break;
	default:
		range = EXYNOS_RANGE_LIMITED;
		cal_log_err(id, "invalid CSC type\n");
		cal_log_err(id, "BT601 with limited range is set as default\n");
	}

	if (test_bit(DPP_ATTR_ODMA, &attr))
		csc_arr = csc_r2y_3x3_t;
	else
		csc_arr = csc_y2r_3x3_t;

	/*
	 * The matrices are provided only for full or limited range
	 * and limited range is used as default.
	 */
	if (range == EXYNOS_RANGE_FULL)
		csc_id += 1;

	c00 = csc_arr[csc_id][0][0];
	c01 = csc_arr[csc_id][0][1];
	c02 = csc_arr[csc_id][0][2];

	c10 = csc_arr[csc_id][1][0];
	c11 = csc_arr[csc_id][1][1];
	c12 = csc_arr[csc_id][1][2];

	c20 = csc_arr[csc_id][2][0];
	c21 = csc_arr[csc_id][2][1];
	c22 = csc_arr[csc_id][2][2];

	mask = (DPP_CSC_COEF_H_MASK | DPP_CSC_COEF_L_MASK);
	val = (DPP_CSC_COEF_H(c01) | DPP_CSC_COEF_L(c00));
	dpp_write_mask(id, DPP_COM_CSC_COEF0, val, mask);

	val = (DPP_CSC_COEF_H(c10) | DPP_CSC_COEF_L(c02));
	dpp_write_mask(id, DPP_COM_CSC_COEF1, val, mask);

	val = (DPP_CSC_COEF_H(c12) | DPP_CSC_COEF_L(c11));
	dpp_write_mask(id, DPP_COM_CSC_COEF2, val, mask);

	val = (DPP_CSC_COEF_H(c21) | DPP_CSC_COEF_L(c20));
	dpp_write_mask(id, DPP_COM_CSC_COEF3, val, mask);

	mask = DPP_CSC_COEF_L_MASK;
	val = DPP_CSC_COEF_L(c22);
	dpp_write_mask(id, DPP_COM_CSC_COEF4, val, mask);

	cal_log_debug(id, "---[%s CSC Type: std=%d, rng=%d]---\n",
		test_bit(DPP_ATTR_ODMA, &attr) ? "R2Y" : "Y2R", std, range);
	cal_log_debug(id, "0x%4x  0x%4x  0x%4x\n", c00, c01, c02);
	cal_log_debug(id, "0x%4x  0x%4x  0x%4x\n", c10, c11, c12);
	cal_log_debug(id, "0x%4x  0x%4x  0x%4x\n", c20, c21, c22);
}

static void
dpp_reg_set_csc_params(u32 id, u32 std, u32 range, const unsigned long attr)
{
	u32 type, hw_range, mode, val, mask;

	mode = DPP_CSC_MODE_HARDWIRED;

	switch (std) {
	case EXYNOS_STANDARD_UNSPECIFIED:
		type = DPP_CSC_TYPE_BT601;
		cal_log_debug(id, "unspecified CSC type! -> BT_601\n");
		break;
	case EXYNOS_STANDARD_BT709:
		type = DPP_CSC_TYPE_BT709;
		break;
	case EXYNOS_STANDARD_BT601_625:
	case EXYNOS_STANDARD_BT601_625_UNADJUSTED:
	case EXYNOS_STANDARD_BT601_525:
	case EXYNOS_STANDARD_BT601_525_UNADJUSTED:
		type = DPP_CSC_TYPE_BT601;
		break;
	case EXYNOS_STANDARD_BT2020:
		type = DPP_CSC_TYPE_BT2020;
		break;
	case EXYNOS_STANDARD_DCI_P3:
		type = DPP_CSC_TYPE_DCI_P3;
		break;
	default:
		type = DPP_CSC_TYPE_BT601;
		mode = DPP_CSC_MODE_CUSTOMIZED;
		break;
	}

	if (test_bit(DPP_ATTR_ODMA, &attr))
		mode = DPP_CSC_MODE_CUSTOMIZED;

	/*
	 * DPP hardware supports full or limited range.
	 * Limited range is used as default
	 */
	if (range == EXYNOS_RANGE_FULL)
		hw_range = DPP_CSC_RANGE_FULL;
	else if (range == EXYNOS_RANGE_LIMITED)
		hw_range = DPP_CSC_RANGE_LIMITED;
	else
		hw_range = DPP_CSC_RANGE_LIMITED;

	val = type | hw_range | mode;
	mask = (DPP_CSC_TYPE_MASK | DPP_CSC_RANGE_MASK | DPP_CSC_MODE_MASK);
	dpp_write_mask(id, DPP_COM_CSC_CON, val, mask);

	if (mode == DPP_CSC_MODE_CUSTOMIZED)
		dpp_reg_set_csc_coef(id, std, range, attr);
}

static void dpp_reg_set_h_coef(u32 id, u32 cid, u32 h_ratio)
{
	int i, j, sc_ratio;

	if (h_ratio <= DPP_SC_RATIO_MAX)
		sc_ratio = 0;
	else if (h_ratio <= DPP_SC_RATIO_7_8)
		sc_ratio = 1;
	else if (h_ratio <= DPP_SC_RATIO_6_8)
		sc_ratio = 2;
	else if (h_ratio <= DPP_SC_RATIO_5_8)
		sc_ratio = 3;
	else if (h_ratio <= DPP_SC_RATIO_4_8)
		sc_ratio = 4;
	else if (h_ratio <= DPP_SC_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < 9; i++)
		for (j = 0; j < 8; j++)
			coef_write(id, DPP_H_COEF(cid, i, j),
					h_coef_8t[sc_ratio][i][j]);
}

static void dpp_reg_set_v_coef(u32 id ,u32 cid, u32 v_ratio)
{
	int i, j, sc_ratio;

	if (v_ratio <= DPP_SC_RATIO_MAX)
		sc_ratio = 0;
	else if (v_ratio <= DPP_SC_RATIO_7_8)
		sc_ratio = 1;
	else if (v_ratio <= DPP_SC_RATIO_6_8)
		sc_ratio = 2;
	else if (v_ratio <= DPP_SC_RATIO_5_8)
		sc_ratio = 3;
	else if (v_ratio <= DPP_SC_RATIO_4_8)
		sc_ratio = 4;
	else if (v_ratio <= DPP_SC_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < 9; i++)
		for (j = 0; j < 4; j++)
			coef_write(id, DPP_V_COEF(cid, i, j),
					v_coef_4t[sc_ratio][i][j]);
}

static void dpp_reg_set_scale_ratio(u32 id, struct dpp_params_info *p)
{
	u32 coef_id = (id % DPP_PER_DPUF) / 2;

	dpp_write_mask(id, DPP_COM_SCL_CTRL, DPP_SCL_ENABLE(p->is_scale),
			DPP_SCL_ENABLE_MASK);
	if (p->is_scale) {
		dpp_write_mask(id, DPP_COM_SCL_CTRL, DPP_SCL_COEF_SEL(coef_id),
				DPP_SCL_COEF_SEL_MASK);
		dpp_write_mask(id, DPP_COM_SCL_MAIN_H_RATIO,
				DPP_SCL_H_RATIO(p->h_ratio), DPP_SCL_H_RATIO_MASK);
		dpp_write_mask(id, DPP_COM_SCL_MAIN_V_RATIO,
				DPP_SCL_V_RATIO(p->v_ratio), DPP_SCL_V_RATIO_MASK);

		dpp_reg_set_h_coef(id, coef_id, p->h_ratio);
		dpp_reg_set_v_coef(id, coef_id, p->v_ratio);
	}

	cal_log_debug(id, "coef_id : %d h_ratio : %#x, v_ratio : %#x\n",
			coef_id, p->h_ratio, p->v_ratio);
}

static void dpp_reg_set_scl_pos(u32 id, struct dpp_params_info *p)
{
	u32 padding = 4; /* split case : padding is fixed to 4 */

	/* Initialize setting of initial phase */
	dpp_write_mask(id, DPP_COM_SCL_HPOSITION, DPP_SCL_HPOS(0),
			DPP_SCL_HPOS_MASK);
	dpp_write_mask(id, DPP_COM_SCL_VPOSITION, DPP_SCL_VPOS(0),
			DPP_SCL_VPOS_MASK);

	if (p->split) { /* except EXYNOS_SPLIT_NONE */
		if (p->src.f_w > p->src.f_h) { /* landscape */
			if ((p->dst.x >= (p->dst.f_w / 2 - padding)) &&
				(p->dst.x <= (p->dst.f_w / 2 + padding))) {
				dpp_write_mask(id, DPP_COM_SCL_HPOSITION,
						DPP_POS_I(padding),
						DPP_POS_I_MASK);
			}
		} else { /* portrait */
			if ((p->dst.y >= (p->dst.f_h / 2 - padding)) &&
				(p->dst.y <= (p->dst.f_h / 2 + padding))) {
				dpp_write_mask(id, DPP_COM_SCL_VPOSITION,
						DPP_POS_I(padding),
						DPP_POS_I_MASK);
			}
		}

		cal_log_debug(id, "split : %d , rotation : %d\n",
				p->split, p->rot);
		cal_log_debug(id, "DPP_COM_SCL_H/VPOSITION = 0x%08x, 0x%08x)\n",
				dpp_read(id, DPP_COM_SCL_HPOSITION),
				dpp_read(id, DPP_COM_SCL_VPOSITION));
	}
}

static void dpp_reg_set_img_size(u32 id, u32 w, u32 h)
{
	dpp_write(id, DPP_COM_IMG_SIZE, DPP_IMG_HEIGHT(h) | DPP_IMG_WIDTH(w));
}

static void dpp_reg_set_scaled_img_size(u32 id, u32 w, u32 h)
{
	dpp_write(id, DPP_COM_SCL_SCALED_IMG_SIZE,
		DPP_SCL_SCALED_IMG_HEIGHT(h) | DPP_SCL_SCALED_IMG_WIDTH(w));
}

void dpp_reg_set_hdr_mul_con(u32 id, u32 en)
{
	cal_log_debug(id, "unsupported\n");
}

static void dpp_reg_set_alpha_type(u32 id, u32 type)
{
	/* [type] 0=per-frame, 1=per-pixel */
	dpp_write_mask(id, DPP_COM_IO_CON, DPP_ALPHA_SEL(type),
			DPP_ALPHA_SEL_MASK);
}

static void dpp_reg_set_format(u32 id, u32 fmt)
{
	dpp_write_mask(id, DPP_COM_IO_CON, DPP_IMG_FORMAT(fmt),
			DPP_IMG_FORMAT_MASK);
}

static void dpp_reg_print_irqs_msg(u32 id, u32 irqs)
{
	u32 cfg_err;

	if (irqs & DPP_CFG_ERROR_IRQ) {
		cfg_err = dpp_read(id, DPP_COM_CFG_ERROR_STATUS);
		cal_log_err(id, "DPP cfg err irq occur(0x%x)\n", cfg_err);
	}
}

static void dpp_reg_set_bpc_mode(u32 id, enum dpp_bpc bpc)
{
	dpp_write_mask(id, DPP_COM_IO_CON, DPP_BPC_MODE(bpc),
			DPP_BPC_MODE_MASK);
}

/****************** HDR_COMM CAL functions ******************/

static void hdrc_reg_set_linecnt(u32 id, u32 en)
{
	if (en)
		hdr_comm_write_mask(id, LSI_COMM_LC_CON,
				COMM_LC_MODE(0) | COMM_LC_EN(1),
				COMM_LC_MODE_MASK | COMM_LC_EN_MASK);
	else
		hdr_comm_write_mask(id, LSI_COMM_LC_CON, COMM_LC_EN(0),
				COMM_LC_EN_MASK);
}

static void hdrc_reg_set_bpc_mode(u32 id, u32 bpc)
{
	hdr_comm_write_mask(id, LSI_COMM_IO_CON, COMM_BPC_MODE(bpc),
			COMM_BPC_MODE_MASK);
}

static void hdrc_reg_set_format(u32 id, u32 fmt)
{
	hdr_comm_write_mask(id, LSI_COMM_IO_CON, COMM_IMG_FORMAT(fmt),
			COMM_IMG_FORMAT_MASK);
}

/*
 * 0: disable
 * 1: enable de-multiplication before HDR and re-multiplication after
 */
void hdrc_reg_set_mul_con(u32 id, u32 en)
{
	hdr_comm_write(id, LSI_COMM_HDR_CON, COMM_MULT_EN(en));
}

static void hdrc_reg_set_dither_en(u32 id, u32 en)
{
	hdr_comm_write_mask(id, LSI_COMM_DITH_CON, COMM_DITH_EN(en),
			COMM_DITH_EN_MASK);
}

/* TODO: static */
void hdrc_reg_set_dither_mask_sel(u32 id, u32 sel)
{
	hdr_comm_write_mask(id, LSI_COMM_DITH_CON, COMM_DITH_MASK_SEL(sel),
			COMM_DITH_MASK_SEL_MASK);
}

/* TODO: static */
void hdrc_reg_set_dither_mask_spin(u32 id, u32 spin)
{
	hdr_comm_write_mask(id, LSI_COMM_DITH_CON, COMM_DITH_MASK_SPIN(spin),
			COMM_DITH_MASK_SPIN_MASK);
}

static void hdrc_reg_set_pslave_err(u32 id, u32 en)
{
	u32 val;

	val = en ? COMM_PSLVERR_EN : 0;
	hdr_comm_write(id, LSI_COMM_PSLVERR_CON, val);
}

/* TODO: static */
void hdrc_reg_set_aosp_en(u32 id, u32 en)
{
	hdr_comm_write_mask(id, LSI_COMM_AOSP_EN, COMM_AOSP_EN(en),
			COMM_AOSP_EN_MASK);
}

static void hdrc_reg_set_img_size(u32 id, u32 w, u32 h)
{
	hdr_comm_write(id, LSI_COMM_SIZE,
			COMM_SFR_VSIZE(h) | COMM_SFR_HSIZE(w));
}

/* AOSP COEF + OFFS */
void hdrc_reg_set_aosp_coef(u32 id, s32 coeff[3][3], s32 offs[3])
{
	int i, j;

	/*
	 * |Rout| = |C00 C01 C02| |Rin| + |offset0|
	 * |Gout| = |C10 C11 C12| |Gin| + |offset1|
	 * |Bout| = |C20 C21 C22| |Bin| + |offset2|
	 */
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			hdr_comm_write(id, LSI_COMM_AOSP_COEF(i * 3 + j),
					coeff[i][j]);
		}
		hdr_comm_write(id, LSI_COMM_AOSP_OFFS(i), offs[i]);
	}

	cal_log_debug(0, "--[ L%d: AOSP COEF Matrix: 3 x 3 + offs ]--\n", id);
	cal_log_debug(0, "0x%04x  0x%04x  0x%04x, 0x%04x\n",
			coeff[0][0], coeff[0][1], coeff[0][2], offs[0]);
	cal_log_debug(0, "0x%04x  0x%04x  0x%04x, 0x%04x\n",
			coeff[1][0], coeff[1][1], coeff[1][2], offs[1]);
	cal_log_debug(0, "0x%04x  0x%04x  0x%04x, 0x%04x\n",
			coeff[2][0], coeff[2][1], coeff[2][2], offs[2]);
}

/* TODO: static */
u32 hdrc_reg_get_dither_line_cnt(u32 id)
{
	u32 val;

	val = hdr_comm_read(id, LSI_COMM_DITH_LINE_CNT);

	return COMM_SFR_LINE_CNT_GET(val);
}

/* TODO: static */
u32 hdrc_reg_get_dither_dbg_data(u32 id)
{
	u32 val;

	val = hdr_comm_read(id, LSI_COMM_DITH_DBG_DATA);

	return val;
}
static void hdrc_reg_set_hdr_bypass(u32 id, u32 en)
{
	hdr_comm_write(id, LSI_COMM_HDR_BYPASS, COMM_HDR_BYPASS(en));
}
/* TODO: static */
void dpp_reg_set_hdr_params(u32 id, struct dpp_params_info *p)
{
#if 0
	cal_log_debug(id, "+\n");

	hdr_reg_set_hdr_en(id, p->hdr.en);
	if (!p->hdr.en) {
		cal_log_debug(id, "HDR Off\n");
		return;
	}

	dpp_reg_set_hdr_mul_con(id, p->hdr.mul_en);

	hdr_reg_set_eotf_en(id, p->hdr.eotf.en);
	if (p->hdr.eotf.en) {
		hdr_reg_set_eotf_lut(id, p->hdr.eotf.pq_en,
				p->hdr.eotf.lut_x, p->hdr.eotf.lut_y);
		if (p->hdr.eotf.pq_en)
			hdr_reg_set_eotf_normal(id, p->hdr.eotf.coef,
					p->hdr.eotf.shift);
	}

	hdr_reg_set_oetf_en(id, p->hdr.oetf.en);
	if (p->hdr.oetf.en)
		hdr_reg_set_oetf_lut(id, p->hdr.oetf.lut_x, p->hdr.oetf.lut_y);

	hdr_reg_set_gm_en(id, p->hdr.gm.en);
	if (p->hdr.gm.en)
		hdr_reg_set_gm_coef(id, p->hdr.gm.coef, p->hdr.gm.offs);

	cal_log_debug(id, "-\n");
#endif
}

/* TODO: static */
void dpp_reg_set_hdr10p_params(u32 id, struct dpp_params_info *p)
{
#if 0
	hdr_reg_set_tm_en(id, p->hdr.tm.en);
	if (!p->hdr.tm.en)
		return;

	hdr_reg_set_tm_coef(id, p->hdr.tm.coef_r, p->hdr.tm.coef_g,
			p->hdr.tm.coef_b);
	hdr_reg_set_tm_range(id, p->hdr.tm.min_x, p->hdr.tm.max_x,
			p->hdr.tm.min_y, p->hdr.tm.max_y);

	hdr_reg_set_tm_lut(id, p->hdr.tm.lut_x, p->hdr.tm.lut_y);
#endif
}

#if IS_ENABLED(CONFIG_DRM_MCD_HDR)
void dpp_reg_set_hdrc_dither(u32 id, bool mcd_hdr, enum dpp_bpc bpc)
{
	if (mcd_hdr) {
		dpp_reg_set_bpc_mode(id, DPP_BPC_10);
		if (bpc == DPP_BPC_8)
			hdrc_reg_set_dither_en(id, 1);
		else
			hdrc_reg_set_dither_en(id, 0);
	}
}
#endif
/********** IDMA and ODMA combination CAL functions **********/
/*
 * Y8 : Y8 or RGB base, AFBC or SBWC-Y header
 * C8 : C8 base,        AFBC or SBWC-Y payload
 * Y2 : (SBWC disable - Y2 base), (SBWC enable - C header)
 * C2 : (SBWC disable - C2 base), (SBWC enable - C payload)
 *
 * PLANE_0_STRIDE : Y-HD (or Y-2B) stride -> y_hd_y2_stride
 * PLANE_1_STRIDE : Y-PL (or C-2B) stride -> y_pl_c2_stride
 * PLANE_2_STRIDE : C-HD stride           -> c_hd_stride
 * PLANE_3_STRIDE : C-PL stride           -> c_pl_stride
 *
 * [ MFC encoder: buffer for SBWC - similar to 8+2 ]
 * plane[0] fd : Y payload(base addr) + Y header => Y header calc.
 * plane[1] fd : C payload(base addr) + C header => C header calc.
 */
static void dma_reg_set_base_addr(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		dma_write(id, RDMA_BASEADDR_P0, p->addr[0]);
		if (p->comp_type == COMP_TYPE_AFBC)
			dma_write(id, RDMA_BASEADDR_P1, p->addr[0]);
		else
			dma_write(id, RDMA_BASEADDR_P1, p->addr[1]);

		/* TODO: consider the 4 planes */
		/* use 4 base addresses */
		if (p->comp_type == COMP_TYPE_SBWC ||
				p->comp_type == COMP_TYPE_SBWCL) {
			dma_write(id, RDMA_BASEADDR_P2, p->addr[2]);
			dma_write(id, RDMA_BASEADDR_P3, p->addr[3]);
			dma_write_mask(id, RDMA_SRC_STRIDE_0,
					IDMA_STRIDE_0(p->y_hd_y2_stride),
					IDMA_STRIDE_0_MASK);
			dma_write_mask(id, RDMA_SRC_STRIDE_1,
					IDMA_STRIDE_1(p->y_pl_c2_stride),
					IDMA_STRIDE_1_MASK);

			/* C-stride of SBWC: valid if STRIDE_SEL is enabled */
			dma_write_mask(id, RDMA_SRC_STRIDE_2,
					IDMA_STRIDE_2(p->c_hd_stride),
					IDMA_STRIDE_2_MASK);
			dma_write_mask(id, RDMA_SRC_STRIDE_3,
					IDMA_STRIDE_3(p->c_pl_stride),
					IDMA_STRIDE_3_MASK);
		}
	} else if (test_bit(DPP_ATTR_ODMA, &attr)) {
		dma_write(id, WDMA_BASEADDR_P0, p->addr[0]);
		dma_write(id, WDMA_BASEADDR_P1, p->addr[1]);

		if (p->comp_type == COMP_TYPE_SBWC) {
			dma_write(id, WDMA_BASEADDR_P2, p->addr[2]);
			dma_write(id, WDMA_BASEADDR_P3, p->addr[3]);
			dma_write_mask(id, WDMA_SRC_STRIDE_0,
					ODMA_STRIDE_0(p->y_hd_y2_stride),
					ODMA_STRIDE_0_MASK);
			dma_write_mask(id, WDMA_SRC_STRIDE_1,
					ODMA_STRIDE_1(p->y_pl_c2_stride),
					ODMA_STRIDE_1_MASK);

			/* C-stride of SBWC: valid if STRIDE_SEL is enabled */
			dma_write_mask(id, WDMA_SRC_STRIDE_2,
					ODMA_STRIDE_2(p->c_hd_stride),
					ODMA_STRIDE_2_MASK);
			dma_write_mask(id, WDMA_SRC_STRIDE_3,
					ODMA_STRIDE_3(p->c_pl_stride),
					ODMA_STRIDE_3_MASK);
		}
	}

	cal_log_debug(id, "base addr 1p(0x%lx) 2p(0x%lx) 3p(0x%lx) 4p(0x%lx)\n",
			(unsigned long)p->addr[0], (unsigned long)p->addr[1],
			(unsigned long)p->addr[2], (unsigned long)p->addr[3]);
	if (p->comp_type == COMP_TYPE_SBWC)
		cal_log_debug(id, "[stride] y(0x%x 0x%x) c(0x%x 0x%x)\n",
			p->y_hd_y2_stride, p->y_pl_c2_stride, p->c_hd_stride,
			p->c_pl_stride);
}

/********** IDMA, ODMA, DPP and WB MUX combination CAL functions **********/
static void dma_dpp_reg_set_coordinates(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_set_coordinates(id, &p->src);

		if (test_bit(DPP_ATTR_DPP, &attr)) {
			hdrc_reg_set_img_size(id, p->dst.w, p->dst.h);
			if (p->rot > DPP_ROT_180)
				dpp_reg_set_img_size(id, p->src.h, p->src.w);
			else
				dpp_reg_set_img_size(id, p->src.w, p->src.h);
		}

		if (test_bit(DPP_ATTR_SCALE, &attr)) {
			dpp_reg_set_scl_pos(id, p);
			dpp_reg_set_scaled_img_size(id, p->dst.w, p->dst.h);
		}
	} else if (test_bit(DPP_ATTR_ODMA, &attr)) {
		odma_reg_set_coordinates(id, &p->src);
		if (test_bit(DPP_ATTR_DPP, &attr))
			dpp_reg_set_img_size(id, p->src.w, p->src.h);
	}
}

static int dma_dpp_reg_set_format(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	u32 alpha_type = 0; /* 0: per-frame, 1: per-pixel */
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);
	enum dpp_bpc bpc = (fmt_info->bpc >= 10) ? DPP_BPC_10 : DPP_BPC_8;

	alpha_type = (fmt_info->len_alpha > 0) ? 1 : 0;

	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_set_format(id, fmt_info->dma_fmt);
		if (test_bit(DPP_ATTR_DPP, &attr)) {
			if (p->in_bpc < bpc) {
				dpp_reg_set_bpc_mode(id, bpc);
				hdrc_reg_set_bpc_mode(id, bpc);
				hdrc_reg_set_dither_en(id, 1);
			} else {
				dpp_reg_set_bpc_mode(id, p->in_bpc);
				hdrc_reg_set_bpc_mode(id, p->in_bpc);
				hdrc_reg_set_dither_en(id, 0);
			}
			dpp_reg_set_alpha_type(id, alpha_type);
			dpp_reg_set_format(id, fmt_info->dpp_fmt);
			hdrc_reg_set_format(id, fmt_info->dpp_fmt);
		}
	} else if (test_bit(DPP_ATTR_ODMA, &attr)) {
		odma_reg_set_format(id, fmt_info->dma_fmt);
		if (test_bit(DPP_ATTR_DPP, &attr)) {
			dpp_reg_set_bpc_mode(id, bpc);
			dpp_reg_set_uv_offset(id, 0, 0);
			dpp_reg_set_alpha_type(id, alpha_type);
			dpp_reg_set_format(id, fmt_info->dpp_fmt);
		}
	}

	return 0;
}

/* trs_idx is valid 8 ~ 15 for each VOTF in pamir. */
static void idma_reg_set_votf(u32 id, u32 en, u32 buf_idx, u32 rcv_num)
{
	u32 trs_id = id % DPP_PER_DPUF + DPP_PER_DPUF;

	if (en) {
		dma_write_mask(id, RDMA_RECOVERY_CTRL, 1, IDMA_RECOVERY_EN);
		dma_write_mask(id, RDMA_RECOVERY_CTRL, IDMA_RECOVERY_NUM(rcv_num),
				IDMA_RECOVERY_NUM_MASK);
		dma_write_mask(id, RDMA_VOTF_EN, IDMA_VOTF_TRS_IDX(trs_id),
				IDMA_VOTF_TRS_IDX_MASK);
		dma_write_mask(id, RDMA_VOTF_EN, IDMA_VOTF_RBUF_IDX(buf_idx),
				IDMA_VOTF_RBUF_IDX_MASK);
		dma_write_mask(id, RDMA_VOTF_EN, IDMA_VOTF_HALF_CONN_MODE,
				IDMA_VOTF_EN_MASK);
	} else {
		idma_reg_set_votf_disable(id);
	}
}

/******************** EXPORTED DPP CAL APIs ********************/
/*
 * When LCD is turned on in the bootloader, sometimes the hardware state of
 * DPU_DMA is not cleaned up normally while booting up the kernel.
 * At this time, the hardware may go into abnormal state, so it needs to reset
 * hardware. However, if it resets every time, it could have an overhead, so
 * it's better to reset once.
 */
void dpp_reg_init(u32 id, const unsigned long attr)
{
	if (test_bit(DPP_ATTR_SRAMC, &attr)) {
		sramc_reg_set_pslave_err(id, 1);
		sramc_reg_set_mode_enable(id, 0);
	}

	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		if (dma_read(id, RDMA_IRQ) & IDMA_DEADLOCK_IRQ) {
			idma_reg_set_sw_reset(id);
			idma_reg_wait_sw_reset_status(id);
		}
		idma_reg_set_irq_mask_all(id, 0);
		idma_reg_set_irq_enable(id);
		idma_reg_set_in_qos_lut(id, 0, 0x44444444);
		idma_reg_set_in_qos_lut(id, 1, 0x44444444);
		/* TODO: clock gating will be enabled */
		idma_reg_set_sram_clk_gate_en(id, 0);
		idma_reg_set_dynamic_gating_en_all(id, 0);
		idma_reg_set_frame_alpha(id, 0xFF);
		idma_reg_set_ic_max(id, 0x40);
		idma_reg_set_assigned_mo(id, 0x40);
		idma_reg_set_pslave_err(id, 1);
	}

	if (test_bit(DPP_ATTR_DPP, &attr)) {
		dpp_reg_set_irq_mask_all(id, 0);
		dpp_reg_set_irq_enable(id);
		dpp_reg_set_linecnt(id, 1);
		dpp_reg_set_pslave_err(id, 1);
	}
	if (test_bit(DPP_ATTR_HDR_COMM, &attr)) {
		hdrc_reg_set_linecnt(id, 1);
		hdrc_reg_set_pslave_err(id, 1);
#if IS_ENABLED(CONFIG_DRM_MCD_HDR)
		hdrc_reg_set_hdr_bypass(id, 0); /* Using MCD HDR IP */
#else
		hdrc_reg_set_hdr_bypass(id, 1); /* disable if customer's HDR is used */
#endif
	}
	if (test_bit(DPP_ATTR_ODMA, &attr)) {
		if (dma_read(id, WDMA_IRQ) & ODMA_DEADLOCK_IRQ) {
			odma_reg_set_sw_reset(id);
			odma_reg_wait_sw_reset_status(id);
		}
		odma_reg_set_irq_mask_all(id, 0); /* irq unmask */
		odma_reg_set_irq_enable(id);
		odma_reg_set_in_qos_lut(id, 0, 0x44444444);
		odma_reg_set_in_qos_lut(id, 1, 0x44444444);
		odma_reg_set_dynamic_gating_en_all(id, 0);
		odma_reg_set_frame_alpha(id, 0xFF);
		odma_reg_set_ic_max(id, 0x40);
		odma_reg_set_pslave_err(id, 1);
	}
}

int dpp_reg_deinit(u32 id, bool reset, const unsigned long attr)
{
	if (test_bit(DPP_ATTR_SRAMC, &attr)) {
		sramc_reg_set_mode_enable(id, 0);
	}

	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_clear_irq(id, IDMA_ALL_IRQ_CLEAR);
		idma_reg_set_irq_mask_all(id, 1);
		idma_reg_set_votf_disable(id);
	}

	if (test_bit(DPP_ATTR_DPP, &attr)) {
		dpp_reg_clear_irq(id, DPP_ALL_IRQ_CLEAR);
		dpp_reg_set_irq_mask_all(id, 1);
	}

	if (test_bit(DPP_ATTR_ODMA, &attr)) {
		odma_reg_clear_irq(id, ODMA_ALL_IRQ_CLEAR);
		odma_reg_set_irq_mask_all(id, 1); /* irq mask */
	}

	if (reset) {
		if (test_bit(DPP_ATTR_IDMA, &attr) &&
				!test_bit(DPP_ATTR_DPP, &attr)) { /* IDMA */
			idma_reg_set_sw_reset(id);
			if (idma_reg_wait_sw_reset_status(id))
				return -1;
		} else if (test_bit(DPP_ATTR_IDMA, &attr) &&
				test_bit(DPP_ATTR_DPP, &attr)) { /* IDMA/DPP */
			idma_reg_set_sw_reset(id);
			dpp_reg_set_sw_reset(id);
			if (idma_reg_wait_sw_reset_status(id) ||
					dpp_reg_wait_sw_reset_status(id))
				return -1;
		} else if (test_bit(DPP_ATTR_ODMA, &attr)) { /* writeback */
			odma_reg_set_sw_reset(id);
			dpp_reg_set_sw_reset(id);
			if (odma_reg_wait_sw_reset_status(id) ||
					dpp_reg_wait_sw_reset_status(id))
				return -1;
		} else {
			cal_log_err(id, "not support attribute(0x%lx)\n", attr);
		}
	}

	if (test_bit(DPP_ATTR_IDMA, &attr) &&
			!test_bit(DPP_ATTR_DPP, &attr)) { /* IDMA */
		if (idma_reg_wait_idle_status(id))
			return -1;
	} else if (test_bit(DPP_ATTR_IDMA, &attr) &&
			test_bit(DPP_ATTR_DPP, &attr)) { /* IDMA/DPP */
		if (idma_reg_wait_idle_status(id) ||
				dpp_reg_wait_idle_status(id))
			return -1;
	} else if (test_bit(DPP_ATTR_ODMA, &attr)) { /* writeback */
		if (odma_reg_wait_idle_status(id) ||
				dpp_reg_wait_idle_status(id))
			return -1;
	}

	return 0;
}

#if defined(DMA_BIST)
static const u32 pattern_data[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0x000000ff,
	0x000000ff,
	0x000000ff,
	0x000000ff,
};
#endif

/* TODO: fix HDR */
void dpp_reg_configure_params(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	const struct dpu_fmt *fmt = dpu_find_fmt_info(p->format);

	if (test_bit(DPP_ATTR_SRAMC, &attr))
		sramc_reg_set_rsc_config(id, p);

	if (test_bit(DPP_ATTR_CSC, &attr) && IS_YUV(fmt))
		dpp_reg_set_csc_params(id, p->standard, p->range, attr);

	if (test_bit(DPP_ATTR_SCALE, &attr))
		dpp_reg_set_scale_ratio(id, p);

	/* configure coordinates and size of IDMA, DPP, ODMA and WB MUX */
	dma_dpp_reg_set_coordinates(id, p, attr);

	if (test_bit(DPP_ATTR_ROT, &attr) || test_bit(DPP_ATTR_FLIP, &attr))
		idma_reg_set_rotation(id, p->rot);

	/* configure base address of IDMA and ODMA */
	dma_reg_set_base_addr(id, p, attr);

	if (test_bit(DPP_ATTR_BLOCK, &attr))
		idma_reg_set_block_mode(id, p->is_block, p->block.x, p->block.y,
				p->block.w, p->block.h);

	/* configure image format of IDMA, DPP, ODMA and WB MUX */
	dma_dpp_reg_set_format(id, p, attr);

	if (test_bit(DPP_ATTR_SAJC, &attr) || test_bit(DPP_ATTR_SBWC, &attr))
		idma_reg_set_comp(id, p->comp_type, p->rcv_num, p->blk_size);

	/*
	 * To check HW stuck
	 * dead_lock min: 17ms (17ms: 1-frame time, rcv_time: 1ms)
	 * but, considered DVFS 3x level switch (ex: 200 <-> 600 Mhz)
	 */
	idma_reg_set_deadlock(id, 1, p->rcv_num * 51);

#if defined(DMA_BIST)
	idma_reg_set_test_pattern(id, 0, pattern_data);
#endif

	idma_reg_set_votf(id, p->votf_en, p->votf_buf_idx, p->rcv_num);

	/* vOTF output config */
	if (test_bit(DPP_ATTR_ODMA, &attr)) {
		if (p->votf_o_en) {
			votf_reg_set_tws(id, 0, p->votf_o_mfc_addr);
			votf_reg_set_mode(id, 0, 1); // tws0, half connection mode

			votf_reg_set_tws(id, 1, p->votf_o_mfc_addr);
			votf_reg_set_mode(id, 1, 1); // tws1, half connection mode

			votf_reg_set_tws(id, 2, p->votf_o_mfc_addr);
			votf_reg_set_mode(id, 2, 1); // tws2, half connection mode
		}
		odma_reg_set_votf(id, p->votf_o_en, p->votf_o_idx);
	}
}

u32 dpp_reg_get_irq_and_clear(u32 id)
{
	u32 val;

	val = dpp_read(id, DPP_COM_IRQ_STATUS);
	dpp_reg_print_irqs_msg(id, val);
	dpp_reg_clear_irq(id, val);

	return val;
}

/*
 * CFG_ERR is cleared when clearing pending bits
 * So, get cfg_err first, then clear pending bits
 */
u32 idma_reg_get_irq_and_clear(u32 id)
{
	u32 val;

	val = dma_read(id, RDMA_IRQ);
	idma_reg_print_irqs_msg(id, val);
	idma_reg_clear_irq(id, val);

	return val;
}

u32 odma_reg_get_irq_and_clear(u32 id)
{
	u32 val;

	val = dma_read(id, WDMA_IRQ);
	odma_reg_print_irqs_msg(id, val);
	odma_reg_clear_irq(id, val);

	return val;
}

void votf_reg_set_global_init(u32 fid, u32 votf_base_addr, bool en)
{
	u32 id = DPP_PER_DPUF * fid;

	votf_reg_set_ring_clock_enable(id, en);
	if (en) {
		votf_reg_set_ring_enable(id, 1);
		votf_reg_set_local_ip(id, votf_base_addr);
		votf_reg_set_immediate_mode(id, 1);
		votf_reg_set_a(id, 1);
	}
}

/* trs_idx is valid 8 ~ 15 for each VOTF in pamir. */
void votf_reg_init_trs(u32 fid)
{
	u32 id = DPP_PER_DPUF * fid;
	u32 trs_id, i;

	for (i = 8; i < 16; ++i) {
		trs_id = i % DPP_PER_DPUF + DPP_PER_DPUF;

		votf_write(id, C2S_TRS_ENABLE(trs_id), VOTF_TRS_EN);
		votf_write(id, C2S_CON_LOST_RECOVER_CONFIG(trs_id),
				VOTF_CON_LOST_FLUSH_EN | VOTF_CON_LOST_RECOVER_EN);
		votf_write(id, C2S_TRS_LIMIT(trs_id), VOTF_TRS_LIMIT_NUM);
		votf_write(id, C2S_TOKEN_CROP_START(trs_id), 0);
		votf_write(id, C2S_CROP_ENABLE(trs_id), 0);
		votf_write(id, C2S_LINE_IN_FIRST_TOKEN(trs_id), 0x01);
		votf_write(id, C2S_LINE_IN_TOKEN(trs_id), 0x01);
		votf_write(id, C2S_LINE_COUNT(trs_id), VOTF_LINE_CNT);
	}
}

void idma_reg_set_votf_disable(u32 id)
{
	dma_write_mask(id, RDMA_VOTF_EN, IDMA_VOTF_DISABLE, IDMA_VOTF_EN_MASK);
}

void dma_reg_get_shd_addr(u32 id, u32 shd_addr[], const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		shd_addr[0] = dma_read(id, RDMA_BASEADDR_P0 + DMA_SHD_OFFSET);
		shd_addr[1] = dma_read(id, RDMA_BASEADDR_P1 + DMA_SHD_OFFSET);
		shd_addr[2] = dma_read(id, RDMA_BASEADDR_P2 + DMA_SHD_OFFSET);
		shd_addr[3] = dma_read(id, RDMA_BASEADDR_P3 + DMA_SHD_OFFSET);
	} else if (test_bit(DPP_ATTR_ODMA, &attr)) {
		shd_addr[0] = dma_read(id, WDMA_BASEADDR_P0 + DMA_SHD_OFFSET);
		shd_addr[1] = dma_read(id, WDMA_BASEADDR_P1 + DMA_SHD_OFFSET);
		shd_addr[2] = dma_read(id, WDMA_BASEADDR_P2 + DMA_SHD_OFFSET);
		shd_addr[3] = dma_read(id, WDMA_BASEADDR_P3 + DMA_SHD_OFFSET);
	}
	cal_log_debug(id, "shadow addr 1p(0x%x) 2p(0x%x) 3p(0x%x) 4p(0x%x)\n",
			shd_addr[0], shd_addr[1], shd_addr[2], shd_addr[3]);
}

static void dpp_reg_dump_ch_data(int id, enum dpp_reg_area reg_area,
					const u32 sel[], u32 cnt)
{
	/* TODO: This will be implemented in the future */
}

static void dma_reg_dump_com_debug_regs(int id)
{
	static bool checked;
	const u32 sel_glb[99] = {
		0x0000, 0x0001, 0x0005, 0x0009, 0x000D, 0x000E, 0x0020, 0x0021,
		0x0025, 0x0029, 0x002D, 0x002E, 0x0040, 0x0041, 0x0045, 0x0049,
		0x004D, 0x004E, 0x0060, 0x0061, 0x0065, 0x0069, 0x006D, 0x006E,
		0x0080, 0x0081, 0x0082, 0x0083, 0x00C0, 0x00C1, 0x00C2, 0x00C3,
		0x0100, 0x0101, 0x0200, 0x0201, 0x0202, 0x0300, 0x0301, 0x0302,
		0x0303, 0x0304, 0x0400, 0x4000, 0x4001, 0x4005, 0x4009, 0x400D,
		0x400E, 0x4020, 0x4021, 0x4025, 0x4029, 0x402D, 0x402E, 0x4040,
		0x4041, 0x4045, 0x4049, 0x404D, 0x404E, 0x4060, 0x4061, 0x4065,
		0x4069, 0x406D, 0x406E, 0x4100, 0x4101, 0x4200, 0x4201, 0x4300,
		0x4301, 0x4302, 0x4303, 0x4304, 0x4400, 0x8080, 0x8081, 0x8082,
		0x8083, 0x80C0, 0x80C1, 0x80C2, 0x80C3, 0x8100, 0x8101, 0x8201,
		0x8202, 0x8300, 0x8301, 0x8302, 0x8303, 0x8304, 0x8400, 0xC000,
		0xC001, 0xC002, 0xC005
	};

	cal_log_info(id, "%s: checked = %d\n", __func__, checked);
	if (checked)
		return;

	cal_log_info(id, "-< DMA COMMON DEBUG SFR >-\n");
	dpp_reg_dump_ch_data(id, REG_AREA_DMA_COM, sel_glb, 99);

	checked = true;
}

static void dma_reg_dump_debug_regs(int id)
{
	/* TODO: This will be implemented in EVT1 */
}

static void dpp_reg_dump_debug_regs(int fid, void __iomem *debug_regs)
{
	cal_log_info(fid, "=== DPUF%d DEBUG SFR DUMP ===\n", fid);
	dpu_print_hex_dump(debug_regs, debug_regs + 0x0000, 0x48);
}

static void scl_coef_dump_regs(int fid, void __iomem *scl_coef_base_regs)
{
	int i;

	cal_log_info(fid, "=== DPUF%d SCALE COEF SFR DUMP ===\n", fid);
	for (i = 0; i < 4; i++)
		dpu_print_hex_dump(scl_coef_base_regs, scl_coef_base_regs +
				SCL_COEF_OFFS(i), 0x20);

	cal_log_info(fid, "=== DPUF%d SCALE COEF SHADOW SFR DUMP ===\n", fid);
	for (i = 0; i < 4; i++)
		dpu_print_hex_dump(scl_coef_base_regs, scl_coef_base_regs +
				SCL_COEF_OFFS(i) + DPP_SCL_SHD_OFFSET, 0x20);
}

static void votf_dump_regs(int fid, void __iomem *votf_base_regs)
{
	int i;

	cal_log_info(fid, "=== DPUF%d GLOBAL VOTF SFR DUMP ===\n", fid);
	dpu_print_hex_dump(votf_base_regs, votf_base_regs, 0x30);

	/* tws_idx is valid 0 ~ 2 for each VOTF in pamir. */
	for (i = 0; i < 3; ++i) {
		cal_log_info(fid, "=== TWS%d VOTF SFR DUMP ===\n", fid);
		dpu_print_hex_dump(votf_base_regs,
				votf_base_regs + 0x100 + TWS_OFFSET(i), 0x1c);
	}

	/* trs_idx is valid 8 ~ 15 for each VOTF in pamir. */
	for (i = 8; i < 16; ++i) {
		cal_log_info(fid, "=== TRS%d VOTF SFR DUMP ===\n", i);
		dpu_print_hex_dump(votf_base_regs,
				votf_base_regs + 0x300 + TRS_OFFSET(i), 0x2c);
	}
}

static void dma_dump_regs(u32 id, void __iomem *dma_regs)
{
	cal_log_info(id, "\n=== DPU_DMA%d SFR DUMP ===\n", id);

	dpu_print_hex_dump(dma_regs, dma_regs + 0x0000, 0x80);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0100, 0x44);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0180, 0x10);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0200, 0x8);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0300, 0x24);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0730, 0x20);

	cal_log_info(id, "=== DPU_DMA%d SHADOW SFR DUMP ===\n", id);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0000 + DMA_SHD_OFFSET, 0x80);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0100 + DMA_SHD_OFFSET, 0x44);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0180 + DMA_SHD_OFFSET, 0x10);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0200 + DMA_SHD_OFFSET, 0x8);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0300 + DMA_SHD_OFFSET, 0x24);
	dpu_print_hex_dump(dma_regs, dma_regs + 0x0730 + DMA_SHD_OFFSET, 0x20);
}

static void dpp_dump_regs(u32 id, void __iomem *regs)
{
	cal_log_info(id, "=== DPP%d SFR DUMP ===\n", id);
	dpu_print_hex_dump(regs, regs + 0x0000, 0x9C);

	cal_log_info(id, "=== DPP%d SHADOW SFR DUMP ===\n", id);
	dpu_print_hex_dump(regs, regs + DPP_COM_SHD_OFFSET, 0x9C);
}

static void sramc_dump_regs(u32 id, void __iomem *regs)
{
	cal_log_info(id, "\n=== SRAMC%d SFR DUMP ===\n", id);
	dpu_print_hex_dump(regs, regs + 0x0000, 0x20);

	cal_log_info(id, "=== SRAMC%d SHADOW SFR DUMP ===\n", id);
	dpu_print_hex_dump(regs, regs + 0x0000 + SRAMC_L_COM_SHD_OFFSET, 0x20);
}

static void hdrc_dump_regs(u32 id, void __iomem *regs)
{
	cal_log_info(id, "\n=== HDR_COMM_L%d SFR DUMP ===\n", id);
	dpu_print_hex_dump(regs, regs + 0x0000, 0x60);

	cal_log_info(id, "=== HDR_COMM_L%d SHADOW SFR DUMP ===\n", id);
	dpu_print_hex_dump(regs, regs + LSI_COMM_SHD_OFFSET, 0x60);
}

#if 0
static void hdr_dump_regs(u32 id, void __iomem *regs)
{
	cal_log_info(id, "\n=== HDR%d SFR DUMP ===\n", id);
	dpu_print_hex_dump(regs + 0x0000, 0xC);
	if (id == ID_L1 || id == ID_L1 || id == ID_L1) {
		dpu_print_hex_dump(regs + 0x000C, 0x104); // OETF_x
		dpu_print_hex_dump(regs + 0x0110, 0x84); // OETF_y
		dpu_print_hex_dump(regs + 0x0194, 0x84); // EOTF_x
		dpu_print_hex_dump(regs + 0x0214, 0x104); // EOTF_y
		dpu_print_hex_dump(regs + 0x031C, 0x8); // NORMAL
		dpu_print_hex_dump(regs + 0x0324, 0x24); // GM_coef
		dpu_print_hex_dump(regs + 0x0348, 0xC); // GM_offs

		dpu_print_hex_dump(regs + 0x0354, 0xC); // TM_coef
		dpu_print_hex_dump(regs + 0x0360, 0x44); // TM_x
		dpu_print_hex_dump(regs + 0x03E4, 0x84); // TM_y
	} else {
		dpu_print_hex_dump(regs + 0x000C, 0x44); // OETF_x
		dpu_print_hex_dump(regs + 0x0050, 0x44); // OETF_y
		dpu_print_hex_dump(regs + 0x0094, 0x104); // EOTF_x
		dpu_print_hex_dump(regs + 0x0198, 0x204); // EOTF_y
		dpu_print_hex_dump(regs + 0x039C, 0x24); // GM_coef
		dpu_print_hex_dump(regs + 0x03C0, 0xC); // GM_offs
	}

	cal_log_info(id, "=== HDR%d SHADOW SFR DUMP ===\n", id);
	dpu_print_hex_dump(regs + 0x0000 + HDR_SHD_OFFSET, 0xC);
	if (id == ID_L1 || id == ID_L1 || id == ID_L1) {
		dpu_print_hex_dump(regs + 0x000C + HDR_SHD_OFFSET, 0x104); // OETF_x
		dpu_print_hex_dump(regs + 0x0110 + HDR_SHD_OFFSET, 0x84); // OETF_y
		dpu_print_hex_dump(regs + 0x0194 + HDR_SHD_OFFSET, 0x84); // EOTF_x
		dpu_print_hex_dump(regs + 0x0214 + HDR_SHD_OFFSET, 0x104); // EOTF_y
		dpu_print_hex_dump(regs + 0x031C + HDR_SHD_OFFSET, 0x8); // NORMAL
		dpu_print_hex_dump(regs + 0x0324 + HDR_SHD_OFFSET, 0x24); // GM_coef
		dpu_print_hex_dump(regs + 0x0348 + HDR_SHD_OFFSET, 0xC); // GM_offs

		dpu_print_hex_dump(regs + 0x0354 + HDR_SHD_OFFSET, 0xC); // TM_coef
		dpu_print_hex_dump(regs + 0x0360 + HDR_SHD_OFFSET, 0x44); // TM_x
		dpu_print_hex_dump(regs + 0x03E4 + HDR_SHD_OFFSET, 0x84); // TM_y
	} else {
		dpu_print_hex_dump(regs + 0x000C + HDR_SHD_OFFSET, 0x44); // OETF_x
		dpu_print_hex_dump(regs + 0x0050 + HDR_SHD_OFFSET, 0x44); // OETF_y
		dpu_print_hex_dump(regs + 0x0094 + HDR_SHD_OFFSET, 0x104); // EOTF_x
		dpu_print_hex_dump(regs + 0x0198 + HDR_SHD_OFFSET, 0x204); // EOTF_y
		dpu_print_hex_dump(regs + 0x039C + HDR_SHD_OFFSET, 0x24); // GM_coef
		dpu_print_hex_dump(regs + 0x03C0 + HDR_SHD_OFFSET, 0xC); // GM_offs
	}
}
#endif

void __dpp_common_dump(u32 fid, struct dpp_regs *regs)
{
	void __iomem *dpp_debug_base_regs = regs->dpp_debug_base_regs;
	void __iomem *scl_coef_base_regs = regs->scl_coef_base_regs;
	void __iomem *votf_base_regs = regs->votf_base_regs;

	dma_reg_dump_com_debug_regs(fid);
	dma_reg_dump_debug_regs(fid);
	if (dpp_debug_base_regs)
		dpp_reg_dump_debug_regs(fid, dpp_debug_base_regs);

	if (scl_coef_base_regs)
		scl_coef_dump_regs(fid, scl_coef_base_regs);

	if (votf_base_regs)
		votf_dump_regs(fid, votf_base_regs);
}

void __dpp_dump(u32 id, struct dpp_regs *regs)
{
	void __iomem *dpp_regs = regs->dpp_base_regs;
	void __iomem *dma_regs = regs->dma_base_regs;
	void __iomem *sramc_regs = regs->sramc_base_regs;
	void __iomem *hdr_comm_regs = regs->hdr_comm_base_regs;

	dma_dump_regs(id, dma_regs);

	dpp_dump_regs(id, dpp_regs);

	if (sramc_regs)
		sramc_dump_regs(id, sramc_regs);

	if (hdr_comm_regs)
		hdrc_dump_regs(id, hdr_comm_regs);
}

void __dpp_init_dpuf_resource_cnt(void)
{
	struct dpp_device *dpp0 = get_dpp_drvdata(0);
	u32 fid;

	if (dpp0->dpuf_resource.check) {
		for (fid = 0; fid < MAX_DPUF_CNT; fid++)
			memset(&cur_req_rsc[fid], 0, sizeof(struct dpp_dpuf_req_rsc));
	}
}

void __dpp_print_rsc_cnt(u32 id)
{
	struct dpp_device *dpp0 = get_dpp_drvdata(0);
	struct dpp_dpuf_resource *dpuf_rsc = &dpp0->dpuf_resource;
	u32 fid;

	if (!dpp0->dpuf_resource.check)
		return;

	cal_log_info(id, "[hw/sram resouce for fid]\n");
	cal_log_info(id, "sajc(%d) sbwc(%d) rotator(%d) scaler(%d) csc(%d) sram(%d)\n",
		dpuf_rsc->sajc, dpuf_rsc->sbwc,
		dpuf_rsc->rot, dpuf_rsc->scl,
		dpuf_rsc->itp_csc, dpuf_rsc->sram);

	for (fid = 0; fid < MAX_DPUF_CNT; fid++) {
		cal_log_info(id, "[requested resouce for fid(%d)]\n", fid);
		cal_log_info(id, "sajc(%d) sbwc(%d) rotator(%d) scaler(%d) csc(%d) sram(%d)%c\n",
			cur_req_rsc[fid].sajc, cur_req_rsc[fid].sbwc,
			cur_req_rsc[fid].rot, cur_req_rsc[fid].scl,
			cur_req_rsc[fid].itp_csc, cur_req_rsc[fid].sram,
			((fid == (MAX_DPUF_CNT - 1))? '\n': '\0'));
	}
}

#define SRAM_LB_WIDTH_512	512
#define SRAM_LB_WIDTH_1024	1024
#define SRAM_LB_WIDTH_1536	1536
#define SRAM_LB_WIDTH_2048	2048
#define SRAM_LB_WIDTH_2304	2304
#define SRAM_LB_WIDTH_2560	2560
#define SRAM_LB_WIDTH_3072	3072
#define SRAM_LB_WIDTH_4096	4096
/*
 * This function corresponds to 9925 evt0.
 * For 9925 evt1, this function should be changed.
 */
int __dpp_check(u32 id, const struct dpp_params_info *p, unsigned long attr)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	struct dpp_device *dpp0 = get_dpp_drvdata(0);
	const struct dpu_fmt *fmt = dpu_find_fmt_info(p->format);
	u32 fid = (dpp->id >= DPP_PER_DPUF) ? 1 : 0;
	struct dpp_dpuf_resource *dpuf_rsc;
	u32 src_w;

	if (p->rot & DPP_ROT) {
		if (!IS_YUV(fmt)) {
			cal_log_err(id, "Rotation is supported for only YUV format\n");
			return -ENOTSUPP;
		}
	}

#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
	/* sbwc & flip will be supported from evt1 */
	if ((p->comp_type == COMP_TYPE_SBWC) || (p->comp_type == COMP_TYPE_SBWCL)) {
		if ((p->rot > DPP_ROT_NORMAL) && (p->rot < DPP_ROT_90)) {
			cal_log_err(id, "SBWC & FLIP is not supported at the same time in DPP%d\n");
			return -ENOTSUPP;
		}
	}
#endif

	if (test_bit(DPP_ATTR_SRAMC, &attr) && (dpp0->dpuf_resource.check)) {
		src_w = (p->rot >= DPP_ROT_90)? p->src.h: p->src.w;

		dpuf_rsc = &dpp0->dpuf_resource;

		if (p->comp_type == COMP_TYPE_SAJC) {
			cur_req_rsc[fid].sajc++;
			if (cur_req_rsc[fid].sajc > dpuf_rsc->sajc) {
				cal_log_err(id, "Total SAJC resources exceeded\n");
				goto err;
			}

			if (src_w <= SRAM_LB_WIDTH_1024)
				cur_req_rsc[fid].sram += 4;
			else if (src_w <= SRAM_LB_WIDTH_2304)
				cur_req_rsc[fid].sram += 8;
			else if (src_w <= SRAM_LB_WIDTH_4096)
				cur_req_rsc[fid].sram += 16;
			else
				cal_log_err(id, "not supported source size range\n");
		}

		if ((p->comp_type == COMP_TYPE_SBWC) ||
			(p->comp_type == COMP_TYPE_SBWCL)) {
			cur_req_rsc[fid].sbwc++;
			if (cur_req_rsc[fid].sbwc > dpuf_rsc->sbwc) {
				cal_log_err(id, "Total SBWC resources exceeded\n");
				goto err;
			}

			/* To consider sram for only rotator is enough in case of both compression & rotator */
			if (p->rot < DPP_ROT_90) {
				if (src_w <= SRAM_LB_WIDTH_2304)
					cur_req_rsc[fid].sram += 3;
				else if (src_w <= SRAM_LB_WIDTH_4096)
					cur_req_rsc[fid].sram += 4;
				else
					cal_log_err(id, "not supported source size range\n");
			}
		}

		if (p->rot >= DPP_ROT_90) {
			cur_req_rsc[fid].rot++;
			if (cur_req_rsc[fid].rot > dpuf_rsc->rot) {
				cal_log_err(id, "Total rotator resources exceeded\n");
				goto err;
			}

			if (src_w <= SRAM_LB_WIDTH_512)
				cur_req_rsc[fid].sram += ((fmt->bpp == 15)? 4: 6);
			else if (src_w <= SRAM_LB_WIDTH_1024)
				cur_req_rsc[fid].sram += ((fmt->bpp == 15)? 6: 12);
			else if (src_w <= SRAM_LB_WIDTH_2048)
				cur_req_rsc[fid].sram += ((fmt->bpp == 15)? 12: 24);
			else if (src_w <= SRAM_LB_WIDTH_4096)
				cur_req_rsc[fid].sram += ((fmt->bpp == 15)? 15: 28);
			else
				cal_log_err(id, "not supported source size range\n");
		}

		if (p->is_scale) {
			cur_req_rsc[fid].scl++;
			if (cur_req_rsc[fid].scl > dpuf_rsc->scl) {
				cal_log_err(id, "Total scaler resources exceeded\n");
				goto err;
			}

			if (fmt->cs == DPU_COLORSPACE_RGB && fmt->len_alpha > 0)
				cur_req_rsc[fid].sram += 16;
			else
				cur_req_rsc[fid].sram += 12;
		}

		if (IS_YUV(fmt)) {
			cur_req_rsc[fid].itp_csc++;
			if (cur_req_rsc[fid].itp_csc > dpuf_rsc->itp_csc) {
				cal_log_err(id, "Total itp_csc resources exceeded\n");
				goto err;
			}

			cur_req_rsc[fid].sram += 2;
		}

		if (cur_req_rsc[fid].sram > dpuf_rsc->sram) {
			cal_log_err(id, "Total sram resources exceeded\n");
			goto err;
		}
		//__dpp_print_rsc_cnt(id);
	}

	return 0;

err:
	cal_log_err(id, "HW/SRAM resources exceeded!!!\n");
	__dpp_print_rsc_cnt(id);
	__dpp_init_dpuf_resource_cnt();

	return -ENOTSUPP;
}
