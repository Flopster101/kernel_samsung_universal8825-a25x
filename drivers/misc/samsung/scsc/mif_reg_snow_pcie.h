/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __MIF_REG_SNOW_PCIE_H
#define __MIF_REG_SNOW_PCIE_H

#define BAR1_MAPPING_OFFSET     (0x10000000)

/* Base Registers addresses */
#define PMU_RSVD_START_ADDR     ((0x14400000 - BAR1_MAPPING_OFFSET))
#define PMU_RSVD_END_ADDR       ((0x14400000 - BAR1_MAPPING_OFFSET))
#define PMU_TZPC_START_ADDR     ((0x14410000 - BAR1_MAPPING_OFFSET))
#define PMU_TZPC_END_ADDR       ((0x14410224 - BAR1_MAPPING_OFFSET))
#define MIF_BAAW_START_ADDR     ((0x14420000 - BAR1_MAPPING_OFFSET))
#define MIF_BAAW_END_ADDR       ((0x14420050 - BAR1_MAPPING_OFFSET))
#define APM_BAAW_START_ADDR     ((0x14430000 - BAR1_MAPPING_OFFSET))
#define APM_BAAW_END_ADDR       ((0x14430090 - BAR1_MAPPING_OFFSET))
#define SMAPPER_START_ADDR      ((0x14440000 - BAR1_MAPPING_OFFSET))
#define SMAPPER_END_ADDR        ((0x14440000 - BAR1_MAPPING_OFFSET))
#define PMU_SYSREG_START_ADDR   ((0x14450000 - BAR1_MAPPING_OFFSET))
#define PMU_SYSREG_END_ADDR     ((0x14450418 - BAR1_MAPPING_OFFSET))
#define PMU_BOOT_START_ADDR     ((0x14460000 - BAR1_MAPPING_OFFSET))
#define PMU_BOOT_END_ADDR       ((0x14460010 - BAR1_MAPPING_OFFSET))
#define PMU_DUMMY_START_ADDR    ((0x14461000 - BAR1_MAPPING_OFFSET))
#define PMU_DUMMY_END_ADDR      ((0x14461000 - BAR1_MAPPING_OFFSET))
#define BOOT_START_ADDR         ((0x14460000 - BAR1_MAPPING_OFFSET))
#define PMU_BOOT_RAM_START_ADDR ((0x14470000 - BAR1_MAPPING_OFFSET))

/*******************/
/*******************/
/*   Registers     */
/*******************/
/*******************/
#define TZPC_PROT0STAT          (PMU_TZPC_START_ADDR + 0x200)
#define TZPC_PROT0SET           (PMU_TZPC_START_ADDR + 0x204)
#define TZPC_PROT0CLR           (PMU_TZPC_START_ADDR + 0x208)
#define PROC_RMP_BOOT_ADDR      (PMU_SYSREG_START_ADDR + 0x400)
#define WPAN_RMP_BOOT_ADDR      (PMU_SYSREG_START_ADDR + 0x404)
#define CHIP_VERSION_ID         (PMU_SYSREG_START_ADDR + 0x414)
#define BOOT_SOURCE             (BOOT_START_ADDR)
#define BOOT_CFG_ACK            (BOOT_START_ADDR + 0x4)
#define AP2WB_MAILBOX           (BOOT_START_ADDR + 0x8)
#define WB2AP_MAILBOX           (BOOT_START_ADDR + 0xc)

/*#define MBOX                    (0x11a70000 - BAR1_MAPPING_OFFSET + 0x50)*/
#define MBOX                    (0x109F0000 - BAR1_MAPPING_OFFSET + 0x50)

#define PMU_AP_MB_MSG_NULL                            (0x00)
#define PMU_AP_MB_MSG_START_WLAN                      (0x01)
#define PMU_AP_MB_MSG_START_WPAN                      (0x02)
#define PMU_AP_MB_MSG_START_WLAN_WPAN                 (0x03)
#define PMU_AP_MB_MSG_RESET_WLAN                      (0x04)
#define PMU_AP_MB_MSG_RESET_WPAN                      (0x05)

