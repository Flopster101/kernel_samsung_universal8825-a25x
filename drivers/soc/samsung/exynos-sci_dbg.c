/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Trace format
 * Traces are dumped in binary formats, encoded in multiple doublewords (64-bit)
 * dw 0 : magic number {'B', 'D', 'U', 'D', 'U', 'M', 'P', '\0')
 * dw 1 : { block number, port number } (each 32-bit integers
 * dw 2 : base physical address of BDU dump region
 * dw 3 : size (in bytes) of BDU dump region
 * dw 4 : tidemark if timestampes are used (0xffffffff if no timestamp)
 * dw 5~15 : zero - reserved for future use
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>
#include <linux/of_reserved_mem.h>
#include <soc/samsung/debug-snapshot.h>

#include <linux/dma-mapping.h>

//#include <asm/map.h>

#include <soc/samsung/acpm_ipc_ctrl.h>
#include <soc/samsung/exynos-sci_dbg.h>

static bool exynos_sci_llc_debug_mode = false;
static struct exynos_ppc_dump_addr ppc_reserved;
static struct exynos_sci_dbg_data *sci_dbg_data;

#define HRTIMER_DURATION	10
static u32 duration = HRTIMER_DURATION;
static u32 buffer_cnt = 0;

static u32 DebugSrc10 = 0x1C231C20;
static u32 DebugSrc32 = 0x1F231F20;
static u32 PPC_DEBUG_SCI_EVENT_SEL = 0x372F170F;

static u32 smc0_dbg_blk_ctl_value[NUM_OF_SMC_DBG_BLK_CTL] = {
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
};
static u32 smc1_dbg_blk_ctl_value[NUM_OF_SMC_DBG_BLK_CTL] = {
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
};
static u32 smc2_dbg_blk_ctl_value[NUM_OF_SMC_DBG_BLK_CTL] = {
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
};
static u32 smc3_dbg_blk_ctl_value[NUM_OF_SMC_DBG_BLK_CTL] = {
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
	0x30000000,
};

static u32 smc_global_dbg_ctl_value = 0x10000000;
static u32 sysreg_mif_ppc_debug_value[NUM_OF_SYSREG_MIF] = {
	0x33210,
	0x33210,
	0x33210,
	0x33210,
};
static u32 sysreg_mif_ppc_debug_value2[NUM_OF_SYSREG_MIF] = {
	0x07654,
	0x07654,
	0x07654,
	0x07654,
};

static u32 llc_ctrl;
static u32 llc_id[LLC_ID_MAX];
static u32 llc_addr_match;
static u32 llc_addr_mask;

bool get_exynos_sci_llc_debug_mode(void)
{
	return exynos_sci_llc_debug_mode;
}

static void sci_dump_data_save(void)
{
	u32 dump_entry_size = sizeof(struct exynos_sci_dbg_dump_data);
	struct exynos_sci_dbg_dump_data *dump_data = NULL;
	u32 remain_buff_size;
	int i;

	dump_data = (struct exynos_sci_dbg_dump_data *)(sci_dbg_data->dump_addr.v_addr + buffer_cnt);
	dump_data->index = sci_dbg_data->dump_data.index;
	dump_data->time = sci_dbg_data->dump_data.time;
	for (i = 0; i < 5; i++)
		dump_data->count[i] = sci_dbg_data->dump_data.count[i];

	buffer_cnt += dump_entry_size;
	remain_buff_size = sci_dbg_data->dump_addr.buff_size - buffer_cnt;

	if (remain_buff_size < dump_entry_size)
		buffer_cnt = 0;
}

static enum hrtimer_restart count_monitor(struct hrtimer *hrtimer)
{
	enum hrtimer_restart ret = HRTIMER_RESTART;

	sci_dbg_data->dump_data.index++;

	/* PMNC stop  */
	__raw_writel(0x0, sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);
	/* Read overflow flag */
	__raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_FLAG);

	sci_dbg_data->dump_data.time = cpu_clock(raw_smp_processor_id());

	/* read counter */
	sci_dbg_data->dump_data.count[0] =
			__raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMCNT0_LOW);
	sci_dbg_data->dump_data.count[1] =
			__raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMCNT1_LOW);
	sci_dbg_data->dump_data.count[2] =
		    __raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMCNT2_LOW);
	sci_dbg_data->dump_data.count[3] =
		    __raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMCNT3_LOW);
	sci_dbg_data->dump_data.count[4] =
			__raw_readl(sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_CCNT_LOW);


	__raw_writel(0x6, sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);
	__raw_writel(0x1, sci_dbg_data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);

	sci_dump_data_save();

	hrtimer_forward_now(hrtimer, ms_to_ktime(HRTIMER_DURATION));

	return ret;
}

