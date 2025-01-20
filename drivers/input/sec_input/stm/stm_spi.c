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

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
int stm_pm_runtime_get_sync(struct stm_ts_data *ts)
{
	return pm_runtime_get_sync(ts->client->controller->dev.parent);
}

void stm_pm_runtime_put_sync(struct stm_ts_data *ts)
{
	pm_runtime_put_sync(ts->client->controller->dev.parent);
}
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_spi_lock(struct spi_master *spi);
extern int stui_spi_unlock(struct spi_master *spi);

int stm_stui_tsp_enter(void)
{
	struct stm_ts_data *ts = dev_get_drvdata(ptsp);
	int ret = 0;

	if (!ts)
		return -EINVAL;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif

	disable_irq(ts->irq);
	stm_ts_release_all_finger(ts);

	ret = stui_spi_lock(ts->client->controller);
	if (ret < 0) {
		pr_err("[STUI] stui_spi_lock failed : %d\n", ret);
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
	struct stm_ts_data *ts = dev_get_drvdata(ptsp);
	int ret = 0;

	if (!ts)
		return -EINVAL;

	ret = stui_spi_unlock(ts->client->controller);
	if (ret < 0)
		pr_err("[STUI] stui_spi_unlock failed : %d\n", ret);

	enable_irq(ts->irq);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif

	return ret;
}

int stm_stui_tsp_type(void)
{
	return STUI_TSP_TYPE_STM;
}
#endif

#ifdef TCLM_CONCEPT
int stm_ts_tclm_execute_force_calibration(struct spi_device *client, int cal_mode)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)spi_get_drvdata(client);

	return stm_ts_execute_autotune(ts, true);
}

int stm_tclm_data_read(struct stm_ts_data *ts, int address)
{
	return ts->tdata->tclm_read_spi(ts->tdata->spi, address);
}

int stm_tclm_data_write(struct stm_ts_data *ts, int address)
{
	return ts->tdata->tclm_write_spi(ts->tdata->spi, address);
}


int stm_tclm_spi_data_read(struct spi_device *client, int address)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)spi_get_drvdata(client);

	return _stm_tclm_data_read(ts, address);
}
int stm_tclm_spi_data_write(struct spi_device *client, int address)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)spi_get_drvdata(client);

	return _stm_tclm_data_write(ts, address);
}
#endif
int stm_ts_wire_mode_change(struct stm_ts_data *ts, u8 *reg)
{
	int rc = 0;

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x32;
	reg[5] = 0x10;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(2);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x34;
	reg[5] = 0x02;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(2);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = STM_REG_MISO_PORT_SETTING;
	reg[5] = 0x07;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(2);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x3D;
	reg[5] = 0x30;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(2);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x2D;
	reg[5] = 0x02;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(15);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x1B;
	reg[5] = 0x66;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(30);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x68;
	reg[5] = 0x13;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(30);
	return 0;
}

void stm_ts_set_spi_mode(struct stm_ts_data *ts)
{
	if (ts->plat_data->gpio_spi_cs > 0) {
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);
		input_err(true, &ts->client->dev, "%s: cs gpio: %d\n",
			__func__, gpio_get_value(ts->plat_data->gpio_spi_cs));
	}

	ts->client->mode = SPI_MODE_0;
	input_err(true, &ts->client->dev, "%s: mode: %d, max_speed_hz: %d: cs: %d\n", __func__, ts->client->mode, ts->client->max_speed_hz, ts->plat_data->gpio_spi_cs);
	input_err(true, &ts->client->dev, "%s: chip_select: %d, bits_per_word: %d\n", __func__, ts->client->chip_select, ts->client->bits_per_word);
}

