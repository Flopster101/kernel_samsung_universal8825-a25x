/*
 * s2mf301_top.c - S2MF301 Power Meter Driver
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/mfd/samsung/s2mf301.h>
#include <linux/power/s2mf301_top.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/power/ifpmic_class.h>

static enum power_supply_property s2mf301_top_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

#if IS_ENABLED(CONFIG_SYSFS_S2MF301)
struct s2mf301_info {
	struct s2mf301_dev *iodev;
	u8 base_addr;
	u8 read_addr;
	u8 read_val;
	struct device *dev;
};
#endif


/* unit functions */
static void s2mf301_top_test_read(struct s2mf301_top_data *top)
{
	u8 data;
	char str[1016] = {0,};
	int i;

	for (i = 0x50; i <= 0x5C; i++) {
		s2mf301_read_reg(top->i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}
	pr_info("%s: %s\n", __func__, str);
}



/* API functions */
static int s2mf301_top_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mf301_top_data *top = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;
#if defined(CONFIG_TOP_S2MF301_SUPPORT_S2MC501)
	u8 data;
#endif

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
#if defined(CONFIG_TOP_S2MF301_SUPPORT_S2MC501)
		case POWER_SUPPLY_S2M_PROP_TOP_DC_INT_MASK:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_AUTO_PPS_INT, &data);
			val->intval = data;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_AUTO_PPS_STATUS:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_AUTO_PPS_STATE, &data);
			val->intval = data;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_EN:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			val->intval = (data & S2MF301_TOP_REG_THERMAL_EN_MASK) >> S2MF301_TOP_REG_THERMAL_EN_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_SELECTION:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			val->intval = (data & S2MF301_TOP_REG_THERMAL_SELECTION_MASK) >> S2MF301_TOP_REG_THERMAL_SELECTION_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_CC_ICHG_SEL:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			val->intval = (data & S2MF301_TOP_REG_CC_ICHG_SEL_MASK) >> S2MF301_TOP_REG_CC_ICHG_SEL_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_PDO_MAX_CUR_SEL:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			val->intval = (data & S2MF301_TOP_REG_DC_PDO_MAX_CUR_SEL_MASK) >> S2MF301_TOP_REG_DC_PDO_MAX_CUR_SEL_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_EN_STEP_CC:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			val->intval = (data & S2MF301_TOP_REG_EN_STEP_CC_MASK) >> S2MF301_TOP_REG_EN_STEP_CC_SHIFT;
			pr_info("%s, EN_STEP_CC, %x\n", __func__, val->intval);
			s2mf301_top_test_read(top);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_AUTO_PPS_START:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			val->intval = (data & S2MF301_TOP_REG_AUTO_PPS_START_MASK) >> S2MF301_TOP_REG_AUTO_PPS_START_SHIFT;
			pr_info("%s, AUTO_PPS_START, %x\n", __func__, val->intval);
			s2mf301_top_test_read(top);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_DONE_SOC:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_TOP_OFF_CURRENT, &data);
			val->intval = (data & S2MF301_TOP_REG_DC_DONE_SOC_MASK) >> S2MF301_TOP_REG_DC_DONE_SOC_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_TOP_OFF_CURRENT:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_TOP_OFF_CURRENT, &data);
			val->intval = (data & S2MF301_TOP_REG_DC_TOP_OFF_CURRENT_MASK) >> S2MF301_TOP_REG_DC_TOP_OFF_CURRENT_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_CV:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CV_LEVEL, &data);
			val->intval = (data & S2MF301_TOP_REG_DC_CV_MASK) >> S2MF301_TOP_REG_DC_CV_SHIFT;
			pr_info("%s, DC_CV, %x\n", __func__, val->intval);
			s2mf301_top_test_read(top);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_END:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_THERMAL_CONDITION, &data);
			val->intval = (data & S2MF301_TOP_REG_THERMAL_END_MASK) >> S2MF301_TOP_REG_THERMAL_END_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_START:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_THERMAL_CONDITION, &data);
			val->intval = (data & S2MF301_TOP_REG_THERMAL_START_MASK) >> S2MF301_TOP_REG_THERMAL_START_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_CC_STEP_VBAT1:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_VBAT1, &data);
			val->intval = (data & S2MF301_TOP_REG_CC_STEP_VBAT1_MASK) >> S2MF301_TOP_REG_CC_STEP_VBAT1_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_CC_STEP_VBAT2:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_VBAT2, &data);
			val->intval = (data & S2MF301_TOP_REG_CC_STEP_VBAT2_MASK) >> S2MF301_TOP_REG_CC_STEP_VBAT2_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_CC_STEP_1:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_1, &data);
			val->intval = (data & S2MF301_TOP_REG_DC_CC_STEP1_MASK) >> S2MF301_TOP_REG_DC_CC_STEP1_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_CC_STEP_2:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_2, &data);
			val->intval = (data & S2MF301_TOP_REG_DC_CC_STEP2_MASK) >> S2MF301_TOP_REG_DC_CC_STEP2_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_CC_STEP_3:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_2, &data);
			val->intval = (data & S2MF301_TOP_REG_DC_CC_STEP3_MASK) >> S2MF301_TOP_REG_DC_CC_STEP3_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_RECOVERY_WAITING:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_THERMAL_WAIT, &data);
			val->intval = (data & S2MF301_TOP_REG_THERMAL_RECOVERY_WAITING_MASK) >> S2MF301_TOP_REG_THERMAL_RECOVERY_WAITING_SHIFT;
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_CONTROL_WAITING:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_THERMAL_WAIT, &data);
			val->intval = (data & S2MF301_TOP_REG_THERMAL_CONTROL_WAITING_MASK) >> S2MF301_TOP_REG_THERMAL_CONTROL_WAITING_SHIFT;
			break;
