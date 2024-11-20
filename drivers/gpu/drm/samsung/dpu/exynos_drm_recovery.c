// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_recovery.c
 *
 * Copyright (C) 2021 Samsung Electronics Co.Ltd
 * Authors:
 *	Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_vblank.h>
#include "exynos_drm_decon.h"
#include "exynos_drm_recovery.h"
#include "exynos_drm_debug.h"

static int dpu_recovery_log_level = 6;
module_param(dpu_recovery_log_level, int, 0600);
MODULE_PARM_DESC(dpu_recovery_log_level, "log level for dpu recovery [default : 6]");

#define RECOVERY_NAME "RECOVERY"
#define recov_info(fmt, ...)	\
	dpu_pr_info(RECOVERY_NAME, 0, dpu_recovery_log_level, fmt, ##__VA_ARGS__)

#define recov_warn(fmt, ...)	\
	dpu_pr_warn(RECOVERY_NAME, 0, dpu_recovery_log_level, fmt, ##__VA_ARGS__)

#define recov_err(fmt, ...)	\
	dpu_pr_err(RECOVERY_NAME, 0, dpu_recovery_log_level, fmt, ##__VA_ARGS__)

#define recov_debug(fmt, ...)	\
	dpu_pr_debug(RECOVERY_NAME, 0, dpu_recovery_log_level, fmt, ##__VA_ARGS__)
#define RECOVERY_RETRY_CNT 5
//#define ENABLE_RECOVERY_DUMP

struct recovery_mapping {
	char name[RECOVERY_NAME_LEN];
	bool (*func)(const struct drm_crtc *crtc);
};

static int __set_panel_reset(struct decon_device *decon, bool reset);

__weak bool dsim_condition_check(const struct drm_crtc *crtc)
{
	return false;
}

__weak bool customer_condition_check(const struct drm_crtc *crtc)
{
	return false;
}

__weak bool skip_condition_check(const struct drm_crtc *crtc)
{
	return true;
}

struct recovery_mapping recovery_map[RECOVERY_MODE_MAX] = {
	/* order : priority */
	{"dsim", dsim_condition_check},
	{"customer", customer_condition_check},

	/* new function sholud be added before force_condition */
	{"force", skip_condition_check},
	{"always", skip_condition_check},
};

const char recovery_state_string[RECOVERY_STATE_NUM][RECOVERY_NAME_LEN] = {
	"not_supported", "idle", "trigger",
	"begin", "restore", "max_error"
};

static bool __rq_is_empty(struct recovery_queue *rq)
{
	return ((rq->front == rq->rear) ? true : false);
}

static bool __rq_is_full(struct recovery_queue *rq)
{
	return ((rq->front == ((rq->rear + 1) % rq->fsz)) ? true : false);
}

static ktime_t __rq_top(struct recovery_queue *rq)
{
	return __rq_is_empty(rq) ? ms_to_ktime(0) : rq->queue[rq->front];
}

static void __rq_delq(struct recovery_queue *rq)
{
	if (!__rq_is_empty(rq))
		rq->front = (rq->front + 1) % rq->fsz;
}

static void __rq_addq(struct recovery_queue *rq, ktime_t data)
{
	if (__rq_is_full(rq))
		__rq_delq(rq);

	rq->queue[rq->rear] = data;
	rq->rear = (rq->rear + 1) % rq->fsz;
}

static int __set_panel_reset(struct decon_device *decon, bool reset)
{
	struct drm_connector *conn;
	struct exynos_drm_connector_state *exynos_conn_state;

	if (!decon)
		return -EINVAL;

	conn = crtc_get_conn(decon->crtc->base.state, DRM_MODE_CONNECTOR_DSI);
	if (!conn)
		return -EINVAL;

	exynos_conn_state = to_exynos_connector_state(conn->state);
	if (!exynos_conn_state)
		return -EINVAL;

	exynos_conn_state->requested_panel_recovery = reset;

	return 0;
}

static bool __verify_in_time(struct exynos_recovery_cond *cond)
{
	struct recovery_queue *rq;
	ktime_t ctime = ktime_get();

	if (!cond || cond->limits_num_in_time == 0 || cond->in_time == 0)
		return true;

	rq = &cond->rq;
	if (__rq_is_full(rq)) {
		ktime_t diff = ktime_sub(ctime, __rq_top(rq));

		if (ktime_compare(diff, cond->in_time) <= 0) {
			recov_info("not matched:%s:%dtimes in %lld ms, cond = %lld ms\n",
					cond->name, cond->limits_num_in_time,
					ktime_to_ms(diff),
					ktime_to_ms(cond->in_time));
			return false;
		}
	}
	__rq_addq(rq, ctime);
	return true;
}

static bool __verify_max_count(struct exynos_recovery_cond *cond)
{
	if (!cond || cond->max_recovery_count == 0)
		return true;

	if (cond->count >= cond->max_recovery_count)
		return false;
	else
		return true;
}

static inline bool is_primary_crtc(const struct drm_crtc *crtc)
{
	return ((drm_crtc_index(crtc) == 0) ? true : false);
}

static void exynos_recovery_simple_dump(const struct exynos_recovery *recovery)
{
#if defined(ENABLE_RECOVERY_DUMP)
	const struct exynos_recovery_cond *cond = NULL;
	bool find = false;

	if (recovery->req_mode_id != RECOVERY_MODE_MAX)
		find = true;

	cond = &recovery->r_cond[recovery->req_mode_id];

	recov_info("====== RECOVERY_SIMPLE_DUMP =====\n");
	recov_info(" selected mode : %s\n", (find ? cond->name : "unknown"));
	if (find) {
		recov_info(" id [%d], count [%d]\n", cond->id, cond->count);
		recov_info(" refresh : panel[%s] vblank[%s]\n",
				(cond->refresh_panel ? "O" : "X"),
				(cond->reset_vblank ? "O" : "X"));
	}
#endif
}

static bool is_supported_mode(struct exynos_recovery *recovery)
{
	bool supported = false;

	mutex_lock(&recovery->r_lock);
	if (recovery->req_mode > 0x0 &&
		recovery->req_mode <= recovery->max_req_mode)
		supported = true;
	mutex_unlock(&recovery->r_lock);

	return supported;
}

static void __find_mode(struct exynos_recovery *recovery, int id)
{
	struct decon_device *decon = container_of(recovery,
			struct decon_device, recovery);
	struct exynos_recovery_cond *cond;
	struct drm_device *dev;
	ktime_t start;

	recovery->req_mode_id = id;

	cond = &recovery->r_cond[recovery->req_mode_id];

	if (!strncmp(&cond->name[0], "dsim", RECOVERY_NAME_LEN)) {
		dev = decon->drm_dev;
		start = ktime_get();

		if (dev) {
			drm_wait_one_vblank(dev, 0);
			if (ktime_ms_delta(ktime_get(), start) > 100) {
				recov_info("not detect vblank\n");
				cond->reset_phy = true;
			}
		}
	}

	if (cond->refresh_panel || cond->reset_phy)
		__set_panel_reset(decon, true);
	else
		__set_panel_reset(decon, false);
}

static bool exynos_recovery_condition_check_sub(const struct drm_crtc *crtc,
				struct exynos_recovery_cond *cond)
{

	if (!cond) {
		recov_err("cond nullptr err\n");
		return false;
	}

	if (cond->func && cond->func(crtc)) {
		recov_info("%s:matched:cond-function\n", cond->name);
		return true;
	}

	return false;
}

static bool exynos_recovery_condition_check(const struct drm_crtc *crtc,
				struct exynos_recovery *recovery)
{
	struct exynos_recovery_cond *cond;
	u32 tmp_mode;
	int id, req_id;
	bool matched = false;

	if (!crtc || !recovery) {
		recov_err("nullptr\n");
		return false;
	}

	tmp_mode = recovery->req_mode;
	id = 0;

	while (tmp_mode) {
		req_id = ((tmp_mode & (1 << id)) ? id : -1);
		tmp_mode &= ~(1 << id);
		if (req_id >= 0) {
			cond = &recovery->r_cond[req_id];
			if (exynos_recovery_condition_check_sub(crtc, cond)) {
				matched = true;
				break;
			}
		}
		++id;
	}

	if (matched)
		__find_mode(recovery, req_id);
	else
		return false;

	cond = &recovery->r_cond[recovery->req_mode_id];
	if (!cond) {
		recov_err("cond null ptr\n");
		return false;
	}

	if (cond->func && !cond->func(crtc)) {
		recov_info("%s:not matched:cond-function\n", cond->name);
		return false;
	}

	if (!__verify_max_count(cond)) {
		recov_info("%s:func matched, but over maximun retry\n", cond->name);
		return false;
	}

	if (!__verify_in_time(cond)) {
		recov_info("%s:func&max matched, but too manay recovery in time\n",
					cond->name);
		return false;
	}

	recov_debug("%s: passed!\n", cond->name);

	return true;
}

static struct drm_atomic_state *
exynos_recovery_atomic_helper_duplicate_state(struct drm_device *dev,
				  struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_atomic_state *state;
	struct drm_connector *conn;
	struct drm_connector_list_iter conn_iter;
	struct drm_plane *plane;
	struct drm_crtc *crtc;
	struct drm_crtc *primary_crtc = NULL;
	int err = 0;
	struct exynos_drm_crtc_state *exynos_crtc_state;

	state = drm_atomic_state_alloc(dev);
	if (!state)
		return ERR_PTR(-ENOMEM);

	state->acquire_ctx = ctx;
	state->duplicated = true;

	drm_for_each_crtc(crtc, dev) {
		struct drm_crtc_state *crtc_state;

		if (!is_primary_crtc(crtc))
			continue;

		primary_crtc = crtc;

		crtc_state = drm_atomic_get_crtc_state(state, crtc);
		if (IS_ERR(crtc_state)) {
			err = PTR_ERR(crtc_state);
			goto free;
		}

		exynos_crtc_state = to_exynos_crtc_state(crtc_state);

		exynos_crtc_state->dqe_fd = -1;
		exynos_crtc_state->reserved_win_mask = 0;
		exynos_crtc_state->visible_win_mask = 0;
	}

	drm_for_each_plane(plane, dev) {
		struct drm_plane_state *plane_state;

		plane_state = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_state)) {
			err = PTR_ERR(plane_state);
			goto free;
		}
	}

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		struct drm_connector_state *conn_state;

		if (conn->state && primary_crtc != conn->state->crtc)
			continue;

		conn_state = drm_atomic_get_connector_state(state, conn);
		if (IS_ERR(conn_state)) {
			err = PTR_ERR(conn_state);
			drm_connector_list_iter_end(&conn_iter);
			goto free;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	/* clear the acquire context so that it isn't accidentally reused */
	state->acquire_ctx = NULL;

free:
	if (err < 0) {
		drm_atomic_state_put(state);
		state = ERR_PTR(err);
	}

	return state;
}

#define DEFAULT_INTERVAL_FPS 60
s64 exynos_recovery_calc_retry_interval(struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	int i;
	int fps = DEFAULT_INTERVAL_FPS;

	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		if (!is_primary_crtc(crtc))
			continue;

		fps = drm_mode_vrefresh(&crtc_state->mode) ?: DEFAULT_INTERVAL_FPS;
	}

	return USEC_PER_SEC / fps / MSEC_PER_SEC;
}

static int __monitor_vblank(struct drm_device *dev, s64 interval)
{
	struct drm_vblank_crtc *vblank;
	int pipe = 0;
	u64 last;
	int i;
	s64 wait_time = interval * 3;

	if (dev) {
		vblank = &dev->vblank[pipe];
		last = atomic64_read(&vblank->count);

		for (i = 0; i < 5; ++i) {
			u64 cur;

			msleep(wait_time);
			cur = atomic64_read(&vblank->count);

			if (last != cur)
				return 0;
		}
	}

	return -EINVAL;
}

static void exynos_recovery_atomic_helper_commit_hw_done(struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct drm_crtc_commit *old_commit, *new_commit;
	int i;

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		old_commit = old_crtc_state->commit;
		new_commit = new_crtc_state->commit;

		if (!new_commit || !old_commit)
			continue;

		drm_crtc_commit_put(old_commit);

		old_crtc_state->commit = drm_crtc_commit_get(new_commit);

		complete_all(&old_commit->hw_done);
		complete_all(&old_commit->flip_done);
	}
}



int exynos_recovery_atomic_helper_commit_duplicated_state(struct drm_atomic_state *state,
					      struct drm_modeset_acquire_ctx *ctx)
{
	int i, ret;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;

	state->acquire_ctx = ctx;

	for_each_new_plane_in_state(state, plane, new_plane_state, i)
		state->planes[i].old_state = plane->state;

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		if (!is_primary_crtc(crtc))
			continue;

		state->crtcs[i].old_state = crtc->state;
	}

	for_each_new_connector_in_state(state, connector, new_conn_state, i) {
		state->connectors[i].old_state = connector->state;
	}

	ret = drm_atomic_commit(state);

	state->acquire_ctx = NULL;

	return ret;
}

