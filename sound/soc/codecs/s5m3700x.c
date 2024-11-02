/*
 * sound/soc/codec/s5m3700x.c
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/samsung/abox.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/completion.h>
#include <soc/samsung/acpm_mfd.h>
#include <uapi/linux/input-event-codes.h>

#include "s5m3700x.h"
#include "s5m3700x-jack.h"

#define S5M3700X_CODEC_SW_VER	2008

static struct s5m3700x_priv *s5m3700x_stat;
static void codec_regdump(struct snd_soc_component *codec);

void s5m3700x_usleep(unsigned int u_sec)
{
	usleep_range(u_sec, u_sec + 10);
}
EXPORT_SYMBOL_GPL(s5m3700x_usleep);

bool s5m3700x_is_probe_done(void)
{
	if (s5m3700x_stat == NULL)
		return false;
	return s5m3700x_stat->is_probe_done;
}
EXPORT_SYMBOL_GPL(s5m3700x_is_probe_done);

int s5m3700x_i2c_read_reg(u8 slave, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	struct i2c_client *client;
	u8 buf[2];
	unsigned char retry;
	int ret;

	client = s5m3700x_stat->i2c_priv[S5M3700D];

	/* Write Format */
	buf[0] = reg;
	buf[1] = slave;
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	/* Read Format */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = val;

	mutex_lock(&s5m3700x_stat->i2c_mutex);
	for (retry = 0; retry < 5; retry++) {
		ret = i2c_transfer(client->adapter, msg, 2);

		if (ret == 2)
			break;
		else if (ret < 0)
			pr_err("[%s] i2c read failed! err: %d\n", __func__, ret);
	}

	mutex_unlock(&s5m3700x_stat->i2c_mutex);
	return ret-2;
}
EXPORT_SYMBOL_GPL(s5m3700x_i2c_read_reg);

int s5m3700x_i2c_write_reg(u8 slave, u8 reg, u8 val)
{
	struct i2c_msg msg;
	struct i2c_client *client;
	u8 buf[3];
	unsigned char retry;
	int ret;

	client = s5m3700x_stat->i2c_priv[S5M3700D];

	/* Write Format */
	buf[0] = reg;
	buf[1] = slave;
	buf[2] = val;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = buf;

	mutex_lock(&s5m3700x_stat->i2c_mutex);
	for (retry = 0; retry < 5; retry++) {
		ret = i2c_transfer(client->adapter, &msg, 1);

		if (ret == 1)
			break;
		else if (ret < 0)
			pr_err("[%s] i2c write failed! err: %d\n", __func__, ret);
	}

	mutex_unlock(&s5m3700x_stat->i2c_mutex);
	return ret-1;
}
EXPORT_SYMBOL_GPL(s5m3700x_i2c_write_reg);

int s5m3700x_i2c_update_reg(u8 slave, u8 reg, u8 mask, u8 val)
{
	struct i2c_msg msg[2];
	struct i2c_client *client;
	u8 buf[3];
	unsigned char retry;
	int ret;
	u8 old, temp;

	mutex_lock(&s5m3700x_stat->i2c_mutex);

	client = s5m3700x_stat->i2c_priv[S5M3700D];

	/* Write Format */
	buf[0] = reg;
	buf[1] = slave;
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	/* Read Format */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &old;

	for (retry = 0; retry < 5; retry++) {
		ret = i2c_transfer(client->adapter, msg, 2);

		if (ret == 2)
			break;
		else if (ret < 0) {
			pr_err("[%s] i2c update failed! err: %d\n", __func__, ret);
			goto i2c_err;
		}
	}

	temp = old & ~mask;
	temp |= val & mask;

	if (temp != old) {
		/* Write Format */
		buf[0] = reg;
		buf[1] = slave;
		buf[2] = temp;
		msg[0].addr = client->addr;
		msg[0].flags = 0;
		msg[0].len = 3;
		msg[0].buf = buf;

		for (retry = 0; retry < 5; retry++) {
			ret = i2c_transfer(client->adapter, &msg[0], 1);

			if (ret == 1)
				break;
			else if (ret < 0) {
				pr_err("[%s] i2c update failed! err: %d\n", __func__, ret);
				goto i2c_err;
			}
		}
	}
	ret = 0;

	mutex_unlock(&s5m3700x_stat->i2c_mutex);
i2c_err:
	mutex_unlock(&s5m3700x_stat->i2c_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_i2c_update_reg);

int s5m3700x_read(struct s5m3700x_priv *s5m3700x,
		unsigned int slave, unsigned int addr, unsigned int *value)
{
	int ret;

	switch (slave) {
	case S5M3700X_DIGITAL_ADDR:
		ret = regmap_read(s5m3700x->regmap[S5M3700D], addr, value);
		break;
	case S5M3700X_ANALOG_ADDR:
		ret = regmap_read(s5m3700x->regmap[S5M3700A], addr, value);
		break;
	case S5M3700X_OTP_ADDR:
		ret = regmap_read(s5m3700x->regmap[S5M3700O], addr, value);
		break;
	default:
		dev_err(s5m3700x->dev, "%s: Cannot find slave address. addr: 0x%02X%02X\n",
				__func__, slave, addr);
		ret = -1;
		break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_read);

int s5m3700x_write(struct s5m3700x_priv *s5m3700x,
		unsigned int slave, unsigned int addr, unsigned int value)
{
	int ret;

	switch (slave) {
	case S5M3700X_DIGITAL_ADDR:
		ret = regmap_write(s5m3700x->regmap[S5M3700D], addr, value);
		break;
	case S5M3700X_ANALOG_ADDR:
		ret = regmap_write(s5m3700x->regmap[S5M3700A], addr, value);
		break;
	case S5M3700X_OTP_ADDR:
		ret = regmap_write(s5m3700x->regmap[S5M3700O], addr, value);
		break;
	default:
		dev_err(s5m3700x->dev, "%s: Cannot find slave address. addr: 0x%02X%02X\n",
				__func__, slave, addr);
		ret = -1;
		break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_write);

int s5m3700x_update_bits(struct s5m3700x_priv *s5m3700x,
		unsigned int slave, unsigned int addr, unsigned int mask, unsigned int value)
{
	int ret;

	switch (slave) {
	case S5M3700X_DIGITAL_ADDR:
		ret = regmap_update_bits(s5m3700x->regmap[S5M3700D], addr, mask, value);
		break;
	case S5M3700X_ANALOG_ADDR:
		ret = regmap_update_bits(s5m3700x->regmap[S5M3700A], addr, mask, value);
		break;
	case S5M3700X_OTP_ADDR:
		ret = regmap_update_bits(s5m3700x->regmap[S5M3700O], addr, mask, value);
		break;
	default:
		dev_err(s5m3700x->dev, "%s: Cannot find slave address. addr: 0x%02X%02X\n",
				__func__, slave, addr);
		ret = -1;
		break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(s5m3700x_update_bits);

void regcache_cache_switch(struct s5m3700x_priv *s5m3700x, bool on)
{
	int p_map;

	mutex_lock(&s5m3700x->regcache_lock);
	if (!on)
		s5m3700x->regcache_count++;
	else
		s5m3700x->regcache_count--;

	mutex_unlock(&s5m3700x->regcache_lock);

	if (!on) {
		if (s5m3700x->regcache_count == 1)
			for (p_map = S5M3700D; p_map <= S5M3700O; p_map++)
				regcache_cache_only(s5m3700x->regmap[p_map], on);
	} else {
		if (s5m3700x->regcache_count == 0)
			for (p_map = S5M3700D; p_map <= S5M3700O; p_map++)
				regcache_cache_only(s5m3700x->regmap[p_map], on);
	}
}
EXPORT_SYMBOL_GPL(regcache_cache_switch);

int s5m3700x_regulators_enable(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int ret;

	ret = regulator_enable(s5m3700x->vdd);
	ret = regulator_enable(s5m3700x->vdd2);

	s5m3700x->regulator_count++;

	return ret;
}

void s5m3700x_regulators_disable(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	s5m3700x->regulator_count--;

	if (s5m3700x->regulator_count == 0) {
		regulator_disable(s5m3700x->vdd);
		regulator_disable(s5m3700x->vdd2);
	}
}

/*
 * Return Value
 * True: If the register value cannot be cached, hence we have to read from the
 * hardware directly.
 * False: If the register value can be read from cache.
 */
static bool s5m3700x_volatile_register(struct device *dev, unsigned int reg)
{
	/*
	 * For all the registers for which we want to restore the value during
	 * regcache_sync operation, we need to return true here. For registers
	 * whose value need not be cached and restored should return false here.
	 *
	 * For the time being, let us cache the value of all registers other
	 * than the IRQ pending and IRQ status registers.
	 */
	switch (reg) {
	case 0x01 ... 0x06:
		return true;
	default:
		return false;
	}
}

/*
 * Return Value
 * True: If the register value can be read
 * False: If the register cannot be read
 */
static bool s5m3700x_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x08 ... 0x0F:
	case 0x10 ... 0x29:
	case 0x2F ... 0x8F:
	case 0x93 ... 0x9E:
	case 0xA0 ... 0xEE:
	case 0xF0 ... 0xFF:
		return true;
	default:
		return false;
	}
}

/*
 * Return Value
 * True: If the register value can be write
 * False: If the register cannot be write
 */
static bool s5m3700x_writeable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x08 ... 0x0D:
	case 0x10 ... 0x25:
	case 0x29:
	case 0x2F ... 0x56:
	case 0x58 ... 0x74:
	case 0x77 ... 0x7F:
	case 0x89 ... 0x8E:
	case 0x93 ... 0x9A:
	case 0x9D ... 0x9E:
	case 0xA0 ... 0xBE:
	case 0xC0 ... 0xCD:
	case 0xD0 ... 0xEE:
	case 0xFD ... 0xFF:
		return true;
	default:
		return false;
	}
}

static int s5m3700x_reg_digital_hw_write(void *context,
		unsigned int reg, unsigned int value)
{
	return s5m3700x_i2c_write_reg(S5M3700X_DIGITAL_ADDR, (u8)reg, (u8)value);
}

static int s5m3700x_reg_digital_hw_read(void *context,
		unsigned int reg, unsigned int *value)
{
	return s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, (u8)reg, (u8 *)value);
}

static const struct regmap_bus s5m3700x_dbus = {
	.reg_write = s5m3700x_reg_digital_hw_write,
	.reg_read = s5m3700x_reg_digital_hw_read,
};

const struct regmap_config s5m3700x_digital_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	/*
	 * "i3c" string should be described in the name field
	 * this will be used for the i3c inteface,
	 * when read/write operations are used in the regmap driver.
	 * APM functions will be called instead of the I2C
	 * refer to the "drivers/base/regmap/regmap-i2c.c
	 */
	.name = "i2c, S5M3700X",
	.max_register = S5M3700X_REGCACHE_SYNC_END,
	.readable_reg = s5m3700x_readable_register,
	.writeable_reg = s5m3700x_writeable_register,
	.volatile_reg = s5m3700x_volatile_register,
	.use_single_read = true,
	.use_single_write = true,
	.cache_type = REGCACHE_RBTREE,
};


static bool s5m3700x_volatile_analog_register(struct device *dev, unsigned int reg)
{
	/*
	 * For all the registers for which we want to restore the value during
	 * regcache_sync operation, we need to return true here. For registers
	 * whose value need not be cached and restored should return false here.
	 *
	 * For the time being, let us cache the value of all registers other
	 * than the IRQ pending and IRQ status registers.
	 */
	switch (reg) {
	default:
		return false;
	}
}

static bool s5m3700x_readable_analog_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x02 ... 0x0D:
	case 0x10 ... 0x23:
	case 0x25:
	case 0x30 ... 0x32:
	case 0x35:
	case 0x38 ... 0x3B:
	case 0x3F ... 0x40:
	case 0x42 ... 0x45:
	case 0x49 ... 0x4A:
	case 0x4C ... 0x4D:
	case 0x50 ... 0x56:
	case 0x80 ... 0x95:
		return true;
	default:
		return false;
	}
}

static bool s5m3700x_writeable_analog_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x02 ... 0x0D:
	case 0x10 ... 0x23:
	case 0x25:
	case 0x30 ... 0x32:
	case 0x35:
	case 0x38 ... 0x3B:
	case 0x3F ... 0x40:
	case 0x42 ... 0x45:
	case 0x49 ... 0x4A:
	case 0x4C ... 0x4D:
	case 0x50 ... 0x56:
		return true;
	default:
		return false;
	}
}

static int s5m3700x_reg_analog_hw_write(void *context,
		unsigned int reg, unsigned int value)
{
	return s5m3700x_i2c_write_reg(S5M3700X_ANALOG_ADDR, (u8)reg, (u8)value);
}

static int s5m3700x_reg_analog_hw_read(void *context,
		unsigned int reg, unsigned int *value)
{
	return s5m3700x_i2c_read_reg(S5M3700X_ANALOG_ADDR, (u8)reg, (u8 *)value);
}

static const struct regmap_bus s5m3700x_abus = {
	.reg_write = s5m3700x_reg_analog_hw_write,
	.reg_read = s5m3700x_reg_analog_hw_read,
};

const struct regmap_config s5m3700x_analog_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.name = "i2c, S5M3700X_ANALOG",
	.max_register = S5M3700X_REGCACHE_SYNC_END,
	.readable_reg = s5m3700x_readable_analog_register,
	.writeable_reg = s5m3700x_writeable_analog_register,
	.volatile_reg = s5m3700x_volatile_analog_register,
	.use_single_read = true,
	.use_single_write = true,
	.cache_type = REGCACHE_RBTREE,
};

static bool s5m3700x_volatile_otp_register(struct device *dev, unsigned int reg)
{
	/*
	 * For all the registers for which we want to restore the value during
	 * regcache_sync operation, we need to return true here. For registers
	 * whose value need not be cached and restored should return false here.
	 *
	 * For the time being, let us cache the value of all registers other
	 * than the IRQ pending and IRQ status registers.
	 */
	switch (reg) {
	default:
		return false;
	}
}

static bool s5m3700x_readable_otp_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x00 ... 0x77:
	case 0x88 ... 0x8A:
	case 0xA6 ... 0xCB:
	case 0xD0 ... 0xF0:
		return true;
	default:
		return false;
	}
}

static bool s5m3700x_writeable_otp_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x00 ... 0x77:
	case 0x88 ... 0x8A:
	case 0xA6 ... 0xCB:
	case 0xD0 ... 0xF0:
		return true;
	default:
		return false;
	}
}

static int s5m3700x_reg_otp_hw_write(void *context,
		unsigned int reg, unsigned int value)
{
	return s5m3700x_i2c_write_reg(S5M3700X_OTP_ADDR, (u8)reg, (u8)value);
}

static int s5m3700x_reg_otp_hw_read(void *context,
		unsigned int reg, unsigned int *value)
{
	return s5m3700x_i2c_read_reg(S5M3700X_OTP_ADDR, (u8)reg, (u8 *)value);
}

static const struct regmap_bus s5m3700x_obus = {
	.reg_write = s5m3700x_reg_otp_hw_write,
	.reg_read = s5m3700x_reg_otp_hw_read,
};

const struct regmap_config s5m3700x_otp_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.name = "i2c, S5M3700X_OTP",
	.max_register = S5M3700X_REGCACHE_SYNC_END,
	.readable_reg = s5m3700x_readable_otp_register,
	.writeable_reg = s5m3700x_writeable_otp_register,
	.volatile_reg = s5m3700x_volatile_otp_register,
	.use_single_read = true,
	.use_single_write = true,
	.cache_type = REGCACHE_RBTREE,
};

bool read_from_cache(struct device *dev, unsigned int reg, int map_type)
{
	bool result = false;

	switch (map_type) {
	case S5M3700D:
		result = s5m3700x_readable_register(dev, reg) &&
			(!s5m3700x_volatile_register(dev, reg));
		break;
	case S5M3700A:
		result = s5m3700x_readable_analog_register(dev, reg) &&
			(!s5m3700x_volatile_analog_register(dev, reg));
		break;
	case S5M3700O:
		result = s5m3700x_readable_otp_register(dev, reg) &&
			(!s5m3700x_volatile_otp_register(dev, reg));
		break;
	}

	return result;
}

bool write_to_hw(struct device *dev, unsigned int reg, int map_type)
{
	bool result = false;

	switch (map_type) {
	case S5M3700D:
		result = s5m3700x_writeable_register(dev, reg) &&
			(!s5m3700x_volatile_register(dev, reg));
		break;
	case S5M3700A:
		result = s5m3700x_writeable_analog_register(dev, reg) &&
			(!s5m3700x_volatile_analog_register(dev, reg));
		break;
	case S5M3700O:
		result = s5m3700x_writeable_otp_register(dev, reg) &&
			(!s5m3700x_volatile_otp_register(dev, reg));
		break;
	}

	return result;
}

static void s5m3700x_regmap_sync(struct device *dev)
{
	struct s5m3700x_priv *s5m3700x = dev_get_drvdata(dev);
	int reg[S5M3700X_REGCACHE_SYNC_END] = {0,};
	int p_reg, p_map;

	for (p_map = S5M3700D; p_map <= S5M3700O; p_map++) {

		/* Read from Cache */
		for (p_reg = 0; p_reg < S5M3700X_REGCACHE_SYNC_END; p_reg++) {
			if (read_from_cache(dev, p_reg, p_map))
				regmap_read(s5m3700x->regmap[p_map], p_reg, &reg[p_reg]);
		}

		regcache_cache_bypass(s5m3700x->regmap[p_map], true);
		/* Update HW */
		for (p_reg = 0; p_reg < S5M3700X_REGCACHE_SYNC_END; p_reg++)
			if (write_to_hw(dev, p_reg, p_map))
				regmap_write(s5m3700x->regmap[p_map], p_reg, reg[p_reg]);
		regcache_cache_bypass(s5m3700x->regmap[p_map], false);
	}
}

