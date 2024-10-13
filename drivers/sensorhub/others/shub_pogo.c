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

#include "../sensormanager/shub_sensor_manager.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../utility/shub_utility.h"
#include "../sensorhub/shub_device.h"

#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#if IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V2) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V3)
#define SHUB_POGO
#endif

#ifdef SHUB_POGO
#include <linux/input/pogo_i2c_notifier.h>

struct notifier_block pogo_nb;
unsigned long pogo_action;

static int shub_pogo_notifier(struct notifier_block *nb, unsigned long action, void *pogo_data)
{

	switch (action) {
	case POGO_NOTIFIER_ID_ATTACHED:
		shub_infof("pogo attach");
		enable_sensor(SENSOR_TYPE_POGO_REQUEST_HANDLER, NULL, 0);
		break;
	case POGO_NOTIFIER_ID_DETACHED:
		shub_infof("pogo dettach");
		disable_sensor(SENSOR_TYPE_POGO_REQUEST_HANDLER, NULL, 0);
		break;
	};
	return 0;
}

void init_shub_pogo(void)
{
	shub_infof();
	pogo_notifier_register(&pogo_nb, shub_pogo_notifier, POGO_NOTIFY_DEV_SENSOR);
}

void remove_shub_pogo(void)
{
	shub_infof();
	pogo_notifier_unregister(&pogo_nb);
}
#else
void init_shub_pogo(void)
{
	shub_infof("tablet concept config is not set");
}

void remove_shub_pogo(void)
{
	shub_infof("tablet concept config is not set");
}
#endif
