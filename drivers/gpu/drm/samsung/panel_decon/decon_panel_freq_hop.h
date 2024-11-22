/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __PANEL_FREQ_HOP_H__
#define __PANEL_FREQ_HOP_H__

#include <dt-bindings/display/rf-band-id.h>

struct __packed ril_noti_info {
	u8 rat;
	u32 band;
	u32 channel;
};

enum {
	RADIO_CH_RANGE_FROM = 0,
	RADIO_CH_RANGE_TO,
	MAX_RADIO_CH_RANGE,
};

struct freq_hop_elem {
	u32 band;
	u32 channel_range[MAX_RADIO_CH_RANGE];
	u32 dsi_freq;
	u32 osc_freq;
	struct list_head list;
};

struct panel_freq_hop {
	struct notifier_block radio_noti;
	struct list_head head;
};

#define DT_NAME_FREQ_TABLE		"freq-hop"
#define DT_NAME_FREQ_HOP_RF_ID		"id"
#define DT_NAME_FREQ_HOP_RF_RANGE	"range"
#define DT_NAME_FREQ_HOP_HS_FREQ	"dsi_freq"
#define DT_NAME_FREQ_HOP_OSC_FREQ	"osc_freq"

#define DEFINE_FREQ_RANGE(_band, _from, _to, _dsi_freq, _osc_freq) { \
	.band = _band, \
	.channel_range[RADIO_CH_RANGE_FROM] = _from, \
	.channel_range[RADIO_CH_RANGE_TO] = _to, \
	.dsi_freq = _dsi_freq, \
	.osc_freq = _osc_freq }

int panel_freq_hop_probe(struct panel_device *panel,
		struct freq_hop_elem *elems, unsigned int size);
int snprintf_freq_hop_elem(char *buf, size_t size,
		struct freq_hop_elem *elem);
int radio_notifier(struct notifier_block *self,
		unsigned long size, void *buf);

#endif /* __PANEL_FREQ_HOP_H__ */
