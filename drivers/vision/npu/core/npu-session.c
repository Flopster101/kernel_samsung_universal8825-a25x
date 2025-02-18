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

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
//#include "lib/vpul-ds.h"
#include "npu-log.h"
#include "npu-device.h"
#include "npu-common.h"
#include "npu-session.h"
#include "npu-system.h"
#include "npu-scheduler.h"
#include "npu-util-common.h"
#ifdef CONFIG_DSP_USE_VS4L
#include "npu-hw-device.h"
#include "dsp-kernel.h"
#include "dsp-dhcp.h"
#include "dl/dsp-dl-engine.h"
#endif

#include <asm/cacheflush.h>

#ifdef NPU_LOG_TAG
#undef NPU_LOG_TAG
#endif
#define NPU_LOG_TAG		"npu-sess"

//#define SYSMMU_FAULT_TEST
#define KPI_FRAME_MAX_BUFFER		16
struct npu_frame kpi_frame[KPI_FRAME_MAX_BUFFER];
#define KPI_FRAME_PRIORITY	200
const struct npu_session_ops npu_session_ops;

/*****************************************************************************
 *****                         wrapper function                          *****
 *****************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static void __npu_session_ion_sync_for_device(struct npu_memory_buffer *pbuf,
					__attribute__((unused))off_t offset,
					__attribute__((unused))size_t size,
						enum dma_data_direction dir)
{
	if (dir == DMA_TO_DEVICE)
		dma_sync_sg_for_device(pbuf->attachment->dev,
			pbuf->sgt->sgl, pbuf->sgt->orig_nents, DMA_TO_DEVICE);
	else // dir == DMA_FROM_DEVICE
		dma_sync_sg_for_cpu(pbuf->attachment->dev,
			pbuf->sgt->sgl, pbuf->sgt->orig_nents, DMA_FROM_DEVICE);
}
#else
static void __npu_session_ion_sync_for_device(struct npu_memory_buffer *pbuf,
			off_t offset, size_t size, enum dma_data_direction dir)
{
	__dma_map_area(pbuf->vaddr + offset, size, dir);
}
#endif

#ifdef CONFIG_DSP_USE_VS4L
static int __remove_buf_from_load_msg(struct npu_session *session) {
	struct npu_memory_buffer *buf, *temp;

	if (!session->buf_count) return 0;

	list_for_each_entry_safe(buf, temp, &session->buf_list, dsp_list) {
		npu_info("load unmap fd : %d\n", buf->fd);
		npu_memory_unmap(session->memory, buf);
		list_del(&buf->dsp_list);
		kfree(buf);
		session->buf_count--;
	}
	if (session->buf_count) {
		npu_err("fail to remove buf in buf list.(%d)\n",session->buf_count);
		return -1;
	}
	return 0;
}

static int __remove_buf_from_exe_msg(struct npu_session *session) {
	struct npu_memory_buffer *buf, *temp;

	if (!session->update_count) return 0;

	list_for_each_entry_safe(buf, temp, &session->update_list, dsp_list) {
		npu_info("exe unmap fd : %d\n", buf->fd);
		npu_memory_unmap(session->memory, buf);
		list_del(&buf->dsp_list);
		kfree(buf);
		session->update_count--;
	}
	if (session->update_count) {
		npu_err("fail to remove buf in update list.(%d)\n",session->update_count);
		return -1;
	}
	return 0;
}
#endif

struct addr_info *find_addr_info(struct npu_session *session, u32 av_index, mem_opt_e mem_type, u32 cl_index)
{
	struct addr_info *ret = NULL;
	u32 i;
	u32 FM_cnt;
	struct addr_info *addr_info = NULL;

	switch (mem_type) {
	case OFM_TYPE:
		FM_cnt = session->OFM_cnt;
		/* Only 0th buffer given for comparison */
		addr_info = session->OFM_info[cl_index][0].addr_info;
		break;
	case IMB_TYPE:
		FM_cnt = session->IMB_cnt;
		addr_info = session->IMB_info;
		break;
	default:
		npu_uerr("Invalid mem_type : %d\n", session, mem_type);
		ret = NULL;
		goto err_exit;
	}
	for (i = 0; i < FM_cnt; i++) {
		if (unlikely((addr_info + i)->av_index == av_index)) {
			ret = addr_info + i;
			break;
		}
	}
err_exit:
	return ret;
}

npu_errno_t chk_nw_result_no_error(struct npu_session *session)
{
	return session->nw_result.result_code;
}

int npu_session_save_result(struct npu_session *session, struct nw_result nw_result)
{
	session->nw_result = nw_result;
	wake_up(&session->wq);
	return 0;
}

void npu_session_queue_done(struct npu_queue *queue, struct vb_container_list *inclist, struct vb_container_list *otclist, unsigned long flag)
{
	u32 buff_cnt;
	buff_cnt = otclist->containers[0].count;
	atomic_inc(&otclist->containers[0].completed_count);
	if (likely(atomic_read(&otclist->containers[0].completed_count) >= buff_cnt))
		npu_queue_done(queue, inclist, otclist, flag);
	return;
}

static save_result_func get_notify_func(const nw_cmd_e nw_cmd)
{
	if (unlikely(NPU_NW_CMD_CLEAR_CB == nw_cmd))
		return NULL;
	else
		return npu_session_save_result;
}

static int npu_session_put_nw_req(struct npu_session *session, nw_cmd_e nw_cmd)
{
	int ret = 0;
	struct npu_nw req = {
		.uid = session->uid,
		.bound_id = session->sched_param.bound_id,
		.npu_req_id = 0,
		.result_code = 0,
		.result_value = 0,
		.session = session,
		.cmd = nw_cmd,
		.ncp_addr = session->ncp_info.ncp_addr,
		.magic_tail = NPU_NW_MAGIC_TAIL,
	};

	BUG_ON(!session);

	req.notify_func = get_notify_func(nw_cmd);

	ret = npu_ncp_mgmt_put(&req);
	if (unlikely(!ret)) {
		npu_uerr("npu_ncp_mgmt_put failed", session);
		return ret;
	}
	return ret;
}

#ifdef CONFIG_NPU_USE_IMB_ALLOCATOR
static struct imb_size_control *imb_size_ctl_ref;

static inline struct npu_system *get_session_system(struct npu_session *session)
{
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_system *system;

	WARN_ON(!session);
	vctx = &session->vctx;
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);
	system = &device->system;
	return system;
}

static inline u32 __calc_imb_req_chunk(struct npu_memory_buffer *IMB_mem_buf)
{
	u32 req_chunk_cnt, req_size;

	WARN_ON(!IMB_mem_buf);

	req_size = IMB_mem_buf->ncp_max_size * 2;
	req_size = ALIGN(req_size, NPU_IMB_GRANULARITY);
	req_chunk_cnt = req_size / NPU_IMB_CHUNK_SIZE;
	if (unlikely(req_chunk_cnt > NPU_IMB_CHUNK_MAX_NUM))
		req_chunk_cnt = NPU_IMB_CHUNK_MAX_NUM;

	return req_chunk_cnt;
}

static int __npu_update_imb_size_save_result(struct npu_session *dummy_session, struct nw_result nw_result)
{
	int ret = 0;

	WARN_ON(!imb_size_ctl_ref);

	imb_size_ctl_ref->result_code = nw_result.result_code;
	imb_size_ctl_ref->fw_size = nw_result.nw.result_value;
	wake_up(&imb_size_ctl_ref->waitq);

	return ret;
}

static int __npu_update_imb_size(struct npu_session *session, u32 new_chunk_cnt)
{
	int ret_req;
	struct npu_system *system = get_session_system(session);
	struct npu_nw req = {
		.uid = session->uid,
		.cmd = NPU_NW_CMD_IMB_SIZE,
		.notify_func = __npu_update_imb_size_save_result,
		.param0 = new_chunk_cnt * NPU_IMB_CHUNK_SIZE,
	};

	WARN_ON(!system);
	imb_size_ctl_ref = &system->imb_size_ctl;
	imb_size_ctl_ref->result_code = NPU_SYSTEM_JUST_STARTED;

	/* queue the message for sending */
	ret_req = npu_ncp_mgmt_put(&req);
	if (unlikely(!ret_req)) {
		npu_uerr("IMB: npu_ncp_mgmt_put failed\n", session);
		return -EWOULDBLOCK;
	}

	/* wait for response from FW */
	wait_event(imb_size_ctl_ref->waitq, imb_size_ctl_ref->result_code != NPU_SYSTEM_JUST_STARTED);

	if (unlikely(imb_size_ctl_ref->result_code != NPU_ERR_NO_ERROR)) {
		/* firmware failed or did not accept the change in IMB size */
		npu_uerr("IMB: failure response for IMB_SIZE message, result code = %d\n",
			 session, imb_size_ctl_ref->result_code);
	}

	return imb_size_ctl_ref->result_code;
}

static int __fw_sync_CMD_IMB_SIZE(struct npu_session *session, u32 new_chunk_cnt)
{
	int ret;
	u32 fw_chunk_cnt;
	struct npu_system *system = get_session_system(session);

	WARN_ON(!system);
	ret = __npu_update_imb_size(session, new_chunk_cnt);
	if (likely(!ret)) {
		fw_chunk_cnt = system->imb_size_ctl.fw_size / NPU_IMB_CHUNK_SIZE;
		if (fw_chunk_cnt == new_chunk_cnt) {
			npu_udbg("IMB: new size 0x%X was accepted by FW\n", session, fw_chunk_cnt * NPU_IMB_CHUNK_SIZE);
		} else {
			npu_udbg("IMB: new size 0x%X was rejected by FW, current FW IMB size 0x%X\n",
				 session, new_chunk_cnt * NPU_IMB_CHUNK_SIZE, fw_chunk_cnt * NPU_IMB_CHUNK_SIZE);
		}
	} else {
		npu_uerr("IMB: sending IMB_SIZE command to FW failed\n", session);
	}

	return ret;
}

static int __alloc_imb_chunk(struct npu_session *session, struct npu_memory_buffer *IMB_mem_buf)
{
	int ret = 0;
	dma_addr_t dAddr;
	u32 i, prev_chunk_cnt;
	struct npu_system *system = get_session_system(session);
	u32 req_chunk_cnt = __calc_imb_req_chunk(IMB_mem_buf);

	WARN_ON(!IMB_mem_buf);

	mutex_lock(&system->imb_alloc_lock);

	prev_chunk_cnt = system->imb_alloc_data.alloc_chunk_cnt;
	for (i = system->imb_alloc_data.alloc_chunk_cnt; i < req_chunk_cnt; i++) {
		dAddr = system->chunk_imb->paddr + (i * NPU_IMB_CHUNK_SIZE);
		system->imb_alloc_data.chunk[i].size = NPU_IMB_CHUNK_SIZE;
		ret = npu_memory_alloc_from_heap(
			system->pdev, &(system->imb_alloc_data.chunk[i]), dAddr, &system->memory, "IMB", 0);
		if (unlikely(ret)) {
			npu_uerr("IMB: npu_memory_alloc_from_heap failed, err: %d\n", session, ret);
			ret = -EFAULT;
			goto lock_free;
		}
		system->imb_alloc_data.alloc_chunk_cnt++;
		npu_udbg("IMB: successfully allocated chunk = %u with size = 0x%X\n", session, i, NPU_IMB_CHUNK_SIZE);
	}

	/* in allocation case we're not interested in FW size acceptance */
	__fw_sync_CMD_IMB_SIZE(session, system->imb_alloc_data.alloc_chunk_cnt);

	if (prev_chunk_cnt != system->imb_alloc_data.alloc_chunk_cnt) {
		npu_udbg("IMB: system total size 0x%X -> 0x%X\n", session,
			 prev_chunk_cnt * NPU_IMB_CHUNK_SIZE,
			 system->imb_alloc_data.alloc_chunk_cnt * NPU_IMB_CHUNK_SIZE);
	}

	IMB_mem_buf->daddr = system->chunk_imb->paddr;

lock_free:
	mutex_unlock(&system->imb_alloc_lock);
	return ret;
}

static inline void __free_imb_chunk(struct npu_session *session, u32 new_chunk_cnt)
{
	struct npu_system *system = get_session_system(session);
	u32 i, cur_chunk_cnt;

	WARN_ON(!system);
	cur_chunk_cnt = system->imb_alloc_data.alloc_chunk_cnt;
	for (i = cur_chunk_cnt; i > new_chunk_cnt; i--) {
		npu_memory_free_from_heap(&system->pdev->dev, &system->memory, &(system->imb_alloc_data.chunk[i - 1]));
		system->imb_alloc_data.alloc_chunk_cnt--;
	}
	if (system->imb_alloc_data.alloc_chunk_cnt != cur_chunk_cnt) {
		npu_udbg("IMB: system total size 0x%X -> 0x%X\n", session,
			 cur_chunk_cnt * NPU_IMB_CHUNK_SIZE,
			 system->imb_alloc_data.alloc_chunk_cnt * NPU_IMB_CHUNK_SIZE);
	}
}

/* full - is used while release graph only, otherwise it's dequeue with guaranteed fw syncing */
static void __free_imb_mem_buf(struct npu_session *session, bool full)
{
	struct npu_session *session_ref;
	int ret;
	u32 idx, fw_chunk, max_next_req_chunk = 0, max_next_size = 0;
	struct npu_system *system = get_session_system(session);
	struct npu_sessionmgr *sessionmgr = session->cookie;

	WARN_ON(!sessionmgr);
	if (likely(session->IMB_mem_buf)) {
		mutex_lock(&system->imb_alloc_lock);

		for (idx = 0; idx < NPU_MAX_SESSION; idx++) {
			if (sessionmgr->session[idx]) {
				session_ref = sessionmgr->session[idx];
				if ((session != session_ref) && (session_ref->IMB_mem_buf) &&
				    (max_next_size < session_ref->IMB_mem_buf->ncp_max_size)) {
					max_next_size = session_ref->IMB_mem_buf->ncp_max_size;
					max_next_req_chunk = __calc_imb_req_chunk(session_ref->IMB_mem_buf);
				}
			}
		}

		if (likely(!full)) {
			/* if deque, deallocate not all memory but up to the THRESHOLD limit */
			max_next_req_chunk = max_next_req_chunk ?
				max_next_req_chunk : (NPU_IMB_THRESHOLD_SIZE / NPU_IMB_CHUNK_SIZE);
		}
		if (max_next_req_chunk < system->imb_alloc_data.alloc_chunk_cnt) {
			if (!full || max_next_req_chunk) {
				/* if deque or there are other non-closed sessions with buffers,
				 * we should sync it with FW
				 */
				ret = __fw_sync_CMD_IMB_SIZE(session, max_next_req_chunk);
				if (likely(!ret)) {
					fw_chunk = system->imb_size_ctl.fw_size / NPU_IMB_CHUNK_SIZE;
					/* deallocate that FW allows, FW may already use less memory,
					 * then we free it regardless of request
					 */
					__free_imb_chunk(session, fw_chunk);
				}
			} else {
				/* if we release graph and there no any sessions with buffers,
				 * deallocate it anyway without FW syncing
				 */
				npu_dbg("IMB: buffer full free w/o FW syncing\n");
				__free_imb_chunk(session, 0);
			}
		}

		mutex_unlock(&system->imb_alloc_lock);
	}
}
#endif

#ifdef CONFIG_DSP_USE_VS4L
struct dsp_common_execute_info_v4 *get_execution_info_for_dsp(struct npu_frame *frame);
int __update_iova_of_exe_message_of_dsp(struct npu_session *session, void *msg_kaddr);
#endif

