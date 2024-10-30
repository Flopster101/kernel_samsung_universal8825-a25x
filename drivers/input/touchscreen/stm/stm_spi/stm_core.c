/* drivers/input/sec_input/stm/stm_core.c
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "stm_dev.h"
#include "stm_reg.h"

struct stm_ts_data *g_ts;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
void stm_ts_secure_work(struct work_struct *work)
{
	struct stm_ts_data *ts = container_of(work, struct stm_ts_data,
			secure_work.work);

	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_info(true, &ts->client->dev, "%s: gpio: %d\n", __func__, gpio_get_value(ts->plat_data->irq_gpio));
		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		schedule_delayed_work(&ts->secure_work, msecs_to_jiffies(300));
	}
}

irqreturn_t secure_filter_interrupt(struct stm_ts_data *ts)
{
	mutex_lock(&ts->secure_lock);
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		cancel_delayed_work_sync(&ts->secure_work);
		if (atomic_cmpxchg(&ts->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		} else {
			input_info(true, &ts->client->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&ts->secure_pending_irqs));
		}

		schedule_delayed_work(&ts->secure_work, msecs_to_jiffies(300));

		mutex_unlock(&ts->secure_lock);
		return IRQ_HANDLED;
	}

	mutex_unlock(&ts->secure_lock);
	return IRQ_NONE;
}

/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_runtime : set rpm_resume or rpm_ilde
 */
ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->secure_enabled));
}

ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *addr, const char *buf, size_t count)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	unsigned long data;

	if (count > 2) {
		input_err(true, &ts->client->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	ret = kstrtoul(buf, 10, &data);
	if (ret != 0) {
		input_err(true, &ts->client->dev, "%s: failed to read:%d\n",
				__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		if (ts->reset_is_on_going) {
			input_err(true, &ts->client->dev, "%s: reset is on going because i2c fail\n", __func__);
			return -EBUSY;
		}

		/* Enable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
			input_err(true, &ts->client->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		sec_delay(200);
		
		/* syncronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		disable_irq(ts->client->irq);

		/* Release All Finger */
		stm_ts_release_all_finger(ts);

		if (pm_runtime_get_sync(ts->client->controller->dev.parent) < 0) {
			enable_irq(ts->irq);
			input_err(true, &ts->client->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif
		reinit_completion(&ts->secure_powerdown);
		reinit_completion(&ts->secure_interrupt);

		atomic_set(&ts->secure_enabled, 1);
		atomic_set(&ts->secure_pending_irqs, 0);

		enable_irq(ts->irq);

		input_info(true, &ts->client->dev, "%s: secure touch enable\n", __func__);

		schedule_delayed_work(&ts->secure_work, msecs_to_jiffies(500));
	} else if (data == 0) {
		cancel_delayed_work_sync(&ts->secure_work);
		/* Disable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
			input_err(true, &ts->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_delay(200);

		pm_runtime_put_sync(ts->client->controller->dev.parent);
		atomic_set(&ts->secure_enabled, 0);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		sec_delay(10);

		stm_ts_irq_thread(ts->client->irq, ts);
		complete(&ts->secure_interrupt);
		complete_all(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: secure touch disable\n", __func__);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
	} else {
		input_err(true, &ts->client->dev, "%s: unsupport value:%ld\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	int val = 0;

	mutex_lock(&ts->secure_lock);
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
		mutex_unlock(&ts->secure_lock);
		input_err(true, &ts->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, -1, 0) == -1) {
		mutex_unlock(&ts->secure_lock);
		input_err(true, &ts->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, 1, 0) == 1) {
		val = 1;
		input_err(true, &ts->client->dev, "%s: pending irq is %d\n",
				__func__, atomic_read(&ts->secure_pending_irqs));
	}

	mutex_unlock(&ts->secure_lock);
	complete(&ts->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

ssize_t secure_ownership_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1");
}

int secure_touch_init(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	init_completion(&ts->secure_interrupt);
	init_completion(&ts->secure_powerdown);

	return 0;
}

void secure_touch_stop(struct stm_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->secure_enabled)) {
		atomic_set(&ts->secure_pending_irqs, -1);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		if (stop)
			wait_for_completion_interruptible(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: %d\n", __func__, stop);
	}
}

static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		secure_touch_enable_show, secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO, secure_touch_show, NULL);
static DEVICE_ATTR(secure_ownership, S_IRUGO, secure_ownership_show, NULL);
static struct attribute *secure_attr[] = {
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	&dev_attr_secure_ownership.attr,
	NULL,
};

static struct attribute_group secure_attr_group = {
	.attrs = secure_attr,
};
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_i2c_lock(struct i2c_adapter *adap);
extern int stui_i2c_unlock(struct i2c_adapter *adap);

int stm_stui_tsp_enter(void)
{
	struct stm_ts_data *ts = g_ts;
	int ret = 0;

	if (!ts)
		return -EINVAL;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif

	disable_irq(ts->irq);
	stm_ts_release_all_finger(ts);

	ret = stui_i2c_lock(ts->client->adapter);
	if (ret) {
		pr_err("[STUI] stui_i2c_lock failed : %d\n", ret);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
		enable_irq(ts->irq);
		return -1;
	}

	return 0;
}

int stm_stui_tsp_exit(void)
{
	struct stm_ts_data *ts = g_ts;
	int ret = 0;

	if (!ts)
		return -EINVAL;

	ret = stui_i2c_unlock(ts->client->adapter);
	if (ret)
		pr_err("[STUI] stui_i2c_unlock failed : %d\n", ret);

	enable_irq(ts->irq);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif

	return ret;
}
#endif

int stm_ts_spi_write(struct stm_ts_data *ts, u8 *reg, int tlen, u8 *data, int len)
{
	struct spi_message *m;
	struct spi_transfer *t;
	char *tbuf;
	int ret = -ENOMEM;
	int wlen;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Sensor stopped\n", __func__);
		return -EIO;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev, "%s: TUI is enabled\n", __func__);
		return -EBUSY;
	}
#endif
	m = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m)
		return -ENOMEM;

	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!t) {
		kfree(m);
		return -ENOMEM;
	}

	tbuf = kzalloc(tlen + len + 1, GFP_KERNEL);
	if (!tbuf) {
		kfree(m);
		kfree(t);
		return -ENOMEM;
	}

	switch (reg[0]) {
		case STM_TS_CMD_SCAN_MODE://0xA0
		case STM_TS_CMD_FEATURE://0xA2
		case STM_TS_CMD_SYSTEM://0xA4
		case STM_TS_CMD_FRM_BUFF_R://0xA7
		case STM_TS_CMD_REG_W://0xFA
		case STM_TS_CMD_REG_R://0xFB
			memcpy(&tbuf[0], reg, tlen);
			memcpy(&tbuf[tlen], data, len);
			wlen = tlen + len;
			break;
		default:
			tbuf[0] = 0xC0;
			memcpy(&tbuf[1], reg, tlen);
			memcpy(&tbuf[1 + tlen], data, len);
			wlen = tlen + len + 1;
			break;
	}

	t->len = wlen;
	t->tx_buf = tbuf;

	mutex_lock(&ts->spi_mutex);

	spi_message_init(m);
	spi_message_add_tail(t, m);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	mutex_unlock(&ts->spi_mutex);

#if 0
	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		int i;

		pr_info("sec_input: spi: W: ");
		for (i = 0; i < wlen; i++)
			pr_cont("%02X ", tbuf[i]);
		pr_cont("\n");

	}
#endif

	kfree(tbuf);
	kfree(m);
	kfree(t);

	return ret;
}

int stm_ts_spi_read(struct stm_ts_data *ts, u8 *reg, int tlen, u8 *buf, int rlen)
{
	struct spi_message *m;
	struct spi_transfer *t;
	char *tbuf, *rbuf;
	int ret = -ENOMEM;
	int wmsg = 0;
	int buf_len = tlen;
	int mem_len;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Sensor stopped\n", __func__);
		return -EIO;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev, "%s: TUI is enabled\n", __func__);
		return -EBUSY;
	}
#endif
	m = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m)
		return -ENOMEM;

	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!t) {
		kfree(m);
		return -ENOMEM;
	}

	if (tlen > 3)
		mem_len = rlen + 1 + tlen;
	else
		mem_len = rlen + 1 + 3;

	tbuf = kzalloc(mem_len, GFP_KERNEL);
	if (!tbuf) {
		kfree(m);
		kfree(t);
		return -ENOMEM;
	}
	rbuf = kzalloc(mem_len, GFP_KERNEL);
	if (!rbuf) {
		kfree(m);
		kfree(t);
		kfree(tbuf);
		return -ENOMEM;
	}

	switch (reg[0]) {
		case 0x87:
		case 0xA7:
		case 0xA4:
		case 0xFA:
		case 0xFB:
			memcpy(tbuf, reg, tlen);
			wmsg = 0;
			break;
		case 0x75:
			buf_len = 3;
			tbuf[0] = 0xD1;
			if (tlen == 1) {
				tbuf[1] = 0x00;
				tbuf[2] = 0x00;
			} else if (tlen == 2) {
				tbuf[1] = 0x00;
				tbuf[2] = reg[0];
			} else if (tlen == 3) {
				tbuf[1] = reg[1];
				tbuf[2] = reg[0];
			} else {
				input_err(true, &ts->client->dev, "%s: tlen is mismatched: %d\n", __func__, tlen);
				kfree(m);
				kfree(t);
				kfree(tbuf);
				kfree(rbuf);
				return -EINVAL;
			}

			wmsg = 0;
			break;
		default:
			buf_len = 3;
			tbuf[0] = 0xD1;

			if (tlen == 1) {
				tbuf[1] = 0x00;
				tbuf[2] = 0x00;
			} else if (tlen == 2) {
				tbuf[1] = 0x00;
				tbuf[2] = reg[0];
			} else if (tlen == 3) {
				tbuf[1] = reg[1];
				tbuf[2] = reg[0];
			} else {
				input_err(true, &ts->client->dev, "%s: tlen is mismatched: %d\n", __func__, tlen);
				kfree(m);
				kfree(t);
				kfree(tbuf);
				kfree(rbuf);
				return -EINVAL;
			}

			wmsg = 1;
			break;
	}

	if (wmsg)
		stm_ts_spi_write(ts, reg, 1, NULL, 0);

	mutex_lock(&ts->spi_mutex);

	spi_message_init(m);

	t->len = rlen + 1 + buf_len;
	t->tx_buf = tbuf;
	t->rx_buf = rbuf;
	t->delay_usecs = 10;

	spi_message_add_tail(t, m);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	mutex_unlock(&ts->spi_mutex);

#if 0
	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		int i;

		pr_info("sec_input: spi: R: ");
		for (i = 0; i < buf_len; i++)
			pr_cont("%02X ", tbuf[i]);
		pr_cont("| ");
		for (i = 0; i < rlen + 1 + buf_len; i++) {
			if (i == buf_len + 1)
				pr_cont("|| ");
			pr_cont("%02X ", rbuf[i]);
		}
		pr_cont("\n");

	}
#endif
	memcpy(buf, &rbuf[1 + buf_len], rlen);

	kfree(rbuf);
	kfree(tbuf);
	kfree(m);
	kfree(t);

	return ret;
}

void stm_ts_reinit(void *data) 
{
	struct stm_ts_data *ts = (struct stm_ts_data *)data;
	int ret = 0;
	int retry = 3;

	do {
		ret = stm_ts_systemreset(ts, 0);
		if (ret < 0)
			stm_ts_reset(ts, 20);
		else
			break;
	} while (--retry);

	if (retry == 0) {
		input_err(true, &ts->client->dev, "%s: Failed to system reset\n", __func__);
		goto out;
	}

	input_info(true, &ts->client->dev,
		"%s: charger=0x%x, touch_functions=0x%x, Power mode=0x%x\n",
		__func__, ts->plat_data->wirelesscharger_mode, ts->plat_data->touch_functions, ts->plat_data->power_state);

	ts->plat_data->touch_noise_status = 0;
	ts->plat_data->touch_pre_noise_status = 0;
	ts->plat_data->wet_mode = 0;

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);
	stm_ts_release_all_finger(ts);

	if (ts->plat_data->wirelesscharger_mode != TYPE_WIRELESS_CHARGER_NONE) {
		ret = stm_ts_set_charger_mode(ts);
		if (ret < 0)
			goto out;
	}

	stm_ts_spi_set_cover_type(ts, ts->plat_data->touch_functions & STM_TS_TOUCHTYPE_BIT_COVER);

	stm_ts_set_custom_library(ts);
	stm_ts_set_press_property(ts);
	stm_ts_set_fod_finger_merge(ts);

	if (ts->plat_data->support_fod && ts->plat_data->fod_data.set_val)
		stm_ts_set_fod_rect(ts);

	/* Power mode */
	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		stm_ts_set_opmode(ts, STM_TS_OPMODE_LOWPOWER);
		sec_delay(50);
		if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_AOD)
			stm_ts_set_aod_rect(ts);
	} else {
		sec_input_set_grip_type(&ts->client->dev, GRIP_ALL_DATA);

		stm_ts_set_external_noise_mode(ts, EXT_NOISE_MODE_MAX);

		if (ts->plat_data->touchable_area) {
			ret = stm_ts_set_touchable_area(ts);
			if (ret < 0)
				goto out;
		}
	}

	if (ts->plat_data->ed_enable)
		stm_ts_ear_detect_enable(ts, ts->plat_data->ed_enable);
	if (ts->plat_data->pocket_mode)
		stm_ts_pocket_mode_enable(ts, ts->plat_data->pocket_mode);
out:
	stm_ts_set_scanmode(ts, ts->scan_mode);
	
}
/*
 * don't need it in interrupt handler in reality, but, need it in vendor IC for requesting vendor IC.
 * If you are requested additional i2c protocol in interrupt handler by vendor.
 * please add it in stm_ts_external_func.
 */
static void stm_ts_external_func(struct stm_ts_data *ts)
{
	sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_IN_IRQ);

}

static void stm_ts_coord_parsing(struct stm_ts_data *ts, struct stm_ts_event_coordinate *p_event_coord, u8 t_id)
{
	ts->plat_data->coord[t_id].id = t_id;
	ts->plat_data->coord[t_id].action = p_event_coord->tchsta;
	ts->plat_data->coord[t_id].x = (p_event_coord->x_11_4 << 4) | (p_event_coord->x_3_0);
	ts->plat_data->coord[t_id].y = (p_event_coord->y_11_4 << 4) | (p_event_coord->y_3_0);
	ts->plat_data->coord[t_id].z = p_event_coord->z & 0x3F;
	ts->plat_data->coord[t_id].ttype = p_event_coord->ttype_3_2 << 2 | p_event_coord->ttype_1_0 << 0;
	ts->plat_data->coord[t_id].major = p_event_coord->major;
	ts->plat_data->coord[t_id].minor = p_event_coord->minor;

	if (!ts->plat_data->coord[t_id].palm && (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_PALM))
		ts->plat_data->coord[t_id].palm_count++;

	ts->plat_data->coord[t_id].palm = (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_PALM);
	if (ts->plat_data->coord[t_id].palm)
		ts->plat_data->palm_flag |= (1 << t_id);
	else
		ts->plat_data->palm_flag &= ~(1 << t_id);

	ts->plat_data->coord[t_id].left_event = p_event_coord->left_event;

	ts->plat_data->coord[t_id].noise_level = max(ts->plat_data->coord[t_id].noise_level,
							p_event_coord->noise_level);
	ts->plat_data->coord[t_id].max_strength = max(ts->plat_data->coord[t_id].max_strength,
							p_event_coord->max_strength);
	ts->plat_data->coord[t_id].hover_id_num = max(ts->plat_data->coord[t_id].hover_id_num,
							(u8)p_event_coord->hover_id_num);

	if (ts->plat_data->coord[t_id].z <= 0)
		ts->plat_data->coord[t_id].z = 1;
}
static void stm_ts_fod_vi_event(struct stm_ts_data *ts)
{
	int ret = 0;

	ts->plat_data->fod_data.vi_data[0] = SEC_TS_CMD_SPONGE_FOD_POSITION;
	ret = ts->stm_ts_read_sponge(ts, ts->plat_data->fod_data.vi_data, ts->plat_data->fod_data.vi_size);
	if (ret < 0)
		input_info(true, &ts->client->dev, "%s: failed read fod vi\n", __func__);
}

static void stm_ts_gesture_event(struct stm_ts_data *ts, u8 *event_buff)
{
	struct stm_ts_gesture_status *p_gesture_status;
	int x, y;

	p_gesture_status = (struct stm_ts_gesture_status *)event_buff;

	x = (p_gesture_status->gesture_data_1 << 4) | (p_gesture_status->gesture_data_3 >> 4);
	y = (p_gesture_status->gesture_data_2 << 4) | (p_gesture_status->gesture_data_3 & 0x0F);

	if (p_gesture_status->stype == STM_TS_SPONGE_EVENT_SWIPE_UP) {
		sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_SPAY, 0, 0);
	} else if (p_gesture_status->stype == STM_TS_GESTURE_CODE_DOUBLE_TAP) {
		if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_AOD) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_AOD_DOUBLETAB, x, y);
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_DOUBLETAP_TO_WAKEUP) {
			input_info(true, &ts->client->dev, "%s: AOT\n", __func__);
			input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 1);
			input_sync(ts->plat_data->input_dev);
			input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 0);
			input_sync(ts->plat_data->input_dev);
		}
	} else if (p_gesture_status->stype  == STM_TS_SPONGE_EVENT_SINGLETAP) {
		sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_SINGLE_TAP, x, y);
	} else if (p_gesture_status->stype  == STM_TS_SPONGE_EVENT_PRESS) {
		if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_LONG ||
			p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_NORMAL) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_PRESS, x, y);
			input_info(true, &ts->client->dev, "%s: FOD %sPRESS\n",
					__func__, p_gesture_status->gesture_id ? "" : "LONG");
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_RELEASE) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_RELEASE, x, y);
			input_info(true, &ts->client->dev, "%s: FOD RELEASE\n", __func__);
			memset(ts->plat_data->fod_data.vi_data, 0x0, ts->plat_data->fod_data.vi_size);
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_OUT) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_OUT, x, y);
			input_info(true, &ts->client->dev, "%s: FOD OUT\n", __func__);
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_VI) {
			if ((ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS) && ts->plat_data->support_fod_lp_mode)
				stm_ts_fod_vi_event(ts);
		} else {
			input_info(true, &ts->client->dev, "%s: invalid id %d\n",
					__func__, p_gesture_status->gesture_id);
		}
	} else if (p_gesture_status->stype  == STM_TS_GESTURE_CODE_DUMPFLUSH) {
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
		if (ts->sponge_inf_dump) {
			if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
				if (p_gesture_status->gesture_id == STM_TS_SPONGE_DUMP_0)
					stm_ts_spi_sponge_dump_flush(ts, STM_TS_SPONGE_DUMP_0);
				if (p_gesture_status->gesture_id == STM_TS_SPONGE_DUMP_1)
					stm_ts_spi_sponge_dump_flush(ts, STM_TS_SPONGE_DUMP_1);
			} else {
				ts->sponge_dump_delayed_flag = true;
				ts->sponge_dump_delayed_area = p_gesture_status->gesture_id;
			}
		}
