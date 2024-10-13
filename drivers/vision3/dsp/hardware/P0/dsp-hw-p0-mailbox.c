// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-p0-system.h"
#include "dsp-hw-p0-memory.h"
#include "dsp-hw-common-mailbox.h"
#include "dsp-hw-p0-mailbox.h"

static int __dsp_hw_p0_mailbox_set_version(struct dsp_mailbox *mbox)
{
	int ret;
	unsigned int mailbox_version, message_version;

	dsp_enter();
	mailbox_version = dsp_ctrl_dhcp_readl(
			DSP_P0_DHCP_IDX(DSP_P0_DHCP_MAILBOX_VERSION));
	message_version = dsp_ctrl_dhcp_readl(
			DSP_P0_DHCP_IDX(DSP_P0_DHCP_MESSAGE_VERSION));

	ret = mbox->ops->check_version(mbox, mailbox_version, message_version);
	if (ret)
		goto p_err;

	mbox->mailbox_version = mailbox_version;
	mbox->message_version = message_version;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_p0_mailbox_start(struct dsp_mailbox *mbox)
{
	int ret;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	ret = __dsp_hw_p0_mailbox_set_version(mbox);
	if (ret)
		goto p_err;

	pmem = &mbox->sys->memory.priv_mem[DSP_P0_PRIV_MEM_DHCP];
	mbox->to_fw = pmem->kvaddr + DSP_P0_DHCP_IDX(DSP_P0_DHCP_TO_CC_MBOX);
	mbox->to_host = pmem->kvaddr +
		DSP_P0_DHCP_IDX(DSP_P0_DHCP_TO_HOST_MBOX);

	pmem = &mbox->sys->memory.priv_mem[DSP_P0_PRIV_MEM_MBOX_MEMORY];
	dsp_util_queue_init(mbox->to_fw, sizeof(struct dsp_mailbox_to_fw),
			pmem->size, (unsigned int)pmem->iova,
			(unsigned long long)pmem->kvaddr);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static const struct dsp_mailbox_ops p0_mailbox_ops = {
	.alloc_pool	= dsp_hw_common_mailbox_alloc_pool,
	.free_pool	= dsp_hw_common_mailbox_free_pool,
	.dump_pool	= dsp_hw_common_mailbox_dump_pool,
	.send_task	= dsp_hw_common_mailbox_send_task,
	.receive_task	= dsp_hw_common_mailbox_receive_task,
	.send_message	= dsp_hw_common_mailbox_send_message,
	.check_version	= dsp_hw_common_mailbox_check_version,

	.start		= dsp_hw_p0_mailbox_start,
	.stop		= dsp_hw_common_mailbox_stop,

	.open		= dsp_hw_common_mailbox_open,
	.close		= dsp_hw_common_mailbox_close,
	.probe		= dsp_hw_common_mailbox_probe,
	.remove		= dsp_hw_common_mailbox_remove,
};

int dsp_hw_p0_mailbox_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_MAILBOX,
			&p0_mailbox_ops,
			sizeof(p0_mailbox_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
