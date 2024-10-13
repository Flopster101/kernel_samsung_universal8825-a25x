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
#include "../sensormanager/shub_sensor_type.h"
#include "../utility/shub_dev_core.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_wait_event.h"
#include "../vendor/shub_vendor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "shub_device.h"

#include <linux/slab.h>
#ifdef CONFIG_SHUB_FIRMWARE_DOWNLOAD
#include "shub_firmware.h"
#endif

ssize_t mcu_reset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct shub_data_t *data = get_shub_data();
	bool is_success = false;
	int ret = 0;
	int prev_reset_cnt;

	prev_reset_cnt = data->cnt_reset;

	reset_mcu(RESET_TYPE_KERNEL_SYSFS);

	ret = shub_wait_event_timeout(&data->reset_lock, 2000);

	shub_infof("");
	if (!ret && is_shub_working() && prev_reset_cnt != data->cnt_reset)
		is_success = true;

	return sprintf(buf, "%s\n", (is_success ? "OK" : "NG"));
}

static ssize_t show_reset_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct reset_info_t reset_info = get_reset_info();

	if (reset_info.reason == RESET_TYPE_KERNEL_SYSFS)
		ret = sprintf(buf, "Kernel Sysfs\n");
	else if (reset_info.reason == RESET_TYPE_KERNEL_NO_EVENT)
		ret = sprintf(buf, "Kernel No Event\n");
	else if (reset_info.reason == RESET_TYPE_HUB_NO_EVENT)
		ret = sprintf(buf, "Hub Req No Event\n");
	else if (reset_info.reason == RESET_TYPE_KERNEL_COM_FAIL)
		ret = sprintf(buf, "Com Fail\n");
	else if (reset_info.reason == RESET_TYPE_HUB_CRASHED)
		ret = sprintf(buf, "HUB Reset\n");

	return ret;
}

static ssize_t fs_ready_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	shub_infof("");
	fs_ready_cb();

	return size;
}

ssize_t mcu_revision_show(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_SHUB_FIRMWARE_DOWNLOAD
	return sprintf(buf, "%s01%u,%s01%u\n", SENSORHUB_VENDOR, get_firmware_rev(), SENSORHUB_VENDOR,
		       get_firmware_rev());
#else
	return sprintf(buf, "N,N\n");
#endif
}

ssize_t mcu_model_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", SENSORHUB_NAME);
}

static ssize_t operation_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	enum cts_state {
		CTS_STATE_NONE = 0,
		CTS_STATE_START,
		CTS_STATE_STOP,
	};
	char cts_state = CTS_STATE_NONE;
	int ret = 0;

	shub_infof("%s", buf);

	if (strstr(buf, "restrict=on")) {
		cts_state = CTS_STATE_START;
		ret = shub_send_command(CMD_SETVALUE, TYPE_HUB, CMD_CTS_STATE_NOTIFICATION, &cts_state, sizeof(cts_state));
	} else if (strstr(buf, "restrict=off")) {
		cts_state = CTS_STATE_STOP;
		ret = shub_send_command(CMD_SETVALUE, TYPE_HUB, CMD_CTS_STATE_NOTIFICATION, &cts_state, sizeof(cts_state));
	}

	return size;
}

ssize_t minidump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct shub_data_t *data = get_shub_data();

	return sprintf(buf, "%s\n", data->mini_dump);
}

static DEVICE_ATTR(mcu_rev, S_IRUGO, mcu_revision_show, NULL);
static DEVICE_ATTR(mcu_name, S_IRUGO, mcu_model_name_show, NULL);
static DEVICE_ATTR(mcu_reset, S_IRUGO, mcu_reset_show, NULL);
static DEVICE_ATTR(reset_info, S_IRUGO, show_reset_info, NULL);
static DEVICE_ATTR(fs_ready, 0220, NULL, fs_ready_store);
static DEVICE_ATTR(operation_mode, 0220, NULL, operation_mode_store);
static DEVICE_ATTR(minidump, S_IRUGO, minidump_show, NULL);

static struct device_attribute *shub_attrs[] = {
	&dev_attr_mcu_rev,
	&dev_attr_mcu_name,
	&dev_attr_mcu_reset,
	&dev_attr_reset_info,
	&dev_attr_fs_ready,
	&dev_attr_operation_mode,
	&dev_attr_minidump,
	NULL,
};

int init_shub_sysfs(struct device *shub_dev)
{
	struct shub_data_t *data = get_shub_data();
	int ret;

	ret = sensor_device_create(&data->sysfs_dev, data, "ssp_sensor");
	if (ret < 0) {
		shub_errf("fail to creat ssp_sensor device");
		return ret;
	}

	ret = add_sensor_device_attr(data->sysfs_dev, shub_attrs);
	if (ret < 0)
		shub_errf("fail to add shub device attr");

	return ret;
}

void remove_shub_sysfs(struct device *shub_dev)
{
	struct shub_data_t *data = get_shub_data();

	remove_sensor_device_attr(data->sysfs_dev, shub_attrs);
	sensor_device_destroy(data->sysfs_dev);
}
