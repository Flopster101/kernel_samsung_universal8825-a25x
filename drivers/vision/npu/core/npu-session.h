/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_SESSION_H_
#define _NPU_SESSION_H_

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/sched.h>

#include "vs4l.h"

#include "npu-vertex.h"
#include "npu-queue.h"
#include "npu-config.h"
#include "npu-common.h"
#include "npu-scheduler.h"
#include "npu-if-session-protodrv.h"
#include "ncp_header.h"
#include "drv_usr_if.h"

#define FM_SHARED_NUM	1
#define MAX_USER_KERNEL 2

#ifdef CONFIG_NPU_USE_HW_DEVICE
#define NPU_SESSION_DSP_MAGIC	0xC0DEC0DE
#endif

#ifdef CONFIG_DSP_USE_VS4L
#include "dsp-common-type.h"
#endif

enum channel_flag {
	MEDIUM_CHANNEL = 0,
	HIGH_CHANNEL = 1,
};

enum npu_session_state {
	NPU_SESSION_STATE_OPEN,
	NPU_SESSION_STATE_REGISTER,
	NPU_SESSION_STATE_GRAPH_ION_MAP,
	NPU_SESSION_STATE_WGT_KALLOC,
	NPU_SESSION_STATE_IOFM_KALLOC,
	NPU_SESSION_STATE_IOFM_ION_ALLOC,
	NPU_SESSION_STATE_IMB_ION_ALLOC,
	NPU_SESSION_STATE_FORMAT_IN,
	NPU_SESSION_STATE_FORMAT_OT,
	NPU_SESSION_STATE_START,
	NPU_SESSION_STATE_STOP,
	NPU_SESSION_STATE_CLOSE,
};

struct ion_info {
	dma_addr_t		daddr;
	void				*vaddr;
	size_t			size;
};

struct ncp_info {
	u32 address_vector_cnt;
	struct addr_info ncp_addr;
};

#define NPU_Q_TIMEDIFF_WIN_MAX 5

struct npu_session {
	u32 uid;
	u32 frame_id;
#ifdef CONFIG_NPU_USE_HW_DEVICE
	u32 unified_id;
	u64 unified_op_id;
#endif
	void *cookie;
	void *memory;
	void *exynos;
	struct npu_vertex_ctx vctx;
	const struct npu_session_ops	*gops;
	int (*undo_cb)(struct npu_session *);

	unsigned long ss_state;
#ifdef CONFIG_NPU_USE_HW_DEVICE
	u32 hids;
#endif
	u32 IFM_cnt;
	u32 OFM_cnt;
	u32 IMB_cnt;
	u32 WGT_cnt;
	u32 IOFM_cnt;
	size_t IMB_size;

	struct ncp_info ncp_info;

	// dynamic allocation, free is required, for ion alloc memory
	struct npu_memory_buffer *ncp_mem_buf;
	struct npu_memory_buffer *ncp_hdr_buf;
	struct npu_memory_buffer *ncp_payload;
	struct npu_memory_buffer *IOFM_mem_buf; // VISION_MAX_CONTAINERLIST * VISION_MAX_BUFFER
	struct npu_memory_buffer *IMB_mem_buf; // IMB_cnt
	struct npu_memory_buffer *sec_mem_buf; // secure memory

	// kmalloc, not ion alloc
	struct av_info IFM_info[VISION_MAX_CONTAINERLIST][VISION_MAX_BUFFER];
	struct av_info OFM_info[VISION_MAX_CONTAINERLIST][VISION_MAX_BUFFER];
	struct addr_info *IMB_info;
	struct addr_info *WGT_info;

	int qbuf_IOFM_idx; // IOFM Buffer index
	int dqbuf_IOFM_idx; // IOFM Buffer index

	u32 FIRMWARE_DRAM_TRANSFER; // dram transfer or sram transfer for firmware

	struct npu_nw_sched_param sched_param;/* For NW priority and boundness */

	struct nw_result nw_result;
	wait_queue_head_t wq;

	struct npu_frame control;
	struct npu_frame *current_frame;
	struct mutex *global_lock;

	struct mbox_process_dat mbox_process_dat;