#endif
		default:
			return -EINVAL;
		}
		return 0;
	default:
		return -EINVAL;
	}
	return 0;
}

static int s2mf301_top_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2mf301_top_data *top = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;
#if defined(CONFIG_TOP_S2MF301_SUPPORT_S2MC501)
	u8 data;
#endif

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
#if defined(CONFIG_TOP_S2MF301_SUPPORT_S2MC501)
		case POWER_SUPPLY_S2M_PROP_TOP_DC_INT_MASK:
			data = val->intval;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_DC_AUTO_PPS_INT, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_EN:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			data &= ~S2MF301_TOP_REG_THERMAL_EN_MASK;
			data |= !!val->intval << S2MF301_TOP_REG_THERMAL_EN_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_SELECTION:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			data &= ~S2MF301_TOP_REG_THERMAL_SELECTION_MASK;
			data |= !!val->intval << S2MF301_TOP_REG_THERMAL_SELECTION_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_CC_ICHG_SEL:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			data &= ~S2MF301_TOP_REG_CC_ICHG_SEL_MASK;
			data |= !!val->intval << S2MF301_TOP_REG_CC_ICHG_SEL_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_PDO_MAX_CUR_SEL:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			data &= ~S2MF301_TOP_REG_DC_PDO_MAX_CUR_SEL_MASK;
			data |= val->intval << S2MF301_TOP_REG_DC_PDO_MAX_CUR_SEL_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_EN_STEP_CC:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			data &= ~S2MF301_TOP_REG_EN_STEP_CC_MASK;
			data |= !!val->intval << S2MF301_TOP_REG_EN_STEP_CC_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, data);
			pr_info("%s, EN_STEP_CC, %x\n", __func__, val->intval);
			s2mf301_top_test_read(top);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_AUTO_PPS_START:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
			data &= ~S2MF301_TOP_REG_AUTO_PPS_START_MASK;
			data |= !!val->intval << S2MF301_TOP_REG_AUTO_PPS_START_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, data);
			pr_info("%s, AUTO_PPS_START, %x\n", __func__, val->intval);
			s2mf301_top_test_read(top);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_DONE_SOC:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_TOP_OFF_CURRENT, &data);
			data &= ~S2MF301_TOP_REG_DC_DONE_SOC_MASK;
			data |= val->intval << S2MF301_TOP_REG_DC_DONE_SOC_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_DC_TOP_OFF_CURRENT, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_TOP_OFF_CURRENT:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_TOP_OFF_CURRENT, &data);
			data &= ~S2MF301_TOP_REG_DC_TOP_OFF_CURRENT_MASK;
			data |= val->intval << S2MF301_TOP_REG_DC_TOP_OFF_CURRENT_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_DC_TOP_OFF_CURRENT, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_CV:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CV_LEVEL, &data);
			data &= ~S2MF301_TOP_REG_DC_CV_MASK;
			data |= val->intval << S2MF301_TOP_REG_DC_CV_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_DC_CV_LEVEL, data);
			pr_info("%s, DC_CV, %x\n", __func__, val->intval);
			s2mf301_top_test_read(top);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_END:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_THERMAL_CONDITION, &data);
			data &= ~S2MF301_TOP_REG_THERMAL_END_MASK;
			data |= val->intval << S2MF301_TOP_REG_THERMAL_END_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_THERMAL_CONDITION, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_START:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_THERMAL_CONDITION, &data);
			data &= ~S2MF301_TOP_REG_THERMAL_START_MASK;
			data |= val->intval << S2MF301_TOP_REG_THERMAL_START_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_THERMAL_CONDITION, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_CC_STEP_VBAT1:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_VBAT1, &data);
			data &= ~S2MF301_TOP_REG_CC_STEP_VBAT1_MASK;
			data |= val->intval << S2MF301_TOP_REG_CC_STEP_VBAT1_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_VBAT1, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_CC_STEP_VBAT2:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_VBAT2, &data);
			data &= ~S2MF301_TOP_REG_CC_STEP_VBAT2_MASK;
			data |= val->intval << S2MF301_TOP_REG_CC_STEP_VBAT2_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_VBAT2, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_CC_STEP_1:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_1, &data);
			data &= ~S2MF301_TOP_REG_DC_CC_STEP1_MASK;
			data |= val->intval << S2MF301_TOP_REG_DC_CC_STEP1_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_1, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_CC_STEP_2:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_2, &data);
			data &= ~S2MF301_TOP_REG_DC_CC_STEP2_MASK;
			data |= val->intval << S2MF301_TOP_REG_DC_CC_STEP2_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_2, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_DC_CC_STEP_3:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_2, &data);
			data &= ~S2MF301_TOP_REG_DC_CC_STEP2_MASK;
			data |= val->intval << S2MF301_TOP_REG_DC_CC_STEP2_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_DC_CC_STEP_2, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_RECOVERY_WAITING:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_THERMAL_WAIT, &data);
			data &= ~S2MF301_TOP_REG_THERMAL_RECOVERY_WAITING_MASK;
			data |= val->intval << S2MF301_TOP_REG_THERMAL_RECOVERY_WAITING_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_THERMAL_WAIT, data);
			break;
		case POWER_SUPPLY_S2M_PROP_TOP_THERMAL_CONTROL_WAITING:
			s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_THERMAL_WAIT, &data);
			data &= ~S2MF301_TOP_REG_THERMAL_CONTROL_WAITING_MASK;
			data |= val->intval << S2MF301_TOP_REG_THERMAL_CONTROL_WAITING_SHIFT;
			s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_THERMAL_WAIT, data);
			break;
