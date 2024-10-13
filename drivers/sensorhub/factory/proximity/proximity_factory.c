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

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>

#include "proximity_factory.h"
#include "../../sensor/proximity.h"
#include "../../sensorhub/shub_device.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_dev_core.h"
#include "../../utility/shub_utility.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

static struct device *proximity_sysfs_device;
static struct device_attribute **chipset_attrs;

static u32 position[6];

static ssize_t prox_probe_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	bool probe_pass_fail;

	probe_pass_fail = get_sensor_probe_state(SENSOR_TYPE_PROXIMITY);
	return snprintf(buf, PAGE_SIZE, "%d\n", probe_pass_fail);
}


#define PROX_THRESH_DUMMY_HIGH 8000
#define PROX_THRESH_DUMMY_LOW  2200

static ssize_t thresh_high_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", PROX_THRESH_DUMMY_HIGH);
}

static ssize_t thresh_low_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", PROX_THRESH_DUMMY_LOW);
}

u16 get_prox_raw_data(void)
{
	u16 raw_data = 0;
	s32 ms_delay = 20;
	char tmpe_buf[8] = { 0, };
	struct prox_raw_event *sensor_value =
	    (struct prox_raw_event *)(get_sensor_event(SENSOR_TYPE_PROXIMITY_RAW)->value);

	memcpy(&tmpe_buf[0], &ms_delay, 4);

	if (!get_sensor_enabled(SENSOR_TYPE_PROXIMITY_RAW)) {
		batch_sensor(SENSOR_TYPE_PROXIMITY_RAW, 20, 0);
		enable_sensor(SENSOR_TYPE_PROXIMITY_RAW, NULL, 0);

		msleep(200);
		raw_data = sensor_value->prox_raw;
		disable_sensor(SENSOR_TYPE_PROXIMITY_RAW, NULL, 0);
	} else {
		raw_data = sensor_value->prox_raw;
	}

	return raw_data;
}


static ssize_t raw_data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!get_sensor_probe_state(SENSOR_TYPE_PROXIMITY_RAW)) {
		shub_errf("sensor is not probed!");
		return 0;
	}

	return sprintf(buf, "%u\n", get_prox_raw_data());
}

static ssize_t prox_avg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct proximity_raw_data *data = get_sensor(SENSOR_TYPE_PROXIMITY_RAW)->data;

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", data->avg_data[PROX_RAW_MIN], data->avg_data[PROX_RAW_AVG],
			data->avg_data[PROX_RAW_MAX]);
}

static ssize_t prox_avg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	char temp_buf[8] = { 0, };
	int ret;
	int64_t enable;
	s32 ms_delay = 20;

	memcpy(&temp_buf[0], &ms_delay, 4);

	ret = kstrtoll(buf, 10, &enable);
	if (ret < 0)
		shub_errf("- failed = %d", ret);

	if (enable) {
		if (!get_sensor_enabled(SENSOR_TYPE_PROXIMITY_RAW)) {
			batch_sensor(SENSOR_TYPE_PROXIMITY_RAW, 20, 0);
			enable_sensor(SENSOR_TYPE_PROXIMITY_RAW, NULL, 0);
		}
	} else {
		if (get_sensor_enabled(SENSOR_TYPE_PROXIMITY_RAW))
			disable_sensor(SENSOR_TYPE_PROXIMITY_RAW, NULL, 0);
	}

	return size;
}

static ssize_t prox_offset_pass_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
}

static ssize_t prox_position_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u.%u %u.%u %u.%u\n", position[0], position[1], position[2], position[3], position[4],
		       position[5]);
}

static ssize_t trim_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_len = 0;
	u8 trim_check;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_PROXIMITY, PROX_SUBCMD_TRIM_CHECK, 1000, NULL, 0,
								 &buffer, &buffer_len, true);

	if (ret < 0) {
		shub_errf("shub_send_command_wait fail %d", ret);
		return ret;
	}

	if (buffer_len != sizeof(trim_check)) {
		shub_errf("buffer length error %d", buffer_len);
		kfree(buffer);
		return -EINVAL;
	}

	memcpy(&trim_check, buffer, sizeof(trim_check));
	kfree(buffer);

	shub_infof("%d", trim_check);

	if (trim_check != 0 && trim_check != 1) {
		shub_errf("hub read trim NG");
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", (trim_check == 0) ? "TRIM" : "UNTRIM");
}

