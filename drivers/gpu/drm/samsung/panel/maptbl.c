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

#include "panel_obj.h"
#include "maptbl.h"
#include "panel_debug.h"
#include "panel_property.h"

int maptbl_alloc_buffer(struct maptbl *m, size_t size)
{
	if (!m || !size)
		return -EINVAL;

	m->arr = kvmalloc(size, GFP_KERNEL);
	if (!m->arr)
		return -ENOMEM;

	return 0;
}

void maptbl_free_buffer(struct maptbl *m)
{
	if (!m)
		return;

	kvfree(m->arr);
	m->arr = NULL;
}

void maptbl_set_shape(struct maptbl *m, struct maptbl_shape *shape)
{
	if (!m || !shape)
		return;

	memcpy(&m->shape, shape, sizeof(*shape));
}

void maptbl_set_sizeof_copy(struct maptbl *m, unsigned int sizeof_copy)
{
	if (!m)
		return;

	m->sz_copy = sizeof_copy;
}

void maptbl_set_ops(struct maptbl *m, struct maptbl_ops *ops)
{
	if (!m || !ops)
		return;

	memcpy(&m->ops, ops, sizeof(*ops));
}

void maptbl_set_props(struct maptbl *m, struct maptbl_props *props)
{
	if (!m || !props)
		return;

	memcpy(&m->props, props, sizeof(*props));
}

void maptbl_set_initialized(struct maptbl *m, bool initialized)
{
	if (!m)
		return;

	m->initialized = initialized;
}

void maptbl_set_private_data(struct maptbl *m, void *priv)
{
	if (!m)
		return;

	m->pdata = priv;
}

void *maptbl_get_private_data(struct maptbl *m)
{
	if (!m)
		return NULL;

	return m->pdata;
}

/**
 * maptbl_create - create a struct maptbl structure
 * @name: pointer to a string for the name of this maptbl.
 * @shape: pointer to struct maptbl_shape for the shape of this maptbl.
 * @ops: pointer to struct maptbl_ops for the operations of this maptbl.
 * @init_data: pointer to buffer to initialize array buffer to be allocated.
 * @priv: pointer to private_data; passing when maptbl_init() invoked.
 *
 * This is used to create a struct maptbl pointer.
 *
 * Returns &struct maptbl pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to maptbl_destroy().
 */
struct maptbl *maptbl_create(char *name, struct maptbl_shape *shape,
		struct maptbl_ops *ops, struct maptbl_props *props, void *init_data, void *priv)
{
	struct maptbl *m;
	int ret;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m)
		return NULL;

	pnobj_init(&m->base, CMD_TYPE_MAP, name);
	maptbl_set_shape(m, shape);
	maptbl_set_sizeof_copy(m,
			maptbl_get_countof_n_dimen_element(m, NDARR_1D));
	maptbl_set_ops(m, ops);
	maptbl_set_props(m, props);
	maptbl_set_private_data(m, priv);

	if (!maptbl_get_sizeof_maptbl(m))
		return m;

	ret = maptbl_alloc_buffer(m, maptbl_get_sizeof_maptbl(m));
	if (ret < 0) {
		panel_err("failed to alloc maptbl buffer(ret:%d)\n", ret);
		goto err;
	}

	memcpy(m->arr, init_data, maptbl_get_sizeof_maptbl(m));

	return m;

err:
	maptbl_destroy(m);
	return NULL;
}
EXPORT_SYMBOL(maptbl_create);

/**
 * maptbl_clone - create a struct maptbl structure by source maptbl
 * @src: pointer to struct maptbl; used by passing argument to maptbl_create() function.
 *
 * This is used to create a struct maptbl pointer by source maptbl.
 *
 * Returns &struct maptbl pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to maptbl_destroy().
 */
struct maptbl *maptbl_clone(struct maptbl *src)
{
	struct maptbl *dst;

	if (!src)
		return NULL;

	dst = maptbl_create(maptbl_get_name(src), &src->shape, &src->ops,
			&src->props, src->arr, maptbl_get_private_data(src));
	maptbl_set_sizeof_copy(dst, src->sz_copy);

