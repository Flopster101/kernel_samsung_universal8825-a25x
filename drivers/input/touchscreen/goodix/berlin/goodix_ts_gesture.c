// SPDX-License-Identifier: GPL-2.0
/*
 * Goodix Gesture Module
 *
 * Copyright (C) 2019 - 2020 Goodix, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include "goodix_ts_core.h"

static bool module_initialized;

int goodix_gesture_enable(struct goodix_ts_data *ts, int enable)
{
	int ret = 0;

	if (!module_initialized)
		return 0;

	if (enable) {
		if (atomic_read(&ts->gsx_gesture->registered))
			ts_info("gesture module has been already registered");
		else
			ret = goodix_register_ext_module_no_wait(&ts->gsx_gesture->module);
	} else {
		if (!atomic_read(&ts->gsx_gesture->registered))
			ts_info("gesture module has been already unregistered");
		else
			ret = goodix_unregister_ext_module(&ts->gsx_gesture->module);
	}

	return ret;
}

static void gsx_set_utc_sponge(struct goodix_ts_data *ts)
{
	struct timespec64 current_time;
	int ret;
	u8 data[4] = {0};

	ktime_get_real_ts64(&current_time);
	data[0] = (0xFF & (u8)((current_time.tv_sec) >> 0));
	data[1] = (0xFF & (u8)((current_time.tv_sec) >> 8));
	data[2] = (0xFF & (u8)((current_time.tv_sec) >> 16));
	data[3] = (0xFF & (u8)((current_time.tv_sec) >> 24));
	ts_info("write UTC to sponge = %X", (int)current_time.tv_sec);

	ret = ts->hw_ops->write_to_sponge(ts, SEC_TS_CMD_SPONGE_OFFSET_UTC, data, 4);
	if (ret < 0)
		ts_err("failed to write UTC");
}

int gsx_set_lowpowermode(void *data, u8 mode)
{
	struct goodix_ts_data *ts = (struct goodix_ts_data *)data;
	int ret = 0;

	ts_info("%s[%X]", mode == TO_LOWPOWER_MODE ? "ENTER" : "EXIT", ts->plat_data->lowpower_mode);

	if (mode) {
		gsx_set_utc_sponge(ts);

		/* switch gesture mode */
		ret = ts->hw_ops->gesture(ts, true);
		if (ret < 0)
			ts_err("failed to switch gesture mode");
		atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_LPM);
	} else {
		/* switch coor mode */
		ret = ts->hw_ops->gesture(ts, false);
		if (ret < 0)
			ts_err("failed to switch coor mode");
		atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_ON);
	}

	return ret;
}

/**
 * gsx_gesture_type_show - show valid gesture type
 *
 * @module: pointer to goodix_ext_module struct
 * @buf: pointer to output buffer
 * Returns >=0 - succeed,< 0 - failed
 */
static ssize_t gsx_gesture_type_show(struct goodix_ext_module *module,
		char *buf)
{
	struct gesture_module *gesture = container_of(module, struct gesture_module, module);
	struct goodix_ts_data *ts = gesture->ts;
	int count = 0, i, ret = 0;
	unsigned char *type;

	type = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!type)
		return -ENOMEM;
	read_lock(&ts->gsx_gesture->rwlock);
	for (i = 0; i < 256; i++) {
		if (QUERYBIT(ts->gsx_gesture->gesture_type, i)) {
			count += scnprintf(type + count,
					PAGE_SIZE, "%02x,", i);
		}
	}
	if (count > 0)
		ret = scnprintf(buf, PAGE_SIZE, "%s\n", type);
	read_unlock(&ts->gsx_gesture->rwlock);

	kfree(type);
	return ret;
}

/**
 * gsx_gesture_type_store - set vailed gesture
 *
 * @module: pointer to goodix_ext_module struct
 * @buf: pointer to valid gesture type
 * @count: length of buf
 * Returns >0 - valid gestures, < 0 - failed
 */
static ssize_t gsx_gesture_type_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	struct gesture_module *gesture = container_of(module, struct gesture_module, module);
	struct goodix_ts_data *ts = gesture->ts;
	int i;

	if (count <= 0 || count > 256 || buf == NULL) {
		ts_err("Parameter error");
		return -EINVAL;
	}

	write_lock(&ts->gsx_gesture->rwlock);
	memset(ts->gsx_gesture->gesture_type, 0, GSX_GESTURE_TYPE_LEN);
	for (i = 0; i < count; i++)
		ts->gsx_gesture->gesture_type[buf[i]/8] |= (0x1 << buf[i]%8);
	write_unlock(&ts->gsx_gesture->rwlock);

	return count;
}

