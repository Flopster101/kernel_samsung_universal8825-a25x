/*
 * Samsung Exynos5 SoC series Actuator driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_LC898219_H
#define IS_DEVICE_LC898219_H

#define LC898219_POS_SIZE_BIT		ACTUATOR_POS_SIZE_10BIT
#define LC898219_POS_MAX_SIZE		((1 << LC898219_POS_SIZE_BIT) - 1)
#define LC898219_POS_DIRECTION		ACTUATOR_RANGE_INF_TO_MAC
#define LC898219_REG_POS_HIGH		0x00
#define LC898219_REG_POS_LOW		0x01
#define LC898219_PRODUCT_ID     0xA5
#endif
