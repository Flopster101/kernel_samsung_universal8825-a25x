/* drivers/input/sec_input/slsi/slsi_fw.c
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "slsi_dev.h"
#include "slsi_reg.h"

#if IS_ENABLED(CONFIG_SPU_VERIFY)
#define SUPPORT_FW_SIGNED
#endif

#ifdef SUPPORT_FW_SIGNED
#include <linux/spu-verify.h>
#endif

#define SLSI_TS_FW_BLK_SIZE		256

enum {
	TSP_BUILT_IN = 0,
	TSP_SDCARD,
	NONE,
	TSP_SPU,
	TSP_VERIFICATION,
};

int slsi_ts_read_calibration_report(struct slsi_ts_data *ts)
{
	int ret;
	u8 buf[5] = { 0 };

	buf[0] = SLSI_TS_READ_CALIBRATION_REPORT;

	ret = ts->slsi_ts_i2c_read(ts, buf[0], &buf[1], 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read, %d\n", __func__, ret);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: count:%d, pass count:%d, fail count:%d, status:0x%X\n",
			__func__, buf[1], buf[2], buf[3], buf[4]);

	return buf[4];
}

static int slsi_ts_enter_fw_mode(struct slsi_ts_data *ts)
{
	int ret;
	u8 fw_update_mode_passwd[] = {0x55, 0xAC};
	u8 fw_status;
	u8 id[3];

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_ENTER_FW_MODE, fw_update_mode_passwd, sizeof(fw_update_mode_passwd));
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: write fail, enter_fw_mode\n", __func__);
		return 0;
	}

	input_info(true, &ts->client->dev, "%s: write ok, enter_fw_mode - 0x%x 0x%x 0x%x\n",
			__func__, SLSI_TS_CMD_ENTER_FW_MODE, fw_update_mode_passwd[0], fw_update_mode_passwd[1]);
	sec_delay(30);

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_BOOT_STATUS, &fw_status, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read fail, read_boot_status\n", __func__);
		return 0;
	}
	if (fw_status != SLSI_TS_STATUS_BOOT_MODE) {
		input_err(true, &ts->client->dev, "%s: enter fail! read_boot_status = 0x%x\n", __func__, fw_status);
		return 0;
	}

	input_info(true, &ts->client->dev, "%s: Success! read_boot_status = 0x%x\n", __func__, fw_status);

	sec_delay(10);

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_ID, id, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read id fail\n", __func__);
		return 0;
	}

	ts->boot_ver[0] = id[0];
	ts->boot_ver[1] = id[1];
	ts->boot_ver[2] = id[2];

	ts->flash_page_size = SLSI_TS_FW_BLK_SIZE_DEFAULT;

	input_info(true, &ts->client->dev, "%s: read_boot_id = %02X%02X%02X\n", __func__, id[0], id[1], id[2]);

	return 1;
}

static int slsi_ts_sw_reset(struct slsi_ts_data *ts)
{
	int ret;

	ret = slsi_ts_wait_for_ready(ts, SLSI_TS_CMD_SW_RESET, NULL, 0, 100);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: time out\n", __func__);
		return 0;
	}

	input_info(true, &ts->client->dev, "%s: sw_reset\n", __func__);

	/* Sense_on */
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SENSE_ON, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: write fail, Sense_on\n", __func__);
		return 0;
	}

	return ret;
}

