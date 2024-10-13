/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020-2021 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "inc/tfa_sysfs.h"

#define TFA_VVAL_DEV_NAME	"tfa_vval"
#define FILESIZE_VVAL	(10)

/* ---------------------------------------------------------------------- */

static ssize_t validation_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t validation_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(validation);

static ssize_t status_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(status);

#if defined(TFA_STEREO_NODE)
static ssize_t status_r_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t status_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(status_r);
#endif /* TFA_STEREO_NODE */

static struct attribute *tfa_vval_attr[] = {
	&dev_attr_validation.attr,
	&dev_attr_status.attr,
#if defined(TFA_STEREO_NODE)
	&dev_attr_status_r.attr,
#endif /* TFA_STEREO_NODE */
	NULL,
};

static struct attribute_group tfa_vval_attr_grp = {
	.attrs = tfa_vval_attr,
};

/* ---------------------------------------------------------------------- */

static struct device *tfa_vval_dev;
static int cur_status;
static int vval_data[MAX_HANDLES] = {
	VVAL_UNTESTED, VVAL_UNTESTED, VVAL_UNTESTED, VVAL_UNTESTED
};

/* ---------------------------------------------------------------------- */

static char *get_vval_string(int vval_index)
{
	switch (vval_index) {
	case VVAL_UNTESTED:
		return "0";
	case VVAL_PASS:
		return "1";
	case VVAL_FAIL:
		return "-1";
	case VVAL_TEST_FAIL:
		return "0";
	default:
		return "0";
	}
}

static ssize_t update_vval_status(int idx, char *buf)
{
	struct tfa_device *tfa = NULL;
	int ret = 0;
	uint16_t value;
	int size;
	char vval_result[FILESIZE_VVAL] = {0};

	tfa = tfa98xx_get_tfa_device_from_index(0);
	if (tfa == NULL)
		return -EINVAL; /* unused device */

	if (idx < 0 || idx >= tfa->dev_count)
		return -EINVAL;

	if (vval_data[idx] != VVAL_PASS
		&& vval_data[idx] != VVAL_FAIL) {
		ret = tfa_get_vval_data(idx, &value);
		if (ret == TFA98XX_ERROR_NOT_OPEN)
			value = VVAL_UNTESTED; /* unused device */
		if (ret) {
			pr_info("%s: tfa_vval failed to read data from amplifier\n",
				__func__);
			value = VVAL_TEST_FAIL;
		}
		if (value == 0xffff) {
			pr_info("%s: tfa_vval read wrong data from amplifier\n",
				__func__);
			value = VVAL_UNTESTED;
		}
		vval_data[idx] = value;
	}

	snprintf(vval_result, FILESIZE_VVAL,
		"%s", get_vval_string(vval_data[idx]));

	if (vval_result[0] == 0)
		size = snprintf(buf, 1 + 1, "0"); /* not tested */
	else
		size = snprintf(buf, strlen(vval_result) + 1,
			"%s", vval_result);

	if (size <= 0) {
		pr_err("%s: tfa_vval failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_0;
	int ret;

	ret = update_vval_status(idx, buf);
	if (ret > 0)
		pr_info("%s: tfa_vval - dev %d - V validation data (%d)\n",
			__func__, idx, vval_data[idx]);
	else
		pr_err("%s: tfa_vval dev %d - error %d\n",
			__func__, idx, ret);

	return ret;
}

static ssize_t status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write V validation result\n",
		__func__, INDEX_0);

	return size;
}

#if defined(TFA_STEREO_NODE)
static ssize_t status_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_1;
	int ret;

	ret = update_vval_status(idx, buf);
	if (ret > 0)
		pr_info("%s: tfa_vval - dev %d - V validation data (%d)\n",
			__func__, idx, vval_data[idx]);
	else
		pr_err("%s: tfa_vval dev %d - error %d\n",
			__func__, idx, ret);

	return ret;
}

static ssize_t status_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write V validation result\n",
		__func__, INDEX_1);

	return size;
}
#endif /* TFA_STEREO_NODE */

static ssize_t validation_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int size;
	char vval_state[FILESIZE_VVAL] = {0};

	snprintf(vval_state, FILESIZE_VVAL,
		"%s", cur_status ?
		"enabled" /* V validation is active */
		: "disabled"); /* V validation is inactive */

	size = snprintf(buf, strlen(vval_state) + 1,
		"%s", vval_state);

	return size;
}

static ssize_t validation_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tfa_device *tfa = NULL;
	int idx, ndev = MAX_HANDLES;
	uint16_t value;
	int ret = 0, status;

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (!sysfs_streq(buf, "1") && !sysfs_streq(buf, "0")) {
		pr_info("%s: tfa_vval invalid value to start V validation\n",
			__func__);
		return -EINVAL;
	}

	ret = kstrtou32(buf, 10, &status);
	if (!status) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}
	if (cur_status)
		pr_info("%s: tfa_vval prior V validation still runs\n",
			__func__);

	pr_info("%s: tfa_vval begin\n", __func__);

	cur_status = status; /* run - changed to active */

	memset(vval_data, 0, sizeof(int) * MAX_HANDLES);

	/* run V valibration */
	ret = tfa_run_vval(0, &value);
	if (ret == TFA98XX_ERROR_NOT_OPEN)
		return -EINVAL; /* unused device */
	if (ret) {
		pr_info("%s: tfa_vval failed to run V validation\n", __func__);
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
		ret = tfa_get_vval_data(idx, &value);
		if (ret) {
			pr_info("%s: tfa_vval failed to read data after V validation\n",
				__func__);
			value = VVAL_TEST_FAIL;
		}
		if (value == 0xffff) {
			pr_info("%s: tfa_vval invalid data\n", __func__);
			value = VVAL_UNTESTED;
		}

		vval_data[idx] = value;

		pr_info("%s: tfa_vval - dev %d - V validation result (%d)\n",
			__func__, idx, vval_data[idx]);
	}

	pr_info("%s: tfa_vval end\n", __func__);

	return size;
}

int tfa98xx_vval_init(struct class *tfa_class)
{
	int ret = 0;

	if (tfa_class) {
		tfa_vval_dev = device_create(tfa_class,
			NULL, DEV_ID_TFA_VVAL, NULL, TFA_VVAL_DEV_NAME);
		if (!IS_ERR(tfa_vval_dev)) {
			ret = sysfs_create_group(&tfa_vval_dev->kobj,
				&tfa_vval_attr_grp);
			if (ret)
				pr_err("%s: failed to create sysfs group. ret (%d)\n",
					__func__, ret);
		}
	}

	pr_info("%s: initialized (%d)\n", __func__,
		(tfa_class != NULL) ? 1 : 0);

	return ret;
}

void tfa98xx_vval_exit(struct class *tfa_class)
{
	device_destroy(tfa_class, DEV_ID_TFA_VVAL);
	pr_info("exited\n");
}

