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

#include "../../utility/shub_utility.h"
#include "../../comm/shub_comm.h"
#include "../../sensor/flip_cover_detector.h"
#include "../../sensor/magnetometer.h"
#include "../../sensorhub/shub_device.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_dev_core.h"
#include "flip_cover_detector_factory.h"

#include <linux/slab.h>
#include <linux/of.h>

struct factory_cover_status_data {
	char cover_status[10];
	uint8_t axis_select;
	int32_t threshold;
	int32_t init[AXIS_MAX];
	int32_t attach[AXIS_MAX];
	int32_t attach_extremum[AXIS_MAX];
	int32_t detach[AXIS_MAX];
	char attach_result[10];
	char detach_result[10];
	char final_result[10];

	uint8_t factory_test_status;
	int32_t attach_diff;
	int32_t detach_diff;
	int32_t failed_attach_max;
	int32_t failed_attach_min;
	uint8_t saturation;

	int event_count;
};

static struct device *fcd_sysfs_device;
static struct factory_cover_status_data *factory_data;

static char sysfs_cover_status[10];

static ssize_t nfc_cover_status_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct flip_cover_detector_data *data = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)->data;

	if (data->nfc_cover_status == COVER_ATTACH || data->nfc_cover_status == COVER_ATTACH_NFC_ACTIVE || data->nfc_cover_status == COVER_ATTACH_NFC_TAG_PRESENT) {
		snprintf(sysfs_cover_status, 10, "CLOSE");
	} else if (data->nfc_cover_status == COVER_DETACH){
		snprintf(sysfs_cover_status, 10, "OPEN");
	}

	shub_infof("[FACTORY] nfc_cover_status=%s", sysfs_cover_status);

	return snprintf(buf, PAGE_SIZE, "%s\n",	sysfs_cover_status);
}

static ssize_t nfc_cover_status_store(struct device *dev,
				      struct device_attribute *attr, const char *buf, size_t size)
{
	int status = 0;
	int ret = 0;
	struct flip_cover_detector_data *data = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)->data;

	ret = kstrtoint(buf, 10, &status);
	if (ret < 0)
		return ret;

	if (!(status == 0 || status == 1 || status == 2 || status == 7)) {
		shub_errf("invalid status %d", status);
		return -EINVAL;
	}

	data->nfc_cover_status = status;
	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_FLIP_COVER_DETECTOR, LIBRARY_DATA,
				(char *)&data->nfc_cover_status, sizeof(data->nfc_cover_status));
	if (ret < 0)
		shub_errf("send nfc_cover_status failed");

	shub_infof("nfc_cover_status is %d", data->nfc_cover_status);

	return size;
}

static void factory_data_init(void)
{
	int mag_data[AXIS_MAX];
	int axis = 0;
	struct flip_cover_detector_data *data = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)->data;
	struct sensor_event *event = get_sensor_event(SENSOR_TYPE_FLIP_COVER_DETECTOR);
	struct flip_cover_detector_event *sensor_value = (struct flip_cover_detector_event *)event->value;

	shub_infof("");

	memset(factory_data, 0, sizeof(struct factory_cover_status_data));
	mag_data[X] = sensor_value->uncal_mag_x;
	mag_data[Y] = sensor_value->uncal_mag_y;
	mag_data[Z] = sensor_value->uncal_mag_z;

	shub_infof("[FACTORY] init data : %d %d %d", mag_data[X], mag_data[Y], mag_data[Z]);

	factory_data->axis_select = data->axis_update;
	factory_data->threshold = (data->threshold_update > 0) ? data->threshold_update : data->threshold_update * (-1);

	for (axis = X; axis < AXIS_MAX; axis++) {
		factory_data->init[axis] = mag_data[axis];
		factory_data->attach[axis] = mag_data[axis];
	}

	factory_data->failed_attach_max = mag_data[factory_data->axis_select];
	factory_data->failed_attach_min = mag_data[factory_data->axis_select];

	snprintf(factory_data->cover_status, 10, DETACH);
	snprintf(factory_data->attach_result, 10, FACTORY_FAIL);
	snprintf(factory_data->detach_result, 10, FACTORY_FAIL);
	snprintf(factory_data->final_result, 10, FACTORY_FAIL);
}

