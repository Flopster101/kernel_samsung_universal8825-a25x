// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_hibernation.c
 *
 * Copyright (C) 2020 Samsung Electronics Co.Ltd
 * Authors:
 *	Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/atomic.h>
#include <uapi/linux/sched/types.h>

#include <dpu_trace.h>

#include <exynos_drm_hibernation.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_writeback.h>
#include <exynos_drm_dp.h>
#include <exynos_drm_partial.h>

#define HIBERNATION_ENTRY_DEFAULT_FPS		60
#define HIBERNATION_ENTRY_MIN_TIME_MS		50
#define HIBERNATION_ENTRY_MIN_ENTRY_CNT		1

static int dpu_hiber_log_level = 6;
module_param(dpu_hiber_log_level, int, 0600);
MODULE_PARM_DESC(dpu_hiber_log_level, "log level for hibernation [default : 6]");

#define HIBER_NAME "exynos-hiber"
#define hiber_info(fmt, ...)	\
	dpu_pr_info(HIBER_NAME, 0, dpu_hiber_log_level, fmt, ##__VA_ARGS__)

#define hiber_warn(fmt, ...)	\
	dpu_pr_warn(HIBER_NAME, 0, dpu_hiber_log_level, fmt, ##__VA_ARGS__)

#define hiber_err(fmt, ...)	\
	dpu_pr_err(HIBER_NAME, 0, dpu_hiber_log_level, fmt, ##__VA_ARGS__)

#define hiber_debug(fmt, ...)	\
	dpu_pr_debug(HIBER_NAME, 0, dpu_hiber_log_level, fmt, ##__VA_ARGS__)

static bool is_dsr_operating(struct exynos_hibernation *hiber)
{
	struct drm_crtc_state *crtc_state = hiber->decon->crtc->base.state;
	struct exynos_drm_crtc_state *exynos_crtc_state;

	if (!crtc_state)
		return false;

	exynos_crtc_state = to_exynos_crtc_state(crtc_state);

	return exynos_crtc_state->dsr_status;
}

static bool is_dim_operating(struct exynos_hibernation *hiber)
{
	const struct decon_device *decon = hiber->decon;

	if (!decon)
		return false;

	return decon->dimming;
}

static ssize_t exynos_show_hibernation_exit(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	struct exynos_drm_crtc *exynos_crtc = decon->crtc;
	char *p = buf;
	int len = 0;

	hiber_debug("%s +\n", __func__);

	if (!exynos_crtc->hibernation->early_wakeup_enable)
		return len;

	exynos_crtc->hibernation->early_wakeup_cnt++;
	kthread_queue_work(&exynos_crtc->hibernation->exit_worker,
			&exynos_crtc->hibernation->exit_work);
	len = sprintf(p, "%d\n", exynos_crtc->hibernation->early_wakeup_cnt);

	hiber_debug("%s -\n", __func__);
	return len;
}
static DEVICE_ATTR(hiber_exit, S_IRUGO, exynos_show_hibernation_exit, NULL);

static void exynos_hibernation_trig_reset(struct exynos_hibernation *hiber)
{
	const struct decon_device *decon = hiber->decon;
	const int fps = decon->config.fps ? : HIBERNATION_ENTRY_DEFAULT_FPS;
	int entry_cnt = DIV_ROUND_UP(fps * HIBERNATION_ENTRY_MIN_TIME_MS, MSEC_PER_SEC);

	if (entry_cnt < HIBERNATION_ENTRY_MIN_ENTRY_CNT)
		entry_cnt = HIBERNATION_ENTRY_MIN_ENTRY_CNT;

	atomic_set(&hiber->trig_cnt, entry_cnt);
}

static bool exynos_hibernation_check(struct exynos_hibernation *hiber)
{
	hiber_debug("%s +\n", __func__);

	return (!is_hibernaton_blocked(hiber) && !is_dsr_operating(hiber) &&
		!is_dim_operating(hiber) && atomic_dec_and_test(&hiber->trig_cnt));
}

static void exynos_hibernation_enter(struct exynos_hibernation *hiber)
{
	struct decon_device *decon = hiber->decon;

	hiber_debug("%s +\n", __func__);

	if (!decon)
		return;

	DPU_ATRACE_BEGIN("exynos_hibernation_enter");
	mutex_lock(&hiber->lock);
	hibernation_block(hiber);

	if (decon->state != DECON_STATE_ON)
		goto ret;

	DPU_EVENT_LOG(DPU_EVT_ENTER_HIBERNATION_IN, decon->crtc, NULL);

	hiber->wb = decon_get_wb(decon);
	if (hiber->wb)
		writeback_enter_hibernation(hiber->wb);

	decon_enter_hibernation(decon);

	hiber->dsim = decon_get_dsim(decon);
	if (hiber->dsim)
		dsim_enter_ulps(hiber->dsim);

	if (IS_ENABLED(CONFIG_EXYNOS_BTS))
		decon->bts.ops->release_bw(decon->crtc);

	pm_runtime_put_sync(decon->dev);

	DPU_EVENT_LOG(DPU_EVT_ENTER_HIBERNATION_OUT, decon->crtc, NULL);

ret:
	hibernation_unblock(hiber);
	mutex_unlock(&hiber->lock);
	DPU_ATRACE_END("exynos_hibernation_enter");

	hiber_debug("%s: DPU power %s -\n", __func__,
			pm_runtime_active(decon->dev) ? "on" : "off");
}

static void exynos_hibernation_exit(struct exynos_hibernation *hiber)
{
	struct decon_device *decon = hiber->decon;

	hiber_debug("%s +\n", __func__);

	if (!decon)
		return;

	hibernation_block(hiber);

	/*
	 * Cancel and/or wait for finishing previous queued hibernation entry work. It only
	 * goes to sleep when work is currently executing. If not, there is no operation here.
	 */
	kthread_cancel_work_sync(&hiber->work);

	mutex_lock(&hiber->lock);

	exynos_hibernation_trig_reset(hiber);

	if (decon->state != DECON_STATE_HIBERNATION)
		goto ret;

	DPU_ATRACE_BEGIN("exynos_hibernation_exit");

	DPU_EVENT_LOG(DPU_EVT_EXIT_HIBERNATION_IN, decon->crtc, NULL);

	pm_runtime_get_sync(decon->dev);

	if (hiber->dsim) {
		dsim_exit_ulps(hiber->dsim);
		hiber->dsim = NULL;
	}

	decon_exit_hibernation(decon);

	if (hiber->wb) {
		writeback_exit_hibernation(hiber->wb);
		hiber->wb = NULL;
	}

	if (decon->crtc->partial)
		exynos_partial_restore(decon->crtc->partial);

	if (IS_ENABLED(CONFIG_EXYNOS_BTS) && decon->bts.ops->set_bus_qos)
		decon->bts.ops->set_bus_qos(decon->crtc);

	DPU_EVENT_LOG(DPU_EVT_EXIT_HIBERNATION_OUT, decon->crtc, NULL);
	DPU_ATRACE_END("exynos_hibernation_exit");

ret:
	mutex_unlock(&hiber->lock);
	hibernation_unblock(hiber);

	hiber_debug("%s: DPU power %s -\n", __func__,
			pm_runtime_active(decon->dev) ? "on" : "off");
}

void hibernation_block_exit(struct exynos_hibernation *hiber)
{
	const struct exynos_hibernation_funcs *funcs;

	if (!hiber)
		return;

	hibernation_block(hiber);

	funcs = hiber->funcs;
	if (funcs)
		funcs->exit(hiber);
}

static const struct exynos_hibernation_funcs hibernation_funcs = {
	.check	= exynos_hibernation_check,
	.enter	= exynos_hibernation_enter,
	.exit	= exynos_hibernation_exit,
};

static void exynos_hibernation_handler(struct kthread_work *work)
{
	struct exynos_hibernation *hibernation =
		container_of(work, struct exynos_hibernation, work);
	const struct exynos_hibernation_funcs *funcs = hibernation->funcs;

	hiber_debug("Display hibernation handler is called(trig_cnt:%d)\n",
			atomic_read(&hibernation->trig_cnt));

	/* If hibernation entry condition does NOT meet, just return here */
	if (!funcs->check(hibernation))
		return;

	funcs->enter(hibernation);
}

static void exynos_hibernation_exit_handler(struct kthread_work *work)
{
	struct exynos_hibernation *hibernation =
		container_of(work, struct exynos_hibernation, exit_work);
	const struct exynos_hibernation_funcs *funcs = hibernation->funcs;

	hiber_debug("Display hibernation exit handler is called\n");

	funcs->exit(hibernation);
}

struct exynos_hibernation *
exynos_hibernation_register(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct device_node *np;
	struct exynos_hibernation *hibernation;
	struct device *dev = decon->dev;
	struct sched_param param;
	int ret = 0;
	u32 val = 1;

	np = dev->of_node;
	ret = of_property_read_u32(np, "hibernation", &val);
	/* if ret == -EINVAL, property is not existed */
	if (ret == -EINVAL || (ret == 0 && val == 0)) {
		hiber_info("display hibernation is not supported\n");
		return NULL;
	}

	hibernation = devm_kzalloc(dev, sizeof(struct exynos_hibernation),
			GFP_KERNEL);
	if (!hibernation)
		return NULL;

	hibernation->decon = decon;
	hibernation->funcs = &hibernation_funcs;

	mutex_init(&hibernation->lock);

	exynos_hibernation_trig_reset(hibernation);

	atomic_set(&hibernation->block_cnt, 0);

	kthread_init_work(&hibernation->work, exynos_hibernation_handler);

	hiber_info("display hibernation is supported\n");

	/* register asynchronous hibernation early wakeup feature */
	hibernation->early_wakeup_enable = false;
	if (!IS_ENABLED(CONFIG_DRM_SAMSUNG_HIBERNATION_EARLY_WAKEUP) || (decon->id)) {
		hiber_info("hibernation early wakeup is disabled\n");
		return hibernation;
	}

	/* initialize exit hibernation thread */
	kthread_init_worker(&hibernation->exit_worker);
	hibernation->exit_thread = kthread_run(kthread_worker_fn,
			&hibernation->exit_worker, "exynos_hibernation_exit");
	if (IS_ERR(hibernation->exit_thread)) {
		hibernation->exit_thread = NULL;
		hiber_err("failed to run exit hibernation thread\n");
		return hibernation;
	}
	param.sched_priority = 20;
	sched_setscheduler_nocheck(hibernation->exit_thread, SCHED_FIFO, &param);
	kthread_init_work(&hibernation->exit_work, exynos_hibernation_exit_handler);
	hibernation->early_wakeup_cnt = 0;
	ret = device_create_file(decon->dev, &dev_attr_hiber_exit);
	if (ret) {
		hiber_err("failed to create hiber exit file\n");
		return hibernation;
	}
	hibernation->early_wakeup_enable = true;

	hiber_info("hibernation early wakeup is enabled\n");

	return hibernation;
}

void exynos_hibernation_destroy(struct exynos_hibernation *hiber)
{
	if (!hiber)
		return;

	if (hiber->cam_op_reg)
		iounmap(hiber->cam_op_reg);
}
