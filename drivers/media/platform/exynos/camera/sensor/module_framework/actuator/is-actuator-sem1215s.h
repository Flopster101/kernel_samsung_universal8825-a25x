/*
 * Samsung Exynos5 SoC series Actuator driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_SEM1215S_H
#define IS_DEVICE_SEM1215S_H

#define SEM1215S_POS_SIZE_BIT		ACTUATOR_POS_SIZE_10BIT
#define SEM1215S_POS_MAX_SIZE		((1 << SEM1215S_POS_SIZE_BIT) - 1)
#define SEM1215S_POS_DIRECTION		ACTUATOR_RANGE_INF_TO_MAC
#define SEM1215S_REG_POS_LOW		0x0204
#define SEM1215S_REG_POS_HIGH		0x0205
#define SEM1215S_PRODUCT_ID     0xA5
#endif