static int exynos_sci_dump_enable(struct exynos_sci_dbg_data *data,
					unsigned int enable)
{
	if (enable) {
		data->dump_data.index = 0;
		buffer_cnt = 0;
		memset(data->dump_addr.v_addr, 0, data->dump_addr.buff_size);

		/* PPC_DEBUG_SCI_EVENT_SEL */
		__raw_writel(PPC_DEBUG_SCI_EVENT_SEL, data->dump_addr.trex_core_base + 0x100);
		/*  */
		__raw_writel(DebugSrc10, data->dump_addr.sci_base + DebugSrc10_offset);	// DebugSrc10
		__raw_writel(DebugSrc32, data->dump_addr.sci_base + DebugSrc32_offset);	// DebugSrc32
		__raw_writel(0x00000001, data->dump_addr.sci_base + DebugCtrl_offset);	// DebugCtrl

		/* CNTENS */
		__raw_writel(0x8000000F, data->dump_addr.cnt_sfr_base + SCI_PPC_CNTENS);
		/* INTENS */
		__raw_writel(0x8000000F, data->dump_addr.cnt_sfr_base + SCI_PPC_INTENS);
		/* FLAG */
		__raw_writel(0x8000000F, data->dump_addr.cnt_sfr_base + SCI_PPC_FLAG);
		/* PMNC reset */
		__raw_writel(0x6, data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);
		/* PMNC start */
		__raw_writel(0x1, data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);
		hrtimer_start(&data->hrtimer,
				ms_to_ktime(HRTIMER_DURATION), HRTIMER_MODE_REL);
	} else {
		hrtimer_try_to_cancel(&data->hrtimer);
		__raw_writel(0x0, data->dump_addr.cnt_sfr_base + SCI_PPC_PMNC);
	}

	return 0;
}

static void smc_dump_data_save(void)
{
	u32 dump_entry_size = sizeof(struct exynos_smc_dump_data) * 4;
	struct exynos_smc_dump_data *dump_data = NULL;
	u32 remain_buff_size;
	int i, j;

	for (i = 0; i < 4; i++) {
		dump_data = (struct exynos_smc_dump_data *)(sci_dbg_data->dump_addr.v_addr + buffer_cnt);
		dump_data->index = sci_dbg_data->smc_dump_data[i].index;
		dump_data->smc_ch = sci_dbg_data->smc_dump_data[i].smc_ch;
		dump_data->time = sci_dbg_data->smc_dump_data[i].time;
		for (j = 0; j < 10; j++)
			dump_data->count[j] = sci_dbg_data->smc_dump_data[i].count[j];

		buffer_cnt += (dump_entry_size / 4);
	}

	remain_buff_size = sci_dbg_data->dump_addr.buff_size - buffer_cnt;

	if (remain_buff_size < dump_entry_size)
		buffer_cnt = 0;
}

static enum hrtimer_restart smc_count_monitor(struct hrtimer *hrtimer)
{
	enum hrtimer_restart ret = HRTIMER_RESTART;
	int i;

	for (i = 0; i < 4; i++) {
		sci_dbg_data->smc_dump_data[i].index++;
		sci_dbg_data->smc_dump_data[i].smc_ch = i;
		__raw_writel(0x0, sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMNC);
	}


	for (i = 0; i < 4; i++) {
		sci_dbg_data->smc_dump_data[i].time = cpu_clock(raw_smp_processor_id());
		sci_dbg_data->smc_dump_data[i].count[0] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_CCNT_LOW);
		sci_dbg_data->smc_dump_data[i].count[1] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_CCNT_HIGH);
		sci_dbg_data->smc_dump_data[i].count[2] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT0);
		sci_dbg_data->smc_dump_data[i].count[3] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT1);
		sci_dbg_data->smc_dump_data[i].count[4] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT2);
		sci_dbg_data->smc_dump_data[i].count[5] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT3);
		sci_dbg_data->smc_dump_data[i].count[6] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT4);
		sci_dbg_data->smc_dump_data[i].count[7] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT5);
		sci_dbg_data->smc_dump_data[i].count[8] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT6);
		sci_dbg_data->smc_dump_data[i].count[9] =
			__raw_readl(sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMCNT7);

		/* PPC_DEBUG: reset CCNT */
		__raw_writel(0x800000FF, sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_CCNT);
		/* PPC_DEBUG: PPC_Counter Reset  */
		__raw_writel(0x800000FF, sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_CNTENS);
		__raw_writel(0x1, sci_dbg_data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMNC);
	}

	smc_dump_data_save();

	hrtimer_forward_now(hrtimer, ms_to_ktime(HRTIMER_DURATION));

	return ret;
}

static int exynos_smc_dump_enable(struct exynos_sci_dbg_data *data,
					unsigned int enable)
{
	int i;

	if (enable) {
		for (i = 0; i < 4; i++)
			data->smc_dump_data[i].index = 0;
		buffer_cnt = 0;
		memset(data->dump_addr.v_addr, 0, data->dump_addr.buff_size);

		for (i = 0; i < 9; i++) {
			/* Disable all debug block */
			__raw_writel(smc0_dbg_blk_ctl_value[i], data->dump_addr.smc_mif_base[0] + SMC_DBG_BLK_CTL(i));
			__raw_writel(smc1_dbg_blk_ctl_value[i], data->dump_addr.smc_mif_base[1] + SMC_DBG_BLK_CTL(i));
			__raw_writel(smc2_dbg_blk_ctl_value[i], data->dump_addr.smc_mif_base[2] + SMC_DBG_BLK_CTL(i));
			__raw_writel(smc3_dbg_blk_ctl_value[i], data->dump_addr.smc_mif_base[3] + SMC_DBG_BLK_CTL(i));
		}

		for (i = 0; i < 4; i++) {
			__raw_writel(smc_global_dbg_ctl_value, data->dump_addr.smc_mif_base[i] + SMC_GLOBAL_DBG_CTL);

			__raw_writel(sysreg_mif_ppc_debug_value[i], data->dump_addr.sysreg_mif_base[i] + 0x1000);
			__raw_writel(sysreg_mif_ppc_debug_value2[i], data->dump_addr.sysreg_mif_base[i] + 0x1004);
			__raw_writel(0x11, data->dump_addr.sysreg_mif_base[i] + 0x102C);
		}

		for (i = 0; i < 4; i++) {
			/* PPC_DEBUG: reset CCNT */
			__raw_writel(0x800000FF, data->dump_addr.ppc_dbg_base[i] + SMC_PPC_CCNT);
			/* CCNT, PMCNT0~4 Enable */
			__raw_writel(0x800000FF, data->dump_addr.ppc_dbg_base[i] + SMC_PPC_CNTENS);
			/* PPC enable */
			__raw_writel(0x1, data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMNC);
		}
		hrtimer_start(&data->smc_hrtimer,
				ms_to_ktime(duration), HRTIMER_MODE_REL);
	} else {
		hrtimer_try_to_cancel(&data->smc_hrtimer);
		for (i = 0; i < 4; i++)
			__raw_writel(0x0, data->dump_addr.ppc_dbg_base[i] + SMC_PPC_PMNC);
	}

	return 0;
}