	return dst;
}
EXPORT_SYMBOL(maptbl_clone);

/**
 * maptbl_deepcopy - copy members of struct maptbl structure from maptbl
 * @dst: pointer to destination struct maptbl
 * @src: pointer to source struct maptbl
 *
 * This is used to copy to destination maptbl from source maptbl.
 *
 * Returns &struct maptbl pointer to destination maptbl on success, or NULL on error.
 *
 * Note, the pointer to be free when finished by making a call to maptbl_free_buffer().
 */
struct maptbl *maptbl_deepcopy(struct maptbl *dst, struct maptbl *src)
{
	int ret;

	if (!dst || !src)
		return NULL;

	if (dst == src)
		return dst;

	pnobj_init(&dst->base, CMD_TYPE_MAP, maptbl_get_name(src));
	maptbl_set_shape(dst, &src->shape);
	maptbl_set_sizeof_copy(dst, src->sz_copy);
	maptbl_set_ops(dst, &src->ops);
	maptbl_set_props(dst, &src->props);
	maptbl_set_private_data(dst, maptbl_get_private_data(src));
	maptbl_free_buffer(dst);

	if (!src->arr || !maptbl_get_sizeof_maptbl(src))
		return dst;

	ret = maptbl_alloc_buffer(dst, maptbl_get_sizeof_maptbl(src));
	if (ret < 0) {
		panel_err("failed to alloc maptbl buffer(ret:%d)\n", ret);
		pnobj_deinit(&dst->base);
		return NULL;
	}

	memcpy(dst->arr, src->arr, maptbl_get_sizeof_maptbl(src));

	return dst;
}
EXPORT_SYMBOL(maptbl_deepcopy);

/**
 * maptbl_destroy - destroys a struct maptbl structure
 * @m: pointer to the struct maptbl that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to maptbl_create().
 */
void maptbl_destroy(struct maptbl *m)
{
	if (!m)
		return;

	pnobj_deinit(&m->base);
	maptbl_free_buffer(m);
	kfree(m);
}
EXPORT_SYMBOL(maptbl_destroy);

int maptbl_get_indexof_n_dimen_element(struct maptbl *tbl, enum ndarray_dimen dimen, unsigned int indexof_element)
{
	if (!tbl)
		return -EINVAL;

	if (indexof_element >= maptbl_get_countof_n_dimen_element(tbl, dimen)) {
		panel_err("%s: out of bound(indexof_%dD_element:%d >= countof_%dD_element:%d)\n",
				maptbl_get_name(tbl), dimen + 1, indexof_element,
				dimen + 1, maptbl_get_countof_n_dimen_element(tbl, dimen));
		return -EINVAL;
	}

	return maptbl_get_sizeof_n_dimen_element(tbl, dimen) * indexof_element;
}
EXPORT_SYMBOL(maptbl_get_indexof_n_dimen_element);

int maptbl_get_indexof_box(struct maptbl *tbl, unsigned int indexof_element)
{
	return maptbl_get_indexof_n_dimen_element(tbl, NDARR_4D, indexof_element);
}
EXPORT_SYMBOL(maptbl_get_indexof_box);

int maptbl_get_indexof_layer(struct maptbl *tbl, unsigned int indexof_element)
{
	return maptbl_get_indexof_n_dimen_element(tbl, NDARR_3D, indexof_element);
}
EXPORT_SYMBOL(maptbl_get_indexof_layer);

int maptbl_get_indexof_row(struct maptbl *tbl, unsigned int indexof_element)
{
	return maptbl_get_indexof_n_dimen_element(tbl, NDARR_2D, indexof_element);
}
EXPORT_SYMBOL(maptbl_get_indexof_row);

int maptbl_get_indexof_col(struct maptbl *tbl, unsigned int indexof_element)
{
	return maptbl_get_indexof_n_dimen_element(tbl, NDARR_1D, indexof_element);
}
EXPORT_SYMBOL(maptbl_get_indexof_col);

