/*
 * Platform Dependent file for Samsung Exynos
 *
 * Copyright (C) 2021, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id$
 */
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/bug.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#if defined(CONFIG_SOC_EXYNOS8895) || defined(CONFIG_SOC_EXYNOS9810) || \
	defined(CONFIG_SOC_EXYNOS9820) || defined(CONFIG_SOC_EXYNOS9830) || \
	defined(CONFIG_SOC_EXYNOS2100) || defined(CONFIG_SOC_EXYNOS1000) || \
	defined(CONFIG_SOC_S5E9925)
#include <linux/exynos-pci-ctrl.h>
#endif /* CONFIG_SOC_EXYNOS8895 || CONFIG_SOC_EXYNOS9810 ||
	* CONFIG_SOC_EXYNOS9820 || CONFIG_SOC_EXYNOS9830 ||
	* CONFIG_SOC_EXYNOS2100 || CONFIG_SOC_EXYNOS1000 ||
	* CONFIG_SOC_S5E9925
	*/

#if defined(CONFIG_64BIT)
#include <asm-generic/gpio.h>
#endif /* CONFIG_64BIT */

#ifdef BCMDHD_MODULAR
#if IS_ENABLED(CONFIG_SEC_SYSFS)
#include <linux/sec_sysfs.h>
#endif /* CONFIG_SEC_SYSFS */
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif /* CONFIG_SEC_SYSFS */
#else
#if defined(CONFIG_SEC_SYSFS)
#include <linux/sec_sysfs.h>
#elif defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif /* CONFIG_SEC_SYSFS */
#endif /* BCMDHD_MODULAR */
#include <linux/wlan_plat.h>

#if defined(CONFIG_MACH_A7LTE) || defined(CONFIG_NOBLESSE)
#define PINCTL_DELAY 150
#endif /* CONFIG_MACH_A7LTE || CONFIG_NOBLESSE */

#define EXYNOS_PCIE_VENDOR_ID 0x144d
#if defined(CONFIG_MACH_UNIVERSAL7420) || defined(CONFIG_SOC_EXYNOS7420)
#define EXYNOS_PCIE_DEVICE_ID 0xa575
#define EXYNOS_PCIE_CH_NUM 1
#elif defined(CONFIG_SOC_EXYNOS8890)
#define EXYNOS_PCIE_DEVICE_ID 0xa544
#define EXYNOS_PCIE_CH_NUM 0
#elif defined(CONFIG_SOC_EXYNOS8895) || defined(CONFIG_SOC_EXYNOS9810) || \
	defined(CONFIG_SOC_EXYNOS9820) || defined(CONFIG_SOC_EXYNOS9830) || \
	defined(CONFIG_SOC_EXYNOS2100) || defined(CONFIG_SOC_EXYNOS1000) || \
	defined(CONFIG_SOC_S5E9925)
#define EXYNOS_PCIE_DEVICE_ID 0xecec
#define EXYNOS_PCIE_CH_NUM 0
#else
#error "Not supported platform"
#endif

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
extern void dhd_exit_wlan_mem(void);
extern int dhd_init_wlan_mem(void);
extern void *dhd_wlan_mem_prealloc(int section, unsigned long size);
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

#define WIFI_TURNON_DELAY	200
static int wlan_pwr_on = -1;

#ifdef CONFIG_BCMDHD_OOB_HOST_WAKE
static int wlan_host_wake_irq = 0;
static unsigned int wlan_host_wake_up = -1;
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE */

#if defined(CONFIG_MACH_A7LTE) || defined(CONFIG_NOBLESSE)
extern struct device *mmc_dev_for_wlan;
#endif /* CONFIG_MACH_A7LTE || CONFIG_NOBLESSE */

#ifdef CONFIG_BCMDHD_PCIE
extern int exynos_pcie_pm_resume(int);
extern void exynos_pcie_pm_suspend(int);
extern int exynos_pcie_l1_exit(int ch_num);
#endif /* CONFIG_BCMDHD_PCIE */

