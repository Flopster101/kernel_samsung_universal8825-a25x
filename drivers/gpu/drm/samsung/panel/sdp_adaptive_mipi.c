/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "panel.h"
#include "panel_drv.h"
#include "panel_debug.h"
#include "sdp_adaptive_mipi.h"
#include "dev_ril_header.h"

static int parse_sdp_mipi_freq(struct device_node *np,
		struct adaptive_mipi_v2_info *sdp_adap_mipi)
{
	int i;
	int ret, num_elems;
	size_t cnt = 0;
	char buf[SZ_128];

	num_elems = of_property_count_u32_elems(np, DT_NAME_MIPI_FREQ_LISTS);
	if (num_elems < 0) {
		panel_err("of_property_count_u32_elems fail %s\n", DT_NAME_MIPI_FREQ_LISTS);
		return -EINVAL;
	}
	sdp_adap_mipi->mipi_clocks_size = num_elems;

	ret = of_property_read_u32_array(np, DT_NAME_MIPI_FREQ_LISTS,
			sdp_adap_mipi->mipi_clocks_kbps, sdp_adap_mipi->mipi_clocks_size);
	if (ret < 0) {
		panel_err("of_property_read_u32_array fail %s\n", DT_NAME_MIPI_FREQ_LISTS);
		return -EINVAL;
	}

	cnt = snprintf(buf, SZ_128, "mipi frequency count: %d, ",
			sdp_adap_mipi->mipi_clocks_size);
	for (i = 0; i < sdp_adap_mipi->mipi_clocks_size; i++)
		cnt += snprintf(buf + cnt, SZ_128 - cnt, "mipi[%d]: %d, ",
				i, sdp_adap_mipi->mipi_clocks_kbps[i]);
	buf[cnt] = 0;
	panel_info("adaptive mipi: %s\n", buf);

	return 0;
}

static int parse_sdp_osc_freq(struct device_node *np,
		struct adaptive_mipi_v2_info *sdp_adap_mipi)
{
	int i;
	int ret, num_elems;
	size_t cnt = 0;
	char buf[SZ_128];

	num_elems = of_property_count_u32_elems(np, DT_NAME_OSC_FREQ_LISTS);
	if (num_elems < 0) {
		panel_err("of_property_count_u32_elems fail %s\n", DT_NAME_OSC_FREQ_LISTS);
		return -EINVAL;
	}
	sdp_adap_mipi->osc_clocks_size = num_elems;

	ret = of_property_read_u32_array(np, DT_NAME_OSC_FREQ_LISTS,
			sdp_adap_mipi->osc_clocks_khz, sdp_adap_mipi->osc_clocks_size);
	if (ret < 0) {
		panel_err("of_property_read_u32_array fail %s\n", DT_NAME_OSC_FREQ_LISTS);
		return -EINVAL;
	}
	cnt = snprintf(buf, SZ_128, "ddi osc count: %d, ", sdp_adap_mipi->osc_clocks_size);
	for (i = 0; i < sdp_adap_mipi->osc_clocks_size; i++)
		cnt += snprintf(buf + cnt, SZ_128 - cnt, "osc[%d]: %d, ",
				i, sdp_adap_mipi->osc_clocks_khz[i]);

	buf[cnt] = 0;
	panel_info("adaptive mipi: %s\n", buf);

	return 0;
}

#ifdef DEBUG_SDP_ADAPTIVE_MIPI
static int snprintf_mipi_rating_elem(char *buf, size_t size,
		struct adaptive_mipi_v2_table_element *elem)
{
	if (!buf || !size || !elem)
		return 0;

	return snprintf(buf, size, "rat:%3d, band:%3d, range:[%6d ~ %6d], rating:%3d %3d %3d",
			elem->rat, elem->band, elem->from_ch, elem->end_ch,
			elem->mipi_clocks_rating[0],
			elem->mipi_clocks_rating[1],
			elem->mipi_clocks_rating[2]);
}

