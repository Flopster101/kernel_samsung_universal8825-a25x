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
#include "../../utility/shub_utility.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../sensor/gyroscope.h"

#include <linux/slab.h>

#define LSM6DSOTR_NAME   "LSM6DSOTR"
#define LSM6DSOTR_VENDOR "STM"

#define DEF_GYRO_SENS_STM            (700) /* 0.0700 * 10000 */
#define DEF_BIAS_LSB_THRESH_SELF_STM (40000 / DEF_GYRO_SENS_STM)

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", LSM6DSOTR_NAME);
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", LSM6DSOTR_VENDOR);
}

static u32 lsm6dsotr_selftest_sqrt(u32 sqsum)
{
	u32 sq_rt;
	u32 g0, g1, g2, g3, g4;
	u32 seed;
	u32 next;
	u32 step;

	g4 = sqsum / 100000000;
	g3 = (sqsum - g4 * 100000000) / 1000000;
	g2 = (sqsum - g4 * 100000000 - g3 * 1000000) / 10000;
	g1 = (sqsum - g4 * 100000000 - g3 * 1000000 - g2 * 10000) / 100;
	g0 = (sqsum - g4 * 100000000 - g3 * 1000000 - g2 * 10000 - g1 * 100);

	next = g4;
	step = 0;
	seed = 0;
	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = seed * 10000;
	next = (next - (seed * step)) * 100 + g3;

	step = 0;
	seed = 2 * seed * 10;
	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = sq_rt + step * 1000;
	next = (next - seed * step) * 100 + g2;
	seed = (seed + step) * 10;
	step = 0;
	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = sq_rt + step * 100;
	next = (next - seed * step) * 100 + g1;
	seed = (seed + step) * 10;
	step = 0;

	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = sq_rt + step * 10;
	next = (next - seed * step) * 100 + g0;
	seed = (seed + step) * 10;
	step = 0;

	while (((seed + 1) * (step + 1)) <= next) {
		step++;
		seed++;
	}

	sq_rt = sq_rt + step;

	return sq_rt;
}
static ssize_t selftest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *temp_buf = NULL;
	int temp_buf_length = 0;
	u8 initialized = 0;
	s8 hw_result = 0;
	int i = 0, j = 0, total_count = 0, ret_val = 0, gyro_lib_dl_fail = 0;
	long avg[3] = {0, }, rms[3] = {0, };
	int gyro_bias[3] = {0, }, gyro_rms[3] = {0, };
	s16 shift_ratio[3] = {0, }; /* self_diff value */
	s16 cal_data[3] = {0, };
	char a_name[3][2] = {"X", "Y", "Z"};
	int ret = 0;
	int dps_rms[3] = {0, };
	u32 temp = 0;
	int bias_thresh = DEF_BIAS_LSB_THRESH_SELF_STM;
	int fifo_ret = 0;
	int cal_ret = 0;
	s16 st_zro[3] = {0, };
	s16 st_bias[3] = {0, };
	int gyro_fifo_avg[3] = {0, }, gyro_self_zro[3] = {0, };
	int gyro_self_bias[3] = {0, }, gyro_self_diff[3] = {0, };
	struct gyroscope_data *data = get_sensor(SENSOR_TYPE_GYROSCOPE)->data;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_GYROSCOPE, SENSOR_FACTORY,
				     7000, NULL, 0, &temp_buf, &temp_buf_length, true);

	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		ret_val = 1;
		goto exit;
	}

	if (temp_buf_length < 36) {
		shub_errf("buffer length error %d", temp_buf_length);
		ret = -EINVAL;
		goto exit;
	}

	shub_errf("%d %d %d %d %d %d %d %d %d %d %d %d\n",
		 temp_buf[0], temp_buf[1], temp_buf[2], temp_buf[3], temp_buf[4], temp_buf[5],
		 temp_buf[6], temp_buf[7], temp_buf[8], temp_buf[9], temp_buf[10], temp_buf[11]);

	initialized = temp_buf[0];
	shift_ratio[0] = (s16)((temp_buf[2] << 8) + temp_buf[1]);
	shift_ratio[1] = (s16)((temp_buf[4] << 8) + temp_buf[3]);
	shift_ratio[2] = (s16)((temp_buf[6] << 8) + temp_buf[5]);
	hw_result = (s8)temp_buf[7];
	total_count = (int)((temp_buf[11] << 24) + (temp_buf[10] << 16) + (temp_buf[9] << 8) + temp_buf[8]);
	avg[0] = (long)((temp_buf[15] << 24) + (temp_buf[14] << 16) + (temp_buf[13] << 8) + temp_buf[12]);
	avg[1] = (long)((temp_buf[19] << 24) + (temp_buf[18] << 16) + (temp_buf[17] << 8) + temp_buf[16]);
	avg[2] = (long)((temp_buf[23] << 24) + (temp_buf[22] << 16) + (temp_buf[21] << 8) + temp_buf[20]);
	rms[0] = (long)((temp_buf[27] << 24) + (temp_buf[26] << 16) + (temp_buf[25] << 8) + temp_buf[24]);
	rms[1] = (long)((temp_buf[31] << 24) + (temp_buf[30] << 16) + (temp_buf[29] << 8) + temp_buf[28]);
	rms[2] = (long)((temp_buf[35] << 24) + (temp_buf[34] << 16) + (temp_buf[33] << 8) + temp_buf[32]);

	st_zro[0] = (s16)((temp_buf[25] << 8) + temp_buf[24]);
	st_zro[1] = (s16)((temp_buf[27] << 8) + temp_buf[26]);
	st_zro[2] = (s16)((temp_buf[29] << 8) + temp_buf[28]);

	st_bias[0] = (s16)((temp_buf[31] << 8) + temp_buf[30]);
	st_bias[1] = (s16)((temp_buf[33] << 8) + temp_buf[32]);
	st_bias[2] = (s16)((temp_buf[35] << 8) + temp_buf[34]);

	shub_infof("init: %d, total cnt: %d\n", initialized, total_count);
	shub_infof("hw_result: %d, %d, %d, %d\n", hw_result, shift_ratio[0], shift_ratio[1], shift_ratio[2]);
	shub_infof("avg %+8ld %+8ld %+8ld (LSB)\n", avg[0], avg[1], avg[2]);
	shub_infof("rms %+8ld %+8ld %+8ld (LSB)\n", rms[0], rms[1], rms[2]);

	/* FIFO ZRO check pass / fail */
	gyro_fifo_avg[0] = avg[0] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_fifo_avg[1] = avg[1] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_fifo_avg[2] = avg[2] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	/* ZRO self test */
	gyro_self_zro[0] = st_zro[0] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_zro[1] = st_zro[1] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_zro[2] = st_zro[2] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	/* bias */
	gyro_self_bias[0] = st_bias[0] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_bias[1] = st_bias[1] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_bias[2] = st_bias[2] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	/* diff = bias - ZRO */
	gyro_self_diff[0] = shift_ratio[0] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_diff[1] = shift_ratio[1] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;
	gyro_self_diff[2] = shift_ratio[2] * DEF_GYRO_SENS_STM / DEF_SCALE_FOR_FLOAT;

	if (total_count != 128) {
		shub_errf("total_count is not 128. goto exit\n");
		ret_val = 2;
		goto exit;
	} else {
		cal_ret = fifo_ret = 1;
	}

	if (hw_result < 0) {
		shub_errf("hw selftest fail(%d), sw selftest skip\n", hw_result);
		if (shift_ratio[0] == GYRO_LIB_DL_FAIL && shift_ratio[1] == GYRO_LIB_DL_FAIL &&
		    shift_ratio[2] == GYRO_LIB_DL_FAIL) {
			shub_errf("gyro lib download fail\n");
			gyro_lib_dl_fail = 1;
		} else {
			/*
			shub_dbg("%d,%d,%d fail.\n",
				 shift_ratio[0] / 10, shift_ratio[1] / 10, shift_ratio[2] / 10);
			return sprintf(buf, "%d,%d,%d\n",
				       shift_ratio[0] / 10, shift_ratio[1] / 10, shift_ratio[2] / 10);
			*/
			shub_dbg("%d,%d,%d fail.\n", gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2]);
			return sprintf(buf, "%d,%d,%d\n", gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2]);
		}
	}

	/* AVG value range test +/- 40 */
	if ((ABS(gyro_fifo_avg[0]) > 40) || (ABS(gyro_fifo_avg[1]) > 40) || (ABS(gyro_fifo_avg[2]) > 40)) {
		shub_dbg("%d,%d,%d fail.\n", gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2]);
		return sprintf(buf, "%d,%d,%d\n", gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2]);
	}

	/* STMICRO */
	gyro_bias[0] = avg[0] * DEF_GYRO_SENS_STM;
	gyro_bias[1] = avg[1] * DEF_GYRO_SENS_STM;
	gyro_bias[2] = avg[2] * DEF_GYRO_SENS_STM;
	cal_data[0] = (s16)avg[0];
	cal_data[1] = (s16)avg[1];
	cal_data[2] = (s16)avg[2];

	if (VERBOSE_OUT) {
		shub_infof("abs bias : %+8d.%03d %+8d.%03d %+8d.%03d (dps)\n",
			  (int)abs(gyro_bias[0]) / DEF_SCALE_FOR_FLOAT, (int)abs(gyro_bias[0]) % DEF_SCALE_FOR_FLOAT,
			  (int)abs(gyro_bias[1]) / DEF_SCALE_FOR_FLOAT, (int)abs(gyro_bias[1]) % DEF_SCALE_FOR_FLOAT,
			  (int)abs(gyro_bias[2]) / DEF_SCALE_FOR_FLOAT, (int)abs(gyro_bias[2]) % DEF_SCALE_FOR_FLOAT);
	}

	for (j = 0; j < 3; j++) {
		if (unlikely(abs(avg[j]) > bias_thresh)) {
			shub_errf("Gyro bias %s(%ld) exceeded threshold (threshold = %d LSB)\n",
				  a_name[j], avg[j], bias_thresh);
			ret_val |= 1 << (3 + j);
		}
	}

	/* STMICRO
	 * 3rd, check RMS for dead gyros
	 *  If any of the RMS noise value returns zero,
	 *  then we might have dead gyro or FIFO/register failure,
	 *  the part is sleeping, or the part is not responsive
	 */


	/*
	 * if (rms[0] == 0 || rms[1] == 0 || rms[2] == 0)
	 *	ret_val |= 1 << 6;
	 */

	if (VERBOSE_OUT) {
		shub_infof("RMS ^ 2 : %+8ld %+8ld %+8ld\n",
			  (long)rms[0] / total_count, (long)rms[1] / total_count, (long)rms[2] / total_count);
	}

	for (i = 0; i < 3; i++) {
		if (rms[i] > 10000)
			temp = ((u32)(rms[i] / total_count)) * DEF_RMS_SCALE_FOR_RMS;
		else
			temp = ((u32)(rms[i] * DEF_RMS_SCALE_FOR_RMS)) / total_count;

		if (rms[i] < 0)
			temp = 1 << 31;

		dps_rms[i] = lsm6dsotr_selftest_sqrt(temp) / DEF_GYRO_SENS_STM;

		gyro_rms[i] = dps_rms[i] * DEF_SCALE_FOR_FLOAT / DEF_SQRT_SCALE_FOR_RMS;
	}

	shub_infof("RMS : %+8d.%03d %+8d.%03d  %+8d.%03d (dps)\n",
		  (int)abs(gyro_rms[0]) / DEF_SCALE_FOR_FLOAT, (int)abs(gyro_rms[0]) % DEF_SCALE_FOR_FLOAT,
		  (int)abs(gyro_rms[1]) / DEF_SCALE_FOR_FLOAT, (int)abs(gyro_rms[1]) % DEF_SCALE_FOR_FLOAT,
		  (int)abs(gyro_rms[2]) / DEF_SCALE_FOR_FLOAT, (int)abs(gyro_rms[2]) % DEF_SCALE_FOR_FLOAT);

	if (gyro_lib_dl_fail) {
		shub_errf("gyro_lib_dl_fail, Don't save cal data\n");
		ret_val = -1;
		goto exit;
	}

	if (likely(!ret_val)) {
		save_gyro_calibration_file(cal_data);
	} else {
		shub_errf("ret_val != 0, gyrocal is 0 at all axis\n");
		data->cal_data.x = 0;
		data->cal_data.y = 0;
		data->cal_data.z = 0;
	}