#if defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS9110)
extern struct mmc_host *wlan_mmc;
extern void mmc_ctrl_power(struct mmc_host *host, bool onoff);
#endif /* SOC_EXYNOS7870 || CONFIG_SOC_EXYNOS9110 */

#ifndef SUPPORT_EXYNOS7420
#include <linux/exynos-pci-noti.h>
extern int exynos_pcie_register_event(struct exynos_pcie_register_event *reg);
extern int exynos_pcie_deregister_event(struct exynos_pcie_register_event *reg);
#endif /* !SUPPORT_EXYNOS7420 */

#ifdef EXYNOS_PCIE_DEBUG
extern void exynos_pcie_register_dump(int ch_num);
#endif /* EXYNOS_PCIE_DEBUG */
#ifdef PRINT_WAKEUP_GPIO_STATUS
extern void exynos_pin_dbg_show(unsigned int pin, const char* str);
#endif /* PRINT_WAKEUP_GPIO_STATUS */

#include <dhd_plat.h>

typedef struct dhd_plat_info {
	struct exynos_pcie_register_event pcie_event;
	struct exynos_pcie_notify pcie_notify;
	struct pci_dev *pdev;
} dhd_plat_info_t;

static dhd_pcie_event_cb_t g_pfn = NULL;

#define DHD_DT_COMPAT_ENTRY		"samsung,brcm-wlan"

char* dhd_get_device_dt_name(void)
{
	return DHD_DT_COMPAT_ENTRY;
}

uint32 dhd_plat_get_info_size(void)
{
	return sizeof(dhd_plat_info_t);
}

void plat_pcie_notify_cb(struct exynos_pcie_notify *pcie_notify)
{
	struct pci_dev *pdev;

	if (pcie_notify == NULL) {
		pr_err("%s(): Invalid argument to Platform layer call back \r\n", __func__);
		return;
	}

	if (g_pfn) {
		pdev = (struct pci_dev *)pcie_notify->user;
		pr_err("%s(): Invoking DHD call back with pdev %p \r\n",
				__func__, pdev);
		(*(g_pfn))(pdev);
	} else {
		pr_err("%s(): Driver Call back pointer is NULL \r\n", __func__);
	}
	return;
}

int dhd_plat_pcie_register_event(void *plat_info, struct pci_dev *pdev, dhd_pcie_event_cb_t pfn)
{
		dhd_plat_info_t *p = plat_info;

#ifndef SUPPORT_EXYNOS7420
		if ((p == NULL) || (pdev == NULL) || (pfn == NULL)) {
			pr_err("%s(): Invalid argument p %p, pdev %p, pfn %p\r\n",
				__func__, p, pdev, pfn);
			return -1;
		}
		g_pfn = pfn;
		p->pdev = pdev;
		p->pcie_event.events = EXYNOS_PCIE_EVENT_LINKDOWN;
		p->pcie_event.user = pdev;
		p->pcie_event.mode = EXYNOS_PCIE_TRIGGER_CALLBACK;
		p->pcie_event.callback = plat_pcie_notify_cb;
		exynos_pcie_register_event(&p->pcie_event);
		pr_err("%s(): Registered Event PCIe event pdev %p \r\n", __func__, pdev);
		return 0;
#else
		return 0;
#endif /* SUPPORT_EXYNOS7420 */
}

void dhd_plat_pcie_deregister_event(void *plat_info)
{
	dhd_plat_info_t *p = plat_info;
#ifndef SUPPORT_EXYNOS7420
	if (p) {
		exynos_pcie_deregister_event(&p->pcie_event);
	}
#endif /* SUPPORT_EXYNOS7420 */
	return;
}

