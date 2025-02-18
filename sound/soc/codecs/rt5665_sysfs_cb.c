/*
 *  rt5665_sysfs_cb.c
 *  Copyright (c) Samsung Electronics
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 */

#include <linux/input.h>

#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/samsung/sec_audio_sysfs.h>

#include "rt5665.h"
#include "rt5665_sysfs_cb.h"

static struct snd_soc_component *rt5665_component;

static int get_jack_status(void)
{
	struct snd_soc_component *component = rt5665_component;
	struct rt5665_priv *rt5665 = snd_soc_component_get_drvdata(component);
	int status = rt5665->jack_type;
	int report = 0;

	if (status) {
	    report = 1;
	}

	dev_info(component->dev, "%s: %d\n", __func__, report);

	return report;
}

static int get_key_status(void)
{
	struct snd_soc_component *component = rt5665_component;
	struct rt5665_priv *rt5665 = snd_soc_component_get_drvdata(component);
	int report = 0;

	report = rt5665->btn_det;

	dev_info(component->dev, "%s: %d\n", __func__, report);

	return report;
}

static int get_mic_adc(void)
{
	struct snd_soc_component *component = rt5665_component;
	struct rt5665_priv *rt5665 = snd_soc_component_get_drvdata(component);
	int adc = 0;

	adc = rt5665->adc_val;

	dev_info(component->dev, "%s: %d\n", __func__, adc);

	return adc;
}

void register_rt5665_sysfs_cb(struct snd_soc_component *component)
{
	rt5665_component = component;

	dev_info(component->dev, "%s\n", __func__);

	audio_register_jack_state_cb(get_jack_status);
	audio_register_key_state_cb(get_key_status);
	audio_register_mic_adc_cb(get_mic_adc);
}
EXPORT_SYMBOL_GPL(register_rt5665_sysfs_cb);
