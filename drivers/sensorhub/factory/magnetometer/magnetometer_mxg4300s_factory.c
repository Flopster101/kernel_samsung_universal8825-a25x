/*
 *  Copyright (C) 2022, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#include "../../comm/shub_comm.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_utility.h"
#include "../../sensor/magnetometer.h"
#include "magnetometer_factory.h"

#include <linux/slab.h>
#include <linux/delay.h>

#define MXG4300S_NAME	"MXG4300S"
#define MXG4300S_VENDOR "Haechitech"

#define GM_HEACHITECH_DATA_SPEC_MIN -16666
#define GM_HEACHITECH_DATA_SPEC_MAX 16666

#define GM_DATA_SUM_SPEC 26666

#define GM_SELFTEST_X_SPEC_MIN -200
#define GM_SELFTEST_X_SPEC_MAX 200
#define GM_SELFTEST_Y_SPEC_MIN -200
#define GM_SELFTEST_Y_SPEC_MAX 200
#define GM_SELFTEST_Z_SPEC_MIN -1000
#define GM_SELFTEST_Z_SPEC_MAX -150

int check_mxg4300s_adc_data_spec(s32 sensor_value[3])
{
	if ((sensor_value[0] == 0) && (sensor_value[1] == 0) && (sensor_value[2] == 0)) {
		return -1;
	} else if ((sensor_value[0] >= GM_HEACHITECH_DATA_SPEC_MAX) || (sensor_value[0] <= GM_HEACHITECH_DATA_SPEC_MIN) ||
		   (sensor_value[1] >= GM_HEACHITECH_DATA_SPEC_MAX) || (sensor_value[1] <= GM_HEACHITECH_DATA_SPEC_MIN) ||
		   (sensor_value[2] >= GM_HEACHITECH_DATA_SPEC_MAX) || (sensor_value[2] <= GM_HEACHITECH_DATA_SPEC_MIN)) {
		return -1;
	} else if ((int)abs(sensor_value[0]) + (int)abs(sensor_value[1])
		   + (int)abs(sensor_value[2]) >= GM_DATA_SUM_SPEC) {
		return -1;
	} else {
		return 0;
	}
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MXG4300S_NAME);
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MXG4300S_VENDOR);
}

static ssize_t matrix_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	return sprintf(buf, "%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
		       data->mag_matrix[0], data->mag_matrix[1], data->mag_matrix[2], data->mag_matrix[3],
		       data->mag_matrix[4], data->mag_matrix[5], data->mag_matrix[6], data->mag_matrix[7],
		       data->mag_matrix[8], data->mag_matrix[9], data->mag_matrix[10], data->mag_matrix[11],
		       data->mag_matrix[12], data->mag_matrix[13], data->mag_matrix[14], data->mag_matrix[15],
		       data->mag_matrix[16], data->mag_matrix[17], data->mag_matrix[18], data->mag_matrix[19],
		       data->mag_matrix[20], data->mag_matrix[21], data->mag_matrix[22], data->mag_matrix[23],
		       data->mag_matrix[24], data->mag_matrix[25], data->mag_matrix[26]);
}

static ssize_t matrix_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	u8 val[27] = {0, };
	int ret = 0;
	int i;
	char *token;
	char *str;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	str = (char *)buf;

	for (i = 0; i < 27; i++) {
		token = strsep(&str, "\n ");
		if (token == NULL) {
			shub_err("too few arguments (27 needed)");
			return -EINVAL;
		}

		ret = kstrtou8(token, 10, &val[i]);
		if (ret < 0) {
			shub_err("kstros8 error %d", ret);
			return ret;
		}
	}

	for (i = 0; i < 27; i++)
		data->mag_matrix[i] = val[i];

	shub_info("%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
		  data->mag_matrix[0], data->mag_matrix[1], data->mag_matrix[2], data->mag_matrix[3],
		  data->mag_matrix[4], data->mag_matrix[5], data->mag_matrix[6], data->mag_matrix[7],
		  data->mag_matrix[8], data->mag_matrix[9], data->mag_matrix[10], data->mag_matrix[11],
		  data->mag_matrix[12], data->mag_matrix[13], data->mag_matrix[14], data->mag_matrix[15],
		  data->mag_matrix[16], data->mag_matrix[17], data->mag_matrix[18], data->mag_matrix[19],
		  data->mag_matrix[20], data->mag_matrix[21], data->mag_matrix[22], data->mag_matrix[23],
		  data->mag_matrix[24], data->mag_matrix[25], data->mag_matrix[26]);
	set_mag_matrix(data);

	return size;
}

static ssize_t cover_matrix_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	return sprintf(buf, "%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
		       data->cover_matrix[0], data->cover_matrix[1], data->cover_matrix[2], data->cover_matrix[3],
		       data->cover_matrix[4], data->cover_matrix[5], data->cover_matrix[6], data->cover_matrix[7],
		       data->cover_matrix[8], data->cover_matrix[9], data->cover_matrix[10], data->cover_matrix[11],
		       data->cover_matrix[12], data->cover_matrix[13], data->cover_matrix[14], data->cover_matrix[15],
		       data->cover_matrix[16], data->cover_matrix[17], data->cover_matrix[18], data->cover_matrix[19],
		       data->cover_matrix[20], data->cover_matrix[21], data->cover_matrix[22], data->cover_matrix[23],
		       data->cover_matrix[24], data->cover_matrix[25], data->cover_matrix[26]);
}

static ssize_t cover_matrix_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	u8 val[27] = {0, };
	int ret = 0;
	int i;
	char *token;
	char *str;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	str = (char *)buf;

	for (i = 0; i < 27; i++) {
		token = strsep(&str, "\n ");
		if (token == NULL) {
			shub_err("too few arguments (27 needed)");
			return -EINVAL;
		}

		ret = kstrtou8(token, 10, &val[i]);
		if (ret < 0) {
			shub_err("kstros8 error %d", ret);
			return ret;
		}
	}

	for (i = 0; i < 27; i++)
		data->cover_matrix[i] = val[i];

	shub_info("%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
		  data->cover_matrix[0], data->cover_matrix[1], data->cover_matrix[2], data->cover_matrix[3],
		  data->cover_matrix[4], data->cover_matrix[5], data->cover_matrix[6], data->cover_matrix[7],
		  data->cover_matrix[8], data->cover_matrix[9], data->cover_matrix[10], data->cover_matrix[11],
		  data->cover_matrix[12], data->cover_matrix[13], data->cover_matrix[14], data->cover_matrix[15],
		  data->cover_matrix[16], data->cover_matrix[17], data->cover_matrix[18], data->cover_matrix[19],
		  data->cover_matrix[20], data->cover_matrix[21], data->cover_matrix[22], data->cover_matrix[23],
		  data->cover_matrix[24], data->cover_matrix[25], data->cover_matrix[26]);
	set_mag_cover_matrix(data);

	return size;
}


static ssize_t selftest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	s8 result[4] = {-1, -1, -1, -1};
	char *buf_selftest = NULL;
	int buf_selftest_length = 0;
	s16 iSF_X = 0, iSF_Y = 0, iSF_Z = 0;
	s16 iADC_X = 0, iADC_Y = 0, iADC_Z = 0;
	int ret = 0;
	int spec_out_retries = 0;

	shub_infof("");

	//Set 0 because fit the interface for factory test
	result[0] = 0;

Retry_selftest:
	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, SENSOR_FACTORY, 1000, NULL, 0,
				     &buf_selftest, &buf_selftest_length, true);
	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		goto exit;
	}

	if (buf_selftest_length < 13) {
		shub_errf("buffer length error %d", buf_selftest_length);
		goto exit;
	}

	/* read 6bytes data registers */
	iSF_X = (s16)((buf_selftest[1] << 8) + buf_selftest[2]);
	iSF_Y = (s16)((buf_selftest[3] << 8) + buf_selftest[4]);
	iSF_Z = (s16)((buf_selftest[5] << 8) + buf_selftest[6]);

	shub_info("self test x = %d, y = %d, z = %d", iSF_X, iSF_Y, iSF_Z);

	if ((iSF_X >= GM_SELFTEST_X_SPEC_MIN) && (iSF_X <= GM_SELFTEST_X_SPEC_MAX))
		shub_info("x passed self test, expect -200<=x<=200");
	else
		shub_info("x failed self test, expect -200<=x<=200");

	if ((iSF_Y >= GM_SELFTEST_Y_SPEC_MIN) && (iSF_Y <= GM_SELFTEST_Y_SPEC_MAX))
		shub_info("y passed self test, expect -200<=y<=200");
	else
		shub_info("y failed self test, expect -200<=y<=200");

	if ((iSF_Z >= GM_SELFTEST_Z_SPEC_MIN) && (iSF_Z <= GM_SELFTEST_Z_SPEC_MAX))
		shub_info("z passed self test, expect -1000<=z<=-150");
	else
		shub_info("z failed self test, expect -1000<=z<=-150");

	/* SELFTEST */
	if ((iSF_X >= GM_SELFTEST_X_SPEC_MIN) && (iSF_X <= GM_SELFTEST_X_SPEC_MAX) &&
	    (iSF_Y >= GM_SELFTEST_Y_SPEC_MIN) && (iSF_Y <= GM_SELFTEST_Y_SPEC_MAX) &&
	    (iSF_Z >= GM_SELFTEST_Z_SPEC_MIN) && (iSF_Z <= GM_SELFTEST_Z_SPEC_MAX)) {
		result[1] = 0;
	}

	if ((result[1] == -1) && (spec_out_retries++ < 5)) {
		shub_info("selftest spec out. Retry = %d", spec_out_retries);
		goto Retry_selftest;
	}

	//Set 0 because fit the interface for factory test
	result[2] = 0;

	spec_out_retries = 10;

	/* ADC */
	iADC_X = (s16)((buf_selftest[7] << 8) + buf_selftest[8]);
	iADC_Y = (s16)((buf_selftest[9] << 8) + buf_selftest[10]);
	iADC_Z = (s16)((buf_selftest[11] << 8) + buf_selftest[12]);

	if ((iADC_X == 0) && (iADC_Y == 0) && (iADC_Z == 0)) {
		result[3] = -1;
	} else if ((iADC_X >= GM_HEACHITECH_DATA_SPEC_MAX) || (iADC_X <= GM_HEACHITECH_DATA_SPEC_MIN) ||
		   (iADC_Y >= GM_HEACHITECH_DATA_SPEC_MAX) || (iADC_Y <= GM_HEACHITECH_DATA_SPEC_MIN) ||
		   (iADC_Z >= GM_HEACHITECH_DATA_SPEC_MAX) || (iADC_Z <= GM_HEACHITECH_DATA_SPEC_MIN)) {
		result[3] = -1;
	} else if ((int)abs(iADC_X) + (int)abs(iADC_Y) + (int)abs(iADC_Z) >= GM_DATA_SUM_SPEC) {
		result[3] = -1;
	} else {
		result[3] = 0;
	}

	shub_info("adc, x = %d, y = %d, z = %d, retry = %d", iADC_X, iADC_Y, iADC_Z, spec_out_retries);

