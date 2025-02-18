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

#include <linux/device.h>
#include <linux/slab.h>

#include "../comm/shub_comm.h"
#include "../sensor/scontext.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../sensorhub/shub_device.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_dev_core.h"
#include "../utility/shub_file_manager.h"
#include "../utility/sensor_core.h"
#include "../vendor/shub_vendor.h"
#include "shub_sensor_dump.h"
#include "shub_system_checker.h"
#include "shub_debug.h"

#define TIMEINFO_SIZE   50
#define SUPPORT_SENSORLIST \
do { \
	{SENSOR_TYPE_ACCELEROMETER, SENSOR_TYPE_GYROSCOPE, SENSOR_TYPE_GEOMAGNETIC_FIELD, SENSOR_TYPE_PRESSURE, \
	SENSOR_TYPE_PROXIMITY, SENSOR_TYPE_LIGHT} \
} while (0)

#if IS_ENABLED(CONFIG_SENSORS_GRIP_FAILURE_DEBUG)
static void sensor_get_grip_info(void)
{
	int i = 0;

	for (i = 0; i < GRIP_MAX_CNT; i++)
		pr_err("GRIP%d_REASON : %d\n", i, grip_error[i]);
}
#endif

/*
 * Caution !
 * Do not change the format. ex) !@#REG_DUMP!@#, @@TYPE:1##
 * Big data parses the data.
 */
