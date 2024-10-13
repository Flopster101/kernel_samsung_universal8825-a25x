/*
 * Samsung Exynos9 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_COMMON_ENUM_H
#define IS_COMMON_ENUM_H

#define IS_MAX_BUFS		VIDEO_MAX_FRAME
#define IS_MAX_PLANES		17
#define IS_EXT_MAX_PLANES	4

/*
 * pixel type
 * [0:5] : pixel size
 * [6:7] : extra
 */
#define PIXEL_TYPE_SIZE_MASK			0x3F
#define	PIXEL_TYPE_SIZE_SHIFT			0

#define PIXEL_TYPE_EXTRA_MASK			0xC0
#define	PIXEL_TYPE_EXTRA_SHIFT			6
enum camera_pixel_size {
	CAMERA_PIXEL_SIZE_8BIT = 0,
	CAMERA_PIXEL_SIZE_10BIT,
	CAMERA_PIXEL_SIZE_PACKED_10BIT,
	CAMERA_PIXEL_SIZE_8_2BIT,
	CAMERA_PIXEL_SIZE_12BIT,
	CAMERA_PIXEL_SIZE_13BIT,
	CAMERA_PIXEL_SIZE_14BIT,
	CAMERA_PIXEL_SIZE_15BIT,
	CAMERA_PIXEL_SIZE_16BIT,
	CAMERA_PIXEL_SIZE_PACKED_12BIT, /* DON'T USE, JUST FOR ENUM SYNC WITH HAL */
	CAMERA_PIXEL_SIZE_AND10,
};

/*
 * COMP : Lossless Compression
 * COMP_LOSS : Loss Compression
 */
enum camera_extra {
	NONE = 0,
	COMP,
	COMP_LOSS
};

/* ----------------------  Input  ----------------------------------- */
enum control_command {
	CONTROL_COMMAND_STOP	= 0,
	CONTROL_COMMAND_START	= 1,
	CONTROL_COMMAND_TEST	= 2
};

enum bypass_command {
	CONTROL_BYPASS_DISABLE		= 0,
	CONTROL_BYPASS_ENABLE		= 1
};

enum control_error {
	CONTROL_ERROR_NO		= 0
};

enum otf_input_command {
	OTF_INPUT_COMMAND_DISABLE	= 0,
	OTF_INPUT_COMMAND_ENABLE	= 1
};

enum otf_input_format {
	OTF_INPUT_FORMAT_BAYER			= 0, /* 1 Channel */
	OTF_INPUT_FORMAT_YUV444			= 1, /* 3 Channel */
	OTF_INPUT_FORMAT_YUV422			= 2, /* 3 Channel */
	OTF_INPUT_FORMAT_YUV420			= 3, /* 3 Channel */
	OTF_INPUT_FORMAT_Y			= 7,
	OTF_INPUT_FORMAT_BAYER_PLUS		= 10,
	OTF_INPUT_FORMAT_BAYER_COMP		= 11,
	OTF_INPUT_FORMAT_BAYER_COMP_LOSSY	= 12,
};

enum otf_input_bitwidth {
	OTF_INPUT_BIT_WIDTH_14BIT	= 14,
	OTF_INPUT_BIT_WIDTH_13BIT	= 13,
	OTF_INPUT_BIT_WIDTH_12BIT	= 12,
	OTF_INPUT_BIT_WIDTH_11BIT	= 11,
	OTF_INPUT_BIT_WIDTH_10BIT	= 10,
	OTF_INPUT_BIT_WIDTH_9BIT	= 9,
	OTF_INPUT_BIT_WIDTH_8BIT	= 8
};

enum otf_input_order {
	OTF_INPUT_ORDER_BAYER_GR_BG	= 0,
	OTF_INPUT_ORDER_BAYER_RG_GB	= 1,
	OTF_INPUT_ORDER_BAYER_BG_GR	= 2,
	OTF_INPUT_ORDER_BAYER_GB_RG	= 3
};

enum otf_input_path {
	OTF_INPUT_SENSOR_PATH = 0,
	OTF_INPUT_PAF_RDMA_PATH = 1,
	OTF_INPUT_PAF_VOTF_PATH = 2
};

