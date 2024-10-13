/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/syscalls.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/dma-buf.h>
#include <linux/moduleparam.h>

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>

#include "is-time.h"
#include "is-core.h"
#include "is-param.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-debug.h"
#include "pablo-mem.h"
#include "is-video.h"

/*
 * copy from 'include/media/v4l2-ioctl.h'
 * #define V4L2_DEV_DEBUG_IOCTL		0x01
 * #define V4L2_DEV_DEBUG_IOCTL_ARG	0x02
 * #define V4L2_DEV_DEBUG_FOP		0x04
 * #define V4L2_DEV_DEBUG_STREAMING	0x08
 * #define V4L2_DEV_DEBUG_POLL		0x10
 */
#define V4L2_DEV_DEBUG_DMA	0x100

#define DDD_CTRL_IMAGE		0x01
#define DDD_CTRL_META		0x02
#define DDD_CTRL_TYPE_MASK

#define DDD_TYPE_PERIOD		0 /* all frame count matching period	 */
#define DDD_TYPE_INTERVAL	1 /* from any frame count to greater one */
#define DDD_TYPE_ONESHOT	2 /* specific frame count only		 */

static unsigned int dbg_dma_dump_ctrl;
static unsigned int dbg_dma_dump_type = DDD_TYPE_PERIOD;
static unsigned int dbg_dma_dump_arg1 = 30;	/* every frame(s) */
static unsigned int dbg_dma_dump_arg2;		/* for interval type */

static int param_get_dbg_dma_dump_ctrl(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "DMA dump control: ");

	if (!dbg_dma_dump_ctrl) {
		ret += sprintf(buffer + ret, "None\n");
		ret += sprintf(buffer + ret, "\t- image(0x1)\n");
		ret += sprintf(buffer + ret, "\t- meta (0x2)\n");
	} else {
		if (dbg_dma_dump_ctrl & DDD_CTRL_IMAGE)
			ret += sprintf(buffer + ret, "dump image(0x1) | ");
		if (dbg_dma_dump_ctrl & DDD_CTRL_META)
			ret += sprintf(buffer + ret, "dump meta (0x2) | ");

		ret -= 3;
		ret += sprintf(buffer + ret, "\n");
	}

	return ret;
}

static const struct kernel_param_ops param_ops_dbg_dma_dump_ctrl = {
	.set = param_set_uint,
	.get = param_get_dbg_dma_dump_ctrl,
};

static int param_get_dbg_dma_dump_type(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "DMA dump type: selected(*)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_PERIOD ?
				"\t- period(0)*\n" : "\t- period(0)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_INTERVAL ?
				"\t- interval(1)*\n" : "\t- interval(1)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_ONESHOT ?
				"\t- oneshot(2)*\n" : "\t- oneshot(2)\n");

	return ret;
}

static const struct kernel_param_ops param_ops_dbg_dma_dump_type = {
	.set = param_set_uint,
	.get = param_get_dbg_dma_dump_type,
};

module_param_cb(dma_dump_ctrl, &param_ops_dbg_dma_dump_ctrl,
			&dbg_dma_dump_ctrl, S_IRUGO | S_IWUSR);
module_param_cb(dma_dump_type, &param_ops_dbg_dma_dump_type,
			&dbg_dma_dump_type, S_IRUGO | S_IWUSR);
module_param_named(dma_dump_arg1, dbg_dma_dump_arg1, uint,
			S_IRUGO | S_IWUSR);
module_param_named(dma_dump_arg2, dbg_dma_dump_arg2, uint,
			S_IRUGO | S_IWUSR);

static void setup_plane_widths(struct is_frame_cfg *fc, u32 widths[], int cnt)
{
	int p;

	for (p = 0; p < cnt ; p++)
		widths[p] = max(fc->width * fc->format->bitsperpixel[p]
						/ BITS_PER_BYTE,
			       fc->bytesperline[p]);
}

/*
 * V4L2_PIX_FMT_YUV444, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_UYVY,
 * V4L2_PIX_FMT_Y10, V4L2_PIX_FMT_Y12,
 * V4L2_PIX_FMT_JPEG,
 * V4L2_PIX_FMT_Z16,
 */
static void widths0_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 1);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = widths[0] * fc->height;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_Y10BPACK */
static void aligned_widths0_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 1);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = ALIGN(widths[0], 16) * fc->height;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV16, V4L2_PIX_FMT_NV61 */
static void widths01_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = (widths[0] + widths[1]) * fc->height;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_NV21 */
static void widths01_420_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = widths[0] * fc->height
				+ widths[1] * fc->height / 2;

	sizes[p] = SIZE_OF_META_PLANE;
}

/*
 * V4L2_PIX_FMT_NV16M, V4L2_PIX_FMT_NV61M,
 * V4L2_PIX_FMT_Y8I
 */
static void widths01_2p_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = widths[0] * fc->height;
		sizes[p + 1] = widths[1] * fc->height;
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV12M, V4L2_PIX_FMT_NV21M */
static void widths01_2p_420_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = widths[0] * fc->height;
		sizes[p + 1] = widths[1] * fc->height / 2;
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV16M_P210 */
static void aligned_widths01_2p_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = ALIGN(widths[0], 16) * fc->height;
		sizes[p + 1] = ALIGN(widths[1], 16) * fc->height;
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV12M_P010 */
static void aligned_widths01_2p_420_sps(struct is_frame_cfg *fc,
					unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = ALIGN(widths[0], 16) * fc->height;
		sizes[p + 1] = ALIGN(widths[1], 16) * fc->height / 2;
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV16M_S10B */
static void aligned_2p_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = NV16M_Y_SIZE(fc->width, fc->height)
				+ NV16M_Y_2B_SIZE(fc->width, fc->height);
		sizes[p + 1] = NV16M_CBCR_SIZE(fc->width, fc->height)
				+ NV16M_CBCR_2B_SIZE(fc->width, fc->height);
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV12M_S10B */
static void aligned_2p_420_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = NV12M_Y_SIZE(fc->width, fc->height)
				+ NV12M_Y_2B_SIZE(fc->width, fc->height);
		sizes[p + 1] = NV12M_CBCR_SIZE(fc->width, fc->height)
				+ NV12M_CBCR_2B_SIZE(fc->width, fc->height);
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_YUV422P, V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_YVU420 */
static void widths012_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 3);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = widths[0] * fc->height
				+ widths[1] * fc->height / 2
				+ widths[2] * fc->height / 2;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_YUV420M, V4L2_PIX_FMT_YVU420M */
static void widths012_3p_420_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 3);
	for (p = 0; p < num_i_planes; p+=3) {
		sizes[p] = widths[0] * fc->height;
		sizes[p + 1] = widths[1] * fc->height / 2;
		sizes[p + 2] = widths[2] * fc->height / 2;
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_YUV32 */
static void widthsp_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, num_i_planes);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = widths[p] * fc->height;

	sizes[p] = SIZE_OF_META_PLANE;
}

/*
 * V4L2_PIX_FMT_SGRBG8, V4L2_PIX_FMT_SBGGR8,
 * V4L2_PIX_FMT_GREY
 */
static void default_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = fc->width * fc->height;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_RGB24, V4L2_PIX_FMT_BGR24 */
static void default_x3_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = fc->width * fc->height * 3;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_RGB32 */
static void default_x4_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = fc->width * fc->height * 4;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SBGGR10, V4L2_PIX_FMT_SBGGR10P */
#define ALIGN_SBGGR10(w) ALIGN(((w) * 5) >> 2, 16)
static void sbggr10x_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++) {
		sizes[p] = ALIGN_SBGGR10(fc->width) * fc->height;
		if (fc->bytesperline[0]) {
			if (fc->bytesperline[0] >= ALIGN_SBGGR10(fc->width))
				sizes[p] = fc->bytesperline[0] * fc->height;
			else
				err("bytesperline is too small"
					" (%s, width: %d, bytesperline: %d)",
						fmt->name, fc->width,
						fc->bytesperline[0]);
		}
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SBGGR12, V4L2_PIX_FMT_SBGGR12P */
#define ALIGN_SBGGR12(w) ALIGN(((w) * 3) >> 1, 16)
static void sbggr12x_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++) {
		sizes[p] = ALIGN_SBGGR12(fc->width) * fc->height;
		if (fc->bytesperline[0]) {
			if (fc->bytesperline[0] >= ALIGN_SBGGR12(fc->width))
				sizes[p] = fc->bytesperline[0] * fc->height;
			else
				err("bytesperline is too small"
					" (%s, width: %d, bytesperline: %d)",
						fmt->name, fc->width,
						fc->bytesperline[0]);
		}
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SBGGR16 */
static void sbggr16_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++) {
		sizes[p] = fc->width * fc->height * 2;
		if (fc->bytesperline[0]) {
			if (fc->bytesperline[0] >= fc->width * 2)
				sizes[p] = fc->bytesperline[0] * fc->height;
			else
				err("bytesperline is too small"
					" (%s, width: %d, bytesperline: %d)",
						fmt->name, fc->width,
						fc->bytesperline[0]);
		}
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SRGB24_SP */
static void srgb24_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = ALIGN(fc->width * fc->height, 16) * 3;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SRGB36P_SP */
static void srgb36p_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = ALIGN(fc->width * fc->height * 12 / 8, 16) * 3;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SRGB36_SP */
static void srgb36_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = ALIGN(fc->width * fc->height * 2, 16) * 3;

	sizes[p] = SIZE_OF_META_PLANE;
}

