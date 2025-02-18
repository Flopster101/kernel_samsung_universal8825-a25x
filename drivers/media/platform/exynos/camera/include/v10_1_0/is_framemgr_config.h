/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo video node functions
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-param.h"

#define MAX_FRAME_INFO		(4)
#define MAX_STRIPE_REGION_NUM	(5)

#define MAX_OUT_LOGICAL_NODE	(1)
#define MAX_CAP_LOGICAL_NODE	(4)

enum is_frame_info_index {
	INFO_FRAME_START,
	INFO_CONFIG_LOCK,
	INFO_FRAME_END_PROC
};

struct is_frame_info {
	int			cpu;
	int			pid;
	unsigned long long	when;
};

struct is_stripe_size {
	/* Horizontal pixel ratio how much stripe processing is done. */
	u32	h_pix_ratio;
	/* x offset position for sub-frame */
	u32	offset_x;
	/* crop x position for sub-frame */
	u32	crop_x;
	/* stripe width for sub-frame */
	u32	crop_width;
	/* left margin for sub-frame */
	u32	left_margin;
	/* right margin for sub-frame */
	u32	right_margin;
};

struct is_stripe_info {
	/* Region index. */
	u32				region_id;
	/* Total region num. */
	u32				region_num;
	/* Horizontal pixel count which stripe processing is done for. */
	u32				h_pix_num[MAX_STRIPE_REGION_NUM];
	/* stripe start position for sub-frame */
	u32				start_pos_x;
	/* stripe width for sub-frame */
	u32				stripe_width;
	/* sub-frame size for incrop/otcrop */
	struct is_stripe_size		in;
	struct is_stripe_size		out;
	/* For image dump */
	ulong                           kva[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];
	size_t                          size[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];
};

struct is_sub_frame {
	/* Target address for all input node
	 * 0: invalid address, DMA should be disabled
	 * any value: valid address
	 */
	u32			id;
	u32			num_planes;
	u32			hw_pixeltype;
	ulong			kva[IS_MAX_PLANES];
	dma_addr_t		dva[IS_MAX_PLANES];
	dma_addr_t		backup_dva[IS_MAX_PLANES];

	struct is_stripe_info	stripe_info;
};

struct is_sub_node {
	u32			hw_id;
	struct is_sub_frame	sframe[CAPTURE_NODE_MAX];
};

struct is_frame {
	struct list_head	list;
	struct kthread_work	work;
	struct kthread_delayed_work dwork;
	void			*groupmgr;
	void			*group;
	void			*subdev; /* is_subdev */

	/* group leader use */
	struct camera2_shot_ext	*shot_ext;
	struct camera2_shot	*shot;
	size_t			shot_size;

	/* stream use */
	struct camera2_stream	*stream;

	/* common use */
	u32			planes; /* total planes include multi-buffers */
	struct is_priv_buf	*pb_output;
	struct is_priv_buf	*pb_capture[CAPTURE_NODE_MAX];
	dma_addr_t		dvaddr_buffer[IS_MAX_PLANES];
	ulong			kvaddr_buffer[IS_MAX_PLANES];
	u32			size[IS_MAX_PLANES];

	/* external plane use */
	u32			ext_planes;
	ulong			kvaddr_ext[IS_EXT_MAX_PLANES];

	/*
	 * target address for capture node
	 * [0] invalid address, stop
	 * [others] valid address
	 */
	u32 cstat_ctx;
	u32 txpTargetAddress[CSTAT_CTX_NUM][IS_MAX_PLANES];		/* CSTAT CDS WDMA */
	u32 efdTargetAddress[CSTAT_CTX_NUM][IS_MAX_PLANES];		/* CSTAT FDPIG WDMA */
	u32 txdgrTargetAddress[CSTAT_CTX_NUM][IS_MAX_PLANES];		/* CSTAT DRC GRID WDMA */
	u32 txldsTargetAddress[CSTAT_CTX_NUM][IS_MAX_PLANES];		/* CSTAT LMEDS0 WDMA */
	u32 dva_cstat_lmeds1[CSTAT_CTX_NUM][IS_MAX_PLANES];		/* CSTAT LMEDS1 WDMA */
	u32 dva_cstat_vhist[CSTAT_CTX_NUM][IS_MAX_PLANES];		/* CSTAT VHIST WDMA */

