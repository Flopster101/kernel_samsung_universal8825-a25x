/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "panel_obj.h"

#ifndef __PANEL_RESOURCE_H__
#define __PANEL_RESOURCE_H__

enum {
	RES_UNINITIALIZED,
	RES_INITIALIZED,
	MAX_RES_INIT_STATE
};

struct res_update_info {
	u32 offset;
	struct rdinfo *rditbl;
};

struct resinfo {
	struct pnobj base;
	int state;
	u8 *data;
	u32 dlen;
	struct res_update_info *resui;
	u32 nr_resui;
};

#define RESUI(_name_) PN_CONCAT(resui, _name_)
#define RESINFO(_name_) PN_CONCAT(res, _name_)
#define RESNAME(_name_) (_name_)

#define RESUI_INIT(_rditbl_, _offset_)\
{												\
	.offset = (_offset_),						\
	.rditbl = (_rditbl_),						\
}

#define DECLARE_RESUI(_name_) \
struct res_update_info RESUI(_name_)[]

#define DEFINE_RESUI(_name_, _rditbl_, _offset_)	\
struct res_update_info RESUI(_name_)[] =			\
{													\
	RESUI_INIT(_rditbl_, _offset_),		\
}

#define RESINFO_INIT(_resname, _arr_, _resui_)	\
	{ .base = __PNOBJ_INITIALIZER(_resname, CMD_TYPE_RES) \
	, .state = (RES_UNINITIALIZED) \
	, .data = (_arr_) \
	, .dlen = ARRAY_SIZE((_arr_)) \
	, .resui = (_resui_) \
	, .nr_resui = ARRAY_SIZE(_resui_) }

#define RESINFO_IMMUTABLE_INIT(_resname, _arr_) \
	{ .base = __PNOBJ_INITIALIZER(_resname, CMD_TYPE_RES) \
	, .state = (RES_INITIALIZED) \
	, .data = (_arr_) \
	, .dlen = ARRAY_SIZE((_arr_)) \
	, .resui = (NULL) \
	, .nr_resui = 0 }

#define DEFINE_RESOURCE(_name_, _arr_, _resui_)	\
struct resinfo RESINFO(_name_) = RESINFO_INIT(_name_, _arr_, _resui_)

#define DEFINE_IMMUTABLE_RESOURCE(_name_, _arr_) \
struct resinfo RESINFO(_name_) = RESINFO_IMMUTABLE_INIT(_name_, _arr_)

char *get_resource_name(struct resinfo *res);
unsigned int get_resource_size(struct resinfo *res);
int copy_resource(u8 *dst, struct resinfo *res);
int copy_resource_slice(u8 *dst, struct resinfo *res, u32 offset, u32 len);
void set_resource_state(struct resinfo *res, int state);
int snprintf_resource_data(char *buf, size_t size, struct resinfo *res);
int snprintf_resource(char *buf, size_t size, struct resinfo *res);
void print_resource(struct resinfo *res);
bool is_valid_resource(struct resinfo *res);
bool is_resource_initialized(struct resinfo *res);
bool is_resource_mutable(struct resinfo *res);
struct resinfo *create_resource(char *name, u8 *initdata,
		u32 size, struct res_update_info *resui, unsigned int nr_resui);
void destroy_resource(struct resinfo *resource);

#endif
