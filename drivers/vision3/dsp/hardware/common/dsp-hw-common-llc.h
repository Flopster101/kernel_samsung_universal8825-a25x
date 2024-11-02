/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_LLC_H__
#define __HW_COMMON_DSP_HW_COMMON_LLC_H__

#include "dsp-config.h"
#include "dsp-llc.h"

#if IS_ENABLED(CONFIG_EXYNOS_SCI)
#include <soc/samsung/exynos-sci.h>
#endif

int dsp_hw_common_llc_get_by_name(struct dsp_llc *llc, unsigned char *name);
int dsp_hw_common_llc_put_by_name(struct dsp_llc *llc, unsigned char *name);
int dsp_hw_common_llc_get_by_id(struct dsp_llc *llc, unsigned int id);
int dsp_hw_common_llc_put_by_id(struct dsp_llc *llc, unsigned int id);
int dsp_hw_common_llc_all_put(struct dsp_llc *llc);

int dsp_hw_common_llc_open(struct dsp_llc *llc);
int dsp_hw_common_llc_close(struct dsp_llc *llc);
int dsp_hw_common_llc_probe(struct dsp_llc *llc, void *sys);
void dsp_hw_common_llc_remove(struct dsp_llc *llc);

int dsp_hw_common_llc_set_ops(struct dsp_llc *llc, const void *ops);

#endif