static ssize_t sensor_dump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char **sensor_dump_data  = get_sensor_dump_data();
	int types[] = SENSOR_DUMP_SENSOR_LIST;
	char str_no_sensor_dump[] = "there is no sensor dump";
	int i = 0, ret;
	char *sensor_dump;
	char temp[sensor_dump_length(DUMPREGISTER_MAX_SIZE) + LENGTH_SENSOR_TYPE_MAX] = {0,};
	char time_temp[TIMEINFO_SIZE] = "";
	char *time_info;
	char str_no_registered_sensor[] = "there is no registered sensor";
	char reset_info[TIMEINFO_SIZE*2 + 20] = "Sensor Hub Reset : ";
	char str_reg_dump_filter[] = "!@#REG_DUMP!@#";
	int cnt = 0;
	struct shub_sensor *sensor;

	sensor_dump = kzalloc(
	    (sensor_dump_length(DUMPREGISTER_MAX_SIZE) + LENGTH_SENSOR_TYPE_MAX) * (ARRAY_SIZE(types)), GFP_KERNEL);

	if (!sensor_dump) {
		shub_errf("fail to allocate memory for dump buffer");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(types); i++) {
		if (sensor_dump_data[types[i]] != NULL) {
			snprintf(temp, sizeof(temp), "@@TYPE:%d##\n%s", types[i], sensor_dump_data[types[i]]);
			strcpy(&sensor_dump[(int)strlen(sensor_dump)], temp);
		}
	}

	if (get_reset_count() > 0) {
		struct reset_info_t reset = get_reset_info();

		if (reset.reason == RESET_TYPE_KERNEL_SYSFS)
			snprintf(&reset_info[(int)strlen(reset_info)], sizeof(reset_info) - (int)strlen(reset_info),
				"Kernel Sysfs\n");
		else if (reset.reason == RESET_TYPE_KERNEL_NO_EVENT)
			snprintf(&reset_info[(int)strlen(reset_info)], sizeof(reset_info) - (int)strlen(reset_info),
				 "Kernel No Event\n");
		else if (reset.reason == RESET_TYPE_HUB_NO_EVENT)
			snprintf(&reset_info[(int)strlen(reset_info)], sizeof(reset_info) - (int)strlen(reset_info),
				 "Hub Req No Event\n");
		else if (reset.reason == RESET_TYPE_KERNEL_COM_FAIL)
			snprintf(&reset_info[(int)strlen(reset_info)], sizeof(reset_info) - (int)strlen(reset_info),
				 "Com Fail\n");
		else
			snprintf(&reset_info[(int)strlen(reset_info)], sizeof(reset_info) - (int)strlen(reset_info),
				 "HUB Reset\n");
		strcpy(&reset_info[(int)strlen(reset_info)], time_temp);

		snprintf(time_temp, sizeof(time_temp),
			 " %04d%02d%02d %02d:%02d:%02d UTC(%llu)\n", reset.time.tm_year + 1900,
			 reset.time.tm_mon + 1, reset.time.tm_mday, reset.time.tm_hour, reset.time.tm_min,
			 reset.time.tm_sec, reset.timestamp);
		strncpy(&reset_info[(int)strlen(reset_info)], time_temp, sizeof(reset_info) - (int)strlen(reset_info));
	} else {
		snprintf(&reset_info[(int)strlen(reset_info)], sizeof(reset_info) - (int)strlen(reset_info), " None\n");
	}

	for (i = 0; i < SENSOR_TYPE_MAX; i++) {
		sensor = get_sensor(i);
		if (sensor && sensor->enable_timestamp != 0)
			cnt++;
	}

	if (cnt > 0) {
		time_info = kzalloc(TIMEINFO_SIZE * 3 * cnt, GFP_KERNEL);
		if (!time_info) {
			shub_infof("fail to allocate memory for time info");
			time_info = NULL;
			cnt = 0;
			goto print_sensordump;
		}

		for (i = 0; i < SENSOR_TYPE_MAX; i++) {
			sensor = get_sensor(i);
			if (sensor && sensor->enable_timestamp != 0) {
				struct rtc_time regi_tm = sensor->enable_time;
				struct rtc_time unregi_tm = sensor->disable_time;
				char name[SENSOR_NAME_MAX] = "";

				memcpy(name, sensor->name, SENSOR_NAME_MAX);

				memset(time_temp, 0, sizeof(time_temp));
				snprintf(time_temp, TIMEINFO_SIZE, "%3d %s\n", i, name);
				strcpy(&time_info[(int)strlen(time_info)], time_temp);

				if (sensor->enabled) {
					if (sensor->disable_timestamp != 0) {
						snprintf(time_temp, TIMEINFO_SIZE,
							 "- %04d%02d%02d %02d:%02d:%02d UTC(%llu)\n",
							 unregi_tm.tm_year + 1900, unregi_tm.tm_mon + 1,
							 unregi_tm.tm_mday, unregi_tm.tm_hour, unregi_tm.tm_min,
							 unregi_tm.tm_sec, sensor->disable_timestamp);
						strcpy(&time_info[(int)strlen(time_info)], time_temp);
					}

					snprintf(time_temp, TIMEINFO_SIZE,
						"+ %04d%02d%02d %02d:%02d:%02d UTC(%llu)\n",
						regi_tm.tm_year + 1900, regi_tm.tm_mon + 1, regi_tm.tm_mday,
						regi_tm.tm_hour, regi_tm.tm_min, regi_tm.tm_sec,
						sensor->enable_timestamp);
					strcpy(&time_info[(int)strlen(time_info)], time_temp);
				} else {
					snprintf(time_temp, TIMEINFO_SIZE,
						"+ %04d%02d%02d %02d:%02d:%02d UTC(%llu)\n",
						regi_tm.tm_year + 1900, regi_tm.tm_mon + 1, regi_tm.tm_mday,
						regi_tm.tm_hour, regi_tm.tm_min, regi_tm.tm_sec,
						sensor->enable_timestamp);
					strcpy(&time_info[(int)strlen(time_info)], time_temp);

					if (sensor->disable_timestamp != 0) {
						snprintf(time_temp, TIMEINFO_SIZE,
							"- %04d%02d%02d %02d:%02d:%02d UTC(%llu)\n",
							unregi_tm.tm_year + 1900, unregi_tm.tm_mon + 1,
							unregi_tm.tm_mday, unregi_tm.tm_hour, unregi_tm.tm_min,
							unregi_tm.tm_sec, sensor->disable_timestamp);
						strcpy(&time_info[(int)strlen(time_info)], time_temp);
					}
				}
			}
		}
	} else {
		time_info = str_no_registered_sensor;
	}