static struct is_fmt is_formats[] = {
	{
		.name		= "YUV 4:4:4 packed, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YUV444,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= 0, /* Not Defined */
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_INOUT_FORMAT_YUV444,
		.hw_order	= DMA_INOUT_ORDER_YCbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_YCbYCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_YCbYCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 packed, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbYCrY,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= widths01_sps,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= widths01_sps,
	}, {
		.name		= "YUV 4:2:2 non-contiguous 2-planar,, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= widths01_2p_sps,
	}, {
		.name		= "YUV 4:2:2 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV61M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= widths01_2p_sps,
	}, {
		.name		= "YUV 4:2:2 planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.setup_plane_sz	= widths012_sps,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.setup_plane_sz	= widths012_sps,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.setup_plane_sz	= widths012_sps,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= widths01_420_sps,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= widths01_420_sps,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= widths01_2p_420_sps,
	}, {
		.name		= "YVU 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= widths01_2p_420_sps,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.num_planes	= 3 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.setup_plane_sz	= widths012_3p_420_sps,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cr/Cb",
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.num_planes	= 3 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.setup_plane_sz	= widths012_3p_420_sps,
	}, {
		.name		= "BAYER 8 bit(GRBG)",
		.pixelformat	= V4L2_PIX_FMT_SGRBG8,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT, /* memory bitwidth */
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= default_sps,
	}, {
		.name		= "BAYER 8 bit(BA81)",
		.pixelformat	= V4L2_PIX_FMT_SBGGR8,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT, /* memory bitwidth */
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= default_sps,
	}, {
		.name		= "BAYER 10 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR10,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT, /* memory bitwidth */
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= sbggr10x_sps,
	}, {
		.name		= "BAYER 12 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT, /* memory bitwidth */
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= sbggr12x_sps,
	}, {
		.name		= "BAYER 16 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT, /* memory bitwidth */
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= sbggr16_sps,
	}, {
		.name		= "BAYER 10 bit packed",
		.pixelformat	= V4L2_PIX_FMT_SBGGR10P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER_PACKED,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= sbggr10x_sps,

	}, {
		.name		= "BAYER 12 bit packed",
		.pixelformat	= V4L2_PIX_FMT_SBGGR12P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER_PACKED,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_12BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= sbggr12x_sps,
	}, {
		.name		= "ARGB32",
		.pixelformat	= V4L2_PIX_FMT_ARGB32,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_ARGB,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= default_x4_sps,
	}, {
		.name		= "BGRA32",
		.pixelformat	= V4L2_PIX_FMT_BGRA32,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_BGRA,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= default_x4_sps,
	}, {
		.name		= "RGBA32",
		.pixelformat	= V4L2_PIX_FMT_RGBA32,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_RGBA,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= default_x4_sps,
	}, {
		.name		= "ABGR32",
		.pixelformat	= V4L2_PIX_FMT_ABGR32,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_BGRA,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= default_x4_sps,
	}, {
		.name		= "RGB24 planar",
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_BGR,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.setup_plane_sz	= default_x3_sps,
	}, {
		.name		= "BGR24 planar",
		.pixelformat	= V4L2_PIX_FMT_BGR24,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_BGR,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.setup_plane_sz	= default_x3_sps,
	}, {
		.name		= "BAYER 12bit packed single plane",
		.pixelformat	= V4L2_PIX_FMT_SRGB36P_SP,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_12BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.setup_plane_sz	= srgb36p_sps,
	}, {
		.name		= "BAYER 12bit unpacked single plane",
		.pixelformat	= V4L2_PIX_FMT_SRGB36_SP,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.setup_plane_sz	= srgb36_sps,
	}, {
		.name		= "BAYER 8bit single plane",
		.pixelformat	= V4L2_PIX_FMT_SRGB24_SP,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.setup_plane_sz	= srgb24_sps,
	}, {
		.name		= "Y 8bit",
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= default_sps,
	}, {
		.name		= "Y 10bit",
		.pixelformat	= V4L2_PIX_FMT_Y10,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "Y 12bit",
		.pixelformat	= V4L2_PIX_FMT_Y12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "Y Packed 10bit",
		.pixelformat	= V4L2_PIX_FMT_Y10BPACK,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.setup_plane_sz	= aligned_widths0_sps,
	}, {
		.name		= "Y L/R interleaved 8bit",
		.pixelformat	= V4L2_PIX_FMT_Y8I,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= widths01_2p_sps,
	}, {
		.name		= "YUV32",
		.pixelformat	= V4L2_PIX_FMT_YUV32,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= widthsp_sps,
	}, {
		.name		= "P210_16B",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= aligned_widths01_2p_sps,
	}, {
		.name		= "P210_12B",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12, 12 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_12BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= aligned_widths01_2p_sps,
	}, {
		.name		= "P210_10B",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10, 10 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= aligned_widths01_2p_sps,
	}, {
		.name		= "P010_16B",
		.pixelformat	= V4L2_PIX_FMT_NV12M_P010,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= aligned_widths01_2p_420_sps,
	}, {
		.name		= "P010_10B",
		.pixelformat	= V4L2_PIX_FMT_NV12M_P010,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10, 10 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.setup_plane_sz	= aligned_widths01_2p_420_sps,
	}, {
		.name		= "YUV422 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV16M_S10B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_4,
		.setup_plane_sz	= aligned_2p_sps,
	}, {
		.name		= "YUV420 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV12M_S10B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_4,
		.setup_plane_sz	= aligned_2p_420_sps,
	}, {
		.name		= "JPEG",
		.pixelformat	= V4L2_PIX_FMT_JPEG,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_JPEG_1X8,
		.bitsperpixel	= { 8 },
		.hw_format	= 0,
		.hw_order	= 0,
		.hw_bitwidth	= 0,
		.hw_plane	= 0,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "DEPTH",
		.pixelformat	= V4L2_PIX_FMT_Z16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= 0,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= 0,
		.hw_plane	= 0,
		.setup_plane_sz	= widths0_sps,
	}
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct is_fmt *pablo_kunit_get_is_formats_struct(ulong index) {
	if (ARRAY_SIZE(is_formats) <= index)
		return NULL;

	return &is_formats[index];
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_is_formats_struct);

ulong pablo_kunit_get_array_size_is_formats(void) {
	return ARRAY_SIZE(is_formats);
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_array_size_is_formats);
#endif

struct is_fmt *is_find_format(u32 pixelformat,
	u32 flags)
{
	ulong i;
	struct is_fmt *result, *fmt;
	u8 pixel_size;
	u32 memory_bitwidth;

	if (!pixelformat) {
		err("pixelformat is null");
		return NULL;
	}

	pixel_size = flags & PIXEL_TYPE_SIZE_MASK;
	if (pixel_size == CAMERA_PIXEL_SIZE_10BIT ||
			pixel_size == CAMERA_PIXEL_SIZE_12BIT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	else if (pixel_size == CAMERA_PIXEL_SIZE_PACKED_12BIT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
	else if (pixel_size == CAMERA_PIXEL_SIZE_PACKED_10BIT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	else if (pixel_size == CAMERA_PIXEL_SIZE_8_2BIT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	else
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;

	result = NULL;
	for (i = 0; i < ARRAY_SIZE(is_formats); ++i) {
		fmt = &is_formats[i];
		if (fmt->pixelformat == pixelformat) {
			if (pixelformat == V4L2_PIX_FMT_NV16M_P210
				|| pixelformat == V4L2_PIX_FMT_NV12M_P010
				|| pixelformat == V4L2_PIX_FMT_YUYV) {
				if (fmt->hw_bitwidth != memory_bitwidth)
					continue;
			}
			result = fmt;
			break;
		}
	}

	return result;
}

static inline void vref_init(struct is_video *video)
{
	atomic_set(&video->refcount, 0);
}

static inline int vref_get(struct is_video *video)
{
	return atomic_inc_return(&video->refcount) - 1;
}

static inline int vref_put(struct is_video *video,
	void (*release)(struct is_video *video))
{
	int ret = 0;

	ret = atomic_sub_and_test(1, &video->refcount);
	if (ret)
		pr_debug("closed all instacne");

	return atomic_read(&video->refcount);
}

static int queue_init(void *priv, struct vb2_queue *vbq,
	struct vb2_queue *vbq_dst)
{
	int ret = 0;
	struct is_video_ctx *vctx = priv;
	struct is_video *video;
	u32 type;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!vbq);

	video = GET_VIDEO(vctx);

	if (video->video_type == IS_VIDEO_TYPE_CAPTURE)
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	vbq->type		= type;
	vbq->io_modes		= VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	vbq->drv_priv		= vctx;
	vbq->buf_struct_size	= sizeof(struct is_vb2_buf);
	vbq->ops		= video->vb2_ops;
	vbq->mem_ops		= video->vb2_mem_ops;
	vbq->timestamp_flags	= V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(vbq);
	if (ret) {
		mverr("vb2_queue_init fail(%d)", vctx, video, ret);
		goto p_err;
	}

	vctx->queue.vbq = vbq;

p_err:
	return ret;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Pablo queue operaions                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static int is_queue_open(struct is_queue *queue,
	u32 rdycount)
{
	int ret = 0;

	queue->buf_maxcount = 0;
	queue->buf_refcount = 0;
	queue->buf_rdycount = rdycount;
	queue->buf_req = 0;
	queue->buf_pre = 0;
	queue->buf_que = 0;
	queue->buf_com = 0;
	queue->buf_dqe = 0;
	clear_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);
	clear_bit(IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(IS_QUEUE_STREAM_ON, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_KMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state);
	memset(&queue->framecfg, 0, sizeof(struct is_frame_cfg));
	frame_manager_probe(&queue->framemgr, queue->id, queue->name);

	return ret;
}

static int is_queue_close(struct is_queue *queue)
{
	int ret = 0;

	queue->buf_maxcount = 0;
	queue->buf_refcount = 0;
	clear_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);
	clear_bit(IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(IS_QUEUE_STREAM_ON, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_KMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state);
	frame_manager_close(&queue->framemgr);

	return ret;
}

static int is_queue_set_format_mplane(struct is_video_ctx *vctx,
	struct is_queue *queue,
	void *device,
	struct v4l2_format *format)
{
	int ret = 0;
	u32 plane;
	struct v4l2_pix_format_mplane *pix;
	struct is_fmt *fmt;
	struct is_video *video;

	FIMC_BUG(!queue);

	video = GET_VIDEO(vctx);

	pix = &format->fmt.pix_mp;
	fmt = is_find_format(pix->pixelformat, pix->flags);
	if (!fmt) {
		mverr("[%s] pixel format is not found", vctx, video, queue->name);
		ret = -EINVAL;
		goto p_err;
	}

	queue->framecfg.format		= fmt;
	queue->framecfg.colorspace	= pix->colorspace;
	queue->framecfg.quantization	= pix->quantization;
	queue->framecfg.width		= pix->width;
	queue->framecfg.height		= pix->height;
	queue->framecfg.hw_pixeltype	= pix->flags;

	for (plane = 0; plane < fmt->hw_plane; ++plane) {
		if (pix->plane_fmt[plane].bytesperline) {
			queue->framecfg.bytesperline[plane] =
				pix->plane_fmt[plane].bytesperline;
		} else {
			queue->framecfg.bytesperline[plane] = 0;
		}
	}

	ret = CALL_QOPS(queue, s_fmt, device, queue);
	if (ret) {
		mverr("[%s] s_fmt is fail(%d)", vctx, video, queue->name, ret);
		goto p_err;
	}

	mvinfo("[%s]pixelformat(%c%c%c%c) bit(%d) size(%dx%d) flag(0x%x)\n",
		vctx, video, queue->name,
		(char)((fmt->pixelformat >> 0) & 0xFF),
		(char)((fmt->pixelformat >> 8) & 0xFF),
		(char)((fmt->pixelformat >> 16) & 0xFF),
		(char)((fmt->pixelformat >> 24) & 0xFF),
		queue->framecfg.format->hw_bitwidth,
		queue->framecfg.width,
		queue->framecfg.height,
		queue->framecfg.hw_pixeltype);
p_err:
	return ret;
}

static void is_queue_subbuf_draw_digit(struct is_queue *queue,
		struct is_frame *frame)
{
	struct is_debug_dma_info dinfo;
	struct is_sub_node *snode = &frame->cap_node;
	struct is_sub_dma_buf *sdbuf;
	struct camera2_node *node;
	struct is_fmt *fmt;
	u32 n, b;

	/* Only handle 1st plane */
	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		node = &frame->shot_ext->node_group.capture[n];
		if (!node->request)
			continue;

		sdbuf = &queue->out_buf[frame->index].sub[n];
		fmt = is_find_format(node->pixelformat, node->flags);
		if (!fmt) {
			warn("[%s][I%d][F%d] pixelformat(%c%c%c%c) is not found",
				queue->name,
				frame->index,
				frame->fcount,
				(char)((node->pixelformat >> 0) & 0xFF),
				(char)((node->pixelformat >> 8) & 0xFF),
				(char)((node->pixelformat >> 16) & 0xFF),
				(char)((node->pixelformat >> 24) & 0xFF));

			continue;
		}

		dinfo.width = node->output.cropRegion[2];
		dinfo.height = node->output.cropRegion[3];
		dinfo.pixeltype = node->flags;
		dinfo.bpp = fmt->bitsperpixel[0];
		dinfo.pixelformat = fmt->pixelformat;

		for (b = 0; b < sdbuf->num_buffer; b++) {
			dinfo.addr = snode->sframe[n].kva[b * sdbuf->num_plane];

			if (dinfo.addr)
				is_dbg_draw_digit(&dinfo, frame->fcount);
		}
	}
}

static void is_queue_draw_digit(struct is_queue *queue, struct is_vb2_buf *vbuf)
{
	struct is_debug_dma_info dinfo;
	struct is_video_ctx *vctx = container_of(queue, struct is_video_ctx, queue);
	struct is_video *video = GET_VIDEO(vctx);
	struct is_frame_cfg *framecfg = &queue->framecfg;
	struct is_frame *frame;
	u32 index = vbuf->vb.vb2_buf.index;
	u32 num_buffer = vbuf->num_merged_dbufs ? vbuf->num_merged_dbufs : 1;
	u32 num_i_planes = vbuf->vb.vb2_buf.num_planes - NUM_OF_META_PLANE;
	u32 num_ext_planes = 0;
	u32 b;

	if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &vctx->queue.state))
		num_ext_planes = num_i_planes - vctx->queue.framecfg.format->hw_plane;

	num_i_planes -= num_ext_planes;
	frame = &queue->framemgr.frames[index];

	/* Draw digit on capture node buffer */
	if (video->video_type == IS_VIDEO_TYPE_CAPTURE) {
		dinfo.width = frame->width ? frame->width : framecfg->width;
		dinfo.height = frame->height ? frame->height : framecfg->height;
		dinfo.pixeltype = framecfg->hw_pixeltype;
		dinfo.bpp = framecfg->format->bitsperpixel[0];
		dinfo.pixelformat = framecfg->format->pixelformat;

		for (b = 0; b < num_buffer; b++) {
			dinfo.addr = queue->buf_kva[index][b * num_i_planes];

			if (dinfo.addr)
				is_dbg_draw_digit(&dinfo, frame->fcount);
		}
	}

	/* Draw digit on LVN sub node buffers */
	if (IS_ENABLED(LOGICAL_VIDEO_NODE) &&
			video->video_type == IS_VIDEO_TYPE_LEADER &&
			queue->mode == CAMERA_NODE_LOGICAL)
		is_queue_subbuf_draw_digit(queue, frame);
}

static int _is_queue_subbuf_prepare(struct device *dev,
		struct is_vb2_buf *vbuf,
		struct camera2_node_group *node_group,
		bool need_vmap)
{
	int ret;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct is_video *video = GET_VIDEO(vctx);
	struct is_queue *queue = GET_QUEUE(vctx);
	struct is_device_ischain *device = GET_DEVICE_ISCHAIN(vctx);
	struct camera2_node *node;
	struct is_sub_buf *sbuf;
	struct v4l2_plane planes[IS_MAX_PLANES];
	struct is_sub_dma_buf *sdbuf;
	u32 index = vb->index;
	u32 num_i_planes, n;
	unsigned int dbg_draw_digit_ctrl;
	struct is_mem *mem;

	mdbgv_lvn(3, "[%s][I%d]subbuf_prepare: queue 0x%lx\n", vctx, video,
			queue->name, index, queue);

	/* Extract input subbuf */
	node = &node_group->leader;
	if (!node->request)
		return 0;
	if (node->vid >= IS_VIDEO_MAX_NUM) {
		mverr("[%s][I%d]subbuf_prepare: invalid input (req:%d, vid:%d, length:%d)\n",
			vctx, video,
			queue->name, index,
			node->request, node->vid, node->buf.length);
	}

	node->result = 1;

	dbg_draw_digit_ctrl = is_get_digit_ctrl();
	/* Disable SBWC for drawing digit on it */
	if (dbg_draw_digit_ctrl)
		node->flags &= ~PIXEL_TYPE_EXTRA_MASK;

#ifdef ENABLE_LVN_DUMMYOUTPUT
	num_i_planes = node->buf.length - NUM_OF_META_PLANE;

	sbuf = &queue->in_buf[index];
	sbuf->ldr_vid = video->id;

	sdbuf = &sbuf->sub[0];
	sdbuf->vid = node->vid;
	sdbuf->num_plane = num_i_planes;

	if (copy_from_user(planes, node->buf.m.planes,
		sizeof(struct v4l2_plane) * num_i_planes) != 0) {
		mverr("[%s][%s][I%d] Failed copy_from_user", vctx, video,
				queue->name,
				vn_name[node->vid],
				index);
		return -EFAULT;
	}

	mem = is_hw_get_iommu_mem(node->vid);
	vbuf->ops->subbuf_prepare(sdbuf, planes, mem->dev);

	ret = vbuf->ops->subbuf_dvmap(sdbuf);
	if (ret) {
		mverr("[%s][%s][I%d]Failed to get dva", vctx, video,
			queue->name,
			vn_name[node->vid],
			index);

		return ret;
	}
#endif

	/* Extract output subbuf */
	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		node = &node_group->capture[n];
		if (!node->request)
			continue;

		if (node->buf.length > IS_MAX_PLANES ||
			node->vid >= IS_VIDEO_MAX_NUM) {
			mverr("[%s][I%d]subbuf_prepare: invalid output[%d] (req:%d, vid:%d, length:%d) input (req:%d, vid:%d)\n",
				vctx, video,
				queue->name, index,
				n,
				node->request, node->vid, node->buf.length,
				node_group->leader.request, node_group->leader.vid);
			/*
			 * FIXME :
			 * For normal error handling, it would be nice to return error to user
			 * but condition of system need to be preserved because of some issues
			 * So, temporarily BUG() is used here.
			 */
			BUG();
		} else if (!node->buf.length) {
			mdbgv_lvn(2, "[%s][%s][I%d]subbuf_prepare: Invalid buf.length %d",
					vctx, video, queue->name, vn_name[node->vid],
					index, node->buf.length);
			continue;
		}

		node->result = 1;
		num_i_planes = node->buf.length - NUM_OF_META_PLANE;

		sbuf = &queue->out_buf[index];
		sbuf->ldr_vid = video->id;

		sdbuf = &sbuf->sub[n];
		sdbuf->vid = node->vid;
		sdbuf->num_plane = num_i_planes;

		if (copy_from_user(planes, node->buf.m.planes,
				sizeof(struct v4l2_plane) * num_i_planes) != 0) {
			mverr("[%s][%s][I%d] Failed copy_from_user", vctx, video,
					queue->name,
					vn_name[node->vid],
					index);
			continue;
		}

		mem = is_hw_get_iommu_mem(node->vid);
		vbuf->ops->subbuf_prepare(sdbuf, planes, mem->dev);

		ret = vbuf->ops->subbuf_dvmap(sdbuf);
		if (ret) {
			mverr("[%s][%s][I%d]Failed to get dva", vctx, video,
					queue->name,
					vn_name[node->vid],
					index);

			return ret;
		}

		if (need_vmap || CHECK_NEED_KVADDR_ID(sdbuf->vid)) {
			ret = vbuf->ops->subbuf_kvmap(sdbuf);
			if (ret) {
				mverr("[%s][%s][I%d]Failed to get dva", vctx, video,
						queue->name,
						vn_name[node->vid],
						index);

				return ret;
			}

			/* need cache maintenance */
			is_q_dbuf_q(device->dbuf_q, sdbuf, vb->vb2_queue->queued_count);

			/* Disable SBWC for drawing digit on it */
			if (dbg_draw_digit_ctrl)
				node->flags &= ~PIXEL_TYPE_EXTRA_MASK;
		}
	}

	return 0;
}

int _is_queue_buffer_tag(dma_addr_t saddr[], dma_addr_t taddr[],
	u32 pixelformat, u32 width, u32 height, u32 planes, u32 *hw_planes)
{
	int i, j;
	int ret = 0;

	*hw_planes = planes;
	switch (pixelformat) {
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		for (i = 0; i < planes; i++) {
			j = i * 2;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
		}
		*hw_planes = planes * 2;
		break;
	case V4L2_PIX_FMT_YVU420M:
		for (i = 0; i < planes; i += 3) {
			taddr[i] = saddr[i];
			taddr[i + 1] = saddr[i + 2];
			taddr[i + 2] = saddr[i + 1];
		}
		break;
	case V4L2_PIX_FMT_YUV420:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
			taddr[j + 2] = taddr[j + 1] + (width * height / 4);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_YVU420: /* AYV12 spec: The width should be aligned by 16 pixel. */
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 2] = taddr[j] + (ALIGN(width, 16) * height);
			taddr[j + 1] = taddr[j + 2] + (ALIGN(width / 2, 16) * height / 2);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_YUV422P:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
			taddr[j + 2] = taddr[j + 1] + (width * height / 2);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_NV12M_S10B:
	case V4L2_PIX_FMT_NV21M_S10B:
		for (i = 0; i < planes; i += 2) {
			j = i * 2;
			/* Y_ADDR, UV_ADDR, Y_2BIT_ADDR, UV_2BIT_ADDR */
			taddr[j] = saddr[i];
			taddr[j + 1] = saddr[i + 1];
			taddr[j + 2] = taddr[j] + NV12M_Y_SIZE(width, height);
			taddr[j + 3] = taddr[j + 1] + NV12M_CBCR_SIZE(width, height);
		}
		break;
	case V4L2_PIX_FMT_NV16M_S10B:
	case V4L2_PIX_FMT_NV61M_S10B:
		for (i = 0; i < planes; i += 2) {
			j = i * 2;
			/* Y_ADDR, UV_ADDR, Y_2BIT_ADDR, UV_2BIT_ADDR */
			taddr[j] = saddr[i];
			taddr[j + 1] = saddr[i + 1];
			taddr[j + 2] = taddr[j] + NV16M_Y_SIZE(width, height);
			taddr[j + 3] = taddr[j + 1] + NV16M_CBCR_SIZE(width, height);
		}
		break;
	case V4L2_PIX_FMT_RGB24:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j + 2] = saddr[i];
			taddr[j + 1] = taddr[j + 2] + (width * height);
			taddr[j] = taddr[j + 1] + (width * height);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_BGR24:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
			taddr[j + 2] = taddr[j + 1] + (width * height);
		}
		*hw_planes = planes * 3;
		break;
	default:
		for (i = 0; i < planes; i++)
			taddr[i] = saddr[i];
		break;
	}

	return ret;
}

