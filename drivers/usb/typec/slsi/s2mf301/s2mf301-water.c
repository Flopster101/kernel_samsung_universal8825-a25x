/*
 driver/usb/typec/slsi/s2mf301/s2mf301-water.c - S2MF301 water device driver
 *
 * Copyright (C) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/version.h>

#include <linux/mfd/slsi/s2mf301/s2mf301_log.h>
#include <linux/usb/typec/slsi/s2mf301/s2mf301-water.h>
#include <linux/usb/typec/slsi/s2mf301/usbpd-s2mf301.h>
#include "../../../../battery/charger/s2mf301_charger/s2mf301_pmeter.h"
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>

static void s2mf301_water_state_dry(struct s2mf301_water_data *water);
static void s2mf301_water_state_attached(struct s2mf301_water_data *water);
static void s2mf301_water_state_water(struct s2mf301_water_data *water);
static int s2mf301_water_state_dry_status(struct s2mf301_water_data *water);
static void s2mf301_water_state_1st_check(struct s2mf301_water_data *water);
static void s2mf301_water_state_wait(struct s2mf301_water_data *water);
static void s2mf301_water_state_2nd_check(struct s2mf301_water_data *water);
static int s2mf301_water_state_wait_recheck(struct s2mf301_water_data *water);

static int s2mf301_water_get_prop(struct s2mf301_water_data *water, int prop);
static void s2mf301_water_psy_set_prop(struct power_supply *psy, int prop, union power_supply_propval *value);

static char *WATER_STATE_TO_STR[] = {
	"INVALID",
	"INIT",
	"DRY",
	"ATTACHED",
	"WATER",
	"CHECK_DRY_STATUS",
	"1st_CHECK",
	"WAIT",
	"2nd_CHECK",
	"WAIT_RECHECK",
	"MAX",
};

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
static char *WATER_EVENT_TO_STR[] = {
	"INVALID",
	"INITIALIZE",
	"SBU_CHANGE_INT",
	"WET_INT",
	"ATTACH_AS_SRC",
	"ATTACH_AS_SNK",
	"DETACH",
	"WATER_DETECTED",
	"DRY_DETECTED",
	"TIMER_EXPIRED",
	"RECHECK",
	"VBUS_OR_PDRID_DETECTED",
	"MAX",
};
#endif

static inline void water_sleep(int ms) {
	if (ms >= 20)
		msleep(ms);
	else
		usleep_range(ms*1000, ms*1000 + 100);
}

static void water_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
static void water_wake_unlock(struct wakeup_source *ws)
{
	__pm_relax(ws);
}
#endif

static int water_set_wakelock(struct s2mf301_water_data *water)
{
	struct wakeup_source *ws = NULL;
	struct wakeup_source *ws2 = NULL;

	ws = wakeup_source_register(NULL, "water_wake");

	if (ws == NULL) {
		s2mf301_info("%s, Fail to Rigster ws\n", __func__);
		goto err;
	} else
		water->water_wake = ws;

	ws2 = wakeup_source_register(NULL, "recheck_wake");

	if (ws2 == NULL) {
		s2mf301_info("%s, Fail to Rigster ws\n", __func__);
		goto err;
	} else
		water->recheck_wake = ws2;

	return 0;
err:
	return -1;
}

#if 0
static enum alarmtimer_restart s2mf301_water_alarm1_callback(struct alarm *alarm, ktime_t now)
{
	struct s2mf301_water_data *water = container_of(alarm, struct s2mf301_water_data, water_alarm1);

	s2mf301_info("%s,\n", __func__);
	water->alarm1_expired = 1;

	return ALARMTIMER_NORESTART;
}

static void s2mf301_water_alarm1_start(struct s2mf301_water_data *water, int ms)
{
	ktime_t sec, msec;

	water->alarm1_expired = 0;
	alarm_cancel(&water->water_alarm1);
	sec = ktime_get_boottime();
	msec = ktime_set(0, ms * NSEC_PER_MSEC);
	alarm_start(&water->water_alarm1, ktime_add(sec, msec));
}
#endif

#if 0
static enum alarmtimer_restart s2mf301_water_alarm2_callback(struct alarm *alarm, ktime_t now)
{
	struct s2mf301_water_data *water = container_of(alarm, struct s2mf301_water_data, water_alarm2);

	s2mf301_info("%s,\n", __func__);
	water->alarm2_expired = 1;

	return ALARMTIMER_NORESTART;
}

static void s2mf301_water_alarm2_start(struct s2mf301_water_data *water, int ms)
{
	ktime_t sec, msec;

	water->alarm2_expired = 0;
	alarm_cancel(&water->water_alarm2);
	sec = ktime_get_boottime();
	msec = ktime_set(0, ms * NSEC_PER_MSEC);
	alarm_start(&water->water_alarm2, ktime_add(sec, msec));
}
#endif

static int s2mf301_water_is_rid_attached(struct s2mf301_water_data *water)
{
	struct s2mf301_usbpd_data *pdic_data = container_of(water, struct s2mf301_usbpd_data, water);
	int ret = 0;

	switch (pdic_data->rid) {
	case REG_RID_255K:
	case REG_RID_301K:
	case REG_RID_523K:
	case REG_RID_619K:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static void s2mf301_water_notify(struct s2mf301_water_data *water, int status)
{
	struct s2mf301_usbpd_data *pdic_data = container_of(water, struct s2mf301_usbpd_data, water);
	union power_supply_propval value;

	if (water->notify_status == status) {
		s2mf301_info("[WATER] %s, skip notify\n", __func__);
		return;
	}

	if (status == S2M_WATER_STATUS_DRY) {
		value.intval = false;
		s2mf301_water_psy_set_prop(water->psy_muic,
				(enum power_supply_property)POWER_SUPPLY_LSI_PROP_WATER_STATUS,
				&value);

		s2mf301_usbpd_water_set_status(pdic_data, S2M_WATER_STATUS_DRY);

	} else if (status ==S2M_WATER_STATUS_WATER) {
		value.intval = true;
		s2mf301_water_psy_set_prop(water->psy_muic,
				(enum power_supply_property)POWER_SUPPLY_LSI_PROP_WATER_STATUS,
				&value);

		s2mf301_usbpd_water_set_status(pdic_data, S2M_WATER_STATUS_WATER);

	} else
		s2mf301_info("[WATER] %s, invalid status(%d)\n", __func__, status);
}

static void s2mf301_water_set_status(struct s2mf301_water_data *water, int status)
{
	struct s2mf301_usbpd_data *pdic_data = container_of(water, struct s2mf301_usbpd_data, water);

	if (water->status == status) {
		s2mf301_info("[WATER] %s, same status, skip\n", __func__);
		return;
	}
	water->status = status;

	if (status == S2M_WATER_STATUS_DRY) {
		s2mf301_info("[WATER] %s, DRY Detected\n", __func__);
		water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_RR);
		water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_WATER);

		s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_DRP);
		water->water_det_en(water->pmeter, true);
		water->water_10s_en(water->pmeter, false);

		water->water_set_status(water->pmeter, S2M_WATER_STATUS_DRY);
		s2mf301_water_notify(water, status);

		water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
		water->pm_enable(water->pmeter, CONTINUOUS_MODE, true, S2MF301_PM_TYPE_GPADC12);

		water_sleep(100);
		water->water_irq_masking(water->pmeter, false, S2MF301_IRQ_TYPE_CHANGE);
	} else if (status == S2M_WATER_STATUS_WATER) {
		s2mf301_info("[WATER] %s, Water Detected\n", __func__);

		water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_RR);
		water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_CHANGE);

		s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_RD);
		//water->water_det_en(water->pmeter, true);
		//water->water_10s_en(water->pmeter, true);

		water->water_set_status(water->pmeter, S2M_WATER_STATUS_WATER);
		s2mf301_water_notify(water, status);

		//water_sleep(50);
		//water->water_irq_masking(water->pmeter, false, S2MF301_IRQ_TYPE_WATER);

		schedule_delayed_work(&water->start_10s_work, msecs_to_jiffies(10 * SEC_PER_MSEC));
	} else if (status == S2M_WATER_STATUS_CHECKING) {
		s2mf301_info("[WATER] %s, start checking\n", __func__);
	} else
		s2mf301_info("[WATER] %s, invalid status(%d)\n", __func__, status);
}

static void s2mf301_water_start_dry_check_work(struct work_struct *work)
{
	struct s2mf301_water_data *water = container_of(work, struct s2mf301_water_data, start_10s_work.work);

	water->water_det_en(water->pmeter, true);
	water->water_10s_en(water->pmeter, true);

	water->water_irq_masking(water->pmeter, false, S2MF301_IRQ_TYPE_WATER);
	s2mf301_info("[WATER] %s, DryChecker enable\n", __func__);
}

static void s2mf301_water_get_vgpadc_rr(struct s2mf301_water_data *water, int *volt)
{
	int timeout = 0;

	water->vgpadc1 = water->vgpadc2 = 0;

	/* unmasking RR int*/
	water->water_irq_masking(water->pmeter, false, S2MF301_IRQ_TYPE_RR);
	/* start RR */
	water->pm_enable(water->pmeter, REQUEST_RESPONSE_MODE, true, S2MF301_PM_TYPE_GPADC12);

	reinit_completion(&water->water_rr_compl);
	timeout = wait_for_completion_timeout(&water->water_rr_compl, msecs_to_jiffies(30));
	if (timeout == 0) {
		s2mf301_info("%s, 30ms timeout, wait 50ms more\n", __func__);
		water_sleep(50);
	}

	/* masking RR int */
	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_RR);
	/* clear RR */
	water->pm_enable(water->pmeter, REQUEST_RESPONSE_MODE, false, S2MF301_PM_TYPE_GPADC12);

	volt[0] = water->vgpadc1;
	volt[1] = water->vgpadc2;
}

