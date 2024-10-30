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

#include "light_autobrightness.h"
#include "../comm/shub_comm.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../comm/shub_iio.h"
#include "../utility/shub_utility.h"

#define CAMERA_LUX_ENABLE		-1
#define CAMERA_LUX_DISABLE		-2

static int init_light_autobrightness_variable(void)
{
	struct light_autobrightness_data *data = get_sensor(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS)->data;

	data->camera_lux_en = false;

	return 0;
}

static void parse_dt_light_autobrightness(struct device *dev)
{
	struct light_autobrightness_data *data = get_sensor(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS)->data;
	struct device_node *np = dev->of_node;

	if (of_property_read_u32_array(np, "light-cam-lux", data->camera_lux_hysteresis,
				       ARRAY_SIZE(data->camera_lux_hysteresis))) {
		shub_errf("no light-cam-high");
		data->camera_lux_hysteresis[0] = -1;
		data->camera_lux_hysteresis[1] = 0;
	}

	if (of_property_read_u32_array(np, "light-cam-br", data->camera_br_hysteresis,
				       ARRAY_SIZE(data->camera_br_hysteresis))) {
		shub_errf("no light-cam-low");
		data->camera_br_hysteresis[0] = 10000;
		data->camera_br_hysteresis[1] = 0;
	}

	shub_infof("light-cam-high : %d %d, light-cam-br : %d %d",
		   data->camera_lux_hysteresis[0], data->camera_lux_hysteresis[1],
		   data->camera_br_hysteresis[0], data->camera_br_hysteresis[1]);
}

static int set_light_ab_camera_hysteresis(void)
{
	int ret = 0;
	int buf[4];
	struct light_autobrightness_data *data = get_sensor(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS)->data;

	if (!get_sensor_probe_state(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS)) {
		shub_infof("light sensor is not connected");
		return ret;
	}

	memcpy(buf, data->camera_lux_hysteresis, sizeof(data->camera_lux_hysteresis));
	memcpy(&buf[2], data->camera_br_hysteresis, sizeof(data->camera_br_hysteresis));

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS, LIGHT_AB_HYSTERESIS, (char *)buf,
				sizeof(buf));

	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return ret;
	}

	shub_infof("%d %d %d %d\n", buf[0], buf[1], buf[2], buf[3]);

	return ret;
}

static void report_camera_lux_data(int lux)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS);
	struct light_ab_event *sensor_value = (struct light_ab_event *)(sensor->event_buffer.value);

	shub_infof("%d", lux);

	sensor_value->lux = lux;
	sensor_value->min_flag = 0;
	shub_report_sensordata(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS, get_current_timestamp(), (char *)&sensor_value->lux,
			       sensor->report_event_size);
}

static int sync_light_autobrightness_status(void)
{
	set_light_ab_camera_hysteresis();
	return 0;
}

static int enable_light_autobrightness(void)
{
	struct light_autobrightness_data *data = get_sensor(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS)->data;

	data->light_ab_log_cnt = 0;
	return 0;
}

static int disable_light_autobrightness(void)
{
	struct light_autobrightness_data *data = get_sensor(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS)->data;

	if (data->camera_lux_en) {
		data->camera_lux_en = false;
		report_camera_lux_data(CAMERA_LUX_DISABLE);
	}
	return 0;
}

static void report_event_light_autobrightness(void)
{
	struct light_autobrightness_data *data = get_sensor(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS)->data;
	struct light_ab_event *sensor_value =
	    (struct light_ab_event *)(get_sensor_event(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS)->value);

	if (!data->camera_lux_en &&
	    (sensor_value->lux <= data->camera_lux_hysteresis[0]) &&
	    (sensor_value->brightness > data->camera_br_hysteresis[0])) {

		if (data->light_ab_log_cnt == 0) {
			shub_infof("Light AB Sensor : report first lux form light sensor");
			report_camera_lux_data(sensor_value->lux);
		}
		shub_infof("Light AB Sensor : report cam enable");
		data->camera_lux_en = true;
		sensor_value->lux = CAMERA_LUX_ENABLE;

	} else if (data->camera_lux_en &&
		   ((sensor_value->lux >= data->camera_lux_hysteresis[1]) ||
		   (sensor_value->brightness <= data->camera_br_hysteresis[1]))) {

		shub_infof("Light AB Sensor : report cam disable");
		data->camera_lux_en = false;
		sensor_value->lux = CAMERA_LUX_DISABLE;

	} else if (data->camera_lux_en) {
		//shub_infof("Light AB Sensor : report skip");
		return;
	}

	if (data->light_ab_log_cnt < 3) {
		shub_info("Light AB Sensor : lux=%u brightness=%u camera_lux_en=%d / %d %d %d %d",
			  sensor_value->lux, sensor_value->brightness, data->camera_lux_en,
			  data->camera_lux_hysteresis[0], data->camera_lux_hysteresis[1],
			  data->camera_br_hysteresis[0], data->camera_br_hysteresis[1]);
		data->light_ab_log_cnt++;
	}
}

static void print_light_autobrightness_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct light_ab_event *sensor_value = (struct light_ab_event *)(event->value);

	shub_info("%s(%u) : %u, %u, %u (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS,
		  sensor_value->lux, sensor_value->min_flag, sensor_value->brightness, event->timestamp,
		  sensor->sampling_period, sensor->max_report_latency);
}

static int inject_light_ab_additional_data(char *buf, int count)
{
	struct light_autobrightness_data *data = get_sensor(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS)->data;

	if (count < 4) {
		shub_errf("camera lux length error %d", count);
		return -EINVAL;
	}
	data->camera_lux = *((int32_t *)(buf));
	shub_infof("cam_lux %d", data->camera_lux);

	if (data->camera_lux_en)
		report_camera_lux_data(data->camera_lux);
	return 0;
}


static struct light_autobrightness_data light_autobrightness_data;
static struct sensor_funcs light_autobrightness_sensor_funcs = {
	.sync_status = sync_light_autobrightness_status,
	.enable = enable_light_autobrightness,
	.disable = disable_light_autobrightness,
	.report_event = report_event_light_autobrightness,
	.print_debug = print_light_autobrightness_debug,
	.inject_additional_data = inject_light_ab_additional_data,
	.init_variable = init_light_autobrightness_variable,
	.parse_dt = parse_dt_light_autobrightness,

};

int init_light_autobrightness(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "auto_brightness", 9, 5, sizeof(struct light_ab_event));
		sensor->data = (void *)&light_autobrightness_data;
		sensor->funcs = &light_autobrightness_sensor_funcs;

	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
