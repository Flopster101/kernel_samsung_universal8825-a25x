/*
 * s2mpu13.c
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <../drivers/pinctrl/samsung/pinctrl-samsung.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/samsung/s2mpu13.h>
#include <linux/mfd/samsung/s2mpu13-regulator.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <linux/regulator/pmic_class.h>
#include <linux/reset/exynos-reset.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#define MAIN_CHANNEL	0
static struct device_node *acpm_mfd_node;
#endif

#define I2C_BASE_COMMON	0x00
#define I2C_BASE_PM	0x01
#define I2C_BASE_RTC	0x02
#define I2C_BASE_ADC	0x0A
#define I2C_BASE_GPIO	0x0B
#define I2C_BASE_CLOSE	0x0F

static struct s2mpu13_info *s2mpu13_static_info;
static int s2mpu13_buck_ocp_cnt[S2MPU13_BUCK_MAX]; /* BUCK 1~10 OCP count */
static int s2mpu13_buck_oi_cnt[S2MPU13_BUCK_MAX]; /* BUCK 1~10 OI count */
static int s2mpu13_ldo_oi_cnt[S2MPU13_LDO_MAX]; /* LDO (1, 2, 11, 13) OI count */
static int s2mpu13_temp_cnt[S2MPU13_TEMP_MAX]; /* 0 : 120 degree , 1 : 140 degree */

struct s2mpu13_info {
	struct regulator_dev *rdev[S2MPU13_REGULATOR_MAX];
	unsigned int opmode[S2MPU13_REGULATOR_MAX];
	int num_regulators;
	struct s2mpu13_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
	struct i2c_client *gpio_i2c;
	int buck_ocp_irq[S2MPU13_BUCK_MAX];	/* BUCK OCP IRQ */
	int buck_oi_irq[S2MPU13_BUCK_MAX];	/* BUCK OI IRQ */
	int ldo_oi_irq[S2MPU13_LDO_MAX];	/* LDO OI IRQ */
	int temp_irq[S2MPU13_TEMP_MAX];	/* 0 : 120 degree, 1 : 140 degree */
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	u8 base_addr;
	u8 read_addr;
	u8 read_val;
	struct device *dev;
#endif
};

int s2mpu13_set_reboot_option(void)
{
	int ret = 0;

	/* PMIC: reboot option disable */
	ret = s2mpu13_write_reg(s2mpu13_static_info->i2c, S2MPU13_PMIC_REG_REBOOT_OPTION, 0x00);
	if (ret < 0) {
		pr_err("%s: s2mpu13_write_reg failed\n", __func__);
		return -1;
	}
	pr_info("%s: reboot_option disable\n", __func__);

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu13_set_reboot_option);

