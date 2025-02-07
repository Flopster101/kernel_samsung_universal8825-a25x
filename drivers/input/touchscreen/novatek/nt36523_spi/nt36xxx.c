/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 61426 $
 * $Date: 2020-04-29 15:04:36 +0800 $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include "nt36xxx.h"
#if NVT_TOUCH_ESD_PROTECT
#include <linux/jiffies.h>
#endif /* #if NVT_TOUCH_ESD_PROTECT */

int nvt_ts_sec_fn_init(struct nvt_ts_data *ts);
void nvt_ts_sec_fn_remove(struct nvt_ts_data *ts);

#if NVT_TOUCH_ESD_PROTECT
static struct delayed_work nvt_esd_check_work;
static struct workqueue_struct *nvt_esd_check_wq;
static unsigned long irq_timer = 0;
uint8_t esd_check = false;
uint8_t esd_retry = 0;
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if NVT_TOUCH_EXT_PROC
extern int32_t nvt_extra_proc_init(void);
extern void nvt_extra_proc_deinit(void);
#endif



struct nvt_ts_data *ts;

uint32_t ENG_RST_ADDR  = 0x7FFF80;
uint32_t SWRST_N8_ADDR = 0; //read from dtsi
uint32_t SPI_RD_FAST_ADDR = 0;	//read from dtsi

#if TOUCH_KEY_NUM > 0
const uint16_t touch_key_array[TOUCH_KEY_NUM] = {
	KEY_BACK,
	KEY_HOME,
	KEY_MENU
};
#endif

#if WAKEUP_GESTURE
const uint16_t gesture_key_array[] = {
	KEY_WAKEUP,  //GESTURE_WORD_C
	KEY_WAKEUP,  //GESTURE_WORD_W
	KEY_WAKEUP,  //GESTURE_WORD_V
	KEY_WAKEUP,  //GESTURE_DOUBLE_CLICK
	KEY_WAKEUP,  //GESTURE_WORD_Z
	KEY_WAKEUP,  //GESTURE_WORD_M
	KEY_WAKEUP,  //GESTURE_WORD_O
	KEY_WAKEUP,  //GESTURE_WORD_e
	KEY_WAKEUP,  //GESTURE_WORD_S
	KEY_BLACK_UI_GESTURE,  //GESTURE_SLIDE_UP
	KEY_WAKEUP,  //GESTURE_SLIDE_DOWN
	KEY_WAKEUP,  //GESTURE_SLIDE_LEFT
	KEY_WAKEUP,  //GESTURE_SLIDE_RIGHT
};

#define GESTURE_WORD_C          12
#define GESTURE_WORD_W          13
#define GESTURE_WORD_V          14
#define GESTURE_DOUBLE_CLICK    15
#define GESTURE_WORD_Z          16
#define GESTURE_WORD_M          17
#define GESTURE_WORD_O          18
#define GESTURE_WORD_e          19
#define GESTURE_WORD_S          20
#define GESTURE_SLIDE_UP        21
#define GESTURE_SLIDE_DOWN      22
#define GESTURE_SLIDE_LEFT      23
#define GESTURE_SLIDE_RIGHT     24
#endif

