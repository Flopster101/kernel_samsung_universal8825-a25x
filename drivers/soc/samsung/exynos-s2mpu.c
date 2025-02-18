/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - Stage 2 Protection Unit(S2MPU)
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/bits.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/mutex.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>

#include <soc/samsung/exynos-hvc.h>
#include <soc/samsung/exynos-s2mpu.h>
#include <soc/samsung/exynos-smc.h>
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/debug-snapshot.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#include <soc/samsung/exynos_pm_qos.h>
#endif

static unsigned int s2mpu_num;
static unsigned int subsystem_num;
static unsigned long s2mpu_pd_bitmap;

static spinlock_t s2mpu_lock;

static const char *s2mpu_list[EXYNOS_MAX_S2MPU_INSTANCE];
static const char *subsystem_list[EXYNOS_MAX_SUBSYSTEM];
static struct s2mpu_notifier_block *nb_list[EXYNOS_MAX_SUBSYSTEM];

static bool s2mpu_success[EXYNOS_MAX_S2MPU_INSTANCE];

static struct s2mpu_pm_qos_sss pm_qos_sss;

/**
 * exynos_s2mpu_notifier_call_register - Register subsystem's S2MPU notifier call.
 *
 * @s2mpu_notifier_block:	S2MPU Notifier structure
 *				It should have two elements.
 *				One is Sub-system's name, 'subsystem'.
 *				The other is notifier call function pointer,
 *				s2mpu_notifier_fn_t 'notifier_call'.
 * @'subsystem':		Please refer to subsystem-names property of s2mpu
 *				node in exynosXXXX-security.dtsi.
 * @'notifier_call'		It's type is s2mpu_notifier_fn_t.
 *				And it should return S2MPU_NOTIFY
 */
unsigned long exynos_s2mpu_notifier_call_register(struct s2mpu_notifier_block *snb)
{
	unsigned int subsys_idx;

	if (snb == NULL) {
		pr_err("%s: subsystem notifier block is NULL pointer\n", __func__);
		return ERROR_INVALID_NOTIFIER_BLOCK;
	}

	if (snb->subsystem == NULL) {
		pr_err("%s: subsystem is NULL pointer\n", __func__);
		return ERROR_INVALID_SUBSYSTEM_NAME;
	}

	for (subsys_idx = 0; subsys_idx < subsystem_num; subsys_idx++) {
		if (!strncmp(subsystem_list[subsys_idx],
					snb->subsystem,
					EXYNOS_MAX_SUBSYSTEM_NAME_LEN))
			break;
	}

	if (subsys_idx >= subsystem_num) {
		pr_err("%s: %s is invalid subsystem name or not suported subsystem\n",
				__func__, snb->subsystem);
		return ERROR_DO_NOT_SUPPORT_SUBSYSTEM;
	}

	nb_list[subsys_idx] = snb;

	pr_info("%s: S2mpu %s FW Notifier call registeration Success!\n",
			__func__, snb->subsystem);

	return 0;
}
EXPORT_SYMBOL(exynos_s2mpu_notifier_call_register);

static unsigned long exynos_s2mpu_notifier_call(struct s2mpu_info_data *data)
{
	int ret;
	unsigned long subsys_idx, p_ret = 0;

	pr_info("    +S2MPU Fault Detector Notifier Call Information\n\n");
	for (subsys_idx = 0; subsys_idx < subsystem_num; subsys_idx++) {
		if (!(data->notifier_flag[subsys_idx]))
			continue;

		/* Check Notifier Call Fucntion pointer exist */
		if (nb_list[subsys_idx] == NULL) {
			pr_info("(%s) subsystem didn't register notifier block\n",
					subsystem_list[subsys_idx]);
			p_ret |= S2MPU_NOTIFIER_PANIC | (S2MPU_NOTIFY_BAD << subsys_idx);
			data->notifier_flag[subsys_idx] = 0;

			continue;
		}

		/* Check Notifier call pointer exist */
		if (nb_list[subsys_idx]->notifier_call == NULL) {
			pr_info("(%s) subsystem notifier block doesn't have notifier call\n",
					subsystem_list[subsys_idx]);
			p_ret |= S2MPU_NOTIFIER_PANIC | (S2MPU_NOTIFY_BAD << subsys_idx);
			data->notifier_flag[subsys_idx] = 0;

			continue;
		}

		pr_info("(%s) subsystem notifier call ++\n\n", subsystem_list[subsys_idx]);

		ret = nb_list[subsys_idx]->notifier_call(nb_list[subsys_idx],
							 &data->noti_info[subsys_idx]);
		if (ret == S2MPU_NOTIFY_BAD)
			p_ret |= S2MPU_NOTIFIER_PANIC | (S2MPU_NOTIFY_BAD << subsys_idx);
		data->notifier_flag[subsys_idx] = 0;

		pr_info("(%s) subsystem notifier ret[%x] --\n", subsystem_list[subsys_idx], ret);
	}

	pr_info("    -S2MPU Fault Detector Notifier Call Information\n");
	pr_info("====================================================\n");

	if (p_ret & S2MPU_NOTIFIER_PANIC) {
		pr_err("%s: Notifier Call Request Panic p_ret[%llx]\n",
					__func__, p_ret);
		return p_ret;
	}

	return S2MPU_NOTIFY_OK;
}

