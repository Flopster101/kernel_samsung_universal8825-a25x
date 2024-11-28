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

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_spi_lock(struct spi_master *spi);
extern int stui_spi_unlock(struct spi_master *spi);

int stm_stui_tsp_enter(void)
{
	struct stm_ts_data *ts = dev_get_drvdata(ptsp);
	struct spi_device *spi;
	int ret = 0;

	if (!ts)
		return -EINVAL;

	spi = (struct spi_device *)ts->client;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif

	disable_irq(ts->irq);
	stm_ts_release_all_finger(ts);

	ret = stui_spi_lock(spi->controller);
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
	struct spi_device *spi;
	int ret = 0;

	if (!ts)
		return -EINVAL;

	spi = (struct spi_device *)ts->client;

	ret = stui_spi_unlock(spi->controller);
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

int stm_ts_wire_mode_change(struct stm_ts_data *ts, u8 *reg)
{
	int ret = 0;

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x32;
	reg[5] = 0x10;
	ret = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (ret < 0)
		return ret;
	sec_delay(2);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x34;
	reg[5] = 0x02;
	ret = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (ret < 0)
		return ret;
	sec_delay(2);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = STM_REG_MISO_PORT_SETTING;
	reg[5] = 0x07;
	ret = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (ret < 0)
		return ret;
	sec_delay(2);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x3D;
	reg[5] = 0x30;
	ret = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (ret < 0)
		return ret;
	sec_delay(2);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x2D;
	reg[5] = 0x02;
	ret = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (ret < 0)
		return ret;
	sec_delay(15);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x1B;
	reg[5] = 0x66;
	ret = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (ret < 0)
		return ret;
	sec_delay(30);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x68;
	reg[5] = 0x13;
	ret = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (ret < 0)
		return ret;
	sec_delay(30);
	return 0;
}

struct device *stm_ts_get_client_dev(struct stm_ts_data *ts)
{
	struct spi_device *spi = (struct spi_device *)ts->client;

	return &spi->dev;
}

void stm_ts_set_spi_mode(struct stm_ts_data *ts)
{
	struct spi_device *spi = (struct spi_device *)ts->client;
	if (ts->plat_data->gpio_spi_cs > 0) {
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);
		input_err(true, ts->dev, "%s: cs gpio: %d\n",
			__func__, gpio_get_value(ts->plat_data->gpio_spi_cs));
	}

	spi->mode = SPI_MODE_0;
	input_err(true, ts->dev, "%s: mode: %d, max_speed_hz: %d: cs: %d\n", __func__, spi->mode, spi->max_speed_hz, ts->plat_data->gpio_spi_cs);
	input_err(true, ts->dev, "%s: chip_select: %d, bits_per_word: %d\n", __func__, spi->chip_select, spi->bits_per_word);
}

static int stm_ts_read_buf_malloc(struct stm_ts_data *ts, int length)
{
	static int define_length = STM_TS_EVENT_BUFF_MAX_SIZE;

	if (define_length == STM_TS_EVENT_BUFF_MAX_SIZE && length <= STM_TS_EVENT_BUFF_MAX_SIZE)
		return SEC_SUCCESS;

	if (length <= STM_TS_EVENT_BUFF_MAX_SIZE) {
		if (ts->read_buf)
			devm_kfree(ts->dev, ts->read_buf);
		ts->read_buf = devm_kzalloc(ts->dev, STM_TS_EVENT_BUFF_MAX_SIZE, GFP_KERNEL);
		if (unlikely(ts->read_buf == NULL))
			return -ENOMEM;
		define_length = STM_TS_EVENT_BUFF_MAX_SIZE;
	} else {
		if (ts->read_buf)
			devm_kfree(ts->dev, ts->read_buf);
		ts->read_buf = devm_kzalloc(ts->dev, length, GFP_KERNEL);
		if (unlikely(ts->read_buf == NULL))
			return -ENOMEM;
		define_length = length;
	}

	return SEC_SUCCESS;
}

