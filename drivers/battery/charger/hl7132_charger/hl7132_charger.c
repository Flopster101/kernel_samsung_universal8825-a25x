#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/power_supply.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include "hl7132_charger.h"
#include "../../common/sec_charging_common.h"
#include "../../common/sec_direct_charger.h"
#include <linux/battery/sec_pd.h>
#else
#include <linux/power/halo/hl7132_charger.h>
#include <linux/power/halo/sec_direct_charger.h>
#endif
#if defined (CONFIG_OF)
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#endif

#define BITS(_end, _start)		((BIT(_end) - BIT(_start)) + BIT(_end))
#define MIN(a, b)				((a < b) ? (a):(b))

#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#define HALO_DBG
#endif

static int hl7132_read_adc(struct hl7132_charger *chg);

#ifdef HALO_DBG
#ifdef CONFIG_DEBUG_FS
static bool hl7132_log_full(struct hl7132_charger *chg)
{
	return chg->logbuffer_tail ==
		(chg->logbuffer_head + 1) % LOG_BUFFER_ENTRIES;
}

__printf(2, 0)
static void _hl7132_log(struct hl7132_charger *chg, const char *fmt,
			 va_list args)
{
	char tmpbuffer[LOG_BUFFER_ENTRY_SIZE];
	u64 ts_nsec = local_clock();
	unsigned long rem_nsec;

	if (!chg->logbuffer[chg->logbuffer_head]) {
		chg->logbuffer[chg->logbuffer_head] =
				kzalloc(LOG_BUFFER_ENTRY_SIZE, GFP_KERNEL);
		if (!chg->logbuffer[chg->logbuffer_head])
			return;
	}

	vsnprintf(tmpbuffer, sizeof(tmpbuffer), fmt, args);

	mutex_lock(&chg->logbuffer_lock);

	if (hl7132_log_full(chg)) {
		chg->logbuffer_head = max(chg->logbuffer_head - 1, 0);
		strlcpy(tmpbuffer, "overflow", sizeof(tmpbuffer));
	}

	if (chg->logbuffer_head < 0 || chg->logbuffer_head >= LOG_BUFFER_ENTRIES) {
		dev_warn(chg->dev,
			 "Bad log buffer index %d\n", chg->logbuffer_head);
		goto abort;
	}

	if (!chg->logbuffer[chg->logbuffer_head]) {
		dev_warn(chg->dev,
			 "Log buffer index %d is NULL\n", chg->logbuffer_head);
		goto abort;
	}

	rem_nsec = do_div(ts_nsec, 1000000000);
	scnprintf(chg->logbuffer[chg->logbuffer_head],
		  LOG_BUFFER_ENTRY_SIZE, "[%5lu.%06lu] %s",
		  (unsigned long)ts_nsec, rem_nsec / 1000,
		  tmpbuffer);
	chg->logbuffer_head = (chg->logbuffer_head + 1) % LOG_BUFFER_ENTRIES;

abort:
	mutex_unlock(&chg->logbuffer_lock);
}

static void hl7132_log(struct hl7132_charger *chg, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	_hl7132_log(chg, fmt, args);
	va_end(args);
}


