/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __MIF_REG_PCIE_PAEAN_H
#define __MIF_REG_PCIE_PAEAN_H

#define PMU_BOOT_RAM_START_ADDR		0

/* PERT Registers */
#define PMU_SECOND_STAGE_BOOT_ADDR	0x328
#define PMU_INTERRUPT_FROM_HOST_INTGR	0x32c
#define PMU_INTERRUPT_FROM_HOST_INTMS	0x324
#define WLAN_INTERRUPT_FROM_HOST_PENDING_BIT_ARRAY_SET 0x340
#define WPAN_INTERRUPT_FROM_HOST_INTGR	0x35c
#define PMU_WLAN_UCPU0_RMP_BOOT_ADDR	0x380
#define PMU_WLAN_UCPU1_RMP_BOOT_ADDR	0x384
#define PMU_WPAN_RMP_BOOT_ADDR 		0x388

#define SECOND_STAGE_BOOT_ADDR		0x60000200
#define DRAM_BASE_ADDR			0x90000000

#endif /* ___MIF_REG_PCIE_PAEAN_H */
