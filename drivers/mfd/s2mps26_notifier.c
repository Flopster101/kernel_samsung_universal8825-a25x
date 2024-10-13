/*
 * s2mps26-notifier.c - Interrupt controller support for S2MPS26
 *
 * Copyright (C) 2021 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/err.h>
#include <linux/mfd/samsung/s2mps26.h>
#include <linux/mfd/samsung/s2mps26-regulator.h>
#include <linux/notifier.h>
#include <linux/irq.h>
#include <linux/regulator/pmic_class.h>

#define BUCK_SR1S_IDX (12)

static struct notifier_block sub_pmic_notifier;
static struct s2mps26_dev *s2mps26_global;
static uint8_t irq_reg[S2MPS26_IRQ_GROUP_NR] = {0};
static int s2mps26_buck_oi_cnt[S2MPS26_BUCK_MAX]; /* BUCK 1~12, SR1S */
static int s2mps26_buck_ocp_cnt[S2MPS26_BUCK_MAX]; /* BUCK 1~12, SR1S */
static int s2mps26_bb_oi_cnt; /* BUCK-BOOST */
static int s2mps26_bb_ocp_cnt; /* BUCK-BOOST */
static int s2mps26_temp_cnt[S2MPS26_TEMP_MAX]; /* Temp */

/* add IRQ handling func here */
static void s2mps26_temp_irq(struct s2mps26_dev *s2mps26, int degree)
{
	int temp = 0;
	s2mps26_temp_cnt[degree]++;

	if (degree == 0)
		temp = 120;
	else
		temp = 140;

	pr_info("%s: S2MPS26 thermal %dC IRQ: %d\n",
		__func__, temp, s2mps26_temp_cnt[degree]);
}

static void s2mps26_bb_ocp_irq(struct s2mps26_dev *s2mps26)
{
	s2mps26_bb_ocp_cnt++;

	pr_info("%s: S2MPS26 BUCKBOOST OCP IRQ: %d\n",
		__func__, s2mps26_bb_ocp_cnt);
}

static void s2mps26_buck_ocp_irq(struct s2mps26_dev *s2mps26, int buck)
{
	s2mps26_buck_ocp_cnt[buck]++;
	if (buck == BUCK_SR1S_IDX)
		pr_info("%s: S2MPS26 BUCK_SR[%d] OCP IRQ: %d\n",
			__func__, buck + 1 - BUCK_SR1S_IDX, s2mps26_buck_ocp_cnt[buck]);
	else
		pr_info("%s: S2MPS26 BUCK[%d] OCP IRQ: %d\n",
			__func__, buck + 1, s2mps26_buck_ocp_cnt[buck]);
}

static void s2mps26_bb_oi_irq(struct s2mps26_dev *s2mps26)
{
	s2mps26_bb_oi_cnt++;

	pr_info("%s: S2MPS26 BUCKBOOST OI IRQ: %d\n",
		__func__, s2mps26_bb_oi_cnt);
}

static void s2mps26_buck_oi_irq(struct s2mps26_dev *s2mps26, int buck)
{
	s2mps26_buck_oi_cnt[buck]++;
	if (buck == BUCK_SR1S_IDX)
		pr_info("%s: S2MPS26 BUCK_SR[%d] OI IRQ: %d\n",
			__func__, buck + 1 - BUCK_SR1S_IDX, s2mps26_buck_oi_cnt[buck]);
	else
		pr_info("%s: S2MPS26 BUCK[%d] OI IRQ: %d\n",
			__func__, buck + 1, s2mps26_buck_oi_cnt[buck]);
}

