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

#include <linux/atomic.h>
#include <linux/mutex.h>

#define NPU_LOG_TAG	"if-protodrv-mbox2"

#include "npu-if-protodrv-mbox2.h"
#include "npu-log.h"
#include "interface/hardware/npu-interface.h"
#ifdef CONFIG_DSP_USE_VS4L
#include "npu-hw-device.h"
#include "dsp-dhcp.h"
#endif
#ifdef CONFIG_NPU_USE_FENCE_SYNC
#include "npu-util-common.h"
#include "npu-util-regs.h"
#endif

extern struct npu_proto_drv *protodr;

const struct npu_if_protodrv_mbox protodrv_mbox = {
	.dev = NULL,
	.npu_if_protodrv_mbox_ops = &protodrv_mbox_ops,
};

int npu_mbox_op_register_notifier(protodrv_notifier sig_func)
{
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->register_notifier))
		return protodrv_mbox.npu_if_protodrv_mbox_ops->register_notifier(sig_func);

	npu_warn("not defined: register_notifier()\n");
	return 0;
}

/* nw_mbox_ops -> Use npu-if-protodrv-mbox object stored in npu_proto_drv */
int npu_nw_mbox_op_is_available(void)
{
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->nw_result_available))
		return protodrv_mbox.npu_if_protodrv_mbox_ops->nw_result_available();

	npu_warn("not defined: nw_result_available()\n");
	return 0;
}
int npu_nw_mbox_ops_get(struct msgid_pool *pool, struct proto_req_nw **target)
{
	int msgid = 0;
	struct npu_nw nw;
	int ret = 0;

	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->nw_get_result))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->nw_get_result(&msgid, &nw);
	else {
		npu_warn("not defined: nw_get_result()\n");
		*target = NULL;
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* No message */
		*target = NULL;
		return ret;
	} else {
		/* Message available */
		*target = msgid_claim_get_ref(pool, msgid, PROTO_DRV_REQ_TYPE_NW);

		if (likely(*target != NULL)) {
			npu_uinfo("mbox->protodrv : NW msgid(%d)\n", &(*target)->nw, msgid);
			(*target)->nw.msgid = -1;
			(*target)->nw.result_code = nw.result_code;
			(*target)->nw.result_value = nw.result_value;
			npu_log_protodrv_set_data(&(*target)->nw, PROTO_DRV_REQ_TYPE_NW, false);
			return 1;
		} else {
			npu_err("failed to find request mapped with msgid[%d]\n", msgid);
			return 0;
		}
	}
}

int npu_nw_mbox_ops_put(struct msgid_pool *pool, struct proto_req_nw *src)
{
	int msgid = 0;
	int ret;

	msgid = msgid_issue_save_ref(pool, PROTO_DRV_REQ_TYPE_NW, src, src->nw.session);
	/* intentional msg id setting for global power down */
#ifndef CONFIG_NPU_USE_BOOT_IOCTL
	if (src->nw.cmd == NPU_NW_CMD_POWER_DOWN)
		msgid = 0;
#endif
	src->nw.msgid = msgid;
	npu_uinfo("protodrv-> : NW msgid(%d)\n", &src->nw, msgid);
	if (unlikely(msgid < 0)) {
		npu_uwarn("no message ID available. Posting message is not temporally available.\n", &src->nw);
		return 0;
	}

	/* Generate mailbox message with given msgid and post it */
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->nw_post_request))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->nw_post_request(msgid, &src->nw);
	else {
		npu_warn("not defined: nw_post_request()\n");
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* Push failed -> return msgid */
		npu_utrace("nw_post_request failed. Reclaiming msgid(%d)\n", &src->nw, msgid);
		msgid_claim(pool, msgid);
	}
	npu_log_protodrv_set_data(&src->nw, PROTO_DRV_REQ_TYPE_NW, true);
	return ret;
}

