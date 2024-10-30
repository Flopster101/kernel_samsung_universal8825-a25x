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

#ifndef _NPU_SESSIONMGR_H_
#define _NPU_SESSIONMGR_H_

#include <linux/time.h>
#include "npu-session.h"
#include "npu-util-common.h"

#define NPU_MAX_SESSION 32
#define NPU_MAX_CORES_ALLOWED	11 /* 1 greather than max(10) to ease indexing */

struct npu_sessionmgr {
	struct npu_session *session[NPU_MAX_SESSION];
	unsigned long state;
#ifdef CONFIG_NPU_ARBITRATION
	unsigned long cumulative_flc_size;
	unsigned long cumulative_sdma_size;
	unsigned long count_thread_ncp[NPU_MAX_CORES_ALLOWED];
#endif
#ifdef CONFIG_NPU_USE_HW_DEVICE
	atomic_t npu_hw_cnt;
	atomic_t dsp_hw_cnt;
	u32 npu_hw_cnt_saved, dsp_hw_cnt_saved;
	struct npu_util_bitmap unified_id_map;
	u32 unified_id_map_cnt[NPU_MAX_SESSION];
#endif
	atomic_t session_cnt;
	struct mutex mlock;
};

int npu_sessionmgr_probe(struct npu_sessionmgr *sessionmgr);
/*
int npu_sessionmgr_start(struct npu_sessionmgr *sessionmgr);
int npu_sessionmgr_stop(struct npu_sessionmgr *sessionmgr);
int npu_sessionmgr_close(struct npu_sessionmgr *sessionmgr);
*/

int npu_sessionmgr_regID(struct npu_sessionmgr *sessionmgr, struct npu_session *session);
int npu_sessionmgr_unregID(struct npu_sessionmgr *sessionmgr, struct npu_session *session);
int npu_sessionmgr_regHW(struct npu_session *session);
int npu_sessionmgr_unregHW(struct npu_session *session);
#ifdef CONFIG_NPU_USE_HW_DEVICE
int npu_sessionmgr_open(struct npu_sessionmgr *sessionmgr);
int npu_sessionmgr_set_unifiedID(struct npu_session *session);
void npu_sessionmgr_unset_unifiedID(struct npu_session *session);
#endif

#endif