static void dump_mipi_rating_elem_table(char *table_name,
		struct adaptive_mipi_v2_table_element *elems,
		int num_elems)
{
	char buf[SZ_128] = { 0 };
	int i;

	pr_info("[%s]\n", table_name);
	for (i = 0; i < num_elems; i++) {
		snprintf_mipi_rating_elem(buf, SZ_128, &elems[i]);
		pr_info("%3d: %s\n", i, buf);
	}
}

static int snprintf_osc_sel_elem(char *buf, size_t size,
		struct adaptive_mipi_v2_table_element *elem)
{
	if (!buf || !size || !elem)
		return 0;

	return snprintf(buf, size, "rat:%3d, band:%3d, range:[%6d ~ %6d], osc_idx:%d",
			elem->rat, elem->band, elem->from_ch, elem->end_ch, elem->osc_idx);
}

static void dump_osc_sel_elem_table(char *table_name,
		struct adaptive_mipi_v2_table_element *elems,
		int num_elems)
{
	char buf[SZ_128] = { 0 };
	int i;

	pr_info("[%s]\n", table_name);
	for (i = 0; i < num_elems; i++) {
		snprintf_osc_sel_elem(buf, SZ_128, &elems[i]);
		pr_info("%3d: %s\n", i, buf);
	}
}
#endif

__visible_for_testing int u32_array_to_mipi_rating_elem(
		struct adaptive_mipi_v2_table_element *elem,
		u32 *array, u32 num_mipi_clocks)
{
	int i;

	if (!elem || !array)
		return -EINVAL;

	if (num_mipi_clocks > MAX_MIPI_FREQ_CNT)
		return -EINVAL;

	elem->rat = array[0];
	elem->band = array[1];
	elem->from_ch = array[2];
	elem->end_ch = array[3];
	for (i = 0; i < num_mipi_clocks; i++)
		elem->mipi_clocks_rating[i] = array[4 + i];

	return 0;
}

__visible_for_testing int u32_array_to_osc_sel_elem(
		struct adaptive_mipi_v2_table_element *elem,
		u32 *array, u32 num_osc_indices)
{
	if (!elem || !array)
		return -EINVAL;

	if (num_osc_indices != 1)
		return -EINVAL;

	elem->rat = array[0];
	elem->band = array[1];
	elem->from_ch = array[2];
	elem->end_ch = array[3];
	elem->osc_idx = array[4];

	return 0;
}

static int parse_sdp_adap_mipi_table(
		struct device_node *np, char *name,
		unsigned int num_values,
		int (*u32_array_to_adap_elem)(struct adaptive_mipi_v2_table_element *, u32 *, u32),
		struct adaptive_mipi_v2_table_element **elems, int *num_elems)
{
	int i, ret, array_size, temp_num_elems;
	struct adaptive_mipi_v2_table_element *temp_elems = NULL;
	u32 *array = NULL, stride = 4 + num_values;

	array_size = of_property_count_u32_elems(np, name);
	if (array_size < 0) {
		panel_err("of_property_count_u32_elems fail %s\n", name);
		return -EINVAL;
	}

	if (array_size % stride) {
		panel_err("array size(%d) is not aligned by (%d)\n",
				array_size, stride);
		return -EINVAL;
	}

	array = kzalloc(array_size * sizeof(*array), GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, name, array, array_size);
	if (ret < 0) {
		panel_err("of_property_read_u32_array fail %s\n", name);
		ret = -EINVAL;
		goto err;
	}

	temp_num_elems = array_size / stride;
	temp_elems = kzalloc(temp_num_elems * sizeof(*temp_elems), GFP_KERNEL);
	if (!temp_elems) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < temp_num_elems; i++) {
		ret = u32_array_to_adap_elem(&temp_elems[i], &array[i * stride], num_values);
		if (ret < 0) {
			ret = -EINVAL;
			goto err;
		}
	}

	*elems = temp_elems;
	*num_elems = temp_num_elems;

	kfree(array);

	return 0;

