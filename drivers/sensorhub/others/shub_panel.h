/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SHUB_PANEL_H_
#define __SHUB_PANEL_H_

#define UB_ID_FILE_PATH "/efs/FactoryApp/ubid2"
#define SDC			0
#define BOE			1
#define CSOT		2
#define DTC			3
#define Tianma		4
#define Skyworth	5
#define OTHER		6
#define SDC_STR			"SDC"
#define BOE_STR			"BOE"
#define CSOT_STR		"CSO"
#define DTC_STR			"DTC"
#define Tianma_STR		"TMC"
#define Skyworth_STR	"SKW"

void init_shub_panel(void);
void remove_shub_panel(void);
void sync_panel_state(void);

void init_shub_panel_callback(void);
void remove_shub_panel_callback(void);

bool is_panel_ubid_changed(void);
int save_panel_ubid(void);
int get_panel_lcd_type(void);
#endif