#endif
	} else if (p_gesture_status->stype  == STM_TS_VENDOR_EVENT_LARGEPALM) {
		input_info(true, &ts->client->dev, "%s: LARGE PALM %s\n", __func__,
			       	p_gesture_status->gesture_id == 1 ? "RELEASED" : "PRESSED");
		if (p_gesture_status->gesture_id == 0x01)
			input_report_key(ts->plat_data->input_dev, BTN_LARGE_PALM, 0);
		else
			input_report_key(ts->plat_data->input_dev, BTN_LARGE_PALM, 1);
		input_sync(ts->plat_data->input_dev);
	}
}

static void stm_ts_coordinate_event(struct stm_ts_data *ts, u8 *event_buff)
{
	struct stm_ts_event_coordinate *p_event_coord;
	u8 t_id = 0;

	if (ts->plat_data->power_state != SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev,
				"%s: device is closed %x %x %x %x %x %x %x %x\n", __func__,
				event_buff[0], event_buff[1], event_buff[2],
				event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7]);
		return;
	}

	p_event_coord = (struct stm_ts_event_coordinate *)event_buff;

	t_id = p_event_coord->tid;

	if (t_id < SEC_TS_SUPPORT_TOUCH_COUNT) {
		ts->plat_data->prev_coord[t_id] = ts->plat_data->coord[t_id];
		stm_ts_coord_parsing(ts, p_event_coord, t_id);

		if ((ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_NORMAL)
				|| (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_PALM)
				|| (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_WET)
				|| (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_GLOVE)) {
			sec_input_coord_event(&ts->client->dev, t_id);

		} else {
			input_err(true, &ts->client->dev,
					"%s: do not support coordinate type(%d)\n",
					__func__, ts->plat_data->coord[t_id].ttype);
		}
	} else {
		input_err(true, &ts->client->dev, "%s: tid(%d) is out of range\n", __func__, t_id);
	}
}

