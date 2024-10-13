
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

#define MMC5633_NAME   "MMC5633"

static int init_mag_mmc5633(void)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	data->mag_matrix_len = 27;
	data->cal_data_len = sizeof(struct calibration_data_mmc5633);

	return 0;
}

static void parse_dt_magnetometer_mmc5633(struct device *dev)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;
	struct device_node *np = dev->of_node;
	int check_mst_gpio, check_nfc_gpio;
	int value_mst = 0, value_nfc = 0;

	if (of_property_read_u32(np, "mag-mmc5633-position", &data->position)) {
		data->position = 0;
		shub_err("no mag-mmc5633-position, set as 0");
	}

	shub_info("position[%d]", data->position);

	if (of_property_read_u8_array(np, "mag-mmc5633-array", data->mag_matrix, data->mag_matrix_len))
		shub_err("no mag-mmc5633-array, set as 0");

	// check nfc/mst for mag matrix
	check_nfc_gpio = of_get_named_gpio(np, "mag-check-nfc", 0);
	if (check_nfc_gpio >= 0)
		value_nfc = gpio_get_value(check_nfc_gpio);

	check_mst_gpio = of_get_named_gpio(np, "mag-check-mst", 0);
	if (check_mst_gpio >= 0)
		value_mst = gpio_get_value(check_mst_gpio);

	if (value_mst == 1) {
		shub_info("mag matrix(%d %d) nfc/mst array", value_nfc, value_mst);
		if (of_property_read_u8_array(np, "mag-mmc5633-mst-array", data->mag_matrix,
							data->mag_matrix_len))
			shub_err("no mag-mmc5633-mst-array");
	} else if (value_nfc == 1) {
		shub_info("mag matrix(%d %d) nfc only array", value_nfc, value_mst);
		if (of_property_read_u8_array(np, "mag-mmc5633-nfc-array", data->mag_matrix,
							data->mag_matrix_len))
			shub_err("no mag-mmc5633-nfc-array");
	}
}

struct sensor_chipset_init_funcs magnetic_mmc5633_ops = {
	.init = init_mag_mmc5633,
	.parse_dt = parse_dt_magnetometer_mmc5633,
};

struct sensor_chipset_init_funcs *get_magnetic_mmc5633_function_pointer(char *name)
{
	if (strcmp(name, MMC5633_NAME) != 0)
		return NULL;

	return &magnetic_mmc5633_ops;
}