static int _is_queue_subbuf_queue(struct is_video_ctx *vctx,
		struct is_queue *queue,
		struct is_frame *frame)
{
	struct is_video *video = GET_VIDEO(vctx);
	struct camera2_node *node = NULL;
	struct is_sub_node * snode = NULL;
	struct is_sub_dma_buf *sdbuf;
	struct is_crop *crop;
	u32 index = frame->index;
	u32 n, stride_w, stride_h, hw_planes = 0;

#ifdef ENABLE_LVN_DUMMYOUTPUT
	/* Extract input subbuf information */
	node = &frame->shot_ext->node_group.leader;
	if (!node->request)
		return 0;

	snode = &frame->out_node;
	if (is_hw_get_output_slot(node->vid) < 0) {
		mverr("[%s][I%d]Invalid vid %d", vctx, video,
				queue->name,
				index,
				node->vid);
		return -EINVAL;
	}

	sdbuf = &queue->in_buf[index].sub[0];

	/* Setup input subframe */
	snode->sframe[0].id = sdbuf->vid;
	snode->sframe[0].num_planes = sdbuf->num_plane * sdbuf->num_buffer;
	crop = (struct is_crop *)node->input.cropRegion;
	stride_w = max(node->width, crop->w);
	stride_h = max(node->height, crop->h);

	_is_queue_buffer_tag(sdbuf->dva,
			snode->sframe[0].dva,
			node->pixelformat,
			stride_w, stride_h
			snode->sframe[0].num_planes,
			&hw_planes);

	snode->sframe[0].num_planes = hw_planes;
#endif

	/* Extract output subbuf information */
	snode = &frame->cap_node;
	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		/* clear first */
		snode->sframe[n].id = 0;

		if (snode->sframe[n].kva[0])
			memset(snode->sframe[n].kva, 0x0,
				sizeof(ulong) * snode->sframe[n].num_planes);

		node = &frame->shot_ext->node_group.capture[n];
		if (!node->request)
			continue;

		sdbuf = &queue->out_buf[index].sub[n];

		/* Setup output subframe */
		snode->sframe[n].id = sdbuf->vid;
		snode->sframe[n].num_planes
			= sdbuf->num_plane * sdbuf->num_buffer;
		crop = (struct is_crop *)node->output.cropRegion;
		stride_w = max(node->width, crop->w);
		stride_h = max(node->height, crop->h);

		_is_queue_buffer_tag(sdbuf->dva,
				snode->sframe[n].dva,
				node->pixelformat,
				stride_w, stride_h,
				snode->sframe[n].num_planes,
				&hw_planes);

		snode->sframe[n].num_planes = hw_planes;

		if (sdbuf->kva[0]) {
			mdbgv_lvn(2, "[%s][%s] 0x%lx\n", vctx, video,
				queue->name, vn_name[node->vid], sdbuf->kva[0]);

			memcpy(snode->sframe[n].kva, sdbuf->kva,
				sizeof(ulong) * snode->sframe[n].num_planes);
		}

		mdbgv_lvn(4, "[%s][%s][N%d][I%d] pixelformat %c%c%c%c size %dx%d(%dx%d) length %d\n",
			vctx, video,
			queue->name, vn_name[node->vid],
			n, index,
			(char)((node->pixelformat >> 0) & 0xFF),
			(char)((node->pixelformat >> 8) & 0xFF),
			(char)((node->pixelformat >> 16) & 0xFF),
			(char)((node->pixelformat >> 24) & 0xFF),
			crop->w, crop->h,
			stride_w, stride_h,
			node->buf.length);
	}

	return 0;
}

int __find_mapped_index(__s32 target_fd, struct is_sub_buf buf[], int nidx, int pidx)
{
	int ret = -1;
#ifndef ENABLE_SKIP_PER_FRAME_MAP
	int j;

	for (j = 0; j < IS_MAX_BUFS; j++) {
		if (target_fd == buf[j].sub[nidx].buf_fd[pidx]) {
			ret = j;
			break;
		}
	}

#endif
	return ret;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Pablo vb2 operaions                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static u32 get_instance_video_ctx(struct is_video_ctx *ivc)
{
	struct is_video *iv = ivc->video;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		idi = (struct is_device_ischain *)ivc->device;
		return idi->instance;
	} else {
		ids = (struct is_device_sensor *)ivc->device;
		return ids->instance;
	}
}

static void mdbgv_video(struct is_video_ctx *ivc, struct is_video *iv,
							const char *msg)
{
	if (iv->device_type == IS_DEVICE_ISCHAIN)
		mdbgv_ischain("%s\n", ivc, msg);
	else
		mdbgv_sensor("%s\n", ivc, msg);
}

int is_queue_setup(struct vb2_queue *vq,
		unsigned *nbuffers, unsigned *nplanes,
		unsigned sizes[], struct device *alloc_devs[])
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vq->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_queue *iq = &ivc->queue;
	struct is_frame_cfg *ifc = &iq->framecfg;
	struct is_fmt *fmt = ifc->format;
	int p;
	struct is_mem *mem;

	*nplanes = (unsigned int)fmt->num_planes;

	if (fmt->setup_plane_sz) {
		fmt->setup_plane_sz(ifc, sizes);
	} else {
		err("failed to setup plane sizes for pixelformat(%c%c%c%c)",
			(char)((fmt->pixelformat >> 0) & 0xFF),
			(char)((fmt->pixelformat >> 8) & 0xFF),
			(char)((fmt->pixelformat >> 16) & 0xFF),
			(char)((fmt->pixelformat >> 24) & 0xFF));

			return -EINVAL;
	}

	/* FIXME: 1. share meta plane, 2. configure the size */
	if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &iq->state)) {
		sizes[*nplanes] = SZ_256K + SZ_256K;
		*nplanes += NUM_OF_EXT_PLANE;
	}

	mem = is_hw_get_iommu_mem(iv->id);

	for (p = 0; p < *nplanes; p++) {
		alloc_devs[p] = mem->dev;
		ifc->size[p] = sizes[p];
		mdbgv_vid("queue[%d] size : %d\n", p, sizes[p]);
	}

	mdbgv_video(ivc, iv, __func__);

	return 0;
}

void is_wait_prepare(struct vb2_queue *vbq)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vbq->drv_priv;
	struct is_video *iv = ivc->video;

	mutex_unlock(&iv->lock);
}

void is_wait_finish(struct vb2_queue *vbq)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vbq->drv_priv;
	struct is_video *iv = ivc->video;

	mutex_lock(&iv->lock);
}

int is_buf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_group *ig = ivc->group;
	struct is_queue *iq = &ivc->queue;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_groupmgr *grpmgr;
	u32 instance = get_instance_video_ctx(ivc);
	int ret;

	mvdbgs(3, "%s(%d)\n", ivc, iq, __func__, vb->index);

	/* for each leader */
	if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			idi = (struct is_device_ischain *)ivc->device;
			grpmgr = idi->groupmgr;

			mdbgs_ischain(4, "%s\n", idi, __func__);
		} else {
			ids = (struct is_device_sensor *)ivc->device;
			grpmgr = ids->groupmgr;

			mdbgs_sensor(4, "%s\n", ids, __func__);
		}

		ret = pablo_group_buffer_init(grpmgr, ig, vb->index);
		if (ret)
			mierr("failure in pablo_group_buffer_init(): %d", instance, ret);
	/* for each non-leader */
	} else {
		ret = pablo_subdev_buffer_init(ivc->subdev, vb);
		if (ret)
			mierr("failure in pablo_subdev_buffer_init(): %d", instance, ret);
	}

	vbuf->ops = iv->is_vb2_buf_ops;

	return 0;
}

