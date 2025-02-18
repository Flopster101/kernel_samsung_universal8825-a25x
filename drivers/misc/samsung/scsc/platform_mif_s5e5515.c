/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* Implements interface */

#include "platform_mif.h"

/* Interfaces it Uses */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/iommu.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/smc.h>
//#include <soc/samsun/exynos-smc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#endif
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <scsc/scsc_logring.h>
#include "mif_reg_S5E5515.h"
#include "platform_mif_module.h"
#include "miframman.h"
#include "platform_mif_s5e5515.h"
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
#include <linux/soc/samsung/exynos-soc.h>
#endif

#ifdef CONFIG_SCSC_QOS
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#else
#include <linux/pm_qos.h>
#endif
#endif

#if !defined(CONFIG_SOC_S5E5515)
#error Target processor CONFIG_SOC_S5E5515 not selected
#endif

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif
/* Time to wait for CFG_REQ IRQ on 9610 */
#define WLBT_BOOT_TIMEOUT (HZ)

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/debug-snapshot.h>
#endif

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
#include <linux/workqueue.h>
#endif

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
#include <soc/samsung/memlogger.h>
#endif


#ifdef CONFIG_WLBT_AUTOGEN_PMUCAL
#include "pmu_cal.h"
#endif

static unsigned long sharedmem_base;
static size_t sharedmem_size;

static bool fw_compiled_in_kernel;
module_param(fw_compiled_in_kernel, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fw_compiled_in_kernel, "Use FW compiled in kernel");

#ifdef CONFIG_SCSC_CHV_SUPPORT
static bool chv_disable_irq;
module_param(chv_disable_irq, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(chv_disable_irq, "Do not register for irq");
#endif

bool enable_hwbypass;
module_param(enable_hwbypass,bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_platform_mif_arm_reset, "Enables hwbypass");

static bool enable_platform_mif_arm_reset = true;
module_param(enable_platform_mif_arm_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_platform_mif_arm_reset, "Enables WIFIBT ARM cores reset");

static bool disable_apm_setup = true;
module_param(disable_apm_setup, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_apm_setup, "Disable host APM setup");

static uint wlbt_status_timeout_value = 500;
module_param(wlbt_status_timeout_value, uint , S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(wlbt_status_timeout_value, "wlbt_status_timeout(ms)");

static bool enable_scandump_wlbt_status_timeout = false;
module_param(enable_scandump_wlbt_status_timeout, bool , S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_scandump_wlbt_status_timeout, "wlbt_status_timeout(ms)");

static bool force_to_wlbt_status_timeout = false;
module_param(force_to_wlbt_status_timeout, bool , S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(force_to_wlbt_status_timeout, "wlbt_status_timeout(ms)");

static bool init_done;

#ifdef CONFIG_SCSC_QOS
struct qos_table {
	unsigned int freq_mif;
	unsigned int freq_int;
	unsigned int freq_cl0;
};
#endif

#include <linux/cpufreq.h>

static void power_supplies_on(struct platform_mif *platform);
inline void platform_int_debug(struct platform_mif *platform);

extern int mx140_log_dump(void);

#define platform_mif_from_mif_abs(MIF_ABS_PTR) container_of(MIF_ABS_PTR, struct platform_mif, interface)

inline void platform_mif_reg_write(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base + offset);
}

inline u32 platform_mif_reg_read(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base + offset);
}

inline void platform_mif_reg_write_wpan(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base_wpan + offset);
}

inline u32 platform_mif_reg_read_wpan(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base_wpan + offset);
}

#ifdef CONFIG_SCSC_QOS
static int platform_mif_set_affinity_cpu(struct scsc_mif_abs *interface, u8 cpu)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Change CPU affinity to %d\n", cpu);
	return irq_set_affinity_hint(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, cpumask_of(cpu));
}

static int platform_mif_parse_qos(struct platform_mif *platform, struct device_node *np)
{
	int len, i;

	platform->qos_enabled = false;

	len = of_property_count_u32_elems(np, "qos_table");
	if (!(len == 15)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "No qos table for wlbt, or incorrect size\n");
		return -ENOENT;
	}

	platform->qos = devm_kzalloc(platform->dev, sizeof(struct qos_table) * len / 4, GFP_KERNEL);
	if (!platform->qos)
		return -ENOMEM;

	of_property_read_u32_array(np, "qos_table", (unsigned int *)platform->qos, len);

	for (i = 0; i < len / 4; i++) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "QoS Table[%d] mif : %u int : %u cl0 : %u\n", i,
			platform->qos[i].freq_mif,
			platform->qos[i].freq_int,
			platform->qos[i].freq_cl0);
	}

	platform->qos_enabled = true;
	return 0;
}

struct qos_table platform_mif_pm_qos_get_table(struct platform_mif *platform, enum scsc_qos_config config)
{
	struct qos_table table;

	switch (config) {
	case SCSC_QOS_MIN:
		table.freq_mif = platform->qos[0].freq_mif;
		table.freq_int = platform->qos[0].freq_int;
		table.freq_cl0 = platform->qos[0].freq_cl0;
		break;

	case SCSC_QOS_MED:
		table.freq_mif = platform->qos[1].freq_mif;
		table.freq_int = platform->qos[1].freq_int;
		table.freq_cl0 = platform->qos[1].freq_cl0;
		break;

	case SCSC_QOS_MAX:
		table.freq_mif = platform->qos[2].freq_mif;
		table.freq_int = platform->qos[2].freq_int;
		table.freq_cl0 = platform->qos[2].freq_cl0;
		break;

	default:
		table.freq_mif = 0;
		table.freq_int = 0;
		table.freq_cl0 = 0;
	}

	return table;
}

static int platform_mif_pm_qos_add_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct qos_table table;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	int ret;
#endif

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	table = platform_mif_pm_qos_get_table(platform, config);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"PM QoS add request: %u. MIF %u INT %u CL0 %u\n", config, table.freq_mif, table.freq_int, table.freq_cl0);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	qos_req->cpu_cluster0_policy = cpufreq_cpu_get(0);

	if (!qos_req->cpu_cluster0_policy) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS add request error. CPU policy not loaded");
		return -ENOENT;
	}
	exynos_pm_qos_add_request(&qos_req->pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT, table.freq_mif);
	exynos_pm_qos_add_request(&qos_req->pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, table.freq_int);

	ret = freq_qos_tracer_add_request(&qos_req->cpu_cluster0_policy->constraints, &qos_req->pm_qos_req_cl0, FREQ_QOS_MIN, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS add request cl0. Setting freq_qos_add_request %d", ret);
#else
	pm_qos_add_request(&qos_req->pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT, table.freq_mif);
	pm_qos_add_request(&qos_req->pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, table.freq_int);
	pm_qos_add_request(&qos_req->pm_qos_req_cl0, PM_QOS_CLUSTER0_FREQ_MIN, table.freq_cl0);
#endif
	return 0;
}

static int platform_mif_pm_qos_update_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct qos_table table;

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	table = platform_mif_pm_qos_get_table(platform, config);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"PM QoS update request: %u. MIF %u INT %u CL0 %u\n", config, table.freq_mif, table.freq_int, table.freq_cl0);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	exynos_pm_qos_update_request(&qos_req->pm_qos_req_mif, table.freq_mif);
	exynos_pm_qos_update_request(&qos_req->pm_qos_req_int, table.freq_int);
	freq_qos_update_request(&qos_req->pm_qos_req_cl0, table.freq_cl0);
#else
	pm_qos_update_request(&qos_req->pm_qos_req_mif, table.freq_mif);
	pm_qos_update_request(&qos_req->pm_qos_req_int, table.freq_int);
	pm_qos_update_request(&qos_req->pm_qos_req_cl0, table.freq_cl0);
#endif

	return 0;
}

static int platform_mif_pm_qos_remove_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;


	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS remove request\n");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	exynos_pm_qos_remove_request(&qos_req->pm_qos_req_mif);
	exynos_pm_qos_remove_request(&qos_req->pm_qos_req_int);
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl0);
#else
	pm_qos_remove_request(&qos_req->pm_qos_req_mif);
	pm_qos_remove_request(&qos_req->pm_qos_req_int);
	pm_qos_remove_request(&qos_req->pm_qos_req_cl0);
#endif

	return 0;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static void platform_recovery_disabled_reg(struct scsc_mif_abs *interface, bool (*handler)(void))
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif recovery %pS\n", handler);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->recovery_disabled = handler;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_recovery_disabled_unreg(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif recovery\n");
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->recovery_disabled = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}
#endif

static void platform_mif_irq_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PLAT_MIF, NULL, "INT handler not registered\n");
}

static void platform_mif_irq_reset_request_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PLAT_MIF, NULL, "INT reset_request handler not registered\n");
}

irqreturn_t platform_mif_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "INT %pS\n", platform->wlan_handler);
	if (platform->wlan_handler != platform_mif_irq_default_handler) {
		platform->wlan_handler(irq, platform->irq_dev);
	}
	else {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "MIF Interrupt Handler not registered.\n");
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), (0xffff << 16));
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), (0xffff << 16));
	}

	return IRQ_HANDLED;
}

irqreturn_t platform_mif_isr_wpan(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "INT %pS\n", platform->wpan_handler);
	if (platform->wpan_handler != platform_mif_irq_default_handler) {
		platform->wpan_handler(irq, platform->irq_dev_wpan);
	} else {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "MIF Interrupt Handler not registered\n");
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), (0xffff << 16));
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), (0xffff << 16));
	}

	return IRQ_HANDLED;
}

/* TODO: ALIVE IRQ seems not connected ., set to if 0*/
/* #ifdef CONFIG_SCSC_ENABLE_ALIVE_IRQ */
#if 0
irqreturn_t platform_alive_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INT received\n");

	return IRQ_HANDLED;
}
#endif

irqreturn_t platform_wdog_isr(int irq, void *data)
{

	int ret = 0;
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INT received %d\n", irq);
	platform_int_debug(platform);

	if (platform->reset_request_handler != platform_mif_irq_reset_request_default_handler) {
		if (platform->boot_state == WLBT_BOOT_WAIT_CFG_REQ) {
			/* Spurious interrupt from the SOC during CFG_REQ phase, just consume it */
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Spurious wdog irq during cfg_req phase\n");
			return IRQ_HANDLED;
		} else {
			disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
			platform->reset_request_handler(irq, platform->irq_reset_request_dev);
		}
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WDOG Interrupt reset_request_handler not registered\n");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Disabling unhandled WDOG IRQ.\n");
		disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		atomic_inc(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt);
	}

	/* The wakeup source isn't cleared until WLBT is reset, so change the interrupt type to suppress this */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	if (platform->recovery_disabled && platform->recovery_disabled()) {
#else
	if (mxman_recovery_disabled()) {
#endif
		ret = regmap_update_bits(platform->pmureg, WAKEUP_INT_TYPE,
					 RESETREQ_WLBT, 0);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Set RESETREQ_WLBT wakeup interrput type to EDGE.\n");
		if (ret < 0)
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Failed to Set WAKEUP_INT_IN[RESETREQ_WLBT]: %d\n", ret);
	}

	return IRQ_HANDLED;
}

