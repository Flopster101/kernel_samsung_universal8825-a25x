/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts_res.h
 *
 * ALSA SoC - Samsung Abox Debug driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_RES_H
#define __SND_SOC_VTS_RES_H

#include "vts.h"

extern struct platform_driver samsung_vts_s_lif_driver;
extern struct platform_driver samsung_vts_dma_driver;

extern int vts_clk_set_rate(struct device *dev, unsigned long combination);
extern void vts_set_sel_pad(struct device *dev, int enable);
extern void vts_port_init(struct device *dev);
extern int vts_port_cfg(struct device *dev, enum vts_port_pad pad,
		enum vts_port_id id, enum vts_dmic_sel gpio_func, bool enable);
#endif /* __SND_SOC_VTS_RES_H */
