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

#ifndef CONFIG_SHUB_MODULE
#include <linux/module.h>
#endif

#include "../comm/misc/shub_misc.h"
#include "../comm/shub_comm.h"
#include "../comm/shub_iio.h"
#include "../debug/shub_debug.h"
#include "../debug/shub_system_checker.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../sensormanager/shub_sensor_sysfs.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_wait_event.h"
#include "../utility/shub_wakelock.h"
#include "../utility/shub_file_manager.h"
#include "../vendor/shub_vendor.h"
#include "../others/shub_motor_callback.h"
#include "../others/shub_panel.h"
#include "../debug/shub_dump.h"
#include "../factory/shub_factory.h"
#include "../sensor/light.h"
#include "shub_device.h"
#include "shub_firmware.h"
#include "shub_sysfs.h"

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>

static struct shub_data_t *shub_data;

#define SHUB_TIMESTAMP_SYNC_TIMER_SEC (10 * HZ)

#define PM_RESUME             (0x1)
#define PM_SUSPEND            (0x2)
#define PM_PREPARE          (0x3)
#define PM_COMPLETE         (0x4)

struct shub_data_t *get_shub_data(void)
{
	return shub_data;
}

struct device *get_shub_device(void)
{
	return &shub_data->pdev->dev;
}

static void timestamp_sync_work_func(struct work_struct *work)
{
	int ret;

	ret = shub_send_command(CMD_SETVALUE, TYPE_HUB, RTC_TIME, NULL, 0);
	if (ret < 0)
		shub_errf("comm fail %d", ret);
}

static void timestamp_sync_timer_func(struct timer_list *timer)
{
	struct shub_data_t *data = container_of(timer, struct shub_data_t, ts_sync_timer);

	queue_work(data->shub_wq, &shub_data->work_ts_sync);
	mod_timer(&shub_data->ts_sync_timer, round_jiffies_up(jiffies + SHUB_TIMESTAMP_SYNC_TIMER_SEC));
}

static void enable_timestamp_sync_timer(void)
{
	mod_timer(&shub_data->ts_sync_timer, round_jiffies_up(jiffies + SHUB_TIMESTAMP_SYNC_TIMER_SEC));
}

static void disable_timestamp_sync_timer(void)
{
	del_timer_sync(&shub_data->ts_sync_timer);
	cancel_work_sync(&shub_data->work_ts_sync);
}

static int initialize_timestamp_sync_timer(void)
{
	timer_setup(&shub_data->ts_sync_timer, timestamp_sync_timer_func, 0);

	INIT_WORK(&shub_data->work_ts_sync, timestamp_sync_work_func);
	return 0;
}

static int get_shub_system_info_from_hub(void)
{
	int ret = 0;
	char *buffer = NULL;
	unsigned int buffer_length;

	ret = shub_send_command_wait(CMD_GETVALUE, TYPE_HUB, HUB_SYSTEM_INFO, 1000, NULL, 0, &buffer, &buffer_length,
				     true);

	if (ret < 0) {
		shub_errf("fail %d", ret);
		return ret;
	}

	if (buffer_length != sizeof(shub_data->system_info)) {
		shub_errf("buffer length error %d", buffer_length);
		return -EINVAL;
	}

	memcpy(&shub_data->system_info, buffer, sizeof(shub_data->system_info));
	kfree(buffer);

	return ret;
}

struct shub_system_info *get_shub_system_info(void)
{
	return &shub_data->system_info;
}


void set_model_name_to_hub(void)
{
	struct device_node *np = shub_data->pdev->dev.of_node;
	const char *model_name_string;

	if (of_property_read_string(np, "model-name", &model_name_string) >= 0) {
		shub_infof("model_name_string: %s", model_name_string);
		strcpy(shub_data->model_name, model_name_string);
		shub_send_command(CMD_SETVALUE, TYPE_HUB, MODEL_NAME_INFO, shub_data->model_name, MODEL_NAME_MAX);
	} else {
		shub_infof("model name dt doesn't exsist");
	}
}

static int send_pm_state(u8 pm_state)
{
	int ret;

	ret = shub_send_command(CMD_SETVALUE, TYPE_HUB, PM_STATE, &pm_state, 1);
	if (ret < 0)
		shub_errf("command %d failed", pm_state);
	else
		shub_infof("command %d", pm_state);

	return ret;
}

static int init_sensorhub(void)
{
	int ret = 0;

	ret = get_shub_system_info_from_hub();
	if (ret < 0)
		return ret;

	set_model_name_to_hub();

	send_pm_state(shub_data->pm_status);
	shub_send_status(shub_data->lcd_status);

	return ret;
}

