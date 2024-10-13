// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts-plat.c
 *
 * ALSA SoC - Samsung VTS platform driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/pm_wakeup.h>

#include <asm-generic/delay.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include <sound/samsung/mailbox.h>
#include <sound/samsung/vts.h>
#include <soc/samsung/exynos-pmu-if.h>

#include "vts.h"
#include "vts_res.h"
#include "vts_dbg.h"

static const struct snd_pcm_hardware vts_dma_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED
				| SNDRV_PCM_INFO_BLOCK_TRANSFER
				| SNDRV_PCM_INFO_MMAP
				| SNDRV_PCM_INFO_MMAP_VALID,
	.formats		= SNDRV_PCM_FMTBIT_S16,
	.rates			= SNDRV_PCM_RATE_16000,
	.channels_min		= 1,
	.channels_max		= 2,
	.buffer_bytes_max	= BUFFER_BYTES_MAX,
	.period_bytes_min	= PERIOD_BYTES_MIN,
	.period_bytes_max	= PERIOD_BYTES_MAX,
	.periods_min		= BUFFER_BYTES_MAX / PERIOD_BYTES_MAX,
	.periods_max		= BUFFER_BYTES_MAX / PERIOD_BYTES_MIN,
};

static struct vts_dma_data *vts_get_drvdata(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_soc_dai_driver *dai_drv = dai->driver;
	struct vts_data *data =
		dev_get_drvdata(dai->dev);
	struct vts_dma_data *dma_data =
		platform_get_drvdata(data->pdev_vtsdma[dai_drv->id]);

	return dma_data;
}

