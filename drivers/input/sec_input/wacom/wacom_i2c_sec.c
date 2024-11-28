/*
 *  wacom_i2c_func.c - Wacom G5 Digitizer Controller (I2C bus)
 *
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "wacom_dev.h"

int calibration_trx_data(struct wacom_i2c *wac_i2c);
void calculate_ratio(struct wacom_i2c *wac_i2c);
void make_decision(struct wacom_i2c *wac_i2c, u16* arrResult);
void print_elec_data(struct wacom_i2c *wac_i2c);
void print_trx_data(struct wacom_i2c *wac_i2c);
void print_cal_trx_data(struct wacom_i2c *wac_i2c);
void print_ratio_trx_data(struct wacom_i2c *wac_i2c);
void print_difference_ratio_trx_data(struct wacom_i2c *wac_i2c);

static inline int power(int num)
{
	int i, ret;

	for (i = 0, ret = 1; i < num; i++)
		ret = ret * 10;

	return ret;
}

#if WACOM_SEC_FACTORY
static ssize_t epen_fac_select_firmware_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	u8 fw_update_way = FW_NONE;
	int ret = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	switch (*buf) {
	case 'k':
	case 'K':
		fw_update_way = FW_BUILT_IN;
		break;
	case 't':
	case 'T':
		fw_update_way = FW_FACTORY_GARAGE;
		break;
	case 'u':
	case 'U':
		fw_update_way = FW_FACTORY_UNIT;
		break;
	default:
		input_err(true, &client->dev, "wrong parameter\n");
		return count;
	}

	ret = wacom_i2c_load_fw(wac_i2c, fw_update_way);
	if (ret < 0) {
		input_info(true, &client->dev, "failed to load fw data\n");
		goto out;
	}

	wacom_i2c_unload_fw(wac_i2c);
out:
	return count;
}
#endif

static ssize_t epen_firm_update_status_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int status = wac_i2c->update_status;
	int ret = 0;

	input_info(true, &client->dev, "%s:(%d)\n", __func__, status);

	if (status == FW_UPDATE_PASS)
		ret = snprintf(buf, PAGE_SIZE, "PASS\n");
	else if (status == FW_UPDATE_RUNNING)
		ret = snprintf(buf, PAGE_SIZE, "DOWNLOADING\n");
	else if (status == FW_UPDATE_FAIL)
		ret = snprintf(buf, PAGE_SIZE, "FAIL\n");
	else
		ret = 0;

	return ret;
}

static ssize_t epen_firm_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	int i;

	for (i = 0; i < 4; i++)
		wac_i2c->pdata->img_version_of_ic[i] = 0;

	wacom_i2c_query(wac_i2c);
#endif

	input_info(true, &client->dev, "%s: %02X%02X%02X%02X|%02X%02X%02X%02X\n", __func__,
			wac_i2c->pdata->img_version_of_ic[0], wac_i2c->pdata->img_version_of_ic[1],
			wac_i2c->pdata->img_version_of_ic[2], wac_i2c->pdata->img_version_of_ic[3],
			wac_i2c->pdata->img_version_of_bin[0], wac_i2c->pdata->img_version_of_bin[1],
			wac_i2c->pdata->img_version_of_bin[2], wac_i2c->pdata->img_version_of_bin[3]);

	return snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X\t%02X%02X%02X%02X\n",
			wac_i2c->pdata->img_version_of_ic[0], wac_i2c->pdata->img_version_of_ic[1],
			wac_i2c->pdata->img_version_of_ic[2], wac_i2c->pdata->img_version_of_ic[3],
			wac_i2c->pdata->img_version_of_bin[0], wac_i2c->pdata->img_version_of_bin[1],
			wac_i2c->pdata->img_version_of_bin[2], wac_i2c->pdata->img_version_of_bin[3]);
}

static ssize_t epen_firmware_update_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	u8 fw_update_way = FW_NONE;

	input_info(true, &client->dev, "%s\n", __func__);

	mutex_lock(&wac_i2c->lock);

	switch (*buf) {
	case 'i':
	case 'I':
#if WACOM_PRODUCT_SHIP
		fw_update_way = FW_IN_SDCARD_SIGNED;
#else
		fw_update_way = FW_IN_SDCARD;
#endif
		break;
	case 'k':
	case 'K':
		fw_update_way = FW_BUILT_IN;
		break;
#if WACOM_SEC_FACTORY
	case 't':
	case 'T':
		fw_update_way = FW_FACTORY_GARAGE;
		break;
	case 'u':
	case 'U':
		fw_update_way = FW_FACTORY_UNIT;
		break;
#endif
	default:
		input_err(true, &client->dev, "wrong parameter\n");
		mutex_unlock(&wac_i2c->lock);
		return count;
	}

	wacom_fw_update_on_hidden_menu(wac_i2c, fw_update_way);
	mutex_unlock(&wac_i2c->lock);

	return count;
}

static ssize_t epen_firm_version_of_ic_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);

	input_info(true, &wac_i2c->client->dev, "%s: %02X%02X%02X%02X\n", __func__,
			wac_i2c->pdata->img_version_of_ic[0], wac_i2c->pdata->img_version_of_ic[1],
			wac_i2c->pdata->img_version_of_ic[2], wac_i2c->pdata->img_version_of_ic[3]);

	return snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X",
			wac_i2c->pdata->img_version_of_ic[0], wac_i2c->pdata->img_version_of_ic[1],
			wac_i2c->pdata->img_version_of_ic[2], wac_i2c->pdata->img_version_of_ic[3]);
}

static ssize_t epen_firm_version_of_bin_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);

	input_info(true, &wac_i2c->client->dev, "%s: %02X%02X%02X%02X\n", __func__,
			wac_i2c->pdata->img_version_of_bin[0], wac_i2c->pdata->img_version_of_bin[1],
			wac_i2c->pdata->img_version_of_bin[2], wac_i2c->pdata->img_version_of_bin[3]);

	return snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X",
			wac_i2c->pdata->img_version_of_bin[0], wac_i2c->pdata->img_version_of_bin[1],
			wac_i2c->pdata->img_version_of_bin[2], wac_i2c->pdata->img_version_of_bin[3]);
}


static ssize_t epen_reset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int val;

	if (kstrtoint(buf, 0, &val)) {
		input_err(true, &client->dev, "%s: failed to get param\n",
				__func__);
		return count;
	}

	val = !(!val);
	if (!val)
		goto out;

	wacom_enable_irq(wac_i2c, false);

	wac_i2c->function_result &= ~EPEN_EVENT_SURVEY;
	wac_i2c->survey_mode = EPEN_SURVEY_MODE_NONE;

	/* Reset IC */
	wacom_reset_hw(wac_i2c);
	/* I2C Test */
	wacom_i2c_query(wac_i2c);

	wacom_enable_irq(wac_i2c, true);

	input_info(true, &client->dev,
			"%s: result %d\n", __func__, wac_i2c->query_status);
	return count;

out:
	input_err(true, &client->dev, "%s: invalid value %d\n", __func__, val);

	return count;
}

static ssize_t epen_reset_result_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int ret = 0;

	if (wac_i2c->query_status) {
		input_info(true, &client->dev, "%s: PASS\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "PASS\n");
	} else {
		input_err(true, &client->dev, "%s: FAIL\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "FAIL\n");
	}

	return ret;
}

static ssize_t epen_checksum_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int val;

	if (kstrtoint(buf, 0, &val)) {
		input_err(true, &client->dev, "%s: failed to get param\n",
				__func__);
		return count;
	}

	val = !(!val);
	if (!val)
		goto out;

	wacom_enable_irq(wac_i2c, false);

	wacom_checksum(wac_i2c);

	wacom_enable_irq(wac_i2c, true);

	input_info(true, &client->dev,
			"%s: result %d\n", __func__, wac_i2c->checksum_result);

	return count;

out:
	input_err(true, &client->dev, "%s: invalid value %d\n", __func__, val);

	return count;
}

static ssize_t epen_checksum_result_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int ret;

	if (wac_i2c->checksum_result) {
		input_info(true, &client->dev, "checksum, PASS\n");
		ret = snprintf(buf, PAGE_SIZE, "PASS\n");
	} else {
		input_err(true, &client->dev, "checksum, FAIL\n");
		ret = snprintf(buf, PAGE_SIZE, "FAIL\n");
	}

	return ret;
}

