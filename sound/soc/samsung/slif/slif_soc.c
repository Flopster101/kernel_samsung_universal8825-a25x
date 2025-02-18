/* sound/soc/samsung/vts/slif.c
 *
 * ALSA SoC - Samsung VTS Serial Local Interface driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #define DEBUG */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/sched/clock.h>
#include <linux/miscdevice.h>

#include <asm-generic/delay.h>

#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include <sound/samsung/mailbox.h>
#include <sound/samsung/vts.h>
#include <sound/samsung/abox.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/exynos-el3_mon.h>

#include "slif.h"
#include "slif_soc.h"
#include "slif_nm.h"
#include "slif_clk_table.h"
#include "slif_memlog.h"

#define vts_set_mask_value(id, mask, value) \
		{id = (typeof(id))((id & ~mask) | (value & mask)); }

#define vts_set_value_by_name(id, name, value) \
		vts_set_mask_value(id, name##_MASK, value << name##_L)

#define SLIF_USE_AUD0
/* #define SLIF_REG_LOW_CTRL */

void slif_debug_pad_en(int en)
{
	/*Legacy Debugging Function*/
}

#ifdef SLIF_REG_LOW_CTRL
static void slif_check_reg(int cmd)
{
	volatile unsigned long rco_reg;
	volatile unsigned long gpv0_con;
	volatile unsigned long sysreg_vts;
	volatile unsigned long dmic_aud0;
	volatile unsigned long dmic_aud1;
	volatile unsigned long dmic_aud2;
	volatile unsigned long serial_lif;
	volatile unsigned long div_ratio;
	volatile unsigned long cmu_aud;

#if 0
{
	volatile unsigned long clk_con;
	clk_con =
		(volatile unsigned long)ioremap_wt(0x18C00000, 0x2000);

	pr_info("clk_con(0x1038) 0x%08x\n",
			readl((volatile void *)(clk_con + 0x1038)));

	if (cmd == 1) {
		unsigned int val = 0;
		/* CLK_CON_MUX_MUX_CLK_AUD_UAIF6 : 0x18C0_0000 + 0x1038 */
		val = readl((volatile void *)(clk_con + 0x1038));
		val &= ~(0x7 << 0);
		/* IOCLK_AUDIOCDCLK6 = 4 */
		val |= (4 << 0);
		writel(val, (volatile void *)(clk_con + 0x1038));
	}

	pr_info("clk_con(0x1038) 0x%08x\n",
			readl((volatile void *)(clk_con + 0x1038)));

	iounmap((volatile void __iomem *)clk_con);

}
#endif
{
	volatile unsigned long sysreg_aud;
	sysreg_aud =
		(volatile unsigned long)ioremap_wt(0x18C10000, 0x1000);

	if (cmd == 1) {
		writel(0x7, (volatile void *)(sysreg_aud + 0x520));
	}

	pr_info("sysreg_aud(0x520) 0x%08x\n",
			readl((volatile void *)(sysreg_aud + 0x520)));
	pr_info("sysreg_aud(0x504) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(sysreg_aud + 0x504)),
			readl((volatile void *)(sysreg_aud + 0x508)),
			readl((volatile void *)(sysreg_aud + 0x50C)),
			readl((volatile void *)(sysreg_aud + 0x520)));
	pr_info("sysreg_aud(0x530) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(sysreg_aud + 0x530)),
			readl((volatile void *)(sysreg_aud + 0x534)),
			readl((volatile void *)(sysreg_aud + 0x538)),
			readl((volatile void *)(sysreg_aud + 0x53C)));
	iounmap((volatile void __iomem *)sysreg_aud);
}

	printk("%s : %d\n", __func__, __LINE__);
	/* check rco */
	rco_reg =
		(volatile unsigned long)ioremap_wt(0x15860000, 0x1000);
	pr_info("rco_reg : 0x%8x\n",
			readl((volatile void *)(rco_reg + 0xb00)));
	iounmap((volatile void __iomem *)rco_reg);

	/* check gpv0 */
	gpv0_con =
		(volatile unsigned long)ioremap_wt(0x15580000, 0x1000);
	pr_info("gpv0_con(0x00) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(gpv0_con + 0x0)),
			readl((volatile void *)(gpv0_con + 0x4)),
			readl((volatile void *)(gpv0_con + 0x8)));
	pr_info("gpv0_con(0x10) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(gpv0_con + 0x10)),
			readl((volatile void *)(gpv0_con + 0x14)),
			readl((volatile void *)(gpv0_con + 0x18)));
	pr_info("gpv0_con(0x20) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(gpv0_con + 0x20)),
			readl((volatile void *)(gpv0_con + 0x24)),
			readl((volatile void *)(gpv0_con + 0x28)));
	iounmap((volatile void __iomem *)gpv0_con);

	/* check sysreg_vts */
	sysreg_vts =
		(volatile unsigned long)ioremap_wt(0x15510000, 0x2000);
	pr_info("sysreg_vts(0x940, 0x944, 0x1010) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(sysreg_vts + 0x940)),
			readl((volatile void *)(sysreg_vts + 0x944)),
			readl((volatile void *)(sysreg_vts + 0x1010)));
	pr_info("sysreg_vts(0x0108) 0x%08x\n",
			readl((volatile void *)(sysreg_vts + 0x108)));
	iounmap((volatile void __iomem *)sysreg_vts);

	/* check dmic_aud0 *//* undescribed register */
	dmic_aud0 =
		/* (volatile unsigned long)ioremap_wt(0x15350000, 0x10); */
		(volatile unsigned long)ioremap_wt(0x18F60000, 0x10);
	pr_info("vts_dmic_aud0(0x0) 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			readl((volatile void *)(dmic_aud0 + 0x0)),
			readl((volatile void *)(dmic_aud0 + 0x4)),
			readl((volatile void *)(dmic_aud0 + 0x8)),
			readl((volatile void *)(dmic_aud0 + 0xC)),
			readl((volatile void *)(dmic_aud0 + 0x10)));
	iounmap((volatile void __iomem *)dmic_aud0);
	/* check dmic_aud1 */
	dmic_aud1 =
		/* (volatile unsigned long)ioremap_wt(0x15360000, 0x10); */
		(volatile unsigned long)ioremap_wt(0x18F70000, 0x10);
	pr_info("vts_dmic_aud1(0x1) 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			readl((volatile void *)(dmic_aud1 + 0x0)),
			readl((volatile void *)(dmic_aud1 + 0x4)),
			readl((volatile void *)(dmic_aud1 + 0x8)),
			readl((volatile void *)(dmic_aud1 + 0xC)),
			readl((volatile void *)(dmic_aud1 + 0x10)));
	iounmap((volatile void __iomem *)dmic_aud1);
	/* check dmic_aud2 */
	dmic_aud2 =
		/* (volatile unsigned long)ioremap_wt(0x15370000, 0x10); */
		(volatile unsigned long)ioremap_wt(0x18F80000, 0x10);
	pr_info("vts_dmic_aud2(0x2) 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x",
			readl((volatile void *)(dmic_aud2 + 0x0)),
			readl((volatile void *)(dmic_aud2 + 0x4)),
			readl((volatile void *)(dmic_aud2 + 0x8)),
			readl((volatile void *)(dmic_aud2 + 0xC)),
			readl((volatile void *)(dmic_aud2 + 0x10)));
	iounmap((volatile void __iomem *)dmic_aud2);

	/* check serial lif */
	serial_lif =
		/* (volatile unsigned long)ioremap_wt(0x15340100, 0x1000); */
		(volatile unsigned long)ioremap_wt(0x18F50100, 0x1000);
	pr_info("slif(0x000) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x0)),
			readl((volatile void *)(serial_lif + 0x4)),
			readl((volatile void *)(serial_lif + 0x8)),
			readl((volatile void *)(serial_lif + 0xC)));
	pr_info("slif(0x010) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x10)),
			readl((volatile void *)(serial_lif + 0x14)),
			readl((volatile void *)(serial_lif + 0x18)),
			readl((volatile void *)(serial_lif + 0x1C)));
	pr_info("slif(0x020) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x20)),
			readl((volatile void *)(serial_lif + 0x24)),
			readl((volatile void *)(serial_lif + 0x28)),
			readl((volatile void *)(serial_lif + 0x2C)));
	pr_info("slif(0x030) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x30)),
			readl((volatile void *)(serial_lif + 0x34)),
			readl((volatile void *)(serial_lif + 0x38)));
	pr_info("slif(0x050) 0x%08x \n",
			readl((volatile void *)(serial_lif + 0x50)));
	pr_info("slif(0x300) 0x%08x\n",
			readl((volatile void *)(serial_lif + 0x300)));
	iounmap((volatile void __iomem *)serial_lif);

	/* check dividr */
	div_ratio =
		(volatile unsigned long)ioremap_wt(0x15500000, 0x2000);
	pr_info("vts_div_ratio(0x1804) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(div_ratio + 0x1804)),
			readl((volatile void *)(div_ratio + 0x1808)),
			readl((volatile void *)(div_ratio + 0x1818)),
			readl((volatile void *)(div_ratio + 0x181c)));
	pr_info("vts_div_ratio(0x181c) 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(div_ratio + 0x1804)),
			readl((volatile void *)(div_ratio + 0x1808)),
			readl((volatile void *)(div_ratio + 0x1818)),
			readl((volatile void *)(div_ratio + 0x181c)));
	iounmap((volatile void __iomem *)div_ratio);

	/* check cmu_aud */
	cmu_aud =
		(volatile unsigned long)ioremap_wt(0x18c00000, 0x4000);
	pr_info("cmu_aud(0x18c03008) 0x%08x 0x%08x 0x%08x\n",
			readl((volatile void *)(cmu_aud + 0x3008)),
			readl((volatile void *)(cmu_aud + 0x300c)),
			readl((volatile void *)(cmu_aud + 0x3010)));
	iounmap((volatile void __iomem *)cmu_aud);
}
#endif

static u32 slif_direct_readl(const volatile void __iomem *addr)
{
	u32 ret = readl(addr);

	return ret;
}

static void slif_direct_writel(u32 b, volatile void __iomem *addr)
{
	writel(b, addr);
}

/* private functions */
static void slif_soc_reset_status(struct slif_data *data)
{
	/* set_bit(0, &data->mode); */
	data->enabled = 0;
	data->running = 0;
	/* data->state = 0; */
	/* data->fmt = -1 */;
}

static void slif_soc_set_default_gain(struct slif_data *data)
{
	int i;

	for (i = 0; i < SLIF_DMIC_AUD_NUM; i++) {
		data->gain_mode[i] = SLIF_DEFAULT_GAIN_MODE;
		data->max_scale_gain[i] = SLIF_DEFAULT_MAX_SCALE_GAIN;
		data->control_dmic_aud[i] = SLIF_DEFAULT_CONTROL_DMIC_AUD;
		slif_soc_dmic_aud_gain_mode_write(data, i);
		slif_soc_dmic_aud_max_scale_gain_write(data, i);
		slif_soc_dmic_aud_control_gain_write(data, i);
	}
}

static void slif_soc_set_dmic_aud(struct slif_data *data, int enable)
{
	int i;
	if (enable) {
#if (SLIF_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
		for (i = 0; i < SLIF_DMIC_AUD_NUM; i++) {
			slif_direct_writel(0x80030000, data->dmic_aud[i] + 0x0);
		}
#elif (SLIF_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
		for (i = 0; i < SLIF_DMIC_AUD_NUM; i++) {
			slif_direct_writel(0xA0030000, data->dmic_aud[i] + 0x0);
		}
#elif (SLIF_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
		for (i = 0; i < SLIF_DMIC_AUD_NUM; i++) {
			slif_direct_writel(0xA0030000, data->dmic_aud[i] + 0x0);
		}
#else
#error "SLIF_SOC_VERSION is not defined"
#endif
	} else {
	}
}
static void __maybe_unused slif_soc_set_sel_pad(struct slif_data *data, int enable)
{
	struct device *dev = data->dev;
	unsigned int ctrl;

	if (enable) {
		slif_direct_writel(0x7, data->sfr_sys_base +
			SLIF_SEL_PAD_AUD_BASE);
		ctrl = slif_direct_readl(data->sfr_sys_base +
				SLIF_SEL_PAD_AUD_BASE);
		slif_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);

		slif_direct_writel(0x38, data->sfr_sys_base +
			SLIF_SEL_DIV2_CLK_BASE);
		ctrl = slif_direct_readl(data->sfr_sys_base +
				SLIF_SEL_DIV2_CLK_BASE);
		slif_info(dev, "SEL_DIV2_CLK(0x%08x)\n", ctrl);
	} else {
		slif_direct_writel(0, data->sfr_sys_base +
			SLIF_SEL_PAD_AUD_BASE);
		ctrl = slif_direct_readl(data->sfr_sys_base +
				SLIF_SEL_PAD_AUD_BASE);
		slif_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);

		slif_direct_writel(0, data->sfr_sys_base +
			SLIF_SEL_DIV2_CLK_BASE);
		ctrl = slif_direct_readl(data->sfr_sys_base +
				SLIF_SEL_DIV2_CLK_BASE);
		slif_info(dev, "SEL_DIV2_CLK(0x%08x)\n", ctrl);
	}
}

