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
#include "adaptive_mipi.h"
#include "dev_ril_header.h"

static int of_get_rf_band_element(struct device_node *np, struct rf_band_element *elem, unsigned int mipi_cnt)
{
	int ret;

	ret = of_property_read_u32(np, DT_NAME_ADAPTIVE_MIPI_RF_ID,
			&elem->band);
	if (ret) {
		panel_err("adaptive mipi: failed to get rf band id\n");
		return ret;
	}

	ret = of_property_read_u32_array(np, DT_NAME_ADAPTIVE_MIPI_RF_RANGE,
			elem->channel_range, MAX_RF_CH_RANGE);
	if (ret) {
		panel_err("adaptive mipi: failed to get rf channel range(ret:%d)\n", ret);
		return ret;
	}

	ret = of_property_read_u32_array(np, DT_NAME_ADAPTIVE_MIPI_RATING, elem->rating, mipi_cnt);
	if (ret) {
		panel_err("adaptive mipi: failed to get rating\n");
		return ret;
	}

	if (of_property_read_u32(np, DT_NAME_ADAPTIVE_MIPI_OSC_FREQ, &elem->osc_freq))
		elem->osc_freq = 0;

	return 0;
}

static int parse_rf_band_elements(struct device_node *np, struct panel_adaptive_mipi *adap_mipi, unsigned int idx)
{
	int ret;
	struct rf_band_element *elem;
	unsigned int mipi_cnt;

	if (!np || !adap_mipi) {
		panel_err("adaptive mipi: Invalid device node\n");
		return -EINVAL;
	}

	elem = kzalloc(sizeof(struct rf_band_element), GFP_KERNEL);
	if (!elem) {
		panel_err("adaptive mipi: failed to alloc mem\n");
		return -ENOMEM;
	}

	mipi_cnt = adap_mipi->freq_info.mipi_cnt;

	ret = of_get_rf_band_element(np, elem, mipi_cnt);
	if (ret < 0) {
		panel_err("adaptive mipi: failed to get band elements\n");
		goto err;
	}

	list_add_tail(&elem->list, &adap_mipi->rf_info_head[idx]);

	return 0;
err:
	kfree(elem);

	return ret;
}


#ifdef DEBUG_ADAPTIVE_MIPI
int snprintf_rf_band_elem(char *buf, size_t size, struct rf_band_element *elem)
{
	if (!buf || !size || !elem)
		return 0;

	return snprintf(buf, size, "band:%3d, range:[%6d ~ %6d], rating:%3d %3d %3d, osc:%d",
				elem->band,
				elem->channel_range[RF_CH_RANGE_FROM], elem->channel_range[RF_CH_RANGE_TO],
				elem->rating[0], elem->rating[1], elem->rating[2],
				elem->osc_freq);
}

static void dump_rf_band_elem(struct rf_band_element *elem)
{
	char buf[SZ_128] = { 0 };

	snprintf_rf_band_elem(buf, SZ_128, elem);
	pr_info("ADAPTIVE MIPI: %s\n", buf);
}

static void dump_rf_element_list(struct panel_adaptive_mipi *adap_mipi)
{
	int i;
	struct rf_band_element *elem;

	for (i = 0; i < MAX_RF_BW_CNT; i++) {
		list_for_each_entry(elem, &adap_mipi->rf_info_head[i], list)
			dump_rf_band_elem(elem);

	}
}
#endif

