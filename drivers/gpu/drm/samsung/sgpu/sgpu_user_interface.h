/*
 * @file sgpu_user_interface.h
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * @brief Defines interfaces for sysfs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef _SGPU_USER_INTERFACE_H_
#define _SGPU_USER_INTERFACE_H_

#include <linux/devfreq.h>

int sgpu_create_sysfs_file(struct devfreq *df);
int sgpu_remove_sysfs_file(struct devfreq *df);

#endif
