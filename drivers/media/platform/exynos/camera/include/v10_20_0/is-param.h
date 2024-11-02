// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_PARAMS_H
#define IS_PARAMS_H

#include <linux/bitmap.h>
#include "is-common-enum.h"

#define PARAMETER_MAX_SIZE	128  /* bytes */
#define PARAMETER_MAX_MEMBER	(PARAMETER_MAX_SIZE >> 2)

#define INC_BIT(bit)		(bit << 1)
#define INC_NUM(bit)		(bit + 1)

#define VOTF_TOKEN_LINE		(8)

#define MCSC_OUTPUT_DS		MCSC_OUTPUT5
#define MCSC_INPUT_HF		MCSC_OUTPUT5

#if IS_ENABLED(LOGICAL_VIDEO_NODE)
enum is_param {
	PARAM_SENSOR_CONFIG,
	PARAM_3AA_CONTROL,
	PARAM_3AA_OTF_INPUT,
	PARAM_3AA_VDMA1_INPUT,
	PARAM_3AA_OTF_OUTPUT,
	PARAM_3AA_VDMA4_OUTPUT,
	PARAM_3AA_VDMA2_OUTPUT,
	PARAM_3AA_FDDMA_OUTPUT,
	PARAM_3AA_MRGDMA_OUTPUT,
	PARAM_3AA_DRCG_OUTPUT,
	PARAM_3AA_ORBDS_OUTPUT,
	PARAM_3AA_LMEDS_OUTPUT,
	PARAM_3AA_HF_OUTPUT,
	PARAM_3AA_STRIPE_INPUT,
	PARAM_ISP_CONTROL,
	PARAM_ISP_OTF_INPUT,
	PARAM_ISP_VDMA1_INPUT,
	PARAM_ISP_VDMA2_INPUT,
	PARAM_ISP_VDMA3_INPUT,
	PARAM_ISP_MOTION_DMA_INPUT,
	PARAM_ISP_RGB_INPUT,
	PARAM_ISP_NOISE_INPUT,
	PARAM_ISP_SCMAP_INPUT,
	PARAM_ISP_DRC_INPUT,
	PARAM_ISP_OTF_OUTPUT,
	PARAM_ISP_VDMA4_OUTPUT,
	PARAM_ISP_VDMA5_OUTPUT,
	PARAM_ISP_VDMA6_OUTPUT,
	PARAM_ISP_VDMA7_OUTPUT,
	PARAM_ISP_NRDS_OUTPUT,
	PARAM_ISP_NOISE_OUTPUT,
	PARAM_ISP_DRC_OUTPUT,
	PARAM_ISP_HIST_OUTPUT,
	PARAM_ISP_NOISE_REP_OUTPUT,
	PARAM_ISP_HF_OUTPUT,
	PARAM_ISP_RGB_OUTPUT,
	PARAM_ISP_NRB_OUTPUT,
	PARAM_ISP_STRIPE_INPUT,
	PARAM_MCS_CONTROL,
	PARAM_MCS_INPUT,
	PARAM_MCS_OUTPUT0,
	PARAM_MCS_OUTPUT1,
	PARAM_MCS_OUTPUT2,
	PARAM_MCS_OUTPUT3,
	PARAM_MCS_OUTPUT4,
	PARAM_MCS_OUTPUT5,
	PARAM_MCSS_DMA_INPUT,
	PARAM_MCS_STRIPE_INPUT,
	PARAM_PAF_CONTROL,
	PARAM_PAF_DMA_INPUT,
	PARAM_YPP_CONTROL,
	PARAM_YPP_DMA_INPUT,
	PARAM_YPP_DMA_LV2_INPUT,
	PARAM_YPP_DMA_NOISE_INPUT,
	PARAM_YPP_DMA_DRC_INPUT,
	PARAM_YPP_DMA_HIST_INPUT,
	PARAM_YPP_OTF_OUTPUT,
	PARAM_YPP_STRIPE_INPUT,
	PARAM_ORB_CONTROL,
	PARAM_ORB_DMA,
	IS_PARAM_NUM
};
#else
enum is_param {
	PARAM_SENSOR_CONFIG,
	PARAM_3AA_CONTROL,
	PARAM_3AA_OTF_INPUT,
	PARAM_3AA_VDMA1_INPUT,
	PARAM_3AA_OTF_OUTPUT,
	PARAM_3AA_VDMA4_OUTPUT,
	PARAM_3AA_VDMA2_OUTPUT,
	PARAM_3AA_FDDMA_OUTPUT,
	PARAM_3AA_MRGDMA_OUTPUT,
	PARAM_3AA_HF_OUTPUT,
	PARAM_3AA_STRIPE_INPUT,
	PARAM_ISP_CONTROL,
	PARAM_ISP_OTF_INPUT,
	PARAM_ISP_VDMA1_INPUT,
	PARAM_ISP_VDMA2_INPUT,
	PARAM_ISP_VDMA3_INPUT,
	PARAM_ISP_MOTION_DMA_INPUT,
	PARAM_ISP_RGB_INPUT,
	PARAM_ISP_NOISE_INPUT,
	PARAM_ISP_SCMAP_INPUT,
	PARAM_ISP_DRC_INPUT,
	PARAM_ISP_OTF_OUTPUT,
	PARAM_ISP_VDMA4_OUTPUT,
	PARAM_ISP_VDMA5_OUTPUT,
	PARAM_ISP_VDMA6_OUTPUT,
	PARAM_ISP_VDMA7_OUTPUT,
	PARAM_ISP_NRDS_OUTPUT,
	PARAM_ISP_NOISE_OUTPUT,
	PARAM_ISP_DRC_OUTPUT,
	PARAM_ISP_HIST_OUTPUT,
	PARAM_ISP_NOISE_REP_OUTPUT,
	PARAM_ISP_HF_OUTPUT,
	PARAM_ISP_RGB_OUTPUT,
	PARAM_ISP_NRB_OUTPUT,
	PARAM_ISP_STRIPE_INPUT,
	PARAM_TPU_CONFIG,
	PARAM_MCS_CONTROL,
	PARAM_MCS_INPUT,
	PARAM_MCS_OUTPUT0,
	PARAM_MCS_OUTPUT1,
	PARAM_MCS_OUTPUT2,
	PARAM_MCS_OUTPUT3,
	PARAM_MCS_OUTPUT4,
	PARAM_MCS_OUTPUT5,
	PARAM_MCSS_DMA_INPUT,
	PARAM_MCS_STRIPE_INPUT,
	PARAM_FD_CONTROL,
	PARAM_FD_OTF_INPUT,
	PARAM_FD_DMA_INPUT,
	PARAM_FD_CONFIG,
	PARAM_PAF_CONTROL,
	PARAM_PAF_DMA_INPUT,
	PARAM_YPP_CONTROL,
	PARAM_YPP_DMA_INPUT,
	PARAM_YPP_DMA_LV2_INPUT,
	PARAM_YPP_DMA_NOISE_INPUT,
	PARAM_YPP_DMA_DRC_INPUT,
	PARAM_YPP_DMA_HIST_INPUT,
	PARAM_YPP_OTF_OUTPUT,
	PARAM_YPP_STRIPE_INPUT,
	PARAM_ORB_CONTROL,
	PARAM_ORB_DMA,
	IS_PARAM_NUM
};
#endif

