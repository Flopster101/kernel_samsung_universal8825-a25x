/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos9630 pablo parameter definitions
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_PARAMS_H
#define IS_PARAMS_H

#include <linux/bitmap.h>
#include "is-common-enum.h"

#define PARAMETER_MAX_SIZE    128  /* in byte */
#define PARAMETER_MAX_MEMBER  (PARAMETER_MAX_SIZE/4)

#define INC_BIT(bit)			(bit<<1)
#define INC_NUM(bit)			(bit + 1)

#define MCSC_OUTPUT_DS		MCSC_OUTPUT5
#define MCSC_INPUT_HF		MCSC_OUTPUT5

enum is_param {
	PARAM_GLOBAL_SHOTMODE = 0,
	PARAM_SENSOR_CONTROL,
	PARAM_SENSOR_OTF_INPUT,
	PARAM_SENSOR_OTF_OUTPUT,
	PARAM_SENSOR_CONFIG,
	PARAM_SENSOR_DMA_OUTPUT,
	PARAM_TAALG_AE_CONFIG,
	PARAM_TAALG_RESERVED1,
	PARAM_TAALG_RESERVED2,
	PARAM_3AA_CONTROL,
	PARAM_3AA_OTF_INPUT,
	PARAM_3AA_VDMA1_INPUT,
	PARAM_3AA_DDMA_INPUT,
	PARAM_3AA_OTF_OUTPUT,
	PARAM_3AA_VDMA4_OUTPUT,
	PARAM_3AA_VDMA2_OUTPUT,
	PARAM_3AA_FDDMA_OUTPUT,
	PARAM_3AA_MRGDMA_OUTPUT,
	PARAM_3AA_DDMA_OUTPUT,
	PARAM_3AA_STRIPE_INPUT,
	PARAM_ISP_CONTROL,
	PARAM_ISP_OTF_INPUT,
	PARAM_ISP_VDMA1_INPUT,
	PARAM_ISP_VDMA2_INPUT,
	PARAM_ISP_VDMA3_INPUT,
	PARAM_ISP_OTF_OUTPUT,
	PARAM_ISP_VDMA4_OUTPUT,
	PARAM_ISP_VDMA5_OUTPUT,
	PARAM_ISP_VDMA6_OUTPUT,
	PARAM_ISP_VDMA7_OUTPUT,
	PARAM_ISP_STRIPE_INPUT,
	PARAM_TPU_CONTROL,
	PARAM_TPU_CONFIG,
	PARAM_TPU_OTF_INPUT,
	PARAM_TPU_DMA_INPUT,
	PARAM_TPU_OTF_OUTPUT,
	PARAM_TPU_DMA_OUTPUT,
	PARAM_MCS_CONTROL,
	PARAM_MCS_INPUT,
	PARAM_MCS_OUTPUT0,
	PARAM_MCS_OUTPUT1,
	PARAM_MCS_OUTPUT2,
	PARAM_MCS_OUTPUT3,
	PARAM_MCS_OUTPUT4,
	PARAM_MCS_OUTPUT5,
	PARAM_MCS_STRIPE_INPUT,
	PARAM_FD_CONTROL,
	PARAM_FD_OTF_INPUT,
	PARAM_FD_DMA_INPUT,
	PARAM_FD_CONFIG,
	PARAM_PAF_CONTROL,
	PARAM_PAF_DMA_INPUT,
	PARAM_PAF_OTF_OUTPUT,
	PARAM_CLH_CONTROL,
	PARAM_CLH_DMA_INPUT,
	PARAM_CLH_DMA_OUTPUT,
	IS_PARAM_NUM
};

#define IS_DECLARE_PMAP(name)  DECLARE_BITMAP(name, IS_PARAM_NUM)
#define IS_INIT_PMAP(name)     bitmap_zero(name, IS_PARAM_NUM)
#define IS_FILL_PMAP(name)     bitmap_fill(name, IS_PARAM_NUM)
#define IS_COPY_PMAP(dst, src) bitmap_copy(dst, src, IS_PARAM_NUM)
#define IS_AND_PMAP(dst, src)  bitmap_and(dst, dst, src, IS_PARAM_NUM)
#define IS_OR_PMAP(dst, src)   bitmap_or(dst, dst, src, IS_PARAM_NUM)
#define IS_EMPTY_PMAP(name)    bitmap_empty(name, IS_PARAM_NUM)