int npu_session_put_frame_req(
	struct npu_session *session, struct npu_queue *queue,
	struct vb_container_list *incl, struct vb_container_list *otcl,
	frame_cmd_e frame_cmd, struct av_info *IFM_info, struct av_info *OFM_info, u32 j)
{
	int ret = 0;
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_vertex_ctx *vctx;
#ifdef CONFIG_DSP_USE_VS4L
	struct dsp_common_execute_info_v4 *exe_info;
#endif
	struct npu_frame frame = {
		.uid = session->uid,
		.frame_id = j+1,
		.npu_req_id = 0,	/* Determined in manager */
		.result_code = 0,
		.session = session,
		.cmd = frame_cmd,
		.priority = session->sched_param.priority,   /* Read priority passed by app */
		.src_queue = queue,
		.input = incl,
		.output = otcl,
		.magic_tail = NPU_FRAME_MAGIC_TAIL,
		.mbox_process_dat = session->mbox_process_dat,
		.IFM_info = IFM_info,
		.OFM_info = OFM_info,
	};
	struct npu_frame *f;

	if (is_kpi_mode_enabled(false)) {
		f = &(kpi_frame[incl->index % KPI_FRAME_MAX_BUFFER]);
		f->uid = session->uid;
		f->frame_id = j+1;
		f->npu_req_id = 0;       /* Determined in manager */
		f->result_code = 0;
		f->session = session;
		f->cmd = frame_cmd;
		/* setting high priority for KPI frames even though app may not have set */
		f->priority = KPI_FRAME_PRIORITY;
		f->src_queue = queue;
		f->input = incl;
		f->output = otcl;
		f->magic_tail = NPU_FRAME_MAGIC_TAIL;
		f->mbox_process_dat = session->mbox_process_dat;
		f->IFM_info = IFM_info;
		f->OFM_info = OFM_info;

		npu_dbg("Sending kpi frames to mailbox : uid %d, index %d/%d, frame id %d, frame 0x%x\n",
				session->uid, incl->index, otcl->index, j+1, f);
		session->current_frame = f;
		ret = kpi_frame_mbox_put(f);
		if (unlikely(!ret)) {
			npu_uerr("fail(%d) in kpi_frame_mobx_put\n",
					session, ret);
			ret = -EFAULT;
			goto p_err;
		}
       } else {
#ifdef CONFIG_DSP_USE_VS4L
		if (session->hids & NPU_HWDEV_ID_DSP) {
			exe_info = get_execution_info_for_dsp(&frame);
			__update_iova_of_exe_message_of_dsp(session, exe_info);
		}
#endif
		npu_dbg("Sending frames to mailbox : uid %d, index %d/%d, frame id %d\n",
				session->uid, incl->index, otcl->index, j+1);
		ret = npu_buffer_q_put(&frame);
		if (unlikely(!ret)) {
			npu_uerr("fail(%d) in npu_buffer_q_put\n",
					session, ret);
			ret = -EFAULT;
			goto p_err;
		}
	}
	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	npu_scheduler_set_init_freq(device, session->uid);
	npu_udbg("success in %s\n", session, __func__);

	return 0;
p_err:
	return ret;
}

int add_ion_mem(struct npu_session *session, struct npu_memory_buffer *mem_buf, mem_opt_e MEM_OPT)
{
	switch (MEM_OPT) {
	case NCP_TYPE:
		session->ncp_mem_buf = mem_buf;
		break;
	case NCP_HDR_TYPE:
		session->ncp_hdr_buf = mem_buf;
		break;
	case IOFM_TYPE:
		session->IOFM_mem_buf = mem_buf;
		break;
	case IMB_TYPE:
		session->IMB_mem_buf = mem_buf;
		break;
	default:
		break;
	}
	return 0;
}

static inline void __release_imb_mem_buf(struct npu_session *session)
{
	struct npu_memory_buffer *ion_mem_buf = session->IMB_mem_buf;

	if (likely(ion_mem_buf)) {
#ifndef CONFIG_NPU_USE_IMB_ALLOCATOR
		if (ion_mem_buf->daddr)
			npu_memory_free(session->memory, ion_mem_buf);
#else /* CONFIG_NPU_USE_IMB_ALLOCATOR */
		/* deallocate all memory chunks */
		__free_imb_mem_buf(session, true);
#endif
		session->IMB_mem_buf = NULL;
		kfree(ion_mem_buf);
	}
}

void __release_graph_ion(struct npu_session *session)
{
	struct npu_memory *memory;
	struct npu_memory_buffer *ion_mem_buf;

	memory = session->memory;
	ion_mem_buf = session->ncp_mem_buf;
	session->ncp_mem_buf = NULL;
	if (unlikely(ion_mem_buf)) {
		npu_memory_unmap(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->ncp_hdr_buf;
	session->ncp_hdr_buf = NULL;
	if (unlikely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->IOFM_mem_buf;
	session->IOFM_mem_buf = NULL;
	if (unlikely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	__release_imb_mem_buf(session);
}

int npu_session_NW_CMD_UNLOAD(struct npu_session *session)
{
	/* check npu_device emergency error */
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	nw_cmd_e nw_cmd = NPU_NW_CMD_UNLOAD;

	if (unlikely(!session)) {
		npu_err("invalid session\n");
		return -EINVAL;
	}

	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	if (unlikely(npu_device_is_emergency_err(device)))
		npu_udbg("NPU DEVICE IS EMERGENCY ERROR\n", session);

	/* Post unload command */
	npu_udbg("sending UNLOAD command.\n", session);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);

	if (session->ncp_payload) {
		npu_memory_free(session->memory, session->ncp_payload);
		kfree(session->ncp_payload);
		session->ncp_payload = NULL;
	}

	return 0;
}

int npu_session_close(struct npu_session *session)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
#ifdef CONFIG_NPU_USE_HW_DEVICE
	struct npu_sessionmgr *sessionmgr;
#endif

	BUG_ON(!session);

	npu_scheduler_unregister_session(session);

	session->ss_state |= BIT(NPU_SESSION_STATE_CLOSE);
	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);
#ifdef CONFIG_NPU_USE_HW_DEVICE
	sessionmgr = session->cookie;
#endif

#ifdef CONFIG_DSP_USE_VS4L
	if (session->hids & NPU_HWDEV_ID_DSP) {
		ret = __remove_buf_from_load_msg(session);
		if (ret)
			goto p_err;

		ret = __remove_buf_from_exe_msg(session);
		if (ret)
			goto p_err;

		dsp_graph_remove_kernel(device, session);
	}
#endif

	ret = npu_session_undo_close(session);
	if (ret)
		goto p_err;

#ifdef CONFIG_NPU_USE_HW_DEVICE
	npu_sessionmgr_unset_unifiedID(session);
#endif

	ret = npu_session_undo_open(session);
	if (ret)
		goto p_err;
	return ret;
p_err:
	npu_err("fail(%d) in npu_session_close\n", ret);
	return ret;
}

static void npu_session_process_register(struct npu_session *session)
{
	struct task_struct *parent;

	session->comm[0] = '\0';
	session->p_comm[0] = '\0';

	session->pid = task_pid_nr(current);

	strncpy(session->comm, current->comm, TASK_COMM_LEN);

	if (current->parent != NULL) {
		parent = current->parent;
		session->p_pid = task_pid_nr(parent);
		strncpy(session->p_comm, parent->comm, TASK_COMM_LEN);
	}

	npu_info("pid(%d), current(%s), parent pid(%d), parent(%s)\n",
		session->pid, session->comm, session->p_pid, session->p_comm);
}

int npu_session_open(struct npu_session **session, void *cookie, void *memory)
{
	int ret = 0;
	int i = 0;
	int k = 0;

	*session = kzalloc(sizeof(struct npu_session), GFP_KERNEL);
	if (unlikely(*session == NULL)) {
		npu_err("fail in npu_session_open kzalloc\n");
		ret = -ENOMEM;
		return ret;
	}

	(*session)->ss_state |= BIT(NPU_SESSION_STATE_OPEN);

	ret = npu_sessionmgr_regID(cookie, *session);
	if (unlikely(ret)) {
		npu_err("fail(%d) in npu_sessionmgr_regID\n", ret);
		npu_session_register_undo_cb(*session, npu_session_undo_open);
		goto p_err;
	}

	(*session)->ss_state |= BIT(NPU_SESSION_STATE_REGISTER);

	init_waitqueue_head(&((*session)->wq));

	(*session)->cookie = cookie;
	(*session)->memory = memory;
	(*session)->gops = &npu_session_ops;
	(*session)->control.owner = *session;

	(*session)->IMB_info = NULL;
	for (k = 0; k < VISION_MAX_CONTAINERLIST; k++) {
		for (i = 0; i < VISION_MAX_BUFFER; i++) {
			(*session)->IFM_info[k][i].addr_info = NULL;
			(*session)->IFM_info[k][i].state = SS_BUF_STATE_UNPREPARE;
			(*session)->OFM_info[k][i].addr_info = NULL;
			(*session)->OFM_info[k][i].state = SS_BUF_STATE_UNPREPARE;
		}
	}
	(*session)->ncp_mem_buf = NULL;
	(*session)->IOFM_mem_buf = NULL;
	(*session)->IMB_mem_buf = NULL;
	(*session)->sec_mem_buf = NULL;

	(*session)->qbuf_IOFM_idx = -1;
	(*session)->dqbuf_IOFM_idx = -1;
#ifdef CONFIG_NPU_USE_HW_DEVICE
	(*session)->hids = NPU_HWDEV_ID_MISC;
	(*session)->unified_id = 0;
	(*session)->unified_op_id = 0;
#endif

	/* Set default scheduling parameters */
	(*session)->sched_param.bound_id = NPU_BOUND_UNBOUND;
	(*session)->sched_param.priority = NPU_PRIORITY_MIN_VAL; /* lower priority */

	(*session)->pid	= task_pid_nr(current);

	npu_session_process_register(*session);

#ifdef CONFIG_DSP_USE_VS4L
	INIT_LIST_HEAD(&(*session)->kernel_list);
	INIT_LIST_HEAD(&(*session)->buf_list);
	INIT_LIST_HEAD(&(*session)->update_list);
	(*session)->buf_count = 0;
	(*session)->update_count = 0;
	(*session)->user_kernel_count = 0;
	for (i = 0; i < VISION_MAX_BUFFER; i++)
		(*session)->buf_info[i].fd = 0;
#endif
	ret = npu_scheduler_register_session(*session);
	if (unlikely(ret)) {
		npu_err("fail(%d) in register_session\n", ret);
		npu_session_register_undo_cb(*session, npu_session_undo_open);
		goto p_err;
	}

	return ret;
p_err:
	npu_session_execute_undo_cb(*session);
	return ret;
}

int _undo_s_graph_each_state(struct npu_session *session)
{
	u32 i = 0;
	u32 k = 0;
	struct npu_memory *memory;
	struct npu_memory_buffer *ion_mem_buf;
	struct addr_info *addr_info;

	BUG_ON(!session);

	memory = session->memory;

	if (session->ss_state <= BIT(NPU_SESSION_STATE_GRAPH_ION_MAP))
		goto graph_ion_unmap;
	if (session->ss_state <= BIT(NPU_SESSION_STATE_WGT_KALLOC))
		goto wgt_kfree;
	if (session->ss_state <= BIT(NPU_SESSION_STATE_IOFM_KALLOC))
		goto iofm_kfree;
	if (session->ss_state <= BIT(NPU_SESSION_STATE_IOFM_ION_ALLOC))
		goto iofm_ion_unmap;
	if (session->ss_state <= BIT(NPU_SESSION_STATE_IMB_ION_ALLOC))
		goto imb_ion_unmap;

imb_ion_unmap:
	addr_info = session->IMB_info;
	session->IMB_info = NULL;
	if (likely(addr_info))
		kfree(addr_info);

	__release_imb_mem_buf(session);

iofm_ion_unmap:
	ion_mem_buf = session->IOFM_mem_buf;
	session->IOFM_mem_buf = NULL;
	if (likely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

iofm_kfree:
	for (k = 0; k < VISION_MAX_CONTAINERLIST; k++) {
		for (i = 0; i < VISION_MAX_BUFFER; i++) {
			addr_info = session->IFM_info[k][i].addr_info;
			session->IFM_info[k][i].addr_info = NULL;
			if (likely(addr_info))
				kfree(addr_info);

			addr_info = session->OFM_info[k][i].addr_info;
			session->OFM_info[k][i].addr_info = NULL;
			if (likely(addr_info))
				kfree(addr_info);
		}
	}

wgt_kfree:
	addr_info = session->WGT_info;
	session->WGT_info = NULL;
	if (likely(addr_info))
		kfree(addr_info);

graph_ion_unmap:
	ion_mem_buf = session->ncp_mem_buf;
	session->ncp_mem_buf = NULL;
	if (likely(ion_mem_buf)) {
		npu_memory_unmap(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	ion_mem_buf = session->ncp_hdr_buf;
	session->ncp_hdr_buf = NULL;
	if (likely(ion_mem_buf)) {
		npu_memory_free(memory, ion_mem_buf);
		kfree(ion_mem_buf);
	}

	return 0;
}

int _undo_s_format_each_state(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (unlikely(session->ss_state < BIT(NPU_SESSION_STATE_IMB_ION_ALLOC))) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_FORMAT_OT)) {
		goto init_format;
	}

init_format:

p_err:
	return ret;
}

int _undo_streamon_each_state(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (unlikely(session->ss_state < BIT(NPU_SESSION_STATE_FORMAT_OT))) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_START))
		goto release_streamon;

release_streamon:

p_err:
	return ret;
}

int _undo_streamoff_each_state(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (unlikely(session->ss_state < BIT(NPU_SESSION_STATE_START))) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_STOP))
		goto release_streamoff;

release_streamoff:

p_err:
	return ret;
}

int _undo_close_each_state(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (unlikely(session->ss_state < BIT(NPU_SESSION_STATE_STOP))) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_CLOSE))
		goto session_close;

session_close:

p_err:
	return ret;
}

bool EVER_FIND_FM(u32 *FM_cnt, struct addr_info *FM_av, u32 address_vector_index)
{
	bool ret = false;
	u32 i;

	for (i = 0; i < (*FM_cnt) ; i++) {
		if ((FM_av + i)->av_index == address_vector_index) {
			ret = true;
			break;
		}
	}
	return ret;
}

void __set_unique_id(struct npu_session *session, struct drv_usr_share *usr_data)
{
	u32 uid = session->uid;
	usr_data->id = uid;
}

#ifdef CONFIG_NPU_USE_HW_DEVICE
int __set_unified_id(struct npu_session *session, struct drv_usr_share *usr_data)
{
	int ret = 0;
	char *ncp_vaddr;
	struct ncp_header *ncp;

	session->unified_op_id = usr_data->unified_op_id;
	ncp_vaddr = (char *)session->ncp_mem_buf->vaddr;
	ncp = (struct ncp_header *)ncp_vaddr;

	ret = npu_sessionmgr_set_unifiedID(session);

	return ret;
}
#endif

/*
int __get_ncp_bin_size(struct npu_binary *binary, char *ncp_path, char *ncp_name, size_t *ncp_size)
{
	int ret = 0;
	struct device dev_obj = {};
	struct device *dev = &dev_obj;

	ret = npu_binary_init(binary, dev, ncp_path, NCP_BIN_PATH, ncp_name);
	if (ret)
		goto p_err;

	ret = npu_binary_g_size(binary, ncp_size);
	if (ret)
		goto p_err;

	return ret;
p_err:
	npu_err("fail in __get_ncp_bin_size %d\n", ret);
	return ret;
}
*/

int __update_ncp_info(struct npu_session *session, struct npu_memory_buffer *ncp_mem_buf)
{
	if (!session) {
		npu_err("invalid session or ncp_mem_buf\n");
		return -EINVAL;
	}

	session->ncp_info.ncp_addr.size = ncp_mem_buf->size;
	session->ncp_info.ncp_addr.vaddr = ncp_mem_buf->vaddr;
	session->ncp_info.ncp_addr.daddr = ncp_mem_buf->daddr;
	return 0;
}

#ifdef CONFIG_DSP_USE_VS4L
void display_graph_info(struct dsp_common_graph_info_v4 *ginfo_src, int size_src)
{
	int i, j, n, size;
	unsigned int *ginfo;

	i = 0;
	ginfo = (unsigned int *)ginfo_src;
	size = size_src / sizeof(unsigned int);

	npu_info("### graph info : %p (%d)\n", ginfo, size);

	n = *(ginfo + i++);
	npu_info("global id : 0x%x %d\n", n, n);
	n = *(ginfo + i++);
	npu_info("tsgd # : 0x%x %d\n", n, n);
	n = *(ginfo + i++);
	npu_info("param # : 0x%x %d\n", n, n);
	n = *(ginfo + i++);
	npu_info("kernel # : 0x%x %d\n", n, n);

	while (i < size) {
		j = i;
		npu_info("DSP common param v4 %p (%ld)\n", ginfo + i, sizeof(struct dsp_common_param_v4) / sizeof(unsigned int));
		n = *(ginfo + j++);
		npu_info("\tparam type : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\tparam index (kernel id) : 0x%x %d\n", n, n);

		n = *(ginfo + j++);
		npu_info("\t\taddr type : 0x%x %d\n", (n >> 24) & 0xff, (n >> 24) & 0xff);
		npu_info("\t\tmem attr : 0x%x %d\n", (n >> 16) & 0xff, (n >> 16) & 0xff);
		npu_info("\t\tmem type : 0x%x %d\n", (n >> 8) & 0xff, (n >> 8) & 0xff);
		npu_info("\t\tmandatory : 0x%x %d\n", n & 0xff, n & 0xff);

		n = *(ginfo + j++);
		npu_info("\t\tsize : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\toffset : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\treserved : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 0 : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 1 : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 2 : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 3 : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 4 : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 5 : 0x%x %d\n", n, n);

		n = *(ginfo + j++);
		npu_info("\t\tfd : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tiova : 0x%x %d\n", n, n);

		i += (sizeof(struct dsp_common_param_v4) / sizeof(unsigned int));
		npu_info("i %d, j %d\n", i, j);
	}
}