static void stm_ts_status_event(struct stm_ts_data *ts, u8 *event_buff)
{
	struct stm_ts_event_status *p_event_status;

	p_event_status = (struct stm_ts_event_status *)event_buff;

	if (p_event_status->stype > 0)
		input_info(true, &ts->client->dev, "%s: STATUS %x %x %x %x %x %x %x %x\n", __func__,
				event_buff[0], event_buff[1], event_buff[2],
				event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7]);

	if (p_event_status->stype == STM_TS_EVENT_STATUSTYPE_ERROR) {
		if (p_event_status->status_id == STM_TS_ERR_EVENT_QUEUE_FULL) {
			input_err(true, &ts->client->dev, "%s: IC Event Queue is full\n", __func__);
			stm_ts_release_all_finger(ts);
		} else if (p_event_status->status_id == STM_TS_ERR_EVENT_ESD) {
			input_err(true, &ts->client->dev, "%s: ESD detected\n", __func__);
			if (!ts->reset_is_on_going)
				schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
		}
	} else if (p_event_status->stype == STM_TS_EVENT_STATUSTYPE_INFO) {
		if (p_event_status->status_id == STM_TS_INFO_READY_STATUS) {
			if (p_event_status->status_data_1 == 0x10) {
				input_err(true, &ts->client->dev, "%s: IC Reset\n", __func__);
				/* if (!ts->reset_is_on_going)
					schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
				*/
			}
		} else if (p_event_status->status_id == STM_TS_INFO_WET_MODE) {
			ts->plat_data->wet_mode = p_event_status->status_data_1;
			input_info(true, &ts->client->dev, "%s: water wet mode %d\n",
				__func__, ts->plat_data->wet_mode);
			if (ts->plat_data->wet_mode)
				ts->plat_data->hw_param.wet_count++;
		} else if (p_event_status->status_id == STM_TS_INFO_NOISE_MODE) {
			ts->plat_data->touch_noise_status = (p_event_status->status_data_1 >> 4);

			input_info(true, &ts->client->dev, "%s: NOISE MODE %s[%02X]\n",
					__func__, ts->plat_data->touch_noise_status == 0 ? "OFF" : "ON",
					p_event_status->status_data_1);

			if (ts->plat_data->touch_noise_status)
				ts->plat_data->hw_param.noise_count++;
		} else if (p_event_status->status_id == STM_TS_INFO_LIGHTSENSOR_DETECT) {
			u8 press = 0, x = 0, y = 0;

			press = (p_event_status->status_data_5 >> 7) & 0x1;
			x = (p_event_status->status_data_1 << 4) | ((p_event_status->status_data_3 & 0xf0) >> 4);
			y = (p_event_status->status_data_2 << 4) | (p_event_status->status_data_3 & 0x0f);

			if (ts->plat_data->input_dev_proximity) {
				input_report_abs(ts->plat_data->input_dev_proximity, ABS_MT_CUSTOM,
						press ? STM_TS_PROTOS_EVENT_LIGHTSENSOR_DETECT : STM_TS_PROTOS_EVENT_LIGHTSENSOR_RELEASE);
				input_sync(ts->plat_data->input_dev_proximity);
			}
			input_info(true, &ts->client->dev, "%s: LIGHTSENSOR DETECT [%d])\n",
					__func__, press);
		}
	} else if (p_event_status->stype == STM_TS_EVENT_STATUSTYPE_VENDORINFO) {
		if (ts->plat_data->support_ear_detect) {
			if (p_event_status->status_id == 0x6A) {
				ts->hover_event = p_event_status->status_data_1;
				input_report_abs(ts->plat_data->input_dev_proximity, ABS_MT_CUSTOM, p_event_status->status_data_1);
				input_sync(ts->plat_data->input_dev_proximity);
				input_info(true, &ts->client->dev, "%s: proximity: %d\n", __func__, p_event_status->status_data_1);
			}
		}
	}
}

