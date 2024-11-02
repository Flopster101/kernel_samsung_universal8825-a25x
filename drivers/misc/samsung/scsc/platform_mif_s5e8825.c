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
#include <soc/samsung/exynos-smc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#endif
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <scsc/scsc_logring.h>
#include "mif_reg_S5E8825.h"
#include "platform_mif_module.h"
#include "miframman.h"
#include "platform_mif_s5e8825.h"
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
#include <linux/soc/samsung/exynos-soc.h>
#endif

#ifdef CONFIG_SCSC_QOS
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#else
#include <linux/pm_qos.h>
#endif
#endif

#if !defined(CONFIG_SOC_S5E8825)
#error Target processor CONFIG_SOC_S5E8825 not selected
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

#if defined(CONFIG_WLBT_DCXO_TUNE)
static u32 oldapm_intmr1_val;
#endif

static unsigned long sharedmem_base;
static size_t sharedmem_size;

static bool fw_compiled_in_kernel;
module_param(fw_compiled_in_kernel, bool, 0644);
MODULE_PARM_DESC(fw_compiled_in_kernel, "Use FW compiled in kernel");

#ifdef CONFIG_SCSC_CHV_SUPPORT
static bool chv_disable_irq;
module_param(chv_disable_irq, bool, 0644);
MODULE_PARM_DESC(chv_disable_irq, "Do not register for irq");
#endif

bool enable_hwbypass;
module_param(enable_hwbypass, bool, 0644);
MODULE_PARM_DESC(enable_platform_mif_arm_reset, "Enables hwbypass");

static bool enable_platform_mif_arm_reset = true;
module_param(enable_platform_mif_arm_reset, bool, 0644);
MODULE_PARM_DESC(enable_platform_mif_arm_reset, "Enables WIFIBT ARM cores reset");

static bool disable_apm_setup = true;
module_param(disable_apm_setup, bool, 0644);
MODULE_PARM_DESC(disable_apm_setup, "Disable host APM setup");

static uint wlbt_status_timeout_value = 500;
module_param(wlbt_status_timeout_value, uint, 0644);
MODULE_PARM_DESC(wlbt_status_timeout_value, "wlbt_status_timeout(ms)");

static bool enable_scandump_wlbt_status_timeout;
module_param(enable_scandump_wlbt_status_timeout, bool, 0644);
MODULE_PARM_DESC(enable_scandump_wlbt_status_timeout, "wlbt_status_timeout(ms)");

static bool force_to_wlbt_status_timeout;
module_param(force_to_wlbt_status_timeout, bool, 0644);
MODULE_PARM_DESC(force_to_wlbt_status_timeout, "wlbt_status_timeout(ms)");

static bool init_done;

#ifdef CONFIG_SCSC_QOS
struct qos_table {
	unsigned int freq_mif;
	unsigned int freq_int;
	unsigned int freq_cl0;
	unsigned int freq_cl1;
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

#if defined(CONFIG_WLBT_DCXO_TUNE)
inline void platform_mif_reg_write_apm(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base_apm + offset);
}

inline u32 platform_mif_reg_read_apm(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base_apm + offset);
}
#endif

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
	if (!(len == 12)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "No qos table for wlbt, or incorrect size\n");
		return -ENOENT;
	}

	platform->qos = devm_kzalloc(platform->dev, sizeof(struct qos_table) * len / 4, GFP_KERNEL);
	if (!platform->qos)
		return -ENOMEM;

	of_property_read_u32_array(np, "qos_table", (unsigned int *)platform->qos, len);

	for (i = 0; i < len * 4 / sizeof(struct qos_table); i++) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "QoS Table[%d] mif : %u int : %u cl0 : %u cl1 : %u\n", i,
			platform->qos[i].freq_mif,
			platform->qos[i].freq_int,
			platform->qos[i].freq_cl0,
			platform->qos[i].freq_cl1);
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
		table.freq_cl1 = platform->qos[0].freq_cl1;
		break;

	case SCSC_QOS_MED:
		table.freq_mif = platform->qos[1].freq_mif;
		table.freq_int = platform->qos[1].freq_int;
		table.freq_cl0 = platform->qos[1].freq_cl0;
		table.freq_cl1 = platform->qos[1].freq_cl1;
		break;

	case SCSC_QOS_MAX:
		table.freq_mif = platform->qos[2].freq_mif;
		table.freq_int = platform->qos[2].freq_int;
		table.freq_cl0 = platform->qos[2].freq_cl0;
		table.freq_cl1 = platform->qos[2].freq_cl1;
		break;

	default:
		table.freq_mif = 0;
		table.freq_int = 0;
		table.freq_cl0 = 0;
		table.freq_cl1 = 0;
	}

	return table;
}