enum otf_intput_error {
       OTF_INPUT_ERROR_NO		= 0 /* Input setting is done */
};

enum dma_input_command {
	DMA_INPUT_COMMAND_DISABLE	= 0,
	DMA_INPUT_COMMAND_ENABLE	= 1,
};

enum dma_inout_format {
	DMA_INOUT_FORMAT_BAYER			= 0,
	DMA_INOUT_FORMAT_YUV444			= 1,
	DMA_INOUT_FORMAT_YUV422			= 2,
	DMA_INOUT_FORMAT_YUV420			= 3,
	DMA_INOUT_FORMAT_RGB			= 4,
	DMA_INOUT_FORMAT_BAYER_PACKED		= 5,
	DMA_INOUT_FORMAT_YUV422_CHUNKER		= 6,
	DMA_INOUT_FORMAT_YUV444_TRUNCATED	= 7,
	DMA_INOUT_FORMAT_YUV422_TRUNCATED	= 8,
	DMA_INOUT_FORMAT_Y			= 9,
	DMA_INOUT_FORMAT_BAYER_PLUS		= 10,
	DMA_INOUT_FORMAT_BAYER_COMP		= 11,
	DMA_INOUT_FORMAT_BAYER_COMP_LOSSY	= 12,
	DMA_INOUT_FORMAT_YUV422_PACKED		= 17,
	DMA_INOUT_FORMAT_SEGCONF		= 19,
	DMA_INOUT_FORMAT_HF			= 20,
	DMA_INOUT_FORMAT_DRCGAIN		= 21,
	DMA_INOUT_FORMAT_NOISE			= 22,
	DMA_INOUT_FORMAT_SVHIST			= 23,
	DMA_INOUT_FORMAT_DRCGRID		= 24,
	DMA_INOUT_FORMAT_ANDROID		= 25,
	DMA_INOUT_FORMAT_MAX
};

enum dma_input_votf {
	DMA_INPUT_VOTF_DISABLE				= 0,
	DMA_INPUT_VOTF_ENABLE				= 1
};

enum dma_input_sbwc_type {
	DMA_INPUT_SBWC_DISABLE				= 0,
	DMA_INPUT_SBWC_LOSSYLESS_32B			= 1,
	DMA_INPUT_SBWC_LOSSY_32B			= 2,
	DMA_INPUT_SBWC_LOSSY_CUSTOM_32B			= 3, /* Y lossy, UV lossless */
	DMA_INPUT_SBWC_LOSSYLESS_64B			= 5,
	DMA_INPUT_SBWC_LOSSY_64B			= 6,
	DMA_INPUT_SBWC_LOSSY_CUSTOM_64B			= 7,
	DMA_INPUT_SBWC_MASK				= 0xF
};

enum dma_inout_bitwidth {
	DMA_INOUT_BIT_WIDTH_32BIT	= 32,
	DMA_INOUT_BIT_WIDTH_16BIT	= 16,
	DMA_INOUT_BIT_WIDTH_15BIT	= 15,
	DMA_INOUT_BIT_WIDTH_14BIT	= 14,
	DMA_INOUT_BIT_WIDTH_13BIT	= 13,
	DMA_INOUT_BIT_WIDTH_12BIT	= 12,
	DMA_INOUT_BIT_WIDTH_11BIT	= 11,
	DMA_INOUT_BIT_WIDTH_10BIT	= 10,
	DMA_INOUT_BIT_WIDTH_9BIT	= 9,
	DMA_INOUT_BIT_WIDTH_8BIT	= 8,
	DMA_INOUT_BIT_WIDTH_4BIT	= 4
};

enum dma_inout_plane {
	DMA_INOUT_PLANE_4	= 4,
	DMA_INOUT_PLANE_3	= 3,
	DMA_INOUT_PLANE_2	= 2,
	DMA_INOUT_PLANE_1	= 1
};