static void slsi_ts_save_version_of_bin(struct slsi_ts_data *ts, const fw_header *fw_hd)
{
	ts->plat_data->img_version_of_bin[3] = ((fw_hd->img_ver >> 24) & 0xff);
	ts->plat_data->img_version_of_bin[2] = ((fw_hd->img_ver >> 16) & 0xff);
	ts->plat_data->img_version_of_bin[1] = ((fw_hd->img_ver >> 8) & 0xff);
	ts->plat_data->img_version_of_bin[0] = ((fw_hd->img_ver >> 0) & 0xff);

	ts->plat_data->core_version_of_bin[3] = ((fw_hd->fw_ver >> 24) & 0xff);
	ts->plat_data->core_version_of_bin[2] = ((fw_hd->fw_ver >> 16) & 0xff);
	ts->plat_data->core_version_of_bin[1] = ((fw_hd->fw_ver >> 8) & 0xff);
	ts->plat_data->core_version_of_bin[0] = ((fw_hd->fw_ver >> 0) & 0xff);

	ts->plat_data->config_version_of_bin[3] = ((fw_hd->para_ver >> 24) & 0xff);
	ts->plat_data->config_version_of_bin[2] = ((fw_hd->para_ver >> 16) & 0xff);
	ts->plat_data->config_version_of_bin[1] = ((fw_hd->para_ver >> 8) & 0xff);
	ts->plat_data->config_version_of_bin[0] = ((fw_hd->para_ver >> 0) & 0xff);

	input_info(true, &ts->client->dev, "%s: img_ver of bin = %x.%x.%x.%x\n", __func__,
			ts->plat_data->img_version_of_bin[0],
			ts->plat_data->img_version_of_bin[1],
			ts->plat_data->img_version_of_bin[2],
			ts->plat_data->img_version_of_bin[3]);

	input_info(true, &ts->client->dev, "%s: core_ver of bin = %x.%x.%x.%x\n", __func__,
			ts->plat_data->core_version_of_bin[0],
			ts->plat_data->core_version_of_bin[1],
			ts->plat_data->core_version_of_bin[2],
			ts->plat_data->core_version_of_bin[3]);

	input_info(true, &ts->client->dev, "%s: config_ver of bin = %x.%x.%x.%x\n", __func__,
			ts->plat_data->config_version_of_bin[0],
			ts->plat_data->config_version_of_bin[1],
			ts->plat_data->config_version_of_bin[2],
			ts->plat_data->config_version_of_bin[3]);
}

static int slsi_ts_save_version_of_ic(struct slsi_ts_data *ts)
{
	u8 img_ver[4] = {0,};
	u8 core_ver[4] = {0,};
	u8 config_ver[4] = {0,};
	int ret;

	/* Image ver */
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_IMG_VERSION, img_ver, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Image version read error\n", __func__);
		return -EIO;
	}
	input_info(true, &ts->client->dev, "%s: IC Image version info : %x.%x.%x.%x\n",
			__func__, img_ver[0], img_ver[1], img_ver[2], img_ver[3]);

	ts->plat_data->img_version_of_ic[0] = img_ver[0];
	ts->plat_data->img_version_of_ic[1] = img_ver[1];
	ts->plat_data->img_version_of_ic[2] = img_ver[2];
	ts->plat_data->img_version_of_ic[3] = img_ver[3];

	/* Core ver */
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_FW_VERSION, core_ver, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: core version read error\n", __func__);
		return -EIO;
	}
	input_info(true, &ts->client->dev, "%s: IC Core version info : %x.%x.%x.%x\n",
			__func__, core_ver[0], core_ver[1], core_ver[2], core_ver[3]);

	ts->plat_data->core_version_of_ic[0] = core_ver[0];
	ts->plat_data->core_version_of_ic[1] = core_ver[1];
	ts->plat_data->core_version_of_ic[2] = core_ver[2];
	ts->plat_data->core_version_of_ic[3] = core_ver[3];

	/* Config ver */
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_PARA_VERSION, config_ver, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: config version read error\n", __func__);
		return -EIO;
	}
	input_info(true, &ts->client->dev, "%s: IC config version info : %x.%x.%x.%x\n",
			__func__, config_ver[0], config_ver[1], config_ver[2], config_ver[3]);

	ts->plat_data->config_version_of_ic[0] = config_ver[0];
	ts->plat_data->config_version_of_ic[1] = config_ver[1];
	ts->plat_data->config_version_of_ic[2] = config_ver[2];
	ts->plat_data->config_version_of_ic[3] = config_ver[3];

	return 1;
}

int slsi_ts_check_firmware_version(struct slsi_ts_data *ts, const u8 *fw_info)
{
	fw_header *fw_hd;
	u8 buff[1];
	int i;
	int ret;
	/*
	 * slsi_ts_check_firmware_version
	 * return value = 1 : firmware download needed,
	 * return value = 0 : skip firmware download
	 */

	fw_hd = (fw_header *)fw_info;

	slsi_ts_save_version_of_bin(ts, fw_hd);

	/* firmware download if READ_BOOT_STATUS = 0x10 */
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_BOOT_STATUS, buff, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: fail to read BootStatus\n", __func__);
		return -EIO;
	}

	if (buff[0] == SLSI_TS_STATUS_BOOT_MODE) {
		input_err(true, &ts->client->dev,
				"%s: ReadBootStatus = 0x%x, Firmware download Start!\n",
				__func__, buff[0]);
		return 1;
	}

	ret = slsi_ts_save_version_of_ic(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to read ic version\n", __func__);
		return -EIO;
	}

	/* check f/w version
	 * ver[0] : IC version
	 * ver[1] : Project version
	 * ver[2] : Panel infomation
	 */
	for (i = 0; i < 3; i++) {
		if (ts->plat_data->img_version_of_ic[i] != ts->plat_data->img_version_of_bin[i]) {
			if (ts->plat_data->bringup == 3) {
				input_err(true, &ts->client->dev, "%s: bringup. force update\n", __func__);
				return 1;
			} else if (i == 2) {
				return 0;
			}

			input_err(true, &ts->client->dev, "%s: not matched version info\n", __func__);
			return 1;
		}
	}

	if (ts->plat_data->img_version_of_ic[3] < ts->plat_data->img_version_of_bin[3])
		return 1;

	return 0;
}