enum mcsc_output_index {
	MCSC_OUTPUT0				= 0,
	MCSC_OUTPUT1				= 1,
	MCSC_OUTPUT2				= 2,
	MCSC_OUTPUT3				= 3,
	MCSC_OUTPUT4				= 4,
	MCSC_OUTPUT5				= 5,
	MCSC_OUTPUT_MAX
};

struct param_global_shotmode {
	u32	cmd;
	u32	reserved[PARAMETER_MAX_MEMBER-2];
	u32	err;
};

struct param_control {
	u32	cmd;
	u32	bypass;
	u32	buffer_address;
	u32	buffer_number;
	u32	lut_index;
	u32	reserved[PARAMETER_MAX_MEMBER-6];
	u32	err;
};

struct param_otf_input {
	u32	cmd;
	u32	format;
	u32	bitwidth;
	u32	order;
	u32	width; /* with margin */
	u32	height; /* with margine */
	u32	bayer_crop_offset_x;
	u32	bayer_crop_offset_y;
	u32	bayer_crop_width; /* BCrop1 output width without considering ISP margin = BDS input width */
	u32	bayer_crop_height;  /* BCrop1 output height without considering ISP margin = BDS input height */
	u32	source;
	u32	reserved[PARAMETER_MAX_MEMBER-12];
	u32	err;
};

struct param_dma_input {
	u32	cmd;
	u32	format;
	u32	bitwidth;
	u32	order;
	u32	plane;
	u32	width;
	u32	height;
	u32	dma_crop_offset; /* uiDmaCropOffset[31:16] : X offset, uiDmaCropOffset[15:0] : Y offset */
	u32	dma_crop_width;
	u32	dma_crop_height;
	u32	bayer_crop_offset_x;
	u32	bayer_crop_offset_y;
	u32	bayer_crop_width;
	u32	bayer_crop_height;
	u32	scene_mode; /* for AE envelop */
	u32	msb; /* last bit of data in memory size */
	u32	stride_plane0;
	u32	stride_plane1;
	u32	stride_plane2;
	u32	v_otf_enable;
	u32	v_otf_token_line;
	u32	orientation;
	u32	strip_mode;
	u32	overlab_width;
	u32	strip_count;
	u32	strip_max_count;
	u32	sequence_id;
	u32	reserved[PARAMETER_MAX_MEMBER-28];
	u32	err;
};

struct param_stripe_input {
	u32	index;		/* Stripe region index */
	u32	total_count;	/* Total stripe region num */
	u32	left_margin;	/* Overlapped left horizontal region */
	u32	right_margin;	/* Overlapped right horizontal region */
	u32	full_width;	/* Original image width */
	u32	full_height;	/* Original image height */
	u32	scaled_left_margin;	/* for scaler */
	u32	scaled_right_margin;	/* for scaler */
	u32	scaled_full_width;	/* for scaler */
	u32	scaled_full_height;	/* for scaler */
	u32	stripe_roi_start_pos_x[5];	/* Start X Position w/o Margin For STRIPE */
	u32	reserved[PARAMETER_MAX_MEMBER - 16];
	u32	error;		/* Error code */
};

struct param_otf_output {
	u32	cmd;
	u32	format;
	u32	bitwidth;
	u32	order;
	u32	width; /* BDS output width */
	u32	height; /* BDS output height */
	u32	crop_offset_x;
	u32	crop_offset_y;
	u32	crop_width;
	u32	crop_height;
	u32	crop_enable; /* 0: use crop before bds, 1: use crop after bds */
	u32	reserved[PARAMETER_MAX_MEMBER-12];
	u32	err;
};

