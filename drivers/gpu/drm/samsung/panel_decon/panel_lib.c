// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <asm-generic/errno-base.h>
#include <linux/string.h>

#include "panel_lib.h"
#include "panel_debug.h"

/* rdinfo */
int panel_lib_rdinfo_alloc_buffer(struct rdinfo *m)
{
	if (!m || !m->len)
		return -EINVAL;

	if (m->data) {
		panel_err("already have buffer\n");
		return -EINVAL;
	}

	m->data = kzalloc(m->len, GFP_KERNEL);
	if (!m->data)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(panel_lib_rdinfo_alloc_buffer);

void panel_lib_rdinfo_free_buffer(struct rdinfo *m)
{
	if (!m)
		return;

	kfree(m->data);
	m->data = NULL;
}
EXPORT_SYMBOL(panel_lib_rdinfo_free_buffer);

struct rdinfo *panel_lib_rdinfo_create(u32 type, char *name, u32 addr, u32 offset, u32 len, u8 *init_data)
{
	struct rdinfo *m;
	int ret;

	if (unlikely(!name) || unlikely(!len) || unlikely(!addr)) {
		panel_err("invalid parameter\n");
		return NULL;
	}

	if (type < CMD_TYPE_RX_PKT_START || type > CMD_TYPE_RX_PKT_END) {
		panel_err("invalid type(%d)\n", type);
		return NULL;
	}

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return NULL;

	m->type = type;
	m->name = name;
	m->addr = addr;
	m->offset = offset;
	m->len = len;

	if (init_data && len) {
		ret = panel_lib_rdinfo_alloc_buffer(m);
		if (ret < 0) {
			panel_err("failed to alloc rdinfo buffer(ret:%d)\n", ret);
			goto err;
		}

		memcpy(m->data, init_data, m->len);
	}

	return m;

err:
	kfree(m);
	return NULL;
}
EXPORT_SYMBOL(panel_lib_rdinfo_create);

void panel_lib_rdinfo_destroy(struct rdinfo *m)
{
	panel_lib_rdinfo_free_buffer(m);
	kfree(m);
}
EXPORT_SYMBOL(panel_lib_rdinfo_destroy);

int panel_lib_rdinfo_copy(struct rdinfo *dst, struct rdinfo *src)
{
	int ret = 0;
	struct rdinfo *temp_dst;

	if (!dst || !src)
		return -EINVAL;

	if (dst == src)
		return -EINVAL;


	if (unlikely(!src->name) || unlikely(!src->len) || unlikely(!src->addr)) {
		panel_err("invalid parameter\n");
		return  -EINVAL;
	}

	if (src->type < CMD_TYPE_RX_PKT_START || src->type > CMD_TYPE_RX_PKT_END) {
		panel_err("invalid type(%d)\n", src->type);
		return  -EINVAL;
	}

	temp_dst = kzalloc(sizeof(struct rdinfo), GFP_KERNEL);
	if (!temp_dst)
		return -ENOMEM;

	memcpy(temp_dst, src, sizeof(struct rdinfo));
	temp_dst->data = NULL;

	if (src->data) {
		ret = panel_lib_rdinfo_alloc_buffer(temp_dst);
		if (ret < 0) {
			panel_err("failed to alloc rdinfo buffer(ret:%d)\n", ret);
			goto err;
		}

		memcpy(temp_dst->data, src->data, src->len);
	}

	if (dst->data)
		panel_lib_rdinfo_free_buffer(dst);

	memcpy(dst, temp_dst, sizeof(struct rdinfo));
err:
	kfree(temp_dst);
	return ret;
}
EXPORT_SYMBOL(panel_lib_rdinfo_copy);

struct res_update_info *panel_lib_res_update_info_create(u32 offset, struct rdinfo *rditbl)
{
	struct res_update_info *resui;

	if (!rditbl) {
		panel_err("invalid parameter\n");
		return NULL;
	}

	resui = kzalloc(sizeof(struct res_update_info), GFP_KERNEL);
	if (!resui)
		return NULL;

	resui->offset = offset;
	resui->rditbl = rditbl;

	return resui;
}
EXPORT_SYMBOL(panel_lib_res_update_info_create);

void panel_lib_res_update_info_destroy(struct res_update_info *resui)
{
	kfree(resui);
}
EXPORT_SYMBOL(panel_lib_res_update_info_destroy);

int panel_lib_res_update_info_copy(struct res_update_info *dst, struct res_update_info *src)
{
	if (!dst || !src)
		return -EINVAL;

	if (dst == src)
		return -EINVAL;

	memcpy(dst, src, sizeof(struct res_update_info));

	return 0;
}
EXPORT_SYMBOL(panel_lib_res_update_info_copy);

int panel_lib_resinfo_alloc_buffer(struct resinfo *m)
{
	if (!m || !m->dlen)
		return -EINVAL;

	if (m->data) {
		panel_err("already have buffer\n");
		return -EINVAL;
	}

	m->data = kzalloc(m->dlen, GFP_KERNEL);
	if (!m->data)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(panel_lib_resinfo_alloc_buffer);

void panel_lib_resinfo_free_buffer(struct resinfo *m)
{
	if (!m)
		return;

	kfree(m->data);
	m->data = NULL;
}
EXPORT_SYMBOL(panel_lib_resinfo_free_buffer);

struct resinfo *panel_lib_resinfo_create(char *name, u8 *init_data, u32 dlen, struct res_update_info *resui, u32 nr_resui)
{
	struct resinfo *m;
	int ret;

	if (!name || !dlen || !resui) {
		panel_err("invalid parameter\n");
		return NULL;
	}

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return NULL;

	m->type = CMD_TYPE_RES;
	m->name = name;
	m->state = RES_UNINITIALIZED;
	m->dlen = dlen;
	m->resui = resui;
	m->nr_resui = nr_resui;

	if (init_data) {
		ret = panel_lib_resinfo_alloc_buffer(m);
		if (ret < 0) {
			panel_err("failed to alloc resinfo buffer(ret:%d)\n", ret);
			goto err;
		}

		memcpy(m->data, init_data, m->dlen);
	}

	return m;
err:
	kfree(m);
	return NULL;
}
EXPORT_SYMBOL(panel_lib_resinfo_create);

void panel_lib_resinfo_destroy(struct resinfo *m)
{
	panel_lib_resinfo_free_buffer(m);
	kfree(m);
}
EXPORT_SYMBOL(panel_lib_resinfo_destroy);

int panel_lib_resinfo_copy(struct resinfo *dst, struct resinfo *src)
{
	int ret = 0;
	struct resinfo *temp_dst;

	if (!dst || !src)
		return -EINVAL;

	if (dst == src)
		return -EINVAL;

	if (unlikely(!src->name) || unlikely(!src->dlen) || unlikely((src->state < 0 || src->state >= MAX_RES_INIT_STATE))) {
		panel_err("invalid parameter\n");
		return  -EINVAL;
	}

	if (src->type != CMD_TYPE_RES) {
		panel_err("invalid type(%d)\n", src->type);
		return  -EINVAL;
	}

	temp_dst = kzalloc(sizeof(struct resinfo), GFP_KERNEL);
	if (!temp_dst)
		return -ENOMEM;

	memcpy(temp_dst, src, sizeof(struct resinfo));
	temp_dst->data = NULL;

	if (src->data) {
		ret = panel_lib_resinfo_alloc_buffer(temp_dst);
		if (ret < 0) {
			panel_err("failed to alloc resinfo buffer(ret:%d)\n", ret);
			goto err;
		}

		memcpy(temp_dst->data, src->data, src->dlen);
	}

	if (dst->data)
		panel_lib_resinfo_free_buffer(dst);

	memcpy(dst, temp_dst, sizeof(struct resinfo));
err:
	kfree(temp_dst);
	return ret;
}
EXPORT_SYMBOL(panel_lib_resinfo_copy);

struct dumpinfo *panel_lib_dumpinfo_create(char *name, struct resinfo *res, dump_cb_t callback)
{
	struct dumpinfo *m;

	if (unlikely(!name) || unlikely(!res) || unlikely(!callback)) {
		panel_err("invalid parameter\n");
		return NULL;
	}

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return NULL;

	m->type = CMD_TYPE_DMP;
	m->name = name;
	m->res = res;
	m->callback = callback;

	return m;
}
EXPORT_SYMBOL(panel_lib_dumpinfo_create);

void panel_lib_dumpinfo_destroy(struct dumpinfo *m)
{
	kfree(m);
}
EXPORT_SYMBOL(panel_lib_dumpinfo_destroy);

int panel_lib_dumpinfo_copy(struct dumpinfo *dst, struct dumpinfo *src)
{
	if (!dst || !src)
		return -EINVAL;

	if (dst == src)
		return -EINVAL;

	memcpy(dst, src, sizeof(struct dumpinfo));

	return 0;
}
EXPORT_SYMBOL(panel_lib_dumpinfo_copy);