static u8 slsi_ts_checksum(u8 *data, int offset, int size)
{
	int i;
	u8 checksum = 0;

	for (i = 0; i < size; i++)
		checksum += data[i + offset];

	return checksum;
}

static int slsi_ts_flashpageerase(struct slsi_ts_data *ts, u32 page_idx, u32 page_num)
{
	int ret;
	u8 tCmd[6];

	tCmd[0] = SLSI_TS_CMD_FLASH_ERASE;
	tCmd[1] = (u8)((page_idx >> 8) & 0xFF);
	tCmd[2] = (u8)((page_idx >> 0) & 0xFF);
	tCmd[3] = (u8)((page_num >> 8) & 0xFF);
	tCmd[4] = (u8)((page_num >> 0) & 0xFF);
	tCmd[5] = slsi_ts_checksum(tCmd, 1, 4);

	ret = ts->slsi_ts_i2c_write_burst(ts, tCmd, 6);

	return ret;
}

static int slsi_ts_flashpagewrite(struct slsi_ts_data *ts, u32 page_idx, u8 *page_data)
{
	int ret;
	u8 tCmd[1 + 2 + SLSI_TS_FW_BLK_SIZE_MAX + 1];
	int flash_page_size = (int)ts->flash_page_size;

	tCmd[0] = 0xD9;
	tCmd[1] = (u8)((page_idx >> 8) & 0xFF);
	tCmd[2] = (u8)((page_idx >> 0) & 0xFF);

	memcpy(&tCmd[3], page_data, flash_page_size);
	tCmd[1 + 2 + flash_page_size] = slsi_ts_checksum(tCmd, 1, 2 + flash_page_size);

	ret = ts->slsi_ts_i2c_write_burst(ts, tCmd, 1 + 2 + flash_page_size + 1);
	return ret;
}

static int slsi_ts_limited_flashpagewrite(struct slsi_ts_data *ts, u32 page_idx, u8 *page_data)
{
	int ret = 0;
	u8 *tCmd;
	u8 copy_data[3 + SLSI_TS_FW_BLK_SIZE_MAX];
	int copy_left = (int)ts->flash_page_size + 3;
	int copy_size = 0;
	int copy_max = ts->plat_data->i2c_burstmax - 1;
	int flash_page_size = (int)ts->flash_page_size;

	copy_data[0] = (u8)((page_idx >> 8) & 0xFF);	/* addH */
	copy_data[1] = (u8)((page_idx >> 0) & 0xFF);	/* addL */

	memcpy(&copy_data[2], page_data, flash_page_size);	/* DATA */
	copy_data[2 + flash_page_size] = slsi_ts_checksum(copy_data, 0, 2 + flash_page_size);	/* CS */

	while (copy_left > 0) {
		int copy_cur = (copy_left > copy_max) ? copy_max : copy_left;

		tCmd = kzalloc(copy_cur + 1, GFP_KERNEL);
		if (!tCmd)
			goto err_write;

		if (copy_size == 0)
			tCmd[0] = SLSI_TS_CMD_FLASH_WRITE;
		else
			tCmd[0] = SLSI_TS_CMD_FLASH_PADDING;

		memcpy(&tCmd[1], &copy_data[copy_size], copy_cur);

		ret = ts->slsi_ts_i2c_write_burst(ts, tCmd, 1 + copy_cur);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed, ret:%d\n", __func__, ret);
			kfree(tCmd);
			return ret;
		}

		copy_size += copy_cur;
		copy_left -= copy_cur;
		kfree(tCmd);
	}
	return ret;

err_write:
	input_err(true, &ts->client->dev,
			"%s: failed to alloc.\n", __func__);
	return -ENOMEM;
}

