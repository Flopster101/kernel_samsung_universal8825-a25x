/* sound/soc/samsung/vts/slif_soc.h
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_SLIF_LIF_SOC_H
#define __SND_SOC_SLIF_LIF_SOC_H

#include <sound/memalloc.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/soc-dai.h>

#define SLIF_LIF_PAD_ROUTE

int slif_soc_set_fmt(struct slif_data *dai, unsigned int fmt);

int slif_soc_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params,
		struct slif_data *data);

int slif_soc_startup(struct snd_pcm_substream *substream,
		struct slif_data *data);

void slif_soc_shutdown(struct snd_pcm_substream *substream,
		struct slif_data *data);

int slif_soc_hw_free(struct snd_pcm_substream *substream,
		struct slif_data *data);

int slif_soc_dma_en(int cmd,
		struct slif_data *data);

int slif_soc_probe(struct slif_data *data);

int slif_soc_dmic_aud_gain_mode_get(struct slif_data *data,
		unsigned int id, unsigned int *val);
int slif_soc_dmic_aud_gain_mode_put(struct slif_data *data,
		unsigned int id, unsigned int val);
int slif_soc_dmic_aud_gain_mode_write(struct slif_data *data,
		unsigned int id);

int slif_soc_dmic_aud_max_scale_gain_get(struct slif_data *data,
		unsigned int id, unsigned int *val);
int slif_soc_dmic_aud_max_scale_gain_put(struct slif_data *data,
		unsigned int id, unsigned int val);
int slif_soc_dmic_aud_max_scale_gain_write(struct slif_data *data,
		unsigned int id);

int slif_soc_dmic_aud_control_gain_get(struct slif_data *data,
		unsigned int id, unsigned int *val);
int slif_soc_dmic_aud_control_gain_put(struct slif_data *data,
		unsigned int id, unsigned int val);
int slif_soc_dmic_aud_control_gain_write(struct slif_data *data,
		unsigned int id);

int slif_soc_vol_set_get(struct slif_data *data,
		unsigned int id, unsigned int *val);
int slif_soc_vol_set_put(struct slif_data *data,
		unsigned int id, unsigned int val);

int slif_soc_vol_change_get(struct slif_data *data,
		unsigned int id, unsigned int *val);
int slif_soc_vol_change_put(struct slif_data *data,
		unsigned int id, unsigned int val);

int slif_soc_channel_map_get(struct slif_data *data,
		unsigned int id, unsigned int *val);
int slif_soc_channel_map_put(struct slif_data *data,
		unsigned int id, unsigned int val);

int slif_soc_dmic_aud_control_hpf_sel_get(struct slif_data *data,
		unsigned int id, unsigned int *val);
int slif_soc_dmic_aud_control_hpf_sel_put(struct slif_data *data,
		unsigned int id, unsigned int val);

int slif_soc_dmic_aud_control_hpf_en_get(struct slif_data *data,
		unsigned int id, unsigned int *val);
int slif_soc_dmic_aud_control_hpf_en_put(struct slif_data *data,
		unsigned int id, unsigned int val);

int slif_soc_dmic_en_get(struct slif_data *data,
		unsigned int id, unsigned int *val);
int slif_soc_dmic_en_put(struct slif_data *data,
		unsigned int id, unsigned int val, bool update);

void slif_debug_pad_en(int en);

#endif /* __SND_SOC_SLIF_LIF_SOC_H */
