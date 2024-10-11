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

#define CSTAT_CTX_NUM			2
#define CSTAT_PARAM_NUM			(PARAM_CSTAT_CDS - PARAM_CSTAT_CONTROL + 1)
#define GET_CSTAT_CTX_PINDEX(p, c)	((p) + ((c) * CSTAT_PARAM_NUM))

#define MCSC_OUTPUT_DS		MCSC_OUTPUT5
#define MCSC_INPUT_HF		MCSC_OUTPUT5

enum is_param {
	PARAM_SENSOR_CONFIG,

	/* CSTAT main context parameters */
	PARAM_CSTAT_CONTROL,
	PARAM_CSTAT_OTF_INPUT,
	PARAM_CSTAT_DMA_INPUT,
	PARAM_CSTAT_LME_DS0,
	PARAM_CSTAT_LME_DS1,
	PARAM_CSTAT_FDPIG,
	PARAM_CSTAT_RGBHIST,
	PARAM_CSTAT_SVHIST,
	PARAM_CSTAT_DRC,
	PARAM_CSTAT_CDS,

	/* CSTAT sub context parameers */
	PARAM_CSTAT2_CONTROL,
	PARAM_CSTAT2_OTF_INPUT,
	PARAM_CSTAT2_DMA_INPUT,
	PARAM_CSTAT2_LME_DS0,
	PARAM_CSTAT2_LME_DS1,
	PARAM_CSTAT2_FDPIG,
	PARAM_CSTAT2_RGBHIST,
	PARAM_CSTAT2_SVHIST,
	PARAM_CSTAT2_DRC,
	PARAM_CSTAT2_CDS,

	PARAM_BYRP_CONTROL,
	PARAM_BYRP_OTF_OUTPUT,
	PARAM_BYRP_DMA_INPUT,
	PARAM_BYRP_STRIPE_INPUT,
	PARAM_BYRP_HDR,
	PARAM_BYRP_BYR,

	PARAM_RGBP_CONTROL,
	PARAM_RGBP_OTF_INPUT,
	PARAM_RGBP_OTF_OUTPUT,
	PARAM_RGBP_DMA_INPUT,
	PARAM_RGBP_STRIPE_INPUT,
	PARAM_RGBP_HF,
	PARAM_RGBP_SF,
	PARAM_RGBP_YUV,
	PARAM_RGBP_RGB,

	PARAM_MCFP_CONTROL,
	PARAM_MCFP_OTF_INPUT,
	PARAM_MCFP_OTF_OUTPUT,
	PARAM_MCFP_DMA_INPUT,
	PARAM_MCFP_FTO_INPUT,		/* backward OTF */
	PARAM_MCFP_STRIPE_INPUT,
	PARAM_MCFP_PREV_YUV,
	PARAM_MCFP_PREV_W,
	PARAM_MCFP_DRC,
	PARAM_MCFP_DP,
	PARAM_MCFP_MV,
	PARAM_MCFP_SF,
	PARAM_MCFP_W,
	PARAM_MCFP_YUV,

	PARAM_YUVP_CONTROL,
	PARAM_YUVP_OTF_INPUT,
	PARAM_YUVP_OTF_OUTPUT,
	PARAM_YUVP_DMA_INPUT,
	PARAM_YUVP_FTO_OUTPUT,
	PARAM_YUVP_STRIPE_INPUT,
	PARAM_YUVP_SEG,
	PARAM_YUVP_DRC,
	PARAM_YUVP_SVHIST,
	PARAM_YUVP_YUV,

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
	PARAM_PAF_OTF_OUTPUT,		/* deprecated, only for compatibility */
	PARAM_PAF_DMA_OUTPUT,

	PARAM_LME_CONTROL,
	PARAM_LME_DMA,

	IS_PARAM_NUM,
};