void init_others(void)
{
	sync_motor_state();
	sync_panel_state();
}

struct reset_info_t get_reset_info(void)
{
	return shub_data->reset_info;
}

void save_reset_info(void)
{
	get_tm(&(shub_data->reset_info.time));
	shub_data->reset_info.timestamp = get_current_timestamp();
	if (shub_data->reset_type < RESET_TYPE_MAX)
		shub_data->reset_info.reason = shub_data->reset_type;
	else
		shub_data->reset_info.reason = RESET_TYPE_HUB_CRASHED;
	shub_data->reset_type = RESET_TYPE_MAX;
}

void sensorhub_stop(void)
{
	shub_infof("sensor hub stop");
	stop_comm_to_hub();
}

static void refresh_task(struct work_struct *work)
{
	if (!is_shub_working()) {
		shub_errf("shub is not working");
		return;
	}
	shub_infof();

	shub_wake_lock();
	shub_data->cnt_reset++;
	save_reset_info();

	if (sensorhub_refresh_func() < 0)
		goto exit;

	if (init_sensorhub() < 0)
		goto exit;

	if (init_shub_firmware() < 0)
		goto exit;

	if (refresh_sensors(&shub_data->pdev->dev) < 0)
		goto exit;

	if (shub_data->cnt_reset == 0) {
		initialize_factory();
		initialize_shub_dump();
	}

#ifdef CONFIG_SHUB_DEBUG
	if (is_system_checking())
		goto exit;
#endif
	init_others();
	enable_timestamp_sync_timer();
exit:
	shub_wake_up_wait_event(&shub_data->reset_lock);
	shub_wake_unlock();

#ifdef CONFIG_SHUB_DEBUG
	if (is_system_checking())
		system_ready_cb();
#endif
}

int queue_refresh_task(void)
{
	cancel_work_sync(&shub_data->work_refresh);

	shub_infof();
	queue_work(shub_data->shub_wq, &shub_data->work_refresh);

	return 0;
}

int shub_send_status(u8 state_sub_cmd)
{
	int ret;

	ret = shub_send_command(CMD_SETVALUE, TYPE_HUB, state_sub_cmd, NULL, 0);
	if (ret < 0)
		shub_errf("command %d failed", state_sub_cmd);
	else
		shub_infof("command %d", state_sub_cmd);

	return ret;
}

bool is_shub_working(void)
{
	return shub_data->is_probe_done && sensorhub_is_working() && !work_busy(&shub_data->work_reset);
}

int get_reset_count(void)
{
	return shub_data->cnt_reset;
}

static void reset_task(struct work_struct *work)
{
	int ret;

	shub_infof("");
	disable_timestamp_sync_timer();
	ret = sensorhub_reset();
	if (ret < 0) {
		shub_errf("reset failed");
		shub_wake_up_wait_event(&shub_data->reset_lock);
	}
}

void reset_mcu(int reason)
{
	if (work_busy(&shub_data->work_reset)) {
		shub_infof("reset work state : pending or running");
		return;
	}

	shub_infof("- reason(%u)", reason);
	shub_data->reset_type = reason;

	shub_data->cnt_shub_reset[RESET_TYPE_MAX]++;
	if (shub_data->reset_type < RESET_TYPE_MAX)
		shub_data->cnt_shub_reset[shub_data->reset_type]++;

	shub_lock_wait_event(&shub_data->reset_lock);
	queue_work(shub_data->shub_wq, &shub_data->work_reset);
}

static int init_sensor_vdd(void)
{
	int ret = 0;
	const char *sensor_vdd;
	struct device_node *np = shub_data->pdev->dev.of_node;
	enum of_gpio_flags flags;

	if (of_property_read_string(np, "sensor-vdd-regulator", &sensor_vdd) >= 0) {
		shub_infof("regulator: %s", sensor_vdd);

		shub_data->sensor_vdd_regulator = regulator_get(NULL, sensor_vdd);
		if (IS_ERR(shub_data->sensor_vdd_regulator)) {
			shub_errf("regulator get failed");
			shub_data->sensor_vdd_regulator = NULL;
			ret = -EINVAL;
		} else {
			regulator_set_load(shub_data->sensor_vdd_regulator, 1800000);
			shub_infof("sensor_vdd_regulator ok");
		}
	} else {
		int sensor_ldo_en = of_get_named_gpio_flags(np, "sensor-ldo-en", 0, &flags);

		if (sensor_ldo_en >= 0) {
			shub_infof("sensor_ldo_en: %d", sensor_ldo_en);
			shub_data->sensor_ldo_en = sensor_ldo_en;

			ret = gpio_request(shub_data->sensor_ldo_en, "sensor_ldo_en");
			if (ret < 0) {
				shub_errf("gpio %d request failed %d", shub_data->sensor_ldo_en, ret);
				return ret;
			}
			gpio_direction_output(shub_data->sensor_ldo_en, 1);
			gpio_free(shub_data->sensor_ldo_en);
		}
	}

	return 0;
}