static int vts_dma_hw_params(struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
	struct device *dev = dai->dev;
	struct vts_dma_data *data = vts_get_drvdata(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	vts_dev_info(dev, "%s state %d %d\n", __func__,
			data->vts_data->vts_state,
			data->vts_data->clk_path);

	if (data->type == PLATFORM_VTS_TRIGGER_RECORD)
		snd_pcm_set_runtime_buffer(
			substream, &data->vts_data->dmab);
	else
		snd_pcm_set_runtime_buffer(
			substream, &data->vts_data->dmab_rec);

	vts_dev_info(dev, "%s:%s:DmaAddr=%pad Total=%zu"
		" PrdSz=%u(%u) #Prds=%u dma_area=%p\n",
		__func__, snd_pcm_stream_str(substream),
		&runtime->dma_addr,	runtime->dma_bytes,
		params_period_size(params),
		params_period_bytes(params),
		params_periods(params), runtime->dma_area);

	data->pointer = 0;

	return 0;
}

static int vts_dma_hw_free(struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
	struct device *dev = dai->dev;
	struct vts_dma_data *data = vts_get_drvdata(substream);

	vts_dev_info(dev, "%s\n", __func__);

	if (IS_ENABLED(CONFIG_SOC_EXYNOS2100) || \
		IS_ENABLED(CONFIG_SOC_S5E9925) || \
		IS_ENABLED(CONFIG_SOC_S5E8825)) {
		vts_request_dram_on(dev, data->type, false);
	} else {
		vts_dev_dbg(dev, "%s request MIF by fw\n", __func__);
	}

	return 0;
}

static int vts_dma_prepare(struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
	struct device *dev = dai->dev;
	struct vts_dma_data *data = vts_get_drvdata(substream);

	vts_dev_info(dev, "%s\n", __func__);

	if (IS_ENABLED(CONFIG_SOC_EXYNOS2100) || \
		IS_ENABLED(CONFIG_SOC_S5E9925) || \
		IS_ENABLED(CONFIG_SOC_S5E8825)) {
		vts_request_dram_on(dev, data->type, true);
	} else {
		vts_dev_dbg(dev, "%s request MIF by fw\n", __func__);
	}

	return 0;
}

static int vts_dma_trigger(struct snd_soc_component *component,
	struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
	struct device *dev = dai->dev;
	struct vts_dma_data *data = vts_get_drvdata(substream);
	u32 values[3] = {0, 0, 0};
	int result = 0;

	vts_dev_info(dev, "%s ++ CMD: %d\n", __func__, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (data->type == PLATFORM_VTS_TRIGGER_RECORD) {
			vts_dev_dbg(dev, "%s VTS_IRQ_AP_START_COPY\n",
				__func__);
			result = vts_start_ipc_transaction(dev, data->vts_data,
				VTS_IRQ_AP_START_COPY, &values, 1, 1);
			data->vts_data->vts_tri_state = VTS_TRI_STATE_COPY_START;
		} else {
			vts_dev_dbg(dev, "%s VTS_IRQ_AP_START_REC\n", __func__);
			result = vts_start_ipc_transaction(dev, data->vts_data,
				VTS_IRQ_AP_START_REC, &values, 1, 1);
			data->vts_data->vts_rec_state = VTS_REC_STATE_START;

		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (data->type == PLATFORM_VTS_TRIGGER_RECORD) {
			vts_dev_dbg(dev, "%s VTS_IRQ_AP_STOP_COPY\n", __func__);
			result = vts_start_ipc_transaction(dev, data->vts_data,
				VTS_IRQ_AP_STOP_COPY, &values, 1, 1);
			data->vts_data->vts_tri_state = VTS_TRI_STATE_COPY_STOP;
		} else {
			vts_dev_dbg(dev, "%s VTS_IRQ_AP_STOP_REC\n", __func__);
			result = vts_start_ipc_transaction(dev, data->vts_data,
				VTS_IRQ_AP_STOP_REC, &values, 1, 1);
			data->vts_data->vts_rec_state = VTS_REC_STATE_STOP;
		}
		break;
	default:
		result = -EINVAL;
		break;
	}

	vts_dev_info(dev, "%s -- CMD: %d\n", __func__, cmd);
	return result;
}

static snd_pcm_uframes_t vts_dma_pointer(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
	struct device *dev = dai->dev;
	struct vts_dma_data *data = vts_get_drvdata(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	vts_dev_dbg(dev, "%s: pointer=%08x\n", __func__, data->pointer);

	return bytes_to_frames(runtime, data->pointer);
}

static int vts_dma_open(struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
	struct device *dev = dai->dev;
	struct vts_dma_data *data = vts_get_drvdata(substream);
	int result = 0;

	vts_dev_info(dev, "%s\n", __func__);

	/* vts_try_request_firmware_interface(data->vts_data); */
	pm_runtime_get_sync(dev);
	vts_start_runtime_resume(dev, 0);

	snd_soc_set_runtime_hwparams(substream, &vts_dma_hardware);

	/* Serial LIF Worked -> VTS (Trigger/Recoding) */
	if (data->vts_data->clk_path == VTS_CLK_SRC_AUD0) {
		data->vts_data->syssel_rate = 4;
		vts_clk_set_rate(&data->vts_data->pdev->dev,
			data->vts_data->syssel_rate);
		vts_dev_info(dev, "[Serial LIF Worked] Set VTS 3072000Hz\n");
	}

	if (data->type == PLATFORM_VTS_NORMAL_RECORD) {
		vts_dev_info(dev, "%s open --\n", __func__);
#ifdef VTS_SICD_CHECK
		vts_dev_info(dev, "SOC down : %d, MIF down : %d",
			readl(data->vts_data->sicd_base + SICD_SOC_DOWN_OFFSET),
			readl(data->vts_data->sicd_base + SICD_MIF_DOWN_OFFSET)
			);
#endif
		result = vts_set_dmicctrl(data->vts_data->pdev,
					VTS_MICCONF_FOR_RECORD, true);
		if (result < 0) {
			vts_dev_err(dev, "%s: MIC control configuration failed\n",
				__func__);
			pm_runtime_put_sync(dev);
		}
	}

	return result;
}

static int vts_dma_close(struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
	struct device *dev = dai->dev;
	struct vts_dma_data *data = vts_get_drvdata(substream);
	int result = 0;

	vts_dev_info(dev, "%s\n", __func__);
	vts_dev_dbg(dev, "%s 0x%x 0x%x \n",
			__func__,
			data->vts_data->shared_info->log_pos_read,
			data->vts_data->shared_info->log_pos_write);

	if (data->type == PLATFORM_VTS_NORMAL_RECORD) {
		vts_dev_info(dev, "%s close --\n", __func__);
#ifdef VTS_SICD_CHECK
		vts_dev_info(dev, "SOC down : %d, MIF down : %d",
			readl(data->vts_data->sicd_base + SICD_SOC_DOWN_OFFSET),
			readl(data->vts_data->sicd_base + SICD_MIF_DOWN_OFFSET)
			);
#endif
		result = vts_set_dmicctrl(data->vts_data->pdev,
					VTS_MICCONF_FOR_RECORD, false);
		if (result < 0)
			vts_dev_warn(dev, "%s: MIC control configuration failed\n",
				__func__);
	}

	pm_runtime_put_sync(dev);
	return result;
}

static int vts_dma_mmap(struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);
	struct device *dev = dai->dev;
	struct snd_pcm_runtime *runtime = substream->runtime;

	vts_dev_info(dev, "%s\n", __func__);

	return dma_mmap_wc(dev, vma,
			runtime->dma_area,
			runtime->dma_addr,
			runtime->dma_bytes);
}

static int vts_dma_copy_user(struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	int channel, unsigned long pos, void __user *buf, unsigned long bytes)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	void *hwbuf;

	hwbuf = runtime->dma_area + pos +
		channel * (runtime->dma_bytes / runtime->channels);
	if (copy_to_user((void __user *)buf, hwbuf, bytes))
		return -EFAULT;

	return 0;
}

static int vts_dma_new(struct snd_soc_component *component,
	struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(runtime, 0);
	struct snd_soc_dai_driver *dai_drv = dai->driver;
	struct device *dev = dai->dev;

	struct vts_data *vtsdata = dev_get_drvdata(dai->dev);
	struct vts_dma_data *data =
		platform_get_drvdata(vtsdata->pdev_vtsdma[dai_drv->id]);
	struct snd_pcm_substream *substream =
		runtime->pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;

	vts_dev_info(dev, "%s\n", __func__);
	data->substream = substream;
	vts_dev_info(dev, "%s Update Soc Card from runtime!!\n", __func__);
	data->vts_data->card = runtime->card;

	return 0;
}

static void vts_dma_free(struct snd_soc_component *component,
	struct snd_pcm *pcm)
{
}

static int vts_dma_ioctl(struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	unsigned int cmd, void *arg)
{
	int ret = 0;

	ret = snd_pcm_lib_ioctl(substream, cmd, arg);

	return ret;
}

static const struct snd_soc_component_driver vts_dma = {
	.pcm_construct	= vts_dma_new,
	.pcm_destruct	= vts_dma_free,
	.open		= vts_dma_open,
	.close		= vts_dma_close,
	.ioctl		= vts_dma_ioctl,
	.hw_params	= vts_dma_hw_params,
	.hw_free	= vts_dma_hw_free,
	.prepare	= vts_dma_prepare,
	.trigger	= vts_dma_trigger,
	.pointer	= vts_dma_pointer,
	.copy_user	= vts_dma_copy_user,
	.mmap		= vts_dma_mmap,
};

static struct snd_soc_dai_driver vts_dma_dai_drv = {
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16,
		.sig_bits = 16,
	},
};