void s2mf301_water_1time_check(struct s2mf301_water_data *water, int *volt)
{
	/* turn on CurrentSource */
	water->water_det_en(water->pmeter, true);

	/* wait charge */
	water_sleep(water->delay_2nd_chg);

	/* Check RR, copy to volt */
	s2mf301_water_get_vgpadc_rr(water, volt);

	s2mf301_info("[WATER] %s, volt1 = %dmV, volt2 = %dmV\n", __func__, volt[0], volt[1]);

	/* turn off CurrentSource */
	water->water_det_en(water->pmeter, false);
}

/* return water ? true : false */
static int s2mf301_water_check_capacitance(struct s2mf301_water_data *water)
{
	int volt[2];

	/* off CurrentSource, disable CO */
	water->water_set_gpadc_mode(water->pmeter, S2MF301_GPADC_VMODE);
	water->pm_enable(water->pmeter, CONTINUOUS_MODE, false, S2MF301_PM_TYPE_GPADC12);
	water->water_det_en(water->pmeter, false);

	/* Wait Dischg Time */
	water_sleep(water->delay_1st_vmode_dischg);

	/* Check RR, copy to volt */
	s2mf301_water_get_vgpadc_rr(water, volt);

	/* restore Rmode */
	water->water_set_gpadc_mode(water->pmeter, S2MF301_GPADC_RMODE);

	s2mf301_info("[WATER] %s, volt[%dmV, %dmV]\n", __func__, volt[0], volt[1]);

	if (IS_NOT_CAP(volt[0]) && IS_NOT_CAP(volt[1]))
		return false;

	return true;
}