#if IS_ENABLED(CONFIG_MTK_SPI)
const struct mt_chip_conf spi_ctrdata = {
	.setuptime = 25,
	.holdtime = 25,
	.high_time = 5,	/* 10MHz (SPI_SPEED=100M / (high_time+low_time(10ns)))*/
	.low_time = 5,
	.cs_idletime = 2,
	.ulthgh_thrsh = 0,
	.cpol = 0,
	.cpha = 0,
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.tx_endian = 0,
	.rx_endian = 0,
	.com_mod = DMA_TRANSFER,
	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif

#if IS_ENABLED(CONFIG_SPI_MT65XX)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
const struct mtk_chip_config spi_ctrdata = {
	.sample_sel = 0,

	.cs_setuptime = 25,
	.cs_holdtime = 0,
	.cs_idletime = 0,
	.tick_delay = 0,
};
#else
const struct mtk_chip_config spi_ctrdata = {
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.sample_sel = 0,

	.cs_setuptime = 25,
	.cs_holdtime = 0,
	.cs_idletime = 0,
	.deassert_mode = false,
	.tick_delay = 0,
};
#endif
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
static irqreturn_t nvt_ts_work_func(int irq, void *data);
irqreturn_t secure_filter_interrupt(struct nvt_ts_data *ts)
{
	mutex_lock(&ts->secure_lock);
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		if (atomic_cmpxchg(&ts->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&ts->input_dev->dev.kobj, NULL, "secure_touch");

		} else {
			input_info(true, &ts->client->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&ts->secure_pending_irqs));
		}

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
	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->secure_enabled));
}

ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *addr, const char *buf, size_t count)
{
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
		nvt_ts_release_all_finger(ts);

		if (pm_runtime_get_sync(ts->client->controller->dev.parent) < 0) {
			enable_irq(ts->client->irq);
			input_err(true, &ts->client->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

		reinit_completion(&ts->secure_powerdown);
		reinit_completion(&ts->secure_interrupt);

		atomic_set(&ts->secure_enabled, 1);
		atomic_set(&ts->secure_pending_irqs, 0);

		enable_irq(ts->client->irq);

		input_info(true, &ts->client->dev, "%s: secure touch enable\n", __func__);

	} else if (data == 0) {
		/* Disable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
			input_err(true, &ts->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_delay(200);

		pm_runtime_put_sync(ts->client->controller->dev.parent);
		atomic_set(&ts->secure_enabled, 0);

		sysfs_notify(&ts->input_dev->dev.kobj, NULL, "secure_touch");

		sec_delay(10);

		nvt_ts_work_func(ts->client->irq, ts);
		complete(&ts->secure_interrupt);
		complete_all(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: secure touch disable\n", __func__);

	} else {
		input_err(true, &ts->client->dev, "%s: unsupport value:%ld\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
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

int secure_touch_init(struct nvt_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	init_completion(&ts->secure_interrupt);
	init_completion(&ts->secure_powerdown);

	return 0;
}

void secure_touch_stop(struct nvt_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->secure_enabled)) {
		atomic_set(&ts->secure_pending_irqs, -1);

		sysfs_notify(&ts->input_dev->dev.kobj, NULL, "secure_touch");

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

/*******************************************************
Description:
	Novatek touchscreen irq enable/disable function.

return:
	n.a.
*******************************************************/
void nvt_irq_enable(bool enable)
{
	struct irq_desc *desc;

	if (enable) {
		if (!ts->irq_enabled) {
			enable_irq(ts->client->irq);
			ts->irq_enabled = true;
		}
	} else {
		if (ts->irq_enabled) {
			disable_irq(ts->client->irq);
			ts->irq_enabled = false;
		}
	}

	desc = irq_to_desc(ts->client->irq);
	input_info(true, &ts->client->dev,"enable=%d, desc->depth=%d\n", enable, desc->depth);
}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_spi_lock(struct spi_master *spi);
extern int stui_spi_unlock(struct spi_master *spi);

static int nvt_stui_tsp_enter(void)
{
	int ret = 0;
	input_info(true, &ts->client->dev, ">> %s\n", __func__);

	nvt_irq_enable(false);

	ret = stui_spi_lock(ts->client->master);
	if (ret < 0) {
		pr_err("[STUI] stui_spi_lock failed : %d\n", ret);
		nvt_irq_enable(true);
		return -1;
	}

	return 0;
}

static int nvt_stui_tsp_exit(void)
{
	int ret = 0;
	input_info(true, &ts->client->dev, ">> %s\n", __func__);

	ret = stui_spi_unlock(ts->client->master);
	if (ret < 0)
		pr_err("[STUI] stui_spi_unlock failed : %d\n", ret);

	nvt_irq_enable(true);

	return ret;
}

static int nvt_stui_tsp_type(void)
{
	input_info(true, &ts->client->dev, ">> %s\n", __func__);

	return STUI_TSP_TYPE_NOVATEK;
}
#endif

/*******************************************************
Description:
	Novatek touchscreen spi read/write core function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static inline int32_t spi_read_write(struct spi_device *client, uint8_t *buf, size_t len , NVT_SPI_RW rw)
{
	struct spi_message m;
	struct spi_transfer t = {
		.len    = len,
	};
	int ret;

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev, "%s: secure touch is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		return -EIO;
	}

	memset(ts->xbuf, 0, len + DUMMY_BYTES);
	memcpy(ts->xbuf, buf, len);

	switch (rw) {
		case NVTREAD:
			t.tx_buf = ts->xbuf;
			t.rx_buf = ts->rbuf;
			t.len    = (len + DUMMY_BYTES);
			break;

		case NVTWRITE:
			t.tx_buf = ts->xbuf;
			break;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	if (ts->platdata->cs_gpio > 0)
		gpio_direction_output(ts->platdata->cs_gpio, 0);

	ret = spi_sync(client, &m);

	if (ts->platdata->cs_gpio > 0)
		gpio_direction_output(ts->platdata->cs_gpio, 1);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen spi read function.

return:
	Executive outcomes. 2---succeed. -5---I/O error
*******************************************************/
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return -EIO;
	}

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		return -EIO;
	}

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_READ_MASK(buf[0]);

	while (retries < NVT_SPI_RETRY_COUNT) {
		ret = spi_read_write(client, buf, len, NVTREAD);
		if (ret == 0) break;
		retries++;

		if (ts->shutdown_called) {
			input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}

		if (ts->power_status == POWER_OFF_STATUS) {
			input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}
		
		if (!nvt_ts_lcd_power_check()) {
			input_err(true, &ts->client->dev, "%s: lcd is off\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}
	}

	if (unlikely(retries == NVT_SPI_RETRY_COUNT)) {
		input_err(true, &client->dev,"read error, ret=%d\n", ret);
		ret = -EIO;
	} else {
		memcpy((buf+1), (ts->rbuf+2), (len-1));
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen spi write function.

return:
	Executive outcomes. 1---succeed. -5---I/O error
*******************************************************/
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return -EIO;
	}

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		return -EIO;
	}

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_WRITE_MASK(buf[0]);

	while (retries < NVT_SPI_RETRY_COUNT) {
		ret = spi_read_write(client, buf, len, NVTWRITE);
		if (ret == 0)	break;
		retries++;

		if (ts->shutdown_called) {
			input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}

		if (ts->power_status == POWER_OFF_STATUS) {
			input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}

		if (!nvt_ts_lcd_power_check()) {
			input_err(true, &ts->client->dev, "%s: lcd is off\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}
	}

	if (unlikely(retries == NVT_SPI_RETRY_COUNT)) {
		input_err(true, &client->dev,"write error, ret=%d\n", ret);
		ret = -EIO;
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen set index/page/addr address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_set_page(uint32_t addr)
{
	uint8_t buf[4] = {0};

	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;

	return CTP_SPI_WRITE(ts->client, buf, 3);
}

/*******************************************************
Description:
	Novatek touchscreen write data to specify address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_write_addr(uint32_t addr, uint8_t data)
{
	int32_t ret = 0;
	uint8_t buf[4] = {0};

	//---set xdata index---
	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret) {
		input_err(true, &ts->client->dev,"set page 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	//---write data to index---
	buf[0] = addr & (0x7F);
	buf[1] = data;
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret) {
		input_err(true, &ts->client->dev,"write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen enable hw bld crc function.

return:
	N/A.
*******************************************************/
void nvt_bld_crc_enable(void)
{
	uint8_t buf[4] = {0};

	//---set xdata index to BLD_CRC_EN_ADDR---
	nvt_set_page(ts->mmap->BLD_CRC_EN_ADDR);

	//---read data from index---
	buf[0] = ts->mmap->BLD_CRC_EN_ADDR & (0x7F);
	buf[1] = 0xFF;
	CTP_SPI_READ(ts->client, buf, 2);

	//---write data to index---
	buf[0] = ts->mmap->BLD_CRC_EN_ADDR & (0x7F);
	buf[1] = buf[1] | (0x01 << 7);
	CTP_SPI_WRITE(ts->client, buf, 2);
}

/*******************************************************
Description:
	Novatek touchscreen clear status & enable fw crc function.

return:
	N/A.
*******************************************************/
void nvt_fw_crc_enable(void)
{
	uint8_t buf[4] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);

	//---clear fw reset status---
	buf[0] = EVENT_MAP_RESET_COMPLETE & (0x7F);
	buf[1] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 2);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable fw crc---
	buf[0] = EVENT_MAP_HOST_CMD & (0x7F);
	buf[1] = 0xAE;	//enable fw crc command
	CTP_SPI_WRITE(ts->client, buf, 2);
}

/*******************************************************
Description:
	Novatek touchscreen set boot ready function.

return:
	N/A.
*******************************************************/
void nvt_boot_ready(void)
{
	//---write BOOT_RDY status cmds---
	nvt_write_addr(ts->mmap->BOOT_RDY_ADDR, 1);

	usleep_range(5000, 5001);

	if (!ts->hw_crc) {
		//---write BOOT_RDY status cmds---
		nvt_write_addr(ts->mmap->BOOT_RDY_ADDR, 0);

		//---write POR_CD cmds---
		nvt_write_addr(ts->mmap->POR_CD_ADDR, 0xA0);
	}
}

/*******************************************************
Description:
	Novatek touchscreen eng reset cmd
    function.

return:
	n.a.
*******************************************************/
void nvt_eng_reset(void)
{
	//---eng reset cmds to ENG_RST_ADDR---
	nvt_write_addr(ENG_RST_ADDR, 0x5A);

	usleep_range(1000, 1001);	//wait tMCU_Idle2TP_REX_Hi after TP_RST
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU
    function.

return:
	n.a.
*******************************************************/
void nvt_sw_reset(void)
{
	//---software reset cmds to SWRST_N8_ADDR---
	nvt_write_addr(SWRST_N8_ADDR, 0x55);

	msleep(10);
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU then into idle mode
    function.

return:
	n.a.
*******************************************************/
void nvt_sw_reset_idle(void)
{
	//---MCU idle cmds to SWRST_N8_ADDR---
	nvt_write_addr(SWRST_N8_ADDR, 0xAA);

	msleep(15);
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU (boot) function.

return:
	n.a.
*******************************************************/
void nvt_bootloader_reset(void)
{
	//---reset cmds to SWRST_N8_ADDR---
	nvt_write_addr(SWRST_N8_ADDR, 0x69);

	usleep_range(50000, 50001);	//wait tBRST2FR after Bootload RST

	if (SPI_RD_FAST_ADDR) {
		/* disable SPI_RD_FAST */
		nvt_write_addr(SPI_RD_FAST_ADDR, 0x00);
	}
}

/*******************************************************
Description:
	Novatek touchscreen clear FW status function.

return:
	Executive outcomes. 0---succeed. -1---fail.
*******************************************************/
int32_t nvt_clear_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_WRITE(ts->client, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev,"failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW status function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev,"failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW reset state function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;
	int32_t retry_max = (check_reset_state == RESET_STATE_INIT) ? 10 : 50;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);

	while (1) {
		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if(unlikely(retry > retry_max)) {
			input_err(true, &ts->client->dev,"error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
				retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}

		usleep_range(10000, 10000);
	}

	if (!ret)
		input_info(true, &ts->client->dev, "%s : retry=%d, buf[1] = %x\n", __func__, retry, buf[1]);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen get novatek project id information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_read_pid(void)
{
	uint8_t buf[4] = {0};
	int32_t ret = 0;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROJECTID);

	//---read project id---
	buf[0] = EVENT_MAP_PROJECTID;
	buf[1] = 0x00;
	buf[2] = 0x00;
	CTP_SPI_READ(ts->client, buf, 3);

	ts->nvt_pid = (buf[2] << 8) + buf[1];
	ts->fw_ver_ic[1] = buf[1];

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	input_info(true, &ts->client->dev,"PID=%04X\n", ts->nvt_pid);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen get firmware related information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_get_fw_info(void)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	CTP_SPI_READ(ts->client, buf, 17);
	ts->fw_ver = buf[1];
	ts->platdata->x_num = buf[3];
	ts->platdata->y_num = buf[4];
	ts->platdata->abs_x_max = (uint16_t)((buf[5] << 8) | buf[6]);
	ts->platdata->abs_y_max = (uint16_t)((buf[7] << 8) | buf[8]);
	ts->max_button_num = buf[11];

	input_info(true, &ts->client->dev, "%s : fw_ver=%d, x_num=%d, y_num=%d,"
			"abs_x_max=%d, abs_y_max=%d, max_button_num=%d, fw_type=0x%02X\n",
			__func__, ts->fw_ver, ts->platdata->x_num, ts->platdata->y_num,
			ts->platdata->abs_x_max, ts->platdata->abs_y_max, ts->max_button_num, buf[14]);

	//---clear x_num, y_num if fw info is broken---
	if ((buf[1] + buf[2]) != 0xFF) {
		input_err(true, &ts->client->dev,"FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		ts->fw_ver = 0;
		ts->platdata->x_num = 18;
		ts->platdata->y_num = 32;
		ts->platdata->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
		ts->platdata->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;
		ts->max_button_num = TOUCH_KEY_NUM;

		if(retry_count < 3) {
			retry_count++;
			input_err(true, &ts->client->dev,"retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			input_err(true, &ts->client->dev,"Set default fw_ver=%d, x_num=%d, y_num=%d, "
					"abs_x_max=%d, abs_y_max=%d, max_button_num=%d!\n",
					ts->fw_ver, ts->platdata->x_num, ts->platdata->y_num,
					ts->platdata->abs_x_max, ts->platdata->abs_y_max, ts->max_button_num);
			ret = -1;
		}
	} else {
		ret = 0;
	}

	ts->fw_ver_ic[0] = buf[15];

	//---Get Novatek PID---
	nvt_read_pid();

	//---get panel id---
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PANEL);
	buf[0] = EVENT_MAP_PANEL;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,"nvt_ts_i2c_read error(%d)\n", ret);
		return ret;
	}
	ts->fw_ver_ic[2] = buf[1];

	//---get firmware version---
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);
	buf[0] = EVENT_MAP_FWINFO;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,"nvt_ts_i2c_read error(%d)\n", ret);
		return ret;
	}
	ts->fw_ver_ic[3] = buf[1];

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	input_info(true, &ts->client->dev, "%s : fw_ver_ic = NO%02X%02X%02X%02X\n",
		__func__, ts->fw_ver_ic[0], ts->fw_ver_ic[1], ts->fw_ver_ic[2], ts->fw_ver_ic[3]);

	return ret;
}

/*******************************************************
  Create Device Node (Proc Entry)
*******************************************************/
#if NVT_TOUCH_PROC
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTSPI"

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI read function.

return:
	Executive outcomes. 2---succeed. -5,-14---failed.
*******************************************************/
static ssize_t nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	uint8_t *str = NULL;
	int32_t ret = 0;
	int32_t retries = 0;
	int8_t spi_wr = 0;
	uint8_t *buf;

	if ((count > NVT_TRANSFER_LEN + 3) || (count < 3)) {
		input_err(true, &ts->client->dev,"invalid transfer len!\n");
		return -EFAULT;
	}

	/* allocate buffer for spi transfer */
	str = (uint8_t *)kzalloc((count), GFP_KERNEL);
	if(str == NULL) {
		input_err(true, &ts->client->dev,"kzalloc for buf failed!\n");
		ret = -ENOMEM;
		goto kzalloc_failed;
	}

	buf = (uint8_t *)kzalloc((count), GFP_KERNEL | GFP_DMA);
	if(buf == NULL) {
		input_err(true, &ts->client->dev,"kzalloc for buf failed!\n");
		ret = -ENOMEM;
		kfree(str);
		str = NULL;
		goto kzalloc_failed;
	}

	if (copy_from_user(str, buff, count)) {
		input_err(true, &ts->client->dev,"copy from user error\n");
		ret = -EFAULT;
		goto out;
	}

#if NVT_TOUCH_ESD_PROTECT
	/*
	 * stop esd check work to avoid case that 0x77 report righ after here to enable esd check again
	 * finally lead to trigger esd recovery bootloader reset
	 */
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	spi_wr = str[0] >> 7;
	memcpy(buf, str+2, ((str[0] & 0x7F) << 8) | str[1]);

	if (spi_wr == NVTWRITE) {	//SPI write
		while (retries < 20) {
			ret = CTP_SPI_WRITE(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			else
				input_err(true, &ts->client->dev,"error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			input_err(true, &ts->client->dev,"error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else if (spi_wr == NVTREAD) {	//SPI read
		while (retries < 20) {
			ret = CTP_SPI_READ(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			else
				input_err(true, &ts->client->dev,"error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		memcpy(str+2, buf, ((str[0] & 0x7F) << 8) | str[1]);
		// copy buff to user if spi transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count)) {
				ret = -EFAULT;
				goto out;
			}
		}

		if (unlikely(retries == 20)) {
			input_err(true, &ts->client->dev,"error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else {
		input_err(true, &ts->client->dev,"Call error, str[0]=%d\n", str[0]);
		ret = -EFAULT;
		goto out;
	}

out:
	kfree(str);
	kfree(buf);
kzalloc_failed:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI open function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	dev = kzalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL) {
		pr_err("Failed to allocate memory for nvt flash data\n");
		return -ENOMEM;
	}

	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI close function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	if (dev)
		kfree(dev);

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops nvt_flash_fops = {
	.proc_open = nvt_flash_open,
	.proc_release = nvt_flash_close,
	.proc_read = nvt_flash_read,
};
#else
const struct file_operations nvt_flash_fops = {
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.read = nvt_flash_read,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_proc_init(void)
{
	NVT_proc_entry = proc_create(DEVICE_NAME, 0444, NULL,&nvt_flash_fops);
	if (NVT_proc_entry == NULL) {
		input_err(true, &ts->client->dev, "%s: Failed!\n", __func__);
		return -ENOMEM;
	} else {
		input_info(true, &ts->client->dev, "%s: Succeeded!\n", __func__);
	}

	input_info(true, &ts->client->dev, "%s: ============================================================\n", __func__);
	input_info(true, &ts->client->dev, "%s: Create /proc/%s\n", __func__, DEVICE_NAME);
	input_info(true, &ts->client->dev, "%s: ============================================================\n", __func__);

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI deinitial function.

return:
	n.a.
*******************************************************/
static void nvt_flash_proc_deinit(void)
{
	if (NVT_proc_entry != NULL) {
		remove_proc_entry(DEVICE_NAME, NULL);
		NVT_proc_entry = NULL;
		input_info(true, &ts->client->dev, "%s: Removed /proc/%s\n", __func__, DEVICE_NAME);
	}
}
#endif

#if WAKEUP_GESTURE

/* customized gesture id */
#define DATA_PROTOCOL           30

/* function page definition */
#define FUNCPAGE_GESTURE	0x01
#if PROXIMITY_FUNCTION
#define FUNCPAGE_PROXIMITY	0x02
#endif

/*******************************************************
Description:
	Novatek touchscreen wake up gesture key report function.

return:
	n.a.
*******************************************************/
void nvt_ts_wakeup_gesture_report(uint8_t *data)
{
	uint32_t keycode = 0;
	uint8_t func_id = data[3];

//	input_info(true, &ts->client->dev, "gesture_id = %d\n", func_id);

	switch (func_id) {
		case GESTURE_WORD_C:
			input_info(true, &ts->client->dev,"Gesture : Word-C.\n");
			keycode = gesture_key_array[0];
			break;
		case GESTURE_WORD_W:
			input_info(true, &ts->client->dev,"Gesture : Word-W.\n");
			keycode = gesture_key_array[1];
			break;
		case GESTURE_WORD_V:
			input_info(true, &ts->client->dev,"Gesture : Word-V.\n");
			keycode = gesture_key_array[2];
			break;
		case GESTURE_DOUBLE_CLICK:
			input_info(true, &ts->client->dev,"Gesture : Double Click.\n");
			keycode = gesture_key_array[3];
			break;
		case GESTURE_WORD_Z:
			input_info(true, &ts->client->dev,"Gesture : Word-Z.\n");
			keycode = gesture_key_array[4];
			break;
		case GESTURE_WORD_M:
			input_info(true, &ts->client->dev,"Gesture : Word-M.\n");
			keycode = gesture_key_array[5];
			break;
		case GESTURE_WORD_O:
			input_info(true, &ts->client->dev,"Gesture : Word-O.\n");
			keycode = gesture_key_array[6];
			break;
		case GESTURE_WORD_e:
			input_info(true, &ts->client->dev,"Gesture : Word-e.\n");
			keycode = gesture_key_array[7];
			break;
		case GESTURE_WORD_S:
			input_info(true, &ts->client->dev,"Gesture : Word-S.\n");
			keycode = gesture_key_array[8];
			break;
		case GESTURE_SLIDE_UP:
			input_info(true, &ts->client->dev,"Gesture : Slide UP.\n");
			keycode = gesture_key_array[9];
			ts->scrub_id = SPONGE_EVENT_TYPE_SPAY;
			break;
		case GESTURE_SLIDE_DOWN:
			input_info(true, &ts->client->dev,"Gesture : Slide DOWN.\n");
			keycode = gesture_key_array[10];
			break;
		case GESTURE_SLIDE_LEFT:
			input_info(true, &ts->client->dev,"Gesture : Slide LEFT.\n");
			keycode = gesture_key_array[11];
			break;
		case GESTURE_SLIDE_RIGHT:
			input_info(true, &ts->client->dev,"Gesture : Slide RIGHT.\n");
			keycode = gesture_key_array[12];
			break;
		default:
			input_err(true, &ts->client->dev, "invalid gesture event (%02X %02X %02X %02X %02X %02X)",
				data[0], data[1], data[2],
				data[3], data[4], data[5]);
			return;
	}

	if (keycode > 0) {
		input_report_key(ts->input_dev, keycode, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, keycode, 0);
		input_sync(ts->input_dev);
	}
}
#endif

int pinctrl_configure(struct nvt_ts_data *ts, bool enable)
{
	struct pinctrl_state *state;
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, enable ? "ACTIVE" : "SUSPEND");

	if (enable) {
		state = pinctrl_lookup_state(ts->platdata->pinctrl, "on_state");
		if (IS_ERR(ts->platdata->pinctrl))
			input_info(true, &ts->client->dev, "%s: could not get active pinstate\n", __func__);
	} else {
		state = pinctrl_lookup_state(ts->platdata->pinctrl, "off_state");
		if (IS_ERR(ts->platdata->pinctrl))
			input_info(true, &ts->client->dev, "%s: could not get suspend pinstate\n", __func__);
	}

	if (!IS_ERR_OR_NULL(state)) {
		ret = pinctrl_select_state(ts->platdata->pinctrl, state);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: could not control pinstate\n", __func__);
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen parse device tree function.

return:
	n.a.
*******************************************************/
#if IS_ENABLED(CONFIG_OF)
static int nvt_parse_dt(struct device *dev)
{
	struct nvt_ts_platdata *platdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	int32_t ret = 0;
	int tmp[3];
	int lcd_id1_gpio = 0, lcd_id2_gpio = 0, lcd_id3_gpio = 0, dt_lcdtype;
	int fw_name_cnt;
	int lcdtype_cnt;
	int fw_sel_idx = 0;
	int lcdtype = 0;

	input_info(true, dev, "%s: start!\n", __func__);

	if (!np)
		return -ENODEV;

	lcdtype = sec_input_get_lcd_id(dev);
	if (lcdtype < 0) {
		input_err(true, dev, "lcd is not attached\n");
		return -ENODEV;
	}

	fw_name_cnt = of_property_count_strings(np, "novatek,fw_name");

	if (fw_name_cnt == 0) {
		input_err(true, dev, "%s: no fw_name in DT\n", __func__);
		return -EINVAL;

	} else if (fw_name_cnt == 1) {
		ret = of_property_read_u32(np, "novatek,lcdtype", &dt_lcdtype);
		if (ret < 0) {
			input_err(true, dev, "%s: failed to read novatek,lcdtype\n", __func__);

		} else {
			input_info(true, dev, "%s: fw_name_cnt(1), ap lcdtype=0x%06X & dt lcdtype=0x%06X\n",
								__func__, lcdtype, dt_lcdtype);
			if (lcdtype != dt_lcdtype) {
				input_err(true, dev, "%s: panel mismatched, unload driver\n", __func__);
				return -EINVAL;
			}
		}
	} else {

		lcd_id1_gpio = of_get_named_gpio(np, "novatek,lcdid1-gpio", 0);
		if (gpio_is_valid(lcd_id1_gpio))
			input_info(true, dev, "%s: lcd id1_gpio %d(%d)\n", __func__, lcd_id1_gpio, gpio_get_value(lcd_id1_gpio));
		else {
			input_err(true, dev, "%s: Failed to get novatek,lcdid1-gpio\n", __func__);
			return -EINVAL;
		}

		lcd_id2_gpio = of_get_named_gpio(np, "novatek,lcdid2-gpio", 0);
		if (gpio_is_valid(lcd_id2_gpio))
			input_info(true, dev, "%s: lcd id2_gpio %d(%d)\n", __func__, lcd_id2_gpio, gpio_get_value(lcd_id2_gpio));
		else {
			input_err(true, dev, "%s: Failed to get novatek,lcdid2-gpio\n", __func__);
			return -EINVAL;
		}

		/* support lcd id3 */
		lcd_id3_gpio = of_get_named_gpio(np, "novatek,lcdid3-gpio", 0);
		if (gpio_is_valid(lcd_id3_gpio)) {
			input_info(true, dev, "%s: lcd id3_gpio %d(%d)\n", __func__, lcd_id3_gpio, gpio_get_value(lcd_id3_gpio));
			fw_sel_idx = (gpio_get_value(lcd_id3_gpio) << 2) | (gpio_get_value(lcd_id2_gpio) << 1) | gpio_get_value(lcd_id1_gpio);

		} else {
			input_err(true, dev, "%s: Failed to get novatek,lcdid3-gpio and use #1 &#2 id\n", __func__);
			fw_sel_idx = (gpio_get_value(lcd_id2_gpio) << 1) | gpio_get_value(lcd_id1_gpio);
		}

		lcdtype_cnt = of_property_count_u32_elems(np, "novatek,lcdtype");
		input_info(true, dev, "%s: fw_name_cnt(%d) & lcdtype_cnt(%d) & fw_sel_idx(%d)\n",
					__func__, fw_name_cnt, lcdtype_cnt, fw_sel_idx);

		if (lcdtype_cnt <= 0 || fw_name_cnt <= 0 || lcdtype_cnt <= fw_sel_idx || fw_name_cnt <= fw_sel_idx) {
			input_err(true, dev, "%s: abnormal lcdtype & fw name count, fw_sel_idx(%d)\n", __func__, fw_sel_idx);
			return -EINVAL;
		}
		of_property_read_u32_index(np, "novatek,lcdtype", fw_sel_idx, &dt_lcdtype);
		input_info(true, dev, "%s: lcd id(%d), ap lcdtype=0x%06X & dt lcdtype=0x%06X\n",
						__func__, fw_sel_idx, lcdtype, dt_lcdtype);
	}

	if (of_property_read_u32_index(np, "novatek,resume_lp_delay", fw_sel_idx, &platdata->resume_lp_delay)) {
		input_err(true, dev, "%s: fail to get resume_lp_delay & set default 15 ms\n", __func__);
		platdata->resume_lp_delay = 15;
	}
	input_info(true, dev, "%s: resume_lp_delay(%d ms)\n", __func__, platdata->resume_lp_delay);

	of_property_read_string_index(np, "novatek,fw_name", fw_sel_idx, &platdata->firmware_name);
	if (platdata->firmware_name == NULL || strlen(platdata->firmware_name) == 0) {
		input_err(true, dev, "%s: Failed to get fw name\n", __func__);
		return -EINVAL;
	} else {
		input_info(true, dev, "%s: fw name(%s)\n", __func__, platdata->firmware_name);
	}

	of_property_read_string_index(np, "novatek,fw_name_mp", fw_sel_idx, &platdata->firmware_name_mp);
	if (platdata->firmware_name_mp == NULL || strlen(platdata->firmware_name_mp) == 0) {
		input_err(true, dev, "%s: Failed to get mp fw name\n", __func__);
		return -EINVAL;
	} else {
		input_info(true, dev, "%s: fw name(%s)\n", __func__, platdata->firmware_name_mp);
	}

	/* lcd regulator */
	if (of_property_read_string(np, "novatek,name_lcd_rst", &platdata->name_lcd_rst)) {
		input_err(true, dev, "%s: Failed to get name_lcd_rst property\n", __func__);
		platdata->name_lcd_rst = NULL;
	}

	if (of_property_read_string(np, "novatek,name_lcd_vddi", &platdata->name_lcd_vddi)) {
		input_err(true, dev, "%s: Failed to get name_lcd_vddi property\n", __func__);
		platdata->name_lcd_vddi = NULL;
	}

	if (of_property_read_string(np, "novatek,name_lcd_bl_en", &platdata->name_lcd_bl_en)) {
		input_err(true, dev, "%s: Failed to get name_lcd_bl_en property\n", __func__);
		platdata->name_lcd_bl_en = NULL;
	}

	if (of_property_read_string(np, "novatek,name_lcd_vsp", &platdata->name_lcd_vsp)) {
		input_err(true, dev, "%s: Failed to get name_lcd_vsp name property\n", __func__);
		platdata->name_lcd_vsp = NULL;
	}

	if (of_property_read_string(np, "novatek,name_lcd_vsn", &platdata->name_lcd_vsn)) {
		input_err(true, dev, "%s: Failed to get name_lcd_vsn name property\n", __func__);
		platdata->name_lcd_vsn = NULL;
	}

#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = of_get_named_gpio_flags(np, "novatek,reset-gpio", 0, &ts->reset_flags);
	input_info(true, dev, "%s: novatek,reset-gpio=%d\n", __func__, ts->reset_gpio);
#endif
	platdata->irq_gpio = of_get_named_gpio_flags(np, "novatek,irq-gpio", 0, &platdata->irq_flags);
	if (!gpio_is_valid(platdata->irq_gpio)) {
		pr_err("failed to get irq-gpio(%d)\n", platdata->irq_gpio);
		return -EINVAL;
	} else if (!platdata->irq_flags) {
		platdata->irq_flags = IRQ_TYPE_EDGE_RISING | IRQF_ONESHOT;
	}
	input_info(true, dev, "%s: novatek,irq-gpio=%d\n", __func__, platdata->irq_gpio);

	platdata->cs_gpio = of_get_named_gpio(np, "novatek,cs-gpio", 0);

	if (!gpio_is_valid(platdata->cs_gpio)) {
		platdata->cs_gpio = -1;
		input_info(true, dev, "%s: gpio_cs value is not valid\n", __func__);

	} else {
		int error;
		input_info(true, dev, "%s: cs_gpio=%d\n", __func__, platdata->cs_gpio);
		error = gpio_request(platdata->cs_gpio, "novatek,cs-gpio");

		if (error < 0) {
			input_err(true, dev, "%s: request gpio_cs pin failed\n", __func__);
		} else {
			gpio_direction_output(platdata->cs_gpio, 1);
		}
	}

	ret = of_property_read_u32(np, "novatek,swrst-n8-addr", &SWRST_N8_ADDR);
	if (ret) {
		input_err(true, dev, "%s: error reading novatek,swrst-n8-addr. ret=%d\n", __func__, ret);
		return -ENXIO;
	} else {
		input_info(true, dev, "%s: SWRST_N8_ADDR=0x%06X\n", __func__, SWRST_N8_ADDR);
	}

	ret = of_property_read_u32(np, "novatek,spi-rd-fast-addr", &SPI_RD_FAST_ADDR);
	if (ret) {
		input_info(true, dev, "%s: not support novatek,spi-rd-fast-addr\n", __func__);
		SPI_RD_FAST_ADDR = 0;
//		ret = ERR_PTR(-ENXIO);
	} else {
		input_info(true, dev, "%s: SPI_RD_FAST_ADDR=0x%06X\n", __func__, SPI_RD_FAST_ADDR);
	}

	ret = of_property_read_u32_array(np, "novatek,resolution", tmp, 2);
	if (!ret) {
		platdata->abs_x_max = tmp[0];
		platdata->abs_y_max = tmp[1];
	} else {
		platdata->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
		platdata->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;
	}

	ret = of_property_read_u32_array(np, "novatek,max_touch_num", tmp, 1);
	if (!ret)
		platdata->max_touch_num = tmp[0];
	else
		platdata->max_touch_num = TOUCH_MAX_FINGER_NUM;

	if (of_property_read_u32_array(np, "novatek,area-size", tmp, 3)) {
		platdata->area_indicator = 48;
		platdata->area_navigation = 96;
		platdata->area_edge = 60;
	} else {
		platdata->area_indicator = tmp[0];
		platdata->area_navigation = tmp[1];
		platdata->area_edge = tmp[2];
	}

	input_info(true, dev, "%s: zone's size - indicator:%d, navigation:%d, edge:%d\n",
		__func__, platdata->area_indicator, platdata->area_navigation, platdata->area_edge);

	input_info(true, dev, "%s: irq_flags: 0x%X, resolution: (%d, %d), max_touch_num: %d\n",
		__func__, platdata->irq_flags,// platdata->firmware_name,
		platdata->abs_x_max, platdata->abs_y_max,
		platdata->max_touch_num);
		
	platdata->support_ear_detect = of_property_read_bool(np, "novatek,support_ear_detect_mode");
	input_info(true, dev, "%s: ED mode %s\n",
				__func__, platdata->support_ear_detect ? "ON" : "OFF");

	platdata->enable_settings_aot = of_property_read_bool(np, "novatek,enable_settings_aot");
	input_info(true, dev, "%s: AOT mode %s\n",
				__func__, platdata->enable_settings_aot ? "ON" : "OFF");

	platdata->enable_sysinput_enabled = of_property_read_bool(np, "novatek,enable_sysinput_enabled");
	input_info(true, dev, "%s: Sysinput enabled %s\n",
				__func__, platdata->enable_sysinput_enabled ? "ON" : "OFF");

	platdata->prox_lp_scan_enabled = of_property_read_bool(np, "novatek,prox_lp_scan_enabled");
	input_info(true, dev, "%s: Prox LP Scan enabled %s\n",
				__func__, platdata->prox_lp_scan_enabled ? "ON" : "OFF");

	platdata->enable_glove_mode = of_property_read_bool(np, "novatek,enable_glove_mode");
	input_info(true, dev, "%s: glove enabled %s\n",
				__func__, platdata->enable_glove_mode ? "ON" : "OFF");

	input_info(true, dev, "%s: end!\n", __func__);

	return 0;
}
#else
static int nvt_parse_dt(struct device *dev)
{
	struct nvt_ts_platdata *platdata = dev->platform_data;

	input_err(true, &dev, "no platform data specified\n");

#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = NVTTOUCH_RST_PIN;
#endif
	platdata->irq_gpio = NVTTOUCH_INT_PIN;
	return platdata;
}
#endif

static int nvt_regulator_init(struct nvt_ts_data *ts)
{

	if (ts->platdata->name_lcd_rst) {
		ts->regulator_lcd_rst = regulator_get(NULL, ts->platdata->name_lcd_rst);
		if (IS_ERR(ts->regulator_lcd_rst)) {
			input_err(true, &ts->client->dev, "%s: Failed to get regulator_lcd_rst regulator.", __func__);
		}
	}

	if (ts->platdata->name_lcd_vddi) {
		ts->regulator_lcd_vddi = regulator_get(NULL, ts->platdata->name_lcd_vddi);
		if (IS_ERR(ts->regulator_lcd_vddi)) {
			input_err(true, &ts->client->dev, "%s: Failed to get regulator_lcd_vddi regulator.", __func__);
		}
	}

	if (ts->platdata->name_lcd_bl_en) {
		ts->regulator_lcd_bl_en = regulator_get(NULL, ts->platdata->name_lcd_bl_en);
		if (IS_ERR(ts->regulator_lcd_bl_en)) {
			input_err(true, &ts->client->dev, "%s: Failed to get regulator_lcd_bl_en regulator.", __func__);
		}
	}

	if (ts->platdata->name_lcd_vsp) {
		ts->regulator_lcd_vsp = regulator_get(NULL, ts->platdata->name_lcd_vsp);
		if (IS_ERR(ts->regulator_lcd_vsp)) {
			input_err(true, &ts->client->dev, "%s: Failed to get regulator_lcd_vsp regulator.", __func__);
		}
	}

	if (ts->platdata->name_lcd_vsn) {
		ts->regulator_lcd_vsn = regulator_get(NULL, ts->platdata->name_lcd_vsn);
		if (IS_ERR(ts->regulator_lcd_vsn)) {
			input_err(true, &ts->client->dev, "%s: Failed to get regulator_lcd_vsn regulator.", __func__);
		}
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen config and request gpio

return:
	Executive outcomes. 0---succeed. not 0---failed.
*******************************************************/
static int nvt_gpio_config(struct device *dev, struct nvt_ts_platdata *platdata)
{
	int32_t ret = 0;

#if NVT_TOUCH_SUPPORT_HW_RST
	/* request RST-pin (Output/High) */
	if (gpio_is_valid(ts->reset_gpio)) {
		ret = gpio_request_one(ts->reset_gpio, GPIOF_OUT_INIT_LOW, "NVT-tp-rst");
		if (ret) {
			input_err(true, &ts->client->dev,"Failed to request NVT-tp-rst GPIO\n");
			goto err_request_reset_gpio;
		}
	}
#endif

	/* request INT-pin (Input) */
	if (gpio_is_valid(platdata->irq_gpio)) {
		ret = gpio_request_one(platdata->irq_gpio, GPIOF_IN, "NVT-int");
		if (ret) {
			input_err(true, &ts->client->dev,"Failed to request NVT-int GPIO\n");
			goto err_request_irq_gpio;
		}
	}

	return ret;

err_request_irq_gpio:
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_free(ts->reset_gpio);
err_request_reset_gpio:
#endif
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen deconfig gpio

return:
	n.a.
*******************************************************/
static void nvt_gpio_deconfig(struct nvt_ts_data *ts)
{
	if (gpio_is_valid(ts->platdata->irq_gpio))
		gpio_free(ts->platdata->irq_gpio);
#if NVT_TOUCH_SUPPORT_HW_RST
	if (gpio_is_valid(ts->reset_gpio))
		gpio_free(ts->reset_gpio);
#endif
}

static uint8_t nvt_fw_recovery(uint8_t *point_data)
{
	uint8_t i = 0;
	uint8_t detected = true;

	/* check pattern */
	for (i=1 ; i<7 ; i++) {
		if (point_data[i] != 0x77) {
			detected = false;
			break;
		}
	}

	return detected;
}

#if NVT_TOUCH_ESD_PROTECT
void nvt_esd_check_enable(uint8_t enable)
{
	/* update interrupt timer */
	irq_timer = jiffies;
	/* clear esd_retry counter, if protect function is enabled */
	esd_retry = enable ? 0 : esd_retry;
	/* enable/disable esd check flag */
	esd_check = enable;
}

static void nvt_esd_check_func(struct work_struct *work)
{
	unsigned int timer = jiffies_to_msecs(jiffies - irq_timer);

	//pr_info("esd_check = %d (retry %d)\n", esd_check, esd_retry);	//DEBUG

	if ((timer > NVT_TOUCH_ESD_CHECK_PERIOD) && esd_check) {
		mutex_lock(&ts->lock);
		input_err(true, &ts->client->dev, "%s: do ESD recovery, timer = %d, retry = %d\n",
					__func__, timer, esd_retry);
		/* do esd recovery, reload fw */
		nvt_update_firmware(ts->platdata->firmware_name);
		if (nvt_check_fw_reset_state(RESET_STATE_REK))
			input_err(true, &ts->client->dev, "%s: Check FW state failed after ESD recovery\n", __func__);
		else
			nvt_ts_mode_restore(ts);
		mutex_unlock(&ts->lock);
		/* update interrupt timer */
		irq_timer = jiffies;
		/* update esd_retry counter */
		esd_retry++;
	}

	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if NVT_TOUCH_WDT_RECOVERY
static uint8_t recovery_cnt = 0;
static uint8_t nvt_wdt_fw_recovery(uint8_t *point_data)
{
   uint32_t recovery_cnt_max = 10;
   uint8_t recovery_enable = false;
   uint8_t i = 0;

   recovery_cnt++;

   /* check pattern */
   for (i=1 ; i<7 ; i++) {
       if ((point_data[i] != 0xFD) && (point_data[i] != 0xFE)) {
           recovery_cnt = 0;
           break;
       }
   }

   if (recovery_cnt > recovery_cnt_max){
       recovery_enable = true;
       recovery_cnt = 0;
   }

   return recovery_enable;
}

void nvt_read_fw_history(uint32_t fw_history_addr)
{
    uint8_t i = 0;
    uint8_t buf[65];
    char str[128];

	if (fw_history_addr == 0)
		return;

    nvt_set_page(fw_history_addr);

    buf[0] = (uint8_t) (fw_history_addr & 0x7F);
    CTP_SPI_READ(ts->client, buf, 64+1);	//read 64bytes history

    //print all data
    input_info(true, &ts->client->dev,"fw history 0x%x: \n", fw_history_addr);
    for (i = 0; i < 4; i++) {
        snprintf(str, sizeof(str),
				"%02X %02X %02X %02X %02X %02X %02X %02X  "
				"%02X %02X %02X %02X %02X %02X %02X %02X\n",
                 buf[1+i*16], buf[2+i*16], buf[3+i*16], buf[4+i*16],
                 buf[5+i*16], buf[6+i*16], buf[7+i*16], buf[8+i*16],
                 buf[9+i*16], buf[10+i*16], buf[11+i*16], buf[12+i*16],
                 buf[13+i*16], buf[14+i*16], buf[15+i*16], buf[16+i*16]);
        input_info(true, &ts->client->dev,"%s", str);
    }

	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}
#endif	/* #if NVT_TOUCH_WDT_RECOVERY */

void nvt_print_info(void)
{
	if (!ts)
		return;

	ts->print_info_cnt_open++;

	if (ts->print_info_cnt_open > 0xfff0)
		ts->print_info_cnt_open = 0;

	if (ts->touch_count == 0)
		ts->print_info_cnt_release++;

	input_info(true, &ts->client->dev,
			"tc:%d mode:%04X noise:%d lp:%d ed:%d // v:NO%02X%02X%02X%02X // #%d %d\n",
			ts->touch_count, ts->sec_function, ts->noise_mode, ts->lowpower_mode, ts->ear_detect_mode,
			ts->fw_ver_bin[0], ts->fw_ver_bin[1], ts->fw_ver_bin[2], ts->fw_ver_bin[3],
			ts->print_info_cnt_open, ts->print_info_cnt_release);
}

static void nvt_print_info_work(struct work_struct *work)
{
	nvt_print_info();

	schedule_delayed_work(&ts->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

static void nvt_read_info_work(struct work_struct *work)
{
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	input_err(true, &ts->client->dev, "%s: factory bin : skip factory_cmd_result_all call\n", __func__);
#endif

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	read_tsp_info_onboot(&ts->sec);
#endif

	cancel_delayed_work(&ts->work_print_info);
	ts->print_info_cnt_open = 0;
	ts->print_info_cnt_release = 0;
	schedule_work(&ts->work_print_info.work);
}

#if POINT_DATA_CHECKSUM
static int32_t nvt_ts_point_data_checksum(uint8_t *buf, uint8_t length)
{
   uint8_t checksum = 0;
   int32_t i = 0;

   // Generate checksum
   for (i = 0; i < length - 1; i++) {
       checksum += buf[i + 1];
   }
   checksum = (~checksum + 1);

   // Compare ckecksum and dump fail data
   if (checksum != buf[length]) {
	   input_err(true, &ts->client->dev, "%s : checksum not match.(point_data[%d]=0x%02X, checksum=0x%02X)\n",
				   __func__, length, buf[length], checksum);

       for (i = 0; i < 10; i++) {
			input_dbg(true, &ts->client->dev, "%02X %02X %02X %02X %02X %02X\n",
                   buf[1 + i*6], buf[2 + i*6], buf[3 + i*6], buf[4 + i*6], buf[5 + i*6], buf[6 + i*6]);
       }

		input_dbg(true, &ts->client->dev, "%02X %02X %02X %02X %02X\n", buf[61], buf[62], buf[63], buf[64], buf[65]);

		return -1;
	}

	return 0;
}

#if SEC_LPWG_DUMP
// Due to more checksum length than point report, duplicate another checksum function
static int32_t nvt_ts_lpwg_dump_checksum(uint8_t *buf, uint32_t length)
{
	uint8_t checksum = 0;
	uint32_t i = 0;

	// Generate checksum
	for (i = 0; i < length - 1; i++) {
		checksum += buf[i + 1];
	}
	checksum = (~checksum + 1);

	// Compare ckecksum and dump fail data
	if (checksum != buf[length]) {
		input_err(true, &ts->client->dev, "%s : checksum not match.(lpwg_dump[%d]=0x%02X, checksum=0x%02X)\n",
					__func__, length, buf[length], checksum);

		for (i = 0; i < 10; i++) {
			input_dbg(true, &ts->client->dev, "%s : %02X %02X %02X %02X %02X %02X\n",
					__func__, buf[1 + i*6], buf[2 + i*6], buf[3 + i*6], buf[4 + i*6], buf[5 + i*6], buf[6 + i*6]);
		}

		input_dbg(true, &ts->client->dev, "%s : %02X %02X %02X %02X %02X\n",
					__func__, buf[61], buf[62], buf[63], buf[64], buf[65]);

		return -1;
	}

	return 0;
}
#endif
#endif /* POINT_DATA_CHECKSUM */

#define NVT_TS_LOCATION_DETECT_SIZE	6
/************************************************************
*  720  * 1480 : <48 96 60> indicator: 24dp navigator:48dp edge:60px dpi=320
* 1080  * 2220 :  4096 * 4096 : <133 266 341>  (approximately value)
************************************************************/
static void location_detect(struct nvt_ts_data *ts, char *loc, int x, int y)
{
	int i;

	for (i = 0; i < NVT_TS_LOCATION_DETECT_SIZE; ++i)
		loc[i] = 0;

	if (x < ts->platdata->area_edge)
		strcat(loc, "E.");
	else if (x < (ts->platdata->abs_x_max - ts->platdata->area_edge))
		strcat(loc, "C.");
	else
		strcat(loc, "e.");

	if (y < ts->platdata->area_indicator)
		strcat(loc, "S");
	else if (y < (ts->platdata->abs_y_max - ts->platdata->area_navigation))
		strcat(loc, "C");
	else
		strcat(loc, "N");
}

static void nvt_ts_print_coord(struct nvt_ts_data *ts, bool force_release)
{
	int i;
	char location[NVT_TS_LOCATION_DETECT_SIZE] = { 0, };

	for (i = 0; i < TOUCH_MAX_FINGER_NUM; i++) {
		location_detect(ts, location, ts->coords[i].x, ts->coords[i].y);

		if (ts->coords[i].press && !ts->coords[i].prev_press) {
			ts->touch_count++;

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &ts->client->dev, "[P] tId:%d.%d x:%d y:%d major:%d minor:%d loc:%s tc:%d type:%X\n",
				i, (ts->input_dev->mt->trkid - 1) & TRKID_MAX,
				ts->coords[i].x, ts->coords[i].y, ts->coords[i].w_major, ts->coords[i].w_minor,
				location, ts->touch_count, ts->coords[i].status);
#else
			input_info(true, &ts->client->dev, "[P] tId:%d.%d major:%d minor:%d loc:%s tc:%d type:%X\n",
				i, (ts->input_dev->mt->trkid - 1) & TRKID_MAX,
				ts->coords[i].w_major, ts->coords[i].w_minor, location, ts->touch_count,
				ts->coords[i].status);
#endif
			ts->coords[i].prev_press = ts->coords[i].press;
			ts->coords[i].first_x = ts->coords[i].x;
			ts->coords[i].first_y = ts->coords[i].y;

			ts->coords[i].prev_status = 0;
			ts->coords[i].move_count = 0;

		} else if (!ts->coords[i].press && ts->coords[i].prev_press) {
			ts->touch_count--;

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &ts->client->dev, "[R%s] tId:%d loc:%s dd:%d,%d mc:%d tc:%d lx:%d ly:%d\n",
				force_release ? "A" : "",
				i, location, ts->coords[i].first_x - ts->coords[i].x,
				ts->coords[i].first_y - ts->coords[i].y,
				ts->coords[i].move_count, ts->touch_count,
				ts->coords[i].x, ts->coords[i].y);
#else
			input_info(true, &ts->client->dev, "[R%s] tId:%d loc:%s dd:%d,%d mc:%d tc:%d\n",
				force_release ? "A" : "",
				i, location, ts->coords[i].first_x - ts->coords[i].x,
				ts->coords[i].first_y - ts->coords[i].y,
				ts->coords[i].move_count, ts->touch_count);
#endif
			ts->coords[i].prev_press = false;
		} else if (ts->coords[i].press) {
			if (ts->coords[i].prev_status && (ts->coords[i].status != ts->coords[i].prev_status)) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &ts->client->dev, "[C] tId:%d x:%d y:%d major:%d minor:%d tc:%d type:%X\n",
					i, ts->coords[i].x, ts->coords[i].y, ts->coords[i].w_major, ts->coords[i].w_minor,
					ts->touch_count, ts->coords[i].status);
#else
				input_info(true, &ts->client->dev, "[C] tId:%d major:%d minor:%d tc:%d type:%X\n",
					i, ts->coords[i].w_major, ts->coords[i].w_minor,
					ts->touch_count, ts->coords[i].status);
#endif
			}

			ts->coords[i].prev_status = ts->coords[i].status;
			ts->coords[i].move_count++;
		}
	}
}

#if SEC_FW_STATUS
#define FW_STATUS_OFFSET	(1 + 0x3C)	// 1 is for read dummy byte, total 2 bytes length for fw status

int nvt_ts_check_all_value(uint8_t *point_data)
{
	int tmp_val = 0xFF;
	int ret = 0, i;

	for (i = 1 ; i < POINT_DATA_CHECKSUM_LEN ; i++) {
		tmp_val &= point_data[i];
	}

	if (tmp_val == 0xff) {
		input_info(true, &ts->client->dev, "%s: all data is 0xff\n", __func__);
		ret = -1;
	}
	return ret;
}

void nvt_ts_ic_status(uint8_t *point_data, bool force_print)
{
	u8 fw_status = 0;
	u8 fw_status_changed = 0;
	char print_buff[800] = { 0 };
	char tmp_buff[100] = { 0 };

	if (force_print) {
		// print current status
		fw_status = ts->fw_status_record;
		fw_status_changed = 0xFF;

	} else {
		if (point_data == NULL) {
			input_err(true, &ts->client->dev, "%s: point_data is null\n", __func__);
			return;
		}

		// check abnormal event
		if (nvt_ts_check_all_value(point_data))
			return;

		// print change status
		fw_status = point_data[FW_STATUS_OFFSET];
		fw_status_changed = fw_status ^ ts->fw_status_record;
		ts->fw_status_record = fw_status;
	}

	if (fw_status_changed & FW_STATUS_WATER_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Water flag %s)\n", __func__, (fw_status & FW_STATUS_WATER_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Water flag %s, ", (fw_status & FW_STATUS_WATER_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_PALM_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Palm flag %s)\n", __func__, (fw_status & FW_STATUS_PALM_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Palm flag %s, ", (fw_status & FW_STATUS_PALM_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_HOPPING_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Hopping flag %s)\n", __func__, (fw_status & FW_STATUS_HOPPING_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Hopping flag %s, ", (fw_status & FW_STATUS_HOPPING_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_BENDING_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Bending flag %s)\n", __func__, (fw_status & FW_STATUS_BENDING_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Bending flag %s, ", (fw_status & FW_STATUS_BENDING_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_GLOVE_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Glove flag %s)\n", __func__, (fw_status & FW_STATUS_GLOVE_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Glove flag %s, ", (fw_status & FW_STATUS_GLOVE_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_GND_UNSTABLE) {
//			input_info(true, &ts->client->dev, "%s: FW status (GND unstable %s)\n", __func__, (fw_status & FW_STATUS_GND_UNSTABLE) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "GND unstable %s, ", (fw_status & FW_STATUS_GND_UNSTABLE) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_TA_PIN) {
//			input_info(true, &ts->client->dev, "%s: FW status (TA pin %s)\n", __func__, (fw_status & FW_STATUS_TA_PIN) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "TA pin %s, ", (fw_status & FW_STATUS_TA_PIN) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}
	if (print_buff[0])
		input_info(true, &ts->client->dev, "%s: %s FW status %s\n",
					__func__, force_print ? "Current" : "Change", print_buff);

	if (force_print)
		return;

	switch (point_data[FW_STATUS_OFFSET + 1] & FW_STATUS_REK_STATUS) {
	case 0:
		// Do nothing...
		break;
	case 1:
		input_err(true, &ts->client->dev, "%s: FW status (1D reK)\n", __func__);
		break;
	case 2:
		input_err(true, &ts->client->dev, "%s: FW status (2D RC reK)\n", __func__);
		break;
	case 3:
		input_err(true, &ts->client->dev, "%s: FW status (2D raw check reK)\n", __func__);
		break;
	default:
		input_err(true, &ts->client->dev, "%s: FW status (invalid reK status : %02x%02x)\n",
					__func__, point_data[FW_STATUS_OFFSET], point_data[FW_STATUS_OFFSET + 1]);
	}
}
#endif

#define POINT_DATA_LEN 108
#define FINGER_ENTER			0x01
#define FINGER_MOVING			0x02
#define GLOVE_TOUCH			0x06

#if SEC_LPWG_DUMP
#define LPWG_DUMP_LOG_LEN	505	//2B Slot ID + 2B History Size + 500B History + 1B Checksum
#define LPWG_DUMP_EVENT_LEN	5
#define LPWG_DUMP_EVENT_MAX_NUM	100
#define LPWG_DUMP_EVENT_MSG_LEN	20

void nvt_ts_lpwg_dump_buf_init(void)
{
	ts->lpwg_dump_buf = devm_kzalloc(&ts->client->dev, LPWG_DUMP_TOTAL_SIZE, GFP_KERNEL);
	if (ts->lpwg_dump_buf == NULL) {
		input_err(true, &ts->client->dev, "kzalloc for lpwg_dump_buf failed!\n");
		return;
	}
	ts->lpwg_dump_buf_idx = 0;
	input_info(true, &ts->client->dev, "%s : done\n", __func__);
}

int nvt_ts_lpwg_dump_buf_write(u8 *buf)
{
	int i = 0;

	if (ts->lpwg_dump_buf == NULL) {
		input_err(true, &ts->client->dev, "%s : kzalloc for lpwg_dump_buf failed!\n", __func__);
		return -1;
	}
//	input_info(true, &ts->client->dev, "%s : idx(%d) data (0x%X,0x%X,0x%X,0x%X,0x%X)\n",
//			__func__, ts->lpwg_dump_buf_idx, buf[0], buf[1], buf[2], buf[3], buf[4]);

	for (i = 0 ; i < LPWG_DUMP_PACKET_SIZE ; i++) {
		ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx++] = buf[i];
	}
	if (ts->lpwg_dump_buf_idx >= LPWG_DUMP_TOTAL_SIZE) {
		input_info(true, &ts->client->dev, "%s : write end of data buf(%d)!\n",
					__func__, ts->lpwg_dump_buf_idx);
		ts->lpwg_dump_buf_idx = 0;
	}
	return 0;
}

int nvt_ts_lpwg_dump_buf_read(u8 *buf)
{

	u8 read_buf[30] = { 0 };
	int read_packet_cnt;
	int start_idx;
	int i;

	if (ts->lpwg_dump_buf == NULL) {
		input_err(true, &ts->client->dev, "%s : kzalloc for lpwg_dump_buf failed!\n", __func__);
		return 0;
	}

	if (ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx] == 0
		&& ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx + 1] == 0
		&& ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx + 2] == 0) {
		start_idx = 0;
		read_packet_cnt = ts->lpwg_dump_buf_idx / LPWG_DUMP_PACKET_SIZE;
	} else {
		start_idx = ts->lpwg_dump_buf_idx;
		read_packet_cnt = LPWG_DUMP_TOTAL_SIZE / LPWG_DUMP_PACKET_SIZE;
	}

	input_info(true, &ts->client->dev, "%s : lpwg_dump_buf_idx(%d), start_idx (%d), read_packet_cnt(%d)\n",
				__func__, ts->lpwg_dump_buf_idx, start_idx, read_packet_cnt);

	for (i = 0 ; i < read_packet_cnt ; i++) {
		memset(read_buf, 0x00, 30);
		snprintf(read_buf, 30, "%03d : %02X%02X%02X%02X%02X\n",
					i, ts->lpwg_dump_buf[start_idx + 0], ts->lpwg_dump_buf[start_idx + 1],
					ts->lpwg_dump_buf[start_idx + 2], ts->lpwg_dump_buf[start_idx + 3],
					ts->lpwg_dump_buf[start_idx + 4]);

//		input_info(true, &ts->client->dev, "%s : %s\n", __func__, read_buf);
		strlcat(buf, read_buf, PAGE_SIZE);

		if (start_idx + LPWG_DUMP_PACKET_SIZE >= LPWG_DUMP_TOTAL_SIZE) {
			start_idx = 0;
		} else {
			start_idx += 5;
		}
	}

	return 0;
}

/*******************************************************
Description:
	Novatek lpwg log dump function.

return:
	n.a.
*******************************************************/
void nvt_ts_lpwg_dump(void)
{
#if 0
	struct nvt_ts_lpwg_coordinate_event *p_lpwg_coordinate_event;
	struct nvt_ts_lpwg_gesture_event *p_lpwg_gesture_event;
	struct nvt_ts_lpwg_vendor_event *p_lpwg_vendor_event;
	char buff[LPWG_DUMP_EVENT_MSG_LEN] = { 0 };
#endif
	uint8_t log_dump[LPWG_DUMP_LOG_LEN + DUMMY_BYTES] = {0};
	u16 next_slot = 0, history_size = 0, event_cnt = 0, i = 0;
	int32_t ret = -1;

	if (ts->mmap->LPWG_DUMP_ADDR == 0) {
		input_err(true, &ts->client->dev, "%s: Invalid LPWG dump address.(%d)\n", __func__, ts->mmap->LPWG_DUMP_ADDR);
		return;
	}

	mutex_lock(&ts->lock);

	//---set xdata index to LPWG_DUMP_ADDR---
	nvt_set_page(ts->mmap->LPWG_DUMP_ADDR);

	//---read data from index---
	log_dump[0] = ts->mmap->LPWG_DUMP_ADDR & (0x7F);
	ret = CTP_SPI_READ(ts->client, log_dump, LPWG_DUMP_LOG_LEN + DUMMY_BYTES);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s:  CTP_SPI_READ failed.(%d)\n", __func__, ret);
		goto XFER_ERROR;
	}

#if POINT_DATA_CHECKSUM
	ret = nvt_ts_lpwg_dump_checksum(log_dump, LPWG_DUMP_LOG_LEN);
	if (ret) {
		goto XFER_ERROR;
	}
#endif /* POINT_DATA_CHECKSUM */

	// Deal with 4 header bytes, skip 1 dummy byte
	next_slot = log_dump[1] + (log_dump[2] << 8);
	history_size = log_dump[3] + (log_dump[4] << 8);
	event_cnt = history_size / LPWG_DUMP_EVENT_LEN;

	if (event_cnt < LPWG_DUMP_EVENT_MAX_NUM) {
		i = 0;
	} else {
		i = next_slot;
	}
	input_info(true, &ts->client->dev, "%s: event_cnt(%d) start i(%d)", __func__, event_cnt, i);

	for (; event_cnt > 0; i++, event_cnt--) {

		i %= LPWG_DUMP_EVENT_MAX_NUM;	// in case overrun, FIFO (Round Robin scheme)

		nvt_ts_lpwg_dump_buf_write(&log_dump[LPWG_DUMP_EVENT_LEN * (i + 1)]);

#if 0
		//extra 1 event len for 1 dummy byte and 4 header bytes
		p_lpwg_coordinate_event = (struct nvt_ts_lpwg_coordinate_event *)&log_dump[LPWG_DUMP_EVENT_LEN * (i + 1)];
//		input_info(true, &ts->client->dev, "%s: Event slot(%d) Event type(%d)",
//				__func__, i, p_lpwg_coordinate_event->event_type);

		if (p_lpwg_coordinate_event->event_type == LPWG_EVENT_COOR) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &ts->client->dev, "%s: slot(%2d) [%s] TID(%2d) : X(%4d) Y(%4d) Frame count(%4d)\n",
				__func__, i, p_lpwg_coordinate_event->touch_status ? "R" : "P", p_lpwg_coordinate_event->tid,
				((p_lpwg_coordinate_event->x_11_4 << 4) + p_lpwg_coordinate_event->x_3_0),
				((p_lpwg_coordinate_event->y_11_4 << 4) + p_lpwg_coordinate_event->y_3_0),
				((p_lpwg_coordinate_event->frame_count_9_8 << 8) + p_lpwg_coordinate_event->frame_count_7_0));
#else
			input_info(true, &ts->client->dev, "%s: slot(%2d) [%s] TID(%2d) Frame count(%4d)\n",
				__func__, i, p_lpwg_coordinate_event->touch_status ? "R" : "P", p_lpwg_coordinate_event->tid,
				((p_lpwg_coordinate_event->frame_count_9_8 << 8) + p_lpwg_coordinate_event->frame_count_7_0));
#endif
		} else if (p_lpwg_coordinate_event->event_type == LPWG_EVENT_GESTURE) {
			p_lpwg_gesture_event = (struct nvt_ts_lpwg_gesture_event *)p_lpwg_coordinate_event;

			input_info(true, &ts->client->dev, "%s: slot(%2d) Gesture ID(%11s)\n",
				__func__, i, p_lpwg_gesture_event->gesture_id ? "Swipe up" : "Double tap");
		} else if (p_lpwg_coordinate_event->event_type == LPWG_EVENT_VENDOR) {
			p_lpwg_vendor_event = (struct nvt_ts_lpwg_vendor_event *)p_lpwg_coordinate_event;

			switch (p_lpwg_vendor_event->ng_code) {
			case 4:
				snprintf(buff, sizeof(buff), "%s", "Timing Err");
				break;
			case 5:
				snprintf(buff, sizeof(buff), "%s", "Distance Err");
				break;
			case 6:
				snprintf(buff, sizeof(buff), "%s", "Long Touch Err");
				break;
			case 7:
				snprintf(buff, sizeof(buff), "%s", "Multi-Finger");
				break;
			default:
				snprintf(buff, sizeof(buff), "%s", "Unknown Err Code");
			}
			input_info(true, &ts->client->dev, "%s: slot(%2d) Event type(Vendor) Info(%2d) NgType(%20s)\n",
					__func__, i, p_lpwg_vendor_event->info_type, buff);
		} else {	// invalid event
			input_info(true, &ts->client->dev, "%s: Event slot(%2d) Event type(Unknown)"
				"    invalid event!!! (%02X, %02X, %02X, %02X, %02X)\n",
				__func__, i, log_dump[LPWG_DUMP_EVENT_LEN * (i + 1)],
				log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 1], log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 2],
				log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 3], log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 4]);
		}
#endif

	}

XFER_ERROR:
	//---set xdata index to EVENT_BUF_ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	mutex_unlock(&ts->lock);

	return;
}
#endif


#if PROXIMITY_FUNCTION
void nvt_ts_proximity_report(uint8_t *data)
{
	struct nvt_ts_event_proximity *p_event_proximity;
	u8 status = 0;

	 p_event_proximity = (struct nvt_ts_event_proximity *)&data[1];

	/* check data page */
	if (p_event_proximity->data_page != FUNCPAGE_PROXIMITY) {
		input_info(true, &ts->client->dev,"proximity data page %d is invalid\n", p_event_proximity->data_page);
		return;
	}

	status = p_event_proximity->status;

	input_info(true, &ts->client->dev,"proximity->status = %d\n", status);

	input_info(true, &ts->client->dev, "%s hover : %d\n", __func__, status);
	ts->hover_event = status;
	input_report_abs(ts->input_dev_proximity, ABS_MT_CUSTOM, status);
	input_sync(ts->input_dev_proximity);

#if 0
	switch (p_event_proximity->status) {
		/* Case amount depends on the status amount */
		case 0:
			pr_info(true, &ts->client->dev,"Proximity State 0.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		case 1:
			pr_info("Proximity State 1.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		case 2:
			pr_info("Proximity State 2.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		case 3:
			pr_info("Proximity State 3.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		case 4:
			pr_info("Proximity State 4.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		case 5:
			pr_info("Proximity State 5.\n");
			/* Action have to be done, like notify DP to do something */
			break;
		default:
			break;
	}
#endif

}
#endif

void nvt_ts_release_all_finger(struct nvt_ts_data *ts)
{
	int i = 0;

	if (ts->prox_power_off) {
		input_info(true, &ts->client->dev, "%s : cancel touch\n", __func__);
		input_report_key(ts->input_dev, KEY_INT_CANCEL, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, KEY_INT_CANCEL, 0);
		input_sync(ts->input_dev);
	}

	for (i = 0; i < ts->platdata->max_touch_num; i++) {
		ts->coords[i].press = false;
		input_mt_slot(ts->input_dev, i);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
	}

	input_report_key(ts->input_dev, BTN_TOUCH, 0);

	nvt_ts_print_coord(ts, true);

	input_sync(ts->input_dev);
}

void nvt_ts_touch_report(struct nvt_ts_data *ts, uint8_t *point_data)
{
	struct nvt_ts_platdata *platdata = ts->platdata;
	struct nvt_ts_event_coord *p_event_coord;
	u8 id = 0, status = 0, press_id[TOUCH_MAX_FINGER_NUM] = {0};
	u16 i = 0, finger_cnt = 0, read_x = 0, read_y = 0;

	for (i = 0; i < platdata->max_touch_num; i++) {
		p_event_coord = (struct nvt_ts_event_coord *)&point_data[1 + 6 * i];
		id = p_event_coord->id;
		if (!id || (id > platdata->max_touch_num))
			continue;

		id = id - 1;
		status = p_event_coord->status;

		if ((status == FINGER_ENTER) || (status == FINGER_MOVING) || (status == GLOVE_TOUCH)) {
#if NVT_TOUCH_ESD_PROTECT
			/* update interrupt timer */
			irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
			read_x = (u16)(p_event_coord->x_11_4 << 4) + (u16)(p_event_coord->x_3_0);
			read_y = (u16)(p_event_coord->y_11_4 << 4) + (u16)(p_event_coord->y_3_0);
			if ((read_x > platdata->abs_x_max) || (read_y > platdata->abs_y_max)) {
				input_err(true, &ts->client->dev, "%s: invalid coordinate (%d, %d)\n",
							__func__, ts->coords[id].x, ts->coords[id].y);
				continue;
			}

			ts->coords[id].status = status;
			ts->coords[id].x = read_x;
			ts->coords[id].y = read_y;
			ts->coords[id].w_major = p_event_coord->w_major ? p_event_coord->w_major : 1;
			ts->coords[id].w_minor = point_data[i + 99] ? point_data[i + 99] : 1;
			press_id[id] = ts->coords[id].press = true;
			finger_cnt++;

			input_mt_slot(ts->input_dev, id);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, ts->coords[id].x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, ts->coords[id].y);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, ts->coords[id].w_major);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, ts->coords[id].w_minor);

		}
	}

	for (i = 0; i < platdata->max_touch_num; i++) {
		if (!press_id[i] && ts->coords[i].prev_press) {
			ts->coords[i].press = false;

			input_mt_slot(ts->input_dev, i);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
		}
	}

	input_report_key(ts->input_dev, BTN_TOUCH, (finger_cnt > 0));

#if TOUCH_KEY_NUM > 0
	if (point_data[61] == 0xF8) {
#if NVT_TOUCH_ESD_PROTECT
		/* update interrupt timer */
		irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		for (i = 0; i < ts->max_button_num; i++) {
			input_report_key(ts->input_dev, touch_key_array[i], ((point_data[62] >> i) & 0x01));
		}
	} else {
		for (i = 0; i < ts->max_button_num; i++) {
			input_report_key(ts->input_dev, touch_key_array[i], 0);
		}
	}
#endif

	input_sync(ts->input_dev);

	nvt_ts_print_coord(ts, false);
}

/*******************************************************
Description:
	Novatek touchscreen work function.

return:
	n.a.
*******************************************************/
static irqreturn_t nvt_ts_work_func(int irq, void *data)
{
	uint8_t point_data[POINT_DATA_LEN + 1 + DUMMY_BYTES] = {0};
	int ret = -1;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (secure_filter_interrupt(ts) == IRQ_HANDLED) {
		wait_for_completion_interruptible_timeout(&ts->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &ts->client->dev,
				"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif

	if (ts->power_status == LP_MODE_STATUS) {
		pm_wakeup_event(&ts->input_dev->dev, 500);

		ret = wait_for_completion_interruptible_timeout(&ts->resume_done, msecs_to_jiffies(500));
		if (ret == 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled\n", __func__);
			return SEC_ERROR;
		}

		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: LPM: -ERESTARTSYS if interrupted, %d\n", __func__, ret);
			return ret;
		}

		input_info(true, &ts->client->dev, "%s: run LPM interrupt handler, %d\n", __func__, ret);
	}

	mutex_lock(&ts->lock);
	ret = CTP_SPI_READ(ts->client, point_data, POINT_DATA_LEN + 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s:  CTP_SPI_READ failed.(%d)\n", __func__, ret);
		goto XFER_ERROR;
	}
/*
	//--- dump SPI buf ---
	for (i = 0; i < 10; i++) {
		printk("%02X %02X %02X %02X %02X %02X  ",
			point_data[1+i*6], point_data[2+i*6], point_data[3+i*6], point_data[4+i*6], point_data[5+i*6], point_data[6+i*6]);
	}
	printk("\n");
*/

#if NVT_TOUCH_WDT_RECOVERY
	/* ESD protect by WDT */
	if (nvt_wdt_fw_recovery(point_data)) {
		pm_wakeup_event(&ts->input_dev->dev, 1000);
		input_err(true, &ts->client->dev,"Recover for fw reset, %02X\n", point_data[1]);

		if (point_data[1] == 0xFE) {
			nvt_sw_reset_idle();
		}
		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT0);
		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT1);

		nvt_update_firmware(ts->platdata->firmware_name);
		if (nvt_check_fw_reset_state(RESET_STATE_REK))
			input_err(true, &ts->client->dev, "%s: Check FW state failed after FW reset recovery\n", __func__);
		else
			nvt_ts_mode_restore(ts);
		goto XFER_ERROR;
	}
#endif /* #if NVT_TOUCH_WDT_RECOVERY */

	/* ESD protect by FW handshake */
	if (nvt_fw_recovery(point_data)) {
#if NVT_TOUCH_ESD_PROTECT
		nvt_esd_check_enable(true);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		goto XFER_ERROR;
	}

#if POINT_DATA_CHECKSUM
   if (POINT_DATA_LEN >= POINT_DATA_CHECKSUM_LEN) {
       ret = nvt_ts_point_data_checksum(point_data, POINT_DATA_CHECKSUM_LEN);
       if (ret) {
           goto XFER_ERROR;
       }
   }
#endif /* POINT_DATA_CHECKSUM */

#if PROXIMITY_FUNCTION
	if (((uint8_t)(point_data[1] >> 3) == DATA_PROTOCOL) && (point_data[2] == FUNCPAGE_PROXIMITY)) {
		nvt_ts_proximity_report(point_data);
		goto XFER_ERROR;
	}
#endif

	if (ts->power_status == LP_MODE_STATUS) {
		if (point_data[2] == FUNCPAGE_GESTURE)
			nvt_ts_wakeup_gesture_report(point_data);
		else
			input_err(true, &ts->client->dev, "invalid lp event (%02X %02X %02X %02X %02X %02X)",
				point_data[0], point_data[1], point_data[2],
				point_data[3], point_data[4], point_data[5]);
		goto XFER_ERROR;
	}

#if SEC_FW_STATUS
	nvt_ts_ic_status(point_data, false);
#endif

	nvt_ts_touch_report(ts, point_data);

XFER_ERROR:
	mutex_unlock(&ts->lock);

	return IRQ_HANDLED;
}


/*******************************************************
Description:
	Novatek touchscreen check chip version trim function.

return:
	Executive outcomes. 0---NVT IC. -1---not NVT IC.
*******************************************************/
static int8_t nvt_ts_check_chip_ver_trim(uint32_t chip_ver_trim_addr)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;
	int32_t list = 0;
	int32_t i = 0;
	int32_t found_nvt_chip = 0;
	int32_t ret = -1;

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {

		nvt_bootloader_reset();

		nvt_set_page(chip_ver_trim_addr);

		buf[0] = chip_ver_trim_addr & 0x7F;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		CTP_SPI_READ(ts->client, buf, 7);
		input_info(true, &ts->client->dev,"buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
			buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		// compare read chip id on supported list
		for (list = 0; list < (sizeof(trim_id_table) / sizeof(struct nvt_ts_trim_id_table)); list++) {
			found_nvt_chip = 0;

			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
				if (trim_id_table[list].mask[i]) {
					if (buf[i + 1] != trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_ID_BYTE_MAX) {
				found_nvt_chip = 1;
			}

			if (found_nvt_chip) {
				input_info(true, &ts->client->dev,"This is NVT touch IC\n");
				ts->mmap = trim_id_table[list].mmap;
				ts->carrier_system = trim_id_table[list].hwinfo->carrier_system;
				ts->hw_crc = trim_id_table[list].hwinfo->hw_crc;
//				ts->fw_ver_ic[0] = ((buf[6] & 0x0F) << 4) | ((buf[5] & 0xF0) >> 4);
				ret = 0;
				goto out;
			} else {
				ts->mmap = NULL;
				ret = -1;
			}
		}

		msleep(10);
	}

out:
	return ret;
}

#if 0	// mtk
static unsigned int lcdtype;
static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	printk("%s: lcdtype: %6X\n", __func__, lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);
#endif

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
static int nvt_notifier_call(struct notifier_block *n, unsigned long data, void *v)
{
	struct panel_state_data *d = (struct panel_state_data *)v;
	int i;

	if (data == PANEL_EVENT_ESD) {
		input_info(true, &ts->client->dev, "%s: LCD ESD INTERRUPT CALLED\n", __func__);
		ts->lcd_esd_recovery = 1;
	} else if (data == PANEL_EVENT_STATE_CHANGED) {
		if (d->state == PANEL_ON) {
			if (ts->lcd_esd_recovery == 1) {
				input_info(true, &ts->client->dev, "%s: LCD ESD LCD ON POST, run fw update\n", __func__);
#if NVT_TOUCH_SUPPORT_HW_RST
				gpio_set_value(ts->reset_gpio, 1);
#endif
				mutex_lock(&ts->lock);
				nvt_update_firmware(ts->platdata->firmware_name);
				nvt_check_fw_reset_state(RESET_STATE_REK);

				nvt_ts_mode_restore(ts);
				mutex_unlock(&ts->lock);
				ts->lcd_esd_recovery = 0;
			}
		}
	}

	if (data == PANEL_EVENT_UB_CON_CHANGED) {
		i = *((char *)v);

		input_info(true, &ts->client->dev, "%s: data = %ld, i = %d\n", __func__, data, i);

		if (i == PANEL_EVENT_UB_CON_DISCONNECTED) {
			input_info(true, &ts->client->dev, "%s: UB_CON_DISCONNECTED : disable irq & pin control\n", __func__);
			nvt_irq_enable(false);
			pinctrl_configure(ts, false);
		}
	}

	return 0;
}
#endif

#if (IS_ENABLED(CONFIG_EXYNOS_DPU30) || IS_ENABLED(CONFIG_DRM_SAMSUNG_DPU)) && IS_ENABLED(CONFIG_PANEL_NOTIFY)
static int nvt_notifier_call(struct notifier_block *n, unsigned long data, void *v)
{
	if (data == PANEL_EVENT_UB_CON_CHANGED) {
		int i = *((char *)v);

		input_info(true, &ts->client->dev, "%s: data = %ld, i = %d\n", __func__, data, i);

		if (i == PANEL_EVENT_UB_CON_DISCONNECTED) {
			input_info(true, &ts->client->dev, "%s: UB_CON_DISCONNECTED : disable irq & pin control\n", __func__);
			nvt_irq_enable(false);
			pinctrl_configure(ts, false);
		}
	}
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
static void nvt_vbus_work(struct work_struct *work)
{
	u8 mode_cmd = 0;
	int ret;

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return;
	}

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: tsp ic is off\n", __func__);
		return;
	}

	if (!nvt_ts_lcd_power_check()) {
		input_err(true, &ts->client->dev, "%s: lcd is off\n", __func__);
		return;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
				__func__);
		return;
	}

	if (ts->sec_function & CHARGER_MASK) {
		mode_cmd = CHARGER_PLUG_AC;
	} else {
		mode_cmd = CHARGER_PLUG_OFF;
	}

	ret = nvt_ts_mode_switch(ts, mode_cmd, false);
	if (ret)
		input_err(true, &ts->client->dev, "failed to switch %s mode\n",
					(mode_cmd == CHARGER_PLUG_AC) ? "CHARGER_PLUG_AC" : "CHARGER_PLUG_OFF");
	else
		input_info(true, &ts->client->dev, "%s : %s done\n",
				__func__, ts->sec_function & CHARGER_MASK ? "attach" : "detach");

	mutex_unlock(&ts->lock);
}

static int tsp_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	vbus_status_t vbus_type = *(vbus_status_t *) data;

	input_info(true, &ts->client->dev, "%s cmd=%lu, vbus_type=%d\n", __func__, cmd, vbus_type);

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return NOTIFY_DONE;
	}

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		input_info(true, &ts->client->dev, "%s : attach\n", __func__);
		ts->sec_function |= CHARGER_MASK;
		break;
	case STATUS_VBUS_LOW:
		input_info(true, &ts->client->dev, "%s : detach\n", __func__);
		ts->sec_function &= ~CHARGER_MASK;
		break;
	default:
		break;
	}

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: tsp ic is off\n", __func__);
		return NOTIFY_DONE;
	}

	if (!nvt_ts_lcd_power_check()) {
		input_err(true, &ts->client->dev, "%s: lcd is off\n", __func__);
		return NOTIFY_DONE;
	}

	schedule_work(&ts->work_vbus.work);

	return NOTIFY_DONE;
}
#endif