static void wlbt_regdump(struct platform_mif *platform)
{
	u32 val = 0;

	regmap_read(platform->pmureg, WLBT_STAT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STAT 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_DEBUG, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_DEBUG 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_CONFIGURATION, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_CONFIGURATION 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_STATUS, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATUS 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_STATES, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATES 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_OPTION, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_OPTION 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_CTRL_NS, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_CTRL_NS 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_CTRL_S, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_CTRL_S 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_OUT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_OUT 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_IN, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_IN 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_IN, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_IN 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_EN, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_EN 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_TYPE, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_TYPE 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_DIR, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_DIR 0x%x\n", val);

	regmap_read(platform->pmureg, SYSTEM_OUT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "SYSTEM_OUT 0x%x\n", val);

	regmap_read(platform->i3c_apm_pmic, VGPIO_TX_MONITOR, &val);
	SCSC_TAG_INFO(PLAT_MIF, "VGPIO_TX_MONITOR 0x%x\n", val);
}

/*
 * Attached array contains the replacement PMU boot code which should
 * be programmed using the CBUS during the config phase.
 */
uint32_t ka_patch[] = {
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x60008000,
	0x60000251,
	0x60000281,
	0x60000285,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x60000289,
	0x00000000,
	0x00000000,
	0x6000028d,
	0x60000291,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x8637bd05,
	0x00000000,
	0x60000100,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x47804802,
	0xff02f000,
	0x4770e010,
	0x60000261,
	0x46012000,
	0x46034602,
	0x46054604,
	0x46074606,
	0x46814680,
	0x46834682,
	0x47704684,
	0xe7fdbf30,
	0x46c0e7fc,
	0x46c0e7fa,
	0x46c0e7f8,
	0x46c0e7f6,
	0x46c0e7f4,
	0x20092381,
	0x009b4902,
	0x54c84a02,
	0x46c04770,
	0x50000000,
	0x50000204,
	0x4b074906,
	0xd0052800,
	0x7c404806,
	0xd0012800,
	0xe0002001,
	0x54c82000,
	0x46c04770,
	0x50000000,
	0x00000205,
	0x60000210,
	0xb5104b07,
	0x31fc0019,
	0x3401690c,
	0x690c1820,
	0xd4fc1a24,
	0x691933fc,
	0xd4fc1a09,
	0x46c0bd10,
	0x50000000,
	0x28003801,
	0x4770d1fc,
	0x4b0a2100,
	0x681cb510,
	0x681c7221,
	0x681c7261,
	0x681c72a1,
	0x681c72e1,
	0x681c6021,
	0x42887321,
	0x681bd002,
	0x73193101,
	0x46c0bd10,
	0x60000210,
	0x4b0a2100,
	0x681cb510,
	0x681c73a1,
	0x681c73e1,
	0x681c7421,
	0x681c7461,
	0x681c6061,
	0x428874a1,
	0x681bd002,
	0x74993101,
	0x46c0bd10,
	0x60000210,
	0x4c18b570,
	0x00212039,
	0x680b319c,
	0x43832540,
	0x43033830,
	0x0021600b,
	0x680b31a8,
	0x432b3015,
	0x21c0600b,
	0x438b6fe3,
	0x430b3940,
	0xf7ff67e3,
	0x6fe3ffa1,
	0x431d200a,
	0x250167e5,
	0xff9af7ff,
	0x6ee32104,
	0x43ab0028,
	0x6f2366e3,
	0x6723430b,
	0xff90f7ff,
	0x6f232198,
	0x6723430b,
	0x43286f60,
	0xbd706760,
	0x50000000,
	0x7013230b,
	0x47184b00,
	0x60000b39,
	0x7013230b,
	0x47184b00,
	0x6000058d,
	0x7013230b,
	0x47184b00,
	0x60000f75,
	0x2300b510,
	0x22404904,
	0xf0002005,
	0x2120feff,
	0x33fc4b02,
	0xbd106059,
	0x60000f25,
	0xe000e000,
	0x2601b570,
	0x69eb4d23,
	0x002c6a29,
	0x08db404b,
	0x42332101,
	0x2308d1f7,
	0xd01b2800,
	0x421969e9,
	0x69e9d137,
	0x61e94319,
	0x42196a21,
	0x0021d0fc,
	0x31f82001,
	0x4383680b,
	0x600b3022,
	0xff44f7ff,
	0x6ee32102,
	0x66e3438b,
	0x686134fc,
	0x400b4b12,
	0xe01e6063,
	0x421869e8,
	0x0028d01b,
	0x680330f8,
	0x6003430b,
	0x30fc0028,
	0x420b6803,
	0x2308d0fc,
	0x439969e1,
	0x6a2161e1,
	0xd1fc4219,
	0x20800021,
	0x684b31fc,
	0x43030140,
	0x2102604b,
	0x430b6ee3,
	0xbd7066e3,
	0x50000000,
	0xffffefff,
	0x230bb5f8,
	0x70134c33,
	0x4e336825,
	0x2b027cab,
	0x7babd14d,
	0x2b07b2db,
	0x2b08d035,
	0x2b01d03b,
	0x7ba3d141,
	0x2b01b2db,
	0x7c29d13d,
	0x02097be8,
	0x63214301,
	0x42196b21,
	0x0031d01b,
	0x690831fc,
	0x63616869,
	0x42196a61,
	0x6aa1d005,
	0x1a096b63,
	0x42991a1b,
	0x6b67d90d,
	0x68184b20,
	0x000b6859,
	0x21000002,
	0xf0000038,
	0x0909ff87,
	0x6ba363a1,
	0x6a616573,
	0x430b6b23,
	0x23016473,
	0x42197c69,
	0x7523d011,
	0x7be8e00f,
	0x42433801,
	0xb2c04158,
	0xff68f7ff,
	0x2101e007,
	0x404b7c63,
	0x7c607463,
	0xf7ffb2c0,
	0x2300feaf,
	0x330173ab,
	0x201074ab,
	0x6bb36bb1,
	0x009b4381,
	0x430b4003,
	0x7b2363b3,
	0x2b004a07,
	0x7ba3d104,
	0xd1012b00,
	0xfe8ef7ff,
	0x46c0bdf8,
	0x60000210,
	0x50000000,
	0x60000200,
	0x50000204,
	0x2001230b,
	0x4908b510,
	0x6acb7013,
	0x43034a07,
	0x4b0762cb,
	0x29007b19,
	0x7b9bd104,
	0xd1012b00,
	0xfe72f7ff,
	0x46c0bd10,
	0x50000000,
	0x50000204,
	0x60000210,
	0x4b0321b0,
	0x400169d8,
	0x6a1b61d9,
	0x46c04770,
	0x50000000,
	0xb5104907,
	0x2307000c,
	0x401834f8,
	0x31fc6823,
	0xb2db4303,
	0x680b6023,
	0x42984003,
	0xbd10d1fb,
	0x50000000,
	0x4c16b510,
	0x07db6a63,
	0x69e3d526,
	0xd511071b,
	0x23030020,
	0x680130dc,
	0x60014319,
	0x00190020,
	0x680330e0,
	0x2b03400b,
	0x0020d1fb,
	0x680130dc,
	0x6003404b,
	0xf7ff2007,
	0x2110ffd1,
	0x430b69e3,
	0x230161e3,
	0x43196aa1,
	0x6a6162a1,
	0xd1fc4219,
	0x6aa32101,
	0x62a3438b,
	0x46c0bd10,
	0x50000000,
	0xb5104907,
	0x2307000c,
	0x401834f8,
	0x31fc6823,
	0xb2db4383,
	0x680b6023,
	0xd1fc4203,
	0x46c0bd10,
	0x50000000,
	0x4e37b5f8,
	0xb2ff7af7,
	0xd1672f02,
	0xf7ff25e0,
	0x4934fe69,
	0x680b4c34,
	0x3301006d,
	0x2101600b,
	0x20645963,
	0x516343bb,
	0x430b5963,
	0x59615163,
	0x400b4b2e,
	0x430b2140,
	0x59635163,
	0x438b393c,
	0x59635163,
	0x438b3104,
	0xf7ff5163,
	0x5963fe05,
	0x431f2003,
	0xf7ff5167,
	0x2301fdff,
	0x21e23504,
	0x00495960,
	0xd0fa4218,
	0x58635865,
	0x075b01ed,
	0x0f9b0aad,
	0x00190068,
	0xd0022b01,
	0x418b1e59,
	0x7c334259,
	0xb2db1841,
	0x000a4f19,
	0xd00a2b00,
	0x23002182,
	0x01492000,
	0xfe64f000,
	0x60796038,
	0x48140029,
	0x2182e008,
	0x01892000,
	0xfe5af000,
	0x60796038,
	0x48100029,
	0xfdc8f000,
	0x61f021e0,
	0x00492001,
	0x43835863,
	0x6b235063,
	0x39ff39be,
	0x3901438b,
	0x6323430b,
	0x72f32300,
	0x46c0bdf8,
	0x60000210,
	0x6000020c,
	0x50000000,
	0xfffff80f,
	0x60000200,
	0x07bfa480,
	0x0f7f4900,
	0xf7ffb5f8,
	0xf7fffdf1,
	0x4d68ff7d,
	0x2b007ceb,
	0x7d2bd102,
	0xd0052b00,
	0xf7ff2001,
	0x2300fe3b,
	0x752b74eb,
	0x4c622101,
	0x00206be3,
	0x63e3438b,
	0x30d42340,
	0x43196801,
	0x20026001,
	0x43016f61,
	0x69e16761,
	0x43013044,
	0x6a2161e1,
	0xd0fc4219,
	0x20230026,
	0xfd82f7ff,
	0xf7ff2001,
	0x2101fd6b,
	0x36c00027,
	0x37c46833,
	0x6033438b,
	0x2004683b,
	0x603b438b,
	0xfd84f7ff,
	0x68332102,
	0x438b2008,
	0x683b6033,
	0x603b438b,
	0xfd7af7ff,
	0x68332110,
	0x438b2004,
	0x68296033,
	0x7b4b0026,
	0x330136cc,
	0x734bb2db,
	0x7ccb6829,
	0xb2db3301,
	0x215574cb,
	0x438b6833,
	0x68336033,
	0x438b394b,
	0xf7ff6033,
	0x682bfd5f,
	0x2b017cdb,
	0x21a0d903,
	0x438b6833,
	0x20046033,
	0xfd54f7ff,
	0x26080021,
	0x680b31c0,
	0x43b32024,
	0x0021600b,
	0x680b31c4,
	0x43832704,
	0x2101600b,
	0x38226fa3,
	0x67a3430b,
	0xfd40f7ff,
	0x6fa32002,
	0x67a34303,
	0x43396fa1,
	0x6f6367a1,
	0x6763433b,
	0x42036f63,
	0x6f63d102,
	0xe00143b3,
	0x43336f63,
	0x67632702,
	0xf7ff2002,
	0x2001fd29,
	0xfd2af7ff,
	0xf7ff2001,
	0x4c1dfd3f,
	0x6fa30038,
	0x43bb0026,
	0xf7ff67a3,
	0x2120fd1b,
	0x683336c0,
	0x438b0038,
	0xf7ff6033,
	0x2140fd13,
	0x431f6fa3,
	0x683367a7,
	0x438b0027,
	0x26016033,
	0x393c4b11,
	0x605933fc,
	0x6059310c,
	0x683b37d4,
	0x433334c4,
	0x6823603b,
	0x00303908,
	0x6023438b,
	0xfce6f7ff,
	0x68232110,
	0x6023438b,
	0x3130683b,
	0x603b438b,
	0xfd68f7ff,
	0x0030732e,
	0xbdf873ae,
	0x60000210,
	0x50000000,
	0xe000e000,
	0xf7ffb5f7,
	0x4c3dfea3,
	0x2b007ce3,
	0x2001d004,
	0xfd64f7ff,
	0x74e32300,
	0x4c392102,
	0x430b6f63,
	0x69e36763,
	0x430b3140,
	0x234061e3,
	0x40196a21,
	0x29409101,
	0x0025d1fa,
	0xf7ff2023,
	0x2001fcb3,
	0xfc9cf7ff,
	0x26022101,
	0x682b35c0,
	0x438b2004,
	0xf7ff602b,
	0x682bfcb9,
	0x43b32008,
	0xf7ff602b,
	0x2110fcb3,
	0x682b0027,
	0x438b37cc,
	0x683b602b,
	0x438b390b,
	0x603b2004,
	0xfca6f7ff,
	0x683b210a,
	0x438b2004,
	0xf7ff603b,
	0x2108fc9f,
	0x682b2704,
	0x438b0030,
	0x6fa3602b,
	0x430b3907,
	0xf7ff67a3,
	0x6fa3fc93,
	0x43332001,
	0x6fa367a3,
	0x67a3433b,
	0xfc8ef7ff,
	0x00306fa3,
	0x67a343b3,
	0xfc84f7ff,
	0x682b2120,
	0x438b0030,
	0xf7ff602b,
	0x6fa3fc7d,
	0x43339901,
	0x682b67a3,
	0x438b34d4,
	0x602b4907,
	0x7b436808,
	0xb2db3301,
	0x20017343,
	0x33fc4b05,
	0x6823605f,
	0x60234303,
	0xbdfe7308,
	0x60000210,
	0x50000000,
	0xe000e000,
	0xf7ffb570,
	0xf7fffc91,
	0x4d3afe1d,
	0x2b007d2b,
	0x2001d004,
	0xfcdef7ff,
	0x752b2300,
	0x4c362101,
	0x00206be3,
	0x63e3438b,
	0x30d42340,
	0x43196801,
	0x20446001,
	0x430169e1,
	0x6a2161e1,
	0xd0fc4219,
	0x20230026,
	0xfc2af7ff,
	0xf7ff2001,
	0x2101fc13,
	0x683336c4,
	0x438b2004,
	0xf7ff6033,
	0x2102fc31,
	0x20086833,
	0x6033438b,
	0xfc2af7ff,
	0x20506829,
	0x33017ccb,
	0x74cbb2db,
	0x33cc0023,
	0x43816819,
	0x68296019,
	0x29017cc9,
	0x6819d903,
	0x43811800,
	0x00216019,
	0x31c42024,
	0x4383680b,
	0x2104600b,
	0x430b6f63,
	0x6f636763,
	0x079b1849,
	0x6f63d402,
	0xe001438b,
	0x430b6f63,
	0x67630026,
	0xf7ff2001,
	0x2108fc1b,
	0x683336c4,
	0x438b2001,
	0xf7ff6033,
	0x2110fbe5,
	0x34d46833,
	0x6033438b,
	0x33fc4b07,
	0x68236059,
	0x438b3130,
	0xf7ff6023,
	0x2001fc63,
	0xbd7073a8,
	0x60000210,
	0x50000000,
	0xe000e000,
	0xb5f7230b,
	0x4d422101,
	0x6cee7013,
	0x4c416c2b,
	0x642b430b,
	0x2b007d63,
	0x2300d016,
	0x7563002f,
	0x69a06d2b,
	0x37fc69e1,
	0x68f99100,
	0x91011a18,
	0x4a392300,
	0xf0002100,
	0x2300fc57,
	0xf0009a00,
	0x9b01fc33,
	0x60f81818,
	0xb2f16823,
	0x68217599,
	0xb2db0a33,
	0x6a6375cb,
	0x1e594033,
	0x6b21418b,
	0x4031b2db,
	0x41811e48,
	0xb2c92001,
	0xd0274206,
	0x6a606d2f,
	0xd00f2800,
	0x1a386ae0,
	0x2001d404,
	0x40336a63,
	0xe0074003,
	0x28006b20,
	0x6ba0d004,
	0xd4011a38,
	0x65686ae0,
	0x6b04481e,
	0xd00f2c00,
	0x1b3c6b84,
	0x6b01d404,
	0x2101400e,
	0xe0074031,
	0x2c006a44,
	0x6ac4d004,
	0xd4011b3f,
	0x65686ac0,
	0x2b004c14,
	0x7b23d010,
	0xd0152900,
	0xd1052b00,
	0x2b007ba3,
	0xf7ffd102,
	0xe00ffdb3,
	0x2b007b23,
	0xf7ffd104,
	0xe009fe89,
	0xd0072900,
	0x2b007ba3,
	0xf7ffd104,
	0xe001ff05,
	0xd0f22b00,
	0x4a077b23,
	0xd1042b00,
	0x2b007ba3,
	0xf7ffd101,
	0xbdf7fb27,
	0x50000000,
	0x60000210,
	0x000f4240,
	0x50000204,
	0x4d13b570,
	0x2b027aeb,
	0x2101d021,
	0x20084c11,
	0x430b6b23,
	0x6d236323,
	0x756961ab,
	0x31c00021,
	0x4303680b,
	0x0021600b,
	0x680b31c4,
	0x43033804,
	0x600b2000,
	0xfbbef7ff,
	0x4b072100,
	0xf7ff54e1,
	0x2140fc93,
	0x430b6fe3,
	0x230267e3,
	0xbd7072eb,
	0x60000210,
	0x50000000,
	0x00000205,
	0x4b10b510,
	0x29007b19,
	0x3102d103,
	0x39017359,
	0x2100e001,
	0x7b987359,
	0xd1042800,
	0x73d83002,
	0xd0092900,
	0x2100e002,
	0xe00573d9,
	0x2b027adb,
	0xf7ffd007,
	0xe004ffb7,
	0x2b027adb,
	0xf7ffd101,
	0xbd10fcc3,
	0x60000210,
	0x4c1db5f8,
	0x00232020,
	0x681933c0,
	0x43012512,
	0x60190020,
	0x680130d4,
	0x43a92600,
	0x43293d10,
	0x21f46001,
	0x50660049,
	0x270a0021,
	0x680831cc,
	0x60084338,
	0x3f056808,
	0x60084338,
	0x68192008,
	0x60194301,
	0x38056819,
	0x60194301,
	0x300d6819,
	0x60194301,
	0x731e4b09,
	0xffaef7ff,
	0x003069e3,
	0x61e343ab,
	0xfacef7ff,
	0x200423c0,
	0x005b4904,
	0x380350c8,
	0x46c0bdf8,
	0x50000000,
	0x60000210,
	0xe000e000,
	0x4c3fb5f0,
	0x6825b085,
	0x7b2b2600,
	0xd15d2b02,
	0xb2db7a2b,
	0xd0092b01,
	0xd1532b07,
	0x38017a68,
	0x41584243,
	0xf7ffb2c0,
	0xe04bfb31,
	0xb2c97b21,
	0xd1472901,
	0x7a687aab,
	0x4303021b,
	0x6a636263,
	0xd034420b,
	0x001e4b2f,
	0x68286d1f,
	0x62a036fc,
	0x69366b20,
	0xd0054208,
	0x6aa16b60,
	0x1b891bc0,
	0xd9244288,
	0x6d1f6aa1,
	0x910069e3,
	0x4b269301,
	0x68184a26,
	0x23006859,
	0x91039002,
	0x21000038,
	0xfb0ef000,
	0x9a012300,
	0xfaeaf000,
	0x21009b00,
	0x18301b9e,
	0x9b039a02,
	0xfb02f000,
	0x62e10909,
	0x42bb6ae3,
	0x4b16d903,
	0x4b166ad9,
	0x26016559,
	0x6a584b13,
	0x43016b19,
	0x64414812,
	0x42317ae9,
	0x74ded000,
	0x722b2300,
	0x732b3301,
	0x480d2420,
	0x6b836b81,
	0x009b43a1,
	0x430b4023,
	0x2e006383,
	0xf7ffd001,
	0x4b06ff4b,
	0x7b194a09,
	0xd1042900,
	0x2b007b9b,
	0xf7ffd101,
	0xb005fa0d,
	0x46c0bdf0,
	0x60000210,
	0x50000000,
	0x60000200,
	0x000f4240,
	0x50000204,
	0x4d20b5f8,
	0x002e21c0,
	0x36d4002c,
	0x34c46833,
	0x6033430b,
	0x39b06823,
	0x2002430b,
	0xf7ff6023,
	0x2108fa21,
	0x20a06823,
	0x6023430b,
	0x33cc002b,
	0x27016819,
	0x60194301,
	0x38506819,
	0x60194301,
	0x4b112100,
	0xf7ff7399,
	0x2104feeb,
	0x68232002,
	0x6023430b,
	0x433b6823,
	0x68236023,
	0x60234303,
	0x30066833,
	0x60334303,
	0x200069eb,
	0x61eb438b,
	0xfa14f7ff,
	0x201023c0,
	0x005b4904,
	0x003850c8,
	0x46c0bdf8,
	0x50000000,
	0x60000210,
	0xe000e000,
	0xb510230b,
	0x4b0e7013,
	0x07496bd9,
	0x2020d517,
	0x31fc490c,
	0x6bd96048,
	0x4301381f,
	0x210263d9,
	0x42086bd8,
	0xf7ffd0fc,
	0x4b07ffa3,
	0x7b194a07,
	0xd1042900,
	0x2b007b9b,
	0xf7ffd101,
	0xbd10f999,
	0x50000000,
	0xe000e000,
	0x60000210,
	0x50000204,
	0x4d2eb570,
	0x33fc002b,
	0x23016998,
	0x28043801,
	0x4c2bd838,
	0xf994f000,
	0x22110a03,
	0x7b21002b,
	0x29002301,
	0xf7ffd12e,
	0xe028fcc9,
	0x23017ba1,
	0xd1272900,
	0xfd46f7ff,
	0x7b23e021,
	0xd1052b00,
	0x2b007ba3,
	0xf7ffd102,
	0xe018fbdd,
	0x2b007b23,
	0x7ba3d0e9,
	0xd0ed2b00,
	0xe0132300,
	0x7b262301,
	0x429eb2f6,
	0xf7ffd10e,
	0x74e6fe8f,
	0x2301e007,
	0xb2f67ba6,
	0xd105429e,
	0xff52f7ff,
	0x23017526,
	0xb2c34058,
	0x31fc0029,
	0x232061cb,
	0x431969e9,
	0x6a2961e9,
	0xd0fc4219,
	0x69eb2120,
	0x61eb438b,
	0xfe4ef7ff,
	0x4a074b06,
	0x29007b19,
	0x7b9bd104,
	0xd1012b00,
	0xf934f7ff,
	0x46c0bd70,
	0x50000000,
	0x60000210,
	0x50000204,
	0xb51021ff,
	0x22404b04,
	0x20067159,
	0x49032300,
	0xf8d4f000,
	0x46c0bd10,
	0x60000210,
	0x600003e1,
	0xb672b5f7,
	0x210b2381,
	0x009b483d,
	0x54c14a3d,
	0x8f4ff3bf,
	0x4f3d4b3c,
	0x4b3d6899,
	0x50f94c3d,
	0x009b23c2,
	0x23009300,
	0x0018493b,
	0xf000220e,
	0x9400f8e3,
	0x49392300,
	0x22072001,
	0xf8dcf000,
	0x26012398,
	0x9300015b,
	0x23004935,
	0x221a2002,
	0xf8d2f000,
	0x23009400,
	0x20034932,
	0xf000221a,
	0x2001f8cb,
	0xf882f000,
	0x4b304c2f,
	0x00202100,
	0x50fe223c,
	0xfabcf000,
	0x21004d2d,
	0x22400028,
	0xfab6f000,
	0x25006025,
	0x735d6823,
	0x74dd6823,
	0x752674e6,
	0xf7ff7466,
	0x002bffa3,
	0x200d4925,
	0xf0002240,
	0x002bf87b,
	0x49230028,
	0xf0002240,
	0x002bf875,
	0x20024921,
	0xf0002240,
	0x23c0f86f,
	0x005b2004,
	0x491e50f8,
	0x2240002b,
	0xf866f000,
	0x231021c0,
	0x507b0049,
	0x491a200c,
	0x2240002b,
	0xf85cf000,
	0x021b2380,
	0x4b0561e3,
	0xf7ff601e,
	0x4a04fd85,
	0xf8a0f7ff,
	0xbf30b662,
	0x46c0e7fd,
	0x50000000,
	0x50000204,
	0x60000200,
	0xe000e000,
	0x00000d08,
	0x00001305,
	0x60000000,
	0x60000000,
	0x50000000,
	0x58000000,
	0x60000210,
	0x00000d94,
	0x60000000,
	0x600005f9,
	0x600003d5,
	0x60000d81,
	0x600004a9,
	0x600003c9,
	0xb5100003,
	0x4c053310,
	0x5119009b,
	0x18184b04,
	0x00db2380,
	0x700218c0,
	0x46c0bd10,
	0x60000100,
	0xe000e000,
	0x0004b513,
	0xf0002004,
	0x2c00f85d,
	0x4b05d101,
	0x4b05e000,
	0x22079300,
	0x49042300,
	0xf0002004,
	0xbd13f835,
	0x00001308,
	0x00001008,
	0x60000100,
	0x0004b5f8,
	0x000e2000,
	0x001d0017,
	0xffe0f7ff,
	0x4a100023,
	0x009b3310,
	0x2280509e,
	0x00d24e0e,
	0x189b1933,
	0x701f2001,
	0xffd2f7ff,
	0x09630032,
	0xd0072d00,
	0x2001211f,
	0x40884021,
	0x31a00019,
	0x51880089,
	0x400c211f,
	0x40a1391e,
	0x009b3340,
	0xbdf850d1,
	0x60000100,
	0xe000e000,
	0x2410b510,
	0x43084321,
	0x490a4c09,
	0xf3bf5060,
	0x20018f4f,
	0x99024082,
	0x0409021b,
	0x430b4301,
	0x23da431a,
	0x50e2011b,
	0x8f6ff3bf,
	0x46c0bd10,
	0xe000e000,
	0x00000d9c,
	0x4a074b06,
	0x50984907,
	0x505a2200,
	0x8f4ff3bf,
	0x505a3104,
	0x8f6ff3bf,
	0x46c04770,
	0xe000e000,
	0x00000d98,
	0x00000d9c,
	0x4671b402,
	0x00490849,
	0x00495c09,
	0xbc02448e,
	0x46c04770,
	0x08432200,
	0xd374428b,
	0x428b0903,
	0x0a03d35f,
	0xd344428b,
	0x428b0b03,
	0x0c03d328,
	0xd30d428b,
	0x020922ff,
	0x0c03ba12,
	0xd302428b,
	0x02091212,
	0x0b03d065,
	0xd319428b,
	0x0a09e000,
	0x428b0bc3,
	0x03cbd301,
	0x41521ac0,
	0x428b0b83,
	0x038bd301,
	0x41521ac0,
	0x428b0b43,
	0x034bd301,
	0x41521ac0,
	0x428b0b03,
	0x030bd301,
	0x41521ac0,
	0x428b0ac3,
	0x02cbd301,
	0x41521ac0,
	0x428b0a83,
	0x028bd301,
	0x41521ac0,
	0x428b0a43,
	0x024bd301,
	0x41521ac0,
	0x428b0a03,
	0x020bd301,
	0x41521ac0,
	0x09c3d2cd,
	0xd301428b,
	0x1ac001cb,
	0x09834152,
	0xd301428b,
	0x1ac0018b,
	0x09434152,
	0xd301428b,
	0x1ac0014b,
	0x09034152,
	0xd301428b,
	0x1ac0010b,
	0x08c34152,
	0xd301428b,
	0x1ac000cb,
	0x08834152,
	0xd301428b,
	0x1ac0008b,
	0x08434152,
	0xd301428b,
	0x1ac0004b,
	0x1a414152,
	0x4601d200,
	0x46104152,
	0xe7ff4770,
	0x2000b501,
	0xf806f000,
	0x46c0bd02,
	0xd0f72900,
	0x4770e776,
	0x46c04770,
	0xd1112b00,
	0xd10f2a00,
	0xd1002900,
	0xd0022800,
	0x43c92100,
	0xb4071c08,
	0xa1024802,
	0x90021840,
	0x46c0bd03,
	0xffffffd9,
	0x4668b403,
	0x9802b501,
	0xf832f000,
	0x469e9b01,
	0xbc0cb002,
	0x46c04770,
	0x464fb5f0,
	0xb4c04646,
	0x0c360416,
	0x00334699,
	0x0c2c0405,
	0x0c150c07,
	0x437e4363,
	0x4365436f,
	0x19ad0c1c,
	0x469c1964,
	0xd90342a6,
	0x025b2380,
	0x44474698,
	0x0c254663,
	0x041d19ef,
	0x434a464b,
	0x0c2d4343,
	0x19640424,
	0x19c91899,
	0xbc0c0020,
	0x46994690,
	0x46c0bdf0,
	0x464db5f0,
	0x46444656,
	0xb4f0465f,
	0xb0834692,
	0x000d0004,
	0x428b4699,
	0xd02cd82f,
	0x46504649,
	0xf8aef000,
	0x00060029,
	0xf0000020,
	0x1a33f8a9,
	0x3b204698,
	0xd500469b,
	0x4653e074,
	0x4093465a,
	0x4653001f,
	0x40934642,
	0x42af001e,
	0xd026d829,
	0x1ba4465b,
	0x2b0041bd,
	0xe079da00,
	0x23002200,
	0x93019200,
	0x465a2301,
	0x93014093,
	0x46422301,
	0x93004093,
	0x4282e019,
	0x2200d9d0,
	0x92002300,
	0x9b0c9301,
	0xd0012b00,
	0x605d601c,
	0x99019800,
	0xbc3cb003,
	0x46994690,
	0x46ab46a2,
	0x42a3bdf0,
	0x2200d9d6,
	0x92002300,
	0x46439301,
	0xd0e82b00,
	0x087207fb,
	0x4646431a,
	0xe00e087b,
	0xd10142ab,
	0xd80c42a2,
	0x419d1aa4,
	0x19242001,
	0x2100416d,
	0x18243e01,
	0x2e00414d,
	0x42abd006,
	0x3e01d9ee,
	0x416d1924,
	0xd1f82e00,
	0x9800465b,
	0x19009901,
	0x2b004169,
	0x002bdb22,
	0x40d3465a,
	0x4644002a,
	0x001c40e2,
	0x0015465b,
	0xdb2c2b00,
	0x409e0026,
	0x00260033,
	0x40be4647,
	0x1a800032,
	0x90004199,
	0xe7ae9101,
	0x23204642,
	0x46521a9b,
	0x464140da,
	0x464a0013,
	0x0017408a,
	0xe782431f,
	0x23204642,
	0x002a1a9b,
	0x409a4646,
	0x40f30023,
	0xe7d54313,
	0x23204642,
	0x1a9b2100,
	0x91002200,
	0x22019201,
	0x920140da,
	0x4642e782,
	0x00262320,
	0x40de1a9b,
	0x46b4002f,
	0x46664097,
	0x4333003b,
	0x46c0e7c9,
	0x2900b510,
	0xf000d103,
	0x3020f807,
	0x1c08e002,
	0xf802f000,
	0x46c0bd10,
	0x2301211c,
	0x4298041b,
	0x0c00d301,
	0x0a1b3910,
	0xd3014298,
	0x39080a00,
	0x4298091b,
	0x0900d301,
	0xa2023904,
	0x18405c10,
	0x46c04770,
	0x02020304,
	0x01010101,
	0x00000000,
	0x00000000,
	0x18820003,
	0xd0024293,
	0x33017019,
	0x4770e7fa,
	0x7ffffe1c,
	0x00000001
};

void platform_cfg_req_irq_clean_pending(struct platform_mif *platform)
{
	int irq;
	int ret;
	bool pending = 0;
	char *irqs_name = {"CFG_REQ"};

	irq = platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num;
	ret = irq_get_irqchip_state(irq, IRQCHIP_STATE_PENDING, &pending);

	if (!ret) {
		if(pending == 1){
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "IRQCHIP_STATE %d(%s): pending %d",
							  irq, irqs_name, pending);
			pending = 0;
			ret = irq_set_irqchip_state(irq, IRQCHIP_STATE_PENDING, pending);
		}
	}
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static void platform_mif_set_memlog_baaw(struct platform_mif *platform, dma_addr_t paddr)
{
	#define MEMLOG_CHECK(x) do { \
		int retval = (x); \
		if (retval < 0) {\
			pr_err("%s failed at L%d", __FUNCTION__, __LINE__); \
			goto baaw1_error; \
		} \
	} while (0)

	MEMLOG_CHECK(regmap_write(platform->dbus_baaw, 0x10, WLBT_DBUS_BAAW_1_START >> 12));
	MEMLOG_CHECK(regmap_write(platform->dbus_baaw, 0x14, WLBT_DBUS_BAAW_1_END >> 12));
	MEMLOG_CHECK(regmap_write(platform->dbus_baaw, 0x18, paddr >> 12));
	MEMLOG_CHECK(regmap_write(platform->dbus_baaw, 0x1C, WLBT_BAAW_ACCESS_CTRL));

baaw1_error:
	return;
}
#endif