static int stm_ts_write_buf_malloc(struct stm_ts_data *ts, int length)
{
	static int define_length = STM_TS_EVENT_BUFF_MAX_SIZE;

	if (define_length == STM_TS_EVENT_BUFF_MAX_SIZE && length <= STM_TS_EVENT_BUFF_MAX_SIZE)
		return SEC_SUCCESS;

	if (length <= STM_TS_EVENT_BUFF_MAX_SIZE) {
		if (ts->write_buf)
			devm_kfree(ts->dev, ts->write_buf);
		ts->write_buf = devm_kzalloc(ts->dev, STM_TS_EVENT_BUFF_MAX_SIZE, GFP_KERNEL);
		if (unlikely(ts->write_buf == NULL))
			return -ENOMEM;
		define_length = STM_TS_EVENT_BUFF_MAX_SIZE;
	} else {
		if (ts->write_buf)
			devm_kfree(ts->dev, ts->write_buf);
		ts->write_buf = devm_kzalloc(ts->dev, length, GFP_KERNEL);
		if (unlikely(ts->write_buf == NULL))
			return -ENOMEM;
		define_length = length;
	}

	return SEC_SUCCESS;
}

int stm_ts_read_from_sponge(struct stm_ts_data *ts, u8 *data, int length)
{
	int ret = -ENOMEM;
	int total_length = 3 + 1 + length; /* (dummy:D1(1) + offset(2) + length*/

	if (atomic_read(&ts->plat_data->power_state) == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, ts->dev, "%s: Touch is stopped!\n", __func__);
		return -ENODEV;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, ts->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}
	if (sec_check_secure_trusted_mode_status(ts->plat_data))
		return -EBUSY;

	mutex_lock(&ts->read_write_mutex);
	ret = stm_ts_write_buf_malloc(ts, total_length);
	if (ret < 0) {
		mutex_unlock(&ts->read_write_mutex);
		return -ENODEV;
	}

	ret = stm_ts_read_buf_malloc(ts, total_length);
	if (ret < 0) {
		mutex_unlock(&ts->read_write_mutex);
		return -ENODEV;
	}

	ts->write_buf[0] = 0xC0;
	ts->write_buf[1] = STM_TS_CMD_SPONGE_READ_WRITE_CMD;

	ts->transfer->len = 2;
	ts->transfer->tx_buf = ts->write_buf;

	spi_message_init(ts->message);
	spi_message_add_tail(ts->transfer, ts->message);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, ts->message);
	if (ret < 0)
		input_err(true, ts->dev, "%s: write failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD)
		pr_info("sec_input: spi rsp: W: %02X %02X\n", ts->write_buf[0], ts->write_buf[1]);

	memset(ts->write_buf, 0x0, total_length);

	ts->write_buf[0] = 0xD1;
	ts->write_buf[1] = data[1];	//offset
	ts->write_buf[2] = data[0];

	spi_message_init(ts->message);
	ts->transfer->len = total_length;
	ts->transfer->tx_buf = ts->write_buf;
	ts->transfer->rx_buf = ts->read_buf;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
	ts->transfer->delay_usecs = 10;
#endif
	spi_message_add_tail(ts->transfer, ts->message);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, ts->message);
	if (ret < 0)
		input_err(true, ts->dev, "%s: read failed%d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		int i;

		pr_info("sec_input: spi rsp: R: ");
		for (i = 0; i < 3; i++)
			pr_cont("%02X ", ts->write_buf[i]);
		pr_cont("| ");
		for (i = 0; i < (total_length); i++) {
			if (i == (total_length - length))
				pr_cont("|| ");
			pr_cont("%02X ", ts->read_buf[i]);
		}
		pr_cont("\n");

	}
	memcpy(data, &ts->read_buf[4], length);
	memset(ts->write_buf, 0x0, 3  + 1 + length);
	memset(ts->read_buf, 0x0, 3  + 1 + length);
	ts->transfer->tx_buf = NULL;
	ts->transfer->rx_buf = NULL;
	mutex_unlock(&ts->read_write_mutex);

	return ret;
}

