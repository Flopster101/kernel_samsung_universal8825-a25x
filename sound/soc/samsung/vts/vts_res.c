// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts-res.c
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/* #define DEBUG */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pinctrl/consumer.h>
#include <linux/sched/clock.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/samsung/vts.h>

#include "vts_res.h"
#include "vts_dbg.h"

#define VTS_PIN_TRACE

#ifdef VTS_PIN_TRACE
#define VTS_PIN_TRACE_MAX (100)
struct vts_pin_trace {
	unsigned int idx;
	char name[16];
	long long updated;
};
static unsigned int p_vts_pin_trace_idx;
static struct vts_pin_trace p_vts_pin_trace[VTS_PIN_TRACE_MAX];
#endif

struct vts_port_request {
	int id;
	int gpio_func;
	int clk;
	bool enable;
};

static struct vts_port_request p_vts_port_req[VTS_PORT_PAD_MAX][VTS_PORT_ID_MAX];

static int vts_cfg_gpio(struct device *dev, const char *name)
{
	struct vts_data *data = dev_get_drvdata(dev);
	struct pinctrl_state *pin_state;
	int ret;
#ifdef VTS_PIN_TRACE
	unsigned int idx = p_vts_pin_trace_idx % VTS_PIN_TRACE_MAX;
	struct vts_pin_trace *pin_trace = &p_vts_pin_trace[idx];
#endif

	if (!data->pinctrl)
		return -ENOENT;

	mutex_lock(&data->mutex_pin);
#ifdef VTS_PIN_TRACE
	strncpy(pin_trace->name, name, sizeof(pin_trace->name) - 1);
	pin_trace->idx = p_vts_pin_trace_idx;
	pin_trace->updated = local_clock();

	vts_dev_dbg(dev, "%s(%d)(%d) %s %lld\n", __func__,
			idx,
			pin_trace->idx,
			pin_trace->name,
			pin_trace->updated);

	vts_dev_info(dev, "%s(%d) %s\n", __func__,
			pin_trace->idx,
			pin_trace->name);

	p_vts_pin_trace_idx++;
#else
	vts_dev_info(dev, "%s(%s)\n", __func__, name);
#endif
	pin_state = pinctrl_lookup_state(data->pinctrl, name);
	if (IS_ERR(pin_state)) {
		vts_dev_err(dev, "Couldn't find pinctrl %s\n", name);
		ret = -EINVAL;
	} else {
		ret = pinctrl_select_state(data->pinctrl, pin_state);
		if (ret < 0)
			vts_dev_err(dev,
				"Unable to configure pinctrl %s\n", name);
	}
	mutex_unlock(&data->mutex_pin);

	return ret;
}

static void vts_port_print(struct device *dev, int pos_x, int pos_y)
{
	struct vts_port_request *req;

	req = &p_vts_port_req[pos_x][pos_y];

	vts_dev_info(dev, "%s[%d %d] %s %s clk%dHz en%d\n",
		__func__,
		pos_x, pos_y,
		(req->id == VTS_PORT_ID_VTS) ? "VTS" : "SLIF",
		(req->gpio_func == DPDM) ? "DPDM" : "APDM",
		req->clk,
		req->enable);
	vts_dev_dbg(dev, "%s[%d %d] %d %d clk%dHz en%d\n",
		__func__,
		pos_x, pos_y,
		req->id,
		req->gpio_func,
		req->clk,
		req->enable);
}

static void vts_port_print_all(struct device *dev)
{
	int i, j = 0;

	for (i = 0; i < VTS_PORT_PAD_MAX; i++)
		for (j = 0; j < VTS_PORT_ID_MAX; j++)
			vts_port_print(dev, i, j);

}

void vts_port_init(struct device *dev)
{
	struct vts_port_request *req;
	int i, j = 0;

	for (i = 0; i < VTS_PORT_PAD_MAX; i++) {
		for (j = 0; j < VTS_PORT_ID_MAX; j++) {
			req = &p_vts_port_req[i][j];
			req->id = j;
			req->gpio_func = 0;
			req->clk = 0;
			req->enable = 0;
		}
	}

	vts_port_print_all(dev);
}

