// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <exynos_drm_hdr.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_dpp.h>
#include <hdr_cal.h>
#include <dpp_cal.h>

static int dpu_hdr_log_level = 6;
module_param(dpu_hdr_log_level, int, 0600);
MODULE_PARM_DESC(dpu_hdr_log_level, "log level for dpu hdr [default : 6]");

#define HDR_DRIVER_NAME "exynos-drmhdr"
#define hdr_info(hdr, fmt, ...)	\
dpu_pr_info(HDR_DRIVER_NAME, (hdr)->id, dpu_hdr_log_level, fmt, ##__VA_ARGS__)

#define hdr_warn(hdr, fmt, ...)	\
dpu_pr_warn(HDR_DRIVER_NAME, (hdr)->id, dpu_hdr_log_level, fmt, ##__VA_ARGS__)

#define hdr_err(hdr, fmt, ...)	\
dpu_pr_err(HDR_DRIVER_NAME, (hdr)->id, dpu_hdr_log_level, fmt, ##__VA_ARGS__)

#define hdr_debug(hdr, fmt, ...)\
dpu_pr_debug(HDR_DRIVER_NAME, (hdr)->id, dpu_hdr_log_level, fmt, ##__VA_ARGS__)

struct hdr_coef_header {
	unsigned int total_bytesize;
	union hdr_coef_header_type {
		struct hdr_coef_header_type_unpack {
			unsigned char hw_type; // 0: None, 1: S.LSI, 2: Custom
			unsigned char layer_index;
			unsigned char log_level;
			unsigned char optional_flag;
		} unpack;
		unsigned int pack;
	} type;
	union hdr_coef_header_con {
		struct hdr_coef_header_con_unpack {
			unsigned char mul_en; /* de-multiplication -> HDR -> re-multiplication */
			unsigned char reserved[3];
		} unpack;
		unsigned int pack;
	} sfr_con; // uses this for other sfr_con, HDR con would move to lut part
	union hdr_coef_header_lut {
		struct hdr_coef_header_lut_unpack {
			unsigned short shall; // # of shall group
			unsigned short need; // # of need group
		} unpack;
		unsigned int pack;
	} num;
};

#define HDR_LUT_MAGIC 0xDADADADA

struct hdr_lut_header {
	unsigned int byte_offset;
	unsigned int length;
	unsigned int magic;
};

static struct class *hdr_cls;
static struct device_attribute *hdr_attrs[];

#define PREFIX_LEN      40
#define ROW_LEN         32
static void hdr_print_hex_dump(struct exynos_hdr *hdr, u32 offset,
				size_t size, const void *buf)
{
	char prefix_buf[PREFIX_LEN];
	size_t i, row;

	hdr_info(hdr, "=== dump %d ===\n", size);
	for (i = 0; i < size; i += ROW_LEN) {
		if (size - i < ROW_LEN)
			row = size - i;
		else
			row = ROW_LEN;
		snprintf(prefix_buf, sizeof(prefix_buf), "HDR:[%08X] ", offset + i);
		print_hex_dump(KERN_INFO, prefix_buf, DUMP_PREFIX_NONE,
				32, 4, buf + i, row, false);
	}
}

static int hdr_import_buffer(struct exynos_hdr *hdr,
				const struct exynos_drm_plane_state *exynos_plane_state)
{
	struct dma_buf *buf = NULL;
	void *vaddr = NULL;
	int64_t hdr_fd;
	int i;

	hdr_debug(hdr, "%s +\n", __func__);

	hdr_fd = exynos_plane_state->hdr_fd;
	if (hdr_fd < 3) {
		if (hdr_fd <= 0) // reset value of hdr_fd is 0
			hdr_debug(hdr, "not allocated hdr_fd [%lld]\n", hdr_fd);
		else
			hdr_err(hdr, "invalid hdr_fd [%lld]\n", hdr_fd);
		goto error;
	}

	buf = dma_buf_get(hdr_fd);
	if (IS_ERR_OR_NULL(buf)) {
		if ((hdr_fd == hdr->hdr_fd) && IS_ERR(buf)) {
			hdr_debug(hdr, "bad fd [%ld] but continued with old vbuf\n", PTR_ERR(buf));
			goto done;
		}

		hdr_warn(hdr, "failed to get dma buf [%x] of fd [%lld]\n", buf, hdr_fd);
		goto error;
	}

	if ((hdr_fd == hdr->hdr_fd) && (buf == hdr->dma_buf)) {
		dma_buf_put(buf);
		goto done;
	}

	// release old buf
	if (hdr->dma_buf) {
		if (hdr->dma_vbuf)
			dma_buf_vunmap(hdr->dma_buf, hdr->dma_vbuf);
		dma_buf_put(hdr->dma_buf);
	}

	vaddr = dma_buf_vmap(buf);
	if (IS_ERR_OR_NULL(vaddr)) {
		hdr_err(hdr, "failed to vmap buffer [%x]\n", vaddr);
		goto error;
	}

	hdr->hdr_fd = hdr_fd;
	hdr->dma_buf = buf;
	hdr->dma_vbuf = vaddr;
	for (i = 0; i < MAX_HDR_CONTEXT; i++) {
		if (!hdr->ctx[i].data)
			hdr->ctx[i].data = kzalloc(buf->size, GFP_KERNEL);
	}
done:
	return 0;

error:
	if (!IS_ERR_OR_NULL(buf)) {
		if (!IS_ERR_OR_NULL(vaddr))
			dma_buf_vunmap(buf, vaddr);
		dma_buf_put(buf);
	}
	return -1;
}

static struct hdr_context *hdr_acquire_context(struct exynos_hdr *hdr)
{
	int ctx_no = (atomic_inc_return(&hdr->ctx_no) & INT_MAX) % MAX_HDR_CONTEXT;

	return &hdr->ctx[ctx_no];
}

static int hdr_prepare_context(struct exynos_hdr *hdr,
			struct exynos_drm_plane_state *exynos_plane_state)
{
	const struct hdr_coef_header *coef_h;
	struct hdr_context *ctx;
	int ret;

	ret = hdr_import_buffer(hdr, exynos_plane_state);
	if (ret < 0)
		return -1;

	coef_h = (struct hdr_coef_header *)hdr->dma_vbuf;
	if (!coef_h) {
		hdr_err(hdr, "no allocated virtual buffer\n");
		return -1;
	}

	if (!coef_h->total_bytesize || (coef_h->total_bytesize < sizeof(*coef_h))) {
		hdr_err(hdr, "invalid size: total %d\n", coef_h->total_bytesize);
		return -1;
	}

	ctx = hdr_acquire_context(hdr);
	if (!ctx || !ctx->data) {
		hdr_err(hdr, "no valid ctx\n");
		return -1;
	}

	memcpy(ctx->data, (void *)hdr->dma_vbuf, coef_h->total_bytesize);
	exynos_plane_state->hdr_ctx = ctx->data;

	return 0;
}

static int hdr_update_context(struct exynos_hdr *hdr,
			const struct exynos_drm_plane_state *exynos_plane_state)
{
	const struct hdr_coef_header *coef_h = NULL;
	const struct hdr_lut_header *lut_h;
	unsigned int size, lut_size, count = 0;
	u32 id = hdr->id;
	const u32 *data;
	unsigned char hw_type;

	if (!exynos_plane_state->hdr_ctx) {
		hdr_debug(hdr, "null hdr_ctx\n");
		return -1;
	}

	coef_h = (struct hdr_coef_header *)exynos_plane_state->hdr_ctx;
	if (!coef_h) {
		hdr_err(hdr, "no allocated virtual buffer\n");
		goto error;
	}

	if (!coef_h->total_bytesize || (coef_h->total_bytesize < sizeof(*coef_h))) {
		hdr_err(hdr, "invalid size: total %d\n", coef_h->total_bytesize);
		goto error;
	}

	if (coef_h->type.unpack.layer_index != id) {
		hdr_err(hdr, "invalid layer index %d\n", coef_h->type.unpack.layer_index);
		goto error_dump;
	}

	hw_type = coef_h->type.unpack.hw_type;
	if (hw_type == HDR_HW_TYPE_0 || hw_type >= HDR_HW_TYPE_MAX) {
		hdr_err(hdr, "invalid hw type %d\n", coef_h->type.unpack.hw_type);
		goto error_dump;
	}

	dpp_reg_set_hdr_mul_con(hdr->dpp->id, coef_h->sfr_con.unpack.mul_en);
	if (coef_h->type.unpack.log_level >= 10) {
		hdr_info(hdr, "coef: fd %lld total %d idx %d hwtype %d num %d+%d\n",
				hdr->hdr_fd, coef_h->total_bytesize,
				coef_h->type.unpack.layer_index, hw_type,
				coef_h->num.unpack.shall, coef_h->num.unpack.need);
	}

	size = sizeof(*coef_h);
	while (size < coef_h->total_bytesize) {
		lut_h = (struct hdr_lut_header *)((char *)coef_h + size);
		if (coef_h->type.unpack.log_level >= 10) {
			hdr_info(hdr, "lut%d/%d: magic %x, len %d off %d\n", count,
				coef_h->num.unpack.shall + coef_h->num.unpack.need,
				lut_h->magic, lut_h->length, lut_h->byte_offset);
		}

		if (lut_h->magic != HDR_LUT_MAGIC || !lut_h->length) {
			hdr_err(hdr, "invalid params [%x] len %d\n",
					lut_h->magic, lut_h->length);
			goto error_dump;
		}

		data = (const u32 *)((char *)lut_h + sizeof(*lut_h));
		size += sizeof(*lut_h);

		lut_size = (unsigned int)(lut_h->length*sizeof(*data));
		if (((lut_h->byte_offset + lut_size) > hdr->reg_size) ||
			(!hdr_reg_verify(id, hw_type, lut_h->byte_offset, lut_h->length))) {
			hdr_err(hdr, "invalid offset %d, size %d\n",
					lut_h->byte_offset, lut_size);
			goto error_dump;
		}
		size += lut_size;

		if (count < coef_h->num.unpack.shall ||
			hdr->state == HDR_STATE_DISABLE) {
			hdr_reg_set_lut(id, lut_h->byte_offset, lut_h->length, data);
			if (coef_h->type.unpack.log_level >= 10) {
				hdr_print_hex_dump(hdr, lut_h->byte_offset,
					lut_h->length * sizeof(*data), data);
			}
		}
		count++;
	}

	if (count != (coef_h->num.unpack.shall + coef_h->num.unpack.need)) {
		hdr_warn(hdr, "number of data %d vs %d mismatch\n", count,
				(coef_h->num.unpack.shall + coef_h->num.unpack.need));
	}

	if (coef_h->type.unpack.log_level >= 10) {
		hdr_info(hdr, "%s context updated %d (%d+%d)\n", __func__,
			count, coef_h->num.unpack.shall, coef_h->num.unpack.need);
	}

	return 0;
error_dump:
	hdr_print_hex_dump(hdr, 0, coef_h->total_bytesize, coef_h);
error:
	hdr_err(hdr, "%s parsing error\n", __func__);
	return -1;
}

static int hdr_remap_regs(struct exynos_hdr *hdr, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int i;
	u32 id = hdr->id;
	struct resource res;

	i = of_property_match_string(np, "reg-names", "hdr_lsi");
	if (i < 0) {
		hdr_err(hdr, "failed to find hdr regs\n");
		goto err;
	}

	if (of_address_to_resource(np, i, &res)) {
		hdr_err(hdr, "failed to map to resource\n");
		goto err;
	}

	hdr->regs.hdr_base_regs = devm_ioremap_resource(dev, &res);
	if (!hdr->regs.hdr_base_regs) {
		hdr_err(hdr, "failed to remap hdr registers\n");
		goto err;
	}
	hdr->reg_size = resource_size(&res);

	hdr_regs_desc_init(id, hdr->regs.hdr_base_regs, "hdr_lsi", REGS_HDR_LSI);

	hdr_info(hdr, "start(0x%x), size(0x%x)\n", (u32)res.start, hdr->reg_size);
	return 0;

err:
	if (hdr->regs.hdr_base_regs)
		iounmap(hdr->regs.hdr_base_regs);

	return -EINVAL;
}

static int hdr_init_resources(struct exynos_hdr *hdr, struct device *dev)
{
	int i;
	int ret = 0;
	char name[16];

	ret = hdr_remap_regs(hdr, dev);
	if (ret) {
		hdr_err(hdr, "failed to remap regs\n");
		goto err;
	}

	if (!hdr_cls)
		hdr_cls = class_create(THIS_MODULE, "hdr");
	if (IS_ERR_OR_NULL(hdr_cls)) {
		hdr_err(hdr, "failed to create hdr class\n");
		goto err;
	}

	sprintf(name, "hdr%d", hdr->id);
	hdr->dev = device_create(hdr_cls, NULL, 0, &hdr, name);
	if (IS_ERR_OR_NULL(hdr->dev)) {
		hdr_err(hdr, "failed to create hdr device\n");
		goto err;
	}

	for (i = 0; hdr_attrs[i] != NULL; i++) {
		ret = device_create_file(hdr->dev, hdr_attrs[i]);
		if (ret) {
			hdr_err(hdr, "failed to create file: %s\n",
				(*hdr_attrs[i]).attr.name);
			goto err;
		}
	}

	return ret;
err:
	return -1;
}

/*============== CREATE SYSFS ===================*/
#define HDR_CREATE_SYSFS_FUNC(name) \
	static ssize_t dpp_hdr_ ## name ## _show(struct device *dev, \
				struct device_attribute *attr, char *buf) \
	{ \
		int ret = 0; \
		struct exynos_hdr *hdr = dev_get_drvdata(dev); \
		if (buf == NULL) \
			return -1;\
		mutex_lock(&hdr->lock); \
		hdr_debug(hdr, "%s +\n", __func__); \
		ret = hdr_ ## name ## _show(hdr, buf); \
		if (ret < 0) \
			hdr_err(hdr, "%s err -\n", __func__); \
		else \
			hdr_debug(hdr, "%s -\n", __func__); \
		mutex_unlock(&hdr->lock); \
		return ret; \
	} \
	static ssize_t dpp_hdr_ ## name ## _store(struct device *dev, \
		struct device_attribute *attr, const char *buffer, size_t count) \
	{ \
		int ret = 0; \
		struct exynos_hdr *hdr = dev_get_drvdata(dev); \
		if (count <= 0 || buffer == NULL) \
			return -1;\
		mutex_lock(&hdr->lock); \
		hdr_debug(hdr, "%s +\n", __func__); \
		ret = hdr_ ## name ## _store(hdr, buffer, count); \
		if (ret < 0) \
			hdr_err(hdr, "%s err -\n", __func__); \
		else \
			hdr_debug(hdr, "%s -\n", __func__); \
		mutex_unlock(&hdr->lock); \
		return ret; \
	} \
	static DEVICE_ATTR(name, 0660, \
					dpp_hdr_ ## name ## _show, \
					dpp_hdr_ ## name ## _store)

#define HDR_CREATE_SYSFS_FUNC_SHOW(name) \
	static ssize_t dpp_hdr_ ## name ## _show(struct device *dev, \
				struct device_attribute *attr, char *buf) \
	{ \
		int ret = 0; \
		struct exynos_hdr *hdr = dev_get_drvdata(dev); \
		if (buf == NULL) \
			return -1;\
		mutex_lock(&hdr->lock); \
		hdr_debug(hdr, "%s +\n", __func__); \
		ret = hdr_ ## name ## _show(hdr, buf); \
		if (ret < 0) \
			hdr_err(hdr, "%s err -\n", __func__); \
		else \
			hdr_debug(hdr, "%s -\n", __func__); \
		mutex_unlock(&hdr->lock); \
		return ret; \
	} \
	static DEVICE_ATTR(name, 0440, \
					dpp_hdr_ ## name ## _show, NULL)

static ssize_t hdr_dump_show(struct exynos_hdr *hdr, char *buf)
{
	struct decon_device *decon = get_decon_drvdata(0);
	int timeout;

	if (!decon || !decon->crtc) {
		hdr_err(hdr, "unable to get a dump as decon0 not connected\n");
		return -1;
	}

	hibernation_block_exit(decon->crtc->hibernation);
	timeout = wait_event_interruptible_timeout(hdr->wait_update,
			hdr->state == HDR_STATE_ENABLE, msecs_to_jiffies(3*1000));
	__hdr_dump(hdr->id, &hdr->regs);
	hibernation_unblock(decon->crtc->hibernation);
	return snprintf(buf, PAGE_SIZE, "dump[%d] = %d\n", hdr->id, timeout);
}

HDR_CREATE_SYSFS_FUNC_SHOW(dump);

static struct device_attribute *hdr_attrs[] = {
	&dev_attr_dump,
	NULL,
};
/*============== CREATE SYSFS END ===================*/
static void __exynos_hdr_dump(struct exynos_hdr *hdr)
{
	__hdr_dump(hdr->id, &hdr->regs);
}

static void __exynos_hdr_prepare(struct exynos_hdr *hdr,
			struct exynos_drm_plane_state *exynos_plane_state)
{
	hdr_debug(hdr, "%s +\n", __func__);
	hdr_prepare_context(hdr, exynos_plane_state);
	hdr_debug(hdr, "%s -\n", __func__);
}

static void __exynos_hdr_update(struct exynos_hdr *hdr,
			const struct exynos_drm_plane_state *exynos_plane_state)
{
	int ret;

	hdr_debug(hdr, "%s +\n", __func__);
	ret = hdr_update_context(hdr, exynos_plane_state);
	if (ret == 0) {
		hdr->state = HDR_STATE_ENABLE;
		wake_up(&hdr->wait_update);
	}
	hdr_debug(hdr, "%s -\n", __func__);
}

static void __exynos_hdr_disable(struct exynos_hdr *hdr)
{
	hdr_debug(hdr, "%s +\n", __func__);
	hdr->state = HDR_STATE_DISABLE;
	hdr_debug(hdr, "%s -\n", __func__);
}

static const struct exynos_hdr_funcs hdr_funcs = {
	.dump = __exynos_hdr_dump,
	.prepare = __exynos_hdr_prepare,
	.update = __exynos_hdr_update,
	.disable = __exynos_hdr_disable,
};

void exynos_hdr_dump(struct exynos_hdr *hdr)
{
	const struct exynos_hdr_funcs *funcs;

	if (!hdr || hdr->initialized == false)
		return;

	funcs = hdr->funcs;
	if (funcs)
		funcs->dump(hdr);
}

void exynos_hdr_prepare(struct exynos_hdr *hdr,
			struct exynos_drm_plane_state *exynos_plane_state)
{
	const struct exynos_hdr_funcs *funcs;

	if (!hdr || hdr->initialized == false || !exynos_plane_state)
		return;

	funcs = hdr->funcs;
	if (funcs)
		funcs->prepare(hdr, exynos_plane_state);
}

void exynos_hdr_update(struct exynos_hdr *hdr,
			const struct exynos_drm_plane_state *exynos_plane_state)
{
	const struct exynos_hdr_funcs *funcs;

	if (!hdr || hdr->initialized == false || !exynos_plane_state)
		return;

	funcs = hdr->funcs;
	if (funcs)
		funcs->update(hdr, exynos_plane_state);
}

void exynos_hdr_disable(struct exynos_hdr *hdr)
{
	const struct exynos_hdr_funcs *funcs;

	if (!hdr || hdr->initialized == false)
		return;

	funcs = hdr->funcs;
	if (funcs)
		funcs->disable(hdr);
}

struct exynos_hdr *exynos_hdr_register(struct dpp_device *dpp)
{
	struct exynos_hdr *hdr;
	int ret;

	if (!dpp) {
		pr_err("%s dpp is NULL\n", __func__);
		return NULL;
	}

	if (!(test_bit(DPP_ATTR_HDR, &dpp->attr)
		|| test_bit(DPP_ATTR_HDR10_PLUS, &dpp->attr)
		|| test_bit(DPP_ATTR_C_HDR, &dpp->attr)
		|| test_bit(DPP_ATTR_C_HDR10_PLUS, &dpp->attr)
		|| test_bit(DPP_ATTR_WCG, &dpp->attr))) {
		pr_info("%s no required as dpp attr disabled\n", __func__, dpp->attr);
		return NULL;
	}

	hdr = devm_kzalloc(dpp->dev, sizeof(struct exynos_hdr), GFP_KERNEL);
	if (!hdr)
		return NULL;

	hdr->dpp = dpp;
	hdr->id = dpp->id;

	ret = hdr_init_resources(hdr, dpp->dev);
	if (ret) {
		hdr_err(hdr, "failed to init resources\n");
		return NULL;
	}

	dev_set_drvdata(hdr->dev, hdr);

	mutex_init(&hdr->lock);
	init_waitqueue_head(&hdr->wait_update);
	atomic_set(&hdr->ctx_no, 0);
	hdr->funcs = &hdr_funcs;
	hdr->state = HDR_STATE_DISABLE;
	hdr->initialized = true;

	hdr_info(hdr, "HDR/WCG supported\n");

	return hdr;
}
