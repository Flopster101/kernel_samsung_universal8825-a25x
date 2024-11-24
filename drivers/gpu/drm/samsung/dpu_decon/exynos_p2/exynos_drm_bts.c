// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * BTS file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <exynos_drm_decon.h>
#include <exynos_drm_bts.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_writeback.h>
#include <exynos_drm_format.h>
#include <exynos_drm_debug.h>

#include <soc/samsung/bts.h>
#include <soc/samsung/exynos-devfreq.h>
#include <dt-bindings/soc/samsung/s5e8825-devfreq.h>
#include <soc/samsung/exynos-pd.h>

#if defined(CONFIG_CAL_IF)
#include <soc/samsung/cal-if.h>
#endif

#define DISP_FACTOR		100UL
#define DISP_REFRESH_RATE	60U
#define MULTI_FACTOR		(1UL << 10)
#define ROT_READ_LINE		(32)

#define ACLK_100MHZ_PERIOD	10000UL
#define FRAME_TIME_NSEC		1000000000UL	/* 1sec */
#define BTS_INFO_PRINT_BLOCK_TIMEOUT (5000)

static int dpu_bts_log_level = 6;
module_param(dpu_bts_log_level, int, 0600);
MODULE_PARM_DESC(dpu_bts_log_level, "log level for dpu bts [default : 6]");

