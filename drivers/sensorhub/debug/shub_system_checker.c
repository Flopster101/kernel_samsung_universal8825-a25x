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

#include <linux/delay.h>
#include <linux/random.h>
#include <linux/slab.h>

#include "../comm/shub_comm.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../sensorhub/shub_device.h"
#include "../utility/shub_utility.h"
#include "shub_system_checker.h"

#ifdef CONFIG_SHUB_DEBUG

#define SSC_DEFAULT_DELAY (500)
#define SSC_DEFAULT_COUNT (50)

struct sensor_rate {
	int32_t sampling_ms;
	int32_t report_ms;
};

struct system_env_backup {
	uint64_t pre_sensor_state;
	struct sensor_rate pre_rate[SENSOR_TYPE_LEGACY_MAX];
	int32_t pre_enabled_count[SENSOR_TYPE_LEGACY_MAX];
};

struct sensor_test_factor {
	int32_t comm_test_response_cnt;
	int32_t event_test_response_cnt;
	int32_t order_test_response_cnt;

	int32_t order_test_fail_cnt;
	int32_t min_delay;

	uint64_t expected_delay_ns;
	uint64_t pre_event_timestamp;
	uint64_t order_test_reversed_timestamp;
	uint64_t standard_deviation;
	uint64_t min_diff;
	uint64_t max_diff;
};

struct system_check_data {
	bool is_system_checking;
	int32_t is_event_order_checking;
	struct mutex system_test_mutex;
	struct system_env_backup env_backup;
	struct sensor_test_factor sensor[SENSOR_TYPE_LEGACY_MAX];
};

static struct system_check_data ssc;
static uint32_t ssc_delay_us;
static uint32_t ssc_count;

static inline void register_system_test_cb(void)
{
	mutex_lock(&ssc.system_test_mutex);
	ssc.is_system_checking = true;
	mutex_unlock(&ssc.system_test_mutex);
}

static inline void unregister_system_test_cb(void)
{
	mutex_lock(&ssc.system_test_mutex);
	ssc.is_system_checking = false;
	mutex_unlock(&ssc.system_test_mutex);
}

static inline void register_event_order_test_cb(void)
{
	mutex_lock(&ssc.system_test_mutex);
	ssc.is_event_order_checking = true;
	mutex_unlock(&ssc.system_test_mutex);
}

static inline void unregister_event_order_test_cb(void)
{
	mutex_lock(&ssc.system_test_mutex);
	ssc.is_event_order_checking = false;
	mutex_unlock(&ssc.system_test_mutex);
}

static void flush_all_sensors(void)
{
	int32_t type = 0;

	for (type = SENSOR_TYPE_ACCELEROMETER; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		if (!get_sensor_probe_state(type))
			continue;
		shub_send_command(CMD_GETVALUE, type, SENSOR_FLUSH, NULL, 0);
		usleep_range(ssc_delay_us, ssc_delay_us * 2);
	}
}

static void run_comm_test(void)
{
	int32_t i;

	shub_infof("[SSC] run comm test...");
	register_system_test_cb();

	for (i = 0; i < ssc_count; i++)
		flush_all_sensors();

	msleep(100);
	unregister_system_test_cb();
}

static void set_sensors_sampling_rate(void)
{
	int32_t type = 0, rate  = 0;
	struct shub_sensor *sensor = NULL;

	for (type = SENSOR_TYPE_ACCELEROMETER; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		sensor = get_sensor(type);
		if (!sensor)
			continue;

		rate = (int32_t)(ssc.sensor[type].expected_delay_ns / 1000000);
		sensor->sampling_period = sensor->max_report_latency = ssc.sensor[type].min_delay = rate;
	}
}

static void run_event_test(void)
{
	int32_t type = 0;

	shub_infof("[SSC] run event test...");
	set_sensors_sampling_rate();
	for (type = SENSOR_TYPE_ACCELEROMETER; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		if (!get_sensor_probe_state(type))
			continue;

		ssc.sensor[type].min_diff = 0xffffffffffffffff;
		ssc.sensor[type].max_diff = 0;
		enable_sensor(type, NULL, 0);
		usleep_range(500, 1000);
	}

	msleep(100);
	register_system_test_cb();
	msleep(1000);

	for (type = SENSOR_TYPE_ACCELEROMETER; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		if (!get_sensor_probe_state(type))
			continue;

		disable_sensor(type, NULL, 0);
		usleep_range(500, 1000);
	}
	unregister_system_test_cb();
}

