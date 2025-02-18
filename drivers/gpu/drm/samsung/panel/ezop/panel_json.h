/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_JSON_H__
#define __PANEL_JSON_H__

#include "panel.h"
#include "panel_obj.h"
#include "panel_function.h"
#include "jsmn.h"
#include "json_reader.h"
#include "json_writer.h"

/* json parser */
int jsonr_pnobj(json_reader_t *r, struct pnobj *pnobj);
int jsonr_maptbl(json_reader_t *r, struct maptbl *m);
int jsonr_packet_update_info(json_reader_t *r, struct pkt_update_info *pktui);
int jsonr_tx_packet(json_reader_t *r, struct pktinfo *pkt);
int jsonr_keyinfo(json_reader_t *r, struct keyinfo *key);
int jsonr_condition(json_reader_t *r, struct condinfo *cond);
int jsonr_rx_packet(json_reader_t *r, struct rdinfo *rdi);
int jsonr_resource(json_reader_t *r, struct resinfo *res);
int jsonr_dumpinfo(json_reader_t *r, struct dumpinfo *dump);
int jsonr_delayinfo(json_reader_t *r, struct delayinfo *delay);
int jsonr_timer_delay_begin_info(json_reader_t *r, struct timer_delay_begin_info *tdbi);
int jsonr_power_ctrl(json_reader_t *r, struct pwrctrl *pwrctrl);
int jsonr_property(json_reader_t *r, struct panel_property *prop);
int jsonr_config(json_reader_t *r, struct pnobj_config *prop);
int jsonr_sequence(json_reader_t *r, struct seqinfo *seq);
int jsonr_function(json_reader_t *r, struct pnobj_func *func);
int jsonr_all(json_reader_t *r);
int panel_json_parse(char *json, size_t size, struct list_head *pnobj_list);

/* json writer */
int jsonw_pnobj_list(json_writer_t *w, u32 pnobj_cmd_type, struct list_head *pnobj_list);
int jsonw_sorted_pnobj_list(json_writer_t *w, u32 pnobj_cmd_type, struct list_head *pnobj_list);
int jsonw_maptbl(json_writer_t *w, struct maptbl *m);
int jsonw_tx_packet(json_writer_t *w, struct pktinfo *pkt);
int jsonw_keyinfo(json_writer_t *w, struct keyinfo *key);
int jsonw_condition(json_writer_t *w, struct condinfo *c);
int jsonw_rx_packet(json_writer_t *w, struct rdinfo *rdi);
int jsonw_resource(json_writer_t *w, struct resinfo *res);
int jsonw_dumpinfo(json_writer_t *w, struct dumpinfo *di);
int jsonw_delayinfo(json_writer_t *w, struct delayinfo *di);
int jsonw_timer_delay_begin_info(json_writer_t *w, struct timer_delay_begin_info *tdbi);
int jsonw_power_ctrl(json_writer_t *w, struct pwrctrl *pi);
int jsonw_property(json_writer_t *w, struct panel_property *prop);
int jsonw_config(json_writer_t *w, struct pnobj_config *config);
int jsonw_sequence(json_writer_t *w, struct seqinfo *seq);
int jsonw_function(json_writer_t *w, struct pnobj_func *func);
int jsonw_maptbl_list(json_writer_t *w, struct list_head *head);
int jsonw_rx_packet_list(json_writer_t *w, struct list_head *head);
int jsonw_resource_list(json_writer_t *w, struct list_head *head);
int jsonw_dumpinfo_list(json_writer_t *w, struct list_head *head);
int jsonw_power_ctrl_list(json_writer_t *w, struct list_head *head);
int jsonw_property_list(json_writer_t *w, struct list_head *head);
int jsonw_config_list(json_writer_t *w, struct list_head *head);
int jsonw_sequence_list(json_writer_t *w, struct list_head *head);
int jsonw_key_list(json_writer_t *w, struct list_head *head);
int jsonw_tx_packet_list(json_writer_t *w, struct list_head *head);
int jsonw_delay_list(json_writer_t *w, struct list_head *head);
int jsonw_condition_list(json_writer_t *w, struct list_head *head);
int jsonw_function_list(json_writer_t *w, struct list_head *head);

#endif /* __PANEL_JSON_H__ */
