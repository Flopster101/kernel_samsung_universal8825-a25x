/*
 * drivers/media/platform/exynos/mfc/mfc_core_buf_ctrl.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_core_reg_api.h"

#include  "base/mfc_common.h"

static void __mfc_enc_store_buf_ctrls_temporal_svc(int id, struct mfc_enc_params *p,
		struct temporal_layer_info *temporal_LC)
{
	unsigned int num_layer = temporal_LC->temporal_layer_count;
	int i;

	switch (id) {
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH:
		p->codec.h264.num_hier_layer = num_layer & 0x7;
		for (i = 0; i < (num_layer & 0x7); i++)
			p->codec.h264.hier_bit_layer[i] =
				temporal_LC->temporal_layer_bitrate[i];
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_CH:
		p->codec.hevc.num_hier_layer = num_layer & 0x7;
		for (i = 0; i < (num_layer & 0x7); i++)
			p->codec.hevc.hier_bit_layer[i] =
				temporal_LC->temporal_layer_bitrate[i];
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH:
		p->codec.vp8.num_hier_layer = num_layer & 0x7;
		for (i = 0; i < (num_layer & 0x7); i++)
			p->codec.vp8.hier_bit_layer[i] =
				temporal_LC->temporal_layer_bitrate[i];
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_CH:
		p->codec.vp9.num_hier_layer = num_layer & 0x7;
		for (i = 0; i < (num_layer & 0x7); i++)
			p->codec.vp9.hier_bit_layer[i] =
				temporal_LC->temporal_layer_bitrate[i];
		break;
	default:
		break;
	}
}

static void __mfc_core_enc_set_buf_ctrls_temporal_svc(struct mfc_core *core,
		struct mfc_ctx *ctx, struct mfc_buf_ctrl *buf_ctrl)
{
	struct mfc_enc *enc = ctx->enc_priv;
	unsigned int value = 0, value2 = 0;
	struct temporal_layer_info temporal_LC;
	unsigned int i;
	struct mfc_enc_params *p = &enc->params;

	if (buf_ctrl->id
		== V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH ||
		buf_ctrl->id
		== V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH ||
		buf_ctrl->id
		== V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_CH ||
		buf_ctrl->id
		== V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_CH) {
		memcpy(&temporal_LC,
			enc->sh_handle_svc.vaddr, sizeof(struct temporal_layer_info));

		/* Store temporal layer information */
		__mfc_enc_store_buf_ctrls_temporal_svc(buf_ctrl->id, p,
				&temporal_LC);

		if (((temporal_LC.temporal_layer_count & 0x7) < 1) ||
			((temporal_LC.temporal_layer_count > 3) && IS_VP8_ENC(ctx)) ||
			((temporal_LC.temporal_layer_count > 3) && IS_VP9_ENC(ctx))) {
			/* clear NUM_T_LAYER_CHANGE */
			value = MFC_CORE_READL(buf_ctrl->flag_addr);
			value &= ~(1 << 10);
			MFC_CORE_WRITEL(value, buf_ctrl->flag_addr);
			mfc_ctx_err("[HIERARCHICAL] layer count is invalid : %d\n",
					temporal_LC.temporal_layer_count);
			return;
		}

		/* enable RC_BIT_RATE_CHANGE */
		value = MFC_CORE_READL(buf_ctrl->flag_addr);
		if (temporal_LC.temporal_layer_bitrate[0] > 0 || p->hier_bitrate_ctrl)
			/* set RC_BIT_RATE_CHANGE */
			value |= (1 << 2);
		else
			/* clear RC_BIT_RATE_CHANGE */
			value &= ~(1 << 2);
		MFC_CORE_WRITEL(value, buf_ctrl->flag_addr);

		mfc_debug(3, "[HIERARCHICAL] layer count %d, E_PARAM_CHANGE %#x\n",
				temporal_LC.temporal_layer_count & 0x7, value);

		value = MFC_CORE_READL(MFC_REG_E_NUM_T_LAYER);
		buf_ctrl->old_val2 = value;
		value &= ~(0x7);
		value |= (temporal_LC.temporal_layer_count & 0x7);
		value &= ~(0x1 << 8);
		value |= (p->hier_bitrate_ctrl & 0x1) << 8;
		MFC_CORE_WRITEL(value, MFC_REG_E_NUM_T_LAYER);
		mfc_debug(3, "[HIERARCHICAL] E_NUM_T_LAYER %#x\n", value);
		for (i = 0; i < (temporal_LC.temporal_layer_count & 0x7); i++) {
			mfc_debug(3, "[HIERARCHICAL] layer bitrate[%d] %d (FW ctrl: %d)\n",
					i, temporal_LC.temporal_layer_bitrate[i], p->hier_bitrate_ctrl);
			MFC_CORE_WRITEL(temporal_LC.temporal_layer_bitrate[i],
					buf_ctrl->addr + i * 4);
		}
		/* priority change */
		if (IS_H264_ENC(ctx)) {
			value = 0;
			value2 = 0;
			for (i = 0; i < (p->codec.h264.num_hier_layer & 0x07); i++) {
				if (i <= 4)
					value |= ((p->codec.h264.base_priority & 0x3F) + i)
						<< (6 * i);
				else
					value2 |= ((p->codec.h264.base_priority & 0x3F) + i)
						<< (6 * (i - 5));
			}
			MFC_CORE_WRITEL(value, MFC_REG_E_H264_HD_SVC_EXTENSION_0);
			MFC_CORE_WRITEL(value2, MFC_REG_E_H264_HD_SVC_EXTENSION_1);
			mfc_debug(3, "[HIERARCHICAL] EXTENSION0 %#x, EXTENSION1 %#x\n",
					value, value2);

			value = MFC_CORE_READL(buf_ctrl->flag_addr);
			value |= (1 << 12);
			MFC_CORE_WRITEL(value, buf_ctrl->flag_addr);
			mfc_debug(3, "[HIERARCHICAL] E_PARAM_CHANGE %#x\n", value);
		}

	}

	/* temproral layer priority */
	if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_BASE_PRIORITY) {
		value = MFC_CORE_READL(MFC_REG_E_H264_HD_SVC_EXTENSION_0);
		buf_ctrl->old_val |= value & 0x3FFFFFC0;
		value &= ~(0x3FFFFFC0);
		value2 = MFC_CORE_READL(MFC_REG_E_H264_HD_SVC_EXTENSION_1);
		buf_ctrl->old_val2 = value2 & 0x0FFF;
		value2 &= ~(0x0FFF);
		for (i = 0; i < (p->codec.h264.num_hier_layer & 0x07); i++) {
			if (i <= 4)
				value |= ((buf_ctrl->val & 0x3F) + i) << (6 * i);
			else
				value2 |= ((buf_ctrl->val & 0x3F) + i) << (6 * (i - 5));
		}
		MFC_CORE_WRITEL(value, MFC_REG_E_H264_HD_SVC_EXTENSION_0);
		MFC_CORE_WRITEL(value2, MFC_REG_E_H264_HD_SVC_EXTENSION_1);
		mfc_debug(3, "[HIERARCHICAL] EXTENSION0 %#x, EXTENSION1 %#x\n",
				value, value2);
	}
}

