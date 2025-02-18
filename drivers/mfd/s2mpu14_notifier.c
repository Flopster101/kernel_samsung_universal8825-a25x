/*
 * s2mpu14-notifier.c - Interrupt controller support for S2MPU14
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
#include <linux/mfd/samsung/s2mpu14.h>
#include <linux/mfd/samsung/s2mpu14-regulator.h>
#include <linux/notifier.h>
#include <linux/irq.h>

static struct notifier_block sub_pmic_notifier;
static struct s2mpu14_dev *s2mpu14_global;
static u8 irq_reg[S2MPU14_IRQ_GROUP_NR] = {0};

static int s2mpu14_ldo_oi_cnt[S2MPU14_LDO_MAX]; /* LDO (6, 10, 11, 23) */
static int s2mpu14_buck_oi_cnt[S2MPU14_BUCK_MAX]; /* BUCK 1~8 */
static int s2mpu14_buck_ocp_cnt[S2MPU14_BUCK_MAX]; /* BUCK 1~8 */
static int s2mpu14_bb_oi_cnt; /* BUCK-BOOST */
static int s2mpu14_bb_ocp_cnt; /* BUCK-BOOST */
static int s2mpu14_temp_cnt[S2MPU14_TEMP_MAX]; /* Temp */

/* add IRQ handling func here */
static void s2mpu14_temp_irq(struct s2mpu14_dev *s2mpu14, int degree)
{
	int temp = 0;

	s2mpu14_temp_cnt[degree]++;

	if (degree == 0)
		temp = 120;
	else
		temp = 140;

	pr_info("%s: S2MPU14 thermal %dC IRQ: %d\n",
		__func__, temp, s2mpu14_temp_cnt[degree]);
}

static void s2mpu14_bb_ocp_irq(struct s2mpu14_dev *s2mpu14)
{
	s2mpu14_bb_ocp_cnt++;

	pr_info("%s: S2MPU14 BUCKBOOST OCP IRQ: %d\n",
		__func__, s2mpu14_bb_ocp_cnt);
}

static void s2mpu14_buck_ocp_irq(struct s2mpu14_dev *s2mpu14, int buck)
{
	s2mpu14_buck_ocp_cnt[buck]++;

	pr_info("%s: S2MPU14 BUCK[%d] OCP IRQ: %d\n",
		__func__, buck + 1, s2mpu14_buck_ocp_cnt[buck]);
}

static void s2mpu14_bb_oi_irq(struct s2mpu14_dev *s2mpu14)
{
	s2mpu14_bb_oi_cnt++;

	pr_info("%s: S2MPU14 BUCKBOOST OI IRQ: %d\n",
		__func__, s2mpu14_bb_oi_cnt);
}

static void s2mpu14_buck_oi_irq(struct s2mpu14_dev *s2mpu14, int buck)
{
	s2mpu14_buck_oi_cnt[buck]++;

	pr_info("%s: S2MPU14 BUCK[%d] OI IRQ: %d\n",
		__func__, buck + 1, s2mpu14_buck_oi_cnt[buck]);
}

static void s2mpu14_ldo_oi_irq(struct s2mpu14_dev *s2mpu14, int ldo)
{
	int ldo_oi_arr[S2MPU14_LDO_MAX] = {6, 10, 11, 23};

	s2mpu14_ldo_oi_cnt[ldo]++;

	pr_info("%s: S2MPU14 LDO[%d] OI IRQ: %d\n",
		__func__, ldo_oi_arr[ldo], s2mpu14_ldo_oi_cnt[ldo]);
}

