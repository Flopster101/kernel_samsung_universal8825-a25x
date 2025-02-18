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

#include "../comm/shub_comm.h"
#include "../comm/shub_iio.h"
#include "../utility/shub_utility.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../sensorhub/shub_device.h"
#include "scontext_cmd.h"

#define BIG_DATA_SIZE       256
#define PRINT_TRUNCATE      6

#define SCONTEXT_NAME_MAX 30

#define SCONTEXT_DATA_LEN   56
#define SCONTEXT_HEADER_LEN 8

#define RESET_REASON_KERNEL_RESET  0x01
#define RESET_REASON_MCU_CRASHED   0x02
#define RESET_REASON_SYSFS_REQUEST 0x03
#define RESET_REASON_HUB_REQUEST   0x04

void shub_scontext_log(const char *func_name, const char *data, int length)
{
	char buf[6];
	char *log_str;
	int log_size, i;

	if (likely(length <= BIG_DATA_SIZE))
		log_size = length;
	else
		log_size = PRINT_TRUNCATE * 2 + 1;

	log_size = sizeof(buf) * log_size + 1;
	log_str = kzalloc(log_size, GFP_ATOMIC);
	if (unlikely(!log_str)) {
		shub_errf("allocate memory for data log err");
		return;
	}

	for (i = 0; i < length; i++) {
		if (length < BIG_DATA_SIZE || i < PRINT_TRUNCATE || i >= length - PRINT_TRUNCATE) {
			snprintf(buf, sizeof(buf), "0x%x", (unsigned char)data[i]);
			strlcat(log_str, buf, log_size);

			if (i < length - 1)
				strlcat(log_str, ", ", log_size);
		}
		if (length > BIG_DATA_SIZE && i == PRINT_TRUNCATE)
			strlcat(log_str, "..., ", log_size);
	}

	shub_info("%s(%d): %s", func_name, length, log_str);
	kfree(log_str);
}

void shub_report_scontext_data(char *data, u32 data_len)
{
	u16 start, end;
	u64 timestamp;
	char buf[SCONTEXT_HEADER_LEN + SCONTEXT_DATA_LEN] = {0,};


	shub_scontext_log(__func__, data, data_len);

	if (data[0] == 0x01 && data[1] == 0x01) {
		int type = data[2] + SENSOR_TYPE_SS_BASE;
		struct shub_sensor *sensor = get_sensor(type);

		if (sensor) {
			sensor->event_buffer.timestamp = get_current_timestamp();
			sensor->event_buffer.received_timestamp = sensor->event_buffer.timestamp;
		}
	}

	start = 0;
	memcpy(buf, &data_len, sizeof(u32));
	timestamp = get_current_timestamp();

	while (start < data_len) {
		if (start + SCONTEXT_DATA_LEN < data_len) {
			end = start + SCONTEXT_DATA_LEN - 1;
		} else {
			memset(buf + SCONTEXT_HEADER_LEN, 0, SCONTEXT_DATA_LEN);
			end = data_len - 1;
		}

		memcpy(buf + sizeof(data_len), &start, sizeof(u16));
		memcpy(buf + sizeof(data_len) + sizeof(start), &end, sizeof(u16));
		memcpy(buf + SCONTEXT_HEADER_LEN, data + start, end - start + 1);

		shub_report_sensordata(SENSOR_TYPE_SCONTEXT, timestamp, buf,
				       get_sensor(SENSOR_TYPE_SCONTEXT)->report_event_size);

		start = end + 1;
	}
}

void shub_report_scontext_notice_data(char notice)
{
	char notice_buf[4] = {0x02, 0x01, 0x00, 0x00};
	int len = 3;

	notice_buf[2] = notice;

	if (notice == SCONTEXT_NOTIFY_HUB_RESET) {
		struct reset_info_t reset_info = get_reset_info();

		len = 4;
		if (reset_info.reason == RESET_TYPE_KERNEL_SYSFS)
			notice_buf[3] = RESET_REASON_SYSFS_REQUEST;
		else if (reset_info.reason == RESET_TYPE_KERNEL_NO_EVENT)
			notice_buf[3] = RESET_REASON_KERNEL_RESET;
		else if (reset_info.reason == RESET_TYPE_KERNEL_COM_FAIL)
			notice_buf[3] = RESET_REASON_KERNEL_RESET;
		else if (reset_info.reason == RESET_TYPE_HUB_CRASHED)
			notice_buf[3] = RESET_REASON_MCU_CRASHED;
		else if (reset_info.reason == RESET_TYPE_HUB_NO_EVENT)
			notice_buf[3] = RESET_REASON_HUB_REQUEST;
	}

	shub_report_scontext_data(notice_buf, len);

	if (notice == SCONTEXT_NOTIFY_HUB_RESET)
		shub_infof("reset");
	else
		shub_errf("invalid notice(0x%x)", notice);

}

