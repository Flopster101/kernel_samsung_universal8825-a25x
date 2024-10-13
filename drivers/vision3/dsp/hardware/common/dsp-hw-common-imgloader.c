// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-common-system.h"
#include "dsp-hw-common-imgloader.h"

int dsp_hw_common_imgloader_boot(struct dsp_imgloader *imgloader)
{
	int ret;

	dsp_enter();
	imgloader->desc.skip_request_firmware = true;
	ret = imgloader_boot(&imgloader->desc);
	if (ret) {
		dsp_err("Failed to boot imgloader(%d)\n", ret);
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_imgloader_shutdown(struct dsp_imgloader *imgloader)
{
	imgloader_shutdown(&imgloader->desc);
	return 0;
}

static int dsp_hw_common_imgloader_ops_mem_setup(struct imgloader_desc *desc,
		const unsigned char *metadata, size_t size,
		phys_addr_t *fw_phys_base, size_t *fw_bin_size,
		size_t *fw_mem_size)
{
	struct dsp_imgloader *imgloader;
	struct dsp_memory *mem;
	struct dsp_reserved_mem *rmem;

	dsp_enter();
	imgloader = desc->data;
	mem = &imgloader->sys->memory;

	if (mem->shared_id.fw_master < 0)
		goto p_end;

	rmem = &mem->reserved_mem[mem->shared_id.fw_master];

	*fw_phys_base = rmem->paddr;
	*fw_bin_size = rmem->used_size;
	*fw_mem_size = rmem->size;

	dsp_leave();
p_end:
	return 0;
}

static int dsp_hw_common_imgloader_ops_verify_fw(struct imgloader_desc *desc,
		phys_addr_t fw_phys_base, size_t fw_bin_size,
		size_t fw_mem_size)
{
	dsp_check();
	return 0;
}

static int dsp_hw_common_imgloader_ops_blk_pwron(struct imgloader_desc *desc)
{
	dsp_check();
	return 0;
}

static int dsp_hw_common_imgloader_ops_init_image(struct imgloader_desc *desc)
{
	dsp_check();
	return 0;
}

static int dsp_hw_common_imgloader_ops_deinit_image(struct imgloader_desc *desc)
{
	dsp_check();
	return 0;
}

static int dsp_hw_common_imgloader_ops_shutdown(struct imgloader_desc *desc)
{
	dsp_check();
	return 0;
}

static const struct imgloader_ops common_imgloader_ops = {
	.mem_setup	= dsp_hw_common_imgloader_ops_mem_setup,
	.verify_fw	= dsp_hw_common_imgloader_ops_verify_fw,
	.blk_pwron	= dsp_hw_common_imgloader_ops_blk_pwron,
	.init_image	= dsp_hw_common_imgloader_ops_init_image,
	.deinit_image	= dsp_hw_common_imgloader_ops_deinit_image,
	.shutdown	= dsp_hw_common_imgloader_ops_shutdown,
};

int dsp_hw_common_imgloader_probe(struct dsp_imgloader *imgloader, void *sys)
{
	int ret;
	struct imgloader_desc *desc;

	dsp_enter();
	imgloader->sys = sys;
	desc = &imgloader->desc;

	desc->dev = imgloader->sys->dev;
	desc->owner = THIS_MODULE;
	desc->ops = &common_imgloader_ops;
	desc->data = (void *)imgloader;
#ifdef CONFIG_EXYNOS_DSP_HW_P0
	desc->name = "DNC";
#else
	desc->name = "VPC";
#endif
	desc->fw_id = 0;

	ret = imgloader_desc_init(desc);
	if (ret) {
		dsp_err("Failed to init imgloader_desc(%d)\n", ret);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_hw_common_imgloader_remove(struct dsp_imgloader *imgloader)
{
	dsp_check();
	imgloader_desc_release(&imgloader->desc);
}

int dsp_hw_common_imgloader_set_ops(struct dsp_imgloader *imgloader,
		const void *ops)
{
	dsp_enter();
	imgloader->ops = ops;
	dsp_leave();
	return 0;
}
