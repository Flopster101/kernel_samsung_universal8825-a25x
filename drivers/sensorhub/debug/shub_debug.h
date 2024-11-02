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

#include <linux/types.h>

enum {
	SHUB_LOG_EVENT_TIMESTAMP = 0,
	SHUB_LOG_DATA_PACKET,
	SHUB_LOG_MAX,
};

int init_shub_debug(void);
void exit_shub_debug(void);
void enable_debug_timer(void);
void disable_debug_timer(void);

int init_shub_debug_sysfs(void);
void remove_shub_debug_sysfs(void);

int print_mcu_debug(char *dataframe, int *index, int frame_len);
void print_log_debug(char *buf, int buf_len, u64 timestamp);
void print_dataframe(char *dataframe, int frame_len);
int print_system_info(char *dataframe, int *index, int frame_len);

bool check_debug_log_state(int log_type);
void set_open_cal_result(int type, int result);

void init_log_dump(void);
int save_log_dump(char *dataframe, int *index, int frame_len);

int shub_debug_level(void);

#ifdef CONFIG_SHUB_DEBUG
struct print_log_t {
	struct list_head list;
	char *buf;
	u64 timestamp;
};
#endif