/* frame_mbox_ops -> Use npu-if-protodrv-mbox object stored in npu_proto_drv */
int npu_frame_mbox_op_is_available(void)
{
	enum channel_flag c = MEDIUM_CHANNEL;

	if (atomic_read(&protodr->high_prio_frame_count) > 0)
		c = HIGH_CHANNEL;
	else if (atomic_read(&protodr->normal_prio_frame_count) > 0)
		c = MEDIUM_CHANNEL;

	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->frame_result_available))
		return protodrv_mbox.npu_if_protodrv_mbox_ops->frame_result_available(c);

	npu_warn("not defined: frame_result_available()\n");
	return 0;
}
int npu_frame_mbox_ops_get(struct msgid_pool *pool, struct proto_req_frame **target)
{
	int msgid = 0;
	struct npu_frame frame;
	int ret = 0;
	enum channel_flag c = MEDIUM_CHANNEL;
#ifdef CONFIG_DSP_USE_VS4L
	struct npu_session *session;
	int npu_hw_time, dsp_hw_time;
#ifdef CONFIG_NPU_USE_FENCE_SYNC
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	u32 pwm_end_time;
	s64 frame_time;
#endif
#endif

	if (atomic_read(&protodr->high_prio_frame_count) > 0)
		c = HIGH_CHANNEL;
	else if (atomic_read(&protodr->normal_prio_frame_count) > 0)
		c = MEDIUM_CHANNEL;

	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->frame_get_result))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->frame_get_result(&msgid, &frame, c);
	else {
		npu_warn("not defined: frame_get_result()\n");
		*target = NULL;
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* No message */
		*target = NULL;
		return ret;
	} else {
		*target = msgid_claim_get_ref(pool, msgid, PROTO_DRV_REQ_TYPE_FRAME);

		if (likely(*target != NULL)) {
			if (c == HIGH_CHANNEL)
				atomic_dec(&protodr->high_prio_frame_count);
			else if (c == MEDIUM_CHANNEL)
				atomic_dec(&protodr->normal_prio_frame_count);
			//npu_ufnotice("mbox->protodrv : frame msgid(%d)\n", &(*target)->frame, msgid);
			(*target)->frame.msgid = -1;
			(*target)->frame.result_code = frame.result_code;
#ifdef CONFIG_DSP_USE_VS4L
			session = (*target)->frame.session;
#ifdef CONFIG_NPU_USE_FENCE_SYNC
			if ((*target)->frame.is_fence) {
				vctx = &session->vctx;
				vertex = vctx->vertex;
				device = container_of(vertex, struct npu_device, vertex);
				pwm_end_time = npu_cmd_map(&device->system, "fwpwm");

				npu_hw_time = 0;
				dsp_hw_time = 0;
				if (session->hids & NPU_HWDEV_ID_NPU) {
					npu_hw_time = PWM_COUNTER_TOTAL_USEC(
						(*target)->frame.pwm_start_time - frame.duration);
					if (npu_hw_time <= 0)
						npu_dbg("profiling npu hw time = %d\n", npu_hw_time);
					(*target)->frame.output->timestamp[5].tv_sec = npu_hw_time;
				} else if (session->hids & NPU_HWDEV_ID_DSP) {
					dsp_hw_time = PWM_COUNTER_TOTAL_USEC(
						(*target)->frame.pwm_start_time - frame.duration);
					if (dsp_hw_time <= 0)
						npu_dbg("profiling dsp hw time = %d\n", dsp_hw_time);
					(*target)->frame.output->timestamp[4].tv_sec = dsp_hw_time;
				} else {
					npu_err("No HWDEV for frame process\n");
				}
				npu_dbg("fence hw time NPU %d, DSP %d\n", npu_hw_time, dsp_hw_time);

				frame_time = PWM_COUNTER_TOTAL_USEC((*target)->frame.pwm_start_time - pwm_end_time);
				npu_dbg("fence end time : %u (%lld)\n", pwm_end_time, frame_time);
				(*target)->frame.output->timestamp[4].tv_usec = frame_time;
				(*target)->frame.is_fence = false;
			} else {
#endif
				npu_hw_time = dsp_dhcp_fw_time(session, NPU_HWDEV_ID_NPU);
				if (npu_hw_time <= 0)
					npu_dbg("profiling npu hw time = %d\n", npu_hw_time);
				(*target)->frame.output->timestamp[5].tv_sec = npu_hw_time;

				dsp_hw_time = dsp_dhcp_fw_time(session, NPU_HWDEV_ID_DSP);
				if (dsp_hw_time <= 0)
					npu_dbg("profiling dsp hw time = %d\n", dsp_hw_time);
				(*target)->frame.output->timestamp[4].tv_sec = dsp_hw_time;
#else
				(*target)->frame.output->timestamp[5].tv_sec = frame.duration;
#endif
#ifdef CONFIG_NPU_USE_FENCE_SYNC
			}
			npu_dbg("get uid %d(%d), msgid %d, frame id %d, index %d, frame 0x%p, tv_usec time : %lx\n",
					session->uid, (*target)->frame.uid, msgid, (*target)->frame.frame_id,
					(*target)->frame.output->index, (void *)&(*target)->frame,
					(*target)->frame.output->timestamp[4].tv_usec);
#endif
			npu_log_protodrv_set_data(&(*target)->frame, PROTO_DRV_REQ_TYPE_FRAME, false);
			return 1;
		} else {
			npu_err("failed to find request mapped with msgid[%d]\n", msgid);
			return 0;
		}
	}
}

