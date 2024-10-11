// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/iommu.h>
#include <linux/dma-iommu.h>
#include <linux/ion.h>
#include <linux/dma-buf.h>

#include "scaler.h"
#include "scaler-ext.h"
#include "scaler-regs.h"

extern int sc_show_stat;

static int sc_ext_cmd_offset[MSCL_NR_CMDS] = {
	[MSCL_SRC_CFG] = SCALER_SRC_CFG,
	[MSCL_SRC_WH] = SCALER_SRC_WH,
	[MSCL_SRC_SPAN] = SCALER_SRC_SPAN,
	[MSCL_SRC_YPOS] = SCALER_SRC_Y_POS,
	[MSCL_SRC_CPOS] = SCALER_SRC_C_POS,
	[MSCL_DST_CFG] = SCALER_DST_CFG,
	[MSCL_DST_WH] = SCALER_DST_WH,
	[MSCL_DST_SPAN] = SCALER_DST_SPAN,
	[MSCL_DST_POS] = SCALER_DST_POS,
	[MSCL_V_RATIO] = SCALER_V_RATIO,
	[MSCL_H_RATIO] = SCALER_H_RATIO,
	[MSCL_ROT_CFG] = SCALER_ROT_CFG,
	[MSCL_SRC_YH_IPHASE] = SCALER_SRC_YH_INIT_PHASE,
	[MSCL_SRC_YV_IPHASE] = SCALER_SRC_YV_INIT_PHASE,
	[MSCL_SRC_CH_IPHASE] = SCALER_SRC_CH_INIT_PHASE,
	[MSCL_SRC_CV_IPHASE] = SCALER_SRC_CV_INIT_PHASE,
};

static int sc_ext_buf_offset[MSCL_NR_DIRS][MSCL_MAX_PLANES] = {
	{ SCALER_SRC_Y_BASE, SCALER_SRC_CB_BASE, SCALER_SRC_CR_BASE, },
	{ SCALER_DST_Y_BASE, SCALER_DST_CB_BASE, SCALER_DST_CR_BASE, },
};

#define SC_EXT_FORMAT_NV12	0
#define SC_EXT_FORMAT_NV21	16
#define SC_EXT_FORMAT_YUYV	10

static const struct sc_ext_format sc_ext_supported_formats[] = {
	{ SC_EXT_FORMAT_NV12, 2, 1, 1, {0, 2, 0}, "NV12" },
	{ SC_EXT_FORMAT_NV21, 2, 1, 1, {0, 2, 0}, "NV21" },
	{ SC_EXT_FORMAT_YUYV, 1, 1, 0, {2, 0, 0}, "YUYV" },
};

static inline const char *sc_ext_get_format_name(uint32_t fmt)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(sc_ext_supported_formats); i++) {
		if (sc_ext_supported_formats[i].format == fmt)
			return sc_ext_supported_formats[i].name;
	}

	return "Unknown";
}

/* debugging information begin */
char *cmd_str[] = {
	"MSCL_SRC_CFG",
	"MSCL_SRC_WH",
	"MSCL_SRC_SPAN",
	"MSCL_SRC_YPOS",
	"MSCL_SRC_CPOS",
	"MSCL_DST_CFG",
	"MSCL_DST_WH",
	"MSCL_DST_SPAN",
	"MSCL_DST_POS",
	"MSCL_V_RATIO",
	"MSCL_H_RATIO",
	"MSCL_ROT_CFG",
	"MSCL_SRC_YH_IPHASE",
	"MSCL_SRC_YV_IPHASE",
	"MSCL_SRC_CH_IPHASE",
	"MSCL_SRC_CV_IPHASE",
};

#define SC_CMD_GET_POS_X(val)	(((val) >> 16) & 0xffff)
#define SC_CMD_GET_POS_Y(val)	((val) & 0xffff)
#define SC_CMD_GET_WIDTH(val)	(((val) >> 16) & 0xffff)
#define SC_CMD_GET_HEIGHT(val)	((val) & 0xffff)
#define SC_CMD_GET_SPAN_Y(val)	((val) & 0xffff)
#define SC_CMD_GET_SPAN_C(val)	(((val) >> 16) & 0xffff)