static int hl7132_debug_show(struct seq_file *s, void *v)
{
	struct hl7132_charger *chg = (struct hl7132_charger *)s->private;
	int tail;

	mutex_lock(&chg->logbuffer_lock);
	tail = chg->logbuffer_tail;
	while (tail != chg->logbuffer_head) {
		seq_printf(s, "%s\n", chg->logbuffer[tail]);
		tail = (tail + 1) % LOG_BUFFER_ENTRIES;
	}
	if (!seq_has_overflowed(s))
		chg->logbuffer_tail = tail;
	mutex_unlock(&chg->logbuffer_lock);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(hl7132_debug);

static void hl7132_log_debugfs_init(struct hl7132_charger *chg)
{
	char name[NAME_MAX];

	mutex_init(&chg->logbuffer_lock);
	snprintf(name, NAME_MAX, "hl7132-%s", dev_name(chg->dev));
	chg->dentry = debugfs_create_file(name, S_IFREG | 0444, chg->debug_root,
					   chg, &hl7132_debug_fops);
}

static void hl7132_log_debugfs_exit(struct hl7132_charger *chg)
{
	debugfs_remove(chg->dentry);
}
#else
static void hl7132_log(const struct hl1732_charger *chg, const char *fmt, ...) { }
static void hl7132_debugfs_init(const struct hl7132_charger *chg) { }
static void hl7132_debugfs_exit(const struct hl7132_charger *chg) { }
#endif

//#define LOG_DBG(fmt, args...)   printk(KERN_ERR "[%s]:: " fmt, __func__, ## args);
#define LOG_DBG(chg, fmt, args...) hl7132_log(chg, "[%s]:: " fmt, __func__, ## args);

#else
#define LOG_DBG(chg, fmt, args...) pr_info("[%s]:: " fmt, __func__, ## args)
#endif


static int hl7132_set_charging_state(struct hl7132_charger *chg, unsigned int charging_state)
{
	union power_supply_propval value = {0,};
	static int prev_val = DC_STATE_NOT_CHARGING;

	chg->charging_state = charging_state;

	switch (charging_state) {
	case DC_STATE_NOT_CHARGING:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
		break;
	case DC_STATE_CHECK_VBAT:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT;
		break;
	case DC_STATE_PRESET_DC:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_PRESET;
		break;
	case DC_STATE_CHECK_ACTIVE:
	case DC_STATE_START_CC:
	case DC_STATE_ADJUST_TAVOL:
	case DC_STATE_ADJUST_TACUR:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST;
		break;
	case DC_STATE_CC_MODE:
	case DC_STATE_CV_MODE:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_ON;
		break;
	case DC_STATE_CHARGING_DONE:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_DONE;
		break;
	default:
		return -1;
	}

	if (prev_val == value.intval)
		return -1;

	prev_val = value.intval;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	psy_do_property(chg->pdata->sec_dc_name, set,
		POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, value);
#endif
	return 0;
}

static int hl7132_read_reg(struct hl7132_charger *halo, int reg, void *value)
{
	int ret = 0;

	mutex_lock(&halo->i2c_lock);
	ret = regmap_read(halo->regmap, reg, value);
	mutex_unlock(&halo->i2c_lock);

	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int hl7132_bulk_read_reg(struct hl7132_charger *chg, int reg, void *val, int count)
{
	int ret = 0;

	mutex_lock(&chg->i2c_lock);
	ret = regmap_bulk_read(chg->regmap, reg, val, count);
	mutex_unlock(&chg->i2c_lock);

	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int hl7132_write_reg(struct hl7132_charger *halo, int reg, u8 value)
{
	int ret = 0;

	mutex_lock(&halo->i2c_lock);
	ret = regmap_write(halo->regmap, reg, value);
	mutex_unlock(&halo->i2c_lock);

	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int hl7132_update_reg(struct hl7132_charger *chg, int reg, u8 mask, u8 val)
{
	int ret = 0;
	unsigned int temp;

	/* This is for raspberry pi only, it can be changed by customer */
	ret = hl7132_read_reg(chg, reg, &temp);
	if (ret < 0) {
		pr_err("failed to read reg 0x%x, ret= %d\n", reg, ret);
	} else {
		temp &= ~mask;
		temp |= val & mask;
		ret = hl7132_write_reg(chg, reg, temp);
		if(ret < 0)
			pr_err("failed to write reg0x%x, ret= %d\n", reg, ret);
	}
	return ret;

	/*
	mutex_lock(&halo->i2c_lock);
	ret = regmap_update_bits(halo->regmap, reg, mask, value);
	mutex_unlock(&halo->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
	*/
}

#if defined (CONFIG_OF)
static int hl7132_parse_dt(struct device *dev, struct hl7132_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int rc = 0;

	if (!np)
		return -EINVAL;

	pdata->irq_gpio = of_get_named_gpio(np, "hl7132,irq-gpio", 0);
	pr_info("[%s]:: irq-gpio:: %d \n",__func__,  pdata->irq_gpio);

	rc = of_property_read_u32(np, "hl7132,vbat-reg", &pdata->vbat_reg);
	if (rc) {
		pr_info("%s: failed to get vbat-reg from dtsi", __func__ );
		pdata->vbat_reg = HL7132_VBAT_REG_DFT;
	}
	pr_info("[%s]:: vbat-reg:: %d \n",__func__,  pdata->vbat_reg);

	rc = of_property_read_u32(np, "hl7132,input-current-limit", &pdata->iin_cfg);
	if (rc) {
		pr_info("%s: failed to get input-current-limit from dtsi", __func__ );
		pdata->iin_cfg = HL7132_IIN_CFG_DFT;
	}
	pr_info("[%s]:: input-current-limit:: %d \n",__func__,  pdata->iin_cfg);

	rc = of_property_read_u32(np, "hl7132,topoff-current", &pdata->iin_topoff);
	if (rc) {
		pr_info("%s: failed to get topoff current from dtsi", __func__ );
		pdata->iin_topoff = HL7132_IIN_DONE_DFT;
	}
	pr_info("[%s]:: topoff-current %d \n",__func__,  pdata->iin_topoff);

	pdata->ts_prot_en = of_property_read_bool(np, "hl7132,ts-prot-en");
	pr_info("%s:: ts-prot-en[%d]", __func__, pdata->ts_prot_en);

	pdata->all_auto_recovery = of_property_read_bool(np, "hl7132,all-auto-recovery");
	pr_info("%s:: all-auto-recovery[%d]", __func__, pdata->all_auto_recovery);

	rc = of_property_read_u32(np, "hl7132,adc-ctrl0", &pdata->adc_ctrl0);
	if (rc) {
		pr_info("%s: failed to get adc_ctrl0 from dtsi", __func__ );
		pdata->adc_ctrl0 = HL7132_ADC0_DFT;
	}
	pr_info("%s:: adc-ctrl0[0x%x]", __func__, pdata->adc_ctrl0);

	rc = of_property_read_u32(np, "hl7132,ts0-th", &pdata->ts0_th);
	if (rc) {
		pr_info("%s: failed to get ts0-th from dtsi", __func__ );
		pdata->ts0_th = HL7132_TS0_TH_DFT;
	}
	pr_info("%s:: ts0-th[0x%x]", __func__, pdata->ts0_th);

	rc = of_property_read_u32(np, "hl7132,ts1-th", &pdata->ts1_th);
	if (rc) {
		pr_info("%s: failed to get ts1-th from dtsi", __func__ );
		pdata->ts1_th = HL7132_TS1_TH_DFT;
	}
	pr_info("%s:: ts1-th[0x%x]", __func__, pdata->ts1_th);

	rc = of_property_read_u32(np, "hl7132,wd-tmr", &pdata->wd_tmr);
	if (rc) {
		pr_info("%s: failed to get wd_tmr from dtsi", __func__ );
		pdata->wd_tmr = WD_TMR_16S;
	}
	pr_info("%s:: wd-tmr[0x%x]", __func__, pdata->wd_tmr);

	pdata->wd_dis = of_property_read_bool(np, "hl7132,wd-dis");
	pr_info("%s: wd-dis[%d]\n", __func__, pdata->wd_dis);

	rc = of_property_read_u32(np, "hl7132,fsw-set", &pdata->fsw_cfg);
	if (rc) {
		pr_info("%s: failed to get fsw_set from dtsi", __func__);
		pdata->fsw_cfg = FSW_CFG_1100KHZ;
	}
	pdata->fsw_set = pdata->fsw_cfg;

	pr_info("%s:: fsw-set[0x%x]", __func__, pdata->fsw_set);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s: np(battery) NULL\n", __func__);
	} else {
		rc = of_property_read_string(np, "battery,charger_name",
				(char const **)&pdata->sec_dc_name);
		if (rc) {
			pr_err("%s: direct_charger is Empty\n", __func__);
			pdata->sec_dc_name = "sec-direct-charger";
		}
		pr_info("%s: battery,charger_name is %s\n", __func__, pdata->sec_dc_name);
	}
#endif

	rc = 0;

	return rc;
}
#else
static int hl7132_parse_dt(struct device *dev, struct hl7132_platform_data *pdata)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
char *charging_state_str[] = {
	"NO_CHARGING", "CHECK_VBAT", "PRESET_DC", "CHECK_ACTIVE", "ADJUST_CC",
	"START_CC", "CC_MODE", "CV_MODE", "CHARGING_DONE", "ADJUST_TAVOL",
	"ADJUST_TACUR"
#ifdef CONFIG_HALO_PASS_THROUGH
	, "PT_MODE", "PRESET_PT", "PRESET_CONFIG_PT"
#endif
};

static void hl7132_test_read(struct hl7132_charger *chg)
{
	pr_info("%s:\n", __func__);
}

static void hl7132_monitor_work(struct hl7132_charger *chg)
{
	int ta_vol = chg->ta_vol / HL7132_SEC_DENOM_U_M;
	int ta_cur = chg->ta_cur / HL7132_SEC_DENOM_U_M;

	if (chg->charging_state == DC_STATE_NOT_CHARGING)
		return;

	hl7132_read_adc(chg);
	pr_info("%s: state(%s),iin_cc(%dmA),vbat_reg(%dmV),vbat(%dmV),vin(%dmV),iin(%dmA),tdie(%d),pps_requested(%d/%dmV/%dmA)",
		__func__, charging_state_str[chg->charging_state],
		chg->iin_cc / HL7132_SEC_DENOM_U_M, chg->pdata->vbat_reg / HL7132_SEC_DENOM_U_M,
		chg->adc_vbat / HL7132_SEC_DENOM_U_M, chg->adc_vin / HL7132_SEC_DENOM_U_M,
		chg->adc_iin / HL7132_SEC_DENOM_U_M, chg->adc_tdie, chg->ta_objpos, ta_vol, ta_cur);
}
#endif

#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
extern int max77958_get_apdo_max_power(unsigned int *pdo_pos,
				unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr);
extern int max77958_select_pps(int num, int ppsVol, int ppsCur);
#endif

static int hl7132_get_apdo_max_power(struct hl7132_charger *chg)
{
	int ret = 0;
	//unsigned int ta_objpos = 0;
	unsigned int ta_max_vol = 0;
	unsigned int ta_max_cur = 0;
	unsigned int ta_max_pwr = 0;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	ta_max_vol = chg->ta_max_vol / HL7132_SEC_DENOM_U_M;

	ret = sec_pd_get_apdo_max_power(&chg->ta_objpos,
				&ta_max_vol, &ta_max_cur, &ta_max_pwr);
#else
	ret = max77958_get_apdo_max_power(&chg->ta_objpos,
				&ta_max_vol, &ta_max_cur, &ta_max_pwr);
#endif

	/* mA,mV,mW --> uA,uV,uW */
	chg->ta_max_vol = ta_max_vol * HL7132_SEC_DENOM_U_M;
	chg->ta_max_cur = ta_max_cur * HL7132_SEC_DENOM_U_M;
	chg->ta_max_pwr = ta_max_pwr;

	LOG_DBG(chg, "ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d\n",
			chg->ta_max_vol, chg->ta_max_cur, chg->ta_max_pwr);

	chg->pdo_index = chg->ta_objpos;
	chg->pdo_max_voltage = ta_max_vol;
	chg->pdo_max_current = ta_max_cur;
	return ret;
}

static int hl7132_send_pd_message(struct hl7132_charger *chg, unsigned int type)
{
	unsigned int pdoIndex, ppsVol, ppsCur;
	int ret = 0;

	//LOG_DBG(chg, "start!\n");

	cancel_delayed_work(&chg->pps_work);

	pdoIndex = chg->ta_objpos;

	ppsVol = chg->ta_vol / HL7132_SEC_DENOM_U_M;
	ppsCur = chg->ta_cur / HL7132_SEC_DENOM_U_M;
	LOG_DBG(chg, "msg_type=%d, pdoIndex=%d, ppsVol=%dmV(max_vol=%dmV), ppsCur=%dmA(max_cur=%dmA)\n",
		type, pdoIndex, ppsVol, chg->pdo_max_voltage, ppsCur, chg->pdo_max_current);

	if (chg->charging_state == DC_STATE_NOT_CHARGING)
		return ret;

	mutex_lock(&chg->lock);
	switch (type) {
	case PD_MSG_REQUEST_APDO:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		ret = sec_pd_select_pps(pdoIndex, ppsVol, ppsCur);
		if (ret == -EBUSY) {
			LOG_DBG(chg, "request again ret=%d\n", ret);

			msleep(100);
			ret = sec_pd_select_pps(pdoIndex, ppsVol, ppsCur);
		}
#else
		ret = max77958_select_pps(pdoIndex, ppsVol, ppsCur);
		if (ret == -EBUSY) {
			LOG_DBG(chg, "request again ret=%d\n", __func__, ret);

			msleep(100);
			ret = max77958_select_pps(pdoIndex, ppsVol, ppsCur);
		}
#endif
		/* Start pps request timer */
		if (ret == 0)
			schedule_delayed_work(&chg->pps_work, msecs_to_jiffies(HL7132_PPS_PERIODIC_T));

		break;

	case PD_MSG_REQUEST_FIXED_PDO:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		ret = sec_pd_select_pps(pdoIndex, ppsVol, ppsCur);
		if (ret == -EBUSY) {
			LOG_DBG(chg, "request again ret=%d\n", ret);
			msleep(100);
			ret = sec_pd_select_pps(pdoIndex, ppsVol, ppsCur);
		}
#else
		ret = max77958_select_pps(pdoIndex, ppsVol, ppsCur);
		if (ret == -EBUSY) {
			LOG_DBG(chg, "request again ret=%d\n", __func__, ret);
			msleep(100);
			ret = max77958_select_pps(pdoIndex, ppsVol, ppsCur);
		}
#endif
		break;

	default:
		break;
	}
	mutex_unlock(&chg->lock);
	LOG_DBG(chg, "ret = %d\n", ret);
	return ret;
}

#ifdef CONFIG_CHARGER_HL7019
static void hl7132_send_pd_msg_fixed_pdo(struct hl7132_charger *chg)
{
	int ret =0;
	int val;

	/* Adjust TA current and voltage step */
	val = chg->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
	chg->ta_vol = val*PD_MSG_TA_VOL_STEP;
	val = chg->ta_cur/PD_MSG_TA_CUR_STEP;	/* PPS current resolution is 50mA */
	chg->ta_cur = val*PD_MSG_TA_CUR_STEP;
	if (chg->ta_cur < HL7132_TA_MIN_CUR)	/* PPS minimum current is 1000mA */
		chg->ta_cur = HL7132_TA_MIN_CUR;

	/* Send PD Message */
	chg->ta_objpos = 0;
	ret = hl7132_send_pd_message(chg, PD_MSG_REQUEST_FIXED_PDO);
	if (ret < 0)
		goto error;

	return;
error:
	LOG_DBG(chg, "Can't send pdmsg!!!\n");
}

static void hl7132_preset_swcharger(struct hl7132_charger *chg)
{
	int ret = 0;

	hl7132_set_charging_state(chg, DC_STATE_NOT_CHARGING);

	chg->ta_max_cur = chg->pdata->iin_cfg;
	chg->ta_max_vol = HL7132_TA_MAX_VOL;
	chg->ta_objpos = 0;

	ret = hl7132_get_apdo_max_power(chg);
	if (ret < 0) {
		pr_err("%s: TA doesn't support any APDO\n", __func__);
		goto Err;
	}

	//input current 2A, charge current 2048mA, vbat reg 4400mV
	chg->ta_vol = 5000000;
	chg->ta_cur = 2000000;

	LOG_DBG(chg, "ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, TA-VOL[%d], TA-CUR[%d]\n",
			chg->ta_max_vol, chg->ta_max_cur,
			chg->ta_max_pwr, chg->iin_cc, chg->ta_vol, chg->ta_cur);

	msleep(200);
	hl7132_send_pd_msg_fixed_pdo(chg);

Err:
	LOG_DBG(chg, "End, ret =%d\n", ret);
}
#endif

#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static int hl7132_get_swcharger_property(enum power_supply_property prop,
				union power_supply_propval *val)
{
	int ret;
	struct power_supply *psy_sw;
	/* this code should be changed by customer, due to switching charger */
	/* only "hl7019-charger" needs to change to customer's switching charger */
	psy_sw = power_supply_get_by_name("hl7019-charger");
	if (psy_sw == NULL) {
		ret = -EINVAL;
		goto Err;
	}
	ret = power_supply_get_property(psy_sw, prop, val);

Err:
	pr_info("%s: End, ret = %d\n", __func__, ret);

	return ret;
}

static int hl7132_set_swcharger(struct hl7132_charger *chg, bool en, unsigned int input_current,
				unsigned int charging_current, unsigned int vbat_reg)
{
	int ret = 0;
	union power_supply_propval val;
	struct power_supply *psy_sw;

	pr_info("%s: en=%d, iin=%d, ichg=%d, vbat_reg=%d\n",
		__func__, en, input_current, charging_current, vbat_reg);

	psy_sw = power_supply_get_by_name("hl7019-charger");
	if (psy_sw == NULL) {
		pr_err("%s: cannot get power_supply_name hl7019-charger\n", __func__);
		ret = -ENODEV;
		goto error;
	}

	if (en == true)	{
		/* Set Switching charger */
		/* input current */
		val.intval = input_current;
		ret = power_supply_set_property(psy_sw, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
		if (ret < 0)
			goto error;
		/* charigng current */
		val.intval = charging_current;
		ret = power_supply_set_property(psy_sw, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
		if (ret < 0)
			goto error;
		/* vbat_reg voltage */
		val.intval = vbat_reg;
		ret = power_supply_set_property(psy_sw, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
		if (ret < 0)
			goto error;
#ifdef CONFIG_CHARGER_HL7019
		/*before sw-charger enabled, need to set apdo and set ta-vol,ta-cur for hl7019 charger */
		hl7132_preset_swcharger(chg);
#endif
		/* it depends on customer's code to enable charger */
		val.intval = en;
		ret = power_supply_set_property(psy_sw, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret < 0)
			goto error;
	} else {
		/* disable charger */
		/* it depends on customer's code to disable charger */
		val.intval = en;
		ret = power_supply_set_property(psy_sw, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret < 0)
			goto error;
	}

error:
	LOG_DBG(chg, "End, ret=%d\n", ret);
	return ret;
}
#else
static int hl7132_set_swcharger(struct hl7132_charger *chg, bool en)
{
	int ret = 0;
	union power_supply_propval value = {0, };

	value.intval = en;
	ret = psy_do_property(chg->pdata->sec_dc_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, value);

	if (ret < 0)
		pr_info("%s: error switching_charger, ret=%d\n", __func__, ret);

	return 0;
}
#endif

#ifdef CONFIG_FG_READING_FOR_CVMODE
static int hl7132_get_vbat_from_battery(struct hl7132_charger *chg)
{
	union power_supply_propval val;
	struct power_supply *psy_battery;
	int ret;
	int vbat;

	psy_battery = power_supply_get_by_name("battery");
	if (!psy_battery) {
		dev_err(chg->dev, "can't find battery power supply\n");
		LOG_DBG(chg, "Can't find battery power_supply\n");
		goto Err;
	}

	ret = power_supply_get_property(psy_battery, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	power_supply_put(psy_battery);
	if (ret < 0) {
		dev_err(chg->dev, "Can't get vbat from FG\n");
		LOG_DBG(chg, "Can't get Vbat from FG\n");
		goto Err;
	}
	vbat = val.intval;
	chg->vbat_fg = vbat;
	LOG_DBG(chg, "vbat[%d] from FG\n", chg->vbat_fg/1000);
	return 0;

Err:
	return -EINVAL;
}
#endif

static void hl7132_set_wdt_time(struct hl7132_charger *chg, int time)
{
	int ret = 0;
	unsigned int value;

	if (time < WD_TMR_2S) {
		LOG_DBG(chg, "WDT should be bigger than 2s\n");
		time = WD_TMR_2S;
	} else if (time > WD_TMR_16S) {
		LOG_DBG(chg, "WDT should be smaller than 16s\n");
		time = WD_TMR_16S;
	}

	value = time << MASK2SHIFT(BITS_WD_TMR);
	ret = hl7132_update_reg(chg, REG_CTRL14_2, BITS_WD_TMR, value);
	LOG_DBG(chg, "SET wdt time[%d], value[0x%x]", time, value);
}

static void hl7132_enable_wdt(struct hl7132_charger *chg, bool en)
{
	int ret = 0;
	unsigned int value;

	value = (!en) << MASK2SHIFT(BIT_WD_DIS);
	ret = hl7132_update_reg(chg, REG_CTRL14_2, BIT_WD_DIS, value);
	LOG_DBG(chg, "set WDT[%d]!! update[0x%x]\n", en, value);
}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static void hl7132_check_wdt_control(struct hl7132_charger *chg)
{
	struct device *dev = chg->dev;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (chg->wdt_kick) {
		hl7132_set_wdt_time(chg, chg->pdata->wd_tmr);
		schedule_delayed_work(&chg->wdt_control_work, msecs_to_jiffies(HL7132_BATT_WDT_CONTROL_T));
	} else {
		hl7132_set_wdt_time(chg, chg->pdata->wd_tmr);
		if (client->addr == 0xff)
			client->addr = 0x57;
	}
}

static void hl7132_wdt_control_work(struct work_struct *work)
{
	struct hl7132_charger *chg = container_of(work, struct hl7132_charger, wdt_control_work.work);
	struct device *dev = chg->dev;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	hl7132_set_wdt_time(chg, WD_TMR_4S);

	/* For kick Watchdog */
	hl7132_read_adc(chg);
	hl7132_send_pd_message(chg, PD_MSG_REQUEST_APDO);

	client->addr = 0xff;

	pr_info("%s: disable addr (vin:%duV, iin:%duA)\n",
		__func__, chg->adc_vin, chg->adc_iin);
}

static void hl7132_set_done(struct hl7132_charger *chg, bool enable)
{
	int ret = 0;
	union power_supply_propval value = {0, };

	value.intval = enable;
	ret = psy_do_property(chg->pdata->sec_dc_name, set,
			POWER_SUPPLY_EXT_PROP_DIRECT_DONE, value);

	if (ret < 0)
		pr_info("%s: error set_done, ret=%d\n", __func__, ret);
}
#endif

static int hl7132_set_input_current(struct hl7132_charger *chg, unsigned int iin)
{
	int ret = 0;
	unsigned int value = 0;

	iin = iin + HL7132_IIN_OFFSET_CUR; //add 200mA-offset
	if (iin >= HL7132_IIN_CFG_MAX)
		iin = HL7132_IIN_CFG_MAX;

	value = HL7132_IIN_TO_HEX(iin);

	ret = hl7132_update_reg(chg, REG_IIN_REG, BITS_IIN_REG_TH, value);
	LOG_DBG(chg, "iin hex [0x%x], real iin [%d]\n", value, iin);
	return ret;
}

static int hl7132_set_vbat_reg(struct hl7132_charger *chg, unsigned int vbat_reg)
{
	int ret, val;

	/* vbat regulation voltage */
	if (vbat_reg < HL7132_VBAT_REG_MIN) {
		vbat_reg = HL7132_VBAT_REG_MIN;
		pr_info("%s: -> Vbat-REG=%d\n", __func__, vbat_reg);
	}
	if (vbat_reg < HL7132_VBAT_REG_OFFSET)
		return -1;
	val = HL7132_VBAT_REG(vbat_reg);
	ret = hl7132_update_reg(chg, REG_VBAT_REG, BITS_VBAT_REG_TH, val);
	LOG_DBG(chg, "VBAT-REG hex:[0x%x], real:[%d]\n", val, vbat_reg);
	return ret;
}

const char *hl7132_get_fsw_str(int fsw)
{
	switch (fsw) {
	case FSW_CFG_500KHZ:
		return "500kHz";
	case FSW_CFG_600KHZ:
		return "600kHz";
	case FSW_CFG_700KHZ:
		return "700kHz";
	case FSW_CFG_800KHZ:
		return "800kHz";
	case FSW_CFG_900KHZ:
		return "900kHz";
	case FSW_CFG_1000KHZ:
		return "1000kHz";
	case FSW_CFG_1100KHZ:
		return "1100kHz";
	case FSW_CFG_1200KHZ:
		return "1200kHz";
	case FSW_CFG_1300KHZ:
		return "1300kHz";
	case FSW_CFG_1400KHZ:
		return "1400kHz";
	case FSW_CFG_1500KHZ:
		return "1500kHz";
	case FSW_CFG_1600KHZ:
		return "1600kHz";
	default:
		return "UNDEFINED";
	}
}

static int hl7132_set_fsw(struct hl7132_charger *chg)
{
	int ret  = 0;
	int iin_target = 0;
	unsigned int  value = 0;
	unsigned int r_val = 0;

	if (chg->pass_through_mode != PTM_NONE) {
		chg->pdata->fsw_set = FSW_CFG_800KHZ;
		LOG_DBG(chg, "fsw is 800khz in pass through mode\n");
	} else {
		/* check mode first! */
		if (chg->charging_state == DC_STATE_NOT_CHARGING)
			iin_target = chg->pdata->iin_cfg;
		else
			iin_target = chg->iin_cc;

		/* set fsw_set to the target */
		if (iin_target >= HL7132_CHECK_IIN_FOR_DEFAULT) {
			chg->pdata->fsw_set = chg->pdata->fsw_cfg;
		} else if ((iin_target >= HL7132_CHECK_IIN_FOR_800KHZ) && (iin_target < HL7132_CHECK_IIN_FOR_DEFAULT)) {
			chg->pdata->fsw_set = FSW_CFG_800KHZ;
		} else if ((iin_target >= HL7132_CHECK_IIN_FOR_600KHZ) && (iin_target < HL7132_CHECK_IIN_FOR_800KHZ)) {
			chg->pdata->fsw_set = FSW_CFG_600KHZ;
		} else {
			LOG_DBG(chg, "iin-taget is out of the range\n");
			chg->pdata->fsw_set = chg->pdata->fsw_cfg;
		}
		LOG_DBG(chg, "fsw is %s, iin_target[%d]\n", hl7132_get_fsw_str(chg->pdata->fsw_set), iin_target);
	}

	value = chg->pdata->fsw_set << MASK2SHIFT(BITS_FSW_SET);
	ret = hl7132_update_reg(chg, REG_CTRL12_0, (BITS_FSW_SET), value);

	/*verify to read */
	ret = hl7132_read_reg(chg, REG_CTRL12_0, &r_val);
	LOG_DBG(chg, "Reg[0x%x] value[0x%x]\n", REG_CTRL12_0, r_val);

	return ret;
}

static int hl7132_set_charging(struct hl7132_charger *chg, bool en)
{
	int ret = 0;
	u8 value = 0;
	unsigned int  r_val = 0;

	value = en ? BIT_CHG_EN : (0);
	ret = hl7132_update_reg(chg, REG_CTRL12_0, BIT_CHG_EN, value);
	if (ret < 0)
		goto Err;

	ret = hl7132_read_reg(chg, REG_CTRL12_0, &r_val);

Err:
	LOG_DBG(chg, "End, ret=%d, r_val=[%x]\n", ret, r_val);
	return ret;
}

static int hl7132_stop_charging(struct hl7132_charger *chg)
{
	int ret = 0;
	unsigned int value;

	if (chg->charging_state != DC_STATE_NOT_CHARGING){
		LOG_DBG(chg, "charging_state--> Not Charging\n");
		cancel_delayed_work(&chg->timer_work);
		cancel_delayed_work(&chg->pps_work);
#ifdef HL7132_STEP_CHARGING
		cancel_delayed_work(&chg->step_work);
		chg->current_step = STEP_DIS;
#endif

#ifdef CONFIG_HALO_PASS_THROUGH
		chg->pdata->fsw_set = chg->pdata->fsw_cfg;
		value = chg->pdata->fsw_set << MASK2SHIFT(BITS_FSW_SET);
		value = value | BIT_UNPLUG_DET_EN;
		ret = hl7132_update_reg(chg, REG_CTRL12_0, (BITS_FSW_SET | BIT_UNPLUG_DET_EN), value);
#endif

		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_ID_NONE;
		chg->timer_period = 0;
		mutex_unlock(&chg->lock);

		__pm_relax(chg->monitor_ws);

		hl7132_set_charging_state(chg, DC_STATE_NOT_CHARGING);
		chg->ret_state = DC_STATE_NOT_CHARGING;
		chg->ta_target_vol = HL7132_TA_MAX_VOL;
		chg->ta_v_ofs = 0;
		chg->prev_iin = 0;
		chg->prev_dec = false;
		chg->prev_inc = INC_NONE;

		mutex_lock(&chg->lock);
		chg->req_new_iin = false;
		chg->req_new_vbat_reg = false;
		mutex_unlock(&chg->lock);

		/* Requested by S/W team, comment the initialization out. 12-13, 2022 */
		/*
		chg->pdata->iin_cfg = HL7132_IIN_CFG_DFT;
		chg->pdata->vbat_reg = chg->pdata->vbat_reg_max;

		chg->new_vbat_reg = chg->pdata->vbat_reg;
		chg->new_iin = chg->pdata->iin_cfg;
		*/

		/* For the abnormal TA detection */
		chg->ab_prev_cur = 0;
		chg->ab_try_cnt = 0;
		chg->ab_ta_connected = false;
		chg->fault_sts_cnt = 0;
		chg->tp_set = 0;
#ifdef CONFIG_HALO_PASS_THROUGH
		chg->pdata->pass_through_mode = DC_NORMAL_MODE;
		chg->pass_through_mode = DC_NORMAL_MODE;

		/* Set new request flag for pass through mode */
		mutex_lock(&chg->lock);
		chg->req_pt_mode = false;
		mutex_unlock(&chg->lock);
#endif

		/* when TA disconnected, vin-uvlo occurred, so it will clear the status */
		ret = hl7132_read_reg(chg, REG_STATUS_A,  &value);
		if (ret < 0) {
			dev_err(chg->dev, "Failed to read reg_sts_a \n");
			return -EINVAL;
		}
		LOG_DBG(chg, "check status-a[0x%x]\n", value);

		ret = hl7132_set_charging(chg, false);

		hl7132_enable_wdt(chg, WDT_DISABLED);
	}

	LOG_DBG(chg, "End, ret=%d\n", ret);
	return ret;
}

static int hl7132_device_init(struct hl7132_charger *chg)
{
	int ret;
	unsigned int value;
	int i;

	ret = hl7132_read_reg(chg, REG_DEVICE_ID, &value);
	if (ret < 0){
		/*Workaround for Raspberry PI*/
		ret = hl7132_read_reg(chg, REG_DEVICE_ID, &value);
		if (ret < 0) {
			dev_err(chg->dev, "Read DEVICE_ID failed , val[0x%x]\n", value);
			return -EINVAL;
		}
	}
	dev_info(chg->dev, "Read DEVICE_ID [0x%x]\n", value);

	/* Disable IBAT regualtion */
	ret = hl7132_update_reg(chg, REG_CTRL11_1, BIT_IBAT_REG_DIS, BIT_IBAT_REG_DIS);
	if (ret < 0)
		goto I2C_FAIL;

	/* REG0x13 CTRL1 Setting */
	value = chg->pdata->ts_prot_en ? 0x10 : 0x00;
	value = chg->pdata->all_auto_recovery ? (value | 0x07) : (value | 0x00);

	ret = hl7132_write_reg(chg, REG_CTRL13_1, value);
	if (ret < 0)
		goto I2C_FAIL;

	/* Disable WDT function */
	hl7132_enable_wdt(chg, WDT_DISABLED);

	/* Setting for ADC_TRL_0 */
	value = chg->pdata->adc_ctrl0;
	ret = hl7132_write_reg(chg, REG_ADC_CTRL_0, value);
	if (ret < 0)
		goto I2C_FAIL;

	/* TS0_TH setting */
	pr_info("[%s]::ts0_th == [0x%x]\n", __func__, chg->pdata->ts0_th);
	value = ((chg->pdata->ts0_th) & 0xFF); //LSB
	ret = hl7132_write_reg(chg, REG_TS0_TH_0, value);
	if (ret < 0)
		goto I2C_FAIL;

	value = (chg->pdata->ts0_th >> 8) & 0x03; //MSB
	ret = hl7132_write_reg(chg, REG_TS0_TH_1, value);
	if (ret < 0)
		goto I2C_FAIL;

	/* TS1_TH setting */
	pr_info("ts1_th == [0x%x]\n", chg->pdata->ts1_th);
	value = (chg->pdata->ts1_th & 0xFF); //LSB
	ret = hl7132_write_reg(chg, REG_TS1_TH_0, value);
	if (ret < 0)
		goto I2C_FAIL;

	value = (chg->pdata->ts1_th >> 8) & 0x03; //MSB
	ret = hl7132_write_reg(chg, REG_TS1_TH_1, value);
	if (ret < 0)
		goto I2C_FAIL;

	/* input current limit */
	ret = hl7132_set_input_current(chg, chg->pdata->iin_cfg);
	if (ret < 0)
		goto I2C_FAIL;

	/* Set Vbat Regualtion */
	ret = hl7132_set_vbat_reg(chg, chg->pdata->vbat_reg);
	if (ret < 0)
		goto I2C_FAIL;

	/* default setting */
	chg->new_iin = chg->pdata->iin_cfg;
	chg->new_vbat_reg = chg->pdata->vbat_reg;

	/* clear new iin/vbat_reg flag */
	mutex_lock(&chg->lock);
	chg->req_new_iin = false;
	chg->req_new_vbat_reg = false;
	mutex_unlock(&chg->lock);

	/* Set FSW and Unplug detectoin */
	/* enable unplug detection and set 1100khz switching frequency */
	chg->pdata->fsw_set = chg->pdata->fsw_cfg;
	value = chg->pdata->fsw_set << MASK2SHIFT(BITS_FSW_SET);
	value = value | BIT_UNPLUG_DET_EN;
	ret = hl7132_update_reg(chg, REG_CTRL12_0, (BITS_FSW_SET | BIT_UNPLUG_DET_EN), value);

	chg->pdata->vbat_reg_max = chg->pdata->vbat_reg;
	pr_info("vbat-reg-max[%d]\n", chg->pdata->vbat_reg_max);

	/* For the abnormal TA detection */
	chg->ab_prev_cur = 0;
	chg->ab_try_cnt = 0;
	chg->ab_ta_connected = false;
	chg->fault_sts_cnt = 0;
	chg->tp_set = 0;
	chg->online = false;

#ifdef CONFIG_HALO_PASS_THROUGH
	chg->pass_through_mode = DC_NORMAL_MODE;
	chg->pdata->pass_through_mode = DC_NORMAL_MODE;
#endif

	for (i = 8; i < 0x1D; i++){
		hl7132_read_reg(chg, i, &value);
		pr_info("[%s] dbg--reg[0x%2x], val[0x%2x]\n", __func__,  i, value);
	}

	return ret;
I2C_FAIL:
	//LOG_DBG(chg, chg, "Failed to update i2c\r\n");
	pr_err("[%s]:: Failed to update i2c\r\n", __func__);
	return ret;
}

static int hl7132_get_adc_channel(struct hl7132_charger *chg, unsigned int channel)
{
	u8 r_data[2];
	u16 raw_adc;
	int ret = 0;

	/* Set AD_MAN_COPY BIT to read manually */
	ret = hl7132_update_reg(chg, REG_ADC_CTRL_0, BIT_ADC_MAN_COPY, BIT_ADC_MAN_COPY);
	if (ret < 0)
		goto Err;

	switch (channel) {
	case ADCCH_VIN:
		ret = hl7132_bulk_read_reg(chg, REG_ADC_VIN_0, r_data, 2);
		if (ret < 0)
			goto Err;

		raw_adc = ((r_data[0] << 2) | (r_data[1] & BITS_ADC_VIN_LSB));
		//conv_adc = raw_adc * HL7132_VIN_STEP;
		chg->adc_vin = raw_adc * HL7132_VIN_STEP;
		LOG_DBG(chg, "raw_adc[%d]VIN[%d]\n", raw_adc, chg->adc_vin);
		break;
	case ADCCH_IIN:
		ret = hl7132_bulk_read_reg(chg, REG_ADC_IIN_0, r_data, 2);
		if (ret < 0)
			goto Err;

		raw_adc = ((r_data[0] << 2) | (r_data[1] & BITS_ADC_IIN_LSB));
		//conv_adc = raw_adc * IIN_STEP;
		chg->adc_iin = ((raw_adc * HL7132_IIN_STEP) / 1000) * 1061;
		LOG_DBG(chg, "raw_adc[%d]IIN[%d]\n", raw_adc, chg->adc_iin);
		break;
	case ADCCH_VBAT:
		ret = hl7132_bulk_read_reg(chg, REG_ADC_VBAT_0, r_data, 2);
		if (ret < 0)
			goto Err;

		raw_adc = ((r_data[0] << 2) | (r_data[1] & BITS_ADC_VBAT_LSB));
		//conv_adc = raw_adc * VBAT_STEP;
		chg->adc_vbat = raw_adc * HL7132_VBAT_STEP;
		LOG_DBG(chg, "raw_adc[%d]VBAT[%d]\n", raw_adc, chg->adc_vbat);
		break;

	/* It depends on customer's scenario */
	case ADCCH_IBAT:
	case ADCCH_VOUT:
		break;
	case ADCCH_VTS:
		ret = hl7132_bulk_read_reg(chg, REG_ADC_VTS_0, r_data, 2);
		if (ret < 0)
			goto Err;

		raw_adc = ((r_data[0] << 2) | (r_data[1] & BITS_ADC_VTS_LSB));
		chg->adc_vts = raw_adc * HL7132_VTS_STEP;
		LOG_DBG(chg, "raw_adc[%d]VTS[%d]\n", raw_adc, chg->adc_vts);
		break;
	case ADCCH_TDIE:
		ret = hl7132_bulk_read_reg(chg, REG_ADC_TDIE_0, r_data, 2);
		if (ret < 0)
			goto Err;

		raw_adc = ((r_data[0] << 2) | (r_data[1] & BITS_ADC_TDIE_LSB));
		chg->adc_tdie = raw_adc * HL7132_TDIE_STEP / HL7132_TDIE_DENOM;
		LOG_DBG(chg, "raw_adc[%d]TDIE[%d]\n", raw_adc, chg->adc_tdie);
		break;
	default:
		LOG_DBG(chg, "unsupported channel[%d]\n", channel);
		break;
	}

Err:
	return ret;
}

static int hl7132_read_adc(struct hl7132_charger *chg)
{
	u8 r_data[2];
	u16 raw_adc;
	int ret = 0;

	/* Set AD_MAN_COPY BIT to read manually */
	ret = hl7132_update_reg(chg, REG_ADC_CTRL_0, BIT_ADC_MAN_COPY, BIT_ADC_MAN_COPY);
	if (ret < 0)
		goto Err;

	/* VIN ADC */
	ret = hl7132_bulk_read_reg(chg, REG_ADC_VIN_0, r_data, 2);
	if (ret < 0)
		goto Err;

	raw_adc = ((r_data[0] << 2) | (r_data[1] & BITS_ADC_VIN_LSB));
	//conv_adc = raw_adc * HL7132_VIN_STEP;
	chg->adc_vin = raw_adc * HL7132_VIN_STEP;

	/* IIN ADC */
	ret = hl7132_bulk_read_reg(chg, REG_ADC_IIN_0, r_data, 2);
	if (ret < 0)
		goto Err;

	raw_adc = ((r_data[0] << 2) | (r_data[1] & BITS_ADC_IIN_LSB));
	// because of too many IIN_REG Loop, charging current can be decreased. try not to touch IIN_REG loop
	chg->adc_iin = ((raw_adc * HL7132_IIN_STEP) / 1000) * 1061;

	/* VBAT ADC */
	ret = hl7132_bulk_read_reg(chg, REG_ADC_VBAT_0, r_data, 2);
	if (ret < 0)
		goto Err;

	raw_adc = ((r_data[0] << 2) | (r_data[1] & BITS_ADC_VBAT_LSB));
	//conv_adc = raw_adc * VBAT_STEP;
	chg->adc_vbat = raw_adc * HL7132_VBAT_STEP;

	/* VTS ADC */
	ret = hl7132_bulk_read_reg(chg, REG_ADC_VTS_0, r_data, 2);
	if (ret < 0)
		goto Err;

	raw_adc = ((r_data[0] << 2) | (r_data[1] & BITS_ADC_VTS_LSB));
	chg->adc_vts = raw_adc * HL7132_VTS_STEP;

	/* TDIE ADC */
	ret = hl7132_bulk_read_reg(chg, REG_ADC_TDIE_0, r_data, 2);
	if (ret < 0)
		goto Err;

	raw_adc = ((r_data[0] << 2) | (r_data[1] & BITS_ADC_TDIE_LSB));
	chg->adc_tdie = raw_adc * HL7132_TDIE_STEP / HL7132_TDIE_DENOM;

	/* VOUT ADC */
	ret = hl7132_bulk_read_reg(chg, REG_ADC_VOUT_0, r_data, 2);
	if (ret < 0)
		goto Err;

	raw_adc = ((r_data[0] << 2) | (r_data[1] & BITS_ADC_VOUT_LSB));
	chg->adc_vout = raw_adc * HL7132_VOUT_STEP;

	//  ADCCH_IBAT
	//  ADCCH_TDIE
	/* Don't need to read IBAT and TDIE*/

Err:
	//LOG_DBG(chg, "rValue:[0x%x][0x%x], raw_adc:[0x%x]\n",
	//            r_data[0], r_data[1], raw_adc);
	LOG_DBG(chg, "VIN:[%d], IIN:[%d], VBAT:[%d], VTS:[%d], TDIE:[%d], VOUT[%d]\n",
		chg->adc_vin, chg->adc_iin, chg->adc_vbat, chg->adc_vts, chg->adc_tdie, chg->adc_vout);

	return ret;
}

static irqreturn_t hl7132_interrupt_handler(int irg, void *data)
{
	struct hl7132_charger *chg = data;
	u8 r_buf[ENUM_INT_MAX];
	u8 masked;
	int ret = 0;
	bool handled = false;

	LOG_DBG(chg, "START!\n");

	ret = hl7132_bulk_read_reg(chg, REG_INT, r_buf, 4);
	if (ret < 0) {
		dev_warn(chg->dev, "can't read mult-bytes\n");
		return IRQ_NONE;
	}

	masked = r_buf[ENUM_INT] & r_buf[ENUM_INT_MASK];

	if (masked & BIT_STATE_CHG_I) {
		pr_info("[%s] dev_state changed!\n", __func__);
		handled = true;
	}

	if (masked & BIT_REG_I) {
		pr_info("[%s] regulation status changed!\n", __func__);
		handled = true;
	}

	if (masked & BIT_TS_TEMP_I) {
		pr_info("[%s] ts_temp_sts status changed!\n", __func__);
		handled = true;
	}

	if (masked & BIT_V_OK_I) {
		pr_info("[%s] V_OK_I changed!\n", __func__);
		handled = true;
	}

	if (masked & BIT_CUR_I) {
		pr_info("[%s] CUR_I changed!\n", __func__);
		handled = true;
	}

	if (masked & BIT_SHORT_I) {
		pr_info("[%s] SHORT_I changed!\n", __func__);
		handled = true;
	}

	if (masked & BIT_WDOG_I) {
		pr_info("[%s] WDOG_I changed!\n", __func__);
		handled = true;
	}

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int hl7132_irq_init(struct hl7132_charger *chg, struct i2c_client *client)
{
	struct hl7132_platform_data *pdata = chg->pdata;
	int ret, irq;

	//pr_info("start!\n");

	irq = gpio_to_irq(pdata->irq_gpio);
	ret = gpio_request_one(pdata->irq_gpio, GPIOF_IN, client->name);
	if (ret < 0)
		goto fail;

	ret = request_threaded_irq(irq, NULL, hl7132_interrupt_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				client->name, chg);
	if (ret < 0)
		goto fail_irq;

	client->irq = irq;

	disable_irq(irq);
	return 0;

fail_irq:
	pr_err("request_threaded_irq failed :: %d\n", ret);
	gpio_free(pdata->irq_gpio);
fail:
	pr_err("gpio_request failed :: ret->%d\n", ret);
	client->irq = 0;
	return ret;
}

static int hl7132_check_sts_reg(struct hl7132_charger *chg)
{
	int ret = 0;
	unsigned int sts_a;
	u8 r_state[4];
	int chg_state, mode;
	/* Abnormal TA detection Flag */
	bool ab_ta_flag = false;

	ret = hl7132_read_reg(chg, REG_INT_STS_A, &sts_a);
	if (ret < 0)
		goto Err;

	LOG_DBG(chg, "REG-STS-A[0x%x]\n", sts_a);

	chg_state = sts_a >> 6;
	mode = (sts_a & 0x3C) >> 2;

	chg->chg_state = chg_state;
	chg->reg_state = mode;

	if (chg_state != DEV_STATE_ACTIVE) {
		ret = hl7132_bulk_read_reg(chg, REG_INT_STS_B, r_state, 4);
		if (ret < 0)
			goto Err;

		if (chg_state == DEV_STATE_STANDBY)
			LOG_DBG(chg, "It's Standby state!!\n");

		LOG_DBG(chg, "CHG_STS:[%d], REG_STS:[%d], STS0x03:[0x%x]\n",
			chg_state, mode, sts_a);
		LOG_DBG(chg, "REG0x04:[0x%x],REG0x05:[0x%x], REG0x06:[0x%x], REG0x07:[0x%x]\n",
			r_state[0], r_state[1], r_state[2], r_state[3]);

		if (chg->pdata->ts_prot_en) {
			/* TS protection check in REG0x03 */
			if ((sts_a & BIT_TS_TEMP_STS) == BIT_TS_TEMP_STS)
				LOG_DBG(chg, "TS protection occurred!\n");
		}

		if ((r_state[0] & BIT_V_NOT_OK_STS) == BIT_V_NOT_OK_STS) {
			/*CHECK STATUS A REG */
			if ((r_state[1] & BIT_VIN_OVP_STS) == BIT_VIN_OVP_STS) {
				LOG_DBG(chg, "VIN > OVP!\n");
				ab_ta_flag = true;
			}
			if ((r_state[1] & BIT_VIN_UVLO_STS) == BIT_VIN_UVLO_STS)
				LOG_DBG(chg, "VIN < UVLO\n");

			if ((r_state[1] & BIT_TRACK_OV_STS) == BIT_TRACK_OV_STS)
				LOG_DBG(chg, "PMID > 2 X VOUT + TRACK_OV!\n");

			if ((r_state[1] & BIT_TRACK_UV_STS) == BIT_TRACK_UV_STS)
				LOG_DBG(chg, "PMID < 2 X VOUT - TRACK_UV!\n");

			if ((r_state[1] & BIT_VBAT_OVP_STS) == BIT_VBAT_OVP_STS)
				LOG_DBG(chg, "VBAT > VBAT_OVP_TH!\n");

			if ((r_state[1] & BIT_VOUT_OVP_STS) == BIT_VOUT_OVP_STS)
				LOG_DBG(chg, "VOUT > VOUT_OVP_TH!\n");

			if ((r_state[1] & BIT_PMID_QUAL_STS) == BIT_PMID_QUAL_STS)
				LOG_DBG(chg, "PMID is NOT regulated at 2 x VOUT during soft-start\n");
		}
		if ((r_state[0] & BIT_CUR_STS) == BIT_CUR_STS) {
			/* Check IIN_* of STATUS B*/
			if ((r_state[2] & BIT_IIN_OCP_STS) == BIT_IIN_OCP_STS) {
				LOG_DBG(chg, "IIN > IIN-OCP!\n");
				ab_ta_flag = true;
			}
			if ((r_state[2] & BIT_IBAT_OCP_STS) == BIT_IBAT_OCP_STS)
				LOG_DBG(chg, "IBAT > IBAT-OCP!\n");

			if ((r_state[2] & BIT_IIN_UCP_STS) == BIT_IIN_UCP_STS) {
				LOG_DBG(chg, "IIN > IIN-UCP\n");
				ab_ta_flag = true;
			}
		}
		if ((r_state[0] & BIT_SHORT_STS) == BIT_SHORT_STS) {
			/* Check short detect of STATU B */
			if ((r_state[2] & BIT_FET_SHORT_STS) == BIT_FET_SHORT_STS)
				LOG_DBG(chg, "Short Detected\n");

			if ((r_state[2] & BIT_CFLY_SHORT_STS) == BIT_CFLY_SHORT_STS)
				LOG_DBG(chg, "Flying Cap is shorted!\n");
		}

		/* WDT is expired! */
		if ((r_state[0] & BIT_WDOG_STS) == BIT_WDOG_STS)
			LOG_DBG(chg, "WDT is expired!!\n");

		/* THSD STS CHECK */
		if ((r_state[2] & BIT_THSD_STS) == BIT_THSD_STS)
			LOG_DBG(chg, "Device is under Thermal Shutdown!!\n");

		/* CHARGE-PUMP Fault */
		if ((r_state[3] & BIT_QPUMP_STS) == BIT_QPUMP_STS)
			LOG_DBG(chg, "Charge-pump fault!!\n");

		ret = -EINVAL;
	}
Err:
	/* fault_sts_cnt will be increased in CC-MODE, it will be 0 in other modes because charging stops */
	if ((ab_ta_flag) || (chg->tp_set == 1)) {
		chg->fault_sts_cnt++;
		LOG_DBG(chg, "fault_sts_cnt[%d]\n", chg->fault_sts_cnt);
		ret = -EINVAL;
	}
	LOG_DBG(chg, "End!, ret = %d\n", ret);
	return ret;
}


#ifdef HL7132_STEP_CHARGING
static void hl7132_step_charging_cccv_ctrl(struct hl7132_charger *chg)
{
	int vbat = chg->adc_vbat;

	if (vbat < HL7132_STEP1_VBAT_REG) {
		/* Step1 Charging! */
		chg->current_step = STEP_ONE;
		LOG_DBG(chg, "Step1 Charging Start!\n");
		chg->pdata->vbat_reg = HL7132_STEP1_VBAT_REG;
		chg->pdata->iin_cfg = HL7132_STEP1_TARGET_IIN;
		chg->pdata->iin_topoff = HL7132_STEP1_TOPOFF;
	} else if ((vbat >= HL7132_STEP1_VBAT_REG) && (vbat < HL7132_STEP2_VBAT_REG)) {
		/* Step2 Charging! */
		chg->current_step = STEP_TWO;
		LOG_DBG(chg, "Step2 Charging Start!\n");
		chg->pdata->vbat_reg = HL7132_STEP2_VBAT_REG;
		chg->pdata->iin_cfg = HL7132_STEP2_TARGET_IIN;
		chg->pdata->iin_topoff = HL7132_STEP2_TOPOFF;
	} else {
		/* Step3 Charging! */
		chg->current_step = STEP_THREE;
		LOG_DBG(chg, "Step3 Charging Start!\n");
		chg->pdata->vbat_reg = HL7132_STEP3_VBAT_REG;
		chg->pdata->iin_cfg = HL7132_STEP3_TARGET_IIN;
		chg->pdata->iin_topoff = HL7132_STEP3_TOPOFF;
	}
}
#endif

#ifdef CONFIG_HALO_PASS_THROUGH
static int hl7132_set_ptmode_ta_current(struct hl7132_charger *chg)
{
	int ret = 0;
	unsigned int val;

	hl7132_set_charging_state(chg, DC_STATE_PT_MODE);
	LOG_DBG(chg, "new_iin[%d]\n", chg->new_iin);

	chg->pdata->iin_cfg = chg->new_iin;
	chg->iin_cc = chg->new_iin;

	/* Set IIN_CFG to IIN_CC */
	ret = hl7132_set_input_current(chg, chg->iin_cc);
	if (ret < 0)
		goto Err;

	/* Clear Request flag */
	mutex_lock(&chg->lock);
	chg->req_new_iin = false;
	mutex_unlock(&chg->lock);

	val = chg->iin_cc/PD_MSG_TA_CUR_STEP;
	chg->iin_cc = val*PD_MSG_TA_CUR_STEP;
	chg->ta_cur = chg->iin_cc;

	LOG_DBG(chg, "ta_cur[%d], ta_vol[%d]\n", chg->ta_cur, chg->ta_vol);
	chg->iin_cc = chg->new_iin;

	mutex_lock(&chg->lock);
	chg->timer_id = TIMER_PDMSG_SEND;
	chg->timer_period = 0;
	mutex_unlock(&chg->lock);
	schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));

Err:
	LOG_DBG(chg, "ret = %d\n", ret);
	return ret;
}

static int hl7132_set_ptmode_ta_voltage(struct hl7132_charger *chg)
{
	int ret = 0;
	unsigned int val;
	int vbat;

	hl7132_set_charging_state(chg, DC_STATE_PT_MODE);

	LOG_DBG(chg, "new vbat-reg[%d]\n", chg->new_vbat_reg);

	ret = hl7132_read_adc(chg);
	if (ret < 0)
		goto Err;
	vbat = chg->adc_vbat;

	chg->pdata->vbat_reg = chg->new_vbat_reg;
	ret = hl7132_set_vbat_reg(chg, chg->pdata->vbat_reg);
	if (ret < 0)
		goto Err;

	/* clear req_new_vbat_reg */
	mutex_lock(&chg->lock);
	chg->req_new_vbat_reg = false;
	mutex_unlock(&chg->lock);


	val = (2*vbat);  // + HL7132_PT_VOL_PRE_OFFSET;
	chg->ta_vol = val;

	LOG_DBG(chg, "ta-vol[%d], ta_cur[%d], vbat[%d]\n", chg->ta_vol, chg->ta_cur, vbat);

	mutex_lock(&chg->lock);
	chg->timer_id = TIMER_PDMSG_SEND;
	chg->timer_period = 0;
	mutex_unlock(&chg->lock);
	schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));

Err:
	LOG_DBG(chg, "ret=%d\n", ret);
	return ret;
}

static int hl7132_set_ptmode_ta_voltage_by_soc(struct hl7132_charger *chg, int delta_soc)
{
	int ret = 0;
	unsigned int prev_ta_vol = chg->ta_vol;

	if (delta_soc > 0) { // decrease soc (ref_soc - soc_now)
		chg->ta_vol -= PD_MSG_TA_VOL_STEP;
	} else if (delta_soc < 0) { // increase soc (ref_soc - soc_now)
		chg->ta_vol += PD_MSG_TA_VOL_STEP;
	} else {
		pr_info("%s: abnormal delta_soc=%d\n", __func__, delta_soc);
		return -1;
	}

	pr_info("%s: delta_soc=%d, prev_ta_vol=%d, ta_vol=%d, ta_cur=%d\n",
		__func__, delta_soc, prev_ta_vol, chg->ta_vol, chg->ta_cur);

	/* Send PD Message */
	mutex_lock(&chg->lock);
	chg->timer_id = TIMER_PDMSG_SEND;
	chg->timer_period = 0;
	mutex_unlock(&chg->lock);
	schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));

	return ret;
}
#endif

static int hl7132_set_new_vbat_reg(struct hl7132_charger *chg)
{
	int ret = 0;
	int vbat;
	unsigned int val, update;

	/* Check the charging state */
	if (chg->charging_state == DC_STATE_NOT_CHARGING) {
		chg->pdata->vbat_reg = chg->new_vbat_reg;
	} else {
		if (chg->req_new_vbat_reg) {
			LOG_DBG(chg, "previous request is not finished yet\n");
			ret = -EBUSY;
		} else {
			//Workaround code for power off charging issue
			//because of too many requests, need to check the previous value here again.
			if (chg->new_vbat_reg == chg->pdata->vbat_reg) {
				LOG_DBG(chg, "new vbat-reg[%dmV] is same, No need to update\n", chg->new_vbat_reg/1000);
				/* cancel delayed_work and  restart queue work again */
				cancel_delayed_work(&chg->timer_work);
				mutex_lock(&chg->lock);
				if (chg->charging_state == DC_STATE_CC_MODE)
					chg->timer_id = TIMER_CHECK_CCMODE;
				else if (chg->charging_state == DC_STATE_CV_MODE)
					chg->timer_id = TIMER_CHECK_CVMODE;
				else if (chg->charging_state == DC_STATE_PT_MODE)
					chg->timer_id = TIMER_CHECK_PTMODE;
				chg->timer_period = 0;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
				return 0;
			}
			/* set new vbat reg voltage flag*/
			mutex_lock(&chg->lock);
			chg->req_new_vbat_reg = true;
			mutex_unlock(&chg->lock);

			/* checking charging state */
			if ((chg->charging_state == DC_STATE_CC_MODE) ||
				(chg->charging_state == DC_STATE_CV_MODE) ||
				(chg->charging_state == DC_STATE_PT_MODE)) {
				/* pass through mode */
				if (chg->pass_through_mode != PTM_NONE) {
					LOG_DBG(chg, "cancel queue and set ta-vol in ptmode\n");
					cancel_delayed_work(&chg->timer_work);
					ret = hl7132_set_ptmode_ta_voltage(chg);
				} else {
					ret = hl7132_read_adc(chg);
					if (ret < 0)
						goto Err;
					vbat = chg->adc_vbat;

					if (chg->new_vbat_reg > vbat) {
						cancel_delayed_work(&chg->timer_work);
						chg->pdata->vbat_reg = chg->new_vbat_reg;
						ret = hl7132_set_vbat_reg(chg, chg->pdata->vbat_reg);
						if (ret < 0)
							goto Err;
						/* save the current iin_cc to iin_cfg */
						chg->pdata->iin_cfg = chg->iin_cc;
						chg->pdata->iin_cfg = MIN(chg->pdata->iin_cfg, chg->ta_max_cur);
						ret = hl7132_set_input_current(chg, chg->pdata->iin_cfg);
						if (ret < 0)
							goto Err;

						chg->iin_cc = chg->pdata->iin_cfg;

						/* clear req_new_vbat_reg */
						mutex_lock(&chg->lock);
						chg->req_new_vbat_reg = false;
						mutex_unlock(&chg->lock);

						/*Calculate new TA Maxim Voltage */
						val = chg->iin_cc / PD_MSG_TA_CUR_STEP;
						chg->iin_cc = val * PD_MSG_TA_CUR_STEP;
						val = chg->ta_max_pwr/(chg->iin_cc/1000);
						val = val*1000/PD_MSG_TA_VOL_STEP;
						val = val*PD_MSG_TA_VOL_STEP;
						chg->ta_max_vol = MIN(val, HL7132_TA_MAX_VOL);
						chg->ta_vol = max(HL7132_TA_MIN_VOL_PRESET, ((2 * vbat) + HL7132_TA_VOL_PRE_OFFSET));
						val = chg->ta_vol / PD_MSG_TA_VOL_STEP;
						chg->ta_vol = val * PD_MSG_TA_VOL_STEP;
						chg->ta_vol = MIN(chg->ta_vol, chg->ta_max_vol);

						chg->ta_cur = chg->iin_cc;
						chg->iin_cc = chg->pdata->iin_cfg;
						LOG_DBG(chg, "ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d\n",
								chg->ta_max_vol, chg->ta_max_cur, chg->ta_max_pwr, chg->iin_cc);


						/* Clear the flag for ajdust cc */
						chg->prev_iin = 0;
						chg->prev_inc = INC_NONE;
						chg->prev_dec = false;

						/* W/A sometimes vbat-reg occurred with low vbat-adc in step1, */
						/* disable vbat-reg only in adjust cc  */
						ret = hl7132_update_reg(chg, REG_CTRL11_1, BIT_VBAT_REG_DIS, BIT_VBAT_REG_DIS);
						if (ret < 0)
							goto Err;
						ret = hl7132_read_reg(chg, REG_CTRL11_1, &update);
						LOG_DBG(chg, "reg[0x11] r_val[0x%x]\n", update);
						/*end W/A */

						hl7132_set_charging_state(chg, DC_STATE_ADJUST_CC);
						mutex_lock(&chg->lock);
						chg->timer_id = TIMER_PDMSG_SEND;
						chg->timer_period = 0;
						mutex_unlock(&chg->lock);
						schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
					} else {
						LOG_DBG(chg, "invalid vbat-reg[%d] higher than vbat[%d]!!\n",
							chg->new_vbat_reg, vbat);
						ret = -EINVAL;
					}
				}
			} else {
				LOG_DBG(chg, "Unsupported charging state[%d]\n", chg->charging_state);
			}
		}
	}
Err:
	LOG_DBG(chg, "ret = %d\n", ret);
	return ret;
}

static int hl7132_adjust_ta_current(struct hl7132_charger *chg)
{
	int ret = 0;
	int val, vbat;
	unsigned int update;

	hl7132_set_charging_state(chg, DC_STATE_ADJUST_TACUR);

	LOG_DBG(chg, "prev[%d], new[%d]\n", chg->pdata->iin_cfg, chg->iin_cc);
	if (chg->iin_cc > chg->ta_max_cur) {
		chg->iin_cc = chg->ta_max_cur;
		LOG_DBG(chg, "new IIN is higher than TA MAX Current!, iin-cc = ta_max_cur\n");
	}

	if (chg->ta_cur == chg->iin_cc) {
		/* Recover IIN_CC to the new_iin */
		chg->iin_cc = chg->new_iin;
		/* Clear req_new_iin */
		mutex_lock(&chg->lock);
		chg->req_new_iin = false;
		mutex_unlock(&chg->lock);

		/* return charging state to the previous state */
		hl7132_set_charging_state(chg, chg->ret_state);

		mutex_lock(&chg->lock);
		if (chg->charging_state == DC_STATE_CC_MODE)
			chg->timer_id = TIMER_CHECK_CCMODE;
		else if (chg->charging_state == DC_STATE_CV_MODE)
			chg->timer_id = TIMER_CHECK_CVMODE;
		chg->timer_period = 1000;
		mutex_unlock(&chg->lock);
	} else {
		chg->pdata->iin_cfg = chg->iin_cc;
		/* set IIN_CFG to new IIN */
		ret = hl7132_set_input_current(chg, chg->iin_cc);
		if (ret < 0)
			goto Err;
		/* Clear req_new_iin */
		mutex_lock(&chg->lock);
		chg->req_new_iin = false;
		mutex_unlock(&chg->lock);
		/* Read ADC-VBAT */
		ret = hl7132_read_adc(chg);
		if (ret < 0)
			goto Err;
		vbat = chg->adc_vbat;
		/* set IIN_CC to MIN(iin, ta_max_cur) */
		chg->iin_cc = MIN(chg->pdata->iin_cfg, chg->ta_max_cur);
		chg->pdata->iin_cfg = chg->iin_cc;

		/* Calculate new TA max current/voltage, it will be using in the dc charging */
		val = chg->iin_cc / PD_MSG_TA_CUR_STEP;
		chg->iin_cc = val * PD_MSG_TA_CUR_STEP;

		val = chg->ta_max_pwr/(chg->iin_cc/1000);
		val = val*1000/PD_MSG_TA_VOL_STEP;
		val = val*PD_MSG_TA_VOL_STEP;
		chg->ta_max_vol = MIN(val, HL7132_TA_MAX_VOL);
		/* Set TA_CV to MAX[(2*VBAT_ADC + 300mV), 7.5V] */
		chg->ta_vol = max(HL7132_TA_MIN_VOL_PRESET, ((2 * vbat) + HL7132_TA_VOL_PRE_OFFSET));
		val = chg->ta_vol / PD_MSG_TA_VOL_STEP;
		chg->ta_vol = val * PD_MSG_TA_VOL_STEP;
		chg->ta_vol = MIN(chg->ta_vol, chg->ta_max_vol);

		chg->ta_cur = chg->iin_cc;
		chg->iin_cc = chg->pdata->iin_cfg;

		LOG_DBG(chg, "ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d\n",
				chg->ta_max_vol, chg->ta_max_cur, chg->ta_max_pwr, chg->iin_cc);
		/* clear the flag for adjsut mode */
		chg->prev_iin = 0;
		chg->prev_inc = INC_NONE;

		/* W/A sometimes vbat-reg occurred with low vbat-adc in step1, disable vbat-reg only in adjust cc  */
		ret = hl7132_update_reg(chg, REG_CTRL11_1, BIT_VBAT_REG_DIS, BIT_VBAT_REG_DIS);
		if (ret < 0)
			goto Err;
		ret = hl7132_read_reg(chg, REG_CTRL11_1, &update);
		LOG_DBG(chg, "reg[0x11] r_val[0x%x]\n", update);
		/*end W/A */

		/* due to new ta-vol/ta-cur, should go to adjust cc, and pd msg  */
		hl7132_set_charging_state(chg, DC_STATE_ADJUST_CC);
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_PDMSG_SEND;
		chg->timer_period = 0;
		mutex_unlock(&chg->lock);
	}
	schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
Err:
	return ret;
}

static int hl7132_adjust_ta_voltage(struct hl7132_charger *chg)
{
	int ret = 0;
	int iin;

	hl7132_set_charging_state(chg, DC_STATE_ADJUST_TAVOL);

	ret = hl7132_read_adc(chg);
	if (ret < 0)
		goto Err;

	iin = chg->adc_iin;

	if (iin > (chg->iin_cc + PD_MSG_TA_CUR_STEP)) {
		/* TA-Cur is higher than target-input-current */
		/* Decrease TA voltage (20mV) */
		LOG_DBG(chg, "iin > iin_cc + 20mV, Dec VOl(20mV)\n");
		chg->ta_vol = chg->ta_vol - PD_MSG_TA_VOL_STEP;
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_PDMSG_SEND;
		chg->timer_period = 0;
		mutex_unlock(&chg->lock);
	} else {
		if (iin < (chg->iin_cc - PD_MSG_TA_CUR_STEP)) {
			/* TA-CUR is lower than target-input-current */
			/* Compare TA MAX Voltage */
			if (chg->ta_vol == chg->ta_max_vol) {
				/* Set TA current and voltage to the same value */
				LOG_DBG(chg, "ta-vol == ta-max_vol\n");
				/* Clear req_new_iin */
				mutex_lock(&chg->lock);
				chg->req_new_iin = false;
				mutex_unlock(&chg->lock);

				/* return charging state to the previous state */
				hl7132_set_charging_state(chg, chg->ret_state);

				/* pd msg */
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_PDMSG_SEND;
				chg->timer_period = 0;
				mutex_unlock(&chg->lock);
			} else {
				/* Increase TA voltage (20mV) */
				LOG_DBG(chg, "inc 20mV\n");
				chg->ta_vol = chg->ta_vol + PD_MSG_TA_VOL_STEP;
				/* pd msg */
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_PDMSG_SEND;
				chg->timer_period = 0;
				mutex_unlock(&chg->lock);
			}
		} else {
			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA  */
			/* IIN ADC is in valid range */
			/* Clear req_new_iin */
			mutex_lock(&chg->lock);
			chg->req_new_iin = false;
			mutex_unlock(&chg->lock);
			/* return charging state to the previous state */
			hl7132_set_charging_state(chg, chg->ret_state);

			LOG_DBG(chg, "invalid value! \n");
			mutex_lock(&chg->lock);
			chg->timer_id = TIMER_PDMSG_SEND;
			chg->timer_period = 0;
			mutex_unlock(&chg->lock);
		}
	}
	schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
	Err:

	return ret;
}

static int hl7132_set_new_iin(struct hl7132_charger *chg)
{
	int ret = 0;

	LOG_DBG(chg, "new_iin[%d]\n", chg->new_iin);

	if (chg->charging_state == DC_STATE_NOT_CHARGING) {
		chg->pdata->iin_cfg = chg->new_iin;
		/* change the switching frequency */
		hl7132_set_fsw(chg);
	} else {
		/* Checking the previous request! */
		if (chg->req_new_iin) {
			pr_err("[%s], previous request is not done yet!\n", __func__);
			ret = -EBUSY;
		} else {
			//Workaround code for power off charging issue
			//because of too many requests, need to check the previous value here again.
			if (chg->new_iin == chg->iin_cc) {
				LOG_DBG(chg, "new iin[%dmA] is same, No need to update\n", chg->new_iin/1000);
				/* cancel delayed_work and  restart queue work again */
				cancel_delayed_work(&chg->timer_work);
				mutex_lock(&chg->lock);
				if (chg->charging_state == DC_STATE_CC_MODE)
					chg->timer_id = TIMER_CHECK_CCMODE;
				else if (chg->charging_state == DC_STATE_CV_MODE)
					chg->timer_id = TIMER_CHECK_CVMODE;
				else if (chg->charging_state == DC_STATE_PT_MODE)
					chg->timer_id = TIMER_CHECK_PTMODE;
				chg->timer_period = 0;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
				return 0;
			}
			/* Set new request flag */
			mutex_lock(&chg->lock);
			chg->req_new_iin = true;
			mutex_unlock(&chg->lock);

			if ((chg->charging_state == DC_STATE_CC_MODE) ||
			(chg->charging_state == DC_STATE_CV_MODE) ||
			(chg->charging_state == DC_STATE_PT_MODE)) {
				if (chg->pass_through_mode != PTM_NONE) {
					cancel_delayed_work(&chg->timer_work);
					ret = hl7132_set_ptmode_ta_current(chg);
				} else {
					/* cancel delayed work */
					cancel_delayed_work(&chg->timer_work);
					/* set new IIN to IIN_CC */
					chg->iin_cc = chg->new_iin;
					/* store current state */
					chg->ret_state = chg->charging_state;

					if (chg->iin_cc < HL7132_TA_MIN_CUR) {
						/* Set ta-cur to 1A */
						chg->ta_cur = HL7132_TA_MIN_CUR;
						ret = hl7132_adjust_ta_voltage(chg);
					} else {
						/* Need to adjust the ta current */
						ret = hl7132_adjust_ta_current(chg);
					}
				}
				/* Change the switching frequency */
				hl7132_set_fsw(chg);
			} else {
				/* unsupported state! */
				LOG_DBG(chg, "unsupported charging state[%d]!\n", chg->charging_state);
			}
		}
	}

	LOG_DBG(chg, "ret = %d\n", ret);
	return ret;
}

static int hl7132_bad_ta_detection_trigger(struct hl7132_charger *chg)
{
	int ret = 0;
	int vbat;
	unsigned int val, update;

	if (chg->charging_state == DC_STATE_CC_MODE) {
		cancel_delayed_work(&chg->timer_work);
		ret = hl7132_read_adc(chg);
		if (ret < 0)
			goto Err;
		vbat = chg->adc_vbat;
		LOG_DBG(chg, "Vbat[%d]\n", vbat);

		/*Calculate new TA Maxim Voltage */
		val = chg->iin_cc / PD_MSG_TA_CUR_STEP;
		chg->iin_cc = val * PD_MSG_TA_CUR_STEP;
		val = chg->ta_max_pwr/(chg->iin_cc/1000);
		val = val*1000/PD_MSG_TA_VOL_STEP;
		val = val*PD_MSG_TA_VOL_STEP;
		chg->ta_max_vol = MIN(val, HL7132_TA_MAX_VOL);

		chg->ta_vol = max(HL7132_TA_MIN_VOL_PRESET, ((2 * vbat) + HL7132_TA_VOL_PRE_OFFSET));
		val = chg->ta_vol / PD_MSG_TA_VOL_STEP;
		chg->ta_vol = val * PD_MSG_TA_VOL_STEP;
		chg->ta_vol = MIN(chg->ta_vol, chg->ta_max_vol);

		chg->ta_cur = chg->iin_cc;
		chg->iin_cc = chg->pdata->iin_cfg;
		LOG_DBG(chg, "ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, TA-VOL[%d], TA-CUR[%d]\n",
			chg->ta_max_vol, chg->ta_max_cur, chg->ta_max_pwr,
			chg->iin_cc, chg->ta_vol, chg->ta_cur);

		/* Clear the flag for ajdust cc */
		chg->prev_iin = 0;
		chg->prev_inc = INC_NONE;
		chg->tp_set = 0;
		chg->prev_dec = false;

		/* W/A sometimes vbat-reg occurred with low vbat-adc in step1, disable vbat-reg only in adjust cc  */
		ret = hl7132_update_reg(chg, REG_CTRL11_1, BIT_VBAT_REG_DIS, BIT_VBAT_REG_DIS);
		if (ret < 0)
			goto Err;
		ret = hl7132_read_reg(chg, REG_CTRL11_1, &update);
		LOG_DBG(chg, "reg[0x11] r_val[0x%x]\n", update);
		/*end W/A */

		hl7132_set_charging_state(chg, DC_STATE_ADJUST_CC);
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_PDMSG_SEND;
		chg->timer_period = 0;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));

		ret = 0;
	} else {
		/* Don't need to do anything when chargingj state isn't in CCMODE */
		ret = -EINVAL;
	}

