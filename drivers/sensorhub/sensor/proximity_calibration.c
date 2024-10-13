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
#include "../sensor/proximity.h"

#include <linux/slab.h>

void proximity_calibration_off(void)
{
	shub_infof("");
	disable_sensor(SENSOR_TYPE_PROXIMITY_CALIBRATION, NULL, 0);
}

void report_event_proximity_calibration(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY_CALIBRATION)->data;
	struct proximity_chipset_funcs *chipset_funcs = get_sensor(SENSOR_TYPE_PROXIMITY)->chipset_funcs;
	struct prox_cal_event *sensor_value =
	    (struct prox_cal_event *)(get_sensor_event(SENSOR_TYPE_PROXIMITY_CALIBRATION)->value);

	data->prox_threshold[0] = sensor_value->prox_cal[0];
	data->prox_threshold[1] = sensor_value->prox_cal[1];
	shub_infof("prox thresh %u %u", data->prox_threshold[0], data->prox_threshold[1]);

	proximity_calibration_off();

	if (chipset_funcs->pre_report_event_proximity)
		chipset_funcs->pre_report_event_proximity();
}


static struct sensor_funcs proximity_calibration_sensor_funcs = {
	.report_event = report_event_proximity_calibration,
};

int init_proximity_calibration(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY_CALIBRATION);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "proximity_calibration", 4, 0, sizeof(struct prox_cal_event));
		sensor->hal_sensor = false;
		sensor->data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
		sensor->funcs = &proximity_calibration_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