#define DPU_DEBUG_BTS(decon, fmt, ...)	\
	dpu_pr_debug("BTS", (decon)->id, dpu_bts_log_level, fmt, ##__VA_ARGS__)

#define DPU_WARN_BTS(decon, fmt, ...)	\
	dpu_pr_warn("BTS", (decon)->id, dpu_bts_log_level, fmt, ##__VA_ARGS__)

#define DPU_INFO_BTS(decon, fmt, ...)	\
	dpu_pr_info("BTS", (decon)->id, dpu_bts_log_level, fmt, ##__VA_ARGS__)

#define DPU_ERR_BTS(decon, fmt, ...)	\
	dpu_pr_err("BTS", (decon)->id, dpu_bts_log_level, fmt, ##__VA_ARGS__)

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
/* Workaround min lock for LPM & Recovery Scenario */
static int boot_mode;
module_param(boot_mode, int, 0444);
/* bootm value definitions.
 * #define SYS_BOOTM_NORMAL	(0x0<<16)
 * #define SYS_BOOTM_UP		(0x1<<16)
 * #define SYS_BOOTM_DOWN		(0x2<<16)
 * #define SYS_BOOTM_LPM		(0x4<<16)
 * #define SYS_BOOTM_RECOVERY	(0x8<<16)
 * #define SYS_BOOTM_FASTBOOT	(0x10<<16)
 */
#define MIF_MINLOCK_FOR_LPM_RECOVERY (676 * 1000)

static inline bool dpu_bts_is_normal_boot(void)
{
	return (boot_mode >> 16) ? false : true;
}
#endif

enum mif_update_dir {
	SET_TO_STATIC = 0,
	GET_FROM_STATIC,
};

/* unit : usec x 1000 -> 5592 (5.592us) for WQHD+ case */
static inline u32 dpu_bts_get_one_line_time(struct decon_device *decon)
{
	u32 tot_v;
	int tmp;

	tot_v = decon->bts.vbp + decon->bts.vfp + decon->bts.vsa;
	tot_v += decon->config.image_height;
	tmp = DIV_ROUND_UP(FRAME_TIME_NSEC, decon->bts.fps);

	return (tmp / tot_v);
}

/* lmc : line memory count (usually 4) */
static inline u32 dpu_bts_comp_latency(u32 src_w, u32 ppc, u32 lmc)
{
	return mult_frac(src_w, lmc, ppc);
}

/*
 * line memory max size : 4096
 * lmc : line memory count (usually 4)
 */
static inline u32 dpu_bts_scale_latency(u32 src_w, u32 dst_w,
		u32 ppc, u32 lmc)
{
	if (src_w > dst_w)
		return mult_frac(src_w * lmc, src_w, dst_w * ppc);
	else
		return DIV_ROUND_CLOSEST(src_w * lmc, ppc);
}

/*
 * 1-read
 *  - 8bit/10bit w/ compression : 32 line
 *  - 8bit w/o compression : 64 line
 *  - 10bit w/o compression : 32 line
 */
static inline u32
dpu_bts_rotate_read_line(bool is_comp, u32 format)
{
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(format);
	u32 read_line = ROT_READ_LINE;

	if (!is_comp && (fmt_info->bpc == 8))
		read_line = (2 * ROT_READ_LINE);

	return read_line;
}

/*
 * rotator ppc is usually 4 or 8
 */
static inline u32
dpu_bts_rotate_latency(u32 src_w, u32 r_ppc, bool is_comp, u32 format)
{
	u32 read_line;

	read_line = dpu_bts_rotate_read_line(is_comp, format);

	return (src_w * (read_line / r_ppc));
}

/*
 * [DSC]
 * Line memory is necessary like followings.
 *  1EA(1ppc) : 2-line for 2-slice, 1-line for 1-slice
 *  2EA(2ppc) : 3.5-line for 4-slice (DSCC 0.5-line + DSC 3-line)
 *        2.5-line for 2-slice (DSCC 0.5-line + DSC 2-line)
 *
 * [DECON] none
 * When 1H is filled at OUT_FIFO, it immediately transfers to DSIM.
 */
static inline u32
dpu_bts_dsc_latency(u32 slice_num, u32 dsc_cnt,
		u32 dst_w, u32 ppc)
{
	u32 lat_dsc = dst_w;

	switch (slice_num) {
	case 1:
		/* DSC: 1EA */
		lat_dsc = dst_w * 1;
		break;
	case 2:
		if (dsc_cnt == 1)
			lat_dsc = dst_w * 2;
		else
			lat_dsc = (dst_w * 25) / (10 * ppc);
		break;
	case 4:
		/* DSC: 2EA */
		lat_dsc = (dst_w * 35) / (10 * ppc);
		break;
	default:
		break;
	}

	return lat_dsc;
}

/*
 * unit : nsec x 1000
 * reference aclk : 100MHz (-> 10ns x 1000)
 * # cycles = usec * aclk_mhz
 */
static inline u32 dpu_bts_convert_aclk_to_ns(u32 aclk_mhz)
{
	return ((ACLK_100MHZ_PERIOD * 100) / aclk_mhz);
}

/*
 * This function is introduced due to VRR feature.
 * return : kHz value based on 1-pixel processing pipe-line
 */
static u64 dpu_bts_get_resol_clock(u32 xres, u32 yres, u32 fps)
{
	u32 op_fps;
	u64 margin;
	u64 resol_khz;

	/*
	 * check lower limit of fps
	 * this can be removed if there is no stuck issue
	 */
	op_fps = (fps < DISP_REFRESH_RATE) ? DISP_REFRESH_RATE : fps;

	/*
	 * aclk_khz = vclk_1pix * ( 1.1 + (48+20)/WIDTH ) : x1000
	 * @ (1.1)   : BUS Latency Considerable Margin (10%)
	 * @ (48+20) : HW bubble cycles
	 *      - 48 : 12 cycles per slice, total 4 slice
	 *      - 20 : hblank cycles for other HW module
	 */
	margin = 1100 + (48000 + 20000) / xres;
	/* convert to kHz unit */
	resol_khz = (xres * yres * (u64)op_fps * margin / 1000) / 1000;

	return resol_khz;
}

static u32 dpu_bts_get_vblank_time_ns(struct decon_device *decon)
{
	u32 line_t_ns, v_blank_t_ns;

	line_t_ns = dpu_bts_get_one_line_time(decon);
	if (decon->config.mode.op_mode == DECON_VIDEO_MODE)
		v_blank_t_ns = (decon->bts.vbp + decon->bts.vfp) *
					line_t_ns;
	else {
		if (decon->bts.v_blank_t)
			v_blank_t_ns = decon->bts.v_blank_t * 1000U;
		else
			v_blank_t_ns = (decon->bts.vbp + decon->bts.vfp) *
					line_t_ns;
	}

	/* v_blank should be over minimum v total porch */
	if (v_blank_t_ns < (3 * line_t_ns)) {
		v_blank_t_ns = 3 * line_t_ns;
		DPU_DEBUG_BTS(decon, "\t-WARN: v_blank_t_ns is abnormal!(-> %d%)\n",
				v_blank_t_ns);
	}

	DPU_DEBUG_BTS(decon, "\t-line_t_ns(%d) v_blank_t_ns(%d)\n",
			line_t_ns, v_blank_t_ns);

	return v_blank_t_ns;
}

static u32
dpu_bts_find_nearest_high_freq(struct decon_device *decon, u32 aclk_base)
{
	int i;

	for (i = (decon->bts.dfs_lv_cnt - 1); i >= 0; i--) {
		if (aclk_base <= decon->bts.dfs_lv[i])
			break;
	}
	if (i < 0) {
		DPU_DEBUG_BTS(decon, "\taclk_base is over L0 frequency!");
		i = 0;
	}
	DPU_DEBUG_BTS(decon, "\tNearest DFS: %d KHz @L%d\n", decon->bts.dfs_lv[i], i);

	return i;
}

/*
 * [caution] src_w/h is rotated size info
 * - src_w : src_h @original input image
 * - src_h : src_w @original input image
 */
static u64
dpu_bts_calc_rotate_cycle(struct decon_device *decon, u32 aclk_base,
		u32 ppc, u32 format, u32 src_w, u32 dst_w,
		bool is_comp, bool is_scale, bool is_dsc,
		u32 *module_cycle, u32 *basic_cycle)
{
	u32 dfs_idx = 0;
	u32 dsi_cycle, base_cycle, temp_cycle = 0;
	u32 comp_cycle = 0, rot_cycle = 0, scale_cycle = 0, dsc_cycle = 0;
	u64 dfs_aclk;

	DPU_DEBUG_BTS(decon, "BEFORE latency check\n");
	DPU_DEBUG_BTS(decon, "\tACLK: %d KHz\n", aclk_base);

	dfs_idx = dpu_bts_find_nearest_high_freq(decon, aclk_base);
	dfs_aclk = decon->bts.dfs_lv[dfs_idx];

	/* post DECON OUTFIFO based on 1H transfer */
	dsi_cycle = decon->config.image_width;

	/* get additional pipeline latency */
	rot_cycle = dpu_bts_rotate_latency(src_w,
		decon->bts.ppc_rotator, is_comp, format);
	DPU_DEBUG_BTS(decon, "\tROT: lat_cycle(%d)\n", rot_cycle);
	temp_cycle += rot_cycle;
	if (is_comp) {
		comp_cycle = dpu_bts_comp_latency(src_w, ppc,
			decon->bts.delay_comp);
		DPU_DEBUG_BTS(decon, "\tCOMP: lat_cycle(%d)\n", comp_cycle);
		temp_cycle += comp_cycle;
	}
	if (is_scale) {
		scale_cycle = dpu_bts_scale_latency(src_w, dst_w,
			decon->bts.ppc_scaler, decon->bts.delay_scaler);
		DPU_DEBUG_BTS(decon, "\tSCALE: lat_cycle(%d)\n", scale_cycle);
		temp_cycle += scale_cycle;
	}
	if (is_dsc) {
		dsc_cycle = dpu_bts_dsc_latency(decon->config.dsc.slice_count,
			decon->config.dsc.dsc_count, dst_w, ppc);
		DPU_DEBUG_BTS(decon, "\tDSC: lat_cycle(%d)\n", dsc_cycle);
		temp_cycle += dsc_cycle;
		dsi_cycle = (dsi_cycle + 2) / 3;
	}

	/*
	 * basic cycle(+ bubble: 10%) + additional cycle based on function
	 * cycle count increases when ACLK goes up due to other conditions
	 * At latency monitor experiment using unit test,
	 *  cycles at 400Mhz were increased by about 800 compared to 200Mhz.
	 * Here, (aclk_mhz * 2) cycles are reflected referring to the result
	 *  because the exact value is unknown.
	 */
	base_cycle = (decon->config.image_width * 11 / 10 + dsi_cycle) / ppc;

	DPU_DEBUG_BTS(decon, "AFTER latency check\n");
	DPU_DEBUG_BTS(decon, "\tACLK: %d KHz\n", dfs_aclk);
	DPU_DEBUG_BTS(decon, "\tMODULE: module_cycle(%d)\n", temp_cycle);
	DPU_DEBUG_BTS(decon, "\tBASIC: basic_cycle(%d)\n", base_cycle);

	*module_cycle = temp_cycle;
	*basic_cycle = base_cycle;

	return dfs_aclk;
}

static u32
dpu_bts_get_rotate_tx_allow_t(struct decon_device *decon, u32 rot_clk,
		u32 module_cycle, u32 basic_cycle, u32 dst_y, u32 *dpu_lat_t)
{
	u32 dpu_cycle;
	u32 aclk_x_1k_ns, dpu_lat_t_ns, max_lat_t_ns, tx_allow_t_ns;
	s32 start_margin_t_ns;

	dpu_cycle = (basic_cycle + module_cycle) + rot_clk * 2 / 1000U;
	aclk_x_1k_ns = dpu_bts_convert_aclk_to_ns(rot_clk / 1000U);
	dpu_lat_t_ns = (dpu_cycle * aclk_x_1k_ns) / 1000U;
	start_margin_t_ns = (s32)dpu_bts_get_one_line_time(decon) * (dst_y - 1);
	max_lat_t_ns = dpu_bts_get_vblank_time_ns(decon) + start_margin_t_ns;
	if (max_lat_t_ns > dpu_lat_t_ns) {
		tx_allow_t_ns = max_lat_t_ns - dpu_lat_t_ns;
	} else {
		tx_allow_t_ns = max_lat_t_ns;
		DPU_DEBUG_BTS(decon,
				"\tWARN: latency calc result is over tx_allow_t_ns!\n");
	}
	tx_allow_t_ns = tx_allow_t_ns * decon->bts.rot_util / 100;
	DPU_DEBUG_BTS(decon, "\t-dpu_cycle(%d) aclk_x_1k_ns(%d)\n",
			dpu_cycle, aclk_x_1k_ns);
	DPU_DEBUG_BTS(decon, "\t-dpu_lat_t_ns(%d) tx_allow_t_ns(%d)\n",
			dpu_lat_t_ns, tx_allow_t_ns);

	*dpu_lat_t = dpu_lat_t_ns;
	return tx_allow_t_ns;
}

static u64 dpu_bts_calc_rotate_aclk(struct decon_device *decon, u32 aclk_base,
		u32 ppc, u32 format, u32 src_w, u32 dst_w, u32 dst_y,
		bool is_comp, bool is_scale, bool is_dsc)
{
	u32 dfs_idx = 0;
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(format);
	u32 bpp;
	u64 rot_clk;
	u32 module_cycle, basic_cycle;
	u32 rot_read_line;
	u32 rot_init_bw = 0;
	u64 rot_need_clk;
	u32 dpu_lat_t_ns, tx_allow_t_ns;
	u32 temp_clk;
	bool retry_flag = false;

	DPU_DEBUG_BTS(decon, "[ROT+] BEFORE latency check: %d KHz\n", aclk_base);

	dfs_idx = dpu_bts_find_nearest_high_freq(decon, aclk_base);
	bpp = fmt_info->bpp + fmt_info->padding;
	rot_clk = dpu_bts_calc_rotate_cycle(decon, aclk_base, ppc, format,
			src_w, dst_w, is_comp, is_scale, is_dsc,
			&module_cycle, &basic_cycle);
	rot_read_line = dpu_bts_rotate_read_line(is_comp, format);

retry_hi_freq:
	tx_allow_t_ns = dpu_bts_get_rotate_tx_allow_t(decon, rot_clk,
			module_cycle, basic_cycle, dst_y, &dpu_lat_t_ns);
	rot_init_bw = (u64)src_w * rot_read_line * bpp / 8 * 1000U * 1000U /
				tx_allow_t_ns;
	rot_need_clk = rot_init_bw / decon->bts.bus_width;

	if (rot_need_clk > rot_clk) {
		/* not max level */
		if ((int)dfs_idx > 0) {
			/* check if calc_clk is greater than 1-step */
			dfs_idx--;
			temp_clk = decon->bts.dfs_lv[dfs_idx];
			if ((rot_need_clk > temp_clk) && (!retry_flag)) {
				DPU_DEBUG_BTS(decon, "\t-allow_ns(%d) dpu_ns(%d)\n",
					tx_allow_t_ns, dpu_lat_t_ns);
				rot_clk = temp_clk;
				retry_flag = true;
				goto retry_hi_freq;
			}
		}
		rot_clk = rot_need_clk;
	}

	DPU_DEBUG_BTS(decon, "\t-rot_init_bw(%d) rot_need_clk(%d)\n",
			rot_init_bw, (u32)rot_need_clk);
	DPU_DEBUG_BTS(decon, "[ROT-] AFTER latency check: %d KHz\n", (u32)rot_clk);

	return rot_clk;
}

static u64 dpu_bts_calc_rotate_crop_aclk(struct decon_device *decon, u32 aclk_base,
		struct dpu_bts_win_config *config)
{
	u64 rot_clk = 0;
	u32 init_rot_lines;
	u32 line_t_ns;
	u32 tx_allow_rot_ns;
	u32 of_lines = decon->bts.of_lines;
	u32 rot_wr_cycle;
	u32 ppc_rot_w = decon->bts.ppc_rot_w;
	u32 init_crop;

	init_crop = config->is_xflip ?
			    config->src_f_w - config->src_w - config->src_x :
			    config->src_x;

	init_rot_lines = ROT_READ_LINE - (init_crop % ROT_READ_LINE);

	/* 2x of_lines is required for operation without initial latency */
	DPU_DEBUG_BTS(decon, "src_x:%d, fb_w:%d, fb_h:%d, src_w:%d, src_h:%d\n",
		      config->src_x, config->src_f_w, config->src_f_h,
		      config->src_w, config->src_h);
	DPU_DEBUG_BTS(decon,
		      "init_rot_line:%d, of_line:%d, ppc_rot_w:%d, xflip:%d\n",
		      init_rot_lines, of_lines, ppc_rot_w, config->is_xflip);

	if (init_rot_lines > of_lines && init_rot_lines <= of_lines * 2) {
		line_t_ns = dpu_bts_get_one_line_time(decon);
		rot_wr_cycle = (32 - of_lines) * config->src_h / ppc_rot_w;
		tx_allow_rot_ns = line_t_ns * of_lines;
		rot_clk = (u64)rot_wr_cycle * 1000U * 1000U / tx_allow_rot_ns;
		DPU_DEBUG_BTS(
			decon,
			"rot_wr_cycle:%d, tx_allow_rot_ns:%d, rot_clk:%d\n",
			rot_wr_cycle, tx_allow_rot_ns, rot_clk);
	}

	if (rot_clk < aclk_base)
		rot_clk = aclk_base;

	DPU_DEBUG_BTS(decon, "[ROT] AFTER OF level check: %d KHz\n", (u32)rot_clk);

	return rot_clk;
}

u64 dpu_bts_calc_aclk_disp(struct decon_device *decon,
		struct dpu_bts_win_config *config, u64 resol_clk, u32 max_clk)
{
	u64 s_ratio_h, s_ratio_v;
	u64 aclk_disp, aclk_base;
	u32 ppc;
	u32 src_w, src_h;
	bool is_rotate = config->is_rot;
	bool is_comp = config->is_comp;
	bool is_scale = false;
	bool is_dsc = false;

	if (config->is_rot) {
		src_w = config->src_h;
		src_h = config->src_w;
	} else {
		src_w = config->src_w;
		src_h = config->src_h;
	}

	s_ratio_h = (src_w <= config->dst_w) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src_w / (u64)config->dst_w;
	s_ratio_v = (src_h <= config->dst_h) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src_h / (u64)config->dst_h;

	/* case for using dsc encoder 1ea at decon0 or decon1 */
	if ((decon->id != 2) && (decon->config.dsc.dsc_count == 1))
		ppc = ((decon->bts.ppc / 2U) >= 1U) ?
				(decon->bts.ppc / 2U) : 1U;
	else
		ppc = decon->bts.ppc;

	if (decon->bts.ppc_scaler && (decon->bts.ppc_scaler < ppc))
		ppc = decon->bts.ppc_scaler;

	aclk_disp = resol_clk * s_ratio_h * s_ratio_v * DISP_FACTOR  / 100UL
		/ ppc / (MULTI_FACTOR * MULTI_FACTOR);

	if (aclk_disp < (resol_clk / ppc))
		aclk_disp = resol_clk / ppc;

	if (!is_rotate)
		return aclk_disp;

	/* rotation case: check if latency conditions are met */
	if (aclk_disp > max_clk)
		aclk_base = aclk_disp;
	else
		aclk_base = max_clk;

	if ((s_ratio_h != MULTI_FACTOR) || (s_ratio_v != MULTI_FACTOR))
		is_scale = true;

	if (decon->config.dsc.enabled)
		is_dsc = true;

	aclk_disp = dpu_bts_calc_rotate_aclk(decon, (u32)aclk_base,
			(u32)ppc, config->format, src_w, config->dst_w, config->dst_y,
			is_comp, is_scale, is_dsc);

	aclk_disp = dpu_bts_calc_rotate_crop_aclk(decon, (u32)aclk_disp, config);

	return aclk_disp;
}

static void dpu_bts_sum_all_decon_bw(struct decon_device *decon, u32 ch_bw[])
{
	int i, j;

	if (decon->id >= MAX_DECON_CNT) {
		DPU_INFO_BTS(decon, "undefined decon id!\n");
		return;
	}

	for (i = 0; i < MAX_PORT_CNT; ++i)
		decon->bts.ch_bw[decon->id][i] = ch_bw[i];

	for (i = 0; i < MAX_DECON_CNT; ++i) {
		if (decon->id == i)
			continue;

		for (j = 0; j < MAX_PORT_CNT; ++j)
			ch_bw[j] += decon->bts.ch_bw[i][j];
	}
}

static u64 dpu_bts_calc_disp_with_full_size(struct decon_device *decon)
{
	u64 resol_clock;
	u32 ppc;

	if (decon->bts.resol_clk)
		resol_clock = decon->bts.resol_clk;
	else
		resol_clock = dpu_bts_get_resol_clock(
				decon->config.image_width,
				decon->config.image_height, decon->bts.fps);

	/* case for using dsc encoder 1ea at decon0 or decon1 */
	if ((decon->id != 2) && (decon->config.dsc.dsc_count == 1))
		ppc = ((decon->bts.ppc / 2U) >= 1U) ?
				(decon->bts.ppc / 2U) : 1U;
	else
		ppc = decon->bts.ppc;

	if (decon->bts.ppc_scaler && (decon->bts.ppc_scaler < ppc))
		ppc = decon->bts.ppc_scaler;

	return (resol_clock/ppc);
}

void dpu_bts_set_bus_qos(const struct exynos_drm_crtc *exynos_crtc)
{
	/* If you need to hold a minlock in a specific scenario, do it here. */
	return;
}

static void dpu_bts_find_max_disp_freq(struct decon_device *decon)
{
	int i, j;
	u32 disp_ch_bw[MAX_PORT_CNT];
	u32 max_disp_ch_bw;
	u64 disp_op_freq = 0, freq = 0, disp_min_freq = 0;
	struct dpu_bts_win_config *config = decon->bts.win_config;

	memset(disp_ch_bw, 0, sizeof(disp_ch_bw));

	for (i = 0; i < MAX_DPP_CNT; ++i)
		for (j = 0; j < MAX_PORT_CNT; ++j)
			if (decon->bts.bw[i].ch_num == j)
				disp_ch_bw[j] += decon->bts.bw[i].val;

	/* must be considered other decon's bw */
	dpu_bts_sum_all_decon_bw(decon, disp_ch_bw);

	for (i = 0; i < MAX_PORT_CNT; ++i)
		if (disp_ch_bw[i])
			DPU_DEBUG_BTS(decon, "\tAXI_DPU%d = %d\n", i, disp_ch_bw[i]);

	max_disp_ch_bw = disp_ch_bw[0];
	for (i = 1; i < MAX_PORT_CNT; ++i)
		if (max_disp_ch_bw < disp_ch_bw[i])
			max_disp_ch_bw = disp_ch_bw[i];

	decon->bts.peak = max_disp_ch_bw;
	if (max_disp_ch_bw < decon->bts.write_bw)
		decon->bts.peak = decon->bts.write_bw;
	decon->bts.max_disp_freq = decon->bts.peak / decon->bts.inner_width;
	disp_op_freq = decon->bts.max_disp_freq;

	for (i = 0; i < decon->win_cnt; ++i) {
		if (config[i].state == DPU_WIN_STATE_DISABLED)
			continue;

		freq = dpu_bts_calc_aclk_disp(decon, &config[i],
				(u64)decon->bts.resol_clk, disp_op_freq);
		if (disp_op_freq < freq)
			disp_op_freq = freq;
	}

	config = &decon->bts.wb_config;
	if (config->state != DPU_WIN_STATE_DISABLED) {
		freq = dpu_bts_calc_aclk_disp(decon, config,
				(u64)decon->bts.resol_clk, disp_op_freq);
		if (disp_op_freq < freq)
			disp_op_freq = freq;
	}

	/*
	 * At least one window is used for colormap if there is a request of
	 * disabling all windows. So, disp frequency for a window of LCD full
	 * size is necessary.
	 */
	disp_min_freq = dpu_bts_calc_disp_with_full_size(decon);
	if (disp_op_freq < disp_min_freq) {
		disp_op_freq = disp_min_freq;
		dpu_bts_set_bus_qos(decon->crtc);
	}

	DPU_DEBUG_BTS(decon, "\tDISP bus freq(%d), operating freq(%d)\n",
			decon->bts.max_disp_freq, disp_op_freq);

	if (decon->bts.max_disp_freq < disp_op_freq)
		decon->bts.max_disp_freq = disp_op_freq;

#if defined(CONFIG_EXYNOS_DISPLAYPORT)
	if (decon->config.out_type & DECON_OUT_DP) {
		if (decon->bts.max_disp_freq < 200000)
			decon->bts.max_disp_freq = 200000;
	}
#endif

	DPU_DEBUG_BTS(decon, "\tMAX DISP FREQ = %d\n", decon->bts.max_disp_freq);
}

static void dpu_bts_share_bw_info(int id)
{
	int i, j;
	struct decon_device *decon[MAX_DECON_CNT];

	for (i = 0; i < MAX_DECON_CNT; i++)
		decon[i] = NULL;

	for (i = 0; i < MAX_DECON_CNT; i++)
		decon[i] = get_decon_drvdata(i);

	for (i = 0; i < MAX_DECON_CNT; ++i) {
		if (id == i || decon[i] == NULL)
			continue;

		for (j = 0; j < MAX_PORT_CNT; ++j)
			decon[i]->bts.ch_bw[id][j] =
				decon[id]->bts.ch_bw[id][j];
	}
}

static void dpu_bts_convert_config_to_info(const struct decon_device *decon,
		struct bts_dpp_info *dpp, const struct dpu_bts_win_config *config)
{
	const struct dpu_fmt *fmt_info;

	dpp->used = true;
	fmt_info = dpu_find_fmt_info(config->format);
	dpp->bpp = fmt_info->bpp + fmt_info->padding;
	dpp->src_w = config->src_w;
	dpp->src_h = config->src_h;
	dpp->dst.x1 = config->dst_x;
	dpp->dst.x2 = config->dst_x + config->dst_w;
	dpp->dst.y1 = config->dst_y;
	dpp->dst.y2 = config->dst_y + config->dst_h;
	dpp->rotation = config->is_rot;
	dpp->compression = config->is_comp;
	dpp->fmt_name = fmt_info->name;

	DPU_DEBUG_BTS(decon,
			"\tDPP%d : bpp(%d) src w(%d) h(%d) rot(%d) comp(%d) fmt(%s)\n",
			config->dpp_ch, dpp->bpp, dpp->src_w, dpp->src_h,
			dpp->rotation, dpp->compression, dpp->fmt_name);
	DPU_DEBUG_BTS(decon,
			"\t\t\t\tdst x(%d) right(%d) y(%d) bottom(%d)\n",
			dpp->dst.x1, dpp->dst.x2, dpp->dst.y1, dpp->dst.y2);
}

static bool dpu_bts_request_mif_qos(bool state, enum mif_update_dir dir)
{
	static bool prev_state = false;
	static bool updated = false;

	if (dir == SET_TO_STATIC) {
		updated = (prev_state != state) ? true : false;
		prev_state = state;
	}

	return (updated && !prev_state);
}

static void
dpu_bts_calc_dpp_bw(struct decon_device *decon, struct bts_dpp_info *dpp,
				struct bts_decon_info *bts_info, u32 format,
				int idx, bool *hold)
{
	u64 ch_bw = 0, rot_bw;
	u32 src_w, src_h;
	u32 dst_w, dst_h;
	u64 s_ratio_h, s_ratio_v;
	u32 ppc;
	u32 aclk_base;
	bool is_comp, is_scale = false, is_dsc;
	u32 module_cycle, basic_cycle;
	u64 rot_clk;
	u32 dpu_lat_t_ns, tx_allow_ns;
	u32 rot_read_line;

	if (dpp->rotation) {
		src_w = dpp->src_h;
		src_h = dpp->src_w;
	} else {
		src_w = dpp->src_w;
		src_h = dpp->src_h;
	}
	dst_w = dpp->dst.x2 - dpp->dst.x1;
	dst_h = dpp->dst.y2 - dpp->dst.y1;
	s_ratio_h = (src_w <= dst_w) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src_w / (u64)dst_w;
	s_ratio_v = (src_h <= dst_h) ? MULTI_FACTOR : MULTI_FACTOR * (u64)src_h / (u64)dst_h;

	/* BW(KB) : s_ratio_h * s_ratio_v * (bpp/8) * resol_clk (* dst_w / xres) */
	ch_bw = s_ratio_h * s_ratio_v * dpp->bpp / 8 * bts_info->vclk
		/ (MULTI_FACTOR * MULTI_FACTOR);
	if (decon->bts.fps <= DISP_REFRESH_RATE)
		ch_bw = ch_bw * (u64)dst_w / bts_info->lcd_w;

	if (dpp->rotation) {
		/* case for using dsc encoder 1ea at decon0 or decon1 */
		if ((decon->id != 2) && (decon->config.dsc.dsc_count == 1))
			ppc = ((decon->bts.ppc / 2U) >= 1U) ?
					(decon->bts.ppc / 2U) : 1U;
		else
			ppc = decon->bts.ppc;
		if (decon->bts.ppc_scaler && (decon->bts.ppc_scaler < ppc))
			ppc = decon->bts.ppc_scaler;

		aclk_base = exynos_devfreq_get_domain_freq(DEVFREQ_DISP);
		if (aclk_base < (decon->bts.resol_clk / ppc))
			aclk_base = decon->bts.resol_clk / ppc;

		is_comp = dpp->compression;
		if ((s_ratio_h != MULTI_FACTOR) || (s_ratio_v != MULTI_FACTOR))
			is_scale = true;
		is_dsc = decon->config.dsc.enabled;

		rot_clk = dpu_bts_calc_rotate_cycle(decon, aclk_base, ppc, format,
				src_w, dst_w, is_comp, is_scale, is_dsc,
				&module_cycle, &basic_cycle);
		tx_allow_ns = dpu_bts_get_rotate_tx_allow_t(decon, (u32)rot_clk,
				module_cycle, basic_cycle, dpp->dst.y1, &dpu_lat_t_ns);

		rot_read_line = dpu_bts_rotate_read_line(is_comp, format);

		/* BW(KB) : sh * 32B * (bpp/8) / v_blank */
		rot_bw = (u64)src_w * rot_read_line * dpp->bpp / 8 * 1000000U /
				tx_allow_ns;
		DPU_DEBUG_BTS(decon, "\tDPP%d ch_bw(%d), rot_bw(%d)\n",
				idx, ch_bw, rot_bw);
		if (rot_bw > ch_bw)
			ch_bw = rot_bw;
	}

	dpp->bw = (u32)ch_bw;

	/*
	 * NV12 square ratio
	 *  - MIF min freq : 845Mhz @ 120
	 */
	if (((dpp->src_w == dpp->src_h) && (dpp->src_w == 1440)) &&
		((dst_w == dst_h) && (dst_w == 1080)) &&
		decon->bts.fps == 120 && dpp->rotation &&
		decon->config.out_type & DECON_OUT_DSI) {
		const struct dpu_fmt *fmt_info;

		fmt_info = dpu_find_fmt_info(format);
		if (fmt_info->fmt == DRM_FORMAT_NV12) {
			exynos_pm_qos_update_request(&decon->bts.mif_qos, 845 * 1000);
			*hold |= true;
		}
	}

	DPU_DEBUG_BTS(decon, "\tDPP%d BW = %d\n", idx, dpp->bw);
}

#define BTS_MIF_QOS_MAX 2093000
#define BTS_MINLOCK_LAYER 6
#define BTS_MINLOCK_FPS 120
static struct exynos_pm_domain *pd_csis = NULL;
static void dpu_bts_set_max_bus_qos(struct decon_device *decon)
{
	if (pd_csis == NULL)
		pd_csis = exynos_pd_lookup_name("pd-csis");

	if ((decon->bts.bts_info.layer_cnt >= BTS_MINLOCK_LAYER) &&
	    (decon->bts.fps >= BTS_MINLOCK_FPS) &&
	    (decon->config.mode.op_mode == DECON_VIDEO_MODE) &&
	    (pd_csis && pd_csis->genpd.status == GENPD_STATE_ON))
		exynos_pm_qos_update_request(&decon->bts.mif_qos, BTS_MIF_QOS_MAX);
	else
		exynos_pm_qos_update_request(&decon->bts.mif_qos, 0);
}

void dpu_bts_calc_bw(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct dpu_bts_win_config *config = decon->bts.win_config;
	struct bts_decon_info bts_info;
	int idx, i, wb_idx;
	u32 read_bw = 0, write_bw = 0;
	u64 resol_clock;
	bool updated = false;

	if (!decon->bts.enabled)
		return;

	DPU_DEBUG_BTS(decon, "\n");
	DPU_DEBUG_BTS(decon, "+\n");

	memset(&bts_info, 0, sizeof(struct bts_decon_info));

	if (decon->config.out_type == DECON_OUT_WB) {
		decon->config.image_width = decon->bts.wb_config.dst_w;
		decon->config.image_height = decon->bts.wb_config.dst_h;
	}

	resol_clock = dpu_bts_get_resol_clock(decon->config.image_width,
				decon->config.image_height, decon->bts.fps);
	decon->bts.resol_clk = (u32)resol_clock;
	DPU_DEBUG_BTS(decon, "[Run] resol clock = %d Khz @%d fps\n",
			decon->bts.resol_clk, decon->bts.fps);

	bts_info.vclk = decon->bts.resol_clk;
	bts_info.lcd_w = decon->config.image_width;
	bts_info.lcd_h = decon->config.image_height;

	if (dpu_bts_request_mif_qos(false, GET_FROM_STATIC))
		exynos_pm_qos_update_request(&decon->bts.mif_qos, 0);

	/* read bw calculation */
	for (i = 0; i < decon->win_cnt; ++i) {
		if (config[i].state != DPU_WIN_STATE_BUFFER)
			continue;

		idx = config[i].dpp_ch;
		dpu_bts_convert_config_to_info(decon, &bts_info.dpp[idx], &config[i]);
		dpu_bts_calc_dpp_bw(decon, &bts_info.dpp[idx], &bts_info,
					config[i].format, idx, &updated);

		read_bw += bts_info.dpp[idx].bw;
		bts_info.layer_cnt++;
	}

	/* write bw calculation */
	config = &decon->bts.wb_config;
	wb_idx = config->dpp_ch;
	if (config->state == DPU_WIN_STATE_BUFFER) {
		dpu_bts_convert_config_to_info(decon, &bts_info.dpp[wb_idx], config);
		dpu_bts_calc_dpp_bw(decon, &bts_info.dpp[wb_idx], &bts_info,
					config->format, wb_idx, &updated);
		write_bw += bts_info.dpp[wb_idx].bw;
	}

	for (i = 0; i < MAX_DPP_CNT; i++)
		decon->bts.bw[i].val = bts_info.dpp[i].bw;

	dpu_bts_request_mif_qos(updated, SET_TO_STATIC);

	decon->bts.read_bw = read_bw;
	decon->bts.write_bw = write_bw;
	decon->bts.total_bw = read_bw + write_bw;
	memcpy(&decon->bts.bts_info, &bts_info, sizeof(struct bts_decon_info));

	DPU_DEBUG_BTS(decon, "\t Total.BW(KB) = %d, Rd.BW = %d, Wr.BW = %d\n",
			decon->bts.total_bw, decon->bts.read_bw, decon->bts.write_bw);

	dpu_bts_find_max_disp_freq(decon);

	/* update bw for other decons */
	dpu_bts_share_bw_info(decon->id);

	dpu_bts_set_max_bus_qos(decon);

	DPU_EVENT_LOG("BTS_CALC_BW", decon->crtc, 0,
			"mif(%lu) int(%lu) disp(%lu) calculated disp(%u)",
			exynos_devfreq_get_domain_freq(DEVFREQ_MIF),
			exynos_devfreq_get_domain_freq(DEVFREQ_INT),
			exynos_devfreq_get_domain_freq(DEVFREQ_DISP),
			decon->bts.max_disp_freq);
	DPU_DEBUG_BTS(decon, "-\n");
}


void dpu_bts_update_bw(struct exynos_drm_crtc *exynos_crtc, bool shadow_updated)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct bts_bw bw = { 0, };

	if (!decon->bts.enabled)
		return;

	DPU_DEBUG_BTS(decon, "+\n");

	/* update peak & read bandwidth per DPU port */
	bw.peak = decon->bts.peak;
	bw.read = decon->bts.read_bw;
	bw.write = decon->bts.write_bw;
	DPU_DEBUG_BTS(decon, "\t(%s shadow_update) peak = %d, read = %d, write = %d\n",
			(shadow_updated ? "after" : "before"),
			bw.peak, bw.read, bw.write);

	if (shadow_updated) {
		/* after DECON h/w configs are updated to shadow SFR */
		if (decon->bts.total_bw < decon->bts.prev_total_bw)
			bts_update_bw(decon->bts.bw_idx, bw);

		if (decon->bts.max_disp_freq < decon->bts.prev_max_disp_freq) {
			exynos_pm_qos_update_request(&decon->bts.disp_qos,
					decon->bts.max_disp_freq);
			DPU_DEBUG_BTS(decon, "disp_qos_update_request(disp_qos=%d)\n",
						decon->bts.max_disp_freq);
		}

		decon->bts.prev_total_bw = decon->bts.total_bw;
		decon->bts.prev_max_disp_freq = decon->bts.max_disp_freq;
	} else {
		if (decon->bts.total_bw > decon->bts.prev_total_bw)
			bts_update_bw(decon->bts.bw_idx, bw);

		if (decon->bts.max_disp_freq > decon->bts.prev_max_disp_freq) {
			exynos_pm_qos_update_request(&decon->bts.disp_qos,
					decon->bts.max_disp_freq);
			DPU_DEBUG_BTS(decon, "disp_qos_update_request(disp_qos=%d)\n",
						decon->bts.max_disp_freq);
		}
	}

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	/* Workaround min lock for LPM / Recovery Scenario */
	if (!dpu_bts_is_normal_boot()) {
		if (exynos_pm_qos_request_active(&decon->bts.mif_qos))
			exynos_pm_qos_update_request(&decon->bts.mif_qos, MIF_MINLOCK_FOR_LPM_RECOVERY);
	}
#endif

	DPU_EVENT_LOG("BTS_UPDATE_BW", decon->crtc, 0, "mif(%lu) int(%lu) disp(%lu)",
			exynos_devfreq_get_domain_freq(DEVFREQ_MIF),
			exynos_devfreq_get_domain_freq(DEVFREQ_INT),
			exynos_devfreq_get_domain_freq(DEVFREQ_DISP));
	DPU_DEBUG_BTS(decon, "-\n");
}

void dpu_bts_release_bw(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct bts_bw bw = { 0, };

	if (!decon->bts.enabled)
		return;

	DPU_DEBUG_BTS(decon, "+\n");

	if ((decon->config.out_type & DECON_OUT_DSI) ||
		(decon->config.out_type == DECON_OUT_WB)) {
		bts_update_bw(decon->bts.bw_idx, bw);
		decon->bts.prev_total_bw = 0;

		if (exynos_pm_qos_request_active(&decon->bts.mif_qos))
			exynos_pm_qos_update_request(&decon->bts.mif_qos, 0);
		else
			DPU_ERR_BTS(decon, "mif qos setting error\n");

		if (exynos_pm_qos_request_active(&decon->bts.int_qos))
			exynos_pm_qos_update_request(&decon->bts.int_qos, 0);
		else
			DPU_ERR_BTS(decon, "int qos setting error\n");

		if (exynos_pm_qos_request_active(&decon->bts.disp_qos))
			exynos_pm_qos_update_request(&decon->bts.disp_qos, 0);
		else
			DPU_ERR_BTS(decon, "disp qos setting error\n");
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
		/* Workaround min lock for LPM / Recovery Scenario */
		if (!dpu_bts_is_normal_boot()) {
			if (exynos_pm_qos_request_active(&decon->bts.mif_qos))
				exynos_pm_qos_update_request(&decon->bts.mif_qos, 0);
			else
				DPU_ERR_BTS(decon, "mif qos setting error\n");
		}
#endif
		decon->bts.prev_max_disp_freq = 0;
	} else if (decon->config.out_type & DECON_OUT_DP) {
		if (exynos_pm_qos_request_active(&decon->bts.mif_qos))
			exynos_pm_qos_update_request(&decon->bts.mif_qos, 0);
		else
			DPU_ERR_BTS(decon, "mif qos setting error\n");

		if (exynos_pm_qos_request_active(&decon->bts.int_qos))
			exynos_pm_qos_update_request(&decon->bts.int_qos, 0);
		else
			DPU_ERR_BTS(decon, "int qos setting error\n");

		if (exynos_pm_qos_request_active(&decon->bts.disp_qos))
			exynos_pm_qos_update_request(&decon->bts.disp_qos, 0);
		else
			DPU_ERR_BTS(decon, "disp qos setting error\n");

		bts_del_scenario(decon->bts.scen_idx[DPU_BS_DP_DEFAULT]);
	}

	DPU_EVENT_LOG("BTS_RELEASE_BW", decon->crtc, 0, "mif(%lu) int(%lu) disp(%lu)",
			exynos_devfreq_get_domain_freq(DEVFREQ_MIF),
			exynos_devfreq_get_domain_freq(DEVFREQ_INT),
			exynos_devfreq_get_domain_freq(DEVFREQ_DISP));
	DPU_DEBUG_BTS(decon, "-\n");
}

#define MAX_IDX_NAME_SIZE	16
void dpu_bts_init(struct exynos_drm_crtc *exynos_crtc)
{
	int i;
	char bts_idx_name[MAX_IDX_NAME_SIZE];
	const struct drm_encoder *encoder;
	struct decon_device *decon = exynos_crtc->ctx;

	DPU_DEBUG_BTS(decon, "+\n");

	decon->bts.enabled = false;

	if ((!IS_ENABLED(CONFIG_EXYNOS_BTS) && !IS_ENABLED(CONFIG_EXYNOS_BTS_MODULE))
			|| (!IS_ENABLED(CONFIG_EXYNOS_PM_QOS) &&
				!IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE))) {
		DPU_ERR_BTS(decon, "bts feature is disabled\n");
		return;
	}

	memset(bts_idx_name, 0, MAX_IDX_NAME_SIZE);
	snprintf(bts_idx_name, MAX_IDX_NAME_SIZE, "DECON%d", decon->id);
	decon->bts.bw_idx = bts_get_bwindex(bts_idx_name);

	for (i = 0; i < MAX_PORT_CNT; i++)
		decon->bts.ch_bw[decon->id][i] = 0;

	DPU_DEBUG_BTS(decon, "BTS_BW_TYPE(%d)\n", decon->bts.bw_idx);
	exynos_pm_qos_add_request(&decon->bts.mif_qos,
					PM_QOS_BUS_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&decon->bts.int_qos,
					PM_QOS_DEVICE_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&decon->bts.disp_qos,
					PM_QOS_DISPLAY_THROUGHPUT, 0);

	for (i = 0; i < decon->win_cnt; ++i) { /* dma type order */
		decon->bts.bw[i].ch_num = decon->dpp[i]->port;
		DPU_INFO_BTS(decon, "CH(%d) Port(%d)\n", i, decon->bts.bw[i].ch_num);
	}

	drm_for_each_encoder(encoder, decon->drm_dev) {
		const struct writeback_device *wb;

		if (encoder->encoder_type == DRM_MODE_ENCODER_VIRTUAL) {
			wb = enc_to_wb_dev(encoder);
			decon->bts.bw[wb->id].ch_num = wb->port;
			break;
		}
	}

	decon->bts.enabled = true;

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	/* Workaround min lock for LPM / Recovery Scenario */
	if (!dpu_bts_is_normal_boot() && (decon->config.out_type & DECON_OUT_DSI))
		DPU_INFO_BTS(decon, "decon%d boot_mode:0x%02X. MIF minlock(%d) will work.\n",
			decon->id, boot_mode >> 16, MIF_MINLOCK_FOR_LPM_RECOVERY);
#endif
	DPU_INFO_BTS(decon, "bts feature is enabled\n");
}

void dpu_bts_deinit(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;

	if (!decon->bts.enabled)
		return;

	DPU_DEBUG_BTS(decon, "+\n");
	exynos_pm_qos_remove_request(&decon->bts.disp_qos);
	exynos_pm_qos_remove_request(&decon->bts.int_qos);
	exynos_pm_qos_remove_request(&decon->bts.mif_qos);
	DPU_DEBUG_BTS(decon, "-\n");
}

void dpu_bts_print_info(const struct exynos_drm_crtc *exynos_crtc)
{
	int i;
	struct decon_device *decon = exynos_crtc->ctx;
	const struct bts_decon_info *info = &decon->bts.bts_info;
	static ktime_t bts_info_print_block_ts;
	bool bts_info_print_blocked = true;

	if (!decon->bts.enabled)
		return;

	DPU_INFO_BTS(decon, "bw(prev:%u curr:%u), disp(prev:%u curr:%u), peak(%u)\n",
			decon->bts.prev_total_bw,
			decon->bts.total_bw, decon->bts.prev_max_disp_freq,
			decon->bts.max_disp_freq, decon->bts.peak);

	DPU_INFO_BTS(decon, "MIF(%lu), INT(%lu), DISP(%lu)\n",
			exynos_devfreq_get_domain_freq(DEVFREQ_MIF),
			exynos_devfreq_get_domain_freq(DEVFREQ_INT),
			exynos_devfreq_get_domain_freq(DEVFREQ_DISP));

	if (ktime_after(ktime_get(), bts_info_print_block_ts)) {
		bts_info_print_block_ts = ktime_add_ms(ktime_get(),
					BTS_INFO_PRINT_BLOCK_TIMEOUT);
		bts_info_print_blocked = false;
	}

	if (bts_info_print_blocked)
		return;

	show_exynos_pm_qos_data(PM_QOS_BUS_THROUGHPUT);
	show_exynos_pm_qos_data(PM_QOS_BUS_THROUGHPUT_MAX);
	show_exynos_pm_qos_data(PM_QOS_DISPLAY_THROUGHPUT);
	show_exynos_pm_qos_data(PM_QOS_DEVICE_THROUGHPUT);

	for (i = 0; i < MAX_DPP_CNT; ++i) {
		if (!info->dpp[i].used)
			continue;

		DPU_INFO_BTS(decon, "DPP[%d] bpp(%d) src(%d %d) dst(%d %d %d %d)\n",
				i, info->dpp[i].bpp,
				info->dpp[i].src_w, info->dpp[i].src_h,
				info->dpp[i].dst.x1, info->dpp[i].dst.x2,
				info->dpp[i].dst.y1, info->dpp[i].dst.y2);
		DPU_INFO_BTS(decon, "rot(%d) comp(%d) fmt(%s)\n",
				info->dpp[i].rotation, info->dpp[i].compression,
				info->dpp[i].fmt_name);
	}
}

struct dpu_bts_ops dpu_bts_control = {
	.init		= dpu_bts_init,
	.calc_bw	= dpu_bts_calc_bw,
	.update_bw	= dpu_bts_update_bw,
	.release_bw	= dpu_bts_release_bw,
	.deinit		= dpu_bts_deinit,
	.print_info	= dpu_bts_print_info,
};
