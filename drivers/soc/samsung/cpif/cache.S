/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cache maintenance
 *
 * Copyright (C) 2001 Deep Blue Solutions Ltd.
 * Copyright (C) 2012 ARM Ltd.
 */

#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>
#include <asm/cpufeature.h>
#include <asm/alternative.h>
#include <asm/asm-uaccess.h>

/*extern unsigned long read_sanitised_ftr_reg(unsigned int id);*/

/*
 * cpif_read_ctr - read CTR_EL0. If the system has mismatched register fields,
 * provide the system wide safe value from arm64_ftr_reg_ctrel0.sys_val
 */
	.macro	cpif_read_ctr, reg
#ifndef __KVM_NVHE_HYPERVISOR__
	mrs	\reg, ctr_el0			// read CTR
	nop
#else
alternative_if_not ARM64_KVM_PROTECTED_MODE
	ASM_BUG()
alternative_else_nop_endif
alternative_cb kvm_compute_final_ctr_el0
	movz	\reg, #0
	movk	\reg, #0, lsl #16
	movk	\reg, #0, lsl #32
	movk	\reg, #0, lsl #48
alternative_cb_end
#endif
	.endm


/*
 * cpif_dcache_line_size - get the safe D-cache line size across all CPUs
 */
	.macro	cpif_dcache_line_size, reg, tmp
	cpif_read_ctr	\tmp
	ubfm		\tmp, \tmp, #16, #19	// cache line size encoding
	mov		\reg, #4		// bytes per word
	lsl		\reg, \reg, \tmp	// actual cache line size
	.endm



/*
 * Macro to perform a data cache maintenance for the interval
 * [kaddr, kaddr + size)
 *
 * 	op:		operation passed to dc instruction
 * 	domain:		domain used in dsb instruciton
 * 	kaddr:		starting virtual address of the region
 * 	size:		size of the region
 * 	Corrupts:	kaddr, size, tmp1, tmp2
 */
	.macro __cpif_dcache_op_workaround_clean_cache, op, kaddr
alternative_if_not ARM64_WORKAROUND_CLEAN_CACHE
	dc	\op, \kaddr
alternative_else
	dc	civac, \kaddr
alternative_endif
	.endm

	.macro cpif_dcache_by_line_op op, domain, kaddr, size, tmp1, tmp2
	cpif_dcache_line_size \tmp1, \tmp2
	add	\size, \kaddr, \size

	sub	\tmp2, \tmp1, #1
	bic	\kaddr, \kaddr, \tmp2
9998:
	.ifc	\op, cvau
	__cpif_dcache_op_workaround_clean_cache \op, \kaddr
	.else
	.ifc	\op, cvac
	__cpif_dcache_op_workaround_clean_cache \op, \kaddr
	.else
	.ifc	\op, cvap
	sys	3, c7, c12, 1, \kaddr	// dc cvap
	.else
	.ifc	\op, cvadp
	sys	3, c7, c13, 1, \kaddr	// dc cvadp
	.else
	dc	\op, \kaddr
	.endif
	.endif
	.endif
	.endif
	add	\kaddr, \kaddr, \tmp1
	cmp	\kaddr, \size
	b.lo	9998b
	dsb	\domain
	.endm

/*
 *	__flush_dcache_area(kaddr, size)
 *
 *	Ensure that any D-cache lines for the interval [kaddr, kaddr+size)
 *	are cleaned and invalidated to the PoC.
 *
 *	- kaddr   - kernel address
 *	- size    - size in question
 */
SYM_FUNC_START_PI(cpif_flush_dcache_area)
	cpif_dcache_by_line_op civac, sy, x0, x1, x2, x3
	ret
SYM_FUNC_END_PI(cpif_flush_dcache_area)