static int platform_mif_pm_qos_add_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct qos_table table;
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	int ret;
#endif

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	table = platform_mif_pm_qos_get_table(platform, config);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"PM QoS add request: %u. MIF %u INT %u CL0 %u CL1 %u\n", config, table.freq_mif, table.freq_int, table.freq_cl0, table.freq_cl1);

#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	qos_req->cpu_cluster0_policy = cpufreq_cpu_get(0);
	qos_req->cpu_cluster1_policy = cpufreq_cpu_get(7);

	if ((!qos_req->cpu_cluster0_policy) || (!qos_req->cpu_cluster1_policy)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS add request error. CPU policy not loaded\n");
		return -ENOENT;
	}
	exynos_pm_qos_add_request(&qos_req->pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT, table.freq_mif);
	exynos_pm_qos_add_request(&qos_req->pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, table.freq_int);

	ret = freq_qos_tracer_add_request(&qos_req->cpu_cluster0_policy->constraints, &qos_req->pm_qos_req_cl0, FREQ_QOS_MIN, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS add request cl0. Setting freq_qos_add_request %d\n", ret);
	ret = freq_qos_tracer_add_request(&qos_req->cpu_cluster1_policy->constraints, &qos_req->pm_qos_req_cl1, FREQ_QOS_MIN, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS add request cl1. Setting freq_qos_add_request %d\n", ret);
#else
	pm_qos_add_request(&qos_req->pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT, table.freq_mif);
	pm_qos_add_request(&qos_req->pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, table.freq_int);
	pm_qos_add_request(&qos_req->pm_qos_req_cl0, PM_QOS_CLUSTER0_FREQ_MIN, table.freq_cl0);
	pm_qos_add_request(&qos_req->pm_qos_req_cl1, PM_QOS_CLUSTER1_FREQ_MIN, table.freq_cl1);
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
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	table = platform_mif_pm_qos_get_table(platform, config);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"PM QoS update request: %u. MIF %u INT %u CL0 %u CL1 %u\n", config, table.freq_mif, table.freq_int, table.freq_cl0, table.freq_cl1);

#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	exynos_pm_qos_update_request(&qos_req->pm_qos_req_mif, table.freq_mif);
	exynos_pm_qos_update_request(&qos_req->pm_qos_req_int, table.freq_int);
	freq_qos_update_request(&qos_req->pm_qos_req_cl0, table.freq_cl0);
	freq_qos_update_request(&qos_req->pm_qos_req_cl1, table.freq_cl1);
#else
	pm_qos_update_request(&qos_req->pm_qos_req_mif, table.freq_mif);
	pm_qos_update_request(&qos_req->pm_qos_req_int, table.freq_int);
	pm_qos_update_request(&qos_req->pm_qos_req_cl0, table.freq_cl0);
	pm_qos_update_request(&qos_req->pm_qos_req_cl1, table.freq_cl1);
#endif

	return 0;
}

static int platform_mif_pm_qos_remove_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;


	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS remove request\n");
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	exynos_pm_qos_remove_request(&qos_req->pm_qos_req_mif);
	exynos_pm_qos_remove_request(&qos_req->pm_qos_req_int);
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl0);
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl1);
#else
	pm_qos_remove_request(&qos_req->pm_qos_req_mif);
	pm_qos_remove_request(&qos_req->pm_qos_req_int);
	pm_qos_remove_request(&qos_req->pm_qos_req_cl0);
	pm_qos_remove_request(&qos_req->pm_qos_req_cl1);
#endif

	return 0;
}
#endif

#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
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

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Unregistering mif recovery\n");
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
	SCSC_TAG_INFO_DEV(PLAT_MIF, NULL,
			  "INT reset_request handler not registered\n");
}

