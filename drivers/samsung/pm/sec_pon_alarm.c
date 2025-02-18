/*
 * /drivers/rtc/power-on-alarm.c
 *
 *  Copyright (C) 2021 Samsung Electronics
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/alarmtimer.h>
#include <linux/sec_pon_alarm.h>

#define BOOTALM_BIT_EN       0
#define BOOTALM_BIT_YEAR     1
#define BOOTALM_BIT_MONTH    5
#define BOOTALM_BIT_DAY      7
#define BOOTALM_BIT_HOUR     9
#define BOOTALM_BIT_MIN     11
#define BOOTALM_BIT_TOTAL   13

struct pon_alarm_info {
	struct device *dev;
	struct rtc_wkalrm val;
	struct alarm check_poll;
	struct work_struct check_func;
	struct wakeup_source *ws;
	uint lpm_mode;
	unsigned char triggered;
};

struct pon_alarm_info pon_alarm;

static unsigned int __read_mostly rtcalarm;
static unsigned int __read_mostly lpcharge;
module_param(rtcalarm, uint, 0444);
module_param(lpcharge, uint, 0444);

int pon_alarm_get_lpcharge(void)
{
	return lpcharge;
}
EXPORT_SYMBOL_GPL(pon_alarm_get_lpcharge);

void pon_alarm_parse_data(char *alarm_data, struct rtc_wkalrm *alm)
{
	char buf_ptr[BOOTALM_BIT_TOTAL+1] = {0,};

	strlcpy(buf_ptr, alarm_data, BOOTALM_BIT_TOTAL+1);

	/* 0|1234|56|78|90|12 */
	/* 1|2010|01|01|00|00 */
	/*en yyyy mm dd hh mm */
	alm->time.tm_sec = 0;
	alm->time.tm_min = (buf_ptr[BOOTALM_BIT_MIN]-'0') * 10
					+ (buf_ptr[BOOTALM_BIT_MIN+1]-'0');
	alm->time.tm_hour = (buf_ptr[BOOTALM_BIT_HOUR]-'0') * 10
					+ (buf_ptr[BOOTALM_BIT_HOUR+1]-'0');
	alm->time.tm_mday = (buf_ptr[BOOTALM_BIT_DAY]-'0') * 10
					+ (buf_ptr[BOOTALM_BIT_DAY+1]-'0');
	alm->time.tm_mon  = (buf_ptr[BOOTALM_BIT_MONTH]-'0') * 10
					+ (buf_ptr[BOOTALM_BIT_MONTH+1]-'0');
	alm->time.tm_year = (buf_ptr[BOOTALM_BIT_YEAR]-'0') * 1000
					+ (buf_ptr[BOOTALM_BIT_YEAR+1]-'0') * 100
					+ (buf_ptr[BOOTALM_BIT_YEAR+2]-'0') * 10
					+ (buf_ptr[BOOTALM_BIT_YEAR+3]-'0');
	alm->time.tm_mon -= 1;
	alm->time.tm_year -= 1900;

	alm->enabled = (*buf_ptr == '1');
	if (*buf_ptr == '2')
		alm->enabled = 2;

	pr_info("%s: %s => tm(%d %04d-%02d-%02d %02d:%02d:%02d)\n",
			__func__, buf_ptr, alm->enabled,
			alm->time.tm_year + 1900, alm->time.tm_mon + 1, alm->time.tm_mday,
			alm->time.tm_hour, alm->time.tm_min, alm->time.tm_sec);
}
EXPORT_SYMBOL_GPL(pon_alarm_parse_data);

static int __init pon_alarm_init(void)
{
	pr_info("%s: rtcalarm=%u, lpcharge=%u\n", __func__, rtcalarm, lpcharge);
	pon_alarm.lpm_mode = lpcharge;
	pon_alarm.triggered = 0;
	pon_alarm.val.enabled = (rtcalarm) ? 1 : 0;
	rtc_time64_to_tm((time64_t)rtcalarm, &pon_alarm.val.time);

	return 0;
}

static void  __exit pon_alarm_exit(void)
{
}

module_init(pon_alarm_init);
module_exit(pon_alarm_exit);

MODULE_ALIAS("platform:sec_pon_alarm");
MODULE_DESCRIPTION("SEC_PON_ALARM driver");
MODULE_LICENSE("GPL v2");