/* WARNING! this function might be
 * running in IRQ context if CONFIG_SCSC_WLBT_CFG_REQ_WQ is not defined */
void platform_set_wlbt_regs(struct platform_mif *platform)
{
	u64 ret64 = 0;
	const u64 EXYNOS_WLBT = 0x1;
	/*s32 ret = 0;*/
	unsigned int ka_addr = PMU_BOOT_RAM_START;
	uint32_t *ka_patch_addr;
	uint32_t *ka_patch_addr_end;
	size_t ka_patch_len;
	unsigned int id;
	unsigned int val;
	int i;
	if (platform->ka_patch_fw && !fw_compiled_in_kernel) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch present in FW image\n");
		ka_patch_addr = platform->ka_patch_fw;
		ka_patch_addr_end = ka_patch_addr + (platform->ka_patch_len / sizeof(uint32_t));
		ka_patch_len = platform->ka_patch_len;
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch not present in FW image. ARRAY_SIZE %d Use default\n", ARRAY_SIZE(ka_patch));
		ka_patch_addr = &ka_patch[0];
		ka_patch_addr_end = ka_patch_addr + ARRAY_SIZE(ka_patch);
		ka_patch_len = ARRAY_SIZE(ka_patch);
	}

#define CHECK(x) do { \
	int retval = (x); \
	if (retval < 0) {\
		pr_err("%s failed at L%d", __FUNCTION__, __LINE__); \
		goto cfg_error; \
	} \
} while (0)

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	/* Set TZPC to non-secure mode */
	/* TODO : check the tz registers */
	ret64 = exynos_smc(SMC_CMD_CONN_IF, (EXYNOS_WLBT << 32) | EXYNOS_SET_CONN_TZPC, 0, 0);
	if (ret64)
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
		"Failed to set TZPC to non-secure mode: %llu\n", ret64);
	else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"SMC_CMD_CONN_IF run successfully : %llu\n", ret64);

	/* Remapping boot address for WLBT processor (0x8000_0000) */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_REMAP begin\n");
	CHECK(regmap_write(platform->wlbt_remap, WLAN_PROC_RMP_BOOT, (WLBT_DBUS_BAAW_0_START + platform->remap_addr_wlan) >> 12));
	regmap_read(platform->wlbt_remap, WLAN_PROC_RMP_BOOT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLAN_REMAP: 0x%x.\n", val);

	CHECK(regmap_write(platform->wlbt_remap, WPAN_PROC_RMP_BOOT, (WLBT_DBUS_BAAW_0_START + platform->remap_addr_wpan)  >> 12));
	regmap_read(platform->wlbt_remap, WPAN_PROC_RMP_BOOT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WPAN_REMAP: 0x%x.\n", val);

	/* CHIP_VERSION_ID - update with AP view of SOC revision */
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "CHIP_VERSION_ID begin\n");
	regmap_read(platform->wlbt_remap, CHIP_VERSION_ID_OFFSET, &id);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Read CHIP_VERSION_ID 0x%x\n", id);
	id &= ~(CHIP_VERSION_ID_IP_MAJOR | CHIP_VERSION_ID_IP_MINOR);
	id |= ((exynos_soc_info.revision << CHIP_VERSION_ID_IP_MINOR_SHIFT) & (CHIP_VERSION_ID_IP_MAJOR | CHIP_VERSION_ID_IP_MINOR));
	CHECK(regmap_write(platform->wlbt_remap, CHIP_VERSION_ID_OFFSET, id));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated CHIP_VERSION_ID 0x%x\n", id);

	/* DBUS_BAAW regions */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "DBUS_BAAW begin\n");

	/* Shared DRAM mapping. The destination address is the location reserved
	 * by the kernel.
	 */
	CHECK(regmap_write(platform->dbus_baaw, BAAW_D_WLBT_START, WLBT_DBUS_BAAW_0_START >> 12));
	CHECK(regmap_write(platform->dbus_baaw, BAAW_D_WLBT_END, WLBT_DBUS_BAAW_0_END >> 12));
	CHECK(regmap_write(platform->dbus_baaw, BAAW_D_WLBT_REMAP, platform->mem_start >> 12));
	/* TODO,, we may only need to set bit 0 */
	CHECK(regmap_write(platform->dbus_baaw, BAAW_D_WLBT_INIT_DONE, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->dbus_baaw, BAAW_D_WLBT_START, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_START: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, BAAW_D_WLBT_END, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_END: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, BAAW_D_WLBT_REMAP, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_REMAP: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, BAAW_D_WLBT_INIT_DONE, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_ENABLE_DONE: 0x%x.\n", val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "DBUS_BAAW end\n");

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	if (platform->paddr > 0)
		platform_mif_set_memlog_baaw(platform, platform->paddr);
#endif

	/* PBUS_BAAW regions */
	/* ref wlbt_if_S5E981.c, updated for MX450 memory map */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "CBUS_BAAW begin\n");

	/* Range for CP2WLBT mailbox */
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_0, WLBT_CBUS_BAAW_0_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_0, WLBT_CBUS_BAAW_0_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_0, WLBT_MAILBOX_CP_WLBT_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_0, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_0, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_0_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_0, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_0_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_0, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_0_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_0, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_0_ENABLE_DONE: 0x%x.\n", val);

	/* Range includes AP2WLBT,APM2WLBT,GNSS2WLBT mailboxes etc. */
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_1, WLBT_CBUS_BAAW_1_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_1, WLBT_CBUS_BAAW_1_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_1, WLBT_MAILBOX_WLBT_CHUB_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_1, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_1, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_1_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_1, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_1_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_1, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_1_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_1, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_1_ENABLE_DONE: 0x%x.\n", val);

	/* GPIO_CMGP_BASE */
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_2, WLBT_CBUS_BAAW_2_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_2, WLBT_CBUS_BAAW_2_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_2, WLBT_GPIO_CMGP_BASE	>> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_2, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_2, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_2_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_2, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_2_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_2, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_2_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_2, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_2_ENABLE_DONE: 0x%x.\n", val);

	/* SYSREG_CMGP2CP_BASE */
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_3, WLBT_CBUS_BAAW_3_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_3, WLBT_CBUS_BAAW_3_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_3, WLBT_SYSREG_CMGP2APM_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_3, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_3, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_3_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_3, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_3_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_3, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_3_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_3, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_3_ENABLE_DONE: 0x%x.\n", val);

	/* SYSREG_CMGP2WLBT_BASE */
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_4, WLBT_CBUS_BAAW_4_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_4, WLBT_CBUS_BAAW_4_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_4, WLBT_SYSREG_CMGP2WLBT_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_4, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_4, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_4_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_4, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_4_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_4, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_4_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_4, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_4_ENABLE_DONE: 0x%x.\n", val);

	/* WLBT_SHUB_USICHUB0_BASE */
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_5, WLBT_CBUS_BAAW_5_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_5, WLBT_CBUS_BAAW_5_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_5, WLBT_SHUB_USICHUB0_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_5, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_5, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_5_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_5, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_5_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_5, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_5_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_5, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_5_ENABLE_DONE: 0x%x.\n", val);
#if 0
	/* WLBT_SHUB_BASE */
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_6, WLBT_CBUS_BAAW_6_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_6, WLBT_CBUS_BAAW_6_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_6, WLBT_SHUB_BASE >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_6, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_6, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_6_START: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_6, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_6_END: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_6, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_6_REMAP: 0x%x.\n", val);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_6, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_PBUS_BAAW_6_ENABLE_DONE: 0x%x.\n", val);
#endif

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "CBUS_BAAW end\n");

	/* PMU boot patch */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "AP accesses KARAM\n");
	CHECK(regmap_write(platform->boot_cfg, PMU_BOOT, PMU_BOOT_AP_ACC));
	regmap_read(platform->boot_cfg, PMU_BOOT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated BOOT_SOURCE: 0x%x\n", val);
	i = 0;
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "KA patch start\n");
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch_addr : 0x%x\n", ka_patch_addr);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch_addr_end: 0x%x\n", ka_patch_addr_end);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch_len: 0x%x\n", ka_patch_len);

	while (ka_patch_addr < ka_patch_addr_end) {
		CHECK(regmap_write(platform->boot_cfg, ka_addr, *ka_patch_addr));
		//SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "i : %d, ka_patch %p ka_patch : 0x%x\n", i, ka_patch_addr, *ka_patch_addr);
		ka_addr += (unsigned int)sizeof(unsigned int);
		ka_patch_addr++;
		i++;
	}
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "KA patch done\n");

	/* Notify PMU of configuration done */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT PMU accesses KARAM\n");
	CHECK(regmap_write(platform->boot_cfg, PMU_BOOT, PMU_BOOT_PMU_ACC));
	regmap_read(platform->boot_cfg, PMU_BOOT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated BOOT_SOURCE: 0x%x\n", val);

	/* WLBT FW could panic as soon as CFG_ACK is set, so change state.
	 * This allows early FW panic to be dumped.
	 */
	platform->boot_state = WLBT_BOOT_ACK_CFG_REQ;

	/* BOOT_CFG_ACK */
	CHECK(regmap_write(platform->boot_cfg, PMU_BOOT_ACK, PMU_BOOT_COMPLETE));
	regmap_read(platform->boot_cfg, PMU_BOOT_ACK, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated BOOT_CFG_ACK: 0x%x\n", val);

	/* Mark as CFQ_REQ handled, so boot may continue */
	platform->boot_state = WLBT_BOOT_CFG_DONE;
	goto done;

cfg_error:
	platform->boot_state = WLBT_BOOT_CFG_ERROR;
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "ERROR: WLBT Config failed. WLBT will not work\n");
done:
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"Complete CFG_ACK\n");
	complete(&platform->cfg_ack);
	return;
}

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
void platform_cfg_req_wq(struct work_struct *data)
{
	struct platform_mif *platform = container_of(data, struct platform_mif, cfgreq_wq);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "\n");
	platform_set_wlbt_regs(platform);
}
#endif

