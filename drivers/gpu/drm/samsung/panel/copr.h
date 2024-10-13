/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __COPR_H__
#define __COPR_H__

#include "panel.h"
#include "timenval.h"
#include <linux/ktime.h>
#include <linux/wait.h>

#define CONFIG_SUPPORT_COPR_AVG

#define COPR_V1_SPI_RX_SIZE (8)
#define COPR_V1_DSI_RX_SIZE	(10)
#define COPR_V2_SPI_RX_SIZE (9)
#define COPR_V2_DSI_RX_SIZE	(12)
#define COPR_V3_SPI_RX_SIZE (41)
#define COPR_V3_DSI_RX_SIZE	(41)
#define COPR_V6_SPI_RX_SIZE (39)
#define COPR_V6_DSI_RX_SIZE	(39)

#define COPR_V5_CTRL_REG_SIZE (52)
#define COPR_V6_CTRL_REG_SIZE (75)

#define COPR_SET_SEQ ("copr_set_seq")
#define COPR_CLR_CNT_ON_SEQ ("copr_clr_cnt_on_seq")
#define COPR_CLR_CNT_OFF_SEQ ("copr_clr_cnt_off_seq")
#define COPR_SPI_GET_SEQ ("copr_spi_get_seq")
#define COPR_DSI_GET_SEQ ("copr_dsi_get_seq")

enum {
	COPR_MAPTBL,
	/* if necessary, add new maptbl */
	MAX_COPR_MAPTBL,
};

enum COPR_STATE {
	COPR_UNINITIALIZED,
	COPR_REG_ON,
	COPR_REG_OFF,
	MAX_COPR_REG_STATE,
};

enum COPR_VER {
	COPR_VER_0,
	COPR_VER_1,
	COPR_VER_2,
	COPR_VER_3,
	COPR_VER_5,
	COPR_VER_6,
	COPR_VER_0_1,			// watch
	MAX_COPR_VER,
};

struct copr_options {
	u8 thread_on;			/* copr_thread 0: off, 1: on */
	u8 check_avg;			/* check_avg when lcd on/off  0: not update avg, 1 : updateavg */
};

struct copr_reg_info {
	const char *name;
	u32 offset;
};

struct copr_ergb {
	u16 copr_er;
	u16 copr_eg;
	u16 copr_eb;
};

struct copr_roi{
	u32 roi_er;
	u32 roi_eg;
	u32 roi_eb;
	u32 roi_xs;
	u32 roi_ys;
	u32 roi_xe;
	u32 roi_ye;
};

struct copr_reg_v0 {
	u32 copr_gamma;		/* 0:GAMMA_1, 1:GAMMA_2.2 */
	u32 copr_en;
	u32 copr_er;
	u32 copr_eg;
	u32 copr_eb;
	u32 roi_on;
	u32 roi_xs;
	u32 roi_ys;
	u32 roi_xe;
	u32 roi_ye;
};

struct copr_reg_v1 {
	u32 cnt_re;
	u32 copr_gamma;		/* 0:GAMMA_1, 1:GAMMA_2.2 */
	u32 copr_en;
	u32 copr_er;
	u32 copr_eg;
	u32 copr_eb;
	u32 max_cnt;
	u32 roi_on;
	u32 roi_xs;
	u32 roi_ys;
	u32 roi_xe;
	u32 roi_ye;
};

struct copr_reg_v2 {
	u32 cnt_re;
	u32 copr_ilc;
	u32 copr_gamma;		/* 0:GAMMA_1, 1:GAMMA_2.2 */
	u32 copr_en;
	u32 copr_er;
	u32 copr_eg;
	u32 copr_eb;
	u32 copr_erc;
	u32 copr_egc;
	u32 copr_ebc;
	u32 max_cnt;
	u32 roi_on;
	u32 roi_xs;
	u32 roi_ys;
	u32 roi_xe;
	u32 roi_ye;
};

struct copr_reg_v3 {
	u32 copr_mask;
	u32 cnt_re;
	u32 copr_ilc;
	u32 copr_gamma;		/* 0:GAMMA_1, 1:GAMMA_2.2 */
	u32 copr_en;
	u32 copr_er;
	u32 copr_eg;
	u32 copr_eb;
	u32 copr_erc;
	u32 copr_egc;
	u32 copr_ebc;
	u32 max_cnt;
	u32 roi_on;
	struct copr_roi roi[6];
};

struct copr_reg_v5 {
	u32 copr_mask;
	u32 cnt_re;
	u32 copr_ilc;
	u32 copr_gamma;		/* 0:GAMMA_1, 1:GAMMA_2.2 */
	u32 copr_en;
	u32 copr_er;
	u32 copr_eg;
	u32 copr_eb;
	u32 copr_erc;
	u32 copr_egc;
	u32 copr_ebc;
	u32 max_cnt;
	u32 roi_on;
	struct copr_roi roi[5];
};

struct copr_reg_v6 {
	u32 copr_mask;
	u32 copr_pwr;
	u32 copr_en;
	u32 copr_gamma;		/* 0:GAMMA_1, 1:GAMMA_2.2 */
	u32 copr_frame_count;
	u32 roi_on;
	struct copr_roi roi[5];
};

struct copr_reg_v0_1 {
	u32 copr_en;
	u32 copr_pwr;
	u32 copr_mask;
	u32 copr_roi_ctrl;
	u32 copr_gamma_ctrl;
	struct copr_roi roi[2];
};