Err:
	LOG_DBG(chg, "ret=%d\n", ret);
	return ret;
}

#ifdef CONFIG_HALO_PASS_THROUGH
static int hl7132_set_pass_through_mode(struct hl7132_charger *chg)
{
	int ret = 0;
	unsigned int update;

	LOG_DBG(chg, "pass_through_mode change!\n");

	if (chg->charging_state == DC_STATE_NOT_CHARGING) {
		chg->pdata->pass_through_mode = chg->pass_through_mode;
	} else {
		if (chg->req_pt_mode) {
			pr_err("[%s], previous request is not done yet!\n", __func__);
			ret = -EBUSY;
		} else {
			/* Set new request flag for pass through mode */
			mutex_lock(&chg->lock);
			chg->req_pt_mode = true;
			mutex_unlock(&chg->lock);

			if ((chg->charging_state == DC_STATE_CC_MODE) || (chg->charging_state == DC_STATE_CV_MODE) ||
			(chg->charging_state == DC_STATE_ADJUST_CC) || (chg->charging_state == DC_STATE_PT_MODE)) {
				/* cancel delayed work */
				cancel_delayed_work(&chg->timer_work);
				if (chg->pass_through_mode != PTM_NONE) {
					/* Disable Charger */
					ret = hl7132_set_charging(chg, false);

					/* disable unplug detection and set 800khz switching frequency */
					chg->pdata->fsw_set = FSW_CFG_800KHZ;
					update = chg->pdata->fsw_set << MASK2SHIFT(BITS_FSW_SET);
					update = update & ~BIT_UNPLUG_DET_EN;
					ret = hl7132_update_reg(chg, REG_CTRL12_0, (BITS_FSW_SET | BIT_UNPLUG_DET_EN), update);
					if (ret < 0)
						goto Err;

					/* Enable hl7132 */
					ret = hl7132_set_charging(chg, true);
					if (ret < 0)
						goto Err;

					msleep(50);

					/*update the mode */
					chg->pdata->pass_through_mode = chg->pass_through_mode;

					/* Set new request flag for pass through mode */
					mutex_lock(&chg->lock);
					chg->req_pt_mode = false;
					mutex_unlock(&chg->lock);

					/* Move to Pass through mode */
					chg->charging_state = DC_STATE_PT_MODE;
					mutex_lock(&chg->lock);
					chg->timer_id = TIMER_CHECK_PTMODE;
					chg->timer_period = HL7132_PT_ACTIVE_DELAY_T;
					mutex_unlock(&chg->lock);
					schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
				} else {
					/*update the mode */
					chg->pdata->pass_through_mode = chg->pass_through_mode;

					/* Set new request flag for pass through mode */
					mutex_lock(&chg->lock);
					chg->req_pt_mode = false;
					mutex_unlock(&chg->lock);

					/* Restart charging with the flag_pass_through */
					mutex_lock(&chg->lock);
					chg->timer_id = TIMER_PRESET_DC;
					chg->timer_period = 0;
					mutex_unlock(&chg->lock);
					schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
				}
			} else {
				/* unsupported state! */
				LOG_DBG(chg, "unsupported charging state[%d]!\n", chg->charging_state);
			}
		}
	}
	return ret;

Err:
	LOG_DBG(chg, "End, ret=%d\n", ret);
	return ret;

}

