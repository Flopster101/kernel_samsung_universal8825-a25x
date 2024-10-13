// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "is-hw-common-dma.h"
#include "hardware/sfr/is-sfr-common-dma.h"

/* Define the test cases. */

static void pablo_hw_api_common_dma_prepare_dma(struct kunit *test, struct is_common_dma *dma)
{
	int ret;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dma);

	ret = is_hw_dma_set_ops(dma);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dma->ops);

	dma->set_id = COREX_DIRECT;
	dma->base = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dma->base);
}

static void pablo_hw_api_common_dma_finish_dma(struct kunit *test, struct is_common_dma *dma)
{
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dma);

	kunit_kfree(test, dma->base);
}

static void pablo_hw_api_common_dma_is_hw_dma_dump_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_dump, dma.base);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_get_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 enable = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_get_enable, &enable);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, enable, (u32)0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_get_header_crc_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 crc_1p = 0;
	u32 crc_2p = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_get_header_crc, &crc_1p, &crc_2p);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, crc_1p, (u32)0);
	KUNIT_EXPECT_EQ(test, crc_2p, (u32)0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_get_img_crc_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 crc_1p = 0;
	u32 crc_2p = 0;
	u32 crc_3p = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_get_img_crc,
			&crc_1p, &crc_2p, &crc_3p);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, crc_1p, (u32)0);
	KUNIT_EXPECT_EQ(test, crc_2p, (u32)0);
	KUNIT_EXPECT_EQ(test, crc_3p, (u32)0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_get_max_mo_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 max_mo = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_get_max_mo, &max_mo);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, max_mo, (u32)0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_get_mon_status_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 status = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);


	ret = CALL_DMA_OPS(&dma, dma_get_mon_status, &status);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, status, (u32)0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_get_rgb_format_kunit_test(struct kunit *test)
{
	int rgb_format;
	u32 bitwidth = DMA_INOUT_BIT_WIDTH_32BIT;
	u32 plane = 0;
	u32 order = DMA_INOUT_ORDER_ARGB;
	int expect_fmt = DMA_FMT_RGB_ARGB8888;

	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	order = DMA_INOUT_ORDER_BGRA;
	expect_fmt = DMA_FMT_RGB_BGRA8888;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	order = DMA_INOUT_ORDER_RGBA;
	expect_fmt = DMA_FMT_RGB_RGBA8888;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	order = DMA_INOUT_ORDER_ABGR;
	expect_fmt = DMA_FMT_RGB_ABGR8888;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	order = DMA_INOUT_ORDER_GR_BG;
	expect_fmt = DMA_FMT_RGB_RGBA8888;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	order = DMA_INOUT_ORDER_RGBA;
	expect_fmt = DMA_FMT_RGB_RGBA1010102;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	order = DMA_INOUT_ORDER_ABGR;
	expect_fmt = DMA_FMT_RGB_ABGR1010102;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	order = DMA_INOUT_ORDER_GR_BG;
	expect_fmt = -EINVAL;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

}

static void pablo_hw_api_common_dma_is_hw_dma_get_yuv_format_kunit_test(struct kunit *test)
{
	int yuv_format;
	u32 bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	u32 format = DMA_INOUT_FORMAT_YUV420;
	u32 plane = 2;
	u32 order = DMA_INOUT_ORDER_CbCr;
	int expect_fmt = DMA_FMT_YUV420_2P_UFIRST;

	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 3;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV420_3P;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV420_2P_VFIRST;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = -EINVAL;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 1;
	order = DMA_INOUT_ORDER_CrYCbY;
	expect_fmt = DMA_FMT_YUV422_1P_VYUY;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 1;
	order = DMA_INOUT_ORDER_CbYCrY;
	expect_fmt = DMA_FMT_YUV422_1P_UYVY;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 1;
	order = DMA_INOUT_ORDER_YCrYCb;
	expect_fmt = DMA_FMT_YUV422_1P_YVYU;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 1;
	order = DMA_INOUT_ORDER_YCbYCr;
	expect_fmt = DMA_FMT_YUV422_1P_YUYV;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV422_2P_UFIRST;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV422_2P_VFIRST;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 3;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV422_3P;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 1;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_1P;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 3;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_3P;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_RGB;
	plane = 3;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_3P;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	/* bitwidth : 10 bit */
	bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV420_2P_UFIRST_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 4;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV420_2P_UFIRST_8P2;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV420_2P_VFIRST_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 4;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV420_2P_VFIRST_8P2;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV422_2P_UFIRST_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 4;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV422_2P_UFIRST_8P2;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV422_2P_VFIRST_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 4;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV422_2P_VFIRST_8P2;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 1;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_1P_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 3;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_3P_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	/* bitwidth : 16 bit */
	bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV420_2P_UFIRST_P010;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV420_2P_VFIRST_P010;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV422_2P_UFIRST_P210;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV422_2P_VFIRST_P210;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 1;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_1P_UNPACKED;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 3;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_3P_UNPACKED;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);
}

static void pablo_hw_api_common_dma_is_hw_dma_print_cr_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 option = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_print_cr, option);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_print_info_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 option = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_print_info, option);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_set_auto_flush_en_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 en = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_set_auto_flush_en, en);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_set_comp_error_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 mode = 0;
	u32 value = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_set_comp_error, mode, value);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_set_line_gap_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 line_gap = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_set_line_gap, line_gap);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_set_max_bl_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 max_bl = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_set_max_bl, max_bl);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_set_max_mo_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 max_mo = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_set_max_mo, max_mo);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_set_mono_mode_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 mode = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_set_mono_mode, mode);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}

static void pablo_hw_api_common_dma_is_hw_dma_set_msb_align_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 align = 0;

	pablo_hw_api_common_dma_prepare_dma(test, &dma);

	ret = CALL_DMA_OPS(&dma, dma_set_msb_align, align);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_api_common_dma_finish_dma(test, &dma);
}


static struct kunit_case pablo_hw_api_common_dma_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_get_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_get_header_crc_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_get_img_crc_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_get_max_mo_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_get_mon_status_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_get_rgb_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_get_yuv_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_print_cr_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_print_info_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_set_auto_flush_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_set_comp_error_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_set_line_gap_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_set_max_bl_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_set_max_mo_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_set_mono_mode_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_is_hw_dma_set_msb_align_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_common_dma_kunit_test_suite = {
	.name = "pablo-hw-api-common-dma-kunit-test",
	.test_cases = pablo_hw_api_common_dma_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_common_dma_kunit_test_suite);

MODULE_LICENSE("GPL");
