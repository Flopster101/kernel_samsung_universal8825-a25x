// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_freq_hop.c
 *
 * Copyright (C) 2021 Samsung Electronics Co.Ltd
 * Authors:
 *	Hwangjae Lee <hj-yo.lee@samsung.com>
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

#include <exynos_drm_crtc.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_freq_hop.h>
#include <exynos_drm_dsim.h>

static int dsim_set_freq_hop(struct dsim_device *dsim, struct dsim_freq_hop *freq)
{
	struct stdphy_pms *pms;

	if (dsim->state != DSIM_STATE_HSCLKEN) {
		pr_info("dsim power is off state(0x%x)\n", dsim->state);
		return -EINVAL;
	}

	pms = &dsim->config.dphy_pms;
	dsim_reg_set_dphy_freq_hopping(dsim->id, pms->p, freq->target_m,
			freq->target_k,	(freq->target_m > 0) ? 1 : 0);

	return 0;
}

// This function is called common display driver */
void dpu_update_freq_hop(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct dsim_device *dsim = decon_get_dsim(decon);

	if ((!dsim) || (!dsim->freq_hop.enabled))
		return;

	dsim->freq_hop.target_m = dsim->freq_hop.request_m;
	dsim->freq_hop.target_k = dsim->freq_hop.request_k;
}

void dpu_set_freq_hop(struct exynos_drm_crtc *exynos_crtc, bool en)
{
	struct stdphy_pms *pms;
	struct decon_device *decon = exynos_crtc->ctx;
	struct dsim_device *dsim = decon_get_dsim(decon);
	struct dsim_freq_hop freq_hop;
	u32 target_m, target_k;

	if (!dsim || !dsim->freq_hop.enabled)
		return;

	target_m = dsim->freq_hop.target_m;
	target_k = dsim->freq_hop.target_k;

	pms = &dsim->config.dphy_pms;
	if ((pms->m != target_m) || (pms->k != target_k)) {
		if (en) {
			dsim_set_freq_hop(dsim, &dsim->freq_hop);
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
			/* wakeup PLL if sleeping... */
			decon_reg_set_pll_wakeup(decon->id, true);
			decon_reg_set_pll_sleep(decon->id, false);
#endif
		} else {
			pms->m = dsim->freq_hop.target_m;
			pms->k = dsim->freq_hop.target_k;
			freq_hop.target_m = 0;
			pr_info("%s: en(%d), pmsk[%d %d %d %d]\n", __func__,
					en, pms->p, pms->m, pms->s, pms->k);
			dsim_set_freq_hop(dsim, &freq_hop);
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
			decon_reg_set_pll_sleep(decon->id, true);
#endif
		}
	}
}

struct dpu_freq_hop_ops dpu_freq_hop_control = {
	.set_freq_hop	 = dpu_set_freq_hop,
	.update_freq_hop = dpu_update_freq_hop,
};

static int dpu_debug_freq_hop_show(struct seq_file *s, void *unused)
{
	struct decon_device *decon = s->private;
	struct dsim_device *dsim = decon_get_dsim(decon);

	if (!dsim || !dsim->freq_hop.enabled)
		return 0;
	seq_printf(s, "m(%u) k(%u)\n", dsim->freq_hop.request_m,
			dsim->freq_hop.request_k);

	return 0;
}

static int dpu_debug_freq_hop_open(struct inode *inode, struct file *file)
{
	return single_open(file, dpu_debug_freq_hop_show, inode->i_private);
}

static ssize_t dpu_debug_freq_hop_write(struct file *file,
		const char __user *buf, size_t count, loff_t *f_ops)
{
	struct decon_device *decon = get_decon_drvdata(0);
	struct dsim_device *dsim = decon_get_dsim(decon);
	char *buf_data;
	int ret;

	if (!dsim || !dsim->freq_hop.enabled)
		return count;

	if (!count)
		return count;

	buf_data = kmalloc(count, GFP_KERNEL);
	if (buf_data == NULL)
		return count;

	memset(buf_data, 0, count);

	ret = copy_from_user(buf_data, buf, count);
	if (ret < 0)
		goto out;

	mutex_lock(&dsim->freq_hop_lock);
	ret = sscanf(buf_data, "%u %u", &dsim->freq_hop.request_m,
			&dsim->freq_hop.request_k);
	mutex_unlock(&dsim->freq_hop_lock);
	if (ret < 0)
		goto out;

out:
	kfree(buf_data);
	return count;
}

static const struct file_operations dpu_freq_hop_fops = {
	.open = dpu_debug_freq_hop_open,
	.write = dpu_debug_freq_hop_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* This function is called decon_late_register */
void dpu_freq_hop_debugfs(struct exynos_drm_crtc *exynos_crtc)
{
#if defined(CONFIG_DEBUG_FS)
	struct dentry *debug_request_mk;
	struct decon_device *decon = exynos_crtc->ctx;
	struct drm_crtc *crtc = &exynos_crtc->base;

	decon->crtc->freq_hop = &dpu_freq_hop_control;
	debug_request_mk = debugfs_create_file("request_mk", 0444,
			crtc->debugfs_entry, decon, &dpu_freq_hop_fops);

	if (!debug_request_mk) {
		pr_err("failed to create debugfs debugfs request_mk directory\n");
	}
#endif
}

/* This function is called dsim_probe */
void dpu_init_freq_hop(struct dsim_device *dsim)
{
	const struct dsim_pll_params *pll_params = dsim->pll_params;

	if (IS_ENABLED(CONFIG_EXYNOS_FREQ_HOP) &&
	    (dsim->config.mode == DSIM_COMMAND_MODE)) {
		dsim->freq_hop.enabled = true;
		dsim->freq_hop.target_m = pll_params->params[0]->m;
		dsim->freq_hop.request_m = pll_params->params[0]->m;
		dsim->freq_hop.target_k = pll_params->params[0]->k;
		dsim->freq_hop.request_k = pll_params->params[0]->k;
	} else {
		dsim->freq_hop.enabled = false;
	}
}
