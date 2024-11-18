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
	return pm_runtime_get_sync(ts->client->adapter->dev.parent);
}

void stm_pm_runtime_put_sync(struct stm_ts_data *ts)
{
	pm_runtime_put_sync(ts->client->adapter->dev.parent);
}
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_i2c_lock(struct i2c_adapter *adap);
extern int stui_i2c_unlock(struct i2c_adapter *adap);

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
	struct stm_ts_data *ts = dev_get_drvdata(ptsp);
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

int stm_stui_tsp_type(void)
{
	return STUI_TSP_TYPE_STM;
}
#endif

#ifdef TCLM_CONCEPT
int stm_ts_tclm_execute_force_calibration(struct i2c_client *client, int cal_mode)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)i2c_get_clientdata(client);

	return stm_ts_execute_autotune(ts, true);
}

int stm_tclm_data_read(struct stm_ts_data *ts, int address)
{
	return ts->tdata->tclm_read(ts->tdata->client, address);
}

int stm_tclm_data_write(struct stm_ts_data *ts, int address)
{
	return ts->tdata->tclm_write(ts->tdata->client, address);
}

int stm_tclm_i2c_data_read(struct i2c_client *client, int address)
{
	struct stm_ts_data *ts = i2c_get_clientdata(client);

	return _stm_tclm_data_read(ts, address);
}
int stm_tclm_i2c_data_write(struct i2c_client *client, int address)
{
	struct stm_ts_data *ts = i2c_get_clientdata(client);

	return _stm_tclm_data_write(ts, address);
}
#endif

int stm_ts_wire_mode_change(struct stm_ts_data *ts, u8 *reg)
{
	return 0;
}

int stm_ts_tool_proc_init(struct stm_ts_data *ts)
{
	return 0;
}

int stm_ts_tool_proc_remove(void)
{
	return 0;
}

int stm_ts_read_from_sponge(struct stm_ts_data *ts, u8 *data, int length)
{
	int ret;
	u8 address[3];

	mutex_lock(&ts->sponge_mutex);
	address[0] = STM_TS_CMD_SPONGE_READ_WRITE_CMD;
	address[1] = data[1];
	address[2] = data[0];
	ret = ts->stm_ts_read(ts, address, 3, data, length);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to read sponge command\n", __func__);
	mutex_unlock(&ts->sponge_mutex);

	return ret;
}

int stm_ts_write_to_sponge(struct stm_ts_data *ts, u8 *data, int length)
{
	int ret;
	u8 address[3];

	mutex_lock(&ts->sponge_mutex);
	address[0] = STM_TS_CMD_SPONGE_READ_WRITE_CMD;
	address[1] = data[1];
	address[2] = data[0];
	ret = ts->stm_ts_write(ts, address, 3, &data[2], length - 2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write offset\n", __func__);

	address[0] = STM_TS_CMD_SPONGE_NOTIFY_CMD;
	ret = ts->stm_ts_write(ts, address, 3, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to send notify\n", __func__);
	mutex_unlock(&ts->sponge_mutex);

	return ret;

}

int stm_ts_i2c_write(struct stm_ts_data *ts, u8 *reg, int cnum, u8 *data, int len)
{
	int ret;
	unsigned char retry;
	struct i2c_msg msg;
	int i;
	const int len_max = 0xffff;
	u8 *buf;
	u8 *msg_buff;

	if (len + 1 > len_max) {
		input_err(true, &ts->client->dev,
				"%s: The i2c buffer size is exceeded.\n", __func__);
		return -ENOMEM;
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
		input_err(true, &ts->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
	buf = kzalloc(cnum, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, reg, cnum);

	msg_buff = kzalloc(len + cnum, GFP_KERNEL);
	if (!msg_buff) {
		kfree(buf);
		return -ENOMEM;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF\n", __func__);
		goto err;
	}

	memcpy(msg_buff, buf, cnum);
	memcpy(msg_buff + cnum, data, len);

	msg.addr = ts->client->addr;
	msg.flags = 0 | I2C_M_DMA_SAFE;
	msg.len = len + cnum;
	msg.buf = msg_buff;

	mutex_lock(&ts->read_write_mutex);
	for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
		ret = i2c_transfer(ts->client->adapter, &msg, 1);
		if (ret == 1)
			break;

		if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
			input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
			mutex_unlock(&ts->read_write_mutex);
			goto err;
		}

		usleep_range(1 * 1000, 1 * 1000);

		if (retry > 1) {
			char result[32];
			input_err(true, &ts->client->dev, "%s: I2C retry %d, ret:%d\n", __func__, retry + 1, ret);
			ts->plat_data->hw_param.comm_err_count++;

			snprintf(result, sizeof(result), "RESULT=I2C");
			if (ts->probe_done)
				sec_cmd_send_event_to_user(&ts->sec, NULL, result);
		}
	}

	mutex_unlock(&ts->read_write_mutex);

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: I2C write over retry limit\n", __func__);
		ret = -EIO;
		if (ts->probe_done && !ts->reset_is_on_going && !ts->plat_data->shutdown_called)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
	}

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		pr_info("sec_input:i2c_cmd: W: %02X | ", *reg);
		for (i = 0; i < len; i++)
			pr_cont("%02X ", data[i]);
		pr_cont("\n");
	}

	if (ret == 1) {
		kfree(msg_buff);
		kfree(buf);
		return 0;
	}
err:
	kfree(msg_buff);
	kfree(buf);
	return -EIO;
}