irqreturn_t platform_cfg_req_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INT received, boot_state = %u\n", platform->boot_state);
#if 0
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "disable_irq\n");

	/* mask the irq */
	disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);
	/* Was the CFG_REQ irq received from WLBT before we expected it?
	 * Typically this indicates an issue returning WLBT HW to reset.
	 */
	if (platform->boot_state != WLBT_BOOT_WAIT_CFG_REQ) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Spurious CFG_REQ IRQ from WLBT!\n");
		wlbt_regdump(platform);

		//reset_failed = true; /* prevent further interaction with HW */

		goto cfg_error;
	}
#endif
	if (platform->boot_state == WLBT_BOOT_WAIT_CFG_REQ) {
#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
		/* it is executed on process context. */
		queue_work(platform->cfgreq_workq, &platform->cfgreq_wq);
#else
		/* it is executed on interrupt context. */
		platform_set_wlbt_regs(platform);
#endif
	} else {
		/* platform->boot_state = WLBT_BOOT_CFG_DONE; */
		if (platform->pmu_handler != platform_mif_irq_default_handler)
			platform->pmu_handler(irq, platform->irq_dev_pmu);
		else
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "MIF PMU Interrupt Handler not registered\n");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated BOOT_CFG_ACK\n");
		regmap_write(platform->boot_cfg, PMU_BOOT_ACK, PMU_BOOT_COMPLETE);
	}
