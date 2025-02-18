/*
 * linux/sound/rt5691.h -- Platform data for RT5691
 *
 * Copyright 2022 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_SND_RT5691_H__
#define __LINUX_SND_RT5691_H__

enum rt5691_jd_src {
	RT5691_JD_NULL,
	RT5691_JD1,
	RT5691_JD1_JD2,
};

enum rt5691_gpio2_func {
	RT5691_GPIO2,
	RT5691_DMIC_SCL,
	RT5691_PDM_SCL_IN,
	RT5691_PDM_SCL_OUT,
};

enum rt5691_gpio3_func {
	RT5691_GPIO3,
	RT5691_DMIC_SDA,
	RT5691_PDM_SDA_OUT,
};

struct rt5691_platform_data {
	bool in1_diff;
	bool in2_diff;
	bool in3_diff;

	int ldo1_en, gpio_1v8, gpio_3v3;

	enum rt5691_jd_src jd_src;
	enum rt5691_gpio2_func gpio2_func;
	enum rt5691_gpio3_func gpio3_func;

	unsigned int delay_plug_in;
	unsigned int delay_plug_out_pb;

	unsigned int sar_hs_type;
	unsigned int sar_hs_open_gender;
	unsigned int sar_pb_vth0;
	unsigned int sar_pb_vth1;
	unsigned int sar_pb_vth2;
	unsigned int sar_pb_vth3;
	unsigned int jd_resistor;
	unsigned int button_clk;
	unsigned int hpa_capless_bias;

	unsigned int i2c_op_count;
};

#endif