static inline void show_cmd_line(int index, uint32_t value)
{
	char comment[50] = { 0, };

	switch (index) {
	case MSCL_SRC_CFG:
	case MSCL_DST_CFG:
		snprintf(comment, sizeof(comment),
			"[format:%s]", sc_ext_get_format_name(value));
		break;
	case MSCL_SRC_WH:
	case MSCL_DST_WH:
		snprintf(comment, sizeof(comment),
				"[width:%d, height:%d]",
				SC_CMD_GET_WIDTH(value),
				SC_CMD_GET_HEIGHT(value));
		break;
	case MSCL_SRC_YPOS:
	case MSCL_SRC_CPOS:
	case MSCL_DST_POS:
		snprintf(comment, sizeof(comment),
				"[x pos:%d, y pos:%d]",
				SC_CMD_GET_POS_X(value),
				SC_CMD_GET_POS_Y(value));
		break;
	case MSCL_SRC_SPAN:
	case MSCL_DST_SPAN:
		snprintf(comment, sizeof(comment),
				"[y span:%d, c span:%d]",
				SC_CMD_GET_SPAN_Y(value),
				SC_CMD_GET_SPAN_C(value));
		break;
	}

	pr_info("  [%20s:%#06x] : %#010x  %s\n",
		cmd_str[index], sc_ext_cmd_offset[index], value, comment);
}

static void show_task_cmd(uint32_t *cmd)
{
	int i;

	pr_info("  - command list (command name:offset)-\n");
	for (i = 0; i < MSCL_NR_CMDS; i++)
		show_cmd_line(i, cmd[i]);
}

static void sc_ext_show_task_dma(struct sc_ext_buf *buf, char *str)
{
	int i;

	pr_info("  - [%s] buffer\n", str);
	for (i = 0; i < buf->count; i++)
		pr_info("   Plane[%d] addr = (base %pad + offset %#lx)\n",
				i, &buf->dma[i].dma_addr, buf->dma[i].offset);
}

static void sc_ext_show_task_buf(struct sc_ext_task_data *data, int index)
{
	pr_info(" = task %#d =\n", index);
	sc_ext_show_task_dma(&data->buf[MSCL_SRC], "SRC");
	sc_ext_show_task_dma(&data->buf[MSCL_DST], "DST");
}

static void sc_ext_show_all_task_buf(struct sc_ext_task *task)
{
	int i;

	pr_info("++ extracted buffer information\n");
	for (i = 0; i < task->task_count; i++)
		sc_ext_show_task_buf(&task->data[i], i);
}

static void sc_ext_show_task_data(struct sc_ext_task_data *data, int index)
{
	pr_info(" = task %#d =\n", index);

	show_task_cmd(data->cmd);
	sc_ext_show_task_dma(&data->buf[MSCL_SRC], "SRC");
	sc_ext_show_task_dma(&data->buf[MSCL_DST], "DST");
}

static void show_mscl_buf(struct mscl_buffer *buf, char *str)
{
	int i;

	pr_info("  - [%s] buffer (count:%d, rsvd:%d) -\n",
				str, buf->count, buf->reserved);
	for (i = 0; i < buf->count; i++)
		pr_info("   Plane[%d] dmabuf: %#x, offset: %#x\n",
				i, buf->dmabuf[i], buf->offset[i]);
}

static void show_mscl_task(struct mscl_task *task, int index)
{
	pr_info(" = task #%d =\n", index);
	show_task_cmd(task->cmd);
	show_mscl_buf(&task->buf[MSCL_SRC], "SRC");
	show_mscl_buf(&task->buf[MSCL_DST], "DST");
}

static void sc_ext_show_mscl_task(struct mscl_task *task, int total)
{
	int i;

	pr_info("++ user task information, total task count: %d\n", total);
	for (i = 0; i < total; i++)
		show_mscl_task(&task[i], i);
}
/* debugging information end */

