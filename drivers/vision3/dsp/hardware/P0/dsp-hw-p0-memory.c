// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-p0-system.h"
#include "dsp-hw-common-memory.h"
#include "dsp-hw-p0-memory.h"

static int __dsp_hw_p0_memory_map_reserved_mem(struct dsp_memory *mem)
{
	int ret;
	struct dsp_reserved_mem *rmem;
	int idx;

	dsp_enter();
	rmem = &mem->reserved_mem[DSP_P0_RESERVED_MEM_FW_MASTER];
	snprintf(rmem->name, DSP_MEM_NAME_LEN, "fw_master");

	if (rmem->size < DSP_P0_MASTER_FW_SIZE +
			(ALIGN(rmem->paddr, SZ_64K) - rmem->paddr)) {
		ret = -EFAULT;
		dsp_err("reserved_mem[%s] size is invalid(%llu/%d/%llu)\n",
				rmem->name, rmem->size, DSP_P0_MASTER_FW_SIZE,
				ALIGN(rmem->paddr, SZ_64K) - rmem->paddr);
		goto p_err;
	}

	rmem->paddr = ALIGN(rmem->paddr, SZ_64K);
	rmem->size = DSP_P0_MASTER_FW_SIZE;
	rmem->iova = DSP_P0_MASTER_FW_IOVA;
	rmem->kmap = true;

	for (idx = 0; idx < mem->reserved_mem_count; ++idx) {
		ret = mem->ops->map_reserved_mem(mem, &mem->reserved_mem[idx]);
		if (ret)
			goto p_err_map;
	}

	dsp_leave();
	return 0;
p_err_map:
	for (idx -= 1; idx >= 0; --idx)
		mem->ops->unmap_reserved_mem(mem, &mem->reserved_mem[idx]);
p_err:
	return ret;
}

static int dsp_hw_p0_memory_probe(struct dsp_memory *mem, void *sys)
{
	int ret;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	mem->priv_mem_count = DSP_P0_PRIV_MEM_COUNT;
	mem->reserved_mem_count = DSP_P0_RESERVED_MEM_COUNT;

	ret = dsp_hw_common_memory_probe(mem, sys);
	if (ret) {
		dsp_err("Failed to probe common memory(%d)\n", ret);
		goto p_err;
	}

	pmem = &mem->priv_mem[DSP_P0_PRIV_MEM_FW];
	snprintf(pmem->name, DSP_MEM_NAME_LEN, "fw");
	pmem->size = PAGE_ALIGN(DSP_P0_MEMORY_MAX_SIZE - DSP_P0_MASTER_FW_SIZE);
	pmem->min_size = PAGE_ALIGN(DSP_P0_FW_SIZE);
	pmem->max_size =
		PAGE_ALIGN(DSP_P0_MEMORY_MAX_SIZE - DSP_P0_MASTER_FW_SIZE);
	pmem->flags = 0;
	pmem->dir = DMA_TO_DEVICE;
	pmem->kmap = true;
	pmem->fixed_iova = true;
	pmem->iova = DSP_P0_FW_IOVA;
	pmem->backup = true;

	pmem = &mem->priv_mem[DSP_P0_PRIV_MEM_DHCP];
	snprintf(pmem->name, DSP_MEM_NAME_LEN, "dhcp_mem");
	pmem->size = PAGE_ALIGN(DSP_P0_DHCP_MEM_SIZE);
	pmem->min_size = PAGE_ALIGN(SZ_4K);
	pmem->max_size = PAGE_ALIGN(DSP_P0_MEMORY_MAX_SIZE);
	pmem->flags = 0;
	pmem->dir = DMA_BIDIRECTIONAL;
	pmem->kmap = true;
	pmem->fixed_iova = true;
	pmem->iova = DSP_P0_DHCP_MEM_IOVA;
	pmem->backup = false;

	pmem = &mem->priv_mem[DSP_P0_PRIV_MEM_FW_LOG];
	snprintf(pmem->name, DSP_MEM_NAME_LEN, "fw_log");
	pmem->size = PAGE_ALIGN(DSP_P0_FW_LOG_SIZE);
	pmem->min_size = PAGE_ALIGN(DSP_P0_FW_LOG_SIZE);
	pmem->max_size = PAGE_ALIGN(DSP_P0_MEMORY_MAX_SIZE);
	pmem->flags = 0;
	pmem->dir = DMA_BIDIRECTIONAL;
	pmem->kmap = true;
	pmem->fixed_iova = true;
	pmem->iova = DSP_P0_FW_LOG_IOVA;
	pmem->backup = false;

	pmem = &mem->priv_mem[DSP_P0_PRIV_MEM_IVP_PM];
	snprintf(pmem->name, DSP_MEM_NAME_LEN, "ivp_pm");
	pmem->size = PAGE_ALIGN(DSP_P0_IVP_PM_SIZE);
	pmem->min_size = PAGE_ALIGN(DSP_P0_IVP_PM_SIZE);
	pmem->max_size = PAGE_ALIGN(DSP_P0_MEMORY_MAX_SIZE);
	pmem->flags = 0;
	pmem->dir = DMA_TO_DEVICE;
	pmem->kmap = true;
	pmem->fixed_iova = true;
	pmem->iova = DSP_P0_IVP_PM_IOVA;
	pmem->backup = false;

	pmem = &mem->priv_mem[DSP_P0_PRIV_MEM_IVP_DM];
	snprintf(pmem->name, DSP_MEM_NAME_LEN, "ivp_dm");
	pmem->size = PAGE_ALIGN(DSP_P0_IVP_DM_SIZE);
	pmem->min_size = PAGE_ALIGN(DSP_P0_IVP_DM_SIZE);
	pmem->max_size = PAGE_ALIGN(DSP_P0_MEMORY_MAX_SIZE);
	pmem->flags = 0;
	pmem->dir = DMA_TO_DEVICE;
	pmem->kmap = true;
	pmem->fixed_iova = true;
	pmem->iova = DSP_P0_IVP_DM_IOVA;
	pmem->backup = false;

	pmem = &mem->priv_mem[DSP_P0_PRIV_MEM_MBOX_MEMORY];
	snprintf(pmem->name, DSP_MEM_NAME_LEN, "mbox_memory");
	pmem->size = PAGE_ALIGN(DSP_P0_MBOX_MEMORY_SIZE);
	pmem->min_size = PAGE_ALIGN(DSP_P0_MBOX_MEMORY_SIZE);
	pmem->max_size = PAGE_ALIGN(DSP_P0_MEMORY_MAX_SIZE);
	pmem->flags = 0;
	pmem->dir = DMA_BIDIRECTIONAL;
	pmem->kmap = true;
	pmem->fixed_iova = true;
	pmem->iova = DSP_P0_MBOX_MEMORY_IOVA;
	pmem->backup = false;

	pmem = &mem->priv_mem[DSP_P0_PRIV_MEM_MBOX_POOL];
	snprintf(pmem->name, DSP_MEM_NAME_LEN, "mbox_pool");
	pmem->size = PAGE_ALIGN(DSP_P0_MBOX_POOL_SIZE);
	pmem->min_size = PAGE_ALIGN(DSP_P0_MBOX_POOL_SIZE);
	pmem->max_size = PAGE_ALIGN(DSP_P0_MEMORY_MAX_SIZE);
	pmem->flags = 0;
	pmem->dir = DMA_BIDIRECTIONAL;
	pmem->kmap = true;
	pmem->fixed_iova = true;
	pmem->iova = DSP_P0_MBOX_POOL_IOVA;
	pmem->backup = false;

	pmem = &mem->priv_mem[DSP_P0_PRIV_MEM_DL_OUT];
	snprintf(pmem->name, DSP_MEM_NAME_LEN, "dl_out");
	pmem->size = PAGE_ALIGN(DSP_P0_DL_OUT_SIZE);
	pmem->min_size = PAGE_ALIGN(DSP_P0_DL_OUT_SIZE);
	pmem->max_size = PAGE_ALIGN(DSP_P0_MEMORY_MAX_SIZE);
	pmem->flags = 0;
	pmem->dir = DMA_BIDIRECTIONAL;
	pmem->kmap = true;
	pmem->fixed_iova = true;
	pmem->iova = DSP_P0_DL_OUT_IOVA;
	pmem->backup = false;

	mem->shared_id.fw_master = DSP_P0_RESERVED_MEM_FW_MASTER;
	mem->shared_id.ivp_pm = DSP_P0_PRIV_MEM_IVP_PM;
	mem->shared_id.dl_out = DSP_P0_PRIV_MEM_DL_OUT;
	mem->shared_id.mbox_pool = DSP_P0_PRIV_MEM_MBOX_POOL;
	mem->shared_id.fw_log = DSP_P0_PRIV_MEM_FW_LOG;

	ret = __dsp_hw_p0_memory_map_reserved_mem(mem);
	if (ret)
		goto p_err_rmem;

	dsp_leave();
	return 0;
p_err_rmem:
	dsp_hw_common_memory_remove(mem);
p_err:
	return ret;
}

