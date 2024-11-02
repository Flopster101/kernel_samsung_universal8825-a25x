/* linux/arch/arm/mach-exynos/setup-is-module.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS gpio and clock configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include <exynos-is-module.h>
#include "is-i2c-config.h"
#include "is-vender.h"
#include "is-time.h"

static int acquire_shared_rsc(struct exynos_sensor_pin *pin_ctrls)
{
	if (pin_ctrls->shared_rsc_count)
		return atomic_inc_return(pin_ctrls->shared_rsc_count);

	return 1;
}

static int release_shared_rsc(struct exynos_sensor_pin *pin_ctrls)
{
	if (pin_ctrls->shared_rsc_count)
		return atomic_dec_return(pin_ctrls->shared_rsc_count);

	return 0;
}

static bool need_to_control_shared_rsc(struct exynos_sensor_pin *pin_ctrls,
					struct is_module_enum *module)
{
	unsigned long flags;
	int active_count = 0;
	u32 delay = pin_ctrls->delay;

	spin_lock_irqsave(pin_ctrls->shared_rsc_slock, flags);

	if (pin_ctrls->shared_rsc_type == SRT_ACQUIRE)
		active_count = acquire_shared_rsc(pin_ctrls);
	else if (pin_ctrls->shared_rsc_type == SRT_RELEASE)
		active_count = release_shared_rsc(pin_ctrls);

	minfo("[%s] shared rsc (act(%d), pin(%ld), val(%d), nm(%s), active(cur: %d, target: %d))\n",
			module, module->sensor_name,
			pin_ctrls->act,
			(pin_ctrls->act == PIN_FUNCTION) ? 0 : pin_ctrls->pin,
			pin_ctrls->value,
			pin_ctrls->name,
			active_count,
			pin_ctrls->shared_rsc_active);

	spin_unlock_irqrestore(pin_ctrls->shared_rsc_slock, flags);

	if (active_count != pin_ctrls->shared_rsc_active) {
		udelay(delay);
		return false;
	}

	return true;
}

static int is_module_regulator_ctrl(struct is_module_regulator *imr,
				struct exynos_sensor_pin *pin_ctrls,
				struct is_module_enum *module,
				struct device *dev)
{
	char* name = pin_ctrls->name;
	u32 delay = pin_ctrls->delay;
	u32 value = pin_ctrls->value;
	u32 vol = pin_ctrls->voltage;
	int ret;

	if (value) {
		if (!imr->regulator)
			imr->regulator = regulator_get_optional(dev, name);

		if (IS_ERR_OR_NULL(imr->regulator)) {
			merr("[%s] regulator_get(%s) fail\n",
					module, module->sensor_name, name);

			ret = PTR_ERR(imr->regulator);
			imr->regulator = NULL;
			return ret;
		}

		if (regulator_is_enabled(imr->regulator)) {
			mwarn("[%s] regulator(%s) is already enabled\n",
					module, module->sensor_name, name);
			udelay(delay);
			return 0;
		}

		if (vol > 0) {
			minfo("[%s] regulator_set_voltage(%d)\n",
					module, module->sensor_name, vol);
			ret = regulator_set_voltage(imr->regulator, vol, vol);
			if(ret) {
				merr("[%s] regulator_set_voltage(%d) fail\n",
						module, module->sensor_name, ret);
			}
		}

		ret = regulator_enable(imr->regulator);

		if (pin_ctrls->actuator_i2c_delay > 0) {
			module->act_available_time = ktime_add_us(ktime_get_boottime(),
							pin_ctrls->actuator_i2c_delay);
		}

		if (ret) {
			merr("[%s] regulator_enable(%s) fail\n",
					module, module->sensor_name, name);
			regulator_put(imr->regulator);
			imr->regulator = NULL;
			return ret;
		}
	} else {
		if (IS_ERR_OR_NULL(imr->regulator)) {
			merr("[%s] regulator(%s) get failed\n",
					module, module->sensor_name, name);
			return PTR_ERR(imr->regulator);
		}

#ifdef CAMERA_RETENTION_SHARE_POWER
		if (!strcmp(name, CAMERA_RETENTION_SHARE_POWER)
			&& (module->ext.use_retention_mode == SENSOR_RETENTION_ACTIVATED
				|| module->ext.use_retention_mode == SENSOR_RETENTION_UNSUPPORTED)) {
			mwarn("[%s] regulator(%s) is retention IO pin\n",
						module, module->sensor_name, name);
			imr->regulator = NULL;
			udelay(delay);
			return 0;
		}
#endif

		if (!regulator_is_enabled(imr->regulator)) {
			mwarn("[%s] regulator(%s) is already disabled\n",
					module, module->sensor_name, name);
			regulator_put(imr->regulator);
			imr->regulator = NULL;
			udelay(delay);
			return 0;
		}

		ret = regulator_disable(imr->regulator);
		if (ret) {
			merr("[%s] regulator_disable(%s) fail\n",
					module, module->sensor_name, name);
			regulator_put(imr->regulator);
			imr->regulator = NULL;
			return ret;
		}

		regulator_put(imr->regulator);
		imr->regulator = NULL;
	}

	udelay(delay);

	return 0;
}

static int is_module_regulator_opt(struct exynos_sensor_pin *pin_ctrls,
				struct is_module_enum *module,
				struct device *dev)
{
	char* name = pin_ctrls->name;
	u32 delay = pin_ctrls->delay;
	u32 value = pin_ctrls->value;
	struct regulator *regulator;
	int ret;

	regulator = regulator_get(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		merr("[%s] regulator_get(%s) fail\n",
				module, module->sensor_name, name);
		return PTR_ERR(regulator);
	}

	if (value) {
		ret = regulator_set_mode(regulator, REGULATOR_MODE_FAST);
		if(ret) {
			dev_err(dev, "Failed to configure fPWM mode: %d\n", ret);
			regulator_put(regulator);
			return ret;
		}
	} else {
		ret = regulator_set_mode(regulator, REGULATOR_MODE_NORMAL);
		if (ret) {
			dev_err(dev, "Failed to configure auto mode: %d\n", ret);
			regulator_put(regulator);
			return ret;
		}
	}

	udelay(delay);
	regulator_put(regulator);

	return 0;
}

static int is_module_mclk_ctrl(struct exynos_sensor_pin *pin_ctrls,
				struct is_module_enum *module,
				struct is_device_sensor *ids,
				u32 scenario)
{
	u32 delay = pin_ctrls->delay;
	u32 value = pin_ctrls->value;
	int ret;

	if (pin_ctrls->shared_rsc_type) {
		if (!ids->pdata) {
			merr("[%s] failed to get platform data (postion: %d)\n",
					module, module->sensor_name, ids->position);
			return -EINVAL;
		}

		if (!module->pdata) {
			merr("[%s] failed to get platform data for module\n",
					module, module->sensor_name);
			return -EINVAL;
		}

		if (value) {
			if (ids->pdata->mclk_on)
				ret = ids->pdata->mclk_on(&ids->pdev->dev,
						scenario, module->pdata->mclk_ch,
						module->pdata->mclk_freq_khz);
			else
				ret = -EPERM;
		} else {
			if (ids->pdata->mclk_off)
				ret = ids->pdata->mclk_off(&ids->pdev->dev,
						scenario, module->pdata->mclk_ch);
			else
				ret = -EPERM;
		}
	} else {
		if (value)
			ret = is_sensor_mclk_on(ids,
					scenario, module->pdata->mclk_ch,
					module->pdata->mclk_freq_khz);
		else
			ret = is_sensor_mclk_off(ids,
					scenario, module->pdata->mclk_ch);
	}

	if (ret) {
		merr("[%s] failed to %s MCLK(%d)", module, module->sensor_name,
				value ? "on" : "off",  ret);
		return ret;
	}

	udelay(delay);

	return 0;
}

static int exynos_is_module_pin_control(struct is_module_enum *module,
	struct pinctrl *pinctrl, struct exynos_sensor_pin *pin_ctrls, u32 scenario)
{
	struct device *dev = module->dev;
	char* name = pin_ctrls->name;
	ulong pin = pin_ctrls->pin;
	u32 delay = pin_ctrls->delay;
	u32 value = pin_ctrls->value;
	u32 act = pin_ctrls->act;
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct is_device_sensor *sensor;
	struct is_resourcemgr *resourcemgr;

	if (pin_ctrls->shared_rsc_type &&
			!need_to_control_shared_rsc(pin_ctrls, module))
		return 0;

	minfo("[%s] pin_ctrl(%s, val(%d), delay(%d), act(%d), pin(%ld))\n", module,
		module->sensor_name, name, value, delay, act, (act == PIN_FUNCTION) ? 0 : pin);

	subdev_module = module->subdev;
	if (!subdev_module) {
		merr("[%s] module's subdev was not probed",
			module, module->sensor_name);
		return -ENODEV;
	}

	sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev_module);
	if (!sensor) {
		merr("[%s] failed to get sensor device\n", module, module->sensor_name);
		return -EINVAL;
	}

	resourcemgr = sensor->resourcemgr;
	if (!sensor) {
		merr("[%s] failed to get resourcemgr\n", module, module->sensor_name);
		return -EINVAL;
	}

	switch (act) {
	case PIN_NONE:
		udelay(delay);
		break;
	case PIN_OUTPUT:
		if (gpio_is_valid(pin)) {
			if (value)
				gpio_request_one(pin, GPIOF_OUT_INIT_HIGH, "CAM_GPIO_OUTPUT_HIGH");
			else
				gpio_request_one(pin, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
			udelay(delay);
			gpio_free(pin);
		}
		break;
	case PIN_INPUT:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_IN, "CAM_GPIO_INPUT");
			gpio_free(pin);
		}
		break;
	case PIN_RESET:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_OUT_INIT_HIGH, "CAM_GPIO_RESET");
			udelay(1000);
			__gpio_set_value(pin, 0);
			udelay(1000);
			__gpio_set_value(pin, 1);
			udelay(1000);
			gpio_free(pin);
		}
		break;
	case PIN_FUNCTION:
		{
			struct pinctrl_state *s = (struct pinctrl_state *)pin;

			ret = pinctrl_select_state(pinctrl, s);
			if (ret < 0) {
				merr("[%s] pinctrl_select_state(%s) is fail(%d)\n",
					module, module->sensor_name, name, ret);
				return ret;
			}
			udelay(delay);
		}
		break;
	case PIN_REGULATOR:
		{
			struct is_module_regulator *imr = NULL;
			list_for_each_entry(imr, &resourcemgr->regulator_list, list) {
				if (!strcmp(imr->name, name))
					break;
			}

			if (IS_ERR_OR_NULL(imr)) {
				merr("[%s] failed to get is_module_regulator",
					module, module->sensor_name);
				return -EINVAL;
			}

			return is_module_regulator_ctrl(imr, pin_ctrls, module, dev);
		}
		break;
	case PIN_REGULATOR_OPTION:
		return is_module_regulator_opt(pin_ctrls, module, dev);
	case PIN_I2C:
		ret = is_i2c_pin_control(module, scenario, value);
		udelay(delay);
		break;
	case PIN_MCLK:
		return is_module_mclk_ctrl(pin_ctrls, module, sensor, scenario);
	default:
		merr("[%s] unknown act for pin\n", module, module->sensor_name);
		break;
	}

	return ret;
}

int exynos_is_module_pins_cfg(struct is_module_enum *module,
	u32 scenario,
	u32 gpio_scenario)
{
	int ret = 0;
	u32 idx_max, idx;
	struct pinctrl *pinctrl;
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX];
	struct exynos_platform_is_module *pdata;

	FIMC_BUG(!module);
	FIMC_BUG(!module->pdata);
	FIMC_BUG(gpio_scenario >= GPIO_SCENARIO_MAX);
	FIMC_BUG(scenario > SENSOR_SCENARIO_MAX);

	pdata = module->pdata;
	pinctrl = pdata->pinctrl;
	pin_ctrls = pdata->pin_ctrls;
	idx_max = pdata->pinctrl_index[scenario][gpio_scenario];

	if (idx_max == 0) {
		err("There is no such a scenario(scen:%d, on:%d)", scenario, gpio_scenario);
		ret = -EINVAL;
		goto p_err;
	}

	minfo("[%s][P%d:S%d:GS%d]: pin_ctrl start\n", module,
		module->sensor_name, module->position, scenario, gpio_scenario);

	/* do configs */
	for (idx = 0; idx < idx_max; ++idx) {
		ret = exynos_is_module_pin_control(module, pinctrl,
			&pin_ctrls[scenario][gpio_scenario][idx], scenario);
		if (ret) {
			merr("[%s] exynos_is_module_pin_control(%d) is fail(%d)", module,
				module->sensor_name, idx, ret);
			goto p_err;
		}
	}

	minfo("[%s][P%d:S%d:GS%d]: pin_ctrl end\n", module, module->sensor_name,
		module->position, scenario, gpio_scenario);