static void __mfc_core_enc_set_buf_ctrls_exception(struct mfc_core *core,
		struct mfc_ctx *ctx, struct mfc_buf_ctrl *buf_ctrl)
{
	struct mfc_enc *enc = ctx->enc_priv;
	struct mfc_enc_params *p = &enc->params;
	unsigned int value = 0;

	/* temporal layer setting */
	__mfc_core_enc_set_buf_ctrls_temporal_svc(core, ctx, buf_ctrl);

	if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_MARK_LTR) {
		value = MFC_CORE_READL(MFC_REG_E_H264_NAL_CONTROL);
		buf_ctrl->old_val2 = (value >> 8) & 0x7;
		value &= ~(0x7 << 8);
		value |= (buf_ctrl->val & 0x7) << 8;
		MFC_CORE_WRITEL(value, MFC_REG_E_H264_NAL_CONTROL);
	}
	if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_USE_LTR) {
		value = MFC_CORE_READL(MFC_REG_E_H264_NAL_CONTROL);
		buf_ctrl->old_val2 = (value >> 11) & 0xF;
		value &= ~(0xF << 11);
		value |= (buf_ctrl->val & 0xF) << 11;
		MFC_CORE_WRITEL(value, MFC_REG_E_H264_NAL_CONTROL);
	}

	if (buf_ctrl->id == V4L2_CID_MPEG_MFC51_VIDEO_I_PERIOD_CH) {
		value = MFC_CORE_READL(MFC_REG_E_GOP_CONFIG2);
		buf_ctrl->old_val |= (value << 16) & 0x3FFF0000;
		value &= ~(0x3FFF);
		value |= (buf_ctrl->val >> 16) & 0x3FFF;
		MFC_CORE_WRITEL(value, MFC_REG_E_GOP_CONFIG2);
	}

	/* PROFILE & LEVEL have to be set up together */
	if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_LEVEL) {
		value = MFC_CORE_READL(MFC_REG_E_PICTURE_PROFILE);
		buf_ctrl->old_val |= (value & 0x000F) << 8;
		value &= ~(0x000F);
		value |= p->codec.h264.profile & 0x000F;
		MFC_CORE_WRITEL(value, MFC_REG_E_PICTURE_PROFILE);
		p->codec.h264.level = buf_ctrl->val;
	}

	if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_PROFILE) {
		value = MFC_CORE_READL(MFC_REG_E_PICTURE_PROFILE);
		buf_ctrl->old_val |= value & 0xFF00;
		value &= ~(0x00FF << 8);
		value |= (p->codec.h264.level << 8) & 0xFF00;
		MFC_CORE_WRITEL(value, MFC_REG_E_PICTURE_PROFILE);
		p->codec.h264.profile = buf_ctrl->val;
	}

	/* set the ROI buffer DVA */
	if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_ROI_CONTROL) {
		MFC_CORE_WRITEL(enc->roi_buf[buf_ctrl->old_val2].daddr,
				MFC_REG_E_ROI_BUFFER_ADDR);
		mfc_debug(3, "[ROI] buffer[%d] addr %#llx, QP val: %#x\n",
				buf_ctrl->old_val2,
				enc->roi_buf[buf_ctrl->old_val2].daddr,
				buf_ctrl->val);
	}

	/* set frame rate change with delta */
	if (buf_ctrl->id == V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE_CH) {
		p->rc_frame_delta = p->rc_framerate_res / buf_ctrl->val;
		value = MFC_CORE_READL(buf_ctrl->addr);
		value &= ~(buf_ctrl->mask << buf_ctrl->shft);
		value |= ((p->rc_frame_delta & buf_ctrl->mask) << buf_ctrl->shft);
		MFC_CORE_WRITEL(value, buf_ctrl->addr);
	}

	/* set drop control */
	if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_DROP_CONTROL) {
		p->rc_frame_delta = mfc_enc_get_ts_delta(ctx);
		value = MFC_CORE_READL(MFC_REG_E_RC_FRAME_RATE);
		value &= ~(0xFFFF);
		value |= (p->rc_frame_delta & 0xFFFF);
		MFC_CORE_WRITEL(value, MFC_REG_E_RC_FRAME_RATE);
		if (ctx->src_ts.ts_last_interval)
			mfc_debug(3, "[DROPCTRL] fps %d -> %ld, delta: %d, reg: %#x\n",
				p->rc_framerate, USEC_PER_SEC / ctx->src_ts.ts_last_interval,
				p->rc_frame_delta, value);
		else
			mfc_debug(3, "[DROPCTRL] fps %d -> 0, delta: %d, reg: %#x\n",
				p->rc_framerate, p->rc_frame_delta, value);
	}

	/* store last config qp value in F/W */
	if (buf_ctrl->id == V4L2_CID_MPEG_MFC_CONFIG_QP)
		enc->config_qp = p->config_qp;
}

