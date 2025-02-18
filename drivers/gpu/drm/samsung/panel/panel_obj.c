// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_obj.h"

const char *pnobj_type_to_string(u32 pnobj_type)
{
	static const char *pnobj_type_name[MAX_PNOBJ_TYPE] = {
		[PNOBJ_TYPE_NONE] = "NONE",
		[PNOBJ_TYPE_PROP] = "PROPERTY",
		[PNOBJ_TYPE_FUNC] = "FUNCTION",
		[PNOBJ_TYPE_MAP] = "MAP",
		[PNOBJ_TYPE_DELAY] = "DELAY",
		[PNOBJ_TYPE_CONDITION] = "CONDITION",
		[PNOBJ_TYPE_PWRCTRL] = "PWRCTRL",
		[PNOBJ_TYPE_RX_PACKET] = "RX_PACKET",
		[PNOBJ_TYPE_CONFIG] = "CONFIG",
		[PNOBJ_TYPE_TX_PACKET] = "TX_PACKET",
		[PNOBJ_TYPE_KEY] = "KEY",
		[PNOBJ_TYPE_RESOURCE] = "RESOURCE",
		[PNOBJ_TYPE_DUMP] = "DUMP",
		[PNOBJ_TYPE_SEQUENCE] = "SEQUENCE",
	};

	return (pnobj_type < MAX_PNOBJ_TYPE) ?
		pnobj_type_name[pnobj_type] :
		pnobj_type_name[PNOBJ_TYPE_NONE];
}

unsigned int cmd_type_to_pnobj_type(unsigned int cmd_type)
{
	static unsigned int cmd_type_pnboj_type[MAX_CMD_TYPE] = {
		[CMD_TYPE_NONE] = PNOBJ_TYPE_NONE,
		[CMD_TYPE_PROP] = PNOBJ_TYPE_PROP,
		[CMD_TYPE_FUNC] = PNOBJ_TYPE_FUNC,
		[CMD_TYPE_MAP] = PNOBJ_TYPE_MAP,
		[CMD_TYPE_DELAY ... CMD_TYPE_TIMER_DELAY_BEGIN] = PNOBJ_TYPE_DELAY,
		[CMD_TYPE_COND_IF ... CMD_TYPE_COND_FI] = PNOBJ_TYPE_CONDITION,
		[CMD_TYPE_PCTRL] = PNOBJ_TYPE_PWRCTRL,
		[CMD_TYPE_RX_PACKET] = PNOBJ_TYPE_RX_PACKET,
		[CMD_TYPE_CFG] = PNOBJ_TYPE_CONFIG,
		[CMD_TYPE_TX_PACKET] = PNOBJ_TYPE_TX_PACKET,
		[CMD_TYPE_KEY] = PNOBJ_TYPE_KEY,
		[CMD_TYPE_RES] = PNOBJ_TYPE_RESOURCE,
		[CMD_TYPE_DMP] = PNOBJ_TYPE_DUMP,
		[CMD_TYPE_SEQ] = PNOBJ_TYPE_SEQUENCE,
	};

	return (cmd_type < MAX_CMD_TYPE) ?
		cmd_type_pnboj_type[cmd_type] :
		cmd_type_pnboj_type[PNOBJ_TYPE_NONE];
}

bool is_valid_panel_obj(struct pnobj *pnobj)
{
	unsigned int type;

	if (!pnobj)
		return false;

	type = get_pnobj_cmd_type(pnobj);
	if (type == CMD_TYPE_NONE || type >= MAX_CMD_TYPE)
		return false;

	if (!get_pnobj_name(pnobj))
		return false;

	return true;
}

struct pnobj *pnobj_find_by_name(struct list_head *head, char *name)
{
	struct pnobj *pnobj;

	if (!name)
		return NULL;

	list_for_each_entry(pnobj, head, list) {
		if (!get_pnobj_name(pnobj))
			continue;
		if (!strncmp(get_pnobj_name(pnobj), name,
					PNOBJ_NAME_LEN))
			return pnobj;
	}

	return NULL;
}

struct pnobj *pnobj_find_by_substr(struct list_head *head, char *substr)
{
	struct pnobj *pnobj;

	if (!substr)
		return NULL;

	list_for_each_entry(pnobj, head, list) {
		if (!get_pnobj_name(pnobj))
			continue;
		if (strstr(get_pnobj_name(pnobj), substr))
			return pnobj;
	}

	return NULL;
}

struct pnobj *pnobj_find_by_pnobj(struct list_head *head, struct pnobj *_pnobj)
{
	struct pnobj *pnobj;

	if (!is_valid_panel_obj(_pnobj)) {
		panel_warn("invalid pnobj(%s)\n",
				get_pnobj_name(_pnobj));
		return NULL;
	}

	list_for_each_entry(pnobj, head, list) {
		if (!is_valid_panel_obj(pnobj)) {
			panel_warn("invalid pnobj(%s)\n",
					get_pnobj_name(pnobj));
			continue;
		}

		if (get_pnobj_cmd_type(pnobj) != get_pnobj_cmd_type(_pnobj))
			continue;

		if (strncmp(get_pnobj_name(pnobj),
					get_pnobj_name(_pnobj), PNOBJ_NAME_LEN))
			continue;

		return pnobj;
	}

	return NULL;
}

/* Compare two pnobj items. */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
static int pnobj_type_compare(void *priv,
		struct list_head *a, struct list_head *b)
#else
static int pnobj_type_compare(void *priv,
		const struct list_head *a, const struct list_head *b)