static int nvt_ts_fw_update_on_probe(void)
{
	int32_t ret = 0;

	mutex_lock(&ts->lock);
	ts->fw_index = NVT_TSP_FW_IDX_BIN;
	ret = nvt_update_firmware(ts->platdata->firmware_name);
	mutex_unlock(&ts->lock);
	if (ret) {
		input_err(true, &ts->client->dev, "%s : tsp fw update failed!\n", __func__);
		return ret;	
	}

	/* Parsing criteria from dts */
	if(of_property_read_bool(ts->client->dev.of_node, "novatek,mp-support-dt")) {
		u8 mpcriteria[32] = { 0 };
		int pid;
		int return_val;

		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROJECTID);

		//---read project id---
		mpcriteria[0] = EVENT_MAP_PROJECTID;
		CTP_SPI_READ(ts->client, mpcriteria, 3);

		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

		pid = (mpcriteria[2] << 8) + mpcriteria[1];

		/*
		* Parsing Criteria by Novatek PID
		* The string rule is "novatek-mp-criteria-<nvt_pid>"
		* nvt_pid is 2 bytes (show hex).
		*
		* Ex. nvt_pid = 500A
		*	  mpcriteria = "novatek-mp-criteria-500A"
		*/
		snprintf(mpcriteria, sizeof(mpcriteria), "novatek-mp-criteria-%04X", pid);

		return_val = nvt_sec_mp_parse_dt(ts, mpcriteria);
		if (return_val) {
				input_err(true, &ts->client->dev, "%s: failed to parse mp device tree\n", __func__);
		}
	}
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen driver probe function.

