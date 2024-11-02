// SPDX-License-Identifier: GPL-2.0-only
/* mcd_drm_dsi.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 * Authors:
 *	Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <asm/unaligned.h>
#include <linux/of.h>
#include <exynos_drm_freq_hop.h>
#include <exynos_drm_dsim.h>
#include <mcd_drm_dsim.h>
#include <dsim_cal.h>

static inline void copy_pmsk_value(struct stdphy_pms *pms, unsigned int *array)
{
	if (!pms || !array) {
		pr_err("ERR:%s: Invalid argument\n", __func__);
		return;
	}

	pms->p = array[0];
	pms->m = array[1];
	pms->s = array[2];
	pms->k = array[3];
	pms->mfr = array[4];
	pms->mrr = array[5];
	pms->sel_pf = array[6];
	pms->icp = array[7];
	pms->afc_enb = array[8];
	pms->extafc = array[9];
	pms->feed_en = array[10];
	pms->fsel = array[11];
	pms->fout_mask = array[12];
	pms->rsel = array[13];
}

static inline int of_get_pll_pmsk(struct device_node *np, struct stdphy_pms *pms)
{
	int ret;
	int pmsk_cnt;
	unsigned int pmsk[PMSK_VALUE_COUNT];

	pmsk_cnt = of_property_count_u32_elems(np, DT_NAME_PMSK);
	if (pmsk_cnt != PMSK_VALUE_COUNT) {
		pr_err("ERR:%s: Wrong pmks element count: %d\n", __func__, pmsk_cnt);
		if (pmsk_cnt < 0)
			pmsk_cnt = 0;
		WARN_ON(1);
	}

	ret = of_property_read_u32_array(np, DT_NAME_PMSK, pmsk, pmsk_cnt);
	if (ret) {
		pr_err("ERR:%s: Failed to get pmsk value : %d\n", __func__, ret);
		return ret;
	}

	if (pms)
		copy_pmsk_value(pms, pmsk);

	return ret;
}

static int mcd_dsim_of_get_pll_param(struct device_node *np, u32 khz, struct stdphy_pms *pms)
{
	int ret;
	u32 hs_clk;
	struct device_node *hs_table, *clock_info;

	if (!np) {
		pr_err("ERR:%s: Invalid device node\n", __func__);
		return -EINVAL;
	}

	if (khz > MAX_HS_CLOCK) {
		pr_err("ERR:%s: Wrong clock: Exceed max clock, input clock: %d\n", __func__, khz);
		return -EINVAL;
	}

	hs_table = of_parse_phandle(np, DT_NAME_HS_TIMING, 0);
	if (!hs_table) {
		pr_err("ERR:%s: Failed to hs table node: %s\n", __func__, DT_NAME_HS_TIMING);
		return -EINVAL;
	}

	for_each_child_of_node(hs_table, clock_info) {
		ret = of_property_read_u32(clock_info, "hs_clk", &hs_clk);
		if (ret) {
			pr_err("ERR:%s: Failed to get hs_clk info from dt\n", __func__);
			return ret;
		}

		if (hs_clk != khz)
			continue;

		ret = of_get_pll_pmsk(clock_info, pms);
		if (ret)
			pr_err("ERR:%s Failed to get pll pmsk from dt, input clock: %d\n", __func__, khz);

		return ret;
	}
	pr_err("ERR:%s Failed to found pll info from dt, input clock: %d\n", __func__, khz);
	return -EINVAL;
}

int mcd_dsim_check_dsi_freq(struct dsim_device *dsim, unsigned int freq)
{
	int ret;
	struct stdphy_pms pms;

	ret = mcd_dsim_of_get_pll_param(dsim->dev->of_node, freq, &pms);
	if (ret) {
		pr_err("FREQ_HOP: ERR:%s: failed to get pll param\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int mcd_dsim_update_dsi_freq(struct dsim_device *dsim, unsigned int freq)
{
	int ret;
	struct stdphy_pms pms;

	ret = mcd_dsim_of_get_pll_param(dsim->dev->of_node, freq, &pms);
	if (ret) {
		pr_err("FREQ_HOP: ERR:%s: failed to get pll param\n", __func__);
		return -EINVAL;
	}

	pr_info("FREQ_HOP: %s: found (dsi_freq:%d kHz, pmsk:%d %d %d %d)\n",
			__func__, freq, pms.p, pms.m, pms.s, pms.k);

	dsim->freq_hop.request_m = pms.m;
	dsim->freq_hop.request_k = pms.k;

	return 0;
}
EXPORT_SYMBOL(mcd_dsim_update_dsi_freq);
