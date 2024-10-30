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

#ifndef IS_CIS_IMX582_H
#define IS_CIS_IMX582_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_IMX582_MAX_WIDTH          (8000 + 0)
#define SENSOR_IMX582_MAX_HEIGHT         (6000 + 0)

/* Related Sensor Parameter */
#define USE_GROUP_PARAM_HOLD                      (1)
#define TOTAL_NUM_OF_IVTPX_CHANNEL                (4)

#define SENSOR_IMX582_FINE_INTEGRATION_TIME                    (6320)    //FINE_INTEG_TIME is a fixed value (0x0200: 16bit - read value is 0x18b0)
#define SENSOR_IMX582_COARSE_INTEGRATION_TIME_MIN              (5)
#define SENSOR_IMX582_COARSE_INTEGRATION_TIME_MIN_FOR_PDAF     (5)//(6)  // To Do: Temporally min integration is setted '5' until applying the PDAF because when bring-up is not applied the PDAF.
#define SENSOR_IMX582_COARSE_INTEGRATION_TIME_MIN_FOR_V2H2     (6)
#define SENSOR_IMX582_COARSE_INTEGRATION_TIME_MAX_MARGIN       (48)
#define SENSOR_IMX582_MAX_COARSE_INTEG_WITH_FRM_LENGTH_CTRL    (65503)
#define SENSOR_IMX582_MAX_CIT_LSHIFT_VALUE                     (0x7)
#define SENSOR_IMX582_MIN_FRAME_DURATION                       (100000)

#define SENSOR_IMX582_OTP_PAGE_SETUP_ADDR         (0x0A02)
#define SENSOR_IMX582_OTP_READ_TRANSFER_MODE_ADDR (0x0A00)
#define SENSOR_IMX582_OTP_STATUS_REGISTER_ADDR    (0x0A01)
#define SENSOR_IMX582_OTP_CHIP_REVISION_ADDR      (0x0018)

#define SENSOR_IMX582_CIT_LSHIFT_ADDR             (0x3100)
#define SENSOR_IMX582_FRAME_LENGTH_LINE_ADDR      (0x0340)
#define SENSOR_IMX582_LINE_LENGTH_PCK_ADDR        (0x0342)
#define SENSOR_IMX582_FINE_INTEG_TIME_ADDR        (0x0200)
#define SENSOR_IMX582_COARSE_INTEG_TIME_ADDR      (0x0202)
#define SENSOR_IMX582_ANALOG_GAIN_ADDR            (0x0204)
#define SENSOR_IMX582_DIG_GAIN_ADDR               (0x020E)

#define SENSOR_IMX582_MIN_ANALOG_GAIN_SET_VALUE   (112)
#define SENSOR_IMX582_MAX_ANALOG_GAIN_SET_VALUE   (1008)
#define SENSOR_IMX582_MIN_DIGITAL_GAIN_SET_VALUE  (0x0100)
#define SENSOR_IMX582_MAX_DIGITAL_GAIN_SET_VALUE  (0x0FFF)
#define SENSOR_IMX582_FULL_PIXEL_MAX_ANALOG_GAIN_SET_VALUE   (960)

#define SENSOR_IMX582_ABS_GAIN_GR_SET_ADDR        (0x0B8E)
#define SENSOR_IMX582_ABS_GAIN_R_SET_ADDR         (0x0B90)
#define SENSOR_IMX582_ABS_GAIN_B_SET_ADDR         (0x0B92)
#define SENSOR_IMX582_ABS_GAIN_GB_SET_ADDR        (0x0B94)


/* Related EEPROM CAL */
#define SENSOR_IMX582_LRC_CAL_BASE_REAR           (0x01D0)
#define SENSOR_IMX582_LRC_CAL_SIZE                (384)
#define SENSOR_IMX582_LRC_REG_ADDR                (0x7b10)

#define SENSOR_IMX582_QUAD_SENS_CAL_BASE_REAR     (0x0350)
#define SENSOR_IMX582_QUAD_SENS_CAL_SIZE          (2304)
#define SENSOR_IMX582_QUAD_SENS_REG_ADDR          (0xC500)

#define SENSOR_IMX582_LRC_DUMP_NAME               "/data/vendor/camera/IMX582_LRC_DUMP.bin"
#define SENSOR_IMX582_QSC_DUMP_NAME               "/data/vendor/camera/IMX582_QSC_DUMP.bin"


/* Related Function Option */
#define SENSOR_IMX582_WRITE_PDAF_CAL              (1) /* 0 for bringup */
#define SENSOR_IMX582_WRITE_SENSOR_CAL            (1) /* 0 for bringup */
#define SENSOR_IMX582_CAL_DEBUG                   (0)
#define SENSOR_IMX582_DEBUG_INFO                  (0)