static void s5m3700x_reg_restore(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	/* For reduce the delay, perform the first time at booting */
	if (!s5m3700x->is_probe_done) {

		msleep(40);

		s5m3700x_regmap_sync(codec->dev);
//		s5m3700x_reset_io_selector_bits(codec);
	} else {
		s5m3700x_regmap_sync(codec->dev);
	}
}

/*
 * s5m3700x_adc_digital_mute() - Set ADC digital Mute
 *
 * @codec: SoC audio codec device
 * @channel: Digital mute control for ADC channel
 * @on: mic mute is true, mic unmute is false
 *
 * Desc: When ADC path turn on, analog block noise can be recorded.
 * For remove this, ADC path was muted always except that it was used.
 */
void s5m3700x_adc_digital_mute(struct s5m3700x_priv *s5m3700x,
		unsigned int channel, bool on)
{
	struct device *dev = s5m3700x->dev;

	mutex_lock(&s5m3700x->adc_mute_lock);

	if (on)
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_30_ADC1, channel, channel);
	else
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_30_ADC1, channel, 0);

	mutex_unlock(&s5m3700x->adc_mute_lock);

	dev_info(dev, "%s called, %s work done.\n", __func__, on ? "Mute" : "Unmute");
}
EXPORT_SYMBOL_GPL(s5m3700x_adc_digital_mute);

/*
 * DAC(Rx) functions
 */
/*
 * s5m3700x_dac_soft_mute() - Set DAC soft mute
 *
 * @codec: SoC audio codec device
 * @channel: Soft mute control for DAC channel
 * @on: dac mute is true, dac unmute is false
 *
 * Desc: When DAC path turn on, analog block noise can be played.
 * For remove this, DAC path was muted always except that it was used.
 */
static void s5m3700x_dac_soft_mute(struct s5m3700x_priv *s5m3700x,
		unsigned int channel, bool on)
{
	struct device *dev = s5m3700x->dev;

	if (channel & DA_SMUTEL_MASK)
		mutex_lock(&s5m3700x->dacl_mute_lock);
	if (channel & DA_SMUTER_MASK)
		mutex_lock(&s5m3700x->dacr_mute_lock);

	if (on)
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_40_PLAY_MODE, channel, channel);
	else
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_40_PLAY_MODE, channel, 0);

	if (channel & DA_SMUTEL_MASK)
		mutex_unlock(&s5m3700x->dacl_mute_lock);
	if (channel & DA_SMUTER_MASK)
		mutex_unlock(&s5m3700x->dacr_mute_lock);

	dev_info(dev, "%s called, %s work done.\n", __func__, on ? "Mute" : "Unmute");
}

static void s5m3700x_adc_mute_work(struct work_struct *work)
{
	struct s5m3700x_priv *s5m3700x =
		container_of(work, struct s5m3700x_priv, adc_mute_work.work);
	unsigned int chop_val;

	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);
	dev_info(s5m3700x->dev, "%s called, chop: 0x%02X\n", __func__, chop_val);

	if (s5m3700x->capture_on)
		s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, false);

	s5m3700x->tx_dapm_done = true;
	if (s5m3700x->codec_debug && s5m3700x->tx_dapm_done && s5m3700x->capture_on)
		codec_regdump(s5m3700x->codec);
}

int check_adc_mode(int chop_val)
{
	unsigned int mode = 0;

	if (chop_val & MIC1_ON_MASK)
		mode++;
	if (chop_val & MIC2_ON_MASK)
		mode++;
	if (chop_val & MIC3_ON_MASK)
		mode++;
	if (chop_val & MIC4_ON_MASK)
		mode++;
	return mode;
}

static int vmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, dac_on, mic_status, vts_on, hifi_on;

	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);

	dac_on = chop_val & DAC_ON_MASK;
	vts_on = (s5m3700x->vts1_on | s5m3700x->vts2_on);
	hifi_on = s5m3700x->hifi_on;

	mic_status = check_adc_mode(chop_val);
	dev_info(codec->dev, "%s called, event = %d, adc mode: %d\n",
			__func__, event, mic_status);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Clock Gate */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
				SEQ_CLK_GATE_MASK | ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
				SEQ_CLK_GATE_MASK | ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK);

		switch (mic_status) {
		case AMIC_MONO:
		case AMIC_STEREO:
			/* Setting Stereo Mode */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
					ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK,
					ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK);

			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_16_CLK_MODE_SEL1,
					ADC_FSEL_MASK, ADC_MODE_STEREO << ADC_FSEL_SHIFT);

			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_30_ADC1,
					CH_MODE_MASK, ADC_CH_STEREO << CH_MODE_SHIFT);
			break;
		case AMIC_TRIPLE:
			/* Setting Triple Mode */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
					ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK | ADC_CIC_CGC_MASK,
					ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK | ADC_CIC_CGC_MASK);

			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_16_CLK_MODE_SEL1,
					ADC_FSEL_MASK, ADC_MODE_TRIPLE << ADC_FSEL_SHIFT);

			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_30_ADC1,
					CH_MODE_MASK, ADC_CH_TRIPLE << CH_MODE_SHIFT);
			break;
		default:
			break;
		}
		/* DMIC OSR Clear */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_33_ADC4,
				DMIC_OSR_MASK, 0);

		switch (s5m3700x->capture_aifrate) {
		case S5M3700X_SAMPLE_RATE_48KHZ:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_33_ADC4,
					CP_TYPE_SEL_MASK, FILTER_TYPE_NORMAL_AMIC << CP_TYPE_SEL_SHIFT);
			break;
		case S5M3700X_SAMPLE_RATE_192KHZ:
		case S5M3700X_SAMPLE_RATE_384KHZ:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_33_ADC4,
					CP_TYPE_SEL_MASK, FILTER_TYPE_UHQA_W_LP_AMIC << CP_TYPE_SEL_SHIFT);
			break;
		}

		/* DSML offset Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_11A_DSMC3, 0x00);

		/* DWA schema change */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_116_DSMC_1, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);

		if ((!dac_on) && (!hifi_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					AVC_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK, 0);

		if ((!dac_on) && (!hifi_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					AVC_RESETB_MASK | CORE_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* DSML offset Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_11A_DSMC3, 0xE0);

		/* DWA schema Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_116_DSMC_1, 0x80);

		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_33_ADC4, 0x00);
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
				ADC_CIC_CGC_MASK | ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
				ADC_CLK_GATE_MASK, 0);

		if ((!dac_on) && (!vts_on)) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
					SEQ_CLK_GATE_MASK, 0);

			if (!hifi_on)
				s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
						COM_CLK_GATE_MASK, 0);
		}
		break;
	}
	return 0;
}

static int dvmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, dac_on, dmic2_on, vts_on, hifi_on;

	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);

	dac_on = chop_val & DAC_ON_MASK;
	dmic2_on = chop_val & DMIC2_ON_MASK;
	vts_on = (s5m3700x->vts1_on | s5m3700x->vts2_on);
	hifi_on = s5m3700x->hifi_on;

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
				ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
				ADC_CLK_GATE_MASK | COM_CLK_GATE_MASK);

		if (!dmic2_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
					DMIC_CLK1_GATE_MASK, DMIC_CLK1_GATE_MASK);
		else
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_11_CLKGATE1,
					DMIC_CLK2_GATE_MASK, DMIC_CLK2_GATE_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* DMIC CLK ZTIE 0 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_32_ADC3,
				DMIC_CLK_ZTIE_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_33_ADC4,
				CP_TYPE_SEL_MASK | DMIC_OSR_MASK,
				FILTER_TYPE_NORMAL_DMIC << CP_TYPE_SEL_SHIFT |
				DMIC_OSR_64 << DMIC_OSR_SHIFT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_33_ADC4,
				CP_TYPE_SEL_MASK | DMIC_OSR_MASK, 0);

		/* DMIC CLK ZTIE 1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_32_ADC3,
				DMIC_CLK_ZTIE_MASK, DMIC_CLK_ZTIE_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
				ADC_CIC_CGL_MASK | ADC_CIC_CGR_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_11_CLKGATE1,
				DMIC_CLK2_GATE_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
				ADC_CLK_GATE_MASK | DMIC_CLK1_GATE_MASK, 0);

		if ((!dac_on) && (!vts_on) && (!hifi_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
					COM_CLK_GATE_MASK, 0);
		break;
	}
	return 0;
}

static int mic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* DSML Blocking & Chopping On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_115_DSM1,
				EN_ISIBLKL_MASK | EN_CHOPL_MASK | CAL_SKIP_MASK,
				EN_ISIBLKL_MASK | EN_CHOPL_MASK | CAL_SKIP_MASK);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* MIC1 APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_MIC1L_MASK, APW_MIC1L_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_BST2_MASK, SEL_BST2_MASK);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* MIC1 APW Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_MIC1L_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if ((!s5m3700x->vts1_on) && (!s5m3700x->vts2_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
					SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK |
					SEL_BST1_MASK | SEL_BST2_MASK, 0);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* DSML Blocking & Chopping Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_115_DSM1,
				EN_CHOPL_MASK | CAL_SKIP_MASK, 0);
		break;
	}
	return 0;
}

static int mic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Boost1 LCP On */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_BA_ODSEL11, 0x02);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_122_BST4PN, 0x80);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		/* DSML Blocking & Chopping On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_115_DSM1,
				EN_CHOPC_MASK | CAL_SKIP_MASK,
				EN_CHOPC_MASK | CAL_SKIP_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* MIC2 APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_MIC2C_MASK, APW_MIC2C_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_BST2_MASK, SEL_BST2_MASK);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* MIC2 APW Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_MIC2C_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if ((!s5m3700x->vts1_on) && (!s5m3700x->vts2_on)) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
					SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK | SEL_BST2_MASK, 0);

			/* Boost1 LCP Off */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_BA_ODSEL11, 0x00);
			s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_122_BST4PN, 0x00);
		}
		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* DSML Blocking & Chopping Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_115_DSM1,
				EN_CHOPC_MASK | CAL_SKIP_MASK, 0);
		break;
	}
	return 0;
}

static int mic3_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, mic1_on;

	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);

	mic1_on = chop_val & MIC1_ON_MASK;

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* MIC3/4 Selection */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_11D_BST_OP, 0x00);

		/* Boost1 LCP On */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_BA_ODSEL11, 0x02);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_122_BST4PN, 0x80);

		/* DSML Blocking & Chopping On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_115_DSM1,
				EN_CHOPR_MASK | CAL_SKIP_MASK,
				EN_CHOPR_MASK | CAL_SKIP_MASK);

		/* DSMR ISI Blocking */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_119_DSM3, 0x10);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_118_DSMC_2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_114_BST4,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* MIC3 APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_MIC3R_MASK, APW_MIC3R_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_BST2_MASK, SEL_BST2_MASK);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_MIC3R_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if ((!s5m3700x->vts1_on) && (!s5m3700x->vts2_on)) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
					SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK |
					SEL_BST3_MASK | SEL_BST2_MASK, 0);

			/* Boost1 LCP Off */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_BA_ODSEL11, 0x00);
			s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_122_BST4PN, 0x00);
		}

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_118_DSMC_2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_114_BST4,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* DSMR ISI Blocking */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_119_DSM3, 0x10);

		/* DSML Blocking & Chopping Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_115_DSM1,
				EN_CHOPR_MASK | CAL_SKIP_MASK, 0);

		/* MIC3/4 Selection */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_11D_BST_OP, 0x00);
		break;
	}
	return 0;
}

static int mic4_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, mic1_on;

	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);

	mic1_on = chop_val & MIC1_ON_MASK;

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* MIC3/4 Selection */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_11D_BST_OP, 0x04);

		/* DSML Blocking & Chopping On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_115_DSM1,
				EN_CHOPR_MASK | CAL_SKIP_MASK,
				EN_CHOPR_MASK | CAL_SKIP_MASK);

		/* DSMR ISI Blocking */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_119_DSM3, 0x10);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_118_DSMC_2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_114_BST4,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK,
				SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* MIC4 APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_MIC4R_MASK, APW_MIC4R_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_BST1_MASK, SEL_BST1_MASK);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* MIC4 APW Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_MIC4R_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if ((!s5m3700x->vts1_on) && (!s5m3700x->vts2_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
					SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK |
					SEL_BST4_MASK | SEL_BST1_MASK, 0);

		/* DSML Blocking & Chopping Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_115_DSM1,
				EN_CHOPR_MASK | CAL_SKIP_MASK, 0);

		/* BST VG Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_118_DSMC_2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* BST BSTR Control */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_114_BST4,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

		/* DSMR ISI Blocking */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_119_DSM3, 0x10);

		/* MIC3/4 Selection */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_11D_BST_OP, 0x00);
		break;
	}
	return 0;
}

static int dmic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, dac_on, hifi_on;

	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);

	dac_on = chop_val & DAC_ON_MASK;
	hifi_on = s5m3700x->hifi_on;

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_33_ADC4,
				DMIC_EN1_MASK, DMIC_EN1_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);

		if ((!dac_on) && (!hifi_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					AVC_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK, 0);

		if ((!dac_on) && (!hifi_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					AVC_RESETB_MASK | CORE_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_33_ADC4,
				DMIC_EN1_MASK, 0);
		break;
	}
	return 0;
}

static int dmic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, dac_on, hifi_on;

	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);

	dac_on = chop_val & DAC_ON_MASK;
	hifi_on = s5m3700x->hifi_on;

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_33_ADC4,
				DMIC_EN2_MASK, DMIC_EN2_MASK);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK,
				RSTB_ADC_MASK | ADC_RESETB_MASK | CORE_RESETB_MASK);

		if ((!dac_on) && (!hifi_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					AVC_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
				RSTB_ADC_MASK | ADC_RESETB_MASK, 0);

		if ((!dac_on) && (!hifi_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					AVC_RESETB_MASK | CORE_RESETB_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_33_ADC4,
				DMIC_EN2_MASK, 0);
		break;
	}
	return 0;
}

static int adc_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, dmic_on, amic_on;

	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);

	dmic_on = chop_val & DMIC_ON_MASK;
	amic_on = chop_val & AMIC_ON_MASK;

	dev_info(codec->dev, "%s called, event = %d, chop: 0x%02X\n",
			__func__, event, chop_val);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;
	case SND_SOC_DAPM_POST_PMU:
		cancel_delayed_work(&s5m3700x->adc_mute_work);
		if (amic_on)
			queue_delayed_work(s5m3700x->adc_mute_wq, &s5m3700x->adc_mute_work,
					msecs_to_jiffies(s5m3700x->amic_delay));
		else if (dmic_on)
			queue_delayed_work(s5m3700x->adc_mute_wq, &s5m3700x->adc_mute_work,
					msecs_to_jiffies(s5m3700x->dmic_delay));
		else
			queue_delayed_work(s5m3700x->adc_mute_wq, &s5m3700x->adc_mute_work,
					msecs_to_jiffies(220));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, true);
		s5m3700x->tx_dapm_done = false;
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	}
	return 0;
}

static int dac_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int chop_val, hp_on, ep_on, mic_on, vts_on, hifi_on;

	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);

	hp_on = chop_val & HP_ON_MASK;
	ep_on = chop_val & EP_ON_MASK;
	mic_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);
	vts_on = (s5m3700x->vts1_on | s5m3700x->vts2_on);
	hifi_on = s5m3700x->hifi_on;

	dev_info(codec->dev, "%s called, event = %d, chop: 0x%02X\n",
			__func__, event, chop_val);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (hp_on) {
			/* CLKGATE 0 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
					OVP_CLK_GATE_MASK | SEQ_CLK_GATE_MASK | AVC_CLK_GATE_MASK
					| DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
					OVP_CLK_GATE_MASK | SEQ_CLK_GATE_MASK | AVC_CLK_GATE_MASK
					| DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK);

			/* CLKGATE 1 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_11_CLKGATE1,
					DAC_CIC_TEMP_MASK | DAC_CIC_CGLR_MASK,
					DAC_CIC_TEMP_MASK | DAC_CIC_CGLR_MASK);

			/* CLKGATE 2 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
					CED_CLK_GATE_MASK, CED_CLK_GATE_MASK);

			/* RESETB */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK,
					AVC_RESETB_MASK | RSTB_DAC_DSM_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK);
		} else {
			/* CLKGATE 0 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
					SEQ_CLK_GATE_MASK | AVC_CLK_GATE_MASK
					| DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
					SEQ_CLK_GATE_MASK | AVC_CLK_GATE_MASK
					| DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK);

			/* CLKGATE 1 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_11_CLKGATE1,
					DAC_CIC_TEMP_MASK | DAC_CIC_CGC_MASK,
					DAC_CIC_TEMP_MASK | DAC_CIC_CGC_MASK);

			/* CLKGATE 2 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
					CED_CLK_GATE_MASK, CED_CLK_GATE_MASK);

			/* RESETB */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					AVC_RESETB_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK,
					AVC_RESETB_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_104_CTRL_CP, 0x88);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_104_CTRL_CP, 0xAA);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_106_CTRL_CP2, 0xFF);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_106_CTRL_CP2, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_104_CTRL_CP, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CLKGATE 2 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
				CED_CLK_GATE_MASK, 0);

		/* CLKGATE 1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_11_CLKGATE1,
				DAC_CIC_TEMP_MASK | DAC_CIC_CGLR_MASK | DAC_CIC_CGC_MASK, 0);

		/* CLKGATE 0 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
				OVP_CLK_GATE_MASK, 0);

		if (!hifi_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
					AVC_CLK_GATE_MASK | DAC_CLK_GATE_MASK, 0);

		if ((!mic_on) && (!vts_on)) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
					SEQ_CLK_GATE_MASK, 0);

			if (!hifi_on)
				s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
						COM_CLK_GATE_MASK, 0);
		}

		/* RESETB */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
				RSTB_DAC_DSM_MASK, 0);

		if (!hifi_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					AVC_RESETB_MASK | DAC_RESETB_MASK, 0);

		if ((!mic_on) && (!hifi_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					CORE_RESETB_MASK, 0);

		s5m3700x_usleep(5000);
		break;
	}
	return 0;
}