void display_execute_info(struct dsp_common_execute_info_v4 *ginfo_src, int size_src)
{
	int i, j, n, size;
	unsigned int *ginfo;

	i = 0;
	ginfo = (unsigned int *)ginfo_src;
	size = size_src / sizeof(unsigned int);

	npu_info("### execute info : %p (%d)\n", ginfo, size);

	n = *(ginfo + i++);
	npu_info("global id : 0x%x %d\n", n, n);
	n = *(ginfo + i++);
	npu_info("update # : 0x%x %d\n", n, n);

	while (i < size) {
		j = i;
		npu_info("DSP common param v4 %p (%ld)\n", ginfo + i, sizeof(struct dsp_common_param_v4) / sizeof(unsigned int));
		n = *(ginfo + j++);
		npu_info("\tparam type : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\tparam index (kernel id) : 0x%x %d\n", n, n);

		n = *(ginfo + j++);
		npu_info("\t\taddr type : 0x%x %d\n", (n >> 24) & 0xff, (n >> 24) & 0xff);
		npu_info("\t\tmem attr : 0x%x %d\n", (n >> 16) & 0xff, (n >> 16) & 0xff);
		npu_info("\t\tmem type : 0x%x %d\n", (n >> 8) & 0xff, (n >> 8) & 0xff);
		npu_info("\t\tmandatory : 0x%x %d\n", n & 0xff, n & 0xff);

		n = *(ginfo + j++);
		npu_info("\t\tsize : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\toffset : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\treserved : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 0 : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 1 : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 2 : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 3 : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 4 : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tparam 5 : 0x%x %d\n", n, n);

		n = *(ginfo + j++);
		npu_info("\t\tfd : 0x%x %d\n", n, n);
		n = *(ginfo + j++);
		npu_info("\t\tiova : 0x%x %d\n", n, n);

		i += (sizeof(struct dsp_common_param_v4) / sizeof(unsigned int));
		npu_info("i %d, j %d\n", i, j);
	}
}

#define DBG_STR_ENTRY(idx) \
	[idx] = #idx

#define PARAM_INDENT    "    "
#define MEM_PARAM_WIDTH(param_mem) (param_mem->param[0])
#define MEM_PARAM_HEIGHT(param_mem) (param_mem->param[1])
#define MEM_PARAM_CHANNEL(param_mem) (param_mem->param[2])

const char *esd_get_mem_type_str(enum dsp_common_param_type param_type)
{
	static const char *p_type_str[] = {
		DBG_STR_ENTRY(DSP_COMMON_MEM_GRAPH_BIN),
		DBG_STR_ENTRY(DSP_COMMON_MEM_CMDQ_BIN),
		DBG_STR_ENTRY(DSP_COMMON_MEM_KERNEL_BIN_STR),
		DBG_STR_ENTRY(DSP_COMMON_MEM_IFM),
		DBG_STR_ENTRY(DSP_COMMON_MEM_OFM),
		DBG_STR_ENTRY(DSP_COMMON_MEM_IMB),
		DBG_STR_ENTRY(DSP_COMMON_MEM_WGT),
		DBG_STR_ENTRY(DSP_COMMON_MEM_BIAS),
		DBG_STR_ENTRY(DSP_COMMON_MEM_SCALAR),
		DBG_STR_ENTRY(DSP_COMMON_MEM_CUSTOM),
		DBG_STR_ENTRY(DSP_COMMON_MEM_NPU_IM),
		DBG_STR_ENTRY(DSP_COMMON_MEM_KERNEL_BIN),
		DBG_STR_ENTRY(DSP_COMMON_MEM_KERNEL_DESC),
	};

	static const char *EMPTY = "DSP_COMMON_MEM_EMPTY";
	static const char *UNKNOWN = "UNKNOWN";

	if (param_type == DSP_COMMON_MEM_EMPTY)
		return EMPTY;

	if (param_type >= ARRAY_SIZE(p_type_str))
		return UNKNOWN;

	return p_type_str[param_type];
}

static void esd_print_param_hdr(void)
{
    npu_info(PARAM_INDENT "%18s %6s | %9s %8s %8s %5s %8s %4s %10s %7s %4s %4s %4s\n",
             "pr_type", "pr_idx",
             "addr_type", "mem_attr", "mem_type", "manda", "size", "fd", "iova", "mem_off",
             "w", "h", "c");
}

static void esd_print_param_info(struct dsp_common_param_v4 *param)
{
    struct dsp_common_mem_v4 *mem = &param->param_mem;
    npu_info(PARAM_INDENT "%15s(%d) %6d | %9d %8d %8d %5s %#8x %4d %#10x %#7x %4d %4d %4d\n",
            esd_get_mem_type_str(param->param_type), param->param_type, param->param_index,
            mem->addr_type, mem->mem_attr, mem->mem_type, mem->is_mandatory ? "T" : "F",
            mem->size, mem->fd, mem->iova, mem->offset,
            MEM_PARAM_WIDTH(mem), MEM_PARAM_HEIGHT(mem), MEM_PARAM_CHANNEL(mem));
}
void esd_print_load_graph_info(struct dsp_common_graph_info_v4 *info)
{
    int i;
    npu_info("========================= ORCA Load Graph info ==========================\n");
    npu_info("graph_id(%#08x) n_tsgd(%d) n_param(%d) n_kernel(%d)\n",
            info->global_id, info->n_tsgd, info->n_param, info->n_kernel);

    esd_print_param_hdr();

    for (i = 0; i < (int)(info->n_param + 1); i++) { // +1 for TSGD.
        npu_info("Param[%d]\n", i);
        esd_print_param_info(&info->param_list[i]);
    }

    npu_info("--------------------------------------------------------------------------\n");
    return;
}

void esd_print_exec_graph_info(struct dsp_common_execute_info_v4 *info)
{
    int32_t i;
    npu_info("========================= ORCA Exec Graph info ==========================\n");
    npu_info("graph_id(%#08x) n_update_param(%d)\n", info->global_id, info->n_update_param);

    esd_print_param_hdr();

    for (i = 0; i < (int32_t)info->n_update_param; i++) {
        npu_info("Param[%d]\n", i);
        esd_print_param_info(&info->param_list[i]);
    }

    npu_info("--------------------------------------------------------------------------\n");
    return;
}

int __update_iova_of_load_message_of_dsp(struct npu_session *session){
	int i = 0;
	int ret = 0;
	int num_param = 0;
	struct dsp_dl_lib_file gkt;
	struct dsp_common_param_v4 *param = NULL;
	struct npu_memory_buffer *mem_buf = NULL;
	struct dsp_common_graph_info_v4 *load_msg =
		(struct dsp_common_graph_info_v4 *)session->ncp_mem_buf->vaddr;

	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;

	vctx = &session->vctx;
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	if (!load_msg) {
		npu_err("Pointer of load message is NULL.\n");
		ret = -ENOMEM;
		goto p_err;
	}

	num_param = load_msg->n_tsgd + load_msg->n_param;
	npu_info("param num : %d\n", num_param);

	for(i = 0; i < num_param; i++)
	{
		param = &load_msg->param_list[i];
		if (param->param_type == DSP_COMMON_MEM_EMPTY ||
			param->param_mem.fd < 0) {
			npu_dbg("%dth param is empty.\n", i);
			continue;
		}

		if (param->param_mem.mem_type == DSP_COMMON_NONE) {
			npu_dbg("%dth param memory type is none.\n", i);
			continue;
		}

		mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
		if (mem_buf == NULL) {
			npu_err("fail in npu_ion_map kzalloc\n");
			ret = -ENOMEM;
			goto p_err;
		}

		mem_buf->fd = param->param_mem.fd;
		mem_buf->size = param->param_mem.size;
		ret = npu_memory_map(session->memory, mem_buf, configs[NPU_PBHA_HINT_11]);
		if (unlikely(ret)) {
			npu_err("npu_memory_map is fail(%d).\n", ret);
			if (mem_buf)
				kfree(mem_buf);
			goto p_err;
		}
		param->param_mem.iova = mem_buf->daddr + param->param_mem.offset;
		npu_dbg("param daddr : (fd %d) 0x%x/%ld\n",
				mem_buf->fd, param->param_mem.iova, mem_buf->size);
		session->buf_count++;
		list_add_tail(&mem_buf->dsp_list, &session->buf_list);

		if (param->param_type == DSP_COMMON_MEM_KERNEL_DESC) {
			npu_info("description\n");
			gkt.mem = mem_buf->vaddr + param->param_mem.offset;
			gkt.size = mem_buf->size;
			gkt.r_ptr = 0;
			mutex_lock(session->global_lock);
			ret = dsp_dl_add_gkt(&gkt);
			mutex_unlock(session->global_lock);
			if (ret) {
				npu_err("failed to parse gkt.xml(%d).\n", ret);
				goto p_err;
			}
		}

		if (param->param_type == DSP_COMMON_MEM_KERNEL_BIN) {
			npu_info("kernel_bin\n");
			session->user_kernel_elf[session->user_kernel_count] = mem_buf->vaddr + param->param_mem.offset;
			session->user_kernel_elf_size[session->user_kernel_count] = mem_buf->size;
			session->user_kernel_count++;
		}
	}

	if (session->user_kernel_count) {
		ret = dsp_graph_add_kernel(device, session, session->kernel_name);
		if (ret) {
			npu_err("Failed to add user kernel\n");
			goto p_err;
		}
	}

	session->global_id = load_msg->global_id;

	//display_graph_info(load_msg, session->ncp_mem_buf->size);
	//esd_print_load_graph_info(load_msg);

#if (CONFIG_NPU_NCP_VERSION >= 25)
	strcpy(session->model_name, "dsp-kernel");
#endif
	return 0;
p_err:
	return ret;
}

int __update_dsp_input_daddr(struct npu_session *session, struct dsp_common_param_v4 *param){
	struct vb_queue *q;
	struct vb_container *container;
	int i, j, k, buf_fd, param_fd;

	if (!session)
		return -1;

	q = &session->vctx.queue.inqueue;
	param_fd = param->param_mem.fd;

	for(i = 0; i < VB_MAX_BUFFER; i++){
		if(!q->bufs[i]) continue;
		for (j = 0; j < q->bufs[i]->clist.count; ++j) {
			container = &q->bufs[i]->clist.containers[j];
			BUG_ON(!container->format);
			if (container->memory != VS4L_MEMORY_VIRTPTR) {
				for (k = 0; k < container->count; ++k) {
					buf_fd = container->buffers[k].m.fd;
					if (param_fd == buf_fd) {
						param->param_mem.iova =
							container->buffers[k].daddr +
							param->param_mem.offset;
						npu_dbg("input daddr : (fd %d) 0x%x\n",
								param_fd, param->param_mem.iova);
						return 0;
					}
				}
			}
		}
	}
	npu_err("failed to get input fd.\n");
	return -1;
}

int __update_dsp_output_daddr(struct npu_session *session, struct dsp_common_param_v4 *param){
	struct vb_queue *q;
	struct vb_container *container;
	int i, j, k, buf_fd, param_fd;

	if (!session)
		return -1;

	q = &session->vctx.queue.otqueue;
	param_fd = param->param_mem.fd ;

	for(i = 0; i < VB_MAX_BUFFER; i++){
		if(!q->bufs[i]) continue;
		for (j = 0; j < q->bufs[i]->clist.count; ++j) {
			container = &q->bufs[i]->clist.containers[j];
			BUG_ON(!container->format);
			if (container->memory != VS4L_MEMORY_VIRTPTR) {
				for (k = 0; k < container->count; ++k) {
					buf_fd = container->buffers[k].m.fd;
					if (param_fd == buf_fd) {
						param->param_mem.iova =
							container->buffers[k].daddr +
							param->param_mem.offset;
						npu_dbg("output daddr : (fd %d) 0x%x\n",
								param_fd, param->param_mem.iova);
						return 0;
					}
				}
			}
		}
	}
	npu_err("failed to get output fd.\n");
	return -1;
}

