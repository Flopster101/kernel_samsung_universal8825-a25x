/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if defined(CONFIG_UML)
#include "lpd_config_for_uml.h"
#else
#include <soc/samsung/exynos/exynos-lpd.h>
#endif

#include "util.h"
#include "panel.h"
#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_packet.h"
#include "panel_sequence.h"
#include "panel_lpd_cmd.h"


#ifdef DEBUG_LPD_CMD
static void print_dynamic_cmd_header(void *buf)
{
	struct dynamic_cmd_header *header = buf;

	panel_info("===================== dump dynamic cmd =====================");
	panel_info("cmd cnt: %d, cmd size: %d, offset: %d\n",
			header->cmd_count, header->cmd_size, header->offset);
}

static void print_dynamic_cmd_elem(void *buf, unsigned int idx)
{
	char d_buf[1024] = { 0, };
	struct dynamic_cmd_header *header = buf;

	usdm_snprintf_bytes(d_buf, ARRAY_SIZE(d_buf),
			buf + GET_DLIST_IDX_VALUE(buf, idx), header->cmd_size);

	panel_info("cmd [%i]: %s\n", idx, d_buf);
}

static void print_dynamic_cmd(void *buf)
{
	int i;
	struct dynamic_cmd_header *header = buf;

	print_dynamic_cmd_header(buf);
	for (i = 0; i < header->cmd_count; i++)
		print_dynamic_cmd_elem(buf, i);
}

static void print_static_cmd_header(void *buf)
{
	struct static_cmd_header *header = buf;

	panel_info("===================== dump static cmd =====================\n");
	panel_info("cmd size: %d, offset: %d\n", header->cmd_size, header->offset);
}

static void print_static_cmd(void *buf)
{
	u8 d_buf[1024] = { 0, };
	struct static_cmd_header *header = buf;

	print_static_cmd_header(buf);
	usdm_snprintf_bytes(d_buf, ARRAY_SIZE(d_buf),
			buf + GET_SCMD_DATA_OFFSET(), header->cmd_size);

	panel_info("cmd : %s\n", d_buf);
}

void do_br_seq(void *buf, int idx)
{
	unsigned int offset;
	unsigned int cmd_type;
	struct cmd_seq_header *header = buf;
	int i;

	for (i = 0; i < header->cmd_count; i++) {
		offset = GET_SEQ_CMD_OFFSET(buf, header->br_count, i);
		panel_info("offset: %x: %d\n", offset, offset);

		cmd_type = *(unsigned int *)(buf + offset);

		switch (cmd_type) {
		case STATIC_CMD:
			print_static_cmd((void *)buf + offset);
			break;
		case DYNAMIC_CMD:
			print_dynamic_cmd_elem((void *)buf + offset, idx);
			break;
		default:
			panel_err("invalid cmd : %d\n", cmd_type);
			return;
		}
	}
}
#endif

static void set_static_cmd_header(struct dynamic_cmd_header *header,
		u32 gpara_offset, u32 cmd_size, u32 total_size)
{
	header->cmd_type = STATIC_CMD;
	header->total_size = total_size;
	header->cmd_size = cmd_size;
	header->offset = gpara_offset;
}

static void set_dynamic_cmd_header(struct dynamic_cmd_header *header,
		u32 gpara_offset, u32 cmd_size, u32 cmd_count, u32 total_size)
{
	header->cmd_type = DYNAMIC_CMD;
	header->total_size = total_size;
	header->cmd_size = cmd_size;
	header->cmd_count = cmd_count;
	header->offset = gpara_offset;
}

static void set_cmd_seq_header(struct cmd_seq_header *header,
		u32 seq_id, u32 br_count, u32 cmd_count, u32 total_size)
{
	header->seq_id = seq_id;
	header->total_size = total_size;
	header->br_count = br_count;
	header->cmd_count = cmd_count;
	header->magic_code1 = CMD_SEQ_MAGIC_CODE1;
	header->magic_code2 = CMD_SEQ_MAGIC_CODE2;
}

static unsigned int write_dynamic_cmd(struct pktinfo *pkt, void *buf,
		struct lpd_br_info *br_info, struct panel_bl_device *panel_bl)
{
	int i, prev_br;
	unsigned int offset = GET_DDATA_OFFSET(br_info->br_cnt);

	if (pkt->type != DSI_PKT_TYPE_WR) {
		panel_err("invalid type: %d\n", pkt->type);
		return 0;
	}

	panel_mutex_lock(&panel_bl->lock);
	prev_br = panel_bl->props.brightness;

	for (i = 0; i < br_info->br_cnt; i++) {
		panel_bl_set_property(panel_bl, &panel_bl->props.brightness, br_info->br_list[i]);
		update_tx_packet(pkt);
		copy_pktinfo_data(buf + offset, pkt);
		SET_DLIST_IDX_VALUE(buf, i, offset);
		offset += pkt->dlen;
#ifdef DEBUG_LPD_CMD
		print_pktinfo(pkt, i);
#endif
	}

	panel_bl_set_property(panel_bl, &panel_bl->props.brightness, prev_br);
	panel_mutex_unlock(&panel_bl->lock);

	set_dynamic_cmd_header(buf, pkt->offset, pkt->dlen, br_info->br_cnt, offset);

#ifdef DEBUG_LPD_CMD
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4, buf, offset, false);
	print_dynamic_cmd(buf);
#endif

	return offset;
}

