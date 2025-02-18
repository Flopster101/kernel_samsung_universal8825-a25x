// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-isp-v2.h"
#include "is-err.h"

#define COREX_SET COREX_DIRECT
#define DISABLE_TNR 0

static ulong debug_isp;
module_param(debug_isp, ulong, 0644);

static enum base_reg_index __is_hw_isp_get_reg_base(enum is_hw_isp_reg_idx reg_idx)
{
	enum base_reg_index reg_base;

	switch (reg_idx) {
	case ISP_REG_ITP:
		reg_base = REG_SETA;
		break;
	case ISP_REG_MCFP0:
		reg_base = REG_EXT1;
		break;
	case ISP_REG_DNS:
		reg_base = REG_EXT2;
		break;
	case ISP_REG_MCFP1:
		reg_base = REG_EXT3;
		break;
	default:
		err_hw("invalid reg_idx (%d)", reg_idx);
		return -EINVAL;
	}

	return reg_base;
}

static int __is_hw_isp_get_reg_cnt(struct is_hw_ip *hw_ip, u32 instance,
	enum is_hw_isp_reg_idx reg_idx, u32 *reg_cnt)
{
	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	switch (reg_idx) {
	case ISP_REG_ITP:
		*reg_cnt = itp_hw_g_reg_cnt();
		break;
	case ISP_REG_DNS:
		*reg_cnt = dns_hw_g_reg_cnt();
		break;
	case ISP_REG_MCFP0:
		*reg_cnt = mcfp0_hw_g_reg_cnt();
		break;
	case ISP_REG_MCFP1:
		*reg_cnt = mcfp1_hw_g_reg_cnt();
		break;
	default:
		mserr_hw("invalid reg_idx (%d)", instance, hw_ip, reg_idx);
		*reg_cnt = 0;
		return -EINVAL;
	}

	return 0;
}

static int __is_hw_isp_get_mcfp_internal_frame(struct is_hw_ip *hw_ip, u32 instance,
	struct is_frame *frame,
	struct is_subdev *subdev,
	struct is_frame **prev_frame, struct is_frame **cur_frame, const char *data_type_name)
{
	struct is_framemgr *internal_framemgr = NULL;

	internal_framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
	if (!internal_framemgr) {
		mserr_hw("Image internal framemgr is NULL", instance, hw_ip);
		return -EINVAL;
	}

	/* cur wdma image or weight */
	*cur_frame = &internal_framemgr->frames[0];
	if (!*cur_frame) {
		mserr_hw("cur %s internal frame is NULL", instance, hw_ip, data_type_name);
		return -EINVAL;
	}

	/* prev rdma image or weight */
	*prev_frame = &internal_framemgr->frames[1];
	if (!*prev_frame) {
		mserr_hw("prev %s internal frame is NULL", instance, hw_ip, data_type_name);
		return -EINVAL;
	}

	msdbg_hw(3, "[F:%d] mcfp prev %s rdma addr(%#x), cur %s rdma addr(%#x)\n",
		instance, hw_ip,
		frame->fcount,
		data_type_name, (*prev_frame)->dvaddr_buffer[0],
		data_type_name, (*cur_frame)->dvaddr_buffer[0]);

	return 0;
}

static int __is_hw_isp_init(struct is_hw_ip *hw_ip, u32 instance)
{
	int res;
	struct is_hw_isp *hw_isp;
	enum base_reg_index itp_base;
	enum base_reg_index dns_base;
	enum base_reg_index mcfp0_base, mcfp1_base;
	u32 seed;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	itp_base = __is_hw_isp_get_reg_base(ISP_REG_ITP);
	dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);
	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
	mcfp1_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP1);

	msinfo_hw("reset\n", instance, hw_ip);
	res = dns_hw_s_reset(hw_ip->regs[dns_base]);
	if (res) {
		mserr_hw("dns hw sw reset fail", instance, hw_ip);
		return -ENODEV;
	}
	res = mcfp0_hw_s_reset(hw_ip->regs[mcfp0_base]);
	if (res) {
		mserr_hw("mcfp hw sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	msinfo_hw("init\n", instance, hw_ip);

	itp_hw_s_control_init(hw_ip->regs[itp_base], COREX_SET);
	itp_hw_s_coutfifo_init(hw_ip->regs[itp_base], COREX_SET);
	itp_hw_s_module_init(hw_ip->regs[itp_base], COREX_SET);
	itp_hw_s_control_debug(hw_ip->regs[itp_base], COREX_SET);

	dns_hw_s_control_init(hw_ip->regs[dns_base], COREX_SET);
	dns_hw_s_control_debug(hw_ip->regs[dns_base], COREX_SET);
	dns_hw_s_cinfifo_init(hw_ip->regs[dns_base], COREX_SET);
	dns_hw_s_int_init(hw_ip->regs[dns_base]);

	mcfp0_hw_s_control_init(hw_ip->regs[mcfp0_base], COREX_SET);
	mcfp1_hw_s_control_init(hw_ip->regs[mcfp1_base], COREX_SET);
	mcfp0_hw_s_int_init(hw_ip->regs[mcfp0_base]);
	mcfp0_hw_s_corex_init(hw_ip->regs[mcfp0_base], false);
	mcfp1_hw_s_corex_init(hw_ip->regs[mcfp1_base], false);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed)) {
		itp_hw_s_crc(hw_ip->regs[itp_base], seed);
		dns_hw_s_crc(hw_ip->regs[dns_base], seed);
		mcfp0_hw_s_crc(hw_ip->regs[mcfp0_base], seed);
		mcfp1_hw_s_crc(hw_ip->regs[mcfp1_base], seed);
	}

	return 0;
}

static int __is_hw_isp_enable(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, u32 set_id)
{
	struct is_hw_isp *hw_isp;
	enum base_reg_index dns_base;
	enum base_reg_index mcfp0_base, mcfp1_base;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;

	dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);
	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
	mcfp1_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP1);

	hw_isp->hw_fro_en = (hw_ip->num_buffers & 0xffff) > 1 ? true : false;

	if (param_set->tnr_mode != TNR_PROCESSING_TNR_BYPASS) {
		atomic_set(&hw_isp->count.max_cnt, 2);
		atomic_set(&hw_isp->count.fs, 0);
		atomic_set(&hw_isp->count.fe, 0);
		dns_hw_s_enable(hw_ip->regs[dns_base], false, hw_isp->hw_fro_en);
		mcfp0_hw_s_enable(hw_ip->regs[mcfp0_base], set_id);
		mcfp1_hw_s_enable(hw_ip->regs[mcfp1_base], set_id);
	} else {
		atomic_set(&hw_isp->count.max_cnt, 1);
		atomic_set(&hw_isp->count.fs, 0);
		atomic_set(&hw_isp->count.fe, 0);
		dns_hw_s_enable(hw_ip->regs[dns_base], true, hw_isp->hw_fro_en);
	}

	return 0;
}

static int __is_hw_isp_set_fro(struct is_hw_ip *hw_ip, u32 instance, u32 set_id)
{
	enum base_reg_index dns_base, mcfp0_base, mcfp1_base;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);
	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
	mcfp1_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP1);

	dns_hw_s_fro(hw_ip->regs[dns_base], set_id, (hw_ip->num_buffers & 0xffff));
	mcfp0_hw_s_fro(hw_ip->regs[mcfp0_base], set_id, (hw_ip->num_buffers & 0xffff));
	mcfp1_hw_s_fro(hw_ip->regs[mcfp1_base], set_id, (hw_ip->num_buffers & 0xffff));

	return 0;
}

static int __is_hw_isp_set_control(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, u32 set_id)
{
	bool mcfp_enable;
	bool prev_sbwc_enable;
	u32 bayer_order;
	struct is_hw_isp *hw_isp;
	struct is_isp_config *iq_config;
	struct dns_control_config dns_config;
	struct itp_control_config itp_config;
	enum base_reg_index itp_base;
	enum base_reg_index dns_base;
	enum base_reg_index mcfp0_base, mcfp1_base;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	itp_base = __is_hw_isp_get_reg_base(ISP_REG_ITP);
	dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);
	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
	mcfp1_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP1);
	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	iq_config = &hw_isp->iq_config[instance];

	if (param_set->tnr_mode == TNR_PROCESSING_TNR_BYPASS) {
		dns_config.input_type = DNS_INPUT_DMA;
		mcfp_enable = false;
	} else {
		dns_config.input_type = DNS_INPUT_OTF;
		if (IS_ENABLED(ISP_DDK_LIB_CALL))
			mcfp_enable = true;
		else
			mcfp_enable = false;
	}


	dns_config.dirty_bayer_sel_dns = iq_config->dirty_bayer_en;
	if ((param_set->tnr_mode == TNR_PROCESSING_CAPTURE_LAST) ||
	    (param_set->tnr_mode == TNR_PROCESSING_CAPTURE_FIRST_AND_LAST))
		dns_config.rdma_dirty_bayer_en = true;
	else
		dns_config.rdma_dirty_bayer_en = false;
	dns_config.ww_lc_en = iq_config->ww_lc_en;
	dns_config.hqdns_tnr_input_en = iq_config->tnr_input_en;

	itp_config.dirty_bayer_sel_dns = iq_config->dirty_bayer_en;
	itp_config.ww_lc_en = iq_config->ww_lc_en;

	if ((param_set->dma_input.sbwc_type & DMA_INPUT_SBWC_MASK) == DMA_INPUT_SBWC_DISABLE)
		prev_sbwc_enable = false;
	else
		prev_sbwc_enable = true;

	bayer_order = param_set->dma_input.order;

	itp_hw_s_control_config(hw_ip->regs[itp_base], set_id, &itp_config);
	itp_hw_s_module_format(hw_ip->regs[itp_base], set_id, bayer_order);

	dns_hw_s_control_config(hw_ip->regs[dns_base], set_id, &dns_config);
	dns_hw_s_module_format(hw_ip->regs[dns_base], set_id, bayer_order);
	if (unlikely(test_bit(ISP_DEBUG_DRC_BYPASS, &debug_isp))) {
		if (dns_hw_bypass_drcdstr(hw_ip->regs[dns_base], set_id))
			hw_isp->cur_hw_iq_set[set_id][ISP_REG_DNS].size = 0;
	}

	mcfp0_hw_s_control_config(hw_ip->regs[mcfp0_base], set_id, &hw_isp->iq_config[instance],
				  bayer_order, prev_sbwc_enable, mcfp_enable);
	mcfp1_hw_s_control_config(hw_ip->regs[mcfp1_base], set_id, &hw_isp->iq_config[instance],
				  bayer_order, &param_set->stripe_input,
				  prev_sbwc_enable, mcfp_enable);

	return 0;
}

