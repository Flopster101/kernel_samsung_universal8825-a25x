/*
 * Samsung EXYNOS SoC series MIPI CSI/DSI D/C-PHY driver
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#if IS_ENABLED(CONFIG_EXYNOS_PMU_IF)
#include <soc/samsung/exynos-pmu-if.h>
#else
#include <soc/samsung/exynos-pmu.h>
#endif

/* the maximum number of PHY for each module */
#define EXYNOS_MIPI_PHYS_NUM	4

#define EXYNOS_MIPI_PHY_MAX_REGULATORS 2
#define EXYNOS_MIPI_PHY_ISO_BYPASS  (1 << 0)
#define EXYNOS_MIPI_PHY_PG_SCPRE		0
#define EXYNOS_MIPI_PHY_PG_SCALL		1
#define EXYNOS_MIPI_PHY_PG_SCPRE_FEEDBACK	2
#define EXYNOS_MIPI_PHY_PG_SCALL_FEEDBACK	3
#define EXYNOS_MIPI_PHY_PG_ISO_EN		4

#define MIPI_PHY_MxSx_UNIQUE	(0 << 1)
#define MIPI_PHY_MxSx_SHARED	(1 << 1)
#define MIPI_PHY_MxSx_INIT_DONE	(2 << 1)

#define HIWORD(d) ((u16)(((u32)(d)) >> 16))
#define LOWORD(d) ((u16)(u32)(d))

enum exynos_mipi_phy_owner {
	EXYNOS_MIPI_PHY_OWNER_DSIM = 0,
	EXYNOS_MIPI_PHY_OWNER_CSIS = 1,
};

/* per MIPI-PHY module */
struct exynos_mipi_phy_data {
	u8 flags;
	int active_count;
	spinlock_t slock;
};

#define MKVER(ma, mi)	(((ma) << 16) | (mi))
enum phy_infos {
	VERSION,
	TYPE,
	LANES,
	SPEED,
	SETTLE,
};

struct exynos_mipi_phy_cfg {
	u16 major;
	u16 minor;
	u16 mode;
	/* u32 max_speed */
	int (*set)(void __iomem *regs, int option, u32 *info);
};

/* per DT MIPI-PHY node, can have multiple elements */
struct exynos_mipi_phy {
	struct device *dev;
	spinlock_t slock;
	struct regmap *reg_pmu;
	struct regmap *reg_reset;
	enum exynos_mipi_phy_owner owner;
	struct regulator *regulators[EXYNOS_MIPI_PHY_MAX_REGULATORS];
	struct mipi_phy_desc {
		struct phy *phy;
		struct exynos_mipi_phy_data *data;
		unsigned int index;
		unsigned int iso_offset;
		unsigned int pg_offset;
		unsigned int rst_bit;
		void __iomem *regs; /* clock */
		void __iomem *regs_lane; /* lane */
	} phys[EXYNOS_MIPI_PHYS_NUM];
};

static void wait_feedback_bit(struct regmap *reg_pmu,
	unsigned int ofs, unsigned int bit, unsigned int exp)
{
	u32 feedback, timeout;

	feedback = exp ? 0 : 1;
	timeout = 1000;
	while (timeout && (feedback != exp)) {
		udelay(10);
		timeout--;
		regmap_read(reg_pmu, ofs, &feedback);
		feedback = (feedback >> bit) & 0x1;
	}
}

static void __set_phy_power_gating(struct regmap *reg_pmu,
				unsigned int ofs, unsigned int on)
{
	if (!on)
		regmap_update_bits(reg_pmu, ofs,
				BIT(EXYNOS_MIPI_PHY_PG_ISO_EN),
				BIT(EXYNOS_MIPI_PHY_PG_ISO_EN));

	regmap_update_bits(reg_pmu, ofs,
			BIT(EXYNOS_MIPI_PHY_PG_SCALL),
			on ? 0 : BIT(EXYNOS_MIPI_PHY_PG_SCALL));
	wait_feedback_bit(reg_pmu, ofs,
			EXYNOS_MIPI_PHY_PG_SCALL_FEEDBACK,
			on ? 0 : 1);

	regmap_update_bits(reg_pmu, ofs,
			BIT(EXYNOS_MIPI_PHY_PG_SCPRE),
			on ? 0 : BIT(EXYNOS_MIPI_PHY_PG_SCPRE));
	wait_feedback_bit(reg_pmu, ofs,
			EXYNOS_MIPI_PHY_PG_SCPRE_FEEDBACK,
			on ? 0 : 1);

	if (on)
		regmap_update_bits(reg_pmu, ofs,
				BIT(EXYNOS_MIPI_PHY_PG_ISO_EN), 0);
}