irqreturn_t platform_mif_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "INT %pS\n", platform->wlan_handler);
	if (platform->wlan_handler != platform_mif_irq_default_handler) {
		platform->wlan_handler(irq, platform->irq_dev);
	} else {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "MIF Interrupt Handler not registered.\n");
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
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "MIF Interrupt Handler not registered\n");
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
			/* Spurious interrupt from the SOC during
			 *  CFG_REQ phase, just consume it
			 */
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "Spurious wdog irq during cfg_req phase\n");
			return IRQ_HANDLED;
		}
		disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		platform->reset_request_handler(irq, platform->irq_reset_request_dev);
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "WDOG Interrupt reset_request_handler not registered\n");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Disabling unhandled WDOG IRQ.\n");
		disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		atomic_inc(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt);
	}

	/* The wakeup source isn't cleared until WLBT is reset, so change the
	 *  interrupt type to suppress this
	 */
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	if (platform->recovery_disabled && platform->recovery_disabled()) {
#else
	if (mxman_recovery_disabled()) {
#endif
		ret = regmap_update_bits(platform->pmureg, WAKEUP_INT_TYPE,
					 RESETREQ_WLBT, 0);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Set RESETREQ_WLBT wakeup interrput type to EDGE.\n");
		if (ret < 0)
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
					 "Failed to Set WAKEUP_INT_IN[RESETREQ_WLBT]: %d\n", ret);
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
0x75888375,
0x43790382,
0x09f7a3e0,
0x75f978b9,
0x80752f81,
0xfce87501,
0x90900174,
0x9090f000,
0x0074f001,
0xf0029090,
0xf0039090,
0x24f50074,
0xf0049090,
0xf0059090,
0xf0069090,
0xf0079090,
0x7581a843,
0xd843ffbf,
0x00927540,
0xb020a0c2,
0x229285fd,
0x43239e85,
0x928501f9,
0x239e8522,
0x1212ff12,
0xf9432084,
0xfef95302,
0x9190f3e5,
0xf6e5f022,
0xf0239190,
0x91900374,
0x8ee5f021,
0x740801b4,
0x02909002,
0xb41a80f0,
0x02740802,
0xf0039090,
0x03b40f80,
0x9002740c,
0x90f00290,
0x80f00390,
0xa4831200,
0x125c8412,
0x80752a85,
0x54b3e584,
0x12166001,
0x07743182,
0xd23e8212,
0x01b443a4,
0x0154b3e5,
0xb453fa70,
0x06c030fe,
0x7501af43,
0xcd808080,
0xf7532091,
0xfd9153fc,
0x7501f743,
0x80752f81,
0xfce87501,
0xe4012075,
0xc075d8f5,
0x07ef75ff,
0x75afc853,
0xb77511b7,
0x25b77517,
0xf581a843,
0x900174bd,
0x75f02091,
0x91904c80,
0xb4f9e020,
0x81020301,
0x488075cf,
0xe52807b4,
0x20b065a0,
0x0970f9e3,
0x82120174,
0x80a3c23e,
0x30a3d213,
0xace5fdb3,
0xc209e030,
0xe5acf5e0,
0xfbe020ad,
0xb4c98102,
0x01b40004,
0x9000e531,
0xaaf02191,
0x2c01ba20,
0xf4f5f1f5,
0x75508075,
0x24750220,
0x03e12001,
0x30022443,
0xf74305e5,
0x53038002,
0x0454fdf7,
0x21f50344,
0x02b44780,
0x01207505,
0x06b43f80,
0xb88da839,
0x8d750cf0,
0x308ed2fe,
0x99c2fd99,
0x89430880,
0x1fe17520,
0x8d759ed2,
0xc299c2f0,
0x7505608f,
0x0380ff99,
0xd2009975,
0xc240788e,
0xfd8f308f,
0x8ec2f9d8,
0x80750380,
0x90017410,
0xe5f02091,
0x3502b420,
0x1054cfe5,
0x82122f60,
0x121b6052,
0xf3e52084,
0xf0229190,
0x9190f6e5,
0x0374f023,
0xf0219190,
0x12be8312,
0x80750d84,
0x0120754c,
0x5380aa43,
0xb543fbf9,
0x03c03018,
0x75808075,
0xb3e58480,
0x16600154,
0x74318212,
0x3e821207,
0xb443a4d2,
0x54b3e501,
0x53fa7001,
0x8102feb4,
0x09a3300f,
0xb1420374,
0x62fdb2b5,
0x42f822b1,
0x58ade5ac,
0x22fa7068,
0xac52f4f8,
0x7058ade5,
0x807522fb,
0x12077440,
0x92853e82,
0x239e8522,
0x7506e320,
0xce7508cd,
0x7fc07510,
0x4301ef43,
0xd84311d8,
0x54c0e540,
0xe5127051,
0x70f155f3,
0x55f6e50c,
0x740960f4,
0x21919001,
0x018302f0,
0x12388075,
0x2878a483,
0x7ac3fd79,
0x1997e604,
0x20fada18,
0x80750be7,
0x9002743c,
0x80f02191,
0x01f94350,
0x30152120,
0x964306e4,
0x559643aa,
0x4324cd43,
0xcd4324ce,
0x03ce4303,
0x53088075,
0xf943f1b5,
0x7faa5304,
0x5303e020,
0xf5e4fe92,
0x759ef592,
0xc47500c1,
0x00c27500,
0x7500c575,
0xe32000c3,
0x01917503,
0x5303e020,
0xff02fe24,
0x44807512,
0x9190f3e5,
0xf6e5f022,
0xf0239190,
0x03a320e4,
0x12fdb320,
0x01448083,
0x75808312,
0xf74300d8,
0x5322e401,
0x0479fece,
0xce53fed9,
0xd91079fd,
0xfbce53fe,
0x75f7ce53,
0x02790393,
0x9375fed9,
0x009353ff,
0x53fa9653,
0xce53dfce,
0x069375ef,
0xfed90279,
0x43ff9375,
0x532201f7,
0x0479fecd,
0xcd53fed9,
0xd91079fd,
0xaa9653fe,
0x43dbcd53,
0xcd53029e,
0xbff753f7,
0x22efcd53,
0x020b30e4,
0xf753e0d2,
0x712771fe,
0xf7cd535f,
0xd2efce53,
0x05e030e1,
0xb330a3d2,
0x488212fd,
0x2201f743,
0xde792878,
0x1918f6e7,
0x78f924b8,
0xc3de7928,
0xec7066e7,
0x24b81819,
0x831222f7,
0xfecd53a4,
0x78fece53,
0x560574cf,
0xcd53fb70,
0xfdce53fd,
0x70560a74,
0xaa9653fb,
0x53dbcd53,
0x2385fbce,
0x9222859e,
0x1200e320,
0x9f438083,
0x02f94301,
0x75fef953,
0x84125480,
0x22017400,
0xe720a974,
0x02a94304,
0xfda95322,
0x30a97422,
0xa94304e7,
0xa9532202,
0xd9f922fd,
0x22fbd8fe,
0x75019275,
0xd57501d2,
0x80c17580,
0xc3740478,
0x751a8412,
0x0278c0c1,
0x84128274,
0x0091751a,
0x78059275,
0x121a7401,
0x92751a84,
0x019e759d,
0x7afd7922,
0xa397e004,
0x22fada19,
0xe0009090,
0x905801b4,
0xb4e00290,
0x00740802,
0xf0029090,
0x90902780,
0x0154e004,
0x91900f60,
0x7ade7904,
0xa397e004,
0x40fada19,
0x04909010,
0x70f254e0,
0x05909008,
0x60f554e0,
0x4aa04320,
0x54e6b078,
0xfa0ab40a,
0x91900074,
0x8312f000,
0x90027427,
0x74f00090,
0x08919001,
0xb46e80f0,
0x03740802,
0xf0009090,
0x03b46380,
0x00919038,
0x5901b4e0,
0x9002f743,
0x90e00291,
0x90f00590,
0x90e00191,
0x54f00490,
0x780c6001,
0x049190fd,
0xa318f6e0,
0x74f9f9b8,
0x00909004,
0x900174f0,
0x80f00891,
0x1a04b428,
0x1054cfe5,
0xce751f60,
0x0a964310,
0x43059643,
0x057424ce,
0xf0009090,
0x05b40b80,
0x90017408,
0x80f00090,
0x90902200,
0x01b4e001,
0x03909065,
0x0802b4e0,
0x90900074,
0x2780f003,
0xe0069090,
0x4c600154,
0x79149190,
0xe0047ade,
0xda19a397,
0x901050fa,
0x54e00690,
0x900870f2,
0x54e00790,
0x782d60f5,
0x14919028,
0x0518f0e6,
0xf824b882,
0x784ca043,
0x0c54e6b0,
0x74fa0cb4,
0x10919000,
0x5f8312f0,
0x90900274,
0x0174f001,
0xf0189190,
0x02b47780,
0x90037408,
0x80f00190,
0x2503b46c,
0xe0109190,
0x901e01b4,
0x90e01191,
0x90f00690,
0x90e01291,
0x74f00790,
0x01909004,
0x900174f0,
0x80f01891,
0x3804b444,
0x0454aee5,
0xaee53b60,
0xaef50144,
0x0254aee5,
0x03740870,
0xf0019090,
0xf7432780,
0x44cde5c0,
0xe5cdf508,
0xf5a04496,
0xf5504496,
0x24cd4396,
0x90900574,
0x0980f001,
0x740605b4,
0x01909001,
0x000022f0,
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
0x75000000,
0x023000bd,
0x75017405,
0xc0750120,
0x2485327f,
0xf4a0e5a0,
0x0a54b055,
0xb553f770,
0x2bff90ef,
0x042075e4,
0x7301bd75,
0x43012075,
0xa0d210b5,
0x22fdb030,
0x00000000,
0x00000000,
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
		if (pending == 1) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "IRQCHIP_STATE %d(%s): pending %d",
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
		pr_err("%s failed at L%d", __func__, __LINE__); \
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
 * running in IRQ context if CONFIG_SCSC_WLBT_CFG_REQ_WQ is not defined
 */
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
	unsigned int val, val1, val2, val3, val4;
	int i;

	if (platform->ka_patch_fw && !fw_compiled_in_kernel) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "ka_patch present in FW image\n");
		ka_patch_addr = platform->ka_patch_fw;
		ka_patch_addr_end = ka_patch_addr + (platform->ka_patch_len / sizeof(uint32_t));
		ka_patch_len = platform->ka_patch_len;
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "no ka_patch FW file. Use default(size:%d)\n",
				  ARRAY_SIZE(ka_patch));
		ka_patch_addr = &ka_patch[0];
		ka_patch_addr_end = ka_patch_addr + ARRAY_SIZE(ka_patch);
		ka_patch_len = ARRAY_SIZE(ka_patch);
	}

