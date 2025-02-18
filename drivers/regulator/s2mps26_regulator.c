/*
 * s2mps26.c
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
#include <linux/mfd/samsung/s2mps26.h>
#include <linux/mfd/samsung/s2mps26-regulator.h>
//#include <linux/reset/exynos-reset.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/regulator/pmic_class.h>
#if IS_ENABLED(CONFIG_SEC_PM)
#include <linux/sec_class.h>
#endif /* CONFIG_SEC_PM */
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#define SUB_CHANNEL	1
static struct device_node *acpm_mfd_node;
#endif

#define I2C_BASE_VGPIO	0x00
#define I2C_BASE_COMMON	0x03
#define I2C_BASE_PM1	0x05
#define I2C_BASE_PM2	0x06
#define I2C_BASE_CLOSE1	0x0E
#define I2C_BASE_CLOSE2	0x0F
#define I2C_BASE_ADC	0x0A
#define I2C_BASE_GPIO	0x0B

static struct s2mps26_info *s2mps26_static_info;
struct s2mps26_info {
	struct regulator_dev *rdev[S2MPS26_REGULATOR_MAX];
	unsigned int opmode[S2MPS26_REGULATOR_MAX];
	struct s2mps26_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
	int num_regulators;
	int wtsr_en;
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	uint8_t base_addr;
	uint8_t read_addr;
	uint8_t read_val;
	struct device *dev;
#endif
};