static void hp_32ohm(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xEE);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x35);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xEE);

		/* Class-H CP2 Setting */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C4_CP2_TH2_32, 0x7E);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C5_CP2_TH3_32, 0x82);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C6_CP2_TH1_16, 0x7C);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C7_CP2_TH2_16, 0x81);

		/* IDAC Chop Enable */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_131_PD_IDAC2, 0xE0);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_17_CLK_MODE_SEL2, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2BE_IDAC0_OTP, 0X81);

		/* BIAS & HPZ & CC Setting */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13A_POP_HP,
				EN_HP_BODY_MASK | SEL_RES_SUB_MASK, 0);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13B_OCP_HP, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B3_CTRL_IHP, 0x13);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B5_CTRL_HP1, 0x13);

		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_289_IDAC4_OTP, 0x1B);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2BF_IDAC1_OTP, 0x3A);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C0_IDAC2_OTP, 0x9B);

		switch (s5m3700x->playback_aifrate) {
		case S5M3700X_SAMPLE_RATE_48KHZ:
			s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x22);
			s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B4_CTRL_HP0, 0x63);
			break;
		case S5M3700X_SAMPLE_RATE_192KHZ:
		case S5M3700X_SAMPLE_RATE_384KHZ:
			s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x44);
			s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13A_POP_HP,
					EN_UHQA_MASK, EN_UHQA_MASK);
			s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B4_CTRL_HP0, 0x13);
			break;
		default:
			s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x22);
			s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B4_CTRL_HP0, 0x63);
			break;
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_7A_AVC43, 0x3A);
		/* HP APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_HP_MASK, APW_HP_MASK);

		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_7A_AVC43, 0x9A);

		/* OVP Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_142_REF_SURGE, 0x29);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_143_CTRL_OVP1, 0x12);

		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);

		s5m3700x->rx_dapm_done = true;
		if (s5m3700x->codec_debug && s5m3700x->rx_dapm_done && s5m3700x->playback_on)
			codec_regdump(codec);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);
		/* HP APW Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_HP_MASK, 0);

		/* OVP Off */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_143_CTRL_OVP1, 0x02);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_142_REF_SURGE, 0x67);
		s5m3700x->rx_dapm_done = false;
		break;
	case SND_SOC_DAPM_POST_PMD:

		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x50);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xBE);

		/* IDAC Chop Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_131_PD_IDAC2, 0xC0);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_17_CLK_MODE_SEL2, 0x08);

		/* BIAS & HPZ & CC Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13B_OCP_HP, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x33);
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13A_POP_HP,
				EN_HP_BODY_MASK | SEL_RES_SUB_MASK,
				EN_HP_BODY_MASK | SEL_RES_SUB_MASK);
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13A_POP_HP,
				EN_UHQA_MASK, 0);

		/* AVC & Playmode Setting Default */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_16_CLK_MODE_SEL1,
				DAC_FSEL_MASK | AVC_CLK_SEL_MASK, 0);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_40_PLAY_MODE, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_47_TRIM_DAC0, 0xF7);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_48_TRIM_DAC1, 0x4F);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_72_AVC35, 0x2F);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_74_AVC37, 0xE9);

		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_7A_AVC43, 0xC0);

		/* AVC Default Setting */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_50_AVC1, 0x05);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_51_AVC2, 0x6A);
		break;
	}
}

static void hp_6ohm(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xEE);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x35);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xEE);

		/* Class-H CP2 Setting */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C4_CP2_TH2_32, 0x7E);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C5_CP2_TH3_32, 0x82);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C6_CP2_TH1_16, 0x79);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C7_CP2_TH2_16, 0x80);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2CA_CP2_TH2_06, 0x75);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2CB_CP2_TH2_06, 0x7B);

		/* IDAC Chop Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_131_PD_IDAC2, 0xE0);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_17_CLK_MODE_SEL2, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2BE_IDAC0_OTP, 0x81);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_289_IDAC4_OTP, 0x1B);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2BF_IDAC1_OTP, 0x3A);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C0_IDAC2_OTP, 0x9B);

		/* BIAS & HPZ & CC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13B_OCP_HP, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B3_CTRL_IHP, 0x13);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B5_CTRL_HP1, 0x13);
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13A_POP_HP,
				EN_HP_BODY_MASK | SEL_RES_SUB_MASK, 0);
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13A_POP_HP,
				EN_UHQA_MASK, EN_UHQA_MASK);

		switch (s5m3700x->playback_aifrate) {
		case S5M3700X_SAMPLE_RATE_48KHZ:
			s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x22);
			s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B4_CTRL_HP0, 0x13);
			break;
		case S5M3700X_SAMPLE_RATE_192KHZ:
		case S5M3700X_SAMPLE_RATE_384KHZ:
			s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x44);
			s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B4_CTRL_HP0, 0x12);
			break;
		default:
			s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x22);
			s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B4_CTRL_HP0, 0x13);
			break;
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* HP APW On */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_HP_MASK, APW_HP_MASK);

		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_7A_AVC43, 0x9A);

		/* OVP Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_142_REF_SURGE, 0x29);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_143_CTRL_OVP1, 0x12);

		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);

		s5m3700x->rx_dapm_done = true;
		if (s5m3700x->codec_debug && s5m3700x->rx_dapm_done && s5m3700x->playback_on)
			codec_regdump(codec);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);
		/* HP APW Off */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_HP_MASK, 0);

		/* OVP Off */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR,  S5M3700X_143_CTRL_OVP1, 0x02);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_142_REF_SURGE, 0x67);
		s5m3700x->rx_dapm_done = false;
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x50);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xBE);

		/* IDAC Chop Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_131_PD_IDAC2, 0xC0);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_17_CLK_MODE_SEL2, 0x08);

		/* BIAS & HPZ & CC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13B_OCP_HP, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x33);
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13A_POP_HP,
				EN_HP_BODY_MASK | SEL_RES_SUB_MASK,
				EN_HP_BODY_MASK | SEL_RES_SUB_MASK);
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13A_POP_HP,
				EN_UHQA_MASK, 0);

		/* AVC & Playmode Setting Default */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_16_CLK_MODE_SEL1,
				DAC_FSEL_MASK | AVC_CLK_SEL_MASK, 0);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_40_PLAY_MODE, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_47_TRIM_DAC0, 0xF7);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_48_TRIM_DAC1, 0x4F);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_72_AVC35, 0x2F);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_74_AVC37, 0xE9);

		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_7A_AVC43, 0xC0);

		/* AVC Default Setting */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_50_AVC1, 0x05);
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_51_AVC2, 0x6A);
		break;
	}
}

static int hpdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	switch (s5m3700x->hp_imp) {
	case HP_IMP_32:
		dev_info(codec->dev, "%s called, hp: 32ohm, event = %d\n",
				__func__, event);
		hp_32ohm(codec, s5m3700x, event);
		break;
	case HP_IMP_6:
		dev_info(codec->dev, "%s called, hp: 16ohm, event = %d\n",
				__func__, event);
		hp_6ohm(codec, s5m3700x, event);
		break;
	default:
		dev_info(codec->dev, "%s called, default hp: 32ohm, event = %d\n",
				__func__, event);
		hp_32ohm(codec, s5m3700x, event);
	}
	return 0;
}

static void ep_32ohm(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				RSTB_DAC_DSM_C_MASK, RSTB_DAC_DSM_C_MASK);

		/* CP2 Zero-Cross */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_4F_HSYS_CP2_SIGN, 0x82);

		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xBD);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x07);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xBD);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x22);

		/* Class-H */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C4_CP2_TH2_32, 0x7C);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C5_CP2_TH3_32, 0x80);

		/* IDAC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_288_IDAC3_OTP, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_289_IDAC4_OTP, 0x15);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_28A_IDAC5_OTP, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_149_PD_EP, 0x80);

		/* EP Control Setting */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B7_CTRL_EP0, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B8_CTRL_EP1, 0x13);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B9_CTRL_EP2, 0x63);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_RCV_MASK, APW_RCV_MASK);

		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);
		s5m3700x->rx_dapm_done = true;
		if (s5m3700x->codec_debug && s5m3700x->rx_dapm_done && s5m3700x->playback_on)
			codec_regdump(codec);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_RCV_MASK, 0);
		s5m3700x->rx_dapm_done = false;
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_4F_HSYS_CP2_SIGN, 0x82);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x50);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x33);

		/* IDAC & EP Control Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_149_PD_EP, 0xD0);

		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				RSTB_DAC_DSM_C_MASK, 0);
		break;
	}
}

static void ep_12ohm(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				RSTB_DAC_DSM_C_MASK, RSTB_DAC_DSM_C_MASK);

		/* CP2 Zero-Cross */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_4F_HSYS_CP2_SIGN, 0x82);

		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xCC);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x07);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xCC);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x22);

		/* Class-H */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C4_CP2_TH2_32, 0x7B);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C5_CP2_TH3_32, 0x7F);

		/* IDAC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_288_IDAC3_OTP, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_289_IDAC4_OTP, 0x15);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_28A_IDAC5_OTP, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_149_PD_EP, 0x80);

		/* EP Control Setting */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B7_CTRL_EP0, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B8_CTRL_EP1, 0x24);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B9_CTRL_EP2, 0x63);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_RCV_MASK, APW_RCV_MASK);

		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);
		s5m3700x->rx_dapm_done = true;
		if (s5m3700x->codec_debug && s5m3700x->rx_dapm_done && s5m3700x->playback_on)
			codec_regdump(codec);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_RCV_MASK, 0);
		s5m3700x->rx_dapm_done = false;
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_4F_HSYS_CP2_SIGN, 0x82);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x50);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x33);

		/* IDAC & EP Control Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_149_PD_EP, 0xD0);

		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				RSTB_DAC_DSM_C_MASK, 0);
		break;
	}
}

static void ep_6ohm(struct snd_soc_component *codec, struct s5m3700x_priv *s5m3700x, int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				RSTB_DAC_DSM_C_MASK, RSTB_DAC_DSM_C_MASK);

		/* CP2 Zero-Cross */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_4F_HSYS_CP2_SIGN, 0x82);

		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xCC);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x07);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xCC);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x22);

		/* Class-H */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C4_CP2_TH2_32, 0x75);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C5_CP2_TH3_32, 0x79);

		/* IDAC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_288_IDAC3_OTP, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_289_IDAC4_OTP, 0x15);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_28A_IDAC5_OTP, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_149_PD_EP, 0x80);

		/* EP Control Setting */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B7_CTRL_EP0, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B8_CTRL_EP1, 0x24);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2B9_CTRL_EP2, 0x63);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_RCV_MASK, APW_RCV_MASK);

		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);
		s5m3700x->rx_dapm_done = true;
		if (s5m3700x->codec_debug && s5m3700x->rx_dapm_done && s5m3700x->playback_on)
			codec_regdump(codec);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_RCV_MASK, 0);
		s5m3700x->rx_dapm_done = false;
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_4F_HSYS_CP2_SIGN, 0x82);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x50);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xBE);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x33);

		/* IDAC & EP Control Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_149_PD_EP, 0xD0);

		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				RSTB_DAC_DSM_C_MASK, 0);
		break;
	}
}
static int epdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	switch (s5m3700x->rcv_imp) {
	case RCV_IMP_32:
		dev_info(codec->dev, "%s called, ep: 32ohm, event = %d\n",
				__func__, event);
		ep_32ohm(codec, s5m3700x, event);
		break;
	case RCV_IMP_12:
		dev_info(codec->dev, "%s called, ep: 12ohm, event = %d\n",
				__func__, event);
		ep_12ohm(codec, s5m3700x, event);
		break;
	case RCV_IMP_6:
		dev_info(codec->dev, "%s called, ep: 6ohm, event = %d\n",
				__func__, event);
		ep_6ohm(codec, s5m3700x, event);
		break;
	default:
		dev_info(codec->dev, "%s called, default ep: 32ohm, event = %d\n",
				__func__, event);
		ep_32ohm(codec, s5m3700x, event);
		break;
	}
	return 0;
}

static int linedrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				RSTB_DAC_DSM_C_MASK, RSTB_DAC_DSM_C_MASK);

		/* CP2 Zero-Cross */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_4F_HSYS_CP2_SIGN, 0x05);

		/* CP Frequency Setting */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xBB);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x07);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xBB);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x22);

		/* Class-H */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C4_CP2_TH2_32, 0x77);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C5_CP2_TH3_32, 0x7D);

		/* IDAC Setting */
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_288_IDAC3_OTP, 0x32);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_289_IDAC4_OTP, 0x15);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_28A_IDAC5_OTP, 0x04);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_14A_PD_LO, 0x80);

		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2BA_CTRL_LN1, 0x22);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2BB_CTRL_LN2, 0x63);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_LINE_MASK, APW_LINE_MASK);

		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_19_PWAUTO_DA,
				APW_LINE_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* CP Frequency Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_4F_HSYS_CP2_SIGN, 0x85);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, 0xBB);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_154_DA_CP1, 0x07);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_155_DA_CP2, 0xBB);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_156_DA_CP3, 0x22);

		/* IDAC & EP Control Setting Default */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_14A_PD_LO, 0xD0);

		/* RESETB1 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				RSTB_DAC_DSM_C_MASK, 0);
		break;
	}
	return 0;
}

/*
 * s5m3700x_dvol_adc_tlv - Digital volume for ADC
 *
 * Map as per data-sheet:
 * 0x00 ~ 0xE0 : +42dB to -70dB, step 0.5dB
 * 0xE0 ~ 0xE5 : -70dB to -80dB, step 2.0dB
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DVOL_ADCL : reg(0x34), shift(0), width(8), invert(1), max(0xE5)
 * DVOL_ADCR : reg(0x35), shift(0), width(8), invert(1), max(0xE5)
 * DVOL_ADCC : reg(0x36), shift(0), width(8), invert(1), max(0xE5)
 */
static const unsigned int s5m3700x_dvol_adc_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x00, 0x05, TLV_DB_SCALE_ITEM(-8000, 200, 0),
	0x06, 0xE5, TLV_DB_SCALE_ITEM(-6950, 50, 0),
};


static int mic1_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	s5m3700x_read(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_111_BST1, &mic_boost);

	ucontrol->value.integer.value[0] = mic_boost >> CTVOL_BST_SHIFT;
	return 0;
}

static int mic1_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_111_BST1,
			CTVOL_BST_MASK, value << CTVOL_BST_SHIFT);
	return 0;
}

static int mic2_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	s5m3700x_read(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_112_BST2, &mic_boost);

	ucontrol->value.integer.value[0] = mic_boost >> CTVOL_BST_SHIFT;
	return 0;
}

static int mic2_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_112_BST2,
			CTVOL_BST_MASK, value << CTVOL_BST_SHIFT);
	return 0;
}

static int mic3_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	s5m3700x_read(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3, &mic_boost);

	ucontrol->value.integer.value[0] = mic_boost >> CTVOL_BST_SHIFT;
	return 0;
}

static int mic3_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
			CTVOL_BST_MASK, value << CTVOL_BST_SHIFT);
	return 0;
}

static int mic4_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	s5m3700x_read(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_114_BST4, &mic_boost);

	ucontrol->value.integer.value[0] = mic_boost >> CTVOL_BST_SHIFT;
	return 0;
}

static int mic4_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_114_BST4,
			CTVOL_BST_MASK, value << CTVOL_BST_SHIFT);
	return 0;
}

/*
 * s5m3700x_dmic_gain_tlv - Digital MIC gain
 *
 * Map as per data-sheet:
 * 000 ~ 111 : 1 to 15, step 2 code
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DMIC_GAIN_PRE : reg(0x32), shift(4), width(3), invert(0), max(7)
 */
static const unsigned int s5m3700x_dmic_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x0, 0x7, TLV_DB_SCALE_ITEM(100, 200, 0),
};

/*
 * s5m3700x_dmic_st_tlv - DMIC IO strength selection
 *
 * Map as per data-sheet:
 * 0x0 ~ 0x3 : 0 to 3, step 1
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DMIC_ST : reg(0x2F), shift(2), width(2), invert(0), max(0x3)
 */
static const unsigned int s5m3700x_dmic_st_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x0, 0x3, TLV_DB_SCALE_ITEM(0, 300, 0),
};

/*
 * s5m3700x_sdout_st_tlv - SDOUT IO strength selection
 *
 * Map as per data-sheet:
 * 0x0 ~ 0x3 : 0 to 3, step 1
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * SDOUT_ST : reg(0x2F), shift(0), width(2), invert(0), max(0x3)
 */
static const unsigned int s5m3700x_sdout_st_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0x0, 0x3, TLV_DB_SCALE_ITEM(0, 300, 0),
};

static int micbias1_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int value;
	bool micbias_flag;

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM, &value);
	micbias_flag = value & APW_MCB1_MASK;
	regcache_cache_switch(s5m3700x, true);

	ucontrol->value.integer.value[0] = micbias_flag;

	return 0;
}

static int micbias1_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (s5m3700x->vts_dmic_on == VTS_DMIC1_ON) {
		dev_info(codec->dev, "%s skip to control micbias1\n", __func__);
		return 0;
	}

	regcache_cache_switch(s5m3700x, false);
	if (value)
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
				APW_MCB1_MASK, APW_MCB1_MASK);
	else
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
				APW_MCB1_MASK, 0);
	regcache_cache_switch(s5m3700x, true);

	return 0;
}

static int micbias2_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int value;
	bool micbias_flag;

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM, &value);
	micbias_flag = value & APW_MCB2_MASK;
	regcache_cache_switch(s5m3700x, true);

	ucontrol->value.integer.value[0] = micbias_flag;

	return 0;
}

