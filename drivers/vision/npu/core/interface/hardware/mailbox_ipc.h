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

#ifndef _MAILBOX_IPC_
#define _MAILBOX_IPC_

#include <linux/types.h>
#include <linux/io.h>
#include "mailbox.h"
#include "npu-common.h"
#include "npu-config.h"
#include "../../npu-log.h"
#include "../../npu-device.h"
#include "mailbox_msg.h"
#include "../../npu-session.h"

#define EPARAM		41
#define ERESOURCE	43
#define EALIGN		44
#define E_RESP_BASE				0x100

enum err_cmnd_response {
	ERR_NO_ERR                       = 0,
	ERR_TIMEOUT                      = (E_RESP_BASE + 1),
	ERR_TOO_BIG_MSG_ID               = (E_RESP_BASE + 2),
	ERR_MSG_ID_NOT_FREE              = (E_RESP_BASE + 3),
	ERR_TOO_BIG_CMND_ID              = (E_RESP_BASE + 4),
	ERR_CMND_ID_NO_PROC_FUNC         = (E_RESP_BASE + 5),
	ERR_TOO_BIG_OBJ_ID               = (E_RESP_BASE + 6),
	ERR_CMND_NOT_ALLOWED_FOR_STATE   = (E_RESP_BASE + 7),
	ERR_LOAD_TOO_BIG_TASK_ID         = (E_RESP_BASE + 8),
	ERR_LOAD_NULL_PAYLOAD            = (E_RESP_BASE + 9),
	ERR_LOAD_LENGTH_0                = (E_RESP_BASE + 10),
	ERR_LOAD_CMD_LENGTH_TOO_BIG      = (E_RESP_BASE + 11),
	ERR_LOAD_CANT_ALLOC_CMD_LENGTH   = (E_RESP_BASE + 12),
	ERR_LOAD_WRONG_MAGIC_1           = (E_RESP_BASE + 13),
	ERR_LOAD_WRONG_MAGIC_2           = (E_RESP_BASE + 14),
	ERR_LOAD_WRONG_VERSION           = (E_RESP_BASE + 15),
	ERR_LOAD_ALREADY_REGISTERED      = (E_RESP_BASE + 16),
	ERR_LOAD_CHUNK_SIZE_TOO_BIG      = (E_RESP_BASE + 17),
	ERR_LOAD_SEQ_ALLOC               = (E_RESP_BASE + 18),
	ERR_LOAD_SEQ_WRONG_GRP_CNT       = (E_RESP_BASE + 19),
	ERR_LOAD_SEQ_WRONG_CHUNK_CNT     = (E_RESP_BASE + 20),
	ERR_LOAD_INTR_OFS_NOT_ALIGNED   = (E_RESP_BASE + 21),
	ERR_LOAD_CMDQ_OFS_NOT_ALIGNED   = (E_RESP_BASE + 22),
	ERR_LOAD_CMDQ_SIZE_NOT_16_MULT   = (E_RESP_BASE + 23),
	ERR_LOAD_NO_AVAIL_TASK           = (E_RESP_BASE + 24),
	ERR_LOAD_EXCESS_MVECTOR          = (E_RESP_BASE + 25),
	ERR_LOAD_UNALIGNED_MVECTOR       = (E_RESP_BASE + 26),
	ERR_LOAD_INVALID_MADDR           = (E_RESP_BASE + 27),
	ERR_LOAD_INVALID_PAYLOAD         = (E_RESP_BASE + 28),

	ERR_LOAD_INVALID_NCP             = (E_RESP_BASE + 40),

	ERR_UNLOAD_NOT_EMPTY_FIFO        = (E_RESP_BASE + 50),
	ERR_UNLOAD_ALREADY_UNREGISTERED  = (E_RESP_BASE + 51),

	ERR_PROC_NULL_PAYLOAD            = (E_RESP_BASE + 60),
	ERR_PROC_BAD_FID                 = (E_RESP_BASE + 61),
	ERR_PROC_CMD_LENGTH_TOO_BIG      = (E_RESP_BASE + 62),
	ERR_PROC_CMD_INVALID_VEC_IND     = (E_RESP_BASE + 63),
	ERR_PROC_CMD_FIFO_FULL           = (E_RESP_BASE + 64),
	ERR_PROC_CMD_OBJ_PURGED          = (E_RESP_BASE + 65),
	ERR_PROC_PUT_SCHED_FAIL          = (E_RESP_BASE + 66),
	ERR_PROC_PREPARE_FAIL            = (E_RESP_BASE + 67),
	ERR_PROC_INVALID_MADDR           = (E_RESP_BASE + 68),
	ERR_PROC_INVALID_PAYLOAD         = (E_RESP_BASE + 69),

	ERR_PURGE_WAIT_FAIL              = (E_RESP_BASE + 70),
	ERR_POWERDOWN_LOADED_PROCESS     = (E_RESP_BASE + 71),
	ERR_PROFILE_NOTIFY               = (E_RESP_BASE + 72),
	ERR_UT_EXCEED_UT_IDX_MAX         = (E_RESP_BASE + 73),
	ERR_INVALID_POLICY               = (E_RESP_BASE + 74),
	ERR_CANT_CHANGE_POLICY           = (E_RESP_BASE + 75),
	ERR_CORE_CTL                     = (E_RESP_BASE + 76),
	ERR_IM_BUF_ADDR                  = (E_RESP_BASE + 77),
	ERR_TIMEOUT_RECOVERED            = (E_RESP_BASE + 78),

	ERR_PRELOAD_INVALID_ADDR         = (E_RESP_BASE + 80),
	ERR_PRELOAD_INVALID_SIZE         = (E_RESP_BASE + 81),
	ERR_PRELOAD_INVALID_OID          = (E_RESP_BASE + 82),
	ERR_PRELOAD_INVALID_RESP         = (E_RESP_BASE + 83),
	ERR_PRELOAD_NUM_INPUTS           = (E_RESP_BASE + 84),
	ERR_PRELOAD_NUM_OUTPUTS          = (E_RESP_BASE + 85),
};

void *mbx_ipc_translate(char *underlay, volatile struct mailbox_ctrl *ctrl, u32 ptr);
void mbx_ipc_print(char *underlay, volatile struct mailbox_ctrl *ctrl, u32 log_level);
void mbx_ipc_print_dbg(char *underlay, volatile struct mailbox_ctrl *ctrl);
int mbx_ipc_put(char *underlay, volatile struct mailbox_ctrl *ctrl, struct message *msg, struct command *cmd);
int mbx_ipc_peek_msg(char *underlay, volatile struct mailbox_ctrl *ctrl, struct message *msg);
int mbx_ipc_get_msg(char *underlay, volatile struct mailbox_ctrl *ctrl, struct message *msg);
int mbx_ipc_get_cmd(char *underlay, volatile struct mailbox_ctrl *ctrl, struct message *msg, struct command *cmd);
int mbx_ipc_clr_msg(char *underlay, volatile struct mailbox_ctrl *ctrl, struct message *msg);
int mailbox_init(volatile struct mailbox_hdr *header, struct npu_system *system);
void mailbox_deinit(volatile struct mailbox_hdr *header, struct npu_system *system);
void dbg_print_hdr(volatile struct mailbox_hdr *mHdr);
void dbg_print_msg(struct message *msg, struct command *cmd);
void dbg_print_ctrl(volatile struct mailbox_ctrl *mCtrl);
int get_message_id(void);
void pop_message_id(int msg_id);

#endif
