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

#ifdef CONFIG_SEC_VIB_NOTIFIER
#include <linux/vibrator/sec_vibrator_notifier.h>
#include "../utility/shub_utility.h"
#include "../sensorhub/shub_device.h"
#include "../comm/shub_comm.h"
#include "../sensormanager/shub_sensor_type.h"

int motor_coef;
int motor_state;
bool cb_registered;
struct work_struct work_motor;

static void shub_queue_motor_work(int state)
{
	shub_infof("motor_state : %d", state);
	motor_state = state;

	shub_queue_work(&work_motor);
}

static int vibrator_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	struct vib_notifier_context *vib_data = (struct vib_notifier_context *)data;
	bool enable = (bool)action;
	int index = vib_data->index;
	int duration = vib_data->timeout;

	shub_infof("enable %d, index %d, duration %d", enable, index, duration);
	if ((enable ==  true && duration >= 20) || (enable == false && motor_state))
		shub_queue_motor_work(enable);

	return 0;
}

static struct notifier_block vib_notifier = {
	.notifier_call = vibrator_notifier,
};

static void shub_motor_work_func(struct work_struct *work)
{
	int ret = 0;
	char buf = motor_coef;
	char sub_cmd;

	if (motor_state)
		sub_cmd = MOTOR_ON;
	else
		sub_cmd = MOTOR_OFF;

	ret = shub_send_command(CMD_SETVALUE, TYPE_HUB, sub_cmd, &buf, sizeof(buf));

	if (ret < 0)
		shub_errf("send motor state failed : %d", motor_state);
	else
		shub_infof("send motor_state success : %d(%d)", motor_state, buf);
}

void init_shub_motor_callback(void)
{
	if (cb_registered || motor_coef == 0)
		return;

	INIT_WORK(&work_motor, shub_motor_work_func);
	motor_state = 0;
	sec_vib_notifier_register(&vib_notifier);
	cb_registered = true;

	shub_info();
}

void remove_shub_motor_callback(void)
{
	if (!cb_registered)
		return;

	cb_registered = false;
	sec_vib_notifier_unregister(&vib_notifier);
	cancel_work_sync(&work_motor);
}

void set_motor_coef(int coef)
{
	int prev_motor_coef = motor_coef;

	if (coef >= 100) {
		shub_errf("invalid value %d", coef);
		return;
	}

	if (coef == motor_coef) {
		return;
	}

	motor_coef = coef;

	if (coef == 0)
		remove_shub_motor_callback();
	else if (prev_motor_coef == 0)
		init_shub_motor_callback();

	shub_infof("%d", motor_coef);
}

int get_motor_coef(void)
{
	return motor_coef;
}

void sync_motor_state(void)
{
	if (motor_coef == 0)
		return;

	shub_infof("%d", motor_state);
	shub_queue_motor_work(motor_state);
}

#else
void init_shub_motor_callback(void) {}
void remove_shub_motor_callback(void) {}
void sync_motor_state(void) {}
void set_motor_coef(int coef) {}
int get_motor_coef(void) {return 0; }
#endif
