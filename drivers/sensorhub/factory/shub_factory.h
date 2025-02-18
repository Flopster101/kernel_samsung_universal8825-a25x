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

#ifndef __SHUB_FACTORY_H_
#define __SHUB_FACTORY_H_
#include <linux/types.h>

int initialize_factory(void);
void remove_factory(void);

void initialize_accelerometer_factory(bool en);
void initialize_gyroscope_factory(bool en);
void initialize_light_factory(bool en);
void initialize_magnetometer_factory(bool en);
void initialize_pressure_factory(bool en);
void initialize_proximity_factory(bool en);
void initialize_flip_cover_detector_factory(bool en);

#endif