#define PARAM_MCSC_CONTROL	PARAM_MCS_CONTROL
#define PARAM_MCSC_OTF_INPUT	PARAM_MCS_INPUT
#define PARAM_MCSC_P0		PARAM_MCS_OUTPUT0
#define PARAM_MCSC_P1		PARAM_MCS_OUTPUT1
#define PARAM_MCSC_P2		PARAM_MCS_OUTPUT2
#define PARAM_MCSC_P3		PARAM_MCS_OUTPUT3
#define PARAM_MCSC_P4		PARAM_MCS_OUTPUT4
#define PARAM_MCSC_P5		PARAM_MCS_OUTPUT5

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
	u32     hdr_frame_type; /* enum hdr_frame_type, single/long/short/middle */
	u32	reserved[PARAMETER_MAX_MEMBER-24];
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
	u32	full_incrop_width;	/* Original incrop image width */
	u32	full_incrop_height;	/* Original incrop image height */
	u32	scaled_left_margin;	/* for scaler */
	u32	scaled_right_margin;	/* for scaler */
	u32	scaled_full_width;	/* for scaler */
	u32	scaled_full_height;	/* for scaler */
	u32	stripe_roi_start_pos_x[5];	/* Start X Position w/o Margin For STRIPE */
	u32	start_pos_x;
	u32	reserved[PARAMETER_MAX_MEMBER - 19];
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
	u32	sad_output_cmd;
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
	u32	sad_output_plane;
	u32	output_width;
	u32	output_height;
	u32	output_msb;
	u32	processed_output_cmd;
	u32	reserved[PARAMETER_MAX_MEMBER-23];
	u32	err;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Pablo parameter definitions					   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
struct sensor_param {
	struct param_sensor_config	config;
};

struct cstat_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_dma_input		dma_input;
	struct param_dma_output		lme_ds0;
	struct param_dma_output		lme_ds1;
	struct param_dma_output		fdpig;
	struct param_dma_output		rgbhist;
	struct param_dma_output		svhist;
	struct param_dma_output		drc;
	struct param_dma_output		cds;
};

struct byrp_param {
	struct param_control		control;
	struct param_otf_output         otf_output;
	struct param_dma_input		dma_input;
	struct param_stripe_input	stripe_input;
	struct param_dma_input		hdr;
	struct param_dma_output		byr;
};

struct rgbp_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_otf_output         otf_output;
	struct param_dma_input		dma_input;
	struct param_stripe_input	stripe_input;
	struct param_dma_output		hf;
	struct param_dma_output		sf;
	struct param_dma_output		yuv;
	struct param_dma_output		rgb;
};

struct mcfp_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_otf_output         otf_output;
	struct param_dma_input		dma_input;
	struct param_otf_input		fto_input;
	struct param_stripe_input	stripe_input;
	struct param_dma_input		prev_yuv;
	struct param_dma_input		prev_w;
	struct param_dma_input		drc;
	struct param_dma_input		dp;
	struct param_dma_input		mv;
	struct param_dma_input		sf;
	struct param_dma_output		w;
	struct param_dma_output		yuv;
};

struct yuvp_param {
	struct param_control		control;
	struct param_otf_input		otf_input;
	struct param_otf_output         otf_output;
	struct param_dma_input		dma_input;
	struct param_otf_output         fto_output;
	struct param_stripe_input	stripe_input;
	struct param_dma_input		seg;
	struct param_dma_input		drc;
	struct param_dma_input		svhist;
	struct param_dma_output		yuv;
};

struct mcs_param {
	struct param_control		control;
	struct param_mcs_input		input;
	struct param_mcs_output		output[MCSC_OUTPUT_MAX];
	struct param_dma_input		dma_input;
	struct param_stripe_input	stripe_input;
};

struct pamir_paf_param {
	struct param_control            control;
	struct param_dma_input          dma_input;
	struct param_otf_output         otf_output;
	struct param_dma_output         dma_output;
};

struct lme_param {
	struct param_control		control;
	struct param_lme_dma		dma;
};

struct paf_rdma_param {
	struct param_control            control;
	struct param_dma_input          dma_input;
	struct param_otf_output         otf_output;
	struct param_dma_output         dma_output;
};

struct vra_param {
	struct param_control		control;
};