static unsigned int s2mps26_of_map_mode(unsigned int val) {
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
	struct s2mps26_info *s2mps26 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPS26_ENABLE_SHIFT;
	s2mps26->opmode[id] = val;

	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mps26_info *s2mps26 = rdev_get_drvdata(rdev);

	return s2mps26_update_reg(s2mps26->i2c, rdev->desc->enable_reg,
				  s2mps26->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mps26_info *s2mps26 = rdev_get_drvdata(rdev);
	unsigned int val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mps26_update_reg(s2mps26->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mps26_info *s2mps26 = rdev_get_drvdata(rdev);
	int ret;
	uint8_t val;

	ret = s2mps26_read_reg(s2mps26->i2c,
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
	struct s2mps26_info *s2mps26 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int ramp_value = 0;
	uint8_t ramp_addr = 0;

	ramp_value = get_ramp_delay(ramp_delay / 1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}
	ramp_value = 0x00; 	// 6.25mv/us fixed

	if (s2mps26->iodev->pmic_rev) {
		switch (reg_id) {
		case S2MPS26_BUCK1:
			ramp_addr = S2MPS26_REG_BUCK1S_DVS;
			break;
		case S2MPS26_BUCK2:
			ramp_addr = S2MPS26_REG_BUCK2S_DVS;
			break;
		case S2MPS26_BUCK3:
			ramp_addr = S2MPS26_REG_BUCK3S_DVS;
			break;
		case S2MPS26_BUCK4:
			ramp_addr = S2MPS26_REG_BUCK4S_DVS;
			break;
		case S2MPS26_BUCK5:
			ramp_addr = S2MPS26_REG_BUCK5S_DVS;
			break;
		case S2MPS26_BUCK6:
			ramp_addr = S2MPS26_REG_BUCK6S_DVS;
			break;
		case S2MPS26_BUCK7:
			ramp_addr = S2MPS26_REG_BUCK7S_DVS;
			break;
		//case S2MPS26_BUCK8:
		//	ramp_addr = S2MPS26_REG_BUCK8S_DVS;
		//	break;
		//case S2MPS26_BUCK9:
		//	ramp_addr = S2MPS26_REG_BUCK9S_DVS;
		//	break;
		//case S2MPS26_BUCK10:
		//	ramp_addr = S2MPS26_REG_BUCK10S_DVS;
		//	break;
		//case S2MPS26_BUCK11:
		//	ramp_addr = S2MPS26_REG_BUCK11S_DVS;
		//	break;
		//case S2MPS26_BUCK12:
		//	ramp_addr = S2MPS26_REG_BUCK12S_DVS;
		//	break;
		case S2MPS26_BUCK_SR1:
			ramp_addr = S2MPS26_REG_SR1S_DVS;
			break;
		case S2MPS26_BB:
			ramp_addr = S2MPS26_REG_BB_DVS1;
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (reg_id) {
		case S2MPS26_BUCK1_EVT0:
			ramp_addr = S2MPS26_REG_BUCK1S_DVS;
			break;
		case S2MPS26_BUCK2_EVT0:
			ramp_addr = S2MPS26_REG_BUCK2S_DVS;
			break;
		case S2MPS26_BUCK3_EVT0:
			ramp_addr = S2MPS26_REG_BUCK3S_DVS;
			break;
		case S2MPS26_BUCK4_EVT0:
			ramp_addr = S2MPS26_REG_BUCK4S_DVS;
			break;
		case S2MPS26_BUCK5_EVT0:
			ramp_addr = S2MPS26_REG_BUCK5S_DVS;
			break;
		case S2MPS26_BUCK6_EVT0:
			ramp_addr = S2MPS26_REG_BUCK6S_DVS;
			break;
		case S2MPS26_BUCK7_EVT0:
			ramp_addr = S2MPS26_REG_BUCK7S_DVS;
			break;
		case S2MPS26_BUCK8_EVT0:
			ramp_addr = S2MPS26_REG_BUCK8S_DVS;
			break;
		case S2MPS26_BUCK9_EVT0:
			ramp_addr = S2MPS26_REG_BUCK9S_DVS;
			break;
		case S2MPS26_BUCK10_EVT0:
			ramp_addr = S2MPS26_REG_BUCK10S_DVS;
			break;
		case S2MPS26_BUCK11_EVT0:
			ramp_addr = S2MPS26_REG_BUCK11S_DVS;
			break;
		case S2MPS26_BUCK12_EVT0:
			ramp_addr = S2MPS26_REG_BUCK12S_DVS;
			break;
		case S2MPS26_BUCK_SR1_EVT0:
			ramp_addr = S2MPS26_REG_SR1S_DVS;
			break;
		case S2MPS26_BB_EVT0:
			ramp_addr = S2MPS26_REG_BB_DVS1;
			break;
		default:
			return -EINVAL;
		}
	}
	return s2mps26_update_reg(s2mps26->i2c, ramp_addr,
				  ramp_value << BUCK_RAMP_UP_SHIFT,
				  BUCK_RAMP_MASK << BUCK_RAMP_UP_SHIFT);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mps26_info *s2mps26 = rdev_get_drvdata(rdev);
	int ret;
	uint8_t val;

	ret = s2mps26_read_reg(s2mps26->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mps26_info *s2mps26 = rdev_get_drvdata(rdev);
	int ret;

	ret = s2mps26_update_reg(s2mps26->i2c, rdev->desc->vsel_reg,
				 sel, rdev->desc->vsel_mask);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mps26_update_reg(s2mps26->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mps26_info *s2mps26 = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2mps26_write_reg(s2mps26->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mps26_update_reg(s2mps26->i2c, rdev->desc->apply_reg,
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

static struct regulator_ops s2mps26_ldo_ops = {
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

static struct regulator_ops s2mps26_buck_ops = {
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

static struct regulator_ops s2mps26_bb_ops = {
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

#define _BUCK(macro)		S2MPS26_BUCK##macro
#define _buck_ops(num)		s2mps26_buck_ops##num
#define _LDO(macro)		S2MPS26_LDO##macro
#define _ldo_ops(num)		s2mps26_ldo_ops##num
#define _BB(macro)              S2MPS26_BB##macro
#define _bb_ops(num)  		s2mps26_bb_ops##num

#define _REG(ctrl)		S2MPS26_REG##ctrl
#define _TIME(macro)		S2MPS26_ENABLE_TIME##macro

#define _LDO_MIN(group)		S2MPS26_LDO_MIN##group
#define _LDO_STEP(group)	S2MPS26_LDO_STEP##group
#define _LDO_MASK(num)		S2MPS26_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2MPS26_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPS26_BUCK_STEP##group
#define _BB_MIN(group)		S2MPS26_BB_MIN##group
#define _BB_STEP(group)		S2MPS26_BB_STEP##group

#define BUCK_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPS26_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS26_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS26_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps26_of_map_mode			\
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
	.enable_mask	= S2MPS26_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps26_of_map_mode			\
}

#define BB_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BB_MIN(g),				\
	.uV_step	= _BB_STEP(g),				\
	.n_voltages	= S2MPS26_BB_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS26_BB_VSEL_MASK,			\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS26_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mps26_of_map_mode			\
}

/* EVT */
static struct regulator_desc regulators_evt0[S2MPS26_REG_MAX_EVT0] = {
	/* name, id, ops, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	/* LDO 1~20 */
	LDO_DESC("LDO1", _LDO(1_EVT0), &_ldo_ops(), 1, _REG(_LDO1S_OUT), 1, _REG(_LDO1S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO2", _LDO(2_EVT0), &_ldo_ops(), 1, _REG(_LDO2S_OUT1), 1, _REG(_LDO2S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3_EVT0), &_ldo_ops(), 1, _REG(_LDO3S_OUT), 1, _REG(_LDO3S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4_EVT0), &_ldo_ops(), 1, _REG(_LDO4S_OUT), 1, _REG(_LDO4S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5_EVT0), &_ldo_ops(), 1, _REG(_LDO5S_OUT), 1, _REG(_LDO5S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6_EVT0), &_ldo_ops(), 1, _REG(_LDO6S_OUT1), 1, _REG(_LDO6S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7_EVT0), &_ldo_ops(), 1, _REG(_LDO7S_OUT), 1, _REG(_LDO7S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8_EVT0), &_ldo_ops(), 1, _REG(_LDO8S_OUT), 1, _REG(_LDO8S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9_EVT0), &_ldo_ops(), 1, _REG(_LDO9S_OUT2), 1, _REG(_LDO9S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO10", _LDO(10_EVT0), &_ldo_ops(), 1, _REG(_LDO10S_OUT), 1, _REG(_LDO10S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO11", _LDO(11_EVT0), &_ldo_ops(), 1, _REG(_LDO11S_OUT), 1, _REG(_LDO11S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO12", _LDO(12_EVT0), &_ldo_ops(), 1, _REG(_LDO12S_OUT), 1, _REG(_LDO12S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO13", _LDO(13_EVT0), &_ldo_ops(), 1, _REG(_LDO13S_OUT), 1, _REG(_LDO13S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14_EVT0), &_ldo_ops(), 2, _REG(_LDO14S_CTRL), 2, _REG(_LDO14S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15_EVT0), &_ldo_ops(), 2, _REG(_LDO15S_CTRL), 2, _REG(_LDO15S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16_EVT0), &_ldo_ops(), 2, _REG(_LDO16S_CTRL), 2, _REG(_LDO16S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17_EVT0), &_ldo_ops(), 2, _REG(_LDO17S_CTRL), 2, _REG(_LDO17S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18_EVT0), &_ldo_ops(), 2, _REG(_LDO18S_CTRL), 2, _REG(_LDO18S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19_EVT0), &_ldo_ops(), 2, _REG(_LDO19S_CTRL), 2, _REG(_LDO19S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20_EVT0), &_ldo_ops(), 2, _REG(_LDO20S_CTRL), 2, _REG(_LDO20S_CTRL), _TIME(_LDO)),

	/* BUCK 1S ~ 12S, SR1S, BB */
	BUCK_DESC("BUCK1", _BUCK(1_EVT0), &_buck_ops(), 1, _REG(_BUCK1S_OUT2), _REG(_BUCK1S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2_EVT0), &_buck_ops(), 1, _REG(_BUCK2S_OUT2), _REG(_BUCK2S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3_EVT0), &_buck_ops(), 1, _REG(_BUCK3S_OUT1), _REG(_BUCK3S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4_EVT0), &_buck_ops(), 1, _REG(_BUCK4S_OUT1), _REG(_BUCK4S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5_EVT0), &_buck_ops(), 1, _REG(_BUCK5S_OUT3), _REG(_BUCK5S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK6", _BUCK(6_EVT0), &_buck_ops(), 1, _REG(_BUCK6S_OUT3), _REG(_BUCK6S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK7", _BUCK(7_EVT0), &_buck_ops(), 1, _REG(_BUCK7S_OUT3), _REG(_BUCK7S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK8", _BUCK(8_EVT0), &_buck_ops(), 1, _REG(_BUCK8S_OUT3), _REG(_BUCK8S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK9", _BUCK(9_EVT0), &_buck_ops(), 2, _REG(_BUCK9S_OUT1), _REG(_BUCK9S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK10", _BUCK(10_EVT0), &_buck_ops(), 1, _REG(_BUCK10S_OUT2), _REG(_BUCK10S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK11", _BUCK(11_EVT0), &_buck_ops(), 1, _REG(_BUCK11S_OUT2), _REG(_BUCK11S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK12", _BUCK(12_EVT0), &_buck_ops(), 1, _REG(_BUCK12S_OUT2), _REG(_BUCK12S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1_EVT0), &_buck_ops(), 3, _REG(_SR1S_OUT1), _REG(_SR1S_CTRL), _TIME(_BUCK_SR)),
	BB_DESC("BUCKB", _BB(_EVT0), &_bb_ops(), 1, _REG(_BB_OUT1), _REG(_BB_CTRL), _TIME(_BB)),
};

static struct regulator_desc regulators[S2MPS26_REG_MAX] = {
	/* name, id, ops, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	/* LDO 1~20 */
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), 1, _REG(_LDO1S_OUT), 1, _REG(_LDO1S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), 1, _REG(_LDO2S_OUT1), 1, _REG(_LDO2S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), 1, _REG(_LDO3S_OUT), 1, _REG(_LDO3S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), 1, _REG(_LDO4S_OUT), 1, _REG(_LDO4S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), 1, _REG(_LDO5S_OUT), 1, _REG(_LDO5S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), 1, _REG(_LDO6S_OUT1), 1, _REG(_LDO6S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), 1, _REG(_LDO7S_OUT), 1, _REG(_LDO7S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), 1, _REG(_LDO8S_OUT), 1, _REG(_LDO8S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), 1, _REG(_LDO9S_OUT2), 1, _REG(_LDO9S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), 1, _REG(_LDO10S_OUT), 1, _REG(_LDO10S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), 1, _REG(_LDO11S_OUT), 1, _REG(_LDO11S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), 1, _REG(_LDO12S_OUT), 1, _REG(_LDO12S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), 1, _REG(_LDO13S_OUT), 1, _REG(_LDO13S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), 2, _REG(_LDO14S_CTRL), 2, _REG(_LDO14S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), 2, _REG(_LDO15S_CTRL), 2, _REG(_LDO15S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), 2, _REG(_LDO16S_CTRL), 2, _REG(_LDO16S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), 2, _REG(_LDO17S_CTRL), 2, _REG(_LDO17S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), 2, _REG(_LDO18S_CTRL), 2, _REG(_LDO18S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), 2, _REG(_LDO19S_CTRL), 2, _REG(_LDO19S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), 2, _REG(_LDO20S_CTRL), 2, _REG(_LDO20S_CTRL), _TIME(_LDO)),

	/* BUCK 1S ~ 12S, SR1S, BB */
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), 1, _REG(_BUCK1S_OUT2), _REG(_BUCK1S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), 1, _REG(_BUCK2S_OUT2), _REG(_BUCK2S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), 1, _REG(_BUCK3S_OUT1), _REG(_BUCK3S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), 1, _REG(_BUCK4S_OUT1), _REG(_BUCK4S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), 1, _REG(_BUCK5S_OUT2), _REG(_BUCK5S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), 1, _REG(_BUCK6S_OUT2), _REG(_BUCK6S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), 1, _REG(_BUCK7S_OUT2), _REG(_BUCK7S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), 1, _REG(_BUCK8S_OUT1), _REG(_BUCK8S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK9", _BUCK(9), &_buck_ops(), 2, _REG(_BUCK9S_OUT1), _REG(_BUCK9S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK10", _BUCK(10), &_buck_ops(), 1, _REG(_BUCK10S_OUT1), _REG(_BUCK10S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK11", _BUCK(11), &_buck_ops(), 1, _REG(_BUCK11S_OUT1), _REG(_BUCK11S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK12", _BUCK(12), &_buck_ops(), 1, _REG(_BUCK12S_OUT1), _REG(_BUCK12S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), &_buck_ops(), 3, _REG(_SR1S_OUT1), _REG(_SR1S_CTRL), _TIME(_BUCK_SR)),
	BB_DESC("BUCKB", _BB(), &_bb_ops(), 1, _REG(_BB_OUT1), _REG(_BB_CTRL), _TIME(_BB)),
};


#if IS_ENABLED(CONFIG_OF)
static int s2mps26_pmic_dt_parse_pdata(struct s2mps26_dev *iodev,
				       struct s2mps26_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mps26_regulator_data *rdata;
	uint32_t i, val;
	int ret;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = pmic_np;
#endif
	/* wtsr_en */
	pdata->wtsr_en = 0;
	ret = of_property_read_u32(pmic_np, "wtsr_en", &val);
	if (ret < 0)
		pr_info("%s: fail to read wtsr_en\n", __func__);
	pdata->wtsr_en = val;

	/* adc_mode */
	pdata->adc_mode = 0;
	ret = of_property_read_u32(pmic_np, "adc_mode", &val);
	if (ret)
		return -EINVAL;
	pdata->adc_mode = val;

#if IS_ENABLED(CONFIG_SEC_PM)
	pdata->ldo4s_boot_off = of_property_read_bool(pmic_np, "ldo4s,boot-off");
#endif /* CONFIG_SEC_PM */

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
		if (iodev->pmic_rev) {
			for (i = 0; i < ARRAY_SIZE(regulators); i++)
				if (!of_node_cmp(reg_np->name, regulators[i].name))
					break;

			if (i == ARRAY_SIZE(regulators)) {
				dev_warn(iodev->dev,
					 "[PMIC] %s: don't know how to configure regulator %s\n",
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
					 "[PMIC] %s: don't know how to configure regulator %s\n",
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
static int s2mps26_pmic_dt_parse_pdata(struct s2mps26_pmic_dev *iodev,
				       struct s2mps26_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_EXYNOS_AFM)
int sub_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	if (!i2c)
		return -ENODEV;

	return s2mps26_update_reg(i2c, reg, val, mask);
}
EXPORT_SYMBOL_GPL(sub_pmic_update_reg);

int sub_pmic_get_i2c(struct i2c_client **i2c)
{
	if (!s2mps26_static_info)
		return -ENODEV;

	*i2c = s2mps26_static_info->i2c;

	return 0;
}
EXPORT_SYMBOL_GPL(sub_pmic_get_i2c);
#endif

struct s2mps26_oi_data {
	uint8_t reg;
	uint8_t val;
};

#define DECLARE_OI(_reg, _val) { .reg = (_reg), .val = (_val) }
static const struct s2mps26_oi_data s2mps26_oi[] = {
	/* BUCK 1~12, SR1M, BB OI function enable, OI power down,OVP disable */
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPS26_REG_BUCK1S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BUCK2S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BUCK3S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BUCK4S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BUCK5S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BUCK6S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BUCK7S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BUCK8S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BUCK9S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BUCK10S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BUCK11S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BUCK12S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_SR1S_OCP, 0xC4),
	DECLARE_OI(S2MPS26_REG_BB_OCP, 0xC4),
};

static int s2mps26_oi_function(struct s2mps26_dev *iodev)
{
	struct i2c_client *i2c = iodev->pmic1;
	uint32_t i;
	uint8_t val;
	int ret, cnt = 0;
	char buf[1024] = {0, };

	for (i = 0; i < ARRAY_SIZE(s2mps26_oi); i++) {
		ret = s2mps26_write_reg(i2c, s2mps26_oi[i].reg, s2mps26_oi[i].val);
		if (ret) {
			pr_err("%s: failed to write register\n", __func__);
			goto err;
		}
	}

	for (i = 0; i < ARRAY_SIZE(s2mps26_oi); i++) {
		ret = s2mps26_read_reg(i2c, s2mps26_oi[i].reg, &val);
		if (ret)
			goto err;

		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "0x%x[0x%02hhx], ", s2mps26_oi[i].reg, val);
	}
	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -1;
}

static void s2mps26_wtsr_enable(struct s2mps26_info *s2mps26,
				struct s2mps26_platform_data *pdata)
{
	int ret;

	pr_info("[PMIC] %s: WTSR (%s)\n", __func__,
		pdata->wtsr_en ? "enable" : "disable");

	ret = s2mps26_update_reg(s2mps26->i2c, S2MPS26_REG_CFG_PM,
				 S2MPS26_WTSREN_MASK, S2MPS26_WTSREN_MASK);

	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);

	s2mps26->wtsr_en = pdata->wtsr_en;
}

static void s2mps26_wtsr_disable(struct s2mps26_info *s2mps26)
{
	int ret;

	pr_info("[PMIC] %s: disable WTSR\n", __func__);

	ret = s2mps26_update_reg(s2mps26->i2c, S2MPS26_REG_CFG_PM,
				 0x00, S2MPS26_WTSREN_MASK);
	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t s2mps26_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mps26_info *s2mps26 = dev_get_drvdata(dev);
	int ret;
	uint8_t base_addr = 0, reg_addr = 0, val = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps26_dev *iodev = s2mps26->iodev;
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
	ret = exynos_acpm_read_reg(acpm_mfd_node, SUB_CHANNEL, base_addr, reg_addr, &val);
	mutex_unlock(&iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to read i2c addr/data\n", __func__);
#endif

	pr_info("%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__,
		base_addr, reg_addr, val);
	s2mps26->base_addr = base_addr;
	s2mps26->read_addr = reg_addr;
	s2mps26->read_val = val;

	return size;
}

static ssize_t s2mps26_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mps26_info *s2mps26 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			s2mps26->base_addr, s2mps26->read_addr, s2mps26->read_val);
}

static ssize_t s2mps26_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct s2mps26_info *s2mps26 = dev_get_drvdata(dev);
	int ret;
	uint8_t base_addr = 0, reg_addr = 0, data = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps26_dev *iodev = s2mps26->iodev;
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
	case I2C_BASE_PM2:
	case I2C_BASE_ADC:
	case I2C_BASE_GPIO:
	case I2C_BASE_CLOSE1:
	case I2C_BASE_CLOSE2:
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return size;
	}

	pr_info("%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__,
		base_addr, reg_addr, data);

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	mutex_lock(&iodev->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, SUB_CHANNEL, base_addr, reg_addr, data);
	mutex_unlock(&iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to write i2c addr/data\n", __func__);
#endif

	return size;
}

static ssize_t s2mps26_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mps26_write\n");
}

static struct pmic_device_attribute regulator_attr[] = {
	PMIC_ATTR(s2mps26_write, S_IRUGO | S_IWUSR, s2mps26_write_show, s2mps26_write_store),
	PMIC_ATTR(s2mps26_read, S_IRUGO | S_IWUSR, s2mps26_read_show, s2mps26_read_store),
};

static int s2mps26_create_sysfs(struct s2mps26_info *s2mps26)
{
	struct device *s2mps26_pmic = s2mps26->dev;
	struct device *dev = s2mps26->iodev->dev;
	char device_name[32] = {0,};
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);
	s2mps26->base_addr = 0;
	s2mps26->read_addr = 0;
	s2mps26->read_val = 0;

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mps26_pmic = pmic_device_create(s2mps26, device_name);
	s2mps26->dev = s2mps26_pmic;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++) {
		err = device_create_file(s2mps26_pmic, &regulator_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mps26_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mps26_pmic->devt);

	return -1;
}
#endif

static int s2mps26_set_powermeter(struct s2mps26_dev *iodev,
				  struct s2mps26_platform_data *pdata)
{
	if (pdata->adc_mode > 0) {
		iodev->adc_mode = pdata->adc_mode;
		s2mps26_powermeter_init(iodev);

		return 0;
	}

	return -1;
}

static int s2mps26_set_regulator_vol(struct s2mps26_info *s2mps26)
{
	int ret = 0;

	if (s2mps26->iodev->pmic_rev == 0x00) {
		/* BUCK1S(VDD_MIF), BUCK2S(VDD_SRAM), BUCK8S(VDD_CPUCL0) */
		ret |= s2mps26_write_reg(s2mps26->i2c, S2MPS26_REG_BUCK1S_OUT1, 0x24);  //0.525V
		ret |= s2mps26_write_reg(s2mps26->i2c, S2MPS26_REG_BUCK2S_OUT1, 0x34);  //0.625V
		ret |= s2mps26_write_reg(s2mps26->i2c, S2MPS26_REG_BUCK8S_OUT1, 0x30);  //0.6V

		if (ret) {
			pr_err("%s %d: failed to write register\n", __func__, __LINE__);
			return -1;
		}
	} else {
		/* BUCK1S(VDD_MIF), BUCK2S(VDD_SRAM) */
		ret |= s2mps26_write_reg(s2mps26->i2c, S2MPS26_REG_BUCK1S_OUT1, 0x1C);  //0.475V
		ret |= s2mps26_write_reg(s2mps26->i2c, S2MPS26_REG_BUCK2S_OUT1, 0x30);  //0.600V

		/* LDO1S 0.9V SR1S 0.95V */
		ret |= exynos_acpm_write_reg(acpm_mfd_node, SUB_CHANNEL, I2C_BASE_PM2, S2MPS26_REG_SR_SEL_DVS_EN1, 0x04);
		ret |= s2mps26_write_reg(s2mps26->i2c, S2MPS26_REG_SR1S_OUT1, 0x68);  //0.950V
		ret |= s2mps26_write_reg(s2mps26->i2c, S2MPS26_REG_LDO1S_OUT, 0x60);  //0.900V
		if (ret) {
			pr_err("%s %d: failed to write register\n", __func__, __LINE__);
			return -1;
		}
	}

	return 0;
}

static int s2mps26_pmic_probe(struct platform_device *pdev)
{
	struct s2mps26_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mps26_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mps26_info *s2mps26;
	int ret;
	uint32_t i;

	pr_info("[PMIC] %s: start\n", __func__);

	if (iodev->dev->of_node) {
		ret = s2mps26_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			goto err_pdata;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mps26 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps26_info), GFP_KERNEL);
	if (!s2mps26)
		return -ENOMEM;

	s2mps26->iodev = iodev;
	s2mps26->i2c = iodev->pmic1;

	mutex_init(&s2mps26->lock);

	s2mps26_static_info = s2mps26;

	platform_set_drvdata(pdev, s2mps26);

	config.dev = &pdev->dev;
	config.driver_data = s2mps26;
	for (i = 0; i < pdata->num_rdata; i++) {
		int id = pdata->regulators[i].id;
		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].reg_node;
		if (iodev->pmic_rev) {
			s2mps26->opmode[id] = regulators[id].enable_mask;
			s2mps26->rdev[i] = devm_regulator_register(&pdev->dev,
								   &regulators[id], &config);
		} else {
			s2mps26->opmode[id] = regulators_evt0[id].enable_mask;
			s2mps26->rdev[i] = devm_regulator_register(&pdev->dev,
								   &regulators_evt0[id], &config);
		}
		if (IS_ERR(s2mps26->rdev[i])) {
			ret = PTR_ERR(s2mps26->rdev[i]);
			dev_err(&pdev->dev, "[PMIC] regulator init failed for %s(%d)\n",
				regulators[i].name, i);
			goto err_s2mps26_data;
		}
	}

	s2mps26->num_regulators = pdata->num_rdata;

	if (pdata->wtsr_en)
		s2mps26_wtsr_enable(s2mps26, pdata);

	ret = s2mps26_oi_function(iodev);
	if (ret < 0)
		pr_err("%s: s2mps26_oi_function fail\n", __func__);

#if IS_ENABLED(CONFIG_SEC_PM)
	/* ap_sub_pmic_dev should be initialized before power meter initialization */
	iodev->ap_sub_pmic_dev = sec_device_create(NULL, "ap_sub_pmic");
#endif /* CONFIG_SEC_PM */

	ret = s2mps26_set_powermeter(iodev, pdata);
	if (ret < 0)
		pr_err("%s: Powermeter disable\n", __func__);

	//exynos_reboot_register_pmic_ops(NULL, s2mps26_power_off_wa, NULL, NULL);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2mps26_create_sysfs(s2mps26);
	if (ret < 0) {
		pr_err("%s: s2mps26_create_sysfs fail\n", __func__);
		goto err_s2mps26_data;
	}
#endif
	ret = s2mps26_set_regulator_vol(s2mps26);
	if (ret < 0) {
		pr_err("%s: s2mps26_set_regulator_vol fail\n", __func__);
		goto err_s2mps26_data;
	}


#if IS_ENABLED(CONFIG_SEC_PM)
	if (pdata->ldo4s_boot_off) {
		/* 1. 0x5CF [7:6]=00 : LDO4S off
		 * 2. 0x672 [3]=0    : Disable on/off by DRAM_SEL
		 */
		s2mps26_update_reg(s2mps26->i2c, S2MPS26_REG_LDO4S_CTRL, 0x00,
				S2MPS26_ENABLE_MASK);
		s2mps26_update_reg(iodev->pmic2, 0x72, 0x00, 1 << 3);
	}
#endif /* CONFIG_SEC_PM */
	pr_info("[PMIC] %s: end\n", __func__);

	return 0;

err_s2mps26_data:
	mutex_destroy(&s2mps26->lock);
err_pdata:
	return ret;
}

static int s2mps26_pmic_remove(struct platform_device *pdev)
{
	struct s2mps26_info *s2mps26 = platform_get_drvdata(pdev);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mps26_pmic = s2mps26->dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++)
		device_remove_file(s2mps26_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mps26_pmic->devt);
#endif
	if (s2mps26->iodev->adc_mode > 0)
		s2mps26_powermeter_deinit(s2mps26->iodev);
	mutex_destroy(&s2mps26->lock);

#if IS_ENABLED(CONFIG_SEC_PM)
	if (!IS_ERR_OR_NULL(s2mps26->iodev->ap_sub_pmic_dev))
		sec_device_destroy(s2mps26->iodev->ap_sub_pmic_dev->devt);
#endif /* CONFIG_SEC_PM */
	return 0;
}

static void s2mps26_pmic_shutdown(struct platform_device *pdev)
{
	struct s2mps26_info *s2mps26 = platform_get_drvdata(pdev);

	/* Power-meter off */
	if (s2mps26->iodev->adc_mode > 0)
		if (s2mps26_adc_set_enable(s2mps26->iodev->adc_meter, 0) < 0)
			pr_err("%s: s2mps26_adc_set_enable fail\n", __func__);

	pr_info("%s: Power-meter off\n", __func__);

	/* disable WTSR */
	if (s2mps26->wtsr_en)
		s2mps26_wtsr_disable(s2mps26);
}

#if IS_ENABLED(CONFIG_PM)
static int s2mps26_pmic_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps26_info *s2mps26 = platform_get_drvdata(pdev);
	int ret = 0;

	pr_info("%s: adc_mode %s\n", __func__,
		s2mps26->iodev->adc_mode == 1 ? "enable" : "disable");

	/* Power-meter off */
	if (s2mps26->iodev->adc_mode > 0)
		if (s2mps26_adc_set_enable(s2mps26->iodev->adc_meter, 0) < 0)
			pr_err("%s: s2mps26_adc_set_enable fail\n", __func__);

	/* Off time reduction */
	if (s2mps26->iodev->pmic_rev) {  /* after EVT1 */
		ret = s2mps26_write_reg(s2mps26->iodev->pmic2, 0x5F, 0x03);
		if (ret)
			pr_err("%s: Failed to reduce off time\n", __func__);
	}

	return 0;
}

static int s2mps26_pmic_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mps26_info *s2mps26 = platform_get_drvdata(pdev);
	int ret = 0;

	pr_info("%s: adc_mode %s\n", __func__,
		s2mps26->iodev->adc_mode == 1 ? "enable" : "disable");

	/* Power-meter on */
	if (s2mps26->iodev->adc_mode > 0)
		if (s2mps26_adc_set_enable(s2mps26->iodev->adc_meter, 1) < 0)
			pr_err("%s: s2mps26_adc_set_enable fail\n", __func__);

	/* Restore off time reduction */
	if (s2mps26->iodev->pmic_rev) {  /* after EVT1 */
		ret = s2mps26_write_reg(s2mps26->iodev->pmic2, 0x5F, 0x55);
		if (ret)
			pr_err("%s: Failed to restore off time\n", __func__);
	}

	return 0;
}
#else
#define s2mps26_pmic_suspend	NULL
#define s2mps26_pmic_resume	NULL
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(s2mps26_pmic_pm, s2mps26_pmic_suspend, s2mps26_pmic_resume);

static const struct platform_device_id s2mps26_pmic_id[] = {
	{ "s2mps26-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps26_pmic_id);

static struct platform_driver s2mps26_pmic_driver = {
	.driver = {
		.name = "s2mps26-regulator",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm = &s2mps26_pmic_pm,
#endif
		.suppress_bind_attrs = true,
	},
	.probe = s2mps26_pmic_probe,
	.remove = s2mps26_pmic_remove,
	.shutdown = s2mps26_pmic_shutdown,
	.id_table = s2mps26_pmic_id,
};

module_platform_driver(s2mps26_pmic_driver);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2MPS26 Regulator Driver");
MODULE_LICENSE("GPL");