int32_t order_test_types[3] = {SENSOR_TYPE_ACCELEROMETER, SENSOR_TYPE_GEOMAGNETIC_FIELD, SENSOR_TYPE_GYROSCOPE};

static uint32_t get_random(void)
{
	uint32_t rand;

	get_random_bytes(&rand, sizeof(uint32_t));

	return rand;
}

static void enable_order_test_sensors(void)
{
	int32_t type;
	uint64_t i;

	for (i = 0 ; i < ARRAY_SIZE(order_test_types) ; i++) {
		type = order_test_types[i];

		batch_sensor(type, ssc.sensor[type].min_delay, 0);
		enable_sensor(type, NULL, 0);
		usleep_range(500, 1000);
	}
}

static void disable_order_test_sensors(void)
{
	int32_t type;
	uint64_t i;

	for (i = 0 ; i < ARRAY_SIZE(order_test_types) ; i++) {
		type = order_test_types[i];
		disable_sensor(type, NULL, 0);
		usleep_range(500, 1000);
	}
}

static void run_event_order_test(void)
{
	uint64_t i;
	int32_t type;
	int32_t sampling_rate = 5;
	int32_t report_latency = 0;

	shub_infof("[SSC] run event order test...");

	for (i = 0 ; i < ARRAY_SIZE(order_test_types) ; i++) {
		type = order_test_types[i];
		ssc.sensor[type].order_test_response_cnt = 0;
		ssc.sensor[type].order_test_fail_cnt = 0;
		ssc.sensor[type].order_test_reversed_timestamp = 0;
	}

	register_event_order_test_cb();
	shub_infof("[SSC] run only once #sampling rate : fastest, report latency : random");

	enable_order_test_sensors();
	msleep(2000);

	for (i = 0 ; i < 100 ; i++) {
		type = order_test_types[get_random() % 3];

		sampling_rate = ssc.sensor[type].min_delay;
		report_latency = (get_random() % 2) * 5000;

		batch_sensor(type, sampling_rate, report_latency);
		flush_sensor(type);
		msleep(10 * (get_random() % 15));
	}

	disable_order_test_sensors();

	shub_infof("[SSC] run only once #sampling rate : random, report latency : random");

	enable_order_test_sensors();

	for (i = 0 ; i < 100 ; i++) {
		type = order_test_types[get_random() % 3];

		sampling_rate = get_random() % 10;
		if (sampling_rate == 0)
			sampling_rate = ssc.sensor[type].min_delay;
		else
			sampling_rate = ssc.sensor[type].min_delay * sampling_rate;

		report_latency = (get_random() % 6) * 1000;
		batch_sensor(type, sampling_rate, report_latency);
		flush_sensor(type);
		msleep(10 * (get_random() % 15));
	}

	disable_order_test_sensors();

	unregister_event_order_test_cb();
}

static void sensor_on_off(void)
{
	int32_t type = 0;

	for (type = SENSOR_TYPE_ACCELEROMETER; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		if (!get_sensor_probe_state(type))
			continue;

		enable_sensor(type, NULL, 0);
		usleep_range(500, 1000);
		disable_sensor(type, NULL, 0);
		usleep_range(500, 1000);
	}
}

static void run_system_test(void)
{
	int32_t count = 0;

	shub_infof("[SSC] run system test...");
	shub_send_command(CMD_SETVALUE, TYPE_HUB, HUB_SYSTEM_CHECK, NULL, 0);
	usleep_range(500, 1000);

	for (count = 0 ; count < ssc_count; count++)
		sensor_on_off();
}

void comm_test_cb(int32_t type)
{
	ssc.sensor[type].comm_test_response_cnt++;
}

void event_test_cb(int32_t type, uint64_t timestamp)
{
	uint64_t temp = 0;

	if (type >= SENSOR_TYPE_LEGACY_MAX || type < SENSOR_TYPE_ACCELEROMETER)
		return;

	if (!ssc.sensor[type].pre_event_timestamp) {
		ssc.sensor[type].pre_event_timestamp = timestamp;
		return;
	}

	temp = timestamp - ssc.sensor[type].pre_event_timestamp;
	temp = temp > ssc.sensor[type].expected_delay_ns ?
		temp - ssc.sensor[type].expected_delay_ns : ssc.sensor[type].expected_delay_ns - temp;

	ssc.sensor[type].min_diff = temp < ssc.sensor[type].min_diff ? temp : ssc.sensor[type].min_diff;
	ssc.sensor[type].max_diff = temp > ssc.sensor[type].max_diff ? temp : ssc.sensor[type].max_diff;
	ssc.sensor[type].standard_deviation += (temp * temp);
	ssc.sensor[type].event_test_response_cnt++;
	ssc.sensor[type].pre_event_timestamp = timestamp;
}

