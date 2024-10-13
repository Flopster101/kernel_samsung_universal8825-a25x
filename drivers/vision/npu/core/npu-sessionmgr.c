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

#include "npu-sessionmgr.h"
#ifdef CONFIG_NPU_USE_HW_DEVICE
#include "npu-hw-device.h"
#endif

int npu_sessionmgr_probe(struct npu_sessionmgr *sessionmgr)
{
	int i = 0, ret = 0;
	mutex_init(&sessionmgr->mlock);

	for (i = 0; i < NPU_MAX_SESSION; i++) {
		sessionmgr->session[i] = NULL;
	}
#ifdef CONFIG_NPU_ARBITRATION
	sessionmgr->cumulative_flc_size = 0;
	sessionmgr->cumulative_sdma_size = 0;
	for (i = 0; i < NPU_MAX_CORES_ALLOWED; i++)
		sessionmgr->count_thread_ncp[i] = 0;
#endif
	atomic_set(&sessionmgr->session_cnt, 0);
#ifdef CONFIG_NPU_USE_HW_DEVICE
	atomic_set(&sessionmgr->dsp_hw_cnt, 0);
	atomic_set(&sessionmgr->npu_hw_cnt, 0);
	for (i = 0; i < NPU_MAX_SESSION; i++)
		sessionmgr->unified_id_map_cnt[i] = 0;

	ret = npu_util_bitmap_init(&sessionmgr->unified_id_map, "unified_bitmap", NPU_MAX_SESSION);
	if (ret)
		probe_err("npu_util_bitmap_init failed\n");
#endif

	return ret;
}

#ifdef CONFIG_NPU_USE_HW_DEVICE
int npu_sessionmgr_open(struct npu_sessionmgr *sessionmgr)
{
	int i = 0, ret = 0;

	for (i = 0; i < NPU_MAX_SESSION; i++)
		sessionmgr->unified_id_map_cnt[i] = 0;

	npu_util_bitmap_zero(&sessionmgr->unified_id_map);
	return ret;
}
#endif

/*
int npu_sessionmgr_close(struct npu_sessionmgr *sessionmgr)
{
	int ret = 0;


	return ret;
}

int npu_sessionmgr_start(struct npu_sessionmgr *sessionmgr)
{
	int ret = 0;

	return ret;
}

int npu_sessionmgr_stop(struct npu_sessionmgr *sessionmgr)
{
	int ret = 0;

	return ret;
}
*/

int npu_sessionmgr_regID(struct npu_sessionmgr *sessionmgr, struct npu_session *session)
{
	int ret = 0;
	u32 index;

	BUG_ON(!sessionmgr);
	BUG_ON(!session);

	mutex_lock(&sessionmgr->mlock);

	for (index = 0; index < NPU_MAX_SESSION; index++) {
		if (!sessionmgr->session[index]) {
			sessionmgr->session[index] = session;
			session->uid = index;
			atomic_inc(&sessionmgr->session_cnt);
			break;
		}
	}

	if (index >= NPU_MAX_SESSION)
		ret = -EINVAL;

	mutex_unlock(&sessionmgr->mlock);
	session->global_lock = &sessionmgr->mlock;

	return ret;
}

int npu_sessionmgr_unregID(struct npu_sessionmgr *sessionmgr, struct npu_session *session)
{
	BUG_ON(!sessionmgr);
	BUG_ON(!session);

	mutex_lock(&sessionmgr->mlock);
	sessionmgr->session[session->uid] = NULL;
	atomic_dec(&sessionmgr->session_cnt);
	mutex_unlock(&sessionmgr->mlock);

	return 0;
}

#ifdef CONFIG_NPU_USE_BOOT_IOCTL
int npu_sessionmgr_regHW(struct npu_session *session)
{
	int ret = 0;
	struct npu_sessionmgr *sessionmgr;

	BUG_ON(!session);

	mutex_lock(session->global_lock);
	sessionmgr = session->cookie;

#ifdef CONFIG_NPU_USE_HW_DEVICE
	if (session->hids & NPU_HWDEV_ID_NPU)
		atomic_inc(&sessionmgr->npu_hw_cnt);
	else
		atomic_inc(&sessionmgr->dsp_hw_cnt);
#endif

	mutex_unlock(session->global_lock);

	return ret;
}

int npu_sessionmgr_unregHW(struct npu_session *session)
{
	int ret = 0;
	struct npu_sessionmgr *sessionmgr;

	BUG_ON(!session);

	mutex_lock(session->global_lock);
	sessionmgr = session->cookie;

#ifdef CONFIG_NPU_USE_HW_DEVICE
	if (session->hids & NPU_HWDEV_ID_NPU)
		atomic_dec(&sessionmgr->npu_hw_cnt);
	else
		atomic_dec(&sessionmgr->dsp_hw_cnt);
#endif

	mutex_unlock(session->global_lock);

	return ret;
}
#endif

#ifdef CONFIG_NPU_USE_HW_DEVICE
int npu_sessionmgr_set_unifiedID(struct npu_session *session)
{
	int i = 0, ret = 0, id = -1;
	struct npu_sessionmgr *sessionmgr;

	WARN_ON(!session);
	mutex_lock(session->global_lock);
	sessionmgr = session->cookie;

	for (i = 0; i < NPU_MAX_SESSION; i++) {
		if (sessionmgr->session[i]) {
			if ((sessionmgr->session[i]->uid != session->uid) &&
					(sessionmgr->session[i]->unified_op_id == session->unified_op_id)) {
				session->unified_id = sessionmgr->session[i]->unified_id;
				id = sessionmgr->session[i]->unified_id;
				sessionmgr->unified_id_map_cnt[id]++;
				npu_info("reuse unified id = %d\n", session->unified_id);
				break;
			}
		}
	}

	if (id == -1) {
		id = npu_util_bitmap_set_region(&sessionmgr->unified_id_map, 1);
		if (id <= -1) {
			npu_err("Failed to allocate unified bitmap\n");
			ret = -ENODATA;
			mutex_unlock(session->global_lock);
			return ret;
		}
		session->unified_id = id;
		sessionmgr->unified_id_map_cnt[id]++;
		npu_info("create unified id = %d\n", session->unified_id);
	}
	mutex_unlock(session->global_lock);

	return ret;
}

void npu_sessionmgr_unset_unifiedID(struct npu_session *session)
{
	struct npu_sessionmgr *sessionmgr;

	WARN_ON(!session);
	mutex_lock(session->global_lock);

	sessionmgr = session->cookie;
	sessionmgr->unified_id_map_cnt[session->unified_id]--;
	if (sessionmgr->unified_id_map_cnt[session->unified_id]) {
		mutex_unlock(session->global_lock);
		return;
	}

	if (session->ss_state & BIT(NPU_SESSION_STATE_GRAPH_ION_MAP))
		npu_util_bitmap_clear_region(&sessionmgr->unified_id_map, session->unified_id, 1);

	mutex_unlock(session->global_lock);
}
#endif
