// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/gpio/driver.h>
#include <linux/of_gpio.h>

#include "core.h"
#include "pinctrl-utils.h"

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#endif
#include <linux/mfd/samsung/s2mpu14.h>
#include <linux/mfd/samsung/s2mpu14-regulator.h>

/* Samsung specific pin configurations */
#define REG_GPIO_STATUS				(0x04)
#define REG_GPIO_SET5				(0x09)
#define GPIO_RANGE				(4)
#define GPIO_DEFAULT_NUM			(4)

#define PMIC_GPIO_PULL_DISABLE			(0 << PMIC_GPIO_PULL_SHIFT)
#define PMIC_GPIO_PULL_DOWN			(1 << PMIC_GPIO_PULL_SHIFT)
#define PMIC_GPIO_PULL_UP			(2 << PMIC_GPIO_PULL_SHIFT)
#define PMIC_GPIO_OEN_INPUT			(0 << PMIC_GPIO_OEN_SHIFT)
#define PMIC_GPIO_OEN_OUTPUT			(1 << PMIC_GPIO_OEN_SHIFT)

#define PMIC_GPIO_PULL_SHIFT			(3)
#define PMIC_GPIO_PULL_MASK			(0x03 << PMIC_GPIO_PULL_SHIFT)
#define PMIC_GPIO_OEN_SHIFT			(6)
#define PMIC_GPIO_OEN_MASK			(0x01 << PMIC_GPIO_OEN_SHIFT)
#define PMIC_GPIO_DRV_STR_SHIFT			(2)
#define PMIC_GPIO_DRV_STR_MASK			(0x01 << PMIC_GPIO_DRV_STR_SHIFT)
#define PMIC_GPIO_OUT_SHIFT			(5)
#define PMIC_GPIO_OUT_MASK			(0x01 << PMIC_GPIO_OUT_SHIFT)

#define PMIC_GPIO_CONF_DISABLE			(PIN_CONFIG_END + 1)
#define PMIC_GPIO_CONF_PULL_DOWN		(PIN_CONFIG_END + 2)
#define PMIC_GPIO_CONF_PULL_UP			(PIN_CONFIG_END + 3)
#define PMIC_GPIO_CONF_INPUT_ENABLE		(PIN_CONFIG_END + 4)
#define PMIC_GPIO_CONF_OUTPUT_ENABLE		(PIN_CONFIG_END + 5)
#define PMIC_GPIO_CONF_OUTPUT			(PIN_CONFIG_END + 6)
#define PMIC_GPIO_CONF_DRIVE_STRENGTH		(PIN_CONFIG_END + 7)

static const char *const biases[] = {
	"no-pull", "pull-down", "pull-up"
};

static const char *const strengths[] = {
	"2mA", "4mA"
};

static const struct pinconf_generic_params pmic_gpio_bindings[] = {
	{"pmic-gpio,pull-disable",	PMIC_GPIO_CONF_DISABLE,		0},
	{"pmic-gpio,pull-down",		PMIC_GPIO_CONF_PULL_DOWN,	1},
	{"pmic-gpio,pull-up",		PMIC_GPIO_CONF_PULL_UP,		2},
	{"pmic-gpio,input-enable",	PMIC_GPIO_CONF_INPUT_ENABLE,	0},
	{"pmic-gpio,output-enable",	PMIC_GPIO_CONF_OUTPUT_ENABLE,	1},
	{"pmic-gpio,output-low",	PMIC_GPIO_CONF_OUTPUT,		0},
	{"pmic-gpio,output-high",	PMIC_GPIO_CONF_OUTPUT,		1},
	{"pmic-gpio,drive-strength",	PMIC_GPIO_CONF_DRIVE_STRENGTH,	0},
};

#ifdef CONFIG_DEBUG_FS
static const struct pin_config_item pmic_conf_items[ARRAY_SIZE(pmic_gpio_bindings)] = {
	PCONFDUMP(PMIC_GPIO_CONF_DISABLE,	"pull-disable", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_PULL_DOWN,	"pull-down", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_PULL_UP,	"pull-up", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_INPUT_ENABLE,	"input-enable", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_OUTPUT_ENABLE,	"output-enable", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_OUTPUT,	"output-high", NULL, true),
	PCONFDUMP(PMIC_GPIO_CONF_DRIVE_STRENGTH,"drive-strength", NULL, true),
};
#endif

struct pmic_gpio_state {
	struct device	*dev;
	struct pinctrl_dev *ctrl;
	struct gpio_chip chip;
	struct irq_chip irq;
	struct i2c_client *i2c;
	int npins;
};

