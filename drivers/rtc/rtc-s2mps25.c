/*
 * rtc-s2mps25.c
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  2013-12-11  Performance improvements and code clean up by
 *		Minsung Kim <ms925.kim@samsung.com>
 *
 */
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/rtc.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/mfd/samsung/rtc-s2mp.h>
#include <linux/mfd/samsung/s2mps25.h>
#include <linux/mfd/samsung/s2mps25-regulator.h>
#include <linux/sec_debug.h>
#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
#include <linux/reboot.h>
#include <linux/fs.h>
#include <soc/samsung/exynos-pmu-if.h>
#endif
#if IS_ENABLED(CONFIG_SEC_PM)
#include <linux/sec_class.h>
#endif /* CONFIG_SEC_PM */
#include <linux/sec_pm_cpufreq.h>

struct s2m_rtc_info {
	struct device		*dev;
	struct i2c_client	*i2c;
	struct i2c_client	*pmic_i2c;
	struct s2mps25_dev	*iodev;
	struct rtc_device	*rtc_dev;
	struct mutex		lock;
	struct delayed_work	irq_work;
	int			irq;
	int			smpl_irq;
#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
	int			boot_alarm_irq;
	bool			boot_alarm_enabled;

#endif
	bool			use_irq;
	bool			wtsr_en;
	bool			smpl_en;
	bool			alarm_enabled;
	uint8_t			update_reg;
	bool			alarm_check;
	uint8_t			wudr_mask;
	uint8_t			audr_mask;
	int			smpl_warn_info;
#if IS_ENABLED(CONFIG_SEC_PM)
	struct device		*pmic_rtc_dev;
	unsigned int		smpl_warn_cnt;
	bool			is_rtc_cleared;
#endif /* CONFIG_SEC_PM */
};

enum S2M_RTC_OP {
	S2M_RTC_READ,
	S2M_RTC_WRITE_TIME,
	S2M_RTC_WRITE_ALARM,
};
#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
#define POWER_SYSIP_INFORM3       0x080C // TODO: check it out
static unsigned int is_charging_mode;
#endif
struct s2m_rtc_info *static_info;

int secure_debug_set_config(uint8_t addr, uint8_t config)
{
	const char *config_bit[] = {"RTC_BOOTING", "RTC_SUSPEND",
				    "RTC_RESUME", "RTC_SHUTDOWN", "RTC_POWEROFF"};
	unsigned char val;
	int ret = 0;

	if (addr != RTC_REG_SECURE1 && addr != RTC_REG_SECURE2 &&
	    addr != RTC_REG_SECURE3 && addr != RTC_REG_SECURE4) {
		pr_err("%s: wrong addr.(0x%02hhx)\n", __func__, addr);
		return -1;
	}

	if (config != RTC_BOOTING && config != RTC_SUSPEND &&
	    config != RTC_RESUME && config != RTC_SHUTDOWN && config != RTC_POWEROFF) {
		pr_err("%s: wrong config(0x%02hhx)\n", __func__, config);
		return -1;
	}

	ret = s2mps25_update_reg(static_info->i2c, addr, config, 0xF);
	if (ret < 0) {
		pr_err("%s: s2mps25_update_reg failed\n", __func__);
		return -1;
	}

	ret = s2mps25_read_reg(static_info->i2c, addr, &val);
	if (ret < 0) {
		pr_err("%s: s2mps25_read_reg failed\n", __func__);
		return -1;
	}

	pr_info("[PMIC] %s: %s: 0x02%02hhx(0x%02hhx)\n",
		__func__, config_bit[config - RTC_BOOTING], addr, val);

	return 0;
}
EXPORT_SYMBOL_GPL(secure_debug_set_config);

int secure_debug_read(uint8_t addr, uint8_t bit)
{
	int ret = 0;
	unsigned char val;

	if (addr != RTC_REG_SECURE1 && addr != RTC_REG_SECURE2 &&
	    addr != RTC_REG_SECURE3 && addr != RTC_REG_SECURE4)
		return -1;

	if (bit > 7)
		return -1;

	ret = s2mps25_read_reg(static_info->i2c, addr, &val);
	if (ret < 0)
		goto err;

	val = (val >> bit) & 0x1;

	return val;
err:
	return -1;
}
EXPORT_SYMBOL_GPL(secure_debug_read);

void secure_debug_write(uint8_t addr, uint8_t bit, uint8_t val)
{
	int ret = 0;

	if (addr != RTC_REG_SECURE1 && addr != RTC_REG_SECURE2 &&
	    addr != RTC_REG_SECURE3 && addr != RTC_REG_SECURE4)
		return;

	if (bit > 7)
		return;

	if (val != 0 && val != 1)
		return;

	ret = s2mps25_update_reg(static_info->i2c, addr, val << bit, 1 << bit);
	if (ret < 0)
		return;
}
EXPORT_SYMBOL_GPL(secure_debug_write);

void secure_debug_clear(void)
{
	int ret = 0, i = 0;

	for (i = RTC_REG_SECURE1; i <= RTC_REG_SECURE4; i++) {
		ret = s2mps25_write_reg(static_info->i2c, i, 0x00);
		if (ret < 0)
			return;
	}
}
EXPORT_SYMBOL_GPL(secure_debug_clear);

static void s2m_data_to_tm(uint8_t *data, struct rtc_time *tm)
{
//	tm->tm_msec = (data[RTC_MSEC] & 0x0f) + (data[RTC_MSEC] & 0xf0) * 10;
	tm->tm_sec = data[RTC_SEC] & 0x7f;
	tm->tm_min = data[RTC_MIN] & 0x7f;
	tm->tm_hour = data[RTC_HOUR] & 0x1f;
	tm->tm_wday = __fls(data[RTC_WEEKDAY] & 0x7f);
	tm->tm_mday = data[RTC_DATE] & 0x1f;
	tm->tm_mon = (data[RTC_MONTH] & 0x0f) - 1;
	tm->tm_year = (data[RTC_YEAR] & 0x7f) + 100;
	tm->tm_yday = 0;
	tm->tm_isdst = 0;
}

