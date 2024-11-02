/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __TFA98XX_INC__
#define __TFA98XX_INC__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <sound/pcm.h>

#include "tfa_device.h"
#include "tfa_container.h"
#include "config.h"
#include "tfa_internal.h"

#define SNDRV_PCM_STREAM_SAAM	(SNDRV_PCM_STREAM_LAST + 1)
/* for SaaM, SNDRV_PCM_STREAM_LAST + 1 */

#define TFA98XX_MAX_REGISTER              0xff

#define TFA98XX_FLAG_SKIP_INTERRUPTS	(1 << 0)
#define TFA98XX_FLAG_SAAM_AVAILABLE	(1 << 1)
#define TFA98XX_FLAG_STEREO_DEVICE	(1 << 2)
#define TFA98XX_FLAG_MULTI_MIC_INPUTS	(1 << 3)
#define TFA98XX_FLAG_TAPDET_AVAILABLE	(1 << 4)
#define TFA98XX_FLAG_CALIBRATION_CTL	(1 << 5)
#define TFA98XX_FLAG_REMOVE_PLOP_NOISE	(1 << 6)
#define TFA98XX_FLAG_LP_MODES	        (1 << 7)
#define TFA98XX_FLAG_TDM_DEVICE         (1 << 8)

#if defined(TFA_NO_SND_FORMAT_CHECK)
#define TFA98XX_NUM_RATES		14
#else
#define TFA98XX_NUM_RATES		9
#endif

/* DSP init status */
enum tfa98xx_dsp_init_state {
	TFA98XX_DSP_INIT_STOPPED,	/* DSP not running */
	TFA98XX_DSP_INIT_RECOVER,	/* DSP error detected at runtime */
	TFA98XX_DSP_INIT_FAIL,		/* DSP init failed */
	TFA98XX_DSP_INIT_PENDING,	/* DSP start requested */
	TFA98XX_DSP_INIT_DONE,		/* DSP running */
	TFA98XX_DSP_INIT_INVALIDATED,	/* DSP was running, requires re-init */
};

enum tfa98xx_dsp_fw_state {
	TFA98XX_DSP_FW_NONE = 0,
	TFA98XX_DSP_FW_PENDING,
	TFA98XX_DSP_FW_FAIL,
	TFA98XX_DSP_FW_OK,
};

struct tfa98xx_firmware {
	void *base;
	struct tfa98xx_device *dev;
	char name[9];	/* TODO: get length from tfa parameter */
};

struct tfa98xx_baseprofile {
	char basename[MAX_CONTROL_NAME];    /* profile basename */
	int len;                            /* profile length */
	int item_id;                        /* profile id */
	int sr_rate_sup[TFA98XX_NUM_RATES]; /* sample rates for profile */
	struct list_head list;              /* list of all profiles */
};

enum tfa_reset_polarity {
	LOW = 0,
	HIGH = 1
};

struct tfa98xx {
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct regulator *vdd;
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	struct snd_soc_component *component;
#else
	struct snd_soc_codec *codec;
#endif
	struct workqueue_struct *tfa98xx_wq;
	struct delayed_work init_work;
	struct delayed_work monitor_work;
	struct delayed_work interrupt_work;
	struct delayed_work tapdet_work;
	struct mutex dsp_lock;
	int dsp_init;
	int dsp_fw_state;
	int sysclk;
	int rst_gpio;
	u16 rev;
	int audio_mode;
	struct tfa98xx_firmware fw;
	char *fw_name;
	int rate;
#if defined(TFA_CHANGE_PCM_FORMAT)
	int hw_rate;
	int sample_size;
	int slot_size;
#endif
	wait_queue_head_t wq;
	struct device *dev;
	unsigned int init_count;
	int pstream;
	int cstream;
	int samstream;
	struct input_dev *input;
	bool tapdet_enabled;		/* service enabled */
	bool tapdet_open;		/* device file opened */
	unsigned int tapdet_profiles;	/* tapdet profile bitfield */
	bool tapdet_poll;		/* tapdet running on polling mode */

	unsigned int rate_constraint_list[TFA98XX_NUM_RATES];
	struct snd_pcm_hw_constraint_list rate_constraint;

	int reset_gpio;
	int power_gpio;
	int irq_gpio;
	enum tfa_reset_polarity reset_polarity;
	struct list_head list;
	struct tfa_device *tfa;
	int vstep;
	int profile;
	/* store vstep per profile (single device) */
	int prof_vsteps[TFACONT_MAXPROFS];

#ifdef CONFIG_DEBUG_FS
	struct dentry *dbg_dir;
#endif
	u8 reg;
	unsigned int flags;
	bool set_mtp_cal;
	uint16_t cal_data;
	int calibrate_done;
#if defined(TFA_SET_IPC_PORT_ID)
	int pid;
#endif
};

#endif /* __TFA98XX_INC__ */