static int hl7132_preset_config_pt(struct hl7132_charger *chg)
{
	int ret = 0;
	unsigned int update;

	LOG_DBG(chg, "Start!\n");
	hl7132_set_charging_state(chg, DC_STATE_PRESET_PT);
	/* disable unplug detection and set 800khz switching frequency */
	chg->pdata->fsw_set = FSW_CFG_800KHZ;
	update = chg->pdata->fsw_set << MASK2SHIFT(BITS_FSW_SET);
	update = update & ~BIT_UNPLUG_DET_EN;
	ret = hl7132_update_reg(chg, REG_CTRL12_0, (BITS_FSW_SET | BIT_UNPLUG_DET_EN), update);
	if (ret < 0)
		goto Err;

	/* Set IIN_CFG to IIN_CC */
	ret = hl7132_set_input_current(chg, chg->iin_cc);
	if (ret < 0)
		goto Err;

	/* Set Vbat Regualtion */
	ret = hl7132_set_vbat_reg(chg, chg->pdata->vbat_reg);
	if (ret < 0)
		goto Err;

	/* Enable hl7132 */
	ret = hl7132_set_charging(chg, true);
	if (ret < 0)
		goto Err;

	/* Move to Pass through mode */
	mutex_lock(&chg->lock);
	chg->timer_id = TIMER_CHECK_PTMODE;
	chg->timer_period = HL7132_PT_ACTIVE_DELAY_T;
	mutex_unlock(&chg->lock);
	schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
	return ret;

Err:
	LOG_DBG(chg, "Err occurred! ret[%d]\n", ret);
	return ret;
}

