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

#include "../comm/shub_comm.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_dev_core.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_file_manager.h"
#include "proximity.h"

#include <linux/of_gpio.h>
#include <linux/slab.h>

get_init_chipset_funcs_ptr get_prox_funcs_ary[] = {
	get_proximity_stk3x6x_function_pointer,
	get_proximity_gp2ap110s_function_pointer,
	get_proximity_stk3328_function_pointer,
	get_proximity_stk3391x_function_pointer,
	get_proximity_stk33512_function_pointer,
	get_proximity_stk3afx_function_pointer,
	get_proximity_tmd3725_function_pointer,
};

static get_init_chipset_funcs_ptr *get_proximity_init_chipset_funcs(int *len)
{
	*len = ARRAY_SIZE(get_prox_funcs_ary);
	return get_prox_funcs_ary;
}

static int init_proximity_variable(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->cal_data_len) {
		data->cal_data = kzalloc(data->cal_data_len, GFP_KERNEL);
		if (!data->cal_data)
			return -ENOMEM;
	}
	return 0;
}

#define TEMPERATURE_THRESH 50
#define COMPENSATION_VALUE 40
#define BATTERY_TEMP_PATH  "/sys/class/power_supply/battery/temp"

int get_proximity_thresh_temperature_compensation(void)
{
	int ret = 0;
	char temp_str[10] = {0, };
	int temp_value = 0;

	ret = shub_file_read(BATTERY_TEMP_PATH, (char *)&temp_str, sizeof(temp_str), 0);
	if (ret <= 0) {
		shub_errf("can't proximity read temperature file %d", ret);
		return 0;
	}

	ret = kstrtos32(temp_str, 10, &temp_value);
	if (ret < 0) {
		shub_errf("kstrtou32 failed(%d)", ret);
		ret = 0;
	} else if (temp_value < TEMPERATURE_THRESH)
		ret = COMPENSATION_VALUE;

	shub_infof("%s temp value %d compensation %d", temp_str, temp_value, ret);
	return ret;
}

void set_proximity_threshold(void)
{
	int ret = 0;
	u8 prox_th_mode = -1;
	u16 prox_th[PROX_THRESH_SIZE] = {0, };
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_chipset_funcs *chipset_funcs = get_sensor(SENSOR_TYPE_PROXIMITY)->chipset_funcs;

	if (!get_sensor_probe_state(SENSOR_TYPE_PROXIMITY)) {
		shub_infof("proximity sensor is not connected");
		return;
	}

	if (chipset_funcs && chipset_funcs->set_proximity_threshold) {
		chipset_funcs->set_proximity_threshold();
	} else {
		memcpy(prox_th, data->prox_threshold, sizeof(prox_th));
		ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_PROXIMITY, PROXIMITY_THRESHOLD, (char *)prox_th,
					sizeof(prox_th));
		if (ret < 0) {
			shub_err("SENSOR_PROXTHRESHOLD CMD fail %d", ret);
			return;
		}
	}

	if (data->need_compensation) {
		int compensation = get_proximity_thresh_temperature_compensation();

		prox_th[0] += compensation;
		prox_th[1] += compensation;
	}

	if (chipset_funcs && chipset_funcs->get_proximity_threshold_mode)
		prox_th_mode = chipset_funcs->get_proximity_threshold_mode();

	shub_info("Proximity Threshold[%d] - %u, %u", prox_th_mode, data->prox_threshold[PROX_THRESH_HIGH],
		  data->prox_threshold[PROX_THRESH_LOW]);
}

static int sync_proximity_status(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_chipset_funcs *chipset_funcs = get_sensor(SENSOR_TYPE_PROXIMITY)->chipset_funcs;

	shub_infof();

	set_proximity_threshold();
	if (chipset_funcs && chipset_funcs->sync_proximity_state)
		chipset_funcs->sync_proximity_state(data);

	return ret;
}

static void print_debug_proximity(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	struct sensor_event *event = &(sensor->event_buffer);
	struct prox_event *sensor_value = (struct prox_event *)(event->value);

	shub_infof("%s(%u) : %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_PROXIMITY, sensor_value->prox,
		   sensor_value->prox_raw, event->timestamp, sensor->sampling_period, sensor->max_report_latency);
}

static int enable_proximity(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_chipset_funcs *chipset_funcs = get_sensor(SENSOR_TYPE_PROXIMITY)->chipset_funcs;

	set_proximity_threshold();
	if (chipset_funcs && chipset_funcs->pre_enable_proximity)
		chipset_funcs->pre_enable_proximity(data);

	return 0;
}

void print_proximity_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct prox_event *sensor_value = (struct prox_event *)(event->value);

	shub_info("%s(%u) : %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_PROXIMITY, sensor_value->prox,
		  sensor_value->prox_raw, event->timestamp, sensor->sampling_period, sensor->max_report_latency);
}