return:
	Executive outcomes. 0---succeed. negative---failed
*******************************************************/
static int32_t nvt_ts_probe(struct spi_device *client)
{
	int32_t ret = 0;
#if ((TOUCH_KEY_NUM > 0) || WAKEUP_GESTURE)
	int32_t retry = 0;
#endif
	struct nvt_ts_platdata *platdata;

	input_info(true, &client->dev, "%s : start\n", __func__);

	ts = kzalloc(sizeof(struct nvt_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		input_err(true, &client->dev,"failed to allocated memory for nvt ts data\n");
		return -ENOMEM;
	}

	ts->xbuf = (uint8_t *)kzalloc((NVT_TRANSFER_LEN+1+DUMMY_BYTES), GFP_KERNEL);
	if(ts->xbuf == NULL) {
		input_err(true, &client->dev,"kzalloc for xbuf failed!\n");
		ret = -ENOMEM;
		goto err_malloc_xbuf;
	}

	ts->rbuf = (uint8_t *)kzalloc(NVT_READ_LEN, GFP_KERNEL);
	if(ts->rbuf == NULL) {
		input_err(true, &client->dev, "kzalloc for rbuf failed!\n");
		ret = -ENOMEM;
		goto err_malloc_rbuf;
	}

	if (client->dev.of_node) {
		platdata = devm_kzalloc(&client->dev,sizeof(struct nvt_ts_platdata), GFP_KERNEL);
		if (!platdata) {
			input_err(true, &client->dev, "%s: Failed to allocate platform data\n", __func__);
			goto err_alloc_platdata_failed;
		}

		// for tui
		client->dev.platform_data = platdata;

		ret = nvt_parse_dt(&client->dev);
		if (ret < 0) {
			input_err(true, &client->dev, "%s: Failed to parse dt(%d)\n", __func__, ret);
			goto err_parse_dt_failed;
		}
	} else {
		goto err_parse_dt_failed;
	}

	//---request and config GPIOs---
	ret = nvt_gpio_config(&client->dev, platdata);
	if (ret) {
		input_err(true, &client->dev,"gpio config error!\n");
		goto err_gpio_config_failed;
	}

	ts->client = client;
	ts->platdata = platdata;
	spi_set_drvdata(client, ts);
	
	ts->platdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(ts->platdata->pinctrl))
		input_info(true, &ts->client->dev, "%s: could not get pinctrl\n", __func__);

	pinctrl_configure(ts, true);

	nvt_regulator_init(ts);

	//---prepare for spi parameter---
	if (ts->client->master->flags & SPI_MASTER_HALF_DUPLEX) {
		input_err(true, &client->dev,"Full duplex not supported by master\n");
		ret = -EIO;
		goto err_ckeck_full_duplex;
	}
	ts->client->bits_per_word = 8;
	ts->client->mode = SPI_MODE_0;

	ret = spi_setup(ts->client);
	if (ret < 0) {
		input_err(true, &client->dev,"Failed to perform SPI setup\n");
		goto err_spi_setup;
	}

#if IS_ENABLED(CONFIG_MTK_SPI)
    /* old usage of MTK spi API */
    memcpy(&ts->spi_ctrl, &spi_ctrdata, sizeof(struct mt_chip_conf));
    ts->client->controller_data = (void *)&ts->spi_ctrl;
#endif

#if IS_ENABLED(CONFIG_SPI_MT65XX)
    /* new usage of MTK spi API */
    memcpy(&ts->spi_ctrl, &spi_ctrdata, sizeof(struct mtk_chip_config));
    ts->client->controller_data = (void *)&ts->spi_ctrl;
#endif

	input_info(true, &client->dev,"mode=%d, max_speed_hz=%d\n", ts->client->mode, ts->client->max_speed_hz);

	mutex_init(&ts->lock);
	mutex_init(&ts->xbuf_lock);

	//---eng reset before TP_RESX high
	ts->power_status = POWER_ON_STATUS;
	nvt_eng_reset();

#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif

	// need 10ms delay after POR(power on reset)
	msleep(10);

	//---check chip version trim---
	ret = nvt_ts_check_chip_ver_trim(CHIP_VER_TRIM_ADDR);
	if (ret) {
		input_err(true, &client->dev,"try to check from old chip ver trim address\n");
		ret = nvt_ts_check_chip_ver_trim(CHIP_VER_TRIM_OLD_ADDR);
		if (ret) {
			input_err(true, &client->dev,"chip is not identified\n");
			ret = -EINVAL;
			goto err_chipvertrim_failed;
		}
	}

	//---allocate input device---
	ts->input_dev = devm_input_allocate_device(&client->dev);
	if (ts->input_dev == NULL) {
		input_err(true, &client->dev,"allocate input device failed\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

#if PROXIMITY_FUNCTION
	//---allocate input proximity device---
	ts->input_dev_proximity = input_allocate_device();
	if (ts->input_dev_proximity == NULL) {
		input_err(true, &ts->client->dev, "%s: allocate input proximity device failed\n", __func__);
		ret = -ENOMEM;
		goto err_input_dev_prox_alloc_failed;
	}
#endif

	ts->platdata->max_touch_num = TOUCH_MAX_FINGER_NUM;

#if TOUCH_KEY_NUM > 0
	ts->max_button_num = TOUCH_KEY_NUM;
#endif

	//---set input device info.---
	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#if PROXIMITY_FUNCTION
	ts->input_dev->keybit[BIT_WORD(KEY_INT_CANCEL)] = BIT_MASK(KEY_INT_CANCEL);
#endif
	ts->input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

	input_mt_init_slots(ts->input_dev, ts->platdata->max_touch_num, 0);

#if TOUCH_MAX_FINGER_NUM > 1
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);    //w_major = 255
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);    //w_minor = 255

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->platdata->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->platdata->abs_y_max, 0, 0);
#endif