int maptbl_4d_index(struct maptbl *tbl, int box, int layer, int row, int col)
{
	int dimen, res = 0;
	int index, indexof_element;

	if (!tbl)
		return -EINVAL;

	maptbl_for_each_dimen(tbl, dimen) {
		if (dimen == NDARR_1D)
			indexof_element = col;
		else if (dimen == NDARR_2D)
			indexof_element = row;
		else if (dimen == NDARR_3D)
			indexof_element = layer;
		else if (dimen == NDARR_4D)
			indexof_element = box;

		index = maptbl_get_indexof_n_dimen_element(tbl, dimen, indexof_element);
		if (index < 0)
			return -EINVAL;

		res += index;
	}

	return res;
}
EXPORT_SYMBOL(maptbl_4d_index);

int maptbl_index(struct maptbl *tbl, int layer, int row, int col)
{
	if (!tbl)
		return -EINVAL;

	return maptbl_4d_index(tbl, 0, layer, row, col);
}
EXPORT_SYMBOL(maptbl_index);

int maptbl_pos_to_index(struct maptbl *tbl, struct maptbl_pos *pos)
{
	int dimen, res = 0;
	int indexof_element;

	if (!tbl || !pos)
		return -EINVAL;

	maptbl_for_each_dimen(tbl, dimen) {
		indexof_element = maptbl_get_indexof_n_dimen_element(tbl,
				dimen, pos->index[dimen]);
		if (indexof_element < 0)
			return -EINVAL;

		res += indexof_element;
	}

	return res;
}
EXPORT_SYMBOL(maptbl_pos_to_index);

int maptbl_index_to_pos(struct maptbl *tbl, unsigned int index, struct maptbl_pos *pos)
{
	int dimen;
	int sizeof_element;

	if (!tbl || !pos)
		return -EINVAL;

	if (index >= maptbl_get_sizeof_maptbl(tbl))
		return -EINVAL;

	maptbl_for_each_dimen_reverse(tbl, dimen) {
		sizeof_element = maptbl_get_sizeof_n_dimen_element(tbl, dimen);
		if (sizeof_element <= 0)
			return -EINVAL;

		pos->index[dimen] = index / sizeof_element;
		index = index % sizeof_element;
	}

	return 0;
}
EXPORT_SYMBOL(maptbl_index_to_pos);

bool maptbl_is_initialized(struct maptbl *tbl)
{
	return (tbl && tbl->initialized);
}
EXPORT_SYMBOL(maptbl_is_initialized);

bool maptbl_is_index_in_bound(struct maptbl *tbl, unsigned int index)
{
	return (tbl && (index + maptbl_get_sizeof_copy(tbl) <= maptbl_get_sizeof_maptbl(tbl)));
}
EXPORT_SYMBOL(maptbl_is_index_in_bound);

int maptbl_init(struct maptbl *tbl)
{
	int ret;

	if (!tbl)
		return -EINVAL;

	if (!maptbl_get_name(tbl))
		return -ENODATA;

	if (!tbl->ops.init) {
		panel_err("%s:no init callback\n", maptbl_get_name(tbl));
		return -EINVAL;
	}

	ret = call_maptbl_init(tbl);
	if (ret < 0) {
		panel_err("%s:failed to init(ret:%d)\n",
				maptbl_get_name(tbl), ret);
		return ret;
	}

	maptbl_set_initialized(tbl, true);
	pr_info("maptbl(%s) init\n", maptbl_get_name(tbl));

	return 0;
}
EXPORT_SYMBOL(maptbl_init);

int maptbl_getidx(struct maptbl *tbl)
{
	int index;

	if (!tbl)
		return -EINVAL;

	if (!tbl->ops.getidx) {
		panel_err("%s:no getidx callback\n", maptbl_get_name(tbl));
		return -EINVAL;
	}

	if (!maptbl_is_initialized(tbl)) {
		panel_err("%s:not initialized\n", maptbl_get_name(tbl));
		return -EINVAL;
	}

	index = call_maptbl_getidx(tbl);
	if (index < 0) {
		panel_err("%s:failed to getidx(ret:%d)\n", maptbl_get_name(tbl), index);
		return -EINVAL;
	}

	if (!maptbl_is_index_in_bound(tbl, index)) {
		panel_err("%s:out of bound(index:%d, sizeof_copy:%d, sizeof_maptbl:%d)\n",
				maptbl_get_name(tbl), index, maptbl_get_sizeof_copy(tbl), maptbl_get_sizeof_maptbl(tbl));
		return -EINVAL;
	}

	return index;
}
EXPORT_SYMBOL(maptbl_getidx);

