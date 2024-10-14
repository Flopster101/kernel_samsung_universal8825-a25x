/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#ifndef EXYNOS_CM_H
#define EXYNOS_CM_H

/******************************************************************************/
/* Common defines                                                             */
/******************************************************************************/
#define CM_MAGIC_STRING_LEN		(16)
#define CM_SSS_DEBUG_MEM_SIZE		(PAGE_SIZE)

uint8_t sss_time_magic[CM_MAGIC_STRING_LEN] = "SSSDEBUGTIME";
uint8_t cm_name[CM_MAGIC_STRING_LEN] = "CryptoManagerV60";

/******************************************************************************/
/* defines for log dump                                                       */
/******************************************************************************/
/* log dump name*/
#define CM_LOG_DSS_NAME1		"log_cm_1"
#define CM_LOG_DSS_NAME2		"log_cm_2"

/******************************************************************************/
/* struct defines for cm_sss_debug_mem                                        */
/******************************************************************************/
struct cm_sss_debug_info {
	uint8_t magic[CM_MAGIC_STRING_LEN];
	uint32_t dbg_req;
	uint32_t dbg_ack;
	uint32_t dbg_ack_int_stat;
	uint32_t dbg_to_int_stat;
	uint32_t dbg_to_history;
	uint32_t dbg_ack_history;
	uint64_t dbg_req_cancel_cnt;
	uint64_t dbg_req_period_cnt;
	uint64_t dbg_to_cnt;
	uint64_t reserved[8];
};

struct cm_sss_sema_state_info {
	uint64_t arbiter_state;
	uint64_t core_state_ap;
	uint64_t core_state_cp;
	uint64_t az_fsm_state;
	uint64_t az_req_ap;
	uint64_t az_req_cp;
	uint8_t reserved[16];    /* for 16byte align */
};

struct cm_sss_debug_mem {
	uint8_t magic[CM_MAGIC_STRING_LEN];
	struct cm_sss_debug_info ap_info;
	struct cm_sss_debug_info cp_info;
	struct cm_sss_sema_state_info st_info;
};

/******************************************************************************/
/* Define SYSREG regarding HW_SEMA                                            */
/******************************************************************************/
/* Offset of System Register for debugging */
#define SYSREG_OFFSET_DBG_STATE			(0x4)
#define SYSREG_OFFSET_DBG_REQ_CANCEL_CNT_AP	(0x8)
#define SYSREG_OFFSET_DBG_REQ_CANCEL_CNT_CP	(0xC)
#define SYSREG_OFFSET_DBG_REQ_PERIOD_CNT_AP	(0x10)
#define SYSREG_OFFSET_DBG_REQ_PERIOD_CNT_CP	(0x14)
#define SYSREG_OFFSET_DBG_TO_CNT_AP		(0x18) /* timeout */
#define SYSREG_OFFSET_DBG_TO_CNT_CP		(0x1C) /* timeout */

/* HW_SEMA_MEC_DBG */
#define DBG_REQ_AP_SHIFT			(0)
#define DBG_REQ_CP_SHIFT			(1)
#define DBG_ACK_AP_SHIFT			(2)
#define DBG_ACK_CP_SHIFT			(3)
#define DBG_ACK_INT_STAT_AP_SHIFT		(4)
#define DBG_TO_INT_STAT_AP_SHIFT		(5)
#define DBG_ACK_INT_STAT_CP_SHIFT		(6)
#define DBG_TO_INT_STAT_CP_SHIFT		(7)
#define DBG_TO_HISTORY_AP_SHIFT			(12)
#define DBG_ACK_HISTORY_AP_SHIFT		(13)
#define DBG_TO_HISTORY_CP_SHIFT			(14)
#define DBG_ACK_HISTORY_CP_SHIFT		(15)

#define HW_SEMA_MEC_DBG_REQ_MASK		(3 << 0)
#define HW_SEMA_MEC_DBG_REQ_AP			(1 << DBG_REQ_AP_SHIFT)
#define HW_SEMA_MEC_DBG_REQ_CP			(1 << DBG_REQ_CP_SHIFT)
#define HW_SEMA_MEC_DBG_ACK_MASK		(3 << 2)
#define HW_SEMA_MEC_DBG_ACK_AP			(1 << DBG_ACK_AP_SHIFT)
#define HW_SEMA_MEC_DBG_ACK_CP			(1 << DBG_ACK_CP_SHIFT)
#define HW_SEMA_MEC_DBG_INT_STATUS_MASK		(0xF << 4)
#define HW_SEMA_MEC_DBG_ACK_INT_STATUS_AP	(1 << DBG_ACK_INT_STAT_AP_SHIFT)
#define HW_SEMA_MEC_DBG_TO_INT_STATUS_AP	(1 << DBG_TO_INT_STAT_AP_SHIFT)
#define HW_SEMA_MEC_DBG_ACK_INT_STATUS_CP	(1 << DBG_ACK_INT_STAT_CP_SHIFT)
#define HW_SEMA_MEC_DBG_TO_INT_STATUS_CP	(1 << DBG_TO_INT_STAT_CP_SHIFT)
#define HW_SEMA_MEC_DBG_INT_HISTORY_MASK	(0xF << 12)
#define HW_SEMA_MEC_DBG_TO_HISTORY_AP		(1 << DBG_TO_HISTORY_AP_SHIFT)
#define HW_SEMA_MEC_DBG_ACK_HISTORY_AP		(1 << DBG_ACK_HISTORY_AP_SHIFT)
#define HW_SEMA_MEC_DBG_TO_HISTORY_CP		(1 << DBG_TO_HISTORY_CP_SHIFT)
#define HW_SEMA_MEC_DBG_ACK_HISTORY_CP		(1 << DBG_ACK_HISTORY_CP_SHIFT)