static int stm_ts_get_event(struct stm_ts_data *ts, u8 *data, int *remain_event_count)
{
	int ret = 0;
	u8 address = 0;

	address = STM_TS_READ_ONE_EVENT;
	ret = ts->stm_ts_spi_read(ts, &address, 1, (u8 *)data, STM_TS_EVENT_BUFF_SIZE);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
		return ret;
	}

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_ONEEVENT)
		input_info(true, &ts->client->dev, "ONE: %02X %02X %02X %02X %02X %02X %02X %02X\n",
				data[0], data[1],
				data[2], data[3],
				data[4], data[5],
				data[6], data[7]);

	if (data[0] == 0) {
		input_info(true, &ts->client->dev, "%s: event buffer is empty\n", __func__);
		return SEC_ERROR;
	}

	*remain_event_count = data[7] & 0x1F;

	if (*remain_event_count > MAX_EVENT_COUNT - 1) {
		input_err(true, &ts->client->dev, "%s: event buffer overflow\n", __func__);
		address = STM_TS_CMD_CLEAR_ALL_EVENT;
		ret = ts->stm_ts_spi_write(ts, &address, 1, NULL, 0); //guide
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: i2c write clear event failed\n", __func__);

		stm_ts_release_all_finger(ts);

		return SEC_ERROR;
	}

	if (*remain_event_count > 0) {
		address = STM_TS_READ_ALL_EVENT;
		ret = ts->stm_ts_spi_read(ts, &address, 1, &data[1 * STM_TS_EVENT_BUFF_SIZE],
				 (STM_TS_EVENT_BUFF_SIZE) * (*remain_event_count));
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
			return ret;
		}
	}

	return SEC_SUCCESS;
}