static int __is_hw_isp_set_dma_cmd(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, struct is_isp_config *conf)
{
	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	/* TODO: Control IQ regs dependent DMAs with config info */
	if (!conf->mixer_en) {
		param_set->motion_dma_input.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->prev_dma_input.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->prev_wgt_dma_input.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->dma_output_tnr_prev.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->dma_output_tnr_wgt.cmd = DMA_OUTPUT_COMMAND_DISABLE;

		return 0;
	}

	if (!conf->still_en) {
		/* Video TNR */
		param_set->dma_output_tnr_prev.cmd = DMA_OUTPUT_COMMAND_ENABLE;
		switch (conf->tnr_mode) {
		case TNR_MODE_PREPARE:
			param_set->motion_dma_input.cmd = DMA_INPUT_COMMAND_DISABLE;
			param_set->prev_dma_input.cmd = DMA_INPUT_COMMAND_DISABLE;
			param_set->prev_wgt_dma_input.cmd = DMA_INPUT_COMMAND_DISABLE;
			param_set->dma_output_tnr_wgt.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			break;
		case TNR_MODE_FIRST:
			param_set->motion_dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
			param_set->prev_dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
			param_set->prev_wgt_dma_input.cmd = DMA_INPUT_COMMAND_DISABLE;
			param_set->dma_output_tnr_wgt.cmd = DMA_OUTPUT_COMMAND_ENABLE;
			break;
		case TNR_MODE_NORMAL:
			param_set->motion_dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
			param_set->prev_dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
			param_set->prev_wgt_dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
			if (conf->skip_wdma) {
				param_set->dma_output_tnr_prev.cmd = DMA_OUTPUT_COMMAND_DISABLE;
				param_set->dma_output_tnr_wgt.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			} else {
				param_set->dma_output_tnr_wgt.cmd = DMA_OUTPUT_COMMAND_ENABLE;
			}
			break;
		default:
			break;
		}
	} else {
		/* MF-Still */
		param_set->prev_wgt_dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
		param_set->dma_output_tnr_wgt.cmd = DMA_OUTPUT_COMMAND_ENABLE;
	}

	return 0;
}

static int __is_hw_isp_set_mcfp_rdma(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, u32 set_id, u32 dma_id)
{
	struct is_hw_isp *hw_isp;
	struct is_isp_config *config;
	u32 *input_dva;
	u32 dma_en = false;
	struct param_dma_input *dma_input;
	enum base_reg_index mcfp0_base;
	int ret;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	config = &hw_isp->iq_config[instance];

	switch (dma_id) {
	case MCFP_RDMA_CURR_BAYER:
		input_dva = param_set->input_dva;
		dma_input = &param_set->dma_input;
		dma_en = dma_input->cmd;
		break;
	case MCFP_RDMA_PREV_BAYER0:
	case MCFP_RDMA_PREV_BAYER1:
	case MCFP_RDMA_PREV_BAYER2:
		input_dva = param_set->input_dva_tnr_prev;
		dma_input = &param_set->dma_input;
		dma_en = param_set->prev_dma_input.cmd;
		break;
	case MCFP_RDMA_PREV_MOTION0:
	case MCFP_RDMA_PREV_MOTION1:
		input_dva = param_set->input_dva_motion;
		dma_input = &param_set->dma_input;
		dma_en = (config->geomatch_bypass == 1) ? DMA_INPUT_COMMAND_DISABLE : param_set->motion_dma_input.cmd;
		break;
	case MCFP_RDMA_PREV_WGT:
		input_dva = param_set->input_dva_tnr_wgt;
		dma_input = &param_set->dma_input;
		dma_en = param_set->prev_wgt_dma_input.cmd;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, dma_id);
		return -EINVAL;
	}

	/* TNR bypass mode , MCFP not used */
	if (param_set->tnr_mode == TNR_PROCESSING_TNR_BYPASS)
		dma_en = false;
	msdbg_hw(2, "%s: enable(%d)\n", instance, hw_ip, hw_isp->mcfp_rdma[dma_id].name, dma_en);

	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
	ret = mcfp0_hw_s_rdma(&hw_isp->mcfp_rdma[dma_id], hw_ip->regs[mcfp0_base], set_id, dma_input, dma_en,
		&param_set->stripe_input, &hw_isp->iq_config[instance],
		input_dva, (hw_ip->num_buffers & 0xffff));
	if (ret) {
		mserr_hw("failed to initialize MCFP_RDMA(%d)", instance, hw_ip, dma_id);
		return -EINVAL;
	}

	return 0;
}

static int __is_hw_isp_set_mcfp_wdma(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, u32 set_id, u32 dma_id)
{
	struct is_hw_isp *hw_isp;
	u32 *output_dva;
	u32 dma_en = false;
	struct param_dma_input *dma_input;
	enum base_reg_index mcfp0_base;
	int ret;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;

	switch (dma_id) {
	case MCFP_WDMA_PREV_BAYER:
		output_dva = param_set->output_dva_tnr_prev;
		dma_input = &param_set->dma_input;
		dma_en = param_set->dma_output_tnr_prev.cmd;
		break;
	case MCFP_WDMA_PREV_WGT:
		output_dva = param_set->output_dva_tnr_wgt;
		dma_input = &param_set->dma_input;
		dma_en = param_set->dma_output_tnr_wgt.cmd;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, dma_id);
		return -EINVAL;
	}

	/* TNR bypass mode , MCFP not used */
	if (param_set->tnr_mode == TNR_PROCESSING_TNR_BYPASS)
		dma_en = false;

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_isp->mcfp_wdma[dma_id].name, dma_en);

	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
	ret = mcfp0_hw_s_wdma(&hw_isp->mcfp_wdma[dma_id], hw_ip->regs[mcfp0_base], set_id, dma_input, dma_en,
		&param_set->stripe_input, &hw_isp->iq_config[instance],
		output_dva, (hw_ip->num_buffers & 0xffff));
	if (ret) {
		mserr_hw("failed to initialize MCFP_WDMA(%d)", instance, hw_ip, dma_id);
		return -EINVAL;
	}

	return 0;
}

static int __is_hw_isp_set_dns_rdma(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, u32 set_id, u32 dma_id)
{
	struct is_hw_isp *hw_isp;
	u32 *input_dva;
	struct param_dma_input *dma_input;
	struct param_dma_input dns_rdma_bayer;
	u32 stripe_idx;
	int ret;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	stripe_idx =  param_set->stripe_input.index;

	switch (dma_id) {
	case DNS_RDMA_BAYER:
		/* set dva */
		if ((param_set->tnr_mode == TNR_PROCESSING_CAPTURE_LAST) &&
		    hw_isp->input_dva_tnr_ref[instance][stripe_idx])
			input_dva = &hw_isp->input_dva_tnr_ref[instance][stripe_idx];
		else
			input_dva = param_set->input_dva;

		/* set dma param */
		dns_rdma_bayer = param_set->dma_input;
		/* disable dns rdma except for tnr bypass & capture last */
		if ((param_set->tnr_mode != TNR_PROCESSING_TNR_BYPASS) &&
		    (param_set->tnr_mode != TNR_PROCESSING_CAPTURE_LAST) &&
		    (param_set->tnr_mode != TNR_PROCESSING_CAPTURE_FIRST_AND_LAST))
			dns_rdma_bayer.cmd = DMA_INPUT_COMMAND_DISABLE;
		dma_input = &dns_rdma_bayer;

		/* keep ref image buffer for capture last */
		if ((param_set->tnr_mode == TNR_PROCESSING_CAPTURE_FIRST_POST_ON) ||
		    (param_set->tnr_mode == TNR_PROCESSING_CAPTURE_FIRST))
			hw_isp->input_dva_tnr_ref[instance][stripe_idx] = param_set->input_dva[0];
		break;
	case DNS_RDMA_DRC0:
	case DNS_RDMA_DRC1:
	case DNS_RDMA_DRC2:
		input_dva = param_set->input_dva_drcgrid;
		dma_input = &param_set->dma_input_drcgrid;
		if (unlikely(test_bit(ISP_DEBUG_DRC_BYPASS, &debug_isp)))
			dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, dma_id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_isp->dns_dma[dma_id].name, dma_input->cmd);

	ret = dns_hw_s_rdma(set_id, &hw_isp->dns_dma[dma_id], dma_input, &param_set->stripe_input,
		input_dva, (hw_ip->num_buffers & 0xffff));
	if (ret) {
		mserr_hw("dns_hw_s_rdma failed: DNS_DMA(%d)", instance, hw_ip, dma_id);
		return -EINVAL;
	}

	return 0;
}

static int __is_hw_isp_set_dma(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, u32 set_id)
{
	int ret, i;

	/* MCFP RDMA */
	for (i = MCFP_RDMA_CURR_BAYER; i < MCFP_RDMA_MAX; i++) {
		ret = __is_hw_isp_set_mcfp_rdma(hw_ip, param_set, instance, set_id, i);
		if (ret) {
			mserr_hw("__is_hw_isp_set_mcfp_rdma is fail", instance, hw_ip);
			return ret;
		}
	}
	/* MCFP WDMA */
	for (i = MCFP_WDMA_PREV_BAYER; i < MCFP_WDMA_MAX; i++) {
		ret = __is_hw_isp_set_mcfp_wdma(hw_ip, param_set, instance, set_id, i);
		if (ret) {
			mserr_hw("__is_hw_isp_set_mcfp_wdma is fail", instance, hw_ip);
			return ret;
		}
	}
	/* DNS RDMA */
	for (i = DNS_RDMA_BAYER; i < DNS_RDMA_MAX; i++) {
		ret = __is_hw_isp_set_dns_rdma(hw_ip, param_set, instance, set_id, i);
		if (ret) {
			mserr_hw("__is_hw_isp_set_dns_rdma is fail", instance, hw_ip);
			return ret;
		}
	}

	return 0;
}

static int __is_hw_isp_set_otf_input(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, u32 set_id)
{
	int ret;
	struct itp_cin_cout_config itp_config;
	struct dns_cinfifo_config dns_config;
	struct is_hw_isp *hw_isp;
	struct is_isp_config *iq_config;
	enum base_reg_index itp_base;
	enum base_reg_index dns_base;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	iq_config = &hw_isp->iq_config[instance];
	itp_base = __is_hw_isp_get_reg_base(ISP_REG_ITP);
	dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);

	if (param_set->tnr_mode == TNR_PROCESSING_TNR_BYPASS)
		dns_config.enable = false;
	else
		dns_config.enable = true;
	dns_config.width = param_set->dma_input.width;
	dns_config.height = param_set->dma_input.height;
	ret = dns_hw_s_cinfifo_config(hw_ip->regs[dns_base], set_id, &dns_config);
	if (ret) {
		mserr_hw("dns_hw_s_cinfifo_config error", instance, hw_ip);
		return -EINVAL;
	}

	if (iq_config->dirty_bayer_en)
		itp_config.enable = false;
	else
		itp_config.enable = true;

	itp_config.width = param_set->dma_input.width;
	itp_config.height = param_set->dma_input.height;
	ret = itp_hw_s_cinfifo_config(hw_ip->regs[itp_base], set_id, &itp_config);
	if (ret) {
		mserr_hw("itp_hw_s_cinfifo_config error", instance, hw_ip);
		return -EINVAL;
	}

	return 0;
}

static int __is_hw_isp_set_otf_output(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, u32 set_id)
{
	int ret;
	struct itp_cin_cout_config itp_config;
	struct param_otf_output *otf_output;
	enum base_reg_index itp_base;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	itp_base = __is_hw_isp_get_reg_base(ISP_REG_ITP);

	otf_output = &param_set->otf_output;

	itp_config.enable = (otf_output->cmd == OTF_OUTPUT_COMMAND_ENABLE) ? true : false;
	itp_config.width = otf_output->width;
	itp_config.height = otf_output->height;

	ret = itp_hw_s_coutfifo_config(hw_ip->regs[itp_base], set_id, &itp_config);
	if (ret) {
		mserr_hw("itp_hw_s_coutfifo_config error", instance, hw_ip);
		return -EINVAL;
	}
	return 0;
}