enum dma_inout_order {
	DMA_INOUT_ORDER_NO		= 0,
	/* (for DMA_INOUT_PLANE_3) */
	DMA_INOUT_ORDER_CrCb		= 1,
	/* (only valid at DMA_INOUT_PLANE_2) */
	DMA_INOUT_ORDER_CbCr		= 2,
	/* (only valid at DMA_INOUT_PLANE_2) */
	DMA_INOUT_ORDER_YYCbCr		= 3,
	/* NOT USED */
	DMA_INOUT_ORDER_CrYCbY		= 4,
	/* (only valid at DMA_INOUT_FORMAT_YUV422 & DMA_INOUT_PLANE_1) */
	DMA_INOUT_ORDER_CbYCrY		= 5,
	/* (only valid at DMA_INOUT_FORMAT_YUV422 & DMA_INOUT_PLANE_1) */
	DMA_INOUT_ORDER_YCrYCb		= 6,
	/* (only valid at DMA_INOUT_FORMAT_YUV422 & DMA_INOUT_PLANE_1) */
	DMA_INOUT_ORDER_YCbYCr		= 7,
	/* (only valid at DMA_INOUT_FORMAT_YUV422 & DMA_INOUT_PLANE_1) */
	DMA_INOUT_ORDER_CrCbY		= 8,
	/* (only valid at DMA_INOUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_INOUT_ORDER_CbYCr		= 9,
	/* (only valid at DMA_INOUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_INOUT_ORDER_YCbCr		= 10,
	/* (only valid at DMA_INOUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_INOUT_ORDER_CrYCb		= 11,
	/* (only valid at DMA_INOUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_INOUT_ORDER_CbCrY		= 12,
	/* (only valid at DMA_INOUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_INOUT_ORDER_YCrCb		= 13,
	/* (only valid at DMA_INOUT_FORMAT_YUV444 & DMA_OUPUT_PLANE_1) */
	DMA_INOUT_ORDER_BGR		= 14,
	/* (only valid at DMA_INOUT_FORMAT_RGB) */
	DMA_INOUT_ORDER_GB_BG		= 15,
	/* (only valid at DMA_INOUT_FORMAT_BAYER) */
	DMA_INOUT_ORDER_ARGB		= 16,
	/* (only valid at DMA_INOUT_FORMAT_RGB) */
	DMA_INOUT_ORDER_BGRA		= 17,
	/* (only valid at DMA_INOUT_FORMAT_RGB) */
	DMA_INOUT_ORDER_RGBA		= 18,
	/* (only valid at DMA_INOUT_FORMAT_RGB) */
	DMA_INOUT_ORDER_ABGR		= 19,
	/* (only valid at DMA_INOUT_FORMAT_RGB) */
	DMA_INOUT_ORDER_GR_BG		= 20,
	/* (only valid at DMA_INOUT_FORMAT_BAYER) */
};

enum dma_input_MemoryWidthBits {
	DMA_INPUT_MEMORY_WIDTH_16BIT	= 16,
	DMA_INPUT_MEMORY_WIDTH_12BIT	= 12,
};

enum dma_input_error {
	DMA_INPUT_ERROR_NO	= 0 /*  DMA input setting is done */
};

/* ----------------------  Output  ----------------------------------- */
enum otf_output_crop {
	OTF_OUTPUT_CROP_DISABLE		= 0,
	OTF_OUTPUT_CROP_ENABLE		= 1
};

enum otf_output_command {
	OTF_OUTPUT_COMMAND_DISABLE	= 0,
	OTF_OUTPUT_COMMAND_ENABLE	= 1
};

enum otf_output_format {
	OTF_OUTPUT_FORMAT_BAYER			= 0,
	OTF_OUTPUT_FORMAT_YUV444		= 1,
	OTF_OUTPUT_FORMAT_YUV422		= 2,
	OTF_OUTPUT_FORMAT_YUV420		= 3,
	OTF_OUTPUT_FORMAT_RGB			= 4,
	OTF_OUTPUT_FORMAT_YUV444_TRUNCATED	= 5,
	OTF_OUTPUT_FORMAT_YUV422_TRUNCATED	= 6,
	OTF_OUTPUT_FORMAT_Y			= 9,
	OTF_OUTPUT_FORMAT_BAYER_PLUS		= 10,
	OTF_OUTPUT_FORMAT_BAYER_COMP		= 11,
	OTF_OUTPUT_FORMAT_BAYER_COMP_LOSSY	= 12
};

enum dma_output_votf {
	DMA_OUTPUT_VOTF_DISABLE				= 0,
	DMA_OUTPUT_VOTF_ENABLE				= 1
};