static void __dsp_hw_p0_memory_unmap_reserved_mem(struct dsp_memory *mem)
{
	int idx;

	dsp_enter();
	for (idx = 0; idx < mem->reserved_mem_count; ++idx)
		mem->ops->unmap_reserved_mem(mem, &mem->reserved_mem[idx]);
	dsp_leave();
}

static void dsp_hw_p0_memory_remove(struct dsp_memory *mem)
{
	dsp_enter();
	__dsp_hw_p0_memory_unmap_reserved_mem(mem);
	dsp_hw_common_memory_remove(mem);
	dsp_leave();
}

static const struct dsp_memory_ops p0_memory_ops = {
	.map_buffer		= dsp_hw_common_memory_map_buffer,
	.unmap_buffer		= dsp_hw_common_memory_unmap_buffer,
	.sync_for_device	= dsp_hw_common_memory_sync_for_device,
	.sync_for_cpu		= dsp_hw_common_memory_sync_for_cpu,

	.ion_alloc		= dsp_hw_common_memory_ion_alloc,
	.ion_free		= dsp_hw_common_memory_ion_free,
	.ion_alloc_secure	= dsp_hw_common_memory_ion_alloc_secure,
	.ion_free_secure	= dsp_hw_common_memory_ion_free_secure,

	.map_reserved_mem	= dsp_hw_common_memory_map_reserved_mem,
	.unmap_reserved_mem	= dsp_hw_common_memory_unmap_reserved_mem,

	.open			= dsp_hw_common_memory_open,
	.close			= dsp_hw_common_memory_close,
	.probe			= dsp_hw_p0_memory_probe,
	.remove			= dsp_hw_p0_memory_remove,
};

int dsp_hw_p0_memory_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_MEMORY,
			&p0_memory_ops, sizeof(p0_memory_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __init dsp_hw_p0_memory_master_setup(struct reserved_mem *rmem)
{
	dsp_enter();
	pr_info("[exynos-dsp][DSP] Reserved mem for master firmware: %pa/%pa\n",
			&rmem->base, &rmem->size);
	dsp_leave();
	return 0;
}
RESERVEDMEM_OF_DECLARE(dsp_master_rmem, "exynos,dsp_master_rmem",
		dsp_hw_p0_memory_master_setup);
