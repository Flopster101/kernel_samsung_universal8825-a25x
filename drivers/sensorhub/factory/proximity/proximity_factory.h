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

#ifndef __SHUB_PROXIMITY_FACTORY_H_
#define __SHUB_PROXIMITY_FACTORY_H_

#include <linux/device.h>
#include <linux/types.h>

struct device_attribute **get_proximity_gp2ap110s_dev_attrs(char *name);
struct device_attribute **get_proximity_stk3x6x_dev_attrs(char *name);
struct device_attribute **get_proximity_stk3328_dev_attrs(char *name);
struct device_attribute **get_proximity_tmd3725_dev_attrs(char *name);
struct device_attribute **get_proximity_tmd4912_dev_attrs(char *name);
struct device_attribute **get_proximity_stk3391x_dev_attrs(char *name);
struct device_attribute **get_proximity_stk33512_dev_attrs(char *name);
struct device_attribute **get_proximity_stk3afx_dev_attrs(char *name);

#endif