enum dma_output_sbwc_type {
	DMA_OUTPUT_SBWC_DISABLE				= 0,
	DMA_OUTPUT_SBWC_LOSSYLESS_32B			= 1,
	DMA_OUTPUT_SBWC_LOSSY_32B			= 2,
	DMA_OUTPUT_SBWC_LOSSYLESS_64B			= 5,
	DMA_OUTPUT_SBWC_LOSSY_64B			= 6
};


enum otf_output_bitwidth {
	OTF_OUTPUT_BIT_WIDTH_32BIT	= 32,
	OTF_OUTPUT_BIT_WIDTH_14BIT	= 14,
	OTF_OUTPUT_BIT_WIDTH_13BIT	= 13,
	OTF_OUTPUT_BIT_WIDTH_12BIT	= 12,
	OTF_OUTPUT_BIT_WIDTH_11BIT	= 11,
	OTF_OUTPUT_BIT_WIDTH_10BIT	= 10,
	OTF_OUTPUT_BIT_WIDTH_9BIT	= 9,
	OTF_OUTPUT_BIT_WIDTH_8BIT	= 8
};

enum otf_output_order {
	OTF_OUTPUT_ORDER_BAYER_GR_BG	= 0,
};

enum otf_output_error {
	OTF_OUTPUT_ERROR_NO = 0 /* Output Setting is done */
};

enum dma_output_command {
	DMA_OUTPUT_COMMAND_DISABLE	= 0,
	DMA_OUTPUT_COMMAND_ENABLE	= 1,
	DMA_OUTPUT_COMMAND_BUF_MNGR	= 2,
	DMA_OUTPUT_UPDATE_MASK_BITS	= 3
};

enum dma_output_notify_dma_done {
	DMA_OUTPUT_NOTIFY_DMA_DONE_DISABLE	= 0,
	DMA_OUTPUT_NOTIFY_DMA_DONE_ENBABLE	= 1,
};

enum dma_output_error {
	DMA_OUTPUT_ERROR_NO		= 0 /* DMA output setting is done */
};

/* ----------------------  Global  ----------------------------------- */
enum global_shotmode_error {
	GLOBAL_SHOTMODE_ERROR_NO	= 0 /* shot-mode setting is done */
};

/* -------------------------  AA  ------------------------------------ */
enum isp_lock_command {
	ISP_AA_COMMAND_START	= 0,
	ISP_AA_COMMAND_STOP	= 1
};

enum isp_lock_target {
	ISP_AA_TARGET_AF	= 1,
	ISP_AA_TARGET_AE	= 2,
	ISP_AA_TARGET_AWB	= 4
};

enum isp_af_mode {
	ISP_AF_MANUAL = 0,
	ISP_AF_SINGLE,
	ISP_AF_CONTINUOUS,
	ISP_AF_REGION,
	ISP_AF_SLEEP,
	ISP_AF_INIT,
	ISP_AF_SET_CENTER_WINDOW,
	ISP_AF_SET_TOUCH_WINDOW,
	ISP_AF_SET_FACE_WINDOW
};

enum isp_af_scene {
	ISP_AF_SCENE_NORMAL		= 0,
	ISP_AF_SCENE_MACRO		= 1
};

enum isp_af_touch {
	ISP_AF_TOUCH_DISABLE = 0,
	ISP_AF_TOUCH_ENABLE
};

enum isp_af_face {
	ISP_AF_FACE_DISABLE = 0,
	ISP_AF_FACE_ENABLE
};

enum isp_af_reponse {
	ISP_AF_RESPONSE_PREVIEW = 0,
	ISP_AF_RESPONSE_MOVIE
};

enum isp_af_sleep {
	ISP_AF_SLEEP_OFF		= 0,
	ISP_AF_SLEEP_ON			= 1
};

enum isp_af_continuous {
	ISP_AF_CONTINUOUS_DISABLE	= 0,
	ISP_AF_CONTINUOUS_ENABLE	= 1
};

enum isp_af_error {
	ISP_AF_ERROR_NO			= 0, /* AF mode change is done */
	ISP_AF_EROOR_NO_LOCK_DONE	= 1  /* AF lock is done */
};

