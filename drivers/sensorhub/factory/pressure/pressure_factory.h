#ifndef __SHUB_PRESSURE_FACTORY_H_
#define __SHUB_PRESSURE_FACTORY_H_

#include <linux/device.h>
#include <linux/types.h>

struct device_attribute **get_pressure_lps22hh_dev_attrs(char *name);
struct device_attribute **get_pressure_lps22df_dev_attrs(char *name);
struct device_attribute **get_pressure_lps25h_dev_attrs(char *name);
struct device_attribute **get_pressure_bmp580_dev_attrs(char *name);

#endif