static int micbias2_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	regcache_cache_switch(s5m3700x, false);

	if (value) {
		s5m3700x->hp_bias_mix_cnt++;
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
				APW_MCB2_MASK, APW_MCB2_MASK);
	} else {
		s5m3700x->hp_bias_mix_cnt--;
		if (s5m3700x->hp_bias_mix_cnt == 0) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
					APW_MCB2_MASK, 0);
		} else if (s5m3700x->hp_bias_mix_cnt < 0) {
			dev_info(codec->dev, "%s called, micbias operation is not match\n", __func__);
			s5m3700x->hp_bias_mix_cnt = 0;
		}
	}
	dev_info(codec->dev, "%s called, micbias turn %s, cnt %d\n",
			__func__, value ? "On" : "Off", s5m3700x->hp_bias_mix_cnt);

	regcache_cache_switch(s5m3700x, true);

	return 0;
}

static int micbias3_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int value;
	bool micbias_flag;

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM, &value);
	micbias_flag = value & APW_MCB3_MASK;
	regcache_cache_switch(s5m3700x, true);

	ucontrol->value.integer.value[0] = micbias_flag;

	return 0;
}

static int micbias3_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (s5m3700x->vts_dmic_on == VTS_DMIC2_ON) {
		dev_info(codec->dev, "%s skip to control micbias3\n", __func__);
		return 0;
	}

	regcache_cache_switch(s5m3700x, false);
	if (value)
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
				APW_MCB3_MASK, APW_MCB3_MASK);
	else
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
				APW_MCB3_MASK, 0);
	regcache_cache_switch(s5m3700x, true);

	return 0;
}

/*
 * s5m3700x_adc_dat_src - I2S channel input data selection
 *
 * Map as per data-sheet:
 * 00 : ADC Left Channel Data
 * 01 : ADC Right Channel Data
 * 10 : ADC Center Channel Data
 * 11 : Zero data
 *
 * SEL_ADC0 : reg(0x23), shift(0), width(2)
 * SEL_ADC1 : reg(0x23), shift(2), width(2)
 * SEL_ADC2 : reg(0x23), shift(4), width(2)
 * SEL_ADC3 : reg(0x23), shift(6), width(2)
 */
static const char * const s5m3700x_adc_dat_src[] = {
		"ADCL", "ADCR", "ADCC", "Zero"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_adc_dat_enum0, S5M3700X_23_IF_FORM4,
		SEL_ADC0_SHIFT, s5m3700x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(s5m3700x_adc_dat_enum1,	S5M3700X_23_IF_FORM4,
		SEL_ADC1_SHIFT, s5m3700x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(s5m3700x_adc_dat_enum2,	S5M3700X_23_IF_FORM4,
		SEL_ADC2_SHIFT, s5m3700x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(s5m3700x_adc_dat_enum3, S5M3700X_23_IF_FORM4,
		SEL_ADC3_SHIFT, s5m3700x_adc_dat_src);

/*
 * s5m3700x_i2s_df_format - I2S Data Format Selection
 *
 * Map as per data-sheet:
 * 000 : I2S, PCM Long format
 * 001 : Left Justified format
 * 010 : Right Justified format
 * 011 : Not used
 * 100 : I2S, PCM Short format
 * 101 ~ 111 : Not used
 * I2S_DF : reg(0x20), shift(4), width(3)
 */
static const char * const s5m3700x_i2s_df_format[] = {
	"PCM-L", "LJ", "RJ", "Zero0", "PCM-S", "Zero1", "Zero2", "Zero3"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_i2s_df_enum, S5M3700X_20_IF_FORM1,
		I2S_DF_SHIFT, s5m3700x_i2s_df_format);

/*
 * s5m3700x_i2s_bck_format - I2S BCLK Data Selection
 *
 * Map as per data-sheet:
 * 0 : Normal
 * 1 : Invert
 * I2S_DF : reg(0x20), shift(1)
 */

static const char * const s5m3700x_i2s_bck_format[] = {
	"Normal", "Invert"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_i2s_bck_enum, S5M3700X_20_IF_FORM1,
		BCK_POL_SHIFT, s5m3700x_i2s_bck_format);

/*
 * s5m3700x_i2s_lrck_format - I2S LRCLK Data Selection
 *
 * Map as per data-sheet:
 * 0 : 0&2 Low, 1&3 High
 * 1 : 1&3 Low, 0&2 High
 * I2S_DF : reg(0x20), shift(0)
 */

static const char * const s5m3700x_i2s_lrck_format[] = {
	"Normal", "Invert"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_i2s_lrck_enum, S5M3700X_20_IF_FORM1,
		LRCK_POL_SHIFT, s5m3700x_i2s_lrck_format);

static int dmic_delay_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->dmic_delay;

	return 0;
}

static int dmic_delay_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x->dmic_delay = value;

	return 0;
}

static int amic_delay_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->amic_delay;

	return 0;
}

static int amic_delay_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x->amic_delay = value;

	return 0;
}



static const char * const s5m3700x_hpf_sel_text[] = {
	"HPF-15Hz", "HPF-33Hz", "HPF-60Hz", "HPF-113Hz"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_hpf_sel_enum, S5M3700X_37_AD_HPF,
		HPF_SEL_SHIFT, s5m3700x_hpf_sel_text);

static const char * const s5m3700x_hpf_order_text[] = {
	"HPF-2nd", "HPF-1st"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_hpf_order_enum, S5M3700X_37_AD_HPF,
		HPF_ORDER_SHIFT, s5m3700x_hpf_order_text);

static const char * const s5m3700x_hpf_channel_text[] = {
	"Off", "On"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_hpf_channel_enum_l, S5M3700X_37_AD_HPF,
		HPF_ENL_SHIFT, s5m3700x_hpf_channel_text);

static SOC_ENUM_SINGLE_DECL(s5m3700x_hpf_channel_enum_r, S5M3700X_37_AD_HPF,
		HPF_ENR_SHIFT, s5m3700x_hpf_channel_text);

static SOC_ENUM_SINGLE_DECL(s5m3700x_hpf_channel_enum_c, S5M3700X_37_AD_HPF,
		HPF_ENC_SHIFT, s5m3700x_hpf_channel_text);

/*
 * s5m3700x_dvol_dac_tlv - Maximum headphone gain for EAR/RCV path
 *
 * Map as per data-sheet:
 * 0x00 ~ 0xE0 : +42dB to -70dB, step 0.5dB
 * 0xE1 ~ 0xE5 : -72dB to -80dB, step 2.0dB
 * 0xE6 : -82.4dB
 * 0xE7 ~ 0xE9 : -84.3dB to -96.3dB, step 6dB
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DVOL_DAL : reg(0x41), shift(0), width(8), invert(1), max(0xE9)
 * DVOL_DAR : reg(0x42), shift(0), width(8), invert(1), max(0xE9)
 */
static const unsigned int s5m3700x_dvol_dac_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0x01, 0x03, TLV_DB_SCALE_ITEM(-9630, 600, 0),
	0x04, 0x04, TLV_DB_SCALE_ITEM(-8240, 0, 0),
	0x05, 0x09, TLV_DB_SCALE_ITEM(-8000, 200, 0),
	0x0A, 0xE9, TLV_DB_SCALE_ITEM(-7000, 50, 0),
};

static int hp_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->hp_avc_gain;

	dev_err(s5m3700x->dev, "%s, hp_avc_gain: %d\n",
			__func__, s5m3700x->hp_avc_gain);

	return 0;
}

static int hp_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x->hp_avc_gain = value;

	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_7A_AVC43, 0x00);

	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_5F_AVC16,
			SIGN_MASK, 1 << SIGN_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_61_AVC18,
			SIGN_MASK, 1 << SIGN_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_63_AVC20,
			SIGN_MASK, 1 << SIGN_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_5F_AVC16,
			AVC_AVOL_MASK, value);
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_61_AVC18,
			AVC_AVOL_MASK, value);
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_63_AVC20,
			AVC_AVOL_MASK, value);
	return 0;
}

static int rcv_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->rcv_avc_gain;

	dev_err(s5m3700x->dev, "%s, rcv_avc_gain: %d\n",
			__func__, s5m3700x->rcv_avc_gain);

	return 0;
}

static int rcv_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x->rcv_avc_gain = value;

	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_7A_AVC43, 0x00);

	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_5F_AVC16,
			SIGN_MASK, 1 << SIGN_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_61_AVC18,
			SIGN_MASK, 1 << SIGN_SHIFT);
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_63_AVC20,
			SIGN_MASK, 1 << SIGN_SHIFT);

	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_5F_AVC16,
			AVC_AVOL_MASK, value);
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_61_AVC18,
			AVC_AVOL_MASK, value);
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_63_AVC20,
			AVC_AVOL_MASK, value);
	return 0;
}

/*
 * s5m3700x_dac_dat_src - I2S channel input data selection
 *
 * Map as per data-sheet:
 * 00 : DAC I2S Channel 0
 * 01 : DAC I2S Channel 1
 * 10 : DAC I2S Channel 2
 * 11 : DAC I2S Channel 3
 *
 * SEL_DAC0 : reg(0x24), shift(0), width(2)
 * SEL_DAC1 : reg(0x23), shift(4), width(2)
 */
static const char * const s5m3700x_dac_dat_src[] = {
		"CH0", "CH1", "CH2", "CH3"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_dac_dat_enum0, S5M3700X_24_IF_FORM5,
		SEL_DAC0_SHIFT, s5m3700x_dac_dat_src);

static SOC_ENUM_SINGLE_DECL(s5m3700x_dac_dat_enum1, S5M3700X_24_IF_FORM5,
		SEL_DAC1_SHIFT, s5m3700x_dac_dat_src);

/*
 * s5m3700x_dac_mixl_mode_text - DACL Mixer Selection
 *
 * Map as per data-sheet:
 * 000 : Data L
 * 001 : (L+R)/2 Mono
 * 010 : (L+R) Mono
 * 011 : (L+R)/2 Polarity Changed
 * 100 : (L+R) Polarity Changed
 * 101 : Zero Padding
 * 110 : Data L Polarity Changed
 * 111 : Data R Polarity Changed
 *
 * DAC_MIXL : reg(0x44), shift(4), width(3)
 */
static const char * const s5m3700x_dac_mixl_mode_text[] = {
		"Data L", "LR/2 Mono", "LR Mono", "LR/2 PolCh",
		"LR PolCh", "Zero", "Data L PolCh", "Data R PolCh"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_dac_mixl_mode_enum, S5M3700X_44_PLAY_MIX0,
		DAC_MIXL_SHIFT, s5m3700x_dac_mixl_mode_text);

/*
 * s5m3700x_dac_mixr_mode_text - DACR Mixer Selection
 *
 * Map as per data-sheet:
 * 000 : Data R
 * 001 : (L+R)/2 Mono
 * 010 : (L+R) Mono
 * 011 : (L+R)/2 Polarity Changed
 * 100 : (L+R) Polarity Changed
 * 101 : Zero Padding
 * 110 : Data R Polarity Changed
 * 111 : Data L Polarity Changed
 *
 * DAC_MIXR : reg(0x44), shift(0), width(3)
 */
static const char * const s5m3700x_dac_mixr_mode_text[] = {
		"Data R", "LR/2 Mono", "LR Mono", "LR/2 PolCh",
		"LR PolCh", "Zero", "Data R PolCh", "Data L PolCh"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_dac_mixr_mode_enum, S5M3700X_44_PLAY_MIX0,
		DAC_MIXR_SHIFT, s5m3700x_dac_mixr_mode_text);

/*
 * s5m3700x_lnrcv_mix_mode_text - LINE/RCV Mixer Selection
 *
 * Map as per data-sheet:
 * 00 : Data L
 * 01 : Data R
 * 10 : L/2 + R/2
 * 11 : Zero
 *
 * LINE/RCV MIX : reg(0x45), shift(3), width(2)
 */
static const char * const s5m3700x_lnrcv_mix_mode_text[] = {
		"Data L", "Data R", "LR/2", "Zero",
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_lnrcv_mix_mode_enum, S5M3700X_45_PLAY_MIX1,
		LN_RCV_MIX_SHIFT, s5m3700x_lnrcv_mix_mode_text);

static int jack_pole_cnt_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->p_jackdet->t_pole_cnt;

	return 0;
}

static int jack_pole_cnt_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x->p_jackdet->t_pole_cnt = value;

	return 0;
}

static int jack_pole_delay_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->p_jackdet->t_pole_delay;

	return 0;
}

static int jack_pole_delay_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x->p_jackdet->t_pole_delay = value;

	return 0;
}

static int jack_btn_cnt_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->p_jackdet->t_btn_cnt;

	return 0;
}

static int jack_btn_cnt_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x->p_jackdet->t_btn_cnt = value;

	return 0;
}

static int jack_btn_delay_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->p_jackdet->t_btn_delay;

	return 0;
}

static int jack_btn_delay_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3700x->p_jackdet->t_btn_delay = value;

	return 0;
}

static int ant_mdet1_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int value;

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_read(s5m3700x, S5M3700X_OTP_ADDR, 0xDD, &value);
	regcache_cache_switch(s5m3700x, true);

	ucontrol->value.integer.value[0] = (value & INP_SEL_R_MASK) >> INP_SEL_R_SHIFT;

	return 0;
}

static int ant_mdet1_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_update_bits(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_DD_DCTR_DBNC6,
			INP_SEL_R_MASK, value << INP_SEL_R_SHIFT);
	regcache_cache_switch(s5m3700x, true);

	return 0;
}

static int ant_mdet2_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int value;

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_read(s5m3700x, S5M3700X_OTP_ADDR, 0xDD, &value);
	regcache_cache_switch(s5m3700x, true);

	ucontrol->value.integer.value[0] = (value & INP_SEL_L_MASK) >> INP_SEL_L_SHIFT;

	return 0;
}

static int ant_mdet2_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_update_bits(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_DD_DCTR_DBNC6,
			INP_SEL_L_MASK, value << INP_SEL_L_SHIFT);
	regcache_cache_switch(s5m3700x, true);

	return 0;
}


