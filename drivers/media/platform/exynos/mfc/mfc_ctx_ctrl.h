/*
 * drivers/media/platform/exynos/mfc/mfc_ctx_ctrl.h
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_CTX_CTRL_H
#define __MFC_CTX_CTRL_H __FILE__

#include "base/mfc_common.h"

void mfc_ctrl_cleanup_ctx(struct mfc_ctx *ctx);
int mfc_ctrl_init_ctx(struct mfc_ctx *ctx, struct mfc_ctrl_cfg *ctrl_list, int count);
void mfc_ctrl_reset_buf(struct list_head *head);
int mfc_ctrl_cleanup_buf(struct mfc_ctx *ctx, enum mfc_ctrl_type type, unsigned int index);
int mfc_ctrl_init_buf(struct mfc_ctx *ctx, struct mfc_ctrl_cfg *ctrl_list,
		enum mfc_ctrl_type type, unsigned int index, int count);
void mfc_ctrl_to_buf(struct mfc_ctx *ctx, struct list_head *head);
void mfc_ctrl_to_ctx(struct mfc_ctx *ctx, struct list_head *head);
int mfc_ctrl_get_buf_val(struct mfc_ctx *ctx, struct list_head *head, unsigned int id);
void mfc_ctrl_update_buf_val(struct mfc_ctx *ctx, struct list_head *head,
		unsigned int id, int value);

#endif /* __MFC_CTX_CTRL_H */
