/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for DPU bts.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_BTS_H__
#define __EXYNOS_DRM_BTS_H__

#include <linux/types.h>
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
#include <soc/samsung/exynos_pm_qos.h>
#endif

struct exynos_drm_crtc;

#if !IS_ENABLED(CONFIG_SOC_S5E8825)
 /* DPU DVFS Level */
#define BTS_DFS_MAX		10
#define BTS_WIN_MAX		16
#define BTS_DPP_MAX		18
#define BTS_DPU_MAX		2
#define BTS_DECON_MAX	4
#else
#define BTS_DFS_MAX		10
#define BTS_WIN_MAX		8
#define BTS_DPP_MAX		9
#define BTS_DPU_MAX		2
#define BTS_DECON_MAX	2
#endif

enum dpu_win_state {
	DPU_WIN_STATE_DISABLED = 0,
	DPU_WIN_STATE_COLOR,
	DPU_WIN_STATE_BUFFER,
};

enum decon_bts_scen {
	DPU_BS_DEFAULT		= 0,
	DPU_BS_UHD,		/* for UHD 8-bit rotation case */
	DPU_BS_UHD_10B,		/* for UHD 10-bit rotation case */
	DPU_BS_DP_DEFAULT,
	/* add scenario index if necessary */
	DPU_BS_MAX
};

struct exynos_drm_crtc;
struct dpu_bts_ops {
	void (*init)(struct exynos_drm_crtc *exynos_crtc);
	void (*release_bw)(struct exynos_drm_crtc *exynos_crtc);
	void (*calc_bw)(struct exynos_drm_crtc *exynos_crtc);
	void (*update_bw)(struct exynos_drm_crtc *exynos_crtc, bool shadow_updated);
	void (*deinit)(struct exynos_drm_crtc *exynos_crtc);
	void (*print_info)(const struct exynos_drm_crtc *exynos_crtc);
	void (*set_bus_qos)(const struct exynos_drm_crtc *exynos_crtc);
};

struct dpu_bts_bw {
	u32 val;
	u32 ch_num;
};

struct dpu_bts_win_config {
	enum dpu_win_state state;
	u32 src_x;
	u32 src_y;
	u32 src_w;
	u32 src_h;
	u32 src_f_w;
	u32 src_f_h;
	int dst_x;
	int dst_y;
	u32 dst_w;
	u32 dst_h;
	bool is_rot;
	bool is_comp;
	bool is_hdr;
	bool is_xflip;
	int dpp_ch;
	u32 format;
	u64 comp_src;
	dma_addr_t dbg_dma_addr; /* only for debugging */
};

struct bts_layer_position {
	u32 x1;
	u32 x2; /* x2 = x1 + width */
	u32 y1;
	u32 y2; /* y2 = y1 + height */
};

struct bts_dpp_info {
	bool used;
	u32 bpp;
	u32 src_h;
	u32 src_w;
	struct bts_layer_position dst;
	u32 bw;
	bool rotation;
	bool compression;
	const char *fmt_name;
	bool hdr;
};

struct bts_decon_info {
	struct bts_dpp_info dpp[BTS_DPP_MAX];
	u32 vclk; /* Khz */
	u32 lcd_w;
	u32 lcd_h;
	u32 layer_cnt;
};

struct dpu_bts {
	bool enabled;
	u32 resol_clk;
	u32 peak;
	u32 read_bw;
	u32 write_bw;
	u32 total_bw;
	u32 prev_total_bw;
	u32 max_disp_freq;
	u32 prev_max_disp_freq;
	u32 ppc;
	u32 ppc_rotator;
	u32 ppc_rot_w;
	u32 ppc_scaler;
	u32 delay_comp;
	u32 delay_scaler;
	u32 inner_width;
	u32 bus_width;
	u32 rot_util;
	u32 dfs_lv_cnt;
	u32 of_lines;
	u32 dfs_lv[BTS_DFS_MAX];
	u32 vbp;
	u32 vfp;
	u32 vsa;
	u32 fps;
	u32 v_blank_t;
	/* includes writeback dpp */
	struct dpu_bts_bw bw[BTS_DPP_MAX];

	/* each decon must know other decon's BW to get overall BW */
	u32 ch_bw[BTS_DECON_MAX][BTS_DPU_MAX];
	int bw_idx;
	u32 scen_idx[DPU_BS_MAX];
	struct bts_decon_info bts_info;
	struct dpu_bts_ops *ops;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	struct exynos_pm_qos_request mif_qos;
	struct exynos_pm_qos_request int_qos;
	struct exynos_pm_qos_request disp_qos;
#endif

	struct dpu_bts_win_config win_config[BTS_WIN_MAX];
	struct dpu_bts_win_config wb_config;
};

extern struct dpu_bts_ops dpu_bts_control;
#endif /* __EXYNOS_DRM_BTS_H__ */
