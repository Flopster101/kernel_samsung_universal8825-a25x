/*
 * copyright (c) 2021 Samsung Electronics Co., Ltd.
 *            http://www.samsung.com/
 *
 * EXYNOS - CryptoManager for SSS H/W debugging
 * Author: Jiye Min <jiye.min@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/printk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/kdebug.h>
#include <asm/cacheflush.h>
#include <asm/memory.h>
#include <asm/arch_timer.h>
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/exynos-cm.h>
#if IS_ENABLED(CONFIG_EXYNOS_SEH)
#include <soc/samsung/exynos-seh.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
#include <soc/samsung/exynos-itmon.h>
#endif
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/debug-snapshot.h>
#endif

#define CM_SSS_HWSEMA_NOT_SUPPORT		(0x70800) /* define at LDFW */
#define CM_INVALID_SMC_CMD			(0x10100) /* define at LDFW */
#define CM_TRUE					(0x91827364)

#define CM_MAX_RETRY_COUNT			(10000)

/* smc to call ldfw functions */
#define SMC_CM_SSS_HW_SEMA			(0xC2001031)
#define CMD_GET_HW_SEMA_ENABLED			(1)
#define CMD_GET_DEBUG_ADDR			(2)

static struct cm_sss_debug_mem *debug_mem;
static void __iomem *sysreg_va_base;
static u32 flag_set_sss_debug_mem;
static u32 flag_enable_hw_sema;
static u64 flag_check_timestamp;

struct cm_device {
	struct device *dev;

	u32 sss_ap_sfr_addr;
	u32 sss_cp_sfr_addr;
	u32 sss_debug_sysreg_addr;         /* HW_APBSEMA_MEC_DBG address */

	u64 cm_debug_mem_addr;
	u64 cm_debug_mem_va_addr;
	u32 cm_debug_mem_size;

	unsigned long sss_debug_mem_pa_addr;

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	struct notifier_block itmon_nb;
#endif
	u32 itmon_notified;
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	struct notifier_block panic_nb;
	struct notifier_block die_nb;
#endif
	u32 panic_notified;
};

static int exynos_cm_smc(u64 *arg0, u64 *arg1, u64 *arg2, u64 *arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(*arg0, *arg1, *arg2, *arg3, 0, 0, 0, 0, &res);

	*arg0 = res.a0;
	*arg1 = res.a1;
	*arg2 = res.a2;
	*arg3 = res.a3;

	return *arg0;
}

static int exynos_cm_map_sfr(u32 pa_addr, void __iomem **va_addr, unsigned int map_size)
{
	int ret = 0;

	*va_addr = ioremap(pa_addr, map_size);
	if (!*va_addr) {
		pr_err("%s: fail to ioremap\n", __func__);
		ret = -EFAULT;
		return ret;
	}

	return ret;
}

static int exynos_cm_get_hw_sema_enabled(u32 *info)
{
	u64 reg0;
	u64 reg1;
	u64 reg2;
	u64 reg3;
	int ret;

	reg0 = SMC_CM_SSS_HW_SEMA;
	reg1 = CMD_GET_HW_SEMA_ENABLED;
	reg2 = 0;
	reg3 = 0;

	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	if (ret != 0)
		return ret;

	*info = (uint32_t)reg2;

	return ret;
}

static int exynos_cm_get_debug_info(u64 *addr, u32 *size)
{
	u64 reg0;
	u64 reg1;
	u64 reg2;
	u64 reg3;
	int ret;

	reg0 = SMC_CM_SSS_HW_SEMA;
	reg1 = CMD_GET_DEBUG_ADDR;
	reg2 = 0;
	reg3 = 0;

	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	if (ret != 0) {
		pr_err("%s: exynos_cm_get_debug_info failed: 0x%x\n", __func__, ret);
		return ret;
	}

	*addr = reg2;
	*size = reg3;

	return ret;
}