#define IS_DECLARE_PMAP(name)	DECLARE_BITMAP(name, IS_PARAM_NUM)
#define IS_INIT_PMAP(name)	bitmap_zero(name, IS_PARAM_NUM)
#define IS_FILL_PMAP(name)	bitmap_fill(name, IS_PARAM_NUM)
#define IS_COPY_PMAP(dst, src)	bitmap_copy(dst, src, IS_PARAM_NUM)
#define IS_AND_PMAP(dst, src)	bitmap_and(dst, dst, src, IS_PARAM_NUM)
#define IS_OR_PMAP(dst, src)	bitmap_or(dst, dst, src, IS_PARAM_NUM)
#define IS_EMPTY_PMAP(name)	bitmap_empty(name, IS_PARAM_NUM)

enum mcsc_output_index {
	MCSC_OUTPUT0	= 0,
	MCSC_OUTPUT1	= 1,
	MCSC_OUTPUT2	= 2,
	MCSC_OUTPUT3	= 3,
	MCSC_OUTPUT4	= 4,
	MCSC_OUTPUT5	= 5,
	MCSC_OUTPUT_MAX
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Pablo parameter types                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
struct param_sensor_config {
	u32	frametime;		/* max exposure time(us) */
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
	u32	bns_output_width;	/* Active scaled image width */
	u32	bns_output_height;	/* Active scaled image height */
	u32	calibrated_width;	/* sensor cal size */
	u32	calibrated_height;
	u32	early_config_lock;
	u32	freeform_sensor_crop_enable;	/* freeform sensor crop, not center aligned */
	u32	freeform_sensor_crop_offset_x;
	u32	freeform_sensor_crop_offset_y;
	u32	pixel_order;
	u32	vvalid_time;		/* Actual sensor vvalid time */
	u32	req_vvalid_time;	/* Required vvalid time for OTF input IPs */
	u32	reserved[PARAMETER_MAX_MEMBER-23];
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
	u32	width;			/* with margin */
	u32	height;			/* with margine */
	u32	bayer_crop_offset_x;
	u32	bayer_crop_offset_y;
	u32	bayer_crop_width;	/* BCrop1 output width without considering ISP margin = BDS input width */
	u32	bayer_crop_height;	/* BCrop1 output height without considering ISP margin = BDS input height */
	u32	source;
	u32	physical_width;		/* dummy + valid image */
	u32	physical_height;	/* dummy + valid image */
	u32	offset_x;		/* BCrop0: for removing dummy region */
	u32	offset_y;		/* BCrop0: for removing dummy region */
	u32	reserved[PARAMETER_MAX_MEMBER-16];
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
	u32	dma_crop_offset;	/* uiDmaCropOffset[31:16] : X offset, uiDmaCropOffset[15:0] : Y offset */
	u32	dma_crop_width;
	u32	dma_crop_height;
	u32	bayer_crop_offset_x;
	u32	bayer_crop_offset_y;
	u32	bayer_crop_width;
	u32	bayer_crop_height;
	u32	scene_mode;		/* for AE envelop */
	u32	msb;			/* last bit of data in memory size */
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
	u32	sbwc_type;		/* COMP_OFF = 0, COMP_LOSSLESS = 1, COMP_LOSSY =2 */
	u32	reserved[PARAMETER_MAX_MEMBER-29];
	u32	err;
};

struct param_stripe_input {
	u32	index;			/* Stripe region index */
	u32	total_count;		/* Total stripe region num */
	u32	left_margin;		/* Overlapped left horizontal region */
	u32	right_margin;		/* Overlapped right horizontal region */
	u32	full_width;		/* Original image width */
	u32	full_height;		/* Original image height */
	u32	scaled_left_margin;	/* for scaler */
	u32	scaled_right_margin;	/* for scaler */
	u32	scaled_full_width;	/* for scaler */
	u32	scaled_full_height;	/* for scaler */
	u32	stripe_roi_start_pos_x[6];	/* Start X Position w/o Margin: MAX_STRIPE_REGION_NUM + 1 */
	u32	reserved[PARAMETER_MAX_MEMBER - 17];
	u32	error;			/* Error code */
};

struct param_otf_output {
	u32	cmd;
	u32	format;
	u32	bitwidth;
	u32	order;
	u32	width;			/* BDS output width */
	u32	height;			/* BDS output height */
	u32	crop_offset_x;
	u32	crop_offset_y;
	u32	crop_width;
	u32	crop_height;
	u32	crop_enable;		/* 0: use crop before bds, 1: use crop after bds */
	u32	reserved[PARAMETER_MAX_MEMBER-12];
	u32	err;
};

struct param_dma_output {
	u32	cmd;
	u32	format;
	u32	bitwidth;
	u32	order;
	u32	plane;
	u32	width;			/* BDS output width */
	u32	height;			/* BDS output height */
	u32	dma_crop_offset_x;
	u32	dma_crop_offset_y;
	u32	dma_crop_width;
	u32	dma_crop_height;
	u32	crop_enable;		/* 0: use crop before bds, 1: use crop after bds */
	u32	msb;			/* last bit of data in memory size */
	u32	stride_plane0;
	u32	stride_plane1;
	u32	stride_plane2;
	u32	v_otf_enable;
	u32	v_otf_dst_ip;		/* ISPLP:0, ISPHQ:1, DCP:2, CIP1:4, CIP2:3 */
	u32	v_otf_dst_axi_id;
	u32	v_otf_token_line;
	u32	sbwc_type;		/* COMP_OFF = 0, COMP_LOSSLESS = 1, COMP_LOSSY =2 */
	u32	channel;		/* 3AA channel for HF */
	u32	reserved[PARAMETER_MAX_MEMBER-23];
	u32	err;
};
struct param_mcs_input {
	u32	otf_cmd;		/* DISABLE or ENABLE */
	u32	otf_format;
	u32	otf_bitwidth;
	u32	otf_order;
	u32	dma_cmd;		/* DISABLE or ENABLE */
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
	u32	stripe_in_start_pos_x;	/* Start X Position w/ Margin For STRIPE */
	u32	stripe_in_end_pos_x;	/* End X Position w/ Margin For STRIPE */
	u32	stripe_roi_start_pos_x;	/* Start X Position w/o Margin For STRIPE */
	u32	stripe_roi_end_pos_x;	/* End X Position w/o Margin For STRIPE */
	u32	sbwc_type;
	u32	reserved[PARAMETER_MAX_MEMBER-25];
	u32	err;
};

struct param_mcs_output {
	u32	otf_cmd;		/* DISABLE or ENABLE */
	u32	otf_format;
	u32	otf_bitwidth;
	u32	otf_order;
	u32	dma_cmd;		/* DISABLE or ENABLE */
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
	u32	yuv_range;		/* FULL or NARROW */
	u32	flip;			/* NORMAL(0) or X-MIRROR(1) or Y- MIRROR(2) or XY- MIRROR(3) */
	u32	hwfc;			/* DISABLE or ENABLE */
	u32	offset_x;
	u32	offset_y;

