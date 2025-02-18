/*
 * exynos-ssp.c - Samsung Secure Platform driver for the Exynos
 *
 * Copyright (C) 2019 Samsung Electronics
 * Keunyoung Park <keun0.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/smc.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/debugfs.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/exynos-pd.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>

#define SSP_RET_OK		0
#define SSP_RET_FAIL		-1
#define SSP_RET_BUSY		0x20010	/* defined at LDFW */
#define SSP_MAX_USER_CNT	100
#define SSP_MAX_USER_PATH_LEN	128
#define SSP_K250_STABLE_TIME_MS    20
#define SSP_V9RR_STABLE_TIME_MS    10

/* smc to call ldfw functions */
#define SMC_CMD_SSP		(0xC2001040)
#define SSP_CMD_BOOT		(0x1)
#define SSP_CMD_BACKUP		(0x2)
#define SSP_CMD_RESTORE		(0x3)
#define SSP_CMD_SELF_TEST	(0x10)
#define SSP_CMD_BOOT_INIT	(0x11)
#define SSP_CMD_BOOT_CHECK	(0x12)
#define SSP_CMD_RESTORE_INIT	(0x31)
#define SSP_CMD_RESTORE_CHECK	(0x32)

/* ioctl command for TAC */
#define SSP_IOCTL_MAGIC		'c'
#define SSP_IOCTL_INIT		_IO(SSP_IOCTL_MAGIC, 1)
#define SSP_IOCTL_EXIT		_IOWR(SSP_IOCTL_MAGIC, 2, uint64_t)
#define SSP_IOCTL_TEST		_IOWR(SSP_IOCTL_MAGIC, 3, uint64_t)
#define SSP_IOCTL_DEBUG		_IOWR(SSP_IOCTL_MAGIC, 4, uint64_t)

#define ssp_err(dev, fmt, arg...)	printk("[EXYNOS][CMSSP][ERROR] " fmt, ##arg)
#define ssp_info(dev, fmt, arg...)	printk("[EXYNOS][CMSSP][ INFO] " fmt, ##arg)

/* SFR for ssp power control */
#define PMU_ALIVE_PA_BASE		(0x15860000 + 0x2000)
#define PMU_SSP_STATUS_VA_OFFSET	(0x304)
#define PMU_SSP_STATUS_MASK		(1 << 0)

void __iomem *pmu_va_base;

spinlock_t ssp_lock;
struct mutex ssp_ioctl_lock;
static int ssp_power_count;
static int ssp_idle_ip_index;
extern struct exynos_chipid_info exynos_soc_info;

struct ssp_device {
	struct device *dev;
	struct miscdevice misc_device;
	struct regulator *secure_nvm_ldo;
	unsigned long snvm_init_time;
	int gpio_se_sel;
	int gpio_snvm_reset;
	int is_k250;
};

struct ssp_user {
	char path[SSP_MAX_USER_PATH_LEN];
	unsigned long init_count;
	unsigned long init_time;
	unsigned long exit_count;
	unsigned long exit_time;
	unsigned long init_fail_count;
	unsigned long init_fail_time;
	unsigned long exit_fail_count;
	unsigned long exit_fail_time;
	struct ssp_user *next;
};

struct ssp_user *head = NULL;

static void exynos_ssp_print_user_list(struct device *dev)
{
	struct ssp_user *ptr = head;
	int i = 0;

	ssp_info(dev, "====== Power control status ================="
		      "=============================================\n");
	ssp_info(dev, "No: %8s:\t%8s\t%8s\t%16s\t%16s\n",
		      "Caller", "ON", "OFF", "ON-TIME", "OFF-TIME");
	ssp_info(dev, "---------------------------------------------"
		      "---------------------------------------------\n");

	while (ptr != NULL) {
		i++;

		ssp_info(dev, "%2u:\t%8s: %s\n",
			i,
			"Path",
			ptr->path);

		ssp_info(dev, "%2u:\t%8s:\t%8u\t%8u\t%16u\t%16u\n",
			i,
			"Success",
			ptr->init_count,
			ptr->exit_count,
			ptr->init_time,
			ptr->exit_time);

		ssp_info(dev, "%2u:\t%8s:\t%8u\t%8u\t%16u\t%16u\n",
			i,
			"Fail",
			ptr->init_fail_count,
			ptr->exit_fail_count,
			ptr->init_fail_time,
			ptr->exit_fail_time);

		ptr = ptr->next;
	}
	ssp_info(dev, "---------------------------------------------"
		      "---------------------------------------------\n");
}