int is_buf_prepare(struct vb2_buffer *vb)
{
	int ret;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct is_queue *queue = GET_QUEUE(vctx);
	struct is_video *video = GET_VIDEO(vctx);
	unsigned int index = vb->index;
	unsigned int dbg_draw_digit_ctrl;
	struct is_frame *frame = &queue->framemgr.frames[index];
	u32 num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	u32 num_buffers = 1, num_shots = 1, pos_meta_p, i;
	ulong kva_meta;
	bool need_vmap;
	struct is_mem *mem;

	dbg_draw_digit_ctrl = is_get_digit_ctrl();
	/* take a snapshot whether it is needed or not */
	need_vmap = (dbg_draw_digit_ctrl ||
			((dbg_dma_dump_ctrl & ~DDD_CTRL_META)
			 && (video->vd.dev_debug & V4L2_DEV_DEBUG_DMA))
		    );

	if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state))
		num_i_planes -= NUM_OF_EXT_PLANE;

	/* Unmerged dma_buf_container */
	if (IS_ENABLED(DMABUF_CONTAINER)) {
		mem = is_hw_get_iommu_mem(video->id);
		ret = vbuf->ops->dbufcon_prepare(vbuf, mem->dev);
		if (ret) {
			mverr("failed to prepare dmabuf-container: %d",
					vctx, video, index);
			return ret;
		}

		if (vbuf->num_merged_dbufs) {
			ret = vbuf->ops->dbufcon_map(vbuf);
			if (ret) {
				mverr("failed to map dmabuf-container: %d",
						vctx, video, index);
				vbuf->ops->dbufcon_finish(vbuf);
				return ret;
			}

			num_buffers = vbuf->num_merged_dbufs;
		}

		/* Unmerged meta plane */
		ret = vbuf->ops->dbufcon_kmap(vbuf, num_i_planes);
		if (ret) {
			mverr("failed to kmap dmabuf-container: %d",
					vctx, video, index);
			return ret;
		}

		if (vbuf->num_merged_sbufs)
			num_shots = vbuf->num_merged_sbufs;
	}

	/* Get kva of image planes */
	if (test_bit(IS_QUEUE_NEED_TO_KMAP, &queue->state)) {
		for (i = 0; i < num_i_planes; i++)
			vbuf->ops->plane_kmap(vbuf, i, 0);
	} else if (need_vmap) {
		if (vbuf->num_merged_dbufs) {
			for (i = 0; i < num_i_planes; i++)
				vbuf->ops->dbufcon_kmap(vbuf, i);
		} else {
			for (i = 0; i < num_i_planes; i++)
				vbuf->kva[i] = vbuf->ops->plane_kvaddr(vbuf, i);
		}
	}

	/* Disable SBWC for drawing digit on it */
	if (dbg_draw_digit_ctrl)
		queue->framecfg.hw_pixeltype &= ~PIXEL_TYPE_EXTRA_MASK;

	/* Get metadata planes */
	pos_meta_p = num_i_planes * num_buffers;
	if (num_shots > 1) {
		for (i = 0; i < num_shots; i++)
			queue->buf_kva[index][pos_meta_p + i] =
				vbuf->kva[pos_meta_p + i];
	} else {
		kva_meta = vbuf->ops->plane_kmap(vbuf, num_i_planes, 0);
		if (!kva_meta) {
			mverr("[%s][I%d][P%d]Failed to get kva", vctx, video,
					queue->name, index, num_i_planes);
			return -ENOMEM;
		}

		queue->buf_kva[index][pos_meta_p] = kva_meta;
	}

	/* Setup frame */
	frame->num_buffers = num_buffers;
	frame->planes = num_i_planes * num_buffers;
	frame->num_shots = num_shots;
	kva_meta = queue->buf_kva[index][pos_meta_p];
	if (video->video_type == IS_VIDEO_TYPE_LEADER) {
		/* Output node */
		frame->shot_ext = (struct camera2_shot_ext *)kva_meta;
		frame->shot = &frame->shot_ext->shot;
		frame->shot_size = sizeof(frame->shot);
		if (sizeof(struct camera2_shot_ext) > SIZE_OF_META_PLANE) {
			mverr("Meta size overflow %d", vctx, video,
					sizeof(struct camera2_shot_ext));
			FIMC_BUG(1);
		}

		if (frame->shot->magicNumber != SHOT_MAGIC_NUMBER) {
			mverr("[%s][I%d]Shot magic number error! 0x%08X size %zd",
					vctx, video,
					queue->name, index,
					frame->shot->magicNumber,
					sizeof(struct camera2_shot_ext));
			return -EINVAL;
		}
	} else {
		/* Capture node */
		frame->stream = (struct camera2_stream *)kva_meta;

		/* TODO : Change type of address into ulong */
		frame->stream->address = (u32)kva_meta;
	}

	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		queue->mode = CAMERA_NODE_LOGICAL;

		if (video->video_type == IS_VIDEO_TYPE_LEADER
				&& queue->mode == CAMERA_NODE_LOGICAL) {
			ret = _is_queue_subbuf_prepare(video->alloc_dev, vbuf,
					&frame->shot_ext->node_group, need_vmap);
			if (ret) {
				mverr("[%s][I%d]Failed to subbuf_prepare",
						vctx, video, queue->name, index);
				return ret;
			}
		}
	}

	if (test_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state)) {
		ret = vbuf->ops->remap_attr(vbuf, 0);
		if (ret) {
			mverr("failed to remap dmabuf: %d", vctx, video, index);
			return ret;
		}
	}

	queue->buf_pre++;

	return 0;
}

#define V4L2_BUF_FLAG_DISPOSAL	0x10000000
static void __is_buf_finish(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *ivb = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct vb2_queue *vbq = vb->vb2_queue;
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_queue *iq = &ivc->queue;
	u32 framecount = iq->framemgr.frames[vb->index].fcount;
	bool ddd_trigger = false;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	unsigned int num_ext_planes = 0;
	int p;
	struct vb2_plane *vbp;
#if !defined(ENABLE_SKIP_PER_FRAME_MAP)
	unsigned int index = vb->index;
	struct is_sub_buf *sbuf;
	struct is_sub_dma_buf *sdbuf;
	u32 n;
#endif

	if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &iq->state))
		num_ext_planes = num_i_planes - iq->framecfg.format->hw_plane;

	num_i_planes -= num_ext_planes;
	if (is_get_digit_ctrl())
		is_queue_draw_digit(iq, ivb);

	if (iv->vd.dev_debug & V4L2_DEV_DEBUG_DMA) {
		if (dbg_dma_dump_type == DDD_TYPE_PERIOD)
			ddd_trigger = !(framecount % dbg_dma_dump_arg1);
		else if (dbg_dma_dump_type == DDD_TYPE_INTERVAL)
			ddd_trigger = (framecount >= dbg_dma_dump_arg1)
				&& (framecount <= dbg_dma_dump_arg2);
		else if (dbg_dma_dump_type == DDD_TYPE_ONESHOT)
			ddd_trigger = (framecount == dbg_dma_dump_arg1);

		if (ddd_trigger && (dbg_dma_dump_ctrl & DDD_CTRL_IMAGE))
			is_dbg_dma_dump(iq, ivc->instance, vb->index,
					iv->id, DBG_DMA_DUMP_IMAGE);

		if (ddd_trigger && (dbg_dma_dump_ctrl & DDD_CTRL_META))
			is_dbg_dma_dump(iq, ivc->instance, vb->index,
					iv->id, DBG_DMA_DUMP_META);
	}

	if (IS_ENABLED(LOGICAL_VIDEO_NODE) &&
			iq->mode == CAMERA_NODE_LOGICAL) {
		mdbgv_lvn(3, "[%s][I%d]buf_finish: queue 0x%lx\n",
				ivc, iv,
				vn_name[iv->id], index, iq);
#ifndef ENABLE_SKIP_PER_FRAME_MAP
#ifdef ENABLE_LVN_DUMMYOUTPUT
		/* release input */
		sbuf = &iq->in_buf[index];
		sdbuf = &sbuf->sub[0];
		ivb->ops->subbuf_finish(sdbuf);
		sbuf->ldr_vid = 0;
#endif
		/* release output */
		for (n = 0; n < CAPTURE_NODE_MAX; n++) {
			sbuf = &iq->out_buf[index];
			sdbuf = &sbuf->sub[n];
			if (!sdbuf->vid)
				continue;

			if (sdbuf->kva[0])
				ivb->ops->subbuf_kunmap(sdbuf);

			ivb->ops->subbuf_finish(sdbuf);
			sbuf->ldr_vid = 0;
		}
#endif
	}

	if (IS_ENABLED(DMABUF_CONTAINER) && (ivb->num_merged_dbufs)) {
		for (p = 0; p < num_i_planes && ivb->kva[p]; p++)
			ivb->ops->dbufcon_kunmap(ivb, p);

		ivb->ops->dbufcon_unmap(ivb);
		ivb->ops->dbufcon_finish(ivb);
	}

	if (test_bit(IS_QUEUE_NEED_TO_REMAP, &iq->state))
		ivb->ops->unremap_attr(ivb, 0);

	if ((vb2_v4l2_buf->flags & V4L2_BUF_FLAG_DISPOSAL) &&
			vbq->memory == VB2_MEMORY_DMABUF) {
		is_buf_cleanup(vb);

		for (p = 0; p < vb->num_planes; p++) {
			vbp = &vb->planes[p];

			if (!vbp->mem_priv)
				continue;

			if (vbp->dbuf_mapped)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
				vbq->mem_ops->unmap_dmabuf(vbp->mem_priv);
#else
				vbq->mem_ops->unmap_dmabuf(vbp->mem_priv, 0);
#endif

			vbq->mem_ops->detach_dmabuf(vbp->mem_priv);
			dma_buf_put(vbp->dbuf);
			vbp->mem_priv = NULL;
			vbp->dbuf = NULL;
			vbp->dbuf_mapped = 0;
		}
	}
}

void is_buf_finish(struct vb2_buffer *vb)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_queue *iq = &ivc->queue;
	struct is_group *ig = ivc->group;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_groupmgr *grpmgr;
	u32 instance = get_instance_video_ctx(ivc);
	int ret;

	mvdbgs(3, "%s(%d)\n", ivc, iq, __func__, vb->index);

	/* for each leader */
	if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			idi = (struct is_device_ischain *)ivc->device;
			grpmgr = idi->groupmgr;

			mdbgs_ischain(4, "%s\n", idi, __func__);
		} else {
			ids = (struct is_device_sensor *)ivc->device;
			grpmgr = ids->groupmgr;

			mdbgs_sensor(4, "%s\n", ids, __func__);
		}

		ret = is_group_buffer_finish(grpmgr, ig, vb->index);
		if (ret)
			mierr("failure in is_group_buffer_finish(): %d", instance, ret);
	/* for each non-leader */
	} else {
		ret = is_subdev_buffer_finish(ivc->subdev, vb);
		if (ret)
			mierr("failure in is_subdev_buffer_finish(): %d", instance, ret);
	}

	__is_buf_finish(vb);
}

void is_buf_cleanup(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	unsigned int num_m_planes;
	unsigned int pos_ext_p;
	int i;

	if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &vctx->queue.state))
		num_i_planes -= NUM_OF_EXT_PLANE;

	num_m_planes = vbuf->num_merged_sbufs ? vbuf->num_merged_sbufs : 1;

	/* FIXME: doesn't support dmabuf container yet */
	if (test_bit(IS_QUEUE_NEED_TO_KMAP, &vctx->queue.state)) {
		for (i = 0; i < vb->num_planes; i++)
			vbuf->ops->plane_kunmap(vbuf, i, 0);
	} else {
		if (vbuf->num_merged_sbufs)
			vbuf->ops->dbufcon_kunmap(vbuf, num_i_planes);
		else
			vbuf->ops->plane_kunmap(vbuf, num_i_planes, 0);

		if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &vctx->queue.state)) {
			pos_ext_p = num_i_planes + num_m_planes;
			for (i = 0; i < NUM_OF_EXT_PLANE; i++)
				vbuf->ops->plane_kunmap(vbuf, pos_ext_p + i, 0);
		}
	}
}

int is_start_streaming(struct vb2_queue *vbq, unsigned int count)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vbq->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_queue *iq = &ivc->queue;
	int ret;

	mdbgv_video(ivc, iv, __func__);

	if (test_bit(IS_QUEUE_STREAM_ON, &iq->state)) {
		err("[%s] already streaming", iq->name);
		return -EINVAL;
	}

	if (iq->buf_rdycount &&
			!test_bit(IS_QUEUE_BUFFER_READY, &iq->state)) {
		err("[%s] need at least %u buffers", iq->name);
		return -EINVAL;
	}

	ret = CALL_QOPS(iq, start_streaming, ivc->device, iq);
	if (ret) {
		err("[%s] failed to start_streaming for device: %d", iq->name, ret);
		return ret;
	}

	set_bit(IS_QUEUE_STREAM_ON, &iq->state);

	return 0;
}

void is_stop_streaming(struct vb2_queue *vbq)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vbq->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_queue *iq = &ivc->queue;
	int ret;

	mdbgv_video(ivc, iv, __func__);

	/*
	 * If you prepare or queue buffers, and then call streamoff without
	 * ever having called streamon, you would still expect those buffers
	 * to be returned to their normal dequeued state.
	 */
	if (!test_bit(IS_QUEUE_STREAM_ON, &iq->state))
		warn("[%s] streaming inactive", iq->name);

	ret = CALL_QOPS(iq, stop_streaming, ivc->device, iq);
	if (ret)
		err("[%s] failed to stop_streaming for device: %d", iq->name, ret);

	clear_bit(IS_QUEUE_STREAM_ON, &iq->state);
	clear_bit(IS_QUEUE_BUFFER_READY, &iq->state);
	clear_bit(IS_QUEUE_BUFFER_PREPARED, &iq->state);
}