int __update_dsp_buffer_daddr(struct npu_session *session, struct dsp_common_param_v4 *param)
{
	int i = 0;
	int ret = 0;
	struct npu_memory_buffer *mem_buf = NULL;

	for (i = 0; i < VISION_MAX_BUFFER; i++) {
		if (session->buf_info[i].fd == param->param_mem.fd)
			return ret;
	}

	npu_dbg("buffer type is = %d\n", param->param_type);

	mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (mem_buf == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	mem_buf->fd = param->param_mem.fd;
	mem_buf->size = param->param_mem.size;
	ret = npu_memory_map(session->memory, mem_buf, configs[NPU_PBHA_HINT_11]);
	if (unlikely(ret)) {
		npu_err("npu_memory_map is fail(%d).\n", ret);
		kfree(mem_buf);
		return ret;
	}
	param->param_mem.iova = mem_buf->daddr + param->param_mem.offset;
	npu_dbg("param daddr : (fd %d) 0x%x/%ld\n",
			mem_buf->fd, param->param_mem.iova, mem_buf->size);
	list_add_tail(&mem_buf->dsp_list, &session->update_list);
	session->buf_info[session->update_count].fd = param->param_mem.fd;
	session->update_count++;
	return ret;
}

int __update_iova_of_exe_message_of_dsp(struct npu_session *session, void *msg_kaddr){
	int i = 0;
	int ret = 0;
	int num_param = 0;
	struct dsp_common_param_v4 *param = NULL;
	struct dsp_common_execute_info_v4 *exe_msg =
		(struct dsp_common_execute_info_v4 *)msg_kaddr;
	if (!exe_msg) {
		npu_err("Pointer of execution message is NULL.\n");
		ret = -ENOMEM;
		goto p_err;
	}

	num_param = exe_msg->n_update_param;
	npu_dbg("param num : %d\n", num_param);

	for(i = 0; i < num_param; i++)
	{
		param = &exe_msg->param_list[i];
		if (param->param_type == DSP_COMMON_MEM_EMPTY ||
			param->param_mem.fd < 0) {
			npu_dbg("%dth param is empty.\n", i);
			continue;
		}

		if (param->param_mem.mem_type == DSP_COMMON_NONE) {
			npu_dbg("%dth param memory type is none.\n", i);
			continue;
		}

		if (param->param_type == DSP_COMMON_MEM_IFM) {
			npu_dbg("%dth param is ifm.\n", i);
			ret = __update_dsp_input_daddr(session, param);
			if (ret < 0) {
				npu_err("fail to find input device virtual address.\n");
				ret = -ENOMEM;
				goto p_err;
			}
			continue;
		}

		if (param->param_type == DSP_COMMON_MEM_OFM) {
			npu_dbg("%dth param is ofm.\n", i);
			ret = __update_dsp_output_daddr(session, param);
			if (ret < 0) {
				npu_err("fail to find output device virtual address.\n");
				ret = -ENOMEM;
				goto p_err;
			}
			continue;
		}

		ret = __update_dsp_buffer_daddr(session, param);
		if (ret < 0) {
			npu_err("can not update buffer daddr\n");
			ret = -ENOMEM;
			goto p_err;
		}
	}

	//esd_print_exec_graph_info(exe_msg);
	return 0;
p_err:
	return ret;
}
#endif

static struct npu_memory_buffer *
copy_ncp_header(struct npu_session *session, struct npu_memory_buffer *ncp_mem_buf)
{
	struct npu_memory_buffer *buffer;
	struct ncp_header *src_hdr = ncp_mem_buf->vaddr;

	buffer = npu_memory_copy(session->memory, ncp_mem_buf, 0, src_hdr->hdr_size);
	if (IS_ERR_OR_NULL(buffer))
		return buffer;

	add_ion_mem(session, buffer, NCP_HDR_TYPE);

	return buffer;
}

int __ncp_ion_map(struct npu_session *session, struct drv_usr_share *usr_data)
{
	int ret = 0;

	struct npu_memory_buffer *ncp_mem_buf = NULL, *ncp_hdr_buf = NULL;
	mem_opt_e opt = NCP_TYPE;

	ncp_mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (ncp_mem_buf == NULL) {
		npu_err("fail in npu_ion_map kzalloc\n");
		return -ENOMEM;
	}
	ncp_mem_buf->fd = usr_data->ncp_fd;
	ncp_mem_buf->size = usr_data->ncp_size;
	strcpy(ncp_mem_buf->name, "NCP");

	ret = npu_memory_map(session->memory, ncp_mem_buf, configs[NPU_PBHA_HINT_11]);
	if (unlikely(ret)) {
		npu_err("npu_memory_map is fail(%d).\n", ret);
		goto err_free;
	}
	npu_info("ncp_ion_map(0x%pad), vaddr(0x%pK)\n", &ncp_mem_buf->daddr, ncp_mem_buf->vaddr);
	ret = __update_ncp_info(session, ncp_mem_buf);
	if (ret) {
		npu_err("__ncp_ion_map is fail(%d).\n", ret);
		goto err_unmap;
	}
	ret = add_ion_mem(session, ncp_mem_buf, opt);
	if (ret) {
		npu_err("__ncp_ion_map is fail(%d).\n", ret);
		goto err_unmap;
	}

#ifdef CONFIG_NPU_USE_HW_DEVICE
	if (session->hids & NPU_HWDEV_ID_NPU)
#endif
	{
		ncp_hdr_buf = copy_ncp_header(session, ncp_mem_buf);
		if (IS_ERR_OR_NULL(ncp_hdr_buf))
			return PTR_ERR(ncp_hdr_buf); /* free buffer from __release_graph_ion */

		ret = npu_util_validate_user_ncp(session, ncp_hdr_buf->vaddr, ncp_mem_buf->size);
		if (ret)
			return ret; /* free buffer from __release_graph_ion */
	}

	return ret;
err_unmap:
	npu_memory_unmap(session->memory, ncp_mem_buf);
err_free:
	kfree(ncp_mem_buf);
	return ret;
}

int __get_session_info(struct npu_session *session, struct vs4l_graph *info)
{
	int ret = 0;
	struct drv_usr_share *usr_data = NULL;
	usr_data = kzalloc(sizeof(struct drv_usr_share), GFP_KERNEL);
	if (unlikely(!usr_data)) {
		npu_err("Not enough Memory\n");
		ret = -ENOMEM;
		return ret;
	}
	ret = copy_from_user((void *)usr_data, (void *)info->addr, sizeof(struct drv_usr_share));
	if (ret) {
		npu_err("copy_from_user failed(%d)\n", ret);
		goto p_err;
	}
	__set_unique_id(session, usr_data);
#ifdef CONFIG_NPU_USE_HW_DEVICE
	ret = __set_unified_id(session, usr_data);
	if (ret < 0) {
		npu_err("__set_unified_id failed(%d)\n", ret);
		goto p_err;
	}
#endif
	npu_utrace("usr_data(0x%pK), ncp_size(%u)\n", session, usr_data, usr_data->ncp_size);
	ret = copy_to_user((void *)info->addr, (void *)usr_data, sizeof(struct drv_usr_share));
	if (ret) {
		npu_err("copy_to_user failed(%d)\n", ret);
		goto p_err;
	}
	ret = __ncp_ion_map(session, usr_data);
	if (unlikely(ret)) {
		npu_uerr("__ncp_ion_map is fail(%d)\n", session, ret);
		goto p_err;
	}
	session->ss_state |= BIT(NPU_SESSION_STATE_GRAPH_ION_MAP);
p_err:
	if (usr_data)
		kfree(usr_data);
	return ret;
}

int __pilot_parsing_ncp(struct npu_session *session, u32 *IFM_cnt, u32 *OFM_cnt, u32 *IMB_cnt, u32 *WGT_cnt)
{
	int ret = 0;
	u32 i = 0;
	char *ncp_vaddr;
	u32 memory_vector_cnt;
	u32 memory_vector_offset;
	struct memory_vector *mv;
	struct ncp_header *ncp;

	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;
	ncp = (struct ncp_header *)ncp_vaddr;
	memory_vector_offset = ncp->memory_vector_offset;
	if (unlikely(memory_vector_offset > session->ncp_mem_buf->size)) {
		npu_err("memory vector offset(0x%x) > max size(0x%x) ;out of bounds\n",
				(u32)memory_vector_offset, (u32)session->ncp_mem_buf->size);
		return -EFAULT;
	}

	memory_vector_cnt = ncp->memory_vector_cnt;
	if (unlikely(((memory_vector_cnt * sizeof(struct memory_vector)) + memory_vector_offset) >  session->ncp_mem_buf->size)) {
		npu_err("memory vector count(0x%x) seems abnormal ;out of bounds\n", memory_vector_cnt);
		return -EFAULT;
	}
	session->memory_vector_offset = memory_vector_offset;
	session->memory_vector_cnt = memory_vector_cnt;

	mv = (struct memory_vector *)(ncp_vaddr + memory_vector_offset);

	for (i = 0; i < memory_vector_cnt; i++) {
		u32 memory_type = (mv + i)->type;
		switch (memory_type) {
		case MEMORY_TYPE_IN_FMAP:
			{
				(*IFM_cnt)++;
				break;
			}
		case MEMORY_TYPE_OT_FMAP:
			{
				(*OFM_cnt)++;
				break;
			}
		case MEMORY_TYPE_IM_FMAP:
			{
				(*IMB_cnt)++;
				break;
			}
		case MEMORY_TYPE_CUCODE:
		case MEMORY_TYPE_WEIGHT:
		case MEMORY_TYPE_WMASK:
			{
				(*WGT_cnt)++;
				break;
			}
		}
	}

#if (CONFIG_NPU_NCP_VERSION >= 25)
	ncp->model_name[NCP_MODEL_NAME_LEN - 1] = '\0';
	strncpy(session->model_name, ncp->model_name, NCP_MODEL_NAME_LEN);
#endif

	return ret;
}

int __second_parsing_ncp(
	struct npu_session *session,
	struct addr_info **IFM_av, struct addr_info **OFM_av,
	struct addr_info **IMB_av, struct addr_info **WGT_av)
{
	int ret = 0;
	u32 i = 0;
	u32 weight_size;

	u32 IFM_cnt = 0;
	u32 OFM_cnt = 0;
	u32 IMB_cnt = 0;
	u32 WGT_cnt = 0;

	u32 address_vector_offset;
	u32 address_vector_cnt;
	u32 memory_vector_offset;
	u32 memory_vector_cnt;

	struct ncp_header *ncp;
	struct address_vector *av;
	struct memory_vector *mv;

	char *ncp_vaddr;
	dma_addr_t ncp_daddr;

	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;
	ncp = (struct ncp_header *)ncp_vaddr;

	address_vector_offset = ncp->address_vector_offset;
	if (unlikely(address_vector_offset > session->ncp_mem_buf->size)) {
		npu_err("address vector offset(0x%x) > max size(0x%x) ;out of bounds\n",
				address_vector_offset, (u32)session->ncp_mem_buf->size);
		return -EFAULT;
	}


	address_vector_cnt = ncp->address_vector_cnt;
	if (unlikely(((address_vector_cnt * sizeof(struct address_vector)) + address_vector_offset) >
									session->ncp_mem_buf->size)) {
		npu_err("address vector count(0x%x) seems abnormal ;out of bounds\n", address_vector_cnt);
		return -EFAULT;
	}

	session->address_vector_offset = address_vector_offset;
	session->address_vector_cnt = address_vector_cnt;

	session->ncp_info.address_vector_cnt = address_vector_cnt;

	memory_vector_offset = session->memory_vector_offset;
	memory_vector_cnt = session->memory_vector_cnt;
#ifdef CONFIG_NPU_USE_HW_DEVICE
	ncp->unique_id |= session->unified_id << 16;
#endif

	if (address_vector_cnt > memory_vector_cnt) {
		npu_err("address_vector_cnt(%d) should not exceed memory_vector_cnt(%d)\n",
				address_vector_cnt, memory_vector_cnt);
		return -EFAULT;
	}

	mv = (struct memory_vector *)(ncp_vaddr + memory_vector_offset);
	av = (struct address_vector *)(ncp_vaddr + address_vector_offset);

	for (i = 0; i < memory_vector_cnt; i++) {
		u32 memory_type = (mv + i)->type;
		u32 address_vector_index;
		u32 weight_offset;

		switch (memory_type) {
		case MEMORY_TYPE_IN_FMAP:
			{
				if (IFM_cnt >= session->IFM_cnt) {
					npu_err("IFM_cnt(%d) should not exceed size of allocated array(%d)\n",
							IFM_cnt, session->IFM_cnt);
					return -EFAULT;
				}
				address_vector_index = (mv + i)->address_vector_index;
				if (likely(!EVER_FIND_FM(&IFM_cnt, *IFM_av, address_vector_index))) {
					(*IFM_av + IFM_cnt)->av_index = address_vector_index;
					if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
						npu_err("address_vector_index(%d) should not exceed max addr vec count(%d)\n",
								address_vector_index, address_vector_cnt);
						return -EFAULT;
					}
					(*IFM_av + IFM_cnt)->size = (av + address_vector_index)->size;
					(*IFM_av + IFM_cnt)->pixel_format = (mv + i)->pixel_format;
					(*IFM_av + IFM_cnt)->width = (mv + i)->width;
					(*IFM_av + IFM_cnt)->height = (mv + i)->height;
					(*IFM_av + IFM_cnt)->channels = (mv + i)->channels;
					(mv + i)->wstride = 0;
					(*IFM_av + IFM_cnt)->stride = (mv + i)->wstride;
					npu_uinfo("(IFM_av + %u)->index = %u\n", session,
						IFM_cnt, (*IFM_av + IFM_cnt)->av_index);
					npu_utrace("[session] IFM, index(%u)\n", session, (*IFM_av + IFM_cnt)->av_index);
					npu_utrace("[session] IFM, size(%zu)\n", session, (*IFM_av + IFM_cnt)->size);
					npu_utrace("[session] IFM, pixel_format(%u)\n", session, (*IFM_av + IFM_cnt)->pixel_format);
					npu_utrace("[session] IFM, width(%u)\n", session, (*IFM_av + IFM_cnt)->width);
					npu_utrace("[session] IFM, height(%u)\n", session, (*IFM_av + IFM_cnt)->height);
					npu_utrace("[session] IFM, channels(%u)\n", session, (*IFM_av + IFM_cnt)->channels);
					npu_utrace("[session] IFM, wstride(%u)\n", session, (*IFM_av + IFM_cnt)->stride);

					IFM_cnt++;
				}
				break;
			}
		case MEMORY_TYPE_OT_FMAP:
			{
				if (OFM_cnt >= session->OFM_cnt) {
					npu_err("OFM_cnt(%d) should not exceed size of allocated array(%d)\n",
							OFM_cnt, session->OFM_cnt);
					return -EFAULT;
				}
				address_vector_index = (mv + i)->address_vector_index;
				if (likely(!EVER_FIND_FM(&OFM_cnt, *OFM_av, address_vector_index))) {
					(*OFM_av + OFM_cnt)->av_index = address_vector_index;
					if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
						npu_err("address_vector_index(%d) should not exceed max addr vec count(%d)\n",
								address_vector_index, address_vector_cnt);
						return -EFAULT;
					}
					(*OFM_av + OFM_cnt)->size = (av + address_vector_index)->size;
					(*OFM_av + OFM_cnt)->pixel_format = (mv + i)->pixel_format;
					(*OFM_av + OFM_cnt)->width = (mv + i)->width;
					(*OFM_av + OFM_cnt)->height = (mv + i)->height;
					(*OFM_av + OFM_cnt)->channels = (mv + i)->channels;
					(mv + i)->wstride = 0;
					(*OFM_av + OFM_cnt)->stride = (mv + i)->wstride;
					npu_uinfo("(OFM_av + %u)->index = %u\n", session, OFM_cnt, (*OFM_av + OFM_cnt)->av_index);
					npu_utrace("OFM, index(%d)\n", session, (*OFM_av + OFM_cnt)->av_index);
					npu_utrace("[session] OFM, size(%zu)\n", session, (*OFM_av + OFM_cnt)->size);
					npu_utrace("[session] OFM, pixel_format(%u)\n", session, (*OFM_av + OFM_cnt)->pixel_format);
					npu_utrace("[session] OFM, width(%u)\n", session, (*OFM_av + OFM_cnt)->width);
					npu_utrace("[session] OFM, height(%u)\n", session, (*OFM_av + OFM_cnt)->height);
					npu_utrace("[session] OFM, channels(%u)\n", session, (*OFM_av + OFM_cnt)->channels);
					npu_utrace("[session] OFM, wstride(%u)\n", session, (*OFM_av + OFM_cnt)->stride);
					OFM_cnt++;
				}
				break;
			}
		case MEMORY_TYPE_IM_FMAP:
			{
				if (IMB_cnt >= session->IMB_cnt) {
					npu_err("IMB: IMB_cnt(%d) should not exceed size of allocated array(%d)\n",
							IMB_cnt, session->IMB_cnt);
					return -EFAULT;
				}
				address_vector_index = (mv + i)->address_vector_index;
				if (likely(!EVER_FIND_FM(&IMB_cnt, *IMB_av, address_vector_index))) {
					(*IMB_av + IMB_cnt)->av_index = address_vector_index;
					if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
						npu_err("IMB: address_vector_index(%d) should not exceed max addr vec count(%d)\n",
								address_vector_index, address_vector_cnt);
						return -EFAULT;
					}
					(*IMB_av + IMB_cnt)->size = (av + address_vector_index)->size;
					(*IMB_av + IMB_cnt)->pixel_format = (mv + i)->pixel_format;
					(*IMB_av + IMB_cnt)->width = (mv + i)->width;
					(*IMB_av + IMB_cnt)->height = (mv + i)->height;
					(*IMB_av + IMB_cnt)->channels = (mv + i)->channels;
					(mv + i)->wstride = 0;
					(*IMB_av + IMB_cnt)->stride = (mv + i)->wstride;
					npu_info("IMB: (*IMB_av + %ld)->index = %llx\n",
							IMB_cnt, (*IMB_av + IMB_cnt)->av_index);
					IMB_cnt++;
				}
				break;
			}
		case MEMORY_TYPE_CUCODE:
		case MEMORY_TYPE_WEIGHT:
		case MEMORY_TYPE_WMASK:
			{
				ncp_daddr = session->ncp_mem_buf->daddr;
				ncp_vaddr = (char *)session->ncp_mem_buf->vaddr;

				if (WGT_cnt >= session->WGT_cnt) {
					npu_err("WGT_cnt(%d) should not exceed size of allocated array(%d)\n",
							WGT_cnt, session->WGT_cnt);
					return -EFAULT;
				}
				// update address vector, m_addr with ncp_alloc_daddr + offset
				address_vector_index = (mv + i)->address_vector_index;
				if (unlikely(((address_vector_index * sizeof(struct address_vector))
						+ address_vector_offset) > session->ncp_mem_buf->size) ||
						unlikely(address_vector_index >= address_vector_cnt)) {
					npu_err("address_vector_index(%d) should not exceed max addr vec count(%d)\n",
							address_vector_index, address_vector_cnt);
					return -EFAULT;
				}
				weight_offset = (av + address_vector_index)->m_addr;
				if (unlikely(weight_offset > (u32)session->ncp_mem_buf->size)) {
					ret = -EINVAL;
					npu_uerr("weight_offset is invalid, offset(0x%x), ncp_daddr(0x%x)\n",
						session, (u32)weight_offset, (u32)session->ncp_mem_buf->size);
					goto p_err;
				}
				(av + address_vector_index)->m_addr = weight_offset + ncp_daddr;

				(*WGT_av + WGT_cnt)->av_index = address_vector_index;
				weight_size = (av + address_vector_index)->size;
				if (unlikely((weight_offset + weight_size) < weight_size)) {
					npu_err("weight_offset(0x%x) + weight size (0x%x) seems to be overflow.\n",
						weight_offset, weight_size);
					return -ERANGE;
				}
				if (unlikely((weight_offset + weight_size) > (u32)session->ncp_mem_buf->size)) {
					npu_err("weight_offset(0x%x) + weight size (0x%x) seems to go beyond ncp size(0x%x)\n",
							weight_offset, weight_size, (u32)session->ncp_mem_buf->size);
					return -EFAULT;
				}

				(*WGT_av + WGT_cnt)->size = weight_size;
				(*WGT_av + WGT_cnt)->daddr = weight_offset + ncp_daddr;
				(*WGT_av + WGT_cnt)->vaddr = weight_offset + ncp_vaddr;
				(*WGT_av + WGT_cnt)->memory_type = memory_type;
				npu_utrace("(*WGT_av + %u)->av_index = %u\n", session,	WGT_cnt, (*WGT_av + WGT_cnt)->av_index);
				npu_utrace("(*WGT_av + %u)->size = %zu\n", session, WGT_cnt, (*WGT_av + WGT_cnt)->size);
				npu_utrace("(*WGT_av + %u)->daddr = 0x%pad\n", session, WGT_cnt, &((*WGT_av + WGT_cnt)->daddr));
				npu_utrace("(*WGT_av + %u)->vaddr = 0x%pK\n", session, WGT_cnt, (*WGT_av + WGT_cnt)->vaddr);
				WGT_cnt++;
				break;
			}
		default:
			break;
		}
	}
	session->IOFM_cnt = IFM_cnt + OFM_cnt;
	return ret;