	/*
	 * cmd
	 * [BIT 0]: output crop(0: disable, 1: enable)
	 * [BIT 1]: crop type(0: center, 1: freeform)
	 */
	u32	cmd;
	u32	stripe_in_start_pos_x;	/* Start X Position w/ Margin For STRIPE */
	u32	stripe_roi_start_pos_x;	/* Start X Position w/o Margin For STRIPE */
	u32	stripe_roi_end_pos_x;	/* End X Position w/o Margin For STRIPE */
	u32	full_input_width; 	/* FULL WIDTH For STRIPE */
	u32	full_output_width;
	u32	sbwc_type;
	u32	bitsperpixel;		/* bitsperpixel[23:16]: plane2, [15:8]: plane1,  [7:0]: plane0 */
	u32	reserved[PARAMETER_MAX_MEMBER-31];
	u32	err;
};

struct param_lme_dma {
	u32	cur_input_cmd;
	u32	prev_input_cmd;
	u32	output_cmd;
	u32	input_format;
	u32	input_bitwidth;
	u32	input_order;
	u32	input_plane;
	u32	input_msb; /* last bit of data in memory size */
	u32	cur_input_width;
	u32	cur_input_height;
	u32	prev_input_width;
	u32	prev_input_height;
	u32	output_format;
	u32	output_bitwidth;
	u32	output_order;
	u32	output_plane;
	u32	output_width;
	u32	output_height;
	u32	output_msb;
	u32	reserved[PARAMETER_MAX_MEMBER-20];
	u32	err;
};

struct param_orbmch_dma {
	u32	cur_input_cmd;
	u32	prev_input_cmd;
	u32	output_cmd;
	u32	input_format;
	u32	input_bitwidth;
	u32	input_order;
	u32	input_plane;
	u32	input_msb; /* last bit of data in memory size */
	u32	cur_input_width;
	u32	cur_input_height;
	u32	prev_input_width;
	u32	prev_input_height;
	u32	output_format;
	u32	output_bitwidth;
	u32	output_order;
	u32	output_plane;
	u32	output_width;
	u32	output_height;
	u32	output_msb;
	u32	prev_output_cmd;	/* mch input - previous descriptor */
	u32	reserved[PARAMETER_MAX_MEMBER-21];
	u32	err;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Pablo parameter definitions					   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
struct sensor_param {
	struct param_sensor_config	config;
};

struct taa_param {
	struct param_control		control;
	struct param_otf_input		otf_input;	/* otf_input */
	struct param_dma_input		vdma1_input;	/* dma1_input */
	struct param_otf_output		otf_output;	/* not use */
	struct param_dma_output		vdma4_output;	/* Before BDS */
	struct param_dma_output		vdma2_output;	/* After BDS */
	struct param_dma_output		efd_output;	/* Early FD */
	struct param_dma_output		mrg_output;	/* mrg_out */
	struct param_dma_output		drc_output;	/* drc grid out */
	struct param_dma_output		ods_output;	/* orb-ds out */
	struct param_dma_output		lds_output;	/* lme-ds out */
	struct param_dma_output		hf_output;	/* High Frequency */
	struct param_stripe_input	stripe_input;
};

struct isp_param {
	struct param_control		control;
	struct param_otf_input		otf_input;	/* not use */
	struct param_dma_input		vdma1_input;	/* dma1_input */
	struct param_dma_input		vdma2_input;	/* tnr weight input */
	struct param_dma_input		vdma3_input;	/* tnr prev img input */
	struct param_dma_input		motion_dma_input; /* motion dma input */
	struct param_dma_input		rgb_input;	/* rgb input */
	struct param_dma_input		noise_input;	/* noise input */
	struct param_dma_input		scmap_input;	/* scmap input */
	struct param_dma_input		drc_input;	/* drc grid input */
	struct param_otf_output		otf_output;	/* otf_out */
	struct param_dma_output		vdma4_output;	/* dns image output */
	struct param_dma_output		vdma5_output;
	struct param_dma_output		vdma6_output;	/* TNR prev image output */
	struct param_dma_output		vdma7_output;	/* TNR prev weight output */
	struct param_dma_output		nrds_output;
	struct param_dma_output		noise_output;
	struct param_dma_output		drc_output;
	struct param_dma_output		hist_output;
	struct param_dma_output		noise_rep_output;
	struct param_dma_output		hf_output;
	struct param_dma_output		rgb_output;
	struct param_dma_output		nrb_output;
	struct param_stripe_input	stripe_input;
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

struct vra_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;
	struct param_fd_config		config;
};

struct clh_param {
	struct param_control		control;
	struct param_dma_input		dma_input;
	struct param_dma_output		dma_output;
};

struct ypp_param {
	struct param_control		control;
	struct param_dma_input		dma_input;
	struct param_dma_input		dma_input_lv2;
	struct param_dma_input		dma_input_noise;
	struct param_dma_input		dma_input_drc;
	struct param_dma_input		dma_input_hist;
	struct param_otf_output		otf_output;
	struct param_stripe_input	stripe_input;
};

struct paf_rdma_param {
	struct param_control            control;
	struct param_dma_input          dma_input;
};

struct mcs_param {
	struct param_control		control;
	struct param_mcs_input		input;
	struct param_mcs_output		output[MCSC_OUTPUT_MAX];
	struct param_dma_input		dma_input;
	struct param_stripe_input	stripe_input;
};

struct lme_param {
	struct param_control		control;
	struct param_lme_dma		dma;
};

struct orbmch_param {
	struct param_control		control;
	struct param_orbmch_dma		dma;
};

#if IS_ENABLED(LOGICAL_VIDEO_NODE)
struct is_param_region {
	struct sensor_param		sensor;
	struct taa_param		taa;
	struct isp_param		isp;
	struct mcs_param		mcs;
	struct paf_rdma_param           paf;
	struct ypp_param		ypp;
	struct orbmch_param		orbmch;
};
#else
struct is_param_region {
	struct sensor_param		sensor;
	struct taa_param		taa;
	struct isp_param		isp;
	struct tpu_param		tpu;
	struct mcs_param		mcs;
	struct vra_param		vra;
	struct paf_rdma_param           paf;
	struct ypp_param		ypp;
	struct orbmch_param		orbmch;
};
#endif

/////////////////////////////////////////////////////////////////////

struct is_fd_rect {
	u32	offset_x;
	u32	offset_y;
	u32	width;
	u32	height;
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

/////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////

struct is_region {
	struct is_param_region	parameter;
	struct is_fdae_info	fdae_info;
	struct nfd_info		fd_info;
	spinlock_t		fd_info_slock;
};

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

struct orbmch_param_set {
	struct param_orbmch_dma		dma;

