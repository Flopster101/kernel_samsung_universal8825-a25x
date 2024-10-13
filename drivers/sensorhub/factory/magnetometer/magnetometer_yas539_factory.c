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

#include "../../comm/shub_comm.h"
#include "../../sensor/magnetometer.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_utility.h"
#include "magnetometer_factory.h"

#include <linux/delay.h>
#include <linux/slab.h>

#define YAS539_NAME   "YAS539"
#define YAS539_VENDOR "YAMAHA"

#define GM_DATA_SPEC_MIN -6500
#define GM_DATA_SPEC_MAX 6500

int check_yas539_adc_data_spec(s32 sensor_value[3])
{
	if ((sensor_value[0] == 0) && (sensor_value[1] == 0) && (sensor_value[2] == 0)) {
		return -1;
	} else if ((sensor_value[0] > GM_DATA_SPEC_MAX) || (sensor_value[0] < GM_DATA_SPEC_MIN)
		   || (sensor_value[1] > GM_DATA_SPEC_MAX) || (sensor_value[1] < GM_DATA_SPEC_MIN)
		   || (sensor_value[2] > GM_DATA_SPEC_MAX) || (sensor_value[2] < GM_DATA_SPEC_MIN)) {
		return -1;
	} else {
		return 0;
	}
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", YAS539_NAME);
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", YAS539_VENDOR);
}

static ssize_t matrix_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	return sprintf(buf, "%d %d %d %d %d %d %d %d %d\n",
		      *(s16 *)&data->mag_matrix[0], *(s16 *)&data->mag_matrix[2], *(s16 *)&data->mag_matrix[4],
		      *(s16 *)&data->mag_matrix[6], *(s16 *)&data->mag_matrix[8], *(s16 *)&data->mag_matrix[10],
		      *(s16 *)&data->mag_matrix[12], *(s16 *)&data->mag_matrix[14], *(s16 *)&data->mag_matrix[16]);
}

static ssize_t matrix_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	s16 val[9] = {0, };
	int ret = 0;
	int i;
	char *token;
	char *str;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	str = (char *)buf;

	for (i = 0; i < 9; i++) {
		token = strsep(&str, "\n ");
		if (token == NULL) {
			shub_err("too few arguments (%d needed)", 9);
			return -EINVAL;
		}

		ret = kstrtos16(token, 10, &val[i]);
		if (ret < 0) {
			shub_err("kstrtos16 error %d", ret);
			return ret;
		}
	}

	memcpy(data->mag_matrix, val, data->mag_matrix_len);

	shub_info("%d %d %d %d %d %d %d %d %d",
		  *(s16 *)&data->mag_matrix[0], *(s16 *)&data->mag_matrix[2], *(s16 *)&data->mag_matrix[4],
		  *(s16 *)&data->mag_matrix[6], *(s16 *)&data->mag_matrix[8], *(s16 *)&data->mag_matrix[10],
		  *(s16 *)&data->mag_matrix[12], *(s16 *)&data->mag_matrix[14], *(s16 *)&data->mag_matrix[16]);

	set_mag_matrix(data);

	return size;
}

static ssize_t selftest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *buf_selftest = NULL;
	int buf_selftest_length = 0;
	int ret = 0;
	s8 id = 0, x = 0, y1 = 0, y2 = 0, dir = 0;
	s16 sx = 0, sy1 = 0, sy2 = 0, ohx = 0, ohy = 0, ohz = 0;
	s8 err[7] = {-1, };

	shub_infof("");

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, SENSOR_FACTORY, 1000, NULL, 0,
				     &buf_selftest, &buf_selftest_length, true);

	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		goto exit;
	}

	if (buf_selftest_length < 24) {
		shub_err("buffer length error %d", buf_selftest_length);
		goto exit;
	}

	id = (s8)(buf_selftest[0]);
	err[0] = (s8)(buf_selftest[1]);
	err[1] = (s8)(buf_selftest[2]);
	err[2] = (s8)(buf_selftest[3]);
	x = (s8)(buf_selftest[4]);
	y1 = (s8)(buf_selftest[5]);
	y2 = (s8)(buf_selftest[6]);
	err[3] = (s8)(buf_selftest[7]);
	dir = (s8)(buf_selftest[8]);
	err[4] = (s8)(buf_selftest[9]);
	ohx = (s16)((buf_selftest[10] << 8) + buf_selftest[11]);
	ohy = (s16)((buf_selftest[12] << 8) + buf_selftest[13]);
	ohz = (s16)((buf_selftest[14] << 8) + buf_selftest[15]);
	err[6] = (s8)(buf_selftest[16]);
	sx = (s16)((buf_selftest[17] << 8) + buf_selftest[18]);
	sy1 = (s16)((buf_selftest[19] << 8) + buf_selftest[20]);
	sy2 = (s16)((buf_selftest[21] << 8) + buf_selftest[22]);
	err[5] = (s8)(buf_selftest[23]);

	if (unlikely(id != 0x8))
		err[0] = -1;

	if (unlikely(x < -30 || x > 30))
		err[3] = -1;

	if (unlikely(y1 < -30 || y1 > 30))
		err[3] = -1;

	if (unlikely(y2 < -30 || y2 > 30))
		err[3] = -1;

	if (unlikely(sx < 16544 || sx > 17024))
		err[5] = -1;

	if (unlikely(sy1 < 16517 || sy1 > 17184))
		err[5] = -1;

	if (unlikely(sy2 < 15584 || sy2 > 16251))
		err[5] = -1;

	if (unlikely(ohx < -1000 || ohx > 1000))
		err[6] = -1;

	if (unlikely(ohy < -1000 || ohy > 1000))
		err[6] = -1;

	if (unlikely(ohz < -1000 || ohz > 1000))
		err[6] = -1;


	shub_info("[SHUB] Test1 - err = %d, id = %d\n"
		  "[SHUB] Test3 - err = %d\n"
		  "[SHUB] Test4 - err = %d, offset = %d,%d,%d\n"
		  "[SHUB] Test5 - err = %d, direction = %d\n"
		  "[SHUB] Test6 - err = %d, sensitivity = %d,%d,%d\n"
		  "[SHUB] Test7 - err = %d, offset = %d,%d,%d\n"
		  "[SHUB] Test2 - err = %d\n",
		  err[0], id, err[2], err[3], x, y1, y2, err[4], dir, err[5], sx, sy1, sy2, err[6], ohx, ohy, ohz,
		  err[1]);

exit:
	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", err[0], id, err[2], err[3], x,
		       y1, y2, err[4], dir, err[5], sx, sy1, sy2, err[6], ohx, ohy, ohz, err[1]);
}

static ssize_t hw_offset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;
	struct calibration_data_yas539 *cal_data = data->cal_data;

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", cal_data->offset_x, cal_data->offset_y, cal_data->offset_z);
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR_RO(selftest);
static DEVICE_ATTR_RO(hw_offset);
static DEVICE_ATTR(matrix, 0664, matrix_show, matrix_store);

static struct device_attribute *mag_yas539_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_selftest,
	&dev_attr_matrix,
	&dev_attr_hw_offset,
	NULL,
};

struct device_attribute **get_magnetometer_yas539_dev_attrs(char *name)
{
	if (strcmp(name, YAS539_NAME) != 0)
		return NULL;

	return mag_yas539_attrs;
}