void check_cover_detection_factory(void)
{
	int axis = 0;
	int axis_select = factory_data->axis_select;
	int mag_data[AXIS_MAX];
	struct sensor_event *event = get_sensor_event(SENSOR_TYPE_FLIP_COVER_DETECTOR);
	struct flip_cover_detector_event *sensor_value = (struct flip_cover_detector_event *)event->value;

	if (factory_data->event_count == 0) {
		factory_data_init();
		factory_data->event_count++;
	}

	mag_data[X] = sensor_value->uncal_mag_x;
	mag_data[Y] = sensor_value->uncal_mag_y;
	mag_data[Z] = sensor_value->uncal_mag_z;
	factory_data->saturation = sensor_value->saturation;

	shub_infof("[FACTORY] uncal mag : %d %d %d, saturation : %d",
		  mag_data[X], mag_data[Y], mag_data[Z], factory_data->saturation);

	if (!strcmp(factory_data->cover_status, DETACH)) {
		if (mag_data[axis_select] > factory_data->failed_attach_max) {
			factory_data->failed_attach_max = mag_data[axis_select];

			if (abs(factory_data->failed_attach_max - factory_data->init[axis_select])
				> abs(factory_data->failed_attach_min - factory_data->init[axis_select])) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->attach[axis] = mag_data[axis];
			}
		} else if (mag_data[axis_select] < factory_data->failed_attach_min) {
			factory_data->failed_attach_min = mag_data[axis_select];

			if (abs(factory_data->failed_attach_max - factory_data->init[axis_select])
				< abs(factory_data->failed_attach_min - factory_data->init[axis_select])) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->attach[axis] = mag_data[axis];
			}
		}

		shub_infof("[FACTORY] : failed_attach_max=%d, failed_attach_min=%d",
			  factory_data->failed_attach_max, factory_data->failed_attach_min);

		factory_data->attach_diff = mag_data[axis_select] - factory_data->init[axis_select];

		if (abs(factory_data->attach_diff) > factory_data->threshold) {
			snprintf(factory_data->cover_status, 10, ATTACH);
			snprintf(factory_data->attach_result, 10, FACTORY_PASS);
			for (axis = X; axis < AXIS_MAX; axis++) {
				factory_data->attach[axis] = mag_data[axis];
				factory_data->attach_extremum[axis] = mag_data[axis];
			}
			shub_infof("[FACTORY] : COVER ATTACHED");
		}
	} else {
		if (factory_data->attach_diff > 0) {
			if (factory_data->saturation) {
				for (axis = X; axis < AXIS_MAX; axis++)
					mag_data[axis] = SATURATION_VALUE;
			}

			if (mag_data[axis_select] > factory_data->attach_extremum[axis_select]) {
				for (axis = X; axis < AXIS_MAX; axis++) {
					factory_data->attach_extremum[axis] = mag_data[axis];
					factory_data->detach[axis] = 0;
				}
			}
		} else {
			if (factory_data->saturation) {
				for (axis = X; axis < AXIS_MAX; axis++)
					mag_data[axis] = -SATURATION_VALUE;
			}

			if (mag_data[axis_select] < factory_data->attach_extremum[axis_select]) {
				for (axis = X; axis < AXIS_MAX; axis++) {
					factory_data->attach_extremum[axis] = mag_data[axis];
					factory_data->detach[axis] = 0;
				}
			}
		}

		factory_data->detach_diff = mag_data[axis_select] - factory_data->attach_extremum[axis_select];

		if (factory_data->attach_diff > 0) {
			if (mag_data[axis_select] < (factory_data->attach_extremum[axis_select] - DETACH_MARGIN)) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->detach[axis] = mag_data[axis];
			}

			if (factory_data->detach_diff < -factory_data->threshold) {
				snprintf(factory_data->cover_status, 10, DETACH);
				snprintf(factory_data->detach_result, 10, FACTORY_PASS);
				snprintf(factory_data->final_result, 10, FACTORY_PASS);
				factory_data->factory_test_status = OFF;
				shub_infof("[FACTORY] : COVER_DETACHED");
			}
		} else {
			if (mag_data[axis_select] > (factory_data->attach_extremum[axis_select] + DETACH_MARGIN)) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->detach[axis] = mag_data[axis];
			}

			if (factory_data->detach_diff > factory_data->threshold) {
				snprintf(factory_data->cover_status, 10, DETACH);
				snprintf(factory_data->detach_result, 10, FACTORY_PASS);
				snprintf(factory_data->final_result, 10, FACTORY_PASS);
				factory_data->factory_test_status = OFF;
				shub_infof("[FACTORY]: COVER_DETACHED");
			}
		}
	}

	shub_infof("[FACTORY] : cover_status=%s, axis_select=%d, thd=%d, \
		    x_init=%d, x_attach=%d, x_min_max=%d, x_detach=%d, \
		    y_init=%d, y_attach=%d, y_min_max=%d, y_detach=%d, \
		    z_init=%d, z_attach=%d, z_min_max=%d, z_detach=%d, \
		    attach_result=%s, detach_result=%s, final_result=%s",
		    factory_data->cover_status, factory_data->axis_select, factory_data->threshold,
		    factory_data->init[X], factory_data->attach[X], factory_data->attach_extremum[X],
		    factory_data->detach[X], factory_data->init[Y], factory_data->attach[Y],
		    factory_data->attach_extremum[Y], factory_data->detach[Y], factory_data->init[Z],
		    factory_data->attach[Z], factory_data->attach_extremum[Z], factory_data->detach[Z],
		    factory_data->attach_result, factory_data->detach_result, factory_data->final_result);
}