static int
dhd_wlan_power(int onoff)
{
#if defined(CONFIG_MACH_A7LTE) || defined(CONFIG_NOBLESSE)
	struct pinctrl *pinctrl = NULL;
#endif /* CONFIG_MACH_A7LTE || ONFIG_NOBLESSE */

	printk(KERN_INFO"%s Enter: WL_REG_ON power %s\n", __FUNCTION__, onoff ? "on" : "off");

#if defined(CONFIG_MACH_A7LTE) || defined(CONFIG_NOBLESSE)
	if (onoff) {
		pinctrl = devm_pinctrl_get_select(mmc_dev_for_wlan, "sdio_wifi_on");
		if (IS_ERR(pinctrl))
			printk(KERN_INFO "%s WLAN SDIO GPIO control error\n", __FUNCTION__);
		msleep(PINCTL_DELAY);
	}
#endif /* CONFIG_MACH_A7LTE || CONFIG_NOBLESSE */

	if (gpio_direction_output(wlan_pwr_on, onoff)) {
		printk(KERN_ERR "%s failed to control WLAN_REG_ON to %s\n",
			__FUNCTION__, onoff ? "HIGH" : "LOW");
		return -EIO;
	}

#if defined(CONFIG_MACH_A7LTE) || defined(CONFIG_NOBLESSE)
	if (!onoff) {
		pinctrl = devm_pinctrl_get_select(mmc_dev_for_wlan, "sdio_wifi_off");
		if (IS_ERR(pinctrl))
			printk(KERN_INFO "%s WLAN SDIO GPIO control error\n", __FUNCTION__);
	}
#endif /* CONFIG_MACH_A7LTE || CONFIG_NOBLESSE */

#if defined(CONFIG_SOC_EXYNOS7870) || defined(CONFIG_SOC_EXYNOS9110)
	if (wlan_mmc)
		mmc_ctrl_power(wlan_mmc, onoff);
#endif /* SOC_EXYNOS7870 || CONFIG_SOC_EXYNOS9110 */
	return 0;
}

static int
dhd_wlan_reset(int onoff)
{
	return 0;
}

#ifndef CONFIG_BCMDHD_PCIE
extern void (*notify_func_callback)(void *dev_id, int state);
extern void *mmc_host_dev;
#endif /* !CONFIG_BCMDHD_PCIE */

static int
dhd_wlan_set_carddetect(int val)
{
#ifndef CONFIG_BCMDHD_PCIE
	pr_err("%s: notify_func=%p, mmc_host_dev=%p, val=%d\n",
		__FUNCTION__, notify_func_callback, mmc_host_dev, val);

	if (notify_func_callback) {
		notify_func_callback(mmc_host_dev, val);
	} else {
		pr_warning("%s: Nobody to notify\n", __FUNCTION__);
	}
#else
	if (val) {
		exynos_pcie_pm_resume(EXYNOS_PCIE_CH_NUM);
	} else {
		printk(KERN_INFO "%s Ignore carddetect: %d\n", __FUNCTION__, val);
	}
#endif /* CONFIG_BCMDHD_PCIE */

	return 0;
}

int
dhd_wlan_init_gpio(void)
{
	const char *wlan_node = DHD_DT_COMPAT_ENTRY;
	struct device_node *root_node = NULL;
	struct device *wlan_dev;

	printk(KERN_INFO "%s\n", __FUNCTION__);

	wlan_dev = sec_device_create(NULL, "wlan");

	root_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!root_node) {
		WARN(1, "failed to get device node of bcm4354\n");
		return -ENODEV;
	}

	if (!of_device_is_available(root_node)) {
		printk(KERN_ERR "%s: brcm wlan device status is disable\n", __FUNCTION__);
		return -ENXIO;
	}

	/* ========== WLAN_PWR_EN ============ */
	wlan_pwr_on = of_get_gpio(root_node, 0);
	if (!gpio_is_valid(wlan_pwr_on)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_pwr_on);
		return -ENODEV;
	}

	if (gpio_request(wlan_pwr_on, "WLAN_REG_ON")) {
		WARN(1, "fail to request gpio(WLAN_REG_ON)\n");
		return -ENODEV;
	}
#ifdef CONFIG_BCMDHD_PCIE
	gpio_direction_output(wlan_pwr_on, 1);
	msleep(WIFI_TURNON_DELAY);
	printk(KERN_INFO "%s: WL_REG_ON High sleep done:%d msec\n",
		__FUNCTION__, WIFI_TURNON_DELAY);
