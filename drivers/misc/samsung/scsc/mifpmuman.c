/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* uses */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <scsc/scsc_logring.h>
#include "scsc_mif_abs.h"
#include <linux/module.h>

#define PMU_MSG_TIMEOUT (HZ)

#ifdef CONFIG_SOC_S5E8825
/* uses */
#include "pmu_host_if.h"
#endif

static uint pmu_cmd_timeout = 1;
module_param(pmu_cmd_timeout, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pmu_cmd_timeout, "PMU command timeout in seconds, default 1");

/* Implements */
#include "mifpmuman.h"

#ifdef CONFIG_SOC_S5E8825
static void mifpmu_cmd_th_to_string(u32 cmd, char *buf, u8 buffer_size)
{
	memset(buf, 0, buffer_size);

	switch (cmd) {
	case PMU_AP_MSG_COMMAND_COMPLETE_IND:
		strncpy(buf, "CMD COMPLETE", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_ERROR_WPAN:
		strncpy(buf, "ERROR WPAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_ERROR_WLAN:
		strncpy(buf, "ERROR WLAN", buffer_size);
		break;
	default:
		strncpy(buf, "Incorrect command", buffer_size);
		break;
	}
}

static void mifpmu_cmd_fh_to_string(u32 cmd, char *buf, u8 buffer_size)
{
	memset(buf, 0, buffer_size);

	switch (cmd) {
	case PMU_AP_MSG_SUBSYS_START_WPAN:
		strncpy(buf, "START WPAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_START_WLAN:
		strncpy(buf, "START WLAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_START_WPAN_WLAN:
		strncpy(buf, "START WLAN and WPAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_RESET_WPAN:
		strncpy(buf, "RESET WPAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_RESET_WLAN:
		strncpy(buf, "RESET WLAN", buffer_size);
		break;
	case PMU_AP_MSG_SUBSYS_RESET_WPAN_WLAN:
		strncpy(buf, "RESET WLAN and WPAN", buffer_size);
		break;
	default:
		strncpy(buf, "Incorrect command", buffer_size);
		break;
	}
}
#endif /* CONFIG_SOC_S5E8825 */

static void mifpmu_isr(int irq, void *data)
{
	struct mifpmuman *pmu = (struct mifpmuman *)data;
	struct scsc_mif_abs *mif;
#ifdef CONFIG_SOC_S5E8825
	u32 cmd;
	char cmd_string[20];
#endif
	/* Get abs implementation */
	mif = pmu->mif;
#ifdef CONFIG_SOC_S5E8825
	pmu->last_msg = cmd = (u32)(mif->get_mbox_pmu(mif));

	mifpmu_cmd_th_to_string(cmd, cmd_string, 20);
	SCSC_TAG_INFO(MXMAN, "Received PMU IRQ cmd [%s] 0x%x\n", cmd_string, cmd);

	switch (cmd) {
	case PMU_AP_MSG_COMMAND_COMPLETE_IND:
		complete(&pmu->msg_ack);
		break;
	case PMU_AP_MSG_SUBSYS_ERROR_WPAN:
		if (pmu->pmu_irq_handler)
			pmu->pmu_irq_handler(pmu->irq_data, MIFPMU_ERROR_WPAN);
		else
			SCSC_TAG_ERR(MXMAN, "No handler for cmd 0%x\n", cmd);
		break;
	case PMU_AP_MSG_SUBSYS_ERROR_WLAN:
		if (pmu->pmu_irq_handler)
			pmu->pmu_irq_handler(pmu->irq_data, MIFPMU_ERROR_WLAN);
		else
			SCSC_TAG_ERR(MXMAN, "No handler for cmd 0%x\n", cmd);
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Command not recognized. cmd 0x%x\n", cmd);
		return;
	}
#else
	pmu->last_msg = (enum pmu_msg)(mif->get_mbox_pmu(mif));
	SCSC_TAG_INFO(MXMAN, "Received PMU IRQ\n");

	complete(&pmu->msg_ack);
#endif
}

int mifpmuman_init(struct mifpmuman *pmu, struct scsc_mif_abs *mif,
		   mifpmuisr_handler handler, void *irq_data)
{
	if (pmu->in_use)
		return -EBUSY;

	mutex_init(&pmu->lock);
	pmu->in_use = true;
	pmu->pmu_irq_handler = handler;
	pmu->irq_data = irq_data;

	/* register isr with mif abstraction */
	mif->irq_reg_pmu_handler(mif, mifpmu_isr, (void *)pmu);

	/* cache mif */
	pmu->mif = mif;

	init_completion(&pmu->msg_ack);
	return 0;
}

int mifpmuman_load_fw(struct mifpmuman *pmu, int *ka_patch, size_t ka_patch_len)
{
	struct scsc_mif_abs *mif;
	int ret;

	mutex_lock(&pmu->lock);
	if (!pmu->in_use) {
		SCSC_TAG_ERR(MXMAN, "PMU not initialized\n");
		mutex_unlock(&pmu->lock);
		return -ENODEV;
	}
	/* Get abs implementation */
	SCSC_TAG_INFO(MXMAN, "ka_patch : 0x%x\n", ka_patch);
	SCSC_TAG_INFO(MXMAN, "ka_patch_len : 0x%x\n", ka_patch_len);
	mif = pmu->mif;

	ret = mif->load_pmu_fw(mif, ka_patch, ka_patch_len);

	mutex_unlock(&pmu->lock);

	return ret;
}

#ifdef CONFIG_SOC_S5E8825
static int mifpmuman_send_msg_blocking(struct mifpmuman *pmu, u32 msg)
{
	struct scsc_mif_abs *mif;
	int ret;
	char cmd_string[20];

	mifpmu_cmd_fh_to_string(msg, cmd_string, 20);
	SCSC_TAG_INFO(MXMAN, "Send PMU cmd [%s] 0x%x\n", cmd_string, msg);

	if (!pmu->in_use) {
		SCSC_TAG_ERR(MXMAN, "PMU not initialized\n");
		return -ENODEV;
	}
	/* Get abs implementation */
	mif = pmu->mif;

	ret = mif->set_mbox_pmu(mif, msg);

	if (wait_for_completion_timeout(&pmu->msg_ack, pmu_cmd_timeout * PMU_MSG_TIMEOUT) == 0) {
		SCSC_TAG_ERR(MXMAN, "Timeout waiting for msg ACK\n");
		return -ETIMEDOUT;
	}

	/* reinit so completion can be re-used */
	reinit_completion(&pmu->msg_ack);

	return ret;
}
#else
int mifpmuman_send_msg(struct mifpmuman *pmu, enum pmu_msg msg)
{
	struct scsc_mif_abs *mif;
	int ret;

	SCSC_TAG_INFO(MXMAN, "Send PMU message %s\n", pmu_msg_string[msg]);

	mutex_lock(&pmu->lock);
	if (!pmu->in_use) {
		mutex_unlock(&pmu->lock);
		return -ENODEV;
	}
	/* Get abs implementation */
	mif = pmu->mif;

	ret = mif->set_mbox_pmu(mif, msg);

	if (wait_for_completion_timeout(&pmu->msg_ack, pmu_cmd_timeout * PMU_MSG_TIMEOUT) == 0) {
		SCSC_TAG_ERR(MXMAN, "Timeout waiting for msg ACK\n");
		mutex_unlock(&pmu->lock);
		return -ETIMEDOUT;
	}

	SCSC_TAG_INFO(MXMAN, "Received PMU message %s\n",
		      pmu_msg_string[pmu->last_msg]);
	/* reinit so completion can be re-used */
	reinit_completion(&pmu->msg_ack);

	mutex_unlock(&pmu->lock);

	return ret;
}
#endif

#ifdef CONFIG_SOC_S5E8825
int mifpmuman_start_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub)
{
	u32 msg;
	int r;

	mutex_lock(&pmu->lock);
	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
		SCSC_TAG_INFO(MXMAN, "Booting WLAN subsystem\n");
		msg = PMU_AP_MSG_SUBSYS_START_WLAN;
		break;
	case SCSC_SUBSYSTEM_WPAN:
		SCSC_TAG_INFO(MXMAN, "Booting WPAN subsystem\n");
		msg = PMU_AP_MSG_SUBSYS_START_WPAN;
		break;
	case SCSC_SUBSYSTEM_WLAN_WPAN:
		SCSC_TAG_INFO(MXMAN, "Booting WLAN and WPAN subsystems\n");
		msg = PMU_AP_MSG_SUBSYS_START_WPAN_WLAN;
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Subsystem %d not found\n", sub);
		mutex_unlock(&pmu->lock);
		return -EIO;
	}

	r = mifpmuman_send_msg_blocking(pmu, msg);

	mutex_unlock(&pmu->lock);

	return r;
}

int mifpmuman_stop_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub)
{
	u32 msg;
	int r;

	mutex_lock(&pmu->lock);
	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
		SCSC_TAG_INFO(MXMAN, "Resetting WLAN subsystem\n");
		msg = PMU_AP_MSG_SUBSYS_RESET_WLAN;
		break;
	case SCSC_SUBSYSTEM_WPAN:
		SCSC_TAG_INFO(MXMAN, "Stopping WPAN subsystem\n");
		msg = PMU_AP_MSG_SUBSYS_RESET_WPAN;
		break;
	case SCSC_SUBSYSTEM_WLAN_WPAN:
		SCSC_TAG_INFO(MXMAN, "Stopping WLAN and WPAN subsystems\n");
		msg = PMU_AP_MSG_SUBSYS_RESET_WPAN_WLAN;
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Subsystem %d not found\n", sub);
		mutex_unlock(&pmu->lock);
		return -EIO;
	}

	r = mifpmuman_send_msg_blocking(pmu, msg);

	mutex_unlock(&pmu->lock);

	return r;
}

int mifpmuman_force_monitor_mode_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub)
{
	u32 msg;
	int r;

	mutex_lock(&pmu->lock);
	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
		SCSC_TAG_INFO(MXMAN, "Setting WLAN subsystem in monitor mode\n");
		msg = PMU_AP_CFG_MONITOR_WLAN;
		break;
	case SCSC_SUBSYSTEM_WPAN:
		SCSC_TAG_INFO(MXMAN, "Setting WPAN subsystem in monitor mode\n");
		msg = PMU_AP_CFG_MONITOR_WPAN;
		break;
	case SCSC_SUBSYSTEM_WLAN_WPAN:
		SCSC_TAG_INFO(MXMAN, "Setting WLAN and WPAN subsystems in monitor mode\n");
		msg = PMU_AP_CFG_MONITOR_WPAN_WLAN;
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Subsystem %d not found\n", sub);
		mutex_unlock(&pmu->lock);
		return -EIO;
	}

	r = mifpmuman_send_msg_blocking(pmu, msg);

	mutex_unlock(&pmu->lock);

	return r;
}
#endif

int mifpmuman_deinit(struct mifpmuman *pmu)
{
	mutex_lock(&pmu->lock);
	if (!pmu->in_use) {
		mutex_unlock(&pmu->lock);
		return -ENODEV;
	}
	pmu->in_use = false;
	mutex_unlock(&pmu->lock);
	return 0;
}
