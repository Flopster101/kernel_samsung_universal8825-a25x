/*
 * PCIe phy driver for Samsung EXYNOS9830
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/exynos-pci-noti.h>
#include <linux/regmap.h>
#include "pcie-designware.h"
#include "pcie-exynos-common.h"
#include "pcie-exynos-rc.h"

#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#include <linux/exynos_otp.h>
#endif

/* avoid checking rx elecidle when access DBI */
void exynos_pcie_rc_phy_check_rx_elecidle(void *phy_pcs_base_regs, int val, int ch_num)
{

}

/* PHY all power down */
void exynos_pcie_rc_phy_all_pwrdn(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void __iomem *phy_base_regs = exynos_pcie->phy_base;
	void __iomem *phyudbg_base_regs = exynos_pcie->phyudbg_base;
	u32 val;

	if (exynos_pcie->chip_ver == 1) {
		//HSI1 PCIe GPIO retention release
		regmap_update_bits(exynos_pcie->pmureg,
				exynos_pcie->pmu_offset2,
				PCIE_PHY_CONTROL_MASK2, (0x3 << 15));
	}
	if (exynos_pcie->ch_num == 0) { //PCIE GEN2 channel
		if (exynos_pcie->chip_ver == 0)
		{
			val = readl(phy_base_regs + 0x204) & ~(0x3 << 2);
			writel(val, phy_base_regs + 0x204);

			val = readl(phyudbg_base_regs + 0xC804) & ~(0x3 << 8);
			writel(val, phyudbg_base_regs + 0xC804);

		}
		//writel(0xFF, phy_base_regs + 0x0208);
		//writel(0x30, phy_base_regs + 0x01B4);
		writel(0x2A, phy_base_regs + 0x1044);
		writel(0xAA, phy_base_regs + 0x1048);
		writel(0xA8, phy_base_regs + 0x104C);
		writel(0x80, phy_base_regs + 0x1050);
		writel(0x0A, phy_base_regs + 0x185C);
		udelay(1);
		writel(0xFF, phy_base_regs + 0x0208);
		udelay(1);
		writel(0x0A, phy_base_regs + 0x0580);
		writel(0xAA, phy_base_regs + 0x0928);

		//Common Bias, PLL off
		writel(0x0A, phy_base_regs + 0x00C);

		mdelay(1);
	}
	else { //PCIe GEN3 channel
		if (exynos_pcie->chip_ver == 0)
		{
			val = readl(phy_base_regs + 0x204) & ~(0x3 << 2);
			writel(val, phy_base_regs + 0x204);

			val = readl(phyudbg_base_regs + 0xC804) & ~(0x3 << 8);
			writel(val, phyudbg_base_regs + 0xC804);
		}
		//writel(0xFF, phy_base_regs + 0x0208);
		//writel(0x30, phy_base_regs + 0x01B4);

		writel(0x2A, phy_base_regs + 0x1044);
		writel(0xAA, phy_base_regs + 0x1048);
		writel(0xA8, phy_base_regs + 0x104C);
		writel(0x80, phy_base_regs + 0x1050);
		writel(0x0A, phy_base_regs + 0x185C);

		writel(0x2A, phy_base_regs + 0x2044);
		writel(0xAA, phy_base_regs + 0x2048);
		writel(0xA8, phy_base_regs + 0x204C);
		writel(0x80, phy_base_regs + 0x2050);
		writel(0x0A, phy_base_regs + 0x285C);

		writel(0xFF, phy_base_regs + 0x208);
		writel(0xFF, phy_base_regs + 0x228);

		writel(0x0A, phy_base_regs + 0x0580);
		writel(0xAA, phy_base_regs + 0x0928);

		writel(0x0A, phy_base_regs + 0x00C);

		udelay(10);
		val = readl(phyudbg_base_regs + 0xC700) | (0x1 << 1);
		writel(val, phyudbg_base_regs + 0xC700);
	}
}

