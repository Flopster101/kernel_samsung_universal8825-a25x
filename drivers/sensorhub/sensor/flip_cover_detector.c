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

#include <linux/of_gpio.h>
#include <linux/slab.h>

#include "../comm/shub_comm.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../utility/shub_utility.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../factory/flip_cover_detector/flip_cover_detector_factory.h"
#include "flip_cover_detector.h"


bool check_flip_cover_detector_supported(void)
{
	struct device_node *np = get_shub_device()->of_node;
	uint32_t axis;

	if (of_property_read_u32(np, "fcd-axis", &axis))
		return false;

	return true;
}

void parse_dt_flip_cover_detector_variable(struct device *dev)
{
	struct flip_cover_detector_data *data = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)->data;
	struct device_node *np = dev->of_node;

	if (of_property_read_u32(np, "fcd-axis", &data->axis_update)) {
		shub_infof("no fcd-axis");
		data->axis_update = AXIS_SELECT;
	}

	if (of_property_read_s32(np, "fcd-threshold", &data->threshold_update)) {
		shub_infof("no fcd-threshold");
		data->threshold_update = THRESHOLD;
	}

	shub_infof("axis[%d], threshold[%d]", data->axis_update, data->threshold_update);
}

int set_flip_cover_detector_axis_threshold(void)
{
	int ret = 0;
	struct flip_cover_detector_data *data = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)->data;
	int8_t shub_data[5] = {0};

	memcpy(shub_data, &data->axis_update, sizeof(data->axis_update));
	memcpy(shub_data + 1, &data->threshold_update, sizeof(data->threshold_update));

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_FLIP_COVER_DETECTOR, CAL_DATA, shub_data, sizeof(shub_data));
	shub_info("flip_cover_detector axis : %d, threshold : %d", data->axis_update, data->threshold_update);

	return ret;
}

int set_nfc_cover_status(void)
{
	int ret = 0;
	struct flip_cover_detector_data *data = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)->data;
	int status = data->nfc_cover_status;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_FLIP_COVER_DETECTOR, LIBRARY_DATA,
				(char *)&status, sizeof(status));
	shub_info("nfc_cover_status %d", status);

	return ret;
}

int sync_flip_cover_detector_status(void)
{
	int ret = 0;

	shub_infof();
	ret = set_flip_cover_detector_axis_threshold();
	if (ret < 0)
		shub_errf("send flip_cover_detector axis/threshold failed");

	ret = set_nfc_cover_status();
	if (ret < 0)
		shub_errf("send nfc_cover_status failed");

	return ret;
}

void print_flip_cover_detector_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct flip_cover_detector_event *sensor_value = (struct flip_cover_detector_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d, %d, %d, %d, %d, %d, %d / %d, %d, %d, %d (%lld) (%ums, %dms)", sensor->name,
		  SENSOR_TYPE_FLIP_COVER_DETECTOR, sensor_value->value, sensor_value->nfc, (int)sensor_value->diff, (int)sensor_value->magX,
		  (int)sensor_value->stable_min, (int)sensor_value->stable_max, (int)sensor_value->detach_mismatch_cnt, 
		  (int)sensor_value->detach_mismatch_stop_cnt, (int)sensor_value->attach_retry_cnt, (int)sensor_value->uncal_mag_x, 
		  (int)sensor_value->uncal_mag_y, (int)sensor_value->uncal_mag_z, sensor_value->saturation, event->timestamp, sensor->sampling_period,
		  sensor->max_report_latency);
}

void report_event_flip_cover_detector(void)
{
	struct flip_cover_detector_data *data = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)->data;

	if (data->factory_cover_status)
		check_cover_detection_factory();
}

static struct sensor_funcs flip_cover_detector_sensor_func = {
	.sync_status = sync_flip_cover_detector_status,
	.report_event = report_event_flip_cover_detector,
	.print_debug = print_flip_cover_detector_debug,
	.parse_dt = parse_dt_flip_cover_detector_variable,
};
static struct flip_cover_detector_data flip_cover_detector_data;

int init_flip_cover_detector(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "flip_cover_detector", 37, 24, sizeof(struct flip_cover_detector_event));

		sensor->data = (void *)&flip_cover_detector_data;
		sensor->funcs = &flip_cover_detector_sensor_func;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
