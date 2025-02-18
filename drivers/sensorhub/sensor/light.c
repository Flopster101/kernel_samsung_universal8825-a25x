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
#include "../utility/shub_utility.h"
#include "../utility/shub_file_manager.h"
#include "../others/shub_panel.h"
#include "light.h"
#include <linux/of_gpio.h>
#include <linux/slab.h>

get_init_chipset_funcs_ptr get_light_funcs_ary[] = {
	get_light_stk_common_function_pointer,
};

static get_init_chipset_funcs_ptr *get_light_init_chipset_funcs(int *len)
{
	*len = ARRAY_SIZE(get_light_funcs_ary);
	return get_light_funcs_ary;
}


static int init_light_variable(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT);
	struct light_data *data = sensor->data;
	struct shub_system_info *system_info = get_shub_system_info();

	data->brightness = -1;
	if(sensor->spec.version >= LIGHT_DEBIG_EVENT_SIZE_4BYTE_VERSION)
		data->raw_data_size = 4;
	else
		data->raw_data_size = 2;

	set_light_ddi_support(system_info->system_feature);

	return 0;
}

static void parse_dt_light(struct device *dev)
{
	struct light_data *data = get_sensor(SENSOR_TYPE_LIGHT)->data;
	struct device_node *np = dev->of_node;
	int coef[LIGHT_COEF_SIZE] = {0, };

	if (!of_property_read_u32_array(np, "light-coef", coef, LIGHT_COEF_SIZE)) {
		data->light_coef = kcalloc(LIGHT_COEF_SIZE, sizeof(*data->light_coef), GFP_KERNEL);
		memcpy(data->light_coef, coef, sizeof(coef));
	}

	if (of_property_read_u32(np, "brightness-array-len", &data->brightness_array_len)) {
		shub_errf("no brightness array len");
		data->brightness_array_len = 0;
		data->brightness_array = NULL;
	} else {
		data->brightness_array = kcalloc(data->brightness_array_len, sizeof(*data->brightness_array),
						 GFP_KERNEL);
		if (of_property_read_u32_array(np, "brightness-array", data->brightness_array,
					       data->brightness_array_len)) {
			shub_errf("no brightness array");
			data->brightness_array_len = 0;
			kfree(data->brightness_array);
			data->brightness_array = NULL;
		}
	}
}

void set_light_coef(struct light_data *data)
{
	int ret = 0;

	if (!get_sensor_probe_state(SENSOR_TYPE_LIGHT)) {
		shub_infof("light sensor is not connected");
		return;
	}

	if (!data->light_coef)
		return;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT, LIGHT_COEF,
				(char *)data->light_coef, LIGHT_COEF_SIZE);
	if (ret < 0) {
		shub_errf("MSG2SHUB_AP_SET_LIGHT_COEF CMD fail %d\n", ret);
		return;
	}

	shub_infof("%d %d %d %d %d %d %d\n", data->light_coef[0], data->light_coef[1], data->light_coef[2],
		   data->light_coef[3], data->light_coef[4], data->light_coef[5], data->light_coef[6]);
}

int set_light_brightness(struct light_data *data)
{
	int ret = 0;

	if (!get_sensor_probe_state(SENSOR_TYPE_LIGHT)) {
		shub_infof("light sensor is not connected");
		return ret;
	}

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT, LIGHT_BRIGHTNESS, (char *)&data->brightness,
				sizeof(data->brightness));
	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return ret;
	}

	shub_infof("%d", data->brightness);

	return ret;
}

#ifdef CONFIG_SENSORS_SSP_LIGHT_JPNCONCEPT
int set_light_region(struct light_data *data)
{
	int ret = 0;
	char region = 0;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT, LIGHT_PRJ_REGION, (char *)&region, sizeof(region));

	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return ret;
	}

	shub_infof("%d", region);
	int ret;
}
#endif

