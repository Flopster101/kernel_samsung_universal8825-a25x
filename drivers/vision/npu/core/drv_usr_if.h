/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _DRV_USR_IF_H_
#define _DRV_USR_IF_H_

/* Current supporting user API version */
#define USER_API_VERSION	4

struct drv_usr_share {
	unsigned int id;
	int ncp_fd;
	unsigned int ncp_size;
	unsigned long ncp_mmap;
#ifdef CONFIG_NPU_USE_HW_DEVICE
/*
 * Unified op ID is used by the unified firmware to identify which binaries belong to single unified op.
 * Unified op ID becames necessary to support an unified op from NCP v25.
 *
 * Steps
 *  1. UENN forwards an 64 bits unified op ID to UDD
 *  2. UDD converts the 64 bits ID to 32 bits ID
 *  3. UDD forwards the converted 32 bits ID to UFW
 *  4. UFW uses the 32 bits ID, finally.
 */
	uint64_t unified_op_id; // unified op ID
#endif
};

#endif /* _DRV_USR_IF_H_ */