	pid_t	pid;
	char	comm[TASK_COMM_LEN];
	pid_t	p_pid;
	char	p_comm[TASK_COMM_LEN];

	u32 address_vector_offset;
	u32 address_vector_cnt;
	u32 memory_vector_offset;
	u32 memory_vector_cnt;

	u32 llc_ways;
#ifdef CONFIG_DSP_USE_VS4L
	/* DSP kernel */
	unsigned int			kernel_count;
	struct list_head		kernel_list;
	void				*kernel_name;
	struct dsp_dl_lib_info		*dl_libs;
	bool				kernel_loaded;
	struct list_head update_list;
	u32 update_count;
	struct list_head buf_list;
	u32 buf_count;
	u32 global_id;
	struct dsp_common_mem_v4 buf_info[VISION_MAX_BUFFER];
	u32 user_kernel_count;
	void *user_kernel_elf[MAX_USER_KERNEL];
	u32 user_kernel_elf_size[MAX_USER_KERNEL];
#endif
#ifdef CONFIG_NPU_ARBITRATION
	unsigned long total_flc_transfer_size, total_sdma_transfer_size;
	u32	cmdq_isa_size;
	u32     inferencefreq_index;
	u64     last_q_time_stamp;
	u32     inferencefreq_window[NPU_Q_TIMEDIFF_WIN_MAX];        /* 0.01 unit */
	u32     inferencefreq;/* can be max, min, avg, or avg2 depending on policy, usually avg*/
#endif
#if (CONFIG_NPU_NCP_VERSION >= 25)
	char model_name[NCP_MODEL_NAME_LEN];
#endif
};

typedef int (*session_cb)(struct npu_session *);

struct npu_session_ops {
	int (*control)(struct npu_session *, struct npu_frame *);
	int (*request)(struct npu_session *, struct npu_frame *);
	int (*process)(struct npu_session *, struct npu_frame *);
	int (*cancel)(struct npu_session *, struct npu_frame *);
	int (*done)(struct npu_session *, struct npu_frame *);
	int (*get_resource)(struct npu_session *, struct npu_frame *);
	int (*put_resource)(struct npu_session *, struct npu_frame *);
	int (*update_param)(struct npu_session *, struct npu_frame *);
};

struct addr_info *find_addr_info(struct npu_session *session, u32 av_index, mem_opt_e mem_type, u32 cl_index);
npu_errno_t chk_nw_result_no_error(struct npu_session *session);
int npu_session_open(struct npu_session **session, void *cookie, void *memory);
int npu_session_close(struct npu_session *session);
int npu_session_s_graph(struct npu_session *session, struct vs4l_graph *info);
int npu_session_param(struct npu_session *session, struct vs4l_param_list *plist);
int npu_session_nw_sched_param(struct npu_session *session, struct vs4l_sched_param *param);
void npu_session_ion_sync_for_device(struct npu_memory_buffer *pbuf, off_t offset, size_t size,
					enum dma_data_direction dir);
int npu_session_NW_CMD_LOAD(struct npu_session *session);
int npu_session_NW_CMD_UNLOAD(struct npu_session *session);
int npu_session_NW_CMD_STREAMON(struct npu_session *session);
int npu_session_NW_CMD_STREAMOFF(struct npu_session *session);
#ifdef CONFIG_NPU_USE_BOOT_IOCTL
#ifdef CONFIG_NPU_USE_HW_DEVICE
int npu_session_NW_CMD_SUSPEND(struct npu_session *session);
int npu_session_NW_CMD_RESUME(struct npu_session *session);
#endif
int npu_session_NW_CMD_POWER_NOTIFY(struct npu_session *session, bool on);
#endif
int npu_session_register_undo_cb(struct npu_session *session, session_cb cb);
int npu_session_execute_undo_cb(struct npu_session *session);
int npu_session_undo_open(struct npu_session *session);
int npu_session_undo_s_graph(struct npu_session *session);
int npu_session_undo_close(struct npu_session *session);
int npu_session_flush(struct npu_session *session);
int kpi_frame_mbox_put(struct npu_frame *frame);
int npu_session_restore_cnt(struct npu_session *session);
void npu_session_restart(void);

#endif