struct param_dma_output {
	u32	cmd;
	u32	format;
	u32	bitwidth;
	u32	order;
	u32	plane;
	u32	width; /* BDS output width */
	u32	height; /* BDS output height */
	u32	dma_crop_offset_x;
	u32	dma_crop_offset_y;
	u32	dma_crop_width;
	u32	dma_crop_height;
	u32	crop_enable; /* 0: use crop before bds, 1: use crop after bds */
	u32	msb; /* last bit of data in memory size */
	u32	stride_plane0;
	u32	stride_plane1;
	u32	stride_plane2;
	u32	v_otf_enable;
	u32	v_otf_dst_ip; /* ISPLP:0, ISPHQ:1, DCP:2, CIP1:4, CIP2:3 */
	u32	v_otf_dst_axi_id;
	u32	v_otf_token_line;
	u32	reserved[PARAMETER_MAX_MEMBER-21];
	u32	err;
};

struct param_sensor_config {
	u32	frametime; /* max exposure time(us) */
	u32	min_target_fps;
	u32	max_target_fps;
	u32	width;
	u32	height;
	u32	sensor_binning_ratio_x;
	u32	sensor_binning_ratio_y;
	u32	bns_binning_ratio_x;
	u32	bns_binning_ratio_y;
	u32	bns_margin_left;
	u32	bns_margin_top;
	u32	bns_output_width; /* Active scaled image width */
	u32	bns_output_height; /* Active scaled image height */
	u32	calibrated_width; /* sensor cal size */
	u32	calibrated_height;
	u32	early_config_lock;
	u32	freeform_sensor_crop_enable;	/* freeform sensor crop, not center aligned */
	u32	freeform_sensor_crop_offset_x;
	u32	freeform_sensor_crop_offset_y;
	u32	vvalid_time;		/* Actual sensor vvalid time */
	u32	req_vvalid_time;	/* Required vvalid time for OTF input IPs */
	u32	mono_mode;	/* use mono sensor */
	u32	reserved[PARAMETER_MAX_MEMBER-23];
	u32	err;
};

struct param_isp_aa {
	u32	cmd;
	u32	target;
	u32	mode;
	u32	scene;
	u32	af_touch;
	u32	af_face;
	u32	af_response;
	u32	sleep;
	u32	touch_x;
	u32	touch_y;
	u32	manual_af_setting;
	/*0: Legacy, 1: Camera 2.0*/
	u32	cam_api_2p0;
	/* For android.control.afRegions in Camera 2.0,
	Resolution based on YUV output size*/
	u32	af_region_left;
	/* For android.control.afRegions in Camera 2.0,
	Resolution based on YUV output size*/
	u32	af_region_top;
	/* For android.control.afRegions in Camera 2.0,
	Resolution based on YUV output size*/
	u32	af_region_right;
	/* For android.control.afRegions in Camera 2.0,
	Resolution based on YUV output size*/
	u32	af_region_bottom;
	u32	reserved[PARAMETER_MAX_MEMBER-17];
	u32	err;
};

struct param_isp_flash {
	u32	cmd;
	u32	redeye;
	u32	flashintensity;
	u32	reserved[PARAMETER_MAX_MEMBER-4];
	u32	err;
};

struct param_isp_awb {
	u32	cmd;
	u32	illumination;
	u32	reserved[PARAMETER_MAX_MEMBER-3];
	u32	err;
};

struct param_isp_imageeffect {
	u32	cmd;
	u32	reserved[PARAMETER_MAX_MEMBER-2];
	u32	err;
};

struct param_isp_iso {
	u32	cmd;
	u32	value;
	u32	reserved[PARAMETER_MAX_MEMBER-3];
	u32	err;
};

struct param_isp_adjust {
	u32	cmd;
	s32	contrast;
	s32	saturation;
	s32	sharpness;
	s32	exposure;
	s32	brightness;
	s32	hue;
	/* 0 or 1 */
	u32	hot_pixel_enable;
	/* -127 ~ 127 */
	s32	noise_reduction_strength;
	/* 0 or 1 */
	u32	shading_correction_enable;
	/* 0 or 1 */
	u32	user_gamma_enable;
	/* -127 ~ 127 */
	s32	edge_enhancement_strength;
	/* ISP_AdjustSceneIndexEnum */
	u32	user_scene_mode;
	u32	min_frame_time;
	u32	max_frame_time;
	u32	reserved[PARAMETER_MAX_MEMBER-16];
	u32	err;
};