int s2mpu13_write_gpio(unsigned char reg, unsigned char value)
{
	int ret = 0;

	if (reg < S2MPU13_GPIO_SETH && reg > S2MPU13_GPIO_SET8) {
		pr_err("%s: fault reg.\n", __func__);
		return -1;
	}

	ret = s2mpu13_write_reg(s2mpu13_static_info->gpio_i2c, reg, value);
	if (ret < 0) {
		pr_err("%s: s2mpu13_write_reg fail\n", __func__);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu13_write_gpio);

int s2mpu13_read_gpio(unsigned char reg, unsigned char *dest)
{
	int ret = 0;

	if (reg < S2MPU13_GPIO_SETH && reg > S2MPU13_GPIO_SET8) {
		pr_err("%s: fault reg.\n", __func__);
		return -1;
	}

	ret = s2mpu13_read_reg(s2mpu13_static_info->gpio_i2c, reg, dest);
	if (ret < 0) {
		pr_err("%s: s2mpu13_read_reg fail\n", __func__);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu13_read_gpio);

static unsigned int s2mpu13_of_map_mode(unsigned int val)
{
	switch (val) {
	case SEC_OPMODE_SUSPEND:	/* ON in Standby Mode */
		return 0x1;
	case SEC_OPMODE_MIF:		/* ON in PWREN_MIF mode */
		return 0x2;
	case SEC_OPMODE_ON:		/* ON in Normal Mode */
		return 0x3;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

/* Some LDOs supports [LPM/Normal]ON mode during suspend state */
static int s2m_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct s2mpu13_info *s2mpu13 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPU13_ENABLE_SHIFT;

	s2mpu13->opmode[id] = val;
	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mpu13_info *s2mpu13 = rdev_get_drvdata(rdev);

	return s2mpu13_update_reg(s2mpu13->i2c, rdev->desc->enable_reg,
				  s2mpu13->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mpu13_info *s2mpu13 = rdev_get_drvdata(rdev);
	unsigned int val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mpu13_update_reg(s2mpu13->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mpu13_info *s2mpu13 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mpu13_read_reg(s2mpu13->i2c,
				rdev->desc->enable_reg, &val);
	if (ret)
		return ret;

	if (rdev->desc->enable_is_inverted)
		return (val & rdev->desc->enable_mask) == 0;
	else
		return (val & rdev->desc->enable_mask) != 0;
}

static int get_ramp_delay(int ramp_delay)
{
	unsigned char cnt = 0;

	ramp_delay /= 6;

	while (true) {
		ramp_delay = ramp_delay >> 1;
		if (ramp_delay == 0)
			break;
		cnt++;
	}
	return cnt;
}

static int s2m_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	struct s2mpu13_info *s2mpu13 = rdev_get_drvdata(rdev);
	int ramp_shift, reg_id = rdev_get_id(rdev);
	int ramp_addr;
	unsigned int ramp_value = 0;

	ramp_value = get_ramp_delay(ramp_delay / 1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}
	ramp_value = 0x00;	// 6.25mv/us fixed

	switch (reg_id) {
	case S2MPU13_BUCK1:
	case S2MPU13_BUCK5:
	case S2MPU13_BUCK9:
		ramp_shift = 6;
		break;
	//case S2MPU13_BUCK2:
	case S2MPU13_BUCK6:
	case S2MPU13_BUCK10:
		ramp_shift = 4;
		break;
	//case S2MPU13_BUCK3:
	//case S2MPU13_BUCK7:
	//	ramp_shift = 2;
	//	break;
	case S2MPU13_BUCK4:
	case S2MPU13_BUCK8:
		ramp_shift = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (reg_id) {
	case S2MPU13_BUCK1:
	//case S2MPU13_BUCK2:
	//case S2MPU13_BUCK3:
	case S2MPU13_BUCK4:
		ramp_addr = S2MPU13_PMIC_REG_BUCK_RAMP_UP1M;
		break;
	case S2MPU13_BUCK5:
	case S2MPU13_BUCK6:
	//case S2MPU13_BUCK7:
	case S2MPU13_BUCK8:
		ramp_addr = S2MPU13_PMIC_REG_BUCK_RAMP_UP2M;
		break;
	case S2MPU13_BUCK9:
	case S2MPU13_BUCK10:
		ramp_addr = S2MPU13_PMIC_REG_BUCK_RAMP_UP3M;
		break;
	default:
		return -EINVAL;
	}

	return s2mpu13_update_reg(s2mpu13->i2c, ramp_addr,
		ramp_value << ramp_shift, BUCK_RAMP_MASK << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mpu13_info *s2mpu13 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mpu13_read_reg(s2mpu13->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpu13_info *s2mpu13 = rdev_get_drvdata(rdev);
	int ret;

	ret = s2mpu13_update_reg(s2mpu13->i2c, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mpu13_update_reg(s2mpu13->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev,
								unsigned sel)
{
	int ret = 0;
	struct s2mpu13_info *s2mpu13 = rdev_get_drvdata(rdev);

	ret = s2mpu13_write_reg(s2mpu13->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0)
		goto i2c_out;

	if (rdev->desc->apply_bit)
		ret = s2mpu13_update_reg(s2mpu13->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

	return ret;

i2c_out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	ret = -EINVAL;
	return ret;
}

static int s2m_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	unsigned int ramp_delay = 0;
	int old_volt, new_volt;

	if (rdev->constraints->ramp_delay)
		ramp_delay = rdev->constraints->ramp_delay;
	else if (rdev->desc->ramp_delay)
		ramp_delay = rdev->desc->ramp_delay;

	if (ramp_delay == 0) {
		pr_warn("%s: ramp_delay not set\n", rdev->desc->name);
		return -EINVAL;
	}

	/* sanity check */
	if (!rdev->desc->ops->list_voltage)
		return -EINVAL;

	old_volt = rdev->desc->ops->list_voltage(rdev, old_selector);
	new_volt = rdev->desc->ops->list_voltage(rdev, new_selector);

	if (old_selector < new_selector)
		return DIV_ROUND_UP(new_volt - old_volt, ramp_delay);
	else
		return DIV_ROUND_UP(old_volt - new_volt, ramp_delay);

	return 0;
}

static int s2mpu13_read_pwron_status(void)
{
	struct s2mpu13_info *s2mpu13 = s2mpu13_static_info;
	u8 val = 0;
	int ret = 0;

	if (!s2mpu13)
		return -ENODEV;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	ret = exynos_acpm_read_reg(acpm_mfd_node, MAIN_CHANNEL, I2C_BASE_PM, S2MPU13_PMIC_REG_STATUS1, &val);
	if (ret) {
		pr_err("%s: acpm ipc fail(%#x)\n", __func__, S2MPU13_PMIC_REG_STATUS1);
		return -EINVAL;
	}
#endif
	pr_info("%s: 0x%02hhx\n", __func__, val);

	return (val & S2MPU13_STATUS1_PWRON);
}

int pmic_read_pwrkey_status(void)
{
	return s2mpu13_read_pwron_status();
}
EXPORT_SYMBOL_GPL(pmic_read_pwrkey_status);

static struct regulator_ops s2mpu13_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
};

static struct regulator_ops s2mpu13_buck_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap_buck,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
	.set_mode		= s2m_set_mode,
	.set_ramp_delay		= s2m_set_ramp_delay,
};
//
//static struct regulator_ops s2mpu13_bb_ops = {
//	.list_voltage		= regulator_list_voltage_linear,
//	.map_voltage		= regulator_map_voltage_linear,
//	.is_enabled		= s2m_is_enabled_regmap,
//	.enable			= s2m_enable,
//	.disable		= s2m_disable_regmap,
//	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
//	.set_voltage_sel	= s2m_set_voltage_sel_regmap,
//	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
//	.set_mode		= s2m_set_mode,
//};
//

#define _BUCK(macro)		S2MPU13_BUCK##macro
#define _buck_ops(num)		s2mpu13_buck_ops##num
#define _LDO(macro)		S2MPU13_LDO##macro
#define _ldo_ops(num)		s2mpu13_ldo_ops##num
#define _BB(macro)		S2MPU13_BB##macro
#define _bb_ops(num)		s2mpu13_bb_ops##num

#define _REG(ctrl)		S2MPU13_PMIC_REG##ctrl
#define _TIME(macro)		S2MPU13_ENABLE_TIME##macro

#define _LDO_MIN(group)		S2MPU13_LDO_MIN##group
#define _LDO_STEP(group)	S2MPU13_LDO_STEP##group
#define _BUCK_MIN(group)	S2MPU13_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPU13_BUCK_STEP##group
#define _BB_MIN(group)		S2MPU13_BB_MIN##group
#define _BB_STEP(group)		S2MPU13_BB_STEP##group

#define BUCK_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPU13_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU13_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU13_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu13_of_map_mode			\
}

#define LDO_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _LDO_MIN(g),				\
	.uV_step	= _LDO_STEP(g),				\
	.n_voltages	= S2MPU13_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU13_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU13_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu13_of_map_mode			\
}
//
//#define BB_DESC(_name, _id, _ops, g, v, e, t)	{		\
//	.name		= _name,				\
//	.id		= _id,					\
//	.ops		= _ops,					\
//	.type		= REGULATOR_VOLTAGE,			\
//	.owner		= THIS_MODULE,				\
//	.min_uV		= _BB_MIN(),				\
//	.uV_step	= _BB_STEP(),				\
//	.n_voltages	= S2MPU13_BB_N_VOLTAGES,		\
//	.vsel_reg	= v,					\
//	.vsel_mask	= S2MPU13_BB_VSEL_MASK,			\
//	.enable_reg	= e,					\
//	.enable_mask	= S2MPU13_ENABLE_MASK,			\
//	.enable_time	= t,					\
//	.of_map_mode	= s2mpu13_of_map_mode			\
//}
//

static struct regulator_desc regulators[S2MPU13_REGULATOR_MAX] = {
	/* name, id, ops, group, vsel_reg, enable_reg, ramp_delay */
	/* LDO 1~28 */
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), 3, _REG(_L1CTRL), _REG(_L1CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), 4, _REG(_L2CTRL), _REG(_L2CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), 2, _REG(_L3CTRL), _REG(_L3CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), 1, _REG(_L4CTRL), _REG(_L4CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), 1, _REG(_L5CTRL1), _REG(_L5CTRL1), _TIME(_LDO)),
	//LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), 1, _REG(_L6CTRL), _REG(_L6CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), 1, _REG(_L7CTRL), _REG(_L7CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), 2, _REG(_L8CTRL), _REG(_L8CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), 1, _REG(_L9CTRL), _REG(_L9CTRL), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), 1, _REG(_L10CTRL), _REG(_L10CTRL), _TIME(_LDO)),

	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), 3, _REG(_L11CTRL), _REG(_L11CTRL), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), 3, _REG(_L12CTRL), _REG(_L12CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), 4, _REG(_L13CTRL), _REG(_L13CTRL), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), 4, _REG(_L14CTRL), _REG(_L14CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), 3, _REG(_L15CTRL), _REG(_L15CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), 4, _REG(_L16CTRL), _REG(_L16CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), 3, _REG(_L17CTRL), _REG(_L17CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), 3, _REG(_L18CTRL), _REG(_L18CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), 4, _REG(_L19CTRL), _REG(_L19CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), 4, _REG(_L20CTRL), _REG(_L20CTRL), _TIME(_LDO)),

	LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), 1, _REG(_L21CTRL1), _REG(_L21CTRL1), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), 4, _REG(_L22CTRL), _REG(_L22CTRL), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), 4, _REG(_L23CTRL), _REG(_L23CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), 4, _REG(_L24CTRL), _REG(_L24CTRL), _TIME(_LDO)),
	LDO_DESC("LDO25", _LDO(25), &_ldo_ops(), 4, _REG(_L25CTRL), _REG(_L25CTRL), _TIME(_LDO)),
	LDO_DESC("LDO26", _LDO(26), &_ldo_ops(), 4, _REG(_L26CTRL), _REG(_L26CTRL), _TIME(_LDO)),
	LDO_DESC("LDO27", _LDO(27), &_ldo_ops(), 4, _REG(_L27CTRL), _REG(_L27CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO28", _LDO(28), &_ldo_ops(), 4, _REG(_L28CTRL), _REG(_L28CTRL), _TIME(_LDO)),

	/* BUCK 1~10 */
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), 1, _REG(_B1M_OUT1), _REG(_B1M_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), 1, _REG(_B2M_OUT1), _REG(_B2M_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), 1, _REG(_B3M_OUT1), _REG(_B3M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), 1, _REG(_B4M_OUT2), _REG(_B4M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), 1, _REG(_B5M_OUT1), _REG(_B5M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), 1, _REG(_B6M_OUT1), _REG(_B6M_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), 1, _REG(_B7M_OUT1), _REG(_B7M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), 2, _REG(_B8M_OUT1), _REG(_B8M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK9", _BUCK(9), &_buck_ops(), 3, _REG(_B9M_OUT1), _REG(_B9M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK10", _BUCK(10), &_buck_ops(), 4, _REG(_B10M_OUT1), _REG(_B10M_CTRL), _TIME(_BUCK)),
};

