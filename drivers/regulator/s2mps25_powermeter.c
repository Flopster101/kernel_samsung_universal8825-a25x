/*
 * s2mps25-powermeter.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mfd/samsung/s2mps25.h>
#include <linux/mfd/samsung/s2mps25-regulator.h>
#include <linux/platform_device.h>
#include <linux/regulator/pmic_class.h>
#include <linux/interrupt.h>

/* Power-meter registers */
#define ADC_INTP		(0x00)
#define ADC_INTM		(0x01)
#define ADC_CTRL0		(0x03)
#define ADC_CTRL1		(0x04)
#define ADC_CTRL2		(0x05)
#define ADC_CTRL3		(0x06)
#define PERIOD_CTRL0		(0x07)
#define PERIOD_CTRL1		(0x08)
#define MUX0SEL			(0x09)
#define MUX1SEL			(0x0A)
#define MUX2SEL			(0x0B)
#define MUX3SEL			(0x0C)
#define MUX4SEL			(0x0D)
#define MUX5SEL			(0x0E)
#define MUX6SEL			(0x0F)
#define MUX7SEL			(0x10)
#define MUX8SEL			(0x11)
#define TH_CTRL0		(0x13)
#define TH_CTRL1		(0x14)
#define TH_CTRL2		(0x15)
#define TH_CTRL3		(0x16)
#define ADC0_ACC0		(0x1F)
#define ADC1_ACC0		(0x24)
#define ADC2_ACC0		(0x29)
#define ADC3_ACC0		(0x2E)
#define ADC4_ACC0		(0x33)
#define ADC5_ACC0		(0x38)
#define ADC6_ACC0		(0x3D)
#define ADC7_ACC0		(0x42)
#define ACC_COUNT_L		(0x47)
#define ACC_COUNT_H		(0x48)
#define PM0_LPF_DATA_L		(0x49)
#define PM0_LPF_DATA_H		(0x4A)
#define PM1_LPF_DATA_L		(0x4B)
#define PM1_LPF_DATA_H		(0x4C)
#define PM2_LPF_DATA_L		(0x4D)
#define PM2_LPF_DATA_H		(0x4E)
#define PM3_LPF_DATA_L		(0x4F)
#define PM3_LPF_DATA_H		(0x50)
#define PM4_LPF_DATA_L		(0x51)
#define PM4_LPF_DATA_H		(0x52)
#define PM5_LPF_DATA_L		(0x53)
#define PM5_LPF_DATA_H		(0x54)
#define PM6_LPF_DATA_L		(0x55)
#define PM6_LPF_DATA_H		(0x56)
#define PM7_LPF_DATA_L		(0x57)
#define PM7_LPF_DATA_H		(0x58)
#define NTC0_DATA_L		(0x59)
#define NTC0_DATA_H		(0x5A)
#define NTC1_DATA_L		(0x5B)
#define NTC1_DATA_H		(0x5C)
#define ADC_CONFIG		(0x5F)

#define NTC_DONE_MASK		(0x02)
#define NTC_EN_MASK		(0x06)
#define NTC_SMP_NUM_32		(0x05)
#define NTC_SMP_NUM_64		(0x06)
#define NTC_SMP_MASK		(0x07)
#define NTC_MAX			(0x02)

/* Power-meter setting */
#define NANO_MICRO		(1000000) /* convert a nanometer into a micrometer for framework service */
#define MAX_ADC_OUTPUT		(5)
#define BUCK_START		(0x01)
#define BUCK_END		(0x0A)
#define ADC_ENABLE		(0x01)
#define ADC_DISABLE		(0x00)
#define ADCEN_MASK		(0x01)
#define ASYNC_RD		(0x02)

#define ADC_B1			(0x01)
#define ADC_B2			(0x02)
#define ADC_B3			(0x03)
#define ADC_B4			(0x04)
#define ADC_B5			(0x05)
#define ADC_B6			(0x06)
#define ADC_B7			(0x07)
#define ADC_B8			(0x08)
#define ADC_B9			(0x09)
#define ADC_B10			(0x0A)

