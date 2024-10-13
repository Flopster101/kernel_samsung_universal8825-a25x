/*
 * drivers/media/platform/exynos/mfc/mfc_ctx_ctrl.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "base/mfc_common.h"

static struct mfc_ctrl_cfg mfc_dec_ctrl_list[] = {
	{
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_PICTURE_TAG,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_RET_PICTURE_TAG_TOP,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_DISPLAY_STATUS,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_DISPLAY_STATUS,
		.mask = 0x7,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	/* CRC related definitions are based on non-H.264 type */
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_LUMA,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_DISPLAY_FIRST_PLANE_CRC,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_CHROMA,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_DISPLAY_SECOND_PLANE_CRC,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_CHROMA1,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_DISPLAY_THIRD_PLANE_CRC,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_2BIT_LUMA,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_DISPLAY_FIRST_PLANE_2BIT_CRC,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_2BIT_CHROMA,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_DISPLAY_SECOND_PLANE_2BIT_CRC,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CRC_GENERATED,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_DISPLAY_STATUS,
		.mask = 0x1,
		.shft = 6,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FP_AVAIL,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_SEI_AVAIL,
		.mask = 0x1,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FP_ARRGMENT_ID,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_FRAME_PACK_ARRGMENT_ID,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FP_INFO,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_FRAME_PACK_SEI_INFO,
		.mask = 0x3FFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FP_GRID_POS,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_FRAME_PACK_GRID_POS,
		.mask = 0xFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_H264_MVC_VIEW_ID,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_MVC_VIEW_ID,
		.mask = 0xFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_SEI_MAX_PIC_AVERAGE_LIGHT,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_CONTENT_LIGHT_LEVEL_INFO_SEI,
		.mask = 0xFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_SEI_MAX_CONTENT_LIGHT,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_CONTENT_LIGHT_LEVEL_INFO_SEI,
		.mask = 0xFFFF,
		.shft = 16,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_SEI_MAX_DISPLAY_LUMINANCE,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_0,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_SEI_MIN_DISPLAY_LUMINANCE,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_1,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_MATRIX_COEFFICIENTS,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_VIDEO_SIGNAL_TYPE,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_FORMAT,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_VIDEO_SIGNAL_TYPE,
		.mask = 0x7,
		.shft = 26,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_FULL_RANGE_FLAG,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_VIDEO_SIGNAL_TYPE,
		.mask = 0x1,
		.shft = 25,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_COLOUR_PRIMARIES,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_VIDEO_SIGNAL_TYPE,
		.mask = 0xFF,
		.shft = 16,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_TRANSFER_CHARACTERISTICS,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_VIDEO_SIGNAL_TYPE,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_SEI_WHITE_POINT,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_2,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_SEI_DISPLAY_PRIMARIES_0,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_3,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_SEI_DISPLAY_PRIMARIES_1,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_4,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_SEI_DISPLAY_PRIMARIES_2,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_MASTERING_DISPLAY_COLOUR_VOLUME_SEI_5,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_POC,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_D_RET_PICTURE_TIME_TOP,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_FRAME_ERROR_TYPE,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_ERROR_CODE,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* buffer additional information */
		.type = MFC_CTRL_TYPE_SRC,
		.id = V4L2_CID_MPEG_VIDEO_SRC_BUF_FLAG,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_NONE,
		.flag_mode = MFC_CTRL_MODE_NONE,
	},
	{	/* buffer additional information */
		.type = MFC_CTRL_TYPE_DST,
		.id = V4L2_CID_MPEG_VIDEO_DST_BUF_FLAG,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_NONE,
		.flag_mode = MFC_CTRL_MODE_NONE,
	}
};
#define NUM_DEC_CTRL_CFGS ARRAY_SIZE(mfc_dec_ctrl_list)

