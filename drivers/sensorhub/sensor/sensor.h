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

#ifndef __SENSOR_H_
#define __SENSOR_H_

int init_accelerometer(bool en);
int init_accelerometer_uncal(bool en);
int init_magnetometer(bool en);
int init_magnetometer_uncal(bool en);
int init_flip_cover_detector(bool en);
int init_gyroscope(bool en);
int init_gyroscope_uncal(bool en);
int init_light(bool en);
int init_light_cct(bool en);
int init_light_autobrightness(bool en);
int init_light_seamless(bool en);
int init_led_cover_event(bool en);
int init_proximity(bool en);
int init_proximity_raw(bool en);
int init_proximity_calibration(bool en);
int init_pressure(bool en);
int init_step_counter(bool en);
int init_scontext(bool en);
int init_interrupt_gyroscope(bool en);
int init_vdis_gyroscope(bool en);
int init_super_steady_gyroscope(bool en);
int init_step_detector(bool en);
int init_magnetometer_power(bool en);
int init_significant_motion(bool en);
int init_tilt_detector(bool en);
int init_pick_up_gesture(bool en);
int init_call_gesture(bool en);
int init_wake_up_motion(bool en);
int init_protos_motion(bool en);
int init_pocket_mode(bool en);
int init_pocket_mode_lite(bool en);
int init_pocket_pos_mode(bool en);
int init_super(bool en);
int init_hub_debugger(bool en);
int init_device_orientation(bool en);
int init_device_orientation_wu(bool en);
int init_sar_backoff_motion(bool en);
int init_pogo_request_handler(bool en);
int init_aois(bool en);
int init_rotation_vector(bool en);
int init_game_rotation_vector(bool en);
int init_light_ir(bool en);
int init_drop_classifier(bool en);
int init_sequential_step(bool en);
#endif
