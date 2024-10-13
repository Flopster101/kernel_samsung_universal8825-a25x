/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_MEMORY_H__
#define __HW_COMMON_DSP_HW_COMMON_MEMORY_H__

#include "dsp-memory.h"

int dsp_hw_common_memory_map_buffer(struct dsp_memory *mem,
		struct dsp_buffer *buf);
int dsp_hw_common_memory_unmap_buffer(struct dsp_memory *mem,
		struct dsp_buffer *buf);
int dsp_hw_common_memory_sync_for_device(struct dsp_memory *mem,
		struct dsp_buffer *buf);
int dsp_hw_common_memory_sync_for_cpu(struct dsp_memory *mem,
		struct dsp_buffer *buf);

int dsp_hw_common_memory_ion_alloc(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);
void dsp_hw_common_memory_ion_free(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);
int dsp_hw_common_memory_ion_alloc_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);
void dsp_hw_common_memory_ion_free_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem);

int dsp_hw_common_memory_map_reserved_mem(struct dsp_memory *mem,
		struct dsp_reserved_mem *rmem);
void dsp_hw_common_memory_unmap_reserved_mem(struct dsp_memory *mem,
		struct dsp_reserved_mem *rmem);

int dsp_hw_common_memory_open(struct dsp_memory *mem);
int dsp_hw_common_memory_close(struct dsp_memory *mem);
int dsp_hw_common_memory_probe(struct dsp_memory *mem, void *sys);
void dsp_hw_common_memory_remove(struct dsp_memory *mem);

int dsp_hw_common_memory_set_ops(struct dsp_memory *mem, const void *ops);

#endif
