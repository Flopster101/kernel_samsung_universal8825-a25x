/*
 * s2mpu14.c
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
#include <linux/mfd/samsung/s2mpu14.h>
#include <linux/mfd/samsung/s2mpu14-regulator.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/regulator/pmic_class.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#define SUB_CHANNEL	1
static struct device_node *acpm_mfd_node;
#endif

#define I2C_BASE_COMMON	0x00
#define I2C_BASE_PM	0x01
#define I2C_BASE_ADC	0x0A
#define I2C_BASE_GPIO	0x0B
#define I2C_BASE_CLOSE	0x0F

static struct s2mpu14_info *s2mpu14_static_info;

struct s2mpu14_info {
	bool g3d_en;
	int wtsr_en;
	int num_regulators;
	unsigned int opmode[S2MPU14_REG_MAX];
	struct regulator_dev *rdev[S2MPU14_REG_MAX];
	struct s2mpu14_dev *iodev;
	struct mutex lock;
	struct i2c_client *i2c;
	struct i2c_client *gpio_i2c;
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	u8 base_addr;
	u8 read_addr;
	u8 read_val;
	struct device *dev;
#endif
};

static unsigned int s2mpu14_of_map_mode(unsigned int val)
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

int s2mpu14_write_gpio(unsigned char reg, unsigned char value)
{
	int ret = 0;

	if (reg < S2MPU14_GPIO_SET1 && reg > S2MPU14_GPIO_SET8) {
		pr_err("%s: fault reg.\n", __func__);
		return -1;
	}

	ret = s2mpu14_write_reg(s2mpu14_static_info->gpio_i2c, reg, value);
	if (ret < 0) {
		pr_err("%s: s2mpu14_write_reg fail\n", __func__);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu14_write_gpio);

int s2mpu14_read_gpio(unsigned char reg, unsigned char *dest)
{
	int ret = 0;

	if (reg < S2MPU14_GPIO_SET1 && reg > S2MPU14_GPIO_SET8) {
		pr_err("%s: fault reg.\n", __func__);
		return -1;
	}

	ret = s2mpu14_read_reg(s2mpu14_static_info->gpio_i2c, reg, dest);
	if (ret < 0) {
		pr_err("%s: s2mpu14_read_reg fail\n", __func__);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu14_read_gpio);

/* Some LDOs supports [LPM/Normal]ON mode during suspend state */
static int s2m_set_mode(struct regulator_dev *rdev,
			unsigned int mode)
{
	struct s2mpu14_info *s2mpu14 = rdev_get_drvdata(rdev);
	unsigned int val;
	int id = rdev_get_id(rdev);

	val = mode << S2MPU14_ENABLE_SHIFT;

	s2mpu14->opmode[id] = val;
	return 0;
}

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mpu14_info *s2mpu14 = rdev_get_drvdata(rdev);

	return s2mpu14_update_reg(s2mpu14->i2c, rdev->desc->enable_reg,
				  s2mpu14->opmode[rdev_get_id(rdev)],
				  rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mpu14_info *s2mpu14 = rdev_get_drvdata(rdev);
	unsigned int val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mpu14_update_reg(s2mpu14->i2c, rdev->desc->enable_reg,
				  val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mpu14_info *s2mpu14 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mpu14_read_reg(s2mpu14->i2c,
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
	struct s2mpu14_info *s2mpu14 = rdev_get_drvdata(rdev);
	int ramp_shift, reg_id = rdev_get_id(rdev);
	unsigned int ramp_value = 0;
	u8 ramp_addr = 0;

	ramp_value = get_ramp_delay(ramp_delay / 1000);
	if (ramp_value > 4) {
		pr_warn("%s: ramp_delay: %d not supported\n",
			rdev->desc->name, ramp_delay);
	}
	ramp_value = 0x00;	// 6.25mv/us fixed

	switch (reg_id) {
	case S2MPU14_BUCK1:
	case S2MPU14_BUCK5:
		ramp_shift = 0;
		break;
	case S2MPU14_BUCK2:
	case S2MPU14_BUCK6:
		ramp_shift = 2;
		break;
	case S2MPU14_BUCK3:
	case S2MPU14_BUCK7:
		ramp_shift = 4;
		break;
	case S2MPU14_BUCK4:
	case S2MPU14_BUCK8:
		ramp_shift = 6;
		break;
	default:
		return -EINVAL;
	}

	switch (reg_id) {
	case S2MPU14_BUCK1:
	case S2MPU14_BUCK2:
	case S2MPU14_BUCK3:
	case S2MPU14_BUCK4:
		ramp_addr = S2MPU14_REG_BUCK_RAMP_UP1S;
		break;
	case S2MPU14_BUCK5:
	case S2MPU14_BUCK6:
	case S2MPU14_BUCK7:
	case S2MPU14_BUCK8:
		ramp_addr = S2MPU14_REG_BUCK_RAMP_UP2S;
		break;
	default:
		return -EINVAL;
	}

	return s2mpu14_update_reg(s2mpu14->i2c, ramp_addr,
				  ramp_value << ramp_shift,
				  BUCK_RAMP_MASK << ramp_shift);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mpu14_info *s2mpu14 = rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = s2mpu14_read_reg(s2mpu14->i2c, rdev->desc->vsel_reg, &val);
	if (ret)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpu14_info *s2mpu14 = rdev_get_drvdata(rdev);
	int ret;

	ret = s2mpu14_update_reg(s2mpu14->i2c, rdev->desc->vsel_reg,
				 sel, rdev->desc->vsel_mask);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return ret;
	}

	if (rdev->desc->apply_bit)
		ret = s2mpu14_update_reg(s2mpu14->i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
}

static int s2m_set_voltage_sel_regmap_buck(struct regulator_dev *rdev,
					   unsigned sel)
{
	struct s2mpu14_info *s2mpu14 = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2mpu14_write_reg(s2mpu14->i2c, rdev->desc->vsel_reg, sel);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return -EINVAL;
	}

	if (rdev->desc->apply_bit)
		ret = s2mpu14_update_reg(s2mpu14->i2c, rdev->desc->apply_reg,
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

static struct regulator_ops s2mpu14_ldo_ops = {
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

static struct regulator_ops s2mpu14_buck_ops = {
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

static struct regulator_ops s2mpu14_bb_ops = {
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

#define _BUCK(macro)		S2MPU14_BUCK##macro
#define _buck_ops(num)		s2mpu14_buck_ops##num
#define _LDO(macro)		S2MPU14_LDO##macro
#define _ldo_ops(num)		s2mpu14_ldo_ops##num
#define _BB(macro)		S2MPU14_BB##macro
#define _bb_ops(num)		s2mpu14_bb_ops##num

#define _REG(ctrl)		S2MPU14_REG##ctrl
#define _TIME(macro)		S2MPU14_ENABLE_TIME##macro

#define _LDO_MIN(group)		S2MPU14_LDO_MIN##group
#define _LDO_STEP(group)	S2MPU14_LDO_STEP##group
#define _BUCK_MIN(group)	S2MPU14_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPU14_BUCK_STEP##group
#define _BB_MIN(group)		S2MPU14_BB_MIN##group
#define _BB_STEP(group)		S2MPU14_BB_STEP##group

#define BUCK_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPU14_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU14_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU14_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu14_of_map_mode			\
}

#define LDO_DESC(_name, _id, _ops, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _LDO_MIN(g),				\
	.uV_step	= _LDO_STEP(g),				\
	.n_voltages	= S2MPU14_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU14_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU14_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu14_of_map_mode			\
}

#define BB_DESC(_name, _id, _ops, g, v, e, t)	{	\
	.name		= _name,				\
	.id			= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BB_MIN(g),				\
	.uV_step	= _BB_STEP(g),				\
	.n_voltages	= S2MPU14_BB_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU14_BB_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU14_ENABLE_MASK,			\
	.enable_time	= t,					\
	.of_map_mode	= s2mpu14_of_map_mode			\
}

static struct regulator_desc regulators[S2MPU14_REG_MAX] = {
	/* name, id, ops, group, vsel_reg, enable_reg, ramp_delay */
	/* LDO 1~28 */
	LDO_DESC("LDO1", _LDO(1), &_ldo_ops(), 1, _REG(_LDO1S_CTRL), _REG(_LDO1S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), &_ldo_ops(), 1, _REG(_LDO2S_CTRL), _REG(_LDO2S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), &_ldo_ops(), 1, _REG(_LDO3S_CTRL), _REG(_LDO3S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), &_ldo_ops(), 1, _REG(_LDO4S_CTRL), _REG(_LDO4S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), &_ldo_ops(), 1, _REG(_LDO5S_CTRL), _REG(_LDO5S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), &_ldo_ops(), 2, _REG(_LDO6S_CTRL), _REG(_LDO6S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO7", _LDO(7), &_ldo_ops(), 2, _REG(_LDO7S_CTRL), _REG(_LDO7S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), &_ldo_ops(), 3, _REG(_LDO8S_CTRL), _REG(_LDO8S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO9", _LDO(9), &_ldo_ops(), 2, _REG(_LDO9S_CTRL), _REG(_LDO9S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO10", _LDO(10), &_ldo_ops(), 3, _REG(_LDO10S_CTRL), _REG(_LDO10S_CTRL), _TIME(_LDO)),

	LDO_DESC("LDO11", _LDO(11), &_ldo_ops(), 3, _REG(_LDO11S_CTRL), _REG(_LDO11S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), &_ldo_ops(), 3, _REG(_LDO12S_CTRL), _REG(_LDO12S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO13", _LDO(13), &_ldo_ops(), 2, _REG(_LDO13S_CTRL), _REG(_LDO13S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO14", _LDO(14), &_ldo_ops(), 2, _REG(_LDO14S_CTRL), _REG(_LDO14S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO15", _LDO(15), &_ldo_ops(), 2, _REG(_LDO15S_CTRL), _REG(_LDO15S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), &_ldo_ops(), 3, _REG(_LDO16S_CTRL), _REG(_LDO16S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), &_ldo_ops(), 2, _REG(_LDO17S_CTRL), _REG(_LDO17S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), &_ldo_ops(), 2, _REG(_LDO18S_CTRL), _REG(_LDO18S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO19", _LDO(19), &_ldo_ops(), 2, _REG(_LDO19S_CTRL), _REG(_LDO19S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO20", _LDO(20), &_ldo_ops(), 3, _REG(_LDO20S_CTRL), _REG(_LDO20S_CTRL), _TIME(_LDO)),

	LDO_DESC("LDO21", _LDO(21), &_ldo_ops(), 3, _REG(_LDO21S_CTRL), _REG(_LDO21S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), &_ldo_ops(), 2, _REG(_LDO22S_CTRL), _REG(_LDO22S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), &_ldo_ops(), 2, _REG(_LDO23S_CTRL), _REG(_LDO23S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO24", _LDO(24), &_ldo_ops(), 3, _REG(_LDO24S_CTRL), _REG(_LDO24S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO25", _LDO(25), &_ldo_ops(), 3, _REG(_LDO25S_CTRL), _REG(_LDO25S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO26", _LDO(26), &_ldo_ops(), 3, _REG(_LDO26S_CTRL), _REG(_LDO26S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO27", _LDO(27), &_ldo_ops(), 3, _REG(_LDO27S_CTRL), _REG(_LDO27S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO28", _LDO(28), &_ldo_ops(), 2, _REG(_LDO28S_CTRL), _REG(_LDO28S_CTRL), _TIME(_LDO)),

	/* BUCK 1~8, BB */
	BUCK_DESC("BUCK1", _BUCK(1), &_buck_ops(), 1, _REG(_BUCK1S_OUT2), _REG(_BUCK1S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), &_buck_ops(), 1, _REG(_BUCK2S_OUT2), _REG(_BUCK2S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), &_buck_ops(), 1, _REG(_BUCK3S_OUT2), _REG(_BUCK3S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), &_buck_ops(), 1, _REG(_BUCK4S_OUT2), _REG(_BUCK4S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), &_buck_ops(), 1, _REG(_BUCK5S_OUT2), _REG(_BUCK5S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK6", _BUCK(6), &_buck_ops(), 1, _REG(_BUCK6S_OUT1), _REG(_BUCK6S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK7", _BUCK(7), &_buck_ops(), 2, _REG(_BUCK7S_OUT1), _REG(_BUCK7S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK8", _BUCK(8), &_buck_ops(), 2, _REG(_BUCK8S_OUT1), _REG(_BUCK8S_CTRL), _TIME(_BUCK)),
	BB_DESC("BUCKB", _BB(), &_bb_ops(), 1, _REG(_BB_OUT1), _REG(_BB_CTRL), _TIME(_BB))
};

#if IS_ENABLED(CONFIG_OF)
static int s2mpu14_pmic_dt_parse_pdata(struct s2mpu14_dev *iodev,
				       struct s2mpu14_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mpu14_regulator_data *rdata;
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
	/* wtsr_en */
	pdata->wtsr_en = 0;
	ret = of_property_read_u32(pmic_np, "wtsr_en", &val);
	if (ret < 0)
		pr_info("%s: fail to read wtsr_en\n", __func__);
	pdata->wtsr_en = val;

	/* Set sel_vgpio (control_sel) */
	p = of_get_property(pmic_np, "sel_vgpio", &len);
	if (!p) {
		pr_info("%s : (ERROR) sel_vgpio isn't parsing\n", __func__);
		return -EINVAL;
	}

	len = len / sizeof(u32);
	if (len != S2MPU14_SEL_VGPIO_NUM) {
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
static int s2mpu14_pmic_dt_parse_pdata(struct s2mpu14_pmic_dev *iodev,
				       struct s2mpu14_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_EXYNOS_AFM)
int sub_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	if (!i2c)
		return -ENODEV;

	return s2mpu14_update_reg(i2c, reg, val, mask);
}
EXPORT_SYMBOL_GPL(sub_pmic_update_reg);

int sub_pmic_get_i2c(struct i2c_client **i2c)
{
	if (!s2mpu14_static_info)
		return -ENODEV;

	*i2c = s2mpu14_static_info->i2c;

	return 0;
}
EXPORT_SYMBOL_GPL(sub_pmic_get_i2c);
#endif

struct s2mpu14_oi_data {
	u8 reg;
	u8 val;
};

#define DECLARE_OI(_reg, _val) { .reg = (_reg), .val = (_val) }
static const struct s2mpu14_oi_data s2mpu14_oi[] = {
	/* BUCK 1~8 & BUCK-BOOST & LDO(6, 10, 11, 23) OI function enable */
	DECLARE_OI(S2MPU14_REG_BUCK_OI_EN1S, 0xFF),
	DECLARE_OI(S2MPU14_REG_BUCK_OI_EN2S, 0x01),
	DECLARE_OI(S2MPU14_REG_LDO_OI_EN_S, 0x0F),
	/* BUCK 1~8 & BUCK-BOOST & LDO(6, 10, 11, 23) OI function power down disable */
	DECLARE_OI(S2MPU14_REG_BUCK_OI_PD_EN1S, 0x00),
	DECLARE_OI(S2MPU14_REG_BUCK_OI_PD_EN2S, 0x00),
	DECLARE_OI(S2MPU14_REG_LDO_OI_PD_EN_S, 0x00),
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPU14_REG_BUCK_OI_CTRL1S, 0xCC),
	DECLARE_OI(S2MPU14_REG_BUCK_OI_CTRL2S, 0xCC),
	DECLARE_OI(S2MPU14_REG_BUCK_OI_CTRL3S, 0xCC),
	DECLARE_OI(S2MPU14_REG_BUCK_OI_CTRL4S, 0xCC),
	DECLARE_OI(S2MPU14_REG_BUCK_OI_CTRL5S, 0x0C),
	DECLARE_OI(S2MPU14_REG_LDO_OI_CTRL_S, 0xFF),
};

static int s2mpu14_oi_function(struct s2mpu14_dev *iodev)
{
	struct i2c_client *i2c = iodev->pmic;
	u32 i;
	u8 val;
	int ret, cnt = 0;
	char buf[1024] = {0, };

	for (i = 0; i < ARRAY_SIZE(s2mpu14_oi); i++) {
		ret = s2mpu14_write_reg(i2c, s2mpu14_oi[i].reg, s2mpu14_oi[i].val);
		if (ret) {
			pr_err("%s: failed to write register\n", __func__);
			goto err;
		}
	}

	for (i = 0; i < ARRAY_SIZE(s2mpu14_oi); i++) {
		ret = s2mpu14_read_reg(i2c, s2mpu14_oi[i].reg, &val);
		if (ret)
			goto err;

		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "0x%x[0x%02hhx], ", s2mpu14_oi[i].reg, val);
	}
	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -1;
}

static void s2mpu14_wtsr_enable(struct s2mpu14_info *s2mpu14,
				struct s2mpu14_platform_data *pdata)
{
	int ret;

	pr_info("[PMIC] %s: WTSR (%s)\n", __func__,
		pdata->wtsr_en ? "enable" : "disable");

	ret = s2mpu14_update_reg(s2mpu14->i2c, S2MPU14_REG_CFG1,
				S2MPU14_WTSR_EN_MASK, S2MPU14_WTSR_EN_MASK);

	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);

	s2mpu14->wtsr_en = pdata->wtsr_en;
}

static void s2mpu14_wtsr_disable(struct s2mpu14_info *s2mpu14)
{
	int ret;

	pr_info("[PMIC] %s: disable WTSR\n", __func__);

	ret = s2mpu14_update_reg(s2mpu14->i2c, S2MPU14_REG_CFG1,
				0x00, S2MPU14_WTSR_EN_MASK);

	if (ret < 0)
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t s2mpu14_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mpu14_info *s2mpu14 = dev_get_drvdata(dev);
	int ret;
	u8 base_addr = 0, reg_addr = 0, val = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu14_dev *iodev = s2mpu14->iodev;
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
	s2mpu14->base_addr = base_addr;
	s2mpu14->read_addr = reg_addr;
	s2mpu14->read_val = val;

	return size;
}

static ssize_t s2mpu14_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mpu14_info *s2mpu14 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			s2mpu14->base_addr, s2mpu14->read_addr, s2mpu14->read_val);
}

static ssize_t s2mpu14_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	int ret;
	u8 base_addr = 0, reg_addr = 0, data = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu14_info *s2mpu14 = dev_get_drvdata(dev);
	struct s2mpu14_dev *iodev = s2mpu14->iodev;
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
	ret = exynos_acpm_write_reg(acpm_mfd_node, SUB_CHANNEL, base_addr, reg_addr, data);
	mutex_unlock(&iodev->i2c_lock);
	if (ret)
		pr_info("%s: fail to write i2c addr/data\n", __func__);
#endif

	return size;
}

static ssize_t s2mpu14_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mpu14_write\n");
}

static struct pmic_device_attribute regulator_attr[] = {
	PMIC_ATTR(s2mpu14_write, 0644, s2mpu14_write_show, s2mpu14_write_store),
	PMIC_ATTR(s2mpu14_read, 0644, s2mpu14_read_show, s2mpu14_read_store),
};

static int s2mpu14_create_sysfs(struct s2mpu14_info *s2mpu14)
{
	struct device *s2mpu14_pmic = s2mpu14->dev;
	struct device *dev = s2mpu14->iodev->dev;
	char device_name[32] = {0,};
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);
	s2mpu14->base_addr = 0;
	s2mpu14->read_addr = 0;
	s2mpu14->read_val = 0;

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mpu14_pmic = pmic_device_create(s2mpu14, device_name);
	s2mpu14->dev = s2mpu14_pmic;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++) {
		err = device_create_file(s2mpu14_pmic, &regulator_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mpu14_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mpu14_pmic->devt);

	return -1;
}
#endif

static int s2mpu14_set_sel_vgpio(struct s2mpu14_info *s2mpu14,
				struct s2mpu14_platform_data *pdata)
{
	int ret, i, cnt = 0;
	u8 reg, val;
	char buf[1024] = {0, };

	for (i = 0; i < S2MPU14_SEL_VGPIO_NUM; i++) {
		reg = S2MPU14_REG_SEL_VGPIO0S + i;
		val = pdata->sel_vgpio[i];

		if (val <= S2MPU14_SEL_VGPIO_MAX_VAL) {
			ret = s2mpu14_write_reg(s2mpu14->i2c, reg, val);
			if (ret) {
				pr_err("%s: sel_vgpio%d write error\n", __func__, i + 1);
				goto err;
			}

			cnt += snprintf(buf + cnt, sizeof(buf) - 1,
					"0x%02hhx[0x%02hhx], ", reg, val);
		} else {
			pr_err("%s : sel_vgpio%d exceed the value\n", __func__, i + 1);
			goto err;
		}
	}

	pr_info("%s: %s\n", __func__, buf);

	return 0;
err:
	return -1;
}

static void s2mpu14_set_regulator_vol(struct s2mpu14_info *s2mpu14)
{
//	s2mpu14_write_reg(s2mpu14->i2c, S2MPU14_REG_BUCK1S_OUT1, 0x20);
//	s2mpu14_write_reg(s2mpu14->i2c, S2MPU14_REG_BUCK2S_OUT1, 0x20);
//	s2mpu14_write_reg(s2mpu14->i2c, S2MPU14_REG_BUCK3S_OUT1, 0x20);
//	s2mpu14_write_reg(s2mpu14->i2c, S2MPU14_REG_BUCK4S_OUT1, 0x20);
//	s2mpu14_write_reg(s2mpu14->i2c, S2MPU14_REG_BUCK5S_OUT1, 0x20);

	/* Set dropout voltage for B7S and B8S */
//	s2mpu14_write_reg(s2mpu14->i2c, S2MPU14_REG_SEL_DVS_EN0S, 0x00);
//	s2mpu14_write_reg(s2mpu14->i2c, S2MPU14_REG_BUCK7S_OUT2, 0x79);
//	s2mpu14_write_reg(s2mpu14->i2c, S2MPU14_REG_BUCK8S_OUT2, 0x9E);
}

static int s2mpu14_pmic_probe(struct platform_device *pdev)
{
	struct s2mpu14_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mpu14_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mpu14_info *s2mpu14;
	int ret;
	u32 i;

	pr_info("[PMIC] %s: start\n", __func__);

	if (iodev->dev->of_node) {
		ret = s2mpu14_pmic_dt_parse_pdata(iodev, pdata);
		if (ret)
			goto err_pdata;
	}

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpu14 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpu14_info),
			       GFP_KERNEL);
	if (!s2mpu14)
		return -ENOMEM;

	s2mpu14->iodev = iodev;
	s2mpu14->i2c = iodev->pmic;
	s2mpu14->gpio_i2c = iodev->gpio_i2c;

	mutex_init(&s2mpu14->lock);

	s2mpu14->g3d_en = pdata->g3d_en;
	s2mpu14_static_info = s2mpu14;

	platform_set_drvdata(pdev, s2mpu14);

	for (i = 0; i < pdata->num_rdata; i++) {
		int id = pdata->regulators[i].id;

		config.dev = &pdev->dev;
		config.init_data = pdata->regulators[i].initdata;
		config.driver_data = s2mpu14;
		config.of_node = pdata->regulators[i].reg_node;
		s2mpu14->opmode[id] = regulators[id].enable_mask;
		s2mpu14->rdev[i] = devm_regulator_register(&pdev->dev,
				&regulators[id], &config);
		if (IS_ERR(s2mpu14->rdev[i])) {
			ret = PTR_ERR(s2mpu14->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n", i);
			s2mpu14->rdev[i] = NULL;
			goto err_s2mpu14_data;
		}
	}

	s2mpu14->num_regulators = pdata->num_rdata;

	if (pdata->wtsr_en)
		s2mpu14_wtsr_enable(s2mpu14, pdata);

	ret = s2mpu14_set_sel_vgpio(s2mpu14, pdata);
	if (ret < 0) {
		pr_err("%s: s2mpu14_set_sel_vgpio fail\n", __func__);
		goto err_s2mpu14_data;
	}

	ret = s2mpu14_oi_function(iodev);
	if (ret < 0)
		pr_err("%s: s2mpu14_oi_function fail\n", __func__);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2mpu14_create_sysfs(s2mpu14);
	if (ret < 0) {
		pr_err("%s: s2mpu14_create_sysfs fail\n", __func__);
		goto err_s2mpu14_data;
	}
#endif
	s2mpu14_set_regulator_vol(s2mpu14);

	pr_info("[PMIC] %s: end\n", __func__);

	return 0;

err_s2mpu14_data:
	mutex_destroy(&s2mpu14->lock);
err_pdata:
	return ret;
}

static int s2mpu14_pmic_remove(struct platform_device *pdev)
{
	struct s2mpu14_info *s2mpu14 = platform_get_drvdata(pdev);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mpu14_pmic = s2mpu14->dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++)
		device_remove_file(s2mpu14_pmic, &regulator_attr[i].dev_attr);
	pmic_device_destroy(s2mpu14_pmic->devt);
#endif
	mutex_destroy(&s2mpu14->lock);
	return 0;
}

static void s2mpu14_pmic_shutdown(struct platform_device *pdev)
{
	struct s2mpu14_info *s2mpu14 = platform_get_drvdata(pdev);

	/* disable WTSR */
	if (s2mpu14->wtsr_en)
		s2mpu14_wtsr_disable(s2mpu14);
}

#if IS_ENABLED(CONFIG_PM)
static int s2mpu14_pmic_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mpu14_info *s2mpu14 = platform_get_drvdata(pdev);
	int ret = 0;

	pr_info("%s\n", __func__);

	/* Off time reduction */
	ret = s2mpu14_write_reg(s2mpu14->i2c, 0xB0, 0x00);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xB0)\n", __func__);

	ret = s2mpu14_write_reg(s2mpu14->i2c, 0xB1, 0x00);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xB1)\n", __func__);

	ret = s2mpu14_write_reg(s2mpu14->i2c, 0xB2, 0x00);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xB2)\n", __func__);

	ret = s2mpu14_write_reg(s2mpu14->i2c, 0xB3, 0x00);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xB3)\n", __func__);

	return 0;
}

static int s2mpu14_pmic_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2mpu14_info *s2mpu14 = platform_get_drvdata(pdev);
	int ret = 0;

	pr_info("%s\n", __func__);

	/* Restore off time reduction */
	ret = s2mpu14_write_reg(s2mpu14->i2c, 0xB0, 0x11);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xB0)\n", __func__);

	ret = s2mpu14_write_reg(s2mpu14->i2c, 0xB1, 0x13);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xB1)\n", __func__);

	ret = s2mpu14_write_reg(s2mpu14->i2c, 0xB2, 0x11);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xB2)\n", __func__);

	ret = s2mpu14_write_reg(s2mpu14->i2c, 0xB3, 0x10);
	if (ret)
		pr_err("%s: Failed to reduce off time(0xB3)\n", __func__);

	return 0;
}
#else
#define s2mpu14_pmic_suspend	NULL
#define s2mpu14_pmic_resume	NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops s2mpu14_pmic_pm = {
	.suspend = s2mpu14_pmic_suspend,
	.resume = s2mpu14_pmic_resume,
};

static const struct platform_device_id s2mpu14_pmic_id[] = {
	{ "s2mpu14-regulator", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpu14_pmic_id);

static struct platform_driver s2mpu14_pmic_driver = {
	.driver = {
		.name = "s2mpu14-regulator",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm = &s2mpu14_pmic_pm,
#endif
		.suppress_bind_attrs = true,
	},
	.probe = s2mpu14_pmic_probe,
	.remove = s2mpu14_pmic_remove,
	.shutdown = s2mpu14_pmic_shutdown,
	.id_table = s2mpu14_pmic_id,
};

static int __init s2mpu14_pmic_init(void)
{
	return platform_driver_register(&s2mpu14_pmic_driver);
}
subsys_initcall(s2mpu14_pmic_init);

static void __exit s2mpu14_pmic_exit(void)
{
	platform_driver_unregister(&s2mpu14_pmic_driver);
}
module_exit(s2mpu14_pmic_exit);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2MPU14 Regulator Driver");
MODULE_LICENSE("GPL");