int stm_ts_write_to_sponge(struct stm_ts_data *ts, u8 *data, int length)
{
	int ret = -ENOMEM;

	if (atomic_read(&ts->plat_data->power_state) == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, ts->dev, "%s: Touch is stopped!\n", __func__);
		return -ENODEV;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, ts->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}
	if (sec_check_secure_trusted_mode_status(ts->plat_data))
		return -EBUSY;

	mutex_lock(&ts->read_write_mutex);
	ret = stm_ts_write_buf_malloc(ts, 3 + length);
	if (ret < 0) {
		mutex_unlock(&ts->read_write_mutex);
		return -ENODEV;
	}

	ts->write_buf[0] = 0xC0;
	ts->write_buf[1] = STM_TS_CMD_SPONGE_READ_WRITE_CMD;

	ts->transfer->len = 2;
	ts->transfer->tx_buf = ts->write_buf;

	spi_message_init(ts->message);
	spi_message_add_tail(ts->transfer, ts->message);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, ts->message);
	if (ret < 0)
		input_err(true, ts->dev, "%s: failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);
	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD)
		pr_info("sec_input: spi wsp: W: %02X %02X\n", ts->write_buf[0], ts->write_buf[1]);
	memset(ts->write_buf, 0x0, 3 + length);

	ts->write_buf[0] = 0xD0;
	ts->write_buf[1] = data[1];
	ts->write_buf[2] = data[0];

	memcpy(&ts->write_buf[3], &data[2], length - 2);

	ts->transfer->len = 1 + length; /* (tbuf2(3) - offset(2) + length*/
	ts->transfer->tx_buf = ts->write_buf;

	spi_message_init(ts->message);
	spi_message_add_tail(ts->transfer, ts->message);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, ts->message);
	if (ret < 0)
		input_err(true, ts->dev, "%s: failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		int i;

		pr_info("sec_input: spi wsp: W: ");
		for (i = 0; i < ts->transfer->len; i++)
			pr_cont("%02X ", ts->write_buf[i]);
		pr_cont("\n");

	}
	memset(ts->write_buf, 0x0, 3 + length);

	ts->write_buf[0] = 0xC0;
	ts->write_buf[1] = STM_TS_CMD_SPONGE_NOTIFY_CMD;

	ts->transfer->len = 2;
	ts->transfer->tx_buf = ts->write_buf;

	spi_message_init(ts->message);
	spi_message_add_tail(ts->transfer, ts->message);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, ts->message);
	if (ret < 0)
		input_err(true, ts->dev, "%s: failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD)
		pr_info("sec_input: spi wsp: W: %02X %02X\n", ts->write_buf[0], ts->write_buf[1]);

	memset(ts->write_buf, 0x0, 3 + length);
	ts->transfer->tx_buf = NULL;
	ts->transfer->rx_buf = NULL;
	mutex_unlock(&ts->read_write_mutex);

	return ret;
}

int stm_ts_no_lock_spi_write(struct stm_ts_data *ts, u8 *reg, int tlen, u8 *data, int len)
{
	int ret = -ENOMEM;
	int wlen;

	ret = stm_ts_write_buf_malloc(ts, tlen + len + 1);
	if (ret < 0) {
		return -ENODEV;
	}

	switch (reg[0]) {
	case STM_TS_CMD_SCAN_MODE:
	case STM_TS_CMD_FEATURE:
	case STM_TS_CMD_SYSTEM:
	case STM_TS_CMD_FRM_BUFF_R:
	case STM_TS_CMD_REG_W:
	case STM_TS_CMD_REG_R:
	case STM_TS_CMD_SPONGE_W:
		memcpy(ts->write_buf, reg, tlen);
		memcpy(&ts->write_buf[tlen], data, len);
		wlen = tlen + len;
		break;
	default:
		ts->write_buf[0] = 0xC0;
		memcpy(&ts->write_buf[1], reg, tlen);
		memcpy(&ts->write_buf[1 + tlen], data, len);
		wlen = tlen + len + 1;
		break;
	}

	ts->transfer->len = wlen;
	ts->transfer->tx_buf = ts->write_buf;

	spi_message_init(ts->message);
	spi_message_add_tail(ts->transfer, ts->message);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, ts->message);
	if (ret < 0)
		input_err(true, ts->dev, "%s: failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		int i;

		pr_info("sec_input: spi: W: ");
		for (i = 0; i < wlen; i++)
			pr_cont("%02X ", ts->write_buf[i]);
		pr_cont("\n");
	}
	memset(ts->write_buf, 0x0, tlen + len + 1);
	ts->transfer->tx_buf = NULL;
	ts->transfer->rx_buf = NULL;

	return ret;
}