/**
 * struct pmic_gpio_pad - keep current GPIO settings
 * @base: Address base in SPMI device.
 * @is_enabled: Set to false when GPIO should be put in high Z state.
 * @out_value: Cached pin output value
 * @have_buffer: Set to true if GPIO output could be configured in push-pull,
 *	open-drain or open-source mode.
 * @output_enabled: Set to true if GPIO output logic is enabled.
 * @input_enabled: Set to true if GPIO input buffer logic is enabled.
 * @analog_pass: Set to true if GPIO is in analog-pass-through mode.
 * @lv_mv_type: Set to true if GPIO subtype is GPIO_LV(0x10) or GPIO_MV(0x11).
 * @num_sources: Number of power-sources supported by this GPIO.
 * @power_source: Current power-source used.
 * @buffer_type: Push-pull, open-drain or open-source.
 * @pullup: Constant current which flow trough GPIO output buffer.
 * @strength: No, Low, Medium, High
 * @function: See pmic_gpio_functions[]
 * @atest: the ATEST selection for GPIO analog-pass-through mode
 * @dtest_buffer: the DTEST buffer selection for digital input mode.
 */
struct pmic_gpio_pad {
	u16		base;
	bool		is_enabled;
	bool		out_value;
	bool		have_buffer;
	bool		output_enabled;
	bool		input_enabled;
	bool		analog_pass;
	bool		lv_mv_type;
	unsigned int	num_sources;
	unsigned int	power_source;
	unsigned int	buffer_type;
	unsigned int	pullup;
	unsigned int	strength;
	unsigned int	function;
	unsigned int	atest;
	unsigned int	dtest_buffer;
};

static const char *const pmic_gpio_groups[] = {
	"gpio_s5", "gpio_s6", "gpio_s7", "gpio_s8",
};

/* The index of each function in pmic_gpio_functions[] array */
#define PMIC_GPIO_FUNC_NORMAL		"normal"

enum pmic_gpio_func_index {
	PMIC_GPIO_FUNC_INDEX_NORMAL,
};

/* To be used with "function" */
static const char *const pmic_gpio_functions[] = {
	[PMIC_GPIO_FUNC_INDEX_NORMAL]	= PMIC_GPIO_FUNC_NORMAL,
};

static int pmic_gpio_read(struct pmic_gpio_state *state, uint8_t addr)
{
	uint8_t val;
	int ret;

	ret = s2mpu14_read_reg(state->i2c, addr, &val);
	if (ret < 0)
		dev_err(state->dev, "%s: read 0x%x failed\n", __func__, addr);
	else
		ret = val;

	return ret;
}

static int pmic_gpio_write(struct pmic_gpio_state *state, uint8_t addr, uint8_t val)
{
	int ret;

	ret = s2mpu14_write_reg(state->i2c, addr, val);
	if (ret < 0)
		dev_err(state->dev, "%s: write 0x%x failed\n", __func__, addr);

	return ret;
}

static int pmic_gpio_get_level(struct pmic_gpio_pad *pad, unsigned int pin, int val)
{
	int ret = 0;

	/* GPIO high/low */
	ret = (val >> ((pin % GPIO_RANGE) + 4)) & 0x1;
	pad->out_value = ret;

	return ret;
}

static int pmic_gpio_get_pull(struct pmic_gpio_pad *pad, int val)
{
	int ret = 0;

	/* Pull Disable/Up/Down */
	ret = (val & PMIC_GPIO_PULL_MASK);

	switch (ret) {
	case PMIC_GPIO_PULL_DISABLE:
	case PMIC_GPIO_PULL_DOWN:
	case PMIC_GPIO_PULL_UP:
		pad->pullup = (ret >> PMIC_GPIO_PULL_SHIFT);
		break;
	default:
		pr_err("%s: unknown PULL status(%d)\n", __func__, ret);
		return -EINVAL;
	}

	return (ret >> PMIC_GPIO_PULL_SHIFT);
}

static int pmic_gpio_get_mode(struct pmic_gpio_pad *pad, int val)
{
	int ret = 0;

	/* Mode input/output */
	ret = (val & PMIC_GPIO_OEN_MASK);

	switch (ret) {
	case PMIC_GPIO_OEN_INPUT:
		pad->input_enabled = true;
		pad->output_enabled = false;
		break;
	case PMIC_GPIO_OEN_OUTPUT:
		pad->input_enabled = false;
		pad->output_enabled = true;
		break;
	default:
		pr_err("%s: unknown GPIO direction(%d)\n", __func__, ret);
		return -ENODEV;
	}

	return (ret >> PMIC_GPIO_OEN_SHIFT);
}

