/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Register definition file for Samsung DisplayPort driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <cal_config.h>
#include <dp_cal.h>
#include "../exynos_drm_dp.h"

void dp_hex_dump(void __iomem *base_addr, int size)
{
	int i, j;
	int height, num_word;

	height = (size / 32) + 1;
	num_word = size / 4;
	for (i = 0; i < height; i++) {
		cal_log_info(0, "[0x%08x] ", base_addr + i * 32);
		for (j = 0; j < 8; j++) {
			if (num_word <= 0) break;
			cal_log_info(0, "%08x ", readl(base_addr + i * 32 + j * 4));
			num_word--;
		}
		cal_log_info(0, "\n");
		if (num_word <= 0) break;
	}
}

int dp_hsi0_domain_power_check(void)
{
	static void __iomem *pwr_regs;
	int ret = 1;

	if (pwr_regs == NULL) {
		pwr_regs = ioremap((phys_addr_t)0x15860000 + 0x2500, 0x24);
		if (!pwr_regs) {
			pr_err("HSI0_PWR SFR ioremap is failed\n");
			return -EINVAL;
		}
	} else
		pr_info("Already has pwr_regs\n");

	pr_info("HSI0_CONFIGURATION[0x1586_2500](0x%08x)\n", readl(pwr_regs));
	pr_info("HSI0_STATUS	   [0x1586_2504](0x%08x)\n", readl(pwr_regs + 0x4));
	pr_info("HSI0_STATES       [0x1586_2508](0x%08x)\n", readl(pwr_regs + 0x8));
	if (readl(pwr_regs + 0x8) == 0)
		ret = 0;
	pr_info("HSI0_CTRL         [0x1586_2510](0x%08x)\n", readl(pwr_regs + 0x10));
	pr_info("HSI0_OUT          [0x1586_2520](0x%08x)\n", readl(pwr_regs + 0x20));

	return ret;
}

void dp_hsi0_clk_check(void)
{
	static void __iomem *clk_regs1;
	static void __iomem *clk_regs2;
	static void __iomem *clk_regs3;

	if (clk_regs1 == NULL) {
		clk_regs1 = ioremap((phys_addr_t)0x10a00000 + 0x0620, 0x10);
		if (!clk_regs1) {
			pr_err("HSI0_CLK SFR ioremap is failed\n");
			return;
		}
	} else
		pr_info("Already has clk_regs1\n");

	pr_info("PLL_CON0_MUX_CLKCMU_HSI0_DPOSC_USER[0x10a0_0620](0x%08x)\n", readl(clk_regs1));
	pr_info("PLL_CON1_MUX_CLKCMU_HSI0_DPOSC_USER[0x10a0_0624](0x%08x)\n", readl(clk_regs1 + 0x4));

	if (clk_regs2 == NULL) {
		if (exynos_soc_info.main_rev == 0)
			clk_regs2 = ioremap((phys_addr_t)0x1a730000 + 0x1118, 0x10);
		else if (exynos_soc_info.main_rev == 1)
			clk_regs2 = ioremap((phys_addr_t)0x1a320000 + 0x1118, 0x10);

		if (!clk_regs2) {
			pr_err("HSI0_CLK SFR ioremap is failed\n");
			return;
		}
	} else
		pr_info("Already has clk_regs2\n");

	if (exynos_soc_info.main_rev == 0)
		pr_info("CLK_CON_MUX_MUX_CLKCMU_HSI0_DPOSC[0x1a73_1118](0x%08x)\n", readl(clk_regs2));
	else if (exynos_soc_info.main_rev == 1)
		pr_info("CLK_CON_MUX_MUX_CLKCMU_HSI0_DPOSC[0x1a32_1118](0x%08x)\n", readl(clk_regs2));

	if (clk_regs3 == NULL) {
		if (exynos_soc_info.main_rev == 0)
			clk_regs3 = ioremap((phys_addr_t)0x1a730000 + 0x1844, 0x10);
		else if(exynos_soc_info.main_rev == 1)
			clk_regs3 = ioremap((phys_addr_t)0x1a320000 + 0x1844, 0x10);
		if (!clk_regs3) {
			pr_err("HSI0_CLK SFR ioremap is failed\n");
			return;
		}
	} else
		pr_info("Already has clk_regs3\n");

	if (exynos_soc_info.main_rev == 0)
		pr_info("CLK_CON_DIV_CLKCMU_HSI0_DPOSC[0x1a73_1844](0x%08x)\n", readl(clk_regs3));
	else if (exynos_soc_info.main_rev == 1)
		pr_info("CLK_CON_DIV_CLKCMU_HSI0_DPOSC[0x1a32_1844](0x%08x)\n", readl(clk_regs3));
}

/*tuning value of each model is separated by phy table*/
u32 phy_eq_hbr0_1[phy_table][4][4][5] = {
	{	/* {eq main, eq pre, eq post, eq vswing lvl, rboost en} */
		{	/* Swing Level_0 */
			{24, 0, 0, 7, 1},	/* Pre-emphasis Level_0 */
			{29, 0, 5, 7, 0},	/* Pre-emphasis Level_1 */
			{36, 0, 12, 7, 0},	/* Pre-emphasis Level_2 */
			{43, 0, 19, 7, 0},	/* Pre-emphasis Level_3 */
		},
		{	/* Swing Level_1 */
			{35, 0, 0, 7, 1},
			{38, 0, 6, 7, 0},
			{46, 0, 14, 7, 0},
			{-1, -1, -1, -1, -1},
		},
		{	/* Swing Level_2 */
			{49, 0, 0, 7, 1},
			{50, 0, 12, 7, 0},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
		},
		{	/* Swing Level_3 */
			{62, 0, 0, 7, 0},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
		},
	},
	{	/* tuning value*/
		{	/* Swing Level_0 */
			{24, 0, 0, 7, 1},	/* Pre-emphasis Level_0 */
			{29, 0, 5, 7, 0},	/* Pre-emphasis Level_1 */
			{36, 0, 12, 7, 0},	/* Pre-emphasis Level_2 */
			{43, 0, 19, 7, 0},	/* Pre-emphasis Level_3 */
		},
		{	/* Swing Level_1 */
			{35, 0, 0, 7, 1},
			{38, 0, 6, 7, 0},
			{46, 0, 14, 7, 0},
			{-1, -1, -1, -1, -1},
		},
		{	/* Swing Level_2 */
			{50, 0, 3, 7, 1},
			{50, 0, 12, 7, 0},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
		},
		{	/* Swing Level_3 */
			{62, 0, 0, 7, 0},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
		},
	},
};

u32 phy_eq_hbr2_3[phy_table][4][4][5] = {
	{	/* {eq main, eq post, eq pre, eq vswing lvl, rboost en} */
		{	/* Swing Level_0 */
			{24, 0, 0, 7, 1},	/* Pre-emphasis Level_0 */
			{29, 0, 5, 7, 0},	/* Pre-emphasis Level_1 */
			{36, 0, 12, 7, 0},	/* Pre-emphasis Level_2 */
			{43, 0, 19, 7, 0},	/* Pre-emphasis Level_3 */
		},
		{	/* Swing Level_1 */
			{35, 0, 0, 7, 1},
			{38, 0, 6, 7, 0},
			{46, 0, 14, 7, 0},
			{-1, -1, -1, -1, -1},
		},
		{	/* Swing Level_2 */
			{49, 0, 0, 7, 1},
			{50, 0, 12, 7, 0},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
		},
		{	/* Swing Level_3 */
			{62, 0, 0, 7, 0},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
		},
	},
	{	/* tuning value */
		{	/* Swing Level_0 */
			{24, 0, 0, 7, 1},	/* Pre-emphasis Level_0 */
			{29, 0, 5, 7, 0},	/* Pre-emphasis Level_1 */
			{36, 0, 12, 7, 0},	/* Pre-emphasis Level_2 */
			{43, 0, 19, 7, 0},	/* Pre-emphasis Level_3 */
		},
		{	/* Swing Level_1 */
			{35, 0, 0, 7, 1},
			{38, 0, 6, 7, 0},
			{46, 0, 14, 7, 0},
			{-1, -1, -1, -1, -1},
		},
		{	/* Swing Level_2 */
			{49, 0, 0, 7, 1},
			{50, 0, 12, 7, 0},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
		},
		{	/* Swing Level_3 */
			{62, 0, 0, 7, 0},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
			{-1, -1, -1, -1, -1},
		},
	},

};

/* supported_videos[] is to be arranged in the order of pixel clock */
struct dp_supported_preset supported_videos[] = {
	{V640X480P60,      V4L2_DV_BT_DMT_640X480P60,         60, SYNC_NEGATIVE, SYNC_NEGATIVE,   1, "V640X480P60"},
	{V720X480P60,      V4L2_DV_BT_CEA_720X480P59_94,      60, SYNC_NEGATIVE, SYNC_NEGATIVE,   2, "V720X480P60"},
	{V720X576P50,      V4L2_DV_BT_CEA_720X576P50,         50, SYNC_NEGATIVE, SYNC_NEGATIVE,  17, "V720X576P50"},
	{V1280X800P60RB,   V4L2_DV_BT_DMT_1280X800P60_RB,     60, SYNC_NEGATIVE, SYNC_POSITIVE,   0, "V1280X800P60RB"},
	{V1280X720P50,     V4L2_DV_BT_CEA_1280X720P50,        50, SYNC_POSITIVE, SYNC_POSITIVE,  19, "V1280X720P50"},
	{V1280X720P60,     V4L2_DV_BT_CEA_1280X720P60,        60, SYNC_POSITIVE, SYNC_POSITIVE,   4, "V1280X720P60"},
	{V1366X768P60,     V4L2_DV_BT_DMT_1366X768P60,        60, SYNC_POSITIVE, SYNC_NEGATIVE,   0, "V1366X768P60"},
	{V1280X1024P60,    V4L2_DV_BT_DMT_1280X1024P60,       60, SYNC_POSITIVE, SYNC_POSITIVE,   0, "V1280X1024P60"},
	{V1920X1080P24,    V4L2_DV_BT_CEA_1920X1080P24,       24, SYNC_POSITIVE, SYNC_POSITIVE,  32, "V1920X1080P24"},
	{V1920X1080P25,    V4L2_DV_BT_CEA_1920X1080P25,       25, SYNC_POSITIVE, SYNC_POSITIVE,  33, "V1920X1080P25"},
	{V1920X1080P30,    V4L2_DV_BT_CEA_1920X1080P30,       30, SYNC_POSITIVE, SYNC_POSITIVE,  34, "V1920X1080P30"},
	{V1600X900P59,     V4L2_DV_BT_CVT_1600X900P59_ADDED,  59, SYNC_POSITIVE, SYNC_POSITIVE,   0, "V1600X900P59"},
	{V1600X900P60RB,   V4L2_DV_BT_DMT_1600X900P60_RB,     60, SYNC_POSITIVE, SYNC_POSITIVE,   0, "V1600X900P60RB"},
	{V1920X1080P50,    V4L2_DV_BT_CEA_1920X1080P50,       50, SYNC_POSITIVE, SYNC_POSITIVE,  31, "V1920X1080P50"},
	{V1920X1080P59,    V4L2_DV_BT_CVT_1920X1080P59_ADDED, 59, SYNC_POSITIVE, SYNC_POSITIVE,   0, "V1920X1080P59"},
	{V1920X1080P60,    V4L2_DV_BT_CEA_1920X1080P60,       60, SYNC_POSITIVE, SYNC_POSITIVE,  16, "V1920X1080P60"},
	{V2048X1536P60,    V4L2_DV_BT_CVT_2048X1536P60_ADDED, 60, SYNC_NEGATIVE, SYNC_POSITIVE,   0, "V2048X1536P60"},
	{V1920X1440P60,    V4L2_DV_BT_DMT_1920X1440P60,       60, SYNC_POSITIVE, SYNC_POSITIVE,   0, "V1920X1440P60"},
	{V2560X1440P59,    V4L2_DV_BT_CVT_2560X1440P59_ADDED, 59, SYNC_POSITIVE, SYNC_POSITIVE,   0, "V2560X1440P59"},
	{V1440x2560P60,    V4L2_DV_BT_CVT_1440X2560P60_ADDED, 60, SYNC_POSITIVE, SYNC_POSITIVE,   0, "V1440x2560P60"},
	{V1440x2560P75,    V4L2_DV_BT_CVT_1440X2560P75_ADDED, 75, SYNC_POSITIVE, SYNC_POSITIVE,   0, "V1440x2560P75"},
	{V2560X1440P60,    V4L2_DV_BT_CVT_2560X1440P60_ADDED, 60, SYNC_POSITIVE, SYNC_POSITIVE,   0, "V2560X1440P60"},
	{V3840X2160P24,    V4L2_DV_BT_CEA_3840X2160P24,       24, SYNC_POSITIVE, SYNC_POSITIVE,  93, "V3840X2160P24"},
	{V3840X2160P25,    V4L2_DV_BT_CEA_3840X2160P25,       25, SYNC_POSITIVE, SYNC_POSITIVE,  94, "V3840X2160P25"},
	{V3840X2160P30,    V4L2_DV_BT_CEA_3840X2160P30,       30, SYNC_POSITIVE, SYNC_POSITIVE,  95, "V3840X2160P30"},
	{V4096X2160P24,    V4L2_DV_BT_CEA_4096X2160P24,       24, SYNC_POSITIVE, SYNC_POSITIVE,  98, "V4096X2160P24"},
	{V4096X2160P25,    V4L2_DV_BT_CEA_4096X2160P25,       25, SYNC_POSITIVE, SYNC_POSITIVE,  99, "V4096X2160P25"},
	{V4096X2160P30,    V4L2_DV_BT_CEA_4096X2160P30,       30, SYNC_POSITIVE, SYNC_POSITIVE, 100, "V4096X2160P30"},
	{V3840X2160P59RB,  V4L2_DV_BT_CVT_3840X2160P59_ADDED, 59, SYNC_POSITIVE, SYNC_POSITIVE,   0, "V3840X2160P59RB"},
	{V3840X2160P50,    V4L2_DV_BT_CEA_3840X2160P50,       50, SYNC_POSITIVE, SYNC_POSITIVE,  96, "V3840X2160P50"},
	{V3840X2160P60,    V4L2_DV_BT_CEA_3840X2160P60,       60, SYNC_POSITIVE, SYNC_POSITIVE,  97, "V3840X2160P60"},
	{V4096X2160P50,    V4L2_DV_BT_CEA_4096X2160P50,       50, SYNC_POSITIVE, SYNC_POSITIVE, 101, "V4096X2160P50"},
	{V4096X2160P60,    V4L2_DV_BT_CEA_4096X2160P60,       60, SYNC_POSITIVE, SYNC_POSITIVE, 102, "V4096X2160P60"},
	{V640X10P60SACRC,  V4L2_DV_BT_CVT_640x10P60_ADDED,    60, SYNC_POSITIVE, SYNC_POSITIVE,   0, "V640X10P60SACRC"},
	{VDUMMYTIMING,     V4L2_DV_BT_CVT_640x10P60_ADDED,    60, SYNC_POSITIVE, SYNC_POSITIVE,   0, "VDUMMYTIMING"},
};

const int supported_videos_pre_cnt = ARRAY_SIZE(supported_videos);

static u32 audio_async_m_n[2][4][7] = {
	{	/* M value set */
		{3314, 4567, 4971, 9134, 9942, 18269, 19884},
		{1988, 2740, 2983, 5481, 5695, 10961, 11930},
		{ 994, 1370, 1491, 2740, 2983,  5481,  5965},
		{ 663,  913,  994, 1827, 1988,  3654,  3977},
	},
	{	/* N value set */
		{32768, 32768, 32768, 32768, 32768, 32768, 32768},
		{32768, 32768, 32768, 32768, 32768, 32768, 32768},
		{32768, 32768, 32768, 32768, 32768, 32768, 32768},
		{32768, 32768, 32768, 32768, 32768, 32768, 32768},
	}
};

static u32 audio_sync_m_n[2][4][7] = {
	{	/* M value set */
		{1024, 784, 512, 1568, 1024, 3136, 2048},
		{1024, 784, 512, 1568, 1024, 3136, 2048},
		{1024, 784, 512,  784,  512, 1568, 1024},
		{1024, 784, 512, 1568, 1024, 3136, 2048},
	},
	{	/* N value set */
		{10125,  5625,  3375,  5625,  3375,  5625,  3375},
		{16875,  9375,  5625,  9375,  5625,  9375,  5625},
		{33750, 18750, 11250,  9375,  5625,  9375,  5625},
		{50625, 28125, 16875, 28125, 16875, 28125, 16875},
	}
};

static u32 m_aud_master[7] = {32000, 44100, 48000, 88200, 96000, 176000, 192000};

static u32 n_aud_master[4] = {81000000, 135000000, 270000000, 405000000};

static struct cal_regs_desc regs_dp[REGS_DP_TYPE_MAX][SST_MAX];

static inline u32 dp_cal_read(struct cal_regs_desc regs_desc, u32 reg_id)
{
	return readl(regs_desc.regs + reg_id);
}

static inline u32 dp_cal_read_mask(struct cal_regs_desc regs_desc, u32 reg_id, u32 mask)
{
	u32 val = dp_cal_read(regs_desc, reg_id);

	val &= (mask);
	return val;
}

static inline void dp_cal_write(struct cal_regs_desc regs_desc, u32 reg_id, u32 val)
{
	cal_log_debug(0, "DP %s write [0x%p]:0x%08x\n", regs_desc.name, regs_desc.regs + reg_id, val);
	writel(val, regs_desc.regs + reg_id);
	cal_log_debug(0, "DP %s read  [0x%p]:0x%08x\n", regs_desc.name, regs_desc.regs + reg_id, readl(regs_desc.regs + reg_id));
}

static inline void dp_cal_write_mask(struct cal_regs_desc regs_desc, u32 reg_id, u32 val, u32 mask)
{
	u32 old = dp_cal_read(regs_desc, reg_id);
	u32 bit_shift;

	for (bit_shift = 0; bit_shift < 32U; bit_shift++) {
		if ((mask >> bit_shift) & 0x00000001)
			break;
	}

	val = ((val<<bit_shift) & mask) | (old & ~mask);
	dp_cal_write(regs_desc, reg_id, val);
}

static inline void dp_cal_formal_write_mask(struct cal_regs_desc regs_desc, u32 reg_id, u32 val, u32 mask)
{
	u32 old = dp_cal_read(regs_desc, reg_id);

	val = (val & mask) | (old & ~mask);
	dp_cal_write(regs_desc, reg_id, val);
}

inline u32 dp_read(u32 reg_id)
{
	return dp_cal_read(regs_dp[REGS_LINK][SST1], reg_id);
}

inline u32 dp_read_mask(u32 reg_id, u32 mask)
{
	return dp_cal_read_mask(regs_dp[REGS_LINK][SST1], reg_id, mask);
}

inline void dp_write(u32 reg_id, u32 val)
{
	dp_cal_write(regs_dp[REGS_LINK][SST1], reg_id, val);
}

inline void dp_write_mask(u32 reg_id, u32 val, u32 mask)
{
	dp_cal_write_mask(regs_dp[REGS_LINK][SST1], reg_id, val, mask);
}

inline void dp_formal_write_mask(u32 reg_id, u32 val, u32 mask)
{
	dp_cal_formal_write_mask(regs_dp[REGS_LINK][SST1], reg_id, val, mask);
}

