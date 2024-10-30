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

#include "../debug/shub_debug.h"
#include "../sensor/scontext.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"
#include "../vendor/shub_vendor.h"
#include "shub_comm.h"
#include "shub_cmd.h"

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#if defined(CONFIG_SHUB_KUNIT)
#include <kunit/mock.h>
#define __mockable __weak
#define __visible_for_testing
#else
#define __mockable
#define __visible_for_testing static
#endif

#define SHUB2AP_BYPASS_DATA	0x37
#define SHUB2AP_LIBRARY_DATA	0x01
#define SHUB2AP_DEBUG_DATA	0x03
#define SHUB2AP_META_DATA	0x05
#define SHUB2AP_GYRO_CAL	0x08
#define SHUB2AP_PROX_THRESH	0x09
#define SHUB2AP_HUB_REQ		0x0A
#define SHUB2AP_MAG_CAL		0x0B
#define SHUB2AP_LOG_DUMP	0x12
#define SHUB2AP_SYSTEM_INFO	0x31
#define SHUB2AP_BIG_DATA	0x51

struct mutex comm_mutex;
struct mutex pending_mutex;
struct mutex rx_msg_mutex;
struct list_head pending_list;

unsigned int cnt_timeout;
unsigned int cnt_comm_fail;

char shub_cmd_data[SHUB_CMD_SIZE];
struct shub_msg rx_msg;

static struct shub_msg *make_msg(u8 cmd, u8 type, u8 subcmd, char *send_buf, int send_buf_len)
{
	struct shub_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);

	if (!msg) {
		shub_errf("kzalloc error");
		return NULL;
	}

	msg->cmd = cmd;
	msg->type = type;
	msg->subcmd = subcmd;
	msg->length = send_buf_len;
	msg->timestamp = get_current_timestamp();

	if (send_buf != NULL && send_buf_len != 0) {
		msg->buffer = kzalloc(send_buf_len, GFP_KERNEL);
		if (!msg->buffer) {
			kfree(msg);
			msg = NULL;
			shub_errf("kzalloc error");
			return NULL;
		}
		memcpy(msg->buffer, send_buf, send_buf_len);
	}

	return msg;
}

static void clean_msg(struct shub_msg *msg, bool buf_free)
{
	if (buf_free)
		kfree(msg->buffer);
	kfree(msg);
}

static void delete_list(struct shub_msg *msg)
{
	mutex_lock(&pending_mutex);
	if ((msg->list.next != NULL) && (msg->list.next != LIST_POISON1))
		list_del(&msg->list);
	mutex_unlock(&pending_mutex);
}

static int comm_to_sensorhub(struct shub_msg *msg)
{
	int ret;

	if (!is_shub_working()) {
		shub_errf("sensorhub is not working");
		return -EIO;
	}

	mutex_lock(&comm_mutex);
	memcpy(shub_cmd_data, msg, SHUB_MSG_HEADER_SIZE);

	if (msg->length > (SHUB_CMD_SIZE - SHUB_MSG_HEADER_SIZE)) {
		shub_errf("command size(%d) is over.", msg->length);
		mutex_unlock(&comm_mutex);
		return -EINVAL;
	} else if (msg->length > 0) {
		memcpy(&shub_cmd_data[SHUB_MSG_HEADER_SIZE], msg->buffer, msg->length);
	}

	shub_infof("cmd %d type %d subcmd %d send_buf_len %d ts %llu", msg->cmd, msg->type, msg->subcmd, msg->length,
		   msg->timestamp);

	ret = sensorhub_comms_write(shub_cmd_data, SHUB_CMD_SIZE);
	mutex_unlock(&comm_mutex);

	if (ret < 0) {
		bool is_shub_shutdown = !is_shub_working();

		cnt_comm_fail += (is_shub_shutdown) ? 0 : 1;
		shub_errf("comm write FAILED. cnt_comm_fail %d , shub_down %d ", cnt_comm_fail, is_shub_shutdown);

#ifndef CONFIG_SHUB_MTK
		reset_mcu(RESET_TYPE_KERNEL_COM_FAIL);
#endif
	}

	return ret;
}

