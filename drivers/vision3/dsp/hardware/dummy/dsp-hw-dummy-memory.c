// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-dummy-memory.h"

int dsp_hw_dummy_memory_ion_alloc_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	dsp_check();
	return 0;
}

void dsp_hw_dummy_memory_ion_free_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	dsp_check();
}

int dsp_hw_dummy_memory_map_reserved_mem(struct dsp_memory *mem,
		struct dsp_reserved_mem *rmem)
{
	dsp_check();
	return 0;
}

void dsp_hw_dummy_memory_unmap_reserved_mem(struct dsp_memory *mem,
		struct dsp_reserved_mem *rmem)
{
	dsp_check();
}
