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

#ifndef __SENSOR_MANAGER_H_
#define __SENSOR_MANAGER_H_

#include "shub_sensor_type.h"

#include <linux/kernel.h>
#include <linux/miscdevice.h>

struct shub_sensor;

struct sensor_manager_t {
	struct shub_sensor *sensor_list[SENSOR_TYPE_MAX];
	uint64_t sensor_probe_state[2];
	uint64_t scontext_probe_state[2];
	struct sensor_spec_t *sensor_spec;
	bool is_fs_ready;
};

int init_sensor_manager(struct device *dev);
void exit_sensor_manager(struct device *dev);

int enable_sensor(int type, char *buf, int buf_len);
int disable_sensor(int type, char *buf, int buf_len);
int batch_sensor(int type, uint32_t sampling_period, uint32_t max_report_latency);
int flush_sensor(int type);
int inject_sensor_additional_data(int type, char *buf, int buf_len);

void print_sensor_debug(int type);

int parsing_bypass_data(char *dataframe, int *index, int frame_len);
int parsing_meta_data(char *dataframe, int *index, int frame_len);
int parsing_scontext_data(char *dataframe, int *index, int frame_len);
int parsing_big_data(char *dataframe, int *index, int frame_len);

int open_sensors_calibration(void);
int sync_sensors_attribute(void); /* sensor hub is ready or reset*/
int refresh_sensors(struct device *dev);

struct shub_sensor *get_sensor(int type);
struct sensor_event *get_sensor_event(int type);

int get_sensors_legacy_probe_state(uint64_t *buf);
int get_sensors_legacy_enable_state(uint64_t *buf);
int get_sensors_scontext_probe_state(uint64_t *buf);

bool get_sensor_probe_state(int type);
bool get_sensor_enabled(int type);
unsigned int get_total_sensor_spec(char *buf);
unsigned int get_bigdata_wakeup_reason(char *buf);

void fs_ready_cb(void);

void get_sensor_vendor_name(int vendor_type, char *vendor_name);

void print_big_data(void);
#endif /* __SENSOR_MANAGER_H_ */