int s2mf301_water_check_facwater(struct s2mf301_water_data *water)
{
	int volt[2] = {0, };

	water->water_det_en(water->pmeter, true);
	water->pm_enable(water->pmeter, CONTINUOUS_MODE, true, S2MF301_PM_TYPE_GPADC12);
	water_sleep(50);

	volt[0] = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_GPADC1);
	volt[1] = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_GPADC2);
	s2mf301_info("[WATER] %s, volt[%d, %d]\n", __func__, volt[0], volt[1]);

	if (volt[0] <= 300 && volt[1] <= 300)
		return true;
	else
		return false;
}

static void s2mf301_water_state_dry(struct s2mf301_water_data *water)
{
	s2mf301_water_set_status(water, S2M_WATER_STATUS_DRY);
}

static void s2mf301_water_state_attached(struct s2mf301_water_data *water)
{
	complete(&water->water_check_done);

	water->event = S2M_WATER_EVENT_DRY_DETECTED;
}

static void s2mf301_water_state_water(struct s2mf301_water_data *water)
{
	s2mf301_water_set_status(water, S2M_WATER_STATUS_WATER);
}

static void s2mf301_water_state_1st_check(struct s2mf301_water_data *water)
{
	int vbus, rid;

	s2mf301_water_set_status(water, S2M_WATER_STATUS_CHECKING);
	vbus = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_VCHGIN);
	rid = s2mf301_water_is_rid_attached(water);
	if (vbus >= 4000 || rid) {
		s2mf301_info("%s, VBUS(%dmV), rid(%d) skip water check\n", __func__, vbus, rid);
		water->event = S2M_WATER_EVENT_VBUS_OR_PDRID_DETECTED;
		goto exit;
	}

	if (!s2mf301_water_check_capacitance(water)) {
		/* cap not exists, goto dry */
		s2mf301_info("[WATER] %s, No Cap Detected, goto Dry\n", __func__);
		water->event = S2M_WATER_EVENT_DRY_DETECTED;
		goto exit;
	} else
		water->event = S2M_WATER_EVENT_WATER_DETECTED;;

