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

#include "../../sensor/accelerometer.h"
#include "../../sensorhub/shub_device.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_dev_core.h"
#include "../../utility/shub_utility.h"
#include "../../utility/shub_file_manager.h"
#include "../../comm/shub_comm.h"

#if defined(CONFIG_SHUB_KUNIT)
#include <kunit/mock.h>
#define __mockable __weak
#define __visible_for_testing
#else
#define __mockable
#define __visible_for_testing static
#endif

#include <linux/delay.h>
#include <linux/slab.h>

#define MAX_ACCEL_1G_8G 4096
#define MAX_ACCEL_1G_16G 2048
#define MIN_ACCEL_1G_8G -4096
#define MIN_ACCEL_1G_16G -2048
#define MAX_ACCEL_2G 8192
#define MIN_ACCEL_2G -8192
#define MAX_ACCEL_4G 16384

#define CALIBRATION_DATA_AMOUNT 20
/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

static struct device *accel_sysfs_device;

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER);

	return sprintf(buf, "%s\n", sensor->spec.name);
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER);
	char vendor[VENDOR_MAX] = "";

	get_sensor_vendor_name(sensor->spec.vendor, vendor);

	return sprintf(buf, "%s\n", vendor);
}

static ssize_t accel_calibration_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER);
	struct accelerometer_data *data = sensor->data;

	ret = sensor->funcs->open_calibration_file();
	if (ret < 0)
		shub_errf("calibration open failed(%d)", ret);

	shub_infof("Cal data : %d %d %d - %d", data->cal_data.x, data->cal_data.y, data->cal_data.z, ret);

	return sprintf(buf, "%d %d %d %d\n", ret, data->cal_data.x, data->cal_data.y, data->cal_data.z);
}

static int accel_do_calibrate(int enable)
{
	int iSum[3] = {0, };
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER);
	uint32_t backup_sampling_period = sensor->sampling_period;
	uint32_t backup_max_report_latency = sensor->max_report_latency;
	struct accelerometer_data *data = sensor->data;
	int range = MAX_ACCEL_1G_8G;
	struct accel_event *sensor_value = (struct accel_event *)sensor->event_buffer.value;

	if (data->range == 16) {
		range = MAX_ACCEL_1G_16G;
	}

	if (enable) {
		int count;

		data->cal_data.x = 0;
		data->cal_data.y = 0;
		data->cal_data.z = 0;
		set_accel_cal(data);

		batch_sensor(SENSOR_TYPE_ACCELEROMETER, 10, 0);
		enable_sensor(SENSOR_TYPE_ACCELEROMETER, NULL, 0);

		msleep(300);

		for (count = 0; count < CALIBRATION_DATA_AMOUNT; count++) {
			iSum[0] += sensor_value->x;
			iSum[1] += sensor_value->y;
			iSum[2] += sensor_value->z;
			mdelay(10);
		}

		batch_sensor(SENSOR_TYPE_ACCELEROMETER, backup_sampling_period, backup_max_report_latency);
		disable_sensor(SENSOR_TYPE_ACCELEROMETER, NULL, 0);

		data->cal_data.x = (iSum[0] / CALIBRATION_DATA_AMOUNT);
		data->cal_data.y = (iSum[1] / CALIBRATION_DATA_AMOUNT);
		data->cal_data.z = (iSum[2] / CALIBRATION_DATA_AMOUNT);

		if (data->cal_data.z > 0)
			data->cal_data.z -= range;
		else if (data->cal_data.z < 0)
			data->cal_data.z += range;

	} else {
		data->cal_data.x = 0;
		data->cal_data.y = 0;
		data->cal_data.z = 0;
	}

	shub_infof("do accel calibrate %d, %d, %d", data->cal_data.x, data->cal_data.y, data->cal_data.z);

	ret = shub_file_write(ACCEL_CALIBRATION_FILE_PATH, (char *)&data->cal_data, sizeof(data->cal_data), 0);
	if (ret != sizeof(data->cal_data)) {
		shub_errf("Can't write the accelcal to file");
		ret = -EIO;
	}

	set_accel_cal(data);
	return ret;
}

static ssize_t accel_calibration_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int64_t enable;

	ret = kstrtoll(buf, 10, &enable);
	if (ret < 0)
		return ret;

	ret = accel_do_calibrate((int)enable);
	if (ret < 0)
		shub_errf("accel_do_calibrate() failed");

	return size;
}

static ssize_t raw_data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct accel_event *sensor_value;

	if (!get_sensor_probe_state(SENSOR_TYPE_ACCELEROMETER)) {
		shub_errf("sensor is not probed!");
		return 0;
	}

	sensor_value = (struct accel_event *)(get_sensor_event(SENSOR_TYPE_ACCELEROMETER)->value);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", sensor_value->x, sensor_value->y,
			sensor_value->z);
}

static ssize_t accel_reactive_alert_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	bool success = false;
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;

	if (data->is_accel_alert == true)
		success = true;
	else
		success = false;

	data->is_accel_alert = false;
	return sprintf(buf, "%u\n", success);
}