#if IS_ENABLED(CONFIG_OF)
static int s2mpu13_pmic_dt_parse_pdata(struct s2mpu13_dev *iodev,
					struct s2mpu13_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mpu13_regulator_data *rdata;
	u32 i;
	int ret, len;
	u32 val;
	const u32 *p;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = pmic_np;
#endif

	/* get 1 gpio values */
	if (of_gpio_count(pmic_np) < 1) {
		dev_err(iodev->dev, "could not find pmic gpios\n");
		return -EINVAL;
	}
	pdata->smpl_warn = of_get_gpio(pmic_np, 0);

	ret = of_property_read_u32(pmic_np, "smpl_warn_en", &val);
	if (ret)
		return -EINVAL;
	pdata->smpl_warn_en = !!val;

	/* Set SEL_VGPIO (control_sel) */
	p = of_get_property(pmic_np, "sel_vgpio", &len);
	if (!p) {
		pr_info("%s : (ERROR) sel_vgpio isn't parsing\n", __func__);
		return -EINVAL;
	}

	len = len / sizeof(u32);
	if (len != S2MPU13_SEL_VGPIO_NUM) {
		pr_info("%s : (ERROR) sel_vgpio num isn't not equal\n", __func__);
		return -EINVAL;
	}

	pdata->sel_vgpio = devm_kzalloc(iodev->dev, sizeof(u32) * len, GFP_KERNEL);
	if (!(pdata->sel_vgpio)) {
		dev_err(iodev->dev,
				"(ERROR) could not allocate memory for sel_vgpio data\n");
		return -ENOMEM;
	}

	for (i = 0; i < len; i++) {
		ret = of_property_read_u32_index(pmic_np, "sel_vgpio", i, &pdata->sel_vgpio[i]);
		if (ret) {
			pr_info("%s : (ERROR) sel_vgpio%d is empty\n", __func__, i + 1);
			pdata->sel_vgpio[i] = 0x1FF;
		}
	}

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(iodev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		pdata->num_regulators++;
	}

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata)
		return -ENOMEM;

	pdata->regulators = rdata;
	pdata->num_rdata = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(regulators); i++)
			if (!of_node_cmp(reg_np->name, regulators[i].name))
				break;

		if (i == ARRAY_SIZE(regulators)) {
			dev_warn(iodev->dev,
					"don't know how to configure regulator %s\n",
					reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(iodev->dev, reg_np,
				&regulators[i]);
		rdata->reg_node = reg_np;
		rdata++;
		pdata->num_rdata++;
	}

	return 0;
}
#else
static int s2mpu13_pmic_dt_parse_pdata(struct s2mpu13_pmic_dev *iodev,
					struct s2mpu13_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_EXYNOS_AFM)