static inline struct sc_ext_ctx *sc_ctx_to_xctx(struct sc_ctx *sc_ctx)
{
	return container_of(sc_ctx, struct sc_ext_ctx, sc_ctx);
}

static void sc_ext_set_buffer(struct sc_dev *sc_dev,
				struct sc_ext_buf *buf, int dir)
{
	struct sc_ext_dma *dma = buf->dma;
	uint32_t value;
	int i;

	for (i = 0; i < buf->count; i++) {
		value = dma[i].dma_addr + dma[i].offset;
		writel_relaxed(value, sc_dev->regs + sc_ext_buf_offset[dir][i]);
	}
}

int sc_ext_run_job(struct sc_ctx *ctx)
{
	int i;
	struct sc_dev *sc_dev = ctx->sc_dev;
	struct sc_ext_task *task = sc_dev->xdev->current_task;
	struct sc_ext_task_data *data = &task->data[task->curr_count];
	uint32_t h_ratio, v_ratio;

	/* Set the SFR from user */
	for (i = 0; i < MSCL_NR_CMDS; i++) {
		if (sc_dev->version >= SCALER_VERSION(6, 0, 1)) {
			if (i == MSCL_SRC_SPAN) {
				writel_relaxed(SC_CMD_GET_SPAN_Y(data->cmd[i]),
					       sc_dev->regs + SCALER_SRC_YSPAN);
				writel_relaxed(SC_CMD_GET_SPAN_C(data->cmd[i]),
					       sc_dev->regs + SCALER_SRC_CSPAN);
				continue;
			} else if (i == MSCL_DST_SPAN) {
				writel_relaxed(SC_CMD_GET_SPAN_Y(data->cmd[i]),
					       sc_dev->regs + SCALER_DST_YSPAN);
				writel_relaxed(SC_CMD_GET_SPAN_C(data->cmd[i]),
					       sc_dev->regs + SCALER_DST_CSPAN);
				continue;
			}
		}
		writel_relaxed(data->cmd[i], sc_dev->regs + sc_ext_cmd_offset[i]);
	}

	/* Set the SFR for buffer */
	sc_ext_set_buffer(sc_dev, &data->buf[MSCL_SRC], MSCL_SRC);
	sc_ext_set_buffer(sc_dev, &data->buf[MSCL_DST], MSCL_DST);

	h_ratio = data->cmd[MSCL_H_RATIO];
	v_ratio = data->cmd[MSCL_V_RATIO];

	/* Set the SFR for internal */
	sc_hwset_csc_coef(sc_dev, NO_CSC, NULL);
	sc_hwset_polyphase_hcoef(sc_dev, h_ratio, h_ratio, 0);
	sc_hwset_polyphase_vcoef(sc_dev, v_ratio, v_ratio, 0);
	sc_hwset_int_en(sc_dev);

	if (sc_show_stat & 0x1)
		sc_hwregs_dump(sc_dev);

	sc_hwset_start(sc_dev);

	return 0;
}

static void sc_ext_try_task(struct sc_ext_dev *xdev, struct sc_ext_task *task)
{
	unsigned long flags;

	spin_lock_irqsave(&xdev->lock_task, flags);
	xdev->current_task = task;
	spin_unlock_irqrestore(&xdev->lock_task, flags);

	task->state = SC_EXT_BUFSTATE_PROCESSING;

	if (sc_ext_device_run(&task->xctx->sc_ctx)) {
		task->state = SC_EXT_BUFSTATE_ERROR;

		spin_lock_irqsave(&xdev->lock_task, flags);
		xdev->current_task = NULL;
		spin_unlock_irqrestore(&xdev->lock_task, flags);

		complete(&task->complete);
	}
}

void sc_ext_current_task_finish(struct sc_ext_dev *xdev, bool success)
{
	struct sc_ext_task *task = xdev->current_task;
	unsigned long flags;

	spin_lock_irqsave(&xdev->lock_task, flags);
	BUG_ON(!xdev->current_task);
	xdev->current_task = NULL;
	spin_unlock_irqrestore(&xdev->lock_task, flags);

	task->state = success ?
			SC_EXT_BUFSTATE_DONE : SC_EXT_BUFSTATE_ERROR;

	complete(&task->complete);
}

