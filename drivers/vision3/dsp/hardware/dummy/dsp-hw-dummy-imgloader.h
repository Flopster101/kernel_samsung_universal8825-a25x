/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_IMGLOADER_H__
#define __HW_DUMMY_DSP_HW_DUMMY_IMGLOADER_H__

#include "dsp-imgloader.h"

int dsp_hw_dummy_imgloader_boot(struct dsp_imgloader *imgloader);
int dsp_hw_dummy_imgloader_shutdown(struct dsp_imgloader *imgloader);

int dsp_hw_dummy_imgloader_probe(struct dsp_imgloader *imgloader, void *sys);
void dsp_hw_dummy_imgloader_remove(struct dsp_imgloader *imgloader);

int dsp_hw_dummy_imgloader_register_ops(unsigned int dev_id);

#endif