struct is_param_region {
	struct sensor_param	sensor;
	struct cstat_param	cstat[CSTAT_CTX_NUM];
	struct byrp_param	byrp;
	struct rgbp_param	rgbp;
	struct mcfp_param	mcfp;
	struct yuvp_param	yuvp;
	struct mcs_param	mcs;
	struct paf_rdma_param	paf;
	struct lme_param	lme;
};

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
	bool		ismask[MAX_FACE_COUNT];
	u32		iseye[MAX_FACE_COUNT];
	struct fd_rect	eyerect[MAX_FACE_COUNT][2];
	struct fd_rect	maskrect[MAX_FACE_COUNT];
};

enum vpl_object_type {
	VPL_OBJECT_BACKGROUND,
	VPL_OBJECT_FACE,
	VPL_OBJECT_TORSO,
	VPL_OBJECT_PERSON,
	VPL_OBJECT_ANIMAL,
	VPL_OBJECT_FOOD,
	VPL_OBJECT_PET_FACE,
	VPL_OBJECT_CHART,
	VPL_OBJECT_MASK,
	VPL_OBJECT_EYE,
	VPL_OBJECT_CLASS_COUNT
};

struct is_od_rect {
	float	offset_x;
	float	offset_y;
	float	width;
	float	height;
};

struct od_info {
	int			od_num;
	unsigned int		id[MAX_FACE_COUNT];
	struct	is_od_rect	rect[MAX_FACE_COUNT];
	enum vpl_object_type	type[MAX_FACE_COUNT];
	float			score[MAX_FACE_COUNT];
};

/////////////////////////////////////////////////////////////////////

struct is_region {
	struct is_param_region	parameter;
	struct is_fdae_info	fdae_info;
	struct nfd_info		fd_info;
	struct od_info		od_info; /* Object Detect */
	spinlock_t		fd_info_slock;
};

/*
 * =================================================================================================
 * DDK interface
 * =================================================================================================
 */
struct lme_param_set {
	struct param_lme_dma		dma;

	u32				cur_input_dva[IS_MAX_PLANES];
	u32				prev_input_dva[IS_MAX_PLANES];
#if IS_ENABLED(ENABLE_LME_PREV_KVA)
	u64				prev_input_kva[IS_MAX_PLANES];
#endif
	u32				output_dva[IS_MAX_PLANES];			/* H/W MV dva */
	u32				output_dva_sad[IS_MAX_PLANES];			/* H/W SAD dva */
	u64				output_kva_motion_pure[IS_MAX_PLANES];		/* H/W MV kva */
	u64				output_kva_sad[IS_MAX_PLANES];			/* H/W SAD kva */
	u64				output_kva_motion_processed[IS_MAX_PLANES];	/* S/W MV kva */
	u64				output_kva_motion_processed_hdr[IS_MAX_PLANES];	/* S/W MV kva for stitch hdr */
	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
	u32				tnr_mode;
};

struct cstat_param_set {
	struct param_sensor_config	sensor_config;
	struct param_otf_input		otf_input;
	struct param_otf_output		otf_output;
	struct param_dma_input		dma_input;
	struct param_dma_output		dma_output_lme_ds0;
	struct param_dma_output		dma_output_lme_ds1;
	struct param_dma_output		dma_output_fdpig;
	struct param_dma_output		dma_output_svhist;
	struct param_dma_output		dma_output_drc;
	struct param_dma_output		dma_output_cds;

	/* Internal purpose DMA params */
	struct param_dma_output		pre;
	struct param_dma_output		ae;
	struct param_dma_output		awb;
	struct param_dma_output		rgby;

	u32				input_dva[IS_MAX_PLANES];
	u32				output_dva_lem_ds0[IS_MAX_PLANES];
	u32				output_dva_lem_ds1[IS_MAX_PLANES];
	u32				output_dva_fdpig[IS_MAX_PLANES];
	u32				output_dva_svhist[IS_MAX_PLANES];
	u32				output_dva_drc[IS_MAX_PLANES];
	u32				output_dva_cds[IS_MAX_PLANES];

	u32				pre_dva;
	u32				ae_dva;
	u32				awb_dva;
	u32				rbgy_dva;

	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
	struct param_stripe_input	stripe_input;
};