/* Returns true if final task is finished. */
bool sc_ext_job_finished(struct sc_ctx *ctx)
{
	struct sc_ext_ctx *xctx = sc_ctx_to_xctx(ctx);
	struct sc_ext_task *task = xctx->xdev->current_task;

	task->curr_count++;

	return (task->curr_count == task->task_count) ? true : false;
}

static const struct sc_ext_format *sc_ext_get_format(uint32_t fmt)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(sc_ext_supported_formats); i++)
		if (sc_ext_supported_formats[i].format == fmt)
			return &sc_ext_supported_formats[i];

	return NULL;
}

static int sc_ext_prepare_format(struct sc_ext_dev *xdev,
					struct sc_ext_task *task)
{
	uint32_t src, dst;
	int i;
	struct sc_ext_task_data *data = task->data;
	const struct sc_ext_format *fmt;

	for (i = 0; i < task->task_count; i++) {
		src = data[i].cmd[MSCL_SRC_CFG];
		dst = data[i].cmd[MSCL_DST_CFG];
		fmt = sc_ext_get_format(src);

		/*
		 * CSC is not supported.
		 * Source format and destination format must be same.
		 */
		if (src != dst || !fmt) {
			dev_err(xdev->dev,
				"Invalid src/dst format[%#x/%#x] for task %d\n",
				src, dst, i);
			return -EINVAL;
		}

		data[i].buf[MSCL_SRC].fmt = fmt;
		data[i].buf[MSCL_DST].fmt = fmt;
	}

	return 0;
}

static void sc_ext_put_dmabuf(struct sc_ext_dev *xdev,
				struct sc_ext_buf *buf, int dir)
{
	struct sc_ext_dma *dma = buf->dma;
	int i;

	for (i = 0; i < buf->count; i++) {
		if (!IS_ERR_OR_NULL(dma[i].sgt)) {
			dma_buf_unmap_attachment(dma[i].attachment,
					dma[i].sgt, DMA_BIDIRECTIONAL);
		}
		if (!IS_ERR_OR_NULL(dma[i].attachment))
			dma_buf_detach(dma[i].dmabuf, dma[i].attachment);
		if (!IS_ERR_OR_NULL(dma[i].dmabuf))
			dma_buf_put(dma[i].dmabuf);

		memset(&dma[i], 0x0, sizeof(dma[i]));
	}
}

static int sc_ext_get_dmabuf(struct sc_ext_dev *xdev,
		struct mscl_buffer *taskbuf, struct sc_ext_buf *buf, int dir)
{
	enum dma_data_direction dma_dir =
			(dir == MSCL_SRC) ? DMA_TO_DEVICE : DMA_FROM_DEVICE;
	struct sc_ext_dma *dma = buf->dma;
	int i, ret;

	if (taskbuf->count > MSCL_MAX_PLANES) {
		dev_err(xdev->dev,
			"taskbuf->count(%d) is larger than MAX(%d)\n",
			taskbuf->count, MSCL_MAX_PLANES);
		return -EINVAL;
	}

	buf->count = taskbuf->count;

	for (i = 0; i < buf->count; i++) {
		dma[i].dmabuf = dma_buf_get(taskbuf->dmabuf[i]);
		if (IS_ERR(dma[i].dmabuf)) {
			ret = PTR_ERR(dma[i].dmabuf);
			dev_err(xdev->dev,
				"Failed to get dmabuf, ret:%d\n", ret);
			goto err_dma_buf_get;
		}

		dma[i].attachment = dma_buf_attach(dma[i].dmabuf, xdev->dev);
		if (IS_ERR(dma[i].attachment)) {
			ret = PTR_ERR(dma[i].attachment);
			dev_err(xdev->dev,
				"Failed to attach dmabuf, ret:%d\n", ret);
			goto err_dma_buf_get;
		}

		dma[i].sgt = dma_buf_map_attachment(dma[i].attachment, dma_dir);
		if (IS_ERR(dma[i].sgt)) {
			ret = PTR_ERR(dma[i].sgt);
			dev_err(xdev->dev,
				"Failed to map dmabuf, ret:%d\n", ret);
			goto err_dma_buf_get;
		}

		dma[i].dma_addr = sg_dma_address(dma[i].sgt->sgl);
		if (IS_ERR_VALUE(dma[i].dma_addr)) {
			ret = dma[i].dma_addr;
			dev_err(xdev->dev,
				"Failed to map dma address, ret:%d\n", ret);
			goto err_dma_buf_get;
		}

		dma[i].offset = taskbuf->offset[i];
	}