static int s2m_tm_to_data(struct rtc_time *tm, uint8_t *data)
{
//	data[RTC_MSEC] = ((tm->tm_msec / 10) << 4) | (tm->tm_msec % 10);
	data[RTC_SEC] = tm->tm_sec;
	data[RTC_MIN] = tm->tm_min;

	if (tm->tm_hour >= 12)
		data[RTC_HOUR] = tm->tm_hour | HOUR_PM_MASK;
	else
		data[RTC_HOUR] = tm->tm_hour;

	data[RTC_WEEKDAY] = BIT(tm->tm_wday);
	data[RTC_DATE] = tm->tm_mday;
	data[RTC_MONTH] = tm->tm_mon + 1;
	data[RTC_YEAR] = tm->tm_year > 100 ? (tm->tm_year - 100) : 0;

	if (tm->tm_year < 100) {
		pr_warn("%s: SEC RTC cannot handle the year %d\n", __func__,
				1900 + tm->tm_year);
		return -EINVAL;
	}
	return 0;
}

static int s2m_rtc_update(struct s2m_rtc_info *info,
				 enum S2M_RTC_OP op)
{
	uint8_t data;
	int ret;

	if (!info || !info->iodev) {
		pr_err("%s: Invalid argument\n", __func__);
		return -EINVAL;
	}

	switch (op) {
	case S2M_RTC_READ:
		data = RTC_RUDR_MASK;
		break;
	case S2M_RTC_WRITE_TIME:
		data = RTC_WUDR_MASK_REV;
		break;
	case S2M_RTC_WRITE_ALARM:
		data = RTC_AUDR_MASK_REV;
		break;
	default:
		dev_err(info->dev, "%s: invalid op(%d)\n", __func__, op);
		return -EINVAL;
	}

	data |= info->update_reg;

	ret = s2mps25_write_reg(info->i2c, S2MP_RTC_REG_UPDATE, data);
	if (ret < 0)
		dev_err(info->dev, "%s: fail to write update reg(%d,%u)\n",
				__func__, ret, data);
	else
		usleep_range(1000, 1005);

	return ret;
}

static int s2m_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	uint8_t data[NR_RTC_CNT_REGS];
	int ret;

	mutex_lock(&info->lock);
	ret = s2m_rtc_update(info, S2M_RTC_READ);
	if (ret < 0)
		goto out;

	ret = s2mps25_bulk_read(info->i2c, S2MP_RTC_REG_SEC, NR_RTC_CNT_REGS,
			data);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read time reg(%d)\n", __func__,
			ret);
		goto out;
	}

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(0x%02hhx)%s\n",
			__func__, data[RTC_YEAR] + 2000, data[RTC_MONTH],
			data[RTC_DATE], data[RTC_HOUR] & 0x1f, data[RTC_MIN],
			data[RTC_SEC], data[RTC_WEEKDAY],
			data[RTC_HOUR] & HOUR_PM_MASK ? "PM" : "AM");

	s2m_data_to_tm(data, tm);
	ret = rtc_valid_tm(tm);
out:
	mutex_unlock(&info->lock);
	return ret;
}

static int s2m_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	uint8_t data[NR_RTC_CNT_REGS];
	int ret;

	ret = s2m_tm_to_data(tm, data);
	if (ret < 0)
		return ret;

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(0x%02hhx)%s\n",
			__func__, data[RTC_YEAR] + 2000, data[RTC_MONTH],
			data[RTC_DATE], data[RTC_HOUR] & 0x1f, data[RTC_MIN],
			data[RTC_SEC], data[RTC_WEEKDAY],
			data[RTC_HOUR] & HOUR_PM_MASK ? "PM" : "AM");

	mutex_lock(&info->lock);
	ret = s2mps25_bulk_write(info->i2c, S2MP_RTC_REG_SEC, NR_RTC_CNT_REGS,
			data);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write time reg(%d)\n", __func__,
			ret);
		goto out;
	}

	ret = s2m_rtc_update(info, S2M_RTC_WRITE_TIME);
out:
	mutex_unlock(&info->lock);
	return ret;
}

static int s2m_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	uint8_t data[NR_RTC_CNT_REGS];
	uint8_t reg, val;
	int ret;

	mutex_lock(&info->lock);
	ret = s2m_rtc_update(info, S2M_RTC_READ);
	if (ret < 0)
		goto out;

	ret = s2mps25_bulk_read(info->i2c, S2MP_RTC_REG_A0SEC, NR_RTC_CNT_REGS,
			data);
	if (ret < 0) {
		dev_err(info->dev, "%s:%d fail to read alarm reg(%d)\n",
			__func__, __LINE__, ret);
		goto out;
	}

	s2m_data_to_tm(data, &alrm->time);

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(%d)\n", __func__,
			alrm->time.tm_year + 1900, alrm->time.tm_mon + 1,
			alrm->time.tm_mday, alrm->time.tm_hour,
			alrm->time.tm_min, alrm->time.tm_sec,
			alrm->time.tm_wday);

	alrm->enabled = info->alarm_enabled;
	alrm->pending = 0;

	switch (info->iodev->device_type) {
	case S2MPS25X:
		reg = S2MPS25_REG_STATUS2;
		break;
	default:
		/* If this happens the core funtion has a problem */
		BUG();
	}

	ret = s2mps25_read_reg(info->pmic_i2c, reg, &val); /* i2c for PM */
	if (ret < 0) {
		dev_err(info->dev, "%s:%d fail to read STATUS2 reg(%d)\n",
			__func__, __LINE__, ret);
		goto out;
	}

	if (val & RTCA0E)
		alrm->pending = 1;