static struct mfc_ctrl_cfg mfc_enc_ctrl_list[] = {
	{	/* set frame tag */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_PICTURE_TAG,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* get frame tag */
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RET_PICTURE_TAG,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* encoded y physical addr */
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_LUMA_ADDR,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_ENCODED_SOURCE_FIRST_ADDR,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* encoded c physical addr */
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CHROMA_ADDR,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_ENCODED_SOURCE_SECOND_ADDR,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* I, not coded frame insertion */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_FRAME_INSERTION,
		.mask = 0x3,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* I period change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_I_PERIOD_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_GOP_CONFIG,
		.mask = 0xFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 0,
	},
	{	/* frame rate change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_FRAME_RATE,
		.mask = 0x0000FFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 1,
	},
	{	/* bit rate change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_BIT_RATE_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_BIT_RATE,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 2,
	},
	{	/* frame status (in slice or not) */
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_STATUS,
		.is_volatile = 0,
		.mode = MFC_CTRL_MODE_NONE,
		.addr = 0,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* H.264 I frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* H.264 I frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H264_MIN_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* H.263 I frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H263_MAX_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* H.263 I frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H263_MIN_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* MPEG4 I frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* MPEG4 I frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* VP8 I frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_VP8_MAX_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* VP8 I frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_VP8_MIN_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* VP9 I frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_VP9_MAX_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* VP9 I frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_VP9_MIN_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* HEVC I frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* HEVC I frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* H.264 P frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* H.264 P frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H264_MIN_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* H.263 P frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H263_MAX_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* H.263 P frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H263_MIN_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* MPEG4 P frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* MPEG4 P frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* VP8 P frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_VP8_MAX_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* VP8 P frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_VP8_MIN_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* VP9 P frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_VP9_MAX_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* VP9 P frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_VP9_MIN_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* HEVC P frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* HEVC P frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP_P,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* H.264 B frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP_B,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 24,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* H.264 B frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H264_MIN_QP_B,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 16,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* MPEG4 B frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP_B,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 24,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* MPEG4 B frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP_B,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 16,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* HEVC B frame QP Max change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP_B,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 24,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* HEVC B frame QP Min change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP_B,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_QP_BOUND_PB,
		.mask = 0xFF,
		.shft = 16,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 4,
	},
	{	/* H.264 Dynamic Temporal Layer & bitrate change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_HIERARCHICAL_BIT_RATE_LAYER0,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 10,
	},
	{	/* HEVC Dynamic Temporal Layer & bitrate change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_HIERARCHICAL_BIT_RATE_LAYER0,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 10,
	},
	{	/* VP8 Dynamic Temporal Layer change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_HIERARCHICAL_BIT_RATE_LAYER0,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 10,
	},
	{	/* VP9 Dynamic Temporal Layer change */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_CH,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_HIERARCHICAL_BIT_RATE_LAYER0,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 10,
	},
	{	/* set level */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_PICTURE_PROFILE,
		.mask = 0x000000FF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 5,
	},
	{	/* set profile */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_PICTURE_PROFILE,
		.mask = 0x0000000F,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 5,
	},
	{	/* set store LTR */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_MFC_H264_MARK_LTR,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_H264_NAL_CONTROL,
		.mask = 0x00000003,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* set use LTR */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_MFC_H264_USE_LTR,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_H264_NAL_CONTROL,
		.mask = 0x00000003,
		.shft = 2,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* set base layer priority */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_MFC_H264_BASE_PRIORITY,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_H264_HD_SVC_EXTENSION_0,
		.mask = 0x0000003F,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 12,
	},
	{	/* set QP per each frame */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_MFC_CONFIG_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_FIXED_PICTURE_QP,
		.mask = 0x000000FF,
		.shft = 24,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* Region-Of-Interest control */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_ROI_CONTROL,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_ROI_CTRL,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* set YSUM for weighted prediction */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_YSUM,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_WEIGHT_FOR_WEIGHTED_PREDICTION,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_mode = 0,
		.flag_shft = 0,
	},
	{	/* set base layer priority */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_RATIO_OF_INTRA,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_MODE,
		.mask = 0x000000FF,
		.shft = 8,
		.flag_mode = MFC_CTRL_MODE_SFR,
		.flag_addr = MFC_REG_E_PARAM_CHANGE,
		.flag_shft = 13,
	},
	{	/* sync the timestamp for drop control */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_DROP_CONTROL,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_RC_FRAME_RATE,
		.mask = 0x0000FFFF,
		.shft = 0,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* average QP of current frame */
		.type = MFC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_AVERAGE_QP,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_NAL_DONE_INFO,
		.mask = 0x000000FF,
		.shft = 12,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* HOR range position of current frame */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_MV_HOR_POSITION_L0,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_MV_HOR_RANGE,
		.mask = 0x000000FF,
		.shft = 16,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* HOR range position of current frame */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_MV_HOR_POSITION_L1,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_MV_HOR_RANGE,
		.mask = 0x000000FF,
		.shft = 24,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* VER range position of current frame */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_MV_VER_POSITION_L0,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_MV_VER_RANGE,
		.mask = 0x000000FF,
		.shft = 16,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* VER range position of current frame */
		.type = MFC_CTRL_TYPE_SET_SRC,
		.id = V4L2_CID_MPEG_VIDEO_MV_VER_POSITION_L1,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_SFR,
		.addr = MFC_REG_E_MV_VER_RANGE,
		.mask = 0x000000FF,
		.shft = 24,
		.flag_mode = MFC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{	/* buffer additional information */
		.type = MFC_CTRL_TYPE_SRC,
		.id = V4L2_CID_MPEG_VIDEO_SRC_BUF_FLAG,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_NONE,
		.flag_mode = MFC_CTRL_MODE_NONE,
	},
	{	/* buffer additional information */
		.type = MFC_CTRL_TYPE_DST,
		.id = V4L2_CID_MPEG_VIDEO_DST_BUF_FLAG,
		.is_volatile = 1,
		.mode = MFC_CTRL_MODE_NONE,
		.flag_mode = MFC_CTRL_MODE_NONE,
	}
};
#define NUM_ENC_CTRL_CFGS ARRAY_SIZE(mfc_enc_ctrl_list)

void mfc_ctrl_cleanup_ctx(struct mfc_ctx *ctx)
{
	struct mfc_ctx_ctrl *ctx_ctrl;

	while (!list_empty(&ctx->ctrls)) {
		ctx_ctrl = list_entry((&ctx->ctrls)->next,
				      struct mfc_ctx_ctrl, list);
		list_del(&ctx_ctrl->list);
		kfree(ctx_ctrl);
	}

	INIT_LIST_HEAD(&ctx->ctrls);
}

int __mfc_ctrl_init_ctx(struct mfc_ctx *ctx, struct mfc_ctrl_cfg *ctrl_list, int count)
{
	unsigned long i;
	struct mfc_ctx_ctrl *ctx_ctrl;

	INIT_LIST_HEAD(&ctx->ctrls);

	for (i = 0; i < count; i++) {
		ctx_ctrl = kzalloc(sizeof(struct mfc_ctx_ctrl), GFP_KERNEL);
		if (ctx_ctrl == NULL) {
			mfc_ctx_err("Failed to allocate context control id: 0x%08x, type: %d\n",
					ctrl_list[i].id, ctrl_list[i].type);

			mfc_ctrl_cleanup_ctx(ctx);

			return -ENOMEM;
		}

		ctx_ctrl->type = ctrl_list[i].type;
		ctx_ctrl->id = ctrl_list[i].id;
		ctx_ctrl->addr = ctrl_list[i].addr;
		ctx_ctrl->set.has_new = 0;
		ctx_ctrl->set.val = 0;
		ctx_ctrl->get.has_new = 0;
		ctx_ctrl->get.val = 0;

		list_add_tail(&ctx_ctrl->list, &ctx->ctrls);
	}

	return 0;
}

int mfc_ctrl_init_ctx(struct mfc_ctx *ctx)
{
	if (ctx->type == MFCINST_DECODER)
		return __mfc_ctrl_init_ctx(ctx, mfc_dec_ctrl_list, NUM_DEC_CTRL_CFGS);
	else if (ctx->type == MFCINST_ENCODER)
		return __mfc_ctrl_init_ctx(ctx, mfc_enc_ctrl_list, NUM_ENC_CTRL_CFGS);

	mfc_ctx_err("[CTRLS] invalid type %d\n", ctx->type);
	return -EINVAL;
}

void mfc_ctrl_reset_buf(struct list_head *head)
{
	struct mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		buf_ctrl->has_new = 0;
		buf_ctrl->val = 0;
		buf_ctrl->old_val = 0;
		buf_ctrl->updated = 0;
	}
}