/* -------------------------  Flash  ------------------------------------- */
enum isp_flash_command {
	ISP_FLASH_COMMAND_DISABLE	= 0,
	ISP_FLASH_COMMAND_MANUALON	= 1, /* (forced flash) */
	ISP_FLASH_COMMAND_AUTO		= 2,
	ISP_FLASH_COMMAND_TORCH		= 3,   /* 3 sec */
	ISP_FLASH_COMMAND_FLASH_ON	= 4,
	ISP_FLASH_COMMAND_CAPTURE	= 5,
	ISP_FLASH_COMMAND_TRIGGER	= 6,
	ISP_FLASH_COMMAND_CALIBRATION	= 7,
	ISP_FLASH_COMMAND_START		= 8,
	ISP_FLASH_COMMAND_CANCLE	= 9
};

enum isp_flash_redeye {
	ISP_FLASH_REDEYE_DISABLE	= 0,
	ISP_FLASH_REDEYE_ENABLE		= 1
};

enum isp_flash_error {
	ISP_FLASH_ERROR_NO		= 0 /* Flash setting is done */
};

/* --------------------------  AWB  ------------------------------------ */
enum isp_awb_command {
	ISP_AWB_COMMAND_AUTO		= 0,
	ISP_AWB_COMMAND_ILLUMINATION	= 1,
	ISP_AWB_COMMAND_MANUAL	= 2
};

enum isp_awb_illumination {
	ISP_AWB_ILLUMINATION_DAYLIGHT		= 0,
	ISP_AWB_ILLUMINATION_CLOUDY		= 1,
	ISP_AWB_ILLUMINATION_TUNGSTEN		= 2,
	ISP_AWB_ILLUMINATION_FLUORESCENT	= 3
};

enum isp_awb_error {
	ISP_AWB_ERROR_NO		= 0 /* AWB setting is done */
};

/* --------------------------  Effect  ----------------------------------- */
enum isp_imageeffect_command {
	ISP_IMAGE_EFFECT_DISABLE		= 0,
	ISP_IMAGE_EFFECT_MONOCHROME		= 1,
	ISP_IMAGE_EFFECT_NEGATIVE_MONO		= 2,
	ISP_IMAGE_EFFECT_NEGATIVE_COLOR		= 3,
	ISP_IMAGE_EFFECT_SEPIA			= 4,
	ISP_IMAGE_EFFECT_AQUA			= 5,
	ISP_IMAGE_EFFECT_EMBOSS			= 6,
	ISP_IMAGE_EFFECT_EMBOSS_MONO		= 7,
	ISP_IMAGE_EFFECT_SKETCH			= 8,
	ISP_IMAGE_EFFECT_RED_YELLOW_POINT	= 9,
	ISP_IMAGE_EFFECT_GREEN_POINT		= 10,
	ISP_IMAGE_EFFECT_BLUE_POINT		= 11,
	ISP_IMAGE_EFFECT_MAGENTA_POINT		= 12,
	ISP_IMAGE_EFFECT_WARM_VINTAGE		= 13,
	ISP_IMAGE_EFFECT_COLD_VINTAGE		= 14,
	ISP_IMAGE_EFFECT_POSTERIZE		= 15,
	ISP_IMAGE_EFFECT_SOLARIZE		= 16,
	ISP_IMAGE_EFFECT_WASHED			= 17,
	ISP_IMAGE_EFFECT_CCM			= 18,
};

enum isp_imageeffect_error {
	ISP_IMAGE_EFFECT_ERROR_NO	= 0 /* Image effect setting is done */
};

/* ---------------------------  ISO  ------------------------------------ */
enum isp_iso_command {
	ISP_ISO_COMMAND_AUTO		= 0,
	ISP_ISO_COMMAND_MANUAL		= 1
};

enum iso_error {
	ISP_ISO_ERROR_NO		= 0 /* ISO setting is done */
};