out:
	mutex_unlock(&info->lock);
	return ret;
}

static int s2m_rtc_set_alarm_enable(struct s2m_rtc_info *info, bool enabled)
{
	if (!info->use_irq)
		return -EPERM;

	if (enabled && !info->alarm_enabled) {
		info->alarm_enabled = true;
		enable_irq(info->irq);
	} else if (!enabled && info->alarm_enabled) {
		info->alarm_enabled = false;
		disable_irq(info->irq);
	}
	return 0;
}

static int s2m_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	uint8_t data[NR_RTC_CNT_REGS];
	int ret, i;

	mutex_lock(&info->lock);
	ret = s2m_tm_to_data(&alrm->time, data);
	if (ret < 0)
		goto out;

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(0x%02hhx)%s\n",
			__func__, data[RTC_YEAR] + 2000, data[RTC_MONTH],
			data[RTC_DATE], data[RTC_HOUR] & 0x1f, data[RTC_MIN],
			data[RTC_SEC], data[RTC_WEEKDAY],
			data[RTC_HOUR] & HOUR_PM_MASK ? "PM" : "AM");

	if (info->alarm_check) {
		for (i = 0; i < NR_RTC_CNT_REGS; i++)
			data[i] &= ~ALARM_ENABLE_MASK;

		ret = s2mps25_bulk_write(info->i2c, S2MP_RTC_REG_A0SEC, NR_RTC_CNT_REGS,
				data);
		if (ret < 0) {
			dev_err(info->dev, "%s: fail to disable alarm reg(%d)\n",
				__func__, ret);
			goto out;
		}

		ret = s2m_rtc_update(info, S2M_RTC_WRITE_ALARM);
		if (ret < 0)
			goto out;
	}

	for (i = 0; i < NR_RTC_CNT_REGS; i++)
		data[i] |= ALARM_ENABLE_MASK;

	ret = s2mps25_bulk_write(info->i2c, S2MP_RTC_REG_A0SEC, NR_RTC_CNT_REGS,
			data);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write alarm reg(%d)\n",
			__func__, ret);
		goto out;
	}

	ret = s2m_rtc_update(info, S2M_RTC_WRITE_ALARM);
	if (ret < 0)
		goto out;

	ret = s2m_rtc_set_alarm_enable(info, alrm->enabled);
out:
	mutex_unlock(&info->lock);
	return ret;
}

static int s2m_rtc_alarm_irq_enable(struct device *dev,
					unsigned int enabled)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&info->lock);
	ret = s2m_rtc_set_alarm_enable(info, enabled);
	mutex_unlock(&info->lock);
	return ret;
}

static int s2m_rtc_wake_lock_timeout(struct device *dev, unsigned int msec)
{
	struct wakeup_source *ws = NULL;

	if (!dev->power.wakeup) {
		pr_err("%s: Not register wakeup source\n", __func__);
		goto err;
	}

	ws = dev->power.wakeup;
	__pm_wakeup_event(ws, msec);

	return 0;
err:
	return -1;
}

