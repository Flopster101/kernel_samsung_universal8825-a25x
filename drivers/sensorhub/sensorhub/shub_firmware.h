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

#ifndef _SHUB_FIRMWARE_H__
#define _SHUB_FIRMWARE_H__

#include <linux/device.h>

#ifdef CONFIG_SHUB_FIRMWARE_DOWNLOAD
int download_sensorhub_firmware(struct device* dev, void * addr);
unsigned int get_kernel_fw_rev(void);
#endif /* CONFIG_SHUB_FIRMWARE_DOWNLOAD */

int initialize_shub_firmware(void);
void remove_shub_firmware(void);

void set_firmware_rev(uint32_t version);
int get_firmware_rev(void);
int get_firmware_type(void);

int init_shub_firmware(void);

#endif /*_SHUB_FIRMWARE_H__*/