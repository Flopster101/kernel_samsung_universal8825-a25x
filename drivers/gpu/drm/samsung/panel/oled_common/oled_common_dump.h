/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OLED_COMMON_DUMP_H__
#define __OLED_COMMON_DUMP_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define OLED_ERR_FG_LEN (1)
#define OLED_DSI_ERR_LEN	 (1)
#define OLED_SELF_DIAG_LEN (1)
#define OLED_SELF_DIAG_MASK (0x40)
#define OLED_SELF_DIAG_VALUE (0x40)
#define OLED_SELF_MASK_CRC_LEN (4)

int snprintf_dump_expects(char *buf, size_t size, struct dumpinfo *dump);
int oled_dump_show_expects(struct dumpinfo *dump);
int oled_dump_show_resource(struct dumpinfo *dump);
int oled_dump_show_resource_and_panic(struct dumpinfo *dump);
int oled_dump_show_rddpm(struct dumpinfo *info);
int oled_dump_show_rddpm_before_sleep_in(struct dumpinfo *info);
int oled_dump_show_rddsm(struct dumpinfo *info);
int oled_dump_show_err(struct dumpinfo *info);
int oled_dump_show_err_fg(struct dumpinfo *info);
int oled_dump_show_dsi_err(struct dumpinfo *info);
int oled_dump_show_self_diag(struct dumpinfo *info);
#ifdef CONFIG_USDM_DDI_CMDLOG
int oled_dump_show_cmdlog(struct dumpinfo *info);
#endif
int oled_dump_show_ssr_err(struct dumpinfo *info);
int oled_dump_show_ecc_err(struct dumpinfo *info);
int oled_dump_show_flash_loaded(struct dumpinfo *info);
#ifdef CONFIG_USDM_PANEL_MAFPC
int oled_dump_show_mafpc_log(struct dumpinfo *info);
int oled_dump_show_mafpc_flash_log(struct dumpinfo *info);
int oled_dump_show_abc_crc_log(struct dumpinfo *info);
#endif
int oled_dump_show_self_mask_crc(struct dumpinfo *info);
#endif /* __OLED_COMMON_DUMP_H__ */