#if TOUCH_KEY_NUM > 0
	for (retry = 0; retry < ts->max_button_num; retry++) {
		input_set_capability(ts->input_dev, EV_KEY, touch_key_array[retry]);
	}
#endif

#if WAKEUP_GESTURE
	for (retry = 0; retry < (sizeof(gesture_key_array) / sizeof(gesture_key_array[0])); retry++) {
		input_set_capability(ts->input_dev, EV_KEY, gesture_key_array[retry]);
	}
#endif

	sprintf(ts->phys, "input/ts");
	ts->input_dev->name = NVT_TS_NAME;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_SPI;
	
	//---set input proximity device info.---
	ts->input_dev_proximity->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_SW);
	ts->input_dev_proximity->propbit[0] = BIT(INPUT_PROP_DIRECT);
	

	input_set_abs_params(ts->input_dev_proximity, ABS_MT_CUSTOM, 0, 0xFFFFFFFF, 0, 0);

#if PROXIMITY_FUNCTION
	ts->input_dev_proximity->name = "sec_touchproximity";
	ts->input_dev_proximity->phys = ts->phys;
	ts->input_dev_proximity->id.bustype = BUS_SPI;
#endif

	//---register input device---
	ret = input_register_device(ts->input_dev);
	if (ret) {
		input_err(true, &client->dev,"register input device (%s) failed. ret=%d\n", ts->input_dev->name, ret);
		goto err_input_register_device_failed;
	}

