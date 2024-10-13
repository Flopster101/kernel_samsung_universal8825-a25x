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

#ifndef IS_VIDEO_H
#define IS_VIDEO_H

#include <linux/version.h>
#include <media/v4l2-ioctl.h>
#include "is-video-config.h"
#include "is-type.h"
#include "pablo-mem.h"
#include "is-framemgr.h"
#include "is-metadata.h"
#include "is-config.h"

#define NUM_OF_META_PLANE	1
#define NUM_OF_EXT_PLANE	1

/* configuration by linux kernel version */

/*
 * sensor scenario (26 ~ 31 bit)
 */
#define SENSOR_SCN_MASK			0xFC000000
#define SENSOR_SCN_SHIFT		26

/*
 * stream type
 * [0] : No reprocessing type
 * [1] : reprocessing type
 * [2] : offline reprocessing type
 */
#define INPUT_STREAM_MASK		0x03000000
#define INPUT_STREAM_SHIFT		24

/*
 * senosr position
 * [0] : rear
 * [1] : front
 * [2] : rear2
 * [3] : secure
 */
#define INPUT_POSITION_MASK		0x00FF0000
#define INPUT_POSITION_SHIFT		16

/*
 * video index
 * [x] : connected capture video node index
 */
#define INPUT_VINDEX_MASK		0x0000FF00
#define INPUT_VINDEX_SHIFT		8

/*
 * input type
 * [0] : memory input
 * [1] : on the fly input
 * [2] : pipe input
 */
#define INPUT_INTYPE_MASK		0x000000F0
#define INPUT_INTYPE_SHIFT		4

/*
 * stream leader
 * [0] : No stream leader
 * [1] : stream leader
 */
#define INPUT_LEADER_MASK		0x0000000F
#define INPUT_LEADER_SHIFT		0

#define VIDEO_SSX_READY_BUFFERS		0
#define VIDEO_3XS_READY_BUFFERS		0
#define VIDEO_3XC_READY_BUFFERS		0
#define VIDEO_3XP_READY_BUFFERS		0
#define VIDEO_3XF_READY_BUFFERS		0
#define VIDEO_3XG_READY_BUFFERS		0
#define VIDEO_3XO_READY_BUFFERS		0
#define VIDEO_3XL_READY_BUFFERS		0
#define VIDEO_LME_READY_BUFFERS		0
#define VIDEO_LMEXS_READY_BUFFERS	0
#define VIDEO_LMEXC_READY_BUFFERS	0
#define VIDEO_IXS_READY_BUFFERS		0
#define VIDEO_IXC_READY_BUFFERS		0
#define VIDEO_IXP_READY_BUFFERS		0
#define VIDEO_IXT_READY_BUFFERS		0
#define VIDEO_IXG_READY_BUFFERS		0
#define VIDEO_IXV_READY_BUFFERS		0
#define VIDEO_IXW_READY_BUFFERS		0
#define VIDEO_MEXC_READY_BUFFERS	0
#define VIDEO_ORB_READY_BUFFERS		0
#define VIDEO_ORBXC_READY_BUFFERS	0
#define VIDEO_MXS_READY_BUFFERS		0
#define VIDEO_MXP_READY_BUFFERS		0
#define VIDEO_VRA_READY_BUFFERS		0
#define VIDEO_SSXVC0_READY_BUFFERS	0
#define VIDEO_SSXVC1_READY_BUFFERS	0
#define VIDEO_SSXVC2_READY_BUFFERS	0
#define VIDEO_SSXVC3_READY_BUFFERS	0
#define VIDEO_PAFXS_READY_BUFFERS	0
#define VIDEO_CLHXS_READY_BUFFERS	0
#define VIDEO_CLHXC_READY_BUFFERS	0
#define VIDEO_YPP_READY_BUFFERS		0
#define VIDEO_RGBP_READY_BUFFERS	0