static void exynos_recovery_handler(struct work_struct *work)
{
	struct exynos_recovery *recovery = container_of(work,
					struct exynos_recovery, work);
	struct decon_device *decon = container_of(recovery, struct decon_device,
					recovery);
	struct drm_device *dev = decon->drm_dev;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_atomic_state *state, *rcv_state;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc = &decon->crtc->base;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	// To Do : find out & fix how to disable correctly.
	/*
	struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	*/
	int ret;
	u64 time_s, time_c;
	int retry = 0;
	bool intf_reset = false;

	if (exynos_recovery_get_state(decon) == RECOVERY_NOT_SUPPORTED)
		return;

	if (!is_primary_crtc(crtc)) {
		exynos_recovery_set_state(decon, RECOVERY_IDLE);
		return;
	}

	if (!is_supported_mode(recovery)) {
		exynos_recovery_set_state(decon, RECOVERY_IDLE);
		return;
	}

	if (!exynos_recovery_condition_check(crtc, recovery)) {
		if (recovery->always) {
			recovery->req_mode_id = recovery->always_id;
			recov_info("not matched:but always-recovery enabled\n");
		} else {
			recov_info("recovery condition not matched\n");
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#if IS_ENABLED(CONFIG_DISPLAY_USE_INFO) || IS_ENABLED(CONFIG_USDM_PANEL_DPUI)
			log_decon_bigdata(decon);
#endif
#endif
			dpu_dump(exynos_crtc);
			dbg_snapshot_expire_watchdog();
			BUG();
			return;
		}
	}

	DPU_EVENT_LOG("RECOVERY_START", exynos_crtc, 0, "id[%d] count[%d]",
			recovery->r_cond[recovery->req_mode_id].id,
			recovery->r_cond[recovery->req_mode_id].count);

	time_s = ktime_to_us(ktime_get());
	pm_stay_awake(decon->dev);
	exynos_recovery_set_state(decon, RECOVERY_BEGIN);

	drm_modeset_acquire_init(&ctx, 0);

retry:
	++retry;

	rcv_state = exynos_recovery_atomic_helper_duplicate_state(dev, &ctx);
	if (IS_ERR(rcv_state)) {
		ret = PTR_ERR(rcv_state);
		goto out_drop_locks;
	}

	state = drm_atomic_state_alloc(dev);
	if (!state) {
		ret = -ENOMEM;
		goto out_drop_locks;
	}

	state->acquire_ctx = &ctx;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		ret = PTR_ERR(crtc_state);
		goto out;
	}

	crtc_state->active = false;

	// To Do : find out & fix how to disable correctly.
	/*
	ret = drm_atomic_set_mode_prop_for_crtc(crtc_state, NULL);
	if (ret)
		goto out;

	ret = drm_atomic_add_affected_planes(state, crtc);
	if (ret)
		goto out;

	ret = drm_atomic_add_affected_connectors(state, crtc);
	if (ret)
		goto out;

	for_each_new_connector_in_state(state, conn, conn_state, i) {
		ret = drm_atomic_set_crtc_for_connector(conn_state, NULL);
		if (ret)
			goto out;
	}

	for_each_new_plane_in_state(state, plane, plane_state, i) {
		ret = drm_atomic_set_crtc_for_plane(plane_state, NULL);
		if (ret)
			goto out;
		drm_atomic_set_fb_for_plane(plane_state, NULL);
	}
	*/

	if (recovery->r_cond[recovery->req_mode_id].reset_vblank ||
		recovery->r_cond[recovery->req_mode_id].refresh_panel ||
		recovery->r_cond[recovery->req_mode_id].reset_phy) {
		intf_reset = true;
		exynos_recovery_atomic_helper_commit_hw_done(rcv_state);
	}

	ret = drm_atomic_commit(state);
	if (ret)
		goto out;

	drm_mode_config_reset(dev);
	exynos_recovery_set_state(decon, RECOVERY_RESTORE);

	ret = exynos_recovery_atomic_helper_commit_duplicated_state(rcv_state, &ctx);
	if (ret)
		goto out;

	if (intf_reset) {
		s64 interval = exynos_recovery_calc_retry_interval(rcv_state);

		ret = __monitor_vblank(dev, interval);
		if (ret != 0)
			goto out;
	}

	recovery->count++;
	recovery->r_cond[recovery->req_mode_id].count++;
	time_c = (ktime_to_us(ktime_get())-time_s);

	DPU_EVENT_LOG("RECOVERY_END", exynos_crtc, 0, "id[%d] count[%d]",
			recovery->r_cond[recovery->req_mode_id].id,
			recovery->r_cond[recovery->req_mode_id].count);

	exynos_recovery_simple_dump(recovery);
	recov_info("recovery(#%d) is successfully finished (%lld.%03lldms)\n",
			recovery->count,
			time_c/USEC_PER_MSEC, time_c%USEC_PER_MSEC);