static int hl7132_preset_ptmode(struct hl7132_charger *chg)
{
	int ret = 0;
	int vbat;
	unsigned int val;

	LOG_DBG(chg, "Start!\n");

	hl7132_set_charging_state(chg, DC_STATE_PRESET_PT);

	ret = hl7132_read_adc(chg);
	if (ret < 0)
		goto Err;
	vbat = chg->adc_vbat;
	LOG_DBG(chg,  "Vbat is [%d]\n", vbat);

	chg->pdata->iin_cfg     = HL7132_PT_IIN_DFT;
	chg->pdata->vbat_reg    = HL7132_PT_VBAT_REG;

	chg->ta_max_cur = chg->pdata->iin_cfg;
	chg->ta_max_vol = HL7132_TA_MAX_VOL;
	chg->ta_objpos = 0;

	ret = hl7132_get_apdo_max_power(chg);
	if (ret < 0) {
		pr_err("%s: TA doesn't support any APDO\n", __func__);
		goto Err;
	}

	chg->iin_cc = MIN(chg->pdata->iin_cfg, chg->ta_max_cur);

	chg->pdata->iin_cfg = chg->iin_cc;

	val = chg->iin_cc / PD_MSG_TA_CUR_STEP;
	chg->iin_cc = val * PD_MSG_TA_CUR_STEP;

	val = chg->ta_max_pwr / (chg->iin_cc/1000); //mV
	val = (val * 1000) / PD_MSG_TA_VOL_STEP;
	val = val * PD_MSG_TA_VOL_STEP;
	chg->ta_max_vol = MIN(val, HL7132_TA_MAX_VOL);

	/* Set TA_CV to MAX[(2*VBAT_ADC + 200mV), 7.5V] */
	chg->ta_vol = max(HL7132_TA_MIN_VOL_PRESET, ((2 * vbat) + HL7132_PT_VOL_PRE_OFFSET));
	val = chg->ta_vol / PD_MSG_TA_VOL_STEP;
	chg->ta_vol = val * PD_MSG_TA_VOL_STEP;
	chg->ta_vol = MIN(chg->ta_vol, chg->ta_max_vol);

	chg->ta_cur = chg->iin_cc;
	chg->iin_cc = chg->pdata->iin_cfg;

	LOG_DBG(chg, "ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, TA-VOL[%d], TA-CUR[%d]\n",
		chg->ta_max_vol, chg->ta_max_cur,
		chg->ta_max_pwr, chg->iin_cc, chg->ta_vol, chg->ta_cur);

	mutex_lock(&chg->lock);
	chg->timer_id = TIMER_PDMSG_SEND;
	chg->timer_period = 0;
	mutex_unlock(&chg->lock);
	schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
	return ret;

Err:
	LOG_DBG(chg, "Err occurred! ret[%d]\n", ret);
	return ret;
}


static int hl7132_pass_through_mode_process(struct hl7132_charger *chg)
{
	int ret = 0;
	int iin, vbat, mode, dev_state;

	LOG_DBG(chg, "Start pt mode process!\n");
	hl7132_set_charging_state(chg, DC_STATE_PT_MODE);

	ret = hl7132_check_sts_reg(chg);
	if (ret < 0)
		goto Err;
	mode = chg->reg_state;
	dev_state = chg->chg_state;

	if (chg->req_pt_mode) {
		LOG_DBG(chg, "update pt_mode flag in pt mode\n");
		mutex_lock(&chg->lock);
		chg->req_pt_mode = false;
		mutex_unlock(&chg->lock);
		ret = hl7132_set_pass_through_mode(chg);
	} else if (chg->req_new_vbat_reg) {
		LOG_DBG(chg, "update new vbat in pt mode\n");
		mutex_lock(&chg->lock);
		chg->req_new_vbat_reg = false;
		mutex_unlock(&chg->lock);
		ret = hl7132_set_new_vbat_reg(chg);
	} else if (chg->req_new_iin) {
		LOG_DBG(chg, "update new iin in pt mode\n");
		mutex_lock(&chg->lock);
		chg->req_new_iin = false;
		mutex_unlock(&chg->lock);
		ret = hl7132_set_new_iin(chg);
	} else {
		ret = hl7132_read_adc(chg);
		if  (ret < 0)
			goto Err;
		iin = chg->adc_iin;//hl7132_read_adc(chg, ADCCH_IIN);
		vbat = chg->adc_vbat;//hl7132_read_adc(chg, ADCCH_VBAT);

		switch (mode) {
		case INACTIVE_LOOP:
			LOG_DBG(chg, "Inactive loop\n");
			break;
		case VBAT_REG_LOOP:
		case VBAT_IBAT_LOOP:
		case VBAT_TDIE_LOOP:
			LOG_DBG(chg, "vbat-reg loop in pass through mode\n");
			break;
		case IIN_REG_LOOP:
		case IIN_TDIE_LOOP:
		case IIN_IBAT_LOOP:
		case IIN_VBAT_TDIE_IBAT_LOOP:
		case IBAT_TDIE_LOOP:
		case IIN_IBAT_TDIE_LOOP:
		case T_DIE_REG_LOOP:
		case VBAT_IIN_LOOP:
		case VBAT_IIN_IBAT_LOOP:
			LOG_DBG(chg, "iin-reg loop in pass through mode\n");
			if (chg->ta_cur <= chg->iin_cc - HL7132_PT_TA_CUR_LOW_OFFSET) {
				/* IIN_LOOP still happens even though TA current is less than IIN_CC - 200mA */
				chg->ta_vol = chg->ta_vol - PD_MSG_TA_VOL_STEP;
				LOG_DBG(chg, "[iin-reg]Abnormal behavior! iin[%d], TA-CUR[%d], TA-VOL[%d]\n",
				iin, chg->ta_cur, chg->ta_vol);
			} else {
				chg->ta_cur = chg->ta_cur - PD_MSG_TA_CUR_STEP;
				LOG_DBG(chg, "[iin-reg] iin[%d], TA-CUR[%d], TA-VOL[%d]\n", iin, chg->ta_cur, chg->ta_vol);
			}
			mutex_lock(&chg->lock);
			chg->timer_id = TIMER_PDMSG_SEND;
			chg->timer_period = 0;
			mutex_unlock(&chg->lock);
			schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			return ret;
		case IBAT_REG_LOOP:
			LOG_DBG(chg, "ibat-reg loop in pass through mode\n");
			break;
		default:
			LOG_DBG(chg, "No cases!\n");
			break;
		}
		/* Because PD IC can't know unpluged-detection, will send pd-msg every 1secs */
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_PDMSG_SEND;
		chg->timer_period = HL7132_PTMODE_UNPLUGED_DETECTION_T;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));

		/* every 10s, Pass through mode function works */
		/*
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_CHECK_PTMODE;
		chg->timer_period = HL7132_PTMODE_DELAY_T;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
		*/
	}

Err:
	return ret;
}
#endif

static int hl7132_preset_dcmode(struct hl7132_charger *chg)
{
	int vbat;
	int ret = 0;
	unsigned int val;

	LOG_DBG(chg, "Start!\n");

	hl7132_set_charging_state(chg, DC_STATE_PRESET_DC);

	ret = hl7132_read_adc(chg);
	if (ret < 0)
		goto Err;

	vbat = chg->adc_vbat;
	LOG_DBG(chg, "Vbat is [%d]\n", vbat);

#ifdef HL7132_STEP_CHARGING
	hl7132_step_charging_cccv_ctrl(chg);
#endif
	//need to check vbat to verify the start level of DC charging
	//need to check whether DC mode or not.

	chg->ta_max_cur = chg->pdata->iin_cfg;
	chg->ta_max_vol = HL7132_TA_MAX_VOL;
	chg->ta_objpos = 0;

	ret = hl7132_get_apdo_max_power(chg);
	if (ret < 0) {
		pr_err("%s: TA doesn't support any APDO\n", __func__);
		goto Err;
	}

	chg->iin_cc = MIN(chg->pdata->iin_cfg, chg->ta_max_cur);
	chg->pdata->iin_cfg = chg->iin_cc;

	val = chg->iin_cc / PD_MSG_TA_CUR_STEP;
	chg->iin_cc = val * PD_MSG_TA_CUR_STEP;

	val = chg->ta_max_pwr / (chg->iin_cc/1000); //mV
	val = (val * 1000) / PD_MSG_TA_VOL_STEP;
	val = val * PD_MSG_TA_VOL_STEP;

	chg->ta_max_vol = MIN(val, HL7132_TA_MAX_VOL);

	/* Set TA_CV to MAX[(2*VBAT_ADC + 300mV), 7.5V] */
	chg->ta_vol = max(HL7132_TA_MIN_VOL_PRESET, ((2 * vbat) + HL7132_TA_VOL_PRE_OFFSET));
	val = chg->ta_vol / PD_MSG_TA_VOL_STEP;
	chg->ta_vol = val * PD_MSG_TA_VOL_STEP;
	chg->ta_vol = MIN(chg->ta_vol, chg->ta_max_vol);
	chg->ta_cur = chg->iin_cc;
	chg->iin_cc = chg->pdata->iin_cfg;

	LOG_DBG(chg, "ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, TA-VOL[%d], TA-CUR[%d]\n",
	chg->ta_max_vol, chg->ta_max_cur,
	chg->ta_max_pwr, chg->iin_cc, chg->ta_vol, chg->ta_cur);


	mutex_lock(&chg->lock);
	chg->timer_id = TIMER_PDMSG_SEND;
	chg->timer_period = 0;
	mutex_unlock(&chg->lock);
	schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));

Err:
	LOG_DBG(chg, "End, ret =%d\n", ret);
	return ret;
}

static int hl7132_preset_config(struct hl7132_charger *chg)
{
 	int ret = 0;
	unsigned int update;

	LOG_DBG(chg, "Start!, set iin:[%d], Vbat-REG:[%d]\n", chg->iin_cc, chg->pdata->vbat_reg);

	hl7132_set_charging_state(chg, DC_STATE_PRESET_DC);

	/* enable unplug detection and set 1100khz switching frequency */
	chg->pdata->fsw_set = chg->pdata->fsw_cfg;
	update = chg->pdata->fsw_set << MASK2SHIFT(BITS_FSW_SET);
	update = update | BIT_UNPLUG_DET_EN;
	ret = hl7132_update_reg(chg, REG_CTRL12_0, (BITS_FSW_SET | BIT_UNPLUG_DET_EN), update);
	if (ret < 0)
		goto error;

	/* Set IIN_CFG to IIN_CC */
	ret = hl7132_set_input_current(chg, chg->iin_cc);
	if (ret < 0)
		goto error;


	/* Set Vbat Regualtion */
	ret = hl7132_set_vbat_reg(chg, chg->pdata->vbat_reg);
	if (ret < 0)
		goto error;

	/* Enable hl7132 */
	ret = hl7132_set_charging(chg, true);
	if (ret < 0)
		goto error;

	/* W/A sometimes vbat-reg occurred with low vbat-adc in step1, disable vbat-reg only in adjust cc  */
	ret = hl7132_update_reg(chg, REG_CTRL11_1, BIT_VBAT_REG_DIS, BIT_VBAT_REG_DIS);
	if (ret < 0)
		goto error;
	ret = hl7132_read_reg(chg, REG_CTRL11_1, &update);
	LOG_DBG(chg, "reg[0x11] r_val[0x%x]\n", update);
	/*end W/A */

	/* Clear previous iin adc */
	chg->prev_iin = 0;

	/* Clear TA increment flag */
	chg->prev_inc = INC_NONE;
	chg->prev_dec = false;

	/* Go to CHECK_ACTIVE state after 150ms*/
	mutex_lock(&chg->lock);
	chg->timer_id = TIMER_CHECK_ACTIVE;
	chg->timer_period = HL7132_CHECK_ACTIVE_DELAY_T;
	mutex_unlock(&chg->lock);
	schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
	ret = 0;

error:
	LOG_DBG(chg, "End, ret=%d\n", ret);
	return ret;
}

static int hl7132_start_dc_charging(struct hl7132_charger *chg)
{
	int ret;
	unsigned int value;
	u8 r_sts[4];

	LOG_DBG(chg, "start!\n");

	/* before charging starts, read on clear the status registers */
	ret = hl7132_bulk_read_reg(chg, REG_INT_STS_B, r_sts, 4);
	LOG_DBG(chg, "sts0[0x%x], sts1[0x%x], sts2[0x%x], sts3[0x%x]\n", r_sts[0], r_sts[1], r_sts[2], r_sts[3]);
	if (ret < 0)
		return ret;

	/* Disable IBAT regualtion  for the test */
	ret = hl7132_update_reg(chg, REG_CTRL11_1, BIT_IBAT_REG_DIS, BIT_IBAT_REG_DIS);
	if (ret < 0)
		return ret;

	/* REG0x13 CTRL1 Setting */
	value = chg->pdata->ts_prot_en ? 0x10 : 0x00;
	value = chg->pdata->all_auto_recovery ? (value | 0x07) : (value | 0x00);
	ret = hl7132_write_reg(chg, REG_CTRL13_1, value);
	if (ret < 0)
		return ret;

	if (chg->pdata->wd_dis) {
		/* Disable WDT function */
		hl7132_enable_wdt(chg, WDT_DISABLED);
	} else {
		hl7132_enable_wdt(chg, WDT_ENABLED);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		hl7132_check_wdt_control(chg);
#else
		hl7132_set_wdt_time(chg, chg->pdata->wd_tmr);
#endif
	}

	/* Setting for ADC_TRL_0 */
	value = chg->pdata->adc_ctrl0;
	ret = hl7132_write_reg(chg, REG_ADC_CTRL_0, value);
	if (ret < 0)
		return ret;

	/* TS0_TH setting*/
	value = ((chg->pdata->ts0_th) & 0xFF ); //LSB
	ret = hl7132_write_reg(chg, REG_TS0_TH_0, value);
	if (ret < 0)
		return ret;

	value = (chg->pdata->ts0_th >> 8) & 0x03; //MSB
	ret = hl7132_write_reg(chg, REG_TS0_TH_1, value);
	if (ret < 0)
		return ret;

	/* TS1_TH setting */
	value = (chg->pdata->ts1_th & 0xFF); //LSB
	ret = hl7132_write_reg(chg, REG_TS1_TH_0, value);
	if (ret < 0)
		return ret;

	value = (chg->pdata->ts1_th >> 8) & 0x03; //MSB
	ret = hl7132_write_reg(chg, REG_TS1_TH_1, value);
	if (ret < 0)
		return ret;

	/* Enable ADC read and adc_reg_copy*/
	ret = hl7132_write_reg(chg, REG_ADC_CTRL_0, HL7132_ADC0_DFT);
	if (ret < 0)
		return ret;

	__pm_stay_awake(chg->monitor_ws);

#ifdef CONFIG_HALO_PASS_THROUGH
	if (chg->pdata->pass_through_mode != PTM_NONE)
		ret = hl7132_preset_ptmode(chg);
	else
		ret = hl7132_preset_dcmode(chg);
#else
	ret = hl7132_preset_dcmode(chg);
#endif

	LOG_DBG(chg, "End, ret=%d\n", ret);
	return ret;
}