struct copr_reg {
	union {
		struct copr_reg_v0 v0;
		struct copr_reg_v1 v1;
		struct copr_reg_v2 v2;
		struct copr_reg_v3 v3;
		struct copr_reg_v5 v5;
		struct copr_reg_v6 v6;
		struct copr_reg_v0_1 v0_1;
	};
};

struct copr_properties {
	bool support;
	u32 version;
	u32 enable;
	u32 state;
	struct copr_reg reg;
	struct copr_options options;

	u32 copr_ready;
	u32 cur_cnt;
	u32 cur_copr;
	u32 avg_copr;
	u32 s_cur_cnt;
	u32 s_avg_copr;
	u32 comp_copr;
	u32 copr_roi_r[5][4];

	struct copr_roi roi[32];
	int nr_roi;
};

struct copr_wq {
	wait_queue_head_t wait;
	atomic_t count;
	struct task_struct *thread;
	bool should_stop;
};

struct copr_info {
	struct device *dev;
	struct class *class;
	struct panel_mutex lock;
	atomic_t stop;
	struct copr_wq wq;
	struct timenval res;
	struct copr_properties props;
	struct notifier_block fb_notif;
};

struct panel_copr_data {
	struct copr_reg reg;
	u32 version;
	struct copr_options options;
	struct seqinfo *seqtbl;
	u32 nr_seqtbl;
	struct maptbl *maptbl;
	u32 nr_maptbl;
	struct copr_roi roi[32];
	int nr_roi;
};

#ifdef CONFIG_USDM_PANEL_COPR
bool copr_is_enabled(struct copr_info *copr);
int copr_enable(struct copr_info *copr);
int copr_disable(struct copr_info *copr);
int copr_update_start(struct copr_info *copr, int count);
int copr_update_average(struct copr_info *copr);
int copr_get_value(struct copr_info *copr);
//int copr_get_average(struct copr_info *, int *, int *);
int copr_get_average_and_clear(struct copr_info *copr);
int copr_roi_set_value(struct copr_info *copr, struct copr_roi *roi, int size);
int copr_roi_get_value(struct copr_info *copr, struct copr_roi *roi, int size, u32 *out);
int copr_prepare(struct panel_device *panel, struct panel_copr_data *copr_data);
int copr_unprepare(struct panel_device *panel);
int copr_probe(struct panel_device *panel, struct panel_copr_data *copr_data);
int copr_remove(struct panel_device *panel);
int copr_res_update(struct copr_info *copr, int index, int cur_value, struct timespec cur_ts);
int get_copr_reg_copr_en(struct copr_info *copr);
int get_copr_reg_size(int version);
int get_copr_reg_packed_size(int version);
const char *get_copr_reg_name(int version, int index);
int get_copr_reg_offset(int version, int index);
u32 *get_copr_reg_ptr(struct copr_reg *reg, int version, int index);
int find_copr_reg_by_name(int version, char *s);
ssize_t copr_reg_show(struct copr_info *copr, char *buf);
int copr_reg_store(struct copr_info *copr, int index, u32 value);
int copr_reg_to_byte_array(struct copr_reg *reg, int version, unsigned char *byte_array);
#else
static inline bool copr_is_enabled(struct copr_info *copr) { return 0; }
static inline int copr_enable(struct copr_info *copr) { return 0; }
static inline int copr_disable(struct copr_info *copr) { return 0; }
static inline int copr_update_start(struct copr_info *copr, int count) { return 0; }
static inline int copr_update_average(struct copr_info *copr) { return 0; }
static inline int copr_get_value(struct copr_info *copr) { return 0; }
static inline int copr_get_average_and_clear(struct copr_info *copr) { return 0; }
static inline int copr_roi_set_value(struct copr_info *copr, struct copr_roi *roi, int size) { return 0; }
static inline int copr_roi_get_value(struct copr_info *copr, struct copr_roi *roi, int size, u32 *out) { return 0; }
static inline int copr_prepare(struct panel_device *panel, struct panel_copr_data *copr_data) { return 0; }
static inline int copr_unprepare(struct panel_device *panel) { return 0; }
static inline int copr_probe(struct panel_device *panel, struct panel_copr_data *copr_data) { return 0; }
static inline int copr_remove(struct panel_device *panel) { return 0; }
static inline int copr_res_update(struct copr_info *copr, int index, int cur_value, struct timespec cur_ts) { return 0; }
static inline int get_copr_reg_copr_en(struct copr_info *copr) { return 0; }
static inline int get_copr_reg_size(int version) { return 0; }
static inline int get_copr_reg_packed_size(int version) { return 0; }
static inline const char *get_copr_reg_name(int version, int index) { return NULL; }
static inline int get_copr_reg_offset(int version, int index) { return 0; }
static inline u32 *get_copr_reg_ptr(struct copr_reg *reg, int version, int index) { return NULL; }
static inline int find_copr_reg_by_name(int version, char *s) { return -EINVAL; }
static inline ssize_t copr_reg_show(struct copr_info *copr, char *buf) { return 0; }
static inline int copr_reg_store(struct copr_info *copr, int index, u32 value) { return 0; }
static inline int copr_reg_to_byte_array(struct copr_reg *reg, int version, unsigned char *byte_array) { return 0; }
#endif
#endif /* __COPR_H__ */
