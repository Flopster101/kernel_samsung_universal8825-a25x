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

#define TFA_STC_DEV_NAME	"tfa_stc"
#define FILESIZE_STC	(10)

#if defined(TFA_ENABLE_STC_TUNING)
#define FILESIZE_STC_TUNING	(100)
#endif

/* ---------------------------------------------------------------------- */

#if defined(TFA_ENABLE_STC_TUNING)
static ssize_t spkt_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t spkt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(spkt);

static ssize_t sknt_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sknt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(sknt);

#if defined(TFA_STEREO_NODE)
static ssize_t spkt_r_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t spkt_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(spkt_r);

static ssize_t sknt_r_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sknt_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(sknt_r);
#endif /* TFA_STEREO_NODE */

#if defined(TFA_USE_STC_VOLUME_TABLE)
static ssize_t minmax_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t minmax_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(minmax);

static ssize_t gtable_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t gtable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(gtable);

#if defined(TFA_STEREO_NODE)
static ssize_t minmax_r_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t minmax_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(minmax_r);

static ssize_t gtable_r_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t gtable_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(gtable_r);
#endif /* TFA_STEREO_NODE */
#endif /* TFA_USE_STC_VOLUME_TABLE */

static struct attribute *tfa_stc_attr[] = {
	&dev_attr_spkt.attr,
	&dev_attr_sknt.attr,
#if defined(TFA_STEREO_NODE)
	&dev_attr_spkt_r.attr,
	&dev_attr_sknt_r.attr,
#endif /* TFA_STEREO_NODE */
#if defined(TFA_USE_STC_VOLUME_TABLE)
	&dev_attr_minmax.attr,
	&dev_attr_gtable.attr,
#if defined(TFA_STEREO_NODE)
	&dev_attr_minmax_r.attr,
	&dev_attr_gtable_r.attr,
#endif /* TFA_STEREO_NODE */
#endif /* TFA_USE_STC_VOLUME_TABLE */
	NULL,
};

static struct attribute_group tfa_stc_attr_grp = {
	.attrs = tfa_stc_attr,
};
#endif /* TFA_ENABLE_STC_TUNING */

/* ---------------------------------------------------------------------- */

#if defined(TFA_ENABLE_STC_TUNING)
static struct device *tfa_stc_dev;
#endif

static int sknt_data[MAX_HANDLES];

/* ---------------------------------------------------------------------- */

#if defined(TFA_ENABLE_STC_TUNING)
static ssize_t update_sknt_control(int idx, char *buf)
{
	struct tfa_device *tfa = NULL;
	int size;
	char sknt_result[FILESIZE_STC] = {0};

	tfa = tfa98xx_get_tfa_device_from_index(0);
	if (tfa == NULL)
		return -EINVAL; /* unused device */

	if (idx < 0 || idx >= tfa->dev_count)
		return -EINVAL;

	snprintf(sknt_result, FILESIZE_STC,
		"%d", sknt_data[idx]);

	if (sknt_result[0] == 0)
		size = snprintf(buf, 7 + 1, "no_data"); /* no data */
	else
		size = snprintf(buf, strlen(sknt_result) + 1,
			"%s", sknt_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t spkt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_0;
	int value, size;
	char spkt_result[FILESIZE_STC] = {0};

	value = tfa98xx_update_spkt_data(idx);
	pr_info("%s: tfa_stc - dev %d - speaker temperature (%d)\n",
		__func__, idx, value);

	snprintf(spkt_result, FILESIZE_STC,
		"%d", value);

	if (spkt_result[0] == 0)
		size = snprintf(buf, 1 + 1, "0"); /* no data */
	else
		size = snprintf(buf, strlen(spkt_result) + 1,
			"%s", spkt_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t spkt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write speaker temperature\n",
		__func__, INDEX_0);

	return size;
}

static ssize_t sknt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_0;
	int ret;

	ret = update_sknt_control(idx, buf);
	if (ret > 0)
		pr_info("%s: tfa_stc - dev %d - surface temperature (%d)\n",
			__func__, idx, sknt_data[idx]);
	else
		pr_err("%s: tfa_stc dev %d - error %d\n",
			__func__, idx, ret);

	return ret;
}

static ssize_t sknt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int idx = INDEX_0;
	int ret;
	int value = 0;

	ret = kstrtou32(buf, 10, &value);
	if (!value) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}

	ret = tfa98xx_write_sknt_control(idx, value);
	if (!ret) {
		pr_info("%s: tfa_stc - dev %d - surface temperature (%d)\n",
			__func__, idx, value);
		sknt_data[idx] = value;
	} else {
		pr_err("%s: tfa_stc dev %d - error %d\n",
			__func__, idx, ret);
	}

	return size;
}