#define CHECK(x) do { \
	int retval = (x); \
	if (retval < 0) {\
		pr_err("%s failed at L%d", __func__, __LINE__); \
		goto cfg_error; \
	} \
} while (0)

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	/* Set TZPC to non-secure mode */
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

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_0, WLBT_CBUS_BAAW_0_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_0, WLBT_CBUS_BAAW_0_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_0, WLBT_MAILBOX_GNSS_WLBT >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_0, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_0, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_0, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_0, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_0, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_0(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_1, WLBT_CBUS_BAAW_1_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_1, WLBT_CBUS_BAAW_1_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_1, WLBT_MAILBOX_WLBT_ABOX >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_1, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_1, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_1, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_1, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_1, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_1(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_2, WLBT_CBUS_BAAW_2_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_2, WLBT_CBUS_BAAW_2_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_2, WLBT_MAILBOX_AP_WLBT_WL >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_2, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_2, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_2, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_2, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_2, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_2(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_3, WLBT_CBUS_BAAW_3_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_3, WLBT_CBUS_BAAW_3_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_3, WLBT_GPIO_CMGP >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_3, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_3, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_3, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_3, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_3, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_3(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_4, WLBT_CBUS_BAAW_4_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_4, WLBT_CBUS_BAAW_4_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_4, WLBT_SYSREG_CMGP2WLBT >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_4, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_4, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_4, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_4, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_4, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_4(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_5, WLBT_CBUS_BAAW_5_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_5, WLBT_CBUS_BAAW_5_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_5, WLBT_USI_CMGP0 >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_5, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_5, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_5, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_5, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_5, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_5(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_6, WLBT_CBUS_BAAW_6_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_6, WLBT_CBUS_BAAW_6_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_6, WLBT_SYSREG_COMBINE_CHUB2WLBT >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_6, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_6, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_6, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_6, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_6, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_6(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_7, WLBT_CBUS_BAAW_7_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_7, WLBT_CBUS_BAAW_7_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_7, WLBT_USI_CHUB0 >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_7, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_7, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_7, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_7, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_7, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_7(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_8, WLBT_CBUS_BAAW_8_START >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_8, WLBT_CBUS_BAAW_8_END >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_8, WLBT_CHUB_SRAM >> 12));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_8, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_8, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_8, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_8, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_8, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_8(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);


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
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated BOOT_CFG_ACK: 0x%x\n", val);

	/* Mark as CFQ_REQ handled, so boot may continue */
	platform->boot_state = WLBT_BOOT_CFG_DONE;
	goto done;

cfg_error:
	platform->boot_state = WLBT_BOOT_CFG_ERROR;
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			 "ERROR: WLBT Config failed. WLBT will not work\n");
done:
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"Complete CFG_ACK\n");
	complete(&platform->cfg_ack);
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

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "INT received, boot_state = %u\n", platform->boot_state);
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
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MIF PMU Int Handler not registered\n");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Updated BOOT_CFG_ACK\n");
		regmap_write(platform->boot_cfg, PMU_BOOT_ACK, PMU_BOOT_COMPLETE);
	}
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
	/* Reset irq_disabled_cnt for WDOG IRQ since the IRQ itself is here
	 * unregistered and disabled
	 */
	atomic_set(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt, 0);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform);
}