/* public functions */
int slif_soc_dmic_aud_gain_mode_write(struct slif_data *data,
		unsigned int id)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_write(regmap,
			SLIF_SFR_GAIN_MODE_BASE,
			data->gain_mode[id]);
	if (ret < 0)
		slif_err(dev, "Failed to write GAIN_MODE(%d): %d\n",
			id, data->gain_mode[id]);

	return ret;
}

int slif_soc_dmic_aud_gain_mode_get(struct slif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_read(regmap,
			SLIF_SFR_GAIN_MODE_BASE,
			&data->gain_mode[id]);

	if (ret < 0)
		slif_err(dev, "Failed to get GAIN_MODE(%d): %d\n",
			id, data->gain_mode[id]);

	*val = data->gain_mode[id];

	return ret;
}

int slif_soc_dmic_aud_gain_mode_put(struct slif_data *data,
		unsigned int id, unsigned int val)
{
	int ret = 0;

	data->gain_mode[id] = val;
	ret = slif_soc_dmic_aud_gain_mode_write(data, id);

	return ret;
}


int slif_soc_dmic_aud_max_scale_gain_write(struct slif_data *data,
		unsigned int id)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_write(regmap,
			SLIF_SFR_MAX_SCALE_GAIN_BASE,
			data->max_scale_gain[id]);
	if (ret < 0)
		slif_err(dev, "Failed to write MAX_SCALE_GAIN(%d): %d\n",
				id, data->max_scale_gain[id]);

	return ret;
}