int npu_kpi_frame_mbox_ops_get(struct msgid_pool *pool, struct npu_frame **target)
{
	int msgid = 0;
	struct npu_frame frame;
	int ret = 0;
	enum channel_flag c = HIGH_CHANNEL;
	s64 start_time = 0;
#ifdef CONFIG_DSP_USE_VS4L
	struct npu_session *session;
	int npu_hw_time, dsp_hw_time;
#ifdef CONFIG_NPU_USE_FENCE_SYNC
	struct npu_vertex_ctx *vctx;
	struct npu_vertex *vertex;
	struct npu_device *device;
	u32 pwm_end_time;
#endif
#endif

	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->frame_get_result))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->frame_get_result(&msgid, &frame, c);
	else {
		npu_warn("not defined: frame_get_result()\n");
		*target = NULL;
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* No message */
		*target = NULL;
		return ret;
	} else {
		*target = msgid_claim_get_ref(pool, msgid, PROTO_DRV_REQ_TYPE_FRAME);

		if (likely(*target != NULL)) {
			//npu_ufnotice("mbox->protodrv : frame msgid(%d)\n", &(*target)->frame, msgid);
			(*target)->msgid = -1;
			(*target)->result_code = frame.result_code;
#ifdef CONFIG_DSP_USE_VS4L
			session = (*target)->session;
#ifdef CONFIG_NPU_USE_FENCE_SYNC
			if ((*target)->is_fence) {
				npu_hw_time = 0;
				dsp_hw_time = 0;
				if (session->hids & NPU_HWDEV_ID_NPU) {
					npu_hw_time = PWM_COUNTER_TOTAL_USEC(
						(*target)->pwm_start_time - frame.duration);
					if (npu_hw_time <= 0)
						npu_dbg("profiling npu hw time = %d\n", npu_hw_time);
					(*target)->output->timestamp[5].tv_sec = npu_hw_time;
				} else if (session->hids & NPU_HWDEV_ID_DSP) {
					dsp_hw_time = PWM_COUNTER_TOTAL_USEC(
						(*target)->pwm_start_time - frame.duration);
					if (dsp_hw_time <= 0)
						npu_dbg("profiling dsp hw time = %d\n", dsp_hw_time);
					(*target)->output->timestamp[4].tv_sec = dsp_hw_time;
				} else {
					npu_err("No HWDEV for frame process\n");
				}
				npu_dbg("kpi fence hw time NPU %d, DSP %d\n", npu_hw_time, dsp_hw_time);
			} else {
#endif
				npu_hw_time = dsp_dhcp_fw_time(session, NPU_HWDEV_ID_NPU);
				if (npu_hw_time <= 0)
					npu_dbg("profiling npu hw time = %d\n", npu_hw_time);
				(*target)->output->timestamp[5].tv_sec = npu_hw_time;

				dsp_hw_time = dsp_dhcp_fw_time(session, NPU_HWDEV_ID_DSP);
				if (dsp_hw_time <= 0)
					npu_dbg("profiling dsp hw time = %d\n", dsp_hw_time);
				(*target)->output->timestamp[4].tv_sec = dsp_hw_time;
#else
				(*target)->output->timestamp[5].tv_sec = frame.duration;
#endif
#ifdef CONFIG_NPU_USE_FENCE_SYNC
			}
#endif
			/* update the frame duration. we had stored the start_time in timestamp[4] */
#ifdef CONFIG_NPU_USE_FENCE_SYNC
			if ((*target)->is_fence) {
				vctx = &session->vctx;
				vertex = vctx->vertex;
				device = container_of(vertex, struct npu_device, vertex);
				pwm_end_time = npu_cmd_map(&device->system, "fwpwm");
				(*target)->output->timestamp[4].tv_usec = PWM_COUNTER_TOTAL_USEC(
						(*target)->pwm_start_time - pwm_end_time);
				(*target)->is_fence = false;
				npu_dbg("kpi end time : %u (%ld)\n", pwm_end_time,
					(*target)->output->timestamp[4].tv_usec);
			} else {
#endif
				start_time = (*target)->output->timestamp[4].tv_usec;
				(*target)->output->timestamp[4].tv_usec = (npu_get_time_us() - start_time);
#ifdef CONFIG_NPU_USE_FENCE_SYNC
			}
			npu_dbg("get kpi uid %d(%d), msgid %d, frame id %d, index %d, frame 0x%p, tv_usec time : %lx\n",
					session->uid, (*target)->uid, msgid, (*target)->frame_id,
					(*target)->output->index, (*target), (*target)->output->timestamp[4].tv_usec);
#endif
			npu_log_protodrv_set_data(*target, PROTO_DRV_REQ_TYPE_FRAME, false);
			return 1;
		} else {
			npu_err("failed to find request mapped with msgid[%d]\n", msgid);
			return 0;
		}
	}
}

