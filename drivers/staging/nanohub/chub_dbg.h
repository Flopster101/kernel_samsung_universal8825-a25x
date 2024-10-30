/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver Debug
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *      Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */

#ifndef __CHUB_DEBUG_H
#define __CHUB_DEBUG_H

#include <linux/platform_device.h>
#include <soc/samsung/debug-snapshot.h>
#include "chub.h"

enum dbg_dump_area {
	DBG_NANOHUB_DD_AREA,
	DBG_IPC_AREA,
	DBG_GPR_AREA,
	DBG_CMU_AREA,
	DBG_SYS_AREA,
	DBG_WDT_AREA,
	DBG_TIMER_AREA,
	DBG_PWM_AREA,
	DBG_RTC_AREA,
	DBG_USI_AREA,
	DBG_SRAM_AREA,
	DBG_AREA_MAX
};

/*	CHUB dump gpr Registers */
#define REG_CHUB_DUMPGPR_CTRL (0x0)
#define REG_CHUB_DUMPGPR_PCR  (0x4)
#define REG_CHUB_DUMPGPR_GP0R (0x10)
#define REG_CHUB_DUMPGPR_GP1R (0x14)
#define REG_CHUB_DUMPGPR_GP2R (0x18)
#define REG_CHUB_DUMPGPR_GP3R (0x1c)
#define REG_CHUB_DUMPGPR_GP4R (0x20)
#define REG_CHUB_DUMPGPR_GP5R (0x24)
#define REG_CHUB_DUMPGPR_GP6R (0x28)
#define REG_CHUB_DUMPGPR_GP7R (0x2c)
#define REG_CHUB_DUMPGPR_GP8R (0x30)
#define REG_CHUB_DUMPGPR_GP9R (0x34)
#define REG_CHUB_DUMPGPR_GPAR (0x38)
#define REG_CHUB_DUMPGPR_GPBR (0x3c)
#define REG_CHUB_DUMPGPR_GPCR (0x40)
#define REG_CHUB_DUMPGPR_GPDR (0x44)
#define REG_CHUB_DUMPGPR_GPER (0x48)
#define REG_CHUB_DUMPGPR_GPFR (0x4c)

/*CHUB dump cmu Resisters*/
#define    REG_CMU_PLL_CON0_MUX_CLK_CHUB_BUS_USER          (0x600)
#define    REG_CMU_PLL_CON1_MUX_CLK_CHUB_BUS_USER          (0x604)
#define    REG_CMU_PLL_CON0_MUX_CLK_CHUB_PERI_USER         (0x610)
#define    REG_CMU_PLL_CON1_MUX_CLK_CHUB_PERI_USER         (0x614)
#define    REG_CMU_PLL_CON0_MUX_CLK_CHUB_RCO_USER          (0x620)
#define    REG_CMU_PLL_CON1_MUX_CLK_CHUB_RCO_USER          (0x624)
#define    REG_CMU_CHUB_CMU_CHUB_CONTROLLER_OPTION         (0x800)
#define    REG_CMU_CLKOUT_CON_BLK_CHUB_CMU_CHUB_CLKOUT0    (0x810)
#define    REG_CMU_CLKOUT_CON_BLK_CHUB_CMU_CHUB_CLKOUT1    (0x814)

#define    REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_BUS            (0x1000)
#define    REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_DMAILBOX       (0x1004)
#define    REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_HPMBUS         (0x1008)
#define    REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_I2C            (0x100C)
#define    REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_TIMER_FCLK     (0x1018)
#define    REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_USI0           (0x101C)
#define    REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_USI1           (0x1020)
#define    REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_USI2           (0x1024)

#define    REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_BUS            (0x1800)
#define    REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_DMAILBOX       (0x1804)
#define    REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_DMAILBOX_CORE  (0x1808)
#define    REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_HPMBUS         (0x180C)
#define    REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_I2C            (0x1810)
#define    REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_SERIAL_LIF_BCLK (0x1814)
#define    REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_SERIAL_LIF_BCLK_CORE (0x1818)
#define    REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_USI0           (0x181C)
#define    REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_USI1           (0x1820)
#define    REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_USI2           (0x1824)

