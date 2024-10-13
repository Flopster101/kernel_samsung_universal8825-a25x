/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_BUS_H__
#define __HW_COMMON_DSP_HW_COMMON_BUS_H__

#include "dsp-config.h"
#include "dsp-bus.h"

#ifndef ENABLE_DSP_VELOCE
#include <soc/samsung/bts.h>
#else
#define bts_del_scenario(x)
#define bts_add_scenario(x)
#define bts_get_scenindex(x)	(1)
#endif

int dsp_hw_common_bus_mo_get_by_name(struct dsp_bus *bus, unsigned char *name);
int dsp_hw_common_bus_mo_put_by_name(struct dsp_bus *bus, unsigned char *name);
int dsp_hw_common_bus_mo_get_by_id(struct dsp_bus *bus, unsigned int id);
int dsp_hw_common_bus_mo_put_by_id(struct dsp_bus *bus, unsigned int id);
int dsp_hw_common_bus_mo_all_put(struct dsp_bus *bus);

int dsp_hw_common_bus_open(struct dsp_bus *bus);
int dsp_hw_common_bus_close(struct dsp_bus *bus);
int dsp_hw_common_bus_probe(struct dsp_bus *bus, void *sys);
void dsp_hw_common_bus_remove(struct dsp_bus *bus);

int dsp_hw_common_bus_set_ops(struct dsp_bus *bus, const void *ops);

#endif