static int __is_hw_isp_bypass_iq_module(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, u32 set_id)
{
	enum base_reg_index itp_base;
	enum base_reg_index dns_base;
	enum base_reg_index mcfp0_base, mcfp1_base;
	struct is_hw_isp *hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	struct is_isp_config *iq_config = &hw_isp->iq_config[instance];

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	itp_base = __is_hw_isp_get_reg_base(ISP_REG_ITP);
	dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);
	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
	mcfp1_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP1);

	memset(&hw_isp->iq_config[instance], 0x0, sizeof(struct is_isp_config));

	iq_config->dirty_bayer_en = 1;
	iq_config->drc_dstr_bypass = 1;
	itp_hw_s_module_bypass(hw_ip->regs[itp_base], set_id);
	dns_hw_s_module_bypass(hw_ip->regs[dns_base], set_id);

	iq_config->geomatch_bypass = 1;
	iq_config->img_bit = DMA_INOUT_BIT_WIDTH_13BIT;
	iq_config->wgt_bit = DMA_INOUT_BIT_WIDTH_8BIT;
	iq_config->top_otfout_mask_clean = 0;
	iq_config->top_otfout_mask_dirty = 1;
	mcfp0_hw_s_module_bypass(hw_ip->regs[mcfp0_base], set_id);
	mcfp1_hw_s_module_bypass(hw_ip->regs[mcfp1_base], set_id);

	return 0;
}

static void __is_hw_isp_update_param(struct is_hw_ip *hw_ip, struct is_region *region,
	struct isp_param_set *param_set, IS_DECLARE_PMAP(pmap), u32 instance)
{
	struct isp_param *param;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	param = &region->parameter.isp;
	param_set->instance_id = instance;

	/* check input */
	if (test_bit(PARAM_ISP_OTF_INPUT, pmap))
		memcpy(&param_set->otf_input, &param->otf_input,
		       sizeof(struct param_otf_input));

	if (test_bit(PARAM_ISP_VDMA1_INPUT, pmap))
		memcpy(&param_set->dma_input, &param->vdma1_input,
		       sizeof(struct param_dma_input));

	if (test_bit(PARAM_ISP_VDMA2_INPUT, pmap))
		memcpy(&param_set->prev_wgt_dma_input, &param->vdma2_input,
		       sizeof(struct param_dma_input));
#if IS_ENABLED(USE_MCFP_MOTION_INTERFACE)
	if (test_bit(PARAM_ISP_MOTION_DMA_INPUT, pmap))
		memcpy(&param_set->motion_dma_input, &param->motion_dma_input,
		       sizeof(struct param_dma_input));

	if (test_bit(PARAM_ISP_DRC_INPUT, pmap))
		memcpy(&param_set->dma_input_drcgrid, &param->drc_input,
		       sizeof(struct param_dma_output));
#endif
	if (test_bit(PARAM_ISP_VDMA6_OUTPUT, pmap))
		memcpy(&param_set->dma_output_tnr_prev, &param->vdma6_output,
		       sizeof(struct param_dma_output));

	if (test_bit(PARAM_ISP_VDMA7_OUTPUT, pmap))
		memcpy(&param_set->dma_output_tnr_wgt, &param->vdma7_output,
		       sizeof(struct param_dma_output));

	if (test_bit(PARAM_ISP_VDMA3_INPUT, pmap))
		memcpy(&param_set->prev_dma_input, &param->vdma3_input,
		       sizeof(struct param_dma_input));

	/* check output*/
	if (test_bit(PARAM_ISP_OTF_OUTPUT, pmap))
		memcpy(&param_set->otf_output, &param->otf_output,
		       sizeof(struct param_otf_output));

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && test_bit(PARAM_ISP_STRIPE_INPUT, pmap))
		memcpy(&param_set->stripe_input, &param->stripe_input,
		       sizeof(struct param_stripe_input));
}

static int __is_hw_isp_allocate_internal_buffer(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, struct is_frame *frame)
{
	int ret;
	bool alloc_img_flag, alloc_wgt_flag;
	struct is_device_ischain *device;

	device = hw_ip->group[instance]->device;

	if (CHK_MFSTILL_MODE(param_set->tnr_mode)) {
		/* mfstill */
		alloc_img_flag = false;
		alloc_wgt_flag = true;
	} else if (CHK_VIDEOTNR_MODE(param_set->tnr_mode)) {
		/* video tnr */
		alloc_img_flag = true;
		alloc_wgt_flag = true;
	} else {
		/* tnr off */
		alloc_img_flag = false;
		alloc_wgt_flag = false;
	}

	/* Image */
	if (alloc_img_flag && device->ixv.internal_framemgr.num_frames == 0) {
		ret = is_subdev_internal_start(device, IS_DEVICE_ISCHAIN, &device->ixv);
		if (ret) {
			merr("subdev internal start is fail(%d)", device, ret);
			return ret;
		}
	} else if (!alloc_img_flag && device->ixv.internal_framemgr.num_frames > 0) {
		ret = is_subdev_internal_stop(device, IS_DEVICE_ISCHAIN, &device->ixv);
		if (ret) {
			merr("subdev internal stop is fail(%d)", device, ret);
			return ret;
		}
	}
	/* Weight */
	if (alloc_wgt_flag && device->ixw.internal_framemgr.num_frames == 0) {
		ret = is_subdev_internal_start(device, IS_DEVICE_ISCHAIN, &device->ixw);
		if (ret) {
			merr("subdev internal start is fail(%d)", device, ret);
			return ret;
		}
	} else if (!alloc_wgt_flag && device->ixw.internal_framemgr.num_frames > 0) {
		ret = is_subdev_internal_stop(device, IS_DEVICE_ISCHAIN, &device->ixw);
		if (ret) {
			merr("subdev internal stop is fail(%d)", device, ret);
			return ret;
		}
	}

	return 0;
}

static int __is_hw_isp_update_internal_param(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, struct is_frame *frame)
{
	int ret;
	struct is_device_ischain *device;
	struct is_hw_isp *hw_isp;
	struct is_isp_config *iq_config;
	struct is_frame *prev_img_frame = NULL, *prev_wgt_frame = NULL;
	struct is_frame *cur_img_frame = NULL, *cur_wgt_frame = NULL;
	u32 ixtTargetAddress[IS_MAX_PLANES] = {0, };
	u32 ixvTargetAddress[IS_MAX_PLANES] = {0, };
	u32 ixgTargetAddress[IS_MAX_PLANES] = {0, };
	u32 ixwTargetAddress[IS_MAX_PLANES] = {0, };
	u32 cur_idx, buf_i;
	bool stripe_enable = false;
	u32 stripe_idx = 0;
	u32 cmd_wgt_out, cmd_prev_wgt_in;
	u32 cmd_yuv_out, cmd_prev_yuv_in;

	device = hw_ip->group[instance]->device;
	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	iq_config = &hw_isp->iq_config[instance];
	cur_idx = frame->cur_buf_index;
	stripe_enable = param_set->stripe_input.total_count >= 2 ? true : false;
	stripe_idx = param_set->stripe_input.index;

	/* DRC GRID */
	if (iq_config->drc_dstr_bypass)
		param_set->dma_input_drcgrid.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	else {
		param_set->dma_input_drcgrid.width = iq_config->drc_dstr_grid_x;
		param_set->dma_input_drcgrid.height = iq_config->drc_dstr_grid_y;
	}

	cmd_wgt_out = param_set->dma_output_tnr_wgt.cmd;
	cmd_prev_wgt_in = param_set->prev_wgt_dma_input.cmd;
	cmd_yuv_out = param_set->dma_output_tnr_prev.cmd;
	cmd_prev_yuv_in = param_set->prev_dma_input.cmd;

	__is_hw_isp_set_dma_cmd(hw_ip, &hw_isp->param_set[instance], instance, iq_config);

	if (iq_config->mixer_en) {
		if (cmd_wgt_out && cmd_prev_wgt_in) {
			msdbg_hw(1, "[MCFP] Use wgt buffer from user\n", instance, hw_ip);

			for (buf_i = 0; buf_i < frame->num_buffers; buf_i++) {
				ixgTargetAddress[buf_i] = frame->ixgTargetAddress[buf_i];
				ixwTargetAddress[buf_i] = frame->ixwTargetAddress[buf_i];
			}
		} else {
			/* Get internal buffer address for weight */
			ret = __is_hw_isp_get_mcfp_internal_frame(hw_ip, instance, frame,
					&device->ixw,
					&prev_wgt_frame, &cur_wgt_frame, "wgt");
			if (ret) {
				mserr_hw("[MCFP] internal wgt frame is NULL", instance, hw_ip);
				return -EINVAL;
			}

			ixgTargetAddress[0] = (u32)prev_wgt_frame->dvaddr_buffer[0];
			ixwTargetAddress[0] = (u32)cur_wgt_frame->dvaddr_buffer[0];
			/*
			* Case 1 -  1/2 dma in video tnr normal mode
			* Case 2 -  0 ~ N-2 frame in N strip procssing. (swap in last striped-frame)
			*/
			if ((iq_config->skip_wdma && iq_config->tnr_mode == TNR_MODE_NORMAL) ||
			(stripe_enable && (stripe_idx < (param_set->stripe_input.total_count - 1)))) {
				/* No swap */
				/* for HW FRO */
				for (buf_i = 1; buf_i < frame->num_buffers; buf_i++) {
					ixgTargetAddress[buf_i] = (u32)prev_wgt_frame->dvaddr_buffer[0];
					ixwTargetAddress[buf_i] = (u32)cur_wgt_frame->dvaddr_buffer[0];
				}
			} else {
				/* Swap current and previous buffer address for next frame*/
				SWAP(prev_wgt_frame->dvaddr_buffer[0], cur_wgt_frame->dvaddr_buffer[0], u32);

				/* for HW FRO */
				for (buf_i = 1; buf_i < frame->num_buffers; buf_i++) {
					ixgTargetAddress[buf_i] = (u32)prev_wgt_frame->dvaddr_buffer[0];
					ixwTargetAddress[buf_i] = (u32)cur_wgt_frame->dvaddr_buffer[0];
					/* Swap current and previous buffer address for next frame*/
					SWAP(prev_wgt_frame->dvaddr_buffer[0], cur_wgt_frame->dvaddr_buffer[0], u32);
				}
			}
			param_set->prev_wgt_dma_input.plane = prev_wgt_frame->planes;
			param_set->dma_output_tnr_wgt.plane = cur_wgt_frame->planes;
		}

		if (iq_config->still_en || (cmd_yuv_out && cmd_prev_yuv_in)) {
			/* MF-Still */
			msdbg_hw(1, "[MCFP] Use img buffer from user\n", instance, hw_ip);

			for (buf_i = 0; buf_i < frame->num_buffers; buf_i++) {
				ixtTargetAddress[buf_i] = frame->ixtTargetAddress[buf_i];
				ixvTargetAddress[buf_i] = frame->ixvTargetAddress[buf_i];
			}
		} else {
			/*
			 * Video TNR
			 * Get internal buffer address for image
			 */
			ret = __is_hw_isp_get_mcfp_internal_frame(hw_ip, instance, frame,
				&device->ixv,
				&prev_img_frame, &cur_img_frame, "img");
			if (ret) {
				mserr_hw("[MCFP] internal img frame is NULL", instance, hw_ip);
				return -EINVAL;
			}
			ixtTargetAddress[0] = (u32)prev_img_frame->dvaddr_buffer[0];
			ixvTargetAddress[0] = (u32)cur_img_frame->dvaddr_buffer[0];

			/*
			 * Case 1 -  1/2 dma in video tnr normal mode
			 * Case 2 -  0 ~ N-2 frame in N strip procssing. (swap in last striped-frame)
			 */
			if ((iq_config->skip_wdma && iq_config->tnr_mode == TNR_MODE_NORMAL) ||
				(stripe_enable && (stripe_idx < (param_set->stripe_input.total_count - 1)))) {
				/*
				 * No swap
				 * for HW FRO
				 */
				for (buf_i = 1; buf_i < frame->num_buffers; buf_i++) {
					ixtTargetAddress[buf_i] = (u32)prev_img_frame->dvaddr_buffer[0];
					ixvTargetAddress[buf_i] = (u32)cur_img_frame->dvaddr_buffer[0];
				}
			} else {
				/* Swap current and previous buffer address for next frame*/
				SWAP(prev_img_frame->dvaddr_buffer[0], cur_img_frame->dvaddr_buffer[0], u32);

				/* for HW FRO */
				for (buf_i = 1; buf_i < frame->num_buffers; buf_i++) {
					ixtTargetAddress[buf_i] = (u32)prev_img_frame->dvaddr_buffer[0];
					ixvTargetAddress[buf_i] = (u32)cur_img_frame->dvaddr_buffer[0];
					SWAP(prev_img_frame->dvaddr_buffer[0], cur_img_frame->dvaddr_buffer[0], u32);
				}
			}
			param_set->prev_dma_input.plane = prev_img_frame->planes;
			param_set->dma_output_tnr_prev.plane = cur_img_frame->planes;
			cur_idx = 0;
		}
	}

#if IS_ENABLED(SOC_TNR_MERGER)
	/* TNR prev image input */
	is_hardware_dma_cfg_32bit("tnr_prev_in", hw_ip,
		frame, cur_idx, frame->num_buffers,
		&param_set->prev_dma_input.cmd,
		param_set->prev_dma_input.plane,
		param_set->input_dva_tnr_prev,
		ixtTargetAddress);

	/* TNR prev weight input */
	is_hardware_dma_cfg_32bit("tnr_wgt_in", hw_ip,
		frame, 0, frame->num_buffers,
		&param_set->prev_wgt_dma_input.cmd,
		param_set->prev_wgt_dma_input.plane,
		param_set->input_dva_tnr_wgt,
		ixgTargetAddress);

	is_hardware_dma_cfg_32bit("tnr_prev_out", hw_ip,
		frame, cur_idx, frame->num_buffers,
		&param_set->dma_output_tnr_prev.cmd,
		param_set->dma_output_tnr_prev.plane,
		param_set->output_dva_tnr_prev,
		ixvTargetAddress);

	is_hardware_dma_cfg_32bit("tnr_wgt_out", hw_ip,
		frame, 0, frame->num_buffers,
		&param_set->dma_output_tnr_wgt.cmd,
		param_set->dma_output_tnr_wgt.plane,
		param_set->output_dva_tnr_wgt,
		ixwTargetAddress);
#endif
	return 0;
}

