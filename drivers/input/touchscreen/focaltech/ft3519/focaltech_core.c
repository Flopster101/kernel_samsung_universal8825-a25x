/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*****************************************************************************
*
* File Name: focaltech_core.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: entrance for focaltech ts driver
*
* Version: V1.0
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include "focaltech_core.h"
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "focaltech_ts"
#define FTS_DRIVER_PEN_NAME                 "focaltech_ts,pen"
#define INTERVAL_READ_REG                   200  /* unit:ms */
#define TIMEOUT_READ_REG                    1000 /* unit:ms */
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      2800000
#define FTS_VTG_MAX_UV                      3300000
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_data;
void fts_irq_enable(void);
void fts_irq_disable(void);
void fts_release_all_finger(void);

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_i2c_lock(struct i2c_adapter *adap);
extern int stui_i2c_unlock(struct i2c_adapter *adap);

static int fts_stui_tsp_enter(void)
{
	int ret = 0;
	struct fts_ts_data *ts = fts_data;

	FTS_INFO("[STUI] %s\n", __func__);
	if (!ts)
		return -EINVAL;

	fts_irq_disable();
	fts_release_all_finger();

	ret = stui_i2c_lock(ts->client->adapter);
	if (ret) {
		pr_err("[STUI] stui_i2c_lock failed : %d\n", ret);
		fts_irq_enable();
		return -1;
	}

	return 0;
}

static int fts_stui_tsp_exit(void)
{
	int ret = 0;
	struct fts_ts_data *ts = fts_data;

	if (!ts)
		return -EINVAL;

	ret = stui_i2c_unlock(ts->client->adapter);
	if (ret)
		pr_err("[STUI] stui_i2c_unlock failed : %d\n", ret);

	fts_irq_enable();

	return ret;
}

static int fts_stui_tsp_type(void)
{
	return STUI_TSP_TYPE_FOCALTECH;
}
#endif

int fts_check_cid(struct fts_ts_data *ts_data, u8 id_h)
{
	int i = 0;
	struct ft_chip_id_t *cid = &ts_data->ic_info.cid;
	u8 cid_h = 0x0;

	if (cid->type == 0)
		return -ENODATA;

	for (i = 0; i < FTS_MAX_CHIP_IDS; i++) {
		cid_h = ((cid->chip_ids[i] >> 8) & 0x00FF);
		if (cid_h && (id_h == cid_h)) {
			return 0;
		}
	}

	return -ENODATA;
}

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(void)
{
	int ret = 0;
	int cnt = 0;
	u8 idh = 0;
	struct fts_ts_data *ts_data = fts_data;
	u8 chip_idh = ts_data->ic_info.ids.chip_idh;

	FTS_FUNC_ENTER();
	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &idh);
		if ((idh == chip_idh) || (fts_check_cid(ts_data, idh) == 0)) {
			FTS_INFO("TP Ready,Device ID:0x%02x", idh);
			return 0;
		} else
			FTS_DEBUG("TP Not Ready,ReadData:0x%02x,ret:%d", idh, ret);

		cnt++;
		sec_delay(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);
	FTS_FUNC_EXIT();

	return -EIO;
}

/*****************************************************************************
*  Name: fts_tp_state_recovery
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_tp_state_recovery(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	/* wait tp stable */
	fts_wait_tp_to_valid();
	/* recover TP charger state 0x8B */
	/* recover TP glove state 0xC0 */
	/* recover TP cover state 0xC1 */
	fts_ex_mode_recovery(ts_data);
	/* recover TP gesture state 0xD0 */
	fts_gesture_recovery(ts_data);
	FTS_FUNC_EXIT();
}

int fts_reset_proc(int hdelayms)
{
	int ret = 0;
	FTS_DEBUG("tp reset");
	ret = fts_write_reg(0xFC, 0xAA);
	if (ret < 0) {
		FTS_ERROR("write FC AA fails");
	}

	ret = fts_write_reg(0xFC, 0x66);
	if (ret < 0) {
		FTS_ERROR("write FC 66 fails");
	}
	if (hdelayms) {
		sec_delay(hdelayms);
	}

	return 0;
}

void fts_irq_disable(void)
{
	mutex_lock(&fts_data->irq_lock);
	if (!fts_data->irq_disabled) {
		FTS_FUNC_ENTER();
		disable_irq_nosync(fts_data->irq);
		fts_data->irq_disabled = true;
	}
	mutex_unlock(&fts_data->irq_lock);
}

void fts_irq_enable(void)
{
	mutex_lock(&fts_data->irq_lock);
	if (fts_data->irq_disabled) {
		FTS_FUNC_ENTER();
		enable_irq(fts_data->irq);
		fts_data->irq_disabled = false;
	}
	mutex_unlock(&fts_data->irq_lock);
}

void fts_hid2std(void)
{
	int ret = 0;
	u8 buf[3] = {0xEB, 0xAA, 0x09};

	if (fts_data->bus_type != BUS_TYPE_I2C)
		return;

	ret = fts_write(buf, 3);
	if (ret < 0) {
		FTS_ERROR("hid2std cmd write fail");
	} else {
		sec_delay(10);
		buf[0] = buf[1] = buf[2] = 0;
		ret = fts_read(NULL, 0, buf, 3);
		if (ret < 0) {
			FTS_ERROR("hid2std cmd read fail");
		} else if ((0xEB == buf[0]) && (0xAA == buf[1]) && (0x08 == buf[2])) {
			FTS_DEBUG("hidi2c change to stdi2c successful");
		} else {
			FTS_DEBUG("hidi2c change to stdi2c not support or fail");
		}
	}
}

static int fts_match_cid(struct fts_ts_data *ts_data,
                         u16 type, u8 id_h, u8 id_l, bool force)
{
#ifdef FTS_CHIP_ID_MAPPING
	u32 i = 0;
	u32 j = 0;
	struct ft_chip_id_t chip_id_list[] = FTS_CHIP_ID_MAPPING;
	u32 cid_entries = sizeof(chip_id_list) / sizeof(struct ft_chip_id_t);
	u16 id = (id_h << 8) + id_l;

	memset(&ts_data->ic_info.cid, 0, sizeof(struct ft_chip_id_t));
	for (i = 0; i < cid_entries; i++) {
		if (!force && (type == chip_id_list[i].type)) {
			break;
		} else if (force && (type == chip_id_list[i].type)) {
			FTS_INFO("match cid,type:0x%x", (int)chip_id_list[i].type);
			ts_data->ic_info.cid = chip_id_list[i];
			return 0;
		}
	}

	if (i >= cid_entries) {
		return -ENODATA;
	}

	for (j = 0; j < FTS_MAX_CHIP_IDS; j++) {
		if (id == chip_id_list[i].chip_ids[j]) {
			FTS_DEBUG("cid:%x==%x", id, chip_id_list[i].chip_ids[j]);
			FTS_INFO("match cid,type:0x%x", (int)chip_id_list[i].type);
			ts_data->ic_info.cid = chip_id_list[i];
			return 0;
		}
	}

	return -ENODATA;
#else
	return -EINVAL;
#endif
}


static int fts_get_chip_types(
    struct fts_ts_data *ts_data,
    u8 id_h, u8 id_l, bool fw_valid)
{
	u32 i = 0;
	struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
	u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

	if ((0x0 == id_h) || (0x0 == id_l)) {
		FTS_ERROR("id_h/id_l is 0");
		return -EINVAL;
	}

	FTS_DEBUG("verify id:0x%02x%02x", id_h, id_l);
	for (i = 0; i < ctype_entries; i++) {
		if (VALID == fw_valid) {
			if (((id_h == ctype[i].chip_idh) && (id_l == ctype[i].chip_idl))
			    || (!fts_match_cid(ts_data, ctype[i].type, id_h, id_l, 0)))
				break;
		} else {
			if (((id_h == ctype[i].rom_idh) && (id_l == ctype[i].rom_idl))
			    || ((id_h == ctype[i].pb_idh) && (id_l == ctype[i].pb_idl))
			    || ((id_h == ctype[i].bl_idh) && (id_l == ctype[i].bl_idl))) {
				break;
			}
		}
	}

	if (i >= ctype_entries) {
		return -ENODATA;
	}

	fts_match_cid(ts_data, ctype[i].type, id_h, id_l, 1);
	ts_data->ic_info.ids = ctype[i];
	return 0;
}

static int fts_read_bootid(struct fts_ts_data *ts_data, u8 *id)
{
	int ret = 0;
	u8 chip_id[2] = { 0 };
	u8 id_cmd[4] = { 0 };
	u32 id_cmd_len = 0;

	id_cmd[0] = FTS_CMD_START1;
	id_cmd[1] = FTS_CMD_START2;
	ret = fts_write(id_cmd, 2);
	if (ret < 0) {
		FTS_ERROR("start cmd write fail");
		return ret;
	}

	sec_delay(FTS_CMD_START_DELAY);
	id_cmd[0] = FTS_CMD_READ_ID;
	id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
	if (ts_data->ic_info.is_incell)
		id_cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
	else
		id_cmd_len = FTS_CMD_READ_ID_LEN;
	ret = fts_read(id_cmd, id_cmd_len, chip_id, 2);
	if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
		FTS_ERROR("read boot id fail,read:0x%02x%02x", chip_id[0], chip_id[1]);
		return -EIO;
	}

	id[0] = chip_id[0];
	id[1] = chip_id[1];
	return 0;
}