static ssize_t accel_reactive_alert_store(struct device *dev, struct device_attribute *attr, const char *buf,
					  size_t size)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_length = 0;
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;

	if (sysfs_streq(buf, "1")) {
		shub_infof("on");
	} else if (sysfs_streq(buf, "0")) {
		shub_infof("off");
	} else if (sysfs_streq(buf, "2")) {
		shub_infof("factory");

		data->is_accel_alert = 0;

		ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_ACCELEROMETER, ACCELOMETER_REACTIVE_ALERT, 3000, NULL, 0,
					     &buffer, &buffer_length, true);

		if (ret < 0) {
			shub_errf("shub_send_command_wait Fail %d", ret);
			goto exit;
		}

		if (buffer_length < 1) {
			shub_errf("length err %d", buffer_length);
			ret = -EINVAL;
			goto exit;
		}

		data->is_accel_alert = *buffer;

		shub_infof("factory test success!");
	} else {
		shub_errf("invalid value %d", *buf);
		ret = -EINVAL;
	}

exit:
	kfree(buffer);
	return size;
}

static ssize_t accel_lowpassfilter_store(struct device *dev, struct device_attribute *attr, const char *buf,
					 size_t size)
{
	int ret = 0;
	int new_enable = 1;
	char temp = 0;

	if (sysfs_streq(buf, "1"))
		new_enable = 1;
	else if (sysfs_streq(buf, "0"))
		new_enable = 0;
	else
		shub_infof(" invalid value!");

	temp = new_enable;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_ACCELEROMETER, ACCELOMETER_LPF_ON_OFF, &temp, sizeof(temp));

	if (ret < 0)
		shub_errf("shub_send_command Fail %d", ret);

	return size;
}


static ssize_t selftest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *buffer = NULL;
	int buffer_length = 0;
	s8 init_status = 0, result = -1;
	u16 diff_axis[3] = {0, };
	u16 shift_ratio_N[3] = {0, };
	int ret = 0;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_ACCELEROMETER, SENSOR_FACTORY, 3000, NULL, 0, &buffer,
				     &buffer_length, true);

	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		ret = sprintf(buf, "%d,%d,%d,%d\n", -5, 0, 0, 0);
		return ret;
	}

	if (buffer_length != 14) {
		shub_errf("length err %d", buffer_length);
		ret = sprintf(buf, "%d,%d,%d,%d\n", -5, 0, 0, 0);
		kfree(buffer);

		return -EINVAL;
	}

	init_status = buffer[0];
	diff_axis[0] = ((s16)(buffer[2] << 8)) + buffer[1];
	diff_axis[1] = ((s16)(buffer[4] << 8)) + buffer[3];
	diff_axis[2] = ((s16)(buffer[6] << 8)) + buffer[5];

	/* negative axis */

	shift_ratio_N[0] = ((s16)(buffer[8] << 8)) + buffer[7];
	shift_ratio_N[1] = ((s16)(buffer[10] << 8)) + buffer[9];
	shift_ratio_N[2] = ((s16)(buffer[12] << 8)) + buffer[11];
	result = buffer[13];

	shub_infof("%d, %d, %d, %d, %d, %d, %d, %d\n", init_status, result, diff_axis[0], diff_axis[1], diff_axis[2],
		   shift_ratio_N[0], shift_ratio_N[1], shift_ratio_N[2]);

	ret = sprintf(buf, "%d,%d,%d,%d,%d,%d,%d\n", result, diff_axis[0], diff_axis[1], diff_axis[2], shift_ratio_N[0],
		      shift_ratio_N[1], shift_ratio_N[2]);

	kfree(buffer);

	return ret;
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR_RO(raw_data);
static DEVICE_ATTR(calibration, 0664, accel_calibration_show, accel_calibration_store);
static DEVICE_ATTR(reactive_alert, 0664, accel_reactive_alert_show, accel_reactive_alert_store);
static DEVICE_ATTR(lowpassfilter, 0220, NULL, accel_lowpassfilter_store);
static DEVICE_ATTR_RO(selftest);

__visible_for_testing struct device_attribute *acc_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_calibration,
	&dev_attr_raw_data,
	&dev_attr_reactive_alert,
	&dev_attr_lowpassfilter,
	&dev_attr_selftest,
	NULL,
};

void initialize_accelerometer_sysfs(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER);
	int ret;

	ret = sensor_device_create(&accel_sysfs_device, NULL, "accelerometer_sensor");
	if (ret < 0) {
		shub_errf("fail to creat %s sysfs device", sensor->name);
		return;
	}

	ret = add_sensor_device_attr(accel_sysfs_device, acc_attrs);
	if (ret < 0) {
		shub_errf("fail to add %s sysfs device attr", sensor->name);
		return;
	}
}

void remove_accelerometer_sysfs(void)
{
	remove_sensor_device_attr(accel_sysfs_device, acc_attrs);
	sensor_device_destroy(accel_sysfs_device);
	accel_sysfs_device = NULL;
}

void initialize_accelerometer_factory(bool en)
{
	if (!get_sensor(SENSOR_TYPE_ACCELEROMETER))
		return;

	if (en)
		initialize_accelerometer_sysfs();
	else
		remove_accelerometer_sysfs();
}
