/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver Exynos specific code
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *	 Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */
#ifndef _CHUB_EXYNOS_H_
#define _CHUB_EXYNOS_H_

#ifdef CONFIG_SENSOR_DRV
#include "main.h"
#endif
#include "comms.h"
#include "chub.h"
#include "ipc_chub.h"
#include "chub_dbg.h"

#ifdef CONFIG_SOC_S5E8825
#include "s5e8825.h"
#endif
#ifdef CONFIG_SOC_S5E9925
#include "s5e9925.h"
#endif

#define REG_CHUB_CPU_DURATION			(0x8)
#define REG_CHUB_CPU_STATUS_BIT_STANDBYWFI	(28)

int contexthub_blk_poweron(struct contexthub_ipc_info *chub);
int contexthub_soc_poweron(struct contexthub_ipc_info *chub);
void contexthub_disable_pin(struct contexthub_ipc_info *chub);
int contexthub_get_qch_base(struct contexthub_ipc_info *chub);
int contexthub_set_clk(struct contexthub_ipc_info *chub);
int contexthub_get_clock_names(struct contexthub_ipc_info *chub);
int contexthub_core_reset(struct contexthub_ipc_info *chub);
void contexthub_upmu_init(struct contexthub_ipc_info *chub);
#endif /* _CHUB_EXYNOS_H_ */
