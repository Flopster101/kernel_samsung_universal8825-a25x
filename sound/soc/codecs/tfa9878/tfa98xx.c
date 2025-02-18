/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) "%s(): " fmt, __func__

#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/input.h>
#include "inc/config.h"
#include "inc/tfa98xx.h"
#include "inc/tfa.h"
#include "inc/tfa_internal.h"

/* required for enum tfa9912_irq */
#include "inc/tfa98xx_tfafieldnames.h"

#define TFA98XX_VERSION	TFA98XX_API_REV_STR

#define I2C_RETRIES 50
#define I2C_RETRY_DELAY 5 /* ms */
#define TFA_RESET_DELAY 5 /* ms */

#ifdef N1A
#include "inc/tfa98xx_genregs_N1A12.h"
#else
#include "inc/tfa98xx_genregs_N1C.h"
#endif

#if defined(TFA_READ_REFERENCE_TEMP)
#if defined(TFA_USE_POWER_SUPPLY)
#include <linux/power_supply.h>
#elif defined(TFA_USE_THERMAL_SENSOR)
#include <linux/thermal.h>
#endif
#endif

#ifdef MPLATFORM
#include <mtk-sp-spk-amp.h>
#endif

#define TFA98XX_VERSION	TFA98XX_API_REV_STR

#define TFA_SET_DAPM_IGNORE_SUSPEND
#define TFA_STOP_AND_RESTART_FOR_CALIBRATION
#define TFA_BYPASS_BEFORE_RECONFIG

/* Change volume selection behavior:
 * Uncomment following line to generate a profile change when updating
 * a volume control (also changes to the profile of the modified volume
 * control)
 */
/*#define TFA98XX_ALSA_CTRL_PROF_CHG_ON_VOL	1*/

/* Supported rates and data formats */
#if !defined(TFA_NO_SND_FORMAT_CHECK)
#if defined(USE_TFA9874) || defined(USE_TFA9878)
#define TFA98XX_RATES (SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_32000 | \
SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)
#elif defined(USE_TFA9894)
#if defined(USE_TFA9894N2)
#define TFA98XX_RATES (SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_32000 | \
SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)
#else
#define TFA98XX_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)
#endif
#elif defined(USE_TFA9872)
#define TFA98XX_RATES (SNDRV_PCM_RATE_16000 | \
SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)
#else
#define TFA98XX_RATES SNDRV_PCM_RATE_8000_48000
#endif
#else
/* if post-conversion is assumed */
#define TFA98XX_RATES SNDRV_PCM_RATE_8000_192000
#endif

#if defined(TFA_FIXED_RATE_FOR_DSP)
static unsigned int sr_converted = 48000;
#endif

#define TFA98XX_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

#undef TFA_RETRY_TO_START
#if defined(TFA_RETRY_TO_START)
#define TF98XX_MAX_DSP_START_TRY_COUNT	10
#endif

#define TFA_FS_CHECK_MTPEX
#define CALFUNC_IN_SYSFS
#define LOGFUNC_IN_SYSFS
#define GAINFUNC_IN_SYSFS

/* data accessible by all instances */
/* Memory pool used for DSP messages */
static struct kmem_cache *tfa98xx_cache;
/* Mutex protected data */
static DEFINE_MUTEX(tfa98xx_mutex);
static DEFINE_MUTEX(probe_lock);
static LIST_HEAD(tfa98xx_device_list);
static int tfa98xx_device_count;
static int tfa98xx_sync_count;
static int tfa98xx_monitor_count;
#define MONITOR_COUNT_MAX 5

static LIST_HEAD(profile_list); /* list of user selectable profiles */
static int tfa98xx_mixer_profiles; /* number of user selectable profiles */
static int tfa98xx_mixer_profile;  /* current mixer profile */
static struct snd_kcontrol_new *tfa98xx_controls;
static struct tfa_container *tfa98xx_container;

#if defined(TFADSP_DSP_BUFFER_POOL)
static int buf_pool_size[POOL_MAX_INDEX] = {
	64 * 1024,
	64 * 1024,
	64 * 1024,
	64 * 1024,
	64 * 1024,
	8 * 1024
};
#endif

static int tfa98xx_kmsg_regs;
static int tfa98xx_ftrace_regs;

static char *fw_name = "tfa98xx.cnt";
module_param(fw_name, charp, 0644);
MODULE_PARM_DESC(fw_name, "TFA98xx DSP firmware (container file) name.");

static int trace_level;
module_param(trace_level, int, 0444);
MODULE_PARM_DESC(trace_level, "TFA98xx debug trace level (0=off, bits:1=verbose,2=regdmesg,3=regftrace,4=timing).");

static char *dflt_prof_name = "";
module_param(dflt_prof_name, charp, 0444);

static int no_start;
module_param(no_start, int, 0444);
MODULE_PARM_DESC(no_start, "do not start the work queue; for debugging via user\n");

static int no_reset;
module_param(no_reset, int, 0444);
MODULE_PARM_DESC(no_reset, "do not use the reset line; for debugging via user\n");

#if !defined(TFA_CHANGE_PCM_FORMAT)
static int pcm_sample_format = -1;
#else
static int pcm_sample_format = 3;
#endif
/*
 * Be careful. Setting pcm_sample_format to 3 means
 * TDM settings will be dynamically adapted,
 * If there's TDM setting in container file (cnt),
 * it's to be overwritten with what's specified by hw_params.
 */
module_param(pcm_sample_format, int, 0444);
#if !defined(TFA_CHANGE_PCM_FORMAT)
MODULE_PARM_DESC(pcm_sample_format, "PCM sample format: 0=S16_LE, 1=S24_LE, 2=S32_LE, -1=all\n");
#else
MODULE_PARM_DESC(pcm_sample_format, "PCM sample format: 0=S16_LE, 1=S24_LE, 2=S32_LE, 3=dynamic, -1=all\n");
#endif

#if defined(TFA_NO_SND_FORMAT_CHECK)
static int pcm_no_constraint = 1;
#else
static int pcm_no_constraint;
#endif
module_param(pcm_no_constraint, int, 0444);
MODULE_PARM_DESC(pcm_no_constraint, "do not use constraints for PCM parameters\n");

static void tfa98xx_dsp_init(struct tfa98xx *tfa98xx);

#if (defined(USE_TFA9891) || defined(USE_TFA9912))
static void tfa98xx_tapdet_check_update(struct tfa98xx *tfa98xx);
#endif
static void tfa98xx_interrupt_enable(struct tfa98xx *tfa98xx, bool enable);

static int get_profile_from_list(char *buf, int id);
static int get_profile_id_for_sr(int id, unsigned int rate);

static int _tfa98xx_mute(struct tfa98xx *tfa98xx, int mute, int stream);
static int _tfa98xx_stop(struct tfa98xx *tfa98xx);

#if defined(TFA_FS_CHECK_MTPEX)
static void tfa98xx_check_calibration(struct tfa98xx *tfa98xx);
#endif
static int tfa98xx_run_calibration(struct tfa98xx *tfa98xx);

#if defined(TFA_BYPASS_BEFORE_RECONFIG)
static enum tfa98xx_error tfa98xx_set_tfadsp_bypass(struct tfa_device *tfa);
#endif

#if defined(TFA_READ_REFERENCE_TEMP)
static enum tfa98xx_error tfa98xx_read_reference_temp(short *value);
#endif

static void tfa98xx_set_dsp_configured(struct tfa98xx *tfa98xx);

struct tfa98xx_rate {
	unsigned int rate;
	unsigned int fssel;
};

static const struct tfa98xx_rate rate_to_fssel[] = {
	{8000, 0},
	{11025, 1},
	{12000, 2},
	{16000, 3},
	{22050, 4},
	{24000, 5},
	{32000, 6},
	{44100, 7},
	{48000, 8},
#if defined(TFA_NO_SND_FORMAT_CHECK)
/* out of range */
	{64000, 9},
	{88200, 10},
	{96000, 11},
	{176400, 12},
	{192000, 13},
#endif
};

#if !defined(TFA_CHECK_PROFILE_RATE)
static const unsigned int index_to_rate[] = {
	5512, 8000, 11025, 16000, 22050, 32000, 44100, 48000
};
#endif

static inline char *_tfa_cont_profile_name
(struct tfa98xx *tfa98xx, int prof_idx)
{
	if (tfa98xx->tfa->cnt == NULL)
		return NULL;

	return tfa_cont_profile_name(tfa98xx->tfa->cnt,
		tfa98xx->tfa->dev_idx, prof_idx);
}

static enum tfa_error tfa98xx_write_re25(struct tfa_device *tfa, int value)
{
	enum tfa_error err;

	/* clear MTPEX */
	err = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 0);
	if (err == tfa_error_ok) {
		/* set RE25 in shadow regiser */
		err = tfa_dev_mtp_set(tfa, TFA_MTP_RE25_PRIM, value);
	}
	if (err == tfa_error_ok) {
		/* set MTPEX to copy RE25 into MTP */
		err = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 2);
	}

	return err;
}

/* Wrapper for tfa start */
static enum tfa_error
tfa98xx_tfa_start(struct tfa98xx *tfa98xx, int next_profile, int vstep)
{
	enum tfa_error err;
	ktime_t start_time = 0;
	ktime_t stop_time = 0;
	u64 delta_time;

	if (trace_level & 8)
		start_time = ktime_get_boottime();

#if defined(TFA_CHANGE_PCM_FORMAT)
	/* config PCM format from hw_params, before start */
	err = tfa_dev_config_pcm_format(tfa98xx->tfa,
		tfa98xx_device_count,
		tfa98xx->hw_rate, tfa98xx->sample_size, tfa98xx->slot_size);
#endif
	err = tfa_dev_start(tfa98xx->tfa, next_profile, vstep);

	if (trace_level & 8) {
		stop_time = ktime_get_boottime();
		delta_time = ktime_to_ns(ktime_sub(stop_time, start_time));
		do_div(delta_time, 1000);
		dev_dbg(tfa98xx->dev, "tfa_dev_start(%d,%d) time = %lld us\n",
			next_profile, vstep, delta_time);
	}

	if ((err == tfa_error_ok) && (tfa98xx->set_mtp_cal)) {
		enum tfa_error err_cal;

		err_cal = tfa98xx_write_re25(tfa98xx->tfa, tfa98xx->cal_data);
		if (err_cal != tfa_error_ok) {
			pr_err("Error, setting calibration value in mtp, err=%d\n",
				err_cal);
		} else {
			tfa98xx->set_mtp_cal = false;
			pr_info("Calibration value (%d) set in mtp\n",
				tfa98xx->cal_data);
		}
	}

#if (defined(USE_TFA9891) || defined(USE_TFA9912))
	/* Check and update tap-detection state (in case of profile change) */
	tfa98xx_tapdet_check_update(tfa98xx);
#endif

	/* Remove sticky bit by reading it once */
	tfa_get_noclk(tfa98xx->tfa);

	/* A cold start erases the configuration, including interrupts setting.
	 * Restore it if required
	 */
	tfa98xx_interrupt_enable(tfa98xx, true);

	return err;
}

#if (defined(USE_TFA9891) || defined(USE_TFA9912))
static int tfa98xx_input_open(struct input_dev *dev)
{
	struct tfa98xx *tfa98xx = input_get_drvdata(dev);
	struct device *cdev;

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	cdev = tfa98xx->component->dev;
#else
	cdev = tfa98xx->codec->dev;
#endif

	dev_dbg(cdev, "opening device file\n");

	/* note: open function is called only once by the framework.
	 * No need to count number of open file instances.
	 */
	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		dev_dbg(tfa98xx->dev,
			"DSP not loaded, cannot start tap-detection\n");
		return -EIO;
	}

	/* enable tap-detection service */
	tfa98xx->tapdet_open = true;
	tfa98xx_tapdet_check_update(tfa98xx);

	return 0;
}

static void tfa98xx_input_close(struct input_dev *dev)
{
	struct tfa98xx *tfa98xx = input_get_drvdata(dev);
	struct device *cdev;

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	cdev = tfa98xx->component->dev;
#else
	cdev = tfa98xx->codec->dev;
#endif

	dev_dbg(cdev, "closing device file\n");

	/* Note: close function is called if the device is unregistered */

	/* disable tap-detection service */
	tfa98xx->tapdet_open = false;
	tfa98xx_tapdet_check_update(tfa98xx);
}

static int tfa98xx_register_inputdev(struct tfa98xx *tfa98xx)
{
	int err;
	struct input_dev *input;
	struct device *cdev;

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	cdev = tfa98xx->component->dev;
#else
	cdev = tfa98xx->codec->dev;
#endif

	input = input_allocate_device();

	if (!input) {
		dev_err(cdev, "Unable to allocate input device\n");
		return -ENOMEM;
	}

	input->evbit[0] = BIT_MASK(EV_KEY);
	input->keybit[BIT_WORD(BTN_0)] |= BIT_MASK(BTN_0);
	input->keybit[BIT_WORD(BTN_1)] |= BIT_MASK(BTN_1);
	input->keybit[BIT_WORD(BTN_2)] |= BIT_MASK(BTN_2);
	input->keybit[BIT_WORD(BTN_3)] |= BIT_MASK(BTN_3);
	input->keybit[BIT_WORD(BTN_4)] |= BIT_MASK(BTN_4);
	input->keybit[BIT_WORD(BTN_5)] |= BIT_MASK(BTN_5);
	input->keybit[BIT_WORD(BTN_6)] |= BIT_MASK(BTN_6);
	input->keybit[BIT_WORD(BTN_7)] |= BIT_MASK(BTN_7);
	input->keybit[BIT_WORD(BTN_8)] |= BIT_MASK(BTN_8);
	input->keybit[BIT_WORD(BTN_9)] |= BIT_MASK(BTN_9);

	input->open = tfa98xx_input_open;
	input->close = tfa98xx_input_close;

	input->name = "tfa98xx-tapdetect";

	input->id.bustype = BUS_I2C;
	input_set_drvdata(input, tfa98xx);

	err = input_register_device(input);
	if (err) {
		dev_err(cdev, "Unable to register input device\n");
		goto err_free_dev;
	}

	dev_dbg(cdev, "Input device for tap-detection registered: %s\n",
		input->name);
	tfa98xx->input = input;

	return 0;

err_free_dev:
	input_free_device(input);

	return err;
}

/*
 * Check if an input device for tap-detection can and shall be registered.
 * Register it if appropriate.
 * If already registered, check if still relevant and remove it if necessary.
 * unregister: true to request inputdev unregistration.
 */
static void
__tfa98xx_inputdev_check_register(struct tfa98xx *tfa98xx,
	bool unregister)
{
	bool tap_profile = false;
	unsigned int i;
	struct device *cdev;

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	cdev = tfa98xx->component->dev;
#else
	cdev = tfa98xx->codec->dev;
#endif

	for (i = 0; i < tfa_cnt_get_dev_nprof(tfa98xx->tfa); i++) {
		if (strnstr(_tfa_cont_profile_name(tfa98xx, i), ".tap",
			strlen(_tfa_cont_profile_name(tfa98xx, i)))
			!= NULL) {
			tap_profile = true;
			tfa98xx->tapdet_profiles |= 1 << i;
			dev_info(cdev, "found a tap-detection profile (%d - %s)\n",
				i, _tfa_cont_profile_name(tfa98xx, i));
		}
	}

	/* Check for device support:
	 * - at device level
	 * - at container (profile) level
	 */
	if (!(tfa98xx->flags & TFA98XX_FLAG_TAPDET_AVAILABLE) ||
		!tap_profile ||
		unregister) {
		/* No input device supported or required */
		if (tfa98xx->input) {
			input_unregister_device(tfa98xx->input);
			tfa98xx->input = NULL;
		}
		return;
	}

	/* input device required */
	if (tfa98xx->input)
		dev_info(cdev, "Input device already registered, skipping\n");
	else
		tfa98xx_register_inputdev(tfa98xx);
}

static void tfa98xx_inputdev_check_register(struct tfa98xx *tfa98xx)
{
	__tfa98xx_inputdev_check_register(tfa98xx, false);
}

static void tfa98xx_inputdev_unregister(struct tfa98xx *tfa98xx)
{
	__tfa98xx_inputdev_check_register(tfa98xx, true);
}
#endif /* (USE_TFA9891) || (USE_TFA9912) */

#if defined(CONFIG_DEBUG_FS)
/* OTC reporting
 * Returns the MTP0 OTC bit value
 */
static int tfa98xx_dbgfs_otc_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int value;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_OTC);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value < 0) {
		pr_err("[0x%x] Unable to access MTPOTC: %d\n",
			tfa98xx->i2c->addr, value);
		return -EIO;
	}

	*val = value;
	pr_debug("[0x%x] OTC : %d\n", tfa98xx->i2c->addr, value);

	return 0;
}

static int tfa98xx_dbgfs_otc_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error err;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (val != 0 && val != 1) {
		pr_err("[0x%x] Unexpected value %llu\n",
			tfa98xx->i2c->addr, val);
		return -EINVAL;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	err = tfa_dev_mtp_set(tfa98xx->tfa, TFA_MTP_OTC, val);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (err != tfa_error_ok) {
		pr_err("[0x%x] Unable to access MTPOTC: err %d\n",
			tfa98xx->i2c->addr, err);
		return -EIO;
	}

	pr_debug("[0x%x] OTC < %llu\n", tfa98xx->i2c->addr, val);

	return 0;
}

static int tfa98xx_dbgfs_mtpex_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int value;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_EX);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value < 0) {
		pr_err("[0x%x] Unable to access MTPEX: %d\n",
			tfa98xx->i2c->addr, value);
		return -EIO;
	}

	*val = value;
	pr_debug("[0x%x] MTPEX : %d\n", tfa98xx->i2c->addr, value);

	return 0;
}

static int tfa98xx_dbgfs_mtpex_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error err;
#if defined(TFA_READ_REFERENCE_TEMP)
	enum tfa98xx_error ret;
	u16 temp_val = DEFAULT_REF_TEMP;
	int idx, ndev;
	struct tfa_device *ntfa = NULL;
#endif

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (val != 0) {
		pr_err("[0x%x] Can only clear MTPEX (0 value expected)\n",
			tfa98xx->i2c->addr);
		return -EINVAL;
	}

#if defined(TFA_READ_REFERENCE_TEMP)
	/* EXT_TEMP */
	ret = tfa98xx_read_reference_temp(&temp_val);
	if (ret)
		pr_err("error in reading reference temp\n");

	ndev = tfa98xx->tfa->dev_count;
	for (idx = 0; idx < ndev; idx++) {
		ntfa = tfa98xx_get_tfa_device_from_channel(idx);
		if (ntfa == NULL)
			continue;
		tfa98xx_set_exttemp(ntfa, (short)temp_val);
	}
#endif

	mutex_lock(&tfa98xx->dsp_lock);
	err = tfa_dev_mtp_set(tfa98xx->tfa, TFA_MTP_EX, 0);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (err != tfa_error_ok) {
		pr_err("[0x%x] Unable to access MTPEX: err %d (suspended)\n",
			tfa98xx->i2c->addr, err);
		/* suspend until TFA98xx is active */
		tfa98xx->tfa->reset_mtpex = 1;
		return -EIO;
	}

	pr_debug("[0x%x] MTPEX < 0\n", tfa98xx->i2c->addr);

	return 0;
}

static int tfa98xx_dbgfs_temp_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	*val = tfa98xx_get_exttemp(tfa98xx->tfa);
	mutex_unlock(&tfa98xx->dsp_lock);

	pr_debug("[0x%x] TEMP : %llu\n", tfa98xx->i2c->addr, *val);

	return 0;
}

static int tfa98xx_dbgfs_temp_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	tfa98xx_set_exttemp(tfa98xx->tfa, (short)val);
	mutex_unlock(&tfa98xx->dsp_lock);

	pr_debug("[0x%x] TEMP < %llu\n", tfa98xx->i2c->addr, val);

	return 0;
}

static ssize_t tfa98xx_dbgfs_start_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;
	int ret = 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

#if defined(TFA_FS_CHECK_MTPEX)
	tfa98xx_check_calibration(tfa98xx);
#endif

	if (tfa98xx->calibrate_done) {
		pr_info("[0x%x] Calibration Success\n", tfa98xx->i2c->addr);
		snprintf(str, PAGE_SIZE, "Success\n");
		ret = sizeof("Success");
	} else {
		pr_info("[0x%x] Calibration Fail\n", tfa98xx->i2c->addr);
		snprintf(str, PAGE_SIZE, "Fail\n");
		ret = sizeof("Fail");
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
	kfree(str);

	return ret;
}

static ssize_t tfa98xx_dbgfs_start_set(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	char buf[32];
	int buf_size;
	static const char ref[] = "1"; /* "please calibrate now" */

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	/* check string length, and account for eol */
	if (count > sizeof(ref) + 1 || count < (sizeof(ref) - 1))
		return -EINVAL;

	buf_size = min(count, (size_t)(sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (strncmp(buf, ref, sizeof(ref) - 1)) {
		pr_info("[0x%x] %s: calibration is triggered with %s!\n",
			tfa98xx->i2c->addr, __func__, ref);
		return -EINVAL;
	}

	ret = tfa98xx_run_calibration(tfa98xx);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t tfa98xx_dbgfs_r_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;
	uint16_t status;
	int ret;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);

	/* Need to ensure DSP is access-able, use mtp read access for this
	 * purpose
	 */
	ret = tfa98xx_get_mtp(tfa98xx->tfa, &status);
	if (ret) {
		ret = -EIO;
		pr_err("[0x%x] MTP read failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	ret = tfa_run_speaker_calibration(tfa98xx->tfa);
	if (ret) {
		ret = -EIO;
		pr_err("[0x%x] calibration failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str) {
		ret = -ENOMEM;
		pr_err("[0x%x] memory allocation failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	if (tfa98xx->tfa->spkr_count > 1) {
		ret = snprintf(str, PAGE_SIZE,
			"Prim:%d mOhms, Sec:%d mOhms\n",
			tfa98xx->tfa->mohm[0],
			tfa98xx->tfa->mohm[1]);
	} else {
		ret = snprintf(str, PAGE_SIZE,
			"Prim:%d mOhms\n",
			tfa98xx->tfa->mohm[0]);
	}

	pr_debug("[0x%x] calib_done: %s", tfa98xx->i2c->addr, str);

	if (ret < 0)
		goto r_err;

	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);

r_err:
	kfree(str);
r_c_err:
	mutex_unlock(&tfa98xx->dsp_lock);
	return ret;
}

static ssize_t tfa98xx_dbgfs_version_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	char str[] = TFA98XX_VERSION "\n";
	int ret;

	ret = simple_read_from_buffer(user_buf, count, ppos, str, sizeof(str));

	return ret;
}

static ssize_t tfa98xx_dbgfs_dsp_state_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	char *str;

	switch (tfa98xx->dsp_init) {
	case TFA98XX_DSP_INIT_STOPPED:
		str = "Stopped\n";
		break;
	case TFA98XX_DSP_INIT_RECOVER:
		str = "Recover requested\n";
		break;
	case TFA98XX_DSP_INIT_FAIL:
		str = "Failed init\n";
		break;
	case TFA98XX_DSP_INIT_PENDING:
		str = "Pending init\n";
		break;
	case TFA98XX_DSP_INIT_DONE:
		str = "Init complete\n";
		break;
	default:
		str = "Invalid\n";
		break;
	}

	pr_debug("[0x%x] dsp_state : %s\n", tfa98xx->i2c->addr, str);

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));
	return ret;
}

static ssize_t tfa98xx_dbgfs_dsp_state_set(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error ret;
	char buf[32];
	static const char start_cmd[] = "start";
	static const char stop_cmd[] = "stop";
	static const char mon_start_cmd[] = "monitor start";
	static const char mon_stop_cmd[] = "monitor stop";
	int buf_size;

	buf_size = min(count, (size_t)(sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	/* Compare strings, excluding the trailing \0 */
	if (!strncmp(buf, start_cmd, sizeof(start_cmd) - 1)) {
		pr_info("[0x%x] Manual triggering of dsp start...\n",
			tfa98xx->i2c->addr);
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa98xx_tfa_start(tfa98xx,
			tfa98xx->profile, tfa98xx->vstep);
		mutex_unlock(&tfa98xx->dsp_lock);
		pr_debug("[0x%x] tfa_dev_start complete: %d\n",
			tfa98xx->i2c->addr, ret);
	} else if (!strncmp(buf, stop_cmd, sizeof(stop_cmd) - 1)) {
		pr_info("[0x%x] Manual triggering of dsp stop...\n",
			tfa98xx->i2c->addr);
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa_dev_stop(tfa98xx->tfa);
		mutex_unlock(&tfa98xx->dsp_lock);
		pr_debug("[0x%x] tfa_dev_stop complete: %d\n",
			tfa98xx->i2c->addr, ret);
	} else if (!strncmp(buf, mon_start_cmd,
		sizeof(mon_start_cmd) - 1)) {
		pr_info("[0x%x] Manual start of monitor thread...\n",
			tfa98xx->i2c->addr);
		tfa98xx_monitor_count = -1;
		queue_delayed_work(tfa98xx->tfa98xx_wq,
			&tfa98xx->monitor_work, HZ);
	} else if (!strncmp(buf, mon_stop_cmd,
		sizeof(mon_stop_cmd) - 1)) {
		pr_info("[0x%x] Manual stop of monitor thread...\n",
			tfa98xx->i2c->addr);
		cancel_delayed_work_sync(&tfa98xx->monitor_work);
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t tfa98xx_dbgfs_fw_state_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;

	switch (tfa98xx->dsp_fw_state) {
	case TFA98XX_DSP_FW_NONE:
		str = "None\n";
		break;
	case TFA98XX_DSP_FW_PENDING:
		str = "Pending\n";
		break;
	case TFA98XX_DSP_FW_FAIL:
		str = "Fail\n";
		break;
	case TFA98XX_DSP_FW_OK:
		str = "Ok\n";
		break;
	default:
		str = "Invalid\n";
		break;
	}

	pr_debug("[0x%x] fw_state : %s", tfa98xx->i2c->addr, str);

	return simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));
}

static ssize_t tfa98xx_dbgfs_rpc_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	uint8_t *buffer;
	enum tfa98xx_error error;
	struct tfa_device *tfa0 = NULL;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (tfa98xx->pstream == 0
		|| tfa98xx->tfa->is_configured <= 0) {
		pr_info("%s: skipped - tfadsp is not active!\n",
			__func__);
		return -EIO;
	}

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}

	tfa0 = tfa98xx->tfa;

	pr_info("%s called (count %d)\n", __func__, (int)count);
	mutex_lock(&tfa98xx->dsp_lock);
	error = dsp_msg_read(tfa0, count, buffer);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error != TFA98XX_ERROR_OK) {
		pr_debug("[0x%x] dsp_msg_read error: %d\n",
			tfa98xx->i2c->addr, error);
		kfree(buffer);
		return -EFAULT;
	}

	ret = copy_to_user(user_buf, buffer, count);
	kfree(buffer);
	if (ret)
		return -EFAULT;

	*ppos += count;
	return count;
}