static int slsi_ts_flashwrite(struct slsi_ts_data *ts, u32 mem_addr, u8 *mem_data, u32 mem_size)
{
	int ret;
	u32 page_idx;
	u32 size_copy;
	u32 flash_page_size;
	u32 page_idx_start;
	u32 page_idx_end;
	u32 page_num;
	u8 page_buf[SLSI_TS_FW_BLK_SIZE_MAX];

	if (mem_size == 0)
		return 0;

	flash_page_size = ts->flash_page_size;
	page_idx_start = mem_addr / flash_page_size;
	page_idx_end = (mem_addr + mem_size - 1) / flash_page_size;
	page_num = page_idx_end - page_idx_start + 1;

	ret = slsi_ts_flashpageerase(ts, page_idx_start, page_num);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: fw erase failed, mem_addr= %08X, pagenum = %d\n",
				__func__, mem_addr, page_num);
		return -EIO;
	}

	sec_delay(page_num + 10);

	size_copy = mem_size % flash_page_size;
	if (size_copy == 0)
		size_copy = flash_page_size;

	memset(page_buf, 0, flash_page_size);

	for (page_idx = page_num - 1;; page_idx--) {
		memcpy(page_buf, mem_data + (page_idx * flash_page_size), size_copy);
		if (ts->boot_ver[0] == 0xB2) {
			ret = slsi_ts_flashpagewrite(ts, (page_idx + page_idx_start), page_buf);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "%s: fw write failed, page_idx = %u\n", __func__, page_idx);
				goto err;
			}
		} else {
			ret = slsi_ts_limited_flashpagewrite(ts, (page_idx + page_idx_start), page_buf);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "%s: fw write failed, page_idx = %u\n", __func__, page_idx);
				goto err;
			}
		}

		size_copy = flash_page_size;
		sec_delay(5);

		if (page_idx == 0) /* end condition (page_idx >= 0)   page_idx type unsinged int */
			break;
	}

	return mem_size;

err:
	return -EIO;
}

#if (!IS_ENABLED(CONFIG_SEC_FACTORY))
static int slsi_ts_memoryblockread(struct slsi_ts_data *ts, u32 mem_addr, int mem_size, u8 *buf)
{
	int ret;
	u8 cmd[5];
	u8 *data;

	if (mem_size >= 64 * 1024) {
		input_err(true, &ts->client->dev,
				"%s: mem size over 64K\n", __func__);
		return -EIO;
	}

	cmd[0] = (u8)SLSI_TS_CMD_FLASH_READ_ADDR;
	cmd[1] = (u8)((mem_addr >> 24) & 0xff);
	cmd[2] = (u8)((mem_addr >> 16) & 0xff);
	cmd[3] = (u8)((mem_addr >> 8) & 0xff);
	cmd[4] = (u8)((mem_addr >> 0) & 0xff);

	ret = ts->slsi_ts_i2c_write_burst(ts, cmd, 5);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: send command failed, %02X\n", __func__, cmd[0]);
		return -EIO;
	}

	udelay(10);
	cmd[0] = (u8)SLSI_TS_CMD_FLASH_READ_SIZE;
	cmd[1] = (u8)((mem_size >> 8) & 0xff);
	cmd[2] = (u8)((mem_size >> 0) & 0xff);

	ret = ts->slsi_ts_i2c_write_burst(ts, cmd, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: send command failed, %02X\n", __func__, cmd[0]);
		return -EIO;
	}

	udelay(10);
	cmd[0] = (u8)SLSI_TS_CMD_FLASH_READ_DATA;

	data = buf;

	ret = ts->slsi_ts_i2c_read(ts, cmd[0], data, mem_size);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: memory read failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int slsi_ts_memoryread(struct slsi_ts_data *ts, u32 mem_addr, u8 *mem_data, u32 mem_size)
{
	int ret;
	int retry = 3;
	int read_size = 0;
	int unit_size;
	int max_size = 1024;
	int read_left = (int)mem_size;
	u8 *tmp_data;

	tmp_data = kmalloc(max_size, GFP_KERNEL);
	if (!tmp_data) {
		input_err(true, &ts->client->dev,
				"%s: failed to kmalloc\n", __func__);
		return -ENOMEM;
	}

	while (read_left > 0) {
		unit_size = (read_left > max_size) ? max_size : read_left;
		retry = 3;
		do {
			ret = slsi_ts_memoryblockread(ts, mem_addr, unit_size, tmp_data);
			if (retry-- == 0) {
				input_err(true, &ts->client->dev,
						"%s: fw read fail mem_addr=%08X,unit_size=%d\n",
						__func__, mem_addr, unit_size);
				kfree(tmp_data);
				return -1;
			}

			memcpy(mem_data + read_size, tmp_data, unit_size);
		} while (ret < 0);

		mem_addr += unit_size;
		read_size += unit_size;
		read_left -= unit_size;
	}

	kfree(tmp_data);

	return read_size;
}
#endif

