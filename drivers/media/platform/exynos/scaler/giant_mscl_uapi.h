/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */
#ifndef __MSCL_UAPI_H__
#define __MSCL_UAPI_H__

enum {
	MSCL_SRC_CFG = 0,
	MSCL_SRC_WH,
	MSCL_SRC_SPAN,
	MSCL_SRC_YPOS,
	MSCL_SRC_CPOS,
	MSCL_DST_CFG,
	MSCL_DST_WH,
	MSCL_DST_SPAN,
	MSCL_DST_POS,
	MSCL_V_RATIO,
	MSCL_H_RATIO,
	MSCL_ROT_CFG,
	MSCL_SRC_YH_IPHASE,
	MSCL_SRC_YV_IPHASE,
	MSCL_SRC_CH_IPHASE,
	MSCL_SRC_CV_IPHASE,

	MSCL_NR_CMDS
};

#define MSCL_MAX_PLANES 3

enum {
	MSCL_SRC = 0,
	MSCL_DST,
	MSCL_NR_DIRS
};

struct mscl_buffer {
	uint32_t count;
	uint32_t reserved;
	int dmabuf[MSCL_MAX_PLANES];
	int offset[MSCL_MAX_PLANES];
};

struct mscl_task {
	uint32_t cmd[MSCL_NR_CMDS];
	struct mscl_buffer buf[MSCL_NR_DIRS];
};

struct mscl_job {
	uint32_t version;
	uint32_t taskcount;
	struct mscl_task *tasks;
};

#define MSCL_IOC_JOB _IOW('M', 1, struct mscl_job)

#endif //__MSCL_UAPI_H__
