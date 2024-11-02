/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include "npu-interface.h"
#include "npu-util-llq.h"
#include "npu-vertex.h"
#include "npu-util-memdump.h"
#ifdef CONFIG_DSP_USE_VS4L
#include "npu-util-regs.h"
#include "dsp-common-type.h"
#include "dsp-dhcp.h"
#endif

#define LINE_TO_SGMT(sgmt_len, ptr)	((ptr) & ((sgmt_len) - 1))
static struct workqueue_struct *wq;
static struct work_struct work_report;
static struct workqueue_struct *wq_p;
static struct work_struct work_profile;

#define FW_PROFILE_BUF_SIZE	(1024*1024)
struct fw_profile_info {
	u8 data[FW_PROFILE_BUF_SIZE];
	size_t info_cnt;
	size_t buf_size;
};

static struct fw_profile_info fwProfile;
/* Modify strArr, when changes are there in cmd_done structure */
static const u8 strArr[][128] = {{"fid : "}, {"duration : "}, };
static void __rprt_manager(struct work_struct *w);
static void __profile_manager(struct work_struct *w);
int npu_kpi_response_read(void);
extern struct npu_proto_drv *protodr;

static struct npu_interface interface = {
	.mbox_hdr = NULL,
	.sfr = NULL,
	.sfr2 = NULL,
	.addr = NULL,
};

void dbg_dump_mbox(void);

/*****************************************************************************
 *****                         wrapper function                          *****
 *****************************************************************************/