out:
	if (ret == -EDEADLK || ret == -EINVAL) {
		drm_atomic_state_clear(state);
		drm_atomic_state_clear(rcv_state);
		ret = drm_modeset_backoff(&ctx);
		if (!ret) {
			if (retry >= RECOVERY_RETRY_CNT) {
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#if IS_ENABLED(CONFIG_DISPLAY_USE_INFO) || IS_ENABLED(CONFIG_USDM_PANEL_DPUI)
				log_decon_bigdata(decon);
#endif
#endif
				dpu_dump(exynos_crtc);
				dbg_snapshot_expire_watchdog();
				BUG();
			}
			recov_info(" *** recovery retry!! (%d) *** \n", retry);
			goto retry;
		}
	}

	drm_atomic_state_put(state);
	drm_atomic_state_put(rcv_state);

out_drop_locks:
	recovery->r_cond[recovery->req_mode_id].reset_phy = false;
	exynos_recovery_set_state(decon, RECOVERY_IDLE);
	pm_relax(decon->dev);
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
}

static void __set_recovery_state(struct exynos_recovery *recovery, enum recovery_state state)
{
	enum recovery_state from = recovery->r_state;

	mutex_lock(&recovery->r_lock);
	recovery->r_state = state;
	mutex_unlock(&recovery->r_lock);

	recov_debug("state changed (%s -> %s)\n", recovery_state_string[from],
			recovery_state_string[recovery->r_state]);
}