#if 0
cfg_error:
#endif
	return IRQ_HANDLED;
}

static void platform_mif_unregister_irq(struct platform_mif *platform)
{
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering IRQs\n");

#ifdef CONFIG_SCSC_QOS
	/* clear affinity mask */
	irq_set_affinity_hint(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, NULL);
#endif
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num, platform);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform);
	/* Reset irq_disabled_cnt for WDOG IRQ since the IRQ itself is here unregistered and disabled */
	atomic_set(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt, 0);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform);
}

static int platform_mif_register_irq(struct platform_mif *platform)
{
	int err;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering IRQs\n");

	/* Register MBOX irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering MBOX irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform->wlbt_irq[PLATFORM_MIF_MBOX].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform_mif_isr,
			       platform->wlbt_irq[PLATFORM_MIF_MBOX].flags, DRV_NAME, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register MBOX handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Register MBOX irq WPAN */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering MBOX WPAN irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num, platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num, platform_mif_isr_wpan,
			       platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].flags, DRV_NAME, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register MBOX handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Register WDOG irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering WDOG irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform->wlbt_irq[PLATFORM_MIF_WDOG].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform_wdog_isr,
			       platform->wlbt_irq[PLATFORM_MIF_WDOG].flags, DRV_NAME, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register WDOG handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Mark as WLBT in reset before enabling IRQ to guard against spurious IRQ */
	platform->boot_state = WLBT_BOOT_IN_RESET;
	smp_wmb(); /* commit before irq */

	/* Register WB2AP_CFG_REQ irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering CFG_REQ irq: %d flag 0x%x\n",
		 platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].flags);

	/* clean CFG_REQ PENDING interrupt. */
	platform_cfg_req_irq_clean_pending(platform);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform_cfg_req_isr,
			       platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].flags, DRV_NAME, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register CFG_REQ handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Leave disabled until ready to handle */
	disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);

	return 0;
}

static void platform_mif_destroy(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform_mif_unregister_irq(platform);
}

static char *platform_mif_get_uid(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
	return "0";
}


static int platform_mif_start_wait_for_cfg_ack_completion(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* done as part of platform_mif_pmu_reset_release() init_done sequence */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "start wait for completion\n");
	/* At this point WLBT should assert the CFG_REQ IRQ, so wait for it */
	if (wait_for_completion_timeout(&platform->cfg_ack, WLBT_BOOT_TIMEOUT) == 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Timeout waiting for CFG_REQ IRQ\n");
		wlbt_regdump(platform);
		return -ETIMEDOUT;
	}

	/* only continue if CFG_REQ IRQ configured WLBT/PMU correctly */
	if (platform->boot_state == WLBT_BOOT_CFG_ERROR) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "CFG_REQ failed to configure WLBT.\n");
		return -EIO;
	}

	return 0;
}
#if 0
static int platform_mif_pmu_reset_release(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	int		ret = 0;
	u32		val = 0;
	u32		v = 0;
	unsigned long	timeout;
	unsigned int wait_count = 0;

	/* We're now ready for the IRQ */
	platform->boot_state = WLBT_BOOT_WAIT_CFG_REQ;
	smp_wmb(); /* commit before irq */

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "on boot_state = WLBT_BOOT_WAIT_CFG_REQ\n");

	/* INIT SEQUENCE - First WLBT boot only, but we use all the time.
	 * Cold reset wrt. AP power sequencer, cold reset for WLBT
	 */

	/* init sequence
	 WLBT_CTRL_S[3] = 1
	 WLBT_PWR_REQ_HW_SEL[0] = 1
	 WLBT_INT_EN[2] = 0
	 WLBT_INT_EN[3] = 0
	 DELAY 3
	 SYSTEM_OUT[set-bit-atomic] = 0x1B
	 read-till: WAIT VGPIO_TX_MONITOR[12] = 1
	 DELAY 0X3E8 (1000)
	 WLBT_CONFIGURATION[0] = 1
	 read-till: WLBT_STATUS[0] = 1
	 read-till: WLBT_IN[4] = 1
	 read-till: WLBT_CTRL_NS[8] = 0
	 read-till: WLBT_CTRL_NS[7] = 1
	 */

	/* reset_release
	 WLBT_PWR_REQ_HW_SEL[0] = 1
	 WLBT_INT_EN[2] = 0
	 WLBT_INT_EN[3] = 0
	 DELAY 3
	 SYSTEM_OUT[set-bit-atomic] = 0x1B
	 read-till: WAIT VGPIO_TX_MONITOR[12] = 1
	 DELAY 0X3E8 (1000)
	 WLBT_CONFIGURATION[0] = 1
	 read-till: WLBT_STATUS[0] = 1
	 read-till: WLBT_IN[4] = 1
	 read-till: WLBT_CTRL_NS[8] = 0
	 read-till: WLBT_CTRL_NS[7] = 1
	 */

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "init\n");

	if (!init_done) {
		/* WLBT_CTRL_S[WLBT_START] = 1 enable */
		ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_S,
				WLBT_START, WLBT_START);
		if (ret < 0) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
					"Failed to update WLBT_CTRL_S[WLBT_START]: %d\n", ret);
			goto done;
		}
		regmap_read(platform->pmureg, WLBT_CTRL_S, &val);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			"updated successfully WLBT_CTRL_S[WLBT_START]: 0x%x\n", val);
	}

	/* WLBT_PWR_REQ_HW_SEL[SELECT] = 1 */
	ret = regmap_update_bits(platform->pmureg, WLBT_PWR_REQ_HW_SEL,
			SELECT, SELECT);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Failed to update WLBT_PWR_REQ_HW_SEL[SELECT]: %d\n", ret);
		goto done;
	}
	regmap_read(platform->pmureg, WLBT_PWR_REQ_HW_SEL, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_PWR_REQ_HW_SEL[SELECT]: 0x%x\n", val);


	/* WLBT_INT_EN[PWR_REQ_R] = 0 */
	ret = regmap_update_bits(platform->pmureg, WLBT_INT_EN,
			PWR_REQ_R, 0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Failed to update WLBT_INT_EN[PWR_REQ_R] %d\n", ret);
		goto done;
	}
	regmap_read(platform->pmureg, WLBT_INT_EN, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully : WLBT_INT_EN[PWR_REQ_R] = 0, 0x%x\n", val);

	/* WLBT_INT_EN[PWR_REQ_F] = 0 */
	ret = regmap_update_bits(platform->pmureg, WLBT_INT_EN,
			PWR_REQ_F, 0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Failed to update WLBT_INT_EN[PWR_REQ_F] %d\n", ret);
		goto done;
	}
	regmap_read(platform->pmureg, WLBT_INT_EN, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully : WLBT_INT_EN[PWR_REQ_F] = 0, 0x%x\n", val);

	/* delay 3 */
	udelay(3);

	/* Send SYSTEM_OUT_ATOMIC_CMD command  to update atomically bit PWRRGTON_WLBT_CMD */
	ret = regmap_update_bits(platform->pmureg, SYSTEM_OUT_ATOMIC_CMD,
			PWRRGTON_WLBT_CMD, PWRRGTON_WLBT_CMD);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update SYSTEM_OUT_ATOMIC_CMD: %d\n", ret);
		goto done;
	}
	regmap_read(platform->pmureg, SYSTEM_OUT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully SYSTEM_OUT[PWRRGTON_WLBT]: 0x%x\n", val);

	/* VGPIO_TX_MONITOR[VGPIO_TX_MON_BIT12] = 0x1 */
	timeout = jiffies + msecs_to_jiffies(500);
	do {
		regmap_read(platform->pmureg, VGPIO_TX_MONITOR, &val);
		val &= (u32)VGPIO_TX_MON_BIT12;
		if (val) {
			SCSC_TAG_INFO(PLAT_MIF, "read VGPIO_TX_MONITOR 0x%x\n", val);
			break;
		}
	} while (time_before(jiffies, timeout));

	if (!val) {
		regmap_read(platform->pmureg, VGPIO_TX_MONITOR, &val);
		SCSC_TAG_INFO(PLAT_MIF, "timeout waiting for VGPIO_TX_MONITOR time-out: "
					"VGPIO_TX_MONITOR 0x%x\n", val);

		ret = -ETIME;
		goto done;
	}

	/* delay 0x3E8 */
	udelay(1000);

	/* WLBT_CONFIGURATION[LOCAL_PWR_CFG] = 1 Power On */
	ret = regmap_update_bits(platform->pmureg, WLBT_CONFIGURATION,
			LOCAL_PWR_CFG, LOCAL_PWR_CFG);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WLBT_CONFIGURATION[LOCAL_PWR_CFG]: %d\n", ret);
		goto done;
	}
	regmap_read(platform->pmureg, WLBT_CONFIGURATION, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CONFIGURATION[PWRRGTON_CON]: 0x%x\n", val);

	wait_count = 0;
	while(1) {
		regmap_read(platform->pmureg, WLBT_STATUS, &val);
		val &= (u32)WLBT_STATUS_BIT0;

		if (val) {
			/* Power On complete */
			SCSC_TAG_INFO(PLAT_MIF, "Power On complete: WLBT_STATUS 0x%x\n", val);
			/* re affirming power on by reading WLBT_STATES */
			/* STATES[7:0] = 0x10 for Power Up */
			regmap_read(platform->pmureg, WLBT_STATES, &v);
			SCSC_TAG_INFO(PLAT_MIF, "Power On complete: WLBT_STATES 0x%x\n", v);

			if (force_to_wlbt_status_timeout) {
				SCSC_TAG_INFO(PLAT_MIF, "Force not to break to check timeout.\n", v);
				val = 0;
			}
			else {
				break;
			}
		}

		if( wait_count == wlbt_status_timeout_value)
			break;
		else {
			/* It waits for 1ms */
			udelay(1000);
			wait_count++;
		}
	}

	if (!val) {
		regmap_read(platform->pmureg, WLBT_STATUS, &val);
		regmap_read(platform->pmureg, WLBT_STATES, &v);
		SCSC_TAG_INFO(PLAT_MIF, "timeout waiting for power on time-out: "
					"WLBT_STATUS 0x%x, WLBT_STATES 0x%x\n", val, v);
		wlbt_regdump(platform);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && defined(GO_S2D_ID)
		if (enable_scandump_wlbt_status_timeout) {
			SCSC_TAG_ERR(PLAT_MIF, "Triggers SCAN2DRAM");
			dbg_snapshot_do_dpm_policy(GO_S2D_ID);
		}
#endif
		ret = -ETIME;
		goto done;

	}

	/* wait for WLBT_IN[BUS_READY] = 1 for BUS READY state */
	timeout = jiffies + msecs_to_jiffies(500);
	do {
		regmap_read(platform->pmureg, WLBT_IN, &val);
		val &= (u32)BUS_READY;
		if (val) {
			/* BUS ready indication signal -> 1: BUS READY state */
			SCSC_TAG_INFO(PLAT_MIF, "Bus Ready: WLBT_IN 0x%x\n", val);

			/* OK to break */
			break;
		}
	} while (time_before(jiffies, timeout));

	if (!val) {
		regmap_read(platform->pmureg, WLBT_IN, &val);
		SCSC_TAG_INFO(PLAT_MIF, "timeout waiting for Bus Ready: WLBT_IN 0x%x\n", val);
		ret = -ETIME;
		goto done;
	}

	/* WLBT_CTRL_NS[WLBT_ACTIVE_CLR]  = 0 */
	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_NS, WLBT_ACTIVE_CLR, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_CLR]: %d\n", ret);
	}
	regmap_read(platform->pmureg, WLBT_CTRL_NS, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CTRL_NS[WLBT_ACTIVE_CLR]: 0x%x\n", val);

	/* WLBT_CTRL_NS[WLBT_ACTIVE_EN] = 1 */
	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_NS, WLBT_ACTIVE_EN, WLBT_ACTIVE_EN);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_EN]: %d\n", ret);
	}
	regmap_read(platform->pmureg, WLBT_CTRL_NS, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CTRL_NS[WLBT_ACTIVE_EN]: 0x%x\n", val);

	/* Now handle the CFG_REQ IRQ */
	enable_irq(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);

	/* Wait for CFG_ACK completion */
	ret = platform_mif_start_wait_for_cfg_ack_completion(interface);
done:
	return ret;
}
#else
static int platform_mif_pmu_reset_release(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
#ifndef CONFIG_WLBT_AUTOGEN_PMUCAL
	u32		val = 0;
	u32		v = 0;
	unsigned long	timeout;
	unsigned int wait_count = 0;
#endif
	int		ret = 0;
	/* We're now ready for the IRQ */
	platform->boot_state = WLBT_BOOT_WAIT_CFG_REQ;
	smp_wmb(); /* commit before irq */

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "on boot_state = WLBT_BOOT_WAIT_CFG_REQ\n");
	if(!init_done){
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "init\n");
		ret = pmu_cal_progress(platform, pmucal_wlbt.init, pmucal_wlbt.init_size);
		if(ret<0) goto done;
		init_done = 1;
	}else{
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "release\n");
		ret =pmu_cal_progress(platform, pmucal_wlbt.reset_release, pmucal_wlbt.reset_release_size);
		if(ret<0) goto done;
	}

	/* Now handle the CFG_REQ IRQ */
	enable_irq(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);

	/* Wait for CFG_ACK completion */
	ret = platform_mif_start_wait_for_cfg_ack_completion(interface);
