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

#ifndef IS_CIS_JN1_H
#define IS_CIS_JN1_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_JN1_MAX_WIDTH          (8160 + 32)
#define SENSOR_JN1_MAX_HEIGHT         (6144 + 32)

#define SENSOR_JN1_BURST_WRITE        (0)

/* Related Sensor Parameter */
#define USE_GROUP_PARAM_HOLD                      (0)
#define TOTAL_NUM_OF_IVTPX_CHANNEL                (1)

#define SENSOR_JN1_FINE_INTEGRATION_TIME                    (0x0100)  /* FINE_INTEG_TIME is a fixed value */
#define SENSOR_JN1_COARSE_INTEGRATION_TIME_FULL             (5)  /* FULL Mode */
#define SENSOR_JN1_COARSE_INTEGRATION_TIME_MIN_4SUM         (3)  /* 4 Binning Mode */
#define SENSOR_JN1_COARSE_INTEGRATION_TIME_MIN_4SUM_A2A2    (4)  /* 4 Binning Mode + A2A2 */
#define SENSOR_JN1_COARSE_INTEGRATION_TIME_MAX_4SUM         (10) /* 4 Binning Mode */
#define SENSOR_JN1_COARSE_INTEGRATION_TIME_MAX_MARGIN       (5)
#define SENSOR_JN1_MAX_COARSE_INTEG_WITH_FRM_LENGTH_CTRL       (65503)
#define SENSOR_JN1_MAX_CIT_LSHIFT_VALUE                        (11)
#define SENSOR_JN1_MIN_FRAME_DURATION                          (100000)
#define SENSOR_JN1_MIN_CAL_DIGITAL                   (1000)

#define SENSOR_JN1_FRM_LENGTH_LINE_LSHIFT_ADDR       (0x0702)
#define SENSOR_JN1_CIT_LSHIFT_ADDR             (0x0704)
#define SENSOR_JN1_FRAME_LENGTH_LINE_ADDR      (0x0340)
#define SENSOR_JN1_LINE_LENGTH_PCK_ADDR        (0x0342)
#define SENSOR_JN1_FINE_INTEG_TIME_ADDR        (0x0200)
#define SENSOR_JN1_COARSE_INTEG_TIME_ADDR      (0x0202)
#define SENSOR_JN1_LONG_COARSE_INTEG_TIME_ADDR (0x0704)

#define SENSOR_JN1_ANALOG_GAIN_ADDR            (0x0204)
#define SENSOR_JN1_DIG_GAIN_ADDR               (0x020E)
#define SENSOR_JN1_MIN_ANALOG_GAIN_ADDR        (0x0084)
#define SENSOR_JN1_MAX_ANALOG_GAIN_ADDR        (0x0086)
#define SENSOR_JN1_MIN_DIG_GAIN_ADDR           (0x1084)
#define SENSOR_JN1_MAX_DIG_GAIN_ADDR           (0x1086)

#define SENSOR_JN1_OTP_PAGE_SETUP_ADDR         (0x0A02)
#define SENSOR_JN1_OTP_READ_TRANSFER_MODE_ADDR (0x0A00)
#define SENSOR_JN1_OTP_STATUS_REGISTER_ADDR    (0x0A01)
#define SENSOR_JN1_OTP_CHIP_REVISION_ADDR      (0x0002)

/* Related EEPROM CAL : WRITE_SENSOR_CAL_FOR_GGC */
#define SENSOR_JN1_GGC_CAL_BASE_REAR           (0x2A36)
#define SENSOR_JN1_GGC_CAL_SIZE                (92)
#define SENSOR_JN1_GGC_REG_ADDR                (0x39DA)

/* Related Function Option */
#define WRITE_SENSOR_CAL_FOR_GGC                (0)
#define SENSOR_JN1_CAL_DEBUG                    (0)
#define SENSOR_JN1_DEBUG_INFO                   (0)

#define SENSOR_JN1_HW_GGC_CAL_BASE_REAR        (0x1352)
#define SENSOR_JN1_HW_GGC_CAL_SIZE             (346)

#define WRITE_SENSOR_CAL_FOR_HW_GGC             (1)

#define SENSOR_JN1_GGC_DUMP_NAME                "/data/vendor/camera/JN1_GGC_DUMP.bin"

enum sensor_jn1_mode_enum {
	/* 4SUM Binning 30Fps */
	SENSOR_JN1_4SUM_4080x3060_30FPS = 0,
	SENSOR_JN1_4SUM_4080x2296_30FPS,
	SENSOR_JN1_A2A2_2032x1524_120FPS,

	/* Remosaic */
	JN1_MODE_REMOSAIC_START,
	SENSOR_JN1_FULL_8160x6120_10FPS = JN1_MODE_REMOSAIC_START,
	JN1_MODE_REMOSAIC_END = SENSOR_JN1_FULL_8160x6120_10FPS,
	SENSOR_JN1_A2A2_1920x1080_240FPS,
	SENSOR_JN1_4SUM_3200x1800_60FPS,
	SENSOR_JN1_A2A2_2032x1148_120FPS,
};

#undef DISABLE_JN1_PDAF_TAIL_MODE

#endif

/* IS_REMOSAIC_MODE(struct is_cis *cis) */
#define IS_REMOSAIC(mode) ({						\
	typecheck(u32, mode) && (mode >= JN1_MODE_REMOSAIC_START) &&	\
	(mode <= JN1_MODE_REMOSAIC_END);				\
})

#define IS_REMOSAIC_MODE(cis) ({					\
	u32 m;								\
	typecheck(struct is_cis *, cis);				\
	m = cis->cis_data->sens_config_index_cur;			\
	(m >= JN1_MODE_REMOSAIC_START) && (m <= JN1_MODE_REMOSAIC_END); \
})

#define IS_SEAMLESS_MODE_CHANGE(cis) ({					\
	u32 m;								\
	typecheck(struct is_cis *, cis);				\
	m = cis->cis_data->sens_config_index_cur;			\
	(m == SENSOR_JN1_REMOSAIC_CROP_4624x3468_30FPS		\
	|| m == SENSOR_JN1_REMOSAIC_CROP_4624x2604_30FPS);		\
})
