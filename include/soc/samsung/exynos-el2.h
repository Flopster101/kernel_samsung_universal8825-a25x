/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL2 module
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_EL2_H
#define __EXYNOS_EL2_H

/* EL2 crash buffer size for each CPU */
#define EL2_CRASH_BUFFER_SIZE			(0x1000)

/* EL2 Log buffer clear config */
#define EL2_LOG_BUFFER_CLEAR			(1)

/* Retry count for allocate EL2 crash buffer */
#define MAX_RETRY_COUNT				(8)

/* EL2 log dump name */
#define EL2_LOG_DSS_NAME			"log_harx"

/* EL2 dynamic level3 table buffer size */
#define EL2_LV3_TABLE_BUF_SIZE_4MB		(0x400000)
#define EL2_DYNAMIC_LV3_TABLE_BUF_256K		(0x40000)
#define EL2_LV3_BUF_MAX				(0x80)

/* EL2 Stage 2 enable check */
#define S2_BUF_SIZE				(30)

#ifndef __ASSEMBLY__

/* EL2 Exception context registers */
enum gpregs_t {
	CTX_GPREG_X0 = 0,
	CTX_GPREG_X1,
	CTX_GPREG_X2,
	CTX_GPREG_X3,
	CTX_GPREG_X4,
	CTX_GPREG_X5 = 5,
	CTX_GPREG_X6,
	CTX_GPREG_X7,
	CTX_GPREG_X8,
	CTX_GPREG_X9,
	CTX_GPREG_X10 = 10,
	CTX_GPREG_X11,
	CTX_GPREG_X12,
	CTX_GPREG_X13,
	CTX_GPREG_X14,
	CTX_GPREG_X15 = 15,
	CTX_GPREG_X16,
	CTX_GPREG_X17,
	CTX_GPREG_X18,
	CTX_GPREG_X19,
	CTX_GPREG_X20 = 20,
	CTX_GPREG_X21,
	CTX_GPREG_X22,
	CTX_GPREG_X23,
	CTX_GPREG_X24,
	CTX_GPREG_X25 = 25,
	CTX_GPREG_X26,
	CTX_GPREG_X27,
	CTX_GPREG_X28,
	CTX_GPREG_X29,
	CTX_GPREG_LR = 30,
	CTX_GPREG_SP_EL0,
	CTX_GPREGS_END
};

enum el2_state_regs_t {
	CTX_HCR_EL2 = 0,
	CTX_ESR_EL2,
	CTX_RUNTIME_SP,
	CTX_SPSR_EL2,
	CTX_ELR_EL2,
	CTX_UNUSED = 5,
	CTX_EL2STATE_END
};

enum el2_sysregs_t {
	/* CTX_HCR_EL2 = 0 */
	CTX_VBAR_EL2 = 1,
	CTX_SCTLR_EL2,
	CTX_ACTLR_EL2,
	CTX_CPTR_EL2,
	CTX_HSTR_EL2 = 5,
	CTX_SP_EL2,
	CTX_TCR_EL2,
	CTX_TTBR0_EL2,
	CTX_TTBR1_EL2,
	CTX_MAIR_EL2 = 10,
	CTX_AMAIR_EL2,
	CTX_VTCR_EL2,
	CTX_VSTCR_EL2,
	CTX_VTTBR_EL2,
	CTX_VSTTBR_EL2 = 15,
	CTX_TPIDR_EL2,
	CTX_MDCR_EL2,
	CTX_HPFAR_EL2,
	CTX_FAR_EL2,
	CTX_AFSR0_EL2 = 20,
	CTX_AFSR1_EL2,
	CTX_CONTEXTIDR_EL2,
	CTX_EL2_SYSREGS_UNUSED,
	CTX_EL2_SYSREGS_END
};

/*
 * struct el2_ectx - EL2 Exception Context
 *
 * @gpregs	: AArch64 general purpose register context(x0-x30, sp_el0)
 * @el2_state	: EL2 state registers used by EL2 firmware to maintain its state
 * @el1_sysregs	: AArch64 EL1 system register context
 */
struct el2_ectx {
	uint64_t gpregs[CTX_GPREGS_END];
	uint64_t el2_state[CTX_EL2STATE_END];
	uint64_t el2_sysregs[CTX_EL2_SYSREGS_END];
};

#endif	/* __ASSEMBLY__ */

#endif	/* __EXYNOS_EL2_H */