/* 1: Isolation bypass, 0: Isolation enable */
static int __set_phy_isolation(struct regmap *reg_pmu,
				unsigned int iso_ofs,
				unsigned int pg_ofs,
				unsigned int on)
{
	unsigned int val;
	int ret = 0;

	if (!reg_pmu)
		return 0;

	val = on ? EXYNOS_MIPI_PHY_ISO_BYPASS : 0;

	if (iso_ofs > 0) {
		ret = regmap_update_bits(reg_pmu, iso_ofs,
				EXYNOS_MIPI_PHY_ISO_BYPASS, val);
		if (ret)
			pr_err("%s failed to %s PHY isolation 0x%x\n",
				__func__, on ? "set" : "clear", iso_ofs);
	}

	if (pg_ofs > 0)
		__set_phy_power_gating(reg_pmu, pg_ofs, on);

	if (IS_ENABLED(CONFIG_PABLO_V10_0_0)) {
		regmap_update_bits(reg_pmu, 0xa88, BIT(4), 0);
		regmap_update_bits(reg_pmu, 0xa8c, BIT(4), 0);
		regmap_update_bits(reg_pmu, 0xa90, BIT(4), 0);
		regmap_update_bits(reg_pmu, 0xa94, BIT(4), 0);
		regmap_update_bits(reg_pmu, 0xa98, BIT(4), 0);
		regmap_update_bits(reg_pmu, 0xa9c, BIT(4), 0);
		regmap_update_bits(reg_pmu, 0xaa0, BIT(4), 0);
	}

	pr_debug("%s iso 0x%x, pg 0x%x, on 0x%x\n", __func__, iso_ofs, pg_ofs, on);

	return ret;
}

/* 1: Enable reset -> release reset, 0: Enable reset */
static int __set_phy_reset(struct regmap *reg_reset,
		unsigned int bit, unsigned int on)
{
	unsigned int cfg;
	int ret = 0;

	if (!reg_reset)
		return 0;

	if (on) {
		/* reset release */
		ret = regmap_update_bits(reg_reset, 0, BIT(bit), BIT(bit));
		if (ret)
			pr_err("%s failed to release reset PHY(%d)\n",
					__func__, bit);
	} else {
		/* reset assertion */
		ret = regmap_update_bits(reg_reset, 0, BIT(bit), 0);
		if (ret)
			pr_err("%s failed to reset PHY(%d)\n", __func__, bit);
	}

	regmap_read(reg_reset, 0, &cfg);
	pr_debug("%s bit=%d, cfg=0x%x\n", __func__, bit, cfg);

	return ret;
}

static int __set_phy_init(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	unsigned int cfg;
	int ret;

	/* Skip phy control for ZEBU */
	if (IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ) || !state->reg_pmu)
		return 0;

	ret = regmap_read(state->reg_pmu,
			phy_desc->iso_offset, &cfg);
	if (ret) {
		dev_err(state->dev, "%s Can't read 0x%x\n",
				__func__, phy_desc->iso_offset);
		ret = -EINVAL;
		goto phy_exit;
	}

	/* Add INIT_DONE flag when ISO is already bypass(LCD_ON_UBOOT) */
	if (cfg & EXYNOS_MIPI_PHY_ISO_BYPASS)
		phy_desc->data->flags |= MIPI_PHY_MxSx_INIT_DONE;

phy_exit:
	return ret;
}

static int __set_phy_ldo(struct exynos_mipi_phy *state, bool on)
{
	int ret = 0;
	if (on) {
		/* [on sequence]
		*  (general) 0.85V on  -> 1.8V on  -> 1.2V on
		*  (current) <1.2V on> -> 0.85V on -> 1.8V on
		*  >> prohibition : 0.85 off -> 1.2 on state
		*/
		if (state->regulators[0]) {
			ret = regulator_enable(state->regulators[0]);
			if (ret) {
				dev_err(state->dev, "phy regulator 0.85V enable failed\n");
				return ret;
			}
		}
		usleep_range(100, 101);

		if (state->regulators[1]) {
			ret = regulator_enable(state->regulators[1]);
			if (ret) {
				dev_err(state->dev, "phy regulator 1.80V enable failed\n");
				return ret;
			}
		}
		usleep_range(100, 101);
	} else {
		/* [off sequence]
		* (current) <1.2V on> -> 1.8V off -> 0.85V off
		*/
		if (state->regulators[1]) {
			ret = regulator_disable(state->regulators[1]);
			if (ret) {
				dev_err(state->dev, "phy regulator 1.80V disable failed\n");
				return ret;
			}
		}
		usleep_range(1000, 1001);

		if (state->regulators[0]) {
			ret = regulator_disable(state->regulators[0]);
			if (ret) {
				dev_err(state->dev, "phy regulator 0.85V disable failed\n");
				return ret;
			}
		}
	}
	return ret;
}

static int __set_phy_alone(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&state->slock, flags);

	if (on) {
		ret = __set_phy_isolation(state->reg_pmu,
				phy_desc->iso_offset,
				phy_desc->pg_offset, on);
		if (ret)
			goto phy_exit;
	} else {
		ret = __set_phy_isolation(state->reg_pmu,
				phy_desc->iso_offset,
				phy_desc->pg_offset, on);
	}

phy_exit:
	pr_debug("%s iso 0x%x, pg 0x%x, reset 0x%x\n", __func__,
					phy_desc->iso_offset,
					phy_desc->pg_offset,
					phy_desc->rst_bit);

	spin_unlock_irqrestore(&state->slock, flags);

	return ret;
}

static int __set_phy_share(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&phy_desc->data->slock, flags);

	on ? ++(phy_desc->data->active_count) : --(phy_desc->data->active_count);

	/* If phy is already initialization(power_on) */
	if (state->owner == EXYNOS_MIPI_PHY_OWNER_DSIM &&
			phy_desc->data->flags & MIPI_PHY_MxSx_INIT_DONE) {
		phy_desc->data->flags &= (~MIPI_PHY_MxSx_INIT_DONE);
		spin_unlock_irqrestore(&phy_desc->data->slock, flags);
		return ret;
	}

	if (on) {
		/* Isolation bypass when reference count is 1 */
		if (phy_desc->data->active_count) {
			ret = __set_phy_isolation(state->reg_pmu,
					phy_desc->iso_offset,
					phy_desc->pg_offset, on);
			if (ret)
				goto phy_exit;
		}
	} else {
		/* Isolation enabled when reference count is zero */
		if (phy_desc->data->active_count == 0)
			ret = __set_phy_isolation(state->reg_pmu,
					phy_desc->iso_offset,
					phy_desc->pg_offset, on);
	}