static int pmic_gpio_get_str(struct pmic_gpio_pad *pad, int val)
{
	int ret = 0;

	/* DRV str */
	ret = (val & PMIC_GPIO_DRV_STR_MASK);
	pad->strength = (ret >> PMIC_GPIO_DRV_STR_SHIFT);

	return (ret >> PMIC_GPIO_DRV_STR_SHIFT);
}

static int pmic_gpio_get_groups_count(struct pinctrl_dev *pctldev)
{
	/* Every PIN is a group */
	return pctldev->desc->npins;
}

static const char *pmic_gpio_get_group_name(struct pinctrl_dev *pctldev,
					    unsigned pin)
{
	return pctldev->desc->pins[pin].name;
}

static int pmic_gpio_get_group_pins(struct pinctrl_dev *pctldev, unsigned pin,
				    const unsigned **pins, unsigned *num_pins)
{
	*pins = &pctldev->desc->pins[pin].number;
	*num_pins = 1;
	return 0;
}

static void pmic_gpio_pin_dbg_show(struct pinctrl_dev *pctldev,
				struct seq_file *s, unsigned pin)
{
	struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct pmic_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;
	uint8_t reg_set = REG_GPIO_SET5 + pin;
	uint8_t reg_status = REG_GPIO_STATUS;
	int val_set = 0, val_status = 0;

	val_set = pmic_gpio_read(state, reg_set);
	if (val_set < 0) {
		seq_printf(s, "pmic_gpio_read fail(%d)", __LINE__);
		return;
	}

	val_status = pmic_gpio_read(state, reg_status);
	if (val_status < 0) {
		seq_printf(s, "pmic_gpio_read fail(%d)", __LINE__);
		return;
	}

	/* Get info. */
	pmic_gpio_get_pull(pad, val_set);
	pmic_gpio_get_mode(pad, val_set);
	pmic_gpio_get_str(pad, val_set);
	pmic_gpio_get_level(pad, pin, val_status);

	/* Print info. */
	seq_printf(s, "%s(%#x) MODE(%s) DRV_STR(%s_%#x) DAT(%s_%#x)",
			biases[pad->pullup], pad->pullup,
			pad->input_enabled ? "input" : "output",
			strengths[pad->strength], pad->strength,
			pad->out_value ? "high" : "low", pad->out_value);
}

static const struct pinctrl_ops pmic_gpio_pinctrl_ops = {
	.get_groups_count	= pmic_gpio_get_groups_count,
	.get_group_name		= pmic_gpio_get_group_name,
	.get_group_pins		= pmic_gpio_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_pin,
	.dt_free_map		= pinctrl_utils_free_map,
	.pin_dbg_show		= pmic_gpio_pin_dbg_show,
};

static int pmic_gpio_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(pmic_gpio_functions);
}

static const char *pmic_gpio_get_function_name(struct pinctrl_dev *pctldev,
					       unsigned function)
{
	return pmic_gpio_functions[function];
}

static int pmic_gpio_get_function_groups(struct pinctrl_dev *pctldev,
					 unsigned function,
					 const char *const **groups,
					 unsigned *const num_qgroups)
{
	*groups = pmic_gpio_groups;
	*num_qgroups = pctldev->desc->npins;
	return 0;
}

static int pmic_gpio_set_mux(struct pinctrl_dev *pctldev, unsigned function,
				unsigned pin)
{
	struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct pmic_gpio_pad *pad;
	unsigned int val;
	//int ret;

	pad = pctldev->desc->pins[pin].drv_data;

	pad->function = function;

	val = pad->is_enabled << 1;//PMIC_GPIO_REG_MASTER_EN_SHIFT;

	return pmic_gpio_write(state, 1, val);//PMIC_GPIO_REG_EN_CTL, val);
}

static const struct pinmux_ops pmic_gpio_pinmux_ops = {
	.get_functions_count	= pmic_gpio_get_functions_count,
	.get_function_name	= pmic_gpio_get_function_name,
	.get_function_groups	= pmic_gpio_get_function_groups,
	.set_mux		= pmic_gpio_set_mux,
};