static enum recovery_state __get_recovery_state(struct exynos_recovery *recovery)
{
	enum recovery_state state;

	mutex_lock(&recovery->r_lock);
	state = recovery->r_state;
	mutex_unlock(&recovery->r_lock);

	return state;
}

static int __find_id(struct exynos_recovery *recovery, char *mode)
{
	int id = 0;
	struct exynos_recovery_cond *cond;

	for (id = 0; id < recovery->modes; ++id) {
		cond = &recovery->r_cond[id];
		if (!strncmp(&cond->name[0], mode, RECOVERY_NAME_LEN))
			return id;
	}

	return -1;
}

static void __set_recovery_mode(struct exynos_recovery *recovery, char *mode)
{
	char mode_str[RECOVERY_MODE_MAX * RECOVERY_NAME_LEN];
	char *tok, *pstr;
	int len = strlen(mode);

	mutex_lock(&recovery->r_lock);
	recovery->req_mode = 0x0;
	strncpy(&mode_str[0], mode, len);
	mode_str[len] = '\0';
	pstr = mode_str;
	tok = strsep(&pstr, "|");
	while (tok) {
		int id = __find_id(recovery, tok);

		if (id >= 0)
			recovery->req_mode |= (1 << id);
		tok = strsep(&pstr, "|");
	}
	recov_info("req_mode = 0x%04x\n", recovery->req_mode);
	mutex_unlock(&recovery->r_lock);
}

