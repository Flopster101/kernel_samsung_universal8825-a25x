/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_MEMORY_H__
#define __HW_DUMMY_DSP_HW_DUMMY_MEMORY_H__

#include "dsp-memory.h"

int dsp_hw_dummy_memory_ion_alloc_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);
void dsp_hw_dummy_memory_ion_free_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);

int dsp_hw_dummy_memory_map_reserved_mem(struct dsp_memory *mem,
		struct dsp_reserved_mem *rmem);
void dsp_hw_dummy_memory_unmap_reserved_mem(struct dsp_memory *mem,
		struct dsp_reserved_mem *rmem);

#endif