static ssize_t tfa98xx_dbgfs_rpc_send(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	struct tfa_file_dsc *msg_file;
	enum tfa98xx_error error;
	struct tfa_device *tfa0 = NULL;
	int ret = 0;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (tfa98xx->pstream == 0
		|| tfa98xx->tfa->is_configured <= 0) {
		pr_info("%s: skipped - tfadsp is not active!\n",
			__func__);
		return -EIO;
	}

	/* msg_file.name is not used */
	msg_file = kmalloc(count + sizeof(struct tfa_file_dsc), GFP_KERNEL);
	if (msg_file == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}
	msg_file->size = (uint32_t)count;

	tfa0 = tfa98xx->tfa;

	if (copy_from_user(msg_file->data, user_buf, count)) {
		kfree(msg_file);
		return -EFAULT;
	}

	pr_info("%s called\n", __func__);
	mutex_lock(&tfa98xx->dsp_lock);
	tfa0->individual_msg = 1;
	if ((msg_file->data[0] == 'M') && (msg_file->data[1] == 'G')) {
		/* int vstep_idx, int vstep_msg_idx both 0 */
		error = tfa_cont_write_file(tfa0,
			msg_file, 0, 0);
		if (error != TFA98XX_ERROR_OK) {
			pr_debug("[0x%x] tfa_cont_write_file error: %d\n",
				tfa98xx->i2c->addr, error);
			ret = -EIO;
		}
	} else {
		error = dsp_msg(tfa0, msg_file->size, msg_file->data);
		if (error != TFA98XX_ERROR_OK) {
			pr_debug("[0x%x] dsp_msg error: %d\n",
				tfa98xx->i2c->addr, error);
			ret = -EIO;
		}
	}
	mutex_unlock(&tfa98xx->dsp_lock);

	kfree(msg_file);

	if (ret)
		return ret;

	return count;
}
/* -- RPC */

/* ++ DSP message fops */
static ssize_t tfa98xx_dbgfs_dsp_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa98xx_error error;
	int ret = 0;
	uint8_t *buffer;
	struct tfa_device *tfa0 = NULL;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (tfa98xx->pstream == 0
		|| tfa98xx->tfa->is_configured <= 0) {
		pr_info("%s: skipped - tfadsp is not active!\n",
			__func__);
		return -EIO;
	}

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}

	tfa0 = tfa98xx->tfa;

	pr_info("%s called (count %d)\n", __func__, (int)count);
	mutex_lock(&tfa98xx->dsp_lock);
	error = dsp_msg_read(tfa0, count, buffer);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error) {
		pr_debug("[0x%x] dsp_msg_read error: %d\n",
			tfa98xx->i2c->addr, error);
		kfree(buffer);
		return -EFAULT;
	}

	/* ret = simple_read_from_buffer
	 * (user_buf, count, ppos, buffer, count);
	 */
	ret = copy_to_user(user_buf, buffer, count);
	if (ret) {
		pr_debug("[0x%x] cannot copy buffer to user: %d\n",
			tfa98xx->i2c->addr, ret);
		kfree(buffer);
		return -EFAULT;
	}

	kfree(buffer);

	return count;
}

static ssize_t tfa98xx_dbgfs_dsp_write(struct file *file,
	const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa98xx_error error;
	int ret = 0;
	struct tfa_device *tfa0 = NULL;
	uint8_t *buffer;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (tfa98xx->pstream == 0
		|| tfa98xx->tfa->is_configured <= 0) {
		pr_info("%s: skipped - tfadsp is not active!\n",
			__func__);
		return -EIO;
	}

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}

	tfa0 = tfa98xx->tfa;

	ret = copy_from_user(buffer, user_buf, count);
	if (ret) {
		pr_debug("[0x%x] cannot copy buffer from user: %d\n",
			tfa98xx->i2c->addr, ret);
		kfree(buffer);
		return -EFAULT;
	}

	pr_info("%s called\n", __func__);
	mutex_lock(&tfa98xx->dsp_lock);
	tfa0->individual_msg = 1;
	error = dsp_msg(tfa0, count, buffer);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error) {
		pr_debug("[0x%x] dsp_msg error: %d\n",
			tfa98xx->i2c->addr, error);
		kfree(buffer);
		return -EFAULT;
	}

	kfree(buffer);

	return count;
}
/* -- DSP */

static ssize_t tfa98xx_dbgfs_spkr_damaged_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	char *str;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	str = kmalloc(count, GFP_KERNEL);
	if (str == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}

	scnprintf(str, PAGE_SIZE, "%s\n",
		(tfa98xx->tfa->spkr_damaged == 1)
		? "damaged" : "ready");

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));

	kfree(str);

	return ret;
}

static int tfa98xx_dbgfs_pga_gain_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	unsigned int value;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	value = tfa_get_pga_gain(tfa98xx->tfa);
	if (value < 0)
		return -EINVAL;
	*val = value;

	return 0;
}

static int tfa98xx_dbgfs_pga_gain_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	uint16_t value;
	int err;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	value = val & 0xffff;
	if (value > 7)
		return -EINVAL;

	err = tfa_set_pga_gain(tfa98xx->tfa, value);
	if (err < 0)
		return -EINVAL;

	return 0;
}

static ssize_t tfa98xx_dbgfs_trace_level_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	char out_buf[4] = {0};
	int ret = 0;

	if (count < 1) {
		pr_err("%s: read size exceeds buf size %zd\n", __func__, count);
		return 0;
	}
	snprintf(out_buf, 4, "%d\n", trace_level);

	ret = simple_read_from_buffer(user_buf,
		count, ppos, out_buf, sizeof(out_buf));

	return ret;
}

static ssize_t tfa98xx_dbgfs_trace_level_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int tl = 0;
	char buf[2] = {0};

	if (copy_from_user(buf, user_buf, 1))
		return -EFAULT;

	tl = buf[0] - 48;
	pr_info("%s: trace_level = %d\n", __func__, tl);
	if (tl < 0 || tl > 15)
		return -EFAULT;

	trace_level = tl;
	tfa98xx_kmsg_regs = trace_level & 2;
	tfa98xx_ftrace_regs = trace_level & 4;
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		if (tfa98xx && tfa98xx->tfa)
			tfa98xx->tfa->verbose = trace_level & 1;
	}

	return count;
}

static ssize_t tfa98xx_dbgfs_show_cal_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	int mtp = 0;
	int mtpex = 0;
	char out_buf[64] = {0};

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (count < 1) {
		pr_err("%s: read size exceeds buf size %zd\n", __func__, count);
		return 0;
	}

	mtp = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_RE25);
	mtpex = TFA_GET_BF(tfa98xx->tfa, MTPEX);

	snprintf(out_buf, 64, "[%s] MTPEX: %d, MTP: %d mOhm\n",
		tfa_cont_device_name(tfa98xx->tfa->cnt, tfa98xx->tfa->dev_idx),
		mtpex, mtp);

	return simple_read_from_buffer(user_buf,
		count, ppos, out_buf, sizeof(out_buf));
}

/* Direct registers access - provide register address in hex */
#define TFA98XX_DEBUGFS_REG_SET(__reg)	\
static int tfa98xx_dbgfs_reg_##__reg##_set(void *data, u64 val)\
{\
	struct i2c_client *i2c = (struct i2c_client *)data;\
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);\
	unsigned int ret, value;\
\
	ret = regmap_write(tfa98xx->regmap, 0x##__reg, (val & 0xffff));\
	value = val & 0xffff;\
	return 0;\
} \
static int tfa98xx_dbgfs_reg_##__reg##_get(void *data, u64 *val)\
{\
	struct i2c_client *i2c = (struct i2c_client *)data;\
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);\
	unsigned int value;\
	int ret;\
\
	ret = regmap_read(tfa98xx->regmap, 0x##__reg, &value);\
	*val = value;\
	return 0;\
} \
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_reg_##__reg##_fops,\
	tfa98xx_dbgfs_reg_##__reg##_get,\
	tfa98xx_dbgfs_reg_##__reg##_set, "0x%llx\n")

#define VAL(str) #str
#define TOSTRING(str) VAL(str)
#define TFA98XX_DEBUGFS_REG_CREATE_FILE(__reg, __name, dbg_dir, i2c)	\
	debugfs_create_file(TOSTRING(__reg) "-" TOSTRING(__name),\
		0664, dbg_dir, i2c,\
		&tfa98xx_dbgfs_reg_##__reg##_fops)

TFA98XX_DEBUGFS_REG_SET(00);
TFA98XX_DEBUGFS_REG_SET(01);
TFA98XX_DEBUGFS_REG_SET(02);
TFA98XX_DEBUGFS_REG_SET(03);
TFA98XX_DEBUGFS_REG_SET(04);
TFA98XX_DEBUGFS_REG_SET(05);
TFA98XX_DEBUGFS_REG_SET(06);
TFA98XX_DEBUGFS_REG_SET(07);
TFA98XX_DEBUGFS_REG_SET(08);
TFA98XX_DEBUGFS_REG_SET(09);
TFA98XX_DEBUGFS_REG_SET(0A);
TFA98XX_DEBUGFS_REG_SET(0B);
TFA98XX_DEBUGFS_REG_SET(0F);
TFA98XX_DEBUGFS_REG_SET(10);
TFA98XX_DEBUGFS_REG_SET(11);
TFA98XX_DEBUGFS_REG_SET(12);
TFA98XX_DEBUGFS_REG_SET(13);
TFA98XX_DEBUGFS_REG_SET(22);
TFA98XX_DEBUGFS_REG_SET(25);

DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_otc_fops,
	tfa98xx_dbgfs_otc_get,
	tfa98xx_dbgfs_otc_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_mtpex_fops,
	tfa98xx_dbgfs_mtpex_get,
	tfa98xx_dbgfs_mtpex_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_temp_fops,
	tfa98xx_dbgfs_temp_get,
	tfa98xx_dbgfs_temp_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_pga_gain_fops,
	tfa98xx_dbgfs_pga_gain_get,
	tfa98xx_dbgfs_pga_gain_set, "%llu\n");

static const struct file_operations tfa98xx_dbgfs_calib_start_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_start_get,
	.write = tfa98xx_dbgfs_start_set,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_r_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_r_read,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_version_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_version_read,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_dsp_state_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_dsp_state_get,
	.write = tfa98xx_dbgfs_dsp_state_set,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_fw_state_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_fw_state_get,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_rpc_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_rpc_read,
	.write = tfa98xx_dbgfs_rpc_send,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_dsp_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_dsp_read,
	.write = tfa98xx_dbgfs_dsp_write,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_spkr_damaged_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_spkr_damaged_get,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_trace_level_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_trace_level_read,
	.write = tfa98xx_dbgfs_trace_level_write,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_show_cal_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_show_cal_read,
	.llseek = default_llseek,
};

static void tfa98xx_debug_init(struct tfa98xx *tfa98xx, struct i2c_client *i2c)
{
	char name[50];

	scnprintf(name, MAX_CONTROL_NAME, "%s-%x", i2c->name, i2c->addr);
	tfa98xx->dbg_dir = debugfs_create_dir(name, NULL);
	debugfs_create_file("OTC", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_otc_fops);
	debugfs_create_file("MTPEX", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_mtpex_fops);
	debugfs_create_file("TEMP", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_temp_fops);
	debugfs_create_file("calibrate", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_start_fops);
	debugfs_create_file("R", 0444, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_r_fops);
	debugfs_create_file("version", 0444, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_version_fops);
	debugfs_create_file("dsp-state", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_dsp_state_fops);
	debugfs_create_file("fw-state", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_fw_state_fops);
	debugfs_create_file("rpc", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_rpc_fops);
	debugfs_create_file("dsp", 0644, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_dsp_fops);

	if (tfa98xx->flags & TFA98XX_FLAG_SAAM_AVAILABLE) {
		dev_dbg(tfa98xx->dev, "Adding pga_gain debug interface\n");
		debugfs_create_file("pga_gain", 0444, tfa98xx->dbg_dir,
			tfa98xx->i2c,
			&tfa98xx_dbgfs_pga_gain_fops);
	}

	debugfs_create_file("trace-level", 0644,
		tfa98xx->dbg_dir,
		tfa98xx->i2c,
		&tfa98xx_dbgfs_trace_level_fops);

	debugfs_create_file("mtp", 0644,
		tfa98xx->dbg_dir,
		tfa98xx->i2c,
		&tfa98xx_dbgfs_show_cal_fops);
}

static void tfa98xx_debug_remove(struct tfa98xx *tfa98xx)
{
	debugfs_remove_recursive(tfa98xx->dbg_dir);
}
#endif /* CONFIG_DEBUG_FS */

#if defined(TFA_FS_CHECK_MTPEX)
static void tfa98xx_check_calibration(struct tfa98xx *tfa98xx)
{
	unsigned short value = 0;

	mutex_lock(&tfa98xx->dsp_lock);
	value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_EX);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value >= 0) {
		tfa98xx->calibrate_done =
			(value) ? 1 : 0;
		pr_info("[0x%x] calibrate_done = MTPEX (%d)\n",
			tfa98xx->i2c->addr, tfa98xx->calibrate_done);

	} else {
		pr_info("[0x%x] error in reading MTPEX\n",
			tfa98xx->i2c->addr);
		tfa98xx->calibrate_done = 0;
	}
}
#endif

#if defined(TFA_BYPASS_BEFORE_RECONFIG)
static enum tfa98xx_error tfa98xx_set_tfadsp_bypass(struct tfa_device *tfa)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int res_len = 3;
#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
	unsigned char buf[2 * 3] = {0};
#else
	unsigned char buf[3] = {0};
#endif /* TFA_CUSTOM_FORMAT_AT_RESPONSE */
	int data[2], is_configured = 0;

#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
	res_len = 2 * 3;
#else
	res_len = 3;
#endif /* TFA_CUSTOM_FORMAT_AT_RESPONSE */
	err = tfa_dsp_cmd_id_write_read(tfa, MODULE_CUSTOM,
		CUSTOM_PARAM_GET_CONFIGURED, res_len, buf);
	if (err == TFA98XX_ERROR_OK) {
		tfa98xx_convert_bytes2data(res_len, buf, data);
#if defined(TFA_CUSTOM_FORMAT_AT_RESPONSE)
		is_configured = data[1];
#else
		is_configured = data[0];
#endif /* TFA_CUSTOM_FORMAT_AT_RESPONSE */

		pr_info("%s: check if configured (%d)\n",
			__func__, is_configured);
	}

	/* move on if not configured */
	if (!is_configured)
		return err;

	pr_info("%s: set bypass if configured\n",
		__func__);

	memset(buf, 0, 3); /* dummy value */
	tfa->individual_msg = 1;
	err = tfa_dsp_cmd_id_write(tfa, MODULE_CUSTOM,
		CUSTOM_PARAM_SET_BYPASS, 3, buf);
	if (err != TFA98XX_ERROR_OK)
		pr_info("%s: error in setting bypass (err = %d)\n",
			__func__, err);

	return err;
}
#endif /* TFA_BYPASS_BEFORE_RECONFIG */

static int tfa98xx_run_calibration(struct tfa98xx *tfa98xx0)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa;
	enum tfa_error ret, cal_err = tfa_error_ok;
	int idx, ndev = tfa98xx_device_count;
	int cal_profile = 0;
	u64 otc_val = 1; /* calibration once by default */
	u16 temp_val = DEFAULT_REF_TEMP; /* default */
#if defined(TFA_USE_TFAVVAL_NODE)
	int mtpex[MAX_HANDLES] = {0};
	int re25;
#endif
#if defined(TFA_DISABLE_AUTO_CAL)
	int temp_calflag = 0;
#endif

	pr_info("%s: begin\n", __func__);

	if (tfa98xx0->pstream == 0) {
		pr_info("[0x%x] %s: calibration is available only when channel is enabled!\n",
			tfa98xx0->i2c->addr, __func__);
		return -EIO;
	}

#if defined(TFA_READ_REFERENCE_TEMP)
	/* EXT_TEMP */
	ret = tfa98xx_read_reference_temp(&temp_val);
	if (ret) {
		pr_err("%s: error in reading reference temp\n",
			__func__);
		temp_val = DEFAULT_REF_TEMP; /* default */
	}
#endif

#if defined(TFA_BYPASS_BEFORE_RECONFIG)
	if (tfa98xx0->tfa->is_bypass)
		pr_debug("%s: skipped setting bypass - tfadsp in bypass\n",
			__func__);
	else
		tfa98xx_set_tfadsp_bypass(tfa98xx0->tfa);
#endif /* TFA_BYPASS_BEFORE_RECONFIG */

	for (idx = 0; idx < ndev; idx++) {
		tfa = tfa98xx_get_tfa_device_from_index(idx);
		if (tfa == NULL)
			continue;

#if defined(TFA_USE_TFAVVAL_NODE)
		if (tfa->vval_active) {
			mtpex[idx] = tfa_dev_mtp_get(tfa, TFA_MTP_EX);
			pr_info("%s: dev %d - storing MTP (%d -> 0)\n",
				__func__, idx, mtpex[idx]);
		}
#endif

		pr_info("%s: dev %d - resetting MTP\n", __func__, idx);

		/* OTC <0:always 1:once> */
		ret = tfa_dev_mtp_set(tfa, TFA_MTP_OTC, otc_val);
		if (ret)
			pr_info("setting OTC failed (%d)\n", ret);
		/* MTPEX <reset to force to calibrate> */
		ret = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 0);
		if (ret) {
			pr_info("resetting MTPEX failed (%d)\n", ret);
			/* suspend until TFA98xx is active */
			tfa->reset_mtpex = 1;
		}
#if defined(TFA_USE_TFAVVAL_NODE)
		if (!tfa->vval_active) {
			/* MTPEX <reset to force to calibrate> */
			ret = tfa_dev_mtp_set(tfa, TFA_MTP_RE25, 0);
			/* EXT_TEMP */
			tfa98xx_set_exttemp(tfa, temp_val);
		}
#else
		/* MTPEX <reset to force to calibrate> */
		ret = tfa_dev_mtp_set(tfa, TFA_MTP_RE25, 0);
		/* EXT_TEMP */
		tfa98xx_set_exttemp(tfa, temp_val);
#endif /* TFA_USE_TFAVVAL_NODE */

#if defined(TFA_DISABLE_AUTO_CAL)
		pr_info("%s: dev %d - force to enable auto calibration (%s -> enabled)",
			__func__, idx,
			(tfa->disable_auto_cal) ? "disabled" : "enabled");
		/* enable auto calibration */
		temp_calflag |= tfa->disable_auto_cal;
		tfa->disable_auto_cal = 0;
#endif /* TFA_DISABLE_AUTO_CAL */

#if defined(TFA_MIXER_ON_DEVICE)
		/* force to enable all the devices */
		tfa->set_active = 1;
#endif

		/* force to mute amplifier to flush buffer */
		tfa_run_mute(tfa);
	}

	/* wait before restarting for calibration */
	msleep_interruptible(10);

#if defined(TFA_STOP_AND_RESTART_FOR_CALIBRATION)
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		pr_info("%s: dev %d - stopping devices\n",
			__func__, tfa98xx->tfa->dev_idx);

		_tfa98xx_stop(tfa98xx);

		mutex_lock(&tfa98xx->dsp_lock);

		tfa98xx->calibrate_done = 0;

		tfa98xx->dsp_init = TFA98XX_DSP_INIT_PENDING;
		tfa98xx_set_dsp_configured(tfa98xx);

		mutex_unlock(&tfa98xx->dsp_lock);
	}
#endif /* TFA_STOP_AND_RESTART_FOR_CALIBRATION */

	pr_info("%s: calibration started!\n", __func__);

	for (idx = 0; idx < ndev; idx++) {
		tfa = tfa98xx_get_tfa_device_from_index(idx);
		if (tfa == NULL)
			continue;

		tfa98xx = (struct tfa98xx *)tfa->data;
		pr_info("%s: dev %d - starting devices for calibration\n",
			__func__, idx);

		mutex_lock(&tfa98xx->dsp_lock);

		cal_profile = tfa_cont_get_cal_profile(tfa98xx->tfa);
		if (cal_profile < 0) {
			pr_warn("[0x%x] no cal profile is defined!\n",
				tfa98xx->i2c->addr);
			/* use current profile if there's no cal profile */
			cal_profile = tfa98xx->profile;
		}

		ret = tfa98xx_tfa_start(tfa98xx, cal_profile, tfa98xx->vstep);
		if (ret != tfa_error_ok) {
			pr_warn("[0x%x] failure in starting device for calibration! (err %d)\n",
				tfa98xx->i2c->addr, ret);
			cal_err |= ret;
		}

		tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
		tfa98xx_set_dsp_configured(tfa98xx);

		mutex_unlock(&tfa98xx->dsp_lock);
	}

#if defined(TFA_USE_TFAVVAL_NODE)
	for (idx = 0; idx < ndev; idx++) {
		tfa = tfa98xx_get_tfa_device_from_index(idx);
		if (tfa == NULL)
			continue;

		if (tfa->vval_active) {
			pr_info("%s: dev %d - restoring MTP (0 -> %d)\n",
				__func__, idx, mtpex[idx]);

			ret = tfa_dev_mtp_set(tfa, TFA_MTP_EX, mtpex[idx]);
			if (ret)
				pr_info("restoring MTPEX failed (%d)\n", ret);

			mtpex[idx] = tfa_dev_mtp_get(tfa, TFA_MTP_EX);
			re25 = tfa_dev_mtp_get(tfa, TFA_MTP_RE25);
			pr_info("%s: dev %d - readback MTP (%d, %d)\n",
				__func__, idx, mtpex[idx], re25);
		}
	}
#endif

#if defined(TFA_DISABLE_AUTO_CAL)
	pr_info("%s: restore flag for auto calibration (enabled -> %s)",
		__func__,
		(temp_calflag) ? "disabled" : "enabled");
	for (idx = 0; idx < ndev; idx++) {
		tfa = tfa98xx_get_tfa_device_from_index(idx);
		if (tfa == NULL)
			continue;

		/* restore flag for auto calibration */
		tfa->disable_auto_cal = temp_calflag;
	}
#endif /* TFA_DISABLE_AUTO_CAL */

	if (cal_err) {
		pr_info("%s: calibration failed! (err %d)\n",
			__func__, cal_err);
#if defined(TFA_USE_TFAVVAL_NODE)
		/* do not return error in case of V validation */
		if (tfa98xx0->tfa->vval_active)
			return 0;
#else
		return -EIO;
#endif
	}

#if !defined(TFA_WAIT_CAL_IN_WORKQUEUE)
	pr_info("%s: calibration done!\n", __func__);
#else
	pr_info("%s: calibration started!\n", __func__);
#endif
	pr_info("%s: end\n", __func__);

	return 0;
}

#if defined(TFA_READ_REFERENCE_TEMP)
static enum tfa98xx_error tfa98xx_read_reference_temp(short *value)
{
#if defined(TFA_USE_POWER_SUPPLY)
	struct power_supply *psy = NULL;
	union power_supply_propval prop_read = {0};
	int ret = 0;

	/* get power supply of "battery" */
	/* value is preserved with default when error happens */
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		pr_err("%s: failed to get power supply\n", __func__);
		return TFA98XX_ERROR_FAIL;
	}

#if KERNEL_VERSION(4, 1, 0) > LINUX_VERSION_CODE
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &prop_read);
#else
	ret = power_supply_get_property(psy,
		POWER_SUPPLY_PROP_TEMP, &prop_read);
#endif
	if (ret) {
		pr_err("%s: failed to get temp property\n", __func__);
#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
		if (psy)
			power_supply_put(psy);
#endif
		return TFA98XX_ERROR_FAIL;
	}

	*value = (short)(prop_read.intval / 10); /* in degC */
	pr_info("%s: read battery temp (%d)\n", __func__, *value);