static ssize_t sci_dump_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int enable = data->dump_enable;

	count += snprintf(buf + count, PAGE_SIZE, "SCI dump enable: %s\n",
			enable ? "enable" : "disable");

	return count;
}

void smc_ppc_enable(unsigned int enable)
{
	int ret;

	ret = exynos_smc_dump_enable(sci_dbg_data, enable);
	sci_dbg_data->smc_dump_enable = enable;

	return;
}
EXPORT_SYMBOL(smc_ppc_enable);

void sci_ppc_enable(unsigned int enable)
{
	int ret;

	ret = exynos_sci_dump_enable(sci_dbg_data, enable);
	sci_dbg_data->dump_enable = enable;

	return;
}
EXPORT_SYMBOL(sci_ppc_enable);

static ssize_t sci_dump_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	unsigned int enable;
	int ret;

	ret = sscanf(buf, "%u",	&enable);
	if (ret != 1)
		return -EINVAL;

	ret = exynos_sci_dump_enable(data, enable);
	if (ret) {
		SCI_DBG_ERR("%s: Failed sci dump enable control\n", __func__);
		return ret;
	}

	data->dump_enable = enable;

	return count;
}

static ssize_t debug_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "exynos_sci_llc_debug_mode: %s\n",
			exynos_sci_llc_debug_mode? "Enabled":"Disabled");

	return count;
}

static ssize_t debug_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%d",	&exynos_sci_llc_debug_mode);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t DebugSrc10_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "DebugSrc10: 0x%08X\n", DebugSrc10);

	return count;
}

static ssize_t DebugSrc10_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%x",	&DebugSrc10);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t DebugSrc32_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "DebugSrc32: 0x%08X\n", DebugSrc32);

	return count;
}

static ssize_t DebugSrc32_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%x",	&DebugSrc32);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t SCI_EVENT_SEL_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "PPC_DEBUG_SCI_EVENT_SEL: 0x%08X\n", PPC_DEBUG_SCI_EVENT_SEL);

	return count;
}

static ssize_t SCI_EVENT_SEL_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%x",	&PPC_DEBUG_SCI_EVENT_SEL);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t smc_dbg_blk_ctl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t count = 0;

	for (i = 0; i < NUM_OF_SMC_DBG_BLK_CTL; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "SMC0_DBG_BLK_CTL%u: 0x%08X\n",
				i, smc0_dbg_blk_ctl_value[i]);
	}

	for (i = 0; i < NUM_OF_SMC_DBG_BLK_CTL; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "SMC1_DBG_BLK_CTL%u: 0x%08X\n",
				i, smc1_dbg_blk_ctl_value[i]);
	}

	for (i = 0; i < NUM_OF_SMC_DBG_BLK_CTL; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "SMC2_DBG_BLK_CTL%u: 0x%08X\n",
				i, smc2_dbg_blk_ctl_value[i]);
	}

	for (i = 0; i < NUM_OF_SMC_DBG_BLK_CTL; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "SMC3_DBG_BLK_CTL%u: 0x%08X\n",
				i, smc3_dbg_blk_ctl_value[i]);
	}

	return count;
}

static ssize_t smc_dbg_blk_ctl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u32 index, value;

	ret = sscanf(buf, "%u %x", &index, &value);
	if (ret != 2)
		return -EINVAL;

	if (index >= NUM_OF_SMC_DBG_BLK_CTL)
		return -EINVAL;

	smc0_dbg_blk_ctl_value[index] = value;
	smc1_dbg_blk_ctl_value[index] = value;
	smc2_dbg_blk_ctl_value[index] = value;
	smc3_dbg_blk_ctl_value[index] = value;

	return count;
}

static ssize_t smc_mif0_dbg_blk_ctl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u32 index, value;

	ret = sscanf(buf, "%u %x", &index, &value);
	if (ret != 2)
		return -EINVAL;

	if (index >= NUM_OF_SMC_DBG_BLK_CTL)
		return -EINVAL;

	smc0_dbg_blk_ctl_value[index] = value;

	return count;
}

static ssize_t smc_mif1_dbg_blk_ctl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u32 index, value;

	ret = sscanf(buf, "%u %x", &index, &value);
	if (ret != 2)
		return -EINVAL;

	if (index >= NUM_OF_SMC_DBG_BLK_CTL)
		return -EINVAL;

	smc1_dbg_blk_ctl_value[index] = value;

	return count;
}