static int mfc_core_set_buf_ctrls(struct mfc_core *core,
		struct mfc_ctx *ctx, struct list_head *head)
{
	struct mfc_buf_ctrl *buf_ctrl;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET) || !buf_ctrl->has_new)
			continue;

		if (buf_ctrl->mode == MFC_CTRL_MODE_SFR) {
			/* read old vlaue */
			value = MFC_CORE_READL(buf_ctrl->addr);

			/* save old vlaue for recovery */
			if (buf_ctrl->is_volatile)
				buf_ctrl->old_val = (value >> buf_ctrl->shft) & buf_ctrl->mask;

			/* write new value */
			value &= ~(buf_ctrl->mask << buf_ctrl->shft);
			value |= ((buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft);
			MFC_CORE_WRITEL(value, buf_ctrl->addr);
		}

		/* set change flag bit */
		if (buf_ctrl->flag_mode == MFC_CTRL_MODE_SFR) {
			value = MFC_CORE_READL(buf_ctrl->flag_addr);
			value |= (1 << buf_ctrl->flag_shft);
			MFC_CORE_WRITEL(value, buf_ctrl->flag_addr);
		}

		buf_ctrl->has_new = 0;
		buf_ctrl->updated = 1;

		if (buf_ctrl->id == V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG)
			ctx->stored_tag = buf_ctrl->val;

		if (ctx->type == MFCINST_ENCODER)
			__mfc_core_enc_set_buf_ctrls_exception(core, ctx, buf_ctrl);

		mfc_debug(6, "[CTRLS] Set buffer control id: 0x%08x, val: %d (%#x)\n",
				buf_ctrl->id, buf_ctrl->val, buf_ctrl->val);
	}

	return 0;
}

static int mfc_core_get_buf_ctrls(struct mfc_core *core,
		struct mfc_ctx *ctx, struct list_head *head)
{
	struct mfc_buf_ctrl *buf_ctrl;
	struct mfc_dec *dec = ctx->dec_priv;
	struct mfc_enc *enc = ctx->enc_priv;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_GET))
			continue;

		if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
			value = MFC_CORE_READL(buf_ctrl->addr);

		value = (value >> buf_ctrl->shft) & buf_ctrl->mask;

		buf_ctrl->val = value;
		buf_ctrl->has_new = 1;

		if (IS_VP9_DEC(ctx) && dec) {
			if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_FULL_RANGE_FLAG)
				buf_ctrl->val = dec->color_range;
			else if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_COLOUR_PRIMARIES)
				buf_ctrl->val = dec->color_space;
		}

		if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_FRAME_ERROR_TYPE)
			buf_ctrl->val = mfc_get_frame_error_type(ctx, value);

		if (buf_ctrl->id == V4L2_CID_MPEG_MFC51_VIDEO_FRAME_STATUS)
			if (enc)
				buf_ctrl->val = !enc->in_slice;

		mfc_debug(6, "[CTRLS] Get buffer control id: 0x%08x, val: %d (%#x)\n",
				buf_ctrl->id, buf_ctrl->val, buf_ctrl->val);
	}

	return 0;
}