#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
	if (psy)
		power_supply_put(psy);
#endif
#elif defined(TFA_USE_THERMAL_SENSOR)
	struct thermal_zone_device *tz_quiet = NULL;
#if KERNEL_VERSION(4, 3, 0) > LINUX_VERSION_CODE
	unsigned long temp = 0;
#else
	int temp = 0;
#endif
	int ret = 0;

	tz_quiet = thermal_zone_get_zone_by_name("quiet-therm-usr");
	if (!tz_quiet) {
		pr_err("%s: failed to get quiet thern devicey\n", __func__);
		return TFA98XX_ERROR_FAIL;
	}

#if KERNEL_VERSION(3, 10, 0) > LINUX_VERSION_CODE
	ret = tz_quiet->ops->get_temp(tz_quiet, &temp);
#else
	ret = thermal_zone_get_temp(tz_quiet, &temp);
#endif
	if (ret) {
		pr_err("%s: failed to get temp property\n", __func__);
		return TFA98XX_ERROR_FAIL;
	}

	pr_info("%s: read quiet temp = (%d)\n", __func__, (int)temp);
	*value = (short)(temp / 1000); /* in degC */
	pr_info("%s: read quiet value = (%d)\n", __func__, *value);
#else
	/* neither TFA_USE_POWER_SUPPLY or TFA_USE_THERMAL_SENSOR */
	pr_info("%s: use default value = (%d)\n", __func__,
		DEFAULT_REF_TEMP);
	*value = DEFAULT_REF_TEMP; /* Re25C, if not specified */
#endif

	return TFA98XX_ERROR_OK;
}
#endif /* TFA_READ_REFERENCE_TEMP */

static void tfa98xx_set_dsp_configured(struct tfa98xx *tfa98xx)
{
	int is_configured; /* reset by default */

	is_configured = tfa98xx->tfa->is_configured;

	switch (tfa98xx->dsp_init) {
	case TFA98XX_DSP_INIT_DONE:
	case TFA98XX_DSP_INIT_RECOVER:
		/* set working if already running */
		is_configured = 1;
		break;
	case TFA98XX_DSP_INIT_INVALIDATED:
		/* preserve state except pstream off */
		if (tfa98xx->pstream == 0)
			is_configured = 0;
		break;
	case TFA98XX_DSP_INIT_STOPPED:
		if (tfa98xx->tfa->is_probus_device) {
			/* preserve state except pstream off */
			if (tfa98xx->pstream == 0)
				is_configured = 0;
		} else {
			/* reset for embedded DSP case */
			is_configured = 0;
		}
		break;
	case TFA98XX_DSP_INIT_FAIL:
		is_configured = -1;
		break;
	case TFA98XX_DSP_INIT_PENDING:
	default:
		is_configured = 0;
		break;
	}

	pr_debug("[0x%x] dsp_init %d, is_configured %d\n",
		tfa98xx->i2c->addr,
		tfa98xx->dsp_init, is_configured);

	tfa98xx->tfa->is_configured = is_configured;
}

#if KERNEL_VERSION(3, 16, 0) > LINUX_VERSION_CODE
static struct snd_soc_codec *
snd_soc_kcontrol_codec(struct snd_kcontrol *kcontrol)
{
	return snd_kcontrol_chip(kcontrol);
}
#endif

static int tfa98xx_get_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	struct snd_soc_component *component
		= snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	int mixer_profile = kcontrol->private_value;
	int ret = 0;
	int profile;

	profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);
	if (profile < 0) {
		pr_err("%s: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int vstep = tfa98xx->prof_vsteps[profile];

		ucontrol->value.integer.value[tfa98xx->tfa->dev_idx]
			= tfa_cont_get_max_vstep(tfa98xx->tfa, profile)
			- vstep - 1;
	}
	mutex_unlock(&tfa98xx_mutex);

	return ret;
}

static int tfa98xx_set_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	struct snd_soc_component *component
		= snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	int mixer_profile = kcontrol->private_value;
	int profile;
	int err = 0;
	int change = 0;
#if defined(TFA_MUTE_DURING_SWITCHING_PROFILE)
#if defined(TFA_RAMPDOWN_BEFORE_MUTE)
	int i = 0;
#endif
#endif

	if (no_start != 0)
		return 0;

	profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);
	if (profile < 0) {
		pr_err("%s: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}

	/* wait until when DSP is ready for initialization */
	if (tfa98xx->pstream == 0 && tfa98xx->samstream == 0) {
		pr_info("%s: tfa_start is suspended unless p/samstream is on\n",
			__func__);
		return 0;
	}

	mutex_lock(&tfa98xx_mutex);

#if defined(TFA_MUTE_DURING_SWITCHING_PROFILE)
	change = 1;
#if defined(TFA_RAMPDOWN_BEFORE_MUTE)
	for (i = 0; i < RAMPDOWN_MAX; i++) {
		err = 0;
		list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
			mutex_lock(&tfa98xx->dsp_lock);
			err += tfa_gain_rampdown(tfa98xx->tfa, i, RAMPDOWN_MAX);
			mutex_unlock(&tfa98xx->dsp_lock);
		}
		if (err == tfa98xx_device_count * TFA98XX_ERROR_OTHER)
			break;
		msleep_interruptible(1);
	}
#endif /* TFA_RAMPDOWN_BEFORE_MUTE */
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		mutex_lock(&tfa98xx->dsp_lock);
		show_current_state(tfa98xx->tfa);
		tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_MUTE, 0);
		mutex_unlock(&tfa98xx->dsp_lock);
	}
#endif /* TFA_MUTE_DURING_SWITCHING_PROFILE */

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int vstep, vsteps;
		int ready = 0;
		int new_vstep;
		int value = ucontrol->value
			.integer.value[tfa98xx->tfa->dev_idx];

		vstep = tfa98xx->prof_vsteps[profile];
		vsteps = tfa_cont_get_max_vstep(tfa98xx->tfa, profile);

		if (vstep == vsteps - value - 1)
			continue;

		new_vstep = vsteps - value - 1;

		if (new_vstep < 0)
			new_vstep = 0;

		tfa98xx->prof_vsteps[profile] = new_vstep;

#ifndef TFA98XX_ALSA_CTRL_PROF_CHG_ON_VOL
		if (profile == tfa98xx->profile) {
#endif
			/* this is the active profile, program the new vstep */
			tfa98xx->vstep = new_vstep;
			mutex_lock(&tfa98xx->dsp_lock);
			tfa98xx_dsp_system_stable(tfa98xx->tfa, &ready);

			if (ready) {
				err = tfa98xx_tfa_start(tfa98xx,
					tfa98xx->profile, tfa98xx->vstep);
				if (err) {
					pr_err("Write vstep error: %d\n", err);
				} else {
					pr_debug("Successfully changed vstep index!\n");
					change = 1;
				}
			}

			/* Set DSP status as profile change may invalidate
			 * current DSP configuration. Next stream start can
			 * trigger a tfa_dev_start.
			 */
			tfa98xx->dsp_init = TFA98XX_DSP_INIT_INVALIDATED;
			tfa98xx_set_dsp_configured(tfa98xx);
			mutex_unlock(&tfa98xx->dsp_lock);
#ifndef TFA98XX_ALSA_CTRL_PROF_CHG_ON_VOL
		}
#endif
		pr_debug("%d: vstep:%d, (control value: %d) - profile %d\n",
			tfa98xx->tfa->dev_idx, new_vstep, value, profile);
	}

	if (!change) {
		mutex_unlock(&tfa98xx_mutex);
		return change;
	}

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		mutex_lock(&tfa98xx->dsp_lock);
#if defined(TFA_TDMSPKG_CONTROL)
		if (tfa98xx->tfa->spkgain != -1) {
			pr_info("%s: set speaker gain 0x%x\n",
				__func__, tfa98xx->tfa->spkgain);
			TFA7x_SET_BF(tfa98xx->tfa, TDMSPKG,
				tfa98xx->tfa->spkgain);
		}
#endif

		pr_info("%s: UNMUTE dev %d\n",
			__func__, tfa98xx->tfa->dev_idx);
		tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_UNMUTE, 0);
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	mutex_unlock(&tfa98xx_mutex);

	return change;
}

static int tfa98xx_info_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	struct snd_soc_component *component
		= snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	int mixer_profile = tfa98xx_mixer_profile;
	int profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);

	if (profile < 0) {
		pr_err("%s: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max
		= max(0, tfa_cont_get_max_vstep(tfa98xx->tfa, profile) - 1);
	pr_debug("vsteps count: %d [prof=%d]\n",
		tfa_cont_get_max_vstep(tfa98xx->tfa, profile),
		profile);
	return 0;
}

static int tfa98xx_get_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&tfa98xx_mutex);
	ucontrol->value.integer.value[0] = tfa98xx_mixer_profile;
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	struct snd_soc_component *component
		= snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	int err = 0;
	int change = 0;
	int new_profile;
	int prof_idx;
	int profile_count = tfa98xx_mixer_profiles;
	int profile = tfa98xx_mixer_profile;
#if defined(TFA_MUTE_DURING_SWITCHING_PROFILE)
#if defined(TFA_RAMPDOWN_BEFORE_MUTE)
	int i = 0;
#endif
#endif

	if (no_start != 0)
		return 0;

	new_profile = ucontrol->value.integer.value[0];
	if (new_profile == profile)
		return 0;

	if ((new_profile < 0) || (new_profile >= profile_count)) {
		pr_err("not existing profile (%d)\n", new_profile);
		return -EINVAL;
	}

	/* get the container profile for the requested sample rate */
	prof_idx = get_profile_id_for_sr(new_profile, tfa98xx->rate);
	if (prof_idx < 0) {
		pr_err("tfa98xx: sample rate [%d] not supported for this mixer profile [%d].\n",
			tfa98xx->rate, new_profile);
		return 0;
	}
	pr_info("%s: selected container profile [%d]\n",
		__func__, prof_idx);

	/* update mixer profile */
	tfa98xx_mixer_profile = new_profile;

	/* wait until when DSP is ready for initialization */
	if (tfa98xx->pstream == 0 && tfa98xx->samstream == 0) {
		pr_info("%s: tfa_start is suspended unless p/samstream is on\n",
			__func__);
		return 0;
	}

	mutex_lock(&tfa98xx_mutex);

#if defined(TFA_MUTE_DURING_SWITCHING_PROFILE)
	change = 1;
#if defined(TFA_RAMPDOWN_BEFORE_MUTE)
	for (i = 0; i < RAMPDOWN_MAX; i++) {
		err = 0;
		list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
			mutex_lock(&tfa98xx->dsp_lock);
			err += tfa_gain_rampdown(tfa98xx->tfa, i, RAMPDOWN_MAX);
			mutex_unlock(&tfa98xx->dsp_lock);
		}
		if (err == tfa98xx_device_count * TFA98XX_ERROR_OTHER)
			break;
		msleep_interruptible(1);
	}
#endif /* TFA_RAMPDOWN_BEFORE_MUTE */
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		mutex_lock(&tfa98xx->dsp_lock);
		show_current_state(tfa98xx->tfa);
		tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_MUTE, 0);
		mutex_unlock(&tfa98xx->dsp_lock);
	}
#endif /* TFA_MUTE_DURING_SWITCHING_PROFILE */

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int ready = 0;

		/* update 'real' profile (container profile) */
		tfa98xx->profile = prof_idx;
		tfa98xx->vstep = tfa98xx->prof_vsteps[prof_idx];

		mutex_lock(&tfa98xx->dsp_lock);
#if defined(TFA_TRIGGER_AT_SET_PROFILE_WITH_CLK)
		/* Don't call tfa_dev_start() if there is no clock */
		tfa98xx_dsp_system_stable(tfa98xx->tfa, &ready);
		pr_debug("%s: device is %sready\n", __func__,
			ready ? "" : "not ");
#else
		/* Set ready by force, for selective channel control */
		ready = 1;
#endif
		if (ready) {
			/* Also re-enables the interrupts */
			pr_info("%s: trigger [dev %d - prof %d]\n", __func__,
				tfa98xx->tfa->dev_idx, prof_idx);
			err = tfa98xx_tfa_start(tfa98xx,
				prof_idx, tfa98xx->vstep);
			if (err) {
				pr_info("Write profile error: %d\n", err);
			} else {
				pr_debug("Changed to profile %d (vstep = %d)\n",
					prof_idx, tfa98xx->vstep);
				change = 1;
			}
		}

		/* Set DSP status as profile change may invalidate
		 * current DSP configuration. Next stream start can
		 * trigger a tfa_dev_start.
		 */
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_INVALIDATED;
		tfa98xx_set_dsp_configured(tfa98xx);
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	if (!change) {
		mutex_unlock(&tfa98xx_mutex);
		return change;
	}

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		mutex_lock(&tfa98xx->dsp_lock);
#if defined(TFA_TDMSPKG_CONTROL)
		if (tfa98xx->tfa->spkgain != -1) {
			pr_info("%s: set speaker gain 0x%x\n",
				__func__, tfa98xx->tfa->spkgain);
			TFA7x_SET_BF(tfa98xx->tfa, TDMSPKG,
				tfa98xx->tfa->spkgain);
		}
#endif

		pr_info("%s: UNMUTE dev %d\n",
			__func__, tfa98xx->tfa->dev_idx);
		tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_UNMUTE, 0);
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	mutex_unlock(&tfa98xx_mutex);

	return change;
}

/* copies the profile basename (i.e. part until .) into buf */
static void get_profile_basename(char *buf, char *profile)
{
	int cp_len = 0, idx = 0;
	char *pch;

	pch = strnchr(profile, strlen(profile), '.');
	idx = pch - profile;
	cp_len = (pch != NULL) ? idx : (int)strlen(profile);
	memcpy(buf, profile, cp_len);
	buf[cp_len] = 0;
}

/* return the profile name accociated with id from the profile list */
static int get_profile_from_list(char *buf, int id)
{
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (bprof->item_id == id) {
			strlcpy(buf, bprof->basename, MAX_CONTROL_NAME);
			return 0;
		}
	}

	return TFA_ERROR;
}

/* search for the profile in the profile list */
static int is_profile_in_list(char *profile, int len)
{
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if ((len == bprof->len)
			&& (strncmp(bprof->basename, profile, len) == 0))
			return 1;
	}

	return 0;
}

/*
 * for the profile with id, look if the requested samplerate is
 * supported, if found return the (container)profile for this
 * samplerate, on error or if not found return -1
 */
static int get_profile_id_for_sr(int id, unsigned int rate)
{
	int idx = 0;
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (id == bprof->item_id) {
			idx = tfa98xx_get_fssel(rate);
			if (idx < 0) {
				/* samplerate not supported */
				return TFA_ERROR;
			}

			return bprof->sr_rate_sup[idx];
		}
	}

	/* profile not found */
	return TFA_ERROR;
}

/* check if this profile is a calibration profile */
static int is_calibration_profile(char *profile)
{
	if (strnstr(profile, ".cal", strlen(profile)) != NULL)
		return 1;
	return 0;
}

/*
 * adds the (container)profile index of the samplerate found in
 * the (container)profile to a fixed samplerate table in the (mixer)profile
 */
static int add_sr_to_profile(struct tfa98xx *tfa98xx,
	char *basename, int len, int profile)
{
	struct tfa98xx_baseprofile *bprof;
	int idx = 0;
	unsigned int sr = 0;
#if defined(TFA_FIXED_RATE_FOR_DSP)
	unsigned int sr0 = 0xff;
	static int sr_hit;
#endif

	list_for_each_entry(bprof, &profile_list, list) {
		if ((len == bprof->len)
			&& (strncmp(bprof->basename, basename, len) == 0)) {
			/* add supported samplerate for this profile */
			sr = tfa98xx_get_profile_sr(tfa98xx->tfa, profile);
			if (!sr) {
				pr_err("unable to identify supported sample rate for %s\n",
					bprof->basename);
				return TFA_ERROR;
			}
#if defined(TFA_FIXED_RATE_FOR_DSP)
			sr0 = (sr0 == 0xff) ? sr : sr0; /* default rate */
			if (sr_converted == sr) {
				pr_debug("sr_converted: hits (%d)!\n",
					sr_converted);
				sr_hit = 1;
			}
#endif

			/* get the index for this samplerate */
			idx = tfa98xx_get_fssel(sr);
			if (idx < 0 || idx >= TFA98XX_NUM_RATES) {
				pr_err("invalid index for samplerate %d\n",
					idx);
				return TFA_ERROR;
			}

			/* enter the (container)profile for this samplerate
			 * at the corresponding index
			 */
			bprof->sr_rate_sup[idx] = profile;

			pr_debug("added profile:samplerate = [%d:%d] for mixer profile: %s\n",
				profile, sr, bprof->basename);
		}
	}

#if defined(TFA_FIXED_RATE_FOR_DSP)
	if (!sr_hit && sr0 != 0xff) {
		pr_info("sr_converted: use %d, as %d does not exist\n",
			sr0, sr_converted);
		sr_converted = sr0;
	}
#endif

	return 0;
}

static int tfa98xx_info_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	char profile_name[MAX_CONTROL_NAME] = {0};
	int count = tfa98xx_mixer_profiles, err = -1;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.enumerated.items = count;

	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	err = get_profile_from_list(profile_name,
		uinfo->value.enumerated.item);
	if (err != 0)
		return -EINVAL;

	strlcpy(uinfo->value.enumerated.name,
		profile_name, MAX_CONTROL_NAME);

	return 0;
}

#if defined(TFA_MIXER_ON_DEVICE)
static int tfa98xx_info_device_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int tfa98xx_get_device_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		ucontrol->value.integer.value[tfa->dev_idx] = tfa->set_active;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_device_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;
	enum tfa_error err;
	int dev;
	int request;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		dev = tfa->dev_idx;
		request = ucontrol->value.integer.value[dev];

		pr_info("%s: [%d] set active %d\n",
			__func__, dev, request);
		tfa->set_active = request; /* store for the next sessions */

		/* exit if stream is not ready for immediate action */
		if (tfa98xx->pstream == 0
			&& tfa98xx->samstream == 0) {
			pr_info("%s: [%d] store set active unless p/samstream is on\n",
				__func__, dev);
			continue;
		}

		switch (request) {
		case 0: /* deactivate immediately */
#if defined(TFA_PAUSE_CONTROL)
			if (tfa->pause_state == 1) {
				pr_info("%s: [%d] already paused; no need to deactivate\n",
					__func__, dev);
				break;
			}
#endif

			pr_info("%s: [%d] deactivate channel\n",
				__func__, dev);
			cancel_delayed_work_sync(&tfa98xx->monitor_work);
#if !defined(TFA_USE_DIRECT_API_CALL)
			cancel_delayed_work_sync(&tfa98xx->init_work);
#endif
			_tfa98xx_stop(tfa98xx);
			break;
		case 1: /* activate immediately */
#if defined(TFA_PAUSE_CONTROL)
			if (tfa->pause_state == 0) {
				pr_info("%s: [%d] already resumed; no need to activate\n",
					__func__, dev);
				break;
			}
#endif

			pr_info("%s: [%d] activate channel\n",
				__func__, dev);
			mutex_lock(&tfa98xx->dsp_lock);
			pr_info("%s: trigger [dev %d - prof %d]\n", __func__,
				dev, tfa98xx->profile);
			err = tfa98xx_tfa_start(tfa98xx,
				tfa98xx->profile, tfa98xx->vstep);
			if (err) {
				pr_info("error in activation: %d\n", err);
				mutex_unlock(&tfa98xx->dsp_lock);
				break;
			}

			tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
			tfa98xx_set_dsp_configured(tfa98xx);

#if defined(TFA_TDMSPKG_CONTROL)
			if (tfa->spkgain != -1) {
				pr_info("%s: set speaker gain 0x%x\n",
					__func__, tfa->spkgain);
				TFA7x_SET_BF(tfa, TDMSPKG,
					tfa->spkgain);
			}
#endif

			pr_info("%s: UNMUTE dev %d\n",
				__func__, dev);
			tfa_dev_set_state(tfa, TFA_STATE_UNMUTE, 0);
			mutex_unlock(&tfa98xx->dsp_lock);
			break;
		default:
			pr_info("%s: [%d] wrong request\n",
					__func__, dev);
			break;
		}
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}
#endif /* TFA_MIXER_ON_DEVICE */

static int tfa98xx_info_stop_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int tfa98xx_get_stop_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		ucontrol->value.integer.value[tfa98xx->tfa->dev_idx] = 0;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_stop_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int ready = 0;
		int i = tfa98xx->tfa->dev_idx;

		pr_debug("%d: %ld\n", i, ucontrol->value.integer.value[i]);

		tfa98xx_dsp_system_stable(tfa98xx->tfa, &ready);

		if ((ucontrol->value.integer.value[i] != 0) && ready) {
			cancel_delayed_work_sync(&tfa98xx->monitor_work);
#if !defined(TFA_USE_DIRECT_API_CALL)
			cancel_delayed_work_sync(&tfa98xx->init_work);
#endif
			_tfa98xx_stop(tfa98xx);
		}

		ucontrol->value.integer.value[i] = 0;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

#if defined(TFA_MUTE_CONTROL)
static int tfa98xx_info_mute_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int tfa98xx_get_mute_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		ucontrol->value.integer.value[tfa->dev_idx] = tfa->mute_state;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_mute_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;
	int dev;
	int request;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		dev = tfa->dev_idx;
		request = ucontrol->value.integer.value[dev];

		/* exit if stream is not ready for initialization */
		if (tfa98xx->pstream == 0
			&& tfa98xx->samstream == 0) {
			pr_info("%s: [%d] only store request (%s), unless p/samstream is on\n",
				__func__, dev,
				(request == 1) ? "mute" : "unmute");
			tfa->mute_state = request;
			continue;
		}

		switch (request) {
		case 0: /* unmute */
			if (tfa->mute_state == 0) {
				pr_info("%s: [%d] already unmuted, skip the request\n",
					__func__, dev);
				break;
			}

			pr_info("%s: [%d] unmute channel\n",
				__func__, dev);
			mutex_lock(&tfa98xx->dsp_lock);
			tfa->mute_state = 0;
			tfa_run_unmute(tfa);
			mutex_unlock(&tfa98xx->dsp_lock);
			break;
		case 1: /* mute */
			if (tfa->mute_state == 1) {
				pr_info("%s: [%d] already muted, skip the request\n",
					__func__, dev);
				break;
			}

			pr_info("%s: [%d] mute channel\n",
				__func__, dev);
			mutex_lock(&tfa98xx->dsp_lock);
			tfa->mute_state = 1;
			tfa_run_mute(tfa);
			mutex_unlock(&tfa98xx->dsp_lock);
			break;
		default:
			pr_info("%s: [%d] wrong request\n",
					__func__, dev);
			break;
		}
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}
#endif /* TFA_MUTE_CONTROL */

#if defined(TFA_PAUSE_CONTROL)
static int tfa98xx_info_pause_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int tfa98xx_get_pause_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		ucontrol->value.integer.value[tfa->dev_idx] = tfa->pause_state;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_pause_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;
	enum tfa_error err;
	int dev;
	int request;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		dev = tfa->dev_idx;
		request = ucontrol->value.integer.value[dev];

		switch (request) {
		case 0: /* resume */
			if (tfa->pause_state == 0) {
				pr_info("%s: [%d] already resumed, skip the request\n",
					__func__, dev);
				break;
			}
			/* exit if stream is not ready for initialization */
			if (tfa98xx->pstream == 0
				&& tfa98xx->samstream == 0) {
				pr_info("%s: [%d] cannot resume unless p/samstream is on\n",
					__func__, dev);
				break;
			}

			pr_info("%s: [%d] resume channel\n",
				__func__, dev);
			mutex_lock(&tfa98xx->dsp_lock);
			pr_info("%s: trigger [dev %d - prof %d]\n", __func__,
				dev, tfa98xx->profile);
			err = tfa98xx_tfa_start(tfa98xx,
				tfa98xx->profile, tfa98xx->vstep);
			if (err) {
				pr_info("error in activation: %d\n", err);
				mutex_unlock(&tfa98xx->dsp_lock);
				break;
			}

			tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
			tfa98xx_set_dsp_configured(tfa98xx);

#if defined(TFA_TDMSPKG_CONTROL)
			if (tfa->spkgain != -1) {
				pr_info("%s: set speaker gain 0x%x\n",
					__func__, tfa->spkgain);
				TFA7x_SET_BF(tfa, TDMSPKG,
					tfa->spkgain);
			}
#endif

			pr_info("%s: UNMUTE dev %d\n",
				__func__, dev);
			tfa_dev_set_state(tfa, TFA_STATE_UNMUTE, 0);
			mutex_unlock(&tfa98xx->dsp_lock);

			tfa->pause_state = 0;
			break;
		case 1: /* pause */
			if (tfa->pause_state == 1) {
				pr_info("%s: [%d] already paused, skip the request\n",
					__func__, dev);
				break;
			}

			pr_info("%s: [%d] pause channel\n",
				__func__, dev);
			cancel_delayed_work_sync(&tfa98xx->monitor_work);
#if !defined(TFA_USE_DIRECT_API_CALL)
			cancel_delayed_work_sync(&tfa98xx->init_work);
#endif
			_tfa98xx_stop(tfa98xx);

			tfa->pause_state = 1;
			break;
		default:
			pr_info("%s: [%d] wrong request\n",
					__func__, dev);
			break;
		}
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}
#endif /* TFA_PAUSE_CONTROL */