static int __is_hw_isp_set_rta_regs(struct is_hw_ip *hw_ip, u32 set_id, struct is_hw_isp *hw_isp, u32 instance)
{
	struct is_hw_isp_iq *iq_set;
	struct is_hw_isp_iq *cur_hw_iq_set;
	struct cr_set *regs;
	unsigned long flag;
	enum base_reg_index base_reg;
	enum is_hw_isp_reg_idx reg_idx;
	u32 regs_size;
	int i, ret = 0;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	for (reg_idx = ISP_REG_ITP; reg_idx < ISP_REG_MAX; reg_idx++) {
		iq_set = &hw_isp->iq_set[instance][reg_idx];
		cur_hw_iq_set = &hw_isp->cur_hw_iq_set[set_id][reg_idx];
		base_reg = __is_hw_isp_get_reg_base(reg_idx);

		spin_lock_irqsave(&iq_set->slock, flag);

		if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
			regs = iq_set->regs;
			regs_size = iq_set->size;

			if (cur_hw_iq_set->size != regs_size ||
				memcmp(cur_hw_iq_set->regs, regs,
					sizeof(struct cr_set) * regs_size) != 0) {

				if (unlikely(test_bit(ISP_DEBUG_SET_REG, &debug_isp))) {
					for (i = 0; i < iq_set->size; i++)
						info_hw("%s: [%d][0x%08x] 0x%08x\n",
							__func__, reg_idx,
							regs[i].reg_addr, regs[i].reg_data);
				}

				for (i = 0; i < regs_size; i++) {
					writel_relaxed(regs[i].reg_data, hw_ip->regs[base_reg] +
						GET_COREX_OFFSET(set_id) + regs[i].reg_addr);
				}

				memcpy(cur_hw_iq_set->regs, regs,
					sizeof(struct cr_set) * regs_size);
				cur_hw_iq_set->size = regs_size;
			}

			set_bit(CR_SET_EMPTY, &iq_set->state);
		} else {
			ret = 1;
		}

		spin_unlock_irqrestore(&iq_set->slock, flag);
	}

	return ret;
}

static int __is_hw_isp_set_size(struct is_hw_ip *hw_ip, struct isp_param_set *param_set,
	u32 instance, const struct is_frame *frame, u32 set_id)
{
	struct is_device_ischain *device;
	struct is_param_region *param_region;
	struct is_hw_isp *hw_isp;
	struct is_isp_config *iq_config;
	struct is_hw_size_config isp_config;
	struct is_rectangle drc_grid;
	struct param_stripe_input *stripe;
	u32 stripe_enable;
	enum base_reg_index itp_base;
	enum base_reg_index dns_base;
	enum base_reg_index mcfp0_base, mcfp1_base;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	itp_base = __is_hw_isp_get_reg_base(ISP_REG_ITP);
	dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);
	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
	mcfp1_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP1);

	device = hw_ip->group[instance]->device;
	param_region = &device->is_region->parameter;
	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	iq_config = &hw_isp->iq_config[instance];
	stripe = &param_set->stripe_input;
	stripe_enable = (stripe->total_count == 0) ? 0 : 1;

	isp_config.sensor_calibrated_width = frame->shot->udm.frame_info.sensor_size[0];
	isp_config.sensor_calibrated_height = frame->shot->udm.frame_info.sensor_size[1];
	if ((iq_config->sensor_center_x < isp_config.sensor_calibrated_width) &&
	    iq_config->sensor_center_x)
		isp_config.sensor_center_x = iq_config->sensor_center_x;
	else
		isp_config.sensor_center_x = isp_config.sensor_calibrated_width / 2;
	if ((iq_config->sensor_center_y < isp_config.sensor_calibrated_height) &&
	    iq_config->sensor_center_y)
		isp_config.sensor_center_y = iq_config->sensor_center_y;
	else
		isp_config.sensor_center_y = isp_config.sensor_calibrated_height / 2;
	isp_config.sensor_binning_x = frame->shot->udm.frame_info.sensor_binning[0];
	isp_config.sensor_binning_y = frame->shot->udm.frame_info.sensor_binning[1];
	isp_config.sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	isp_config.sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	isp_config.sensor_crop_width = frame->shot->udm.frame_info.sensor_crop_region[2];
	isp_config.sensor_crop_height = frame->shot->udm.frame_info.sensor_crop_region[3];
	isp_config.bcrop_x = frame->shot->udm.frame_info.taa_in_crop_region[0];
	isp_config.bcrop_y = frame->shot->udm.frame_info.taa_in_crop_region[1];
	isp_config.bcrop_width = frame->shot->udm.frame_info.taa_in_crop_region[2];
	isp_config.bcrop_height = frame->shot->udm.frame_info.taa_in_crop_region[3];
	isp_config.taasc_width = (stripe_enable) ?
		stripe->full_width : param_set->dma_input.width;
	isp_config.width = param_set->dma_input.width;
	isp_config.height = param_set->dma_input.height;
	isp_config.dma_crop_x = (stripe_enable) ?
		stripe->stripe_roi_start_pos_x[stripe->index] - stripe->left_margin : 0;
	drc_grid.w = iq_config->drc_dstr_grid_x;
	drc_grid.h = iq_config->drc_dstr_grid_y;

	msdbg_hw(2, "[SENSOR] %dx%d (center %d, %d) -> binning %dx%d -> crop %d, %d, %dx%d",
		instance, hw_ip,
		isp_config.sensor_calibrated_width, isp_config.sensor_calibrated_height,
		isp_config.sensor_center_x, isp_config.sensor_center_y,
		isp_config.sensor_binning_x, isp_config.sensor_binning_y,
		isp_config.sensor_crop_x, isp_config.sensor_crop_y,
		isp_config.sensor_crop_width, isp_config.sensor_crop_height);
	msdbg_hw(2, "[BCROP] %d, %d, %dx%d",
		instance, hw_ip,
		isp_config.bcrop_x, isp_config.bcrop_x,
		isp_config.bcrop_width, isp_config.bcrop_height);
	msdbg_hw(2, "[3AASC] %dx%d -> [DMA] %d, 0, %dx%d",
		instance, hw_ip,
		isp_config.taasc_width, isp_config.height,
		isp_config.dma_crop_x,
		isp_config.width, isp_config.height);
	msdbg_hw(2, "[DRCDSTR] %d %dx%d",
		instance, hw_ip,
		!iq_config->drc_dstr_bypass, drc_grid.w, drc_grid.h);

	dns_hw_s_module_size(hw_ip->regs[dns_base], set_id, &isp_config, &drc_grid);
	itp_hw_s_module_size(hw_ip->regs[itp_base], set_id, &isp_config);

	mcfp0_hw_s_module_size(hw_ip->regs[mcfp0_base], set_id, &hw_isp->iq_config[instance],
			       &param_set->stripe_input, isp_config.width, isp_config.height);
	mcfp1_hw_s_module_size(hw_ip->regs[mcfp1_base], set_id, &isp_config, &param_set->stripe_input);

	return 0;
}

static void is_hw_isp_handle_start_interrupt(struct is_hw_ip *hw_ip, struct is_hw_isp *hw_isp, u32 instance)
{
	struct is_hardware *hardware;
	struct isp_param_set *param_set;
	u32 hw_fcount;
	u32 strip_index, strip_total_count;
	u32 cur_idx, batch_num;
	bool is_first_shot;

	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	param_set = &hw_isp->param_set[instance];
	strip_index = param_set->stripe_input.index;
	strip_total_count = param_set->stripe_input.total_count;
	cur_idx = (hw_ip->num_buffers >> CURR_INDEX_SHIFT) & 0xff;
	batch_num = (hw_ip->num_buffers >> SW_FRO_NUM_SHIFT) & 0xff;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (!batch_num && !strip_total_count) {
			/* normal shot */
			is_first_shot = true;
		} else {
			/* SW batch mode or Stripe processing */
			if ((batch_num && !cur_idx) || (strip_total_count && !strip_index))
				is_first_shot = true;
			else
				is_first_shot = false;
		}
	} else {
		is_first_shot = true;
	}

	if (IS_ENABLED(ISP_DDK_LIB_CALL) && is_first_shot) {
		is_lib_isp_event_notifier(hw_ip, &hw_isp->lib[instance],
								instance, hw_fcount, EVENT_FRAME_START, 0, NULL);
	} else {
		atomic_add(1, &hw_ip->count.fs);
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);

		if (unlikely(!atomic_read(&hardware->streaming[hardware->sensor_position[instance]])))
			msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

		is_hardware_frame_start(hw_ip, instance);
	}
}