static irqreturn_t s2m_rtc_alarm_irq(int irq, void *data)
{
	struct s2m_rtc_info *info = data;

	if (!info->rtc_dev)
		return IRQ_HANDLED;

	dev_info(info->dev, "[PMIC] %s: irq(%d)\n", __func__, irq);
	rtc_update_irq(info->rtc_dev, 1, RTC_IRQF | RTC_AF);

	if (s2m_rtc_wake_lock_timeout(info->dev, 500) < 0)
		return IRQ_NONE;

	return IRQ_HANDLED;
}
#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
static int s2m_rtc_stop_boot_alarm0(struct s2m_rtc_info *info)
{
	uint8_t data[7];
	int ret, i;
	struct rtc_time tm;

	ret = s2mps25_bulk_read(info->i2c, S2MP_RTC_REG_A0SEC, NR_RTC_CNT_REGS, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, &tm);

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(%d)\n", __func__,
			1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	for (i = 0; i < 7; i++)
		data[i] &= ~ALARM_ENABLE_MASK;

	ret = s2mps25_bulk_write(info->i2c, S2MP_RTC_REG_A0SEC, NR_RTC_CNT_REGS, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_update(info, S2M_RTC_WRITE_ALARM);

	return ret;
}

static int s2m_rtc_stop_boot_alarm(struct s2m_rtc_info *info)
{
	uint8_t data[7];
	int ret, i;
	struct rtc_time tm;

	ret = s2mps25_bulk_read(info->i2c, S2MP_RTC_REG_A1SEC, NR_RTC_CNT_REGS, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, &tm);

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(%d)\n", __func__,
			1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	for (i = 0; i < 7; i++)
		data[i] &= ~ALARM_ENABLE_MASK;

	ret = s2mps25_bulk_write(info->i2c, S2MP_RTC_REG_A1SEC, NR_RTC_CNT_REGS, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_update(info, S2M_RTC_WRITE_ALARM);

	return ret;
}

static int s2m_rtc_start_boot_alarm(struct s2m_rtc_info *info)
{
	int ret;
	uint8_t data[7];
	struct rtc_time tm;

	ret = s2mps25_bulk_read(info->i2c, S2MP_RTC_REG_A1SEC, NR_RTC_CNT_REGS, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, &tm);

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(%d)\n", __func__,
			1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	data[RTC_SEC] |= ALARM_ENABLE_MASK;
	data[RTC_MIN] |= ALARM_ENABLE_MASK;
	data[RTC_HOUR] |= ALARM_ENABLE_MASK;
	data[RTC_WEEKDAY] &= 0x00;

	if (data[RTC_DATE] & 0x1F)
		data[RTC_DATE] |= ALARM_ENABLE_MASK;
	if (data[RTC_MONTH] & 0x0F)
		data[RTC_MONTH] |= ALARM_ENABLE_MASK;
	if (data[RTC_YEAR] & 0x7F)
		data[RTC_YEAR] |= ALARM_ENABLE_MASK;

	ret = s2mps25_bulk_write(info->i2c, S2MP_RTC_REG_A1SEC, NR_RTC_CNT_REGS, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_update(info, S2M_RTC_WRITE_ALARM);

	return ret;
}

static int s2m_rtc_read_boot_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	uint8_t data[NR_RTC_CNT_REGS];
	uint8_t reg, val;
	int ret;

	mutex_lock(&info->lock);
	ret = s2m_rtc_update(info, S2M_RTC_READ);
	if (ret < 0)
		goto out;
	ret = s2mps25_bulk_read(info->i2c, S2MP_RTC_REG_A1SEC, NR_RTC_CNT_REGS,	data);
	if (ret < 0) {
		dev_err(info->dev, "%s: %d fail to read alarm1 reg(%d)\n", __func__, __LINE__, ret);
		goto out;
	}

	s2m_data_to_tm(data, &alrm->time);
	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(%d)\n", __func__,
			alrm->time.tm_year + 1900, alrm->time.tm_mon + 1,
			alrm->time.tm_mday, alrm->time.tm_hour,
			alrm->time.tm_min, alrm->time.tm_sec,
			alrm->time.tm_wday);

	//alrm->enabled = info->alarm_enabled;
	alrm->enabled = info->boot_alarm_enabled; /*This is not bootalarm status, it is alarm status*/
	alrm->pending = 0;

	switch (info->iodev->device_type) {
	case S2MPS25X:
		reg = S2MPS25_REG_STATUS2;
		break;
	default:
		BUG();
	}

	ret = s2mps25_read_reg(info->pmic_i2c, reg, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s: %d fail to read STATUS2 reg(%d)\n", __func__, __LINE__, ret);
		goto out;
	}

	if (val & RTCA1E)
		alrm->pending = 1;

out:
	mutex_unlock(&info->lock);
	return ret;
}

/*original s2m_rtc_set_boot_alarm() function*/
static int _s2m_rtc_set_boot_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	uint8_t data[7];
	int ret;

	mutex_lock(&info->lock);

	ret = s2m_tm_to_data(&alrm->time, data);
	if (ret < 0)
		goto out;

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(0x%02x)%s, [%s]\n",
			__func__, data[RTC_YEAR] + 2000, data[RTC_MONTH],
			data[RTC_DATE], data[RTC_HOUR] & 0x1F, data[RTC_MIN],
			data[RTC_SEC], data[RTC_WEEKDAY],
			data[RTC_HOUR] & HOUR_PM_MASK ? "PM" : "AM",
			alrm->enabled ? "enabled" : "disabled");

	ret = s2m_rtc_stop_boot_alarm(info);
	if (ret < 0)
		return ret;

	ret = s2mps25_read_reg(info->i2c, S2MP_RTC_REG_UPDATE, &info->update_reg);
	if (ret < 0) {
		dev_err(info->dev, "%s: read fail\n", __func__);
		return ret;
	}
	if (alrm->enabled)
		info->update_reg |= RTC_WAKE_MASK;
	else
		info->update_reg &= ~RTC_WAKE_MASK;

	ret = s2mps25_write_reg(info->i2c, S2MP_RTC_REG_UPDATE, (char)info->update_reg);
	if (ret < 0)
		dev_err(info->dev, "%s: fail to write update reg(%d)\n", __func__, ret);
	else
		usleep_range(1000, 1005);

	ret = s2mps25_bulk_write(info->i2c, S2MP_RTC_REG_A1SEC, NR_RTC_CNT_REGS, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_update(info, S2M_RTC_WRITE_ALARM);
	if (ret < 0)
		return ret;

	if (alrm->enabled)
		ret = s2m_rtc_start_boot_alarm(info);

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int _s2m_rtc_disable_boot_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&info->lock);
	/*stop alarm*/
	ret = s2m_rtc_stop_boot_alarm(info);
	if (ret < 0) {
		dev_err(info->dev, "%s: stop_boot_alarm failed\n", __func__);
		goto out;
	}

	ret = s2mps25_read_reg(info->i2c, S2MP_RTC_REG_UPDATE, &info->update_reg);
	if (ret < 0) {
		dev_err(info->dev, "%s: read fail\n", __func__);
		goto out;
	}

	/*disable RTCWAKE*/
	info->update_reg &= ~RTC_WAKE_MASK;

	ret = s2mps25_write_reg(info->i2c, S2MP_RTC_REG_UPDATE, (char)info->update_reg);
	if (ret < 0)
		dev_err(info->dev, "%s: fail to write update reg(%d)\n", __func__, ret);
out:
	mutex_unlock(&info->lock);
	return ret;
}

static int s2m_rtc_set_boot_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	int ret;
	struct s2m_rtc_info *info = dev_get_drvdata(dev);

	info->boot_alarm_enabled = alrm->enabled;
	/*if get disabled bootalarm, directly stop, do not set time*/
	if (!alrm->enabled)
		ret = _s2m_rtc_disable_boot_alarm(dev, alrm);
	else
		ret = _s2m_rtc_set_boot_alarm(dev, alrm);

	return ret;
}

static irqreturn_t s2m_rtc_boot_alarm_irq(int irq, void *data)
{
	struct s2m_rtc_info *info = data;

	dev_info(info->dev, "[PMIC] %s: irq(%d)\n", __func__, irq);

	rtc_update_irq(info->rtc_dev, 1, RTC_IRQF | RTC_AF);
	if (s2m_rtc_wake_lock_timeout(info->dev, 500) < 0)
		return IRQ_NONE;

	dev_info(info->dev, "[PMIC] %s: is_charging_mode(%d)\n", __func__, is_charging_mode);
	if (info->boot_alarm_enabled) {
		if (is_charging_mode)
			kernel_restart(NULL);
		else {
			info->boot_alarm_enabled = 0;
			info->update_reg &= ~RTC_WAKE_MASK;
			s2mps25_write_reg(info->i2c, S2MP_RTC_REG_UPDATE, (char)info->update_reg);
		}
	}

	return IRQ_HANDLED;
}