int wacom_open_test(struct wacom_i2c *wac_i2c, int test_mode)
{
	struct i2c_client *client = wac_i2c->client;
	u8 cmd = 0;
	u8 buf[COM_COORD_NUM + 1] = { 0, };
	int ret = 0, retry = 0, retval = 0;
	int retry_int = 30;

	input_info(true, &client->dev, "%s : start (%d)\n", __func__, test_mode);

	if (test_mode == WACOM_GARAGE_TEST) {
		wac_i2c->garage_connection_check = false;
		wac_i2c->garage_fail_channel = 0;
		wac_i2c->garage_min_adc_val = 0;
		wac_i2c->garage_error_cal = 0;
		wac_i2c->garage_min_cal_val = 0;
	} else if (test_mode == WACOM_DIGITIZER_TEST) {
		wac_i2c->connection_check = false;
		wac_i2c->fail_channel = 0;
		wac_i2c->min_adc_val = 0;
		wac_i2c->error_cal = 0;
		wac_i2c->min_cal_val = 0;
	}

	if(!wac_i2c->pdata->support_garage_open_test && test_mode == WACOM_GARAGE_TEST){
		input_err(true, &client->dev, "%s: not support garage open test", __func__);
		retval = EPEN_OPEN_TEST_NOTSUPPORT;
		goto err1;
	}

	/* change normal mode for open test */
	mutex_lock(&wac_i2c->mode_lock);
	wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_NONE);
	// prevent I2C fail
	msleep(300);
	mutex_unlock(&wac_i2c->mode_lock);

	ret = wacom_start_stop_cmd(wac_i2c, WACOM_STOP_CMD);
	if (ret != 1) {
		input_err(true, &client->dev, "%s: failed to set stop cmd, %d\n",
				__func__, ret);
		retval = EPEN_OPEN_TEST_EIO;
		goto err1;
	}

	msleep(50);

	wacom_enable_irq(wac_i2c, false);

	if (test_mode == WACOM_GARAGE_TEST) {
		cmd = COM_GARAGE_TEST_MODE;
	} else if (test_mode == WACOM_DIGITIZER_TEST) {
		cmd = COM_DIGITIZER_TEST_MODE;
	}

	if (wac_i2c->pdata->support_garage_open_test) {
		input_info(true, &client->dev, "%s : send data(%02x)\n", __func__, cmd);

		ret = wacom_i2c_send(wac_i2c, &cmd, 1);
		if (ret != 1) {
			input_err(true, &client->dev, "%s : failed to send data(%02x)\n", __func__, cmd);
			retval = EPEN_OPEN_TEST_EIO;
			goto err2;
		}
		msleep(50);
	}

	cmd = COM_OPEN_CHECK_START;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1);
	if (ret != 1) {
		input_err(true, &client->dev, "%s : failed to send data(%02x)\n", __func__, cmd);
		retval = EPEN_OPEN_TEST_EIO;
		goto err2;
	}

	msleep(50);

	retry = 5;
	cmd = COM_OPEN_CHECK_STATUS;
	do {
		input_info(true, &client->dev, "%s : read status, retry %d\n", __func__, retry);
		ret = wacom_i2c_send(wac_i2c, &cmd, 1);
		if (ret != 1) {
			input_err(true, &client->dev,
					"%s : failed to send data(%02x)\n", __func__, cmd);
			usleep_range(4500, 5500);
			continue;
		}

		retry_int = 30;
		while (retry_int--) {
			if (!gpio_get_value(wac_i2c->pdata->irq_gpio))
				break;
			else
				msleep(10);
		}
		input_info(true, &client->dev, "%s : retry_int[%d]\n", __func__, retry_int);

		ret = wacom_i2c_recv(wac_i2c, buf, COM_COORD_NUM);
		if (ret != COM_COORD_NUM) {
			input_err(true, &client->dev, "%s : failed to recv\n", __func__);
			usleep_range(4500, 5500);
			continue;
		}

		if (buf[0] != 0x0E && buf[1] != 0x01) {
			input_err(true, &client->dev,
					"%s : invalid packet(%02x %02x %02x)\n",
					__func__, buf[0], buf[1], buf[3]);
			continue;
		}
		/*
		 * status value
		 * 0 : data is not ready
		 * 1 : PASS
		 * 2 : Fail (coil function error)
		 * 3 : Fail (All coil function error)
		 */
		if (buf[2] == 1) {
			input_info(true, &client->dev, "%s : Open check Pass\n", __func__);
			break;
		}
	} while (retry--);

	if (test_mode == WACOM_GARAGE_TEST) {
		if (ret == COM_COORD_NUM) {
			if (wac_i2c->pdata->module_ver == 0x2)
				wac_i2c->garage_connection_check = ((buf[2] == 1) && !buf[6]);
			else
				wac_i2c->garage_connection_check = (buf[2] == 1);
		} else {
			wac_i2c->garage_connection_check = false;
			retval = EPEN_OPEN_TEST_EIO;
			goto err2;
		}

		wac_i2c->garage_fail_channel = buf[3];
		wac_i2c->garage_min_adc_val = buf[4] << 8 | buf[5];
		wac_i2c->garage_error_cal = buf[6];
		wac_i2c->garage_min_cal_val = buf[7] << 8 | buf[8];

		input_info(true, &client->dev,
				"%s: garage %s buf[3]:%d, buf[4]:%d, buf[5]:%d, buf[6]:%d, buf[7]:%d, buf[8]:%d\n",
				__func__, wac_i2c->garage_connection_check ? "Pass" : "Fail",
				buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);

		if (!wac_i2c->garage_connection_check)
			retval = EPEN_OPEN_TEST_FAIL;

	} else if (test_mode == WACOM_DIGITIZER_TEST) {
		if (ret == COM_COORD_NUM) {
			if (wac_i2c->pdata->module_ver == 0x2)
				wac_i2c->connection_check = ((buf[2] == 1) && !buf[6]);
			else
				wac_i2c->connection_check = (buf[2] == 1);
		} else {
			wac_i2c->connection_check = false;
			retval = EPEN_OPEN_TEST_EIO;
			goto err2;
		}

		wac_i2c->fail_channel = buf[3];
		wac_i2c->min_adc_val = buf[4] << 8 | buf[5];
		wac_i2c->error_cal = buf[6];
		wac_i2c->min_cal_val = buf[7] << 8 | buf[8];

		input_info(true, &client->dev,
				"%s: digitizer %s buf[3]:%d, buf[4]:%d, buf[5]:%d, buf[6]:%d, buf[7]:%d, buf[8]:%d\n",
				__func__, wac_i2c->connection_check ? "Pass" : "Fail",
				buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);

		if (!wac_i2c->connection_check)
			retval = EPEN_OPEN_TEST_FAIL;
	}

err2:
	wacom_enable_irq(wac_i2c, true);

err1:
	ret = wacom_start_stop_cmd(wac_i2c, WACOM_STOP_AND_START_CMD);
	if (ret != 1) {
		input_err(true, &client->dev, "%s: failed to set stop-start cmd, %d\n",
				__func__, ret);
		retval = EPEN_OPEN_TEST_EMODECHG;
		goto out;
	}

out:
	return retval;
}

static int get_connection_test(struct wacom_i2c *wac_i2c, int test_mode)
{
	struct i2c_client *client = wac_i2c->client;

	int retry = 2;
	int ret;

	mutex_lock(&wac_i2c->lock);

	input_info(true, &client->dev, "%s : start(%d)\n", __func__, test_mode);

	wac_i2c->is_open_test = true;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	if (wac_i2c->is_tsp_block) {
		input_info(true, &wac_i2c->client->dev, "%s: full scan OUT\n", __func__);
		wac_i2c->tsp_scan_mode = sec_input_notify(&wac_i2c->nb, NOTIFIER_TSP_BLOCKING_RELEASE, NULL);
		wac_i2c->is_tsp_block = false;
	}
#endif

	while (retry--) {
		ret = wacom_open_test(wac_i2c, test_mode);
		if (ret == EPEN_OPEN_TEST_PASS || ret == EPEN_OPEN_TEST_NOTSUPPORT)
			break;

		input_err(true, &client->dev, "failed(%d). retry %d\n", ret, retry);

		wacom_power(wac_i2c, false);
		wac_i2c->function_result &= ~EPEN_EVENT_SURVEY;
		wac_i2c->survey_mode = EPEN_SURVEY_MODE_NONE;
		/* recommended delay in spec */
		msleep(100);
		wacom_power(wac_i2c, true);

		if (ret == EPEN_OPEN_TEST_EIO || ret == EPEN_OPEN_TEST_FAIL) {
			msleep(150);
			continue;
		} else if (ret == EPEN_OPEN_TEST_EMODECHG) {
			break;
		}
	}

	if (!wac_i2c->samplerate_state) {
		input_info(true, &client->dev, "%s: samplerate state is %d, need to recovery\n",
				__func__, wac_i2c->samplerate_state);

		ret = wacom_start_stop_cmd(wac_i2c, WACOM_START_CMD);
		if (ret != 1) {
			input_err(true, &client->dev, "%s: failed to set start cmd, %d\n",
					__func__, ret);
		}
	}

	wac_i2c->is_open_test = false;

	mutex_unlock(&wac_i2c->lock);

	input_info(true, &client->dev, "connection_check : %s\n",
			wac_i2c->connection_check ? "OK" : "NG");

	return ret;
}

static ssize_t epen_connection_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);

#if WACOM_SEC_FACTORY
	struct i2c_client *client = wac_i2c->client;
	int ret;

	ret = sec_input_get_lcd_id(&client->dev);
	if (ret < 0) {
		input_info(true, &client->dev, "%s: digitizer is not attached\n", __func__);
		goto out;
	}
#endif

	get_connection_test(wac_i2c, WACOM_DIGITIZER_TEST);

#if WACOM_SEC_FACTORY
out:
#endif
	if (wac_i2c->pdata->module_ver == 0x2) {
		return snprintf(buf, PAGE_SIZE, "%s %d %d %d %s %d\n",
				wac_i2c->connection_check ? "OK" : "NG",
				wac_i2c->pdata->module_ver, wac_i2c->fail_channel,
				wac_i2c->min_adc_val, wac_i2c->error_cal ? "NG" : "OK",
				wac_i2c->min_cal_val);
	} else {
		return snprintf(buf, PAGE_SIZE, "%s %d %d %d\n",
				wac_i2c->connection_check ? "OK" : "NG",
				wac_i2c->pdata->module_ver, wac_i2c->fail_channel,
				wac_i2c->min_adc_val);
	}
}

static ssize_t epen_saving_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	static u64 call_count;
	int val;

	call_count++;

	if (kstrtoint(buf, 0, &val)) {
		input_err(true, &client->dev, "%s: failed to get param\n",
				__func__);
		return count;
	}

	wac_i2c->battery_saving_mode = !(!val);

	input_info(true, &client->dev, "%s: ps %s & pen %s [%llu]\n",
			__func__, wac_i2c->battery_saving_mode ? "on" : "off",
			(wac_i2c->function_result & EPEN_EVENT_PEN_OUT) ? "out" : "in",
			call_count);

	if (!wac_i2c->power_enable && wac_i2c->battery_saving_mode) {
		input_err(true, &wac_i2c->client->dev,
				"%s: already power off, save & return\n", __func__);
		return count;
	}

	if (!(wac_i2c->function_result & EPEN_EVENT_PEN_OUT)) {
		wacom_select_survey_mode(wac_i2c, wac_i2c->screen_on);

		if (wac_i2c->battery_saving_mode)
			forced_release_fullscan(wac_i2c);
	}

	return count;
}

static ssize_t epen_screen_off_memo_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int val = 0;

	if (kstrtoint(buf, 0, &val)) {
		input_err(true, &client->dev, "%s: failed to get param\n",
				__func__);
		return count;
	}

#if WACOM_SEC_FACTORY
	input_info(true, &client->dev,
			"%s : Not support screen off memo mode(%d) in Factory Bin\n",
			__func__, val);

	return count;
#endif

	val = !(!val);

	if (val)
		wac_i2c->function_set |= EPEN_SETMODE_AOP_OPTION_SCREENOFFMEMO;
	else
		wac_i2c->function_set &= (~EPEN_SETMODE_AOP_OPTION_SCREENOFFMEMO);

	input_info(true, &client->dev,
			"%s: ps %s aop(%d) set(0x%x) ret(0x%x)\n", __func__,
			wac_i2c->battery_saving_mode ? "on" : "off",
			(wac_i2c->function_set & EPEN_SETMODE_AOP) ? 1 : 0,
			wac_i2c->function_set, wac_i2c->function_result);

	if (!wac_i2c->screen_on)
		wacom_select_survey_mode(wac_i2c, false);

	return count;
}

static ssize_t epen_aod_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int val = 0;

	if (kstrtoint(buf, 0, &val)) {
		input_err(true, &client->dev, "%s: failed to get param\n",
				__func__);
		return count;
	}

#if WACOM_SEC_FACTORY
	input_info(true, &client->dev,
			"%s : Not support aod mode(%d) in Factory Bin\n",
			__func__, val);

	return count;
#endif

	val = !(!val);

	if (val)
		wac_i2c->function_set |= EPEN_SETMODE_AOP_OPTION_AOD_LCD_OFF;
	else
		wac_i2c->function_set &= ~EPEN_SETMODE_AOP_OPTION_AOD_LCD_ON;

	input_info(true, &client->dev,
			"%s: ps %s aop(%d) set(0x%x) ret(0x%x)\n", __func__,
			wac_i2c->battery_saving_mode ? "on" : "off",
			(wac_i2c->function_set & EPEN_SETMODE_AOP) ? 1 : 0,
			wac_i2c->function_set, wac_i2c->function_result);

	if (!wac_i2c->screen_on)
		wacom_select_survey_mode(wac_i2c, false);

	return count;
}