static const u8 s2mpu14_get_irq_mask[] = {
	/* OI */
	[S2MPU14_IRQ_OI_B1S] = S2MPU14_PMIC_IRQ_OI_B1_MASK,
	[S2MPU14_IRQ_OI_B2S] = S2MPU14_PMIC_IRQ_OI_B2_MASK,
	[S2MPU14_IRQ_OI_B3S] = S2MPU14_PMIC_IRQ_OI_B3_MASK,
	[S2MPU14_IRQ_OI_B4S] = S2MPU14_PMIC_IRQ_OI_B4_MASK,
	[S2MPU14_IRQ_OI_B5S] = S2MPU14_PMIC_IRQ_OI_B5_MASK,
	[S2MPU14_IRQ_OI_B6S] = S2MPU14_PMIC_IRQ_OI_B6_MASK,
	[S2MPU14_IRQ_OI_B7S] = S2MPU14_PMIC_IRQ_OI_B7_MASK,
	[S2MPU14_IRQ_OI_B8S] = S2MPU14_PMIC_IRQ_OI_B8_MASK,
	[S2MPU14_IRQ_OI_BBS] = S2MPU14_PMIC_IRQ_OI_BB_MASK,

	[S2MPU14_IRQ_OI_L6S] = S2MPU14_PMIC_IRQ_OI_L6_MASK,
	[S2MPU14_IRQ_OI_L10S] = S2MPU14_PMIC_IRQ_OI_L10_MASK,
	[S2MPU14_IRQ_OI_L11S] = S2MPU14_PMIC_IRQ_OI_L11_MASK,
	[S2MPU14_IRQ_OI_L23S] = S2MPU14_PMIC_IRQ_OI_L23_MASK,
	/* OCP */
	[S2MPU14_IRQ_OCP_B1S] = S2MPU14_PMIC_IRQ_OCP_B1_MASK,
	[S2MPU14_IRQ_OCP_B2S] = S2MPU14_PMIC_IRQ_OCP_B2_MASK,
	[S2MPU14_IRQ_OCP_B3S] = S2MPU14_PMIC_IRQ_OCP_B3_MASK,
	[S2MPU14_IRQ_OCP_B4S] = S2MPU14_PMIC_IRQ_OCP_B4_MASK,
	[S2MPU14_IRQ_OCP_B5S] = S2MPU14_PMIC_IRQ_OCP_B5_MASK,
	[S2MPU14_IRQ_OCP_B6S] = S2MPU14_PMIC_IRQ_OCP_B6_MASK,
	[S2MPU14_IRQ_OCP_B7S] = S2MPU14_PMIC_IRQ_OCP_B7_MASK,
	[S2MPU14_IRQ_OCP_B8S] = S2MPU14_PMIC_IRQ_OCP_B8_MASK,
	[S2MPU14_IRQ_OCP_BBS] = S2MPU14_PMIC_IRQ_OCP_BB_MASK,
	/* Temp */
	[S2MPU14_IRQ_INT120C] = S2MPU14_IRQ_INT120C_MASK,
	[S2MPU14_IRQ_INT140C] = S2MPU14_IRQ_INT140C_MASK,
};

static void s2mpu14_call_interrupt(struct s2mpu14_dev *s2mpu14,
				   u8 int1, u8 int2, u8 int3, u8 int4, u8 int5)
{
	size_t i;
	u8 reg = 0;

	/* BUCK OI interrupt */
	for (i = 0; i < S2MPU14_BUCK_MAX; i++) {
		reg = S2MPU14_IRQ_OI_B1S + i;

		if (int4 & s2mpu14_get_irq_mask[reg])
			s2mpu14_buck_oi_irq(s2mpu14, i);
	}

	/* BUCK OCP interrupt */
	for (i = 0; i < S2MPU14_BUCK_MAX; i++) {
		reg = S2MPU14_IRQ_OCP_B1S + i;

		if (int2 & s2mpu14_get_irq_mask[reg])
			s2mpu14_buck_ocp_irq(s2mpu14, i);
	}

	/* BUCK-BOOST OI interrupt */
	if (int5 & s2mpu14_get_irq_mask[S2MPU14_IRQ_OI_BBS])
		s2mpu14_bb_oi_irq(s2mpu14);

	/* BUCK-BOOST OCP interrupt */
	if (int3 & s2mpu14_get_irq_mask[S2MPU14_IRQ_OCP_BBS])
		s2mpu14_bb_ocp_irq(s2mpu14);

	/* Thermal interrupt */
	for (i = 0; i < S2MPU14_TEMP_MAX; i++) {
		reg = S2MPU14_IRQ_INT120C + i;

		if (int1 & s2mpu14_get_irq_mask[reg])
			s2mpu14_temp_irq(s2mpu14, i);
	}

	/* LDO OI interrupt */
	for (i = 0; i < S2MPU14_LDO_MAX; i++) {
		reg = S2MPU14_IRQ_OI_L6S + i;

		if (int5 & s2mpu14_get_irq_mask[reg])
			s2mpu14_ldo_oi_irq(s2mpu14, i);
	}
}