/* PHY all power down clear */
void exynos_pcie_rc_phy_all_pwrdn_clear(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void __iomem *phy_base_regs = exynos_pcie->phy_base;
	void __iomem *phyudbg_base_regs = exynos_pcie->phyudbg_base;
	u32 val;

	if (exynos_pcie->ch_num == 0) { //PCIE GEN2 channel
		if (exynos_pcie->chip_ver == 1) {
			//HSI1 PCIe GPIO retention release
			regmap_update_bits(exynos_pcie->pmureg,
					exynos_pcie->pmu_offset2,
					PCIE_PHY_CONTROL_MASK2, (0x3 << 15));
		}

		if (exynos_pcie->chip_ver == 0)
		{
			//val = readl(phy_base_regs + 0x204) & ~(0x3 << 2);
			//writel(val, phy_base_regs + 0x204);

			//val = readl(phyudbg_base_regs + 0xC804) & ~(0x3 << 8);
			//writel(val, phyudbg_base_regs + 0xC804);
		}

		//writel(0x00, phy_base_regs + 0x01B4);
		//writel(0x00, phy_base_regs + 0x0208);
		writel(0x02, phy_base_regs + 0x0580);
		writel(0x55, phy_base_regs + 0x0928);
		mdelay(1);
		writel(0x00, phy_base_regs + 0xC);
		mdelay(1);
		writel(0x00, phy_base_regs + 0x208);
		writel(0x00, phy_base_regs + 0x1044);
		writel(0x00, phy_base_regs + 0x1048);
		writel(0x00, phy_base_regs + 0x104C);
		writel(0x00, phy_base_regs + 0x1050);
		writel(0x00, phy_base_regs + 0x185C);
		mdelay(1);
	}
	else { //PCIe GEN3 channel
		if (exynos_pcie->chip_ver == 0)
		{
			//val = readl(phy_base_regs + 0x204) & ~(0x3 << 2);
			//writel(val, phy_base_regs + 0x204);

			//val = readl(phyudbg_base_regs + 0xC804) & ~(0x3 << 8);
			//writel(val, phyudbg_base_regs + 0xC804);
		}

		//writel(0x00, phy_base_regs + 0x01B4);
		//writel(0x00, phy_base_regs + 0x0208);

		val = readl(phyudbg_base_regs + 0xC700) & ~(0x1 << 1);
		writel(val, phyudbg_base_regs + 0xC700);
		udelay(100);

		if (exynos_pcie->chip_ver == 1) {
			//HSI1 PCIe GPIO retention release
			regmap_update_bits(exynos_pcie->pmureg,
					exynos_pcie->pmu_offset2,
					PCIE_PHY_CONTROL_MASK2, (0x3 << 15));
		}

		writel(0x02, phy_base_regs + 0x0580);
		writel(0x55, phy_base_regs + 0x0928);
		mdelay(1);
		writel(0x00, phy_base_regs + 0xC);
		mdelay(1);
		writel(0x00, phy_base_regs + 0x208);
		writel(0x00, phy_base_regs + 0x228);

		writel(0x00, phy_base_regs + 0x1044);
		writel(0x00, phy_base_regs + 0x1048);
		writel(0x00, phy_base_regs + 0x104C);
		writel(0x00, phy_base_regs + 0x1050);
		writel(0x00, phy_base_regs + 0x185C);

		writel(0x00, phy_base_regs + 0x2044);
		writel(0x00, phy_base_regs + 0x2048);
		writel(0x00, phy_base_regs + 0x204C);
		writel(0x00, phy_base_regs + 0x2050);
		writel(0x00, phy_base_regs + 0x285C);
	}
}

/* PHY input clk change */
void exynos_pcie_rc_phy_input_clk_change(struct exynos_pcie *exynos_pcie, bool enable)
{

}

