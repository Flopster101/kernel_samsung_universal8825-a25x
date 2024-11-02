/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_GD2_H
#define IS_CIS_GD2_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_GD2_MAX_WIDTH          (6560 + 0)
#define SENSOR_GD2_MAX_HEIGHT         (4936 + 0)

#define SENSOR_GD2_BURST_WRITE        (1)

/* Related Sensor Parameter */
#define USE_GROUP_PARAM_HOLD                      (0)
#define TOTAL_NUM_OF_IVTPX_CHANNEL                (1)

#define SENSOR_GD2_FINE_INTEGRATION_TIME                    (0x0100)    /* FINE_INTEG_TIME is a fixed value */
#define SENSOR_GD2_COARSE_INTEGRATION_TIME_MIN_FULL_2SUM    (10) /* FULL Mode */
#define SENSOR_GD2_COARSE_INTEGRATION_TIME_MIN_4SUM         (12) /* 4 Binning Mode 120 fps */
#define SENSOR_GD2_COARSE_INTEGRATION_TIME_MAX_MARGIN_FULL  (9)
#define SENSOR_GD2_COARSE_INTEGRATION_TIME_MAX_MARGIN_4SUM  (10)

#define SENSOR_GD2_FRAME_LENGTH_LINE_ADDR      (0x0340)  
#define SENSOR_GD2_LINE_LENGTH_PCK_ADDR        (0x0342)
#define SENSOR_GD2_FINE_INTEG_TIME_ADDR        (0x0200)
#define SENSOR_GD2_COARSE_INTEG_TIME_ADDR      (0x0202)
#define SENSOR_GD2_LONG_COARSE_INTEG_TIME_ADDR (0x021E)

#define SENSOR_GD2_ANALOG_GAIN_ADDR            (0x0204)
#define SENSOR_GD2_DIG_GAIN_ADDR               (0x020E)
#define SENSOR_GD2_MIN_ANALOG_GAIN_ADDR        (0x0084)
#define SENSOR_GD2_MAX_ANALOG_GAIN_ADDR        (0x0086)
#define SENSOR_GD2_MIN_DIG_GAIN_ADDR           (0x1084)
#define SENSOR_GD2_MAX_DIG_GAIN_ADDR           (0x1086)

/* OPT Chip ID Version Check */
#define SENSOR_GD2_OTP_PAGE_SETUP_ADDR         (0x0A02)
#define SENSOR_GD2_OTP_READ_TRANSFER_MODE_ADDR (0x0A00)
#define SENSOR_GD2_OTP_STATUS_REGISTER_ADDR    (0x0A01)
#define SENSOR_GD2_OTP_CHIP_REVISION_ADDR      (0x0002)

/* Related EEPROM CAL : WRITE_SENSOR_CAL_FOR_GGC */
#define SENSOR_GD2_GGC_CAL_BASE_REAR           (0x2A36)
#define SENSOR_GD2_GGC_CAL_SIZE                (92)
#define SENSOR_GD2_GGC_REG_ADDR                (0x39DA)

/* Related Function Option */
#define WRITE_SENSOR_CAL_FOR_GGC                (1)
#define SENSOR_GD2_CAL_DEBUG                    (0)
#define SENSOR_GD2_DEBUG_INFO                   (0)

#define SENSOR_GD2_GGC_DUMP_NAME                "/data/vendor/camera/GD2_GGC_DUMP.bin"

enum sensor_gd2_mode_enum {
	/* 2SUM */
	SENSOR_GD2_2SUM_FULL_3264X2448_30FPS,
	SENSOR_GD2_2SUM_CROP_3264X1836_30FPS,
	SENSOR_GD2_2SUM_CROP_2880X1980_30FPS,
	SENSOR_GD2_2SUM_CROP_2640X1980_30FPS,
	SENSOR_GD2_2SUM_CROP_1632X1224_120FPS,
	SENSOR_GD2_2SUM_CROP_3264X1836_60FPS,
	/* 4SUM */
	SENSOR_GD2_4SUM_CROP_3264X1836_120FPS,
	/* FULL Remosaic */
	SENSOR_GD2_REMOSAIC_FULL_6528x4896_24FPS,
};

#undef DISABLE_GD2_PDAF_TAIL_MODE

#endif