static ssize_t smc_mif2_dbg_blk_ctl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u32 index, value;

	ret = sscanf(buf, "%u %x", &index, &value);
	if (ret != 2)
		return -EINVAL;

	if (index >= NUM_OF_SMC_DBG_BLK_CTL)
		return -EINVAL;

	smc2_dbg_blk_ctl_value[index] = value;

	return count;
}

static ssize_t smc_mif3_dbg_blk_ctl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u32 index, value;

	ret = sscanf(buf, "%u %x", &index, &value);
	if (ret != 2)
		return -EINVAL;

	if (index >= NUM_OF_SMC_DBG_BLK_CTL)
		return -EINVAL;

	smc3_dbg_blk_ctl_value[index] = value;

	return count;
}

static ssize_t smc_global_dbg_ctl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "GlobalDbgCtl: 0x%08X\n",
			smc_global_dbg_ctl_value);

	return count;
}

static ssize_t smc_global_dbg_ctl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;

	ret = sscanf(buf, "%x", &smc_global_dbg_ctl_value);
	if (ret != 1)
		return -EINVAL;

	return count;
}

static ssize_t sysreg_mif_ppc_debug_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t count = 0;

	for (i = 0; i < NUM_OF_SYSREG_MIF; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "MIF_PPC_DEBUG%u: 0x%08X\n",
				i, sysreg_mif_ppc_debug_value[i]);
	}

	for (i = 0; i < NUM_OF_SYSREG_MIF; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "MIF_PPC_DEBUG2%u: 0x%08X\n",
				i, sysreg_mif_ppc_debug_value2[i]);
	}


	return count;
}

static ssize_t sysreg_mif_ppc_debug_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	u32 index, value1, value2;

	ret = sscanf(buf, "%u %x %x", &index, &value1, &value2);
	if (ret != 3)
		return -EINVAL;

	if (index >= NUM_OF_SYSREG_MIF)
		return -EINVAL;

	sysreg_mif_ppc_debug_value[index] = value1;
	sysreg_mif_ppc_debug_value2[index] = value2;

	return count;
}

static ssize_t smc_mif_spare_cfg_ctl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int i = 0;
	ssize_t count = 0;

	for (i = 0; i < NUM_OF_SYSREG_MIF; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "MIF[%d] 0x1C%d3_F48C: 0x%.8X\n",
				i, i, __raw_readl(data->dump_addr.smc_mif_base[i] + SMC_SPARE_CFG_CTL));
	}

	return count;
}

static ssize_t smc_mif_spare_cfg_ctl_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	unsigned int i, reg;
	int ret;

	ret = sscanf(buf, "%u %x", &i, &reg);
	if (ret != 2)
		return -EINVAL;

	__raw_writel(reg, data->dump_addr.smc_mif_base[i] + SMC_SPARE_CFG_CTL);

	return count;
}

static ssize_t smc_dump_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	unsigned int enable = data->smc_dump_enable;

	count += snprintf(buf + count, PAGE_SIZE, "SMC dump enable: %s\n",
			enable ? "enable" : "disable");

	return count;
}

static ssize_t smc_dump_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	unsigned int enable;
	int ret;

	ret = sscanf(buf, "%u",	&enable);
	if (ret != 1)
		return -EINVAL;

	ret = exynos_smc_dump_enable(data, enable);
	if (ret) {
		SCI_DBG_ERR("%s: Failed sci dump enable control\n", __func__);
		return ret;
	}

	data->smc_dump_enable = enable;

	return count;
}

static ssize_t llc_ctrl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	llc_ctrl = __raw_readl(data->dump_addr.sci_base + LLCControl);	// LLCControl

	count += snprintf(buf + count, PAGE_SIZE, "LLCControl: 0x%08X\n", llc_ctrl);

	return count;
}

static ssize_t llc_ctrl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;

	ret = sscanf(buf, "%x",	&llc_ctrl);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(llc_ctrl, data->dump_addr.sci_base + LLCControl);	// LLCControl

	return count;
}

static ssize_t llc_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;
	u32 llc_alloc;

	for (i = 0; i < LLC_ID_MAX; i++) {
		llc_id[i] = __raw_readl(data->dump_addr.sci_base + LLCId_0 + (i * 0x8));	// LLCId_x
		llc_alloc = __raw_readl(data->dump_addr.sci_base + LLCIdAllocLkup_0 + (i * 0x8));
		count += snprintf(buf + count, PAGE_SIZE, "LLCId_%d: 0x%08X / ", i, llc_id[i]);
		count += snprintf(buf + count, PAGE_SIZE, "LLCIdAllocLkup_%d: 0x%08X\n", i, llc_alloc);
	}

	return count;
}

static ssize_t llc_id_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 id, llc_val, llc_alloc;

	ret = sscanf(buf, "%d %x %x", &id, &llc_val, &llc_alloc);
	if (ret != 3)
		return -EINVAL;

	if (id >= LLC_ID_MAX)
		return -EINVAL;

	llc_id[id] = llc_val;

	__raw_writel(llc_id[id], data->dump_addr.sci_base + LLCId_0 + (id * 0x8));// LLCId_x
	__raw_writel(llc_alloc, data->dump_addr.sci_base + LLCIdAllocLkup_0 + (id * 0x8));	// LLCIdAllocLkup_x

	return count;
}