static const uint8_t s2mps26_get_irq_mask[] = {
	/* OI */
	[S2MPS26_IRQ_OI_B1S] = S2MPS26_PMIC_IRQ_OI_B1_MASK,
	[S2MPS26_IRQ_OI_B2S] = S2MPS26_PMIC_IRQ_OI_B2_MASK,
	[S2MPS26_IRQ_OI_B3S] = S2MPS26_PMIC_IRQ_OI_B3_MASK,
	[S2MPS26_IRQ_OI_B4S] = S2MPS26_PMIC_IRQ_OI_B4_MASK,
	[S2MPS26_IRQ_OI_B5S] = S2MPS26_PMIC_IRQ_OI_B5_MASK,
	[S2MPS26_IRQ_OI_B6S] = S2MPS26_PMIC_IRQ_OI_B6_MASK,
	[S2MPS26_IRQ_OI_B7S] = S2MPS26_PMIC_IRQ_OI_B7_MASK,
	[S2MPS26_IRQ_OI_B8S] = S2MPS26_PMIC_IRQ_OI_B8_MASK,
	[S2MPS26_IRQ_OI_B9S] = S2MPS26_PMIC_IRQ_OI_B9_MASK,
	[S2MPS26_IRQ_OI_B10S] = S2MPS26_PMIC_IRQ_OI_B10_MASK,
	[S2MPS26_IRQ_OI_B11S] = S2MPS26_PMIC_IRQ_OI_B11_MASK,
	[S2MPS26_IRQ_OI_B12S] = S2MPS26_PMIC_IRQ_OI_B12_MASK,
	[S2MPS26_IRQ_OI_SR1S] = S2MPS26_PMIC_IRQ_OI_SR1S_MASK,
	[S2MPS26_IRQ_OI_BBS] = S2MPS26_PMIC_IRQ_OI_BB_MASK,
	/* OCP */
	[S2MPS26_IRQ_OCP_B1S] = S2MPS26_PMIC_IRQ_OCP_B1_MASK,
	[S2MPS26_IRQ_OCP_B2S] = S2MPS26_PMIC_IRQ_OCP_B2_MASK,
	[S2MPS26_IRQ_OCP_B3S] = S2MPS26_PMIC_IRQ_OCP_B3_MASK,
	[S2MPS26_IRQ_OCP_B4S] = S2MPS26_PMIC_IRQ_OCP_B4_MASK,
	[S2MPS26_IRQ_OCP_B5S] = S2MPS26_PMIC_IRQ_OCP_B5_MASK,
	[S2MPS26_IRQ_OCP_B6S] = S2MPS26_PMIC_IRQ_OCP_B6_MASK,
	[S2MPS26_IRQ_OCP_B7S] = S2MPS26_PMIC_IRQ_OCP_B7_MASK,
	[S2MPS26_IRQ_OCP_B8S] = S2MPS26_PMIC_IRQ_OCP_B8_MASK,
	[S2MPS26_IRQ_OCP_B9S] = S2MPS26_PMIC_IRQ_OCP_B9_MASK,
	[S2MPS26_IRQ_OCP_B10S] = S2MPS26_PMIC_IRQ_OCP_B10_MASK,
	[S2MPS26_IRQ_OCP_B11S] = S2MPS26_PMIC_IRQ_OCP_B11_MASK,
	[S2MPS26_IRQ_OCP_B12S] = S2MPS26_PMIC_IRQ_OCP_B12_MASK,
	[S2MPS26_IRQ_OCP_SR1S] = S2MPS26_PMIC_IRQ_OCP_SR1S_MASK,
	[S2MPS26_IRQ_OCP_BBS] = S2MPS26_PMIC_IRQ_OCP_BB_MASK,
	/* Temp */
	[S2MPS26_IRQ_INT120C] = S2MPS26_IRQ_INT120C_MASK,
	[S2MPS26_IRQ_INT140C] = S2MPS26_IRQ_INT140C_MASK,
};

static void s2mps26_call_interrupt(struct s2mps26_dev *s2mps26)
{
	size_t i;
	uint8_t reg = 0;

	/* OI interrupt */
	for (i = 0; i < S2MPS26_BUCK_MAX; i++) {
		reg = S2MPS26_IRQ_OI_B1S + i;
		if (reg < S2MPS26_IRQ_OI_B9S) {
			if (irq_reg[S2MPS26_PMIC_INT4] & s2mps26_get_irq_mask[reg])
				s2mps26_buck_oi_irq(s2mps26, i);
		} else
			if (irq_reg[S2MPS26_PMIC_INT5] & s2mps26_get_irq_mask[reg])
				s2mps26_buck_oi_irq(s2mps26, i);
	}

	/* OCP interrupt */
	for (i = 0; i < S2MPS26_BUCK_MAX; i++) {
		reg = S2MPS26_IRQ_OCP_B1S + i;
		if (reg < S2MPS26_IRQ_OCP_B9S) {
			if (irq_reg[S2MPS26_PMIC_INT2] & s2mps26_get_irq_mask[reg])
				s2mps26_buck_ocp_irq(s2mps26, i);
		} else
			if (irq_reg[S2MPS26_PMIC_INT3] & s2mps26_get_irq_mask[reg])
				s2mps26_buck_ocp_irq(s2mps26, i);
	}

	/* BUCK-BOOST OI interrupt */
	if (irq_reg[S2MPS26_PMIC_INT5] & s2mps26_get_irq_mask[S2MPS26_IRQ_OI_BBS])
		s2mps26_bb_oi_irq(s2mps26);

	/* BUCK-BOOST OCP interrupt */
	if (irq_reg[S2MPS26_PMIC_INT3] & s2mps26_get_irq_mask[S2MPS26_IRQ_OCP_BBS])
		s2mps26_bb_ocp_irq(s2mps26);

	/* Thermal interrupt */
	for (i = 0; i < S2MPS26_TEMP_MAX; i++) {
		reg = S2MPS26_IRQ_INT120C + i;

		if (irq_reg[S2MPS26_PMIC_INT1] & s2mps26_get_irq_mask[reg])
			s2mps26_temp_irq(s2mps26, i);
	}
}