#if PROXIMITY_FUNCTION
	//---register input proximity device---
	ret = input_register_device(ts->input_dev_proximity);
	if (ret) {
		input_err(true, &client->dev,"register input proximity device (%s) failed. ret=%d\n", ts->input_dev_proximity->name, ret);
		goto err_input_register_proximity_device_failed;
	}
#endif

	//---set int-pin & request irq---
	client->irq = gpio_to_irq(platdata->irq_gpio);
	if (client->irq) {
		input_info(true, &client->dev,"irq_flags=0x%X\n", ts->platdata->irq_flags);
		ts->irq_enabled = true;
		ret = request_threaded_irq(client->irq, NULL, nvt_ts_work_func,
						ts->platdata->irq_flags, NVT_SPI_NAME, ts);
		if (ret != 0) {
			input_err(true, &client->dev,"request irq failed. ret=%d\n", ret);
			goto err_int_request_failed;
		} else {
			nvt_irq_enable(false);
			input_err(true, &client->dev,"request irq %d succeed\n", client->irq);
		}
	}

	if (ts->platdata->support_ear_detect)
		device_init_wakeup(&ts->input_dev->dev, 1);

	ret = nvt_ts_fw_update_on_probe();
	if (ret) {
		input_err(true, &client->dev,"nvt_ts_fw_update_on_probe failed. ret(%d)\n", ret);
		goto err_fw_update_failed;
	}

	input_info(true, &client->dev,"NVT_TOUCH_ESD_PROTECT is %d\n", NVT_TOUCH_ESD_PROTECT);