static void __mfc_core_enc_recover_buf_ctrls_exception(struct mfc_core *core,
		struct mfc_ctx *ctx, struct mfc_buf_ctrl *buf_ctrl)
{
	unsigned int value = 0;

	if (buf_ctrl->id == V4L2_CID_MPEG_MFC51_VIDEO_I_PERIOD_CH) {
		value = MFC_CORE_READL(MFC_REG_E_GOP_CONFIG2);
		value &= ~(0x3FFF);
		value |= (buf_ctrl->old_val >> 16) & 0x3FFF;
		MFC_CORE_WRITEL(value, MFC_REG_E_GOP_CONFIG2);
	}

	if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_LEVEL) {
		value = MFC_CORE_READL(MFC_REG_E_PICTURE_PROFILE);
		value &= ~(0x000F);
		value |= (buf_ctrl->old_val >> 8) & 0x000F;
		MFC_CORE_WRITEL(value, MFC_REG_E_PICTURE_PROFILE);
	}

	if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_PROFILE) {
		value = MFC_CORE_READL(MFC_REG_E_PICTURE_PROFILE);
		value &= ~(0xFF00);
		value |= buf_ctrl->old_val & 0xFF00;
		MFC_CORE_WRITEL(value, MFC_REG_E_PICTURE_PROFILE);
	}

	if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_BASE_PRIORITY) {
		MFC_CORE_WRITEL(buf_ctrl->old_val, MFC_REG_E_H264_HD_SVC_EXTENSION_0);
		MFC_CORE_WRITEL(buf_ctrl->old_val2, MFC_REG_E_H264_HD_SVC_EXTENSION_1);
	}

	if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH ||
		buf_ctrl->id == V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH ||
		buf_ctrl->id == V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_CH) {
		MFC_CORE_WRITEL(buf_ctrl->old_val2, MFC_REG_E_NUM_T_LAYER);
		/* clear RC_BIT_RATE_CHANGE */
		value = MFC_CORE_READL(buf_ctrl->flag_addr);
		value &= ~(1 << 2);
		MFC_CORE_WRITEL(value, buf_ctrl->flag_addr);
	}

	if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_MARK_LTR) {
		value = MFC_CORE_READL(MFC_REG_E_H264_NAL_CONTROL);
		value &= ~(0x7 << 8);
		value |= (buf_ctrl->old_val2 & 0x7) << 8;
		MFC_CORE_WRITEL(value, MFC_REG_E_H264_NAL_CONTROL);
	}

	if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_USE_LTR) {
		value = MFC_CORE_READL(MFC_REG_E_H264_NAL_CONTROL);
		value &= ~(0xF << 11);
		value |= (buf_ctrl->old_val2 & 0xF) << 11;
		MFC_CORE_WRITEL(value, MFC_REG_E_H264_NAL_CONTROL);
	}
}

static int mfc_core_recover_buf_ctrls(struct mfc_core *core,
		struct mfc_ctx *ctx, struct list_head *head)
{
	struct mfc_buf_ctrl *buf_ctrl;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET)
			|| !buf_ctrl->is_volatile
			|| !buf_ctrl->updated)
			continue;

		if (buf_ctrl->mode == MFC_CTRL_MODE_SFR) {
			value = MFC_CORE_READL(buf_ctrl->addr);
			value &= ~(buf_ctrl->mask << buf_ctrl->shft);
			value |= ((buf_ctrl->old_val & buf_ctrl->mask) << buf_ctrl->shft);
			MFC_CORE_WRITEL(value, buf_ctrl->addr);
		}

		/* clear change flag bit */
		if (buf_ctrl->flag_mode == MFC_CTRL_MODE_SFR) {
			value = MFC_CORE_READL(buf_ctrl->flag_addr);
			value &= ~(1 << buf_ctrl->flag_shft);
			MFC_CORE_WRITEL(value, buf_ctrl->flag_addr);
		}

		/* Exception */
		if (ctx->type == MFCINST_ENCODER)
			__mfc_core_enc_recover_buf_ctrls_exception(core, ctx, buf_ctrl);

		buf_ctrl->updated = 0;
		mfc_debug(6, "[CTRLS] Recover buffer control id: 0x%08x, old val: %d\n",
				buf_ctrl->id, buf_ctrl->old_val);
	}

	return 0;
}

static int mfc_core_set_buf_ctrls_nal_q_dec(struct mfc_ctx *ctx,
			struct list_head *head, DecoderInputStr *pInStr)
{
	struct mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET) || !buf_ctrl->has_new)
			continue;
		switch (buf_ctrl->id) {
		case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG:
			pInStr->PictureTag &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->PictureTag |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			ctx->stored_tag = buf_ctrl->val;
			break;
		/* If new dynamic controls are added, insert here */
		default:
			if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
				mfc_ctx_info("[NALQ] can't find control, id: 0x%x\n",
					buf_ctrl->id);
		}
		buf_ctrl->has_new = 0;
		buf_ctrl->updated = 1;

		mfc_debug(6, "[NALQ][CTRLS] Set buffer control id: 0x%08x, val: %d (%#x)\n",
				buf_ctrl->id, buf_ctrl->val, buf_ctrl->val);
	}

	return 0;
}

