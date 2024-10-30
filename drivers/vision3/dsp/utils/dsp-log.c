// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 */

#include <linux/printk.h>
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/memlogger.h>
#endif

#include "dsp-log.h"

#define DSP_LOG_TAG	"exynos-dsp"

static unsigned int dsp_log_debug_ctrl;
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
static struct memlog_obj *dsp_memlog_log_obj;
#else
#define MEMLOG_LEVEL_EMERG		(0)
#define MEMLOG_LEVEL_ERR		(0)
#define MEMLOG_LEVEL_CAUTION		(0)
#define MEMLOG_LEVEL_NOTICE		(0)
#define MEMLOG_LEVEL_INFO		(0)
#define MEMLOG_LEVEL_DEBUG		(0)
#endif

void dsp_log_set_debug_ctrl(unsigned int val)
{
	dsp_check();
	dsp_log_debug_ctrl = val;
}

unsigned int dsp_log_get_debug_ctrl(void)
{
	dsp_check();
	return dsp_log_debug_ctrl;
}

static void __dsp_log_memlog_write_printf(int level, const char *fmt, ...)
{
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	if (dsp_memlog_log_obj) {
		struct va_format vaf;
		va_list args;

		va_start(args, fmt);

		vaf.fmt = fmt;
		vaf.va = &args;

		memlog_write_printf(dsp_memlog_log_obj, level, "%pV", &vaf);

		va_end(args);
	}
#endif
}

void dsp_log_memlog_init(void *obj)
{
	dsp_enter();
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	dsp_memlog_log_obj = obj;
#endif
	dsp_leave();
}

void dsp_log_err(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_err("%s: ERR:%pV", DSP_LOG_TAG, &vaf);
	__dsp_log_memlog_write_printf(MEMLOG_LEVEL_ERR,
			"%s: ERR:%pV", DSP_LOG_TAG, &vaf);

	va_end(args);
}

void dsp_log_warn(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_warn("%s: WAR:%pV", DSP_LOG_TAG, &vaf);
	__dsp_log_memlog_write_printf(MEMLOG_LEVEL_CAUTION,
			"%s: WAR:%pV", DSP_LOG_TAG, &vaf);

	va_end(args);
}

void dsp_log_notice(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_notice("%s: NOT:%pV", DSP_LOG_TAG, &vaf);
	__dsp_log_memlog_write_printf(MEMLOG_LEVEL_CAUTION,
			"%s: NOT:%pV", DSP_LOG_TAG, &vaf);

	va_end(args);
}

void dsp_log_info(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

#if defined(ENABLE_INFO_LOG)
	pr_info("%s: INF:%pV", DSP_LOG_TAG, &vaf);
	__dsp_log_memlog_write_printf(MEMLOG_LEVEL_CAUTION,
			"%s: INF:%pV", DSP_LOG_TAG, &vaf);
#else
	__dsp_log_memlog_write_printf(MEMLOG_LEVEL_NOTICE,
			"%s: INF:%pV", DSP_LOG_TAG, &vaf);
#endif

	va_end(args);
}

void dsp_log_dbg(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

#if defined(ENABLE_DYNAMIC_DEBUG_LOG)
	if (dsp_log_debug_ctrl & 0x1)
		pr_info("%s: DBG:%pV", DSP_LOG_TAG, &vaf);
	else
		pr_debug("%s: DBG:%pV", DSP_LOG_TAG, &vaf);
#else
	pr_debug("%s: DBG:%pV", DSP_LOG_TAG, &vaf);
#endif

	__dsp_log_memlog_write_printf(MEMLOG_LEVEL_DEBUG,
			"%s: DBG:%pV", DSP_LOG_TAG, &vaf);

	va_end(args);
}

void dsp_log_dl_dbg(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

#if defined(ENABLE_DYNAMIC_DEBUG_LOG)
	if (dsp_log_debug_ctrl & 0x2)
		pr_info("%s: DL-DBG:%pV", DSP_LOG_TAG, &vaf);
	else
		pr_debug("%s: DL-DBG:%pV", DSP_LOG_TAG, &vaf);
#else
	pr_debug("%s: DL-DBG:%pV", DSP_LOG_TAG, &vaf);
#endif

	__dsp_log_memlog_write_printf(MEMLOG_LEVEL_DEBUG,
			"%s: DL-DBG:%pV", DSP_LOG_TAG, &vaf);

	va_end(args);
}
