// SPDX-License-Identifier: GPL-2.0
#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/cal-if.h>
#if IS_ENABLED(CONFIG_EXYNOS_PMU_IF)
#include <soc/samsung/exynos-pmu-if.h>
#else
#include <soc/samsung/exynos-pmu.h>
#endif

#include "../cal-if/acpm_dvfs.h"
#include "pmu-gnss.h"
#include "gnss_prj.h"

/* Connectivity sub system */
#define EXYNOS_GNSS		(0)
/* Target to set */
#define EXYNOS_SET_CONN_TZPC	(0)

#define gnss_pmu_read	exynos_pmu_read
#define gnss_pmu_write	exynos_pmu_write
#define gnss_pmu_update exynos_pmu_update

#if IS_ENABLED(CONFIG_SOC_EXYNOS9630)
#define BAAW_GNSS_CMGP_ADDR	(0x13FE0000)
#define BAAW_GNSS_CMGP_SIZE	(SZ_64K)

#define BAAW_GNSS_DBUS_ADDR	(0x13FD0000)
#define BAAW_GNSS_DBUS_SIZE	(SZ_64K)

#define CMUPMU_ADDR (0x13E43000)

#elif IS_ENABLED(CONFIG_SOC_EXYNOS3830)
#define BAAW_GNSS_CMGP_ADDR	(0x13FE0000)
#define BAAW_GNSS_CMGP_SIZE	(SZ_64K)

#define BAAW_GNSS_DBUS_ADDR	(0x13FD0000)
#define BAAW_GNSS_DBUS_SIZE	(SZ_64K)

#define CMUPMU_ADDR (0x13E43000)

#elif IS_ENABLED(CONFIG_SOC_S5E9815)
#define BAAW_GNSS_CMGP_ADDR	(0x159E0000)
#define BAAW_GNSS_CMGP_SIZE	(SZ_64K)

#define BAAW_GNSS_DBUS_ADDR	(0x159D0000)
#define BAAW_GNSS_DBUS_SIZE	(SZ_64K)

#elif IS_ENABLED(CONFIG_SOC_S5E9925)
#define BAAW_GNSS_CMGP_ADDR	(0x147E0000)
#define BAAW_GNSS_CMGP_SIZE	(SZ_64K)

/* Set OFFSET only. ADDR and SIZE are the same with CMGP values. */
#define BAAW_GNSS_DBUS_OFFSET	(0x20)

#elif IS_ENABLED(CONFIG_SOC_S5E8825)
#define BAAW_GNSS_CMGP_ADDR	(0x13FE0000)
#define BAAW_GNSS_CMGP_SIZE	(SZ_64K)

#define BAAW_GNSS_DBUS_ADDR	(0x13FD0000)
#define BAAW_GNSS_DBUS_SIZE	(SZ_64K)

#define SYSREG_ALIVE_ADDR	(0x11820000)
#define LPP_RA1_HD_ADME_OFFSET	(0x00000314)

static void __iomem *sysreg_alive_reg;
#endif

static u32 g_shmem_size;
static u64 g_shmem_base;
static u32 g_base_addr;
static u32 g_base_addr_2nd;

static void __iomem *baaw_cmgp_reg;
static void __iomem *baaw_dbus_reg;
static unsigned int baaw_dbus_offset;

int gnss_cmgp_read(unsigned int reg_offset, unsigned int *ret)
{
	if (baaw_cmgp_reg == NULL)
		return -EIO;

	*ret = __raw_readl(baaw_cmgp_reg + reg_offset);
	return 0;
}

int gnss_cmgp_write(unsigned int reg_offset, unsigned int val)
{
	unsigned int read_val = 0;

	if (baaw_cmgp_reg == NULL)
		return -EIO;

	__raw_writel(val, baaw_cmgp_reg + reg_offset);
	read_val = __raw_readl(baaw_cmgp_reg + reg_offset);

	if (val != read_val)
		gif_err("ADDR:0x%08X DATA:0x%08X => Read to verify:0x%08X\n",
			BAAW_GNSS_CMGP_ADDR + reg_offset, val,
			__raw_readl(baaw_cmgp_reg + reg_offset));

	return 0;
}