void order_test_cb(int32_t type, uint64_t timestamp)
{
	if (!(type == SENSOR_TYPE_ACCELEROMETER || type == SENSOR_TYPE_GYROSCOPE ||
	      type == SENSOR_TYPE_GEOMAGNETIC_FIELD))
		return;

	if (!ssc.sensor[type].pre_event_timestamp || !timestamp) {
		ssc.sensor[type].pre_event_timestamp = timestamp;
		return;
	}

	if (ssc.sensor[type].pre_event_timestamp >= timestamp) {
		ssc.sensor[type].order_test_reversed_timestamp = timestamp;
		ssc.sensor[type].order_test_fail_cnt++;

		shub_errf("[SSC] reversed type : %d, prev : %lld, curr : %lld",
		    type, ssc.sensor[type].pre_event_timestamp, timestamp);
	}

	ssc.sensor[type].order_test_response_cnt++;

	//shub_infof("[SSC] type : %d, prev : %lld, curr : %lld",
	//	    type, ssc.sensor[type].pre_event_timestamp, timestamp);

	ssc.sensor[type].pre_event_timestamp = timestamp;
}

static void restore_test_env(void)
{
	int32_t type = 0;
	struct shub_sensor *sensor;

	shub_infof("");
	for (type = SENSOR_TYPE_ACCELEROMETER; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		sensor = get_sensor(type);
		if (sensor) {
			mutex_lock(&sensor->enabled_mutex);
			sensor->enabled = (ssc.env_backup.pre_sensor_state & (1ULL << type));
			sensor->enabled_cnt = ssc.env_backup.pre_enabled_count[type];
			mutex_unlock(&sensor->enabled_mutex);

			sensor->sampling_period = ssc.env_backup.pre_rate[type].sampling_ms;
			sensor->max_report_latency = ssc.env_backup.pre_rate[type].report_ms;
		}
	}
}

static uint64_t test_sqrt(uint64_t in)
{
	int32_t count = 30;
	uint64_t out = 1;

	while (count-- > 0)
		out = (out + in / out) / 2;

	return out;
}

static void report_test_result(void)
{
	enum system_test_op {
		OPERATION_OPEN = 0,
		OPERATION_READ,
		OPERATION_CLOSE,
		OPERATION_MAX,
	};

	struct sensor_debug_factor {
		int32_t count;
		uint64_t tt_sum;
		uint64_t tt_max;
		uint64_t tt_min;
	} __attribute__((__packed__));

	struct system_test {
		uint8_t type;
		struct sensor_debug_factor op[OPERATION_MAX];
	} __attribute__((__packed__));

	int32_t ret = 0;
	int32_t i = 0;
	int32_t type = 0;
	int32_t count = 0;
	int32_t index = 0;
	int32_t buffer_length = 0;
	int8_t *buffer = NULL;

	shub_infof("[SSC] == comm test result ======");
	for (type = SENSOR_TYPE_ACCELEROMETER; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		if (!get_sensor_probe_state(type))
			continue;

		shub_infof("[SSC] sensor(%2d) : %d", type, ssc.sensor[type].comm_test_response_cnt);
		if (ssc.sensor[type].comm_test_response_cnt != ssc_count)
			shub_infof("sensor(%d) failed!!", type);
	}

	shub_infof("[SSC] == event test result ======");
	for (type = SENSOR_TYPE_ACCELEROMETER; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		if (!get_sensor_probe_state(type))
			continue;

		shub_infof("[SSC] sensor(%2d) : cnt(%4d) std dev(%6lld), min(%6lld), max(%6lld)",
				type,
				ssc.sensor[type].event_test_response_cnt,
				test_sqrt(
					ssc.sensor[type].standard_deviation / ssc.sensor[type].event_test_response_cnt),
					ssc.sensor[type].min_diff,
					ssc.sensor[type].max_diff);
	}

	shub_infof("[SSC] == system test result ======");
	ret = shub_send_command_wait(CMD_GETVALUE, TYPE_HUB, HUB_SYSTEM_CHECK,
			       1000, NULL, 0, (char **)&buffer, &buffer_length, true);
	if (ret < 0 || !buffer || buffer_length == 0) {
		shub_infof("[SSC] ERROR! Fail to get system test result!!");
		return;
	}
	count = buffer[0];
	index++;
	for (type = 0; type < count; type++) {
		struct system_test *result = (struct system_test *)(buffer + index);

		index += sizeof(struct system_test);

		shub_infof("[SSC] sensor(%2d)", result->type);
		shub_infof("[SSC]    OPEN  - cnt(%4d) min(%7lld), max(%7lld), avg(%7lld)",
				result->op[OPERATION_OPEN].count,
				result->op[OPERATION_OPEN].tt_min,
				result->op[OPERATION_OPEN].tt_max,
				result->op[OPERATION_OPEN].tt_sum / result->op[OPERATION_OPEN].count);

		shub_infof("[SSC]    READ  - cnt(%4d) min(%7lld), max(%7lld), avg(%7lld)",
				result->op[OPERATION_READ].count,
				result->op[OPERATION_READ].tt_min,
				result->op[OPERATION_READ].tt_max,
				result->op[OPERATION_READ].tt_sum / result->op[OPERATION_READ].count);

		shub_infof("[SSC]    CLOSE - cnt(%4d) min(%7lld), max(%7lld), avg(%7lld)",
				result->op[OPERATION_CLOSE].count,
				result->op[OPERATION_CLOSE].tt_min,
				result->op[OPERATION_CLOSE].tt_max,
				result->op[OPERATION_CLOSE].tt_sum / result->op[OPERATION_CLOSE].count);
	}

	shub_infof("[SSC] == event order test result ======");
	for (i = 0 ; i < ARRAY_SIZE(order_test_types); i++) {
		type = order_test_types[i];
		shub_infof("[SSC] sensor(%2d)", type);
		shub_infof("[SSC]    ORDER  - cnt(%4d) fail cnt(%4d), last reversed timestamp(%7lld)",
			    ssc.sensor[type].order_test_response_cnt, ssc.sensor[type].order_test_fail_cnt,
			    ssc.sensor[type].order_test_reversed_timestamp);
	}

	kfree(buffer);
}