/* --------------------------  Adjust  ----------------------------------- */
enum iso_adjust_command {
	ISP_ADJUST_COMMAND_AUTO			= 0,
	ISP_ADJUST_COMMAND_MANUAL_CONTRAST	= (1 << 0),
	ISP_ADJUST_COMMAND_MANUAL_SATURATION	= (1 << 1),
	ISP_ADJUST_COMMAND_MANUAL_SHARPNESS	= (1 << 2),
	ISP_ADJUST_COMMAND_MANUAL_EXPOSURE	= (1 << 3),
	ISP_ADJUST_COMMAND_MANUAL_BRIGHTNESS	= (1 << 4),
	ISP_ADJUST_COMMAND_MANUAL_HUE		= (1 << 5),
	ISP_ADJUST_COMMAND_MANUAL_HOTPIXEL	= (1 << 6),
	ISP_ADJUST_COMMAND_MANUAL_NOISEREDUCTION = (1 << 7),
	ISP_ADJUST_COMMAND_MANUAL_SHADING	= (1 << 8),
	ISP_ADJUST_COMMAND_MANUAL_GAMMA		= (1 << 9),
	ISP_ADJUST_COMMAND_MANUAL_EDGEENHANCEMENT = (1 << 10),
	ISP_ADJUST_COMMAND_MANUAL_SCENE		= (1 << 11),
	ISP_ADJUST_COMMAND_MANUAL_FRAMETIME	= (1 << 12),
	ISP_ADJUST_COMMAND_MANUAL_ALL		= 0x1FFF
};

enum isp_adjust_scene_index {
	ISP_ADJUST_SCENE_NORMAL			= 0,
	ISP_ADJUST_SCENE_NIGHT_PREVIEW		= 1,
	ISP_ADJUST_SCENE_NIGHT_CAPTURE		= 2
};


enum isp_adjust_error {
	ISP_ADJUST_ERROR_NO		= 0 /* Adjust setting is done */
};

/* -------------------------  Metering  ---------------------------------- */
enum isp_metering_command {
	ISP_METERING_COMMAND_AVERAGE		= 0,
	ISP_METERING_COMMAND_SPOT		= 1,
	ISP_METERING_COMMAND_MATRIX		= 2,
	ISP_METERING_COMMAND_CENTER		= 3,
	ISP_METERING_COMMAND_EXPOSURE_MODE	= (1 << 8)
};

enum isp_exposure_mode {
	ISP_EXPOSUREMODE_OFF		= 1,
	ISP_EXPOSUREMODE_AUTO		= 2
};

enum isp_metering_error {
	ISP_METERING_ERROR_NO	= 0 /* Metering setting is done */
};

/* --------------------------  AFC  ----------------------------------- */
enum isp_afc_command {
	ISP_AFC_COMMAND_DISABLE		= 0,
	ISP_AFC_COMMAND_AUTO		= 1,
	ISP_AFC_COMMAND_MANUAL		= 2
};

enum isp_afc_manual {
	ISP_AFC_MANUAL_50HZ		= 50,
	ISP_AFC_MANUAL_60HZ		= 60
};

enum isp_afc_error {
	ISP_AFC_ERROR_NO	= 0 /* AFC setting is done */
};

enum isp_scene_command {
	ISP_SCENE_NONE		= 0,
	ISP_SCENE_PORTRAIT	= 1,
	ISP_SCENE_LANDSCAPE     = 2,
	ISP_SCENE_SPORTS        = 3,
	ISP_SCENE_PARTYINDOOR	= 4,
	ISP_SCENE_BEACHSNOW	= 5,
	ISP_SCENE_SUNSET	= 6,
	ISP_SCENE_DAWN		= 7,
	ISP_SCENE_FALL		= 8,
	ISP_SCENE_NIGHT		= 9,
	ISP_SCENE_AGAINSTLIGHTWLIGHT	= 10,
	ISP_SCENE_AGAINSTLIGHTWOLIGHT	= 11,
	ISP_SCENE_FIRE			= 12,
	ISP_SCENE_TEXT			= 13,
	ISP_SCENE_CANDLE		= 14
};

enum ISP_BDSCommandEnum {
	ISP_BDS_COMMAND_DISABLE		= 0,
	ISP_BDS_COMMAND_ENABLE		= 1
};

