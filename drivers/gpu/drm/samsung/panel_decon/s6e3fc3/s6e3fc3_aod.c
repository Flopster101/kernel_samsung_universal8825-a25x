/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fc3/s6e3fc3_aod.c
 *
 * Source file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../panel_drv.h"
#include "../panel_debug.h"
#include "s6e3fc3_aod.h"

void s6e3fc3_copy_self_mask_ctrl(struct maptbl *tbl, u8 *dst)
{
	panel_info("was called\n");
	panel_info("%x %x %x\n", dst[0], dst[1], dst[2]);
}

int s6e3fc3_init_self_mask_ctrl(struct maptbl *tbl)
{
	struct aod_dev_info *aod = tbl->pdata;
	struct aod_ioctl_props *props = &aod->props;

	props->self_mask_checksum_len = SELFMASK_CHECKSUM_LEN;
	props->self_mask_checksum = kmalloc(sizeof(u8) * props->self_mask_checksum_len, GFP_KERNEL);
	if (!props->self_mask_checksum) {
		panel_err("failed to mem alloc\n");
		return -ENOMEM;
	}
	props->self_mask_checksum[0] = SELFMASK_CHECKSUM_VALID1;
	props->self_mask_checksum[1] = SELFMASK_CHECKSUM_VALID2;
	panel_info("was called\n");
	return 0;
}