static void s2mps26_irq_work_func(struct work_struct *work)
{
	char buf[1024] = {0, };
	int i, cnt = 0, irq_cnt = 0;

	if (s2mps26_global->pmic_rev & S2MPS26_CHIP_ID_HW_MASK)
		irq_cnt = S2MPS26_NUM_IRQ_PMIC_REGS;
	else
		irq_cnt = S2MPS26_NUM_IRQ_PMIC_REGS_EVT0;

	for (i = 0; i < irq_cnt; i++)
		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "INT%d(0x%02hhx) ", i + 1, irq_reg[i]);

	pr_info("[PMIC] %s: sub pmic interrupt %s\n", __func__, buf);
}

static int s2mps26_notifier_handler(struct notifier_block *nb,
				    unsigned long action,
				    void *data)
{
	int ret, irq_cnt;
	struct s2mps26_dev *s2mps26 = data;

	if (!s2mps26) {
		pr_err("%s: fail to load dev.\n", __func__);
		return IRQ_HANDLED;
	}

	mutex_lock(&s2mps26->irqlock);

	/* Read interrupt */
	if (s2mps26->pmic_rev & S2MPS26_CHIP_ID_HW_MASK)
		irq_cnt = S2MPS26_NUM_IRQ_PMIC_REGS;
	else
		irq_cnt = S2MPS26_NUM_IRQ_PMIC_REGS_EVT0;

	ret = s2mps26_bulk_read(s2mps26->pmic1, S2MPS26_REG_INT1,
				irq_cnt, &irq_reg[S2MPS26_PMIC_INT1]);
	if (ret) {
		pr_err("%s: fail to read INT sources\n", __func__);
		mutex_unlock(&s2mps26->irqlock);

		return IRQ_HANDLED;
	}

	queue_delayed_work(s2mps26->irq_wqueue, &s2mps26->irq_work, 0);

	/* Call interrupt */
	s2mps26_call_interrupt(s2mps26);

	mutex_unlock(&s2mps26->irqlock);

	return IRQ_HANDLED;
}

static BLOCKING_NOTIFIER_HEAD(s2mps26_notifier);

static int s2mps26_register_notifier(struct notifier_block *nb,
			      struct s2mps26_dev *s2mps26)
{
	int ret;

	ret = blocking_notifier_chain_register(&s2mps26_notifier, nb);
	if (ret < 0)
		pr_err("%s: fail to register notifier\n", __func__);

	return ret;
}

void s2mps26_call_notifier(void)
{
	blocking_notifier_call_chain(&s2mps26_notifier, 0, s2mps26_global);
}
EXPORT_SYMBOL(s2mps26_call_notifier);

static const uint8_t s2mps26_mask_reg_evt0[] = {
	[S2MPS26_PMIC_INT1] = S2MPS26_REG_INT1M_EVT0,
	[S2MPS26_PMIC_INT2] = S2MPS26_REG_INT2M_EVT0,
	[S2MPS26_PMIC_INT3] = S2MPS26_REG_INT3M_EVT0,
	[S2MPS26_PMIC_INT4] = S2MPS26_REG_INT4M_EVT0,
	[S2MPS26_PMIC_INT5] = S2MPS26_REG_INT5M_EVT0,
	[S2MPS26_PMIC_INT6] = S2MPS26_REG_INT6M_EVT0,
};

static const uint8_t s2mps26_mask_reg[] = {
	[S2MPS26_PMIC_INT1] = S2MPS26_REG_INT1M,
	[S2MPS26_PMIC_INT2] = S2MPS26_REG_INT2M,
	[S2MPS26_PMIC_INT3] = S2MPS26_REG_INT3M,
	[S2MPS26_PMIC_INT4] = S2MPS26_REG_INT4M,
	[S2MPS26_PMIC_INT5] = S2MPS26_REG_INT5M,
	[S2MPS26_PMIC_INT6] = S2MPS26_REG_INT6M,
	[S2MPS26_PMIC_INT7] = S2MPS26_REG_INT7M,
};