static ssize_t bootalarm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t retval;
	struct rtc_wkalrm alarm;
	struct rtc_device *rtc = to_rtc_device(dev);

	/* Don't show disabled alarms.  For uniformity, RTC alarms are
	 * conceptually one-shot, even though some common RTCs (on PCs)
	 * don't actually work that way.
	 *
	 * NOTE: RTC implementations where the alarm doesn't match an
	 * exact YYYY-MM-DD HH:MM[:SS] date *must* disable their RTC
	 * alarms after they trigger, to ensure one-shot semantics.
	 */
	retval = mutex_lock_interruptible(&rtc->ops_lock);
	if (retval)
		return retval;

	retval = s2m_rtc_read_boot_alarm(rtc->dev.parent, &alarm);
	mutex_unlock(&rtc->ops_lock);
	if (retval == 0) {
		retval = sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d(%d) [%s, %s]\n",
				alarm.time.tm_year + 1900, alarm.time.tm_mon + 1, alarm.time.tm_mday,
				alarm.time.tm_hour, alarm.time.tm_min, alarm.time.tm_sec,
				alarm.time.tm_wday,
				((alarm.enabled == 1) ? "Enable" : "Disable"),
				((alarm.pending == 1) ? "Pending" : "Not Pending"));
	}

	return retval;
}

static ssize_t bootalarm_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	ssize_t retval;
	unsigned long now, alrm_long;
	struct rtc_wkalrm alarm;
	struct rtc_device *rtc = to_rtc_device(dev);
	char *buf_ptr;
	int adjust = 0;
	int ret;

	/* Only request alarms to be triggered in the future.
	 * by write another time, e.g. 0 meaning Jan 1 1970 UTC.
	 */
	retval = rtc_read_time(rtc, &alarm.time);
	if (retval < 0)
		return retval;
	rtc_tm_to_time(&alarm.time, &now);

	buf_ptr = (char *)buf;
	if (*buf_ptr == '+') {
		buf_ptr++;
		adjust = 1;
	}

	ret = kstrtoul(buf_ptr, 0, &alrm_long);
	if (ret) {
		pr_info("%s: kstrtoul error\n", __func__);
		return -1;
	}
	if (alrm_long == 0)
		alarm.enabled = 0;
	else
		alarm.enabled = 1;
	if (adjust)
		alrm_long += now;

	rtc_time_to_tm(alrm_long, &alarm.time);

	retval = mutex_lock_interruptible(&rtc->ops_lock);
	if (retval)
		return retval;

	retval = s2m_rtc_set_boot_alarm(rtc->dev.parent, &alarm);
	mutex_unlock(&rtc->ops_lock);

	return (retval < 0) ? retval : n;
}

static DEVICE_ATTR_RW(bootalarm);
static struct attribute *s2mps25_rtc_attrs[] = {
	&dev_attr_bootalarm.attr,
	NULL
};

static const struct attribute_group s2mps25_rtc_sysfs_files = {
	.attrs	= s2mps25_rtc_attrs,
};
#endif

static const struct rtc_class_ops s2m_rtc_ops = {
	.read_time = s2m_rtc_read_time,
	.set_time = s2m_rtc_set_time,
	.read_alarm = s2m_rtc_read_alarm,
	.set_alarm = s2m_rtc_set_alarm,
	.alarm_irq_enable = s2m_rtc_alarm_irq_enable,
};

static void s2m_rtc_optimize_osc(struct s2m_rtc_info *info,
						struct s2mps25_platform_data *pdata)
{
	int ret = 0;

	/* edit option for OSC_BIAS_UP */
	if (pdata->osc_bias_up >= 0) {
		ret = s2mps25_update_reg(info->i2c, S2MP_RTC_REG_CAPSEL,
			pdata->osc_bias_up << OSC_BIAS_UP_SHIFT,
			OSC_BIAS_UP_MASK);
		if (ret < 0) {
			dev_err(info->dev, "%s: fail to write OSC_BIAS_UP(%d)\n",
				__func__, pdata->osc_bias_up);
			return;
		}
	}

	/* edit option for CAP_SEL */
	if (pdata->cap_sel >= 0) {
		ret = s2mps25_update_reg(info->i2c, S2MP_RTC_REG_CAPSEL,
			pdata->cap_sel << CAP_SEL_SHIFT, CAP_SEL_MASK);
		if (ret < 0) {
			dev_err(info->dev, "%s: fail to write CAP_SEL(%d)\n",
			__func__, pdata->cap_sel);
			return;
		}
	}

	/* edit option for OSC_CTRL */
	if (pdata->osc_xin >= 0) {
		ret = s2mps25_update_reg(info->i2c, S2MP_RTC_REG_OSCCTRL,
			pdata->osc_xin << OSC_XIN_SHIFT, OSC_XIN_MASK);
		if (ret < 0) {
			dev_err(info->dev, "%s: fail to write OSC_CTRL(%d)\n",
			__func__,  pdata->osc_xin);
			return;
		}
	}
	if (pdata->osc_xout >= 0) {
		ret = s2mps25_update_reg(info->i2c, S2MP_RTC_REG_OSCCTRL,
			pdata->osc_xout << OSC_XOUT_SHIFT, OSC_XOUT_MASK);
		if (ret < 0) {
			dev_err(info->dev, "%s: fail to write OSC_CTRL(%d)\n",
			__func__, pdata->osc_xout);
			return;
		}
	}
}

