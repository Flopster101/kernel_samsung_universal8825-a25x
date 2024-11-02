/*
 * Samsung TUI HW Handler driver.
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_SAMSUNG_TUI_INF_H
#define __LINUX_SAMSUNG_TUI_INF_H

#define STUI_TSP_TYPE_SLSI	0
#define STUI_TSP_TYPE_ILITEK	1
#define STUI_TSP_TYPE_NOVATEK	2
#define STUI_TSP_TYPE_MELFAS	3
#define STUI_TSP_TYPE_STM	4
#define STUI_TSP_TYPE_ZINITIX	5
#define STUI_TSP_TYPE_SNAPTICS	6
#define STUI_TSP_TYPE_GOODIX	7
#define STUI_TSP_TYPE_FOCALTECH	8
#define STUI_TSP_TYPE_HIMAX	9

#define STUI_MODE_OFF           0x00
#define STUI_MODE_TUI_SESSION   0x01
#define STUI_MODE_DISPLAY_SEC   0x02
#define STUI_MODE_TOUCH_SEC     0x04
#define STUI_MODE_ALL           (STUI_MODE_TUI_SESSION | STUI_MODE_DISPLAY_SEC | STUI_MODE_TOUCH_SEC)


int  stui_get_mode(void);
void stui_set_mode(int mode);

int  stui_set_mask(int mask);
int  stui_clear_mask(int mask);

int stui_cancel_session(void);

void stui_set_touch_type(uint32_t type);
int stui_get_touch_type(void);

enum tui_version {
	TUI_NOPE,
	TUI_OLD,
	TUI_LL
};

void stui_set_tui_version(enum tui_version version);
enum tui_version stui_get_tui_version(void);

#endif /* __LINUX_SAMSUNG_TUI_INF_H */
