/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/sec_debug.h>
#include <linux/version.h>

#include "../comm/shub_comm.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../sensorhub/shub_device.h"
#include "../sensorhub/shub_firmware.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_file_manager.h"
#include "shub_debug.h"

static struct timer_list debug_timer;
static struct workqueue_struct *debug_wq;
static struct work_struct work_debug;

#ifdef CONFIG_SHUB_DEBUG
static struct workqueue_struct *print_log_wq;
static struct work_struct work_print_log;
static struct list_head print_log_list;
#endif

static int check_noevent_reset_cnt;
static char open_cal_result[9];

#define SHUB_DEBUG_TIMER_SEC    (5 * HZ)

/*
 * check_sensor_event
 *  - return true : there is no accel or light sensor event over 5sec when sensor is registered
 */
static void check_no_event(void)
{
	u64 timestamp = get_current_timestamp();
	int type, ret;
	struct shub_sensor *sensor;
	struct sensor_event *event;
	bool check_reset = false;
	char buffer[9] = {0,};

	if (check_noevent_reset_cnt >= 0 && check_noevent_reset_cnt == get_reset_count())
		check_reset = true;

	check_noevent_reset_cnt = -1;

	for (type = 0 ; type < SENSOR_TYPE_LEGACY_MAX ; type++) {
		sensor = get_sensor(type);
		if (!sensor)
			continue;

		event = get_sensor_event(type);
		if (sensor->report_mode_continuous && sensor->enabled && sensor->max_report_latency == 0 &&
		    MAX(sensor->enable_timestamp, sensor->change_timestamp) + 5000000000ULL < timestamp &&
		    event->received_timestamp + 5000000000ULL < timestamp) {
			shub_infof("sensor(%d) %lld(%lld), cur = %lld en = %lld change = %lld", type, event->received_timestamp,
				   event->timestamp, timestamp, sensor->enable_timestamp, sensor->change_timestamp);

			if (sensor->enable_timestamp < sensor->disable_timestamp) {
				shub_infof("enable_timestamp invalid: enable_timestamp(%lld)  disable_timestamp(%lld)",
				sensor->enable_timestamp, sensor->disable_timestamp);
				break;
			}
			if (check_reset) {
				shub_errf("no event, no sensorhub reset");
				reset_mcu(RESET_TYPE_KERNEL_NO_EVENT);
				break;
			}

			check_noevent_reset_cnt = get_reset_count();

			buffer[0] = type;
			memcpy(&buffer[1], &(sensor->sampling_period), sizeof(sensor->sampling_period));
			ret = shub_send_command(CMD_SETVALUE, TYPE_HUB, NO_EVENT_CHECK, buffer, sizeof(buffer));
			if (ret < 0)
				shub_errf("type %d comm failed ret %d", type, ret);
			else
				break;
		}
	}
}