static int parse_mipi_freq(struct device_node *np, struct adaptive_mipi_freq *freq)
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
	freq->mipi_cnt = num_elems;

	ret = of_property_read_u32_array(np, DT_NAME_MIPI_FREQ_LISTS, freq->mipi_lists, freq->mipi_cnt);
	if (ret < 0) {
		panel_err("of_property_read_u32_array fail %s\n", DT_NAME_MIPI_FREQ_LISTS);
		return -EINVAL;
	}

	cnt = snprintf(buf, SZ_128, "mipi frequency count: %d, ", freq->mipi_cnt);
	for (i = 0; i < freq->mipi_cnt; i++)
		cnt += snprintf(buf + cnt, SZ_128 - cnt, "mipi[%d]: %d, ", i, freq->mipi_lists[i]);
	buf[cnt] = 0;
	panel_info("adaptive mipi: %s\n", buf);

	num_elems = of_property_count_u32_elems(np, DT_NAME_OSC_FREQ_LISTS);
	if (num_elems < 0) {
		panel_err("of_property_count_u32_elems fail %s\n", DT_NAME_OSC_FREQ_LISTS);
		return -EINVAL;
	}
	freq->osc_cnt = num_elems;

	ret = of_property_read_u32_array(np, DT_NAME_OSC_FREQ_LISTS, freq->osc_lists, freq->osc_cnt);
	if (ret < 0) {
		panel_err("of_property_read_u32_array fail %s\n", DT_NAME_OSC_FREQ_LISTS);
		return -EINVAL;
	}
	cnt = snprintf(buf, SZ_128, "ddi osc count: %d, ", freq->osc_cnt);
	for (i = 0; i < freq->osc_cnt; i++)
		cnt += snprintf(buf + cnt, SZ_128 - cnt, "osc[%d]: %d, ", i, freq->osc_lists[i]);

	buf[cnt] = 0;
	panel_info("adaptive mipi: %s\n", buf);

	return 0;
}

static int parse_dt(struct device_node *np, struct panel_adaptive_mipi *adap_mipi)
{
	int ret, i;
	struct device_node *entry;
	int element_cnt = 0;
	int table_cnt;
	struct device_node *table_np;
	struct adaptive_mipi_freq *freq;

	if (!np || !adap_mipi) {
		panel_err("Invalid device node");
		return -EINVAL;
	}

	freq = &adap_mipi->freq_info;

	ret = parse_mipi_freq(np, freq);
	if (ret) {
		panel_err("failed to prase mipi frequence info\n");
		return ret;
	}

	table_cnt = of_property_count_u32_elems(np, DT_NAME_RF_TABLE_LISTS);
	if (table_cnt < 0) {
		panel_err("of_property_count_u32_elems fail %s\n", DT_NAME_RF_TABLE_LISTS);
		return -EINVAL;
	}
	panel_info("adaptive mipi: rf table count: %d\n", table_cnt);

	for (i = 0; i < table_cnt; i++) {
		table_np = of_parse_phandle(np, DT_NAME_RF_TABLE_LISTS, i);
		if (!table_np) {
			panel_err("failed to get phandle of %s\n", DT_NAME_RF_TABLE_LISTS);
			return -EINVAL;
		}
		for_each_child_of_node(table_np, entry) {
			ret = parse_rf_band_elements(entry, adap_mipi, i);
			if (ret) {
				of_node_put(table_np);
				panel_err("failed to get rf element\n");
				return -EINVAL;
			}
			element_cnt++;
		}
		of_node_put(table_np);
	}

	panel_info("adaptive mipi: total rf element count: %d\n", element_cnt);

#ifdef DEBUG_ADAPTIVE_MIPI
	dump_rf_element_list(adap_mipi);
#endif

	return 0;
}


static bool check_ril_msg(struct dev_ril_bridge_msg *msg)
{
	if (!msg) {
		panel_err("adaptive mipi: null msg\n");
		return false;
	}

	if (msg->dev_id != SUB_CMD_ADAPTIVE_MIPI) {
		panel_dbg("adaptive mipi: invalid sub cmd: %d\n", msg->dev_id);
		return false;
	}

	return true;
}


static int get_rftbl_idx(u32 bandwidth)
{
	if (bandwidth <= JUDGE_NARROW_BANDWIDTH)
		return 0;
	return 1;
}

static bool check_band_msg(struct band_info *band_msg)
{
	if (band_msg == NULL) {
		panel_err("adaptive mipi: null band msg\n");
		return false;
	}

	if (band_msg->band >= MAX_BAND_ID) {
		panel_err("adaptive mipi: invalid band:%d\n", band_msg->band);
		return false;
	}

	if (get_rftbl_idx(band_msg->bandwidth) >= MAX_RF_BW_CNT) {
		panel_err("adaptive mipi: invalid rf table idx: %d\n", get_rftbl_idx(band_msg->bandwidth));
		return false;
	}

	return true;
}