static int __send_interrupt(u32 cmdType, struct command *cmd, u32 type)
{
	int ret = 0;
	u32 val;
	u32 timeout;
#ifndef CONFIG_NPU_USE_MAILBOX_GROUP
	u32 bit;
#endif

	timeout = MAILBOX_CLEAR_CHECK_TIMEOUT;
	switch (cmdType) {
	case COMMAND_LOAD:
	case COMMAND_UNLOAD:
	case COMMAND_PROFILE_CTL:
	case COMMAND_PURGE:
#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	case COMMAND_POWER_CTL:
#else
	case COMMAND_POWERDOWN:
#endif
	case COMMAND_MODE:
	case COMMAND_FW_TEST:
	case COMMAND_CORE_CTL:
#if (CONFIG_NPU_COMMAND_VERSION >= 10)
		/* fallthrough */
	case COMMAND_IMB_SIZE:
#endif
		/* Wait clr bit from FW */
#ifdef CONFIG_NPU_USE_MAILBOX_GROUP
		while (timeout && interface.sfr->grp[0].ms) {
#else
		while (timeout && interface.sfr->grp[1].ms & 0x1) {
#endif
			timeout--;
			if (!(timeout % 100))
				npu_warn("Mailbox %d delayed for %d\n", cmdType,
					MAILBOX_CLEAR_CHECK_TIMEOUT - timeout);
		}

		if (!timeout) {
			npu_err("Mailbox %d delayed for %d times, cancelled\n",
					cmdType, MAILBOX_CLEAR_CHECK_TIMEOUT);
			dbg_dump_mbox();
			ret = -EWOULDBLOCK;
			return ret;
		}

#ifdef CONFIG_NPU_USE_MAILBOX_GROUP
		interface.sfr->grp[0].g = 0x10000;
		val = interface.sfr->grp[0].g;
#else
		interface.sfr->grp[1].s = 0x1;
		val = interface.sfr->grp[1].s;
#endif
		//npu_notice("g0.g = %x, nw->uid = %d, nw->bound_id = %d\n", val, cmd->c.load.oid, cmd->c.load.tid);
		break;
	case COMMAND_PROCESS:
#ifdef CONFIG_NPU_USE_MAILBOX_GROUP
		while (timeout && interface.sfr->grp[2].ms) {
#else
		if (type == NPU_MBOX_REQUEST_MEDIUM || type == NPU_MBOX_REQUEST_LOW)
			bit = 0x2;
		else
			bit = 0x4;

		/* Wait clr bit from FW */
		while (timeout && interface.sfr->grp[1].ms & bit) {
#endif
			timeout--;
			if (!(timeout % 100))
				npu_warn("Mailbox %d delayed for %d\n", cmdType,
					MAILBOX_CLEAR_CHECK_TIMEOUT - timeout);
		}

		if (!timeout) {
			npu_err("Mailbox %d delayed for %d times, cancelled\n",
					cmdType, MAILBOX_CLEAR_CHECK_TIMEOUT);
			dbg_dump_mbox();
			ret = -EWOULDBLOCK;
			return ret;
		}

#ifdef CONFIG_NPU_USE_MAILBOX_GROUP
		interface.sfr->grp[2].g = 0xFFFFFFFF;
		val = interface.sfr->grp[2].g;
#else
		interface.sfr->grp[1].s = bit;
		val = interface.sfr->grp[1].s;
#endif
		//npu_notice("g2.g = %x, frame->uid = %d, frame->frame_id = %d\n", val,
		//						cmd->c.process.oid, cmd->c.process.fid);
		break;
	case COMMAND_DONE:
		/* Wait clr bit from FW */
#ifdef CONFIG_NPU_USE_MAILBOX_GROUP
		while (timeout && interface.sfr->grp[3].ms) {
#else
		while (timeout && interface.sfr->grp[1].ms & 0x4) {
#endif
			timeout--;
			if (!(timeout % 100))
				npu_warn("Mailbox %d delayed for %d\n", cmdType,
					MAILBOX_CLEAR_CHECK_TIMEOUT - timeout);
		}

		if (!timeout) {
			npu_err("Mailbox %d delayed for %d times, cancelled\n",
					cmdType, MAILBOX_CLEAR_CHECK_TIMEOUT);
			dbg_dump_mbox();
			ret = -EWOULDBLOCK;
			return ret;
		}

#ifdef CONFIG_NPU_USE_MAILBOX_GROUP
		interface.sfr->grp[3].g = 0xFFFFFFFF;
		val = interface.sfr->grp[3].g;
#else
		interface.sfr->grp[1].s = 0x4;
		val = interface.sfr->grp[1].s;
#endif
		//npu_info("g3.g = %x\n", val);
		break;
	default:
		break;
	}

	return ret;
}

static irqreturn_t mailbox_isr0(int irq, void *data)
{
	/* high reponse */
	u32 val;
#ifdef CONFIG_NPU_USE_MAILBOX_GROUP
	val = interface.sfr2->grp[3].ms;
#else
	val = interface.sfr2->grp[0].ms & 1;
#endif
	if (val)
#ifdef CONFIG_NPU_USE_MAILBOX_GROUP
		interface.sfr2->grp[3].c = val;
#else
		interface.sfr2->grp[0].c = val;
#endif
	if (is_kpi_mode_enabled(false))
		npu_kpi_response_read();
	else {
		atomic_inc(&protodr->high_prio_frame_count);
		if (interface.rslt_notifier != NULL)
			interface.rslt_notifier(NULL);
	}
	return IRQ_HANDLED;
}
static irqreturn_t mailbox_isr1(int irq, void *data)
{
#ifdef CONFIG_NPU_USE_MAILBOX_GROUP
	u32 val;
	val = interface.sfr2->grp[4].ms;
	if (val)
		interface.sfr2->grp[4].c = val;
#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	if (interface.rslt_notifier != NULL)
		interface.rslt_notifier(NULL);
#else
	if (wq)
		queue_work(wq, &work_report);
#endif
#else
#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	/* normal reponse */
	u32 val;
	val = interface.sfr2->grp[0].ms & 2;
	if (val)
		interface.sfr2->grp[0].c = val;
	atomic_inc(&protodr->normal_prio_frame_count);
	if (interface.rslt_notifier != NULL)
		interface.rslt_notifier(NULL);
#endif
#endif
	return IRQ_HANDLED;
}
static irqreturn_t mailbox_isr2(int irq, void *data)
{
	/* report */
	u32 val;
#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	val = interface.sfr2->grp[0].ms & 4;
#else
	val = interface.sfr2->grp[0].ms & 2;
#endif
	if (val)
		interface.sfr2->grp[0].c = val;
	if (wq)
		queue_work(wq, &work_report);
	return IRQ_HANDLED;
}

#ifdef CONFIG_DSP_USE_VS4L
static void __dsp_interface_isr_reset_request(void)
{
	npu_info("dsp reset request done\n");
	return;
}

static void __dsp_interface_isr(void)
{
	unsigned int status;

	status = dsp_dhcp_read_reg_idx(interface.dhcp, DSP_DHCP_TO_HOST_INT_STATUS);
	if (status & BIT(DSP_TO_HOST_INT_RESET_REQUEST)) {
		__dsp_interface_isr_reset_request();
	} else {
		npu_err("interrupt status is invalid(%u)", status);
		//dsp_dump_ctrl();
	}
	dsp_dhcp_write_reg_idx(interface.dhcp, DSP_DHCP_TO_HOST_INT_STATUS, 0x0);
}

int dsp_interface_send_irq(int status)
{
	int ret;

	if (status == DSP_WAIT_RESET) {
		npu_write_hw_reg(npu_get_io_area(interface.system, "sfrdnc"),
				0x2081C, 0x1, 0xFFFFFFFF, 0);
	} else {
		npu_err("wrong interrupt requested(%x)\n", status);
		ret = -EINVAL;
		goto p_err;
	}
	return 0;
p_err:
	return ret;
}

static irqreturn_t dsp_isr0(int irq, void *data)
{
	u32 status;

#if 1 // Pamir
	status = npu_read_hw_reg(npu_get_io_area(interface.system, "sfrdnc"), 0x21018, 0xFFFFFFFF, 0);
#else // Orange
	status = npu_read_hw_reg(npu_get_io_area(interface.system, "sfrnpus"), 0x0, 0xFFFFFFFF);
#endif
	if (!status) {
		npu_warn("interrupt occurred incorrectly\n");
		goto p_end;
	}

	__dsp_interface_isr();

#if 1 // Pamir
	npu_write_hw_reg(npu_get_io_area(interface.system, "sfrdnc"), 0x21018, 0x0, 0xFFFFFFFF, 0);
#else // Orange
	npu_write_hw_reg(npu_get_io_area(interface.system, "sfrnpus"), 0x0, 0x0, 0xFFFFFFFF);
#endif
p_end:
	return IRQ_HANDLED;
}
#else
static irqreturn_t temp_isr(int irq, void *data)
{
	(void)irq;
	(void)data;
	return IRQ_HANDLED;
}
#endif

irqreturn_t (*mailbox_isr_list[])(int, void *) = {
	mailbox_isr0,
	mailbox_isr1,
	mailbox_isr2,
#ifdef CONFIG_DSP_USE_VS4L
	dsp_isr0,
#else
	temp_isr,
#endif
};

void dbg_dump_mbox(void)
{
	int i, rptr;
	u32 sgmt_len;
	char *base;

	volatile struct mailbox_ctrl *ctrl;

	for (i = 0; i < MAX_MAILBOX / 2; i++) {
		ctrl = &(interface.mbox_hdr->f2hctrl[i]);
		sgmt_len = ctrl->sgmt_len;
		base = NPU_MAILBOX_GET_CTRL(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl->sgmt_ofs);
		rptr = 0;
		npu_err("H2F_MBOX - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
	}

	for (i = 0; i < MAX_MAILBOX / 2; i++) {
		ctrl = &(interface.mbox_hdr->h2fctrl[i]);
		sgmt_len = ctrl->sgmt_len;
		base = NPU_MAILBOX_GET_CTRL(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl->sgmt_ofs);
		rptr = 0;
		npu_err("F2H_MBOX - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
	}
}

void dbg_print_error(void)
{
	u32 nSize = 0;
	u32 wptr, rptr, sgmt_len;
	char buf[BUFSIZE - 1] = "";
	char *base;

	volatile struct mailbox_ctrl *ctrl;

	if (interface.mbox_hdr == NULL)
		return;
	mutex_lock(&interface.lock);

	ctrl = &(interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_REPORT]);
	wptr = ctrl->wptr;
	rptr = ctrl->rptr;
	sgmt_len = ctrl->sgmt_len;
	base = NPU_MAILBOX_GET_CTRL(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl->sgmt_ofs);

	if (wptr < rptr) { //When wptr circled a time,
		while (rptr != 0) {
			if ((sgmt_len - rptr) >= BUFSIZE) {
				nSize = BUFSIZE - 1;
				memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
				rptr += nSize;
			} else {
				nSize = sgmt_len - rptr;
				memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
				rptr = 0;
			}
			pr_err("%s\n", buf);
			buf[0] = '\0';
		}
	}
	while (wptr != rptr) {
		if ((wptr - rptr) > BUFSIZE - 1) {//need more memcpy from io.
			nSize = BUFSIZE - 1;
			memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
			rptr += nSize;
		} else {
			nSize = wptr - rptr;
			memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
			rptr = wptr;
		}
		pr_err("%s\n", buf);
		buf[0] = '\0';
	}
	mutex_unlock(&interface.lock);
}

void dbg_print_ncp_header(struct ncp_header *nhdr)
{
	npu_info("============= ncp_header =============\n");
	npu_info("mabic_number1: 0X%X\n", nhdr->magic_number1);
	npu_info("hdr_version: 0X%X\n", nhdr->hdr_version);
	npu_info("hdr_size: 0X%X\n", nhdr->hdr_size);
#if (NCP_VERSION == 25)
	npu_info("model name: %s\n", nhdr->model_name);
	npu_info("periodicity: 0X%X\n", nhdr->periodicity);
#else
	npu_info("net_id: 0X%X\n", nhdr->net_id);
	npu_info("unique_id: 0X%X\n", nhdr->unique_id);
	npu_info("period: 0X%X\n", nhdr->period);
#endif
	npu_info("priority: 0X%X\n", nhdr->priority);
	npu_info("flags: 0X%X\n", nhdr->flags);
	npu_info("workload: 0X%X\n", nhdr->workload);

	npu_info("addr_vector_offset: 0X%X\n", nhdr->address_vector_offset);
	npu_info("addr_vector_cnt: 0X%X\n", nhdr->address_vector_cnt);
	npu_info("magic_number2: 0X%X\n", nhdr->magic_number2);
}

void dbg_print_interface(void)
{
	npu_info("=============  interface mbox hdr =============\n");
	npu_info("mbox_hdr: 0x%pK\n", interface.mbox_hdr);
	npu_info("mbox_hdr.max_slot: %d\n", interface.mbox_hdr->max_slot);
	npu_info("mbox_hdr.version: %d\n", interface.mbox_hdr->version);
	npu_info("mbox_hdr.signature2: 0x%x\n", interface.mbox_hdr->signature2);
	npu_info("mbox_hdr.signature1: 0x%x\n", interface.mbox_hdr->signature1);
	npu_info("sfr: 0x%pK\n", interface.sfr);
	npu_info("addr: 0x%pK\n", NPU_MBOX_BASE(interface.mbox_hdr));
}

static int npu_set_cmd(struct message *msg, struct command *cmd, u32 cmdType)
{
	int ret = 0;
	volatile struct mailbox_ctrl *ctrl;
	ctrl = &interface.mbox_hdr->h2fctrl[cmdType];

	ret = mbx_ipc_put(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, msg, cmd);
	if (ret)
		goto I_ERR;
	npu_log_ipc_set_date(cmdType, ctrl->rptr, ctrl->wptr);
	ret = __send_interrupt(msg->command, cmd, cmdType);
	if (ret)
		goto I_ERR;
	return 0;
I_ERR:
	switch (ret) {
	case -ERESOURCE:
		npu_warn("No space left on mailbox : ret = %d\n", ret);
		break;
	default:
		npu_err("mbx_ipc_put err with %d\n", ret);
		break;
	}
	return ret;
}

int npu_interface_probe(struct device *dev, void *regs1, void *regs2)
{
	int ret = 0;

	BUG_ON(!dev);
	if (!regs1 || !regs2) {
		probe_err("fail in %s\n", __func__);
		ret = -EINVAL;
		return ret;
	}

	interface.sfr = (volatile struct mailbox_sfr *)regs1;
	interface.sfr2 = (volatile struct mailbox_sfr *)regs2;
	mutex_init(&interface.lock);
	wq = alloc_workqueue("my work", WQ_FREEZABLE|WQ_HIGHPRI, 1);
	INIT_WORK(&work_report, __rprt_manager);

	wq_p = alloc_workqueue("my work profile", WQ_FREEZABLE | WQ_HIGHPRI, 1);
	INIT_WORK(&work_profile, __profile_manager);

	memset(&fwProfile, 0, sizeof(struct fw_profile_info));

	probe_info("complete in %s\n", __func__);
	return ret;
}

int npu_interface_open(struct npu_system *system)
{
	int i, ret = 0;
	unsigned int isr_cpu_affinity = 0;
	struct npu_device *device;
	struct device *dev = &system->pdev->dev;

	BUG_ON(!system);
	device = container_of(system, struct npu_device, system);
	interface.mbox_hdr = system->mbox_hdr;
#ifdef CONFIG_DSP_USE_VS4L
	interface.system = &device->system;
	interface.dhcp = device->system.dhcp;
#endif
	for (i = 0; i < system->irq_num; i++) {
		ret = devm_request_irq(dev, system->irq[i], mailbox_isr_list[i],
				IRQF_TRIGGER_HIGH, "exynos-npu", NULL);
		if (ret) {
			npu_err("fail(%d) in devm_request_irq(%d)\n", ret, i);
			goto err_probe_irq;
		}
	}

	ret = of_property_read_u32(dev->of_node,
		"samsung,npuinter-isr-cpu-affinity", &isr_cpu_affinity);
	if (ret)
		isr_cpu_affinity = 0;

	npu_info("set the CPU affinity of ISR to %u\n", isr_cpu_affinity);
	for (i = 0; i < system->irq_num; i++) {
		ret = irq_set_affinity_hint(system->irq[i], cpumask_of(isr_cpu_affinity));
		if (ret) {
			npu_err("fail(%d) in irq_set_affinity_hint(%d)\n", ret, i);
			goto err_probe_irq;
		}
	}

	wq = alloc_workqueue("rprt_manager", __WQ_LEGACY | __WQ_ORDERED, 0);
	if (!wq) {
		npu_err("err in alloc_worqueue.\n");
		goto err_exit;
	}
	ret = mailbox_init(interface.mbox_hdr, system);
	if (ret) {
		npu_err("error(%d) in npu_mailbox_init\n", ret);
		dbg_print_interface();
		goto err_workqueue;
	}
	return ret;

err_workqueue:
	destroy_workqueue(wq);
err_probe_irq:
	for (i = 0; i < system->irq_num; i++) {
		irq_set_affinity_hint(system->irq[i], NULL);
		devm_free_irq(dev, system->irq[i], NULL);
	}
err_exit:
	interface.addr = NULL;
	interface.mbox_hdr = NULL;
	set_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
	npu_err("EMERGENCY_RECOVERY is triggered.\n");
	return ret;
}

int npu_interface_close(struct npu_system *system)
{
	int i, wptr, rptr;
	struct device *dev;

	if (!system) {
		npu_err("fail in %s\n", __func__);
		return -EINVAL;
	}

	if (likely(interface.mbox_hdr))
		mailbox_deinit(interface.mbox_hdr, system);

	dev = &system->pdev->dev;

	for (i = 0; i < system->irq_num; i++)
		irq_set_affinity_hint(system->irq[i], NULL);

	for (i = 0; i < system->irq_num; i++)
		devm_free_irq(dev, system->irq[i], NULL);

	queue_work(wq, &work_report);
	if ((wq) && (interface.mbox_hdr)) {
		if (work_pending(&work_report)) {
			cancel_work_sync(&work_report);
			wptr = interface.mbox_hdr->f2hctrl[1].wptr;
			rptr = interface.mbox_hdr->f2hctrl[1].rptr;
			npu_dbg("work was canceled due to interface close, rptr/wptr : %d/%d\n", wptr, rptr);
		}
		flush_workqueue(wq);
		destroy_workqueue(wq);
		wq = NULL;
	}

	interface.addr = NULL;
	interface.mbox_hdr = NULL;
	return 0;
}

int register_rslt_notifier(protodrv_notifier func)
{
	interface.rslt_notifier = func;
	return 0;
}

int register_msgid_get_type(int (*msgid_get_type_func)(int))
{
	interface.msgid_get_type = msgid_get_type_func;
	return 0;
}

static u32 config_npu_load_payload(struct npu_nw *nw)
{
	struct npu_session *session = nw->session;
	struct cmd_load_payload *payload = session->ncp_payload->vaddr;

	payload[COMMAND_LOAD_USER_NCP].addr = nw->ncp_addr.daddr;
	payload[COMMAND_LOAD_USER_NCP].size = nw->ncp_addr.size;
	payload[COMMAND_LOAD_USER_NCP].id = COMMAND_LOAD_USER_NCP;

	payload[COMMAND_LOAD_HDR_COPY].addr = session->ncp_hdr_buf->daddr;
	payload[COMMAND_LOAD_HDR_COPY].size = session->ncp_hdr_buf->size;
	payload[COMMAND_LOAD_HDR_COPY].id = COMMAND_LOAD_HDR_COPY;

	npu_session_ion_sync_for_device(session->ncp_payload, 0, session->ncp_payload->size,
					DMA_TO_DEVICE);

	return session->ncp_payload->daddr;
}

int nw_req_manager(int msgid, struct npu_nw *nw)
{
	int ret = 0;
	struct command cmd = {};
	struct message msg = {};

	switch (nw->cmd) {
	case NPU_NW_CMD_BASE:
		npu_info("abnormal command type\n");
		break;
	case NPU_NW_CMD_LOAD:
#ifdef CONFIG_DSP_USE_VS4L
		/* TODO: refactoring msgid check, NPU (0 ~ 31), DSP(32 ~ 63)*/
		if (msgid < 32) {
			/* NPU */
#endif
			cmd.c.load.oid = nw->uid;
			cmd.c.load.tid = nw->bound_id;
			cmd.length = sizeof(struct cmd_load_payload) * 2;
			cmd.payload = config_npu_load_payload(nw);

#ifdef CONFIG_DSP_USE_VS4L
		} else {
			/* DSP */
			struct dsp_common_graph_info_v4 *load_msg =
				(struct dsp_common_graph_info_v4 *)(nw->ncp_addr.vaddr);
			int num_param = load_msg->n_tsgd + load_msg->n_param;

			cmd.c.load.oid = nw->session->unified_id << 16 | (load_msg->global_id & 0xFFFF);
			cmd.c.load.tid = load_msg->n_tsgd;
			cmd.c.load.n_param = load_msg->n_param;
			cmd.c.load.n_kernel = load_msg->n_kernel;

			/* only need param list */
			cmd.length = (u32)sizeof(struct dsp_common_param_v4) * num_param;
			cmd.payload = nw->ncp_addr.daddr + sizeof(struct dsp_common_graph_info_v4);
		}
#endif
		npu_info("Loading data 0x%08x (%d)\n", cmd.payload, cmd.length);
		msg.command = COMMAND_LOAD;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_UNLOAD:
#ifdef CONFIG_DSP_USE_VS4L
		/* TODO: refactoring msgid check, NPU (0 ~ 31), DSP(32 ~ 63)*/
		if (msgid < 32) {
			/* NPU */
#endif
			cmd.c.unload.oid = nw->uid;//NPU_MAILBOX_DEFAULT_OID;
#ifdef CONFIG_DSP_USE_VS4L
		} else {
			/* DSP */
			cmd.c.unload.oid = nw->session->unified_id << 16 | (nw->session->global_id & 0xFFFF);
		}
#endif
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_UNLOAD;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_MODE:
		cmd.c.mode.mode = nw->param0;
		cmd.c.mode.llc_size = nw->param1;
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_MODE;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_PROFILE_START:
		cmd.c.profile_ctl.ctl = PROFILE_CTL_CODE_START;
		cmd.payload = nw->ncp_addr.daddr;
		cmd.length = nw->ncp_addr.size;
		msg.command = COMMAND_PROFILE_CTL;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_PROFILE_STOP:
		cmd.c.profile_ctl.ctl = PROFILE_CTL_CODE_STOP;
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_PROFILE_CTL;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_STREAMOFF:
		cmd.c.purge.oid = nw->uid;
		cmd.payload = 0;
		msg.command = COMMAND_PURGE;
		msg.length = sizeof(struct command);
		break;
#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	case NPU_NW_CMD_POWER_CTL:
		cmd.c.power_ctl.magic = 0xdeadbeef;
		cmd.payload = 0;
		msg.command = COMMAND_POWER_CTL;
		msg.length = sizeof(struct command);
		break;
#else
	case NPU_NW_CMD_POWER_DOWN:
		cmd.c.powerdown.dummy = nw->uid;
		cmd.payload = 0;
		msg.command = COMMAND_POWERDOWN;
		msg.length = sizeof(struct command);
		break;
#endif
	case NPU_NW_CMD_FW_TC_EXECUTE:
		cmd.c.fw_test.param = nw->param0;
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_FW_TEST;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_CORE_CTL:
#if (CONFIG_NPU_COMMAND_VERSION >= 9)
		cmd.c.core_ctl.magic = nw->param0;
#else
		cmd.c.core_ctl.active_cores = nw->param0;
#endif
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_CORE_CTL;
		msg.length = sizeof(struct command);
		break;
#if (CONFIG_NPU_COMMAND_VERSION >= 10)
	case NPU_NW_CMD_IMB_SIZE:
		cmd.c.imb.imb_size = nw->param0;
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_IMB_SIZE;
		msg.length = sizeof(struct command);
		break;
#endif
	case NPU_NW_CMD_END:
		break;
	default:
		npu_err("invalid CMD ID: (%db)", nw->cmd);
		ret = FALSE;
		goto nw_req_err;
	}
	msg.mid = msgid;

	ret = npu_set_cmd(&msg, &cmd, NPU_MBOX_REQUEST_LOW);
	if (ret)
		goto nw_req_err;

	mbx_ipc_print_dbg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->h2fctrl[0]);
	return TRUE;
nw_req_err:
	return ret;
}

#ifdef CONFIG_DSP_USE_VS4L
#include "npu-hw-device.h"
static struct vb_buffer *get_buffer_of_execution_info(struct npu_frame *frame)
{
	struct vb_container_list *input;
	struct vb_container *containers;
	struct vb_buffer *buffers;

	input = frame->input;
	if (!input) {
		npu_err("failed to get input of execution info for dsp\n");
		return NULL;
	}

	containers = &input->containers[input->count - 1];
	if (!containers) {
		npu_err("failed to get containers of execution info for dsp\n");
		return NULL;
	}

	buffers = containers->buffers;
	if (!buffers) {
		npu_err("failed to get buffer of execution info for dsp\n");
		return NULL;
	}

	return buffers;
}

struct dsp_common_execute_info_v4 *get_execution_info_for_dsp(struct npu_frame *frame)
{
	struct vb_buffer *buffers = get_buffer_of_execution_info(frame);
	if (!buffers) {
		npu_err("failed to get buffer of execution info for dsp\n");
		return NULL;
	}

	return buffers->vaddr;
}

static int get_size_of_execution_info(struct npu_frame *frame)
{
	struct vb_buffer *buffers;
	struct dsp_common_execute_info_v4 *exe_info;
	int num_param;
	int size_of_exe_info;

	buffers = get_buffer_of_execution_info(frame);
	if (!buffers) {
		npu_err("failed to get buffer of execution info for dsp\n");
		return -1;
	}

	exe_info = buffers->vaddr;
	if (!exe_info) {
		npu_err("failed to get vaddr of execution info for dsp\n");
		return -2;
	}

	num_param = exe_info->n_update_param;
	size_of_exe_info = sizeof(struct dsp_common_param_v4) * num_param;

	return size_of_exe_info;
}

static dma_addr_t get_iova_of_execution_info(struct npu_frame *frame)
{
	struct vb_buffer *buffers = get_buffer_of_execution_info(frame);
	if (!buffers) {
		npu_err("failed to get buffer of execution info for dsp\n");
		return 0;
	}

	return buffers->daddr + sizeof(struct dsp_common_execute_info_v4);
}

int __update_iova_of_exe_message_of_dsp(struct npu_session *session, void *msg_kaddr);
#endif

int fr_req_manager(int msgid, struct npu_frame *frame)
{
	int offset = 0;
	int ret = FALSE;
	struct command cmd = {};
	struct message msg = {};
#ifdef CONFIG_DSP_USE_VS4L
	struct dsp_common_execute_info_v4 *exe_info;
	struct npu_session *session = frame->session;
	struct vb_buffer *buffers = get_buffer_of_execution_info(frame);
	WARN_ON(!buffers);
#endif
	switch (frame->cmd) {
	case NPU_FRAME_CMD_BASE:
		break;
	case NPU_FRAME_CMD_Q:
		cmd.c.process.fid = frame->frame_id;
		cmd.c.process.priority = frame->priority;
#if (CONFIG_NPU_COMMAND_VERSION >= 9)
		cmd.c.process.flag = 0;
#else
		cmd.c.process.profiler_ctrl = 0;
#endif

#ifdef CONFIG_DSP_USE_VS4L
		if (session->hids & NPU_HWDEV_ID_DSP) {
			exe_info = get_execution_info_for_dsp(frame);
			cmd.c.process.oid = frame->session->unified_id << 16 | (exe_info->global_id & 0xFFFF);
			cmd.length = get_size_of_execution_info(frame);
			cmd.payload = get_iova_of_execution_info(frame);
			if (buffers)
				dma_buf_end_cpu_access(buffers->dma_buf, DMA_TO_DEVICE);
		} else {
			cmd.c.process.oid = frame->uid;
			cmd.length = frame->mbox_process_dat.address_vector_cnt;
			cmd.payload = frame->mbox_process_dat.address_vector_start_daddr;
		}
#else
		cmd.c.process.oid = frame->uid;
		cmd.length = frame->mbox_process_dat.address_vector_cnt;
		cmd.payload = frame->mbox_process_dat.address_vector_start_daddr;
#endif
		msg.command = COMMAND_PROCESS;
		msg.length = sizeof(struct command);
		npu_dbg("oid %d, len %d, payload 0x%x, cmd %d\n",
				cmd.c.process.oid, cmd.length, cmd.payload, msg.command);
		break;
	case NPU_FRAME_CMD_PROFILER:
		cmd.c.process.fid = frame->frame_id;
		cmd.c.process.priority = frame->priority;
#if (CONFIG_NPU_COMMAND_VERSION >= 9)
		cmd.c.process.flag = 1;
#else
		cmd.c.process.profiler_ctrl = (u32)frame->output->timestamp[5].tv_usec;
#endif

#ifdef CONFIG_DSP_USE_VS4L
		if (session->hids & NPU_HWDEV_ID_DSP) {
			exe_info = get_execution_info_for_dsp(frame);
			cmd.c.process.oid = frame->session->unified_id << 16 | (exe_info->global_id & 0xFFFF);
			cmd.length = get_size_of_execution_info(frame);
			cmd.payload = get_iova_of_execution_info(frame);
			if (buffers)
				dma_buf_end_cpu_access(buffers->dma_buf, DMA_TO_DEVICE);
		} else {
			cmd.c.process.oid = frame->uid;
			cmd.length = frame->mbox_process_dat.address_vector_cnt;
			cmd.payload = frame->mbox_process_dat.address_vector_start_daddr;
		}
#else
		cmd.c.process.oid = frame->uid;
		cmd.length = frame->mbox_process_dat.address_vector_cnt;
		cmd.payload = frame->mbox_process_dat.address_vector_start_daddr;
#endif
		msg.command = COMMAND_PROCESS;
		msg.length = sizeof(struct command);
		npu_dbg("oid %d, len %d, payload 0x%x, cmd %d\n",
				cmd.c.process.oid, cmd.length, cmd.payload, msg.command);
		break;
	case NPU_FRAME_CMD_END:
		break;
	}
	msg.mid = msgid;
#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	if (frame->priority < NPU_PRIORITY_PREMPT_MIN) {
		ret = npu_set_cmd(&msg, &cmd, NPU_MBOX_REQUEST_MEDIUM);
		offset = 1;
	} else {
		ret = npu_set_cmd(&msg, &cmd, NPU_MBOX_REQUEST_HIGH);
		offset = 2;
	}
#else
	ret = npu_set_cmd(&msg, &cmd, NPU_MBOX_REQUEST_HIGH);
#endif
	if (ret)
		goto fr_req_err;

	mbx_ipc_print_dbg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->h2fctrl[offset]);
	return TRUE;
fr_req_err:
	return ret;
}

void makeStructToString(struct cmd_done *done)
{
	u32 val;
	u8 tempbuf[128];
	u32 i;

	memset(&fwProfile, 0, sizeof(struct fw_profile_info));
	fwProfile.info_cnt = sizeof(struct cmd_done) / sizeof(u32);
	if (done == NULL) {
		npu_err("done is NULL\n");
		goto error;
	}
	for (i = 0; i < (int)fwProfile.info_cnt; i++) {
		memset(tempbuf, 0, sizeof(tempbuf));
		val = ((u32 *)done)[i];
		sprintf(tempbuf, "%d", val);

		memcpy(fwProfile.data + fwProfile.buf_size, strArr[i], strlen(strArr[i]));
		strcat(fwProfile.data, tempbuf);
		strcat(fwProfile.data, "\n");
		fwProfile.buf_size += strlen(strArr[i]) + strlen(tempbuf) + 1;
	}
error:
	return;
}

int nw_rslt_manager(int *ret_msgid, struct npu_nw *nw)
{
	int ret, channel;
	struct message msg;
	struct command cmd;

#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	channel = MAILBOX_F2HCTRL_NRESPONSE;
#else
	channel = MAILBOX_F2HCTRL_RESPONSE;
#endif
	ret = mbx_ipc_peek_msg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg);
	if (ret <= 0)
		return FALSE;

	ret = interface.msgid_get_type(msg.mid);
	if (ret != PROTO_DRV_REQ_TYPE_NW)
		return FALSE;//report error content:

	ret = mbx_ipc_get_msg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg);
	if (ret <= 0)
		return FALSE;

	ret = mbx_ipc_get_cmd(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg, &cmd);
	if (ret) {
		npu_err("get command error\n");
		return FALSE;
	}

	if (msg.command == COMMAND_DONE) {
		npu_info("COMMAND_DONE for mid: (%d), val(0x%x)\n", msg.mid,
			 cmd.c.done.request_specific_value);
		nw->result_value = cmd.c.done.request_specific_value;
		nw->result_code = NPU_ERR_NO_ERROR;
		makeStructToString(&(cmd.c.done));
	} else if (msg.command == COMMAND_NDONE) {
		npu_err("COMMAND_NDONE for mid: (%d) error(%u/0x%08x)\n"
			, msg.mid, cmd.c.ndone.error, cmd.c.ndone.error);
		nw->result_code = cmd.c.ndone.error;
	} else {
		npu_err("invalid msg.command: (%d)\n", msg.command);
		return FALSE;
	}

	fw_rprt_manager();
	// fw_profile_manager();
	*ret_msgid = msg.mid;

	return TRUE;
}