static int s2mps26_unmask_interrupt(struct s2mps26_dev *s2mps26)
{
	int ret;
	/* unmask IRQM interrupt */
	ret = s2mps26_update_reg(s2mps26->i2c, S2MPS26_REG_TX_MASK,
				 0x00, S2MPS26_PM_IRQM_MASK);
	if (ret)
		goto err;

	if (s2mps26->pmic_rev & S2MPS26_CHIP_ID_HW_MASK) {
		/* unmask OI interrupt */
		ret = s2mps26_write_reg(s2mps26->pmic1, S2MPS26_REG_INT4M, 0x00);
		if (ret)
			goto err;
		ret = s2mps26_update_reg(s2mps26->pmic1, S2MPS26_REG_INT5M, 0x00, 0x3F);
		if (ret)
			goto err;

		/* unmask OCP interrupt */
		ret = s2mps26_write_reg(s2mps26->pmic1, S2MPS26_REG_INT2M, 0x00);
		if (ret)
			goto err;
		ret = s2mps26_update_reg(s2mps26->pmic1, S2MPS26_REG_INT3M, 0x00, 0x3F);
		if (ret)
			goto err;

		/* unmask Temp interrupt */
		ret = s2mps26_update_reg(s2mps26->pmic1, S2MPS26_REG_INT1M, 0x00, 0x0C);
		if (ret)
			goto err;
	} else {
		/* unmask OI interrupt */
		ret = s2mps26_write_reg(s2mps26->pmic1, S2MPS26_REG_INT4M_EVT0, 0x00);
		if (ret)
			goto err;
		ret = s2mps26_update_reg(s2mps26->pmic1, S2MPS26_REG_INT5M_EVT0, 0x00, 0x3F);
		if (ret)
			goto err;

		/* unmask OCP interrupt */
		ret = s2mps26_write_reg(s2mps26->pmic1, S2MPS26_REG_INT2M_EVT0, 0x00);
		if (ret)
			goto err;
		ret = s2mps26_update_reg(s2mps26->pmic1, S2MPS26_REG_INT3M_EVT0, 0x00, 0x3F);
		if (ret)
			goto err;

		/* unmask Temp interrupt */
		ret = s2mps26_update_reg(s2mps26->pmic1, S2MPS26_REG_INT1M_EVT0, 0x00, 0x0C);
		if (ret)
			goto err;
	}

	return 0;
err:
	return -1;
}

static int s2mps26_set_interrupt(struct s2mps26_dev *s2mps26)
{
	uint8_t i, val;
	int ret;

	/* Mask all the interrupt sources */
	if (s2mps26->pmic_rev & S2MPS26_CHIP_ID_HW_MASK) {
		for (i = 0; i < S2MPS26_NUM_IRQ_PMIC_REGS; i++) {
			ret = s2mps26_write_reg(s2mps26->pmic1, s2mps26_mask_reg[i], 0xFF);
			if (ret)
				goto err;
		}
	} else {
		for (i = 0; i < S2MPS26_NUM_IRQ_PMIC_REGS_EVT0; i++) {
			ret = s2mps26_write_reg(s2mps26->pmic1, s2mps26_mask_reg_evt0[i], 0xFF);
			if (ret)
				goto err;
		}
	}
	//ret = s2mps26_update_reg(s2mps26->i2c, S2MPS26_REG_TX_MASK,
	//			 S2MPS26_PM_IRQM_MASK, S2MPS26_PM_IRQM_MASK);
	//if (ret)
	//	goto err;

	/* Unmask interrupt sources */
	ret = s2mps26_unmask_interrupt(s2mps26);
	if (ret < 0) {
		pr_err("%s: s2mps26_unmask_interrupt fail\n", __func__);
		goto err;
	}

	/* Check unmask interrupt register */
	if (s2mps26->pmic_rev & S2MPS26_CHIP_ID_HW_MASK) {
		for (i = 0; i < S2MPS26_NUM_IRQ_PMIC_REGS; i++) {
			ret = s2mps26_read_reg(s2mps26->pmic1, s2mps26_mask_reg[i], &val);
			if (ret)
				goto err;
			pr_info("%s: INT%dM = 0x%02hhx\n", __func__, i + 1, val);
		}
	} else {
		for (i = 0; i < S2MPS26_NUM_IRQ_PMIC_REGS_EVT0; i++) {
			ret = s2mps26_read_reg(s2mps26->pmic1, s2mps26_mask_reg_evt0[i], &val);
			if (ret)
				goto err;
			pr_info("%s: INT%dM = 0x%02hhx\n", __func__, i + 1, val);
		}
	}
	return 0;
err:
	return -1;
}

