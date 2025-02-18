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

#ifndef __SHUB_IIO_H_
#define __SHUB_IIO_H_

#include <linux/device.h>
#include <linux/kernel.h>

int initialize_indio_dev(struct device *dev);
void remove_indio_dev(void);
void shub_report_sensordata(int type, u64 timestamp, char *data, int data_len);
void remove_empty_dev(void);

#endif /* __SHUB_IIO_H_ */