static int platform_mif_register_irq(struct platform_mif *platform)
{
	int err;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering IRQs\n");

	/* Register MBOX irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Registering MBOX irq: %d flag 0x%x\n",
			  platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform->wlbt_irq[PLATFORM_MIF_MBOX].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform_mif_isr,
			       platform->wlbt_irq[PLATFORM_MIF_MBOX].flags, IRQ_NAME_MBOX, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register MBOX handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Register MBOX irq WPAN */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Registering MBOX WPAN irq: %d flag 0x%x\n",
			  platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num,
			  platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num, platform_mif_isr_wpan,
			       platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].flags, IRQ_NAME_MBOX_WPAN, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register MBOX handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Register WDOG irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Registering WDOG irq: %d flag 0x%x\n", platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num,
			  platform->wlbt_irq[PLATFORM_MIF_WDOG].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform_wdog_isr,
			       platform->wlbt_irq[PLATFORM_MIF_WDOG].flags, IRQ_NAME_WDOG, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register WDOG handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Mark as WLBT in reset before enabling IRQ to guard against
	 * spurious IRQ
	 */
	platform->boot_state = WLBT_BOOT_IN_RESET;
	smp_wmb(); /* commit before irq */

	/* Register WB2AP_CFG_REQ irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Registering CFG_REQ irq: %d flag 0x%x\n", platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num,
			  platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].flags);

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

	/* done as part of platform_mif_pmu_reset_release() init_done sequence*/
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "start wait for completion\n");
	/* At this point WLBT should assert the CFG_REQ IRQ, so wait for it */
	if (wait_for_completion_timeout(&platform->cfg_ack, WLBT_BOOT_TIMEOUT) == 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "Timeout waiting for CFG_REQ IRQ\n");
		wlbt_regdump(platform);
		return -ETIMEDOUT;
	}

	/* only continue if CFG_REQ IRQ configured WLBT/PMU correctly */
	if (platform->boot_state == WLBT_BOOT_CFG_ERROR) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "CFG_REQ failed to configure WLBT.\n");
		return -EIO;
	}

	return 0;
}

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

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "on boot_state = WLBT_BOOT_WAIT_CFG_REQ\n");
	if (!init_done) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "init\n");
		ret = pmu_cal_progress(platform, pmucal_wlbt.init, pmucal_wlbt.init_size);
		if (ret < 0)
			goto done;
		init_done = 1;
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "release\n");
		ret = pmu_cal_progress(platform, pmucal_wlbt.reset_release, pmucal_wlbt.reset_release_size);
		if (ret < 0)
			goto done;
	}

	/* Now handle the CFG_REQ IRQ */
	enable_irq(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);

	/* Wait for CFG_ACK completion */
	ret = platform_mif_start_wait_for_cfg_ack_completion(interface);