static int pmic_gpio_pin_config_get(struct pinctrl_dev *pctldev,
					unsigned pin, unsigned long *config)
{
	struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct pmic_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;
	unsigned param = pinconf_to_config_param(*config);
	unsigned arg;
	uint8_t reg_set = REG_GPIO_SET5 + pin;
	uint8_t reg_status = REG_GPIO_STATUS;
	int ret = 0, val_set = 0, val_status = 0;

	val_set = pmic_gpio_read(state, reg_set);
	if (val_set < 0)
		return -EINVAL;

	val_status = pmic_gpio_read(state, reg_status);
	if (val_status < 0)
		return -EINVAL;

	switch (param) {
	case PMIC_GPIO_CONF_DISABLE:
	case PMIC_GPIO_CONF_PULL_DOWN:
	case PMIC_GPIO_CONF_PULL_UP:
		ret = pmic_gpio_get_pull(pad, val_set);
		if (ret < 0)
			return ret;
		arg = pad->pullup;

		dev_info(state->dev, "%s: %s: %s(%#x)\n", __func__,
				pmic_gpio_groups[pin], biases[pad->pullup], pad->pullup);
		break;
	case PMIC_GPIO_CONF_INPUT_ENABLE:
		ret = pmic_gpio_get_mode(pad, val_set);
		if (ret < 0)
			return ret;
		arg = !!pad->input_enabled;

		dev_info(state->dev, "%s: %s: MODE(%s)\n", __func__,
				pmic_gpio_groups[pin], pad->input_enabled ? "input" : "output");
		break;
	case PMIC_GPIO_CONF_OUTPUT_ENABLE:
		ret = pmic_gpio_get_mode(pad, val_set);
		if (ret < 0)
			return ret;
		arg = !!pad->output_enabled;

		dev_info(state->dev, "%s: %s: MODE(%s)\n", __func__,
				pmic_gpio_groups[pin], pad->input_enabled ? "input" : "output");
		break;
	case PMIC_GPIO_CONF_OUTPUT:
		pmic_gpio_get_level(pad, pin, val_status);
		arg = !!pad->out_value;

		dev_info(state->dev, "%s: %s: DAT(%s_%#x)\n", __func__,
				pmic_gpio_groups[pin], pad->out_value ? "high" : "low", pad->out_value);
		break;
	case PMIC_GPIO_CONF_DRIVE_STRENGTH:
		pmic_gpio_get_str(pad, val_set);
		arg = pad->strength;

		dev_info(state->dev, "%s: %s: DRV_STR(%s_%#x)\n", __func__,
				pmic_gpio_groups[pin], strengths[pad->strength], pad->strength);
		break;
	default:
		dev_err(state->dev, "%s: pmic_gpio_pin_config_get fail(param: %#x)\n",
				__func__, param);
		return -EINVAL;
	}

	*config = pinconf_to_config_packed(param, arg);

	return 0;
}

