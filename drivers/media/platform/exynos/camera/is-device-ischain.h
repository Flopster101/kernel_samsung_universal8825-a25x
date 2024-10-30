/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_ISCHAIN_H
#define IS_DEVICE_ISCHAIN_H

#include "pablo-mem.h"
#include "is-subdev-ctrl.h"
#include "is-hw.h"
#include "is-groupmgr.h"
#include "is-resourcemgr.h"
#include "is-binary.h"
#include "is-debug.h"

#define SENSOR_MAX_CTL			0x3
#define SENSOR_MAX_CTL_MASK		(SENSOR_MAX_CTL-1)

#define REPROCESSING_FLAG		0x80000000
#define REPROCESSING_MASK		0xF0000000
#define REPROCESSING_SHIFT		28
#define OTF_3AA_MASK			0x0F000000
#define OTF_3AA_SHIFT			24
#define SSX_VINDEX_MASK			0x00FF0000
#define SSX_VINDEX_SHIFT		16
#define TAX_VINDEX_MASK			0x0000FF00
#define TAX_VINDEX_SHIFT		8
#define MODULE_MASK			0x000000FF

#define IS_SETFILE_MASK		0x0000FFFF
#define IS_SCENARIO_MASK		0xFFFF0000
#define IS_SCENARIO_SHIFT		16
#define IS_ISP_CRANGE_MASK		0x0F000000
#define IS_ISP_CRANGE_SHIFT	24
#define IS_SCC_CRANGE_MASK		0x00F00000
#define IS_SCC_CRANGE_SHIFT	20
#define IS_SCP_CRANGE_MASK		0x000F0000
#define IS_SCP_CRANGE_SHIFT	16
#define IS_CRANGE_FULL		0
#define IS_CRANGE_LIMITED		1

/*global state*/
enum is_ischain_state {
	IS_ISCHAIN_OPENING,
	IS_ISCHAIN_CLOSING,
	IS_ISCHAIN_OPEN,
	IS_ISCHAIN_INITING,
	IS_ISCHAIN_INIT,
	IS_ISCHAIN_START,
	IS_ISCHAIN_LOADED,
	IS_ISCHAIN_POWER_ON,
	IS_ISCHAIN_OPEN_STREAM,
	IS_ISCHAIN_REPROCESSING,
	IS_ISCHAIN_OFFLINE_REPROCESSING,
	IS_ISCHAIN_MULTI_CH,
};

enum is_stream_type {
	IS_PREVIEW_STREAM = 0,
	IS_REPROCESSING_STREAM,
	IS_OFFLINE_REPROCESSING_STREAM,
};

enum is_camera_device {
	CAMERA_SINGLE_REAR,
	CAMERA_SINGLE_FRONT,
};

enum is_fast_ctl_state {
	IS_FAST_CTL_FREE,
	IS_FAST_CTL_REQUEST,
	IS_FAST_CTL_STATE,
};

struct is_fast_ctl {
	struct list_head	list;
	u32			state;

	bool			lens_pos_flag;
	uint32_t		lens_pos;
};

#define MAX_NUM_FAST_CTL	5
struct fast_control_mgr {
	u32			fast_capture_count;

	spinlock_t		slock;
	struct is_fast_ctl	fast_ctl[MAX_NUM_FAST_CTL];
	u32			queued_count[IS_FAST_CTL_STATE];
	struct list_head	queued_list[IS_FAST_CTL_STATE];
};

#define NUM_OF_3AA_SUBDEV	4
#define NUM_OF_ISP_SUBDEV	2
#define NUM_OF_DCP_SUBDEV	6
#define NUM_OF_MCS_SUBDEV	6
#define MAX_NUM_OF_SUBDEV	9
struct is_device_ischain {
	u32					instance; /* logical stream id */

	struct platform_device			*pdev;
	struct exynos_platform_is		*pdata;

	struct is_resourcemgr			*resourcemgr;
	struct is_groupmgr			*groupmgr;
	struct is_devicemgr			*devicemgr;
	struct is_interface			*interface;

	struct is_hardware			*hardware;

	struct is_minfo				*minfo;
	struct is_path_info			path;

	/* for offline processing */
	struct is_module_enum			*module_enum;
	struct v4l2_subdev			*subdev_module;
	u32					binning;
#if defined(USE_OFFLINE_PROCESSING)
	struct is_dvfs_offline_dvfs_param off_dvfs_param;
#endif

	struct is_region			*is_region;

	unsigned long				state;
	atomic_t				group_open_cnt;
	atomic_t				open_cnt;
	atomic_t				init_cnt;

	u32					setfile;
	u32					apply_setfile_fcount;
	bool					llc_mode; /* 0: LLC off, 1: LLC on */

	struct camera2_fd_uctl			fdUd;
#ifdef ENABLE_SENSOR_DRIVER
	struct camera2_uctl			peri_ctls[SENSOR_MAX_CTL];
#endif

	struct is_group				group_paf;	/* for PAF Bayer RDMA */
	struct is_subdev			pdaf;		/* PDP(PATSTAT) AF RDMA */
	struct is_subdev			pdst;		/* PDP(PATSTAT) PD STAT WDMA */

	struct is_group				group_3aa;
	struct is_subdev			txc;
	struct is_subdev			txp;
	struct is_subdev			txf;
	struct is_subdev			txg;
	struct is_subdev			txo;
	struct is_subdev			txl;

#if IS_ENABLED(SOC_CSTAT0)
	struct is_subdev			subdev_cstat[IS_CSTAT_SUBDEV_NUM];
#endif