static ssize_t epen_aod_lcd_onoff_status_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int val = 0;

	if (kstrtoint(buf, 0, &val)) {
		input_err(true, &client->dev, "%s: failed to get param\n",
				__func__);
		return count;
	}

#if WACOM_SEC_FACTORY
	input_info(true, &client->dev,
			"%s : Not support aod mode(%d) in Factory Bin\n",
			__func__, val);

	return count;
#endif

	val = !(!val);

	if (val)
		wac_i2c->function_set |= EPEN_SETMODE_AOP_OPTION_AOD_LCD_STATUS;
	else
		wac_i2c->function_set &= ~EPEN_SETMODE_AOP_OPTION_AOD_LCD_STATUS;

	if (!(wac_i2c->function_set & EPEN_SETMODE_AOP_OPTION_AOD) || wac_i2c->screen_on)
		goto out;

	if (wac_i2c->survey_mode == EPEN_SURVEY_MODE_GARAGE_AOP) {
		if ((wac_i2c->function_set & EPEN_SETMODE_AOP_OPTION_AOD_LCD_ON)
				== EPEN_SETMODE_AOP_OPTION_AOD_LCD_OFF) {
			forced_release_fullscan(wac_i2c);
		}

		mutex_lock(&wac_i2c->mode_lock);
		wacom_i2c_set_survey_mode(wac_i2c, wac_i2c->survey_mode);
		mutex_unlock(&wac_i2c->mode_lock);
	}

out:
	input_info(true, &client->dev, "%s: screen %s, survey mode:0x%x, set:0x%x\n",
			__func__, wac_i2c->screen_on ? "on" : "off",
			wac_i2c->survey_mode, wac_i2c->function_set);

	return count;
}

static ssize_t epen_aot_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int val = 0;

	if (kstrtoint(buf, 0, &val)) {
		input_err(true, &client->dev, "%s: failed to get param\n",
				__func__);
		return count;
	}

#if WACOM_SEC_FACTORY
	input_info(true, &client->dev,
			"%s : Not support aot mode(%d) in Factory Bin\n",
			__func__, val);

	return count;
#endif

	val = !(!val);

	if (val)
		wac_i2c->function_set |= EPEN_SETMODE_AOP_OPTION_AOT;
	else
		wac_i2c->function_set &= (~EPEN_SETMODE_AOP_OPTION_AOT);

	input_info(true, &client->dev,
			"%s: ps %s aop(%d) set(0x%x) ret(0x%x)\n", __func__,
			wac_i2c->battery_saving_mode ? "on" : "off",
			(wac_i2c->function_set & EPEN_SETMODE_AOP) ? 1 : 0,
			wac_i2c->function_set, wac_i2c->function_result);

	if (!wac_i2c->screen_on)
		wacom_select_survey_mode(wac_i2c, false);

	return count;
}

static ssize_t epen_wcharging_mode_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;

	input_info(true, &client->dev, "%s: %s\n", __func__,
			!wac_i2c->wcharging_mode ? "NORMAL" : "LOWSENSE");

	return snprintf(buf, PAGE_SIZE, "%s\n",
			!wac_i2c->wcharging_mode ? "NORMAL" : "LOWSENSE");
}

static ssize_t epen_wcharging_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int retval = 0;

	if (kstrtoint(buf, 0, &retval)) {
		input_err(true, &client->dev, "%s: failed to get param\n",
				__func__);
		return count;
	}

	wac_i2c->wcharging_mode = retval;

	input_info(true, &client->dev, "%s: %d\n", __func__,
			wac_i2c->wcharging_mode);

	if (!wac_i2c->power_enable) {
		input_err(true, &wac_i2c->client->dev,
				"%s: power off, save & return\n", __func__);
		return count;
	}

	retval = wacom_i2c_set_sense_mode(wac_i2c);
	if (retval < 0) {
		input_err(true, &client->dev,
				"%s: do not set sensitivity mode, %d\n", __func__,
				retval);
	}

	return count;

}

static ssize_t epen_disable_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int val = 0;

	if (kstrtoint(buf, 0, &val)) {
		input_err(true, &client->dev, "%s: failed to get param\n",
				__func__);
		return count;
	}

	if (val)
		wacom_disable_mode(wac_i2c, WACOM_DISABLE);
	else
		wacom_disable_mode(wac_i2c, WACOM_ENABLE);

	input_info(true, &client->dev, "%s: %d\n", __func__, val);

	return count;
}

static ssize_t hardware_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[516];
	char tbuff[128];

	memset(buff, 0x00, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"RESET\":\"%d\",", wac_i2c->abnormal_reset_count);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"I2C_FAIL\":\"%d\",", wac_i2c->i2c_fail_count);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"PEN_OUT\":\"%d\",", wac_i2c->pen_out_count);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"SDCONN\":\"%d\",", wac_i2c->connection_check);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"SECCNT\":\"%d\",", wac_i2c->fail_channel);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"SADCVAL\":\"%d\",", wac_i2c->min_adc_val);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "\"SECLVL\":\"%d\",", wac_i2c->error_cal);
	strlcat(buff, tbuff, sizeof(buff));

	memset(tbuff, 0x00, sizeof(tbuff));
	/* remove last comma (csv) */
	snprintf(tbuff, sizeof(tbuff), "\"SCALLVL\":\"%d\"", wac_i2c->min_cal_val);
	strlcat(buff, tbuff, sizeof(buff));

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}

static ssize_t hardware_param_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);

	wac_i2c->abnormal_reset_count = 0;
	wac_i2c->i2c_fail_count = 0;
	wac_i2c->pen_out_count = 0;

	input_info(true, &wac_i2c->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t epen_connection_check_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;

	input_info(true, &client->dev, "%s: SDCONN:%d,SECCNT:%d,SADCVAL:%d,SECLVL:%d,SCALLVL:%d\n",
			__func__, wac_i2c->connection_check,
			wac_i2c->fail_channel, wac_i2c->min_adc_val,
			wac_i2c->error_cal, wac_i2c->min_cal_val);

	return snprintf(buf, PAGE_SIZE,
			"\"SDCONN\":\"%d\",\"SECCNT\":\"%d\",\"SADCVAL\":\"%d\",\"SECLVL\":\"%d\",\"SCALLVL\":\"%d\"",
			wac_i2c->connection_check, wac_i2c->fail_channel,
			wac_i2c->min_adc_val, wac_i2c->error_cal,
			wac_i2c->min_cal_val);
}

static ssize_t get_epen_pos_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	int max_x, max_y;

	if (wac_i2c->pdata->xy_switch) {
		max_x = wac_i2c->pdata->max_y;
		max_y = wac_i2c->pdata->max_x;
	} else {
		max_x = wac_i2c->pdata->max_x;
		max_y = wac_i2c->pdata->max_y;
	}

	input_info(true, &client->dev,
			"%s: id(%d), x(%d), y(%d), max_x(%d), max_y(%d)\n", __func__,
			wac_i2c->survey_pos.id, wac_i2c->survey_pos.x, wac_i2c->survey_pos.y,
			max_x, max_y);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d\n",
			wac_i2c->survey_pos.id, wac_i2c->survey_pos.x, wac_i2c->survey_pos.y,
			max_x, max_y);
}

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	u32 feature = 0;

	if (wac_i2c->pdata->enable_sysinput_enabled)
		feature |= INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED;

	input_info(true, &client->dev,"%s: %d%s\n",
			__func__, feature,
			feature & INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED ? " SE" : "");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static int wacom_input_enable_device(struct input_dev *dev)
{
	int retval;

	retval = mutex_lock_interruptible(&dev->mutex);
	if (retval)
		return retval;

	if (dev->users && dev->open)
		retval = dev->open(dev);

	mutex_unlock(&dev->mutex);

	return retval;
}

static int wacom_input_disable_device(struct input_dev *dev)
{
	int retval;

	retval = mutex_lock_interruptible(&dev->mutex);
	if (retval)
		return retval;

	if (dev->users && dev->close)
		dev->close(dev);

	mutex_unlock(&dev->mutex);
	return 0;
}

static ssize_t flip_status_detect_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);

	input_info(true, &wac_i2c->client->dev, "%s: %d\n",
			__func__,	wac_i2c->flip_state);

	return snprintf(buf, PAGE_SIZE, "%d\n", wac_i2c->flip_state);
}

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!g_wac_i2c->pdata->enable_sysinput_enabled)
		return -EINVAL;

	input_info(true, &g_wac_i2c->client->dev, "%s: %d\n", __func__, g_wac_i2c->pdata->enabled);
	return scnprintf(buf, PAGE_SIZE, "%d\n", g_wac_i2c->pdata->enabled);
}

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	int buff[2];

	if (!g_wac_i2c->pdata->enable_sysinput_enabled)
		return -EINVAL;

	ret = sscanf(buf, "%d,%d", &buff[0], &buff[1]);
	if (ret != 2) {
		input_err(true, &g_wac_i2c->client->dev,
				"%s: failed read params [%d]\n", __func__, ret);
		return -EINVAL;
	}

	if (buff[0] == DISPLAY_STATE_ON && buff[1] == DISPLAY_EVENT_LATE) {
		if (g_wac_i2c->pdata->enabled) {
			input_info(true, &g_wac_i2c->client->dev, "%s: [%s] device already enabled\n", __func__, current->comm);
			goto out;
		}
		input_info(true, &g_wac_i2c->client->dev, "%s: [%s] enable\n", __func__, current->comm);
		wacom_input_enable_device(g_wac_i2c->input_dev);
	} else if (buff[0] == DISPLAY_STATE_OFF && buff[1] == DISPLAY_EVENT_EARLY) {
		if (!g_wac_i2c->pdata->enabled) {
			input_info(true, &g_wac_i2c->client->dev, "%s: [%s] device already disabled\n", __func__, current->comm);
			goto out;
		}
		input_info(true, &g_wac_i2c->client->dev, "%s: [%s] disable\n", __func__, current->comm);
		wacom_input_disable_device(g_wac_i2c->input_dev);
	} else if (buff[0] == DISPLAY_STATE_FORCE_ON) {
		input_info(true, &g_wac_i2c->client->dev, "%s: [%s] DISPLAY_STATE_FORCE_ON\n", __func__, current->comm);
		wacom_input_enable_device(g_wac_i2c->input_dev);
	} else if (buff[0] == DISPLAY_STATE_FORCE_OFF) {
		input_info(true, &g_wac_i2c->client->dev, "%s: [%s] DISPLAY_STATE_FORCE_OFF\n", __func__, current->comm);
		wacom_input_disable_device(g_wac_i2c->input_dev);
	}
out:
	return count;
}

/* firmware update */
static DEVICE_ATTR(epen_firm_update, (S_IWUSR | S_IWGRP),
		NULL, epen_firmware_update_store);
/* return firmware update status */
static DEVICE_ATTR(epen_firm_update_status, S_IRUGO,
		epen_firm_update_status_show, NULL);
/* return firmware version */
static DEVICE_ATTR(epen_firm_version, S_IRUGO, epen_firm_version_show, NULL);
static DEVICE_ATTR(ver_of_ic, S_IRUGO, epen_firm_version_of_ic_show, NULL);
static DEVICE_ATTR(ver_of_bin, S_IRUGO, epen_firm_version_of_bin_show, NULL);