/*****************************************************************************
* Name: fts_get_ic_information
* Brief: read chip id to get ic information, after run the function, driver w-
*        ill know which IC is it.
*        If cant get the ic information, maybe not focaltech's touch IC, need
*        unregister the driver
* Input:
* Output:
* Return: return 0 if get correct ic information, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int cnt = 0;
	u8 chip_id[2] = { 0 };

	ts_data->ic_info.is_incell = FTS_CHIP_IDC;
	ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;

	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id[0]);
		ret = fts_read_reg(FTS_REG_CHIP_ID2, &chip_id[1]);
		if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
			FTS_DEBUG("chip id read invalid, read:0x%02x%02x",
			          chip_id[0], chip_id[1]);
		} else {
			ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], VALID);
			if (!ret)
				break;
			else
				FTS_DEBUG("TP not ready, read:0x%02x%02x",
				          chip_id[0], chip_id[1]);
		}

		cnt++;
		sec_delay(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
		FTS_INFO("fw is invalid, need read boot id");
		if (ts_data->ic_info.hid_supported) {
			fts_hid2std();
		}


		ret = fts_read_bootid(ts_data, &chip_id[0]);
		if (ret <  0) {
			FTS_ERROR("read boot id fail");
			return ret;
		}

		ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
		if (ret < 0) {
			FTS_ERROR("can't get ic informaton");
			return ret;
		}
	}

	FTS_INFO("get ic information, chip id = 0x%02x%02x(cid type=0x%x)",
	         ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl,
	         ts_data->ic_info.cid.type);

	return 0;
}

/*****************************************************************************
*  Reprot related
*****************************************************************************/
static void fts_show_touch_buffer(u8 *data, int datalen)
{
	int i = 0;
	int count = 0;
	char *tmpbuf = NULL;

	tmpbuf = kzalloc(1024, GFP_KERNEL);
	if (!tmpbuf) {
		FTS_ERROR("tmpbuf zalloc fail");
		return;
	}

	for (i = 0; i < datalen; i++) {
		count += snprintf(tmpbuf + count, 1024 - count, "%02X,", data[i]);
		if (count >= 1024)
			break;
	}
	FTS_DEBUG("point buffer:%s", tmpbuf);

	if (tmpbuf) {
		kfree(tmpbuf);
		tmpbuf = NULL;
	}
}

void fts_release_all_finger(void)
{
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev = ts_data->input_dev;
#if FTS_MT_PROTOCOL_B_EN
	u32 finger_count = 0;
	u32 max_touches = ts_data->max_touch_number;
#endif

	mutex_lock(&ts_data->report_mutex);

	if (ts_data->pdata->prox_power_off == 1) {
		input_report_key(input_dev, KEY_INT_CANCEL, 1);
		input_sync(input_dev);
		input_report_key(input_dev, KEY_INT_CANCEL, 0);
		input_sync(input_dev);
	}

	if (ts_data->pdata->input_dev_proximity) {
		ts_data->pocket_state = ts_data->hover_event = 0xff;
		input_report_abs(ts_data->pdata->input_dev_proximity, ABS_MT_CUSTOM, ts_data->hover_event);
		input_sync(ts_data->pdata->input_dev_proximity);
	}

#if FTS_MT_PROTOCOL_B_EN
	for (finger_count = 0; finger_count < max_touches; finger_count++) {
		input_mt_slot(input_dev, finger_count);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);

		if (ts_data->touchs & BIT(finger_count))
			FTS_INFO("[RA] tID:%d", finger_count);
	}
#else
	input_mt_sync(input_dev);
#endif
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_report_key(input_dev, BTN_PALM, 0);
	input_sync(input_dev);

#if FTS_PEN_EN
	input_report_key(ts_data->pen_dev, BTN_TOOL_PEN, 0);
	input_report_key(ts_data->pen_dev, BTN_TOUCH, 0);
	input_sync(ts_data->pen_dev);
#endif

	ts_data->touchs = 0;
	ts_data->key_state = 0;
	ts_data->palm_flag = 0;
	mutex_unlock(&ts_data->report_mutex);
}

/*****************************************************************************
* Name: fts_input_report_key
* Brief: process key events,need report key-event if key enable.
*        if point's coordinate is in (x_dim-50,y_dim-50) ~ (x_dim+50,y_dim+50),
*        need report it to key event.
*        x_dim: parse from dts, means key x_coordinate, dimension:+-50
*        y_dim: parse from dts, means key y_coordinate, dimension:+-50
* Input:
* Output:
* Return: return 0 if it's key event, otherwise return error code
*****************************************************************************/
static int fts_input_report_key(struct fts_ts_data *data, int index)
{
	int i = 0;
	int x = data->events[index].x;
	int y = data->events[index].y;
	int *x_dim = &data->key_x_coords[0];
	int *y_dim = &data->key_y_coords[0];

	if (!data->have_key) {
		return -EINVAL;
	}
	for (i = 0; i < data->key_number; i++) {
		if ((x >= x_dim[i] - FTS_KEY_DIM) && (x <= x_dim[i] + FTS_KEY_DIM) &&
		    (y >= y_dim[i] - FTS_KEY_DIM) && (y <= y_dim[i] + FTS_KEY_DIM)) {
			if (EVENT_DOWN(data->events[index].flag)
			    && !(data->key_state & (1 << i))) {
				input_report_key(data->input_dev, data->keys[i], 1);
				data->key_state |= (1 << i);
				FTS_DEBUG("Key%d(%d,%d) DOWN!", i, x, y);
			} else if (EVENT_UP(data->events[index].flag)
			           && (data->key_state & (1 << i))) {
				input_report_key(data->input_dev, data->keys[i], 0);
				data->key_state &= ~(1 << i);
				FTS_DEBUG("Key%d(%d,%d) Up!", i, x, y);
			}
			return 0;
		}
	}
	return -EINVAL;
}

static void location_detect(struct fts_ts_data *ts, char *loc, int x, int y)
{
	memset(loc, 0, SEC_TS_LOCATION_DETECT_SIZE);

	if (ts->pdata) {
		if (x < ts->pdata->area_edge)
			strncat(loc, "E.", 2);
		else if (x < (ts->pdata->max_x - ts->pdata->area_edge))
			strncat(loc, "C.", 2);
		else
			strncat(loc, "e.", 2);

		if (y < ts->pdata->area_indicator)
			strncat(loc, "S", 1);
		else if (y < (ts->pdata->max_y - ts->pdata->area_navigation))
			strncat(loc, "C", 1);
		else
			strncat(loc, "N", 1);
	}
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *data)
{
	int i = 0;
	unsigned long touchs = 0;
	bool va_reported = false;
	u32 max_touch_num = data->max_touch_number;
	struct ts_event *events = data->events;
	char loc[SEC_TS_LOCATION_DETECT_SIZE] = { 0 };

	for (i = 0; i < data->touch_point; i++) {
		if (fts_input_report_key(data, i) == 0) {
			continue;
		}

		va_reported = true;
		input_mt_slot(data->input_dev, events[i].id);

		if (EVENT_DOWN(events[i].flag)) {
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);

#if FTS_REPORT_PRESSURE_EN
			if (events[i].p <= 0) {
				events[i].p = 0x3f;
			}
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
			//if (events[i].area <= 0) {
			//events[i].area = 0x09;
			//}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MINOR, events[i].area_minor);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);
			input_report_key(data->input_dev, BTN_PALM, data->palm_flag);

			touchs |= BIT(events[i].id);
			data->touchs |= BIT(events[i].id);

			if (events[i].flag == FTS_TOUCH_DOWN) {
				location_detect(data, loc, events[i].x, events[i].y);
				events[i].p_x = events[i].x;
				events[i].p_y = events[i].y;
				events[i].mcount = 0;
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
				FTS_INFO("[P] tID:%d.%d x:%d y:%d z:%d p:%d major:%d minor:%d loc:%s tc:%ld",
						events[i].id, (data->input_dev->mt->trkid - 1) & TRKID_MAX,
						events[i].x, events[i].y,
						events[i].p, events[i].palm,
						events[i].area, events[i].area_minor, loc,
						FTS_TOUCH_COUNT(&touchs));
#else
				FTS_INFO("[P] tID:%d.%d z:%d p:%d major:%d minor:%d loc:%s tc:%ld",
						events[i].id, (data->input_dev->mt->trkid - 1) & TRKID_MAX,
						events[i].p, events[i].palm,
						events[i].area, events[i].area_minor, loc,
						FTS_TOUCH_COUNT(&touchs));
#endif
			} else if (events[i].flag == FTS_TOUCH_CONTACT) {
				events[i].mcount++;
			}
		} else {
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			data->touchs &= ~BIT(events[i].id);
			location_detect(data, loc, events[i].x, events[i].y);

			data->palm_flag &= ~BIT(events[i].id);
			input_report_key(data->input_dev, BTN_PALM, data->palm_flag);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			FTS_INFO("[R] tID:%d loc:%s dd:%d,%d pc:%ld mc:%d tc:%ld lx:%d ly:%d",
					events[i].id, loc, events[i].x - events[i].p_x, events[i].y - events[i].p_y,
					FTS_TOUCH_COUNT(&data->palm_flag),
					events[i].mcount, FTS_TOUCH_COUNT(&touchs),
					events[i].x, events[i].y);
#else
			FTS_INFO("[R] tID:%d loc:%s dd:%d,%d pc:%ld mc:%d tc:%ld",
					events[i].id, loc, events[i].x - events[i].p_x, events[i].y - events[i].p_y,
					FTS_TOUCH_COUNT(&data->palm_flag),
					events[i].mcount, FTS_TOUCH_COUNT(&touchs));
#endif
		}
	}

	if (unlikely(data->touchs ^ touchs)) {
		for (i = 0; i < max_touch_num; i++)  {
			if (BIT(i) & (data->touchs ^ touchs)) {
				FTS_INFO("[R] tID:%d", i);
				va_reported = true;
				input_mt_slot(data->input_dev, i);
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
				data->palm_flag &= ~BIT(i);
				input_report_key(data->input_dev, BTN_PALM, data->palm_flag);
			}
		}
	}
	data->touchs = touchs;

	if (va_reported) {
		/* touchs==0, there's no point but key */
		if (EVENT_NO_DOWN(data) || (!touchs)) {
			data->print_info_cnt_release = 0;
			if (data->log_level >= 2)
				FTS_DEBUG("[B]Points All Up!");

			input_report_key(data->input_dev, BTN_TOUCH, 0);
		} else {
			input_report_key(data->input_dev, BTN_TOUCH, 1);
		}
	}

	input_sync(data->input_dev);
	return 0;
}