done:
	return ret;
}
#endif

static int platform_mif_pmu_reset_assert(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
#ifndef CONFIG_WLBT_AUTOGEN_PMUCAL
	unsigned long       timeout;
	int                 ret;
	u32                 val;
#endif
	/* wlbt_if_reset_assertion()
	 WLBT_INT_EN[3] = 0  <---------- different from Orange!
	 WLBT_INT_EN[5] = 0  <---------- different from Orange!
	 WLBT_CTRL_NS[7] = 0
	 WLBT_CONFIGURATION[0] = 0
	 WAIT WLBT_STATUS[0] = 0
	 */

#ifdef CONFIG_WLBT_AUTOGEN_PMUCAL
	return pmu_cal_progress(platform, pmucal_wlbt.reset_assert,pmucal_wlbt.reset_assert_size);
#else

#if 0 /* Different from Orange */
	/* WLBT_CTRL_NS[WLBT_ACTIVE_CLR]  = 0 */
	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_NS, WLBT_ACTIVE_CLR, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_CLR]: %d\n", ret);
	}
	regmap_read(platform->pmureg, WLBT_CTRL_NS, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CTRL_NS[WLBT_ACTIVE_CLR]: 0x%x\n", val);
#endif

#if 1
	/* WLBT_INT_EN[PWR_REQ_F] = 1 enable */
	ret = regmap_update_bits(platform->pmureg, WLBT_INT_EN,
			PWR_REQ_F, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Failed to update WLBT_INT_EN[PWR_REQ_F] %d\n", ret);
		return ret;
	}
	regmap_read(platform->pmureg, WLBT_INT_EN, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully : WLBT_INT_EN[PWR_REQ_F] 0x%x\n", val);

	/* WLBT_INT_EN[TCXO_REQ_F] = 1 enable */
	ret = regmap_update_bits(platform->pmureg, WLBT_INT_EN,
			TCXO_REQ_F, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Failed to update WLBT_INT_EN[TCXO_REQ_F] %d\n", ret);
		return ret;
	}
	regmap_read(platform->pmureg, WLBT_INT_EN, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully : WLBT_INT_EN[TCXO_REQ_F] 0x%x\n", val);
#endif


	/* WLBT_CTRL_NS[WLBT_ACTIVE_EN] = 0 */
	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_NS, WLBT_ACTIVE_EN, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_EN]: %d\n", ret);
	}
	regmap_read(platform->pmureg, WLBT_CTRL_NS, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CTRL_NS[WLBT_ACTIVE_EN]: 0x%x\n", val);

	/* Power Down */
	ret = regmap_write_bits(platform->pmureg, WLBT_CONFIGURATION,
		LOCAL_PWR_CFG, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WLBT_CONFIGURATION[LOCAL_PWR_CFG]: %d\n", ret);
		return ret;
	}

	regmap_read(platform->pmureg, WLBT_CONFIGURATION, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CONFIGURATION[LOCAL_PWR_CFG]: 0x%x\n", val);

	/* Wait for power Off WLBT_STATUS[WLBT_STATUS_BIT0] = 0 */
	timeout = jiffies + msecs_to_jiffies(500);
	do {
		regmap_read(platform->pmureg, WLBT_STATUS, &val);
		val &= (u32)WLBT_STATUS_BIT0;
		if (val == 0) {
			SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATUS 0x%x\n", val);
			/* re affirming power down by reading WLBT_STATES */
			/* STATES[7:0] = 0x80 for Power Down */
			regmap_read(platform->pmureg, WLBT_STATES, &val);
			SCSC_TAG_INFO(PLAT_MIF, "Power down complete: WLBT_STATES 0x%x\n", val);
			ret = 0; /* OK - return */
#if 0
			goto check_vgpio;
#else
			/*>>> different from Orange */
			return 0;
#endif
		}
	} while (time_before(jiffies, timeout));

	/* Timed out */
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Timeout waiting for WLBT_STATUS status\n");
	regmap_read(platform->pmureg, WLBT_STATUS, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATUS 0x%x\n", val);
	regmap_read(platform->pmureg, WLBT_DEBUG, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_DEBUG 0x%x\n", val);
	regmap_read(platform->pmureg, WLBT_STATES, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATES 0x%x\n", val);
	return -ETIME;

#if 0 /*>>> different from Orange */
check_vgpio:
	/* VGPIO_TX_MONITOR[VGPIO_TX_MON_BIT12] = 0x1 */
	timeout = jiffies + msecs_to_jiffies(500);
	do {
		regmap_read(platform->i3c_apm_pmic, VGPIO_TX_MONITOR, &val);
		val &= (u32)VGPIO_TX_MON_BIT12;
		if (val == 0) {
			SCSC_TAG_INFO(PLAT_MIF, "read VGPIO_TX_MONITOR 0x%x\n", val);
			return 0;
		}
	} while (time_before(jiffies, timeout));

	regmap_read(platform->i3c_apm_pmic, VGPIO_TX_MONITOR, &val);
	SCSC_TAG_INFO(PLAT_MIF, "timeout waiting for VGPIO_TX_MONITOR time-out: "
				"VGPIO_TX_MONITOR 0x%x\n", val);
	return -ETIME;
#endif
#endif
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static int platform_mif_set_mem_region2
	(struct scsc_mif_abs *interface,
	void __iomem *_mem_region2,
	size_t _mem_size_region2)
{
	/* If memlogger API is enabled, mxlogger's mem should be set by another routine */
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform->mem_region2 = _mem_region2;
	platform->mem_size_region2 = _mem_size_region2;
	return 0;
}

static void platform_mif_set_memlog_paddr(struct scsc_mif_abs *interface, dma_addr_t paddr)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform->paddr = paddr;
}

#endif

/* reset=0 - release from reset */
/* reset=1 - hold reset */
static int platform_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 ret = 0;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (enable_platform_mif_arm_reset || !reset) {
		if (!reset) { /* Release from reset */
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"SOC_VERSION: product_id 0x%x, rev 0x%x\n",
				exynos_soc_info.product_id, exynos_soc_info.revision);
#endif
			power_supplies_on(platform);
			ret = platform_mif_pmu_reset_release(interface);
		} else {
#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
			cancel_work_sync(&platform->cfgreq_wq);
			flush_workqueue(platform->cfgreq_workq);
#endif
			/* Put back into reset */
			ret = platform_mif_pmu_reset_assert(interface);

			/* Free pmu array */
			if (platform->ka_patch_fw) {
				devm_kfree(platform->dev, platform->ka_patch_fw);
				platform->ka_patch_fw = NULL;
			}
		}
	} else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Not resetting ARM Cores - enable_platform_mif_arm_reset: %d\n",
			 enable_platform_mif_arm_reset);
	return ret;
}

#define SCSC_STATIC_MIFRAM_PAGE_TABLE

static void __iomem *platform_mif_map_region(unsigned long phys_addr, size_t size)
{
	size_t      i;
	struct page **pages;
	void        *vmem;

	size = PAGE_ALIGN(size);
#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	pages = kmalloc((size >> PAGE_SHIFT) * sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		SCSC_TAG_ERR(PLAT_MIF, "wlbt: kmalloc of %zd byte pages table failed\n", (size >> PAGE_SHIFT) * sizeof(*pages));
		return NULL;
	}
#else
	/* Reserve the table statically, but make sure .dts doesn't exceed it */
	{
		static struct page *mif_map_pages[(MIFRAMMAN_MAXMEM >> PAGE_SHIFT) * sizeof(*pages)];

		pages = mif_map_pages;

		SCSC_TAG_INFO(PLAT_MIF, "static mif_map_pages size %zd\n", sizeof(mif_map_pages));

		if (size > MIFRAMMAN_MAXMEM) { /* Size passed in from .dts exceeds array */
			SCSC_TAG_ERR(PLAT_MIF, "wlbt: shared DRAM requested in .dts %zd exceeds mapping table %d\n",
					size, MIFRAMMAN_MAXMEM);
			return NULL;
		}
	}
#endif

	/* Map NORMAL_NC pages with kernel virtual space */
	for (i = 0; i < (size >> PAGE_SHIFT); i++) {
		pages[i] = phys_to_page(phys_addr);
		phys_addr += PAGE_SIZE;
	}

	vmem = vmap(pages, size >> PAGE_SHIFT, VM_MAP, pgprot_writecombine(PAGE_KERNEL));

#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	kfree(pages);
#endif
	if (!vmem)
		SCSC_TAG_ERR(PLAT_MIF, "wlbt: vmap of %zd pages failed\n", (size >> PAGE_SHIFT));
	return (void __iomem *)vmem;
}

static void platform_mif_unmap_region(void *vmem)
{
	vunmap(vmem);
}

static void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u8                  i;

	if (allocated)
		*allocated = 0;

	platform->mem =
		platform_mif_map_region(platform->mem_start, platform->mem_size);

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Error remaping shared memory\n");
		return NULL;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Map: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);

	/* Initialise MIF registers with documented defaults */
	/* MBOXes */
	for (i = 0; i < NUM_MBOX_PLAT; i++) {
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
	}

	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);