static int __is_buf_queue(struct is_queue *iq, struct vb2_buffer *vb)
{
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
	struct is_video *iv = ivc->video;
	struct is_framemgr *framemgr = &iq->framemgr;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	unsigned int index = vb->index;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	unsigned int num_ext_planes = 0, pos_ext_p = 0;
	struct is_frame *frame;
	int i;
	int ret = 0;

	/* image planes */
	if (IS_ENABLED(DMABUF_CONTAINER) && vbuf->num_merged_dbufs) {
		/* vbuf has been sorted by the order of buffer */
		memcpy(iq->buf_dva[index], vbuf->dva,
			sizeof(dma_addr_t) * vbuf->num_merged_dbufs * num_i_planes);

		if (vbuf->kva[0])
			memcpy(iq->buf_kva[index], vbuf->kva,
				sizeof(ulong) * vbuf->num_merged_dbufs * num_i_planes);
		else
			memset(iq->buf_kva[index], 0x0,
				sizeof(ulong) * vbuf->num_merged_dbufs * num_i_planes);
	} else {
		/* if additional plane exist, map for use ext feature */
		if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &iq->state))
			num_ext_planes = NUM_OF_EXT_PLANE;

		num_i_planes -= num_ext_planes;

		for (i = 0; i < num_i_planes; i++) {
			if (test_bit(IS_QUEUE_NEED_TO_REMAP, &iq->state))
				iq->buf_dva[index][i] = vbuf->dva[i];
			else
				iq->buf_dva[index][i] = vbuf->ops->plane_dvaddr(vbuf, i);
		}

		if (vbuf->kva[0])
			memcpy(iq->buf_kva[index], vbuf->kva,
				sizeof(ulong) * num_i_planes);
		else
			memset(iq->buf_kva[index], 0x0,
				sizeof(ulong) * num_i_planes);
	}

	frame = &framemgr->frames[index];
	frame->ext_planes = num_ext_planes;

	/* ext plane if exist */
	if(test_bit(IS_QUEUE_NEED_TO_EXTMAP, &iq->state)) {
		pos_ext_p = (num_i_planes * frame->num_buffers) + NUM_OF_META_PLANE;
		for (i = pos_ext_p; i < pos_ext_p + num_ext_planes; i++) {
			iq->buf_kva[index][i] =
				vbuf->ops->plane_kmap(vbuf, i, 0);
			if (!iq->buf_kva[index][i]) {
				mverr("failed to get ext kva for %s", ivc, iv, iq->name);
				ret = -ENOMEM;
				goto err_get_kva_for_ext;
			}
			iq->buf_dva[index][i] =
				vbuf->ops->plane_dvaddr(vbuf, i);
			if (!iq->buf_dva[index][i]) {
				mverr("failed to get ext dva for %s", ivc,
						iv, iq->name);
				ret = -ENOMEM;
				goto err_get_dva_for_ext;
			}
		}
	}

	if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
#if defined(USE_OFFLINE_PROCESSING)
		for (i = 0; i < num_ext_planes; i++) {
			frame->shot->uctl.offlineDtDesc.base_kvaddr =
				iq->buf_kva[index][pos_ext_p + i];
			frame->shot->uctl.offlineDtDesc.base_dvaddr =
				iq->buf_dva[index][pos_ext_p + i];
		}
#else
		for (i = 0; i < num_ext_planes; i++)
			frame->kvaddr_ext[i] = iq->buf_kva[index][pos_ext_p + i];
#endif
#ifdef MEASURE_TIME
		frame->tzone = (struct timespec64 *)frame->shot_ext->timeZone;
#endif
		if (IS_ENABLED(LOGICAL_VIDEO_NODE)
				&& (iq->mode == CAMERA_NODE_LOGICAL)) {
			ret = _is_queue_subbuf_queue(ivc, iq, frame);
			if (ret) {
				mverr("[%s][I%d]Failed to subbuf_queue",
						ivc, iv, iq->name, index);
				goto err_logical_node;
			}
		}
	}

	/* uninitialized frame need to get info */
	if (!test_bit(FRAME_MEM_INIT, &frame->mem_state))
		goto set_info;

	/* plane address check */
	for (i = 0; i < frame->planes; i++) {
		if (frame->dvaddr_buffer[i] != iq->buf_dva[index][i]) {
			if (iv->resourcemgr->hal_version == IS_HAL_VER_3_2) {
				frame->dvaddr_buffer[i] = iq->buf_dva[index][i];
			} else {
				mverr("buffer[%d][%d] is changed(%pad != %pad)",
						ivc, iv, index, i,
						&frame->dvaddr_buffer[i],
						&iq->buf_dva[index][i]);
				ret = -EINVAL;
				goto err_dva_changed;
			}
		}

		if (frame->kvaddr_buffer[i] != iq->buf_kva[index][i]) {
			if (iv->resourcemgr->hal_version == IS_HAL_VER_3_2) {
				frame->kvaddr_buffer[i] = iq->buf_kva[index][i];
			} else {
				mverr("kvaddr buffer[%d][%d] is changed(0x%08lx != 0x%08lx)",
						ivc, iv, index, i,
						frame->kvaddr_buffer[i], iq->buf_kva[index][i]);
				ret = -EINVAL;
				goto err_kva_changed;
			}
		}
	}

	return 0;

set_info:
	if (test_bit(IS_QUEUE_BUFFER_PREPARED, &iq->state)) {
		mverr("already prepared but new index(%d) is came", ivc, iv, index);
		ret = -EINVAL;
		goto err_queue_prepared_already;
	}

	for (i = 0; i < frame->planes; i++) {
		frame->dvaddr_buffer[i] = iq->buf_dva[index][i];
		frame->kvaddr_buffer[i] = iq->buf_kva[index][i];
		frame->size[i] = iq->framecfg.size[i];
#ifdef PRINT_BUFADDR
		mvinfo("%s %d.%d %pad\n", ivc, iv, framemgr->name, index,
				i, &frame->dvaddr_buffer[i]);
#endif
	}

	set_bit(FRAME_MEM_INIT, &frame->mem_state);

	iq->buf_refcount++;

	if (iq->buf_rdycount == iq->buf_refcount)
		set_bit(IS_QUEUE_BUFFER_READY, &iq->state);

	if (iq->buf_maxcount == iq->buf_refcount) {
		if (IS_ENABLED(DMABUF_CONTAINER) && vbuf->num_merged_dbufs)
			mvinfo("%s number of merged buffers: %d\n",
					ivc, iv, iq->name, frame->num_buffers);
		set_bit(IS_QUEUE_BUFFER_PREPARED, &iq->state);
	}

	iq->buf_que++;

err_logical_node:
err_queue_prepared_already:
err_kva_changed:
err_dva_changed:
err_get_kva_for_ext:
err_get_dva_for_ext:
	return ret;
}

void is_buf_queue(struct vb2_buffer *vb)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_queue *iq = &ivc->queue;
	struct is_group *ig = ivc->group;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_groupmgr *grpmgr;
	u32 instance = get_instance_video_ctx(ivc);
	int ret;

	mvdbgs(3, "%s(%d)\n", ivc, iq, __func__, vb->index);

	ret = __is_buf_queue(iq, vb);
	if (ret) {
		mierr("failure in _is_buf_queue(): %d", instance, ret);
		return;
	}

	/* for each leader */
	if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			idi = (struct is_device_ischain *)ivc->device;
			if (!test_bit(IS_ISCHAIN_OPEN, &idi->state)) {
				merr("trying buf_queue to not opened device", idi);
				return;
			}

			grpmgr = idi->groupmgr;

			mdbgs_ischain(4, "%s\n", idi, __func__);
		} else {
			ids = (struct is_device_sensor *)ivc->device;
			if (!test_bit(IS_SENSOR_OPEN, &ids->state)) {
				merr("trying buf_queue to not opened device", ids);
				return;
			}

			grpmgr = ids->groupmgr;

			mdbgs_sensor(4, "%s\n", ids, __func__);
		}

		ret = is_group_buffer_queue(grpmgr, ig, iq, vb->index);
		if (ret)
			mierr("failure in is_group_buffer_queue(): %d", instance, ret);
	/* for each non-leader */
	} else {
		ret = is_subdev_buffer_queue(ivc->subdev, vb);
		if (ret)
			mierr("failure in is_subdev_buffer_queue(): %d", instance, ret);
	}
}

const struct vb2_ops is_default_vb2_ops = {
	.queue_setup		= is_queue_setup,
	.wait_prepare		= is_wait_prepare,
	.wait_finish		= is_wait_finish,
	.buf_init		= is_buf_init,
	.buf_prepare		= is_buf_prepare,
	.buf_finish		= is_buf_finish,
	.buf_cleanup		= is_buf_cleanup,
	.start_streaming	= is_start_streaming,
	.stop_streaming		= is_stop_streaming,
	.buf_queue		= is_buf_queue,
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Pablo v42l file operaions                                       *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static enum is_device_type device_type_vid(int vid)
{
	if ((vid >= IS_VIDEO_SS0_NUM && vid <= IS_VIDEO_SS5_NUM)
	    || (vid >= IS_VIDEO_SS0VC0_NUM && vid < IS_VIDEO_SS5VC3_NUM))
		return IS_DEVICE_SENSOR;
	else
		return IS_DEVICE_ISCHAIN;
}

static unsigned int rsctype_vid(int vid)
{
	if (vid >= IS_VIDEO_SS0_NUM && vid <= IS_VIDEO_SS5_NUM)
		return vid - IS_VIDEO_SS0_NUM;
	else if (vid >= IS_VIDEO_SS0VC0_NUM && vid < IS_VIDEO_SS5VC3_NUM)
		/* FIXME: it depends on the nubmer of VC channels: 4 */
		return (vid - IS_VIDEO_SS0VC0_NUM) >> 2;
	else
		return RESOURCE_TYPE_ISCHAIN;
}

static struct is_video_ctx *is_vctx_open(struct file *file,
	struct is_video *video,
	u32 instance,
	ulong id,
	const char *name)
{
	struct is_video_ctx *ivc;

	if (atomic_read(&video->refcount) > IS_STREAM_COUNT) {
		err("[V%02d] can't open vctx, refcount is invalid", video->id);
		return ERR_PTR(-EINVAL);
	}

	ivc = vzalloc(sizeof(struct is_video_ctx));
	if (!ivc) {
		err("[V%02d] vzalloc is fail", video->id);
		return ERR_PTR(-ENOMEM);
	}

	ivc->refcount = vref_get(video);
	ivc->instance = instance;
	ivc->queue.id = id;
	snprintf(ivc->queue.name, sizeof(ivc->queue.name), "%s", name);
	ivc->state = BIT(IS_VIDEO_CLOSE);

	file->private_data = ivc;

	return ivc;
}

static int is_vctx_close(struct file *file,
	struct is_video *video,
	struct is_video_ctx *vctx)
{
	vfree(vctx);
	file->private_data = NULL;

	return vref_put(video, NULL);
}

static int __is_video_open(struct is_video_ctx *ivc,
	struct is_video *iv, void *device)
{
	struct is_queue *iq = &ivc->queue;
	int ret;

	if (!(ivc->state & BIT(IS_VIDEO_CLOSE))) {
		mverr("already open(%lX)", ivc, iv, ivc->state);
		return -EEXIST;
	}

	if (atomic_read(&iv->refcount) == 1) {
		sema_init(&iv->smp_multi_input, 1);
		iv->try_smp = false;
	}

	ivc->device		= device;
	ivc->video		= iv;
	ivc->vops.qbuf		= is_video_qbuf;
	ivc->vops.dqbuf		= is_video_dqbuf;
	ivc->vops.done		= is_video_buffer_done;

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		if (iv->video_type == IS_VIDEO_TYPE_LEADER)
			iq->qops = is_get_ischain_device_qops();
		else
			iq->qops = is_get_ischain_subdev_qops();
	} else {
		if (iv->video_type == IS_VIDEO_TYPE_LEADER)
			iq->qops = is_get_sensor_device_qops();
		else
			iq->qops = is_get_sensor_subdev_qops();
	}

	ret = is_queue_open(iq, iv->buf_rdycount);
	if (ret) {
		mverr("failure in is_queue_open(): %d", ivc, iv, ret);
		return ret;
	}

	iq->vbq = vzalloc(sizeof(struct vb2_queue));
	if (!iq->vbq) {
		mverr("out of memory for vbq", ivc, iv);
		return -ENOMEM;
	}

	ret = queue_init(ivc, iq->vbq, NULL);
	if (ret) {
		mverr("failure in queue_init(): %d", ivc, iv, ret);
		vfree(iq->vbq);
		iq->vbq = NULL;
		return ret;
	}

	ivc->state = BIT(IS_VIDEO_OPEN);

	return 0;
}

static int __is_video_close(struct is_video_ctx *ivc)
{
	struct is_video *iv = ivc->video;
	struct is_queue *iq = &ivc->queue;

	if (ivc->state < BIT(IS_VIDEO_OPEN)) {
		mverr("already close(%lX)", ivc, iv, ivc->state);
		return -ENOENT;
	}

	vb2_queue_release(iq->vbq);
	vfree(iq->vbq);
	iq->vbq = NULL;
	is_queue_close(iq);

	/*
	 * vb2 release can call stop callback
	 * not if video node is not stream off
	 */
	ivc->device = NULL;
	ivc->state = BIT(IS_VIDEO_CLOSE);

	return 0;
}