void __mfc_ctrl_cleanup_buf(struct list_head *head)
{
	struct mfc_buf_ctrl *buf_ctrl;

	while (!list_empty(head)) {
		buf_ctrl = list_entry(head->next,
				struct mfc_buf_ctrl, list);
		list_del(&buf_ctrl->list);
		kfree(buf_ctrl);
	}

	INIT_LIST_HEAD(head);
}

int mfc_ctrl_cleanup_buf(struct mfc_ctx *ctx, enum mfc_ctrl_type type, unsigned int index)
{
	struct list_head *head;

	if (index >= MFC_MAX_BUFFERS) {
		mfc_ctx_err("Per-buffer control index is out of range\n");
		return -EINVAL;
	}

	if (type & MFC_CTRL_TYPE_SRC) {
		if (!(test_and_clear_bit(index, &ctx->src_ctrls_avail)))
			return 0;

		head = &ctx->src_ctrls[index];
	} else if (type & MFC_CTRL_TYPE_DST) {
		if (!(test_and_clear_bit(index, &ctx->dst_ctrls_avail)))
			return 0;

		head = &ctx->dst_ctrls[index];
	} else {
		mfc_ctx_err("Control type mismatch. type : %d\n", type);
		return -EINVAL;
	}

	__mfc_ctrl_cleanup_buf(head);

	return 0;
}