static void debug_work_func(struct work_struct *work)
{
	int type;
	uint64_t probe_state[2] = {0, };
	uint64_t en_state[2] = {0, };
	struct shub_data_t *data = get_shub_data();
	struct rtc_time tm;
	char time_temp[50] = {0, };

	for (type = 0; type < SENSOR_TYPE_MAX; type++) {
		if (get_sensor_enabled(type))
			print_sensor_debug(type);
	}

	get_tm(&(tm));

	memset(time_temp, 0, sizeof(time_temp));
	snprintf(time_temp, sizeof(time_temp), "%04d/%02d/%02d %02d:%02d:%02d",
		 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	get_sensors_legacy_probe_state(probe_state);
	get_sensors_legacy_enable_state(en_state);
	shub_infof(" (%s) FW(%d):%u, Sensor state: 0x%llx, 0x%llx, En: 0x%llx, 0x%llx, "
		   "Reset cnt: %d[%d : C %u(%u, %u), N %u, %u, %u], Cal result : [M:%c, P:%c]",
		   time_temp, get_firmware_type(), get_firmware_rev(),
		   probe_state[0], probe_state[1], en_state[0], en_state[1], data->cnt_reset,
		   data->cnt_shub_reset[RESET_TYPE_MAX], data->cnt_shub_reset[RESET_TYPE_KERNEL_COM_FAIL],
		   get_cnt_comm_fail(), get_cnt_timeout(), data->cnt_shub_reset[RESET_TYPE_KERNEL_NO_EVENT],
		   data->cnt_shub_reset[RESET_TYPE_HUB_NO_EVENT], data->cnt_shub_reset[RESET_TYPE_HUB_REQ_TASK_FAILURE],
		   open_cal_result[SENSOR_TYPE_GEOMAGNETIC_FIELD], open_cal_result[SENSOR_TYPE_PRESSURE]);

	if (is_shub_working())
		check_no_event();

#if defined(CONFIG_SHUB_MTK) && IS_ENABLED(CONFIG_SEC_DEBUG)
	if (data->hub_crash_timestamp && data->hub_crash_timestamp + 100000000000ULL < get_current_timestamp() ) {
		shub_infof("hub crash timestamp %llu", data->hub_crash_timestamp);
		/* only work for debug level is mid */
		if (shub_debug_level()) {
			shub_infof("panic!");
			shub_infof("mini dump : %s", data->mini_dump);
			panic("sensorhub crash error\n");

		} else {
			shub_infof("debug level is low");
		}
		data->hub_crash_timestamp = 0;
	}
#endif
}

static void debug_timer_func(struct timer_list *timer)
{
	queue_work(debug_wq, &work_debug);
	mod_timer(&debug_timer, round_jiffies_up(jiffies + SHUB_DEBUG_TIMER_SEC));
}

#ifdef CONFIG_SHUB_DEBUG
static void print_log_work_func(struct work_struct *work)
{
	struct list_head *curr;
	struct print_log_t *log;

	if (!list_empty(&print_log_list)) {
		curr = print_log_list.next;
		log = list_entry(curr, struct print_log_t, list);

		shub_infof("[%lld] %s", log->timestamp, log->buf);

		list_del(curr);
		kfree(log->buf);
		kfree(log);
	} else {
		shub_errf("print_log_list is empty");
	}
}
#endif

void set_open_cal_result(int type, int result)
{
	if (result >= 0)
		open_cal_result[type] = 'P';
	else
		open_cal_result[type] = 'F';
}

/*
 * low return 0, mid and high return over 0.
 */
int shub_debug_level(void)
{
#if defined(CONFIG_SHUB_MTK)
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 0, 0)
	return !is_debug_level_low();
#else
	return SEC_DEBUG_LEVEL(kernel);
#endif
#else // LSI
	return secdbg_mode_enter_upload();
#endif
}

int init_shub_debug(void)
{
	int type;

	timer_setup(&debug_timer, debug_timer_func, 0);
	debug_wq = create_singlethread_workqueue("shub_debug_wq");
	if (!debug_wq)
		return -ENOMEM;

	INIT_WORK(&work_debug, debug_work_func);

#ifdef CONFIG_SHUB_DEBUG
	print_log_wq = create_singlethread_workqueue("shub_print_log_wq");
	if (!print_log_wq)
		return -ENOMEM;

	INIT_WORK((struct work_struct *)&work_print_log, print_log_work_func);
	INIT_LIST_HEAD(&print_log_list);
#endif
	for (type = 0 ; type <= SENSOR_TYPE_PROXIMITY ; type++)
		open_cal_result[type] = 'N';

	return 0;
}

void exit_shub_debug(void)
{
	del_timer(&debug_timer);
	cancel_work_sync(&work_debug);
	destroy_workqueue(debug_wq);
}

void enable_debug_timer(void)
{
	mod_timer(&debug_timer, round_jiffies_up(jiffies + SHUB_DEBUG_TIMER_SEC));
}

void disable_debug_timer(void)
{
	del_timer_sync(&debug_timer);
	cancel_work_sync(&work_debug);
}

int print_mcu_debug(char *dataframe, int *index, int frame_len)
{
	u16 length = 0;
	int cur = *index;
	int len_size = 1;

	if (is_support_system_feature(SF_DEBUG_V2))
		len_size = 2;

	memcpy(&length, dataframe + *index, len_size);
	*index += len_size;

	if (length > frame_len - *index || length <= 0) {
		shub_infof("[M] invalid debug length(%u/%d/%d)", length, frame_len, cur);
		return -1;
	}

	shub_info("[M] %s", &dataframe[*index]);
	*index += length;
	return 0;
}

