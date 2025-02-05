/*
 * Copyright (C) 2022 Samsung Electronics
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

#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/power_supply.h>
#include <linux/usb/typec.h>

#include <linux/mutex.h>
#include <linux/alarmtimer.h>

#ifndef __S2MF301_WATER_H__
#define __S2MF301_WATER_H__

#ifndef SEC_PER_MSEC
#define SEC_PER_MSEC 1000
#endif

#define IS_NOT_CAP(volt)	(volt <= water->cap_threshold)
#define IS_DRY(volt)		(volt > water->dry_threshold)
#define IS_WATER(volt)		(volt <= water->water_threshold)

enum s2m_water_cc_state {
	S2M_WATER_CC_OPEN,
	S2M_WATER_CC_RD,
	S2M_WATER_CC_DRP,
};

enum s2m_water_status_t {
	S2M_WATER_STATUS_INVALID,
	S2M_WATER_STATUS_DRY,
	S2M_WATER_STATUS_WATER,
	S2M_WATER_STATUS_CHECKING,
};

enum s2m_water_enable {
	S2M_WATER_DISABLE,
	S2M_WATER_ENABLE,
};

enum s2m_water_state_t {
	S2M_WATER_STATE_INVALID,
	S2M_WATER_STATE_INIT,
	S2M_WATER_STATE_DRY,
	S2M_WATER_STATE_ATTACHED,
	S2M_WATER_STATE_WATER,
	S2M_WATER_STATE_CHECK_DRY_STATUS,
	S2M_WATER_STATE_1st_CHECK,
	S2M_WATER_STATE_WAIT,
	S2M_WATER_STATE_2nd_CHECK,
	S2M_WATER_STATE_WAIT_RECHECK,
	S2M_WATER_STATE_MAX,
};

enum s2m_water_event_t {
	S2M_WATER_EVENT_INVALID,
	S2M_WATER_EVENT_INITIALIZE,
	S2M_WATER_EVENT_SBU_CHANGE_INT,
	S2M_WATER_EVENT_WET_INT,
	S2M_WATER_EVENT_ATTACH_AS_SRC,
	S2M_WATER_EVENT_ATTACH_AS_SNK,
	S2M_WATER_EVENT_DETACH,
	S2M_WATER_EVENT_WATER_DETECTED,
	S2M_WATER_EVENT_DRY_DETECTED,
	S2M_WATER_EVENT_TIMER_EXPIRED,
	S2M_WATER_EVENT_RECHECK,
	S2M_WATER_EVENT_VBUS_OR_PDRID_DETECTED,
	S2M_WATER_EVENT_MAX,
};

struct s2mf301_water_data {
	struct power_supply *psy_pm;
	struct power_supply *psy_muic;
	void *pmeter;

	struct mutex			mutex;
	struct completion		water_check_done;
	struct completion		water_wait_compl;
	struct completion		water_rr_compl;
	struct wakeup_source	*water_wake;
	struct wakeup_source	*recheck_wake;

	int check_cnt;
	int recheck_delay;
	int delay_1st_vmode_dischg;
	int delay_1st_rmode_dischg;
	int delay_1st_rmode_chg;

	int delay_2nd_dischg;
	int delay_2nd_chg;
	int delay_2nd_interval;

	int water_threshold;
	int dry_threshold;
	int cap_threshold;

	enum s2m_water_status_t notify_status;
	enum s2m_water_status_t status;
	enum s2m_water_state_t cur_state;
	enum s2m_water_event_t event;
	struct delayed_work state_work;
	struct delayed_work start_10s_work;

	struct alarm water_alarm1;
	struct alarm water_alarm2;
	bool alarm1_expired;
	bool alarm2_expired;

	int vgpadc1;
	int vgpadc2;

	int gpadc_rr_done;

	void	(*water_irq_masking)	(void *data, int masking, int mask);
	void	(*water_det_en)			(void *data, int enable);
	void	(*water_10s_en)			(void *data, int enable);
	void	(*water_set_gpadc_mode)	(void *data, int mode);
	void	(*water_set_status)		(void *data, int enable);
	int		(*water_get_status)		(void *data);
	void	(*water_1time_check)	(void *data, int *);
	int		(*pm_enable)			(void *data, int mode, int enable, int type);
	int		(*pm_get_value)			(void *data, int type);
};

extern int s2mf301_water_check_facwater(struct s2mf301_water_data *water);
extern void s2mf301_water_init(struct s2mf301_water_data *water);
extern void s2mf301_water_init_work_start(struct s2mf301_water_data *water);


#endif /* __S2MF301_WATER_H__ */
