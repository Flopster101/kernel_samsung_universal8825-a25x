// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ALSA SoC - Samsung Abox SoC dependent layer
 *
 * Copyright (c) 2018 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/regmap.h>
#include "abox.h"
#include "abox_soc.h"

int abox_soc_ver(struct device *adev)
{
	struct abox_data *data = dev_get_drvdata(adev);

	return readl(data->sfr_base + ABOX_VERSION) >> ABOX_VERSION_L;
}

int abox_soc_vercmp(struct device *adev, int m, int n, int r)
{
	return ABOX_SOC_VERSION(m, n, r) - abox_soc_ver(adev);
}

static bool volatile_reg(struct device *dev, unsigned int reg)
{
	if (writeonly_reg(reg) || readonly_reg(reg) || shared_reg(reg))
		return true;

	return false;
}

static bool readable_reg(struct device *dev, unsigned int reg)
{
	return accessible_reg(reg);
}

static bool writeable_reg(struct device *dev, unsigned int reg)
{
	if (readonly_reg(reg))
		return false;
	else
		return accessible_reg(reg);
}

static struct regmap_config regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.volatile_reg = volatile_reg,
	.readable_reg = readable_reg,
	.writeable_reg = writeable_reg,
	.max_register = ABOX_MAX_REGISTERS,
	.num_reg_defaults_raw = ABOX_MAX_REGISTERS + 1,
	.cache_type = REGCACHE_RBTREE,
	.fast_io = true,
};

struct regmap *abox_soc_get_regmap(struct device *adev)
{
	struct abox_data *data = dev_get_drvdata(adev);
	struct regmap *regmap;

	if (!IS_ERR_OR_NULL(data->regmap))
		return data->regmap;

	regmap = devm_regmap_init_mmio(adev, data->sfr_base, &regmap_config);
	apply_patch(regmap);

	return regmap;
}
