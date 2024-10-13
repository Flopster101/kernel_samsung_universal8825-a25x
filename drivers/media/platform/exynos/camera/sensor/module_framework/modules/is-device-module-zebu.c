/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_reserved_mem.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-is-sensor.h>
#include "is-hw.h"
#include "is-core.h"
#include "is-device-sensor.h"
#include "is-resourcemgr.h"
#include "is-dt.h"
#include "is-hw-api-common.h"
#include "is-device-module-base.h"

#define SENSOR_NAME "IS-SENSOR-MODULE-ZEBU"

/* IMG Driver */
enum img_drv_reg_name {
	IMG_DRV_R_START,
	IMG_DRV_R_BUSY,
	IMG_DRV_R_FRAME_NUM,
	IMG_DRV_R_LANE_NUM,
	IMG_DRV_R_INTERLEAVE_MODE,
	IMG_DRV_R_FRAME_BLANK,
	IMG_DRV_R_FRAME_BLANK_MULT_VALUE,
	IMG_DRV_R_LINE_BLANK,
	IMG_DRV_R_IMG_SIZE_VC0,
	IMG_DRV_R_FORMAT_VC0,
	IMG_DRV_R_BASE_ADDRESS_VC0,
	IMG_DRV_R_IMG_SIZE_VC1,
	IMG_DRV_R_FORMAT_VC1,
	IMG_DRV_R_BASE_ADDRESS_VC1,
	IMG_DRV_R_IMG_SIZE_VC2,
	IMG_DRV_R_FORMAT_VC2,
	IMG_DRV_R_BASE_ADDRESS_VC2,
	IMG_DRV_R_IMG_SIZE_VC3,
	IMG_DRV_R_FORMAT_VC3,
	IMG_DRV_R_BASE_ADDRESS_VC3,
	IMG_DRV_R_IMG_SIZE_VC4,
	IMG_DRV_R_FORMAT_VC4,
	IMG_DRV_R_BASE_ADDRESS_VC4,
	IMG_DRV_R_IMG_SIZE_VC5,
	IMG_DRV_R_FORMAT_VC5,
	IMG_DRV_R_BASE_ADDRESS_VC5,
	IMG_DRV_R_IMG_SIZE_VC6,
	IMG_DRV_R_FORMAT_VC6,
	IMG_DRV_R_BASE_ADDRESS_VC6,
	IMG_DRV_R_IMG_SIZE_VC7,
	IMG_DRV_R_FORMAT_VC7,
	IMG_DRV_R_BASE_ADDRESS_VC7,
	IMG_DRV_REG_CNT
};

static const struct is_reg img_drv_regs[IMG_DRV_REG_CNT] = {
	{0x0000, "START"},
	{0x0004, "BUSY"},
	{0x0008, "FRAME_NUM"},
	{0x0010, "LANE_NUM"},
	{0x0014, "INTERLEAVE_MODE"},
	{0x0018, "FRAME_BLANK"},
	{0x001C, "FRAME_BLANK_MULT_VALUE"},
	{0x0020, "LINE_BLANK"},
	{0x1000, "IMG_SIZE_VC0"},
	{0x1004, "FORMAT_VC0"},
	{0x1008, "BASE_ADDRESS_VC0"},
	{0x1100, "IMG_SIZE_VC1"},
	{0x1104, "FORMAT_VC1"},
	{0x1108, "BASE_ADDRESS_VC1"},
	{0x1200, "IMG_SIZE_VC2"},
	{0x1204, "FORMAT_VC2"},
	{0x1208, "BASE_ADDRESS_VC2"},
	{0x1300, "IMG_SIZE_VC3"},
	{0x1304, "FORMAT_VC3"},
	{0x1308, "BASE_ADDRESS_VC3"},
	{0x1400, "IMG_SIZE_VC4"},
	{0x1404, "FORMAT_VC4"},
	{0x1408, "BASE_ADDRESS_VC4"},
	{0x1500, "IMG_SIZE_VC5"},
	{0x1504, "FORMAT_VC5"},
	{0x1508, "BASE_ADDRESS_VC5"},
	{0x1600, "IMG_SIZE_VC6"},
	{0x1604, "FORMAT_VC6"},
	{0x1608, "BASE_ADDRESS_VC6"},
	{0x1700, "IMG_SIZE_VC7"},
	{0x1704, "FORMAT_VC7"},
	{0x1708, "BASE_ADDRESS_VC7"},
};