#else
static int fts_input_report_a(struct fts_ts_data *data)
{
	int i = 0;
	int touchs = 0;
	bool va_reported = false;
	struct ts_event *events = data->events;

	for (i = 0; i < data->touch_point; i++) {
		if (fts_input_report_key(data, i) == 0) {
			continue;
		}

		va_reported = true;
		if (EVENT_DOWN(events[i].flag)) {
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, events[i].id);
#if FTS_REPORT_PRESSURE_EN
			if (events[i].p <= 0) {
				events[i].p = 0x3f;
			}
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
			if (events[i].area <= 0) {
				events[i].area = 0x09;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);

			input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

			input_mt_sync(data->input_dev);

			if ((data->log_level >= 2) ||
			    ((1 == data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
				FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
				          events[i].id,
				          events[i].x, events[i].y,
				          events[i].p, events[i].area);
			}
			touchs++;
		}
	}

	/* last point down, current no point but key */
	if (data->touchs && !touchs) {
		va_reported = true;
	}
	data->touchs = touchs;

	if (va_reported) {
		if (EVENT_NO_DOWN(data)) {
			if (data->log_level >= 1) {
				FTS_DEBUG("[A]Points All Up!");
			}
			input_report_key(data->input_dev, BTN_TOUCH, 0);
			input_mt_sync(data->input_dev);
		} else {
			input_report_key(data->input_dev, BTN_TOUCH, 1);
		}
	}

	input_sync(data->input_dev);
	return 0;
}
#endif

#if FTS_PEN_EN
static int fts_input_pen_report(struct fts_ts_data *data)
{
	struct input_dev *pen_dev = data->pen_dev;
	struct pen_event *pevt = &data->pevent;
	u8 *buf = data->point_buf;


	if (buf[3] & 0x08)
		input_report_key(pen_dev, BTN_STYLUS, 1);
	else
		input_report_key(pen_dev, BTN_STYLUS, 0);

	if (buf[3] & 0x02)
		input_report_key(pen_dev, BTN_STYLUS2, 1);
	else
		input_report_key(pen_dev, BTN_STYLUS2, 0);

	pevt->inrange = (buf[3] & 0x20) ? 1 : 0;
	pevt->tip = (buf[3] & 0x01) ? 1 : 0;
	pevt->x = ((buf[4] & 0x0F) << 8) + buf[5];
	pevt->y = ((buf[6] & 0x0F) << 8) + buf[7];
	pevt->p = ((buf[8] & 0x0F) << 8) + buf[9];
	pevt->id = buf[6] >> 4;
	pevt->flag = buf[4] >> 6;
	pevt->tilt_x = (buf[10] << 8) + buf[11];
	pevt->tilt_y = (buf[12] << 8) + buf[13];
	pevt->tool_type = BTN_TOOL_PEN;

	if (data->log_level >= 2  ||
	    ((1 == data->log_level) && (FTS_TOUCH_DOWN == pevt->flag))) {
		FTS_DEBUG("[PEN]x:%d,y:%d,p:%d,inrange:%d,tip:%d,flag:%d DOWN",
		          pevt->x, pevt->y, pevt->p, pevt->inrange,
		          pevt->tip, pevt->flag);
	}

	if ( (data->log_level >= 1) && (!pevt->inrange)) {
		FTS_DEBUG("[PEN]UP");
	}

	input_report_abs(pen_dev, ABS_X, pevt->x);
	input_report_abs(pen_dev, ABS_Y, pevt->y);
	input_report_abs(pen_dev, ABS_PRESSURE, pevt->p);

	/* check if the pen support tilt event */
	if ((pevt->tilt_x != 0) || (pevt->tilt_y != 0)) {
		input_report_abs(pen_dev, ABS_TILT_X, pevt->tilt_x);
		input_report_abs(pen_dev, ABS_TILT_Y, pevt->tilt_y);
	}

	input_report_key(pen_dev, BTN_TOUCH, pevt->tip);
	input_report_key(pen_dev, BTN_TOOL_PEN, pevt->inrange);
	input_sync(pen_dev);

	return 0;
}
#endif

#define OUT_POCKET 10
#define IN_POCKET 11
static int fts_read_pocket_result(struct fts_ts_data *ts_data)
{
	int ret = 0;
	u8 val = 0;

	ret = fts_read_reg(POCKET_DATA_REG, &val);
	if (ret < 0) {
		FTS_ERROR("read pocket data fail");
		return ret;
	}

	if (ts_data->pocket_state == val)
		return 0;
	else
		ts_data->pocket_state = val;

	if (val == 0) {
		FTS_INFO("pocket status out");
		ts_data->hover_event = OUT_POCKET;
	} else if (val == 1) {
		FTS_INFO("pocket status in");
		ts_data->hover_event = IN_POCKET;
	}

	input_report_abs(ts_data->pdata->input_dev_proximity, ABS_MT_CUSTOM, ts_data->hover_event);
	input_sync(ts_data->pdata->input_dev_proximity);
	FTS_INFO("proximity: %d", ts_data->hover_event);

	return 0;
}

static int fts_read_proximity_result(struct fts_ts_data *ts_data)
{
	int ret = 0;
	u8 val = 0;

	ret = fts_read_reg(PROXIMITY_DATA_REG, &val);
	if (ret < 0) {
		FTS_ERROR("read pocket data fail");
		return ret;
	}

	if (ts_data->hover_event == (val >> 4))
		return 0;
	else
		ts_data->hover_event = (val >> 4);

	input_report_abs(ts_data->pdata->input_dev_proximity, ABS_MT_CUSTOM, ts_data->hover_event);
	input_sync(ts_data->pdata->input_dev_proximity);
	FTS_INFO("proximity: %d", ts_data->hover_event);

	return 0;
}

int fts_read_fod_data(struct fts_ts_data *ts_data)
{
	int ret = 0;
	u8 val = 0;
	u8 data[6] = { 0 };

	ret = fts_read_reg(FOD_TX_NUM, &val);
	if (ret < 0) {
		FTS_ERROR("read fod tx num fail");
		return ret;
	}
	FTS_INFO("fod tx num %d", val);
	data[0] = val;

	ret = fts_read_reg(FOD_RX_NUM, &val);
	if (ret < 0) {
		FTS_ERROR("read fod rx num fail");
		return ret;
	}
	FTS_INFO("fod rx num %d", val);
	data[1] = val;

	ret = fts_read_reg(FOD_VI_SIZE, &val);
	if (ret < 0) {
		FTS_ERROR("read fod vi size fail");
		return ret;
	}
	FTS_INFO("fod vi size %d", val);
	data[2] = val;

	sec_input_set_fod_info(ts_data->dev, data[0], data[1], data[2], data[3]);

	return ret;
}

int fts_set_fod_rect(struct fts_ts_data *ts_data)
{
	u8 data[10] = { 0 };
	int ret, i;

	FTS_INFO("l:%d, t:%d, r:%d, b:%d",
		ts_data->pdata->fod_data.rect_data[0], ts_data->pdata->fod_data.rect_data[1],
		ts_data->pdata->fod_data.rect_data[2], ts_data->pdata->fod_data.rect_data[3]);

	for (i = 0; i < 4; i++) {
		data[i * 2 + 1] = (ts_data->pdata->fod_data.rect_data[i] >> 8) & 0xFF;
		data[i * 2 + 2] = ts_data->pdata->fod_data.rect_data[i] & 0xFF;
	}

	data[0] = FTS_REG_FOD_RECT;
	ret = fts_write(data, 9);
	if (ret < 0)
		FTS_ERROR("Failed to write");

	return ret;
}

static int fts_read_fod_result(struct fts_ts_data *ts_data)
{
	int ret = 0;
	u8 val = 0;
	u8 fod_coordinate[4] = {0};

	ret = fts_read_reg(FOD_EVENT_REG, &val);
	if (ret < 0) {
		FTS_ERROR("read fod data fail");
		return ret;
	}

	if (ts_data->fod_state == val)
		return 0;
	else
		ts_data->fod_state = val;

	fod_coordinate[0] = 0xE7;
	ret = fts_read(fod_coordinate, 1, fod_coordinate, 4);
	if (ret < 0) {
		FTS_ERROR("read fod coordinate fail");
		return ret;
	}

	// #0 : long press, #1 : press, #2 : release, #3 : out of area, 0xff : no event
	if (val == 0 || val == 1) {
		ts_data->pdata->gesture_id = SPONGE_EVENT_TYPE_FOD_PRESS;
		ts_data->pdata->gesture_x = (fod_coordinate[0] << 8) + fod_coordinate[1];
		ts_data->pdata->gesture_y = (fod_coordinate[2] << 8) + fod_coordinate[3];
		sec_input_gesture_report(ts_data->dev, ts_data->pdata->gesture_id,
						ts_data->pdata->gesture_x, ts_data->pdata->gesture_y);
		FTS_INFO("FOD %s PRESS", val ? "" : "LONG");
	} else if (val == 2) {
		ts_data->pdata->gesture_id = SPONGE_EVENT_TYPE_FOD_RELEASE;
		ts_data->pdata->gesture_x = (fod_coordinate[0] << 8) + fod_coordinate[1];
		ts_data->pdata->gesture_y = (fod_coordinate[2] << 8) + fod_coordinate[3];
		sec_input_gesture_report(ts_data->dev, ts_data->pdata->gesture_id,
						ts_data->pdata->gesture_x, ts_data->pdata->gesture_y);
	} else if (val == 3) {
		ts_data->pdata->gesture_id = SPONGE_EVENT_TYPE_FOD_OUT;
		ts_data->pdata->gesture_x = (fod_coordinate[0] << 8) + fod_coordinate[1];
		ts_data->pdata->gesture_y = (fod_coordinate[2] << 8) + fod_coordinate[3];
		sec_input_gesture_report(ts_data->dev, ts_data->pdata->gesture_id,
						ts_data->pdata->gesture_x, ts_data->pdata->gesture_y);
	} else if (val == 0xff) {
		FTS_INFO("fod has no event");
	}

	return 0;
}