	struct is_group				group_lme;	/* LME RDMA */
	struct is_subdev			lmes;		/* LME MVF WDMA */
	struct is_subdev			lmec;		/* LME COST WDMA */
	struct is_subdev			subdev_lme[MAX_NUM_OF_SUBDEV];

	struct is_group				group_orb;	/* ORBMCH */
	struct is_subdev			orbxc;		/* ORBMCH WDMA */

	struct is_group				group_isp;
	struct is_subdev			ixc;
	struct is_subdev			ixp;
	struct is_subdev			ixt;
	struct is_subdev			ixg;
	struct is_subdev			ixv;
	struct is_subdev			ixw;
	struct is_subdev			mexc;		/* for ME */

	struct is_group				group_byrp;
	struct is_subdev			subdev_byrp[MAX_NUM_OF_SUBDEV];

	struct is_group				group_rgbp;
	struct is_subdev			subdev_rgbp[MAX_NUM_OF_SUBDEV];

	struct is_group				group_mcfp;
	struct is_subdev			subdev_mcfp[MAX_NUM_OF_SUBDEV];

	struct is_group				group_ypp;

	struct is_group				group_yuvp;
	struct is_subdev			subdev_yuvp[MAX_NUM_OF_SUBDEV];

	struct is_group				group_mcs;
	struct is_subdev			m0p;
	struct is_subdev			m1p;
	struct is_subdev			m2p;
	struct is_subdev			m3p;
	struct is_subdev			m4p;
	struct is_subdev			m5p;

	struct is_group				group_vra;

	u32					private_data;
	struct is_device_sensor			*sensor;
	struct is_pm_qos_request		user_qos;

	/* Async metadata control to reduce frame delay */
	struct fast_control_mgr			fastctlmgr;

	/* for NI(noise index from DDK) use */
	u32					cur_noise_idx[NI_BACKUP_MAX]; /* Noise index for N + 1 */
	u32					next_noise_idx[NI_BACKUP_MAX]; /* Noise index for N + 2 */

	/* cache maintenance for user buffer */
	struct is_dbuf_q			*dbuf_q;

	ulong					ginstance_map;

	struct pablo_rta_frame_info		prfi;

	u32 yuv_max_width;
	u32 yuv_max_height;
};

/*global function*/
int is_ischain_probe(struct is_device_ischain *device,
	struct is_interface *interface,
	struct is_resourcemgr *resourcemgr,
	struct is_groupmgr *groupmgr,
	struct is_devicemgr *devicemgr,
	struct platform_device *pdev,
	u32 instance);
int is_ischain_g_ddk_capture_meta_update(struct is_group *group, struct is_device_ischain *device,
	u32 fcount, u32 size, ulong addr);
int is_ischain_g_ddk_setfile_version(struct is_device_ischain *device,
	void *user_ptr);
int is_ischain_g_capability(struct is_device_ischain *this,
	ulong user_ptr);

int is_ischain_open_wrap(struct is_device_ischain *device, bool EOS);
int is_ischain_close_wrap(struct is_device_ischain *device);
int is_ischain_start_wrap(struct is_device_ischain *device,
	struct is_group *group);
int is_ischain_stop_wrap(struct is_device_ischain *device,
	struct is_group *group);

int is_ischain_group_open(struct is_device_ischain *device,
			struct is_video_ctx *vctx, u32 group_id);
int is_ischain_group_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx, struct is_group *group);
int is_ischain_group_s_input(struct is_device_ischain *device,
	struct is_group *group,
	u32 stream_type,
	u32 position,
	u32 video_id,
	u32 input_type,
	u32 stream_leader);
int is_ischain_group_buffer_queue(struct is_device_ischain *device,
	struct is_group *group,
	struct is_queue *queue,
	u32 index);
int is_ischain_group_buffer_finish(struct is_device_ischain *device,
	struct is_group *group,
	u32 index);

int is_itf_stream_on(struct is_device_ischain *this);
int is_itf_stream_off(struct is_device_ischain *this);
int is_itf_process_start(struct is_device_ischain *device,
	u64 group);
int is_itf_process_stop(struct is_device_ischain *device,
	u64 group);
int is_itf_force_stop(struct is_device_ischain *device,
	u64 group);
int is_itf_grp_shot(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame);

int is_itf_s_param(struct is_device_ischain *device,
	struct is_frame *frame,
	IS_DECLARE_PMAP(pmap));
void * is_itf_g_param(struct is_device_ischain *device,
	struct is_frame *frame,
	u32 index);
void is_itf_storefirm(struct is_device_ischain *device);
void is_itf_restorefirm(struct is_device_ischain *device);
int is_itf_set_fwboot(struct is_device_ischain *device, u32 val);

int is_ischain_buf_tag_input(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	u32 target_addr[]);
int is_ischain_buf_tag(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	u32 target_addr[]);
int is_ischain_buf_tag_64bit(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	uint64_t target_addr[]);
int is_ischain_update_sensor_mode(struct is_device_ischain *device,
	struct is_frame *frame);

struct is_queue_ops *is_get_ischain_device_qops(void);

int is_itf_power_down(struct is_interface *interface);
int is_ischain_power(struct is_device_ischain *this, int on);

#define IS_EQUAL_COORD(i, o)				\
	(((i)[0] != (o)[0]) || ((i)[1] != (o)[1]) ||	\
	 ((i)[2] != (o)[2]) || ((i)[3] != (o)[3]))
#define IS_NULL_COORD(c)				\
	(!(c)[0] && !(c)[1] && !(c)[2] && !(c)[3])
#endif
