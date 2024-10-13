/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DT_H
#define IS_DT_H

#if IS_ENABLED(CONFIG_OF)
#include "is-spi.h"
#include <exynos-is-module.h>
#include <exynos-is-sensor.h>

#define DT_READ_U32(node, key, value) do {\
		pprop = key; \
		temp = 0; \
		if (of_property_read_u32((node), key, &temp)) \
			pr_debug("%s: no property in the node.\n", pprop);\
		(value) = temp; \
	} while (0)

#define DT_READ_U32_DEFAULT(node, key, value, default_value) do {\
		pprop = key; \
		temp = 0; \
		if (of_property_read_u32((node), key, &temp)) {\
			pr_debug("%s: no property in the node.\n", pprop);\
			(value) = default_value;\
		} else {\
			(value) = temp; \
		}\
	} while (0)

#define DT_READ_STR(node, key, value) do {\
		pprop = key; \
		name = NULL; \
		if (of_property_read_string((node), key, (const char **)&name)) \
			pr_debug("%s: no property in the node.\n", pprop);\
		(value) = name; \
	} while (0)

int is_chain_dev_parse_dt(struct platform_device *pdev);
int is_sensor_dev_parse_dt(struct platform_device *pdev);
int is_sensor_module_parse_dt(struct device *dev,
	int (*module_callback)(struct device *, struct exynos_platform_is_module *));
int is_spi_dev_parse_dt(struct is_spi *spi);
#else
#define is_chain_dev_parse_dt(p)	(-EINVAL)
#define is_sensor_dev_parse_dt(p)	(-EINVAL)
#define is_sensor_module_parse_dt(d, c)	(-EINVAL)
#define is_spi_dev_parse_dt(s)		(-EINVAL)
#endif /* CONFIG_OF */
#endif /* IS_DT_H */