int npu_frame_mbox_ops_put(struct msgid_pool *pool, struct proto_req_frame *src)
{
	int msgid = 0;
	int ret;

	msgid = msgid_issue_save_ref(pool, PROTO_DRV_REQ_TYPE_FRAME, src, src->frame.session);
	src->frame.msgid = msgid;
	if (unlikely(msgid < 0)) {
		npu_ufwarn("no message ID available. Posting message is not temporally available.\n", &src->frame);
		return 0;
	}
	npu_dbg("put uid %d, msgid %d, frame id %d, index %d, frame 0x%p\n",
			src->frame.uid, msgid, src->frame.frame_id, src->frame.output->index, (void *)&src->frame);

	/* Generate mailbox message with given msgid and post it */
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->frame_post_request))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->frame_post_request(msgid, &src->frame);
	else {
		npu_warn("not defined: frame_post_request()\n");
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* Push failed -> return msgid */
		npu_uftrace("frame_post_request failed. Reclaiming msgid(%d)\n", &src->frame, msgid);
		msgid_claim(pool, msgid);
	}
	npu_log_protodrv_set_data(&src->frame, PROTO_DRV_REQ_TYPE_FRAME, true);
	return ret;
}

int npu_kpi_frame_mbox_put(struct msgid_pool *pool, struct npu_frame *frame)
{
	int msgid = 0;
	int ret;

	msgid = msgid_issue_save_ref(pool, PROTO_DRV_REQ_TYPE_FRAME, frame, frame->session);
	frame->msgid = msgid;
	if (unlikely(msgid < 0)) {
		npu_ufwarn("no message ID available. Posting message is not temporally available.\n", frame);
		return 0;
	}
	npu_dbg("put kpi uid %d, msgid %d, frame id %d, index %d, frame 0x%p\n",
			frame->uid, msgid, frame->frame_id, frame->output->index, frame);

	/* store start time in timestamp[4], and update it once the frame is done */
	frame->output->timestamp[4].tv_usec = npu_get_time_us();

	/* Generate mailbox message with given msgid and post it */
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->frame_post_request))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->frame_post_request(msgid, frame);
	else {
		npu_warn("not defined: frame_post_request()\n");
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* Push failed -> return msgid */
		npu_uftrace("frame_post_request failed. Reclaiming msgid(%d)\n", frame, msgid);
		msgid_claim(pool, msgid);
	}
	npu_log_protodrv_set_data(frame, PROTO_DRV_REQ_TYPE_FRAME, true);
	return ret;
}

int npu_kpi_response_read(void)
{
	int ret = 0;
	enum channel_flag c = HIGH_CHANNEL;
	struct npu_frame *frame = NULL;

	if (fr_rslt_available(c) > 0) {
		ret = npu_kpi_frame_mbox_ops_get(&protodr->msgid_pool, &frame);
		if (unlikely(ret <= 0)) {
			npu_err("No frames received\n");
			return ret;
		}
		npu_buffer_q_notify_done(frame);
	}
	return 0;
}

int npu_mbox_op_register_msgid_type_getter(int (*msgid_get_type_func)(int))
{
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->register_msgid_type_getter))
		return protodrv_mbox.npu_if_protodrv_mbox_ops->register_msgid_type_getter(msgid_get_type_func);

	npu_warn("not defined: register_msgid_type_getter()\n");
	return 0;
}