struct param_isp_metering {
	u32	cmd;
	u32	win_pos_x;
	u32	win_pos_y;
	u32	win_width;
	u32	win_height;
	u32	exposure_mode;
	/* 0: Legacy, 1: Camera 2.0 */
	u32	cam_api_2p0;
	u32	reserved[PARAMETER_MAX_MEMBER-8];
	u32	err;
};

struct param_isp_afc {
	u32	cmd;
	u32	manual;
	u32	reserved[PARAMETER_MAX_MEMBER-3];
	u32	err;
};

struct param_scaler_imageeffect {
	u32	cmd;
	u32	arbitrary_cb;
	u32	arbitrary_cr;
	u32	yuv_range;
	u32	reserved[PARAMETER_MAX_MEMBER-5];
	u32	err;
};

struct param_capture_intent {
	u32	cmd;
	u32	reserved[PARAMETER_MAX_MEMBER-2];
	u32	err;
};

struct param_scaler_input_crop {
	u32  cmd;
	u32  pos_x;
	u32  pos_y;
	u32  crop_width;
	u32  crop_height;
	u32  in_width;
	u32  in_height;
	u32  out_width;
	u32  out_height;
	u32  reserved[PARAMETER_MAX_MEMBER-10];
	u32  err;
};

struct param_scaler_output_crop {
	u32  cmd;
	u32  pos_x;
	u32  pos_y;
	u32  crop_width;
	u32  crop_height;
	u32  format;
	u32  reserved[PARAMETER_MAX_MEMBER-7];
	u32  err;
};

struct param_scaler_rotation {
	u32	cmd;
	u32	reserved[PARAMETER_MAX_MEMBER-2];
	u32	err;
};

struct param_scaler_flip {
	u32	cmd;
	u32	reserved[PARAMETER_MAX_MEMBER-2];
	u32	err;
};

struct param_3dnr_1stframe {
	u32	cmd;
	u32	reserved[PARAMETER_MAX_MEMBER-2];
	u32	err;
};

struct param_fd_config {
	u32	cmd;
	u32	mode;
	u32	max_number;
	u32	roll_angle;
	u32	yaw_angle;
	s32	smile_mode;
	s32	blink_mode;
	u32	eye_detect;
	u32	mouth_detect;
	u32	orientation;
	u32	orientation_value;
	u32	map_width;
	u32	map_height;
	u32	reserved[PARAMETER_MAX_MEMBER-14];
	u32	err;
};

struct param_tpu_config {
	u32	odc_bypass;
	u32	dis_bypass;
	u32	tdnr_bypass;
	u32	reserved[PARAMETER_MAX_MEMBER-4];
	u32	err;
};

struct param_dcp_config {
	u32	reserved[PARAMETER_MAX_MEMBER-1];
	u32	err;
};

struct global_param {
	struct param_global_shotmode	shotmode; /* 0 */
};

/* To be added */
struct sensor_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_otf_output		otf_output;
	struct param_sensor_config	config;
	struct param_dma_output		dma_output;
};

struct aeconfig_param {
	u32 uiCameraEntrance;
	u32 uiReserved[PARAMETER_MAX_MEMBER-2];
	u32 uiError;
};

struct taalg_param{
    struct aeconfig_param        ParameterAeConfig;
    struct aeconfig_param        ParameterReserved1;
    struct aeconfig_param        ParameterReserved2;
};

struct taa_param {
	struct param_control		control;
	struct param_otf_input		otf_input;	/* otf_input */
	struct param_dma_input		vdma1_input;	/* dma1_input */
	struct param_dma_input		ddma_input;	/* not use */
	struct param_otf_output		otf_output;	/* not use */
	struct param_dma_output		vdma4_output;	/* Before BDS */
	struct param_dma_output		vdma2_output;	/* After BDS */
	struct param_dma_output		efd_output;	/* Early FD */
	struct param_dma_output		mrg_output;	/* mrg_out*/
	struct param_dma_output		ddma_output;	/* not use */
	struct param_stripe_input	stripe_input;
};