#define POWER_B1		(9155300) /* unit: nW */
#define POWER_B2		(18311000)
#define POWER_B3		(18311000)
#define POWER_B4		(18311000)
#define POWER_B5		(18311000)
#define POWER_B6		(9155300)
#define POWER_B7		(9155300)
#define POWER_B8		(27465800)
#define POWER_B9		(27465800)
#define POWER_B10		(27465800)

struct adc_info {
	struct i2c_client *i2c;
	struct mutex adc_lock;
	uint64_t *power_val;
	uint8_t *adc_reg;
	uint8_t pmic_rev;

	/* NTC irq */
	int ntc_irq[NTC_MAX];
	int ntc_cnt[NTC_MAX];
	struct mutex irq_lock;
};

/* only use BUCK1~6 */
static const uint64_t power_coeffs[] =
	{POWER_B1, POWER_B2, POWER_B3, POWER_B4, POWER_B5,
	 POWER_B6, POWER_B7, POWER_B8, POWER_B9, POWER_B10};

enum s2mps25_adc_ch {
	ADC_CH0 = 0,
	ADC_CH1,
	ADC_CH2,
	ADC_CH3,
	ADC_CH4,
	ADC_CH5,
	ADC_CH6,
	ADC_CH7,
};

static const uint8_t adc_channel_reg[] = {
	[ADC_CH0] = ADC0_ACC0,
	[ADC_CH1] = ADC1_ACC0,
	[ADC_CH2] = ADC2_ACC0,
	[ADC_CH3] = ADC3_ACC0,
	[ADC_CH4] = ADC4_ACC0,
	[ADC_CH5] = ADC5_ACC0,
	[ADC_CH6] = ADC5_ACC0,
	[ADC_CH7] = ADC5_ACC0,
};

static const uint8_t adc_mux_val[] = {
	[ADC_B1]  = 0x01,
	[ADC_B2]  = 0x02,
	[ADC_B3]  = 0x02,
	[ADC_B4]  = 0x04,
	[ADC_B5]  = 0x04,
	[ADC_B6]  = 0x06,
	[ADC_B7]  = 0x07,
	[ADC_B8]  = 0x08,
	[ADC_B9]  = 0x08,
	[ADC_B10] = 0x08,
};

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static int adc_assign_mux_channel(struct adc_info *adc_meter);

static uint64_t adc_get_count(struct adc_info *adc_meter)
{
	struct i2c_client *i2c = adc_meter->i2c;
	uint8_t acc_count_l = 0, acc_count_h = 0;
	uint64_t count = 0;
	int ret = 0;

	ret = s2mps25_read_reg(i2c, ACC_COUNT_L, &acc_count_l);
	if (ret)
		pr_err("%s: failed to read register\n", __func__);

	ret = s2mps25_read_reg(i2c, ACC_COUNT_H, &acc_count_h);
	if (ret)
		pr_err("%s: failed to read register\n", __func__);

	count = (acc_count_h << 8) | (acc_count_l);

	return count;
}

static uint64_t adc_get_acc(struct adc_info *adc_meter, int chan)
{
	struct i2c_client *i2c = adc_meter->i2c;
	uint8_t adc_acc[MAX_ADC_OUTPUT] = {0};
	uint64_t acc = 0;
	int ret = 0;
	size_t i = 0;

	for (i = 0; i < MAX_ADC_OUTPUT; i++) {
		ret = s2mps25_read_reg(i2c, adc_channel_reg[chan] + i, adc_acc + i);
		if (ret) {
			pr_err("%s: failed to read register\n", __func__);
			break;
		}
	}

	acc = ((uint64_t)(adc_acc[4] & 0x3F) << 32) | (uint64_t)(adc_acc[3] << 24) |
	       (adc_acc[2] << 16) | (adc_acc[1] << 8) | (adc_acc[0]);

	return acc;
}

static uint64_t adc_get_pout(struct adc_info *adc_meter, int chan)
{
	uint64_t pout = 0, acc = 0, count = 0;

	acc = adc_get_acc(adc_meter, chan);
	count = adc_get_count(adc_meter);

	/* Calculate power output */
	pout = acc / count;

	return pout;
}