void exynos_pcie_rc_pcie_phy_otp_config(void *phy_base_regs, int ch_num)
{
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#else
	return ;
#endif
}
#define LCPLL_REF_CLK_SEL	(0x3 << 4)
void exynos_pcie_rc_pcie_phy_config(struct exynos_pcie *exynos_pcie, int ch_num)
{
	void __iomem *elbi_base_regs = exynos_pcie->elbi_base;
	void __iomem *soc_base_regs = exynos_pcie->soc_base;
	void __iomem *phy_base_regs = exynos_pcie->phy_base;
	void __iomem *phyudbg_base_regs = exynos_pcie->phyudbg_base;
	void __iomem *phy_pcs_base_regs = exynos_pcie->phy_pcs_base;
	void __iomem *sysreg_base_regs = exynos_pcie->sysreg_base;
	u32 i = 0;
	u32 ext_pll_lock = 0;
	u32 pll_lock = 0, cdr_lock = 0;
	u32 oc_done = 0;
	u32 lane = 1;
	u32 val;

#if IS_ENABLED(CONFIG_EXYNOS_OTP)
	/* PHY OTP Tuning bit configuration Setting */
	//exynos_pcie_phy_otp_config(phy_base_regs, ch_num);
#endif

	dev_info(exynos_pcie->pci->dev, "PCIe s5300 CAL ver0.0\n");

	if (exynos_pcie->ch_num == 0) {
		writel(0x00, phy_base_regs + 0x01B4);
		writel(0x00, phy_base_regs + 0x0208);
		udelay(10);

		/* soc_ctrl setting */
		//need to update soc_ctrl SFR
		writel(0x6, soc_base_regs + 0x4000);	//ELBI & Link clock switch TCXO to PCLK

		if (exynos_pcie->chip_ver == 1)
		{
			writel(0x3, phy_base_regs + 0x032C);	//PHY input clock un-gating
			writel(0x2, phyudbg_base_regs + 0xC804); //PHY input clock un-gating
		}

		/* SYSREG setting */
		val = readl(sysreg_base_regs) | (1 << 0);
		writel(val, sysreg_base_regs);	//Select PHY input clock. 1b'1 = TCXO, 1b'0 = External PLL clock

		val = readl(sysreg_base_regs) & ~(0x3 << 4);
		writel(val, sysreg_base_regs);	//Select PHY input clock. 2b'00 = TCXO

		/* device type setting */
		writel(0x4, elbi_base_regs + 0x0080);

		/* soft_pwr_rst */
		writel(0xF, elbi_base_regs + 0x3A4);
		writel(0xD, elbi_base_regs + 0x3A4);
		udelay(10);
		writel(0xF, elbi_base_regs + 0x3A4);
		udelay(10);

		/* pma rst assert*/
		writel(0x1, elbi_base_regs + 0x1404);
		writel(0x1, elbi_base_regs + 0x1408);
		writel(0x1, elbi_base_regs + 0x1400);
		writel(0x0, elbi_base_regs + 0x1404);
		writel(0x0, elbi_base_regs + 0x1408);
		writel(0x0, elbi_base_regs + 0x1400);

		/* pma_setting */

		//Common
		writel(0x88, phy_base_regs + 0x0000);
		writel(0x66, phy_base_regs + 0x001C);
		writel(0x00, phy_base_regs + 0x01F4);
		writel(0x80, phy_base_regs + 0x0510);
		writel(0x59, phy_base_regs + 0x0514);
		writel(0x11, phy_base_regs + 0x051C);
		writel(0x0E, phy_base_regs + 0x062C);
		writel(0x22, phy_base_regs + 0x0644);
		writel(0x03, phy_base_regs + 0x0688);
		writel(0x28, phy_base_regs + 0x06D4);
		writel(0x64, phy_base_regs + 0x0788);
		writel(0x64, phy_base_regs + 0x078C);
		writel(0x50, phy_base_regs + 0x0790);
		writel(0x50, phy_base_regs + 0x0794);
		writel(0x05, phy_base_regs + 0x0944);
		writel(0x05, phy_base_regs + 0x0948);
		writel(0x05, phy_base_regs + 0x094C);
		writel(0x05, phy_base_regs + 0x0950);

		//if (PCIe_TYPE == RC) {
		writel(0x00, phy_base_regs + 0x0590);
		writel(0xA0, phy_base_regs + 0x07F8);
		writel(0x09, phy_base_regs + 0x0730);

		//test with no relatin of CLKREQ @L1.2
		writel(0x03, phy_base_regs + 0x204);

		//Delay@L1.2 = 7.68us
		writel(0xC0, phy_base_regs + 0x344);


		//test with no relatin of CLKREQ @L1.2
		val = (readl(phy_base_regs + 0x40) & ~(0x7 << 1));
		writel(val, phy_base_regs + 0x40);
		val = (readl(phy_base_regs + 0x40) | (0x1 << 2));
		writel(val, phy_base_regs + 0x40);

		val = (readl(phy_base_regs + 0x2D4) & ~(0x1 << 1));
		writel(val, phy_base_regs + 0x2D4);
		val = (readl(phy_base_regs + 0x2D4) | (0x1 << 1));
		writel(val, phy_base_regs + 0x2D4);

		writel(0x80, phy_base_regs + 0x358);
		writel(0x01, phy_base_regs + 0x0018);
		//} else {
		//	writel(0x00, phy_base_regs + 0x0018);
		//}

		//lane
		for (i = 0; i < lane; i++) {
			phy_base_regs += (i * 0x1000);

			writel(0x04, phy_base_regs + 0x1140);
			writel(0x04, phy_base_regs + 0x1144);
			writel(0x04, phy_base_regs + 0x1148);
			writel(0x02, phy_base_regs + 0x114C);
			writel(0x00, phy_base_regs + 0x1150);
			writel(0x00, phy_base_regs + 0x1154);
			writel(0x00, phy_base_regs + 0x1158);
			writel(0x00, phy_base_regs + 0x115C);
			writel(0x1C, phy_base_regs + 0x12CC);
			writel(0x6C, phy_base_regs + 0x12DC);
			writel(0x29, phy_base_regs + 0x130C);
			writel(0x2F, phy_base_regs + 0x13B4);
			writel(0x05, phy_base_regs + 0x1A64);
			writel(0x05, phy_base_regs + 0x1A68);
			writel(0x05, phy_base_regs + 0x1A84);
			writel(0x05, phy_base_regs + 0x1A88);
			writel(0x00, phy_base_regs + 0x1A98);
			writel(0x00, phy_base_regs + 0x1A9C);
			writel(0x07, phy_base_regs + 0x1AA8);
			writel(0x00, phy_base_regs + 0x1AB8);
			writel(0x00, phy_base_regs + 0x1ABC);
			writel(0x03, phy_base_regs + 0x1BB0);
			writel(0x03, phy_base_regs + 0x1BB4);
			writel(0x06, phy_base_regs + 0x1BC0);
			writel(0x06, phy_base_regs + 0x1BC4);
			writel(0x01, phy_base_regs + 0x1BE8);
			writel(0x04, phy_base_regs + 0x1BF8);
			writel(0x00, phy_base_regs + 0x1C98);
			writel(0x81, phy_base_regs + 0x1CA4);

			//override pre_post
			writel(0x20, phy_base_regs + 0x1444);
			writel(0x10, phy_base_regs + 0x1448);
		}

		/* PCS setting */
		//FIXME
		writel(0x700DD, phy_pcs_base_regs + 0x154);

		writel(0x100B0604, phy_pcs_base_regs + 0x190);//New Guide
		writel(0x000300DE, phy_pcs_base_regs + 0x150);
		writel(0x16400000, phy_pcs_base_regs + 0x100);
		writel(0x08600000, phy_pcs_base_regs + 0x104);
		writel(0x18700000, phy_pcs_base_regs + 0x114);
		writel(0x00000080, phy_pcs_base_regs + 0x178);
		writel(0x00000018, phy_pcs_base_regs + 0x17C);

		//test with no relatin of CLKREQ @L1.2
		writel(0x000700D5, phy_pcs_base_regs + 0x154);
		writel(0x18500000, phy_pcs_base_regs + 0x114);
		writel(0x60700000, phy_pcs_base_regs + 0x124);
		writel(0x00000007, phy_pcs_base_regs + 0x174);
		writel(0x00000100, phy_pcs_base_regs + 0x178);
		writel(0x00000010, phy_pcs_base_regs + 0x17c);

		/* pma rst release */
		writel(0x1, elbi_base_regs + 0x1404);
		udelay(10);
		writel(0x1, elbi_base_regs + 0x1408);
		writel(0x1, elbi_base_regs + 0x1400);

		/* check pll & cdr lock */
		phy_base_regs = exynos_pcie->phy_base;
		for (i = 0; i < 1000; i++) {
			udelay(15);
			pll_lock = readl(phy_base_regs + 0x0A80) & (1 << 0);
			cdr_lock = readl(phy_base_regs + 0x15C0) & (1 << 4);

			if ((pll_lock != 0) && (cdr_lock != 0))
				break;
		}
		if ((pll_lock == 0) || (cdr_lock == 0)) {
			printk("PLL & CDR lock fail");
			//return 0;
		}

		/* check offset calibration */
		for (i = 0; i < 1000; i++) {
			udelay(10);
			oc_done = readl(phy_base_regs + 0x140C) & (1 << 7);

			if (oc_done != 0)
				break;
		}
		if (oc_done == 0) {
			printk("OC fail");
			//return 0;
		}

		/* udbg setting */
		//need to udbg base SFR
		if (exynos_pcie->chip_ver == 0)
		{
			val = readl(phyudbg_base_regs + 0xC800) | (0x7 << 12);
			writel(val, phyudbg_base_regs + 0xC800);
		}
		else if (exynos_pcie->chip_ver == 1)
		{
			//after phy setting, clear
			writel(0x0, phyudbg_base_regs + 0xC804);

			val = readl(phyudbg_base_regs + 0xC800) | (0x1 << 8);
			writel(val, phyudbg_base_regs + 0xC800);

			val = (readl(phyudbg_base_regs + 0xC80C) & ~(0xFFFF << 16));
			writel(val, phyudbg_base_regs + 0xC80C);
			val = (readl(phyudbg_base_regs + 0xC80C) | (0x18 << 16));
			writel(val, phyudbg_base_regs + 0xC80C);

			//L2 clock gating option, [1]=only clock gating / [0]=clock/power gating
			val = (readl(phyudbg_base_regs + 0xC800) & ~(0x1 << 6));
			writel(val, phyudbg_base_regs + 0xC800);

			//L1ss clock gating option, [1]=only clock gating / [0]=clock/power gating
			val = (readl(phyudbg_base_regs + 0xC800) & ~(0x1 << 5));
			writel(val, phyudbg_base_regs + 0xC800);

			//Override release pll_ref_clk_reqn for input CLK un-gating
			writel(0x0, phy_base_regs + 0x032C);	//PHY input clock un-gating #
		}
		//L1 exit off by DBI
		writel(0x1, elbi_base_regs + 0x1078);
	}
	else {
		lane = 2;
		writel(0x00, phy_base_regs + 0x01B4);
		writel(0x00, phy_base_regs + 0x0208);
		udelay(10);

		/* soc_ctrl setting */
		//need to update soc_ctrl SFR
		writel(0x6, soc_base_regs + 0x4000);	//ELBI & Link clock switch TCXO to PCLK

		if (exynos_pcie->chip_ver == 1)
		{
			writel(0x3, phy_base_regs + 0x032C);	//PHY input clock un-gating
		}

		/* External PLL seting */
		val = readl(phyudbg_base_regs + 0xC710) & ~(0x1 << 1);
		writel(val, phyudbg_base_regs + 0xC710);	//External PLL initialization

		val = readl(phyudbg_base_regs + 0xC700) & ~(0x1 << 1);
		writel(val, phyudbg_base_regs + 0xC700);	//Override External PLL RESETB

		/* check external pll lock */
		for (i = 0; i < 1000; i++) {
			udelay(1);
			ext_pll_lock = readl(phyudbg_base_regs + 0xC704) & (1 << 3);

			if (ext_pll_lock != 0)
				break;
		}
		if (ext_pll_lock == 0) {
			printk("External PLL lock fail");
			//return 0;
		}

		/* SYSREG setting */
		val = readl(sysreg_base_regs) & ~(1 << 0);
		writel(val, sysreg_base_regs);	//Select PHY input clock. 1b'1 = TCXO, 1b'0 = External PLL clock

		val = readl(sysreg_base_regs) | (0x3 << 4);
		writel(val, sysreg_base_regs);	//Select PHY input clock. 2b'11 = External PLL clock

		/* device type setting */
		writel(0x4, elbi_base_regs + 0x0080);

		/* soft_pwr_rst */
		writel(0xF, elbi_base_regs + 0x3A4);
		writel(0xD, elbi_base_regs + 0x3A4);
		udelay(10);
		writel(0xF, elbi_base_regs + 0x3A4);
		udelay(10);

		/* pma rst assert*/
		writel(0x1, elbi_base_regs + 0x1404);
		writel(0x1, elbi_base_regs + 0x1408);
		writel(0x1, elbi_base_regs + 0x1400);
		writel(0x0, elbi_base_regs + 0x1404);
		writel(0x0, elbi_base_regs + 0x1408);
		writel(0x0, elbi_base_regs + 0x1400);

		/* pma_setting */

		//Common
		writel(0x88, phy_base_regs + 0x0000);
		writel(0x66, phy_base_regs + 0x001C);
		writel(0x00, phy_base_regs + 0x01F4);
		writel(0x80, phy_base_regs + 0x0510);
		writel(0x59, phy_base_regs + 0x0514);
		writel(0x11, phy_base_regs + 0x051C);
		writel(0x0E, phy_base_regs + 0x062C);
		writel(0x22, phy_base_regs + 0x0644);
		writel(0x03, phy_base_regs + 0x0688);
		writel(0x28, phy_base_regs + 0x06D4);
		writel(0x64, phy_base_regs + 0x0788);
		writel(0x64, phy_base_regs + 0x078C);
		writel(0x50, phy_base_regs + 0x0790);
		writel(0x50, phy_base_regs + 0x0794);
		writel(0x05, phy_base_regs + 0x0944);
		writel(0x05, phy_base_regs + 0x0948);
		writel(0x05, phy_base_regs + 0x094C);
		writel(0x05, phy_base_regs + 0x0950);

		/* REFCLK 38.4Mhz to External PLL path */
		//MUX3, select External PLL path
		writel(0x02, phy_base_regs + 0x0590);

		//MUX2, select External PLL path
		writel(0xB0, phy_base_regs + 0x07F8);

		//LCPLL DIV disable
		writel(0x08, phy_base_regs + 0x730);

		//test with no relatin of CLKREQ @L1.2
		writel(0x03, phy_base_regs + 0x0204);

		//Delay@L1.2 = 7.68us
		writel(0xC0, phy_base_regs + 0x344);

		//test with no relatin of CLKREQ @L1.2
		val = (readl(phy_base_regs + 0x40) & ~(0x7 << 1));
		writel(val, phy_base_regs + 0x40);
		val = (readl(phy_base_regs + 0x40) | (0x1 << 2));
		writel(val, phy_base_regs + 0x40);

		val = (readl(phy_base_regs + 0x2D4) & ~(0x1 << 1));
		writel(val, phy_base_regs + 0x2D4);
		val = (readl(phy_base_regs + 0x2D4) | (0x1 << 1));
		writel(val, phy_base_regs + 0x2D4);

		writel(0x80, phy_base_regs + 0x358);

		//if (PCIe_TYPE == RC) {
			writel(0x01, phy_base_regs + 0x0018);
		//} else {
		//	writel(0x00, phy_base_regs + 0x0018);
		//}

		//lane
		for (i = 0; i < lane; i++) {
			phy_base_regs += (i * 0x1000);

			writel(0x04, phy_base_regs + 0x1140);
			writel(0x04, phy_base_regs + 0x1144);
			writel(0x04, phy_base_regs + 0x1148);
			writel(0x02, phy_base_regs + 0x114C);
			writel(0x00, phy_base_regs + 0x1150);
			writel(0x00, phy_base_regs + 0x1154);
			writel(0x00, phy_base_regs + 0x1158);
			writel(0x00, phy_base_regs + 0x115C);
			writel(0x1C, phy_base_regs + 0x12CC);
			writel(0x6C, phy_base_regs + 0x12DC);
			writel(0x29, phy_base_regs + 0x130C);
			writel(0x2F, phy_base_regs + 0x13B4);
			writel(0x05, phy_base_regs + 0x1A64);
			writel(0x05, phy_base_regs + 0x1A68);
			writel(0x05, phy_base_regs + 0x1A84);
			writel(0x05, phy_base_regs + 0x1A88);
			writel(0x00, phy_base_regs + 0x1A98);
			writel(0x00, phy_base_regs + 0x1A9C);
			writel(0x07, phy_base_regs + 0x1AA8);
			writel(0x00, phy_base_regs + 0x1AB8);
			writel(0x00, phy_base_regs + 0x1ABC);
			writel(0x03, phy_base_regs + 0x1BB0);
			writel(0x03, phy_base_regs + 0x1BB4);
			writel(0x06, phy_base_regs + 0x1BC0);
			writel(0x06, phy_base_regs + 0x1BC4);
			writel(0x01, phy_base_regs + 0x1BE8);
			writel(0x04, phy_base_regs + 0x1BF8);
			writel(0x00, phy_base_regs + 0x1C98);
			writel(0x81, phy_base_regs + 0x1CA4);

			//override pre_post
			writel(0x20, phy_base_regs + 0x1444);
			writel(0x10, phy_base_regs + 0x1448);
		}

		/* PCS setting */
		//FIXME
		writel(0x700DD, phy_pcs_base_regs + 0x154);

		//when L1ss & L2 entering, Add pll_en and bias_en delay
		writel(0x100B0604, phy_pcs_base_regs + 0x190);
		//P2 CTRL setting
		writel(0x000300DE, phy_pcs_base_regs + 0x150);

		//when P0->P1.CPM, P1 entering, operate timer[0]
		writel(0x16400000, phy_pcs_base_regs + 0x100);

		//when P0->P2 entering, operate timer[2]
		writel(0x08600000, phy_pcs_base_regs + 0x104);

		//when P1->P2 entering, operate timer[3]
		writel(0x18700000, phy_pcs_base_regs + 0x114);

		//timer[2]
		writel(0x00000080, phy_pcs_base_regs + 0x178);

		//timer[3]
		writel(0x00000018, phy_pcs_base_regs + 0x17c);

		//test with no relatin of CLKREQ @L1.2
		writel(0x000700D5, phy_pcs_base_regs + 0x154);
		writel(0x18500000, phy_pcs_base_regs + 0x114);
		writel(0x60700000, phy_pcs_base_regs + 0x124);
		writel(0x00000007, phy_pcs_base_regs + 0x174);
		writel(0x00000100, phy_pcs_base_regs + 0x178);
		writel(0x00000010, phy_pcs_base_regs + 0x17c);

		/* pma rst release */
		writel(0x1, elbi_base_regs + 0x1404);
		udelay(10);
		writel(0x1, elbi_base_regs + 0x1408);
		writel(0x1, elbi_base_regs + 0x1400);

		/* check pll & cdr lock */
		phy_base_regs = exynos_pcie->phy_base;
		for (i = 0; i < 1000; i++) {
			udelay(1);
			pll_lock = readl(phy_base_regs + 0x0A80) & (1 << 0);
			cdr_lock = readl(phy_base_regs + 0x15C0) & (1 << 4);

			if ((pll_lock != 0) && (cdr_lock != 0))
				break;
		}
		if ((pll_lock == 0) || (cdr_lock == 0)) {
			printk("PLL & CDR lock fail");
			//return 0;
		}

		/* check offset calibration */
		for (i = 0; i < 1000; i++) {
			udelay(10);
			oc_done = readl(phy_base_regs + 0x140C) & (1 << 7);

			if (oc_done != 0)
				break;
		}
		if (oc_done == 0) {
			printk("OC fail");
			//return 0;
		}

		/* udbg setting */
		//need to udbg base SFR
		if (exynos_pcie->chip_ver == 0)
		{
			val = readl(phyudbg_base_regs + 0xC710) | (1 << 8);
			writel(val, phyudbg_base_regs + 0xC710);	//when entring L2, External PLL clock gating
		}
		else if (exynos_pcie->chip_ver == 1)
		{
			val = (readl(phyudbg_base_regs + 0xC710) & ~(0x3 << 8)) | (0x3 << 8);
			writel(val, phyudbg_base_regs + 0xC710); //when entring L2, External PLL clock gating

			writel(0x0, phy_base_regs + 0x032C);	//PHY input clock un-gating
		}

		val = readl(phy_base_regs + 0x204);
		val = readl(phy_base_regs + 0x344);

		val = readl(phy_pcs_base_regs + 0x114);
		val = readl(phy_pcs_base_regs + 0x124);
		val = readl(phy_pcs_base_regs + 0x174);
		val = readl(phy_pcs_base_regs + 0x17C);

		val = readl(phyudbg_base_regs + 0xC800);
		val = val | (0x1 << 5);
		writel(val, phyudbg_base_regs + 0xC800);

		//L1 exit off by DBI
		writel(0x1, elbi_base_regs + 0x1078);
	}
}