static int hl7132_check_starting_vbat_level(struct hl7132_charger *chg)
{
	int ret = 0;
	int vbat;
	union power_supply_propval val;

	hl7132_set_charging_state(chg, DC_STATE_CHECK_VBAT);

	ret = hl7132_read_adc(chg);
	if (ret < 0)
		goto EXIT;

	vbat = chg->adc_vbat;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	ret = psy_do_property(chg->pdata->sec_dc_name, get,
			POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
#else
	ret = hl7132_get_swcharger_property(POWER_SUPPLY_PROP_ONLINE, &val);
#endif
	if (ret < 0) {
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_VBATMIN_CHECK;
		chg->timer_period = HL7132_VBATMIN_CHECK_T;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
		LOG_DBG(chg, "TIMER_VBATMIN_CHECK again!!\n");
		goto EXIT;
	}

	if (val.intval == 0) {
#ifdef CONFIG_CHARGER_HL7019
		/* S W charger is already disabled!*/
		if (vbat < HL7132_DC_VBAT_MIN) {
			//input current 2A, charge current 2048mA, vbat reg 4400mV
			ret = hl7132_set_swcharger(chg, true, 2000000, 2048000, 4400000);
			goto EXIT;
		}
#endif
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_PRESET_DC;
		chg->timer_period = 0;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
	} else {
	/* Switching charger is not disabled */
		if (vbat > HL7132_DC_VBAT_MIN)
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			ret = hl7132_set_swcharger(chg, false);
#else
			ret = hl7132_set_swcharger(chg, false, 0, 0, 0);
#endif

		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_VBATMIN_CHECK;
		chg->timer_period = HL7132_VBATMIN_CHECK_T;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
	}

EXIT:
	LOG_DBG(chg, "End, ret = %d\n", ret);
	return ret;
}

static int hl7132_check_active_state(struct hl7132_charger *chg)
{
	int ret = 0;

	LOG_DBG(chg, "start!\n");

	hl7132_set_charging_state(chg, DC_STATE_CHECK_ACTIVE);

	ret = hl7132_check_sts_reg(chg);
	if (ret < 0)
		goto Err;

	if (chg->chg_state == DEV_STATE_ACTIVE) {
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_ADJUST_CCMODE;
		chg->timer_period = 0;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
		ret = 0;
	} else if ((chg->chg_state == DEV_STATE_STANDBY) || (chg->chg_state == DEV_STATE_SHUTDOWN)
		|| (chg->chg_state == DEV_STATE_RESET)) {
		ret = hl7132_set_charging(chg, false);
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_PRESET_DC;
		chg->timer_period = 0;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
		ret = 0;
	} else {
		LOG_DBG(chg, "invalid charging state=%d\n", chg->chg_state);
		ret = -EINVAL;
	}

Err:
	LOG_DBG(chg, "End!, ret = %d\n", ret);
	return ret;
}

static int hl7132_adjust_ccmode(struct hl7132_charger *chg)
{
	int iin, ccmode;
	int vbat, val;
	int ret = 0;

	LOG_DBG(chg, "start!\n");

	hl7132_set_charging_state(chg, DC_STATE_ADJUST_CC);

	ret = hl7132_check_sts_reg(chg);
	if (ret != 0)
		goto error;	// This is not active mode.

	ccmode = chg->reg_state;

	ret = hl7132_read_adc(chg);
	if (ret < 0)
		goto error;

	iin = chg->adc_iin;
	vbat = chg->adc_vbat;
	LOG_DBG(chg, "iin_target:[%d], ta-vol:[%d], ta-cur:[%d]", chg->iin_cc, chg->ta_vol, chg->ta_cur);
	LOG_DBG(chg, "IIN: [%d], VBAT: [%d]\n", iin, vbat);

	if (vbat >= chg->pdata->vbat_reg) {
		chg->ta_target_vol = chg->ta_vol;
		LOG_DBG(chg, "Adjust end!!(VBAT >= vbat_reg) ta_target_vol [%d]", chg->ta_target_vol);
		chg->prev_inc = INC_NONE;
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_CHECK_CVMODE;
		chg->timer_period = 0;
		mutex_unlock(&chg->lock);
		chg->prev_iin = 0;
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
		goto error;
	}

	if (chg->prev_dec) {
		chg->prev_dec = false;
		//exit adjust mode
		//Calculate TA_V_ofs = TA_CV -(2 x VBAT_ADC) + 0.1V
#ifdef HL7132_STEP_CHARGING
		if (chg->ta_v_ofs == 0)
			chg->ta_v_ofs = (chg->ta_vol - (2 * vbat)) + HL7132_TA_V_OFFSET;
#else
		chg->ta_v_ofs = (chg->ta_vol - (2 * vbat)) + HL7132_TA_V_OFFSET;
#endif
		val = (vbat * 2) + chg->ta_v_ofs;
		val = val/PD_MSG_TA_VOL_STEP;
		chg->ta_target_vol = val*PD_MSG_TA_VOL_STEP;
		if (chg->ta_target_vol > chg->ta_max_vol)
			chg->ta_target_vol = chg->ta_max_vol;
		chg->ta_vol = chg->ta_target_vol;
		LOG_DBG(chg, "Ener CC, already increased TA_CV! TA-V-OFS:[%d]\n", chg->ta_v_ofs);
		chg->prev_inc = INC_NONE;
		chg->prev_iin = 0;
		hl7132_set_charging_state(chg, DC_STATE_START_CC);

		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_ENTER_CCMODE;
		chg->timer_period = 0;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
		goto error;
	}

	switch(ccmode) {
	case INACTIVE_LOOP:
		//IIN_ADC > IIN_TG + 20mA
		if (iin  > (chg->iin_cc + 25000)) {
			// Decrease TA_CC (50mA)
			chg->ta_cur = chg->ta_cur - PD_MSG_TA_CUR_STEP;
			LOG_DBG(chg, "(IIN>IIN TARGET+20mA), ta-cur[%d], ta-vol[%d], got ta_v_ofs\n",
			chg->ta_cur, chg->ta_vol);
			chg->prev_dec = true;
			//send pd msg
			mutex_lock(&chg->lock);
			chg->timer_id = TIMER_PDMSG_SEND;
			chg->timer_period = 0;
			mutex_unlock(&chg->lock);
			goto EXIT;
		} else {
			//|(IIN_ADC - IIN_TG)| < 25mA?
			if (abs(iin - chg->iin_cc) < 25000) {
				LOG_DBG(chg, "iin delta is < 21mA\n");
				goto ENTER_CC;
			} else {
				if (chg->ta_vol >= chg->ta_max_vol) {
					LOG_DBG(chg, "TA-VOL > TA-MAX-VOL\n");
					goto ENTER_CC;
				} else {
					if (iin > (chg->prev_iin + 20000)) {
						//Increase TA_CV (40mV)
						LOG_DBG(chg, "iin[%d] > prev[%d]+20000, increase TA_CV\n", iin, chg->prev_iin);
						chg->ta_vol = chg->ta_vol + HL7132_TA_VOL_STEP_ADJ_CC;
						chg->prev_iin = iin;
						chg->prev_inc = INC_TA_VOL;
						//send pd msg
						mutex_lock(&chg->lock);
						chg->timer_id = TIMER_PDMSG_SEND;
						chg->timer_period = 0;
						mutex_unlock(&chg->lock);
						goto EXIT;
					} else {
						if (chg->prev_inc == INC_TA_CUR) {
							//Increase TA_CV (40mV)
							chg->ta_vol = chg->ta_vol + HL7132_TA_VOL_STEP_ADJ_CC;
							if (chg->ta_vol > chg->ta_max_vol)
								chg->ta_vol = chg->ta_max_vol;
							LOG_DBG(chg, "pre_inc == TA-CUR, Now inc =  TA-VOL[%dmV]\n", chg->ta_vol);
							chg->prev_inc = INC_TA_VOL;

							//send pd msg
							mutex_lock(&chg->lock);
							chg->timer_id = TIMER_PDMSG_SEND;
							chg->timer_period = 0;
							mutex_unlock(&chg->lock);
							goto EXIT;
						} else {
							if (chg->ta_cur >= chg->ta_max_cur) {
								LOG_DBG(chg, "TA-CUR == TA-MAX-CUR\n");
								if ((iin + 20000) < chg->iin_cc) {
								/* sometimes TA-CV is not high enough to enter the CC-mode.
								* it will increase TA-CV(40mV) because TA-CUR is already MAX-CUR
								*/
									chg->ta_vol = chg->ta_vol + HL7132_TA_VOL_STEP_ADJ_CC;
									if (chg->ta_vol > chg->ta_max_vol)
										chg->ta_vol = chg->ta_max_vol;
									LOG_DBG(chg, "IIN is not high enough, Now inc = TA-VOL[%dmV]\n",
											chg->ta_vol);
									chg->prev_inc = INC_TA_VOL;
									//send pd msg
									mutex_lock(&chg->lock);
									chg->timer_id = TIMER_PDMSG_SEND;
									chg->timer_period = 0;
									mutex_unlock(&chg->lock);
									goto EXIT;
								} else
									goto ENTER_CC;
							} else {
								chg->ta_cur = chg->ta_cur + PD_MSG_TA_CUR_STEP;
								if (chg->ta_cur > chg->ta_max_cur)
									chg->ta_cur = chg->ta_max_cur;

								LOG_DBG(chg, "INC = TA_CUR!!\n");
								chg->prev_inc = INC_TA_CUR;
								//send pd msg
								mutex_lock(&chg->lock);
								chg->timer_id = TIMER_PDMSG_SEND;
								chg->timer_period = 0;
								mutex_unlock(&chg->lock);
								goto EXIT;
							}
						}
					}
				}
			}
		}

ENTER_CC:
		//exit adjust mode
		//Calculate TA_V_ofs = TA_CV -(2 x VBAT_ADC) + 0.1V
#ifdef HL7132_STEP_CHARGING
			if (chg->ta_v_ofs == 0)
				chg->ta_v_ofs = (chg->ta_vol - (2 * vbat)) + HL7132_TA_V_OFFSET;
#else
			chg->ta_v_ofs = (chg->ta_vol - (2 * vbat)) + HL7132_TA_V_OFFSET;
#endif
			val = (vbat * 2) + chg->ta_v_ofs;
			val = val/PD_MSG_TA_VOL_STEP;
			chg->ta_target_vol = val*PD_MSG_TA_VOL_STEP;
			if (chg->ta_target_vol > chg->ta_max_vol)
				chg->ta_target_vol = chg->ta_max_vol;
			chg->ta_vol = chg->ta_target_vol;
			LOG_DBG(chg, "Adjust ended!!,ta_vol [%d], TA-V-OFS:[%d]", chg->ta_vol, chg->ta_v_ofs);
			chg->prev_inc = INC_NONE;
			hl7132_set_charging_state(chg, DC_STATE_START_CC);

			mutex_lock(&chg->lock);
			chg->timer_id = TIMER_ENTER_CCMODE;
			chg->timer_period = 0;
			mutex_unlock(&chg->lock);

EXIT:
			chg->prev_iin = iin;
			schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			break;

		case VBAT_REG_LOOP:
		case VBAT_IBAT_LOOP:
		case VBAT_TDIE_LOOP:
			chg->ta_target_vol = chg->ta_vol;
			LOG_DBG(chg, "Ad end!!(VBAT-reg) ta_target_vol [%d]", chg->ta_target_vol);
			chg->prev_inc = INC_NONE;
			chg->prev_dec = false;

			mutex_lock(&chg->lock);
			chg->timer_id = TIMER_CHECK_CVMODE;
			chg->timer_period = 0;
			mutex_unlock(&chg->lock);
			chg->prev_iin = 0;
			schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			break;

	case IIN_REG_LOOP:
	case IBAT_REG_LOOP:
	case IIN_TDIE_LOOP:
	case IIN_IBAT_LOOP:
	case IIN_VBAT_TDIE_IBAT_LOOP:
	case IBAT_TDIE_LOOP:
	case IIN_IBAT_TDIE_LOOP:
	case T_DIE_REG_LOOP:
	case VBAT_IIN_LOOP:
	case VBAT_IIN_IBAT_LOOP:
		//(REG_STS == 0010b or 0100b) or IIN_ADC > IIN_TG + 20mA
		chg->prev_dec = true;
		chg->ta_cur = chg->ta_cur - PD_MSG_TA_CUR_STEP;
		chg->prev_iin = iin;

		LOG_DBG(chg, "IIN-REG: Ad ended!! ta-cur[%d], ta-vol[%d", chg->ta_cur, chg->ta_vol);
		//send pd msg
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_PDMSG_SEND;
		chg->timer_period = 0;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
		break;

	default:
		LOG_DBG(chg, "No Cases for Adjust![%d]\n", ccmode);
		break;
	}
error:
	LOG_DBG(chg, "End, ret=%d\n", ret);
	return ret;
}


static int hl7132_enter_ccmode(struct hl7132_charger *chg)
{
	int ret = 0;
	unsigned int r_val = 0;
	hl7132_set_charging_state(chg, DC_STATE_START_CC);
	//LOG_DBG(chg,  "Start!, TA-Vol:[%d], TA-CUR[%d]\n", chg->ta_vol, chg->ta_cur);
	chg->ta_vol = chg->ta_vol + HL7132_TA_VOL_STEP_PRE_CC;
	if (chg->ta_vol >= chg->ta_target_vol) {
		chg->ta_vol = chg->ta_target_vol;
		hl7132_set_charging_state(chg, DC_STATE_CC_MODE);
	}

	/* W/A low current issue, sometimes vbat-reg is triggered, disable vbat-reg only in adjust-cc */
	ret = hl7132_update_reg(chg, REG_CTRL11_1, BIT_VBAT_REG_DIS, (0 << 7));
	if (ret < 0) {
		LOG_DBG(chg, "failed to update reg[0x%x], ret[%d]", REG_CTRL11_1, ret);
		return ret;
	}

	ret = hl7132_read_reg(chg, REG_CTRL11_1, &r_val);
	LOG_DBG(chg, "reg[0x11] r_val[0x%x]\n", r_val);
	/*end W/A */

	//send pd msg
	mutex_lock(&chg->lock);
	chg->timer_id = TIMER_PDMSG_SEND;
	chg->timer_period = 0;
	mutex_unlock(&chg->lock);
	schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
	return ret;
}

static int hl7132_ccmode_regulation_process(struct hl7132_charger *chg)
{
	int ret = 0;
	int iin, vbat, ccmode, dev_state, delta;
	int ta_v_ofs, new_ta_vol, new_ta_cur;

	//LOG_DBG(chg, "STart!!\n");

	hl7132_set_charging_state(chg, DC_STATE_CC_MODE);

	ret = hl7132_check_sts_reg(chg);
	if (ret < 0) {
		if (chg->fault_sts_cnt == 1) {
			/* this code is only one-time-check */
			hl7132_bad_ta_detection_trigger(chg);
			ret = 0;
		}
		goto Err;
	}
	/* Check new vbat-reg/new iin request first */
	if (chg->req_pt_mode) {
		mutex_lock(&chg->lock);
		chg->req_pt_mode = false;
		mutex_unlock(&chg->lock);
		ret = hl7132_set_pass_through_mode(chg);
	} else if (chg->req_new_vbat_reg) {
		mutex_lock(&chg->lock);
		chg->req_new_vbat_reg = false;
		mutex_unlock(&chg->lock);
		ret = hl7132_set_new_vbat_reg(chg);
	} else if (chg->req_new_iin) {
		mutex_lock(&chg->lock);
		chg->req_new_iin = false;
		mutex_unlock(&chg->lock);
		ret = hl7132_set_new_iin(chg);
	} else {
		ccmode = chg->reg_state;
		dev_state = chg->chg_state;

		ret = hl7132_read_adc(chg);
		if (ret < 0)
			goto Err;

		iin = chg->adc_iin;
		vbat = chg->adc_vbat;
		LOG_DBG(chg, "IIN_TARGET: [%d],IIN: [%d], VBAT: [%d]\n", chg->iin_cc, iin, vbat);

#ifdef CONFIG_FG_READING_FOR_CVMODE
		ret = hl7132_get_vbat_from_battery(chg);
		if (ret < 0) {
			LOG_DBG(chg, "failed to get vbat_fg from FG\n");
			goto Err;
		}
#endif

		switch(ccmode) {
		case INACTIVE_LOOP:
#ifdef CONFIG_FG_READING_FOR_CVMODE
			if (chg->vbat_fg >= chg->pdata->vbat_reg) {
				LOG_DBG(chg, "vbat-fg > vbat_reg, move to CV\n");
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_CHECK_CVMODE;
				chg->timer_period = 0;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
				break;
			}
#endif
			new_ta_vol = chg->ta_vol;
			new_ta_cur = chg->ta_cur;

			/* Need to clear the flag for bad TA detection */
			if (chg->ab_try_cnt != 0) {
				LOG_DBG(chg, "clear ab_try_cnt for normal mode!ab_cnt[%d]\n", chg->ab_try_cnt);
				chg->ab_try_cnt = 0;
			}

			if ((chg->ab_ta_connected) && (iin > (chg->iin_cc+50000))) {
				/* Decrease TA VOL, because TA CUR can't be controlled */
				new_ta_vol = chg->ta_vol - PD_MSG_TA_VOL_STEP;
				LOG_DBG(chg, "[AB_TA]:IIN[%d], TA_VOL[%d], 20mV --\n", iin, chg->ta_vol);
			} else {
				if (chg->ab_ta_connected) {
					delta = chg->iin_cc - iin;
					if ((delta >= 200000) && (chg->iin_cc > chg->ta_cur))
						new_ta_cur = chg->iin_cc;

					if ((delta > 50000) && (delta < 100000)) {
						new_ta_vol = chg->ta_vol + PD_MSG_TA_VOL_STEP;
						LOG_DBG(chg, "[AB_TA]:IIN[%d], new_ta_vol[%d], delta[%d], 20mV ++\n",
							iin, new_ta_vol, delta);
					} else if (delta > 100000) {
						new_ta_vol = chg->ta_vol + (PD_MSG_TA_VOL_STEP*2);
						LOG_DBG(chg, "[AB_TA]:IIN[%d], new_ta_vol[%d], delta[%d], 40mV ++\n",
							iin, new_ta_vol, delta);
					} else
						new_ta_vol = chg->ta_vol;
				} else {
					if ((iin > chg->iin_cc) && ((iin - chg->iin_cc) > (HL7132_IIN_CFG_OFFSET_CC * 2))) {
						new_ta_cur = chg->ta_cur - PD_MSG_TA_CUR_STEP;
						//ta_v_ofs = chg->ta_v_ofs;
						//new_ta_vol = (2*vbat) + ta_v_ofs;
						LOG_DBG(chg, "IIN is too high, decrease 50mA, delta[%d], TA-VOL[%dmV], TA-CUR[%dmA]\n",
							(iin-chg->iin_cc), new_ta_vol/1000, chg->ta_cur/1000);
					} else {
						ta_v_ofs = chg->ta_v_ofs;
						/* Workaround code for low CC-Current issue, to keep high current, increase TA-CUR */
						if ((iin < chg->iin_cc) && (chg->ta_cur < chg->iin_cc)) {
							new_ta_cur = chg->ta_cur + PD_MSG_TA_CUR_STEP;
							LOG_DBG(chg, "Increase 50mA\n");
						} else if ((chg->iin_cc + 20000) >  iin) {
							//Calculate TA_V_ofs = TA_CV - 2xVBAT_ADC+100mV
							//Set TA_CV to 2xVBAT_ADC + TA_V_ofs
							LOG_DBG(chg, "iin-adc is lower than (iin-cc + 20mA), add vin offset!\n");
							new_ta_vol = (2*vbat) + ta_v_ofs;
						}

						LOG_DBG(chg, "TA-VOL:[%d],TA-CUR:[%d], ta_v_offs:[%d]\n",
							chg->ta_vol, chg->ta_cur, ta_v_ofs);
					}
				}
			}
			/* if TA-VOL is higher than Max-Vol, CC current can go lower than the target */
			if (new_ta_vol > ((chg->pdo_max_voltage * 1000) - 200000)) {
				LOG_DBG(chg, "new ta_vol[%dmV] > pdo_max_vol[%dmV], new ta_vol can't be higher!!!\n"
					, new_ta_vol, chg->pdo_max_voltage);
				/*some USB-PD IC will be shutting off, if TA-VOL is higher than pdo_max_vol*/
				new_ta_vol = (chg->pdo_max_voltage * 1000) - 200000;
			}

			if (chg->ab_ta_connected) {
				//send pd msg
				if (new_ta_vol != chg->ta_vol) {
					chg->ta_vol = new_ta_vol;
					chg->ta_cur = new_ta_cur;
					mutex_lock(&chg->lock);
					chg->timer_id = TIMER_PDMSG_SEND;
					chg->timer_period = 0;
					mutex_unlock(&chg->lock);
				} else {
					mutex_lock(&chg->lock);
					chg->timer_id = TIMER_CHECK_CCMODE;
					chg->timer_period = HL7132_CCMODE_CHECK_T;
					mutex_unlock(&chg->lock);
				}
			} else {
				//send pd msg
				if ((new_ta_vol >= (chg->ta_vol + PD_MSG_TA_VOL_STEP)) || (new_ta_cur != chg->ta_cur)) {
					LOG_DBG(chg, "New-Vol[%d], New-Cur[%d], TA-Vol[%d], TA-Cur[%d]\n",
						new_ta_vol/1000, new_ta_cur/1000, chg->ta_vol/1000, chg->ta_cur/1000);
					chg->ta_vol = new_ta_vol;
					chg->ta_cur = new_ta_cur;
					mutex_lock(&chg->lock);
					chg->timer_id = TIMER_PDMSG_SEND;
					chg->timer_period = 0;
					mutex_unlock(&chg->lock);
				} else {
					LOG_DBG(chg, "Wait 10s, TA-Vol[%d]/TA-Cur[%d]",
						chg->ta_vol/1000, chg->ta_cur/1000);
					mutex_lock(&chg->lock);
					chg->timer_id = TIMER_CHECK_CCMODE;
					chg->timer_period = HL7132_CCMODE_CHECK_T;
					mutex_unlock(&chg->lock);
				}
			}

			schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			break;

		case VBAT_REG_LOOP:
		case VBAT_IBAT_LOOP:
		case VBAT_TDIE_LOOP:
#ifdef CONFIG_FG_READING_FOR_CVMODE
			if (chg->vbat_fg >= chg->pdata->vbat_reg) {
				LOG_DBG(chg, "[vbat_reg_loop]vbat-fg > vbat_reg, move to CV\n");
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_CHECK_CVMODE;
				chg->timer_period = 0;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			} else {
				LOG_DBG(chg, "Even Vbat_reg loop, it will stay in CC\n");
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_CHECK_CCMODE;
				chg->timer_period = HL7132_CCMODE_CHECK_T;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			}
			break;
#else
			/* early Vbat-REG happens because of the TA's noise. checking target and Vbat-ADC */
			if ((vbat < chg->pdata->vbat_reg) && (vbat < (chg->pdata->vbat_reg - 20000))) {
				LOG_DBG(chg, "early Vbat_reg loop, it will stay in CC\n");
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_CHECK_CCMODE;
				chg->timer_period = HL7132_CCMODE_CHECK_T;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			} else {
				LOG_DBG(chg, "VBAT-REG, move to CV\n");
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_CHECK_CVMODE;
				chg->timer_period = 0;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			}
			break;
#endif

		case IIN_REG_LOOP:
		case IBAT_REG_LOOP:
		case IIN_TDIE_LOOP:
		case IIN_IBAT_LOOP:
		case IIN_VBAT_TDIE_IBAT_LOOP:
		case IBAT_TDIE_LOOP:
		case IIN_IBAT_TDIE_LOOP:
		case T_DIE_REG_LOOP:
		case VBAT_IIN_LOOP:
		case VBAT_IIN_IBAT_LOOP:
			/* Request from samsung for abnormal TA connection */
			if (!chg->ab_ta_connected) {
				if (chg->ab_try_cnt > 3) {
					/* abnormal TA connected! */
					chg->ab_ta_connected = true;
					chg->ab_try_cnt = 0;
					/* Decrease TA VOL, because TA CUR can't be controlled */
					chg->ta_vol = chg->ta_vol - PD_MSG_TA_VOL_STEP;
					chg->ta_cur = chg->ta_cur - PD_MSG_TA_CUR_STEP;
					//chg->ta_cur = chg->iin_cc;
					LOG_DBG(chg, "[AB_TA][IIN-LOOP]:IIN[%d], TA_VOL[%d],TA_CUR[%d] 20mV --\n",
						iin, chg->ta_vol, chg->ta_cur);
				} else {
					if (abs(iin - chg->ab_prev_cur) < 20000)
						chg->ab_try_cnt++;
					else
						chg->ab_try_cnt = 0;

					chg->ta_cur = chg->ta_cur - PD_MSG_TA_CUR_STEP;
					LOG_DBG(chg, "[IIN-LOOP]:iin=%d, next_ta_cur=%d, abCNT[%d]\n",
							iin, chg->ta_cur, chg->ab_try_cnt);
				}
			} else {
				/* Decrease TA CUR, because IIN is still higher than target */
				if (((chg->ab_prev_cur - iin) < 20000) && (iin > chg->iin_cc))
					chg->ta_cur = chg->ta_cur - PD_MSG_TA_CUR_STEP;

				/* Decrease TA VOL, because TA CUR can't be controlled */
				chg->ta_vol = chg->ta_vol - PD_MSG_TA_VOL_STEP;
				LOG_DBG(chg, "[AB_TA][IIN-LOOP]:IIN[%d], TA_VOL[%d], 20mV --\n", iin, chg->ta_vol);
			}

			/* store iin to compare the change */
			chg->ab_prev_cur = iin;

			//send pd msg
			mutex_lock(&chg->lock);
			chg->timer_id = TIMER_PDMSG_SEND;
			chg->timer_period = 0;
			mutex_unlock(&chg->lock);
			schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			break;

		default:
			/* it depends on customer's scenario */
			break;
		}
	}
Err:
	//pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

static int hl7132_cvmode_process(struct hl7132_charger *chg)
{
	int ret = 0;
	int iin, vbat, cvmode, dev_state;

	//LOG_DBG(chg, "STart!!\n");

	hl7132_set_charging_state(chg, DC_STATE_CV_MODE);

	ret = hl7132_check_sts_reg(chg);
	if (ret < 0)
		goto Err;
	cvmode = chg->reg_state;
	dev_state = chg->chg_state;

	/* Check new vbat-reg/new iin request first */
	if (chg->req_pt_mode) {
		mutex_lock(&chg->lock);
		chg->req_pt_mode = false;
		mutex_unlock(&chg->lock);
		ret = hl7132_set_pass_through_mode(chg);
	} else if (chg->req_new_vbat_reg) {
		mutex_lock(&chg->lock);
		chg->req_new_vbat_reg = false;
		mutex_unlock(&chg->lock);
		ret = hl7132_set_new_vbat_reg(chg);
	} else if (chg->req_new_iin) {
		mutex_lock(&chg->lock);
		chg->req_new_iin = false;
		mutex_unlock(&chg->lock);
		ret = hl7132_set_new_iin(chg);
	} else {
		ret = hl7132_read_adc(chg);
		if (ret < 0)
			goto Err;
		iin = chg->adc_iin;//hl7132_read_adc(chg, ADCCH_IIN);
		vbat = chg->adc_vbat;//hl7132_read_adc(chg, ADCCH_VBAT);

#ifdef CONFIG_FG_READING_FOR_CVMODE
		ret = hl7132_get_vbat_from_battery(chg);
		if (ret < 0) {
			LOG_DBG(chg, "failed to get vbat_fg from FG\n");
			goto Err;
		}
#endif
		LOG_DBG(chg, "IIN_TARGET[%d],TA_TARGET_VOL[%d], IIN[%d], VBAT[%d], TA-VOL[%d], TA-CUR[%d]\n",
				chg->iin_cc, chg->ta_target_vol, iin, vbat, chg->ta_vol, chg->ta_cur);

		switch(cvmode) {
		case INACTIVE_LOOP:
			if (iin < chg->pdata->iin_topoff) {
				LOG_DBG(chg, "DC charging done!");
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				hl7132_set_done(chg, true);
#endif

				if (chg->charging_state != DC_STATE_NOT_CHARGING) {
#ifdef HL7132_STEP_CHARGING
					if (chg->pdata->vbat_reg == HL7132_STEP3_VBAT_REG)
						hl7132_stop_charging(chg);
					else
						schedule_delayed_work(&chg->step_work, 100);
#else
					hl7132_stop_charging(chg);
#endif
				} else {
					LOG_DBG(chg, "Charging is already disabled!\n");
				}
			} else {
#ifdef CONFIG_FG_READING_FOR_CVMODE
				if (chg->vbat_fg > chg->pdata->vbat_reg) {
					chg->ta_vol = chg->ta_vol - HL7132_TA_VOL_STEP_PRE_CV;
					LOG_DBG(chg, "vbat-fg > vbat_reg, TA-VOL:[%d], TA-CUR:[%d]\n",
						chg->ta_vol/1000, chg->ta_cur/1000);
					//send pd msg
					mutex_lock(&chg->lock);
					chg->timer_id = TIMER_PDMSG_SEND;
					chg->timer_period = HL7132_CVMODE_FG_READING_CHECK_T;
					mutex_unlock(&chg->lock);
					schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
				} else {
					LOG_DBG(chg, "No ACTIVE, Just Wait for 2s\n");
					mutex_lock(&chg->lock);
					chg->timer_id = TIMER_CHECK_CVMODE;
					chg->timer_period = HL7132_CVMODE_FG_READING_CHECK_T;
					mutex_unlock(&chg->lock);
					schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
				}
#else
				LOG_DBG(chg, "No ACTIVE, Just Wait for 10s\n");
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_CHECK_CVMODE;
				chg->timer_period = HL7132_CVMODE_CHECK_T;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
#endif
			}
			break;

		case VBAT_REG_LOOP:
		case VBAT_IBAT_LOOP:
		case VBAT_TDIE_LOOP:
#ifdef CONFIG_FG_READING_FOR_CVMODE
			if (chg->vbat_fg > chg->pdata->vbat_reg) {
				chg->ta_vol = chg->ta_vol - HL7132_TA_VOL_STEP_PRE_CV;
				LOG_DBG(chg, " [vbat-loop]vbat-fg > vbat_reg, TA-VOL:[%d], TA-CUR:[%d]\n",
					chg->ta_vol/1000, chg->ta_cur/1000);
				//send pd msg
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_PDMSG_SEND;
				chg->timer_period = HL7132_CVMODE_FG_READING_CHECK_T;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			} else {
				LOG_DBG(chg, "[fg-read]No ACTIVE, Just Wait for 2s\n");
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_CHECK_CVMODE;
				chg->timer_period = HL7132_CVMODE_FG_READING_CHECK_T;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			}
#else
			chg->ta_vol = chg->ta_vol - HL7132_TA_VOL_STEP_PRE_CV;
			LOG_DBG(chg, "VBAT-REG, Decrease 20mV, TA-VOL:[%d], TA-CUR:[%d]\n", chg->ta_vol, chg->ta_cur);
			//send pd msg
			mutex_lock(&chg->lock);
			chg->timer_id = TIMER_PDMSG_SEND;
			chg->timer_period = 0;
			mutex_unlock(&chg->lock);
			schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
#endif
			break;

		case IIN_REG_LOOP:
		case IBAT_REG_LOOP:
		case IIN_TDIE_LOOP:
		case IIN_IBAT_LOOP:
		case IIN_VBAT_TDIE_IBAT_LOOP:
		case IBAT_TDIE_LOOP:
		case IIN_IBAT_TDIE_LOOP:
		case T_DIE_REG_LOOP:
		case VBAT_IIN_LOOP:
		case VBAT_IIN_IBAT_LOOP:
			chg->ta_cur = chg->ta_cur - PD_MSG_TA_CUR_STEP;
			LOG_DBG(chg, "IIN-REG/IBAT-REG, Decrease 50mA, TA-VOL[%d], TA-CUR[%d]\n", chg->ta_vol, chg->ta_cur);
			//send pd msg
			mutex_lock(&chg->lock);
			chg->timer_id = TIMER_PDMSG_SEND;
			chg->timer_period = 0;
			mutex_unlock(&chg->lock);
			schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
			break;

		default:
			LOG_DBG(chg, "No cases!\n");
			break;
		}
	}

Err:
	//pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

static void hl7132_timer_work(struct work_struct *work)
{
	struct hl7132_charger *chg = container_of(work, struct hl7132_charger, timer_work.work);
	int ret = 0;
	unsigned int val;

	LOG_DBG(chg, "start!, timer_id =[%d], charging_state=[%d]\n", chg->timer_id, chg->charging_state);

	switch (chg->timer_id) {
	case TIMER_VBATMIN_CHECK:
		ret = hl7132_check_starting_vbat_level(chg);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PRESET_DC:
		ret = hl7132_start_dc_charging(chg);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PRESET_CONFIG:
		ret = hl7132_preset_config(chg);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_ACTIVE:
		ret = hl7132_check_active_state(chg);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ADJUST_CCMODE:
		ret = hl7132_adjust_ccmode(chg);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ENTER_CCMODE:
		ret = hl7132_enter_ccmode(chg);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_CCMODE:
		ret = hl7132_ccmode_regulation_process(chg);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_CVMODE:
		ret = hl7132_cvmode_process(chg);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PDMSG_SEND:
		/* Adjust TA current and voltage step */
		val = chg->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
		chg->ta_vol = val*PD_MSG_TA_VOL_STEP;
		val = chg->ta_cur/PD_MSG_TA_CUR_STEP;	/* PPS current resolution is 50mA */
		chg->ta_cur = val*PD_MSG_TA_CUR_STEP;
		if (chg->ta_cur < HL7132_TA_MIN_CUR)	/* PPS minimum current is 1000mA */
			chg->ta_cur = HL7132_TA_MIN_CUR;
		/* Send PD Message */
		ret = hl7132_send_pd_message(chg, PD_MSG_REQUEST_APDO);
		if (ret < 0)
			goto error;

		/* Go to the next state */
		mutex_lock(&chg->lock);
		switch (chg->charging_state) {
		case DC_STATE_PRESET_DC:
			chg->timer_id = TIMER_PRESET_CONFIG;
			break;
		case DC_STATE_ADJUST_CC:
			chg->timer_id = TIMER_ADJUST_CCMODE;
			break;
		case DC_STATE_START_CC:
			chg->timer_id = TIMER_ENTER_CCMODE;
			break;
		case DC_STATE_CC_MODE:
			chg->timer_id = TIMER_CHECK_CCMODE;
			break;
		case DC_STATE_CV_MODE:
			chg->timer_id = TIMER_CHECK_CVMODE;
			break;
		case DC_STATE_ADJUST_TAVOL:
			chg->timer_id = TIMER_ADJUST_TAVOL;
			break;
		case DC_STATE_ADJUST_TACUR:
			chg->timer_id = TIMER_ADJUST_TACUR;
			break;
#ifdef CONFIG_HALO_PASS_THROUGH
		case DC_STATE_PRESET_PT:
			chg->timer_id = TIMER_PRESET_CONFIG_PT;
			break;
		case DC_STATE_PT_MODE:
			chg->timer_id = TIMER_CHECK_PTMODE;
			break;
#endif
		default:
			ret = -EINVAL;
			break;
		}

		chg->timer_period = HL7132_PDMSG_WAIT_T;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
		break;

	case TIMER_ADJUST_TAVOL:
		ret = hl7132_adjust_ta_voltage(chg);
		if (ret < 0)
			goto error;
		break;
	case TIMER_ADJUST_TACUR:
		ret = hl7132_adjust_ta_current(chg);
		if (ret < 0)
			goto error;
		break;
#ifdef CONFIG_HALO_PASS_THROUGH
	case TIMER_PRESET_PT:
		ret = hl7132_preset_config_pt(chg);
		if (ret < 0)
			goto error;
		break;
	case TIMER_PRESET_CONFIG_PT:
		ret = hl7132_preset_config_pt(chg);
		if (ret < 0)
			goto error;
		break;
	case TIMER_CHECK_PTMODE:
		ret = hl7132_pass_through_mode_process(chg);
		if (ret < 0)
			goto error;
#endif
	default:
		break;
	}

	if (chg->charging_state == DC_STATE_NOT_CHARGING) {
		LOG_DBG(chg, "Cancel timer_work, because NOT charging!!\n");
		cancel_delayed_work(&chg->timer_work);
		cancel_delayed_work(&chg->pps_work);
	}

	LOG_DBG(chg, "END!!\n");
	return;

error:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	chg->health_status = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
#endif
	hl7132_stop_charging(chg);
	return;
}

static void hl7132_pps_request_work(struct work_struct *work)
{
	struct hl7132_charger *chg = container_of(work, struct hl7132_charger,
						 pps_work.work);
	int ret = 0;

	/* Send PD message */
	ret = hl7132_send_pd_message(chg, PD_MSG_REQUEST_APDO);
	LOG_DBG(chg, "End, ret = %d\n", ret);

	if (ret < 0) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		chg->health_status = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
#endif
		hl7132_stop_charging(chg);
	}
}

#ifdef HL7132_STEP_CHARGING
static int hl7132_prepare_nextstep(struct hl7132_charger *chg)
{
	int ret = 0;
	int vbat;
	unsigned int val;

	hl7132_set_charging_state(chg, DC_STATE_PRESET_DC);

	ret = hl7132_read_adc(chg);
	if (ret < 0)
		goto Err;

	vbat = chg->adc_vbat;
	LOG_DBG(chg, "Vbat is [%d]\n", vbat);

	if ((vbat >= HL7132_STEP1_VBAT_REG) && (vbat < HL7132_STEP2_VBAT_REG)) {
		/* Step2 Charging! */
		LOG_DBG(chg, "Step2 Charging Start!\n");
		chg->current_step = STEP_TWO;
		chg->pdata->vbat_reg = HL7132_STEP2_VBAT_REG;
		chg->pdata->iin_cfg = HL7132_STEP2_TARGET_IIN;
		chg->pdata->iin_topoff = HL7132_STEP2_TOPOFF;
	} else {
		/* Step3 Charging! */
		LOG_DBG(chg, "Step3 Charging Start!\n");
		chg->current_step = STEP_THREE;
		chg->pdata->vbat_reg = HL7132_STEP3_VBAT_REG;
		chg->pdata->iin_cfg = HL7132_STEP3_TARGET_IIN;
		chg->pdata->iin_topoff = HL7132_STEP3_TOPOFF;
	}

	chg->iin_cc = MIN(chg->pdata->iin_cfg, chg->ta_max_cur);
	chg->pdata->iin_cfg = chg->iin_cc;

	val = chg->iin_cc / PD_MSG_TA_CUR_STEP;
	chg->iin_cc = val * PD_MSG_TA_CUR_STEP;
	chg->ta_cur = chg->iin_cc;
	chg->iin_cc = chg->pdata->iin_cfg;

	LOG_DBG(chg, "ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, TA-VOL[%d], TA-CUR[%d]\n",
		chg->ta_max_vol, chg->ta_max_cur,
		chg->ta_max_pwr, chg->iin_cc, chg->ta_vol, chg->ta_cur);

	mutex_lock(&chg->lock);
	chg->timer_id = TIMER_PDMSG_SEND;
	chg->timer_period = 0;
	mutex_unlock(&chg->lock);
	schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));

Err:
	LOG_DBG(chg, "End, ret =%d\n", ret);
	return ret;
}