static irqreturn_t exynos_s2mpu_irq_handler(int irq, void *dev_id)
{
	struct s2mpu_info_data *data = dev_id;
	unsigned int irq_idx;
	unsigned long ret;

	for (irq_idx = 0; irq_idx < data->irqcnt; irq_idx++) {
		if (irq == data->irq[irq_idx])
			break;
	}

	pr_info("S2MPU interrupt called %d !! [%s]\n",
			irq, s2mpu_list[irq_idx]);

	/*
	 * Interrupt status register in S2MPU will be cleared
	 * in this HVC handler.
	 */
	ret = exynos_hvc(HVC_CMD_GET_S2MPUFD_FAULT_INFO,
			 irq_idx, 0, 0, 0);
	if (ret)
		pr_err("Can't handle %s S2MPU fault [%#llx]\n",
				s2mpu_list[irq_idx], ret);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t exynos_s2mpu_irq_handler_thread(int irq, void *dev_id)
{
	struct s2mpu_info_data *data = dev_id;
	struct __s2mpu_fault_info *info;
	struct s2mpu_fault_mptc *fstlb, *fptlb;
	struct s2mpu_mptc *stlb, *ptlb;
	unsigned int cnt, addr_low, fault_vid, fault_type, fault_rw, len, axid, ctx;
	unsigned long addr_high;
	unsigned int fault_subsys_idx, fault_blacklist_owner;
	unsigned int irq_idx, i, j;
	unsigned long flags;
	unsigned long ret;

	spin_lock_irqsave(&s2mpu_lock, flags);

	pr_auto(ASL4, "===============[S2MPU FAULT DETECTION]===============\n");

	/* find right irq index */
	for (irq_idx = 0; irq_idx < data->irqcnt; irq_idx++) {
		if (irq == data->irq[irq_idx])
			break;
	}

	pr_auto(ASL4, "\n");
	pr_auto(ASL4, "- Fault Instance : %s\n", s2mpu_list[irq_idx]);

	cnt = data->fault_info[irq_idx].fault_cnt;

	pr_auto(ASL4, "> Fault Count : %d\n\n", cnt);
	pr_auto(ASL4, "\n");

	if (cnt == 0) {
		ret = ERROR_NOTHING_S2MPU_FAULT;
		goto do_halt;
	}

	for (i = 0; i < cnt; i++) {
		info = &data->fault_info[irq_idx].info[i];

		fault_subsys_idx = info->subsystem;
		addr_low = info->fault_addr_low;
		addr_high = info->fault_addr_high;
		fault_vid = info->vid;
		fault_type = info->type;
		fault_rw = info->rw;
		len = info->len;
		axid = info->axid;
		ctx = info->context;
		fault_blacklist_owner = info->blacklist_owner;

		pr_auto(ASL4, "=====================================================\n");
		pr_auto(ASL4, "\n");

		if (!strncmp(subsystem_list[fault_subsys_idx],
			     "DEFAULT",
			     EXYNOS_MAX_SUBSYSTEM_NAME_LEN)) {
			pr_auto(ASL4, "  - Fault Subsystem/IP\t: %s\n",
					s2mpu_list[irq_idx]);
		} else {
			pr_auto(ASL4, "  - Fault Subsystem\t: %s\n",
					subsystem_list[fault_subsys_idx]);
		}

		pr_auto(ASL4, "- Fault Address : %#lx\n",
				addr_high ?
				(addr_high << 32) | addr_low :
				addr_low);

		if (fault_blacklist_owner != subsystem_num) {
			if (fault_blacklist_owner == OWNER_IS_KERNEL_RO) {
				pr_auto(ASL4, "  (This address is overlapped with blacklist and used by "
						"kernel code region)\n");
			} else if (fault_blacklist_owner == OWNER_IS_TEST) {
				pr_auto(ASL4, "  (This address is overlapped with blacklist and used by "
						"test region)\n");
			} else {
				pr_auto(ASL4, "  (This address is overlapped with blacklist and used by "
						"[%s])\n", subsystem_list[fault_blacklist_owner]);
			}
		}

		if (fault_type == FAULT_TYPE_MPTW_ACCESS_FAULT) {
			pr_auto(ASL4, "  - Fault Type\t\t: %s\n",
					"Memory protection table walk access fault");
		} else if (fault_type == FAULT_TYPE_AP_FAULT) {
			pr_auto(ASL4, "  - Fault Type\t\t: %s\n",
					"Access Permission fault");
		} else if (fault_type == FAULT_TYPE_CONTEXT_FAULT) {
			pr_auto(ASL4, "  - Fault Type\t\t: %s\n",
					"Context fault");
		} else {
			pr_info("  - Fault Type\t\t: %s, Num[%#x]\n",
					"Anonymous type", fault_type);
		}

		pr_auto(ASL4, "  - Fault Direction\t: %s\n",
				fault_rw ? "WRITE" : "READ");
		pr_auto(ASL4, "  - Fault VID\t\t: %d\n", fault_vid);
		pr_auto(ASL4, "  - Fault RW Length\t: %#x\n", len);
		pr_auto(ASL4, "  - Fault AxID\t\t: %#x\n", axid);
		pr_auto(ASL4, "  - Fault Context\t: CONTEXT%d_VID[%d] / %s\n",
				(ctx >> S2MPU_CONTEXT_VID_SHIFT) &
				S2MPU_CONTEXT_VID_MASK,
				(ctx >> S2MPU_CONTEXT_VID_SHIFT) &
				S2MPU_CONTEXT_VID_MASK,
				((ctx >> S2MPU_CONTEXT_VALID_SHIFT) &
				 S2MPU_CONTEXT_VALID_MASK) ?
				"VALID" : "INVALID");

		pr_auto(ASL4, "\n");
		pr_info("\n");

		pr_info("  - Fault MPTC dump\n");

		fstlb = &info->fault_mptc[S2MPU_FAULT_STLB_INDEX];

		pr_info(" ----------------------------------- [STLB] ----------------------------------\n");
		for (j = 0; j < NUM_OF_MPTC_WAY; j++) {
			stlb = &fstlb->mptc[j];

			pr_info("| SET[%d]/WAY[%d] : %s : PPN[0x%06x] - VID[%d] - Granule[%#x] - AP[0x%08x] |\n",
					fstlb->set, j, (stlb->valid == 1) ? "V" : "I",
					stlb->ppn, stlb->vid, stlb->granule, stlb->ap);
		}
		pr_info(" -----------------------------------------------------------------------------\n");

		fptlb = &info->fault_mptc[S2MPU_FAULT_PTLB_INDEX];

		pr_info(" ----------------------------------- [PTLB] ----------------------------------\n");
		for (j = 0; j < NUM_OF_MPTC_WAY; j++) {
			ptlb = &fptlb->mptc[j];

			pr_info("| SET[%d]/WAY[%d] : %s : PPN[0x%06x] - VID[%d] - Granule[%#x] - AP[0x%08x] |\n",
					fptlb->set, j, (ptlb->valid == 1) ? "V" : "I",
					ptlb->ppn, ptlb->vid, 0x0, ptlb->ap);
		}
		pr_info(" -----------------------------------------------------------------------------\n");

		pr_info("\n");

		data->noti_info[fault_subsys_idx].subsystem = subsystem_list[fault_subsys_idx];
		data->noti_info[fault_subsys_idx].fault_addr = (addr_high << 32) | addr_low;
		data->noti_info[fault_subsys_idx].fault_rw = fault_rw;
		data->noti_info[fault_subsys_idx].fault_len = len;
		data->noti_info[fault_subsys_idx].fault_type = fault_type;
		data->notifier_flag[fault_subsys_idx] = S2MPU_NOTIFIER_SET;
	}

	pr_auto(ASL4, "=====================================================\n");

	spin_unlock_irqrestore(&s2mpu_lock, flags);

	ret = exynos_s2mpu_notifier_call(data);
do_halt:
	if (ret) {
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		if (dbg_snapshot_expire_watchdog())
			pr_err("WDT reset fails\n");
#endif
		BUG();
	}

	return IRQ_HANDLED;
}

static bool has_s2mpu(unsigned int pd)
{
	return (s2mpu_pd_bitmap & BIT_ULL_MASK(pd)) != 0;
}

unsigned long exynos_s2mpu_set_stage2_ap(const char *subsystem,
					 unsigned long base,
					 unsigned long size,
					 unsigned int ap)
{
	unsigned int subsys_idx;

	if (subsystem == NULL) {
		pr_err("%s: Invalid S2MPU name\n", __func__);
		return ERROR_INVALID_SUBSYSTEM_NAME;
	}

	for (subsys_idx = 0; subsys_idx < subsystem_num; subsys_idx++) {
		if (!strncmp(subsystem_list[subsys_idx],
			     subsystem,
			     EXYNOS_MAX_SUBSYSTEM_NAME_LEN))
			break;
	}

	if (subsys_idx == subsystem_num) {
		pr_err("%s: DO NOT support %s S2MPU\n",
				__func__, subsystem);
		return ERROR_DO_NOT_SUPPORT_SUBSYSTEM;
	}

	return exynos_hvc(HVC_FID_SET_S2MPU,
			  subsys_idx,
			  base, size, ap);
}
EXPORT_SYMBOL(exynos_s2mpu_set_stage2_ap);

unsigned long exynos_s2mpu_set_blacklist(const char *subsystem,
					 unsigned long base,
					 unsigned long size)
{
	unsigned int subsys_idx;

	if (subsystem == NULL) {
		pr_err("%s: Invalid S2MPU name\n", __func__);
		return ERROR_INVALID_SUBSYSTEM_NAME;
	}

	for (subsys_idx = 0; subsys_idx < subsystem_num; subsys_idx++) {
		if (!strncmp(subsystem_list[subsys_idx],
			     subsystem,
			     EXYNOS_MAX_SUBSYSTEM_NAME_LEN))
			break;
	}

	if (subsys_idx == subsystem_num) {
		pr_err("%s: DO NOT support %s S2MPU\n",
				__func__, subsystem);
		return ERROR_DO_NOT_SUPPORT_SUBSYSTEM;
	}

	return exynos_hvc(HVC_FID_SET_S2MPU_BLACKLIST,
			  subsys_idx, base, size, 0);
}
EXPORT_SYMBOL(exynos_s2mpu_set_blacklist);

unsigned long exynos_s2mpu_set_all_blacklist(unsigned long base,
					     unsigned long size)
{
	unsigned long ret = 0;

	ret = exynos_hvc(HVC_FID_SET_ALL_S2MPU_BLACKLIST,
			 base, size, 0, 0);
	if (ret) {
		pr_err("%s: [ERROR] base[%llx], size[%llx]\n",
				__func__, base, size);
		return ret;
	}

	pr_info("%s: Set s2mpu blacklist done for test\n", __func__);

	return ret;
}
EXPORT_SYMBOL(exynos_s2mpu_set_all_blacklist);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
static void s2mpu_update_pm_qos_sss(bool update)
{
	if (!pm_qos_sss.need_qos_sss)
		return;

	mutex_lock(&pm_qos_sss.qos_count_lock);

	if (update) {
		if (pm_qos_sss.qos_count == 0) {
			exynos_pm_qos_update_request(&pm_qos_sss.qos_sss,
						     pm_qos_sss.qos_sss_freq);

			pr_info("%s: + QoS SSS update [%d]\n",
					__func__, pm_qos_sss.qos_sss_freq);
		}

		pm_qos_sss.qos_count++;
	} else {
		if (pm_qos_sss.qos_count == 1) {
			exynos_pm_qos_update_request(&pm_qos_sss.qos_sss, 0);

			pr_info("%s: - QoS SSS release\n", __func__);
		}

		pm_qos_sss.qos_count--;
	}

	pr_debug("%s: qos_count = %d\n", __func__, pm_qos_sss.qos_count);

	mutex_unlock(&pm_qos_sss.qos_count_lock);
}
#endif

/**
 * exynos_s2mpu_verify_subsystem_fw - Verify the signature of sub-system FW.
 *
 * @subsystem:	 Sub-system name.
 *		 Please refer to subsystem-names property of s2mpu node
 *		 in exynosXXXX-security.dtsi.
 * @fw_id:	 FW ID of this subsystem.
 * @fw_base:	 FW base address.
 *		 It's physical address(PA) and should be aligned with 64KB
 *		 because of S2MPU granule.
 * @fw_bin_size: FW binary size.
 *		 It should be aligned with 4bytes because of the limit of
 *		 signature verification.
 * @fw_mem_size: The size to be used by FW.
 *		 This memory region needs to be protected from other
 *		 sub-systems. It should be aligned with 64KB like fw_base
 *		 because of S2MPU granlue.
 */
unsigned long exynos_s2mpu_verify_subsystem_fw(const char *subsystem,
					       unsigned int fw_id,
					       unsigned long fw_base,
					       size_t fw_bin_size,
					       size_t fw_mem_size)
{
	unsigned int idx;
	unsigned long ret = 0;

	if (subsystem == NULL) {
		pr_err("%s: subsystem is NULL pointer\n", __func__);
		return ERROR_INVALID_SUBSYSTEM_NAME;
	}

	if ((fw_base == 0) || (fw_bin_size == 0) || (fw_mem_size == 0)) {
		pr_err("%s: Invalid FW[%d] binary information",
				__func__, fw_id);
		pr_err(" - fw_base[%#llx]\n", fw_base);
		pr_err(" - fw_bin_size[%#llx]\n", fw_bin_size);
		pr_err(" - fw_mem_size[%#llx]\n", fw_mem_size);
		return ERROR_INVALID_FW_BIN_INFO;
	}

	for (idx = 0; idx < subsystem_num; idx++) {
		if (!strncmp(subsystem_list[idx],
				subsystem,
				EXYNOS_MAX_SUBSYSTEM_NAME_LEN))
			break;
	}

	if (idx == subsystem_num) {
		pr_err("%s: DO NOT SUPPORT %s Sub-system\n",
				__func__, subsystem);
		return ERROR_DO_NOT_SUPPORT_SUBSYSTEM;
	}

	pr_debug("%s: Start %s Sub-system FW[%d] verification\n",
			__func__, subsystem, fw_id);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	s2mpu_update_pm_qos_sss(PM_QOS_SSS_UPDATE);
#endif

	ret = exynos_hvc(HVC_FID_VERIFY_SUBSYSTEM_FW,
			 ((unsigned long)fw_id << SUBSYSTEM_FW_NUM_SHIFT) |
			 ((unsigned long)idx << SUBSYSTEM_INDEX_SHIFT),
			 fw_base,
			 fw_bin_size,
			 fw_mem_size);
	if (ret) {
		pr_err("%s: %s FW[%d] verification is failed (%#llx)\n",
				__func__, subsystem, fw_id, ret);
		return ret;
	}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	s2mpu_update_pm_qos_sss(PM_QOS_SSS_RELEASE);
#endif

	pr_info("%s: %s FW[%d] is verified!\n", __func__, subsystem, fw_id);

	return 0;
}
EXPORT_SYMBOL(exynos_s2mpu_verify_subsystem_fw);

/**
 * exynos_s2mpu_request_fw_stage2_ap - Request Stage 2 access permission
 *				       of FW to allow access memory.
 *
 * @subsystem: Sub-system name.
 *	       Please refer to subsystem-names property of s2mpu node
 *	       in exynosXXXX-security.dtsi.
 *
 * This function must be called in case that only sub-system FW is
 * verified.
 */
unsigned long exynos_s2mpu_request_fw_stage2_ap(const char *subsystem)
{
	unsigned int idx;
	unsigned long ret = 0;

	if (subsystem == NULL) {
		pr_err("%s: subsystem is NULL pointer\n", __func__);
		return ERROR_INVALID_SUBSYSTEM_NAME;
	}

	for (idx = 0; idx < subsystem_num; idx++) {
		if (!strncmp(subsystem_list[idx],
			     subsystem,
			     EXYNOS_MAX_SUBSYSTEM_NAME_LEN))
			break;
	}

	if (idx == subsystem_num) {
		pr_err("%s: DO NOT SUPPORT %s Sub-system\n",
				__func__, subsystem);
		return ERROR_DO_NOT_SUPPORT_SUBSYSTEM;
	}

	pr_debug("%s: %s Sub-system requests FW Stage 2 AP\n",
			__func__, subsystem);

	ret = exynos_hvc(HVC_FID_REQUEST_FW_STAGE2_AP,
			 idx, 0, 0, 0);
	if (ret) {
		pr_err("%s: Enabling %s FW Stage 2 AP is failed (%#llx)\n",
				__func__, subsystem, ret);
		return ret;
	}

	pr_info("%s: Allow %s FW Stage 2 access permission\n",
			__func__, subsystem);

	return 0;
}
EXPORT_SYMBOL(exynos_s2mpu_request_fw_stage2_ap);

/**
 * exynos_s2mpu_release_fw_stage2_ap - Release Stage 2 access permission
 *				       for sub-system FW region and block
 *				       all Stage 2 access permission of
 *				       the sub-system.
 *
 * @subsystem:	 Sub-system name
 *		 Please refer to subsystem-names property of
 *		 s2mpu node in exynosXXXX-security.dtsi.
 * @fw_id:	 FW ID of this subsystem
 *
 * This function must be called in case that only sub-system FW is verified.
 */
unsigned long exynos_s2mpu_release_fw_stage2_ap(const char *subsystem,
						uint32_t fw_id)
{
	unsigned int idx;
	unsigned long ret = 0;

	if (subsystem == NULL) {
		pr_err("%s: subsystem is NULL pointer\n", __func__);
		return ERROR_INVALID_SUBSYSTEM_NAME;
	}

	for (idx = 0; idx < subsystem_num; idx++) {
		if (!strncmp(subsystem_list[idx],
			     subsystem,
			     EXYNOS_MAX_SUBSYSTEM_NAME_LEN))
			break;
	}

	if (idx == subsystem_num) {
		pr_err("%s: DO NOT SUPPORT %s Sub-system\n",
				__func__, subsystem);
		return ERROR_DO_NOT_SUPPORT_SUBSYSTEM;
	}

	pr_debug("%s: Start %s releasing Sub-system FW[%d]\n",
			__func__, subsystem, fw_id);

	ret = exynos_hvc(HVC_FID_RELEASE_FW_STAGE2_AP,
			 idx, fw_id, 0, 0);
	if (ret) {
		pr_err("%s: %s FW[%d] release is failed (%#llx)\n",
				__func__, subsystem, fw_id, ret);
		return ret;
	}

	pr_info("%s: %s FW[%d] is released\n",
			__func__, subsystem, fw_id);

	return 0;
}
EXPORT_SYMBOL(exynos_s2mpu_release_fw_stage2_ap);

unsigned long exynos_pd_backup_s2mpu(unsigned int pd)
{
	unsigned long ret;

	if (has_s2mpu(pd) == false)
		return 0;

	ret = exynos_hvc(HVC_FID_BACKUP_RESTORE_S2MPU,
			 EXYNOS_PD_S2MPU_BACKUP,
			 pd, 0, 0);
	if (ret) {
		pr_err("%s: Fail to backup PD[%d] S2MPU (%#lx)\n",
				__func__, pd, ret);
	}

	return ret;
}
EXPORT_SYMBOL(exynos_pd_backup_s2mpu);

unsigned long exynos_pd_restore_s2mpu(unsigned int pd)
{
	unsigned long ret;

	if (has_s2mpu(pd) == false)
		return 0;

	ret = exynos_hvc(HVC_FID_BACKUP_RESTORE_S2MPU,
			 EXYNOS_PD_S2MPU_RESTORE,
			 pd, 0, 0);
	if (ret) {
		pr_err("%s: Fail to restore PD[%d] S2MPU (%#lx)\n",
				__func__, pd, ret);
	}

	return ret;
}
EXPORT_SYMBOL(exynos_pd_restore_s2mpu);

#ifdef CONFIG_OF_RESERVED_MEM
static int __init exynos_s2mpu_reserved_mem_setup(struct reserved_mem *remem)
{
	pr_err("%s: Reserved memory for S2MPU table: addr=%lx, size=%lx\n",
			__func__, remem->base, remem->size);

	return 0;
}
RESERVEDMEM_OF_DECLARE(s2mpu_table, "exynos,s2mpu_table", exynos_s2mpu_reserved_mem_setup);
#endif

static ssize_t exynos_s2mpu_enable_check(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int ret = 0;
	int idx;

	for (idx = 0; idx < s2mpu_num; idx++) {
		if (!strncmp(s2mpu_list[idx], "EMPTY",
					EXYNOS_MAX_SUBSYSTEM_NAME_LEN))
			continue;

		if (s2mpu_success[idx]) {
			ret += snprintf(buf + ret,
					(s2mpu_num - idx) * S2_BUF_SIZE,
					"s2mpu(%2d) %s is enable\n",
					idx, s2mpu_list[idx]);
		} else {
			ret += snprintf(buf + ret,
					(s2mpu_num - idx) * S2_BUF_SIZE,
					"s2mpu(%2d) %s is disable\n",
					idx, s2mpu_list[idx]);
		}
	}

	return ret;
}

static ssize_t exynos_s2mpu_enable_test(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	unsigned long ret = 0;
	unsigned long idx;

	dev_info(dev, "s2mpu enable check end\n");

	ret = exynos_hvc(HVC_FID_CHECK_S2MPU_ENABLE, 0, 0, 0, 0);

	for (idx = 0; idx < s2mpu_num; idx++) {
		if (!strncmp(s2mpu_list[idx], "EMPTY",
					EXYNOS_MAX_SUBSYSTEM_NAME_LEN))
			continue;

		if (!(ret & ((unsigned long)0x1 << idx))) {
			s2mpu_success[idx] = 0;
			dev_err(dev,
				"s2mpu(%lld) is not enable\n",
				idx);
		} else {
			s2mpu_success[idx] = 1;
		}
	}

	dev_info(dev, "s2mpu enable check end\n");

	return count;
}

static DEVICE_ATTR(exynos_s2mpu_enable, 0644,
		   exynos_s2mpu_enable_check,
		   exynos_s2mpu_enable_test);

static void __init get_s2mpu_pd_bitmap(void)
{
	unsigned long bitmap;

	bitmap = exynos_hvc(HVC_FID_GET_S2MPU_PD_BITMAP,
				0, 0, 0, 0);
	if (bitmap == HVC_UNK) {
		pr_err("%s: Fail to get S2MPU PD bitmap\n");
		return;
	}

	s2mpu_pd_bitmap = bitmap;
}

static int exynos_s2mpu_probe(struct platform_device *pdev)
{
	struct s2mpu_info_data *data;
	int i, ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	const char *domain_name;
#endif

	spin_lock_init(&s2mpu_lock);

	data = devm_kzalloc(&pdev->dev,
			    sizeof(struct s2mpu_info_data),
			    GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev,
			"Fail to allocate memory(s2mpu_info_data)\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, data);
	data->dev = &pdev->dev;
	data->instance_num = s2mpu_num;

	data->fault_info = devm_kzalloc(data->dev,
					sizeof(struct s2mpu_fault_info) *
					data->instance_num,
					GFP_KERNEL);
	if (!data->fault_info) {
		dev_err(data->dev,
			"Fail to allocate memory(s2mpu_fault_info)\n");
		return -ENOMEM;
	}

	data->fault_info_pa = virt_to_phys((void *)data->fault_info);

	dev_dbg(data->dev,
		"VA of s2mpu_fault_info : %lx\n",
		(unsigned long)data->fault_info);
	dev_dbg(data->dev,
		"PA of s2mpu_fault_info : %llx\n",
		data->fault_info_pa);

	ret = of_property_read_u32(data->dev->of_node,
				   "irqcnt",
				   &data->irqcnt);
	if (ret) {
		dev_err(data->dev,
			"Fail to get irqcnt(%d) from dt\n",
			data->irqcnt);
		goto out;
	}

	dev_dbg(data->dev,
		"The number of S2MPU interrupt : %d\n",
		data->irqcnt);

	for (i = 0; i < data->irqcnt; i++) {
		if (!strncmp(s2mpu_list[i], "EMPTY",
				EXYNOS_MAX_SUBSYSTEM_NAME_LEN))
			continue;

		data->irq[i] = irq_of_parse_and_map(data->dev->of_node, i);
		if (data->irq[i] <= 0) {
			dev_err(data->dev,
				"Fail to get irq(%d) from dt\n",
				data->irq[i]);
			ret = -EINVAL;
			goto out;
		}

		ret = devm_request_threaded_irq(data->dev,
						data->irq[i],
						exynos_s2mpu_irq_handler,
						exynos_s2mpu_irq_handler_thread,
						IRQF_ONESHOT,
						pdev->name,
						data);
		if (ret) {
			dev_err(data->dev,
				"Fail to request IRQ handler. ret(%d) irq(%d)\n",
				ret, data->irq[i]);
			goto out;
		}
	}

	ret = exynos_hvc(HVC_CMD_INIT_S2MPUFD,
			data->fault_info_pa,
			data->instance_num,
			sizeof(struct s2mpu_fault_info),
			0);
	if (ret) {
		switch (ret) {
		case S2MPUFD_ERROR_INVALID_CH_NUM:
			dev_err(data->dev,
				"S2MPU instance number(%d) defined in DT is invalid\n",
				data->instance_num);
			break;
		case S2MPUFD_ERROR_INVALID_FAULT_INFO_SIZE:
			dev_err(data->dev,
				"The size of struct s2mpu_fault_info(%#lx) is invalid\n",
				sizeof(struct s2mpu_fault_info));
			break;
		default:
			dev_err(data->dev,
				"Unknown error from HVC - ret[%#x]\n",
				ret);
			break;
		}

		ret = -EINVAL;
		goto out;
	}

	/* S2MPU Notifier */
	data->noti_info = devm_kzalloc(data->dev,
				       sizeof(struct s2mpu_notifier_info) *
				       subsystem_num,
				       GFP_KERNEL);
	if (!data->noti_info) {
		dev_err(data->dev,
			"Fail to allocate memory(s2mpu_notifier_info)\n");
		ret = -ENOMEM;
		goto out;
	}

	data->notifier_flag = devm_kzalloc(data->dev,
					   sizeof(unsigned int) *
					   subsystem_num,
					   GFP_KERNEL);
	if (!data->notifier_flag) {
		dev_err(data->dev,
			"Fail to allocate memory(s2mpu_notifier_flag)\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = device_create_file(data->dev, &dev_attr_exynos_s2mpu_enable);
	if (ret) {
		dev_err(data->dev,
			"exynos_s2mpu_enable sysfs_create_file fail");
		return ret;
	}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	/* Prepare to guarantee SSS freq */
	pm_qos_sss.need_qos_sss = of_property_read_bool(data->dev->of_node,
							"pm-qos-sss-support");
	if (pm_qos_sss.need_qos_sss) {
		ret = of_property_read_string(data->dev->of_node,
					      "sss-freq-domain",
					      &domain_name);
		if (ret) {
			dev_err(data->dev,
				"Fail to get sss-freq-domain\n");
			return ret;
		}

		if (!strncmp(domain_name, "INT", PM_QOS_SSS_FREQ_DOMAIN_LEN)) {
			pm_qos_sss.sss_freq_domain = PM_QOS_DEVICE_THROUGHPUT;
		} else if (!strncmp(domain_name, "MIF", PM_QOS_SSS_FREQ_DOMAIN_LEN)) {
			pm_qos_sss.sss_freq_domain = PM_QOS_BUS_THROUGHPUT;
		} else {
			dev_err(data->dev,
				"Invalid freq domain (%s)\n",
				domain_name);
			return -EINVAL;
		}

		dev_info(data->dev, "SSS freq domain = %s(%d)\n",
				domain_name, pm_qos_sss.sss_freq_domain);

		ret = of_property_read_u32(data->dev->of_node,
					   "qos-sss-freq",
					   &pm_qos_sss.qos_sss_freq);
		if (ret) {
			dev_err(data->dev,
				"Fail to get qos-sss-freq\n");
			return ret;
		}

		dev_info(data->dev, "QoS SSS freq = %d\n", pm_qos_sss.qos_sss_freq);

		exynos_pm_qos_add_request(&pm_qos_sss.qos_sss,
					  pm_qos_sss.sss_freq_domain,
					  0);

		pm_qos_sss.qos_count = 0;

		mutex_init(&pm_qos_sss.qos_count_lock);
	}
#endif

	dev_info(data->dev, "Exynos S2MPU driver probe done!\n");

	return 0;

out:
	platform_set_drvdata(pdev, NULL);

	data->fault_info = NULL;
	data->fault_info_pa = 0;

	data->instance_num = 0;
	data->irqcnt = 0;

	return ret;
}

static int exynos_s2mpu_remove(struct platform_device *pdev)
{
	struct s2mpu_info_data *data = platform_get_drvdata(pdev);
	int i;

	platform_set_drvdata(pdev, NULL);

	for (i = 0; i < data->instance_num; i++)
		data->irq[i] = 0;

	data->instance_num = 0;
	data->irqcnt = 0;

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_remove_request(&pm_qos_sss.qos_sss);
	pm_qos_sss.sss_freq_domain = 0;
	pm_qos_sss.qos_count = 0;
#endif

	return 0;
}

static const struct of_device_id exynos_s2mpu_of_match_table[] = {
	{ .compatible = "samsung,exynos-s2mpu", },
	{ },
};

static struct platform_driver exynos_s2mpu_driver = {
	.probe = exynos_s2mpu_probe,
	.remove = exynos_s2mpu_remove,
	.driver = {
		.name = "exynos-s2mpu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_s2mpu_of_match_table),
	}
};
MODULE_DEVICE_TABLE(of, exynos_s2mpu_of_match_table);

static int __init exynos_s2mpu_init(void)
{
	struct device_node *np = NULL;
	int ret = 0;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	unsigned long hvc_ret = 0;

	np = of_find_compatible_node(NULL, NULL, "samsung,exynos-s2mpu");
	if (np == NULL) {
		pr_err("%s: Do not support S2MPU\n", __func__);
		return -ENODEV;
	}

	rmem_np = of_parse_phandle(np, "memory_region", 0);
	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		pr_err("%s: fail to acquire memory region\n", __func__);
		return -EADDRNOTAVAIL;
	}

	pr_err("%s: Reserved memory for S2MPU table: addr=%lx, size=%lx\n",
			__func__, rmem->base, rmem->size);

	ret = of_property_read_u32(np, "subsystem-num", &subsystem_num);
	if (ret) {
		pr_err("%s: Fail to get S2MPU subsystem number from device tree\n",
			__func__);
		return ret;
	}

	pr_info("%s: S2MPU sub-system number : %d\n", __func__, subsystem_num);

	ret = of_property_read_string_array(np, "subsystem-names",
					    subsystem_list,
					    subsystem_num);
	if (ret < 0) {
		pr_err("%s: Fail to get S2MPU subsystem list from device tree\n",
			__func__);
		return ret;
	}

	ret = of_property_read_u32(np, "instance-num", &s2mpu_num);
	if (ret) {
		pr_err("%s: Fail to get S2MPU instance number from device tree\n",
			__func__);
		return ret;
	}

	pr_info("%s: S2MPU instance number : %d\n", __func__, s2mpu_num);

	ret = of_property_read_string_array(np, "instance-names",
					    s2mpu_list,
					    s2mpu_num);
	if (ret < 0) {
		pr_err("%s: Fail to get S2MPU instance list from device tree\n",
			__func__);
		return ret;
	}

	/* Set kernel_RO_region to all s2mpu blacklist */
	hvc_ret = exynos_hvc(HVC_FID_BAN_KERNEL_RO_REGION, 0, 0, 0, 0);

	if (hvc_ret != 0)
		pr_info("%s: S2MPU blacklist set fail, hvc_ret[%d]\n",
				__func__, hvc_ret);
	else
		pr_info("%s: Set kernel RO region to all s2mpu blacklist done\n",
				__func__);

	get_s2mpu_pd_bitmap();

	pr_info("%s: S2MPU PD bitmap - %llx\n", __func__, s2mpu_pd_bitmap);
	pr_info("%s: Making S2MPU list is done\n", __func__);

	return platform_driver_register(&exynos_s2mpu_driver);
}

static void __exit exynos_s2mpu_exit(void)
{
	platform_driver_unregister(&exynos_s2mpu_driver);
}

module_init(exynos_s2mpu_init);
module_exit(exynos_s2mpu_exit);

MODULE_DESCRIPTION("Exynos Stage 2 Protection Unit(S2MPU) driver");
MODULE_AUTHOR("<junhosj.choi@samsung.com>");
MODULE_LICENSE("GPL");
