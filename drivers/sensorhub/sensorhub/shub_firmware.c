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

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>

#include "../utility/shub_utility.h"
#include "../comm/shub_comm.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../sensorhub/shub_device.h"
#include "../vendor/shub_vendor.h"
#include "../utility/shub_dev_core.h"

enum _fw_type {
	FW_TYPE_NONE,
	FW_TYPE_BIN,
	FW_TYPE_PUSH,
	FW_TYPE_SPU,
};

enum _fw_type fw_type;
char fw_name[PATH_MAX];
u32 cur_fw_version;

#ifdef CONFIG_SHUB_FIRMWARE_DOWNLOAD

#define UPDATE_BIN_FILE "shub.bin"

#define SPU_FW_FILE "sensorhub/shub_spu.bin"

#define FW_VER_LEN 8

#define FW_HEADER_OFFSET	(0xF0)

u32 get_fw_version(char *fw_buf)
{
	return *(u32 *)(fw_buf + FW_HEADER_OFFSET);
}

u32 get_default_fw_version(struct device *dev)
{
	int ret;
	const struct firmware *entry;
	u32 version = 0;

	ret = request_firmware(&entry, fw_name, dev);

	if (ret) {
		shub_errf("Failed to image verify for version check : %d", ret);
		release_firmware(entry);
		return 0;
	}

	version = get_fw_version((char *)entry->data);

	release_firmware(entry);

	return version;
}


#ifdef CONFIG_SHUB_SPU
extern long spu_firmware_signature_verify(const char *fw_name, const u8 *fw_data, const long fw_size);

static int request_spu_firmware(const struct firmware **entry, const char *path, struct device *device)
{
	int ret = 0;
	long fw_size = 0;

	shub_infof("");

	ret = request_firmware(entry, path, device);
	if (ret < 0) {
		shub_infof("request_firmware failed %d", ret);
		return ret;
	}

	//check signing
	shub_infof("fw size %ld", (*entry)->size);
	fw_size = spu_firmware_signature_verify("SENSORHUB", (*entry)->data, (*entry)->size);
	if (fw_size < 0) {
		shub_errf("signature verification failed %ld", fw_size);
		goto spu_err;
	} else {
		u32 spu_version = 0;
		u32 hub_fw_version;
		
		if (fw_size < FW_HEADER_OFFSET + sizeof(hub_fw_version)) {
			shub_errf("fw size is wrong %d", fw_size);
			goto spu_err;
		}

		hub_fw_version = get_default_fw_version(device);

		shub_infof("signature verification success %d", fw_size);

		spu_version = get_fw_version((char *)((*entry)->data));

		shub_infof("urgent fw_version %d kernel ver %d", spu_version, hub_fw_version);

		if (spu_version > hub_fw_version)
			shub_infof("use spu fw size");
		else
			goto spu_err;
	}

	return 0;

spu_err:
	release_firmware(*entry);
	*entry = NULL;
	return -EINVAL;
}
#endif
int download_sensorhub_firmware(struct device *dev, void *addr)
{
	int ret = 0;
	int fw_size;
	char *fw_buf = NULL;
	const struct firmware *entry = NULL;
	if (addr == NULL)
		return -EINVAL;

#ifdef CONFIG_SHUB_DEBUG
	shub_infof("check push bin file");
	ret = request_firmware(&entry, UPDATE_BIN_FILE, dev);
	if (!ret) {
		fw_type = FW_TYPE_PUSH;
		fw_size = (int)entry->size;
		fw_buf = (char *)entry->data;
	} else
#endif
	{
#ifdef CONFIG_SHUB_SPU
		ret = request_spu_firmware(&entry, SPU_FW_FILE, dev);
		if (!ret) {
			shub_infof("download spu firmware");
			fw_type = FW_TYPE_SPU;
			fw_size = (int)entry->size - FW_VER_LEN;
			fw_buf = (char *)entry->data;
		} else
#endif
		{
			shub_infof("download %s", fw_name);
			ret = request_firmware(&entry, fw_name, dev);
			if (ret) {
				shub_errf("request_firmware failed %d", ret);
				fw_type = FW_TYPE_NONE;
				release_firmware(entry);
				return -EINVAL;
			}

			fw_type = FW_TYPE_BIN;
			fw_size = (int)entry->size;
			fw_buf = (char *)entry->data;
		}
	}

	shub_infof("fw type %d ver %d bin(size:%d) on %lx", fw_type, get_fw_version(fw_buf), (int)fw_size, (unsigned long)addr);
	cur_fw_version = 0;

	memcpy(addr, fw_buf, fw_size);
	if (entry)
		release_firmware(entry);

	return 0;
}
#endif /* CONFIG_SHUB_FIRMWARE_DOWNLOAD */

#ifdef CONFIG_SHUB_SPU
static ssize_t spu_verify_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

#ifdef CONFIG_SHUB_FIRMWARE_DOWNLOAD
	const struct firmware *entry;

	ret = request_spu_firmware(&entry, SPU_FW_FILE, get_shub_device());

	if (entry)
		release_firmware(entry);
#endif

	if (ret < 0)
		return sprintf(buf, "%s\n", "NG");
	else
		return sprintf(buf, "%s\n", "OK");
}

static DEVICE_ATTR_RO(spu_verify);

static struct device_attribute *fw_attrs[] = {
	&dev_attr_spu_verify,
	NULL,
};
#endif /* CONFIG_SHUB_SPU */

int initialize_shub_firmware(void)
{
	int ret = 0;
#ifdef CONFIG_SHUB_SPU
	struct shub_data_t *data = get_shub_data();
#endif
#ifdef CONFIG_SHUB_FIRMWARE_DOWNLOAD
	struct device_node *np = get_shub_device()->of_node;
	const char *name;

	name = of_get_property(np, SENSORHUB_NAME_FW_PROPERTY_NAME, NULL);
	if (!name)
		return -ENODEV;

	strcpy(fw_name, name);
	shub_infof("use %s", fw_name);
#endif
#ifdef CONFIG_SHUB_SPU
	ret = sensor_device_create(&data->sysfs_dev, data, "ssp_sensor");
	if (ret < 0) {
		shub_errf("fail to creat ssp_sensor device");
		return ret;
	}

	ret = add_sensor_device_attr(data->sysfs_dev, fw_attrs);
	if (ret < 0)
		shub_errf("fail to add shub device attr");
#endif
	return ret;
}

void remove_shub_firmware(void)
{
#ifdef CONFIG_SHUB_SPU
	struct shub_data_t *data = get_shub_data();

	remove_sensor_device_attr(data->sysfs_dev, fw_attrs);
	sensor_device_destroy(data->sysfs_dev);
#endif
}

int init_shub_firmware(void)
{
	struct shub_system_info *system_info = get_shub_system_info();

	cur_fw_version = system_info->fw_version;
	shub_infof("Firm Version %8u", cur_fw_version);

	return 0;
}

int get_firmware_rev(void)
{
	return cur_fw_version;
}

int get_firmware_type(void)
{
	return fw_type;
}
