// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_DUMP_H__
#define __PANEL_DUMP_H__

struct dumpinfo;
typedef int (*dump_show_t)(struct dumpinfo *dump);

struct dump_expect {
	u32 offset;
	u8 mask;
	u8 value;
	char *msg;
};

struct dump_ops {
	struct pnobj_func *show;
};

enum {
	DUMP_STATUS_NONE,
	DUMP_STATUS_FAILURE,
	DUMP_STATUS_SUCCESS,
};

struct dumpinfo {
	struct pnobj base;
	struct resinfo *res;
	struct dump_ops ops;
	struct dump_expect *expects;
	unsigned int nr_expects;
	char **abd_print;
	int result;
};

#define call_dump_func(_dump) \
	((_dump) && (_dump)->ops.show && (_dump)->ops.show->symaddr ? \
	 ((dump_show_t)(_dump)->ops.show->symaddr)(_dump) : -EINVAL)

#define DUMPINFO_INIT(_dumpname, _resource, _show)	\
	{ .base = __PNOBJ_INITIALIZER(_dumpname, CMD_TYPE_DMP) \
	, .res = (_resource) \
	, .ops = { .show = (_show) }}

#define DUMPINFO_INIT_V2(_dumpname, _resource, _show, _expects)	\
	{ .base = __PNOBJ_INITIALIZER(_dumpname, CMD_TYPE_DMP) \
	, .res = (_resource) \
	, .ops = { .show = (_show) } \
	, .expects = (_expects) \
	, .nr_expects = ARRAY_SIZE(_expects) }

static inline char *get_dump_name(struct dumpinfo *dump)
{
	return get_pnobj_name(&dump->base);
}

struct dumpinfo *create_dumpinfo(char *name,
		struct resinfo *res, struct dump_ops *ops,
		struct dump_expect *expects, unsigned int nr_expects);
void destroy_dumpinfo(struct dumpinfo *dump);

#endif /* __PANEL_DUMP_H__ */