static int stm_ts_get_rawdata(struct stm_ts_data *ts)
{
	u8 reg[3];
	int ret;
	s16 *val;
	int ii;
	int jj;
	int kk = 0;
	int num;
	static int prev_touch_count;

	if (ts->raw_mode == 0)
		return 0;

	if (!ts->raw_addr_h && !ts->raw_addr_l)
		return 0;

	if (ts->raw_len == 0)
		return 0;

	if (!ts->raw_u8)
		return 0;

	if (!ts->raw)
		return 0;

	reg[0] = 0xA7;
	reg[1] = ts->raw_addr_h;
	reg[2] = ts->raw_addr_l;

	mutex_lock(&ts->raw_lock);
	if (prev_touch_count == 0 && ts->plat_data->touch_count != 0) {
		/* first press */
		ts->raw_irq_count = 1;
		ts->before_irq_count = 1;
	} else if (prev_touch_count != 0 && ts->plat_data->touch_count == 0) {
		/* all finger released */
		ts->raw_irq_count++;
		ts->before_irq_count = 0;
	} else if (prev_touch_count != 0 && ts->plat_data->touch_count != 0) {
		/* move */
		ts->raw_irq_count++;
		ts->before_irq_count++;
	}
	prev_touch_count = ts->plat_data->touch_count;

	ret = ts->stm_ts_spi_read(ts, reg, 3, ts->raw_u8, ts->tx_count * ts->rx_count * 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to get rawdata: %d\n", __func__, ret);
		mutex_unlock(&ts->raw_lock);
		return ret;
	}

	val = (s16 *)ts->raw_u8;

	for (ii = 0; ii < ts->tx_count; ii++) {
		for (jj = (ts->rx_count - 1); jj >= 0; jj--) {
			ts->raw[4 + jj * ts->tx_count + ii] = val[kk++];
		}
	}

	ts->raw[0] = ts->plat_data->coord[0].action == 0 ? 3 : ts->plat_data->coord[0].action;
	ts->raw[1] = ts->raw_irq_count;

	num = ts->raw_irq_count % 5;
	if (num == 0)
		memcpy(ts->raw_v0, ts->raw, ts->raw_len);
	else if (num == 1)
		memcpy(ts->raw_v1, ts->raw, ts->raw_len);
	else if (num == 2)
		memcpy(ts->raw_v2, ts->raw, ts->raw_len);
	else if (num == 3)
		memcpy(ts->raw_v3, ts->raw, ts->raw_len);
	else if (num == 4)
		memcpy(ts->raw_v4, ts->raw, ts->raw_len);

	input_info(true, &ts->client->dev, "%s: num: %d | %d | %d | %d | %d\n", __func__, num, ts->raw[0], ts->raw[1], ts->raw[2], ts->raw[3]);
/*
	kk = 0;
	for (jj = 0; jj < ts->rx_count; jj++) {
		pr_cont("[sec_input]: ");
		for (ii = 0; ii < ts->tx_count; ii++) {
			pr_cont(" %d", ts->raw[4 + kk++]);
		}
		pr_cont("\n");
	}
*/
	mutex_unlock(&ts->raw_lock);
	return ret;
}

irqreturn_t stm_ts_irq_thread(int irq, void *ptr)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)ptr;
	int ret;
	u8 event_id;
	u8 read_event_buff[MAX_EVENT_COUNT * STM_TS_EVENT_BUFF_SIZE] = {0};
	u8 *event_buff;
	int curr_pos;
	int remain_event_count;
	int raw_irq_flag = 0;

	ret = event_id = curr_pos = remain_event_count = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (secure_filter_interrupt(ts) == IRQ_HANDLED) {
		wait_for_completion_interruptible_timeout(&ts->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &ts->client->dev,
				"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return IRQ_HANDLED;
#endif

	ret = sec_input_handler_start(&ts->client->dev);
	if (ret < 0)
		return IRQ_HANDLED;

	ret = stm_ts_get_event(ts, read_event_buff, &remain_event_count);
	if (ret < 0)
		return IRQ_HANDLED;

	mutex_lock(&ts->eventlock);

	do {
		event_buff = &read_event_buff[curr_pos * STM_TS_EVENT_BUFF_SIZE];
		event_id = event_buff[0] & 0x3;
		if (ts->debug_flag & SEC_TS_DEBUG_PRINT_ALLEVENT)
			input_info(true, &ts->client->dev, "ALL: %02X %02X %02X %02X %02X %02X %02X %02X\n",
					event_buff[0], event_buff[1], event_buff[2], event_buff[3],
					event_buff[4], event_buff[5], event_buff[6], event_buff[7]);

		if (event_id == STM_TS_STATUS_EVENT) {
			stm_ts_status_event(ts, event_buff);
		} else if (event_id == STM_TS_COORDINATE_EVENT) {
			stm_ts_coordinate_event(ts, event_buff);
			raw_irq_flag = 1;
		} else if (event_id == STM_TS_GESTURE_EVENT) {
			stm_ts_gesture_event(ts, event_buff);
		} else if (event_id == STM_TS_VENDOR_EVENT) {
			input_info(true, &ts->client->dev,
				"%s: %s event %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
				__func__, (event_buff[1] == 0x01 ? "echo": ""), event_buff[0], event_buff[1], event_buff[2], event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7], event_buff[8], event_buff[9], event_buff[10], event_buff[11],
				event_buff[12], event_buff[13], event_buff[14], event_buff[15]);
		} else {
			input_info(true, &ts->client->dev,
					"%s: unknown event %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
						__func__, event_buff[0], event_buff[1], event_buff[2], event_buff[3], event_buff[4], event_buff[5],
						event_buff[6], event_buff[7], event_buff[8], event_buff[9], event_buff[10], event_buff[11],
						event_buff[12], event_buff[13], event_buff[14], event_buff[15]);
		}
		curr_pos++;
		remain_event_count--;
	} while (remain_event_count >= 0);

	stm_ts_external_func(ts);

	mutex_unlock(&ts->eventlock);

	if (raw_irq_flag) {
		stm_ts_get_rawdata(ts);
		sysfs_notify(&ts->sec.fac_dev->kobj, NULL, "raw_irq");
	}
	return IRQ_HANDLED;
}

int stm_ts_input_open(struct input_dev *dev)
{
	struct stm_ts_data *ts = input_get_drvdata(dev);
	struct irq_desc *desc = irq_to_desc(ts->irq);
	int ret;

	cancel_delayed_work_sync(&ts->work_read_info);

	mutex_lock(&ts->modechange);

	ts->plat_data->enabled = true;
	ts->plat_data->prox_power_off = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 0);
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	cancel_delayed_work_sync(&ts->switching_work);
#endif
	mutex_lock(&ts->switching_mutex);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		ts->plat_data->lpmode(ts, TO_TOUCH_MODE);
		sec_input_set_grip_type(&ts->client->dev, ONLY_EDGE_HANDLER);
	} else {
		ret = ts->plat_data->start_device(ts);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);
	}

	while (desc->depth > 0)
		enable_irq(ts->irq);

	mutex_unlock(&ts->switching_mutex);

	if (ts->fix_active_mode) 
		stm_ts_fix_active_mode(ts, true);

	sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_FORCE);

	mutex_unlock(&ts->modechange);

	cancel_delayed_work(&ts->work_print_info);
	ts->plat_data->print_info_cnt_open = 0;
	ts->plat_data->print_info_cnt_release = 0;
	if (!ts->plat_data->shutdown_called)
		schedule_work(&ts->work_print_info.work);
	ts->flip_status_prev = ts->flip_status_current;

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE) && IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	if (ts->plat_data->support_flex_mode && (ts->plat_data->support_dual_foldable == MAIN_TOUCH))
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_MAIN_TOUCH_ON, NULL);
#endif
	return 0;
}