	return 0;

err_dma_buf_get:
	sc_ext_put_dmabuf(xdev, buf, dir);

	return ret;
}

static int sc_ext_prepare_dmabuf(struct sc_ext_dev *xdev,
			struct sc_ext_task_data *data, struct mscl_task *mscl_task)
{
	int ret;

	ret = sc_ext_get_dmabuf(xdev, &mscl_task->buf[MSCL_SRC],
			&data->buf[MSCL_SRC], MSCL_SRC);
	if (ret)
		return ret;

	ret = sc_ext_get_dmabuf(xdev, &mscl_task->buf[MSCL_DST],
			&data->buf[MSCL_DST], MSCL_DST);
	if (ret) {
		sc_ext_put_dmabuf(xdev, &data->buf[MSCL_SRC], MSCL_SRC);
		return ret;
	}

	return 0;
}

static void sc_ext_finish_dmabuf(struct sc_ext_dev *xdev,
					struct sc_ext_task_data *data)
{
	sc_ext_put_dmabuf(xdev, &data->buf[MSCL_SRC], MSCL_SRC);
	sc_ext_put_dmabuf(xdev, &data->buf[MSCL_DST], MSCL_DST);
}

static bool sc_ext_buf_access_safe(struct sc_ext_buf *buf,
			int width, int height, int span, int plane_idx)
{
	int payload = 0;

	/* Luma is on plane 0 */
	if (plane_idx == 0) {
		payload = span * (height - 1) + width;
		if (width < span)
			payload += 128;
	}

	if (buf->fmt->nr_chroma[plane_idx] > 0) {
		int chroma_payload;

		span >>= buf->fmt->hshift;
		width >>= buf->fmt->hshift;
		height >>= buf->fmt->vshift;

		chroma_payload = span * (height - 1) + width;
		chroma_payload *= buf->fmt->nr_chroma[plane_idx];
		if (width < span)
			chroma_payload += 128;

		payload += chroma_payload;
	}

	payload += buf->dma[plane_idx].offset;

	if (payload > buf->dma[plane_idx].dmabuf->size)
		return false;

	return true;
}

static int sc_ext_check_buf_range(struct sc_ext_dev *xdev,
		struct sc_ext_buf *buf, int width, int height, int span)
{
	int i = 0;

	if (width > span)
		goto err_buf_range;

	for (i = 0; i < buf->count; i++) {
		if (!sc_ext_buf_access_safe(buf, width, height, span, i))
			goto err_buf_range;
	}

	return 0;

err_buf_range:
	dev_err(xdev->dev, "Error for buf %d, size %d, %d x %d (span:%d)\n",
			i, buf->dma[i].dmabuf->size, width, height, span);

	return -EINVAL;
}

static int sc_ext_check_task(struct sc_ext_dev *xdev,
					struct sc_ext_task_data *data)
{
	int ret = 0;

	/* Buffer range check */
	ret = sc_ext_check_buf_range(xdev, &data->buf[MSCL_SRC],
			SC_CMD_GET_WIDTH(data->cmd[MSCL_SRC_WH]),
			SC_CMD_GET_HEIGHT(data->cmd[MSCL_SRC_WH]),
			SC_CMD_GET_SPAN_Y(data->cmd[MSCL_SRC_SPAN]));
	if (ret) {
		dev_err(xdev->dev, "Out of range for source buffer\n");
		return -EINVAL;
	}