static int s2mps26_set_wqueue(struct s2mps26_dev *s2mps26)
{
	static char device_name[32] = {0, };

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-wqueue@%s",
		 dev_driver_string(s2mps26->dev), dev_name(s2mps26->dev));

	s2mps26->irq_wqueue = create_singlethread_workqueue(device_name);
	if (!s2mps26->irq_wqueue) {
		pr_err("%s: fail to create workqueue\n", __func__);
		return -1;
	}

	INIT_DELAYED_WORK(&s2mps26->irq_work, s2mps26_irq_work_func);

	return 0;
}

static void s2mps26_set_notifier(struct s2mps26_dev *s2mps26)
{
	sub_pmic_notifier.notifier_call = s2mps26_notifier_handler;
	s2mps26_register_notifier(&sub_pmic_notifier, s2mps26);
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t irq_read_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int i, cnt = 0;

	cnt += snprintf(buf + cnt, PAGE_SIZE, "------ INTERRUPTS (/pmic/%s) ------\n",
			dev_driver_string(s2mps26_global->dev));

	for (i = 0; i < S2MPS26_BUCK_MAX; i++) {
		if (i == BUCK_SR1S_IDX)
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK_SR%d_OCP_IRQ:\t\t%5d\n",
					i + 1 - BUCK_SR1S_IDX, s2mps26_buck_ocp_cnt[i]);
		else
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK%d_OCP_IRQ:\t\t%5d\n",
					i + 1, s2mps26_buck_ocp_cnt[i]);
	}
	cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCKBOOST_OCP_IRQ:\t%5d\n",
			s2mps26_bb_ocp_cnt);

	for (i = 0; i < S2MPS26_BUCK_MAX; i++) {
		if (i == BUCK_SR1S_IDX)
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK_SR%d_OI_IRQ:\t\t%5d\n",
					i + 1 - BUCK_SR1S_IDX, s2mps26_buck_oi_cnt[i]);
		else
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK%d_OI_IRQ:\t\t%5d\n",
					i + 1, s2mps26_buck_oi_cnt[i]);

	}
	cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCKBOOST_OI_IRQ:\t%5d\n",
			s2mps26_bb_oi_cnt);

	for (i = 0; i < S2MPS26_TEMP_MAX; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE, "TEMP_%d_IRQ:\t\t%5d\n",
				i ? 140 : 120, s2mps26_temp_cnt[i]);

	return cnt;
}

static struct pmic_device_attribute irq_attr[] = {
	PMIC_ATTR(irq_read_all, S_IRUGO, irq_read_show, NULL),
};

static int s2mps26_create_irq_sysfs(struct s2mps26_dev *s2mps26)
{
	struct device *s2mps26_irq_dev;
	struct device *dev = s2mps26->dev;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-irq@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mps26_irq_dev = pmic_device_create(s2mps26, device_name);
	s2mps26->irq_dev = s2mps26_irq_dev;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(irq_attr); i++) {
		err = device_create_file(s2mps26_irq_dev, &irq_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mps26_irq_dev, &irq_attr[i].dev_attr);
	pmic_device_destroy(s2mps26_irq_dev->devt);

	return -ENODEV;
}
#endif

int s2mps26_notifier_init(struct s2mps26_dev *s2mps26)
{
	s2mps26_global = s2mps26;
	mutex_init(&s2mps26->irqlock);

	/* Register notifier */
	s2mps26_set_notifier(s2mps26);

	/* Set workqueue */
	if (s2mps26_set_wqueue(s2mps26) < 0) {
		pr_err("%s: s2mps26_set_wqueue fail\n", __func__);
		goto err;
	}

	/* Set interrupt */
	if (s2mps26_set_interrupt(s2mps26) < 0) {
		pr_err("%s: s2mps26_set_interrupt fail\n", __func__);
		goto err;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	if (s2mps26_create_irq_sysfs(s2mps26) < 0) {
		pr_info("%s: failed to create sysfs\n", __func__);
		goto err;
	}
#endif
	return 0;
err:
	return -1;
}
EXPORT_SYMBOL_GPL(s2mps26_notifier_init);

void s2mps26_notifier_deinit(struct s2mps26_dev *s2mps26)
{
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mps26_irq_dev = s2mps26->irq_dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(irq_attr); i++)
		device_remove_file(s2mps26_irq_dev, &irq_attr[i].dev_attr);
	pmic_device_destroy(s2mps26_irq_dev->devt);
#endif
}
EXPORT_SYMBOL(s2mps26_notifier_deinit);