exit:
	shub_dbg("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		 gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2], gyro_self_zro[0], gyro_self_zro[1],
		 gyro_self_zro[2], gyro_self_bias[0], gyro_self_bias[1], gyro_self_bias[2], gyro_self_diff[0],
		 gyro_self_diff[1], gyro_self_diff[2], fifo_ret, cal_ret, hw_result);

	/* Gyro Calibration pass / fail, buffer 1~6 values. */
	if ((fifo_ret == 0) || (cal_ret == 0)) {
		ret = sprintf(buf, "%d,%d,%d\n", gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2]);
		kfree(temp_buf);
		return ret;
	}

	fifo_ret = hw_result;

	ret = sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		      gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2], gyro_self_zro[0], gyro_self_zro[1],
		      gyro_self_zro[2], gyro_self_bias[0], gyro_self_bias[1], gyro_self_bias[2], gyro_self_diff[0],
		      gyro_self_diff[1], gyro_self_diff[2], fifo_ret, cal_ret);

	kfree(temp_buf);

	return ret;
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR_RO(selftest);

static struct device_attribute *gyro_lsm6dsotr_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_selftest,
	NULL,
};

struct device_attribute **get_gyroscope_lsm6dsotr_dev_attrs(char *name)
{
	if (strcmp(name, LSM6DSOTR_NAME) != 0)
		return NULL;

	return gyro_lsm6dsotr_attrs;
}