static int adc_check_async(struct adc_info *adc_meter)
{
	struct i2c_client *i2c = adc_meter->i2c;
	int ret = 0;
	uint8_t val = 0;

	ret = s2mps25_update_reg(i2c, ADC_CTRL0, ASYNC_RD, ASYNC_RD);
	if (ret)
		goto err;

	usleep_range(2000, 2100);
	ret = s2mps25_read_reg(i2c, ADC_CTRL0, &val);
	if (ret)
		goto err;

	pr_info("%s: check async clear(0x%02hhx)\n", __func__, val);

	if (val & ASYNC_RD)
		goto err;

	return 0;
err:
	return -1;
}

static int adc_read_data(struct adc_info *adc_meter, int channel)
{
	size_t i = 0;

	mutex_lock(&adc_meter->adc_lock);

	if (adc_check_async(adc_meter) < 0) {
		pr_err("%s: adc_check_async fail\n", __func__);
		mutex_unlock(&adc_meter->adc_lock);
		goto err;
	}

	if (channel < 0)
		for (i = 0; i < ARRAY_SIZE(adc_channel_reg); i++)
			adc_meter->power_val[i] = adc_get_pout(adc_meter, i);
	else
		adc_meter->power_val[channel] = adc_get_pout(adc_meter, channel);

	mutex_unlock(&adc_meter->adc_lock);

	return 0;
err:
	return -1;
}

static uint64_t get_coeff_p(uint8_t adc_reg_num)
{
	uint64_t coeff = 0;

	coeff = power_coeffs[adc_reg_num - BUCK_START];

	return coeff;
}

static ssize_t adc_val_power_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	size_t i = 0;
	int cnt = 0, chan = 0;

	for(i = 0; i < ARRAY_SIZE(adc_channel_reg); i++) {
		if ((adc_meter->adc_reg[i] < BUCK_START) &&
		    (adc_meter->adc_reg[i] > BUCK_END))
			return snprintf(buf, PAGE_SIZE,
					"Power-Meter supports only BUCK%d~%d\n",
					BUCK_START, BUCK_END);
	}

	if (adc_read_data(adc_meter, -1) < 0)
		goto err;

	for (i = 0; i < ARRAY_SIZE(adc_channel_reg); i++) {
		chan = ADC_CH0 + i;
		cnt += snprintf(buf + cnt, PAGE_SIZE, "CH%d[0x%02hhx]:%7llu uW (%7llu)  ",
				chan,
				adc_meter->adc_reg[chan],
				(adc_meter->power_val[chan] * get_coeff_p(adc_meter->adc_reg[chan])) / NANO_MICRO,
				adc_meter->power_val[chan]);

		if (i == ARRAY_SIZE(adc_channel_reg) / 2 - 1)
			cnt += snprintf(buf + cnt, PAGE_SIZE, "\n");
	}
	cnt += snprintf(buf + cnt, PAGE_SIZE, "\n");

	return cnt;
err:
	return snprintf(buf, PAGE_SIZE, "Not clear ASYNC_RD\n");
}

static ssize_t adc_en_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	int ret = 0;
	uint8_t val = 0;

	ret = s2mps25_read_reg(adc_meter->i2c, ADC_CTRL1, &val);
	if (ret)
		pr_err("%s: failed to read register\n", __func__);

	if (val & ADCEN_MASK)
		return snprintf(buf, PAGE_SIZE, "ADC enable(0x%02hhx)\n", val);
	else
		return snprintf(buf, PAGE_SIZE, "ADC disable(0x%02hhx)\n", val);
}

static ssize_t adc_en_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	int ret = 0;
	uint8_t temp = 0, val = 0;

	ret = kstrtou8(buf, 16, &temp);
	if (ret)
		return -EINVAL;

	if (temp == ADCEN_MASK)
		val = temp;

	ret = s2mps25_update_reg(adc_meter->i2c, ADC_CTRL1, val, ADCEN_MASK);
	if (ret)
		pr_err("%s: failed to update register\n", __func__);

	return count;
}