#if defined(TFA_USE_STC_VOLUME_TABLE)
static ssize_t minmax_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_0;
	int size = 0;
	int stc_min = 0, stc_max = 0;
	char minmax_result[FILESIZE_STC_TUNING] = {0};

	stc_min = tfa_get_stc_minmax(idx, 0);
	stc_max = tfa_get_stc_minmax(idx, 1);

	snprintf(minmax_result, FILESIZE_STC_TUNING,
		"%d %d", stc_min, stc_max);

	if (minmax_result[0] == 0)
		size = snprintf(buf, 7 + 1, "no_data"); /* no data */
	else
		size = snprintf(buf, strlen(minmax_result) + 1,
			"%s", minmax_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: tfa_stc - dev %d - (min %d, max %d)\n",
		__func__, idx, stc_min, stc_max);

	return size;
}

static ssize_t minmax_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int idx = INDEX_0;
	int ret = 0;
	int mint = 0, maxt = 0;

	ret = sscanf(buf, "%d %d", &mint, &maxt);
	if (ret < 2) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}

	tfa_set_stc_minmax(idx, 0, mint);
	tfa_set_stc_minmax(idx, 1, maxt);
	pr_info("%s: tfa_stc - dev %d - (min %d, max %d)\n",
		__func__, idx, mint, maxt);

	return size;
}

static ssize_t gtable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_0;
	int i, size = 0;
	char gtable_result[FILESIZE_STC_TUNING] = {0};
	int vol_table[STC_TABLE_MAX] = {0};

	tfa_get_stc_gtable(idx, vol_table);

	snprintf(gtable_result, FILESIZE_STC_TUNING,
		"%d %d %d %d %d %d %d %d %d %d",
		vol_table[0], vol_table[1],
		vol_table[2], vol_table[3],
		vol_table[4], vol_table[5],
		vol_table[6], vol_table[7],
		vol_table[8], vol_table[9]);

	if (gtable_result[0] == 0)
		size = snprintf(buf, 7 + 1, "no_data"); /* no data */
	else
		size = snprintf(buf, strlen(gtable_result) + 1,
			"%s", gtable_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < STC_TABLE_MAX; i++)
		pr_info("%s: tfa_stc - dev %d - gain[%d]=%d\n",
			__func__, idx, i, vol_table[i]);

	return size;
}

static ssize_t gtable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int idx = INDEX_0;
	int i, ret = 0;
	int value[STC_TABLE_MAX] = {0};

	ret = sscanf(buf, "%d %d %d %d %d %d %d %d %d %d",
		&value[0], &value[1], &value[2], &value[3], &value[4],
		&value[5], &value[6], &value[7], &value[8], &value[9]);
	if (ret < STC_TABLE_MAX) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}

	tfa_set_stc_gtable(idx, value);

	for (i = 0; i < STC_TABLE_MAX; i++) {
		pr_info("%s: tfa_stc - dev %d - gain[%d]=%d\n",
			__func__, idx, i, value[i]);
	}

	return size;
}
#endif /* TFA_USE_STC_VOLUME_TABLE */

#if defined(TFA_STEREO_NODE)
static ssize_t spkt_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_1;
	int value, size;
	char spkt_result[FILESIZE_STC] = {0};

	value = tfa98xx_update_spkt_data(idx);
	pr_info("%s: tfa_stc - dev %d - speaker temperature (%d)\n",
		__func__, idx, value);

	snprintf(spkt_result, FILESIZE_STC,
		"%d", value);

	if (spkt_result[0] == 0)
		size = snprintf(buf, 1 + 1, "0"); /* no data */
	else
		size = snprintf(buf, strlen(spkt_result) + 1,
			"%s", spkt_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t spkt_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write speaker temperature\n",
		__func__, INDEX_1);

	return size;
}

static ssize_t sknt_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_1;
	int ret;

	ret = update_sknt_control(idx, buf);
	if (ret > 0)
		pr_info("%s: tfa_stc - dev %d - surface temperature (%d)\n",
			__func__, idx, sknt_data[idx]);
	else
		pr_err("%s: tfa_stc dev %d - error %d\n",
			__func__, idx, ret);

	return ret;
}