static ssize_t llc_addr_match_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	llc_addr_match = __raw_readl(data->dump_addr.sci_base + LLCAddrMatch);	// LLCAddrMatch

	count += snprintf(buf + count, PAGE_SIZE, "LLCAddrMatch: 0x%08X\n", llc_addr_match);

	return count;
}

static ssize_t llc_addr_match_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;

	ret = sscanf(buf, "%x",	&llc_addr_match);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(llc_addr_match, data->dump_addr.sci_base + LLCAddrMatch);	// LLCAddrMatch

	return count;
}

static ssize_t llc_addr_mask_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;

	llc_addr_mask = __raw_readl(data->dump_addr.sci_base + LLCAddrMask);	// LLCAddrMask

	count += snprintf(buf + count, PAGE_SIZE, "LLCAddrMask: 0x%08X\n", llc_addr_mask);

	return count;
}

static ssize_t llc_addr_mask_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;

	ret = sscanf(buf, "%x",	&llc_addr_mask);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(llc_addr_mask, data->dump_addr.sci_base + LLCAddrMask);	// LLCAddrMask

	return count;
}

static ssize_t irps_llc_user_config_default_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 config_default;
	int i;

	for (i = 0; i < 4; i++) {
		config_default = __raw_readl(data->dump_addr.trex_irps_base[i] + 0x4);
		count += snprintf(buf + count, PAGE_SIZE,
			"IRPS%d_LLC_USER_CONFIG_DEFAULT: 0x%08X\n", i, config_default);
	}

	return count;
}

static ssize_t irps_llc_user_config_default_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 config_default;
	int i;

	ret = sscanf(buf, "%x",	&config_default);
	if (ret != 1)
		return -EINVAL;

	for (i = 0; i < 4; i++)
		__raw_writel(config_default, data->dump_addr.trex_irps_base[i] + 0x4);

	return count;
}

static ssize_t irps_llc_user_config_match_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i, j;
	u32 config_match;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 16; j++) {
			config_match =
				__raw_readl(data->dump_addr.trex_irps_base[i]
						+ LLC_USER_CONFIG_MATCH + (j * 0x8));
			count += snprintf(buf + count, PAGE_SIZE,
				"IRPS%d_LLC_USER_CONFIG_MATCH_%d: 0x%08X\n", i, j, config_match);
		}
	}

	return count;
}

static ssize_t irps_llc_user_config_match_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 id, val;
	int i;

	ret = sscanf(buf, "%d %x", &id, &val);
	if (ret != 2)
		return -EINVAL;

	if (id >= 16)
		return -EINVAL;

	for (i = 0; i < 4; i++)
		__raw_writel(val, data->dump_addr.trex_irps_base[i] +
				LLC_USER_CONFIG_MATCH + (id * 0x8));

	return count;
}

static ssize_t irps_llc_user_config_user_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i, j;
	u32 config_user;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 16; j++) {
			config_user =
				__raw_readl(data->dump_addr.trex_irps_base[i]
						+ LLC_USER_CONFIG_USER + (j * 0x8));
			count += snprintf(buf + count, PAGE_SIZE,
				"IRPS%d_LLC_USER_CONFIG_USER_%d: 0x%08X\n", i, j, config_user);
		}
	}

	return count;
}

static ssize_t irps_llc_user_config_user_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 id, val;
	int i;

	ret = sscanf(buf, "%d %x", &id, &val);
	if (ret != 2)
		return -EINVAL;

	if (id >= 16)
		return -EINVAL;

	for (i = 0; i < 4; i++)
		__raw_writel(val, data->dump_addr.trex_irps_base[i] +
				LLC_USER_CONFIG_USER + (id * 0x8));

	return count;
}

static ssize_t cacheaid_user_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;
	u32 cacheaid_user;

	for (i = 0; i < 32; i++) {
		cacheaid_user = __raw_readl(data->cacheaid_base
					+ CACHEAID_USER + (i * 0x10));
		count += snprintf(buf + count, PAGE_SIZE,
				"CACHEAID_USER_%d: 0x%08X\n", i, cacheaid_user);
	}

	return count;
}

static ssize_t cacheaid_user_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 index, val;

	ret = sscanf(buf, "%d %x", &index, &val);
	if (ret != 2)
		return -EINVAL;

	if (index >= 32)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_USER + (index * 0x10));

	return count;
}

static ssize_t cacheaid_ctrl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;
	u32 cacheaid_ctrl;

	for (i = 0; i < 32; i++) {
		cacheaid_ctrl = __raw_readl(data->cacheaid_base
					+ CACHEAID_CTRL + (i * 0x10));
		count += snprintf(buf + count, PAGE_SIZE,
				"CACHEAID_CTRL_%d: 0x%08X\n", i, cacheaid_ctrl);
	}

	return count;
}

static ssize_t cacheaid_ctrl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 index, val;

	ret = sscanf(buf, "%d %x", &index, &val);
	if (ret != 2)
		return -EINVAL;

	if (index >= 32)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_CTRL + (index * 0x10));

	return count;
}

static ssize_t cacheaid_global_ctrl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 val;

	val = __raw_readl(data->cacheaid_base
			+ CACHEAID_GLOBAL_CTRL);
	count += snprintf(buf + count, PAGE_SIZE,
			"CACHEAID_GLOBAL_CTRL: 0x%08X\n", val);

	return count;
}

static ssize_t cacheaid_global_ctrl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 val;

	ret = sscanf(buf, "%x", &val);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_GLOBAL_CTRL);

	return count;
}