	ret = sc_ext_check_buf_range(xdev, &data->buf[MSCL_DST],
			SC_CMD_GET_WIDTH(data->cmd[MSCL_DST_WH]),
			SC_CMD_GET_HEIGHT(data->cmd[MSCL_DST_WH]),
			SC_CMD_GET_SPAN_Y(data->cmd[MSCL_DST_SPAN]));
	if (ret) {
		dev_err(xdev->dev, "Out of range for destination buffer\n");
		return -EINVAL;
	}

	return 0;
}

static int sc_ext_check_sanity(struct sc_ext_dev *xdev,
					struct sc_ext_task *task)
{
	int i, ret;

	for (i = 0; i < task->task_count; i++) {
		ret = sc_ext_check_task(xdev, &task->data[i]);
		if (ret) {
			dev_err(xdev->dev, "Error detected in task %d\n", i);
			sc_ext_show_task_data(&task->data[i], i);
			return ret;
		}
	}

	return 0;
}

static int sc_ext_prepare_buffer(struct sc_ext_dev *xdev,
		struct sc_ext_task *task, struct mscl_task *mscl_task)
{
	int i, ret;

	for (i = 0; i < task->task_count; i++) {
		ret = sc_ext_prepare_dmabuf(xdev,
				&task->data[i], &mscl_task[i]);
		if (ret) {
			dev_err(xdev->dev, "Failed to prepare dmabuf\n");
			goto err_prepare;
		}
	}

	return 0;

err_prepare:
	for (i--; i >= 0; i--)
		sc_ext_finish_dmabuf(xdev, &task->data[i]);

	return ret;
}

static void sc_ext_finish_buffer(struct sc_ext_dev *xdev,
					struct sc_ext_task *task)
{
	int i;

	for (i = 0; i < task->task_count; i++)
		sc_ext_finish_dmabuf(xdev, &task->data[i]);
}

static int sc_ext_prepare_task(struct sc_ext_dev *xdev,
		struct sc_ext_task *task, struct mscl_task *mscl_task)
{
	int i, ret;

	task->data = kcalloc(task->task_count,
				sizeof(*task->data), GFP_KERNEL);
	if (!task->data)
		return -ENOMEM;

	for (i = 0; i < task->task_count; i++)
		memcpy(&task->data[i].cmd, &mscl_task[i].cmd,
					sizeof(task->data[i].cmd));

	ret = sc_ext_prepare_format(xdev, task);
	if (ret) {
		dev_err(xdev->dev, "%s: Failed to prepare format, ret:%d\n",
			__func__, ret);
		goto err_prepare_task;
	}

	ret = sc_ext_prepare_buffer(xdev, task, mscl_task);
	if (ret) {
		dev_err(xdev->dev, "%s: Failed to prepare buffer, ret:%d\n",
			__func__, ret);
		goto err_prepare_task;
	}

	ret = sc_ext_check_sanity(xdev, task);
	if (ret) {
		dev_err(xdev->dev, "%s: Failed to check sanity, ret:%d\n",
			__func__, ret);
		goto err_check_sanity;
	}

	if (sc_show_stat & 0x2)
		sc_ext_show_all_task_buf(task);

	return 0;

err_check_sanity:
	sc_ext_finish_buffer(xdev, task);

err_prepare_task:
	kfree(task->data);

	return ret;
}

static void sc_ext_finish_task(struct sc_ext_dev *xdev,
				struct sc_ext_task *task)
{
	sc_ext_finish_buffer(xdev, task);
	kfree(task->data);
}

static int sc_ext_process(struct sc_ext_task *task,
				struct mscl_task *mscl_task)
{
	struct sc_ext_ctx *xctx = task->xctx;
	struct sc_ext_dev *xdev = xctx->xdev;
	int ret;

	init_completion(&task->complete);

	ret = sc_ext_prepare_task(xdev, task, mscl_task);
	if (ret)
		goto err;

	task->state = SC_EXT_BUFSTATE_READY;

	sc_ext_try_task(xdev, task);

	/* No timeout */
	wait_for_completion(&task->complete);

	BUG_ON(task->state == SC_EXT_BUFSTATE_READY);