int maptbl_copy(struct maptbl *tbl, u8 *dst)
{
	if (!tbl || !dst)
		return -EINVAL;

	if (!tbl->ops.copy) {
		panel_err("%s:no copy callback\n", maptbl_get_name(tbl));
		return -EINVAL;
	}

	if (!maptbl_is_initialized(tbl)) {
		panel_err("%s:not initialized\n", maptbl_get_name(tbl));
		return -EINVAL;
	}

	call_maptbl_copy(tbl, dst);

	return 0;
}
EXPORT_SYMBOL(maptbl_copy);

int maptbl_fill(struct maptbl *tbl, struct maptbl_pos *pos, u8 *src, size_t n)
{
	u32 index;

	if (!tbl || !tbl->arr || !pos || !src || !n) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	index = maptbl_pos_to_index(tbl, pos);
	if (maptbl_get_sizeof_maptbl(tbl) <= index) {
		panel_err("index(%d) exceed maptbl size\n", index);
		return -EINVAL;
	}

	if (maptbl_get_sizeof_row(tbl) < n) {
		panel_err("size(%ld) exceed row(%d) size\n",
				n, maptbl_get_sizeof_row(tbl));
		return -EINVAL;
	}

	memcpy(tbl->arr + index, src, n);

	return 0;
}
EXPORT_SYMBOL(maptbl_fill);

int maptbl_cmp_shape(struct maptbl *m1, struct maptbl *m2)
{
	int dimen, res = 0;

	if (m1 == m2)
		return 0;

	if (!m1 || !m2)
		return !m1 ? -1 : 1;

	if ((res = maptbl_get_countof_dimen(m1) -
				maptbl_get_countof_dimen(m2)) != 0)
		return res;

	maptbl_for_each_dimen_reverse(m1, dimen)
		if ((res = maptbl_get_countof_n_dimen_element(m1, dimen) -
					maptbl_get_countof_n_dimen_element(m2, dimen)) != 0)
			break;

	return res;
}
EXPORT_SYMBOL(maptbl_cmp_shape);

struct maptbl *maptbl_memcpy(struct maptbl *dst, struct maptbl *src)
{
	if (!src || !dst)
		return NULL;

	if (!src->arr || !dst->arr)
		return NULL;

	if (maptbl_cmp_shape(src, dst)) {
		panel_err("failed to copy different shape of maptbl, src:%s(%d), dst:%s(%d)\n",
				maptbl_get_name(src), maptbl_get_sizeof_maptbl(src),
				maptbl_get_name(dst), maptbl_get_sizeof_maptbl(dst));
		return NULL;
	}

	memcpy(dst->arr, src->arr, maptbl_get_sizeof_maptbl(dst));

	return dst;
}
EXPORT_SYMBOL(maptbl_memcpy);

int maptbl_snprintf_head(struct maptbl *tbl, char *buf, size_t size)
{
	int i, len;

	if (!tbl || !buf || !size)
		return 0;

	len = snprintf(buf, size, "%dD-MAPTBL:%s",
			maptbl_get_countof_dimen(tbl), maptbl_get_name(tbl));
	maptbl_for_each_dimen_reverse(tbl, i)
		len += snprintf(buf + len, size - len, "[%d]",
				maptbl_get_countof_n_dimen_element(tbl, i));
	len += snprintf(buf + len, size - len, "\n");

	return len;
}