static int slsi_ts_chunk_update(struct slsi_ts_data *ts, u32 addr, u32 size, u8 *data)
{
	u32 fw_size;
	u32 write_size;
#if (!IS_ENABLED(CONFIG_SEC_FACTORY))
	u8 *mem_rb;
#endif
	int ret = 0;

	fw_size = size;

	write_size = slsi_ts_flashwrite(ts, addr, data, fw_size);
	if (write_size != fw_size) {
		input_err(true, &ts->client->dev, "%s: fw write failed\n", __func__);
		ret = -1;
		goto err_write_fail;
	}

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	input_info(true, &ts->client->dev, "%s: verify skip(%d)\n", __func__, ret);
#else
	mem_rb = vzalloc(fw_size);
	if (!mem_rb) {
		input_err(true, &ts->client->dev, "%s: vzalloc failed\n", __func__);
		ret = -1;
		goto err_write_fail;
	}

	if (slsi_ts_memoryread(ts, addr, mem_rb, fw_size) >= 0) {
		u32 ii;

		for (ii = 0; ii < fw_size; ii++) {
			if (data[ii] != mem_rb[ii])
				break;
		}

		if (fw_size != ii) {
			input_err(true, &ts->client->dev, "%s: fw verify fail\n", __func__);
			ret = -1;
			goto out;
		}
	} else {
		ret = -1;
		goto out;
	}

	input_info(true, &ts->client->dev, "%s: verify done(%d)\n", __func__, ret);

out:
	vfree(mem_rb);
#endif
err_write_fail:
	sec_delay(10);

	return ret;
}

static int slsi_ts_firmware_update(struct slsi_ts_data *ts, const u8 *data, int bl_update)
{
	int i;
	int ret;
	fw_header *fw_hd;
	fw_chunk *fw_ch;
	u8 fw_status = 0;
	u8 *fd = (u8 *)data;
	u8 tBuff[3];

	/* Check whether CRC is appended or not.
	 * Enter Firmware Update Mode
	 */
	if (!slsi_ts_enter_fw_mode(ts)) {
		input_err(true, &ts->client->dev, "%s: firmware mode failed\n", __func__);
		return -EPERM;
	}

	if (bl_update && (ts->boot_ver[0] == 0xB4)) {
		input_info(true, &ts->client->dev, "%s: bootloader is up to date\n", __func__);
		return 0;
	}

	input_info(true, &ts->client->dev, "%s: firmware update\n", __func__);

	fw_hd = (fw_header *)fd;
	fd += sizeof(fw_header);

	if (fw_hd->signature != SLSI_TS_FW_HEADER_SIGN) {
		input_err(true, &ts->client->dev, "%s: firmware header error = %08X\n", __func__, fw_hd->signature);
		return -EINVAL;
	}

	input_err(true, &ts->client->dev, "%s: num_chunk : %d\n", __func__, fw_hd->num_chunk);

	for (i = 0; i < fw_hd->num_chunk; i++) {
		fw_ch = (fw_chunk *)fd;

		input_err(true, &ts->client->dev, "%s: [%d] 0x%08X, 0x%08X, 0x%08X, 0x%08X\n", __func__, i,
				fw_ch->signature, fw_ch->addr, fw_ch->size, fw_ch->reserved);

		if (fw_ch->signature != SLSI_TS_FW_CHUNK_SIGN) {
			input_err(true, &ts->client->dev, "%s: firmware chunk error = %08X\n", __func__, fw_ch->signature);
			return -EINVAL;
		}
		fd += sizeof(fw_chunk);
		ret = slsi_ts_chunk_update(ts, fw_ch->addr, fw_ch->size, fd);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: firmware chunk write failed, addr=%08X, size = %d\n", __func__, fw_ch->addr, fw_ch->size);
			return ret;
		}
		fd += fw_ch->size;
	}

	slsi_ts_sw_reset(ts);

	if (!bl_update) {
		/* Sense_on */
		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SENSE_ON, NULL, 0);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: write fail, Sense_on\n", __func__);
			return -EIO;
		}

		if (ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_BOOT_STATUS, &fw_status, 1) < 0) {
			input_err(true, &ts->client->dev, "%s: read fail, read_boot_status = 0x%x\n", __func__, fw_status);
			return -EIO;
		}

		if (fw_status != SLSI_TS_STATUS_APP_MODE) {
			input_err(true, &ts->client->dev, "%s: fw update sequence done, BUT read_boot_status = 0x%x\n", __func__, fw_status);
			return -EIO;
		}

		input_info(true, &ts->client->dev, "%s: fw update Success! read_boot_status = 0x%x\n", __func__, fw_status);

		return 1;
	} else {

		if (ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_ID, tBuff, 3) < 0) {
			input_err(true, &ts->client->dev, "%s: read device id fail after bl fw download\n", __func__);
			return -EIO;
		}

		if (tBuff[0] == 0xA0) {
			input_info(true, &ts->client->dev, "%s: bl fw download success - device id = %02X\n", __func__, tBuff[0]);
			return -EIO;
		} else {
			input_err(true, &ts->client->dev, "%s: bl fw id does not match - device id = %02X\n", __func__, tBuff[0]);
			return -EIO;
		}
	}
}

