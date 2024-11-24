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

#define HDR_DRIVER_NAME "exynos-drmhdr"
#define hdr_info(hdr, fmt, ...)	\
pr_info("%s[%d]: "fmt, HDR_DRIVER_NAME, hdr->id, ##__VA_ARGS__)

#define hdr_warn(hdr, fmt, ...)	\
pr_warn("%s[%d]: "fmt, HDR_DRIVER_NAME, hdr->id, ##__VA_ARGS__)

#define hdr_err(hdr, fmt, ...)	\
pr_err("%s[%d]: "fmt, HDR_DRIVER_NAME, hdr->id, ##__VA_ARGS__)

#define hdr_debug(hdr, fmt, ...)	\
pr_debug("%s[%d]: "fmt, HDR_DRIVER_NAME, hdr->id, ##__VA_ARGS__)

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
	unsigned int sfr_con;
	union hdr_coef_header_lut {
		struct hdr_coef_header_lut_unpack {
			unsigned short shall; // # of shall group
			unsigned short need; // # of need group
		} unpack;
		unsigned int pack;
	} num;
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
		if ((hdr_fd == hdr->hdr_fd) && (PTR_ERR(buf) == -EBADF)) {
			hdr_debug(hdr, "FD in bad state but continued\n");
			goto done;
		} else {
			hdr_err(hdr, "failed to get dma buf [%x] of fd [%lld]\n", buf, hdr_fd);
			WARN_ON(1);
			goto error;
		}
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

#define MAX_HDR_CONTEXT 3 // 3 ctx buffer per layer
static void hdr_allocate_context(struct exynos_hdr *hdr)
{
	u32 i;

	if (!hdr->ctx) {
		if (!hdr->dma_buf || hdr->dma_buf->size == 0) {
			hdr_err(hdr, "invalid dma_buf\n");
			return;
		}

		hdr->ctx = kzalloc(sizeof(struct hdr_context)*MAX_HDR_CONTEXT, GFP_KERNEL);
		if (!hdr->ctx)
			return;

		for (i = 0; i < MAX_HDR_CONTEXT; i++) {
			hdr->ctx[i].data = kzalloc(hdr->dma_buf->size, GFP_KERNEL);
			if (!hdr->ctx[i].data)
				return;
		}
	}
}

static struct hdr_context *hdr_acquire_context(struct exynos_hdr *hdr)
{
	int i;
	struct hdr_context *ctx = NULL;

	if (!hdr->ctx)
		return NULL;

	for (i = 0; i < MAX_HDR_CONTEXT; i++) {
		if (hdr->ctx[i].used == 0) {
			ctx = &hdr->ctx[i];
			ctx->used = 1;
			break;
		}
	}

	if (i == MAX_HDR_CONTEXT)
		hdr_err(hdr, "all ctx pools are occupied\n");

	return ctx;
}

static void hdr_release_context(struct exynos_hdr *hdr,
				struct hdr_context *ctx)
{
	int i;

	if (ctx) {
		if (ctx->used == 0)
			hdr_warn(hdr, "ctx is already unused state\n");
		ctx->used = 0;
	} else {
		if (!hdr->ctx)
			return;

		for (i = 0; i < MAX_HDR_CONTEXT; i++)
			hdr->ctx[i].used = 0;
	}
}

static int hdr_prepare_context(struct exynos_hdr *hdr,
			const struct exynos_drm_plane_state *exynos_plane_state)
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

	hdr_allocate_context(hdr);
	ctx = hdr_acquire_context(hdr);
	if (!ctx) {
		hdr_err(hdr, "no valid ctx\n");
		return -1;
	}

	memcpy(ctx->data, (void *)hdr->dma_vbuf, coef_h->total_bytesize);

	mutex_lock(&hdr->ctx_list_lock);
	list_add_tail(&ctx->list, &hdr->ctx_list);
	mutex_unlock(&hdr->ctx_list_lock);

	return 0;
}


#define HDR_ADDR_RANGE 0x1000
#define HDR_MAX_REG_CNT (HDR_ADDR_RANGE / 4)
#define HDR_REG_CON 0x0
#define HDR_REG_CON_EN	0x1
#define MAX_UNPACK_COUNT	6

static int hdr_update_lut(struct exynos_hdr *hdr, u32 *lut, int lut_offset)
{
	int i;
	u32 reg_offset = 0;
	u32 reg_length = 0;

	if (!lut || !hdr) {
		pr_err("invalid argument\n");
		goto exit_update;
	}

	reg_offset = lut[lut_offset++];
	reg_length = lut[lut_offset++];

	if (reg_length > HDR_MAX_REG_CNT) {
		hdr_err(hdr, "out of reg length: %d\n", reg_length);
		goto exit_update;
	}

	for (i = 0; i < reg_length; i++) {
		if (reg_offset + (i * 4) > HDR_ADDR_RANGE) {
			hdr_err(hdr, "out of address range: %x(offset:%d: length:%d)\n",
				reg_offset + (i * 4), reg_offset, reg_length);
			goto exit_update;
		}
		hdr_write_reg(hdr->id, reg_offset + (i * 4), lut[lut_offset + i]);
	}

	return lut_offset + reg_length;

exit_update:
	return 0;
}

static int hdr_update_reg(struct exynos_hdr *hdr, struct hdr_coef_header *header)
{
	int i;
	int shall, need;
	int lut_offset;
	u32 con_reg = 0;

	if (!hdr || !header) {
		pr_err("invalid argument\n");
		goto update_err;
	}

	shall = header->num.unpack.shall;
	if ((shall < 0) || (shall > MAX_UNPACK_COUNT)) {
		hdr_err(hdr, "%d exceed unpack shall: %d\b", hdr->id, shall);
		goto update_err;
	}

	lut_offset = sizeof(struct hdr_coef_header) / sizeof(int);
	for (i = 0; i < shall; i++) {
		lut_offset = hdr_update_lut(hdr, (u32 *)header, lut_offset);
		if (lut_offset == 0) {
			hdr_err(hdr, "failed to update lut\n");
			goto update_err;
		}
	}

	/* in case of the first frame update after hibernation */
	con_reg = hdr_read_reg(hdr->id, HDR_REG_CON);
	if (!(con_reg & HDR_REG_CON_EN)) {
		need = header->num.unpack.need;
		if ((need < 0) || (need > MAX_UNPACK_COUNT)) {
			hdr_err(hdr, "%d exceed unpack shall: %d\n", hdr->id, need);
			goto update_err;
		}

		for (i = 0; i < need; i++) {
			lut_offset = hdr_update_lut(hdr, (u32 *)header, lut_offset);
			if (lut_offset == 0) {
				hdr_err(hdr, "failed to update lut\n");
				goto update_err;
			}
		}
	}

	hdr_write_reg(hdr->id, HDR_REG_CON, header->sfr_con);

	if (header->sfr_con & HDR_REG_CON_EN)
		hdr->enable = true;
	else
		hdr->enable = false;

	return 0;

update_err:
	return -EINVAL;
}


#ifdef HDR_DEBUG
static void print_reg_data(int shall, int need, u32 *data)
{
	int i, j;
	int lut_offset;
	u32 reg_offset;
	u32 reg_length;
	u32 value;

	lut_offset = sizeof(struct hdr_coef_header) / sizeof(u32);

	for (i = 0; i < shall; i++) {
		reg_offset = data[lut_offset++];
		reg_length = data[lut_offset++];

		pr_info("shall: %d, offset: %d, length: %d", i, reg_offset, reg_length);
		for (j = 0; j < reg_length; j++) {
			value = data[lut_offset++];
			pr_info("offset: %d, value: %x\n", reg_offset + (j * 4), value);
		}
	}

	for (i = 0; i < need; i++) {
		reg_offset = data[lut_offset++];
		reg_length = data[lut_offset++];

		pr_info("need: %d, offset: %d, length: %d", i, reg_offset, reg_length);
		for (j = 0; j < reg_length; j++) {
			value = data[lut_offset++];
			pr_info("offset: %d, value: %x\n", reg_offset + (j * 4), value);
		}
	}

}

static void print_hdr_data(const struct hdr_coef_header *header)
{
	pr_info("hdr header info -> size: %d, log_level: %d, shall: %d, need: %d, con_reg: %x\n",
		header->total_bytesize, header->type.unpack.log_level,
		header->num.unpack.shall, header->num.unpack.need, header->sfr_con);

	print_reg_data(header->num.unpack.shall, header->num.unpack.need, (u32 *)header);

}
#endif
static int check_hdr_data(struct exynos_hdr *hdr, struct hdr_coef_header *header)
{
	unsigned char hw_type;

	if (!hdr || !header) {
		pr_err("invalid argument\n");
		goto error;
	}

	if (!header->total_bytesize || (header->total_bytesize < sizeof(struct hdr_coef_header))) {
		hdr_err(hdr, "invalid size: total %d\n", header->total_bytesize);
		goto error;
	}

	if (header->type.unpack.layer_index != hdr->id) {
		hdr_err(hdr, "invalid layer index %d\n", header->type.unpack.layer_index);
		goto error;
	}

	hw_type = header->type.unpack.hw_type;
	if (hw_type != HDR_HW_TYPE_2) {
		hdr_err(hdr, "invalid hw type %d\n", header->type.unpack.hw_type);
		goto error;
	}
	return 0;

error:
	return -EINVAL;
}

static int hdr_update_context(struct exynos_hdr *hdr)
{
	int ret;
	struct hdr_coef_header *header = NULL;
	struct hdr_context *ctx = NULL;

	mutex_lock(&hdr->ctx_list_lock);
	if (list_empty(&hdr->ctx_list)) {
		mutex_unlock(&hdr->ctx_list_lock);
		return -1;
	}

	ctx = list_first_entry(&hdr->ctx_list, struct hdr_context, list);
	list_del(&ctx->list);
	mutex_unlock(&hdr->ctx_list_lock);

	header = (struct hdr_coef_header *)ctx->data;
	if (!header) {
		hdr_err(hdr, "no allocated virtual buffer\n");
		goto error;
	}

	ret = check_hdr_data(hdr, header);
	if (ret) {
		hdr_err(hdr, "invalid header data\n");
		goto error_dump;
	}

	ret = hdr_update_reg(hdr, header);
	if (ret) {
		hdr_err(hdr, "failed to update hdr reg\n");
		goto error_dump;
	}

	hdr_release_context(hdr, ctx);
	return 0;
error_dump:
	hdr_print_hex_dump(hdr, 0, header->total_bytesize, header);
error:
	hdr_err(hdr, "%s parsing error\n", __func__);
	hdr_release_context(hdr, ctx);
	return -1;
}

static int hdr_remap_regs(struct exynos_hdr *hdr, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int i;
	u32 id = hdr->id;
	struct resource res;

	i = of_property_match_string(np, "reg-names", "mcd_hdr");
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

	hdr_regs_desc_init(id, hdr->regs.hdr_base_regs, "mcd_hdr", REGS_HDR_LSI);

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
			const struct exynos_drm_plane_state *exynos_plane_state)
{
	hdr_debug(hdr, "%s +\n", __func__);
	hdr_prepare_context(hdr, exynos_plane_state);
	hdr_debug(hdr, "%s -\n", __func__);
}

static void __exynos_hdr_update(struct exynos_hdr *hdr)
{
	int ret;

	hdr_debug(hdr, "%s +\n", __func__);
	ret = hdr_update_context(hdr);
	if (ret == 0) {
		hdr->state = HDR_STATE_ENABLE;
		wake_up(&hdr->wait_update);
	}
	hdr_debug(hdr, "%s -\n", __func__);
}

static void __exynos_hdr_disable(struct exynos_hdr *hdr)
{
	hdr_debug(hdr, "%s +\n", __func__);
	mutex_lock(&hdr->ctx_list_lock);
	if (!list_empty(&hdr->ctx_list))
		list_del_init(&hdr->ctx_list);
	mutex_unlock(&hdr->ctx_list_lock);
	hdr_release_context(hdr, NULL);
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
			const struct exynos_drm_plane_state *exynos_plane_state)
{
	const struct exynos_hdr_funcs *funcs;

	if (!hdr || hdr->initialized == false || !exynos_plane_state)
		return;

	funcs = hdr->funcs;
	if (funcs)
		funcs->prepare(hdr, exynos_plane_state);
}

void exynos_hdr_update(struct exynos_hdr *hdr)
{
	const struct exynos_hdr_funcs *funcs;

	if (!hdr || hdr->initialized == false)
		return;

	funcs = hdr->funcs;
	if (funcs)
		funcs->update(hdr);
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

	dpp->attr |= (1 << DPP_ATTR_C_HDR);
	dpp->attr |= (1 << DPP_ATTR_C_HDR10_PLUS);
	dpp->attr |= (1 << DPP_ATTR_WCG);

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

	INIT_LIST_HEAD(&hdr->ctx_list);
	mutex_init(&hdr->ctx_list_lock);
	mutex_init(&hdr->lock);
	init_waitqueue_head(&hdr->wait_update);
	hdr->funcs = &hdr_funcs;
	hdr->state = HDR_STATE_DISABLE;
	hdr->initialized = true;
	hdr->enable = false;

	hdr_info(hdr, "HDR/WCG supported\n");

	return hdr;
}