int convert_ap_status(unsigned char command)
{
	int ret = -1;

	switch (command) {
	case SCONTEXT_VALUE_SCREEN_STATE:
		ret = SCREEN_STATE;
		break;
	case SCONTEXT_AP_STATUS_LCD_ON:
		ret = LCD_ON;
		break;
	case SCONTEXT_AP_STATUS_LCD_OFF:
		ret = LCD_OFF;
		break;
	case SCONTEXT_AP_STATUS_POW_CONNECTED:
		ret = POW_CONNECTED;
		break;
	case SCONTEXT_AP_STATUS_POW_DISCONNECTED:
		ret = POW_DISCONNECTED;
		break;
	case SCONTEXT_AP_STATUS_CALL_IDLE:
		ret = CALL_IDLE;
		break;
	case SCONTEXT_AP_STATUS_CALL_ACTIVE:
		ret = CALL_ACTIVE;
		break;
	}

	return ret;
}

int shub_scontext_send_cmd(const unsigned char *buf, int count)
{
	int ret = 0;
	int convert_status = 0;
	struct shub_data_t *shub_data = get_shub_data();

	convert_status = convert_ap_status(buf[2]);
	if (convert_status < 0) {
		shub_errf("INST_LIB_NOTI err(%d)", buf[2]);
		return -EINVAL;
	}

	ret = shub_send_status(convert_status);

	if (buf[2] == SCONTEXT_AP_STATUS_LCD_ON || buf[2] == SCONTEXT_AP_STATUS_LCD_OFF)
		shub_data->lcd_status = convert_status;

	return ret;
}

#define SCONTEXT_VALUE_CURRENTSYSTEMTIME	      0x0E
#define SCONTEXT_VALUE_PEDOMETER_USERHEIGHT	   0x12
#define SCONTEXT_VALUE_PEDOMETER_USERWEIGHT	   0x13
#define SCONTEXT_VALUE_PEDOMETER_USERGENDER	   0x14
#define SCONTEXT_VALUE_PEDOMETER_INFOUPDATETIME       0x15
#define SCONTEXT_VLAUE_DISPLAY_STATE			0x47

int convert_scontext_putvalue_subcmd(int subcmd)
{
	int ret = -1;

	switch (subcmd) {
	case SCONTEXT_VLAUE_DISPLAY_STATE:
		ret = SCREEN_STATE;
		break;
	case SCONTEXT_VALUE_CURRENTSYSTEMTIME:
		ret = CURRENT_SYSTEM_TIME;
		break;
	case SCONTEXT_VALUE_PEDOMETER_USERHEIGHT:
		ret = PEDOMETER_USERHEIGHT;
		break;
	case SCONTEXT_VALUE_PEDOMETER_USERWEIGHT:
		ret = PEDOMETER_USERWEIGHT;
		break;
	case SCONTEXT_VALUE_PEDOMETER_USERGENDER:
		ret = PEDOMETER_USERGENDER;
		break;
	case SCONTEXT_VALUE_PEDOMETER_INFOUPDATETIME:
		ret = PEDOMETER_INFOUPDATETIME;
		break;
	default:
		ret = subcmd;
	}

	return ret;
}

int convert_scontext_getvalue_subcmd(int subcmd)
{
	int ret = -1;

	switch (subcmd) {
	case SCONTEXT_VALUE_CURRENTSTATUS:
		ret = LIBRARY_CURRENTSTATUS;
		break;
	case SCONTEXT_VALUE_CURRENTSTATUS_BATCH:
		ret = LIBRARY_CURRENTSTATUS_BATCH;
		break;
	case SCONTEXT_VALUE_VERSIONINFO:
		ret = LIBRARY_VERSIONINFO;
		break;
	default:
		ret = subcmd;
	}

	return ret;
}

void get_ss_sensor_name(int type, char *buf, int buf_size)
{
	memset(buf, 0, buf_size);
	switch (type) {
	case SENSOR_TYPE_SS_PEDOMETER:
		strncpy(buf, "pedometer", buf_size);
		break;
	case SENSOR_TYPE_SS_STEP_COUNT_ALERT:
		strncpy(buf, "step count alert", buf_size);
		break;
	case SENSOR_TYPE_SS_AUTO_ROTATION:
		strncpy(buf, "auto rotation", buf_size);
		break;
	case SENSOR_TYPE_SS_SLOCATION:
		strncpy(buf, "slocation", buf_size);
		break;
	case SENSOR_TYPE_SS_MOVEMENT:
		strncpy(buf, "smart alert", buf_size);
		break;
	case SENSOR_TYPE_SS_ACTIVITY_TRACKER:
		strncpy(buf, "activity tracker", buf_size);
		break;
	case SENSOR_TYPE_SS_DPCM:
		strncpy(buf, "dpcm", buf_size);
		break;
	case SENSOR_TYPE_SS_SENSOR_STATUS_CHECK:
		strncpy(buf, "sensor status check", buf_size);
		break;
	case SENSOR_TYPE_SS_ACTIVITY_CALIBRATION:
		strncpy(buf, "activity calibration", buf_size);
		break;
	case SENSOR_TYPE_SS_DEVICE_POSITION:
		strncpy(buf, "device position", buf_size);
		break;
	case SENSOR_TYPE_SS_CHANGE_LOCATION_TRIGGER:
		strncpy(buf, "change location trigger", buf_size);
		break;
	case SENSOR_TYPE_SS_FREE_FALL_DETECTION:
		strncpy(buf, "free fall detection", buf_size);
		break;
	}
}