int vts_port_cfg(struct device *dev,
		enum vts_port_pad pad,
		enum vts_port_id id,
		enum vts_dmic_sel gpio_func,
		bool enable)
{
	static DEFINE_MUTEX(port_lock);
	char pin_name[32] = {0,};
	int ret = 0;

	struct vts_port_request *port_req = &p_vts_port_req[pad][id];

	if (port_req->enable == enable) {
		vts_dev_dbg(dev, "%s [%d %d]already %s\n",
				__func__,
				pad, id,
				(enable ? "enabled" : "disabled"));

		return 0;
	}

	/* update port_req infomation */
	mutex_lock(&port_lock);
	port_req->gpio_func = (int)gpio_func;
	port_req->enable = enable;

	if (enable) {
		if (p_vts_port_req[pad][VTS_PORT_ID_VTS].enable ||
			p_vts_port_req[pad][VTS_PORT_ID_SLIF].enable) {

			if (gpio_func == DPDM) {
				/* dmic default */
				snprintf(pin_name, sizeof(pin_name),
					"dmic%d_default", pad);
			} else {
				/* dmic default */
				snprintf(pin_name, sizeof(pin_name),
					"amic%d_default", pad);
			}
			ret = vts_cfg_gpio(dev, pin_name);
			if (ret < 0)
				vts_dev_err(dev, "vts_cfg_gpio is failed %d\n",
						ret);
		}
	} else {
		if (!p_vts_port_req[pad][VTS_PORT_ID_VTS].enable &&
			!p_vts_port_req[pad][VTS_PORT_ID_SLIF].enable) {

			/* dmic idle */
			snprintf(pin_name, sizeof(pin_name),
				"dmic%d_idle", pad);
			ret = vts_cfg_gpio(dev, pin_name);
			if (ret < 0)
				vts_dev_err(dev, "vts_cfg_gpio is failed %d\n",
						ret);
		}
	}

	mutex_unlock(&port_lock);

	vts_dev_info(dev, "%s pad%d id%d f%d en%d\n",
			__func__,
			pad, id,
			port_req->gpio_func,
			port_req->enable);

	return ret;
}
EXPORT_SYMBOL(vts_port_cfg);

int vts_set_clk_src(struct device *dev, enum vts_clk_src clk_src)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int ret = 0;

	vts_dev_info(dev, "%s clk src=%d\n", __func__, clk_src);
	data->clk_path = clk_src;

	return ret;
}
EXPORT_SYMBOL(vts_set_clk_src);