struct byrp_param_set {
	struct stat_param_set		*stat_param;
	struct param_otf_output		otf_output;
	struct param_dma_input		dma_input;
	struct param_stripe_input	stripe_input;
	struct param_dma_input		dma_input_hdr;
	struct param_dma_output		dma_output_byr;
	u32				input_dva[IS_MAX_PLANES];
	u32				input_dva_hdr[IS_MAX_PLANES];
	u32				output_dva_byr[IS_MAX_PLANES];

	u32				instance_id;
	u32				fcount;
	bool				reprocessing;
};

struct rgbp_param_set {
	struct byrp_param		*byrp_param;
	struct param_otf_input		otf_input;
	struct param_otf_output		otf_output;
	struct param_dma_input		dma_input;
	struct param_stripe_input	stripe_input;
	struct param_dma_output		dma_output_hf;
	struct param_dma_output		dma_output_sf;
	struct param_dma_output		dma_output_yuv;
	struct param_dma_output		dma_output_rgb;
	u32				input_dva[IS_MAX_PLANES];
	u32				output_dva_hf[IS_MAX_PLANES];
	u32				output_dva_sf[IS_MAX_PLANES];
	u32				output_dva_yuv[IS_MAX_PLANES];
	u32				output_dva_rgb[IS_MAX_PLANES];

	u32				instance_id;
	u32				fcount;
	u32				tnr_mode;
	bool				reprocessing;
};

struct mcfp_param_set {
	struct rgbp_param_set		*rgbp_param;
	struct param_otf_input		otf_input;
	struct param_otf_output         otf_output;
	struct param_dma_input		dma_input;
	struct param_otf_input		fto_input;
	struct param_stripe_input	stripe_input;
	struct param_dma_input		dma_input_prev_yuv;
	struct param_dma_input		dma_input_prev_w;
	struct param_dma_input		dma_input_drc_prv_map;
	struct param_dma_input		dma_input_drc_cur_map;
	struct param_dma_input		dma_input_mv;
	struct param_dma_input		dma_input_sf;
	struct param_dma_output		dma_output_w;
	struct param_dma_output		dma_output_yuv;
	u32				input_dva[IS_MAX_PLANES];
	u32				input_dva_prv_yuv[IS_MAX_PLANES];
	u32				input_dva_prv_w[IS_MAX_PLANES];
	u32				input_dva_drc_prv_map[IS_MAX_PLANES];
	u32				input_dva_drc_cur_map[IS_MAX_PLANES];
	u32				input_dva_mv[IS_MAX_PLANES];
	u32				input_dva_sf[IS_MAX_PLANES];
	u32				output_dva_w[IS_MAX_PLANES];
	u32				output_dva_yuv[IS_MAX_PLANES];

	u32				instance_id;
	u32				fcount;
	u32				tnr_mode;
	bool				reprocessing;
};

struct yuvp_param_set {
	struct mcfp_param_set		*mcfp_param;
	struct param_otf_input		otf_input;
	struct param_otf_output		otf_output;
	struct param_dma_input		dma_input;
	struct param_otf_output         fto_output;
	struct param_stripe_input	stripe_input;
	struct param_dma_input		dma_input_seg;
	struct param_dma_input		dma_input_drc;
	struct param_dma_input		dma_input_svhist;
	struct param_dma_output		dma_output_yuv;
	u32				input_dva[IS_MAX_PLANES];
	u32				input_dva_seg[IS_MAX_PLANES];
	u32				input_dva_drc[IS_MAX_PLANES];
	u32				input_dva_svhist[IS_MAX_PLANES];
	u32				output_dva_yuv[IS_MAX_PLANES];

	u32				instance_id;
	u32				fcount;
	u32				tnr_mode;
	bool				reprocessing;
};