static int convert_adc_val(struct device *dev, char *buf, int channel)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);
	uint64_t coeff_p = get_coeff_p(adc_meter->adc_reg[channel]);

	if (adc_read_data(adc_meter, channel) < 0)
		return snprintf(buf, PAGE_SIZE, "Not clear ASYNC_RD\n");

	return snprintf(buf, PAGE_SIZE, "CH%d[0x%02hhx]:%7llu uW (%7llu)\n",
			channel, adc_meter->adc_reg[channel],
			(adc_meter->power_val[channel] * coeff_p) / NANO_MICRO,
			adc_meter->power_val[channel]);
}

static ssize_t adc_val_0_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH0);
}

static ssize_t adc_val_1_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH1);
}

static ssize_t adc_val_2_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH2);
}

static ssize_t adc_val_3_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH3);
}

static ssize_t adc_val_4_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH4);
}

static ssize_t adc_val_5_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH5);
}

static ssize_t adc_val_6_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH6);
}

static ssize_t adc_val_7_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, ADC_CH7);
}

static ssize_t adc_reg_0_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH0]);
}

static ssize_t adc_reg_1_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH1]);
}

static ssize_t adc_reg_2_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH2]);
}

static ssize_t adc_reg_3_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH3]);
}

static ssize_t adc_reg_4_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH4]);
}

static ssize_t adc_reg_5_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH5]);
}

static ssize_t adc_reg_6_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH6]);
}

static ssize_t adc_reg_7_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[ADC_CH7]);
}

static void adc_reg_update(struct adc_info *adc_meter)
{
	if (s2mps25_adc_set_enable(adc_meter, ADC_DISABLE) < 0)
		pr_err("%s: s2mps25_adc_set_enable fail\n", __func__);

	if (adc_assign_mux_channel(adc_meter) < 0)
		pr_err("%s: adc_assign_mux_channel fail\n", __func__);

	if (s2mps25_adc_set_enable(adc_meter, ADC_ENABLE) < 0)
		pr_err("%s: s2mps25_adc_set_enable fail\n", __func__);
}

static uint8_t buf_to_adc_reg(const char *buf)
{
	uint8_t adc_reg_num = 0;

	if (kstrtou8(buf, 16, &adc_reg_num))
		return 0;

	if (adc_reg_num >= BUCK_START && adc_reg_num <= BUCK_END)
		return adc_reg_num;
	else
		return 0;
}

static ssize_t assign_adc_reg(struct device *dev, const char *buf,
			      size_t count, int channel)
{
	struct adc_info *adc_meter = dev_get_drvdata(dev);

	uint8_t adc_reg_num = buf_to_adc_reg(buf);
	if (!adc_reg_num)
		return -EINVAL;
	else {
		adc_meter->adc_reg[channel] = adc_reg_num;
		adc_reg_update(adc_meter);
		return count;
	}
}

static ssize_t adc_reg_0_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH0);
}

static ssize_t adc_reg_1_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH1);
}

static ssize_t adc_reg_2_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH2);
}

static ssize_t adc_reg_3_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH3);
}

static ssize_t adc_reg_4_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH4);
}
static ssize_t adc_reg_5_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH5);
}
static ssize_t adc_reg_6_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH6);
}
static ssize_t adc_reg_7_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, ADC_CH7);
}