#if IS_ENABLED(CONFIG_SND_SOC_S5M3700X_VTS)
void s5m3700x_vts(struct s5m3700x_priv *s5m3700x, unsigned int type)
{
	struct snd_soc_component *codec = s5m3700x->codec;
	int chop_val, dac_on, adc_on, mic1_on, mic2_on, mic3_on, mic4_on, hifi_on;

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);
	dac_on = chop_val & DAC_ON_MASK;
	adc_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);
	mic1_on = chop_val & MIC1_ON_MASK;
	mic2_on = chop_val & MIC2_ON_MASK;
	mic3_on = chop_val & MIC3_ON_MASK;
	mic4_on = chop_val & MIC4_ON_MASK;
	hifi_on = s5m3700x->hifi_on;

	switch (type) {
	case VTS_OFF:
		dev_info(codec->dev, "%s called. VTS Off.\n", __func__);

		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_BA_ODSEL11, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_122_BST4PN, 0x00);

		/* VTS1 & VTS2 APW OFF */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_VTS1_MASK | APW_VTS2_MASK, 0);

		/* VTS High-Z Setting */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_2F_DMIC_ST,
				VTS_DATA_ENB_MASK, VTS_DATA_ENB_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				VTS_RESETB_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
				VTS_CLK_GATE_MASK, 0);

		if ((!dac_on) && (!adc_on)) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
					SEQ_CLK_GATE_MASK, 0);

			if (!hifi_on)
				s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
						COM_CLK_GATE_MASK, 0);
		}

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_109_CTRL_VTS1,
				CTMI_VTS_INT1_MASK | CTMI_VTS_INT2_MASK,
				CTMI_VTS_2 << CTMI_VTS_INT1_SHIFT |
				CTMI_VTS_2 << CTMI_VTS_INT2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_10A_CTRL_VTS2,
				CTMI_VTS_INT3_MASK | CTMI_VTS_INT4_MASK,
				CTMI_VTS_2 << CTMI_VTS_INT3_SHIFT |
				CTMI_VTS_2 << CTMI_VTS_INT4_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_11E_CTRL_MIC_I1,
				CTMI_MIC_BST_MASK | CTMI_MIC_INT1_MASK,
				MIC_VAL_0 << CTMI_MIC_BST_SHIFT |
				MIC_VAL_2 << CTMI_MIC_INT1_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_10B_CTRL_VTS3,
				CTMF_VTS_LPF_CH1_MASK | CTMF_VTS_LPF_CH2_MASK,
				VTS_LPF_72K << CTMF_VTS_LPF_CH1_SHIFT |
				VTS_LPF_72K << CTMF_VTS_LPF_CH2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_111_BST1,
				EN_BST1_LPM_MASK | EN_BST2_LPM_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_111_BST1,
				CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
				CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
				EN_ISIBLKC_MASK, EN_ISIBLKC_MASK);

		if ((!mic1_on) && (!mic2_on)) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
					CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
					CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
					CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);

			s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
					CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
					CTMR_BSTR_1 << CTMR_BSTR1_SHIFT |
					CTMR_BSTR_1 << CTMR_BSTR2_SHIFT);
		}

		if ((!mic2_on) && (!mic3_on)) {
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_BA_ODSEL11, 0x00);
			s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_122_BST4PN, 0x00);
		}

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_BST3_MASK, 0);

		if (!adc_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
					SEL_CLK_LCP_MASK | SEL_BST_IN_VCM_MASK, 0);

		/* MCB3 OFF */
		if (!mic3_on) {
			/* MCB3 OFF */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
					APW_MCB3_MASK, 0);

			/* Setting Mic3 Bias Voltage Default Value */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_C9_ACTR_MCB5,
					CTRV_MCB3_MASK, s5m3700x->p_jackdet->mic_bias3_voltage << CTRV_MCB3_SHIFT);
		}

		if ((!mic1_on) && (!mic2_on) && (!mic3_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
					SEL_BST2_MASK, 0);

		if (!mic4_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
					SEL_BST1_MASK, 0);

		break;
	case VTS1_ON:
		dev_info(codec->dev, "%s called. VTS1 On.\n", __func__);
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
				SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK,
				SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
				VTS_CLK_GATE_MASK, VTS_CLK_GATE_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				VTS_RESETB_MASK, VTS_RESETB_MASK);

		/* VTS Output Setting */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_2F_DMIC_ST,
				VTS_DATA_ENB_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_111_BST1,
				EN_BST1_LPM_MASK, EN_BST1_LPM_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_0 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
				CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_0 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
				EN_ISIBLKC_MASK, EN_ISIBLKC_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_109_CTRL_VTS1,
				CTMI_VTS_INT1_MASK | CTMI_VTS_INT2_MASK,
				CTMI_VTS_2 << CTMI_VTS_INT1_SHIFT |
				CTMI_VTS_0 << CTMI_VTS_INT2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_10A_CTRL_VTS2,
				CTMI_VTS_INT3_MASK | CTMI_VTS_INT4_MASK,
				CTMI_VTS_0 << CTMI_VTS_INT3_SHIFT |
				CTMI_VTS_0 << CTMI_VTS_INT4_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_11E_CTRL_MIC_I1,
				CTMI_MIC_BST_MASK | CTMI_MIC_INT1_MASK,
				MIC_VAL_0 << CTMI_MIC_BST_SHIFT |
				MIC_VAL_2 << CTMI_MIC_INT1_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_10B_CTRL_VTS3,
				CTMF_VTS_LPF_CH1_MASK | CTMF_VTS_LPF_CH2_MASK,
				VTS_LPF_12K << CTMF_VTS_LPF_CH1_SHIFT |
				VTS_LPF_12K << CTMF_VTS_LPF_CH2_SHIFT);

		/* Setting Mic3 Bias Voltage Default Value */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_C9_ACTR_MCB5,
				CTRV_MCB3_MASK, MICBIAS_VO_1_8V << CTRV_MCB3_SHIFT);

		/* MCB3 ON */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
				APW_MCB3_MASK, APW_MCB3_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_CLK_LCP_MASK | SEL_BST2_MASK |
				SEL_BST3_MASK | SEL_BST_IN_VCM_MASK,
				SEL_CLK_LCP_MASK | SEL_BST2_MASK |
				SEL_BST3_MASK | SEL_BST_IN_VCM_MASK);

		/* VTS1 APW ON */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_VTS1_MASK, APW_VTS1_MASK);
		msleep(60);
		break;
	case VTS2_ON:
		dev_info(codec->dev, "%s called. VTS2 On.\n", __func__);
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
				SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK,
				SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
				VTS_CLK_GATE_MASK, VTS_CLK_GATE_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				VTS_RESETB_MASK, VTS_RESETB_MASK);

		/* VTS Output Setting */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_2F_DMIC_ST,
				VTS_DATA_ENB_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_111_BST1,
				EN_BST2_LPM_MASK, EN_BST2_LPM_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_111_BST1,
				CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_0 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
				CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_0 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
				EN_ISIBLKC_MASK, EN_ISIBLKC_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_109_CTRL_VTS1,
				CTMI_VTS_INT1_MASK | CTMI_VTS_INT2_MASK,
				CTMI_VTS_2 << CTMI_VTS_INT1_SHIFT |
				CTMI_VTS_0 << CTMI_VTS_INT2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_10A_CTRL_VTS2,
				CTMI_VTS_INT3_MASK | CTMI_VTS_INT4_MASK,
				CTMI_VTS_0 << CTMI_VTS_INT3_SHIFT |
				CTMI_VTS_0 << CTMI_VTS_INT4_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_11E_CTRL_MIC_I1,
				CTMI_MIC_BST_MASK | CTMI_MIC_INT1_MASK,
				MIC_VAL_0 << CTMI_MIC_BST_SHIFT |
				MIC_VAL_2 << CTMI_MIC_INT1_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_10B_CTRL_VTS3,
				CTMF_VTS_LPF_CH1_MASK | CTMF_VTS_LPF_CH2_MASK,
				VTS_LPF_12K << CTMF_VTS_LPF_CH1_SHIFT |
				VTS_LPF_12K << CTMF_VTS_LPF_CH2_SHIFT);

		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_BA_ODSEL11, 0x02);
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_122_BST4PN, 0x80);

		/* Setting Mic3 Bias Voltage Default Value */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_C9_ACTR_MCB5,
				CTRV_MCB3_MASK, MICBIAS_VO_1_8V << CTRV_MCB3_SHIFT);

		/* MCB3 ON */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
				APW_MCB3_MASK, APW_MCB3_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_CLK_LCP_MASK | SEL_BST2_MASK |
				SEL_BST3_MASK | SEL_BST_IN_VCM_MASK,
				SEL_CLK_LCP_MASK | SEL_BST2_MASK |
				SEL_BST3_MASK | SEL_BST_IN_VCM_MASK);

		/* VTS2 APW ON */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_VTS2_MASK, APW_VTS2_MASK);
		msleep(60);
		break;
	case VTS_DUAL:
		dev_info(codec->dev, "%s called. VTS DUAL On.\n", __func__);
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
				SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK,
				SEQ_CLK_GATE_MASK | COM_CLK_GATE_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_12_CLKGATE2,
				VTS_CLK_GATE_MASK, VTS_CLK_GATE_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_15_RESETB1,
				VTS_RESETB_MASK, VTS_RESETB_MASK);

		/* VTS Output Setting */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_2F_DMIC_ST,
				VTS_DATA_ENB_MASK, 0);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_111_BST1,
				EN_BST1_LPM_MASK | EN_BST2_LPM_MASK,
				EN_BST1_LPM_MASK | EN_BST2_LPM_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_113_BST3,
				CTVOL_BST_MASK, 0x1 << CTVOL_BST_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
				CTMR_BSTR1_MASK | CTMR_BSTR2_MASK,
				CTMR_BSTR_3 << CTMR_BSTR1_SHIFT |
				CTMR_BSTR_3 << CTMR_BSTR2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_117_DSM2,
				EN_ISIBLKC_MASK, EN_ISIBLKC_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_109_CTRL_VTS1,
				CTMI_VTS_INT1_MASK | CTMI_VTS_INT2_MASK,
				CTMI_VTS_2 << CTMI_VTS_INT1_SHIFT |
				CTMI_VTS_0 << CTMI_VTS_INT2_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_10A_CTRL_VTS2,
				CTMI_VTS_INT3_MASK | CTMI_VTS_INT4_MASK,
				CTMI_VTS_0 << CTMI_VTS_INT3_SHIFT |
				CTMI_VTS_0 << CTMI_VTS_INT4_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_11E_CTRL_MIC_I1,
				CTMI_MIC_BST_MASK | CTMI_MIC_INT1_MASK,
				MIC_VAL_0 << CTMI_MIC_BST_SHIFT |
				MIC_VAL_2 << CTMI_MIC_INT1_SHIFT);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_10B_CTRL_VTS3,
				CTMF_VTS_LPF_CH1_MASK | CTMF_VTS_LPF_CH2_MASK,
				VTS_LPF_12K << CTMF_VTS_LPF_CH1_SHIFT |
				VTS_LPF_12K << CTMF_VTS_LPF_CH2_SHIFT);

		/* Setting Mic3 Bias Voltage Default Value */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_C9_ACTR_MCB5,
				CTRV_MCB3_MASK, MICBIAS_VO_1_8V << CTRV_MCB3_SHIFT);

		/* MCB3 ON */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
				APW_MCB3_MASK, APW_MCB3_MASK);

		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_125_RSVD,
				SEL_CLK_LCP_MASK | SEL_BST2_MASK |
				SEL_BST3_MASK | SEL_BST_IN_VCM_MASK,
				SEL_CLK_LCP_MASK | SEL_BST2_MASK |
				SEL_BST3_MASK | SEL_BST_IN_VCM_MASK);

		/* VTS1 & VTS2 APW ON */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_18_PWAUTO_AD,
				APW_VTS1_MASK | APW_VTS2_MASK,
				APW_VTS1_MASK | APW_VTS2_MASK);
		msleep(60);
		break;
	}
	regcache_cache_switch(s5m3700x, true);
}

static int vts_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->vts1_on;

	return 0;
}

static int vts_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	switch (value) {
	case VTS_OFF:
		s5m3700x->vts1_on = false;
		s5m3700x->vts2_on = false;
		break;
	case VTS1_ON:
		s5m3700x->vts1_on = true;
		s5m3700x->vts2_on = false;
		break;
	case VTS2_ON:
		s5m3700x->vts1_on = false;
		s5m3700x->vts2_on = true;
		break;
	case VTS_DUAL:
		s5m3700x->vts1_on = true;
		s5m3700x->vts2_on = true;
		break;
	}

	s5m3700x_vts(s5m3700x, value);

	dev_info(codec->dev, "%s: VTS: %d\n", __func__, value);

	return 0;
}

void s5m3700x_vts_dmic(struct s5m3700x_priv *s5m3700x, unsigned int type)
{
	struct snd_soc_component *codec = s5m3700x->codec;
	unsigned int chop_val = 0;

	regcache_cache_switch(s5m3700x, false);

	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);

	switch (type) {
	case VTS_DMIC1_ON:
		if (s5m3700x->vts_dmic_on == VTS_DMIC_OFF) {
			dev_info(codec->dev, "%s called VTS_DMIC1_ON.\n", __func__);
			if (!(chop_val & DMIC1_ON_MASK))
				/* Enable MicBias1 */
				s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
						APW_MCB1_MASK, APW_MCB1_MASK);
			s5m3700x->vts_dmic_on = VTS_DMIC1_ON;
		} else {
			dev_info(codec->dev, "%s prev state should be VTS_DMIC_OFF.\n", __func__);
		}
		break;
	case VTS_DMIC2_ON:
		if (s5m3700x->vts_dmic_on == VTS_DMIC_OFF) {
			dev_info(codec->dev, "%s called VTS_DMIC2_ON.\n", __func__);
			if (!(chop_val & DMIC2_ON_MASK))
				/* Enable MicBias3 */
				s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
						APW_MCB3_MASK, APW_MCB3_MASK);
			s5m3700x->vts_dmic_on = VTS_DMIC2_ON;
		} else {
			dev_info(codec->dev, "%s prev state should be VTS_DMIC_OFF.\n", __func__);
		}
		break;
	case VTS_DMIC_OFF:
		if (s5m3700x->vts_dmic_on == VTS_DMIC1_ON) {
			dev_info(codec->dev, "%s called VTS_DMIC_OFF. prev %d\n", __func__, s5m3700x->vts_dmic_on);
			if (!(chop_val & DMIC1_ON_MASK))
				/* Disable MicBias1 */
				s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
						APW_MCB1_MASK, 0);
			s5m3700x->vts_dmic_on = VTS_DMIC_OFF;
		} else if (s5m3700x->vts_dmic_on == VTS_DMIC2_ON) {
			dev_info(codec->dev, "%s called VTS_DMIC_OFF. prev %d\n", __func__, s5m3700x->vts_dmic_on);
			if (!(chop_val & DMIC2_ON_MASK))
				/* Disable MicBias3 */
				s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM,
						APW_MCB3_MASK, 0);
			s5m3700x->vts_dmic_on = VTS_DMIC_OFF;
		}
		break;
	}
	regcache_cache_switch(s5m3700x, true);
}


static int vts_dmic_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->vts_dmic_on;

	return 0;
}

static int vts_dmic_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	int value = ucontrol->value.integer.value[0];

	if ((value < VTS_DMIC_OFF) || (value > VTS_DMIC2_ON)) {
		dev_err(codec->dev, "%s: unsupported value for dmic vts %d\n", __func__, value);
		return 0;
	}

	s5m3700x_vts_dmic(s5m3700x, value);

	dev_info(codec->dev, "%s: DMIC VTS: %d\n", __func__, s5m3700x->vts_dmic_on);

	return 0;
}

static int vts1_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_read(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_111_BST1, &mic_boost);
	regcache_cache_switch(s5m3700x, true);

	ucontrol->value.integer.value[0] = mic_boost >> CTVOL_BST_SHIFT;
	return 0;
}

static int vts1_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_111_BST1,
			CTVOL_BST_MASK, value << CTVOL_BST_SHIFT);
	regcache_cache_switch(s5m3700x, true);

	return 0;
}

static int vts2_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_read(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_112_BST2, &mic_boost);
	regcache_cache_switch(s5m3700x, true);

	ucontrol->value.integer.value[0] = mic_boost >> CTVOL_BST_SHIFT;
	return 0;
}

static int vts2_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_112_BST2,
			CTVOL_BST_MASK, value << CTVOL_BST_SHIFT);
	regcache_cache_switch(s5m3700x, true);

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_SND_SOC_S5M3700X_HIFI)
void s5m3700x_hifi(struct s5m3700x_priv *s5m3700x, int type)
{
	struct snd_soc_component *codec = s5m3700x->codec;
	int chop_val, dac_on, adc_on, vts_on;

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &chop_val);
	dac_on = chop_val & DAC_ON_MASK;
	adc_on = chop_val & (DMIC_ON_MASK | AMIC_ON_MASK);
	vts_on = (s5m3700x->vts1_on | s5m3700x->vts2_on);

	switch (type) {
	case HIFI_OFF:
		dev_info(codec->dev, "%s called. HIFI DAC Off.\n", __func__);

		/* CP1, CP2 CLK Manual Setting OFF */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0,
				SEL_AUTO_CP1_MASK | SEL_AUTO_CP2_MASK,
				SEL_AUTO_CP1_MASK | SEL_AUTO_CP2_MASK);

		/* CTRL CP1 & CP2 Disable */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_104_CTRL_CP, 0x00);

		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C4_CP2_TH2_32, 0x7E);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C5_CP2_TH3_32, 0x82);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C7_CP2_TH2_16, 0x79);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C8_CP2_TH3_16, 0x80);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2CA_CP2_TH2_06, 0x7E);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2CB_CP2_TH2_06, 0x82);

		/* CP2 Zero-Cross */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_4F_HSYS_CP2_SIGN, 0x85);

		/* RESETB0 */
		if (!dac_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					AVC_RESETB_MASK | DAC_RESETB_MASK, 0);

		if ((!dac_on) && (!adc_on) && (!vts_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
					CORE_RESETB_MASK, 0);

		/* CLKGATE0 */
		if (!dac_on)
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
					AVC_CLK_GATE_MASK | DAC_CLK_GATE_MASK, 0);

		if ((!dac_on) && (!adc_on) && (!vts_on))
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
					COM_CLK_GATE_MASK, 0);
		break;
	case HIFI_ON:
		dev_info(codec->dev, "%s called. HIFI DAC On.\n", __func__);

		/* CLKGATE0 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_10_CLKGATE0,
				AVC_CLK_GATE_MASK | DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK,
				AVC_CLK_GATE_MASK | DAC_CLK_GATE_MASK | COM_CLK_GATE_MASK);

		/* RESETB0 */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_14_RESETB0,
				AVC_RESETB_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK,
				AVC_RESETB_MASK | DAC_RESETB_MASK | CORE_RESETB_MASK);

		/* Default: 0x99, Don't off it */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_5C_AVC13,
				DMUTE_MASKB_MASK, DMUTE_MASKB_MASK);

		/* CP2 Zero-Cross */
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_4F_HSYS_CP2_SIGN, 0x05);

		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C4_CP2_TH2_32, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C5_CP2_TH3_32, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C7_CP2_TH2_16, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2C8_CP2_TH3_16, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2CA_CP2_TH2_06, 0x00);
		s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2CB_CP2_TH2_06, 0x00);

		/* CP1, CP2 CLK Manual Setting ON */
		s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0,
				SEL_AUTO_CP1_MASK | SEL_AUTO_CP2_MASK, 0);

		/* CTRL CP1 & CP2 Enable */
		s5m3700x_write(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_104_CTRL_CP, 0x2A);
		break;
	}

	regcache_cache_switch(s5m3700x, true);
}

static int hifi_dac_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->hifi_on;
	return 0;
}

static int hifi_dac_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	switch (value) {
	case HIFI_OFF:
		s5m3700x->hifi_on = false;
		break;
	case HIFI_ON:
		s5m3700x->hifi_on = true;
		break;
	}

	s5m3700x_hifi(s5m3700x, value);

	dev_info(codec->dev, "%s: HIFI: %d\n", __func__, value);
	return 0;
}

static int hifi_clk_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int clk_value;

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_read(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0, &clk_value);
	regcache_cache_switch(s5m3700x, true);

	ucontrol->value.integer.value[0] = clk_value & CTMF_CP_CLK2_MASK;
	return 0;
}

static int hifi_clk_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int clk_value = ucontrol->value.integer.value[0];

	regcache_cache_switch(s5m3700x, false);
	s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_153_DA_CP0,
			CTMF_CP_CLK2_MASK, clk_value << CTMF_CP_CLK2_SHIFT);
	regcache_cache_switch(s5m3700x, true);
	return 0;
}
#endif

static int codec_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = !(s5m3700x->is_suspend);

	return 0;
}

static int codec_enable_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	int value = ucontrol->value.integer.value[0];

	if (value)
		s5m3700x_enable(codec->dev);
	else
		s5m3700x_disable(codec->dev);

	dev_info(codec->dev, "%s: codec enable : %s\n",
			__func__, (value) ? "On" : "Off");

	return 0;
}

