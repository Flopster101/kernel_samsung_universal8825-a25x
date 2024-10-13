// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DPU Event log file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/moduleparam.h>
#include <video/mipi_display.h>
#include <drm/drm_print.h>
#include <drm/drm_atomic.h>

#include <exynos_drm_debug.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_plane.h>
#include <exynos_drm_format.h>
#include <exynos_drm_gem.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_recovery.h>
#include <cal_config.h>
#include <decon_cal.h>
#include <soc/samsung/memlogger.h>
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
#include <soc/samsung/exynos-itmon.h>
#endif

/* TODO: erase global variable */
struct memlog_obj *g_log_obj;
EXPORT_SYMBOL(g_log_obj);

struct memlog_obj *g_errlog_obj;
EXPORT_SYMBOL(g_errlog_obj);

#define LOG_BUF_SIZE	256

#define RSC_STATUS_IDLE	7
static size_t __dpu_rsc_to_string(struct exynos_drm_crtc *exynos_crtc,
		void *buf, size_t remained, u64 rsc)
{
	const struct drm_device *drm_dev = exynos_crtc->base.dev;
	int i, n = 0;
	int win_cnt = get_plane_num(drm_dev);
	unsigned int decon_id;

	for (i = 0; i < win_cnt; ++i) {
		decon_id = is_decon_using_ch(rsc, i);
		n += scnprintf(buf + n, remained - n, "%d[%c] ", i,
			decon_id < RSC_STATUS_IDLE ? '0' + decon_id : 'X');
	}

	return n;
}

size_t dpu_rsc_ch_to_string(void *src, void *buf, size_t remained)
{
	struct exynos_drm_crtc *exynos_crtc = src;
	u64 rsc_ch = decon_reg_get_rsc_ch(exynos_crtc->base.index);

	return __dpu_rsc_to_string(exynos_crtc, buf, remained, rsc_ch);
}

size_t dpu_rsc_win_to_string(void *src, void *buf, size_t remained)
{
	struct exynos_drm_crtc *exynos_crtc = src;
	u64 rsc_win = decon_reg_get_rsc_win(exynos_crtc->base.index);

	return __dpu_rsc_to_string(exynos_crtc, buf, remained, rsc_win);
}

size_t dpu_config_to_string(void *src, void *buf, size_t remained)
{
	struct dpu_bts_win_config *win = src;
	char *str_state[3] = {"DISABLED", "COLOR", "BUFFER"};
	const struct dpu_fmt *fmt;
	int n = 0;

	if (win->state == DPU_WIN_STATE_DISABLED)
		return n;

	fmt = dpu_find_fmt_info(win->format);

	n += scnprintf(buf + n, remained - n,
			"%s[0x%llx] SRC[%d %d %d %d] ",	str_state[win->state],
			(win->state == DPU_WIN_STATE_BUFFER) ?
			win->dbg_dma_addr : 0,
			win->src_x, win->src_y, win->src_w, win->src_h);
	n += scnprintf(buf + n, remained - n, "DST[%d %d %d %d] ",
			win->dst_x, win->dst_y, win->dst_w, win->dst_h);
	n += scnprintf(buf + n, remained - n, "ROT[%d] COMP[%d] HDR[%d] ",
			win->is_rot, win->is_comp, win->is_hdr);
	if (win->state == DPU_WIN_STATE_BUFFER)
		n += scnprintf(buf + n, remained - n, "CH%d ", win->dpp_ch);

	n += scnprintf(buf + n, remained - n, "%s ", fmt->name);
	n += scnprintf(buf + n, remained - n, "%s ", get_comp_src_name(win->comp_src));

	return n;
}