	u32 lmesTargetAddress[IS_MAX_PLANES];		/* LME prev IN DMA */
	u32 lmecTargetAddress[IS_MAX_PLANES];		/* LME MV OUT DMA */
	u64 lmecKTargetAddress[IS_MAX_PLANES];		/* LME MV OUT KVA */
	u32 lmembmv0TargetAddress[IS_MAX_PLANES];	/* LME MBMV DMA */
	u32 lmembmv1TargetAddress[IS_MAX_PLANES];	/* LME MBMV DMA */
	u32 lmesadTargetAddress[IS_MAX_PLANES];		/* LME SAD OUT DMA */
	u64 lmesadKTargetAddress[IS_MAX_PLANES];	/* LME SAD OUT KVA */
	u64 lmemKTargetAddress[IS_MAX_PLANES];		/* LME MV SW OUT KVA */
	u64 lmemhdrKTargetAddress[IS_MAX_PLANES];	/* LME HDR MV SW OUT KVA*/
	u32 ixcTargetAddress[IS_MAX_PLANES];		/* DNS YUV out WDMA */
	u32 dva_byrp_hdr[IS_MAX_PLANES];		/* BYRP HDR RDMA */
	u32 ixpTargetAddress[IS_MAX_PLANES];
	u32 ixdgrTargetAddress[IS_MAX_PLANES];		/* DNS DRC GRID RDMA */
	u32 ixrrgbTargetAddress[IS_MAX_PLANES];		/* DNS REP RGB RDMA */
	u32 ixnoirTargetAddress[IS_MAX_PLANES];		/* DNS REP NOISE RDMA */
	u32 ixscmapTargetAddress[IS_MAX_PLANES];	/* DNS SC MAP RDMA */
	u32 ixnrdsTargetAddress[IS_MAX_PLANES];		/* DNS NR DS WDMA */
	u32 ixnoiTargetAddress[IS_MAX_PLANES];		/* DNS NOISE WDMA */
	u32 ixdgaTargetAddress[IS_MAX_PLANES];		/* DNS DRC GAIN WDMA */
	u32 ixsvhistTargetAddress[IS_MAX_PLANES];	/* DNS SV HIST WDMA */
	u32 ixhfTargetAddress[IS_MAX_PLANES];		/* DNS HF WDMA */
	u32 ixwrgbTargetAddress[IS_MAX_PLANES];		/* DNS REP RGB WDMA */
	u32 ixnoirwTargetAddress[IS_MAX_PLANES];	/* DNS REP NOISE WDMA */
	u32 ixbnrTargetAddress[IS_MAX_PLANES];		/* DNS BNR WDMA */
	u32 ixvTargetAddress[IS_MAX_PLANES];		/* TNR PREV WDMA */
	u32 ixwTargetAddress[IS_MAX_PLANES];		/* TNR PREV weight WDMA */
	u32 ixtTargetAddress[IS_MAX_PLANES];		/* TNR PREV RDMA */
	u32 ixgTargetAddress[IS_MAX_PLANES];		/* TNR PREV weight RDMA */
	u32 ixmTargetAddress[IS_MAX_PLANES];		/* TNR MOTION RDMA */
	u64 ixmKTargetAddress[IS_MAX_PLANES];		/* TNR MOTION KVA */
	u64 mexcTargetAddress[IS_MAX_PLANES];		/* ME or MCH out DMA */
	u64 orbxcKTargetAddress[IS_MAX_PLANES];		/* ME or MCH out DMA */
	u32 ypnrdsTargetAddress[IS_MAX_PLANES];		/* YPP Y/UV L2 RDMA */
	u32 ypnoiTargetAddress[IS_MAX_PLANES];		/* YPP NOISE RDMA */
	u32 ypdgaTargetAddress[IS_MAX_PLANES];		/* YPP DRC GAIN RDMA */
	u32 ypsvhistTargetAddress[IS_MAX_PLANES];	/* YPP SV HIST RDMA */
	u32 sc0TargetAddress[IS_MAX_PLANES];
	u32 sc1TargetAddress[IS_MAX_PLANES];
	u32 sc2TargetAddress[IS_MAX_PLANES];
	u32 sc3TargetAddress[IS_MAX_PLANES];
	u32 sc4TargetAddress[IS_MAX_PLANES];
	u32 sc5TargetAddress[IS_MAX_PLANES];
	u32 clxsTargetAddress[IS_MAX_PLANES];		/* CLAHE IN DMA */
	u32 clxcTargetAddress[IS_MAX_PLANES];		/* CLAHE OUT DMA */
	u32 dva_mcfp_prev_yuv[IS_MAX_PLANES];		/* MCFP PREV YUV RDMA */
	u32 dva_mcfp_prev_wgt[IS_MAX_PLANES];		/* MCFP PREV WGT RDMA */
	u32 dva_mcfp_cur_drc[IS_MAX_PLANES];		/* MCFP CUR DRC RDMA */
	u32 dva_mcfp_prev_drc[IS_MAX_PLANES];		/* MCFP PREV DRC RDMA */
	u32 dva_mcfp_motion[IS_MAX_PLANES];		/* MCFP MOTION RDMA */
	u64 kva_mcfp_motion[IS_MAX_PLANES];		/* MCFP MOTION RDMA */
	u32 dva_mcfp_sat_flag[IS_MAX_PLANES];		/* MCFP SAT FLAG RDMA */
	u32 dva_mcfp_wgt[IS_MAX_PLANES];		/* MCFP WGT WDMA */
	u32 dva_mcfp_yuv[IS_MAX_PLANES];		/* MCFP YUV WDMA */
	u32 dva_rgbp_hf[IS_MAX_PLANES];			/* RGBP HF WDMA*/
	u32 dva_rgbp_sf[IS_MAX_PLANES];			/* RGBP SF WDMA */
	u32 dva_rgbp_yuv[IS_MAX_PLANES];		/* RGBP YUV WDMA*/
	u32 dva_rgbp_rgb[IS_MAX_PLANES];		/* RGBP RGB WDMA */

