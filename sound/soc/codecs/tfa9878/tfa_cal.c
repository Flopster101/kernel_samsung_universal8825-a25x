/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "inc/tfa_sysfs.h"

#define TFA_CAL_DEV_NAME	"tfa_cal"
#define FILESIZE_CAL	(10)

/* ---------------------------------------------------------------------- */

static ssize_t rdc_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t rdc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(rdc);

static ssize_t temp_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t temp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(temp);

#if defined(TFA_STEREO_NODE)
static ssize_t rdc_r_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t rdc_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(rdc_r);

static ssize_t temp_r_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t temp_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(temp_r);
#endif /* TFA_STEREO_NODE */

static ssize_t status_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(status);

static struct attribute *tfa_cal_attr[] = {
	&dev_attr_rdc.attr,
	&dev_attr_temp.attr,
#if defined(TFA_STEREO_NODE)
	&dev_attr_rdc_r.attr,
	&dev_attr_temp_r.attr,
#endif /* TFA_STEREO_NODE */
	&dev_attr_status.attr,
	NULL,
};

static struct attribute_group tfa_cal_attr_grp = {
	.attrs = tfa_cal_attr,
};

struct tfa_cal {
	int rdc;
	int temp;
};

/* ---------------------------------------------------------------------- */

static struct device *tfa_cal_dev;
static int cur_status;
static struct tfa_cal cal_data[MAX_HANDLES];

/* ---------------------------------------------------------------------- */

static ssize_t update_rdc_status(int idx, char *buf)
{
	struct tfa_device *tfa = NULL;
	int ret = 0;
	uint16_t value;
	int size;
	char cal_result[FILESIZE_CAL] = {0};

	tfa = tfa98xx_get_tfa_device_from_index(0);
	if (tfa == NULL)
		return -EINVAL; /* unused device */

	if (idx < 0 || idx >= tfa->dev_count)
		return -EINVAL;

	if (cal_data[idx].rdc == 0
		|| cal_data[idx].rdc == 0xffff) {
		ret = tfa_get_cal_data(idx, &value);
		if (ret == TFA98XX_ERROR_NOT_OPEN)
			value = 0xffff; /* unused device */
		if (ret) {
			pr_info("%s: tfa_cal failed to read data from amplifier\n",
				__func__);
			value = 0;
		}
		if (value == 0xffff)
			pr_info("%s: tfa_cal read wrong data from amplifier\n",
				__func__);
		cal_data[idx].rdc = value;
	}

	snprintf(cal_result, FILESIZE_CAL,
		"%d", cal_data[idx].rdc);

	if (cal_result[0] == 0)
		size = snprintf(buf, 7 + 1, "no_data");
	else
		size = snprintf(buf, strlen(cal_result) + 1,
			"%s", cal_result);

	if (size <= 0) {
		pr_err("%s: tfa_cal failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t update_temp_status(int idx, char *buf)
{
	struct tfa_device *tfa = NULL;
	int ret = 0;
	uint16_t value;
	int size;
	char cal_result[FILESIZE_CAL] = {0};

	tfa = tfa98xx_get_tfa_device_from_index(0);
	if (tfa == NULL)
		return -EINVAL; /* unused device */

	if (idx < 0 || idx >= tfa->dev_count)
		return -EINVAL;

	if (cal_data[idx].temp == 0
		|| cal_data[idx].temp == 0xffff) {
		ret = tfa_get_cal_temp(idx, &value);
		if (ret == TFA98XX_ERROR_NOT_OPEN)
			value = 0xffff; /* unused device */
		if (ret) {
			pr_info("%s: tfa_cal failed to read temp from amplifier\n",
				__func__);
			value = 0;
		}
		if (value == 0xffff)
			pr_info("%s: tfa_cal read wrong temp from amplifier\n",
				__func__);
		cal_data[idx].temp = value;
	}

	snprintf(cal_result, FILESIZE_CAL,
		"%d", cal_data[idx].temp);

	if (cal_result[0] == 0)
		size = snprintf(buf, 7 + 1, "no_data");
	else
		size = snprintf(buf, strlen(cal_result) + 1,
			"%s", cal_result);

	if (size <= 0) {
		pr_err("%s: tfa_cal failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t rdc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_0;
	int ret;

	ret = update_rdc_status(idx, buf);
	if (ret > 0)
		pr_info("%s: tfa_cal - dev %d - calibration data (rdc %d)\n",
			__func__, idx, cal_data[idx].rdc);
	else
		pr_err("%s: tfa_cal dev %d - error %d\n",
			__func__, idx, ret);

	return ret;
}

static ssize_t rdc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write calibration data\n",
		__func__, INDEX_0);

	return size;
}

static ssize_t temp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_0;
	int ret;

	ret = update_temp_status(idx, buf);
	if (ret > 0)
		pr_info("%s: tfa_cal - dev %d - calibration data (temp %d)\n",
			__func__, idx, cal_data[idx].temp);
	else
		pr_err("%s: tfa_cal dev %d - error %d\n",
			__func__, idx, ret);

	return ret;
}

static ssize_t temp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write temperature in calibration\n",
		__func__, INDEX_0);

	return size;
}

