/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_DUMP_H__
#define __DSP_DUMP_H__

#include <linux/seq_file.h>

void dsp_dump_set_value(unsigned int dump_value);
void dsp_dump_print_value(void);
void dsp_dump_print_status_user(struct seq_file *file);

void dsp_dump_task_count(void);
void dsp_dump_kernel(void);

/* hardware */
void dsp_dump_ctrl(void);
void dsp_dump_ctrl_user(struct seq_file *file);
void dsp_dump_mailbox_pool_error(void *pool);
void dsp_dump_mailbox_pool_debug(void *pool);

#endif