static struct pmic_device_attribute powermeter_attr[] = {
	PMIC_ATTR(power_val_all, S_IRUGO, adc_val_power_show, NULL),
	PMIC_ATTR(adc_en, S_IRUGO | S_IWUSR, adc_en_show, adc_en_store),
	PMIC_ATTR(adc_val_0, S_IRUGO, adc_val_0_show, NULL),
	PMIC_ATTR(adc_val_1, S_IRUGO, adc_val_1_show, NULL),
	PMIC_ATTR(adc_val_2, S_IRUGO, adc_val_2_show, NULL),
	PMIC_ATTR(adc_val_3, S_IRUGO, adc_val_3_show, NULL),
	PMIC_ATTR(adc_val_4, S_IRUGO, adc_val_4_show, NULL),
	PMIC_ATTR(adc_val_5, S_IRUGO, adc_val_5_show, NULL),
	PMIC_ATTR(adc_val_6, S_IRUGO, adc_val_6_show, NULL),
	PMIC_ATTR(adc_val_7, S_IRUGO, adc_val_7_show, NULL),
	PMIC_ATTR(adc_reg_0, S_IRUGO | S_IWUSR, adc_reg_0_show, adc_reg_0_store),
	PMIC_ATTR(adc_reg_1, S_IRUGO | S_IWUSR, adc_reg_1_show, adc_reg_1_store),
	PMIC_ATTR(adc_reg_2, S_IRUGO | S_IWUSR, adc_reg_2_show, adc_reg_2_store),
	PMIC_ATTR(adc_reg_3, S_IRUGO | S_IWUSR, adc_reg_3_show, adc_reg_3_store),
	PMIC_ATTR(adc_reg_4, S_IRUGO | S_IWUSR, adc_reg_4_show, adc_reg_4_store),
	PMIC_ATTR(adc_reg_5, S_IRUGO | S_IWUSR, adc_reg_5_show, adc_reg_5_store),
	PMIC_ATTR(adc_reg_6, S_IRUGO | S_IWUSR, adc_reg_6_show, adc_reg_6_store),
	PMIC_ATTR(adc_reg_7, S_IRUGO | S_IWUSR, adc_reg_7_show, adc_reg_7_store),
};

static int s2mps25_create_powermeter_sysfs(struct s2mps25_dev *s2mps25)
{
	struct device *s2mps25_adc_dev;
	struct device *dev = s2mps25->dev;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-powermeter@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mps25_adc_dev = pmic_device_create(s2mps25->adc_meter, device_name);
	s2mps25->powermeter_dev = s2mps25_adc_dev;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(powermeter_attr); i++) {
		err = device_create_file(s2mps25_adc_dev, &powermeter_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

#if IS_ENABLED(CONFIG_SEC_PM)
	if (!IS_ERR_OR_NULL(s2mps25->ap_pmic_dev)) {
		err = sysfs_create_link(&s2mps25->ap_pmic_dev->kobj,
				&s2mps25_adc_dev->kobj, "power_meter");
		if (err)
			pr_err("%s: fail to create link for power_meter(%d)\n",
					__func__, err);
	}
#endif /* CONFIG_SEC_PM */

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mps25_adc_dev, &powermeter_attr[i].dev_attr);
	pmic_device_destroy(s2mps25_adc_dev->devt);

	return -1;
}
#endif

int s2mps25_adc_set_enable(struct adc_info *adc_meter, int en)
{
	int ret = 0;
	uint8_t val = 0;

	if (en)
		val = ADC_ENABLE;
	else
		val = ADC_DISABLE;

	/* [W/A][EVT0] power-meter force disable */
	if ((adc_meter->pmic_rev & 0x0F) == 0x00) {
		pr_err("%s: power-meter force disable !!! pmic_rev(%#x)\n", __func__, adc_meter->pmic_rev);
		val = ADC_DISABLE;
	}

	ret = s2mps25_update_reg(adc_meter->i2c, ADC_CTRL1, val, ADCEN_MASK);
	if (ret)
		return -1;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mps25_adc_set_enable);

static int adc_assign_mux_channel(struct adc_info *adc_meter)
{
	int ret = 0, count = 0;
	ssize_t i = 0;

	for (i = MUX0SEL; i <= MUX5SEL; i++) {
		uint8_t mux_val = adc_mux_val[adc_meter->adc_reg[count++]];

		ret = s2mps25_write_reg(adc_meter->i2c, i, mux_val);
		if (ret) {
			pr_err("%s: failed to write register\n", __func__);
			return -1;
		}
	}

	pr_info("%s: Done\n", __func__);

	return 0;
}

static int adc_set_channel(struct adc_info *adc_meter)
{
	/* Assign BUCKs for MUX channel */
	adc_meter->adc_reg[ADC_CH0] = ADC_B1;
	adc_meter->adc_reg[ADC_CH1] = ADC_B2;
	adc_meter->adc_reg[ADC_CH2] = ADC_B4;
	adc_meter->adc_reg[ADC_CH3] = ADC_B6;
	adc_meter->adc_reg[ADC_CH4] = ADC_B7;
	adc_meter->adc_reg[ADC_CH5] = ADC_B8;
	adc_meter->adc_reg[ADC_CH6] = ADC_B8;
	adc_meter->adc_reg[ADC_CH7] = ADC_B8;

	if (adc_assign_mux_channel(adc_meter) < 0)
		return -1;

	return 0;
}