/********** MIF BAAW regions *********/
/* Region #0 */
#define MIF_START_0             (MIF_BAAW_START_ADDR + 0x00)
#define MIF_END_0               (MIF_BAAW_START_ADDR + 0x04)
#define MIF_REMAP_0             (MIF_BAAW_START_ADDR + 0x08)
#define MIF_DONE_0              (MIF_BAAW_START_ADDR + 0x0c)
/* Region #1 */
#define MIF_START_1             (MIF_BAAW_START_ADDR + 0x10)
#define MIF_END_1               (MIF_BAAW_START_ADDR + 0x14)
#define MIF_REMAP_1             (MIF_BAAW_START_ADDR + 0x18)
#define MIF_DONE_1              (MIF_BAAW_START_ADDR + 0x1c)
/* Region #2 covers DRAM shared with Cellular (or DRAM for IQ capture) */
#define MIF_START_2             (MIF_BAAW_START_ADDR + 0x20)
#define MIF_END_2               (MIF_BAAW_START_ADDR + 0x24)
#define MIF_REMAP_2             (MIF_BAAW_START_ADDR + 0x28)
#define MIF_DONE_2              (MIF_BAAW_START_ADDR + 0x2c)
/* Region #3 covers DRAM shared with A-BOX (or DRAM for IQ capture) */
#define MIF_START_3             (MIF_BAAW_START_ADDR + 0x30)
#define MIF_END_3               (MIF_BAAW_START_ADDR + 0x34)
#define MIF_REMAP_3             (MIF_BAAW_START_ADDR + 0x38)
#define MIF_DONE_3              (MIF_BAAW_START_ADDR + 0x3c)
/* Region #4 covers 16kB A-BOX SRAM (or DRAM for IQ capture) */
#define MIF_START_4             (MIF_BAAW_START_ADDR + 0x40)
#define MIF_END_4               (MIF_BAAW_START_ADDR + 0x44)
#define MIF_REMAP_4             (MIF_BAAW_START_ADDR + 0x48)
#define MIF_DONE_4              (MIF_BAAW_START_ADDR + 0x4c)

/********** APM BAAW regions *********/
/* Region #0 covers GNSS and APM mailboxes */
#define APM_START_0             (APM_BAAW_START_ADDR + 0x00)
#define APM_END_0               (APM_BAAW_START_ADDR + 0x04)
#define APM_REMAP_0             (APM_BAAW_START_ADDR + 0x08)
#define APM_DONE_0              (APM_BAAW_START_ADDR + 0x0c)
/* Region #1 covers A-BOX and CHUB mailboxes */
#define APM_START_1             (APM_BAAW_START_ADDR + 0x10)
#define APM_END_1               (APM_BAAW_START_ADDR + 0x14)
#define APM_REMAP_1             (APM_BAAW_START_ADDR + 0x18)
#define APM_DONE_1              (APM_BAAW_START_ADDR + 0x1c)
/* Region #2 covers mailboxes (AP2WL, AP2BT, CP2WL, CP2BT, and the SCSC super-secret mailbox!) */
#define APM_START_2             (APM_BAAW_START_ADDR + 0x20)
#define APM_END_2               (APM_BAAW_START_ADDR + 0x24)
#define APM_REMAP_2             (APM_BAAW_START_ADDR + 0x28)
#define APM_DONE_2              (APM_BAAW_START_ADDR + 0x2c)
/* Region #3 covers mailboxes GPIO_CMGP, ADC_CMGP */
#define APM_START_3             (APM_BAAW_START_ADDR + 0x30)
#define APM_END_3               (APM_BAAW_START_ADDR + 0x34)
#define APM_REMAP_3             (APM_BAAW_START_ADDR + 0x38)
#define APM_DONE_3              (APM_BAAW_START_ADDR + 0x3c)
/* Region #4 covers CMGP0 SFRs (for SYSREG_CMGP2WLBT) */
#define APM_START_4             (APM_BAAW_START_ADDR + 0x40)
#define APM_END_4               (APM_BAAW_START_ADDR + 0x44)
#define APM_REMAP_4             (APM_BAAW_START_ADDR + 0x48)
#define APM_DONE_4              (APM_BAAW_START_ADDR + 0x4c)
/* Region #5 covers CMGP1 SFRs (for USI/I2C) */
#define APM_START_5             (APM_BAAW_START_ADDR + 0x50)
#define APM_END_5               (APM_BAAW_START_ADDR + 0x54)
#define APM_REMAP_5             (APM_BAAW_START_ADDR + 0x58)
#define APM_DONE_5              (APM_BAAW_START_ADDR + 0x5c)
/* Region #6 covers SYSREG_COMBINE_CHUB2WLBT */
#define APM_START_6             (APM_BAAW_START_ADDR + 0x60)
#define APM_END_6               (APM_BAAW_START_ADDR + 0x64)
#define APM_REMAP_6             (APM_BAAW_START_ADDR + 0x68)
#define APM_DONE_6              (APM_BAAW_START_ADDR + 0x6c)
/* Region #7 covers Context Hub SFRs (for USI/I2C/GPIO) */
#define APM_START_7             (APM_BAAW_START_ADDR + 0x70)
#define APM_END_7               (APM_BAAW_START_ADDR + 0x74)
#define APM_REMAP_7             (APM_BAAW_START_ADDR + 0x78)
#define APM_DONE_7              (APM_BAAW_START_ADDR + 0x7c)
/* Region #8 covers Context Hub SRAM dd*/
#define APM_START_8             (APM_BAAW_START_ADDR + 0x80)
#define APM_END_8               (APM_BAAW_START_ADDR + 0x84)
#define APM_REMAP_8             (APM_BAAW_START_ADDR + 0x88)
#define APM_DONE_8              (APM_BAAW_START_ADDR + 0x8c)