print_sensordump:
	if ((int)strlen(sensor_dump) == 0)
		ret = snprintf(buf, PAGE_SIZE, "%s\n%s\n%s\n", str_no_sensor_dump, reset_info, time_info);
	else
		ret = snprintf(buf, PAGE_SIZE, "%s\n%s%s\n\n%s\n%s\n",
				str_reg_dump_filter, sensor_dump, str_reg_dump_filter, reset_info, time_info);

	if (shub_debug_level()) {
		char file_path[255] = "";
		struct rtc_time tm;
		get_tm(&(tm));

		memset(time_temp, 0, sizeof(time_temp));
		snprintf(time_temp, sizeof(time_temp),
				"%04d%02d%02d_%02d%02d%02d(%llu)", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec, get_current_timestamp());
		snprintf(file_path, sizeof(file_path), "/data/vendor/sensorhub/register_dump_%s.txt", time_temp);

		if (shub_file_write(file_path, sensor_dump, strlen(sensor_dump), 0) > 0) {
			shub_info("save register_dump_%s", time_temp);
		}
	}

	kfree(sensor_dump);
	if (cnt > 0)
		kfree(time_info);

	return ret;
}

static ssize_t sensor_dump_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int sensor_type, ret;
	char name[SENSOR_NAME_MAX + 1] = {0,};

	if (sscanf(buf, "%40s", name) != 1)             // 40 : SENSOR_NAME_MAX
		return -EINVAL;

	clear_sensor_dump();

	if ((strcmp(name, "all")) == 0) {
		int type;

		shub_errf("last event of sensors");
		for (type = 0; type < SENSOR_TYPE_MAX; type++) {
			print_sensor_debug(type);
		}

		print_big_data();

		sensorhub_save_ram_dump();
		ret = send_all_sensor_dump_command();
	} else {
		if (strcmp(name, "accelerometer") == 0)
			sensor_type = SENSOR_TYPE_ACCELEROMETER;
		else if (strcmp(name, "gyroscope") == 0)
			sensor_type = SENSOR_TYPE_GYROSCOPE;
		else if (strcmp(name, "magnetic") == 0)
			sensor_type = SENSOR_TYPE_GEOMAGNETIC_FIELD;
		else if (strcmp(name, "pressure") == 0)
			sensor_type = SENSOR_TYPE_PRESSURE;
		else if (strcmp(name, "proximity") == 0)
			sensor_type = SENSOR_TYPE_PROXIMITY;
		else if (strcmp(name, "light") == 0)
			sensor_type = SENSOR_TYPE_LIGHT;
		else {
			shub_errf("is not supported : %s", buf);
			sensor_type = -1;
			return -EINVAL;
		}
		ret = send_sensor_dump_command(sensor_type);
	}
#if IS_ENABLED(CONFIG_SENSORS_GRIP_FAILURE_DEBUG)
	sensor_get_grip_info();
#endif
	return (ret == 0) ? size : ret;
}

static ssize_t sensor_axis_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int accel_position = -1;
	int gyro_position = -1;
	int mag_position = -1;
	struct shub_sensor *sensor;

	sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER);
	if (sensor)
		accel_position = sensor->funcs->get_position();

	sensor = get_sensor(SENSOR_TYPE_GYROSCOPE);
	if (sensor)
		gyro_position = sensor->funcs->get_position();

	sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);
	if (sensor)
		mag_position = sensor->funcs->get_position();

	return snprintf(buf, PAGE_SIZE, "%d: %d\n%d: %d\n%d: %d\n",
			SENSOR_TYPE_ACCELEROMETER, accel_position,
			SENSOR_TYPE_GYROSCOPE, gyro_position,
			SENSOR_TYPE_GEOMAGNETIC_FIELD, mag_position);
}