#ifdef CONFIG_SCSC_CHV_SUPPORT
	if (chv_disable_irq == true) {
		if (allocated)
			*allocated = platform->mem_size;
		return platform->mem;
	}
#endif
	/* register interrupts */
	if (platform_mif_register_irq(platform)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
		platform_mif_unmap_region(platform->mem);
		return NULL;
	}

	if (allocated)
		*allocated = platform->mem_size;
	/* Set the CR4 base address in Mailbox??*/
	return platform->mem;
}

/* HERE: Not sure why mem is passed in - its stored in platform - as it should be */
static void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* Avoid unused parameter error */
	(void)mem;

	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);

#ifdef CONFIG_SCSC_CHV_SUPPORT
	/* Restore PIO changed by Maxwell subsystem */
	if (chv_disable_irq == false)
		/* Unregister IRQs */
		platform_mif_unregister_irq(platform);
#else
	platform_mif_unregister_irq(platform);
#endif
	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
	platform_mif_unmap_region(platform->mem);
	platform->mem = NULL;
}

static u32 platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0)) >> 16;
	else
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0)) >> 16;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Getting INTMR0 0x%x target %s\n", val,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
	return val;
}

static u32 platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;

	/* Function has to return the interrupts that are enabled *AND* not masked */
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR0)) >> 16;
	else
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMSR0)) >> 16;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Getting INTMSR0 0x%x target %s\n", val,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
	return val;
}

static void platform_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 reg;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}

	reg = INTGR1;
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(reg), (1 << bit_num));
	else
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(reg), (1 << bit_num));

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTGR1: bit %d on target %s\n", bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}

static void platform_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	/* WRITE : 1 = Clears Interrupt */
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), ((1 << bit_num) << 16));
	else
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), ((1 << bit_num) << 16));

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTCR0: bit %d on target %s\n", bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}

static void platform_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 1 = Mask Interrupt */
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val | ((1 << bit_num) << 16));
	} else {
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 1 = Mask Interrupt */
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), val | ((1 << bit_num) << 16));
	}
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTMR0: 0x%x bit %d on target %s\n", val | (1 << bit_num), bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}

static void platform_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 0 = Unmask Interrupt */
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val & ~((1 << bit_num) << 16));
	} else {
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 0 = Unmask Interrupt */
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), val & ~((1 << bit_num) << 16));
	}
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "UNMASK Setting INTMR0: 0x%x bit %d on target %s\n", val & ~((1 << bit_num) << 16), bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}

static void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting remapper address %u target %s\n", remap_addr,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		platform->remap_addr_wlan = remap_addr;
	else if (target == SCSC_MIF_ABS_TARGET_WPAN)
		platform->remap_addr_wpan = remap_addr;
	else
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect target %d\n", target);
}

/* Return the contents of the mask register */
static u32 __platform_mif_irq_bit_mask_read(struct platform_mif *platform)
{
	u32                 val;
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read INTMR0: 0x%x\n", val);

	return val;
}

/* Return the contents of the mask register */
static u32 __platform_mif_irq_bit_mask_read_wpan(struct platform_mif *platform)
{
	u32                 val;
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read INTMR0: 0x%x\n", val);

	return val;
}

/* Write the mask register, destroying previous contents */
static void __platform_mif_irq_bit_mask_write(struct platform_mif *platform, u32 val)
{
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val);
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Write INTMR0: 0x%x\n", val);
}

static void __platform_mif_irq_bit_mask_write_wpan(struct platform_mif *platform, u32 val)
{
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), val);
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Write INTMR0: 0x%x\n", val);
}

static void platform_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif int handler %pS in %p %p\n", handler, platform, interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wlan_handler = handler;
	platform->irq_dev = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_unreg_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif int handler %pS\n", interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wlan_handler = platform_mif_irq_default_handler;
	platform->irq_dev = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_reg_handler_wpan(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif int handler for WPAN %pS in %p %p\n", handler, platform, interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wpan_handler = handler;
	platform->irq_dev_wpan = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_unreg_handler_wpan(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif int handler for WPAN %pS\n", interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wpan_handler = platform_mif_irq_default_handler;
	platform->irq_dev_wpan = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif reset_request int handler %pS in %p %p\n", handler, platform, interface);
	platform->reset_request_handler = handler;
	platform->irq_reset_request_dev = dev;
	if (atomic_read(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Default WDOG handler disabled by spurios IRQ...re-enabling.\n");
		enable_irq(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		atomic_set(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt, 0);
	}
}

static void platform_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "UnRegistering mif reset_request int handler %pS\n", interface);
	platform->reset_request_handler = platform_mif_irq_reset_request_default_handler;
	platform->irq_reset_request_dev = NULL;
}

static void platform_mif_suspend_reg_handler(struct scsc_mif_abs *interface,
		int (*suspend)(struct scsc_mif_abs *abs, void *data),
		void (*resume)(struct scsc_mif_abs *abs, void *data),
		void *data)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif suspend/resume handlers in %p %p\n", platform, interface);
	platform->suspend_handler = suspend;
	platform->resume_handler = resume;
	platform->suspendresume_data = data;
}

static void platform_mif_suspend_unreg_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif suspend/resume handlers in %p %p\n", platform, interface);
	platform->suspend_handler = NULL;
	platform->resume_handler = NULL;
	platform->suspendresume_data = NULL;
}

static u32 *platform_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 *addr;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "mbox index 0x%x target %s\n", mbox_index,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		addr = platform->base + MAILBOX_WLBT_REG(ISSR(mbox_index));
	else
		addr = platform->base_wpan + MAILBOX_WLBT_REG(ISSR(mbox_index));
	return addr;
}

static int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val;

	regmap_read(platform->boot_cfg, WB2AP_MAILBOX, &val);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read WB2AP_MAILBOX: %u\n", val);
	return val;
}

static int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	regmap_write(platform->boot_cfg, AP2WB_MAILBOX, val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Write AP2WB_MAILBOX: %u\n", val);
	return 0;
}

static int platform_load_pmu_fw(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (platform->ka_patch_fw) {
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "ka_patch already allocated\n");
		devm_kfree(platform->dev, platform->ka_patch_fw);
	}

	platform->ka_patch_fw = devm_kzalloc(platform->dev, ka_patch_len, GFP_KERNEL);

	if (!platform->ka_patch_fw) {
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "Unable to allocate 0x%x\n", ka_patch_len);
		return -ENOMEM;
	}

	memcpy(platform->ka_patch_fw, ka_patch, ka_patch_len);
	platform->ka_patch_len = ka_patch_len;
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "load_pmu_fw sz %u done\n", ka_patch_len);
	return 0;
}

static void platform_mif_irq_reg_pmu_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif pmu int handler %pS in %p %p\n", handler, platform, interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->pmu_handler = handler;
	platform->irq_dev_pmu = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static int platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return -ENOMEM;
	}

	/* Check limits! */
	if (ptr >= (platform->mem + platform->mem_size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Unable to get pointer reference\n");
		return -ENOMEM;
	}

	*ref = (scsc_mifram_ref)((uintptr_t)ptr - (uintptr_t)platform->mem);

	return 0;
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static int platform_mif_get_mifram_ref_region2(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_region2) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return -ENOMEM;
	}

	/* Check limits! */
	if (ptr >= (platform->mem_region2 + platform->mem_size_region2))
		return -ENOMEM;

	*ref = (scsc_mifram_ref)(
		(uintptr_t)ptr
		- (uintptr_t)platform->mem_region2
		+ platform->mem_size);
	return 0;
}
#endif

static void *platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}

	/* Check limits */
	if (ref >= 0 && ref < platform->mem_size)
		return (void *)((uintptr_t)platform->mem + (uintptr_t)ref);
	else
		return NULL;
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static void *platform_mif_get_mifram_ptr_region2(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_region2) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}
	ref -= platform->mem_size;

	/* Check limits */
	if (ref >= 0 && ref < platform->mem_size_region2)
		return (void *)((uintptr_t)platform->mem_region2 + (uintptr_t)ref);
	else
		return NULL;
}
#endif

static void *platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_start) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}

	return (void *)((uintptr_t)platform->mem_start + (uintptr_t)ref);
}

static uintptr_t platform_mif_get_mif_pfn(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return vmalloc_to_pfn(platform->mem);
}

static struct device *platform_mif_get_mif_device(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	return platform->dev;
}

static void platform_mif_irq_clear(void)
{
	/* Implement if required */
}

static int platform_mif_read_register(struct scsc_mif_abs *interface, u64 id, u32 *val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (id == SCSC_REG_READ_WLBT_STAT) {
		regmap_read(platform->pmureg, WLBT_STAT, val);
		return 0;
	}

	return -EIO;
}

static void platform_mif_dump_register(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTGR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTCR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTSR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR0)));

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTGR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTCR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTSR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR1)));

	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

inline void platform_int_debug(struct platform_mif *platform)
{
	int i;
	int irq;
	int ret;
	bool pending, active, masked;
	int irqs[] = {PLATFORM_MIF_MBOX, PLATFORM_MIF_WDOG};
	char *irqs_name[] = {"MBOX", "WDOG"};

	for (i = 0; i < (sizeof(irqs) / sizeof(int)); i++) {
		irq = platform->wlbt_irq[irqs[i]].irq_num;

		ret  = irq_get_irqchip_state(irq, IRQCHIP_STATE_PENDING, &pending);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_ACTIVE,  &active);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_MASKED,  &masked);
		if (!ret)
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "IRQCHIP_STATE %d(%s): pending %d, active %d, masked %d\n",
							  irq, irqs_name[i], pending, active, masked);
	}
	platform_mif_dump_register(&platform->interface);
}

static void platform_mif_cleanup(struct scsc_mif_abs *interface)
{
}

static void platform_mif_restart(struct scsc_mif_abs *interface)
{
}

#ifdef CONFIG_OF_RESERVED_MEM
static int __init platform_mif_wifibt_if_reserved_mem_setup(struct reserved_mem *remem)
{
	SCSC_TAG_DEBUG(PLAT_MIF, "memory reserved: mem_base=%#lx, mem_size=%zd\n",
		       (unsigned long)remem->base, (size_t)remem->size);

	sharedmem_base = remem->base;
	sharedmem_size = remem->size;
	return 0;
}
RESERVEDMEM_OF_DECLARE(wifibt_if, "exynos,wifibt_if", platform_mif_wifibt_if_reserved_mem_setup);
#endif

struct scsc_mif_abs *platform_mif_create(struct platform_device *pdev)
{
	struct scsc_mif_abs *platform_if;
	struct platform_mif *platform =
		(struct platform_mif *)devm_kzalloc(&pdev->dev, sizeof(struct platform_mif), GFP_KERNEL);
	int                 err = 0;
	u8                  i = 0;
	struct resource     *reg_res;

	if (!platform)
		return NULL;

	SCSC_TAG_INFO_DEV(PLAT_MIF, &pdev->dev, "Creating MIF platform device\n");

	platform_if = &platform->interface;

