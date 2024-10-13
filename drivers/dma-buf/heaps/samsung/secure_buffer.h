// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Heap Secure Structure header to interface ldfw
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#ifndef _SECURE_BUFFER_H
#define _SECURE_BUFFER_H

#include <linux/arm-smccc.h>

/*
 * struct buffer_prot_info - buffer protection information
 * @chunk_count: number of physically contiguous memory chunks to protect
 *               each chunk should has the same size.
 * @dma_addr:    device virtual address for protected memory access
 * @flags:       protection flags but actually, protectid
 * @chunk_size:  length in bytes of each chunk.
 * @bus_address: if @chunk_count is 1, this is the physical address the chunk.
 *               if @chunk_count > 1, this is the physical address of unsigned
 *               long array of @chunk_count elements that contains the physical
 *               address of each chunk.
 */
struct buffer_prot_info {
	unsigned int chunk_count;
	unsigned int dma_addr;
	unsigned int flags;
	unsigned int chunk_size;
	unsigned long bus_address;
};

#define SMC_DRM_PPMP_PROT		(0x82002110)
#define SMC_DRM_PPMP_UNPROT		(0x82002111)

static inline unsigned long ppmp_smc(unsigned long cmd, unsigned long arg0,
				     unsigned long arg1, unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(cmd, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return (unsigned long)res.a0;
}

#endif