exit:
	return;
}

static void s2mf301_water_state_wait(struct s2mf301_water_data *water)
{
	struct s2mf301_usbpd_data *pdic_data =
		container_of(water, struct s2mf301_usbpd_data, water);
	int timeout = 0;
	int vbus, rid;

	s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_RD);

	reinit_completion(&water->water_wait_compl);
	timeout = wait_for_completion_timeout(&water->water_wait_compl, msecs_to_jiffies(200));

	vbus = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_VCHGIN);
	rid = s2mf301_water_is_rid_attached(water);
	s2mf301_info("[WATER] %s, remaintime(%d), vbus(%d)\n", __func__, timeout, vbus);
	if (vbus >= 4000 || rid) {
		s2mf301_info("%s, VBUS(%dmV), rid(%d) skip water check\n", __func__, vbus, rid);
		water->event = S2M_WATER_EVENT_VBUS_OR_PDRID_DETECTED;
		goto exit;
	}

	if (timeout == 0) {
		water->event = S2M_WATER_EVENT_TIMER_EXPIRED;
	}
exit:
	return;
}

static int s2mf301_water_state_dry_status(struct s2mf301_water_data *water)
{
	int vbus = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_VCHGIN);
	int status = water->water_get_status(water->pmeter);
	int volt[2] = {0, };
	int is_cap_water = false;
	bool is_water_detected = false;

	s2mf301_water_set_status(water, S2M_WATER_STATUS_CHECKING);

	s2mf301_info("%s, vbus(%d)mV, ++\n", __func__, vbus);
	if (vbus >= 4000) {
		s2mf301_info("%s, recheck after 10s\n", __func__);
		water->event = S2M_WATER_EVENT_TIMER_EXPIRED;
		//schedule_delayed_work(&water->state_work, msecs_to_jiffies(10 * 1000));
		water_wake_lock(water->recheck_wake);

		water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
		water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_WATER);
		water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_RR);

		return 10 * SEC_PER_MSEC;
	}

	s2mf301_info("%s, status[0x%x / %s]\n", __func__, status, ((status) ? ("WATER") : ("DRY")));

	switch (status) {
	case S2MF301_WATER_GPADC_DRY:
		break;
	case S2MF301_WATER_GPADC1:
	case S2MF301_WATER_GPADC2:
	case S2MF301_WATER_GPADC12:
		is_water_detected = false;
		break;
	default:
		break;
	}

	volt[0] = s2mf301_water_get_prop(water, POWER_SUPPLY_LSI_PROP_VGPADC1);
	volt[1] = s2mf301_water_get_prop(water, POWER_SUPPLY_LSI_PROP_VGPADC2);
	s2mf301_info("%s, vgpadc[%dmV / %dmV]\n", __func__, volt[0], volt[1]);

	if (is_water_detected) {
		water->event = S2M_WATER_EVENT_WATER_DETECTED;
		return 0;
	}

	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_WATER);
	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_RR);

	water->pm_enable(water->pmeter, CONTINUOUS_MODE, true, S2MF301_PM_TYPE_GPADC12);

	/* charging */
	water->water_10s_en(water->pmeter, false);
	water_sleep(80);

	/* check resistance */
	s2mf301_water_get_vgpadc_rr(water, volt);
	s2mf301_info("%s, res(%d, %d)\n", __func__, volt[0], volt[1]);

	/* discharging */
	water_sleep(30);

	/* check capacitance */
	is_cap_water = s2mf301_water_check_capacitance(water);

	if ( !(IS_DRY(volt[0]) && IS_DRY(volt[1])) )
		is_water_detected = true;

	if (is_water_detected || is_cap_water)
		water->event = S2M_WATER_EVENT_WATER_DETECTED;
	else
		water->event = S2M_WATER_EVENT_DRY_DETECTED;

	return 0;
}