#define CMU_REG_MAX (27)

static const u32 dump_chub_cmu_registers[] = {
	REG_CMU_PLL_CON0_MUX_CLK_CHUB_BUS_USER,
	REG_CMU_PLL_CON1_MUX_CLK_CHUB_BUS_USER,
	REG_CMU_PLL_CON0_MUX_CLK_CHUB_PERI_USER,
	REG_CMU_PLL_CON1_MUX_CLK_CHUB_PERI_USER,
	REG_CMU_PLL_CON0_MUX_CLK_CHUB_RCO_USER,
	REG_CMU_PLL_CON1_MUX_CLK_CHUB_RCO_USER,
	REG_CMU_CHUB_CMU_CHUB_CONTROLLER_OPTION,
	REG_CMU_CLKOUT_CON_BLK_CHUB_CMU_CHUB_CLKOUT0,
	REG_CMU_CLKOUT_CON_BLK_CHUB_CMU_CHUB_CLKOUT1,
	REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_BUS,
	REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_DMAILBOX,
	REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_HPMBUS,
	REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_I2C,
	REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_TIMER_FCLK,
	REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_USI0,
	REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_USI0,
	REG_CMU_CLK_CON_MUX_MUX_CLK_CHUB_USI0,
	REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_BUS,
	REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_DMAILBOX,
	REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_DMAILBOX_CORE,
	REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_HPMBUS,
	REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_I2C,
	REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_SERIAL_LIF_BCLK,
	REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_SERIAL_LIF_BCLK_CORE,
	REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_USI0,
	REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_USI1,
	REG_CMU_CLK_CON_DIV_DIV_CLK_CHUB_USI2,
};

/*CHUB dump sys Resisters*/
#define    REG_SYSREG_BUS_COMPONENET_DRCG_EN       (0x104)
#define    REG_SYSREG_PD_REQ                       (0x220)
#define    REG_SYSREG_EALY_WAKEUP_WINDOW_REQ       (0x230)
#define    REG_SYSREG_APM_UP_STATUS                (0x240)
#define    REG_SYSREG_CLEAR_VVALID                 (0x250)
#define    REG_SYSREG_CMGP_REQ_OUT                 (0x260)
#define    REG_SYSREG_CMGP_REQ_ACK_IN              (0x264)
#define    REG_SYSREG_OSCCLK_ENABLE                (0x270)
#define    REG_SYSREG_HWACG_CM4_CLKREQ             (0x428)

#define    REG_SYSREG_USI_CHUB00_SW_CONF           (0x2000)
#define    REG_SYSREG_I2C_CHUB00_SW_CONF           (0x2004)
#define    REG_SYSREG_USI_CHUB01_SW_CONF           (0x2010)
#define    REG_SYSREG_I2C_CHUB01_SW_CONF           (0x2014)
#define    REG_SYSREG_USI_CHUB02_SW_CONF           (0x2020)
#define    REG_SYSREG_I2C_CHUB02_SW_CONF           (0x2024)

#define    REG_SYSREG_USI_CHUB00_IPCLK             (0x3000)
#define    REG_SYSREG_I2C_CHUB00_IPCLK             (0x3004)
#define    REG_SYSREG_USI_CHUB01_IPCLK             (0x3010)
#define    REG_SYSREG_I2C_CHUB01_IPCLK             (0x3014)
#define    REG_SYSREG_USI_CHUB02_IPCLK             (0x3020)
#define    REG_SYSREG_I2C_CHUB02_IPCLK             (0x3024)

#define SYS_REG_MAX (21)