#if NVT_TOUCH_ESD_PROTECT
	INIT_DELAYED_WORK(&nvt_esd_check_work, nvt_esd_check_func);
	nvt_esd_check_wq = alloc_workqueue("nvt_esd_check_wq", WQ_MEM_RECLAIM, 1);
	if (!nvt_esd_check_wq) {
		input_err(true, &client->dev,"nvt_esd_check_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_esd_check_wq_failed;
	}
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	INIT_DELAYED_WORK(&ts->work_print_info, nvt_print_info_work);
	INIT_DELAYED_WORK(&ts->work_read_info, nvt_read_info_work);
	schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(100));

	//---set device node---
#if NVT_TOUCH_PROC
	ret = nvt_flash_proc_init();
	if (ret != 0) {
		input_err(true, &client->dev,"nvt flash proc init failed. ret=%d\n", ret);
		goto err_flash_proc_init_failed;
	}
#endif

#if NVT_TOUCH_EXT_PROC
	ret = nvt_extra_proc_init();
	if (ret != 0) {
		input_err(true, &client->dev,"nvt extra proc init failed. ret=%d\n", ret);
		goto err_extra_proc_init_failed;
	}
#endif

	ret = nvt_ts_sec_fn_init(ts);
	if (ret) {
		input_err(true, &client->dev,"failed to init for factory function\n");
		goto err_init_sec_fn;
	}

	device_init_wakeup(&client->dev, true);

	init_completion(&ts->resume_done);
	complete_all(&ts->resume_done);

#if SEC_LPWG_DUMP
	nvt_ts_lpwg_dump_buf_init();
#endif

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
	ts->nb.priority = 1;
	ts->nb.notifier_call = nvt_notifier_call;
	ss_panel_notifier_register(&ts->nb);
#endif

#if (IS_ENABLED(CONFIG_EXYNOS_DPU30) || IS_ENABLED(CONFIG_DRM_SAMSUNG_DPU)) && IS_ENABLED(CONFIG_PANEL_NOTIFY)
	ts->nb.priority = 1;
	ts->nb.notifier_call = nvt_notifier_call;
	panel_notifier_register(&ts->nb);
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	stui_tsp_init(nvt_stui_tsp_enter, nvt_stui_tsp_exit, nvt_stui_tsp_type);
	input_info(true, &client->dev,"secure touch support\n");
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	mutex_init(&ts->secure_lock);
	if (sysfs_create_group(&ts->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &ts->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);

	sec_secure_touch_register(ts, 1, &ts->input_dev->dev.kobj);
#endif

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	INIT_DELAYED_WORK(&ts->work_vbus, nvt_vbus_work);
	vbus_notifier_register(&ts->vbus_nb, tsp_vbus_notification,
						VBUS_NOTIFY_DEV_CHARGER);
#endif

	nvt_ts_mode_read(ts);
	input_info(true, &ts->client->dev, "%s: %s prox(1) & AOT\n",
				__func__, ts->prox_in_aot ? "Support" : "Not support");

	nvt_irq_enable(true);
	input_info(true, &client->dev, "%s : end\n", __func__);

	return 0;

err_init_sec_fn:
	nvt_ts_sec_fn_remove(ts);
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
err_extra_proc_init_failed:
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
err_flash_proc_init_failed:
#endif
#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
err_create_nvt_esd_check_wq_failed:
#endif
err_fw_update_failed:
	if (ts->platdata->support_ear_detect)
		device_init_wakeup(&ts->input_dev->dev, 0);
	free_irq(client->irq, ts);
err_int_request_failed:
#if PROXIMITY_FUNCTION
	input_unregister_device(ts->input_dev_proximity);
	ts->input_dev_proximity = NULL;
err_input_register_proximity_device_failed:
#endif
	input_unregister_device(ts->input_dev);
	ts->input_dev = NULL;
err_input_register_device_failed:
#if PROXIMITY_FUNCTION
	if (ts->input_dev_proximity != NULL) {
		input_free_device(ts->input_dev_proximity);
		ts->input_dev_proximity = NULL;
	}
err_input_dev_prox_alloc_failed:
#endif
	if (ts->input_dev != NULL) {
		input_free_device(ts->input_dev);
		ts->input_dev = NULL;
	}
err_input_dev_alloc_failed:
err_chipvertrim_failed:
	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);
	nvt_gpio_deconfig(ts);
err_spi_setup:
err_ckeck_full_duplex:
	spi_set_drvdata(client, NULL);

	if (ts->platdata->name_lcd_rst)
		regulator_put(ts->regulator_lcd_rst);
	if (ts->platdata->name_lcd_vddi)
		regulator_put(ts->regulator_lcd_vddi);
	if (ts->platdata->name_lcd_bl_en)
		regulator_put(ts->regulator_lcd_bl_en);
	if (ts->platdata->name_lcd_vsp)
		regulator_put(ts->regulator_lcd_vsp);
	if (ts->platdata->name_lcd_vsn)
		regulator_put(ts->regulator_lcd_vsn);
err_gpio_config_failed:
err_parse_dt_failed:
	if (ts->platdata) {
		devm_kfree(&client->dev, ts->platdata);
		ts->platdata = NULL;
	}
err_alloc_platdata_failed:
	if (ts->rbuf) {
		kfree(ts->rbuf);
		ts->rbuf = NULL;
	}
err_malloc_rbuf:
	if (ts->xbuf) {
		kfree(ts->xbuf);
		ts->xbuf = NULL;
	}
err_malloc_xbuf:
	if (ts) {
		kfree(ts);
		ts = NULL;
	}
	input_err(true, &client->dev, "%s : end - fail unload driver\n", __func__);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen driver release function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_ts_remove(struct spi_device *client)
{
	input_info(true, &client->dev, "%s : Removing driver...\n", __func__);

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	cancel_delayed_work_sync(&ts->work_vbus);
#endif
	cancel_delayed_work_sync(&ts->work_print_info);

	nvt_ts_sec_fn_remove(ts);

#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_unregister(&ts->vbus_nb);
#endif

#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
#endif

	cancel_delayed_work_sync(&ts->work_read_info);

	if (ts->platdata->support_ear_detect)
		device_init_wakeup(&ts->input_dev->dev, 0);

	nvt_irq_enable(false);
	pinctrl_configure(ts, false);

	free_irq(client->irq, ts);

	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);

	nvt_gpio_deconfig(ts);

	if (ts->platdata->name_lcd_rst)
		regulator_put(ts->regulator_lcd_rst);
	if (ts->platdata->name_lcd_vddi)
		regulator_put(ts->regulator_lcd_vddi);
	if (ts->platdata->name_lcd_bl_en)
		regulator_put(ts->regulator_lcd_bl_en);
	if (ts->platdata->name_lcd_vsp)
		regulator_put(ts->regulator_lcd_vsp);
	if (ts->platdata->name_lcd_vsn)
		regulator_put(ts->regulator_lcd_vsn);

	if (ts->input_dev) {
		input_unregister_device(ts->input_dev);
		ts->input_dev = NULL;
	}
	
	if (ts->input_dev_proximity) {
		input_mt_destroy_slots(ts->input_dev_proximity);
		input_unregister_device(ts->input_dev_proximity);
		ts->input_dev_proximity = NULL;
	}

	spi_set_drvdata(client, NULL);

	if (ts->platdata) {
		devm_kfree(&client->dev, ts->platdata);
		ts->platdata = NULL;
	}
	if (ts->rbuf) {
		kfree(ts->rbuf);
		ts->rbuf = NULL;
	}
	if (ts->xbuf) {
		kfree(ts->xbuf);
		ts->xbuf = NULL;
	}
	if (ts) {
		kfree(ts);
		ts = NULL;
	}

	return 0;
}