static void s2mf301_water_state_2nd_check(struct s2mf301_water_data *water)
{
	int vbus = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_VCHGIN);
	int rid = s2mf301_water_is_rid_attached(water);
	int i;
	int check_cnt = water->check_cnt;
	int avg[2] = {0, 0};
	int volt[2] = {0, 0};

	s2mf301_water_set_status(water, S2M_WATER_STATUS_CHECKING);
	if (vbus >= 4000 || rid) {
		s2mf301_info("%s, VBUS(%dmV), rid(%d) skip water check\n", __func__, vbus, rid);
		water->event = S2M_WATER_EVENT_VBUS_OR_PDRID_DETECTED;
		goto exit;
	}

	// 2nd pre setting
	water->water_det_en(water->pmeter, false);
	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_WATER);

	water->water_set_status(water->pmeter, S2M_WATER_STATUS_DRY); //need ?

	s2mf301_info("%s, start check cnt[%d], dischg[%d], recheck[%d],  interval[%d]\n", __func__,
			water->check_cnt, water->delay_2nd_dischg,
			water->recheck_delay, water->delay_2nd_interval);

	for(i = 0; i < check_cnt; i++) {
		vbus = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_VCHGIN);
		rid = s2mf301_water_is_rid_attached(water);
		if (vbus >= 4000 || rid) {
			s2mf301_info("%s, VBUS(%dmV), rid(%d) skip water check\n", __func__, vbus, rid);
			water->event = S2M_WATER_EVENT_VBUS_OR_PDRID_DETECTED;
			goto exit;
		}

		s2mf301_water_1time_check(water, volt);
		avg[0]+=volt[0];
		avg[1]+=volt[1];
		water_sleep(water->delay_2nd_interval);
	}

	avg[0] = avg[0] / check_cnt;
	avg[1] = avg[1] / check_cnt;
	s2mf301_info("[WATER] %s, avg[%dmV / %dmV]\n", __func__, avg[0], avg[1]);

	if (IS_WATER(avg[0]) || IS_WATER(avg[1]))
		water->event = S2M_WATER_EVENT_WATER_DETECTED;
	else if (IS_DRY(avg[0]) && IS_DRY(avg[1]))
		water->event = S2M_WATER_EVENT_DRY_DETECTED;
	else
		water->event = S2M_WATER_EVENT_RECHECK;

exit:
	return;
}

static int s2mf301_water_state_wait_recheck(struct s2mf301_water_data *water)
{
	struct s2mf301_usbpd_data *pdic_data = container_of(water, struct s2mf301_usbpd_data, water);

	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_RR);
	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_WATER);

	s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_DRP);

	water->water_set_status(water->pmeter, S2M_WATER_STATUS_DRY);

	water_wake_lock(water->recheck_wake);

	water->event = S2M_WATER_EVENT_TIMER_EXPIRED;

	return 1 * MSEC_PER_SEC;
}

static void s2mf301_water_state_transition(struct s2mf301_water_data *water, enum s2m_water_state_t next)
{
	int ms = 0;

	mutex_lock(&water->mutex);
	water->cur_state = next;
	water->event = S2M_WATER_EVENT_INVALID;
	s2mf301_info("[WATER] %s, new state[%s]\n", __func__, WATER_STATE_TO_STR[water->cur_state]);

	switch (next) {
	case S2M_WATER_STATE_DRY:
		s2mf301_water_state_dry(water);
		break;
	case S2M_WATER_STATE_ATTACHED:
		s2mf301_water_state_attached(water);
		break;
	case S2M_WATER_STATE_WATER:
		s2mf301_water_state_water(water);
		break;
	case S2M_WATER_STATE_1st_CHECK:
		s2mf301_water_state_1st_check(water);
		break;
	case S2M_WATER_STATE_WAIT:
		s2mf301_water_state_wait(water);
		break;
	case S2M_WATER_STATE_CHECK_DRY_STATUS:
		ms = s2mf301_water_state_dry_status(water);
		break;
	case S2M_WATER_STATE_2nd_CHECK:
		s2mf301_water_state_2nd_check(water);
		break;
	case S2M_WATER_STATE_WAIT_RECHECK:
		ms = s2mf301_water_state_wait_recheck(water);
		break;
	default:
		break;
	}

	mutex_unlock(&water->mutex);
	if (water->event == S2M_WATER_EVENT_INVALID)
		return;
	else {
		s2mf301_info("%s, goto next state ms(%d)\n", __func__, ms);
		schedule_delayed_work(&water->state_work, msecs_to_jiffies(ms));
	}
}