static bool s2m_is_jigonb_low(struct s2m_rtc_info *info)
{
	int ret, reg;
	uint8_t val, mask;

	switch (info->iodev->device_type) {
	case S2MPS25X:
		reg = S2MPS25_REG_STATUS1;
		mask = BIT(1);
		break;
	default:
		BUG();
	}

	ret = s2mps25_read_reg(info->pmic_i2c, reg, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read status1 reg(%d)\n",
			__func__, ret);
		return false;
	}

	return !(val & mask);
}

static irqreturn_t s2m_smpl_warn_irq_handler(int irq, void *data)
{
	struct s2m_rtc_info *info = data;

	if (!info->rtc_dev)
		return IRQ_HANDLED;

	if (gpio_get_value(info->smpl_warn_info) & 0x1)
		return IRQ_HANDLED;

	dev_info(info->dev, "[PMIC] %s: SMPL_WARN\n", __func__);

	disable_irq_nosync(info->smpl_irq);

	queue_delayed_work(system_freezable_wq, &info->irq_work,
			msecs_to_jiffies(100));

	return IRQ_HANDLED;
}
#if IS_ENABLED(CONFIG_SOC_EXYNOS9830_EVT0)
void exynos9830_smpl_warn_sw_release(void);
#endif
static void exynos_smpl_warn_work(struct work_struct *work)
{
	struct s2m_rtc_info *info = container_of(work,
			struct s2m_rtc_info, irq_work.work);
	int state = 0;

	state = (gpio_get_value(info->smpl_warn_info) & 0x1);

	if (!state) {
		sec_pm_cpufreq_throttle_by_one_step();
		queue_delayed_work(system_freezable_wq, &info->irq_work,
				msecs_to_jiffies(10));
	} else {
		dev_info(info->dev, "%s: SMPL_WARN polling End!\n", __func__);
		sec_pm_cpufreq_unthrottle();
#if IS_ENABLED(CONFIG_SOC_EXYNOS9830_EVT0)
		exynos9830_smpl_warn_sw_release();
#endif
		enable_irq(info->smpl_irq);

#if IS_ENABLED(CONFIG_SEC_PM)
		info->smpl_warn_cnt++;
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
		secdbg_exin_set_smpl(info->smpl_warn_cnt);
#endif
#endif /* CONFIG_SEC_PM */
	}
}

static void s2m_rtc_enable_wtsr_smpl(struct s2m_rtc_info *info,
						struct s2mps25_platform_data *pdata)
{
	uint8_t val;
	int ret;

	if (pdata->wtsr_smpl->check_jigon && s2m_is_jigonb_low(info))
		pdata->wtsr_smpl->smpl_en = false;

	val = (pdata->wtsr_smpl->wtsr_en << WTSR_EN_SHIFT)
		| (pdata->wtsr_smpl->smpl_en << SMPL_EN_SHIFT)
		| WTSR_TIMER_BITS(pdata->wtsr_smpl->wtsr_timer_val)
		| SMPL_TIMER_BITS(pdata->wtsr_smpl->smpl_timer_val);

	dev_info(info->dev, "[PMIC] %s: WTSR: %s, SMPL: %s\n", __func__,
			pdata->wtsr_smpl->wtsr_en ? "enable" : "disable",
			pdata->wtsr_smpl->smpl_en ? "enable" : "disable");

	ret = s2mps25_write_reg(info->i2c, S2MP_RTC_REG_WTSR_SMPL, val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write WTSR/SMPL reg(%d)\n",
				__func__, ret);
		return;
	}
	info->wtsr_en = pdata->wtsr_smpl->wtsr_en;
	info->smpl_en = pdata->wtsr_smpl->smpl_en;

}

static void s2m_rtc_disable_wtsr_smpl(struct s2m_rtc_info *info,
					struct s2mps25_platform_data *pdata)
{
	int ret;

	dev_info(info->dev, "[PMIC] %s: disable SMPL & WTSR\n", __func__);
	ret = s2mps25_update_reg(info->i2c, S2MP_RTC_REG_WTSR_SMPL, 0,
			WTSR_EN_MASK | SMPL_EN_MASK);
	if (ret < 0)
		dev_err(info->dev, "%s: fail to update WTSR reg(%d)\n",
				__func__, ret);
}

static int s2m_rtc_init_reg(struct s2m_rtc_info *info,
				struct s2mps25_platform_data *pdata)
{
	uint8_t data, update_val, ctrl_val, capsel_val;
	int ret;

#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
	uint8_t data_alrm1[7];
	struct rtc_time alrm, now;
	unsigned long now_int, alrm_int;

	ret = s2mps25_bulk_read(info->i2c, S2MP_RTC_REG_A1SEC, NR_RTC_CNT_REGS, data_alrm1);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data_alrm1, &alrm);

	dev_info(info->dev, "%s [boot alarm]: %d-%02d-%02d %02d:%02d:%02d(%d)\n", __func__,
				alrm.tm_year + 1900, alrm.tm_mon + 1,
				alrm.tm_mday, alrm.tm_hour,
				alrm.tm_min, alrm.tm_sec,
				alrm.tm_wday);
#endif

	ret = s2mps25_read_reg(info->i2c, S2MP_RTC_REG_UPDATE, &update_val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read update reg(%d)\n",
			__func__, ret);
		return ret;
	}

#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
	info->boot_alarm_enabled = (update_val & RTC_WAKE_MASK) ? 1 : 0;
	if (info->boot_alarm_enabled) {
		s2m_rtc_read_time(info->dev, &now);
		rtc_tm_to_time(&now, &now_int);
		rtc_tm_to_time(&alrm, &alrm_int);
		if (now_int >= alrm_int) {
			info->boot_alarm_enabled = 0;
			update_val &= ~RTC_WAKE_MASK;
		}
	}