	u32				cur_input_dva[IS_MAX_PLANES];
	u32				prev_input_dva[IS_MAX_PLANES];
	u32				output_dva[IS_MAX_PLANES];
	u64				output_kva_motion_pure[IS_MAX_PLANES];
	u64				output_kva_motion_processed[IS_MAX_PLANES];
	u32				prev_output_dva[IS_MAX_PLANES];		/* Driver use only */
	u64				prev_output_kva[IS_MAX_PLANES];		/* Driver use only */
	u32				instance_id;
	u32				fcount;
	bool			reprocessing;
	u32				tnr_mode;
};

struct is_3aa_config {
	u32 pdaf_ident0_bypass;
	u32 dtp_bypass;
	u32 bitmask_bypass;
	u32 blc_bypass;
	u32 thstatpre_bypass;
	u32 thstatpre_grid_w;
	u32 thstatpre_grid_h;
	u32 cgras_bypass;
	u32 pdaf_ident1_bypass;
	u32 bpc_bypass;
	u32 bpc_radial_compensation_en;
	u32 bpc_radial_stretch;
	u32 bpc_pd_bpr_en;
	u32 recon_bypass;
	u32 thstat_bypass;
	u32 thstat_grid_w;
	u32 thstat_grid_h;
	u32 rgbyhist_bypass;
	u32 rgby_bin_num;
	u32 rgby_hist_num;
	u32 wbg_bypass;
	u32 cdaf_bypass;
	u32 drcclct_bypass;
	u32 drc_grid_w;
	u32 drc_grid_h;
	u32 fdpig_post_gamma_bypass;
	u32 fdpig_ccm_bypass;
	u32 fdpig_rgb_gamma_bypass;
	u32 orbds_pre_gamma_bypass;
	u32 orbds_bypass;
	u32 menr_bypass;
	u32 orbds0_w;
	u32 orbds0_h;
	u32 orbds1_w;
	u32 orbds1_h;
	u32 blc_dns_bypass;
	u32 blc_zsl_bypass;
	u32 blc_strp_bypass;
	u32 sensor_center_x;
	u32 sensor_center_y;
	u32 event_interval;
	u32 magic;
};

struct is_orbmch_config {
	u32 orbmch_lv1_in_w;
	u32 orbmch_lv1_in_h;
	u32 orbmch_lv2_in_w;
	u32 orbmch_lv2_in_h;
	u32 magic;
};

struct is_isp_config {
	/* MCFP */
	u32 geomatch_en;
	u32 geomatch_bypass;
	u32 bgdc_en;
	u32 mixer_en;
	u32 still_en;
	u32 tnr_mode;
	u32 img_bit;
	u32 wgt_bit;
	u32 motion_size_w;
	u32 motion_size_h;
	u32 skip_wdma;
	u32 top_nflbc_6line;
	u32 top_otfout_mask_clean;
	u32 top_otfout_mask_dirty;