static void is_hw_isp_handle_end_interrupt(struct is_hw_ip *hw_ip, struct is_hw_isp *hw_isp, u32 instance, u32 status)
{
	struct is_hardware *hardware;
	struct isp_param_set *param_set;
	u32 hw_fcount;
	u32 strip_index, strip_total_count;
	u32 cur_idx, batch_num;
	bool is_last_shot;

	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	param_set = &hw_isp->param_set[instance];
	strip_index = param_set->stripe_input.index;
	strip_total_count = param_set->stripe_input.total_count;
	cur_idx = (hw_ip->num_buffers >> CURR_INDEX_SHIFT) & 0xff;
	batch_num = (hw_ip->num_buffers >> SW_FRO_NUM_SHIFT) & 0xff;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (!batch_num && !strip_total_count) {
			/* normal shot */
			is_last_shot = true;
		} else {
			/* SW batch mode or Stripe processing */
			if ((batch_num && (batch_num - 1 == cur_idx)) ||
			(strip_total_count && (strip_total_count - 1 == strip_index)))
				is_last_shot = true;
			else
				is_last_shot = false;
		}
	} else {
		is_last_shot = true;
	}

	if (IS_ENABLED(ISP_DDK_LIB_CALL) && is_last_shot) {
		is_lib_isp_event_notifier(hw_ip, &hw_isp->lib[instance],
									instance, hw_fcount, EVENT_FRAME_END, 0, NULL);
	} else {
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_END);
		atomic_add(1, &hw_ip->count.fe);

		is_hardware_frame_done(hw_ip, NULL, -1, IS_HW_CORE_END,
							IS_SHOT_SUCCESS, true);

		if (unlikely(!atomic_read(&hardware->streaming[hardware->sensor_position[instance]])))
			msinfo_hw("[F:%d]F.E\n", instance, hw_ip, hw_fcount);

		atomic_set(&hw_ip->status.Vvalid, V_BLANK);
		if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
			mserr_hw("fs(%d), fe(%d), dma(%d), status(0x%x)",
					instance, hw_ip,
					atomic_read(&hw_ip->count.fs),
					atomic_read(&hw_ip->count.fe),
					atomic_read(&hw_ip->count.dma),
					status);
		}
		wake_up(&hw_ip->status.wait_queue);
	}
}

static int is_hw_isp_handle_mcfp_interrupt(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_isp *hw_isp;
	struct is_interface_hwip *itf_hw;
	u32 status, instance, hw_fcount;
	u32 f_err;
	int hw_slot;
	enum base_reg_index mcfp0_base, mcfp1_base;

	hw_ip = (struct is_hw_ip *)context;
	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	if (!hw_isp) {
		err("failed to get ISP");
		return 0;
	}
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	hw_slot = is_hw_slot_id(hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return IRQ_NONE;
	}

	itf_hw = &hw_ip->itfc->itf_ip[hw_slot];

	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
	mcfp1_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP1);

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	status = mcfp0_hw_g_int_status(hw_ip->regs[mcfp0_base], id, true, hw_isp->hw_fro_en);

	msdbg_hw(2, "MCFP interrupt %d status(0x%x)\n", instance, hw_ip, id, status);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("[MCFP] invalid interrupt: 0x%x", instance, hw_ip, status);
		return 0;
	}

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[MCFP] HW disabled!! interrupt(0x%x)", instance, hw_ip, status);
		return 0;
	}

	if (mcfp0_hw_is_occurred(status, id, MCFP_EVENT_FRAME_START)
		&& mcfp0_hw_is_occurred(status, id, MCFP_EVENT_FRAME_END))
		mswarn_hw("[MCFP] start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (mcfp0_hw_is_occurred(status, id, MCFP_EVENT_FRAME_START)) {
		msdbg_hw(1, "[F:%d] MCFP F.S\n", instance, hw_ip, hw_fcount);
		_is_hw_frame_dbg_ext_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_START, 0);
		if (atomic_add_return(1, &hw_isp->count.fs) == atomic_read(&hw_isp->count.max_cnt))
			is_hw_isp_handle_start_interrupt(hw_ip, hw_isp, instance);
	}

	if (mcfp0_hw_is_occurred(status, id, MCFP_EVENT_FRAME_END)) {
		msdbg_hw(1, "[F:%d] MCFP F.E\n", instance, hw_ip, hw_fcount);
		_is_hw_frame_dbg_ext_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_END, 0);
		if (atomic_add_return(1, &hw_isp->count.fe) == atomic_read(&hw_isp->count.max_cnt))
			is_hw_isp_handle_end_interrupt(hw_ip, hw_isp, instance, status);
	}

	f_err = mcfp0_hw_is_occurred(status, id, MCFP_EVENT_ERR);
	if (f_err) {
		msinfo_hw("[MCFP][F:%d] Ocurred error interrupt status(0x%x), irq_id(%d)\n",
			instance, hw_ip, hw_fcount, status, id);
		mcfp0_hw_dump(hw_ip->regs[mcfp0_base]);
		mcfp1_hw_dump(hw_ip->regs[mcfp1_base]);
	}

	return 0;
}

static int is_hw_isp_handle_dns_interrupt(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_isp *hw_isp;
	struct is_interface_hwip *itf_hw;
	u32 status, instance, hw_fcount;
	u32 f_err;
	int hw_slot;
	enum base_reg_index itp_base;
	enum base_reg_index dns_base;

	hw_ip = (struct is_hw_ip *)context;
	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	if (!hw_isp) {
		err("failed to get ISP");
		return 0;
	}
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	hw_slot = is_hw_slot_id(hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return IRQ_NONE;
	}

	itf_hw = &hw_ip->itfc->itf_ip[hw_slot];

	itp_base = __is_hw_isp_get_reg_base(ISP_REG_ITP);
	dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	status = dns_hw_g_int_status(hw_ip->regs[dns_base], id, true, hw_isp->hw_fro_en);

	msdbg_hw(2, "ISP interrupt status(0x%x)\n", instance, hw_ip, status);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt: 0x%x", instance, hw_ip, status);
		return 0;
	}

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("HW disabled!! interrupt(0x%x)", instance, hw_ip, status);
		return 0;
	}

	if (dns_hw_is_occurred(status, id, DNS_EVENT_FRAME_START) &&
		dns_hw_is_occurred(status, id, DNS_EVENT_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (dns_hw_is_occurred(status, id, DNS_EVENT_FRAME_START)) {
		msdbg_hw(1, "[F:%d] DNS F.S\n", instance, hw_ip, hw_fcount);
		_is_hw_frame_dbg_ext_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_START, 1);
		if (atomic_add_return(1, &hw_isp->count.fs) == atomic_read(&hw_isp->count.max_cnt))
			is_hw_isp_handle_start_interrupt(hw_ip, hw_isp, instance);
	}

	if (dns_hw_is_occurred(status, id, DNS_EVENT_FRAME_END)) {
		msdbg_hw(1, "[F:%d] DNS F.E\n", instance, hw_ip, hw_fcount);
		_is_hw_frame_dbg_ext_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_END, 1);
		dns_hw_s_disable(hw_ip->regs[dns_base]);
		if (atomic_add_return(1, &hw_isp->count.fe) == atomic_read(&hw_isp->count.max_cnt))
			is_hw_isp_handle_end_interrupt(hw_ip, hw_isp, instance, status);
	}

	f_err = dns_hw_is_occurred(status, id, DNS_EVENT_ERR);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt status(0x%x)\n",
			instance, hw_ip, hw_fcount, status);
		itp_hw_dump(hw_ip->regs[itp_base]);
		dns_hw_dump(hw_ip->regs[dns_base]);
	}

	return 0;
}

static int __nocfi is_hw_isp_open(struct is_hw_ip *hw_ip, u32 instance, struct is_group *group)
{
	int ret, i;
	struct is_hw_isp *hw_isp;
	enum is_hw_isp_reg_idx reg_idx;
	u32 reg_cnt;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWISP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_isp));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;

	memset(hw_isp->iq_set, 0, sizeof(struct is_hw_isp_iq) * ISP_REG_MAX * IS_STREAM_COUNT);
	memset(hw_isp->cur_hw_iq_set, 0, sizeof(struct is_hw_isp_iq) * ISP_REG_MAX * COREX_MAX);

	for (reg_idx = ISP_REG_ITP; reg_idx < ISP_REG_MAX; reg_idx++) {
		if (__is_hw_isp_get_reg_cnt(hw_ip, instance, reg_idx, &reg_cnt)) {
			mserr_hw("[I%d] failed to get reg cnt", instance, hw_ip, reg_idx);
			continue;
		}

		/* init iq_set */
		for (i = 0; i < IS_STREAM_COUNT; i++) {
			hw_isp->iq_set[i][reg_idx].regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
			if (!hw_isp->iq_set[i][reg_idx].regs) {
				mserr_hw("failed to alloc iq_set.regs", instance, hw_ip);
				ret = -ENOMEM;
				goto err_regs_alloc;
			}
			clear_bit(CR_SET_CONFIG, &hw_isp->iq_set[i][reg_idx].state);
			set_bit(CR_SET_EMPTY, &hw_isp->iq_set[i][reg_idx].state);
			spin_lock_init(&hw_isp->iq_set[i][reg_idx].slock);
		}

		/* init cur_hw_iq_set */
		for (i = 0; i < COREX_MAX; i++) {
			hw_isp->cur_hw_iq_set[i][reg_idx].regs =
				vzalloc(sizeof(struct cr_set) * reg_cnt);
			if (!hw_isp->cur_hw_iq_set[i][reg_idx].regs) {
				mserr_hw("failed to alloc cur_hw_iq_set regs", instance, hw_ip);
				ret = -ENOMEM;
				goto err_regs_alloc;
			}
		}
	}

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		ret = get_lib_func(LIB_FUNC_ISP, (void **)&hw_isp->lib_func);
		if (!hw_isp->lib_func) {
			mserr_hw("hw_isp->lib_func(null)", instance, hw_ip);
			is_load_clear();
			ret = -EINVAL;
			goto err_lib_func;
		}
		msinfo_hw("get_lib_func is set\n", instance, hw_ip);
	}

	hw_isp->lib[instance].func = hw_isp->lib_func;
	if (IS_ENABLED(ISP_DDK_LIB_CALL)) {
		ret = is_lib_isp_chain_create(hw_ip, &hw_isp->lib[instance], instance);
		if (ret) {
			mserr_hw("chain create fail", instance, hw_ip);
			ret = -EINVAL;
			goto err_chain_create;
		}
	}

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: [G:0x%lx], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;

err_regs_alloc:
	for (reg_idx = ISP_REG_ITP; reg_idx < ISP_REG_MAX; reg_idx++) {
		for (i = 0; i < IS_STREAM_COUNT; i++) {
			if (hw_isp->iq_set[i][reg_idx].regs) {
				vfree(hw_isp->iq_set[i][reg_idx].regs);
				hw_isp->iq_set[i][reg_idx].regs = NULL;
			}
		}
		for (i = 0; i < COREX_MAX; i++) {
			if (hw_isp->cur_hw_iq_set[i][reg_idx].regs) {
				vfree(hw_isp->cur_hw_iq_set[i][reg_idx].regs);
				hw_isp->cur_hw_iq_set[i][reg_idx].regs = NULL;
			}
		}
	}