int main_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	if (!i2c)
		return -ENODEV;

	return s2mpu13_update_reg(i2c, reg, val, mask);
}
EXPORT_SYMBOL_GPL(main_pmic_update_reg);

int main_pmic_get_i2c(struct i2c_client **i2c)
{
	if (!s2mpu13_static_info)
		return -ENODEV;

	*i2c = s2mpu13_static_info->i2c;

	return 0;
}
EXPORT_SYMBOL_GPL(main_pmic_get_i2c);
#endif

static irqreturn_t s2mpu13_buck_ocp_irq(int irq, void *data)
{
	struct s2mpu13_info *s2mpu13 = data;
	u32 i;

	mutex_lock(&s2mpu13->lock);

	for (i = 0; i < S2MPU13_BUCK_MAX; i++) {
		if (s2mpu13_static_info->buck_ocp_irq[i] == irq) {
			s2mpu13_buck_ocp_cnt[i]++;
			pr_info("%s : BUCK[%d] OCP IRQ : %d, %d\n",
				__func__, i + 1, s2mpu13_buck_ocp_cnt[i], irq);
			break;
		}
	}

	mutex_unlock(&s2mpu13->lock);

	return IRQ_HANDLED;
}

static irqreturn_t s2mpu13_buck_oi_irq(int irq, void *data)
{
	struct s2mpu13_info *s2mpu13 = data;
	u32 i;

	mutex_lock(&s2mpu13->lock);

	for (i = 0; i < S2MPU13_BUCK_MAX; i++) {
		if (s2mpu13_static_info->buck_oi_irq[i] == irq) {
			s2mpu13_buck_oi_cnt[i]++;
			pr_info("%s : BUCK[%d] OI IRQ : %d, %d\n",
				__func__, i + 1, s2mpu13_buck_oi_cnt[i], irq);
			break;
		}
	}

	mutex_unlock(&s2mpu13->lock);

	return IRQ_HANDLED;
}

static irqreturn_t s2mpu13_ldo_oi_irq(int irq, void *data)
{
	struct s2mpu13_info *s2mpu13 = data;
	int ldo_oi_arr[S2MPU13_LDO_MAX] = {1, 2, 11, 13};
	u32 i;

	mutex_lock(&s2mpu13->lock);

	for (i = 0; i < S2MPU13_LDO_MAX; i++) {
		if (s2mpu13_static_info->ldo_oi_irq[i] == irq) {
			s2mpu13_ldo_oi_cnt[i]++;
			pr_info("%s : LDO[%d] OI IRQ : %d, %d\n",
				__func__, ldo_oi_arr[i], s2mpu13_buck_oi_cnt[i], irq);
			break;
		}
	}

	mutex_unlock(&s2mpu13->lock);

	return IRQ_HANDLED;
}