p_err:
	return ret;
}


int __make_IFM_info(struct npu_session *session, struct addr_info **IFM_av)
{
	int ret = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	u32 IFM_cnt = session->IFM_cnt;
	struct addr_info *IFM_addr;

	for (k = 0; k < VISION_MAX_CONTAINERLIST; k++) {
		for (i = 0; i < VISION_MAX_BUFFER; i++) {
			IFM_addr = kcalloc(IFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
			if (unlikely(!IFM_addr)) {
				npu_err("failed in __make_IFM_info(ENOMEM)\n");
				ret = -ENOMEM;
				goto p_err;
			}
			session->IFM_info[k][i].addr_info = IFM_addr;
			for (j = 0; j < IFM_cnt; j++) {
			(IFM_addr + j)->av_index = (*IFM_av + j)->av_index;
			(IFM_addr + j)->size = (*IFM_av + j)->size;
			(IFM_addr + j)->pixel_format = (*IFM_av + j)->pixel_format;
			(IFM_addr + j)->width = (*IFM_av + j)->width;
			(IFM_addr + j)->height = (*IFM_av + j)->height;
			(IFM_addr + j)->channels = (*IFM_av + j)->channels;
			(IFM_addr + j)->stride = (*IFM_av + j)->stride;
			}
		}
	}

p_err:
	return ret;
}

int __make_OFM_info(struct npu_session *session, struct addr_info **OFM_av)
{
	int ret = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	u32 OFM_cnt = session->OFM_cnt;
	struct addr_info *OFM_addr;

	for (k = 0; k < VISION_MAX_CONTAINERLIST; k++) {
		for (i = 0; i < VISION_MAX_BUFFER; i++) {
			OFM_addr = kcalloc(OFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
			if (unlikely(!OFM_addr)) {
				npu_err("failed in __make_OFM_info(ENOMEM)\n");
				ret = -ENOMEM;
				goto p_err;
			}
			session->OFM_info[k][i].addr_info = OFM_addr;
			for (j = 0; j < OFM_cnt; j++) {
			(OFM_addr + j)->av_index = (*OFM_av + j)->av_index;
			(OFM_addr + j)->size = (*OFM_av + j)->size;
			(OFM_addr + j)->pixel_format = (*OFM_av + j)->pixel_format;
			(OFM_addr + j)->width = (*OFM_av + j)->width;
			(OFM_addr + j)->height = (*OFM_av + j)->height;
			(OFM_addr + j)->channels = (*OFM_av + j)->channels;
			(OFM_addr + j)->stride = (*OFM_av + j)->stride;
			}
		}
	}

p_err:
	return ret;
}

int __ion_alloc_IOFM(struct npu_session *session, struct addr_info **IFM_av, struct addr_info **OFM_av)
{
	int ret = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	struct npu_memory_buffer *IOFM_mem_buf;
	mem_opt_e opt = IOFM_TYPE;
	struct address_vector *vaddr;

	u32 IFM_cnt = session->IFM_cnt;
	u32 IOFM_cnt = session->IOFM_cnt;

	IOFM_mem_buf = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (unlikely(!IOFM_mem_buf)) {
		npu_err("failed in __ion_alloc_IOFM(ENOMEM)\n");
		ret = -ENOMEM;
		return ret;
	}
	npu_udbg("ion alloc IOFM(0x%pK)\n", session, IOFM_mem_buf);

	IOFM_mem_buf->size = VISION_MAX_CONTAINERLIST * VISION_MAX_BUFFER * IOFM_cnt * sizeof(struct address_vector);
	strcpy(IOFM_mem_buf->name, "IOFM");
	ret = npu_memory_alloc(session->memory, IOFM_mem_buf, configs[NPU_PBHA_HINT_00]);
	if (unlikely(ret)) {
		npu_uerr("npu_memory_alloc is fail(%d).\n", session, ret);
		goto p_err;
	}
	npu_udbg(" (IOFM_mem_buf + %d)->vaddr(0x%pK), daddr(0x%pad), size(%zu)\n",
		session, 0, (IOFM_mem_buf + 0)->vaddr, &(IOFM_mem_buf + 0)->daddr, (IOFM_mem_buf + 0)->size);

	vaddr = IOFM_mem_buf->vaddr;
	for (i = 0; i < IOFM_cnt; i++) {
		for (k = 0; k < VISION_MAX_CONTAINERLIST; k++) {
			for (j = 0; j < VISION_MAX_BUFFER; j++) {
				if (i < IFM_cnt) {
					(vaddr + (k*VISION_MAX_BUFFER + j)*IOFM_cnt + i)->index = (*IFM_av + i)->av_index;
					(vaddr + (k*VISION_MAX_BUFFER + j)*IOFM_cnt + i)->size = (*IFM_av + i)->size;
				} else {
					(vaddr + (k*VISION_MAX_BUFFER + j)*IOFM_cnt + i)->index = (*OFM_av + (i - IFM_cnt))->av_index;
					(vaddr + (k*VISION_MAX_BUFFER + j)*IOFM_cnt + i)->size = (*OFM_av + (i - IFM_cnt))->size;
				}
			}
		}
	}
	ret = add_ion_mem(session, IOFM_mem_buf, opt);
	return ret;
p_err:
	npu_memory_free(session->memory, IOFM_mem_buf);
	if (IOFM_mem_buf)
		kfree(IOFM_mem_buf);
	return ret;
}

int __make_IMB_info(struct npu_session *session, struct addr_info **temp_IMB_av)
{
	int ret = 0;
	u32 i = 0;
	u32 IMB_cnt = session->IMB_cnt;
	struct addr_info *IMB_info;

	IMB_info = session->IMB_info;
	for (i = 0; i < IMB_cnt; i++) {
		(IMB_info + i)->av_index = (*temp_IMB_av + i)->av_index;
		(IMB_info + i)->vaddr = (*temp_IMB_av + i)->vaddr;
		(IMB_info + i)->daddr = (*temp_IMB_av + i)->daddr;
		(IMB_info + i)->size = (*temp_IMB_av + i)->size;
		(IMB_info + i)->pixel_format = (*temp_IMB_av + i)->pixel_format;
		(IMB_info + i)->width = (*temp_IMB_av + i)->width;
		(IMB_info + i)->height = (*temp_IMB_av + i)->height;
		(IMB_info + i)->channels = (*temp_IMB_av + i)->channels;
		(IMB_info + i)->stride = (*temp_IMB_av + i)->stride;
	}

	return ret;
}

int __prepare_IMB_info(struct npu_session *session, struct addr_info **IMB_av, struct npu_memory_buffer *IMB_mem_buf)
{
	int ret = 0;
	u32 i = 0;
	u32 IMB_cnt = session->IMB_cnt;
	struct addr_info *IMB_info;

	IMB_info = kcalloc(IMB_cnt, sizeof(struct addr_info), GFP_KERNEL);
	if (unlikely(!IMB_info)) {
		npu_err("IMB: IMB_info alloc is fail\n");
		ret = -ENOMEM;
		goto p_err;
	}

	session->IMB_info = IMB_info;
	__make_IMB_info(session, IMB_av);

	for (i = 0; i < IMB_cnt; i++) {
#ifdef CONFIG_NPU_USE_IMB_ALLOCATOR
		IMB_mem_buf->ncp_max_size = max(IMB_mem_buf->ncp_max_size, ALIGN((*IMB_av + i)->size, NPU_IMB_ALIGN));
#endif
		IMB_mem_buf->size += ALIGN((*IMB_av + i)->size, NPU_IMB_ALIGN);
	}
	session->IMB_size = IMB_mem_buf->size;
	session->IMB_mem_buf = IMB_mem_buf;

#ifdef CONFIG_NPU_USE_IMB_ALLOCATOR
	if (session->IMB_size > (NPU_IMB_CHUNK_SIZE * NPU_IMB_CHUNK_MAX_NUM)) {
		npu_uerr("IMB_size(%lld) is larger than %u\n",
			session, session->IMB_size, NPU_IMB_CHUNK_SIZE * NPU_IMB_CHUNK_MAX_NUM);
		ret = -ENOMEM;
		goto p_err;
	}
#endif

	return ret;

p_err:
	if (likely(IMB_info))
		kfree(IMB_info);
	if (likely(IMB_mem_buf))
		kfree(IMB_mem_buf);
	return ret;
}

void npu_session_ion_sync_for_device(struct npu_memory_buffer *pbuf, off_t offset, size_t size, enum dma_data_direction dir)
{
	if (likely(pbuf->vaddr)) {
		BUG_ON((offset < 0) || (offset > pbuf->size));
		BUG_ON((offset + size) < size);
		BUG_ON((size > pbuf->size) || ((offset + size) > pbuf->size));

		__npu_session_ion_sync_for_device(pbuf, offset, size, dir);
	}
}

int __ion_alloc_IMB(struct npu_session *session, struct addr_info **IMB_av, struct npu_memory_buffer *IMB_mem_buf)
{
	int ret = 0;
	u32 i = 0;
	u32 address_vector_offset;
	struct address_vector *av;
	char *ncp_vaddr;
	u32 IMB_cnt = session->IMB_cnt;
	u32 addr_offset = 0;

	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;
	address_vector_offset = session->address_vector_offset;

	av = (struct address_vector *)(ncp_vaddr + address_vector_offset);
#ifdef CONFIG_NPU_USE_IMB_ALLOCATOR
	ret = __alloc_imb_chunk(session, IMB_mem_buf);
	if (unlikely(ret)) {
		npu_uerr("IMB: __alloc_imb_chunk is fail(%d)\n", session, ret);
		goto p_err;
	}
#else /* !CONFIG_NPU_USE_IMB_ALLOCATOR */
	ret = npu_memory_alloc(session->memory, IMB_mem_buf, configs[NPU_PBHA_HINT_00]);
	if (unlikely(ret)) {
		npu_uerr("IMB: npu_memory_alloc is fail(%d).\n", session, ret);
		goto p_err;
	}
#endif
	for (i = 0; i < IMB_cnt; i++) {
		(av + (*IMB_av + i)->av_index)->m_addr = IMB_mem_buf->daddr + addr_offset;
		(session->IMB_info + i)->daddr = IMB_mem_buf->daddr + addr_offset;
		(session->IMB_info + i)->vaddr = ((void *)((char *)(IMB_mem_buf->vaddr)) + addr_offset);
		(session->IMB_info + i)->size = (*IMB_av + i)->size;
		addr_offset += ALIGN((*IMB_av + i)->size, (u32)NPU_IMB_ALIGN);
		npu_udbg("IMB: (IMB_mem_buf + %d)->vaddr(%pad), daddr(%pad), size(%zu)\n",
			 session, i, (session->IMB_info + i)->vaddr, &(session->IMB_info + i)->daddr,
			 (session->IMB_info + i)->size);
	}

	npu_session_ion_sync_for_device(IMB_mem_buf, 0, IMB_mem_buf->size, DMA_TO_DEVICE);
	return ret;

p_err:
#ifndef CONFIG_NPU_USE_IMB_ALLOCATOR
	npu_memory_free(session->memory, IMB_mem_buf);
#endif

	kfree(IMB_mem_buf);
	session->IMB_mem_buf = NULL;

	return ret;
}

int __config_session_info(struct npu_session *session)
{
	int ret = 0;

	struct addr_info *IFM_av = NULL;
	struct addr_info *OFM_av = NULL;
	struct addr_info *IMB_av = NULL;
	struct addr_info *WGT_av = NULL;

	struct npu_memory_buffer *IMB_mem_buf = NULL;

	ret = __pilot_parsing_ncp(session, &session->IFM_cnt, &session->OFM_cnt, &session->IMB_cnt, &session->WGT_cnt);
	if (ret) {
		npu_err("failed in __pilot_parsing_ncp\n");
		goto p_err;
	}

	IFM_av = kcalloc(session->IFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
	OFM_av = kcalloc(session->OFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
	IMB_av = kcalloc(session->IMB_cnt, sizeof(struct addr_info), GFP_KERNEL);
	WGT_av = kcalloc(session->WGT_cnt, sizeof(struct addr_info), GFP_KERNEL);

	session->WGT_info = WGT_av;
	session->ss_state |= BIT(NPU_SESSION_STATE_WGT_KALLOC);

	if (unlikely(!IFM_av || !OFM_av || !IMB_av || !WGT_av)) {
		npu_err("failed in __config_session_info(ENOMEM)\n");
		ret = -ENOMEM;
		goto p_err;
	}

	ret = __second_parsing_ncp(session, &IFM_av, &OFM_av, &IMB_av, &WGT_av);
	if (unlikely(ret)) {
		npu_uerr("fail(%d) in second_parsing_ncp\n", session, ret);
		goto p_err;
	}

	ret = __make_IFM_info(session, &IFM_av);
	ret = __make_OFM_info(session, &OFM_av);
	session->ss_state |= BIT(NPU_SESSION_STATE_IOFM_KALLOC);

	ret = __ion_alloc_IOFM(session, &IFM_av, &OFM_av);
	if (unlikely(ret)) {
		npu_uerr("fail(%d) in __ion_alloc_IOFM\n", session, ret);
		goto p_err;
	}
	session->ss_state |= BIT(NPU_SESSION_STATE_IOFM_ION_ALLOC);

	IMB_mem_buf = kcalloc(1, sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (unlikely(!IMB_mem_buf)) {
		npu_err("IMB: IMB_mem_buf alloc is fail\n");
		ret = -ENOMEM;
		goto p_err;
	}
	strcpy(IMB_mem_buf->name, "IMB");

	ret = __prepare_IMB_info(session, &IMB_av, IMB_mem_buf);
	if (unlikely(ret)) {
		npu_uerr("IMB: fail(%d) in __prepare_IMB_info\n", session, ret);
		goto p_err;
	}
	npu_udbg("IMB: buffer summary size = 0x%zX\n", session, session->IMB_size);
	npu_udbg("IMB: threshold size = 0x%X\n", session, configs[NPU_IMB_THRESHOLD_SIZE]);
#ifdef CONFIG_NPU_USE_IMB_ALLOCATOR
	npu_udbg("IMB: NCP max size = 0x%zX\n", session, session->IMB_mem_buf->ncp_max_size);
	npu_udbg("IMB: granularity size = 0x%X\n", session, NPU_IMB_GRANULARITY);
#endif
	/* For CONFIG_NPU_USE_IMB_ALLOCATOR case we allocate/deallocate additional memory chunks also dynamically
	 * in qbuf, but for now as we're using chunks nature we can allocate all that we need here to skip it at
	 * first qbuf iteration and just deallocate additional chunks in dequeu buf by NPU_IMB_THRESHOLD_SIZE limit.
	 */
#ifndef CONFIG_NPU_USE_IMB_ALLOCATOR
	if (session->IMB_size <= configs[NPU_IMB_THRESHOLD_SIZE])
#endif
	{
		ret = __ion_alloc_IMB(session, &IMB_av, IMB_mem_buf);
		if (unlikely(ret)) {
			npu_uerr("IMB: fail(%d) in ion alloc IMB\n", session, ret);
			goto p_err;
		}
		session->ss_state |= BIT(NPU_SESSION_STATE_IMB_ION_ALLOC);
	}

	npu_session_ion_sync_for_device(session->ncp_hdr_buf, 0, session->ncp_hdr_buf->size, DMA_TO_DEVICE);
p_err:
	if (likely(IFM_av))
		kfree(IFM_av);
	if (likely(OFM_av))
		kfree(OFM_av);
	if (likely(IMB_av))
		kfree(IMB_av);

	return ret;
}

#ifdef CONFIG_DSP_USE_VS4L
int __pilot_parsing_ncp_for_dsp(struct npu_session *session, u32 *IFM_cnt, u32 *OFM_cnt, u32 *IMB_cnt, u32 *WGT_cnt)
{
	int ret = 0;
	u32 i = 0;
	char *msg_vaddr;
	struct dsp_common_graph_info_v4 *msg;
	struct dsp_common_param_v4 *param;
	u32 param_cnt;

	msg_vaddr = (char *)session->ncp_mem_buf->vaddr;
	msg = (struct dsp_common_graph_info_v4 *)msg_vaddr;
	param = (struct dsp_common_param_v4 *)msg->param_list;
	param_cnt = msg->n_tsgd + msg->n_param;

	/* do NOT use memory vector for DSP */
	session->memory_vector_offset = 0;
	session->memory_vector_cnt = 0;

	for (i = 0; i < param_cnt; i++) {
		u32 param_type = (param + i)->param_type;
		switch (param_type) {
		case DSP_COMMON_MEM_IFM:
			{
				(*IFM_cnt)++;
				break;
			}
		case DSP_COMMON_MEM_OFM:
			{
				(*OFM_cnt)++;
				break;
			}
		case DSP_COMMON_MEM_IMB:
			{
				(*IMB_cnt)++;
				break;
			}
		case DSP_COMMON_MEM_WGT:
			{
				(*WGT_cnt)++;
				break;
			}
		}
	}

	/* DSP has at lease 1 IFM as execute msg */
	if (!(*IFM_cnt))
		(*IFM_cnt)++;
	return ret;
}

int __second_parsing_ncp_for_dsp(
	struct npu_session *session,
	struct addr_info **IFM_av, struct addr_info **OFM_av,
	struct addr_info **IMB_av, struct addr_info **WGT_av)
{
	int ret = 0;
	u32 i = 0;

	u32 IFM_cnt = 0;
	u32 OFM_cnt = 0;
	u32 IMB_cnt = 0;
	u32 WGT_cnt = 0;

	char *msg_vaddr;
	struct dsp_common_graph_info_v4 *msg;
	struct dsp_common_param_v4 *param;
	struct dsp_common_mem_v4 *mem;
	u32 param_type, param_cnt;

	msg_vaddr = (char *)session->ncp_mem_buf->vaddr;
	msg = (struct dsp_common_graph_info_v4 *)msg_vaddr;
	param = (struct dsp_common_param_v4 *)msg->param_list;
	param_cnt = msg->n_tsgd + msg->n_param;

	/* do NOT use address vector for DSP */
	session->address_vector_offset = 0;
	session->address_vector_cnt = 0;

	session->ncp_info.address_vector_cnt = 0;

	for (i = 0; i < param_cnt; i++) {
		param_type = (param + i)->param_type;
		mem = &((param + i)->param_mem);
		switch (param_type) {
		case DSP_COMMON_MEM_IFM:
			{
				if (IFM_cnt >= session->IFM_cnt) {
					npu_err("IFM_cnt(%d) should not exceed size of allocated array(%d)\n",
							IFM_cnt, session->IFM_cnt);
					return -EFAULT;
				}
				/* do NOT use address vector for DSP */
				(*IFM_av + IFM_cnt)->av_index = 0;
				(*IFM_av + IFM_cnt)->size = mem->size;
				/* use param in 'struct dsp_common_mem_vx'
				 * for format info
				 */
				(*IFM_av + IFM_cnt)->pixel_format = mem->param[3];
				(*IFM_av + IFM_cnt)->width = mem->param[0];
				(*IFM_av + IFM_cnt)->height = mem->param[1];
				(*IFM_av + IFM_cnt)->channels = mem->param[2];
				(*IFM_av + IFM_cnt)->stride = 0;
				npu_uinfo("(IFM_av + %u)->index = %u\n", session,
						IFM_cnt, (*IFM_av + IFM_cnt)->av_index);
				npu_utrace("[session] IFM, index(%u)\n", session, (*IFM_av + IFM_cnt)->av_index);
				npu_utrace("[session] IFM, size(%zu)\n", session, (*IFM_av + IFM_cnt)->size);
				npu_utrace("[session] IFM, pixel_format(%u)\n", session, (*IFM_av + IFM_cnt)->pixel_format);
				npu_utrace("[session] IFM, width(%u)\n", session, (*IFM_av + IFM_cnt)->width);
				npu_utrace("[session] IFM, height(%u)\n", session, (*IFM_av + IFM_cnt)->height);
				npu_utrace("[session] IFM, channels(%u)\n", session, (*IFM_av + IFM_cnt)->channels);
				npu_utrace("[session] IFM, wstride(%u)\n", session, (*IFM_av + IFM_cnt)->stride);

				IFM_cnt++;
				break;
			}
		case DSP_COMMON_MEM_OFM:
			{
				if (OFM_cnt >= session->OFM_cnt) {
					npu_err("OFM_cnt(%d) should not exceed size of allocated array(%d)\n",
							OFM_cnt, session->OFM_cnt);
					return -EFAULT;
				}
				/* do NOT use address vector for DSP */
				(*OFM_av + OFM_cnt)->av_index = 0;
				(*OFM_av + OFM_cnt)->size = mem->size;
				/* use param in 'struct dsp_common_mem_vx'
				 * for format info
				 */
				(*OFM_av + OFM_cnt)->pixel_format = mem->param[3];
				(*OFM_av + OFM_cnt)->width = mem->param[0];
				(*OFM_av + OFM_cnt)->height = mem->param[1];
				(*OFM_av + OFM_cnt)->channels = mem->param[2];
				(*OFM_av + OFM_cnt)->stride = 0;
				npu_uinfo("(OFM_av + %u)->index = %u\n", session, OFM_cnt, (*OFM_av + OFM_cnt)->av_index);
				npu_utrace("OFM, index(%d)\n", session, (*OFM_av + OFM_cnt)->av_index);
				npu_utrace("[session] OFM, size(%zu)\n", session, (*OFM_av + OFM_cnt)->size);
				npu_utrace("[session] OFM, pixel_format(%u)\n", session, (*OFM_av + OFM_cnt)->pixel_format);
				npu_utrace("[session] OFM, width(%u)\n", session, (*OFM_av + OFM_cnt)->width);
				npu_utrace("[session] OFM, height(%u)\n", session, (*OFM_av + OFM_cnt)->height);
				npu_utrace("[session] OFM, channels(%u)\n", session, (*OFM_av + OFM_cnt)->channels);
				npu_utrace("[session] OFM, wstride(%u)\n", session, (*OFM_av + OFM_cnt)->stride);
				OFM_cnt++;
				break;
			}
		case DSP_COMMON_MEM_IMB:
			{
				if (IMB_cnt >= session->IMB_cnt) {
					npu_err("IMB_cnt(%d) should not exceed size of allocated array(%d)\n",
							IMB_cnt, session->IMB_cnt);
					return -EFAULT;
				}
				/* do NOT use address vector for DSP */
				(*IMB_av + IMB_cnt)->av_index = 0;
				(*IMB_av + IMB_cnt)->size = mem->size;
				(*IMB_av + IMB_cnt)->pixel_format = mem->param[3];
				(*IMB_av + IMB_cnt)->width = mem->param[0];
				(*IMB_av + IMB_cnt)->height = mem->param[1];
				(*IMB_av + IMB_cnt)->channels = mem->param[2];
				(*IMB_av + IMB_cnt)->stride = 0;
				//npu_info("(*temp_IMB_av + %ld)->index = 0x%x\n",
				//		IMB_cnt, (*temp_IMB_av + IMB_cnt)->index);
				//npu_info("(*temp_IMB_av + %ld)->size = 0x%x\n",
				//		IMB_cnt, (*temp_IMB_av + IMB_cnt)->size);
				IMB_cnt++;
				break;
			}
		case DSP_COMMON_MEM_WGT:
			{
				if (WGT_cnt >= session->WGT_cnt) {
					npu_err("WGT_cnt(%d) should not exceed size of allocated array(%d)\n",
							WGT_cnt, session->WGT_cnt);
					return -EFAULT;
				}
				/* do NOT use address vector for DSP */
				(*WGT_av + WGT_cnt)->av_index = 0;
				(*WGT_av + WGT_cnt)->size = mem->size;
				(*WGT_av + WGT_cnt)->daddr = mem->iova;
				(*WGT_av + WGT_cnt)->memory_type = mem->mem_type;
				npu_utrace("(*WGT_av + %u)->av_index = %u\n", session,	WGT_cnt, (*WGT_av + WGT_cnt)->av_index);
				npu_utrace("(*WGT_av + %u)->size = %zu\n", session, WGT_cnt, (*WGT_av + WGT_cnt)->size);
				npu_utrace("(*WGT_av + %u)->daddr = 0x%pad\n", session, WGT_cnt, &((*WGT_av + WGT_cnt)->daddr));
				WGT_cnt++;
				break;
			}
		default:
			break;
		}
	}

	/* DSP has at lease 1 IFM as execute msg */
	if (!IFM_cnt) {
		if (IFM_cnt >= session->IFM_cnt) {
			npu_err("IFM_cnt(%d) should not exceed size of allocated array(%d)\n",
					IFM_cnt, session->IFM_cnt);
			return -EFAULT;
		}
		/* do NOT use address vector for DSP */
		(*IFM_av + IFM_cnt)->av_index = 0;
		(*IFM_av + IFM_cnt)->size = 0;
		/* use param in 'struct dsp_common_mem_vx'
		 * for format info
		 */
		(*IFM_av + IFM_cnt)->pixel_format = 0;
		(*IFM_av + IFM_cnt)->width = 0;
		(*IFM_av + IFM_cnt)->height = 0;
		(*IFM_av + IFM_cnt)->channels = 0;
		(*IFM_av + IFM_cnt)->stride = 0;
		npu_uinfo("(IFM_av + %u)->index = %u\n", session,
				IFM_cnt, (*IFM_av + IFM_cnt)->av_index);
		npu_utrace("[session] IFM, index(%u)\n", session, (*IFM_av + IFM_cnt)->av_index);
		npu_utrace("[session] IFM, size(%zu)\n", session, (*IFM_av + IFM_cnt)->size);
		npu_utrace("[session] IFM, pixel_format(%u)\n", session, (*IFM_av + IFM_cnt)->pixel_format);
		npu_utrace("[session] IFM, width(%u)\n", session, (*IFM_av + IFM_cnt)->width);
		npu_utrace("[session] IFM, height(%u)\n", session, (*IFM_av + IFM_cnt)->height);
		npu_utrace("[session] IFM, channels(%u)\n", session, (*IFM_av + IFM_cnt)->channels);
		npu_utrace("[session] IFM, wstride(%u)\n", session, (*IFM_av + IFM_cnt)->stride);

		IFM_cnt++;
	}

	session->IOFM_cnt = IFM_cnt + OFM_cnt;
	return ret;
}

int __config_session_info_for_dsp(struct npu_session *session)
{
	int ret = 0;

	struct addr_info *IFM_av = NULL;
	struct addr_info *OFM_av = NULL;
	struct addr_info *IMB_av = NULL;
	struct addr_info *WGT_av = NULL;

	//struct npu_memory_buffer *IMB_mem_buf;

	ret = __pilot_parsing_ncp_for_dsp(session, &session->IFM_cnt, &session->OFM_cnt, &session->IMB_cnt, &session->WGT_cnt);
	if (ret) {
		npu_err("failed in __pilot_parsing_ncp\n");
		goto p_err;
	}

	IFM_av = kcalloc(session->IFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
	OFM_av = kcalloc(session->OFM_cnt, sizeof(struct addr_info), GFP_KERNEL);
	IMB_av = kcalloc(session->IMB_cnt, sizeof(struct addr_info), GFP_KERNEL);
	WGT_av = kcalloc(session->WGT_cnt, sizeof(struct addr_info), GFP_KERNEL);

	session->WGT_info = WGT_av;
	session->ss_state |= BIT(NPU_SESSION_STATE_WGT_KALLOC);

	if (unlikely(!IFM_av || !OFM_av || !IMB_av || !WGT_av)) {
		npu_err("failed in __config_session_info(ENOMEM)\n");
		ret = -ENOMEM;
		goto p_err;
	}

	ret = __second_parsing_ncp_for_dsp(session, &IFM_av, &OFM_av, &IMB_av, &WGT_av);
	if (unlikely(ret)) {
		npu_uerr("fail(%d) in second_parsing_ncp\n", session, ret);
		goto p_err;
	}

	ret = __make_IFM_info(session, &IFM_av);
	ret = __make_OFM_info(session, &OFM_av);
	session->ss_state |= BIT(NPU_SESSION_STATE_IOFM_KALLOC);

	ret = __ion_alloc_IOFM(session, &IFM_av, &OFM_av);
	if (unlikely(ret)) {
		npu_uerr("fail(%d) in __ion_alloc_IOFM\n", session, ret);
		goto p_err;
	}
	session->ss_state |= BIT(NPU_SESSION_STATE_IOFM_ION_ALLOC);

	/* do NOT use internal IMB for DSP */
	//ret = __ion_alloc_IMB(session, &IMB_av);
	//if (unlikely(ret)) {
	//	npu_uerr("fail(%d) in __ion_alloc_IMB\n", session, ret);
	//	goto p_err;
	//}
	session->ss_state |= BIT(NPU_SESSION_STATE_IMB_ION_ALLOC);

	ret = __update_iova_of_load_message_of_dsp(session);
	if (unlikely(ret)) {
		npu_uerr("invalid in __update_iova_of_load_message\n",
				session);
		goto p_err;
	}
	npu_session_ion_sync_for_device(session->ncp_mem_buf, 0, session->ncp_mem_buf->size, DMA_TO_DEVICE);

	//IMB_mem_buf = session->IMB_mem_buf;
	//npu_session_ion_sync_for_device(IMB_mem_buf, 0, IMB_mem_buf->size, DMA_TO_DEVICE);

p_err:
	if (likely(IFM_av))
		kfree(IFM_av);
	if (likely(OFM_av))
		kfree(OFM_av);
	if (likely(IMB_av))
		kfree(IMB_av);

	return ret;
}
#endif

#ifdef CONFIG_NPU_ARBITRATION
int __arbitration_session_info(struct npu_session *session)
{
	int ret = 0;
	struct npu_sessionmgr *sessionmgr;
	struct ncp_header *ncp;
	struct group_vector *gcurr;
	u32 isa_size;
	int count;

	sessionmgr = session->cookie;
	ncp = (struct ncp_header *)session->ncp_mem_buf->vaddr;
	mutex_lock(&sessionmgr->mlock);
	session->total_flc_transfer_size = ncp->total_flc_transfer_size;
	session->total_sdma_transfer_size = ncp->total_sdma_transfer_size;
	sessionmgr->cumulative_flc_size += session->total_flc_transfer_size;
	sessionmgr->cumulative_sdma_size += session->total_sdma_transfer_size;

	if (ncp->group_vector_offset < session->ncp_mem_buf->size) {
		gcurr = (struct group_vector *)((u64)ncp + ncp->group_vector_offset);
	} else {
		npu_err("memory vector offset(0x%x) > max size(0x%x), out of bounds\n",
						(u32)ncp->group_vector_offset, (u32)session->ncp_mem_buf->size);
		return -EFAULT;
	}

	isa_size = 0;
	for (count = 0; count < ncp->group_vector_cnt; ++count, ++gcurr)
		isa_size += gcurr->isa_size;

	session->cmdq_isa_size = isa_size;

	if (ncp->thread_vector_cnt > (NPU_MAX_CORES_ALLOWED-1)) {
		npu_err("please change the macro NPU_MAX_CORES_ALLOWED to (%d + 1)\n", ncp->thread_vector_cnt);
		return -EINVAL;
	}
	sessionmgr->count_thread_ncp[ncp->thread_vector_cnt]++;
	session->inferencefreq_index = 0;
	session->last_q_time_stamp = npu_get_time_us();
	for (count = 0; count < NPU_Q_TIMEDIFF_WIN_MAX; count++)
		session->inferencefreq_window[count] = 0;
	session->inferencefreq = 0;
	npu_trace("npu_session_start: %d %d %d %d %d %d", session->inferencefreq_window[0],
						session->inferencefreq_window[1],
						session->inferencefreq_window[2],
						session->inferencefreq_window[3],
						session->inferencefreq_window[4],
						session->inferencefreq
					);
	mutex_unlock(&sessionmgr->mlock);

	return ret;
}

void npu_arbitration_session_stop(struct npu_session *session)
{
	struct npu_sessionmgr *sessionmgr;
	struct ncp_header *ncp;
	int count;

	sessionmgr = session->cookie;
	ncp = (struct ncp_header *)session->ncp_mem_buf->vaddr;
	mutex_lock(&sessionmgr->mlock);
	sessionmgr->cumulative_flc_size -= session->total_flc_transfer_size;
	sessionmgr->cumulative_sdma_size -= session->total_sdma_transfer_size;
	sessionmgr->count_thread_ncp[ncp->thread_vector_cnt]--;
	session->inferencefreq_index = 0;
	for (count = 0; count < NPU_Q_TIMEDIFF_WIN_MAX; count++)
		session->inferencefreq_window[count] = 0;
	session->inferencefreq = 0;
	npu_trace("npu_session_stop: %d %d %d %d %d %d", session->inferencefreq_window[0],
						session->inferencefreq_window[1],
						session->inferencefreq_window[2],
						session->inferencefreq_window[3],
						session->inferencefreq_window[4],
						session->inferencefreq
					);
	mutex_unlock(&sessionmgr->mlock);
}

void npu_arbitration_info_update(struct npu_session *session)
{
	u64 now, maxvalue = -1;
	u32 temp;

	now = npu_get_time_us();

	temp = session->inferencefreq * NPU_Q_TIMEDIFF_WIN_MAX - session->inferencefreq_window[session->inferencefreq_index];
	if (now > session->last_q_time_stamp)
		session->inferencefreq_window[session->inferencefreq_index] = (u32)(1000000/(now - session->last_q_time_stamp));
	else
		session->inferencefreq_window[session->inferencefreq_index] = (u32)(1000000/( (maxvalue - session->last_q_time_stamp) + now));

	session->inferencefreq = (temp + session->inferencefreq_window[session->inferencefreq_index])/NPU_Q_TIMEDIFF_WIN_MAX;
	session->last_q_time_stamp = now;

	session->inferencefreq_index = (session->inferencefreq_index + 1) % NPU_Q_TIMEDIFF_WIN_MAX;
	npu_dbg("infFreq=%d addrofInfFreq = 0x%llx window = 0x%x 0x%x 0x%x 0x%x 0x%x %llx",
			session->inferencefreq, &session->inferencefreq,
			session->inferencefreq_window[0],
			session->inferencefreq_window[1],
			session->inferencefreq_window[2],
			session->inferencefreq_window[3],
			session->inferencefreq_window[4],
			session->last_q_time_stamp);
}

#else /* !CONFIG_NPU_ARBITRATION */

static inline int __arbitration_session_info(struct npu_session *session) { return 0; }
static inline void npu_arbitration_session_stop(struct npu_session *session) { }
static inline void npu_arbitration_info_update(struct npu_session *session) { }

#endif /* CONFIG_NPU_ARBITRATION */


int npu_session_s_graph(struct npu_session *session, struct vs4l_graph *info)
{
	int ret = 0;
	BUG_ON(!session);
	BUG_ON(!info);
	ret = __get_session_info(session, info);
	if (unlikely(ret)) {
		npu_uerr("invalid in __get_session_info\n", session);
		goto p_err;
	}

#ifdef CONFIG_DSP_USE_VS4L
	/* jump by hids */
	if (session->hids & NPU_HWDEV_ID_NPU &&
		session->hids & NPU_HWDEV_ID_DSP) {
	}
	else if (session->hids & NPU_HWDEV_ID_NPU) {
#endif
		ret = __config_session_info(session);
		if (unlikely(ret)) {
			npu_uerr("invalid in __config_session_info\n", session);
			goto p_err;
		}

		ret = __arbitration_session_info(session);
		if (unlikely(ret)) {
			npu_uerr("invalid in __arbitration_session_info\n", session);
			goto p_err;
		}

		return ret;
#ifdef CONFIG_DSP_USE_VS4L
	}
	else if (session->hids & NPU_HWDEV_ID_DSP) {
		ret = __config_session_info_for_dsp(session);
		if (unlikely(ret)) {
			npu_uerr("invalid in __config_session_info_for_dsp\n",
					session);
			goto p_err_dsp;
		}

		ret = __arbitration_session_info(session);
		if (unlikely(ret)) {
			npu_uerr("invalid in __arbitration_session_info\n",
					session);
			goto p_err_dsp;
		}

		return ret;
p_err_dsp:
		npu_uerr("Clean-up buffers for messages\n", session);
		__remove_buf_from_load_msg(session);
	}
	else {
		npu_uerr("Invalid HW device ids\n", session);
	}
#endif
p_err:
	npu_uerr("Clean-up buffers for graph\n", session);
	__release_graph_ion(session);

	return ret;
}

int npu_session_start(struct npu_queue *queue)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	BUG_ON(!queue);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	BUG_ON(!vctx);
	BUG_ON(!session);

	session->ss_state |= BIT(NPU_SESSION_STATE_START);
	return ret;
}

int npu_session_NW_CMD_STREAMON(struct npu_session *session)
{
	nw_cmd_e nw_cmd = NPU_NW_CMD_STREAMON;

	BUG_ON(!session);

	profile_point1(PROBE_ID_DD_NW_RECEIVED, session->uid, 0, nw_cmd);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);

	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);
	profile_point1(PROBE_ID_DD_NW_NOTIFIED, session->uid, 0, nw_cmd);

	return 0;
}

int npu_session_streamoff(struct npu_queue *queue)
{
	BUG_ON(!queue);
	return 0;
}

int npu_session_NW_CMD_STREAMOFF(struct npu_session *session)
{
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	nw_cmd_e nw_cmd = NPU_NW_CMD_STREAMOFF;

	BUG_ON(!session);

	/* check npu_device emergency error */
	vctx = &session->vctx;
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	npu_udbg("sending STREAM OFF command.\n", session);
	profile_point1(PROBE_ID_DD_NW_RECEIVED, session->uid, 0, nw_cmd);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);
	profile_point1(PROBE_ID_DD_NW_NOTIFIED, session->uid, 0, nw_cmd);

	if (npu_device_is_emergency_err(device)) {
		npu_udbg("NPU DEVICE IS EMERGENCY ERROR !\n", session);
		npu_udbg("sending CLEAR_CB command.\n", session);
		npu_session_put_nw_req(session, NPU_NW_CMD_CLEAR_CB);
		/* Clear CB has no notify function */
	}

	return 0;
}

int npu_session_stop(struct npu_queue *queue)
{
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;

	BUG_ON(!queue);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	npu_arbitration_session_stop(session);

	session->ss_state |= BIT(NPU_SESSION_STATE_STOP);

	return 0;
}

int npu_session_format(struct npu_queue *queue, struct vs4l_format_list *flist)
{
	int ret = 0;
	u32 i = 0;
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;

	struct address_vector *av = NULL;
	struct memory_vector *mv = NULL;

	char *ncp_vaddr;
	struct vs4l_format *formats;
	struct addr_info *FM_av;
	u32 address_vector_offset;
	u32 address_vector_cnt;

	u32 memory_vector_offset;
	u32 memory_vector_cnt;

	u32 cal_size;
	u32 bpp;
	u32 channels;
	u32 width;
	u32 height;

	u32 FM_cnt;

	BUG_ON(!queue);
	BUG_ON(!flist);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	BUG_ON(!vctx);
	BUG_ON(!session);

#ifdef CONFIG_NPU_USE_HW_DEVICE
	if (session->hids & NPU_HWDEV_ID_NPU)
		ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;
	else
		ncp_vaddr = (char *)session->ncp_mem_buf->vaddr;
#else
	ncp_vaddr = (char *)session->ncp_hdr_buf->vaddr;
#endif

	address_vector_offset = session->address_vector_offset;
	address_vector_cnt = session->address_vector_cnt;

	memory_vector_offset = session->memory_vector_offset;
	memory_vector_cnt = session->memory_vector_cnt;

	if (memory_vector_cnt && memory_vector_offset)
		mv = (struct memory_vector *)(ncp_vaddr + memory_vector_offset);
	if (address_vector_cnt && address_vector_offset)
		av = (struct address_vector *)(ncp_vaddr + address_vector_offset);

	formats = flist->formats;

	if (flist->direction == VS4L_DIRECTION_IN) {
		FM_av = session->IFM_info[0][0].addr_info;
		FM_cnt = session->IFM_cnt;
	} else {
		FM_av = session->OFM_info[0][0].addr_info;
		FM_cnt = session->OFM_cnt;
	}

	npu_udbg("flist %d, FM_cnt %d\n", session, flist->count, FM_cnt);
#ifdef CONFIG_NPU_USE_HW_DEVICE
	if(!(session->hids & NPU_HWDEV_ID_DSP))
#endif
	{
		if (FM_cnt > flist->count || FM_cnt + FM_SHARED_NUM < flist->count) {
			ret = -EINVAL;
			goto p_err;
		}

		for (i = 0; i < FM_cnt; i++) {
			if (mv) {
				(mv + (FM_av + i)->av_index)->wstride = (formats + i)->stride;
				(mv + (FM_av + i)->av_index)->cstride = (formats + i)->cstride;
			}

			bpp = (formats + i)->pixel_format;
			channels = (formats + i)->channels;
			width = (formats + i)->width;
			height = (formats + i)->height;
			cal_size = (bpp / 8) * channels * width * height;

			npu_udbg("dir(%d), av_index(%d)\n", session, flist->direction, (FM_av + i)->av_index);
			npu_udbg("dir(%d), av_size(%zu), cal_size(%u)\n", session, flist->direction, (FM_av + i)->size, cal_size);
#ifndef SYSMMU_FAULT_TEST
			if ((FM_av + i)->size > cal_size) {
				npu_uinfo("in_size(%zu), cal_size(%u) invalid\n", session, (FM_av + i)->size, cal_size);
				ret = NPU_ERR_DRIVER(NPU_ERR_SIZE_NOT_MATCH);
				goto p_err;
			}
#endif
		}
	}

	if (flist->direction == VS4L_DIRECTION_IN) {
		session->ss_state |= BIT(NPU_SESSION_STATE_FORMAT_IN);
	} else {
		session->ss_state |= BIT(NPU_SESSION_STATE_FORMAT_OT);
	}
p_err:
	npu_uinfo("direction (%d)\n", session, flist->direction);
	return ret;
}

#ifdef CONFIG_NPU_USE_BOOT_IOCTL
int npu_session_NW_CMD_POWER_NOTIFY(struct npu_session *session, bool on)
{
	int ret = 0;
	struct npu_sessionmgr *sessionmgr;
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	int session_cnt = 0;
	nw_cmd_e nw_cmd = NPU_NW_CMD_POWER_CTL;

	if (!session) {
		npu_err("invalid session\n");
		ret = -EINVAL;
		return ret;
	}

	mutex_lock(session->global_lock);

	sessionmgr = session->cookie;
	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	if (session->hids & NPU_HWDEV_ID_NPU)
		session_cnt = atomic_read(&sessionmgr->npu_hw_cnt);
	else
		session_cnt = atomic_read(&sessionmgr->dsp_hw_cnt);

	if (on) {
		if (session_cnt > 1)  {
			mutex_unlock(session->global_lock);
			return ret;
		}
		dsp_dhcp_update_pwr_status(device, session->hids, true);
	}
	else  {
		if (session_cnt > 0) {
			mutex_unlock(session->global_lock);
			return ret;
		}
		dsp_dhcp_update_pwr_status(device, session->hids, false);
	}

	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);

	mutex_unlock(session->global_lock);

	return ret;
}

int npu_session_restore_cnt(struct npu_session *session)
{
	int ret = 0;
	struct npu_sessionmgr *sessionmgr;

	mutex_lock(session->global_lock);

	sessionmgr = session->cookie;
	atomic_xchg(&sessionmgr->npu_hw_cnt, sessionmgr->npu_hw_cnt_saved);
	atomic_xchg(&sessionmgr->npu_hw_cnt, sessionmgr->npu_hw_cnt_saved);

	mutex_unlock(session->global_lock);

	return ret;
}

int npu_session_NW_CMD_RESUME(struct npu_session *session)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	nw_cmd_e nw_cmd = NPU_NW_CMD_POWER_CTL;

	if (!session) {
		npu_err("invalid session\n");
		ret = -EINVAL;
		return ret;
	}

	mutex_lock(session->global_lock);

	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	dsp_dhcp_update_pwr_status(device, session->hids, true);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);

	mutex_unlock(session->global_lock);
	return ret;
}

