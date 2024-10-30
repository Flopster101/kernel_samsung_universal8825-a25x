/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for DPU debug.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_DEBUG_H__
#define __EXYNOS_DRM_DEBUG_H__

#include <linux/iommu.h>
#include <drm/drm_rect.h>
#include <exynos_drm_modifier.h>
#include <exynos_drm_bts.h>

#include <soc/samsung/memlogger.h>

/**
 * Display Subsystem event management status.
 *
 * These status labels are used internally by the DECON to indicate the
 * current status of a device with operations.
 */

/**
 * FENCE_FMT - printf string for &struct dma_fence
 */
#define FENCE_FMT "name:%-15s timeline:%-20s ctx:%-5llu seqno:%-5u flags:%#x"
/**
 * FENCE_ARG - printf arguments for &struct dma_fence
 * @f: dma_fence struct
 */
#define FENCE_ARG(f)	(f)->ops->get_driver_name((f)),		\
			(f)->ops->get_timeline_name((f)),	\
			(f)->context, (f)->seqno, (f)->flags

#define STATE_FMT "enable(%d) active(%d) [p:%d m:%d a:%d c:%d]"

#define STATE_ARG(s) (s)->enable, (s)->active, (s)->planes_changed, \
	(s)->mode_changed, (s)->active_changed, (s)->color_mgmt_changed

typedef size_t (*dpu_data_to_string)(void *src, void *buf, size_t remained);

struct drm_crtc;
struct exynos_drm_crtc;
int dpu_init_debug(struct exynos_drm_crtc *exynos_crtc);
int dpu_deinit_debug(struct exynos_drm_crtc *exynos_crtc);

size_t dpu_rsc_ch_to_string(void *src, void *buf, size_t remained);
size_t dpu_rsc_win_to_string(void *src, void *buf, size_t remained);
size_t dpu_config_to_string(void *src, void *buf, size_t remained);
#define EVENT_FLAG_REPEAT	(1 << 0)
#define EVENT_FLAG_ERROR	(1 << 1)
#define EVENT_FLAG_FENCE	(1 << 2)
#define EVENT_FLAG_LONG		(1 << 3)
#define EVENT_FLAG_MASK		(EVENT_FLAG_REPEAT | EVENT_FLAG_ERROR |\
				EVENT_FLAG_FENCE | EVENT_FLAG_LONG)
void DPU_EVENT_LOG(const char *name, struct exynos_drm_crtc *exynos_crtc,
		u32 flag, void *arg, ...);

void dpu_print_eint_state(struct drm_crtc *crtc);
void dpu_check_panel_status(struct drm_crtc *crtc);
void dpu_dump(struct exynos_drm_crtc *exynos_crtc);
int dpu_sysmmu_fault_handler(struct iommu_fault *fault, void *data);
void dpu_profile_hiber_enter(struct exynos_drm_crtc *exynos_crtc);
void dpu_profile_hiber_exit(struct exynos_drm_crtc *exynos_crtc);
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
int dpu_itmon_notifier(struct notifier_block *nb, unsigned long action,
		void *data);
#endif

static __always_inline const char *get_comp_src_name(u64 comp_src)
{
	if (comp_src == AFBC_FORMAT_MOD_SOURCE_GPU)
		return "GPU";
	else if (comp_src == AFBC_FORMAT_MOD_SOURCE_G2D)
		return "G2D";
	else
		return "";
}

struct memlog;
struct memlog_obj;

#define DPU_EVENT_MAX_LEN	(25)
#define DPU_EVENT_KEEP_CNT	(3)
struct dpu_memlog_event {
	struct memlog_obj *obj;
	spinlock_t slock;
	size_t last_event_len;
	char last_event[DPU_EVENT_MAX_LEN];
	char prefix[DPU_EVENT_MAX_LEN];
	u32 repeat_cnt;
};