inline u32 dp_phy_read(u32 reg_id)
{
	return dp_cal_read(regs_dp[REGS_PHY][SST1], reg_id);
}

inline u32 dp_phy_read_mask(u32 reg_id, u32 mask)
{
	return dp_cal_read_mask(regs_dp[REGS_PHY][SST1], reg_id, mask);
}

inline void dp_phy_write(u32 reg_id, u32 val)
{
	dp_cal_write(regs_dp[REGS_PHY][SST1], reg_id, val);
}

inline void dp_phy_write_mask(u32 reg_id, u32 val, u32 mask)
{
	dp_cal_write_mask(regs_dp[REGS_PHY][SST1], reg_id, val, mask);
}

inline void dp_phy_formal_write_mask(u32 reg_id, u32 val, u32 mask)
{
	dp_cal_formal_write_mask(regs_dp[REGS_PHY][SST1], reg_id, val, mask);
}

inline u32 dp_phy_tca_read(u32 reg_id)
{
	return dp_cal_read(regs_dp[REGS_PHY_TCA][SST1], reg_id);
}

inline u32 dp_phy_tca_read_mask(u32 reg_id, u32 mask)
{
	return dp_cal_read_mask(regs_dp[REGS_PHY_TCA][SST1], reg_id, mask);
}

inline void dp_phy_tca_write(u32 reg_id, u32 val)
{
	dp_cal_write(regs_dp[REGS_PHY_TCA][SST1], reg_id, val);
}

inline void dp_phy_tca_write_mask(u32 reg_id, u32 val, u32 mask)
{
	dp_cal_write_mask(regs_dp[REGS_PHY_TCA][SST1], reg_id, val, mask);
}

inline void dp_phy_tca_formal_write_mask(u32 reg_id, u32 val, u32 mask)
{
	dp_cal_formal_write_mask(regs_dp[REGS_PHY_TCA][SST1], reg_id, val, mask);
}

void dp_regs_desc_init(void __iomem *regs, const char *name,\
		enum dp_regs_type type, unsigned int id)
{
	cal_regs_desc_check(type, id, REGS_DP_TYPE_MAX, SST_MAX);
	cal_regs_desc_set(regs_dp, regs, name, type, id);
}

void dp_reg_gf_clk_mux_sel(void)
{
	/* Program the GFCLK MUX select line to pass
	 * 10bit and 20bit clocks to the logic
	 */
	dp_formal_write_mask(SYSTEM_CLK_CONTROL,
			(0x1 << 4) | (0x1 << 5),
			GFCLKMUX_SEL_20 | GFCLKMUX_SEL_10);
}

void dp_reg_gf_clk_mux_sel_osc(void)
{
	cal_log_debug(0, "%s\n", __func__);
	dp_formal_write_mask(SYSTEM_CLK_CONTROL,
		(0x0 << 4) | (0x0 << 5), GFCLKMUX_SEL_20 | GFCLKMUX_SEL_10);
}

void dp_reg_gf_clk_mux_sel_tx(void)
{
	cal_log_debug(0, "%s\n", __func__);
	/* Program the GFCLK MUX select line
	 * to pass 10bit and 20bit clocks to the logic
	 */
	dp_formal_write_mask(SYSTEM_CLK_CONTROL,
		(0x1 << 4) | (0x1 << 5), GFCLKMUX_SEL_20 | GFCLKMUX_SEL_10);
}

void dp_reg_sw_reset(struct dp_cal_res *cal_res)
{
	u32 cnt = 10;
	u32 state;

#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
	//dwc3_exynos_phy_enable(1, 1);
	//atomic_inc(&cal_res->usbdp_phy_en_cnt);
#endif
	cal_log_info(0, "%s\n", __func__);
	dp_reg_osc_clk_qch_con(true);

	cal_log_debug(0, "SYSTEM_SW_RESET_CONTROL(-)[0x%p]:0x%08x\n",
			(void __iomem *)0x10A80000 + SYSTEM_SW_RESET_CONTROL,
			dp_read(SYSTEM_SW_RESET_CONTROL));

	cal_log_debug(0, "SYSTEM_CLK_CONTROL[0x%p]:0x%08x\n",
			(void __iomem *)0x10A80000 + SYSTEM_CLK_CONTROL,
			dp_read(SYSTEM_CLK_CONTROL));

	dp_write_mask(SYSTEM_SW_RESET_CONTROL, ~0, SW_RESET);
	cal_log_debug(0, "SYSTEM_SW_RESET_CONTROL(1)[0x%p]:0x%08x\n",
			(void __iomem *)0x10A80000 + SYSTEM_SW_RESET_CONTROL,
			dp_read(SYSTEM_SW_RESET_CONTROL));

	cal_log_debug(0, "SYSTEM_CLK_CONTROL[0x%p]:0x%08x\n",
			(void __iomem *)0x10A80000 + SYSTEM_CLK_CONTROL,
			dp_read(SYSTEM_CLK_CONTROL));

	udelay(1);
	dp_write_mask(SYSTEM_SW_RESET_CONTROL, 0, SW_RESET);
	cal_log_debug(0, "SYSTEM_SW_RESET_CONTROL(0)[0x%p]:0x%08x\n",
			(void __iomem *)0x10A80000 + SYSTEM_SW_RESET_CONTROL,
			dp_read(SYSTEM_SW_RESET_CONTROL));
	cal_log_debug(0, "SYSTEM_CLK_CONTROL[0x%p]:0x%08x\n",
			(void __iomem *)0x10A80000 + SYSTEM_CLK_CONTROL,
			dp_read(SYSTEM_CLK_CONTROL));

	do {
		state = dp_read(SYSTEM_SW_RESET_CONTROL) & SW_RESET;
		cnt--;
		udelay(1);
	} while (state && cnt);

	if (!cnt)
		cal_log_err(0, "%s is timeout.\n", __func__);
}

int dp_reg_usbdrd_qch_en(u32 en)
{
	void __iomem *cmu_regs = NULL;
	static u32 usbdrd_qch;
	u32 ret = 0;

	cmu_regs = ioremap((phys_addr_t)0x10A00000 + 0x3058, 0x8);
	if (!cmu_regs) {
		pr_err("QCH_CON_USB32DRD_QCH_S_CTRL SFR ioremap is failed\n");
		return -EINVAL;
	}

	if (en) {
		pr_info("USB32DRD_QCH OFF\n");
		usbdrd_qch = readl(cmu_regs);
		pr_info("QCH_CON_USB32DRD_QCH_S_CTRL[0x10A03058](0x%08x)\n", readl(cmu_regs));
		writel(0x6, cmu_regs);
		pr_info("QCH_CON_USB32DRD_QCH_S_CTRL[0x10A03058](0x%08x)\n", readl(cmu_regs));
	} else {
		pr_info("USB32DRD_QCH ON(restor)\n");
		pr_info("QCH_CON_USB32DRD_QCH_S_CTRL[0x10A03058](0x%08x)\n", readl(cmu_regs));
		writel(usbdrd_qch, cmu_regs);
		pr_info("QCH_CON_USB32DRD_QCH_S_CTRL[0x10A03058](0x%08x)\n", readl(cmu_regs));
	}

	return ret;
}

void dp_reg_phy_update(u32 req_status_val, u32 req_status_mask)
{
	u32 val = 0;
	u32 cnt = 500;
	struct dp_device *dp = get_dp_drvdata();

	cal_log_debug(0, "%s +\n", __func__);
	// USBDP PHY MPLLB reset assertion
	dp_phy_formal_write_mask(DP_CONFIG12, req_status_val, req_status_mask);

	do {
		val = dp_phy_read_mask(DP_CONFIG12, req_status_mask);
		udelay(10);
		cnt--;
	} while (val != 0x0 && cnt > 0);

	if (!cnt) {
		cal_log_err(0, "val(0x%08x) DP PHY DP_CONFIG12 [0x%p]:0x%08x\n",
				val,
				dp->res.phy_regs + DP_CONFIG12,
				readl(dp->res.phy_regs + DP_CONFIG12));

		cal_log_err(0, "Fail to reset mpllb\n");
		return;
	}
	cal_log_debug(0, "%s -\n", __func__);
}

void dp_reg_phy_reset(u32 en)
{
	u32 req_status_val, req_status_mask;

	cal_log_debug(0, "%s +\n", __func__);
	// USBDP PHY MPLLB reset assertion
#if 0
	dp_phy_write_mask(DP_CONFIG12, 0x0, DP_TX0_MPLL_EN);
	dp_phy_write_mask(DP_CONFIG12, 0x0, DP_TX1_MPLL_EN);
	dp_phy_write_mask(DP_CONFIG12, 0x0, DP_TX2_MPLL_EN);
	dp_phy_write_mask(DP_CONFIG12, 0x0, DP_TX3_MPLL_EN);
#endif
	dp_phy_write_mask(DP_CONFIG12, 0x1, DP_TX0_MPLL_EN);
	dp_phy_write_mask(DP_CONFIG12, 0x1, DP_TX1_MPLL_EN);
	dp_phy_write_mask(DP_CONFIG12, 0x1, DP_TX2_MPLL_EN);
	dp_phy_write_mask(DP_CONFIG12, 0x1, DP_TX3_MPLL_EN);

	dp_phy_write_mask(DP_CONFIG11, 0x0, DP_TX0_PSTATE);
	dp_phy_write_mask(DP_CONFIG11, 0x0, DP_TX1_PSTATE);
	dp_phy_write_mask(DP_CONFIG11, 0x0, DP_TX2_PSTATE);
	dp_phy_write_mask(DP_CONFIG11, 0x0, DP_TX3_PSTATE);

	req_status_val = req_status_mask =
		DP_TX0_REQ_STATUS | DP_TX1_REQ_STATUS |
		DP_TX2_REQ_STATUS | DP_TX3_REQ_STATUS;

	dp_reg_phy_update(req_status_val, req_status_mask);
	cal_log_debug(0, "%s -\n", __func__);
}

void dp_reg_phy_tca_mode_change(void)
{
	unsigned int cnt = 0;
	unsigned int val = 0;

	dp_phy_write_mask(TCA_STATE_CTRL, 0x0, SS_RXDET_DISABLE_ACK);
	dp_phy_write_mask(TCA_STATE_CTRL, 0x1, SS_RXDET_DISABLE_ACK);
	cal_log_info(0, "Wait to change from USB to DP2/4\n");

	cnt = 2500;
	do {
		val = dp_phy_tca_read_mask(TCA_REG_TCA_INTR_STS, TCA_REG_XA_ACK_EVT);
		udelay(1000);
		cnt--;
	} while (val == 0 && cnt > 0);

	if (!cnt) {
		cal_log_err(0, "val(0x%08x) DP PHY tca read  [0x%p]:0x%08x\n", val,
				regs_dp[REGS_PHY_TCA][SST1].regs + TCA_REG_TCA_INTR_STS,
				readl(regs_dp[REGS_PHY_TCA][SST1].regs + TCA_REG_TCA_INTR_STS));

		cal_log_err(0, "Fail to change mode from USB to DP2/4\n");
	}
}

void dp_reg_phy_init_setting(void)
{
	void __iomem    *cmu_regs;

	cmu_regs = ioremap(0x10A00620, 0x08);
	cal_log_debug(0, "PLL CLOCK SFR for DP OSC\n");
	cal_log_debug(0, "ADDR:0x10A00620(0x%08x)\n", readl(cmu_regs));

	cal_log_debug(0, "PHY init common setting\n");
	/* USBDP PHY common */
#if 0
	/* TODO: optimize PHY init seq. */
	cal_log_debug(0, "KMS Add USB CR setting!!!!, disable setting by design\n");
	cal_log_debug(0, "KMS PHY CONFIG0 mplla force en, pwr stable set\n");
	dp_phy_write_mask(PHY_CONFIG0, 0x1, PHY0_SS_MPLLA_FORCE_EN);
	dp_phy_write_mask(PHY_CONFIG0, 0x1, PHY0_PMA_PWR_STABLE);
	dp_phy_write_mask(PHY_CONFIG0, 0x1, PHY0_PCS_PWR_STABLE);
	cal_log_debug(0, "KMS mpllb en\n");
	dp_phy_write_mask(DP_CONFIG12, 0x1, DP_TX3_MPLL_EN);
	dp_phy_write_mask(DP_CONFIG12, 0x1, DP_TX2_MPLL_EN);
	dp_phy_write_mask(DP_CONFIG12, 0x1, DP_TX1_MPLL_EN);
	dp_phy_write_mask(DP_CONFIG12, 0x1, DP_TX0_MPLL_EN);
	cal_log_debug(0, "KMS swap aux lane!!!!\n");
	dp_phy_write(DP_AUX_CONFIG0, 0x00002321);
#endif
	cal_log_debug(0, "non-swap aux lane\n");
	dp_phy_write_mask(DP_AUX_CONFIG0, 0x1, AUX_PWDNB);
	dp_phy_write_mask(DP_CONFIG13, 0x1, DP_TX0_RESET);
	dp_phy_write_mask(DP_CONFIG13, 0x1, DP_TX1_RESET);
	dp_phy_write_mask(DP_CONFIG13, 0x1, DP_TX2_RESET);
	dp_phy_write_mask(DP_CONFIG13, 0x1, DP_TX3_RESET);
	dp_phy_write_mask(DP_CONFIG11, 0x3, DP_TX0_PSTATE);
	dp_phy_write_mask(DP_CONFIG11, 0x3, DP_TX1_PSTATE);
	dp_phy_write_mask(DP_CONFIG11, 0x3, DP_TX2_PSTATE);
	dp_phy_write_mask(DP_CONFIG11, 0x3, DP_TX3_PSTATE);
	dp_phy_write_mask(DP_CONFIG13, 0x0, DP_TX0_RESET);
	dp_phy_write_mask(DP_CONFIG13, 0x0, DP_TX1_RESET);
	dp_phy_write_mask(DP_CONFIG13, 0x0, DP_TX2_RESET);
	dp_phy_write_mask(DP_CONFIG13, 0x0, DP_TX3_RESET);
	cal_log_debug(0, "DP CONFIG13 3 -> f\n");
	dp_phy_write_mask(DP_CONFIG13, 0x1, DP_TX0_DISABLE);
	dp_phy_write_mask(DP_CONFIG13, 0x1, DP_TX1_DISABLE);
	dp_phy_write_mask(DP_CONFIG13, 0x1, DP_TX2_DISABLE);
	dp_phy_write_mask(DP_CONFIG13, 0x1, DP_TX3_DISABLE);
	dp_phy_write_mask(DP_CONFIG19, 0x1, DPALT_DISABLE_ACK);
	dp_phy_write_mask(TCA_STATE_CTRL, 0x1, PIPE_LANE0_POWERDOWN_OVRD_EN);
	dp_phy_write_mask(TCA_STATE_CTRL, 0x1, SS_RXDET_DISABLE_ACK);
	dp_phy_write_mask(TCA_STATE_CTRL, 0x1, SS_RXDET_DISABLE_ACK_OVRD_EN);
}

void dp_reg_phy_mode_setting(struct dp_cal_res *cal_res)
{
	u32 mask = 0, val = 0;
	u32 tcpc_mux_con = 0;
	u32 tcpc_orientation = 0;
	u32 width_val = 0, width_mask = 0;
	u32 mpll_en_val = 0, mpll_en_mask = 0;
	u32 req_status_val = 0, req_status_mask = 0;
	u32 disable_mask = 0;

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	cal_log_debug(0, "CONFIG_USB_TYPEC_MANAGER_NOTIFIER\n");
	switch (cal_res->pdic_notify_dp_conf) {
	case PDIC_NOTIFY_DP_PIN_UNKNOWN:
		cal_log_debug(0, "PDIC_NOTIFY_DP_PIN_UNKNOWN\n");
		break;

	case PDIC_NOTIFY_DP_PIN_A:
		cal_log_debug(0, "PDIC_NOTIFY_DP_PIN_A\n");
		break;
	case PDIC_NOTIFY_DP_PIN_B:
		cal_log_debug(0, "PDIC_NOTIFY_DP_PIN_B\n");
		break;
	case PDIC_NOTIFY_DP_PIN_C:
		cal_log_debug(0, "PDIC_NOTIFY_DP_PIN_C\n");
	case PDIC_NOTIFY_DP_PIN_E:
		cal_log_debug(0, "PDIC_NOTIFY_DP_PIN_E\n");
#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
		exynos_usbdrd_inform_dp_use(1, 4);
#endif
		tcpc_mux_con = 0x2;
		if (cal_res->dp_sw_sel) {
			tcpc_orientation = 0;
		} else {
			tcpc_orientation = 0;
		}
		width_val = width_mask =
			DP_TX0_WIDTH | DP_TX1_WIDTH |
			DP_TX2_WIDTH | DP_TX3_WIDTH;
		mpll_en_val = mpll_en_mask =
			DP_TX0_MPLL_EN | DP_TX1_MPLL_EN |
			DP_TX2_MPLL_EN | DP_TX3_MPLL_EN;
		req_status_val = req_status_mask =
			DP_TX0_REQ_STATUS | DP_TX1_REQ_STATUS |
			DP_TX2_REQ_STATUS | DP_TX3_REQ_STATUS;
		disable_mask =
			DP_TX0_DISABLE | DP_TX1_DISABLE |
			DP_TX2_DISABLE | DP_TX3_DISABLE;
		break;

	case PDIC_NOTIFY_DP_PIN_D:
		cal_log_debug(0, "PDIC_NOTIFY_DP_PIN_D\n");
	case PDIC_NOTIFY_DP_PIN_F:
		cal_log_debug(0, "PDIC_NOTIFY_DP_PIN_F\n");
#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
		exynos_usbdrd_inform_dp_use(1, 2);
#endif
		tcpc_mux_con = 0x3;
		if (cal_res->dp_sw_sel) {
			tcpc_orientation = 0;
		} else {
			tcpc_orientation = 0;
		}
		width_val = width_mask =
			DP_TX0_WIDTH | DP_TX1_WIDTH;
		mpll_en_val = mpll_en_mask =
			DP_TX0_MPLL_EN | DP_TX1_MPLL_EN;
		req_status_val = req_status_mask =
			DP_TX0_REQ_STATUS | DP_TX1_REQ_STATUS;
		disable_mask =
			DP_TX0_DISABLE | DP_TX1_DISABLE;
		break;

	default:
		cal_log_debug(0, "PDIC_NOTIFY_DP_PIN_UNKNOWN\n");
		break;
	}
#elif IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	cal_log_info(0, "CONFIG_IFCONN_NOTIFIER\n");
	switch (cal_res->ifconn_notify_dp_conf) {
	case IFCONN_NOTIFY_DP_PIN_UNKNOWN:
		cal_log_debug(0, "IFCONN_NOTIFY_DP_PIN_UNKNOWN\n");
		break;

	case IFCONN_NOTIFY_DP_PIN_A:
		cal_log_info(0, "IFCONN_NOTIFY_DP_PIN_A\n");
		break;
	case IFCONN_NOTIFY_DP_PIN_B:
		cal_log_info(0, "IFCONN_NOTIFY_DP_PIN_B\n");
		break;

	case IFCONN_NOTIFY_DP_PIN_C:
		cal_log_info(0, "IFCONN_NOTIFY_DP_PIN_C\n");
	case IFCONN_NOTIFY_DP_PIN_E:
		cal_log_info(0, "IFCONN_NOTIFY_DP_PIN_E\n");
#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
		exynos_usbdrd_inform_dp_use(1, 4);
#endif
		tcpc_mux_con = 0x2;
		if (cal_res->dp_sw_sel) {
			tcpc_orientation = 0;
		} else {
			tcpc_orientation = 0;
		}
		width_val = width_mask =
			DP_TX0_WIDTH | DP_TX1_WIDTH |
			DP_TX2_WIDTH | DP_TX3_WIDTH;
		mpll_en_val = mpll_en_mask =
			DP_TX0_MPLL_EN | DP_TX1_MPLL_EN |
			DP_TX2_MPLL_EN | DP_TX3_MPLL_EN;
		req_status_val = req_status_mask =
			DP_TX0_REQ_STATUS | DP_TX1_REQ_STATUS |
			DP_TX2_REQ_STATUS | DP_TX3_REQ_STATUS;
		disable_mask =
			DP_TX0_DISABLE | DP_TX1_DISABLE |
			DP_TX2_DISABLE | DP_TX3_DISABLE;
		break;

	case IFCONN_NOTIFY_DP_PIN_D:
		cal_log_info(0, "IFCONN_NOTIFY_DP_PIN_D\n");
	case IFCONN_NOTIFY_DP_PIN_F:
		cal_log_info(0, "IFCONN_NOTIFY_DP_PIN_F\n");
#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
		exynos_usbdrd_inform_dp_use(1, 2);
#endif
		tcpc_mux_con = 0x3;
		if (cal_res->dp_sw_sel) {
			tcpc_orientation = 0;
		} else {
			tcpc_orientation = 0;
		}
		width_val = width_mask =
			DP_TX0_WIDTH | DP_TX1_WIDTH;
		mpll_en_val = mpll_en_mask =
			DP_TX0_MPLL_EN | DP_TX1_MPLL_EN;
		req_status_val = req_status_mask =
			DP_TX0_REQ_STATUS | DP_TX1_REQ_STATUS;
		disable_mask =
			DP_TX0_DISABLE | DP_TX1_DISABLE;
		break;

	default:
		cal_log_debug(0, "IFCONN_NOTIFY_DP_PIN_UNKNOWN\n");
		break;
	}
#endif

	cal_log_debug(0, "direction_%s\n", tcpc_orientation ? "flipped":"origin");
	cal_log_debug(0, "Start USB to DP2/4\n");
	/* Start USB to DP4 : COMMON */
	/* read : 0x00000001 */
	udelay(1000);
	cal_log_debug(0, "TCA_REG_TCA_TCPC(0x1 == 0x%08x)\n", dp_phy_tca_read(TCA_REG_TCA_TCPC));
	val = (tcpc_mux_con << 0)
		| (tcpc_orientation << 2)
		| (0x0 << 3)
		| (0x1 << 4);
	mask = TCA_REG_TCPC_MUX_CONTROL
		| TCA_REG_TCPC_CONNECTOR_ORIENTATION
		| TCA_REG_TCPC_LOW_POWER_EN
		| TCA_REG_TCPC_VALID;
	cal_log_debug(0, "val(0x%08x)\n", val);
	cal_log_debug(0, "mask(0x%08x)\n", mask);
	dp_phy_tca_formal_write_mask(TCA_REG_TCA_TCPC, val, mask);

	cal_log_debug(0, "TCA_REG_TCA_TCPC(0x%08x)\n", dp_phy_tca_read(TCA_REG_TCA_TCPC));
	dp_reg_phy_tca_mode_change();

	dp_phy_formal_write_mask(DP_CONFIG13, 0x0, disable_mask);
	dp_phy_write_mask(DP_CONFIG19, 0x0, DPALT_DISABLE_ACK);

	dp_phy_formal_write_mask(DP_CONFIG12, width_val, width_mask);
	dp_phy_formal_write_mask(DP_CONFIG12, mpll_en_val, mpll_en_mask);

	dp_reg_phy_update(req_status_val, req_status_mask);
}

