/*
 * s2mpm07.c
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
#include <linux/mfd/samsung/s2mpm07.h>
#include <linux/mfd/samsung/s2mpm07-regulator.h>
//#include <linux/reset/exynos-reset.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/regulator/pmic_class.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#define RF_CHANNEL	2
static struct device_node *acpm_mfd_node;
#endif

#define I2C_BASE_VGPIO	0x00
#define I2C_BASE_COMMON	0x03
#define I2C_BASE_PM1	0x05
#define I2C_BASE_CLOSE1	0x0E
#define I2C_BASE_ADC	0x0A
#define I2C_BASE_GPIO	0x0B

static struct s2mpm07_info *s2mpm07_static_info;

struct s2mpm07_info {
	struct regulator_dev *rdev[S2MPM07_REGULATOR_MAX];
	unsigned int opmode[S2MPM07_REGULATOR_MAX];
	struct s2mpm07_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
	int num_regulators;
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	uint8_t base_addr;
	uint8_t read_addr;
	uint8_t read_val;
	struct device *dev;
#endif
};

static unsigned int s2mpm07_of_map_mode(unsigned int val) {
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
	struct s2mpm07_info *s2mpm07 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPM07_ENABLE_SHIFT;
	s2mpm07->opmode[id] = val;

	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mpm07_info *s2mpm07 = rdev_get_drvdata(rdev);

	return s2mpm07_update_reg(s2mpm07->i2c, rdev->desc->enable_reg,
				  s2mpm07->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mpm07_info *s2mpm07 = rdev_get_drvdata(rdev);
	unsigned int val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mpm07_update_reg(s2mpm07->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mpm07_info *s2mpm07 = rdev_get_drvdata(rdev);
	int ret;
	uint8_t val;

	ret = s2mpm07_read_reg(s2mpm07->i2c,
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
	struct s2mpm07_info *s2mpm07 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int ramp_value = 0;
	uint8_t ramp_addr = 0;

	ramp_value = get_ramp_delay(ramp_delay / 1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}
	ramp_value = 0x00;	// 6.25mv/us fixed

	if (s2mpm07->iodev->pmic_rev != 0x29) {
		switch (reg_id) {
		case S2MPM07_BUCK1:
			ramp_addr = S2MPM07_REG_BUCK1R_DVS;
			break;
		//case S2MPM07_BUCK_SR1:
		//	ramp_addr = S2MPM07_REG_BUCK_SR1R_DVS;
		//	break;
		default:
			return -EINVAL;
		}
	} else {
		switch (reg_id) {
		case S2MPM07_BUCK1_EVT0:
			ramp_addr = S2MPM07_REG_BUCK1R_DVS;
			break;
		//case S2MPM07_BUCK_SR1_EVT0:
		//	ramp_addr = S2MPM07_REG_BUCK_SR1R_DVS;
		//	break;
		default:
			return -EINVAL;
		}
	}

	return s2mpm07_update_reg(s2mpm07->i2c, ramp_addr,
				  ramp_value << BUCK_RAMP_UP_SHIFT,
				  BUCK_RAMP_MASK << BUCK_RAMP_UP_SHIFT);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mpm07_info *s2mpm07 = rdev_get_drvdata(rdev);
	int ret;
	uint8_t val;

	ret = s2mpm07_read_reg(s2mpm07->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpm07_info *s2mpm07 = rdev_get_drvdata(rdev);
	int ret;

	ret = s2mpm07_update_reg(s2mpm07->i2c, rdev->desc->vsel_reg,
				  sel, rdev->desc->vsel_mask);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mpm07_update_reg(s2mpm07->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpm07_info *s2mpm07 = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2mpm07_write_reg(s2mpm07->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mpm07_update_reg(s2mpm07->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);

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

static struct regulator_ops s2mpm07_ldo_ops = {
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

static struct regulator_ops s2mpm07_buck_ops = {
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
#if 0
static struct regulator_ops s2mpm07_bb_ops = {
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
#endif

#define _BUCK(macro)		S2MPM07_BUCK##macro
#define _buck_ops(num)		s2mpm07_buck_ops##num
#define _LDO(macro)		S2MPM07_LDO##macro
#define _ldo_ops(num)		s2mpm07_ldo_ops##num
#define _BB(macro)		S2MPM07_BB##macro
#define _bb_ops(num)		s2mpm07_bb_ops##num

#define _REG(ctrl)		S2MPM07_REG##ctrl
#define _TIME(macro)		S2MPM07_ENABLE_TIME##macro

#define _LDO_MIN(group)		S2MPM07_LDO_MIN##group
#define _LDO_STEP(group)	S2MPM07_LDO_STEP##group
#define _LDO_MASK(num)		S2MPM07_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2MPM07_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPM07_BUCK_STEP##group
#define _BB_MIN(group)		S2MPM07_BB_MIN##group
#define _BB_STEP(group)		S2MPM07_BB_STEP##group

#define BUCK_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPM07_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPM07_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPM07_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpm07_of_map_mode			\
}

#define LDO_DESC(_name, _id, _ops, g, v, v_m, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _LDO_MIN(g),				\
	.uV_step	= _LDO_STEP(g),				\
	.n_voltages	= _LDO_MASK(v_m) + 1,			\
	.vsel_reg	= v,					\
	.vsel_mask	= _LDO_MASK(v_m),			\
	.enable_reg	= e,					\
	.enable_mask	= S2MPM07_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpm07_of_map_mode			\
}
#if 0
#define BB_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BB_MIN(),				\
	.uV_step	= _BB_STEP(),				\
	.n_voltages	= S2MPM07_BB_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPM07_BB_VSEL_MASK,			\
	.enable_reg	= e,					\
	.enable_mask	= S2MPM07_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpm07_of_map_mode			\
}
#endif

/* EVT0 */
static struct regulator_desc regulators_evt0[S2MPM07_REG_MAX_EVT0] = {
	/* name, id, ops, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	// LDO 1R ~ 23R
	//LDO_DESC("LDO1", _LDO(1_EVT0), &_ldo_ops(), 1, _REG(_LDO1R_OUT), 1, _REG(_LDO1R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO2", _LDO(2_EVT0), &_ldo_ops(), 1, _REG(_LDO2R_OUT1), 1, _REG(_LDO2R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO3", _LDO(3_EVT0), &_ldo_ops(), 2, _REG(_LDO3R_OUT), 1, _REG(_LDO3R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO4", _LDO(4_EVT0), &_ldo_ops(), 1, _REG(_LDO4R_OUT), 1, _REG(_LDO4R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5_EVT0), &_ldo_ops(), 1, _REG(_LDO5R_OUT), 1, _REG(_LDO5R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO6", _LDO(6_EVT0), &_ldo_ops(), 2, _REG(_LDO6R_CTRL), 2, _REG(_LDO6R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO7", _LDO(7_EVT0), &_ldo_ops(), 1, _REG(_LDO7R_OUT), 1, _REG(_LDO7R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO8", _LDO(8_EVT0), &_ldo_ops(), 1, _REG(_LDO8R_OUT), 1, _REG(_LDO8R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO9", _LDO(9_EVT0), &_ldo_ops(), 1, _REG(_LDO9R_OUT), 1, _REG(_LDO9R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO10", _LDO(10_EVT0), &_ldo_ops(), 2, _REG(_LDO10R_CTRL), 2, _REG(_LDO10R_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11_EVT0), &_ldo_ops(), 3, _REG(_LDO11R_CTRL), 2, _REG(_LDO11R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO12", _LDO(12_EVT0), &_ldo_ops(), 1, _REG(_LDO12R_OUT), 1, _REG(_LDO12R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO13", _LDO(13_EVT0), &_ldo_ops(), 1, _REG(_LDO13R_OUT), 1, _REG(_LDO13R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO14", _LDO(14_EVT0), &_ldo_ops(), 1, _REG(_LDO14R_OUT), 1, _REG(_LDO14R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO15", _LDO(15_EVT0), &_ldo_ops(), 1, _REG(_LDO15R_OUT), 1, _REG(_LDO15R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO16", _LDO(16_EVT0), &_ldo_ops(), 2, _REG(_LDO16R_CTRL), 2, _REG(_LDO16R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO17", _LDO(17_EVT0), &_ldo_ops(), 2, _REG(_LDO17R_CTRL), 2, _REG(_LDO17R_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18_EVT0), &_ldo_ops(), 3, _REG(_LDO18R_CTRL), 2, _REG(_LDO18R_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19_EVT0), &_ldo_ops(), 3, _REG(_LDO19R_CTRL), 2, _REG(_LDO19R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO20", _LDO(20_EVT0), &_ldo_ops(), 3, _REG(_LDO20R_CTRL), 2, _REG(_LDO20R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO21", _LDO(21_EVT0), &_ldo_ops(), 3, _REG(_LDO21R_CTRL), 2, _REG(_LDO21R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO22", _LDO(22_EVT0), &_ldo_ops(), 3, _REG(_LDO22R_CTRL), 2, _REG(_LDO22R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO23", _LDO(23_EVT0), &_ldo_ops(), 3, _REG(_LDO23R_CTRL), 2, _REG(_LDO23R_CTRL), _TIME(_LDO)),

	// BUCK 1R, SR1R
	BUCK_DESC("BUCK1", _BUCK(1_EVT0), &_buck_ops(), 1, _REG(_BUCK1R_OUT1), _REG(_BUCK1R_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK_SR1", _BUCK(_SR1_EVT0), &_buck_ops(), 2, _REG(_BUCK_SR1R_OUT1), _REG(_BUCK_SR1R_CTRL), _TIME(_BUCK_SR)),
};

static struct regulator_desc regulators[S2MPM07_REG_MAX] = {
	/* name, id, ops, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	// LDO 1R ~ 21R
	//LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), 1, _REG(_LDO1R_OUT), 1, _REG(_LDO1R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), 1, _REG(_LDO2R_OUT1), 1, _REG(_LDO2R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), 2, _REG(_LDO3R_OUT), 1, _REG(_LDO3R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), 1, _REG(_LDO4R_OUT), 1, _REG(_LDO4R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), 1, _REG(_LDO5R_OUT), 1, _REG(_LDO5R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), 2, _REG(_LDO6R_CTRL), 2, _REG(_LDO6R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), 1, _REG(_LDO7R_OUT), 1, _REG(_LDO7R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), 1, _REG(_LDO8R_OUT), 1, _REG(_LDO8R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), 1, _REG(_LDO9R_OUT), 1, _REG(_LDO9R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), 2, _REG(_LDO10R_CTRL), 2, _REG(_LDO10R_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), 3, _REG(_LDO11R_CTRL), 2, _REG(_LDO11R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), 1, _REG(_LDO12R_OUT), 1, _REG(_LDO12R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), 1, _REG(_LDO13R_OUT), 1, _REG(_LDO13R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), 1, _REG(_LDO14R_OUT), 1, _REG(_LDO14R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), 1, _REG(_LDO15R_OUT), 1, _REG(_LDO15R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), 2, _REG(_LDO16R_CTRL), 2, _REG(_LDO16R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), 2, _REG(_LDO17R_CTRL), 2, _REG(_LDO17R_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), 3, _REG(_LDO18R_CTRL), 2, _REG(_LDO18R_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), 3, _REG(_LDO19R_CTRL), 2, _REG(_LDO19R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), 3, _REG(_LDO20R_CTRL), 2, _REG(_LDO20R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), 3, _REG(_LDO21R_CTRL), 2, _REG(_LDO21R_CTRL), _TIME(_LDO)),

	// BUCK 1R, SR1R
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), 1, _REG(_BUCK1R_OUT1), _REG(_BUCK1R_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK_SR1", _BUCK(_SR1), &_buck_ops(), 2, _REG(_BUCK_SR1R_OUT1), _REG(_BUCK_SR1R_CTRL), _TIME(_BUCK_SR)),
};

#if IS_ENABLED(CONFIG_OF)
static int s2mpm07_pmic_dt_parse_pdata(struct s2mpm07_dev *iodev,
				       struct s2mpm07_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mpm07_regulator_data *rdata;
	uint32_t i;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = pmic_np;
#endif
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

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) * pdata->num_regulators, GFP_KERNEL);
	if (!rdata)
		return -ENOMEM;

	pdata->regulators = rdata;
	pdata->num_rdata = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		if (iodev->pmic_rev != 0x29) {
			for (i = 0; i < ARRAY_SIZE(regulators); i++)
				if (!of_node_cmp(reg_np->name, regulators[i].name))
					break;

			if (i == ARRAY_SIZE(regulators)) {
				dev_warn(iodev->dev,
					 "[RF_PMIC] %s: don't know how to configure regulator %s\n",
					 __func__, reg_np->name);
				continue;
			}

			rdata->id = i;
			rdata->initdata = of_get_regulator_init_data(iodev->dev, reg_np, &regulators[i]);
			rdata->reg_node = reg_np;
			rdata++;
			pdata->num_rdata++;
		} else {
			for (i = 0; i < ARRAY_SIZE(regulators_evt0); i++)
				if (!of_node_cmp(reg_np->name, regulators_evt0[i].name))
					break;

			if (i == ARRAY_SIZE(regulators_evt0)) {
				dev_warn(iodev->dev,
					 "[RF_PMIC] %s: don't know how to configure regulator %s\n",
					 __func__, reg_np->name);
				continue;
			}

			rdata->id = i;
			rdata->initdata = of_get_regulator_init_data(iodev->dev, reg_np, &regulators_evt0[i]);
			rdata->reg_node = reg_np;
			rdata++;
			pdata->num_rdata++;
		}
	}

	return 0;
}
#else
static int s2mpm07_pmic_dt_parse_pdata(struct s2mpm07_pmic_dev *iodev,
					struct s2mpm07_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t s2mpm07_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mpm07_info *s2mpm07 = dev_get_drvdata(dev);
	int ret;
	uint8_t base_addr = 0, reg_addr = 0, val = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpm07_dev *iodev = s2mpm07->iodev;
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
	ret = exynos_acpm_read_reg(acpm_mfd_node, RF_CHANNEL, base_addr, reg_addr, &val);
	mutex_unlock(&iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to read i2c addr/data\n", __func__);
#endif

	pr_info("%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__,
		base_addr, reg_addr, val);
	s2mpm07->base_addr = base_addr;
	s2mpm07->read_addr = reg_addr;
	s2mpm07->read_val = val;

	return size;
}

static ssize_t s2mpm07_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mpm07_info *s2mpm07 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			s2mpm07->base_addr, s2mpm07->read_addr, s2mpm07->read_val);
}

static ssize_t s2mpm07_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct s2mpm07_info *s2mpm07 = dev_get_drvdata(dev);
	int ret;
	uint8_t base_addr = 0, reg_addr = 0, data = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpm07_dev *iodev = s2mpm07->iodev;
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
	case I2C_BASE_VGPIO:
	case I2C_BASE_COMMON:
	case I2C_BASE_PM1:
	case I2C_BASE_ADC:
	case I2C_BASE_CLOSE1:
	case I2C_BASE_GPIO:
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return size;
	}

	pr_info("%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__,
		base_addr, reg_addr, data);

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	mutex_lock(&iodev->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, RF_CHANNEL, base_addr, reg_addr, data);
	mutex_unlock(&iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to write i2c addr/data\n", __func__);
#endif

	return size;
}

static ssize_t s2mpm07_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mpm07_write\n");
}

static struct pmic_device_attribute regulator_attr[] = {
	PMIC_ATTR(s2mpm07_write, S_IRUGO | S_IWUSR, s2mpm07_write_show, s2mpm07_write_store),
	PMIC_ATTR(s2mpm07_read, S_IRUGO | S_IWUSR, s2mpm07_read_show, s2mpm07_read_store),
};

static int s2mpm07_create_sysfs(struct s2mpm07_info *s2mpm07)
{
	struct device *s2mpm07_pmic = s2mpm07->dev;
	struct device *dev = s2mpm07->iodev->dev;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);
	s2mpm07->base_addr = 0;
	s2mpm07->read_addr = 0;
	s2mpm07->read_val = 0;

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mpm07_pmic = pmic_device_create(s2mpm07, device_name);
	s2mpm07->dev = s2mpm07_pmic;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++) {
		err = device_create_file(s2mpm07_pmic, &regulator_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mpm07_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mpm07_pmic->devt);

	return -1;
}
#endif

static int s2mpm07_pmic_probe(struct platform_device *pdev)
{
	struct s2mpm07_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mpm07_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mpm07_info *s2mpm07;
	int ret;
	uint32_t i;

	pr_info("[RF_PMIC] %s: start\n", __func__);

	if (iodev->dev->of_node) {
		ret = s2mpm07_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			goto err_pdata;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpm07 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpm07_info), GFP_KERNEL);
	if (!s2mpm07)
		return -ENOMEM;

	s2mpm07->iodev = iodev;
	s2mpm07->i2c = iodev->pmic1;

	mutex_init(&s2mpm07->lock);

	s2mpm07_static_info = s2mpm07;

	platform_set_drvdata(pdev, s2mpm07);

	config.dev = &pdev->dev;
	config.driver_data = s2mpm07;
	for (i = 0; i < pdata->num_rdata; i++) {
		int id = pdata->regulators[i].id;
		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].reg_node;
		if (iodev->pmic_rev != 0x29) {
			s2mpm07->opmode[id] = regulators[id].enable_mask;
			s2mpm07->rdev[i] = devm_regulator_register(&pdev->dev,
								   &regulators[id], &config);
		} else {
			s2mpm07->opmode[id] = regulators_evt0[id].enable_mask;
			s2mpm07->rdev[i] = devm_regulator_register(&pdev->dev,
								   &regulators_evt0[id], &config);
		}
		if (IS_ERR(s2mpm07->rdev[i])) {
			ret = PTR_ERR(s2mpm07->rdev[i]);
			dev_err(&pdev->dev, "[RF_PMIC] regulator init failed for %s(%d)\n",
				regulators[i].name, i);
			goto err_s2mpm07_data;
		}
	}

	s2mpm07->num_regulators = pdata->num_rdata;

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2mpm07_create_sysfs(s2mpm07);
	if (ret < 0) {
		pr_err("%s: s2mpm07_create_sysfs fail\n", __func__);
		goto err_s2mpm07_data;
	}
#endif
	pr_info("[RF_PMIC] %s: end\n", __func__);

	return 0;

err_s2mpm07_data:
	mutex_destroy(&s2mpm07->lock);
err_pdata:
	return ret;
}

static int s2mpm07_pmic_remove(struct platform_device *pdev)
{
	struct s2mpm07_info *s2mpm07 = platform_get_drvdata(pdev);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mpm07_pmic = s2mpm07->dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++)
		device_remove_file(s2mpm07_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mpm07_pmic->devt);
#endif
	mutex_destroy(&s2mpm07->lock);
	return 0;
}

static void s2mpm07_pmic_shutdown(struct platform_device *pdev)
{
	pr_info("%s()\n", __func__);
}

static const struct platform_device_id s2mpm07_pmic_id[] = {
	{ "s2mpm07-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpm07_pmic_id);

static struct platform_driver s2mpm07_pmic_driver = {
	.driver = {
		.name = "s2mpm07-regulator",
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
	},
	.probe = s2mpm07_pmic_probe,
	.remove = s2mpm07_pmic_remove,
	.shutdown = s2mpm07_pmic_shutdown,
	.id_table = s2mpm07_pmic_id,
};

module_platform_driver(s2mpm07_pmic_driver);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2MPM07 Regulator Driver");
MODULE_LICENSE("GPL");
