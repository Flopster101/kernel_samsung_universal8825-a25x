/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * drivers/media/radio/s610/fm_ctrl.h
 *
 * Speedy driver header for SAMSUNG S610 chip
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#ifndef FM_CTRL_H
#define FM_CTRL_H

void fm_pwron(void);
void fm_pwroff(void);
void fmspeedy_wakeup(void);
void fm_speedy_m_int_enable(void);
void fm_speedy_m_int_disable(void);
void fm_en_speedy_m_int(void);
void fm_dis_speedy_m_int(void);
void fm_speedy_m_int_stat_clear(void);
u32 fmspeedy_get_reg(u32 addr);
u32 fmspeedy_get_reg_work(u32 addr);
int fmspeedy_set_reg(u32 addr, u32 data);
int fmspeedy_set_reg_work(u32 addr, u32 data);
u32 fmspeedy_get_reg_field(u32 addr, u32 shift, u32 mask);
u32 fmspeedy_get_reg_field_work(u32 addr, u32 shift, u32 mask);
int fmspeedy_set_reg_field(u32 addr, u32 shift, u32 mask, u32 data);
int fmspeedy_set_reg_field_work(u32 addr, u32 shift, u32 mask, u32 data);
void fm_audio_check(void);
void fm_audio_control(struct s610_radio *radio, bool audio_out, bool lr_switch,
		u32 req_time, u32 audio_addr);
void fm_set_audio_enable(bool enable);
extern struct s610_radio *gradio;
#endif	/* FM_CTRL_H */