static void s2mpu14_irq_work_func(struct work_struct *work)
{
	pr_info("[PMIC] %s: sub pmic interrupt(0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx, 0x%02hhx)\n",
		__func__, irq_reg[S2MPU14_PMIC_INT1],
		irq_reg[S2MPU14_PMIC_INT2], irq_reg[S2MPU14_PMIC_INT3],
		irq_reg[S2MPU14_PMIC_INT4], irq_reg[S2MPU14_PMIC_INT5]);
}

static int s2mpu14_notifier_handler(struct notifier_block *nb,
				    unsigned long action,
				    void *data)
{
	int ret;
	struct s2mpu14_dev *s2mpu14 = data;

	if (!s2mpu14) {
		pr_err("%s: fail to load dev.\n", __func__);
		return IRQ_HANDLED;
	}

	mutex_lock(&s2mpu14->irq_lock);

	/* Read interrupt */
	ret = s2mpu14_bulk_read(s2mpu14->pmic, S2MPU14_REG_INT1,
				S2MPU14_NUM_IRQ_PMIC_REGS,
				&irq_reg[S2MPU14_PMIC_INT1]);
	if (ret) {
		pr_err("%s: fail to read INT sources\n", __func__);
		mutex_unlock(&s2mpu14->irq_lock);

		return IRQ_HANDLED;
	}

	queue_delayed_work(s2mpu14->irq_wqueue, &s2mpu14->irq_work, 0);

	/* Call interrupt */
	s2mpu14_call_interrupt(s2mpu14, irq_reg[S2MPU14_PMIC_INT1],
			       irq_reg[S2MPU14_PMIC_INT2],
			       irq_reg[S2MPU14_PMIC_INT3],
			       irq_reg[S2MPU14_PMIC_INT4],
			       irq_reg[S2MPU14_PMIC_INT5]);

	mutex_unlock(&s2mpu14->irq_lock);

	return IRQ_HANDLED;
}

static BLOCKING_NOTIFIER_HEAD(s2mpu14_notifier);

static int s2mpu14_register_notifier(struct notifier_block *nb,
			      struct s2mpu14_dev *s2mpu14)
{
	int ret;

	ret = blocking_notifier_chain_register(&s2mpu14_notifier, nb);
	if (ret < 0)
		pr_err("%s: fail to register notifier\n", __func__);

	return ret;
}

void s2mpu14_call_notifier(void)
{
	blocking_notifier_call_chain(&s2mpu14_notifier, 0, s2mpu14_global);
}
EXPORT_SYMBOL(s2mpu14_call_notifier);

static const u8 s2mpu14_mask_reg[] = {
	[S2MPU14_PMIC_INT1] = S2MPU14_REG_INT1M,
	[S2MPU14_PMIC_INT2] = S2MPU14_REG_INT2M,
	[S2MPU14_PMIC_INT3] = S2MPU14_REG_INT3M,
	[S2MPU14_PMIC_INT4] = S2MPU14_REG_INT4M,
	[S2MPU14_PMIC_INT5] = S2MPU14_REG_INT5M,
};