	/* DNS,ITP */
	u32 dtp_byapss;
	u32 blc_bypass;
	u32 wbg_byapss;
	u32 hqdns_bypass;
	u32 hqdns_biquad_scale_shift_adder;
	u32 pre_gamma_byapss;
	u32 drc_dstr_bypass;
	u32 drc_dstr_grid_x;
	u32 drc_dstr_grid_y;
	u32 drc_clct_bypass;
	u32 drc_clct_grid_x;
	u32 drc_clct_grid_y;
	u32 dirty_bayer_en;
	u32 ww_lc_en;
	u32 tnr_input_en;
	u32 dmsc_bypass;
	u32 post_gamma_bypass;
	u32 sharpen_bypass;
	u32 nfltr_bypass;
	u32 edgedet_bypass;
	u32 nsmix_nypass;
	u32 drc_tmc_bypass;
	u32 stk_bypass;
	u32 ccm_bypass;
	u32 rgb_gamma_bypass;
	u32 prc_bypass;
	u32 skind_bypass;
	u32 snadd_bypass;
	u32 oetf_bypass;
	u32 sensor_center_x;
	u32 sensor_center_y;
	u32 magic;
};

enum taa_event_id {
	EVENT_TAA_FRAME_START = 1,
	EVENT_TAA_FRAME_END,
	EVENT_TAA_FRAME_END_REG_WRITE_FAILURE,