static unsigned int write_static_cmd(struct pktinfo *pkt, void *buf)
{
	unsigned int offset = GET_SCMD_DATA_OFFSET();

	if (pkt->type != DSI_PKT_TYPE_WR) {
		panel_err("invalid type: %d\n", pkt->type);
		return 0;
	}

	copy_pktinfo_data(buf + offset, pkt);
	offset += pkt->dlen;
#ifdef DEBUG_LPD_CMD
	print_pktinfo(pkt, 0);
#endif
	set_static_cmd_header(buf, pkt->offset, pkt->dlen, offset);
#ifdef DEBUG_LPD_CMD
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4, buf, offset, false);
	print_static_cmd(buf);
#endif

	return offset;
}

__visible_for_testing int load_lpd_br_cmd(struct panel_device *panel, struct lpd_panel_cmd *panel_cmd)
{
	int i, idx = 0;
	struct panel_bl_device *panel_bl;
	unsigned int cmd_cnt;
	unsigned int offset;
	struct pnobj_refs *cmd_refs;
	struct pnobj_ref *ref;
	struct pktinfo *pkt;
	struct lpd_cmd_buf *cmd_buf;
	struct lpd_br_info *br_info;

	if (!panel)
		return -EINVAL;

	if (!panel_cmd)
		return -EINVAL;

	cmd_buf = &panel_cmd->cmd_buf;
	br_info = &panel_cmd->br_info;
	cmd_refs = &panel->lpd_br_list;

	if ((cmd_buf->buf == NULL) || (cmd_buf->buf_size == 0)) {
		panel_err("invalid cmd buffer");
		return -EINVAL;
	}
	cmd_cnt = get_count_of_pnobj_ref(cmd_refs);

	panel_bl = &panel->panel_bl;

	offset = GET_SEQ_BODY_OFFSET(br_info->br_cnt, cmd_cnt);

	list_for_each_entry(ref, get_pnobj_refs_list(cmd_refs), list) {
		if (!ref->pnobj)
			continue;

		pkt = pnobj_container_of(ref->pnobj, struct pktinfo);

		SET_SEQ_CMD_OFFSET_LIST(cmd_buf->buf, br_info->br_cnt, idx++, offset);
		if (is_variable_packet(pkt))
			offset += write_dynamic_cmd(pkt, cmd_buf->buf + offset, br_info, panel_bl);
		else
			offset += write_static_cmd(pkt, cmd_buf->buf + offset);

		panel_dbg("command offset: %d\n", offset);

		if (offset >= cmd_buf->buf_size) {
			panel_err("exceed allocate buffer: %d, (allocated: %d)\n",
					offset, cmd_buf->buf_size);
			return -ENOMEM;
		}
	}

	set_cmd_seq_header(cmd_buf->buf, LPD_BR_SEQ,
			br_info->br_cnt, cmd_cnt, offset);
	for (i = 0; i < br_info->br_cnt; i++)
		SET_SEQ_BR_LIST(cmd_buf->buf, i, br_info->nit_list[i]);

	panel_info("total command size: %d\n", offset);
#ifdef DEBUG_LPD_CMD
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4, cmd_buf->buf, offset, false);
#endif
	return 0;
}

static int notifier_lpd_config(struct notifier_block *nb, unsigned long action, void *nb_data)
{
	int ret = 0;
	struct panel_device *panel =
		container_of(nb, struct panel_device, lpd_nb);

	switch (action) {
	case LPD_CONFIG_BR_CMD:
		panel_info("LPD_CONFIG_BR_CMD\n");
		ret = load_lpd_br_cmd(panel, (struct lpd_panel_cmd *)nb_data);
		break;
	case LPD_CONFIG_HBM_BR_CMD:
		panel_info("LPD_CONFIG_HBM_BR_CMD\n");
		break;
	default:
		panel_err("invalid action: %ld\n", action);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int probe_lpd_panel_cmd(struct panel_device *panel)
{
	int ret;

	if (panel == NULL) {
		panel_err("invalid panel\n");
		return -EINVAL;
	}

	INIT_PNOBJ_REFS(&panel->lpd_br_list);
	INIT_PNOBJ_REFS(&panel->lpd_hbm_br_list);
	INIT_PNOBJ_REFS(&panel->lpd_init_list);

	ret = get_packet_refs_from_sequence(panel, PANEL_LPD_BR_SEQ, &panel->lpd_br_list);
	if (ret) {
		panel_err("failed to get packet list of sequence(%s)\n", PANEL_LPD_BR_SEQ);
		return ret;
	}

/*
 * below commands does not support now
 */
#if 0
	ret = get_packet_refs_from_sequence(panel, PANEL_LPD_HBM_BR_SEQ, &panel->lpd_hbm_br_list);
	if (ret) {
		panel_err("failed to get packet list of sequence(%s)\n", PANEL_LPD_HBM_BR_SEQ);
		return ret;
	}

	ret = get_packet_refs_from_sequence(panel, PANEL_LPD_INIT_SEQ, &panel->lpd_init_list);
	if (ret) {
		panel_err("failed to get packet list of sequence(%s)\n", PANEL_LPD_INIT_SEQ);
		return ret;
	}
#endif
	panel->lpd_nb.notifier_call = notifier_lpd_config;
	lpd_config_notifier_register(&panel->lpd_nb);

	return 0;
}

#if 0
int exit_lpd_panel_cmd(struct panel_device *panel)
{
	remove_pnobj_ref(&panel->lpd_br_list);
	remove_pnobj_ref(&panel->lpd_hbm_br_list);
	remove_pnobj_ref(&panel->lpd_init_list);
}
#endif