#define EXYNOS_VIDEONODE_FIMC_IS	(100)
#define IS_VIDEO_NAME(name)		("exynos-is-"name)
#define IS_VIDEO_SSX_NAME		IS_VIDEO_NAME("ss")
#define IS_VIDEO_PRE_NAME		IS_VIDEO_NAME("pre")
#define IS_VIDEO_3XS_NAME(id)		IS_VIDEO_NAME("3"#id"s")
#define IS_VIDEO_3XC_NAME(id)		IS_VIDEO_NAME("3"#id"c")
#define IS_VIDEO_3XP_NAME(id)		IS_VIDEO_NAME("3"#id"p")
#define IS_VIDEO_3XF_NAME(id)		IS_VIDEO_NAME("3"#id"f")
#define IS_VIDEO_3XG_NAME(id)		IS_VIDEO_NAME("3"#id"g")
#define IS_VIDEO_3XO_NAME(id)		IS_VIDEO_NAME("3"#id"o")
#define IS_VIDEO_3XL_NAME(id)		IS_VIDEO_NAME("3"#id"l")
#define IS_VIDEO_IXS_NAME(id)		IS_VIDEO_NAME("i"#id"s")
#define IS_VIDEO_IXC_NAME(id)		IS_VIDEO_NAME("i"#id"c")
#define IS_VIDEO_IXP_NAME(id)		IS_VIDEO_NAME("i"#id"p")
#define IS_VIDEO_IXT_NAME(id)		IS_VIDEO_NAME("i"#id"t")
#define IS_VIDEO_IXW_NAME(id)		IS_VIDEO_NAME("i"#id"w")
#define IS_VIDEO_IXG_NAME(id)		IS_VIDEO_NAME("i"#id"g")
#define IS_VIDEO_IXV_NAME(id)		IS_VIDEO_NAME("i"#id"v")
#define IS_VIDEO_MEXC_NAME(id)		IS_VIDEO_NAME("me"#id"c")
#define IS_VIDEO_ORB_NAME(id)		IS_VIDEO_NAME("orb"#id)
#define IS_VIDEO_ORBXC_NAME(id)		IS_VIDEO_NAME("orb"#id"c")
#define IS_VIDEO_MXS_NAME(id)		IS_VIDEO_NAME("m"#id"s")
#define IS_VIDEO_MXP_NAME(id)		IS_VIDEO_NAME("m"#id"p")
#define IS_VIDEO_VRA_NAME		IS_VIDEO_NAME("vra")
#define IS_VIDEO_SSXVC0_NAME(id)	IS_VIDEO_NAME("ss"#id"vc0")
#define IS_VIDEO_SSXVC1_NAME(id)	IS_VIDEO_NAME("ss"#id"vc1")
#define IS_VIDEO_SSXVC2_NAME(id)	IS_VIDEO_NAME("ss"#id"vc2")
#define IS_VIDEO_SSXVC3_NAME(id)	IS_VIDEO_NAME("ss"#id"vc3")
#define IS_VIDEO_PAFXS_NAME(id)		IS_VIDEO_NAME("p"#id"s")
#define IS_VIDEO_CLHXS_NAME(id)		IS_VIDEO_NAME("cl"#id"s")
#define IS_VIDEO_CLHXC_NAME(id)		IS_VIDEO_NAME("cl"#id"c")
#define IS_VIDEO_YPP_NAME		IS_VIDEO_NAME("ypp")
#define IS_VIDEO_LME_NAME(id)		IS_VIDEO_NAME("lme"#id)
#define IS_VIDEO_LMEXS_NAME(id)		IS_VIDEO_NAME("lme"#id"s")
#define IS_VIDEO_LMEXC_NAME(id)		IS_VIDEO_NAME("lme"#id"c")

/* from kernel 4.20 version, some vb2 API parameter has changed */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
#define is_vb2_qbuf(vbq, buf)			vb2_qbuf(vbq, NULL, buf)
#define is_vb2_prepare_buf(vbq, buf)		vb2_prepare_buf(vbq, NULL, buf)
#define is_fill_vb2_buffer(vb, buf, planes)	fill_vb2_buffer(vb, planes)
#else
#define is_vb2_qbuf(vbq, buf)			vb2_qbuf(vbq, buf)
#define is_vb2_prepare_buf(vbq, buf)		vb2_prepare_buf(vbq, buf)
#define is_fill_vb2_buffer(vb, buf, planes)	fill_vb2_buffer(vb, buf, planes)
#endif

