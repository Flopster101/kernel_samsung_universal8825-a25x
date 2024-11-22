/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_OBJ_H__
#define __PANEL_OBJ_H__

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/version.h>

#define PNOBJ_NAME_LEN (128)

#define pnobj_container_of(pnobj, type) \
	container_of(pnobj, type, base)

/* pnobj type */
enum {
	PNOBJ_TYPE_NONE,
	/* independant objects */
	PNOBJ_TYPE_PROP,
	PNOBJ_TYPE_FUNC,
	PNOBJ_TYPE_MAP,
	PNOBJ_TYPE_DELAY,
	PNOBJ_TYPE_CONDITION,
	PNOBJ_TYPE_PWRCTRL,
	PNOBJ_TYPE_RX_PACKET,
	/* dependant objects */
	PNOBJ_TYPE_CONFIG, /* config -> property */
	PNOBJ_TYPE_TX_PACKET,	/* packet -> maptbl */
	PNOBJ_TYPE_KEY,	/* key -> packet */
	PNOBJ_TYPE_RESOURCE, /* resource -> readinfo(packet) */
	PNOBJ_TYPE_DUMP, /* dump -> resource */
	PNOBJ_TYPE_SEQUENCE, /* sequence -> all pnobj */
	MAX_PNOBJ_TYPE,
};

/* panel object */
struct pnobj {
	unsigned int cmd_type;
	char *name;
	struct list_head list;
};

/* panel object reference */
struct pnobj_ref {
	struct pnobj *pnobj;
	struct list_head list;
};

struct pnobj_refs {
	struct list_head list;
};

static inline void set_pnobj_cmd_type(struct pnobj *base, unsigned int type)
{
	base->cmd_type = type;
}

static inline unsigned int get_pnobj_cmd_type(struct pnobj *base)
{
	return base->cmd_type;
}

static inline void set_pnobj_name(struct pnobj *base, char *name)
{
	base->name = kstrndup(name, PNOBJ_NAME_LEN-1, GFP_KERNEL);
}

static inline char *get_pnobj_name(struct pnobj *base)
{
	return base->name;
}

static inline void free_pnobj_name(struct pnobj *base)
{
	kfree(base->name);
}

static inline struct list_head *get_pnobj_list(struct pnobj *base)
{
	return &base->list;
}

static inline void delete_pnobj_list(struct pnobj *base)
{
	list_del(get_pnobj_list(base));
}

static inline void pnobj_init(struct pnobj *base, u32 type, char *name)
{
	set_pnobj_cmd_type(base, type);
	set_pnobj_name(base, name);
	INIT_LIST_HEAD(&base->list);
}

static inline void pnobj_deinit(struct pnobj *base)
{
	delete_pnobj_list(base);
	free_pnobj_name(base);
}

#define __PNOBJ_INITIALIZER(_pnobjname, _cmdtype) \
{ .name = (#_pnobjname), .cmd_type = (_cmdtype) }

static inline void INIT_PNOBJ_REFS(struct pnobj_refs *refs)
{
	INIT_LIST_HEAD(&refs->list);
}

static inline struct list_head *get_pnobj_refs_list(struct pnobj_refs *pnobj_refs)
{
	return &pnobj_refs->list;
}

bool is_valid_panel_obj(struct pnobj *pnobj);
const char *pnobj_type_to_string(u32 type);
unsigned int cmd_type_to_pnobj_type(unsigned int cmd_type);
struct pnobj *pnobj_find_by_name(struct list_head *head, char *name);
struct pnobj *pnobj_find_by_substr(struct list_head *head, char *substr);
struct pnobj *pnobj_find_by_pnobj(struct list_head *head, struct pnobj *pnobj);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
int pnobj_compare(void *priv,
		struct list_head *a, struct list_head *b);
#else
int pnobj_compare(void *priv,
		const struct list_head *a, const struct list_head *b);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
int pnobj_ref_compare(void *priv,
		struct list_head *a, struct list_head *b);
#else
int pnobj_ref_compare(void *priv,
		const struct list_head *a, const struct list_head *b);
#endif
struct pnobj_refs *create_pnobj_refs(void);
int add_pnobj_ref(struct pnobj_refs *pnobj_refs, struct pnobj *pnobj);
int get_count_of_pnobj_ref(struct pnobj_refs *pnobj_refs);
void remove_all_pnobj_ref(struct pnobj_refs *pnobj_refs);
void remove_pnobj_refs(struct pnobj_refs *pnobj_refs);
struct pnobj_refs *pnobj_refs_filter(bool (*filter_func)(struct pnobj *), struct pnobj_refs *orig_refs);
struct pnobj_refs *pnobj_list_to_pnobj_refs(struct list_head *pnobj_list);
#endif /* __PANEL_OBJ_H__ */