	/* initialise interface structure */
	platform_if->destroy = platform_mif_destroy;
	platform_if->get_uid = platform_mif_get_uid;
	platform_if->reset = platform_mif_reset;
	platform_if->map = platform_mif_map;
	platform_if->unmap = platform_mif_unmap;
	platform_if->irq_bit_set = platform_mif_irq_bit_set;
	platform_if->irq_get = platform_mif_irq_get;
	platform_if->irq_bit_mask_status_get = platform_mif_irq_bit_mask_status_get;
	platform_if->irq_bit_clear = platform_mif_irq_bit_clear;
	platform_if->irq_bit_mask = platform_mif_irq_bit_mask;
	platform_if->irq_bit_unmask = platform_mif_irq_bit_unmask;
	platform_if->remap_set = platform_mif_remap_set;
	platform_if->irq_reg_handler = platform_mif_irq_reg_handler;
	platform_if->irq_unreg_handler = platform_mif_irq_unreg_handler;
	platform_if->irq_reg_handler_wpan = platform_mif_irq_reg_handler_wpan;
	platform_if->irq_unreg_handler_wpan = platform_mif_irq_unreg_handler_wpan;
	platform_if->irq_reg_reset_request_handler = platform_mif_irq_reg_reset_request_handler;
	platform_if->irq_unreg_reset_request_handler = platform_mif_irq_unreg_reset_request_handler;
	platform_if->suspend_reg_handler = platform_mif_suspend_reg_handler;
	platform_if->suspend_unreg_handler = platform_mif_suspend_unreg_handler;
	platform_if->get_mbox_ptr = platform_mif_get_mbox_ptr;
	platform_if->get_mifram_ptr = platform_mif_get_mifram_ptr;
	platform_if->get_mifram_ref = platform_mif_get_mifram_ref;
	platform_if->get_mifram_pfn = platform_mif_get_mif_pfn;
	platform_if->get_mifram_phy_ptr = platform_mif_get_mifram_phy_ptr;
	platform_if->get_mif_device = platform_mif_get_mif_device;
	platform_if->irq_clear = platform_mif_irq_clear;
	platform_if->mif_dump_registers = platform_mif_dump_register;
	platform_if->mif_read_register = platform_mif_read_register;
	platform_if->mif_cleanup = platform_mif_cleanup;
	platform_if->mif_restart = platform_mif_restart;
#ifdef CONFIG_SCSC_QOS
	platform_if->mif_pm_qos_add_request = platform_mif_pm_qos_add_request;
	platform_if->mif_pm_qos_update_request = platform_mif_pm_qos_update_request;
	platform_if->mif_pm_qos_remove_request = platform_mif_pm_qos_remove_request;
	platform_if->mif_set_affinity_cpu = platform_mif_set_affinity_cpu;
#endif
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	platform_if->get_mifram_ptr_region2 = platform_mif_get_mifram_ptr_region2;
	platform_if->get_mifram_ref_region2 = platform_mif_get_mifram_ref_region2;
	platform_if->set_mem_region2 = platform_mif_set_mem_region2;
	platform_if->set_memlog_paddr = platform_mif_set_memlog_paddr;
	platform->paddr = 0;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	platform_if->recovery_disabled_reg = platform_recovery_disabled_reg;
	platform_if->recovery_disabled_unreg = platform_recovery_disabled_unreg;
#endif
	platform_if->get_mbox_pmu = platform_mif_get_mbox_pmu;
	platform_if->set_mbox_pmu = platform_mif_set_mbox_pmu;
	platform_if->load_pmu_fw = platform_load_pmu_fw;
	platform_if->irq_reg_pmu_handler = platform_mif_irq_reg_pmu_handler;
	platform->pmu_handler = platform_mif_irq_default_handler;
	platform->irq_dev_pmu = NULL;
	/* Reset ka_patch pointer & size */
	platform->ka_patch_fw = NULL;
	platform->ka_patch_len = 0;
	/* Update state */
	platform->pdev = pdev;
	platform->dev = &pdev->dev;

	platform->wlan_handler = platform_mif_irq_default_handler;
	platform->wpan_handler = platform_mif_irq_default_handler;
	platform->irq_dev = NULL;
	platform->reset_request_handler = platform_mif_irq_reset_request_default_handler;
	platform->irq_reset_request_dev = NULL;
	platform->suspend_handler = NULL;
	platform->resume_handler = NULL;
	platform->suspendresume_data = NULL;

#ifdef CONFIG_OF_RESERVED_MEM
	if(!sharedmem_base){
		struct device_node * np;
		np = of_parse_phandle(platform->dev->of_node, "memory-region", 0);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "module build register sharedmem np %x\n",np);
		if(np){
			platform->mem_start = of_reserved_mem_lookup(np)->base;
			platform->mem_size = of_reserved_mem_lookup(np)->size;
		}
	}
	else{
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "built-in register sharedmem \n");
		platform->mem_start = sharedmem_base;
		platform->mem_size = sharedmem_size;
	}
#else
	/* If CONFIG_OF_RESERVED_MEM is not defined, sharedmem values should be
	 * parsed from the scsc_wifibt binding
	 */
	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-base", &sharedmem_base)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_start = sharedmem_base;

	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-size", &sharedmem_size)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_size = sharedmem_size;
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "platform->mem_start 0x%x platform->mem_size 0x%x\n",
			(u32)platform->mem_start, (u32)platform->mem_size);
	if (platform->mem_start == 0)
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "platform->mem_start is 0");

	if (platform->mem_size == 0) {
		/* We return return if mem_size is 0 as it does not make any sense.
		 * This may be an indication of an incorrect platform device binding.
		 */
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "platform->mem_size is 0");
		err = -EINVAL;
		goto error_exit;
	}
#if 0 /* Different from Orange */
	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!reg_res) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error getting mem resource for MAILBOX_WLBT\n");
		err = -ENOENT;
		goto error_exit;
	}

	platform->reg_start = reg_res->start;
	platform->reg_size = resource_size(reg_res);

	platform->base =
		devm_ioremap_nocache(platform->dev, reg_res->start, resource_size(reg_res));

	if (!platform->base) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error mapping register region\n");
		err = -EBUSY;
		goto error_exit;
	}
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "platform->reg_start %lx size %x base %p\n",
		(uintptr_t)platform->reg_start, (u32)platform->reg_size, platform->base);
#else
	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	platform->base = devm_ioremap_resource(&pdev->dev, reg_res);
	if (IS_ERR(platform->base)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error getting mem resource for MAILBOX_WLAN\n");
		err = PTR_ERR(platform->base);
		goto error_exit;
	}

	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	platform->base_wpan = devm_ioremap_resource(&pdev->dev, reg_res);
	if (IS_ERR(platform->base_wpan)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error getting mem resource for MAILBOX_WPAN\n");
		err = PTR_ERR(platform->base_wpan);
		goto error_exit;
	}
#endif
	/* Get the 5 IRQ resources */
	for (i = 0; i < 5; i++) {
		struct resource *irq_res;
		int             irqtag;

		irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!irq_res) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"No IRQ resource at index %d\n", i);
			err = -ENOENT;
			goto error_exit;
		}

		if (!strcmp(irq_res->name, "MBOX_WLAN")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "MBOX_WLAN irq %d flag 0x%x\n",
				(u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_MBOX;
		} else if (!strcmp(irq_res->name, "MBOX_WPAN")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "MBOX_WPAN irq %d flag 0x%x\n",
				(u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_MBOX_WPAN;
		} else if (!strcmp(irq_res->name, "ALIVE")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ALIVE irq %d flag 0x%x\n",
				(u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_ALIVE;
		} else if (!strcmp(irq_res->name, "WDOG")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WDOG irq %d flag 0x%x\n",
				(u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_WDOG;
		} else if (!strcmp(irq_res->name, "CFG_REQ")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "CFG_REQ irq %d flag 0x%x\n",
				(u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_CFG_REQ;
		} else {
			SCSC_TAG_ERR_DEV(PLAT_MIF, &pdev->dev, "Invalid irq res name: %s\n",
				irq_res->name);
			err = -EINVAL;
			goto error_exit;
		}
		platform->wlbt_irq[irqtag].irq_num = irq_res->start;
		platform->wlbt_irq[irqtag].flags = (irq_res->flags & IRQF_TRIGGER_MASK);
		atomic_set(&platform->wlbt_irq[irqtag].irq_disabled_cnt, 0);
	}

	/* PMU reg map - syscon */
	platform->pmureg = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,syscon-phandle");
	if (IS_ERR(platform->pmureg)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"syscon regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->pmureg));
		err = -EINVAL;
		goto error_exit;
	}

	/* Completion event and state used to indicate CFG_REQ IRQ occurred */
	init_completion(&platform->cfg_ack);
	platform->boot_state = WLBT_BOOT_IN_RESET;
	/* I3C_APM_PMIC */
	platform->i3c_apm_pmic = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,i3c_apm_pmic-syscon-phandle");
	if (IS_ERR(platform->i3c_apm_pmic)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"i3c_apm_pmic regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->i3c_apm_pmic));
		err = -EINVAL;
		goto error_exit;
	}
#if 1
	/* DBUS_BAAW */
	platform->dbus_baaw = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,dbus_baaw-syscon-phandle");
	if (IS_ERR(platform->dbus_baaw)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"dbus_baaw regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->dbus_baaw));
		err = -EINVAL;
		goto error_exit;
	}

	/* PBUS_BAAW */
	platform->pbus_baaw = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,pbus_baaw-syscon-phandle");
	if (IS_ERR(platform->pbus_baaw)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"pbus_baaw regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->pbus_baaw));
		err = -EINVAL;
		goto error_exit;
	}
	/* WLBT_REMAP */
	platform->wlbt_remap = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,wlbt_remap-syscon-phandle");
	if (IS_ERR(platform->wlbt_remap)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"wlbt_remap regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->wlbt_remap));
		err = -EINVAL;
		goto error_exit;
	}

	/* BOOT_CFG */
	platform->boot_cfg = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,boot_cfg-syscon-phandle");
	if (IS_ERR(platform->boot_cfg)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"boot_cfg regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->boot_cfg));
		err = -EINVAL;
		goto error_exit;
	}
#endif

#ifdef CONFIG_SCSC_QOS
	platform_mif_parse_qos(platform, platform->dev->of_node);
#endif
	/* Initialize spinlock */
	spin_lock_init(&platform->mif_spinlock);

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	platform->cfgreq_workq = create_singlethread_workqueue("wlbt_cfg_reg_work");
	if (!platform->cfgreq_workq) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Error creating CFG_REQ singlethread_workqueue\n");
		err = -ENOMEM;
		goto error_exit;
	}

	INIT_WORK(&platform->cfgreq_wq, platform_cfg_req_wq);
#endif

	return platform_if;

error_exit:
	devm_kfree(&pdev->dev, platform);
	return NULL;
}

void platform_mif_destroy_platform(struct platform_device *pdev, struct scsc_mif_abs *interface)
{
#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	destroy_workqueue(platform->cfgreq_workq);
#endif
}

struct platform_device *platform_mif_get_platform_dev(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	BUG_ON(!interface || !platform);

	return platform->pdev;
}

/* Preserve MIF registers during suspend.
 * If all users of the MIF (AP, mx140, CP, etc) release it, the registers
 * will lose their values. Save the useful subset here.
 *
 * Assumption: the AP will not change the register values between the suspend
 * and resume handlers being called!
 */
static void platform_mif_reg_save(struct platform_mif *platform)
{
	platform->mif_preserve.irq_bit_mask = __platform_mif_irq_bit_mask_read(platform);
	platform->mif_preserve.irq_bit_mask_wpan = __platform_mif_irq_bit_mask_read_wpan(platform);
}

/* Restore MIF registers that may have been lost during suspend */
static void platform_mif_reg_restore(struct platform_mif *platform)
{
	__platform_mif_irq_bit_mask_write(platform, platform->mif_preserve.irq_bit_mask);
	__platform_mif_irq_bit_mask_write_wpan(platform, platform->mif_preserve.irq_bit_mask_wpan);
}

int platform_mif_suspend(struct scsc_mif_abs *interface)
{
	int r = 0;
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (platform->suspend_handler)
		r = platform->suspend_handler(interface, platform->suspendresume_data);

	/* Save the MIF registers.
	 * This must be done last as the suspend_handler may use the MIF
	 */
	platform_mif_reg_save(platform);

	return r;
}

void platform_mif_resume(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	s32 ret;

	/* Restore the MIF registers.
	 * This must be done first as the resume_handler may use the MIF.
	 */
	platform_mif_reg_restore(platform);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Clear WLBT_ACTIVE_CLR flag\n");
	/* Clear WLBT_ACTIVE_CLR flag in WLBT_CTRL_NS */
	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_NS, WLBT_ACTIVE_CLR, WLBT_ACTIVE_CLR);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_CLR]: %d\n", ret);
	}

	if (platform->resume_handler)
		platform->resume_handler(interface, platform->suspendresume_data);
}

/* Temporary workaround to power up slave PMIC LDOs before FW APM/WLBT signalling
 * is complete
 */
static void power_supplies_on(struct platform_mif *platform)
{
	//struct i2c_client i2c;

	/* HACK: Note only addr field is needed by s2mpu11_write_reg() */
	//i2c.addr = 0x1;

	/* The APM IPC in FW will be used instead */
	if (disable_apm_setup) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT LDOs firmware controlled\n");
		return;
	}

	//SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT LDOs on (PMIC i2c_addr = 0x%x)\n", i2c.addr);

	/* SLAVE PMIC
	 * echo 0x22 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xE0 > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x23 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xE8 > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x24 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xEC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x25 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xEC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x26 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xFC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 *
	 * echo 0x27 > /sys/kernel/debug/s2mpu11-regs/i2caddr
	 * echo 0xFC > /sys/kernel/debug/s2mpu11-regs/i2cdata
	 */

	/*s2mpu11_write_reg(&i2c, 0x22, 0xe0);
	s2mpu11_write_reg(&i2c, 0x23, 0xe8);
	s2mpu11_write_reg(&i2c, 0x24, 0xec);
	s2mpu11_write_reg(&i2c, 0x25, 0xec);
	s2mpu11_write_reg(&i2c, 0x26, 0xfc);
	s2mpu11_write_reg(&i2c, 0x27, 0xfc);*/
}