static bool
is_event_repeated(const char *name, struct dpu_memlog_event *event, u32 flag)
{
	unsigned long flags;
	bool ret = false;

	spin_lock_irqsave(&event->slock, flags);
	if (!(flag & EVENT_FLAG_REPEAT)) {
		event->repeat_cnt = 0;
		goto out;
	}

	if (event->repeat_cnt == 0) {
		event->last_event_len = strlen(name);
		strlcpy(event->last_event, name, event->last_event_len + 1);
	} else if (strncmp(event->last_event, name, event->last_event_len)) {
		event->repeat_cnt = 1;
		event->last_event_len = strlen(name);
		strlcpy(event->last_event, name, event->last_event_len + 1);
		goto out;
	}

	/*
	 * If the same event occurs DPU_EVENT_KEEP_CNT times continuously,
	 * it will be skipped.
	 */
	if (event->repeat_cnt < DPU_EVENT_KEEP_CNT)
		++event->repeat_cnt;
	else
		ret = true;

out:
	spin_unlock_irqrestore(&event->slock, flags);
	return ret;
}

/* ===== EXTERN APIs ===== */
/*
 * DPU_EVENT_LOG() - store information to log buffer by common API
 * @event_name: event name
 * @exynos_crtc: object to store event log, it can be NULL
 * @flag: EVENT_FLAG_xxx for DPU EVENT LOG
 * @fmt: format string or to_sting function ptr
 * @...: Arguments for the format string or function ptr
 *
 * Store information related to DECON, DSIM or DPP. Each DECON has event log
 * So, DECON id is used as @index
 */
void DPU_EVENT_LOG(const char *event_name, struct exynos_drm_crtc *exynos_crtc,
		u32 flag, void *fmt, ...)
{
	struct dpu_debug *d;
	struct dpu_memlog_event *event_log;
	char buf[LOG_BUF_SIZE];
	va_list args;
	int n = 0;

	if (!exynos_crtc)
		return;

	d = &exynos_crtc->d;
	if (flag & EVENT_FLAG_FENCE)
		event_log = &d->memlog.fevent_log;
	else
		event_log = &d->memlog.event_log;

	if (!event_log->obj)
		return;

	if (is_event_repeated(event_name, event_log, flag))
		return;

	if (flag & EVENT_FLAG_ERROR)
		d->err_event_cnt++;

	n += scnprintf(buf + n, sizeof(buf) - n, "%s: %s\t",
			event_log->prefix, event_name);

	if (fmt) {
		if (flag & EVENT_FLAG_LONG) {
			dpu_data_to_string fp;

			va_start(args, (dpu_data_to_string)fmt);
			fp = (dpu_data_to_string)fmt;
			n += fp(va_arg(args, void*), buf + n, sizeof(buf) - n);
			va_end(args);
		} else {
			va_start(args, (const char *)fmt);
			n += vscnprintf(buf + n, sizeof(buf) - n, fmt, args);
			va_end(args);
		}
	}

	memlog_write_printf(event_log->obj, MEMLOG_LEVEL_INFO, "%s\n", buf);
}

static int dpu_debug_err_event_show(struct seq_file *s, void *unused)
{
	struct exynos_drm_crtc *exynos_crtc = s->private;
	const struct dpu_debug *d = &exynos_crtc->d;

	seq_printf(s, "%d\n", d->err_event_cnt);
	return 0;
}

static int dpu_debug_err_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, dpu_debug_err_event_show, inode->i_private);
}

static ssize_t dpu_debug_err_event_write(struct file *file, const char __user *buf,
		size_t count, loff_t *f_ops)
{
	struct seq_file *s = file->private_data;
	struct exynos_drm_crtc *exynos_crtc = s->private;

	exynos_crtc->d.err_event_cnt = 0;

	return count;
}