struct dpu_memlog {
	struct memlog *desc;
	struct dpu_memlog_event event_log;
	struct dpu_memlog_event fevent_log;
};

struct dpu_debug {
	/* ioremap region to read TE PEND register */
	void __iomem *eint_pend;
	/* count of shodow update timeout */
	atomic_t shadow_timeout_cnt;

	/* count of underrun interrupt */
	u32 underrun_cnt;

	u32 err_event_cnt;

	struct dpu_memlog memlog;

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	struct notifier_block itmon_nb;
	bool itmon_notified;
#endif
};

#if IS_ENABLED(CONFIG_EXYNOS_DRM_BUFFER_SANITY_CHECK)
void
exynos_atomic_commit_prepare_buf_sanity(struct drm_atomic_state *old_state);
bool exynos_atomic_commit_check_buf_sanity(struct drm_atomic_state *old_state);
#endif

extern struct memlog_obj *g_log_obj;
extern struct memlog_obj *g_errlog_obj;
#define DPU_PR_PREFIX		""
#define DPU_PR_FMT		"%s[%d]: %s:%d "
#define DPU_PR_ARG(name, id)	(name), (id), __func__, __LINE__

#define drv_name(d) (((d) && (d)->dev && (d)->dev->driver) ?			\
			(d)->dev->driver->name : "unregistered")
#define dpu_pr_memlog(handle, name, id, memlog_lv, fmt, ...)			\
	do {									\
		if ((handle) && ((handle)->enabled))				\
			memlog_write_printf((handle), (memlog_lv),		\
				DPU_PR_PREFIX DPU_PR_FMT fmt,			\
				DPU_PR_ARG((name), (id)), ##__VA_ARGS__);	\
	} while (0)

#define dpu_pr_err(name, id, log_lv, fmt, ...)					\
	do {									\
		if ((log_lv) >= 3)						\
			pr_err(DPU_PR_PREFIX DPU_PR_FMT fmt,			\
				DPU_PR_ARG((name), (id)), ##__VA_ARGS__);	\
										\
		dpu_pr_memlog((g_log_obj), (name), (id), MEMLOG_LEVEL_ERR,	\
				fmt, ##__VA_ARGS__);				\
		dpu_pr_memlog((g_errlog_obj), (name), (id), MEMLOG_LEVEL_ERR,	\
				fmt, ##__VA_ARGS__);				\
	} while (0)

#define dpu_pr_warn(name, id, log_lv, fmt, ...)					\
	do {									\
		if ((log_lv) >= 5)						\
			pr_warn(DPU_PR_PREFIX DPU_PR_FMT fmt,			\
				DPU_PR_ARG((name), (id)), ##__VA_ARGS__);	\
										\
		dpu_pr_memlog(g_log_obj, name, id, MEMLOG_LEVEL_CAUTION,	\
				fmt, ##__VA_ARGS__);				\
	} while (0)

#define dpu_pr_info(name, id, log_lv, fmt, ...)					\
	do {									\
		if ((log_lv) >= 6)						\
			pr_info(DPU_PR_PREFIX DPU_PR_FMT fmt,			\
				DPU_PR_ARG((name), (id)), ##__VA_ARGS__);	\
										\
		dpu_pr_memlog((g_log_obj), (name), (id), MEMLOG_LEVEL_INFO,	\
				fmt, ##__VA_ARGS__);				\
	} while (0)

#define dpu_pr_debug(name, id, log_lv, fmt, ...)				\
	do {									\
		if ((log_lv) >= 7)						\
			pr_debug(DPU_PR_PREFIX DPU_PR_FMT fmt,			\
				DPU_PR_ARG((name), (id)), ##__VA_ARGS__);	\
										\
		dpu_pr_memlog((g_log_obj), (name), (id), MEMLOG_LEVEL_DEBUG,	\
				fmt, ##__VA_ARGS__);				\
	} while (0)

#endif /* __EXYNOS_DRM_DEBUG_H__ */