int maptbl_snprintf_body(struct maptbl *tbl, char *buf, size_t size)
{
	int box, layer, row, col, len = 0;
	char *space[MAX_NDARR_DIMEN] = {"", "\t", "\t\t", "\t\t\t", "\t\t\t\t", "\t\t\t\t\t", "\t\t\t\t\t\t", "\t\t\t\t\t\t\t"};

	if (!tbl || !buf || !size)
		return 0;

	len += snprintf(buf + len, size - len, "\n");
	maptbl_for_each_n_dimen_element(tbl, NDARR_4D, box) {
		if (maptbl_get_countof_dimen(tbl) >= 4)
			len += snprintf(buf + len, size - len, "%s[%d] = {\n", space[maptbl_get_countof_dimen(tbl) - 4], box);
		maptbl_for_each_n_dimen_element(tbl, NDARR_3D, layer) {
			if (maptbl_get_countof_dimen(tbl) >= 3)
				len += snprintf(buf + len, size - len, "%s[%d] = {\n", space[maptbl_get_countof_dimen(tbl) - 3], layer);
			maptbl_for_each_n_dimen_element(tbl, NDARR_2D, row) {
				if (maptbl_get_countof_dimen(tbl) >= 2)
					len += snprintf(buf + len, size - len, "%s[%d] = {\n", space[maptbl_get_countof_dimen(tbl) - 2], row);
				maptbl_for_each_n_dimen_element(tbl, NDARR_1D, col) {
					if (!(col % 8))
						len += snprintf(buf + len, size - len, "%s", space[maptbl_get_countof_dimen(tbl) - 1]);
					len += snprintf(buf + len, size - len, "%02X",
							tbl->arr[maptbl_4d_index(tbl, box, layer, row, col)]);
					if (!((col + 1) % 8) ||(col + 1 == maptbl_get_countof_col(tbl)))
						len += snprintf(buf + len, size - len, "\n");
					else
						len += snprintf(buf + len, size - len, " ");
				}
				if (maptbl_get_countof_dimen(tbl) >= 2)
					len += snprintf(buf + len, size - len, "%s},\n", space[maptbl_get_countof_dimen(tbl) - 2]);
			}
			if (maptbl_get_countof_dimen(tbl) >= 3)
				len += snprintf(buf + len, size - len, "%s},\n", space[maptbl_get_countof_dimen(tbl) - 3]);
		}
		if (maptbl_get_countof_dimen(tbl) >= 4)
			len += snprintf(buf + len, size - len, "%s},\n", space[maptbl_get_countof_dimen(tbl) - 4]);
	}

	return len;
}

int maptbl_snprintf_tail(struct maptbl *tbl, char *buf, size_t size)
{
	if (!tbl || !buf || !size)
		return 0;

	return snprintf(buf, size, "=====================================\n");
}

int maptbl_snprintf(struct maptbl *tbl, char *buf, size_t size)
{
	int len;

	if (!tbl || !buf || !size)
		return 0;

	len = maptbl_snprintf_head(tbl, buf, size);
	len += maptbl_snprintf_body(tbl, buf + len, size - len);
	len += maptbl_snprintf_tail(tbl, buf + len, size - len);

	return len;
}

void maptbl_print(struct maptbl *tbl)
{
	char buf[256] = { 0, };

	if (panel_log_level < 7)
		return;

	maptbl_snprintf(tbl, buf, sizeof(buf));
	pr_info("%s\n", buf);
}
EXPORT_SYMBOL(maptbl_print);

static int maptbl_props_to_pos(struct maptbl *m, struct maptbl_pos *pos)
{
	struct panel_device *panel;
	int dimen;

	if (!m || !m->pdata)
		return -EINVAL;

	panel = m->pdata;
	maptbl_for_each_dimen_reverse(m, dimen) {
		int index = 0;

		if (m->props.name[dimen]) {
			index = panel_get_property_value(panel, m->props.name[dimen]);
			if (index < 0) {
				panel_err("%s: failed to get property(%s) value\n",
						maptbl_get_name(m), m->props.name[dimen]);
				return -EINVAL;
			}
		}
		pos->index[dimen] = index;
	}

	return 0;
}

int maptbl_getidx_from_props(struct maptbl *m)
{
	struct maptbl_pos pos;

	if (!m || !m->pdata)
		return -EINVAL;

	memset(&pos, 0, sizeof(pos));
	maptbl_props_to_pos(m, &pos);

	return maptbl_pos_to_index(m, &pos);
}
EXPORT_SYMBOL(maptbl_getidx_from_props);