int fr_rslt_manager(int *ret_msgid, struct npu_frame *frame, enum channel_flag c)
{

	int ret = FALSE, channel;
	struct message msg;
	struct command cmd;

#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	if (c == HIGH_CHANNEL)
		channel = MAILBOX_F2HCTRL_RESPONSE;
	else
		channel = MAILBOX_F2HCTRL_NRESPONSE;
#else
	channel = MAILBOX_F2HCTRL_RESPONSE;
#endif
	ret = mbx_ipc_peek_msg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg);
	if (ret <= 0)
		return FALSE;

	ret = interface.msgid_get_type(msg.mid);
	if (ret != PROTO_DRV_REQ_TYPE_FRAME) {
		//npu_info("get type error: (%d)\n", ret);
		return FALSE;
	}

	ret = mbx_ipc_get_msg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg);
	if (ret <= 0)// 0 : no msg, less than zero : Err
		return FALSE;

	ret = mbx_ipc_get_cmd(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg, &cmd);
	if (ret)
		return FALSE;

	if (msg.command == COMMAND_DONE) {
		npu_dbg("COMMAND_DONE for mid: (%d)\n", msg.mid);
		if (cmd.c.done.request_specific_value > 0)
			frame->duration	= cmd.c.done.request_specific_value;

		frame->result_code = NPU_ERR_NO_ERROR;
		makeStructToString(&(cmd.c.done));
	} else if (msg.command == COMMAND_NDONE) {
		npu_err("COMMAND_NDONE for mid: (%d) error(%u/0x%08x)\n"
			, msg.mid, cmd.c.ndone.error, cmd.c.ndone.error);
		if (cmd.c.ndone.error == -ERR_TIMEOUT_RECOVERED)
			frame->result_code = NPU_ERR_NPU_HW_TIMEOUT_RECOVERED;
		else if (cmd.c.ndone.error == -ERR_TIMEOUT) {
			frame->result_code = NPU_ERR_NPU_HW_TIMEOUT_NOTRECOVERABLE;
			fw_will_note(FW_LOGSIZE);
			npu_err("watchdog reset triggered by NPU driver\n");
#ifdef CONFIG_NPU_USE_HW_DEVICE
			npu_util_dump_handle_nrespone(interface.system);
#endif
		} else
			frame->result_code = cmd.c.ndone.error;
	} else {
		npu_err("invalid msg.command: (%d)\n", msg.command);
		return FALSE;
	}
	*ret_msgid = msg.mid;
	fw_rprt_manager();
	// fw_profile_manager();
	return TRUE;
}