struct isp_param {
	struct param_control		control;
	struct param_otf_input		otf_input;	/* not use */
	struct param_dma_input		vdma1_input;	/* dma1_input */
	struct param_dma_input		vdma2_input;	/* tnr weight input */
	struct param_dma_input		vdma3_input;	/* tnr prev img input */
	struct param_otf_output		otf_output;	/* otf_out */
	struct param_dma_output		vdma4_output;	/* ITP image output */
	struct param_dma_output		vdma5_output;
	struct param_dma_output		vdma6_output;	/* TNR prev image output */
	struct param_dma_output		vdma7_output;	/* TNR prev weight output */
	struct param_stripe_input	stripe_input;
};

struct drc_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;
	struct param_otf_output		otf_output;
};

struct tpu_param {
	struct param_control		control;
	struct param_tpu_config		config;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;
	struct param_otf_output		otf_output;
	struct param_dma_output		dma_output;
};

struct dis_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_otf_output		otf_output;
};

struct paf_rdma_param {
	struct param_control            control;
	struct param_dma_input          dma_input;
	struct param_otf_output         otf_output;
};

struct param_mcs_input {
	u32	otf_cmd; /* DISABLE or ENABLE */
	u32	otf_format;
	u32	otf_bitwidth;
	u32	otf_order;
	u32	dma_cmd; /* DISABLE or ENABLE */
	u32	dma_format;
	u32	dma_bitwidth;
	u32	dma_order;
	u32	plane;
	u32	width;
	u32	height;
	u32	dma_stride_y;
	u32	dma_stride_c;
	u32	dma_crop_offset_x;
	u32	dma_crop_offset_y;
	u32	dma_crop_width;
	u32	dma_crop_height;
	u32	djag_out_width;
	u32	djag_out_height;
	u32	reserved[PARAMETER_MAX_MEMBER-20];
	u32	err;
};

struct param_mcs_output {
	u32	otf_cmd; /* DISABLE or ENABLE */
	u32	otf_format;
	u32	otf_bitwidth;
	u32	otf_order;
	u32	dma_cmd; /* DISABLE or ENABLE */
	u32	dma_format;
	u32	dma_bitwidth;
	u32	dma_order;
	u32	plane;
	u32	crop_offset_x;
	u32	crop_offset_y;
	u32	crop_width;
	u32	crop_height;
	u32	width;
	u32	height;
	u32	dma_stride_y;
	u32	dma_stride_c;
	u32	yuv_range; /* FULL or NARROW */
	u32	flip; /* NORMAL or X-MIRROR or Y- MIRROR or XY- MIRROR */
	u32	hwfc; /* DISABLE or ENABLE */
	u32	offset_x;
	u32	offset_y;
	/*
	 * cmd
	 * [BIT 0]: output crop(0: disable, 1: enable)
	 * [BIT 1]: crop type(0: center, 1: freeform)
	 */
	u32	cmd;
	u32	bitsperpixel;				/* bitsperpixel[23:16]: plane2, [15:8]: plane1,  [7:0]: plane0 */
	u32	reserved[PARAMETER_MAX_MEMBER-25];
	u32	err;
};

struct mcs_param {
	struct param_control			control;
	struct param_mcs_input			input;
	struct param_mcs_output			output[MCSC_OUTPUT_MAX];
	struct param_stripe_input		stripe_input;
};

struct scp_param {
	struct param_control			control;
	struct param_otf_input			otf_input;
	struct param_scaler_imageeffect		effect;
	struct param_scaler_input_crop		input_crop;
	struct param_scaler_output_crop		output_crop;
	struct param_scaler_rotation		rotation;
	struct param_scaler_flip		flip;
	struct param_otf_output			otf_output;
	struct param_dma_output			dma_output;
};

struct vra_param {
	struct param_control			control;
	struct param_otf_input			otf_input;
	struct param_dma_input			dma_input;
	struct param_fd_config			config;
};

struct clh_param {
	struct param_control			control;
	struct param_dma_input			dma_input;
	struct param_dma_output			dma_output;
};

struct is_param_region {
	struct global_param		global;
	struct sensor_param		sensor;
	struct taalg_param		TaAlg;
	struct taa_param		taa;
	struct isp_param		isp;
	struct tpu_param		tpu;
	struct mcs_param		mcs;
	struct vra_param		vra;
	struct paf_rdma_param           paf;
	struct clh_param		clh;
};