int slsi_ts_bl_update(struct slsi_ts_data *ts)
{
	int ret;
	u8 tCmd[5] = { 0xDE, 0xAD, 0xBE, 0xEF };
	u8 tBuff[3];

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_READ_BL_UPDATE_STATUS, tCmd, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: bl update command send fail!\n", __func__);
		goto err;
	}
	sec_delay(10);

	do {
		ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_BL_UPDATE_STATUS, tBuff, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: read bl update status fail!\n", __func__);
			goto err;
		}
		sec_delay(2);

	} while (tBuff[0] == 0x1);

	tCmd[0] = 0x55;
	tCmd[1] = 0xAC;
	ret = ts->slsi_ts_i2c_write(ts, 0x57, tCmd, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: write passwd fail!\n", __func__);
		goto err;
	}

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_ID, tBuff, 3);

	if (tBuff[0]  == 0xB4) {
		input_info(true, &ts->client->dev, "%s: bl update completed!\n", __func__);
		ret = 1;
	} else {
		input_info(true, &ts->client->dev, "%s: bl updated but bl version not matching, ver=%02X\n", __func__, tBuff[0]);
		goto err;
	}

	return ret;

err:
	return -EIO;
}

int slsi_ts_firmware_update_on_probe(struct slsi_ts_data *ts, bool force_update)
{
	const struct firmware *fw_entry;
	char fw_path[SEC_TS_MAX_FW_PATH];
	int result = -1;
	int ii = 0;
	int ret = 0;

#ifdef TCLM_CONCEPT
	int retry = 3;
	int restore_cal = 0;
	if (ts->tdata->support_tclm_test) {
		ret = sec_tclm_test_on_probe(ts->tdata);
		if (ret < 0)
			input_info(true, &ts->client->dev, "%s: SEC_TCLM_NVM_ALL_DATA i2c read fail", __func__);

	}
#endif

	if (ts->plat_data->bringup == 1) {
		input_err(true, &ts->client->dev, "%s: bringup. do not update\n", __func__);
		return 0;
	}

	if (ts->plat_data->firmware_name)
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", ts->plat_data->firmware_name);
	else
		return 0;

	disable_irq(ts->client->irq);

	ts->cal_status = slsi_ts_read_calibration_report(ts);

	input_info(true, &ts->client->dev, "%s: initial firmware update %s, cal:%X\n",
				__func__, fw_path, ts->cal_status);

	if (request_firmware(&fw_entry, fw_path, &ts->client->dev) !=  0) {
		input_err(true, &ts->client->dev, "%s: not exist firmware\n", __func__);
		result = 0;
		goto err_request_fw;
	}

	result = slsi_ts_check_firmware_version(ts, fw_entry->data);
	if (ts->plat_data->bringup == 2) {
		input_err(true, &ts->client->dev, "%s: bringup. do not update\n", __func__);
		result = 0;
		goto err_fw;
	}

	/* don't firmup case */
	if ((result <= 0) && (!force_update)) {
		input_info(true, &ts->client->dev, "%s: skip - fw update\n", __func__);
		goto err_fw;
	} else {	/* firmup case */

		for (ii = 0; ii < 3; ii++) {
			ret = slsi_ts_firmware_update(ts, fw_entry->data, 0);
			if (ret >= 0)
				break;
			input_err(true, &ts->client->dev, "%s: failed, retry=%d\n", __func__, ii + 1);
		}

		if (ret < 0) {
			result = -1;
			goto err_fw;
		}

		slsi_ts_save_version_of_ic(ts);

		result = 0;

#ifdef TCLM_CONCEPT
		while (retry--) {
			ret = ts->tdata->tclm_read(ts->tdata->client, SEC_TCLM_NVM_ALL_DATA);
			if (ret >= 0)
				break;
		}

		if (ret < 0) {
			input_info(true, &ts->client->dev, "%s: SEC_TCLM_NVM_ALL_DATA i2c read fail", __func__);
			goto err_fw;
		}

		input_info(true, &ts->client->dev, "%s: tune_fix_ver [%04X] afe_base [%04X]\n",
			__func__, ts->tdata->nvdata.tune_fix_ver, ts->tdata->afe_base);

		if ((ts->tdata->tclm_level > TCLM_LEVEL_CLEAR_NV) &&
			((ts->tdata->nvdata.tune_fix_ver == 0xffff)
			|| (ts->tdata->afe_base > ts->tdata->nvdata.tune_fix_ver))) {
			/* tune version up case */
			sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TUNEUP);
			restore_cal = 1;
		} else if (ts->tdata->tclm_level == TCLM_LEVEL_CLEAR_NV) {
			/* firmup case */
			sec_tclm_root_of_cal(ts->tdata, CALPOSITION_FIRMUP);
			restore_cal = 1;
		}

		if (restore_cal == 1) {
			input_err(true, &ts->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
			ret = sec_execute_tclm_package(ts->tdata, 0);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "%s: sec_execute_tclm_package fail\n", __func__);
			}
		}
