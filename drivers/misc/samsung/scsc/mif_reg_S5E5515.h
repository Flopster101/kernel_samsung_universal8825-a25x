/****************************************************************************
 *
 * Copyright (c) 2014 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIF_REG_5515_H
#define __MIF_REG_5515_H

/*********************************/
/* PLATFORM register definitions */
/*********************************/
#define NUM_MBOX_PLAT 4
#define NUM_SEMAPHORE 12

/********************************************/
/* MAILBOX_AP_WLAN_BASE       0x129D0000    */
/* MAILBOX_AP_WPAN_BASE       0x129E0000    */
/********************************************/
#define MCUCTRL 0x000 /* MCU Controller Register */

#define MAILBOX_WLBT_BASE 0x0000
#define MAILBOX_WLBT_REG(r) (MAILBOX_WLBT_BASE + (r))

/* WLBT to AP */
#define INTGR0 0x008 /* Interrupt Generation Register 0 (r/w) */
#define INTCR0 0x00C /* Interrupt Clear Register 0 (w) */
#define INTMR0 0x010 /* Interrupt Mask Register 0 (r/w) */
#define INTSR0 0x014 /* Interrupt Status Register 0 (r) */
#define INTMSR0 0x018 /* Interrupt Mask Status Register 0 (r) */

/* AP to WLBT */
#define INTGR1 0x01c /* Interrupt Generation Register 1 */
#define INTCR1 0x020 /* Interrupt Clear Register 1 */
#define INTMR1 0x024 /* Interrupt Mask Register 1 */
#define INTSR1 0x028 /* Interrupt Status Register 1 */
#define INTMSR1 0x02c /* Interrupt Mask Status Register 1 */

/* Shared register */
#define ISSR_BASE 0x100 /* IS_Shared_Register Base address */
#define ISSR(r) (ISSR_BASE + (4 * (r)))

#define MIF_INIT 0x06c /* MIF_init */
#define IS_VERSION 0x070 /* Version Information Register */
/********************************************/
/* END MAILBOX_AP_WLAN_BASE                 */
/* END MAILBOX_AP_WPAN_BASE                 */
/********************************************/

/********************************************/
/* PMU_ALIVE_BASE             0x12860000    */
/********************************************/
#define WLBT_STAT 0x0058
#define WLBT_PWRDN_DONE BIT(0) /* Check WLBT power-down status.*/
#define WLBT_ACCESS_MIF BIT(4) /* Check whether WLBT accesses MIF domain */

#define WLBT_DEBUG 0x005C /* MIF sleep, wakeup debugging control */
#define MASK_CLKREQ_WLBT                                                       \
	BIT(8) /* When this field is set to HIGH, ALIVE ignores
					* CLKREQ from WLBT.
					*/
#define WLBT_CONFIGURATION 0x3900
#define LOCAL_PWR_CFG BIT(0) /* Control power state 0: Power down 1: Power on */

#define WLBT_STATUS 0x3904
#define WLBT_STATUS_BIT0 BIT(0) /* Status 0 : Power down 1 : Power on */

#define WLBT_STATES                                                            \
	0x3908 /* STATES [7:0] States index for debugging
					* 0x00 : Reset
					* 0x10 : Power up
					* 0x80 : Power down
					* */

#define WLBT_OPTION 0x390C
#define WLBT_OPTION_DATA BIT(3)

#define WLBT_CTRL_NS 0x3910
#define WLBT_ACTIVE_CLR                                                        \
	BIT(8) /* WLBT_ACTIVE_REQ is clear internally on WAKEUP */
#define WLBT_ACTIVE_EN BIT(7) /* Enable of WIFI_ACTIVE_REQ */
#define SW_TCXO_REQ                                                            \
	BIT(6) /* SW TCXO Request register, if MASK_TCXO_REQ
					 *  filed value is 1, This register value control TCXO Request*/
#define MASK_TCXO_REQ                                                          \
	BIT(5) /* 1:mask TCXO_REQ coming from CP,
					 * 0:enable request source
					 */
#define TCXO_GATE BIT(4) /* TCXO gate control 0: TCXO enabled 1: TCXO gated */
#define RTC_OUT_EN BIT(0) /* RTC output enable 0:Disable 1:Enable */

#define WLBT_CTRL_S 0x3914 /* WLBT Control SFR secure */
#define WLBT_START BIT(3) /* CP control enable 0: Disable 1: Enable */

#define WLBT_OUT 0x3920
#define INISO_EN BIT(19)
#define TCXO_ACK BIT(18)
#define PWR_ACK BIT(17)
#define INTREQ_ACTIVE BIT(14)
#define SWEEPER_BYPASS                                                         \
	BIT(13) /* SWEEPER bypass mode control(WLBT2AP path) If
					 * this bit is set to 1, SWEEPER is bypass mode.
					 */