int exynos_pcie_rc_eom(struct device *dev, void *phy_base_regs)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct device_node *np = dev->of_node;
	unsigned int val;
	unsigned int num_of_smpl;
	unsigned int lane_width = 1;
	unsigned int timeout;
	int i, ret;
	int test_cnt = 0;
	struct pcie_eom_result **eom_result;

	u32 phase_sweep = 0;
	u32 vref_sweep = 0;
	u32 err_cnt = 0;
	u32 err_cnt_13_8;
	u32 err_cnt_7_0;

	dev_info(dev, "[%s] START! \n", __func__);

	ret = of_property_read_u32(np, "num-lanes", &lane_width);
	if (ret) {
		dev_err(dev, "[%s] failed to get num of lane, lane width = 0\n", __func__);
		lane_width = 0;
	} else
		dev_info(dev, "[%s] num-lanes : %d\n", __func__, lane_width);

	/* eom_result[lane_num][test_cnt] */
	eom_result = kzalloc(sizeof(struct pcie_eom_result*) * lane_width, GFP_KERNEL);
	for (i = 0; i < lane_width; i ++) {
		eom_result[i] = kzalloc(sizeof(struct pcie_eom_result) *
				EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX, GFP_KERNEL);
	}
	if (eom_result == NULL) {
		return -ENOMEM;
	}
	exynos_pcie->eom_result = eom_result;

	num_of_smpl = 0xf;

	for (i = 0; i < lane_width; i++)
	{
		val = readl(phy_base_regs + RX_EFOM_MODE);
		val = val | (0x3 << 4);
		writel(val, phy_base_regs + RX_EFOM_MODE);

		writel(0x27, phy_base_regs + RX_SSLMS_ADAP_HOLD_PMAD);

		val = readl(phy_base_regs + RX_EFOM_MODE);
		val = val & ~(0x7 << 0);
		writel(val, phy_base_regs + RX_EFOM_MODE);
		val = readl(phy_base_regs + RX_EFOM_MODE);
		val = val | (0x4 << 0);
		writel(val, phy_base_regs + RX_EFOM_MODE);

		writel(0x0, phy_base_regs + RX_EFOM_NUM_OF_SAMPLE_13_8);
		writel(num_of_smpl, phy_base_regs + RX_EFOM_NUM_OF_SAMPLE_7_0);
		udelay(10);

		for (phase_sweep = 0; phase_sweep < PHASE_MAX; phase_sweep++)
		{
			val = readl(phy_base_regs + RX_EFOM_EOM_PH_SEL);
			val = val & ~(0xff << 0);
			writel(val, phy_base_regs + RX_EFOM_EOM_PH_SEL);
			val = readl(phy_base_regs + RX_EFOM_EOM_PH_SEL);
			val = val | (phase_sweep << 0);
			writel(val, phy_base_regs + RX_EFOM_EOM_PH_SEL);

			for (vref_sweep = 0; vref_sweep < VREF_MAX; vref_sweep++)
			{
				writel(vref_sweep, phy_base_regs + RX_EFOM_DFE_VREF_CTRL);
				val = readl(phy_base_regs + RX_EFOM_START);
				val = val | (1 << 0);
				writel(val, phy_base_regs + RX_EFOM_START);

				timeout = 0;
				do {
					if (timeout == 100) {
						dev_err(dev, "[%s] timeout happened \n", __func__);
						return false;
					}

					udelay(1);
					val = readl(phy_base_regs + RX_EFOM_DONE) & (0x1);

					timeout++;
				} while(val != 0x1);

				err_cnt_13_8 = readl(phy_base_regs + MON_RX_EFOM_ERR_CNT_13_8) << 8;
				err_cnt_7_0 = readl(phy_base_regs + MON_RX_EFOM_ERR_CNT_7_0);
				err_cnt = err_cnt_13_8 | err_cnt_7_0;

				//dev_dbg(dev, "[%s] %d,%d : %d %d %d\n",
				//		__func__, i, test_cnt, phase_sweep, vref_sweep, err_cnt);

				//save result
				eom_result[i][test_cnt].phase = phase_sweep;
				eom_result[i][test_cnt].vref = vref_sweep;
				eom_result[i][test_cnt].err_cnt = err_cnt;
				test_cnt++;

				val = readl(phy_base_regs + RX_EFOM_START);
				val = val & ~(1 << 0);
				writel(val, phy_base_regs + RX_EFOM_START);
				udelay(10);
			}
		}
		writel(0x21, phy_base_regs + RX_SSLMS_ADAP_HOLD_PMAD);
		writel(0x0, phy_base_regs + RX_EFOM_MODE);

		/* goto next lane */
		phy_base_regs += 0x1000;
		test_cnt = 0;
	}

	return 0;
}

