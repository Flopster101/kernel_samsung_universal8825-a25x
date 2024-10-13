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

#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"

#include <linux/slab.h>

int init_super(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_SENSORHUB);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "sensorhub_sensor", 0, 3, 3);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