static int samsung_vts_dma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *np_vts;
	struct vts_dma_data *data;
	int result;
	int ret = 0;
	const char *type;

	dev_info(dev, "%s\n", __func__);
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, data);
	data->dev = dev;

	np_vts = of_parse_phandle(np, "vts", 0);
	if (!np_vts) {
		dev_err(dev, "Failed to get vts device node\n");
		return -EPROBE_DEFER;
	}
	data->pdev_vts = of_find_device_by_node(np_vts);
	if (!data->pdev_vts) {
		dev_err(dev, "Failed to get vts platform device\n");
		return -EPROBE_DEFER;
	}
	data->vts_data = platform_get_drvdata(data->pdev_vts);

	result = of_property_read_u32_index(np, "id", 0, &data->id);
	if (result < 0) {
		dev_err(dev, "id property reading fail\n");
		return result;
	}

	result = of_property_read_string(np, "type", &type);
	if (result < 0) {
		dev_err(dev, "type property reading fail\n");
		return result;
	}

	if (!strncmp(type, "vts-record", sizeof("vts-record"))) {
		data->type = PLATFORM_VTS_NORMAL_RECORD;
		dev_info(dev, "%s - vts-record Probed\n", __func__);
	} else {
		data->type = PLATFORM_VTS_TRIGGER_RECORD;
		dev_info(dev, "%s - vts-trigger-record Probed\n", __func__);
	}

	vts_register_dma(data->vts_data->pdev, pdev, data->id);

	ret = devm_snd_soc_register_component(dev, &vts_dma,
						&vts_dma_dai_drv, 1);
	if (ret < 0) {
		dev_err(dev, "devm_snd_soc_register_component Fail");
		return ret;
	}
	dev_info(dev, "Probed successfully\n");
	return ret;
}

static int samsung_vts_dma_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id samsung_vts_dma_match[] = {
	{
		.compatible = "samsung,vts-dma",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_vts_dma_match);

struct platform_driver samsung_vts_dma_driver = {
	.probe  = samsung_vts_dma_probe,
	.remove = samsung_vts_dma_remove,
	.driver = {
		.name = "samsung-vts-dma",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_vts_dma_match),
	},
};