#define VIDEO_OUTPUT_DEVICE_CAPS	(V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE)
#define VIDEO_CAPTURE_DEVICE_CAPS	(V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
#define VFL_TYPE_PABLO	VFL_TYPE_VIDEO
#else
#define VFL_TYPE_PABLO	VFL_TYPE_GRABBER
#endif

struct is_device_ischain;
struct is_subdev;
struct is_queue;
struct is_video_ctx;
struct is_resourcemgr;

enum is_video_type {
	IS_VIDEO_TYPE_LEADER,
	IS_VIDEO_TYPE_CAPTURE,
};

enum is_video_state {
	IS_VIDEO_CLOSE,
	IS_VIDEO_OPEN,
	IS_VIDEO_S_INPUT,
	IS_VIDEO_S_FORMAT,
	IS_VIDEO_S_BUFS,
	IS_VIDEO_STOP,
	IS_VIDEO_START,
};

enum is_queue_state {
	IS_QUEUE_BUFFER_PREPARED,
	IS_QUEUE_BUFFER_READY,
	IS_QUEUE_STREAM_ON,
	IS_QUEUE_NEED_TO_REMAP,	/* need remapped DVA with specific attribute */
	IS_QUEUE_NEED_TO_KMAP,	/* need permanent KVA for image planes */
	IS_QUEUE_NEED_TO_EXTMAP,	/* need ext plane for thumbnai, histgram */
};

struct is_queue_ops {
	int (*start_streaming)(void *device, struct is_queue *iq);
	int (*stop_streaming)(void *device, struct is_queue *iq);
	int (*s_fmt)(void *device, struct is_queue *iq);
	int (*reqbufs)(void *device, struct is_queue *iq, u32 count);
};

struct is_video_ops {
	int (*qbuf)(struct is_video_ctx *ivc, struct v4l2_buffer *b);
	int (*dqbuf)(struct is_video_ctx *ivc, struct v4l2_buffer *b, bool blocking);
	int (*done)(struct is_video_ctx *ivc, u32 index, u32 state);
};

struct is_queue {
	struct vb2_queue		*vbq;
	const struct is_queue_ops	*qops;
	struct is_framemgr		framemgr;
	struct is_frame_cfg		framecfg;

	u32				buf_maxcount;
	u32				buf_rdycount;
	u32				buf_refcount;
	dma_addr_t			buf_dva[IS_MAX_BUFS][IS_MAX_PLANES];
	ulong				buf_kva[IS_MAX_BUFS][IS_MAX_PLANES];

	/* for debugging */
	u32				buf_req;
	u32				buf_pre;
	u32				buf_que;
	u32				buf_com;
	u32				buf_dqe;

	u32				id;
	char				name[IS_STR_LEN];
	unsigned long			state;

	/* for logical video node */
	u32				mode;
	struct is_sub_buf		in_buf[IS_MAX_BUFS];
	struct is_sub_buf		out_buf[IS_MAX_BUFS];
};

struct is_video_ctx {
	u32				instance; /* logical stream id */

	struct is_queue			queue;
	u32				refcount;
	unsigned long			state;

	void				*device;
	void				*next_device;
	struct is_subdev		*subdev;
	struct is_group			*group;
	struct is_video			*video;
	struct is_video_ops		vops;

#if defined(MEASURE_TIME) && defined(MONITOR_TIME)
	unsigned long long		time[TMQ_END];
	unsigned long long		time_total[TMQ_END];
	unsigned long			time_cnt;
#endif
};

struct is_video {
	u32				id;
	enum is_video_type		video_type;
	enum is_device_type		device_type;
	atomic_t			refcount;
	struct mutex			lock;

	u32				buf_rdycount;
	u32				group_id;
	size_t				group_ofs;
	u32				subdev_id;
	size_t				subdev_ofs;

	struct video_device		vd;
	struct is_resourcemgr		*resourcemgr;
	const struct vb2_ops		*vb2_ops;
	const struct vb2_mem_ops	*vb2_mem_ops;
	const struct is_vb2_buf_ops	*is_vb2_buf_ops;
	void				*alloc_ctx;
	struct device			*alloc_dev;

	struct semaphore		smp_multi_input;
	bool				try_smp;
};