p_err:
	return ret;
}

static int exynos_is_module_pin_debug(struct device *dev,
	struct pinctrl *pinctrl, struct exynos_sensor_pin *pin_ctrls)
{
	int ret = 0;
	ulong pin = pin_ctrls->pin;
	char* name = pin_ctrls->name;
	u32 act = pin_ctrls->act;

	switch (act) {
	case PIN_NONE:
		break;
	case PIN_OUTPUT:
	case PIN_INPUT:
	case PIN_RESET:
		if (gpio_is_valid(pin))
			pr_info("[@] pin %s : %d\n", name, gpio_get_value(pin));
		break;
	case PIN_FUNCTION:
#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7890)
		{
			/* there is no way to get pin by name after probe */
			ulong base, pin;
			u32 index;

			base = (ulong)ioremap(0x13470000, 0x10000);

			index = 0x60; /* GPC2 */
			pin = base + index;
			pr_info("[@] CON[0x%X] : 0x%X\n", index, readl((void *)pin));
			pr_info("[@] DAT[0x%X] : 0x%X\n", index, readl((void *)(pin + 4)));

			index = 0x160; /* GPD7 */
			pin = base + index;
			pr_info("[@] CON[0x%X] : 0x%X\n", index, readl((void *)pin));
			pr_info("[@] DAT[0x%X] : 0x%X\n", index, readl((void *)(pin + 4)));

			iounmap((void *)base);
		}
#endif
		break;
	case PIN_REGULATOR:
		{
			struct regulator *regulator;
			int voltage;

			regulator = regulator_get_optional(dev, name);
			if (IS_ERR(regulator)) {
				pr_err("%s : regulator_get(%s) fail\n", __func__, name);
				return PTR_ERR(regulator);
			}

			if (regulator_is_enabled(regulator))
				voltage = regulator_get_voltage(regulator);
			else
				voltage = 0;

			regulator_put(regulator);

			pr_info("[@] %s LDO : %d\n", name, voltage);
		}
		break;
	default:
		pr_err("unknown act for pin\n");
		break;
	}

