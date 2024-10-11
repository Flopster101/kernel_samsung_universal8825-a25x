/*
 * leds-s2mf301.c - LED class driver for S2MF301 FLASH LEDs.
 *
 * Copyright (C) 2023 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/mfd/samsung/s2mf301.h>
#include <linux/leds-s2mf301.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#define CONTROL_I2C	0
#define CONTROL_GPIO	1

struct s2mf301_fled_data *g_fled_data;
struct device *camera_flash_dev;
extern void pdo_ctrl_by_flash(bool mode);

static char *fled_supplied_to[] = {
	"battery",
};

static enum power_supply_property s2mf301_fled_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *s2mf301_fled_operating_mode_string[] = {
	"AUTO",
	"BOOST",
	"TA",
	"SYS",
};

/* IC current limit */
static int s2mf301_fled_torch_curr_max[] = {
	0, 320
};

/* IC current limit */
static int s2mf301_fled_flash_curr_max[] = {
	0, 1600
};

#if IS_ENABLED(DEBUG_TEST_READ)
static void s2mf301_fled_test_read(struct s2mf301_fled_data *fled)
{
	u8 data;
	char str[1016] = {0,};
	int i;
	struct i2c_client *i2c = fled->i2c;

	s2mf301_read_reg(i2c, 0x09, &data);
	pr_err("%s: %s0x09:0x%02x\n", __func__, str, data);

	for (i = 0x10; i <= 0x11; i++) {
		s2mf301_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	for (i = 0x12; i <= 0x14; i++) {
		s2mf301_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	for (i = 0x15; i <= 0x17; i++) {
		s2mf301_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}
}
#endif

static int s2mf301_fled_get_flash_curr(struct s2mf301_fled_data *fled, int chan)
{
	int curr = -1;
	u8 data;
	u8 dest;

	if ((chan < 1) || (chan > S2MF301_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MF301_FLED_CH1_CTRL0;
		break;
	}

	s2mf301_read_reg(fled->i2c, dest, &data);

	data = data & S2MF301_CHX_FLASH_IOUT;
	curr = (data * 50) + 50;

	pr_info("%s: CH%02d flash curr. = %dmA\n", __func__, chan, curr);

	return curr;
}

static int s2mf301_fled_set_flash_curr(struct s2mf301_fled_data *fled, int chan, int curr)
{
	int ret = -1;
	u8 data;
	u8 dest;
	int curr_set;

	if ((chan < 1) || (chan > S2MF301_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MF301_FLED_CH1_CTRL0;
		break;
	}

	if (curr < 50)
		curr = 50;
	else if (curr > s2mf301_fled_flash_curr_max[chan])
		curr = s2mf301_fled_flash_curr_max[chan];

	data = (curr - 50)/50;

	s2mf301_update_reg(fled->i2c, dest, data, S2MF301_CHX_FLASH_IOUT);

	curr_set = s2mf301_fled_get_flash_curr(fled, chan);

	pr_info("%s: curr: %d, curr_set: %d\n", __func__, curr, curr_set);

	return ret;
}

static int s2mf301_fled_get_torch_curr(struct s2mf301_fled_data *fled, int chan)
{
	int curr = -1;
	u8 data;
	u8 dest;

	if ((chan < 1) || (chan > S2MF301_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MF301_FLED_CH1_CTRL1;
		break;
	}

	s2mf301_read_reg(fled->i2c, dest, &data);

	data = data & S2MF301_CHX_TORCH_IOUT;
	curr = data * 10 + 10;

	pr_info("%s: CH%02d torch curr. = %dmA\n", __func__, chan, curr);

	return curr;
}

static int s2mf301_fled_set_torch_curr(struct s2mf301_fled_data *fled, int chan, int curr)
{
	int ret = -1;
	u8 data;
	u8 dest;
	int curr_set;

	if ((chan < 1) || (chan > S2MF301_CH_MAX)) {
		pr_info("%s: Wrong channel!!\n", __func__);
		return -1;
	}

	switch (chan) {
	case 1:
		dest = S2MF301_FLED_CH1_CTRL1;
		break;
	}

	if (curr < 10)
		curr = 10;
	else if (curr > s2mf301_fled_torch_curr_max[chan])
		curr = s2mf301_fled_torch_curr_max[chan];

	data = (curr - 10)/10;

	s2mf301_update_reg(fled->i2c, dest, data, S2MF301_CHX_TORCH_IOUT);

	curr_set = s2mf301_fled_get_torch_curr(fled, chan);

	pr_info("%s: curr: %d, curr_set: %d\n", __func__, curr, curr_set);

	ret = 0;

	return ret;

}

static void s2mf301_fled_operating_mode(struct s2mf301_fled_data *fled, int mode)
{
	u8 value;

	if (fled->set_on_factory) {
		pr_info("%s Factory Status, Return\n", __func__);
		return;
	}

	if (mode < 0 || mode > 3) {
		pr_info("%s, wrong mode\n", __func__);
		mode = AUTO_MODE;
	}

	pr_info("%s = %s\n", __func__, s2mf301_fled_operating_mode_string[mode]);

	value = mode << 6;
	s2mf301_update_reg(fled->i2c, S2MF301_FLED_CTRL0, value, 0xC0);
}

static int s2mf301_fled_set_mode(struct s2mf301_fled_data *fled, int chan, int mode)
{
	u8 status = 0, chg_sts = 0;
	union power_supply_propval value;
	int gpio_torch = fled->pdata->torch_gpio;
	int gpio_flash = fled->pdata->flash_gpio;

	if ((chan <= 0) || (chan > S2MF301_CH_MAX) || (mode < 0) || (mode > S2MF301_FLED_MODE_MAX)) {
		pr_err("%s: Wrong channel or mode.\n", __func__);
		return -EFAULT;
	}
	pr_info("%s: channel: %d, mode: %d\n", __func__, chan, mode);
	gpio_request(gpio_torch, "s2mf301_gpio_torch");
	gpio_request(gpio_flash, "s2mf301_gpio_flash");

	/* 0b000: OFF, 0b101: i2c bit control(on) */
	switch (mode) {
	case S2MF301_FLED_MODE_OFF:
		gpio_direction_output(gpio_torch, 0);
		gpio_direction_output(gpio_flash, 0);

		if (fled->ic_301x) {
			/* W/A Reactivate charger mode */
			s2mf301_read_reg(fled->i2c, S2MF301_FLED_STATUS1, &status);
			s2mf301_read_reg(fled->chg, 0x0A, &chg_sts);
			/* flash -> off, TA exist */
			if ((status & S2MF301_CH1_FLASH_ON) && (chg_sts & 1 << 3)) {
				/* charger off */
				s2mf301_update_reg(fled->chg, 0x18, 0x0, 0x0F);
				mdelay(1);
				/* charger mode */
				s2mf301_update_reg(fled->chg, 0x18, 0x03, 0x0F);
			}
		} else {
			if (!fled->psy_chg)
				fled->psy_chg = power_supply_get_by_name("s2mf301-charger");
			if (fled->is_en_flash) {
				fled->is_en_flash = value.intval = false;
				power_supply_set_property(fled->psy_chg,
						POWER_SUPPLY_PROP_ENERGY_AVG, &value);
			}
		}
		s2mf301_update_reg(fled->i2c, S2MF301_FLED_CTRL0, 0, S2MF301_EN_FLED_PRE);
		s2mf301_fled_operating_mode(fled, AUTO_MODE);
		fled->pdata->en_flash = false;
		fled->pdata->en_torch = false;
		break;
	case S2MF301_FLED_MODE_FLASH:
		s2mf301_update_reg(fled->i2c, S2MF301_FLED_CTRL0,
				S2MF301_EN_FLED_PRE, S2MF301_EN_FLED_PRE);
		if (!fled->set_on_factory) {
			if (!fled->ic_301x) {
				if (!fled->psy_chg)
					fled->psy_chg = power_supply_get_by_name("s2mf301-charger");

				fled->is_en_flash = value.intval = true;
				power_supply_set_property(fled->psy_chg,
						POWER_SUPPLY_PROP_ENERGY_AVG, &value);
			}
		}
		gpio_direction_output(gpio_flash, 1);
		break;
	case S2MF301_FLED_MODE_TORCH:
		s2mf301_update_reg(fled->i2c, S2MF301_FLED_CTRL0,
				S2MF301_EN_FLED_PRE, S2MF301_EN_FLED_PRE);
		s2mf301_fled_operating_mode(fled, SYS_MODE);
		gpio_direction_output(gpio_torch, 1);
		break;
	default:
		return -EFAULT;
	}

	gpio_free(gpio_torch);
	gpio_free(gpio_flash);

	return 0;
}

int s2mf301_mode_change_cam_to_leds(enum cam_flash_mode cam_mode)
{
	int mode = -1;

	switch (cam_mode) {
	case CAM_FLASH_MODE_OFF:
		mode = S2MF301_FLED_MODE_OFF;
		break;
	case CAM_FLASH_MODE_SINGLE:
		mode = S2MF301_FLED_MODE_FLASH;
		break;
	case CAM_FLASH_MODE_TORCH:
		mode = S2MF301_FLED_MODE_TORCH;
		break;
	default:
		mode = S2MF301_FLED_MODE_OFF;
		break;
	}

	return mode;
}

int s2mf301_fled_set_mode_ctrl(int chan, enum cam_flash_mode cam_mode)
{
	struct s2mf301_fled_data *fled = g_fled_data;
	int mode = -1;

	mode = s2mf301_mode_change_cam_to_leds(cam_mode);

	if ((chan <= 0) || (chan > S2MF301_CH_MAX) || (mode < 0) || (mode >= S2MF301_FLED_MODE_MAX)) {
		pr_err("%s: channel: %d, mode: %d\n", __func__, chan, mode);
		pr_err("%s: Wrong channel or mode.\n", __func__);
		return -1;
	}

	s2mf301_fled_set_mode(fled, chan, mode);
#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(s2mf301_fled_set_mode_ctrl);

int s2mf301_fled_set_curr(int chan, enum cam_flash_mode cam_mode)
{
	struct s2mf301_fled_data *fled = g_fled_data;
	int mode = -1;

	mode = s2mf301_mode_change_cam_to_leds(cam_mode);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MF301_CH_MAX)) {
		pr_err("%s: Wrong channel.\n", __func__);
		return -EFAULT;
	}

	switch (mode) {
	case S2MF301_FLED_MODE_TORCH:
		/* Set curr. */
		s2mf301_fled_set_torch_curr(fled, chan, fled->preflash_current);
		break;
	case S2MF301_FLED_MODE_FLASH:
		/* Set curr. */
		s2mf301_fled_set_flash_curr(fled, chan, fled->flash_current);
		break;
	default:
		return -1;
	}
	/* Test read */
#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(s2mf301_fled_set_curr);

int s2mf301_fled_get_curr(int chan, enum cam_flash_mode cam_mode)
{
	struct s2mf301_fled_data *fled = g_fled_data;
	int mode = -1;
	int curr = 0;

	mode = s2mf301_mode_change_cam_to_leds(cam_mode);

	/* Check channel */
	if ((chan <= 0) || (chan > S2MF301_CH_MAX)) {
		pr_err("%s: Wrong channel.\n", __func__);
		return -EFAULT;
	}

	switch (mode) {
	case S2MF301_FLED_MODE_TORCH:
		curr = s2mf301_fled_get_torch_curr(fled, chan);
		break;
	case S2MF301_FLED_MODE_FLASH:
		curr = s2mf301_fled_get_flash_curr(fled, chan);
		break;
	default:
		return -1;
	}
	/* Test read */
#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif

	return curr;
}

static int s2mf301_fled_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mf301_fled_data *fled = power_supply_get_drvdata(psy);
	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		val->intval = fled->set_on_factory;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int s2mf301_fled_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2mf301_fled_data *fled = power_supply_get_drvdata(psy);
	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		if (val->intval) {
			s2mf301_fled_operating_mode(fled, TA_MODE);
			fled->set_on_factory = 1;
		} else {
			fled->set_on_factory = 0;
			s2mf301_fled_operating_mode(fled, AUTO_MODE);
		}
		pr_info("%s, set_on_factory(%d)!\n", __func__, fled->set_on_factory);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void s2mf301_fled_init(struct s2mf301_fled_data *fled)
{
	int i;
	struct i2c_client *i2c = fled->i2c;
	u8 data;

	pr_info("%s: s2mf301_fled init start\n", __func__);

	for (i = 1; i <= S2MF301_CH_MAX; i++) {
		s2mf301_fled_set_flash_curr(fled, i, fled->pdata->default_current);
		s2mf301_fled_set_torch_curr(fled, i, fled->pdata->default_current);
	}
	s2mf301_write_reg(i2c, 0x24, 0xC8);

	s2mf301_read_reg(i2c, S2MF301_FLED_IC_VER, &data);
	fled->ic_301x = (data == 0x18) ? true : false;
	s2mf301_read_reg(i2c, S2MF301_FLED_PMIC_ID, &data);
	fled->rev_id = data & S2MF301_FLED_REV_NO;
	pr_info("%s: PMIC_ID = 0x%02x, Rev. No. = %d\n", __func__, data, fled->rev_id);

	if (fled->rev_id >= 1) {
		/* Set Auto source change for flash mode boosting
		 * VBUS_DET(0x1D[5] = 1), T_VBUS_DET_MASK_B(0x1E[6] = 1)
		 */
		s2mf301_update_reg(i2c, 0x1D, 1 << 5, 1 << 5);
		s2mf301_update_reg(i2c, 0x1E, 1 << 6, 1 << 6);
	}

	pr_info("%s: GPIO control!", __func__);
	s2mf301_update_reg(i2c, S2MF301_FLED_CTRL1, S2MF301_FLED_FLASH_GPIO << S2MF301_FLED_FLASH_SHIFT, S2MF301_CHX_FLASH_FLED_EN);
	s2mf301_update_reg(i2c, S2MF301_FLED_CTRL1, S2MF301_FLED_TORCH_GPIO << S2MF301_FLED_TORCH_SHIFT, S2MF301_CHX_TORCH_FLED_EN);
#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(fled);
#endif
}

static int s2mf301_led_dt_parse_pdata(struct device *dev, struct s2mf301_fled_platform_data *pdata)
{
	struct device_node *led_np, *np, *c_np;
	int ret;
	u32 temp;
	u32 index;

	led_np = dev->parent->of_node;

	if (!led_np) {
		pr_err("<%s> could not find led sub-node led_np\n", __func__);
		return -ENODEV;
	}

	np = of_find_node_by_name(led_np, "flash_led");
	if (!np) {
		pr_err("%s : could not find led sub-node np\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "default_current", &pdata->default_current);
	if (ret < 0)
		pr_err("%s : could not find default_current\n", __func__);

	ret = of_property_read_u32(np, "max_current", &pdata->max_current);
	if (ret < 0)
		pr_err("%s : could not find max_current\n", __func__);

	ret = of_property_read_u32(np, "default_timer", &pdata->default_timer);
	if (ret < 0)
		pr_err("%s : could not find default_timer\n", __func__);


	ret = pdata->flash_gpio = of_get_named_gpio(np, "flash-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get flash-gpio\n", __func__);
		return ret;
	}

	ret = pdata->torch_gpio = of_get_named_gpio(np, "torch-gpio", 0);
	if (ret < 0) {
		pr_err("%s : can't get torch-gpio\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "flash_current",
			&pdata->flash_current);
	if (ret < 0)
		pr_err("%s : could not find flash_current\n", __func__);

	ret = of_property_read_u32(np, "preflash_current",
			&pdata->preflash_current);
	if (ret < 0)
		pr_err("%s : could not find preflash_current\n", __func__);

	ret = of_property_read_u32(np, "torch_current",
			&pdata->torch_current);
	if (ret < 0)
		pr_err("%s : could not find torch_current\n", __func__);

	ret = of_property_read_u32(np, "movie_current",
			&pdata->movie_current);
	if (ret < 0)
		pr_err("%s : could not find movie_current\n", __func__);

	ret = of_property_read_u32(np, "factory_torch_current",
			&pdata->factory_torch_current);
	if (ret < 0)
		pr_err("%s : could not find factory_torch_current\n", __func__);

	ret = of_property_read_u32(np, "factory_flash_current",
			&pdata->factory_flash_current);
	if (ret < 0)
		pr_err("%s : could not find factory_flash_current\n", __func__);

	ret = of_property_read_u32_array(np, "flashlight_current",
			pdata->flashlight_current, S2MF301_FLASH_LIGHT_MAX);
	if (ret < 0) {
		pr_err("%s : could not find flashlight_current\n", __func__);

		pdata->flashlight_current[0] = 50;
		pdata->flashlight_current[1] = 80;
		pdata->flashlight_current[2] = 110;
		pdata->flashlight_current[3] = 170;
		pdata->flashlight_current[4] = 250;
	}

	pdata->chan_num = of_get_child_count(np);

	if (pdata->chan_num > S2MF301_CH_MAX)
		pdata->chan_num = S2MF301_CH_MAX;

	pdata->channel = devm_kzalloc(dev, sizeof(struct s2mf301_fled_chan) * pdata->chan_num, GFP_KERNEL);

	for_each_child_of_node(np, c_np) {
		ret = of_property_read_u32(c_np, "id", &temp);
		if (ret < 0)
			goto dt_err;
		index = temp;

		pr_info("%s: temp = %d, index = %d\n", __func__, temp, index);

		if (index < S2MF301_CH_MAX) {
			pdata->channel[index].id = index;

			ret = of_property_read_u32_index(np, "current", index, &pdata->channel[index].curr);
			if (ret < 0) {
				pr_err("%s : could not find current for channel%d\n",
					__func__, pdata->channel[index].id);
				pdata->channel[index].curr = pdata->default_current;
			}

			ret = of_property_read_u32_index(np, "timer", index, &pdata->channel[index].timer);
			if (ret < 0) {
				pr_err("%s : could not find timer for channel%d\n",
					__func__, pdata->channel[index].id);
				pdata->channel[index].timer = pdata->default_timer;
			}
		}
	}
	return 0;
dt_err:
	pr_err("%s: DT parsing finish. ret = %d\n", __func__, ret);
	return ret;
}

static ssize_t s2mf301_rear_flash_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	int mode = -1;
	int value = 0;
	int flash_current = 0;
	int torch_current = 0;

	if (g_fled_data == NULL) {
		pr_err("s2mf301-fled: %s: g_fled_data is NULL\n", __func__);
		return -ENODEV;
	}

	if ((buf == NULL) || kstrtouint(buf, 10, &value))
		return -ENXIO;

	pr_info("%s: %d: rear_flash_store:\n", __func__, value);
	g_fled_data->sysfs_input_data = value;

	flash_current = g_fled_data->pdata->default_current;
	torch_current = g_fled_data->pdata->default_current;

	if (value == 0) {
		if (g_fled_data->pdata->en_flash == false && g_fled_data->pdata->en_torch == false) {
			goto exit;
		}
		mode = S2MF301_FLED_MODE_OFF;
	} else if (value == 1) {
		mode = S2MF301_FLED_MODE_TORCH;
	} else if (value == 100) {
		/* Factory Torch*/
		torch_current = g_fled_data->factory_torch_current;
		mode = S2MF301_FLED_MODE_TORCH;
		g_fled_data->pdata->en_torch = true;
		pr_info("%s: factory torch current [%d]\n", __func__,
			torch_current);
	} else if (value == 200) {
		/* Factory Flash */
		if (g_fled_data->pdata->en_flash == true) {
			/* Turn off flash and torch if already on */
			s2mf301_fled_set_flash_curr(g_fled_data, 1, 0);
			s2mf301_fled_set_torch_curr(g_fled_data, 1, 0);
			s2mf301_fled_set_mode(g_fled_data, 1, S2MF301_FLED_MODE_OFF);
		}
		flash_current = g_fled_data->factory_flash_current;
		mode = S2MF301_FLED_MODE_FLASH;
		pr_info("%s: factory flash current [%d]\n", __func__, flash_current);
	} else if (value <= 1010 && value >= 1001) {
		mode = S2MF301_FLED_MODE_TORCH;
		/* (value) 1001, 1002, 1004, 1006, 1009 */
		if (value <= 1001)
			torch_current = g_fled_data->pdata->flashlight_current[0];
		else if (value <= 1002)
			torch_current = g_fled_data->pdata->flashlight_current[1];
		else if (value <= 1004)
			torch_current = g_fled_data->pdata->flashlight_current[2];
		else if (value <= 1006)
			torch_current = g_fled_data->pdata->flashlight_current[3];
		else if (value <= 1009)
			torch_current = g_fled_data->pdata->flashlight_current[4];
		else
			torch_current = g_fled_data->pdata->torch_current;
		g_fled_data->sysfs_input_data = 1;
	} else if (value == 2) {
		mode = S2MF301_FLED_MODE_FLASH;
	} else {
		pr_err("%s Unsupported value : %d",  __func__, value);
		goto exit;
	}
	if (mode == S2MF301_FLED_MODE_TORCH) {
		pr_info("%s: %d: S2MF301_FLED_MODE_TORCH - %dmA\n", __func__, value, torch_current);
		/* torch current set */
		s2mf301_fled_set_torch_curr(g_fled_data, 1, torch_current);
		s2mf301_fled_set_mode(g_fled_data, 1, S2MF301_FLED_MODE_TORCH);
		g_fled_data->pdata->en_torch = true;
	} else if (mode == S2MF301_FLED_MODE_FLASH) {
		pr_info("%s: %d: S2MF301_FLED_MODE_FLASH - %dmA\n", __func__, value, flash_current);
		/* flash current set */
		s2mf301_fled_set_flash_curr(g_fled_data, 1, flash_current);
		s2mf301_fled_set_mode(g_fled_data, 1, S2MF301_FLED_MODE_FLASH);
		g_fled_data->pdata->en_flash = true;
	} else {
		pr_info("%s: %d: S2MF301_FLED_MODE_OFF\n", __func__, value);
		/* false torch current set for initial current */
		/* flash current set */
		s2mf301_fled_set_flash_curr(g_fled_data, 1, flash_current);
		/* torch current set */
		s2mf301_fled_set_torch_curr(g_fled_data, 1, torch_current);
		s2mf301_fled_set_mode(g_fled_data, 1, S2MF301_FLED_MODE_OFF);
	}
#if IS_ENABLED(DEBUG_TEST_READ)
	s2mf301_fled_test_read(g_fled_data);
#endif

exit:
	pr_info("%s: rear_flash_store END\n", __func__);
	return size;
}

static ssize_t s2mf301_rear_flash_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_fled_data->sysfs_input_data);
}

static DEVICE_ATTR(rear_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, s2mf301_rear_flash_show, s2mf301_rear_flash_store);

//static DEVICE_ATTR(rear_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, sm5714_rear_flash_show, sm5714_rear_flash_store);
int s2mf301_create_sysfs(struct class *class)
{
	if (class == NULL) {
		class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(class)) {
			pr_err("Failed to create class(camera)!\n");
			return PTR_ERR(class);
		}
	}

	camera_flash_dev = device_create(class, NULL, 3, NULL, "flash");

	if (IS_ERR(camera_flash_dev)) {
		pr_err("<%s> Failed to create device(flash)!\n", __func__);
	} else {
		if (device_create_file(camera_flash_dev, &dev_attr_rear_flash) < 0) {
			pr_err("<%s> failed to create device file, %s\n",
				__func__, dev_attr_rear_flash.attr.name);
		}
	}

	return 0;
}
EXPORT_SYMBOL(s2mf301_create_sysfs);

int s2mf301_destroy_sysfs(struct class *class)
{
	if (camera_flash_dev)
		device_remove_file(camera_flash_dev, &dev_attr_rear_flash);

	if (class && camera_flash_dev)
		device_destroy(class, camera_flash_dev->devt);

	return 0;
}
EXPORT_SYMBOL(s2mf301_destroy_sysfs);

static int s2mf301_led_probe(struct platform_device *pdev)
{
	int ret = 0;
	int cnt = 0;
	struct s2mf301_dev *s2mf301 = dev_get_drvdata(pdev->dev.parent);
	struct s2mf301_fled_data *fled_data;
	struct power_supply_config psy_cfg = {};

	pr_info("%s: start\n", __func__);

	if (!s2mf301) {
		dev_err(&pdev->dev, "drvdata->dev.parent not supplied\n");
		return -ENODEV;
	}

	fled_data = devm_kzalloc(&pdev->dev, sizeof(struct s2mf301_fled_data), GFP_KERNEL);
	if (!fled_data) {
		pr_err("%s: failed to allocate devm\n", __func__);
		return -ENOMEM;
	}

	fled_data->dev = &pdev->dev;
	fled_data->i2c = s2mf301->i2c;
	fled_data->chg = s2mf301->chg;
	fled_data->pdata = devm_kzalloc(&pdev->dev, sizeof(*(fled_data->pdata)), GFP_KERNEL);
	if (!fled_data->pdata) {
		pr_err("%s: failed to allocate platform data\n", __func__);
		return -ENOMEM;
	}
	fled_data->pdata->en_flash = false;
	fled_data->pdata->en_torch = false;
	fled_data->set_on_factory = 0;

	if (s2mf301->dev->of_node) {
		ret = s2mf301_led_dt_parse_pdata(&pdev->dev, fled_data->pdata);
		if (ret < 0) {
			pr_err("%s: not found leds dt! ret=%d\n", __func__, ret);
			return -1;
		}
	}

	platform_set_drvdata(pdev, fled_data);

	fled_data->psy_fled_desc.name      = "s2mf301-fled";
	fled_data->psy_fled_desc.type      = POWER_SUPPLY_TYPE_UNKNOWN;
	fled_data->psy_fled_desc.get_property      = s2mf301_fled_get_property;
	fled_data->psy_fled_desc.set_property      = s2mf301_fled_set_property;
	fled_data->psy_fled_desc.properties        = s2mf301_fled_props;
	fled_data->psy_fled_desc.num_properties = ARRAY_SIZE(s2mf301_fled_props);

	psy_cfg.drv_data = fled_data;
	psy_cfg.supplied_to = fled_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(fled_supplied_to);

	fled_data->psy_fled = power_supply_register(&pdev->dev, &fled_data->psy_fled_desc, &psy_cfg);

	if (IS_ERR(fled_data->psy_fled)) {
		pr_err("%s: Failed to Register psy_fled\n", __func__);
		ret = PTR_ERR(fled_data->psy_fled);
	}

	s2mf301_fled_init(fled_data);

	/* Store fled_data for EXPORT_SYMBOL */
	g_fled_data = fled_data;
	g_fled_data->flash_gpio					= fled_data->pdata->flash_gpio;
	g_fled_data->torch_gpio					= fled_data->pdata->torch_gpio;
	g_fled_data->default_current				= fled_data->pdata->default_current;
	g_fled_data->flash_current				= fled_data->pdata->flash_current;
	g_fled_data->torch_current				= fled_data->pdata->torch_current;
	g_fled_data->preflash_current				= fled_data->pdata->preflash_current;
	g_fled_data->movie_current				= fled_data->pdata->movie_current;
	g_fled_data->factory_torch_current			= fled_data->pdata->factory_torch_current;
	g_fled_data->factory_flash_current			= fled_data->pdata->factory_flash_current;

	for (cnt = 0; cnt < S2MF301_FLASH_LIGHT_MAX; cnt++) {
		g_fled_data->flashlight_current[cnt] = fled_data->pdata->flashlight_current[cnt];
	}

	pr_info("%s: end\n", __func__);
	return 0;
}

static int s2mf301_led_remove(struct platform_device *pdev)
{
	return 0;
}

static void s2mf301_led_shutdown(struct platform_device *pdev)
{
	struct s2mf301_fled_data *fled_data =
		platform_get_drvdata(pdev);
	int chan;

	if (!fled_data->i2c) {
		pr_err("%s: no i2c client\n", __func__);
		return;
	}

	/* Turn off all leds when power off */
	pr_info("%s: turn off all leds\n", __func__);
	for (chan = 1; chan <= S2MF301_CH_MAX; chan++)
		s2mf301_fled_set_mode(fled_data, chan, S2MF301_FLED_MODE_SHUTDOWN_OFF);
}

static struct platform_driver s2mf301_led_driver = {
	.driver = {
		.name  = "leds-s2mf301",
		.owner = THIS_MODULE,
		},
	.probe  = s2mf301_led_probe,
	.remove = s2mf301_led_remove,
	.shutdown = s2mf301_led_shutdown,
};

static int __init s2mf301_led_driver_init(void)
{
	return platform_driver_register(&s2mf301_led_driver);
}
module_init(s2mf301_led_driver_init);

static void __exit s2mf301_led_driver_exit(void)
{
	platform_driver_unregister(&s2mf301_led_driver);
}
module_exit(s2mf301_led_driver_exit);

MODULE_DESCRIPTION("SAMSUNG s2mf301 flash LED Driver");
MODULE_LICENSE("GPL");