int npu_session_NW_CMD_SUSPEND(struct npu_session *session)
{
	int ret = 0;
#if (CONFIG_NPU_MAILBOX_VERSION == 9)
	int cnt = 0;
#endif
	struct npu_sessionmgr *sessionmgr;
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	nw_cmd_e nw_cmd = NPU_NW_CMD_POWER_CTL;

	if (!session) {
		npu_err("invalid session\n");
		ret = -EINVAL;
		return ret;
	}

	mutex_lock(session->global_lock);

	sessionmgr = session->cookie;
	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	sessionmgr->npu_hw_cnt_saved = 0;
	sessionmgr->npu_hw_cnt_saved = atomic_xchg(&sessionmgr->npu_hw_cnt, sessionmgr->npu_hw_cnt_saved);
	sessionmgr->dsp_hw_cnt_saved = 0;
	sessionmgr->dsp_hw_cnt_saved = atomic_xchg(&sessionmgr->dsp_hw_cnt, sessionmgr->dsp_hw_cnt_saved);

	dsp_dhcp_update_pwr_status(device, session->hids, false);
	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);

	/* Waiting for FW to update the flag */
#if (CONFIG_NPU_MAILBOX_VERSION == 9)
#define BOOT_TYPE_WARM_BOOT (0xb0080c0d)
	ret = -EFAULT;
	do {
		if (device->system.mbox_hdr->boottype == BOOT_TYPE_WARM_BOOT) {
			ret = 0;
			break;
		}
		msleep(1);
	} while (cnt++ < 100); /* Maximum 100 retries */
