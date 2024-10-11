/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _CAMERA_OIS_AOIS_IF_H_
#define _CAMERA_OIS_AOIS_IF_H_

struct notifier_block;

struct noti_reg_data {
	unsigned short addr;
	unsigned char data_size;
	unsigned char data[28];
};

enum cam_ois_aois_fac_mode {
	FACTORY_MODE_OFF = 0,
	FACTORY_MODE_ON  = 1,
	FACTORY_ONETIME  = 2,
	FACTORY_1MS      = 3,
};

enum AOIS_IF_DATA_TYPE {
	AOIS_DATA_TYPE_INVALID = 0,
	AOIS_DATA_TYPE_BYTE,
	AOIS_DATA_TYPE_WORD,
	AOIS_DATA_TYPE_TREE_BYTE,
	AOIS_DATA_TYPE_DWORD,
	AOIS_DATA_TYPE_MAX,
};

int cam_ois_factory_mode_notifier_register(struct notifier_block *nb);
int cam_ois_factory_mode_notifier_unregister(struct notifier_block *nb);
int cam_ois_reg_read_notifier_register(struct notifier_block *nb);
int cam_ois_reg_read_notifier_unregister(struct notifier_block *nb);
int cam_ois_cmd_notifier_register(struct notifier_block *nb);
int cam_ois_cmd_notifier_unregister(struct notifier_block *nb);

int cam_ois_set_fac_mode(enum cam_ois_aois_fac_mode op);
int cam_ois_reg_read_notifier_call_chain(unsigned long val, unsigned short addr,
	unsigned char *data, int size);
int cam_ois_cmd_notifier_call_chain(unsigned long val, unsigned short addr,
	unsigned char *data, int size);
int cam_ois_factory_mode_notifier_call_chain(unsigned long val, void *v);

void cam_ois_set_aois_fac_mode_on(void);
void cam_ois_set_aois_fac_mode_off(void);
void cam_ois_set_aois_fac_mode(enum cam_ois_aois_fac_mode fac_mode, unsigned int msec);

#endif