static int mfc_core_get_buf_ctrls_nal_q_dec(struct mfc_ctx *ctx,
			struct list_head *head, DecoderOutputStr *pOutStr)
{
	struct mfc_dec *dec = ctx->dec_priv;
	struct mfc_buf_ctrl *buf_ctrl;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_GET))
			continue;
		switch (buf_ctrl->id) {
		case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG:
			value = pOutStr->PictureTagTop;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_POC:
			value = pOutStr->PictureTimeTop;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_DISPLAY_STATUS:
			value = pOutStr->DisplayStatus;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_LUMA:
			value = pOutStr->DisplayFirstCrc;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_CHROMA:
			value = pOutStr->DisplaySecondCrc;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_CHROMA1:
			value = pOutStr->DisplayThirdCrc;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_2BIT_LUMA:
			value = pOutStr->DisplayFirst2BitCrc;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_2BIT_CHROMA:
			value = pOutStr->DisplaySecond2BitCrc;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_CRC_GENERATED:
			value = pOutStr->DisplayStatus;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_SEI_FP_AVAIL:
			value = pOutStr->SeiAvail;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_SEI_FP_ARRGMENT_ID:
			value = pOutStr->FramePackArrgmentId;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_SEI_FP_INFO:
			value = pOutStr->FramePackSeiInfo;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_SEI_FP_GRID_POS:
			value = pOutStr->FramePackGridPos;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_MVC_VIEW_ID:
			value = 0;
			break;
		/* TO-DO: SEI information has to be added in NAL-Q */
		case V4L2_CID_MPEG_VIDEO_SEI_MAX_PIC_AVERAGE_LIGHT:
		case V4L2_CID_MPEG_VIDEO_SEI_MAX_CONTENT_LIGHT:
			value = pOutStr->ContentLightLevelInfoSei;
			break;
		case V4L2_CID_MPEG_VIDEO_SEI_MAX_DISPLAY_LUMINANCE:
			value = pOutStr->MasteringDisplayColourVolumeSei0;
			break;
		case V4L2_CID_MPEG_VIDEO_SEI_MIN_DISPLAY_LUMINANCE:
			value = pOutStr->MasteringDisplayColourVolumeSei1;
			break;
		case V4L2_CID_MPEG_VIDEO_FULL_RANGE_FLAG:
		case V4L2_CID_MPEG_VIDEO_COLOUR_PRIMARIES:
		case V4L2_CID_MPEG_VIDEO_FORMAT:
		case V4L2_CID_MPEG_VIDEO_TRANSFER_CHARACTERISTICS:
		case V4L2_CID_MPEG_VIDEO_MATRIX_COEFFICIENTS:
			value = pOutStr->VideoSignalType;
			break;
		case V4L2_CID_MPEG_VIDEO_SEI_WHITE_POINT:
			value = pOutStr->MasteringDisplayColourVolumeSei2;
			break;
		case V4L2_CID_MPEG_VIDEO_SEI_DISPLAY_PRIMARIES_0:
			value = pOutStr->MasteringDisplayColourVolumeSei3;
			break;
		case V4L2_CID_MPEG_VIDEO_SEI_DISPLAY_PRIMARIES_1:
			value = pOutStr->MasteringDisplayColourVolumeSei4;
			break;
		case V4L2_CID_MPEG_VIDEO_SEI_DISPLAY_PRIMARIES_2:
			value = pOutStr->MasteringDisplayColourVolumeSei5;
			break;
		case V4L2_CID_MPEG_VIDEO_FRAME_ERROR_TYPE:
			value = mfc_get_frame_error_type(ctx, pOutStr->ErrorCode);
			break;
			/* If new dynamic controls are added, insert here */
		default:
			if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
				mfc_ctx_info("[NALQ] can't find control, id: 0x%x\n",
					buf_ctrl->id);
		}
		value = (value >> buf_ctrl->shft) & buf_ctrl->mask;

		buf_ctrl->val = value;
		buf_ctrl->has_new = 1;

		if (IS_VP9_DEC(ctx)) {
			if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_FULL_RANGE_FLAG)
				buf_ctrl->val = dec->color_range;
			else if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_COLOUR_PRIMARIES)
				buf_ctrl->val = dec->color_space;
		}

		mfc_debug(6, "[NALQ][CTRLS] Get buffer control id: 0x%08x, val: %d (%#x)\n",
				buf_ctrl->id, buf_ctrl->val, buf_ctrl->val);
	}

	mfc_debug_leave();

	return 0;
}