static int adc_init_hw(struct adc_info *adc_meter)
{
	int ret = 0;

	/* Enable ACC power mode */
	ret = s2mps25_write_reg(adc_meter->i2c, ADC_CTRL3, 0x3F);
	if (ret)
		goto err;

	/* EVT1 Power-meter sampling delay option */
	if (adc_meter->pmic_rev) {
		ret = s2mps25_update_reg(adc_meter->i2c, ADC_CTRL0, 0x0C, 0x0C);
		if (ret)
			goto err;
	}

	return 0;
err:
	return -1;
}

int s2mps25_adc_ntc_init(struct adc_info *adc_meter)
{
	int ret = 0;

	/* ADC configuration */
	ret = s2mps25_write_reg(adc_meter->i2c, ADC_CONFIG, 0x0B);
	if (ret)
		goto err;

	/* PM period, 1.038ms, minimum is '1' */
	ret = s2mps25_write_reg(adc_meter->i2c, PERIOD_CTRL0, 0x11);
	if (ret)
		goto err;

	/* NTC period, 10.01ms, minimum is '1' */
	ret = s2mps25_write_reg(adc_meter->i2c, PERIOD_CTRL1, 0xA4);
	if (ret)
		goto err;

	/* NTC SMP_NUM */
	ret = s2mps25_update_reg(adc_meter->i2c, ADC_CTRL2, NTC_SMP_NUM_32, NTC_SMP_MASK);
	if (ret)
		goto err;

	/* NTC mux selection */
	ret = s2mps25_write_reg(adc_meter->i2c, MUX8SEL, 0xED);
	if (ret)
		goto err;

	/* CHUB0 threshold & hys. Int. is occurred on 75 degree, disappeared on 70 degree */
	ret = s2mps25_write_reg(adc_meter->i2c, TH_CTRL0, 0xCB);
	if (ret)
		goto err;
	ret = s2mps25_write_reg(adc_meter->i2c, TH_CTRL1, 0x51);
	if (ret)
		goto err;
	/* CHUB1 threshold & hys. Int. is occurred on 75 degree, disappeared on 70 degree */
	ret = s2mps25_write_reg(adc_meter->i2c, TH_CTRL2, 0xCB);
	if (ret)
		goto err;
	ret = s2mps25_write_reg(adc_meter->i2c, TH_CTRL3, 0x51);
	if (ret)
		goto err;

	return ret;
err:
	pr_err("%s: failed to register\n", __func__);
	return -1;
}

static irqreturn_t s2mps25_ntc_irq(int irq, void *data)
{
	struct adc_info *adc_meter = data;

	mutex_lock(&adc_meter->irq_lock);

	if (adc_meter->ntc_irq[0] == irq) {
		adc_meter->ntc_cnt[0]++;
		pr_info("[PMIC] %s: NTC0_OVER IRQ: %d, %d\n", __func__, adc_meter->ntc_cnt[0], irq);
	} else if (adc_meter->ntc_irq[1] == irq) {
		adc_meter->ntc_cnt[1]++;
		pr_info("[PMIC] %s: NTC1_OVER IRQ: %d, %d\n", __func__, adc_meter->ntc_cnt[1], irq);
	}

	mutex_unlock(&adc_meter->irq_lock);

	return IRQ_HANDLED;
}