#if defined(TFA_STEREO_NODE)
static ssize_t rdc_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_1;
	int ret;

	ret = update_rdc_status(idx, buf);
	if (ret > 0)
		pr_info("%s: tfa_cal - dev %d - calibration data (rdc %d)\n",
			__func__, idx, cal_data[idx].rdc);
	else
		pr_err("%s: tfa_cal dev %d - error %d\n",
			__func__, idx, ret);

	return ret;
}

static ssize_t rdc_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write calibration data\n",
		__func__, INDEX_1);

	return size;
}

static ssize_t temp_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_1;
	int ret;

	ret = update_temp_status(idx, buf);
	if (ret > 0)
		pr_info("%s: tfa_cal - dev %d - calibration data (temp %d)\n",
			__func__, idx, cal_data[idx].temp);
	else
		pr_err("%s: tfa_cal dev %d - error %d\n",
			__func__, idx, ret);

	return ret;
}

static ssize_t temp_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write temperature in calibration\n",
		__func__, INDEX_1);

	return size;
}
#endif /* TFA_STEREO_NODE */

static ssize_t status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int size;
	char cal_state[FILESIZE_CAL] = {0};

	snprintf(cal_state, FILESIZE_CAL,
		"%s", cur_status ?
		"enabled" /* calibration is active */
		: "disabled"); /* calibration is inactive */

	size = snprintf(buf, strlen(cal_state) + 1,
		"%s", cal_state);

	return size;
}

static ssize_t status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tfa_device *tfa = NULL;
	int idx, ndev = MAX_HANDLES;
	uint16_t value;
	int ret = 0, status;

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (!sysfs_streq(buf, "1") && !sysfs_streq(buf, "0")) {
		pr_info("%s: tfa_cal invalid value to start calibration\n",
			__func__);
		return -EINVAL;
	}

	ret = kstrtou32(buf, 10, &status);
	if (!status) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}
	if (cur_status)
		pr_info("%s: tfa_cal prior calibration still runs\n", __func__);

	pr_info("%s: tfa_cal begin\n", __func__);

	cur_status = status; /* run - changed to active */

	memset(cal_data, 0, sizeof(struct tfa_cal) * MAX_HANDLES);

	/* run calibration */
	ret = tfa_run_cal(0, &value);
	if (ret == TFA98XX_ERROR_NOT_OPEN)
		return -EINVAL; /* unused device */
	if (ret) {
		pr_info("%s: tfa_cal failed to calibrate speaker\n", __func__);
		return -EINVAL;
	}

	cur_status = 0; /* done - changed to inactive */

	tfa = tfa98xx_get_tfa_device_from_index(0);
	if (tfa == NULL)
		return -EINVAL; /* unused device */

	ndev = tfa->dev_count;
	if (ndev < 1)
		return -EINVAL;

	for (idx = 0; idx < ndev; idx++) {
		/* read data to store */
		ret = tfa_get_cal_data(idx, &value);
		if (ret) {
			pr_info("%s: tfa_cal failed to read data after calibration\n",
				__func__);
			continue;
		}

		if (value == 0xffff) {
			pr_info("%s: tfa_cal invalid data\n", __func__);
			return -EINVAL;
		}

		cal_data[idx].rdc = value;

		/* read temp to store */
		ret = tfa_get_cal_temp(idx, &value);
		if (ret) {
			pr_info("%s: tfa_cal failed to read temp after calibration\n",
				__func__);
			continue;
		}

		if (value == 0xffff) {
			pr_info("%s: tfa_cal invalid data\n", __func__);
			return -EINVAL;
		}

		cal_data[idx].temp = value;

		pr_info("%s: tfa_cal - dev %d - calibration data (%d, %d)\n",
			__func__, idx, cal_data[idx].rdc, cal_data[idx].temp);
	}

	pr_info("%s: tfa_cal end\n", __func__);

	return size;
}

int tfa98xx_cal_init(struct class *tfa_class)
{
	int ret = 0;

	if (tfa_class) {
		tfa_cal_dev = device_create(tfa_class,
			NULL, DEV_ID_TFA_CAL, NULL, TFA_CAL_DEV_NAME);
		if (!IS_ERR(tfa_cal_dev)) {
			ret = sysfs_create_group(&tfa_cal_dev->kobj,
				&tfa_cal_attr_grp);
			if (ret)
				pr_err("%s: failed to create sysfs group. ret (%d)\n",
					__func__, ret);
		}
	}

	pr_info("%s: initialized (%d)\n", __func__,
		(tfa_class != NULL) ? 1 : 0);

	return ret;
}

void tfa98xx_cal_exit(struct class *tfa_class)
{
	device_destroy(tfa_class, DEV_ID_TFA_CAL);
	pr_info("exited\n");
}