static const u32 dump_chub_sys_registers[] = {
	REG_SYSREG_BUS_COMPONENET_DRCG_EN,
	REG_SYSREG_PD_REQ,
	REG_SYSREG_EALY_WAKEUP_WINDOW_REQ,
	REG_SYSREG_APM_UP_STATUS,
	REG_SYSREG_CLEAR_VVALID,
	REG_SYSREG_CMGP_REQ_OUT,
	REG_SYSREG_CMGP_REQ_ACK_IN,
	REG_SYSREG_OSCCLK_ENABLE,
	REG_SYSREG_HWACG_CM4_CLKREQ,
	REG_SYSREG_USI_CHUB00_SW_CONF,
	REG_SYSREG_I2C_CHUB00_SW_CONF,
	REG_SYSREG_USI_CHUB01_SW_CONF,
	REG_SYSREG_I2C_CHUB01_SW_CONF,
	REG_SYSREG_USI_CHUB02_SW_CONF,
	REG_SYSREG_I2C_CHUB02_SW_CONF,
	REG_SYSREG_USI_CHUB00_IPCLK,
	REG_SYSREG_I2C_CHUB00_IPCLK,
	REG_SYSREG_USI_CHUB01_IPCLK,
	REG_SYSREG_I2C_CHUB01_IPCLK,
	REG_SYSREG_USI_CHUB02_IPCLK,
	REG_SYSREG_I2C_CHUB02_IPCLK,
};

/*CHUB dump wdt Resisters*/
#define REG_WDT_WTCON               (0x0)
#define REG_WDT_WTDAT               (0x4)

#define WDT_REG_MAX (2)

static const u32 dump_chub_wdt_registers[] = {
	REG_WDT_WTCON,
	REG_WDT_WTDAT,
};

/*CHUB dump timer Resisters*/
#define TIMER_TCSR (0x0)
#define TIMER_TRVR (0x4)
#define TIMER_TCVR (0x8)

#define TIMER_REG_MAX (3)

static const u32 dump_chub_timer_registers[] = {
	TIMER_TCSR,
	TIMER_TRVR,
	TIMER_TCVR,
};

/*CHUB dump pwm Resisters*/
#define REG_PWM_TCFG0           (0x00)
#define REG_PWM_TCFG1           (0x04)
#define REG_PWM_TCON            (0x08)
#define REG_PWM_TCNTB0          (0x0C)
#define REG_PWM_TCMPB0          (0x10)
#define REG_PWM_TCNTO0          (0x14)
#define REG_PWM_TCNTB1          (0x18)
#define REG_PWM_TCMPB1          (0x1C)
#define REG_PWM_TCNTO1          (0x20)
#define REG_PWM_TCNTB2          (0x24)
#define REG_PWM_TCMPB2          (0x28)
#define REG_PWM_TCNTO2          (0x2C)
#define REG_PWM_TCNTB3          (0x30)
#define REG_PWM_TCMPB3          (0x34)
#define REG_PWM_TCNTO3          (0x38)
#define REG_PWM_TCNPB4          (0x3C)
#define REG_PWM_TCNTO4          (0x40)
#define REG_PWM_TINT_CSTAT      (0x44)

#define PWM_REG_MAX (18)

static const u32 dump_chub_pwm_registers[] = {
	REG_PWM_TCFG0,
	REG_PWM_TCFG1,
	REG_PWM_TCON,
	REG_PWM_TCNTB0,
	REG_PWM_TCMPB0,
	REG_PWM_TCNTO0,
	REG_PWM_TCNTB1,
	REG_PWM_TCMPB1,
	REG_PWM_TCNTO1,
	REG_PWM_TCNTB2,
	REG_PWM_TCMPB2,
	REG_PWM_TCNTO2,
	REG_PWM_TCNTB3,
	REG_PWM_TCMPB3,
	REG_PWM_TCNTO3,
	REG_PWM_TCNPB4,
	REG_PWM_TCNTO4,
	REG_PWM_TINT_CSTAT,
};