static int mfc_core_set_buf_ctrls_nal_q_enc(struct mfc_ctx *ctx,
			struct list_head *head, EncoderInputStr *pInStr)
{
	struct mfc_buf_ctrl *buf_ctrl;
	struct mfc_enc *enc = ctx->enc_priv;
	struct temporal_layer_info temporal_LC;
	unsigned int i, param_change;
	struct mfc_enc_params *p = &enc->params;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET) || !buf_ctrl->has_new)
			continue;
		param_change = 0;
		switch (buf_ctrl->id) {
		case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG:
			pInStr->PictureTag &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->PictureTag |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			ctx->stored_tag = buf_ctrl->val;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE:
			pInStr->FrameInsertion &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->FrameInsertion |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_I_PERIOD_CH:
			pInStr->GopConfig &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->GopConfig |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			pInStr->GopConfig2 &= ~(0x3FFF);
			pInStr->GopConfig2 |= (buf_ctrl->val >> 16) & 0x3FFF;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE_CH:
			p->rc_frame_delta = p->rc_framerate_res / buf_ctrl->val;
			pInStr->RcFrameRate &= ~(0xFFFF << 16);
			pInStr->RcFrameRate |= (p->rc_framerate_res & 0xFFFF) << 16;
			pInStr->RcFrameRate &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->RcFrameRate |=
				(p->rc_frame_delta & buf_ctrl->mask) << buf_ctrl->shft;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_BIT_RATE_CH:
			pInStr->RcBitRate &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->RcBitRate |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_H263_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_VP8_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_VP9_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
		case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP:
		case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP:
		case V4L2_CID_MPEG_VIDEO_H263_MIN_QP:
		case V4L2_CID_MPEG_VIDEO_VP8_MIN_QP:
		case V4L2_CID_MPEG_VIDEO_VP9_MIN_QP:
			pInStr->RcQpBound &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->RcQpBound |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_MAX_QP_P:
		case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP_P:
		case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP_P:
		case V4L2_CID_MPEG_VIDEO_H263_MAX_QP_P:
		case V4L2_CID_MPEG_VIDEO_VP8_MAX_QP_P:
		case V4L2_CID_MPEG_VIDEO_VP9_MAX_QP_P:
		case V4L2_CID_MPEG_VIDEO_H264_MIN_QP_P:
		case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP_P:
		case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP_P:
		case V4L2_CID_MPEG_VIDEO_H263_MIN_QP_P:
		case V4L2_CID_MPEG_VIDEO_VP8_MIN_QP_P:
		case V4L2_CID_MPEG_VIDEO_VP9_MIN_QP_P:
		case V4L2_CID_MPEG_VIDEO_H264_MAX_QP_B:
		case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP_B:
		case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP_B:
		case V4L2_CID_MPEG_VIDEO_H264_MIN_QP_B:
		case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP_B:
		case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP_B:
			pInStr->RcQpBoundPb &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->RcQpBoundPb |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH:
		case V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH:
		case V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_CH:
		case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_CH:
			memcpy(&temporal_LC,
				enc->sh_handle_svc.vaddr, sizeof(struct temporal_layer_info));

			/* Store temporal layer information */
			__mfc_enc_store_buf_ctrls_temporal_svc(buf_ctrl->id, p,
					&temporal_LC);

			if (((temporal_LC.temporal_layer_count & 0x7) < 1) ||
				((temporal_LC.temporal_layer_count > 3) && IS_VP8_ENC(ctx)) ||
				((temporal_LC.temporal_layer_count > 3) && IS_VP9_ENC(ctx))) {
				/* claer NUM_T_LAYER_CHANGE */
				mfc_ctx_err("[NALQ][HIERARCHICAL] layer count(%d) is invalid\n",
						temporal_LC.temporal_layer_count);
				return 0;
			}

			/* enable RC_BIT_RATE_CHANGE */
			if (temporal_LC.temporal_layer_bitrate[0] > 0 || p->hier_bitrate_ctrl)
				pInStr->ParamChange |= (1 << 2);
			else
				pInStr->ParamChange &= ~(1 << 2);

			/* enalbe NUM_T_LAYER_CHANGE */
			if (temporal_LC.temporal_layer_count & 0x7)
				pInStr->ParamChange |= (1 << 10);
			else
				pInStr->ParamChange &= ~(1 << 10);
			mfc_debug(3, "[NALQ][HIERARCHICAL] layer count %d\n",
					temporal_LC.temporal_layer_count & 0x7);

			pInStr->NumTLayer &= ~(0x7);
			pInStr->NumTLayer |= (temporal_LC.temporal_layer_count & 0x7);
			pInStr->NumTLayer &= ~(0x1 << 8);
			pInStr->NumTLayer |= (p->hier_bitrate_ctrl & 0x1) << 8;
			for (i = 0; i < (temporal_LC.temporal_layer_count & 0x7); i++) {
				mfc_debug(3, "[NALQ][HIERARCHICAL] layer bitrate[%d] %d (FW ctrl: %d)\n",
					i, temporal_LC.temporal_layer_bitrate[i], p->hier_bitrate_ctrl);
				pInStr->HierarchicalBitRateLayer[i] =
					temporal_LC.temporal_layer_bitrate[i];
			}

			/* priority change */
			if (IS_H264_ENC(ctx)) {
				for (i = 0; i < (temporal_LC.temporal_layer_count & 0x7); i++) {
					if (i <= 4)
						pInStr->H264HDSvcExtension0 |=
							((p->codec.h264.base_priority & 0x3f) + i) << (6 * i);
					else
						pInStr->H264HDSvcExtension1 |=
							((p->codec.h264.base_priority & 0x3f) + i) << (6 * (i - 5));
				}
				mfc_debug(3, "[NALQ][HIERARCHICAL] EXTENSION0 %#x, EXTENSION1 %#x\n",
						pInStr->H264HDSvcExtension0, pInStr->H264HDSvcExtension1);

				pInStr->ParamChange |= (1 << 12);
			}
			break;
		case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
			pInStr->PictureProfile &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->PictureProfile |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			pInStr->PictureProfile &= ~(0xf);
			pInStr->PictureProfile |= p->codec.h264.profile & 0xf;
			p->codec.h264.level = buf_ctrl->val;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
			pInStr->PictureProfile &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->PictureProfile |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			pInStr->PictureProfile &= ~(0xff << 8);
			pInStr->PictureProfile |= (p->codec.h264.level << 8) & 0xff00;
			p->codec.h264.profile = buf_ctrl->val;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_MFC_H264_MARK_LTR:
			pInStr->H264NalControl &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->H264NalControl |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			pInStr->H264NalControl &= ~(0x7 << 8);
			pInStr->H264NalControl |= (buf_ctrl->val & 0x7) << 8;
			break;
		case V4L2_CID_MPEG_MFC_H264_USE_LTR:
			pInStr->H264NalControl &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->H264NalControl |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			pInStr->H264NalControl &= ~(0xF << 11);
			pInStr->H264NalControl |= (buf_ctrl->val & 0xF) << 11;
			break;
		case V4L2_CID_MPEG_MFC_H264_BASE_PRIORITY:
			for (i = 0; i < (p->codec.h264.num_hier_layer & 0x7); i++)
				if (i <= 4)
					pInStr->H264HDSvcExtension0 |=
						((buf_ctrl->val & 0x3f) + i) << (6 * i);
				else
					pInStr->H264HDSvcExtension1 |=
						((buf_ctrl->val & 0x3f) + i) << (6 * (i - 5));
			param_change = 1;
			break;
		case V4L2_CID_MPEG_MFC_CONFIG_QP:
			pInStr->FixedPictureQp &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->FixedPictureQp |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			enc->config_qp = p->config_qp;
			break;
		case V4L2_CID_MPEG_VIDEO_ROI_CONTROL:
			pInStr->RcRoiCtrl &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->RcRoiCtrl |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			pInStr->RoiBufferAddr = enc->roi_buf[buf_ctrl->old_val2].daddr;
			mfc_debug(3, "[NALQ][ROI] buffer[%d] addr %#llx, QP val: %#x\n",
					buf_ctrl->old_val2,
					enc->roi_buf[buf_ctrl->old_val2].daddr,
					buf_ctrl->val);
			break;
		case V4L2_CID_MPEG_VIDEO_YSUM:
			pInStr->Weight &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->Weight |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			break;
		case V4L2_CID_MPEG_VIDEO_RATIO_OF_INTRA:
			pInStr->RcMode &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->RcMode |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_VIDEO_DROP_CONTROL:
			if (!ctx->src_ts.ts_last_interval) {
				p->rc_frame_delta = p->rc_framerate_res / p->rc_framerate;
				mfc_debug(3, "[NALQ][DROPCTRL] default delta: %d\n", p->rc_frame_delta);
			} else {
				/*
				 * FRAME_DELTA specifies the amount of
				 * increment of frame modulo base time.
				 * So, we will take to framerate resolution / fps concept.
				 * - delta unit = framerate resolution / fps
				 * - fps = 1000000(usec per sec) / timestamp interval
				 * For the sophistication of calculation, we will divide later.
				 * Excluding H.263, resolution is fixed to 10000,
				 * so thie is also divided into pre-calculated 100.
				 * (Preventing both overflow and calculation duplication)
				 */
				if (IS_H263_ENC(ctx))
					p->rc_frame_delta = ctx->src_ts.ts_last_interval *
						p->rc_framerate_res / 1000000;
				else
					p->rc_frame_delta = ctx->src_ts.ts_last_interval / 100;
			}
			pInStr->RcFrameRate &= ~(0xFFFF << 16);
			pInStr->RcFrameRate |= (p->rc_framerate_res & 0xFFFF) << 16;
			pInStr->RcFrameRate &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->RcFrameRate |=
				(p->rc_frame_delta & buf_ctrl->mask) << buf_ctrl->shft;
			if (ctx->src_ts.ts_last_interval)
				mfc_debug(3, "[NALQ][DROPCTRL] fps %d -> %ld, delta: %d, reg: %#x\n",
					p->rc_framerate, USEC_PER_SEC / ctx->src_ts.ts_last_interval,
					p->rc_frame_delta, pInStr->RcFrameRate);
			else
				mfc_debug(3, "[NALQ][DROPCTRL] fps %d -> 0, delta: %d, reg: %#x\n",
					p->rc_framerate, p->rc_frame_delta, pInStr->RcFrameRate);
			break;
		case V4L2_CID_MPEG_VIDEO_MV_HOR_POSITION_L0:
		case V4L2_CID_MPEG_VIDEO_MV_HOR_POSITION_L1:
			pInStr->MVHorRange &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->MVHorRange |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			break;
		case V4L2_CID_MPEG_VIDEO_MV_VER_POSITION_L0:
		case V4L2_CID_MPEG_VIDEO_MV_VER_POSITION_L1:
			pInStr->MVVerRange &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->MVVerRange |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			break;
			break;
		/* If new dynamic controls are added, insert here */
		default:
			if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
				mfc_ctx_info("[NALQ] can't find control, id: 0x%x\n",
					buf_ctrl->id);
		}

		if (param_change)
			pInStr->ParamChange |= (1 << buf_ctrl->flag_shft);

		buf_ctrl->has_new = 0;
		buf_ctrl->updated = 1;

		mfc_debug(6, "[NALQ][CTRLS] Set buffer control id: 0x%08x, val: %d (%#x)\n",
				buf_ctrl->id, buf_ctrl->val, buf_ctrl->val);
	}

	return 0;
}

