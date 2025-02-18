/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ALSA SoC - Samsung Abox UAIF/DSIF driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_IF_H
#define __SND_SOC_ABOX_IF_H

#include "abox.h"

#define UAIF_REG_CTRL0	0x0
#define UAIF_REG_CTRL1	0x4
#define UAIF_REG_CTRL2	0x8
#define UAIF_REG_SPK_VOL_FACTOR	0xc
#define UAIF_REG_SPK_VOL_CHANGE	0x10
#define UAIF_REG_MIC_VOL_FACTOR	0x14
#define UAIF_REG_MIC_VOL_CHANGE	0x18
#if (ABOX_SOC_VERSION(4, 0, 0) < CONFIG_SND_SOC_SAMSUNG_ABOX_VERSION)
#define UAIF_REG_STATUS	0x20
#else
#define UAIF_REG_STATUS	0xc
#endif
#define UAIF_REG_MAX	UAIF_REG_STATUS

#define DSIF_REG_CTRL	0x0
#define DSIF_REG_STATUS	0x4
#define DSIF_REG_MAX	DSIF_REG_STATUS

#define SPDY_REG_CTRL	0x0
#define SPDY_REG_MAX	SPDY_REG_CTRL

enum abox_if_config {
	ABOX_IF_WIDTH,
	ABOX_IF_CHANNEL,
	ABOX_IF_RATE,
	ABOX_IF_FMT_COUNT,
};

enum abox_if_irq {
	ABOX_IF_IRQ_SPEAKER,
	ABOX_IF_IRQ_MIC,
	ABOX_IF_IRQ_HOLD,
	ABOX_IF_IRQ_RESUME,
	ABOX_IF_IRQ_SPK_FADE_DONE,
	ABOX_IF_IRQ_MIC_FADE_DONE,
	ABOX_IF_IRQ_COUNT,
};

struct abox_if_of_data {
	enum abox_irq (*get_irq)(int id, enum abox_if_irq irq);
	enum abox_dai (*get_dai_id)(int id);
	const char *(*get_dai_name)(int id);
	unsigned int (*get_reg_base)(int id);
	struct snd_soc_dai_driver *base_dai_drv;
};

struct abox_if_data {
	int id;
	bool slave;
	unsigned int base;
	void __iomem *sfr_base;
	struct clk *clk_bclk;
	struct clk *clk_bclk_gate;
	struct clk *clk_mux;
	struct snd_soc_component *cmpnt;
	struct snd_soc_dai_driver *dai_drv;
	struct abox_data *abox_data;
	const struct abox_if_of_data *of_data;
	unsigned int config[ABOX_IF_FMT_COUNT];
	unsigned int rconfig[ABOX_IF_FMT_COUNT];
	unsigned long quirks;
	bool extend_bclk;
	bool enable_bclk; /* force enable */
	bool mic_auto_fade_in;
	bool spk_auto_fade_in;
	struct completion mic_func_changed;
	struct completion spk_func_changed;
};

/**
 * UAIF/DSIF hw params fixup helper by dai
 * @param[in]	dai	snd_soc_dai
 * @param[out]	params	snd_pcm_hw_params
 * @return		error code if any
 */
extern int abox_if_hw_params_fixup(struct snd_soc_dai *dai,
		struct snd_pcm_hw_params *params);

/**
 * UAIF/DSIF hw params fixup helper
 * @param[in]	rtd	snd_soc_pcm_runtime
 * @param[out]	params	snd_pcm_hw_params
 * @return		error code if any
 */
extern int abox_if_hw_params_fixup_helper(struct snd_soc_pcm_runtime *rtd,
		struct snd_pcm_hw_params *params);

#endif /* __SND_SOC_ABOX_IF_H */