static ssize_t sensor_axis_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct shub_sensor *sensor;
	int type = 0;
	int position = 0;

	sscanf(buf, "%9d,%9d", &type, &position);

	if (position < 0)
		return -EINVAL;

	sensor = get_sensor(type);
	if (!sensor) {
		shub_errf("type %d is not suppoerted", type);
		return -EINVAL;
	}

	if (sensor->funcs && sensor->funcs->set_position)
		sensor->funcs->set_position(position);

	return size;
}

static bool debug_enable[SHUB_LOG_MAX];

bool check_debug_log_state(int log_type)
{
	return log_type < SHUB_LOG_MAX ? debug_enable[log_type] : false;
}

static ssize_t debug_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d %d\n",
			debug_enable[SHUB_LOG_EVENT_TIMESTAMP], debug_enable[SHUB_LOG_DATA_PACKET]);
}

static ssize_t debug_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	char *input_str = NULL, *input_str_origin = NULL, *token = NULL;
	unsigned int arg[5] = {0,};
	int index = 0;

	shub_infof("%s", buf);
	if (strlen(buf) == 0)
		return size;

	input_str = kzalloc(strlen(buf) + 1, GFP_KERNEL);
	if (!input_str)
		return -ENOMEM;

	memcpy(input_str, buf, strlen(buf));
	input_str_origin = input_str;

	while (((token = strsep(&input_str, " ")) != NULL)) {
		switch (index) {
		case 0:
			if (kstrtoint(token, 10, &arg[0]) < 0)
				goto exit;
			break;
		case 1:
			if (kstrtoint(token, 10, &arg[1]) < 0)
				goto exit;
			break;
		default:
			goto exit;
		}
		index++;
	}

	if (index == 1) {
		for (index = 0; index < SHUB_LOG_MAX; index++)
			debug_enable[index] = arg[0] ? true : false;
	} else if (arg[1] < SHUB_LOG_MAX) {
		debug_enable[arg[1]] = arg[0] ? true : false;
	}
exit:
	kfree(input_str_origin);
	return size;
}

#ifdef CONFIG_SHUB_DEBUG
int htou8(char input)
{
	int ret = 0;

	if ('0' <= input && input <= '9')
		return ret = input - '0';
	else if ('a' <= input && input <= 'f')
		return ret = input - 'a' + 10;
	else if ('A' <= input && input <= 'F')
		return ret = input - 'A' + 10;
	else
		return 0;
}

char register_value[5];