static int exynos_ssp_get_path(struct device *dev, struct task_struct *task, char *task_path)
{
	int ret = SSP_RET_OK;
	char *buf;
	char *path;
	struct file *exe_file;

	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf) {
		ssp_err(dev, "%s: fail to __get_free_page.\n", __func__);
		return -ENOMEM;
	}

	exe_file = get_task_exe_file(task);
	if (!exe_file) {
		ret = -ENOENT;
		ssp_err(dev, "%s: fail to get_task_exe_file.\n", __func__);
		goto end;
	}

	path = d_path(&exe_file->f_path, buf, PAGE_SIZE);
	if (IS_ERR(path)) {
		ret = PTR_ERR(path);
		ssp_err(dev, "%s: fail to d_path. ret = 0x%x\n", __func__, ret);
		goto end;
	}

	memset(task_path, 0, SSP_MAX_USER_PATH_LEN);
	strncpy(task_path, path, SSP_MAX_USER_PATH_LEN - 1);
end:
	free_page((unsigned long)buf);

	return ret;
}

static struct ssp_user *exynos_ssp_find_user(char *path)
{
	struct ssp_user *now = head;

	if (head == NULL)
		return NULL;

	while (strncmp(now->path, path, SSP_MAX_USER_PATH_LEN)) {
		if (now->next == NULL)
			return NULL;

		now = now->next;
	}

	return now;
}