int stm_ts_spi_write(struct stm_ts_data *ts, u8 *reg, int tlen, u8 *data, int len)
{
	int ret = -ENOMEM;

	if (atomic_read(&ts->plat_data->power_state) == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, ts->dev, "%s: Sensor stopped\n", __func__);
		return -EIO;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, ts->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
	if (sec_check_secure_trusted_mode_status(ts->plat_data))
		return -EBUSY;

	mutex_lock(&ts->read_write_mutex);
	ret = stm_ts_no_lock_spi_write(ts, reg, tlen, data, len);
	mutex_unlock(&ts->read_write_mutex);

	return ret;
}

int stm_ts_spi_read(struct stm_ts_data *ts, u8 *reg, int tlen, u8 *buf, int rlen)
{
	int ret = -ENOMEM;
	int wmsg = 0;
	int buf_len = tlen;
	int mem_len;
	u8 temp_write_buf[5] = { 0 };

	if (atomic_read(&ts->plat_data->power_state) == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, ts->dev, "%s: Sensor stopped\n", __func__);
		return -EIO;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, ts->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
	if (sec_check_secure_trusted_mode_status(ts->plat_data))
		return -EBUSY;

	mutex_lock(&ts->read_write_mutex);
	if (tlen > 3)
		mem_len = rlen + 1 + tlen;
	else
		mem_len = rlen + 1 + 3;

	ret = stm_ts_write_buf_malloc(ts, mem_len);
	if (ret < 0) {
		mutex_unlock(&ts->read_write_mutex);
		return -ENODEV;
	}

	ret = stm_ts_read_buf_malloc(ts, mem_len);
	if (ret < 0) {
		mutex_unlock(&ts->read_write_mutex);
		return -ENODEV;
	}

	switch (reg[0]) {
	case 0x87:
	case STM_TS_CMD_FRM_BUFF_R:
	case STM_TS_CMD_SYSTEM:
	case STM_TS_CMD_REG_W:
	case STM_TS_CMD_REG_R:
		memcpy(temp_write_buf, reg, tlen);
		wmsg = 0;
		break;
	case 0x75:
		buf_len = 3;
		temp_write_buf[0] = 0xD1;
		if (tlen == 1) {
			temp_write_buf[1] = 0x00;
			temp_write_buf[2] = 0x00;
		} else if (tlen == 2) {
			temp_write_buf[1] = 0x00;
			temp_write_buf[2] = reg[0];
		} else if (tlen == 3) {
			temp_write_buf[1] = reg[1];
			temp_write_buf[2] = reg[0];
		} else {
			input_err(true, ts->dev, "%s: tlen is mismatched: %d\n", __func__, tlen);
			mutex_unlock(&ts->read_write_mutex);
			return -EINVAL;
		}
		wmsg = 0;
		break;
	default:
		buf_len = 3;
		temp_write_buf[0] = 0xD1;

		if (tlen <= 3) {
			temp_write_buf[1] = 0x00;
			temp_write_buf[2] = 0x00;
		} else {
			input_err(true, ts->dev, "%s: tlen is mismatched: %d\n", __func__, tlen);
			mutex_unlock(&ts->read_write_mutex);
			return -EINVAL;
		}
		wmsg = 1;
		break;
	}

	if (wmsg)
		stm_ts_no_lock_spi_write(ts, reg, tlen, NULL, 0);

	spi_message_init(ts->message);
	memcpy(ts->write_buf, temp_write_buf, buf_len);
	ts->transfer->len = rlen + 1 + buf_len;
	ts->transfer->tx_buf = ts->write_buf;
	ts->transfer->rx_buf = ts->read_buf;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0))
	ts->transfer->delay_usecs = 10;