struct is_fd_rect {
	u32 offset_x;
	u32 offset_y;
	u32 width;
	u32 height;
};

#define MAX_FACE_COUNT		16

struct is_fdae_info {
	u32			id[MAX_FACE_COUNT];
	u32			score[MAX_FACE_COUNT];
	struct is_fd_rect	rect[MAX_FACE_COUNT];
	u32			is_rot[MAX_FACE_COUNT];
	u32			rot[MAX_FACE_COUNT];
	u32			face_num;
	u32			frame_count;
	spinlock_t		slock;
};

struct fd_rect {
	float	offset_x;
	float	offset_y;
	float	width;
	float	height;
};

struct nfd_info {
	int		face_num;
	u32		id[MAX_FACE_COUNT];
	struct fd_rect	rect[MAX_FACE_COUNT];
	float		score[MAX_FACE_COUNT];
	u32		frame_count;
	float		rot[MAX_FACE_COUNT];    /* for NFD v3.0 */
	float		yaw[MAX_FACE_COUNT];
	float		pitch[MAX_FACE_COUNT];
	u32		width;			/* Original width size */
	u32		height;			/* Original height size */
	u32		crop_x;			/* crop image offset x */
	u32		crop_y;			/* crop image offset y */
	u32		crop_w;			/* crop width size */
	u32		crop_h;			/* crop height size */
};

struct is_region {
	struct is_param_region	parameter;
	struct is_fdae_info	fdae_info;
	struct nfd_info		fd_info;
	spinlock_t		fd_info_slock;
};

#if defined(CONVERT_BUFFER_SECURE_TO_NON_SECURE)
struct secure_buffer_convert_info {
	int secure_buffer_fd;
	int secure_buffer_size;
	int non_secure_buffer_fd;
	int non_secure_buffer_size;
};
#endif

/*
 * =================================================================================================
 * DDK interface
 * =================================================================================================
 */
struct taa_param_set {
	struct param_sensor_config	sensor_config;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;
#if !IS_ENABLED(SOC_ORB0C)
	struct param_dma_input		ddma_input;     /* deprecated */
#endif
	struct param_otf_output		otf_output;
	struct param_dma_output		dma_output_before_bds;
	struct param_dma_output		dma_output_after_bds;
	struct param_dma_output		dma_output_mrg;
	struct param_dma_output		dma_output_efd;
#if defined(ENABLE_ORBDS)
	struct param_dma_output		dma_output_orbds;
#endif
#if defined(ENABLE_LMEDS)
	struct param_dma_output		dma_output_lmeds;
#endif
#if !IS_ENABLED(SOC_ORB0C)
	struct param_dma_output		ddma_output;    /* deprecated */
#endif
#if IS_ENABLED(USE_HF_INTERFACE)
	struct param_dma_output		dma_output_hf;
#endif
#if IS_ENABLED(LOGICAL_VIDEO_NODE)
	struct param_dma_output		dma_output_drcgrid;
#endif
	u32				input_dva[IS_MAX_PLANES];
	u32				output_dva_before_bds[IS_MAX_PLANES];
	u32				output_dva_after_bds[IS_MAX_PLANES];
	u32				output_dva_mrg[IS_MAX_PLANES];
	u32				output_dva_efd[IS_MAX_PLANES];
#if defined(ENABLE_ORBDS)
	u32				output_dva_orbds[IS_MAX_PLANES];
#endif
#if defined(ENABLE_LMEDS)
	u32				output_dva_lmeds[IS_MAX_PLANES];
#endif
#if IS_ENABLED(USE_HF_INTERFACE)
	u32				output_dva_hf[IS_MAX_PLANES];
#endif
#if IS_ENABLED(LOGICAL_VIDEO_NODE)
	u32				output_dva_drcgrid[IS_MAX_PLANES];
#endif
	uint64_t			output_kva_me[IS_MAX_PLANES];	/* ME or MCH */
#if IS_ENABLED(SOC_ORB0C)
	uint64_t			output_kva_orb[IS_MAX_PLANES];
#endif
	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
#ifdef CHAIN_STRIPE_PROCESSING
	struct param_stripe_input	stripe_input;
#endif
};