int __mfc_ctrl_init_buf(struct mfc_ctx *ctx, struct mfc_ctrl_cfg *ctrl_list,
		enum mfc_ctrl_type type, unsigned int index, int count)
{
	unsigned long i;
	struct mfc_buf_ctrl *buf_ctrl;
	struct list_head *head;

	if (index >= MFC_MAX_BUFFERS) {
		mfc_ctx_err("Per-buffer control index is out of range\n");
		return -EINVAL;
	}

	if (type & MFC_CTRL_TYPE_SRC) {
		if (test_bit(index, &ctx->src_ctrls_avail)) {
			mfc_ctrl_reset_buf(&ctx->src_ctrls[index]);

			return 0;
		}

		head = &ctx->src_ctrls[index];
	} else if (type & MFC_CTRL_TYPE_DST) {
		if (test_bit(index, &ctx->dst_ctrls_avail)) {
			mfc_ctrl_reset_buf(&ctx->dst_ctrls[index]);

			return 0;
		}

		head = &ctx->dst_ctrls[index];
	} else {
		mfc_ctx_err("Control type mismatch. type : %d\n", type);
		return -EINVAL;
	}

	INIT_LIST_HEAD(head);

	for (i = 0; i < count; i++) {
		if (!(type & ctrl_list[i].type))
			continue;

		buf_ctrl = kzalloc(sizeof(struct mfc_buf_ctrl), GFP_KERNEL);
		if (buf_ctrl == NULL) {
			mfc_ctx_err("Failed to allocate buffer control id: 0x%08x, type: %d\n",
					ctrl_list[i].id, ctrl_list[i].type);

			__mfc_ctrl_cleanup_buf(head);

			return -ENOMEM;
		}

		buf_ctrl->type = ctrl_list[i].type;
		buf_ctrl->id = ctrl_list[i].id;
		buf_ctrl->is_volatile = ctrl_list[i].is_volatile;
		buf_ctrl->mode = ctrl_list[i].mode;
		buf_ctrl->addr = ctrl_list[i].addr;
		buf_ctrl->mask = ctrl_list[i].mask;
		buf_ctrl->shft = ctrl_list[i].shft;
		buf_ctrl->flag_mode = ctrl_list[i].flag_mode;
		buf_ctrl->flag_addr = ctrl_list[i].flag_addr;
		buf_ctrl->flag_shft = ctrl_list[i].flag_shft;

		list_add_tail(&buf_ctrl->list, head);
	}

	mfc_ctrl_reset_buf(head);

	if (type & MFC_CTRL_TYPE_SRC)
		set_bit(index, &ctx->src_ctrls_avail);
	else
		set_bit(index, &ctx->dst_ctrls_avail);

	return 0;
}

int mfc_ctrl_init_buf(struct mfc_ctx *ctx, enum mfc_ctrl_type type, unsigned int index)
{
	if (ctx->type == MFCINST_DECODER)
		return __mfc_ctrl_init_buf(ctx, mfc_dec_ctrl_list, type, index, NUM_DEC_CTRL_CFGS);
	else if (ctx->type == MFCINST_ENCODER)
		return __mfc_ctrl_init_buf(ctx, mfc_enc_ctrl_list, type, index, NUM_ENC_CTRL_CFGS);

	mfc_ctx_err("[CTRLS] invalid type %d\n", ctx->type);
	return -EINVAL;
}

