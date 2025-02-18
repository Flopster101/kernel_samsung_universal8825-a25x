// SPDX-License-Identifier: GPL-2.0+
/*
 * Structs and functions to support MPAM CPBM
 *
 * Copyright (C) 2021 Samsung Electronics
 */

#ifndef __SEC_MPAM_CPBM_H
#define __SEC_MPAM_CPBM_H

#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/types.h>

/* Reference: "11.4.2 MPAMCFG_CPBM<n>, MPAM Cache Portion Bitmap Partition
 * Configuration Register, n = 0 -1023", pp.233,
 * "Memory System Resource Partitioning and Monitoring (MPAM), for Armv8-A",
 * 2021.1.22, Issue B.c, "Arm® Architecture Reference Manual Supplement"
 */
#define SEC_MPAM_CPBM_BITLEN	(32)

typedef struct mpam_cpbm_s { DECLARE_BITMAP(bits, SEC_MPAM_CPBM_BITLEN); } mpam_cpbm_t;
typedef struct mpam_cpbm_s *mpam_cpbm_var_t;

static inline unsigned int __mpam_cpbm_bitlen(const mpam_cpbm_var_t m)
{
	return m ? SEC_MPAM_CPBM_BITLEN : 0;
}
static inline unsigned long *__mpam_cpbm_bits(const mpam_cpbm_var_t m)
{
	return m ? m->bits : NULL;
}
static inline unsigned int mpam_cpbm_arraysize(void)
{
	return BITS_TO_LONGS(SEC_MPAM_CPBM_BITLEN);
}
static inline unsigned int mpam_cpbm_size(void)
{
	return mpam_cpbm_arraysize() * sizeof(long);
}

static inline bool alloc_mpam_cpbm_var(mpam_cpbm_var_t *mask, gfp_t flags)
{
	*mask = kmalloc_node(mpam_cpbm_size(), flags, NUMA_NO_NODE);
	return *mask != NULL;
}

static inline void free_mpam_cpbm_var(mpam_cpbm_var_t mask)
{
	kfree(mask);
}

#define mpam_cpbm_pr_args(maskp)	__mpam_cpbm_bitlen(maskp), \
	__mpam_cpbm_bits(maskp)

#define mpam_cpbm_clear(dstp) __mpam_cpbm_clear(dstp, SEC_MPAM_CPBM_BITLEN)
static inline void __mpam_cpbm_clear(mpam_cpbm_var_t dstp, unsigned int nbits)
{
	bitmap_zero(dstp->bits, nbits);
}
#define mpam_cpbm_setall(dstp) __mpam_cpbm_setall(dstp, SEC_MPAM_CPBM_BITLEN)
static inline void __mpam_cpbm_setall(mpam_cpbm_var_t dstp, unsigned int nbits)
{
	bitmap_fill(dstp->bits, nbits);
}

static inline int mpam_cpbm_parse(const char *buf, mpam_cpbm_var_t dstp)
{
	return bitmap_parselist(buf, __mpam_cpbm_bits(dstp), __mpam_cpbm_bitlen(dstp));
}

#define mpam_cpbm_subset(src1p, src2p) \
	__mpam_cpbm_subset(src1p, src2p, SEC_MPAM_CPBM_BITLEN)
static inline int __mpam_cpbm_subset(const mpam_cpbm_var_t src1p,
		mpam_cpbm_var_t src2p, unsigned int nbits)
{
	return bitmap_subset(src1p->bits, src2p->bits, nbits);
}

#define mpam_cpbm_equal(src1p, src2p) \
	__mpam_cpbm_equal(src1p, src2p, SEC_MPAM_CPBM_BITLEN)
static inline int __mpam_cpbm_equal(const mpam_cpbm_var_t src1p,
		const mpam_cpbm_var_t src2p, unsigned int nbits)
{
	return bitmap_equal(src1p->bits, src2p->bits, nbits);
}

#define mpam_cpbm_full(src) \
	__mpam_cpbm_full(src, SEC_MPAM_CPBM_BITLEN)
static inline int __mpam_cpbm_full(const mpam_cpbm_var_t src, unsigned int nbits)
{
	return bitmap_full(src->bits, nbits);
}

#endif /* __SEC_MPAM_CPBM_H */