int exynos_recovery_set_state(struct decon_device *decon, enum recovery_state state)
{
	struct exynos_recovery *recovery;
	const struct exynos_recovery_funcs *funcs;

	if (!decon || state >= RECOVERY_SMAX)
		return -EINVAL;

	recovery = &decon->recovery;
	if (!recovery || !recovery->funcs)
		return -EINVAL;

	funcs = recovery->funcs;
	if (funcs)
		funcs->set_state(recovery, state);

	return 0;
}

enum recovery_state exynos_recovery_get_state(struct decon_device *decon)
{
	struct exynos_recovery *recovery;
	const struct exynos_recovery_funcs *funcs;
	enum recovery_state cstate = RECOVERY_NOT_SUPPORTED;

	if (!decon)
		return cstate;

	recovery = &decon->recovery;
	if (!recovery || !recovery->funcs)
		return cstate;

	funcs = recovery->funcs;
	if (funcs)
		cstate = funcs->get_state(recovery);

	return cstate;
}

int exynos_recovery_set_mode(struct decon_device *decon, char *mode)
{
	struct exynos_recovery *recovery;
	const struct exynos_recovery_funcs *funcs;

	if (!decon)
		return -EINVAL;

	recovery = &decon->recovery;
	if (!recovery || !recovery->funcs)
		return -EINVAL;

	funcs = recovery->funcs;
	if (funcs)
		funcs->set_mode(recovery, mode);

	return 0;
}