phy_exit:
	pr_debug("%s iso 0x%x, pg 0x%x, reset 0x%x\n", __func__,
					phy_desc->iso_offset,
					phy_desc->pg_offset,
					phy_desc->rst_bit);

	spin_unlock_irqrestore(&phy_desc->data->slock, flags);

	return ret;
}

static void __set_def_cfg_0800_0000_bias_pll(void)
{
	void __iomem *bias;

	/* BIAS/PLL */
	bias = ioremap(0x170F1000, 0x1000);
	writel(0x00000000, bias + 0x000);
	writel(0x00000000, bias + 0x004);
	writel(0x00000000, bias + 0x008);
	writel(0x00000000, bias + 0x00C);
	writel(0x00000000, bias + 0x010);
	writel(0x00000000, bias + 0x014);
	writel(0x00000000, bias + 0x100);
	writel(0x00000000, bias + 0x104);
	writel(0x00000000, bias + 0x108);
	writel(0x00000000, bias + 0x10C);
	writel(0x00000000, bias + 0x110);
	writel(0x00000000, bias + 0x114);
	writel(0x00000000, bias + 0x118);
	writel(0x00000000, bias + 0x11c);
	writel(0x00000000, bias + 0x120);
	writel(0x00000000, bias + 0x124);
	iounmap(bias);
}

static void __set_def_cfg_0800_0000_combo(void __iomem *regs)
{
	int i;

	/* SC */
	writel(0x00000000, regs + 0x000);
	writel(0x00000000, regs + 0x004);
	writel(0x00000000, regs + 0x008);
	writel(0x00000000, regs + 0x00C);
	writel(0x00000000, regs + 0x010);
	writel(0x00000000, regs + 0x014);
	writel(0x00000000, regs + 0x018);
	writel(0x00000000, regs + 0x01C);
	writel(0x00000000, regs + 0x030);
	writel(0x00000000, regs + 0x040);
	writel(0x00000000, regs + 0x070);
	writel(0x00000000, regs + 0x074);

	/* CSDs */
	for (i = 0; i < 3; i++) {
		writel(0x00000000, regs + 0x100 + (i * 0x100));
		writel(0x00000000, regs + 0x104 + (i * 0x100));
		writel(0x00000000, regs + 0x108 + (i * 0x100));
		writel(0x00000000, regs + 0x10C + (i * 0x100));
		writel(0x00000000, regs + 0x110 + (i * 0x100));
		writel(0x00000000, regs + 0x114 + (i * 0x100));
		writel(0x00000000, regs + 0x118 + (i * 0x100));
		writel(0x00000000, regs + 0x11C + (i * 0x100));
		writel(0x00000000, regs + 0x120 + (i * 0x100));
		writel(0x00000000, regs + 0x124 + (i * 0x100));
		writel(0x00000000, regs + 0x130 + (i * 0x100));
		writel(0x00000000, regs + 0x134 + (i * 0x100));
		writel(0x00000000, regs + 0x138 + (i * 0x100));
		writel(0x00000000, regs + 0x140 + (i * 0x100));
		writel(0x00000000, regs + 0x144 + (i * 0x100));
		writel(0x00000000, regs + 0x148 + (i * 0x100));
		writel(0x00000000, regs + 0x14C + (i * 0x100));
		writel(0x00000000, regs + 0x150 + (i * 0x100));
		writel(0x00000000, regs + 0x15C + (i * 0x100));
		writel(0x00000000, regs + 0x160 + (i * 0x100));
		writel(0x00000000, regs + 0x164 + (i * 0x100));
		writel(0x00000000, regs + 0x168 + (i * 0x100));
		writel(0x00000000, regs + 0x16C + (i * 0x100));
		writel(0x00000000, regs + 0x170 + (i * 0x100));
		writel(0x00000000, regs + 0x174 + (i * 0x100));
		writel(0x00000000, regs + 0x178 + (i * 0x100));
		writel(0x00000000, regs + 0x17C + (i * 0x100));
		writel(0x00000000, regs + 0x180 + (i * 0x100));
		writel(0x00000000, regs + 0x184 + (i * 0x100));
		writel(0x00000000, regs + 0x188 + (i * 0x100));
		writel(0x00000000, regs + 0x18C + (i * 0x100));
		writel(0x00000000, regs + 0x190 + (i * 0x100));
		writel(0x00000000, regs + 0x194 + (i * 0x100));
		writel(0x00000000, regs + 0x198 + (i * 0x100));
		writel(0x00000000, regs + 0x19C + (i * 0x100));
		writel(0x00000000, regs + 0x1A0 + (i * 0x100));
	}

	/* SD */
	writel(0x00000000, regs + 0x400);
	writel(0x00000000, regs + 0x404);
	writel(0x00000000, regs + 0x408);
	writel(0x00000000, regs + 0x40C);
	writel(0x00000000, regs + 0x410);
	writel(0x00000000, regs + 0x414);
	writel(0x00000000, regs + 0x418);
	writel(0x00000000, regs + 0x430);
	writel(0x00000000, regs + 0x434);
	writel(0x00000000, regs + 0x438);
	writel(0x00000000, regs + 0x440);
	writel(0x00000000, regs + 0x444);
	writel(0x00000000, regs + 0x448);
	writel(0x00000000, regs + 0x44C);
	writel(0x00000000, regs + 0x450);
	writel(0x00000000, regs + 0x470);
	writel(0x00000000, regs + 0x474);
	writel(0x00000000, regs + 0x47C);
	writel(0x00000000, regs + 0x484);
	writel(0x00000000, regs + 0x488);
	writel(0x00000000, regs + 0x490);
	writel(0x00000000, regs + 0x494);
	writel(0x00000000, regs + 0x498);
	writel(0x00000000, regs + 0x4A0);
}

