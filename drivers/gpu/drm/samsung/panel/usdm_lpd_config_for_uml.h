/* soc/samsung/exynos-lpd.h
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LPD_CONFIG_FOR_UML_H__
#define __LPD_CONFIG_FOR_UML_H__

#include <linux/notifier.h>

#define CMD_SEQ_MAGIC_CODE1 0xacc12345
#define CMD_SEQ_MAGIC_CODE2 0xacc23456
#define CMD_SEQ_MAGIC_OFFSET1 0
#define CMD_SEQ_MAGIC_OFFSET2 1
#define MAX_AUTO_BR_CNT 20

struct lpd_br_info {
	unsigned int br_cnt;
	unsigned int br_list[MAX_AUTO_BR_CNT];
	unsigned int nit_list[MAX_AUTO_BR_CNT];
};

struct lpd_cmd_buf {
	void *buf;
	unsigned int buf_size;
};

struct lpd_panel_cmd {
	struct lpd_br_info br_info;
	struct lpd_cmd_buf cmd_buf;
};

enum lpd_config_action {
	LPD_CONFIG_BR_CMD = 0,
	LPD_CONFIG_HBM_BR_CMD,
	LPD_CONFIG_INIT_CMD
};

#if IS_ENABLED(CONFIG_LPD_AUTO_BR)
extern int lpd_config_notifier_register(struct notifier_block *nb);
extern int lpd_config_notifier_unregister(struct notifier_block *nb);
extern int lpd_config_notifier_call(u32 action, void *data);
#else
static inline int lpd_config_notifier_register(struct notifier_block *nb) { return 0; }
static inline int lpd_config_notifier_unregister(struct notifier_block *nb) { return 0; }
static inline int lpd_config_notifier_call(u32 action, void *data) { return 0; }
#endif

#endif /* __LPD_CONFIG_FOR_UML_H__ */