#if defined(TFA_TDMSPKG_CONTROL)
static int tfa98xx_info_spkgain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0x0;
	uinfo->value.integer.max = 0xf;

	return 0;
}

static int tfa98xx_get_spkgain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;
	int spkgain;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		spkgain = tfa->spkgain;
		if (spkgain == -1) {
			spkgain = TFA7x_GET_BF(tfa, TDMSPKG);
			pr_info("%s: [%d] read current speaker gain 0x%x\n",
				__func__, tfa->dev_idx, spkgain);
		}

		ucontrol->value.integer.value[tfa->dev_idx] = spkgain;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_spkgain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;
	enum tfa98xx_error err;
	int dev;
	int cur_spkgain;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		dev = tfa->dev_idx;
		tfa->spkgain = ucontrol->value.integer.value[dev];

		cur_spkgain = TFA7x_GET_BF(tfa, TDMSPKG);
		pr_info("%s: [%d] set spekaer gain 0x%x (currently, 0x%x)\n",
			__func__, dev, tfa->spkgain, cur_spkgain);
		err = TFA7x_SET_BF(tfa, TDMSPKG, tfa->spkgain);
		if (err)
			pr_err("%s: [%d] failed to set speaker gain\n",
				__func__, dev);
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}
#endif /* TFA_TDMSPKG_CONTROL */

static int tfa98xx_info_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffff; /* 16 bit value */

	return 0;
}

static int tfa98xx_set_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		enum tfa_error err;
		int i = tfa98xx->tfa->dev_idx;

		tfa98xx->cal_data = (uint16_t)ucontrol->value.integer.value[i];

		mutex_lock(&tfa98xx->dsp_lock);
		err = tfa98xx_write_re25(tfa98xx->tfa, tfa98xx->cal_data);
		tfa98xx->set_mtp_cal = (err != tfa_error_ok);
		if (tfa98xx->set_mtp_cal == false)
			pr_info("Calibration value (%d) set in mtp\n",
				tfa98xx->cal_data);
		mutex_unlock(&tfa98xx->dsp_lock);
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

static int tfa98xx_get_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		mutex_lock(&tfa98xx->dsp_lock);
		ucontrol->value.integer.value[tfa98xx->tfa->dev_idx]
			= tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_RE25_PRIM);
		mutex_unlock(&tfa98xx->dsp_lock);
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_create_controls(struct tfa98xx *tfa98xx)
{
	int prof, nprof, mix_index = 0;
	int nr_controls = 0, id = 0;
	char *name;
	struct tfa98xx_baseprofile *bprofile;
	struct device *cdev;
	int ret;
	static int is_control_created;

	if (is_control_created) {
		pr_info("%s: Already created\n", __func__);
		return 0;
	}

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	cdev = tfa98xx->component->dev;
#else
	cdev = tfa98xx->codec->dev;
#endif

	/* Create the following controls:
	 * - enum control to select the active profile
	 * - one volume control for each profile hosting a vstep
	 * - Stop control on TFA1 devices
	 */

	nr_controls = 2; /* Profile and stop control */

#if defined(TFA_MIXER_ON_DEVICE)
	nr_controls += 1; /* set active */
#endif
#if defined(TFA_MUTE_CONTROL)
	nr_controls += 1; /* set mute */
#endif
#if defined(TFA_PAUSE_CONTROL)
	nr_controls += 1; /* set pause */
#endif
#if defined(TFA_TDMSPKG_CONTROL)
	nr_controls += 1; /* set speaker gain */
#endif

	if (tfa98xx->flags & TFA98XX_FLAG_CALIBRATION_CTL)
		nr_controls += 1; /* calibration */

	/* allocate the tfa98xx_controls base on the nr of profiles */
	nprof = tfa_cnt_get_dev_nprof(tfa98xx->tfa);
	for (prof = 0; prof < nprof; prof++) {
		if (tfa_cont_get_max_vstep(tfa98xx->tfa, prof))
			nr_controls++; /* Playback Volume control */
	}

	tfa98xx_controls = devm_kzalloc(cdev,
		nr_controls * sizeof(tfa98xx_controls[0]), GFP_KERNEL);
	if (!tfa98xx_controls)
		return -ENOMEM;

	/* Create a mixer item for selecting the active profile */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Profile", tfa98xx->fw.name);
	pr_info("%s: Mixer Control Name = %s\n", __func__, name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_profile;
	tfa98xx_controls[mix_index].get = tfa98xx_get_profile;
	tfa98xx_controls[mix_index].put = tfa98xx_set_profile;
	/* tfa98xx_controls[mix_index].private_value = profs; */
	/* save number of profiles */
	mix_index++;

	/* create mixer items for each profile that has volume */
	for (prof = 0; prof < nprof; prof++) {
		/* create an new empty profile */
		bprofile = devm_kzalloc(cdev,
			sizeof(*bprofile), GFP_KERNEL);
		if (!bprofile)
			return -ENOMEM;

		bprofile->len = 0;
		bprofile->item_id = -1;
		INIT_LIST_HEAD(&bprofile->list);

		/* copy profile name into basename until the . */
		get_profile_basename(bprofile->basename,
			_tfa_cont_profile_name(tfa98xx, prof));
		bprofile->len = strlen(bprofile->basename);

		/*
		 * search the profile list for a profile with basename,
		 * if it is not found then
		 * add it to the list and add a new mixer control (with vsteps)
		 * also, if it is a calibration profile,
		 * do not add it to the list
		 */
		if ((is_profile_in_list(bprofile->basename, bprofile->len) == 0)
			&& is_calibration_profile
			(_tfa_cont_profile_name(tfa98xx, prof)) == 0) {
			/* the profile is not present, add it to the list */
			list_add(&bprofile->list, &profile_list);
			bprofile->item_id = id++;

			pr_debug("profile added [%d]: %s\n",
				bprofile->item_id, bprofile->basename);

			if (tfa_cont_get_max_vstep(tfa98xx->tfa, prof)) {
				name = devm_kzalloc(cdev,
					MAX_CONTROL_NAME, GFP_KERNEL);
				if (!name)
					return -ENOMEM;

				scnprintf(name, MAX_CONTROL_NAME,
					"%s %s Playback Volume",
					tfa98xx->fw.name, bprofile->basename);
				tfa98xx_controls[mix_index].name = name;
				tfa98xx_controls[mix_index].iface
					= SNDRV_CTL_ELEM_IFACE_MIXER;
				tfa98xx_controls[mix_index].info
					= tfa98xx_info_vstep;
				tfa98xx_controls[mix_index].get
					= tfa98xx_get_vstep;
				tfa98xx_controls[mix_index].put
					= tfa98xx_set_vstep;
				tfa98xx_controls[mix_index].private_value
					= bprofile->item_id;
				/* save profile index */
				mix_index++;
			}
		}

		/* look for the basename profile in the list of mixer profiles
		 * and add the container profile index
		 * to the supported samplerates of this mixer profile
		 */
		add_sr_to_profile(tfa98xx, bprofile->basename,
			bprofile->len, prof);
	}

	/* set the number of user selectable profiles in the mixer */
	if (id > 0) /* if any profile to be registered */
		tfa98xx_mixer_profiles = id;
	else if (tfa98xx_mixer_profiles == 0)
		tfa98xx_mixer_profiles = nprof;

#if defined(TFA_MIXER_ON_DEVICE)
	/* set active device for the following sessions */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Active", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_device_ctl;
	tfa98xx_controls[mix_index].get = tfa98xx_get_device_ctl;
	tfa98xx_controls[mix_index].put = tfa98xx_set_device_ctl;
	mix_index++;
#endif

	/* Create a mixer item for stop control on TFA1 */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Stop", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_stop_ctl;
	tfa98xx_controls[mix_index].get = tfa98xx_get_stop_ctl;
	tfa98xx_controls[mix_index].put = tfa98xx_set_stop_ctl;
	mix_index++;

#if defined(TFA_MUTE_CONTROL)
	/* set mute / unmute by force */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Mute", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_mute_ctl;
	tfa98xx_controls[mix_index].get = tfa98xx_get_mute_ctl;
	tfa98xx_controls[mix_index].put = tfa98xx_set_mute_ctl;
	mix_index++;
#endif

#if defined(TFA_PAUSE_CONTROL)
	/* set pause / resume by force */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Pause", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_pause_ctl;
	tfa98xx_controls[mix_index].get = tfa98xx_get_pause_ctl;
	tfa98xx_controls[mix_index].put = tfa98xx_set_pause_ctl;
	mix_index++;
#endif

#if defined(TFA_TDMSPKG_CONTROL)
	/* set speaker gain by force */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Gain", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_spkgain;
	tfa98xx_controls[mix_index].get = tfa98xx_get_spkgain;
	tfa98xx_controls[mix_index].put = tfa98xx_set_spkgain;
	mix_index++;
#endif

	if (tfa98xx->flags & TFA98XX_FLAG_CALIBRATION_CTL) {
		name = devm_kzalloc(cdev,
			MAX_CONTROL_NAME, GFP_KERNEL);
		if (!name)
			return -ENOMEM;

		scnprintf(name, MAX_CONTROL_NAME,
			"%s Calibration", tfa98xx->fw.name);
		tfa98xx_controls[mix_index].name = name;
		tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		tfa98xx_controls[mix_index].info = tfa98xx_info_cal_ctl;
		tfa98xx_controls[mix_index].get = tfa98xx_get_cal_ctl;
		tfa98xx_controls[mix_index].put = tfa98xx_set_cal_ctl;
		mix_index++;
	}

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	ret = snd_soc_add_component_controls(tfa98xx->component,
		tfa98xx_controls, mix_index);
#else
	ret = snd_soc_add_codec_controls(tfa98xx->codec,
		tfa98xx_controls, mix_index);
#endif

	if (!ret)
		is_control_created = 1;

	return ret;
}

static void *tfa98xx_devm_kstrdup(struct device *dev, char *buf)
{
	char *str = devm_kzalloc(dev, strlen(buf) + 1, GFP_KERNEL);

	if (!str)
		return str;
	memcpy(str, buf, strlen(buf));

	return str;
}

static int tfa98xx_append_i2c_address(struct device *dev,
	struct i2c_client *i2c,
	struct snd_soc_dapm_widget *widgets,
	int num_widgets,
	struct snd_soc_dai_driver *dai_drv,
	int num_dai)
{
	char buf[50];
	int i;
	int i2cbus = i2c->adapter->nr;
	int addr = i2c->addr;

	if (dai_drv && num_dai > 0)
		for (i = 0; i < num_dai; i++) {
			snprintf(buf, 50, "%s-%d-%x",
				dai_drv[i].name,
				i2cbus, addr);
			dai_drv[i].name = tfa98xx_devm_kstrdup(dev, buf);
			pr_info("dai_drv[%d].name=%s\n", i, dai_drv[i].name);

			snprintf(buf, 50, "%s-%d-%x",
				dai_drv[i].playback.stream_name,
				i2cbus, addr);
			dai_drv[i].playback.stream_name
				= tfa98xx_devm_kstrdup(dev, buf);
			pr_info("dai_drv[%d].playback.stream_name=%s\n",
				i, dai_drv[i].playback.stream_name);

			snprintf(buf, 50, "%s-%d-%x",
				dai_drv[i].capture.stream_name,
				i2cbus, addr);
			dai_drv[i].capture.stream_name
				= tfa98xx_devm_kstrdup(dev, buf);
			pr_info("dai_drv[%d].capture.stream_name=%s\n",
				i, dai_drv[i].capture.stream_name);
		}

	/* the idea behind this is convert:
	 * SND_SOC_DAPM_AIF_IN
	 *  ("AIF IN","AIF Playback",0,SND_SOC_NOPM,0,0),
	 * into:
	 * SND_SOC_DAPM_AIF_IN
	 *  ("AIF IN","AIF Playback-2-36",0,SND_SOC_NOPM,0,0),
	 */
	if (widgets && num_widgets > 0)
		for (i = 0; i < num_widgets; i++) {
			if (!widgets[i].sname)
				continue;
			if ((widgets[i].id == snd_soc_dapm_aif_in)
				|| (widgets[i].id == snd_soc_dapm_aif_out)) {
				snprintf(buf, 50, "%s-%d-%x",
					widgets[i].sname, i2cbus, addr);
				widgets[i].sname
					= tfa98xx_devm_kstrdup(dev, buf);
				pr_info("widgets[%d].sname=%s\n",
					i, widgets[i].sname);
			}
		}

	return 0;
}

static struct snd_soc_dapm_widget tfa98xx_dapm_widgets_common[] = {
	/* Stream widgets */
	SND_SOC_DAPM_AIF_IN("AIF IN", "AIF Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF OUT", "AIF Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("OUTL"),
	SND_SOC_DAPM_INPUT("AEC Loopback"),
};

static struct snd_soc_dapm_widget tfa98xx_dapm_widgets_stereo[] = {
	SND_SOC_DAPM_OUTPUT("OUTR"),
};

#if defined(USE_DMIC_FOR_AMPLIFIER)
static struct snd_soc_dapm_widget tfa98xx_dapm_widgets_saam[] = {
	SND_SOC_DAPM_INPUT("SAAM MIC"),
};

static struct snd_soc_dapm_widget tfa9888_dapm_inputs[] = {
	SND_SOC_DAPM_INPUT("DMIC1"),
	SND_SOC_DAPM_INPUT("DMIC2"),
	SND_SOC_DAPM_INPUT("DMIC3"),
	SND_SOC_DAPM_INPUT("DMIC4"),
};
#endif

static const struct snd_soc_dapm_route tfa98xx_dapm_routes_common[] = {
	{"OUTL", NULL, "AIF IN"},
	{"AIF OUT", NULL, "AEC Loopback"},
};

static const struct snd_soc_dapm_route tfa98xx_dapm_routes_stereo[] = {
	{"OUTR", NULL, "AIF IN"},
};

#if defined(USE_DMIC_FOR_AMPLIFIER)
static const struct snd_soc_dapm_route tfa98xx_dapm_routes_saam[] = {
	{"AIF OUT", NULL, "SAAM MIC"},
};

static const struct snd_soc_dapm_route tfa9888_input_dapm_routes[] = {
	{"AIF OUT", NULL, "DMIC1"},
	{"AIF OUT", NULL, "DMIC2"},
	{"AIF OUT", NULL, "DMIC3"},
	{"AIF OUT", NULL, "DMIC4"},
};
#endif

#if KERNEL_VERSION(4, 2, 0) > LINUX_VERSION_CODE
static struct snd_soc_dapm_context *
snd_soc_codec_get_dapm(struct snd_soc_codec *codec)
{
	return &codec->dapm;
}
#endif

static void tfa98xx_add_widgets(struct tfa98xx *tfa98xx)
{
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	struct snd_soc_dapm_context *dapm
		= snd_soc_component_get_dapm(tfa98xx->component);
#elif KERNEL_VERSION(4, 2, 0) > LINUX_VERSION_CODE
	struct snd_soc_dapm_context *dapm
		= &tfa98xx->codec->dapm;
#else
	struct snd_soc_dapm_context *dapm
		= snd_soc_codec_get_dapm(tfa98xx->codec);
#endif
	unsigned int num_dapm_widgets
		= ARRAY_SIZE(tfa98xx_dapm_widgets_common);
	struct snd_soc_dapm_widget *widgets;

	widgets = devm_kzalloc(tfa98xx->dev,
		sizeof(struct snd_soc_dapm_widget)
		* ARRAY_SIZE(tfa98xx_dapm_widgets_common),
		GFP_KERNEL);
	if (!widgets)
		return;
	memcpy(widgets, tfa98xx_dapm_widgets_common,
		sizeof(struct snd_soc_dapm_widget)
		* ARRAY_SIZE(tfa98xx_dapm_widgets_common));

	tfa98xx_append_i2c_address(tfa98xx->dev,
		tfa98xx->i2c,
		widgets,
		num_dapm_widgets,
		NULL,
		0);

	snd_soc_dapm_new_controls(dapm, widgets,
		ARRAY_SIZE(tfa98xx_dapm_widgets_common));
	snd_soc_dapm_add_routes(dapm, tfa98xx_dapm_routes_common,
		ARRAY_SIZE(tfa98xx_dapm_routes_common));

#if defined(TFA_SET_DAPM_IGNORE_SUSPEND)
	snd_soc_dapm_ignore_suspend(dapm, "AIF IN");
	snd_soc_dapm_ignore_suspend(dapm, "OUTL");
	snd_soc_dapm_ignore_suspend(dapm, "AIF OUT");
	snd_soc_dapm_ignore_suspend(dapm, "AEC Loopback");
#endif

	if (tfa98xx->flags & TFA98XX_FLAG_STEREO_DEVICE) {
		snd_soc_dapm_new_controls
			(dapm, tfa98xx_dapm_widgets_stereo,
			ARRAY_SIZE(tfa98xx_dapm_widgets_stereo));
		snd_soc_dapm_add_routes
			(dapm, tfa98xx_dapm_routes_stereo,
			ARRAY_SIZE(tfa98xx_dapm_routes_stereo));

#if defined(TFA_SET_DAPM_IGNORE_SUSPEND)
		snd_soc_dapm_ignore_suspend(dapm, "OUTR");
#endif
	}

#if defined(USE_DMIC_FOR_AMPLIFIER)
	if (tfa98xx->flags & TFA98XX_FLAG_MULTI_MIC_INPUTS) {
		snd_soc_dapm_new_controls
			(dapm, tfa9888_dapm_inputs,
			ARRAY_SIZE(tfa9888_dapm_inputs));
		snd_soc_dapm_add_routes
			(dapm, tfa9888_input_dapm_routes,
			ARRAY_SIZE(tfa9888_input_dapm_routes));
	}

	if (tfa98xx->flags & TFA98XX_FLAG_SAAM_AVAILABLE) {
		snd_soc_dapm_new_controls
			(dapm, tfa98xx_dapm_widgets_saam,
			ARRAY_SIZE(tfa98xx_dapm_widgets_saam));
		snd_soc_dapm_add_routes
			(dapm, tfa98xx_dapm_routes_saam,
			ARRAY_SIZE(tfa98xx_dapm_routes_saam));
	}
#endif
}

/* I2C wrapper functions */
enum tfa98xx_error tfa98xx_write_register16(struct tfa_device *tfa,
	unsigned char subaddress,
	unsigned short value)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	int ret;
	int retries = I2C_RETRIES;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return TFA98XX_ERROR_FAIL;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (!tfa98xx || !tfa98xx->regmap) {
		pr_err("No tfa98xx regmap available\n");
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

retry:
	ret = regmap_write(tfa98xx->regmap, subaddress, value);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return TFA98XX_ERROR_FAIL;
	}

	if (tfa98xx_kmsg_regs)
		dev_dbg(tfa98xx->dev,
			"WR reg=0x%02x, val=0x%04x %s\n",
			subaddress, value,
			ret < 0 ? "Error!!" : "");

	if (tfa98xx_ftrace_regs)
		tfa98xx_trace_printk
			("WR reg=0x%02x, val=0x%04x %s\n",
			subaddress, value,
			ret < 0 ? "Error!!" : "");

	return error;
}

enum tfa98xx_error tfa98xx_read_register16(struct tfa_device *tfa,
	unsigned char subaddress,
	unsigned short *val)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	unsigned int value;
	int retries = I2C_RETRIES;
	int ret;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return TFA98XX_ERROR_FAIL;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (!tfa98xx || !tfa98xx->regmap) {
		pr_err("No tfa98xx regmap available\n");
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

retry:
	ret = regmap_read(tfa98xx->regmap, subaddress, &value);
	if (ret < 0) {
		pr_warn("i2c error at subaddress 0x%x, retries left: %d\n",
			subaddress, retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return TFA98XX_ERROR_FAIL;
	}
	*val = value & 0xffff;

	if (tfa98xx_kmsg_regs)
		dev_dbg(tfa98xx->dev,
			"RD reg=0x%02x, val=0x%04x %s\n",
			subaddress, *val,
			ret < 0 ? "Error!!" : "");
	if (tfa98xx_ftrace_regs)
		tfa98xx_trace_printk
			("RD reg=0x%02x, val=0x%04x %s\n",
			subaddress, *val,
			ret < 0 ? "Error!!" : "");

	return error;
}

/*
 * init external dsp
 */
enum tfa98xx_error tfa98xx_init_dsp(struct tfa_device *tfa)
{
	return TFA98XX_ERROR_NOT_SUPPORTED;
}

int tfa98xx_get_dsp_status(struct tfa_device *tfa)
{
	return 0;
}

/*
 * write external dsp message
 */
int tfa98xx_write_dsp(void *tfa,
	int num_bytes, const char *command_buffer)
{
	return TFA98XX_ERROR_NOT_SUPPORTED;
}

/*
 * read external dsp message
 */
int tfa98xx_read_dsp(void *tfa,
	int num_bytes, unsigned char *result_buffer)
{
	return TFA98XX_ERROR_NOT_SUPPORTED;
}
/*
 * write/read external dsp message
 */
int tfa98xx_writeread_dsp(void *tfa,
	int command_length, void *command_buffer,
	int result_length, void *result_buffer)
{
	return TFA98XX_ERROR_NOT_SUPPORTED;
}

enum tfa98xx_error tfa98xx_read_data(struct tfa_device *tfa,
	unsigned char reg,
	int len, unsigned char value[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	struct i2c_client *tfa98xx_client;
	int err;
	int tries = 0;
	unsigned char *reg_buf = NULL;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.len = 1,
			.buf = NULL,
		}, {
			.flags = I2C_M_RD,
			.len = len,
			.buf = value,
		},
	};

	reg_buf = (unsigned char *)
		kmalloc(sizeof(reg), GFP_DMA); /* GRP_KERNEL also works */
	if (!reg_buf)
		return -ENOMEM;

	*reg_buf = reg;
	msgs[0].buf = reg_buf;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return TFA98XX_ERROR_FAIL;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (tfa98xx->i2c) {
		tfa98xx_client = tfa98xx->i2c;
		msgs[0].addr = tfa98xx_client->addr;
		msgs[1].addr = tfa98xx_client->addr;

		do {
			err = i2c_transfer(tfa98xx_client->adapter, msgs,
				ARRAY_SIZE(msgs));
			if (err != ARRAY_SIZE(msgs))
				msleep_interruptible(I2C_RETRY_DELAY);
		} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

		if (err != ARRAY_SIZE(msgs)) {
			dev_err(&tfa98xx_client->dev,
				"read transfer error %d\n", err);
			error = TFA98XX_ERROR_FAIL;
		}

		if (tfa98xx_kmsg_regs)
			dev_dbg(&tfa98xx_client->dev,
				"RD-DAT reg=0x%02x, len=%d\n",
				reg, len);
		if (tfa98xx_ftrace_regs)
			tfa98xx_trace_printk
				("RD-DAT reg=0x%02x, len=%d\n",
				reg, len);
	} else {
		pr_err("No device available\n");
		error = TFA98XX_ERROR_FAIL;
	}

	kfree(reg_buf);

	return error;
}

enum tfa98xx_error tfa98xx_write_raw(struct tfa_device *tfa,
	int len,
	const unsigned char data[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	int ret;
	int retries = I2C_RETRIES;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return TFA98XX_ERROR_FAIL;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;

retry:
	ret = i2c_master_send(tfa98xx->i2c, data, len);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	if (ret == len) {
		if (tfa98xx_kmsg_regs)
			dev_dbg(tfa98xx->dev,
				"WR-RAW len=%d\n", len);
		if (tfa98xx_ftrace_regs)
			tfa98xx_trace_printk
				("WR-RAW len=%d\n", len);
		return TFA98XX_ERROR_OK;
	}
	pr_err("WR-RAW (len=%d) Error I2C send size mismatch %d\n",
		len, ret);
	error = TFA98XX_ERROR_FAIL;

	return error;
}

int tfa_ext_register(dsp_send_message_t tfa_send_message,
	dsp_read_message_t tfa_read_message,
	tfa_event_handler_t *tfa_event_handler)
{
	struct tfa98xx *tfa98xx;
	int dirt = 0;

	mutex_lock(&tfa98xx_mutex);

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa98xx->tfa->ext_dsp = 1;
		tfa98xx->tfa->is_probus_device = 1;
		tfa98xx->tfa->is_cold = 1;

		if (tfa_send_message != NULL) {
			dirt |= 0x1;
			tfa98xx->tfa->dev_ops.dsp_msg = tfa_send_message;
		}
		if (tfa_read_message != NULL) {
			dirt |= 0x2;
			tfa98xx->tfa->dev_ops.dsp_msg_read = tfa_read_message;
		}
	}

#if defined(TFA_SET_IPC_PORT_ID)
	if (tfa98xx->pid >= 0) {
		if (afe_tfadsp_setup((uint16_t)tfa98xx->pid) < 0)
			pr_err("%s: error in setting up port ID (%d)\n",
				__func__, tfa98xx->pid);
	} else {
		pr_err("%s: invalid port ID\n", __func__);
	}
#endif

	if (tfa_event_handler != NULL)
		tfa_event_handler
			= (tfa_event_handler_t *)tfa_ext_event_handler;

	if (dirt == 0x3)
		tfa_set_ipc_loaded(1);

	mutex_unlock(&tfa98xx_mutex);

	return 0;
}
#if !defined(TFA_SET_EXT_INTERNALLY)
EXPORT_SYMBOL(tfa_ext_register);
#endif

#if defined(TFA_SET_EXT_INTERNALLY)
#ifdef MPLATFORM
int ipi_tfadsp_write(void *tfa, int length, const char *buf)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int ret = 0;

	if (buf == NULL) {
		pr_err("%s: error with NULL buffer\n", __func__);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

#if defined(TFADSP_DSP_MSG_PACKET_STRATEGY)
	if (length > 6) {
		pr_debug("%s: [0]:0x%02x-[1]:0x%02x-[2]:0x%02x-[3]:0x%02x, packet_id:%d, packet_size:%d\n",
			__func__, buf[0], buf[1], buf[2], buf[3],
			(buf[0] << 8) | buf[1], (buf[2] << 8) | buf[3]);
		pr_debug("%s: [4]:0x%02x-[5]:0x%02x-[6]:0x%02x, length:%d\n",
			__func__, buf[4], buf[5], buf[6], length);
	}
#else
	if (length >= 3)
		pr_debug("%s: [0]:0x%02x-[1]:0x%02x-[2]:0x%02x, length:%d\n",
			__func__, buf[0], buf[1], buf[2], length);
#endif /* TFADSP_DSP_MSG_PACKET_STRATEGY */

	ret = mtk_spk_send_ipi_buf_to_dsp((void *)buf, (uint32_t)length);
	if (ret != 0) {
		pr_err("%s: error in sending message to DSP (err %d)\n",
			__func__, ret);
		err = TFA98XX_ERROR_DSP_NOT_RUNNING;
	}

	return (int)err;
}

int ipi_tfadsp_read(void *tfa, int length, unsigned char *bytes)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int ret = 0;
	uint32_t buf_len;

	if (bytes == NULL) {
		pr_err("%s: error with NULL buffer\n", __func__);
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

	ret = mtk_spk_recv_ipi_buf_from_dsp((int8_t *)bytes,
		(int16_t)length, &buf_len);
	if (ret != 0) {
		pr_err("%s: error in receiving message to DSP (err %d)\n",
			__func__, ret);
		err = TFA98XX_ERROR_DSP_NOT_RUNNING;
	}

	if (length >= 3)
		pr_debug("%s: [0]:0x%02x-[1]:0x%02x-[2]:0x%02x, length:%d, buf_len:%d\n",
			__func__, bytes[0], bytes[1], bytes[2],
			length, buf_len);

	return (int)err;
}
#endif
#endif /* TFA_SET_EXT_INTERNALLY */

#if defined(TFA_BLACKBOX_LOGGING)
int tfa_set_blackbox(int enable)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list)
		tfa98xx->tfa->blackbox_enable = enable;

	mutex_unlock(&tfa98xx_mutex);

	return 0;
}
#if defined(TFA_USE_TFALOG_NODE)
EXPORT_SYMBOL(tfa_set_blackbox);
#endif
#endif /* TFA_BLACKBOX_LOGGING */