static ssize_t cacheaid_default_ctrl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 val;

	val = __raw_readl(data->cacheaid_base
			+ CACHEAID_DEFAULT_CTRL);
	count += snprintf(buf + count, PAGE_SIZE,
			"CACHEAID_DEFAULT_CTRL: 0x%08X\n", val);

	return count;
}

static ssize_t cacheaid_default_ctrl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 val;

	ret = sscanf(buf, "%x", &val);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_DEFAULT_CTRL);

	return count;
}

static ssize_t cacheaid_pmu_access_ctrl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 val;

	val = __raw_readl(data->cacheaid_base
			+ CACHEAID_PMU_ACCESS_CTRL);
	count += snprintf(buf + count, PAGE_SIZE,
			"CACHEAID_PMU_ACCESS_CTRL: 0x%08X\n", val);

	return count;
}

static ssize_t cacheaid_pmu_access_ctrl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 val;

	ret = sscanf(buf, "%x", &val);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_PMU_ACCESS_CTRL);

	return count;
}

static ssize_t cacheaid_pmu_access_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 val;

	val = __raw_readl(data->cacheaid_base
			+ CACHEAID_PMU_ACCESS_INFO);
	count += snprintf(buf + count, PAGE_SIZE,
			"CACHEAID_PMU_ACCESS_INFO: 0x%08X\n", val);

	return count;
}

static ssize_t cacheaid_pmu_access_info_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 val;

	ret = sscanf(buf, "%x", &val);
	if (ret != 1)
		return -EINVAL;

	__raw_writel(val, data->cacheaid_base +
			CACHEAID_PMU_ACCESS_INFO);

	return count;
}

static ssize_t duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "hrtimer_duration(ms): %u\n", duration);

	return count;
}

static ssize_t duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;

	ret = kstrtouint(buf, 0, &duration);
	if (ret != 1)
		return -EINVAL;

	return count;
}

unsigned int sfr_reg;

static ssize_t sfr_rw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	void __iomem *reg;

	if (sfr_reg != 0) {
		reg = ioremap(sfr_reg, SZ_4);
		count += snprintf(buf + count, PAGE_SIZE, "0x%.8X 0x%.8X\n", sfr_reg, __raw_readl(reg));
	}

	return count;
}


static ssize_t sfr_rw_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	unsigned int val;
	void __iomem *reg;

	ret = sscanf(buf, "%x %x", &sfr_reg, &val);
	if (ret != 2)
		return -EINVAL;

	if (sfr_reg != 0) {
		reg = ioremap(sfr_reg, SZ_4);
		__raw_writel(val, reg);
	}

	return count;
}

static DEVICE_ATTR_RW(debug_mode);
static DEVICE_ATTR_RW(DebugSrc10);
static DEVICE_ATTR_RW(DebugSrc32);
static DEVICE_ATTR_RW(SCI_EVENT_SEL);
static DEVICE_ATTR_RW(smc_dbg_blk_ctl);
static DEVICE_ATTR_WO(smc_mif0_dbg_blk_ctl);
static DEVICE_ATTR_WO(smc_mif1_dbg_blk_ctl);
static DEVICE_ATTR_WO(smc_mif2_dbg_blk_ctl);
static DEVICE_ATTR_WO(smc_mif3_dbg_blk_ctl);
static DEVICE_ATTR_RW(smc_global_dbg_ctl);
static DEVICE_ATTR_RW(sysreg_mif_ppc_debug);
static DEVICE_ATTR_RW(smc_mif_spare_cfg_ctl);
static DEVICE_ATTR_RW(smc_dump_enable);
static DEVICE_ATTR_RW(sci_dump_enable);
static DEVICE_ATTR_RW(llc_ctrl);
static DEVICE_ATTR_RW(llc_id);
static DEVICE_ATTR_RW(llc_addr_match);
static DEVICE_ATTR_RW(llc_addr_mask);
static DEVICE_ATTR_RW(irps_llc_user_config_default);
static DEVICE_ATTR_RW(irps_llc_user_config_match);
static DEVICE_ATTR_RW(irps_llc_user_config_user);
static DEVICE_ATTR_RW(cacheaid_user);
static DEVICE_ATTR_RW(cacheaid_ctrl);
static DEVICE_ATTR_RW(cacheaid_global_ctrl);
static DEVICE_ATTR_RW(cacheaid_default_ctrl);
static DEVICE_ATTR_RW(cacheaid_pmu_access_ctrl);
static DEVICE_ATTR_RW(cacheaid_pmu_access_info);
static DEVICE_ATTR_RW(duration);
static DEVICE_ATTR_RW(sfr_rw);