int stm_ts_read_from_sponge(struct stm_ts_data *ts, u8 *data, int length)
{
	struct spi_message *m1;
	struct spi_transfer *t1;
	char *tbuf1;
	struct spi_message *m2;
	struct spi_transfer *t2;
	char *tbuf2;
	char *rbuf2;
	int ret = -ENOMEM;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		return -ENODEV;
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
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	if (atomic_read(&ts->trusted_touch_enabled) != 0) {
		input_err(true, &ts->client->dev, "%s: TVM is enabled\n", __func__);
		return -EBUSY;
	}
#endif
#endif

	m1 = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m1)
		return -ENOMEM;

	t1 = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!t1) {
		kfree(m1);
		return -ENOMEM;
	}

	m2 = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m2) {
		kfree(m1);
		kfree(t1);
		return -ENOMEM;
	}

	t2 = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!t2) {
		kfree(m1);
		kfree(t1);
		kfree(m2);
		return -ENOMEM;
	}

	tbuf1 = kzalloc(2, GFP_KERNEL);
	if (!tbuf1) {
		kfree(m1);
		kfree(t1);
		kfree(m2);
		kfree(t2);
		return -ENOMEM;
	}

	tbuf1[0] = 0xC0;
	tbuf1[1] = STM_TS_CMD_SPONGE_READ_WRITE_CMD;

	t1->len = 2;
	t1->tx_buf = tbuf1;

	mutex_lock(&ts->read_write_mutex);

	spi_message_init(m1);
	spi_message_add_tail(t1, m1);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: write failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD)
		pr_info("sec_input: spi rsp: W: %02X %02X\n", tbuf1[0], tbuf1[1]);

	rbuf2 = kzalloc(3 + 1 + length, GFP_KERNEL);
	if (!rbuf2) {
		kfree(m1);
		kfree(t1);
		kfree(m2);
		kfree(t2);
		kfree(tbuf1);
		mutex_unlock(&ts->read_write_mutex);
		return -ENOMEM;
	}

	tbuf2 = kzalloc(3 + 1 + length, GFP_KERNEL);
	if (!tbuf2) {
		kfree(m1);
		kfree(t1);
		kfree(m2);
		kfree(t2);
		kfree(tbuf1);
		kfree(rbuf2);
		mutex_unlock(&ts->read_write_mutex);
		return -ENOMEM;
	}

	tbuf2[0] = 0xD1;
	tbuf2[1] = data[1];	//offset
	tbuf2[2] = data[0];

	spi_message_init(m2);
	t2->len = 3 + 1 + length; /* (dummy:D1(1) + offset(2) + length*/
	t2->tx_buf = tbuf2;
	t2->rx_buf = rbuf2;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
	t2->delay_usecs = 10;
#endif
	spi_message_add_tail(t2, m2);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: read failed%d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	mutex_unlock(&ts->read_write_mutex);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		int i;

		pr_info("sec_input: spi rsp: R: ");
		for (i = 0; i < 3; i++)
			pr_cont("%02X ", tbuf2[i]);
		pr_cont("| ");
		for (i = 0; i < (3  + 1 + length); i++) {
			if (i == (3 + 1))
				pr_cont("|| ");
			pr_cont("%02X ", rbuf2[i]);
		}
		pr_cont("\n");

	}
	memcpy(data, &rbuf2[4], length);

	kfree(tbuf1);
	kfree(tbuf2);
	kfree(rbuf2);
	kfree(m1);
	kfree(t1);
	kfree(m2);
	kfree(t2);

	return ret;
}

