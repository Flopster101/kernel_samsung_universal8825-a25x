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

#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#include "../../../comm/shub_comm.h"
#include "../../../utility/shub_utility.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../sensormanager/shub_sensor_manager.h"

#include "../../magnetometer.h"

#define AK09918C_NAME	"AK09918C"

static int init_mag_ak09918c(void)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	data->mag_matrix_len = 27;
	data->cal_data_len = sizeof(struct calibration_data_ak09918c);

	return 0;
}

static void parse_dt_magnetometer_ak09918c(struct device *dev)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;
	struct device_node *np = dev->of_node;
	int check_mst_gpio, check_nfc_gpio;
	int value_mst = 0, value_nfc = 0;

	shub_infof("");

	if (of_property_read_u32(np, "mag-ak09918c-position", &data->position)) {
		data->position = 0;
		shub_err("no mag-ak09918c-position, set as 0");
	}

	shub_info("position[%d]", data->position);

	shub_infof("matrix_len %d", data->mag_matrix_len);
	if (of_property_read_u8_array(np, "mag-ak09918c-array", data->mag_matrix, data->mag_matrix_len))
		shub_err("no mag-ak09918c-array, set as 0");

	// check nfc/mst for mag matrix
	check_nfc_gpio = of_get_named_gpio(np, "mag-check-nfc", 0);
	if (check_nfc_gpio >= 0)
		value_nfc = gpio_get_value(check_nfc_gpio);

	check_mst_gpio = of_get_named_gpio(np, "mag-check-mst", 0);
	if (check_mst_gpio >= 0)
		value_mst = gpio_get_value(check_mst_gpio);

	if (value_mst == 1) {
		shub_info("mag matrix(%d %d) nfc/mst array", value_nfc, value_mst);
		if (of_property_read_u8_array(np, "mag-ak09918c-mst-array", data->mag_matrix,
							data->mag_matrix_len))
			shub_err("no mag-ak09918c-mst-array");
	} else if (value_nfc == 1) {
		shub_info("mag matrix(%d %d) nfc only array", value_nfc, value_mst);
		if (of_property_read_u8_array(np, "mag-ak09918c-nfc-array", data->mag_matrix,
							data->mag_matrix_len))
			shub_err("no mag-ak09918c-nfc-array");
	}

	if (get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR)) {
		if (of_property_read_u8_array(np, "mag-ak09918c-cover-array", data->cover_matrix, data->mag_matrix_len))
			shub_err("no mag-ak09918c-cover-array, set as 0");
	}
}

struct sensor_chipset_init_funcs magnetometer_ak09918c_func = {
	.init = init_mag_ak09918c,
	.parse_dt = parse_dt_magnetometer_ak09918c,
};

struct sensor_chipset_init_funcs *get_magnetic_ak09918c_function_pointer(char *name)
{
	if (strcmp(name, AK09918C_NAME) != 0)
		return NULL;

	return &magnetometer_ak09918c_func;
}