done:
	return ret;
}

static int platform_mif_pmu_reset_assert(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
#ifndef CONFIG_WLBT_AUTOGEN_PMUCAL
	unsigned long       timeout;
	int                 ret;
	u32                 val;
#endif
	/* wlbt_if_reset_assertion()
	 * WLBT_INT_EN[3] = 0  <---------- different from Orange!
	 * WLBT_INT_EN[5] = 0  <---------- different from Orange!
	 * WLBT_CTRL_NS[7] = 0
	 * WLBT_CONFIGURATION[0] = 0
	 * WAIT WLBT_STATUS[0] = 0
	 */

#ifdef CONFIG_WLBT_AUTOGEN_PMUCAL
	int r = pmu_cal_progress(platform, pmucal_wlbt.reset_assert, pmucal_wlbt.reset_assert_size);

	if (r)
		wlbt_regdump(platform);
	return r;
#else

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
			return 0;
		}
	} while (time_before(jiffies, timeout));

	/* Timed out */
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			 "Timeout waiting for WLBT_STATUS status\n");
	regmap_read(platform->pmureg, WLBT_STATUS, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATUS 0x%x\n", val);
	regmap_read(platform->pmureg, WLBT_DEBUG, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_DEBUG 0x%x\n", val);
	regmap_read(platform->pmureg, WLBT_STATES, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATES 0x%x\n", val);
	return -ETIME;

#endif
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static int platform_mif_set_mem_region2
	(struct scsc_mif_abs *interface,
	void __iomem *_mem_region2,
	size_t _mem_size_region2)
{
	/* If memlogger API is enabled, mxlogger's mem should be set
	 * by another routine
	 */
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
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Not resetting ARM Cores - enable_platform_mif_arm_reset: %d\n",
				  enable_platform_mif_arm_reset);
	}
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

		/* Size passed in from .dts exceeds array */
		if (size > MIFRAMMAN_MAXMEM) {
			SCSC_TAG_ERR(PLAT_MIF,
				     "wlbt: shared DRAM requested in .dts %zd exceeds mapping table %d\n",
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
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "Error remaping shared memory\n");
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

/* HERE: Not sure why mem is passed in - its stored in platform -
 * as it should be
 */
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

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev,
			   "Getting INTMR0 0x%x target %s\n",
			   val, (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
	return val;
}

static u32 platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;

	/* Function has to return the interrupts that are enabled *AND* not
	 *  masked
	 */
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
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev,
				   "ka_patch already allocated\n");
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

#if defined(CONFIG_WLBT_DCXO_TUNE)
static void platform_mif_send_dcxo_cmd(struct scsc_mif_abs *interface, u8 opcode, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	static u8 seq = 0;

	platform_mif_reg_write_apm(platform, MAILBOX_WLBT_REG(ISSR(0)), BUILD_ISSR0_VALUE(opcode, seq));
	platform_mif_reg_write_apm(platform, MAILBOX_WLBT_REG(ISSR(1)), val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Write APM MAILBOX: 0x%x\n", val);

	seq = (seq + 1) % APM_CMD_MAX_SEQ_NUM;

	platform_mif_reg_write_apm(platform, MAILBOX_WLBT_REG(INTGR0), (1 << APM_IRQ_BIT_DCXO_SHIFT) << 16);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTGR0: bit 1 on target APM\n");
}

static int platform_mif_irq_register_mbox_apm(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	int	i;

	/* Initialise MIF registers with documented defaults */
	/* MBOXes */
	for (i = 0; i < 8; i++) {
		platform_mif_reg_write_apm(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
	}

	// INTXR0 : AP/FW -> APM , INTXR1 : APM -> AP/FW
	/* MRs */ /*1's - set bit 1 as unmasked */
	oldapm_intmr1_val = platform_mif_reg_read_apm(platform, MAILBOX_WLBT_REG(INTMR1));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "APM MAILBOX INTMR1 %x\n", oldapm_intmr1_val);
	platform_mif_reg_write_apm(platform, MAILBOX_WLBT_REG(INTMR1),
							   oldapm_intmr1_val & ~(1 << APM_IRQ_BIT_DCXO_SHIFT));
	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write_apm(platform, MAILBOX_WLBT_REG(INTCR1), (1 << APM_IRQ_BIT_DCXO_SHIFT));

	/* Register MBOX irq APM */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering MBOX APM\n");

	return 0;
}

static void platform_mif_irq_unregister_mbox_apm(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering MBOX APM irq\n");

	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write_apm(platform, MAILBOX_WLBT_REG(INTMR1), oldapm_intmr1_val);

	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write_apm(platform, MAILBOX_WLBT_REG(INTCR1), (1 << APM_IRQ_BIT_DCXO_SHIFT));
}

static int platform_mif_check_dcxo_ack(struct scsc_mif_abs *interface, u8 opcode, u32* val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long timeout;
	u32 irq_val;
	int ret;

	SCSC_TAG_INFO(PLAT_MIF, "wait for dcxo tune ack\n");

	timeout = jiffies + msecs_to_jiffies(500);
	do {
		irq_val = platform_mif_reg_read_apm(platform, MAILBOX_WLBT_REG(INTMSR1));
		if (irq_val & (1 << APM_IRQ_BIT_DCXO_SHIFT)) {
			SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "APM MAILBOX INTMSR1 %x\n", irq_val);

			irq_val = platform_mif_reg_read_apm(platform, MAILBOX_WLBT_REG(ISSR(2)));
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Read Ack for setting DCXO tune: 0x%x\n", irq_val);

			ret = (irq_val & MASK_DONE) >> SHIFT_DONE ? 0 : 1;

			if (opcode == OP_GET_TUNE && val != NULL) {
				irq_val = platform_mif_reg_read_apm(platform, MAILBOX_WLBT_REG(ISSR(3)));
				SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Read tune value for DCXO: 0x%x\n", irq_val);

				*val = irq_val;
			}

			platform_mif_reg_write_apm(platform, MAILBOX_WLBT_REG(INTCR1), (1 << APM_IRQ_BIT_DCXO_SHIFT));
			goto done;
		}
	} while (time_before(jiffies, timeout));

	SCSC_TAG_INFO(PLAT_MIF, "timeout waiting for INTMSR1 bit 1 0x%08x\n", irq_val);
	return -ECOMM;
done:
	return ret;
}
#endif

static int platform_mif_wlbt_phandle_property_read_u32(struct scsc_mif_abs *interface,
				const char *phandle_name, const char *propname, u32 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct device_node *np;
	int err = 0;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get phandle: %s\n", phandle_name);
	np = of_parse_phandle(platform->dev->of_node, phandle_name, 0);
	if (np) {
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);
		if (of_property_read_u32(np, propname, out_value)) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
			err = -EINVAL;
		}
	} else {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "phandle doesn't exist\n");
		err = -EINVAL;
	}

	return err;
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
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "Unable to get pointer reference\n");
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
		+ (LOGGING_REF_OFFSET));
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
	ref -= LOGGING_REF_OFFSET;

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
	unsigned int val;

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

	regmap_read(platform->pmureg, WLBT_CONFIGURATION, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_CONFIGURATION 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_STAT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_STAT 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_OUT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_OUT 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_IN, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_IN 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_DEBUG, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_DEBUG 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_STATUS, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_STATUS 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_STATES, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_STATES 0x%08x\n", val);

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
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "IRQCHIP_STATE %d(%s): pending %d, active %d, masked %d\n",
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
	SCSC_TAG_DEBUG(PLAT_MIF,
		       "memory reserved: mem_base=%#lx, mem_size=%zd\n",
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
	int err = 0;
	u8 i = 0;
	struct resource *reg_res;

	if (!platform)
		return NULL;

	SCSC_TAG_INFO_DEV(PLAT_MIF, &pdev->dev,
			  "Creating MIF platform device\n");

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
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	platform_if->recovery_disabled_reg = platform_recovery_disabled_reg;
	platform_if->recovery_disabled_unreg = platform_recovery_disabled_unreg;
#endif
	platform_if->get_mbox_pmu = platform_mif_get_mbox_pmu;
	platform_if->set_mbox_pmu = platform_mif_set_mbox_pmu;
	platform_if->load_pmu_fw = platform_load_pmu_fw;
	platform_if->irq_reg_pmu_handler = platform_mif_irq_reg_pmu_handler;
	platform_if->wlbt_phandle_property_read_u32 = platform_mif_wlbt_phandle_property_read_u32;
	platform->pmu_handler = platform_mif_irq_default_handler;
	platform->irq_dev_pmu = NULL;
#if defined(CONFIG_WLBT_DCXO_TUNE)
	platform_if->send_dcxo_cmd = platform_mif_send_dcxo_cmd;
	platform_if->check_dcxo_ack = platform_mif_check_dcxo_ack;
	platform_if->irq_register_mbox_apm = platform_mif_irq_register_mbox_apm;
	platform_if->irq_unregister_mbox_apm = platform_mif_irq_unregister_mbox_apm;
#endif
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
	if (!sharedmem_base) {
		struct device_node *np;

		np = of_parse_phandle(platform->dev->of_node, "memory-region", 0);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "module build register sharedmem np %x\n", np);
		if (np) {
			platform->mem_start = of_reserved_mem_lookup(np)->base;
			platform->mem_size = of_reserved_mem_lookup(np)->size;
		}
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "built-in register sharedmem\n");
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
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "platform->mem_start 0x%x platform->mem_size 0x%x\n",
			  (u32)platform->mem_start, (u32)platform->mem_size);
	if (platform->mem_start == 0)
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev,
				     "platform->mem_start is 0");

	if (platform->mem_size == 0) {
		/* We return return if mem_size is 0 as it does not make any
		 * sense. This may be an indication of an incorrect platform
		 * device binding.
		 */
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "platform->mem_size is 0");
		err = -EINVAL;
		goto error_exit;
	}

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

