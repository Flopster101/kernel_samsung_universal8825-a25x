/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __SDP_ADAPTIVE_MIPI_H__
#define __SDP_ADAPTIVE_MIPI_H__

#include <dt-bindings/display/rf-band-id.h>
#include <linux/sdp/adaptive_mipi_v2.h>
#include <linux/sdp/adaptive_mipi_v2_cp_info.h>
#include <linux/notifier.h>

enum {
	RF_CH_RANGE_FROM = 0,
	RF_CH_RANGE_TO,
	MAX_RF_CH_RANGE,
};

struct rf_band_element {
	u32 band;
	u32 channel_range[MAX_RF_CH_RANGE];
	u32 rating[MAX_MIPI_FREQ_CNT];
	u32 osc_freq;
	struct list_head list;
};

struct adaptive_mipi_freq {
	unsigned int mipi_cnt;
	unsigned int mipi_lists[MAX_MIPI_FREQ_CNT];

	unsigned int osc_cnt;
	unsigned int osc_lists[MAX_OSC_FREQ_CNT];
};

struct panel_adaptive_mipi {
	struct adaptive_mipi_freq freq_info;
	struct notifier_block radio_noti;
	struct list_head rf_info_head[MAX_BANDWIDTH_IDX];
	struct cp_info debug_cp_info;
};

#define SUB_CMD_ADAPTIVE_MIPI			0x05

#define DT_NAME_ADAPTIVE_MIPI_TABLE		"sdp-adaptive-mipi"
#define DT_NAME_MIPI_FREQ_LISTS			"ADAPTIVE_MIPI_CLOCKS"
#define DT_NAME_OSC_FREQ_LISTS			"ADAPTIVE_OSC_CLOCKS"
#define DT_NAME_MIPI_FREQ_SEL_10M		"ADAPTIVE_MIPI_V2_CLK_SEL_TABLE_10M"
#define DT_NAME_MIPI_FREQ_SEL_20M		"ADAPTIVE_MIPI_V2_CLK_SEL_TABLE_20M"
#define DT_NAME_OSC_FREQ_SEL			"ADAPTIVE_MIPI_V2_CLK_SEL_TABLE_OSC"

#define DEFINE_AD_TABLE_RATING3(_band, _from, _to, _rating0, _rating1, _rating2, _osc_freq) { \
	.band = _band, \
	.channel_range[RF_CH_RANGE_FROM] = _from, \
	.channel_range[RF_CH_RANGE_TO] = _to, \
	.rating[0] = _rating0, \
	.rating[1] = _rating1, \
	.rating[2] = _rating2,\
	.osc_freq = _osc_freq }

#define DEFINE_BAND_INFO(_rat, _band, _channel, _conn_status, _bandwidth, _sinr, _rsrp, _rsrq, _cqi, _dl_mcs, _pusch_power) { \
	.rat = _rat, \
	.band = _band, \
	.channel = _channel, \
	.connection_status = _conn_status, \
	.bandwidth = _bandwidth, \
	.sinr = _sinr, \
	.rsrp = _rsrp, \
	.rsrq = _rsrq, \
	.cqi = _cqi, \
	.dl_mcs = _dl_mcs, \
	.pusch_power = _pusch_power }

#define DEFINE_TEST_BAND(_band, _channel, _conn_status, _bandwidth, _sinr) { \
	.rat = 0, \
	.band = _band, \
	.channel = _channel, \
	.connection_status = _conn_status, \
	.bandwidth = _bandwidth, \
	.sinr = _sinr, \
	.rsrp = 0, \
	.rsrq = 0, \
	.cqi = 0, \
	.dl_mcs = 0, \
	.pusch_power = 0 }

int adaptive_mipi_v2_info_initialize(
		struct adaptive_mipi_v2_info *sdp_adap_mipi,
		struct panel_adaptive_mipi *adap_mipi);
int probe_sdp_adaptive_mipi(struct panel_device *panel);
int remove_sdp_adaptive_mipi(struct panel_device *panel);

#endif /* __SDP_ADAPTIVE_MIPI_H__ */