err_chain_create:
err_lib_func:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_isp_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag)
{
	int ret;
	struct is_device_ischain *device;
	struct is_hw_isp *hw_isp;
	u32 idx, f_type;
	enum base_reg_index dns_base;
	enum base_reg_index mcfp0_base;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	if (!hw_isp) {
		mserr_hw("hw_isp is null ", instance, hw_ip);
		return -ENODATA;
	}

	hw_isp->lib[instance].object = NULL;
	hw_isp->lib[instance].func = hw_isp->lib_func;

	if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
		f_type = LIB_FRAME_HDR_SHORT;
	else if (is_sensor_g_aeb_mode(device->sensor))
		f_type = LIB_FRAME_HDR_LONG;
	else
		f_type = LIB_FRAME_HDR_SINGLE;

	if (hw_isp->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else {
		if (IS_ENABLED(ISP_DDK_LIB_CALL)) {
			ret = is_lib_isp_object_create(hw_ip,
				&hw_isp->lib[instance], instance,
				(u32)flag, f_type);
			if (ret) {
				mserr_hw("object create fail", instance, hw_ip);
				return -EINVAL;
			}
		}
	}

	dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);
	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);

	/* create MCFP RDMA */
	for (idx = MCFP_RDMA_CURR_BAYER; idx < MCFP_RDMA_MAX; idx++) {
		ret = mcfp0_hw_create_rdma(&hw_isp->mcfp_rdma[idx], hw_ip->regs[mcfp0_base], idx);
		if (ret) {
			mserr_hw("mcfp0_hw_create_rdma error[%d]", instance, hw_ip, idx);
			return ret;
		}
	}
	/* create MCFP WDMA */
	for (idx = MCFP_WDMA_PREV_BAYER; idx < MCFP_WDMA_MAX; idx++) {
		ret = mcfp0_hw_create_wdma(&hw_isp->mcfp_wdma[idx], hw_ip->regs[mcfp0_base], idx);
		if (ret) {
			mserr_hw("mcfp0_hw_create_wdma error[%d]", instance, hw_ip, idx);
			return ret;
		}
	}
	/* create DNS DMA */
	for (idx = DNS_RDMA_BAYER; idx < DNS_DMA_MAX; idx++) {
		ret = dns_hw_create_dma(hw_ip->regs[dns_base], idx, &hw_isp->dns_dma[idx]);
		if (ret) {
			mserr_hw("dns_hw_create_dma error[%d]", instance, hw_ip, idx);
			return -ENODATA;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);

	return 0;
}

static int is_hw_isp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_isp *hw_isp;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;

	if (IS_ENABLED(ISP_DDK_LIB_CALL)) {
		is_lib_isp_object_destroy(hw_ip, &hw_isp->lib[instance], instance);
		hw_isp->lib[instance].object = NULL;
	}

	return 0;
}

static int is_hw_isp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_isp *hw_isp;
	int res;
	u32 i;
	enum base_reg_index dns_base, mcfp0_base;
	enum is_hw_isp_reg_idx reg_idx;

	msdbg_hw(2, "[HW][%s]%d: is called\n", instance, hw_ip, __func__, __LINE__);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;

	if (IS_ENABLED(ISP_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_isp->lib[instance], instance);


	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
	res = mcfp0_hw_s_reset(hw_ip->regs[mcfp0_base]);
	if (res)
		mserr_hw("mcfp hw sw reset fail", instance, hw_ip);

	dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);
	res = dns_hw_s_reset(hw_ip->regs[dns_base]);
	if (res)
		mserr_hw("dns_hw_s_reset fail", instance, hw_ip);

	for (reg_idx = ISP_REG_ITP; reg_idx < ISP_REG_MAX; reg_idx++) {
		for (i = 0; i < IS_STREAM_COUNT; i++) {
			if (hw_isp->iq_set[i][reg_idx].regs) {
				vfree(hw_isp->iq_set[i][reg_idx].regs);
				hw_isp->iq_set[i][reg_idx].regs = NULL;
			}
		}
		for (i = 0; i < COREX_MAX; i++) {
			if (hw_isp->cur_hw_iq_set[i][reg_idx].regs) {
				vfree(hw_isp->cur_hw_iq_set[i][reg_idx].regs);
				hw_isp->cur_hw_iq_set[i][reg_idx].regs = NULL;
			}
		}
	}
	vfree(hw_isp);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return 0;
}

static int is_hw_isp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret;
	struct is_hw_isp *hw_isp;
	struct is_device_ischain *device;
	u32 width, height;
	u32 i, j;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	device = hw_ip->group[instance]->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	width = device->group_isp.leader.input.width;
	height = device->group_isp.leader.input.height;
	msdbg_hw(2, "MCFP internal buffer info (w: %d, h: %d)\n", instance, hw_ip,
		width, height);

	/* mcfp previous image (r/w) */
	if (!test_bit(IS_SUBDEV_OPEN, &device->ixv.state)) {
		ret = is_subdev_internal_open(device, IS_DEVICE_ISCHAIN, device->group_isp.leader.vid, &device->ixv);
		if (ret) {
			merr("is_subdev_internal_open is fail(%d)", device, ret);
			return ret;
		}
	} else {
		set_bit(IS_SUBDEV_INTERNAL_USE, &device->ixv.state);
	}

	ret = is_subdev_internal_s_format(device, IS_DEVICE_ISCHAIN, &device->ixv,
			width, height, DMA_INOUT_BIT_WIDTH_13BIT, COMP, 0, 2, "MCFP PREV IMG");
	if (ret) {
		merr("is_subdev_internal_s_format is fail(%d)", device, ret);
		return ret;
	}

	/* mcfp previous weight (r/w) */
	if (!test_bit(IS_SUBDEV_OPEN, &device->ixw.state)) {
		ret = is_subdev_internal_open(device, IS_DEVICE_ISCHAIN, device->group_isp.leader.vid, &device->ixw);
		if (ret) {
			merr("is_subdev_internal_open is fail(%d)", device, ret);
			return ret;
		}
	} else {
		set_bit(IS_SUBDEV_INTERNAL_USE, &device->ixw.state);
	}

	ret = is_subdev_internal_s_format(device, IS_DEVICE_ISCHAIN, &device->ixw,
			width, height, DMA_INOUT_BIT_WIDTH_8BIT, NONE, 0, 2, "MCFP PREV WGT");
	if (ret) {
		merr("is_subdev_internal_s_format is fail(%d)", device, ret);
		return ret;
	}

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	memset(hw_isp->input_dva_tnr_ref[instance], 0, sizeof(hw_isp->input_dva_tnr_ref[instance]));

	atomic_inc(&hw_ip->run_rsccount);

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	__is_hw_isp_init(hw_ip, instance);

	memset(&hw_isp->iq_config[instance], 0x0, sizeof(struct is_isp_config));
	for (i = 0; i < COREX_MAX; i++)
		for (j = 0; j < ISP_REG_MAX; j++)
			hw_isp->cur_hw_iq_set[i][j].size = 0;

	set_bit(HW_RUN, &hw_ip->state);

	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int is_hw_isp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_isp *hw_isp;
	struct isp_param_set *param_set;
	struct is_device_ischain *device;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("isp_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
			!atomic_read(&hw_ip->status.Vvalid),
			IS_HW_STOP_TIMEOUT);

	if (!timetowait)
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);

	/* mcfp internal buffer free */
	device = hw_ip->group[instance]->device;
	if (device->ixv.internal_framemgr.num_frames > 0) {
		ret = is_subdev_internal_stop(device, IS_DEVICE_ISCHAIN, &device->ixv);
		if (ret)
			mswarn_hw("subdev internal stop is fail(%d)", instance, hw_ip, ret);
	}
	ret = is_subdev_internal_close(device, IS_DEVICE_ISCHAIN, &device->ixv);
	if (ret)
		mserr_hw("is_subdev_internal_close is failed(%d)", instance, hw_ip, ret);

	if (device->ixw.internal_framemgr.num_frames > 0) {
		ret = is_subdev_internal_stop(device, IS_DEVICE_ISCHAIN, &device->ixw);
		if (ret)
			mswarn_hw("subdev internal stop is fail(%d)", instance, hw_ip, ret);
	}
	ret = is_subdev_internal_close(device, IS_DEVICE_ISCHAIN, &device->ixw);
	if (ret)
		mserr_hw("is_subdev_internal_close is failed(%d)", instance, hw_ip, ret);

	param_set = &hw_isp->param_set[instance];
	param_set->fcount = 0;
	if (test_bit(HW_RUN, &hw_ip->state)) {
		if (IS_ENABLED(ISP_DDK_LIB_CALL))
			is_lib_isp_stop(hw_ip, &hw_isp->lib[instance], instance, 1);
	} else {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
	}

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int is_hw_isp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	struct is_hw_isp *hw_isp;
	struct is_region *region;
	struct isp_param_set *param_set;
	struct isp_param *param;
	struct is_isp_config *iq_config;
	u32 fcount, instance;
	u32 cur_idx;
	u32 set_id;
	u32 cmd_mv_in;
	u32 reg_idx;
	int ret, batch_num;
	bool bypass_iq;
	enum base_reg_index itp_base;
	enum base_reg_index dns_base;
	enum base_reg_index mcfp0_base, mcfp1_base;
	ulong debug_iq = (unsigned long)is_get_debug_param(IS_DEBUG_PARAM_IQ);
	u32 strip_index, strip_total_count;
	bool skip_isp_shot;
	u32 cmd_wgt_out, cmd_prev_wgt_in;
	u32 cmd_yuv_out, cmd_prev_yuv_in;

	msdbg_hw(2, "[F%d][%s] is called\n", frame->instance, hw_ip, frame->fcount, __func__);

	instance = frame->instance;
	itp_base = __is_hw_isp_get_reg_base(ISP_REG_ITP);
	dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);
	mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
	mcfp1_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP1);

	msdbgs_hw(2, "[F:%d]shot\n", instance, hw_ip, frame->fcount);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	set_bit(hw_ip->id, &frame->core_flag);

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	param_set = &hw_isp->param_set[instance];
	region = hw_ip->region[instance];
	param = &region->parameter.isp;

	atomic_set(&hw_isp->strip_index, frame->stripe_info.region_id);
	fcount = frame->fcount;

	if (frame->shot_ext) {
		if ((param_set->tnr_mode != frame->shot_ext->tnr_mode) &&
			!CHK_VIDEOHDR_MODE_CHANGE(param_set->tnr_mode, frame->shot_ext->tnr_mode))
			msinfo_hw("[F:%d] TNR mode is changed (%d -> %d)\n",
				instance, hw_ip, frame->fcount,
				param_set->tnr_mode, frame->shot_ext->tnr_mode);
		param_set->tnr_mode = frame->shot_ext->tnr_mode;
	} else {
		mswarn_hw("frame->shot_ext is null", instance, hw_ip);
		param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
	}

	if (unlikely(test_bit(ISP_DEBUG_TNR_BYPASS, &debug_isp))) {
		param_set->tnr_mode = TNR_PROCESSING_TNR_BYPASS;
		msinfo_hw("[F:%d] set TNR mode: %d\n",
			instance, hw_ip, frame->fcount, param_set->tnr_mode);
	}

	__is_hw_isp_update_param(hw_ip, region, param_set, frame->pmap, instance);

	/* DMA settings */
	cur_idx = frame->cur_buf_index;

	is_hardware_dma_cfg("ixs", hw_ip, frame, cur_idx, frame->num_buffers,
		&param_set->dma_input.cmd,
		param_set->dma_input.plane,
		param_set->input_dva,
		frame->dvaddr_buffer);

#if IS_ENABLED(SOC_TNR_MERGER)
#if IS_ENABLED(USE_MCFP_MOTION_INTERFACE)
	cmd_mv_in = is_hardware_dma_cfg_32bit("motion", hw_ip,
		frame, cur_idx, frame->num_buffers,
		&param_set->motion_dma_input.cmd,
		param_set->motion_dma_input.plane,
		param_set->input_dva_motion,
		frame->ixmTargetAddress);

	if (CHECK_NEED_KVADDR_ID(IS_VIDEO_IMM_NUM))
		is_hardware_dma_cfg_64bit("motion", hw_ip,
			frame, cur_idx,
			&param_set->motion_dma_input.cmd,
			param_set->motion_dma_input.plane,
			param_set->input_kva_motion,
			frame->ixmKTargetAddress);

	is_hardware_dma_cfg_32bit("drcgrid", hw_ip,
		frame, cur_idx, frame->num_buffers,
		&param_set->dma_input_drcgrid.cmd,
		param_set->dma_input_drcgrid.plane,
		param_set->input_dva_drcgrid,
		frame->ixdgrTargetAddress);
