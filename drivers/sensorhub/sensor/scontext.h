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

#include "scontext_cmd.h"

void shub_report_scontext_data(char *data, u32 data_len);
void shub_report_scontext_notice_data(char notice);
void shub_scontext_log(const char *func_name, const char *data, int length);
int shub_scontext_send_instruction(const unsigned char *buf, int count);
int shub_scontext_send_cmd(const unsigned char *buf, int count);
void get_ss_sensor_name(int type, char *buf, int buf_size);
void init_scontext_enable_state(void);
void disable_scontext_all(void);
