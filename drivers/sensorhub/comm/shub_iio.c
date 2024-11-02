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

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/buffer_impl.h>
#include <linux/iio/events.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/types.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "../sensorhub/shub_device.h"
#include "../utility/shub_wakelock.h"
#include "../utility/shub_utility.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "shub_kfifo_buf.h"

#if defined(CONFIG_SHUB_KUNIT)
#include <kunit/mock.h>
#define __mockable __weak
#define __visible_for_testing
#else
#define __mockable
#define __visible_for_testing static
#endif

#define SCONTEXT_DATA_LEN       56
#define SCONTEXT_HEADER_LEN     8

#define IIO_CHANNEL            -1
#define IIO_SCAN_INDEX          0
#define IIO_SIGN               's'
#define IIO_SHIFT               0

struct iio_probe_device {
	int type;
	char *name;
	int report_event_size;
};

static struct iio_probe_device iio_probe_list[] = {
	{SENSOR_TYPE_ACCELEROMETER, "accelerometer_sensor", 6 },
	{SENSOR_TYPE_GEOMAGNETIC_FIELD, "geomagnetic_sensor", 13 },
	{SENSOR_TYPE_GYROSCOPE, "gyro_sensor", 6 },
	{SENSOR_TYPE_LIGHT, "light_sensor", 4 },
	{SENSOR_TYPE_PRESSURE, "pressure_sensor", 14 },
	{SENSOR_TYPE_PROXIMITY, "proximity_sensor", 1 },
	{SENSOR_TYPE_ROTATION_VECTOR, "rotation_vector_sensor", 17 },
	{SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED, "uncal_geomagnetic_sensor", 24 },
	{SENSOR_TYPE_GAME_ROTATION_VECTOR, "game_rotation_vector_sensor", 17 },
	{SENSOR_TYPE_GYROSCOPE_UNCALIBRATED, "uncal_gyro_sensor", 12 },
	{SENSOR_TYPE_SIGNIFICANT_MOTION, "sig_motion_sensor", 1 },
	{SENSOR_TYPE_STEP_DETECTOR, "step_det_sensor", 1 },
	{SENSOR_TYPE_STEP_COUNTER, "step_cnt_sensor", 12 },
	{SENSOR_TYPE_TILT_DETECTOR, "tilt_detector", 1 },
	{SENSOR_TYPE_PICK_UP_GESTURE, "pickup_gesture", 1 },
	{SENSOR_TYPE_DEVICE_ORIENTATION, "device_orientation", 1 },
	{SENSOR_TYPE_GEOMAGNETIC_POWER, "geomagnetic_power", 6 },
	{SENSOR_TYPE_INTERRUPT_GYRO, "interrupt_gyro_sensor", 6 },
	{SENSOR_TYPE_SCONTEXT, "scontext_iio", 64 },
	{SENSOR_TYPE_SENSORHUB, "sensorhub_sensor", 3 },
	{SENSOR_TYPE_LIGHT_CCT, "light_cct_sensor", 14 },
	{SENSOR_TYPE_CALL_GESTURE, "call_gesture", 1 },
	{SENSOR_TYPE_WAKE_UP_MOTION, "wake_up_motion", 1 },
	{SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS, "auto_brightness", 5 },
	{SENSOR_TYPE_VDIS_GYROSCOPE, "vdis_gyro_sensor", 6 },
	{SENSOR_TYPE_POCKET_MODE_LITE, "pocket_mode_lite", 5 },
	{SENSOR_TYPE_POCKET_MODE, "pocket_mode", 58 },
	{SENSOR_TYPE_POCKET_POS_MODE, "pocket_pos_mode", 15 },
	{SENSOR_TYPE_PROTOS_MOTION, "protos_motion", 1 },
	{SENSOR_TYPE_FLIP_COVER_DETECTOR, "flip_cover_detector", 24 },
	{SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED, "uncal_accel_sensor", 12 },
	{SENSOR_TYPE_AOIS, "aois_sensor", 0 },
	{SENSOR_TYPE_SUPER_STEADY_GYROSCOPE, "super_steady_gyro_sensor", 6 },
	{SENSOR_TYPE_DEVICE_ORIENTATION_WU, "device_orientation_wu", 1 },
	{SENSOR_TYPE_SAR_BACKOFF_MOTION, "sar_backoff_motion", 1 },
	{SENSOR_TYPE_LIGHT_SEAMLESS, "light_seamless_sensor", 4 },
	{SENSOR_TYPE_LED_COVER_EVENT, "led_cover_event_sensor", 1 },
	{SENSOR_TYPE_LIGHT_IR, "light_ir_sensor", 24 },
	{SENSOR_TYPE_DROP_CLASSIFIER, "drop_classifier", 25 },
	{SENSOR_TYPE_SEQUENTIAL_STEP, "sequential_step", 4 },
};