#endif
{
	struct pnobj *pnobj_a;
	struct pnobj *pnobj_b;
	unsigned int type_a, type_b;

	pnobj_a = container_of(a, struct pnobj, list);
	pnobj_b = container_of(b, struct pnobj, list);

	type_a = get_pnobj_cmd_type(pnobj_a);
	type_b = get_pnobj_cmd_type(pnobj_b);

	if (cmd_type_to_pnobj_type(type_a) !=
			cmd_type_to_pnobj_type(type_b))
		return cmd_type_to_pnobj_type(type_a) -
			cmd_type_to_pnobj_type(type_b);

	return type_a - type_b;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
static int pnobj_name_compare(void *priv,
		struct list_head *a, struct list_head *b)
#else
static int pnobj_name_compare(void *priv,
		const struct list_head *a, const struct list_head *b)
#endif
{
	struct pnobj *pnobj_a;
	struct pnobj *pnobj_b;
	char *name_a, *name_b;

	pnobj_a = container_of(a, struct pnobj, list);
	pnobj_b = container_of(b, struct pnobj, list);

	name_a = get_pnobj_name(pnobj_a);
	name_b = get_pnobj_name(pnobj_b);

	return strncmp(name_a, name_b, PNOBJ_NAME_LEN);
}

/* Compare two pnobj items. */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
int pnobj_compare(void *priv,
		struct list_head *a, struct list_head *b)
#else
int pnobj_compare(void *priv,
		const struct list_head *a, const struct list_head *b)
#endif
{
	int diff;

	diff = pnobj_type_compare(priv, a, b);
	if (diff)
		return diff;

	return pnobj_name_compare(priv, a, b);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
int pnobj_ref_compare(void *priv,
		struct list_head *a, struct list_head *b)
#else
int pnobj_ref_compare(void *priv,
		const struct list_head *a, const struct list_head *b)
#endif
{
	struct pnobj_ref *ref_a;
	struct pnobj_ref *ref_b;
	int diff;

	ref_a = container_of(a, struct pnobj_ref, list);
	ref_b = container_of(b, struct pnobj_ref, list);

	diff = pnobj_type_compare(priv,
			get_pnobj_list(ref_a->pnobj),
			get_pnobj_list(ref_b->pnobj));
	if (diff)
		return diff;

	return pnobj_name_compare(priv,
			get_pnobj_list(ref_a->pnobj),
			get_pnobj_list(ref_b->pnobj));
}

struct pnobj_refs *create_pnobj_refs(void)
{
	struct pnobj_refs *pnobj_refs;

	pnobj_refs = kzalloc(sizeof(*pnobj_refs), GFP_KERNEL);
	if (!pnobj_refs)
		return NULL;

	INIT_PNOBJ_REFS(pnobj_refs);

	return pnobj_refs;
}

static struct pnobj_ref *create_pnobj_ref(struct pnobj *pnobj)
{
	struct pnobj_ref *ref;

	if (!pnobj)
		return NULL;

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref)
		return NULL;

	ref->pnobj = pnobj;

	return ref;
}

int add_pnobj_ref(struct pnobj_refs *pnobj_refs, struct pnobj *pnobj)
{
	struct pnobj_ref *ref;

	if (!pnobj_refs)
		return -EINVAL;

	if (!is_valid_panel_obj(pnobj)) {
		panel_warn("invalid pnobj(%s)\n",
				get_pnobj_name(pnobj));
		return -EINVAL;
	}

	ref = create_pnobj_ref(pnobj);
	if (!ref)
		return -EINVAL;

	list_add_tail(&ref->list, get_pnobj_refs_list(pnobj_refs));

	return 0;
}

static void del_pnobj_ref(struct pnobj_ref *ref)
{
	list_del(&ref->list);
	kfree(ref);
}

int get_count_of_pnobj_ref(struct pnobj_refs *pnobj_refs)
{
	struct pnobj_ref *ref;
	int count = 0;

	if (!pnobj_refs)
		return -EINVAL;

	list_for_each_entry(ref, get_pnobj_refs_list(pnobj_refs), list) {
		if (!is_valid_panel_obj(ref->pnobj))
			continue;
		count++;
	}

	return count;
}

void remove_all_pnobj_ref(struct pnobj_refs *pnobj_refs)
{
	struct pnobj_ref *ref, *next;

	list_for_each_entry_safe(ref, next, get_pnobj_refs_list(pnobj_refs), list)
		del_pnobj_ref(ref);
}

void remove_pnobj_refs(struct pnobj_refs *pnobj_refs)
{
	if (!pnobj_refs)
		return;

	remove_all_pnobj_ref(pnobj_refs);
	kfree(pnobj_refs);
}

struct pnobj_refs *pnobj_refs_filter(bool (*filter_func)(struct pnobj *), struct pnobj_refs *orig_refs)
{
	struct pnobj_refs *filtered_refs;
	struct pnobj_ref *ref;
	int ret;

	filtered_refs = create_pnobj_refs();
	if (!filtered_refs)
		return NULL;

	list_for_each_entry(ref, get_pnobj_refs_list(orig_refs), list) {
		if (!filter_func(ref->pnobj))
			continue;

		ret = add_pnobj_ref(filtered_refs, ref->pnobj);
		if (ret < 0)
			goto err;
	}

	return filtered_refs;

err:
	remove_pnobj_refs(filtered_refs);
	return NULL;
}

struct pnobj_refs *pnobj_list_to_pnobj_refs(struct list_head *pnobj_list)
{
	struct pnobj_refs *refs;
	struct pnobj *pnobj;
	int ret;

	refs = create_pnobj_refs();
	if (!refs)
		return NULL;

	list_for_each_entry(pnobj, pnobj_list, list) {
		ret = add_pnobj_ref(refs, pnobj);
		if (ret < 0)
			goto err;
	}

	return refs;

err:
	remove_pnobj_refs(refs);
	return NULL;
}

MODULE_DESCRIPTION("obj driver for panel");
MODULE_LICENSE("GPL v2");
