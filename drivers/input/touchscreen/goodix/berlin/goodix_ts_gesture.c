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

#define QUERYBIT(longlong, bit) (!!(longlong[bit/8] & (1 << bit%8)))

#define GSX_GESTURE_TYPE_LEN	32

/*
 * struct gesture_module - gesture module data
 * @registered: module register state
 * @sysfs_node_created: sysfs node state
 * @gesture_type: valid gesture type, each bit represent one gesture type
 * @gesture_data: store latest gesture code get from irq event
 * @gesture_ts_cmd: gesture command data
 */
struct gesture_module {
	atomic_t registered;
	rwlock_t rwlock;
	u8 gesture_type[GSX_GESTURE_TYPE_LEN];
	u8 gesture_data;
	struct goodix_ext_module module;
};

static struct gesture_module *gsx_gesture; /*allocated in gesture init module*/
static bool module_initialized;


int goodix_gesture_enable(int enable)
{
	int ret = 0;

	if (!module_initialized)
		return 0;

	if (enable) {
		if (atomic_read(&gsx_gesture->registered))
			ts_info("gesture module has been already registered");
		else
			ret = goodix_register_ext_module_no_wait(&gsx_gesture->module);
	} else {
		if (!atomic_read(&gsx_gesture->registered))
			ts_info("gesture module has been already unregistered");
		else
			ret = goodix_unregister_ext_module(&gsx_gesture->module);
	}

	return ret;
}

static void gsx_set_utc_sponge(struct goodix_ts_core *cd)
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

	ret = cd->hw_ops->write_to_sponge(cd, SEC_TS_CMD_SPONGE_OFFSET_UTC, data, 4);
	if (ret < 0)
		ts_err("failed to write UTC");
}

int gsx_set_lowpowermode(void *data, u8 mode)
{
	struct goodix_ts_core *cd = (struct goodix_ts_core *)data;
	int ret = 0;

	ts_info("%s[%X]", mode == TO_LOWPOWER_MODE ? "ENTER" : "EXIT", cd->plat_data->lowpower_mode);

	if (mode) {
		gsx_set_utc_sponge(cd);

		/* switch gesture mode */
		ret = cd->hw_ops->gesture(cd, true);
		if (ret < 0)
			ts_err("failed to switch gesture mode");
		cd->plat_data->power_state = SEC_INPUT_STATE_LPM;
	} else {
		/* switch coor mode */
		ret = cd->hw_ops->gesture(cd, false);
		if (ret < 0)
			ts_err("failed to switch coor mode");
		cd->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;
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
	int count = 0, i, ret = 0;
	unsigned char *type;

	type = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!type)
		return -ENOMEM;
	read_lock(&gsx_gesture->rwlock);
	for (i = 0; i < 256; i++) {
		if (QUERYBIT(gsx_gesture->gesture_type, i)) {
			count += scnprintf(type + count,
					PAGE_SIZE, "%02x,", i);
		}
	}
	if (count > 0)
		ret = scnprintf(buf, PAGE_SIZE, "%s\n", type);
	read_unlock(&gsx_gesture->rwlock);

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
	int i;

	if (count <= 0 || count > 256 || buf == NULL) {
		ts_err("Parameter error");
		return -EINVAL;
	}

	write_lock(&gsx_gesture->rwlock);
	memset(gsx_gesture->gesture_type, 0, GSX_GESTURE_TYPE_LEN);
	for (i = 0; i < count; i++)
		gsx_gesture->gesture_type[buf[i]/8] |= (0x1 << buf[i]%8);
	write_unlock(&gsx_gesture->rwlock);

	return count;
}

static ssize_t gsx_gesture_enable_show(struct goodix_ext_module *module,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			atomic_read(&gsx_gesture->registered));
}

static ssize_t gsx_gesture_enable_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	bool val;
	int ret;

	ret = strtobool(buf, &val);
	if (ret < 0)
		return ret;

	if (val) {
		ret = goodix_gesture_enable(1);
		return ret ? ret : count;
	} else {
		ret = goodix_gesture_enable(0);
		return ret ? ret : count;
	}
}

static ssize_t gsx_gesture_data_show(struct goodix_ext_module *module,
		char *buf)
{
	ssize_t count;

	read_lock(&gsx_gesture->rwlock);
	count = scnprintf(buf, PAGE_SIZE, "gesture type code:0x%x\n",
			gsx_gesture->gesture_data);
	read_unlock(&gsx_gesture->rwlock);

	return count;
}

const struct goodix_ext_attribute gesture_attrs[] = {
	__EXTMOD_ATTR(type, 0666, gsx_gesture_type_show,
			gsx_gesture_type_store),
	__EXTMOD_ATTR(enable, 0666, gsx_gesture_enable_show,
			gsx_gesture_enable_store),
	__EXTMOD_ATTR(data, 0444, gsx_gesture_data_show, NULL)
};

static int gsx_gesture_init(struct goodix_ts_core *cd,
		struct goodix_ext_module *module)
{
	if (!cd || !cd->hw_ops->gesture) {
		ts_err("gesture unsupported");
		return -EINVAL;
	}

	ts_info("gesture switch: ON");
	ts_debug("enable all gesture type");
	/* set all bit to 1 to enable all gesture wakeup */
	memset(gsx_gesture->gesture_type, 0xff, GSX_GESTURE_TYPE_LEN);
	atomic_set(&gsx_gesture->registered, 1);

	return 0;
}