static const struct file_operations dpu_err_event_fops = {
	.open = dpu_debug_err_event_open,
	.read = seq_read,
	.write = dpu_debug_err_event_write,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int recovery_show(struct seq_file *s, void *unused)
{
	return 0;
}

static int recovery_open(struct inode *inode, struct file *file)
{
	return single_open(file, recovery_show, inode->i_private);
}

static ssize_t recovery_write(struct file *file, const char *user_buf,
			      size_t count, loff_t *f_pos)
{
	struct seq_file *s = file->private_data;
	struct exynos_drm_crtc *exynos_crtc = s->private;
	char local_buf[256];
	size_t size;
	u32 req;

	size = min(sizeof(local_buf) - 1, count);
	if (copy_from_user(local_buf, user_buf, size)) {
		pr_warn("failed to copy from userbuf\n");
		return count;
	}

	local_buf[size] = '\0';

	if (kstrtou32(local_buf, 0, &req) != 0 ||
			!exynos_crtc->ops->recovery)
		return count;

	switch (req) {
		case 0:
			exynos_crtc->ops->recovery(exynos_crtc,
					"dsim");
			break;
		case 2:
			exynos_crtc->ops->recovery(exynos_crtc,
					"customer");
			break;
		case 3:
			exynos_crtc->ops->recovery(exynos_crtc,
					"undefined|force|dsim|customer");
			break;
		default:
			exynos_crtc->ops->recovery(exynos_crtc,
					"force");
	}

	return count;
}

static const struct file_operations recovery_fops = {
	.open = recovery_open,
	.read = seq_read,
	.write = recovery_write,
	.llseek = seq_lseek,
	.release = seq_release,
};

void dpu_profile_hiber_enter(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;

	if (!profile->started)
		return;

	profile->hiber_entry_time = ktime_get();
	profile->profile_enter_cnt++;
}

void dpu_profile_hiber_exit(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;

	if (!profile->started)
		return;

	profile->hiber_time += ktime_us_delta(ktime_get(), profile->hiber_entry_time);
	profile->profile_exit_cnt++;
}

static int dpu_get_hiber_ratio(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;
	s64 residency = profile->hiber_time;

	if (!residency)
		return 0;

	residency *= 100;
	do_div(residency, profile->profile_time);

	return residency;
}

static void _dpu_profile_hiber_show(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;

	if (profile->started) {
		pr_info("%s: hibernation profiling is ongoing\n", __func__);
		return;
	}

	pr_info("#########################################\n");
	pr_info("Profiling Time: %llu us\n", profile->profile_time);
	pr_info("Hibernation Entry Time: %llu us\n", profile->hiber_time);
	pr_info("Hibernation Entry Ratio: %d %%\n", dpu_get_hiber_ratio(exynos_crtc));
	pr_info("Entry count: %d, Exit count: %d\n", profile->profile_enter_cnt,
			profile->profile_exit_cnt);
	pr_info("Framedone count: %d, FPS: %lld\n", profile->frame_cnt,
			(profile->frame_cnt * 1000000) / profile->profile_time);
	pr_info("#########################################\n");
}

static int dpu_profile_hiber_show(struct seq_file *s, void *unused)
{
	struct exynos_drm_crtc *exynos_crtc = s->private;
	struct drm_printer p = drm_seq_file_printer(s);

	if (!exynos_crtc->hibernation) {
		drm_printf(&p, "decon%d is not support hibernation\n",
				drm_crtc_index(&exynos_crtc->base));
		return 0;
	}

	_dpu_profile_hiber_show(exynos_crtc);

	return 0;
}

static int dpu_profile_hiber_open(struct inode *inode, struct file *file)
{
	return single_open(file, dpu_profile_hiber_show, inode->i_private);
}

static void dpu_profile_hiber_start(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;

	if (profile->started) {
		pr_err("%s: hibernation profiling is ongoing\n", __func__);
		return;
	}

	/* reset profiling variables */
	memset(profile, 0, sizeof(*profile));

	/* profiling is just started */
	profile->profile_start_time = ktime_get();
	profile->started = true;

	/* hibernation status when profiling is started */
	if (IS_DECON_HIBER_STATE(exynos_crtc))
		dpu_profile_hiber_enter(exynos_crtc);

	pr_info("display hibernation profiling is started\n");
}

static void dpu_profile_hiber_finish(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_hiber_profile *profile = &exynos_crtc->hibernation->profile;

	if (!profile->started) {
		pr_err("%s: hibernation profiling is not started\n", __func__);
		return;
	}

	profile->profile_time = ktime_us_delta(ktime_get(),
			profile->profile_start_time);

	/* hibernation status when profiling is finished */
	if (IS_DECON_HIBER_STATE(exynos_crtc))
		dpu_profile_hiber_exit(exynos_crtc);

	profile->started = false;

	_dpu_profile_hiber_show(exynos_crtc);

	pr_info("display hibernation profiling is finished\n");
}

static ssize_t dpu_profile_hiber_write(struct file *file, const char __user *buf,
		size_t count, loff_t *f_ops)
{
	char *buf_data;
	int ret;
	int input;
	struct seq_file *s = file->private_data;
	struct exynos_drm_crtc *exynos_crtc = s->private;

	if (!count)
		return count;

	buf_data = kmalloc(count, GFP_KERNEL);
	if (buf_data == NULL)
		goto out_cnt;

	ret = copy_from_user(buf_data, buf, count);
	if (ret < 0)
		goto out;

	ret = sscanf(buf_data, "%u", &input);
	if (ret < 0)
		goto out;

	if (input)
		dpu_profile_hiber_start(exynos_crtc);
	else
		dpu_profile_hiber_finish(exynos_crtc);

out:
	kfree(buf_data);
out_cnt:
	return count;
}

static const struct file_operations dpu_profile_hiber_fops = {
	.open = dpu_profile_hiber_open,
	.write = dpu_profile_hiber_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static void dpu_init_memlogger(struct exynos_drm_crtc *exynos_crtc);
int dpu_init_debug(struct exynos_drm_crtc *exynos_crtc)
{
	struct drm_crtc *crtc;

	crtc = &exynos_crtc->base;

	debugfs_create_file("err_event", 0664, crtc->debugfs_entry, exynos_crtc,
			&dpu_err_event_fops);

	debugfs_create_file("recovery", 0644, crtc->debugfs_entry, exynos_crtc,
			&recovery_fops);

	debugfs_create_u32("underrun_cnt", 0664, crtc->debugfs_entry,
			&exynos_crtc->d.underrun_cnt);

	debugfs_create_file("profile_hiber", 0444, crtc->debugfs_entry,
			exynos_crtc, &dpu_profile_hiber_fops);

	dpu_init_memlogger(exynos_crtc);

	return 0;
}

int dpu_deinit_debug(struct exynos_drm_crtc *exynos_crtc)
{
	if (exynos_crtc->d.memlog.desc)
		memlog_unregister(exynos_crtc->d.memlog.desc);

	return 0;
}

#define PREFIX_LEN	40
#define ROW_LEN		32
void dpu_print_hex_dump(void __iomem *regs, const void *buf, size_t len)
{
	char prefix_buf[PREFIX_LEN];
	unsigned long p;
	int i, row;

	for (i = 0; i < len; i += ROW_LEN) {
		p = buf - regs + i;

		if (len - i < ROW_LEN)
			row = len - i;
		else
			row = ROW_LEN;

		snprintf(prefix_buf, sizeof(prefix_buf), "[%08lX] ", p);
		print_hex_dump(KERN_INFO, prefix_buf, DUMP_PREFIX_NONE,
				32, 4, buf + i, row, false);
	}
}

void dpu_print_eint_state(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (!exynos_crtc->d.eint_pend)
		return;

	DRM_INFO("%s: eint pend state(0x%x)\n", __func__,
			readl(exynos_crtc->d.eint_pend));
}

void dpu_check_panel_status(struct drm_crtc *crtc)
{
	struct drm_connector *connector =
		crtc_get_conn(crtc->state, DRM_MODE_CONNECTOR_DSI);

	if (connector) {
		struct exynos_drm_connector *exynos_conn =
			to_exynos_connector(connector);
		const struct exynos_drm_connector_funcs *funcs = exynos_conn->funcs;

		if (funcs && funcs->query_status)
			funcs->query_status(exynos_conn);
	}
}

void dpu_dump(struct exynos_drm_crtc *exynos_crtc)
{
	bool active;

	active = pm_runtime_active(exynos_crtc->dev);
	pr_info("DPU power %s state\n", active ? "on" : "off");

	if (active && exynos_crtc->ops->dump_register)
		exynos_crtc->ops->dump_register(exynos_crtc);
}

int dpu_sysmmu_fault_handler(struct iommu_fault *fault, void *data)
{
	struct exynos_drm_crtc *exynos_crtc = data;
	pr_info("%s +\n", __func__);

	dpu_dump(exynos_crtc);
	dbg_snapshot_expire_watchdog();

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
int dpu_itmon_notifier(struct notifier_block *nb, unsigned long act, void *data)
{
	struct dpu_debug *d;
	struct exynos_drm_crtc *exynos_crtc;
	struct itmon_notifier *itmon_data = data;
	struct drm_printer p;
	static bool is_dumped = false;

	d = container_of(nb, struct dpu_debug, itmon_nb);
	exynos_crtc = container_of(d, struct exynos_drm_crtc, d);
	p = drm_info_printer(exynos_crtc->dev);

	if (d->itmon_notified)
		return NOTIFY_DONE;

	if (IS_ERR_OR_NULL(itmon_data))
		return NOTIFY_DONE;

	if (is_dumped)
		return NOTIFY_DONE;

	/* port is master and dest is target */
	if ((itmon_data->port &&
		(strncmp("DPU", itmon_data->port, sizeof("DPU") - 1) == 0)) ||
		(itmon_data->dest &&
		(strncmp("DPU", itmon_data->dest, sizeof("DPU") - 1) == 0))) {

		pr_info("%s: DECON%d +\n", __func__, exynos_crtc->base.index);

		pr_info("%s: port: %s, dest: %s\n", __func__,
				itmon_data->port, itmon_data->dest);

		dpu_dump(exynos_crtc);

		d->itmon_notified = true;
		is_dumped = true;
		pr_info("%s -\n", __func__);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_DRM_BUFFER_SANITY_CHECK)
#define BUFFER_SANITY_CHECK_SIZE (2048UL)
static u64 get_buffer_checksum(struct exynos_drm_gem *exynos_gem)
{
	struct drm_gem_object *gem = &exynos_gem->base;
	u64 checksum64 = 0;
	size_t i, size;

	size = gem->size;
	if (size > BUFFER_SANITY_CHECK_SIZE)
		size = BUFFER_SANITY_CHECK_SIZE;

	for (i = 0; i + sizeof(u64) < size; i += sizeof(u64))
		checksum64 += *((u64 *)(exynos_gem->vaddr + i));

	return checksum64;
}

void
exynos_atomic_commit_prepare_buf_sanity(struct drm_atomic_state *old_state)
{
	int i, j;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *new_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	struct drm_framebuffer *fb;
	struct drm_gem_object **gem;
	struct exynos_drm_gem *exynos_gem;

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		if (crtc->index == 0)
			break;
	}
	if (!crtc || crtc->index)
		return;

	for_each_new_plane_in_state(old_state, plane, new_plane_state, i) {
		fb = new_plane_state->fb;
		if (!fb)
			continue;
		gem = fb->obj;
		for (j = 0; gem[j]; j++) {
			exynos_gem = to_exynos_gem(gem[j]);
			if (!exynos_gem->vaddr)
				continue;

			exynos_gem->checksum64 = get_buffer_checksum(exynos_gem);
			pr_debug("%s: ch(%lu)[%d] fd(%d) checksum(%#llx)\n",
				__func__, plane->index, j, exynos_gem->acq_fence,
				exynos_gem->checksum64);
		}
	}
}

#define SANITY_DFT_FPS	(60)
bool exynos_atomic_commit_check_buf_sanity(struct drm_atomic_state *old_state)
{
	int i, j;
	int fps;
	u64 checksum64;
	bool sanity_ok = true;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *new_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	struct drm_framebuffer *fb;
	struct drm_gem_object **gem;
	struct exynos_drm_gem *exynos_gem;

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		if (crtc->index == 0)
			break;
	}
	if (!crtc || crtc->index)
		return sanity_ok;

	fps = drm_mode_vrefresh(&new_crtc_state->adjusted_mode);
	if (!fps)
		fps = SANITY_DFT_FPS;

	usleep_range(USEC_PER_SEC/fps/4, USEC_PER_SEC/fps/4 + 10);

	for_each_new_plane_in_state(old_state, plane, new_plane_state, i) {
		fb = new_plane_state->fb;
		if (!fb)
			continue;
		gem = fb->obj;
		for (j = 0; gem[j]; j++) {
			exynos_gem = to_exynos_gem(gem[j]);
			if (!exynos_gem->vaddr)
				continue;

			checksum64 = get_buffer_checksum(exynos_gem);

			sanity_ok = (exynos_gem->checksum64 == checksum64);

			if (sanity_ok) {
				pr_debug("%s: ch(%lu)[%d] fd(%d) checksum(%#llx)\n",
						__func__, plane->index, j,
						exynos_gem->acq_fence,
						exynos_gem->checksum64);
			} else {
				pr_err("%s: sanity check error\n", __func__);
				pr_err("%s: ch(%lu)[%d] fd(%d) checksum(%#llx)\n",
						__func__, plane->index, j,
						exynos_gem->acq_fence,
						exynos_gem->checksum64);
			}
		}
	}

	return sanity_ok;
}
#endif

static int dpu_memlog_ops_dummy(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static const struct memlog_ops dpu_memlog_ops = {
	.file_ops_completed = dpu_memlog_ops_dummy,
	.log_status_notify = dpu_memlog_ops_dummy,
	.log_level_notify = dpu_memlog_ops_dummy,
	.log_enable_notify = dpu_memlog_ops_dummy,
};

#define DPU_MEMLOG_SIZE		(SZ_256K)
#define DPU_ERRMEMLOG_SIZE	(SZ_32K)
#define DPU_EVENT_MEMLOG_SIZE	(SZ_32K)
#define DPU_FEVENT_MEMLOG_SIZE	(SZ_16K)
static void dpu_init_memlogger(struct exynos_drm_crtc *exynos_crtc)
{
	char dev_name[10];
	struct dpu_memlog *memlog = &exynos_crtc->d.memlog;
	int ret;

	scnprintf(dev_name, sizeof(dev_name), "DPU%d",
			drm_crtc_index(&exynos_crtc->base));
	ret = memlog_register(dev_name, exynos_crtc->dev, &memlog->desc);
	if (ret) {
		pr_info("%s: failed to register memlog(%d)\n", __func__, ret);
		goto err;
	}

	memlog->desc->ops = dpu_memlog_ops;

	if (!g_log_obj) {
		g_log_obj = memlog_alloc_printf(memlog->desc, DPU_MEMLOG_SIZE,
				NULL, "log-mem0", 0);
		if (!g_log_obj) {
			pr_info("%s: failed to alloc dev memlog memory for log\n",
					__func__);
			goto err;
		}
	}

	if (!g_errlog_obj) {
		g_errlog_obj = memlog_alloc_printf(memlog->desc, DPU_ERRMEMLOG_SIZE,
				NULL, "log-mem1", 0);
		if (!g_errlog_obj) {
			pr_info("%s: failed to alloc dev err memlog memory for log\n",
					__func__);
			goto err;
		}
	}

	memlog->event_log.obj = memlog_alloc_printf(memlog->desc,
			DPU_EVENT_MEMLOG_SIZE, NULL, "evt-mem", 0);
	if (!memlog->event_log.obj) {
		pr_info("%s: failed to alloc dev memlog for event log\n", __func__);
		goto err;
	}
	spin_lock_init(&memlog->event_log.slock);
	scnprintf(memlog->event_log.prefix, DPU_EVENT_MAX_LEN, "[DECON%d] EVENT",
			drm_crtc_index(&exynos_crtc->base));

	memlog->fevent_log.obj = memlog_alloc_printf(memlog->desc,
			DPU_FEVENT_MEMLOG_SIZE, NULL, "fevt-mem", 0);
	if (!memlog->fevent_log.obj) {
		pr_info("%s: failed to alloc dev memlog for fevent log\n", __func__);
		goto err;
	}
	spin_lock_init(&memlog->fevent_log.slock);
	scnprintf(memlog->fevent_log.prefix, DPU_EVENT_MAX_LEN, "[DECON%d] FEVENT",
			drm_crtc_index(&exynos_crtc->base));

	pr_info("%s: successfully registered\n", __func__);

	return;
err:
	pr_err("%s: failed\n", __func__);
}
