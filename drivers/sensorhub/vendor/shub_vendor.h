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

#ifndef _SHUB_VENDOR_H__
#define _SHUB_VENDOR_H__

#ifdef CONFIG_SHUB_LSI
#include "shub_lsi.h"
#else
#include "shub_mtk.h"
#endif

int sensorhub_comms_write(u8 *buf, int length);
int sensorhub_reset(void);
int sensorhub_probe(void);
int sensorhub_shutdown(void);
int sensorhub_refresh_func(void);
void sensorhub_fs_ready(void);
bool sensorhub_is_working(void);

void sensorhub_save_ram_dump(void);
int sensorhub_get_dump_size(void);

#endif