//Print log which was written with last 128 byte.
int npu_check_unposted_mbox(int nCtrl)
{
	int pos;
	int ret = TRUE;
	char *base;
	u32 nSize, wptr, rptr, sgmt_len;
	u32 *buf, *strOut;
	volatile struct mailbox_ctrl *ctrl;

	if (interface.mbox_hdr == NULL)
		return -1;

	strOut = kzalloc(LENGTHOFEVIDENCE, GFP_ATOMIC);
	if (!strOut) {
		ret = -ENOMEM;
		goto err_exit;
	}
	buf = kzalloc(LENGTHOFEVIDENCE, GFP_ATOMIC);
	if (!buf) {
		if (strOut)
			kfree(strOut);
		ret = -ENOMEM;
		goto err_exit;
	}

	if (!in_interrupt())
		mutex_lock(&interface.lock);

	switch (nCtrl) {
	case ECTRL_LOW:
		ctrl = &(interface.mbox_hdr->h2fctrl[MAILBOX_H2FCTRL_LPRIORITY]);
		npu_dump("[V] H2F_MBOX[LOW] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	case ECTRL_MEDIUM:
		ctrl = &(interface.mbox_hdr->h2fctrl[MAILBOX_H2FCTRL_MPRIORITY]);
		npu_dump("[V] H2F_MBOX[MEDIUM] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
#endif
	case ECTRL_HIGH:
		ctrl = &(interface.mbox_hdr->h2fctrl[MAILBOX_H2FCTRL_HPRIORITY]);
		npu_dump("[V] H2F_MBOX[HIGH] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
	case ECTRL_ACK:
		ctrl = &(interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_RESPONSE]);
		npu_dump("[V] F2H_MBOX[RESPONSE] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	case ECTRL_NACK:
		ctrl = &(interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_NRESPONSE]);
		npu_dump("[V] F2H_MBOX[NRESPONSE] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
#endif
	case ECTRL_REPORT:
		ctrl = &(interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_REPORT]);
		npu_dump("[V] F2H_MBOX[REPORT] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		break;
	default:
		BUG_ON(1);
	}

	wptr = ctrl->wptr;
	sgmt_len = ctrl->sgmt_len;
	base = NPU_MAILBOX_GET_CTRL(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl->sgmt_ofs);

	pos = 0;
	if (wptr < LENGTHOFEVIDENCE) {
		rptr = sgmt_len - (LENGTHOFEVIDENCE - wptr);
		nSize = LENGTHOFEVIDENCE - wptr;
		npu_debug_memdump32_by_memcpy((u32 *)(base + LINE_TO_SGMT(sgmt_len, rptr)), (u32 *)(base + LINE_TO_SGMT(sgmt_len, rptr) + nSize));
		rptr = 0;
		pos = 0;
		nSize = wptr;
	} else {
		rptr = wptr - LENGTHOFEVIDENCE;
		nSize = LENGTHOFEVIDENCE;
	}
	if (wptr > 0)
		npu_debug_memdump32_by_memcpy((u32 *)(base + LINE_TO_SGMT(sgmt_len, rptr)), (u32 *)(base + LINE_TO_SGMT(sgmt_len, rptr) + nSize));

	if (!in_interrupt())
		mutex_unlock(&interface.lock);

	if (buf)
		kfree(buf);
	if (strOut)
		kfree(strOut);
err_exit:
	return ret;
}

void fw_rprt_gather(void)
{
	char *base;
	u32 nSize;
	u32 wptr, rptr, sgmt_len;
	char buf[BUFSIZE - 1] = "\0";
	volatile struct mailbox_ctrl *ctrl;

	if (interface.mbox_hdr == NULL)
		return;
	mutex_lock(&interface.lock);

	ctrl = &(interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_REPORT]);
	wptr = ctrl->wptr;
	rptr = ctrl->rptr;
	sgmt_len = ctrl->sgmt_len;
	base = NPU_MAILBOX_GET_CTRL(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl->sgmt_ofs);

	if (wptr < rptr) { //When wptr circled a time,
		while (rptr != 0) {
			if ((sgmt_len - rptr) >= BUFSIZE) {
				nSize = BUFSIZE - 1;
				memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
				rptr += nSize;
			} else {
				nSize = sgmt_len - rptr;
				memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
				rptr = 0;
			}
			npu_fw_report_store(buf, nSize);
			buf[0] = '\0';
		}
	}

	while (wptr != rptr) {
		if ((wptr - rptr) > BUFSIZE - 1) {//need more memcpy from io.
			nSize = BUFSIZE - 1;
			memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
			rptr += nSize;
		} else {
			nSize = wptr - rptr;
			memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
			rptr = wptr;
		}
		npu_fw_report_store(buf, nSize);
		buf[0] = '\0';
	}
	interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_REPORT].rptr = wptr;
	mutex_unlock(&interface.lock);
}

static void __rprt_manager(struct work_struct *w)
{
	fw_rprt_gather();
}

static void __profile_manager(struct work_struct *w)
{
	npu_fw_profile_store(fwProfile.data, fwProfile.buf_size);
}

void fw_rprt_manager(void)
{
	if (wq)
		queue_work(wq, &work_report);
	else//not opened, or already closed.
		return;
}

void fw_profile_manager(void)
{
	if (wq_p)
		queue_work(wq_p, &work_profile);
	else
		return;
}

int mbx_rslt_fault_listener(void)
{
	int i;

	for (i = 0; i < MAX_MAILBOX / 2; i++) {
		dbg_print_ctrl(&interface.mbox_hdr->h2fctrl[i]);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr),
					&interface.mbox_hdr->h2fctrl[i], NPU_LOG_ERR);
	}

	for (i = 0; i < MAX_MAILBOX / 2; i++) {
		dbg_print_ctrl(&interface.mbox_hdr->f2hctrl[i]);
		if (i != MAILBOX_F2HCTRL_REPORT)
			mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr),
					&interface.mbox_hdr->f2hctrl[i], NPU_LOG_ERR);
	}

	return 0;
}