int enable_sensor_vdd(void)
{
	int ret = 0;

	shub_infof();

	if (shub_data->sensor_vdd_regulator) {
		if (regulator_is_enabled(shub_data->sensor_vdd_regulator) == 0) {
			ret = regulator_enable(shub_data->sensor_vdd_regulator);
			if (ret)
				shub_err("sensor vdd regulator enable failed, ret = %d", ret);
		} else {
			shub_info("sensor vdd regulator is already enabled");
		}
	} else if (shub_data->sensor_ldo_en) {
		ret = gpio_request(shub_data->sensor_ldo_en, "sensor_ldo_en");
		if (ret < 0) {
			shub_errf("sensor ldo en gpio %d request failed %d", shub_data->sensor_ldo_en, ret);
		} else {
			gpio_set_value(shub_data->sensor_ldo_en, 1);
			gpio_free(shub_data->sensor_ldo_en);
		}
	}

	return ret;
}

int disable_sensor_vdd(void)
{
	int ret = 0;

	shub_infof();

	if (shub_data->sensor_vdd_regulator) {
		if (regulator_is_enabled(shub_data->sensor_vdd_regulator)) {
			ret = regulator_disable(shub_data->sensor_vdd_regulator);
			if (ret)
				shub_err("sensor vdd regulator disable failed, ret = %d", ret);
		} else {
			shub_info("sensor vdd regulator is already disabled");
		}
	} else if (shub_data->sensor_ldo_en) {
		ret = gpio_request(shub_data->sensor_ldo_en, "sensor_ldo_en");
		if (ret < 0) {
			shub_errf("sensor ldo en gpio %d request failed %d", shub_data->sensor_ldo_en, ret);
		} else {
			gpio_set_value(shub_data->sensor_ldo_en, 0);
			gpio_free(shub_data->sensor_ldo_en);
		}
	}

	return ret;
}

int init_sensorhub_device(void)
{
	int ret;

	ret = initialize_shub_firmware();
	if (ret < 0) {
		shub_errf("init firmware failed");
		return ret;
	}

	ret = init_sensor_vdd();
	if (ret < 0) {
		shub_errf("init sensor vdd failed");
		return ret;
	}

	shub_data->is_probe_done = false;
	shub_data->is_working = false;

	/* Reset */
	shub_data->cnt_reset = -1;
	{
		int type;

		for (type = 0; type <= RESET_TYPE_MAX; type++)
			shub_data->cnt_shub_reset[type] = 0;

		for (type = 0; type < MINI_DUMP_LENGTH; type++)
			shub_data->mini_dump[type] = 0;
	}

	shub_data->pm_status = PM_COMPLETE;
	shub_data->lcd_status = LCD_OFF;

	shub_data->shub_wq = create_singlethread_workqueue("shub_dev_wq");
	if (!shub_data->shub_wq)
		return -ENOMEM;

	INIT_WORK(&shub_data->work_refresh, refresh_task);
	INIT_WORK(&shub_data->work_reset, reset_task);
	init_waitqueue_head(&shub_data->reset_lock.waitqueue);
	atomic_set(&shub_data->reset_lock.state, 1);

	ret = initialize_timestamp_sync_timer();
	if (ret < 0) {
		shub_errf("could not create ts_sync workqueue");
		return ret;
	}
	shub_wake_lock_register();

	return 0;
}

void exit_sensorhub_device(void)
{
	shub_wake_lock_unregister();
	disable_timestamp_sync_timer();

	cancel_work_sync(&shub_data->work_refresh);
	cancel_work_sync(&shub_data->work_reset);

	del_timer(&shub_data->ts_sync_timer);
	cancel_work_sync(&shub_data->work_ts_sync);
	destroy_workqueue(shub_data->shub_wq);
	remove_shub_firmware();
}