static irqreturn_t s2mpu13_temp_irq(int irq, void *data)
{
	struct s2mpu13_info *s2mpu13 = data;

	mutex_lock(&s2mpu13->lock);

	if (s2mpu13_static_info->temp_irq[0] == irq) {
		s2mpu13_temp_cnt[0]++;
		pr_info("%s: PMIC thermal 120C IRQ : %d, %d\n",
			__func__, s2mpu13_temp_cnt[0], irq);
	} else if (s2mpu13_static_info->temp_irq[1] == irq) {
		s2mpu13_temp_cnt[1]++;
		pr_info("%s: PMIC thermal 140C IRQ : %d, %d\n",
			__func__, s2mpu13_temp_cnt[1], irq);
	}

	mutex_unlock(&s2mpu13->lock);

	return IRQ_HANDLED;
}

struct s2mpu13_oi_data {
	u8 reg;
	u8 val;
};

#define DECLARE_OI(_reg, _val)	{ .reg = (_reg), .val = (_val) }
static const struct s2mpu13_oi_data s2mpu13_oi[] = {
	/* BUCK(1~10) & LDO(1, 2, 11, 13) OI function enable */
	DECLARE_OI(S2MPU13_PMIC_REG_BUCK_OI_EN1M, 0xFF),
	DECLARE_OI(S2MPU13_PMIC_REG_BUCK_OI_EN2M, 0x03),
	DECLARE_OI(S2MPU13_PMIC_REG_LDO_OI_EN_M, 0x0F),
	/* BUCK(1~10) & LDO(1, 2, 11, 13) OI function power down disable */
	DECLARE_OI(S2MPU13_PMIC_REG_BUCK_OI_PD_EN1M, 0x00),
	DECLARE_OI(S2MPU13_PMIC_REG_BUCK_OI_PD_EN2M, 0x00),
	DECLARE_OI(S2MPU13_PMIC_REG_LDO_OI_PD_EN_M, 0x00),
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPU13_PMIC_REG_BUCK_OI_CTRL1M, 0xCC),
	DECLARE_OI(S2MPU13_PMIC_REG_BUCK_OI_CTRL2M, 0xCC),
	DECLARE_OI(S2MPU13_PMIC_REG_BUCK_OI_CTRL3M, 0xCC),
	DECLARE_OI(S2MPU13_PMIC_REG_BUCK_OI_CTRL4M, 0xCC),
	DECLARE_OI(S2MPU13_PMIC_REG_BUCK_OI_CTRL5M, 0xCC),
	DECLARE_OI(S2MPU13_PMIC_REG_LDO_OI_CTRL_M, 0xFF),
};

static int s2mpu13_oi_function(struct s2mpu13_dev *iodev)
{
	struct i2c_client *i2c = iodev->pmic;
	u32 i;
	u8 val;
	int ret, cnt = 0;
	char buf[1024] = {0, };

	for (i = 0; i < ARRAY_SIZE(s2mpu13_oi); i++) {
		ret = s2mpu13_write_reg(i2c, s2mpu13_oi[i].reg, s2mpu13_oi[i].val);
		if (ret) {
			pr_err("%s: failed to write register\n", __func__);
			goto err;
		}
	}

	for (i = 0; i < ARRAY_SIZE(s2mpu13_oi); i++) {
		ret = s2mpu13_read_reg(i2c,  s2mpu13_oi[i].reg, &val);
		if (ret)
			goto err;

		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "0x%x[0x%02hhx], ", s2mpu13_oi[i].reg, val);
	}
	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -1;
}

static int s2mpu13_set_interrupt(struct platform_device *pdev,
				 struct s2mpu13_info *s2mpu13, int irq_base)
{
	int i, ret;

	/* BUCK 1~10 OCP interrupt */
	for (i = 0; i < S2MPU13_BUCK_MAX; i++) {
		if (i == 0)
			continue;

		s2mpu13->buck_ocp_irq[i] = irq_base +
					   S2MPU13_PMIC_IRQ_OCP_B1M_INT3 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mpu13->buck_ocp_irq[i], NULL,
						s2mpu13_buck_ocp_irq, 0,
						"BUCK_OCP_IRQ", s2mpu13);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request BUCK[%d] OCP IRQ: %d: %d\n",
				i + 1, s2mpu13->buck_ocp_irq[i], ret);
			goto err;
		}
	}

	/* BUCK 1~10 OI interrupt */
	for (i = 0; i < S2MPU13_BUCK_MAX; i++) {
		s2mpu13->buck_oi_irq[i] = irq_base +
					   S2MPU13_PMIC_IRQ_OI_B1M_INT5 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mpu13->buck_oi_irq[i], NULL,
						s2mpu13_buck_oi_irq, 0,
						"BUCK_OI_IRQ", s2mpu13);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request BUCK[%d] OCP IRQ: %d: %d\n",
				i + 1, s2mpu13->buck_oi_irq[i], ret);
			goto err;
		}
	}

	/* LDO (1, 2, 11, 13) OI interrupt */
	for (i = 0; i < S2MPU13_LDO_MAX; i++) {
		s2mpu13->ldo_oi_irq[i] = irq_base +
					   S2MPU13_PMIC_IRQ_SC_LDO1M_INT4 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mpu13->ldo_oi_irq[i], NULL,
						s2mpu13_ldo_oi_irq, 0,
						"LDO_OI_IRQ", s2mpu13);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request LDO[%d] OCP IRQ: %d: %d\n",
				i + 1, s2mpu13->ldo_oi_irq[i], ret);
			goto err;
		}
	}

	/* Thermal interrupt */
	for (i = 0; i < S2MPU13_TEMP_MAX; i++) {
		s2mpu13->temp_irq[i] = irq_base + S2MPU13_PMIC_IRQ_INT120C_INT6 + i;

		ret = devm_request_threaded_irq(&pdev->dev,
						s2mpu13->temp_irq[i], NULL,
						s2mpu13_temp_irq, 0,
						"TEMP_IRQ", s2mpu13);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Failed to request over temperature[%d] IRQ: %d: %d\n",
				i, s2mpu13->temp_irq[i], ret);
			goto err;
		}
	}

	return 0;