int is_video_open(struct file *file)
{
	struct is_video *iv = video_drvdata(file);
	struct is_resourcemgr *rscmgr = iv->resourcemgr;
	void *device;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_video_ctx *ivc;
	u32 instance;
	int ret;

	ret = is_resource_open(rscmgr, rsctype_vid(iv->id), &device);
	if (ret) {
		err("failure in is_resource_open(): %d", ret);
		goto err_resource_open;
	}

	if (!device) {
		err("failed to get device");
		ret = -EINVAL;
		goto err_invalid_device;
	}

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		idi = (struct is_device_ischain *)device;
		instance = idi->instance;

		minfo("[%s]%s\n", idi, vn_name[iv->id], __func__);
	} else {
		ids = (struct is_device_sensor *)device;
		instance = ids->instance;

		if (ids->reboot && iv->video_type == IS_VIDEO_TYPE_LEADER) {
			warn("%s%s: failed to open sensor due to reboot",
					vn_name[iv->id], __func__);
			ret = -EINVAL;
			goto err_reboot;
		}

		minfo("[%s]%s\n", ids, vn_name[iv->id], __func__);
	}

	ivc = is_vctx_open(file, iv, instance, iv->subdev_id, vn_name[iv->id]);
	if (IS_ERR_OR_NULL(ivc)) {
		ret = PTR_ERR(ivc);
		err("failure in open_vctx(): %d", instance, ret);
		goto err_vctx_open;
	}

	ret = __is_video_open(ivc, iv, device);
	if (ret) {
		mierr("failure in __is_video_open(): %d", instance, ret);
		goto err_video_open;
	}

	if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
		ivc->group = (struct is_group *)((char *)device + iv->group_ofs);

		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			ret = is_ischain_group_open(idi, ivc, iv->group_id);
			if (ret) {
				mierr("failure in is_ischain_group_open(): %d",
								instance, ret);
				goto err_ischain_group_open;
			}
		} else {
			ret = is_sensor_open(ids, ivc);
			if (ret) {
				mierr("failure in is_sensor_open(): %d",
							instance, ret);
				goto err_sensor_open;
			}
		}
	} else {
		ivc->subdev = (struct is_subdev *)((char *)device + iv->subdev_ofs);

		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			ret = is_ischain_subdev_open(idi, ivc);
			if (ret) {
				mierr("failure in is_ischain_subdev_open(): %d",
								instance, ret);
				goto err_ischain_subdev_open;
			}
		} else {
			ret = is_sensor_subdev_open(ids, ivc);
			if (ret) {
				mierr("failure in is_sensor_subdev_open(): %d",
								instance, ret);
				goto err_sensor_subdev_open;
			}
		}
	}

	return 0;

err_sensor_subdev_open:
err_ischain_subdev_open:
err_sensor_open:
err_ischain_group_open:
	__is_video_close(ivc);

err_video_open:
	is_vctx_close(file, iv, ivc);

err_vctx_open:
err_reboot:
err_invalid_device:
err_resource_open:
	return ret;
}

int is_video_close(struct file *file)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	u32 instance;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	int refcount;
	int ret;

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		idi = (struct is_device_ischain *)ivc->device;
		instance = idi->instance;
	} else {
		ids = (struct is_device_sensor *)ivc->device;
		instance = ids->instance;
	}

	if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			ret = is_ischain_group_close(idi, ivc, ivc->group);
			if (ret)
				mierr("failure in is_ischain_group_close(): %d",
								instance, ret);
		} else {
			ret = is_sensor_close(ids);
			if (ret)
				mierr("failure in is_sensor_close(): %d",
								instance, ret);
		}
	} else {
		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			ret = is_ischain_subdev_close(idi, ivc);
			if (ret)
				mierr("failure in is_ischain_subdev_close(): %d",
								instance, ret);
		} else {
			ret = is_sensor_subdev_close(ids, ivc);
			if (ret)
				mierr("failure in is_sensor_subdev_close(): %d",
								instance, ret);
		}
	}

	ret = __is_video_close(ivc);
	if (ret)
		mierr("failure in is_video_close(): %d", instance, ret);

	refcount = is_vctx_close(file, iv, ivc);
	if (refcount < 0)
		mierr("failure in is_vctx_close(): %d", instance, refcount);

	if (iv->device_type == IS_DEVICE_ISCHAIN)
		minfo("[%s]%s(open count: %d, ref. cont: %d): %d\n", idi, vn_name[iv->id],
				__func__, atomic_read(&idi->open_cnt), refcount, ret);
	 else
		minfo("[%s]%s(ref. cont: %d): %d\n", ids, vn_name[iv->id], __func__,
									refcount, ret);

	return 0;
}

__poll_t is_video_poll(struct file *file, struct poll_table_struct *wait)
{
	struct is_video_ctx *vctx = file->private_data;
	struct is_queue *queue = GET_QUEUE(vctx);

	return vb2_poll(queue->vbq, file, wait);
}

int is_video_mmap(struct file *file,
	struct vm_area_struct *vma)
{
	struct is_video_ctx *vctx = file->private_data;
	struct is_queue *queue = GET_QUEUE(vctx);

	return vb2_mmap(queue->vbq, vma);
}

static struct v4l2_file_operations is_default_v4l2_file_ops = {
	.owner		= THIS_MODULE,
	.open		= is_video_open,
	.release	= is_video_close,
	.poll		= is_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= is_video_mmap,
};

static struct v4l2_ioctl_ops is_default_v4l2_ioctl_ops;

int is_video_probe(struct is_video *video,
	char *video_name,
	u32 video_number,
	u32 vfl_dir,
	struct is_mem *mem,
	struct v4l2_device *v4l2_dev,
	const struct v4l2_file_operations *file_ops,
	const struct v4l2_ioctl_ops *ioctl_ops,
	const struct vb2_ops *vb2_ops)
{
	int ret = 0;
	u32 video_id;

	vref_init(video);
	mutex_init(&video->lock);
	snprintf(video->vd.name, sizeof(video->vd.name), "%s", video_name);
	video->id		= video_number;
	video->vb2_ops		= vb2_ops ? vb2_ops : &is_default_vb2_ops;
	video->vb2_mem_ops	= mem->vb2_mem_ops;
	video->is_vb2_buf_ops	= mem->is_vb2_buf_ops;
	video->alloc_ctx	= mem->priv;
	video->alloc_dev	= mem->dev;
	video->video_type	= (vfl_dir == VFL_DIR_RX) ?
					IS_VIDEO_TYPE_CAPTURE : IS_VIDEO_TYPE_LEADER;
	video->device_type	= device_type_vid(video_number);

	video->vd.vfl_dir	= vfl_dir;
	video->vd.v4l2_dev	= v4l2_dev;
	video->vd.fops		= file_ops ? file_ops : &is_default_v4l2_file_ops;
	video->vd.ioctl_ops	= ioctl_ops ? ioctl_ops : &is_default_v4l2_ioctl_ops;
	video->vd.minor		= -1;
	video->vd.release	= video_device_release;
	video->vd.lock		= &video->lock;
	video->vd.device_caps	= (vfl_dir == VFL_DIR_RX) ? VIDEO_CAPTURE_DEVICE_CAPS : VIDEO_OUTPUT_DEVICE_CAPS;
	video_set_drvdata(&video->vd, video);

	video_id = EXYNOS_VIDEONODE_FIMC_IS + video_number;
	ret = video_register_device(&video->vd, VFL_TYPE_PABLO, video_id);
	if (ret) {
		err("[V%02d] Failed to register video device", video->id);
		goto p_err;
	}

p_err:
	info("[VID] %s(%d) is created. minor(%d)\n", video_name, video_id, video->vd.minor);
	return ret;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Pablo v42l ioctl operaions                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int is_vidioc_querycap(struct file *file, void *fh,
			struct v4l2_capability *cap)
{
	struct is_video *iv = video_drvdata(file);

	snprintf(cap->driver, sizeof(cap->driver), "%s", iv->vd.name);
	snprintf(cap->card, sizeof(cap->card), "%s", iv->vd.name);

	if (iv->video_type == IS_VIDEO_TYPE_LEADER)
		cap->capabilities |= V4L2_CAP_STREAMING
			| V4L2_CAP_VIDEO_OUTPUT
			| V4L2_CAP_VIDEO_OUTPUT_MPLANE;
	else
		cap->capabilities |= V4L2_CAP_STREAMING
			| V4L2_CAP_VIDEO_CAPTURE
			| V4L2_CAP_VIDEO_CAPTURE_MPLANE;

	cap->device_caps |= cap->capabilities;

	return 0;
}

int is_vidioc_g_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	return 0;
}

int is_vidioc_s_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = &ivc->queue;
	int ret;
	u32 condition;

	mdbgv_video(ivc, iv, __func__);

	/* capture video node can skip s_input */
	if (iv->video_type == IS_VIDEO_TYPE_LEADER)
		condition = BIT(IS_VIDEO_S_INPUT)
			  | BIT(IS_VIDEO_S_FORMAT)
			  | BIT(IS_VIDEO_S_BUFS);
	else
		condition = BIT(IS_VIDEO_S_INPUT)
			  | BIT(IS_VIDEO_S_BUFS)
			  | BIT(IS_VIDEO_S_FORMAT)
			  | BIT(IS_VIDEO_OPEN)
			  | BIT(IS_VIDEO_STOP);

	if (!(ivc->state & condition)) {
		mverr("invalid state(%lX)", ivc, iv, ivc->state);
		return -EINVAL;
	}

	ret = is_queue_set_format_mplane(ivc, iq, ivc->device, f);
	if (ret) {
		mverr("failure in is_queue_set_format_mplane(): %d", ivc, iv, ret);
		return ret;
	}

	ivc->state = BIT(IS_VIDEO_S_FORMAT);

	mdbgv_vid("set_format(%d x %d)\n", iq->framecfg.width, iq->framecfg.height);

	return 0;
}

int is_vidioc_reqbufs(struct file *file, void* fh,
		struct v4l2_requestbuffers *b)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_subdev *leader;
	struct is_queue *iq = &ivc->queue;
	struct is_framemgr *framemgr = &iq->framemgr;
	int ret;

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		idi = (struct is_device_ischain *)ivc->device;

		mdbgv_ischain("%s(buffer count: %d\n", ivc, __func__, b->count);
	} else {
		ids = (struct is_device_sensor *)ivc->device;

		mdbgv_sensor("%s(buffer count: %d)\n", ivc, __func__, b->count);
	}

	if (iv->video_type != IS_VIDEO_TYPE_LEADER) {
		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			leader = ivc->subdev->leader;
			if (leader && test_bit(IS_SUBDEV_START, &leader->state)) {
				merr("leader%d still running, subdev%d req is not applied",
							idi, leader->id, ivc->subdev->id);
				return -EBUSY;
			}
		} else {
			if (test_bit(IS_SENSOR_BACK_START, &ids->state)) {
				err("sensor%d still running, vid%d req is not applied",
								ids->device_id, iv->id);
				return -EBUSY;
			}
		}
	}

	if (!(ivc->state
	    & (BIT(IS_VIDEO_S_FORMAT) | BIT(IS_VIDEO_STOP) | BIT(IS_VIDEO_S_BUFS)))) {
		mverr("invalid state(%lX)", ivc, iv, ivc->state);
		return -EINVAL;
	}

	if (test_bit(IS_QUEUE_STREAM_ON, &iq->state)) {
		mverr("already streaming", ivc, iv);
		return -EINVAL;
	}

	/* before call queue ops if request count is zero */
	if (!b->count) {
		ret = CALL_QOPS(iq, reqbufs, ivc->device, iq, b->count);
		if (ret) {
			mverr("failure in reqbufs QOP", ivc, iv, ret);
			return ret;
		}
	}

	ret = vb2_reqbufs(iq->vbq, b);
	if (ret) {
		mverr("failure in vb2_reqbufs(): %d", ivc, iv, ret);
		return ret;
	}

	iq->buf_maxcount = b->count;

	if (iq->buf_maxcount == 0) {
		iq->buf_req = 0;
		iq->buf_pre = 0;
		iq->buf_que = 0;
		iq->buf_com = 0;
		iq->buf_dqe = 0;
		iq->buf_refcount = 0;

		clear_bit(IS_QUEUE_BUFFER_READY, &iq->state);
		clear_bit(IS_QUEUE_BUFFER_PREPARED, &iq->state);

		frame_manager_close(framemgr);
	} else {
		if (iq->buf_maxcount < iq->buf_rdycount) {
			mverr("buffer count is not invalid(%d < %d)", ivc, iv,
					iq->buf_maxcount, iq->buf_rdycount);
			return -EINVAL;
		}

		ret = frame_manager_open(framemgr, iq->buf_maxcount);
		if (ret) {
			mverr("failure in frame_manager_open(): %d", ivc, iv, ret);
			return ret;
		}
	}

	/* after call queue ops if request count is not zero */
	if (b->count) {
		ret = CALL_QOPS(iq, reqbufs, ivc->device, iq, b->count);
		if (ret) {
			mverr("failure in vb2_reqbufs(): %d", ivc, iv, ret);
			return ret;
		}
	}

	ivc->state = BIT(IS_VIDEO_S_BUFS);

	return 0;
}

int is_vidioc_querybuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = &ivc->queue;
	int ret;

	mdbgv_video(ivc, iv, __func__);

	ret = vb2_querybuf(iq->vbq, b);
	if (ret) {
		mverr("failure in vb2_querybuf(): %d", ivc, iv, ret);
		return ret;
	}

	return 0;
}

int is_vidioc_qbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	u32 instance = get_instance_video_ctx(ivc);
	int ret;

	mvdbgs(3, "%s(%02d:%d)\n", ivc, &ivc->queue, __func__, b->type, b->index);

	/* FIXME: clh, isp, lme, mcs, vra */
	/*
	struct is_queue *iq = &ivc->queue;

	if (!test_bit(IS_QUEUE_STREAM_ON, &iq->state)) {
		mierr("stream off state, can NOT qbuf", instance);
		return = -EINVAL;
	}
	*/

	ret = CALL_VOPS(ivc, qbuf, b);
	if (ret) {
		mierr("failure in qbuf video op: %d", instance, ret);
		return ret;
	}

	return 0;
}