static void enable_factory_test(int request)
{
	int ret = 0;
	struct flip_cover_detector_data *data = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)->data;

	data->factory_cover_status = request;
	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_FLIP_COVER_DETECTOR, SENSOR_FACTORY,
				(char *)&data->factory_cover_status,
				sizeof(data->factory_cover_status));
	if (ret < 0) {
		shub_errf("send factory_cover_status failed");
		return;
	}

	if (request == ON)
		factory_data->event_count = 0;
}

static ssize_t factory_cover_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	shub_infof("[FACTORY] status=%s, axis=%d, thd=%d, \
		   x_init=%d, x_attach=%d, x_min_max=%d, x_detach=%d, \
		   y_init=%d, y_attach=%d, y_min_max=%d, y_detach=%d, \
		   z_init=%d, z_attach=%d, z_min_max=%d, z_detach=%d, \
		   attach_result=%s, detach_result=%s, final_result=%s",
		   factory_data->cover_status, factory_data->axis_select, factory_data->threshold,
		   factory_data->init[X], factory_data->attach[X], factory_data->attach_extremum[X],
		   factory_data->detach[X], factory_data->init[Y], factory_data->attach[Y],
		   factory_data->attach_extremum[Y], factory_data->detach[Y], factory_data->init[Z],
		   factory_data->attach[Z], factory_data->attach_extremum[Z], factory_data->detach[Z],
		   factory_data->attach_result, factory_data->detach_result, factory_data->final_result);

	return snprintf(buf, PAGE_SIZE, "%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%s,%s\n",
		factory_data->cover_status, factory_data->axis_select, factory_data->threshold,
		factory_data->init[X], factory_data->attach[X], factory_data->attach_extremum[X],
		factory_data->detach[X], factory_data->init[Y], factory_data->attach[Y],
		factory_data->attach_extremum[Y], factory_data->detach[Y], factory_data->init[Z],
		factory_data->attach[Z], factory_data->attach_extremum[Z], factory_data->detach[Z],
		factory_data->attach_result, factory_data->detach_result, factory_data->final_result);
}

static ssize_t factory_cover_status_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	int factory_test_request = 0;
	int ret = 0;
	struct flip_cover_detector_data *data = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)->data;

	ret = kstrtoint(buf, 10, &factory_test_request);
	if (ret < 0)
		return ret;

	if (factory_test_request < 0 || factory_test_request > 1) {
		shub_errf("[FACTORY] invalid status %d", factory_test_request);
		return -EINVAL;
	}

	if (factory_test_request == ON /*&& factory_data->factory_test_status == OFF*/)
		enable_factory_test(ON);
	else if (factory_test_request == OFF /*&& factory_data->factory_test_status == ON*/)
		enable_factory_test(OFF);

	shub_infof("[FACTORY] factory_cover_status is %d", data->factory_cover_status);
	return size;
}