void dp_reg_phy_ssc_enable(u32 en)
{
	/* TODO: Check SSC enable */
	//dp_phy_write_mask(CMN_REG00B8, en, SSC_EN);
}

void dp_reg_wait_phy_pll_lock(void)
{
	u32 cnt = 2000;	/* wait for 150us + 10% margin */
	u32 state;

	do {
		state = dp_read(SYSTEM_PLL_LOCK_CONTROL) & PLL_LOCK_STATUS;
		cnt--;
		udelay(1000);
	} while (!state && cnt);

	if (!cnt) {
		cal_log_err(0, "%s - is timeout.\n", __func__);
		dp_debug_dump();
		return;
	}

	dp_reg_gf_clk_mux_sel_tx();
}

void dp_reg_phy_set_link_bw(struct dp_cal_res *cal_res, u8 link_rate)
{
	cal_res->g_link_rate = link_rate;

	switch (link_rate) {
	case LINK_RATE_8_1Gbps:
		cal_log_info(0, "PHY init setting for LINK_RATE_8_1Gbps\n");
		dp_phy_write_mask(DP_CONFIG1, 0xe, DP_MPLLB_CP_INT);
		dp_phy_write_mask(DP_CONFIG1, 0x43, DP_MPLLB_CP_INT_GS);
		dp_phy_write_mask(DP_CONFIG1, 0x19, DP_MPLLB_CP_PROP);
		dp_phy_write_mask(DP_CONFIG1, 0x7f, DP_MPLLB_CP_PROP_GS);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_DIV5_CLK_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x0, DP_MPLLB_DIV_CLK_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x0, DP_MPLLB_DIV_MULTIPLIER);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FORCE_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_CFG_UPDATE_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_DEN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_EN);
		dp_phy_write_mask(DP_CONFIG3, 0xf000, DP_MPLLB_FRACN_QUOT);
		dp_phy_write_mask(DP_CONFIG3, 0x0, DP_MPLLB_FRACN_REM);
		dp_phy_write_mask(DP_CONFIG4, 0x2, DP_MPLLB_FREQ_VCO);
		dp_phy_write_mask(DP_CONFIG4, 0x0, DP_MPLLB_INIT_CAL_DISABLE);
		dp_phy_write_mask(DP_CONFIG4, 0x184, DP_MPLLB_MULTIPLIER);
		dp_phy_write_mask(DP_CONFIG4, 0x1, DP_MPLLB_PMIX_EN);
		dp_phy_write_mask(DP_CONFIG4, 0x0, DP_MPLLB_SSC_EN);
		dp_phy_write_mask(DP_CONFIG5, 0xf300, DP_MPLLB_SSC_PEAK);
		dp_phy_write_mask(DP_CONFIG6, 0x1983d, DP_MPLLB_SSC_STEPSIZE);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_SSC_UP_SPREAD);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_TX_CLK_DIV);
		dp_phy_write_mask(DP_CONFIG7, 0x3, DP_MPLLB_V2I);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_WORD_DIV2_EN);

		dp_phy_write_mask(DP_CONFIG7, 0x1, DP_REF_CLK_EN);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX0_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX1_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX2_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX3_DCC_BYP_AC_CAP);
		break;
	case LINK_RATE_5_4Gbps:
		cal_log_debug(0, "PHY init setting for LINK_RATE_5_4Gbps\n");
		/* USBDP PHY difference, HBR2_5_4_GBPS */
		dp_phy_write_mask(DP_CONFIG1, 0xe, DP_MPLLB_CP_INT);
		dp_phy_write_mask(DP_CONFIG1, 0x43, DP_MPLLB_CP_INT_GS);
		dp_phy_write_mask(DP_CONFIG1, 0x14, DP_MPLLB_CP_PROP);
		dp_phy_write_mask(DP_CONFIG1, 0x7f, DP_MPLLB_CP_PROP_GS);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_DIV5_CLK_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x0, DP_MPLLB_DIV_CLK_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x0, DP_MPLLB_DIV_MULTIPLIER);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FORCE_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_CFG_UPDATE_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_DEN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_EN);
		dp_phy_write_mask(DP_CONFIG3, 0xa000, DP_MPLLB_FRACN_QUOT);
		dp_phy_write_mask(DP_CONFIG3, 0x0, DP_MPLLB_FRACN_REM);
		dp_phy_write_mask(DP_CONFIG4, 0x3, DP_MPLLB_FREQ_VCO);
		dp_phy_write_mask(DP_CONFIG4, 0x0, DP_MPLLB_INIT_CAL_DISABLE);
		dp_phy_write_mask(DP_CONFIG4, 0xf8, DP_MPLLB_MULTIPLIER);
		dp_phy_write_mask(DP_CONFIG4, 0x1, DP_MPLLB_PMIX_EN);
		dp_phy_write_mask(DP_CONFIG4, 0x0, DP_MPLLB_SSC_EN);
		dp_phy_write_mask(DP_CONFIG5, 0xa200, DP_MPLLB_SSC_PEAK);
		dp_phy_write_mask(DP_CONFIG6, 0x11029, DP_MPLLB_SSC_STEPSIZE);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_SSC_UP_SPREAD);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_TX_CLK_DIV);
		dp_phy_write_mask(DP_CONFIG7, 0x3, DP_MPLLB_V2I);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_WORD_DIV2_EN);

		dp_phy_write_mask(DP_CONFIG7, 0x1, DP_REF_CLK_EN);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX0_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX1_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX2_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX3_DCC_BYP_AC_CAP);
		break;
	case LINK_RATE_2_7Gbps:
		cal_log_debug(0, "PHY init setting for LINK_RATE_2_7Gbps\n");
		/* USBDP PHY difference, HBR_2_7_GBPS */
		dp_phy_write_mask(DP_CONFIG1, 0xe, DP_MPLLB_CP_INT);
		dp_phy_write_mask(DP_CONFIG1, 0x43, DP_MPLLB_CP_INT_GS);
		dp_phy_write_mask(DP_CONFIG1, 0x14, DP_MPLLB_CP_PROP);
		dp_phy_write_mask(DP_CONFIG1, 0x7f, DP_MPLLB_CP_PROP_GS);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_DIV5_CLK_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x0, DP_MPLLB_DIV_CLK_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x0, DP_MPLLB_DIV_MULTIPLIER);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FORCE_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_CFG_UPDATE_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_DEN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_EN);
		dp_phy_write_mask(DP_CONFIG3, 0xa000, DP_MPLLB_FRACN_QUOT);
		dp_phy_write_mask(DP_CONFIG3, 0x0, DP_MPLLB_FRACN_REM);
		dp_phy_write_mask(DP_CONFIG4, 0x3, DP_MPLLB_FREQ_VCO);
		dp_phy_write_mask(DP_CONFIG4, 0x0, DP_MPLLB_INIT_CAL_DISABLE);
		dp_phy_write_mask(DP_CONFIG4, 0xf8, DP_MPLLB_MULTIPLIER);
		dp_phy_write_mask(DP_CONFIG4, 0x1, DP_MPLLB_PMIX_EN);
		dp_phy_write_mask(DP_CONFIG4, 0x0, DP_MPLLB_SSC_EN);
		dp_phy_write_mask(DP_CONFIG5, 0xa200, DP_MPLLB_SSC_PEAK);
		dp_phy_write_mask(DP_CONFIG6, 0x11029, DP_MPLLB_SSC_STEPSIZE);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_SSC_UP_SPREAD);
		dp_phy_write_mask(DP_CONFIG7, 0x1, DP_MPLLB_TX_CLK_DIV);
		dp_phy_write_mask(DP_CONFIG7, 0x3, DP_MPLLB_V2I);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_WORD_DIV2_EN);

		dp_phy_write_mask(DP_CONFIG7, 0x1, DP_REF_CLK_EN);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX0_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX1_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX2_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x0, DP_TX3_DCC_BYP_AC_CAP);
		break;
	case LINK_RATE_1_62Gbps:
		cal_log_debug(0, "PHY init setting for LINK_RATE_1_62Gbps\n");
		/* USBDP PHY difference, RBR_1_6_GBPS */
		dp_phy_write_mask(DP_CONFIG1, 0xe, DP_MPLLB_CP_INT);
		dp_phy_write_mask(DP_CONFIG1, 0x41, DP_MPLLB_CP_INT_GS);
		dp_phy_write_mask(DP_CONFIG1, 0x3, DP_MPLLB_CP_PROP);
		dp_phy_write_mask(DP_CONFIG1, 0x7f, DP_MPLLB_CP_PROP_GS);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_DIV5_CLK_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x0, DP_MPLLB_DIV_CLK_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x0, DP_MPLLB_DIV_MULTIPLIER);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FORCE_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_CFG_UPDATE_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_DEN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_EN);
		dp_phy_write_mask(DP_CONFIG3, 0xc000, DP_MPLLB_FRACN_QUOT);
		dp_phy_write_mask(DP_CONFIG3, 0x0, DP_MPLLB_FRACN_REM);
		dp_phy_write_mask(DP_CONFIG4, 0x3, DP_MPLLB_FREQ_VCO);
		dp_phy_write_mask(DP_CONFIG4, 0x0, DP_MPLLB_INIT_CAL_DISABLE);
		dp_phy_write_mask(DP_CONFIG4, 0x130, DP_MPLLB_MULTIPLIER);
		dp_phy_write_mask(DP_CONFIG4, 0x1, DP_MPLLB_PMIX_EN);
		dp_phy_write_mask(DP_CONFIG4, 0x0, DP_MPLLB_SSC_EN);
		dp_phy_write_mask(DP_CONFIG5, 0xc266, DP_MPLLB_SSC_PEAK);
		dp_phy_write_mask(DP_CONFIG6, 0x14698, DP_MPLLB_SSC_STEPSIZE);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_SSC_UP_SPREAD);
		dp_phy_write_mask(DP_CONFIG7, 0x2, DP_MPLLB_TX_CLK_DIV);
		dp_phy_write_mask(DP_CONFIG7, 0x2, DP_MPLLB_V2I);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_WORD_DIV2_EN);

		dp_phy_write_mask(DP_CONFIG7, 0x1, DP_REF_CLK_EN);
		dp_phy_write_mask(DP_CONFIG17, 0x1, DP_TX0_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x1, DP_TX1_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x1, DP_TX2_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x1, DP_TX3_DCC_BYP_AC_CAP);
		break;
	default:
		cal_log_debug(0, "PHY init setting for LINK_RATE_1_62Gbps\n");
		/* USBDP PHY difference, RBR_1_6_GBPS */
		dp_phy_write_mask(DP_CONFIG1, 0xe, DP_MPLLB_CP_INT);
		dp_phy_write_mask(DP_CONFIG1, 0x41, DP_MPLLB_CP_INT_GS);
		dp_phy_write_mask(DP_CONFIG1, 0x3, DP_MPLLB_CP_PROP);
		dp_phy_write_mask(DP_CONFIG1, 0x7f, DP_MPLLB_CP_PROP_GS);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_DIV5_CLK_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x0, DP_MPLLB_DIV_CLK_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x0, DP_MPLLB_DIV_MULTIPLIER);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FORCE_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_CFG_UPDATE_EN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_DEN);
		dp_phy_write_mask(DP_CONFIG2, 0x1, DP_MPLLB_FRACN_EN);
		dp_phy_write_mask(DP_CONFIG3, 0xc000, DP_MPLLB_FRACN_QUOT);
		dp_phy_write_mask(DP_CONFIG3, 0x0, DP_MPLLB_FRACN_REM);
		dp_phy_write_mask(DP_CONFIG4, 0x3, DP_MPLLB_FREQ_VCO);
		dp_phy_write_mask(DP_CONFIG4, 0x0, DP_MPLLB_INIT_CAL_DISABLE);
		dp_phy_write_mask(DP_CONFIG4, 0x130, DP_MPLLB_MULTIPLIER);
		dp_phy_write_mask(DP_CONFIG4, 0x1, DP_MPLLB_PMIX_EN);
		dp_phy_write_mask(DP_CONFIG4, 0x0, DP_MPLLB_SSC_EN);
		dp_phy_write_mask(DP_CONFIG5, 0xc266, DP_MPLLB_SSC_PEAK);
		dp_phy_write_mask(DP_CONFIG6, 0x14698, DP_MPLLB_SSC_STEPSIZE);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_SSC_UP_SPREAD);
		dp_phy_write_mask(DP_CONFIG7, 0x2, DP_MPLLB_TX_CLK_DIV);
		dp_phy_write_mask(DP_CONFIG7, 0x2, DP_MPLLB_V2I);
		dp_phy_write_mask(DP_CONFIG7, 0x0, DP_MPLLB_WORD_DIV2_EN);

		dp_phy_write_mask(DP_CONFIG7, 0x1, DP_REF_CLK_EN);
		dp_phy_write_mask(DP_CONFIG17, 0x1, DP_TX0_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x1, DP_TX1_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x1, DP_TX2_DCC_BYP_AC_CAP);
		dp_phy_write_mask(DP_CONFIG17, 0x1, DP_TX3_DCC_BYP_AC_CAP);
	}
	cal_log_debug(0, "%s link_rate(0x%08x)\n",
			__func__, dp_reg_phy_get_link_bw(cal_res));
}
void dp_reg_phy_print_link_bw(struct dp_cal_res *cal_res)
{
	u32 val = cal_res->g_link_rate;
	switch (val) {
	case LINK_RATE_8_1Gbps:
		cal_log_info(0, "PHY init setting for LINK_RATE_8_1Gbps\n");
		break;
	case LINK_RATE_5_4Gbps:
		cal_log_info(0, "PHY init setting for LINK_RATE_5_4Gbps\n");
		break;
	case LINK_RATE_2_7Gbps:
		cal_log_info(0, "PHY init setting for LINK_RATE_2_7Gbps\n");
		break;
	case LINK_RATE_1_62Gbps:
		cal_log_info(0, "PHY init setting for LINK_RATE_1_62Gbps\n");
		break;
	default:
		cal_log_info(0, "PHY init setting for LINK_RATE_1_62Gbps\n");
	}
}

u32 dp_reg_phy_get_link_bw(struct dp_cal_res *cal_res)
{
	return cal_res->g_link_rate;
}

void dp_reg_set_lane_count(u8 lane_cnt)
{
	dp_write(SYSTEM_MAIN_LINK_LANE_COUNT, lane_cnt);
}

u32 dp_reg_get_lane_count(void)
{
	return dp_read(SYSTEM_MAIN_LINK_LANE_COUNT);
}

void dp_reg_set_training_pattern(dp_training_pattern pattern)
{
	dp_write_mask(PCS_TEST_PATTERN_CONTROL, 0, LINK_QUALITY_PATTERN_SET);
	dp_write_mask(PCS_CONTROL, pattern, LINK_TRAINING_PATTERN_SET);

	if (pattern == NORAMAL_DATA || pattern == TRAINING_PATTERN_4)
		dp_write_mask(PCS_CONTROL, 0, SCRAMBLE_BYPASS);
	else
		dp_write_mask(PCS_CONTROL, 1, SCRAMBLE_BYPASS);
}

void dp_reg_set_qual_pattern(dp_qual_pattern pattern, dp_scrambling scramble)
{
	dp_write_mask(PCS_CONTROL, 0, LINK_TRAINING_PATTERN_SET);
	dp_write_mask(PCS_TEST_PATTERN_CONTROL, pattern, LINK_QUALITY_PATTERN_SET);
	dp_write_mask(PCS_CONTROL, scramble, SCRAMBLE_BYPASS);
}

void dp_reg_set_hbr2_scrambler_reset(u32 uResetCount)
{
	uResetCount /= 2;       /* only even value@Istor EVT1, ?*/
	dp_write_mask(PCS_HBR2_EYE_SR_CONTROL, uResetCount, HBR2_EYE_SR_COUNT);
}