#endif
	if (!ret) {
		sessionmgr->npu_hw_cnt_saved = 0;
		sessionmgr->npu_hw_cnt_saved = atomic_xchg(&sessionmgr->npu_hw_cnt, sessionmgr->npu_hw_cnt_saved);
		sessionmgr->dsp_hw_cnt_saved = 0;
		sessionmgr->dsp_hw_cnt_saved = atomic_xchg(&sessionmgr->dsp_hw_cnt, sessionmgr->dsp_hw_cnt_saved);
	}

	mutex_unlock(session->global_lock);

	return ret;
}
#endif

static struct npu_memory_buffer *alloc_npu_load_payload(struct npu_session *session)
{
	struct npu_memory_buffer *buffer;
	int ret;

	buffer = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	/* 2 payloads.
	 * - User NCP
	 * - Kernel copy and updated NCP header.
	 */
	buffer->size = sizeof(struct cmd_load_payload) * 2;

	ret = npu_memory_alloc(session->memory, buffer, configs[NPU_PBHA_HINT_00]);
	if (ret) {
		npu_err("failed to allocate payload buffer(%d)\n", ret);
		goto err_free_buffer;
	}

	return buffer;
err_free_buffer:
	kfree(buffer);
	return ERR_PTR(ret);
}

int npu_session_NW_CMD_LOAD(struct npu_session *session)
{
	int ret = 0;
	nw_cmd_e nw_cmd = NPU_NW_CMD_LOAD;

	if (unlikely(!session)) {
		npu_err("invalid session\n");
		ret = -EINVAL;
		return ret;
	}

	profile_point1(PROBE_ID_DD_NW_RECEIVED, session->uid, 0, nw_cmd);

#ifdef CONFIG_NPU_USE_HW_DEVICE
	if (session->hids & NPU_HWDEV_ID_NPU)
#endif
	{
		session->ncp_payload = alloc_npu_load_payload(session);
		if (IS_ERR_OR_NULL(session->ncp_payload)) {
			ret = PTR_ERR(session->ncp_payload);
			session->ncp_payload = NULL;
			return ret;
		}
	}

	session->nw_result.result_code = NPU_NW_JUST_STARTED;
	npu_session_put_nw_req(session, nw_cmd);
	wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);
	profile_point1(PROBE_ID_DD_NW_NOTIFIED, session->uid, 0, nw_cmd);
	return ret;
}