void set_light_ddi_support(uint32_t system_feature)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT);
	struct light_data *data = sensor->data;

	data->ddi_support = is_support_system_feature(SF_DDI_SUPPORT);

	shub_infof("%d", data->ddi_support);
}

int light_open_calibration(void)
{
	int ret;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT);
	struct light_data *data = sensor->data;

	ret = shub_file_read(LIGHT_CALIBRATION_FILE_PATH, (char *)&data->cal_data, sizeof(data->cal_data), 0);
	if (ret != sizeof(data->cal_data)) {
		shub_errf("Can't read calibration file %d", ret);
		return -EIO;
	}

	shub_infof("%d %d %d", data->cal_data.result, data->cal_data.max, data->cal_data.lux);

	return ret;
}

static int set_light_cal(struct light_data *data)
{
	int ret = 0;

	if (!data->use_cal_data || is_panel_ubid_changed())
		return 0;

	shub_infof("%d %d %d", data->cal_data.result, data->cal_data.max, data->cal_data.lux);

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT, CAL_DATA, (u8 *)(&data->cal_data),
							sizeof(data->cal_data));
	if (ret < 0)
		shub_errf("shub_send_command fail %d", ret);

	return ret;
}

static int set_panel_vendor(struct light_data *data)
{
	int ret = 0;

	data->panel_vendor = get_panel_lcd_type();
	if (data->panel_vendor < 0)
		return ret;

	shub_infof("%d", data->panel_vendor);
	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT, LIGHT_SUBCMD_PANEL_TYPE, (u8 *)(&data->panel_vendor),
							sizeof(int8_t));
	if (ret < 0)
		shub_errf("shub_send_command fail %d", ret);

	return ret;
}

static int set_hbm_finger(struct light_data *data)
{
	int ret = 0;

	shub_infof("%d", data->hbm_finger);
	if (data->hbm_finger == true) {
		ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT, LIGHT_SUBCMD_HBM_FINGERPRINT,
		(u8 *)(&data->hbm_finger), sizeof(data->hbm_finger));
	}
	if (ret < 0)
		shub_errf("shub_send_command fail %d", ret);

	return ret;
}

static int sync_light_status(void)
{
	int ret = 0;
	struct light_data *data = get_sensor(SENSOR_TYPE_LIGHT)->data;
	shub_info("sync_light_status ");
	set_light_coef(data);
	set_light_brightness(data);
#ifdef CONFIG_SENSORS_SSP_LIGHT_JPNCONCEPT
	set_light_region(data);
#endif
	set_light_cal(data);
	set_panel_vendor(data);
	set_hbm_finger(data);
	return ret;
}

static int enable_light(void)
{
	struct light_data *data = get_sensor(SENSOR_TYPE_LIGHT)->data;

	data->light_log_cnt = 0;
	return 0;
}

static void report_event_light(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT);
	struct light_data *data = sensor->data;
	struct light_event *sensor_value = (struct light_event *)(sensor->event_buffer.value);

	if (data->light_log_cnt < 3) {
		shub_info("Light Sensor : lux=%u brightness=%u r=%d g=%d b=%d c=%d atime=%d again=%d",
			  sensor_value->lux, sensor_value->brightness, sensor_value->r, sensor_value->g,
			  sensor_value->b, sensor_value->w, sensor_value->a_time, sensor_value->a_gain);

		data->light_log_cnt++;
	}
}

void print_light_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct light_cct_event *sensor_value = (struct light_cct_event *)(event->value);

	shub_info("%s(%u) : %u(%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_LIGHT, sensor_value->lux,
		  event->timestamp, sensor->sampling_period, sensor->max_report_latency);
}