static struct attribute *exynos_sci_dbg_sysfs_entries[] = {
	&dev_attr_debug_mode.attr,
	&dev_attr_DebugSrc10.attr,
	&dev_attr_DebugSrc32.attr,
	&dev_attr_SCI_EVENT_SEL.attr,
	&dev_attr_smc_dbg_blk_ctl.attr,
	&dev_attr_smc_mif0_dbg_blk_ctl.attr,
	&dev_attr_smc_mif1_dbg_blk_ctl.attr,
	&dev_attr_smc_mif2_dbg_blk_ctl.attr,
	&dev_attr_smc_mif3_dbg_blk_ctl.attr,
	&dev_attr_smc_global_dbg_ctl.attr,
	&dev_attr_smc_mif_spare_cfg_ctl.attr,
	&dev_attr_sysreg_mif_ppc_debug.attr,
	&dev_attr_sci_dump_enable.attr,
	&dev_attr_smc_dump_enable.attr,
	&dev_attr_llc_ctrl.attr,
	&dev_attr_llc_id.attr,
	&dev_attr_llc_addr_match.attr,
	&dev_attr_llc_addr_mask.attr,
	&dev_attr_irps_llc_user_config_default.attr,
	&dev_attr_irps_llc_user_config_match.attr,
	&dev_attr_irps_llc_user_config_user.attr,
	&dev_attr_cacheaid_user.attr,
	&dev_attr_cacheaid_ctrl.attr,
	&dev_attr_cacheaid_global_ctrl.attr,
	&dev_attr_cacheaid_default_ctrl.attr,
	&dev_attr_cacheaid_pmu_access_ctrl.attr,
	&dev_attr_cacheaid_pmu_access_info.attr,
	&dev_attr_duration.attr,
	&dev_attr_sfr_rw.attr,
	NULL,
};

static struct attribute_group exynos_sci_dbg_attr_group = {
	.name	= "sci_dbg_attr",
	.attrs	= exynos_sci_dbg_sysfs_entries,
};

static void __iomem *exynos_sci_dbg_remap(unsigned long addr, unsigned int size)
{
	int i;
	unsigned int num_pages = (size >> PAGE_SHIFT);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages = NULL;
	void __iomem *v_addr = NULL;

	if (!addr)
		return 0;

	pages = kmalloc_array(num_pages, sizeof(struct page *), GFP_ATOMIC);
	if (!pages)
		return 0;

	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(addr);
		addr += PAGE_SIZE;
	}

	v_addr = vmap(pages, num_pages, VM_MAP, prot);
	kfree(pages);

	return v_addr;
}