err:
	return -1;
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t s2mpu13_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mpu13_info *s2mpu13 = dev_get_drvdata(dev);
	int ret;
	u8 base_addr = 0, reg_addr = 0, val = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu13_dev *iodev = s2mpu13->iodev;
#endif

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return -1;
	}

	ret = sscanf(buf, "0x%02hhx%02hhx", &base_addr, &reg_addr);
	if (ret != 2) {
		pr_err("%s: input error\n", __func__);
		return size;
	}

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	mutex_lock(&iodev->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, MAIN_CHANNEL, base_addr, reg_addr, &val);
	mutex_unlock(&iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to read i2c addr/data\n", __func__);
#endif

	pr_info("%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__,
		base_addr, reg_addr, val);
	s2mpu13->base_addr = base_addr;
	s2mpu13->read_addr = reg_addr;
	s2mpu13->read_val = val;

	return size;
}

static ssize_t s2mpu13_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mpu13_info *s2mpu13 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			s2mpu13->base_addr, s2mpu13->read_addr, s2mpu13->read_val);
}

static ssize_t s2mpu13_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	int ret;
	u8 base_addr = 0, reg_addr = 0, data = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu13_info *s2mpu13 = dev_get_drvdata(dev);
	struct s2mpu13_dev *iodev = s2mpu13->iodev;
#endif

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return size;
	}

	ret = sscanf(buf, "0x%02hhx%02hhx 0x%02hhx", &base_addr, &reg_addr, &data);
	if (ret != 3) {
		pr_err("%s: input error\n", __func__);
		return size;
	}

	switch (base_addr) {
	case I2C_BASE_COMMON:
	case I2C_BASE_PM:
	case I2C_BASE_RTC:
	case I2C_BASE_ADC:
	case I2C_BASE_GPIO:
	case I2C_BASE_CLOSE:
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return size;
	}

	pr_info("%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__,
		base_addr, reg_addr, data);

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	mutex_lock(&iodev->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, MAIN_CHANNEL, base_addr, reg_addr, data);
	mutex_unlock(&iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to write i2c addr/data\n", __func__);
#endif

	return size;
}

static ssize_t s2mpu13_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mpu13_write\n");
}

static struct pmic_device_attribute regulator_attr[] = {
	PMIC_ATTR(s2mpu13_write, 0644, s2mpu13_write_show, s2mpu13_write_store),
	PMIC_ATTR(s2mpu13_read, 0644, s2mpu13_read_show, s2mpu13_read_store),
};

static int s2mpu13_create_sysfs(struct s2mpu13_info *s2mpu13)
{
	struct device *s2mpu13_pmic = s2mpu13->dev;
	struct device *dev = s2mpu13->iodev->dev;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);
	s2mpu13->base_addr = 0;
	s2mpu13->read_addr = 0;
	s2mpu13->read_val = 0;

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mpu13_pmic = pmic_device_create(s2mpu13, device_name);
	s2mpu13->dev = s2mpu13_pmic;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++) {
		err = device_create_file(s2mpu13_pmic, &regulator_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mpu13_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mpu13_pmic->devt);

	return -1;
}
#endif

static int s2mpu13_set_sel_vgpio(struct s2mpu13_info *s2mpu13,
				   struct s2mpu13_platform_data *pdata)
{
	int ret, i, cnt = 0;
	u8 reg, val;
	char buf[1024] = {0, };

	for (i = 0; i < S2MPU13_SEL_VGPIO_NUM; i++) {
		reg = S2MPU13_PMIC_REG_SEL_VGPIO0M + i;
		val = pdata->sel_vgpio[i];

		if (val <= S2MPU13_SEL_VGPIO_MAX_VAL) {
			ret = s2mpu13_write_reg(s2mpu13->i2c, reg, val);
			if (ret) {
				pr_err("%s: sel_vgpio%d write error\n", __func__, i + 1, __func__);
				goto err;
			}

			cnt += snprintf(buf + cnt, sizeof(buf) - 1,
					"0x%02hhx[0x%02hhx], ", reg, val);
		} else {
			pr_err("%s: sel_vgpio%d exceed the value\n", __func__, i + 1);
			goto err;
		}
	}

	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -1;
}

int s2mpu13_set_instacok(void)
{
	int ret;

	ret = s2mpu13_update_reg(s2mpu13_static_info->i2c, S2MPU13_PMIC_REG_CFG1, 0x04, 0x04);
	if (ret)
		return -1;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu13_set_instacok);

#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
static u8 pmic_onsrc;
static u8 pmic_offsrc[2];

static ssize_t pwr_on_off_src_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "ONSRC:0x%02X OFFSRC:0x%02X,0x%02X\n",
			pmic_onsrc,
			pmic_offsrc[0], pmic_offsrc[1]);
}

static DEVICE_ATTR_RO(pwr_on_off_src);

static struct attribute *sec_pm_debug_attrs[] = {
	&dev_attr_pwr_on_off_src.attr,
	NULL
};
ATTRIBUTE_GROUPS(sec_pm_debug);