exit:
	shub_info("out. Result = %d %d %d %d\n", result[0], result[1], result[2], result[3]);

	ret = sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", result[0], result[1], iSF_X, iSF_Y, iSF_Z, result[2],
		      result[3], iADC_X, iADC_Y, iADC_Z);

	kfree(buf_selftest);

	return ret;
}

static ssize_t ak09911_asa_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0,0,0\n");
}

static ssize_t hw_offset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;
	struct calibration_data_mxg4300s *cal_data = data->cal_data;

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", cal_data->offset_x, cal_data->offset_y, cal_data->offset_z);
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR_RO(selftest);
static DEVICE_ATTR_RO(ak09911_asa);
static DEVICE_ATTR_RO(hw_offset);
static DEVICE_ATTR(matrix, 0664, matrix_show, matrix_store);
static DEVICE_ATTR(matrix2, 0664, cover_matrix_show, cover_matrix_store);

static struct device_attribute *mag_mxg4300s_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_selftest,
	&dev_attr_ak09911_asa,
	&dev_attr_matrix,
	&dev_attr_hw_offset,
	NULL,
	NULL,
};

struct device_attribute **get_magnetometer_mxg4300s_dev_attrs(char *name)
{
	int index = 0;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	if (strcmp(name, MXG4300S_NAME) != 0)
		return NULL;

	if (data->cover_matrix) {
		while (mag_mxg4300s_attrs[index] != NULL)
			index++;
		mag_mxg4300s_attrs[index] = &dev_attr_matrix2;
	}

	return mag_mxg4300s_attrs;
}