void stm_ts_input_close(struct input_dev *dev)
{
	struct stm_ts_data *ts = input_get_drvdata(dev);

	cancel_delayed_work_sync(&ts->work_read_info);

	if (ts->plat_data->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return;
	}

	mutex_lock(&ts->modechange);

	ts->plat_data->enabled = false;

#ifdef TCLM_CONCEPT
	sec_tclm_debug_info(ts->tdata);
#endif
	cancel_delayed_work(&ts->work_print_info);
	sec_input_print_info(&ts->client->dev, ts->tdata);
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 1);
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	stui_cancel_session();
#endif

	if (ts->flip_status_prev != ts->flip_status_current || ts->plat_data->prox_power_off) {
		input_report_key(ts->plat_data->input_dev, KEY_INT_CANCEL, 1);
		input_sync(ts->plat_data->input_dev);
		input_report_key(ts->plat_data->input_dev, KEY_INT_CANCEL, 0);
		input_sync(ts->plat_data->input_dev);
	}

	cancel_delayed_work(&ts->reset_work);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	cancel_delayed_work_sync(&ts->switching_work);
#endif
	mutex_lock(&ts->switching_mutex);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	if (ts->plat_data->support_dual_foldable == MAIN_TOUCH && ts->flip_status_current == STM_TS_STATUS_FOLDING) {
		ts->plat_data->stop_device(ts);
	} else {
#endif
		if (ts->plat_data->lowpower_mode || ts->plat_data->ed_enable || ts->plat_data->pocket_mode || ts->plat_data->fod_lp_mode)
			ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
		else
			ts->plat_data->stop_device(ts);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	}
#endif

	mutex_unlock(&ts->switching_mutex);
	mutex_unlock(&ts->modechange);
}

int stm_ts_stop_device(void *data)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)data;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	if (ts->sec.fac_dev)
		get_lp_dump(ts->sec.fac_dev, NULL, NULL);

	mutex_lock(&ts->device_mutex);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: already power off\n", __func__);
		goto out;
	}

	disable_irq(ts->irq);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_OFF;

	stm_ts_locked_release_all_finger(ts);

	ts->plat_data->power(&ts->client->dev, false);
	ts->plat_data->pinctrl_configure(&ts->client->dev, false);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

int stm_ts_start_device(void *data)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)data;
	int ret = -1;
	u8 address = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	ts->plat_data->pinctrl_configure(&ts->client->dev, true);

	mutex_lock(&ts->device_mutex);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev, "%s: already power on\n", __func__);
		goto out;
	}

	stm_ts_locked_release_all_finger(ts);

	ts->plat_data->power(&ts->client->dev, true);

	sec_delay(20);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;
	ts->plat_data->touch_noise_status = 0;

	ret = stm_ts_wait_for_ready(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to wait_for_ready\n", __func__);
		goto err;
	}

	ret = stm_ts_read_chip_id(ts);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to read chip id\n", __func__);

	ts->plat_data->init(ts);

err:
	/* Sense_on */
	address = STM_TS_CMD_SENSE_ON;
	ret = ts->stm_ts_spi_write(ts, &address, 1, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to write Sense_on\n", __func__);

	enable_irq(ts->irq);
out:
	mutex_unlock(&ts->device_mutex);
	return ret;
}

static int stm_ts_hw_init(struct spi_device *client)
{
	struct stm_ts_data *ts = spi_get_drvdata(client);
	int ret = 0;
	int retry = 3;
//	u8 reg[3] = { 0 };
//	u8 data[STM_TS_EVENT_BUFF_SIZE] = { 0 };

	ts->plat_data->pinctrl_configure(&ts->client->dev, true);

	ts->plat_data->power(&ts->client->dev, true);
	if (!ts->plat_data->regulator_boot_on)
		sec_delay(TOUCH_POWER_ON_DWORK_TIME);

	if (ts->plat_data->gpio_spi_cs > 0) {
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);
		input_err(true, &ts->client->dev, "%s: cs gpio: %d\n",
					__func__, gpio_get_value(ts->plat_data->gpio_spi_cs));		
	}

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;

	ts->client->mode = SPI_MODE_0;
	input_err(true, &ts->client->dev, "%s: mode: %d, max_speed_hz: %d: cs: %d\n", __func__, ts->client->mode, ts->client->max_speed_hz, ts->plat_data->gpio_spi_cs);
	input_err(true, &ts->client->dev, "%s: chip_select: %d, bits_per_word: %d\n", __func__, ts->client->chip_select, ts->client->bits_per_word);

	do {
		ret = stm_ts_fw_corruption_check(ts);
		if (ret == -STM_TS_ERROR_FW_CORRUPTION) {
			ts->plat_data->hw_param.checksum_result = 1;
			break;
		} else if (ret < 0) {
			if (ret == -STM_TS_ERROR_BROKEN_OSC_TRIM) {
				break;
			} else if (ts->plat_data->hw_param.checksum_result) {
				break;
			} else if (ret == -STM_TS_ERROR_TIMEOUT_ZERO) {
				ret = stm_ts_read_chip_id_hw(ts);
				if (ret == STM_TS_NOT_ERROR) {
					ts->plat_data->hw_param.checksum_result = 1;
					input_err(true, &ts->client->dev, "%s: config corruption\n", __func__);
					break;
				}
			}
			stm_ts_systemreset(ts, 20);
		} else {
			break;
		}
	} while (--retry);

	stm_ts_get_version_info(ts);

	if (ret == -STM_TS_ERROR_BROKEN_OSC_TRIM) {
		ret = stm_ts_osc_trim_recovery(ts);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to recover osc trim\n", __func__);
	}

	if (ts->plat_data->hw_param.checksum_result) {
		ts->fw_version_of_ic = 0;
		ts->config_version_of_ic = 0;
		ts->fw_main_version_of_ic = 0;
	}

	ret = stm_ts_read_chip_id(ts);
	if (ret < 0) {
		stm_ts_systemreset(ts, 500);	/* Delay to discharge the IC from ESD or On-state.*/
		input_err(true, &ts->client->dev, "%s: Reset caused by chip id error\n", __func__);
		stm_ts_read_chip_id(ts);
	}

	ret = stm_ts_spi_fw_update_on_probe(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to firmware update\n",
				__func__);
		return -STM_TS_ERROR_FW_UPDATE_FAIL;
	}

	ret = stm_ts_get_channel_info(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read failed rc = %d\n", __func__, ret);
		return ret;
	}

	ts->pFrame = kzalloc(ts->rx_count * ts->tx_count* 2 + 1, GFP_KERNEL);
	if (!ts->pFrame)
		return -ENOMEM;

	ts->cx_data = kzalloc(ts->rx_count * ts->tx_count+ 1, GFP_KERNEL);
	if (!ts->cx_data) {
		kfree(ts->pFrame);
		return -ENOMEM;
	}

	ts->ito_result = kzalloc(STM_TS_ITO_RESULT_PRINT_SIZE, GFP_KERNEL);
	if (!ts->ito_result) {
		kfree(ts->cx_data);
		kfree(ts->pFrame);
		return -ENOMEM;
	}

	/* fts driver set functional feature */
	ts->plat_data->touch_count = 0;
	ts->touch_opmode = STM_TS_OPMODE_NORMAL;
	ts->charger_mode = STM_TS_BIT_CHARGER_MODE_NORMAL;