int slif_soc_dmic_aud_max_scale_gain_get(struct slif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_read(regmap,
			SLIF_SFR_MAX_SCALE_GAIN_BASE,
			&data->max_scale_gain[id]);

	if (ret < 0)
		slif_err(dev, "Failed to get MAX_SCALE_GAIN(%d): %d\n",
				id, data->max_scale_gain[id]);

	*val = data->max_scale_gain[id];

	return ret;
}

int slif_soc_dmic_aud_max_scale_gain_put(struct slif_data *data,
		unsigned int id, unsigned int val)
{
	int ret = 0;

	data->max_scale_gain[id] = val;
	ret = slif_soc_dmic_aud_max_scale_gain_write(data, id);

	return ret;
}

int slif_soc_dmic_aud_control_gain_write(struct slif_data *data,
		unsigned int id)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	int ret = 0;

	ret = regmap_write(regmap,
			SLIF_SFR_CONTROL_DMIC_AUD_BASE,
			data->control_dmic_aud[id]);

	if (ret < 0)
		slif_err(dev, "Failed to write CONTROL_DMIC_AUD(%d): %d\n",
				id, data->control_dmic_aud[id]);

	return ret;
}

int slif_soc_dmic_aud_control_gain_get(struct slif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	unsigned int mask = 0;
	int ret = 0;

	ret = regmap_read(regmap,
			SLIF_SFR_CONTROL_DMIC_AUD_BASE,
			&data->control_dmic_aud[id]);

	if (ret < 0)
		slif_err(dev, "Failed to get CONTROL_DMIC_AUD(%d): %d\n",
				id, data->control_dmic_aud[id]);

	mask = SLIF_SFR_CONTROL_DMIC_AUD_GAIN_MASK;
	*val = ((data->control_dmic_aud[id] & mask) >>
			SLIF_SFR_CONTROL_DMIC_AUD_GAIN_L);

	slif_dbg(dev, "mask 0x%x\n", mask);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);
	slif_dbg(dev, "val 0x%x \n", *val);

	return ret;
}

int slif_soc_dmic_aud_control_gain_put(struct slif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = SLIF_SFR_CONTROL_DMIC_AUD_GAIN_MASK;
	value = (val << SLIF_SFR_CONTROL_DMIC_AUD_GAIN_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	slif_dbg(dev, "mask 0x%x val 0x%x value 0x%x\n",
			mask, val, value);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);

	ret = slif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

int slif_soc_vol_set_get(struct slif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	unsigned int volumes;
	int ret = 0;

	ctrl = snd_soc_component_read(cmpnt, SLIF_VOL_SET(id));

	volumes = (ctrl & SLIF_VOL_SET_MASK);

	slif_dbg(dev, "(0x%08x, %u)\n", id, volumes);

	*val = volumes;

	return ret;
}

int slif_soc_vol_set_put(struct slif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	ret = snd_soc_component_update_bits(cmpnt,
			SLIF_VOL_SET(id),
			SLIF_VOL_SET_MASK,
			val << SLIF_VOL_SET_L);

	slif_dbg(dev, "(0x%08x, %u)\n", id, val);

	return ret;
}

int slif_soc_vol_change_get(struct slif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	unsigned int volumes;
	int ret = 0;

	ctrl = snd_soc_component_read(cmpnt, SLIF_VOL_CHANGE(id));

	volumes = (ctrl & SLIF_VOL_CHANGE_MASK);

	slif_dbg(dev, "(0x%08x, %u)\n", id, volumes);

	*val = volumes;

	return ret;
}

int slif_soc_vol_change_put(struct slif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	ret = snd_soc_component_update_bits(cmpnt,
			SLIF_VOL_CHANGE(id),
			SLIF_VOL_CHANGE_MASK,
			val << SLIF_VOL_CHANGE_L);
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);

	slif_dbg(dev, "(0x%08x, %u)\n", id, val);

	return ret;
}

int slif_soc_channel_map_get(struct slif_data *data,
		unsigned int id, unsigned int *val)
{
#if (SLIF_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	return 0;

#elif (SLIF_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	unsigned int channel_map;
	unsigned int channel_map_mask;
	int ret = 0;

	if (id > 7) {
		slif_err(dev, "id(%d) is not valid\n", id);
		return -EINVAL;
	}

	ctrl = snd_soc_component_read(cmpnt, SLIF_CHANNEL_MAP_BASE);

	channel_map_mask = SLIF_CHANNEL_MAP_MASK(id);
	channel_map = ((ctrl & channel_map_mask) >>
			(SLIF_CHANNEL_MAP_ITV * id));

	slif_dbg(dev, "(0x%08x 0x%08x)\n", ctrl, channel_map_mask);
	slif_dbg(dev, "(%d, 0x%08x)\n", id, channel_map);

	*val = channel_map;
	data->channel_map = ctrl;

	return ret;
#elif (SLIF_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	unsigned int channel_map;
	unsigned int channel_map_mask;
	int ret = 0;

	if (id > 7) {
		slif_err(dev, "id(%d) is not valid\n", id);
		return -EINVAL;
	}

	ctrl = snd_soc_component_read(cmpnt, SLIF_CHANNEL_MAP_BASE);

	channel_map_mask = SLIF_CHANNEL_MAP_MASK(id);
	channel_map = ((ctrl & channel_map_mask) >>
			(SLIF_CHANNEL_MAP_ITV * id));

	slif_dbg(dev, "(0x%08x 0x%08x)\n", ctrl, channel_map_mask);
	slif_dbg(dev, "(%d, 0x%08x)\n", id, channel_map);

	*val = channel_map;
	data->channel_map = ctrl;

	return ret;
#endif
}

int slif_soc_channel_map_put(struct slif_data *data,
		unsigned int id, unsigned int val)
{
#if (SLIF_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	return 0;

#elif (SLIF_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	if (id > 7) {
		slif_err(dev, "id(%d) is not valid\n", id);
		return -EINVAL;
	}

	ret = snd_soc_component_update_bits(cmpnt,
			SLIF_CHANNEL_MAP_BASE,
			SLIF_CHANNEL_MAP_MASK(id),
			val << (SLIF_CHANNEL_MAP_ITV * id));
	if (ret < 0)
		slif_err(dev, "failed: %d\n", ret);

	data->channel_map = snd_soc_component_read(cmpnt,
			SLIF_CHANNEL_MAP_BASE);
	slif_info(dev, "(0x%08x, 0x%x)\n", id, data->channel_map);

	return ret;
#elif (SLIF_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	if (id > 7) {
		slif_err(dev, "id(%d) is not valid\n", id);
		return -EINVAL;
	}

	ret = snd_soc_component_update_bits(cmpnt,
			SLIF_CHANNEL_MAP_BASE,
			SLIF_CHANNEL_MAP_MASK(id),
			val << (SLIF_CHANNEL_MAP_ITV * id));
	if (ret < 0)
		slif_err(dev, "failed:%d\n", ret);

	data->channel_map = snd_soc_component_read(cmpnt,
			SLIF_CHANNEL_MAP_BASE);
	slif_info(dev, "(0x%08x, 0x%x)\n", id, data->channel_map);

	return ret;