int stm_ts_i2c_read(struct stm_ts_data *ts, u8 *reg, int cnum, u8 *data, int len)
{
	int ret;
	unsigned char retry;
	struct i2c_msg msg[2];
	int remain = len;
	int i;
	u8 *msg_buff;
	u8 *buf;

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev,
				"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
	buf = kzalloc(cnum, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, reg, cnum);

	msg_buff = kzalloc(len, GFP_KERNEL);
	if (!msg_buff) {
		kfree(buf);
		return -ENOMEM;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF\n", __func__);
		goto err;
	}

	msg[0].addr = ts->client->addr;
	msg[0].flags = 0 | I2C_M_DMA_SAFE;
	msg[0].len = cnum;
	msg[0].buf = buf;

	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD | I2C_M_DMA_SAFE;
	msg[1].buf = msg_buff;

	mutex_lock(&ts->read_write_mutex);
	if (len <= ts->plat_data->i2c_burstmax) {
		msg[1].len = len;
		for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, msg, 2);
			if (ret == 2)
				break;
			usleep_range(1 * 1000, 1 * 1000);
			if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
				input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
				mutex_unlock(&ts->read_write_mutex);
				goto err;
			}

			if (retry > 1) {
				char result[32];
				input_err(true, &ts->client->dev, "%s: I2C retry %d, ret:%d\n",
					__func__, retry + 1, ret);
				ts->plat_data->hw_param.comm_err_count++;

				snprintf(result, sizeof(result), "RESULT=I2C");
				if (ts->probe_done)
					sec_cmd_send_event_to_user(&ts->sec, NULL, result);
			}
		}
	} else {
		/*
		 * I2C read buffer is 256 byte. do not support long buffer over than 256.
		 * So, try to seperate reading data about 256 bytes.
		 */
		for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, msg, 1);
			if (ret == 1)
				break;
			usleep_range(1 * 1000, 1 * 1000);
			if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
				input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
				mutex_unlock(&ts->read_write_mutex);
				goto err;
			}

			if (retry > 1) {
				input_err(true, &ts->client->dev, "%s: I2C retry %d, ret:%d\n",
					__func__, retry + 1, ret);
				ts->plat_data->hw_param.comm_err_count++;
			}
		}

		do {
			if (remain > ts->plat_data->i2c_burstmax)
				msg[1].len = ts->plat_data->i2c_burstmax;
			else
				msg[1].len = remain;

			remain -= ts->plat_data->i2c_burstmax;

			for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
				ret = i2c_transfer(ts->client->adapter, &msg[1], 1);
				if (ret == 1)
					break;
				usleep_range(1 * 1000, 1 * 1000);
				if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
					input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
					mutex_unlock(&ts->read_write_mutex);
					goto err;
				}

				if (retry > 1) {
					input_err(true, &ts->client->dev, "%s: I2C retry %d, ret:%d\n",
						__func__, retry + 1, ret);
					ts->plat_data->hw_param.comm_err_count++;
				}
			}
			msg[1].buf += msg[1].len;
		} while (remain > 0);
	}

	mutex_unlock(&ts->read_write_mutex);

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
		ret = -EIO;
		if (ts->probe_done && !ts->reset_is_on_going && !ts->plat_data->shutdown_called)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
	}

	memcpy(data, msg_buff, len);
	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		pr_info("sec_input:i2c_cmd: R: %02X | ", *reg);
		for (i = 0; i < len; i++)
			pr_cont("%02X ", data[i]);
		pr_cont("\n");
	}

	kfree(buf);
	kfree(msg_buff);
	return ret;