int is_vidioc_dqbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	u32 instance = get_instance_video_ctx(ivc);
	bool nonblocking = file->f_flags & O_NONBLOCK;
	int ret;

	mvdbgs(3, "%s(%02d:%d)\n", ivc, &ivc->queue, __func__, b->type, b->index);

	ret = CALL_VOPS(ivc, dqbuf, b, nonblocking);
	if (ret) {
		if (ret != -EAGAIN)
			mierr("failure in dqbuf video op: %d", instance, ret);
		return ret;
	}

	return 0;
}

int is_vidioc_prepare_buf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = &ivc->queue;
	struct vb2_queue *vbq = iq->vbq;
	struct vb2_buffer *vb;
	int ret;

	if (!vbq) {
		mverr("vbq is NULL", ivc, iv);
		return -EINVAL;
	}

	ret = is_vb2_prepare_buf(vbq, b);
	if (ret) {
		mverr("failure in vb2_prepare_buf(index: %d): %d", ivc, iv, b->index, ret);
		return ret;
	}

	vb = vbq->bufs[b->index];
        if (!vb) {
		mverr("vb is NULL", ivc, iv);
                return -EINVAL;
        }

	ret = __is_buf_queue(iq, vb);
	if (ret) {
		mverr("failure in __is_buf_queue(index: %d): %d", ivc, iv, b->index, ret);
		return ret;
	}

	info("[%s]%s(index: %d)\n", vn_name[iv->id], __func__, b->index);

	return 0;
}

int is_vidioc_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = &ivc->queue;
	struct vb2_queue *vbq = iq->vbq;
	int ret;

	mdbgv_video(ivc, iv, __func__);

	if (!(ivc->state & (BIT(IS_VIDEO_S_BUFS) | BIT(IS_VIDEO_STOP)))) {
		mverr("invalid state(%lX)", ivc, iv, ivc->state);
		return -EINVAL;
	}

	if (!vbq) {
		mverr("vbq is NULL", ivc, iv);
		return -EINVAL;
	}

	ret = vb2_streamon(vbq, i);
	if (ret) {
		mverr("failure in vb2_streamon(): %d", ivc, iv, ret);
		return ret;
	}

	ivc->state = BIT(IS_VIDEO_START);

	return 0;
}

int is_vidioc_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = &ivc->queue;
	struct vb2_queue *vbq = iq->vbq;
	struct is_framemgr *framemgr = &iq->framemgr;
	u32 rcnt, pcnt, ccnt;
	int ret;

	mdbgv_video(ivc, iv, __func__);

	if (!(ivc->state & BIT(IS_VIDEO_START))) {
		mverr("invalid state(%lX)", ivc, iv, ivc->state);
		return -EINVAL;
	}

	framemgr_e_barrier_irq(framemgr, FMGR_IDX_0);
	rcnt = framemgr->queued_count[FS_REQUEST];
	pcnt = framemgr->queued_count[FS_PROCESS];
	ccnt = framemgr->queued_count[FS_COMPLETE];
	framemgr_x_barrier_irq(framemgr, FMGR_IDX_0);

	if (rcnt + pcnt + ccnt > 0)
		mvwarn("streamoff: queued buffer is not empty(R%d, P%d, C%d)",
						ivc, iv, rcnt, pcnt, ccnt);

	if (!vbq) {
		mverr("vbq is NULL", ivc, iv);
		return -EINVAL;
	}

	ret = vb2_streamoff(vbq, i);
	if (ret) {
		mverr("failure in vb2_streamoff(): %d", ivc, iv, ret);
		return ret;
	}

	ret = frame_manager_flush(framemgr);
	if (ret) {
		mverr("failure in frame_manager_flush(): %d", ivc, iv, ret);
		return ret;
	}

	ivc->state = BIT(IS_VIDEO_STOP);

	return 0;
}

int is_vidioc_s_input(struct file *file, void *fs, unsigned int i)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	u32 instance = get_instance_video_ctx(ivc);
	u32 scenario, stream, position, vindex, intype, leader;
	int ret;

	/* for each leader */
	if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
		scenario = (i & SENSOR_SCN_MASK) >> SENSOR_SCN_SHIFT;
		stream = (i & INPUT_STREAM_MASK) >> INPUT_STREAM_SHIFT;
		position = (i & INPUT_POSITION_MASK) >> INPUT_POSITION_SHIFT;
		vindex = (i & INPUT_VINDEX_MASK) >> INPUT_VINDEX_SHIFT;
		intype = (i & INPUT_INTYPE_MASK) >> INPUT_INTYPE_SHIFT;
		leader = (i & INPUT_LEADER_MASK) >> INPUT_LEADER_SHIFT;

		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			mdbgv_ischain("%s(input: %08X) - stream: %d, position: %d, vindex: %d, "
					"intype: %d, leader: %d\n", ivc, __func__,
					i, stream, position, vindex, intype, leader);

			ret = is_ischain_group_s_input(ivc->device, ivc->group, stream,
							position, vindex, intype, leader);
			if (ret) {
				mierr("failure in is_ischain_group_s_input(): %d", instance, ret);
				return ret;
			}
		} else {
#ifdef CONFIG_USE_SENSOR_GROUP
			mdbgv_sensor("%s(input: %08X) - scenario: %d, stream: %d, position: %d, "
					"vindex: %d, intype: %d, leader: %d\n", ivc, __func__,
					scenario, i, stream, position, vindex, intype, leader);
			ret = is_sensor_s_input(ivc->device, position, scenario, vindex, leader);
			if (ret) {
				mierr("failure in is_sensor_s_input(): %d", instance, ret);
				return ret;
			}
#else
			scenario = (i & SENSOR_SCENARIO_MASK) >> SENSOR_SCENARIO_SHIFT;
			int input = (i & SENSOR_MODULE_MASK) >> SENSOR_MODULE_SHIFT;

			mdbgv_sensor("%s(input: %08X)\n", __func__, i);

			ret = is_sensor_s_input(device, input, scenario);
			if (ret) {
				mierr("failure in is_sensor_s_input(): %d", instance, ret);
				return ret;
			}
#endif
		}
	}

	if (!(ivc->state
	    & (BIT(IS_VIDEO_OPEN) | BIT(IS_VIDEO_S_INPUT) | BIT(IS_VIDEO_S_BUFS)))) {
		mverr("invalid state(%lX)", ivc, iv, ivc->state);
		return -EINVAL;
	}

	ivc->state = BIT(IS_VIDEO_S_INPUT);

	return 0;
}

static int is_g_ctrl_completes(struct file *file)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = &ivc->queue;
	struct is_framemgr *framemgr = &iq->framemgr;

	return framemgr->queued_count[FS_COMPLETE];
}

int is_vidioc_g_ctrl(struct file *file, void * fh, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	u32 instance = get_instance_video_ctx(ivc);

	mdbgv_video(ivc, iv, __func__);

	switch (a->id) {
	case V4L2_CID_IS_G_COMPLETES:
		if (iv->video_type != IS_VIDEO_TYPE_LEADER) {
			a->value = is_g_ctrl_completes(file);
		} else {
			mierr("unsupported ioctl(%lx, %d)", instance,
						a->id, a->id & 0xff);
			return -EINVAL;
		}
		break;
	default:
		mierr("unsupported ioctl(%lx, %d)", instance, a->id,
							a->id & 0xff);
		return -EINVAL;
	}

	return 0;
}

static int is_g_ctrl_capture_meta(struct file *file, struct v4l2_ext_control *ext_ctrl)
{
#if IS_ENABLED(USE_DDK_INTF_CAP_META)
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_group *group = ivc->group;
	struct is_device_ischain *device = GET_DEVICE(ivc);
	struct is_cap_meta_info cap_meta_info;
	struct dma_buf *dma_buf = NULL;
	u32 size, fcount;
	ulong meta_kva = 0, cap_shot_kva;
	int dma_buf_fd, ret;

	mdbgv_video(ivc, iv, __func__);

	ret = copy_from_user(&cap_meta_info, ext_ctrl->ptr, sizeof(struct is_cap_meta_info));
	if (ret) {
		err("fail to copy_from_user, ret(%d)\n", ret);
		goto p_err;
	}

	fcount = cap_meta_info.frame_count;
	dma_buf_fd = cap_meta_info.dma_buf_fd;
	size = (sizeof(u32) * CAMERA2_MAX_IPC_VENDER_LENGTH);

	dma_buf = dma_buf_get(dma_buf_fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		err("Failed to get dmabuf. fd %d ret %ld",
			dma_buf_fd, PTR_ERR(dma_buf));
		ret = -EINVAL;
		goto err_get_dbuf;
	}

	meta_kva = (ulong)dma_buf_vmap(dma_buf);
	if (IS_ERR_OR_NULL((void *)meta_kva)) {
		err("Failed to get kva %ld", meta_kva);
		ret = -ENOMEM;
		goto err_vmap;
	}

	/* To start the end of camera2_shot_ext */
	cap_shot_kva = meta_kva + sizeof(struct camera2_shot_ext);

	mdbgv_ischain("%s: request capture meta update. fcount(%d), size(%d), addr(%llx)\n",
		ivc, __func__, fcount, size, cap_shot_kva);

	ret = is_ischain_g_ddk_capture_meta_update(group, device, fcount, size, (ulong)cap_shot_kva);
	if (ret) {
		err("fail to ischain_g_ddk_capture_meta_upadte, ret(%d)\n", ret);
		goto p_err;
	}

p_err:
	if (!IS_ERR_OR_NULL((void *) dma_buf)) {
		dma_buf_vunmap(dma_buf, (void *)meta_kva);
		meta_kva = 0;
	}

err_vmap:
	if (!IS_ERR_OR_NULL(dma_buf))
		dma_buf_put(dma_buf);

err_get_dbuf:
#endif
	return 0;
}

int is_vidioc_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *device;
	struct is_framemgr *framemgr;
	struct is_queue *queue;
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	int i, instance, ret;

	mdbgv_video(ivc, iv, __func__);

	device = GET_DEVICE(ivc);
	queue = GET_QUEUE(ivc);
	framemgr = &queue->framemgr;
	instance = get_instance_video_ctx(ivc);

	if (a->which != V4L2_CTRL_CLASS_CAMERA) {
		mierr("invalid control class(%d)", instance, a->which);
		return -EINVAL;
	}

	for (i = 0; i < a->count; i++) {
		ext_ctrl = (a->controls + i);

		switch (ext_ctrl->id) {
		case V4L2_CID_IS_G_CAP_META_UPDATE:
			is_g_ctrl_capture_meta(file, ext_ctrl);
			break;
		default:
			ctrl.id = ext_ctrl->id;
			ctrl.value = ext_ctrl->value;

			ret = is_vidioc_g_ctrl(file, fh, &ctrl);
			if (ret) {
				merr("is_vidioc_g_ctrl is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		}
	}

p_err:
	return 0;
}

static void is_s_ctrl_flip(struct file *file, struct v4l2_control *a)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = &ivc->queue;
	unsigned int flipnr;

	if (a->id == V4L2_CID_HFLIP)
		flipnr = SCALER_FLIP_COMMAND_X_MIRROR;
	else
		flipnr = SCALER_FLIP_COMMAND_Y_MIRROR;

	if (a->value)
		set_bit(flipnr, &iq->framecfg.flip);
	else
		clear_bit(flipnr, &iq->framecfg.flip);
}

static int is_s_ctrl_dvfs_cluster(struct file *file, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_resourcemgr *rscmgr;

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		idi = (struct is_device_ischain *)ivc->device;
		rscmgr = idi->resourcemgr;
	} else {
		ids = (struct is_device_sensor *)ivc->device;
		rscmgr = ids->resourcemgr;
	}

	return is_resource_ioctl(rscmgr, a);
}

static int is_s_ctrl_setfile(struct file *file, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi = GET_DEVICE_ISCHAIN(ivc);
	u32 scenario;

	if (test_bit(IS_ISCHAIN_START, &idi->state)) {
		mverr("already start, failed to apply setfile", ivc, iv);
		return -EINVAL;
	}

	idi->setfile = a->value;
	scenario = (idi->setfile & IS_SCENARIO_MASK) >> IS_SCENARIO_SHIFT;
	mvinfo("[S_CTRL] setfile(%d), scenario(%d)\n", idi, iv,
			idi->setfile & IS_SETFILE_MASK, scenario);

	return 0;
}

static int is_s_ctrl_camera_type(struct file *file, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi = (struct is_device_ischain *)ivc->device;

	if (a->value == IS_COLD_BOOT) {
		/* change value to X when !TWIZ | front */
		is_itf_fwboot_init(idi->interface);
	} else if (a->value == IS_WARM_BOOT) {
		/* change value to X when TWIZ & back | frist time back camera */
		if (!test_bit(IS_IF_LAUNCH_FIRST, &idi->interface->launch_state))
			idi->interface->fw_boot_mode = FIRST_LAUNCHING;
		else
			idi->interface->fw_boot_mode = WARM_BOOT;
	} else {
		mverr("invalid camera type", ivc, iv, a->id);
		return -EINVAL;
	}

	return 0;
}

static void is_s_ctrl_dvfs_scenario(struct file *file, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi = (struct is_device_ischain *)ivc->device;
	struct is_dvfs_ctrl *dvfs_ctrl = &idi->resourcemgr->dvfs_ctrl;
	u32 vendor;

	dvfs_ctrl->dvfs_scenario = a->value;
	vendor = (dvfs_ctrl->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT)
						& IS_DVFS_SCENARIO_VENDOR_MASK;
	mvinfo("[S_CTRL][DVFS] value(%x), common(%d), vendor(%d)\n", ivc, iv,
			dvfs_ctrl->dvfs_scenario,
			dvfs_ctrl->dvfs_scenario & IS_DVFS_SCENARIO_COMMON_MODE_MASK,
			vendor);
}