	EVENT_TAA_CDAF = 100,
	EVENT_TAA_THSTAT_PRE,
	EVENT_TAA_THSTAT,
	EVENT_TAA_RGBYHIST,

	EVENT_TAA_ID_END,
};

struct taa_event_data {
	ulong kva;
	u32 bytes;
	struct param_dma_output dma; /* Dummy */
	u32 magic;
};

enum orbmch_event_id {
	EVENT_ORBMCH_FRAME_START = 1,
	EVENT_ORBMCH_FRAME_END,
	EVENT_ORBMCH_MOTION_DATA = 100,
	EVENT_ORBMCH_ID_END,
};

struct orbmch_data {
	u64 kpt_kva;
	u32 kpt_bytes;
	u64 mch_kva;
	u32 mch_bytes;
	u64 lmc_kva;
	u32 lmc_bytes;
	u32 magic;
};

/* Image RTA */
struct pablo_point {
	s32 x;
	s32 y;
};

struct pablo_size {
	u32 width;
	u32 height;
};

struct pablo_area {
	struct pablo_point offset;
	struct pablo_size size;
};

struct pablo_rta_frame_info {
	/* Common */
	struct pablo_size sensor_calibration_size;
	struct pablo_point sensor_binning;
	struct pablo_area sensor_crop;
	u32 sensor_mono_en;
	u32 sensor_bayer_pattern;
	u32 sensor_bayer_order;
	u32 sensor_output_bit;
	u32 sensor_hdr;

	u32 magic;
};

#endif /* IS_PARAMS_H */