/* HW_SEMA_MEC_DBG_STATE */
#define DBQ_ARBITER_FSM_STAT_SHIFT		(0)
#define DBG_CORE_FSM_STAT_AP_SHIFT		(4)
#define DBG_CORE_FSM_STAT_CP_SHIFT		(10)
#define DBG_AZ_FSM_STAT_SHIFT			(16)
#define DBG_AZ_REQ_AP_SHIFT			(29)
#define DBG_AZ_REQ_CP_SHIFT			(30)

#define HW_SEMA_MEC_DBG_ARBITER_STAT_MASK	(0xF << 0)
#define HW_SEMA_MEC_DBQ_ARBITER_FSM_STAT	(0xF << DBQ_ARBITER_FSM_STAT_SHIFT)
#define HW_SEMA_MEC_DBG_CORE_STAT_MASK		(0xFFF << 4)
#define HW_SEMA_MEC_DBG_CORE_FSM_STAT_AP	(0x3F << DBG_CORE_FSM_STAT_AP_SHIFT)
#define HW_SEMA_MEC_DBG_CORE_FSM_STAT_CP	(0x3F << DBG_CORE_FSM_STAT_CP_SHIFT)
#define HW_SEMA_MEC_DBG_AZ_STAT_MASK		(0x7FFF << 16)
#define HW_SEMA_MEC_DBG_AZ_FSM_STAT		(0x1FFF << DBG_AZ_FSM_STAT_SHIFT)
#define HW_SEMA_MEC_DBG_AZ_REQ_AP		(1 << DBG_AZ_REQ_AP_SHIFT)
#define HW_SEMA_MEC_DBG_AZ_REQ_CP		(1 << DBG_AZ_REQ_CP_SHIFT)

/* State define of DBG_ARBITER_STATE */
#define ARBITER_ST_IDLE				(1 << 0)
#define ARBITER_ST_INIT				(1 << 1)
#define ARBITER_ST_CHECK			(1 << 2)
#define ARBITER_ST_STAY				(1 << 3)

/* State define of DBG_CORE_STATE */
#define CORE_FSM_ST_IDLE                        (1 << 0)
#define CORE_FSM_ST_INIT                        (1 << 1)
#define CORE_FSM_ST_WAIT_SEMA                   (1 << 2)
#define CORE_FSM_ST_DECODE                      (1 << 3)
#define CORE_FSM_ST_DATA                        (1 << 4)
#define CORE_FSM_ST_DONE                        (1 << 5)

/* State define of DBG_AZ_STATE */
#define AZ_FSM_ST_IDLE                          (0 << 0)
#define AZ_FSM_ST_INIT                          (1 << 0)
#define AZ_FSM_ST_END                           (1 << 1)
#define AZ_FSM_ST_INT_SET                       (1 << 2)
#define AZ_FSM_ST_IP_SEL                        (1 << 3)
#define AZ_FSM_ST_START                         (1 << 4)
#define AZ_FSM_ST_WAIT                          (1 << 5)
#define AZ_FSM_ST_DONE                          (1 << 6)
#define AZ_FSM_ST_ERROR                         (1 << 7)
#define AZ_FSM_ST_SW_RESET                      (1 << 8)
#define AZ_FSM_ST_SW_RESET_WAIT                 (1 << 9)
#define AZ_FSM_ST_SW_RESET_CNT_INIT             (1 << 10)
#define AZ_FSM_ST_PKE_INT_SET                   (1 << 11)
#define AZ_FSM_ST_PKE_SEL                       (1 << 12)

/* counter definition for debugging */
#define HW_SEMA_MEC_DBG_REQ_CANCLE_CNT_AP_MASK	(0xFFFFFFFF)
#define HW_SEMA_MEC_DBG_REQ_CANCLE_CNT_CP_MASK	(0xFFFFFFFF)
#define HW_SEMA_MEC_DBG_REQ_PERIOD_CNT_AP_MASK	(0xFFFFFFFF)
#define HW_SEMA_MEC_DBG_REQ_PERIOD_CNT_CP_MASK	(0xFFFFFFFF)
#define HW_SEMA_MEC_DBG_TO_CNT_AP_MASK		(0xFFFFFFFF)
#define HW_SEMA_MEC_DBG_TO_CNT_CP_MASK		(0xFFFFFFFF)

#endif
