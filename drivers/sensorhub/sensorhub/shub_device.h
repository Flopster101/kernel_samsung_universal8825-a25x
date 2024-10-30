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

#ifndef __SHUB_DEVICE_H_
#define __SHUB_DEVICE_H_

#include "../utility/shub_wait_event.h"

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/rtc.h>

#define HUB_RESET_REQ_NO_EVENT		0x1a
#define HUB_RESET_REQ_TASK_FAILURE	0x1b
#define MINI_DUMP_LENGTH		512
#define MODEL_NAME_MAX 10

enum {
	RESET_TYPE_HUB_CRASHED = 0,
	RESET_TYPE_KERNEL_SYSFS,
	RESET_TYPE_KERNEL_NO_EVENT,
	RESET_TYPE_KERNEL_COM_FAIL,
	RESET_TYPE_HUB_NO_EVENT,
	RESET_TYPE_HUB_REQ_TASK_FAILURE,
	RESET_TYPE_MAX,
};

struct reset_info_t {
	u64 timestamp;
	struct rtc_time time;
	int reason;
};

enum {
	SF_BASE = 0,
	SF_DDI_SUPPORT = 1,
	SF_ACCEL_16G = 2,
	SF_DEBUG_V2 = 3,
	SF_PROBE_V2 = 4,
	SF_MAX,
};

struct shub_system_info {
	uint32_t fw_version;
	uint64_t scan_sensor_probe[2];
	uint64_t scan_scontext_probe[2];
	uint32_t system_feature;
	uint32_t reserved_1;
	uint32_t reserved_2;
} __attribute__((__packed__));

struct shub_data_t {
	struct platform_device *pdev;
	struct device *sysfs_dev;

	bool is_probe_done;
	bool is_working;

	int cnt_reset;
	int reset_type;
	unsigned int cnt_shub_reset[RESET_TYPE_MAX + 1]; /* index RESET_TYPE_MAX : total reset count */
	struct work_struct work_reset;
	struct shub_waitevent reset_lock;
	struct reset_info_t reset_info;

	u64 hub_crash_timestamp;

	u8 pm_status;
	u8 lcd_status;
	u8 intent_screen_state;
	u8 display_screen_state;

	struct workqueue_struct *shub_wq;

	struct work_struct work_refresh;

	struct timer_list ts_sync_timer;
	struct work_struct work_ts_sync;

	struct shub_system_info system_info;

	struct regulator *sensor_vdd_regulator;

	int sensor_ldo_en;
	int prox_ldo_en;
	char mini_dump[MINI_DUMP_LENGTH];
	char model_name[MODEL_NAME_MAX];
};

#if IS_ENABLED(CONFIG_SENSORS_GRIP_FAILURE_DEBUG)
enum grip_ic_num {
	MAIN_GRIP = 0,
	SUB_GRIP,
	SUB2_GRIP,
	WIFI_GRIP,
	GRIP_MAX_CNT
};
static u32 grip_error[GRIP_MAX_CNT] = {0,};
#endif

struct device *get_shub_device(void);
struct shub_data_t *get_shub_data(void);

void reset_mcu(int reason);
int get_reset_type(void);
void init_reset_type(void);
int get_reset_count(void);
struct reset_info_t get_reset_info(void);

bool is_shub_working(void);
int shub_send_status_with_buffer(u8, char *, int);
int shub_send_status(u8);
int queue_refresh_task(void);

int shub_probe(struct platform_device *pdev);
void shub_shutdown(struct platform_device *pdev);
int shub_suspend(struct device *dev);
int shub_resume(struct device *dev);
int shub_prepare(struct device *dev);
void shub_complete(struct device *dev);

void shub_queue_work(struct work_struct *work);

struct shub_system_info *get_shub_system_info(void);
bool is_support_system_feature(int feature);

int enable_sensor_vdd(void);
int disable_sensor_vdd(void);

void sensorhub_stop(void);
#endif /* __SHUB_DEVICE_H_ */
