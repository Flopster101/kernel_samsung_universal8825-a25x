/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IPC Driver AP side
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *
 */

#ifndef _MAILBOX_CHUB_AP_IPC_H
#define _MAILBOX_CHUB_AP_IPC_H

#define AP_IPC

#define IPC_VERSION (20200831)

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#include "comms.h"
#include "ipc_common.h"

#define DEBUG_LEVEL (KERN_ERR)

struct runtimelog_buf {
	char *buffer;
	unsigned int buffer_size;
	unsigned int write_index;
	int loglevel; /* enum ipc_fw_loglevel */
	wait_queue_head_t wq;
	bool wrap;
	bool updated;
};

enum DfsGovernor {
	DFS_GOVERNOR_OFF,
	DFS_GOVERNOR_SIMPLE,
	DFS_GOVERNOR_POWER,
	DFS_GVERNOR_MAX,
};

#define chub_cipc_init(owner, sram_base, func) cipc_init(owner, sram_base, func)

int ipc_logbuf_outprint(void *chub_p);
bool ipc_logbuf_filled(void);
void ipc_logbuf_flush_on(bool on);
u32 ipc_get_chub_mem_size(void);
void ipc_set_chub_clk(u32 clk);
void ipc_reset_map(void);
u8 ipc_get_sensor_info(u8 type);
#endif
