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

#ifdef CONFIG_SHUB_DEBUG

void sensorhub_system_check(uint32_t test_delay_us, uint32_t test_count);
void shub_system_checker_init(void);
bool is_system_checking(void);
bool is_event_order_checking(void);
void system_ready_cb(void);
void comm_test_cb(int32_t type);
void event_test_cb(int32_t type, uint64_t timestamp);
void order_test_cb(int32_t type, uint64_t timestamp);
void shub_system_check_lock(void);
void shub_system_check_unlock(void);

#endif