/* DUMP_RTC_CHUB */
#define REG_RTC_INTP            (0x30)
#define REG_RTC_TICCON0         (0x38)
#define REG_RTC_TICCON1         (0x3C)
#define REG_RTC_RTCCON          (0x40)
#define REG_RTC_TICCNT0         (0x44)
#define REG_RTC_TICCNT1         (0x48)
#define REG_RTC_PRETICK         (0x4C)
#define REG_RTC_RTCALM          (0x50)
#define REG_RTC_ALMSEC          (0x54)
#define REG_RTC_ALMMIN          (0x58)
#define REG_RTC_ALMHOUR         (0x5C)
#define REG_RTC_ALMDAY          (0x60)
#define REG_RTC_ALMMON          (0x64)
#define REG_RTC_ALMYEAR         (0x68)
#define REG_RTC_BCDSEC          (0x70)
#define REG_RTC_BCDMIN          (0x74)
#define REG_RTC_BCDHOUR         (0x78)
#define REG_RTC_BCDDAY          (0x7C)
#define REG_RTC_BCDDAYWEEK      (0x80)
#define REG_RTC_BCDMON          (0x84)
#define REG_RTC_BCDYEAR         (0x88)
#define REG_RTC_CURTICCNT0      (0x90)
#define REG_RTC_CURTICCNT1      (0x94)

#define RTC_REG_MAX (23)

static const u32 dump_chub_rtc_registers[] = {
	REG_RTC_INTP,
	REG_RTC_TICCON0,
	REG_RTC_TICCON1,
	REG_RTC_RTCCON,
	REG_RTC_TICCNT0,
	REG_RTC_TICCNT1,
	REG_RTC_PRETICK,
	REG_RTC_RTCALM,
	REG_RTC_ALMSEC,
	REG_RTC_ALMMIN,
	REG_RTC_ALMHOUR,
	REG_RTC_ALMDAY,
	REG_RTC_ALMMON,
	REG_RTC_ALMYEAR,
	REG_RTC_BCDSEC,
	REG_RTC_BCDMIN,
	REG_RTC_BCDHOUR,
	REG_RTC_BCDDAY,
	REG_RTC_BCDDAYWEEK,
	REG_RTC_BCDMON,
	REG_RTC_BCDYEAR,
	REG_RTC_CURTICCNT0,
	REG_RTC_CURTICCNT1,
};

/*CHUB UIS Configure Resisters*/
#define    USI_REG_USI_CONFIG           (0xC0)
#define    USI_REG_MAX (20)

enum UsiProtocolType {
	USI_PROTOCOL_UART = 0x1,
	USI_PROTOCOL_SPI = 0x2,
	USI_PROTOCOL_I2C = 0x4,
};

/*UART*/
#define    REG_UART_ULCON	            (0x00)
#define    REG_UART_UCON	            (0x04)
#define    REG_UART_UFCON	            (0x08)
#define    REG_UART_UMCON	            (0x0C)
#define    REG_UART_UTRSTAT	            (0x10)
#define    REG_UART_UERSTAT	            (0x14)
#define    REG_UART_UFSTAT	            (0x18)
#define    REG_UART_UMSTAT	            (0x1C)
#define    REG_UART_UTXH	            (0x20)
#define    REG_UART_URXH	            (0x24)
#define    REG_UART_UBRDIV	            (0x28)
#define    REG_UART_FRACVAL	            (0x2C)
#define    REG_UART_INTP	            (0x30)
#define    REG_UART_INTS	            (0x34)
#define    REG_UART_INTM	            (0x38)
#define    REG_UART_UFLT_CONF               (0x40)

#define UART_REG_MAX (16)

static const u32 dump_chub_uart_registers[] = {
	REG_UART_ULCON,
	REG_UART_UCON,
	REG_UART_UFCON,
	REG_UART_UMCON,
	REG_UART_UTRSTAT,
	REG_UART_UERSTAT,
	REG_UART_UFSTAT,
	REG_UART_UMSTAT,
	REG_UART_UTXH,
	REG_UART_URXH,
	REG_UART_UBRDIV,
	REG_UART_FRACVAL,
	REG_UART_INTP,
	REG_UART_INTS,
	REG_UART_INTM,
	REG_UART_UFLT_CONF,
};