void dp_reg_set_pattern_PLTPAT(void)
{
	dp_write(PCS_TEST_PATTERN_SET0, 0x3E0F83E0);	/* 00111110 00001111 10000011 11100000 */
	dp_write(PCS_TEST_PATTERN_SET1, 0x0F83E0F8);	/* 00001111 10000011 11100000 11111000 */
	dp_write(PCS_TEST_PATTERN_SET2, 0x0000F83E);	/* 11111000 00111110 */
}

void dp_reg_set_phy_tune(struct dp_cal_res *cal_res, u32 phy_lane_num, u32 amp_lvl, u32 pre_emp_lvl)
{
	struct dp_device *dp = get_dp_drvdata();
	u8 link_rate = dp_reg_phy_get_link_bw(cal_res);
	u32 val = 0;
	u32 mask = 0;
	int i = 0;

	cal_log_debug(0, "phy_lane_num(%d), amp_lvl(%d), pre_emp_lvl(%d)\n", phy_lane_num, amp_lvl, pre_emp_lvl);
	for (i = EQ_MAIN; i <= RBOOST; i++) {
		val = mask = 0;
		if (link_rate <=  LINK_RATE_2_7Gbps)
			val = phy_eq_hbr0_1[dp->phy_tune_set][amp_lvl][pre_emp_lvl][i];
		else
			val = phy_eq_hbr2_3[dp->phy_tune_set][amp_lvl][pre_emp_lvl][i];
		if (val == -1)
			continue;

		if (i == EQ_MAIN) {
			mask = (0x3F << (8 * phy_lane_num));
			cal_log_debug(0, "DP_CONFIG8 val 0x%08x mask 0x%08x\n", val, mask);
			dp_phy_write_mask(DP_CONFIG8, val, mask);
		} else if (i == EQ_POST) {
			mask = (0x3F << (8 * phy_lane_num));
			cal_log_debug(0, "DP_CONFIG9 val 0x%08x mask 0x%08x\n", val, mask);
			dp_phy_write_mask(DP_CONFIG9, val, mask);
		} else if (i == EQ_PRE) {
			mask = (0x3F << (8 * phy_lane_num));
			cal_log_debug(0, "DP_CONFIG10 val 0x%08x mask 0x%08x\n", val, mask);
			dp_phy_write_mask(DP_CONFIG10, val, mask);
		} else if (i == EQ_VSWING) {
			dp_reg_cr_write(0x22, (u16)0xD0);
		} else if (i == RBOOST) {
			if (link_rate > LINK_RATE_2_7Gbps) {
				cal_log_debug(0, "RBOOST are set\n");
				if (phy_lane_num == 0)
					dp_reg_cr_write(0x1005, (u16)0x70);
				else if (phy_lane_num == 1)
					dp_reg_cr_write(0x1105, (u16)0x70);
				else if (phy_lane_num == 2)
					dp_reg_cr_write(0x1205, (u16)0x70);
				else if (phy_lane_num == 3)
					dp_reg_cr_write(0x1305, (u16)0x70);
				else
					cal_log_err(0, "Unspported lane num\n");
			} else {
				if (phy_lane_num == 0)
					dp_reg_cr_write(0x1005, (u16)0x40);
				else if (phy_lane_num == 1)
					dp_reg_cr_write(0x1105, (u16)0x40);
				else if (phy_lane_num == 2)
					dp_reg_cr_write(0x1205, (u16)0x40);
				else if (phy_lane_num == 3)
					dp_reg_cr_write(0x1305, (u16)0x40);
				else
					cal_log_err(0, "Unspported lane num\n");
			}
		}
	}
	/* For lane control */
	dp_reg_cr_write(0x1002, (u16)0x180);
	dp_reg_cr_write(0x10EB, (u16)0x0);
#ifdef DP_DUMP
	{
		cal_log_info(0, "DP link rate(%d), DP PHY EQ SFR DP_CONFIG8/9/10/17 DUMP\n",
				link_rate);
		dp_print_hex_dump(regs_dp[REGS_PHY][SST1].regs,
				regs_dp[REGS_PHY][SST1].regs + 0x224, 0x4);
		dp_print_hex_dump(regs_dp[REGS_PHY][SST1].regs,
				regs_dp[REGS_PHY][SST1].regs + 0x228, 0x4);
		dp_print_hex_dump(regs_dp[REGS_PHY][SST1].regs,
				regs_dp[REGS_PHY][SST1].regs + 0x22C, 0x4);
		dp_print_hex_dump(regs_dp[REGS_PHY][SST1].regs,
				regs_dp[REGS_PHY][SST1].regs + 0x248, 0x4);
	}
#endif

}

void dp_reg_set_phy_voltage_and_pre_emphasis(struct dp_cal_res *cal_res, u8 *voltage, u8 *pre_emphasis)
{
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	switch (cal_res->pdic_notify_dp_conf) {
	case PDIC_NOTIFY_DP_PIN_UNKNOWN:
		break;

	case PDIC_NOTIFY_DP_PIN_A:
		if (cal_res->dp_sw_sel) {
			dp_reg_set_phy_tune(cal_res, 0, voltage[1], pre_emphasis[1]);
			dp_reg_set_phy_tune(cal_res, 1, voltage[2], pre_emphasis[2]);
			dp_reg_set_phy_tune(cal_res, 2, voltage[3], pre_emphasis[3]);
			dp_reg_set_phy_tune(cal_res, 3, voltage[0], pre_emphasis[0]);
		} else {
			dp_reg_set_phy_tune(cal_res, 0, voltage[0], pre_emphasis[0]);
			dp_reg_set_phy_tune(cal_res, 1, voltage[3], pre_emphasis[3]);
			dp_reg_set_phy_tune(cal_res, 2, voltage[2], pre_emphasis[2]);
			dp_reg_set_phy_tune(cal_res, 3, voltage[1], pre_emphasis[1]);
		}
		break;
	case PDIC_NOTIFY_DP_PIN_B:
		if (cal_res->dp_sw_sel) {
			dp_reg_set_phy_tune(cal_res, 2, voltage[0], pre_emphasis[0]);
			dp_reg_set_phy_tune(cal_res, 3, voltage[1], pre_emphasis[1]);
		} else {
			dp_reg_set_phy_tune(cal_res, 0, voltage[1], pre_emphasis[1]);
			dp_reg_set_phy_tune(cal_res, 1, voltage[0], pre_emphasis[0]);
		}
		break;

	case PDIC_NOTIFY_DP_PIN_C:
	case PDIC_NOTIFY_DP_PIN_E:
		if (cal_res->dp_sw_sel) {
			dp_reg_set_phy_tune(cal_res, 0, voltage[2], pre_emphasis[2]);
			dp_reg_set_phy_tune(cal_res, 1, voltage[3], pre_emphasis[3]);
			dp_reg_set_phy_tune(cal_res, 2, voltage[1], pre_emphasis[1]);
			dp_reg_set_phy_tune(cal_res, 3, voltage[0], pre_emphasis[0]);
		} else {
			dp_reg_set_phy_tune(cal_res, 0, voltage[0], pre_emphasis[0]);
			dp_reg_set_phy_tune(cal_res, 1, voltage[1], pre_emphasis[1]);
			dp_reg_set_phy_tune(cal_res, 2, voltage[3], pre_emphasis[3]);
			dp_reg_set_phy_tune(cal_res, 3, voltage[2], pre_emphasis[2]);
		}
		break;

	case PDIC_NOTIFY_DP_PIN_D:
	case PDIC_NOTIFY_DP_PIN_F:
		if (cal_res->dp_sw_sel) {
			dp_reg_set_phy_tune(cal_res, 2, voltage[1], pre_emphasis[1]);
			dp_reg_set_phy_tune(cal_res, 3, voltage[0], pre_emphasis[0]);
		} else {
			dp_reg_set_phy_tune(cal_res, 0, voltage[0], pre_emphasis[0]);
			dp_reg_set_phy_tune(cal_res, 1, voltage[1], pre_emphasis[1]);
		}
		break;

	default:
		break;
	}
#elif IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	switch (cal_res->ifconn_notify_dp_conf) {
		case IFCONN_NOTIFY_DP_PIN_UNKNOWN:
			break;

		case IFCONN_NOTIFY_DP_PIN_A:
			if (cal_res->dp_sw_sel) {
				dp_reg_set_phy_tune(cal_res, 0, voltage[1], pre_emphasis[1]);
				dp_reg_set_phy_tune(cal_res, 1, voltage[2], pre_emphasis[2]);
				dp_reg_set_phy_tune(cal_res, 2, voltage[3], pre_emphasis[3]);
				dp_reg_set_phy_tune(cal_res, 3, voltage[0], pre_emphasis[0]);
			} else {
				dp_reg_set_phy_tune(cal_res, 0, voltage[0], pre_emphasis[0]);
				dp_reg_set_phy_tune(cal_res, 1, voltage[3], pre_emphasis[3]);
				dp_reg_set_phy_tune(cal_res, 2, voltage[2], pre_emphasis[2]);
				dp_reg_set_phy_tune(cal_res, 3, voltage[1], pre_emphasis[1]);
			}
			break;
		case IFCONN_NOTIFY_DP_PIN_B:
			if (cal_res->dp_sw_sel) {
				dp_reg_set_phy_tune(cal_res, 2, voltage[0], pre_emphasis[0]);
				dp_reg_set_phy_tune(cal_res, 3, voltage[1], pre_emphasis[1]);
			} else {
				dp_reg_set_phy_tune(cal_res, 0, voltage[1], pre_emphasis[1]);
				dp_reg_set_phy_tune(cal_res, 1, voltage[0], pre_emphasis[0]);
			}
			break;
		case IFCONN_NOTIFY_DP_PIN_C:
		case IFCONN_NOTIFY_DP_PIN_E:
			if (cal_res->dp_sw_sel) {
				dp_reg_set_phy_tune(cal_res, 0, voltage[2], pre_emphasis[2]);
				dp_reg_set_phy_tune(cal_res, 1, voltage[3], pre_emphasis[3]);
				dp_reg_set_phy_tune(cal_res, 2, voltage[1], pre_emphasis[1]);
				dp_reg_set_phy_tune(cal_res, 3, voltage[0], pre_emphasis[0]);
			} else {
				dp_reg_set_phy_tune(cal_res, 0, voltage[0], pre_emphasis[0]);
				dp_reg_set_phy_tune(cal_res, 1, voltage[1], pre_emphasis[1]);
				dp_reg_set_phy_tune(cal_res, 2, voltage[3], pre_emphasis[3]);
				dp_reg_set_phy_tune(cal_res, 3, voltage[2], pre_emphasis[2]);
			}
			break;

		case IFCONN_NOTIFY_DP_PIN_D:
		case IFCONN_NOTIFY_DP_PIN_F:
			if (cal_res->dp_sw_sel) {
				dp_reg_set_phy_tune(cal_res, 2, voltage[1], pre_emphasis[1]);
				dp_reg_set_phy_tune(cal_res, 3, voltage[0], pre_emphasis[0]);
			} else {
				dp_reg_set_phy_tune(cal_res, 0, voltage[0], pre_emphasis[0]);
				dp_reg_set_phy_tune(cal_res, 1, voltage[1], pre_emphasis[1]);
			}
			break;

		default:
			break;
		}
#endif
}

void dp_reg_set_voltage_and_pre_emphasis(struct dp_cal_res *cal_res, u8 *voltage, u8 *pre_emphasis)
{
	dp_reg_phy_print_link_bw(cal_res);
	dp_reg_set_phy_voltage_and_pre_emphasis(cal_res, voltage, pre_emphasis);
}

void dp_reg_function_enable(void)
{
	dp_write_mask(SYSTEM_COMMON_FUNCTION_ENABLE, 1, PCS_FUNC_EN);
	dp_write_mask(SYSTEM_COMMON_FUNCTION_ENABLE, 1, AUX_FUNC_EN);
	dp_write_mask(SYSTEM_SST1_FUNCTION_ENABLE, 1, SST1_VIDEO_FUNC_EN);
}

void dp_reg_set_common_interrupt_mask(enum dp_interrupt_mask param, u8 set)
{
	u32 val = set ? ~0 : 0;

	switch (param) {
	case HOTPLUG_CHG_INT_MASK:
		dp_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HPD_CHG_MASK);
		break;
	case HPD_LOST_INT_MASK:
		dp_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HPD_LOST_MASK);
		break;
	case PLUG_INT_MASK:
		dp_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HPD_PLUG_MASK);
		break;
	case HPD_IRQ_INT_MASK:
		dp_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HPD_IRQ_MASK);
		break;
	case RPLY_RECEIV_INT_MASK:
		dp_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, AUX_REPLY_RECEIVED_MASK);
		break;
	case AUX_ERR_INT_MASK:
		dp_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, AUX_ERR_MASK);
		break;
	case HDCP_LINK_CHECK_INT_MASK:
		dp_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HDCP_R0_CHECK_FLAG_MASK);
		break;
	case HDCP_LINK_FAIL_INT_MASK:
		dp_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HDCP_LINK_CHK_FAIL_MASK);
		break;
	case HDCP_R0_READY_INT_MASK:
		dp_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HDCP_R0_CHECK_FLAG_MASK);
		break;
	case PLL_LOCK_CHG_INT_MASK:
		dp_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, PLL_LOCK_CHG_MASK);
		break;
	case ALL_INT_MASK:
		dp_write(SYSTEM_IRQ_COMMON_STATUS_MASK, 0xFF);
		break;
	default:
			break;
	}
}

void dp_reg_set_interrupt_mask(enum dp_interrupt_mask param, u8 set)
{
	u32 val = set ? ~0 : 0;

	switch (param) {
	case VIDEO_FIFO_UNDER_FLOW_MASK:
		dp_write_mask(SST1_INTERRUPT_MASK_SET0,
				val, MAPI_FIFO_UNDER_FLOW_MASK);
		break;
	case VSYNC_DET_INT_MASK:
		dp_write_mask(SST1_INTERRUPT_MASK_SET0,
				val, VSYNC_DET_MASK);
		break;
	case AUDIO_FIFO_UNDER_RUN_INT_MASK:
		dp_write_mask(SST1_AUDIO_BUFFER_CONTROL,
				val, MASTER_AUDIO_BUFFER_EMPTY_INT_EN);
		dp_write_mask(SST1_AUDIO_BUFFER_CONTROL,
				val, MASTER_AUDIO_BUFFER_EMPTY_INT_MASK);
		break;
	case ALL_INT_MASK:
		dp_write(SST1_INTERRUPT_MASK_SET0, 0xFF);
		dp_write(SST1_INTERRUPT_STATUS_SET1, 0xFF);
		break;
	default:
			break;
	}
}

void dp_reg_set_interrupt(u32 en)
{
	u32 val = en ? ~0 : 0;
	int i = 0;

	dp_write(SYSTEM_IRQ_COMMON_STATUS, ~0);
	dp_write(SST1_INTERRUPT_STATUS_SET0 + 0x1000 * i, ~0);
	dp_write(SST1_INTERRUPT_STATUS_SET1 + 0x1000 * i, ~0);
	dp_write_mask(SST1_AUDIO_BUFFER_CONTROL + 0x1000 * i,
				1, MASTER_AUDIO_BUFFER_EMPTY_INT);

#if !IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	dp_reg_set_common_interrupt_mask(HPD_IRQ_INT_MASK, val);
	dp_reg_set_common_interrupt_mask(HOTPLUG_CHG_INT_MASK, val);
	dp_reg_set_common_interrupt_mask(HPD_LOST_INT_MASK, val);
	dp_reg_set_common_interrupt_mask(PLUG_INT_MASK, val);
#endif

	dp_reg_set_interrupt_mask(VSYNC_DET_INT_MASK, val);
	dp_reg_set_interrupt_mask(VIDEO_FIFO_UNDER_FLOW_MASK, val);
	dp_reg_set_interrupt_mask(AUDIO_FIFO_UNDER_RUN_INT_MASK, val);
}

u32 dp_reg_get_common_interrupt_and_clear(void)
{
	u32 val = 0;

	val = dp_read(SYSTEM_IRQ_COMMON_STATUS);

	dp_write(SYSTEM_IRQ_COMMON_STATUS, ~0);

	return val;
}

u32 dp_reg_get_video_interrupt_and_clear(void)
{
	u32 val = 0;

	val = dp_read(SST1_INTERRUPT_STATUS_SET0);

	dp_write(SST1_INTERRUPT_STATUS_SET0, ~0);

	return val;
}

u32 dp_reg_get_audio_interrupt_and_clear(void)
{
	u32 val = 0;

	val = dp_read_mask(SST1_AUDIO_BUFFER_CONTROL,
			MASTER_AUDIO_BUFFER_EMPTY_INT);

	dp_write_mask(SST1_AUDIO_BUFFER_CONTROL,
			1, MASTER_AUDIO_BUFFER_EMPTY_INT);

	return val;
}

void dp_reg_set_daynamic_range(enum dp_dynamic_range_type dynamic_range)
{
	dp_write_mask(SST1_VIDEO_CONTROL,
			dynamic_range, DYNAMIC_RANGE_MODE);
}

void dp_reg_set_video_bist_mode(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(SST1_VIDEO_CONTROL,
			val, STRM_VALID_FORCE | STRM_VALID_CTRL);
	dp_write_mask(SST1_VIDEO_BIST_CONTROL,
			val, BIST_EN);
}

void dp_reg_set_audio_bist_mode(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(SST1_AUDIO_BIST_CONTROL, 0x0F, SIN_AMPL);
	dp_write_mask(SST1_AUDIO_BIST_CONTROL, val, AUD_BIST_EN);
}

#ifdef FEATURE_USE_DRM_EDID_PARSER
void dp_reg_drm_mode_register_setting(void)
{
	struct dp_device *dp = get_dp_drvdata();
	u32 val = 0;
	struct videomode vm;
	char log_tmp[256] = {0x0, };
	int log_len = 0;

	drm_display_mode_to_videomode(&dp->cur_mode, &vm);

	val = dp->cur_mode.vtotal;
	log_len = sprintf(log_tmp, "vtotal: %u", val);
	dp_write(SST1_VIDEO_VERTICAL_TOTAL_PIXELS, val);

	val = dp->cur_mode.htotal;
	log_len += sprintf(log_tmp + log_len, ", htotal: %u", val);
	dp_write(SST1_VIDEO_HORIZONTAL_TOTAL_PIXELS, val);

	val = vm.vactive;
	log_len += sprintf(log_tmp + log_len, ", vactive: %u", val);
	dp_write(SST1_VIDEO_VERTICAL_ACTIVE, val);

	val = vm.vfront_porch;
	log_len += sprintf(log_tmp + log_len, ", vfp: %u", val);
	dp_write(SST1_VIDEO_VERTICAL_FRONT_PORCH, val);

	val = vm.vback_porch;
	log_len += sprintf(log_tmp + log_len, ", vbp: %u", val);
	dp_write(SST1_VIDEO_VERTICAL_BACK_PORCH, val);

	val = vm.hactive;
	log_len += sprintf(log_tmp + log_len, ", hactive: %u", val);
	dp_write(SST1_VIDEO_HORIZONTAL_ACTIVE, val);

	val = vm.hfront_porch;
	log_len += sprintf(log_tmp + log_len, ", hfp: %u", val);
	dp_write(SST1_VIDEO_HORIZONTAL_FRONT_PORCH, val);

	val = vm.hback_porch;
	log_len += sprintf(log_tmp + log_len, ", hbp: %u", val);
	dp_write(SST1_VIDEO_HORIZONTAL_BACK_PORCH, val);

	val = (dp->cur_mode.flags & DRM_MODE_FLAG_NVSYNC) ? SYNC_NEGATIVE : SYNC_POSITIVE;
	log_len += sprintf(log_tmp + log_len, ", vsyncp: %c", val ? 'N' : 'P');
	dp_write_mask(SST1_VIDEO_CONTROL, val, VSYNC_POLARITY);

	val = (dp->cur_mode.flags & DRM_MODE_FLAG_NHSYNC) ? SYNC_NEGATIVE : SYNC_POSITIVE;
	log_len += sprintf(log_tmp + log_len, ", hsyncp: %c", val ? 'N' : 'P');
	dp_write_mask(SST1_VIDEO_CONTROL, val, HSYNC_POLARITY);

	cal_log_warn(0, "%s\n", log_tmp);
}
#endif

