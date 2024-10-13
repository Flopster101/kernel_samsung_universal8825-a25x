/*
 * linux/drivers/video/fbdev/exynos/panel/mafpc/mafpc_drv.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAFPC_DRV_H__
#define __MAFPC_DRV_H__

#include <linux/kernel.h>
#include <linux/miscdevice.h>

#define MAFPC_DEV_NAME "mafpc"

#define MAX_MAFPC_CTRL_CMD_SIZE 66

#define MAFPC_CRC_LEN		2

#define MAFPC_HEADER_SIZE	1
#define MAFPC_HEADER		'M'
#define MAFPC_DATA_IDENTIFIER	0

#define MAFPC_UPDATED_FROM_SVC		0x01
#define MAFPC_UPDATED_TO_DEV		0x10

#define ABC_CTRL_CMD_OFFSET(_mafpc) (MAFPC_HEADER_SIZE)
#define ABC_CTRL_CMD_SIZE(_mafpc) ((_mafpc)->ctrl_cmd_len)
#define ABC_COMP_IMG_OFFSET(_mafpc) (ABC_CTRL_CMD_OFFSET(_mafpc) + ABC_CTRL_CMD_SIZE(_mafpc))
#define ABC_COMP_IMG_SIZE(_mafpc) ((_mafpc)->comp_img_len)
#define ABC_SCALE_FACTOR_OFFSET(_mafpc) (ABC_COMP_IMG_OFFSET(_mafpc) + ABC_COMP_IMG_SIZE(_mafpc))
#define ABC_SCALE_FACTOR_SIZE(_mafpc) ((_mafpc)->scale_len)
#define ABC_SCALE_FACTOR_EXIST(_mafpc) ((_mafpc)->scale_len > 0)
#define ABC_DATA_SIZE(_mafpc) (ABC_SCALE_FACTOR_OFFSET(_mafpc) + ABC_SCALE_FACTOR_SIZE(_mafpc))

enum {
	INSTANT_CMD_NONE,
	INSTANT_CMD_ON,
	INSTANT_CMD_OFF,
	MAX_INSTANT_CMD,
};

/*reserved mem info for generate lpd burn-in image*/
struct comp_mem_info {
	bool reserved;
	phys_addr_t image_base;
	size_t image_size;
	phys_addr_t lut_base;
	size_t lut_size;
};

struct mafpc_device {
	int id;
	struct device *dev;
	struct miscdevice miscdev;
	struct panel_mutex lock;
	struct panel_device *panel;

	bool req_update;
	bool enable;
	u32 written;

	u8 *comp_img_buf;
	int comp_img_len;
	u8 *scale_buf;
	u8 scale_len;
	u8 *comp_crc_buf;
	int comp_crc_len;
	/* table: platform brightness -> scale factor index */
	u8 *scale_map_br_tbl;
	int scale_map_br_tbl_len;
	u8 ctrl_cmd[MAX_MAFPC_CTRL_CMD_SIZE];
	u32 ctrl_cmd_len;

	struct comp_mem_info comp_mem;

	struct workqueue_struct *instant_wq;
	struct work_struct instant_work;
	u32 instant_cmd;
};

struct mafpc_info {
	char *name;
	u8 *abc_img;
	u32 abc_img_len;
	u8 *abc_scale_factor;
	u32 abc_scale_factor_len;
	u8 *abc_crc;
	u32 abc_crc_len;
	u8* abc_scale_map_tbl;
	int abc_scale_map_tbl_len;
	u32 abc_ctrl_cmd_len;
};

struct mafpc_device *get_mafpc_device(struct panel_device *panel);
struct mafpc_device *mafpc_device_create(void);
int mafpc_device_init(struct mafpc_device *mafpc);
int mafpc_device_probe(struct mafpc_device *mafpc, struct mafpc_info *info);
int mafpc_set_written_to_dev(struct mafpc_device *mafpc);
int mafpc_clear_written_to_dev(struct mafpc_device *mafpc);

#define MAFPC_IOCTL_MAGIC		'M'
#define IOCTL_MAFPC_ON			_IOW(MAFPC_IOCTL_MAGIC, 1, int)
#define IOCTL_MAFPC_ON_INSTANT		_IOW(MAFPC_IOCTL_MAGIC, 2, int)
#define IOCTL_MAFPC_OFF			_IOW(MAFPC_IOCTL_MAGIC, 3, int)
#define IOCTL_MAFPC_OFF_INSTANT		_IOW(MAFPC_IOCTL_MAGIC, 4, int)
#define IOCTL_CLEAR_IMAGE_BUFFER	_IOW(MAFPC_IOCTL_MAGIC, 5, int)
#endif