int inject_light_additional_data(char *buf, int count)
{
	int cur_level = 0;
	int cal_brightness = 0;
	int32_t brightness;
	int i, ret = 0;
	struct light_data *data = get_sensor(SENSOR_TYPE_LIGHT)->data;

	if (count < 1) {
		shub_errf("brightness length error %d", count);
		return -EINVAL;
	} else if (count == sizeof(int8_t)) {
		int8_t finger_print;

		finger_print = *((int8_t *)(buf));
		shub_infof("finger_print on/off = %d", finger_print);
		if (data->hbm_finger != finger_print) {
			shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT, LIGHT_SUBCMD_HBM_FINGERPRINT,
								(char *)&finger_print, sizeof(finger_print));
		}
		data->hbm_finger = finger_print;
	} else if (count == sizeof(int32_t)) {
		brightness = *((int32_t *)(buf));
		cal_brightness = brightness / 10;
		cal_brightness *= 10;

		// shub_errf("br %d, cal_br %d", brightness, cal_brightness);
		// set current level for changing itime
		for (i = 0; i < data->brightness_array_len; i++) {
			if (brightness <= data->brightness_array[i]) {
				cur_level = i + 1;
				// shub_infof("brightness %d <= %d , level %d", brightness, data->brightness_array[i],
				// cur_level);
				break;
			}
		}

		if (data->last_brightness_level != cur_level) {
			data->brightness = brightness;
			// update last level
			data->last_brightness_level = cur_level;
			ret = set_light_brightness(data);
			data->brightness = cal_brightness;
		} else if (data->brightness != cal_brightness) {
			data->brightness = brightness;
			ret = set_light_brightness(data);
			data->brightness = cal_brightness;
		}
	} else if (count == sizeof(int32_t)*4) {
		int32_t data[4] = {0,};

		if (!get_sensor_probe_state(SENSOR_TYPE_LIGHT)) {
			shub_infof("light sensor is not connected");
			return ret;
		}
		memcpy(data, buf, sizeof(data));
		shub_infof("target br %d threshold_dark %d lux %d threshold_bright %d", data[0], data[1], data[2], data[3]);
		ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT, LIGHT_SUBCMD_BRIGHTNESS_HYSTERESIS, (char *)data, sizeof(data));

		if (ret < 0) {
			shub_errf("CMD fail %d\n", ret);
			return ret;
		}
	}

	return ret;
}

int get_light_sensor_value(char *dataframe, int *index, struct sensor_event *event, int frame_len)
{
	struct light_data *data = get_sensor(SENSOR_TYPE_LIGHT)->data;
	struct light_event *sensor_value = (struct light_event *)event->value;

	memcpy(&sensor_value->lux, dataframe + *index, sizeof(sensor_value->lux));
	*index += sizeof(sensor_value->lux);
	memcpy(&sensor_value->cct, dataframe + *index, sizeof(sensor_value->cct));
	*index += sizeof(sensor_value->cct);
	memcpy(&sensor_value->raw_lux, dataframe + *index, sizeof(sensor_value->raw_lux));
	*index += sizeof(sensor_value->raw_lux);

	memcpy(&sensor_value->r, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->g, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->b, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->w, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->a_time, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->a_gain, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;

	memcpy(&sensor_value->brightness, dataframe + *index, sizeof(sensor_value->brightness));
	*index += sizeof(sensor_value->brightness);

	return 0;
}


static struct light_data light_data;
static struct sensor_funcs light_sensor_funcs = {
	.sync_status = sync_light_status,
	.enable = enable_light,
	.report_event = report_event_light,
	.print_debug = print_light_debug,
	.inject_additional_data = inject_light_additional_data,
	.get_sensor_value = get_light_sensor_value,
	.open_calibration_file = light_open_calibration,
	.parse_dt = parse_dt_light,
	.init_variable = init_light_variable,
	.get_init_chipset_funcs = get_light_init_chipset_funcs,
};

int init_light(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "light_sensor",
				sensor->spec.version >= LIGHT_DEBIG_EVENT_SIZE_4BYTE_VERSION ? 40 : 28, 4, sizeof(struct light_event));
		sensor->data = (void *)&light_data;
		sensor->funcs = &light_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