void dp_reg_video_format_register_setting(videoformat video_format)
{
	u32 val = 0;
	char log_tmp[256] = {0x0, };
	int log_len = 0;

	val += supported_videos[video_format].dv_timings.bt.height;
	val += supported_videos[video_format].dv_timings.bt.vfrontporch;
	val += supported_videos[video_format].dv_timings.bt.vsync;
	val += supported_videos[video_format].dv_timings.bt.vbackporch;
	log_len = sprintf(log_tmp, "vtotal: %u", val);
	dp_write(SST1_VIDEO_VERTICAL_TOTAL_PIXELS, val);

	val = 0;
	val += supported_videos[video_format].dv_timings.bt.width;
	val += supported_videos[video_format].dv_timings.bt.hfrontporch;
	val += supported_videos[video_format].dv_timings.bt.hsync;
	val += supported_videos[video_format].dv_timings.bt.hbackporch;
	log_len += sprintf(log_tmp + log_len, ", htotal: %u", val);
	dp_write(SST1_VIDEO_HORIZONTAL_TOTAL_PIXELS, val);

	val = supported_videos[video_format].dv_timings.bt.height;
	log_len += sprintf(log_tmp + log_len, ", vactive: %u", val);
	dp_write(SST1_VIDEO_VERTICAL_ACTIVE, val);

	val = supported_videos[video_format].dv_timings.bt.vfrontporch;
	log_len += sprintf(log_tmp + log_len, ", vfp: %u", val);
	dp_write(SST1_VIDEO_VERTICAL_FRONT_PORCH, val);

	val = supported_videos[video_format].dv_timings.bt.vbackporch;
	log_len += sprintf(log_tmp + log_len, ", vbp: %u", val);
	dp_write(SST1_VIDEO_VERTICAL_BACK_PORCH, val);

	val = supported_videos[video_format].dv_timings.bt.width;
	log_len += sprintf(log_tmp + log_len, ", hactive: %u", val);
	dp_write(SST1_VIDEO_HORIZONTAL_ACTIVE, val);

	val = supported_videos[video_format].dv_timings.bt.hfrontporch;
	log_len += sprintf(log_tmp + log_len, ", hfp: %u", val);
	dp_write(SST1_VIDEO_HORIZONTAL_FRONT_PORCH, val);

	val = supported_videos[video_format].dv_timings.bt.hbackporch;
	log_len += sprintf(log_tmp + log_len, ", hbp: %u", val);
	dp_write(SST1_VIDEO_HORIZONTAL_BACK_PORCH, val);

	val = supported_videos[video_format].v_sync_pol;
	log_len += sprintf(log_tmp + log_len, ", vsyncp: %c", val ? 'N' : 'P');
	dp_write_mask(SST1_VIDEO_CONTROL, val, VSYNC_POLARITY);

	val = supported_videos[video_format].h_sync_pol;
	log_len += sprintf(log_tmp + log_len, ", hsyncp: %c", val ? 'N' : 'P');
	dp_write_mask(SST1_VIDEO_CONTROL, val, HSYNC_POLARITY);

	cal_log_warn(0, "%s\n", log_tmp);
}

u32 dp_reg_get_video_clk(void)
{
#ifdef FEATURE_USE_DRM_EDID_PARSER
	struct dp_device *dp = get_dp_drvdata();

	return dp->cur_mode.clock * 1000;
#else
	struct dp_device *dp = get_dp_drvdata();

	return supported_videos[dp->cur_video].dv_timings.bt.pixelclock;
#endif
}

u32 dp_reg_get_ls_clk(struct dp_cal_res *cal_res)
{
	u32 val;
	u32 ls_clk;

	val = dp_reg_phy_get_link_bw(cal_res);
	dp_reg_phy_print_link_bw(cal_res);

	if (val == LINK_RATE_8_1Gbps)
		ls_clk = 810000000;
	else if (val == LINK_RATE_5_4Gbps)
		ls_clk = 540000000;
	else if (val == LINK_RATE_2_7Gbps)
		ls_clk = 270000000;
	else /* LINK_RATE_1_62Gbps */
		ls_clk = 162000000;

	return ls_clk;
}

void dp_reg_set_video_clock(struct dp_cal_res *cal_res)
{
	u32 stream_clk = 0;
	u32 ls_clk = 0;
	u32 mvid_master = 0;
	u32 nvid_master = 0;

	stream_clk = dp_reg_get_video_clk() / 1000;
	ls_clk = dp_reg_get_ls_clk(cal_res) / 1000;

	mvid_master = stream_clk >> 2;
	nvid_master = ls_clk;

	dp_write(SST1_MVID_MASTER_MODE, mvid_master);
	dp_write(SST1_NVID_MASTER_MODE, nvid_master);

	//dp_write_mask(SST1_MAIN_CONTROL, 1, MVID_MODE);

	dp_write(SST1_MVID_SFR_CONFIGURE, stream_clk);
	dp_write(SST1_NVID_SFR_CONFIGURE, ls_clk);
}

void dp_reg_set_active_symbol(struct dp_cal_res *cal_res)
{
	u64 TU_off = 0;	/* TU Size when FEC is off*/
	u64 TU_on = 0;	/* TU Size when FEC is on*/
	u32 bpp = 0;	/* Bit Per Pixel */
	u32 lanecount = 0;
	u32 bandwidth = 0;
	u32 integer_fec_off = 0;
	u32 fraction_fec_off = 0;
	u32 threshold_fec_off = 0;
	u32 integer_fec_on = 0;
	u32 fraction_fec_on = 0;
	u32 threshold_fec_on = 0;
	u32 clk = 0;

#if 0
	dp_write_mask(SST1_ACTIVE_SYMBOL_MODE_CONTROL,
			1, ACTIVE_SYMBOL_MODE_CONTROL);
#endif
	dp_write_mask(SST1_ACTIVE_SYMBOL_THRESHOLD_SEL_FEC_OFF,
			1, ACTIVE_SYMBOL_THRESHOLD_SEL_FEC_OFF);
	dp_write_mask(SST1_ACTIVE_SYMBOL_THRESHOLD_SEL_FEC_ON,
			1, ACTIVE_SYMBOL_THRESHOLD_SEL_FEC_ON);

	switch (cal_res->bpc) {
	case BPC_8:
		bpp = 24;
		break;
	case BPC_10:
		bpp = 30;
		break;
	default:
		bpp = 18;
		break;
	} /* if DSC on, bpp / 3 */

	/* change to Mbps from bps of pixel clock*/
	clk = dp_reg_get_video_clk() / 1000;

	bandwidth = dp_reg_get_ls_clk(cal_res) / 1000;
	lanecount = dp_reg_get_lane_count();

	cal_log_info(0, "bpp:%u, clk: %u, bandwidth: %u, lanecount: %u\n",
		bpp, clk, bandwidth, lanecount);

	TU_off = ((clk * bpp * 32) * 10000000000) / (lanecount * bandwidth * 8);
	TU_on = (TU_off * 1000) / 976;

	integer_fec_off = (u32)(TU_off / 10000000000);
	fraction_fec_off = (u32)((TU_off - (integer_fec_off * 10000000000)) / 10);
	integer_fec_on = (u32)(TU_on / 10000000000);
	fraction_fec_on = (u32)((TU_on - (integer_fec_on * 10000000000)) / 10);

	if (integer_fec_off <= 2)
		threshold_fec_off = 7;
	else if (integer_fec_off > 2 && integer_fec_off <= 5)
		threshold_fec_off = 8;
	else if (integer_fec_off > 5)
		threshold_fec_off = 9;

	if (integer_fec_on <= 2)
		threshold_fec_on = 7;
	else if (integer_fec_on > 2 && integer_fec_on <= 5)
		threshold_fec_on = 8;
	else if (integer_fec_on > 5)
		threshold_fec_on = 9;

	cal_log_info(0, "fec_off(int: %d, frac: %d, thr: %d), fec_on(int: %d, frac: %d, thr: %d)\n",
			integer_fec_off, fraction_fec_off, threshold_fec_off,
			integer_fec_on, fraction_fec_on, threshold_fec_on);

	dp_write_mask(SST1_ACTIVE_SYMBOL_INTEGER_FEC_OFF,
			integer_fec_off, ACTIVE_SYMBOL_INTEGER_FEC_OFF);
	dp_write_mask(SST1_ACTIVE_SYMBOL_FRACTION_FEC_OFF,
			fraction_fec_off, ACTIVE_SYMBOL_FRACTION_FEC_OFF);
	dp_write_mask(SST1_ACTIVE_SYMBOL_THRESHOLD_FEC_OFF,
			threshold_fec_off, ACTIVE_SYMBOL_FRACTION_FEC_OFF);

	dp_write_mask(SST1_ACTIVE_SYMBOL_INTEGER_FEC_ON,
			integer_fec_on, ACTIVE_SYMBOL_INTEGER_FEC_ON);
	dp_write_mask(SST1_ACTIVE_SYMBOL_FRACTION_FEC_ON,
			fraction_fec_on, ACTIVE_SYMBOL_FRACTION_FEC_OFF);
	dp_write_mask(SST1_ACTIVE_SYMBOL_THRESHOLD_FEC_ON,
			threshold_fec_on, ACTIVE_SYMBOL_THRESHOLD_FEC_ON);
}

void dp_reg_enable_interface_crc(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(SST1_STREAM_IF_CRC_CONTROL_1, val, IF_CRC_EN);
	dp_write_mask(SST1_STREAM_IF_CRC_CONTROL_1, val, IF_CRC_SW_COMPARE);

	if (val == 0) {
		dp_write_mask(SST1_STREAM_IF_CRC_CONTROL_1, 1, IF_CRC_CLEAR);
		dp_write_mask(SST1_STREAM_IF_CRC_CONTROL_1, 0, IF_CRC_CLEAR);
	}
}

void dp_reg_get_interface_crc(u32 *crc_r_result, u32 *crc_g_result, u32 *crc_b_result)
{
	*crc_r_result = dp_read_mask(SST1_STREAM_IF_CRC_CONTROL_2, IF_CRC_R_RESULT);
	*crc_g_result = dp_read_mask(SST1_STREAM_IF_CRC_CONTROL_3, IF_CRC_G_RESULT);
	*crc_b_result = dp_read_mask(SST1_STREAM_IF_CRC_CONTROL_4, IF_CRC_B_RESULT);
}

void dp_reg_enable_stand_alone_crc(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(PCS_SA_CRC_CONTROL_1, val,
		SA_CRC_LANE_0_ENABLE | SA_CRC_LANE_1_ENABLE |
		SA_CRC_LANE_2_ENABLE | SA_CRC_LANE_3_ENABLE);
	dp_write_mask(PCS_SA_CRC_CONTROL_1, val, SA_CRC_SW_COMPARE);

	if (val == 0) {
		dp_write_mask(PCS_SA_CRC_CONTROL_1, 1, SA_CRC_CLEAR);
		dp_write_mask(PCS_SA_CRC_CONTROL_1, 0, SA_CRC_CLEAR);
	}
}

void dp_reg_get_stand_alone_crc(u32 *ln0, u32 *ln1, u32 *ln2, u32 *ln3)
{
	*ln0 = dp_read_mask(PCS_SA_CRC_CONTROL_2, SA_CRC_LN0_RESULT);
	*ln1 = dp_read_mask(PCS_SA_CRC_CONTROL_3, SA_CRC_LN1_RESULT);
	*ln2 = dp_read_mask(PCS_SA_CRC_CONTROL_4, SA_CRC_LN2_RESULT);
	*ln3 = dp_read_mask(PCS_SA_CRC_CONTROL_5, SA_CRC_LN3_RESULT);
}

void dp_reg_aux_ch_buf_clr(void)
{
	dp_write_mask(AUX_BUFFER_CLEAR, 1, AUX_BUF_CLR);
}

void dp_reg_aux_defer_ctrl(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(AUX_COMMAND_CONTROL, val, DEFER_CTRL_EN);
}

void dp_reg_set_aux_reply_timeout(void)
{
	dp_write_mask(AUX_CONTROL, AUX_TIMEOUT_1800us, AUX_REPLY_TIMER_MODE);
}

void dp_reg_set_aux_ch_command(enum dp_aux_ch_command_type aux_ch_mode)
{
	dp_write_mask(AUX_REQUEST_CONTROL, aux_ch_mode, REQ_COMM);
}

void dp_reg_set_aux_ch_address(u32 aux_ch_address)
{
	dp_write_mask(AUX_REQUEST_CONTROL, aux_ch_address, REQ_ADDR);
}

void dp_reg_set_aux_ch_length(u32 aux_ch_length)
{
	dp_write_mask(AUX_REQUEST_CONTROL, aux_ch_length - 1, REQ_LENGTH);
}

void dp_reg_aux_ch_send_buf(u8 *aux_ch_send_buf, u32 aux_ch_length)
{
	int i;

	for (i = 0; i < aux_ch_length; i++) {
		dp_write_mask(AUX_TX_DATA_SET0 + ((i / 4) * 4),
			aux_ch_send_buf[i], (0x000000FF << ((i % 4) * 8)));
	}
}

void dp_reg_aux_ch_received_buf(u8 *aux_ch_received_buf, u32 aux_ch_length)
{
	int i;

	for (i = 0; i < aux_ch_length; i++) {
		aux_ch_received_buf[i] =
			(dp_read_mask(AUX_RX_DATA_SET0 + ((i / 4) * 4),
			0xFF << ((i % 4) * 8)) >> (i % 4) * 8);
	}
}

int dp_reg_set_aux_ch_operation_enable(void)
{
	u32 cnt = 5000;
	u32 state;
	u32 val0, val1;

	dp_write_mask(AUX_TRANSACTION_START, 1, AUX_TRAN_START);

	do {
		state = dp_read(AUX_TRANSACTION_START) & AUX_TRAN_START;
		cnt--;
		usleep_range(10, 11);
	} while (state && cnt);

	if (!cnt) {
		cal_log_err(0, "AUX_TRAN_START waiting timeout.\n");
		return -ETIME;
	}

	val0 = dp_read(AUX_MONITOR_1);
	val1 = dp_read(AUX_MONITOR_2);

	if ((val0 & AUX_CMD_STATUS) != 0x00 || val1 != 0x00) {
		cal_log_debug(0, "AUX_MONITOR_1 : 0x%X, AUX_MONITOR_2 : 0x%X\n", val0, val1);
		cal_log_debug(0, "AUX_CONTROL : 0x%X, AUX_REQUEST_CONTROL : 0x%X, AUX_COMMAND_CONTROL : 0x%X\n",
				dp_read(AUX_CONTROL),
				dp_read(AUX_REQUEST_CONTROL),
				dp_read(AUX_COMMAND_CONTROL));

		usleep_range(400, 410);
		return -EIO;
	}

	return 0;
}

void dp_reg_set_aux_ch_address_only_command(u32 en)
{
	dp_write_mask(AUX_ADDR_ONLY_COMMAND, en, ADDR_ONLY_CMD);
}

int dp_reg_dpcd_write(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data)
{
	int ret;
	int retry_cnt = AUX_RETRY_COUNT;
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	struct dp_device *dp = get_dp_drvdata();
#endif
#endif

	mutex_lock(&cal_res->aux_lock);
	while(retry_cnt > 0) {
		dp_reg_aux_ch_buf_clr();
		dp_reg_aux_defer_ctrl(1);
		dp_reg_set_aux_reply_timeout();
		dp_reg_set_aux_ch_command(DPCD_WRITE);
		dp_reg_set_aux_ch_address(address);
		dp_reg_set_aux_ch_length(length);
		dp_reg_aux_ch_send_buf(data, length);
		ret = dp_reg_set_aux_ch_operation_enable();
		if (ret == 0)
			break;

		retry_cnt--;
	}

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	if (ret == 0)
		secdp_bigdata_clr_error_cnt(ERR_AUX);
	else if (dp->pdic_hpd)
		secdp_bigdata_inc_error_cnt(ERR_AUX);
#endif
#endif

	mutex_unlock(&cal_res->aux_lock);

	return ret;
}

int dp_reg_dpcd_read(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data)
{
	int ret;
	int retry_cnt = AUX_RETRY_COUNT;
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	struct dp_device *dp = get_dp_drvdata();
#endif
#endif

	mutex_lock(&cal_res->aux_lock);
	while(retry_cnt > 0) {
		dp_reg_set_aux_ch_command(DPCD_READ);
		dp_reg_set_aux_ch_address(address);
		dp_reg_set_aux_ch_length(length);
		dp_reg_aux_ch_buf_clr();
		dp_reg_aux_defer_ctrl(1);
		dp_reg_set_aux_reply_timeout();
		ret = dp_reg_set_aux_ch_operation_enable();

		if (ret == 0)
			break;
		retry_cnt--;
	}

	if (ret == 0)
		dp_reg_aux_ch_received_buf(data, length);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	if (ret == 0)
		secdp_bigdata_clr_error_cnt(ERR_AUX);
	else if (dp->pdic_hpd)
		secdp_bigdata_inc_error_cnt(ERR_AUX);
#endif
#endif

	mutex_unlock(&cal_res->aux_lock);

	return ret;
}

int dp_reg_dpcd_write_burst(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data)
{
	int ret = 0;
	u32 i, buf_length, length_calculation;

	length_calculation = length;
	for (i = 0; i < length; i += AUX_DATA_BUF_COUNT) {
		if (length_calculation >= AUX_DATA_BUF_COUNT) {
			buf_length = AUX_DATA_BUF_COUNT;
			length_calculation -= AUX_DATA_BUF_COUNT;
		} else {
			buf_length = length % AUX_DATA_BUF_COUNT;
			length_calculation = 0;
		}

		ret = dp_reg_dpcd_write(cal_res, address + i, buf_length, data + i);
		if (ret != 0) {
			cal_log_err(0, "dp_reg_dpcd_write_burst fail\n");
			break;
		}
	}

	return ret;
}

int dp_reg_dpcd_read_burst(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data)
{
	int ret = 0;
	u32 i, buf_length, length_calculation;

	length_calculation = length;

	for (i = 0; i < length; i += AUX_DATA_BUF_COUNT) {
		if (length_calculation >= AUX_DATA_BUF_COUNT) {
			buf_length = AUX_DATA_BUF_COUNT;
			length_calculation -= AUX_DATA_BUF_COUNT;
		} else {
			buf_length = length % AUX_DATA_BUF_COUNT;
			length_calculation = 0;
		}

		ret = dp_reg_dpcd_read(cal_res, address + i, buf_length, data + i);

		if (ret != 0) {
			cal_log_err(0, "dp_reg_dpcd_read_burst fail\n");
			break;
		}
	}

	return ret;
}