static DEVICE_ATTR_RO(prox_position);
static DEVICE_ATTR_RO(prox_probe);
static DEVICE_ATTR_RO(thresh_high);
static DEVICE_ATTR_RO(thresh_low);
static DEVICE_ATTR_RO(raw_data);
static DEVICE_ATTR(prox_avg, 0664, prox_avg_show, prox_avg_store);
static DEVICE_ATTR_RO(prox_offset_pass);
static DEVICE_ATTR_RO(trim_check);

static struct device_attribute *proximity_attrs[] = {
	&dev_attr_prox_probe,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_raw_data,
	&dev_attr_prox_avg,
	&dev_attr_prox_offset_pass,
	NULL,
	NULL,
	NULL,
};

typedef struct device_attribute** (*get_chipset_dev_attrs)(char *);
get_chipset_dev_attrs get_proximity_chipset_dev_attrs[] = {
	get_proximity_gp2ap110s_dev_attrs,
	get_proximity_stk3x6x_dev_attrs,
	get_proximity_stk3328_dev_attrs,
	get_proximity_tmd3725_dev_attrs,
	get_proximity_tmd4912_dev_attrs,
	get_proximity_stk3391x_dev_attrs,
	get_proximity_stk33512_dev_attrs,
	get_proximity_stk3afx_dev_attrs,
};

static void check_proximity_dev_attr(void)
{
	struct device_node *np = get_shub_device()->of_node;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	int index = 0;

	while (proximity_attrs[index] != NULL)
		index++;

	if (!of_property_read_u32_array(np, "prox-position", position, ARRAY_SIZE(position))) {
		if (index < ARRAY_SIZE(proximity_attrs))
			proximity_attrs[index++] = &dev_attr_prox_position;
		shub_info("prox-position - %u.%u %u.%u %u.%u", position[0], position[1], position[2], position[3],
			   position[4], position[5]);
	}

	if (sensor->spec.vendor == VENDOR_AMS
	|| sensor->spec.vendor == VENDOR_CAPELLA
	|| sensor->spec.vendor == VENDOR_SITRONIX) {
		if (index < ARRAY_SIZE(proximity_attrs))
			proximity_attrs[index++] = &dev_attr_trim_check;
	}
}

void initialize_proximity_sysfs(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	int ret;
	uint64_t i;

	ret = sensor_device_create(&proximity_sysfs_device, NULL, "proximity_sensor");
	if (ret < 0) {
		shub_errf("fail to creat %s sysfs device", sensor->name);
		return;
	}

	check_proximity_dev_attr();

	ret = add_sensor_device_attr(proximity_sysfs_device, proximity_attrs);
	if (ret < 0) {
		shub_errf("fail to add %s sysfs device attr", sensor->name);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(get_proximity_chipset_dev_attrs); i++) {
		chipset_attrs = get_proximity_chipset_dev_attrs[i](sensor->spec.name);
		if (chipset_attrs) {
			ret = add_sensor_device_attr(proximity_sysfs_device, chipset_attrs);
			if (ret < 0) {
				shub_errf("fail to add sysfs chipset device attr(%d)", (int)i);
				return;
			}
			break;
		}
	}
}

void remove_proximity_sysfs(void)
{
	if (chipset_attrs)
		remove_sensor_device_attr(proximity_sysfs_device, chipset_attrs);
	remove_sensor_device_attr(proximity_sysfs_device, proximity_attrs);
	sensor_device_destroy(proximity_sysfs_device);
	proximity_sysfs_device = NULL;
}

void initialize_proximity_factory(bool en)
{
	if (!get_sensor(SENSOR_TYPE_PROXIMITY))
		return;

	if (en)
		initialize_proximity_sysfs();
	else
		remove_proximity_sysfs();
}