static int pmic_gpio_pin_config_group_get(struct pinctrl_dev *pctldev,
					unsigned int pin, unsigned long *config)
{
	struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	int i, ret = 0;

	for (i = 0; i < state->npins; i++) {
		ret = pmic_gpio_pin_config_get(pctldev, i, config);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int pmic_gpio_pin_config_set(struct pinctrl_dev *pctldev, unsigned pin,
					unsigned long *configs, unsigned num_configs)
{
	struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct pmic_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;
	unsigned param, arg;
	uint8_t reg_set = REG_GPIO_SET5 + pin;
	uint8_t reg_status = REG_GPIO_STATUS;
	int ret = 0, i = 0, val_set = 0, val_status = 0;

	pad->is_enabled = true;

	val_set = pmic_gpio_read(state, reg_set);
	if (val_set < 0)
		return -EINVAL;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		switch (param) {
		case PMIC_GPIO_CONF_DISABLE:
			val_set = (val_set & ~PMIC_GPIO_PULL_MASK) | PMIC_GPIO_PULL_DISABLE;
			break;
		case PMIC_GPIO_CONF_PULL_DOWN:
			val_set = (val_set & ~PMIC_GPIO_PULL_MASK) | PMIC_GPIO_PULL_DOWN;
			break;
		case PMIC_GPIO_CONF_PULL_UP:
			val_set = (val_set & ~PMIC_GPIO_PULL_MASK) | PMIC_GPIO_PULL_UP;
			break;
		case PMIC_GPIO_CONF_INPUT_ENABLE:
			val_set = (val_set & ~PMIC_GPIO_OEN_MASK);
			break;
		case PMIC_GPIO_CONF_OUTPUT_ENABLE:
			val_set = (val_set & ~PMIC_GPIO_OEN_MASK) | PMIC_GPIO_OEN_OUTPUT;
			break;
		case PMIC_GPIO_CONF_OUTPUT:
			arg = (arg << PMIC_GPIO_OUT_SHIFT) & PMIC_GPIO_OUT_MASK;
			val_set = (val_set & ~(PMIC_GPIO_OEN_MASK | PMIC_GPIO_OUT_MASK)) |
				(PMIC_GPIO_OEN_OUTPUT | arg);
			break;
		case PMIC_GPIO_CONF_DRIVE_STRENGTH:
			arg = (arg << PMIC_GPIO_DRV_STR_SHIFT) & PMIC_GPIO_DRV_STR_MASK;
			val_set = (val_set & ~PMIC_GPIO_DRV_STR_MASK) | arg;
			break;
		default:
			dev_err(state->dev, "%s: pmic_gpio_pin_config_set fail(param: %#x)\n",
					__func__, param);
			return -ENOTSUPP;
		}
	}

	ret = pmic_gpio_write(state, reg_set, val_set);
	if (ret < 0) {
		dev_err(state->dev, "%s: pmic_gpio_write fail\n", __func__);
		return ret;
	}

	val_set = pmic_gpio_read(state, reg_set);
	if (val_set < 0)
		return -EINVAL;

	val_status = pmic_gpio_read(state, reg_status);
	if (val_status < 0)
		return -EINVAL;

	pmic_gpio_get_pull(pad, val_set);
	pmic_gpio_get_mode(pad, val_set);
	pmic_gpio_get_str(pad, val_set);
	pmic_gpio_get_level(pad, pin, val_status);

	dev_info(state->dev, "%s: %s: reg(%#02hhx%02hhx:%#02hhx), "
			"%s(%#x), MODE(%s), DRV_STR(%s_%#x), DAT(%s_%#x)\n",
			__func__, pmic_gpio_groups[pin], state->i2c->addr, reg_set, val_set,
			biases[pad->pullup], pad->pullup,
			pad->input_enabled ? "input" : "output",
			strengths[pad->strength], pad->strength,
			pad->out_value ? "high" : "low", pad->out_value);

	return ret;
}

static int pmic_gpio_pin_config_group_set(struct pinctrl_dev *pctldev, unsigned selector,
				unsigned long *configs, unsigned num_configs)
{
	struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	int i, ret = 0;

	for (i = 0; i < state->npins; i++) {
		ret = pmic_gpio_pin_config_set(pctldev, i, configs, num_configs);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static void pmic_gpio_pin_config_dbg_show(struct pinctrl_dev *pctldev,
					struct seq_file *s, unsigned pin)
{
	struct pmic_gpio_state *state = pinctrl_dev_get_drvdata(pctldev);
	struct pmic_gpio_pad *pad = pctldev->desc->pins[pin].drv_data;
	uint8_t reg_set = REG_GPIO_SET5 + pin;
	uint8_t reg_status = REG_GPIO_STATUS;
	int val_set = 0, val_status = 0;

	val_set = pmic_gpio_read(state, reg_set);
	if (val_set < 0) {
		seq_printf(s, "pmic_gpio_read fail(%d)", __LINE__);
		return;
	}

	val_status = pmic_gpio_read(state, reg_status);
	if (val_status < 0) {
		seq_printf(s, "pmic_gpio_read fail(%d)", __LINE__);
		return;
	}

	/* Get info. */
	pmic_gpio_get_pull(pad, val_set);
	pmic_gpio_get_mode(pad, val_set);
	pmic_gpio_get_str(pad, val_set);
	pmic_gpio_get_level(pad, pin, val_status);

	/* Print info. */
	seq_printf(s, "%s(%#x) MODE(%s) DRV_STR(%s_%#x) DAT(%s_%#x)",
			biases[pad->pullup], pad->pullup,
			pad->input_enabled ? "input" : "output",
			strengths[pad->strength], pad->strength,
			pad->out_value ? "high" : "low", pad->out_value);
}

static const struct pinconf_ops pmic_gpio_pinconf_ops = {
	.is_generic			= false,
	.pin_config_get			= pmic_gpio_pin_config_get,
	.pin_config_set			= pmic_gpio_pin_config_set,
	.pin_config_group_get		= pmic_gpio_pin_config_group_get,
	.pin_config_group_set		= pmic_gpio_pin_config_group_set,
	.pin_config_dbg_show		= pmic_gpio_pin_config_dbg_show,
};

static int pmic_gpio_direction_input(struct gpio_chip *chip, unsigned pin)
{
	struct pmic_gpio_state *state = gpiochip_get_data(chip);
	unsigned long config;
	int ret;

	config = pinconf_to_config_packed(PMIC_GPIO_CONF_INPUT_ENABLE, 1);
	ret = pmic_gpio_pin_config_set(state->ctrl, pin, &config, 1);

	dev_info(state->dev, "%s: %s: ret(%#x)\n", __func__, pmic_gpio_groups[pin], ret);

	return ret;
}

static int pmic_gpio_direction_output(struct gpio_chip *chip,
				      unsigned pin, int val)
{
	struct pmic_gpio_state *state = gpiochip_get_data(chip);
	unsigned long config;
	int ret;

	config = pinconf_to_config_packed(PMIC_GPIO_CONF_OUTPUT, val);
	ret = pmic_gpio_pin_config_set(state->ctrl, pin, &config, 1);

	dev_info(state->dev, "%s: %s: val(%#x), ret(%#x)\n",
			__func__, pmic_gpio_groups[pin], val, ret);

	return ret;
}

static int pmic_gpio_get(struct gpio_chip *chip, unsigned int pin)
{
	struct pmic_gpio_state *state = gpiochip_get_data(chip);
	struct pmic_gpio_pad *pad = state->ctrl->desc->pins[pin].drv_data;
	uint8_t reg_status = REG_GPIO_STATUS;
	int ret = 0, val_status = 0;

	val_status = pmic_gpio_read(state, reg_status);
	if (val_status < 0)
		return -EINVAL;

	ret = pmic_gpio_get_level(pad, pin, val_status);

	dev_info(state->dev, "%s: %s: DAT(%s_%#x)\n",
			__func__, pmic_gpio_groups[pin], ret ? "high" : "low", ret);

	return ret;
}

static void pmic_gpio_set(struct gpio_chip *chip, unsigned pin, int value)
{
	struct pmic_gpio_state *state = gpiochip_get_data(chip);
	unsigned long config;

	dev_info(state->dev, "%s: %s: Set DAT(%s_%#x)\n",
			__func__, pmic_gpio_groups[pin], value ? "high" : "low", value);

	config = pinconf_to_config_packed(PMIC_GPIO_CONF_OUTPUT, value);
	pmic_gpio_pin_config_set(state->ctrl, pin, &config, 1);
}

static int pmic_gpio_pin_xlate(uint32_t gpio)
{
	/* Translate a DT GPIO specifier into a PMIC GPIO number */
	if (gpio == 5)
		return 0;
	else if (gpio == 6)
		return 1;
	else if (gpio == 7)
		return 2;
	else if (gpio == 8)
		return 3;
	else
		return -EINVAL;
}

static int pmic_gpio_of_xlate(struct gpio_chip *chip,
			      const struct of_phandle_args *gpio_desc,
			      u32 *flags)
{
	struct pmic_gpio_state *state = gpiochip_get_data(chip);
	int ret = 0;

	if (chip->of_gpio_n_cells < 2)
		return -EINVAL;

	if (flags)
		*flags = gpio_desc->args[1];

	ret = pmic_gpio_pin_xlate(gpio_desc->args[0]);
	if (ret < 0) {
		dev_err(state->dev, "%s: Invalid GPIO pin(%d)\n", __func__, gpio_desc->args[0]);
		return ret;
	}

	return ret;
}

static void pmic_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	struct pmic_gpio_state *state = gpiochip_get_data(chip);
	unsigned i;

	for (i = 0; i < chip->ngpio; i++) {
		pmic_gpio_pin_config_dbg_show(state->ctrl, s, i);
		seq_puts(s, "\n");
	}
}

static int pmic_gpio_set_config(struct gpio_chip *gc, unsigned int offset, unsigned long config)
{
	struct pmic_gpio_state *state = gpiochip_get_data(gc);

	dev_err(state->dev, "%s: %s: config(%#x)\n", __func__, offset, config);

	return pmic_gpio_pin_config_set(state->ctrl, offset, &config, 1);
}

static const struct gpio_chip pmic_gpio_gpio_template = {
	.direction_input	= pmic_gpio_direction_input,
	.direction_output	= pmic_gpio_direction_output,
	.get			= pmic_gpio_get,
	.set			= pmic_gpio_set,
	.request		= gpiochip_generic_request,
	.free			= gpiochip_generic_free,
	.of_xlate		= pmic_gpio_of_xlate,
	.dbg_show		= pmic_gpio_dbg_show,
	.set_config		= pmic_gpio_set_config,
};

static int pmic_gpio_populate(struct pmic_gpio_state *state,
			      struct pmic_gpio_pad *pad, unsigned int pin)
{
	uint8_t reg_set = REG_GPIO_SET5 + pin;
	uint8_t reg_status = REG_GPIO_STATUS;
	int ret = 0, val_set = 0, val_status = 0;

	val_set = pmic_gpio_read(state, reg_set);
	if (val_set < 0)
		return -EINVAL;

	val_status = pmic_gpio_read(state, reg_status);
	if (val_status < 0)
		return -EINVAL;

	/* Pull up/dn */
	ret = pmic_gpio_get_pull(pad, val_set);
	if (ret < 0)
		return ret;

	/* Mode input/output */
	ret = pmic_gpio_get_mode(pad, val_set);
	if (ret < 0)
		return ret;

	/* DRV str */
	pmic_gpio_get_str(pad, val_set);

	/* GPIO level */
	pmic_gpio_get_level(pad, pin, val_status);

	dev_info(state->dev, "%s: %s: %s(%#x), MODE(%s), "
			"DRV_STR(%s_%#x), DAT(%s_%#x)\n", __func__,
			pmic_gpio_groups[pin], biases[pad->pullup], pad->pullup,
			pad->input_enabled ? "input" : "output",
			strengths[pad->strength], pad->strength,
			pad->out_value ? "high" : "low", pad->out_value);

	/* Pin could be disabled with PIN_CONFIG_BIAS_HIGH_IMPEDANCE */
	pad->is_enabled = true;

	return 0;
}

/* TODO: irq related function */
//static int pmic_gpio_domain_translate(struct irq_domain *domain,
//				      struct irq_fwspec *fwspec,
//				      unsigned long *hwirq,
//				      unsigned int *type)
//{
//	struct pmic_gpio_state *state = container_of(domain->host_data,
//						     struct pmic_gpio_state,
//						     chip);
//
//	if (fwspec->param_count != 2 ||
//	    fwspec->param[0] < 1 || fwspec->param[0] > state->chip.ngpio)
//		return -EINVAL;
//
//	*hwirq = fwspec->param[0] - PMIC_GPIO_PHYSICAL_OFFSET;
//	*type = fwspec->param[1];
//
//	return 0;
//}
//
//static unsigned int pmic_gpio_child_offset_to_irq(struct gpio_chip *chip,
//						  unsigned int offset)
//{
//	return offset + PMIC_GPIO_PHYSICAL_OFFSET;
//}
//
//static int pmic_gpio_child_to_parent_hwirq(struct gpio_chip *chip,
//					   unsigned int child_hwirq,
//					   unsigned int child_type,
//					   unsigned int *parent_hwirq,
//					   unsigned int *parent_type)
//{
//	*parent_hwirq = child_hwirq + 0xc0;
//	*parent_type = child_type;
//
//	return 0;
//}

static int pmic_gpio_parse_dt(struct s2mpu14_dev *iodev, struct pmic_gpio_state *state)
{
	struct device_node *mfd_np, *gpio_np;
	uint32_t val;
	int ret;

	if (!iodev->dev->of_node) {
		dev_err(state->dev, "%s: error\n", __func__);
		goto err;
	}

	mfd_np = iodev->dev->of_node;
	if (!mfd_np) {
		dev_err(state->dev, "%s: could not find parent_node\n", __func__);
		goto err;
	}

	gpio_np = of_find_node_by_name(mfd_np, "s2mpu14-gpio");
	if (!gpio_np) {
		dev_err(state->dev, "%s: could not find current_node\n", __func__);
		goto err;
	}
	state->dev->of_node = gpio_np;

	ret = of_property_read_u32(gpio_np, "samsung,npins", &val);
	if (ret)
		state->npins = GPIO_DEFAULT_NUM;
	state->npins = val;

	return 0;
err:
	return -1;
}

static int pmic_gpio_probe(struct platform_device *pdev)
{
	//struct irq_domain *parent_domain;
	//struct device_node *parent_node;
	struct device *dev = &pdev->dev;
	struct pinctrl_pin_desc *pindesc;
	struct pinctrl_desc *pctrldesc;
	struct pmic_gpio_pad *pad, *pads;
	struct pmic_gpio_state *state;
	//struct gpio_irq_chip *girq;
	struct s2mpu14_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	int ret, i;

	dev_info(dev, "%s: start\n", __func__);

	state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	platform_set_drvdata(pdev, state);

	state->dev = &pdev->dev;
	state->i2c = iodev->gpio_i2c;

	ret = pmic_gpio_parse_dt(iodev, state);
	if (ret < 0) {
		dev_err(state->dev, "%s: pmic_gpio_parse_dt fail\n", __func__);
		return -ENODEV;
	}

	pindesc = devm_kcalloc(dev, state->npins, sizeof(*pindesc), GFP_KERNEL);
	if (!pindesc)
		return -ENOMEM;

	pads = devm_kcalloc(dev, state->npins, sizeof(*pads), GFP_KERNEL);
	if (!pads)
		return -ENOMEM;

	pctrldesc = devm_kzalloc(dev, sizeof(*pctrldesc), GFP_KERNEL);
	if (!pctrldesc)
		ret = -ENOMEM;

	pctrldesc->pctlops = &pmic_gpio_pinctrl_ops;
	pctrldesc->pmxops = &pmic_gpio_pinmux_ops;
	pctrldesc->confops = &pmic_gpio_pinconf_ops;
	pctrldesc->owner = THIS_MODULE;
	pctrldesc->name = dev_name(dev);
	pctrldesc->pins = pindesc;
	pctrldesc->npins = state->npins;
	pctrldesc->num_custom_params = ARRAY_SIZE(pmic_gpio_bindings);
	pctrldesc->custom_params = pmic_gpio_bindings;
#ifdef CONFIG_DEBUG_FS
	pctrldesc->custom_conf_items = pmic_conf_items;
#endif

	for (i = 0; i < state->npins; i++, pindesc++) {
		pad = &pads[i];
		pindesc->drv_data = pad;
		pindesc->number = i;
		pindesc->name = pmic_gpio_groups[i];

		pad->base = i;

		ret = pmic_gpio_populate(state, pad, i);
		if (ret < 0)
			return ret;
	}

	state->chip = pmic_gpio_gpio_template;
	state->chip.parent = dev;
	state->chip.base = -1;
	state->chip.ngpio = state->npins;
	state->chip.label = dev_name(dev);
	state->chip.of_gpio_n_cells = 2;
	state->chip.can_sleep = false;
	state->chip.of_node = state->dev->of_node;

	state->ctrl = devm_pinctrl_register(dev, pctrldesc, state);
	if (IS_ERR(state->ctrl))
		return PTR_ERR(state->ctrl);

	/* TODO: irq related function */
//	parent_node = of_irq_find_parent(state->dev->of_node);
//	if (!parent_node)
//		return -ENXIO;
//
//	parent_domain = irq_find_host(parent_node);
//	of_node_put(parent_node);
//	if (!parent_domain)
//		return -ENXIO;
//
//	state->irq.name = "spmi-gpio",
//	state->irq.irq_ack = irq_chip_ack_parent,
//	state->irq.irq_mask = irq_chip_mask_parent,
//	state->irq.irq_unmask = irq_chip_unmask_parent,
//	state->irq.irq_set_type = irq_chip_set_type_parent,
//	state->irq.irq_set_wake = irq_chip_set_wake_parent,
//	state->irq.flags = IRQCHIP_MASK_ON_SUSPEND,
//
//	girq = &state->chip.irq;
//	girq->chip = &state->irq;
//	girq->default_type = IRQ_TYPE_NONE;
//	girq->handler = handle_level_irq;
//	girq->fwnode = of_node_to_fwnode(state->dev->of_node);
//	girq->parent_domain = parent_domain;
//	girq->child_to_parent_hwirq = pmic_gpio_child_to_parent_hwirq;
//	girq->populate_parent_alloc_arg = gpiochip_populate_parent_fwspec_fourcell;
//	girq->child_offset_to_irq = pmic_gpio_child_offset_to_irq;
//	girq->child_irq_domain_ops.translate = pmic_gpio_domain_translate;

	ret = gpiochip_add_data(&state->chip, state);
	if (ret) {
		dev_err(state->dev, "%s: can't add gpio chip\n", __func__);
		return ret;
	}

	/*
	 * For DeviceTree-supported systems, the gpio core checks the
	 * pinctrl's device node for the "gpio-ranges" property.
	 * If it is present, it takes care of adding the pin ranges
	 * for the driver. In this case the driver can skip ahead.
	 *
	 * In order to remain compatible with older, existing DeviceTree
	 * files which don't set the "gpio-ranges" property or systems that
	 * utilize ACPI the driver has to call gpiochip_add_pin_range().
	 */
	if (!of_property_read_bool(dev->of_node, "gpio-ranges")) {
		ret = gpiochip_add_pin_range(&state->chip, dev_name(dev), 0, 0,
					     state->npins);
		if (ret) {
			dev_err(dev, "%s: failed to add pin range\n", __func__);
			goto err_range;
		}
	}

	dev_info(dev, "%s: end\n", __func__);

	return 0;

err_range:
	gpiochip_remove(&state->chip);

	return ret;
}

static int pmic_gpio_remove(struct platform_device *pdev)
{
	struct pmic_gpio_state *state = platform_get_drvdata(pdev);

	gpiochip_remove(&state->chip);

	return 0;
}

static const struct of_device_id pmic_gpio_of_match[] = {
	{ .compatible = "s2mpu14-gpio" },
	{ },
};

MODULE_DEVICE_TABLE(of, pmic_gpio_of_match);

static struct platform_driver pmic_gpio_driver = {
	.driver = {
		   .name = "s2mpu14-gpio",
		   .of_match_table = pmic_gpio_of_match,
	},
	.probe	= pmic_gpio_probe,
	.remove = pmic_gpio_remove,
};

module_platform_driver(pmic_gpio_driver);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Samsung PMIC GPIO pin control driver");
MODULE_ALIAS("platform:samsung-spmi-gpio");
MODULE_LICENSE("GPL v2");