#if defined(CONFIG_WLBT_DCXO_TUNE)
	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	platform->base_apm = devm_ioremap_resource(&pdev->dev, reg_res);
	if (IS_ERR(platform->base_apm)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error getting mem resource for MAILBOX_APM\n");
		err = PTR_ERR(platform->base_apm);
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
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MBOX_WLAN irq %d flag 0x%x\n",
					  (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_MBOX;
		} else if (!strcmp(irq_res->name, "MBOX_WPAN")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MBOX_WPAN irq %d flag 0x%x\n",
					  (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_MBOX_WPAN;
		} else if (!strcmp(irq_res->name, "ALIVE")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "ALIVE irq %d flag 0x%x\n",
					  (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_ALIVE;
		} else if (!strcmp(irq_res->name, "WDOG")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "WDOG irq %d flag 0x%x\n",
					  (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_WDOG;
		} else if (!strcmp(irq_res->name, "CFG_REQ")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "CFG_REQ irq %d flag 0x%x\n",
					  (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_CFG_REQ;
		} else {
			SCSC_TAG_ERR_DEV(PLAT_MIF, &pdev->dev,
					 "Invalid irq res name: %s\n",
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

#ifdef CONFIG_SCSC_QOS
	platform_mif_parse_qos(platform, platform->dev->of_node);
#endif
	/* Initialize spinlock */
	spin_lock_init(&platform->mif_spinlock);

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	platform->cfgreq_workq =
			 create_singlethread_workqueue("wlbt_cfg_reg_work");
	if (!platform->cfgreq_workq) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "Error creating CFG_REQ singlethread_workqueue\n");
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

	enable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num);
	enable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num);


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

	disable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num);
	disable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num);

	/* Restore the MIF registers.
	 * This must be done first as the resume_handler may use the MIF.
	 */
	platform_mif_reg_restore(platform);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Set TCXO_GATE bit\n");
	/* Clear WLBT_ACTIVE_CLR flag in WLBT_CTRL_NS */
	ret = regmap_write_bits(platform->pmureg, WLBT_CTRL_NS  | 0xc000, TCXO_GATE, TCXO_GATE);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[TCXO_GATE]: %d\n", ret);
	}

	if (platform->resume_handler)
		platform->resume_handler(interface, platform->suspendresume_data);
}

static void power_supplies_on(struct platform_mif *platform)
{
	/* The APM IPC in FW will be used instead */
	if (disable_apm_setup) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "WLBT LDOs firmware controlled\n");
		return;
	}
}