static void __set_def_cfg_0800_0000_dphy(void __iomem *regs)
{
	int i;

	/* SC */
	writel(0x00000000, regs + 0x000);
	writel(0x00000000, regs + 0x004);
	writel(0x00000000, regs + 0x008);
	writel(0x00000000, regs + 0x00C);
	writel(0x00000000, regs + 0x010);
	writel(0x00000000, regs + 0x014);
	writel(0x00000000, regs + 0x018);
	writel(0x00000000, regs + 0x01C);
	writel(0x00000000, regs + 0x030);
	writel(0x00000000, regs + 0x040);
	writel(0x00000000, regs + 0x070);
	writel(0x00000000, regs + 0x074);

	/* SDs */
	for (i = 0; i < 4; i++) {
		writel(0x00000000, regs + 0x100 + (i * 0x100));
		writel(0x00000000, regs + 0x104 + (i * 0x100));
		writel(0x00000000, regs + 0x108 + (i * 0x100));
		writel(0x00000000, regs + 0x10C + (i * 0x100));
		writel(0x00000000, regs + 0x110 + (i * 0x100));
		writel(0x00000000, regs + 0x114 + (i * 0x100));
		writel(0x00000000, regs + 0x118 + (i * 0x100));
		writel(0x00000000, regs + 0x130 + (i * 0x100));
		writel(0x00000000, regs + 0x134 + (i * 0x100));
		writel(0x00000000, regs + 0x138 + (i * 0x100));
		writel(0x00000000, regs + 0x140 + (i * 0x100));
		writel(0x00000000, regs + 0x144 + (i * 0x100));
		writel(0x00000000, regs + 0x148 + (i * 0x100));
		writel(0x00000000, regs + 0x14C + (i * 0x100));
		writel(0x00000000, regs + 0x150 + (i * 0x100));
		writel(0x00000000, regs + 0x170 + (i * 0x100));
		writel(0x00000000, regs + 0x174 + (i * 0x100));
		writel(0x00000000, regs + 0x17C + (i * 0x100));
		writel(0x00000000, regs + 0x184 + (i * 0x100));
		writel(0x00000000, regs + 0x188 + (i * 0x100));
		writel(0x00000000, regs + 0x190 + (i * 0x100));
		writel(0x00000000, regs + 0x194 + (i * 0x100));
		writel(0x00000000, regs + 0x198 + (i * 0x100));
		writel(0x00000000, regs + 0x1A0 + (i * 0x100));
	}
}

static int __set_phy_state(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, unsigned int on)
{
	static int active_count;
	int ret = 0;

	if (phy_desc->data->flags & MIPI_PHY_MxSx_SHARED)
		ret = __set_phy_share(state, phy_desc, on);
	else
		ret = __set_phy_alone(state, phy_desc, on);

	if (IS_ENABLED(CONFIG_PABLO_V10_1_0)) {
		on ? active_count++ : active_count--;

		if (on) {
			if (active_count == 1)
				__set_def_cfg_0800_0000_bias_pll();

			if (phy_desc->rst_bit == 4)
				__set_def_cfg_0800_0000_dphy(phy_desc->regs);
			else
				__set_def_cfg_0800_0000_combo(phy_desc->regs);
		}
	}

	return ret;
}

static void update_bits(void __iomem *addr, unsigned int start,
			unsigned int width, unsigned int val)
{
	unsigned int cfg;
	unsigned int mask = (width >= 32) ? 0xffffffff : ((1U << width) - 1);

	cfg = readl(addr);
	cfg &= ~(mask << start);
	cfg |= ((val & mask) << start);
	writel(cfg, addr);
}