int nw_rslt_available(void)
{
	int ret, channel;
	struct message msg;

#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	channel = MAILBOX_F2HCTRL_NRESPONSE;
#else
	channel = MAILBOX_F2HCTRL_RESPONSE;
#endif
	ret = mbx_ipc_peek_msg(
		NPU_MBOX_BASE((void *)interface.mbox_hdr),
		&interface.mbox_hdr->f2hctrl[channel], &msg);
	return ret;
}
int fr_rslt_available(enum channel_flag c)
{
	int ret, channel;
	struct message msg;

#if (CONFIG_NPU_MAILBOX_VERSION >= 8)
	if (c == HIGH_CHANNEL)
		channel = MAILBOX_F2HCTRL_RESPONSE;
	else
		channel = MAILBOX_F2HCTRL_NRESPONSE;
#else
	channel = MAILBOX_F2HCTRL_RESPONSE;
#endif
	ret = mbx_ipc_peek_msg(
		NPU_MBOX_BASE((void *)interface.mbox_hdr),
		&interface.mbox_hdr->f2hctrl[channel], &msg);
	return ret;
}

const struct npu_log_ops npu_log_ops = {
	.fw_rprt_manager = fw_rprt_manager,
	.fw_rprt_gather = fw_rprt_gather,
	.npu_check_unposted_mbox = npu_check_unposted_mbox,
};

const struct npu_if_protodrv_mbox_ops protodrv_mbox_ops = {
	.frame_result_available = fr_rslt_available,
	.frame_post_request = fr_req_manager,
	.frame_get_result = fr_rslt_manager,
	.nw_result_available = nw_rslt_available,
	.nw_post_request = nw_req_manager,
	.nw_get_result = nw_rslt_manager,
	.register_notifier = register_rslt_notifier,
	.register_msgid_type_getter = register_msgid_get_type,
};

/* Unit test */
#ifdef CONFIG_VISION_UNITTEST
#define IDIOT_TESTCASE_IMPL "npu-interface.idiot"
#include "idiot-def.h"
#endif