struct isp_param_set {
	struct taa_param_set		*taa_param;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input; /* VDMA1 */
	struct param_dma_input		prev_dma_input;	/* VDMA2, for TNR merger */
	struct param_dma_input		prev_wgt_dma_input;	/* VDMA3, for TNR merger */
#if IS_ENABLED(USE_MCFP_MOTION_INTERFACE)
	struct param_dma_input		motion_dma_input;
#endif
#if defined(ENABLE_RGB_REPROCESSING)
	struct param_dma_input		dma_input_rgb;
	struct param_dma_input		dma_input_noise;
#endif
#if defined(ENABLE_SC_MAP)
	struct param_dma_input		dma_input_scmap;
#endif
#if IS_ENABLED(USE_MCFP_MOTION_INTERFACE)
	struct param_dma_input		dma_input_drcgrid;
#endif
	struct param_otf_output		otf_output;
#if IS_ENABLED(USE_YPP_INTERFACE)
	struct param_dma_output		dma_output_yuv; /* VDMA4 */
	struct param_dma_output		dma_output_rgb; /* VDMA5 */
#else
	struct param_dma_output		dma_output_chunk;
	struct param_dma_output		dma_output_yuv;
#endif
	struct param_dma_output		dma_output_tnr_wgt; /* VDMA6 */
	struct param_dma_output		dma_output_tnr_prev; /* VDMA7 */
#if IS_ENABLED(USE_YPP_INTERFACE)
	struct param_dma_output		dma_output_nrds;
	struct param_dma_output		dma_output_noise;
	struct param_dma_output		dma_output_hf;
	struct param_dma_output		dma_output_drc;
	struct param_dma_output		dma_output_hist;
#endif
#if defined(ENABLE_RGB_REPROCESSING)
	struct param_dma_output		dma_output_noise_rep;
#endif

	u32				input_dva[IS_MAX_PLANES];
	u32				input_dva_tnr_prev[IS_MAX_PLANES];
	u32				input_dva_tnr_wgt[IS_MAX_PLANES];
#if IS_ENABLED(USE_MCFP_MOTION_INTERFACE)
	u32				input_dva_motion[IS_MAX_PLANES];
	u64				input_kva_motion[IS_MAX_PLANES]; /* For debugging */
#endif
#if defined(ENABLE_RGB_REPROCESSING)
	u32				input_dva_rgb[IS_MAX_PLANES];
	u32				input_dva_noise[IS_MAX_PLANES];
#endif
#if defined(ENABLE_SC_MAP)
	u32				input_dva_scmap[IS_MAX_PLANES];
#endif
#if IS_ENABLED(USE_MCFP_MOTION_INTERFACE)
	u32				input_dva_drcgrid[IS_MAX_PLANES];
#endif
#if IS_ENABLED(USE_YPP_INTERFACE)
	u32				output_dva_yuv[IS_MAX_PLANES];
	u32				output_dva_rgb[IS_MAX_PLANES];
#else
	u32				output_dva_chunk[IS_MAX_PLANES];
	u32				output_dva_yuv[IS_MAX_PLANES];
#endif
	u32				output_dva_tnr_wgt[IS_MAX_PLANES];
	u32				output_dva_tnr_prev[IS_MAX_PLANES];
#if IS_ENABLED(USE_YPP_INTERFACE)
	u32				output_dva_nrds[IS_MAX_PLANES];
	u32				output_dva_noise[IS_MAX_PLANES];
	u32				output_dva_hf[IS_MAX_PLANES];
	u32				output_dva_drc[IS_MAX_PLANES];
	u32				output_dva_hist[IS_MAX_PLANES];
#endif
#if defined(ENABLE_RGB_REPROCESSING)
	u32				output_dva_noise_rep[IS_MAX_PLANES];
#endif
	uint64_t                        output_kva_ext[IS_MAX_PLANES];  /* use for EXT */

	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
	u32				tnr_mode;
#ifdef CHAIN_STRIPE_PROCESSING
	struct param_stripe_input	stripe_input;
#endif
};
#endif