#define SWEEPER_PND_CLR_REQ                                                    \
	BIT(7) /* SWEEPER_CLEAN Request. SWPPER is the IP
					 * that can clean up hung transaction in the Long hop
					 * async Bus Interface, when <SUBSYS> get hung
					 * state. 0: Normal 1: SWEEPER CLEAN Requested
					 */

#define WLBT_IN 0x3924
#define SHIFTING_DONE_OTP_BLK                                                  \
	BIT(5) /* OTP shifting controls feedback 0:None 1:Shifting done */
#define BUS_READY                                                              \
	BIT(4) /* BUS ready indication signal when reset released. 0:
					* Normal 1: BUS ready state */
#define PWRDOWN_IND                                                            \
	BIT(2) /* PWRDOWN state indication 0: Normal 1: In the
					* power down state */
#define SWEEPER_PND_CLR_ACK                                                    \
	BIT(0) /* SWEEPER_CLEAN ACK signal. SWPPER is the IP
					* that can clean up hung transaction in the Long hop
					* async Bus Interface, when <SUBSYS> get hung
					* state. 0: Normal 1: SWEEPER CLEAN
					* Acknowledged */

#define WLBT_INT_IN 0x3940
#define PWR_REQ_R BIT(2)
#define PWR_REQ_F BIT(3)
#define TCXO_REQ_R BIT(4)
#define TCXO_REQ_F BIT(5)

#define WLBT_INT_EN 0x3944

#define WLBT_INT_TYPE 0x3948
#define WLBT_INT_DIR 0x394c

#define WAKEUP_INT_IN 0x3c40
#define WAKEUP_INT_EN 0x3c44
#define WAKEUP_INT_TYPE 0x3c48
#define RESETREQ_WLBT BIT(18) /* Interrupt type 0:Edge, 1:Level */

/* New access type : set-bit-atomic
 * write at Base_addr + (offset|0xc0000) "value"
 * then only Base_addr+offset's "value" bit will be updated.
 * changed method from 'write' to 'set-bit-atomic'
 * Add SYSTEM_OUT_ATOMIC for (offset|0xC000)
 * updated PWRRGTON_WLBT BIT(20) -> 0x1B (20)
*/
#define SYSTEM_OUT 0x3d20
#define SYSTEM_OUT_ATOMIC_CMD ((SYSTEM_OUT) | (0xC000))
#define PWRRGTON_WLBT_CMD 0x1B /* 27 for update 27th bit */

#define WLBT_PWR_REQ_HW_SEL 0x3f94
#define SELECT                                                                 \
	BIT(0) /* PWR_REQ of WLBT selection signal. 0: APM SW
					handles PWR_REQ CLKREQ. 1: PWR_REQ is
					connected directly.Interrupt type 0:Edge, 1:Level */

/********************************************/
/* END PMU_ALIVE_BASE                       */
/********************************************/

/********************************************/
/* PBUS_BASE                     0x10800000 */
/********************************************/
#define PADDR_WLBT_PBUS_BASE 0x00000
#define WLBT_PBUS_D_TZPC_SFR 0x10000

/********************************************/
/* WLBT_PBUS_BAAW_DBUS           0x10820000 */
/********************************************/
/* REGISTERS */
#define WLBT_PBUS_BAAW_DBUS 0x0
#define BAAW_D_WLBT_START ((WLBT_PBUS_BAAW_DBUS) + 0x0)
#define BAAW_D_WLBT_END ((WLBT_PBUS_BAAW_DBUS) + 0x4)
#define BAAW_D_WLBT_REMAP ((WLBT_PBUS_BAAW_DBUS) + 0x8)
#define BAAW_D_WLBT_INIT_DONE ((WLBT_PBUS_BAAW_DBUS) + 0xc)
/* VALUES */
#define WLBT_DBUS_BAAW_0_START 0x90000000 /* Start of DRAM for WLBT R7 */
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
#define WLBT_DBUS_BAAW_0_END 0x90800000 /* 8 MB */
#define WLBT_DBUS_BAAW_1_START                                                 \
	(WLBT_DBUS_BAAW_0_END) /* Start of DRAM for sable */
#define WLBT_DBUS_BAAW_1_END (WLBT_DBUS_BAAW_1_START + 0x800000) /* 8MB */
#else
#define WLBT_DBUS_BAAW_0_END 0x91000000
#endif
/* TODO document says SET only 0 bit 0xC init done */
#define WLBT_BAAW_CON_INIT_DONE (1 << 31)
#define WLBT_BAAW_CON_EN_WRITE (1 << 1)
#define WLBT_BAAW_CON_EN_READ (1 << 0)
#define WLBT_BAAW_ACCESS_CTRL                                                  \
	(WLBT_BAAW_CON_INIT_DONE | WLBT_BAAW_CON_EN_WRITE |                    \
	 WLBT_BAAW_CON_EN_READ)