static int s2mpu14_unmask_interrupt(struct s2mpu14_dev *s2mpu14)
{
	int ret;
	/* unmask IRQM interrupt */
	ret = s2mpu14_update_reg(s2mpu14->i2c, S2MPU14_PMIC_REG_IRQM,
				 0x00, S2MPU14_PMIC_REG_IRQM_MASK);
	if (ret)
		goto err;

	/* unmask OI interrupt */
	ret = s2mpu14_write_reg(s2mpu14->pmic, S2MPU14_REG_INT4M, 0x00);
	if (ret)
		goto err;
	ret = s2mpu14_update_reg(s2mpu14->pmic, S2MPU14_REG_INT5M, 0x00, 0x1F);
	if (ret)
		goto err;

	/* unmask OCP interrupt */
	ret = s2mpu14_write_reg(s2mpu14->pmic, S2MPU14_REG_INT2M, 0x08);
	if (ret)
		goto err;
	ret = s2mpu14_update_reg(s2mpu14->pmic, S2MPU14_REG_INT3M, 0x00, 0x01);
	if (ret)
		goto err;

	/* unmask Temp interrupt */
	ret = s2mpu14_update_reg(s2mpu14->pmic, S2MPU14_REG_INT1M, 0x00, 0x0C);
	if (ret)
		goto err;

	return 0;
err:
	return -1;
}

static int s2mpu14_set_interrupt(struct s2mpu14_dev *s2mpu14)
{
	u8 i, val;
	int ret;

	/* Mask all the interrupt sources */
	for (i = 0; i < S2MPU14_IRQ_GROUP_NR; i++) {
		ret = s2mpu14_write_reg(s2mpu14->pmic, s2mpu14_mask_reg[i], 0xFF);
		if (ret)
			goto err;
	}
	ret = s2mpu14_update_reg(s2mpu14->i2c, S2MPU14_PMIC_REG_IRQM,
				 S2MPU14_PMIC_REG_IRQM_MASK, S2MPU14_PMIC_REG_IRQM_MASK);
	if (ret)
		goto err;

	/* Unmask interrupt sources */
	ret = s2mpu14_unmask_interrupt(s2mpu14);
	if (ret < 0) {
		pr_err("%s: s2mpu14_unmask_interrupt fail\n", __func__);
		goto err;
	}

	/* Check unmask interrupt register */
	for (i = 0; i < S2MPU14_IRQ_GROUP_NR; i++) {
		ret = s2mpu14_read_reg(s2mpu14->pmic, s2mpu14_mask_reg[i], &val);
		if (ret)
			goto err;
		pr_info("%s: INT%dM = 0x%02hhx\n", __func__, i + 1, val);
	}

	return 0;
err:
	return -1;
}

static int s2mpu14_set_wqueue(struct s2mpu14_dev *s2mpu14)
{
	static char device_name[32] = {0, };

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-wqueue@%s",
		 dev_driver_string(s2mpu14->dev), dev_name(s2mpu14->dev));

	s2mpu14->irq_wqueue = create_singlethread_workqueue(device_name);
	if (!s2mpu14->irq_wqueue) {
		pr_err("%s: fail to create workqueue\n", __func__);
		return -1;
	}

	INIT_DELAYED_WORK(&s2mpu14->irq_work, s2mpu14_irq_work_func);

	return 0;
}

static void s2mpu14_set_notifier(struct s2mpu14_dev *s2mpu14)
{
	sub_pmic_notifier.notifier_call = s2mpu14_notifier_handler;
	s2mpu14_register_notifier(&sub_pmic_notifier, s2mpu14);
}

int s2mpu14_notifier_init(struct s2mpu14_dev *s2mpu14)
{
	s2mpu14_global = s2mpu14;
	mutex_init(&s2mpu14->irq_lock);

	/* Register notifier */
	s2mpu14_set_notifier(s2mpu14);

	/* Set workqueue */
	if (s2mpu14_set_wqueue(s2mpu14) < 0) {
		pr_err("%s: s2mpu14_set_wqueue fail\n", __func__);
		goto err;
	}

	/* Set interrupt */
	if (s2mpu14_set_interrupt(s2mpu14) < 0) {
		pr_err("%s: s2mpu14_set_interrupt fail\n", __func__);
		goto err;
	}

	return 0;
err:
	return -1;
}
EXPORT_SYMBOL_GPL(s2mpu14_notifier_init);