static void hl7132_step_charging_work(struct work_struct *work)
{
	struct hl7132_charger *chg = container_of(work, struct hl7132_charger, step_work.work);
	int ret = 0;

	if (chg->charging_state == DC_STATE_NOT_CHARGING) {
		/*Start from VBATMIN CHECK */
		mutex_lock(&chg->lock);
		chg->timer_id = TIMER_VBATMIN_CHECK;
		chg->timer_period = HL7132_VBATMIN_CHECK_T;
		mutex_unlock(&chg->lock);
		schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
	} else if (chg->charging_state == DC_STATE_CV_MODE) {
		/* Go back to PRESET DC */
		ret = hl7132_prepare_nextstep(chg);
		//ret = hl7132_setiin_for_stepcharging(chg);
		if (ret < 0)
			goto ERR;
	} else {
		LOG_DBG(chg, "invalid charging STATE, charging_state[%d]\n", chg->charging_state);
	}

ERR:
	LOG_DBG(chg, "ret = %d\n", ret);
}
#endif

static int hl7132_psy_set_property(struct power_supply *psy, enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct hl7132_charger *chg = power_supply_get_drvdata(psy);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
#endif
	int ret = 0;
	unsigned int temp = 0;

	//LOG_DBG(chg, "Start!, prop==[%d], val==[%d]\n", psp, val->intval);

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		//LOG_DBG(chg, "ONLINE:\r\n");
		if (chg->online) {
			/* charger is enabled, need to stop charging!! */
			if (!val->intval) {
				chg->online = false;

				chg->pdata->iin_cfg = HL7132_IIN_CFG_DFT;
				chg->new_iin = chg->pdata->iin_cfg;
				chg->pdata->vbat_reg = chg->pdata->vbat_reg_max;
				chg->new_vbat_reg = chg->pdata->vbat_reg;
				LOG_DBG(chg, "iin_cfg : [%d], vbat_reg : [%d]\n",
					chg->pdata->iin_cfg, chg->pdata->vbat_reg);
				
				ret = hl7132_stop_charging(chg);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				chg->health_status = POWER_SUPPLY_HEALTH_GOOD;
#endif
				if (ret < 0)
					goto Err;
			} else {
				//pr_info("%s: charger is already enabled! \n", __func__);
			}
		} else {
			/* charger is disabled, need to start charging!! */
			if (val->intval) {
				chg->online = true;
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#ifndef HL7132_STEP_CHARGING
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_VBATMIN_CHECK;
				chg->timer_period = HL7132_VBATMIN_CHECK_T;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
#else
				schedule_delayed_work(&chg->step_work, 0);
#endif
#endif
				ret = 0;
			} else {
				if (delayed_work_pending(&chg->timer_work)) {
					cancel_delayed_work(&chg->timer_work);
					cancel_delayed_work(&chg->pps_work);
					LOG_DBG(chg, "cancel work queue!!");
				}

				LOG_DBG(chg, "charger is already disabled!!\n");
			}
		}
		break;

	case POWER_SUPPLY_PROP_STATUS:
		LOG_DBG(chg, "STATUS:\r\n");

		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		/* add this code for halo switching charger */
		if (val->intval) {
			LOG_DBG(chg, "set from switching charger!\r\n");
			chg->online = true;
#ifndef HL7132_STEP_CHARGING
			mutex_lock(&chg->lock);
			chg->timer_id = TIMER_VBATMIN_CHECK;
			chg->timer_period = HL7132_VBATMIN_CHECK_T;
			mutex_unlock(&chg->lock);
			schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
#else
			schedule_delayed_work(&chg->step_work, 0);
#endif
			ret = 0;
		}
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		chg->float_voltage = val->intval;
		temp = chg->float_voltage * HL7132_SEC_DENOM_U_M;
#endif
		LOG_DBG(chg, "request new vbat reg[%d] / old[%d]\n", temp, chg->new_vbat_reg);
		if (temp != chg->new_vbat_reg) {
			chg->new_vbat_reg = temp;
			if ((chg->charging_state == DC_STATE_NOT_CHARGING) ||
				(chg->charging_state == DC_STATE_CHECK_VBAT)) {
				/* Set new vbat-reg to pdata->vbat-reg, before PRESET_DC state */
				chg->pdata->vbat_reg = chg->new_vbat_reg;
			} else {
				if (chg->req_new_vbat_reg == true) {
					LOG_DBG(chg, "previous vbat-reg is not finished yet\n");
					ret = -EBUSY;
				} else {
					/* set new vbat reg voltage flag*/
					mutex_lock(&chg->lock);
					chg->req_new_vbat_reg = true;
					mutex_unlock(&chg->lock);

					/* Check the charging state */
					if ((chg->charging_state == DC_STATE_CC_MODE) ||
							(chg->charging_state == DC_STATE_CV_MODE) ||
							/*(chg->charging_state == DC_STATE_PT_MODE) ||*/
							(chg->charging_state == DC_STATE_CHARGING_DONE)) {
						/* cancel delayed_work and  restart queue work again */
						cancel_delayed_work(&chg->timer_work);
						mutex_lock(&chg->lock);
						chg->timer_period = 0;
						mutex_unlock(&chg->lock);
						schedule_delayed_work(&chg->timer_work,
								msecs_to_jiffies(chg->timer_period));
					} else {
						/* Wait for next valid state - cc, cv, or bypass state */
						LOG_DBG(chg, "Unsupported charging state[%d]\n", chg->charging_state);
					}
				}
			}
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		chg->input_current = val->intval;
		temp = chg->input_current * HL7132_SEC_DENOM_U_M;
#endif
		LOG_DBG(chg, "INPUT_CURRENT_LIMIT: request new iin[%d]  / old[%d]\n", temp, chg->new_iin);
		if (temp != chg->new_iin) {
			chg->new_iin = temp;
			if ((chg->charging_state == DC_STATE_NOT_CHARGING) ||
				(chg->charging_state == DC_STATE_CHECK_VBAT)) {
				/* Set new iin to pdata->iin_cfg, before PRESET_DC state */
				chg->pdata->iin_cfg = chg->new_iin;
				/* set switching frequency */
				hl7132_set_fsw(chg);
			} else {
				if (chg->req_new_iin == true) {
					LOG_DBG(chg, "previous iin-req is not finished yet\n");
					ret = -EBUSY;
				} else {
					/* set new iin flag */
					mutex_lock(&chg->lock);
					chg->req_new_iin = true;
					mutex_unlock(&chg->lock);

					/* Check the charging state */
					if ((chg->charging_state == DC_STATE_CC_MODE) ||
						(chg->charging_state == DC_STATE_CV_MODE) ||
						/*(chg->charging_state == DC_STATE_PT_MODE) ||*/
						(chg->charging_state == DC_STATE_CHARGING_DONE)) {
						/* cancel delayed_work and  restart queue work again */
						cancel_delayed_work(&chg->timer_work);
						mutex_lock(&chg->lock);
						chg->timer_period = 0;
						mutex_unlock(&chg->lock);
						schedule_delayed_work(&chg->timer_work,
								msecs_to_jiffies(chg->timer_period));
					} else {
						/* Wait for next valid state - cc, cv, or bypass state */
						LOG_DBG(chg, "Unsupported charging state[%d]\n", chg->charging_state);
					}
				}
			}
		}
		break;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		chg->pdata->vbat_reg_max = val->intval * HL7132_SEC_DENOM_U_M;
		pr_info("%s: vbat_reg_max(%duV)\n", __func__, chg->pdata->vbat_reg_max);
		break;

	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL:
			if (val->intval) {
				chg->wdt_kick = true;
			} else {
				chg->wdt_kick = false;
				cancel_delayed_work(&chg->wdt_control_work);
			}
			pr_info("%s: wdt kick (%d)\n", __func__, chg->wdt_kick);
			break;

		case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			chg->input_current = val->intval;
			temp = chg->input_current * HL7132_SEC_DENOM_U_M;
#endif
			LOG_DBG(chg, "DIRECT_CURRENT_MAX: request new iin[%d] / old[%d]\n", temp, chg->new_iin);
			if (temp != chg->new_iin) {
				chg->new_iin = temp;
				if ((chg->charging_state == DC_STATE_NOT_CHARGING) ||
					(chg->charging_state == DC_STATE_CHECK_VBAT)) {
					/* Set new iin to pdata->iin_cfg, before PRESET_DC state */
					chg->pdata->iin_cfg = chg->new_iin;
				} else {
					if (chg->req_new_iin == true) {
						LOG_DBG(chg, "previous iin-req is not finished yet\n");
						ret = -EBUSY;
					} else {
						/* set new iin flag */
						mutex_lock(&chg->lock);
						chg->req_new_iin = true;
						mutex_unlock(&chg->lock);

						/* Check the charging state */
						if ((chg->charging_state == DC_STATE_CC_MODE) ||
							(chg->charging_state == DC_STATE_CV_MODE) ||
							/*(chg->charging_state == DC_STATE_PT_MODE) ||*/
							(chg->charging_state == DC_STATE_CHARGING_DONE)) {
							/* cancel delayed_work and	restart queue work again */
							cancel_delayed_work(&chg->timer_work);
							mutex_lock(&chg->lock);
							chg->timer_period = 0;
							mutex_unlock(&chg->lock);
							schedule_delayed_work(&chg->timer_work,
									msecs_to_jiffies(chg->timer_period));
						} else {
							/* Wait for next valid state - cc, cv, or bypass state */
							LOG_DBG(chg, "Unsupported charging state[%d]\n",
										chg->charging_state);
						}
					}
				}
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			if (val->intval == 0) {
				// Stop Direct Charging
				ret = hl7132_stop_charging(chg);
				chg->health_status = POWER_SUPPLY_HEALTH_GOOD;
				if (ret < 0)
					goto Err;
			} else {
				if (chg->charging_state != DC_STATE_NOT_CHARGING) {
					pr_info("%s: duplicate charging enabled(%d)\n", __func__, val->intval);
					goto Err;
				}
				if (!chg->online) {
					pr_info("%s: online is not attached(%d)\n", __func__, val->intval);
					goto Err;
				}
#ifndef HL7132_STEP_CHARGING
				mutex_lock(&chg->lock);
				chg->timer_id = TIMER_VBATMIN_CHECK;
				chg->timer_period = HL7132_VBATMIN_CHECK_T;
				mutex_unlock(&chg->lock);
				schedule_delayed_work(&chg->timer_work, msecs_to_jiffies(chg->timer_period));
#else
				schedule_delayed_work(&chg->step_work, 0);
#endif
				ret = 0;
			}
			break;
#ifdef CONFIG_HALO_PASS_THROUGH
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE:
			LOG_DBG(chg, "Pass Through mode[%d]\n", val->intval);
			if (val->intval != chg->pass_through_mode) {
				chg->pass_through_mode = val->intval;
				ret = hl7132_set_pass_through_mode(chg);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE_TA_VOL:
			if ((chg->charging_state == DC_STATE_PT_MODE) &&
				(chg->pdata->pass_through_mode != PTM_NONE)) {
				LOG_DBG(chg, "ptmode voltage!\n");
				hl7132_set_ptmode_ta_voltage_by_soc(chg, val->intval);
			} else {
				LOG_DBG(chg, "not in ptmode!\n");
			}
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_D2D_REVERSE_VOLTAGE:
			break;
		default:
			LOG_DBG(chg, "invalid ext-property![%d]\n", psp);
			return -EINVAL;
		}
		break;
#endif
	default:
		LOG_DBG(chg, "invalid property![%d]\n", psp);
		return -EINVAL;
	}

Err:
	//LOG_DBG(chg, "End, ret== %d\n", ret);
	return ret;
}

static int hl7132_psy_get_property(struct power_supply *psy, enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct hl7132_charger *chg = power_supply_get_drvdata(psy);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
#endif
	int ret = 0;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		LOG_DBG(chg, "ONLINE:, [%d]\n", chg->online);
		val->intval = chg->online;
		break;

	case POWER_SUPPLY_PROP_STATUS:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		val->intval = (chg->charging_state == DC_STATE_NOT_CHARGING) ?
			POWER_SUPPLY_STATUS_DISCHARGING : POWER_SUPPLY_STATUS_CHARGING;
#else
		//LOG_DBG(chg, "STATUS:\r\n");
		//val->intval = chg->online ? POWER_SUPPLY_STATUS_CHARGING : POWER_SUPPLY_STATUS_DISCHARGING;
		val->intval = chg->charging_state;
#endif
		break;

	case POWER_SUPPLY_PROP_POWER_NOW:
		if ((chg->charging_state >= DC_STATE_CHECK_ACTIVE) && (chg->charging_state <= DC_STATE_CV_MODE)) {
			ret = hl7132_read_adc(chg);
			if (ret < 0) {
				pr_err("[%s] can't read adc!\n", __func__);
				return ret;
			}
			val->intval = chg->adc_vbat;
		}
		break;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chg->health_status;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		hl7132_get_adc_channel(chg, ADCCH_VTS);
		val->intval = chg->adc_vts;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = chg->float_voltage;
		break;

	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			hl7132_test_read(chg);
			hl7132_monitor_work(chg);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			switch (val->intval) {
			case SEC_BATTERY_IIN_MA:
				hl7132_get_adc_channel(chg, ADCCH_IIN);
				val->intval = chg->adc_iin / HL7132_SEC_DENOM_U_M;
				break;
			case SEC_BATTERY_IIN_UA:
				hl7132_get_adc_channel(chg, ADCCH_IIN);
				val->intval = chg->adc_iin;
				break;
			case SEC_BATTERY_VIN_MA:
				hl7132_get_adc_channel(chg, ADCCH_VIN);
				val->intval = chg->adc_vin / HL7132_SEC_DENOM_U_M;
				break;
			case SEC_BATTERY_VIN_UA:
				hl7132_get_adc_channel(chg, ADCCH_VIN);
				val->intval = chg->adc_vin;
				break;
			default:
				val->intval = 0;
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_D2D_REVERSE_VOLTAGE:
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS:
			val->strval = charging_state_str[chg->charging_state];
			break;
		default:
			return -EINVAL;
		}
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int hl7132_regmap_init(struct hl7132_charger *chg)
{
	int ret;
	// LOG_DBG(chg, "Start!!\n");
	if (!i2c_check_functionality(chg->client->adapter, I2C_FUNC_I2C)) {
		dev_err(chg->dev, "%s: check_functionality failed.", __func__);
		return -ENODEV;
	}

	chg->regmap = devm_regmap_init_i2c(chg->client, &hl7132_regmap_config);
	if (IS_ERR(chg->regmap)) {
		ret = PTR_ERR(chg->regmap);
		dev_err(chg->dev, "regmap init failed, err == %d\n", ret);
		return PTR_ERR(chg->regmap);
	}
	i2c_set_clientdata(chg->client, chg);

	return 0;
}


static int read_reg(void *data, u64 *val)
{
	struct hl7132_charger *chg = data;
	int rc;
	unsigned int temp;
	LOG_DBG(chg, "Start!\n");
	rc = regmap_read(chg->regmap, chg->debug_address, &temp);
	if (rc) {
		pr_err("Couldn't read reg 0x%x rc = %d\n",
			chg->debug_address, rc);
		return -EAGAIN;
	}

	LOG_DBG(chg, "address = [0x%x], value = [0x%x]\n", chg->debug_address, temp);
	*val = temp;
	return 0;
}

static int write_reg(void *data, u64 val)
{
	struct hl7132_charger *chg = data;
	int rc;
	u8 temp;

	LOG_DBG(chg, "Start! val == [%x]\n", (u8)val);
	temp = (u8) val;
	rc = regmap_write(chg->regmap, chg->debug_address, temp);
	if (rc) {
		pr_err("Couldn't write 0x%02x to 0x%02x rc= %d\n",
			temp, chg->debug_address, rc);
		return -EAGAIN;
	}
	return 0;
}

static int get_vbat_reg(void *data, u64 *val)
{
	struct hl7132_charger *chg = data;
	unsigned int temp;
	LOG_DBG(chg, "Start!\n");

	temp = chg->pdata->vbat_reg;
	*val = temp;
	return 0;
}

static int set_vbat_reg(void *data, u64 val)
{
	struct hl7132_charger *chg = data;
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name(hl7132_supplied_to[0]);

	LOG_DBG(chg, "Start! new_vbat_reg == [%d]\n", (unsigned int)val);
	value.intval = val;
	power_supply_set_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &value);
	LOG_DBG(chg, "new_vbat_reg == [%d]\n", chg->new_vbat_reg);
	return 0;
}

static int get_iin_cfg(void *data, u64 *val)
{
	struct hl7132_charger *chg = data;
	unsigned int temp;
	LOG_DBG(chg, "Start!\n");

	temp = chg->pdata->iin_cfg;
	*val = temp;
	return 0;
}

static int set_iin_cfg(void *data, u64 val)
{
	struct hl7132_charger *chg = data;
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name(hl7132_supplied_to[0]);

	LOG_DBG(chg, "Start! new_iin == [%d]\n", (unsigned int)val);
	value.intval = val;
	power_supply_set_property(psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &value);
	LOG_DBG(chg, "new_iin == [%d]\n", chg->new_iin);
	return 0;
}

/* Below Debugfs us only used for the the bad-ta_connection test! */
static int set_tp_set_cfg(void *data, u64 val)
{
	struct hl7132_charger *chg = data;

	LOG_DBG(chg, "[Set tp_set:[%d]\n", (unsigned int)val);
	chg->tp_set = val;

	return 0;
}

static int get_tp_set_cfg(void *data, u64 *val)
{
	struct hl7132_charger *chg = data;
	unsigned int temp;

	LOG_DBG(chg, "Start!\n");

	temp = chg->tp_set;
	*val = temp;

	LOG_DBG(chg, "tp-set[%d], ab_try_cnt[%d]\n", chg->tp_set, chg->ab_try_cnt);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0)
/* Adapting to the upstream debugfs_create_x32() change */
static int hl7132_u32_get(void *data, u64 *val)
{
	*val = *(u32 *)data;
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(hl7132_fops_x32_ro, hl7132_u32_get, NULL, "0x%08llx\n");
#endif
DEFINE_SIMPLE_ATTRIBUTE(register_debug_ops, read_reg, write_reg, "0x%02llx\n");
DEFINE_SIMPLE_ATTRIBUTE(vbat_reg_debug_ops, get_vbat_reg, set_vbat_reg, "%02lld\n");
DEFINE_SIMPLE_ATTRIBUTE(iin_cfg_debug_ops, get_iin_cfg, set_iin_cfg, "%02lld\n");
DEFINE_SIMPLE_ATTRIBUTE(tp_set_debug_ops, get_tp_set_cfg, set_tp_set_cfg, "%02lld\n");

static int hl7132_create_debugfs_entries(struct hl7132_charger *chg)
{
	struct dentry *ent;
	int rc = 0;

	chg->debug_root = debugfs_create_dir("hl7132-debugfs", NULL);
	if (!chg->debug_root) {
		dev_err(chg->dev, "Couldn't create debug dir\n");
		rc = -ENOENT;
	} else {
		/* After version 5.5.0 debugfs_create_x32 changed to a void type function */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
		ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
				chg->debug_root,
				&(chg->debug_address));
#else
		ent = debugfs_create_file_unsafe("address", S_IFREG | S_IWUSR | S_IRUGO,
				chg->debug_root,
				&(chg->debug_address),
				&hl7132_fops_x32_ro);
#endif
		if (!ent) {
			dev_err(chg->dev, "Couldn't create address debug file\n");
			rc = -ENOENT;
		}

		ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
				chg->debug_root, chg,
				&register_debug_ops);
		if (!ent) {
			dev_err(chg->dev, "Couldn't create data debug file\n");
			rc = -ENOENT;
		}

		ent = debugfs_create_file("vbat-reg", S_IFREG | S_IWUSR | S_IRUGO,
				chg->debug_root, chg,
				&vbat_reg_debug_ops);
		if (!ent) {
			dev_err(chg->dev, "Couldn't create vbat-reg file\n");
			rc = -ENOENT;
		}
		ent = debugfs_create_file("iin-cfg", S_IFREG | S_IWUSR | S_IRUGO,
				chg->debug_root, chg,
				&iin_cfg_debug_ops);
		if (!ent) {
			dev_err(chg->dev, "Couldn't create iin-cfg file\n");
			rc = -ENOENT;
		}
		ent = debugfs_create_file("tp-set", S_IFREG | S_IWUSR | S_IRUGO,
			chg->debug_root, chg,
			&tp_set_debug_ops);
		if (!ent) {
			dev_err(chg->dev, "Couldn't create tp-set file\n");
			rc = -ENOENT;
		}
#ifdef HALO_DBG
		hl7132_log_debugfs_init(chg);
#endif
	}

	return rc;
}