#endif
}

int slif_soc_dmic_aud_control_hpf_sel_get(struct slif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	unsigned int mask = 0;
	int ret = 0;

	ret = regmap_read(regmap,
			SLIF_SFR_CONTROL_DMIC_AUD_BASE,
			&data->control_dmic_aud[id]);

	if (ret < 0)
		slif_err(dev, "Failed to get HPF_SEL(%d): %d\n",
				id, data->control_dmic_aud[id]);

	mask = SLIF_SFR_CONTROL_DMIC_AUD_HPF_SEL_MASK;
	*val = ((data->control_dmic_aud[id] & mask) >>
			SLIF_SFR_CONTROL_DMIC_AUD_HPF_SEL_L);

	slif_dbg(dev, "mask 0x%x\n", mask);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);
	slif_dbg(dev, "val 0x%x \n", *val);

	return ret;
}

int slif_soc_dmic_aud_control_hpf_sel_put(struct slif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = SLIF_SFR_CONTROL_DMIC_AUD_HPF_SEL_MASK;
	value = (val << SLIF_SFR_CONTROL_DMIC_AUD_HPF_SEL_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	slif_dbg(dev, "mask 0x%x val 0x%x value 0x%x\n",
			mask, val, value);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);

	ret = slif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

int slif_soc_dmic_aud_control_hpf_en_get(struct slif_data *data,
		unsigned int id, unsigned int *val)
{
	struct device *dev = data->dev;
	struct regmap *regmap = data->regmap_dmic_aud[id];
	unsigned int mask = 0;
	int ret = 0;

	ret = regmap_read(regmap,
			SLIF_SFR_CONTROL_DMIC_AUD_BASE,
			&data->control_dmic_aud[id]);

	if (ret < 0)
		slif_err(dev, "Failed to get HPF_EN(%d): %d\n",
				id, data->control_dmic_aud[id]);

	mask = SLIF_SFR_CONTROL_DMIC_AUD_HPF_EN_MASK;
	*val = ((data->control_dmic_aud[id] & mask) >>
			SLIF_SFR_CONTROL_DMIC_AUD_HPF_EN_L);
	slif_dbg(dev, "mask 0x%x\n", mask);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);
	slif_dbg(dev, "val 0x%x \n", *val);

	return ret;
}

int slif_soc_dmic_aud_control_hpf_en_put(struct slif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = SLIF_SFR_CONTROL_DMIC_AUD_HPF_EN_MASK;
	value = (val << SLIF_SFR_CONTROL_DMIC_AUD_HPF_EN_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	slif_dbg(dev, "mask 0x%x val 0x%x value 0x%x\n",
			mask, val, value);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);

	ret = slif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

int slif_soc_dmic_en_get(struct slif_data *data,
		unsigned int id, unsigned int *val)
{
	int ret = 0;

	*val = data->dmic_en[id];

	return ret;
}

int slif_soc_dmic_en_put(struct slif_data *data,
		unsigned int port, unsigned int val, bool update)
{
	struct device *dev = data->dev;

	if (update)
		data->dmic_en[port] = val;

	slif_info(dev, "port%d id%d PDM%d val%d\n",
			port, VTS_PORT_ID_SLIF, DPDM, val);

	if (!test_bit(SLIF_STATE_OPENED, &data->state)) {
		slif_info(dev, "not powered\n");
		return 0;
	} else {
		if (!data->dev_vts)
			return -ENODEV;

		return vts_port_cfg(data->dev_vts, port,
				VTS_PORT_ID_SLIF, DPDM, !!val);
	}
}

int slif_soc_dmic_aud_control_sys_sel_put(struct slif_data *data,
		unsigned int id, unsigned int val)
{
	struct device *dev = data->dev;
	unsigned int mask = 0;
	unsigned int value = 0;
	int ret = 0;

	mask = SLIF_SFR_CONTROL_DMIC_AUD_SYS_SEL_MASK;
	value = (val << SLIF_SFR_CONTROL_DMIC_AUD_SYS_SEL_L) & mask;
	data->control_dmic_aud[id] &= ~mask;
	data->control_dmic_aud[id] |= value;

	slif_dbg(dev, "mask 0x%x val 0x%x value 0x%x\n",
			mask, val, value);
	slif_dbg(dev, "dmic aud[%d] 0x%x \n",
			id, data->control_dmic_aud[id]);

	ret = slif_soc_dmic_aud_control_gain_write(data, id);

	return ret;
}

static void slif_mark_dirty_register(struct slif_data *data)
{
	int i;

	for (i = 0; i < SLIF_DMIC_AUD_NUM; i++) {
		regcache_mark_dirty(data->regmap_dmic_aud[i]);
	}
	regcache_mark_dirty(data->regmap_sfr);
}

static void slif_save_register(struct slif_data *data)
{
	int i;

	for (i = 0; i < SLIF_DMIC_AUD_NUM; i++) {
		regcache_cache_only(data->regmap_dmic_aud[i], true);
		regcache_mark_dirty(data->regmap_dmic_aud[i]);
	}
	regcache_cache_only(data->regmap_sfr, true);
	regcache_mark_dirty(data->regmap_sfr);
}

static void slif_restore_register(struct slif_data *data)
{
	int i;

	for (i = 0; i < SLIF_DMIC_AUD_NUM; i++) {
		regcache_cache_only(data->regmap_dmic_aud[i], false);
		regcache_sync(data->regmap_dmic_aud[i]);
	}
	regcache_cache_only(data->regmap_sfr, false);
	regcache_sync(data->regmap_sfr);
}

static int slif_soc_set_fmt_master(struct slif_data *data,
		unsigned int fmt)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	int ret = 0;

	slif_info(dev, "(0x%08x)\n", fmt);

	if (fmt < 0)
		return -EINVAL;

	ctrl = snd_soc_component_read(cmpnt, SLIF_CONFIG_MASTER_BASE);

#if (SLIF_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}
#elif (SLIF_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}
#elif (SLIF_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 0);
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_BCLK_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 1);
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_BCLK_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 0);
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_BCLK_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_WS_POLAR, 1);
		vts_set_value_by_name(ctrl, SLIF_CONFIG_MASTER_BCLK_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}
#else
#error "SLIF_SOC_VERSION is not defined"
#endif

	slif_info(dev, "ctrl(0x%08x)\n", ctrl);
	snd_soc_component_write(cmpnt, SLIF_CONFIG_MASTER_BASE, ctrl);

	return ret;
}

static int slif_soc_set_fmt_slave(struct slif_data *data,
		unsigned int fmt)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	int ret = 0;

	slif_info(dev, "(0x%08x)\n", fmt);

	if (fmt < 0)
		return -EINVAL;

	ctrl = snd_soc_component_read(cmpnt, SLIF_CONFIG_SLAVE_BASE);

#if (SLIF_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}
#elif (SLIF_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}
#elif (SLIF_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 0);
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_BCLK_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 1);
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_BCLK_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 0);
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_BCLK_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_WS_POLAR, 1);
		vts_set_value_by_name(ctrl, SLIF_CONFIG_SLAVE_BCLK_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}