err:
	kfree(buf);
	kfree(msg_buff);
	return -EIO;
}

int stm_ts_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct stm_ts_data *ts;
	struct sec_ts_plat_data *pdata;
	struct sec_tclm_data *tdata;
	int ret = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "%s: EIO err!\n", __func__);
		return -EIO;
	}

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
	ts->stm_ts_read = stm_ts_i2c_read;
	ts->stm_ts_write = stm_ts_i2c_write;
	ts->stm_ts_read_sponge = stm_ts_read_from_sponge;
	ts->stm_ts_write_sponge = stm_ts_write_to_sponge;
	ts->tdata = tdata;
#ifdef TCLM_CONCEPT
	ts->tdata->client = ts->client;
	ts->tdata->tclm_read = stm_tclm_i2c_data_read;
	ts->tdata->tclm_write = stm_tclm_i2c_data_write;
	ts->tdata->tclm_execute_force_calibration = stm_ts_tclm_execute_force_calibration;
#endif
	i2c_set_clientdata(client, ts);

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

int stm_ts_i2c_remove(struct i2c_client *client)
{
	struct stm_ts_data *ts = i2c_get_clientdata(client);
	int ret = 0;

	ret = stm_ts_remove(ts);

	return 0;
}

void stm_ts_i2c_shutdown(struct i2c_client *client)
{
	struct stm_ts_data *ts = i2c_get_clientdata(client);

	stm_ts_shutdown(ts);
}


#if IS_ENABLED(CONFIG_PM)
static int stm_ts_i2c_pm_suspend(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	stm_ts_pm_suspend(ts);

	return 0;
}

static int stm_ts_i2c_pm_resume(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	stm_ts_pm_resume(ts);

	return 0;
}
#endif


static const struct i2c_device_id stm_ts_id[] = {
	{ STM_TS_I2C_NAME, 0 },
	{ },
};

#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops stm_ts_dev_pm_ops = {
	.suspend = stm_ts_i2c_pm_suspend,
	.resume = stm_ts_i2c_pm_resume,
};
#endif

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id stm_ts_match_table[] = {
	{ .compatible = "stm,stm_ts",},
	{ },
};
#else
#define stm_ts_match_table NULL
#endif

static struct i2c_driver stm_ts_driver = {
	.probe		= stm_ts_i2c_probe,
	.remove		= stm_ts_i2c_remove,
	.shutdown	= stm_ts_i2c_shutdown,
	.id_table	= stm_ts_id,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= STM_TS_I2C_NAME,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = stm_ts_match_table,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &stm_ts_dev_pm_ops,
#endif
	},
};

module_i2c_driver(stm_ts_driver);

MODULE_SOFTDEP("pre: acpm-mfd-bus");
MODULE_DESCRIPTION("stm TouchScreen driver");
MODULE_LICENSE("GPL");
