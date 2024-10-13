/* SPDX-License-Identifier: GPL-2.0-only
 * exynos_drm_writeback.h
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *	Wonyeong Choi <won0.choi@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _EXYNOS_DRM_WRTIEBACK_H_
#define _EXYNOS_DRM_WRTIEBACK_H_

#include <drm/drm_writeback.h>
#include <samsung_drm.h>
#include <exynos_drm_drv.h>

#include <linux/dma-buf.h>
#include <media/exynos_repeater.h>

#include <dpp_cal.h>

enum writeback_state {
	WB_STATE_OFF = 0,
	WB_STATE_ON,
	WB_STATE_HIBERNATION,
};

struct repeater_dma_buf {
	bool 				votf_o_feature_enabled;
	bool 				repeater_dev_opened;
	bool 				repeater_buf_active;
	struct shared_buffer_info 	repeater_shared_buf;
	uint32_t			repeater_valid_buf_idx;
	struct dma_buf_attachment	*attachment[MAX_SHARED_BUF_NUM];
	struct sg_table			*sg_table[MAX_SHARED_BUF_NUM];
	dma_addr_t			dma_addr[MAX_SHARED_BUF_NUM];
};

struct dpuf_votf;
struct writeback_device {
	struct device *dev;
	u32 id;
	u32 port;
	enum writeback_state state;
	unsigned long attr;
	int odma_irq;
	const uint32_t *pixel_formats;
	unsigned int num_pixel_formats;
	int decon_id;		/* connected DECON id */

	spinlock_t odma_slock;

	struct dpp_regs	regs;
	struct dpp_params_info win_config;
	struct dpp_restriction restriction;
	struct drm_writeback_connector writeback;

	enum exynos_drm_output_type output_type;

	struct {
		struct drm_property *standard;
		struct drm_property *range;
		struct drm_property *restriction;
		struct drm_property *use_repeater_buffer;
	} props;

	struct repeater_dma_buf rdb;
	bool protection;
	struct dpuf_votf *votf;
};

struct exynos_drm_writeback_state {
	struct drm_connector_state base;
	uint32_t blob_id_restriction;
	uint32_t standard;
	uint32_t range;
	uint32_t use_repeater_buffer;
	uint32_t repeater_valid_buf_idx;
	enum exynos_drm_writeback_type type;
};

#define to_wb_dev(wb_conn)		\
	container_of(wb_conn, struct writeback_device, writeback)
#define to_exynos_wb_state(state)	\
	container_of(state, struct exynos_drm_writeback_state, base)

#define conn_to_wb_conn(conn)		\
	container_of(conn, struct drm_writeback_connector, base)
#define conn_to_wb_dev(conn)	to_wb_dev(conn_to_wb_conn(conn))

#define enc_to_wb_conn(enc)		\
	container_of(enc, struct drm_writeback_connector, encoder)
#define enc_to_wb_dev(enc)	to_wb_dev(enc_to_wb_conn(enc))

static inline struct exynos_drm_crtc *
wb_get_exynos_crtc(const struct writeback_device *wb)
{
	const struct drm_connector_state *conn_state = wb->writeback.base.state;

	if (!conn_state || !conn_state->crtc)
		return NULL;

	return to_exynos_crtc(conn_state->crtc);
}

static inline const struct decon_device *
wb_get_decon(const struct writeback_device *wb)
{
	const struct drm_connector_state *conn_state = wb->writeback.base.state;

	if (!conn_state || !conn_state->crtc)
		return NULL;

	return to_exynos_crtc(conn_state->crtc)->ctx;
}

static inline bool wb_check_job(const struct drm_connector_state *conn_state)
{
	return (conn_state->writeback_job && conn_state->writeback_job->fb);
}

int exynos_drm_atomic_check_writeback(struct drm_device *dev,
		struct drm_atomic_state *state);
void wb_dump(struct writeback_device *wb);

void writeback_exit_hibernation(struct writeback_device *wb);
void writeback_enter_hibernation(struct writeback_device *wb);

#if IS_ENABLED(CONFIG_VIDEO_EXYNOS_REPEATER)
extern int repeater_request_buffer(struct shared_buffer_info *info, int owner);
extern int repeater_get_valid_buffer(int *buf_idx);
extern int repeater_set_valid_buffer(int buf_idx, int capture_idx);
#else
static inline int repeater_request_buffer(struct shared_buffer_info *info, int owner) { return 0; }
static inline int repeater_get_valid_buffer(int *buf_idx) { return 0; }
static inline int repeater_set_valid_buffer(int buf_idx, int capture_idx) { return 0; }
#endif
#endif