__visible_for_testing struct rf_band_element *
search_rf_band_element(struct panel_adaptive_mipi *adap_mipi, struct band_info *band)
{
	int gap1, gap2;
	struct rf_band_element *elem;

	if (!adap_mipi) {
		panel_err("adaptive mipi: null adap mipi\n");
		return NULL;
	}

	if (!check_band_msg(band)) {
		panel_err("adaptive mipi: invalid band msg\n");
		return NULL;
	}

	panel_info("adaptive mipi: (band:%d, channel:%d, bw: %d, rf table idx: %d)\n",
		band->band, band->channel, band->bandwidth, get_rftbl_idx(band->bandwidth));

	list_for_each_entry(elem, &adap_mipi->rf_info_head[get_rftbl_idx(band->bandwidth)], list) {
		if (elem->band != band->band)
			continue;

		gap1 = (int)band->channel - (int)elem->channel_range[RF_CH_RANGE_FROM];
		gap2 = (int)band->channel - (int)elem->channel_range[RF_CH_RANGE_TO];

		if ((gap1 >= 0) && (gap2 <= 0)) {
			panel_info("adaptive mipi: found (band:%d, channel_range:[%d~%d])\n",
					elem->band, elem->channel_range[RF_CH_RANGE_FROM],
					elem->channel_range[RF_CH_RANGE_TO]);
			return elem;
		}
	}

	panel_err("adaptive mipi not found rf band:%d, channel:%d)\n",
		band->band, band->channel);

	return NULL;
}


static void dump_band_info(struct band_info *band_msg)
{
	size_t len;
	char buf[SZ_512];

	if (band_msg == NULL)
		return;

	len = snprintf(buf, SZ_512, "band:%d, channel:%d, status:%d, bw:%d, sinr:%d",
		band_msg->band, band_msg->channel, band_msg->connection_status,
		band_msg->bandwidth, band_msg->sinr);

	buf[len] = 0;
	pr_info("adaptive mipi: %s\n", buf);
}

static void dump_cp_msg(struct cp_info *cp_msg)
{
	int i;

	for (i = 0; i < cp_msg->cell_count; i++)
		dump_band_info(&cp_msg->infos[i]);
}

static void dump_score_info(struct score_info *score, unsigned int cnt)
{
	int i;
	size_t len = 0;
	char buf[SZ_128];

	for (i = 0; i < cnt; i++)
		len += snprintf(buf + len, SZ_128 - len, "rating[%2d]: %3d, ", i, score->rating[i]);

	buf[len] = 0;
	pr_info("adaptive mipi: %s\n", buf);
}


static bool check_strong_signal(int sinr)
{
	if (sinr >= JUDGE_STRONG_SIGNAL)
		return true;

	return false;
}


__visible_for_testing int
get_optimal_frequency(struct panel_adaptive_mipi *adap_mipi, struct cp_info *cp_msg, struct freq_hop_param *param)
{
	int i, j;
	struct band_info *band_msg;
	struct rf_band_element *elem;
	struct score_info score[MAX_BAND + 1];

	unsigned int status_weight, signal_weight;
	unsigned int min_score = 1000, min_idx = 0;

	memset(score, 0, sizeof(score));

	for (i = 0 ; i < cp_msg->cell_count; i++) {
		band_msg = &cp_msg->infos[i];
		elem = search_rf_band_element(adap_mipi, band_msg);
		if (!elem)
			continue;

		for (j = 0; j < adap_mipi->freq_info.mipi_cnt; j++) {
			status_weight = WEIGHT_SECONDARY_SERVING;
			if (band_msg->connection_status == STATUS_PRIMARY_SERVING)
				status_weight = WEIGHT_PRIMARY_SERVING;

			signal_weight = WEIGHT_WEAK_SIGNAL;
			if (check_strong_signal(band_msg->sinr))
				signal_weight = WEIGHT_STRONG_SIGNAL;

			score[i].rating[j] = elem->rating[j] * status_weight * signal_weight;
			score[MAX_BAND].rating[j] += score[i].rating[j];

		}
	}

	/* dump score for each band */
	for (i = 0; i < cp_msg->cell_count; i++)
		dump_score_info(&score[i], adap_mipi->freq_info.mipi_cnt);

	/* dump calculated score */
	dump_score_info(&score[MAX_BAND], adap_mipi->freq_info.mipi_cnt);

	for (i = 0; i < adap_mipi->freq_info.mipi_cnt; i++) {
		if (min_score > score[MAX_BAND].rating[i]) {
			min_score = score[MAX_BAND].rating[i];
			min_idx = i;
		}
	}

	param->dsi_freq = adap_mipi->freq_info.mipi_lists[min_idx] * 1000;
	param->osc_freq = 0;

	panel_info("adaptive mipi: optimal mipi freq: %d:%d\n",
		min_idx, adap_mipi->freq_info.mipi_lists[min_idx]);

	return 0;

}