void run_system_check(void)
{
	static int32_t test_cnt = 1;

	shub_infof("[SSC] !! #%d test begin !!!!!!!!!!!!!!!!!!", test_cnt++);
	shub_infof("[SSC] !! DELAY(%dusec) COUNT(%d)", ssc_delay_us, ssc_count);

	run_system_test();
	run_comm_test();
	run_event_test();
	run_event_order_test();

	usleep_range(500, 1000);

	restore_test_env();
	report_test_result();
	reset_mcu(RESET_TYPE_KERNEL_SYSFS);
}

void system_ready_cb(void)
{
	unregister_system_test_cb();
	unregister_event_order_test_cb();
	run_system_check();
}

static void prepare_test_env(void)
{
	int32_t type = 0;
	struct shub_sensor *sensor;

	shub_infof("");
	// init check_system data
	memset(&ssc, 0, sizeof(ssc));

	// backup shub env
	ssc.env_backup.pre_sensor_state = get_sensors_legacy_enable_state();
	for (type = SENSOR_TYPE_ACCELEROMETER; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		sensor = get_sensor(type);
		if (sensor) {
			ssc.env_backup.pre_enabled_count[type] = sensor->enabled_cnt;
			mutex_lock(&sensor->enabled_mutex);
			sensor->enabled = false;
			sensor->enabled_cnt = 0;
			mutex_unlock(&sensor->enabled_mutex);

			ssc.env_backup.pre_rate[type].sampling_ms = sensor->sampling_period;
			ssc.env_backup.pre_rate[type].report_ms = sensor->max_report_latency;
			ssc.sensor[type].expected_delay_ns = sensor->spec.min_delay * 1000;
		}
	}
}

void sensorhub_system_check(uint32_t test_delay_us, uint32_t test_count)
{
	ssc_delay_us = test_delay_us && test_delay_us < 5000 ? test_delay_us : SSC_DEFAULT_DELAY;
	ssc_count = test_count && test_count < 100 ? test_count : SSC_DEFAULT_COUNT;

	prepare_test_env();
	register_system_test_cb();
	reset_mcu(RESET_TYPE_KERNEL_SYSFS);
}

void shub_system_checker_init(void)
{
	mutex_init(&ssc.system_test_mutex);
}

bool is_system_checking(void)
{
	return ssc.is_system_checking;
}

bool is_event_order_checking(void)
{
	return ssc.is_event_order_checking;
}

void shub_system_check_lock(void)
{
	mutex_lock(&ssc.system_test_mutex);
}

void shub_system_check_unlock(void)
{
	mutex_unlock(&ssc.system_test_mutex);
}

#endif