#endif
	info->update_reg =
		update_val & ~(info->wudr_mask | RTC_FREEZE_MASK | RTC_RUDR_MASK | info->audr_mask);

	ret = s2mps25_write_reg(info->i2c, S2MP_RTC_REG_UPDATE, info->update_reg);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
			__func__, ret);
		return ret;
	}

	s2m_rtc_update(info, S2M_RTC_READ);

	ret = s2mps25_read_reg(info->i2c, S2MP_RTC_REG_CTRL, &ctrl_val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read control reg(%d)\n",
			__func__, ret);
		return ret;
	}

	ret = s2mps25_read_reg(info->i2c, S2MP_RTC_REG_CAPSEL, &capsel_val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read cap_sel reg(%d)\n",
			__func__, ret);
		return ret;
	}

	/* If the value of RTC_CTRL register is 0, RTC registers were reset */
	if ((ctrl_val & MODEL24_MASK) && ((capsel_val & 0xf0) == 0xf0))
		return 0;

#if IS_ENABLED(CONFIG_SEC_PM)
	info->is_rtc_cleared = true;
#endif /* CONFIG_SEC_PM */

	/* Set RTC control register : Binary mode, 24hour mode */
	data = MODEL24_MASK;
	ret = s2mps25_write_reg(info->i2c, S2MP_RTC_REG_CTRL, data);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write CTRL reg(%d)\n",
			__func__, ret);
		return ret;
	}

	ret = s2m_rtc_update(info, S2M_RTC_WRITE_ALARM);
	if (ret < 0)
		return ret;

	capsel_val |= 0xf0;
	ret = s2mps25_write_reg(info->i2c, S2MP_RTC_REG_CAPSEL, capsel_val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write CAP_SEL reg(%d)\n",
				__func__, ret);
		return ret;
	}

	if (pdata->init_time) {
		dev_info(info->dev, "%s: initialize RTC time\n", __func__);
		ret = s2m_rtc_set_time(info->dev, pdata->init_time);
	} else
		dev_info(info->dev, "%s: RTC initialize is not operated: This causes a weekday problem\n", __func__);

	return ret;
}

#if IS_ENABLED(CONFIG_SEC_PM)
static ssize_t rtc_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", info->is_rtc_cleared);
}
static DEVICE_ATTR_RO(rtc_status);

static ssize_t smpl_warn_cnt_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	int cnt = info->smpl_warn_cnt;

	info->smpl_warn_cnt = 0;

	return sprintf(buf, "%d\n", cnt);
}
static DEVICE_ATTR_RO(smpl_warn_cnt);

static struct attribute *pmic_rtc_attributes[] = {
	&dev_attr_rtc_status.attr,
	&dev_attr_smpl_warn_cnt.attr,
	NULL
};

static const struct attribute_group pmic_rtc_attr_group = {
	.attrs = pmic_rtc_attributes,
};
#endif /* CONFIG_SEC_PM */

#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
static int s2m_rtc_init_boot_alarm(struct platform_device *pdev,
				   struct s2m_rtc_info *info, int irq_base)
{
	int ret = 0;

	switch (info->iodev->device_type) {
	case S2MPS25X:
		info->boot_alarm_irq = irq_base + S2MPS25_PMIC_IRQ_RTCA1_INT2;
		break;
	default:
		/* If this happens the core funtion has a problem */
		BUG();
	}

	ret = exynos_pmu_read(POWER_SYSIP_INFORM3, &is_charging_mode);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to get charge mode: %d\n", ret);
		goto err;
	}

	/* Set a VGPIO for ALARM1 IRQ */
	ret = devm_request_threaded_irq(&pdev->dev, info->boot_alarm_irq, NULL,
					s2m_rtc_boot_alarm_irq, 0, "rtc-alarm1", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request alarm1 IRQ: %d: %d\n",
			info->boot_alarm_irq, ret);
		goto err;
	}

	ret = rtc_add_group(info->rtc_dev, &s2mps25_rtc_sysfs_files);
	if (ret < 0) {
		devm_free_irq(&pdev->dev, info->boot_alarm_irq, info);
		dev_err(&pdev->dev, "Failed to creat sysfs: %d\n", ret);
		goto err;
	}

	return 0;
err:
	return -1;
}
#endif

static int s2m_rtc_set_interrupt(struct platform_device *pdev,
				 struct s2mps25_platform_data *pdata,
				 struct s2m_rtc_info *info, int irq_base)
{
	int ret = 0;

	if (!irq_base) {
		dev_err(&pdev->dev, "[PMIC] %s: Failed to get irq base %d\n",
			__func__, irq_base);
		return -ENODEV;
	}

	switch (info->iodev->device_type) {
	case S2MPS25X:
		info->irq = irq_base + S2MPS25_PMIC_IRQ_RTCA0_INT2;
		break;
	default:
		/* If this happens the core funtion has a problem */
		BUG();
	}

	/* Set a VGPIO for ALARM0 IRQ */
	ret = devm_request_threaded_irq(&pdev->dev, info->irq, NULL,
					s2m_rtc_alarm_irq, 0, "rtc-alarm0", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: Failed to request alarm IRQ: %d: %d\n",
			__func__, info->irq, ret);
		return ret;
	}

	disable_irq(info->irq);
	disable_irq(info->irq);
	info->use_irq = true;

	/* Set a GPIO pin for SMPL IRQ */
	if (pdata->smpl_warn_en) {
		if (!gpio_is_valid(pdata->smpl_warn)) {
			dev_err(&pdev->dev, "smpl_warn GPIO NOT VALID\n");
			return -1;
		}

		INIT_DELAYED_WORK(&info->irq_work, exynos_smpl_warn_work);

		info->smpl_irq = gpio_to_irq(pdata->smpl_warn);

		irq_set_status_flags(info->smpl_irq, IRQ_DISABLE_UNLAZY);

		ret = devm_request_threaded_irq(&pdev->dev, info->smpl_irq,
			s2m_smpl_warn_irq_handler, NULL,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, "SMPL WARN", info);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to request smpl warn IRQ: %d: %d\n",
				info->smpl_irq, ret);
			return -1;
		}

		info->smpl_warn_info = pdata->smpl_warn;
	}

	enable_irq(info->irq);

	return 0;
}