static int exynos_ssp_powerctl_update(struct device *dev, bool power_on, bool pass)
{
	int ret = SSP_RET_OK;
	char path[SSP_MAX_USER_PATH_LEN];
	static int ssp_user_count;
	struct ssp_user *link = NULL;

	ret = exynos_ssp_get_path(dev, current, path);
	if (ret) {
		ssp_err(dev, "%s: fail to get path. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	link = exynos_ssp_find_user(path);

	if (link == NULL) {
		if (++ssp_user_count >= SSP_MAX_USER_CNT) {
			ssp_err(dev, "%s: exceed max user count.\n", __func__);
			return -ENOMEM;
		}

		link = (struct ssp_user *)kzalloc(sizeof(struct ssp_user), GFP_KERNEL);
		if (!link) {
			ssp_err(dev, "%s: fail to kzalloc.\n", __func__);
			return -ENOMEM;
		}

		memcpy(link->path, path, sizeof(path));
		link->init_count = 0;
		link->exit_count = 0;

		link->next = head;
		head = link;
	}

	if (power_on == 1 && pass == 1) {
		link->init_count++;
		link->init_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	} else if (power_on == 1 && pass == 0) {
		link->init_fail_count++;
		link->init_fail_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	} else if (power_on == 0 && pass == 1) {
		link->exit_count++;
		link->exit_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	} else if (power_on == 0 && pass == 0) {
		link->exit_fail_count++;
		link->exit_fail_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	}

	return ret;
}

static int exynos_cm_smc(uint64_t *arg0, uint64_t *arg1,
			 uint64_t *arg2, uint64_t *arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(*arg0, *arg1, *arg2, *arg3, 0, 0, 0, 0, &res);

	*arg0 = res.a0;
	*arg1 = res.a1;
	*arg2 = res.a2;
	*arg3 = res.a3;

	return *arg0;
}

static int exynos_ssp_map_sfr(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	pmu_va_base = ioremap(PMU_ALIVE_PA_BASE, SZ_4K);
	if (!pmu_va_base) {
		ssp_err(sspdev->dev, "%s: fail to ioremap\n", __func__);
		ret = SSP_RET_FAIL;
	}

	return ret;
}

static bool exynos_ssp_check_power_status(struct device *dev)
{
	unsigned int reg;

	if (!pmu_va_base) {
		ssp_err(dev, "%s: invalid pmu_va_base\n", __func__);
		return false;
	}

	reg = readl(pmu_va_base + PMU_SSP_STATUS_VA_OFFSET);
	if (reg & PMU_SSP_STATUS_MASK)
		return true;

	reg = readl(pmu_va_base + PMU_SSP_STATUS_VA_OFFSET);
	if (reg & PMU_SSP_STATUS_MASK)
		return true;

	ssp_err(dev, "%s: power hasn't been enabled: 0x%x\n", __func__, reg);
	ssp_err(dev, "%s: pmu address: 0x%x\n", __func__, pmu_va_base + PMU_SSP_STATUS_VA_OFFSET);

	return false;
}

static void exynos_ssp_pm_enable(struct ssp_device *sspdev)
{
	pm_runtime_enable(sspdev->dev);
	ssp_info(sspdev->dev, "pm_runtime_enable\n");
}

static int exynos_ssp_power_on(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	ret = pm_runtime_get_sync(sspdev->dev);
	if (ret != SSP_RET_OK)
		ssp_err(sspdev->dev, "%s: fail to pm_runtime_get_sync. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(sspdev->dev, "pm_runtime_get_sync done\n");

	if (exynos_ssp_check_power_status(sspdev->dev) == false) {
		ssp_err(sspdev->dev, "%s: ssp power status\n", __func__);
		return SSP_RET_FAIL;
	}

	return ret;
}

static int exynos_ssp_power_off(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	ret = pm_runtime_put_sync(sspdev->dev);
	if (ret != SSP_RET_OK)
		ssp_err(sspdev->dev, "%s: fail to pm_runtime_put_sync. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(sspdev->dev, "pm_runtime_put_sync done\n");

	return ret;
}

static int exynos_ssp_boot_init(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	uint64_t count;
	unsigned long flag;

	ssp_info(dev, "ssp boot start\n");

	exynos_update_ip_idle_status(ssp_idle_ip_index, 0);
	count = 0;

	do {
		count++;

		reg0 = SMC_CMD_SSP;
		reg1 = SSP_CMD_BOOT_INIT;
		reg2 = 0;
		reg3 = 0;

		spin_lock_irqsave(&ssp_lock, flag);
		ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
		spin_unlock_irqrestore(&ssp_lock, flag);

		if (ret == SSP_RET_BUSY)
			usleep_range(500, 1000);

	} while (ret == SSP_RET_BUSY);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to boot at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp boot done: %d\n", count);

	return ret;
}

static int exynos_ssp_boot_check(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	uint64_t count;
	unsigned long flag;

	ssp_info(dev, "ssp boot check start\n");

	count = 0;

	do {
		count++;

		reg0 = SMC_CMD_SSP;
		reg1 = SSP_CMD_BOOT_CHECK;
		reg2 = 0;
		reg3 = 0;

		spin_lock_irqsave(&ssp_lock, flag);
		ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
		spin_unlock_irqrestore(&ssp_lock, flag);

		if (ret == SSP_RET_BUSY)
			usleep_range(500, 1000);

	} while (ret == SSP_RET_BUSY);

	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to boot check at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp boot check done: %d\n", count);

	return ret;
}

static int exynos_ssp_backup(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(dev, "ssp backup start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_BACKUP;
	reg2 = 0;
	reg3 = 0;

	exynos_update_ip_idle_status(ssp_idle_ip_index, 0);

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to backup at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp backup done\n");

	return ret;
}

static int exynos_ssp_restore_init(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(dev, "ssp restore init start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_RESTORE_INIT;
	reg2 = 0;
	reg3 = 0;

	exynos_update_ip_idle_status(ssp_idle_ip_index, 0);

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to restore check at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp restore init done\n");

	return ret;
}

static int exynos_ssp_restore_check(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(dev, "ssp restore check start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_RESTORE_CHECK;
	reg2 = 0;
	reg3 = 0;

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to restore check at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp restore check done\n");

	return ret;
}

static int exynos_ssp_self_test(struct device *dev, uint64_t test_mode)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(dev, "call ssp function: %d\n", (int)test_mode);

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_SELF_TEST;
	reg2 = test_mode;
	reg3 = 0;

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	ssp_info(dev, "return from ldfw: 0x%x\n", ret);

	return ret;
}

static int exynos_se_power_on_phase1(struct ssp_device *dev)
{
	int ret = SSP_RET_OK;

	ssp_info(dev, "Secure NVM on\n");

	if (IS_ERR(dev->secure_nvm_ldo) || regulator_enable(dev->secure_nvm_ldo) < 0) {
		ssp_err(dev, "%s - failed to enable LDO for SE\n", __func__);
		return -ENODEV;
	}
	if (dev->is_k250) {
		if (dev->gpio_snvm_reset >= 0) {
			/* GPC5.1 set to 1 if controllable */
			gpio_set_value(dev->gpio_snvm_reset, 1);
		}
	}
	dev->snvm_init_time = ktime_get_boottime_ns();

	return ret;
}

static int exynos_se_power_on_phase2(struct ssp_device *dev)
{
	int ret = SSP_RET_OK;
	unsigned long delay;

	/* delay for chip stabilization */
	delay = (ktime_get_boottime_ns() - dev->snvm_init_time) / NSEC_PER_MSEC;

	if (dev->is_k250) {
		if (delay < SSP_K250_STABLE_TIME_MS) {
			mdelay(SSP_K250_STABLE_TIME_MS - delay);
		}

		if (dev->gpio_se_sel >= 0) {
			/* GPC1.0 set to 1 if controllable */
			gpio_set_value(dev->gpio_se_sel, 1);
		}
	} else {
		if (delay < SSP_V9RR_STABLE_TIME_MS) {
			mdelay(SSP_V9RR_STABLE_TIME_MS - delay);
		}

		if (dev->gpio_se_sel >= 0) {
			/* GPC1.0 set to 0 if controllable */
			gpio_set_value(dev->gpio_se_sel, 0);
		}
	}
	ssp_info(dev, "Secure NVM on\n");
	return ret;
}

static int exynos_se_power_off(struct ssp_device *dev)
{
	int ret = SSP_RET_OK;

	ssp_info(dev, "Secure NVM off\n");

	if (dev->is_k250) {
		if (dev->gpio_se_sel >= 0) {
			/* GPC1.0 set to 1 if controllable */
			gpio_set_value(dev->gpio_se_sel, 1);
		}
	}

	if (IS_ERR(dev->secure_nvm_ldo) || regulator_disable(dev->secure_nvm_ldo) < 0) {
		ssp_err(dev, "%s - failed to disable LDO for SE\n", __func__);
		return -ENODEV;
	}

	if (dev->is_k250) {
		mdelay(SSP_K250_STABLE_TIME_MS);
	} else {
		mdelay(SSP_V9RR_STABLE_TIME_MS);
	}

	ssp_info(dev, "Secure NVM off done\n");

	return ret;
}

static int exynos_ssp_enable(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;
	static int ssp_boot_flag;

	++ssp_power_count;

	if (ssp_power_count == 1) {
		pm_stay_awake(sspdev->dev);

		if (!ssp_boot_flag) {
			ret = exynos_se_power_on_phase1(sspdev);
			if (unlikely(ret))
				goto ERR_OUT1;

			/* step1: write FW base & size to mailbox for boot */
			ret = exynos_ssp_boot_init(sspdev->dev);
			if (unlikely(ret))
				goto ERR_OUT2;

			/* step2: power on */
			ret = exynos_ssp_power_on(sspdev);
			if (unlikely(ret))
				goto ERR_OUT2;

			/* step3: check booting status */
			ret = exynos_ssp_boot_check(sspdev->dev);
			if (unlikely(ret))
				goto ERR_OUT3;

			ret = exynos_se_power_on_phase2(sspdev);
			if (unlikely(ret))
				goto ERR_OUT3;

			ssp_boot_flag = 1;
		} else {
			ret = exynos_se_power_on_phase1(sspdev);
			if (unlikely(ret))
				goto ERR_OUT1;

			/* step1: write FW base & size to mailbox for boot */
			ret = exynos_ssp_restore_init(sspdev->dev);
			if (unlikely(ret))
				goto ERR_OUT2;

			/* step2: power on */
			ret = exynos_ssp_power_on(sspdev);
			if (unlikely(ret))
				goto ERR_OUT2;

			/* step3: check booting status */
			ret = exynos_ssp_restore_check(sspdev->dev);
			if (unlikely(ret))
				goto ERR_OUT3;

			ret = exynos_se_power_on_phase2(sspdev);
			if (unlikely(ret))
				goto ERR_OUT3;
		}
	}

	exynos_ssp_powerctl_update(sspdev->dev, 1, 1);
	ssp_info(sspdev->dev, "ssp enable: count: %d\n", ssp_power_count);

	return ret;

ERR_OUT3:
	exynos_ssp_power_off(sspdev);

ERR_OUT2:
	exynos_se_power_off(sspdev);

ERR_OUT1:
	exynos_ssp_powerctl_update(sspdev->dev, 1, 0);
	pm_relax(sspdev->dev);
	--ssp_power_count;

	return ret;
}

static int exynos_ssp_disable(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	if (ssp_power_count <= 0) {
		ssp_err(sspdev->dev, "%s ssp has already been disabled\n", __func__);
		ssp_err(sspdev->dev, "ssp disable: count: %d\n", ssp_power_count);
		return ret;
	}

	--ssp_power_count;

	if (ssp_power_count == 0) {
		ret = exynos_ssp_backup(sspdev->dev);
		if (unlikely(ret))
			goto ERR_OUT1;

		ret = exynos_ssp_power_off(sspdev);
		if (unlikely(ret))
			goto ERR_OUT2;

		ret = exynos_se_power_off(sspdev);
		if (unlikely(ret))
			goto ERR_OUT3;

		/* keep the wake-up lock when above two functions fail */
		/* for debugging purpose */

		pm_relax(sspdev->dev);
	}

	exynos_ssp_powerctl_update(sspdev->dev, 0, 1);
	ssp_info(sspdev->dev, "ssp disable: count: %d\n", ssp_power_count);

	return ret;

ERR_OUT1:
	exynos_ssp_power_off(sspdev);

ERR_OUT2:
	exynos_se_power_off(sspdev);

ERR_OUT3:
	exynos_ssp_powerctl_update(sspdev->dev, 0, 0);
	pm_relax(sspdev->dev);

	return ret;
}

static long ssp_ioctl(struct file *filp, unsigned int cmd, unsigned long __arg)
{
	int ret = SSP_RET_OK;
	uint64_t test_mode;
	char path[SSP_MAX_USER_PATH_LEN];

	struct ssp_device *sspdev = filp->private_data;
	void __user *arg = (void __user *)__arg;

	ret = exynos_ssp_get_path(sspdev->dev, current, path);
	if (ret) {
		ssp_err(sspdev->dev, "%s: fail to get user path. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	ssp_info(sspdev->dev, "requested by %s\n", path);

	mutex_lock(&ssp_ioctl_lock);

	switch (cmd) {
	case SSP_IOCTL_INIT:
		ret = exynos_ssp_enable(sspdev);
		if (ret != SSP_RET_OK)
			exynos_ssp_print_user_list(sspdev->dev);
		break;
	case SSP_IOCTL_EXIT:
		ret = exynos_ssp_disable(sspdev);
		if (ret != SSP_RET_OK)
			exynos_ssp_print_user_list(sspdev->dev);
		break;
	case SSP_IOCTL_TEST:
		ret = get_user(test_mode, (uint64_t __user *)arg);
		if (unlikely(ret)) {
			ssp_err(sspdev->dev, "%s: fail to get_user. ret = 0x%x\n", __func__, ret);
			break;
		}
		ret = exynos_ssp_self_test(sspdev->dev, test_mode);
		break;
	case SSP_IOCTL_DEBUG:
		ssp_info(sspdev->dev, "power-on count: %d\n", ssp_power_count);
		exynos_ssp_print_user_list(sspdev->dev);
		break;
	default:
		ssp_err(sspdev->dev, "%s: invalid ioctl cmd: 0x%x\n", __func__, cmd);
		ret = -EPERM;
		break;
	}

	mutex_unlock(&ssp_ioctl_lock);

	return ret;
}

static int ssp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct ssp_device *sspdev = container_of(misc,
			struct ssp_device, misc_device);

	file->private_data = sspdev;

	ssp_info(sspdev->dev, "driver open is done\n");

	return 0;
}

static const struct file_operations ssp_fops = {
	.owner		= THIS_MODULE,
	.open		= ssp_open,
	.unlocked_ioctl	= ssp_ioctl,
	.compat_ioctl	= ssp_ioctl,
};

static int exynos_ssp_probe(struct platform_device *pdev)
{
	int ret = SSP_RET_OK;
	struct ssp_device *sspdev = NULL;

	sspdev = kzalloc(sizeof(struct ssp_device), GFP_KERNEL);
	if (!sspdev) {
		ssp_err(&pdev->dev, "%s: fail to kzalloc.\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, sspdev);
	sspdev->dev = &pdev->dev;

	spin_lock_init(&ssp_lock);
	mutex_init(&ssp_ioctl_lock);

	exynos_ssp_map_sfr(sspdev);

	/* get regulator */
	sspdev->secure_nvm_ldo = devm_regulator_get_optional(&(pdev->dev), "vdd_se");
	if (IS_ERR(sspdev->secure_nvm_ldo)) {
		ssp_err(sspdev->dev, "%s: failed to get regulator", __func__);
		ret = -ENODEV;
		goto err;
	}

	/* get SE_SEL GPIO number */
	sspdev->gpio_se_sel = of_get_named_gpio(pdev->dev.of_node, "se_sel-gpios", 0);

	/* get SNVM_RESET GPIO number */
	sspdev->gpio_snvm_reset = of_get_named_gpio(pdev->dev.of_node, "snvm_reset-gpios", 0);

	if (sspdev->gpio_se_sel < 0 || sspdev->gpio_snvm_reset < 0) {
		/* If GPIO cannot be controlled, default is K250AF. */
		ssp_info(sspdev->dev, "Default SE is K250AF.", __func__);
		sspdev->is_k250 = 1;
	} else {
		/* request GPIO */
		ssp_info(sspdev->dev, "se_sel gpio : %d\n", sspdev->gpio_se_sel);
		if (devm_gpio_request(&pdev->dev, sspdev->gpio_se_sel, "se_sel") < 0) {
			ssp_err(sspdev->dev, "%s: failed to request se_sel gpio", __func__);
			ret = -ENODEV;
			goto err;
		}

		/* change GPIO direction to output */
		if (gpio_direction_output(sspdev->gpio_se_sel, 0) < 0) {
			ssp_err(sspdev->dev, "%s: failed to set gpio_output for SE_SEL\n", __func__);
			ret = -ENODEV;
			goto err;
		}

		/* request GPIO */
		ssp_info(sspdev->dev, "snvm_reset gpio : %d\n", sspdev->gpio_snvm_reset);
		if (devm_gpio_request(&pdev->dev, sspdev->gpio_snvm_reset, "snvm_reset") < 0) {
			ssp_err(sspdev->dev, "%s: failed to request snvm_reset gpio", __func__);
			ret = -ENODEV;
			goto err;
		}

		/* change GPIO direction to output */
		if (gpio_direction_output(sspdev->gpio_snvm_reset, 0) < 0) {
			ssp_err(sspdev->dev, "%s: failed to set gpio_output for reset\n", __func__);
			ret = -ENODEV;
			goto err;
		}

		/* set SE to K250 */
		sspdev->is_k250 = 1;
	}

	/* enable runtime PM */
	exynos_ssp_pm_enable(sspdev);
	ssp_idle_ip_index = exynos_get_idle_ip_index(dev_name(sspdev->dev), 1);
	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	/* set misc driver */
	memset((void *)&sspdev->misc_device, 0, sizeof(struct miscdevice));
	sspdev->misc_device.minor = MISC_DYNAMIC_MINOR;
	sspdev->misc_device.name = "ssp";
	sspdev->misc_device.fops = &ssp_fops;
	ret = misc_register(&sspdev->misc_device);
	if (ret) {
		ssp_err(sspdev->dev, "%s: fail to misc_register. ret = %d\n", __func__, ret);
		ret = -ENOMEM;
		goto err;
	}

	ret = device_init_wakeup(sspdev->dev, true);
	if (ret) {
		ssp_err(sspdev->dev, "%s: fail to init wakeup. ret = %d\n", __func__, ret);
		goto err;
	}

	return SSP_RET_OK;
err:
	kfree(sspdev);

	return ret;
}

static int exynos_ssp_remove(struct platform_device *pdev)
{
	struct ssp_device *sspdev = platform_get_drvdata(pdev);

	misc_deregister(&sspdev->misc_device);
	kfree(sspdev);

	return 0;
}

#if defined(CONFIG_PM_SLEEP) || defined(CONFIG_PM_RUNTIME)
static int exynos_ssp_suspend(struct device *dev)
{
	int ret = SSP_RET_OK;

	/* Do nothing */

	return ret;
}

static int exynos_ssp_resume(struct device *dev)
{
	int ret = SSP_RET_OK;

	/* Do nothing */

	return ret;
}
#endif

static struct dev_pm_ops exynos_ssp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(exynos_ssp_suspend,
			exynos_ssp_resume)
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_ssp_match[] = {
	{
		.compatible = "samsung,exynos-ssp",
	},
	{},
};
#endif

static struct platform_driver exynos_ssp_driver = {
	.probe		= exynos_ssp_probe,
	.remove		= exynos_ssp_remove,
	.driver		= {
		.name	= "ssp",
		.owner	= THIS_MODULE,
		.pm     = &exynos_ssp_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = exynos_ssp_match,
#endif
	},
};

static int __init exynos_ssp_init(void)
{
	int ret = SSP_RET_OK;

	ret = platform_driver_register(&exynos_ssp_driver);
	if (ret) {
		pr_err("[Exynos][CMSSP] %s: fail to register driver. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	pr_info("[Exynos][CMSSP] driver, (c) 2019 Samsung Electronics\n");

	return 0;
}

static void __exit exynos_ssp_exit(void)
{
	platform_driver_unregister(&exynos_ssp_driver);
}

module_init(exynos_ssp_init);
module_exit(exynos_ssp_exit);

MODULE_DESCRIPTION("EXYNOS Samsung Secure Platform driver");
MODULE_AUTHOR("Keunyoung Park <keun0.park@samsung.com>");
MODULE_LICENSE("GPL");