static ssize_t gsx_gesture_enable_show(struct goodix_ext_module *module,
		char *buf)
{
	struct gesture_module *gesture = container_of(module, struct gesture_module, module);
	struct goodix_ts_data *ts = gesture->ts;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			atomic_read(&ts->gsx_gesture->registered));
}

static ssize_t gsx_gesture_enable_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	struct gesture_module *gesture = container_of(module, struct gesture_module, module);
	struct goodix_ts_data *ts = gesture->ts;

	bool val;
	int ret;

	ret = strtobool(buf, &val);
	if (ret < 0)
		return ret;

	if (val) {
		ret = goodix_gesture_enable(ts, 1);
		return ret ? ret : count;
	} else {
		ret = goodix_gesture_enable(ts, 0);
		return ret ? ret : count;
	}
}

static ssize_t gsx_gesture_data_show(struct goodix_ext_module *module,
		char *buf)
{
	struct gesture_module *gesture = container_of(module, struct gesture_module, module);
	struct goodix_ts_data *ts = gesture->ts;

	ssize_t count;

	read_lock(&ts->gsx_gesture->rwlock);
	count = scnprintf(buf, PAGE_SIZE, "gesture type code:0x%x\n",
			ts->gsx_gesture->gesture_data);
	read_unlock(&ts->gsx_gesture->rwlock);

	return count;
}

const struct goodix_ext_attribute gesture_attrs[] = {
	__EXTMOD_ATTR(type, 0666, gsx_gesture_type_show,
			gsx_gesture_type_store),
	__EXTMOD_ATTR(enable, 0666, gsx_gesture_enable_show,
			gsx_gesture_enable_store),
	__EXTMOD_ATTR(data, 0444, gsx_gesture_data_show, NULL)
};

static int gsx_gesture_init(struct goodix_ts_data *ts,
		struct goodix_ext_module *module)
{
	if (!ts || !ts->hw_ops->gesture) {
		ts_err("gesture unsupported");
		return -EINVAL;
	}

	ts_info("gesture switch: ON");
	ts_debug("enable all gesture type");
	/* set all bit to 1 to enable all gesture wakeup */
	memset(ts->gsx_gesture->gesture_type, 0xff, GSX_GESTURE_TYPE_LEN);
	atomic_set(&ts->gsx_gesture->registered, 1);

	return 0;
}

static int gsx_gesture_exit(struct goodix_ts_data *ts,
		struct goodix_ext_module *module)
{
	if (!ts || !ts->hw_ops->gesture) {
		ts_err("gesture unsupported");
		return -EINVAL;
	}

	ts_info("gesture switch: OFF");
	ts_debug("disable all gesture type");
	memset(ts->gsx_gesture->gesture_type, 0x00, GSX_GESTURE_TYPE_LEN);
	atomic_set(&ts->gsx_gesture->registered, 0);

	return 0;
}

/**
 * gsx_gesture_ist - Gesture Irq handle
 * This functions is excuted when interrupt happended and
 * ic in doze mode.
 *
 * @ts: pointer to goodix_ts_data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_CANCEL_IRQEVT  stop execute
 */
static int gsx_gesture_ist(struct goodix_ts_data *ts,
		struct goodix_ext_module *module)
{
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	struct goodix_ts_event *gs_event = &ts->ts_event;
	int ret;

	if (atomic_read(&ts->plat_data->enabled))
		return EVT_CONTINUE;

	if (hw_ops->event_handler == NULL) {
		ts_err("hw_ops->event_handler is NULL");
		goto re_send_ges_cmd;
	}

	ret = hw_ops->event_handler(ts, gs_event);
	if (ret) {
		ts_err("failed get gesture data");
		goto re_send_ges_cmd;
	}

	if (gs_event->event_type & EVENT_EMPTY) {
		ts_err("force release all finger because event type is empty");
		goodix_ts_release_all_finger(ts);
	}

re_send_ges_cmd:
	gs_event->event_type = EVENT_INVALID;	// clear event type

	return EVT_CANCEL_IRQEVT;
}

/**
 * gsx_gesture_before_suspend - execute gesture suspend routine
 * This functions is excuted to set ic into doze mode
 *
 * @ts: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_IRQCANCLED  stop execute
 */
static int gsx_gesture_before_suspend(struct goodix_ts_data *ts,
		struct goodix_ext_module *module)
{
	int ret;
	const struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;

	goodix_set_custom_library(ts);