static void codec_regdump(struct snd_soc_component *codec)
{
	u8 reg_d[S5M3700X_REGCACHE_SYNC_END] = {0,};
	u8 reg_a[S5M3700X_REGCACHE_SYNC_END] = {0,};
	u8 reg_o[S5M3700X_REGCACHE_SYNC_END] = {0,};
	u8 reg_p[S5M3700X_REGCACHE_SYNC_END] = {0,};
	u8 p_reg, line, p_line;

	for (p_reg = 0; p_reg < S5M3700X_REGCACHE_SYNC_END; p_reg++) {
		if (read_from_cache(codec->dev, p_reg, S5M3700D))
			s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, p_reg, &reg_d[p_reg]);

		if (read_from_cache(codec->dev, p_reg, S5M3700A))
			s5m3700x_i2c_read_reg(S5M3700X_ANALOG_ADDR, p_reg, &reg_a[p_reg]);

		if (read_from_cache(codec->dev, p_reg, S5M3700O))
			s5m3700x_i2c_read_reg(S5M3700X_OTP_ADDR, p_reg, &reg_o[p_reg]);

		s5m3700x_i2c_read_reg(S5M3700X_MV_ADDR, p_reg, &reg_p[p_reg]);
	}

	dev_info(codec->dev, "========== Codec Digital Block(0x0) Dump ==========\n");
	dev_info(codec->dev, "    00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n");
	for (line = 0; line <= 0xf; line++) {
		p_line = line << 4;
		dev_info(codec->dev,
				"%02x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				p_line,	reg_d[p_line + 0], reg_d[p_line + 1], reg_d[p_line + 2],
				reg_d[p_line + 3], reg_d[p_line + 4], reg_d[p_line + 5],
				reg_d[p_line + 6], reg_d[p_line + 7], reg_d[p_line + 8],
				reg_d[p_line + 9], reg_d[p_line + 10], reg_d[p_line + 11],
				reg_d[p_line + 12], reg_d[p_line + 13], reg_d[p_line + 14],
				reg_d[p_line + 15]);
	}

	dev_info(codec->dev, "========== Codec Analog Block(0x1) Dump ==========\n");
	dev_info(codec->dev, "    00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n");
	for (line = 0; line <= 0xf; line++) {
		p_line = line << 4;
		dev_info(codec->dev,
				"%02x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				p_line,	reg_a[p_line + 0], reg_a[p_line + 1], reg_a[p_line + 2],
				reg_a[p_line + 3], reg_a[p_line + 4], reg_a[p_line + 5],
				reg_a[p_line + 6], reg_a[p_line + 7], reg_a[p_line + 8],
				reg_a[p_line + 9], reg_a[p_line + 10], reg_a[p_line + 11],
				reg_a[p_line + 12], reg_a[p_line + 13], reg_a[p_line + 14],
				reg_a[p_line + 15]);
	}

	dev_info(codec->dev, "========== Codec OTP Block(0x2) Dump ==========\n");
	dev_info(codec->dev, "    00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n");
	for (line = 0; line <= 0xf; line++) {
		p_line = line << 4;
		dev_info(codec->dev,
				"%02x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				p_line,	reg_o[p_line + 0], reg_o[p_line + 1], reg_o[p_line + 2],
				reg_o[p_line + 3], reg_o[p_line + 4], reg_o[p_line + 5],
				reg_o[p_line + 6], reg_o[p_line + 7], reg_o[p_line + 8],
				reg_o[p_line + 9], reg_o[p_line + 10], reg_o[p_line + 11],
				reg_o[p_line + 12], reg_o[p_line + 13], reg_o[p_line + 14],
				reg_o[p_line + 15]);
	}

	dev_info(codec->dev, "========== Codec MV Block(0x3) Dump ==========\n");
	dev_info(codec->dev, "    00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n");
	for (line = 0; line <= 0xf; line++) {
		p_line = line << 4;
		dev_info(codec->dev,
				"%02x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				p_line,	reg_p[p_line + 0], reg_p[p_line + 1], reg_p[p_line + 2],
				reg_p[p_line + 3], reg_p[p_line + 4], reg_p[p_line + 5],
				reg_p[p_line + 6], reg_p[p_line + 7], reg_p[p_line + 8],
				reg_p[p_line + 9], reg_p[p_line + 10], reg_p[p_line + 11],
				reg_p[p_line + 12], reg_p[p_line + 13], reg_p[p_line + 14],
				reg_p[p_line + 15]);
	}
}

static int codec_regdump_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3700x->codec_debug;

	return 0;
}

static int codec_regdump_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value) {
		s5m3700x->codec_debug = true;
		codec_regdump(codec);
	} else {
		s5m3700x->codec_debug = false;
	}

	return 0;
}


/*
 * struct snd_kcontrol_new s5m3700x_snd_control
 *
 * Every distinct bit-fields within the CODEC SFR range may be considered
 * as a control elements. Such control elements are defined here.
 *
 * Depending on the access mode of these registers, different macros are
 * used to define these control elements.
 *
 * SOC_ENUM: 1-to-1 mapping between bit-field value and provided text
 * SOC_SINGLE: Single register, value is a number
 * SOC_SINGLE_TLV: Single register, value corresponds to a TLV scale
 * SOC_SINGLE_TLV_EXT: Above + custom get/set operation for this value
 * SOC_SINGLE_RANGE_TLV: Register value is an offset from minimum value
 * SOC_DOUBLE: Two bit-fields are updated in a single register
 * SOC_DOUBLE_R: Two bit-fields in 2 different registers are updated
 */

/*
 * All the data goes into s5m3700x_snd_controls.
 * All path inter-connections goes into s5m3700x_dapm_routes
 */