#ifdef TCLM_CONCEPT
	ts->tdata->external_factory = false;
#endif

	ts->plat_data->touch_functions = STM_TS_TOUCHTYPE_DEFAULT_ENABLE;
	stm_ts_set_touch_function(ts);
	sec_delay(10);

	stm_ts_command(ts, STM_TS_CMD_FORCE_CALIBRATION, true);
	stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);
	ts->scan_mode = STM_TS_SCAN_MODE_DEFAULT;
	stm_ts_set_scanmode(ts, ts->scan_mode);

	ts->flip_status = -1;

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	if (ts->plat_data->support_dual_foldable == SUB_TOUCH)
		ts->flip_status_current = STM_TS_STATUS_FOLDING;
	else
		ts->flip_status_current = STM_TS_STATUS_UNFOLDING;
#endif

	input_info(true, &ts->client->dev, "%s: Initialized\n", __func__);

	stm_ts_init_proc(ts);

	return ret;
}

static int stm_ts_init(struct spi_device *client)
{
	struct stm_ts_data *ts;
	struct sec_ts_plat_data *pdata;
	struct sec_tclm_data *tdata = NULL;
	int ret = 0;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_ts_plat_data), GFP_KERNEL);

		if (!pdata) {
			ret = -ENOMEM;
			goto error_allocate_pdata;
		}

		client->dev.platform_data = pdata;

		ret = sec_input_parse_dt(&client->dev);
		if (ret) {
			input_err(true, &client->dev, "%s: Failed to parse dt\n", __func__);
			goto error_allocate_mem;
		}
		tdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_tclm_data), GFP_KERNEL);
		if (!tdata) {
			ret = -ENOMEM;
			goto error_allocate_tdata;
		}

#ifdef TCLM_CONCEPT
		sec_tclm_parse_dt(&client->dev, tdata);
#endif
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			ret = -ENOMEM;
			input_err(true, &client->dev, "%s: No platform data found\n", __func__);
			goto error_allocate_pdata;
		}
	}

	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl))
		input_err(true, &client->dev, "%s: could not get pinctrl\n", __func__);

	ts = devm_kzalloc(&client->dev, sizeof(struct stm_ts_data), GFP_KERNEL);
	if (!ts) {
		ret = -ENOMEM;
		goto error_allocate_mem;
	}

	client->irq = gpio_to_irq(pdata->irq_gpio);

	ts->client = client;
	ts->plat_data = pdata;
	ts->irq = client->irq;
	ts->stm_ts_spi_read = stm_ts_spi_read;
	ts->stm_ts_spi_write = stm_ts_spi_write;
	ts->stm_ts_read_sponge = stm_ts_read_from_sponge;
	ts->stm_ts_write_sponge = stm_ts_write_to_sponge;
	ts->stm_ts_systemreset = stm_ts_systemreset;
	ts->stm_ts_command = stm_ts_command;

	ts->plat_data->pinctrl_configure = sec_input_pinctrl_configure;
	ts->plat_data->power = sec_input_power;
	ts->plat_data->start_device = stm_ts_start_device;
	ts->plat_data->stop_device = stm_ts_stop_device;
	ts->plat_data->init = stm_ts_reinit;
	ts->plat_data->lpmode = stm_ts_set_lowpowermode;
	ts->plat_data->set_grip_data = stm_set_grip_data_to_ic;
	ts->plat_data->set_temperature = stm_ts_set_temperature;

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	ts->plat_data->stui_tsp_enter = stm_stui_tsp_enter;
	ts->plat_data->stui_tsp_exit = stm_stui_tsp_exit;
#endif

	ts->tdata = tdata;
	if (!ts->tdata) {
		ret = -ENOMEM;
		goto err_null_tdata;
	}

#ifdef TCLM_CONCEPT
	sec_tclm_initialize(ts->tdata);
	ts->tdata->spi = ts->client;
	ts->tdata->tclm_read_spi = stm_tclm_data_read;
	ts->tdata->tclm_write_spi = stm_tclm_data_write;
	ts->tdata->tclm_execute_force_calibration_spi = stm_ts_tclm_execute_force_calibration;
	ts->tdata->tclm_parse_dt = sec_tclm_parse_dt;
