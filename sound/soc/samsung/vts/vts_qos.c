// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts_qos.c
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pinctrl/consumer.h>
#include <linux/sched/clock.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/samsung/vts.h>

#include "vts.h"
#include "vts_dbg.h"

static void vts_dma_mif_control(struct device *dev, struct vts_data *vts_data, int on)
{
	int mif_status;
	int mif_req_out;
	int mif_req_out_status;
	int mif_ack_in;
	int mif_on_cnt = 0;

	if (on) {
		mif_req_out = 0x1;
		mif_ack_in = 0x0;
	} else {
		mif_req_out = 0x0;
		mif_ack_in = 0x10;
	}
	mif_status = readl(vts_data->sfr_base + VTS_MIF_REQ_ACK_IN);
	mif_req_out_status = readl(vts_data->sfr_base + VTS_MIF_REQ_OUT);
	vts_dev_info(dev, "%s: MIF STATUS : 0x%x (0x%x), on :%d\n",
			__func__, mif_status, mif_req_out_status, on);
	if ((mif_status & (0x1 << 4)) == mif_ack_in) {
		if (on && (mif_req_out_status == 0x1)) {
			vts_dev_info(dev, "%s: Wrong REQ OUT STATUS -> Toggle\n", __func__);
			writel(0x0, vts_data->sfr_base + VTS_MIF_REQ_OUT);
			mdelay(5);
		}
		writel(mif_req_out, vts_data->sfr_base + VTS_MIF_REQ_OUT);
		mif_req_out_status = readl(vts_data->sfr_base + VTS_MIF_REQ_OUT);
		vts_dev_info(dev, "%s: MIF %d - 0x%x\n", __func__, on, mif_req_out_status);
		while (((mif_status & (0x1 << 4)) == mif_ack_in) && mif_on_cnt <= 480) {
			mif_on_cnt++;
			udelay(50);
			mif_status = readl(vts_data->sfr_base +
				VTS_MIF_REQ_ACK_IN);
		}
	} else {
		vts_dev_info(dev, "%s: MIF already %d\n", __func__, on);
	}
	vts_dev_info(dev, "%s: MIF STATUS OUT : 0x%x cnt : %d\n", __func__, mif_status, mif_on_cnt);
}

void vts_request_dram_on(struct device *dev, unsigned int id, bool on)
{
	struct vts_data *data = dev_get_drvdata(dev);
	struct vts_dram_request *request;
	size_t len = ARRAY_SIZE(data->dram_requests);
	unsigned int val = 0;

	vts_dev_info(dev, "%s: (%#x, %d)\n", __func__, id, on);

	for (request = data->dram_requests; request - data->dram_requests <
			len && request->id && request->id != id; request++) {
	}

	if (request - data->dram_requests >= len) {
		vts_dev_err(dev, "too many dram requests: %#x, %d\n", id, on);
		return;
	}

	request->on = on;
	request->id = id;
	request->updated = local_clock();

	for (request = data->dram_requests; request - data->dram_requests <
			len && request->id; request++) {
		if (request->on) {
			val = 1;
			break;
		}
	}

	vts_dma_mif_control(dev, data, val);
}
EXPORT_SYMBOL(vts_request_dram_on);