/* For SMD Test */
static DEVICE_ATTR(epen_reset, (S_IWUSR | S_IWGRP), NULL, epen_reset_store);
static DEVICE_ATTR(epen_reset_result, S_IRUGO, epen_reset_result_show, NULL);
/* For SMD Test. Check checksum */
static DEVICE_ATTR(epen_checksum, (S_IWUSR | S_IWGRP),
		NULL, epen_checksum_store);
static DEVICE_ATTR(epen_checksum_result, S_IRUGO,
		epen_checksum_result_show, NULL);
static DEVICE_ATTR(epen_connection, S_IRUGO, epen_connection_show, NULL);
static DEVICE_ATTR(epen_saving_mode, (S_IWUSR | S_IWGRP),
		NULL, epen_saving_mode_store);
static DEVICE_ATTR(epen_wcharging_mode, (S_IRUGO | S_IWUSR | S_IWGRP),
		epen_wcharging_mode_show, epen_wcharging_mode_store);
static DEVICE_ATTR(epen_disable_mode, (S_IWUSR | S_IWGRP),
		NULL, epen_disable_mode_store);
static DEVICE_ATTR(screen_off_memo_enable, (S_IWUSR | S_IWGRP),
		NULL, epen_screen_off_memo_enable_store);
static DEVICE_ATTR(get_epen_pos,
		S_IRUGO, get_epen_pos_show, NULL);
static DEVICE_ATTR(aod_enable, (S_IWUSR | S_IWGRP),
		NULL, epen_aod_enable_store);
static DEVICE_ATTR(aod_lcd_onoff_status, (S_IWUSR | S_IWGRP),
		NULL, epen_aod_lcd_onoff_status_store);
static DEVICE_ATTR(aot_enable, (S_IWUSR | S_IWGRP),
		NULL, epen_aot_enable_store);
static DEVICE_ATTR(hw_param, 0644, hardware_param_show, hardware_param_store);
static DEVICE_ATTR(epen_connection_check, S_IRUGO,
		epen_connection_check_show, NULL);
#if WACOM_SEC_FACTORY
static DEVICE_ATTR(epen_fac_select_firmware, (S_IWUSR | S_IWGRP),
		NULL, epen_fac_select_firmware_store);
#endif
static DEVICE_ATTR(flip_status_detect, 0444,
		flip_status_detect_show, NULL);
static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);
static DEVICE_ATTR(enabled, 0664, enabled_show, enabled_store);

static struct attribute *epen_attributes[] = {
	&dev_attr_epen_firm_update.attr,
	&dev_attr_epen_firm_update_status.attr,
	&dev_attr_epen_firm_version.attr,
	&dev_attr_ver_of_ic.attr,
	&dev_attr_ver_of_bin.attr,
	&dev_attr_epen_reset.attr,
	&dev_attr_epen_reset_result.attr,
	&dev_attr_epen_checksum.attr,
	&dev_attr_epen_checksum_result.attr,
	&dev_attr_epen_connection.attr,
	&dev_attr_epen_saving_mode.attr,
	&dev_attr_epen_wcharging_mode.attr,
	&dev_attr_epen_disable_mode.attr,
	&dev_attr_screen_off_memo_enable.attr,
	&dev_attr_get_epen_pos.attr,
	&dev_attr_aod_enable.attr,
	&dev_attr_aod_lcd_onoff_status.attr,
	&dev_attr_aot_enable.attr,
	&dev_attr_hw_param.attr,
	&dev_attr_epen_connection_check.attr,
#if WACOM_SEC_FACTORY
	&dev_attr_epen_fac_select_firmware.attr,
#endif
	&dev_attr_flip_status_detect.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_enabled.attr,
	NULL,
};

static struct attribute_group epen_attr_group = {
	.attrs = epen_attributes,
};

static void print_spec_data(struct wacom_i2c *wac_i2c)
{
	struct wacom_elec_data *edata = wac_i2c->pdata->edata;
	struct i2c_client *client = wac_i2c->client;
	u8 tmp_buf[WACOM_CMD_RESULT_WORD_LEN] = { 0 };
	u8 *buff;
	int buff_size;
	int i;

	buff_size = edata->max_x_ch > edata->max_y_ch ? edata->max_x_ch : edata->max_y_ch;
	buff_size = WACOM_CMD_RESULT_WORD_LEN * (buff_size + 1);

	buff = kzalloc(buff_size, GFP_KERNEL);
	if (!buff)
		return;

	input_info(true, &client->dev, "%s: shift_value %d, spec ver %d.%d", __func__,
			edata->shift_value, edata->spec_ver[0], edata->spec_ver[1]);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "xx_ref: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->xx_ref[i] * power(edata->shift_value));
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "xy_ref: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->xy_ref[i] * power(edata->shift_value));
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "yx_ref: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->yx_ref[i] * power(edata->shift_value));
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "yy_ref: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->yy_ref[i] * power(edata->shift_value));
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "xy_edg_ref: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->xy_edg_ref[i] * power(edata->shift_value));
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "yx_edg_ref: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->yx_edg_ref[i] * power(edata->shift_value));
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "xx_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->xx_spec[i] / POWER_OFFSET * power(edata->shift_value));
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "xy_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->xy_spec[i] / POWER_OFFSET * power(edata->shift_value));
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "yx_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->yx_spec[i] / POWER_OFFSET * power(edata->shift_value));
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "yy_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->yy_spec[i] / POWER_OFFSET * power(edata->shift_value));
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "rxx_ref: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->rxx_ref[i] * power(edata->shift_value) / POWER_OFFSET);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "rxy_ref: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->rxy_ref[i] * power(edata->shift_value) / POWER_OFFSET);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "ryx_ref: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->ryx_ref[i] * power(edata->shift_value) / POWER_OFFSET);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "ryy_ref: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->ryy_ref[i] * power(edata->shift_value) / POWER_OFFSET);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "drxx_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->drxx_spec[i] * power(edata->shift_value) / POWER_OFFSET);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "drxy_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->drxy_spec[i] * power(edata->shift_value) / POWER_OFFSET);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "dryx_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->dryx_spec[i] * power(edata->shift_value) / POWER_OFFSET);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "dryy_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->dryy_spec[i] * power(edata->shift_value) / POWER_OFFSET);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "drxy_edg_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->drxy_edg_spec[i] * power(edata->shift_value) / POWER_OFFSET);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "dryx_edg_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ",
				edata->dryx_edg_spec[i] * power(edata->shift_value) / POWER_OFFSET);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "xx_self_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ", edata->xx_self_spec[i]);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "yy_self_spec: ");
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, WACOM_CMD_RESULT_WORD_LEN, "%lld ", edata->yy_self_spec[i]);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, WACOM_CMD_RESULT_WORD_LEN);
	}

	input_info(true, &client->dev, "%s\n", buff);
	memset(buff, 0x00, buff_size);

	kfree(buff);
}

