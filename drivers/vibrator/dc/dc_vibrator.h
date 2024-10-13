// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __DC_VIBRATOR_H__
#define __DC_VIBRATOR_H__
#define DC_VIB_NAME "dc_vib"
#include <linux/regulator/consumer.h>
#include <linux/vibrator/sec_vibrator.h>

struct dc_vib_pdata {
	const char *regulator_name;
	struct regulator *regulator;
	int gpio_en;
	const char *motor_type;
};

struct dc_vib_drvdata {
	struct sec_vibrator_drvdata sec_vib_ddata;
	struct dc_vib_pdata *pdata;
	bool running;
};
#endif

