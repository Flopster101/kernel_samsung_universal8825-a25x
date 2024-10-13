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
#include "../sensormanager/shub_sensor_type.h"
#include "../utility/shub_utility.h"
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/slab.h>

#ifdef CONFIG_SHUB_AOIS
#define AOIS_MAX_DATA		28

extern int cam_ois_cmd_notifier_register(struct notifier_block *nb);
extern int cam_ois_cmd_notifier_unregister(struct notifier_block *nb);
extern int cam_ois_reg_read_notifier_register(struct notifier_block *nb);
extern int cam_ois_reg_read_notifier_unregister(struct notifier_block *nb);
extern int cam_ois_factory_mode_notifier_register(struct notifier_block *nb);
extern int cam_ois_factory_mode_notifier_unregister(struct notifier_block *nb);

#define AOIS_SUBCMD_WRITE		1
#define AOIS_SUBCMD_READ		2
#define AOIS_SUBCMD_FACTORY		3

struct aois_data {
	uint16_t addr;
	int8_t size;
	int8_t data[AOIS_MAX_DATA];
};


static int aois_cmd_factory_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	int ret;
	char factory_op;

	if (!data) {
		shub_errf("data is NULL");
		return NOTIFY_BAD;
	}

	memcpy(&factory_op, data, sizeof(factory_op));
#ifdef CONFIG_SHUB_DEBUG
	shub_infof("[DEBUG] factory mode %d", factory_op);
#endif
	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_AOIS, AOIS_SUBCMD_FACTORY, (char *)&factory_op,
							sizeof(factory_op));

	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}


static int aois_cmd_write_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	int ret;
	struct aois_data cmd_data;

	if (!data) {
		shub_errf("data is NULL");
		return NOTIFY_BAD;
	}

	memcpy(&cmd_data, data, sizeof(cmd_data));
#ifdef CONFIG_SHUB_DEBUG
	shub_infof("[DEBUG] 0x%x %d", cmd_data.addr, cmd_data.size);
#endif
	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_AOIS, AOIS_SUBCMD_WRITE, (char *)&cmd_data,
							sizeof(cmd_data));

	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}

static int aois_reg_read_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	int ret;
	char *buf = NULL;
	int buf_len = 0;
	u16 addr;
	u8 size;

	if (!data) {
		shub_errf("data is NULL");
		return NOTIFY_BAD;
	}

	memcpy(&addr, data, sizeof(u16));
	memcpy(&size, ((u8 *)data + 2), sizeof(u8));
#ifdef CONFIG_SHUB_DEBUG
	shub_infof("[DEBUG] 0x%x %d", addr, size);
#endif
	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_AOIS, AOIS_SUBCMD_READ, 1000, (char *)data, 3, &buf,
								 &buf_len, false);

	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return NOTIFY_BAD;
	}

	if (buf != NULL && buf_len == size) {
		memcpy(((u8*)data + 3), buf, buf_len);
		ret = NOTIFY_OK;
	} else {
		shub_errf("buf error(%d)", buf_len);
		ret = NOTIFY_BAD;
	}

	kfree(buf);

	return ret;
}


static struct notifier_block aois_write_nb = {
	.notifier_call = aois_cmd_write_notifier,
};

static struct notifier_block aois_read_nb = {
	.notifier_call = aois_reg_read_notifier,
};

static struct notifier_block aois_factory_nb = {
	.notifier_call = aois_cmd_factory_notifier,
};

void init_shub_aois(void)
{
	shub_infof("");
	cam_ois_cmd_notifier_register(&aois_write_nb);
	cam_ois_reg_read_notifier_register(&aois_read_nb);
	cam_ois_factory_mode_notifier_register(&aois_factory_nb);
}

void remove_shub_aois(void)
{
	shub_infof("");
	cam_ois_cmd_notifier_unregister(&aois_write_nb);
	cam_ois_reg_read_notifier_unregister(&aois_read_nb);
	cam_ois_factory_mode_notifier_unregister(&aois_factory_nb);
}
#else
void init_shub_aois(void)
{
	shub_infof("aois is not supported");
}

void remove_shub_aois(void)
{
	shub_infof("aois is not supported");
}
#endif
