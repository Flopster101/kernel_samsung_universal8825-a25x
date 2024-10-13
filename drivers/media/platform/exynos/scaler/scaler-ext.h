/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#ifndef SCALER_EXT_H_
#define SCALER_EXT_H_

#include <linux/completion.h>
#include <linux/dma-direction.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscdevice.h>

#include "scaler.h"

#define SC_EXT_DEV_NAME	"scaler_ext"

enum sc_ext_task_state {
	SC_EXT_BUFSTATE_READY,
	SC_EXT_BUFSTATE_PROCESSING,
	SC_EXT_BUFSTATE_DONE,
	SC_EXT_BUFSTATE_ERROR,
};

struct sc_ext_dma {
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
	int offset;
};

struct sc_ext_format {
	int format;
	int plane_num;
	int vshift;	/* log2(horizontal chroma subsampling factor) */
	int hshift;	/* log2(vertical chroma subsampling factor) */
	int nr_chroma[3];
	const char *name;
};

struct sc_ext_buf {
	uint32_t count;
	const struct sc_ext_format *fmt;
	struct sc_ext_dma dma[MSCL_MAX_PLANES];
};

struct sc_ext_task_data {
	uint32_t cmd[MSCL_NR_CMDS];
	struct sc_ext_buf buf[MSCL_NR_DIRS];
};

struct sc_ext_task {
	uint32_t task_count;
	uint32_t curr_count;
	struct sc_ext_task_data *data;
	struct sc_ext_ctx *xctx;
	struct completion complete;
	enum sc_ext_task_state state;
};

struct sc_ext_dev {
	struct miscdevice misc;
	struct list_head contexts;
	struct device *dev;
	spinlock_t lock_task;
	spinlock_t lock_ctx;
	struct mutex lock_ioctl;
	struct sc_ext_task *current_task;
};

struct sc_ext_ctx {
	struct sc_ctx sc_ctx;
	struct list_head node;
	struct sc_ext_dev *xdev;
	struct sc_ext_task *task;
};

#endif /* SCALER_EXT_H_ */