static int gsx_gesture_exit(struct goodix_ts_core *cd,
		struct goodix_ext_module *module)
{
	if (!cd || !cd->hw_ops->gesture) {
		ts_err("gesture unsupported");
		return -EINVAL;
	}

	ts_info("gesture switch: OFF");
	ts_debug("disable all gesture type");
	memset(gsx_gesture->gesture_type, 0x00, GSX_GESTURE_TYPE_LEN);
	atomic_set(&gsx_gesture->registered, 0);

	return 0;
}

/**
 * gsx_gesture_ist - Gesture Irq handle
 * This functions is excuted when interrupt happended and
 * ic in doze mode.
 *
 * @cd: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_CANCEL_IRQEVT  stop execute
 */
static int gsx_gesture_ist(struct goodix_ts_core *cd,
		struct goodix_ext_module *module)
{
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	struct goodix_ts_event *gs_event = &cd->ts_event;
	int ret;

	if (atomic_read(&cd->suspended) == 0)
		return EVT_CONTINUE;

	if (hw_ops->event_handler == NULL) {
		ts_err("hw_ops->event_handler is NULL");
		goto re_send_ges_cmd;
	}

	ret = hw_ops->event_handler(cd, gs_event);
	if (ret) {
		ts_err("failed get gesture data");
		goto re_send_ges_cmd;
	}

	if (gs_event->event_type & EVENT_EMPTY) {
		ts_err("force release all finger because event type is empty");
		goodix_ts_release_all_finger(cd);
	}

re_send_ges_cmd:
	gs_event->event_type = EVENT_INVALID;	// clear event type

	return EVT_CANCEL_IRQEVT;
}

/**
 * gsx_gesture_before_suspend - execute gesture suspend routine
 * This functions is excuted to set ic into doze mode
 *
 * @cd: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_IRQCANCLED  stop execute
 */
static int gsx_gesture_before_suspend(struct goodix_ts_core *cd,
		struct goodix_ext_module *module)
{
	int ret;
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	goodix_set_custom_library(cd);

	ret = gsx_set_lowpowermode(cd, TO_LOWPOWER_MODE);
	if (ret < 0)
		ts_err("failed to enter lowpowermode");

	cd->lpm_coord_event_cnt = 0;
	hw_ops->irq_enable(cd, true);
	enable_irq_wake(cd->irq);

	/* LP mode no need ESD function */
	goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);

	if (cd->plat_data->sense_off_when_cover_closed && cd->flip_enable)
		cd->hw_ops->sense_off(cd, 1);

	return EVT_CANCEL_SUSPEND;
}

static int gsx_gesture_before_resume(struct goodix_ts_core *cd,
		struct goodix_ext_module *module)
{
	disable_irq_wake(cd->irq);
	gsx_set_lowpowermode(cd, TO_TOUCH_MODE);

	sec_input_set_grip_type(cd->bus->dev, ONLY_EDGE_HANDLER);

	return EVT_CANCEL_RESUME;
}

static struct goodix_ext_module_funcs gsx_gesture_funcs = {
	.irq_event = gsx_gesture_ist,
	.init = gsx_gesture_init,
	.exit = gsx_gesture_exit,
	.before_suspend = gsx_gesture_before_suspend,
	.before_resume = gsx_gesture_before_resume,
};

int gesture_module_init(void)
{
	int ret;
	int i;
	struct kobject *def_kobj = goodix_get_default_kobj();
	struct kobj_type *def_kobj_type = goodix_get_default_ktype();

	gsx_gesture = kzalloc(sizeof(struct gesture_module), GFP_KERNEL);
	if (!gsx_gesture)
		return -ENOMEM;

	gsx_gesture->module.funcs = &gsx_gesture_funcs;
	gsx_gesture->module.priority = EXTMOD_PRIO_GESTURE;
	gsx_gesture->module.name = "Goodix_gsx_gesture";
	gsx_gesture->module.priv_data = gsx_gesture;

	atomic_set(&gsx_gesture->registered, 0);
	rwlock_init(&gsx_gesture->rwlock);

	/* gesture sysfs init */
	ret = kobject_init_and_add(&gsx_gesture->module.kobj,
			def_kobj_type, def_kobj, "gesture");
	if (ret) {
		ts_err("failed create gesture sysfs node!");
		goto err_out;
	}

	for (i = 0; i < ARRAY_SIZE(gesture_attrs) && !ret; i++)
		ret = sysfs_create_file(&gsx_gesture->module.kobj,
				&gesture_attrs[i].attr);
	if (ret) {
		ts_err("failed create gst sysfs files");
		while (--i >= 0)
			sysfs_remove_file(&gsx_gesture->module.kobj,
					&gesture_attrs[i].attr);

		kobject_put(&gsx_gesture->module.kobj);
		goto err_out;
	}

	module_initialized = true;
	ts_info("gesture module init success");
	goodix_gesture_enable(1);

	return 0;

err_out:
	ts_err("gesture module init failed!");
	kfree(gsx_gesture);
	return ret;
}

void gesture_module_exit(void)
{
	int i;

	ts_info("gesture module exit");
	if (!module_initialized)
		return;

	goodix_gesture_enable(0);

	/* deinit sysfs */
	for (i = 0; i < ARRAY_SIZE(gesture_attrs); i++)
		sysfs_remove_file(&gsx_gesture->module.kobj,
				&gesture_attrs[i].attr);

	kobject_put(&gsx_gesture->module.kobj);
	kfree(gsx_gesture);
	module_initialized = false;
}