enum img_drv_reg_field {
	/* START */
	IMG_DRV_F_START,
	/* BUSY */
	IMG_DRV_F_LOADER_BUSY,
	IMG_DRV_F_DRIVER_BUSY,
	/* FRAME_NUM */
	IMG_DRV_F_FRAME_NUM,
	IMG_DRV_F_INFINITE_RUN,
	/* LANE_NUM */
	IMG_DRV_F_LANE_NUM,
	/* INTERLEAVE_MODE */
	IMG_DRV_F_INTERLEAVE_MODE,
	/* FRAME_BLANK */
	IMG_DRV_F_FRAME_BLANK,
	/* FRAME_BLANK_MULT_VALUE */
	IMG_DRV_F_FRAME_BLANK_MULT_VALUE,
	/* LINE_BLANK */
	IMG_DRV_F_LINE_BLANK,
	/* IMG_SIZE_VCX */
	IMG_DRV_F_WIDTH,
	IMG_DRV_F_HEIGHT,
	/* FORMAT_VCX */
	IMG_DRV_F_FORMAT,
	/* BASE_ADDRESS_VCX */
	IMG_DRV_F_ADDR,
	IMG_DRV_REG_FIELD_CNT
};

static const struct is_field img_drv_fields[IMG_DRV_REG_FIELD_CNT] = {
	/* filed_name, start_bit, bit_width, type, reset */
	/* START */
	{"START", 0, 1, WO, 0x0},
	/* BUSY */
	{"LOADER_BUSY", 0, 1, RO, 0x0},
	{"DRIVER_BUSY", 1, 1, RO, 0x0},
	/* FRAME_NUM */
	{"FRAME_NUM", 0, 7, RW, 0x0},
	{"INFINITE_RUN", 7, 1, RW, 0x0},
	/* LANE_NUM */
	{"LANE_NUM", 0, 2, RW, 0x0},
	/* INTERLEAVE_MODE */
	{"INTERLEAVE_MODE", 0, 2, RW, 0x0},
	/* FRAME_BLANK */
	{"FRAME_BLANK", 0, 32, RW, 0x0},
	/* FRAME_BLANK_MULT_VALUE */
	{"FRAME_BLANK_MULT_VALUE", 0, 16, RW, 0x0},
	/* LINE_BLANK */
	{"LINE_BLANK", 0, 32, RW, 0x0},
	/* IMG_SIZE_VCX */
	{"WIDTH", 0, 16, RW, 0x0},
	{"HEIGHT", 16, 16, RW, 0x0},
	/* FORMAT_VCX */
	{"FORMAT", 0, 8, RW, 0x0},
	/* BASE_ADDRESS_VCX */
	{"ADDR", 0, 32, RW, 0x0},
};

#define IMG_DRV_BIN_SIZE_MAX 	(0x1000000) /* 16MB */
#define IMG_DRV_HEADER_SIZE	(4)
#define IMG_DRV_CRC_SIZE	(2)

static struct sensor_zebu_module {
	u32 use_img_drv;
	void __iomem *reg;
	struct is_resource_rmem rmem;
	char *bin_path;
} module_zebu;

static int sensor_zebu_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;

	FIMC_BUG(!subdev);

	if (module_zebu.use_img_drv)
		is_hw_set_reg(module_zebu.reg, &img_drv_regs[IMG_DRV_R_FRAME_NUM], 0);

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	FIMC_BUG(!sensor_peri);

	memset(&sensor_peri->sensor_interface, 0, sizeof(struct is_sensor_interface));

	ret = init_sensor_interface(&sensor_peri->sensor_interface);
	if (ret) {
		err("failed in init_sensor_interface, return: %d", ret);
		return -EINVAL;
	}

	pr_info("[MOD:%s] %s(%d)\n", module->sensor_name, __func__, val);

	return 0;
}

static long sensor_zebu_module_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	return 0;
}

int sensor_zebu_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	/* No operations. */
	return 0;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_zebu_init,
	.g_ctrl = sensor_zebu_g_ctrl,
	.ioctl = sensor_zebu_module_ioctl,
};


static int sensor_zebu_s_routing(struct v4l2_subdev *sd,
		u32 input, u32 output, u32 config) {

	return 0;
}