static void run_elec_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct wacom_elec_data *edata = NULL;
	struct i2c_client *client = wac_i2c->client;
	u8 *buff = NULL;
	u8 tmp_buf[WACOM_CMD_RESULT_WORD_LEN] = { 0 };
	u8 data[COM_ELEC_NUM] = { 0, };
	u16 tmp, offset;
	u8 cmd;
	int i, tx, rx;
	int retry = RETRY_COUNT;
	int ret;
	int max_ch;
	int buff_size;
	u32 count;
	int remain_data_count;
	u16 *elec_result = NULL;

	sec_cmd_set_default_result(sec);

	input_info(true, &client->dev, "%s: parm[%d]\n", __func__, sec->cmd_param[0]);

	if (sec->cmd_param[0] < EPEN_ELEC_DATA_MAIN || sec->cmd_param[0] > EPEN_ELEC_DATA_UNIT) {
		input_err(true, &client->dev,
				"%s: Abnormal parm[%d]\n", __func__, sec->cmd_param[0]);
		goto error;
	}

	edata = wac_i2c->pdata->edata = wac_i2c->pdata->edatas[sec->cmd_param[0]];

	if (!edata) {
		input_err(true, &client->dev, "%s: test spec is NULL\n", __func__);
		goto error;
	}

	max_ch = edata->max_x_ch + edata->max_y_ch;
	buff_size = max_ch * 2 *  WACOM_CMD_RESULT_WORD_LEN + SEC_CMD_STR_LEN;

	buff = kzalloc(buff_size, GFP_KERNEL);
	if (!buff)
		goto error;

	elec_result = kzalloc((max_ch + 1) * sizeof(u16), GFP_KERNEL);
	if (!elec_result)
		goto error;

	wacom_enable_irq(wac_i2c, false);

	/* change wacom mode to 0x2D */
	mutex_lock(&wac_i2c->mode_lock);
	wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_NONE);
	mutex_unlock(&wac_i2c->mode_lock);

	/* wait for status event for normal mode of wacom */
	msleep(250);
	do {
		ret = wacom_i2c_recv(wac_i2c, data, COM_COORD_NUM);
		if (ret != COM_COORD_NUM) {
			input_err(true, &client->dev, "%s: failed to recv status data\n",
					__func__);
			msleep(50);
			continue;
		}

		/* check packet ID */
		if ((data[0] & 0x0F) != NOTI_PACKET && (data[1] != CMD_PACKET)) {
			input_info(true, &client->dev,
					"%s: invalid status data %d\n",
					__func__, (RETRY_COUNT - retry + 1));
		} else {
			break;
		}

		msleep(50);
	} while(retry--);

	retry = RETRY_COUNT;
	cmd = COM_ELEC_XSCAN;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1);
	if (ret != 1) {
		input_err(true, &client->dev,
				"%s: failed to send 0x%02X\n", __func__, cmd);
		goto error_test;
	}
	msleep(30);

	cmd = COM_ELEC_TRSX0;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1);
	if (ret != 1) {
		input_err(true, &client->dev,
				"%s: failed to send 0x%02X\n", __func__, cmd);
		goto error_test;
	}
	msleep(30);

	cmd = COM_ELEC_SCAN_START;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1);
	if (ret != 1) {
		input_err(true, &client->dev,
				"%s: failed to send 0x%02X\n", __func__, cmd);
		goto error_test;
	}
	msleep(30);

	for (tx = 0; tx < max_ch; tx++) {
		/* x scan mode receive data through x coils */
		remain_data_count = edata->max_x_ch;

		do {
			cmd = COM_ELEC_REQUESTDATA;
			ret = wacom_i2c_send(wac_i2c, &cmd, 1);
			if (ret != 1) {
				input_err(true, &client->dev,
						"%s: failed to send 0x%02X\n",
						__func__, cmd);
				goto error_test;
			}
			msleep(30);

			ret = wacom_i2c_recv(wac_i2c, data, COM_ELEC_NUM);
			if (ret != COM_ELEC_NUM) {
				input_err(true, &client->dev,
						"%s: failed to receive elec data\n",
						__func__);
				goto error_test;
			}
#if 0
			input_info(true, &client->dev, "%X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X\n",
					data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9],
					data[10], data[11], data[12], data[13], data[14], data[15], data[16], data[17], data[18], data[19],
					data[20], data[21], data[22], data[23], data[24], data[25], data[26], data[27],	data[28], data[29],
					data[30], data[31], data[32], data[33], data[34], data[35], data[36], data[37]);
#endif
			/* check packet ID & sub packet ID */
			if (((data[0] & 0x0F) != 0x0E) && (data[1] != 0x65)) {
				input_info(true, &client->dev,
						"%s: %d data of wacom elec\n",
						__func__, (RETRY_COUNT - retry + 1));
				if (--retry) {
					continue;
				} else {
					input_err(true, &client->dev,
							"%s: invalid elec data %d %d\n",
							__func__, (data[0] & 0x0F), data[1]);
					goto error_test;
				}
			}

			if (data[11] != 3) {
				input_info(true, &client->dev,
						"%s: %d data of wacom elec\n",
						__func__, (RETRY_COUNT - retry + 1));
				if (--retry) {
					continue;
				} else {
					input_info(true, &client->dev,
							"%s: invalid elec status %d\n",
							__func__, data[11]);
					goto error_test;
				}
			}

			for (i = 0; i < COM_ELEC_DATA_NUM; i++) {
				if (!remain_data_count)
					break;

				offset = COM_ELEC_DATA_POS + (i * 2);
				tmp = ((u16)(data[offset]) << 8) + data[offset + 1];
				rx = data[13] + i;

				edata->elec_data[rx * max_ch + tx] = tmp;

				remain_data_count--;
			}
		} while (remain_data_count);

		cmd = COM_ELEC_TRSCON;
		ret = wacom_i2c_send(wac_i2c, &cmd, 1);
		if (ret != 1) {
			input_err(true, &client->dev, "%s: failed to send 0x%02X\n",
					__func__, cmd);
			goto error_test;
		}
		msleep(30);
	}

	cmd = COM_ELEC_YSCAN;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1);
	if (ret != 1) {
		input_err(true, &client->dev,
				"%s: failed to send 0x%02X\n", __func__, cmd);
		goto error_test;
	}
	msleep(30);

	cmd = COM_ELEC_TRSX0;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1);
	if (ret != 1) {
		input_err(true, &client->dev,
				"%s: failed to send 0x%02X\n", __func__, cmd);
		goto error_test;
	}
	msleep(30);

	cmd = COM_ELEC_SCAN_START;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1);
	if (ret != 1) {
		input_err(true, &client->dev,
				"%s: failed to send 0x%02X\n", __func__, cmd);
		goto error_test;
	}
	msleep(30);

	for (tx = 0; tx < max_ch; tx++) {
		/* y scan mode receive data through y coils */
		remain_data_count = edata->max_y_ch;

		do {
			cmd = COM_ELEC_REQUESTDATA;
			ret = wacom_i2c_send(wac_i2c, &cmd, 1);
			if (ret != 1) {
				input_err(true, &client->dev,
						"%s: failed to send 0x%02X\n",
						__func__, cmd);
				goto error_test;
			}
			msleep(30);

			ret = wacom_i2c_recv(wac_i2c, data, COM_ELEC_NUM);
			if (ret != COM_ELEC_NUM) {
				input_err(true, &client->dev,
						"%s: failed to receive elec data\n",
						__func__);
				goto error_test;
			}
#if 0
			input_info(true, &client->dev, "%X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X\n",
					data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9],
					data[10], data[11], data[12], data[13], data[14], data[15], data[16], data[17], data[18], data[19],
					data[20], data[21], data[22], data[23], data[24], data[25], data[26], data[27], data[28], data[29],
					data[30], data[31], data[32], data[33], data[34], data[35], data[36], data[37]);
#endif
			/* check packet ID & sub packet ID */
			if (((data[0] & 0x0F) != 0x0E) && (data[1] != 0x65)) {
				input_info(true, &client->dev,
						"%s: %d data of wacom elec\n",
						__func__, (RETRY_COUNT - retry + 1));
				if (--retry) {
					continue;
				} else {
					input_err(true, &client->dev,
							"%s: invalid elec data %d %d\n",
							__func__, (data[0] & 0x0F), data[1]);
					goto error_test;
				}
			}

			if (data[11] != 3) {
				input_info(true, &client->dev,
						"%s: %d data of wacom elec\n",
						__func__, (RETRY_COUNT - retry + 1));
				if (--retry) {
					continue;
				} else {
					input_info(true, &client->dev,
							"%s: invalid elec status %d\n",
							__func__, data[11]);
					goto error_test;
				}
			}

			for (i = 0; i < COM_ELEC_DATA_NUM; i++) {
				if (!remain_data_count)
					break;

				offset = COM_ELEC_DATA_POS + (i * 2);
				tmp = ((u16)(data[offset]) << 8) + data[offset + 1];
				rx = edata->max_x_ch + data[13] + i;

				edata->elec_data[rx * max_ch + tx] = tmp;

				remain_data_count--;
			}
		} while (remain_data_count);

		cmd = COM_ELEC_TRSCON;
		ret = wacom_i2c_send(wac_i2c, &cmd, 1);
		if (ret != 1) {
			input_err(true, &client->dev, "%s: failed to send 0x%02X\n",
					__func__, cmd);
			goto error_test;
		}
		msleep(30);
	}

	print_spec_data(wac_i2c);

	print_elec_data(wac_i2c);

	/* tx about x coil */
	for (tx = 0, i = 0; tx < edata->max_x_ch; tx++, i++) {
		edata->xx[i] = 0;
		edata->xy[i] = 0;
		/* rx about x coil */
		for (rx = 0; rx < edata->max_x_ch; rx++) {
			offset = rx * max_ch + tx;
			if ((edata->elec_data[offset] > edata->xx[i]) && (tx != rx))
				edata->xx[i] = edata->elec_data[offset];
		}
		/* rx about y coil */
		for (rx = edata->max_x_ch; rx < max_ch; rx++) {
			offset = rx * max_ch + tx;
			if (edata->elec_data[offset] > edata->xy[i])
				edata->xy[i] = edata->elec_data[offset];
		}
	}

	/* tx about y coil */
	for (tx = edata->max_x_ch, i = 0; tx < max_ch; tx++, i++) {
		edata->yx[i] = 0;
		edata->yy[i] = 0;
		/* rx about x coil */
		for (rx = 0; rx < edata->max_x_ch; rx++) {
			offset = rx * max_ch + tx;
			if (edata->elec_data[offset] > edata->yx[i])
				edata->yx[i] = edata->elec_data[offset];
		}
		/* rx about y coil */
		for (rx = edata->max_x_ch; rx < max_ch; rx++) {
			offset = rx * max_ch + tx;
			if ((edata->elec_data[offset] > edata->yy[i]) && (rx != tx))
				edata->yy[i] = edata->elec_data[offset];
		}
	}

	print_trx_data(wac_i2c);

	calibration_trx_data(wac_i2c);

	print_cal_trx_data(wac_i2c);

	calculate_ratio(wac_i2c);

	print_ratio_trx_data(wac_i2c);

	make_decision(wac_i2c, elec_result);

	print_difference_ratio_trx_data(wac_i2c);

	count = elec_result[0] >> 8;
	if (!count) {
		snprintf(tmp_buf, sizeof(tmp_buf), "0_");
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, sizeof(tmp_buf));
	} else {
		if (count > 3)
			snprintf(tmp_buf, sizeof(tmp_buf), "3,");
		else
			snprintf(tmp_buf, sizeof(tmp_buf), "%d,", count);

		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, sizeof(tmp_buf));

		for (i = 0, count = 0; i < max_ch; i++) {
			if ((elec_result[i + 1] & SEC_SHORT) && count < 3) {
				if (i < edata->max_x_ch)
					snprintf(tmp_buf, sizeof(tmp_buf), "X%d,", i);
				else
					snprintf(tmp_buf, sizeof(tmp_buf), "Y%d,", i - edata->max_x_ch);

				strlcat(buff, tmp_buf, buff_size);
				memset(tmp_buf, 0x00, sizeof(tmp_buf));
				count++;
			}
		}
		buff[strlen(buff) - 1] = '_';
	}

	count = elec_result[0] & 0x00FF;
	if (!count) {
		snprintf(tmp_buf, sizeof(tmp_buf), "0_");
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, sizeof(tmp_buf));
	} else {
		if (count > 3)
			snprintf(tmp_buf, sizeof(tmp_buf), "3,");
		else
			snprintf(tmp_buf, sizeof(tmp_buf), "%d,", count);

		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, sizeof(tmp_buf));

		for (i = 0, count = 0; i < max_ch; i++) {
			if ((elec_result[i + 1] & SEC_OPEN) && count < 3) {
				if (i < edata->max_x_ch)
					snprintf(tmp_buf, sizeof(tmp_buf), "X%d,", i);
				else
					snprintf(tmp_buf, sizeof(tmp_buf), "Y%d,", i - edata->max_x_ch);

				strlcat(buff, tmp_buf, buff_size);
				memset(tmp_buf, 0x00, sizeof(tmp_buf));
				count++;
			}
		}
		buff[strlen(buff) - 1] = '_';
	}

	snprintf(tmp_buf, sizeof(tmp_buf), "%d,%d_", edata->max_x_ch, edata->max_y_ch);
	strlcat(buff, tmp_buf, buff_size);
	memset(tmp_buf, 0x00, sizeof(tmp_buf));

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, sizeof(tmp_buf), "%d,",  edata->xx[i]);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, sizeof(tmp_buf));
	}

	for (i = 0; i < edata->max_x_ch; i++) {
		snprintf(tmp_buf, sizeof(tmp_buf), "%d,", edata->xy[i]);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, sizeof(tmp_buf));
	}

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, sizeof(tmp_buf), "%d,", edata->yx[i]);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, sizeof(tmp_buf));
	}

	for (i = 0; i < edata->max_y_ch; i++) {
		snprintf(tmp_buf, sizeof(tmp_buf), "%d,", edata->yy[i]);
		strlcat(buff, tmp_buf, buff_size);
		memset(tmp_buf, 0x00, sizeof(tmp_buf));
	}

	sec_cmd_set_cmd_result(sec, buff, buff_size);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(elec_result);
	kfree(buff);

	ret = wacom_start_stop_cmd(wac_i2c, WACOM_STOP_AND_START_CMD);
	if (ret != 1) {
		input_err(true, &client->dev, "%s: failed to set stop-start cmd, %d\n",
				__func__, ret);
	}

	wacom_enable_irq(wac_i2c, true);

	return;

error_test:
	ret = wacom_start_stop_cmd(wac_i2c, WACOM_STOP_AND_START_CMD);
	if (ret != 1) {
		input_err(true, &client->dev, "%s: failed to set stop-start cmd, %d\n",
				__func__, ret);
	}

	wacom_enable_irq(wac_i2c, true);

error:
	if (elec_result)
		kfree(elec_result);

	if (buff)
		kfree(buff);

	snprintf(tmp_buf, sizeof(tmp_buf), "NG");
	sec_cmd_set_cmd_result(sec, tmp_buf, sizeof(tmp_buf));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return;
}