int vts_clk_set_rate(struct device *dev, unsigned long combination)
{
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned long dmic_rate, dmic_sync, dmic_if, mux_dmic_if;
	unsigned long mux_slif;
	unsigned long slif_vts;
	int result = 0;

	vts_dev_info(dev, "%s(%lu)\n", __func__, combination);

	switch (combination) {
	case 2:
		mux_dmic_if = VTS_DMIC_CLK_MUX;
		dmic_rate = 384000;
		dmic_sync = 384000;
		dmic_if = 768000;
		mux_slif = VTS_DMIC_CLK_MUX;
		slif_vts = 384000;
		break;
	case 0:
		mux_dmic_if = VTS_DMIC_CLK_MUX;
		dmic_rate = 512000;
		dmic_sync = 512000;
		dmic_if = 1024000;
		mux_slif = VTS_DMIC_CLK_MUX;
		slif_vts = 512000;
		break;
	case 1:
		mux_dmic_if = VTS_DMIC_CLK_MUX;
		dmic_rate = 768000;
		dmic_sync = 768000;
		dmic_if = 1536000;
		mux_slif = VTS_DMIC_CLK_MUX;
		slif_vts = 512000;
		break;
	case 3:
		mux_dmic_if = VTS_DMIC_CLK_MUX;
		dmic_rate = 4096000;
		dmic_sync = 2048000;
		dmic_if = 4096000;
		mux_slif = VTS_DMIC_CLK_MUX;
		slif_vts = 4096000;
		break;
	case 4:
	case 5:
		mux_dmic_if = DUAL_DMIC_CLK_MUX;
		dmic_rate = 3072000;
		dmic_sync = 3072000;
		dmic_if = 6144000;
		mux_slif = SLIF_DMIC_CLK_MUX;
		slif_vts = 512000;
		break;
	default:
		result = -EINVAL;
		goto out;
	}

	if (data->mux_clk_dmic_if) {
		result = clk_set_rate(data->mux_clk_dmic_if, mux_dmic_if);
		if (result < 0)
			vts_dev_err(dev, "clk_set_rate: %d\n", result);

		vts_dev_info(dev, "DMIC IF MUX Clock : %ld\n",
				clk_get_rate(data->mux_clk_dmic_if));
	}

	if (data->clk_slif_src1) {
		result = clk_set_rate(data->clk_slif_src1, mux_slif);
		if (result < 0)
			vts_dev_err(dev, "clk_slif_src1: %d\n", result);

		vts_dev_info(dev, "MUX_CLK_VTS_DMIC_PAD: %ld\n",
			clk_get_rate(data->clk_slif_src1));
	}

	if (data->clk_slif_src2) {
		result = clk_set_rate(data->clk_slif_src2, 24576000);
		if (result < 0)
			vts_dev_err(dev, "clk_slif_src2: %d\n", result);

		vts_dev_info(dev, "DOUT_DIV_CLKVTS_AUD_DMIC0: %ld\n",
			clk_get_rate(data->clk_slif_src2));
	}

	if (data->clk_dmic_if) {
		result = clk_set_rate(data->clk_dmic_if, dmic_if);
		if (result < 0) {
			vts_dev_err(dev,
				"Failed to set rate of the clock %s\n",
				"dmic_if");
			goto out;
		}
		vts_dev_info(dev, "DMIC IF clock rate: %lu\n",
			clk_get_rate(data->clk_dmic_if));
	}

	if (data->clk_dmic_sync) {
		result = clk_set_rate(data->clk_dmic_sync, dmic_sync);
		if (result < 0) {
			vts_dev_err(dev,
				"Failed to set rate of the clock %s\n",
				"dmic_sync");
			goto out;
		}
		vts_dev_info(dev, "DMIC SYNC clock rate: %lu\n",
			clk_get_rate(data->clk_dmic_sync));
	}

	if (data->clk_slif_vts) {
		result = clk_set_rate(data->clk_slif_vts, slif_vts);
		if (result < 0) {
			vts_dev_err(dev,
				"Failed to set rate of the clock %s\n",
				"dmic_sync");
			goto out;
		}
		vts_dev_info(dev, "slif_vts clock rate: %lu\n",
			clk_get_rate(data->clk_slif_vts));
	}

#if IS_ENABLED(CONFIG_SOC_EXYNOS2100) || \
	IS_ENABLED(CONFIG_SOC_S5E9925) || IS_ENABLED(CONFIG_SOC_S5E8825)
	/* Do Nothing */
#else
	result = clk_set_rate(data->clk_dmic, dmic_rate);
	if (result < 0) {
		vts_dev_err(dev,
			"Failed to set rate of the clock %s\n", "dmic");
		goto out;
	}
	vts_dev_info(dev, "DMIC clock rate: %lu\n",
		clk_get_rate(data->clk_dmic));
#endif

out:
	return result;
}