static ssize_t axis_threshold_setting_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct flip_cover_detector_data *data = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)->data;

	shub_infof("[FACTORY] axis=%d, threshold=%d", data->axis_update, data->threshold_update);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", data->axis_update, data->threshold_update);
}

static ssize_t axis_threshold_setting_store(struct device *dev,
					    struct device_attribute *attr, const char *buf, size_t size)
{
	struct flip_cover_detector_data *data = get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)->data;
	int ret;
	int axis;
	int threshold;
	int8_t shub_data[5] = {0};

	ret = sscanf(buf, "%d,%d", &axis, &threshold);

	if (ret != 2) {
		shub_errf("[FACTORY] Invalid values received, ret=%d\n", ret);
		return -EINVAL;
	}

	if (axis < 0 || axis >= AXIS_MAX) {
		shub_errf("[FACTORY] Invalid axis value received\n");
		return -EINVAL;
	}

	shub_infof("[FACTORY] axis=%d, threshold=%d\n", axis, threshold);

	data->axis_update = axis;
	data->threshold_update = threshold;

	memcpy(shub_data, &data->axis_update, sizeof(data->axis_update));
	memcpy(shub_data + 1, &data->threshold_update, sizeof(data->threshold_update));

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_FLIP_COVER_DETECTOR, CAL_DATA, shub_data, sizeof(shub_data));
	if (ret < 0)
		shub_errf("send axis/threshold failed, ret=%d", ret);

	return size;
}

static ssize_t cal_matrix_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	u8 cal_matrix_num;
	char *buffer = NULL;
	int buffer_length = 0;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_FLIP_COVER_DETECTOR, FCD_SUBCMD_CAL_MATRIX_NUM,
				     1000, NULL, 0, &buffer, &buffer_length, false);
	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		return ret;
	}
	if (buffer_length != 1) {
		shub_errf("buffer length error %d", buffer_length);
		kfree(buffer);
		return -EINVAL;
	}

	cal_matrix_num = buffer[0];
	shub_infof("%d\n", cal_matrix_num);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", cal_matrix_num);
	kfree(buffer);

	return ret;
}

static DEVICE_ATTR(nfc_cover_status, 0664, nfc_cover_status_show, nfc_cover_status_store);
static DEVICE_ATTR(factory_cover_status, 0664, factory_cover_status_show, factory_cover_status_store);
static DEVICE_ATTR(axis_threshold_setting, 0664, axis_threshold_setting_show, axis_threshold_setting_store);
static DEVICE_ATTR_RO(cal_matrix_num);

static struct device_attribute *fcd_attrs[] = {
	&dev_attr_nfc_cover_status,
	&dev_attr_factory_cover_status,
	&dev_attr_axis_threshold_setting,
	&dev_attr_cal_matrix_num,
	NULL,
};

static void initialize_fcd_factorytest(void)
{
	factory_data = kzalloc(sizeof(*factory_data), GFP_KERNEL);
	if (factory_data == NULL)
		shub_errf("[FACTORY] Memory allocation failed for factory_data");

	sensor_device_create(&fcd_sysfs_device, NULL, "flip_cover_detector_sensor");
	add_sensor_device_attr(fcd_sysfs_device, fcd_attrs);

	snprintf(sysfs_cover_status, 10, "OPEN");
}

static void remove_fcd_factorytest(void)
{
	remove_sensor_device_attr(fcd_sysfs_device, fcd_attrs);
	sensor_device_destroy(fcd_sysfs_device);

	if (factory_data != NULL) {
		if (factory_data->factory_test_status == ON)
			enable_factory_test(OFF);
	}

	kfree(factory_data);
}

void initialize_flip_cover_detector_factory(bool en)
{
	if (!get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR) || !check_flip_cover_detector_supported())
		return;

	if (en)
		initialize_fcd_factorytest();
	else
		remove_fcd_factorytest();
}