static int exynos_sci_dbg_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	struct exynos_sci_dbg_data *data;

	rmem_np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		dev_err(&pdev->dev, "failed to acquire memory region\n");
		return 0;
	}

	ppc_reserved.p_addr = rmem->base;
	ppc_reserved.p_size = rmem->size;

	data = kzalloc(sizeof(struct exynos_sci_dbg_data), GFP_KERNEL);
	if (data == NULL) {
		SCI_DBG_ERR("%s: failed to allocate SCI_DBG device\n", __func__);
		ret = -ENOMEM;
		goto err_data;
	}

	sci_dbg_data = data;
	data->dev = &pdev->dev;

	spin_lock_init(&data->lock);

	data->sci_base = ioremap(SCI_BASE, SZ_4K);
	data->cacheaid_base = ioremap(CACHEAID_BASE, SZ_4K);
	if (IS_ERR(data->sci_base) || IS_ERR(data->cacheaid_base)) {
		SCI_DBG_ERR("%s: Failed SCI base remap\n", __func__);
		goto err_ioremap;
	}

	data->dump_addr.p_addr = ppc_reserved.p_addr;
	data->dump_addr.p_size = ppc_reserved.p_size;
	data->dump_addr.v_addr = exynos_sci_dbg_remap(data->dump_addr.p_addr,
			data->dump_addr.p_size);
	SCI_DBG_INFO("%s: p_addr = 0x%08X\n", __func__, data->dump_addr.p_addr);
	SCI_DBG_INFO("%s: p_size = 0x%X\n", __func__, data->dump_addr.p_size);
	SCI_DBG_INFO("%s: v_addr = 0x%08X\n", __func__, data->dump_addr.v_addr);

	data->dump_addr.buff_size = data->dump_addr.p_size;
	dbg_snapshot_add_bl_item_info("log_ppc",
			data->dump_addr.p_addr, data->dump_addr.p_size);

	data->dump_addr.cnt_sfr_base = ioremap(PPC_DEBUG_CCI, SZ_4K);/* PPC_DEBUG_CCI */
	data->dump_addr.trex_core_base = ioremap(SYSREG_CORE_PPC_BASE, SZ_4K);
	data->dump_addr.sci_base = ioremap(SCI_BASE, SZ_4K);
	data->dump_addr.trex_irps_base[0] = ioremap(TREX_IRPS0_BASE, SZ_4K);
	data->dump_addr.trex_irps_base[1] = ioremap(TREX_IRPS1_BASE, SZ_4K);
	data->dump_addr.trex_irps_base[2] = ioremap(TREX_IRPS2_BASE, SZ_4K);
	data->dump_addr.trex_irps_base[3] = ioremap(TREX_IRPS3_BASE, SZ_4K);

	data->dump_addr.sysreg_mif_base[0] = ioremap(SYSREG_MIF0_BASE, SZ_8K);
	data->dump_addr.sysreg_mif_base[1] = ioremap(SYSREG_MIF1_BASE, SZ_8K);
	data->dump_addr.sysreg_mif_base[2] = ioremap(SYSREG_MIF2_BASE, SZ_8K);
	data->dump_addr.sysreg_mif_base[3] = ioremap(SYSREG_MIF3_BASE, SZ_8K);
	data->dump_addr.smc_mif_base[0] = ioremap(SMC_MIF0_BASE, SZ_8K);
	data->dump_addr.smc_mif_base[1] = ioremap(SMC_MIF1_BASE, SZ_8K);
	data->dump_addr.smc_mif_base[2] = ioremap(SMC_MIF2_BASE, SZ_8K);
	data->dump_addr.smc_mif_base[3] = ioremap(SMC_MIF3_BASE, SZ_8K);
	data->dump_addr.ppc_dbg_base[0] = ioremap(PPC_DEBUG0_BASE, SZ_4K);
	data->dump_addr.ppc_dbg_base[1] = ioremap(PPC_DEBUG1_BASE, SZ_4K);
	data->dump_addr.ppc_dbg_base[2] = ioremap(PPC_DEBUG2_BASE, SZ_4K);
	data->dump_addr.ppc_dbg_base[3] = ioremap(PPC_DEBUG3_BASE, SZ_4K);

	if (IS_ERR(data->dump_addr.cnt_sfr_base) ||
			IS_ERR(data->dump_addr.trex_core_base) ||
			IS_ERR(data->dump_addr.sci_base) ||
			IS_ERR(data->dump_addr.trex_irps_base[0]) ||
			IS_ERR(data->dump_addr.trex_irps_base[1]) ||
			IS_ERR(data->dump_addr.trex_irps_base[2]) ||
			IS_ERR(data->dump_addr.trex_irps_base[3]) ||
			IS_ERR(data->dump_addr.sysreg_mif_base[0]) ||
			IS_ERR(data->dump_addr.sysreg_mif_base[1]) ||
			IS_ERR(data->dump_addr.sysreg_mif_base[2]) ||
			IS_ERR(data->dump_addr.sysreg_mif_base[3]) ||
			IS_ERR(data->dump_addr.smc_mif_base[0]) ||
			IS_ERR(data->dump_addr.smc_mif_base[1]) ||
			IS_ERR(data->dump_addr.smc_mif_base[2]) ||
			IS_ERR(data->dump_addr.smc_mif_base[3]) ||
			IS_ERR(data->dump_addr.ppc_dbg_base[0]) ||
			IS_ERR(data->dump_addr.ppc_dbg_base[1]) ||
			IS_ERR(data->dump_addr.ppc_dbg_base[2]) ||
			IS_ERR(data->dump_addr.ppc_dbg_base[3]))
		goto err_ioremap_all;

	hrtimer_init(&data->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->hrtimer.function = count_monitor;

	hrtimer_init(&data->smc_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->smc_hrtimer.function = smc_count_monitor;

	platform_set_drvdata(pdev, data);

	ret = sysfs_create_group(&data->dev->kobj, &exynos_sci_dbg_attr_group);
	if (ret)
		SCI_DBG_ERR("%s: failed creat sysfs for Exynos SCI_DBG\n", __func__);

	SCI_DBG_INFO("%s: exynos SCI_DBG driver is initialized!!\n", __func__);

	return 0;

err_ioremap_all:
	iounmap(data->dump_addr.cnt_sfr_base);
	iounmap(data->dump_addr.trex_core_base);
	iounmap(data->dump_addr.sci_base);
	iounmap(data->dump_addr.trex_irps_base[0]);
	iounmap(data->dump_addr.trex_irps_base[1]);
	iounmap(data->dump_addr.trex_irps_base[2]);
	iounmap(data->dump_addr.trex_irps_base[3]);
	iounmap(data->dump_addr.sysreg_mif_base[0]);
	iounmap(data->dump_addr.sysreg_mif_base[1]);
	iounmap(data->dump_addr.sysreg_mif_base[2]);
	iounmap(data->dump_addr.sysreg_mif_base[3]);
	iounmap(data->dump_addr.smc_mif_base[0]);
	iounmap(data->dump_addr.smc_mif_base[1]);
	iounmap(data->dump_addr.smc_mif_base[2]);
	iounmap(data->dump_addr.smc_mif_base[3]);
	iounmap(data->dump_addr.ppc_dbg_base[0]);
	iounmap(data->dump_addr.ppc_dbg_base[1]);
	iounmap(data->dump_addr.ppc_dbg_base[2]);
	iounmap(data->dump_addr.ppc_dbg_base[3]);

err_ioremap:
	kfree(data);

err_data:
	return ret;
}

static int exynos_sci_dbg_remove(struct platform_device *pdev)
{
	struct exynos_sci_dbg_data *data = platform_get_drvdata(pdev);

	sysfs_remove_group(&data->dev->kobj, &exynos_sci_dbg_attr_group);
	platform_set_drvdata(pdev, NULL);
	iounmap(data->sci_base);
	iounmap(data->dump_addr.cnt_sfr_base);
	kfree(data);

	SCI_DBG_INFO("%s: exynos SCI_DBG driver is removed!!\n", __func__);

	return 0;
}

static struct platform_device_id exynos_sci_dbg_driver_ids[] = {
	{ .name = EXYNOS_SCI_DBG_MODULE_NAME, },
	{},
};
MODULE_DEVICE_TABLE(platform, exynos_sci_dbg_driver_ids);

static const struct of_device_id exynos_sci_dbg_match[] = {
	{ .compatible = "samsung,exynos-sci_dbg", },
	{},
};

struct platform_driver exynos_sci_dbg_driver = {
	.remove = exynos_sci_dbg_remove,
	.id_table = exynos_sci_dbg_driver_ids,
	.driver = {
		.name = EXYNOS_SCI_DBG_MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = exynos_sci_dbg_match,
	},
	.probe = exynos_sci_dbg_probe,
};

MODULE_AUTHOR("Junhee Yoo <jh22.yoo@samsung.com>, Taekki Kim <taekki.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung SCI_DBG dump-to-DRAM driver");
MODULE_LICENSE("GPL");