static int mfc_core_get_buf_ctrls_nal_q_enc(struct mfc_ctx *ctx,
			struct list_head *head, EncoderOutputStr *pOutStr)
{
	struct mfc_buf_ctrl *buf_ctrl;
	struct mfc_enc *enc = ctx->enc_priv;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_GET))
			continue;
		switch (buf_ctrl->id) {
		case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG:
			value = pOutStr->PictureTag;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_LUMA_ADDR:
			value = pOutStr->EncodedFrameAddr[0];
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_CHROMA_ADDR:
			value = pOutStr->EncodedFrameAddr[1];
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_STATUS:
			value = !enc->in_slice;
			break;
		case V4L2_CID_MPEG_VIDEO_AVERAGE_QP:
			value = pOutStr->NalDoneInfo;
			break;
		/* If new dynamic controls are added, insert here */
		default:
			if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
				mfc_ctx_info("[NALQ] can't find control, id: 0x%x\n",
					buf_ctrl->id);
		}
		value = (value >> buf_ctrl->shft) & buf_ctrl->mask;

		buf_ctrl->val = value;
		buf_ctrl->has_new = 1;

		mfc_debug(6, "[NALQ][CTRLS] Get buffer control id: 0x%08x, val: %d (%#x)\n",
				buf_ctrl->id, buf_ctrl->val, buf_ctrl->val);
	}

	return 0;
}

