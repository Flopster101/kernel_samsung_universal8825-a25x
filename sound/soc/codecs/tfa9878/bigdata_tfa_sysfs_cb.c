/*
 *  bigdata_tfa_sysfs_cb.c
 *  Copyright (c) Samsung Electronics
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 */

#include <sound/soc.h>
#include <sound/samsung/sec_audio_sysfs.h>
#include "bigdata_tfa_sysfs_cb.h"
#include <sound/tfa_ext.h>

static struct snd_soc_component *tfa98xx_component;

/*
 * Index 0: maximum x (in um); e.g. 174 for 0.174 mm
 * Index 1: maximum t (in degC)
 * Index 2: count for x > x_max
 * Index 3: count for t > t_max
 * Index 4: keep maximum x
 * Index 5: keep maximum t
 */
enum tfa_bigdata {
	TFA_MAX_X,
	TFA_MAX_T,
	TFA_MAX_X_COUNT,
	TFA_MAX_T_COUNT,
	TFA_MAX_X_KEEP,
	TFA_MAX_T_KEEP,
	TFA_BIGDATA_MAX,
};

#define RESET 1

static int get_tfa98xx_amp_temperature_max(enum amp_id id)
{
	struct snd_soc_component *component = tfa98xx_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = tfa98xx_get_blackbox_data_index(id, TFA_MAX_T, RESET);
	if (value < 0) {
		dev_err(component->dev, "%s: invalid value(%d)\n", __func__, value);
		return -EINVAL;
	}

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_tfa98xx_amp_temperature_keep_max(enum amp_id id)
{
	struct snd_soc_component *component = tfa98xx_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = tfa98xx_get_blackbox_data_index(id, TFA_MAX_T_KEEP, RESET);
	if (value < 0) {
		dev_err(component->dev, "%s: invalid value(%d)\n", __func__, value);
		return -EINVAL;
	}

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_tfa98xx_amp_temperature_overcount(enum amp_id id)
{
	struct snd_soc_component *component = tfa98xx_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = tfa98xx_get_blackbox_data_index(id, TFA_MAX_T_COUNT, RESET);
	if (value < 0) {
		dev_err(component->dev, "%s: invalid value(%d)\n", __func__, value);
		return -EINVAL;
	}

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_tfa98xx_amp_excursion_max(enum amp_id id)
{
	struct snd_soc_component *component = tfa98xx_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = tfa98xx_get_blackbox_data_index(id, TFA_MAX_X, RESET);
	if (value < 0) {
		dev_err(component->dev, "%s: invalid value(%d)\n", __func__, value);
		return -EINVAL;
	}

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_tfa98xx_amp_excursion_overcount(enum amp_id id)
{
	struct snd_soc_component *component = tfa98xx_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = tfa98xx_get_blackbox_data_index(id, TFA_MAX_X_COUNT, RESET);
	if (value < 0) {
		dev_err(component->dev, "%s: invalid value(%d)\n", __func__, value);
		return -EINVAL;
	}

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_tfa98xx_amp_curr_temperature(enum amp_id id)
{
	struct snd_soc_component *component = tfa98xx_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	if (id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	value = tfa98xx_update_spkt_data(id);
	if (value < 0) {
		dev_err(component->dev, "%s: invalid value(%d)\n", __func__, value);
		return -EINVAL;
	}

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int set_tfa98xx_amp_surface_temperature(enum amp_id id, int temperature)
{
	struct snd_soc_component *component = tfa98xx_component;
	int ret = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	dev_info(component->dev, "%s: id %d temperature %d\n", __func__, id, temperature);

	if (id >=  AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id(%d)\n", __func__, id);
		return -EINVAL;
	}

	ret = tfa98xx_write_sknt_control(id, temperature);
	if (ret < 0) {
		dev_err(component->dev, "%s: invalid ret(%d)\n", __func__, ret);
		return -EINVAL;
	}

	return ret;
}

void register_tfa98xx_bigdata_cb(struct snd_soc_component *component)
{
	tfa98xx_component = component;
	dev_info(component->dev, "%s\n", __func__);

	audio_register_temperature_max_cb(get_tfa98xx_amp_temperature_max);
	audio_register_temperature_keep_max_cb(get_tfa98xx_amp_temperature_keep_max);
	audio_register_temperature_overcount_cb(get_tfa98xx_amp_temperature_overcount);
	audio_register_excursion_max_cb(get_tfa98xx_amp_excursion_max);
	audio_register_excursion_overcount_cb(get_tfa98xx_amp_excursion_overcount);
	audio_register_curr_temperature_cb(get_tfa98xx_amp_curr_temperature);
	audio_register_surface_temperature_cb(set_tfa98xx_amp_surface_temperature);
}
EXPORT_SYMBOL_GPL(register_tfa98xx_bigdata_cb);