static void ready_elec_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char data;
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0])
		data = COM_ASYNC_VSYNC;
	else
		data = COM_SYNC_VSYNC;

	ret = wacom_i2c_send(wac_i2c, &data, 1);
	if (ret != 1) {
		input_err(true, &wac_i2c->client->dev,
				"%s: failed to send data(%02x, %d)\n",
				__func__, data, ret);
		snprintf(buff, sizeof(buff), "NG\n");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto out;
	}

	msleep(30);

	snprintf(buff, sizeof(buff), "OK\n");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &wac_i2c->client->dev, "%s: %02x %s\n",
				__func__, data, buff);

	return;
}

static void get_elec_test_ver(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < EPEN_ELEC_DATA_MAIN || sec->cmd_param[0] > EPEN_ELEC_DATA_UNIT) {
		input_err(true, &wac_i2c->client->dev,
				"%s: Abnormal parm[%d]\n", __func__, sec->cmd_param[0]);
		goto err_out;
	}

	if (wac_i2c->pdata->edatas[sec->cmd_param[0]] == NULL) {
		input_err(true, &wac_i2c->client->dev,
				"%s: Elec test spec is null[%d]\n", __func__, sec->cmd_param[0]);
		goto err_out;
	}

	snprintf(buff, sizeof(buff), "%d.%d",
			wac_i2c->pdata->edatas[sec->cmd_param[0]]->spec_ver[0],
			wac_i2c->pdata->edatas[sec->cmd_param[0]]->spec_ver[1]);

	input_info(true, &wac_i2c->client->dev, "%s: pos[%d] ver(%s)\n",
				__func__, sec->cmd_param[0], buff);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;

err_out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	return;

}

/* have to add cmd from Q os for garage test */
static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (wac_i2c->pdata->img_version_of_ic[0] == MPU_W9018)
		snprintf(buff, sizeof(buff), "W9018");
	else if (wac_i2c->pdata->img_version_of_ic[0] == MPU_W9019)
		snprintf(buff, sizeof(buff), "W9019");
	else if (wac_i2c->pdata->img_version_of_ic[0] == MPU_W9020)
		snprintf(buff, sizeof(buff), "W9020");
	else if (wac_i2c->pdata->img_version_of_ic[0] == MPU_W9021)
		snprintf(buff, sizeof(buff), "W9021");
	else if (wac_i2c->pdata->img_version_of_ic[0] == MPU_WEZ01)
		snprintf(buff, sizeof(buff), "WEZ01");
	else
		snprintf(buff, sizeof(buff), "N/A");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

/* Factory cmd for firmware update
 * argument represent what is source of firmware like below.
 *
 * 0 : [BUILT_IN] Getting firmware which is for user.
 * 1 : [UMS] Getting firmware from sd card.
 * 2 : none
 * 3 : [FFU] Getting firmware from apk.
 */

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (!wac_i2c->power_enable) {
		input_err(true, &wac_i2c->client->dev, "%s: [ERROR] Wacom is stopped\n", __func__);
		goto err_out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > WACOM_VERIFICATION) {
		input_err(true, &wac_i2c->client->dev, "%s: [ERROR] parm error![%d]\n",
				__func__, sec->cmd_param[0]);
		goto err_out;
	}

	mutex_lock(&wac_i2c->lock);

	switch (sec->cmd_param[0]) {
	case WACOM_BUILT_IN:
		ret = wacom_fw_update_on_hidden_menu(wac_i2c, FW_BUILT_IN);
		break;
	case WACOM_SDCARD:
#if WACOM_PRODUCT_SHIP
		ret = wacom_fw_update_on_hidden_menu(wac_i2c, FW_IN_SDCARD_SIGNED);
#else
		ret = wacom_fw_update_on_hidden_menu(wac_i2c, FW_IN_SDCARD);
#endif
		break;
	case WACOM_SPU:
		ret = wacom_fw_update_on_hidden_menu(wac_i2c, FW_SPU);
		break;
	case WACOM_VERIFICATION:
		ret = wacom_fw_update_on_hidden_menu(wac_i2c, FW_VERIFICATION);
		break;
	default:
		input_err(true, &wac_i2c->client->dev, "%s: Not support command[%d]\n",
				__func__, sec->cmd_param[0]);
		mutex_unlock(&wac_i2c->lock);
		goto err_out;
	}

	mutex_unlock(&wac_i2c->lock);

	if (ret) {
		input_err(true, &wac_i2c->client->dev, "%s: Wacom fw upate(%d) fail ret [%d]\n",
				__func__, sec->cmd_param[0], ret);
		goto err_out;
	}

	input_info(true, &wac_i2c->client->dev, "%s: Wacom fw upate[%d]\n", __func__, sec->cmd_param[0]);

	snprintf(buff, sizeof(buff), "OK\n");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &wac_i2c->client->dev, "%s: Done\n", __func__);

	return;

err_out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	return;

}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%02X%02X%02X%02X",
			wac_i2c->pdata->img_version_of_bin[0], wac_i2c->pdata->img_version_of_bin[1],
			wac_i2c->pdata->img_version_of_bin[2], wac_i2c->pdata->img_version_of_bin[3]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	int i;
#endif

	sec_cmd_set_default_result(sec);

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	for (i = 0; i < 4; i++)
		wac_i2c->pdata->img_version_of_ic[i] = 0;

	wacom_i2c_query(wac_i2c);