int gnss_dbus_read(unsigned int reg_offset, unsigned int *ret)
{
	unsigned int offset = baaw_dbus_offset + reg_offset;

	if (baaw_dbus_reg == NULL)
		return -EIO;

	*ret = __raw_readl(baaw_dbus_reg + offset);
	return 0;
}

int gnss_dbus_write(unsigned int reg_offset, unsigned int val)
{
	unsigned int offset = baaw_dbus_offset + reg_offset;
	unsigned int read_val = 0;

	if (baaw_dbus_reg == NULL)
		return -EIO;

	__raw_writel(val, baaw_dbus_reg + offset);
	read_val = __raw_readl(baaw_dbus_reg + offset);

	if (val != read_val) {
		gif_err("DATA:0x%08X => Read to verify:0x%08X\n",
			val, __raw_readl(baaw_dbus_reg + offset));
	}

	return 0;
}

static int gnss_pmu_clear_interrupt(enum gnss_int_clear gnss_int)
{
	switch (gnss_int) {
	case GNSS_INT_WAKEUP_CLEAR:
		break;
	case GNSS_INT_ACTIVE_CLEAR:
		cal_gnss_active_clear();
		break;
	case GNSS_INT_WDT_RESET_CLEAR:
		break;
	default:
		gif_err("Unexpected interrupt value!\n");
		return -EIO;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_SOC_EXYNOS9630) || IS_ENABLED(CONFIG_SOC_EXYNOS3830)
static void gnss_get_swreg(struct gnss_swreg *swreg)
{
	exynos_smc_readsfr(CMUPMU_ADDR, (unsigned long *)&swreg->swreg_0);
	exynos_smc_readsfr(CMUPMU_ADDR + 0x4, (unsigned long *)&swreg->swreg_1);
	exynos_smc_readsfr(CMUPMU_ADDR + 0x8, (unsigned long *)&swreg->swreg_2);
	exynos_smc_readsfr(CMUPMU_ADDR + 0xC, (unsigned long *)&swreg->swreg_3);
	exynos_smc_readsfr(CMUPMU_ADDR + 0x10, (unsigned long *)&swreg->swreg_4);
	exynos_smc_readsfr(CMUPMU_ADDR + 0x14, (unsigned long *)&swreg->swreg_5);

	gif_info("SWREG 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n",
			swreg->swreg_0, swreg->swreg_1, swreg->swreg_2,
			swreg->swreg_3, swreg->swreg_4, swreg->swreg_5);
}
#endif

static void gnss_get_apreg(struct gnss_apreg *apreg)
{
	gnss_pmu_read(EXYNOS_PMU_GNSS_CTRL_NS, &apreg->CTRL_NS);
	gnss_pmu_read(EXYNOS_PMU_GNSS_CTRL_S, &apreg->CTRL_S);
	gnss_pmu_read(EXYNOS_PMU_GNSS_STAT, &apreg->STAT);
	gnss_pmu_read(EXYNOS_PMU_GNSS_DEBUG, &apreg->DEBUG);
	gif_info("APREG CTRL_NS 0x%08X CTRL_S 0x%08X STAT 0x%08X DEBUG 0x%08X\n",
			apreg->CTRL_NS, apreg->CTRL_S, apreg->STAT, apreg->DEBUG);
}

#if IS_ENABLED(CONFIG_SOC_EXYNOS9630)
static void __iomem *intr_bid_pend; /* check APM pending before release reset */
static bool check_apm_int_pending(void)
{
	bool ret = false;
	int reg_val = 0;
	int count = 20; /* 50ms * 20 times = 1 sec */
	if (intr_bid_pend == NULL) {
		intr_bid_pend = ioremap(0x10E71A04, SZ_4);
		if (intr_bid_pend == NULL) {
			gif_err("Err: failed to ioremap GRP26_INTR_BID_PEND!\n");
			return ret;
		}
	}
	while (count > 0) {
		reg_val = __raw_readl(intr_bid_pend);
		gif_info("APM PENDING CHECK REGISTER VAL: 0x%08x\n", reg_val);
		if ((reg_val >> 17) & 0x3F) {/* check if one or more of bits [22:17] are 1 */
			count--;
			msleep(50);
			continue;
		} else {
			ret = true;
			break;
		}
	}
	return ret;
}
#else
static bool check_apm_int_pending(void)
{
	return true;
}
#endif

static int gnss_pmu_release_reset(void)
{
	int ret = 0;

	if (check_apm_int_pending())
		cal_gnss_reset_release();
	else
		ret = -1;

	return ret;
}

static int gnss_pmu_hold_reset(void)
{
	int ret = 0;

	if (check_apm_int_pending()) {
		cal_gnss_reset_assert();
		msleep(50);
	} else
		ret = 1;

	return ret;
}

static int gnss_request_tzpc(void)
{
	int ret;

	ret = (int)exynos_smc(SMC_CMD_CONN_IF, (EXYNOS_GNSS << 31) |
			EXYNOS_SET_CONN_TZPC, 0, 0);
	if (ret)
		gif_err("ERR: fail to TZPC setting - %d\n", ret);

	return ret;
}

static void gnss_request_gnss2ap_baaw(void)
{
#if IS_ENABLED(CONFIG_SOC_S5E8825)
	int lpp_adme_val = 0;
#endif

	gif_info("Config GNSS2AP BAAW\n");

	gif_info("DRAM Configuration\n");

#if IS_ENABLED(CONFIG_SOC_S5E9925)
	gnss_dbus_write(0x0, (g_base_addr >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x4, (g_base_addr >> MEMBASE_ADDR_SHIFT)
			+ (g_shmem_size >> MEMBASE_ADDR_SHIFT));
#else
	gnss_dbus_write(0x0, (MEMBASE_GNSS_ADDR >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x4, (MEMBASE_GNSS_ADDR >> MEMBASE_ADDR_SHIFT)
			+ (g_shmem_size >> MEMBASE_ADDR_SHIFT));
#endif
	gnss_dbus_write(0x8, (g_shmem_base >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0xC, 0x80000003);

#if IS_ENABLED(CONFIG_SOC_S5E9925)
	gnss_dbus_write(0x10, (g_base_addr_2nd >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x14, (g_base_addr_2nd >> MEMBASE_ADDR_SHIFT)
			+ (g_shmem_size >> MEMBASE_ADDR_SHIFT));
#else
	gnss_dbus_write(0x10, (MEMBASE_GNSS_ADDR_2ND >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x14, (MEMBASE_GNSS_ADDR_2ND >> MEMBASE_ADDR_SHIFT)
			+ (g_shmem_size >> MEMBASE_ADDR_SHIFT));
#endif
	gnss_dbus_write(0x18, (g_shmem_base >> MEMBASE_ADDR_SHIFT));
	gnss_dbus_write(0x1C, 0x80000003);

#if IS_ENABLED(CONFIG_SOC_EXYNOS3830)
	gif_info("MAILBOX CP APM AP CHUB WLBT\n");
	gnss_cmgp_write(0x00, 0x000B1960);	/* GNSS Start address >> 12bit */
	gnss_cmgp_write(0x04, 0x000B19B0);	/* GNSS End address >> 12bit */
	gnss_cmgp_write(0x08, 0x00011960);	/* AP Start address >> 12bit */
	gnss_cmgp_write(0x0C, 0x80000003);

	gif_info("CHUB_SRAM non cachable\n");
	gnss_cmgp_write(0x10, 0x000B0E00);
	gnss_cmgp_write(0x14, 0x000B0E40);
	gnss_cmgp_write(0x18, 0x00010E00);
	gnss_cmgp_write(0x1C, 0x80000003);

#elif IS_ENABLED(CONFIG_SOC_EXYNOS9630)
	gif_info("MAILBOX CP APM AP CHUB WLBT\n");
	gnss_cmgp_write(0x00, 0x000B0F60);	/* GNSS Start address >> 12bit */
	gnss_cmgp_write(0x04, 0x000B0FB0);	/* GNSS End address >> 12bit */
	gnss_cmgp_write(0x08, 0x00010F60);	/* AP Start address >> 12bit */
	gnss_cmgp_write(0x0C, 0x80000003);

	gif_info("CHUB_SRAM non cachable\n");
	gnss_cmgp_write(0x10, 0x000B1A00);
	gnss_cmgp_write(0x14, 0x000B1A68);
	gnss_cmgp_write(0x18, 0x00011A00);
	gnss_cmgp_write(0x1C, 0x80000003);

#elif IS_ENABLED(CONFIG_SOC_S5E9815)
	gif_info("MAILBOX WLBT\n");
	gnss_cmgp_write(0x00, 0x000B0A10);	/* GNSS Start address >> 12bit */
	gnss_cmgp_write(0x04, 0x000B0A20);	/* GNSS End address >> 12bit */
	gnss_cmgp_write(0x08, 0x00010A10);	/* AP Start address >> 12bit */
	gnss_cmgp_write(0x0C, 0x80000003);

	gif_info("MAILBOX CP APM AP CHUB\n");
	gnss_cmgp_write(0x00, 0x000B0A50);	/* GNSS Start address >> 12bit */
	gnss_cmgp_write(0x04, 0x000B0A90);	/* GNSS End address >> 12bit */
	gnss_cmgp_write(0x08, 0x00010A50);	/* AP Start address >> 12bit */
	gnss_cmgp_write(0x0C, 0x80000003);

	gif_info("CHUB_SRAM non cachable\n");
	gnss_cmgp_write(0x10, 0x000B1A00);
	gnss_cmgp_write(0x14, 0x000B1A40);
	gnss_cmgp_write(0x18, 0x000113B0);
	gnss_cmgp_write(0x1C, 0x80000003);

#elif IS_ENABLED(CONFIG_SOC_S5E9925)
	gif_info("MAILBOX CP APM AP CHUB\n");
	gnss_cmgp_write(0x00, 0x000B4C50);	/* GNSS Start address >> 12bit */
	gnss_cmgp_write(0x04, 0x000B4C90);	/* GNSS End address >> 12bit */
	gnss_cmgp_write(0x08, 0x00014C50);	/* AP Start address >> 12bit */
	gnss_cmgp_write(0x0C, 0x80000003);

	gif_info("CHUB_SRAM non cachable\n");
	gnss_cmgp_write(0x10, 0x000B1A0F);
	gnss_cmgp_write(0x14, 0x000B1A10);
	gnss_cmgp_write(0x18, 0x0000297F);
	gnss_cmgp_write(0x1C, 0x80000003);

	/* Modify BAAW_GNSS_DBUS_OFFSET if want to add more */

#elif IS_ENABLED(CONFIG_SOC_S5E8825)
	gif_info("MAILBOX CP APM AP CHUB WLBT\n");
	gnss_cmgp_write(0x00, 0x000B1960);	/* GNSS Start address >> 12bit */
	gnss_cmgp_write(0x04, 0x000B19B0);	/* GNSS End address >> 12bit */
	gnss_cmgp_write(0x08, 0x00011960);	/* AP Start address >> 12bit */
	gnss_cmgp_write(0x0C, 0x80000003);

	gif_info("CHUB_SRAM non cachable\n");
	gnss_cmgp_write(0x10, 0x000B1A0F);
	gnss_cmgp_write(0x14, 0x000B1A10);
	gnss_cmgp_write(0x18, 0x00011267);
	gnss_cmgp_write(0x1C, 0x80000003);

	lpp_adme_val = __raw_readl(sysreg_alive_reg + LPP_RA1_HD_ADME_OFFSET);
	gif_info("lpp adme val before set: 0x%08x\n", lpp_adme_val);

	lpp_adme_val = (0x7 << 20) | (lpp_adme_val & ~(0x7 << 20));
	gif_info("lpp adme val after set: 0x%08x\n", lpp_adme_val);

	__raw_writel(lpp_adme_val, sysreg_alive_reg + LPP_RA1_HD_ADME_OFFSET);
#endif
}

static int gnss_pmu_power_on(enum gnss_mode mode)
{
	int ret = 0;

	gif_info("mode[%d]\n", mode);

	if (mode == GNSS_POWER_ON) {
		if (cal_gnss_status() > 0) {
			if (check_apm_int_pending()) {
				gif_info("GNSS is already Power on, try reset\n");
				cal_gnss_reset_assert();
			} else
				ret = -1;
		} else {
			gif_info("GNSS Power On\n");
			cal_gnss_init();
		}
	} else {
		if (check_apm_int_pending())
			cal_gnss_reset_release();
		else
			ret = -1;
	}

	return ret;

}

static int gnss_pmu_init_conf(struct gnss_ctl *gc)
{
	u32 __maybe_unused shmem_size, shmem_base;

	baaw_cmgp_reg = devm_ioremap(gc->dev, BAAW_GNSS_CMGP_ADDR, BAAW_GNSS_CMGP_SIZE);
	if (!baaw_cmgp_reg) {
		gif_err("%s: pmu ioremap failed.\n", gc->pdata->name);
		return -EIO;
	}
	gif_info("baaw_cmgp_reg:%pK\n", baaw_cmgp_reg);

#ifdef BAAW_GNSS_DBUS_OFFSET
	baaw_dbus_reg = baaw_cmgp_reg;
	baaw_dbus_offset = BAAW_GNSS_DBUS_OFFSET;
#else
	baaw_dbus_reg = devm_ioremap(gc->dev, BAAW_GNSS_DBUS_ADDR, BAAW_GNSS_DBUS_SIZE);
	if (!baaw_dbus_reg) {
		gif_err("%s: pmu ioremap failed.\n", gc->pdata->name);
		return -EIO;
	}
#endif
	gif_info("baaw_dbus_reg:%pK offset:0x%X\n", baaw_dbus_reg, baaw_dbus_offset);

	g_shmem_size = gc->pdata->shmem_size;
	g_shmem_base = gc->pdata->shmem_base;
	g_base_addr = gc->pdata->base_addr;
	g_base_addr_2nd = gc->pdata->base_addr_2nd;

	gif_info("GNSS SHM address:0x%llX size:0x%08X\n", g_shmem_base, g_shmem_size);

#if IS_ENABLED(CONFIG_SOC_S5E8825)
	sysreg_alive_reg = devm_ioremap(gc->dev, SYSREG_ALIVE_ADDR, SZ_64K);
	if (!sysreg_alive_reg) {
		gif_err("sysreg alive ioremap failed.");
		return -EIO;
	}
#endif

	return 0;
}

static struct gnssctl_pmu_ops pmu_ops = {
	.init_conf = gnss_pmu_init_conf,
	.hold_reset = gnss_pmu_hold_reset,
	.release_reset = gnss_pmu_release_reset,
	.power_on = gnss_pmu_power_on,
	.clear_int = gnss_pmu_clear_interrupt,
	.req_security = gnss_request_tzpc,
	.req_baaw = gnss_request_gnss2ap_baaw,
#if IS_ENABLED(CONFIG_SOC_EXYNOS9630) || IS_ENABLED(CONFIG_SOC_EXYNOS3830)
	.get_swreg = gnss_get_swreg,
#endif
	.get_apreg = gnss_get_apreg,
};

void gnss_get_pmu_ops(struct gnss_ctl *gc)
{
	gc->pmu_ops = &pmu_ops;
}