#endif
	}


#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif

err_fw:
	release_firmware(fw_entry);
err_request_fw:
	enable_irq(ts->client->irq);
	return result;
}

static int slsi_ts_load_fw_from_bin(struct slsi_ts_data *ts)
{
	int error = 0;
	int restore_cal = 0;
	const struct firmware *fw_entry;
	char fw_path[SEC_TS_MAX_FW_PATH];

	if (ts->plat_data->bringup == 1) {
		error = -1;
		input_info(true, &ts->client->dev, "%s: can't update for bringup:%d\n",
				__func__, ts->plat_data->bringup);
		return error;
	}

	if (ts->plat_data->firmware_name)
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", ts->plat_data->firmware_name);
	else
		return 0;

	if (ts->client->irq)
		disable_irq(ts->client->irq);

	/* Loading Firmware */
	if (request_firmware(&fw_entry, fw_path, &ts->client->dev) !=  0) {
		input_err(true, &ts->client->dev, "%s: not exist firmware\n", __func__);
		error = -1;
		goto err_request_fw;
	}


#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TESTMODE);
	restore_cal = 1;
#endif
	/* use virtual tclm_control - magic cal 1 */
	if (slsi_ts_firmware_update(ts, fw_entry->data, 0) < 0) {
		error = -1;
		restore_cal = 0;
	}

#ifdef TCLM_CONCEPT
	if (restore_cal == 1) {
		sec_execute_tclm_package(ts->tdata, 0);
	}
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif

	slsi_ts_save_version_of_ic(ts);

	release_firmware(fw_entry);
err_request_fw:
	if (ts->client->irq)
		enable_irq(ts->client->irq);

	return error;
}

static int slsi_ts_load_fw(struct slsi_ts_data *ts, int update_type)
{
	int error = 0;
	fw_header *fw_hd;
	const struct firmware *fw_entry;
	char fw_path[SEC_TS_MAX_FW_PATH];
	bool is_fw_signed = false;
#ifdef SUPPORT_FW_SIGNED
	long spu_ret = 0;
	long ori_size = 0;
#endif
#ifdef TCLM_CONCEPT
	int restore_cal = 0;
#endif

	if (ts->client->irq)
		disable_irq(ts->client->irq);

	switch (update_type) {
	case TSP_SDCARD:
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_EXTERNAL_FW);
#else
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_EXTERNAL_FW_SIGNED);
		is_fw_signed = true;
#endif
		break;
	case TSP_SPU:
	case TSP_VERIFICATION:
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_SPU_FW_SIGNED);
		is_fw_signed = true;
		break;
	default:
		goto err_firmware_path;
	}


	error = request_firmware(&fw_entry, fw_path, &ts->client->dev);

	if (error) {
		input_err(true, &ts->client->dev, "%s: firmware is not available %d\n", __func__, error);
		goto err_request_fw;
	}
	input_info(true, &ts->client->dev, "%s: request firmware done! size = %d\n", __func__, (int)fw_entry->size);

	fw_hd = (fw_header *)fw_entry->data;
	input_info(true, &ts->client->dev, "%s: firmware version %08X, parameter version %08X\n", __func__, fw_hd->fw_ver, fw_hd->para_ver);