int shub_scontext_send_instruction(const unsigned char *buf, int count)
{
	char cmd, type, sub_cmd = 0;
	unsigned char *buffer = (unsigned char *)(buf + 2);
	int length = count - 2;
	char name[SCONTEXT_NAME_MAX] = "";

	if (buf[0] == SCONTEXT_INST_LIBRARY_REMOVE) {
		cmd = CMD_REMOVE;
		type = buf[1] + SENSOR_TYPE_SS_BASE;
		if (type < SENSOR_TYPE_SS_MAX) {
			get_ss_sensor_name(type, name, sizeof(name));
			shub_infof("REMOVE LIB %s, type %d", name, type);
			return disable_sensor(type, buffer, length);
		} else {
			return -EINVAL;
		}
	} else if (buf[0] == SCONTEXT_INST_LIBRARY_ADD) {
		cmd = CMD_ADD;
		type = buf[1] + SENSOR_TYPE_SS_BASE;
		if (type < SENSOR_TYPE_SS_MAX) {
			get_ss_sensor_name(type, name, sizeof(name));
			shub_infof("ADD LIB %s, type %d", name, type);
			return enable_sensor(type, buffer, length);
		} else {
			return -EINVAL;
		}
	} else if (buf[0] == SCONTEXT_INST_LIB_SET_DATA) {
		cmd = CMD_SETVALUE;
		if (buf[1] != SCONTEXT_VALUE_LIBRARY_DATA) {
			type = TYPE_HUB;
			sub_cmd = convert_scontext_putvalue_subcmd(buf[1]);
			if (sub_cmd == SCREEN_STATE) {
				struct shub_data_t *shub_data = get_shub_data();

				if (buffer[1] == 0xf1 || buffer[1] == 0xf2)
					shub_data->intent_screen_state = buffer[1];
				else if (buffer[1] == 0x00 || buffer[1] == 0x01)
					shub_data->display_screen_state = buffer[1];
			}
		} else {
			type = buf[2] + SENSOR_TYPE_SS_BASE;
			sub_cmd = LIBRARY_DATA;
			length = count - 3;
			if (length > 0)
				buffer = (char *)(buf + 3);
			else
				buffer = NULL;
		}
	} else if (buf[0] == SCONTEXT_INST_LIB_GET_DATA) {
		cmd = CMD_GETVALUE;
		type = buf[1] + SENSOR_TYPE_SS_BASE;
		sub_cmd =  convert_scontext_getvalue_subcmd(buf[2]);
		length = count - 3;
		if (length > 0)
			buffer = (char *)(buf + 3);
		else
			buffer = NULL;
	} else {
		shub_errf("0x%x is not supported", buf[0]);
		return -EINVAL;
	}

	return shub_send_command(cmd, type, sub_cmd, buffer, length);
}

void init_scontext_enable_state(void)
{
	int type;

	for (type = SENSOR_TYPE_SS_BASE; type < SENSOR_TYPE_SS_MAX; type++) {
		struct shub_sensor *sensor = get_sensor(type);

		if (sensor) {
			mutex_lock(&sensor->enabled_mutex);
			sensor->enabled_cnt = 0;
			sensor->enabled = false;
			mutex_unlock(&sensor->enabled_mutex);
		}
	}
}

void disable_scontext_all(void)
{
	int type;
	char buf[2];

	for (type = SENSOR_TYPE_SS_BASE; type < SENSOR_TYPE_SS_MAX; type++) {
		struct shub_sensor *sensor = get_sensor(type);

		if (sensor && sensor->enabled) {
			/*
			 * enabled_cnt may be 1 or more. ref) EXTRA_EXTERNAL_CLIENT_REMOVED
			 * if count > 1, buffer should not be null. So, set enabled_cnt to 1 intentionally.
			 */
			mutex_lock(&sensor->enabled_mutex);
			sensor->enabled_cnt = 1;
			mutex_unlock(&sensor->enabled_mutex);
			disable_sensor(type, buf, sizeof(buf));
		}
	}
}

void print_scontext_debug(void)
{
	/* print nothing for debug */
}

static struct sensor_funcs scontext_sensor_funcs = {
	.print_debug = print_scontext_debug,
};

int init_scontext(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_SCONTEXT);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "scontext_iio", 0, 64, 64);
		sensor->funcs = &scontext_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