	sc_ext_finish_task(xdev, task);
err:
	if (ret)
		return ret;
	return (task->state == SC_EXT_BUFSTATE_DONE) ? 0 : -EINVAL;
}

static int sc_ext_open(struct inode *inode, struct file *filp)
{
	struct sc_ext_dev *xdev = container_of(filp->private_data,
						struct sc_ext_dev, misc);
	struct sc_dev *sc_dev = dev_get_drvdata(xdev->dev);
	struct sc_ext_ctx *xctx;
	struct sc_ctx *sc_ctx;
	int ret = 0;

	xctx = kzalloc(sizeof(*xctx), GFP_KERNEL);
	if (!xctx)
		return -ENOMEM;

	INIT_LIST_HEAD(&xctx->node);

	xctx->xdev = xdev;

	spin_lock(&xdev->lock_ctx);
	list_add_tail(&xctx->node, &xdev->contexts);
	spin_unlock(&xdev->lock_ctx);

	filp->private_data = xctx;

	sc_ctx = &xctx->sc_ctx;

	atomic_inc(&sc_dev->m2m.in_use);

	if (!IS_ERR(sc_dev->pclk)) {
		ret = clk_prepare(sc_dev->pclk);
		if (ret) {
			dev_err(sc_dev->dev,
				"%s: failed to prepare PCLK(err %d)\n",
				__func__, ret);
			goto err_pclk;
		}
	}

	if (!IS_ERR(sc_dev->aclk)) {
		ret = clk_prepare(sc_dev->aclk);
		if (ret) {
			dev_err(sc_dev->dev,
				"%s: failed to prepare ACLK(err %d)\n",
				__func__, ret);
			goto err_aclk;
		}
	}

	sc_ctx->context_type = SC_CTX_EXT_TYPE;
	INIT_LIST_HEAD(&sc_ctx->node);
	sc_ctx->sc_dev = sc_dev;

	return ret;

err_aclk:
	if (!IS_ERR(sc_dev->pclk))
		clk_unprepare(sc_dev->pclk);
err_pclk:
	kfree(xctx);
	return ret;
}

static int sc_ext_release(struct inode *inode, struct file *filp)
{
	struct sc_ext_ctx *xctx = filp->private_data;
	struct sc_ext_dev *xdev = xctx->xdev;
	struct sc_dev *sc_dev = dev_get_drvdata(xdev->dev);
	unsigned long flags;
	bool need_wait = false;

	spin_lock_irqsave(&xdev->lock_task, flags);
	if (xdev->current_task &&
			xdev->current_task == xctx->task)
		need_wait = true;
	spin_unlock_irqrestore(&xdev->lock_task, flags);

	if (need_wait)
		wait_for_completion(&xctx->task->complete);

	atomic_dec(&sc_dev->m2m.in_use);

	if (!IS_ERR(sc_dev->aclk))
		clk_unprepare(sc_dev->aclk);
	if (!IS_ERR(sc_dev->pclk))
		clk_unprepare(sc_dev->pclk);

	spin_lock(&xctx->xdev->lock_ctx);
	list_del(&xctx->node);
	spin_unlock(&xctx->xdev->lock_ctx);

	kfree(xctx);

	return 0;
}

