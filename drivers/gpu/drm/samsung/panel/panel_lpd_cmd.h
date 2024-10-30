/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_LPD_CMD__
#define __PANEL_LPD_CMD__

#include "panel_drv.h"

enum cmd_seq_id {
	LPD_BR_SEQ = 0,
	LPD_HBM_BR_SEQ,
	LPD_INIT_SEQ
};

enum cmd_type {
	STATIC_CMD = 0,
	DYNAMIC_CMD,
};

struct cmd_seq_header {
	u32 magic_code1;
	u32 magic_code2;
	u32 seq_id;
	u32 total_size;
	u32 br_count;
	u32 cmd_count;
/*
 *	u32 br_list[];
 *	u32 cmd_offset_list[];
 *	cmd list[]
*/
} __attribute__((aligned(4), packed));

struct static_cmd_header {
	u32 cmd_type;
	u32 total_size;
	u32 cmd_size;
	u8 offset;
} __attribute__((aligned(4), packed));

struct dynamic_cmd_header {
	u32 cmd_type;
	u32 total_size;
	u32 cmd_size;
	u32 cmd_count;
	u8 offset;
/*
	u32 br_list[];
*/
} __attribute__((aligned(4), packed));

/* macro for dynamic commands */
#define GET_DDATA_OFFSET(br_cnt) \
	(sizeof(struct dynamic_cmd_header) + (sizeof(u32) * br_cnt))

#define GET_DLIST_OFFSET() \
	(sizeof(struct dynamic_cmd_header))

#define GET_DLIST_IDX_OFFSET(idx) \
	(sizeof(struct dynamic_cmd_header) + (sizeof(u32) * idx))

#define GET_DLIST_IDX_VALUE(buf, idx) \
	(*((unsigned int *)(buf + GET_DLIST_IDX_OFFSET(idx))))

#define SET_DLIST_IDX_VALUE(buf, idx, val) \
	(*(unsigned int *)(buf + GET_DLIST_IDX_OFFSET(idx)) = val)

#define GET_SCMD_DATA_OFFSET()\
	(sizeof(struct static_cmd_header))

#define GET_SEQ_BODY_OFFSET(br_cnt, cmd_cnt) \
	(sizeof(struct cmd_seq_header) + (sizeof(u32) * br_cnt) + (sizeof(u32) * cmd_cnt))

#define GET_SEQ_CMD_LIST_IDX(br_cnt, idx) \
	(sizeof(struct cmd_seq_header) + (sizeof(u32) * br_cnt) + (sizeof(u32) * idx))

#define GET_SEQ_BR_LIST_IDX(idx) \
	(sizeof(struct cmd_seq_header) + (sizeof(u32) * idx))

#define SET_SEQ_CMD_OFFSET_LIST(buf, br_cnt, idx, val) \
	(*(unsigned int *)(buf + GET_SEQ_CMD_LIST_IDX(br_cnt, idx)) = val)

#define SET_SEQ_BR_LIST(buf, idx, val) \
	(*(unsigned int *)(buf + GET_SEQ_BR_LIST_IDX(idx)) = val)

#define GET_SEQ_CMD_OFFSET(buf, br_cnt, idx) \
	(*((unsigned int *)(buf + GET_SEQ_CMD_LIST_IDX(br_cnt, idx))))

int probe_lpd_panel_cmd(struct panel_device *panel);

#endif /* __PANEL_LPD_CMD__ */