static int s2m_rtc_probe(struct platform_device *pdev)
{
	struct s2mps25_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mps25_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct s2m_rtc_info *info;
	int ret = 0;

	pr_info("[PMIC] %s: start\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(struct s2m_rtc_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = &pdev->dev;
	info->iodev = iodev;
	info->i2c = iodev->rtc;
	info->pmic_i2c = iodev->pmic1;
	info->alarm_check = true;

	mutex_init(&info->lock);
	platform_set_drvdata(pdev, info);

	ret = s2m_rtc_init_reg(info, pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to initialize RTC reg:%d\n", ret);
		goto err_rtc_init_reg;
	}

	if (pdata->wtsr_smpl)
		s2m_rtc_enable_wtsr_smpl(info, pdata);

	s2m_rtc_optimize_osc(info, pdata);

	ret = device_init_wakeup(&pdev->dev, true);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: device_init_wakeup fail(%d)\n", __func__, ret);
		goto err_rtc_init_reg;
	}

	info->rtc_dev = devm_rtc_device_register(&pdev->dev, "s2m-rtc", &s2m_rtc_ops, THIS_MODULE);
	if (IS_ERR(info->rtc_dev)) {
		dev_err(&pdev->dev, "%s: devm_rtc_device_register failed: %d\n", __func__, ret);
		ret = PTR_ERR(info->rtc_dev);
		goto err_rtc_init_reg;
	}

	ret = s2m_rtc_set_interrupt(pdev, pdata, info, pdata->irq_base);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: s2m_rtc_set_interrupt failed(%d)\n", __func__, ret);
		goto err_rtc_init_reg;
	}

#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
	ret = s2m_rtc_init_boot_alarm(pdev, info, pdata->irq_base);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: s2m_rtc_init_boot_alarm failed: %d\n", __func__, ret);
		goto err_rtc_init_reg;
	}
#endif

	/* Set secure debug */
	static_info = info;
	ret = secure_debug_set_config(RTC_REG_SECURE1, RTC_BOOTING);
	if (ret < 0) {
		pr_err("%s: secure_debug_set_config failed\n", __func__);
		goto err_rtc_init_reg;
	}

#if IS_ENABLED(CONFIG_SEC_PM)
	info->pmic_rtc_dev = sec_device_create(info, "rtc");

	ret = sysfs_create_group(&info->pmic_rtc_dev->kobj, &pmic_rtc_attr_group);
	if (ret)
		dev_err(&pdev->dev, "failed to create pmic_rtc sysfs group\n");
#endif /* CONFIG_SEC_PM */

	pr_info("[PMIC] %s: end\n", __func__);

	return 0;

err_rtc_init_reg:
	mutex_destroy(&info->lock);

	return ret;
}

static int s2m_rtc_remove(struct platform_device *pdev)
{
	struct s2m_rtc_info *info = platform_get_drvdata(pdev);

	if (!info->alarm_enabled)
		enable_irq(info->irq);
	if (info->dev->power.wakeup)
		device_init_wakeup(&pdev->dev, false);
	mutex_destroy(&info->lock);
#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
	devm_free_irq(&pdev->dev, info->boot_alarm_irq, info);
#endif

	return 0;
}

static void s2m_rtc_shutdown(struct platform_device *pdev)
{
	/*disable wtsr, smpl */
	struct s2m_rtc_info *info = platform_get_drvdata(pdev);
	struct s2mps25_platform_data *pdata = dev_get_platdata(info->iodev->dev);

	if (info->wtsr_en || info->smpl_en)
		s2m_rtc_disable_wtsr_smpl(info, pdata);
#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
	s2m_rtc_stop_boot_alarm0(info);
#endif

	/* Set secure debug */
	if (secure_debug_set_config(RTC_REG_SECURE1, RTC_SHUTDOWN) < 0)
		pr_err("%s: secure_debug_set_config failed\n", __func__);
}

static const struct platform_device_id s2m_rtc_id[] = {
	{ "s2mps25-rtc", 0 },
	{},
};

static int s2mps25_rtc_suspend(struct device *dev)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	struct rtc_time tm;
	int ret;

	ret = s2m_rtc_read_time(dev, &tm);
	if (ret < 0)
		dev_err(info->dev, "%s: fail to read rtc time\n",
			__func__);

	/* Set secure debug */
	if (secure_debug_set_config(RTC_REG_SECURE1, RTC_SUSPEND) < 0)
		pr_err("%s: secure_debug_set_config failed\n", __func__);

	return 0;
}

static int s2mps25_rtc_resume(struct device *dev)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	struct rtc_time tm;
	int ret;

	ret = s2m_rtc_read_time(dev, &tm);
	if (ret < 0)
		dev_err(info->dev, "%s: fail to read rtc time\n",
			__func__);

	/* Set secure debug */
	if (secure_debug_set_config(RTC_REG_SECURE1, RTC_RESUME) < 0)
		pr_err("%s: secure_debug_set_config failed\n", __func__);

	return 0;
}

static SIMPLE_DEV_PM_OPS(s2mps25_rtc_pm, s2mps25_rtc_suspend, s2mps25_rtc_resume);

static struct platform_driver s2m_rtc_driver = {
	.driver		= {
		.name	= "s2mps25-rtc",
		.owner	= THIS_MODULE,
		.pm	= &s2mps25_rtc_pm,
	},
	.probe		= s2m_rtc_probe,
	.remove		= s2m_rtc_remove,
	.shutdown	= s2m_rtc_shutdown,
	.id_table	= s2m_rtc_id,
};

module_platform_driver(s2m_rtc_driver);

/* Module information */
MODULE_DESCRIPTION("Samsung RTC driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
