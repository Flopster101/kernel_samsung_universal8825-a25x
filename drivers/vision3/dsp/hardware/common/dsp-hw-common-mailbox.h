/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_MAILBOX_H__
#define __HW_COMMON_DSP_HW_COMMON_MAILBOX_H__

#include "dsp-mailbox.h"

struct dsp_mailbox_pool *dsp_hw_common_mailbox_alloc_pool(
		struct dsp_mailbox *mbox, unsigned int size);
void dsp_hw_common_mailbox_free_pool(struct dsp_mailbox *mbox,
		struct dsp_mailbox_pool *pool);
void dsp_hw_common_mailbox_dump_pool(struct dsp_mailbox *mbox,
		struct dsp_mailbox_pool *pool);
int dsp_hw_common_mailbox_send_message(struct dsp_mailbox *mbox,
		unsigned int message_id);
int dsp_hw_common_mailbox_send_task(struct dsp_mailbox *mbox,
		struct dsp_task *task);
int dsp_hw_common_mailbox_receive_task(struct dsp_mailbox *mbox);
int dsp_hw_common_mailbox_check_version(struct dsp_mailbox *mbox,
		unsigned int mbox_ver, unsigned int msg_ver);

int dsp_hw_common_mailbox_stop(struct dsp_mailbox *mbox);
int dsp_hw_common_mailbox_open(struct dsp_mailbox *mbox);

int dsp_hw_common_mailbox_close(struct dsp_mailbox *mbox);
int dsp_hw_common_mailbox_probe(struct dsp_mailbox *mbox, void *sys);
void dsp_hw_common_mailbox_remove(struct dsp_mailbox *mbox);

int dsp_hw_common_mailbox_set_ops(struct dsp_mailbox *mbox, const void *ops);

#endif