int dp_reg_i2c_write(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data)
{
	int ret;
	int retry_cnt = AUX_RETRY_COUNT;

	mutex_lock(&cal_res->aux_lock);

	while (retry_cnt > 0) {
		dp_reg_set_aux_ch_command(I2C_WRITE);
		dp_reg_set_aux_ch_address(address);
		dp_reg_set_aux_ch_address_only_command(1);
		ret = dp_reg_set_aux_ch_operation_enable();
		dp_reg_set_aux_ch_address_only_command(0);

		dp_reg_aux_ch_buf_clr();
		dp_reg_aux_defer_ctrl(1);
		dp_reg_set_aux_reply_timeout();
		dp_reg_set_aux_ch_address_only_command(0);
		dp_reg_set_aux_ch_command(I2C_WRITE);
		dp_reg_set_aux_ch_address(address);
		dp_reg_set_aux_ch_length(length);
		dp_reg_aux_ch_send_buf(data, length);
		ret = dp_reg_set_aux_ch_operation_enable();

		if (ret == 0) {
			dp_reg_set_aux_ch_command(I2C_WRITE);
			dp_reg_set_aux_ch_address(EDID_ADDRESS);
			dp_reg_set_aux_ch_address_only_command(1);
			ret = dp_reg_set_aux_ch_operation_enable();
			dp_reg_set_aux_ch_address_only_command(0);
			cal_log_debug(0, "address only request in i2c write\n");
		}

		if (ret == 0)
			break;

		retry_cnt--;
	}

	mutex_unlock(&cal_res->aux_lock);

	return ret;
}

int dp_reg_i2c_read(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data)
{
	int ret;
	int retry_cnt = AUX_RETRY_COUNT;

	mutex_lock(&cal_res->aux_lock);
	while (retry_cnt > 0) {
		dp_reg_set_aux_ch_command(I2C_READ);
		dp_reg_set_aux_ch_address(address);
		dp_reg_set_aux_ch_length(length);
		dp_reg_aux_ch_buf_clr();
		dp_reg_aux_defer_ctrl(1);
		dp_reg_set_aux_reply_timeout();
		ret = dp_reg_set_aux_ch_operation_enable();

		if (ret == 0)
			break;
		retry_cnt--;
	}

	if (ret == 0)
		dp_reg_aux_ch_received_buf(data, length);

	mutex_unlock(&cal_res->aux_lock);

	return ret;
}

int dp_reg_edid_write(u8 edid_addr_offset, u32 length, u8 *data)
{
	u32 i, buf_length, length_calculation;
	int ret;
	int retry_cnt = AUX_RETRY_COUNT;

	while(retry_cnt > 0) {
		dp_reg_aux_ch_buf_clr();
		dp_reg_aux_defer_ctrl(1);
		dp_reg_set_aux_reply_timeout();
		dp_reg_set_aux_ch_command(I2C_WRITE);
		dp_reg_set_aux_ch_address(EDID_ADDRESS);
		dp_reg_set_aux_ch_length(1);
		dp_reg_aux_ch_send_buf(&edid_addr_offset, 1);
		ret = dp_reg_set_aux_ch_operation_enable();

		if (ret == 0) {
			length_calculation = length;


			/*	dp_write_mask(AUX_Ch_MISC_Ctrl_1, 0x3, 3 << 6); */
			for (i = 0; i < length; i += AUX_DATA_BUF_COUNT) {
				if (length_calculation >= AUX_DATA_BUF_COUNT) {
					buf_length = AUX_DATA_BUF_COUNT;
					length_calculation -= AUX_DATA_BUF_COUNT;
				} else {
					buf_length = length%AUX_DATA_BUF_COUNT;
					length_calculation = 0;
				}

				dp_reg_set_aux_ch_length(buf_length);
				dp_reg_aux_ch_send_buf(data+((i/AUX_DATA_BUF_COUNT)*AUX_DATA_BUF_COUNT), buf_length);
				ret = dp_reg_set_aux_ch_operation_enable();

				if (ret == 0)
					break;
			}
		}

		if (ret == 0) {
			dp_reg_set_aux_ch_address_only_command(1);
			ret = dp_reg_set_aux_ch_operation_enable();
			dp_reg_set_aux_ch_address_only_command(0);
		}
		if (ret == 0)
			break;

		retry_cnt--;
	}


	return ret;
}

#define DDC_SEGMENT_ADDR 0x30
int dp_reg_edid_read(struct dp_cal_res *cal_res, u8 block_cnt, u32 length, u8 *data)
{
	u32 i, buf_length, length_calculation;
	int ret;
	int retry_cnt = AUX_RETRY_COUNT;
	u8 offset = (block_cnt & 1) * EDID_BLOCK_SIZE;
	u8 segment = 0;

	mutex_lock(&cal_res->aux_lock);

	while (retry_cnt > 0) {
		dp_reg_aux_ch_buf_clr();
		dp_reg_aux_defer_ctrl(1);
		dp_reg_set_aux_reply_timeout();
		dp_reg_set_aux_ch_address_only_command(0);

		/* for 3,4 block */
		if (block_cnt > 1)
			segment = 1;

		/* write segment offset */
		cal_log_warn(0, "read block%d\n", block_cnt);
		dp_reg_set_aux_ch_command(I2C_WRITE);
		dp_reg_set_aux_ch_address(DDC_SEGMENT_ADDR);
		dp_reg_set_aux_ch_length(1);
		dp_reg_aux_ch_send_buf(&segment, 1);
		ret = dp_reg_set_aux_ch_operation_enable();
		if (ret)
			cal_log_err(0, "sending segment failed\n");

		dp_reg_set_aux_ch_command(I2C_WRITE);
		dp_reg_set_aux_ch_address(EDID_ADDRESS);
		dp_reg_set_aux_ch_length(1);
		dp_reg_aux_ch_send_buf(&offset, 1);
		ret = dp_reg_set_aux_ch_operation_enable();

		cal_log_debug(0, "EDID address command in EDID read\n");

		if (ret == 0) {
			dp_reg_set_aux_ch_command(I2C_READ);
			length_calculation = length;

			for (i = 0; i < length; i += AUX_DATA_BUF_COUNT) {
				if (length_calculation >= AUX_DATA_BUF_COUNT) {
					buf_length = AUX_DATA_BUF_COUNT;
					length_calculation -= AUX_DATA_BUF_COUNT;
				} else {
					buf_length = length%AUX_DATA_BUF_COUNT;
					length_calculation = 0;
				}

				dp_reg_set_aux_ch_length(buf_length);
				dp_reg_aux_ch_buf_clr();
				ret = dp_reg_set_aux_ch_operation_enable();

				if (ret == 0) {
					dp_reg_aux_ch_received_buf(data+((i/AUX_DATA_BUF_COUNT)*AUX_DATA_BUF_COUNT), buf_length);
					cal_log_debug(0, "AUX buffer read count = %d in EDID read\n", i);
				} else {
					cal_log_debug(0, "AUX buffer read fail in EDID read\n");
					break;
				}
			}
		}

		if (ret == 0) {
			dp_reg_set_aux_ch_address_only_command(1);
			ret = dp_reg_set_aux_ch_operation_enable();
			dp_reg_set_aux_ch_address_only_command(0);

			cal_log_debug(0, "2nd address only request in EDID read\n");
		}

		if (ret == 0)
			break;

		retry_cnt--;
	}

	mutex_unlock(&cal_res->aux_lock);

	return ret;
}

void dp_reg_set_lane_map(u32 lane0, u32 lane1, u32 lane2, u32 lane3)
{
	cal_log_debug(0, "lane mapping info\n");
	cal_log_debug(0, "ln0(%d), ln1(%d), ln2(%d), ln3(%d)\n",
		lane0, lane1, lane2, lane3);
	dp_write_mask(PCS_LANE_CONTROL, lane0, LANE0_MAP);
	dp_write_mask(PCS_LANE_CONTROL, lane1, LANE1_MAP);
	dp_write_mask(PCS_LANE_CONTROL, lane2, LANE2_MAP);
	dp_write_mask(PCS_LANE_CONTROL, lane3, LANE3_MAP);
}

/* fix abnormal lane2 signal */
void dp_reg_usb_tune_reset(int dir)
{
	if (dir) {
		dp_reg_cr_write_mask(0x1003, (0x0 << 13) | (0x34 << 7), (0x1 << 13) | (0x3F << 7));
		dp_reg_cr_write_mask(0x1002, (0x0 << 10) | (0xa << 4), (0x1 << 10) | (0x3F << 4));
	} else {
		dp_reg_cr_write_mask(0x1303, (0x0 << 13) | (0x34 << 7), (0x1 << 13) | (0x3F << 7));
		dp_reg_cr_write_mask(0x1302, (0x0 << 10) | (0xa << 4), (0x1 << 10) | (0x3F << 4));
	}
}

void dp_reg_set_lane_map_config(struct dp_cal_res *cal_res)
{
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	switch (cal_res->pdic_notify_dp_conf) {
	case PDIC_NOTIFY_DP_PIN_UNKNOWN:
		dp_reg_set_lane_map(0, 1, 2, 3);
		break;

	case PDIC_NOTIFY_DP_PIN_A:
		dp_reg_usb_tune_reset(cal_res->dp_sw_sel);
		if (cal_res->dp_sw_sel)
			dp_reg_set_lane_map(0, 1, 2, 3);
		else
			dp_reg_set_lane_map(0, 1, 2, 3);
		break;

	case PDIC_NOTIFY_DP_PIN_B:
		if (cal_res->dp_sw_sel)
			dp_reg_set_lane_map(0, 1, 2, 3);
		else
			dp_reg_set_lane_map(0, 1, 2, 3);
		break;

	case PDIC_NOTIFY_DP_PIN_C:
	case PDIC_NOTIFY_DP_PIN_E:
		dp_reg_usb_tune_reset(cal_res->dp_sw_sel);
	case PDIC_NOTIFY_DP_PIN_D:
	case PDIC_NOTIFY_DP_PIN_F:
		if (cal_res->dp_sw_sel)
			dp_reg_set_lane_map(0, 1, 2, 3);
		else
			dp_reg_set_lane_map(0, 1, 2, 3);
		break;

	default:
		dp_reg_set_lane_map(0, 1, 2, 3);
		break;
	}
#elif IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	switch (cal_res->ifconn_notify_dp_conf) {
	case IFCONN_NOTIFY_DP_PIN_UNKNOWN:
		dp_reg_set_lane_map(0, 1, 2, 3);
		break;

	case IFCONN_NOTIFY_DP_PIN_A:
		if (cal_res->dp_sw_sel)
			dp_reg_set_lane_map(0, 1, 2, 3);
		else
			dp_reg_set_lane_map(0, 1, 2, 3);
		break;

	case IFCONN_NOTIFY_DP_PIN_B:
		if (cal_res->dp_sw_sel)
			dp_reg_set_lane_map(0, 1, 2, 3);
		else
			dp_reg_set_lane_map(0, 1, 2, 3);
		break;

	case IFCONN_NOTIFY_DP_PIN_C:
	case IFCONN_NOTIFY_DP_PIN_E:
	case IFCONN_NOTIFY_DP_PIN_D:
	case IFCONN_NOTIFY_DP_PIN_F:
		if (cal_res->dp_sw_sel)
			dp_reg_set_lane_map(0, 1, 2, 3);
		else
			dp_reg_set_lane_map(0, 1, 2, 3);
		break;
	default:
		dp_reg_set_lane_map(0, 1, 2, 3);
		break;
	}
#endif
}

void dp_reg_lh_p_ch_power(u32 en)
{
	u32 cnt = 20 * 1000;	/* wait 20ms */
	u32 state;

	if (en) {
		dp_write_mask(SYSTEM_SST1_FUNCTION_ENABLE, 1, SST1_LH_PWR_ON);
		do {
			state = dp_read_mask(SYSTEM_SST1_FUNCTION_ENABLE, SST1_LH_PWR_ON_STATUS);
			cnt--;
			udelay(1);
		} while (!state && cnt);

		if (!cnt)
			cal_log_err(0, "%s on is timeout[%d].\n", __func__, state);
	} else {
		dp_write_mask(SYSTEM_SST1_FUNCTION_ENABLE, 0, SST1_LH_PWR_ON);
		do {
			state = dp_read_mask(SYSTEM_SST1_FUNCTION_ENABLE, SST1_LH_PWR_ON_STATUS);
			cnt--;
			udelay(1);
		} while (state && cnt);

		if (!cnt) {
			cal_log_err(0, "SYSTEM_CLK_CONTROL[0x%08x]\n",
					dp_read(SYSTEM_CLK_CONTROL));
			cal_log_err(0, "SYSTEM_PLL_LOCK_CONTROL[0x%08x]\n",
					dp_read(SYSTEM_PLL_LOCK_CONTROL));
			cal_log_err(0, "SYSTEM_DEBUG[0x%08x]\n",
					dp_read(SYSTEM_DEBUG));
			cal_log_err(0, "SYSTEM_DEBUG_LH_PCH[0x%08x]\n",
					dp_read(SYSTEM_DEBUG_LH_PCH));
			cal_log_err(0, "VIDEO_CONTROL[0x%08x]\n",
					dp_read(SST1_VIDEO_CONTROL));
			cal_log_err(0, "VIDEO_DEBUG_FSM_STATE[0x%08x]\n",
					dp_read(SST1_VIDEO_DEBUG_FSM_STATE));
			cal_log_err(0, "VIDEO_DEBUG_MAPI[0x%08x]\n",
					dp_read(SST1_VIDEO_DEBUG_MAPI));
			cal_log_err(0, "SYSTEM_SW_FUNCTION_ENABLE[0x%08x]\n",
					dp_read(SYSTEM_SW_FUNCTION_ENABLE));
			cal_log_err(0, "SYSTEM_COMMON_FUNCTION_ENABLE[0x%08x]\n",
					dp_read(SYSTEM_COMMON_FUNCTION_ENABLE));
			cal_log_err(0, "SYSTEM_SST1_FUNCTION_ENABLE[0x%08x]\n",
					dp_read(SYSTEM_SST1_FUNCTION_ENABLE));
		}
	}
}

void dp_reg_sw_function_en(u32 en)
{
	if (en)
		dp_write_mask(SYSTEM_SW_FUNCTION_ENABLE, 1, SW_FUNC_EN);
	else
		dp_write_mask(SYSTEM_SW_FUNCTION_ENABLE, 0, SW_FUNC_EN);
}

void dp_reg_phy_post_init(struct dp_cal_res *cal_res)
{
	u32 req_status_val = 0, req_status_mask = 0;
	u32 pstate_val = 0, pstate_mask = 0;
	u32 datapath_control_val[3] = {0, 0, 0};

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	switch (cal_res->pdic_notify_dp_conf) {
	case PDIC_NOTIFY_DP_PIN_UNKNOWN:
		break;
	case PDIC_NOTIFY_DP_PIN_A:
		break;
	case PDIC_NOTIFY_DP_PIN_B:
		break;
	case PDIC_NOTIFY_DP_PIN_C:
	case PDIC_NOTIFY_DP_PIN_E:
		req_status_val = req_status_mask =
			DP_TX0_REQ_STATUS | DP_TX1_REQ_STATUS |
			DP_TX2_REQ_STATUS | DP_TX3_REQ_STATUS;
		pstate_mask =
			DP_TX0_PSTATE | DP_TX1_PSTATE |
			DP_TX2_PSTATE | DP_TX3_PSTATE;
		datapath_control_val[0] = 0x0000000f;
		datapath_control_val[1] = 0x00000f0f;
		datapath_control_val[2] = 0x000f0f0f;
		break;
	case PDIC_NOTIFY_DP_PIN_D:
	case PDIC_NOTIFY_DP_PIN_F:
		req_status_val = req_status_mask =
			DP_TX0_REQ_STATUS | DP_TX1_REQ_STATUS;
		pstate_mask = pstate_val =
			DP_TX2_PSTATE | DP_TX3_PSTATE;
		datapath_control_val[0] = 0x00000003;
		datapath_control_val[1] = 0x00000303;
		datapath_control_val[2] = 0x00030303;
		break;
	default:
		break;
	}
#elif IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	switch (cal_res->ifconn_notify_dp_conf) {
	case IFCONN_NOTIFY_DP_PIN_UNKNOWN:
		break;
	case IFCONN_NOTIFY_DP_PIN_A:
		break;
	case IFCONN_NOTIFY_DP_PIN_B:
		break;
	case IFCONN_NOTIFY_DP_PIN_C:
	case IFCONN_NOTIFY_DP_PIN_E:
		req_status_val = req_status_mask =
			DP_TX0_REQ_STATUS | DP_TX1_REQ_STATUS |
			DP_TX2_REQ_STATUS | DP_TX3_REQ_STATUS;
		pstate_mask =
			DP_TX0_PSTATE | DP_TX1_PSTATE |
			DP_TX2_PSTATE | DP_TX3_PSTATE;
		datapath_control_val[0] = 0x0000000f;
		datapath_control_val[1] = 0x00000f0f;
		datapath_control_val[2] = 0x000f0f0f;
		break;
	case IFCONN_NOTIFY_DP_PIN_D:
	case IFCONN_NOTIFY_DP_PIN_F:
		req_status_val = req_status_mask =
			DP_TX0_REQ_STATUS | DP_TX1_REQ_STATUS;
		pstate_mask = pstate_val =
			DP_TX2_PSTATE | DP_TX3_PSTATE;
		datapath_control_val[0] = 0x00000003;
		datapath_control_val[1] = 0x00000303;
		datapath_control_val[2] = 0x00030303;
		break;
	default:
		cal_log_debug(0, "IFCONN_NOTIFY_DP_PIN_UNKNOWN\n");
		break;
	}
#endif
	dp_write(PCS_SNPS_PHY_DATAPATH_CONTROL, datapath_control_val[0]);
	dp_reg_phy_update(req_status_val, req_status_mask);
	dp_write(PCS_SNPS_PHY_DATAPATH_CONTROL, datapath_control_val[1]);
	dp_phy_formal_write_mask(DP_CONFIG11, pstate_val, pstate_mask);

	dp_reg_phy_update(req_status_val, req_status_mask);
	dp_write(PCS_SNPS_PHY_DATAPATH_CONTROL, datapath_control_val[2]);
}

#define PHY_INIT_RBR
void dp_reg_phy_init(struct dp_cal_res *cal_res)
{
#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
	dwc3_exynos_phy_enable(1, 1);
	atomic_inc(&cal_res->usbdp_phy_en_cnt);
#endif
	dp_reg_usbdrd_qch_en(1);
	//dp_reg_phy_reset(1);
	dp_reg_phy_init_setting();
#if defined(PHY_INIT_RBR)
	dp_reg_phy_set_link_bw(cal_res, LINK_RATE_1_62Gbps);
#elif defined(PHY_INIT_HBR1)
	dp_reg_phy_set_link_bw(cal_res, LINK_RATE_2_7Gbps);
#elif defined(PHY_INIT_HBR2)
	dp_reg_phy_set_link_bw(cal_res, LINK_RATE_5_4Gbps);
#elif defined(PHY_INIT_HBR3)
	dp_reg_phy_set_link_bw(cal_res, LINK_RATE_8_1Gbps);
#endif

	dp_reg_phy_mode_setting(cal_res);
	dp_reg_phy_ssc_enable(0);
	//dp_reg_phy_reset(0);
	dp_reg_wait_phy_pll_lock();
	dp_reg_phy_post_init(cal_res);
}