	/* Logical node information */
	struct is_sub_node	out_node;
	struct is_sub_node	cap_node;

	/* multi-buffer use */
	/* total number of buffers per frame */
	u32			num_buffers;
	/* current processed buffer index */
	u32			cur_buf_index;
	/* total number of shots per frame */
	u32			num_shots;
	/* current processed buffer index */
	u32			cur_shot_idx;

	/* internal use */
	unsigned long		mem_state;
	u32			state;
	u32			fcount;
	u32			rcount;
	u32			index;
	u32			result;
	unsigned long		out_flag;
	unsigned long		bak_flag;
	IS_DECLARE_PMAP(pmap);

	struct is_frame_info frame_info[MAX_FRAME_INFO];
	u32			instance; /* device instance */
	u32			type;
	unsigned long		core_flag;
	atomic_t		shot_done_flag;

#ifdef ENABLE_SYNC_REPROCESSING
	struct list_head	sync_list;
	struct list_head	preview_list;
#endif
	struct list_head	votf_list;

	/* for NI(noise index from DDK) use */
	u32			noise_idx; /* Noise index */

#ifdef MEASURE_TIME
	/* time measure externally */
	struct timespec64	*tzone;
	/* time measure internally */
	struct is_monitor	mpoint[TMS_END];
#endif

	/* for draw digit */
	u32			width;
	u32			height;

	struct is_stripe_info	stripe_info;

	struct pablo_rta_frame_info prfi;
};