#endif

	snprintf(buff, sizeof(buff), "%02X%02X%02X%02X",
			wac_i2c->pdata->img_version_of_ic[0], wac_i2c->pdata->img_version_of_ic[1],
			wac_i2c->pdata->img_version_of_ic[2], wac_i2c->pdata->img_version_of_ic[3]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void set_ic_reset(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	wacom_enable_irq(wac_i2c, false);

	wac_i2c->function_result &= ~EPEN_EVENT_SURVEY;
	wac_i2c->survey_mode = EPEN_SURVEY_MODE_NONE;

	/* Reset IC */
	wacom_reset_hw(wac_i2c);
	/* I2C Test */
	wacom_i2c_query(wac_i2c);

	wacom_enable_irq(wac_i2c, true);

	input_info(true, &wac_i2c->client->dev, "%s: result %d\n", __func__, wac_i2c->query_status);

	if (wac_i2c->query_status) {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_checksum(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	wacom_enable_irq(wac_i2c, false);

	wacom_checksum(wac_i2c);

	wacom_enable_irq(wac_i2c, true);

	input_info(true, &wac_i2c->client->dev,
			"%s: result %d\n", __func__, wac_i2c->checksum_result);

	if (wac_i2c->checksum_result) {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_digitizer_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct i2c_client *client = wac_i2c->client;
#if WACOM_SEC_FACTORY
	int ret;
#endif

	input_info(true, &client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

#if WACOM_SEC_FACTORY
	ret = sec_input_get_lcd_id(&client->dev);
	if (!ret) {
		input_info(true, &client->dev, "%s: digitizer is not attached\n", __func__);
		goto out;
	}
#endif

	get_connection_test(wac_i2c, WACOM_DIGITIZER_TEST);

#if WACOM_SEC_FACTORY
out:
#endif
	if (wac_i2c->pdata->module_ver == 0x2) {
		snprintf(buff, sizeof(buff), "%s %d %d %d %s %d\n",
				wac_i2c->connection_check ? "OK" : "NG",
				wac_i2c->pdata->module_ver, wac_i2c->fail_channel,
				wac_i2c->min_adc_val, wac_i2c->error_cal ? "NG" : "OK",
				wac_i2c->min_cal_val);
	} else {
		snprintf(buff, sizeof(buff), "%s %d %d %d\n",
				wac_i2c->connection_check ? "OK" : "NG",
				wac_i2c->pdata->module_ver, wac_i2c->fail_channel,
				wac_i2c->min_adc_val);
	}

	if (wac_i2c->connection_check) {
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_garage_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct i2c_client *client = wac_i2c->client;

	input_info(true, &client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (!wac_i2c->pdata->support_garage_open_test) {
		input_err(true, &client->dev, "%s: not support garage open test", __func__);

		snprintf(buff, sizeof(buff), "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	get_connection_test(wac_i2c, WACOM_GARAGE_TEST);
	
	if (wac_i2c->pdata->module_ver == 0x2) {
		snprintf(buff, sizeof(buff), "%s %d %d %d %s %d\n",
				wac_i2c->garage_connection_check ? "OK" : "NG",
				wac_i2c->pdata->module_ver, wac_i2c->garage_fail_channel,
				wac_i2c->garage_min_adc_val, wac_i2c->garage_error_cal ? "NG" : "OK",
				wac_i2c->garage_min_cal_val);
	} else {
		snprintf(buff, sizeof(buff), "%s %d %d %d\n",
				wac_i2c->garage_connection_check ? "OK" : "NG",
				wac_i2c->pdata->module_ver, wac_i2c->garage_fail_channel,
				wac_i2c->garage_min_adc_val);
	}

	if (wac_i2c->garage_connection_check) {
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void set_saving_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	static u32 call_count;

	sec_cmd_set_default_result(sec);

	if(sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &client->dev, "%s: Abnormal parm[%d]\n",
					__func__, sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto fail;
	}

	if (call_count < 0xFFFFFFF0)
		call_count++;

	wac_i2c->battery_saving_mode = sec->cmd_param[0];

	input_info(true, &client->dev, "%s: ps %s & pen %s [%d]\n",
			__func__, wac_i2c->battery_saving_mode ? "on" : "off",
			(wac_i2c->function_result & EPEN_EVENT_PEN_OUT) ? "out" : "in",
			call_count);

	if (!wac_i2c->power_enable && wac_i2c->battery_saving_mode) {
		input_err(true, &client->dev, "%s: already power off, save & return\n", __func__);
		goto out;
	}

	if (!(wac_i2c->function_result & EPEN_EVENT_PEN_OUT)) {
		wacom_select_survey_mode(wac_i2c, wac_i2c->screen_on);

		if (wac_i2c->battery_saving_mode)
			forced_release_fullscan(wac_i2c);
	}

out:
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

fail:
	input_info(true, &client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void get_insert_status(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int pen_state = (wac_i2c->function_result & EPEN_EVENT_PEN_OUT);

	sec_cmd_set_default_result(sec);

	input_info(true, &wac_i2c->client->dev, "%s : pen is %s\n",
				__func__, pen_state ? "OUT" : "IN");
	
	sec->cmd_state = SEC_CMD_STATUS_OK;
	snprintf(buff, sizeof(buff), "%d\n", pen_state ? 0 : 1);

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void set_wcharging_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	wac_i2c->wcharging_mode = sec->cmd_param[0];
	input_info(true, &client->dev, "%s: %d\n", __func__, wac_i2c->wcharging_mode);

	if (!wac_i2c->power_enable) {
		input_err(true, &wac_i2c->client->dev,
				"%s: power off, save & return\n", __func__);
		sec->cmd_state = SEC_CMD_STATUS_OK;
		goto out;
	}

	ret = wacom_i2c_set_sense_mode(wac_i2c);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: do not set sensitivity mode, %d\n",
						__func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void get_wcharging_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__,
			!wac_i2c->wcharging_mode ? "NORMAL" : "LOWSENSE");
	
	sec->cmd_state = SEC_CMD_STATUS_OK;
	snprintf(buff, sizeof(buff), "%s\n", !wac_i2c->wcharging_mode ? "NORMAL" : "LOWSENSE");

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void set_disable_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0])
		wacom_disable_mode(wac_i2c, WACOM_DISABLE);
	else
		wacom_disable_mode(wac_i2c, WACOM_ENABLE);

	snprintf(buff, sizeof(buff), "OK");

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_screen_off_memo(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int val = 0;

	sec_cmd_set_default_result(sec);

#if WACOM_SEC_FACTORY
	input_info(true, &client->dev, "%s : Not support screen off memo mode(%d) in Factory Bin\n",
				__func__, sec->cmd_param[0]);

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	goto out;
#endif

	val = !(!sec->cmd_param[0]);

	if (val)
		wac_i2c->function_set |= EPEN_SETMODE_AOP_OPTION_SCREENOFFMEMO;
	else
		wac_i2c->function_set &= (~EPEN_SETMODE_AOP_OPTION_SCREENOFFMEMO);

	input_info(true, &client->dev,
			"%s: ps %s aop(%d) set(0x%x) ret(0x%x)\n", __func__,
			wac_i2c->battery_saving_mode ? "on" : "off",
			(wac_i2c->function_set & EPEN_SETMODE_AOP) ? 1 : 0,
			wac_i2c->function_set, wac_i2c->function_result);

	if (!wac_i2c->screen_on)
		wacom_select_survey_mode(wac_i2c, false);

	sec->cmd_state = SEC_CMD_STATUS_OK;

#if WACOM_SEC_FACTORY
out:
#endif
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_epen_aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int val = 0;

	sec_cmd_set_default_result(sec);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	
#if WACOM_SEC_FACTORY
	input_info(true, &client->dev, "%s : Not support aod mode(%d) in Factory Bin\n",
					__func__, val);
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	goto out;
#endif

	val = !(!sec->cmd_param[0]);

	if (val)
		wac_i2c->function_set |= EPEN_SETMODE_AOP_OPTION_AOD_LCD_OFF;
	else
		wac_i2c->function_set &= ~EPEN_SETMODE_AOP_OPTION_AOD_LCD_ON;

	input_info(true, &client->dev,
			"%s: ps %s aop(%d) set(0x%x) ret(0x%x)\n", __func__,
			wac_i2c->battery_saving_mode ? "on" : "off",
			(wac_i2c->function_set & EPEN_SETMODE_AOP) ? 1 : 0,
			wac_i2c->function_set, wac_i2c->function_result);

	if (!wac_i2c->screen_on)
		wacom_select_survey_mode(wac_i2c, false);

#if WACOM_SEC_FACTORY
out:
#endif
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_aod_lcd_onoff_status(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int val = 0;
	int ret;

	sec_cmd_set_default_result(sec);

#if WACOM_SEC_FACTORY
	input_info(true, &client->dev, "%s : Not support aod mode(%d) in Factory Bin\n",
					__func__, val);

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	goto out;
#endif

	val = !(!sec->cmd_param[0]);

	if (val)
		wac_i2c->function_set |= EPEN_SETMODE_AOP_OPTION_AOD_LCD_STATUS;
	else
		wac_i2c->function_set &= ~EPEN_SETMODE_AOP_OPTION_AOD_LCD_STATUS;

	if (!(wac_i2c->function_set & EPEN_SETMODE_AOP_OPTION_AOD) || wac_i2c->screen_on) {
		sec->cmd_state = SEC_CMD_STATUS_OK;	
		goto out;
	}

	if (wac_i2c->survey_mode == EPEN_SURVEY_MODE_GARAGE_AOP) {
		if ((wac_i2c->function_set & EPEN_SETMODE_AOP_OPTION_AOD_LCD_ON)
				== EPEN_SETMODE_AOP_OPTION_AOD_LCD_OFF) {
			forced_release_fullscan(wac_i2c);
		}

		mutex_lock(&wac_i2c->mode_lock);
		wacom_i2c_set_survey_mode(wac_i2c, wac_i2c->survey_mode);
		mutex_unlock(&wac_i2c->mode_lock);
	}

	ret = wacom_i2c_set_sense_mode(wac_i2c);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: do not set sensitivity mode, %d\n",
						__func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_epen_aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int val = 0;

	sec_cmd_set_default_result(sec);
	sec->cmd_state = SEC_CMD_STATUS_OK;

#if WACOM_SEC_FACTORY
	input_info(true, &client->dev, "%s : Not support aod mode(%d) in Factory Bin\n",
					__func__, val);
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	goto out;
#endif

	val = !(!sec->cmd_param[0]);

	if (val)
		wac_i2c->function_set |= EPEN_SETMODE_AOP_OPTION_AOT;
	else
		wac_i2c->function_set &= (~EPEN_SETMODE_AOP_OPTION_AOT);

	input_info(true, &client->dev,
			"%s: ps %s aop(%d) set(0x%x) ret(0x%x)\n", __func__,
			wac_i2c->battery_saving_mode ? "on" : "off",
			(wac_i2c->function_set & EPEN_SETMODE_AOP) ? 1 : 0,
			wac_i2c->function_set, wac_i2c->function_result);

	if (!wac_i2c->screen_on)
		wacom_select_survey_mode(wac_i2c, false);

#if WACOM_SEC_FACTORY
out:
#endif
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

#if WACOM_SEC_FACTORY
/* k|K|0 : FW_BUILT_IN, t|T|1 : FW_FACTORY_GARAGE u|U|2 : FW_FACTORY_UNIT */
static void set_fac_select_firmware(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 fw_update_way = FW_NONE;
	int ret;

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case 0:
		fw_update_way = FW_BUILT_IN;
		break;
	case 1:
		fw_update_way = FW_FACTORY_GARAGE;
		break;
	case 2:
		fw_update_way = FW_FACTORY_UNIT;
		break;
	default:
		input_err(true, &client->dev, "wrong parameter\n");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	ret = wacom_i2c_load_fw(wac_i2c, fw_update_way);
	if (ret < 0) {
		input_info(true, &client->dev, "failed to load fw data\n");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	wacom_i2c_unload_fw(wac_i2c);

	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_fac_garage_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1 ) {
		input_err(true, &client->dev, "%s: abnormal param(%d)\n",
					__func__, sec->cmd_param[0]);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (sec->cmd_param[0]) {
		/* change normal mode for garage monitoring */
		mutex_lock(&wac_i2c->mode_lock);
		wacom_i2c_set_survey_mode(wac_i2c, EPEN_SURVEY_MODE_NONE);
		mutex_unlock(&wac_i2c->mode_lock);

		ret = wacom_start_stop_cmd(wac_i2c, WACOM_STOP_CMD);
		if (ret != 1) {
			input_err(true, &client->dev, "%s: failed to set stop cmd, %d\n",
					__func__, ret);
			wac_i2c->fac_garage_mode = 0;
			goto fail_out;
		}

		wac_i2c->fac_garage_mode = 1;

	} else {
		wac_i2c->fac_garage_mode = 0;

		ret = wacom_start_stop_cmd(wac_i2c, WACOM_STOP_AND_START_CMD);
		if (ret != 1) {
			input_err(true, &client->dev, "%s: failed to set stop cmd, %d\n",
					__func__, ret);
			goto fail_out;
		}

		/* recovery wacom mode */
		wacom_select_survey_mode(wac_i2c, wac_i2c->screen_on);
	}

	input_info(true, &client->dev, "%s: done(%d)\n", __func__, sec->cmd_param[0]);

out:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	return;

fail_out:
	/* recovery wacom mode */
	wacom_select_survey_mode(wac_i2c, wac_i2c->screen_on);

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "NG");
	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

}

static void get_fac_garage_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	snprintf(buff, sizeof(buff), "garage mode %s",
			wac_i2c->fac_garage_mode ? "IN" : "OUT");

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void get_fac_garage_rawdata(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	struct i2c_client *client = wac_i2c->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char data[17] = { 0, };
	int ret, retry = 1;
	u8 cmd;

	sec_cmd_set_default_result(sec);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (!wac_i2c->fac_garage_mode) {
		input_err(true, &client->dev, "not in factory garage mode\n");
		goto out;
	}

get_garage_retry:
	wacom_enable_irq(wac_i2c, false);

	cmd = COM_REQUEST_GARAGEDATA;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1);
	if (ret < 0) {
		input_err(true, &client->dev, "failed to send request garage data command %d\n", ret);
		msleep(30);

		goto fail_out;
	}

	msleep(30);

	ret = wacom_i2c_recv(wac_i2c, data, sizeof(data));
	if (ret < 0) {
		input_err(true, &client->dev, "failed to read garage raw data, %d\n", ret);

		wac_i2c->garage_freq0 = wac_i2c->garage_freq1 = 0;
		wac_i2c->garage_gain0 = wac_i2c->garage_gain1 = 0;

		goto fail_out;
	}

	wacom_enable_irq(wac_i2c, true);

	input_info(true, &client->dev, "%x %x %x %x %x %x %x %x %x %x\n",
			data[2], data[3], data[4], data[5], data[6], data[7],
			data[8], data[9], data[10], data[11]);

	wac_i2c->garage_gain0 = data[6];
	wac_i2c->garage_freq0 = ((u16)data[7] << 8) + data[8];

	wac_i2c->garage_gain1 = data[9];
	wac_i2c->garage_freq1 = ((u16)data[10] << 8) + data[11];

	if (wac_i2c->garage_freq0 == 0 && retry > 0) {
		retry--;
		goto get_garage_retry;
	}

	snprintf(buff, sizeof(buff), "%d,%d,%d,%d",
			wac_i2c->garage_gain0, wac_i2c->garage_freq0,
			wac_i2c->garage_gain1, wac_i2c->garage_freq1);

	input_info(true, &client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	return;

fail_out:
	wacom_enable_irq(wac_i2c, true);
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "NG");

	input_info(true, &wac_i2c->client->dev, "%s: %s\n", __func__, buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}
#endif

static void debug(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	wac_i2c->debug_flag = sec->cmd_param[0];

	snprintf(buff, sizeof(buff), "OK");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);
}

static void set_cover_type(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct wacom_i2c *wac_i2c = container_of(sec, struct wacom_i2c, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (wac_i2c->pdata->support_pogo_cover)
		goto return_ok;

	if (wac_i2c->pdata->support_cover_noti) {
		if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 2) {
			input_err(true, &wac_i2c->client->dev, "%s: Not support command[%d]\n",
					__func__, sec->cmd_param[0]);
			goto err_out;
		} else 
			wac_i2c->cover = sec->cmd_param[0];
	}else {
		input_err(true, &wac_i2c->client->dev, "%s: Not support notice cover command\n",
					__func__);
		goto err_out;
	}

	if (!wac_i2c->power_enable) {
		input_err(true, &wac_i2c->client->dev, "%s: [ERROR] Wacom is stopped\n", __func__);
		goto err_out;
	}

	input_info(true, &wac_i2c->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	wacom_swap_compensation(wac_i2c, wac_i2c->cover);
return_ok:
	snprintf(buff, sizeof(buff), "OK\n");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &wac_i2c->client->dev, "%s: Done\n", __func__);

	return;

err_out:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &wac_i2c->client->dev, "%s: Fail\n", __func__);

	return;
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);
}

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD_H("run_elec_test", run_elec_test),},
	{SEC_CMD_H("ready_elec_test", ready_elec_test),},
	{SEC_CMD_H("get_elec_test_ver", get_elec_test_ver),},
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("set_ic_reset", set_ic_reset),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_checksum", get_checksum),},
	{SEC_CMD("get_digitizer_connection", get_digitizer_connection),},
	{SEC_CMD("get_garage_connection", get_garage_connection),},
	{SEC_CMD_H("set_saving_mode", set_saving_mode),},
	{SEC_CMD("get_insert_status", get_insert_status),},
	{SEC_CMD_H("set_wcharging_mode", set_wcharging_mode),},
	{SEC_CMD("get_wcharging_mode", get_wcharging_mode),},
	{SEC_CMD_H("set_disable_mode", set_disable_mode),},
	{SEC_CMD("set_screen_off_memo", set_screen_off_memo),},
	{SEC_CMD("set_epen_aod_enable", set_epen_aod_enable),},
	{SEC_CMD("set_aod_lcd_onoff_status", set_aod_lcd_onoff_status),},
	{SEC_CMD("set_epen_aot_enable", set_epen_aot_enable),},
#if WACOM_SEC_FACTORY
	{SEC_CMD_H("set_fac_select_firmware", set_fac_select_firmware),},
	{SEC_CMD("set_fac_garage_mode", set_fac_garage_mode),},
	{SEC_CMD("get_fac_garage_mode", get_fac_garage_mode),},
	{SEC_CMD("get_fac_garage_rawdata", get_fac_garage_rawdata),},
#endif
	{SEC_CMD("debug", debug),},
	{SEC_CMD("set_cover_type", set_cover_type),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static int wacom_sec_elec_init(struct wacom_i2c *wac_i2c, int pos)
{
	struct wacom_elec_data *edata = NULL;
	struct i2c_client *client = wac_i2c->client;
	struct device_node *np = wac_i2c->client->dev.of_node;
	u32 tmp[2];
	int max_len;
	int ret;
	int i;
	u8 tmp_name[30] = { 0 };

	snprintf(tmp_name, sizeof(tmp_name), "wacom_elec%d", pos);
	input_info(true, &client->dev, "%s: init : %s\n", __func__, tmp_name);

	np = of_get_child_by_name(np, tmp_name);
	if (!np) {
		input_err(true, &client->dev, "%s: node is not exist for elec\n", __func__);
		return -EINVAL;
	}

	edata = devm_kzalloc(&client->dev, sizeof(*edata), GFP_KERNEL);
	if (!edata)
		return -ENOMEM;

	wac_i2c->pdata->edatas[pos] = edata;

	ret = of_property_read_u32_array(np, "spec_ver", tmp, 2);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read sepc ver %d\n", ret);
		return -EINVAL;
	}
	edata->spec_ver[0] = tmp[0];
	edata->spec_ver[1] = tmp[1];

	ret = of_property_read_u32_array(np, "max_channel", tmp, 2);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read max channel %d\n", ret);
		return -EINVAL;
	}
	edata->max_x_ch = tmp[0];
	edata->max_y_ch = tmp[1];

	ret = of_property_read_u32(np, "shift_value", tmp);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read max channel %d\n", ret);
		return -EINVAL;
	}
	edata->shift_value = tmp[0];

	input_info(true, &client->dev, "channel(%d %d), spec_ver %d.%d shift_value %d\n",
			edata->max_x_ch, edata->max_y_ch, edata->spec_ver[0], edata->spec_ver[1], edata->shift_value);

	max_len = edata->max_x_ch + edata->max_y_ch;

	edata->elec_data = devm_kzalloc(&client->dev,
			max_len * max_len * sizeof(u16),
			GFP_KERNEL);
	if (!edata->elec_data)
		return -ENOMEM;

	edata->xx = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(u16), GFP_KERNEL);
	edata->xy = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(u16), GFP_KERNEL);
	edata->yx = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(u16), GFP_KERNEL);
	edata->yy = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(u16), GFP_KERNEL);
	if (!edata->xx || !edata->xy || !edata->yx || !edata->yy)
		return -ENOMEM;

	edata->xx_xx = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->xy_xy = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->yx_yx = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	edata->yy_yy = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	if (!edata->xx_xx || !edata->xy_xy || !edata->yx_yx || !edata->yy_yy)
		return -ENOMEM;

	edata->rxx = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->rxy = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->ryx = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	edata->ryy = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	if (!edata->rxx || !edata->rxy || !edata->ryx || !edata->ryy)
		return -ENOMEM;

	edata->drxx = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->drxy = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->dryx = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	edata->dryy = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	if (!edata->drxx || !edata->drxy || !edata->dryx || !edata->dryy)
		return -ENOMEM;

	edata->xx_ref = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->xy_ref = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->yx_ref = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	edata->yy_ref = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	if (!edata->xx_ref || !edata->xy_ref || !edata->yx_ref || !edata->yy_ref)
		return -ENOMEM;

	ret = of_property_read_u64_array(np, "xx_ref", edata->xx_ref, edata->max_x_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read xx reference data %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u64_array(np, "xy_ref", edata->xy_ref, edata->max_x_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read xy reference data %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u64_array(np, "yx_ref", edata->yx_ref, edata->max_y_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read yx reference data %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u64_array(np, "yy_ref", edata->yy_ref, edata->max_y_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read yy reference data %d\n", ret);
		return -EINVAL;
	}

	edata->xx_spec = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->xy_spec = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->yx_spec = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	edata->yy_spec = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	if (!edata->xx_spec || !edata->xy_spec || !edata->yx_spec || !edata->yy_spec)
		return -ENOMEM;

	ret = of_property_read_u64_array(np, "xx_spec", edata->xx_spec, edata->max_x_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read xx spec data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_x_ch; i++)
		edata->xx_spec[i] = edata->xx_spec[i] * POWER_OFFSET;

	ret = of_property_read_u64_array(np, "xy_spec", edata->xy_spec, edata->max_x_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read xy spec data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_x_ch; i++)
		edata->xy_spec[i] = edata->xy_spec[i] * POWER_OFFSET;

	ret = of_property_read_u64_array(np, "yx_spec", edata->yx_spec, edata->max_y_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read yx spec data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_y_ch; i++)
		edata->yx_spec[i] = edata->yx_spec[i] * POWER_OFFSET;

	ret = of_property_read_u64_array(np, "yy_spec", edata->yy_spec, edata->max_y_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read yy spec data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_y_ch; i++)
		edata->yy_spec[i] = edata->yy_spec[i] * POWER_OFFSET;

	edata->rxx_ref = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->rxy_ref = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->ryx_ref = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	edata->ryy_ref = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	if (!edata->rxx_ref || !edata->rxy_ref || !edata->ryx_ref || !edata->ryy_ref)
		return -ENOMEM;

	ret = of_property_read_u64_array(np, "rxx_ref", edata->rxx_ref, edata->max_x_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read xx ratio reference data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_x_ch; i++)
		edata->rxx_ref[i] = edata->rxx_ref[i] * POWER_OFFSET / power(edata->shift_value);

	ret = of_property_read_u64_array(np, "rxy_ref", edata->rxy_ref, edata->max_x_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read xy ratio reference data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_x_ch; i++)
		edata->rxy_ref[i] = edata->rxy_ref[i] * POWER_OFFSET / power(edata->shift_value);

	ret = of_property_read_u64_array(np, "ryx_ref", edata->ryx_ref, edata->max_y_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read yx ratio reference data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_y_ch; i++)
		edata->ryx_ref[i] = edata->ryx_ref[i] * POWER_OFFSET / power(edata->shift_value);

	ret = of_property_read_u64_array(np, "ryy_ref", edata->ryy_ref, edata->max_y_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read yy ratio reference data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_y_ch; i++)
		edata->ryy_ref[i] = edata->ryy_ref[i] * POWER_OFFSET / power(edata->shift_value);

	edata->drxx_spec = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->drxy_spec = devm_kzalloc(&client->dev, edata->max_x_ch * sizeof(long long), GFP_KERNEL);
	edata->dryx_spec = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	edata->dryy_spec = devm_kzalloc(&client->dev, edata->max_y_ch * sizeof(long long), GFP_KERNEL);
	if (!edata->drxx_spec || !edata->drxy_spec || !edata->dryx_spec || !edata->dryy_spec)
		return -ENOMEM;

	ret = of_property_read_u64_array(np, "drxx_spec", edata->drxx_spec, edata->max_x_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read xx difference ratio spec data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_x_ch; i++)
		edata->drxx_spec[i] = edata->drxx_spec[i] * POWER_OFFSET / power(edata->shift_value);

	ret = of_property_read_u64_array(np, "drxy_spec", edata->drxy_spec, edata->max_x_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read xy difference ratio spec data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_x_ch; i++)
		edata->drxy_spec[i] = edata->drxy_spec[i] * POWER_OFFSET / power(edata->shift_value);

	ret = of_property_read_u64_array(np, "dryx_spec", edata->dryx_spec, edata->max_y_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read yx difference ratio spec data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_y_ch; i++)
		edata->dryx_spec[i] = edata->dryx_spec[i] * POWER_OFFSET / power(edata->shift_value);

	ret = of_property_read_u64_array(np, "dryy_spec", edata->dryy_spec, edata->max_y_ch);
	if (ret) {
		input_err(true, &client->dev,
				"failed to read yy difference ratio spec data %d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < edata->max_y_ch; i++)
		edata->dryy_spec[i] = edata->dryy_spec[i] * POWER_OFFSET / power(edata->shift_value);

	return 1;
}

int wacom_sec_init(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
	int retval = 0;
	int i = 0;

	retval = sec_cmd_init(&wac_i2c->sec, NULL, sec_cmds, ARRAY_SIZE(sec_cmds),
			SEC_CLASS_DEVT_WACOM, &epen_attr_group);
	if (retval < 0) {
		input_err(true, &client->dev, "failed to sec_cmd_init\n");
		return retval;
	}

	retval = sysfs_create_link(&wac_i2c->sec.fac_dev->kobj,
			&wac_i2c->input_dev->dev.kobj, "input");
	if (retval) {
		input_err(true, &client->dev, "failed to create sysfs link\n");
		goto err_create_symlink;
	}

	for (i = EPEN_ELEC_DATA_MAIN ; i <= EPEN_ELEC_DATA_UNIT ; i++) {
		wacom_sec_elec_init(wac_i2c, i);
	}

	return 0;

err_create_symlink:
	sec_cmd_exit(&wac_i2c->sec, SEC_CLASS_DEVT_WACOM);

	return retval;
}

void wacom_sec_remove(struct wacom_i2c *wac_i2c)
{
	sysfs_remove_link(&wac_i2c->sec.fac_dev->kobj, "input");
	sysfs_remove_group(&wac_i2c->sec.fac_dev->kobj, &epen_attr_group);
	sec_cmd_exit(&wac_i2c->sec, SEC_CLASS_DEVT_WACOM);
}