static void __mfc_enc_set_roi(struct mfc_ctx *ctx, struct mfc_buf_ctrl *buf_ctrl)
{
	struct mfc_enc *enc = ctx->enc_priv;
	int index = 0;
	unsigned int reg = 0;

	index = enc->roi_index;
	if (enc->roi_info[index].enable) {
		enc->roi_index = (index + 1) % MFC_MAX_EXTRA_BUF;
		reg |= enc->roi_info[index].enable;
		reg &= ~(0xFF << 8);
		reg |= (enc->roi_info[index].lower_qp << 8);
		reg &= ~(0xFFFF << 16);
		reg |= (enc->roi_info[index].upper_qp << 16);
		/*
		 * 2bit ROI
		 * - All codec type: upper_qp and lower_qp is valid
		 * 8bit ROI
		 * - H.264/HEVC/MPEG4: upper_qp and lower_qp is invalid
		 * - VP8/VP9: upper_qp and lower_qp is valid
		 */
		mfc_debug(3, "[ROI] buffer[%d] en %d QP lower %d upper %d reg %#x\n",
				index, enc->roi_info[index].enable,
				enc->roi_info[index].lower_qp,
				enc->roi_info[index].upper_qp,
				reg);
	} else {
		mfc_debug(3, "[ROI] buffer[%d] is not enabled\n", index);
	}

	buf_ctrl->val = reg;
	buf_ctrl->old_val2 = index;
}

void mfc_ctrl_to_buf(struct mfc_ctx *ctx, struct list_head *head)
{
	struct mfc_ctx_ctrl *ctx_ctrl;
	struct mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
		if (!(ctx_ctrl->type & MFC_CTRL_TYPE_SET) ||
					!ctx_ctrl->set.has_new)
			continue;

		list_for_each_entry(buf_ctrl, head, list) {
			if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET))
				continue;

			if (buf_ctrl->id == ctx_ctrl->id) {
				buf_ctrl->has_new = 1;
				buf_ctrl->val = ctx_ctrl->set.val;

				if (buf_ctrl->is_volatile)
					buf_ctrl->updated = 0;

				ctx_ctrl->set.has_new = 0;

				if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_ROI_CONTROL)
					__mfc_enc_set_roi(ctx, buf_ctrl);

				break;
			}
		}
	}
}

void mfc_ctrl_to_ctx(struct mfc_ctx *ctx, struct list_head *head)
{
	struct mfc_ctx_ctrl *ctx_ctrl;
	struct mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_GET) || !buf_ctrl->has_new)
			continue;

		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			if (!(ctx_ctrl->type & MFC_CTRL_TYPE_GET))
				continue;

			if (ctx_ctrl->id == buf_ctrl->id) {
				if (ctx_ctrl->get.has_new)
					mfc_debug(8, "Overwrite ctx_ctrl value id: %#x, val: %d\n",
						ctx_ctrl->id, ctx_ctrl->get.val);

				ctx_ctrl->get.has_new = 1;
				ctx_ctrl->get.val = buf_ctrl->val;

				buf_ctrl->has_new = 0;
			}
		}
	}
}

int mfc_ctrl_get_buf_val(struct mfc_ctx *ctx, struct list_head *head, unsigned int id)
{
	struct mfc_buf_ctrl *buf_ctrl;
	int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (buf_ctrl->id == id) {
			value = buf_ctrl->val;
			mfc_debug(6, "[CTRLS] Get buffer control id: 0x%08x, val: %d (%#x)\n",
					buf_ctrl->id, value, value);
			break;
		}
	}

	return value;
}

void mfc_ctrl_update_buf_val(struct mfc_ctx *ctx, struct list_head *head,
		unsigned int id, int value)
{
	struct mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		if (buf_ctrl->id == id) {
			buf_ctrl->val = value;
			mfc_debug(6, "[CTRLS] Update buffer control id: 0x%08x, val: %d (%#x)\n",
					buf_ctrl->id, buf_ctrl->val, buf_ctrl->val);
			break;
		}
	}
}

struct mfc_ctrls_ops mfc_ctrls_ops = {
	.cleanup_ctx_ctrls		= mfc_ctrl_cleanup_ctx,
	.init_ctx_ctrls			= mfc_ctrl_init_ctx,
	.reset_buf_ctrls		= mfc_ctrl_reset_buf,
	.cleanup_buf_ctrls		= mfc_ctrl_cleanup_buf,
	.init_buf_ctrls			= mfc_ctrl_init_buf,
	.to_buf_ctrls			= mfc_ctrl_to_buf,
	.to_ctx_ctrls			= mfc_ctrl_to_ctx,
	.get_buf_ctrl_val		= mfc_ctrl_get_buf_val,
	.update_buf_val			= mfc_ctrl_update_buf_val,
};