#endif
		default:
			return -EINVAL;
		}
		return 0;
	default:
		return -EINVAL;
	}
	return 0;
}

/* IRQ functions */
#if defined(CONFIG_TOP_S2MF301_SUPPORT_S2MC501)
static irqreturn_t s2mf301_top_ramp_up_done_isr(int irq, void *data)
{
	pr_info("%s, \n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_top_ramp_up_fail_isr(int irq, void *data)
{
	pr_info("%s, \n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_top_thermal_control_isr(int irq, void *data)
{
	pr_info("%s, \n", __func__);
	return IRQ_HANDLED;
}
static irqreturn_t s2mf301_top_charging_state_change_isr(int irq, void *data)
{
	pr_info("%s, \n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_top_charging_done_isr(int irq, void *data)
{
	pr_info("%s, \n", __func__);
	return IRQ_HANDLED;
}
#endif

static const struct of_device_id s2mf301_top_match_table[] = {
	{ .compatible = "samsung,s2mf301-top",},
	{},
};

static void s2mf301_top_init_reg(struct s2mf301_top_data *top)
{
	u8 data;

	s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_TOP_PM_RID_INT, 0xff);
	s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_TOP_TC_RID_INT, 0xff);

#if defined(CONFIG_TOP_S2MF301_SUPPORT_S2MC501)
	s2mf301_read_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, &data);
	data |= S2MF301_TOP_REG_EN_REG_WRITE_MASK;
	s2mf301_write_reg(top->i2c, S2MF301_TOP_REG_AUTO_PPS_SETTING, data);
#endif
}

#if IS_ENABLED(CONFIG_SYSFS_S2MF301)
static ssize_t s2mf301_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mf301_info *s2mf301 = dev_get_drvdata(dev);
	int ret;
	u8 base_addr = 0, reg_addr = 0, val = 0;
	struct s2mf301_dev *iodev = s2mf301->iodev;
	struct i2c_client *i2c;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return -1;
	}

	ret = sscanf(buf, "0x%02hhx%02hhx", &base_addr, &reg_addr);
	if (ret != 2) {
		pr_err("%s: input error\n", __func__);
		return size;
	}

	switch (base_addr) {
	case I2C_BASE_TOP:
		i2c = iodev->i2c;
		break;
	case I2C_BASE_CHG:
		i2c = iodev->chg;
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return size;
	}

	ret = s2mf301_read_reg(i2c, reg_addr, &val);
	if (ret < 0)
		pr_info("%s: fail to read i2c addr/data\n", __func__);

	pr_info("%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__,
		base_addr, reg_addr, val);
	s2mf301->base_addr = base_addr;
	s2mf301->read_addr = reg_addr;
	s2mf301->read_val = val;

	return size;
}

static ssize_t s2mf301_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mf301_info *s2mf301 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02hhx%02hhx: 0x%02hhx\n",
			s2mf301->base_addr, s2mf301->read_addr, s2mf301->read_val);
}