int stm_ts_write_to_sponge(struct stm_ts_data *ts, u8 *data, int length)
{
	struct spi_message *m1;
	struct spi_transfer *t1;
	struct spi_message *m2;
	struct spi_transfer *t2;
	struct spi_message *m3;
	struct spi_transfer *t3;
	char *tbuf1;
	char *tbuf2;
	int ret = -ENOMEM;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		return -ENODEV;
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
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	if (atomic_read(&ts->trusted_touch_enabled) != 0) {
		input_err(true, &ts->client->dev, "%s: TVM is enabled\n", __func__);
		return -EBUSY;
	}
#endif
#endif

	m1 = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m1)
		return -ENOMEM;

	t1 = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!t1) {
		kfree(m1);
		return -ENOMEM;
	}

	m2 = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m2) {
		kfree(m1);
		kfree(t1);
		return -ENOMEM;
	}

	t2 = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!t2) {
		kfree(m1);
		kfree(t1);
		kfree(m2);
		return -ENOMEM;
	}

	m3 = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m3) {
		kfree(m1);
		kfree(t1);
		kfree(m2);
		kfree(t2);
		return -ENOMEM;
	}

	t3 = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!t3) {
		kfree(m1);
		kfree(t1);
		kfree(m2);
		kfree(t2);
		kfree(m3);
		return -ENOMEM;
	}

	tbuf1 = kzalloc(2, GFP_KERNEL);
	if (!tbuf1) {
		kfree(m1);
		kfree(t1);
		kfree(m2);
		kfree(t2);
		kfree(m3);
		kfree(t3);
		return -ENOMEM;
	}

	tbuf1[0] = 0xC0;
	tbuf1[1] = STM_TS_CMD_SPONGE_READ_WRITE_CMD;

	t1->len = 2;
	t1->tx_buf = tbuf1;

	mutex_lock(&ts->read_write_mutex);

	spi_message_init(m1);
	spi_message_add_tail(t1, m1);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD)
		pr_info("sec_input: spi wsp: W: %02X %02X\n", tbuf1[0], tbuf1[1]);

	tbuf2 = kzalloc(3 + length, GFP_KERNEL);
	if (!tbuf2) {
		kfree(m1);
		kfree(t1);
		kfree(m2);
		kfree(t2);
		kfree(m3);
		kfree(t3);
		kfree(tbuf1);
		mutex_unlock(&ts->read_write_mutex);
		return -ENOMEM;
	}

	tbuf2[0] = 0xD0;
	tbuf2[1] = data[1];
	tbuf2[2] = data[0];

	memcpy(&tbuf2[3], &data[2], length - 2);

	t2->len = 1 + length; /* (tbuf2(3) - offset(2) + length*/
	t2->tx_buf = tbuf2;

	spi_message_init(m2);
	spi_message_add_tail(t2, m2);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		int i;

		pr_info("sec_input: spi wsp: W: ");
		for (i = 0; i < t2->len; i++)
			pr_cont("%02X ", tbuf2[i]);
		pr_cont("\n");

	}

	tbuf1[0] = 0xC0;
	tbuf1[1] = STM_TS_CMD_SPONGE_NOTIFY_CMD;

	t3->len = 2;
	t3->tx_buf = tbuf1;

	spi_message_init(m3);
	spi_message_add_tail(t3, m3);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD)
		pr_info("sec_input: spi wsp: W: %02X %02X\n", tbuf1[0], tbuf1[1]);

	mutex_unlock(&ts->read_write_mutex);
	kfree(tbuf1);
	kfree(tbuf2);
	kfree(m1);
	kfree(t1);
	kfree(m2);
	kfree(t2);
	kfree(m3);
	kfree(t3);

	return ret;
}

int stm_ts_no_lock_spi_write(struct stm_ts_data *ts, u8 *reg, int tlen, u8 *data, int len)
{
	struct spi_message *m;
	struct spi_transfer *t;
	char *tbuf;
	int ret = -ENOMEM;
	int wlen;

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
	case STM_TS_CMD_SCAN_MODE:
	case STM_TS_CMD_FEATURE:
	case STM_TS_CMD_SYSTEM:
	case STM_TS_CMD_FRM_BUFF_R:
	case STM_TS_CMD_REG_W:
	case STM_TS_CMD_REG_R:
	case STM_TS_CMD_SPONGE_W:
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

	spi_message_init(m);
	spi_message_add_tail(t, m);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		int i;

		pr_info("sec_input: spi: W: ");
		for (i = 0; i < wlen; i++)
			pr_cont("%02X ", tbuf[i]);
		pr_cont("\n");

	}

	kfree(tbuf);
	kfree(m);
	kfree(t);

	return ret;
}

int stm_ts_spi_write(struct stm_ts_data *ts, u8 *reg, int tlen, u8 *data, int len)
{
	int ret = -ENOMEM;

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

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev, "%s: TUI is enabled\n", __func__);
		return -EBUSY;
	}
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	if (atomic_read(&ts->trusted_touch_enabled) != 0) {
		input_err(true, &ts->client->dev, "%s: TVM is enabled\n", __func__);
		return -EBUSY;
	}
#endif
#endif
	mutex_lock(&ts->read_write_mutex);
	ret = stm_ts_no_lock_spi_write(ts, reg, tlen, data, len);
	mutex_unlock(&ts->read_write_mutex);

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

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev, "%s: TUI is enabled\n", __func__);
		return -EBUSY;
	}
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	if (atomic_read(&ts->trusted_touch_enabled) != 0) {
		input_err(true, &ts->client->dev, "%s: TVM is enabled\n", __func__);
		return -EBUSY;
	}