static int fts_read_touchdata(struct fts_ts_data *data)
{
	int ret = 0;
	int size = 0;
	u8 *buf = data->point_buf;

	memset(buf, 0xFF, data->pnt_buf_size);
	buf[0] = 0x01;

	if (atomic_read(&data->pdata->power_state) == SEC_INPUT_STATE_LPM) {
		if (0 == fts_gesture_readdata(data, NULL)) {
			FTS_INFO("succuss to get gesture data in irq handler");
			return 1;
		}
	}

#if defined(FTS_HIGH_REPORT) && FTS_HIGH_REPORT
	if (data->point_num >= data->max_touch_number)
		size = data->pnt_buf_size - 1;
	else if (data->point_num > 2)
		size = data->point_num * 6 + 3;
	else
		size = FTS_SIZE_DEFAULT;
#if defined(FTS_PEN_EN) && FTS_PEN_EN
	size = FTS_SIZE_DEFAULT;
#endif

	ret = fts_read(buf, 1, buf + 1, size);
	if (ret < 0) {
		FTS_ERROR("read touchdata failed, ret:%d", ret);
		return ret;
	}

	if ((buf[size] != 0xFF) && (size < (data->pnt_buf_size - 1))
#if defined(FTS_PEN_EN) && FTS_PEN_EN
	    && ((buf[2] & 0xF0) != 0xB0)
#endif
	   ) {
		buf[0] += size;
		ret = fts_read(buf, 1, buf + 1 + size, data->pnt_buf_size - 1 - size);
		if (ret < 0) {
			FTS_ERROR("read touchdata2 failed, ret:%d", ret);
			return ret;
		}
	}
#else
	size = data->pnt_buf_size - 1;
	ret = fts_read(buf, 1, buf + 1, size);
	if (ret < 0) {
		FTS_ERROR("read touchdata failed, ret:%d", ret);
		return ret;
	}
#endif

	if (data->log_level >= 3) {
		fts_show_touch_buffer(buf, data->pnt_buf_size);
	}

	return 0;
}

static int fts_read_parse_touchdata(struct fts_ts_data *data)
{
	int ret = 0;
	int i = 0;
	u8 pointid = 0;
	int base = 0;
	struct ts_event *events = data->events;
	int max_touch_num = data->max_touch_number;
	u8 *buf = data->point_buf;
	unsigned long palm_temp = data->palm_flag;

	ret = fts_read_touchdata(data);
	if (ret) {
		return ret;
	}

	if ((buf[1] & 0x01) != data->pdata->wet_mode) {
		data->pdata->wet_mode = (buf[1] & 0x01);

		if (data->pdata->wet_mode) {
			FTS_INFO("Waterproof mode on");
			data->pdata->hw_param.wet_count++;
		} else {
			FTS_INFO("Waterproof mode off");
		}
	}

	if ((buf[1] & 0x02) != atomic_read(&data->pdata->touch_noise_status)) {
		atomic_set(&data->pdata->touch_noise_status, (buf[1] & 0x02));

		if (atomic_read(&data->pdata->touch_noise_status)) {
			FTS_INFO("Noise mode on");
			data->pdata->hw_param.noise_count++;
		} else {
			FTS_INFO("Noise mode off");
		}
	}

#if FTS_PEN_EN
	if ((buf[2] & 0xF0) == 0xB0) {
		fts_input_pen_report(data);
		return 2;
	}
#endif

	data->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
	data->touch_point = 0;

	if ((data->point_num == 0x0F) && (buf[2] == 0xFF) && (buf[3] == 0xFF)
	    && (buf[4] == 0xFF) && (buf[5] == 0xFF) && (buf[6] == 0xFF)) {
		FTS_DEBUG("touch buff is 0xff, need recovery state");
		fts_release_all_finger();
		fts_tp_state_recovery(data);
		data->point_num = 0;
		return -EIO;
	}

	if (data->point_num > max_touch_num) {
		FTS_INFO("invalid point_num(%d)", data->point_num);
		data->point_num = 0;
		return -EIO;
	}

	for (i = 0; i < max_touch_num; i++) {
		base = FTS_ONE_TCH_LEN * i;
		pointid = (buf[FTS_TOUCH_ID_POS + base]) >> 4;
		if (pointid >= FTS_MAX_ID)
			break;
		else if (pointid >= max_touch_num) {
			FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
			return -EINVAL;
		}

		data->touch_point++;
		events[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 8) +
		              (buf[FTS_TOUCH_X_L_POS + base] & 0xFF);
		events[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 8) +
		              (buf[FTS_TOUCH_Y_L_POS + base] & 0xFF);
		events[i].flag = buf[FTS_TOUCH_EVENT_POS + base] >> 6;
		events[i].id = buf[FTS_TOUCH_ID_POS + base] >> 4;
		events[i].area = buf[FTS_TOUCH_AREA_POS + base];
		events[i].p =  buf[FTS_TOUCH_PRE_POS + base];
		events[i].palm = (buf[FTS_TOUCH_EVENT_POS + base] >> 4) & 0x01;
		if (events[i].palm)
			data->palm_flag |= BIT(events[i].id);
		else
			data->palm_flag &= ~BIT(events[i].id);
		events[i].area_minor = buf[FTS_TOUCH_PRE_POS + base];

		if (palm_temp != data->palm_flag) {
			FTS_INFO("tID:%d palm changed to %d", events[i].id, events[i].palm);
			palm_temp = data->palm_flag;
		}

		if (EVENT_DOWN(events[i].flag) && (data->point_num == 0)) {
			FTS_INFO("abnormal touch data from fw");
			return -EIO;
		}
	}

	if (data->touch_point == 0) {
		FTS_INFO("no touch point information(%02x)", buf[2]);
		return -EIO;
	}

	return 0;
}

static void fts_irq_read_report(void)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(1);
#endif

#if FTS_POINT_REPORT_CHECK_EN
	fts_prc_queue_work(ts_data);
#endif

	if (ts_data->pdata->pocket_mode)
		fts_read_pocket_result(ts_data);

	if (ts_data->pdata->ed_enable)
		fts_read_proximity_result(ts_data);

	ret = fts_read_parse_touchdata(ts_data);
	if (ret == 0) {
		mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
		fts_input_report_b(ts_data);
#else
		fts_input_report_a(ts_data);
#endif
		mutex_unlock(&ts_data->report_mutex);
	}

	if (ts_data->fod_mode & 0x01)
		fts_read_fod_result(ts_data);

#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(0);
#endif
}

#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
static void fts_wakeup_source_init(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 110))
	ts_data->wake_lock = wakeup_source_register(ts_data->sec.fac_dev, "TSP");
#else
	wakeup_source_init(ts_data->wake_lock, "tsp");
#endif
	device_init_wakeup(ts_data->sec.fac_dev, true);
	FTS_FUNC_EXIT();
}

static void fts_wakeup_source_unregister(struct fts_ts_data *ts_data)
{
	if (!ts_data->wake_lock)
		return;

	FTS_FUNC_ENTER();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 110))
	wakeup_source_unregister(ts_data->wake_lock);
#else
	wakeup_source_trash(ts_data->wake_lock);
#endif
	FTS_FUNC_EXIT();
}
#endif

static void fts_handler_wait_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data, irq_work);
	struct irq_desc *desc = irq_to_desc(ts_data->irq);
	int ret;

	ret = wait_for_completion_interruptible_timeout(&ts_data->pm_completion,
			msecs_to_jiffies(SEC_TS_WAKE_LOCK_TIME));
	if (ret == 0) {
		FTS_ERROR("LPM: pm resume is not handled");
		goto out;
	}
	if (ret < 0) {
		FTS_ERROR("LPM: -ERESTARTSYS if interrupted, %d", ret);
		goto out;
	}

	if (desc && desc->action && desc->action->thread_fn) {
		FTS_INFO("run irq thread");
		desc->action->thread_fn(ts_data->irq, desc->action->dev_id);
	}
out:
	fts_irq_enable();
}

static irqreturn_t fts_irq_handler(int irq, void *data)
{
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	struct fts_ts_data *ts_data = fts_data;

	if (ts_data->suspended) {
		__pm_wakeup_event(ts_data->wake_lock, SEC_TS_WAKE_LOCK_TIME);

		if (!ts_data->pm_completion.done) {
			if (!IS_ERR_OR_NULL(ts_data->irq_workqueue)) {
				FTS_INFO("disable irq and queue waiting work");
				fts_irq_disable();
				queue_work(ts_data->irq_workqueue, &ts_data->irq_work);
			} else {
				FTS_INFO("irq_workqueue not exist");
			}
			return IRQ_HANDLED;
		}
		FTS_INFO("run LPM interrupt handler");
	}
#endif

	fts_irq_read_report();
	return IRQ_HANDLED;
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct sec_ts_plat_data *pdata = ts_data->pdata;

	ts_data->irq_workqueue = create_singlethread_workqueue("fts_irq_wq");
	if (!IS_ERR_OR_NULL(ts_data->irq_workqueue)) {
		INIT_WORK(&ts_data->irq_work, fts_handler_wait_resume_work);
		FTS_INFO("set fts_handler_wait_resume_work");
	} else {
		FTS_ERROR("failed to create irq_workqueue, err: %ld", PTR_ERR(ts_data->irq_workqueue));
	}

	ts_data->irq = gpio_to_irq(pdata->irq_gpio);
	FTS_INFO("irq:%d", ts_data->irq);
	ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler,
	                           IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	                           FTS_DRIVER_NAME, ts_data);

	return ret;
}