struct shub_iio_device {
	int type;
	struct iio_chan_spec iio_channel;
	struct iio_dev* indio_dev;
};

static struct shub_iio_device *iio_list[SENSOR_TYPE_LEGACY_MAX];

static struct iio_dev* get_iio_device(int type)
{
	if (type < 0 || type >= SENSOR_TYPE_LEGACY_MAX)
		return NULL;

	return iio_list[type] ? iio_list[type]->indio_dev : NULL;
}

static int shub_preenable(struct iio_dev *indio_dev)
{
	return 0;
}

static int shub_predisable(struct iio_dev *indio_dev)
{
	return 0;
}

static const struct iio_buffer_setup_ops shub_iio_buffer_setup_ops = {
	.preenable = &shub_preenable,
	.predisable = &shub_predisable,
};

static int shub_iio_configure_buffer(struct iio_dev *indio_dev, int bytes)
{
	struct iio_buffer *buffer;

	buffer = shub_iio_kfifo_allocate();
	if (!buffer)
		return -ENOMEM;

	buffer->scan_timestamp = true;
	buffer->bytes_per_datum = bytes;
	buffer->scan_mask = bitmap_zalloc(1, GFP_KERNEL);
	set_bit(0, buffer->scan_mask);

	iio_device_attach_buffer(indio_dev, buffer);

	indio_dev->setup_ops = &shub_iio_buffer_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_SOFTWARE;

	return 0;
}

static void *init_indio_device(struct device *dev, const struct iio_info *info,
			       const struct iio_chan_spec *channels, const char *device_name, const int bytes)
{
	struct iio_dev *indio_dev;
	int ret = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0))
	indio_dev = iio_device_alloc(dev, sizeof(*dev));
#else
	indio_dev = iio_device_alloc(0);
#endif
	if (!indio_dev)
		goto err_alloc;

	indio_dev->name = device_name;
	indio_dev->dev.parent = dev;
	indio_dev->info = info;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0))
	indio_dev->driver_module = THIS_MODULE;
#endif
	indio_dev->channels = channels;
	indio_dev->num_channels = 1;
	indio_dev->modes = INDIO_DIRECT_MODE;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	indio_dev->currentmode = INDIO_DIRECT_MODE;
#endif

	ret = shub_iio_configure_buffer(indio_dev, bytes);
	if (ret) {
		goto err_config_ring;
	}

	ret = iio_device_register(indio_dev);
	if (ret) {
		goto err_register_device;
	}

	return indio_dev;

err_register_device:
	shub_errf("fail to register %s device", device_name);
	shub_iio_kfifo_free(indio_dev->buffer);
err_config_ring:
	shub_errf("failed to configure %s buffer", indio_dev->name);
	iio_device_unregister(indio_dev);
err_alloc:
	shub_errf("fail to allocate memory for iio %s device", device_name);
	return NULL;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0))
static const struct iio_info indio_info = {
	.driver_module = THIS_MODULE,
};
#else
static const struct iio_info indio_info;
#endif