int __mockable shub_send_command(u8 cmd, u8 type, u8 subcmd, char *send_buf, int send_buf_len)
{
	int ret = 0;
	struct shub_msg *msg = make_msg(cmd, type, subcmd, send_buf, send_buf_len);

	if (msg == NULL)
		return -EINVAL;

	ret = comm_to_sensorhub(msg);
	if (ret < 0)
		shub_errf("comm_to_sensorhub FAILED.");

	clean_msg(msg, true);

	return ret;
}

int __mockable shub_send_command_wait(u8 cmd, u8 type, u8 subcmd, int timeout, char *send_buf, int send_buf_len,
			   char **receive_buf, int *receive_buf_len, bool reset)
{
	int ret = 0;
	DECLARE_COMPLETION_ONSTACK(done);
	struct shub_msg *msg;

	if (cmd != CMD_GETVALUE) {
		shub_errf("invalid command %d", cmd);
		return -EINVAL;
	}

	if (receive_buf == NULL || receive_buf_len == NULL)
		return -EINVAL;

	msg = make_msg(cmd, type, subcmd, send_buf, send_buf_len);
	if (msg == NULL)
		return -EINVAL;

	msg->done = &done;

	mutex_lock(&pending_mutex);
	list_add_tail(&msg->list, &pending_list);
	mutex_unlock(&pending_mutex);

	ret = comm_to_sensorhub(msg);
	if (ret < 0) {
		shub_errf("comm_to_sensorhub FAILED.");
		delete_list(msg);
		goto exit;
	}

	ret = wait_for_completion_timeout(msg->done, msecs_to_jiffies(timeout));

	if (msg->is_empty_pending_list) {
		shub_errf("pending list is empty");
		msg->is_empty_pending_list = false;
		goto exit;
	}
	/* when timeout happen */
	if (!ret) {
		bool is_shub_shutdown = !is_shub_working();

		msg->done = NULL;
		delete_list(msg);
		cnt_timeout += (is_shub_shutdown) ? 0 : 1;

		shub_errf("timeout(%d %d %d). cnt_timeout %d, shub_down %d", cmd, type, subcmd, cnt_timeout,
			  is_shub_shutdown);
		ret = -EIO;
#ifndef CONFIG_SHUB_MTK
		if (reset)
			reset_mcu(RESET_TYPE_KERNEL_COM_FAIL);
#endif
	} else {
		if (msg->length != 0) {
			*receive_buf = msg->buffer;
			*receive_buf_len = msg->length;
		} else {
			ret = -EINVAL;
		}
	}

exit:
	clean_msg(msg, false);
	return ret;
}

void clean_pending_list(void)
{
	struct shub_msg *msg, *n;

	shub_infof("");

	mutex_lock(&pending_mutex);
	list_for_each_entry_safe(msg, n, &pending_list, list) {
		list_del(&msg->list);
		if (msg->done != NULL && !completion_done(msg->done)) {
			msg->is_empty_pending_list = 1;
			complete(msg->done);
		}
	}
	mutex_unlock(&pending_mutex);
}

static int parsing_hub_request(int *index, char *dataframe)
{
	int ret = 0;
	int req_type = dataframe[(*index)++];

	if (req_type == HUB_RESET_REQ_NO_EVENT) {
		int no_event_type = dataframe[(*index)++];
		shub_infof("Hub request reset[0x%x] No Event type %d", req_type, no_event_type);
		reset_mcu(RESET_TYPE_HUB_NO_EVENT);
	} else if (req_type == HUB_RESET_REQ_TASK_FAILURE) {
		shub_infof("Hub request reset[0x%x] request task failure", req_type);
		reset_mcu(RESET_TYPE_HUB_REQ_TASK_FAILURE);
	} else {
		shub_infof("Hub [0x%x] invalid request", req_type);
		ret = -EINVAL;
	}
	return ret;
}