static void nvt_ts_shutdown(struct spi_device *client)
{
	input_info(true, &client->dev, "%s : Shutdown driver...\n", __func__);

	if (ts == NULL) {
		input_err(true, &client->dev, "%s : tsp data null\n", __func__);
		return;
	}

	ts->shutdown_called = true;

	if (ts->platdata->support_ear_detect)
		device_init_wakeup(&ts->input_dev->dev, 0);

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	cancel_delayed_work_sync(&ts->work_vbus);
#endif
	cancel_delayed_work_sync(&ts->work_print_info);

#if NVT_TOUCH_ESD_PROTECT
	if (nvt_esd_check_wq) {
		cancel_delayed_work_sync(&nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(nvt_esd_check_wq);
		nvt_esd_check_wq = NULL;
	}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	cancel_delayed_work_sync(&ts->work_read_info);

	ts->power_status = POWER_OFF_STATUS;

	nvt_irq_enable(false);
	pinctrl_configure(ts, false);

	nvt_ts_sec_fn_remove(ts);

#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif

}


int nvt_ts_lcd_reset_ctrl(bool on)
{
	int retval;
	static bool enabled;

	if (enabled == on)
		return 0;

	if (ts->platdata->name_lcd_rst == NULL) {
		input_err(true, &ts->client->dev, "%s: name_lcd_rst is null\n", __func__);
		return 0;
	}

	if (on) {
		retval = regulator_enable(ts->regulator_lcd_rst);
		if (retval) {
			input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_rst: %d\n", __func__, retval);
			return retval;
		}

	} else {
		regulator_disable(ts->regulator_lcd_rst);
	}

	enabled = on;

	input_info(true, &ts->client->dev, "%s %d done\n", __func__, on);

	return 0;
}

int nvt_ts_lcd_power_ctrl(bool on)
{
	int retval;
	static bool enabled;

	if (enabled == on)
		return 0;

	if (on) {
		if (ts->platdata->name_lcd_vddi) {
			retval = regulator_enable(ts->regulator_lcd_vddi);
			if (retval) {
				input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_vddi: %d\n", __func__, retval);
				return retval;
			}
		}

		if (ts->platdata->name_lcd_bl_en) {
			retval = regulator_enable(ts->regulator_lcd_bl_en);
			if (retval) {
				input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_bl_en: %d\n", __func__, retval);
				return retval;
			}
		}

		if (ts->platdata->name_lcd_vsp) {
			retval = regulator_enable(ts->regulator_lcd_vsp);
			if (retval) {
				input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_vsp: %d\n", __func__, retval);
				return retval;
			}
		}
		if (ts->platdata->name_lcd_vsn) {
			retval = regulator_enable(ts->regulator_lcd_vsn);
			if (retval) {
				input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_vsn: %d\n", __func__, retval);
				return retval;
			}
		}
	} else {
		if (ts->platdata->name_lcd_vddi)
			regulator_disable(ts->regulator_lcd_vddi);
		if (ts->platdata->name_lcd_bl_en)
			regulator_disable(ts->regulator_lcd_bl_en);
		if (ts->platdata->name_lcd_vsp)
			regulator_disable(ts->regulator_lcd_vsp);
		if (ts->platdata->name_lcd_vsn)
			regulator_disable(ts->regulator_lcd_vsn);
	}

	enabled = on;

	input_info(true, &ts->client->dev, "%s %d done\n", __func__, on);

	return 0;
}

bool nvt_ts_lcd_power_check()
{
	int enabled_count = 0, regulator_count = 0;

	if (ts->platdata->name_lcd_vddi) {
		regulator_count++;
		enabled_count += regulator_is_enabled(ts->regulator_lcd_vddi);
	}

	if (ts->platdata->name_lcd_bl_en) {
		regulator_count++;
		enabled_count += regulator_is_enabled(ts->regulator_lcd_bl_en);
	}

	if (ts->platdata->name_lcd_vsp) {
		regulator_count++;
		enabled_count += regulator_is_enabled(ts->regulator_lcd_vsp);
	}

	if (ts->platdata->name_lcd_vsn) {
		regulator_count++;
		enabled_count += regulator_is_enabled(ts->regulator_lcd_vsn);
	}

	if (regulator_count > 0 && enabled_count == 0) {
		input_info(true, &ts->client->dev, "%s : regulator_count (%d), enabled_count zero(lcd off)\n",
					__func__, regulator_count);
		return false;
	} else
		return true;
}

static void nvt_ts_set_lp_mode(struct nvt_ts_data *ts)
{
	uint8_t buf[4] = {0};

	nvt_ts_lcd_power_ctrl(true);
	nvt_ts_lcd_reset_ctrl(true);

	nvt_irq_enable(false);

	mutex_lock(&ts->lock);
	ts->power_status = LP_MODE_STATUS;
	/* LPWG enter */
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = LPWG_ENTER;
	CTP_SPI_WRITE(ts->client, buf, 2);
	mutex_unlock(&ts->lock);

	nvt_irq_enable(true);
	enable_irq_wake(ts->client->irq);

	input_info(true, &ts->client->dev, "%s: called(%d)\n", __func__, ts->lowpower_mode);
}
static void nvt_ts_set_ed_mode(struct nvt_ts_data *ts)
{
	nvt_ts_lcd_power_ctrl(true);
	nvt_ts_lcd_reset_ctrl(true);

	mutex_lock(&ts->lock);
	ts->power_status = LP_MODE_STATUS;
	mutex_unlock(&ts->lock);

	enable_irq_wake(ts->client->irq);

	input_info(true, &ts->client->dev, "%s: called(%d)\n", __func__, ts->lowpower_mode);
}
static void nvt_ts_set_icoff_mode(struct nvt_ts_data *ts)
{
	nvt_irq_enable(false);

	mutex_lock(&ts->lock);
	pinctrl_configure(ts, false);
	ts->power_status = POWER_OFF_STATUS;
	mutex_unlock(&ts->lock);

	input_info(true, &ts->client->dev, "%s: power off %d\n", __func__, ts->lowpower_mode);
}

/*******************************************************
Description:
	Novatek touchscreen driver suspend function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
int32_t nvt_ts_suspend(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
	int enter_force_ed_mode = 0;
#if SEC_LPWG_DUMP
	u8 lpwg_dump[5] = {0x3, 0x0, 0x0, 0x0, 0x0};
#endif

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	cancel_delayed_work_sync(&ts->work_vbus);
#endif
	cancel_delayed_work_sync(&ts->work_read_info);

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return 0;
	}

	if (ts->power_status == POWER_OFF_STATUS || ts->power_status == LP_MODE_STATUS) {
		input_info(true, &ts->client->dev, "Touch is already suspend(%d)\n", ts->power_status);
		return 0;
	}

#if NVT_TOUCH_ESD_PROTECT
	input_info(true, &ts->client->dev,"cancel delayed work sync\n");
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	input_info(true, &ts->client->dev, "%s : ed:%d, lp:%d, prox:%ld, test:%d, prox_in_aot:%d\n",
				__func__, ts->ear_detect_mode, ts->lowpower_mode, ts->prox_power_off,
				ts->lcdoff_test, ts->prox_in_aot);

	if (ts->lowpower_mode && (ts->prox_power_off || ts->ear_detect_mode))
		enter_force_ed_mode = 1;	// for ed

	/* against ed mode and suppport prox_in_aot only */
	if ((!ts->prox_power_off && ts->ear_detect_mode) && ts->prox_in_aot)
		enter_force_ed_mode = 0;	// for prox in aot feature

	if ((ts->lowpower_mode || ts->lcdoff_test) && enter_force_ed_mode == 0) {
		nvt_ts_set_lp_mode(ts);
	} else if (ts->ear_detect_mode) {
		nvt_ts_set_ed_mode(ts);
	} else {
		nvt_ts_set_icoff_mode(ts);
	}

#if SEC_LPWG_DUMP
	if (ts->power_status == LP_MODE_STATUS) {
		nvt_ts_lpwg_dump_buf_write(lpwg_dump);
	}
#endif

	nvt_ts_release_all_finger(ts);

	msleep(50);

	cancel_delayed_work(&ts->work_print_info);
	nvt_print_info();

	input_info(true, &ts->client->dev, "%s : end\n", __func__);

	return 0;
}

void nvt_ts_early_resume(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	input_info(true, &ts->client->dev, "%s : start(%d)\n", __func__, ts->power_status);

	cancel_delayed_work_sync(&ts->work_read_info);

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return;
	}

	if (ts->power_status == LP_MODE_STATUS) {
		if (device_may_wakeup(&ts->client->dev))
			disable_irq_wake(ts->client->irq);

#if SEC_LPWG_DUMP
		input_info(true, &ts->client->dev, "%s : read lpgw logs start\n", __func__);
		nvt_ts_lpwg_dump();
		input_info(true, &ts->client->dev, "%s : read lpgw logs end\n", __func__);
#endif

		nvt_irq_enable(false);

		mutex_lock(&ts->lock);
		ts->power_status = LP_MODE_EXIT;
		nvt_ts_lcd_reset_ctrl(false);
		mutex_unlock(&ts->lock);
	}
}

/*******************************************************
Description:
	Novatek touchscreen driver resume function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
int32_t nvt_ts_resume(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
#if SEC_LPWG_DUMP
	u8 lpwg_dump[5] = {0x7, 0x0, 0x0, 0x0, 0x0};
#endif

	ts->lcd_esd_recovery = 0;
	cancel_delayed_work_sync(&ts->work_read_info);

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return 0;
	}

	if (ts->power_status == POWER_ON_STATUS) {
		input_info(true, &ts->client->dev, "%s: Touch is already resume\n", __func__);
		return 0;
	}

	mutex_lock(&ts->lock);

	if (ts->power_status == LP_MODE_EXIT) {
		nvt_ts_lcd_power_ctrl(false);
#if SEC_LPWG_DUMP
		nvt_ts_lpwg_dump_buf_write(lpwg_dump);
#endif
	} else {
		pinctrl_configure(ts, true);
	}

	ts->prox_power_off = 0;
	ts->power_status = POWER_ON_STATUS;

	// please make sure display reset(RESX) sequence and mipi dsi cmds sent before this
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif

	if (nvt_update_firmware(ts->platdata->firmware_name)) {
		input_err(true, &ts->client->dev,"download firmware failed, ignore check fw state\n");
	} else {
		nvt_check_fw_reset_state(RESET_STATE_REK);
	}

	nvt_ts_mode_restore(ts);

	if (ts->ear_detect_mode) {
		set_ear_detect(ts, ts->ear_detect_mode, false);
	} else {
		if (ts->ed_reset_flag) {
			input_info(true, &ts->client->dev, "%s : set ed on & off\n", __func__);
			set_ear_detect(ts, 1, false);
			set_ear_detect(ts, 0, false);
		}
	}
	ts->ed_reset_flag = false;

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	mutex_unlock(&ts->lock);

#if SEC_FW_STATUS
	nvt_ts_ic_status(NULL, true);
#endif

	nvt_irq_enable(true);

	cancel_delayed_work(&ts->work_print_info);
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	cancel_delayed_work(&ts->work_vbus);
#endif
	ts->print_info_cnt_open = 0;
	ts->print_info_cnt_release = 0;
	schedule_work(&ts->work_print_info.work);

	input_info(true, &ts->client->dev, "%s : end\n", __func__);

	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int nvt_pm_suspend(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	reinit_completion(&ts->resume_done);
	input_dbg(true, &ts->client->dev, "%s : called!\n", __func__);

	return 0;
}

static int nvt_pm_resume(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	complete_all(&ts->resume_done);
	input_dbg(true, &ts->client->dev, "%s : called!\n", __func__);

	return 0;
}
#endif

static const struct spi_device_id nvt_ts_id[] = {
	{ NVT_SPI_NAME, 0 },
	{ }
};

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id nvt_match_table[] = {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	{ .compatible = "nvt_ts_spi",},
#else
	{ .compatible = "novatek,NVT-ts-spi",},
#endif
	{ },
};
#endif
#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops nvt_dev_pm_ops = {
	.suspend = nvt_pm_suspend,
	.resume = nvt_pm_resume,
};
#endif

static struct spi_driver nvt_spi_driver = {
	.probe		= nvt_ts_probe,
	.remove		= nvt_ts_remove,
	.shutdown	= nvt_ts_shutdown,
	.id_table	= nvt_ts_id,
	.driver = {
		.name	= NVT_SPI_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = nvt_match_table,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &nvt_dev_pm_ops,
#endif

	},
};

/*******************************************************
Description:
	Driver Install function.

return:
	Executive Outcomes. 0---succeed. not 0---failed.
********************************************************/
static int32_t __init nvt_driver_init(void)
{
	int32_t ret = 0;

	pr_info("[sec_input] %s : start\n", __func__);

	//---add spi driver---
	ret = spi_register_driver(&nvt_spi_driver);
	if (ret) {
		pr_err("[sec_input] failed to add spi driver");
		goto err_driver;
	}

	pr_info("[sec_input] %s : finished\n", __func__);

err_driver:
	return ret;
}

/*******************************************************
Description:
	Driver uninstall function.

return:
	n.a.
********************************************************/
static void __exit nvt_driver_exit(void)
{
	spi_unregister_driver(&nvt_spi_driver);
}

//late_initcall(nvt_driver_init);
module_init(nvt_driver_init);
module_exit(nvt_driver_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