#else
#error "SLIF_SOC_VERSION is not defined"
#endif

	slif_info(dev, "ctrl(0x%08x)\n", ctrl);
	snd_soc_component_write(cmpnt, SLIF_CONFIG_SLAVE_BASE, ctrl);

	return ret;
}

int slif_soc_set_fmt(struct slif_data *data, unsigned int fmt)
{
	struct device *dev = data->dev;
	int ret = 0;

	slif_info(dev, "(0x%08x)\n", fmt);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
		set_bit(SLIF_MODE_MASTER, &data->mode);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBS_CFS:
		set_bit(SLIF_MODE_SLAVE, &data->mode);
		break;
	default:
		ret = -EINVAL;
	}

	data->fmt = fmt;

	return ret;
}
static int slif_soc_mux_clk_enable(struct slif_data *data)
{
	struct device *dev = data->dev;
	int ret = 0;

	if (data->clk_mux_dmic_aud_user) {
		ret = clk_enable(data->clk_mux_dmic_aud_user);
		if (ret < 0) {
			slif_err(dev, "Failed to enable clk_mux_dmic_aud_user: %d\n", ret);
			return ret;
		}
		slif_info(dev, "clk_mux_dmic_aud_user:%d\n",
				clk_get_rate(data->clk_mux_dmic_aud_user));
	}

	if (data->clk_mux_serial_lif) {
		ret = clk_enable(data->clk_mux_serial_lif);
		if (ret < 0) {
			slif_err(dev, "Failed to enable clk_mux_serial_lif: %d\n", ret);
			return ret;
		}
		slif_info(dev, "clk_mux_serial_lif:%d\n",
				clk_get_rate(data->clk_mux_serial_lif));
	}

	if (data->clk_mux_dmic_aud0) {
		ret = clk_enable(data->clk_mux_dmic_aud0);
		if (ret < 0) {
			slif_err(dev, "Failed to enable clk_mux_dmic_aud0: %d\n", ret);
			return ret;
		}
		slif_info(dev, "clk_mux_dmic_aud0:%d\n",
				clk_get_rate(data->clk_mux_dmic_aud0));
	}

	if (data->clk_mux_dmic_aud1) {
		ret = clk_enable(data->clk_mux_dmic_aud1);
		if (ret < 0) {
			slif_err(dev, "Failed to enable clk_mux_dmic_aud1: %d\n", ret);
			return ret;
		}
		slif_info(dev, "clk_mux_dmic_aud1:%d\n",
				clk_get_rate(data->clk_mux_dmic_aud1));
	}

	return ret;
}

static int slif_soc_div_clk_enable(struct slif_data *data)
{
	struct device *dev = data->dev;
	int ret = 0;

	if (data->clk_enable)
		return 0;

	data->clk_enable = true;

	if (data->clk_dmic_aud_div2) {
		ret = clk_enable(data->clk_dmic_aud_div2);
		if (ret < 0) {
			slif_err(dev, "Failed to enable clk_dmic_aud_div2: %d\n", ret);
			goto rewind2;
		}
		slif_info(dev, "clk_dmic_aud_div2 aft:%d\n",
				clk_get_rate(data->clk_dmic_aud_div2));
	}

	if (data->clk_serial_lif) {
		ret = clk_enable(data->clk_serial_lif);
		if (ret < 0) {
			slif_err(dev, "Failed to enable clk_s_lif: %d\n", ret);
			goto rewind1;
		}
		slif_info(dev, "clk_s_lif aft:%d\n",
				clk_get_rate(data->clk_serial_lif));
	}

	return ret;
rewind1:
	clk_disable(data->clk_dmic_aud_div2);
rewind2:
	data->clk_enable = false;
	return ret;
}

static void slif_soc_mux_clk_disable(struct slif_data *data)
{
	if (data->clk_mux_dmic_aud_user)
		clk_disable(data->clk_mux_dmic_aud_user);

	clk_disable(data->clk_mux_serial_lif);

	if (data->clk_mux_dmic_aud0)
		clk_disable(data->clk_mux_dmic_aud0);

	if (data->clk_mux_dmic_aud1)
		clk_disable(data->clk_mux_dmic_aud1);
}

static void slif_soc_div_clk_disable(struct slif_data *data)
{
	if (!data->clk_enable)
		return;

	data->clk_enable = false;

	clk_disable(data->clk_serial_lif);

	clk_disable(data->clk_dmic_aud_div2);
}

int slif_soc_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params,
		struct slif_data *data)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl0, ctrl1;
	int clk_val = 0;
	int val = 0;
	int ret = 0;
	unsigned int i = 0;

	slif_info(dev, "[%c]\n",
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	data->channels = params_channels(hw_params);
	data->rate = params_rate(hw_params);
	data->width = params_width(hw_params);
	data->clk_table_id = slif_clk_table_id_search(data->rate,
			data->width);

	slif_info(dev, "rate=%u, width=%d, channel=%u id=%d\n",
			data->rate, data->width,
			data->channels, data->clk_table_id);

	if (data->clk_table_id < 0) {
		slif_err(dev, "clk table is not matched(%d)\n", data->clk_table_id);
		return -EINVAL;
	}

	if (data->channels > 8) {
		slif_err(dev, "(%d) is not support channels\n", data->channels);
		return -EINVAL;
	}

	ctrl0 = snd_soc_component_read(cmpnt, SLIF_CONFIG_MASTER_BASE);
	ctrl1 = snd_soc_component_read(cmpnt, SLIF_CONFIG_SLAVE_BASE);

#if (SLIF_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16:
		vts_set_value_by_name(ctrl0, SLIF_CONFIG_MASTER_OPMODE, 1);
		vts_set_value_by_name(ctrl1, SLIF_CONFIG_SLAVE_OPMODE, 1);

		break;
	case SNDRV_PCM_FORMAT_S24:
		vts_set_value_by_name(ctrl0, SLIF_CONFIG_MASTER_OPMODE, 3);
		vts_set_value_by_name(ctrl1, SLIF_CONFIG_SLAVE_OPMODE, 3);

		break;
	default:
		return -EINVAL;
	}
#elif (SLIF_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16:
		vts_set_value_by_name(ctrl0, SLIF_CONFIG_MASTER_OP_D, 1);
		vts_set_value_by_name(ctrl1, SLIF_CONFIG_SLAVE_OP_D, 1);

		break;
	case SNDRV_PCM_FORMAT_S24:
		vts_set_value_by_name(ctrl0, SLIF_CONFIG_MASTER_OP_D, 0);
		vts_set_value_by_name(ctrl1, SLIF_CONFIG_SLAVE_OP_D, 0);

		break;
	default:
		return -EINVAL;
	}

	vts_set_value_by_name(ctrl0, SLIF_CONFIG_MASTER_OP_C,
			(data->channels - 1));
	vts_set_value_by_name(ctrl1, SLIF_CONFIG_SLAVE_OP_C,
			(data->channels - 1));
#elif (SLIF_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16:
		vts_set_value_by_name(ctrl0, SLIF_CONFIG_MASTER_OP_D, 1);
		vts_set_value_by_name(ctrl1, SLIF_CONFIG_SLAVE_OP_D, 1);

		break;
	case SNDRV_PCM_FORMAT_S24:
		vts_set_value_by_name(ctrl0, SLIF_CONFIG_MASTER_OP_D, 0);
		vts_set_value_by_name(ctrl1, SLIF_CONFIG_SLAVE_OP_D, 0);

		break;
	default:
		return -EINVAL;
	}

	vts_set_value_by_name(ctrl0, SLIF_CONFIG_MASTER_OP_C,
			(data->channels - 1));
	vts_set_value_by_name(ctrl1, SLIF_CONFIG_SLAVE_OP_C,
			(data->channels - 1));
