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

#ifndef __SHUB_PRESSURE_H_
#define __SHUB_PRESSURE_H_

#include <linux/types.h>

struct pressure_event {
	s32 pressure;
	s16 temperature;
	s32 pressure_cal;
	s32 pressure_sealevel;
} __attribute__((__packed__));


struct pressure_data {
	int sw_offset_default;
	int sw_offset;
	int convert_coef;
};

struct sensor_chipset_init_funcs *get_pressure_bmp580_function_pointer(char *name);
struct sensor_chipset_init_funcs *get_pressure_lps22hh_function_pointer(char *name);
struct sensor_chipset_init_funcs *get_pressure_lps22df_function_pointer(char *name);

int save_pressure_sw_offset_file(int offset);

#define PRESSURE_SUBCMD_ESN	126

#endif /* __SHUB_PRESSURE_H_ */