#endif
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

	mutex_lock(&ts->read_write_mutex);

	switch (reg[0]) {
	case 0x87:
	case STM_TS_CMD_FRM_BUFF_R:
	case STM_TS_CMD_SYSTEM:
	case STM_TS_CMD_REG_W:
	case STM_TS_CMD_REG_R:
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
			mutex_unlock(&ts->read_write_mutex);
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
			mutex_unlock(&ts->read_write_mutex);
			return -EINVAL;
		}

			wmsg = 1;
			break;
	}

	if (wmsg)
		stm_ts_no_lock_spi_write(ts, reg, 1, NULL, 0);

	spi_message_init(m);

	t->len = rlen + 1 + buf_len;
	t->tx_buf = tbuf;
	t->rx_buf = rbuf;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
	t->delay_usecs = 10;
#endif
	spi_message_add_tail(t, m);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	mutex_unlock(&ts->read_write_mutex);

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
	memcpy(buf, &rbuf[1 + buf_len], rlen);

	kfree(rbuf);
	kfree(tbuf);
	kfree(m);
	kfree(t);

	return ret;
}

int stm_ts_spi_probe(struct spi_device *client)
{
	struct stm_ts_data *ts;
	struct sec_ts_plat_data *pdata;
	struct sec_tclm_data *tdata;
	int ret = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	ts = devm_kzalloc(&client->dev, sizeof(struct stm_ts_data), GFP_KERNEL);
	if (!ts) {
		ret = -ENOMEM;
		goto error_allocate_mem;
	}

	pdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_ts_plat_data), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto error_allocate_pdata;
	}

	tdata = devm_kzalloc(&client->dev,
			sizeof(struct sec_tclm_data), GFP_KERNEL);
	if (!tdata) {
		ret = -ENOMEM;
		goto error_allocate_tdata;
	}
	client->dev.platform_data = pdata;

	ts->client = client;
	ts->plat_data = pdata;
	ts->stm_ts_read = stm_ts_spi_read;
	ts->stm_ts_write = stm_ts_spi_write;
	ts->stm_ts_read_sponge = stm_ts_read_from_sponge;
	ts->stm_ts_write_sponge = stm_ts_write_to_sponge;
	ts->tdata = tdata;
#ifdef TCLM_CONCEPT
	ts->tdata->spi = ts->client;
	ts->tdata->tclm_read_spi = stm_tclm_spi_data_read;
	ts->tdata->tclm_write_spi = stm_tclm_spi_data_write;
	ts->tdata->tclm_execute_force_calibration_spi = stm_ts_tclm_execute_force_calibration;
#endif
	spi_set_drvdata(client, ts);
	ret = spi_setup(client);
	input_info(true, &client->dev, "%s: spi setup: %d\n", __func__, ret);


#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	ts->plat_data->stui_tsp_enter = stm_stui_tsp_enter;
	ts->plat_data->stui_tsp_exit = stm_stui_tsp_exit;
	ts->plat_data->stui_tsp_type = stm_stui_tsp_type;
#endif
	ret = stm_ts_probe(ts);

	return ret;

error_allocate_tdata:
error_allocate_pdata:
error_allocate_mem:
	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
void stm_ts_spi_remove(struct spi_device *client)
#else
int stm_ts_spi_remove(struct spi_device *client)
#endif

{
	struct stm_ts_data *ts = spi_get_drvdata(client);
	int ret = 0;

	ret = stm_ts_remove(ts);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
	return;
#else
	return ret;
#endif

}

void stm_ts_spi_shutdown(struct spi_device *client)
{
	struct stm_ts_data *ts = spi_get_drvdata(client);

	stm_ts_shutdown(ts);
}


#if IS_ENABLED(CONFIG_PM)
static int stm_ts_spi_pm_suspend(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	stm_ts_pm_suspend(ts);

	return 0;
}

static int stm_ts_spi_pm_resume(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	stm_ts_pm_resume(ts);

	return 0;
}
#endif


static const struct spi_device_id stm_ts_spi_id[] = {
	{ STM_TS_SPI_NAME, 0 },
	{ },
};

#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops stm_ts_dev_pm_ops = {
	.suspend = stm_ts_spi_pm_suspend,
	.resume = stm_ts_spi_pm_resume,
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
MODULE_DESCRIPTION("stm TouchScreen driver");
MODULE_LICENSE("GPL");