void dp_reg_phy_disable(struct dp_cal_res *cal_res)
{
	//dp_reg_phy_reset(1);

	dp_reg_usbdrd_qch_en(0);
#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
	exynos_usbdrd_inform_dp_use(0, dp_reg_get_lane_count());
	dwc3_exynos_phy_enable(1, 0);
	atomic_dec(&cal_res->usbdp_phy_en_cnt);
#endif
}

void dp_reg_osc_clk_qch_con(unsigned int en)
{
	dp_write_mask(SYSTEM_OSCLK_QCH_FUNCTION_ENABLE, en, OSCCLK_QCH_FUNC_EN);
	cal_log_debug(0, "OSC_QCH_FUNC_EN(0x%08x)\n", dp_read(SYSTEM_OSCLK_QCH_FUNCTION_ENABLE));
}

#define NUM_OSC 16
#define NUM_OSC_SET_VAL 7
static unsigned int osc_clk[NUM_OSC][NUM_OSC_SET_VAL] = {
	/* OSC CLOCK FREQUENCY(MHz)
	 * OSC_CLK_DIV_HPD(0x50), OSC_CLK_DIV_HDCP_10US(0x54), OSC_CLK_DIV_GTC_1MS(0x58),
	 * OSC_CLK_DIV_AUX_1US(0x5C), OSC_CLK_DIV_MAN_UI(0x60), OSC_CLK_DIV_AUX_10US(0x64),
	 */
	{18, 36000, 179, 17999, 17, 8,  180},
	{20, 40000, 199, 19999, 19, 9,  200},
	{22, 44000, 219, 21999, 21, 10, 220},
	{24, 48000, 239, 23999, 23, 11, 240},
	{26, 52000, 259, 25999, 25, 12, 260},
	{28, 56000, 279, 27999, 27, 13, 280},
	{30, 60000, 299, 29999, 29, 14, 300},
	{32, 64000, 319, 31999, 31, 15, 320},
	{34, 68000, 339, 33999, 33, 16, 340},
	{36, 72000, 359, 35999, 35, 17, 360},
	{38, 76000, 379, 37999, 37, 18, 380},
	{40, 80000, 399, 39999, 39, 19, 400},
	{42, 84000, 419, 41999, 41, 20, 420},
	{44, 88000, 439, 43999, 43, 21, 440},
	{46, 92000, 459, 45999, 45, 22, 460},
	{48, 96000, 479, 47999, 47, 23, 480},
};

void dp_reg_set_osc_clk(unsigned int mhz)
{
	int i;

	//dp_reg_osc_clk_qch_con(true);
	for(i = 0; i < NUM_OSC; i++) {
		if (osc_clk[i][0] != mhz) {
			continue;
		} else {
			dp_write_mask(OSC_CLK_DIV_HPD, osc_clk[i][1], HPD_EVENT_CLK_COUNT);
			dp_write_mask(OSC_CLK_DIV_HDCP_10US, osc_clk[i][2], I2C_GEN10US_TIMER);
			dp_write_mask(OSC_CLK_DIV_GTC_1MS, osc_clk[i][3], GTC_1MS_OSC_CLK_COUNT);
			dp_write_mask(OSC_CLK_DIV_AUX_1US, osc_clk[i][4], AUX_1US_OSC_CLK_COUNT);
			dp_write_mask(OSC_CLK_DIV_AUX_MAN_UI, osc_clk[i][5], AUX_MAN_UI_OSC_CLK_COUNT);
			dp_write_mask(OSC_CLK_DIV_AUX_10US, osc_clk[i][6], AUX_10US_OSC_CLK_COUNT);
			cal_log_debug(0, "OSC_CLK_DIV_HPD(%d)\n", osc_clk[i][1]);
			cal_log_debug(0, "OSC_CLK_DIV_HDCP_10US(%d)\n", osc_clk[i][2]);
			cal_log_debug(0, "OSC_CLK_DIV_GTC_1MS(%d)\n", osc_clk[i][3]);
			cal_log_debug(0, "OSC_CLK_DIV_AUX_1US(%d)\n", osc_clk[i][4]);
			cal_log_debug(0, "OSC_CLK_DIV_AUX_MAN_UI(%d)\n", osc_clk[i][5]);
			cal_log_debug(0, "OSC_CLK_DIV_AUX_10US(%d)\n", osc_clk[i][6]);
		}
	}

	dp_reg_gf_clk_mux_sel_osc();
	dp_write_mask(PCS_CONTROL, 0x1, BIT_SWAP);
#ifdef DP_DUMP
	{
		dp_print_hex_dump(regs_dp[REGS_LINK][SST1].regs,
			regs_dp[REGS_LINK][SST1].regs + 0x50, 0x64 - 0x50);
	}
#endif
}

void dp_reg_init(struct dp_cal_res *cal_res)
{
	/* 9925 DP link addtional guide */
	/* dp set osc clk func and dp q ch enable func are
	 * moved to sw reset func to block ITMON error by clock gating
	 */
	dp_reg_set_osc_clk(40);

	dp_reg_phy_init(cal_res);
	dp_reg_function_enable();
	dp_reg_sw_function_en(1);
	dp_reg_set_lane_map_config(cal_res);
	dp_reg_set_interrupt(1);
}

void dp_reg_deinit(void)
{
	dp_reg_set_interrupt(0);
	dp_reg_sw_function_en(0);
	dp_reg_gf_clk_mux_sel_osc();
	dp_reg_osc_clk_qch_con(false);
}

void dp_reg_set_video_config(struct dp_cal_res *cal_res, videoformat video_format,
		u8 bpc, u8 range)
{
	cal_log_info(0, "color range: %d, bpc: %d\n", range, bpc);
	dp_reg_set_daynamic_range((range)?CEA_RANGE:VESA_RANGE);
	dp_write_mask(SST1_VIDEO_CONTROL, bpc, BPC);	/* 0 : 6bits, 1 : 8bits */
	dp_write_mask(SST1_VIDEO_CONTROL, 0, COLOR_FORMAT);	/* RGB */
#ifdef FEATURE_USE_DRM_EDID_PARSER
	dp_reg_drm_mode_register_setting();
#else
	dp_reg_video_format_register_setting(video_format);
#endif
	dp_reg_set_video_clock(cal_res);
	dp_reg_set_active_symbol(cal_res);
	dp_write_mask(SST1_VIDEO_MASTER_TIMING_GEN,	1, VIDEO_MASTER_TIME_GEN);
	dp_write_mask(SST1_MAIN_CONTROL, 0, VIDEO_MODE);
}

void dp_reg_set_bist_video_config(struct dp_cal_res *cal_res, videoformat video_format,
		u8 bpc, u8 type, u8 range)
{
	if (type < CTS_COLOR_RAMP) {
		dp_reg_set_video_config(cal_res, video_format, bpc, range);
		dp_write_mask(SST1_VIDEO_BIST_CONTROL, type, BIST_TYPE);
		dp_write_mask(SST1_VIDEO_BIST_CONTROL, 0, CTS_BIST_EN);
	} else {
		if (type == CTS_COLOR_SQUARE_CEA)
			dp_reg_set_video_config(cal_res, video_format, bpc, CEA_RANGE);
		else
			dp_reg_set_video_config(cal_res, video_format, bpc, VESA_RANGE);

		dp_write_mask(SST1_VIDEO_BIST_CONTROL,
				type - CTS_COLOR_RAMP, CTS_BIST_TYPE);
		dp_write_mask(SST1_VIDEO_BIST_CONTROL,
				1, CTS_BIST_EN);
	}

	dp_reg_set_video_bist_mode(1);

	cal_log_info(0, "set bist video config format:%d range:%d bpc:%d type:%d\n",
			video_format, (range)?1:0, (bpc)?1:0, type);
}

void dp_reg_set_bist_video_config_for_blue_screen(struct dp_cal_res *cal_res, videoformat video_format)
{
	dp_reg_set_video_config(cal_res, video_format, BPC_8, CEA_RANGE); /* 8 bits */
	dp_write(SST1_VIDEO_BIST_USER_DATA_R, 0x00);
	dp_write(SST1_VIDEO_BIST_USER_DATA_G, 0x00);
	dp_write(SST1_VIDEO_BIST_USER_DATA_B, 0xFF);
	dp_write_mask(SST1_VIDEO_BIST_CONTROL, 1, BIST_USER_DATA_EN);
	dp_write_mask(SST1_VIDEO_BIST_CONTROL, 0, CTS_BIST_EN);
	dp_reg_set_video_bist_mode(1);

	cal_log_debug(0, "set bist video config for blue screen\n");
}

void dp_reg_set_avi_infoframe(struct infoframe avi_infoframe)
{
	u32 avi_infoframe_data = 0;

	avi_infoframe_data = ((u32)avi_infoframe.data[3] << 24) | ((u32)avi_infoframe.data[2] << 16)
			| ((u32)avi_infoframe.data[1] << 8) | (u32)avi_infoframe.data[0];
	dp_write(SST1_INFOFRAME_AVI_PACKET_DATA_SET0, avi_infoframe_data);

	avi_infoframe_data = ((u32)avi_infoframe.data[7] << 24) | ((u32)avi_infoframe.data[6] << 16)
			| ((u32)avi_infoframe.data[5] << 8) | (u32)avi_infoframe.data[4];
	dp_write(SST1_INFOFRAME_AVI_PACKET_DATA_SET1, avi_infoframe_data);

	avi_infoframe_data = ((u32)avi_infoframe.data[11] << 24) | ((u32)avi_infoframe.data[10] << 16)
			| ((u32)avi_infoframe.data[9] << 8) | (u32)avi_infoframe.data[8];
	dp_write(SST1_INFOFRAME_AVI_PACKET_DATA_SET2, avi_infoframe_data);

	avi_infoframe_data = (u32)avi_infoframe.data[12];
	dp_write(SST1_INFOFRAME_AVI_PACKET_DATA_SET3, avi_infoframe_data);

	dp_write_mask(SST1_INFOFRAME_UPDATE_CONTROL, 1, AVI_INFO_UPDATE);
	dp_write_mask(SST1_INFOFRAME_SEND_CONTROL, 1, AVI_INFO_SEND);
}

void dp_reg_set_spd_infoframe(struct infoframe spd_infoframe)
{
	int i, j;
	int data_ind = 0;
	u32 spd_infoframe_data;

	dp_write(SST1_INFOFRAME_SPD_PACKET_TYPE, spd_infoframe.type_code);

	for (i = 0; i < 24; i += 4) {
		spd_infoframe_data = 0;

		for (j = 0; j < 32; j += 8)
			spd_infoframe_data |= spd_infoframe.data[data_ind++] << j;

		dp_write(SST1_INFOFRAME_SPD_PACKET_DATA_SET0 + i, spd_infoframe_data);
	}

	dp_write(SST1_INFOFRAME_SPD_PACKET_DATA_SET6, spd_infoframe.data[24]);

	dp_write_mask(SST1_INFOFRAME_UPDATE_CONTROL, 1, SPD_INFO_UPDATE);
	dp_write_mask(SST1_INFOFRAME_SEND_CONTROL, 1, SPD_INFO_SEND);
}

void dp_reg_set_audio_infoframe(struct infoframe audio_infoframe, u32 en)
{
	u32 audio_infoframe_data = 0;

	audio_infoframe_data = ((u32)audio_infoframe.data[3] << 24) | ((u32)audio_infoframe.data[2] << 16)
			| ((u32)audio_infoframe.data[1] << 8) | (u32)audio_infoframe.data[0];
	dp_write(SST1_INFOFRAME_AUDIO_PACKET_DATA_SET0, audio_infoframe_data);

	audio_infoframe_data = ((u32)audio_infoframe.data[7] << 24) | ((u32)audio_infoframe.data[6] << 16)
			| ((u32)audio_infoframe.data[5] << 8) | (u32)audio_infoframe.data[4];
	dp_write(SST1_INFOFRAME_AUDIO_PACKET_DATA_SET1, audio_infoframe_data);

	audio_infoframe_data = ((u32)audio_infoframe.data[9] << 8) | (u32)audio_infoframe.data[8];
	dp_write(SST1_INFOFRAME_AUDIO_PACKET_DATA_SET2, audio_infoframe_data);

	dp_write_mask(SST1_INFOFRAME_UPDATE_CONTROL, en, AUDIO_INFO_UPDATE);
	dp_write_mask(SST1_INFOFRAME_SEND_CONTROL, en, AUDIO_INFO_SEND);
}

void dp_reg_set_hdr_infoframe(struct infoframe hdr_infoframe, u32 en)
{
	int i, j;
	u32 hdr_infoframe_data = 0;

	if (en == 1) {
		for (i = 0; i < HDR_INFOFRAME_LENGTH; i++) {
			for (j = 0; j < DATA_NUM_PER_REG; j++) {
				hdr_infoframe_data |=
					(u32)hdr_infoframe.data[i]
					<< ((j % DATA_NUM_PER_REG) * INFOFRAME_DATA_SIZE);

				if (j < DATA_NUM_PER_REG - 1)
					i++;

				if (i >= HDR_INFOFRAME_LENGTH)
					break;
			}

			dp_write((SST1_INFOFRAME_HDR_PACKET_DATA_SET_0 +
				i / DATA_NUM_PER_REG * DATA_NUM_PER_REG),
				hdr_infoframe_data);

			hdr_infoframe_data = 0;
		}
	}

	for (i = 0; i <= SST1_INFOFRAME_HDR_PACKET_DATA_SET_7 - SST1_INFOFRAME_HDR_PACKET_DATA_SET_0;
		i += DATA_NUM_PER_REG) {
		cal_log_debug(0, "HDR_PACKET_DATA_SET_%d = 0x%x",
			i / DATA_NUM_PER_REG,
			dp_read(SST1_INFOFRAME_HDR_PACKET_DATA_SET_0 + i));
	}

	dp_write_mask(SST1_INFOFRAME_UPDATE_CONTROL, en, HDR_INFO_UPDATE);
	dp_write_mask(SST1_INFOFRAME_SEND_CONTROL, en, HDR_INFO_SEND);
}

void dp_reg_start(void)
{
	dp_reg_set_interrupt_mask(VIDEO_FIFO_UNDER_FLOW_MASK, 1);
	dp_write_mask(SST1_VIDEO_ENABLE, 1, VIDEO_EN);
}

void dp_reg_video_mute(u32 en)
{
/*	cal_log_debug(0, "set mute %d\n", en);
	dp_write_mask(SST1_VIDEO_MUTE, en, VIDEO_MUTE);
 */
}

void dp_reg_stop(void)
{
	dp_reg_set_interrupt_mask(VIDEO_FIFO_UNDER_FLOW_MASK, 0);
	dp_write_mask(SST1_VIDEO_ENABLE, 0, VIDEO_EN);
}

/* Set SA CRC, For Sorting Vector */
void dp_reg_set_stand_alone_crc(u32 crc_ln0_ref, u32 crc_ln1_ref, u32 crc_ln2_ref, u32 crc_ln3_ref)
{
	dp_write_mask(PCS_SA_CRC_CONTROL_2, crc_ln0_ref, SA_CRC_LN0_REF);
	dp_write_mask(PCS_SA_CRC_CONTROL_3, crc_ln1_ref, SA_CRC_LN1_REF);
	dp_write_mask(PCS_SA_CRC_CONTROL_4, crc_ln2_ref, SA_CRC_LN2_REF);
	dp_write_mask(PCS_SA_CRC_CONTROL_5, crc_ln3_ref, SA_CRC_LN3_REF);
}

void dp_reg_set_result_flag_clear(void)
{
	dp_write_mask(PCS_SA_CRC_CONTROL_1, 1, SA_CRC_CLEAR);
	dp_write_mask(PCS_SA_CRC_CONTROL_1, 0, SA_CRC_CLEAR);
}

void dp_reg_enable_stand_alone_crc_hw(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(PCS_SA_CRC_CONTROL_1, 0, SA_CRC_SW_COMPARE);	/* use H/W compare */

	dp_write_mask(PCS_SA_CRC_CONTROL_1, val,
		SA_CRC_LANE_0_ENABLE | SA_CRC_LANE_1_ENABLE | SA_CRC_LANE_2_ENABLE | SA_CRC_LANE_3_ENABLE);
}

int dp_reg_get_stand_alone_crc_result(void)
{
	u32 val;
	int err = 0;

	val = dp_read_mask(PCS_SA_CRC_CONTROL_1, 0x00000FF0);
	val = val >> 4;

	if (val == 0xF0) {
		cal_log_info(0, "SA CRC Pass !!!\n");
	} else {
		err = -1;
		cal_log_info(0, "SA CRC Fail : 0x%02X !!!\n", val);
	}

	return  err;
}

/* SA CRC Condition : 8bpc, 4lane, 640x10 size, BIST_TYPE=0, BIST_WIDTH =0 */
int dp_reg_stand_alone_crc_sorting(struct dp_cal_res *cal_res)
{
	int ret;

	dp_reg_init(cal_res);
	dp_reg_set_lane_count(4);
	dp_reg_set_bist_video_config(cal_res, V640X10P60SACRC,
			BPC_8, COLOR_BAR, VESA_RANGE);
	dp_reg_set_stand_alone_crc(0x135E, 0x135E, 0x135E, 0x135E);
	dp_reg_enable_stand_alone_crc_hw(1);
	dp_reg_start();

	usleep_range(20000, 20050);

	dp_reg_set_result_flag_clear();

	usleep_range(20000, 20050);

	ret =  dp_reg_get_stand_alone_crc_result();

	dp_reg_set_result_flag_clear();
	dp_reg_enable_stand_alone_crc_hw(0);

	dp_reg_set_video_bist_mode(0);
	dp_reg_stop();

	return ret;
}

void dp_reg_set_audio_m_n(struct dp_cal_res *cal_res, audio_sync_mode audio_sync_mode,
		enum audio_sampling_frequency audio_sampling_freq)
{
	u32 link_bandwidth_set;
	u32 array_set;
	u32 m_value;
	u32 n_value;

	link_bandwidth_set = dp_reg_phy_get_link_bw(cal_res);
	dp_reg_phy_print_link_bw(cal_res);
	if (link_bandwidth_set == LINK_RATE_1_62Gbps)
		array_set = 0;
	else if (link_bandwidth_set == LINK_RATE_2_7Gbps)
		array_set = 1;
	else if (link_bandwidth_set == LINK_RATE_5_4Gbps)
		array_set = 2;
	else /* if (link_bandwidth_set == LINK_RATE_8_1Gbps) */
		array_set = 3;

	if (audio_sync_mode == ASYNC_MODE) {
		m_value = audio_async_m_n[0][array_set][audio_sampling_freq];
		n_value = audio_async_m_n[1][array_set][audio_sampling_freq];
		dp_write_mask(SST1_MAIN_CONTROL, 0, MAUD_MODE);
	} else {
		m_value = audio_sync_m_n[0][array_set][audio_sampling_freq];
		n_value = audio_sync_m_n[1][array_set][audio_sampling_freq];
		dp_write_mask(SST1_MAIN_CONTROL, 1, MAUD_MODE);
	}

	dp_write(SST1_MAUD_SFR_CONFIGURE, m_value);
	dp_write(SST1_NAUD_SFR_CONFIGURE, n_value);
}

void dp_reg_set_audio_function_enable(u32 en)
{
	dp_write_mask(SYSTEM_SST1_FUNCTION_ENABLE, en, SST1_AUDIO_FUNC_EN);
}