#if IS_ENABLED(CONFIG_SEC_FACTORY)
static void s2mf301_water_state_work(struct work_struct *work)
{
	struct s2mf301_water_data *water = container_of(work, struct s2mf301_water_data, state_work.work);

	s2mf301_info("[WATER] %s, skip Factory Binary\n", __func__);
	s2mf301_water_state_transition(water, S2M_WATER_STATE_DRY);
}
#else
static void s2mf301_water_state_work(struct work_struct *work)
{
	struct s2mf301_water_data *water = container_of(work, struct s2mf301_water_data, state_work.work);

	enum s2m_water_state_t cur_state = water->cur_state;
	enum s2m_water_state_t next_state = S2M_WATER_STATE_INVALID;
	enum s2m_water_event_t event = water->event;
	bool skip = false;

	water_wake_lock(water->water_wake);
	switch (cur_state) {
	case S2M_WATER_STATE_INIT:
		switch (event) {
		case S2M_WATER_EVENT_WATER_DETECTED:
			next_state = S2M_WATER_STATE_WATER;
			break;
		default:
			next_state = S2M_WATER_STATE_DRY;
			break;
		}
		break;
	case S2M_WATER_STATE_DRY:
		switch (event) {
		case S2M_WATER_EVENT_ATTACH_AS_SRC:
		case S2M_WATER_EVENT_ATTACH_AS_SNK:
			next_state = S2M_WATER_STATE_ATTACHED;
			break;
		case S2M_WATER_EVENT_SBU_CHANGE_INT:
		default:
			next_state = S2M_WATER_STATE_1st_CHECK;
			break;
		}
		break;
	case S2M_WATER_STATE_ATTACHED:
		next_state = S2M_WATER_STATE_DRY;
		break;
	case S2M_WATER_STATE_1st_CHECK:
		switch (event) {
		case S2M_WATER_EVENT_VBUS_OR_PDRID_DETECTED:
			next_state = S2M_WATER_STATE_ATTACHED;
			break;
		case S2M_WATER_EVENT_DRY_DETECTED:
			next_state = S2M_WATER_STATE_DRY;
			break;
		default:
			next_state = S2M_WATER_STATE_WAIT;
			break;
		}
		break;
	case S2M_WATER_STATE_WAIT:
		switch (event) {
		case S2M_WATER_EVENT_VBUS_OR_PDRID_DETECTED:
		case S2M_WATER_EVENT_ATTACH_AS_SNK:
			next_state = S2M_WATER_STATE_ATTACHED;
			break;
		case S2M_WATER_EVENT_ATTACH_AS_SRC:
		case S2M_WATER_EVENT_TIMER_EXPIRED:
		default:
			next_state = S2M_WATER_STATE_2nd_CHECK;
			break;
		}
		break;
	case S2M_WATER_STATE_2nd_CHECK:
		switch (event) {
		case S2M_WATER_EVENT_VBUS_OR_PDRID_DETECTED:
		case S2M_WATER_EVENT_ATTACH_AS_SNK:
			next_state = S2M_WATER_STATE_ATTACHED;
			break;
		case S2M_WATER_EVENT_WATER_DETECTED:
			next_state = S2M_WATER_STATE_WATER;
			break;
		case S2M_WATER_EVENT_DRY_DETECTED:
			next_state = S2M_WATER_STATE_DRY;
			break;
		case S2M_WATER_EVENT_RECHECK:
		default:
			next_state = S2M_WATER_STATE_WAIT_RECHECK;
			break;
		}
		break;
	case S2M_WATER_STATE_WATER:
		next_state = S2M_WATER_STATE_CHECK_DRY_STATUS;
		break;
	case S2M_WATER_STATE_CHECK_DRY_STATUS:
		water_wake_unlock(water->recheck_wake);
		switch (event) {
		case S2M_WATER_EVENT_DRY_DETECTED:
			next_state = S2M_WATER_STATE_DRY;
			break;
		case S2M_WATER_EVENT_WATER_DETECTED:
			next_state = S2M_WATER_STATE_WATER;
			break;
		default:
			next_state = S2M_WATER_STATE_CHECK_DRY_STATUS;
			break;
		}
		break;
	case S2M_WATER_STATE_WAIT_RECHECK:
		water_wake_unlock(water->recheck_wake);
		next_state = S2M_WATER_STATE_2nd_CHECK;
		break;
	default:
		skip = true;
		break;
	}

	if (skip) {
		s2mf301_info("[WATER] %s, skip state[%s], event[%s]\n", __func__, WATER_STATE_TO_STR[cur_state],
				WATER_EVENT_TO_STR[event]);
	} else {
		s2mf301_info("[WATER] %s, event[%s], state[%s -> %s]\n", __func__, WATER_EVENT_TO_STR[event],
				WATER_STATE_TO_STR[cur_state], WATER_STATE_TO_STR[next_state]);
		s2mf301_water_state_transition(water, next_state);
	}

	water_wake_unlock(water->water_wake);
}
#endif