/* Interrupts management */
static void tfa98xx_interrupt_enable_tfa2(struct tfa98xx *tfa98xx, bool enable)
{
#if defined(USE_INTERRUPT_FROM_AMPLIFIER)
#if (defined(USE_TFA9891) || defined(USE_TFA9912))
	/* Only for 0x72 we need to enable NOCLK interrupts */
	if (tfa98xx->flags & TFA98XX_FLAG_REMOVE_PLOP_NOISE)
		tfa_irq_ena(tfa98xx->tfa,
			tfa9912_irq_stnoclk, enable);

	if (tfa98xx->flags & TFA98XX_FLAG_LP_MODES) {
		tfa_irq_ena(tfa98xx->tfa,
			36, enable); /* FIXME: IELP0 does not excist for 9912 */
		tfa_irq_ena(tfa98xx->tfa,
			tfa9912_irq_stclpr, enable);
	}
#endif /* (USE_TFA9891) || (USE_TFA9912) */
#endif
}

#if (defined(USE_TFA9891) || defined(USE_TFA9912))
/* Check if tap-detection can and shall be enabled.
 * Configure SPK interrupt accordingly or setup polling mode
 * Tap-detection shall be active if:
 * - the service is enabled (tapdet_open), AND
 * - the current profile is a tap-detection profile
 * On TFA1 familiy of devices, activating tap-detection means enabling the SPK
 * interrupt if available.
 * We also update the tapdet_enabled and tapdet_poll variables.
 */
static void tfa98xx_tapdet_check_update(struct tfa98xx *tfa98xx)
{
	unsigned int enable = false;
	struct device *cdev;

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	cdev = tfa98xx->component->dev;
#else
	cdev = tfa98xx->codec->dev;
#endif

	/* Support tap-detection on TFA1 family of devices */
	if ((tfa98xx->flags & TFA98XX_FLAG_TAPDET_AVAILABLE) == 0)
		return;

	if (tfa98xx->tapdet_open &&
		(tfa98xx->tapdet_profiles & (1 << tfa98xx->profile)))
		enable = true;

	if (!gpio_is_valid(tfa98xx->irq_gpio)) {
		dev_dbg(cdev, "Polling for tap-detection: %s (%d; 0x%x, %d)\n",
			enable ? "enabled" : "disabled",
			tfa98xx->tapdet_open, tfa98xx->tapdet_profiles,
			tfa98xx->profile);

		/* interrupt not available, setup polling mode */
		tfa98xx->tapdet_poll = true;
		if (enable)
			queue_delayed_work(tfa98xx->tfa98xx_wq,
				&tfa98xx->tapdet_work, HZ / 10);
		else
			cancel_delayed_work_sync(&tfa98xx->tapdet_work);
	} else {
		dev_dbg(cdev, "Interrupt for tap-detection: %s (%d; 0x%x, %d)\n",
			enable ? "enabled" : "disabled",
			tfa98xx->tapdet_open, tfa98xx->tapdet_profiles,
			tfa98xx->profile);

		/* enabled interrupt */
		tfa_irq_ena(tfa98xx->tfa, tfa9912_irq_sttapdet, enable);
	}

	/* check disabled => enabled transition to clear pending events */
	if (!tfa98xx->tapdet_enabled && enable)
		/* clear pending event if any */
		tfa_irq_clear(tfa98xx->tfa,
			tfa9912_irq_sttapdet);

	if (!tfa98xx->tapdet_poll)
		tfa_irq_ena(tfa98xx->tfa,
			tfa9912_irq_sttapdet, 1); /* enable again */
}
#endif /* (USE_TFA9891) || (USE_TFA9912) */

/* global enable / disable interrupts */
static void tfa98xx_interrupt_enable(struct tfa98xx *tfa98xx, bool enable)
{
	if (tfa98xx->flags & TFA98XX_FLAG_SKIP_INTERRUPTS)
		return;

	if (tfa98xx->tfa->tfa_family == 2)
		tfa98xx_interrupt_enable_tfa2(tfa98xx, enable);
}

/* Firmware management */
static void tfa98xx_container_loaded
	(const struct firmware *cont, void *context)
{
	struct tfa_container *container;
	struct tfa98xx *tfa98xx = context;
	enum tfa_error tfa_err;
	int container_size;
#if defined(TFA_PRELOAD_SETTING_AT_PROBING)
	int ret;
	static struct tfa98xx *tfa98xx_main;
#endif
#if defined(TFA_FS_CHECK_MTPEX)
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	unsigned int value;
#else
	unsigned short value;
#endif
#endif

	mutex_lock(&probe_lock);

	if (tfa98xx->dsp_fw_state == TFA98XX_DSP_FW_OK) {
		pr_info("%s: Already loaded\n", __func__);
		mutex_unlock(&probe_lock);
		return;
	}
	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;

	if (!cont) {
		pr_err("Failed to read %s\n", fw_name);
		mutex_unlock(&probe_lock);
		return;
	}

	pr_debug("loaded %s - size: %zu\n", fw_name, cont->size);

	mutex_lock(&tfa98xx_mutex);
	if (tfa98xx_container == NULL) {
		container = kzalloc(cont->size, GFP_KERNEL);
		if (container == NULL) {
			mutex_unlock(&tfa98xx_mutex);
			release_firmware(cont);
			pr_err("Error allocating memory\n");
			mutex_unlock(&probe_lock);
			return;
		}

		container_size = cont->size;
		memcpy(container, cont->data, container_size);
		release_firmware(cont);

		pr_debug("%.2s%.2s\n", container->version,
			container->subversion);
		pr_debug("%.8s\n", container->customer);
		pr_debug("%.8s\n", container->application);
		pr_debug("%.8s\n", container->type);
		pr_debug("%d ndev\n", container->ndev);
		pr_debug("%d nprof\n", container->nprof);

		tfa_err = tfa_load_cnt(container, container_size);
		if (tfa_err != tfa_error_ok) {
			mutex_unlock(&tfa98xx_mutex);
			kfree(container);
			dev_err(tfa98xx->dev, "Cannot load container file, aborting\n");
			mutex_unlock(&probe_lock);
			return;
		}

		tfa98xx_container = container;
	} else {
		pr_debug("container file already loaded...\n");
		container = tfa98xx_container;
		release_firmware(cont);
	}
	mutex_unlock(&tfa98xx_mutex);

	tfa98xx->tfa->cnt = container;

	/*
	 * i2c transaction limited to 64k
	 * (Documentation/i2c/writing-clients)
	 */
	tfa98xx->tfa->buffer_size = 65536;

	/* DSP messages via i2c */
	tfa98xx->tfa->has_msg = 0;

	if (tfa_dev_probe(tfa98xx->i2c->addr, tfa98xx->tfa) != 0) {
		dev_err(tfa98xx->dev,
			"Failed to probe TFA98xx @ 0x%.2x\n",
			tfa98xx->i2c->addr);
		mutex_unlock(&probe_lock);
		return;
	}

/* TEMPORARY, until TFA device is probed before tfa_ext is called */
	if (tfa98xx->tfa->is_probus_device) {
#if defined(TFA_SET_EXT_INTERNALLY)
#if defined(QPLATFORM)
		tfa98xx->tfa->dev_ops.dsp_msg
			= (dsp_send_message_t)afe_tfadsp_write;
		tfa98xx->tfa->dev_ops.dsp_msg_read
			= (dsp_read_message_t)afe_tfadsp_read;
#if defined(TFA_SET_IPC_PORT_ID)
		if (tfa98xx->pid >= 0) {
			if (afe_tfadsp_setup((uint16_t)tfa98xx->pid) < 0)
				pr_err("%s: error in setting up port ID (%d)\n",
					__func__, tfa98xx->pid);
		} else {
			pr_err("%s: invalid port ID\n", __func__);
		}
#endif
#elif defined(MPLATFORM)
		tfa98xx->tfa->dev_ops.dsp_msg
			= (dsp_send_message_t)ipi_tfadsp_write;
		tfa98xx->tfa->dev_ops.dsp_msg_read
			= (dsp_read_message_t)ipi_tfadsp_read;
#endif
		tfa_set_ipc_loaded(1);
#endif /* TFA_SET_EXT_INTERNALLY */
	} else {
		tfa98xx->tfa->dev_ops.dsp_msg
			= (dsp_send_message_t)tfa_dsp_msg;
		tfa98xx->tfa->dev_ops.dsp_msg_read
			= (dsp_read_message_t)tfa_dsp_msg_read;
		tfa_set_ipc_loaded(1);
	}

	/* Enable debug traces */
	tfa98xx->tfa->verbose = trace_level & 1;

	/* prefix is the application name from the cnt */
	tfa_cont_get_app_name(tfa98xx->tfa, tfa98xx->fw.name);

	/* set default profile/vstep */
	tfa98xx->profile = 0;
	tfa98xx->vstep = 0;

	/* Override default profile if requested */
	if (strcmp(dflt_prof_name, "")) {
		unsigned int i;
		int nprof = tfa_cnt_get_dev_nprof(tfa98xx->tfa);

		for (i = 0; i < nprof; i++) {
			if (strcmp(_tfa_cont_profile_name(tfa98xx, i),
				dflt_prof_name) == 0) {
				tfa98xx->profile = i;
				dev_info(tfa98xx->dev,
					"changing default profile to %s (%d)\n",
					dflt_prof_name, tfa98xx->profile);
				break;
			}
		}
		if (i >= nprof)
			dev_info(tfa98xx->dev,
				"Default profile override failed (%s profile not found)\n",
				dflt_prof_name);
	}

	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_OK;

#if defined(TFA_FS_CHECK_MTPEX)
#if KERNEL_VERSION(5, 9, 0) <= LINUX_VERSION_CODE
	/* snd_soc_component_read32, not supported later than 5.8 */
	value = snd_soc_component_read(tfa98xx->component,
		TFA98XX_KEY2_PROTECTED_MTP0);
#elif KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	value = snd_soc_component_read32(tfa98xx->component,
		TFA98XX_KEY2_PROTECTED_MTP0);
#else
	value = snd_soc_read(tfa98xx->codec, TFA98XX_KEY2_PROTECTED_MTP0);
#endif
	if (value != -1) {
		tfa98xx->calibrate_done =
			(value & TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK) ? 1 : 0;
		pr_info("[0x%x] calibrate_done = MTPEX (%d) 0x%04x\n",
			tfa98xx->i2c->addr, tfa98xx->calibrate_done, value);
	} else {
		pr_info("[0x%x] error in reading MTPEX\n", tfa98xx->i2c->addr);
		tfa98xx->calibrate_done = 0;
	}
#else
	tfa98xx->calibrate_done = 0;
#endif

	pr_debug("Firmware init complete\n");

#if defined(TFADSP_DSP_BUFFER_POOL)
	/* allocate buffer_pool */
	if (tfa98xx->tfa->dev_idx == 0) {
		int index = 0;

		pr_info("Allocate buffer_pool\n");
		for (index = 0; index < POOL_MAX_INDEX; index++)
			tfa_buffer_pool(tfa98xx->tfa, index,
				buf_pool_size[index], POOL_ALLOC);
	}
#endif

	if (no_start != 0) {
		mutex_unlock(&probe_lock);
		return;
	}

	/* Only controls for main device */
#if !defined(TFA_PRELOAD_SETTING_AT_PROBING)
	/* for the first device */
	if (tfa98xx->tfa->dev_idx == 0)
		tfa98xx_create_controls(tfa98xx);
#else
	/* for the last device */
	if (tfa98xx->tfa->dev_idx == 0)
		tfa98xx_main = tfa98xx;
	if (tfa98xx->tfa->dev_idx
		== tfa98xx->tfa->dev_count - 1) {
		if (tfa98xx_main != NULL)
			tfa98xx_create_controls(tfa98xx_main);
	}
#endif /* TFA_PRELOAD_SETTING_AT_PROBING */

#if (defined(USE_TFA9891) || defined(USE_TFA9912))
	tfa98xx_inputdev_check_register(tfa98xx);
#endif

	if (tfa_is_cold(tfa98xx->tfa) == 0) {
		pr_debug("Warning: device 0x%.2x is still warm\n",
			tfa98xx->i2c->addr);
		tfa_reset(tfa98xx->tfa);
	}

	/* Preload settings using internal clock on TFA2 */
#if defined(TFA_PRELOAD_SETTING_AT_PROBING)
	if (tfa98xx->tfa->tfa_family == 2) {
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa98xx_tfa_start(tfa98xx,
			tfa98xx->profile, tfa98xx->vstep);
		if (ret == TFA98XX_ERROR_NOT_SUPPORTED)
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
		if (ret == tfa_error_ok)
			tfa98xx->tfa->first_after_boot = 2;
		mutex_unlock(&tfa98xx->dsp_lock);
	}
#endif /* TFA_PRELOAD_SETTING_AT_PROBING */

#if defined(TFA_DISABLE_INT_VDDS)
	/* disable VDDS interrupt flag to void POR interrupt */
	if (tfa98xx->tfa->tfa_family == 2)
		tfa_set_bf(tfa98xx->tfa, TFA2_BF_IEVDDS, 0);
#endif /* TFA_DISABLE_INT_VDDS */

	tfa98xx_interrupt_enable(tfa98xx, true);

	mutex_unlock(&probe_lock);
}

static int tfa98xx_load_container(struct tfa98xx *tfa98xx)
{
	int tries = 0, ret;

	mutex_lock(&probe_lock);
	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_PENDING;
	mutex_unlock(&probe_lock);

	do {
		ret = request_firmware_nowait(THIS_MODULE,
			FW_ACTION_HOTPLUG,
			fw_name, tfa98xx->dev, GFP_KERNEL,
			tfa98xx, tfa98xx_container_loaded);

		/* wait until driver completes loading */
		msleep_interruptible(20);
		if (tfa98xx->dsp_fw_state == TFA98XX_DSP_FW_OK)
			break;

		msleep_interruptible(80);
		tries++;
	} while (tries < TFA98XX_LOADFW_NTRIES
		&& tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK);

	return ret;
}

#if (defined(USE_TFA9891) || defined(USE_TFA9912))
static void tfa98xx_tapdet(struct tfa98xx *tfa98xx)
{
	unsigned int tap_pattern;
	int btn;

	/* check tap pattern (BTN_0 is "error" wrong tap indication */
	tap_pattern = tfa_get_tap_pattern(tfa98xx->tfa);
	switch (tap_pattern) {
	case 0xffffffff:
		pr_info("More than 4 taps detected! (flagTapPattern = -1)\n");
		btn = BTN_0;
		break;
	case 0xfffffffe:
	case 0xfe:
		pr_info("Illegal tap detected!\n");
		btn = BTN_0;
		break;
	case 0:
		pr_info("Unrecognized pattern! (flagTapPattern = 0)\n");
		btn = BTN_0;
		break;
	default:
		pr_info("Detected pattern: %d\n", tap_pattern);
		btn = BTN_0 + tap_pattern;
		break;
	}

	input_report_key(tfa98xx->input, btn, 1);
	input_report_key(tfa98xx->input, btn, 0);
	input_sync(tfa98xx->input);

	/* acknowledge event done by clearing interrupt */
}

static void tfa98xx_tapdet_work(struct work_struct *work)
{
	struct tfa98xx *tfa98xx;

	/* TODO check is this is still needed for tap polling */
	tfa98xx = container_of(work, struct tfa98xx, tapdet_work.work);

	if (tfa_irq_get(tfa98xx->tfa, tfa9912_irq_sttapdet))
		tfa98xx_tapdet(tfa98xx);

	queue_delayed_work(tfa98xx->tfa98xx_wq,
		&tfa98xx->tapdet_work, HZ / 10);
}
#endif /* (USE_TFA9891) || (USE_TFA9912) */