static const struct exynos_recovery_funcs recovery_funcs = {
	.set_state = __set_recovery_state,
	.get_state = __get_recovery_state,
	.set_mode = __set_recovery_mode,
};

static int __init_condition_params(struct exynos_recovery *recovery,
					int id, const char *name)
{
	struct exynos_recovery_cond *cond;
	int i;

	if (!recovery)
		return -EINVAL;

	cond = &recovery->r_cond[id];
	cond->id = id;
	strncpy(&cond->name[0], name, sizeof(cond->name) - 1);
	cond->count = 0;
	cond->max_recovery_count = 0;
	cond->limits_num_in_time = 0;
	cond->in_time = ms_to_ktime(0);
	cond->refresh_panel = false;
	cond->reset_vblank = false;
	cond->reset_phy = false;

	for (i = 0; i < sizeof(recovery_map)/sizeof(struct recovery_mapping) ; ++i) {
		if (strncmp(cond->name, recovery_map[i].name, RECOVERY_NAME_LEN) == 0)
			cond->func = recovery_map[i].func;
	}

	return 0;
}

static int exynos_recovery_parse_dt_sub(struct device_node *np,
					struct exynos_recovery *recovery,
					int id, const char *name)
{
	struct exynos_recovery_cond *cond;
	u32 data[2];
	int ret, i;
	bool findout = false;

	if (!recovery)
		return -EINVAL;

	cond = &recovery->r_cond[id];
	cond->id = id;
	strncpy(&cond->name[0], name, RECOVERY_NAME_LEN - 1);

	of_property_read_u32_array(np, "max-limit", data, 2);
	if (WARN_ON(data[0] >= INTERNAL_MAX_QUEUE_SZ))
		data[0] = INTERNAL_MAX_QUEUE_SZ - 1;
	cond->limits_num_in_time = data[0];
	cond->in_time = ms_to_ktime((u64)data[1] * 60 * MSEC_PER_SEC);
	cond->rq.front = 0;
	cond->rq.rear = 0;
	cond->rq.fsz = (int)data[0] + 1;

	cond->count = 0;
	ret = of_property_read_u32(np, "max-recovery", &data[0]);
	if (ret == -EINVAL || (ret == 0 && data[0] == 0))
		cond->max_recovery_count = 0;
	else
		cond->max_recovery_count = data[0];

	cond->refresh_panel = of_property_read_bool(np, "refresh-panel");
	recov_info("refresh panel=%d\n", cond->refresh_panel);

	cond->reset_vblank = of_property_read_bool(np, "reset-vblank");
	recov_info("reset vblank=%d\n", cond->reset_vblank);

	cond->reset_phy = false;

	for (i = 0; i < sizeof(recovery_map)/sizeof(struct recovery_mapping) ; ++i) {
		if (strncmp(cond->name, recovery_map[i].name, RECOVERY_NAME_LEN) == 0) {
			cond->func = recovery_map[i].func;
			findout = true;
		}
	}

	if (!findout)
		cond->func = skip_condition_check;

	return 0;
}