static int s2mps25_adc_set_interrupt(struct s2mps25_dev *s2mps25, struct adc_info *adc_meter)
{
	int i, ret = 0;
	static char ntc_name[NTC_MAX][32] = {0, };

	/* [W/A][EVT0] NTC force disable */
	if ((adc_meter->pmic_rev & 0x0F) == 0x00) {
		pr_err("%s: NTC force disable !!! pmic_rev(%#x)\n", __func__, adc_meter->pmic_rev);
		return 0;
	}

	/* Set NTC0 ~ NTC1_EN */
	ret = s2mps25_update_reg(adc_meter->i2c, ADC_CTRL1, NTC_EN_MASK, NTC_EN_MASK);
	if (ret)
		goto err;

	for (i = 0; i < NTC_MAX; i++) {
		adc_meter->ntc_irq[i] = s2mps25->irq_base +
					S2MPS25_ADC_IRQ_NTC0_OVER_INTP + i;

		/* Dynamic allocation for device name */
		snprintf(ntc_name[i], sizeof(ntc_name[i]) - 1, "%s-NTC-IRQ%d@%s",
			 dev_driver_string(s2mps25->dev), i, dev_name(s2mps25->dev));

		/* Set NTC IRQ */
		ret = devm_request_threaded_irq(s2mps25->dev, adc_meter->ntc_irq[i], NULL,
				s2mps25_ntc_irq, 0, ntc_name[i], adc_meter);
		if (ret < 0) {
			dev_err(s2mps25->dev, "%s: Failed to request NTC IRQ: %d: %d\n",
					__func__, adc_meter->ntc_irq[i], ret);
			return ret;
		}
	}

	return ret;
err:
	pr_err("%s: failed to set interrupts\n", __func__);
	return -1;
}

void s2mps25_powermeter_init(struct s2mps25_dev *s2mps25)
{
	struct adc_info *adc_meter;
	size_t size;

	pr_info("[PMIC] %s: start\n", __func__);

	size = sizeof(struct adc_info);
	adc_meter = devm_kzalloc(s2mps25->dev, size, GFP_KERNEL);
	if (!adc_meter) {
		pr_err("%s: adc_meter alloc fail.\n", __func__);
		return;
	}

	size = sizeof(uint64_t) * ARRAY_SIZE(adc_channel_reg);
	adc_meter->power_val = devm_kzalloc(s2mps25->dev, size, GFP_KERNEL);

	size = sizeof(uint8_t) * ARRAY_SIZE(adc_channel_reg);
	adc_meter->adc_reg = devm_kzalloc(s2mps25->dev, size, GFP_KERNEL);

	adc_meter->pmic_rev = s2mps25->pmic_rev;
	adc_meter->i2c = s2mps25->adc_i2c;
	mutex_init(&adc_meter->adc_lock);
	mutex_init(&adc_meter->irq_lock);
	s2mps25->adc_meter = adc_meter;

	if (adc_set_channel(adc_meter) < 0) {
		pr_err("%s: adc_set_channel fail\n", __func__);
		return;
	}

	if (adc_init_hw(adc_meter) < 0) {
		pr_err("%s: adc_init_hw fail\n", __func__);
		return;
	}

	if (s2mps25_adc_set_enable(adc_meter, ADC_ENABLE) < 0) {
		pr_err("%s: s2mps25_adc_set_enable fail\n", __func__);
		return;
	}

	if (s2mps25_adc_ntc_init(adc_meter) < 0) {
		pr_err("%s: s2mps25_adc_ntc_init fail\n", __func__);
		return;
	}

	if (s2mps25_adc_set_interrupt(s2mps25, adc_meter) < 0) {
		pr_err("%s: s2mps25_adc_set_interrupt fail\n", __func__);
		return;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	if (s2mps25_create_powermeter_sysfs(s2mps25) < 0) {
		pr_info("%s: failed to create sysfs\n", __func__);
		return;
	}
#endif
	pr_info("[PMIC] %s: end\n", __func__);
}
EXPORT_SYMBOL_GPL(s2mps25_powermeter_init);

void s2mps25_powermeter_deinit(struct s2mps25_dev *s2mps25)
{
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mps25_adc_dev = s2mps25->powermeter_dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(powermeter_attr); i++)
		device_remove_file(s2mps25_adc_dev, &powermeter_attr[i].dev_attr);
	pmic_device_destroy(s2mps25_adc_dev->devt);
#endif
	/* ADC turned off */
	s2mps25_adc_set_enable(s2mps25->adc_meter, ADC_DISABLE);
}
EXPORT_SYMBOL(s2mps25_powermeter_deinit);