struct is_cstat_config {
	u32 sensor_center_x;
	u32 sensor_center_y;
	u32 bitmask_bypass;
	u32 afident_dtp_bypass;
	u32 dtp_bypass;
	u32 afident_sbpc_bypass;
	u32 sbpc_bypass;
	u32 gamma_sensor_bypass;
	u32 blc_bypass;
	u32 thstatpre_bypass;
	u32 pre_grid_w;
	u32 pre_grid_h;
	u32 cgras_bypass;
	u32 thstatawb_bypass;
	u32 awb_grid_w;
	u32 awb_grid_h;
	u32 thstatae_bypass;
	u32 ae_grid_w;
	u32 ae_grid_h;
	u32 rgbyhist_bypass;
	u32 rgby_bin_num;
	u32 rgby_hist_num;
	u32 wbg_bypass;
	u32 cdaf_bypass;
	u32 makergb_bypass;
	u32 luma_shading_bypass;
	u32 rgb2y_bypass;
	u32 menr_bypass;
	u32 gamma_y_bypass;
	u32 meds_bypass;
	u32 lmeds_bypass;
	u32 lmeds_w;
	u32 lmeds_h;
	u32 lmeds1_w;
	u32 lmeds1_h;
	u32 rgb_ccmFdpig_bypass;
	u32 gamma_rgb_fdpig_bypass;
	u32 drcclct_bypass;
	u32 drc_grid_w;
	u32 drc_grid_h;
	u32 vhist_bypass;
	u32 vhist_grid_num;
	u32 lsc_step_x;
	u32 lsc_step_y;

	u32 magic;
};

struct is_byrp_config {
	u32 sensor_center_x;
	u32 sensor_center_y;
	u32 pre_bpc_bypass;
	u32 post_bpc_bypass;
	u32 lsc_step_x;
	u32 lsc_step_y;

	u32 magic;
};

struct is_rgbp_config {
	u32 sensor_center_x;
	u32 sensor_center_y;
	u32 dns_bypass;
	u32 tdmsc_bypass;
	u32 dmsc_bypass;
	u32 luma_shading_bypass;
	u32 gammargb_bypass;
	u32 gtm_bypass;
	u32 rgbtoyuv_bypass;
	u32 yuv444to422_bypass;
	u32 satflag_bypass;
	u32 decomp_enable;
	u32 decomp_bypass;
	u32 ccm_bypass;
	u32 gammalr_bypass;
	u32 gammalr_enable;
	u32 gammahr_bypass;
	u32 gammahr_enable;
	u32 biquad_scale_shift_adder;
	u32 lsc_step_x;
	u32 lsc_step_y;

	u32 magic;
};

struct is_yuvpp_config {
	u32 hf_decomposition_enable;
	u32 yuvnr_bypass;
	u32 yuv422torgb_bypass;
	u32 clahe_bypass;
	u32 prc_bypass;
	u32 skind_bypass;
	u32 noise_gen_bypass;
	u32 nadd_bypass;
	u32 oetf_gamma_bypass;
	u32 rgbtoyuv422_bypass;
	u32 drc_bypass;
	u32 magic;
};

struct is_yuvp_config {
	u32 hf_decomposition_enable;
	u32 yuvnr_bypass;
	u32 yuv422torgb_bypass;
	u32 clahe_bypass;
	u32 vhist_grid_num;
	u32 prc_bypass;
	u32 skind_bypass;
	u32 noise_gen_bypass;
	u32 nadd_bypass;
	u32 oetf_gamma_bypass;
	u32 rgbtoyuv422_bypass;
	u32 drc_bypass;
	u32 drc_grid_w;
	u32 drc_grid_h;
	u32 biquad_scale_shift_adder;
	u32 drc_grid_enabled_num;
	u32 yuvnr_contents_aware_isp_en;
	u32 ccm_contents_aware_isp_en;
	u32 sharpen_contents_aware_isp_en;

	u32 magic;
};

struct is_mcfp_config {
	u32 geomatch_en;
	u32 geomatch_bypass;
	u32 mixer_enable;
	u32 still_en;
	u32 mixer_mode;
	u32 img_bit;
	u32 wgt_bit;
	u32 skip_wdma;
	u32 yuv422to444_bypass;
	u32 yuvtorgb_bypass;
	u32 gammargb_bypass;
	u32 rgbtoyuv_bypass;
	u32 yuv444to422_bypass;
	u32 biquad_scale_shift_adder;
	u32 motion_scale_x;
	u32 motion_scale_y;
	u32 drc_grid_w;
	u32 drc_grid_h;
	u32 hdr_type; /* 0: HDR off, 1: Basic HDR, 2: Stitch HDR */