#define MAX_MSCL_TASKS		10
static long sc_ext_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	struct sc_ext_ctx *xctx = filp->private_data;
	struct sc_ext_dev *xdev = xctx->xdev;

	switch (cmd) {
	case MSCL_IOC_JOB:
	{
		struct sc_ext_task task;
		struct mscl_job job;
		struct mscl_task *mscl_task;
		int ret;

		memset(&task, 0, sizeof(task));

		if (copy_from_user(&job, (void __user *)arg, sizeof(job))) {
			dev_err(xdev->dev,
				"%s: Failed to read userdata\n", __func__);
			return -EFAULT;
		}

		/* TODO: check for supported version */

		if (job.taskcount >= MAX_MSCL_TASKS || !job.taskcount) {
			dev_err(xdev->dev,
				"%s: (%s) count of tasks %d, max tasks: %d\n",
				__func__,
				(!job.taskcount) ? "zero-count" : "Too many",
				job.taskcount, MAX_MSCL_TASKS);
			return -EINVAL;
		}

		mscl_task = kcalloc(job.taskcount, sizeof(*mscl_task), GFP_KERNEL);
		if (!mscl_task)
			return -ENOMEM;

		if (copy_from_user(mscl_task, (void __user *)job.tasks,
				job.taskcount * sizeof(*mscl_task))) {
			dev_err(xdev->dev,
				"%s: Failed to read tasks\n", __func__);
			kfree(mscl_task);
			return -EFAULT;
		}


		task.task_count = job.taskcount;
		task.xctx = xctx;

		if (sc_show_stat & 0x2)
			sc_ext_show_mscl_task(mscl_task, job.taskcount);

		mutex_lock(&xdev->lock_ioctl);

		/* No pending job is allowed for simple operation */
		ret = sc_ext_process(&task, mscl_task);

		mutex_unlock(&xdev->lock_ioctl);

		kfree(mscl_task);

		return ret;
	}
	default:
		dev_err(xdev->dev, "%s: Unknown ioctl cmd %x\n",
			__func__, cmd);
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
struct compat_mscl_job {
	uint32_t version;
	uint32_t taskcount;
	compat_uptr_t tasks;
};

#define COMPAT_MSCL_IOC_JOB _IOW('M', 1, struct compat_mscl_job)

static long sc_ext_compat_ioctl32(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	struct sc_ext_ctx *xctx = filp->private_data;
	struct sc_ext_dev *xdev = xctx->xdev;
	struct compat_mscl_job __user *udata = compat_ptr(arg);
	struct mscl_job __user *data;
	compat_uptr_t uptr;
	size_t alloc_size;
	__u32 w;
	int ret;

	if (cmd != COMPAT_MSCL_IOC_JOB) {
		dev_err(xdev->dev, "Unknown ioctl %#x\n", cmd);
		return -EINVAL;
	}

	alloc_size = sizeof(*data);
	data = compat_alloc_user_space(alloc_size);

	ret = get_user(w, &udata->version);
	ret |= put_user(w, &data->version);
	ret |= get_user(w, &udata->taskcount);
	ret |= put_user(w, &data->taskcount);
	ret |= get_user(uptr, &udata->tasks);
	ret |= put_user(compat_ptr(uptr), &data->tasks);
	if (ret) {
		dev_err(xdev->dev, "failed to read task data\n");
		return ret;
	}

	ret = sc_ext_ioctl(filp,
			(unsigned int)MSCL_IOC_JOB, (unsigned long)data);
	if (ret)
		return ret;

	return 0;
}
#endif

static const struct file_operations sc_ext_fops = {
	.owner          = THIS_MODULE,
	.open           = sc_ext_open,
	.release        = sc_ext_release,
	.unlocked_ioctl	= sc_ext_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= sc_ext_compat_ioctl32,
#endif
};

struct sc_ext_dev *create_scaler_ext_device(struct device *dev)
{
	struct sc_ext_dev *xdev;
	int ret;

	xdev = devm_kzalloc(dev, sizeof(*xdev), GFP_KERNEL);
	if (!xdev)
		return ERR_PTR(-ENOMEM);

	xdev->misc.minor = MISC_DYNAMIC_MINOR;
	xdev->misc.name = SC_EXT_DEV_NAME;
	xdev->misc.fops = &sc_ext_fops;
	ret = misc_register(&xdev->misc);
	if (ret) {
		dev_err(dev, "failed to register %s\n", SC_EXT_DEV_NAME);
		return ERR_PTR(ret);
	}

	INIT_LIST_HEAD(&xdev->contexts);

	spin_lock_init(&xdev->lock_task);
	spin_lock_init(&xdev->lock_ctx);
	mutex_init(&xdev->lock_ioctl);
	xdev->dev = dev;

	return xdev;
}

void destroy_scaler_ext_device(struct sc_ext_dev *xdev)
{
	misc_deregister(&xdev->misc);
}