/*
 * [Mode Information]
 *
 * Reference File : IMX582_SEC-DPHY-26MHz_RegisterSetting_ver2.1x-4.00_b1_190611_test.xlsx
 * Update Data    : 2019-06-11
 * Author         : takkyoum.kim
 *
 * - Global Setting -
 *
 * - Remosaic Full For Single Still Remosaic Capture -
 *    [  0 ] REG_G   : Remosaic Full 8000x6000 15fps       : Single Still Remosaic Capture (4:3)   ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2184
 *
 * - 2x2 BIN For Still Preview / Capture -
 *    [  1 ] REG_H   : 2x2 Binning Full 4000x3000 30fps    : Single Still Preview/Capture (4:3)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  2 ] REG_I   : 2x2 Binning Crop 4000X2256 30fps    : Single Still Preview/Capture (16:9)   ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  3 ] REG_J   : 2x2 Binning Crop 4000X1952 30fps    : Single Still Preview/Capture (18.5:9) ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  4 ] REG_K   : 2x2 Binning Crop 4000X1844 30fps    : Single Still Preview/Capture (19.5:9) ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  5 ] REG_L   : 2x2 Binning Crop 4000X1800 30fps    : Single Still Preview/Capture (20:9)   ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  6 ] REG_M   : 2x2 Binning Crop 3664X3000 30fps    : MMS (11:9)                            ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *    [  7 ] REG_N   : 2x2 Binning Crop 3000X3000 30fps    : Single Still Preview/Capture (1:1)    ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 2058
 *
 * - 2x2 BIN V2H2 For HighSpeed Recording/FastAE-
 *    [  8 ] REG_O   : 2x2 Binning V2H2 2000X1500 120fps   : FAST AE (4:3)                         ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 1848
 *    [  9 ] REG_P   : 2x2 Binning V2H2 2000X1128 120fps   : High Speed Recording (16:9)           ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 1848
 *
 * - 2x2 BIN V2H2 For HighSpeed Recording -
 *    [ 10 ] REG_Q   : 2x2 Binning V2H2 1296X736  240fps   : High Speed Recording (16:9)           ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 1848
 *    [ 11 ] REG_R   : 2x2 Binning V2H2 2000X1128 240fps   : High Speed Recording (16:9)           ,  MIPI lane: 4, MIPI data rate(Mbps/lane): 1848
 *
 */

enum sensor_imx582_mode_enum {

	/* 2x2 Binning 30Fps */
	SENSOR_IMX582_2X2BIN_FULL_4000X3000_30FPS = 0,
	SENSOR_IMX582_2X2BIN_CROP_4000X2256_30FPS,
	SENSOR_IMX582_2X2BIN_CROP_4000X1800_30FPS,

	/* 2X2 Binning V2H2 120FPS */
	SENSOR_IMX582_2X2BIN_V2H2_2000X1128_120FPS,

	SENSOR_IMX582_2X2BIN_V2H2_2000X1128_240FPS,
	SENSOR_IMX582_2X2BIN_V2H2_2000X1500_120FPS,
	SENSOR_IMX582_2X2BIN_CROP_3168X1780_60FPS,
	IMX582_MODE_REMOSAIC_START,
	SENSOR_IMX582_REMOSAIC_FULL_8000x6000_15FPS = IMX582_MODE_REMOSAIC_START,
	SENSOR_IMX582_REMOSAIC_CROP_4000x3000_30FPS,
	SENSOR_IMX582_REMOSAIC_CROP_4000x2256_30FPS,
	IMX582_MODE_REMOSAIC_END = SENSOR_IMX582_REMOSAIC_CROP_4000x2256_30FPS,
};

enum sensor_imx582_chip_id_ver_enum {
	SENSOR_IMX582_CHIP_ID_VER_5_0_0X02 = 0x02,
	SENSOR_IMX582_CHIP_ID_VER_5_0_0X08 = 0x08,
	SENSOR_IMX582_CHIP_ID_VER_5_0_0X09 = 0x09,
	SENSOR_IMX582_CHIP_ID_VER_5_0_0X10 = 0x10,
	SENSOR_IMX582_CHIP_ID_VER_5_0_0X18 = 0x18,
	SENSOR_IMX582_CHIP_ID_VER_6_0_0X1B = 0x1B,
	SENSOR_IMX582_CHIP_ID_VER_6_0_0X20 = 0x20,
};

#endif


/* IS_REMOSAIC_MODE(struct is_cis *cis) */
#define IS_REMOSAIC(mode) ({                                            \
	typecheck(u32, mode) && (mode >= IMX582_MODE_REMOSAIC_START) && \
	(mode <= IMX582_MODE_REMOSAIC_END);                             \
})

#define IS_REMOSAIC_CROP_ZOOM(mode) ({                                                          \
	typecheck(u32, mode) && (mode >= SENSOR_IMX582_REMOSAIC_CROP_4000x3000_30FPS) &&        \
	(mode <= IMX582_MODE_REMOSAIC_END);                                                     \
})

#define IS_REMOSAIC_MODE(cis) ({                                        \
	u32 m;                                                          \
	typecheck(struct is_cis *, cis);                           \
	m = cis->cis_data->sens_config_index_cur;                       \
	(m >= IMX582_MODE_REMOSAIC_START) && (m <= IMX582_MODE_REMOSAIC_END); \
})