static ssize_t make_command_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	u8 cmd = 0, type = 0, subcmd = 0;
	char *send_buf = NULL;
	int send_buf_len = 0;
	unsigned int arg[10] = {0, };

	char *input_str = NULL, *input_str_origin = NULL, *token = NULL;
	int index = 0, i = 0;

	shub_infof("%s", buf);

	if (strlen(buf) == 0)
		return size;

	input_str = kzalloc(strlen(buf) + 1, GFP_KERNEL);
	if (!input_str)
		return -ENOMEM;

	memcpy(input_str, buf, strlen(buf));
	input_str_origin = input_str;

	while (((token = strsep(&input_str, " ")) != NULL)) {
		switch (index) {
		case 0:
			if (kstrtou8(token, 10, &cmd) < 0) {
				shub_errf("invalid cmd(%d)", cmd);
				goto exit;
			}
			break;
		case 1:
			if (kstrtou8(token, 10, &type) < 0) {
				shub_errf("invalid type(%d)", type);
				goto exit;
			}
			break;
		case 2:
			if (kstrtou8(token, 10, &subcmd) < 0) {
				shub_errf("invalid subcmd(%d)", subcmd);
				goto exit;
			}
			break;
		case 3:
			if (cmd == CMD_SETVALUE && subcmd == HUB_SYSTEM_CHECK) {
				if (kstrtouint(token, 10, &arg[0])) {
					shub_errf("parsing error");
					goto exit;
				}
			} else if (cmd == CMD_ADD) {
				send_buf_len = 8;
				send_buf = kzalloc(send_buf_len, GFP_KERNEL);
				if (kstrtouint(token, 10, &arg[0])) {
					shub_errf("parssing error");
					goto exit;
				}
				memcpy(&send_buf[0], &arg[0], 4);
			} else {
				if ((strlen(token) - 1) % 2 != 0) {
					shub_errf("not match buf len(%d) != %d", (int)strlen(token), send_buf_len);
					goto exit;
				}
				send_buf_len = (strlen(token) - 1) / 2;
				send_buf = kzalloc(send_buf_len, GFP_KERNEL);
				if (!send_buf) {
					shub_errf("fail to alloc memory");
					goto exit;
				}

				for (i = 0; i < send_buf_len; i++) {
					send_buf[i] = (u8)((htou8(token[2 * i]) << 4) | htou8(token[2 * i + 1]));
					shub_infof("[%d]:%d", i, send_buf[i]);
				}
			}
			break;
		case 4:
			if (cmd == CMD_SETVALUE && subcmd == HUB_SYSTEM_CHECK) {
				if (kstrtouint(token, 10, &arg[1])) {
					shub_errf("parsing error");
					goto exit;
				}
			} else if (cmd == CMD_ADD) {
				if (kstrtouint(token, 10, &arg[1])) {
					shub_errf("parssing error");
					goto exit;
				}
				memcpy(&send_buf[4], &arg[1], 4);
				shub_infof("type : %d, sampling : %d, report : %d", type, arg[0], arg[1]);
			} else {
				shub_errf("unused input");
			}
			break;
		default:
			goto exit;
		}
		index++;
	}

	if (index < 2) {
		shub_errf("need more input");
		goto exit;
	}

	if (cmd == CMD_SETVALUE && subcmd == HUB_SYSTEM_CHECK) {
		sensorhub_system_check(arg[0], arg[1]);
	} else {
		ret = shub_send_command(cmd, type, subcmd, send_buf, send_buf_len);
		if (ret < 0) {
			shub_errf("shub_send_command failed");
			goto exit;
		}
	}
exit:
	kfree(send_buf);
	kfree(input_str_origin);

	return size;
}

static ssize_t register_rw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (register_value[1] == 'r') {
		return sprintf(buf, "sensor(%d) %c regi(0x%x) val(0x%x) ret(%d)\n",
			       register_value[0], register_value[1], register_value[2],
			       register_value[3], register_value[4]);
	} else {
		if (register_value[4] == true) {
			return sprintf(buf, "sensor(%d) %c regi(0x%x) val(0x%x) SUCCESS\n",
				       register_value[0], register_value[1], register_value[2], register_value[3]);
		} else {
			return sprintf(buf, "sensor(%d) %c regi(0x%x) val(0x%x) FAIL\n",
				       register_value[0], register_value[1], register_value[2], register_value[3]);
		}
	}
}

static ssize_t register_rw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int index = 0, ret = 0;
	u8 sensor_type, send_val[2];
	char rw_cmd;

	char input_str[20] = {0,};
	char *tmp_str = NULL, *token = NULL;

	if (strlen(buf) >= sizeof(input_str)) {
		shub_errf("bufsize too long(%d)", (int)strlen(buf));
		goto exit;
	}

	memcpy(input_str, buf, strlen(buf));
	tmp_str = input_str;

	while (((token = strsep(&tmp_str, " ")) != NULL)) {
		switch (index) {
		case 0:
			if (kstrtou8(token, 10, &sensor_type) < 0 || (sensor_type >= SENSOR_TYPE_MAX)) {
				shub_errf("invalid type(%d)", sensor_type);
				goto exit;
			}
			break;
		case 1:
			if (token[0] == 'r' || token[0] == 'w')
				rw_cmd = token[0];
			else {
				shub_errf("invalid cmd(%c)", token[0]);
				goto exit;
			}
			break;
		case 2:
		case 3:
			if ((strlen(token) == 4) && token[0] != '0' && token[1] != 'x') {
				shub_errf("invalid value(0xOO) %s", token);
				goto exit;
			}
			send_val[index - 2] = (u8)((htou8(token[2]) << 4) | htou8(token[3]));
			break;
		default:
			goto exit;
		}
		index++;
	}

	register_value[0] = sensor_type;
	register_value[1] = rw_cmd;
	register_value[2] = send_val[0];

	if (rw_cmd == 'r') {
		char *rec_buf = NULL;
		int rec_buf_len = 0;

		ret = shub_send_command_wait(CMD_GETVALUE, sensor_type, SENSOR_REGISTER_RW,
					     1000, send_val, 1, &rec_buf, &rec_buf_len, false);
		register_value[4] = true;
		if (ret < 0) {
			register_value[4] = false;
			shub_errf("shub_send_command_wait fail %d", ret);
			goto exit;
		}

		register_value[3] = rec_buf[0];

		kfree(rec_buf);
	} else { /* rw_cmd == w */
		ret = shub_send_command(CMD_SETVALUE, sensor_type, SENSOR_REGISTER_RW, send_val, 2);
		register_value[3] = send_val[1];
		register_value[4] = true;
		if (ret < 0) {
			register_value[4] = false;
			shub_errf("shub_send_command fail %d", ret);
			goto exit;
		}
	}