static void tfa98xx_monitor(struct work_struct *work)
{
	struct tfa98xx *tfa98xx;
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int handle = -1;
	unsigned int val;
	int ret;

	mutex_lock(&probe_lock);

	tfa98xx = container_of(work, struct tfa98xx, monitor_work.work);

	pr_info("%s: [%d] - profile = %d: %s\n", __func__,
		tfa98xx->tfa->dev_idx, tfa98xx->profile,
		_tfa_cont_profile_name(tfa98xx, tfa98xx->profile));

	if (tfa98xx->tfa->active_count == -1)
		tfa_set_active_handle(tfa98xx->tfa, tfa98xx->profile);
	handle = tfa98xx->tfa->active_handle;

	if (handle != -1) {
		pr_info("%s: profile = %d, active handle [%s]\n",
			__func__, tfa98xx->profile,
			tfa_cont_device_name(tfa98xx->tfa->cnt,
			handle));
		if (handle != tfa98xx->tfa->dev_idx)
			goto tfa_monitor_exit;
	} else {
		pr_info("%s: profile = %d, all active\n",
			__func__, tfa98xx->profile);
		handle = tfa98xx->tfa->dev_idx;
	}

	/* Check for tap-detection - bypass monitor if it is active */
	if (tfa98xx->input)
		goto tfa_monitor_exit;

	mutex_lock(&tfa98xx->dsp_lock);
#if defined(USE_TFA9874) || defined(USE_TFA9878) || defined(USE_TFA9894)
	error = tfa7x_status(tfa98xx->tfa);
#else
	error = tfa_status(tfa98xx->tfa);
#endif
	mutex_unlock(&tfa98xx->dsp_lock);

	if (error == TFA98XX_ERROR_DSP_NOT_RUNNING) {
		if (tfa98xx->dsp_init == TFA98XX_DSP_INIT_DONE) {
			tfa98xx->dsp_init = TFA98XX_DSP_INIT_RECOVER;
			tfa98xx_set_dsp_configured(tfa98xx);
#if !defined(TFA_USE_DIRECT_API_CALL)
			queue_delayed_work(tfa98xx->tfa98xx_wq,
				&tfa98xx->init_work, 0);
#else
			pr_info("%s: dsp_init (direct) with device %d, profile %d\n",
				__func__,
				tfa98xx->tfa->dev_idx,
				tfa98xx->profile);
			tfa98xx_dsp_init(tfa98xx);
#endif
		}
	}

	/* for debugging */
	mutex_lock(&tfa98xx->dsp_lock);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_SYS_CONTROL0, &val);
	if (!ret)
		pr_debug("[%d] SYS_CONTROL0: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_SYS_CONTROL1, &val);
	if (!ret)
		pr_debug("[%d] SYS_CONTROL1: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_SYS_CONTROL2, &val);
	if (!ret)
		pr_debug("[%d] SYS_CONTROL2: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_CLOCK_CONTROL, &val);
	if (!ret)
		pr_debug("[%d] CLOCK_CONTROL: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_STATUS_FLAGS0, &val);
	if (!ret)
		pr_debug("[%d] STATUS_FLAG0: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_STATUS_FLAGS1, &val);
	if (!ret)
		pr_debug("[%d] STATUS_FLAG1: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_STATUS_FLAGS3, &val);
	if (!ret)
		pr_debug("[%d] STATUS_FLAG3: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_STATUS_FLAGS4, &val);
	if (!ret)
		pr_debug("[%d] STATUS_FLAG4: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_TDM_CONFIG0, &val);
	if (!ret)
		pr_debug("[%d] TDM_CONFIG0: 0x%04x\n", handle, val);
	mutex_unlock(&tfa98xx->dsp_lock);

tfa_monitor_exit:
	pr_info("%s: exit\n", __func__);

	mutex_unlock(&probe_lock);

	if (!tfa98xx->tfa->verbose)
		return;

	if (tfa98xx_monitor_count != -1)
		if (++tfa98xx_monitor_count > MONITOR_COUNT_MAX)
			return;

	/* reschedule */
	queue_delayed_work(tfa98xx->tfa98xx_wq,
		&tfa98xx->monitor_work, 5 * HZ);
}

static void tfa98xx_dsp_init(struct tfa98xx *tfa98xx)
{
	int ret;
	static bool failed;
	bool reschedule = false;
	bool sync = false;
	bool do_sync;
	int active_handle = -1;

	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		pr_debug("Skipping tfa_dev_start (no FW: %d)\n",
			tfa98xx->dsp_fw_state);
		return;
	}

	if (tfa98xx->dsp_init == TFA98XX_DSP_INIT_DONE) {
		pr_debug("Stream already started, skipping DSP power-on\n");
		return;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	pr_info("%s: ...\n", __func__);

	tfa98xx->dsp_init = TFA98XX_DSP_INIT_PENDING;

#if defined(TFA_RETRY_TO_START)
	if (tfa98xx->init_count < TF98XX_MAX_DSP_START_TRY_COUNT) {
		/* directly try to start DSP */
		ret = tfa98xx_tfa_start(tfa98xx,
			tfa98xx->profile, tfa98xx->vstep);
		if (ret == TFA98XX_ERROR_NOT_SUPPORTED) {
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
			dev_err(tfa98xx->dev, "Failed starting device\n");
			failed = true;
		} else if (ret != TFA98XX_ERROR_OK) {
			/* It may fail as we may not have a valid clock at that
			 * time, so re-schedule and re-try later.
			 */
			dev_err(tfa98xx->dev,
				"tfa_dev_start failed! (err %d) - %d\n",
				ret, tfa98xx->init_count);
			reschedule = true;
		} else {
			sync = true;
			failed = false;

			/* Subsystem ready, tfa init complete */
			tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
			dev_dbg(tfa98xx->dev,
				"tfa_dev_start success (%d)\n",
				tfa98xx->init_count);
#if !defined(TFA_USE_DIRECT_API_CALL)
			/* cancel other pending init works */
			cancel_delayed_work(&tfa98xx->init_work);
#endif
			tfa98xx->init_count = 0;
		}
	} else {
		/* exceeded max number ot start tentatives, cancel start */
		dev_err(tfa98xx->dev,
			"Failed starting device (%d)\n",
			tfa98xx->init_count);
		failed = true;
#if defined(TFA_BYPASS_AT_START_FAILURE)
		sync = true; /* unmute by force, even if it fails */
#endif
#if !defined(TFA_USE_DIRECT_API_CALL)
		/* cancel other pending init works */
		cancel_delayed_work(&tfa98xx->init_work);
#endif
		tfa98xx->init_count = 0;
	}
#else
	/* directly try to start DSP */
	ret = tfa98xx_tfa_start(tfa98xx,
		tfa98xx->profile, tfa98xx->vstep);
	if (ret == TFA98XX_ERROR_NOT_SUPPORTED) {
		tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
		dev_err(tfa98xx->dev, "Failed starting device\n");
		failed = true;
	} else if (ret != TFA98XX_ERROR_OK) {
		dev_err(tfa98xx->dev,
			"Failed starting device (err %d) - %d\n",
			ret, tfa98xx->init_count);
		failed = true;
#if defined(TFA_BYPASS_AT_START_FAILURE)
		sync = true; /* unmute by force, even if it fails */
#endif
#if !defined(TFA_USE_DIRECT_API_CALL)
		/* cancel other pending init works */
		cancel_delayed_work(&tfa98xx->init_work);
#endif
		tfa98xx->init_count = 0;
	} else {
		sync = true;
		failed = false;

		/* Subsystem ready, tfa init complete */
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
		dev_dbg(tfa98xx->dev,
			"tfa_dev_start success (%d)\n",
			tfa98xx->init_count);
#if !defined(TFA_USE_DIRECT_API_CALL)
		/* cancel other pending init works */
		cancel_delayed_work(&tfa98xx->init_work);
#endif
		tfa98xx->init_count = 0;
	}
#endif /* TFA_RETRY_TO_START */

	mutex_unlock(&tfa98xx->dsp_lock);

	if (reschedule) {
		struct tfa98xx *ntfa98xx;

		failed = false;

		/* reschedule this init work for later */
		list_for_each_entry(ntfa98xx, &tfa98xx_device_list, list) {
#if !defined(TFA_USE_DIRECT_API_CALL)
			queue_delayed_work(ntfa98xx->tfa98xx_wq,
				&ntfa98xx->init_work,
				msecs_to_jiffies(5));
			ntfa98xx->init_count++;
#else
			ntfa98xx->init_count++;
			pr_info("%s: dsp_init (direct) with device %d, profile %d\n",
				__func__,
				ntfa98xx->tfa->dev_idx,
				ntfa98xx->profile);
			tfa98xx_dsp_init(ntfa98xx);
#endif
		}
	}

	if (!sync)
		return;

	if (tfa98xx->tfa->active_count == -1)
		tfa_set_active_handle(tfa98xx->tfa, tfa98xx->profile);
	active_handle = tfa98xx->tfa->active_handle;

	/* check if all devices have started */
	mutex_lock(&tfa98xx_mutex);
	if (tfa98xx_sync_count < tfa98xx_device_count)
		tfa98xx_sync_count++;

	do_sync = (tfa98xx_sync_count >= tfa98xx_device_count);

	/* when all devices have started then unmute */
	if (do_sync) {
		tfa98xx_sync_count = 0;

		list_for_each_entry(tfa98xx,
			&tfa98xx_device_list, list) {
			struct tfa_device *ntfa = tfa98xx->tfa;

			mutex_lock(&tfa98xx->dsp_lock);
			if (failed)
				tfa98xx->dsp_init
					= TFA98XX_DSP_INIT_FAIL;
			tfa98xx_set_dsp_configured(tfa98xx);
			mutex_unlock(&tfa98xx->dsp_lock);

			if ((active_handle != -1)
				&& (active_handle != ntfa->dev_idx))
				continue;

			pr_info("%s: profile = %d, active handle [%s]\n",
				__func__, tfa98xx->profile,
				tfa_cont_device_name(ntfa->cnt,
				ntfa->dev_idx));

			if (failed) {
				tfa_handle_damaged_speakers(ntfa);
				continue;
			}

#if defined(TFA_TDMSPKG_CONTROL)
			if (ntfa->spkgain != -1) {
				pr_info("%s: set speaker gain 0x%x\n",
					__func__, ntfa->spkgain);
				TFA7x_SET_BF(ntfa, TDMSPKG,
					ntfa->spkgain);
			}
#endif
			pr_info("%s: UNMUTE dev %d\n",
				__func__, ntfa->dev_idx);
			tfa_dev_set_state(ntfa,
				TFA_STATE_UNMUTE, 0);

			mutex_lock(&tfa98xx->dsp_lock);
			/*
			 * start monitor thread to check IC status bit
			 * periodically, and re-init IC to recover if
			 * needed.
			 */
			tfa98xx_monitor_count = 0;
			queue_delayed_work(tfa98xx->tfa98xx_wq,
				&tfa98xx->monitor_work,
				1 * HZ);
			mutex_unlock(&tfa98xx->dsp_lock);
		}

		failed = false;
	}
	mutex_unlock(&tfa98xx_mutex);
}

#if !defined(TFA_USE_DIRECT_API_CALL)
static void tfa98xx_dsp_init_work(struct work_struct *work)
{
	struct tfa98xx *tfa98xx
		= container_of(work, struct tfa98xx, init_work.work);

	pr_debug("%s: dsp_init (queued) with device %d, profile %d\n",
		__func__, tfa98xx->tfa->dev_idx, tfa98xx->profile);

	/* check index if it needs dsp init only for main device */

	tfa98xx_dsp_init(tfa98xx);
}
#endif

static void tfa98xx_interrupt(struct work_struct *work)
{
	struct tfa98xx *tfa98xx
		= container_of(work, struct tfa98xx, interrupt_work.work);

	pr_info("\n");

#if (defined(USE_TFA9891) || defined(USE_TFA9912))
	/* TFA98XX_FLAG_TAPDET_AVAILABLE */
	if (tfa98xx->flags & TFA98XX_FLAG_TAPDET_AVAILABLE) {
		/* check for tap interrupt */
		if (tfa_irq_get(tfa98xx->tfa, tfa9912_irq_sttapdet)) {
			tfa98xx_tapdet(tfa98xx);

			/* clear interrupt */
			tfa_irq_clear(tfa98xx->tfa, tfa9912_irq_sttapdet);
		}
	}

	/* TFA98XX_FLAG_REMOVE_PLOP_NOISE */
	if (tfa98xx->flags & TFA98XX_FLAG_REMOVE_PLOP_NOISE) {
		int start_triggered;

		mutex_lock(&tfa98xx->dsp_lock);
		start_triggered = tfa_plop_noise_interrupt
			(tfa98xx->tfa, tfa98xx->profile, tfa98xx->vstep);
		/* Only enable when the return value is 1,
		 * otherwise the interrupt is triggered twice
		 */
		if (start_triggered)
			tfa98xx_interrupt_enable(tfa98xx, true);
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	/* TFA98XX_FLAG_LP_MODES */
	if (tfa98xx->flags & TFA98XX_FLAG_LP_MODES)
		tfa_lp_mode_interrupt(tfa98xx->tfa);
#endif /* (USE_TFA9891) || (USE_TFA9912) */

	/* unmask interrupts masked in IRQ handler */
	tfa_irq_unmask(tfa98xx->tfa);
}

static int tfa98xx_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	struct snd_soc_component *component = dai->component;
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	int idx = 0;
#if !defined(TFA_NO_SND_FORMAT_CHECK)
#if defined(TFA_CHECK_PROFILE_RATE)
	unsigned int sr;
	int len, prof, nprof;
#if defined(TFADSP_DSP_BUFFER_POOL)
	char basename[MAX_CONTROL_NAME] = {0};
#else
	char *basename;
#endif
#else
	int i = 0;
#endif /* TFA_CHECK_PROFILE_RATE */
	u64 formats;
	int err;
#endif /* TFA_NO_SND_FORMAT_CHECK */
	struct device *cdev;

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	cdev = component->dev;
#else
	cdev = codec->dev;
#endif

	/*
	 * Support CODEC to CODEC links,
	 * these are called with a NULL runtime pointer.
	 */
	if (!substream->runtime)
		return 0;

	if (pcm_no_constraint != 0)
		return 0;

#if !defined(TFA_NO_SND_FORMAT_CHECK)
	switch (pcm_sample_format) {
	case 0:
		formats = SNDRV_PCM_FMTBIT_S16_LE;
		break;
	case 1:
		formats = SNDRV_PCM_FMTBIT_S24_LE;
		break;
	case 2:
		formats = SNDRV_PCM_FMTBIT_S32_LE;
		break;
	default:
		formats = TFA98XX_FORMATS;
		break;
	}

	err = snd_pcm_hw_constraint_mask64(substream->runtime,
		SNDRV_PCM_HW_PARAM_FORMAT, formats);
	if (err < 0)
		return err;
#else
	pr_info("%s: skip setting format constraint, assuming configurable format\n",
		__func__);
#endif /* TFA_NO_SND_FORMAT_CHECK */

	if (no_start != 0)
		return 0;

	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		dev_info(cdev, "Container file not loaded\n");
		return -EINVAL;
	}

	tfa98xx->rate_constraint.list = &tfa98xx->rate_constraint_list[0];
	tfa98xx->rate_constraint.count = 0;

#if !defined(TFA_NO_SND_FORMAT_CHECK)
#if defined(TFA_CHECK_PROFILE_RATE)
	/* loop over all profiles and get the supported samples rate(s) from
	 * the profiles with the same basename
	 */
#if !defined(TFADSP_DSP_BUFFER_POOL)
	basename = kzalloc(MAX_CONTROL_NAME, GFP_KERNEL);
	if (!basename)
		return -ENOMEM;
#endif

	/* copy profile name into basename until the . */
	get_profile_basename(basename,
		_tfa_cont_profile_name(tfa98xx, tfa98xx->profile));
	len = strlen(basename);

	nprof = tfa_cnt_get_dev_nprof(tfa98xx->tfa);
	for (prof = 0; prof < nprof; prof++) {
		if (strncmp(basename,
			_tfa_cont_profile_name(tfa98xx, prof), len)
			== 0) {
			/* Check which sample rate is supported
			 * with current profile, and enforce this.
			 */
			sr = tfa98xx_get_profile_sr(tfa98xx->tfa, prof);
			if (!sr)
				dev_info(cdev,
					"Unable to identify supported sample rate\n");

			if (tfa98xx->rate_constraint.count
				>= TFA98XX_NUM_RATES) {
				dev_err(cdev, "too many sample rates\n");
			} else {
				tfa98xx->rate_constraint_list[idx++] = sr;
				tfa98xx->rate_constraint.count += 1;
			}
		}
	}

#if !defined(TFADSP_DSP_BUFFER_POOL)
	kfree(basename);
#endif
#else
	pr_info("%s: add all the supported rates: 0x%04x\n",
		__func__, TFA98XX_RATES);
	for (i = 0; i < (int)ARRAY_SIZE(index_to_rate); i++) {
		if ((1 << i) & TFA98XX_RATES) {
			tfa98xx->rate_constraint_list[idx++] = index_to_rate[i];
			tfa98xx->rate_constraint.count += 1;
		}
	}
#endif /* TFA_CHECK_PROFILE_RATE */

	pr_info("%s: setting rate constraint (%d)\n", __func__, idx);

	return snd_pcm_hw_constraint_list(substream->runtime, 0,
		SNDRV_PCM_HW_PARAM_RATE,
		&tfa98xx->rate_constraint);
#else
	pr_info("%s: add all the rates in the list\n", __func__);
	for (idx = 0; idx < (int)ARRAY_SIZE(rate_to_fssel); idx++) {
		tfa98xx->rate_constraint_list[idx] = rate_to_fssel[idx].rate;
		tfa98xx->rate_constraint.count += 1;
	}

	pr_info("%s: skip setting rate constraint, assuming fixed format\n",
		__func__);

	return 0;
#endif /* TFA_NO_SND_FORMAT_CHECK */
}

static int tfa98xx_set_dai_sysclk(struct snd_soc_dai *codec_dai,
	int clk_id, unsigned int freq, int dir)
{
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	struct tfa98xx *tfa98xx
		= snd_soc_component_get_drvdata(codec_dai->component);
#else
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec_dai->codec);
#endif

	tfa98xx->sysclk = freq;
	return 0;
}

static int tfa98xx_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
	unsigned int rx_mask, int slots, int slot_width)
{
	pr_debug("\n");
	return 0;
}

static int tfa98xx_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	struct tfa98xx *tfa98xx
		= snd_soc_component_get_drvdata(dai->component);
	struct snd_soc_component *component = dai->component;
#else
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(dai->codec);
	struct snd_soc_codec *codec = dai->codec;
#endif
	struct device *cdev;

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	cdev = component->dev;
#else
	cdev = codec->dev;
#endif

	pr_debug("fmt=0x%x\n", fmt);

	/* Supported mode: I2S, PDM */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_DSP_A:
		if ((fmt & SND_SOC_DAIFMT_MASTER_MASK)
			!= SND_SOC_DAIFMT_CBS_CFS) {
			dev_err(cdev, "Invalid Codec main mode\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_PDM:
		break;
	default:
		dev_err(cdev, "Unsupported DAI format %d\n",
			fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	tfa98xx->audio_mode = fmt & SND_SOC_DAIFMT_FORMAT_MASK;

	return 0;
}

int tfa98xx_get_fssel(unsigned int rate)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(rate_to_fssel); i++)
		if (rate_to_fssel[i].rate == rate)
			return rate_to_fssel[i].fssel;

	return -EINVAL;
}

static int tfa98xx_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	struct snd_soc_component *component = dai->component;
	struct tfa98xx *tfa98xx
		= snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	unsigned int rate;
	int prof_idx;
	int sample_size;
	int slot_size;

	/* Supported */
	rate = params_rate(params);
	sample_size = snd_pcm_format_width(params_format(params));
	slot_size = snd_pcm_format_physical_width(params_format(params));
	pr_info("%s: requested rate: %d, sample size: %d, physical size: %d\n",
		__func__, rate, sample_size, slot_size);

	if (no_start != 0)
		return 0;

#if defined(TFA_FIXED_RATE_FOR_DSP)
	pr_info("%s: forced to change rate: %d to %d\n",
		__func__, rate, sr_converted);
	rate = sr_converted;
#endif

#if defined(TFA_CHANGE_PCM_FORMAT)
	/* Assume signed / little endian by default */
	/*
	 * For channels, use 2 in stereo case (compress)
	 * but use 2 x # of devices for V/I in other cases
	 * - 2 for mono only, currently
	 * - 1 for mono, which needs to check how it differs
	 * - 4 for 4-way speaker
	 * - undefined for 3 devices
	 */
	if (pcm_sample_format == 3) { /* dynamic */
		pr_info("%s: set dynamic rate: %d, sample size: %d, physical size: %d\n",
			__func__, rate, sample_size, slot_size);
		tfa98xx->hw_rate = rate;
		tfa98xx->sample_size = sample_size;
		tfa98xx->slot_size = slot_size;
	}
#endif

	/* check if samplerate is supported for this mixer profile */
	prof_idx = get_profile_id_for_sr(tfa98xx_mixer_profile, rate);
	if (prof_idx < 0) {
		pr_err("tfa98xx: invalid sample rate %d.\n", rate);
		return -EINVAL;
	}
	pr_debug("mixer profile:container profile = [%d:%d]\n",
		tfa98xx_mixer_profile, prof_idx);

	/* update 'real' profile (container profile) */
	tfa98xx->profile = prof_idx;

	pr_info("%s: tfa98xx_profile %d\n", __func__, tfa98xx->profile);

	/* update to new rate */
	tfa98xx->rate = rate;

	return 0;
}

static int tfa98xx_mute(struct snd_soc_dai *dai, int mute, int stream)
{
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	struct snd_soc_component *component = dai->component;
	struct tfa98xx *tfa98xx
		= snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif

	dev_dbg(tfa98xx->dev, "%s: state: %d (stream = %d)\n",
		__func__, mute, stream);

	if (no_start) {
		pr_debug("no_start parameter set no tfa_dev_start or tfa_dev_stop, returning\n");
		return 0;
	}

	_tfa98xx_mute(tfa98xx, mute, stream);

	return 0;
}

static int _tfa98xx_mute(struct tfa98xx *tfa98xx, int mute, int stream)
{
	if (mute) {
		/* stop DSP only when both playback and capture streams
		 * are deactivated
		 */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (tfa98xx->pstream == 0) {
				pr_debug("mute:%d [pstream duplicated]\n",
					mute);
				return 0;
			}
			tfa98xx->pstream = 0;
		} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (tfa98xx->cstream == 0) {
				pr_debug("mute:%d [cstream duplicated]\n",
					mute);
				return 0;
			}
			tfa98xx->cstream = 0;
		}
		mutex_lock(&tfa98xx->dsp_lock);
		pr_info("mute:%d [pstream %d, cstream %d, samstream %d]\n",
			mute,
			tfa98xx->pstream, tfa98xx->cstream, tfa98xx->samstream);

#if defined(TFA_BLACKBOX_LOGGING)
		if (tfa98xx_count_active_stream(BIT_PSTREAM)
			== tfa98xx_device_count) /* at first device */
			if (tfa98xx->tfa->blackbox_enable
				&& !tfa98xx->tfa->unset_log) {
				/* get logging once
				 * before shutting down pstream
				 */
				pr_info("%s: get blackbox logging\n", __func__);
				tfa_update_log();
			}
		tfa98xx->tfa->unset_log = 0;
#endif /* TFA_BLACKBOX_LOGGING */

		tfa98xx_set_stream_state(tfa98xx->tfa,
			(tfa98xx->pstream & BIT_PSTREAM)
			|((tfa98xx->cstream<<1) & BIT_CSTREAM)
			|((tfa98xx->samstream<<2) & BIT_SAMSTREAM));
		mutex_unlock(&tfa98xx->dsp_lock);

		/* case: both p/cstream (either) and samstream are off
		 * if (!(tfa98xx->pstream == 0 || tfa98xx->cstream == 0)
		 *  || (tfa98xx->samstream != 0)) {
		 *  pr_info("mute is suspended until playback/saam are off\n");
		 *  return 0;
		 * }
		 */
		/* wait until both main streams (pstream / samstream) are off */
		if ((tfa98xx->pstream == 0)
			&& (tfa98xx->samstream == 0)) {
			pr_info("mute is triggered\n");
		} else {
			pr_info("mute is suspended when only cstream is off\n");
			return 0;
		}

		mutex_lock(&tfa98xx_mutex);
		tfa98xx_sync_count = 0;
		mutex_unlock(&tfa98xx_mutex);

		cancel_delayed_work_sync(&tfa98xx->monitor_work);
#if !defined(TFA_USE_DIRECT_API_CALL)
		cancel_delayed_work_sync(&tfa98xx->init_work);
#endif
		_tfa98xx_stop(tfa98xx);
	} else {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			tfa98xx->pstream = 1;
		else if (stream == SNDRV_PCM_STREAM_CAPTURE)
			tfa98xx->cstream = 1;
		mutex_lock(&tfa98xx->dsp_lock);
		pr_info("mute:%d [pstream %d, cstream %d, samstream %d]\n",
			mute,
			tfa98xx->pstream, tfa98xx->cstream, tfa98xx->samstream);
		tfa98xx_set_stream_state(tfa98xx->tfa,
			(tfa98xx->pstream & BIT_PSTREAM)
			|((tfa98xx->cstream<<1) & BIT_CSTREAM)
			|((tfa98xx->samstream<<2) & BIT_SAMSTREAM));
		mutex_unlock(&tfa98xx->dsp_lock);

		/* case: either p/cstream (both) or samstream is on
		 * if ((tfa98xx->pstream != 0 && tfa98xx->cstream != 0)
		 *  || tfa98xx->samstream != 0) {
		 */
		/* wait until DSP is ready for initialization */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK
			|| stream == SNDRV_PCM_STREAM_SAAM) {
			pr_info("unmute is triggered\n");
		} else {
			pr_info("unmute is suspended when only cstream is on\n");
			return 0;
		}

		pr_debug("%s: unmute with profile %d\n",
			__func__, tfa98xx->profile);

		/* check index if it needs dsp init only for main device */

		/* Start DSP */
		pr_info("%s: start tfa amp\n", __func__);
#if !defined(TFA_USE_DIRECT_API_CALL)
		if (tfa98xx->dsp_init != TFA98XX_DSP_INIT_PENDING)
			queue_delayed_work(tfa98xx->tfa98xx_wq,
				&tfa98xx->init_work, 0);
#else
		pr_info("%s: dsp_init (direct) with device %d, profile %d\n",
			__func__, tfa98xx->tfa->dev_idx, tfa98xx->profile);
		tfa98xx_dsp_init(tfa98xx);
#endif
	}

	return 0;
}

static int _tfa98xx_stop(struct tfa98xx *tfa98xx)
{
	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK)
		return 0;

	mutex_lock(&tfa98xx->dsp_lock);
	tfa_dev_stop(tfa98xx->tfa);
	tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
	tfa98xx_set_dsp_configured(tfa98xx);
	mutex_unlock(&tfa98xx->dsp_lock);

	return 0;
}

static const struct snd_soc_dai_ops tfa98xx_dai_ops = {
	.startup = tfa98xx_startup,
	.set_fmt = tfa98xx_set_fmt,
	.set_sysclk = tfa98xx_set_dai_sysclk,
	.set_tdm_slot = tfa98xx_set_tdm_slot,
	.hw_params = tfa98xx_hw_params,
	.mute_stream = tfa98xx_mute,
};

static struct snd_soc_dai_driver tfa98xx_dai[] = {
	{
		.name = "tfa98xx-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = MAX_HANDLES,
			.rates = TFA98XX_RATES,
			.formats = TFA98XX_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = MAX_HANDLES,
			.rates = TFA98XX_RATES,
			.formats = TFA98XX_FORMATS,
		},
		.ops = &tfa98xx_dai_ops,
		.symmetric_rates = 1,
#if KERNEL_VERSION(3, 14, 0) <= LINUX_VERSION_CODE
		.symmetric_channels = 0,
		.symmetric_samplebits = 0,
#endif
	},
};

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
static int tfa98xx_probe(struct snd_soc_component *component)
{
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
#else
static int tfa98xx_probe(struct snd_soc_codec *codec)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif
	int ret = 0;
	struct device *cdev;

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	cdev = component->dev;
#else
	cdev = codec->dev;
#endif

	pr_debug("%s:\n", __func__);

	/* setup work queue, will be used to initial DSP on first boot up */
	tfa98xx->tfa98xx_wq = create_singlethread_workqueue("tfa98xx");
	if (!tfa98xx->tfa98xx_wq)
		return -ENOMEM;

#if defined(TFA_WAIT_CAL_IN_WORKQUEUE)
	tfa98xx->tfa->tfacal_wq = create_singlethread_workqueue("tfacal");
	if (!tfa98xx->tfa->tfacal_wq)
		return -ENOMEM;
#endif

#if !defined(TFA_USE_DIRECT_API_CALL)
	INIT_DELAYED_WORK(&tfa98xx->init_work, tfa98xx_dsp_init_work);
#endif
	INIT_DELAYED_WORK(&tfa98xx->monitor_work, tfa98xx_monitor);
	INIT_DELAYED_WORK(&tfa98xx->interrupt_work, tfa98xx_interrupt);
#if (defined(USE_TFA9891) || defined(USE_TFA9912))
	INIT_DELAYED_WORK(&tfa98xx->tapdet_work, tfa98xx_tapdet_work);
#endif
#if defined(TFA_USE_WAITQUEUE_SEQ)
	init_waitqueue_head(&tfa98xx->tfa->waitq_seq);
#endif

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	tfa98xx->component = component;
#else
	tfa98xx->codec = codec;
#endif

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	snd_soc_component_init_regmap(component, tfa98xx->regmap);
#endif

	ret = tfa98xx_load_container(tfa98xx);
	pr_debug("Container loading requested: %d\n", ret);

#if KERNEL_VERSION(3, 16, 0) > LINUX_VERSION_CODE
	codec->control_data = tfa98xx->regmap;
	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
#endif
	tfa98xx_add_widgets(tfa98xx);

	dev_info(cdev, "tfa98xx codec registered (%s)",
		tfa98xx->fw.name);

	return ret;
}

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
static void tfa98xx_remove(struct snd_soc_component *component)
{
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
#else
static int tfa98xx_remove(struct snd_soc_codec *codec)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#endif

	pr_debug("%s:\n", __func__);

	if (tfa98xx->tfa == NULL)
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
		return;
#else
		return -ENOMEM;
#endif

	tfa98xx_interrupt_enable(tfa98xx, false);

#if (defined(USE_TFA9891) || defined(USE_TFA9912))
	tfa98xx_inputdev_unregister(tfa98xx);
#endif

	cancel_delayed_work_sync(&tfa98xx->interrupt_work);
	cancel_delayed_work_sync(&tfa98xx->monitor_work);
#if !defined(TFA_USE_DIRECT_API_CALL)
	cancel_delayed_work_sync(&tfa98xx->init_work);
#endif
#if (defined(USE_TFA9891) || defined(USE_TFA9912))
	cancel_delayed_work_sync(&tfa98xx->tapdet_work);
#endif

	if (tfa98xx->tfa98xx_wq)
		destroy_workqueue(tfa98xx->tfa98xx_wq);

#if defined(TFA_WAIT_CAL_IN_WORKQUEUE)
	if (tfa98xx->tfa->tfacal_wq)
		destroy_workqueue(tfa98xx->tfa->tfacal_wq);
#endif

#if defined(TFADSP_DSP_BUFFER_POOL)
	/* deallocate buffer_pool */
	if (tfa98xx->tfa->dev_idx == 0) {
		int index = 0;

		pr_info("Deallocate buffer_pool\n");
		for (index = 0; index < POOL_MAX_INDEX; index++)
			tfa_buffer_pool(tfa98xx->tfa,
				index, 0, POOL_FREE);
	}
#endif

#if KERNEL_VERSION(4, 18, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	/* Do nothing about REGMAP */
#elif KERNEL_VERSION(3, 16, 0) < LINUX_VERSION_CODE
static struct regmap *tfa98xx_get_regmap(struct device *dev)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);

	return tfa98xx->regmap;
}
#endif

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
static const struct snd_soc_component_driver soc_component_dev_tfa98xx = {
#else
static const struct snd_soc_codec_driver soc_codec_dev_tfa98xx = {
#endif
	.probe = tfa98xx_probe,
	.remove = tfa98xx_remove,
#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	/* Do nothing about REGMAP */
#elif KERNEL_VERSION(3, 16, 0) < LINUX_VERSION_CODE
	.get_regmap = tfa98xx_get_regmap,
#endif
};

static bool tfa98xx_writeable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa98xx_readable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa98xx_volatile_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static const struct regmap_config tfa98xx_regmap = {
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = TFA98XX_MAX_REGISTER,
	.writeable_reg = tfa98xx_writeable_register,
	.readable_reg = tfa98xx_readable_register,
	.volatile_reg = tfa98xx_volatile_register,
	.cache_type = REGCACHE_NONE,
};

static void tfa98xx_irq_tfa2(struct tfa98xx *tfa98xx)
{
	pr_info("%s:\n", __func__);

	/*
	 * mask interrupts
	 * will be unmasked after handling interrupts in workqueue
	 */
	tfa_irq_mask(tfa98xx->tfa);
	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->interrupt_work, 0);
}