#endif

	INIT_DELAYED_WORK(&ts->reset_work, stm_ts_reset_work);
	INIT_DELAYED_WORK(&ts->work_read_info, stm_ts_read_info_work);
	INIT_DELAYED_WORK(&ts->work_print_info, stm_ts_print_info_work);
	INIT_DELAYED_WORK(&ts->work_read_functions, stm_ts_get_touch_function);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	INIT_DELAYED_WORK(&ts->switching_work, stm_switching_work);
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	INIT_DELAYED_WORK(&ts->secure_work, stm_ts_secure_work);
#endif
	mutex_init(&ts->device_mutex);
	mutex_init(&ts->spi_mutex);
	mutex_init(&ts->eventlock);
	mutex_init(&ts->modechange);
	mutex_init(&ts->sponge_mutex);
	mutex_init(&ts->fn_mutex);
	mutex_init(&ts->switching_mutex);
	mutex_init(&ts->status_mutex);
	mutex_init(&ts->secure_lock);
	mutex_init(&ts->raw_lock);

	ts->plat_data->sec_ws = wakeup_source_register(&ts->client->dev, "tsp");
	device_init_wakeup(&client->dev, true);

	init_completion(&ts->plat_data->resume_done);
	complete_all(&ts->plat_data->resume_done);

	spi_set_drvdata(client, ts);

	ret = sec_input_device_register(&client->dev, ts);
	if (ret) {
		input_err(true, &ts->client->dev, "failed to register input device, %d\n", ret);
		goto err_register_input_device;
	}

	ts->plat_data->input_dev->open = stm_ts_input_open;
	ts->plat_data->input_dev->close = stm_ts_input_close;

	ret = stm_ts_fn_init(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: fail to init fn\n", __func__);
		goto err_fn_init;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_register_notify(&ts->stm_input_nb, stm_notifier_call, 1);
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	ts->hall_ic_nb.priority = 1;
	ts->hall_ic_nb.notifier_call = stm_hall_ic_notify;
	hall_notifier_register(&ts->hall_ic_nb);
	input_info(true, &ts->client->dev, "%s: hall ic register\n", __func__);
#endif
#if IS_ENABLED(CONFIG_SUPPORT_SENSOR_FOLD)
	ts->hall_ic_nb_ssh.priority = 1;
	ts->hall_ic_nb_ssh.notifier_call = stm_hall_ic_ssh_notify;
	sensorfold_notifier_register(&ts->hall_ic_nb_ssh);
	input_info(true, &ts->client->dev, "%s: hall ic(ssh) register\n", __func__);
#endif
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	dump_callbacks.inform_dump = stm_ts_dump_tsp_log;
	INIT_DELAYED_WORK(&ts->check_rawdata, stm_ts_check_rawdata);
#endif
	input_info(true, &client->dev, "%s: init resource\n", __func__);

	return 0;

err_fn_init:
err_register_input_device:
	wakeup_source_unregister(ts->plat_data->sec_ws);
err_null_tdata:
error_allocate_mem:
#if !IS_ENABLED(CONFIG_QGKI)
	return -EPROBE_DEFER;
#endif
	regulator_put(pdata->dvdd);
	regulator_put(pdata->avdd);
error_allocate_tdata:
error_allocate_pdata:
	input_err(true, &client->dev, "%s: failed(%d)\n", __func__, ret);
	input_log_fix();
	return ret;
}

void stm_ts_release(struct spi_device *client)
{
	struct stm_ts_data *ts = spi_get_drvdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	if (ts->stm_input_nb.notifier_call)
		sec_input_unregister_notify(&ts->stm_input_nb);
#endif
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	if (ts->hall_ic_nb.notifier_call)
		hall_notifier_unregister(&ts->hall_ic_nb);
#endif
#endif
	cancel_delayed_work_sync(&ts->work_read_info);
	cancel_delayed_work_sync(&ts->work_print_info);
	cancel_delayed_work_sync(&ts->work_read_functions);
	cancel_delayed_work_sync(&ts->reset_work);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	cancel_delayed_work_sync(&ts->switching_work);
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	cancel_delayed_work_sync(&ts->secure_work);
#endif
	flush_delayed_work(&ts->reset_work);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	cancel_delayed_work_sync(&ts->check_rawdata);
	dump_callbacks.inform_dump = NULL;
#endif
	stm_ts_fn_remove(ts);

	device_init_wakeup(&client->dev, false);
	wakeup_source_unregister(ts->plat_data->sec_ws);

	ts->plat_data->lowpower_mode = false;
	ts->probe_done = false;

	ts->plat_data->power(&ts->client->dev, false);

	regulator_put(ts->plat_data->dvdd);
	regulator_put(ts->plat_data->avdd);
}


int stm_ts_spi_probe(struct spi_device *client)
{
	struct stm_ts_data *ts;
	int ret = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	ret = stm_ts_init(client);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: fail to init resource\n", __func__);
		return ret;
	}

	ts = spi_get_drvdata(client);
	ret = spi_setup(client);
	input_info(true, &ts->client->dev, "%s: spi setup: %d\n", __func__, ret);
	g_ts = ts;

	ret = stm_ts_hw_init(client);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to init hw\n", __func__);
		stm_ts_release(client);
		return ret;
	}

	stm_ts_get_custom_library(ts);
	stm_ts_set_custom_library(ts);

	input_info(true, &ts->client->dev, "%s: request_irq = %d\n", __func__, client->irq);
	ret = request_threaded_irq(client->irq, NULL, stm_ts_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, STM_TS_SPI_NAME, ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Unable to request threaded irq\n", __func__);
		stm_ts_release(client);
		return ret;
	}

	ts->probe_done = true;
	ts->plat_data->enabled = true;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (sysfs_create_group(&ts->plat_data->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &ts->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);

	sec_secure_touch_register(ts, ts->plat_data->ss_touch_num, &ts->plat_data->input_dev->dev.kobj);
#endif

	input_err(true, &ts->client->dev, "%s: done\n", __func__);
#ifdef ENABLE_RAWDATA_SERVICE
	stm_ts_rawdata_map_init(ts);
#endif
	input_log_fix();

	if (!ts->plat_data->shutdown_called && ts->plat_data->bringup == 0)
		schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(50));

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	stm_ts_spi_tool_proc_init(ts);
#endif
	return 0;
}

int stm_ts_spi_remove(struct spi_device *client)
{
	struct stm_ts_data *ts = spi_get_drvdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	stm_ts_spi_tool_proc_remove();
#endif

	mutex_lock(&ts->modechange);
	ts->plat_data->shutdown_called = true;
	mutex_unlock(&ts->modechange);

	disable_irq_nosync(ts->client->irq);
	free_irq(ts->client->irq, ts);

	stm_ts_release(client);

	return 0;
}

void stm_ts_spi_shutdown(struct spi_device *client)
{
	struct stm_ts_data *ts = spi_get_drvdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	stm_ts_spi_remove(client);
}


#if IS_ENABLED(CONFIG_PM)
static int stm_ts_pm_suspend(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	reinit_completion(&ts->plat_data->resume_done);

	return 0;
}

static int stm_ts_pm_resume(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	complete_all(&ts->plat_data->resume_done);

	return 0;
}
#endif


static const struct spi_device_id stm_ts_spi_id[] = {
	{ STM_TS_SPI_NAME, 0 },
	{ },
};

#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops stm_ts_dev_pm_ops = {
	.suspend = stm_ts_pm_suspend,
	.resume = stm_ts_pm_resume,
};
#endif

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id stm_ts_match_table[] = {
	{ .compatible = "stm,stm_ts_spi",},
	{ },
};
#else
#define stm_ts_match_table NULL
#endif

static struct spi_driver stm_ts_driver = {
	.probe		= stm_ts_spi_probe,
	.remove		= stm_ts_spi_remove,
	.shutdown	= stm_ts_spi_shutdown,
	.id_table	= stm_ts_spi_id,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= STM_TS_SPI_NAME,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = stm_ts_match_table,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &stm_ts_dev_pm_ops,
#endif
	},
};

module_spi_driver(stm_ts_driver);

MODULE_SOFTDEP("pre: acpm-mfd-bus");
MODULE_SOFTDEP("pre: s2dos05-regulator");
MODULE_DESCRIPTION("stm spi TouchScreen driver");
MODULE_LICENSE("GPL");