#else
	gpio_direction_output(wlan_pwr_on, 0);
#endif /* CONFIG_BCMDHD_PCIE */

	gpio_export(wlan_pwr_on, 1);
	if (wlan_dev)
		gpio_export_link(wlan_dev, "WLAN_REG_ON", wlan_pwr_on);

#ifdef CONFIG_BCMDHD_OOB_HOST_WAKE
	/* ========== WLAN_HOST_WAKE ============ */
	wlan_host_wake_up = of_get_gpio(root_node, 1);
	if (!gpio_is_valid(wlan_host_wake_up)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_host_wake_up);
		return -ENODEV;
	}

	if (gpio_request(wlan_host_wake_up, "WLAN_HOST_WAKE")) {
		WARN(1, "fail to request gpio(WLAN_HOST_WAKE)\n");
		return -ENODEV;
	}
	gpio_direction_input(wlan_host_wake_up);
	gpio_export(wlan_host_wake_up, 1);
	if (wlan_dev)
		gpio_export_link(wlan_dev, "WLAN_HOST_WAKE", wlan_host_wake_up);

	wlan_host_wake_irq = gpio_to_irq(wlan_host_wake_up);
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE */

	return 0;
}

#if defined(CONFIG_BCMDHD_OOB_HOST_WAKE) && defined(CONFIG_BCMDHD_GET_OOB_STATE)
int
dhd_get_wlan_oob_gpio(void)
{
	return gpio_is_valid(wlan_host_wake_up) ?
		gpio_get_value(wlan_host_wake_up) : -1;
}
EXPORT_SYMBOL(dhd_get_wlan_oob_gpio);
int
dhd_get_wlan_oob_gpio_number(void)
{
	return gpio_is_valid(wlan_host_wake_up) ?
		wlan_host_wake_up : -1;
}
EXPORT_SYMBOL(dhd_get_wlan_oob_gpio_number);
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE && CONFIG_BCMDHD_GET_OOB_STATE */

struct resource dhd_wlan_resources = {
	.name	= "bcmdhd_wlan_irq",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE |
	IORESOURCE_IRQ_HIGHLEVEL,
};
EXPORT_SYMBOL(dhd_wlan_resources);

struct wifi_platform_data dhd_wlan_control = {
	.set_power	= dhd_wlan_power,
	.set_reset	= dhd_wlan_reset,
	.set_carddetect	= dhd_wlan_set_carddetect,
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= dhd_wlan_mem_prealloc,
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
};
EXPORT_SYMBOL(dhd_wlan_control);

int
dhd_wlan_init(void)
{
	int ret;

	printk(KERN_INFO "%s: START.......\n", __FUNCTION__);

	ret = dhd_wlan_init_gpio();
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to initiate GPIO, ret=%d\n",
			__FUNCTION__, ret);
		goto fail;
	}

#ifdef CONFIG_BCMDHD_OOB_HOST_WAKE
	dhd_wlan_resources.start = wlan_host_wake_irq;
	dhd_wlan_resources.end = wlan_host_wake_irq;
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE */

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	ret = dhd_init_wlan_mem();
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to alloc reserved memory,"
			" ret=%d\n", __FUNCTION__, ret);
	}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

fail:
	return ret;
}

int
dhd_wlan_deinit(void)
{
#ifdef CONFIG_BCMDHD_OOB_HOST_WAKE
	gpio_free(wlan_host_wake_up);
#endif /* CONFIG_BCMDHD_OOB_HOST_WAKE */
	gpio_free(wlan_pwr_on);

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	dhd_exit_wlan_mem();
#endif /*  CONFIG_BROADCOM_WIFI_RESERVED_MEM */
	return 0;
}