static irqreturn_t tfa98xx_irq(int irq, void *data)
{
	struct tfa98xx *tfa98xx = data;

	if (tfa98xx->tfa->tfa_family == 2)
		tfa98xx_irq_tfa2(tfa98xx);

	return IRQ_HANDLED;
}

static int tfa98xx_ext_reset(struct tfa98xx *tfa98xx)
{
	if (tfa98xx && gpio_is_valid(tfa98xx->reset_gpio)) {
		int reset = tfa98xx->reset_polarity;

		gpio_set_value_cansleep(tfa98xx->reset_gpio, reset);
		msleep(TFA_RESET_DELAY);
		gpio_set_value_cansleep(tfa98xx->reset_gpio, !reset);
		msleep(TFA_RESET_DELAY);
	}

	return 0;
}

static int tfa98xx_parse_dt(struct device *dev,
	struct tfa98xx *tfa98xx, struct device_node *np)
{
	u32 value;
	int ret;

	tfa98xx->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (tfa98xx->reset_gpio < 0)
		dev_dbg(dev, "No reset GPIO provided, will not HW reset device\n");

	tfa98xx->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (tfa98xx->irq_gpio < 0)
		dev_dbg(dev, "No IRQ GPIO provided.\n");

	ret = of_property_read_u32(np, "reset-polarity", &value);
	if (ret < 0) /* when it fails to read from device tree */
#if defined(USE_TFA9878)
		tfa98xx->reset_polarity = LOW; /* RSTN */
#else
		tfa98xx->reset_polarity = HIGH; /* RST */
#endif
	else
		tfa98xx->reset_polarity = (value == 0) ? LOW : HIGH;

	dev_info(dev, "reset-polarity:%d\n", tfa98xx->reset_polarity);

#if defined(TFA_SET_IPC_PORT_ID)
	tfa98xx->pid = -1;

	ret = of_property_read_u32(np, "port_id", &value);
	if (ret < 0) /* when it fails to read from device tree */
		dev_err(dev, "failed to read port_id from DT\n");
	else
		tfa98xx->pid = (int)value;

	dev_info(dev, "port_id: %d\n", tfa98xx->pid);
#endif

	return 0;
}

#if defined(TFA_LIMIT_CAL_FROM_DTS)
static int tfa98xx_parse_limit_cal_dt(struct device *dev,
	struct tfa98xx *tfa98xx, struct device_node *np)
{
	u32 value;
	int err_lower, err_upper;

	err_lower = of_property_read_u32(np, "lower-limit-cal", &value);
	if ((err_lower < 0)
		|| (value < MIN_CALIBRATION_DATA))
		tfa98xx->tfa->lower_limit_cal = MIN_CALIBRATION_DATA;
	else
		tfa98xx->tfa->lower_limit_cal = value;
	pr_info("[0x%x] lower limit cal : %d\n",
		tfa98xx->i2c->addr, tfa98xx->tfa->lower_limit_cal);

	err_upper = of_property_read_u32(np, "upper-limit-cal", &value);
	if ((err_upper < 0)
		|| (value > MAX_CALIBRATION_DATA))
		tfa98xx->tfa->upper_limit_cal = MAX_CALIBRATION_DATA;
	else
		tfa98xx->tfa->upper_limit_cal = value;
	pr_info("[0x%x] upper limit cal : %d\n",
		tfa98xx->i2c->addr, tfa98xx->tfa->upper_limit_cal);

	return ((err_lower == 0 && err_upper == 0) ? 0 : -1);
}
#endif /* TFA_LIMIT_CAL_FROM_DTS */

#if defined(TFA_USE_DUMMY_CAL)
static int tfa98xx_parse_dummy_cal_dt(struct device *dev,
	struct tfa98xx *tfa98xx, struct device_node *np)
{
	u32 value;
	int err;

	err = of_property_read_u32(np, "dummy-cal", &value);
	if (err < 0) {
		tfa98xx->tfa->dummy_cal = DUMMY_CALIBRATION_DATA;
		return -1;
	}

	if (value < MIN_CALIBRATION_DATA)
		tfa98xx->tfa->dummy_cal = MIN_CALIBRATION_DATA;
	else if (value > MAX_CALIBRATION_DATA)
		tfa98xx->tfa->dummy_cal = MAX_CALIBRATION_DATA;
	else
		tfa98xx->tfa->dummy_cal = value;
	pr_info("[0x%x] dummy cal : %d\n",
		tfa98xx->i2c->addr, tfa98xx->tfa->dummy_cal);

	return 0;
}
#endif /* TFA_USE_DUMMY_CAL */

static ssize_t tfa98xx_reg_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);

	if (count != 1) {
		pr_debug("invalid register address");
		return -EINVAL;
	}

	pr_info("i2c set reg: 0x%x\n", tfa98xx->reg);

	tfa98xx->reg = buf[0];

	return 1;
}

static ssize_t tfa98xx_rw_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	size_t write_count = (count > 2) ? 2 : count; /* 16-bit */
	u8 *data;
	int ret = 0;
	int retries = I2C_RETRIES;

	data = kmalloc(write_count + 1, GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		pr_debug("can not allocate memory\n");
		return ret;
	}

	data[0] = tfa98xx->reg;
	memcpy(&data[1], buf, write_count);

	pr_debug("i2c rw write: 0x%x (offset %d, write_count %d, count %d)\n",
		tfa98xx->reg, (int)off, (int)write_count, (int)count);
retry:
	ret = i2c_master_send(tfa98xx->i2c, data, write_count + 1);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	kfree(data);

	/* the number of data bytes written without the register address */
	return ((ret > 1) ? count : -EIO);
}

static ssize_t tfa98xx_rw_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	size_t read_count = (count > 2) ? 2 : count; /* 16-bit */
	struct i2c_msg msgs[] = {
		{
			.addr = tfa98xx->i2c->addr,
			.flags = 0,
			.len = 1,
			.buf = &tfa98xx->reg,
		},
		{
			.addr = tfa98xx->i2c->addr,
			.flags = I2C_M_RD,
			.len = read_count,
			.buf = buf,
		},
	};
	int ret;
	int retries = I2C_RETRIES;

	if (count >= PAGE_SIZE) {
		pr_info("%s: blocked anonymous read!\n", __func__);
		return 0;
	}

	pr_debug("i2c rw read: 0x%x (offset %d, read_count %d, count %d)\n",
		tfa98xx->reg, (int)off, (int)read_count, (int)count);

	memset(buf, 0, count);
retry:
	ret = i2c_transfer(tfa98xx->i2c->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return ret;
	}

	/* ret contains the number of i2c transaction */
	/* return the number of bytes read */
	return ((ret > 1) ? count : 0);
}

#ifdef CALFUNC_IN_SYSFS
static ssize_t tfa98xx_calibrate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int mtp = 0, mtpex = 0;
	int count = 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

#if defined(TFA_FS_CHECK_MTPEX)
	tfa98xx_check_calibration(tfa98xx);
#endif

	if (tfa98xx->calibrate_done) {
		pr_info("[0x%x] Calibration Success\n", tfa98xx->i2c->addr);
		count = snprintf(buf, PAGE_SIZE, "[0x%02x] Success\n",
			tfa98xx->i2c->addr);
	} else {
		pr_info("[0x%x] Calibration Fail\n", tfa98xx->i2c->addr);
		count = snprintf(buf, PAGE_SIZE, "[0x%02x] Fail\n",
			tfa98xx->i2c->addr);
	}

	mtp = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_RE25);
	mtpex = TFA_GET_BF(tfa98xx->tfa, MTPEX);

	pr_info("[0x%x] MTPEX: %d, MTP: %d mOhm\n",
		tfa98xx->i2c->addr, mtpex, mtp);

	return count;
}

static ssize_t tfa98xx_calibrate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int ret = 0;
	static const char ref[] = "1"; /* "please calibrate now" */

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	/* check string length, and account for eol */
	if (count > sizeof(ref) + 1 || count < (sizeof(ref) - 1))
		return -EINVAL;

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (strncmp(buf, ref, sizeof(ref) - 1)) {
		pr_info("[0x%x] %s: calibration is triggered with %s!\n",
			tfa98xx->i2c->addr, __func__, buf);
		return -EINVAL;
	}

	ret = tfa98xx_run_calibration(tfa98xx);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t tfa98xx_mtpex_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int count = 0, value;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_EX);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value < 0) {
		pr_err("[0x%x] Unable to access MTPEX: %d\n",
			tfa98xx->i2c->addr, value);
		return -EIO;
	}

	pr_debug("[0x%x] MTPEX : %d\n", tfa98xx->i2c->addr, value);
	count = snprintf(buf, PAGE_SIZE, "[0x%02x] MTPEX %d\n",
		tfa98xx->i2c->addr, value);

	return count;
}

static ssize_t tfa98xx_mtpex_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	enum tfa_error err;
	static const char ref[] = "0"; /* "please calibrate now" */
#if defined(TFA_READ_REFERENCE_TEMP)
	enum tfa98xx_error ret;
	u16 temp_val = DEFAULT_REF_TEMP;
	int idx, ndev;
	struct tfa_device *ntfa = NULL;
#endif

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	/* check string length, and account for eol */
	if (count > sizeof(ref) + 1 || count < (sizeof(ref) - 1))
		return -EINVAL;

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (strncmp(buf, ref, sizeof(ref) - 1)) {
		pr_info("[0x%x] Can only clear MTPEX (0 value expected)\n",
			tfa98xx->i2c->addr);
		return -EINVAL;
	}

#if defined(TFA_READ_REFERENCE_TEMP)
	/* EXT_TEMP */
	ret = tfa98xx_read_reference_temp(&temp_val);
	if (ret)
		pr_err("error in reading reference temp\n");

	ndev = tfa98xx->tfa->dev_count;
	for (idx = 0; idx < ndev; idx++) {
		ntfa = tfa98xx_get_tfa_device_from_channel(idx);
		if (ntfa == NULL)
			continue;
		tfa98xx_set_exttemp(ntfa, (short)temp_val);
	}
#endif

	mutex_lock(&tfa98xx->dsp_lock);
	err = tfa_dev_mtp_set(tfa98xx->tfa, TFA_MTP_EX, 0);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (err != tfa_error_ok) {
		pr_err("[0x%x] Unable to access MTPEX: err %d (suspended)\n",
			tfa98xx->i2c->addr, err);
		/* suspend until TFA98xx is active */
		tfa98xx->tfa->reset_mtpex = 1;
		return -EIO;
	}

	pr_info("[0x%x] MTPEX < 0\n", tfa98xx->i2c->addr);

	return count;
}

static ssize_t tfa98xx_re25_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int count = 0, mtpex, value;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	mtpex = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_EX);
	if (mtpex)
		value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_RE25);
	else
		value = 0;
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value < 0) {
		pr_err("[0x%x] Unable to access MTP RE25: %d\n",
			tfa98xx->i2c->addr, value);
		return -EIO;
	}

	pr_debug("[0x%x] MTP : %d\n", tfa98xx->i2c->addr, value);
	count = snprintf(buf, PAGE_SIZE, "[0x%02x] Calibration data %d mOhm\n",
		tfa98xx->i2c->addr, value);

	return count;
}

static ssize_t tfa98xx_re25_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* to prevent attack to tfa98xx_re25_store */
	return count;
}
#endif /* CALFUNC_IN_SYSFS */

#ifdef LOGFUNC_IN_SYSFS
static ssize_t tfa98xx_blackbox_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int count = 0;
#if defined(TFA_BLACKBOX_LOGGING)
	int idx, ndev, offset, addr;
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_device *tfa0 = NULL;
	struct tfa_device *ntfa = NULL;
#endif

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

#if defined(TFA_BLACKBOX_LOGGING)
	ndev = tfa98xx->tfa->dev_count;
	if (ndev < 1)
		return -EINVAL;

	/* select main device */
	tfa0 = tfa98xx_get_tfa_device_from_index(0);

	/* update current session if it's active */
	if (tfa98xx_count_active_stream(BIT_PSTREAM) > 0)
		err = tfa_update_log();

	pr_info("blackbox state: %d\n",
		tfa0->blackbox_enable);

	for (idx = 0; idx < ndev; idx++) {
		offset = idx * ID_BLACKBOX_MAX;
		ntfa = tfa98xx_get_tfa_device_from_channel(idx);
		if (ntfa == NULL)
			continue;
		addr = ntfa->resp_address;
		count = snprintf(buf + strlen(buf), PAGE_SIZE,
			"[0x%02x] maxX %d um, maxT %d degC, countXmax %d, countTmax %d, maxX_keep %d um, maxT_keep %d degC\n",
			addr,
			tfa0->log_data[offset + ID_MAXX_LOG],
			tfa0->log_data[offset + ID_MAXT_LOG],
			tfa0->log_data[offset + ID_OVERXMAX_COUNT],
			tfa0->log_data[offset + ID_OVERTMAX_COUNT],
			tfa0->log_data[offset + ID_MAXX_KEEP_LOG],
			tfa0->log_data[offset + ID_MAXT_KEEP_LOG]);

		/* skip resetting; reset only by registered callback
		 * memset(tfa0->log_data + offset,
		 *  0, sizeof(int) * TFA_LOG_MAX_COUNT);
		 */
	}
#else
	pr_info("%s: blackbox is not implemented\n", __func__);
#endif

	return count;
}

static ssize_t tfa98xx_blackbox_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int enable = 0;
#if defined(TFA_BLACKBOX_LOGGING)
	int ret = 0;
#endif

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	/* check string length, and account for eol */
	if (count < 1)
		return -EINVAL;

	if (!strncmp(buf, "1", 1))
		enable = 1;
	else if (!strncmp(buf, "0", 1))
		enable = 0;
	else {
		pr_info("%s: blackbox is triggered with %s!\n", __func__, buf);
		return -EINVAL;
	}

#if defined(TFA_BLACKBOX_LOGGING)
	pr_info("%s: blackbox < %d\n", __func__, enable);
	ret = tfa_set_blackbox(enable);

	if (tfa98xx->tfa->is_configured > 0) {
		pr_info("%s: set blackbox directly\n", __func__);
		tfa98xx->tfa->individual_msg = 1;
		ret = tfa_configure_log(enable);
		if (ret < 0)
			return ret;
	}
#else
	pr_info("%s: blackbox is not implemented\n", __func__);
#endif

	return count;
}
#endif /* LOGFUNC_IN_SYSFS */

#ifdef GAINFUNC_IN_SYSFS
static ssize_t tfa98xx_gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int count = 0;
	int spkgain, ampgain;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	spkgain = TFA7x_GET_BF(tfa98xx->tfa, TDMSPKG);
	ampgain = TFA7x_GET_BF(tfa98xx->tfa, AMPGAIN);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (spkgain < 0 || ampgain < 0) {
		pr_err("[0x%x] Unable to access TDMSPKG / AMPGAIN: (%d, %d)\n",
			tfa98xx->i2c->addr, spkgain, ampgain);
		return -EIO;
	}

	pr_debug("[0x%x] TDMSPKG : %d, AMPGAIN : %d\n",
		tfa98xx->i2c->addr, spkgain, ampgain);
	count = snprintf(buf, PAGE_SIZE, "[0x%02x] TDMSPKG %d, AMAPGAIN %d\n",
		tfa98xx->i2c->addr, spkgain, ampgain);

	return count;
}

static ssize_t tfa98xx_gain_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* to prevent attack to tfa98xx_re25_store */
	return count;
}
#endif /* GAINFUNC_IN_SYSFS */

#ifdef TFA_DISABLE_AUTO_CAL
static ssize_t tfa98xx_autocal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct tfa_device *tfa = NULL;
	int count = 0;

	tfa = tfa98xx->tfa;
	if (!tfa)
		return -ENODEV;
	if (tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	pr_debug("[0x%x] autocal : %s\n",
		tfa98xx->i2c->addr,
		(tfa->disable_auto_cal) ? "disabled" : "enabled");
	count = snprintf(buf, PAGE_SIZE, "%s\n",
		(tfa->disable_auto_cal) ? "0 (disabled)" : "1 (enabled)");

	return count;
}

static ssize_t tfa98xx_autocal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct tfa_device *tfa = NULL;
	int enable = 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	/* check string length, and account for eol */
	if (count < 1)
		return -EINVAL;

	if (!strncmp(buf, "1", 1))
		enable = 1;
	else if (!strncmp(buf, "0", 1))
		enable = 0;
	else {
		pr_info("%s: autocal is triggered with %s!\n", __func__, buf);
		return -EINVAL;
	}

	pr_info("%s: autocal < %d\n", __func__, enable);

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		tfa->disable_auto_cal = (enable) ? 0 : 1;
	}
	mutex_unlock(&tfa98xx_mutex);

	return count;
}
#endif /* TFA_DISABLE_AUTO_CAL */

static struct bin_attribute dev_attr_rw = {
	.attr = {
		.name = "rw",
		.mode = 0600,
	},
	.size = 0,
	.read = tfa98xx_rw_read,
	.write = tfa98xx_rw_write,
};

static struct bin_attribute dev_attr_reg = {
	.attr = {
		.name = "reg",
		.mode = 0200,
	},
	.size = 0,
	.read = NULL,
	.write = tfa98xx_reg_write,
};

#ifdef CALFUNC_IN_SYSFS
static struct device_attribute dev_attr_calibrate = {
	.attr = {
		.name = "calibrate",
		.mode = 0600,
	},
	.show = tfa98xx_calibrate_show,
	.store = tfa98xx_calibrate_store,
};

static struct device_attribute dev_attr_mtpex = {
	.attr = {
		.name = "MTPEX",
		.mode = 0600,
	},
	.show = tfa98xx_mtpex_show,
	.store = tfa98xx_mtpex_store,
};

static struct device_attribute dev_attr_re25 = {
	.attr = {
		.name = "re25",
		.mode = 0600,
	},
	.show = tfa98xx_re25_show,
	.store = tfa98xx_re25_store,
};
#endif /* CALFUNC_IN_SYSFS */

#ifdef LOGFUNC_IN_SYSFS
static struct device_attribute dev_attr_blackbox = {
	.attr = {
		.name = "log",
		.mode = 0600,
	},
	.show = tfa98xx_blackbox_show,
	.store = tfa98xx_blackbox_store,
};
#endif /* LOGFUNC_IN_SYSFS */

#ifdef GAINFUNC_IN_SYSFS
static struct device_attribute dev_attr_gain = {
	.attr = {
		.name = "gain",
		.mode = 0600,
	},
	.show = tfa98xx_gain_show,
	.store = tfa98xx_gain_store,
};
#endif /* GAINFUNC_IN_SYSFS */

#if defined(TFA_DISABLE_AUTO_CAL)
static struct device_attribute dev_attr_autocal = {
	.attr = {
		.name = "autocal",
		.mode = 0600,
	},
	.show = tfa98xx_autocal_show,
	.store = tfa98xx_autocal_store,
};
#endif /* TFA_DISABLE_AUTO_CAL */

struct tfa_device *tfa98xx_get_tfa_device_from_index(int index)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *ntfa = NULL;

	if (index < 0)
		return NULL;

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		if (tfa98xx->tfa->dev_idx == index) {
			ntfa = tfa98xx->tfa;
			break;
		}
	}

	return ntfa;
}
EXPORT_SYMBOL(tfa98xx_get_tfa_device_from_index);

struct tfa_device *tfa98xx_get_tfa_device_from_channel(int channel)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *ntfa = NULL;
	int nchannel = -1;

	if (channel < 0)
		return NULL;

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		nchannel = tfa98xx_get_cnt_bitfield(tfa98xx->tfa,
			TFA7x_FAM(tfa98xx->tfa, TDMSPKS));
		if (nchannel == channel) {
			ntfa = tfa98xx->tfa;
			break;
		}
	}

	return ntfa;
}
EXPORT_SYMBOL(tfa98xx_get_tfa_device_from_channel);

int tfa98xx_count_active_stream(int stream_flag)
{
	struct tfa98xx *tfa98xx;
	int stream_counter = 0;

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		if (tfa98xx->tfa->stream_state & stream_flag)
			stream_counter++;
	}

	return stream_counter;
}

#if defined(TFA_USE_TFACAL_NODE)
enum tfa98xx_error tfa_run_cal(int index, uint16_t *value)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(index);
	struct tfa98xx *tfa98xx;
	int ret = 0;
	int mtpex = 0;
#if defined(TFA_WAIT_CAL_IN_WORKQUEUE)
	int tries = 0;
#endif

	if (!tfa)
		return TFA98XX_ERROR_NOT_OPEN;

	tfa98xx = (struct tfa98xx *)tfa->data;

	/* check if calibration already runs */
	tfa_wait_until_calibration_done(tfa);

	ret = tfa98xx_run_calibration(tfa98xx);
	if (ret < 0)
		return TFA98XX_ERROR_FAIL;

#if defined(TFA_WAIT_CAL_IN_WORKQUEUE)
	tfa_wait_until_calibration_done(tfa);
#endif /* TFA_WAIT_CAL_IN_WORKQUEUE */

	if (value == NULL)
		return TFA98XX_ERROR_BAD_PARAMETER;

#if defined(TFA_WAIT_CAL_IN_WORKQUEUE)
	while (tries < TFA98XX_API_REWRTIE_MTP_NTRIES) {
		msleep_interruptible(CAL_STATUS_INTERVAL);
		mtpex = TFA_GET_BF(tfa, MTPEX);
		if (mtpex != 0) {
			msleep_interruptible(CAL_STATUS_INTERVAL);
			break;
		}
		tries++;
	}
#endif /* TFA_WAIT_CAL_IN_WORKQUEUE */
	mtpex = TFA_GET_BF(tfa, MTPEX);
	if (mtpex != 1)
		return TFA98XX_ERROR_FAIL;

	*value = tfa_dev_mtp_get(tfa, TFA_MTP_RE25);
	if ((int)*value < 0) {
		pr_info("%s: calibration data is not valid\n",
			__func__);
		*value = 0xffff;
		tfa->temp = 0xffff;
		return TFA98XX_ERROR_FAIL;
	}

	return TFA98XX_ERROR_OK;
}
EXPORT_SYMBOL(tfa_run_cal);

enum tfa98xx_error tfa_get_cal_data(int index, uint16_t *value)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(index);
	int mtp = 0, mtpex = 0;

	if (!tfa)
		return TFA98XX_ERROR_NOT_OPEN;

	mtp = tfa_dev_mtp_get(tfa, TFA_MTP_RE25);
	mtpex = TFA_GET_BF(tfa, MTPEX);

	if (value == NULL)
		return TFA98XX_ERROR_BAD_PARAMETER;
	if (mtpex != 1)
		return TFA98XX_ERROR_FAIL;

	*value = mtp;
	if ((int)*value < 0) {
		pr_info("%s: calibration data is not valid\n",
			__func__);
		*value = 0xffff;
		tfa->temp = 0xffff;
		return TFA98XX_ERROR_FAIL;
	}

	return TFA98XX_ERROR_OK;
}
EXPORT_SYMBOL(tfa_get_cal_data);

enum tfa98xx_error tfa_get_cal_temp(int index, uint16_t *value)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(index);
	int mtpex = 0;

	if (!tfa)
		return TFA98XX_ERROR_NOT_OPEN;

	mtpex = TFA_GET_BF(tfa, MTPEX);

	if (value == NULL)
		return TFA98XX_ERROR_BAD_PARAMETER;
	if (mtpex != 1)
		return TFA98XX_ERROR_FAIL;

	*value = tfa->temp;
	if (*value == 0xffff) {
		pr_info("%s: calibration temperature is not valid\n",
			__func__);
		*value = tfa98xx_get_exttemp(tfa);
		pr_info("%s: calibration temperature is not valid\n",
			__func__);
		return TFA98XX_ERROR_FAIL;
	}

	return TFA98XX_ERROR_OK;
}
EXPORT_SYMBOL(tfa_get_cal_temp);
#endif /* TFA_USE_TFACAL_NODE */

int tfa98xx_set_blackbox(int enable)
{
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(0);

	if (!tfa)
		return TFA98XX_ERROR_NOT_OPEN;
	if (tfa->tfa_family == 0)
		return TFA98XX_ERROR_NOT_OPEN;

#if defined(TFA_BLACKBOX_LOGGING)
	pr_info("%s: blackbox < %d\n", __func__, enable);
	ret = tfa_set_blackbox(enable);

	if (tfa->is_configured > 0) {
		pr_info("%s: set blackbox directly\n", __func__);
		tfa->individual_msg = 1;
		ret = tfa_configure_log(enable);
	}
#else
	pr_info("%s: blackbox is not implemented\n", __func__);
#endif /* TFA_BLACKBOX_LOGGING */

	return ret;
}
#if defined(TFA_BLACKBOX_LOGGING)
EXPORT_SYMBOL(tfa98xx_set_blackbox);
#endif