static void is_s_ctrl_dvfs_recording_size(struct file *file, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi = (struct is_device_ischain *)ivc->device;
	struct is_dvfs_ctrl *dvfs_ctrl = &idi->resourcemgr->dvfs_ctrl;

	dvfs_ctrl->dvfs_rec_size = a->value;

	mvinfo("[S_CTRL][DVFS] rec_width(%d), rec_height(%d)\n", ivc, iv,
			dvfs_ctrl->dvfs_rec_size & 0xffff,
			dvfs_ctrl->dvfs_rec_size >> IS_DVFS_SCENARIO_HEIGHT_SHIFT);
}

static int is_s_ctrl_hal_version(struct file *file, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_resourcemgr *rscmgr;

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		idi = (struct is_device_ischain *)ivc->device;
		rscmgr = idi->resourcemgr;
	} else {
		ids = (struct is_device_sensor *)ivc->device;
		rscmgr = ids->resourcemgr;
	}

	if (a->value < IS_HAL_VER_1_0 || a->value >= IS_HAL_VER_MAX) {
		mverr("hal version(%d) is invalid", ivc, iv, a->value);
		return -EINVAL;
	}

	rscmgr->hal_version = a->value;

	return 0;
}

int is_vidioc_s_ctrl(struct file *file, void *fh, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = &ivc->queue;
	struct is_device_ischain *idi;
	struct is_core *core;
	u32 instance = get_instance_video_ctx(ivc);
	int ret;

	mdbgv_video(ivc, iv, __func__);

	switch (a->id) {
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
		is_s_ctrl_flip(file, a);
		break;
	case V4L2_CID_IS_G_CAPABILITY:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi)
			is_ischain_g_capability(idi, a->value);
		else
			return -EINVAL;
		break;
	case V4L2_CID_IS_DVFS_LOCK:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi) {
			is_pm_qos_add_request(&idi->user_qos,
				PM_QOS_DEVICE_THROUGHPUT, a->value);
			mvinfo("[S_CTRL][DVFS] lock: %d\n",
						ivc, iv, a->value);
		} else {
			return -EINVAL;
		}
		break;
	case V4L2_CID_IS_DVFS_UNLOCK:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi) {
			is_pm_qos_remove_request(&idi->user_qos);
			mvinfo("[S_CTRL][DVFS] unlock: %d\n",
						ivc, iv, a->value);
		} else {
			return -EINVAL;
		}
		break;
	case V4L2_CID_IS_DVFS_CLUSTER0:
	case V4L2_CID_IS_DVFS_CLUSTER1:
	case V4L2_CID_IS_DVFS_CLUSTER2:
		return is_s_ctrl_dvfs_cluster(file, a);
	case V4L2_CID_IS_S_USE_EXT_PLANE:
		if (a->value)
			set_bit(IS_QUEUE_NEED_TO_EXTMAP, &iq->state);
		else
			clear_bit(IS_QUEUE_NEED_TO_EXTMAP, &iq->state);
		mvinfo("[S_CTRL][%s] use extra plane: %d\n", ivc, iv,
						iq->name, a->value);
		break;
	case V4L2_CID_IS_FORCE_DONE:
		if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
			set_bit(IS_GROUP_REQUEST_FSTOP, &ivc->group->state);
		} else {
			mierr("unsupported ioctl(%lx, %d)", instance,
						a->id, a->id & 0xff);
			return -EINVAL;
		}
		break;
	case V4L2_CID_IS_SET_SETFILE:
		return is_s_ctrl_setfile(file, a);
	case V4L2_CID_IS_END_OF_STREAM:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi)
			return is_ischain_open_wrap(idi, true);
		else
			return -EINVAL;
	case V4L2_CID_IS_CAMERA_TYPE:
		if (iv->device_type == IS_DEVICE_ISCHAIN)
			return is_s_ctrl_camera_type(file, a);
		else
			return -EINVAL;
	case V4L2_CID_IS_S_DVFS_SCENARIO:
		if (iv->device_type == IS_DEVICE_ISCHAIN)
			is_s_ctrl_dvfs_scenario(file, a);
		else
			return -EINVAL;
		break;
	case V4L2_CID_IS_S_DVFS_RECORDING_SIZE:
		if (iv->device_type == IS_DEVICE_ISCHAIN)
			is_s_ctrl_dvfs_recording_size(file, a);
		else
			return -EINVAL;
		break;
	case V4L2_CID_IS_OPENING_HINT:
		core = is_get_is_core();
		if (core) {
			mvinfo("opening hint(%d)\n", ivc, iv, a->value);
			core->vender.opening_hint = a->value;
		}
		break;
	case V4L2_CID_IS_CLOSING_HINT:
		core = is_get_is_core();
		if (core) {
			mvinfo("closing hint(%d)\n", ivc, iv, a->value);
			core->vender.closing_hint = a->value;
		}
		break;
	case V4L2_CID_IS_MAX_SIZE:
		core = is_get_is_core();
		if (core) {
			core->vender.isp_max_width = a->value >> 16;
			core->vender.isp_max_height = a->value & 0xffff;
			mvinfo("[S_CTRL] max buffer size: %d x %d (0x%08X)",
					ivc, iv, core->vender.isp_max_width,
					core->vender.isp_max_height,
					a->value);
		}
		break;
	case V4L2_CID_IS_YUV_MAX_SIZE:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi) {
			idi->yuv_max_width = a->value >> 16;
			idi->yuv_max_height = a->value & 0xffff;
			mvinfo("[S_CTRL] max yuv buffer size: %d x %d (0x%08X)",
					ivc, iv, idi->yuv_max_width,
					idi->yuv_max_height,
					a->value);
		}
		break;
	case V4L2_CID_IS_DEBUG_DUMP:
		info("camera resource dump is requested\n");
		is_resource_dump();

		if (a->value)
			panic("and requested panic from user");
		break;
	case V4L2_CID_IS_DEBUG_SYNC_LOG:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi)
			return is_logsync(idi->interface, a->value,
						IS_MSG_TEST_SYNC_LOG);
		else
			return -EINVAL;
	case V4L2_CID_IS_HAL_VERSION:
		return is_s_ctrl_hal_version(file, a);
	case V4L2_CID_IS_S_LLC_CONFIG:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi)
			idi->llc_mode = a->value;
		else
			return -EINVAL;
		break;
	default:
		ret = is_vender_vidioc_s_ctrl(ivc, a);
		if (ret) {
			mierr("unsupported ioctl(%lx, %d)", instance, a->id,
								a->id & 0xff);
			return -EINVAL;
		}
	}

	return 0;
}

int is_vidioc_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	u32 instance = get_instance_video_ctx(ivc);
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	int i, ret;

	mdbgv_video(ivc, iv, __func__);

	if (a->which != V4L2_CTRL_CLASS_CAMERA) {
		mierr("invalid control class(%d)", instance, a->which);
		return -EINVAL;
	}

	for (i = 0; i < a->count; i++) {
		ext_ctrl = (a->controls + i);

		ctrl.id = ext_ctrl->id;
		ctrl.value = ext_ctrl->value;

		ret = is_vidioc_s_ctrl(file, fh, &ctrl);
		if (ret) {
			mierr("failure in is_vidioc_s_ctrl(): %d", instance, ret);
			return ret;
		}
	}

	return 0;
}

static struct v4l2_ioctl_ops is_default_v4l2_ioctl_ops = {
	.vidioc_querycap		= is_vidioc_querycap,
	.vidioc_g_fmt_vid_out_mplane	= is_vidioc_g_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= is_vidioc_g_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= is_vidioc_s_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= is_vidioc_s_fmt_mplane,
	.vidioc_reqbufs			= is_vidioc_reqbufs,
	.vidioc_querybuf		= is_vidioc_querybuf,
	.vidioc_qbuf			= is_vidioc_qbuf,
	.vidioc_dqbuf			= is_vidioc_dqbuf,
	.vidioc_prepare_buf		= is_vidioc_prepare_buf,
	.vidioc_streamon		= is_vidioc_streamon,
	.vidioc_streamoff		= is_vidioc_streamoff,
	.vidioc_s_input			= is_vidioc_s_input,
	.vidioc_g_ctrl			= is_vidioc_g_ctrl,
	.vidioc_g_ext_ctrls		= is_vidioc_g_ext_ctrls,
	.vidioc_s_ctrl			= is_vidioc_s_ctrl,
	.vidioc_s_ext_ctrls		= is_vidioc_s_ext_ctrls,
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Pablo video operaions                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int is_video_qbuf(struct is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_queue *queue;
	struct vb2_queue *vbq;
	struct vb2_buffer *vb;
	struct is_video *video;
	int plane;
	struct vb2_plane planes[VB2_MAX_PLANES];

	FIMC_BUG(!vctx);
	FIMC_BUG(!buf);

	TIME_QUEUE(TMQ_QS);

	queue = GET_QUEUE(vctx);
	vbq = queue->vbq;
	video = GET_VIDEO(vctx);

	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_req++;

	ret = is_vb2_qbuf(queue->vbq, buf);
	if (ret) {
		mverr("vb2_qbuf is fail(index : %d, %d)", vctx, video, buf->index, ret);

		if (vbq->fileio) {
			mverr("file io in progress", vctx, video);
			ret = -EBUSY;
			goto p_err;
		}

		if (buf->type != queue->vbq->type) {
			mverr("buf type is invalid(%d != %d)", vctx, video,
				buf->type, queue->vbq->type);
			ret = -EINVAL;
			goto p_err;
		}

		if (buf->index >= vbq->num_buffers) {
			mverr("buffer index%d out of range", vctx, video, buf->index);
			ret = -EINVAL;
			goto p_err;
		}

		if (buf->memory != vbq->memory) {
			mverr("invalid memory type%d", vctx, video, buf->memory);
			ret = -EINVAL;
			goto p_err;
		}

		vb = vbq->bufs[buf->index];
		if (!vb) {
			mverr("vb is NULL", vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		if (V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
			/* Is memory for copying plane information present? */
			if (buf->m.planes == NULL) {
				mverr("multi-planar buffer passed but "
					   "planes array not provided\n", vctx, video);
				ret = -EINVAL;
				goto p_err;
			}

			if (buf->length < vb->num_planes || buf->length > VB2_MAX_PLANES) {
				mverr("incorrect planes array length, "
					   "expected %d, got %d\n", vctx, video,
					   vb->num_planes, buf->length);
				ret = -EINVAL;
				goto p_err;
			}
		}

		/* for detect vb2 framework err, operate some vb2 functions */
		memset(planes, 0, sizeof(planes[0]) * vb->num_planes);
		vb->vb2_queue->buf_ops->is_fill_vb2_buffer(vb, buf, planes);

		for (plane = 0; plane < vb->num_planes; ++plane) {
			struct dma_buf *dbuf;

			dbuf = dma_buf_get(planes[plane].m.fd);
			if (IS_ERR_OR_NULL(dbuf)) {
				mverr("invalid dmabuf fd(%d) for plane %d\n",
					vctx, video, planes[plane].m.fd, plane);
				goto p_err;
			}

			if (planes[plane].length == 0)
				planes[plane].length = (unsigned int)dbuf->size;

			if (planes[plane].length < vb->planes[plane].min_length) {
				mverr("invalid dmabuf length %u for plane %d, "
					"minimum length %u\n",
					vctx, video, planes[plane].length, plane,
					vb->planes[plane].min_length);
				dma_buf_put(dbuf);
				goto p_err;
			}

			dma_buf_put(dbuf);
		}

		goto p_err;
	}

p_err:
	TIME_QUEUE(TMQ_QE);
	return ret;
}

int is_video_dqbuf(struct is_video_ctx *vctx,
	struct v4l2_buffer *buf,
	bool blocking)
{
	int ret = 0;
	struct is_video *video;
	struct is_queue *queue;
	struct is_sysfs_debug *sysfs_debug;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!buf);

	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);
	sysfs_debug = is_get_sysfs_debug();

	if (!queue->vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (buf->type != queue->vbq->type) {
		mverr("buf type is invalid(%d != %d)", vctx, video, buf->type, queue->vbq->type);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		mverr("queue is not streamon(%ld)", vctx,  video, queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_dqe++;

	ret = vb2_dqbuf(queue->vbq, buf, blocking);
	if (ret) {
		mverr("vb2_dqbuf is fail(%d)", vctx,  video, ret);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug->hal_debug_mode) &&
				ret == -ERESTARTSYS) {
			msleep(sysfs_debug->hal_debug_delay);
			panic("HAL dead");
		}
		goto p_err;
	}

p_err:
	TIME_QUEUE(TMQ_DQ);
	return ret;
}

int is_video_buffer_done(struct is_video_ctx *vctx,
	u32 index, u32 state)
{
	int ret = 0;
	struct vb2_buffer *vb;
	struct is_queue *queue;
	struct is_video *video;

	FIMC_BUG(!vctx);
	FIMC_BUG(!vctx->video);
	FIMC_BUG(index >= IS_MAX_BUFS);

	queue = GET_QUEUE(vctx);
	video = GET_VIDEO(vctx);

	if (!queue->vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	vb = queue->vbq->bufs[index];
	if (!vb) {
		mverr("vb is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		warn("%d video queue is not stream on", vctx->video->id);
		ret = -EINVAL;
		goto p_err;
	}

	if (vb->state != VB2_BUF_STATE_ACTIVE && vb->state != VB2_BUF_STATE_ERROR) {
		mverr("vb buffer[%d] state is not active(%d)", vctx, video, index, vb->state);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_com++;

	vb2_buffer_done(vb, state);

p_err:
	return ret;
}