	ret = gsx_set_lowpowermode(ts, TO_LOWPOWER_MODE);
	if (ret < 0)
		ts_err("failed to enter lowpowermode");

	ts->lpm_coord_event_cnt = 0;
	hw_ops->irq_enable(ts, true);
	enable_irq_wake(ts->irq);

	/* LP mode no need ESD function */
	goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);

	return EVT_CANCEL_SUSPEND;
}

static int gsx_gesture_before_resume(struct goodix_ts_data *ts,
		struct goodix_ext_module *module)
{
	disable_irq_wake(ts->irq);
	gsx_set_lowpowermode(ts, TO_TOUCH_MODE);

	sec_input_set_grip_type(ts->bus->dev, ONLY_EDGE_HANDLER);

	return EVT_CANCEL_RESUME;
}

static struct goodix_ext_module_funcs gsx_gesture_funcs = {
	.irq_event = gsx_gesture_ist,
	.init = gsx_gesture_init,
	.exit = gsx_gesture_exit,
	.before_suspend = gsx_gesture_before_suspend,
	.before_resume = gsx_gesture_before_resume,
};

static void goodix_ext_sysfs_release(struct kobject *kobj)
{
	ts_info("Kobject released!");
}

#define to_ext_module(kobj)	container_of(kobj,\
		struct goodix_ext_module, kobj)
#define to_ext_attr(attr)	container_of(attr,\
		struct goodix_ext_attribute, attr)

static ssize_t goodix_ext_sysfs_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct goodix_ext_module *module = to_ext_module(kobj);
	struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->show)
		return ext_attr->show(module, buf);

	return -EIO;
}

static ssize_t goodix_ext_sysfs_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	struct goodix_ext_module *module = to_ext_module(kobj);
	struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->store)
		return ext_attr->store(module, buf, count);

	return -EIO;
}

static const struct sysfs_ops goodix_ext_ops = {
	.show = goodix_ext_sysfs_show,
	.store = goodix_ext_sysfs_store
};

static struct kobj_type goodix_ext_ktype = {
	.release = goodix_ext_sysfs_release,
	.sysfs_ops = &goodix_ext_ops,
};

int gesture_module_init(struct goodix_ts_data *ts)
{
	int ret;
	int i;

	ts->gsx_gesture = kzalloc(sizeof(struct gesture_module), GFP_KERNEL);
	if (!ts->gsx_gesture)
		return -ENOMEM;

	ts->gsx_gesture->ts = ts;
	ts->gsx_gesture->module.funcs = &gsx_gesture_funcs;
	ts->gsx_gesture->module.priority = EXTMOD_PRIO_GESTURE;
	ts->gsx_gesture->module.name = "Goodix_gsx_gesture";
	ts->gsx_gesture->module.priv_data = ts->gsx_gesture;

	atomic_set(&ts->gsx_gesture->registered, 0);
	rwlock_init(&ts->gsx_gesture->rwlock);

	/* gesture sysfs init */
	ret = kobject_init_and_add(&ts->gsx_gesture->module.kobj,
			&goodix_ext_ktype, &ts->pdev->dev.kobj, "gesture");
	if (ret) {
		ts_err("failed create gesture sysfs node!");
		goto err_out;
	}

	for (i = 0; i < ARRAY_SIZE(gesture_attrs) && !ret; i++)
		ret = sysfs_create_file(&ts->gsx_gesture->module.kobj,
				&gesture_attrs[i].attr);
	if (ret) {
		ts_err("failed create gst sysfs files");
		while (--i >= 0)
			sysfs_remove_file(&ts->gsx_gesture->module.kobj,
					&gesture_attrs[i].attr);

		kobject_put(&ts->gsx_gesture->module.kobj);
		goto err_out;
	}

	module_initialized = true;
	ts_info("gesture module init success");
	goodix_gesture_enable(ts, 1);

	return 0;

err_out:
	ts_err("gesture module init failed!");
	kfree(ts->gsx_gesture);
	return ret;
}

void gesture_module_exit(struct goodix_ts_data *ts)
{
	int i;

	ts_info("gesture module exit");
	if (!module_initialized)
		return;

	goodix_gesture_enable(ts, 0);

	/* deinit sysfs */
	for (i = 0; i < ARRAY_SIZE(gesture_attrs); i++)
		sysfs_remove_file(&ts->gsx_gesture->module.kobj,
				&gesture_attrs[i].attr);

	kobject_put(&ts->gsx_gesture->module.kobj);
	kfree(ts->gsx_gesture);
	module_initialized = false;
}