/*SPI*/
#define REG_CH_CFG             (0X00)
#define REG_MODE_CFG           (0x08)
#define REG_CS_REG             (0x0C)
#define REG_SPI_INT_EN         (0x10)
#define REG_SPI_STATUS         (0x14)
#define REG_SPI_TX_DATA        (0x18)
#define REG_SPI_RX_DATA        (0x1C)
#define REG_PACKET_CNT_REG     (0x20)
#define REG_PENDING_CLR_REG    (0x24)
#define REG_SWAP_CFG           (0x28)
#define REG_FB_CLK_SEL         (0x2C)

#define SPI_REG_MAX (11)

static const u32 dump_chub_spi_registers[] = {
	REG_CH_CFG,
	REG_MODE_CFG,
	REG_CS_REG,
	REG_SPI_INT_EN,
	REG_SPI_STATUS,
	REG_SPI_TX_DATA,
	REG_SPI_RX_DATA,
	REG_PACKET_CNT_REG,
	REG_PENDING_CLR_REG,
	REG_SWAP_CFG,
	REG_FB_CLK_SEL,
};

/*I2C*/
#define REG_I2C_CTL             (0x00)
#define REG_I2C_FIFO_CTL        (0x04)
#define REG_I2C_TRAILING_CTL    (0x08)
#define REG_I2C_INT_EN          (0x20)
#define REG_I2C_INT_STAT        (0x24)
#define REG_I2C_FIFO_STAT       (0x30)
#define REG_I2C_TXDATA          (0x34)
#define REG_I2C_RXDATA          (0x38)
#define REG_I2C_CONF            (0x40)
#define REG_I2C_CONF2           (0x44)
#define REG_I2C_TIMEOUT         (0x48)
#define REG_I2C_CMD             (0x4C)
#define REG_I2C_TRANS_STATUS    (0x50)
#define REG_I2C_TIMING_HS1      (0x54)
#define REG_I2C_TIMING_HS2      (0x58)
#define REG_I2C_TIMING_HS3      (0x5C)
#define REG_I2C_TIMING_FS1      (0x60)
#define REG_I2C_TIMING_FS2      (0x64)
#define REG_I2C_TIMING_FS3      (0x68)
#define REG_I2C_ADDR            (0x70)

#define I2C_REG_MAX (20)

static const u32 dump_chub_i2c_registers[] = {
	REG_I2C_CTL,
	REG_I2C_FIFO_CTL,
	REG_I2C_TRAILING_CTL,
	REG_I2C_INT_EN,
	REG_I2C_INT_STAT,
	REG_I2C_FIFO_STAT,
	REG_I2C_TXDATA,
	REG_I2C_RXDATA,
	REG_I2C_CONF,
	REG_I2C_CONF2,
	REG_I2C_TIMEOUT,
	REG_I2C_CMD,
	REG_I2C_TRANS_STATUS,
	REG_I2C_TIMING_HS1,
	REG_I2C_TIMING_HS2,
	REG_I2C_TIMING_HS3,
	REG_I2C_TIMING_FS1,
	REG_I2C_TIMING_FS2,
	REG_I2C_TIMING_FS3,
	REG_I2C_ADDR,
};

int chub_dbg_init(struct contexthub_ipc_info *chub, void *logbuf, int logbuf_size);
void chub_dbg_get_memory(struct device_node *node);
void chub_dbg_dump_hw(struct contexthub_ipc_info *chub, enum chub_err_type reason);
void chub_dbg_print_hw(struct contexthub_ipc_info *chub);
void chub_dbg_dump_on_reset(struct contexthub_ipc_info *chub);
void chub_dbg_dump_ram(struct contexthub_ipc_info *chub, enum chub_err_type reason);

void chub_dbg_register_dump_to_dss(uint32_t sram_phys, uint32_t sram_size);
#endif /* __CHUB_DEBUG_H */