void remove_indio_dev(void)
{
	int i;

	for (i = 0 ; i < SENSOR_TYPE_LEGACY_MAX ; i++) {
		if (iio_list[i]) {
			iio_device_unregister(iio_list[i]->indio_dev);
			kfree(iio_list[i]);
		}
	}
}

static inline void set_channel_spec(struct iio_chan_spec *iio_channel, int realbits_size, int repeat_size)
{
	iio_channel->type = IIO_TIMESTAMP;
	iio_channel->channel = IIO_CHANNEL;
	iio_channel->scan_index = IIO_SCAN_INDEX;
	iio_channel->scan_type.sign = IIO_SIGN;
	iio_channel->scan_type.realbits = realbits_size;
	iio_channel->scan_type.storagebits = realbits_size;
	iio_channel->scan_type.shift = IIO_SHIFT;
	iio_channel->scan_type.repeat = repeat_size;
}

/* this function should be called when sensor list of sensor manager is existed */
int __mockable initialize_indio_dev(struct device *dev)
{
	int timestamp_len = sizeof(u64);
	int type;
	int iter;
	int realbits_size = 0;
	int repeat_size = 0;
	int bytes = 0;
	struct iio_probe_device iio_dev_probe;

	for (iter = 0 ; iter < (sizeof(iio_probe_list)/sizeof(iio_dev_probe)); iter++) {
		iio_dev_probe = iio_probe_list[iter];
		shub_infof("type : %d name : %s size : %d",
					iio_dev_probe.type, iio_dev_probe.name, iio_dev_probe.report_event_size);

		type = iio_dev_probe.type;
		bytes = iio_dev_probe.report_event_size + timestamp_len;
		realbits_size =  bytes * BITS_PER_BYTE;
		repeat_size = 1;

		while ((realbits_size / repeat_size > 255) && (realbits_size % repeat_size == 0))
			repeat_size++;
		realbits_size /= repeat_size;

		iio_list[type] = (struct shub_iio_device *)kzalloc(sizeof(struct shub_iio_device), GFP_KERNEL);
		if (!iio_list[type]) {
			shub_errf("fail to malloc %s iio dev", iio_dev_probe.name);
			continue;
		}
		set_channel_spec(&iio_list[type]->iio_channel, realbits_size, repeat_size);
		iio_list[type]->indio_dev = (struct iio_dev *)init_indio_device(dev, &indio_info, &iio_list[type]->iio_channel, iio_dev_probe.name, bytes);
		if (!iio_list[type]->indio_dev) {
			shub_errf("fail to init_indio_device %s", iio_dev_probe.name);
			kfree(iio_list[type]);
			iio_list[type] = NULL;
		}
	}

	return 0;
}

void shub_report_sensordata(int type, u64 timestamp, char *data, int data_len)
{
	struct iio_dev *indio_dev = get_iio_device(type);
	struct shub_sensor *sensor = get_sensor(type);
	char *buf;

	if (!sensor || !indio_dev)
		return;

	buf = kzalloc(sensor->report_event_size + sizeof(timestamp), GFP_KERNEL);
	if (!buf) {
		shub_errf("fail to alloc memory");
		return;
	}

	if (data && data_len > 0)
		memcpy(buf, data, data_len);

	if (sensor->spec.is_wake_up)
		shub_wake_lock_timeout(300);

	memcpy(buf + data_len, &timestamp, sizeof(timestamp));
	mutex_lock(&indio_dev->mlock);
	iio_push_to_buffers(indio_dev, buf);
	mutex_unlock(&indio_dev->mlock);

	kfree(buf);
}

void remove_empty_dev(void)
{
	int i;

	for (i = 0 ; i < SENSOR_TYPE_LEGACY_MAX ; i++) {
		if (iio_list[i] && get_sensor(i) == NULL) {
			iio_device_unregister(iio_list[i]->indio_dev);
			shub_infof("type %d", i);
			kfree(iio_list[i]);
			iio_list[i] = NULL;
		}
	}
}