#endif
#endif

	cmd_wgt_out = param_set->dma_output_tnr_wgt.cmd;
	cmd_prev_wgt_in = param_set->prev_wgt_dma_input.cmd;
	cmd_yuv_out = param_set->dma_output_tnr_prev.cmd;
	cmd_prev_yuv_in = param_set->prev_dma_input.cmd;

	param_set->instance_id = instance;
	param_set->fcount = fcount;
	/* multi-buffer */
	hw_ip->num_buffers = frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;
	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= cur_idx << CURR_INDEX_SHIFT;
	} else {
		batch_num = 0;
	}

	strip_index = param_set->stripe_input.index;
	strip_total_count = param_set->stripe_input.total_count;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (!batch_num && !strip_total_count) {
			/* normal shot */
			skip_isp_shot = false;
		} else {
			/* SW batch mode or Stripe processing */
			if ((batch_num && !cur_idx) || (strip_total_count && !strip_index))
				skip_isp_shot = false;
			else
				skip_isp_shot = true;
		}
	} else {
		skip_isp_shot = false;
	}

	if (IS_ENABLED(ISP_DDK_LIB_CALL)) {
		if (!skip_isp_shot) {
			ret = is_lib_isp_set_ctrl(hw_ip, &hw_isp->lib[instance], frame);
			if (ret)
				mserr_hw("set_ctrl fail", instance, hw_ip);
		}
	}

	/* temp direct set */
	set_id = COREX_SET;

	bypass_iq = false;
	if (IS_ENABLED(ISP_DDK_LIB_CALL)) {
		if (!skip_isp_shot) {
			ret = is_lib_isp_shot(hw_ip, &hw_isp->lib[instance], param_set, frame->shot);
			if (ret)
				return ret;
		} else {
			for (reg_idx = ISP_REG_ITP; reg_idx < ISP_REG_MAX; reg_idx++)
				set_bit(CR_SET_CONFIG, &hw_isp->iq_set[instance][reg_idx].state);
		}

		if (likely(!test_bit(hw_ip->id, (unsigned long *)&debug_iq))) {
			_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_E);
			ret = __is_hw_isp_set_rta_regs(hw_ip, set_id, hw_isp, instance);
			_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_X);
			if (ret > 0) {
				msinfo_hw("set_regs is not called from ddk\n", instance, hw_ip);
				bypass_iq = true;
			} else if (ret) {
				mserr_hw("__is_hw_isp_set_rta_regs is fail", instance, hw_ip);
				bypass_iq = true;
			}
		} else {
			msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
			bypass_iq = true;
		}
	} else {
		bypass_iq = true;
	}

	if (unlikely(bypass_iq))
		__is_hw_isp_bypass_iq_module(hw_ip, param_set, instance, set_id);

	iq_config = &hw_isp->iq_config[instance];
	if (iq_config->geomatch_en && !iq_config->geomatch_bypass && !cmd_mv_in) {
		mswarn_hw("LMC is enabled, but mv buffer is disabled", instance, hw_ip);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}

	ret = __is_hw_isp_allocate_internal_buffer(hw_ip, param_set, instance, frame);
	if (ret) {
		mserr_hw("__is_hw_isp_allocate_internal_buffer is fail", instance, hw_ip);
		return ret;
	}

	ret = __is_hw_isp_update_internal_param(hw_ip, param_set, instance, frame);
	if (ret) {
		mserr_hw("__is_hw_isp_update_internal_param is fail", instance, hw_ip);
		return ret;
	}

	ret = __is_hw_isp_set_control(hw_ip, param_set, instance, set_id);
	if (ret) {
		mserr_hw("is_hw_isp_init_config is fail", instance, hw_ip);
		return ret;
	}

	ret = __is_hw_isp_set_size(hw_ip, param_set, instance, frame, set_id);
	if (ret) {
		mserr_hw("__is_hw_isp_set_size is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_isp_set_dma(hw_ip, param_set, instance, set_id);
	if (ret) {
		mserr_hw("__is_hw_isp_set_dma is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_isp_set_otf_input(hw_ip, param_set, instance, set_id);
	if (ret) {
		mserr_hw("__is_hw_isp_set_otf_input is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_isp_set_otf_output(hw_ip, param_set, instance, set_id);
	if (ret) {
		mserr_hw("__is_hw_isp_set_otf_output is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_isp_set_fro(hw_ip, instance, set_id);
	if (ret) {
		mserr_hw("__is_hw_isp_set_fro is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (unlikely(test_bit(ISP_DEBUG_DTP, &debug_isp)))
		dns_hw_enable_dtp(hw_ip->regs[dns_base], set_id, true);

	if (unlikely(test_bit(ISP_DEBUG_MCFP0_DUMP, &debug_isp)))
		mcfp0_hw_dump(hw_ip->regs[mcfp0_base]);

	if (unlikely(test_bit(ISP_DEBUG_MCFP1_DUMP, &debug_isp)))
		mcfp1_hw_dump(hw_ip->regs[mcfp1_base]);

	if (unlikely(test_bit(ISP_DEBUG_DNS_DUMP, &debug_isp)))
		dns_hw_dump(hw_ip->regs[dns_base]);

	if (unlikely(test_bit(ISP_DEBUG_ITP_DUMP, &debug_isp)))
		itp_hw_dump(hw_ip->regs[itp_base]);

	ret = __is_hw_isp_enable(hw_ip, param_set, instance, set_id);
	if (ret) {
		mserr_hw("__is_hw_isp_enable is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	param_set->dma_output_tnr_wgt.cmd = cmd_wgt_out;
	param_set->prev_wgt_dma_input.cmd = cmd_prev_wgt_in;
	param_set->dma_output_tnr_prev.cmd = cmd_yuv_out;
	param_set->prev_dma_input.cmd = cmd_prev_yuv_in;

	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	if (IS_ENABLED(ISP_DDK_LIB_CALL))
		is_lib_isp_reset_recovery(hw_ip, &hw_isp->lib[instance], instance);

	return ret;
}

static int is_hw_isp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
	IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	return 0;
}

static int is_hw_isp_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	int ret;
	struct is_hw_isp *hw_isp;

	msdbg_hw(2, "[HW][%s] is called\n", frame->instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;

	if (IS_ENABLED(ISP_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_meta(hw_ip, &hw_isp->lib[frame->instance], frame);
		if (ret) {
			mserr_hw("get_meta fail", frame->instance, hw_ip);
			return ret;
		}
	}

	return 0;
}

static int is_hw_isp_get_cap_meta(struct is_hw_ip *hw_ip, ulong hw_map,
	u32 instance, u32 fcount, u32 size, ulong addr)
{
	int ret = 0;
	struct is_hw_isp *hw_isp;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	if (unlikely(!hw_isp)) {
		mserr_hw("priv_info is NULL", instance, hw_ip);
		return -EINVAL;
	}

	if (IS_ENABLED(ISP_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_cap_meta(hw_ip, &hw_isp->lib[instance],
				instance, fcount, size, addr);
		if (ret)
			mserr_hw("get_cap_meta fail", instance, hw_ip);
	}

	return ret;
}

static int is_hw_isp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	u32 output_id = 0;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (test_bit(hw_ip->id, &frame->core_flag))
		output_id = IS_HW_CORE_END;

	return is_hardware_frame_done(hw_ip, frame, -1,
		IS_HW_CORE_END, done_type, false);
}

static int is_hw_isp_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0, flag;
	ulong addr;
	u32 size, index;
	struct is_hw_isp *hw_isp;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (test_bit(DEV_HW_3AA0, &hw_map) ||
		test_bit(DEV_HW_3AA1, &hw_map) ||
		test_bit(DEV_HW_3AA2, &hw_map)) {
		return 0;
	}

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	switch (setfile->version) {
	case SETFILE_V2:
		flag = false;
		break;
	case SETFILE_V3:
		flag = true;
		break;
	default:
		mserr_hw("invalid version (%d)", instance, hw_ip, setfile->version);
		return -EINVAL;
	}

	FIMC_BUG(!hw_ip->priv_info);
	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	msinfo_lib("create_tune_set count %d\n", instance, hw_ip,
			setfile->using_count);
	if (IS_ENABLED(ISP_DDK_LIB_CALL)) {
		for (index = 0; index < setfile->using_count; index++) {
			addr = setfile->table[index].addr;
			size = setfile->table[index].size;
			ret = is_lib_isp_create_tune_set(&hw_isp->lib[instance],
				addr, size, index, flag, instance);

			set_bit(index, &hw_isp->lib[instance].tune_count);
		}
	}

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_isp_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret = 0;
	u32 setfile_index;
	struct is_hw_isp *hw_isp;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (test_bit(DEV_HW_3AA0, &hw_map) ||
		test_bit(DEV_HW_3AA1, &hw_map) ||
		test_bit(DEV_HW_3AA2, &hw_map)) {
		return 0;
	}

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	if (setfile->using_count == 0)
		return 0;

	setfile_index = setfile->index[scenario];
	if (setfile_index >= setfile->using_count) {
		mserr_hw("setfile index is out-of-range, [%d:%d]",
			instance, hw_ip, scenario, setfile_index);
		return -EINVAL;
	}

	msinfo_hw("setfile (%d) scenario (%d)\n", instance, hw_ip,
		setfile_index, scenario);

	FIMC_BUG(!hw_ip->priv_info);
	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;

	if (IS_ENABLED(ISP_DDK_LIB_CALL))
		ret = is_lib_isp_apply_tune_set(&hw_isp->lib[instance],
			setfile_index, instance, scenario);

	return ret;
}

static int is_hw_isp_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_isp *hw_isp;
	int i, ret = 0;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	FIMC_BUG(!hw_ip);

	if (test_bit(DEV_HW_3AA0, &hw_map) ||
		test_bit(DEV_HW_3AA1, &hw_map) ||
		test_bit(DEV_HW_3AA2, &hw_map)) {
		return 0;
	}

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		msdbg_hw(2, "Not initialized\n", instance, hw_ip);
		return 0;
	}

	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	if (setfile->using_count == 0)
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;

	if (IS_ENABLED(ISP_DDK_LIB_CALL)) {
		for (i = 0; i < setfile->using_count; i++) {
			if (test_bit(i, &hw_isp->lib[instance].tune_count)) {
				ret = is_lib_isp_delete_tune_set(&hw_isp->lib[instance],
					(u32)i, instance);
				if (ret)
					mserr_hw("delete_tune_set fail", instance, hw_ip);
				clear_bit(i, &hw_isp->lib[instance].tune_count);
			}
		}
	}

	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int is_hw_isp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_isp *hw_isp;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	if (__is_hw_isp_init(hw_ip, instance)) {
		mserr_hw("__is_hw_isp_init fail", instance, hw_ip);
		return -ENODEV;
	}

	if (IS_ENABLED(ISP_DDK_LIB_CALL)) {

		hw_isp = (struct is_hw_isp *)hw_ip->priv_info;

		ret = is_lib_isp_reset_recovery(hw_ip, &hw_isp->lib[instance], instance);
		if (ret) {
			mserr_hw("is_lib_isp_reset_recovery fail ret(%d)",
				instance, hw_ip, ret);
			return ret;
		}
	}

	return 0;
}

int is_hw_isp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size)
{
	struct is_hw_isp *hw_isp;
	struct is_hw_isp_iq *iq_set;
	ulong flag = 0;
	u32 reg_idx, i;

	msdbg_hw(2, "[HW][%s] is called: chain_id (%d)\n", instance, hw_ip, __func__, chain_id);

	FIMC_BUG(!regs);

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	reg_idx = chain_id / EXT1_CHAIN_OFFSET;
	iq_set = &hw_isp->iq_set[instance][reg_idx];

	if (!test_and_clear_bit(CR_SET_EMPTY, &iq_set->state))
		return -EBUSY;

	spin_lock_irqsave(&iq_set->slock, flag);

	iq_set->size = regs_size;
	iq_set->fcount = fcount;
	memcpy((void *)iq_set->regs, (void *)regs, sizeof(struct cr_set) * regs_size);

	if (unlikely(test_bit(ISP_DEBUG_SET_REG, &debug_isp))) {
		for (i = 0; i < regs_size; i++)
			msinfo_hw("%s:[%d][0x%08x] 0x%08x\n", instance, hw_ip, __func__, reg_idx,
				regs[i].reg_addr, regs[i].reg_data);
	}

	spin_unlock_irqrestore(&iq_set->slock, flag);
	set_bit(CR_SET_CONFIG, &iq_set->state);

	msdbg_hw(2, "Store IQ regs set: %p, size(%d)\n", instance, hw_ip, regs, regs_size);

	return 0;
}

int is_hw_isp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	enum base_reg_index itp_base;
	enum base_reg_index dns_base;
	enum base_reg_index mcfp0_base, mcfp1_base;
	struct is_common_dma *dma;
	struct is_hw_isp *hw_isp = NULL;
	u32 i;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		itp_base = __is_hw_isp_get_reg_base(ISP_REG_ITP);
		dns_base = __is_hw_isp_get_reg_base(ISP_REG_DNS);
		mcfp0_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP0);
		mcfp1_base = __is_hw_isp_get_reg_base(ISP_REG_MCFP1);

		if (test_bit(ISP_DEBUG_DUMP_FORMAT, &debug_isp)) {
			is_hw_dump_regs_hex(hw_ip->regs[itp_base], "ITP",
				hw_ip->regs_end[itp_base] - hw_ip->regs_start[itp_base]);
			is_hw_dump_regs_hex(hw_ip->regs[dns_base], "DNS",
				hw_ip->regs_end[dns_base] - hw_ip->regs_start[dns_base]);
			is_hw_dump_regs_hex(hw_ip->regs[mcfp0_base], "MCFP0",
				hw_ip->regs_end[mcfp0_base] - hw_ip->regs_start[mcfp0_base]);
			is_hw_dump_regs_hex(hw_ip->regs[mcfp1_base], "MCFP1",
				hw_ip->regs_end[mcfp1_base] - hw_ip->regs_start[mcfp1_base]);
		} else {
			itp_hw_dump(hw_ip->regs[itp_base]);
			dns_hw_dump(hw_ip->regs[dns_base]);
			mcfp0_hw_dump(hw_ip->regs[mcfp0_base]);
			mcfp1_hw_dump(hw_ip->regs[mcfp1_base]);
		}
		break;
	case IS_REG_DUMP_DMA:
		for (i = MCFP_RDMA_CURR_BAYER; i < MCFP_RDMA_MAX; i++) {
			hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
			dma = &hw_isp->mcfp_rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = MCFP_WDMA_PREV_BAYER; i < MCFP_WDMA_MAX; i++) {
			hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
			dma = &hw_isp->mcfp_wdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = DNS_RDMA_BAYER; i < DNS_DMA_MAX; i++) {
			hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
			dma = &hw_isp->dns_dma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int is_hw_isp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf)
{
	struct is_hw_isp *hw_isp;
	struct is_isp_config *isp_config = (struct is_isp_config *)conf;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);
	FIMC_BUG(!conf);

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;

	if (hw_isp->iq_config[instance].geomatch_en != isp_config->geomatch_en)
		msinfo_hw("[F:%d] MCFP geomatch en: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].geomatch_en,
			isp_config->geomatch_en);

	if (hw_isp->iq_config[instance].geomatch_bypass != isp_config->geomatch_bypass)
		msinfo_hw("[F:%d] MCFP geomatch bypass: %d -> %d %s", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].geomatch_bypass,
			isp_config->geomatch_bypass,
			isp_config->geomatch_en ? (isp_config->geomatch_bypass ? "GMC Mode" : "LMC Mode") : "");

	if (hw_isp->iq_config[instance].bgdc_en != isp_config->bgdc_en)
		msinfo_hw("[F:%d] MCFP bgdc_en: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].bgdc_en,
			isp_config->bgdc_en);

	if (hw_isp->iq_config[instance].mixer_en != isp_config->mixer_en)
		msinfo_hw("[F:%d] MCFP mixer_en: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].mixer_en,
			isp_config->mixer_en);

	if (hw_isp->iq_config[instance].still_en != isp_config->still_en)
		msinfo_hw("[F:%d] MCFP still enable: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].still_en,
			isp_config->still_en);

	if (hw_isp->iq_config[instance].tnr_mode != isp_config->tnr_mode)
		msinfo_hw("[F:%d] MCFP tnr_mode: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].tnr_mode,
			isp_config->tnr_mode);

	if (hw_isp->iq_config[instance].img_bit != isp_config->img_bit)
		msinfo_hw("[F:%d] MCFP img bit: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].img_bit,
			isp_config->img_bit);

	if (hw_isp->iq_config[instance].wgt_bit != isp_config->wgt_bit)
		msinfo_hw("[F:%d] MCFP wgt bit: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].wgt_bit,
			isp_config->wgt_bit);

	if (hw_isp->iq_config[instance].top_nflbc_6line != isp_config->top_nflbc_6line)
		msinfo_hw("[F:%d] MCFP top_nflbc_6line: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].top_nflbc_6line,
			isp_config->top_nflbc_6line);

	if (hw_isp->iq_config[instance].top_otfout_mask_clean != isp_config->top_otfout_mask_clean)
		msinfo_hw("[F:%d] MCFP top_otfout_mask_clean: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].top_otfout_mask_clean,
			isp_config->top_otfout_mask_clean);

	if (hw_isp->iq_config[instance].top_otfout_mask_dirty != isp_config->top_otfout_mask_dirty)
		msinfo_hw("[F:%d] MCFP top_otfout_mask_dirty: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].top_otfout_mask_dirty,
			isp_config->top_otfout_mask_dirty);

	if (hw_isp->iq_config[instance].drc_dstr_bypass != isp_config->drc_dstr_bypass)
		msinfo_hw("[F:%d] ITP/DNS drc_dstr_bypass: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].drc_dstr_bypass,
			isp_config->drc_dstr_bypass);

	if ((hw_isp->iq_config[instance].drc_dstr_grid_x != isp_config->drc_dstr_grid_x) ||
	    (hw_isp->iq_config[instance].drc_dstr_grid_y != isp_config->drc_dstr_grid_y))
		msinfo_hw("[F:%d] ITP/DNS drc_dstr_grid_size: %dx%d -> %dx%d",
			instance, hw_ip, fcount,
			hw_isp->iq_config[instance].drc_dstr_grid_x, hw_isp->iq_config[instance].drc_dstr_grid_y,
			isp_config->drc_dstr_grid_x, isp_config->drc_dstr_grid_y);

	if (hw_isp->iq_config[instance].dirty_bayer_en != isp_config->dirty_bayer_en)
		msinfo_hw("[F:%d] ITP/DNS dirty_bayer_en: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].dirty_bayer_en,
			isp_config->dirty_bayer_en);

	if (hw_isp->iq_config[instance].ww_lc_en != isp_config->ww_lc_en)
		msinfo_hw("[F:%d] ITP/DNS ww_lc_en: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].ww_lc_en,
			isp_config->ww_lc_en);

	if (hw_isp->iq_config[instance].tnr_input_en != isp_config->tnr_input_en)
		msinfo_hw("[F:%d] ITP/DNS tnr_input_en: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].tnr_input_en,
			isp_config->tnr_input_en);

	if (hw_isp->iq_config[instance].sensor_center_y != isp_config->sensor_center_y)
		msinfo_hw("[F:%d] ITP/DNS sensor_center_x: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].sensor_center_x,
			isp_config->sensor_center_x);

	if (hw_isp->iq_config[instance].sensor_center_y != isp_config->sensor_center_y)
		msinfo_hw("[F:%d] ITP/DNS sensor_center_y: %d -> %d", instance, hw_ip, fcount,
			hw_isp->iq_config[instance].sensor_center_y,
			isp_config->sensor_center_y);

	msdbg_hw(2, "[F:%d] skip_wdma(%d)\n", instance, hw_ip, fcount, isp_config->skip_wdma);

	memcpy(&hw_isp->iq_config[instance], conf, sizeof(struct is_isp_config));
	return 0;
}

static int is_hw_isp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	u32 fcount;
	struct is_hw_isp *hw_isp;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	fcount = atomic_read(&hw_ip->fcount);
	is_hw_isp_dump_regs(hw_ip, instance, 0, NULL, 0, IS_REG_DUMP_TO_LOG);

	hw_isp = (struct is_hw_isp *)hw_ip->priv_info;
	if (IS_ENABLED(ISP_DDK_LIB_CALL))
		return is_lib_notify_timeout(&hw_isp->lib[instance], instance);
	else
		return 0;
}

const struct is_hw_ip_ops is_hw_isp_ops = {
	.open			= is_hw_isp_open,
	.init			= is_hw_isp_init,
	.deinit			= is_hw_isp_deinit,
	.close			= is_hw_isp_close,
	.enable			= is_hw_isp_enable,
	.disable		= is_hw_isp_disable,
	.shot			= is_hw_isp_shot,
	.set_param		= is_hw_isp_set_param,
	.get_meta		= is_hw_isp_get_meta,
	.get_cap_meta		= is_hw_isp_get_cap_meta,
	.frame_ndone		= is_hw_isp_frame_ndone,
	.load_setfile		= is_hw_isp_load_setfile,
	.apply_setfile		= is_hw_isp_apply_setfile,
	.delete_setfile		= is_hw_isp_delete_setfile,
	.restore		= is_hw_isp_restore,
	.set_regs		= is_hw_isp_set_regs,
	.dump_regs		= is_hw_isp_dump_regs,
	.set_config		= is_hw_isp_set_config,
	.notify_timeout		= is_hw_isp_notify_timeout,
};

int is_hw_isp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;

	dbg_hw(2, "[HW][%s] is called\n", __func__);

	/* initialize device hardware */
	hw_ip->id   = id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "%s", name);
	hw_ip->ops  = &is_hw_isp_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	atomic_set(&hw_ip->fcount, 0);
	hw_ip->is_leader = false;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw_ip->rsccount, 0);
	atomic_set(&hw_ip->run_rsccount, 0);
	init_waitqueue_head(&hw_ip->status.wait_queue);

	hw_slot = is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_isp_handle_dns_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_isp_handle_dns_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP3].handler = &is_hw_isp_handle_mcfp_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP3].id = INTR_HWIP3;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP3].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP4].handler = &is_hw_isp_handle_mcfp_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP4].id = INTR_HWIP4;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP4].valid = true;

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

	sinfo_hw("probe done\n", hw_ip);

	return 0;
}