/* --------------------------  Scaler  --------------------------------- */
enum scaler_imageeffect_command {
	SCALER_IMAGE_EFFECT_COMMNAD_DISABLE	= 0,
	SCALER_IMAGE_EFFECT_COMMNAD_SEPIA_CB	= 1,
	SCALER_IMAGE_EFFECT_COMMAND_SEPIA_CR	= 2,
	SCALER_IMAGE_EFFECT_COMMAND_NEGATIVE	= 3,
	SCALER_IMAGE_EFFECT_COMMAND_ARTFREEZE	= 4,
	SCALER_IMAGE_EFFECT_COMMAND_EMBOSSING	= 5,
	SCALER_IMAGE_EFFECT_COMMAND_SILHOUETTE	= 6
};

enum scaler_imageeffect_error {
	SCALER_IMAGE_EFFECT_ERROR_NO		= 0
};

enum scaler_crop_command {
	SCALER_CROP_COMMAND_DISABLE		= 0,
	SCALER_CROP_COMMAND_ENABLE		= 1
};

enum scaler_crop_error {
	SCALER_CROP_ERROR_NO			= 0 /* crop setting is done */
};

enum scaler_scaling_command {
	SCALER_SCALING_COMMNAD_DISABLE		= 0,
	SCALER_SCALING_COMMAND_UP		= 1,
	SCALER_SCALING_COMMAND_DOWN		= 2
};

enum scaler_scaling_error {
	SCALER_SCALING_ERROR_NO			= 0
};

enum scaler_rotation_command {
	SCALER_ROTATION_COMMAND_DISABLE		= 0,
	SCALER_ROTATION_COMMAND_CLOCKWISE90	= 1
};

enum scaler_rotation_error {
	SCALER_ROTATION_ERROR_NO		= 0
};

enum scaler_flip_command {
	SCALER_FLIP_COMMAND_NORMAL		= 0,
	SCALER_FLIP_COMMAND_X_MIRROR		= 1,
	SCALER_FLIP_COMMAND_Y_MIRROR		= 2,
	SCALER_FLIP_COMMAND_XY_MIRROR		= 3 /* (180 rotation) */
};

enum scaler_flip_error {
	SCALER_FLIP_ERROR_NO			= 0 /* flip setting is done */
};

enum scaler_dma_out_sel {
	SCALER_DMA_OUT_IMAGE_EFFECT		= 0,
	SCALER_DMA_OUT_SCALED			= 1,
	SCALER_DMA_OUT_UNSCALED			= 2
};

enum scaler_output_yuv_range {
	SCALER_OUTPUT_YUV_RANGE_FULL = 0,
	SCALER_OUTPUT_YUV_RANGE_NARROW = 1,
};

/* --------------------------  MCSCALER  ----------------------------------- */
enum mcsc_cmd {
	MCSC_OUT_CROP				= 0,
	MCSC_CROP_TYPE				= 1,
};

/* --------------------------  3DNR  ----------------------------------- */
enum tdnr_1st_frame_command {
	TDNR_1ST_FRAME_COMMAND_NOPROCESSING	= 0,
	TDNR_1ST_FRAME_COMMAND_2DNR		= 1
};

enum tdnr_1st_frame_error {
	TDNR_1ST_FRAME_ERROR_NO			= 0
		/*1st frame setting is done*/
};

enum tnr_processing_mode {
	TNR_PROCESSING_INVALID = -1,

	/* 2 New enum (0, 2, 5, 6, 7 is the same meaning with old enum) */
	TNR_PROCESSING_PREVIEW_POST_ON = 0,
	TNR_PROCESSING_PREVIEW_POST_ON_0_MOTION = 1,	/* ME group X */
	TNR_PROCESSING_PREVIEW_POST_OFF = 2,
	TNR_PROCESSING_CAPTURE_FIRST_POST_ON = 4,	/* for IDCG */
	TNR_PROCESSING_CAPTURE_FIRST = 5,
	TNR_PROCESSING_CAPTURE_MID = 6,
	TNR_PROCESSING_CAPTURE_LAST = 7,
	TNR_PROCESSING_CAPTURE_FIRST_AND_LAST = 8,	/* Use when num of merged frames is 2 */
	TNR_PROCESSING_TNR_BYPASS = 9,			/* DNS RDMA in (not used) */
	TNR_PROCESSING_TNR_OFF = 10,			/* single capture, MCFP RDMA input */
};