#define PHY_REF_SPEED	(1500)
#define CPHY_REF_SPEED	(500)
static int __set_phy_cfg_0800_0000_cphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 settle_clk_sel = 1;
	void __iomem *base;
	void __iomem *bias;

	/* phy disable for analog logic reset */
	for (i = 0; i <= cfg[LANES]; i++)
		writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */

	usleep_range(200, 201);

	if (IS_ENABLED(CONFIG_PABLO_V10_0_0)) {
		base = ioremap(0x173E1000, 0x3000);
		update_bits(base + 0x041C, 8, 3, 1);
		update_bits(base + 0x051C, 8, 3, 1);
		update_bits(base + 0x061C, 8, 3, 1);
		update_bits(base + 0x0C1C, 8, 3, 1);
		update_bits(base + 0x0D1C, 8, 3, 1);
		update_bits(base + 0x0E1C, 8, 3, 1);
		update_bits(base + 0x141C, 8, 3, 1);
		update_bits(base + 0x151C, 8, 3, 1);
		update_bits(base + 0x161C, 8, 3, 1);
		update_bits(base + 0x1C1C, 8, 3, 1);
		update_bits(base + 0x1D1C, 8, 3, 1);
		update_bits(base + 0x1E1C, 8, 3, 1);
		update_bits(base + 0x241C, 8, 3, 1);
		update_bits(base + 0x251C, 8, 3, 1);
		update_bits(base + 0x261C, 8, 3, 1);
		iounmap(base);

		bias = ioremap(0x173E1000, 0x1000);
	} else {
		bias = ioremap(0x170F1000, 0x1000);
	}
	/* BIAS/PLL */
	writel(0x00000010, bias + 0x0000); /* M_BIAS_CON0 */
	writel(0x00000110, bias + 0x0004); /* M_BIAS_CON1 */
	writel(0x00003223, bias + 0x0008); /* M_BIAS_CON2 */
	writel(0x00000240, bias + 0x0010); /* M_BIAS_CON4 */
	writel(0x00000500, bias + 0x0114); /* M_PLL_CON5 */
	writel(0x00000010, bias + 0x0118); /* M_PLL_CON6 */
	writel(0x00003D40, bias + 0x011c); /* M_PLL_CON7 */
	writel(0x00001E00, bias + 0x0120); /* M_PLL_CON8 */
	writel(0x00001300, bias + 0x0124); /* M_PLL_CON9 */
	iounmap(bias);

	if (cfg[SPEED] >= CPHY_REF_SPEED)
		settle_clk_sel = 0;

	for (i = 0; i <= cfg[LANES]; i++) {
		writel(0x00003D40, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
		writel(0x00000031, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
		writel(0x0000B349, regs + 0x010C + (i * 0x100)); /* SD_ANA_CON1 */
		writel(0x00000005, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
		writel(0x00008600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
		writel(0x00004040, regs + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */
		writel(0x00002200, regs + 0x011C + (i * 0x100)); /* SD_ANA_CON5 */
		writel(0x00000E08, regs + 0x0120 + (i * 0x100)); /* SD_ANA_CON6 */
		writel(0x00000040, regs + 0x0124 + (i * 0x100)); /* SD_ANA_CON7 */
		update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
		update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */
		writel(0x00000034, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */
		writel(0x00001000, regs + 0x015C + (i * 0x100)); /* SD_CRC_DLY_CON0 */
		writel(0x00001501, regs + 0x0164 + (i * 0x100)); /* SD_CRC_CON1 */
		writel(0x00000023, regs + 0x0168 + (i * 0x100)); /* SD_CRC_CON2 */
		writel(0x00000080, regs + 0x019C + (i * 0x100)); /* SD_CRC_DLY_CON3 */

		/* Enable should be set at last. */
		writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
 	}

 	usleep_range(200, 201);

 	return 0;
}

static int __set_phy_cfg_0800_0000_dphy(void __iomem *regs, int option, u32 *cfg)
{
	int i;
	u32 settle_clk_sel = 1;
	u32 skew_delay_sel = 0;
	void __iomem *base;
	void __iomem *bias;

	/* phy disable for analog logic reset */
	writel(0x00000000, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

	for (i = 0; i <= cfg[LANES]; i++)
		writel(0x00000000, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */

	usleep_range(200, 201);

	if (IS_ENABLED(CONFIG_PABLO_V10_0_0)) {
		base = ioremap(0x173E1000, 0x3000);
		update_bits(base + 0x041C, 8, 3, 1);
		update_bits(base + 0x051C, 8, 3, 1);
		update_bits(base + 0x061C, 8, 3, 1);
		update_bits(base + 0x0C1C, 8, 3, 1);
		update_bits(base + 0x0D1C, 8, 3, 1);
		update_bits(base + 0x0E1C, 8, 3, 1);
		update_bits(base + 0x141C, 8, 3, 1);
		update_bits(base + 0x151C, 8, 3, 1);
		update_bits(base + 0x161C, 8, 3, 1);
		update_bits(base + 0x1C1C, 8, 3, 1);
		update_bits(base + 0x1D1C, 8, 3, 1);
		update_bits(base + 0x1E1C, 8, 3, 1);
		update_bits(base + 0x241C, 8, 3, 1);
		update_bits(base + 0x251C, 8, 3, 1);
		update_bits(base + 0x261C, 8, 3, 1);
		iounmap(base);

		bias = ioremap(0x173E1000, 0x1000);
	} else {
		bias = ioremap(0x170F1000, 0x1000);
	}
	/* BIAS/PLL */
	writel(0x00000010, bias + 0x0000); /* M_BIAS_CON0 */
	writel(0x00000110, bias + 0x0004); /* M_BIAS_CON1 */
	writel(0x00003223, bias + 0x0008); /* M_BIAS_CON2 */
	writel(0x00000200, bias + 0x0010); /* M_BIAS_CON4 */
	writel(0x00000500, bias + 0x0114); /* M_PLL_CON5 */
	writel(0x00000000, bias + 0x0118); /* M_PLL_CON6 */
	writel(0x00003D40, bias + 0x011c); /* M_PLL_CON7 */
	writel(0x00001E00, bias + 0x0120); /* M_PLL_CON8 */
	writel(0x00001300, bias + 0x0124); /* M_PLL_CON9 */
	iounmap(bias);

	if (cfg[SPEED] >= PHY_REF_SPEED) {
		settle_clk_sel = 0;

		if (cfg[SPEED] >= 4000 && cfg[SPEED] <= 6500)
			skew_delay_sel = 0;
		else if (cfg[SPEED] >= 3000 && cfg[SPEED] < 4000)
			skew_delay_sel = 1;
		else if (cfg[SPEED] >= 2000 && cfg[SPEED] < 3000)
			skew_delay_sel = 2;
		else if (cfg[SPEED] >= 1500 && cfg[SPEED] < 2000)
			skew_delay_sel = 3;
		else
			skew_delay_sel = 0;
	}

	/* Clock lane */
	writel(0x00003D40, regs + 0x0004); /* SC_GNR_CON1 */
	writel(0x00000004, regs + 0x0008); /* SC_ANA_CON0 */
	writel(0x0000A0F0, regs + 0x000C); /* SC_ANA_CON1 */
	writel(0x00000002, regs + 0x0010); /* SC_ANA_CON2 */
	writel(0x00008600, regs + 0x0014); /* SC_ANA_CON3 */
	writel(0x00004000, regs + 0x0018); /* SC_ANA_CON4 */
	writel(0x00000309, regs + 0x0030); /* SC_TIME_CON0 */
	writel(0x00000001, regs + 0x0040); /* SC_DATA_CON0 */

	/* Enable should be set at last. */
	writel(0x00000001, regs + 0x0000); /* SC_GNR_CON0, Phy clock enable */

	/* Data lane */
	for (i = 0; i <= cfg[LANES]; i++) {
		writel(0x00003D40, regs + 0x0104 + (i * 0x100)); /* SD_GNR_CON1 */
		writel(0x00000004, regs + 0x0108 + (i * 0x100)); /* SD_ANA_CON0 */
		writel(0x0000A0F0, regs + 0x010C + (i * 0x100)); /* SD_ANA_CON1 */
		writel(0x00000002, regs + 0x0110 + (i * 0x100)); /* SD_ANA_CON2 */
		update_bits(regs + 0x0110 + (i * 0x100), 8, 2, skew_delay_sel); /* SD_ANA_CON2 */
		writel(0x00008600, regs + 0x0114 + (i * 0x100)); /* SD_ANA_CON3 */
		/* Combo data lane only */
		if (i < 3)
			writel(0x00004000, regs + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */
		else
			writel(0x00002000, regs + 0x0118 + (i * 0x100)); /* SD_ANA_CON4 */
		update_bits(regs + 0x0130 + (i * 0x100), 0, 8, cfg[SETTLE]);    /* SD_TIME_CON0 */
		update_bits(regs + 0x0130 + (i * 0x100), 8, 1, settle_clk_sel); /* SD_TIME_CON0 */
		writel(0x00000003, regs + 0x0134 + (i * 0x100)); /* SD_TIME_CON1 */
		update_bits(regs + 0x0140 + (i * 0x100), 0, 1, 1); /* SD_DESKEW_CON0 */
		writel(0x0000081A, regs + 0x0150 + (i * 0x100)); /* SD_DESKEW_CON4 */

		/* Enable should be set at last. */
		writel(0x00000001, regs + 0x0100 + (i * 0x100)); /* SD_GNR_CON0 , Phy data enable */
	}

	usleep_range(200, 201);

	return 0;
}

static const struct exynos_mipi_phy_cfg phy_cfg_table[] = {
	{
		/* type : Combo(DCphy), mode : Cphy */
		.major = 0x0800,
		.minor = 0x0000,
		.mode = 0xC,
		.set = __set_phy_cfg_0800_0000_cphy,
	},
	{
		/* type : Combo(DCphy), mode : Dphy */
		.major = 0x0800,
		.minor = 0x0000,
		.mode = 0xD,
		.set = __set_phy_cfg_0800_0000_dphy,
	},
	{ },
};

static int __set_phy_cfg(struct exynos_mipi_phy *state,
		struct mipi_phy_desc *phy_desc, int option, void *info)
{
	u32 *cfg = (u32 *)info;
	unsigned long i;
	int ret = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(phy_cfg_table); i++) {
		if (HIWORD(cfg[VERSION]) != phy_cfg_table[i].major)
			continue;

		if (LOWORD(cfg[VERSION]) != phy_cfg_table[i].minor)
			continue;

		if (HIWORD(cfg[TYPE]) != (phy_cfg_table[i].mode & 0x0F) &&
		    HIWORD(cfg[TYPE]) != ((phy_cfg_table[i].mode & 0xF0) >> 4))
			continue;

		ret = phy_cfg_table[i].set(phy_desc->regs,
				option, cfg);

		break;
	}

	return ret;
}

static struct exynos_mipi_phy_data mipi_phy_m4s4_top = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m4s4_top.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m4s4_mod = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m4s4_mod.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m4s4s4 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m4s4s4.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m4s0 = {
	.flags = MIPI_PHY_MxSx_UNIQUE,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m4s0.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m2s4s4s2 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m2s4s4s2.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m1s2s2 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m1s2s2.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m0s4s4s4_mod = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m0s4s4s4.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m0s4s4s4s4s4s2 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m0s4s4s4s4s4s2.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m0s4s4s4s4s2 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m0s4s4s4s4s2.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m0s4s4s2 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m0s4s4s2.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m0s4s4s4s4s4s4_s22 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m0s4s4s4s4s4s4_s22.slock),
};

static struct exynos_mipi_phy_data mipi_phy_m0s4s4s4s4s2s1 = {
	.flags = MIPI_PHY_MxSx_SHARED,
	.active_count = 0,
	.slock = __SPIN_LOCK_UNLOCKED(mipi_phy_m0s4s4s4s4s2s1.slock),
};

static const struct of_device_id exynos_mipi_phy_of_table[] = {
	{
		.compatible = "samsung,mipi-phy-m4s4-top",
		.data = &mipi_phy_m4s4_top,
	},
	{
		.compatible = "samsung,mipi-phy-m4s4-mod",
		.data = &mipi_phy_m4s4_mod,
	},
	{
		.compatible = "samsung,mipi-phy-m4s4s4",
		.data = &mipi_phy_m4s4s4,
	},
	{
		.compatible = "samsung,mipi-phy-m4s0",
		.data = &mipi_phy_m4s0,
	},
	{
		.compatible = "samsung,mipi-phy-m2s4s4s2",
		.data = &mipi_phy_m2s4s4s2,
	},
	{
		.compatible = "samsung,mipi-phy-m1s2s2",
		.data = &mipi_phy_m1s2s2,
	},
	{
		.compatible = "samsung,mipi-phy-m0s4s4s4-mod",
		.data = &mipi_phy_m0s4s4s4_mod,
	},
	{
		.compatible = "samsung,mipi-phy-m0s4s4s4s4s4s2",
		.data = &mipi_phy_m0s4s4s4s4s4s2,
	},
	{
		.compatible = "samsung,mipi-phy-m0s4s4s4s4s2",
		.data = &mipi_phy_m0s4s4s4s4s2,
	},
	{
		.compatible = "samsung,mipi-phy-m0s4s4s2",
		.data = &mipi_phy_m0s4s4s2,
	},
	{
		.compatible = "samsung,mipi-phy-m0s4s4s4s4s4s4_s22",
		.data = &mipi_phy_m0s4s4s4s4s4s4_s22,
	},
	{
		.compatible = "samsung,mipi-phy-m0s4s4s4s4s2s1",
		.data = &mipi_phy_m0s4s4s4s4s2s1,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_mipi_phy_of_table);

#define to_mipi_video_phy(desc) \
	container_of((desc), struct exynos_mipi_phy, phys[(desc)->index])

static int exynos_mipi_phy_init(struct phy *phy)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_init(state, phy_desc, 1);
}

static int exynos_mipi_phy_power_on(struct phy *phy)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	__set_phy_ldo(state, 1);

	return __set_phy_state(state, phy_desc, 1);
}

static int exynos_mipi_phy_power_off(struct phy *phy)
{
	int ret;
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	ret =  __set_phy_state(state, phy_desc, 0);
	__set_phy_ldo(state, 0);

	return ret;
}

/* reset assertion */
static int exynos_mipi_phy_reset(struct phy *phy)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_reset(state->reg_reset, phy_desc->rst_bit, 0);
}

static int exynos_mipi_phy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	struct mipi_phy_desc *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);
	u32 cfg[5];

	if (opts) {
		cfg[VERSION] = opts->mipi_dphy.clk_miss;
		cfg[TYPE] = opts->mipi_dphy.clk_post;
		cfg[LANES] = opts->mipi_dphy.lanes;
		cfg[SPEED] = (u32)opts->mipi_dphy.hs_clk_rate;
		cfg[SETTLE] = opts->mipi_dphy.hs_settle;

		__set_phy_cfg(state, phy_desc, 0, cfg);
	}

	return __set_phy_reset(state->reg_reset, phy_desc->rst_bit, 1);
}

static struct phy *exynos_mipi_phy_of_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct exynos_mipi_phy *state = dev_get_drvdata(dev);

	if (WARN_ON(args->args[0] >= EXYNOS_MIPI_PHYS_NUM))
		return ERR_PTR(-ENODEV);

	return state->phys[args->args[0]].phy;
}

static struct phy_ops exynos_mipi_phy_ops = {
	.init		= exynos_mipi_phy_init,
	.power_on	= exynos_mipi_phy_power_on,
	.power_off	= exynos_mipi_phy_power_off,
	.reset		= exynos_mipi_phy_reset, /* reset assertion */
	.configure	= exynos_mipi_phy_configure,
	.owner		= THIS_MODULE,
};

static int exynos_mipi_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct exynos_mipi_phy *state;
	struct phy_provider *phy_provider;
	struct exynos_mipi_phy_data *phy_data;
	const struct of_device_id *of_id;
	unsigned int iso[EXYNOS_MIPI_PHYS_NUM] = {0,};
	unsigned int pg[EXYNOS_MIPI_PHYS_NUM] = {0,};
	unsigned int rst[EXYNOS_MIPI_PHYS_NUM] = {0,};
	struct resource *res;
	unsigned int i;
	int ret = 0, elements = 0;
	char *str_regulator[EXYNOS_MIPI_PHY_MAX_REGULATORS];

	state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->dev = &pdev->dev;

	of_id = of_match_device(of_match_ptr(exynos_mipi_phy_of_table), dev);
	if (!of_id)
		return -EINVAL;

	phy_data = (struct exynos_mipi_phy_data *)of_id->data;

	dev_set_drvdata(dev, state);
	spin_lock_init(&state->slock);

	state->reg_pmu = syscon_regmap_lookup_by_phandle(node,
						   "samsung,pmu-syscon");
	if (IS_ERR(state->reg_pmu)) {
		dev_warn(dev, "failed to lookup PMU regmap, Skip PMU controls\n");
		state->reg_pmu = NULL;
	} else {
		/* assuming that offset 0 is invalid */
		/* PMU isolation (optional) */
		elements = of_property_count_u32_elems(node, "isolation");
		if ((elements > 0) && (elements <= EXYNOS_MIPI_PHYS_NUM)) {
			ret = of_property_read_u32_array(node, "isolation",
					iso, elements);
			if (ret) {
				dev_err(dev, "cannot get PHY isolation offset\n");
				return ret;
			}
		}

		/* PMU power-gating (optional) */
		elements = of_property_count_u32_elems(node, "power-gating");
		if ((elements > 0) && (elements <= EXYNOS_MIPI_PHYS_NUM)) {
			ret = of_property_read_u32_array(node, "power-gating",
					pg, elements);
			if (ret) {
				dev_err(dev, "cannot get PHY power-gating offset\n");
				return ret;
			}
		}
	}

	/* SYSREG reset (optional) */
	state->reg_reset = syscon_regmap_lookup_by_phandle(node,
						"samsung,reset-sysreg");
	if (IS_ERR(state->reg_reset)) {
		state->reg_reset = NULL;
	} else {
		elements = of_property_count_u32_elems(node, "reset");
		if ((elements < 0) || (elements > EXYNOS_MIPI_PHYS_NUM))
			return -EINVAL;

		ret = of_property_read_u32_array(node, "reset", rst, elements);
		if (ret) {
			dev_err(dev, "cannot get PHY reset bit\n");
			return ret;
		}
	}

	of_property_read_u32(node, "owner", &state->owner);

	for (i = 0; i < elements; i++) {
		state->phys[i].iso_offset = iso[i];
		state->phys[i].pg_offset  = pg[i];
		state->phys[i].rst_bit	  = rst[i];

		if (state->reg_pmu) {
			dev_info(dev, "isolation: 0x%x, power-gating: 0x%x\n",
					state->phys[i].iso_offset,
					state->phys[i].pg_offset);

			if (pg[i] > 0)
				__set_phy_power_gating(state->reg_pmu, pg[i], 0);
		}

		if (state->reg_reset)
			dev_info(dev, "reset: %d\n", state->phys[i].rst_bit);

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res) {
			/* clock */
			state->phys[i].regs = devm_ioremap_resource(dev, res);
			if (IS_ERR(state->phys[i].regs))
				return PTR_ERR(state->phys[i].regs);
		}

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "lane");
		if (res) {
			/* lane */
			state->phys[i].regs_lane =
				devm_ioremap(dev, res->start, resource_size(res));
			if (!state->phys[i].regs_lane) {
				dev_err(dev, "can't devm_ioremap for lane %pR\n", res);
				return -ENOMEM;
			}
		} else {
			/* default lane */
			state->phys[i].regs_lane = state->phys[i].regs + 0x100;
		}
	}

	for (i = 0; i < elements; i++) {
		struct phy *generic_phy = devm_phy_create(dev, NULL,
				&exynos_mipi_phy_ops);
		if (IS_ERR(generic_phy)) {
			dev_err(dev, "failed to create PHY\n");
			return PTR_ERR(generic_phy);
		}

		state->phys[i].index	= i;
		state->phys[i].phy	= generic_phy;
		state->phys[i].data	= phy_data;
		phy_set_drvdata(generic_phy, &state->phys[i]);
	}

	/* parse phy regulator */
	for (i = 0; i < EXYNOS_MIPI_PHY_MAX_REGULATORS; i++) {
		state->regulators[i] = NULL;
		str_regulator[i] = NULL;
	}

	if (!of_property_read_string(node, "ldo0",
				(const char **)&str_regulator[0])) {
		state->regulators[0] = regulator_get(dev, str_regulator[0]);
		if (IS_ERR(state->regulators[0])) {
			dev_err(dev, "phy regulator 0.85V get failed\n");
			state->regulators[0] = NULL;
		}
	}

	if (!of_property_read_string(dev->of_node, "ldo1",
				(const char **)&str_regulator[1])) {
		state->regulators[1] = regulator_get(dev, str_regulator[1]);
		if (IS_ERR(state->regulators[1])) {
			dev_err(dev, "phy regulator 1.80V get failed\n");
			state->regulators[1] = NULL;
		}
	}

	phy_provider = devm_of_phy_provider_register(dev,
			exynos_mipi_phy_of_xlate);

	if (IS_ERR(phy_provider))
		dev_err(dev, "failed to create exynos mipi-phy\n");
	else
		dev_err(dev, "creating exynos-mipi-phy\n");

	return PTR_ERR_OR_ZERO(phy_provider);
}

static struct platform_driver exynos_mipi_phy_driver = {
	.probe	= exynos_mipi_phy_probe,
	.driver = {
		.name  = "exynos-mipi-phy-csi",
		.of_match_table = of_match_ptr(exynos_mipi_phy_of_table),
		.suppress_bind_attrs = true,
	}
};
module_platform_driver(exynos_mipi_phy_driver);

MODULE_DESCRIPTION("Samsung EXYNOS SoC MIPI CSI/DSI D/C-PHY driver");
MODULE_LICENSE("GPL v2");
