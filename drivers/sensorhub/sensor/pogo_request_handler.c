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
#include "../others/shub_pogo.h"

#include <linux/slab.h>

int init_pogo_request_handler(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_POGO_REQUEST_HANDLER);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "pogo_request_handler");

		sensor->hal_sensor = false;
		sensor->receive_event_size = 0;
		sensor->report_event_size = 0;

		init_shub_pogo();
	} else {
		remove_shub_pogo();
	}

	return 0;
}