	return ret;
}

int exynos_is_module_pins_dbg(struct is_module_enum *module,
	u32 scenario,
	u32 gpio_scenario)
{
	int ret = 0;
	u32 idx_max, idx;
	struct pinctrl *pinctrl;
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX];
	struct exynos_platform_is_module *pdata;

	FIMC_BUG(!module);
	FIMC_BUG(!module->pdata);
	FIMC_BUG(gpio_scenario > 1);
	FIMC_BUG(scenario > SENSOR_SCENARIO_MAX);

	pdata = module->pdata;
	pinctrl = pdata->pinctrl;
	pin_ctrls = pdata->pin_ctrls;
	idx_max = pdata->pinctrl_index[scenario][gpio_scenario];

	/* print configs */
	for (idx = 0; idx < idx_max; ++idx) {
		minfo("[%s] pin_ctrl(act(%d), pin(%ld), val(%d), nm(%s)\n", module,
			module->sensor_name,
			pin_ctrls[scenario][gpio_scenario][idx].act,
			(pin_ctrls[scenario][gpio_scenario][idx].act == PIN_FUNCTION) ? 0 : pin_ctrls[scenario][gpio_scenario][idx].pin,
			pin_ctrls[scenario][gpio_scenario][idx].value,
			pin_ctrls[scenario][gpio_scenario][idx].name);
	}

	/* do configs */
	for (idx = 0; idx < idx_max; ++idx) {
		ret = exynos_is_module_pin_debug(module->dev, pinctrl, &pin_ctrls[scenario][gpio_scenario][idx]);
		if (ret) {
			merr("[%s] exynos_is_module_pin_debug(%d) is fail(%d)", module,
				module->sensor_name, idx, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

void is_get_gpio_ops(struct exynos_platform_is_module *pdata)
{
	pdata->gpio_cfg = exynos_is_module_pins_cfg;
	pdata->gpio_dbg = exynos_is_module_pins_dbg;
}