void report_event_proximity(void)
{
	struct prox_event *sensor_value = (struct prox_event *)(get_sensor_event(SENSOR_TYPE_PROXIMITY)->value);

	shub_infof("Proximity Sensor Detect : %u, raw : %u", sensor_value->prox, sensor_value->prox_raw);
}

int parsing_proximity_threshold(char *dataframe, int *index, int frame_len)
{
	u16 thresh[2] = {0, };
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_chipset_funcs *chipset_funcs = get_sensor(SENSOR_TYPE_PROXIMITY)->chipset_funcs;

	if (*index + sizeof(thresh) > frame_len) {
		shub_errf("parsing error");
		return -EINVAL;
	}

	memcpy(thresh, dataframe + (*index), sizeof(thresh));
	data->prox_threshold[0] = thresh[0];
	data->prox_threshold[1] = thresh[1];

	if (chipset_funcs && chipset_funcs->set_proximity_threshold_mode)
		chipset_funcs->set_proximity_threshold_mode(3);

	(*index) += sizeof(thresh);
	shub_infof("prox threshold received %u %u", data->prox_threshold[0], data->prox_threshold[1]);

	return 0;
}

int set_proximity_calibration(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->cal_data_len == 0)
		return ret;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_PROXIMITY, CAL_DATA, (char *)data->cal_data,
				 data->cal_data_len);
	if (ret < 0)
		shub_errf("failed %d", ret);

	return ret;
}

int save_proximity_calibration(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->cal_data_len == 0)
		return ret;

	ret = shub_file_write_no_wait(PROX_CALIBRATION_FILE_PATH, (char *)data->cal_data, data->cal_data_len, 0);
	if (ret != data->cal_data_len) {
		shub_errf("failed");
		return -EIO;
	}

	return ret;
}

int open_default_proximity_calibration(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->cal_data_len == 0)
		return ret;

	ret = shub_file_read(PROX_CALIBRATION_FILE_PATH, (char *)data->cal_data, data->cal_data_len, 0);
	if (ret != data->cal_data_len) {
		shub_errf("failed");
		return -EIO;
	}

	return ret;
}

static int open_proximity_calibration(void)
{
	int ret = 0;
	struct proximity_chipset_funcs *chipset_funcs = get_sensor(SENSOR_TYPE_PROXIMITY)->chipset_funcs;

	if (chipset_funcs && chipset_funcs->open_calibration_file)
		ret = chipset_funcs->open_calibration_file();
	else
		ret = open_default_proximity_calibration();

	return ret;
}

int set_proximity_setting_mode(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->setting_mode == 0)
		return ret;

	shub_infof("%d", data->setting_mode);

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_PROXIMITY, PROXIMITY_SETTING_MODE,
				 (char *)&data->setting_mode, sizeof(data->setting_mode));
	if (ret < 0)
		shub_errf("failed %d", ret);

	return ret;
}

int save_proximity_setting_mode(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->setting_mode == 0)
		return ret;

	shub_infof("%d", data->setting_mode);

	ret = shub_file_write_no_wait(PROX_SETTING_MODE_FILE_PATH, (char *)&data->setting_mode,
					sizeof(data->setting_mode), 0);
	if (ret != sizeof(data->setting_mode)) {
		shub_errf("failed");
		return -EIO;
	}

	return ret;
}

int open_default_proximity_setting_mode(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	u8 mode;

	ret = shub_file_read(PROX_SETTING_MODE_FILE_PATH, (char *)&mode, sizeof(mode), 0);
	if (ret != sizeof(mode)) {
		shub_errf("failed");
		ret = -EIO;
	} else {
		data->setting_mode = mode;
	}

	shub_infof("%d", data->setting_mode);

	return ret;
}

static struct proximity_data proximity_data;
static struct sensor_funcs proximity_sensor_funcs = {
	.enable = enable_proximity,
	.sync_status = sync_proximity_status,
	.print_debug = print_debug_proximity,
	.report_event = report_event_proximity,
	.parsing_data = parsing_proximity_threshold,
	.open_calibration_file = open_proximity_calibration,
	.init_variable = init_proximity_variable,
	.get_init_chipset_funcs = get_proximity_init_chipset_funcs,
};

int init_proximity(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "proximity_sensor", 3, 1, sizeof(struct prox_event));
		sensor->data = (void *)&proximity_data;
		sensor->funcs = &proximity_sensor_funcs;
	} else {
		kfree_and_clear(proximity_data.threshold_data);
		kfree_and_clear(proximity_data.cal_data);
		destroy_default_func(sensor);
	}

	return ret;
}
