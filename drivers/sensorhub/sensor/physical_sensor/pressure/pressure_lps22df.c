#include "../../pressure.h"
#include "../../../utility/shub_utility.h"
#include "../../../sensormanager/shub_sensor_type.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../sensormanager/shub_sensor.h"

#define LPS22DF_NAME	"LPS22DFTR"

static int init_pressure_lps22df(void)
{
	struct pressure_data *data = (struct pressure_data *)(get_sensor(SENSOR_TYPE_PRESSURE)->data);

	shub_infof("");

	data->convert_coef = 4096;

	return 0;
}


struct sensor_chipset_init_funcs pressure_lps22df_func = {
	.init = init_pressure_lps22df,
};

struct sensor_chipset_init_funcs *get_pressure_lps22df_function_pointer(char *name)
{
	if (strcmp(name, LPS22DF_NAME) != 0)
		return NULL;

	return &pressure_lps22df_func;
}