#if FTS_PEN_EN
static int fts_input_pen_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct input_dev *pen_dev;
	struct sec_ts_plat_data *pdata = ts_data->pdata;

	FTS_FUNC_ENTER();
	pen_dev = devm_input_allocate_device(ts_data->dev);
	if (!pen_dev) {
		FTS_ERROR("Failed to allocate memory for input_pen device");
		return -ENOMEM;
	}

	pen_dev->dev.parent = ts_data->dev;
	pen_dev->name = FTS_DRIVER_PEN_NAME;
	pen_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	__set_bit(ABS_X, pen_dev->absbit);
	__set_bit(ABS_Y, pen_dev->absbit);
	__set_bit(BTN_STYLUS, pen_dev->keybit);
	__set_bit(BTN_STYLUS2, pen_dev->keybit);
	__set_bit(BTN_TOUCH, pen_dev->keybit);
	__set_bit(BTN_TOOL_PEN, pen_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, pen_dev->propbit);
	input_set_abs_params(pen_dev, ABS_X, 0, pdata->max_x, 0, 0);
	input_set_abs_params(pen_dev, ABS_Y, 0, pdata->max_y, 0, 0);
	input_set_abs_params(pen_dev, ABS_PRESSURE, 0, 4096, 0, 0);

	ret = input_register_device(pen_dev);
	if (ret) {
		FTS_ERROR("Input device registration failed");
		return ret;
	}

	ts_data->pen_dev = pen_dev;
	FTS_FUNC_EXIT();
	return 0;
}
#endif
/*
static int fts_input_set_prop(struct fts_ts_data *ts_data, struct input_dev *input_dev, int propbit)
{
	int key_num = 0;
	int ret;

	if (ts_data->bus_type == BUS_TYPE_I2C)
		input_dev->id.bustype = BUS_I2C;
	else
		input_dev->id.bustype = BUS_SPI;
	input_dev->dev.parent = ts_data->dev;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(propbit, input_dev->propbit);
	set_bit(KEY_WAKEUP, input_dev->keybit);
	set_bit(KEY_INT_CANCEL, input_dev->keybit);
	set_bit(BTN_PALM, input_dev->keybit);

	if (ts_data->have_key) {
		FTS_INFO("set key capabilities");
		for (key_num = 0; key_num < ts_data->key_number; key_num++)
			input_set_capability(input_dev, EV_KEY, ts_data->keys[key_num]);
	}

#if FTS_MT_PROTOCOL_B_EN
	if (propbit == INPUT_PROP_DIRECT)
		input_mt_init_slots(input_dev, ts_data->max_touch_number, INPUT_MT_DIRECT);
	else
		input_mt_init_slots(input_dev, ts_data->max_touch_number, INPUT_MT_POINTER);
#else
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0F, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, ts_data->pdata->x_min, ts_data->pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, ts_data->pdata->y_min, ts_data->pdata->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
	if (propbit == INPUT_PROP_DIRECT)
		input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif

	ret = input_register_device(input_dev);
	if (ret) {
		FTS_ERROR("Input device registration failed");
		input_set_drvdata(input_dev, NULL);
		return ret;
	}

	return 0;
}

static int fts_input_init(struct fts_ts_data *ts_data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ts_data->input_dev = devm_input_allocate_device(ts_data->dev);
	if (!ts_data->input_dev) {
		FTS_ERROR("Failed to allocate memory for input device");
		return -ENOMEM;
	}

	// Init and register Input device
	ts_data->input_dev->name = "sec_touchscreen";
	input_set_drvdata(ts_data->input_dev, ts_data);
	ret = fts_input_set_prop(ts_data, ts_data->input_dev, INPUT_PROP_DIRECT);
	if (ret)
		return ret;

#if FTS_PEN_EN
	ret = fts_input_pen_init(ts_data);
	if (ret) {
		FTS_ERROR("Input-pen device registration failed");
		input_set_drvdata(ts_data->input_dev, NULL);
		return ret;
	}
#endif

	if (ts_data->pdata->support_dex) {
		ts_data->input_dev_pad = devm_input_allocate_device(ts_data->dev);
		if (!ts_data->input_dev_pad) {
			FTS_ERROR("Failed to allocate memory for input pad device");
			return 0;
		}
		ts_data->input_dev_pad->name = "sec_touchpad";
		fts_input_set_prop(ts_data, ts_data->input_dev_pad, INPUT_PROP_POINTER);
	}

	FTS_FUNC_EXIT();
	return 0;
}
*/
static int fts_report_buffer_init(struct fts_ts_data *ts_data)
{
	int point_num = 0;
	int events_num = 0;

	ts_data->max_touch_number = FTS_MAX_POINTS_SUPPORT;
	point_num = ts_data->max_touch_number;
	ts_data->pnt_buf_size = point_num * FTS_ONE_TCH_LEN + 3;
	ts_data->point_buf = (u8 *)devm_kzalloc(ts_data->dev, ts_data->pnt_buf_size + 1, GFP_KERNEL);
	if (!ts_data->point_buf) {
		FTS_ERROR("failed to alloc memory for point buf");
		return -ENOMEM;
	}

	events_num = point_num * sizeof(struct ts_event);
	ts_data->events = (struct ts_event *)devm_kzalloc(ts_data->dev, events_num, GFP_KERNEL);
	if (!ts_data->events) {
		FTS_ERROR("failed to alloc memory for point events");
		return -ENOMEM;
	}

	return 0;
}

#if FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
* Power Control
*****************************************************************************/
#if FTS_PINCTRL_EN
static int fts_pinctrl_init(struct fts_ts_data *ts)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ts->pinctrl = devm_pinctrl_get(ts->dev);
	if (IS_ERR_OR_NULL(ts->pinctrl)) {
		FTS_ERROR("Failed to get pinctrl, please check dts");
		ret = PTR_ERR(ts->pinctrl);
		goto err_pinctrl_get;
	}

	ts->pins_active = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(ts->pins_active)) {
		FTS_ERROR("Pin state[active] not found");
		ret = PTR_ERR(ts->pins_active);
		goto err_pinctrl_lookup;
	}

	ts->pins_suspend = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(ts->pins_suspend)) {
		FTS_ERROR("Pin state[suspend] not found");
		ret = PTR_ERR(ts->pins_suspend);
		goto err_pinctrl_lookup;
	}

	ts->pins_release = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_release");
	if (IS_ERR_OR_NULL(ts->pins_release)) {
		FTS_ERROR("Pin state[release] not found");
		ret = PTR_ERR(ts->pins_release);
	}

	FTS_FUNC_EXIT();
	return 0;
err_pinctrl_lookup:
	if (ts->pinctrl) {
		devm_pinctrl_put(ts->pinctrl);
	}
err_pinctrl_get:
	ts->pinctrl = NULL;
	ts->pins_release = NULL;
	ts->pins_suspend = NULL;
	ts->pins_active = NULL;
	return ret;
}

static int fts_pinctrl_select_normal(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl && ts->pins_active) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pins_active);
		if (ret < 0) {
			FTS_ERROR("Set normal pin state error:%d", ret);
		} else {
			FTS_INFO("success");
		}
	}

	return ret;
}

static int fts_pinctrl_select_suspend(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl && ts->pins_suspend) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pins_suspend);
		if (ret < 0) {
			FTS_ERROR("Set suspend pin state error:%d", ret);
		} else {
			FTS_INFO("success");
		}
	}

	return ret;
}

static int fts_pinctrl_select_release(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl) {
		if (IS_ERR_OR_NULL(ts->pins_release)) {
			devm_pinctrl_put(ts->pinctrl);
			ts->pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(ts->pinctrl, ts->pins_release);
			if (ret < 0)
				FTS_ERROR("Set gesture pin state error:%d", ret);
			else
				FTS_INFO("success");
		}
	}

	return ret;
}
#endif /* FTS_PINCTRL_EN */

static int fts_power_source_ctrl(struct fts_ts_data *ts_data, int enable)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(ts_data->vdd)) {
		FTS_ERROR("vdd is invalid");
		return -EINVAL;
	}

	FTS_FUNC_ENTER();
	if (enable) {
		if (ts_data->power_disabled) {
			FTS_DEBUG("regulator enable !");
			ret = regulator_enable(ts_data->vdd);
			if (ret) {
				FTS_ERROR("enable vdd regulator failed,ret=%d", ret);
			}

			if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
				ret = regulator_enable(ts_data->vcc_i2c);
				if (ret) {
					FTS_ERROR("enable vcc_i2c regulator failed,ret=%d", ret);
				}
			}
			ts_data->power_disabled = false;
		}
	} else {
		if (!ts_data->power_disabled) {
			FTS_DEBUG("regulator disable !");
			ret = regulator_disable(ts_data->vdd);
			if (ret) {
				FTS_ERROR("disable vdd regulator failed,ret=%d", ret);
			}
			if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
				ret = regulator_disable(ts_data->vcc_i2c);
				if (ret) {
					FTS_ERROR("disable vcc_i2c regulator failed,ret=%d", ret);
				}
			}
			ts_data->power_disabled = true;
		}
	}

	FTS_FUNC_EXIT();
	return ret;
}

/*****************************************************************************
* Name: fts_power_source_init
* Brief: Init regulator power:vdd/vcc_io(if have), generally, no vcc_io
*        vdd---->vdd-supply in dts, kernel will auto add "-supply" to parse
*        Must be call after fts_gpio_configure() execute,because this function
*        will operate reset-gpio which request gpio in fts_gpio_configure()
* Input:
* Output:
* Return: return 0 if init power successfully, otherwise return error code
*****************************************************************************/
static int fts_power_source_init(struct fts_ts_data *ts_data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ts_data->vdd = regulator_get(ts_data->dev, "vdd");
	if (IS_ERR_OR_NULL(ts_data->vdd)) {
		ret = PTR_ERR(ts_data->vdd);
		FTS_ERROR("get vdd regulator failed,ret=%d", ret);
		return ret;
	}

	if (regulator_count_voltages(ts_data->vdd) > 0) {
		ret = regulator_set_voltage(ts_data->vdd, FTS_VTG_MIN_UV,
		                            FTS_VTG_MAX_UV);
		if (ret) {
			FTS_ERROR("vdd regulator set_vtg failed ret=%d", ret);
			regulator_put(ts_data->vdd);
			return ret;
		}
	}

	ts_data->vcc_i2c = regulator_get(ts_data->dev, "vcc_i2c");
	if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
		if (regulator_count_voltages(ts_data->vcc_i2c) > 0) {
			ret = regulator_set_voltage(ts_data->vcc_i2c,
			                            FTS_I2C_VTG_MIN_UV,
			                            FTS_I2C_VTG_MAX_UV);
			if (ret) {
				FTS_ERROR("vcc_i2c regulator set_vtg failed,ret=%d", ret);
				regulator_put(ts_data->vcc_i2c);
			}
		}
	}

#if FTS_PINCTRL_EN
	fts_pinctrl_init(ts_data);
	fts_pinctrl_select_normal(ts_data);
#endif

	ts_data->power_disabled = true;
	ret = fts_power_source_ctrl(ts_data, ENABLE);
	if (ret) {
		FTS_ERROR("fail to enable power(regulator)");
	}

	FTS_FUNC_EXIT();
	return ret;
}