void exynos_cm_set_sss_debug_mem(void)
{
	void __iomem *DBG_ADDR = sysreg_va_base;
	struct cm_sss_debug_mem *info = debug_mem;
	unsigned int DBG_VAL = 0;
	unsigned int DBG_STAT_VAL = 0;
	unsigned int DBG_REQ_CANCEL_CNT_AP = 0;
	unsigned int DBG_REQ_CANCEL_CNT_CP = 0;
	unsigned int DBG_REQ_PERIOD_CNT_AP = 0;
	unsigned int DBG_REQ_PERIOD_CNT_CP = 0;
	unsigned int DBG_TO_CNT_AP = 0;
	unsigned int DBG_TO_CNT_CP = 0;
	unsigned int temp, retry_cnt;
	uint8_t ap_name[CM_MAGIC_STRING_LEN] = "AP_INFO";
	uint8_t cp_name[CM_MAGIC_STRING_LEN] = "CP_INFO";

	/* If SSS HW_SEMA is not enabled, return */
	if (flag_enable_hw_sema != CM_TRUE)
		return;

	if (!DBG_ADDR || !info) {
		pr_info("%s: DBG_ADDR %lx, info %lx\n", __func__, DBG_ADDR, info);
		return;
	}

	if (flag_set_sss_debug_mem == CM_TRUE) {
		pr_info("%s: already function is called\n", __func__);
		return;
	}

	/* read debug register */
	DBG_VAL = readl(DBG_ADDR);
	DBG_STAT_VAL = readl(DBG_ADDR + SYSREG_OFFSET_DBG_STATE);
	DBG_REQ_CANCEL_CNT_AP = readl(DBG_ADDR + SYSREG_OFFSET_DBG_REQ_CANCEL_CNT_AP);
	DBG_REQ_CANCEL_CNT_CP = readl(DBG_ADDR+ SYSREG_OFFSET_DBG_REQ_CANCEL_CNT_CP);
	DBG_REQ_PERIOD_CNT_AP = readl(DBG_ADDR+ SYSREG_OFFSET_DBG_REQ_PERIOD_CNT_AP);
	DBG_REQ_PERIOD_CNT_CP = readl(DBG_ADDR + SYSREG_OFFSET_DBG_REQ_PERIOD_CNT_CP);

	/* set AP debugging information */
	memcpy(info->ap_info.magic, ap_name, sizeof(ap_name));
	info->ap_info.dbg_req = ((DBG_VAL & HW_SEMA_MEC_DBG_REQ_MASK) &
				HW_SEMA_MEC_DBG_REQ_AP) >> DBG_REQ_AP_SHIFT;
	info->ap_info.dbg_ack = ((DBG_VAL & HW_SEMA_MEC_DBG_ACK_MASK) &
				HW_SEMA_MEC_DBG_ACK_AP) >> DBG_ACK_AP_SHIFT;

	/* interrupt status */
	temp = DBG_VAL & HW_SEMA_MEC_DBG_INT_STATUS_MASK;
	info->ap_info.dbg_ack_int_stat = (temp & HW_SEMA_MEC_DBG_ACK_INT_STATUS_AP) >>
					 DBG_ACK_INT_STAT_AP_SHIFT;
	info->ap_info.dbg_to_int_stat = (temp & HW_SEMA_MEC_DBG_TO_INT_STATUS_AP) >>
					DBG_TO_INT_STAT_AP_SHIFT;

	/* interrupt history */
	temp = DBG_VAL & HW_SEMA_MEC_DBG_INT_HISTORY_MASK;
	info->ap_info.dbg_to_history = (temp & HW_SEMA_MEC_DBG_TO_HISTORY_AP) >>
				       DBG_TO_HISTORY_AP_SHIFT;
	info->ap_info.dbg_ack_history = (temp & HW_SEMA_MEC_DBG_ACK_HISTORY_AP) >>
					DBG_ACK_HISTORY_AP_SHIFT;

	/* counter values */
	info->ap_info.dbg_req_cancel_cnt = DBG_REQ_CANCEL_CNT_AP &
					   HW_SEMA_MEC_DBG_REQ_CANCLE_CNT_AP_MASK;
	info->ap_info.dbg_req_period_cnt = DBG_REQ_PERIOD_CNT_AP &
					   HW_SEMA_MEC_DBG_REQ_PERIOD_CNT_AP_MASK;

	/* timeout counter of AP */
	retry_cnt = 0;
	do {
		if (retry_cnt++ > CM_MAX_RETRY_COUNT) {
			pr_info("%s: exceed retry in read TIMEOUT_CNT_AP", __func__);
			break;
		}

		DBG_TO_CNT_AP = readl(DBG_ADDR + SYSREG_OFFSET_DBG_TO_CNT_AP);
	} while ((DBG_TO_CNT_AP & HW_SEMA_MEC_DBG_TO_CNT_AP_MASK) == 0);

	info->ap_info.dbg_to_cnt = DBG_TO_CNT_AP & HW_SEMA_MEC_DBG_TO_CNT_AP_MASK;

	/* set CP debugging information */
	memcpy(info->cp_info.magic, cp_name, sizeof(cp_name));
	info->cp_info.dbg_req = ((DBG_VAL & HW_SEMA_MEC_DBG_REQ_MASK) &
				HW_SEMA_MEC_DBG_REQ_CP) >> DBG_REQ_CP_SHIFT;
	info->cp_info.dbg_ack = ((DBG_VAL & HW_SEMA_MEC_DBG_ACK_MASK) &
				HW_SEMA_MEC_DBG_ACK_CP) >> DBG_ACK_CP_SHIFT;

	/* interrupt status */
	temp = DBG_VAL & HW_SEMA_MEC_DBG_INT_STATUS_MASK;
	info->cp_info.dbg_ack_int_stat = (temp & HW_SEMA_MEC_DBG_ACK_INT_STATUS_CP) >>
					 DBG_ACK_INT_STAT_CP_SHIFT;
	info->cp_info.dbg_to_int_stat = (temp & HW_SEMA_MEC_DBG_TO_INT_STATUS_CP) >>
					DBG_TO_INT_STAT_CP_SHIFT;

	/* interrupt history */
	temp = DBG_VAL & HW_SEMA_MEC_DBG_INT_HISTORY_MASK;
	info->cp_info.dbg_to_history = (temp & HW_SEMA_MEC_DBG_TO_HISTORY_CP) >>
				       DBG_TO_HISTORY_CP_SHIFT;
	info->cp_info.dbg_ack_history = (temp & HW_SEMA_MEC_DBG_ACK_HISTORY_CP) >>
					DBG_ACK_HISTORY_CP_SHIFT;

	/* counter values */
	info->cp_info.dbg_req_cancel_cnt = DBG_REQ_CANCEL_CNT_CP &
					   HW_SEMA_MEC_DBG_REQ_CANCLE_CNT_CP_MASK;
	info->cp_info.dbg_req_period_cnt = DBG_REQ_PERIOD_CNT_CP &
					   HW_SEMA_MEC_DBG_REQ_PERIOD_CNT_CP_MASK;

	/* timeout counter of CP */
	retry_cnt = 0;
	do {
		if (retry_cnt++ > CM_MAX_RETRY_COUNT) {
			pr_info("%s: exceed retry in read TIMEOUT_CNT_CP", __func__);
			break;
		}

		DBG_TO_CNT_CP = readl(DBG_ADDR + SYSREG_OFFSET_DBG_TO_CNT_CP);
	} while ((DBG_TO_CNT_CP & HW_SEMA_MEC_DBG_TO_CNT_CP_MASK) == 0);

	info->cp_info.dbg_to_cnt = DBG_TO_CNT_CP & HW_SEMA_MEC_DBG_TO_CNT_CP_MASK;

	/* state information */
	info->st_info.arbiter_state = (DBG_STAT_VAL & HW_SEMA_MEC_DBG_ARBITER_STAT_MASK) >>
				      DBQ_ARBITER_FSM_STAT_SHIFT;

	/* core state */
	temp = DBG_STAT_VAL & HW_SEMA_MEC_DBG_CORE_STAT_MASK;
	info->st_info.core_state_ap = (temp & HW_SEMA_MEC_DBG_CORE_FSM_STAT_AP) >>
				      DBG_CORE_FSM_STAT_AP_SHIFT;
	info->st_info.core_state_cp = (temp & HW_SEMA_MEC_DBG_CORE_FSM_STAT_CP) >>
				      DBG_CORE_FSM_STAT_CP_SHIFT;

	/* AZ state */
	temp = DBG_STAT_VAL & HW_SEMA_MEC_DBG_AZ_STAT_MASK;
	info->st_info.az_fsm_state = (temp & HW_SEMA_MEC_DBG_AZ_FSM_STAT) >>
				     DBG_AZ_FSM_STAT_SHIFT;
	info->st_info.az_req_ap = (temp & HW_SEMA_MEC_DBG_AZ_REQ_AP) >>
				  DBG_AZ_REQ_AP_SHIFT;
	info->st_info.az_req_cp = (temp & HW_SEMA_MEC_DBG_AZ_REQ_CP) >>
				  DBG_AZ_REQ_CP_SHIFT;

	/* flush debug memory */
	flush_dcache_page((struct page *)info);

	flag_set_sss_debug_mem = CM_TRUE;

	pr_info("%s: dump success.\n", __func__);
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
static void exynos_cm_print_debug_arbiter_state(uint64_t state)
{
	switch (state) {
	case ARBITER_ST_IDLE:
		pr_info("  - APBITER State: 0x%x (SEMA Disable state)", state);
		break;
	case ARBITER_ST_INIT:
		pr_info("  - APBITER State: 0x%x (Init Value)", state);
		break;
	case ARBITER_ST_CHECK:
		pr_info("  - APBITER State: 0x%x (Check PERMISSION)", state);
		break;
	case ARBITER_ST_STAY:
		pr_info("  - APBITER State: 0x%x (Maintain PERMISSION)", state);
		break;
	default:
		pr_info("  - ARBITER State is invalid");
		break;
	}
}

static void exynos_cm_print_debug_core_state(uint32_t core_num,
					     uint8_t *name,
					     uint64_t state)
{
	switch (state) {
	case CORE_FSM_ST_IDLE:
		pr_info("    > Core %d (%s) FSM State: 0x%x (SEMA Disable state)",
			core_num, name, state);
		break;
	case CORE_FSM_ST_INIT:
		pr_info("    > Core %d (%s) FSM State: 0x%x (Init value (SEMA enable))",
			core_num, name, state);
		break;
	case CORE_FSM_ST_WAIT_SEMA:
		pr_info("    > Core %d (%s) FSM State: 0x%x (REQ and wait PERMISSION)",
			core_num, name, state);
		break;
	case CORE_FSM_ST_DECODE:
		pr_info("    > Core %d (%s) FSM State: 0x%x (Maintain PERMISSION)",
			core_num, name, state);
		break;
	case CORE_FSM_ST_DATA:
		pr_info("    > Core %d (%s) FSM State: 0x%x (DATA in progress)",
			core_num, name, state);
		break;
	case CORE_FSM_ST_DONE:
		pr_info("    > Core %d (%s) FSM State: 0x%x (PERMISSION release)",
			core_num, name, state);
		break;
	default:
		pr_info("    > Core %d (%s) FSM State is Invalid");
		break;
	}
}

static void exynos_cm_print_debug_az_state(uint64_t state)
{
	switch (state) {
	case AZ_FSM_ST_IDLE:
		pr_info("    > AZ FSM State: 0x%x (SEMA Disable state)", state);
		break;
	case AZ_FSM_ST_INIT:
		pr_info("    > AZ FSM State: 0x%x (Init value)", state);
		break;
	case AZ_FSM_ST_END:
		pr_info("    > AZ FSM State: 0x%x (Auto Zeroise done)", state);
		break;
	case AZ_FSM_ST_INT_SET:
		pr_info("    > AZ FSM State: 0x%x (Interrupt setting)", state);
		break;
	case AZ_FSM_ST_IP_SEL:
		pr_info("    > AZ FSM State: 0x%x (Auto Zeroise IP select)", state);
		break;
	case AZ_FSM_ST_START:
		pr_info("    > AZ FSM State: 0x%x (Auto Zeroise start)", state);
		break;
	case AZ_FSM_ST_WAIT:
		pr_info("    > AZ FSM State: 0x%x (Waiting for Zeroise result)", state);
		break;
	case AZ_FSM_ST_DONE:
		pr_info("    > AZ FSM State: 0x%x (Receive Zeroise done interrupt and clearing interrupt)",
			state);
		break;
	case AZ_FSM_ST_ERROR:
		pr_info("    > AZ FSM State: 0x%x (Receive Zeroise error interrupt and clearing interrupt)",
			state);
		break;
	case AZ_FSM_ST_SW_RESET:
		pr_info("    > AZ FSM State: 0x%x (S/W reset upon receiving Zeroise error interrupt",
			state);
		break;
	case AZ_FSM_ST_SW_RESET_WAIT:
		pr_info("    > AZ FSM State: 0x%x (Waiting for S/W reset result)", state);
		break;
	case AZ_FSM_ST_SW_RESET_CNT_INIT:
		pr_info("    > AZ FSM State: 0x%x (S/W reset Period counter initialize)",
			state);
		break;
	case AZ_FSM_ST_PKE_INT_SET:
		pr_info("    > AZ FSM State: 0x%x (interrupt setting for PKE Zeroise)",
			state);
		break;
	case AZ_FSM_ST_PKE_SEL:
		pr_info("    > AZ FSM State: 0x%x (Auto Zeroise PKE select)", state);
		break;
	default:
		pr_info("    > AZ FSM State is invalid");
		break;
	}
}

static void exynos_cm_print_sss_debug_sysreg(struct cm_device *cmdev)
{
	struct cm_sss_debug_mem *info = debug_mem;
	uint8_t ap_magic[4] = "AP";
	uint8_t cp_magic[4] = "CP";

	if (!info)
		return;

	pr_info("=========================[SSS ITMON ERROR]=========================");
	pr_info("  > AP Information");

	pr_info("  - REQ status: 0x%x", info->ap_info.dbg_req);
	pr_info("  - ACK status: 0x%x", info->ap_info.dbg_ack);

	pr_info("  - Interrupt Status");
	pr_info("    > ACK Interrupt: 0x%x", info->ap_info.dbg_ack_int_stat);
	if (info->ap_info.dbg_to_int_stat == 0) {
		pr_info("    > Timeout Interrupt: 0x0");
	} else {
		pr_info("    > Timeout Interrupt: 0x1 (AP timeout is occured!)");
	}

	pr_info("  - Interrupt History (Indicates the previous usage history of HW_SEMA)");
	if (info->ap_info.dbg_to_history == 0) {
		pr_info("    > Timeout History: 0x0");
	} else {
		pr_info("    > Timeout History: 0x1 (AP timeout is occured!)");
	}

	if (info->ap_info.dbg_ack_history == 0) {
		pr_info("    > SEMA_ACK History: 0x0");
	} else {
		pr_info("    > SEMA_ACK History: 0x1");
	}

	pr_info("  - Request Cancel Counter: 0x%x", info->ap_info.dbg_req_cancel_cnt);
	pr_info("  - Request Period Counter: 0x%x", info->ap_info.dbg_req_period_cnt);
	pr_info("  - Timeout Counter: 0x%x", info->ap_info.dbg_to_cnt);

	pr_info("------------------------------------------------------------------");
	pr_info("  > CP Information");

	pr_info("  - REQ status: 0x%x", info->cp_info.dbg_req);
	pr_info("  - ACK status: 0x%x", info->cp_info.dbg_ack);

	pr_info("  - Interrupt Status");
	pr_info("    > ACK Interrupt: 0x%x", info->cp_info.dbg_ack_int_stat);
	if (info->cp_info.dbg_to_int_stat == 0) {
		pr_info("    > Timeout Interrupt: 0x0");
	} else {
		pr_info("    > Timeout Interrupt: 0x1 (CP timeout is occured!)");
	}

	pr_info("  - Interrupt History (Indicates the previous usage history of HW_SEMA)");
	if (info->cp_info.dbg_to_history == 0) {
		pr_info("    > Timeout History: 0x0");
	} else {
		pr_info("    > Timeout History: 0x1 (CP timeout is occured!)");
	}

	if (info->cp_info.dbg_ack_history == 0) {
		pr_info("    > SEMA_ACK History: 0x0");
	} else {
		pr_info("    > SEMA_ACK History: 0x1");
	}

	pr_info("  - Request Cancel Counter: 0x%x", info->cp_info.dbg_req_cancel_cnt);
	pr_info("  - Request Period Counter: 0x%x", info->cp_info.dbg_req_period_cnt);
	pr_info("  - Timeout Counter: 0x%x", info->cp_info.dbg_to_cnt);

	pr_info("-----------------------------------------------------------------");
	pr_info("  > State Information");

	/* print ARBITER state */
	exynos_cm_print_debug_arbiter_state(info->st_info.arbiter_state);

	pr_info("  - Core State");
	exynos_cm_print_debug_core_state(0, ap_magic, info->st_info.core_state_ap);
	exynos_cm_print_debug_core_state(1, cp_magic, info->st_info.core_state_cp);

	pr_info("  - AZ State");
	exynos_cm_print_debug_az_state(info->st_info.az_fsm_state);
	pr_info("    > AZ REQ AP: %d\n", info->st_info.az_req_ap);
	pr_info("    > AZ REQ CP: %d\n", info->st_info.az_req_cp);
	pr_info("=================================================================");
}

static int __exynos_cm_itmon_notifier(struct notifier_block *nb,
				      unsigned long action,
				      void *nb_data)
{
	struct cm_device *cmdev = NULL;
	struct itmon_notifier *itmon_info = nb_data;
	int is_sss_itmon = 0;

	/* check if SSS HW_SEMA is enabled */
	if (flag_enable_hw_sema != CM_TRUE)
		return 0;

	if (flag_check_timestamp == 0) {
		pr_info("%s: debug handler timestamp 0x%lx\n", __func__, arch_timer_read_cntpct_el0());
		flag_check_timestamp = 1;
	}

	cmdev = container_of(nb, struct cm_device, itmon_nb);

	if (cmdev->itmon_notified == CM_TRUE)
		return NOTIFY_DONE;

	if (IS_ERR_OR_NULL(itmon_info))
		return NOTIFY_DONE;

	/* check SSS ITMON error is occured */
	if (itmon_info->port &&
	    (((itmon_info->target_addr & 0xFFFF0000) == cmdev->sss_ap_sfr_addr) ||
	     ((itmon_info->target_addr & 0xFFFF0000) == cmdev->sss_cp_sfr_addr)))
		is_sss_itmon = 1;

	if (!is_sss_itmon)
		return NOTIFY_DONE;

	pr_info("%s: port: %s, dest: %s, master: %s\n", __func__,
		itmon_info->port, itmon_info->dest, itmon_info->master);

	/* print debugging information */
	exynos_cm_set_sss_debug_mem();
	exynos_cm_print_sss_debug_sysreg(cmdev);

	cmdev->itmon_notified = CM_TRUE;

	return NOTIFY_OK;
}
#endif

static int exynos_cm_debug_handler(struct cm_device *cmdev)
{
	/* If SSS HW_SEMA is not enabled, return */
	if (flag_enable_hw_sema != CM_TRUE)
		return 0;

	if (cmdev->panic_notified == CM_TRUE)
		return 0;

	if (flag_check_timestamp == 0) {
		pr_info("%s: debug handler timestamp 0x%lx\n", __func__, arch_timer_read_cntpct_el0());
		flag_check_timestamp = 1;
	}

	/* set SSS debugging information */
	exynos_cm_set_sss_debug_mem();

	cmdev->panic_notified = CM_TRUE;

	pr_debug("%s: exynos_cm_debug_handler exit\n", __func__);

	return 0;
}

static int exynos_cm_panic_handler(struct notifier_block *nb, unsigned long l, void *buf)
{
	struct cm_device *cmdev = container_of(nb, struct cm_device, panic_nb);

	pr_debug("%s called\n", __func__);

	return exynos_cm_debug_handler(cmdev);
}

static int exynos_cm_die_handler(struct notifier_block *nb, unsigned long l, void *buf)
{
	struct cm_device *cmdev = container_of(nb, struct cm_device, die_nb);

	pr_debug("%s called\n", __func__);

	return exynos_cm_debug_handler(cmdev);
}

static int exynos_cm_probe(struct platform_device *pdev)
{
	struct cm_device *cmdev = NULL;
	struct page *page_ptr;
	struct device_node *np = NULL;
	int ret;
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	unsigned long log_base, log_size;
#endif
	/* check if SSS HW_SEMA is enabled */
	ret = exynos_cm_get_hw_sema_enabled(&flag_enable_hw_sema);
	if (ret == CM_SSS_HWSEMA_NOT_SUPPORT || ret == CM_INVALID_SMC_CMD) {
		pr_info("%s: SSS HW_SEMA is not support.\n", __func__);
		flag_enable_hw_sema = 0;
		return 0;
	} else if (ret != 0) {
		pr_err("%s: get status of SSS HW_SEMA is failed.\n", __func__);
		flag_enable_hw_sema = 0;
		return ret;
	} else if (flag_enable_hw_sema == 0) {
		pr_info("%s: SSS HW_SEMA is not enabled.\n", __func__);
		return 0;
	}

	/* set flag */
	flag_enable_hw_sema = CM_TRUE;
	flag_check_timestamp = 0;

	cmdev = devm_kzalloc(&pdev->dev, sizeof(*cmdev), GFP_KERNEL);
	if (!cmdev) {
		pr_err("%s: fail to devm_zalloc()\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, cmdev);

	/* initialize cmdev struct */
	cmdev->dev = &pdev->dev;
	cmdev->cm_debug_mem_addr = 0;
	cmdev->cm_debug_mem_va_addr = 0;
	cmdev->itmon_notified = 0;
	cmdev->panic_notified = 0;

	/* set cm_sss_debug_mem */
	page_ptr = alloc_pages(GFP_KERNEL, 1);
	if (!page_ptr) {
		pr_err("%s: alloc_page for cm_sss_debug failed.\n", __func__);
		return 0;
	}

	debug_mem = (struct cm_sss_debug_mem *)page_address(page_ptr);
	if (!debug_mem) {
		pr_err("%s: kmalloc for cm_sss_debug failed.\n", __func__);
		ret = -ENOMEM;
		return ret;
	}

	/* to map & flush memory */
	memset(debug_mem, 0x00, CM_SSS_DEBUG_MEM_SIZE);
	memcpy(debug_mem->magic, cm_name, sizeof(cm_name));
	flush_dcache_page((struct page *)debug_mem);
	cmdev->sss_debug_mem_pa_addr = (unsigned long)virt_to_phys(debug_mem);
	pr_info("%s: debug memory address: 0x%lx\n", __func__, cmdev->sss_debug_mem_pa_addr);

	/* get information from device tree */
	np = of_find_compatible_node(NULL, NULL, "samsung,exynos-cm");
	if (np == NULL) {
		pr_err("%s: Do not support exynos-cm\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "sysreg_addr", &cmdev->sss_debug_sysreg_addr);
	if (ret) {
		pr_err("%s: Fail to get debug_sysreg_addr from device tree\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "ap_addr", &cmdev->sss_ap_sfr_addr);
	if (ret) {
		pr_err("%s: Fail to get ap_addr from device tree\n", __func__);
		return ret;
	}

	ret = of_property_read_u32(np, "cp_addr", &cmdev->sss_cp_sfr_addr);
	if (ret) {
		pr_err("%s: Fail to get cp_addr from device tree\n", __func__);
		return ret;
	}

	/* get address & size for debugging to CryptoManager */
	ret = exynos_cm_get_debug_info(&cmdev->cm_debug_mem_addr,
				       &cmdev->cm_debug_mem_size);
	if (ret != 0) {
		pr_err("%s: getting debug information is failed.\n", __func__);
		return ret;
	}
	pr_info("%s: cm_debug_mem_addr: 0x%lx, size: 0x%x\n", __func__,
		cmdev->cm_debug_mem_addr, cmdev->cm_debug_mem_size);

	/* get VA addresses */
	if (cmdev->sss_debug_sysreg_addr)
		exynos_cm_map_sfr(cmdev->sss_debug_sysreg_addr, &sysreg_va_base, SZ_32);

	if (cmdev->cm_debug_mem_addr)
		cmdev->cm_debug_mem_va_addr = (u64)phys_to_virt(cmdev->cm_debug_mem_addr);

	/* register notifiers */
	cmdev->panic_nb.notifier_call = exynos_cm_panic_handler;
	atomic_notifier_chain_register(&panic_notifier_list, &cmdev->panic_nb);

	cmdev->die_nb.notifier_call = exynos_cm_die_handler;
	register_die_notifier(&cmdev->die_nb);
#if IS_ENABLED(CONFIG_EXYNOS_SEH)
	exynos_seh_set_cm_debug_function((unsigned long)exynos_cm_set_sss_debug_mem);
#endif
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	cmdev->itmon_nb.notifier_call = __exynos_cm_itmon_notifier;
	itmon_notifier_chain_register(&cmdev->itmon_nb);
#endif
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	log_base = (unsigned long)cmdev->sss_debug_mem_pa_addr;
	log_size = CM_SSS_DEBUG_MEM_SIZE;

	if (log_base != 0) {
		ret = dbg_snapshot_add_bl_item_info(CM_LOG_DSS_NAME1,
				log_base, log_size);
		if (ret) {
			dev_err(&pdev->dev, "Fail to add CM log dump - ret[%d]\n", ret);
		}
	}

	log_base = (unsigned long)cmdev->cm_debug_mem_addr;
	log_size = cmdev->cm_debug_mem_size;

	if (log_base != 0 && log_size != 0) {
		ret = dbg_snapshot_add_bl_item_info(CM_LOG_DSS_NAME2,
				log_base, log_size);
		if (ret) {
			dev_err(&pdev->dev, "Fail to add CM log dump - ret[%d]\n", ret);
		}
	}
#endif
	pr_info("%s: exynos-cm registered.\n", __func__);

	return 0;
}

static int exynos_cm_remove(struct platform_device *pdev)
{
	/* Do nothing */

	return 0;
}

static const struct of_device_id exynos_cm_of_match_table[] = {
	{ .compatible = "samsung,exynos-cm", },
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_cm_of_match_table);

static struct platform_driver exynos_cm_driver = {
	.probe		= exynos_cm_probe,
	.remove		= exynos_cm_remove,
	.driver		= {
		.name	= "exynos-cm",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_cm_of_match_table),
	},
};

static int __init exynos_cm_init(void)
{
	int ret;

	ret = platform_driver_register(&exynos_cm_driver);
	if (ret)
		return ret;

	pr_info("%s: Exynos CM driver init done. \n", __func__);
	pr_info("%s: Exynos CM driver, (c) 2021 Samsung Electronics\n", __func__);

	return 0;
}

static void __exit exynos_cm_exit(void)
{
	platform_driver_unregister(&exynos_cm_driver);
}

module_init(exynos_cm_init);
module_exit(exynos_cm_exit);

MODULE_SOFTDEP("pre: exynos-seh");
MODULE_DESCRIPTION("Exynos CryptoManager driver");
MODULE_AUTHOR("<jiye.min@samsung.com>");
MODULE_LICENSE("GPL");