static int sensor_zebu_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct is_module_enum *sensor;

	sensor = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!sensor) {
		err("sensor is NULL");
		return -EINVAL;
	}

	if (enable)
		ret = CALL_MOPS(sensor, stream_on, subdev);
	else
		ret = CALL_MOPS(sensor, stream_off, subdev);

	if (ret < 0) {
		err("stream_%s is fail(%d)", enable ? "on" : "off",ret);
		return -EINVAL;
	}

	return 0;
}

static int sensor_zebu_s_format(struct v4l2_subdev *subdev,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *fmt)
{
	/* TODO */
	return 0;
}

static int sensor_zebu_crc16_8(int cur_crc, int data)
{
	u32 b;

	for (b = 0; b < BITS_PER_BYTE; b++) {
		if ((cur_crc & 1) ^ (data & 1))
			cur_crc = ((cur_crc >> 1) ^ 0x8408) & 0xFFFF;
		else
			cur_crc = cur_crc >> 1;

		data = data >> 1;
	}

	return cur_crc;
}

static int sensor_zebu_encode_binary(void *dst_addr, struct is_binary *bin, struct is_vci_config *vci_cfg)
{
	u32 bitdepth, wrap_num, bytesperline, length;
	u32 x, y, crc;
	u32 src_pos = 0, dst_pos = 0;
	u16 *src16;
	u8 *src8, *dst;
	u8 data, rest;

	if (!dst_addr || !bin || !vci_cfg) {
		err("%s:Invalid argument. dst_addr 0x%lx bin 0x%lx vci_cfg 0x%lx\n", __func__,
				dst_addr, bin, vci_cfg);
		return -EINVAL;
	}

	switch (vci_cfg->hwformat) {
	case HW_FORMAT_RAW8:
		bitdepth = 8;
		wrap_num = 1;
		break;
	case HW_FORMAT_RAW10:
		bitdepth = 10;
		wrap_num = 4;
		break;
	case HW_FORMAT_RAW12:
		bitdepth = 12;
		wrap_num = 2;
		break;
	case HW_FORMAT_RAW14:
		bitdepth = 14;
		wrap_num = 4;
		break;
	case HW_FORMAT_YUV422_8BIT:
		bitdepth = 16;
		wrap_num = 1;
		break;
	default:
		/* This would NOT guarantee normal operation. */
		bitdepth = 8;
		wrap_num = 1;
	}

	bytesperline = ALIGN(vci_cfg->width, wrap_num) * bitdepth / BITS_PER_BYTE;
	length = (IMG_DRV_HEADER_SIZE + bytesperline + IMG_DRV_CRC_SIZE)
		* vci_cfg->height;
	src8 = (u8 *)bin->data;
	src16 = (u16 *)bin->data;
	dst = (u8 *)dst_addr;

	info("%s:Generate CSIS packet stream. %dx%d(0x%x) 0x%lx -> 0x%lx (%d)\n", __func__,
			vci_cfg->width, vci_cfg->height, vci_cfg->hwformat,
			src8, dst, length);

	/* Loop per line */
	for (y = 0; y < vci_cfg->height; y++) {
		/* Put line header */
		dst[dst_pos++] = (u8)vci_cfg->hwformat;
		dst[dst_pos++] = (u8)(bytesperline & 0xFF);
		dst[dst_pos++] = (u8)((bytesperline >> BITS_PER_BYTE) & 0xFF);
		dst[dst_pos++] = (u8)0;

		crc = 0xffff;

		/* Put 1 line encoded payload */
		for (x = 0; x < vci_cfg->width; x += wrap_num) {
			data = rest = 0;

			if (vci_cfg->hwformat == HW_FORMAT_RAW8) {
				/* dst byte[0] = src pixel[0] 8bit */
				data = src8[src_pos++];
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
			} else if (vci_cfg->hwformat == HW_FORMAT_RAW10) {
				/* dst byte[0] = src pixel[0] MSB 8bit */
				data = (u8)((src16[src_pos] >> 2) & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
				rest = (u8)(src16[src_pos++] & 0x3);

				/* dst byte[1] = src pixel[1] MSB 8bit */
				data = (u8)((src16[src_pos] >> 2) & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
				rest |= (u8)((src16[src_pos++] & 0x3) << 2);

				/* dst byte[2] = src pixel[2] MSB 8bit */
				data = (u8)((src16[src_pos] >> 2) & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
				rest |= (u8)((src16[src_pos++] & 0x3) << 4);

				/* dst byte[3] = src pixel[3] MSB 8bit */
				data = (u8)((src16[src_pos] >> 2) & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
				rest |= (u8)((src16[src_pos++] & 0x3) << 6);

				/* dst byte[4] = src pixel[0:3] LSB 2bit */
				dst[dst_pos++] = rest;
				crc = sensor_zebu_crc16_8(crc, rest);
			} else if (vci_cfg->hwformat == HW_FORMAT_RAW12) {
				/* dst byte[0] = src pixel[0] MSB 8bit */
				data = (u8)((src16[src_pos] >> 4) & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
				rest = (u8)(src16[src_pos++] & 0xF);

				/* dst byte[1] = src pixel[1] MSB 8bit */
				data = (u8)((src16[src_pos] >> 4) & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
				rest |= (u8)((src16[src_pos++] & 0xF) << 4);

				/* dst byte[2] = src pixel[0:1] LSB 4bit */
				dst[dst_pos++] = rest;
				crc = sensor_zebu_crc16_8(crc, rest);
			} else if (vci_cfg->hwformat == HW_FORMAT_RAW14) {
				/* dst byte[0] = src pixel[0] MSB 8bit */
				data = (u8)((src16[src_pos] >> 6) & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
				rest = (u8)(src16[src_pos++] & 0x3F);

				/* dst byte[1] = src pixel[1] MSB 8bit */
				data = (u8)((src16[src_pos] >> 6) & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
				rest |= (u8)((src16[src_pos] & 0x3) << 6);

				/* dst byte[2] = src pixel[0] LSB 6bit + pixel[1] LSB 2bit */
				dst[dst_pos++] = rest;
				crc = sensor_zebu_crc16_8(crc, rest);
				rest = (u8)((src16[src_pos++] >> 2) & 0xF);

				/* dst byte[3] = src pixel[2] MSB 8bit */
				data = (u8)((src16[src_pos] >> 6) & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
				rest |= (u8)((src16[src_pos] & 0xF) << 4);

				/* dst byte[4] = src pixel[1] [5:2] 4bit + pixel[2] LSB 4bit */
				dst[dst_pos++] = rest;
				crc = sensor_zebu_crc16_8(crc, rest);
				rest = (u8)((src16[src_pos++] >> 4) & 0x3);

				/* dst byte[5] = src pixel[3] MSB 8bit */
				data = (u8)((src16[src_pos] >> 6) & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
				rest |= (u8)((src16[src_pos++] & 0x3F) << 2);

				/* dst byte[6] = src pixel[2] [5:4] 2bit + pixel[3] LSB 6bit */
				dst[dst_pos++] = rest;
				crc = sensor_zebu_crc16_8(crc, rest);
			} else if (vci_cfg->hwformat == HW_FORMAT_YUV422_8BIT) {
				/* dst byte[0] = src pixel[0] LSB 8bit */
				data = (u8)(src16[src_pos] & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);

				/* dst byte[1] = src pixel[0] MSB 8bit */
				data = (u8)((src16[src_pos++] >> 8) & 0xFF);
				dst[dst_pos++] = data;
				crc = sensor_zebu_crc16_8(crc, data);
			} else {
				err("%s:Not supported format 0x%X", __func__, vci_cfg->hwformat);
				return -EINVAL;
			}
		}

		/* Put line CRC */
		dst[dst_pos++] = (u8)(crc & 0xFF);
		dst[dst_pos++] = (u8)((crc >> BITS_PER_BYTE) & 0xFF);
	}

	if (dst_pos != length) {
		err("%s:Abnormal CSIS packet stream generated. %dx%d(0x%d) %d -> %d bytes expected %d", __func__,
				vci_cfg->width, vci_cfg->height,
				vci_cfg->hwformat,
				(wrap_num > 1) ? (src_pos * 2) : src_pos, dst_pos, length);
		return -EINVAL;
	}

	info("%s:CSIS packet stream generated. %dx%d(0x%x) %d -> %d bytes\n", __func__,
			vci_cfg->width, vci_cfg->height,
			vci_cfg->hwformat,
			(wrap_num > 1) ? (src_pos * 2) : src_pos, dst_pos);

	return 0;
}

static void sensor_zebu_img_drv_dump(u32 __iomem *reg)
{
	info("IMG_DRV REG DUMP\n");
	is_hw_dump_regs(reg, img_drv_regs, IMG_DRV_REG_CNT);
}

int sensor_zebu_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0, size;
	struct is_module_enum *module;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *sensor_cfg;
	void __iomem *regs;
	struct is_vci_config *vci_cfg;
	struct is_binary bin;
	char *f_name;
	u32 vc, val, paddr;

	if (!module_zebu.use_img_drv)
		goto func_exit;

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	sensor_cfg = sensor->cfg;
	regs = module_zebu.reg;
	paddr = module_zebu.rmem.phys_addr;

	f_name = __getname();
	if (unlikely(!f_name)) {
		err("%s:failed to getname", __func__);
		return -ENOMEM;
	}

	setup_binary_loader(&bin, 3, -EAGAIN, NULL, NULL);

	info("%s:CSI %d lanes intr_mode %d\n", __func__,
			sensor_cfg->lanes, sensor_cfg->interleave_mode);
	is_hw_set_field(regs, &img_drv_regs[IMG_DRV_R_LANE_NUM],
			&img_drv_fields[IMG_DRV_F_LANE_NUM], sensor_cfg->lanes);
	is_hw_set_field(regs, &img_drv_regs[IMG_DRV_R_INTERLEAVE_MODE],
			&img_drv_fields[IMG_DRV_F_INTERLEAVE_MODE], sensor_cfg->interleave_mode);

	/* Follows the guided value from MMDV */
	is_hw_set_field(regs, &img_drv_regs[IMG_DRV_R_FRAME_BLANK],
			&img_drv_fields[IMG_DRV_F_FRAME_BLANK], 0xFFFF);
	is_hw_set_field(regs, &img_drv_regs[IMG_DRV_R_FRAME_BLANK_MULT_VALUE],
			&img_drv_fields[IMG_DRV_F_FRAME_BLANK_MULT_VALUE], 0x1);
	is_hw_set_field(regs, &img_drv_regs[IMG_DRV_R_LINE_BLANK],
			&img_drv_fields[IMG_DRV_F_LINE_BLANK], 0x100);

	is_hw_set_field(regs, &img_drv_regs[IMG_DRV_R_FRAME_NUM],
			&img_drv_fields[IMG_DRV_F_INFINITE_RUN], 1);

	/* For each VC */
	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		vci_cfg = &sensor_cfg->input[vc];
		if (vci_cfg->hwformat == HW_FORMAT_UNKNOWN)
			continue;

		size = snprintf(f_name, PATH_MAX, "img_vc%d_%dx%d.bin",
				vc, vci_cfg->width, vci_cfg->height);

		info("%s:[VC%d] binary %s%s\n", __func__,
				vc, module_zebu.bin_path, f_name);
		ret = request_binary(&bin, module_zebu.bin_path, f_name, module->dev);
		if (ret) {
			err("%s:[VC%d]failed to get %s%s. ret %d", __func__,
					vc,
					module_zebu.bin_path, f_name,
					ret);
			ret = 0;
			continue;
		} else if (bin.size > IMG_DRV_BIN_SIZE_MAX) {
			err("%s:[VC%d]Too large bin size 0x%X", __func__, vc, bin.size);
			release_binary(&bin);
			continue;
		}

		paddr += (IMG_DRV_BIN_SIZE_MAX * vc);
		ret = sensor_zebu_encode_binary(phys_to_virt(paddr), &bin, vci_cfg);
		release_binary(&bin);
		if (ret) {
			err("%s:[VC%d]failed to encode image. ret %d", __func__, vc);
			ret = 0;
			continue;
		}

		val = 0;
		val = is_hw_set_field_value(val, &img_drv_fields[IMG_DRV_F_WIDTH], vci_cfg->width);
		val = is_hw_set_field_value(val, &img_drv_fields[IMG_DRV_F_HEIGHT], vci_cfg->height);
		is_hw_set_reg(regs, &img_drv_regs[IMG_DRV_R_IMG_SIZE_VC0 + (vc * 3)], val);
		is_hw_set_field(regs, &img_drv_regs[IMG_DRV_R_FORMAT_VC0 + (vc * 3)],
				&img_drv_fields[IMG_DRV_F_FORMAT], vci_cfg->hwformat);

		is_hw_set_reg(regs, &img_drv_regs[IMG_DRV_R_BASE_ADDRESS_VC0 + (vc * 3)], paddr);

		info("%s:[VC%d] %dx%d(0x%x) paddr 0x%x\n", __func__, vc,
				vci_cfg->width, vci_cfg->height,
				vci_cfg->hwformat, paddr);
	}

	__putname(f_name);

	info("%s:Start IMG_DRV stream\n", __func__);
	is_hw_set_field(regs, &img_drv_regs[IMG_DRV_R_START],
			&img_drv_fields[IMG_DRV_F_START], 1);

	mdelay(10);

	sensor_zebu_img_drv_dump(regs);

func_exit:
	info("%s\n", __func__);

	return ret;
}

int sensor_zebu_stream_off(struct v4l2_subdev *subdev)
{
	/* Nothing to do for IMG_drv */

	info("%s\n", __func__);

	return 0;
}

/*
 * @ brief
 * frame duration time
 * @ unit
 * nano second
 * @ remarks
 */
int sensor_zebu_s_duration(struct v4l2_subdev *subdev, u64 duration)
{
	return 0;
}

int sensor_zebu_g_min_duration(struct v4l2_subdev *subdev)
{
	return 0;
}

int sensor_zebu_g_max_duration(struct v4l2_subdev *subdev)
{
	return 0;
}

int sensor_zebu_s_exposure(struct v4l2_subdev *subdev, u64 exposure)
{
	return 0;
}

int sensor_zebu_g_min_exposure(struct v4l2_subdev *subdev)
{
	return 0;
}

int sensor_zebu_g_max_exposure(struct v4l2_subdev *subdev)
{
	return 0;
}

int sensor_zebu_s_again(struct v4l2_subdev *subdev, u64 sensitivity)
{
	return 0;
}

int sensor_zebu_g_min_again(struct v4l2_subdev *subdev)
{
	return 0;
}

int sensor_zebu_g_max_again(struct v4l2_subdev *subdev)
{
	return 0;
}

int sensor_zebu_s_dgain(struct v4l2_subdev *subdev)
{
	return 0;
}

int sensor_zebu_g_min_dgain(struct v4l2_subdev *subdev)
{
	return 0;
}

int sensor_zebu_g_max_dgain(struct v4l2_subdev *subdev)
{
	return 0;
}

static const struct v4l2_subdev_video_ops video_ops = {
	.s_routing = sensor_zebu_s_routing,
	.s_stream = sensor_zebu_s_stream,
};

static const struct v4l2_subdev_pad_ops pad_ops = {
	.set_fmt = sensor_zebu_s_format
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops,
	.pad = &pad_ops
};

struct is_sensor_ops module_zebu_ops = {
	.stream_on	= sensor_zebu_stream_on,
	.stream_off	= sensor_zebu_stream_off,
	.s_duration	= sensor_zebu_s_duration,
	.g_min_duration	= sensor_zebu_g_min_duration,
	.g_max_duration	= sensor_zebu_g_max_duration,
	.s_exposure	= sensor_zebu_s_exposure,
	.g_min_exposure	= sensor_zebu_g_min_exposure,
	.g_max_exposure	= sensor_zebu_g_max_exposure,
	.s_again	= sensor_zebu_s_again,
	.g_min_again	= sensor_zebu_g_min_again,
	.g_max_again	= sensor_zebu_g_max_again,
	.s_dgain	= sensor_zebu_s_dgain,
	.g_min_dgain	= sensor_zebu_g_min_dgain,
	.g_max_dgain	= sensor_zebu_g_max_dgain
};

static int sensor_zebu_power_setpin(struct device *dev,
	struct exynos_platform_is_module *pdata)
{
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF);

	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, 0, "", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, 0, "", PIN_OUTPUT, 0, 0);

	return 0;
}

static int sensor_module_zebu_parse_dt(struct platform_device *pdev)
{
	int ret;
	struct device_node *dnode;
	struct resource *res;
	struct device_node *rmem_np;
	struct reserved_mem *rmem;

	FIMC_BUG(!pdev->dev.of_node);

	dnode = pdev->dev.of_node;

	if (of_property_read_u32(dnode, "use_img_drv", &module_zebu.use_img_drv))
		probe_warn("%s:Missing use_img_drv DT attr", __func__);

	probe_info("%s:IMG_DRV %s\n", __func__,
			module_zebu.use_img_drv ? "ON" : "OFF");

	if (!module_zebu.use_img_drv)
		return 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		probe_err("failed to get IMG_DRV reg");
		return -EINVAL;
	}

	module_zebu.reg = ioremap(res->start, resource_size(res));
	if (!module_zebu.reg) {
		probe_err("failed to map IMG_DRV IO region");
		return -ENOMEM;
	}

	rmem_np = of_parse_phandle(dnode, "rmem", 0);
	if (rmem_np) {
		rmem = of_reserved_mem_lookup(rmem_np);
		if (!rmem) {
			probe_err("failed to get rmem");
			ret = -ENOMEM;
			goto p_err;
		}

		module_zebu.rmem.phys_addr = rmem->base;
		module_zebu.rmem.size = rmem->size;

		probe_info("%s:img_drv_rmem: pa 0x%lx size 0x%lx\n", __func__,
				module_zebu.rmem.phys_addr, module_zebu.rmem.size);
	}

	if (of_property_read_string(dnode, "bin_path", (const char **)&module_zebu.bin_path)) {
		probe_err("failed to get IMG_DRV bin_path");
		ret = -EINVAL;
		goto p_err;
	}

	return 0;

p_err:
	iounmap(module_zebu.reg);
	return ret;
}

static int sensor_module_zebu_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct is_core *core;
	struct v4l2_subdev *subdev_module;
	struct is_module_enum *module;
	struct is_device_sensor *device;
	struct exynos_platform_is_module *pdata;
	struct device *dev;
	int mc;

	core = is_get_is_core();
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &pdev->dev;

	ret = is_sensor_module_parse_dt(dev, sensor_zebu_power_setpin);
	if (ret)
		return ret;

#ifdef CONFIG_OF
	sensor_module_zebu_parse_dt(pdev);
#endif

	pdata = dev_get_platdata(dev);
	device = &core->sensor[pdata->id];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	/*zebu sensor*/
	mc = atomic_read(&device->module_count);
	atomic_inc(&device->module_count);
	probe_info("%s pdata->id(%d), module count = %d\n",
			__func__, pdata->id, mc);

	module = &device->module_enum[mc];
	module->enum_id = mc;
	module->pdata = pdata;
	module->dev = dev;
	module->subdev = subdev_module;

	/* DT data */
	module->sensor_id = pdata->sensor_id;
	module->position = pdata->position;
	module->device = pdata->id;
	module->active_width = pdata->active_width;
	module->active_height = pdata->active_height;
	module->margin_left = pdata->margin_left;
	module->margin_right = pdata->margin_right;
	module->margin_top = pdata->margin_top;
	module->margin_bottom = pdata->margin_bottom;
	module->max_framerate = pdata->max_framerate;
	module->bitwidth = pdata->bitwidth;
	module->sensor_maker = pdata->sensor_maker;
	module->sensor_name = pdata->sensor_name;
	module->setfile_name = pdata->setfile_name;
	module->cfgs = pdata->cfgs;
	module->cfg = pdata->cfg;

	/* not DT data */
	module->pixel_width = module->active_width + module->margin_left + module->margin_right;
	module->pixel_height = module->active_height + module->margin_top + module->margin_bottom;
	module->ops = &module_zebu_ops;
	module->client = NULL;
	module->private_data = NULL;

	/* Sensor peri */
	module->private_data = devm_kzalloc(&pdev->dev,
		sizeof(struct is_device_sensor_peri), GFP_KERNEL);
	if (!module->private_data) {
		dev_err(&pdev->dev, "is_device_sensor_peri is NULL");
		return -ENOMEM;
	}
	is_sensor_peri_probe((struct is_device_sensor_peri *)module->private_data);
	PERI_SET_MODULE(module);

	platform_set_drvdata(pdev, module->private_data);

	v4l2_subdev_init(subdev_module, &subdev_ops);

	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->sensor_id);

p_err:
	probe_info("%s(%d)\n", __func__, ret);
	return ret;
}

static const struct of_device_id exynos_is_sensor_module_zebu_match[] = {
	{
		.compatible = "samsung,sensor-module-zebu",
		.data = NULL,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_is_sensor_module_zebu_match);

struct platform_driver sensor_module_zebu_driver = {
	.probe = sensor_module_zebu_probe,
	.driver = {
		.name   = SENSOR_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = exynos_is_sensor_module_zebu_match,
	}
};

#ifndef MODULE
static int __init is_sensor_module_zebu_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_module_zebu_driver,
				sensor_module_zebu_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_module_zebu_driver.driver.name, ret);

	return ret;
}
late_initcall(is_sensor_module_zebu_init);
#endif

MODULE_AUTHOR("Gilyeon lim");
MODULE_DESCRIPTION("Sensor zebu driver");
MODULE_LICENSE("GPL v2");