int main_pmic_init_debug_sysfs(struct device *sec_pm_dev)
{
	int ret;

	ret = sysfs_create_groups(&sec_pm_dev->kobj, sec_pm_debug_groups);
	if (ret)
		pr_err("%s: failed to create sysfs groups(%d)\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(main_pmic_init_debug_sysfs);
#endif /* CONFIG_SEC_PM_DEBUG */

static void s2mpu13_set_regulator_vol(struct s2mpu13_info *s2mpu13)
{
//	s2mpu13_write_reg(s2mpu13->i2c, S2MPU13_PMIC_REG_B4M_OUT1, 0x20);
}

static void s2mpu13_set_dropout_vol(struct s2mpu13_info *s2mpu13)
{
	s2mpu13_write_reg(s2mpu13->i2c, S2MPU13_PMIC_REG_B8M_OUT2, 0x58);
	s2mpu13_write_reg(s2mpu13->i2c, S2MPU13_PMIC_REG_B9M_OUT2, 0x68);
	s2mpu13_write_reg(s2mpu13->i2c, S2MPU13_PMIC_REG_B10M_OUT2, 0x64);

	s2mpu13_write_reg(s2mpu13->i2c, S2MPU13_PMIC_REG_SEL_DVS_EN2M, 0x00);
	s2mpu13_update_reg(s2mpu13->i2c, S2MPU13_PMIC_REG_SEL_DVS_EN3M, 0x00, 0x0F);
}

void s2mpu13_set_reg_changes(struct s2mpu13_info *s2mpu13)
{
	u8 hw_chip_id = s2mpu13->iodev->pmic_rev & CHIP_ID_MASK;

	/* Change min voltage and step for L1M in EVT0.1 (HW_REV = 0x02) */
	if (hw_chip_id >= 0x02) {
		regulators[S2MPU13_LDO1].min_uV = _LDO_MIN(2);
		regulators[S2MPU13_LDO1].uV_step = _LDO_STEP(2);
	}
}

static int s2mpu13_power_off_wa(void)
{
	struct s2mpu13_info *s2mpu13 = s2mpu13_static_info;
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	uint8_t val = 0;
#endif

	if (!s2mpu13)
		return -ENODEV;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	ret = exynos_acpm_read_reg(acpm_mfd_node, MAIN_CHANNEL, I2C_BASE_PM, 0x1B, &val);
	if (ret) {
		pr_err("%s: acpm ipc fail(%#x)\n", __func__, 0x1B);
		return -EINVAL;
	}

	if ((val & 0x10) == 0x00) {
		ret = exynos_acpm_update_reg(acpm_mfd_node, MAIN_CHANNEL, I2C_BASE_PM, 0x1B, 0x20, 0x20);
		if (ret) {
			pr_err("%s: acpm ipc fail(%#x)\n", __func__, 0x1B);
			return -EINVAL;
		}

		ret = exynos_acpm_update_reg(acpm_mfd_node, MAIN_CHANNEL, I2C_BASE_PM, 0x19, 0x00, 0x0F);
		if (ret) {
			pr_err("%s: acpm ipc fail(%#x)\n", __func__, 0x19);
			return -EINVAL;
		}

		ret = exynos_acpm_update_reg(acpm_mfd_node, MAIN_CHANNEL, I2C_BASE_PM, 0xCF, 0x04, 0x04);
		if (ret) {
			pr_err("%s: acpm ipc fail(%#x)\n", __func__, 0xCF);
			return -EINVAL;
		}
	}
#endif
	return ret;
}

static int s2mpu13_pmic_probe(struct platform_device *pdev)
{
	struct s2mpu13_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mpu13_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mpu13_info *s2mpu13;
	int irq_base, ret;
	u32 i;
#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
	u8 offsrc_val[2] = {0, };
#endif
	pr_info("[PMIC] %s: start\n", __func__);

	if (iodev->dev->of_node) {
		ret = s2mpu13_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			goto err_pdata;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpu13 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpu13_info),
				GFP_KERNEL);
	if (!s2mpu13)
		return -ENOMEM;

	irq_base = pdata->irq_base;
	if (!irq_base) {
		dev_err(&pdev->dev, "Failed to get irq base %d\n", irq_base);
		return -ENODEV;
	}

	s2mpu13->iodev = iodev;
	s2mpu13->i2c = iodev->pmic;
	s2mpu13->gpio_i2c = iodev->gpio_i2c;

	mutex_init(&s2mpu13->lock);

	s2mpu13_static_info = s2mpu13;

	platform_set_drvdata(pdev, s2mpu13);

	/* Change min/max voltage and step */
	s2mpu13_set_reg_changes(s2mpu13);

	for (i = 0; i < pdata->num_rdata; i++) {
		int id = pdata->regulators[i].id;

		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mpu13;
		config.of_node = pdata->regulators[i].reg_node;
		s2mpu13->opmode[id] = regulators[id].enable_mask;
		s2mpu13->rdev[i] = devm_regulator_register(&pdev->dev,
							&regulators[id], &config);
		if (IS_ERR(s2mpu13->rdev[i])) {
			ret = PTR_ERR(s2mpu13->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n", i);
			s2mpu13->rdev[i] = NULL;
			goto err_s2mpu13_data;
		}
	}

	s2mpu13->num_regulators = pdata->num_rdata;

	ret = s2mpu13_set_sel_vgpio(s2mpu13, pdata);
	if (ret < 0) {
		pr_err("%s: s2mpu13_set_sel_vgpio fail\n", __func__);
		goto err_s2mpu13_data;
	}

	ret = s2mpu13_set_interrupt(pdev, s2mpu13, irq_base);
	if (ret < 0)
		pr_err("%s: s2mpu13_set_interrupt fail\n", __func__);

	ret = s2mpu13_oi_function(iodev);
	if (ret < 0)
		pr_err("%s: s2mpu13_oi_function fail\n", __func__);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2mpu13_create_sysfs(s2mpu13);
	if (ret < 0) {
		pr_err("%s: s2mpu13_create_sysfs fail\n", __func__);
		goto err_s2mpu13_data;
	}
#endif
	s2mpu13_set_regulator_vol(s2mpu13);

	s2mpu13_set_dropout_vol(s2mpu13);

	exynos_reboot_register_pmic_ops(s2mpu13_power_off_wa, NULL, NULL, s2mpu13_read_pwron_status);

#if IS_ENABLED(CONFIG_SEC_PM_DEBUG)
	ret = s2mpu13_read_reg(s2mpu13->i2c, S2MPU13_PMIC_REG_PWRONSRC,
			&pmic_onsrc);
	if (ret)
		dev_err(&pdev->dev, "failed to read PWRONSRC\n");

	ret = s2mpu13_bulk_read(s2mpu13->i2c, S2MPU13_PMIC_REG_OFFSRC1, 2,
			pmic_offsrc);
	if (ret)
		dev_err(&pdev->dev, "failed to read OFFSRC\n");

	/* Clear OFFSRC1, OFFSRC2 register */
	ret = s2mpu13_bulk_write(s2mpu13->i2c, S2MPU13_PMIC_REG_OFFSRC1, 2,
			offsrc_val);
	if (ret)
		dev_err(&pdev->dev, "failed to write OFFSRC\n");
#endif /* CONFIG_SEC_PM_DEBUG */

	pr_info("[PMIC] %s: end\n", __func__);

	return 0;

err_s2mpu13_data:
	mutex_destroy(&s2mpu13->lock);
err_pdata:
	return ret;
}

static int s2mpu13_pmic_remove(struct platform_device *pdev)
{
	struct s2mpu13_info *s2mpu13 = platform_get_drvdata(pdev);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mpu13_pmic = s2mpu13->dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++)
		device_remove_file(s2mpu13_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mpu13_pmic->devt);
#endif
	mutex_destroy(&s2mpu13->lock);
	return 0;
}

static void s2mpu13_pmic_shutdown(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

#if IS_ENABLED(CONFIG_SEC_PM)
	s2mpu13_set_instacok();
#endif /* CONFIG_SEC_PM */
}

#if IS_ENABLED(CONFIG_PM)
static int s2mpu13_pmic_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mpu13_info *s2mpu13 = platform_get_drvdata(pdev);
	int ret = 0;

	pr_info("%s\n", __func__);

	/* Off time reduction */
	ret = s2mpu13_write_reg(s2mpu13->i2c, 0xA4, 0x00);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xA4)\n", __func__);

	ret = s2mpu13_write_reg(s2mpu13->i2c, 0xA5, 0x00);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xA5)\n", __func__);

	ret = s2mpu13_write_reg(s2mpu13->i2c, 0xA6, 0x00);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xA6)\n", __func__);

	ret = s2mpu13_write_reg(s2mpu13->i2c, 0xA7, 0x00);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xA7)\n", __func__);

	return 0;
}