static int mfc_core_recover_buf_ctrls_nal_q(struct mfc_ctx *ctx, struct list_head *head)
{
	struct mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET)
				|| !buf_ctrl->is_volatile
				|| !(buf_ctrl->updated))
			continue;

		buf_ctrl->updated = 0;
		mfc_debug(6, "[NALQ][CTRLS] Recover buffer control id: 0x%08x\n", buf_ctrl->id);
	}

	return 0;
}

/*
 * This function is used when you want to restore buffer ctrls.
 * 1) NAL_Q stop: It is enqueued in the NAL_Q queue,
 *		but it must be restored because HW is not used.
 * 2) DRC: The input buffer that caused the DRC was used for HW, but it must be reused.
 */
static int mfc_core_restore_buf_ctrls(struct mfc_ctx *ctx, struct list_head *head)
{
	struct mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET)
				|| !(buf_ctrl->updated))
			continue;

		buf_ctrl->has_new = 1;
		buf_ctrl->updated = 0;

		mfc_debug(6, "[CTRLS] Restore buffer control id: 0x%08x, val: %d\n",
				buf_ctrl->id, buf_ctrl->val);
	}

	return 0;
}

struct mfc_bufs_ops mfc_bufs_ops = {
	.core_set_buf_ctrls		= mfc_core_set_buf_ctrls,
	.core_get_buf_ctrls		= mfc_core_get_buf_ctrls,
	.core_recover_buf_ctrls		= mfc_core_recover_buf_ctrls,
	.core_set_buf_ctrls_nal_q_dec	= mfc_core_set_buf_ctrls_nal_q_dec,
	.core_get_buf_ctrls_nal_q_dec	= mfc_core_get_buf_ctrls_nal_q_dec,
	.core_set_buf_ctrls_nal_q_enc	= mfc_core_set_buf_ctrls_nal_q_enc,
	.core_get_buf_ctrls_nal_q_enc	= mfc_core_get_buf_ctrls_nal_q_enc,
	.core_recover_buf_ctrls_nal_q	= mfc_core_recover_buf_ctrls_nal_q,
	.core_restore_buf_ctrls		= mfc_core_restore_buf_ctrls,
};
