/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * KQ(Kernel Quality) MESH driver implementation
 *  : Jaecheol Kim <jc22.kim@samsung.com>
 *    ChangHwi Seo <c.seo@samsung.com>
 */

#ifndef __KQ_MESH_USER_NAD_H__
#define __KQ_MESH_USER_NAD_H__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>

#define KQ_MESH_USER_NAD_DEVICE_NAME        ("mesh")
#define KQ_MESH_USER_NAD_BUF_SIZE           (65536)
#define KQ_MESH_USER_NAD_COPY_AREA_LEN		(4)

#define KQ_MESH_USER_NAD_IOCTL_MAGIC        ('m')
#define KQ_MESH_USER_NAD_ALLOC_VMAP         _IOW(KQ_MESH_USER_NAD_IOCTL_MAGIC, 1, unsigned long long)
#define KQ_MESH_USER_NAD_FREE_VMAP          _IO(KQ_MESH_USER_NAD_IOCTL_MAGIC, 2)
#define KQ_MESH_USER_NAD_SET_OFFSET         _IOW(KQ_MESH_USER_NAD_IOCTL_MAGIC, 3, unsigned long long)

struct kq_mesh_user_nad_info {
	dev_t id;

	struct device *dev;
	struct class *class;
	struct cdev cdev;

	char *buf;
	void *v_addr;
	unsigned long long base;        //target copy area base address
	unsigned long long size;        //target copy area size
	unsigned long long v_offset;

	unsigned int area_len;
	unsigned int *copy_area_size_arr;
	unsigned long long *copy_area_address_arr;
};

void kq_mesh_user_nad_init_kmn_info(void *kmn_info);
int kq_mesh_user_nad_create_devfs(void);
#endif

