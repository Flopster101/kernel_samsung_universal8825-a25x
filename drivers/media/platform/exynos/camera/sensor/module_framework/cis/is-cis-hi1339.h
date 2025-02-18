/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_HI1339_H
#define IS_CIS_HI1339_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_HI1339_MAX_WIDTH                   (4208 + 0)
#define SENSOR_HI1339_MAX_HEIGHT                  (3120 + 0)

#define USE_GROUP_PARAM_HOLD                      (0)
#define TOTAL_NUM_OF_MIPI_LANES                   (4)

/* TODO: Check below values are valid */
#define SENSOR_HI1339_FINE_INTEGRATION_TIME_MIN           (0x0) //Not supported
#define SENSOR_HI1339_FINE_INTEGRATION_TIME_MAX           (0x0) //Not supported
#define SENSOR_HI1339_COARSE_INTEGRATION_TIME_MIN         (0x04)
#define SENSOR_HI1339_COARSE_INTEGRATION_TIME_MAX_MARGIN  (0x04)

#define SENSOR_HI1339_FRAME_LENGTH_LINE_ADDR      (0x020E)
#define SENSOR_HI1339_LINE_LENGTH_PCK_ADDR        (0x0206)
#define SENSOR_HI1339_GROUP_PARAM_HOLD_ADDR       (0x0208)
#define SENSOR_HI1339_COARSE_INTEG_TIME_ADDR      (0x020A)
#define SENSOR_HI1339_ANALOG_GAIN_ADDR            (0x0213) //8bit
#define SENSOR_HI1339_DIG_GAIN_ADDR               (0x0214) //Gr:0x214 ,Gb:0x216, R:0x218, B:0x21A
#define SENSOR_HI1339_MODEL_ID_ADDR               (0x0716)
#define SENSOR_HI1339_STREAM_ONOFF_ADDR           (0x0B00)
#define SENSOR_HI1339_ISP_ENABLE_ADDR             (0x0B04) //pdaf, mipi, fmt, Hscaler, D gain, DPC, LSC
#define SENSOR_HI1339_MIPI_TX_OP_MODE_ADDR        (0x1002)
#define SENSOR_HI1339_ISP_FRAME_CNT_ADDR          (0x1056)
#define SENSOR_HI1339_ISP_PLL_ENABLE_ADDR         (0x0702)

#define SENSOR_HI1339_MIN_ANALOG_GAIN_SET_VALUE   (0)      //x1.0
#define SENSOR_HI1339_MAX_ANALOG_GAIN_SET_VALUE   (0xF0)   //x16.0
#define SENSOR_HI1339_MIN_DIGITAL_GAIN_SET_VALUE  (0x0200) //x1.0
#define SENSOR_HI1339_MAX_DIGITAL_GAIN_SET_VALUE  (0x1FFB) //x15.99

//WB gain : Not Supported ??

struct setfile_info {
	const u32 *file;
	u32 file_size;
};

#define SET_SETFILE_INFO(s) {   \
	.file = s,                  \
	.file_size = ARRAY_SIZE(s), \
}

/*
enum hi1339_fsync_enum {
	HI1339_FSYNC_NORMAL,
	HI1339_FSYNC_SLAVE,
	HI1339_FSYNC_MASTER_FULL,
	HI1339_FSYNC_MASTER_2_BINNING,
	HI1339_FSYNC_MASTER_4_BINNING,
};
*/

enum sensor_hi1339_mode_enum {
	SENSOR_HI1339_4128X3096_30FPS = 0,
	SENSOR_HI1339_4128X2324_30FPS = 1,
	SENSOR_HI1339_3712X2556_30FPS = 2,
	SENSOR_HI1339_3408X2556_30FPS = 3,
	SENSOR_HI1339_1024X768_120FPS = 4,
	SENSOR_HI1339_2064x1548_30FPS = 5,
	SENSOR_HI1339_2064x1160_30FPS = 6,
	SENSOR_HI1339_1696x1272_30FPS = 7,
	SENSOR_HI1339_1856x1044_30FPS = 8,
	SENSOR_HI1339_MODE_MAX,
};
#endif