static ssize_t s2mf301_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	int ret;
	u8 base_addr = 0, reg_addr = 0, data = 0;
	struct s2mf301_info *s2mf301 = dev_get_drvdata(dev);
	struct s2mf301_dev *iodev = s2mf301->iodev;
	struct i2c_client *i2c;

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
	case I2C_BASE_TOP:
		i2c = iodev->i2c;
		break;
	case I2C_BASE_CHG:
		i2c = iodev->chg;
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return size;
	}

	pr_info("%s: reg(0x%02hhx%02hhx) data(0x%02hhx)\n", __func__,
		base_addr, reg_addr, data);

	ret = s2mf301_write_reg(i2c, reg_addr, data);
	if (ret < 0)
		pr_info("%s: fail to write i2c addr/data\n", __func__);

	return size;
}

static ssize_t s2mf301_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mf301_write\n");
}

static struct ifpmic_device_attribute ifpmic_attr[] = {
	IFPMIC_ATTR(s2mf301_write, 0644, s2mf301_write_show, s2mf301_write_store),
	IFPMIC_ATTR(s2mf301_read, 0644, s2mf301_read_show, s2mf301_read_store),
};

static int s2mf301_create_sysfs(struct s2mf301_info *s2mf301_top_info)
{
	struct device *s2mf301_ifpmic = s2mf301_top_info->dev;
	struct device *dev = s2mf301_top_info->iodev->dev;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);
	s2mf301_top_info->base_addr = 0;
	s2mf301_top_info->read_addr = 0;
	s2mf301_top_info->read_val = 0;

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mf301_ifpmic = ifpmic_device_create(s2mf301_top_info, device_name);
	s2mf301_top_info->dev = s2mf301_ifpmic;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(ifpmic_attr); i++) {
		err = device_create_file(s2mf301_ifpmic, &ifpmic_attr[i].dev_attr);
		if (err)
			goto remove_ifpmic_device;
	}

	return 0;

remove_ifpmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mf301_ifpmic, &ifpmic_attr[i].dev_attr);
	ifpmic_device_destroy(s2mf301_ifpmic->devt);

	return -1;
}
#endif


static int s2mf301_top_probe(struct platform_device *pdev)
{
	struct s2mf301_dev *s2mf301 = dev_get_drvdata(pdev->dev.parent);
	struct s2mf301_top_data *top;
	struct power_supply_config psy_cfg = {};
	struct s2mf301_info *s2mf301_top_info;
	int ret = 0;

	pr_info("%s:[BATT] S2MF301 TOP driver probe\n", __func__);
	top = kzalloc(sizeof(struct s2mf301_top_data), GFP_KERNEL);
	if (!top)
		return -ENOMEM;

	top->dev = &pdev->dev;
	/* need 0x74 i2c */
	top->i2c = s2mf301->i2c;

	s2mf301_top_info = devm_kzalloc(&pdev->dev, sizeof(struct s2mf301_info), GFP_KERNEL);
	s2mf301_top_info->iodev = s2mf301;

	platform_set_drvdata(pdev, top);

	top->psy_pm_desc.name			= "s2mf301-top";
	top->psy_pm_desc.type			= POWER_SUPPLY_TYPE_UNKNOWN;
	top->psy_pm_desc.get_property	= s2mf301_top_get_property;
	top->psy_pm_desc.set_property	= s2mf301_top_set_property;
	top->psy_pm_desc.properties		= s2mf301_top_props;
	top->psy_pm_desc.num_properties = ARRAY_SIZE(s2mf301_top_props);

	psy_cfg.drv_data = top;

	top->psy_pm = power_supply_register(&pdev->dev, &top->psy_pm_desc, &psy_cfg);
	if (IS_ERR(top->psy_pm)) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = PTR_ERR(top->psy_pm);
		goto err_power_supply_register;
	}

