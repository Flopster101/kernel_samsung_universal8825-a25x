#include "../../pressure.h"
#include "../../../utility/shub_utility.h"
#include "../../../sensormanager/shub_sensor_type.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../sensormanager/shub_sensor.h"


#define BMP580_NAME	"BMP580"

static int init_pressure_bmp580(void)
{
	struct pressure_data *data = (struct pressure_data *)(get_sensor(SENSOR_TYPE_PRESSURE)->data);

	shub_infof("");

	data->convert_coef = 6400;

	return 0;
}


struct sensor_chipset_init_funcs pressure_bmp580_func = {
	.init = init_pressure_bmp580,
};

struct sensor_chipset_init_funcs *get_pressure_bmp580_function_pointer(char *name)
{
	if (strcmp(name, BMP580_NAME) != 0)
		return NULL;

	return &pressure_bmp580_func;
}