static int exynos_recovery_parse_dt(struct device_node *np,
					struct decon_device *decon)
{
	struct exynos_recovery *recovery = &decon->recovery;
	const char *name[RECOVERY_NAME_LEN];
	struct device_node *child_np, *cchild_np;
	int num_cond, i;

	num_cond = of_property_count_strings(np, "recovery-modes");
	if (num_cond < 0) {
		recov_err("failed to get recovery mode count\n");
		return -EINVAL;
	}

	num_cond = of_property_read_string_array(np, "recovery-modes",
				name, num_cond);
	if (num_cond < 0) {
		recov_err("failed to get recovery mode name\n");
		return -EINVAL;
	}
	recovery->modes = num_cond;
	recovery->max_req_mode = 0x0;

	child_np = of_get_child_by_name(np, "recovery-condition");
	if (!child_np) {
		for (i = 0; i < num_cond; ++i)
			__init_condition_params(recovery, i, name[i]);
	} else {
		for (i = 0; i < num_cond; ++i) {
			cchild_np = of_get_child_by_name(child_np, name[i]);
			if (!cchild_np) {
				__init_condition_params(recovery, i, name[i]);
			} else {
				exynos_recovery_parse_dt_sub(cchild_np,
						recovery, i, name[i]);
			}
		}
	}

	for (i = 0; i < num_cond; ++i) {
		recovery->max_req_mode |= (1 << i);
		if (strncmp(name[i], "always", RECOVERY_NAME_LEN) == 0) {
			recovery->always = true;
			recovery->always_id = i;
			break;
		}
	}

	return 0;
}

void exynos_recovery_register(struct decon_device *decon)
{
	struct exynos_recovery *recovery = &decon->recovery;
	struct device *dev = decon->dev;
	struct device_node *np;
	int ret = 0;
	u32 val = 1;

	mutex_init(&recovery->r_lock);
	np = dev->of_node;
	ret = of_property_read_u32(np, "recovery-enable", &val);
	if (ret == -EINVAL || (ret == 0 && val == 0)) {
		exynos_recovery_set_state(decon, RECOVERY_NOT_SUPPORTED);
		recov_debug("decon#%d:recovery not supported\n", decon->id);
		return;
	}

	if (exynos_recovery_parse_dt(np, decon) < 0) {
		exynos_recovery_set_state(decon, RECOVERY_NOT_SUPPORTED);
		return;
	}

	INIT_WORK(&recovery->work, exynos_recovery_handler);
	recovery->count = 0;
	recovery->funcs = &recovery_funcs;
	exynos_recovery_set_state(decon, RECOVERY_IDLE);
	recov_info("decon#%d recovery registered\n", decon->id);
}
EXPORT_SYMBOL(exynos_recovery_register);
