/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - TrustZone Address Space Controller(TZASC) fail detector
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_TZASC_H
#define __EXYNOS_TZASC_H

/* The maximum number of TZASC channel */
#define MAX_NUM_OF_TZASC_CHANNEL			(4)

/* SFR bits information */
/* TZASC Interrupt Status Register */
#define TZASC_INTR_STATUS_INTR_STAT_MASK		(0xFF << 0)
#define TZASC_INTR_STATUS_OVERRUN_MASK			(0xFF << 8)

#define TZASC_DREX_INTR_STATUS_RS_FAIL_MASK		(1 << 16)
#define TZASC_DREX_INTR_STATUS_AXADDR_DECERR_MASK	(1 << 17)
#define TZASC_DREX_INTR_STATUS_WLAST_ERROR_MASK		(1 << 18)

#define TZASC_SMC_INTR_STATUS_OVERLAP_MASK		(1 << 16)

/* TZASC Fail Address High Register */
#define TZASC_FAIL_ADDR_HIGH_MASK			(0xF << 0)

/* TZASC Fail Control Register */
#define TZASC_FAIL_CONTROL_DIRECTION_MASK		(0x1 << 1)
#define TZASC_FAIL_CONTROL_DIRECTION_MASK_TZC380	(0xF << 4)
#define TZASC_FAIL_CONTROL_NON_SECURE_MASK		(1 << 21)

#define TZASC_DREX_FAIL_CONTROL_PRIVILEGED_MASK		(1 << 20)

/* TZASC Fail ID Register */
#define TZASC_DREX_FAIL_ID_AXID_MASK			(0xFFFFFFFF)

#define TZASC_SMC_FAIL_ID_VNET_MASK			(0xF << 24)
#define TZASC_SMC_FAIL_ID_FAIL_ID_MASK			(0xFFFFFFFF)

/* Return values from SMC */
#define TZASC_ERROR_INVALID_CH_NUM			(0xA)
#define TZASC_ERROR_INVALID_FAIL_INFO_SIZE		(0xB)

#define TZASC_NEED_FAIL_INFO_LOGGING			(0x1EED)
#define TZASC_SKIP_FAIL_INFO_LOGGING			(0x2419)
#define TZASC_NO_TZASC_FAIL_INTERRUPT			(0x7017)

#define TZASC_HANDLE_INTERRUPT_THREAD			(1)
#define TZASC_DO_NOT_HANDLE_INTERRUPT_THREAD		(0)

/* Flag whether fail read information is logged */
#define TZASC_STR_INFO_FLAG				(0x45545A43)	/* ETZC */

/* Version number of TZC */
#define TZASC_VERSION_TZC380				(380)		/* DREX */
#define TZASC_VERSION_TZC400				(400)		/* SMC */

#ifndef __ASSEMBLY__
/* Registers of TZASC Fail Information */
struct tzasc_fail_info {
	unsigned int tzasc_intr_stat;
	unsigned int tzasc_fail_addr_low;
	unsigned int tzasc_fail_addr_high;
	unsigned int tzasc_fail_ctrl;
	unsigned int tzasc_fail_id;
};

/* Data structure for TZASC Fail Information */
struct tzasc_info_data {
	struct device *dev;

	struct tzasc_fail_info *fail_info;
	dma_addr_t fail_info_pa;

	unsigned int ch_num;
	unsigned int tzc_ver;

	unsigned int irq[MAX_NUM_OF_TZASC_CHANNEL];
	unsigned int irqcnt;

	unsigned int info_flag;
	int need_log;
	int need_handle;
};

/*
 * TZC380 - TZASC address traslation
 *
 * If the number of TZASC channels is 2 in TZC380 model
 * then the addresses in TZASC fail address registers
 * should be translated to physical address depending on
 * a specific formula.
 */
static inline unsigned int mif_addr_to_pa(unsigned int mif_addr,
					  unsigned int ch)
{
	unsigned long ta, sa;
	unsigned int ba, bit_8th;

	ta = (mif_addr & 0x7FFFFF00) << 1;
	ba = (mif_addr & 0xFF);
	sa = mif_addr << 1;

	bit_8th = (((sa & (1 << 12)) >> 12) ^
		  ((sa & (1 << 14)) >> 14) ^
		  ((sa & (1 << 16)) >> 16) ^
		  ((sa & (1 << 18)) >> 18) ^
		  ((sa & (1 << 21)) >> 21) ^
		  ch) << 8;

	return ta | bit_8th | ba;
}

#endif	/* __ASSEMBLY__ */
#endif	/* __EXYNOS_TZASC_H */
