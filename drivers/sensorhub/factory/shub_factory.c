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

#include "shub_factory.h"
#include "../utility/shub_utility.h"

typedef void (*init_sensor_factory)(bool en);
init_sensor_factory init_sensor_factory_funcs[] = {
	initialize_accelerometer_factory,
	initialize_magnetometer_factory,
	initialize_gyroscope_factory,
	initialize_light_factory,
	initialize_proximity_factory,
	initialize_pressure_factory,
	initialize_flip_cover_detector_factory,
};

int initialize_factory(void)
{
	uint64_t i;

	for (i = 0; i < ARRAY_SIZE(init_sensor_factory_funcs); i++)
		init_sensor_factory_funcs[i](true);

	return 0;
}

void remove_factory(void)
{
	uint64_t i;

	for (i = 0; i < ARRAY_SIZE(init_sensor_factory_funcs); i++)
		init_sensor_factory_funcs[i](false);

}
