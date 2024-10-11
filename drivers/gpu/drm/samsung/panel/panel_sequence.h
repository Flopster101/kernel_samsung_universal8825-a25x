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

#ifndef __PANEL_SEQUENCE_H__
#define __PANEL_SEQUENCE_H__

struct panel_device;

struct seqinfo {
	struct pnobj base;
	void **cmdtbl;
	unsigned int size;
};

#define SEQINFO(_seqname) (_seqname)
#define SEQINFO_INIT(_seqname, _cmdtbl)	\
	{ .base = { .name = (_seqname), .cmd_type = (CMD_TYPE_SEQ) } \
	, .cmdtbl = (_cmdtbl) \
	, .size = ARRAY_SIZE((_cmdtbl)) }

#define DEFINE_SEQINFO(_seqname, _cmdtbl) \
struct seqinfo SEQINFO(_seqname) = SEQINFO_INIT((#_seqname), (_cmdtbl))


bool is_valid_sequence(struct seqinfo *seq);
char *get_sequence_name(struct seqinfo *seq);
int snprintf_sequence(char *buf, size_t size, struct seqinfo *seq);
struct seqinfo *create_sequence(char *name, size_t size);
void destroy_sequence(struct seqinfo *seq);

struct command_node {
	int vertex;
	struct list_head list;
};

int get_index_of_sequence(struct list_head *seq_list,
		struct seqinfo *sequence);
int get_count_of_sequence(struct list_head *seq_list);
int sequence_sort(struct list_head *seq_list);
struct pnobj_refs *sequence_to_pnobj_refs(struct seqinfo *seq);
struct pnobj_refs *sequence_condition_filter(struct panel_device *panel, struct seqinfo *seq);
int get_packet_refs_from_sequence(struct panel_device *panel,
		char *seqname, struct pnobj_refs *pnobj_refs);

#endif /* __PANEL_SEQUENCE_H__ */