static int parse_dataframe(char *dataframe, int frame_len)
{
	int index;
	int ret = 0;
	struct shub_sensor *sensor;

	if (!is_shub_working()) {
		shub_infof("ssp shutdown, do not parse");
		return 0;
	}

	// print_dataframe(data, dataframe, frame_len);

	for (index = 0; index < frame_len && (ret == 0);) {
		int cmd = dataframe[index++];
		switch (cmd) {
		case SHUB2AP_DEBUG_DATA:
			ret = print_mcu_debug(dataframe, &index, frame_len);
			break;
		case SHUB2AP_BYPASS_DATA:
			ret = parsing_bypass_data(dataframe, &index, frame_len);
			break;
		case SHUB2AP_META_DATA:
			ret = parsing_meta_data(dataframe, &index, frame_len);
			break;
		case SHUB2AP_LIBRARY_DATA:
			ret = parsing_scontext_data(dataframe, &index, frame_len);
			break;
		case SHUB2AP_GYRO_CAL:
			sensor = get_sensor(SENSOR_TYPE_GYROSCOPE);
			if (sensor)
				ret = sensor->funcs->parsing_data(dataframe, &index, frame_len);
			break;
		case SHUB2AP_MAG_CAL:
			sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);
			if (sensor)
				ret = sensor->funcs->parsing_data(dataframe, &index, frame_len);
			break;
		case SHUB2AP_SYSTEM_INFO:
			ret = print_system_info(dataframe + index, &index, frame_len);
			break;
		case SHUB2AP_HUB_REQ:
			ret = parsing_hub_request(&index, dataframe);
			break;
		case SHUB2AP_PROX_THRESH:
			sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
			if (sensor)
				sensor->funcs->parsing_data(dataframe, &index, frame_len);
			break;
		case SHUB2AP_LOG_DUMP:
			ret = save_log_dump(dataframe, &index, frame_len);
			break;
		case SHUB2AP_BIG_DATA:
			ret = parsing_big_data(dataframe, &index, frame_len);
			break;
		default:
			shub_errf("0x%x cmd doesn't support, index = %d", cmd, index);
			ret = -1;
			break;
		}
	}

	if (ret < 0) {
		print_dataframe(dataframe, frame_len);
		return ret;
	}

	return ret;
}


int get_shub_msg_big_buffer(struct shub_msg *msg, char *packet, int packet_size)
{
	int ret = 0;

	mutex_lock(&rx_msg_mutex);
	if (rx_msg.timestamp != msg->timestamp) {
		kfree(rx_msg.buffer);
		memcpy(&rx_msg, msg, SHUB_MSG_HEADER_SIZE);
		rx_msg.length = 0;
		rx_msg.buffer = kzalloc(rx_msg.total_length, GFP_KERNEL);
		if (ZERO_OR_NULL_PTR(rx_msg.buffer)) {
			shub_errf("fail to alloc memory for total buffer(%d %d %d)", msg->cmd, msg->type,
					msg->subcmd);
			ret = -ENOMEM;
			goto msg_alloc_error;
		}
	}

	if (rx_msg.length + msg->length > rx_msg.total_length) {
		shub_errf("length error. cmd %d, type %d, sub_cmd %d, total %d, len %d", rx_msg.cmd,
			rx_msg.type, rx_msg.subcmd, rx_msg.total_length, rx_msg.length + msg->length);
		ret = -EINVAL;
		goto msg_error;
	}

	memcpy(&rx_msg.buffer[rx_msg.length], packet + SHUB_MSG_HEADER_SIZE, msg->length);
	rx_msg.length += msg->length;

	if (rx_msg.length == rx_msg.total_length) {
		msg->length = rx_msg.length;
		msg->buffer = rx_msg.buffer;
		memset(&rx_msg, 0, sizeof(struct shub_msg));
		ret = 0;
	} else {
		ret = -EINVAL;
	}

	mutex_unlock(&rx_msg_mutex);
	return ret;
msg_error:
	kfree(rx_msg.buffer);
msg_alloc_error:
	memset(&rx_msg, 0, sizeof(struct shub_msg));
	mutex_unlock(&rx_msg_mutex);
	return ret;
}

