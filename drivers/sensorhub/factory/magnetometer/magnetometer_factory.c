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

#include "magnetometer_factory.h"

#include "../../comm/shub_comm.h"
#include "../../sensor/magnetometer.h"
#include "../../sensorhub/shub_device.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_dev_core.h"
#include "../../utility/shub_utility.h"

#include <linux/delay.h>
#include <linux/slab.h>

#if defined(CONFIG_SHUB_KUNIT)
#include <kunit/mock.h>
#define __mockable __weak
#define __visible_for_testing
#else
#define __mockable
#define __visible_for_testing static
#endif

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

static struct device *mag_sysfs_device;
static struct device_attribute **chipset_attrs;
static u8 chipset_index;

typedef int (*check_adc_data_spec)(s32 sensor_value[3]);
check_adc_data_spec check_adc_data_spec_funcs[] = {
	check_ak09918c_adc_data_spec,
	check_mmc5633_adc_data_spec,
	check_yas539_adc_data_spec,
	check_mxg4300s_adc_data_spec,
};

void get_magnetometer_sensor_value_s32(struct mag_power_event *event, s32 *sensor_value)
{
	sensor_value[0] = (s32) event->x;
	sensor_value[1] = (s32) event->y;
	sensor_value[2] = (s32) event->z;
}

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* fuse rom data is not used */
	return sprintf(buf, "NG,0\n");
}

static ssize_t dac_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	bool bSuccess = false;
	char *buffer = NULL;
	int buffer_length = 0;
	int ret = 0;
	int geomag_cntl_regdata = 1;

	shub_infof("check cntl register before selftest");
	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, SENSOR_FACTORY, 1000, NULL, 0,
					&buffer, &buffer_length, true);

	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		return ret;
	}

	if (buffer_length < 22) {
		shub_errf("buffer length error %d", buffer_length);
		kfree(buffer);
		return -EINVAL;
	}

	geomag_cntl_regdata = buffer[21];
	bSuccess = !geomag_cntl_regdata;

	shub_info("CTRL : 0x%x", geomag_cntl_regdata);

	ret = sprintf(buf, "%s,%d,%d,%d\n", (bSuccess ? "OK" : "NG"), (bSuccess ? 1 : 0), 0, 0);

	kfree(buffer);

	return ret;
}

static ssize_t logging_data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *buffer = NULL;
	int buffer_length = 0;
	int ret = 0;
	int logging_data[8] = {0, };

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, SENSOR_FACTORY, 1000, NULL, 0,
				     &buffer, &buffer_length, true);

	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		ret = snprintf(buf, PAGE_SIZE, "-1,0,0,0,0,0,0,0,0,0,0\n");
		return ret;
	}

	if (buffer_length != 14) {
		shub_errf("buffer length error %d", buffer_length);
		ret = snprintf(buf, PAGE_SIZE, "-1,0,0,0,0,0,0,0,0,0,0\n");
		kfree(buffer);
		return -EINVAL;
	}

	logging_data[0] = buffer[0]; /* ST1 Reg */
	logging_data[1] = (short)((buffer[3] << 8) + buffer[2]);
	logging_data[2] = (short)((buffer[5] << 8) + buffer[4]);
	logging_data[3] = (short)((buffer[7] << 8) + buffer[6]);
	logging_data[4] = buffer[1]; /* ST2 Reg */
	logging_data[5] = (short)((buffer[9] << 8) + buffer[8]);
	logging_data[6] = (short)((buffer[11] << 8) + buffer[10]);
	logging_data[7] = (short)((buffer[13] << 8) + buffer[12]);

	ret = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,0,0,0\n", logging_data[0], logging_data[1],
		       logging_data[2], logging_data[3], logging_data[4], logging_data[5], logging_data[6],
		       logging_data[7]);

	kfree(buffer);

	return ret;
}

static ssize_t raw_data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mag_power_event *sensor_value;

	if (!get_sensor_probe_state(SENSOR_TYPE_GEOMAGNETIC_POWER)) {
		shub_errf("sensor is not probed!");
		return 0;
	}

	sensor_value = (struct mag_power_event *)(get_sensor_event(SENSOR_TYPE_GEOMAGNETIC_POWER)->value);
	shub_info("%d,%d,%d\n", sensor_value->x, sensor_value->y, sensor_value->z);

	if (!get_sensor_enabled(SENSOR_TYPE_GEOMAGNETIC_POWER)) {
		sensor_value->x = -1;
		sensor_value->y = -1;
		sensor_value->z = -1;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", sensor_value->x, sensor_value->y, sensor_value->z);
}