#ifdef SUPPORT_FW_SIGNED
	/* If SPU firmware version is lower than IC's version, do not run update routine */
	if (update_type == TSP_VERIFICATION) {
		ori_size = fw_entry->size - SPU_METADATA_SIZE(TSP);
		spu_ret = spu_firmware_signature_verify("TSP", fw_entry->data, fw_entry->size);
		if (spu_ret != ori_size) {
			input_err(true, &ts->client->dev, "%s: signature verify failed, spu_ret:%ld, ori_size:%ld\n",
				__func__, spu_ret, ori_size);
			error = -EPERM;
		}
		release_firmware(fw_entry);
		goto err_request_fw;

	} else if (is_fw_signed) {
		/* digest 32, signature 512 TSP 3 */
		ori_size = fw_entry->size - SPU_METADATA_SIZE(TSP);
		if ((update_type == TSP_SPU) && (ts->plat_data->img_version_of_ic[0] == ((fw_hd->img_ver >> 0) & 0xff) &&
			ts->plat_data->img_version_of_ic[1] == ((fw_hd->img_ver >> 8) & 0xff) &&
			ts->plat_data->img_version_of_ic[2] == ((fw_hd->img_ver >> 16) & 0xff))) {
			if (ts->plat_data->img_version_of_ic[3] >= ((fw_hd->img_ver >> 24) & 0xff)) {
				input_info(true, &ts->client->dev, "%s: img version: %02X%02X%02X%02X/%08X exit\n",
					__func__, ts->plat_data->img_version_of_ic[3], ts->plat_data->img_version_of_ic[2],
					ts->plat_data->img_version_of_ic[1], ts->plat_data->img_version_of_ic[0],
					fw_hd->img_ver);
				error = 0;
				input_info(true, &ts->client->dev, "%s: skip spu\n", __func__);
				goto done;
			} else {
				input_info(true, &ts->client->dev, "%s: run spu\n", __func__);
			}
		} else if ((update_type == TSP_SDCARD) && (ts->plat_data->img_version_of_ic[0] == ((fw_hd->img_ver >> 0) & 0xff) &&
			ts->plat_data->img_version_of_ic[1] == ((fw_hd->img_ver >> 8) & 0xff))) {
			input_info(true, &ts->client->dev, "%s: run sfu\n", __func__);
		} else {
			input_info(true, &ts->client->dev, "%s: not matched product version\n", __func__);
			error = -ENOENT;
			goto done;
		}

		spu_ret = spu_firmware_signature_verify("TSP", fw_entry->data, fw_entry->size);
		if (spu_ret != ori_size) {
			input_err(true, &ts->client->dev, "%s: signature verify failed, spu_ret:%ld, ori_size:%ld\n",
				__func__, spu_ret, ori_size);
			error = -EPERM;
			goto done;
		}
	}
#endif

#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TESTMODE);
	restore_cal = 1;
#endif

	error = slsi_ts_firmware_update(ts, fw_entry->data, 0);
	if (error < 0)
		goto done;

	slsi_ts_save_version_of_ic(ts);

#ifdef TCLM_CONCEPT
	sec_execute_tclm_package(ts->tdata, 0);
#endif
done:
	if (error < 0)
		input_err(true, ts->dev, "%s: failed update firmware\n", __func__);
#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif
	release_firmware(fw_entry);
err_request_fw:
err_firmware_path:
	if (ts->client->irq)
		enable_irq(ts->client->irq);
	return error;
}

int slsi_ts_firmware_update_on_hidden_menu(struct slsi_ts_data *ts, int update_type)
{
	int ret = SEC_ERROR;

	switch (update_type) {
	case TSP_BUILT_IN:
		ret = slsi_ts_load_fw_from_bin(ts);
		break;
	case TSP_SDCARD:
	case TSP_SPU:
	case TSP_VERIFICATION:
		ret = slsi_ts_load_fw(ts, update_type);
		break;
	default:
		input_err(true, ts->dev, "%s: Not support command[%d]\n",
				__func__, update_type);
		break;
	}

	slsi_ts_get_custom_library(ts);
	slsi_ts_set_custom_library(ts);

	return ret;
}

MODULE_LICENSE("GPL");
EXPORT_SYMBOL(slsi_ts_firmware_update_on_hidden_menu);