err:
	kfree(array);
	kfree(temp_elems);

	return ret;
}

static int parse_sdp_adaptive_mipi_dt(struct device_node *np,
		struct adaptive_mipi_v2_info *sdp_adap_mipi)
{
	int i, ret;
	static char *dt_mipi_rating_table_names[MAX_BANDWIDTH_IDX] = {
		[BANDWIDTH_10M_IDX] = DT_NAME_MIPI_FREQ_SEL_10M,
		[BANDWIDTH_20M_IDX] = DT_NAME_MIPI_FREQ_SEL_20M,
	};

	if (!np || !sdp_adap_mipi) {
		panel_err("Invalid device node");
		return -EINVAL;
	}

	ret = parse_sdp_mipi_freq(np, sdp_adap_mipi);
	if (ret) {
		panel_err("failed to prase mipi frequence info\n");
		return ret;
	}

	for (i = 0; i < MAX_BANDWIDTH_IDX; i++) {
		if (!dt_mipi_rating_table_names[i])
			continue;

		parse_sdp_adap_mipi_table(np,
				dt_mipi_rating_table_names[i], sdp_adap_mipi->mipi_clocks_size,
				u32_array_to_mipi_rating_elem,
				&sdp_adap_mipi->mipi_table[i],
				&sdp_adap_mipi->mipi_table_size[i]);

#ifdef DEBUG_SDP_ADAPTIVE_MIPI
		dump_mipi_rating_elem_table(
				dt_mipi_rating_table_names[i],
				sdp_adap_mipi->mipi_table[i],
				sdp_adap_mipi->mipi_table_size[i]);
#endif
	}

	if (!of_find_property(np, DT_NAME_OSC_FREQ_LISTS, NULL))
		return 0;

	ret = parse_sdp_osc_freq(np, sdp_adap_mipi);
	if (ret) {
		panel_err("failed to prase mipi frequence info\n");
		return ret;
	}

	parse_sdp_adap_mipi_table(np,
			DT_NAME_OSC_FREQ_SEL, 1,
			u32_array_to_osc_sel_elem,
			&sdp_adap_mipi->osc_table,
			&sdp_adap_mipi->osc_table_size);

#ifdef DEBUG_SDP_ADAPTIVE_MIPI
	dump_osc_sel_elem_table(
			DT_NAME_OSC_FREQ_SEL,
			sdp_adap_mipi->osc_table,
			sdp_adap_mipi->osc_table_size);
#endif

	return 0;
}

static int sdp_adaptive_mipi_v2_apply_freq(int mipi_clk_kbps, int osc_clk_khz, void *ctx)
{
	struct freq_hop_param param = {
		.dsi_freq = mipi_clk_kbps,
		.osc_freq = osc_clk_khz,
	};
	int ret;

	ret = panel_set_freq_hop(ctx, &param);
	if (ret < 0) {
		panel_err("adaptive mipi: failed to set freq_hop\n");
		return ret;
	}

	return 0;
}

static struct adaptive_mipi_v2_adapter_funcs sdp_adaptive_mipi_v2_adapter_funcs = {
	.apply_freq = sdp_adaptive_mipi_v2_apply_freq,
};

int adaptive_mipi_v2_info_initialize(
		struct adaptive_mipi_v2_info *sdp_adap_mipi,
		struct panel_adaptive_mipi *adap_mipi)
{
	struct rf_band_element *elem;
	struct adaptive_mipi_v2_table_element *mipi_table;
	int i, j, count;

	memcpy(sdp_adap_mipi->mipi_clocks_kbps, adap_mipi->freq_info.mipi_lists,
			sizeof(sdp_adap_mipi->mipi_clocks_kbps));
	sdp_adap_mipi->mipi_clocks_size = adap_mipi->freq_info.mipi_cnt;
	memcpy(sdp_adap_mipi->osc_clocks_khz, adap_mipi->freq_info.osc_lists,
			sizeof(sdp_adap_mipi->osc_clocks_khz));
	sdp_adap_mipi->osc_clocks_size = adap_mipi->freq_info.osc_cnt;