void dp_reg_set_init_dma_config(void)
{
	dp_write_mask(SST1_AUDIO_CONTROL, 1, AUD_DMA_IF_MODE_CONFIG);
	dp_write_mask(SST1_AUDIO_CONTROL, 0, AUD_DMA_IF_LTNCY_TRG_MODE);
}

void dp_reg_set_dma_force_req_low(u32 en)
{
	if (en == 1) {
		dp_write_mask(SST1_AUDIO_DMA_REQUEST_LATENCY_CONFIG,
				0, AUD_DMA_FORCE_REQ_VAL);
		dp_write_mask(SST1_AUDIO_DMA_REQUEST_LATENCY_CONFIG,
				1, AUD_DMA_FORCE_REQ_SEL);
	} else
		dp_write_mask(SST1_AUDIO_DMA_REQUEST_LATENCY_CONFIG,
				0, AUD_DMA_FORCE_REQ_SEL);

	cal_log_info(0, "AUDIO_DMA_REQUEST_LATENCY_CONFIG = 0x%x\n",
			dp_read(SST1_AUDIO_DMA_REQUEST_LATENCY_CONFIG));
}

void dp_reg_set_dma_burst_size(enum audio_dma_word_length word_length)
{
	dp_write_mask(SST1_AUDIO_CONTROL, word_length, DMA_BURST_SEL);
}

void dp_reg_set_dma_pack_mode(enum audio_16bit_dma_mode dma_mode)
{
	dp_write_mask(SST1_AUDIO_CONTROL, dma_mode, AUDIO_BIT_MAPPING_TYPE);
}

void dp_reg_set_pcm_size(enum audio_bit_per_channel audio_bit_size)
{
	dp_write_mask(SST1_AUDIO_CONTROL, audio_bit_size, PCM_SIZE);
}

void dp_reg_set_audio_ch_status_same(u32 en)
{
	dp_write_mask(SST1_AUDIO_CONTROL, en, AUDIO_CH_STATUS_SAME);
}

void dp_reg_set_audio_ch(u32 audio_ch_cnt)
{
	dp_write_mask(SST1_AUDIO_BUFFER_CONTROL,
			audio_ch_cnt - 1, MASTER_AUDIO_CHANNEL_COUNT);
}

void dp_reg_set_audio_ch_mapping(u8 pkt_1, u8 pkt_2,
		u8 pkt_3, u8 pkt_4,	u8 pkt_5, u8 pkt_6, u8 pkt_7, u8 pkt_8)
{
	dp_write_mask(SST1_AUDIO_CHANNEL_1_4_REMAP, pkt_1, AUD_CH_01_REMAP);
	dp_write_mask(SST1_AUDIO_CHANNEL_1_4_REMAP, pkt_2, AUD_CH_02_REMAP);
	dp_write_mask(SST1_AUDIO_CHANNEL_1_4_REMAP, pkt_3, AUD_CH_03_REMAP);
	dp_write_mask(SST1_AUDIO_CHANNEL_1_4_REMAP, pkt_4, AUD_CH_04_REMAP);

	dp_write_mask(SST1_AUDIO_CHANNEL_5_8_REMAP, pkt_5, AUD_CH_05_REMAP);
	dp_write_mask(SST1_AUDIO_CHANNEL_5_8_REMAP, pkt_6, AUD_CH_06_REMAP);
	dp_write_mask(SST1_AUDIO_CHANNEL_5_8_REMAP, pkt_7, AUD_CH_07_REMAP);
	dp_write_mask(SST1_AUDIO_CHANNEL_5_8_REMAP, pkt_8, AUD_CH_08_REMAP);

	cal_log_debug(0, "audio 1~4 channel mapping = 0x%X\n",
			dp_read(SST1_AUDIO_CHANNEL_1_4_REMAP));
	cal_log_debug(0, "audio 5~8 channel mapping = 0x%X\n",
			dp_read(SST1_AUDIO_CHANNEL_5_8_REMAP));
}

void dp_reg_set_audio_fifo_function_enable(u32 en)
{
	dp_write_mask(SYSTEM_SST1_FUNCTION_ENABLE, en, SST1_AUDIO_FIFO_FUNC_EN);
}

void dp_reg_set_audio_sampling_frequency(struct dp_cal_res *cal_res, enum audio_sampling_frequency audio_sampling_freq)
{
	u32 link_bandwidth_set;
	u32 n_aud_master_set;

	link_bandwidth_set = dp_reg_phy_get_link_bw(cal_res);
	dp_reg_phy_print_link_bw(cal_res);
	if (link_bandwidth_set == LINK_RATE_1_62Gbps)
		n_aud_master_set = 0;
	else if (link_bandwidth_set == LINK_RATE_2_7Gbps)
		n_aud_master_set = 1;
	else if (link_bandwidth_set == LINK_RATE_5_4Gbps)
		n_aud_master_set = 2;
	else /* if (link_bandwidth_set == LINK_RATE_8_1Gbps) */
		n_aud_master_set = 3;

	dp_write(SST1_MAUD_MASTER_MODE, m_aud_master[audio_sampling_freq]);
	dp_write(SST1_NAUD_MASTER_MODE, n_aud_master[n_aud_master_set]);
}

void dp_reg_set_dp_audio_enable(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(SST1_AUDIO_ENABLE, val, AUDIO_EN);
}

void dp_reg_set_audio_master_mode_enable(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(SST1_AUDIO_MASTER_TIMING_GEN,
			val, AUDIO_MASTER_TIME_GEN);
}

void dp_reg_set_ch_status_ch_cnt(u32 audio_ch_cnt)
{
	dp_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET0,
				audio_ch_cnt, CH_NUM);

	dp_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET0,
				audio_ch_cnt, SOURCE_NUM);

	dp_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_0,
				audio_ch_cnt, CH_NUM);

	dp_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_0,
				audio_ch_cnt, SOURCE_NUM);
}

void dp_reg_set_ch_status_word_length(	enum audio_bit_per_channel audio_bit_size)
{
	u32 word_max = 0;
	u32 sample_word_length = 0;

	switch (audio_bit_size) {
	case AUDIO_24_BIT:
		word_max = 1;
		sample_word_length = 0x05;
		break;

	case AUDIO_16_BIT:
		word_max = 0;
		sample_word_length = 0x01;
		break;

	case AUDIO_20_BIT:
		word_max = 0;
		sample_word_length = 0x05;
		break;

	default:
		word_max = 0;
		sample_word_length = 0x00;
		break;
	}

	dp_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET1, word_max, WORD_MAX);
	dp_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET1, sample_word_length, WORD_LENGTH);
	dp_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_1, word_max, WORD_MAX);
	dp_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_1, sample_word_length, WORD_LENGTH);
}

void dp_reg_set_ch_status_sampling_frequency(enum audio_sampling_frequency audio_sampling_freq)
{
	u32 fs_freq = 0;

	switch (audio_sampling_freq) {
	case FS_32KHZ:
		fs_freq = 0x03;
		break;
	case FS_44KHZ:
		fs_freq = 0x00;
		break;
	case FS_48KHZ:
		fs_freq = 0x02;
		break;
	case FS_88KHZ:
		fs_freq = 0x08;
		break;
	case FS_96KHZ:
		fs_freq = 0x0A;
		break;
	case FS_176KHZ:
		fs_freq = 0x0C;
		break;
	case FS_192KHZ:
		fs_freq = 0x0E;
		break;
	default:
		fs_freq = 0x00;
		break;
	}

	dp_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET0, fs_freq, FS_FREQ);
	dp_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_0, fs_freq, FS_FREQ);
}

void dp_reg_set_ch_status_clock_accuracy(enum audio_clock_accuracy clock_accuracy)
{
	dp_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET0, clock_accuracy, CLK_ACCUR);
	dp_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_0, clock_accuracy, CLK_ACCUR);
}

void dp_reg_wait_buf_full(void)
{
	u32 cnt = 2000;
	u32 state = 0;

	do {
		state = (dp_read(SST1_AUDIO_BUFFER_CONTROL) & MASTER_AUDIO_BUFFER_LEVEL) >> MASTER_AUDIO_BUFFER_LEVEL_BIT_POS;
		cnt--;
		udelay(1);
	} while ((state < AUDIO_BUF_FULL_SIZE) && cnt);

	if (!cnt)
		cal_log_err(0, "%s is timeout.\n", __func__);
}

void dp_reg_print_audio_state(void)
{
	u32 val1 = 0;
	u32 val2 = 0;
	u32 val3 = 0;
	u32 val4 = 0;
	u32 val5 = 0;

	val1 = dp_read(SYSTEM_SST1_FUNCTION_ENABLE);
	val2 = dp_read(SST1_AUDIO_ENABLE);
	val3 = dp_read(SST1_AUDIO_MASTER_TIMING_GEN);
	val4 = dp_read(SST1_AUDIO_DMA_REQUEST_LATENCY_CONFIG);
	val5 = dp_read(SST1_AUDIO_CONTROL);
	cal_log_info(0, "audio state: func_en=0x%x, aud_en=0x%x, master_t_gen=0x%x, dma_req=0x%x, aud_con=0x%X\n",
			val1, val2, val3, val4, val5);
}

void dp_reg_set_dma_req_gen(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(SST1_AUDIO_CONTROL, val, DMA_REQ_GEN_EN);
}

void dp_reg_set_clear_audio_fifo(void)
{
	dp_write_mask(SST1_MAIN_FIFO_CONTROL, 1, CLEAR_AUDIO_1_FIFO);
}

void dp_set_audio_ch_status(struct dp_audio_config_data *audio_config_data)
{
	dp_reg_set_ch_status_ch_cnt(audio_config_data->audio_channel_cnt);
	dp_reg_set_ch_status_word_length(audio_config_data->audio_bit);
	dp_reg_set_ch_status_sampling_frequency(audio_config_data->audio_fs);
	dp_reg_set_ch_status_clock_accuracy(NOT_MATCH);
}

void dp_wait_audio_buf_empty(struct dp_cal_res *cal_res)
{
	u32 cnt = 1000;

	do {
		cnt--;
		udelay(1);
	} while (!cal_res->audio_buf_empty_check && cnt);

	if (!cnt)
		cal_log_err(0, "%s is timeout.\n", __func__);
}

void dp_reg_audio_enable(struct dp_cal_res *cal_res, struct dp_audio_config_data *audio_config_data)
{
	dp_reg_set_audio_m_n(cal_res, ASYNC_MODE, audio_config_data->audio_fs);
	dp_reg_set_audio_function_enable(audio_config_data->audio_enable);
	dp_reg_set_dma_burst_size(audio_config_data->audio_word_length);
	dp_reg_set_pcm_size(audio_config_data->audio_bit);
	dp_reg_set_dma_pack_mode(audio_config_data->audio_packed_mode);
	dp_reg_set_audio_ch(audio_config_data->audio_channel_cnt);
	dp_reg_set_audio_fifo_function_enable(audio_config_data->audio_enable);
	dp_reg_set_audio_sampling_frequency(cal_res, audio_config_data->audio_fs);
	/* channel mapping: FL, FR, C, SW, RL, RR */
	dp_reg_set_audio_ch_mapping(1, 2, 4, 3, 5, 6, 7, 8);
	dp_reg_set_dp_audio_enable(audio_config_data->audio_enable);
	dp_set_audio_ch_status(audio_config_data);
	dp_reg_set_audio_ch_status_same(1);
	dp_reg_set_audio_master_mode_enable(audio_config_data->audio_enable);
	dp_reg_print_audio_state();
}

void dp_reg_audio_disable(void)
{
	if (dp_read_mask(SST1_AUDIO_ENABLE, AUDIO_EN) == 1) {
		udelay(1000);
		dp_reg_set_dp_audio_enable(0);
		dp_reg_set_audio_fifo_function_enable(0);
		dp_reg_set_clear_audio_fifo();
		cal_log_info(0, "audio_disable\n");
	} else
		cal_log_info(0, "audio_disable, AUDIO_EN = 0\n");
}

void dp_reg_audio_wait_buf_full(void)
{
	dp_reg_set_audio_master_mode_enable(0);
	dp_reg_set_dma_req_gen(0);
	cal_log_info(0, "dp_audio_wait_buf_full\n");
}

void dp_reg_audio_bist_enable(struct dp_cal_res *cal_res, struct dp_audio_config_data audio_config_data)
{
	cal_log_info(0, "dp_audio_bist\n");
	cal_log_info(0, "audio_enable = %d\n", audio_config_data.audio_enable);
	cal_log_info(0, "audio_channel_cnt = %d\n", audio_config_data.audio_channel_cnt);
	cal_log_info(0, "audio_fs = %d\n", audio_config_data.audio_fs);

	if (audio_config_data.audio_enable == 1) {
		dp_reg_set_audio_m_n(cal_res, ASYNC_MODE, audio_config_data.audio_fs);
		dp_reg_set_audio_function_enable(audio_config_data.audio_enable);

		dp_reg_set_audio_ch(audio_config_data.audio_channel_cnt);
		dp_reg_set_audio_fifo_function_enable(audio_config_data.audio_enable);
		dp_reg_set_audio_ch_status_same(1);
		dp_reg_set_audio_sampling_frequency(cal_res, audio_config_data.audio_fs);
		dp_reg_set_dp_audio_enable(audio_config_data.audio_enable);
		dp_reg_set_audio_bist_mode(1);
		dp_set_audio_ch_status(&audio_config_data);
		dp_reg_set_audio_master_mode_enable(audio_config_data.audio_enable);
	} else {
		dp_reg_set_audio_master_mode_enable(0);
		dp_reg_audio_disable();
	}
}

void dp_reg_audio_init_config(struct dp_cal_res *cal_res)
{
	dp_reg_set_audio_m_n(cal_res, ASYNC_MODE, FS_48KHZ);
	dp_reg_set_audio_function_enable(1);
	dp_reg_set_audio_sampling_frequency(cal_res, FS_48KHZ);
	dp_reg_set_dp_audio_enable(1);
	dp_reg_set_audio_master_mode_enable(1);
	cal_log_info(0, "dp_audio_init_config\n");
}

void dp_reg_set_hdcp22_system_enable(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(HDCP22_SYS_EN, val, SYSTEM_ENABLE);
}

void dp_reg_set_hdcp22_mode(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(SYSTEM_COMMON_FUNCTION_ENABLE, val, HDCP22_FUNC_EN);
}

void dp_reg_set_hdcp22_encryption_enable(u32 en)
{
	u32 val = en ? ~0 : 0;

	dp_write_mask(HDCP22_CONTROL, val, HDCP22_ENC_EN);
}

u32 dp_reg_get_hdcp22_encryption_enable(void)
{
	return dp_read_mask(HDCP22_CONTROL, HDCP22_ENC_EN);
}

void dp_reg_set_aux_pn_inv(u32 val)
{
	dp_write_mask(AUX_CONTROL, val, AUX_PN_INV);
}

static void dp_reg_set_cr_clk_high(void)
{
	dp_phy_write_mask(PHY_CR_PARA_CON0, 0x1, PHY0_CR_PARA_CLK);
	udelay(1);
}

static void dp_reg_set_cr_clk_low(void)
{
	dp_phy_write_mask(PHY_CR_PARA_CON0, 0x0, PHY0_CR_PARA_CLK);
	udelay(1);
}

static bool dp_reg_toggle_cr_clk(int cycle_cnt, bool check_ack)
{
	int clk_toggle_cycle;

	for (clk_toggle_cycle = 0; clk_toggle_cycle < cycle_cnt; clk_toggle_cycle++) {
		dp_reg_set_cr_clk_high();
		dp_reg_set_cr_clk_low();
		if (check_ack) {
			if (dp_phy_read_mask(PHY_CR_PARA_CON0, PHY0_CR_PARA_ACK)) {
				return true;
			}
		}
	}
	if (check_ack)
		return false;
	else
		return true;
}

static void dp_reg_clear_cr_port(void)
{
	dp_phy_write(PHY_CR_PARA_CON1, 0x0);
	dp_phy_write(PHY_CR_PARA_CON2, 0x0);
	dp_phy_write(PHY_CR_PARA_CON0, 0x1);

	do {
		dp_reg_toggle_cr_clk(1, false);
		if (dp_phy_read_mask(PHY_CR_PARA_CON0, PHY0_CR_PARA_ACK) == 0)
			break;
	} while (1);

	dp_phy_write_mask(PHY_CR_PARA_CON0, 0, PHY0_CR_PARA_CLK);
}

void dp_reg_cr_write_mask(u16 addr, u16 data, u16 mask)
{
	int err = 0;
	u16 old = 0;

	old = dp_reg_cr_read(addr, &err);
	cal_log_debug(0, "[CR][%04X][%04X]", addr, old);
	data = (data & mask) | (old & ~mask);
	dp_reg_cr_write(addr, data);
	old = dp_reg_cr_read(addr, &err);
	cal_log_debug(0, "[CR][%04X][%04X]", addr, old);
}

void dp_reg_cr_write(u16 addr, u16 data)
{
	u32 clk_cycle = 0;

	dp_reg_clear_cr_port();

	dp_phy_formal_write_mask(PHY_CR_PARA_CON0, (addr << 16) | \
			PHY0_CR_PARA_CLK, PHY0_CR_PARA_ADDR | PHY0_CR_PARA_CLK);
	dp_reg_set_cr_clk_low();

	dp_phy_formal_write_mask(PHY_CR_PARA_CON2, (data << 16) | \
			PHY0_CR_PARA_WR_EN, PHY0_CR_PARA_WR_DATA | PHY0_CR_PARA_WR_EN);

	dp_reg_set_cr_clk_high();
	dp_reg_set_cr_clk_low();

	dp_phy_write_mask(PHY_CR_PARA_CON2, 0x0, PHY0_CR_PARA_WR_EN);

	do {
		dp_reg_set_cr_clk_high();
		if (dp_phy_read_mask(PHY_CR_PARA_CON0, PHY0_CR_PARA_ACK))
			break;
		dp_reg_set_cr_clk_low();
		clk_cycle++;
	} while (clk_cycle < 10);

	if (clk_cycle == 10)
		cal_log_info(0, "CR write failed to (0x%04x)\n", addr);
	else
		dp_reg_set_cr_clk_low();
}

u16 dp_reg_cr_read(u16 addr, int *err)
{
	u32 clk_cycle = 0;

	dp_reg_clear_cr_port();

	dp_phy_formal_write_mask(PHY_CR_PARA_CON0, (addr << 16) | PHY0_CR_PARA_CLK, \
			PHY0_CR_PARA_ADDR | PHY0_CR_PARA_CLK);
	dp_phy_write_mask(PHY_CR_PARA_CON1, 0x1, PHY0_CR_PARA_RD_EN);

	dp_reg_set_cr_clk_low();
	dp_reg_set_cr_clk_high();

	dp_phy_write_mask(PHY_CR_PARA_CON1, 0x0, PHY0_CR_PARA_RD_EN);

	dp_reg_set_cr_clk_low();

	do {
		dp_reg_set_cr_clk_high();
		if (dp_phy_read_mask(PHY_CR_PARA_CON0, PHY0_CR_PARA_ACK))
			break;
		dp_reg_set_cr_clk_low();
		clk_cycle++;
	} while (clk_cycle < 10);

	if (clk_cycle == 10) {
		cal_log_debug(0, "CR read failed to (0x%04x)\n", addr);
		*err = 1;
		return -1;
	} else {
		dp_reg_set_cr_clk_low();
		*err = 0;
		return (u16)(dp_phy_read_mask(PHY_CR_PARA_CON1, PHY0_CR_PARA_RD_DATA) >> 16);
	}
}