static const struct snd_kcontrol_new s5m3700x_snd_controls[] = {
	/*
	 * ADC(Tx) path control
	 */
	SOC_SINGLE_TLV("ADC Left Gain", S5M3700X_34_AD_VOLL,
			DVOL_ADC_SHIFT,
			ADC_DVOL_MAXNUM, 1, s5m3700x_dvol_adc_tlv),

	SOC_SINGLE_TLV("ADC Right Gain", S5M3700X_35_AD_VOLR,
			DVOL_ADC_SHIFT,
			ADC_DVOL_MAXNUM, 1, s5m3700x_dvol_adc_tlv),

	SOC_SINGLE_TLV("ADC Center Gain", S5M3700X_36_AD_VOLC,
			DVOL_ADC_SHIFT,
			ADC_DVOL_MAXNUM, 1, s5m3700x_dvol_adc_tlv),

	SOC_SINGLE_EXT("MIC1 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			mic1_boost_get, mic1_boost_put),

	SOC_SINGLE_EXT("MIC2 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			mic2_boost_get, mic2_boost_put),

	SOC_SINGLE_EXT("MIC3 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			mic3_boost_get, mic3_boost_put),

	SOC_SINGLE_EXT("MIC4 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			mic4_boost_get, mic4_boost_put),

	SOC_SINGLE_TLV("DMIC Gain", S5M3700X_32_ADC3,
			DMIC_GAIN_PRE_SHIFT,
			DMIC_GAIN_15, 0, s5m3700x_dmic_gain_tlv),

	SOC_SINGLE_TLV("DMIC ST", S5M3700X_2F_DMIC_ST,
			DMIC_ST_SHIFT,
			DMIC_IO_16mA, 0, s5m3700x_dmic_st_tlv),

	SOC_SINGLE_TLV("SDOUT ST", S5M3700X_2F_DMIC_ST,
			SDOUT_ST_SHIFT,
			SDOUT_IO_16mA, 0, s5m3700x_sdout_st_tlv),

	SOC_SINGLE_EXT("MIC BIAS1", SND_SOC_NOPM, 0, 1, 0,
			micbias1_get, micbias1_put),

	SOC_SINGLE_EXT("MIC BIAS2", SND_SOC_NOPM, 0, 1, 0,
			micbias2_get, micbias2_put),

	SOC_SINGLE_EXT("MIC BIAS3", SND_SOC_NOPM, 0, 1, 0,
			micbias3_get, micbias3_put),

	SOC_ENUM("ADC DAT Mux0", s5m3700x_adc_dat_enum0),

	SOC_ENUM("ADC DAT Mux1", s5m3700x_adc_dat_enum1),

	SOC_ENUM("ADC DAT Mux2", s5m3700x_adc_dat_enum2),

	SOC_ENUM("ADC DAT Mux3", s5m3700x_adc_dat_enum3),

	SOC_ENUM("I2S DF", s5m3700x_i2s_df_enum),

	SOC_ENUM("I2S BCLK POL", s5m3700x_i2s_bck_enum),

	SOC_ENUM("I2S LRCK POL", s5m3700x_i2s_lrck_enum),

	SOC_SINGLE_EXT("DMIC delay", SND_SOC_NOPM, 0, 1000, 0,
			dmic_delay_get, dmic_delay_put),

	SOC_SINGLE_EXT("AMIC delay", SND_SOC_NOPM, 0, 1000, 0,
			amic_delay_get, amic_delay_put),

	/*
	 * HPF Tuning Control
	 */
	SOC_ENUM("HPF Tuning", s5m3700x_hpf_sel_enum),

	SOC_ENUM("HPF Order", s5m3700x_hpf_order_enum),

	SOC_ENUM("HPF Left", s5m3700x_hpf_channel_enum_l),

	SOC_ENUM("HPF Right", s5m3700x_hpf_channel_enum_r),

	SOC_ENUM("HPF Center", s5m3700x_hpf_channel_enum_c),

	/*
	 * DAC(Rx) path control
	 */
	SOC_DOUBLE_R_TLV("DAC Gain", S5M3700X_41_PLAY_VOLL, S5M3700X_42_PLAY_VOLR,
			DVOL_DA_SHIFT,
			DAC_DVOL_MINNUM, 1, s5m3700x_dvol_dac_tlv),

	SOC_SINGLE_EXT("HP AVC Gain", SND_SOC_NOPM, 0, 24, 0,
			hp_gain_get, hp_gain_put),

	SOC_SINGLE_EXT("RCV AVC Gain", SND_SOC_NOPM, 0, 20, 0,
			rcv_gain_get, rcv_gain_put),

	SOC_ENUM("DAC DAT Mux0", s5m3700x_dac_dat_enum0),

	SOC_ENUM("DAC DAT Mux1", s5m3700x_dac_dat_enum1),

	SOC_ENUM("DAC MIXL Mode", s5m3700x_dac_mixl_mode_enum),

	SOC_ENUM("DAC MIXR Mode", s5m3700x_dac_mixr_mode_enum),

	SOC_ENUM("LNRCV MIX Mode", s5m3700x_lnrcv_mix_mode_enum),

	/* Jack control */
	SOC_SINGLE("Jack DBNC In", S5M3700X_D8_DCTR_DBNC1,
			JACK_DBNC_IN_SHIFT, JACK_DBNC_MAX, 0),

	SOC_SINGLE("Jack DBNC Out", S5M3700X_D8_DCTR_DBNC1,
			JACK_DBNC_OUT_SHIFT, JACK_DBNC_MAX, 0),

	SOC_SINGLE("Jack DBNC IRQ In", S5M3700X_D9_DCTR_DBNC2,
			JACK_DBNC_INT_IN_SHIFT, JACK_DBNC_INT_MAX, 0),

	SOC_SINGLE("Jack DBNC IRQ Out", S5M3700X_D9_DCTR_DBNC2,
			JACK_DBNC_INT_OUT_SHIFT, JACK_DBNC_INT_MAX, 0),

	SOC_SINGLE("MDET Threshold", S5M3700X_C4_ACTR_JD5,
			CTRV_ANT_MDET_SHIFT, 7, 0),

	SOC_SINGLE("MDET DBNC In", S5M3700X_DB_DCTR_DBNC4,
			ANT_MDET_DBNC_IN_SHIFT, 15, 0),

	SOC_SINGLE("MDET DBNC Out", S5M3700X_DB_DCTR_DBNC4,
			ANT_MDET_DBNC_OUT_SHIFT, 15, 0),

	SOC_SINGLE("MDET2 DBNC In", S5M3700X_DC_DCTR_DBNC5,
			ANT_MDET_DBNC_IN_SHIFT, 15, 0),

	SOC_SINGLE("MDET2 DBNC Out", S5M3700X_DC_DCTR_DBNC5,
			ANT_MDET_DBNC_OUT_SHIFT, 15, 0),

	SOC_SINGLE("Jack BTN DBNC", S5M3700X_DD_DCTR_DBNC6,
			CTMD_BTN_DBNC_SHIFT, 15, 0),

	SOC_SINGLE_EXT("Jack Pole Cnt", SND_SOC_NOPM, 0, 5, 0,
			jack_pole_cnt_get, jack_pole_cnt_put),

	SOC_SINGLE_EXT("Jack Pole Delay", SND_SOC_NOPM, 0, 10, 0,
			jack_pole_delay_get, jack_pole_delay_put),

	SOC_SINGLE_EXT("Jack Btn Cnt", SND_SOC_NOPM, 0, 5, 0,
			jack_btn_cnt_get, jack_btn_cnt_put),

	SOC_SINGLE_EXT("Jack Btn Delay", SND_SOC_NOPM, 0, 10, 0,
			jack_btn_delay_get, jack_btn_delay_put),

	SOC_SINGLE_EXT("ANT MDET1 TRIM", SND_SOC_NOPM, 0, 7, 0,
			ant_mdet1_get, ant_mdet1_put),

	SOC_SINGLE_EXT("ANT MDET2 TRIM", SND_SOC_NOPM, 0, 7, 0,
			ant_mdet2_get, ant_mdet2_put),

#if IS_ENABLED(CONFIG_SND_SOC_S5M3700X_VTS)
	/*
	 * VTS Control
	 */
	SOC_SINGLE_EXT("AMIC VTS On", SND_SOC_NOPM, 0, 3, 0,
			vts_get, vts_put),

	SOC_SINGLE_EXT("DMIC VTS On", SND_SOC_NOPM, 0, 2, 0,
			vts_dmic_get, vts_dmic_put),

	SOC_SINGLE_EXT("VTS1 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			vts1_boost_get, vts1_boost_put),

	SOC_SINGLE_EXT("VTS2 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			vts2_boost_get, vts2_boost_put),
#endif

#if IS_ENABLED(CONFIG_SND_SOC_S5M3700X_HIFI)
	/*
	 * HIFI DAC Control (CP2 Manual On function)
	 */
	SOC_SINGLE_EXT("HIFI DAC On", SND_SOC_NOPM, 0, 1, 0,
			hifi_dac_get, hifi_dac_put),

	SOC_SINGLE_EXT("HIFI CP2 CLK", SND_SOC_NOPM, 0, 7, 0,
			hifi_clk_get, hifi_clk_put),
#endif

	/*
	 * Codec control
	 */
	SOC_SINGLE_EXT("Codec Enable", SND_SOC_NOPM, 0, 1, 0,
			codec_enable_get, codec_enable_put),

	SOC_SINGLE_EXT("Codec Regdump On", SND_SOC_NOPM, 0, 1, 0,
			codec_regdump_get, codec_regdump_put),
};

/*
 * dapm widget controls set
 */

/* INP SEL */
static const char * const s5m3700x_inp_sel_src_l[] = {
	"AMIC_L ADC_L", "AMIC_R ADC_L", "AMIC_C ADC_L", "Zero ADC_L",
	"DMIC1L ADC_L", "DMIC1R ADC_L", "DMIC2L ADC_L", "DMIC2R ADC_L"
};
static const char * const s5m3700x_inp_sel_src_r[] = {
	"AMIC_R ADC_R", "AMIC_L ADC_R", "AMIC_C ADC_R", "Zero ADC_R",
	"DMIC1L ADC_R", "DMIC1R ADC_R", "DMIC2L ADC_R", "DMIC2R ADC_R"
};
static const char * const s5m3700x_inp_sel_src_c[] = {
	"AMIC_C ADC_C", "AMIC_L ADC_C", "AMIC_R ADC_C", "Zero ADC_C",
	"DMIC1L ADC_C", "DMIC1R ADC_C", "DMIC2L ADC_C", "DMIC2R ADC_C"
};

static SOC_ENUM_SINGLE_DECL(s5m3700x_inp_sel_enum_l, S5M3700X_31_ADC2,
		INP_SEL_L_SHIFT, s5m3700x_inp_sel_src_l);
static SOC_ENUM_SINGLE_DECL(s5m3700x_inp_sel_enum_r, S5M3700X_31_ADC2,
		INP_SEL_R_SHIFT, s5m3700x_inp_sel_src_r);
static SOC_ENUM_SINGLE_DECL(s5m3700x_inp_sel_enum_c, S5M3700X_32_ADC3,
		INP_SEL_C_SHIFT, s5m3700x_inp_sel_src_c);

static const struct snd_kcontrol_new s5m3700x_inp_sel_l =
		SOC_DAPM_ENUM("INP_SEL_L", s5m3700x_inp_sel_enum_l);
static const struct snd_kcontrol_new s5m3700x_inp_sel_r =
		SOC_DAPM_ENUM("INP_SEL_R", s5m3700x_inp_sel_enum_r);
static const struct snd_kcontrol_new s5m3700x_inp_sel_c =
		SOC_DAPM_ENUM("INP_SEL_C", s5m3700x_inp_sel_enum_c);

/* MIC On */
static const struct snd_kcontrol_new mic1_on[] = {
	SOC_DAPM_SINGLE("MIC1 On", S5M3700X_1E_CHOP1, 0, 1, 0),
};
static const struct snd_kcontrol_new mic2_on[] = {
	SOC_DAPM_SINGLE("MIC2 On", S5M3700X_1E_CHOP1, 1, 1, 0),
};
static const struct snd_kcontrol_new mic3_on[] = {
	SOC_DAPM_SINGLE("MIC3 On", S5M3700X_1E_CHOP1, 2, 1, 0),
};
static const struct snd_kcontrol_new mic4_on[] = {
	SOC_DAPM_SINGLE("MIC4 On", S5M3700X_1E_CHOP1, 3, 1, 0),
};

/* DMIC On */
static const struct snd_kcontrol_new dmic1_on[] = {
	SOC_DAPM_SINGLE("DMIC1 On", S5M3700X_1E_CHOP1, 4, 1, 0),
};
static const struct snd_kcontrol_new dmic2_on[] = {
	SOC_DAPM_SINGLE("DMIC2 On", S5M3700X_1E_CHOP1, 5, 1, 0),
};

/* Rx Devices */
static const struct snd_kcontrol_new ep_on[] = {
	SOC_DAPM_SINGLE("EP On", S5M3700X_1E_CHOP1, 6, 1, 0),
};
static const struct snd_kcontrol_new hp_on[] = {
	SOC_DAPM_SINGLE("HP On", S5M3700X_1E_CHOP1, 7, 1, 0),
};
static const struct snd_kcontrol_new line_on[] = {
	SOC_DAPM_SINGLE("LN On", S5M3700X_1F_CHOP2, 0, 1, 0),
};
static const struct snd_soc_dapm_widget s5m3700x_dapm_widgets[] = {
	/*
	 * ADC(Tx) dapm widget
	 */
	SND_SOC_DAPM_INPUT("IN1L"),
	SND_SOC_DAPM_INPUT("IN2L"),
	SND_SOC_DAPM_INPUT("IN3L"),
	SND_SOC_DAPM_INPUT("IN4L"),

	SND_SOC_DAPM_SUPPLY("VMID", SND_SOC_NOPM, 0, 0, vmid_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("DVMID", SND_SOC_NOPM, 0, 0, dvmid_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("MIC1_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic1_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MIC2_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic2_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MIC3_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic3_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MIC4_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic4_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("DMIC1_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, dmic1_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("DMIC2_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, dmic2_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SWITCH("MIC1", SND_SOC_NOPM, 0, 0, mic1_on),
	SND_SOC_DAPM_SWITCH("MIC2", SND_SOC_NOPM, 0, 0, mic2_on),
	SND_SOC_DAPM_SWITCH("MIC3", SND_SOC_NOPM, 0, 0, mic3_on),
	SND_SOC_DAPM_SWITCH("MIC4", SND_SOC_NOPM, 0, 0, mic4_on),

	SND_SOC_DAPM_SWITCH("DMIC1", SND_SOC_NOPM, 0, 0, dmic1_on),
	SND_SOC_DAPM_SWITCH("DMIC2", SND_SOC_NOPM, 0, 0, dmic2_on),

	SND_SOC_DAPM_MUX("INP_SEL_L", SND_SOC_NOPM, 0, 0, &s5m3700x_inp_sel_l),
	SND_SOC_DAPM_MUX("INP_SEL_R", SND_SOC_NOPM, 0, 0, &s5m3700x_inp_sel_r),
	SND_SOC_DAPM_MUX("INP_SEL_C", SND_SOC_NOPM, 0, 0, &s5m3700x_inp_sel_c),

	SND_SOC_DAPM_ADC_E("ADC", "AIF Capture", SND_SOC_NOPM, 0, 0, adc_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC", "AIF2 Capture", SND_SOC_NOPM, 0, 0, adc_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	/*
	 * DAC(Rx) dapm widget
	 */
	SND_SOC_DAPM_SWITCH("LINE", SND_SOC_NOPM, 0, 0, line_on),
	SND_SOC_DAPM_SWITCH("EP", SND_SOC_NOPM, 0, 0, ep_on),
	SND_SOC_DAPM_SWITCH("HP", SND_SOC_NOPM, 0, 0, hp_on),

	SND_SOC_DAPM_OUT_DRV_E("LNDRV", SND_SOC_NOPM, 0, 0, NULL, 0, linedrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUT_DRV_E("EPDRV", SND_SOC_NOPM, 0, 0, NULL, 0, epdrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUT_DRV_E("HPDRV", SND_SOC_NOPM, 0, 0, NULL, 0, hpdrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("DAC", "AIF Playback", SND_SOC_NOPM, 0, 0, dac_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("DAC", "AIF2 Playback", SND_SOC_NOPM, 0, 0, dac_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("LNOUTN"),
	SND_SOC_DAPM_OUTPUT("EPOUTN"),
	SND_SOC_DAPM_OUTPUT("HPOUTLN"),
};

static const struct snd_soc_dapm_route s5m3700x_dapm_routes[] = {
	/*
	 * ADC(Tx) dapm route
	 */
	{"MIC1_PGA", NULL, "IN1L"},
	{"MIC1_PGA", NULL, "VMID"},
	{"MIC1", "MIC1 On", "MIC1_PGA"},

	{"MIC2_PGA", NULL, "IN2L"},
	{"MIC2_PGA", NULL, "VMID"},
	{"MIC2", "MIC2 On", "MIC2_PGA"},

	{"MIC3_PGA", NULL, "IN3L"},
	{"MIC3_PGA", NULL, "VMID"},
	{"MIC3", "MIC3 On", "MIC3_PGA"},

	{"MIC4_PGA", NULL, "IN4L"},
	{"MIC4_PGA", NULL, "VMID"},
	{"MIC4", "MIC4 On", "MIC4_PGA"},

	{"DMIC1_PGA", NULL, "IN1L"},
	{"DMIC1_PGA", NULL, "DVMID"},
	{"DMIC1", "DMIC1 On", "DMIC1_PGA"},

	{"DMIC2_PGA", NULL, "IN2L"},
	{"DMIC2_PGA", NULL, "DVMID"},
	{"DMIC2", "DMIC2 On", "DMIC2_PGA"},

	{"INP_SEL_L", "AMIC_L ADC_L", "MIC1"},
	{"INP_SEL_L", "AMIC_C ADC_L", "MIC2"},
	{"INP_SEL_L", "AMIC_R ADC_L", "MIC3"},
	{"INP_SEL_L", "AMIC_R ADC_L", "MIC4"},
	{"INP_SEL_L", "DMIC1L ADC_L", "DMIC1"},
	{"INP_SEL_L", "DMIC1R ADC_L", "DMIC1"},
	{"INP_SEL_L", "DMIC2L ADC_L", "DMIC2"},
	{"INP_SEL_L", "DMIC2R ADC_L", "DMIC2"},

	{"INP_SEL_R", "AMIC_L ADC_R", "MIC1"},
	{"INP_SEL_R", "AMIC_C ADC_R", "MIC2"},
	{"INP_SEL_R", "AMIC_R ADC_R", "MIC3"},
	{"INP_SEL_R", "AMIC_R ADC_R", "MIC4"},
	{"INP_SEL_R", "DMIC1L ADC_R", "DMIC1"},
	{"INP_SEL_R", "DMIC1R ADC_R", "DMIC1"},
	{"INP_SEL_R", "DMIC2L ADC_R", "DMIC2"},
	{"INP_SEL_R", "DMIC2R ADC_R", "DMIC2"},

	{"INP_SEL_C", "AMIC_L ADC_C", "MIC1"},
	{"INP_SEL_C", "AMIC_C ADC_C", "MIC2"},
	{"INP_SEL_C", "AMIC_R ADC_C", "MIC3"},
	{"INP_SEL_C", "AMIC_R ADC_C", "MIC4"},
	{"INP_SEL_C", "DMIC1L ADC_C", "DMIC1"},
	{"INP_SEL_C", "DMIC1R ADC_C", "DMIC1"},
	{"INP_SEL_C", "DMIC2L ADC_C", "DMIC2"},
	{"INP_SEL_C", "DMIC2R ADC_C", "DMIC2"},

	{"ADC", NULL, "INP_SEL_L"},
	{"ADC", NULL, "INP_SEL_R"},
	{"ADC", NULL, "INP_SEL_C"},

	{"AIF Capture", NULL, "ADC"},
	{"AIF2 Capture", NULL, "ADC"},

	/*
	 * DAC(Rx) dapm route
	 */
	{"DAC", NULL, "AIF Playback"},
	{"DAC", NULL, "AIF2 Playback"},

	{"LNDRV", NULL, "DAC"},
	{"LINE", "LN On", "LNDRV"},
	{"LNOUTN", NULL, "LINE"},

	{"EPDRV", NULL, "DAC"},
	{"EP", "EP On", "EPDRV"},
	{"EPOUTN", NULL, "EP"},

	{"HPDRV", NULL, "DAC"},
	{"HP", "HP On", "HPDRV"},
	{"HPOUTLN", NULL, "HP"},
};

static int s5m3700x_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *codec = dai->component;

	dev_info(codec->dev, "%s called. fmt: %d\n", __func__, fmt);

	return 0;
}

static int s5m3700x_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	dev_info(s5m3700x->dev, "(%s) %s completed\n",
			substream->stream ? "C" : "P", __func__);

	return 0;
}

/*
 * capture_hw_params() - Register setting for capture
 *
 * @codec: SoC audio codec device
 * @cur_aifrate: current sample rate
 *
 * Desc: Set codec register related sample rate format before capture.
 */
static void capture_hw_params(struct snd_soc_component *codec,
		unsigned int cur_aifrate)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called. priv_aif: %d, cur_aif %d\n",
			__func__, s5m3700x->capture_aifrate, cur_aifrate);
	s5m3700x->capture_on = true;
	s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, true);

	if (s5m3700x->capture_on) {
		switch (cur_aifrate) {
		case S5M3700X_SAMPLE_RATE_48KHZ:
			/* 48K output format selection */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR,
					S5M3700X_29_IF_FORM6, ADC_OUT_FORMAT_SEL_MASK, ADC_FM_48K_AT_48K);
			break;
		case S5M3700X_SAMPLE_RATE_192KHZ:
			/* 192K output format selection */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR,
					S5M3700X_29_IF_FORM6, ADC_OUT_FORMAT_SEL_MASK, ADC_FM_192K_AT_192K);
			break;
		case S5M3700X_SAMPLE_RATE_384KHZ:
			/* 192K output format selection */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR,
					S5M3700X_29_IF_FORM6, ADC_OUT_FORMAT_SEL_MASK, ADC_FM_192K_AT_192K);
			break;
		default:
			dev_err(codec->dev, "%s: sample rate error!\n", __func__);
			/* 48K output format selection */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR,
					S5M3700X_29_IF_FORM6, ADC_OUT_FORMAT_SEL_MASK, ADC_FM_48K_AT_48K);
			break;
		}
		s5m3700x->capture_aifrate = cur_aifrate;
	}
	if (s5m3700x->codec_debug && s5m3700x->tx_dapm_done && s5m3700x->capture_on)
		codec_regdump(codec);
}

/*
 * playback_hw_params() - Register setting for playback
 *
 * @codec: SoC audio codec device
 * @cur_aifrate: current sample rate
 *
 * Desc: Set codec register related sample rate format before playback.
 */
static void playback_hw_params(struct snd_soc_component *codec,
		unsigned int cur_aifrate)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int i = 0;

	dev_info(codec->dev, "%s called. priv_aif: %d, cur_aif %d\n",
			__func__, s5m3700x->playback_aifrate, cur_aifrate);
	s5m3700x->playback_on = true;

	if (s5m3700x->playback_on) {
		s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_7A_AVC43, 0x9A);

		switch (cur_aifrate) {
		case S5M3700X_SAMPLE_RATE_48KHZ:
			/* AVC Var Mode on */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_50_AVC1, 0x05);

			/* AVC Range Default Setting */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_51_AVC2, 0x6A);

			/* CLK Mode Selection */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_16_CLK_MODE_SEL1,
					DAC_FSEL_MASK, 0);

			/* PlayMode Selection */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_40_PLAY_MODE, 0x04);

			/* DAC Gain trim */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_46_PLAY_MIX2, 0x00);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_47_TRIM_DAC0, 0xF7);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_48_TRIM_DAC1, 0x4F);
			/* AVC Delay Setting */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_71_AVC34, 0x18);

			/* Setting the HP Normal Trim */
			for (i = 0; i < 50; i++)
				s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, i, s5m3700x->hp_normal_trim[i]);

			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_72_AVC35, 0x2F);

			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_73_AVC36, 0x17);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_74_AVC37, 0xE9);
			break;
		case S5M3700X_SAMPLE_RATE_192KHZ:
			/* AVC Var Mode off */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_50_AVC1, 0x01);

			/* AVC Range Default Setting */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_51_AVC2, 0x50);

			/* CLK Mode Selection */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_16_CLK_MODE_SEL1,
					DAC_FSEL_MASK, DAC_FSEL_MASK);

			/* PlayMode Selection */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_40_PLAY_MODE, 0x44);

			/* DAC Gain trim */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_46_PLAY_MIX2, 0x00);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_47_TRIM_DAC0, 0xFF);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_48_TRIM_DAC1, 0x1E);

			/* Setting the HP UHQA Trim */
			for (i = 0; i < 50; i++)
				s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, i, s5m3700x->hp_uhqa_trim[i]);

			/* AVC Delay Setting */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_71_AVC34, 0xC6);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_72_AVC35, 0x01);

			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_73_AVC36, 0xC4);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_74_AVC37, 0x39);
			break;
		case S5M3700X_SAMPLE_RATE_384KHZ:
			/* AVC Var Mode off */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_50_AVC1, 0x01);

			/* AVC Range Default Setting */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_51_AVC2, 0x50);

			/* CLK Mode Selection */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_16_CLK_MODE_SEL1,
					DAC_FSEL_MASK | AVC_CLK_SEL_MASK,
					DAC_FSEL_MASK | AVC_CLK_SEL_MASK);

			/* PlayMode Selection */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_40_PLAY_MODE, 0x54);

			/* DAC Gain trim */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_46_PLAY_MIX2, 0x54);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_47_TRIM_DAC0, 0xFD);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_48_TRIM_DAC1, 0x12);

			/* AVC Delay Setting */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_71_AVC34, 0xC6);

			/* Setting the HP UHQA Trim */
			for (i = 0; i < 50; i++)
				s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, i, s5m3700x->hp_uhqa_trim[i]);

			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_72_AVC35, 0x00);

			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_73_AVC36, 0xC4);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_74_AVC37, 0x3A);
			break;
		default:
			/* CLK Mode Selection */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_16_CLK_MODE_SEL1,
					DAC_FSEL_MASK, 0);

			/* PlayMode Selection */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_40_PLAY_MODE, 0x04);

			/* DAC Gain trim */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_46_PLAY_MIX2, 0x00);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_47_TRIM_DAC0, 0xF7);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_48_TRIM_DAC1, 0x4F);
			/* AVC Delay Setting */
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_71_AVC34, 0x18);

			/* Setting the HP Normal Trim */
			for (i = 0; i < 50; i++)
				s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, i, s5m3700x->hp_normal_trim[i]);

			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_72_AVC35, 0x2F);

			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_73_AVC36, 0x17);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_74_AVC37, 0xE9);

			dev_err(codec->dev, "%s: sample rate error!\n", __func__);
			break;
		}
		s5m3700x->playback_aifrate = cur_aifrate;
	}
	if (s5m3700x->codec_debug && s5m3700x->rx_dapm_done && s5m3700x->playback_on)
		codec_regdump(codec);
}

static int s5m3700x_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	unsigned int cur_aifrate, width, channels;

	/* Get params */
	cur_aifrate = params_rate(params);
	width = params_width(params);
	channels = params_channels(params);

	dev_info(codec->dev, "(%s) %s called. aifrate: %d, width: %d, channels: %d\n",
			substream->stream ? "C" : "P", __func__, cur_aifrate, width, channels);

	switch (width) {
	case BIT_RATE_16:
		/* I2S 16bit Set */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_21_IF_FORM2,
				I2S_DL_MASK, I2S_DL_16);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_32);
			break;
		case CHANNEL_4:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_64);
			break;
		default:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_32);
			break;
		}
		break;
	case BIT_RATE_24:
		/* I2S 24bit Set */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_21_IF_FORM2,
				I2S_DL_MASK, I2S_DL_24);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_48);
			break;
		case CHANNEL_4:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_96);
			break;
		default:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_48);
			break;
		}
		break;
	case BIT_RATE_32:
		/* I2S 32bit Set */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_21_IF_FORM2,
				I2S_DL_MASK, I2S_DL_32);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_64);
			break;
		case CHANNEL_4:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_128);
			break;
		default:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_64);
			break;
		}
		break;
	default:
		dev_err(codec->dev, "%s: bit rate error!\n", __func__);
		/* I2S 16bit Set */
		s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_21_IF_FORM2,
				I2S_DL_MASK, I2S_DL_16);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_32);
			break;
		case CHANNEL_4:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_64);
			break;
		default:
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_22_IF_FORM3,
					I2S_XFS_MASK, I2S_XFS_32);
			break;
		}
		break;
	}

	if (substream->stream)
		capture_hw_params(codec, cur_aifrate);
	else
		playback_hw_params(codec, cur_aifrate);
	return 0;
}

static void s5m3700x_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "(%s) %s completed\n",
			substream->stream ? "C" : "P", __func__);

	if (substream->stream)
		s5m3700x->capture_on = false;
	else
		s5m3700x->playback_on = false;
}

static const struct snd_soc_dai_ops s5m3700x_dai_ops = {
	.set_fmt = s5m3700x_dai_set_fmt,
	.startup = s5m3700x_dai_startup,
	.hw_params = s5m3700x_dai_hw_params,
	.shutdown = s5m3700x_dai_shutdown,
};

#define S5M3700X_RATES		SNDRV_PCM_RATE_8000_192000
#define S5M3700X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
							SNDRV_PCM_FMTBIT_S20_3LE | \
							SNDRV_PCM_FMTBIT_S24_LE  | \
							SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver s5m3700x_dai[] = {
	{
		.name = "s5m3700x-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = S5M3700X_RATES,
			.formats = S5M3700X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = S5M3700X_RATES,
			.formats = S5M3700X_FORMATS,
		},
		.ops = &s5m3700x_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "s5m3700x-aif2",
		.id = 2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = S5M3700X_RATES,
			.formats = S5M3700X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = S5M3700X_RATES,
			.formats = S5M3700X_FORMATS,
		},
		.ops = &s5m3700x_dai_ops,
		.symmetric_rates = 1,
	},
};