static int s2mpu13_pmic_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mpu13_info *s2mpu13 = platform_get_drvdata(pdev);
	int ret = 0;

	pr_info("%s\n", __func__);

	/* Restore off time reduction */
	ret = s2mpu13_write_reg(s2mpu13->i2c, 0xA4, 0x11);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xA4)\n", __func__);

	ret = s2mpu13_write_reg(s2mpu13->i2c, 0xA5, 0x13);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xA5)\n", __func__);

	ret = s2mpu13_write_reg(s2mpu13->i2c, 0xA6, 0x11);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xA6)\n", __func__);

	ret = s2mpu13_write_reg(s2mpu13->i2c, 0xA7, 0x10);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xA7)\n", __func__);

	return 0;
}
#else
#define s2mpu13_pmic_suspend	NULL
#define s2mpu13_pmic_resume	NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops s2mpu13_pmic_pm = {
	.suspend = s2mpu13_pmic_suspend,
	.resume = s2mpu13_pmic_resume,
};

static const struct platform_device_id s2mpu13_pmic_id[] = {
	{ "s2mpu13-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpu13_pmic_id);

static struct platform_driver s2mpu13_pmic_driver = {
	.driver = {
		.name = "s2mpu13-regulator",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm = &s2mpu13_pmic_pm,
#endif
		.suppress_bind_attrs = true,
	},
	.probe = s2mpu13_pmic_probe,
	.remove = s2mpu13_pmic_remove,
	.shutdown = s2mpu13_pmic_shutdown,
	.id_table = s2mpu13_pmic_id,
};

static int __init s2mpu13_pmic_init(void)
{
	return platform_driver_register(&s2mpu13_pmic_driver);
}
subsys_initcall(s2mpu13_pmic_init);

static void __exit s2mpu13_pmic_exit(void)
{
	platform_driver_unregister(&s2mpu13_pmic_driver);
}
module_exit(s2mpu13_pmic_exit);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2MPU13 Regulator Driver");
MODULE_LICENSE("GPL");