static int fts_power_source_exit(struct fts_ts_data *ts_data)
{
#if FTS_PINCTRL_EN
	fts_pinctrl_select_release(ts_data);
#endif

	fts_power_source_ctrl(ts_data, DISABLE);

	if (!IS_ERR_OR_NULL(ts_data->vdd)) {
		if (regulator_count_voltages(ts_data->vdd) > 0)
			regulator_set_voltage(ts_data->vdd, 0, FTS_VTG_MAX_UV);
		regulator_put(ts_data->vdd);
	}

	if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
		if (regulator_count_voltages(ts_data->vcc_i2c) > 0)
			regulator_set_voltage(ts_data->vcc_i2c, 0, FTS_I2C_VTG_MAX_UV);
		regulator_put(ts_data->vcc_i2c);
	}

	return 0;
}

static int fts_power_source_suspend(struct fts_ts_data *ts_data)
{
	int ret = 0;

#if FTS_PINCTRL_EN
	fts_pinctrl_select_suspend(ts_data);
#endif

	ret = fts_power_source_ctrl(ts_data, DISABLE);
	if (ret < 0) {
		FTS_ERROR("power off fail, ret=%d", ret);
	}

	return ret;
}

static int fts_power_source_resume(struct fts_ts_data *ts_data)
{
	int ret = 0;

#if FTS_PINCTRL_EN
	fts_pinctrl_select_normal(ts_data);
#endif

	ret = fts_power_source_ctrl(ts_data, ENABLE);
	if (ret < 0) {
		FTS_ERROR("power on fail, ret=%d", ret);
	}

	return ret;
}
#else
int fts_power_ctrl(struct device *dev, bool on)
{
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int ret = 0;

	if (pdata->power_enabled == on) {
		FTS_INFO("power_enabled %d", pdata->power_enabled);
		return ret;
	}

	if (on) {
		if (!pdata->not_support_io_ldo) {
			ret = regulator_enable(pdata->dvdd);
			if (ret) {
				FTS_ERROR("Failed to enable iovdd:%d", ret);
				return ret;
			}
		}
		ret = regulator_enable(pdata->avdd);
		if (ret) {
			if (!pdata->not_support_io_ldo) {
				regulator_disable(pdata->dvdd);
			}
			FTS_ERROR("Failed to enable avdd:%d", ret);
			return ret;
		}
	} else {
		if (!pdata->not_support_io_ldo) {
			ret = regulator_disable(pdata->dvdd);
			if (ret)
				FTS_ERROR("Failed to disable iovdd:%d", ret);
		}
		ret = regulator_disable(pdata->avdd);
		if (ret)
			FTS_ERROR("Failed to disable avdd:%d", ret);
	}

	pdata->power_enabled = on;

	if (!pdata->not_support_io_ldo) {
		FTS_INFO("%s: avdd:%s, iovdd:%s", on ? "on" : "off",
				regulator_is_enabled(pdata->avdd) ? "on" : "off",
				regulator_is_enabled(pdata->dvdd) ? "on" : "off");
	} else {
		FTS_INFO("%s: avdd:%s", on ? "on" : "off", regulator_is_enabled(pdata->avdd) ? "on" : "off");
	}
	return ret;
}


int fts_ts_power_on(struct fts_ts_data *ts_data)
{
	int ret = 0;

	FTS_INFO("power on %d", atomic_read(&ts_data->pdata->power_state));
	ts_data->pdata->pinctrl_configure(ts_data->dev, true);

	if (atomic_read(&ts_data->pdata->power_state) == SEC_INPUT_STATE_POWER_ON) {
		FTS_ERROR("already power on");
		return 0;
	}

	ret = ts_data->pdata->power(ts_data->dev, true);
	sec_delay(5);
	if (!ret)
		atomic_set(&ts_data->pdata->power_state, SEC_INPUT_STATE_POWER_ON);
	else
		FTS_ERROR("failed power on, %d", ret);

	return ret;
}

int fts_ts_power_off(struct fts_ts_data *ts_data)
{
	int ret;

	FTS_INFO("power off %d", atomic_read(&ts_data->pdata->power_state));
	if (atomic_read(&ts_data->pdata->power_state) == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("already power off");
		return 0;
	}

	ret = ts_data->pdata->power(ts_data->dev, false);
	if (!ret)
		atomic_set(&ts_data->pdata->power_state, SEC_INPUT_STATE_POWER_OFF);
	else
		FTS_ERROR("failed power off, %d", ret);

	ts_data->pdata->pinctrl_configure(ts_data->dev, false);

	return ret;
}
#endif /* FTS_POWER_SOURCE_CUST_EN */

/*
static int fts_gpio_configure(struct fts_ts_data *data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	// request irq gpio
	if (gpio_is_valid(data->pdata->irq_gpio)) {
		ret = gpio_request(data->pdata->irq_gpio, "fts_irq_gpio");
		if (ret) {
			FTS_ERROR("[GPIO]irq gpio request failed");
			goto err_irq_gpio_req;
		}

		ret = gpio_direction_input(data->pdata->irq_gpio);
		if (ret) {
			FTS_ERROR("[GPIO]set_direction for irq gpio failed");
			goto err_irq_gpio_dir;
		}
	}

	FTS_FUNC_EXIT();
	return 0;

err_irq_gpio_dir:
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
	FTS_FUNC_EXIT();
	return ret;
}

static int fts_get_dt_coords(struct device *dev, char *name,
                             struct sec_ts_plat_data *pdata)
{
	int ret = 0;
	u32 coords[FTS_COORDS_ARR_SIZE] = { 0 };
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size;
	u32 px_zone[3] = { 0 };

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FTS_COORDS_ARR_SIZE) {
		FTS_ERROR("invalid:%s, size:%d", name, coords_size);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, name, coords, coords_size);
	if (ret < 0) {
		FTS_ERROR("Unable to read %s, please check dts", name);
		pdata->x_min = FTS_X_MIN_DISPLAY_DEFAULT;
		pdata->y_min = FTS_Y_MIN_DISPLAY_DEFAULT;
		pdata->max_x = FTS_max_x_DISPLAY_DEFAULT;
		pdata->max_y = FTS_max_y_DISPLAY_DEFAULT;
		return -ENODATA;
	} else {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->max_x = coords[2];
		pdata->max_y = coords[3];
	}

	FTS_INFO("display x(%d %d) y(%d %d)", pdata->x_min, pdata->max_x,
			pdata->y_min, pdata->max_y);

	if (of_property_read_u32_array(np, "focaltech,area-size", px_zone, 3)) {
		pdata->area_indicator = 48;
		pdata->area_navigation = 96;
		pdata->area_edge = 60;
	} else {
		pdata->area_indicator = px_zone[0];
		pdata->area_navigation = px_zone[1];
		pdata->area_edge = px_zone[2];
	}
	FTS_INFO("zone's size - indicator:%d, navigation:%d, edge:%d",
		pdata->area_indicator, pdata->area_navigation, pdata->area_edge);

	return 0;
}

static int fts_parse_dt(struct device *dev, struct sec_ts_plat_data *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	u32 temp_val = 0;
	int lcdtype = 0;

	FTS_FUNC_ENTER();

#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	lcdtype = get_lcd_attached("GET");
	if (lcdtype == 0xFFFFFF) {
		FTS_ERROR("lcd is not attached");
		//return -ENODEV;
	}
#endif
	FTS_INFO("lcdtype=%X", lcdtype);

	ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (ret < 0)
		FTS_ERROR("Unable to get display-coords");

	// key
	have_key = of_property_read_bool(np, "focaltech,have-key");
	if (have_key) {
		ret = of_property_read_u32(np, "focaltech,key-number", &key_number);
		if (ret < 0)
			FTS_ERROR("Key number undefined!");

		ret = of_property_read_u32_array(np, "focaltech,keys",
		                                 keys, key_number);
		if (ret < 0)
			FTS_ERROR("Keys undefined!");
		else if (key_number > FTS_MAX_KEYS)
			key_number = FTS_MAX_KEYS;

		ret = of_property_read_u32_array(np, "focaltech,key-x-coords",
		                                 key_x_coords,
		                                 key_number);
		if (ret < 0)
			FTS_ERROR("Key Y Coords undefined!");

		ret = of_property_read_u32_array(np, "focaltech,key-y-coords",
		                                 key_y_coords,
		                                 key_number);
		if (ret < 0)
			FTS_ERROR("Key X Coords undefined!");

		FTS_INFO("VK Number:%d, key:(%d,%d,%d), "
		         "coords:(%d,%d),(%d,%d),(%d,%d)",
		         key_number,
		         keys[0], keys[1], keys[2],
		         key_x_coords[0], key_y_coords[0],
		         key_x_coords[1], key_y_coords[1],
		         key_x_coords[2], key_y_coords[2]);
	}

	// reset, irq gpio info
	pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio",
	                  0, &pdata->irq_flag);
	if (pdata->irq_gpio < 0)
		FTS_ERROR("Unable to get irq_gpio");

	ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
	if (ret < 0) {
		FTS_ERROR("Unable to get max-touch-number, please check dts");
		pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
	} else {
		if (temp_val < 2)
			pdata->max_touch_number = 2; // max_touch_number must >= 2
		else if (temp_val > FTS_MAX_POINTS_SUPPORT)
			pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
		else
			pdata->max_touch_number = temp_val;
	}

	of_property_read_string(np, "sec,firmware_name", &pdata->firmware_name);

	FTS_INFO("max touch number:%d, irq gpio:%d",
	         pdata->max_touch_number, pdata->irq_gpio);
	pdata->enable_settings_aot = of_property_read_bool(np, "enable_settings_aot");
	pdata->enable_sysinput_enabled = of_property_read_bool(np, "enable_sysinput_enabled");
	pdata->support_dex = of_property_read_bool(np, "support_dex");
	pdata->enable_vbus_notifier = of_property_read_bool(np, "enable_vbus_notifier");

	FTS_FUNC_EXIT();
	return 0;
}
*/
static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
	                              resume_work);

	fts_ts_resume(ts_data);
}