static void s2mf301_water_psy_set_prop(struct power_supply *psy,
		int prop, union power_supply_propval *value)
{
	if (!psy) {
		s2mf301_err("%s invalid psy, prop(%d)\n", __func__, prop);
		return;
	}

	power_supply_set_property(psy, (enum power_supply_property)prop, value);
}

static int s2mf301_water_get_prop(struct s2mf301_water_data *water, int prop)
{
	 union power_supply_propval value;

	if (!water->psy_pm) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		water->psy_pm = get_power_supply_by_name("s2mf301-pmeter");
#endif
		if (!water->psy_pm) {
			s2mf301_info("%s, Fail to get psy_pm\n", __func__);
			return -1;
		}
	}

	water->psy_pm->desc->get_property(water->psy_pm, prop, &value);

	return value.intval;
}

void s2mf301_water_init_work_start(struct s2mf301_water_data *water)
{
	s2mf301_pm_water_irq_init(water);
	schedule_delayed_work(&water->state_work, 0);
}

void s2mf301_water_init(struct s2mf301_water_data *water)
{

	s2mf301_info("[WATER] %s, s2mf301 water detection enabled\n", __func__);

	water->cur_state = S2M_WATER_STATE_INIT;
	water->event = S2M_WATER_EVENT_INITIALIZE;
	INIT_DELAYED_WORK(&water->state_work, s2mf301_water_state_work);
	INIT_DELAYED_WORK(&water->start_10s_work, s2mf301_water_start_dry_check_work);
#if 0
	alarm_init(&water->water_alarm1, ALARM_BOOTTIME, s2mf301_water_alarm1_callback);
	alarm_init(&water->water_alarm2, ALARM_BOOTTIME, s2mf301_water_alarm2_callback);
#endif

	water->psy_muic = power_supply_get_by_name("muic-manager");

	water->check_cnt = 5;					//count(times)
	water->recheck_delay = 1000;			//ms

	water->delay_1st_vmode_dischg = 10;		//ms
	water->delay_1st_rmode_dischg = 100;	//ms
	water->delay_1st_rmode_chg = 10;		//ms

	water->delay_2nd_dischg = 100;			//ms
	water->delay_2nd_chg = 10;				//ms
	water->delay_2nd_interval = 70;			//ms

	water->water_threshold = 780;
	water->dry_threshold = 1060;
	water->cap_threshold = 100;

	init_completion(&water->water_check_done);
	init_completion(&water->water_wait_compl);
	init_completion(&water->water_rr_compl);
	water_set_wakelock(water);
	mutex_init(&water->mutex);

	water->pmeter = (struct s2mf301_pmeter_data *)s2mf301_pm_water_init(water);
}
