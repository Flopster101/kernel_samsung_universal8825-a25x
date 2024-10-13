/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include "pmu_cal.h"
/*  structure of pmucal_data */
/*	struct pmucal_data{
 *		int accesstype;
 *		bool pmureg;
 *		int  sfr;
 *		int field;
 *		int value;
 *
 *	};
 */

extern enable_hwbypass;

static char *pmu_cal_getsfr(int sfr)
{
	/* translate string to sfr addr */
	switch (sfr) {
	case WLBT_CTRL_NS:
		return "WLBT_CTRL_NS";
	case WLBT_CTRL_S:
		return "WLBT_CTRL_S";
	case SYSTEM_OUT:
		return "SYSTEM_OUT";
	case WLBT_CONFIGURATION:
		return "CONFIGURATION";
	case WLBT_STATUS:
		return "WLBT_STATUS";
	case WLBT_IN:
		return "WLBT_IN";
	case VGPIO_TX_MONITOR:
		return "VGPIO_TX_MONITOR";
	case WLBT_INT_TYPE:
		return "WLBT_INT_TYPE";
	case WLBT_OPTION:
		return "WLBT_OPTION";
	default:
		return NULL;
	}
}

static struct regmap *pmu_cal_check_base(struct platform_mif *platform,
					 bool pmureg)
{
	/* Set base */
	if (pmureg)
		return platform->pmureg;

	return platform->i3c_apm_pmic;
}

static int pmu_cal_write(struct platform_mif *platform, struct pmucal_data data)
{
	struct regmap *target;
	char *target_sfr;
	int value;
	int field;
	int ret;
	int val;

	target = pmu_cal_check_base(platform, data.pmureg);
	/* set value */
	value = data.value << data.field;
	field = 1 << data.field;

	/* translate sfr addr to string */
	target_sfr = pmu_cal_getsfr(data.sfr);
	if (!target_sfr)
		return -EINVAL;

	/* clean CFG_REQ PENDING interrupt */
	if (data.sfr == WLBT_CONFIGURATION && value == 1) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "clean CFG_REQ irq\n");
		platform_cfg_req_irq_clean_pending(platform);
	}

	/* write reg */
	ret = regmap_update_bits(target, data.sfr, field, value);
	if (ret < 0) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Failed to Set  %s[%d]: 0x%08x\n", target_sfr,
				  field, value);
		return ret;
	}
	regmap_read(target, data.sfr, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "updated successfully %s[%d]: 0x%08x\n", target_sfr,
			  data.field, val);
	return 0;
}

static int pmu_cal_read(struct platform_mif *platform, struct pmucal_data data)
{
	unsigned long timeout;
	struct regmap *target;
	char *target_sfr;
	u32 val = 0, reg_val = 0;

	SCSC_TAG_INFO(PLAT_MIF, "start pmu_read\n");
	target = pmu_cal_check_base(platform, data.pmureg);

	if (data.field > 31) {
		SCSC_TAG_ERR(PLAT_MIF, "field(%d) > 31\n", data.field);
		return -EINVAL;
	}

	if (data.value & 0xFFFFFFFE) {
		SCSC_TAG_ERR(PLAT_MIF, "value(%d). Multi bit not supported.\n", data.value);
		return -EINVAL;
	}

	/* translate sfr addr to string */
	target_sfr = pmu_cal_getsfr(data.sfr);
	if (!target_sfr)
		return -EINVAL;

	timeout = jiffies + msecs_to_jiffies(500);
	do {
		regmap_read(target, data.sfr, &reg_val);
		val = reg_val & BIT(data.field);
		val >>= data.field;
		if (val == data.value) {
			SCSC_TAG_INFO(PLAT_MIF, "read %s[%d]  0x%08x\n",
				      target_sfr, data.field, reg_val);
			goto done;
		}
	} while (time_before(jiffies, timeout));

	regmap_read(target, data.sfr, &reg_val);
	val = reg_val & BIT(data.field);
	val >>= data.field;
	if (val == data.value) {
		SCSC_TAG_INFO(PLAT_MIF, "read %s[%d]  0x%08x\n",
			      target_sfr, data.field, reg_val);
		goto done;
	}
	SCSC_TAG_INFO(PLAT_MIF, "timeout waiting %s 0x%08x\n", target_sfr, reg_val);
	return -EINVAL;

done:
	return 0;
}

static int pmu_cal_atomic(struct platform_mif *platform,
			  struct pmucal_data data)
{
	struct regmap *target;
	char *target_sfr;
	int sfr;
	int ret;
	int val;

	SCSC_TAG_INFO(PLAT_MIF, "start pmu_atomic\n");
	target = pmu_cal_check_base(platform, data.pmureg);
	/* set value */
	sfr = data.sfr | 0xc000;

	/* translate sfr addr to string */
	target_sfr = pmu_cal_getsfr(data.sfr);
	if (!target_sfr)
		return -EINVAL;

	/* write reg */
	ret = regmap_write_bits(target, sfr, data.value, data.value);
	if (ret < 0) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Failed to Set  %s: 0x%08x\n", target_sfr,
				  data.value);
		return ret;
	}
	regmap_read(target, data.sfr, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "updated successfully %s[%d]: 0x%08x\n", target_sfr,
			  data.value, val);
	return 0;
}

static int pmu_cal_clear(struct platform_mif *platform,
			 struct pmucal_data data)
{
	struct regmap *target;
	char *target_sfr;
	int sfr;
	int ret;
	int val;

	SCSC_TAG_INFO(PLAT_MIF, "start pmu_clear\n");
	target = pmu_cal_check_base(platform, data.pmureg);
	/* set value */
	sfr = data.sfr | 0x8000;

	/* translate sfr addr to string */
	target_sfr = pmu_cal_getsfr(data.sfr);
	if (!target_sfr)
		return -EINVAL;

	/* write reg */
	ret = regmap_write_bits(target, sfr, data.value, data.value);
	if (ret < 0) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Failed to Set  %s: 0x%08x\n", target_sfr,
				  data.value);
		return ret;
	}
	regmap_read(target, data.sfr, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "updated successfully %s[%d]: 0x%08x\n", target_sfr,
			  data.value, val);
	return 0;
}

int pmu_cal_progress(struct platform_mif *platform,
		     struct pmucal_data *pmu_data, int pmucal_data_size)
{
	int ret = 0;
	int i   = 0;

	SCSC_TAG_INFO(PLAT_MIF, "start pmu_cal\n");

	for (i = 0; i < pmucal_data_size; i++) {
		if (pmu_data[i].bypass || enable_hwbypass) {
			switch (pmu_data[i].accesstype) {
			case PMUCAL_WRITE:
				ret = pmu_cal_write(platform, pmu_data[i]);
				break;
			case PMUCAL_DELAY:
				udelay(pmu_data[i].value);
				break;
			case PMUCAL_READ:
				ret = pmu_cal_read(platform, pmu_data[i]);
				break;
			case PMUCAL_ATOMIC:
				ret = pmu_cal_atomic(platform, pmu_data[i]);
				break;
			case PMUCAL_CLEAR:
				ret = pmu_cal_clear(platform, pmu_data[i]);
				break;
			default:
				return -EINVAL;
			}
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}
EXPORT_SYMBOL(pmu_cal_progress);