/********************************************/
/* WLBT_PBUS_BAAW_CBUS           0x10830000 */
/********************************************/
#define WLBT_PBUS_BAAW_CBUS 0x0
/* REGISTERS WLBT_PBUS_BAAW_CBUS0 */
#define BAAW_C_WLBT_START_0 ((WLBT_PBUS_BAAW_CBUS) + 0x00)
#define BAAW_C_WLBT_END_0 ((WLBT_PBUS_BAAW_CBUS) + 0x04)
#define BAAW_C_WLBT_REMAP_0 ((WLBT_PBUS_BAAW_CBUS) + 0x08)
#define BAAW_C_WLBT_INIT_DONE_0 ((WLBT_PBUS_BAAW_CBUS) + 0x0c)
/* VALUES for CP */
#define WLBT_MAILBOX_CP_WLBT_BASE 0x12950000
#define WLBT_CBUS_BAAW_0_START 0x58000000
#define WLBT_CBUS_BAAW_0_END 0x58020000

/* REGISTERS WLBT_PBUS_BAAW_CBUS1 */
#define BAAW_C_WLBT_START_1 ((WLBT_PBUS_BAAW_CBUS) + 0x10)
#define BAAW_C_WLBT_END_1 ((WLBT_PBUS_BAAW_CBUS) + 0x14)
#define BAAW_C_WLBT_REMAP_1 ((WLBT_PBUS_BAAW_CBUS) + 0x18)
#define BAAW_C_WLBT_INIT_DONE_1 ((WLBT_PBUS_BAAW_CBUS) + 0x1c)
/* VALUES */
#define WLBT_MAILBOX_WLBT_CHUB_BASE 0x129B0000
#define WLBT_CBUS_BAAW_1_START 0x58020000
#define WLBT_CBUS_BAAW_1_END 0x58090000

/* REGISTERS WLBT_PBUS_BAAW_CBUS2 */
#define BAAW_C_WLBT_START_2 ((WLBT_PBUS_BAAW_CBUS) + 0x20)
#define BAAW_C_WLBT_END_2 ((WLBT_PBUS_BAAW_CBUS) + 0x24)
#define BAAW_C_WLBT_REMAP_2 ((WLBT_PBUS_BAAW_CBUS) + 0x28)
#define BAAW_C_WLBT_INIT_DONE_2 ((WLBT_PBUS_BAAW_CBUS) + 0x2c)
/* VALUES */
#define WLBT_GPIO_CMGP_BASE 0x12B30000
#define WLBT_CBUS_BAAW_2_START 0x58090000
#define WLBT_CBUS_BAAW_2_END 0x580C0000

/* REGISTERS WLBT_PBUS_BAAW_CBUS3 */
#define BAAW_C_WLBT_START_3 ((WLBT_PBUS_BAAW_CBUS) + 0x30)
#define BAAW_C_WLBT_END_3 ((WLBT_PBUS_BAAW_CBUS) + 0x34)
#define BAAW_C_WLBT_REMAP_3 ((WLBT_PBUS_BAAW_CBUS) + 0x38)
#define BAAW_C_WLBT_INIT_DONE_3 ((WLBT_PBUS_BAAW_CBUS) + 0x3c)
/* VALUES */
#define WLBT_SYSREG_CMGP2APM_BASE 0x12B80000
#define WLBT_CBUS_BAAW_3_START 0x580C0000
#define WLBT_CBUS_BAAW_3_END 0x580D0000

/* REGISTERS WLBT_PBUS_BAAW_CBUS4 */
#define BAAW_C_WLBT_START_4 ((WLBT_PBUS_BAAW_CBUS) + 0x40)
#define BAAW_C_WLBT_END_4 ((WLBT_PBUS_BAAW_CBUS) + 0x44)
#define BAAW_C_WLBT_REMAP_4 ((WLBT_PBUS_BAAW_CBUS) + 0x48)
#define BAAW_C_WLBT_INIT_DONE_4 ((WLBT_PBUS_BAAW_CBUS) + 0x4c)
/* VALUES */
#define WLBT_SYSREG_CMGP2WLBT_BASE 0x12BD0000
#define WLBT_CBUS_BAAW_4_START 0x580D0000
#define WLBT_CBUS_BAAW_4_END 0x581E0000

