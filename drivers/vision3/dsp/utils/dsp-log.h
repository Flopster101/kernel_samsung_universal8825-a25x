/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_LOG_H__
#define __DSP_LOG_H__

#include <linux/types.h>

#include "dsp-config.h"

#define dsp_err(fmt, args...)		\
	dsp_log_err("%4d:" fmt, __LINE__, ##args)
#define dsp_warn(fmt, args...)		\
	dsp_log_warn("%4d:" fmt, __LINE__, ##args)
#define dsp_notice(fmt, args...)	\
	dsp_log_notice("%4d:" fmt, __LINE__, ##args)
#define dsp_info(fmt, args...)		\
	dsp_log_info("%4d:" fmt, __LINE__, ##args)
#define dsp_dbg(fmt, args...)		\
	dsp_log_dbg("%4d:" fmt, __LINE__, ##args)
#define dsp_dl_dbg(fmt, args...)	\
	dsp_log_dl_dbg("%4d:" fmt, __LINE__, ##args)

#if defined(ENABLE_CALL_PATH_LOG)
#define dsp_enter()			dsp_info("[%s] enter\n", __func__)
#define dsp_leave()			dsp_info("[%s] leave\n", __func__)
#define dsp_check()			dsp_info("[%s] check\n", __func__)
#else
#define dsp_enter()
#define dsp_leave()
#define dsp_check()
#endif

void dsp_log_set_debug_ctrl(unsigned int val);
unsigned int dsp_log_get_debug_ctrl(void);
void dsp_log_memlog_init(void *obj);

void dsp_log_err(const char *fmt, ...);
void dsp_log_warn(const char *fmt, ...);
void dsp_log_notice(const char *fmt, ...);
void dsp_log_info(const char *fmt, ...);
void dsp_log_dbg(const char *fmt, ...);
void dsp_log_dl_dbg(const char *fmt, ...);

#endif  // __DSP_LOG_H__