#else
#error "SLIF_SOC_VERSION is not defined"
#endif

	/* SYS_SEL */
	for (i = 0; i < SLIF_DMIC_AUD_NUM; i++) {
		val = slif_clk_table_get(data->clk_table_id,
				CLK_TABLE_SYS_SEL);
		if (val < 0) {
			slif_err(dev, "clk id is not valid(%d)\n",
					data->clk_table_id);
			return -EINVAL;
		}

		ret = slif_soc_dmic_aud_control_sys_sel_put(data, i, val);
		if (ret < 0)
			slif_err(dev, "failed SYS_SEL[%d] ctrl %d\n",
				i, ret);
	}

	clk_val = slif_clk_table_get(data->clk_table_id,
			CLK_TABLE_DMIC_AUD);
	if (clk_val < 0)
		slif_err(dev, "Failed to find clk table : %s\n", "clk_dmic_aud");
	slif_info(dev, "clk_dmic_aud bef:%d\n",
			clk_get_rate(data->clk_dmic_aud));
	slif_dbg(dev, "find clk table : %s: (%d)\n", "clk_dmic_aud", clk_val);
	ret = clk_set_rate(data->clk_dmic_aud, clk_val);
	if (ret < 0) {
		slif_err(dev, "Failed to set rate : %s\n", "dmic_aud");
		return ret;
	}
	if (data->clk_mux_dmic_aud0) {
		ret = clk_set_rate(data->clk_mux_dmic_aud0,
				clk_get_rate(data->clk_dmic_aud));
		if (ret < 0) {
			slif_err(dev, "Failed to set rate : %s\n", "dmic_mux_aud0");
			return ret;
		}
	}

#if (SLIF_SOC_VERSION(1, 1, 1) >= CONFIG_SND_SOC_SAMSUNG_SLIF_VERSION)
	clk_val = slif_clk_table_get(data->clk_table_id,
			CLK_TABLE_DMIC_AUD_PAD);
	if (clk_val < 0)
		slif_err(dev, "Failed to find clk table : %s\n", "clk_dmic_pad");
	slif_info(dev, "find clk table : %s: (%d)\n", "clk_dmic_pad", clk_val);
	ret = clk_set_rate(data->clk_dmic_aud_pad, clk_val);
	if (ret < 0) {
		slif_err(dev, "Failed to set rate : %s\n", "dmic_aud_pad");
		return ret;
	}
#endif
	clk_val = slif_clk_table_get(data->clk_table_id,
			CLK_TABLE_DMIC_AUD_DIV2);
	if (clk_val < 0)
		slif_err(dev, "Failed to find clk : %s\n", "clk_dmic_div2");
	slif_dbg(dev, "find clk table : %s: (%d)\n", "clk_dmic_div2", clk_val);
	slif_info(dev, "clk_dmic_aud_div2 bef:%d\n",
			clk_get_rate(data->clk_dmic_aud_div2));
	ret = clk_set_rate(data->clk_dmic_aud_div2, clk_val);
	if (ret < 0) {
		slif_err(dev, "Failed to set rate : %s\n", "dmic_aud_div2");
		return ret;
	}

	if (data->clk_mux_dmic_aud1) {
		ret = clk_set_rate(data->clk_mux_dmic_aud1,
				clk_get_rate(data->clk_dmic_aud_div2));
		if (ret < 0) {
			slif_err(dev, "Failed to set rate : %s\n", "mux_dmic_aud1");
			return ret;
		}
	}
	clk_val = slif_clk_table_get(data->clk_table_id,
			CLK_TABLE_SERIAL_LIF);
	if (clk_val < 0)
		slif_err(dev, "Failed to find clk table : %s\n",
				"clk_serial_lif");
	slif_info(dev, "find clk table : %s: (%d)\n",
			"clk_serial_lif", clk_val);
	/* change blck to supprot channel */
	clk_val = (clk_val * data->channels) / SLIF_MAX_CHANNEL;
	slif_info(dev, "clk_s_lif bef:%d:%d\n",
			clk_val, clk_get_rate(data->clk_serial_lif));
	ret = clk_set_rate(data->clk_serial_lif, clk_val);
	if (ret < 0)
		slif_err(dev, "Failed to set rate : %s\n", "clk_s_lif");

	ret = slif_soc_div_clk_enable(data);
	if (ret < 0)
		slif_err(dev, "Failed to enable div clocks: %d\n", ret);

	ret = snd_soc_component_write(cmpnt, SLIF_CONFIG_MASTER_BASE, ctrl0);
	if (ret < 0)
		slif_err(dev, "Failed to access CONFIG_MASTER sfr:%d\n",
				ret);
	ret = snd_soc_component_write(cmpnt, SLIF_CONFIG_SLAVE_BASE, ctrl1);
	if (ret < 0)
		slif_err(dev, "Failed to access CONFIG_SLAVE sfr:%d\n",
				ret);

	set_bit(SLIF_STATE_SET_PARAM, &data->state);

	return 0;
}

int slif_soc_startup(struct snd_pcm_substream *substream,
		struct slif_data *data)
{
	struct device *dev = data->dev;
	int clk_val = 0;
	int ret = 0;
	int i;

	slif_info(dev, "[%c]\n",
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	pm_runtime_get_sync(dev);
#if IS_ENABLED(CONFIG_SOC_S5E9925)
	if (data->dev_vts) {
		pm_runtime_get_sync(data->dev_vts);
	} else {
		slif_err(dev, "data->dev_vts is NULL\n");
		return -EINVAL;
	}
#endif

	ret = slif_soc_mux_clk_enable(data);
	if (ret < 0)
		slif_err(dev, "Failed to enable mux clocks: %d\n", ret);

	vts_set_clk_src(data->dev_vts, VTS_CLK_SRC_AUD0);
	clk_val = 73728000;

	if (data->clk_mux_dmic_aud_user) {
		ret = clk_set_rate(data->clk_mux_dmic_aud_user, clk_val);
		if (ret < 0) {
			slif_err(dev, "Failed to set rate : %s\n", "clk_mux_dmic_aud_user");
			return ret;
		}
	}

	if (data->clk_slif_src) {
		ret = clk_set_rate(data->clk_slif_src, clk_val);
		if (ret < 0) {
			slif_err(dev, "Failed to set rate : %s\n", "clk_slif_src");
			return ret;
		}
	} else {
		slif_info(dev, "%s is null\n", "clk_slif_src");
	}

	if (data->clk_mux_dmic_aud) {
		ret = clk_set_rate(data->clk_mux_dmic_aud, clk_val);
		if (ret < 0) {
			slif_err(dev, "Failed to set rate : %s\n", "clk_mux_dmic_aud");
			return ret;
		}
	} else {
		slif_info(dev, "%s is null\n", "clk_mux_dmic_aud");
	}

	if (data->clk_mux_serial_lif) {
		ret = clk_set_rate(data->clk_mux_serial_lif, clk_val);
		if (ret < 0) {
			slif_err(dev, "Failed to set rate : %s\n", "clk_mux_serial_lif");
			return ret;
		}
	} else {
		slif_info(dev, "%s is null\n", "clk_mux_serial_lif");
	}

	slif_restore_register(data);
	set_bit(SLIF_STATE_OPENED, &data->state);

	for (i = 0; i < SLIF_DMIC_AUD_NUM; i++) {
		slif_soc_dmic_en_put(data, i, data->dmic_en[i], true);
	}

	return 0;

	pm_runtime_put_sync(dev);
#if IS_ENABLED(CONFIG_SOC_S5E9925)
	if (data->dev_vts)
		pm_runtime_put_sync(data->dev_vts);
#endif
	return ret;
}

void slif_soc_shutdown(struct snd_pcm_substream *substream,
		struct slif_data *data)
{
	struct device *dev = data->dev;
	int ret_chk = 0;
	int i;