void dhd_plat_l1ss_ctrl(bool ctrl)
{
#if defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS9820) || \
	defined(CONFIG_SOC_EXYNOS9830) || defined(CONFIG_SOC_EXYNOS2100) || \
	defined(CONFIG_SOC_EXYNOS1000) || defined(CONFIG_SOC_S5E9925)
	printk(KERN_ERR "%s: Control L1ss EP side %d \n", __FUNCTION__, ctrl);
#if !defined(CONFIG_SOC_S5E9925)
	exynos_pcie_l1ss_ctrl(ctrl, PCIE_L1SS_CTRL_WIFI);
#else /* CONFIG_SOC_S5E9925 */
	exynos_pcie_l1ss_ctrl(ctrl, PCIE_L1SS_CTRL_WIFI, EXYNOS_PCIE_CH_NUM);
#endif /* !CONFIG_SOC_S5E9925 */
#endif /* CONFIG_SOC_EXYNOS9810 || CONFIG_SOC_EXYNOS9820 ||
	* CONFIG_SOC_EXYNOS9830 || CONFIG_SOC_EXYNOS2100 ||
	* CONFIG_SOC_EXYNOS1000 || CONFIG_SOC_S5E9925
	*/
	return;
}

#if defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS9820) || \
	defined(CONFIG_SOC_EXYNOS9830)
#define DHD_PCIE_L1_EXIT_DURING_IO
#endif /* CONFIG_SOC_EXYNOS9810 || CONFIG_SOC_EXYNOS9820
	* CONFIG_SOC_EXYNOS9830
	*/

void dhd_plat_l1_exit_io(void)
{
#if defined(DHD_PCIE_L1_EXIT_DURING_IO)
	exynos_pcie_l1_exit(EXYNOS_PCIE_CH_NUM);
#endif /* DHD_PCIE_L1_EXIT_DURING_IO */
	return;
}

void dhd_plat_l1_exit(void)
{
	exynos_pcie_l1_exit(EXYNOS_PCIE_CH_NUM);
	return;
}

int dhd_plat_pcie_suspend(void *plat_info)
{
#if !defined(SUPPORT_EXYNOS7420)
	exynos_pcie_pm_suspend(EXYNOS_PCIE_CH_NUM);
#endif
	return 0;
}

int dhd_plat_pcie_resume(void *plat_info)
{
	int ret = 0;
#if !defined(SUPPORT_EXYNOS7420)
	ret = exynos_pcie_pm_resume(EXYNOS_PCIE_CH_NUM);
#endif
	return ret;
}

void dhd_plat_pin_dbg_show(void *plat_info)
{
#ifdef PRINT_WAKEUP_GPIO_STATUS
	exynos_pin_dbg_show(dhd_get_wlan_oob_gpio_number(), "gpa0");
#endif /* PRINT_WAKEUP_GPIO_STATUS */
}

void dhd_plat_pcie_register_dump(void *plat_info)
{
#ifdef EXYNOS_PCIE_DEBUG
	exynos_pcie_register_dump(1);
#endif /* EXYNOS_PCIE_DEBUG */
}

uint32 dhd_plat_get_rc_vendor_id(void)
{
	return EXYNOS_PCIE_VENDOR_ID;
}

uint32 dhd_plat_get_rc_device_id(void)
{
	return EXYNOS_PCIE_DEVICE_ID;
}

#ifndef BCMDHD_MODULAR
#if defined(CONFIG_MACH_UNIVERSAL7420) || defined(CONFIG_SOC_EXYNOS8890) || \
	defined(CONFIG_SOC_EXYNOS8895) || defined(CONFIG_SOC_EXYNOS9810) || \
	defined(CONFIG_SOC_EXYNOS9820) || defined(CONFIG_SOC_EXYNOS9830)
#if defined(CONFIG_DEFERRED_INITCALLS)
deferred_module_init(dhd_wlan_init);
#else
late_initcall(dhd_wlan_init);
#endif /* CONFIG_DEFERRED_INITCALLS */
#else
device_initcall(dhd_wlan_init);
#endif /* CONFIG Exynos PCIE Platforms */
#endif /* !BCMDHD_MODULAR */