/* REGISTERS WLBT_PBUS_BAAW_CBUS5 */
#define BAAW_C_WLBT_START_5 ((WLBT_PBUS_BAAW_CBUS) + 0x50)
#define BAAW_C_WLBT_END_5 ((WLBT_PBUS_BAAW_CBUS) + 0x54)
#define BAAW_C_WLBT_REMAP_5 ((WLBT_PBUS_BAAW_CBUS) + 0x58)
#define BAAW_C_WLBT_INIT_DONE_5 ((WLBT_PBUS_BAAW_CBUS) + 0x5c)
/* VALUES */
#define WLBT_SHUB_USICHUB0_BASE 0x10CA0000
#define WLBT_CBUS_BAAW_5_START 0x58180000
#define WLBT_CBUS_BAAW_5_END 0x581B0000

#if 0
/* REGISTERS WLBT_PBUS_BAAW_CBUS6 */
#define BAAW_C_WLBT_START_6 ((WLBT_PBUS_BAAW_CBUS) + 0x60)
#define BAAW_C_WLBT_END_6 ((WLBT_PBUS_BAAW_CBUS) + 0x64)
#define BAAW_C_WLBT_REMAP_6 ((WLBT_PBUS_BAAW_CBUS) + 0x68)
#define BAAW_C_WLBT_INIT_DONE_6 ((WLBT_PBUS_BAAW_CBUS) + 0x6c)
/* VALUES */
#define WLBT_SHUB_BASE 0x11300000 //Orange3:0xA0 todo SCSC
#define WLBT_CBUS_BAAW_6_START 0xA0190000 /* APM BAAW5: CHUB INSR ?????? */
#define WLBT_CBUS_BAAW_6_END 0xA01C0000
#endif
/********************************************/
/* WLBT_PBUS_SYSREG              0x10850000 */
/********************************************/
#define WLBT_PBUS_SYSREG 0x0
#define WLAN_PROC_RMP_BOOT ((WLBT_PBUS_SYSREG) + 0x0400)
#define WPAN_PROC_RMP_BOOT ((WLBT_PBUS_SYSREG) + 0x0404)

#define CHIP_VERSION_ID_OFFSET ((WLBT_PBUS_SYSREG) + 0x0414)
#define CHIP_VERSION_ID_VER_MASK 0xFFFFFFFF /* [00:31] Version ID */
#define CHIP_VERSION_ID_IP_PMU 0x0000F000 /* [12:15] PMU ROM Rev */
#define CHIP_VERSION_ID_IP_MINOR 0x000F0000 /* [16:19] Minor Rev */
#define CHIP_VERSION_ID_IP_MAJOR 0x00F00000 /* [20:23] Major Rev */
#define CHIP_VERSION_ID_IP_PMU_SHIFT 12
#define CHIP_VERSION_ID_IP_MINOR_SHIFT 16
#define CHIP_VERSION_ID_IP_MAJOR_SHIFT 20
/*******************************************/
/* WLBT_PBUS_BOOT		0x10860000 */
/*******************************************/
#define WLBT_PBUS_BOOT 0x0
#define PMU_BOOT (WLBT_PBUS_BOOT + 0x0000)
#define PMU_BOOT_PMU_ACC 0x0 /* PMU has access to KARAM */
#define PMU_BOOT_AP_ACC 0x1 /* AP has access to KARAM */
#define PMU_BOOT_ACK (WLBT_PBUS_BOOT + 0x0004)
#define PMU_BOOT_COMPLETE 0x1 /* Boot ACK complete */
#define PMU_BOOT_RAM_START (WLBT_PBUS_BOOT + 0x10000)
#define PMU_BOOT_RAM_END (PMU_BOOT_RAM_START + 0xfff)

/* PMU COMMANDS */
#define PMU_AP_MB_MSG_NULL (0x00)
#define PMU_AP_MB_MSG_START_WLAN (0x01)
#define PMU_AP_MB_MSG_START_WPAN (0x02)
#define PMU_AP_MB_MSG_START_WLAN_WPAN (0x03)
#define PMU_AP_MB_MSG_RESET_WLAN (0x04)
#define PMU_AP_MB_MSG_RESET_WPAN (0x05)

/* PMU MAILBOXES */
#define AP2WB_MAILBOX (WLBT_PBUS_BOOT + 0x0008)
#define WB2AP_MAILBOX (WLBT_PBUS_BOOT + 0x000C)

/********************************************/
/* END PBUS_BASE                0x10800000  */
/********************************************/

/********************************************/
/* PMU_ALIVE                    0x12860000  */
/********************************************/
#define VGPIO_TX_MONITOR 0x0A14
#define VGPIO_TX_MON_BIT12 BIT(12)
/********************************************/
/* END PMU_ALIVE                0x12860000  */
/********************************************/

/* TODO:  check TZ stuff*/
/* TZASC (TrustZone Address Space Controller) configuration for Katmai onwards */
#define EXYNOS_SET_CONN_TZPC 0
#define SMC_CMD_CONN_IF (0x82000710)
#endif /* __MIF_REG_5515_H */