/* PMU config registers */
#define TB_ICONNECT_WLBT_AXI_NONSECURE          0x00
#define TB_ICONNECT_WLBT_RESET                  0x04
#define TB_ICONNECT_WLBT_PCM0_CONFIG_SELECT_FPGA    0x08
#define TB_ICONNECT_CMGP                        0x0c
#define TB_ICONNECT_APM_PCH_LONGHOP_DOWN_REQ    0x10
#define TB_ICONNECT_APM_PCH_LONGHOP_DOWN_ACK    0x14
#define TB_ICONNECT_MIF_PCH_LONGHOP_DOWN_REQ    0x18
#define TB_ICONNECT_MIF_PCH_LONGHOP_DOWN_ACK    0x1c
#define TB_ICONNECT_CFG_PCH_LONGHOP_DOWN_REQ    0x20
#define TB_ICONNECT_CFG_PCH_LONGHOP_DOWN_ACK    0x24
#define TB_ICONNECT_LONGHOP_RESET               0x28
#define TB_ICONNECT_SWEEPER_REQ                 0x2c
#define TB_ICONNECT_SWEEPER_ACK                 0x30
#define TB_ICONNECT_BT_LOOPBACK_EN              0x34
#define TB_ICONNECT_32K_CLOCK_SCALE_COUNTER     0x38
#define TB_ICONNECT_32K_CLOCK_SCALE_COUNTER     0x38
#define TB_ICONNECT_PMU_MILDRED_PC              0x3C
/**
0x40 = writing bit 1 clears the corresponding interrupt enable bit, reading gives the current interrupt enables
0x44 = writing bit 1 sets the correspondinginterrupt enable bit, reading gives the current interrupt enables
0x48 = writing bit 1 clears the corresponding pending status, reading gives pending interrupt status
0x4C = wiriting is ignored, reading gives RAW interrupt status
*/
#define TB_ICONNECT_SINGLE_IRQ_TO_HOST_DISABLE	0x40
#define TB_ICONNECT_SINGLE_IRQ_TO_HOST_ENABLE	0x44
#define TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR	0x48
#define TB_ICONNECT_SINGLE_IRQ_TO_HOST_RAW      0x4C
#define TB_ICONNECT_SINGLE_IRQ_TO_HOST_MODE     0x50

#define AP2WB_IRQ	BIT(7)
#define AP2BT_IRQ	BIT(2)
#define AP2GFG		BIT(8)


#endif /* ___MIF_REG_SNOW_PCIE_H */