int get_shub_msg_buffer(struct shub_msg *msg, char *packet, int packet_size)
{
	int ret = 0;

	if (msg->total_length != msg->length) {
		ret = get_shub_msg_big_buffer(msg, packet, packet_size);
	} else {
		msg->buffer = kzalloc(msg->length, GFP_KERNEL);
		if (ZERO_OR_NULL_PTR(msg->buffer)) {
			shub_errf("fail to alloc memory for msg buffer(%d %d %d)", msg->cmd, msg->type, msg->subcmd);
			return -ENOMEM;
		}

		memcpy(msg->buffer, &packet[SHUB_MSG_HEADER_SIZE], msg->length);
	}

	return ret;
}

int get_shub_msg(struct shub_msg *msg, char *packet, int packet_size)
{
	int ret = 0;

	if (packet_size < SHUB_MSG_HEADER_SIZE) {
		shub_infof("packet size is small/(%s)", packet);
		return -EINVAL;
	}

	memcpy(msg, packet, SHUB_MSG_HEADER_SIZE);

	if (msg->total_length)
		ret = get_shub_msg_buffer(msg, packet, packet_size);

	return ret;
}

struct shub_msg *get_msg_from_pending_list(struct shub_msg msg)
{
	struct shub_msg *m, *n;
	struct shub_msg *found_msg = NULL;

	mutex_lock(&pending_mutex);
	if (!list_empty(&pending_list)) {
		list_for_each_entry_safe(m, n, &pending_list, list) {
			if ((m->cmd == msg.cmd) && (m->type == msg.type) && (m->subcmd == msg.subcmd)) {
				list_del(&m->list);
				found_msg = m;
				break;
			}
		}

		if (!found_msg)
			shub_errf("%d %d %d - Not match error", msg.cmd, msg.type, msg.subcmd);
	} else {
		shub_errf("List empty error(%d %d %d)", msg.cmd, msg.type, msg.subcmd);
	}

	mutex_unlock(&pending_mutex);

	return found_msg;
}

void handle_packet(char *packet, int packet_size)
{
	struct shub_msg msg;
	int ret;

#ifdef CONFIG_SHUB_DEBUG
	if (check_debug_log_state(SHUB_LOG_DATA_PACKET))
		print_dataframe(packet, packet_size);
#endif

	ret = get_shub_msg(&msg, packet, packet_size);
	if (ret)
		return;

	if (msg.cmd == CMD_GETVALUE) {
		struct shub_msg *pending_msg;

		pending_msg = get_msg_from_pending_list(msg);
		if (pending_msg) {
			kfree(pending_msg->buffer);
			pending_msg->length = msg.length;
			if (pending_msg->length != 0)
				pending_msg->buffer = msg.buffer;

			if (pending_msg->done != NULL && !completion_done(pending_msg->done))
				complete(pending_msg->done);
		}
	} else if (msg.cmd == CMD_REPORT) {
		parse_dataframe(msg.buffer, msg.length);
		kfree(msg.buffer);
	} else {
		shub_errf("msg_cmd : %d, packet size %d", msg.cmd, packet_size);
		print_dataframe(packet, packet_size);
	}
}

int get_cnt_comm_fail(void)
{
	return cnt_comm_fail;
}

int get_cnt_timeout(void)
{
	return cnt_timeout;
}

void stop_comm_to_hub(void)
{
	clean_pending_list();
}

int init_comm_to_hub(void)
{
	mutex_init(&comm_mutex);
	mutex_init(&pending_mutex);
	mutex_init(&rx_msg_mutex);
	INIT_LIST_HEAD(&pending_list);

	cnt_timeout = 0;
	cnt_comm_fail = 0;

	memset(shub_cmd_data, 0, SHUB_CMD_SIZE);
	return 0;
}

void exit_comm_to_hub(void)
{
	clean_pending_list();
	mutex_destroy(&comm_mutex);
	mutex_destroy(&pending_mutex);
	mutex_destroy(&rx_msg_mutex);
}