exit:
	return size;
}
#endif

#if IS_ENABLED(CONFIG_SENSORS_GRIP_FAILURE_DEBUG)
void update_grip_error(u8 idx, u32 error_state)
{
	if (idx >= GRIP_MAX_CNT) {
		pr_info("[FACTORY] %s dump is NULL \n", __func__,
			idx);
		return;
	}
	grip_error[idx] = error_state;
	pr_info("[FACTORY] %s [IC num %d] grip_error %d\n", __func__,
		idx, error_state);
}
EXPORT_SYMBOL(update_grip_error);
static ssize_t grip_fail_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i = 0, j = 0;

	for (i = 0; i < GRIP_MAX_CNT; i++) {
		j += snprintf(buf + j, PAGE_SIZE - j,
			"\"GRIP%d_REASON\":\"%d\",",
			i, grip_error[i]);
		pr_info("[FACTORY] %s \"GRIP%d_REASON\":\"%d\",",
			__func__, i, grip_error[i]);
		grip_error[i] = 0;
	}

	return j;
}
#endif

static DEVICE_ATTR(sensor_dump, 0664, sensor_dump_show, sensor_dump_store);
static DEVICE_ATTR_RW(sensor_axis);
static DEVICE_ATTR_RW(debug_enable);
#ifdef CONFIG_SHUB_DEBUG
static DEVICE_ATTR(make_command, 0220, NULL, make_command_store);
static DEVICE_ATTR_RW(register_rw);
#endif
#if IS_ENABLED(CONFIG_SENSORS_GRIP_FAILURE_DEBUG)
static DEVICE_ATTR(grip_fail, 0440, grip_fail_show, NULL);
#endif
static struct device_attribute *shub_debug_attrs[] = {
	&dev_attr_sensor_axis,
#if IS_ENABLED(CONFIG_SENSORS_GRIP_FAILURE_DEBUG)
	&dev_attr_grip_fail,
#endif
	&dev_attr_sensor_dump,
	&dev_attr_debug_enable,
#ifdef CONFIG_SHUB_DEBUG
	&dev_attr_make_command,
	&dev_attr_register_rw,
#endif
	NULL,
};

int init_shub_debug_sysfs(void)
{
	struct shub_data_t *data = get_shub_data();
	int ret;

	ret = sensor_device_create(&data->sysfs_dev, data, "ssp_sensor");
	if (ret < 0) {
		shub_errf("fail to creat ssp_sensor device");
		return ret;
	}

	ret = add_sensor_device_attr(data->sysfs_dev, shub_debug_attrs);
	if (ret < 0)
		shub_errf("fail to add shub debug attr");

	return ret;
}

void remove_shub_debug_sysfs(void)
{
	struct shub_data_t *data = get_shub_data();

	remove_sensor_device_attr(data->sysfs_dev, shub_debug_attrs);
	sensor_device_destroy(data->sysfs_dev);
}