#ifdef CONFIG_SHUB_DEBUG
void print_log_debug(char *buf, int buf_len, u64 timestamp)
{
	struct print_log_t *print_log;

	print_log = kzalloc(sizeof(struct print_log_t), GFP_KERNEL);
	print_log->buf = kzalloc(buf_len, GFP_KERNEL);
	memcpy(print_log->buf, buf, buf_len);
	print_log->timestamp = timestamp;

	list_add_tail(&print_log->list, &print_log_list);
	queue_work(print_log_wq, (struct work_struct *)&work_print_log);
}
#endif

#define SHUB_LOG_MAX_BYTE 200
#define LOG_UNIT_SIZE	 3
void print_dataframe(char *dataframe, int frame_len)
{
	char raw_data[SHUB_LOG_MAX_BYTE * LOG_UNIT_SIZE + 1];
	int i = 0, cur = 0, size;
	int len = sizeof(raw_data);

	while ((frame_len - cur) > 0) {
		int pr_size = ((frame_len - cur) > SHUB_LOG_MAX_BYTE) ? SHUB_LOG_MAX_BYTE : (frame_len - cur);

		memset(raw_data, 0, sizeof(raw_data));
		for (i = 0, size = 0; i < pr_size; i++)
			size += snprintf(raw_data + size, len - size, "%02x ", *(dataframe + cur++));

		shub_info("(%d/%d) %s", cur, frame_len, raw_data);
	}
}

int print_system_info(char *dataframe, int *index, int frame_len)
{
	struct sensor_debug_info {
		uint8_t uid;
		uint8_t total_count;
		uint8_t ext_client;
		int32_t ext_sampling;
		int32_t ext_report;
		int32_t fastest_sampling;
	};

	struct base_timestamp {
		uint64_t kernel_base;
		uint64_t hub_base;
	};

	struct utc_time {
		int8_t nHour;
		int8_t nMinute;
		int8_t nSecond;
		int16_t nMilliSecond;
	};

	struct system_debug_info {
		int32_t version;
		int32_t rate;
		int8_t ap_state;
		struct utc_time time;
		struct base_timestamp timestamp;
	};
	//===================================================//
	struct sensor_debug_info *info = 0;
	struct system_debug_info *s_info = 0;
	int i;
	int count, system_info_len;

	if (*index + 1 > frame_len) {
		shub_errf("parsing error");
		return -EINVAL;
	}
	count = *dataframe;
	++dataframe;
	*index += 1;

	system_info_len = (sizeof(struct sensor_debug_info) * count + sizeof(struct system_debug_info));

	if (*index + system_info_len > frame_len) {
		shub_errf("parsing error %d", count);
		return -EINVAL;
	}

	*index += system_info_len;

	shub_info("==system info ===");
	for (i = 0; i < count; ++i) {
		info = (struct sensor_debug_info *)dataframe;
		shub_info("id(%d), total(%d), external(%d), e_sampling(%d), e_report(%d), fastest(%d)", info->uid,
			  info->total_count, info->ext_client, info->ext_sampling, info->ext_report,
			  info->fastest_sampling);
		dataframe += sizeof(struct sensor_debug_info);
	}

	s_info = (struct system_debug_info *)dataframe;
	shub_info("version(%d), rate(%d), ap_state(%s), time(%d:%d:%d.%d), base_ts_k(%lld), base_ts_hub(%lld)",
		  s_info->version, s_info->rate, s_info->ap_state == 0 ? "run" : "suspend", s_info->time.nHour,
		  s_info->time.nMinute, s_info->time.nSecond, s_info->time.nMilliSecond, s_info->timestamp.kernel_base,
		  s_info->timestamp.hub_base);

	return 0;
}


int log_dump_size;

void init_log_dump(void)
{
	shub_infof("");
	log_dump_size = 0;
	shub_file_remove("/data/vendor/sensorhub/hub_log_dump.txt");
}

int save_log_dump(char *dataframe, int *index, int frame_len)
{
	u16 length = 0;
	int ret;

	shub_infof("%d", log_dump_size);
	memcpy(&length, dataframe + *index, 1);
	*index += 1;

	shub_info("[D] %s", &dataframe[*index]);

	ret = shub_file_write("/data/vendor/sensorhub/hub_log_dump.txt", dataframe + *index, length, log_dump_size);
	if (ret > 0) {
		shub_infof("ret %d length %d", ret, length);
		log_dump_size += length;
	} else {
		shub_errf("save file failed");
	}

	*index += length;
	return 0;
}
