/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SHUB_LIGHT_H_
#define __SHUB_LIGHT_H_

#include <linux/types.h>
#include <linux/device.h>

#define LIGHT_COEF_SIZE 7

#ifdef CONFIG_SHUB_TEST_FOR_ONLY_UML
#define LIGHT_CALIBRATION_FILE_PATH "calibration_data.txt"
#else
#define LIGHT_CALIBRATION_FILE_PATH "/efs/FactoryApp/light_cal_data"
#endif

#define PANEL_TYPE_FILE_PATH "sys/class/lcd/panel/lcd_type"
#define LIGHT_DEBIG_EVENT_SIZE_4BYTE_VERSION	2000
#define LIGHT_CAL_CH0_SIZE_4BYTE_VERSION		3000

struct light_event {
	u32 lux;	/* lux, cct, raw_lux : fix 4byte */
	s32 cct;
	u32 raw_lux;
	s32 r;		/* r, g, b, w, a_time, a_gain : 2byte or 4byte */
	s32 g;		/* LIGHT_DEBIG_EVENT_SIZE_4BYTE_VERSION : 4byte, others : 2byte */
	s32 b;
	s32 w;
	s32 a_time;
	s32 a_gain;
	u32 brightness;
} __attribute__((__packed__));

struct light_cct_event {
	u32 lux;	/* lux, cct, raw_lux : fix 4byte */
	s32 cct;
	u32 raw_lux;
	u16 roi;	/* roi : ddi_support is supported */
	u32 r;		/* r, g, b, w, a_time, a_gain : 2byte or 4byte */
	u32 g;		/* LIGHT_DEBIG_EVENT_SIZE_4BYTE_VERSION : 4byte, others : 2byte */
	u32 b;
	u32 w;
	u32 a_time;
	u32 a_gain;
} __attribute__((__packed__));

struct light_cal_data {
	u8 result;
	u32 max;
	u32 lux;
} __attribute__((__packed__));

struct light_cal_data_legacy {
	u8 result;
	u16 max;
	u32 lux;
} __attribute__((__packed__));

struct light_data {
	int *light_coef;
	int light_log_cnt;
	int brightness;
	int last_brightness_level;
	int brightness_array_len;
	u32 *brightness_array;
	int raw_data_size;
	int panel_vendor;
	bool ddi_support;
	bool use_cal_data;
	bool hbm_finger;
	bool light_dual;
	struct light_cal_data cal_data;
};

void set_light_ddi_support(uint32_t system_feature);


/* light sub command */
#define LIGHT_SUBCMD_TWO_LIGHT_FACTORY_TEST		130
#define LIGHT_SUBCMD_TRIM_CHECK					131
// reserved subcmd from sensorhub				132
// reserved subcmd from sensorhub				133
#define LIGHT_SUBCMD_BRIGHTNESS_HYSTERESIS		134
#define LIGHT_SUBCMD_HBM_FINGERPRINT			135
#define LIGHT_SUBCMD_PANEL_TYPE					136
// reserved subcmd from sensorhub				137
// reserved subcmd from sensorhub				138
#define LIGHT_SUBCMD_PANEL_INFORMATION			139
#define LIGHT_SUBCMD_UB_CONNECTED				140
#define LIGHT_SUBCMD_SCREEN_MODE_INFORMATION	141
struct sensor_chipset_init_funcs *get_light_stk_common_function_pointer(char *name);

#endif /* __SHUB_LIGHT_H_ */