static ssize_t raw_data_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	char chTempbuf[8] = {0, };
	int ret = 0;
	int64_t dEnable;
	s32 dMsDelay = 20;
	s32 sensor_buf[3] = {0, };

	memcpy(&chTempbuf[0], &dMsDelay, 4);

	ret = kstrtoll(buf, 10, &dEnable);
	if (ret < 0)
		return ret;

	if (dEnable) {
		struct mag_power_event *sensor_value =
		    (struct mag_power_event *)(get_sensor_event(SENSOR_TYPE_GEOMAGNETIC_POWER)->value);

		int retries = 50;

		sensor_value->x = 0;
		sensor_value->y = 0;
		sensor_value->z = 0;

		batch_sensor(SENSOR_TYPE_GEOMAGNETIC_POWER, 20, 0);
		enable_sensor(SENSOR_TYPE_GEOMAGNETIC_POWER, NULL, 0);

		do {
			msleep(20);
			get_magnetometer_sensor_value_s32(sensor_value, sensor_buf);
			if (check_adc_data_spec_funcs[chipset_index](sensor_buf) == 0) { // success
				break;
			}
		} while (--retries);

		if (retries > 0)
			shub_infof("success, %d\n", retries);
		else
			shub_errf("wait timeout, %d\n", retries);

	} else {
		disable_sensor(SENSOR_TYPE_GEOMAGNETIC_POWER, NULL, 0);
	}

	return size;
}

static ssize_t adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	bool bSuccess = false;
	s32 sensor_buf[3] = {0, };
	int retries = 10;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);
	struct mag_event *sensor_value = (struct mag_event *)(sensor->event_buffer.value);

	sensor_value->x = 0;
	sensor_value->y = 0;
	sensor_value->z = 0;

	if (!sensor->enabled)
		batch_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD, 20, 0);
	enable_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD, NULL, 0);

	do {
		msleep(60);
		sensor_buf[0] = sensor_value->x;
		sensor_buf[1] = sensor_value->y;
		sensor_buf[2] = sensor_value->z;
		if (check_adc_data_spec_funcs[chipset_index](sensor_buf) == 0) { // success
			break;
		}
	} while (--retries);

	if (retries > 0)
		bSuccess = true;

	sensor_buf[0] = sensor_value->x;
	sensor_buf[1] = sensor_value->y;
	sensor_buf[2] = sensor_value->z;

	disable_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD, NULL, 0);

	shub_infof("x = %d, y = %d, z = %d\n", sensor_buf[0], sensor_buf[1], sensor_buf[2]);

	return sprintf(buf, "%s,%d,%d,%d\n", (bSuccess ? "OK" : "NG"), sensor_buf[0], sensor_buf[1], sensor_buf[2]);
}

static DEVICE_ATTR(raw_data, 0664, raw_data_show, raw_data_store);
static DEVICE_ATTR_RO(adc);
static DEVICE_ATTR_RO(dac);
static DEVICE_ATTR_RO(status);
static DEVICE_ATTR_RO(logging_data);

__visible_for_testing struct device_attribute *mag_attrs[] = {
	&dev_attr_adc,
	&dev_attr_dac,
	&dev_attr_raw_data,
	&dev_attr_status,
	&dev_attr_logging_data,
	NULL,
};

typedef struct device_attribute **(*get_chipset_dev_attrs)(char *);
get_chipset_dev_attrs get_mag_chipset_dev_attrs[] = {
	get_magnetometer_ak09918c_dev_attrs,
	get_magnetometer_mmc5633_dev_attrs,
	get_magnetometer_yas539_dev_attrs,
	get_magnetometer_mxg4300s_dev_attrs,
};

void initialize_magnetometer_sysfs(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);
	int ret;
	uint64_t i;

	ret = sensor_device_create(&mag_sysfs_device, NULL, "magnetic_sensor");
	if (ret < 0) {
		shub_errf("fail to creat %s sysfs device", sensor->name);
		return;
	}

	ret = add_sensor_device_attr(mag_sysfs_device, mag_attrs);
	if (ret < 0) {
		shub_errf("fail to add %s sysfs device attr", sensor->name);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(get_mag_chipset_dev_attrs); i++) {
		chipset_attrs = get_mag_chipset_dev_attrs[i](sensor->spec.name);
		if (chipset_attrs) {
			ret = add_sensor_device_attr(mag_sysfs_device, chipset_attrs);
			chipset_index = i;
			if (ret < 0) {
				shub_errf("fail to add sysfs chipset device attr(%d)", (int)i);
				return;
			}
			break;
		}
	}
}

void remove_magnetometer_sysfs(void)
{
	if (chipset_attrs)
		remove_sensor_device_attr(mag_sysfs_device, chipset_attrs);
	remove_sensor_device_attr(mag_sysfs_device, mag_attrs);
	sensor_device_destroy(mag_sysfs_device);
	mag_sysfs_device = NULL;
}

void initialize_magnetometer_factory(bool en)
{
	if (!get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD))
		return;

	if (en)
		initialize_magnetometer_sysfs();
	else
		remove_magnetometer_sysfs();
}