	slif_dbg(dev, "[%c]\n",
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	if (!data->dev_vts) {
		slif_err(dev, "data->dev_vts is NULL\n");
		return;
	}

	/* make default pin state as idle to prevent conflict with vts */
	for (i = 0; i < SLIF_DMIC_AUD_NUM; i++) {
		slif_soc_dmic_en_put(data, i, 0, false);
	}

	slif_save_register(data);

	/* reset status */
	slif_soc_reset_status(data);

	slif_info(dev, " - set VTS clk\n");
	vts_set_clk_src(data->dev_vts, VTS_CLK_SRC_RCO);
	ret_chk = vts_chk_dmic_clk_mode(data->dev_vts);
	if (ret_chk < 0) {
		slif_info(dev, "ret_chk failed(%d)\n", ret_chk);
	}

	if (test_bit(SLIF_STATE_SET_PARAM, &data->state)) {
		slif_soc_div_clk_disable(data);
		clear_bit(SLIF_STATE_SET_PARAM, &data->state);
	}
	slif_soc_mux_clk_disable(data);

	clear_bit(SLIF_STATE_OPENED, &data->state);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_sync(dev);
#if IS_ENABLED(CONFIG_SOC_S5E9925)
	if (data->dev_vts)
		pm_runtime_put_sync(data->dev_vts);
#endif
}

int slif_soc_hw_free(struct snd_pcm_substream *substream, struct slif_data *data)
{
	struct device *dev = data->dev;

	slif_dbg(dev, "[%c]\n",
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	return 0;
}

int slif_soc_dma_en(int enable,
		struct slif_data *data)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	int ret = 0;
	int ret_chk = 0;

	slif_info(dev, "enable(%d)\n", enable);

	if (unlikely(data->slif_dump_enabled)) {
		ret = slif_soc_set_fmt_slave(data, data->fmt);
		ret |= slif_soc_set_fmt_master(data, data->fmt);
		if (ret < 0) {
			slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
			return ret;
		}
	} else {
		if (test_bit(SLIF_MODE_SLAVE, &data->mode))
			ret = slif_soc_set_fmt_slave(data, data->fmt);
		if (test_bit(SLIF_MODE_MASTER, &data->mode))
			ret = slif_soc_set_fmt_master(data, data->fmt);

		if (ret < 0) {
			slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
			return ret;
		}
	}

	if (unlikely(data->slif_dump_enabled)) {
		ret = snd_soc_component_update_bits(cmpnt,
				SLIF_CONFIG_DONE_BASE,
				SLIF_CONFIG_DONE_MASTER_CONFIG_MASK |
				SLIF_CONFIG_DONE_SLAVE_CONFIG_MASK |
				SLIF_CONFIG_DONE_ALL_CONFIG_MASK,
				(enable << SLIF_CONFIG_DONE_MASTER_CONFIG_L) |
				(enable << SLIF_CONFIG_DONE_SLAVE_CONFIG_L) |
				(enable << SLIF_CONFIG_DONE_ALL_CONFIG_L));
		if (ret < 0)
			slif_err(dev, "Failed to access CTRL sfr:%d\n",
				ret);
	} else {
		if (test_bit(SLIF_MODE_SLAVE, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					SLIF_CONFIG_DONE_BASE,
					SLIF_CONFIG_DONE_SLAVE_CONFIG_MASK |
					SLIF_CONFIG_DONE_ALL_CONFIG_MASK,
					(enable << SLIF_CONFIG_DONE_SLAVE_CONFIG_L) |
					(enable << SLIF_CONFIG_DONE_ALL_CONFIG_L));
			if (ret < 0)
				slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
		}
		if (test_bit(SLIF_MODE_MASTER, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					SLIF_CONFIG_DONE_BASE,
					SLIF_CONFIG_DONE_MASTER_CONFIG_MASK |
					SLIF_CONFIG_DONE_ALL_CONFIG_MASK,
					(enable << SLIF_CONFIG_DONE_MASTER_CONFIG_L) |
					(enable << SLIF_CONFIG_DONE_ALL_CONFIG_L));
			if (ret < 0)
				slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
		}
	}

	ctrl = snd_soc_component_read(cmpnt, SLIF_CONFIG_DONE_BASE);
	slif_info(dev, "ctrl(0x%08x)\n", ctrl);

	/* PAD configuration */
#if 0
	slif_soc_set_sel_pad(data, enable);
#else
	vts_clk_aud_set_rate(data->dev_vts, 5);
	vts_set_sel_pad(data->dev_vts, enable);
	if (IS_ENABLED(CONFIG_SOC_S5E9925))
		abox_write_sysreg(0x7, 0x520);
#endif
	/* slif_dmic_aud_en(data->dev_vts, enable); */
	/* slif_dmic_if_en(data->dev_vts, enable); */

	/* HACK : MOVE to resume */
	if (enable)
		vts_pad_retention(false);

	/* DMIC_IF configuration */
	slif_soc_set_dmic_aud(data, enable);

	data->mute_enable = enable;
	slif_info(dev, "en(%d) ms(%d)\n", enable, data->mute_ms);
	if (enable && (data->mute_ms != 0)) {
		/* queue delayed work at starting */
		schedule_delayed_work(&data->mute_work, msecs_to_jiffies(data->mute_ms));
	} else {
		/* check dmic port and enable EN bit */
		if (SLIF_DMIC_AUD_NUM == 1){
			ret = snd_soc_component_update_bits(cmpnt,
				SLIF_INPUT_EN_BASE,
				SLIF_INPUT_EN_EN0_MASK,
				(enable << SLIF_INPUT_EN_EN0_L));
		} else if(SLIF_DMIC_AUD_NUM == 2){
			ret = snd_soc_component_update_bits(cmpnt,
				SLIF_INPUT_EN_BASE,
				SLIF_INPUT_EN_EN0_MASK |
				SLIF_INPUT_EN_EN1_MASK,
				(enable << SLIF_INPUT_EN_EN0_L) |
				(enable << SLIF_INPUT_EN_EN1_L));
		} else if(SLIF_DMIC_AUD_NUM ==3) {
			ret = snd_soc_component_update_bits(cmpnt,
				SLIF_INPUT_EN_BASE,
				SLIF_INPUT_EN_EN0_MASK |
				SLIF_INPUT_EN_EN1_MASK |
				SLIF_INPUT_EN_EN2_MASK,
				(enable << SLIF_INPUT_EN_EN0_L) |
				(enable << SLIF_INPUT_EN_EN1_L) |
				(enable << SLIF_INPUT_EN_EN2_L));
		} else
			slif_info(dev, "Please check the number of DMIC ports\n");

		if (ret < 0)
			slif_err(dev, "Failed to access INPUT_EN sfr:%d\n",
				ret);
	}

	if (unlikely(data->slif_dump_enabled)) {
		ret = snd_soc_component_update_bits(cmpnt,
				SLIF_CTRL_BASE,
				SLIF_CTRL_SLAVE_IF_EN_MASK |
				SLIF_CTRL_MASTER_IF_EN_MASK |
				SLIF_CTRL_LOOPBACK_EN_MASK |
				SLIF_CTRL_SPU_EN_MASK,
				(enable << SLIF_CTRL_SLAVE_IF_EN_L) |
				(enable << SLIF_CTRL_MASTER_IF_EN_L) |
				(enable << SLIF_CTRL_LOOPBACK_EN_L) |
				(enable << SLIF_CTRL_SPU_EN_L));
		if (ret < 0)
			slif_err(dev, "Failed to access CTRL sfr:%d\n",
				ret);
	} else {
		if (test_bit(SLIF_MODE_SLAVE, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					SLIF_CTRL_BASE,
					SLIF_CTRL_SLAVE_IF_EN_MASK |
					SLIF_CTRL_SPU_EN_MASK,
					(enable << SLIF_CTRL_SLAVE_IF_EN_L) |
					(enable << SLIF_CTRL_SPU_EN_L));
			if (ret < 0)
				slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
		}
		if (test_bit(SLIF_MODE_MASTER, &data->mode)) {
			ret = snd_soc_component_update_bits(cmpnt,
					SLIF_CTRL_BASE,
					SLIF_CTRL_MASTER_IF_EN_MASK |
					SLIF_CTRL_SPU_EN_MASK,
					(enable << SLIF_CTRL_MASTER_IF_EN_L) |
					(enable << SLIF_CTRL_SPU_EN_L));
			if (ret < 0)
				slif_err(dev, "Failed to access CTRL sfr:%d\n",
					ret);
		}
	}

	ctrl = snd_soc_component_read(cmpnt, SLIF_CTRL_BASE);
	slif_info(dev, "ctrl(0x%08x)\n", ctrl);
#ifdef SLIF_REG_LOW_CTRL
	slif_check_reg(0);
#endif

	if (enable) {
		slif_info(dev, " - set VTS clk\n");
		ret_chk = vts_chk_dmic_clk_mode(data->dev_vts);
		if (ret_chk < 0) {
			slif_info(dev, "ret_chk failed(%d)\n", ret_chk);
		}
	}
	return ret;
}

static struct clk *devm_clk_get_and_prepare(struct device *dev, const char *name)
{
	struct clk *clk;
	int result;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		slif_err(dev, "Failed to get clock %s\n", name);
		goto error;
	}

