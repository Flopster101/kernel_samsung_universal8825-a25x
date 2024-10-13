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
#include "panel_freq_hop.h"
#include "dev_ril_header.h"

static int of_get_freq_hop_elem(struct device_node *np, struct freq_hop_elem *elem)
{
	int ret;

	ret = of_property_read_u32(np, DT_NAME_FREQ_HOP_RF_ID,
			&elem->band);
	if (ret) {
		pr_err("%s: failed to get rf band id\n", __func__);
		return ret;
	}

	ret = of_property_read_u32_array(np, DT_NAME_FREQ_HOP_RF_RANGE,
			elem->channel_range, MAX_RADIO_CH_RANGE);
	if (ret) {
		pr_err("%s: failed to get rf channel range(ret:%d)\n", __func__, ret);
		return ret;
	}

	ret = of_property_read_u32(np, DT_NAME_FREQ_HOP_HS_FREQ,
			&elem->dsi_freq);
	if (ret) {
		pr_err("%s: failed to get dsi frequency\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, DT_NAME_FREQ_HOP_OSC_FREQ,
			&elem->osc_freq);
	if (ret) {
		pr_err("%s: failed to get osc frequency\n", __func__);
		return ret;
	}

	return ret;
}

static int get_freq_hop_elem(struct device_node *np, struct panel_freq_hop *freq_hop)
{
	int ret;
	struct freq_hop_elem *elem;

	if (!np || !freq_hop) {
		pr_err("FREQ_HOP: ERR:%s: Invalid device node\n", __func__);
		return -EINVAL;
	}

	elem = kzalloc(sizeof(*elem), GFP_KERNEL);
	if (!elem) {
		pr_err("FREQ_HOP: ERR:%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	ret = of_get_freq_hop_elem(np, elem);
	if (ret < 0) {
		pr_err("FREQ_HOP: ERR:%s: failed to get freq_hop_elem\n", __func__);
		goto err;
	}

	list_add_tail(&elem->list, &freq_hop->head);

	return 0;
err:
	kfree(elem);

	return ret;
}

int snprintf_freq_hop_elem(char *buf, size_t size, struct freq_hop_elem *elem)
{
	if (!buf || !size || !elem)
		return 0;

	return snprintf(buf, size, "band:%3d, channel_range:[%6d ~ %6d], dsi:%d kHz, osc:%d kHz",
				elem->band, elem->channel_range[RADIO_CH_RANGE_FROM],
				elem->channel_range[RADIO_CH_RANGE_TO],
				elem->dsi_freq, elem->osc_freq);
}

#ifdef FREQ_HOP_DEBUG
static void dump_freq_hop_elem(struct freq_hop_elem *elem)
{
	char buf[SZ_128] = { 0 };

	snprintf_freq_hop_elem(buf, SZ_128, elem);
	pr_info("FREQ_HOP: %s\n", buf);
}

static void dump_freq_hop_list(struct panel_freq_hop *freq_hop)
{
	struct freq_hop_elem *elem;

	list_for_each_entry(elem, &freq_hop->head, list)
		dump_freq_hop_elem(elem);
}
#endif

static int freq_hop_parse_dt(struct device_node *np, struct panel_freq_hop *freq_hop)
{
	struct device_node *entry;
	int num_elems = 0;
	int ret;

	if (!np || !freq_hop) {
		pr_err("FREQ_HOP: ERR:%s: Invalid device node\n", __func__);
		return -EINVAL;
	}

	for_each_child_of_node(np, entry) {
		ret = get_freq_hop_elem(entry, freq_hop);
		if (ret) {
			pr_err("FREQ_HOP: ERR:%s: failed to get hop_freq\n", __func__);
			return -EINVAL;
		}
		num_elems++;
	}

	pr_info("FREQ_HOP: %s: freq_hop rf band size: %d\n", __func__, num_elems);

#ifdef FREQ_HOP_DEBUG
	dump_freq_hop_list(freq_hop);
#endif
	return 0;
}

static bool is_valid_msg_info(struct dev_ril_bridge_msg *msg)
{
	if (!msg) {
		pr_err("FREQ_HOP: ERR:%s: invalid message\n", __func__);
		return false;
	}

	if (msg->dev_id != IPC_SYSTEM_CP_CHANNEL_INFO) {
		pr_err("FREQ_HOP: ERR:%s: invalid channel info: %d\n", __func__, msg->dev_id);
		return false;
	}

	if (msg->data_len != sizeof(struct ril_noti_info)) {
		pr_err("FREQ_HOP: ERR:%s: invalid data len: %d\n", __func__, msg->data_len);
		return false;
	}

	return true;
}

__visible_for_testing struct freq_hop_elem *
search_freq_hop_element(struct panel_freq_hop *freq_hop,
		unsigned int band, unsigned int channel)
{
	struct freq_hop_elem *elem;
	int gap1, gap2;

	if (!freq_hop) {
		pr_err("FREQ_HOP: ERR:%s: Invalid freq_hop info\n", __func__);
		return NULL;
	}

	pr_debug("FREQ_HOP: %s: (band:%d, channel:%d)\n",
			__func__, band, channel);
	if (band >= MAX_BAND_ID) {
		pr_err("FREQ_HOP: ERR:%s: invalid band:%d\n",
				__func__, band);
		return NULL;
	}

	list_for_each_entry(elem, &freq_hop->head, list) {
		if (elem->band != band)
			continue;

		gap1 = (int)channel - (int)elem->channel_range[RADIO_CH_RANGE_FROM];
		gap2 = (int)channel - (int)elem->channel_range[RADIO_CH_RANGE_TO];

		if ((gap1 >= 0) && (gap2 <= 0)) {
			pr_info("FREQ_HOP: found (band:%d, channel_range:[%d~%d], dsi:%d kHz, osc:%d kHz)\n",
					elem->band, elem->channel_range[RADIO_CH_RANGE_FROM],
					elem->channel_range[RADIO_CH_RANGE_TO],
					elem->dsi_freq, elem->osc_freq);
			return elem;
		}
	}

	pr_err("FREQ_HOP: ERR:%s: not found (band:%d, channel:%d)\n",
			__func__, band, channel);

	return NULL;
}

int radio_notifier(struct notifier_block *self, unsigned long size, void *buf)
{
	struct freq_hop_elem *elem;
	struct panel_freq_hop *freq_hop =
		container_of(self, struct panel_freq_hop, radio_noti);
	struct ril_noti_info *rf_info;
	struct dev_ril_bridge_msg *msg = buf;
	struct freq_hop_param param;
	int ret;

	if (!is_valid_msg_info(msg)) {
		pr_err("FREQ_HOP: ERR:%s: Invalid ril msg\n", __func__);
		goto exit_notifier;
	}
	rf_info = msg->data;

	elem = search_freq_hop_element(freq_hop, rf_info->band, rf_info->channel);
	if (!elem) {
		pr_err("FREQ_HOP: ERR:%s: failed to search freq_hop_elem\n", __func__);
		goto exit_notifier;
	}

	param.dsi_freq = elem->dsi_freq;
	param.osc_freq = elem->osc_freq;

	ret = panel_set_freq_hop(to_panel_device(freq_hop), &param);
	if (ret < 0) {
		pr_err("FREQ_HOP: ERR:%s: failed to set freq_hop\n", __func__);
		goto exit_notifier;
	}

exit_notifier:
	return 0;
}

int panel_freq_hop_probe(struct panel_device *panel,
		struct freq_hop_elem *elems, unsigned int size)
{
	int ret;
	struct panel_freq_hop *freq_hop;

	if (!panel) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	freq_hop = &panel->freq_hop;

	INIT_LIST_HEAD(&freq_hop->head);
	if (!elems) {
		ret = freq_hop_parse_dt(panel->freq_hop_node, freq_hop);
		if (ret < 0) {
			pr_err("FREQ_HOP: ERR:%s: failed to parse freq_hop dt\n", __func__);
			return ret;
		}
	} else {
		int i;

		for (i = 0; i < size; i++)
			list_add_tail(&elems[i].list, &freq_hop->head);
	}

	freq_hop->radio_noti.notifier_call = radio_notifier;
	ret = register_dev_ril_bridge_event_notifier(&freq_hop->radio_noti);
	if (ret < 0) {
		pr_err("FREQ_HOP: ERR:%s: failed to register ril notifier\n", __func__);
		return ret;
	}

	pr_info("FREQ_HOP: %s: done\n", __func__);

	return 0;
}