void exynos_pcie_rc_phy_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	dev_info(pci->dev, "Initialize PHY functions.\n");

	exynos_pcie->phy_ops.phy_check_rx_elecidle =
		exynos_pcie_rc_phy_check_rx_elecidle;
	exynos_pcie->phy_ops.phy_all_pwrdn = exynos_pcie_rc_phy_all_pwrdn;
	exynos_pcie->phy_ops.phy_all_pwrdn_clear = exynos_pcie_rc_phy_all_pwrdn_clear;
	exynos_pcie->phy_ops.phy_config = exynos_pcie_rc_pcie_phy_config;
	exynos_pcie->phy_ops.phy_eom = exynos_pcie_rc_eom;
	exynos_pcie->phy_ops.phy_input_clk_change = exynos_pcie_rc_phy_input_clk_change;
}
EXPORT_SYMBOL(exynos_pcie_rc_phy_init);

static void exynos_pcie_quirks(struct pci_dev *dev)
{
#if IS_ENABLED(CONFIG_EXYNOS_PCI_PM_ASYNC)
	device_enable_async_suspend(&dev->dev);
	pr_info("[%s:pcie_1] enable async_suspend\n", __func__);
#else
	device_disable_async_suspend(&dev->dev);
	pr_info("[%s] async suspend disabled\n", __func__);
#endif
}
DECLARE_PCI_FIXUP_FINAL(PCI_ANY_ID, PCI_ANY_ID, exynos_pcie_quirks);

MODULE_AUTHOR("Kisang Lee <kisang80.lee@samsung.com>");
MODULE_AUTHOR("Kwangho Kim <kwangho2.kim@samsung.com>");
MODULE_AUTHOR("Seungo Jang <seungo.jang@samsung.com>");
MODULE_DESCRIPTION("Samsung PCIe host controller driver");
MODULE_LICENSE("GPL v2");