	result = clk_prepare(clk);
	if (result < 0) {
		slif_err(dev, "Failed to prepare clock %s\n", name);
		goto error;
	}

error:
	return clk;
}

static void slif_soc_mute_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct slif_data *data = container_of(dwork, struct slif_data,
			mute_work);
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	slif_info(dev, ":(en:%d)\n", data->mute_enable);

	/* check dmic port and enable EN bit */
	ret = snd_soc_component_update_bits(cmpnt,
			SLIF_INPUT_EN_BASE,
			SLIF_INPUT_EN_EN0_MASK |
			SLIF_INPUT_EN_EN1_MASK |
			SLIF_INPUT_EN_EN2_MASK,
			(data->mute_enable << SLIF_INPUT_EN_EN0_L) |
			(data->mute_enable << SLIF_INPUT_EN_EN1_L) |
			(data->mute_enable << SLIF_INPUT_EN_EN2_L));
	if (ret < 0)
		slif_err(dev, "Failed to access INPUT_EN sfr:%d\n",
			ret);

}

static DECLARE_DELAYED_WORK(slif_soc_mute, slif_soc_mute_func);

int slif_soc_probe(struct slif_data *data)
{
	struct device *dev = data->dev;
	data->clk_mux_dmic_aud_user = devm_clk_get_and_prepare(dev, "mux_dmic_aud_user");
	if (IS_ERR(data->clk_mux_dmic_aud_user)) {
		data->clk_mux_dmic_aud_user = NULL;
		slif_info(dev, "Failed to get clk_mux_dmic_aud_user\n");
	}

	data->clk_mux_dmic_aud = devm_clk_get_and_prepare(dev, "mux_dmic_aud");
	if (IS_ERR(data->clk_mux_dmic_aud)) {
		data->clk_mux_dmic_aud = NULL;
		slif_info(dev, "Failed to get clk_mux_dmic_aud\n");
	}

	data->clk_mux_serial_lif = devm_clk_get_and_prepare(dev, "mux_serial_lif");
	if (IS_ERR(data->clk_mux_serial_lif)) {
		data->clk_mux_serial_lif = NULL;
		slif_info(dev, "Failed to get clk_mux_serial_lif\n");
	}

	data->clk_dmic_aud_div2 = devm_clk_get_and_prepare(dev, "dmic_aud_div2");
	if (IS_ERR(data->clk_dmic_aud_div2)) {
		data->clk_dmic_aud_div2 = NULL;
		slif_info(dev, "Failed to get clk_dmic_aud_div2\n");
	}

	data->clk_dmic_aud = devm_clk_get_and_prepare(dev, "dmic_aud");
	if (IS_ERR(data->clk_dmic_aud)) {
		data->clk_dmic_aud = NULL;
		slif_info(dev, "Failed to get clk_dmic_aud\n");
	}

	data->clk_mux_dmic_aud0 = devm_clk_get_and_prepare(dev, "mux_dmic_aud0");
	if (IS_ERR(data->clk_mux_dmic_aud0)) {
		data->clk_mux_dmic_aud0 = NULL;
		slif_info(dev, "Failed to get clk_mux_dmic_aud0\n");
	}

	data->clk_mux_dmic_aud1 = devm_clk_get_and_prepare(dev, "mux_dmic_aud1");
	if (IS_ERR(data->clk_mux_dmic_aud1)) {
		data->clk_mux_dmic_aud1 = NULL;
		slif_info(dev, "Failed to get clk_mux_dmic_aud1\n");
	}

	data->clk_serial_lif = devm_clk_get_and_prepare(dev, "serial_lif");
	if (IS_ERR(data->clk_serial_lif)) {
		data->clk_serial_lif = NULL;
		slif_info(dev, "Failed to get clk_serial_lif\n");
	}

	data->clk_slif_src = devm_clk_get_and_prepare(dev, "clk_slif_src");
	if (IS_ERR(data->clk_slif_src)) {
		data->clk_slif_src = NULL;
		slif_info(dev, "Failed to get clk_slif_src\n");
	}

	data->mute_enable = false;
	data->clk_enable = false;
	data->mute_ms = 0;
	data->clk_input_path = SLIF_CLK_PLL_AUD0;
	slif_mark_dirty_register(data);
	slif_save_register(data);

	slif_soc_set_default_gain(data);

	pm_runtime_no_callbacks(dev);
	pm_runtime_enable(dev);

	INIT_DELAYED_WORK(&data->mute_work, slif_soc_mute_func);

	return 0;

}