int __update_qbuf_IOFM_daddr(struct npu_session *session, struct addr_info *IFM_addr, struct addr_info *OFM_addr, u32 j)
{
	int ret = 0;
	u32 i = 0;
	struct address_vector *IOFM_av;
	struct npu_memory_buffer *IOFM_mem_buf;
	u32 IFM_cnt = session->IFM_cnt;
	u32 OFM_cnt = session->OFM_cnt;
	u32 IOFM_cnt = session->IOFM_cnt;

	int cur_IOFM_idx = session->qbuf_IOFM_idx;

	if (cur_IOFM_idx == -1 || cur_IOFM_idx >= VISION_MAX_CONTAINERLIST) {
		ret = -EINVAL;
		goto p_err;
	}

	if (j >= VISION_MAX_BUFFER) {
		ret = -EINVAL;
		goto p_err;
	}


	IOFM_mem_buf = session->IOFM_mem_buf;
	IOFM_av = (struct address_vector *)(IOFM_mem_buf->vaddr);
	IOFM_av = IOFM_av + (cur_IOFM_idx*VISION_MAX_BUFFER + j)*IOFM_cnt;

	for (i = 0; i < IOFM_cnt; i++) {
		if (i < IFM_cnt) {
			(IOFM_av + i)->m_addr = (IFM_addr + i)->daddr;
			npu_udbg("update_IOFM_ion, m_addr(%d/0x%x)\n", session, i, (IOFM_av + i)->m_addr);
		} else {
			if (OFM_cnt){
				(IOFM_av + i)->m_addr = (OFM_addr + (i - IFM_cnt))->daddr;
				npu_udbg("update_OFM_ion, m_addr(%d/0x%x)\n", session, i, (IOFM_av + i)->m_addr);
			}
		}
	}
p_err:
	return ret;
}

void npu_session_restart(void)
{
	npu_buffer_q_restart();
}

static int npu_session_queue(struct npu_queue *queue, struct vb_container_list *incl, struct vb_container_list *otcl)
{
	int ret = 0;
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;

	struct mbox_process_dat *mbox_process_dat;

	struct av_info *IFM_info;
	struct av_info *OFM_info;

	struct addr_info *IFM_addr;
	struct addr_info *OFM_addr;
	u32 j;
	u32 buff_cnt;

	frame_cmd_e frame_cmd = NPU_FRAME_CMD_Q;

	BUG_ON(!queue);
	BUG_ON(!incl);
	/*BUG_ON(!incl->index >= configs[NPU_MAX_FRAME]);*/
	BUG_ON(!otcl);
	/*BUG_ON(!otcl->index >= configs[NPU_MAX_FRAME]);*/

	if (otcl->timestamp[5].tv_usec == 1) {
		frame_cmd = NPU_FRAME_CMD_PROFILER;
	} else {
		frame_cmd = NPU_FRAME_CMD_Q;
	}

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	session->qbuf_IOFM_idx = incl->index;

	atomic_set(&otcl->containers[0].completed_count, 0);
	buff_cnt = otcl->containers[0].count;

#ifdef CONFIG_NPU_USE_HW_DEVICE
	if ((session->hids == NPU_HWDEV_ID_NPU) &&
	    (session->IMB_size > configs[NPU_IMB_THRESHOLD_SIZE])) {
#else
	if (session->IMB_size > configs[NPU_IMB_THRESHOLD_SIZE]) {
#endif
		npu_udbg("IMB: alloc in qbuf\n", session);
		ret = __ion_alloc_IMB(session, &session->IMB_info, session->IMB_mem_buf);
		if (unlikely(ret)) {
			npu_uerr("IMB: fail(%d) in ion alloc IMB\n", session, ret);
			goto p_err;
		}
		session->ss_state |= BIT(NPU_SESSION_STATE_IMB_ION_ALLOC);
		npu_session_ion_sync_for_device(session->ncp_hdr_buf, 0, session->ncp_hdr_buf->size, DMA_TO_DEVICE);
	}

#ifdef CONFIG_NPU_USE_HW_DEVICE
	if (session->hids == NPU_HWDEV_ID_DSP && buff_cnt == 0)
		buff_cnt++;
#endif
	for (j = 0; j < buff_cnt; j++)	{
		IFM_info = &session->IFM_info[incl->index][j];
		IFM_addr = IFM_info->addr_info;
		IFM_info->IOFM_buf_idx = incl->index;

		OFM_info = &session->OFM_info[otcl->index][j];
		OFM_addr = OFM_info->addr_info;
		OFM_info->IOFM_buf_idx = otcl->index;

		/* Save profile data */
		profile_point1(PROBE_ID_DD_FRAME_EQ, session->uid, incl->id, incl->direction);
		profile_point1(PROBE_ID_DD_FRAME_EQ, session->uid, otcl->id, otcl->direction);

		npu_udbg("uid %d, in id %d, idx %d, out id %d, idx %d\n", session,
				session->uid, incl->id, incl->index, otcl->id, otcl->index);
		ret = __update_qbuf_IOFM_daddr(session, IFM_addr, OFM_addr, j);
		if (unlikely(ret)) {
			goto p_err;
		}

		mbox_process_dat = &session->mbox_process_dat;
		mbox_process_dat->address_vector_cnt = session->IOFM_cnt;
		mbox_process_dat->address_vector_start_daddr = (session->IOFM_mem_buf->daddr)
			+ ((session->qbuf_IOFM_idx*VISION_MAX_BUFFER + j)*session->IOFM_cnt) * (sizeof(struct address_vector));

		ret = npu_session_put_frame_req(session, queue, incl, otcl, frame_cmd, IFM_info, OFM_info,
											incl->id*VISION_MAX_BUFFER + j);
		if (unlikely(ret))
			goto p_err;

		IFM_info->state = SS_BUF_STATE_QUEUE;
		OFM_info->state = SS_BUF_STATE_QUEUE;
	}

	npu_arbitration_info_update(session);

	return 0;
p_err:
	return ret;
}

static int npu_session_deque(struct npu_queue *queue, struct vb_container_list *clist)
{
	int ret = 0;

	struct npu_session *session;
	struct npu_vertex_ctx *vctx;
	int j;
	u32 buff_cnt;
	u32 containerListIndex = clist->index;

	struct av_info *FM_info;

	BUG_ON(!queue);
	BUG_ON(!clist);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	if (unlikely(clist->index >= VISION_MAX_CONTAINERLIST)) {
		ret = -EINVAL;
		goto p_err;
	}

	if (clist->direction == VS4L_DIRECTION_OT)
#ifdef CONFIG_NPU_USE_HW_DEVICE
		if ((session->hids == NPU_HWDEV_ID_NPU) &&
		    (session->IMB_size > configs[NPU_IMB_THRESHOLD_SIZE])) {
#else
		if (session->IMB_size > configs[NPU_IMB_THRESHOLD_SIZE]) {
#endif
			npu_udbg("IMB: buffer free in dqbuf\n", session);
#ifdef CONFIG_NPU_USE_IMB_ALLOCATOR
			/* deallocate of unnecessary memory chunks over the NPU_IMB_THRESHOLD_SIZE limit */
			__free_imb_mem_buf(session, false);
#else /* !CONFIG_NPU_USE_IMB_ALLOCATOR */
			npu_memory_free(session->memory, session->IMB_mem_buf);
#endif
		}

	buff_cnt = clist->containers[0].count;
	for (j = 0; j < buff_cnt; j++) {
		if (clist->direction == VS4L_DIRECTION_IN) {
			FM_info = &session->IFM_info[containerListIndex][j];
		} else {
			FM_info = &session->OFM_info[containerListIndex][j];
		}

		FM_info->state = SS_BUF_STATE_DEQUEUE;
		session->dqbuf_IOFM_idx = clist->index;
		/* Save profile data */
		profile_point1(PROBE_ID_DD_FRAME_DQ, session->uid, clist->id, clist->direction);

		npu_udbg("success in %s()\n", session, __func__);
	}
p_err:
	return ret;
}

const struct npu_queue_ops npu_queue_ops = {
	.start		= npu_session_start,
	.stop		= npu_session_stop,
	.format	= npu_session_format,
	.queue		= npu_session_queue,
	.deque		= npu_session_deque,
	.streamoff	= npu_session_streamoff,
};

static int npu_session_control(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_request(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_process(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_cancel(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_done(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_get_resource(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_put_resource(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

static int npu_session_update_param(struct npu_session *session, struct npu_frame *frame)
{
	int ret = 0;

	return ret;
}

const struct npu_session_ops npu_session_ops = {
	.control	= npu_session_control,
	.request	= npu_session_request,
	.process	= npu_session_process,
	.cancel	= npu_session_cancel,
	.done		= npu_session_done,
	.get_resource	= npu_session_get_resource,
	.put_resource	= npu_session_put_resource,
	.update_param	= npu_session_update_param
};

static int npu_session_prepare(struct vb_queue *q, struct vb_container_list *clist)
{
	int ret = 0;
	struct npu_queue *queue = q->private_data;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	struct vb_buffer *buffer;
	struct av_info *FM_info;
	struct addr_info *FM_addr;
	struct vb_container *container;

	u32 FM_cnt, count;
	u32 i, j;
	u32 buf_cnt;
	u32 contailerListIndex;

	BUG_ON(!queue);
	BUG_ON(!clist);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	contailerListIndex = clist->index;

	if (clist->index >= VISION_MAX_CONTAINERLIST) {
		ret = -EINVAL;
		goto p_err;
	}

#ifdef CONFIG_NPU_USE_HW_DEVICE
	if (session->hids == NPU_HWDEV_ID_DSP && clist->direction == 1)
		count = clist->count - 1;
	else
		count = clist->count;
#else
	count = clist->count;
#endif
	for (i = 0; i < count ; i++) {
		container = &clist->containers[i];
		buf_cnt = clist->containers[i].count;

		for (j = 0; j < buf_cnt; j++) {
			npu_udbg("i %d/%d, j %d/%d\n", session, i, count, j, buf_cnt);
			if (clist->direction == VS4L_DIRECTION_IN) {
				FM_info = &session->IFM_info[contailerListIndex][j];
				FM_cnt = session->IFM_cnt;
			} else {
				FM_info = &session->OFM_info[contailerListIndex][j];
				FM_cnt = session->OFM_cnt;
			}

			npu_udbg("clist %d, FM_cnt %d\n", session, count, FM_cnt);
			if (FM_cnt > count || FM_cnt + FM_SHARED_NUM < count) {
				npu_uerr("dir(%d), FM_cnt(%u) != count(%u)\n", session, clist->direction, FM_cnt, count);
				ret = -EINVAL;
				goto p_err;
			}

			FM_addr = FM_info->addr_info;
			buffer = &container->buffers[j];
			(FM_addr + i)->daddr = buffer->daddr;
			(FM_addr + i)->vaddr = buffer->vaddr;
		}
		FM_info->address_vector_cnt = count;
		FM_info->state = SS_BUF_STATE_PREPARE;
	}

p_err:
	return ret;
}

static int npu_session_unprepare(struct vb_queue *q, struct vb_container_list *clist)
{
	int ret = 0;
	struct npu_queue *queue = q->private_data;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	struct av_info *FM_info;

	u32 k;

	BUG_ON(!queue);
	BUG_ON(!clist);

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	if (clist->index >= VISION_MAX_CONTAINERLIST) {
		ret = -EINVAL;
		goto p_err;
	}

	for (k = 0; k < VISION_MAX_BUFFER; k++) {
		if (clist->direction == VS4L_DIRECTION_IN) {
			FM_info = &session->IFM_info[clist->index][k];
		} else {
			FM_info = &session->OFM_info[clist->index][k];
		}

		FM_info->state = SS_BUF_STATE_UNPREPARE;
	}
p_err:
	return ret;
}

/* S_PARAM handler list definition - Chain of responsibility pattern */
typedef npu_s_param_ret (*fn_s_param_handler)(struct npu_session *, struct vs4l_param *, int *);
fn_s_param_handler s_param_handlers[] = {
	NULL,	/* NULL termination is required to denote EOL */
	npu_scheduler_param_handler,
	npu_qos_param_handler,
	fw_test_s_param_handler,
#ifdef CONFIG_DSP_USE_VS4L
	dsp_kernel_param_handler
#endif
};

int npu_session_param(struct npu_session *session, struct vs4l_param_list *plist)
{
	npu_s_param_ret		ret;
	int			retval = 0;
	struct vs4l_param	*param;
	u32			i, target;
	fn_s_param_handler	*p_fn;

	/* Search over each set param request over handlers */
	for (i = 0; i < plist->count; i++) {
		param = &plist->params[i];
		target = param->target;
		npu_udbg("Try set param [%u/%u] - target: [0x%x]\n",
			session, i+1, plist->count, target);

		p_fn = s_param_handlers;
		if ((target >= NPU_S_PARAM_PERF_MODE) &&
				(target < NPU_S_PARAM_SCH_PARAM_END)) {
			p_fn += 1;
		} else if ((target >= NPU_S_PARAM_QOS_NPU) &&
				(target < NPU_S_PARAM_QOS_PARAM_END)) {
			p_fn += 2;
		} else if ((target >= NPU_S_PARAM_FW_UTC_LOAD) &&
				(target < NPU_S_PARAM_FW_PARAM_END)) {
			p_fn += 3;
#ifdef CONFIG_DSP_USE_VS4L
		} else if ((target >= NPU_S_PARAM_DSP_KERNEL) &&
				(target < NPU_S_PARAM_DSP_PARAM_END)) {
			p_fn += 4;
#endif
		} else {
			npu_uwarn("No handler defined for target [0x%x]."
				"Skipping.\n", session, param->target);
			/* Continue process others but retuen -EINVAL */
			retval = -EINVAL;
			continue;
		}

		ret = (*p_fn)(session, param, &retval);
		if (ret != S_PARAM_NOMB)
			npu_udbg("Handled by handler at [%pK](%d)\n",
						session, *p_fn, ret);
	}

	return retval;
}

const struct vb_ops vb_ops = {
	.buf_prepare = npu_session_prepare,
	.buf_unprepare = npu_session_unprepare
};

int npu_session_nw_sched_param(struct npu_session *session, struct vs4l_sched_param *param)
{
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;

	int		retval = 0;
	u32		priority;
	u32		bound_id;
	u32		max_npu_core;

	vctx = &(session->vctx);
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);

	priority = param->priority;
	bound_id = param->bound_id;
	max_npu_core = session->sched_param.max_npu_core;
	/* Check priority range */
	if (priority <= NPU_PRIORITY_MAX_VAL) {
		session->sched_param.priority = priority;
	} else {
		retval = -EINVAL;
	}
	/* Check boundness to the core */
	if ((bound_id < max_npu_core) || (bound_id == NPU_BOUND_UNBOUND)) {
		session->sched_param.bound_id = bound_id;
	} else {
		retval = -EINVAL;
	}
	/* Set the default value if anything is wrong */
	if (retval != 0) {
		session->sched_param.priority = NPU_PRIORITY_MIN_VAL;
		session->sched_param.bound_id = NPU_BOUND_UNBOUND;
	}

	npu_dbg("r[%d], p[%u], b[%u], c[%u]\n",
			retval, priority, bound_id, max_npu_core);
	npu_scheduler_update_sched_param(device, session);

	return retval;
}

int npu_session_register_undo_cb(struct npu_session *session, session_cb cb)
{
	BUG_ON(!session);

	session->undo_cb = cb;

	return 0;
}

int npu_session_execute_undo_cb(struct npu_session *session)
{
	BUG_ON(!session);

	if (session->undo_cb)
		session->undo_cb(session);

	return 0;
}

int npu_session_undo_open(struct npu_session *session)
{
	int ret = 0;

	BUG_ON(!session);

	if (session->ss_state < BIT(NPU_SESSION_STATE_OPEN)) {
		ret = -EINVAL;
		goto p_err;
	}

	if (session->ss_state <= BIT(NPU_SESSION_STATE_OPEN))
		goto session_free;

	npu_sessionmgr_unregID(session->cookie, session);

session_free:
	kfree(session);
	session = NULL;
p_err:
	return ret;
}

int npu_session_undo_s_graph(struct npu_session *session)
{
	int ret = 0;

	if (!_undo_s_graph_each_state(session)) {
		npu_session_register_undo_cb(session, NULL);
		goto next_cb;
	}
	ret = -EINVAL;
	return ret;
next_cb:
	npu_session_execute_undo_cb(session);
	return ret;
}

int npu_session_undo_close(struct npu_session *session)
{
	int ret = 0;

	if (!_undo_close_each_state(session)) {
		npu_session_register_undo_cb(session, npu_session_undo_s_graph);
		goto next_cb;
	}
	ret = -EINVAL;
	return ret;
next_cb:
	npu_session_execute_undo_cb(session);
	return ret;
}

int npu_session_flush(struct npu_session *session)
{
	int ret = 0;

	if (!session)
	{
		ret = -EINVAL;
		return ret;
	}
	else
	{
		struct vs4l_param	param;
		int	retval = 0;

		param.target = NPU_S_PARAM_PERF_MODE;
		param.offset = NPU_PERF_MODE_NORMAL;

		ret = npu_scheduler_param_handler(session, &param, &retval);

		if (ret == S_PARAM_ERROR)
			npu_err("fail(%d) in npu_session_param\n", ret);

		_undo_s_graph_each_state(session);
	}

	return 0;
}