static ssize_t sknt_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int idx = INDEX_1;
	int ret;
	int value = 0;

	ret = kstrtou32(buf, 10, &value);
	if (!value) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}

	ret = tfa98xx_write_sknt_control(idx, value);
	if (!ret) {
		pr_info("%s: tfa_stc - dev %d - surface temperature (%d)\n",
			__func__, idx, value);
		sknt_data[idx] = value;
	} else {
		pr_err("%s: tfa_stc dev %d - error %d\n",
			__func__, idx, ret);
	}

	return size;
}

#if defined(TFA_USE_STC_VOLUME_TABLE)
static ssize_t minmax_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_1;
	int size = 0;
	int stc_min = 0, stc_max = 0;
	char minmax_result[FILESIZE_STC_TUNING] = {0};

	stc_min = tfa_get_stc_minmax(idx, 0);
	stc_max = tfa_get_stc_minmax(idx, 1);

	snprintf(minmax_result, FILESIZE_STC_TUNING,
		"%d %d", stc_min, stc_max);

	if (minmax_result[0] == 0)
		size = snprintf(buf, 7 + 1, "no_data"); /* no data */
	else
		size = snprintf(buf, strlen(minmax_result) + 1,
			"%s", minmax_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: tfa_stc - dev %d - (min %d, max %d)\n",
		__func__, idx, stc_min, stc_max);

	return size;
}

static ssize_t minmax_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int idx = INDEX_1;
	int ret = 0;
	int mint = 0, maxt = 0;

	ret = sscanf(buf, "%d %d", &mint, &maxt);
	if (ret < 2) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}

	tfa_set_stc_minmax(idx, 0, mint);
	tfa_set_stc_minmax(idx, 1, maxt);
	pr_info("%s: tfa_stc - dev %d - (min %d, max %d)\n",
		__func__, idx, mint, maxt);

	return size;
}

static ssize_t gtable_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = INDEX_1;
	int i, size = 0;
	char gtable_result[FILESIZE_STC_TUNING] = {0};
	int vol_table[STC_TABLE_MAX] = {0};

	tfa_get_stc_gtable(idx, vol_table);

	snprintf(gtable_result, FILESIZE_STC_TUNING,
		"%d %d %d %d %d %d %d %d %d %d",
		vol_table[0], vol_table[1],
		vol_table[2], vol_table[3],
		vol_table[4], vol_table[5],
		vol_table[6], vol_table[7],
		vol_table[8], vol_table[9]);

	if (gtable_result[0] == 0)
		size = snprintf(buf, 7 + 1, "no_data"); /* no data */
	else
		size = snprintf(buf, strlen(gtable_result) + 1,
			"%s", gtable_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < STC_TABLE_MAX; i++)
		pr_info("%s: tfa_stc - dev %d - gain[%d]=%d\n",
			__func__, idx, i, vol_table[i]);

	return size;
}

static ssize_t gtable_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int idx = INDEX_1;
	int i, ret = 0;
	int value[STC_TABLE_MAX] = {0};

	ret = sscanf(buf, "%d %d %d %d %d %d %d %d %d %d",
		&value[0], &value[1], &value[2], &value[3], &value[4],
		&value[5], &value[6], &value[7], &value[8], &value[9]);
	if (ret < STC_TABLE_MAX) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}

	tfa_set_stc_gtable(idx, value);

	for (i = 0; i < STC_TABLE_MAX; i++) {
		pr_info("%s: tfa_stc - dev %d - gain[%d]=%d\n",
			__func__, idx, i, value[i]);
	}

	return size;
}
#endif /* TFA_USE_STC_VOLUME_TABLE */
#endif /* TFA_STEREO_NODE */
#endif /* TFA_ENABLE_STC_TUNING */

int tfa98xx_stc_init(struct class *tfa_class)
{
	int ret = 0;

#if defined(TFA_ENABLE_STC_TUNING)
	if (tfa_class) {
		tfa_stc_dev = device_create(tfa_class,
			NULL, DEV_ID_TFA_STC, NULL, TFA_STC_DEV_NAME);
		if (!IS_ERR(tfa_stc_dev)) {
			ret = sysfs_create_group(&tfa_stc_dev->kobj,
				&tfa_stc_attr_grp);
			if (ret)
				pr_err("%s: failed to create sysfs group. ret (%d)\n",
					__func__, ret);
		}
	}
#endif /* TFA_ENABLE_STC_TUNING */

	pr_info("%s: initialized (%d)\n", __func__,
		(tfa_class != NULL) ? 1 : 0);

	return ret;
}

void tfa98xx_stc_exit(struct class *tfa_class)
{
#if defined(TFA_ENABLE_STC_TUNING)
	device_destroy(tfa_class, DEV_ID_TFA_STC);
#endif
	pr_info("exited\n");
}

