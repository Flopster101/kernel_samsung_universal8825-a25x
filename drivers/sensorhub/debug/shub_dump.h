/*
 *  Copyright (C) 2018, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#ifndef __SHUB_DUMP_H_
#define __SHUB_DUMP_H_

#define DUMP_TYPE_BASE 100

void write_shub_dump_file(char *dump, int dumpsize, int type, int count);
void initialize_shub_dump(void);
void remove_shub_dump(void);

#endif
