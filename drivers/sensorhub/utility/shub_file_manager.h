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

#ifndef __SHUB_FILE_MANAGER_H_
#define __SHUB_FILE_MANAGER_H_
#include <linux/notifier.h>

int init_file_manager(void);
void remove_file_manager(void);
void register_file_manager_ready_callback(struct notifier_block *nb);

int shub_file_read(char *path, char *buf, int buf_len, long long pos);
int shub_file_write(char *path, char *buf, int buf_len, long long pos);
int shub_file_write_no_wait(char *path, char *buf, int buf_len, long long pos);
int shub_file_remove(char *path);

#endif