int tfa98xx_get_blackbox_data(int dev, int *data)
{
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(0);
	int ndev = 0;
#if defined(TFA_BLACKBOX_LOGGING)
	int offset;
#endif

	if (!tfa)
		return TFA98XX_ERROR_NOT_OPEN;
	if (tfa->tfa_family == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	ndev = tfa->dev_count;
	if (ndev < 1)
		return TFA98XX_ERROR_NOT_OPEN;
	if (dev < 0 || dev >= ndev)
		return TFA98XX_ERROR_BAD_PARAMETER;

#if defined(TFA_BLACKBOX_LOGGING)
	pr_info("%s: blackbox state: %d\n",
		__func__, tfa->blackbox_enable);
	if (tfa->blackbox_enable == 0) {
		pr_info("%s: blackbox disabled - no update\n",
			__func__);
		return ret;
	}

	/* update current session if it's active */
	if (tfa98xx_count_active_stream(BIT_PSTREAM) > 0
		&& tfa->is_configured > 0) {
		ret = tfa_update_log();
		if (ret != TFA98XX_ERROR_OK)
			pr_info("%s: failure in updating current data\n",
				__func__);
	}

	offset = dev * ID_BLACKBOX_MAX;
	memcpy(data, tfa->log_data + offset,
		sizeof(int) * ID_BLACKBOX_MAX);

	/* reset after reading, except MAXX/T_KEEP_LOG */
	memset(tfa->log_data + offset, 0,
		sizeof(int) * TFA_LOG_MAX_COUNT);
#else
	pr_info("%s: blackbox is not implemented\n", __func__);
#endif /* TFA_BLACKBOX_LOGGING */

	return ret;
}
#if defined(TFA_BLACKBOX_LOGGING)
EXPORT_SYMBOL(tfa98xx_get_blackbox_data);
#endif

int tfa98xx_get_blackbox_data_index(int dev, int index, int reset)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(0);
	int ndev = 0;
#if defined(TFA_BLACKBOX_LOGGING)
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;
	int offset;
#endif
	int value = 0;

	if (!tfa)
		return -ENODEV;
	if (tfa->tfa_family == 0)
		return -ENODEV;

	if (index < 0 || index >= ID_BLACKBOX_MAX)
		return -EINVAL;

	ndev = tfa->dev_count;
	if (ndev < 1)
		return -EINVAL;
	if (dev < 0 || dev >= ndev)
		return -EINVAL;

#if defined(TFA_BLACKBOX_LOGGING)
	pr_info("%s: blackbox state: %d\n",
		__func__, tfa->blackbox_enable);
	if (tfa->blackbox_enable == 0) {
		pr_info("%s: blackbox disabled - no update\n",
			__func__);
		return -ENODEV;
	}

	/* update current session if it's active */
	if (tfa98xx_count_active_stream(BIT_PSTREAM) > 0
		&& tfa->is_configured > 0) {
		ret = tfa_update_log();
		if (ret != TFA98XX_ERROR_OK)
			pr_info("%s: failure in updating current data\n",
				__func__);
	}

	offset = dev * ID_BLACKBOX_MAX;
	value = tfa->log_data[offset + index];

	/* reset after reading */
	if (reset && (index < TFA_LOG_MAX_COUNT))
		tfa->log_data[offset + index] = 0;
#else
	pr_info("%s: blackbox is not implemented\n", __func__);
#endif /* TFA_BLACKBOX_LOGGING */

	return value;
}
#if defined(TFA_BLACKBOX_LOGGING)
EXPORT_SYMBOL(tfa98xx_get_blackbox_data_index);
#endif

#if defined(TFA_USE_TFAVVAL_NODE)
enum tfa98xx_error tfa_run_vval(int index, uint16_t *value)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(index);
	struct tfa_device *ntfa;
	struct tfa98xx *tfa98xx;
	int idx, ndev, ret = 0;
	int test_done = 0;

	if (!tfa)
		return TFA98XX_ERROR_NOT_OPEN;

	tfa98xx = (struct tfa98xx *)tfa->data;

	pr_info("%s: begin\n", __func__);

	ndev = tfa->dev_count;
	for (idx = 0; idx < ndev; idx++) {
		ntfa = tfa98xx_get_tfa_device_from_index(idx);
		if (ntfa == NULL)
			continue;

		/* set V validation */
		ntfa->vval_active = 1;
		ntfa->vval_result = VVAL_UNTESTED;
	}

	/* check if calibration already runs */
	test_done = tfa_wait_until_calibration_done(tfa);

	if (!test_done) {
		ret = tfa98xx_run_calibration(tfa98xx);
		if (ret < 0) {
			pr_info("%s: V validation failed\n",
				__func__);
			return TFA98XX_ERROR_FAIL;
		}

#if defined(TFA_WAIT_CAL_IN_WORKQUEUE)
		tfa_wait_until_calibration_done(tfa);
#endif /* TFA_WAIT_CAL_IN_WORKQUEUE */
	}

	if (value == NULL)
		return TFA98XX_ERROR_BAD_PARAMETER;

	*value = tfa->vval_result;
	if ((int)*value < 0) {
		pr_info("%s: V validation is not tested\n",
			__func__);
		*value = 0xffff;
		return TFA98XX_ERROR_OK;
	}

	for (idx = 0; idx < ndev; idx++) {
		ntfa = tfa98xx_get_tfa_device_from_index(idx);
		if (ntfa == NULL)
			continue;

		/* reset V validation */
		ntfa->vval_active = 0;
	}

	pr_info("%s: end\n", __func__);

	return TFA98XX_ERROR_OK;
}
EXPORT_SYMBOL(tfa_run_vval);

enum tfa98xx_error tfa_get_vval_data(int index, uint16_t *value)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(index);

	if (!tfa)
		return TFA98XX_ERROR_NOT_OPEN;

	if (value == NULL)
		return TFA98XX_ERROR_BAD_PARAMETER;

	*value = tfa->vval_result;
	if ((int)*value < 0) {
		pr_info("%s: V validation is not tested\n",
			__func__);
		*value = 0xffff;
		return TFA98XX_ERROR_OK;
	}

	return TFA98XX_ERROR_OK;
}
EXPORT_SYMBOL(tfa_get_vval_data);
#endif /* TFA_USE_TFAVVAL_NODE */

#if defined(TFA_USE_TFASTC_NODE)
#define TFA_USE_DEFAULT_TEMP_IN_LPM
#undef TFA_RESET_STC_VOLUME_IN_LPM

static int tfa_get_power_state(int index)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(index);
	int pm = 0, state = 0, control = 0;

	if (tfa == NULL)
		return 0; /* unused device */

#if defined(USE_TFA9878)
	state = TFA7x_GET_BF(tfa, LP0);
	control = TFA7x_GET_BF(tfa, IPM);
	if ((control == 0x0 || control == 0x3)
		&& (state == 0x1))
		pm |= 0x2; /* idle power */
#endif /* USE_TFA9878 */
	switch (tfa->rev & 0xff) {
	case 0x78:
	case 0x74:
	case 0x72:
	case 0x94:
		state = TFA7x_GET_BF(tfa, LP1);
		control = TFA7x_GET_BF(tfa, LPM1MODE);
		if ((control == 0x0 || control == 0x3)
			&& (state == 0x1))
			pm |= 0x1; /* low power */
		break;
	default:
		/* neither TFA987x */
		break;
	}

	state = TFA_GET_BF(tfa, PWDN);
	if (state == 1)
		pm |= 0x4; /* power down */

	return pm;
}

int tfa98xx_update_spkt_data(int idx)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(0);
	struct tfa98xx *tfa98xx;
	int ret = 0;
	int value[MAX_HANDLES] = {0};
	int i, ndev, data = 0;
#if defined(TFA_USE_DEFAULT_TEMP_IN_LPM)
	int pm = 0;
#endif

	if (tfa == NULL)
		return DEFAULT_REF_TEMP; /* unused device */
	if (tfa->tfa_family == 0)
		return DEFAULT_REF_TEMP;

	ndev = tfa->dev_count;
	if ((ndev < 1)
		|| (idx < 0 || idx >= ndev))
		return DEFAULT_REF_TEMP;

	if (tfa98xx_count_active_stream(BIT_PSTREAM) == 0) {
		pr_info("%s: skipped - tfadsp is not active!\n",
			__func__);
		return DEFAULT_REF_TEMP;
	}

	if (tfa->is_calibrating) {
		pr_info("%s: skipped - tfadsp is running calibraion!\n",
			__func__);
		return DEFAULT_REF_TEMP;
	}

#if defined(TFA_USE_DEFAULT_TEMP_IN_LPM)
	pm = tfa_get_power_state(idx);
	pr_info("%s: tfa_stc - dev %d - power state 0x%x\n",
		__func__, idx, pm);

	if (pm > 0) /* reset temperature in low power state */
		return DEFAULT_REF_TEMP;
#endif /* TFA_USE_DEFAULT_TEMP_IN_LPM */

	pr_info("%s: tfa_stc - read tspkr for stc\n",
		__func__);

	tfa98xx = (struct tfa98xx *)tfa->data;

	mutex_lock(&tfa98xx->dsp_lock);
	ret = tfa_read_tspkr(tfa, value);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (ret) {
		pr_info("%s: tfa_stc failed to read data from amplifier\n",
			__func__);
		value[idx] = DEFAULT_REF_TEMP;
	}
	if (value[idx] == 0xffff) {
		pr_info("%s: tfa_stc read wrong data from amplifier\n",
			__func__);
	}
	data = value[idx];

	for (i = 0; i < ndev; i++)
		pr_debug("%s: data[%d]%s - %d\n", __func__, i,
			(idx == i) ? "*" : "", value[i]);

	return data;
}
EXPORT_SYMBOL(tfa98xx_update_spkt_data);

int tfa98xx_write_sknt_control(int idx, int value)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(0);
	struct tfa98xx *tfa98xx;
	int ret = 0;
	int pm = 0;
	int i, ndev, ready = 0;
	static int data[MAX_HANDLES];
	static int update[MAX_HANDLES];

	if (tfa == NULL)
		return -ENODEV;
	if (tfa->tfa_family == 0)
		return -ENODEV;

	ndev = tfa->dev_count;
	if ((ndev < 1)
		|| (idx < 0 || idx >= ndev))
		return -EINVAL;

	if (tfa98xx_count_active_stream(BIT_PSTREAM) == 0) {
		pr_info("%s: skipped - no active stream!\n",
			__func__);
		goto tfa98xx_write_sknt_control_exit;
	}

	if (tfa->is_bypass) {
		pr_info("%s: skipped - tfadsp in bypass\n",
			__func__);
		goto tfa98xx_write_sknt_control_exit;
	}

	if (tfa->is_calibrating) {
		pr_info("%s: skipped - tfadsp is running calibraion!\n",
			__func__);
		goto tfa98xx_write_sknt_control_exit;
	}

	pm = tfa_get_power_state(idx);
	if (pm & 0x4) { /* skip if device is powered down */
		pr_info("%s: tfa_stc - skip dev %d - power state 0x%x\n",
			__func__, idx, pm);
		return ret;
	}
#if defined(TFA_RESET_STC_VOLUME_IN_LPM)
	if (pm > 0) { /* reset gain in low power state */
		pr_info("%s: tfa_stc - dev %d - set default - power state 0x%x\n",
			__func__, idx, pm);
		value = DEFAULT_REF_TEMP;
	}
#endif /* TFA_RESET_STC_VOLUME_IN_LPM */

	pr_info("%s: tfa_stc - dev %d - set surface temperature (%d)\n",
		__func__, idx, value);
	if (update[idx])
		pr_debug("%s: tfa_stc - dev %d - overwrite data\n",
			__func__, idx);

#if !defined(TFA_USE_STC_VOLUME_TABLE)
	data[idx] = value;
#else
	data[idx] = tfa_get_sknt_data_from_table(idx, value);
#endif
	update[idx] = 1;

	for (i = 0; i < ndev; i++) {
		pm = tfa_get_power_state(i);
		if (pm & 0x4) { /* count power down */
			pr_info("%s: tfa_stc - dev %d: check power down\n",
				__func__, i);
			ready++;
			data[i] = DEFAULT_REF_TEMP;
			continue;
		}

		if (update[i] > 0)
			ready++;
	}

	if (ready < ndev)
		/* wait until all the active devices are ready */
		return ret;

	pr_info("%s: tfa_stc - write volume for stc\n",
		__func__);

	tfa98xx = (struct tfa98xx *)tfa->data;

	mutex_lock(&tfa98xx->dsp_lock);
	tfa->individual_msg = 1;
	ret = tfa_write_volume(tfa, data);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (ret) {
		pr_info("%s: tfa_stc failed to write data to amplifier\n",
			__func__);
		goto tfa98xx_write_sknt_control_exit;
	}

	for (i = 0; i < ndev; i++)
		pr_debug("%s: data[%d]%s - %d\n", __func__, i,
			(update[i]) ? "*" : "", data[i]);

tfa98xx_write_sknt_control_exit:
	pr_info("%s: tfa_stc - reset update flags\n",
		__func__);
	memset(update, 0, ndev * sizeof(int));

	return ret;
}
EXPORT_SYMBOL(tfa98xx_write_sknt_control);
#endif /* TFA_USE_TFASTC_NODE */

static int tfa98xx_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct snd_soc_dai_driver *dai;
	struct tfa98xx *tfa98xx;
	struct device_node *np = i2c->dev.of_node;
	int irq_flags;
	unsigned int reg;
	int ret;

	pr_info("%s: start probing\n", __func__);
	pr_info("addr=0x%x\n", i2c->addr);

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	tfa98xx = devm_kzalloc(&i2c->dev,
		sizeof(struct tfa98xx), GFP_KERNEL);
	if (tfa98xx == NULL)
		return -ENOMEM;

	tfa98xx->dev = &i2c->dev;
	tfa98xx->i2c = i2c;
	tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
	tfa98xx->rate = 48000; /* init to the default sample rate (48kHz) */
	tfa98xx->tfa = NULL;

	tfa98xx->regmap = devm_regmap_init_i2c(i2c, &tfa98xx_regmap);
	if (IS_ERR(tfa98xx->regmap)) {
		ret = PTR_ERR(tfa98xx->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	i2c_set_clientdata(i2c, tfa98xx);
	mutex_init(&tfa98xx->dsp_lock);
	init_waitqueue_head(&tfa98xx->wq);

	if (np) {
		ret = tfa98xx_parse_dt(&i2c->dev, tfa98xx, np);
		if (ret) {
			dev_err(&i2c->dev, "Failed to parse DT node\n");
			return ret;
		}
		if (no_start)
			tfa98xx->irq_gpio = -1;
		if (no_reset)
			tfa98xx->reset_gpio = -1;
	} else {
		tfa98xx->reset_gpio = -1;
		tfa98xx->irq_gpio = -1;
	}

	if (gpio_is_valid(tfa98xx->reset_gpio)) {
#if defined(USE_TFA9878)
		ret = gpio_request_one(tfa98xx->reset_gpio,
			GPIOF_OUT_INIT_HIGH, "TFA98XX_RSTN"); /* RSTN */
#else
		ret = gpio_request_one(tfa98xx->reset_gpio,
			GPIOF_OUT_INIT_LOW, "TFA98XX_RST"); /* RST */
#endif
		if (ret)
			return ret;
	}

	if (gpio_is_valid(tfa98xx->irq_gpio)) {
		ret = gpio_request_one(tfa98xx->irq_gpio,
			GPIOF_DIR_IN, "TFA98XX_INT");
		if (ret)
			return ret;
	}

	/* Power up! */
	tfa98xx_ext_reset(tfa98xx);

	if ((no_start == 0) && (no_reset == 0)) {
		ret = regmap_read(tfa98xx->regmap, 0x03, &reg);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to read Revision register: %d\n",
				ret);
			return -EIO;
		}

		switch (reg & 0xff) {
		case 0x72: /* tfa9872 */
			pr_info("TFA9872 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_REMOVE_PLOP_NOISE;
			/* tfa98xx->flags |= TFA98XX_FLAG_LP_MODES; */
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x74: /* tfa9874 */
			pr_info("TFA9874 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x78: /* tfa9878 */
			pr_info("TFA9878 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x88: /* tfa9888 */
			pr_info("TFA9888 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_STEREO_DEVICE;
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x13: /* tfa9912 */
			pr_info("TFA9912 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			/* tfa98xx->flags |= TFA98XX_FLAG_TAPDET_AVAILABLE; */
			break;
		case 0x94: /* tfa9894 */
			pr_info("TFA9894 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x80: /* tfa9890 */
		case 0x81: /* tfa9890 */
			pr_info("TFA9890 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x92: /* tfa9891 */
			pr_info("TFA9891 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SAAM_AVAILABLE;
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x12: /* tfa9895 */
			pr_info("TFA9895 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x97:
			pr_info("TFA9897 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x96:
			pr_info("TFA9896 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		default:
			pr_info("Unsupported device revision (0x%x)\n",
				reg & 0xff);
			return -EINVAL;
		}
	}

	tfa98xx->tfa = devm_kzalloc(&i2c->dev,
		sizeof(struct tfa_device), GFP_KERNEL);
	if (tfa98xx->tfa == NULL)
		return -ENOMEM;

	tfa98xx->tfa->data = (void *)tfa98xx;
	tfa98xx->tfa->cachep = tfa98xx_cache;

#if defined(TFA_LIMIT_CAL_FROM_DTS)
	if (np) {
		ret = tfa98xx_parse_limit_cal_dt(&i2c->dev, tfa98xx, np);
		if (ret) {
			dev_err(&i2c->dev,
				"Failed to parse DT node for cal range\n");
			/* set default range instead */
		}
	}
#endif /* TFA_LIMIT_CAL_FROM_DTS */
#if defined(TFA_USE_DUMMY_CAL)
	if (np) {
		ret = tfa98xx_parse_dummy_cal_dt(&i2c->dev, tfa98xx, np);
		if (ret) {
			dev_err(&i2c->dev,
				"Failed to parse DT node for dummy value for calibration\n");
			/* set default value instead */
		}
	}
#endif /* TFA_USE_DUMMY_CAL */

	/* Modify the stream names, by appending the i2c device address.
	 * This is used with multicodec, in order to discriminate devices.
	 * Stream names appear in the dai definition and in the stream.
	 * We create copies of original structures because each device will
	 * have its own instance of this structure, with its own address.
	 */
	dai = devm_kzalloc(&i2c->dev, sizeof(tfa98xx_dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;
	memcpy(dai, tfa98xx_dai, sizeof(tfa98xx_dai));

	tfa98xx_append_i2c_address(&i2c->dev,
		i2c,
		NULL,
		0,
		dai,
		ARRAY_SIZE(tfa98xx_dai));

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	ret = snd_soc_register_component(&i2c->dev,
		&soc_component_dev_tfa98xx, dai,
		ARRAY_SIZE(tfa98xx_dai));
#else
	ret = snd_soc_register_codec(&i2c->dev,
		&soc_codec_dev_tfa98xx, dai,
		ARRAY_SIZE(tfa98xx_dai));
#endif

	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register TFA98xx: %d\n", ret);
		return ret;
	}

	if (gpio_is_valid(tfa98xx->irq_gpio) &&
		!(tfa98xx->flags & TFA98XX_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
			gpio_to_irq(tfa98xx->irq_gpio),
			NULL, tfa98xx_irq, irq_flags,
			"tfa98xx", tfa98xx);
		if (ret != 0) {
			dev_err(&i2c->dev, "Failed to request IRQ %d: %d\n",
				gpio_to_irq(tfa98xx->irq_gpio), ret);
			return ret;
		}
	} else {
		dev_info(&i2c->dev, "Skipping IRQ registration\n");
		/* disable feature support if gpio was invalid */
		tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
	}

#if defined(CONFIG_DEBUG_FS)
	if (no_start == 0)
		tfa98xx_debug_init(tfa98xx, i2c);
#endif
	/* Register the sysfs files for climax backdoor access */
	ret = sysfs_create_bin_file(&i2c->dev.kobj, &dev_attr_rw);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, rw\n");
	ret = sysfs_create_bin_file(&i2c->dev.kobj, &dev_attr_reg);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, reg\n");

#ifdef CALFUNC_IN_SYSFS
	ret = device_create_file(&i2c->dev, &dev_attr_calibrate);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, calibrate\n");
	ret = device_create_file(&i2c->dev, &dev_attr_mtpex);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, MTPEX\n");
	ret = device_create_file(&i2c->dev, &dev_attr_re25);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, re25\n");
#endif

#ifdef LOGFUNC_IN_SYSFS
	ret = device_create_file(&i2c->dev, &dev_attr_blackbox);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, log\n");
#endif

#ifdef GAINFUNC_IN_SYSFS
	ret = device_create_file(&i2c->dev, &dev_attr_gain);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, gain\n");
#endif

#ifdef TFA_DISABLE_AUTO_CAL
	ret = device_create_file(&i2c->dev, &dev_attr_autocal);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, autocal\n");
#endif

	pr_info("%s Probe completed successfully!\n", __func__);

	INIT_LIST_HEAD(&tfa98xx->list);

	mutex_lock(&tfa98xx_mutex);
	tfa98xx_device_count++;
	list_add(&tfa98xx->list, &tfa98xx_device_list); /* stack */
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_i2c_remove(struct i2c_client *i2c)
{
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	pr_debug("addr=0x%x\n", i2c->addr);

	tfa98xx_interrupt_enable(tfa98xx, false);

	cancel_delayed_work_sync(&tfa98xx->interrupt_work);
	cancel_delayed_work_sync(&tfa98xx->monitor_work);
#if !defined(TFA_USE_DIRECT_API_CALL)
	cancel_delayed_work_sync(&tfa98xx->init_work);
#endif
#if (defined(USE_TFA9891) || defined(USE_TFA9912))
	cancel_delayed_work_sync(&tfa98xx->tapdet_work);
#endif

	sysfs_remove_bin_file(&i2c->dev.kobj, &dev_attr_reg);
	sysfs_remove_bin_file(&i2c->dev.kobj, &dev_attr_rw);
#if defined(CONFIG_DEBUG_FS)
	tfa98xx_debug_remove(tfa98xx);
#endif

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	snd_soc_unregister_component(&i2c->dev);
#else
	snd_soc_unregister_codec(&i2c->dev);
#endif

	if (gpio_is_valid(tfa98xx->irq_gpio))
		gpio_free(tfa98xx->irq_gpio);
	if (gpio_is_valid(tfa98xx->reset_gpio))
		gpio_free(tfa98xx->reset_gpio);

	mutex_lock(&tfa98xx_mutex);
	list_del(&tfa98xx->list);
	tfa98xx_device_count--;
	if (tfa98xx_device_count == 0) {
		kfree(tfa98xx_container);
		tfa98xx_container = NULL;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static const struct i2c_device_id tfa98xx_i2c_id[] = {
	{"tfa98xx", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, tfa98xx_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id tfa98xx_dt_match[] = {
	{.compatible = "tfa,tfa98xx"},
	{.compatible = "tfa,tfa9872"},
	{.compatible = "tfa,tfa9874"},
	{.compatible = "tfa,tfa9878"},
	{.compatible = "tfa,tfa9888"},
	{.compatible = "tfa,tfa9890"},
	{.compatible = "tfa,tfa9891"},
	{.compatible = "tfa,tfa9894"},
	{.compatible = "tfa,tfa9895"},
	{.compatible = "tfa,tfa9896"},
	{.compatible = "tfa,tfa9897"},
	{.compatible = "tfa,tfa9912"},
	{},
};
#endif

static struct i2c_driver tfa98xx_i2c_driver = {
	.driver = {
		.name = "tfa98xx",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tfa98xx_dt_match),
	},
	.probe = tfa98xx_i2c_probe,
	.remove = tfa98xx_i2c_remove,
	.id_table = tfa98xx_i2c_id,
};

static int __init tfa98xx_i2c_init(void)
{
	int ret = 0;

	pr_info("TFA98XX driver version %s\n", TFA98XX_VERSION);

	/* Enable debug traces */
	tfa98xx_kmsg_regs = trace_level & 2;
	tfa98xx_ftrace_regs = trace_level & 4;

	/* Initialize kmem_cache */
	/* Cache name /proc/slabinfo */
	tfa98xx_cache = kmem_cache_create("tfa98xx_cache",
		PAGE_SIZE, /* Structure size, we should fit in single page */
		0, /* Structure alignment */
		(SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT |
		SLAB_MEM_SPREAD), /* Cache property */
		NULL); /* Object constructor */
	if (!tfa98xx_cache) {
		pr_err("tfa98xx can't create memory pool\n");
		ret = -ENOMEM;
	}

	ret = i2c_add_driver(&tfa98xx_i2c_driver);

	return ret;
}
module_init(tfa98xx_i2c_init);

static void __exit tfa98xx_i2c_exit(void)
{
	i2c_del_driver(&tfa98xx_i2c_driver);
	kmem_cache_destroy(tfa98xx_cache);
}
module_exit(tfa98xx_i2c_exit);

MODULE_DESCRIPTION("ASoC TFA98XX driver");
MODULE_LICENSE("GPL");