static void s5m3700x_i2c_parse_dt(struct s5m3700x_priv *s5m3700x)
{
	struct device *dev = s5m3700x->dev;
	struct device_node *np = dev->of_node;
	unsigned int jack_type;

	/*
	 * Setting Jack type
	 * 0: 3.5 Pi, 1: USB Type-C
	 */
	if (!of_property_read_u32(dev->of_node, "jack-type", &jack_type))
		s5m3700x->jack_type = jack_type;
	else
		s5m3700x->jack_type = JACK;

	dev_info(dev, "Jack type: %s\n", s5m3700x->jack_type ? "USB Jack" : "3.5 PI");

	/* RESETB gpio */
	s5m3700x->resetb_gpio = of_get_named_gpio(np, "s5m3700x-resetb", 0);
	if (s5m3700x->resetb_gpio < 0)
		dev_err(dev, "%s: cannot find resetb gpio in the dt\n", __func__);
	else
		dev_info(dev, "%s: resetb gpio = %d\n",
				__func__, s5m3700x->resetb_gpio);

	/* external_ldo gpio */
	s5m3700x->external_ldo = of_get_named_gpio(np, "s5m3700x-external-ldo", 0);
	if (s5m3700x->external_ldo < 0)
		dev_err(dev, "%s: cannot find external_ldo gpio in the dt\n", __func__);
	else
		dev_info(dev, "%s: external_ldo gpio = %d\n",
				__func__, s5m3700x->external_ldo);

}

static void s5m3700x_power_setting(struct s5m3700x_priv *s5m3700x)
{
	struct device *dev = s5m3700x->dev;

	/* Setting External LDO gpio */
	if (s5m3700x->external_ldo > 0) {
		if (gpio_request(s5m3700x->external_ldo, "s5m3700x_external_ldo") < 0)
			dev_err(s5m3700x->dev, "%s: Request for %d GPIO failed\n",
					__func__, (int)s5m3700x->external_ldo);
		if (gpio_direction_output(s5m3700x->external_ldo, 1) < 0)
			dev_err(s5m3700x->dev, "%s: GPIO direction to output failed!\n",
					__func__);
	}
	dev_info(dev, "S5M3700X Setting External LDO High.\n");

	/* HW Delay */
	s5m3700x_usleep(1000);

	if (!regulator_is_enabled(s5m3700x->vdd)) {
		regulator_enable(s5m3700x->vdd);
		dev_info(dev, "S5M3700X enable vdd regulator.\n");
	}

	if (!regulator_is_enabled(s5m3700x->vdd2)) {
		regulator_enable(s5m3700x->vdd2);
		dev_info(dev, "S5M3700X enable vdd2 regulator.\n");
	}

	/* HW Delay */
	s5m3700x_usleep(2000);

	/* Setting RESETB gpio */
	if (s5m3700x->resetb_gpio > 0) {
		if (gpio_request(s5m3700x->resetb_gpio, "s5m3700x_resetb_gpio") < 0)
			dev_err(s5m3700x->dev, "%s: Request for %d GPIO failed\n",
					__func__, (int)s5m3700x->resetb_gpio);
		if (gpio_direction_output(s5m3700x->resetb_gpio, 1) < 0)
			dev_err(s5m3700x->dev, "%s: GPIO direction to output failed!\n",
					__func__);
	}
	dev_info(dev, "S5M3700X Setting RESETB High.\n");
}

static void s5m3700x_register_initialize(struct s5m3700x_priv *s5m3700x)
{
	dev_info(s5m3700x->dev, "%s called, setting defaults\n", __func__);

	/* BGR & IBIAS On */
	s5m3700x_i2c_write_reg(S5M3700X_MV_ADDR, 0x04, 0x03);
	s5m3700x_usleep(5000);

	/* DLDO On */
	s5m3700x_i2c_write_reg(S5M3700X_MV_ADDR, 0x04, 0x07);

	/* Audio Codec LV RESETB High */
	s5m3700x_i2c_write_reg(S5M3700X_MV_ADDR, 0x07, 0x42);
	msleep(20);

	/* Read Codec Chip ID */
	s5m3700x_i2c_read_reg(S5M3700X_MV_ADDR, 0x03, &s5m3700x->codec_ver);

#if IS_ENABLED(CONFIG_PM)
	pm_runtime_get_sync(s5m3700x->dev);
#else
	s5m3700x_enable(s5m3700x->dev);
#endif

#if IS_ENABLED(CONFIG_SND_SOC_S5M3700X_VTS)
	/* VTS HighZ Setting */
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_2F_DMIC_ST,
			VTS_DATA_ENB_MASK, VTS_DATA_ENB_MASK);
#endif

	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_33_ADC4, 0x11);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_38_AD_TRIM1, 0x48);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_39_AD_TRIM2, 0x98);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_40_PLAY_MODE, 0x04);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_50_AVC1, 0x05);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_53_AVC4, 0x80);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_54_AVC5, 0x89);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_58_AVC9, 0x00);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_5C_AVC13, 0x99);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_68_AVC25, 0x08);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_93_DSM_CON1, 0x52);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_9D_AVC_DWA, 0x46);

	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_4B_OFFSET_CTR, 0xFF);

	/* PD_IDAC4 */
	s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_135_PD_IDAC4,
			EN_IDAC1_OUTTIE_MASK|EN_IDAC2_OUTTIE_MASK|EN_OUTTIEL_MASK|EN_OUTTIER_MASK, 0x00);
	/* ODSEL6 */
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_B5_ODSEL6,
			T_EN_OUTTIEL_MASK|T_EN_OUTTIER_MASK, T_EN_OUTTIEL_MASK|T_EN_OUTTIER_MASK);

	/* Default Digital Gain */
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_41_PLAY_VOLL, 0x54);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_42_PLAY_VOLR, 0x54);

	/* RCV gain setting faster to 166usec */
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_7E_AVC45, 0x4B);

	/* ADC/DAC Mute */
	s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, true);
	s5m3700x_dac_soft_mute(s5m3700x, DAC_MUTE_ALL, true);

#if IS_ENABLED(CONFIG_PM)
	pm_runtime_put_sync(s5m3700x->dev);
#else
	s5m3700x_disable(s5m3700x->dev);
#endif
}

static int s5m3700x_codec_probe(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	pr_err("Codec Driver Probe: (%s).\n", __func__);

	s5m3700x->codec = codec;

	/* register codec power */
	s5m3700x->vdd = devm_regulator_get(s5m3700x->dev, "vdd_hldo");
	if (IS_ERR(s5m3700x->vdd)) {
		dev_warn(s5m3700x->dev, "failed to get regulator vdd\n");
		return PTR_ERR(s5m3700x->vdd);
	}

	s5m3700x->vdd2 = devm_regulator_get(s5m3700x->dev, "vdd_ldo22s");
	if (IS_ERR(s5m3700x->vdd2)) {
		dev_warn(s5m3700x->dev, "failed to get regulator vdd2\n");
		return PTR_ERR(s5m3700x->vdd2);
	}

	/* Parsing to dt */
	s5m3700x_i2c_parse_dt(s5m3700x);

	/* Codec Power GPIO Setting */
	s5m3700x_power_setting(s5m3700x);

	/* Initialize Codec Register */
	s5m3700x_register_initialize(s5m3700x);

	/* Ignore suspend status for DAPM endpoint */
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "HPOUTLN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "EPOUTN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "LNOUTN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN1L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN2L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN3L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN4L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF2 Capture");
	snd_soc_dapm_sync(snd_soc_component_get_dapm(codec));

	dev_info(codec->dev, "Codec Probe Done.\n");
	s5m3700x->is_probe_done = true;

	/* Jack probe */
	if (s5m3700x->jack_type  == USB_JACK)
		s5m3700x_usbjack_probe(s5m3700x);
	else
		s5m3700x_jack_probe(s5m3700x);

	return 0;
}

static void s5m3700x_codec_remove(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	dev_info(s5m3700x->dev, "(*) %s called\n", __func__);

	destroy_workqueue(s5m3700x->adc_mute_wq);

	if (s5m3700x->jack_type  == USB_JACK)
		s5m3700x_usbjack_remove(codec);
	else
		s5m3700x_jack_remove(codec);
	s5m3700x_regulators_disable(codec);
}

static const struct snd_soc_component_driver soc_codec_dev_s5m3700x = {
	.probe = s5m3700x_codec_probe,
	.remove = s5m3700x_codec_remove,
	.controls = s5m3700x_snd_controls,
	.num_controls = ARRAY_SIZE(s5m3700x_snd_controls),
	.dapm_widgets = s5m3700x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(s5m3700x_dapm_widgets),
	.dapm_routes = s5m3700x_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(s5m3700x_dapm_routes),
	.use_pmdown_time = false,
	.idle_bias_on = false,
};

static int s5m3700x_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct s5m3700x_priv *s5m3700x;
	struct device *dev;
	int ret = 0;

	dev_info(&i2c->dev, "Codec I2C Probe: (%s) name: %s, size: %d, i2c addr: 0x%02x\n",
	__func__, id->name, sizeof(*s5m3700x), (int)i2c->addr);

	s5m3700x = kzalloc(sizeof(*s5m3700x), GFP_KERNEL);
	if (s5m3700x == NULL)
		return -ENOMEM;

	i2c->addr = 0x14;
	s5m3700x_stat = s5m3700x;
	s5m3700x->dev = &i2c->dev;
	s5m3700x->is_probe_done = false;
	s5m3700x->regulator_count = 0;
	s5m3700x->regcache_count = 0;

	/* initialize codec_priv variable */
	s5m3700x->playback_aifrate = 0;
	s5m3700x->capture_aifrate = 0;
	s5m3700x->hp_avc_gain = 0;
	s5m3700x->rcv_avc_gain = 0;
	s5m3700x->playback_on = false;
	s5m3700x->capture_on = false;
	s5m3700x->dmic_delay = 120;
	s5m3700x->amic_delay = 100;
	s5m3700x->vts1_on = false;
	s5m3700x->vts2_on = false;
	s5m3700x->vts_dmic_on = VTS_DMIC_OFF;
	s5m3700x->hifi_on = false;
	s5m3700x->codec_debug = false;
	s5m3700x->tx_dapm_done = false;
	s5m3700x->rx_dapm_done = false;
	s5m3700x->codec_sw_ver = S5M3700X_CODEC_SW_VER;
	s5m3700x->hp_imp = HP_IMP_32;
	s5m3700x->rcv_imp = RCV_IMP_32;
	s5m3700x->hp_bias_cnt = 0;
	s5m3700x->hp_bias_mix_cnt = 0;
	s5m3700x->jack_type = JACK;

	s5m3700x->i2c_priv[S5M3700D] = i2c;
	s5m3700x->i2c_priv[S5M3700A] = i2c_new_dummy_device(i2c->adapter, S5M3700X_ANALOG_ADDR);
	s5m3700x->i2c_priv[S5M3700O] = i2c_new_dummy_device(i2c->adapter, S5M3700X_OTP_ADDR);

	i2c_set_clientdata(s5m3700x->i2c_priv[S5M3700D], s5m3700x);
	i2c_set_clientdata(s5m3700x->i2c_priv[S5M3700A], s5m3700x);
	i2c_set_clientdata(s5m3700x->i2c_priv[S5M3700O], s5m3700x);

	s5m3700x->regmap[S5M3700D] =
		devm_regmap_init(&s5m3700x->i2c_priv[S5M3700D]->dev, &s5m3700x_dbus,
				s5m3700x, &s5m3700x_digital_regmap);
	if (IS_ERR(s5m3700x->regmap[S5M3700D])) {
		dev_err(&i2c->dev, "Failed to allocate digital regmap: %li\n",
				PTR_ERR(s5m3700x->regmap[S5M3700D]));
		ret = -ENOMEM;
		goto err;
	}

	s5m3700x->regmap[S5M3700A] =
		devm_regmap_init(&s5m3700x->i2c_priv[S5M3700A]->dev, &s5m3700x_abus,
				s5m3700x, &s5m3700x_analog_regmap);
	if (IS_ERR(s5m3700x->regmap[S5M3700A])) {
		dev_err(&i2c->dev, "Failed to allocate analog regmap: %li\n",
				PTR_ERR(s5m3700x->regmap[S5M3700A]));
		ret = -ENOMEM;
		goto err;
	}

	s5m3700x->regmap[S5M3700O] =
		devm_regmap_init(&s5m3700x->i2c_priv[S5M3700O]->dev, &s5m3700x_obus,
				s5m3700x, &s5m3700x_otp_regmap);
	if (IS_ERR(s5m3700x->regmap[S5M3700O])) {
		dev_err(&i2c->dev, "Failed to allocate otp regmap: %li\n",
				PTR_ERR(s5m3700x->regmap[S5M3700O]));
		ret = -ENOMEM;
		goto err;
	}

	dev = &s5m3700x->i2c_priv[S5M3700D]->dev;

	/* initialize workqueue */
	INIT_DELAYED_WORK(&s5m3700x->adc_mute_work, s5m3700x_adc_mute_work);
	s5m3700x->adc_mute_wq = create_singlethread_workqueue("adc_mute_wq");
	if (s5m3700x->adc_mute_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create adc_mute_wq\n");
		return -ENOMEM;
	}

	/* initialize mutex lock */
	mutex_init(&s5m3700x->i2c_mutex);
	mutex_init(&s5m3700x->regcache_lock);
	mutex_init(&s5m3700x->adc_mute_lock);
	mutex_init(&s5m3700x->dacl_mute_lock);
	mutex_init(&s5m3700x->dacr_mute_lock);

	ret = snd_soc_register_component(dev, &soc_codec_dev_s5m3700x,
			s5m3700x_dai, ARRAY_SIZE(s5m3700x_dai));
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register digital codec: %d\n", ret);
		goto err;
	}
#if IS_ENABLED(CONFIG_PM)
	pm_runtime_enable(s5m3700x->dev);
#endif
	return ret;
err:
	kfree(s5m3700x);
	return ret;
}

static int s5m3700x_i2c_remove(struct i2c_client *i2c)
{
	struct s5m3700x_priv *s5m3700x = dev_get_drvdata(&i2c->dev);

	dev_info(s5m3700x->dev, "(*) %s called\n", __func__);

	snd_soc_unregister_component(&i2c->dev);
	kfree(s5m3700x);

	return 0;
}

int s5m3700x_enable(struct device *dev)
{
	struct s5m3700x_priv *s5m3700x = dev_get_drvdata(dev);
	int p_map;

	dev_info(dev, "(*) %s. Codec Rev: 0.%d, Ver: %d.\n",
			__func__, s5m3700x->codec_ver, s5m3700x->codec_sw_ver);
	s5m3700x->is_suspend = false;
	s5m3700x->regcache_count++;

	s5m3700x_regulators_enable(s5m3700x->codec);

	if (s5m3700x->regcache_count == 1) {
		/* Disable cache_only feature and sync the cache with h/w */
		for (p_map = S5M3700D; p_map <= S5M3700O; p_map++)
			regcache_cache_only(s5m3700x->regmap[p_map], false);
	}

	s5m3700x_reg_restore(s5m3700x->codec);
	return 0;
}

int s5m3700x_disable(struct device *dev)
{
	struct s5m3700x_priv *s5m3700x = dev_get_drvdata(dev);
	int p_map;

	dev_info(dev, "(*) %s\n", __func__);

	s5m3700x->is_suspend = true;
	s5m3700x->regcache_count--;

	if (s5m3700x->regcache_count == 0) {
		/* As device is going to suspend-state, limit the writes to cache */
		for (p_map = S5M3700D; p_map <= S5M3700O; p_map++)
			regcache_cache_only(s5m3700x->regmap[p_map], true);
	}

	s5m3700x_regulators_disable(s5m3700x->codec);

	return 0;
}

static int s5m3700x_sys_resume(struct device *dev)
{
#if !IS_ENABLED(CONFIG_PM)
	struct s5m3700x_priv *s5m3700x = dev_get_drvdata(dev);

	if (!s5m3700x->is_suspend) {
		dev_info(dev, "(*)s5m3700x not resuming, cp functioning\n");
		return 0;
	}
	dev_info(dev, "(*) %s\n", __func__);
	s5m3700x->pm_suspend = false;
	s5m3700x_enable(dev);
#endif
	return 0;
}

static int s5m3700x_sys_suspend(struct device *dev)
{
#if !IS_ENABLED(CONFIG_PM)
	struct s5m3700x_priv *s5m3700x = dev_get_drvdata(dev);

	if (abox_is_on()) {
		dev_info(dev, "(*)Don't suspend s5m3700x, cp functioning\n");
		return 0;
	}
	dev_info(dev, "(*) %s\n", __func__);
	s5m3700x->pm_suspend = true;
	s5m3700x_disable(dev);
#endif
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int s5m3700x_runtime_resume(struct device *dev)
{
	dev_info(dev, "(*) %s\n", __func__);
	s5m3700x_enable(dev);

	return 0;
}

static int s5m3700x_runtime_suspend(struct device *dev)
{
	dev_info(dev, "(*) %s\n", __func__);
	s5m3700x_disable(dev);

	return 0;
}
#endif

static const struct dev_pm_ops s5m3700x_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(
			s5m3700x_sys_suspend,
			s5m3700x_sys_resume)
#if IS_ENABLED(CONFIG_PM)
	SET_RUNTIME_PM_OPS(
			s5m3700x_runtime_suspend,
			s5m3700x_runtime_resume,
			NULL)
#endif
};

static const struct i2c_device_id s5m3700x_i2c_id[] = {
	{ "s5m3700x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s5m3700x_i2c_id);

const struct of_device_id s5m3700x_of_match[] = {
	{ .compatible = "samsung,s5m3700x", },
	{ },
};

static struct i2c_driver s5m3700x_i2c_driver = {
	.driver = {
		.name = "s5m3700x",
		.owner = THIS_MODULE,
		.pm = &s5m3700x_pm,
		.of_match_table = of_match_ptr(s5m3700x_of_match),
	},
	.probe = s5m3700x_i2c_probe,
	.remove = s5m3700x_i2c_remove,
	.id_table = s5m3700x_i2c_id,
};

module_i2c_driver(s5m3700x_i2c_driver);

MODULE_SOFTDEP("pre: snd-soc-samsung-vts");
MODULE_DESCRIPTION("ASoC S5M3700X driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:S5M3700X-codec");