#if defined(CONFIG_TOP_S2MF301_SUPPORT_S2MC501)
	pr_info("%s, SUPPORT MC501 enabled\n", __func__);
	top->irq_rampup_done = s2mf301->pdata->irq_base + S2MF301_TOP_DC_IRQ_RAMP_UP_DONE;
	ret = request_threaded_irq(top->irq_rampup_done, NULL, s2mf301_top_ramp_up_done_isr, 0, "ramp-up-done", top);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, top->irq_rampup_done, ret);

	top->irq_rampup_fail = s2mf301->pdata->irq_base + S2MF301_TOP_DC_IRQ_RAMP_UP_FAIL;
	ret = request_threaded_irq(top->irq_rampup_fail, NULL, s2mf301_top_ramp_up_fail_isr, 0, "ramp-up-fail", top);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, top->irq_rampup_fail, ret);

	top->irq_thermal_control = s2mf301->pdata->irq_base + S2MF301_TOP_DC_IRQ_THERMAL_CONTROL;
	ret = request_threaded_irq(top->irq_thermal_control, NULL, s2mf301_top_thermal_control_isr, 0, "thermal-control", top);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, top->irq_thermal_control, ret);

	top->irq_charging_state_change = s2mf301->pdata->irq_base + S2MF301_TOP_DC_IRQ_CHARGING_STATE_CHAGNE;
	ret = request_threaded_irq(top->irq_charging_state_change, NULL, s2mf301_top_charging_state_change_isr, 0, "charging-state-change", top);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, top->irq_charging_state_change, ret);

	top->irq_charging_done = s2mf301->pdata->irq_base + S2MF301_TOP_DC_IRQ_CHARGING_DONE;
	ret = request_threaded_irq(top->irq_charging_done, NULL, s2mf301_top_charging_done_isr, 0, "charging-done", top);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, top->irq_charging_done, ret);
#endif

	s2mf301_top_init_reg(top);

#if IS_ENABLED(CONFIG_SYSFS_S2MF301)
	ret = s2mf301_create_sysfs(s2mf301_top_info);
	if (ret < 0) {
		pr_err("%s: s2mf301_create_sysfs fail\n", __func__);
		return -ENODEV;
	}
#endif

	pr_info("%s:[BATT] S2MF301 TOP driver loaded OK\n", __func__);

	return ret;

err_power_supply_register:
	kfree(top);

	return ret;
}

static int s2mf301_top_remove(struct platform_device *pdev)
{
	struct s2mf301_top_data *top = platform_get_drvdata(pdev);
#if IS_ENABLED(CONFIG_SYSFS_S2MF301)
	struct s2mf301_dev *s2mf301 = dev_get_drvdata(pdev->dev.parent);
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(ifpmic_attr); i++)
		device_remove_file(s2mf301->dev, &ifpmic_attr[i].dev_attr);
	ifpmic_device_destroy(s2mf301->dev->devt);
#endif

	kfree(top);
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int s2mf301_top_suspend(struct device *dev)
{
	return 0;
}

static int s2mf301_top_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mf301_top_suspend NULL
#define s2mf301_top_resume NULL
#endif

static void s2mf301_top_shutdown(struct device *dev)
{
	pr_info("%s: S2MF301 TOP driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mf301_top_pm_ops, s2mf301_top_suspend,
		s2mf301_top_resume);

static struct platform_driver s2mf301_top_driver = {
	.driver		 = {
		.name			= "s2mf301-top",
		.owner			= THIS_MODULE,
		.of_match_table	= s2mf301_top_match_table,
		.pm				= &s2mf301_top_pm_ops,
		.shutdown		= s2mf301_top_shutdown,
	},
	.probe		= s2mf301_top_probe,
	.remove		= s2mf301_top_remove,
};

static int __init s2mf301_top_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&s2mf301_top_driver);
}
module_init(s2mf301_top_init);

static void __exit s2mf301_top_exit(void)
{
	platform_driver_unregister(&s2mf301_top_driver);
}
module_exit(s2mf301_top_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("TOP driver for S2MF301");
MODULE_SOFTDEP("post: s2m_muic_module");