#endif
	spi_message_add_tail(ts->transfer, ts->message);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, ts->message);
	if (ret < 0)
		input_err(true, ts->dev, "%s: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		int i;

		pr_info("sec_input: spi: R: ");
		for (i = 0; i < buf_len; i++)
			pr_cont("%02X ", ts->write_buf[i]);
		pr_cont("| ");
		for (i = 0; i < rlen + 1 + buf_len; i++) {
			if (i == buf_len + 1)
				pr_cont("|| ");
			pr_cont("%02X ", ts->read_buf[i]);
		}
		pr_cont("\n");

	}
	memcpy(buf, &ts->read_buf[1 + buf_len], rlen);
	memset(ts->write_buf, 0x0, mem_len);
	memset(ts->read_buf, 0x0, mem_len);
	ts->transfer->tx_buf = NULL;
	ts->transfer->rx_buf = NULL;
	mutex_unlock(&ts->read_write_mutex);

	return ret;
}

static int stm_ts_spi_init(struct spi_device *client)
{
	struct stm_ts_data *ts;
	int ret = 0;

	ts = devm_kzalloc(&client->dev, sizeof(struct stm_ts_data), GFP_KERNEL);
	if (unlikely(ts == NULL)) {
		ret = -ENOMEM;
		goto error_allocate;
	}

	ts->plat_data = devm_kzalloc(&client->dev, sizeof(struct sec_ts_plat_data), GFP_KERNEL);
	if (unlikely(ts->plat_data == NULL)) {
		ret = -ENOMEM;
		goto error_allocate;
	}

	ts->tdata = devm_kzalloc(&client->dev, sizeof(struct sec_tclm_data), GFP_KERNEL);
	if (unlikely(ts->tdata == NULL)) {
		ret = -ENOMEM;
		goto error_allocate;
	}
	client->dev.platform_data = ts->plat_data;

	ts->read_buf = devm_kzalloc(&client->dev, STM_TS_EVENT_BUFF_MAX_SIZE, GFP_KERNEL);
	if (unlikely(ts->read_buf == NULL)) {
		ret = -ENOMEM;
		goto error_allocate;
	}

	ts->write_buf = devm_kzalloc(&client->dev, STM_TS_EVENT_BUFF_MAX_SIZE, GFP_KERNEL);
	if (unlikely(ts->write_buf == NULL)) {
		ret = -ENOMEM;
		goto error_allocate;
	}

	ts->message = devm_kzalloc(&client->dev, sizeof(struct spi_message), GFP_KERNEL);
	if (unlikely(ts->message == NULL)) {
		ret = -ENOMEM;
		goto error_allocate;
	}

	ts->transfer = devm_kzalloc(&client->dev, sizeof(struct spi_transfer), GFP_KERNEL);
	if (unlikely(ts->transfer == NULL)) {
		ret = -ENOMEM;
		goto error_allocate;
	}

	ts->client = client;
	ts->dev = &client->dev;
#if IS_ENABLED(CONFIG_INPUT_SEC_TRUSTED_TOUCH)
	ts->plat_data->dev =  &client->dev;
	ts->plat_data->bus_master = &client->controller->dev;
#endif
	ts->stm_ts_read = stm_ts_spi_read;
	ts->stm_ts_write = stm_ts_spi_write;
	ts->stm_ts_read_sponge = stm_ts_read_from_sponge;
	ts->stm_ts_write_sponge = stm_ts_write_to_sponge;

	spi_set_drvdata(client, ts);
	ret = spi_setup(client);
	input_info(true, &client->dev, "%s: spi setup: %d\n", __func__, ret);


#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	ts->plat_data->stui_tsp_enter = stm_stui_tsp_enter;
	ts->plat_data->stui_tsp_exit = stm_stui_tsp_exit;
	ts->plat_data->stui_tsp_type = stm_stui_tsp_type;
#endif

	ret = stm_ts_init(ts);
	if (ret < 0) {
		input_err(true, ts->dev, "%s: fail to init resource\n", __func__);
		return ret;
	}
	ret = 0;
error_allocate:
	return ret;
}

int stm_ts_spi_probe(struct spi_device *client)
{
	struct stm_ts_data *ts;
	int ret = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	ret = stm_ts_spi_init(client);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: fail to init resource\n", __func__);
		return ret;
	}

	ts = spi_get_drvdata(client);
	if (!ts->plat_data->work_queue_probe_enabled)
		return stm_ts_probe(ts->dev);

	queue_work(ts->plat_data->probe_workqueue, &ts->plat_data->probe_work);
	return 0;
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
