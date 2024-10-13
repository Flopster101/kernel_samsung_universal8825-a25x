
/* sound/soc/samsung/vts/slif_nm.h
 *
 * ALSA SoC - Samsung VTS Serial Local Interface driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_SLIF_LIF_NM_H
#define __SND_SOC_SLIF_LIF_NM_H

int slif_cfg_gpio(struct device *dev, const char *name);
void slif_dmic_aud_en(struct device *dev, int en);
void slif_dmic_if_en(struct device *dev, int en);

#endif /* __SND_SOC_SLIF_LIF_NM_H */