int shub_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;

	shub_infof();

	shub_data = kzalloc(sizeof(struct shub_data_t), GFP_KERNEL);
	if (!shub_data) {
		shub_errf("failed to alloc for shub data");
		return -ENOMEM;
	}

	shub_data->pdev = pdev;

	if (!dev->of_node) {
		shub_errf("failed to get device node");
		return -ENODEV;
	}

	if (init_sensor_manager(dev) < 0) {
		shub_errf("failed to init sensor manager");
		goto err_init_sensor_manager;
	}

	if (init_sensorhub_device() < 0) {
		shub_errf("failed to init sensorhub device");
		goto err_init_device;
	}

	if (init_shub_debug() < 0) {
		shub_errf("failed to init debug");
		goto err_init_debug;
	}

	if (init_comm_to_hub() < 0) {
		shub_errf("failed to init comm");
		goto err_init_comm;
	}

	if (initialize_shub_misc_dev() < 0) {
		shub_errf("failed to init misc device");
		goto err_init_misc_dev;
	}

	if (init_shub_sysfs() < 0) {
		shub_errf("failed to init shub sysfs");
		goto err_init_shub_sysfs;
	}

	if (init_shub_sensor_sysfs() < 0) {
		shub_errf("failed to init sensor sysfs");
		goto err_init_sensor_sysfs;
	}

	if (init_shub_debug_sysfs() < 0) {
		shub_errf("failed to init debug sysfs");
		goto err_init_debug_sysfs;
	}

	if (init_file_manager() < 0) {
		shub_errf("failed to init file_manager");
		goto err_init_file_manager;
	}

	if (initialize_indio_dev(&shub_data->pdev->dev) < 0) {
		shub_errf("failed to init initialize_indio_dev");
		goto err_initialize_indio_dev;
	}

	init_shub_panel();
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2) && IS_ENABLED(CONFIG_SHUB_PANEL_NOTIFY)
	init_shub_panel_callback();
#endif
#ifdef CONFIG_SHUB_DEBUG
	shub_system_checker_init();
#endif
	enable_debug_timer();

	shub_data->is_probe_done = true;

	sensorhub_probe();

	dev_set_drvdata(dev, shub_data);
	shub_infof("probe success");

	return ret;

err_initialize_indio_dev:
	remove_indio_dev();
err_init_file_manager:
	remove_shub_debug_sysfs();
err_init_debug_sysfs:
	remove_shub_sysfs();
err_init_sensor_sysfs:
	remove_shub_misc_dev();
err_init_shub_sysfs:
	remove_shub_sensor_sysfs();
err_init_misc_dev:
	exit_comm_to_hub();
err_init_comm:
	exit_shub_debug();
err_init_debug:
	exit_sensorhub_device();
err_init_device:
	exit_sensor_manager(&pdev->dev);
err_init_sensor_manager:
	shub_errf("probe fail");
	return ret;
}

void shub_shutdown(struct platform_device *pdev)
{
	shub_infof();
	if (shub_data->is_probe_done == false)
		return;

	shub_data->is_probe_done = false;

	sensorhub_shutdown();
	remove_shub_panel();
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2) && IS_ENABLED(CONFIG_SHUB_PANEL_NOTIFY)
	remove_shub_panel_callback();
#endif
	remove_shub_dump();
	remove_shub_motor_callback();
	remove_factory();
	remove_indio_dev();
	remove_file_manager();
	remove_shub_debug_sysfs();
	remove_shub_sysfs();
	remove_shub_sensor_sysfs();
	remove_shub_misc_dev();
	exit_comm_to_hub();
	exit_shub_debug();
	exit_sensorhub_device();
	exit_sensor_manager(&pdev->dev);
	shub_infof("done");
}

int shub_suspend(struct device *dev)
{
	shub_infof();

	shub_data->pm_status = PM_SUSPEND;

#ifdef CONFIG_SHUB_AP_STATUS_CMD
	send_pm_state(PM_SUSPEND);
#endif
	return 0;
}

int shub_resume(struct device *dev)
{
	shub_infof();

	shub_data->pm_status = PM_RESUME;

#ifdef CONFIG_SHUB_AP_STATUS_CMD
	send_pm_state(PM_RESUME);
#endif
	return 0;
}

int shub_prepare(struct device *dev)
{
	shub_infof();

	disable_debug_timer();
	disable_timestamp_sync_timer();

	shub_data->pm_status = PM_PREPARE;

#ifdef CONFIG_SHUB_AP_STATUS_CMD
	send_pm_state(PM_PREPARE);
#endif
	return 0;
}

void shub_complete(struct device *dev)
{
	shub_infof();

	enable_debug_timer();
	enable_timestamp_sync_timer();

	shub_data->pm_status = PM_COMPLETE;

#ifdef CONFIG_SHUB_AP_STATUS_CMD
	send_pm_state(PM_COMPLETE);
#endif
}

void shub_queue_work(struct work_struct *work)
{
	queue_work(shub_data->shub_wq, work);
}
