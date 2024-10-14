/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - Stage 2 Protection Unit(S2MPU)
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_S2MPU_H__
#define __EXYNOS_S2MPU_H__

#define EXYNOS_MAX_S2MPU_INSTANCE		(64)
#define EXYNOS_MAX_SUBSYSTEM			(32)
#define EXYNOS_MAX_SUBSYSTEM_NAME_LEN		(16)

#define SUBSYSTEM_FW_NUM_SHIFT			(32)
#define SUBSYSTEM_INDEX_SHIFT			(0)

/* Error */
#define ERROR_INVALID_SUBSYSTEM_NAME		(0x1)
#define ERROR_DO_NOT_SUPPORT_SUBSYSTEM		(0x2)
#define ERROR_INVALID_FW_BIN_INFO		(0x3)
#define ERROR_INVALID_NOTIFIER_BLOCK		(0x4)
#define ERROR_INVALID_PARAMETER			(0x5)
#define ERROR_S2MPU_NOT_INITIALIZED		(0xE12E0001)

/* Backup and restore S2MPU */
#define EXYNOS_PD_S2MPU_BACKUP			(0)
#define EXYNOS_PD_S2MPU_RESTORE			(1)

/* S2MPU fault */
#define MAX_VID_OF_S2MPU_FAULT_INFO		(8)

#define FAULT_TYPE_MPTW_ACCESS_FAULT		(0x1)
#define FAULT_TYPE_AP_FAULT			(0x2)
#define FAULT_TYPE_CONTEXT_FAULT		(0x4)
#define FAULT_THIS_ADDR_IS_NOT_BLACKLIST	(0xFFFF)

#define S2MPU_CONTEXT_VALID_SHIFT		(3)
#define S2MPU_CONTEXT_VID_SHIFT			(0)
#define S2MPU_CONTEXT_VALID_MASK		(0x1)
#define S2MPU_CONTEXT_VID_MASK			(0x7)

#define OWNER_IS_KERNEL_RO			(99)
#define OWNER_IS_TEST				(98)

#define ERROR_NOTHING_S2MPU_FAULT		(0x2)

/* Return values from SMC */
#define S2MPUFD_ERROR_INVALID_CH_NUM		(0x600)
#define S2MPUFD_ERROR_INVALID_FAULT_INFO_SIZE	(0x601)

/* S2MPU Notifier */
#define S2MPU_NOTIFIER_SET			(1)
#define S2MPU_NOTIFIER_UNSET			(0)

#define S2MPU_NOTIFIER_PANIC			(0xFF00000000)
#define S2MPU_NOTIFY_OK				(0)
#define S2MPU_NOTIFY_BAD			(1)

/* S2MPU enable check */
#define S2_BUF_SIZE 				(30)

/* MPTC */
#define NUM_OF_MPTC_WAY				(4)

#define S2MPU_MAX_NUM_OF_FAULT_MPTC		(2)
#define S2MPU_FAULT_STLB_INDEX			(0)
#define S2MPU_FAULT_PTLB_INDEX			(1)

/* For backward compatibility */
#define exynos_verify_subsystem_fw		exynos_s2mpu_verify_subsystem_fw
#define exynos_request_fw_stage2_ap		exynos_s2mpu_request_fw_stage2_ap

/* PM QoS for SSS */
#define PM_QOS_SSS_UPDATE			(true)
#define PM_QOS_SSS_RELEASE			(false)

#define PM_QOS_SSS_FREQ_DOMAIN_LEN		(8)

#ifndef __ASSEMBLY__

#include <linux/mutex.h>

#include <soc/samsung/exynos_pm_qos.h>

/* S2MPU access permission */
enum stage2_ap {
	ATTR_NO_ACCESS = 0x0,
	ATTR_RO = 0x1,
	ATTR_WO = 0x2,
	ATTR_RW = 0x3
};

/* Registers of S2MPUFD Fault Information */
struct s2mpu_mptc {
	uint32_t valid;
	uint32_t ppn;
	uint32_t vid;
	uint32_t granule;
	uint32_t ap;
};

struct s2mpu_fault_mptc {
	uint32_t set;
	struct s2mpu_mptc mptc[NUM_OF_MPTC_WAY];
};

struct __s2mpu_fault_info {
	unsigned int fault_addr_low;
	unsigned int fault_addr_high;
	unsigned int vid;
	unsigned int type;
	unsigned int rw;
	unsigned int len;
	unsigned int axid;
	unsigned int context;
	unsigned int subsystem;
	unsigned int blacklist_owner;
	struct s2mpu_fault_mptc fault_mptc[S2MPU_MAX_NUM_OF_FAULT_MPTC];
};

struct s2mpu_fault_info {
	unsigned int fault_cnt;
	struct __s2mpu_fault_info info[MAX_VID_OF_S2MPU_FAULT_INFO];
};

/* Structure of notifier info */
struct s2mpu_notifier_info {
	const char *subsystem;
	unsigned long fault_addr;
	unsigned int fault_rw;
	unsigned int fault_len;
	unsigned int fault_type;
};

/* Structure of notifier block */
struct s2mpu_notifier_block {
	const char *subsystem;
	int (*notifier_call)(struct s2mpu_notifier_block *,
			     struct s2mpu_notifier_info *);
};

/* Data structure for S2MPU Fault Information */
struct s2mpu_info_data {
	struct device *dev;
	struct s2mpu_fault_info *fault_info;
	dma_addr_t fault_info_pa;
	unsigned int instance_num;
	unsigned int irq[EXYNOS_MAX_S2MPU_INSTANCE];
	unsigned int irqcnt;
	struct s2mpu_notifier_info *noti_info;
	unsigned int *notifier_flag;
};

/* PM QoS for SSS */
struct s2mpu_pm_qos_sss {
	struct exynos_pm_qos_request qos_sss;
	unsigned int sss_freq_domain;
	unsigned int qos_sss_freq;
	unsigned int qos_count;
	struct mutex qos_count_lock;
	bool need_qos_sss;
};


unsigned long exynos_s2mpu_set_stage2_ap(const char *subsystem,
					 unsigned long base,
					 unsigned long size,
					 unsigned int ap);
unsigned long exynos_s2mpu_set_blacklist(const char *subsystem,
					 unsigned long base,
					 unsigned long size);
unsigned long exynos_s2mpu_set_all_blacklist(unsigned long base,
					     unsigned long size);

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
					       size_t fw_mem_size);

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
unsigned long exynos_s2mpu_request_fw_stage2_ap(const char *subsystem);

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
						uint32_t fw_id);

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
unsigned long exynos_s2mpu_notifier_call_register(struct s2mpu_notifier_block *snb);

#ifdef CONFIG_EXYNOS_S2MPU_PD
unsigned long exynos_pd_backup_s2mpu(unsigned int pd);
unsigned long exynos_pd_restore_s2mpu(unsigned int pd);
#else
static inline unsigned long exynos_pd_backup_s2mpu(unsigned int pd)
{
	return 0;
}

static inline unsigned long exynos_pd_restore_s2mpu(unsigned int pd)
{
	return 0;
}
#endif

#endif	/* __ASSEMBLY__ */
#endif	/* __EXYNOS_S2MPU_H__ */