#if defined(CONFIG_SOC_S5E9925)
#define SLIF_SEL_PDM_BASE	(0x940) /* 0x15510940 */
#define SLIF_SEL_PDM_REQ_BASE	(0x944) /* 0x15510944 */
#define SLIF_SEL_PAD_AUD_BASE	(0x1010) /* 0x15511010 */
void vts_set_sel_pad(struct device *dev, int enable)
{
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned int ctrl;

	/* SYS REG : 0x15510000 + 1010 */
	if (enable) {
		writel(0x7, data->sfr_base + SLIF_SEL_PAD_AUD_BASE);
		ctrl = readl(data->sfr_base +
				SLIF_SEL_PAD_AUD_BASE);
		vts_dev_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);
		writel(0x7, data->sfr_base + SLIF_SEL_PDM_REQ_BASE);
		ctrl = readl(data->sfr_base +
				SLIF_SEL_PDM_REQ_BASE);
		vts_dev_info(dev, "SEL_PDM_REQ(0x%08x)\n", ctrl);
		writel(0x0, data->sfr_base + SLIF_SEL_PDM_BASE);
		ctrl = readl(data->sfr_base +
				SLIF_SEL_PDM_BASE);
		vts_dev_info(dev, "SEL_PDM(0x%08x)\n", ctrl);
	} else {
		writel(0x0, data->sfr_base + SLIF_SEL_PAD_AUD_BASE);
		ctrl = readl(data->sfr_base +
				SLIF_SEL_PAD_AUD_BASE);
		vts_dev_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);
		writel(0x0, data->sfr_base + SLIF_SEL_PDM_REQ_BASE);
		ctrl = readl(data->sfr_base +
				SLIF_SEL_PDM_REQ_BASE);
		vts_dev_info(dev, "SEL_PDM_REQ(0x%08x)\n", ctrl);
		writel(0x0, data->sfr_base + SLIF_SEL_PDM_BASE);
		ctrl = readl(data->sfr_base +
				SLIF_SEL_PDM_BASE);
		vts_dev_info(dev, "SEL_PDM(0x%08x)\n", ctrl);
	}
}
#elif defined(CONFIG_SOC_S5E8825)
#define SLIF_SEL_PAD_AUD_BASE	(0x1014) /* 0x15511010 */
#define SLIF_SEL_ENABLE_DMIC_IF (0x1000) /* 0x15511014 */
void vts_set_sel_pad(struct device *dev, int enable)
{
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned int ctrl;

	/* SYS REG : 0x15510000 + 1010 */
	if (enable) {
		writel(0x3, data->sfr_base + SLIF_SEL_PAD_AUD_BASE);
		ctrl = readl(data->sfr_base +
				SLIF_SEL_PAD_AUD_BASE);
		vts_dev_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);
		writel(0xC, data->sfr_base + SLIF_SEL_ENABLE_DMIC_IF);
		ctrl = readl(data->sfr_base +
				SLIF_SEL_ENABLE_DMIC_IF);
		vts_dev_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);
	} else {
		writel(0x0, data->sfr_base + SLIF_SEL_PAD_AUD_BASE);
		ctrl = readl(data->sfr_base +
				SLIF_SEL_PAD_AUD_BASE);
		vts_dev_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);
		writel(0x0, data->sfr_base + SLIF_SEL_ENABLE_DMIC_IF);
		ctrl = readl(data->sfr_base +
				SLIF_SEL_ENABLE_DMIC_IF);
		vts_dev_info(dev, "SEL_PAD_AUD(0x%08x)\n", ctrl);
	}
}
#endif
EXPORT_SYMBOL(vts_set_sel_pad);

#define VTS_SYS_CLOCK_MAX	(393216000)
/* #define VTS_SYS_CLOCK		(66000000) */	/* 66M */
int vts_clk_aud_set_rate(struct device *dev, unsigned long combination)
{
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned long clk_src, dmic_rate, dmic_sync, dmic_if, mux_dmic_if;
	int result = 0;

	vts_dev_info(dev, "%s(%lu)\n", __func__, combination);

	switch (combination) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		clk_src		= 400000000;
		mux_dmic_if	= 294912000;
		dmic_rate	= 3072000;
		dmic_sync	= 3072000;
		dmic_if		= 6144000;
		break;
	default:
		result = -EINVAL;
		goto out;
	}

	if (data->clk_sys_mux) {
		result = clk_set_rate(data->clk_sys_mux, VTS_SYS_CLOCK_MAX);
		if (result < 0) {
			dev_err(dev, "clk_sys_mux: %d\n", result);
			goto out;
		}
	}

	if (data->clk_sys) {
		result = clk_set_rate(data->clk_sys, VTS_SYS_CLOCK);
		if (result < 0) {
			dev_err(dev, "clk_sys: %d\n", result);
			goto out;
		}
	}

	if (data->mux_clk_dmic_if) {
		result = clk_set_rate(data->mux_clk_dmic_if, mux_dmic_if);
		if (result < 0)
			vts_dev_err(dev, "Failed to set rate of the clock %s\n", "mux_dmic_if_clk");

		vts_dev_info(dev, "MUX DMIC IF clock rate: %lu\n", clk_get_rate(data->mux_clk_dmic_if));
	}

	if (data->clk_dmic_if) {
		result = clk_set_rate(data->clk_dmic_if, dmic_if);
		if (result < 0) {
			vts_dev_err(dev,
				"Failed to set rate of the clock %s\n",
				"dmic_if");
			goto out;
		}
		vts_dev_info(dev, "DMIC IF clock rate: %lu\n",
			clk_get_rate(data->clk_dmic_if));
	}

	if (data->clk_dmic_sync) {
		result = clk_set_rate(data->clk_dmic_sync, dmic_sync);
		if (result < 0) {
			vts_dev_err(dev,
				"Failed to set rate of the clock %s\n",
				"dmic_sync");
			goto out;
		}
		vts_dev_info(dev, "DMIC SYNC clock rate: %lu\n",
			clk_get_rate(data->clk_dmic_sync));
	}

out:
	return result;
}
EXPORT_SYMBOL(vts_clk_aud_set_rate);