	u32 magic;
};

struct is_lme_config {
	u32 lme_in_w;
	u32 lme_in_h;
	u32 first_frame;

	u32 magic;
};

/* CSTAT specific */
enum cstat_event_id {
	EVENT_CSTAT_FRAME_START = 1,
	EVENT_CSTAT_FRAME_END,
	EVENT_CSTAT_FRAME_END_REG_WRITE_FAILURE,

	EVENT_CSTAT_CDAF = 100,
	EVENT_CSTAT_PRE,
	EVENT_CSTAT_AE,
	EVENT_CSTAT_AWB,
	EVENT_CSTAT_RGBY,
	EVENT_CSTAT_EDGE_SCORE,

	EVENT_CSTAT_ID_END,
};

struct cstat_cdaf {
	ulong kva;
	u32 bytes;
	struct param_dma_output dma; /* Dummy */
};

struct cstat_pre {
	ulong kva;
	u32 bytes;
	struct param_dma_output dma;
};

struct cstat_awb {
	ulong kva;
	u32 bytes;
	struct param_dma_output dma;
};

struct cstat_ae {
	ulong kva;
	u32 bytes;
	struct param_dma_output dma;
};

struct cstat_rgby {
	ulong kva;
	u32 bytes;
	struct param_dma_output dma;
};

struct is_lme_data {
	u64 hw_mv_kva;
	u32 hw_mv_bytes;
	u64 hw_sad_kva;
	u32 hw_sad_bytes;
	u64 sw_mv_kva;
	u32 sw_mv_bytes;
	u64 sw_hdr_mv_kva;
	u32 sw_hdr_mv_bytes;
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

enum mcfp_prev_input_select {
	MCFP_PREV_INPUT_SELECT_INVALID = -1,

	MCFP_PREV_INPUT_SELECT_MCFP = 0,
	MCFP_PREV_INPUT_SELECT_YUVP,
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

	struct pablo_point csis_bns_binning;	/* 1000 = 1.0 */
	struct pablo_point csis_mcb_binning;	/* 1000 = 1.0 */

	/* CSTAT */
	u32 cstat_input_bayer_bit;
	struct pablo_size cstat_input_size;
	struct pablo_area cstat_crop_in;
	struct pablo_area cstat_crop_dzoom;
	struct pablo_size cstat_bns_size;
	struct pablo_area cstat_crop_bns;
	struct pablo_size cstat_fdpig_ds_size;
	struct pablo_area cstat_fdpig_crop;

	/* ME */
	u32 me_run;

	/* BYRP */
	u32 byrp_otfhdr_en;
	u32 byrp_input_bit;
	struct pablo_size byrp_input_size;
	struct pablo_area byrp_bcrop_byr;
	struct pablo_area byrp_bcrop_rgb;
	struct pablo_area byrp_bcrop_zsl;
	u32 byrp_zsl_type;
	s32 byrp_zsl_bit;

	/* RGBP */
	u32 rgbp_input_bit;
	struct pablo_size rgbp_input_size;
	struct pablo_area rgbp_crop_dmsc;
	struct pablo_size rgbp_yuvsc;
	u32 rgbp_y_decomp_en;

	/* MCFP */
	enum mcfp_prev_input_select mcfp_prev_input;
	enum tnr_processing_mode mcfp_tnr_mode;
	u32 mcfp_input_bit;
	struct pablo_size mcfp_input_size;

	/* YUVP */
	u32 yuvp_input_bit;
	struct pablo_size yuvp_input_size;

	/* Buffer available */
	u32 cstat_svhist_buffer_available;
	u32 cstat_drcclct_buffer_available;
	u32 cstat_meds_buffer_available;

	u32 yuvp_yuv_nr_contents_aware_buffer_available;
	u32 yuvp_ccm_contents_aware_buffer_available;
	u32 yuvp_sharp_enhancer_contents_aware_buffer_available;

	u32 magic;
};

#endif /* IS_PARAMS_H */