static void fts_print_info(struct fts_ts_data *ts)
{
	ts->print_info_cnt_open++;

	if (ts->print_info_cnt_open > 0xfff0)
		ts->print_info_cnt_open = 0;

	if (ts->touchs == 0)
		ts->print_info_cnt_release++;

	FTS_INFO("tc:%ld lp:0x%02X pm:0x%02X gm:0x%02X // v:FT%02X%02X%02X%02X // #%d %d",
		FTS_TOUCH_COUNT(&ts->touchs), ts->pdata->lowpower_mode, ts->power_mode, ts->gesture_mode,
		ts->ic_fw_ver.ic_name, ts->ic_fw_ver.project_name,
		ts->ic_fw_ver.module_id, ts->ic_fw_ver.fw_ver,
		ts->print_info_cnt_open, ts->print_info_cnt_release);
}

static void fts_print_info_work(struct work_struct *work)
{
	struct fts_ts_data *ts = container_of(work, struct fts_ts_data,
			print_info_work.work);

	fts_print_info(ts);

	schedule_delayed_work(&ts->print_info_work, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

static void fts_read_info_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
			read_info_work.work);

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	fts_run_rawdata_all(&ts_data->sec);
#endif

	if (!atomic_read(&ts_data->pdata->shutdown_called))
		schedule_work(&ts_data->print_info_work.work);
}

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
int fts_charger_attached(struct fts_ts_data *ts_data, bool status)
{
	int ret = 0;

	FTS_INFO("%s [START] %s", __func__, status ? "connected" : "disconnected");

	if (status == 1) {
		FTS_DEBUG("enter charger mode");
		ret = fts_write_reg(FTS_REG_CHARGER_MODE_EN, ENABLE);
		if (ret >= 0)
			ts_data->charger_mode = ENABLE;
	} else if (status == 0) {
		FTS_DEBUG("exit charger mode");
		ret = fts_write_reg(FTS_REG_CHARGER_MODE_EN, DISABLE);
		if (ret >= 0)
			ts_data->charger_mode = DISABLE;
	} else
		FTS_ERROR("%s [ERROR] Unknown value[%d]", __func__, status);

	FTS_INFO("%s [DONE]", __func__);
	return ret;
}

int fts_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	struct fts_ts_data *ts_data = container_of(nb, struct fts_ts_data, vbus_nb);
	vbus_status_t vbus_type = *(vbus_status_t *)data;

	FTS_INFO("%s cmd=%lu, vbus_type=%d", __func__, cmd, vbus_type);

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		FTS_INFO("%s : attach", __func__);
		ts_data->ta_status = true;
		break;
	case STATUS_VBUS_LOW:
		FTS_INFO("%s : detach", __func__);
		ts_data->ta_status = false;
		break;
	default:
		break;
	}

	fts_charger_attached(ts_data, ts_data->ta_status);
	return 0;
}
#endif

int fts_ts_enable(struct device *dev)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	FTS_INFO("called");
	atomic_set(&ts_data->pdata->enabled, true);
	ts_data->pdata->prox_power_off = 0;
	fts_ts_resume(ts_data);

	cancel_delayed_work(&ts_data->print_info_work);
	ts_data->pdata->print_info_cnt_open = 0;
	ts_data->pdata->print_info_cnt_release = 0;
	if (!atomic_read(&ts_data->pdata->shutdown_called))
		schedule_work(&ts_data->print_info_work.work);

	return 0;
}

int fts_ts_disable(struct device *dev)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	cancel_delayed_work(&ts_data->read_info_work);

	if (atomic_read(&ts_data->pdata->shutdown_called)) {
		FTS_INFO("shutdown was called");
		return 0;
	}

	FTS_INFO("called");
	atomic_set(&ts_data->pdata->enabled, false);

	cancel_delayed_work(&ts_data->print_info_work);
	sec_input_print_info(ts_data->dev, NULL);

	fts_ts_suspend(ts_data);
	return 0;
}

static int fts_ts_probe_entry(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int pdata_size = sizeof(struct sec_ts_plat_data);

	FTS_FUNC_ENTER();
	FTS_INFO("%s", FTS_DRIVER_VERSION);
	ts_data->pdata = devm_kzalloc(ts_data->dev, pdata_size, GFP_KERNEL);
	if (!ts_data->pdata) {
		FTS_ERROR("allocate memory for platform_data fail");
		return -ENOMEM;
	}

	ts_data->dev->platform_data = ts_data->pdata;

	if (ts_data->dev->of_node) {
		ret = sec_input_parse_dt(ts_data->dev);
		if (ret) {
			FTS_ERROR("device-tree parse fail");
			goto err_parse_dt;
		}
	} else {
		if (ts_data->dev->platform_data) {
			memcpy(ts_data->pdata, ts_data->dev->platform_data, pdata_size);
		} else {
			FTS_ERROR("platform_data is null");
			goto err_parse_dt;
		}
	}

	ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
	if (!ts_data->ts_workqueue) {
		FTS_ERROR("create fts workqueue fail");
		ret = -ENOMEM;
		goto err_workqueue;
	}

	mutex_init(&ts_data->report_mutex);
	mutex_init(&ts_data->bus_lock);
	mutex_init(&ts_data->device_lock);
	mutex_init(&ts_data->irq_lock);

	ret = fts_bus_init(ts_data);
	if (ret) {
		FTS_ERROR("bus initialize fail");
		goto err_bus_init;
	}

	ret = sec_input_device_register(ts_data->dev, ts_data);
	if (ret) {
		FTS_ERROR("failed to register input device, %d", ret);
		goto err_input_register;
	}

	ts_data->input_dev = ts_data->pdata->input_dev;
	ts_data->input_dev_proximity = ts_data->pdata->input_dev_proximity;

	mutex_init(&ts_data->pdata->enable_mutex);
	ts_data->pdata->pinctrl = devm_pinctrl_get(ts_data->dev);
	if (IS_ERR(ts_data->pdata->pinctrl))
		FTS_ERROR("could not get pinctrl");

	ts_data->pdata->pinctrl_configure = sec_input_pinctrl_configure;
	ts_data->pdata->power = fts_power_ctrl;

	ret = fts_report_buffer_init(ts_data);
	if (ret) {
		FTS_ERROR("report buffer init fail");
		goto err_report_buffer;
	}

#if FTS_POWER_SOURCE_CUST_EN
	ret = fts_power_source_init(ts_data);
#else
	ret = fts_ts_power_on(ts_data);
#endif
	if (ret) {
		FTS_ERROR("fail to get power(regulator)");
		goto err_power_init;
	}

	ret = fts_get_ic_information(ts_data);
	if (ret) {
		FTS_ERROR("not focal IC, unregister driver");
		goto err_get_ic_info;
	}

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	ret = fts_create_apk_debug_channel(ts_data);
	if (ret) {
		FTS_ERROR("create apk debug node fail");
	}

	ret = fts_create_sysfs(ts_data);
	if (ret) {
		FTS_ERROR("create sysfs node fail");
	}
#endif

#if FTS_POINT_REPORT_CHECK_EN
	ret = fts_point_report_check_init(ts_data);
	if (ret) {
		FTS_ERROR("init point report check fail");
	}
#endif

	ret = fts_ex_mode_init(ts_data);
	if (ret) {
		FTS_ERROR("init glove/cover/charger fail");
	}

	ret = fts_gesture_init(ts_data);
	if (ret) {
		FTS_ERROR("init gesture fail");
	}

#if FTS_ESDCHECK_EN
	ret = fts_esdcheck_init(ts_data);
	if (ret) {
		FTS_ERROR("init esd check fail");
	}
#endif

	ret = fts_irq_registration(ts_data);
	if (ret) {
		FTS_ERROR("request irq failed");
		goto err_irq_req;
	}

	ret = fts_fwupg_init(ts_data);
	if (ret) {
		FTS_ERROR("init fw upgrade fail");
		goto err_fwupg_init;
	}

	atomic_set(&ts_data->pdata->enabled, true);

	ts_data->pdata->enable = fts_ts_enable;
	ts_data->pdata->disable = fts_ts_disable;

	ret = fts_sec_cmd_init(ts_data);
	if (ret) {
		FTS_ERROR("init sec_cmd fail");
		goto err_cmd_init;
	}

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&ts_data->vbus_nb, fts_vbus_notification, VBUS_NOTIFY_DEV_CHARGER);
#endif

	if (ts_data->ts_workqueue) {
		INIT_WORK(&ts_data->resume_work, fts_resume_work);
	}

#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	init_completion(&ts_data->pm_completion);
	complete_all(&ts_data->pm_completion);
	fts_wakeup_source_init(ts_data);
#endif

	INIT_DELAYED_WORK(&ts_data->print_info_work, fts_print_info_work);
	INIT_DELAYED_WORK(&ts_data->read_info_work, fts_read_info_work);
	if (!atomic_read(&ts_data->pdata->shutdown_called))
		schedule_delayed_work(&ts_data->read_info_work, msecs_to_jiffies(50));

	FTS_FUNC_EXIT();
	return 0;

err_cmd_init:
	ts_data->pdata->enable = NULL;
	ts_data->pdata->disable = NULL;
err_fwupg_init:
	fts_irq_disable();
	free_irq(ts_data->irq, ts_data);
err_irq_req:
#if FTS_POWER_SOURCE_CUST_EN
	fts_power_source_exit(ts_data);
#endif
	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);
err_get_ic_info:
#if !FTS_POWER_SOURCE_CUST_EN
	fts_ts_power_off(ts_data);
#endif
err_power_init:
err_report_buffer:
	if (ts_data->pdata && ts_data->pdata->input_dev) {
		input_unregister_device(ts_data->pdata->input_dev);
		ts_data->pdata->input_dev = NULL;
	}
err_input_register:
	fts_bus_exit(ts_data);
err_bus_init:
	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);
	mutex_destroy(&ts_data->report_mutex);
	mutex_destroy(&ts_data->bus_lock);
	mutex_destroy(&ts_data->device_lock);
	mutex_destroy(&ts_data->irq_lock);
err_workqueue:
err_parse_dt:
	devm_kfree(ts_data->dev, ts_data->pdata);
	FTS_FUNC_EXIT();
	return ret;
}