/* vb2 operations */
int is_queue_setup(struct vb2_queue *vq,
		unsigned *nbuffers, unsigned *nplanes,
		unsigned sizes[], struct device *alloc_devs[]);
int is_buf_init(struct vb2_buffer *vb);
void is_buf_cleanup(struct vb2_buffer *vb);
int is_buf_prepare(struct vb2_buffer *vb);
void is_wait_prepare(struct vb2_queue *vbq);
void is_wait_finish(struct vb2_queue *vbq);
void is_buf_finish(struct vb2_buffer *vb);
int is_start_streaming(struct vb2_queue *vbq, unsigned int count);
void is_stop_streaming(struct vb2_queue *vbq);
void is_buf_queue(struct vb2_buffer *vb);

/* v4l2 file operations */
int is_video_open(struct file *file);
int is_video_close(struct file *file);
__poll_t is_video_poll(struct file *file, struct poll_table_struct *wait);
int is_video_mmap(struct file *file, struct vm_area_struct *vma);

int is_video_probe(struct is_video *video,
	char *video_name,
	u32 video_number,
	u32 vfl_dir,
	struct is_mem *mem,
	struct v4l2_device *v4l2_dev,
	const struct v4l2_file_operations *file_ops,
	const struct v4l2_ioctl_ops *ioctl_ops,
	const struct vb2_ops *vb2_ops);

/* v4l2 ioctl operations */
int is_vidioc_querycap(struct file *file, void *fh, struct v4l2_capability *cap);
int is_vidioc_g_fmt_mplane(struct file *file, void *fh, struct v4l2_format *f);
int is_vidioc_s_fmt_mplane(struct file *file, void *fh, struct v4l2_format *f);
int is_vidioc_reqbufs(struct file *file, void *fh, struct v4l2_requestbuffers *b);
int is_vidioc_querybuf(struct file *file, void *fh, struct v4l2_buffer *b);
int is_vidioc_qbuf(struct file *file, void *fh, struct v4l2_buffer *b);
int is_vidioc_dqbuf(struct file *file, void *fh, struct v4l2_buffer *b);
int is_vidioc_prepare_buf(struct file *file, void *fh, struct v4l2_buffer *b);
int is_vidioc_streamon(struct file *file, void *fh, enum v4l2_buf_type i);
int is_vidioc_streamoff(struct file *file, void *fh, enum v4l2_buf_type i);
int is_vidioc_s_input(struct file *file, void *fh, unsigned int i);
int is_vidioc_g_ctrl(struct file *file, void * fh, struct v4l2_control *a);
int is_vidioc_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *a);
int is_vidioc_s_ctrl(struct file *file, void * fh, struct v4l2_control *a);

struct v4l2_ioctl_ops *is_get_default_v4l2_ioctl_ops(void);

/* video operations */
int is_video_qbuf(struct is_video_ctx *vctx,
	struct v4l2_buffer *buf);
int is_video_dqbuf(struct is_video_ctx *vctx,
	struct v4l2_buffer *buf,
	bool blocking);
int is_video_buffer_done(struct is_video_ctx *vctx,
	u32 index, u32 state);

struct is_fmt *is_find_format(u32 pixelformat, u32 flags);

extern int is_ssx_video_probe(void *data);

extern int is_ssxvc0_video_probe(void *data);
extern int is_ssxvc1_video_probe(void *data);
extern int is_ssxvc2_video_probe(void *data);
extern int is_ssxvc3_video_probe(void *data);
extern int is_paf0s_video_probe(void *data);
extern int is_paf1s_video_probe(void *data);
extern int is_paf2s_video_probe(void *data);
extern int is_paf3s_video_probe(void *data);

/* 3AA or CSTAT */
int is_30s_video_probe(void *data);
int is_30c_video_probe(void *data); /* capture */
int is_30p_video_probe(void *data); /* preview */
int is_30f_video_probe(void *data); /* early-FD */
int is_30g_video_probe(void *data); /* DNG_OUT */
int is_30o_video_probe(void *data); /* orbds */
int is_30l_video_probe(void *data); /* lmeds */