static enum power_supply_property hl7132_psy_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc hl7132_psy_desc = {
	.name = "hl7132-charger",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property = hl7132_psy_get_property,
	.set_property = hl7132_psy_set_property,
	.properties = hl7132_psy_props,
	.num_properties = ARRAY_SIZE(hl7132_psy_props),
};

static int hl7132_charger_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct hl7132_platform_data *pdata;
	struct hl7132_charger *charger;
	struct power_supply_config psy_cfg = {};
	//union power_supply_propval value;

	int ret;

	pr_info("%s: HL7132 Charger Driver Loading\n", __func__);
	pr_info("[%s]:: Ver[%s]!\n", __func__, HL7132_MODULE_VERSION);

	charger = devm_kzalloc(&client->dev, sizeof(*charger), GFP_KERNEL);
	if (!charger) {
		dev_err(&client->dev, "failed to allocate chip memory\n");
		return -ENOMEM;
	}
#if defined(CONFIG_OF)
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(struct hl7132_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate pdata memory\n");
			return -ENOMEM;
		}

		ret = hl7132_parse_dt(&client->dev, pdata);
		if (ret < 0) {
			dev_err(&client->dev, "Failed to get device of node\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
	} else {
		pdata = client->dev.platform_data;
	}
#else
	pdata = dev->platform_data;
#endif

	if (!pdata)
		return -EINVAL;

	charger->dev = &client->dev;
	charger->pdata = pdata;
	charger->client = client;
	charger->dc_state = DC_STATE_NOT_CHARGING;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	charger->health_status = POWER_SUPPLY_HEALTH_GOOD;
#endif

	ret = hl7132_regmap_init(charger);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to initialize i2c,[%s]\n", HL7132_I2C_NAME);
		return ret;
	}

	mutex_init(&charger->i2c_lock);
	mutex_init(&charger->lock);
	charger->monitor_ws = wakeup_source_register(&client->dev, "hl7132-monitor");

	INIT_DELAYED_WORK(&charger->timer_work, hl7132_timer_work);
	mutex_lock(&charger->lock);
	charger->timer_id = TIMER_ID_NONE;
	charger->timer_period = 0;
	mutex_unlock(&charger->lock);

	INIT_DELAYED_WORK(&charger->pps_work, hl7132_pps_request_work);

#ifdef HL7132_STEP_CHARGING
	/* step_charging schedule work*/
	INIT_DELAYED_WORK(&charger->step_work, hl7132_step_charging_work);
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	INIT_DELAYED_WORK(&charger->wdt_control_work, hl7132_wdt_control_work);
#endif

	hl7132_device_init(charger);

	psy_cfg.supplied_to = hl7132_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(hl7132_supplied_to);
	psy_cfg.drv_data = charger;

	charger->psy_chg = power_supply_register(&client->dev, &hl7132_psy_desc, &psy_cfg);
	if (IS_ERR(charger->psy_chg)) {
		dev_err(&client->dev, "failed to register power supply\n");
		return PTR_ERR(charger->psy_chg);
	}

	if (pdata->irq_gpio >= 0) {
		ret = hl7132_irq_init(charger, client);
		if (ret < 0) {
			dev_warn(&client->dev, "failed to initialize IRQ :: %d\n", ret);
			goto FAIL_IRQ;
		}
	}

	ret = hl7132_create_debugfs_entries(charger);
	if (ret < 0)
		goto FAIL_DEBUGFS;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	sec_chg_set_dev_init(SC_DEV_DIR_CHG);
#endif
	pr_info("%s: HL7132 Charger Driver Loaded\n", __func__);

	return 0;

FAIL_DEBUGFS:
	free_irq(charger->pdata->irq_gpio, NULL);
FAIL_IRQ:
	power_supply_unregister(charger->psy_chg);
	wakeup_source_unregister(charger->monitor_ws);
	devm_kfree(&client->dev, charger);
	devm_kfree(&client->dev, pdata);
	mutex_destroy(&charger->i2c_lock);
	return ret;
}


static int hl7132_charger_remove(struct i2c_client *client)
{
	struct hl7132_charger *chg = i2c_get_clientdata(client);

	LOG_DBG(chg, "START!!");
	pr_info("%s: ++\n", __func__);

	if (client->irq){
		free_irq(client->irq, chg);
		gpio_free(chg->pdata->irq_gpio);
	}

	wakeup_source_unregister(chg->monitor_ws);

	if (chg->psy_chg)
		power_supply_unregister(chg->psy_chg);

#ifdef HALO_DBG
	hl7132_log_debugfs_exit(chg);
#endif

	pr_info("%s: --\n", __func__);

	return 0;
}

static void hl7132_charger_shutdown(struct i2c_client *client)
{
	struct hl7132_charger *chg = i2c_get_clientdata(client);

	LOG_DBG(chg, "START!!");
	pr_info("%s: ++\n", __func__);

	if (client->irq){
		free_irq(client->irq, chg);
		gpio_free(chg->pdata->irq_gpio);
	}

	wakeup_source_unregister(chg->monitor_ws);

	if (chg->psy_chg)
		power_supply_unregister(chg->psy_chg);

	cancel_delayed_work(&chg->timer_work);
	cancel_delayed_work(&chg->pps_work);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	cancel_delayed_work(&chg->wdt_control_work);
#endif

#ifdef HALO_DBG
	hl7132_log_debugfs_exit(chg);
#endif
	pr_info("%s: --\n", __func__);
}

#if defined (CONFIG_PM)
static int hl7132_charger_resume(struct device *dev)
{
	//struct hl7132_charger *charger = dev_get_drvdata(dev);
	return 0;
}

static int hl7132_charger_suspend(struct device *dev)
{
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	struct hl7132_charger *chg = dev_get_drvdata(dev);

	LOG_DBG(chg, "Start!!\r\n");
#endif

	return 0;
}
#else
#define hl7132_charger_suspend	NULL
#define hl7132_charger_resume	NULL
#endif

static const struct i2c_device_id hl7132_id_table[] = {
	{HL7132_I2C_NAME, 0},
	{ },
};

static struct of_device_id hl7132_match_table[] = {
	{ .compatible = "halo,hl7132", },
	{},
};


const struct dev_pm_ops hl7132_pm_ops = {
	.suspend = hl7132_charger_suspend,
	.resume  = hl7132_charger_resume,
};

static struct i2c_driver hl7132_driver = {
	.driver = {
		.name = HL7132_I2C_NAME,
#if defined(CONFIG_OF)
		.of_match_table = hl7132_match_table,
#endif
#if defined(CONFIG_PM)
		.pm = &hl7132_pm_ops,
#endif
	},
	.probe = hl7132_charger_probe,
	.remove = hl7132_charger_remove,
	.shutdown = hl7132_charger_shutdown,
	.id_table = hl7132_id_table,
};


static int __init hl7132_charger_init(void)
{
	int err;

	err = i2c_add_driver(&hl7132_driver);
	if (err)
		pr_err("%s: hl7132_charger driver failed (errno = %d\n", __func__, err);
	else
		pr_info("%s: Successfully added driver %s\n", __func__, hl7132_driver.driver.name);

	return err;
}

static void __exit hl7132_charger_exit(void)
{
	i2c_del_driver(&hl7132_driver);
}

module_init(hl7132_charger_init);
module_exit(hl7132_charger_exit);

MODULE_LICENSE("GPL");
MODULE_VERSION(HL7132_MODULE_VERSION);