static int fts_ts_remove_entry(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();

	// Disable and set shutdown state
	ts_data->pdata->enable = NULL;
	ts_data->pdata->disable = NULL;
	atomic_set(&ts_data->pdata->shutdown_called, true);

	// Disable IRQ and cancel delayed work
	disable_irq(ts_data->irq);
	cancel_delayed_work_sync(&ts_data->print_info_work);
	cancel_delayed_work_sync(&ts_data->read_info_work);

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_unregister(&ts_data->vbus_nb);
#endif

#if FTS_POINT_REPORT_CHECK_EN
	fts_point_report_check_exit(ts_data);
#endif

	fts_sec_cmd_exit(ts_data);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	fts_release_apk_debug_channel(ts_data);
	fts_remove_sysfs(ts_data);
#endif

	fts_ex_mode_exit(ts_data);
	fts_fwupg_exit(ts_data);

#if FTS_ESDCHECK_EN
	fts_esdcheck_exit(ts_data);
#endif

	fts_gesture_exit(ts_data);
	fts_bus_exit(ts_data);

	fts_wakeup_source_unregister(ts_data);

	// Free IRQ
	free_irq(ts_data->irq, ts_data);

	// Destroy workqueue if it exists
	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);

	// Free IRQ GPIO if valid
	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);

#if FTS_POWER_SOURCE_CUST_EN
	fts_power_source_exit(ts_data);
#endif

	// Destroy mutexes
	mutex_destroy(&ts_data->report_mutex);
	mutex_destroy(&ts_data->bus_lock);
	mutex_destroy(&ts_data->device_lock);
	mutex_destroy(&ts_data->irq_lock);

	FTS_FUNC_EXIT();
	fts_data = NULL;

	return 0;
}

int fts_ts_suspend(struct fts_ts_data *ts_data)
{
	int ret = 0;

	mutex_lock(&ts_data->device_lock);
	FTS_INFO("Enter. gesture_mode=0x%02X, prox_power_off=%d, lp:0x%02X pm:0x%02X ed:%d pocket_mode:%d fod_lp_mode:%d",
			ts_data->gesture_mode, ts_data->pdata->prox_power_off,
			ts_data->pdata->lowpower_mode, ts_data->power_mode, ts_data->pdata->ed_enable,
			ts_data->pdata->pocket_mode, ts_data->pdata->fod_lp_mode);

	if (ts_data->suspended) {
		FTS_INFO("Already in suspend state");
		mutex_unlock(&ts_data->device_lock);
		return 0;
	}

	if (ts_data->fw_loading) {
		FTS_INFO("fw upgrade in process, can't suspend");
		mutex_unlock(&ts_data->device_lock);
		return 0;
	}

#if FTS_ESDCHECK_EN
	fts_esdcheck_suspend();
#endif

	if (ts_data->pdata->prox_power_off == 1 && ts_data->aot_enable) {
		ts_data->gesture_mode &= ~GESTURE_DOUBLECLICK_EN;
		FTS_INFO("disable aot when prox_power_off");
	}

	if (ts_data->gesture_mode || ts_data->power_mode || !sec_input_need_ic_off(ts_data->pdata)) {
		fts_gesture_suspend(ts_data);

		ret = fts_write_reg(FTS_REG_HOST_POWER_MODE, 0x01);
		if (ret < 0) {
			FTS_ERROR("set power mode fail");
		} else {
			u8 val = 0;
			ret = fts_read_reg(FTS_REG_HOST_POWER_MODE_STATUS, &val);
			if (ret < 0)
				FTS_ERROR("read power mode status fail");
			else
				FTS_INFO("mode:0x%02X pm:0x%02X, AOT:%d, AOD:%d, ST:%d, SPAY:%d, ED:%d, Pocket:%d, FOD:%d",
						val, ts_data->power_mode,
						(val & FTS_POWER_MODE_DOUBLETAP_TO_WAKEUP) >> 7, (val & FTS_POWER_MODE_AOD) >> 6,
						(val & FTS_POWER_MODE_SINGLE_TAP) >> 5, (val & FTS_POWER_MODE_SWIPE) >> 4,
						(val & FTS_POWER_MODE_EAR_DETECT) >> 3, (val & FTS_POWER_MODE_POCKET) >> 2,
						(val & FTS_POWER_MODE_FOD) >> 1);
		}

		atomic_set(&ts_data->pdata->power_state, SEC_INPUT_STATE_LPM);
	} else {
		fts_irq_disable();
#if 0
		FTS_INFO("make TP enter into sleep mode");
		ret = fts_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
		if (ret < 0)
			FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);
#endif

		if (!ts_data->ic_info.is_incell) {
#if FTS_POWER_SOURCE_CUST_EN
			ret = fts_power_source_suspend(ts_data);
#else
			ret = fts_ts_power_off(ts_data);
#endif
			if (ret < 0) {
				FTS_ERROR("power enter suspend fail");
			}
		}
	}

	fts_release_all_finger();
	ts_data->suspended = true;

	cancel_delayed_work(&ts_data->print_info_work);
	fts_print_info(ts_data);

	FTS_FUNC_EXIT();
	mutex_unlock(&ts_data->device_lock);
	return 0;
}

int fts_ts_resume(struct fts_ts_data *ts_data)
{
	ktime_t start_time = ktime_get();
	s64 time_diff_ms;
	int ret = 0;

	mutex_lock(&ts_data->device_lock);
	FTS_FUNC_ENTER();
	if (!ts_data->suspended) {
		FTS_DEBUG("Already in awake state");
		mutex_unlock(&ts_data->device_lock);
		return 0;
	}

	fts_release_all_finger();

	if (!ts_data->ic_info.is_incell &&
		(atomic_read(&ts_data->pdata->power_state) == SEC_INPUT_STATE_POWER_OFF)) {
#if FTS_POWER_SOURCE_CUST_EN
		fts_power_source_resume(ts_data);
#else
		fts_ts_power_on(ts_data);
#endif
		fts_reset_proc(100);

		fts_wait_tp_to_valid();
		fts_ex_mode_recovery(ts_data);
	}

#if FTS_ESDCHECK_EN
	fts_esdcheck_resume();
#endif

	if (ts_data->gesture_mode ||
		(atomic_read(&ts_data->pdata->power_state) == SEC_INPUT_STATE_LPM)) {
		fts_gesture_resume(ts_data);

		ret = fts_write_reg(FTS_REG_HOST_POWER_MODE, 0x00);
		if (ret < 0) {
			FTS_ERROR("set power mode fail");
		}
	} else {
		fts_irq_enable();
	}

	atomic_set(&ts_data->pdata->power_state, SEC_INPUT_STATE_POWER_ON);

	if (ts_data->aot_enable)
		ts_data->gesture_mode |= GESTURE_DOUBLECLICK_EN;
	ts_data->pdata->prox_power_off = 0;

	ts_data->suspended = false;
	
	time_diff_ms = ktime_ms_delta(ktime_get(), start_time);
	FTS_INFO("Exit(%d). spent %lldms", __LINE__, time_diff_ms);
	mutex_unlock(&ts_data->device_lock);
	return 0;
}

#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
static int fts_pm_suspend(struct device *dev)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	//FTS_INFO("system enters into pm_suspend");
	reinit_completion(&ts_data->pm_completion);
	return 0;
}

static int fts_pm_resume(struct device *dev)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	//FTS_INFO("system resumes from pm_suspend");
	complete_all(&ts_data->pm_completion);
	return 0;
}

static const struct dev_pm_ops fts_dev_pm_ops = {
	.suspend = fts_pm_suspend,
	.resume = fts_pm_resume,
};
#endif

/*****************************************************************************
* TP Driver
*****************************************************************************/

static int fts_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct fts_ts_data *ts_data = NULL;

	FTS_INFO("Touch Screen(I2C BUS) driver probe...");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		FTS_ERROR("I2C not supported");
		return -ENODEV;
	}

	/* malloc memory for global struct variable */
	ts_data = devm_kzalloc(&client->dev, sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		FTS_ERROR("allocate memory for fts_data fail");
		return -ENOMEM;
	}

	fts_data = ts_data;
	ts_data->client = client;
	ts_data->dev = &client->dev;
	ts_data->log_level = 1;
	ts_data->fw_is_running = 0;
	ts_data->bus_type = BUS_TYPE_I2C;
	i2c_set_clientdata(client, ts_data);

	ret = fts_ts_probe_entry(ts_data);
	if (ret) {
		FTS_ERROR("Touch Screen(I2C BUS) driver probe fail");
		devm_kfree(&client->dev, ts_data);
		fts_data = NULL;
		return ret;
	}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	ptsp = &client->dev;
	ts_data->pdata->stui_tsp_enter = fts_stui_tsp_enter;
	ts_data->pdata->stui_tsp_exit = fts_stui_tsp_exit;
	ts_data->pdata->stui_tsp_type = fts_stui_tsp_type;
#endif

	FTS_INFO("Touch Screen(I2C BUS) driver probe successfully");
	return 0;
}

static int fts_ts_dev_remove(struct i2c_client *client)
{
	return fts_ts_remove_entry(i2c_get_clientdata(client));
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void fts_ts_remove(struct i2c_client *client)
{
	fts_ts_dev_remove(client);
}
#else
static int fts_ts_remove(struct i2c_client *client)
{
	fts_ts_dev_remove(client);
	return 0;
}
#endif

static void fts_ts_shutdown(struct i2c_client *client)
{
	fts_ts_remove_entry(i2c_get_clientdata(client));
}

static const struct i2c_device_id fts_ts_id[] = {
	{FTS_DRIVER_NAME, 0},
	{},
};
static const struct of_device_id fts_dt_match[] = {
	{.compatible = "focaltech,fts", },
	{},
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct i2c_driver fts_ts_driver = {
	.probe = fts_ts_probe,
	.remove = fts_ts_remove,
	.shutdown = fts_ts_shutdown,
	.driver = {
		.name = FTS_DRIVER_NAME,
		.owner = THIS_MODULE,
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
		.pm = &fts_dev_pm_ops,
#endif
		.of_match_table = of_match_ptr(fts_dt_match),
	},
	.id_table = fts_ts_id,
};

static int __init fts_ts_init(void)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ret = i2c_add_driver(&fts_ts_driver);
	if ( ret != 0 ) {
		FTS_ERROR("Focaltech touch screen driver init failed!");
	}
	FTS_FUNC_EXIT();
	input_log_fix();
	return ret;
}

static void __exit fts_ts_exit(void)
{
	i2c_del_driver(&fts_ts_driver);
}
module_init(fts_ts_init);
module_exit(fts_ts_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