int is_31s_video_probe(void *data);
int is_31c_video_probe(void *data); /* capture */
int is_31p_video_probe(void *data); /* preview */
int is_31f_video_probe(void *data); /* early-FD */
int is_31g_video_probe(void *data); /* DNG_OUT */
int is_31o_video_probe(void *data); /* orbds */
int is_31l_video_probe(void *data); /* lmeds */

int is_32s_video_probe(void *data);
int is_32c_video_probe(void *data); /* capture */
int is_32p_video_probe(void *data); /* preview */
int is_32f_video_probe(void *data); /* early-FD */
int is_32g_video_probe(void *data); /* DNG_OUT */
int is_32o_video_probe(void *data); /* orbds */
int is_32l_video_probe(void *data); /* lmeds */

int is_33s_video_probe(void *data);
int is_33c_video_probe(void *data); /* capture */
int is_33p_video_probe(void *data); /* preview */
int is_33f_video_probe(void *data); /* early-FD */
int is_33g_video_probe(void *data); /* DNG_OUT */
int is_33o_video_probe(void *data); /* orbds */
int is_33l_video_probe(void *data); /* lmeds */

/* LME */
int is_lme0_video_probe(void *data);
int is_lme0s_video_probe(void *data); /* MVF output */
int is_lme0c_video_probe(void *data); /* LME MOTION HW OUT(pure motion) */

/* ISP */
int is_i0s_video_probe(void *data);
int is_i0c_video_probe(void *data);

/* TNR */
int is_i0t_video_probe(void *data); /* MCFP/TNR PREV BAYER RDMA */
int is_i0g_video_probe(void *data); /* MCFP/TNR PREV WEIGHT RDMA */
int is_i0v_video_probe(void *data); /* MCFP/TNR PREV BAYER WDMA */
int is_i0w_video_probe(void *data); /* MCFP/TNR PREV WEIGHT WDMA */

/* ME */
int is_me0c_video_probe(void *data);/* me out */
int is_me1c_video_probe(void *data);/* me out */

/* ORB */
int is_orb0_video_probe(void *data);/* orb0 in */
int is_orb1_video_probe(void *data);/* orb1 in */
int is_orb0c_video_probe(void *data);/* orb0 out */
int is_orb1c_video_probe(void *data);/* orb1 out */

/* MCSC */
int is_m0s_video_probe(void *data);
int is_m1s_video_probe(void *data);

int is_m0p_video_probe(void *data);
int is_m1p_video_probe(void *data);
int is_m2p_video_probe(void *data);
int is_m3p_video_probe(void *data);
int is_m4p_video_probe(void *data);
int is_m5p_video_probe(void *data);

/* VRA */
int is_vra_video_probe(void *data);

/* YUVPP */
int is_ypp_video_probe(void *data);

/* BYRP */
int is_byrp_video_probe(void *data);

/* RGBP */
int is_rgbp_video_probe(void *data);

/* MCFP */
int is_mcfp_video_probe(void *data);

/* YUVP */
int is_yuvp_video_probe(void *data);

typedef int(*ip_video_probe_t)(void *);

#define GET_VIDEO(vctx) 		(vctx ? (vctx)->video : NULL)
#define GET_QUEUE(vctx) 		(vctx ? &(vctx)->queue : NULL)
#define GET_FRAMEMGR(vctx)		(vctx ? &(vctx)->queue.framemgr : NULL)
#define GET_DEVICE(vctx)		(vctx ? (vctx)->device : NULL)
#ifdef CONFIG_USE_SENSOR_GROUP
#define GET_DEVICE_ISCHAIN(vctx)	(vctx ? (((vctx)->next_device) ? (vctx)->next_device : (vctx)->device) : NULL)
#else
#define GET_DEVICE_ISCHAIN(vctx)	GET_DEVICE(vctx)
#endif
#define CALL_QOPS(q, op, args...)	(((q)->qops->op) ? ((q)->qops->op(args)) : 0)
#define CALL_VOPS(v, op, args...)	((v) && ((v)->vops.op) ? ((v)->vops.op(v, args)) : 0)
#endif


#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct is_fmt *pablo_kunit_get_is_formats_struct(ulong index);
ulong pablo_kunit_get_array_size_is_formats(void);
#endif