	for (i = 0; i < MAX_BANDWIDTH_IDX; i++) {
		if (list_empty(&adap_mipi->rf_info_head[i]))
			continue;

		count = 0;
		list_for_each_entry(elem, &adap_mipi->rf_info_head[i], list)
			count++;
		mipi_table = kzalloc(count * sizeof(*mipi_table), GFP_KERNEL);

		j = 0;
		list_for_each_entry(elem, &adap_mipi->rf_info_head[i], list) {
			mipi_table[j].rat = 0;
			mipi_table[j].band = elem->band;
			mipi_table[j].from_ch = elem->channel_range[RF_CH_RANGE_FROM];
			mipi_table[j].end_ch = elem->channel_range[RF_CH_RANGE_TO];
			memcpy(mipi_table[j].mipi_clocks_rating,
					elem->rating, sizeof(mipi_table[j].mipi_clocks_rating));
#ifdef DEBUG_ADAPTIVE_MIPI
			panel_info("%d: band:%d ch:[%d~%d] rating:(%d %d %d)\n",
					j, mipi_table[j].band,
					mipi_table[j].from_ch, mipi_table[j].end_ch,
					mipi_table[j].mipi_clocks_rating[0],
					mipi_table[j].mipi_clocks_rating[1],
					mipi_table[j].mipi_clocks_rating[2]);
#endif
			j++;
		}
		sdp_adap_mipi->mipi_table[i] = mipi_table;
		sdp_adap_mipi->mipi_table_size[i] = count;
	}
	sdp_adap_mipi->osc_table = NULL;
	sdp_adap_mipi->osc_table_size = 0;

	sdp_adap_mipi->funcs = &sdp_adaptive_mipi_v2_adapter_funcs;

	return 0;
}

int probe_sdp_adaptive_mipi(struct panel_device *panel)
{
	struct adaptive_mipi_v2_info *sdp_adap_mipi;
	int ret;

	if (!panel) {
		panel_err("null panel\n");
		return -EINVAL;
	}

	if (panel->adap_mipi_node == NULL) {
		panel_err("null device tree node\n");
		return -EINVAL;
	}
	sdp_adap_mipi = &panel->sdp_adap_mipi;
	sdp_adap_mipi->ctx = panel;

	ret = parse_sdp_adaptive_mipi_dt(panel->adap_mipi_node, sdp_adap_mipi);
	if (ret < 0) {
		panel_err("failed to parse dt\n");
		return ret;
	}
	sdp_adap_mipi->funcs = &sdp_adaptive_mipi_v2_adapter_funcs;

	ret = sdp_init_adaptive_mipi_v2(sdp_adap_mipi);
	if (ret < 0) {
		panel_info("failed to init sdp adaptive mipi\n");
		return ret;
	}

	panel_info("sdp adaptive mipi: probe done\n");

	return 0;
}

int remove_sdp_adaptive_mipi(struct panel_device *panel)
{
	struct adaptive_mipi_v2_info *sdp_adap_mipi;
	int i;

	if (!panel) {
		panel_err("null panel\n");
		return -EINVAL;
	}
	sdp_adap_mipi = &panel->sdp_adap_mipi;
	sdp_cleanup_adaptive_mipi_v2(sdp_adap_mipi);

	for (i = 0; i < MAX_BANDWIDTH_IDX; i++) {
		kfree(sdp_adap_mipi->mipi_table[i]);
		sdp_adap_mipi->mipi_table[i] = NULL;
		sdp_adap_mipi->mipi_table_size[i] = 0;
	}
	kfree(sdp_adap_mipi->osc_table);
	sdp_adap_mipi->osc_table = NULL;
	sdp_adap_mipi->osc_table_size = 0;

	return 0;
}