static int copy_cp_msg(struct cp_info *dst_msg, struct cp_info *src_msg)
{
	int i;

	memcpy(&dst_msg->cell_count, &src_msg->cell_count, sizeof(dst_msg->cell_count));

	for (i = 0; i < dst_msg->cell_count; i++)
		memcpy(&dst_msg->infos[i], &src_msg->infos[i], sizeof(struct band_info));

	dump_cp_msg(dst_msg);

	return 0;
}

//#define SIMULATE_MULTI_BAND

#ifdef SIMULATE_MULTI_BAND
struct cp_info testing_cp_msg = {
	.infos[0] = DEFINE_BAND_INFO(3, 93, 1850, 1, 100000, 7, -120, -11, 10, 5, 1),
	.infos[1] = DEFINE_BAND_INFO(3, 108, 5860, 2, 100000, 0, 0, 0, 0, 0, 0),
	.infos[2] = DEFINE_BAND_INFO(4, 132, 42500, 1, 100000, 30, -60, -10, 5, 10, 0),
	.infos[3] = DEFINE_BAND_INFO(5, 138, 55450, 2, 100000, 10, -60, -10, 5, 10, 0),
	.infos[4] = DEFINE_BAND_INFO(3, 138, 55582, 2, 100000, 10, -60, -10, 5, 10, 0),
	.infos[5] = DEFINE_BAND_INFO(3, 138, 56191, 2, 100000, 10, -60, -10, 5, 10, 0),
	.cell_count = 6,
};
#endif

int ril_notifier(struct notifier_block *self, unsigned long size, void *buf)
{
	int ret;
	struct cp_info cp_msg, *msg;
	struct freq_hop_param param;
	struct dev_ril_bridge_msg *ril_msg = buf;
	struct panel_adaptive_mipi *adap_mipi =
		container_of(self, struct panel_adaptive_mipi, radio_noti);

	if (self == NULL) {
		panel_err("null self\n");
		goto exit_notifier;
	}

	if (ril_msg == NULL) {
		panel_err("null ril message\n");
		goto exit_notifier;
	}

	if (!check_ril_msg(ril_msg)) {
		panel_dbg("adaptive mipi: invalid ril message\n");
		goto exit_notifier;
	}
	msg = (struct cp_info *)ril_msg->data;

#ifdef SIMULATE_MULTI_BAND
	msg = &testing_cp_msg;
#endif
	ret = copy_cp_msg(&cp_msg, msg);
	if (ret) {
		panel_err("adaptive mipi: failed to copy message\n");
		goto exit_notifier;
	}

	ret = get_optimal_frequency(adap_mipi, &cp_msg, &param);
	if (ret) {
		panel_err("adaptive mipi: failed to get optimal frequency\n");
		goto exit_notifier;
	}

	ret = panel_set_freq_hop(to_panel_device(adap_mipi), &param);
	if (ret < 0) {
		panel_err("adaptive mipi: failed to set freq_hop\n");
		goto exit_notifier;
	}

exit_notifier:
	return 0;
}



int probe_adaptive_mipi(struct panel_device *panel, struct rf_band_element *elems, unsigned int elements_nr)
{
	int i;
	int ret;
	struct panel_adaptive_mipi *adap_mipi;

	if (!panel) {
		panel_err("null panel\n");
		return -EINVAL;
	}

	if (panel->adap_mipi_node == NULL) {
		panel_err("null device tree node\n");
		return -EINVAL;
	}
	adap_mipi = &panel->adap_mipi;

	for (i = 0; i < MAX_RF_BW_CNT; i++)
		INIT_LIST_HEAD(&adap_mipi->rf_info_head[i]);

	if (!elems) {
		ret = parse_dt(panel->adap_mipi_node, adap_mipi);
		if (ret < 0) {
			panel_err("failed to parse dt\n");
			return ret;
		}
	}

/* Todo: Add codes to unit test */
	adap_mipi->radio_noti.notifier_call = ril_notifier;
	ret = register_dev_ril_bridge_event_notifier(&adap_mipi->radio_noti);
	if (ret < 0) {
		panel_err("failed to register ril notifier\n");
		return ret;
	}

	panel_info("adaptive mipi: probe done\n");

	return 0;
}