#define CHK_VIDEOHDR_MODE_CHANGE(curr_tnr_mode, next_tnr_mode)		\
	(((curr_tnr_mode == TNR_PROCESSING_PREVIEW_POST_OFF &&		\
		next_tnr_mode == TNR_PROCESSING_PREVIEW_POST_ON)	\
	|| (curr_tnr_mode == TNR_PROCESSING_PREVIEW_POST_ON &&		\
		next_tnr_mode == TNR_PROCESSING_PREVIEW_POST_OFF)) ? 1 : 0)

#define CHK_VIDEOTNR_MODE(tnr_mode)					\
	(((tnr_mode == TNR_PROCESSING_PREVIEW_POST_ON	||	\
		tnr_mode == TNR_PROCESSING_PREVIEW_POST_ON_0_MOTION	||	\
		tnr_mode == TNR_PROCESSING_PREVIEW_POST_OFF)) ? 1 : 0)

#define CHK_MFSTILL_MODE(tnr_mode)					\
	(((tnr_mode == TNR_PROCESSING_CAPTURE_FIRST_POST_ON	||	\
		tnr_mode == TNR_PROCESSING_CAPTURE_FIRST	||	\
		tnr_mode == TNR_PROCESSING_CAPTURE_MID		||	\
		tnr_mode == TNR_PROCESSING_CAPTURE_LAST		||	\
		tnr_mode == TNR_PROCESSING_CAPTURE_FIRST_AND_LAST)) ? 1 : 0)


/* ----------------------------  FD  ------------------------------------- */
enum fd_config_command {
	FD_CONFIG_COMMAND_MAXIMUM_NUMBER	= 0x1,
	FD_CONFIG_COMMAND_ROLL_ANGLE		= 0x2,
	FD_CONFIG_COMMAND_YAW_ANGLE		= 0x4,
	FD_CONFIG_COMMAND_SMILE_MODE		= 0x8,
	FD_CONFIG_COMMAND_BLINK_MODE		= 0x10,
	FD_CONFIG_COMMAND_EYES_DETECT		= 0x20,
	FD_CONFIG_COMMAND_MOUTH_DETECT		= 0x40,
	FD_CONFIG_COMMAND_ORIENTATION		= 0x80,
	FD_CONFIG_COMMAND_ORIENTATION_VALUE	= 0x100
};

enum fd_config_mode {
	FD_CONFIG_MODE_NORMAL		= 0,
	FD_CONFIG_MODE_HWONLY		= 1
};

enum fd_config_roll_angle {
	FD_CONFIG_ROLL_ANGLE_BASIC		= 0,
	FD_CONFIG_ROLL_ANGLE_PRECISE_BASIC	= 1,
	FD_CONFIG_ROLL_ANGLE_SIDES		= 2,
	FD_CONFIG_ROLL_ANGLE_PRECISE_SIDES	= 3,
	FD_CONFIG_ROLL_ANGLE_FULL		= 4,
	FD_CONFIG_ROLL_ANGLE_PRECISE_FULL	= 5,
};

enum fd_config_yaw_angle {
	FD_CONFIG_YAW_ANGLE_0			= 0,
	FD_CONFIG_YAW_ANGLE_45			= 1,
	FD_CONFIG_YAW_ANGLE_90			= 2,
	FD_CONFIG_YAW_ANGLE_45_90		= 3,
};

enum fd_config_smile_mode {
	FD_CONFIG_SMILE_MODE_DISABLE		= 0,
	FD_CONFIG_SMILE_MODE_ENABLE		= 1
};

enum fd_config_blink_mode {
	FD_CONFIG_BLINK_MODE_DISABLE		= 0,
	FD_CONFIG_BLINK_MODE_ENABLE		= 1
};

enum fd_config_eye_result {
	FD_CONFIG_EYES_DETECT_DISABLE		= 0,
	FD_CONFIG_EYES_DETECT_ENABLE		= 1
};

enum fd_config_mouth_result {
	FD_CONFIG_MOUTH_DETECT_DISABLE		= 0,
	FD_CONFIG_MOUTH_DETECT_ENABLE		= 1
};

enum fd_config_orientation {
	FD_CONFIG_ORIENTATION_DISABLE		= 0,
	FD_CONFIG_ORIENTATION_ENABLE		= 1
};
#endif
