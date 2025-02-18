/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ADAPTIVE_MIPI_H__
#define __ADAPTIVE_MIPI_H__

#include <dt-bindings/display/rf-band-id.h>

enum {
	RF_CH_RANGE_FROM = 0,
	RF_CH_RANGE_TO,
	MAX_RF_CH_RANGE,
};

#define MAX_MIPI_FREQ_CNT			3
#define MAX_OSC_FREQ_CNT			2

struct rf_band_element {
	u32 band;
	u32 channel_range[MAX_RF_CH_RANGE];
	u32 rating[MAX_MIPI_FREQ_CNT];
	u32 osc_freq;
	struct list_head list;
};


#define MAX_RF_BW_CNT				3
struct score_info {
	unsigned int rating[MAX_MIPI_FREQ_CNT];
};


struct adaptive_mipi_freq {
	unsigned int mipi_cnt;
	unsigned int mipi_lists[MAX_MIPI_FREQ_CNT];

	unsigned int osc_cnt;
	unsigned int osc_lists[MAX_OSC_FREQ_CNT];
};

/* ipc structure from cp */
#define MAX_BAND				16

#define STATUS_PRIMARY_SERVING			0x01
#define STATUS_SECONDARY_SERVING		0x02

#define SUB_CMD_ADAPTIVE_MIPI			0x05

struct __packed band_info {
	u8 rat;
	u32 band;
	u32 channel;
	u8 connection_status;
	u32 bandwidth; // bandwidth (kHz)
	s32 sinr;
	s32 rsrp;
	s32 rsrq;
	u8 cqi;
	u8 dl_mcs;
	s32 pusch_power;
};

struct __packed cp_info {
	u32 cell_count;
	struct band_info infos[MAX_BAND];
};

struct panel_adaptive_mipi {
	struct adaptive_mipi_freq freq_info;
	struct notifier_block radio_noti;
	struct list_head rf_info_head[MAX_RF_BW_CNT];
	struct cp_info debug_cp_info;
};

/* tunable value */
#define JUDGE_NARROW_BANDWIDTH			10000	/* 10000khz -> 10Mhz*/

#define WEIGHT_PRIMARY_SERVING			10
#define WEIGHT_SECONDARY_SERVING		1

#define JUDGE_STRONG_SIGNAL			20
#define WEIGHT_STRONG_SIGNAL			0
#define WEIGHT_WEAK_SIGNAL			1

#define DT_NAME_ADAPTIVE_MIPI_TABLE		"adaptive-mipi"
#define DT_NAME_ADAPTIVE_MIPI_RF_ID		"id"
#define DT_NAME_ADAPTIVE_MIPI_RF_RANGE		"range"
#define DT_NAME_ADAPTIVE_MIPI_RATING		"rating"
#define DT_NAME_ADAPTIVE_MIPI_OSC_FREQ		"osc_freq"

#define DT_NAME_MIPI_FREQ_LISTS			"mipi_freq_lists"
#define DT_NAME_OSC_FREQ_LISTS			"ddi_osc_freq_lists"
#define DT_NAME_RF_TABLE_LISTS			"rf_table_lists"


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


int probe_adaptive_mipi(struct panel_device *panel, struct rf_band_element *elems, unsigned int elements_nr);
int ril_notifier(struct notifier_block *self, unsigned long size, void *buf);

#endif /* __ADAPTIVE_MIPI_H__ */
