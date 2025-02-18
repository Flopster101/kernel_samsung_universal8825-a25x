// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts.c
 *
 * ALSA SoC - Samsung VTS driver
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
#include <linux/sched/clock.h>
#include <linux/miscdevice.h>
#include <linux/pinctrl/consumer.h>
#include <linux/suspend.h>

#include <asm-generic/delay.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/samsung/vts.h>
#include <sound/sounddev_vts.h>

#include <sound/samsung/mailbox.h>
#include <sound/samsung/vts.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/exynos-el3_mon.h>
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
#include <soc/samsung/exynos-itmon.h>
#endif
#include <soc/samsung/imgloader.h>
#include <soc/samsung/exynos-s2mpu.h>
#include <soc/samsung/debug-snapshot.h>

#include "vts.h"
#include "vts_res.h"
#include "vts_log.h"
#include "vts_dump.h"
/* #include "vts_s_lif_dump.h" */
#include "vts_dbg.h"
/* For DEF_VTS_PCM_DUMP */
#include "vts_pcm_dump.h"
#include "vts_proc.h"

#undef EMULATOR
#ifdef EMULATOR
static void __iomem *pmu_alive;
#define set_mask_value(id, mask, value)	(id = ((id & ~mask) | (value & mask)))

static void update_mask_value(void __iomem *sfr,
	unsigned int mask, unsigned int value)
{
	unsigned int sfr_value = readl(sfr);

	set_mask_value(sfr_value, mask, value);
	writel(sfr_value, sfr);
}
#endif

#define clear_bit(data, offset) ((data) &= ~(0x1<<(offset)))
#define clear_bits(data, value, offset) ((data) &= ~((value)<<(offset)))
#define set_bit(data, offset) ((data) |= (0x1<<(offset)))
#define set_bits(data, value, offset) ((data) |= ((value)<<(offset)))
#define check_bit(data, offset)	(((data)>>(offset)) & (0x1))

/* For only external static functions */
static struct vts_data *p_vts_data;
static void vts_dbg_dump_fw_gpr(struct device *dev, struct vts_data *data,
			unsigned int dbg_type);

static void vts_check_version(struct device *dev);

static struct platform_driver samsung_vts_driver;

bool is_vts(struct device *dev)
{
	return (&samsung_vts_driver.driver) == dev->driver;
}

struct vts_data *vts_get_data(struct device *dev)
{
	while (dev && !is_vts(dev))
		dev = dev->parent;

	return dev ? dev_get_drvdata(dev) : NULL;
}

/* vts mailbox interface functions */
int vts_mailbox_generate_interrupt(
	const struct platform_device *pdev,
	int hw_irq)
{
	struct vts_data *data = p_vts_data;
	struct device *dev = data ? (data->pdev ?
				&data->pdev->dev : NULL) : NULL;
	unsigned long flag;
	int result = 0;

	if (!data || !dev) {
		vts_dev_warn(dev, "%s: VTS not Initialized\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&data->state_spinlock, flag);
	/* Check VTS state before accessing mailbox */
	if (data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_VOICECALL ||
		data->vts_state == VTS_STATE_NONE) {
		vts_dev_warn(dev, "%s: VTS wrong state [%d]\n", __func__,
			data->vts_state);
		result = -EINVAL;
		goto out;
	}
	result = mailbox_generate_interrupt(pdev, hw_irq);

out:
	spin_unlock_irqrestore(&data->state_spinlock, flag);
	return result;
}

void vts_mailbox_write_shared_register(
	const struct platform_device *pdev,
	const u32 *values,
	int start,
	int count)
{
	struct vts_data *data = p_vts_data;
	struct device *dev = data ? (data->pdev ?
				&data->pdev->dev : NULL) : NULL;
	unsigned long flag;

	if (!data || !dev) {
		vts_dev_warn(dev, "%s: VTS not Initialized\n", __func__);
		return;
	}

	spin_lock_irqsave(&data->state_spinlock, flag);
	/* Check VTS state before accessing mailbox */
	if (data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_VOICECALL ||
		data->vts_state == VTS_STATE_NONE) {
		vts_dev_warn(dev, "%s: VTS wrong state [%d]\n", __func__,
			data->vts_state);
		goto out;
	}
	mailbox_write_shared_register(pdev, values, start, count);

out:
	spin_unlock_irqrestore(&data->state_spinlock, flag);
}

void vts_mailbox_read_shared_register(
	const struct platform_device *pdev,
	u32 *values,
	int start,
	int count)
{
	struct vts_data *data = p_vts_data;
	struct device *dev = data ? (data->pdev ?
				&data->pdev->dev : NULL) : NULL;
	unsigned long flag;

	if (!data || !dev) {
		vts_dev_warn(dev, "%s: VTS not Initialized\n", __func__);
		return;
	}

	spin_lock_irqsave(&data->state_spinlock, flag);
	/* Check VTS state before accessing mailbox */
	if (data && (data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_VOICECALL ||
		data->vts_state == VTS_STATE_NONE)) {
		vts_dev_warn(dev, "%s: VTS wrong state [%d]\n", __func__,
			data->vts_state);
		goto out;
	}

	mailbox_read_shared_register(pdev, values, start, count);

out:
	spin_unlock_irqrestore(&data->state_spinlock, flag);
}

static int vts_start_ipc_transaction_atomic(
	struct device *dev, struct vts_data *data,
	int msg, u32 (*values)[3], int sync)
{
	unsigned long flag;
	long result = 0;
	u32 ack_value = 0;
	enum ipc_state *state = &data->ipc_state_ap;

	vts_dev_info(dev, "%s:[+%d][0x%x,0x%x,0x%x]\n",
		VTS_TAG_IPC, msg, (*values)[0],
		(*values)[1], (*values)[2]);

	/* Check VTS state before processing IPC,
	 * in VTS_STATE_RUNTIME_SUSPENDING state only Power Down IPC
	 * can be processed
	 */
	spin_lock_irqsave(&data->state_spinlock, flag);
	if ((data->vts_state == VTS_STATE_RUNTIME_SUSPENDING &&
		msg != VTS_IRQ_AP_POWER_DOWN) ||
		data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_VOICECALL ||
		data->vts_state == VTS_STATE_NONE) {
		vts_dev_warn(dev, "%s: VTS IP %s state\n", __func__,
			(data->vts_state == VTS_STATE_VOICECALL ?
			"VoiceCall" : "Suspended"));
		spin_unlock_irqrestore(&data->state_spinlock, flag);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&data->state_spinlock, flag);

	if (pm_runtime_suspended(dev)) {
		vts_dev_warn(dev, "%s: VTS IP is in suspended state, IPC cann't be processed\n",
			VTS_TAG_IPC);
		return -EINVAL;
	}

	if (!data->vts_ready) {
		vts_dev_warn(dev, "%s: VTS Firmware Not running\n", VTS_TAG_IPC);
		return -EINVAL;
	}

	spin_lock(&data->ipc_spinlock);

	*state = SEND_MSG;
	vts_mailbox_write_shared_register(data->pdev_mailbox, *values, 0, 3);
	vts_mailbox_generate_interrupt(data->pdev_mailbox, msg);
	data->running_ipc = msg;

	if (sync) {
		int i;

		/* for (i = 50; i && (*state != SEND_MSG_OK) && */
		for (i = 200; i && (*state != SEND_MSG_OK) &&
				(*state != SEND_MSG_FAIL) &&
				(ack_value != (0x1 << msg)); i--) {
			vts_mailbox_read_shared_register(data->pdev_mailbox,
							&ack_value, 3, 1);
			vts_dev_dbg(dev, "%s ACK-value: 0x%08x\n", VTS_TAG_IPC,
				ack_value);
			mdelay(1);
		}
		if (!i) {
			vts_dev_warn(dev, "Transaction timeout!! Ack_value:0x%x\n",
					ack_value);
			vts_dbg_dump_fw_gpr(dev, data, VTS_IPC_TRANS_FAIL);
			vts_dev_info(dev, "shared_info 0x%x, %d %d",
				data->shared_info->vendor_data[0],
				data->shared_info->vendor_data[1],
				data->shared_info->vendor_data[2]);
			print_hex_dump(KERN_ERR, "vts-fw-log", DUMP_PREFIX_OFFSET, 32, 4, data->sramlog_baseaddr,
				VTS_SRAM_EVENTLOG_SIZE_MAX, true);
			print_hex_dump(KERN_ERR, "vts-time-log", DUMP_PREFIX_OFFSET, 32, 4,
				data->sramlog_baseaddr + VTS_SRAM_EVENTLOG_SIZE_MAX,
				VTS_SRAM_TIMELOG_SIZE_MAX, true);
		}
		if (*state == SEND_MSG_OK || ack_value == (0x1 << msg)) {
			vts_dev_dbg(dev, "Transaction success Ack_value:0x%x\n",
					ack_value);
			if (ack_value == (0x1 << VTS_IRQ_AP_COMMAND) &&
				((*values)[0] == VTS_ENABLE_DEBUGLOG ||
				(*values)[0] == VTS_ENABLE_AUDIODUMP ||
				(*values)[0] == VTS_ENABLE_LOGDUMP)) {
				u32 ackvalues[3] = {0, 0, 0};

				mailbox_read_shared_register(data->pdev_mailbox,
					ackvalues, 4, 2);
				vts_dev_info(dev, "%s: offset: 0x%x size:0x%x\n",
					VTS_TAG_IPC, ackvalues[0], ackvalues[1]);
				if ((*values)[0] == VTS_ENABLE_DEBUGLOG) {
					/* Register debug log buffer */
					vts_register_log_buffer(dev,
						ackvalues[0], ackvalues[1]);
					vts_dev_dbg(dev, "%s: Log buffer\n",
						VTS_TAG_IPC);
				} else {
					u32 dumpmode =
					((*values)[0] == VTS_ENABLE_LOGDUMP ?
					VTS_LOG_DUMP : VTS_AUDIO_DUMP);
					/* register dump offset & size */
					vts_dump_addr_register(dev,
							ackvalues[0],
							ackvalues[1],
							dumpmode);
					vts_dev_dbg(dev, "%s: Dump buffer\n",
							VTS_TAG_IPC);
				}
			} else if (ack_value ==
				(0x1 << VTS_IRQ_AP_GET_VERSION)) {
				u32 version;
				mailbox_read_shared_register(data->pdev_mailbox,
					&version, 2, 1);
				vts_dev_dbg(dev, "google version(%d) : %d %d",
					__LINE__, version,
					data->google_version);
			}
		} else {
			vts_dev_err(dev, "Transaction failed\n");
		}
		result = (*state == SEND_MSG_OK || ack_value) ? 0 : -EIO;
	}

	/* Clear running IPC & ACK value */
	ack_value = 0x0;
	vts_mailbox_write_shared_register(data->pdev_mailbox,
						&ack_value, 3, 1);
	data->running_ipc = 0;
	*state = IDLE;

	spin_unlock(&data->ipc_spinlock);
	vts_dev_info(dev, "%s:[-%d]\n", VTS_TAG_IPC, msg);

	return (int)result;
}

int vts_start_ipc_transaction(struct device *dev, struct vts_data *data,
		int msg, u32 (*values)[3], int atomic, int sync)
{
	return vts_start_ipc_transaction_atomic(dev, data, msg, values, sync);
}
EXPORT_SYMBOL(vts_start_ipc_transaction);

static int vts_ipc_ack(struct vts_data *data, u32 result)
{
	if (!data->vts_ready)
		return 0;

	pr_debug("%s(%p, %u)\n", __func__, data, result);
	vts_mailbox_write_shared_register(data->pdev_mailbox,
						&result, 0, 1);
	vts_mailbox_generate_interrupt(data->pdev_mailbox,
					VTS_IRQ_AP_IPC_RECEIVED);
	return 0;
}

int vts_send_ipc_ack(struct vts_data *data, u32 result)
{
	return vts_ipc_ack(data, result);
}

int vts_imgloader_mem_setup(
	struct imgloader_desc *desc, const u8 *metadata, size_t size,
	phys_addr_t *fw_phys_base, size_t *fw_bin_size, size_t *fw_mem_size)
{
	struct vts_data *data = dev_get_drvdata(desc->dev);

	vts_dev_info(desc->dev, "%s\n", __func__);
	if (!data) {
		vts_dev_err(desc->dev, "vts data is null\n");
		return -EINVAL;
	}

	if (!data->firmware) {
		vts_dev_err(desc->dev, "fw is not loaded\n");
		return -EINVAL;
	}

	memcpy(data->sram_base, data->firmware->data, data->firmware->size);
	*fw_phys_base = data->sram_phys;
	*fw_bin_size = data->firmware->size;
	*fw_mem_size = data->sram_size;
	vts_dev_info(desc->dev, "fw is downloaded(size=%zu)\n",
		data->firmware->size);
	return 0;
}

int vts_imgloader_verify_fw(struct imgloader_desc *desc,
	phys_addr_t fw_phys_base,
	size_t fw_bin_size, size_t fw_mem_size)
{
	uint64_t ret64 = 0;

	if (!IS_ENABLED(CONFIG_EXYNOS_S2MPU)) {
		vts_dev_warn(desc->dev, "H-Arx is not enabled\n");
		return 0;
	}

	ret64 = exynos_verify_subsystem_fw(desc->name, desc->fw_id,
					fw_phys_base, fw_bin_size, fw_mem_size);
	vts_dev_info(desc->dev, "verify fw(size:%zu)\n", fw_bin_size);
	if (ret64) {
		vts_dev_warn(desc->dev, "Failed F/W verification, ret=%llu\n",
			ret64);
		return -EIO;
	}
	vts_cpu_power(desc->dev, true);
	ret64 = exynos_request_fw_stage2_ap(desc->name);
	if (ret64) {
		vts_dev_warn(desc->dev, "Failed F/W verification to S2MPU, ret=%llu\n",
			ret64);
		return -EIO;
	}
	return 0;
}

struct imgloader_ops vts_imgloader_ops = {
	.mem_setup = vts_imgloader_mem_setup,
	.verify_fw = vts_imgloader_verify_fw,
};

static int vts_core_imgloader_desc_init(struct platform_device *pdev)
{
	struct vts_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	struct imgloader_desc *desc = &data->vts_imgloader_desc;

	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->ops = &vts_imgloader_ops;
	if (IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)) {
		vts_dev_info(dev, "%s: EVT0 \n", __func__);
		desc->fw_name = "vts_evt0.bin";
	} else {
		desc->fw_name = "vts.bin";
	}
	desc->name = "VTS";
	desc->s2mpu_support = false;
	desc->skip_request_firmware = true;
	desc->fw_id = 0;
	return imgloader_desc_init(desc);
}

static int vts_download_firmware(struct platform_device *pdev)
{
	struct vts_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	vts_dev_info(dev, "%s\n", __func__);
	if (!data->firmware) {
		vts_dev_err(dev, "fw is not loaded\n");
		return -EAGAIN;
	}

	memcpy(data->sram_base, data->firmware->data, data->firmware->size);
	vts_dev_info(dev, "fw is downloaded(size=%zu)\n",
		data->firmware->size);
	return 0;
}

static int vts_wait_for_fw_ready(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int result;

	result = wait_event_timeout(data->ipc_wait_queue,
		data->vts_ready, msecs_to_jiffies(3000));
	if (data->vts_ready) {
		result = 0;
	} else {
		vts_dev_err(dev, "VTS Firmware is not ready\n");
		vts_dbg_dump_fw_gpr(dev, data, VTS_FW_NOT_READY);
		result = -ETIME;
	}

	return result;
}


bool vts_is_on(void)
{
	vts_info("[VTS]%s : %d\n", __func__,
		(p_vts_data && p_vts_data->enabled));
	return p_vts_data && p_vts_data->enabled;
}
EXPORT_SYMBOL(vts_is_on);

bool vts_is_recognitionrunning(void)
{
	return p_vts_data && p_vts_data->running;
}
EXPORT_SYMBOL(vts_is_recognitionrunning);

static struct snd_soc_dai_driver vts_dai[] = {
	{
		.name = "vts-tri",
		.id = 0,
		.capture = {
			.stream_name = "VTS Trigger Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16,
			.sig_bits = 16,
		 },
	},
	{
		.name = "vts-rec",
		.id = 1,
		.capture = {
			.stream_name = "VTS Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16,
			.sig_bits = 16,
		 },
	},
};

static const char * const vts_hpf_sel_texts[] = {"120Hz", "40Hz"};
static SOC_ENUM_SINGLE_DECL(vts_hpf_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_HPF_SEL_OFFSET, vts_hpf_sel_texts);

static const char * const vts_cps_sel_texts[] = {"normal", "absolute"};
static SOC_ENUM_SINGLE_DECL(vts_cps_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_CPS_SEL_OFFSET, vts_cps_sel_texts);

static const DECLARE_TLV_DB_SCALE(vts_gain_tlv_array, 0, 6, 0);

static const char * const vts_sys_sel_texts[] = {"512kHz", "768kHz",
	"384kHz", "2048kHz", "3072kHz_48kHz", "3072kHz_96kHz"};
static SOC_ENUM_SINGLE_DECL(vts_sys_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_SYS_SEL_OFFSET, vts_sys_sel_texts);

static int vts_sys_sel_put_enum(
	struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	unsigned int *item = ucontrol->value.enumerated.item;
	struct vts_data *data = p_vts_data;

	vts_dev_dbg(dev, "%s(%u)\n", __func__, item[0]);

	data->syssel_rate = item[0];

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

static const char * const vts_polarity_clk_texts[] = {"rising edge of clock",
	"falling edge of clock"};
static SOC_ENUM_SINGLE_DECL(vts_polarity_clk, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_POLARITY_CLK_OFFSET, vts_polarity_clk_texts);

static const char * const vts_polarity_output_texts[] = {"right first",
	"left first"};
static SOC_ENUM_SINGLE_DECL(vts_polarity_output, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_POLARITY_OUTPUT_OFFSET, vts_polarity_output_texts);

static const char * const vts_polarity_input_texts[] = {"left PDM on CLK high",
	"left PDM on CLK low"};
static SOC_ENUM_SINGLE_DECL(vts_polarity_input, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_POLARITY_INPUT_OFFSET, vts_polarity_input_texts);

static const char * const vts_ovfw_ctrl_texts[] = {"limit", "reset"};
static SOC_ENUM_SINGLE_DECL(vts_ovfw_ctrl, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_OVFW_CTRL_OFFSET, vts_ovfw_ctrl_texts);

static const char * const vts_cic_sel_texts[] = {"Off", "On"};
static SOC_ENUM_SINGLE_DECL(vts_cic_sel, VTS_DMIC_CONTROL_DMIC_IF,
	VTS_DMIC_CIC_SEL_OFFSET, vts_cic_sel_texts);

static const char * const vtsvcrecog_mode_text[] = {
	"OFF", "ON"
};

static const char * const vtsactive_phrase_text[] = {
	"NONE", "SoundModel_ID_1", "SoundModel_ID_2", "SoundModel_ID_3"
};
static const char * const vtsforce_reset_text[] = {
	"NONE", "RESET"
};
static SOC_ENUM_SINGLE_EXT_DECL(vtsvcrecog_mode_enum, vtsvcrecog_mode_text);
static SOC_ENUM_SINGLE_EXT_DECL(vtsactive_phrase_enum, vtsactive_phrase_text);
static SOC_ENUM_SINGLE_EXT_DECL(vtsforce_reset_enum, vtsforce_reset_text);

static int vts_start_recognization(struct device *dev, int start)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int active_trigger = data->active_trigger;
	int result;
	u32 values[3];

	vts_dev_info(dev, "%s for %s start %d\n", __func__,
		vtsactive_phrase_text[active_trigger], start);

	start = !!start;
	if (start) {
		vts_dev_info(dev, "%s: for %s, sm_loaded: %d\n",
			__func__, vtsactive_phrase_text[active_trigger],
			data->sm_loaded);
		vts_dev_info(dev, "%s exec_mode %d active_trig :%d\n", __func__,
			data->exec_mode, active_trigger);
		if (data->sm_loaded) {
			/*
			 * load sound model data before starting recognition
			 */
			memcpy(data->sram_base +
				data->sm_info.offset,
				data->sm_data,
				data->sm_info.actual_sz);
			vts_dev_info(dev, "Binary uploaded size=%zu\n",
				data->sm_info.actual_sz);
			data->sm_loaded = false;
		} else {
			vts_dev_err(dev, "%s: Model Binary File not Loaded\n",
				__func__);
			return -EINVAL;
		}

		result = vts_set_dmicctrl(data->pdev, active_trigger, true);
		if (result < 0) {
			vts_dev_err(dev, "%s: MIC control failed\n",
						 __func__);
			return result;
		}

		/* Send Start recognition IPC command to VTS */
		values[0] = active_trigger;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_START_RECOGNITION,
				&values, 0, 1);
		if (result < 0) {
			vts_dev_err(dev, "vts ipc VTS_IRQ_AP_START_RECOGNITION failed: %d\n",
				result);
			return result;
		}

#ifdef DEF_VTS_PCM_DUMP
		if (vts_pcm_dump_get_file_started(0)) {
			values[0] = 0;
			values[1] = 0;
			values[2] = 0;
			dev_info(dev, "VTS Recoding DUMP Start\n");
			vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_START_REC, &values, 1, 1);
			vts_pcm_dump_reset(0);
			vts_pcm_dump_reset(1);
			vts_set_dmicctrl(data->pdev, VTS_MICCONF_FOR_RECORD, true);
		}
#endif

		data->vts_state = VTS_STATE_RECOG_STARTED;
		data->poll_event_type = EVENT_NONE;
		vts_dev_info(dev, "%s start=%d, active_trigger=%d\n", __func__,
			start, active_trigger);
	} else if (!start) {
		values[0] = active_trigger;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_STOP_RECOGNITION,
				&values, 0, 1);
		if (result < 0) {
			vts_dev_err(dev, "vts ipc VTS_IRQ_AP_STOP_RECOGNITION failed: %d\n",
				result);
			/* HACK */
			/* return result; */
		}

#ifdef DEF_VTS_PCM_DUMP
		if (vts_pcm_dump_get_file_started(0)) {
			values[0] = 0;
			values[1] = 0;
			values[2] = 0;
			dev_info(dev, "VTS Recoding DUMP Stop\n");
			vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_STOP_REC, &values, 1, 1);
			vts_pcm_dump_reset(0);
			vts_pcm_dump_reset(1);
			vts_set_dmicctrl(data->pdev, VTS_MICCONF_FOR_RECORD, false);
		}
#endif
		data->vts_state = VTS_STATE_RECOG_STOPPED;
		vts_dev_info(dev, "%s: start=%d, active_trigger=%d\n",
				   __func__, start, active_trigger);

		if (data->clk_path == VTS_CLK_SRC_RCO || IS_ENABLED(CONFIG_SOC_S5E8825) ||
				IS_ENABLED(CONFIG_SOC_S5E9925)) {
			result = vts_set_dmicctrl(data->pdev, active_trigger, false);
			if (result < 0) {
				vts_dev_err(dev, "%s: MIC control failed\n",
							 __func__);
				return result;
			}
		}
	}
	return 0;
}

static int get_vtsvoicerecognize_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;

	ucontrol->value.integer.value[0] = data->exec_mode;

	vts_dev_dbg(component->dev, "GET VTS Execution mode: %d\n",
			 data->exec_mode);

	return 0;
}

static int set_voicerecognize_mode(struct device *dev, int vcrecognize_mode)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int result = 0;
	int vcrecognize_start = 0;

	pm_runtime_barrier(dev);

	vts_dev_info(dev, "%s: requested id %d %s, current active recognition: 0x%x\n",
			 __func__, data->active_trigger,
			 vtsvcrecog_mode_text[vcrecognize_mode],
			 data->voicerecog_start);

	if (vcrecognize_mode == VTS_RECOGNIZE_START) {
		if (data->voicerecog_start & (0x1 << data->active_trigger)) {
			vts_dev_warn(dev, "%s: requested trigger id %d is already completed",
					__func__, data->active_trigger);
			return 0;
		}

		pm_runtime_get_sync(dev);
		vts_start_runtime_resume(dev, 0);
		vts_clk_set_rate(dev, data->syssel_rate);
		vcrecognize_start = true;
	} else {
		if (!(data->voicerecog_start & (0x1 << data->active_trigger))) {
			vts_dev_warn(dev, "%s: requested trigger id %d is already off",
					__func__, data->active_trigger);
			return 0;
		}
		vcrecognize_start = false;
	}

	if (!pm_runtime_active(dev)) {
		vts_dev_warn(dev, "%s wrong state %d req: %d\n",
				__func__, data->exec_mode,
				vcrecognize_mode);
		return 0;
	}

	data->exec_mode = vcrecognize_mode;

	/* Start/stop the request Voice recognization mode */
	result = vts_start_recognization(dev,
					vcrecognize_start);
	if (result < 0) {
		vts_dev_err(dev, "Start Recognization Failed: %d\n",
			 result);
		goto err_ipcmode;
	}

	if (vcrecognize_start)
		data->voicerecog_start |= (0x1 << data->active_trigger);
	else
		data->voicerecog_start &= ~(0x1 << data->active_trigger);

	vts_dev_info(dev, "%s Configured: [%d] %s started\n",
		 __func__, data->exec_mode,
		 vtsvcrecog_mode_text[vcrecognize_mode]);

	if (!vcrecognize_start &&
			pm_runtime_active(dev)) {
		pm_runtime_put_sync(dev);
		if (data->mic_ready == 0x0 && pm_runtime_active(dev)) {
			vts_dev_info(dev, "%s: VTS is off. Power off vts cpu\n",
					__func__);
			pm_runtime_get_sync(dev);
			vts_cpu_enable(dev, false);
			vts_cpu_power(dev, false);
			data->running = false;
			pm_runtime_put_sync(dev);
		}
	}

	return  0;

err_ipcmode:
	if (pm_runtime_active(dev) && vcrecognize_start){
		pm_runtime_put_sync(dev);
	}

	if (!vcrecognize_start && pm_runtime_active(dev)) {
		pm_runtime_put_sync(dev);
		data->voicerecog_start &= ~(0x1 << data->active_trigger);
	}

	return result;
}

static int set_vtsvoicerecognize_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = dev_get_drvdata(dev);
	int vcrecognize_mode = 0;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	int ret = 0;

	vcrecognize_mode = ucontrol->value.integer.value[0];
	vts_dev_info(dev, "%s state %d %d (%d)\n", __func__,
			data->vts_state,
			data->clk_path,
			data->micclk_init_cnt);

	if (item[0] >= e->items) {
		vts_dev_err(dev,
			"%s try to set %d but should be under %d\n",
			__func__, item[0], e->items);

		return -EINVAL;
	}

	/* check dmic_if clk */
	if (data->clk_path == VTS_CLK_SRC_AUD0) {
		data->syssel_rate = 4;
		vts_dev_info(dev, "changed as 3072000Hz mode\n");
	}
	ret = set_voicerecognize_mode(dev, vcrecognize_mode);

	return ret;
}

int vts_chk_dmic_clk_mode(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int ret = 0;
	int syssel_rate_value = 1;
	u32 values[3] = {0, 0, 0};

	vts_dev_info(dev, "%s state %d %d (%d)\n",
		__func__, data->vts_state,
	    data->clk_path,
	    data->micclk_init_cnt);

	/* already started recognization mode and dmic_if clk is 3.072MHz*/
	/* VTS worked + Serial LIF */
	if (data->clk_path == VTS_CLK_SRC_RCO)
		syssel_rate_value = 1;
	else
		syssel_rate_value = 4;

	if (data->vts_state == VTS_STATE_RECOG_STARTED) {
		data->syssel_rate = syssel_rate_value;

		/* restart VTS to update mic and clock setting */
		data->poll_event_type |= EVENT_RESTART|EVENT_READY;
		wake_up(&data->poll_wait_queue);
	}

	if (data->vts_state == VTS_STATE_RECOG_TRIGGERED) {
		ret = vts_start_ipc_transaction(dev, data, VTS_IRQ_AP_STOP_COPY,
			&values, 1, 1);
		data->syssel_rate = syssel_rate_value;
		vts_clk_set_rate(dev, data->syssel_rate);
		if (data->micclk_init_cnt)
			data->micclk_init_cnt--;
		ret = vts_set_dmicctrl(data->pdev, data->active_trigger, true);
		if (ret < 0)
			vts_dev_err(dev, "%s: MIC control failed\n", __func__);
		vts_dev_info(dev, "VTS Triggered Fail : Serial LIF ON\n");
		ret = vts_start_ipc_transaction(dev, data,
			VTS_IRQ_AP_START_COPY, &values, 1, 1);
	}

	if (data->vts_rec_state == VTS_REC_STATE_START) {
		ret = vts_start_ipc_transaction(dev, data,
			VTS_IRQ_AP_STOP_REC, &values, 1, 1);
		data->syssel_rate = syssel_rate_value;
		vts_clk_set_rate(dev, data->syssel_rate);
		if (data->micclk_init_cnt)
			data->micclk_init_cnt--;
		ret = vts_set_dmicctrl(data->pdev, VTS_MICCONF_FOR_RECORD, true);
		if (ret < 0)
			vts_dev_err(dev, "%s: MIC control failed\n", __func__);
		vts_dev_info(dev, "VTS Recoding : Change SYS_SEL : %d\n",
			syssel_rate_value);
		ret = vts_start_ipc_transaction(dev, data,
			VTS_IRQ_AP_START_REC, &values, 1, 1);
	}
	return ret;
}
EXPORT_SYMBOL(vts_chk_dmic_clk_mode);

static int get_vtsactive_phrase(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;

	ucontrol->value.integer.value[0] = data->active_trigger;

	vts_dev_dbg(component->dev, "GET VTS Active Phrase: %s\n",
			vtsactive_phrase_text[data->active_trigger]);

	return 0;
}

static int set_vtsactive_phrase(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	int vtsactive_phrase;

	pm_runtime_barrier(component->dev);

	if (item[0] >= e->items) {
		vts_dev_err(component->dev,
			"%s try to set %d but shuld be under %d\n",
			__func__, item[0], e->items);

		return -EINVAL;
	}

	vtsactive_phrase = ucontrol->value.integer.value[0];

	if (vtsactive_phrase < 0) {
		vts_dev_err(component->dev,
		"Invalid VTS Trigger Key phrase =%d", vtsactive_phrase);
		return 0;
	}

	data->active_trigger = vtsactive_phrase;
	vts_dev_info(component->dev, "VTS Active phrase: %s\n",
		vtsactive_phrase_text[vtsactive_phrase]);

	return  0;
}

static int get_voicetrigger_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;

	ucontrol->value.integer.value[0] = data->target_size;

	vts_dev_info(component->dev, "GET Voice Trigger Value: %d\n",
			data->target_size);

	return 0;
}

static int set_voicetrigger_value(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;
	int active_trigger = data->active_trigger;
	u32 values[3];
	int result = 0;
	int trig_ms;

	pm_runtime_barrier(component->dev);

	trig_ms = ucontrol->value.integer.value[0];

	if (trig_ms > 2000 || trig_ms < 0) {
		vts_dev_err(component->dev,
		"Invalid Voice Trigger Value = %d (valid range 0~2000ms)",
			trig_ms);
		return 0;
	}

	/* Configure VTS target size */
	/* 1ms requires (16KHz,16bit,Mono) = 16samples * 2 bytes = 32 bytes*/
	values[0] = trig_ms * 32;
	values[1] = active_trigger;
	values[2] = 0;
	result = vts_start_ipc_transaction(component->dev, data,
		VTS_IRQ_AP_TARGET_SIZE, &values, 0, 1);
	if (result < 0) {
		vts_dev_err(component->dev, "Voice Trigger Value setting IPC Transaction Failed: %d\n",
			result);
		return result;
	}

	data->target_size = trig_ms;
	vts_dev_info(component->dev, "SET Voice Trigger Value: %dms\n",
		data->target_size);

	return 0;
}

static int get_vtsforce_reset(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;

	ucontrol->value.integer.value[0] = data->running;

	vts_dev_dbg(component->dev, "GET VTS Force Reset: %s\n",
			(data->running ? "VTS Running" :
			"VTS Not Running"));

	return 0;
}

static int set_vtsforce_reset(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = p_vts_data;

	vts_dev_info(dev, "%s: VTS RESET\n", __func__);

	while (data->running && pm_runtime_active(dev)) {
		vts_dev_warn(dev, "%s: clear active models\n", __func__);
		pm_runtime_put_sync(dev);
	}

	return  0;
}

static int get_force_trigger(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* Nothing To Do */

	return 0;
}

static int set_force_trigger(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component
		= snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = p_vts_data;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	u32 values[3];
	int result = 0;
	int val = ucontrol->value.integer.value[0];

	if (item[0] >= e->items) {
		vts_dev_err(dev,
			"%s try to set %d but should be under %d\n",
			__func__, item[0], e->items);
		return -EINVAL;
	}

	values[0] = VTS_FORCE_TRIGGER;
	values[1] = val;
	values[2] = 0;
	result = vts_start_ipc_transaction(dev, data,
					VTS_IRQ_AP_COMMAND,
					&values, 0, 0);
	if (result < 0) {
		vts_dev_err(dev, "Force Trigger setting IPC Transaction Failed: %d\n",
			result);
		return result;
	}

	vts_dev_info(dev, "Force Trigger Command, command num : %d\n", val);

	return  0;
}

static int get_supported_mic_num(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct vts_data *data = p_vts_data;

	ucontrol->value.integer.value[0] = data->supported_mic_num;

	return 0;
}

static int set_supported_mic_num(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = p_vts_data;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;

	if (item[0] >= e->items) {
		vts_dev_err(dev,
			"%s try to set %d but should be under %d\n",
			__func__, item[0], e->items);
		return -EINVAL;
	}

	data->supported_mic_num = (unsigned int)ucontrol->value.integer.value[0];
	vts_dev_info(dev, "%s: %d\n", __func__, data->supported_mic_num);

	return 0;
}

static int get_sysclk_div(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct vts_data *data = p_vts_data;

	ucontrol->value.integer.value[0] = data->sysclk_div;

	return 0;
}

static int set_sysclk_div(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = component->dev;
	struct vts_data *data = p_vts_data;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;

	if (item[0] >= e->items) {
		vts_dev_err(dev,
			"%s try to set %d but should be under %d\n",
			__func__, item[0], e->items);
		return -EINVAL;
	}

	data->sysclk_div = (unsigned int)ucontrol->value.integer.value[0];
	data->target_sysclk = VTS_SYS_CLOCK_MAX / data->sysclk_div;

	vts_dev_info(dev, "%s: sysclk div(%d) target_sysclk(%d)\n",
			   __func__, data->sysclk_div, data->target_sysclk);

	return 0;
}

static const struct snd_kcontrol_new vts_controls[] = {
	SOC_SINGLE("PERIOD DATA2REQ", VTS_DMIC_ENABLE_DMIC_IF,
		VTS_DMIC_PERIOD_DATA2REQ_OFFSET, 3, 0),
	SOC_SINGLE("HPF EN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_HPF_EN_OFFSET, 1, 0),
	SOC_ENUM("HPF SEL", vts_hpf_sel),
	SOC_ENUM("CPS SEL", vts_cps_sel),
	SOC_SINGLE_TLV("GAIN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_GAIN_OFFSET, 4, 0, vts_gain_tlv_array),
	SOC_SINGLE("1DB GAIN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_1DB_GAIN_OFFSET, 5, 0),
	SOC_ENUM_EXT("SYS SEL", vts_sys_sel, snd_soc_get_enum_double,
		vts_sys_sel_put_enum),
	SOC_ENUM("POLARITY CLK", vts_polarity_clk),
	SOC_ENUM("POLARITY OUTPUT", vts_polarity_output),
	SOC_ENUM("POLARITY INPUT", vts_polarity_input),
	SOC_ENUM("OVFW CTRL", vts_ovfw_ctrl),
	SOC_ENUM("CIC SEL", vts_cic_sel),
	SOC_ENUM_EXT("VoiceRecognization Mode", vtsvcrecog_mode_enum,
		get_vtsvoicerecognize_mode, set_vtsvoicerecognize_mode),
	SOC_ENUM_EXT("Active Keyphrase", vtsactive_phrase_enum,
		get_vtsactive_phrase, set_vtsactive_phrase),
	SOC_SINGLE_EXT("VoiceTrigger Value",
		SND_SOC_NOPM,
		0, 2000, 0,
		get_voicetrigger_value, set_voicetrigger_value),
	SOC_ENUM_EXT("Force Reset", vtsforce_reset_enum,
		get_vtsforce_reset, set_vtsforce_reset),
	SOC_SINGLE_EXT("Force Trigger",
		SND_SOC_NOPM,
		0, 1000, 0,
		get_force_trigger, set_force_trigger),
	SOC_SINGLE_EXT("Supported Mic Num",
		SND_SOC_NOPM,
		0, 3, 0,
		get_supported_mic_num, set_supported_mic_num),
	SOC_SINGLE_EXT("SYSCLK DIV",
		SND_SOC_NOPM,
		0, 10, 0,
		get_sysclk_div, set_sysclk_div),
};

static int vts_dmic_sel_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct vts_data *data = p_vts_data;
	unsigned int dmic_sel;

	dmic_sel = ucontrol->value.enumerated.item[0];
	if (dmic_sel > 1)
		return -EINVAL;

	vts_info("[VTS]%s : VTS DMIC SEL: %d\n", __func__, dmic_sel);

	data->dmic_if = dmic_sel;

	return  0;
}

static const char * const dmic_sel_texts[] = {"DPDM", "APDM"};
static SOC_ENUM_SINGLE_EXT_DECL(dmic_sel_enum, dmic_sel_texts);

static const struct snd_kcontrol_new dmic_sel_controls[] = {
	SOC_DAPM_ENUM_EXT("MUX", dmic_sel_enum,
			snd_soc_dapm_get_enum_double, vts_dmic_sel_put),
};

static const struct snd_kcontrol_new dmic_if_controls[] = {
	SOC_DAPM_SINGLE("RCH EN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_RCH_EN_OFFSET, 1, 0),
	SOC_DAPM_SINGLE("LCH EN", VTS_DMIC_CONTROL_DMIC_IF,
		VTS_DMIC_LCH_EN_OFFSET, 1, 0),
};

static const struct snd_soc_dapm_widget vts_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("PAD APDM"),
	SND_SOC_DAPM_INPUT("PAD DPDM"),
	SND_SOC_DAPM_MUX("DMIC SEL", SND_SOC_NOPM, 0, 0, dmic_sel_controls),
	SOC_MIXER_ARRAY("DMIC IF", SND_SOC_NOPM, 0, 0, dmic_if_controls),
};

static const struct snd_soc_dapm_route vts_dapm_routes[] = {
	// sink, control, source
	{"DMIC SEL", "APDM", "PAD APDM"},
	{"DMIC SEL", "DPDM", "PAD DPDM"},
	{"DMIC IF", "RCH EN", "DMIC SEL"},
	{"DMIC IF", "LCH EN", "DMIC SEL"},
	{"VTS Capture", NULL, "DMIC IF"},
};

int vts_set_dmicctrl(struct platform_device *pdev, int micconf_type, bool enable)
{
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	int ctrl_dmicif = 0;
	int select_dmicclk = 0;

	vts_dev_dbg(dev, "%s-- flag: %d mictype: %d micusagecnt: %d\n",
			 __func__, enable, micconf_type, data->micclk_init_cnt);

	if (!data->vts_ready) {
		vts_dev_warn(dev, "%s: VTS Firmware Not running\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		if (!data->micclk_init_cnt) {
			ctrl_dmicif = readl(data->dmic_if0_base + VTS_DMIC_CONTROL_DMIC_IF);

			if (IS_ENABLED(CONFIG_SOC_EXYNOS2100)
			    || IS_ENABLED(CONFIG_SOC_S5E9925) || IS_ENABLED(CONFIG_SOC_S5E8825)) {
				clear_bits(ctrl_dmicif, 7, VTS_DMIC_SYS_SEL_OFFSET);
				set_bits(ctrl_dmicif, data->syssel_rate, VTS_DMIC_SYS_SEL_OFFSET);
				vts_dev_info(dev, "%s DMIC IF SYS_SEL : %d\n",
					__func__, data->syssel_rate);
			}

			if (data->dmic_if == APDM) {
				vts_port_cfg(dev, VTS_PORT_PAD0,
						VTS_PORT_ID_VTS, APDM, true);
				set_bit(select_dmicclk,
					VTS_ENABLE_CLK_GEN_OFFSET);
				set_bit(select_dmicclk,
					VTS_SEL_EXT_DMIC_CLK_OFFSET);
				set_bit(select_dmicclk,
					VTS_ENABLE_CLK_CLK_GEN_OFFSET);

				/* Set AMIC VTS Gain */
				set_bits(ctrl_dmicif, data->dmic_if,
					VTS_DMIC_DMIC_SEL_OFFSET);
				set_bits(ctrl_dmicif, data->amicgain,
					VTS_DMIC_GAIN_OFFSET);
			} else {
				vts_port_cfg(dev, VTS_PORT_PAD0,
						VTS_PORT_ID_VTS, DPDM, true);

				if (data->supported_mic_num == MIC_NUM2)
					vts_port_cfg(dev, VTS_PORT_PAD1, VTS_PORT_ID_VTS, DPDM, true);

				clear_bit(select_dmicclk,
					VTS_ENABLE_CLK_GEN_OFFSET);
				clear_bit(select_dmicclk,
					VTS_SEL_EXT_DMIC_CLK_OFFSET);
				clear_bit(select_dmicclk,
					VTS_ENABLE_CLK_CLK_GEN_OFFSET);

				/* Set DMIC VTS Gain */
				set_bits(ctrl_dmicif, data->dmic_if,
					VTS_DMIC_DMIC_SEL_OFFSET);
				set_bits(ctrl_dmicif,
					data->dmicgain, VTS_DMIC_GAIN_OFFSET);
			}
			writel(select_dmicclk,
				data->sfr_base + VTS_DMIC_CLK_CON);

			writel(ctrl_dmicif,
				data->dmic_if0_base + VTS_DMIC_CONTROL_DMIC_IF);

			if (data->supported_mic_num == MIC_NUM2)
				writel(ctrl_dmicif, data->dmic_if1_base + VTS_DMIC_CONTROL_DMIC_IF);

		}

		/* check whether Mic is already configure or not based on VTS
		 * option type for MIC configuration book keeping
		 */
		if (!(data->mic_ready & (0x1 << micconf_type))) {
			data->micclk_init_cnt++;
			data->mic_ready |= (0x1 << micconf_type);
			vts_dev_info(dev, "%s Micclk ENABLED for mic_ready ++ %d\n",
				 __func__, data->mic_ready);
		}
	} else {
		if (data->micclk_init_cnt)
			data->micclk_init_cnt--;

		if (!data->micclk_init_cnt) {
			if (data->clk_path != VTS_CLK_SRC_AUD0) {
				vts_port_cfg(dev, VTS_PORT_PAD0,
						VTS_PORT_ID_VTS, DPDM, false);

				if (data->supported_mic_num == MIC_NUM2)
					vts_port_cfg(dev, VTS_PORT_PAD1, VTS_PORT_ID_VTS, DPDM, false);
			}

			/* reset VTS Gain to default */
			writel(0x0, data->dmic_if0_base + VTS_DMIC_CONTROL_DMIC_IF);

			if (data->supported_mic_num == MIC_NUM2)
				writel(0x0, data->dmic_if1_base + VTS_DMIC_CONTROL_DMIC_IF);

			vts_dev_info(dev, "%s Micclk setting DISABLED\n",
				__func__);
		}

		/* MIC configuration book keeping */
		if (data->mic_ready & (0x1 << micconf_type)) {
			data->mic_ready &= ~(0x1 << micconf_type);
			vts_dev_info(dev, "%s Micclk DISABLED for mic_ready -- %d\n",
				 __func__, data->mic_ready);
		}
	}

	return 0;
}
EXPORT_SYMBOL(vts_set_dmicctrl);

static irqreturn_t vts_error_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	u32 error_cfsr, error_hfsr, error_dfsr, error_afsr, error_bfsr;
	u32 error_code;

	vts_mailbox_read_shared_register(data->pdev_mailbox,
						&error_code, 3, 1);

	switch (error_code) {
	case VTS_ERR_HARD_FAULT:
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_cfsr, 1, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_hfsr, 2, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_dfsr, 4, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_afsr, 5, 1);
		vts_dev_err(dev, "HardFault CFSR 0x%x\n", (int)error_cfsr);
		vts_dev_err(dev, "HardFault HFSR 0x%x\n", (int)error_hfsr);
		vts_dev_err(dev, "HardFault DFSR 0x%x\n", (int)error_dfsr);
		vts_dev_err(dev, "HardFault AFSR 0x%x\n", (int)error_afsr);

		break;
	case VTS_ERR_BUS_FAULT:
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_cfsr, 1, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_hfsr, 2, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_dfsr, 4, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_bfsr, 5, 1);
		vts_dev_err(dev, "BusFault CFSR 0x%x\n", (int)error_cfsr);
		vts_dev_err(dev, "BusFault HFSR 0x%x\n", (int)error_hfsr);
		vts_dev_err(dev, "BusFault DFSR 0x%x\n", (int)error_dfsr);
		vts_dev_err(dev, "BusFault BFSR 0x%x\n", (int)error_bfsr);

		break;
	default:
		vts_dev_warn(dev, "undefined error_code: %d\n", error_code);

		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_cfsr, 1, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_hfsr, 2, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_dfsr, 4, 1);
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							&error_afsr, 5, 1);
		vts_dev_err(dev, "0x%x 0x%x 0x%x 0x%x\n",
				(int)error_cfsr, (int)error_hfsr,
				(int)error_dfsr, (int)error_afsr);
		break;
	}
	vts_ipc_ack(data, 1);
	vts_dev_err(dev, "Error occurred on VTS: 0x%x\n", (int)error_code);
#ifdef VTS_SICD_CHECK
	vts_dev_err(dev, "SOC down : %d, MIF down : %d",
		readl(data->sicd_base + SICD_SOC_DOWN_OFFSET),
		readl(data->sicd_base + SICD_MIF_DOWN_OFFSET));
#endif

	/* Dump VTS GPR register & SRAM */
	vts_dbg_dump_fw_gpr(dev, data, VTS_FW_ERROR);
	vts_dev_err(dev, "shared_info0x%x, 0x%x 0x%x",
		data->shared_info->vendor_data[0],
		data->shared_info->vendor_data[1],
		data->shared_info->vendor_data[2]);

	/* log dump */
	print_hex_dump(KERN_ERR, "vts-fw-log", DUMP_PREFIX_OFFSET, 32, 4, data->sramlog_baseaddr,
		VTS_SRAM_EVENTLOG_SIZE_MAX, true);
	print_hex_dump(KERN_ERR, "vts-time-log", DUMP_PREFIX_OFFSET, 32, 4,
		data->sramlog_baseaddr + VTS_SRAM_EVENTLOG_SIZE_MAX,
		VTS_SRAM_TIMELOG_SIZE_MAX, true);

	switch (error_code) {
	case VTS_ERR_HARD_FAULT:
	case VTS_ERR_BUS_FAULT:
		dbg_snapshot_expire_watchdog();

		break;
	default:
		vts_reset_cpu(dev);
		break;
	}

	return IRQ_HANDLED;
}

static irqreturn_t vts_boot_completed_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);

	data->vts_ready = 1;

	vts_ipc_ack(data, 1);
	wake_up(&data->ipc_wait_queue);

	vts_dev_info(dev, "VTS boot completed\n");

	return IRQ_HANDLED;
}

static irqreturn_t vts_ipc_received_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	u32 result;

	mailbox_read_shared_register(data->pdev_mailbox, &result, 3, 1);
	vts_dev_info(dev, "VTS received IPC: 0x%x\n", result);

	switch (data->ipc_state_ap) {
	case SEND_MSG:
		if (result  == (0x1 << data->running_ipc)) {
			vts_dev_dbg(dev, "IPC transaction completed\n");
			data->ipc_state_ap = SEND_MSG_OK;
		} else {
			vts_dev_err(dev, "IPC transaction error\n");
			data->ipc_state_ap = SEND_MSG_FAIL;
		}
		break;
	default:
		vts_dev_warn(dev, "State fault: %d Ack_value:0x%x\n",
				data->ipc_state_ap, result);
		break;
	}

	return IRQ_HANDLED;
}

static irqreturn_t vts_voice_triggered_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	u32 id, score, frame_count;

	vts_dev_info(dev, "%s: enter\n", __func__);

	vts_mailbox_read_shared_register(data->pdev_mailbox,
					&id, 3, 1);
	vts_ipc_ack(data, 1);
	frame_count = (u32)(id & GENMASK(15, 0));
	score = (u32)((id & GENMASK(27, 16)) >> 16);
	id >>= 28;

	if (data->mic_ready & (1 << id)) {
		vts_dev_info(dev, "VTS triggered: id = %u, score = %u\n",
				id, score);
		vts_dev_info(dev, "VTS triggered: frame_count = %u\n",
				 frame_count);

		/* trigger event should be sent with triggered id */
		data->poll_event_type |= (EVENT_TRIGGERED|EVENT_READY) + id;
		wake_up(&data->poll_wait_queue);
		__pm_wakeup_event(data->wake_lock, VTS_TRIGGERED_TIMEOUT_MS);
		data->vts_state = VTS_STATE_RECOG_TRIGGERED;
	}

	return IRQ_HANDLED;
}

static void vts_update_kernel_time(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned long long kernel_time = sched_clock();

	data->shared_info->kernel_sec = (u32)(kernel_time / 1000000000);
	data->shared_info->kernel_msec = (u32)(kernel_time % 1000000000 / 1000000);
}

static irqreturn_t vts_trigger_period_elapsed_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	struct vts_dma_data *dma_data =
		platform_get_drvdata(data->pdev_vtsdma[0]);
	u32 pointer;

	if (data->mic_ready && (data->mic_ready & ~(1 << VTS_MICCONF_FOR_RECORD))) {
		vts_mailbox_read_shared_register(data->pdev_mailbox,
					 &pointer, 2, 1);
		vts_dev_dbg(dev, "%s:[%s] Base: %08x pointer:%08x\n",
			 __func__, (dma_data->id ? "VTS-RECORD" :
			 "VTS-TRIGGER"), data->dma_area_vts, pointer);

		if (pointer)
			dma_data->pointer = (pointer -
					 data->dma_area_vts);
		vts_ipc_ack(data, 1);
#ifdef DEF_VTS_PCM_DUMP
		if (vts_pcm_dump_get_file_started(1))
			vts_pcm_dump_period_elapsed(1, dma_data->pointer);
#endif
		snd_pcm_period_elapsed(dma_data->substream);
	}

	return IRQ_HANDLED;
}

static irqreturn_t vts_record_period_elapsed_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	struct vts_dma_data *dma_data =
		platform_get_drvdata(data->pdev_vtsdma[1]);
	u32 pointer;

	if (data->vts_state == VTS_STATE_RUNTIME_SUSPENDING ||
		data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_NONE) {
		vts_dev_warn(dev, "%s: VTS wrong state\n", __func__);
		return IRQ_HANDLED;
	}

	if (data->mic_ready & (0x1 << VTS_MICCONF_FOR_RECORD)) {
		vts_mailbox_read_shared_register(data->pdev_mailbox,
						 &pointer, 1, 1);
		vts_dev_dbg(dev, "%s:[%s] 0x%x:0x%x\n",
			 __func__,
			 (dma_data->id ? "REC" : "TRI"),
			 (data->dma_area_vts + BUFFER_BYTES_MAX/2), pointer);

		if (pointer)
			dma_data->pointer = (pointer -
				(data->dma_area_vts + BUFFER_BYTES_MAX/2));
		vts_ipc_ack(data, 1);
#ifdef DEF_VTS_PCM_DUMP
		if (vts_pcm_dump_get_file_started(0))
			vts_pcm_dump_period_elapsed(0, dma_data->pointer);
#endif
		snd_pcm_period_elapsed(dma_data->substream);
	}

	return IRQ_HANDLED;
}

static irqreturn_t vts_debuglog_bufzero_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);

	if (!data->running) {
		vts_dev_err(dev, "%s: wrong status\n", __func__);
		return IRQ_HANDLED;
	}
	vts_dev_dbg(dev, "%s LogBuffer Index: %d\n", __func__, 0);

	/* schedule log dump */
	vts_log_schedule_flush(dev, 0);

	return IRQ_HANDLED;
}

static irqreturn_t vts_debuglog_bufone_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);

	if (!data->running) {
		vts_dev_err(dev, "%s: wrong status\n", __func__);
		return IRQ_HANDLED;
	}
	vts_dev_dbg(dev, "%s LogBuffer Index: %d\n", __func__, 1);

	/* schedule log dump */
	vts_log_schedule_flush(dev, 1);

	vts_ipc_ack(data, 1);

	return IRQ_HANDLED;
}

static irqreturn_t vts_audiodump_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	/* u32 pointer; */

	vts_dev_dbg(dev, "%s\n", __func__);

	if (data->vts_ready && data->audiodump_enabled) {
		u32 ackvalues[3] = {0, 0, 0};

		mailbox_read_shared_register(data->pdev_mailbox,
			ackvalues, 0, 2);
		vts_dev_info(dev, "%sDump offset: 0x%x size:0x%x\n",
			__func__, ackvalues[0], ackvalues[1]);
		/* register audio dump offset & size */
		vts_dump_addr_register(dev, ackvalues[0],
				ackvalues[1], VTS_AUDIO_DUMP);
		/* schedule pcm dump */
		vts_audiodump_schedule_flush(dev);
		/* vts_ipc_ack should be sent once dump is completed */
	} else if (IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_SLIF)
		   && data->vts_ready && data->slif_dump_enabled) {
//		vts_mailbox_read_shared_register(data->pdev_mailbox,
//						 &pointer, 1, 1);
//		vts_dev_info(dev, "audiodump[%08x:%08x][p:%08x]\n",
//			 data->dma_area_vts,
//			 (data->dma_area_vts + BUFFER_BYTES_MAX/2),
//			 pointer);
//		vts_ipc_ack(data, 1);
		/* vts_s_lif_dump_period_elapsed(1, pointer); */
	} else {
		vts_ipc_ack(data, 1);
	}

	return IRQ_HANDLED;
}

static irqreturn_t vts_logdump_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);

	vts_dev_info(dev, "%s\n", __func__);

	if (data->vts_ready && data->logdump_enabled) {
		/* schedule pcm dump */
		vts_logdump_schedule_flush(dev);
		/* vts_ipc_ack should be sent once dump is completed */
	} else {
		vts_ipc_ack(data, 1);
	}

	return IRQ_HANDLED;
}

void vts_register_dma(struct platform_device *pdev_vts,
		struct platform_device *pdev_vts_dma, unsigned int id)
{
	struct vts_data *data = platform_get_drvdata(pdev_vts);

	if (id < ARRAY_SIZE(data->pdev_vtsdma)) {
		data->pdev_vtsdma[id] = pdev_vts_dma;
		if (id > data->vtsdma_count)
			data->vtsdma_count = id + 1;
		vts_dev_info(&data->pdev->dev, "%s: VTS-DMA id(%u)Registered\n",
			__func__, id);
	} else {
		vts_dev_err(&data->pdev->dev, "%s: invalid id(%u)\n",
			__func__, id);
	}
}
EXPORT_SYMBOL(vts_register_dma);

static int vts_suspend(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	u32 values[3] = {0, 0, 0};
	int result = 0;

	if (data->vts_ready) {
		if (data->running &&
			data->vts_state == VTS_STATE_RECOG_TRIGGERED) {
			result = vts_start_ipc_transaction(dev, data,
					VTS_IRQ_AP_RESTART_RECOGNITION,
					&values, 0, 1);
			if (result < 0) {
				vts_dev_err(dev, "%s restarted trigger failed\n",
					__func__);
				goto error_ipc;
			}
			data->vts_state = VTS_STATE_RECOG_STARTED;
		}

		/* enable vts wakeup source interrupts */
		enable_irq_wake(data->irq[VTS_IRQ_VTS_VOICE_TRIGGERED]);
		enable_irq_wake(data->irq[VTS_IRQ_VTS_ERROR]);
#ifdef TEST_WAKEUP
		enable_irq_wake(data->irq[VTS_IRQ_VTS_CP_WAKEUP]);
#endif
		if (data->audiodump_enabled)
			enable_irq_wake(data->irq[VTS_IRQ_VTS_AUDIO_DUMP]);
		if (data->logdump_enabled)
			enable_irq_wake(data->irq[VTS_IRQ_VTS_LOG_DUMP]);

		vts_dev_info(dev, "%s: Enable VTS Wakeup source irqs\n",
			__func__);
	}
	vts_dev_info(dev, "%s: TEST\n", __func__);
error_ipc:
	return result;
}

static int vts_resume(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);

	if (data->vts_ready) {
		/* disable vts wakeup source interrupts */
		disable_irq_wake(data->irq[VTS_IRQ_VTS_VOICE_TRIGGERED]);
		disable_irq_wake(data->irq[VTS_IRQ_VTS_ERROR]);
#ifdef TEST_WAKEUP
		disable_irq_wake(data->irq[VTS_IRQ_VTS_CP_WAKEUP]);
#endif
		if (data->audiodump_enabled)
			disable_irq_wake(data->irq[VTS_IRQ_VTS_AUDIO_DUMP]);
		if (data->logdump_enabled)
			disable_irq_wake(data->irq[VTS_IRQ_VTS_LOG_DUMP]);

		vts_dev_info(dev, "%s: Disable VTS Wakeup source irqs\n",
			__func__);
	}
	return 0;
}

static void vts_irq_enable(struct platform_device *pdev, bool enable)
{
	struct vts_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int irqidx;

	vts_dev_info(dev, "%s IRQ Enable: [%s]\n", __func__,
			(enable ? "TRUE" : "FALSE"));

	for (irqidx = 0; irqidx < VTS_IRQ_COUNT; irqidx++) {
		if (enable)
			enable_irq(data->irq[irqidx]);
		else
			disable_irq(data->irq[irqidx]);
	}
#ifdef TEST_WAKEUP
	if (enable)
		enable_irq(data->irq[VTS_IRQ_VTS_CP_WAKEUP]);
	else
		disable_irq(data->irq[VTS_IRQ_VTS_CP_WAKEUP]);
#endif
}

static void vts_save_register(struct vts_data *data)
{
	regcache_cache_only(data->regmap_dmic, true);
	regcache_mark_dirty(data->regmap_dmic);
}

static void vts_restore_register(struct vts_data *data)
{
	regcache_cache_only(data->regmap_dmic, false);
	regcache_sync(data->regmap_dmic);
}

static void vts_memlog_sync_to_file(struct device *dev, struct vts_data *data)
{
	if (data->kernel_log_obj)
		vts_dev_info(dev, "%s kernel_log_obj sync : %d",
			__func__, memlog_sync_to_file(data->kernel_log_obj));

	if (data->fw_log_obj)
		vts_dev_info(dev, "%s fw_log_obj sync : %d",
			__func__, memlog_sync_to_file(data->fw_log_obj));
}

void vts_dbg_dump_fw_gpr(struct device *dev, struct vts_data *data, unsigned int dbg_type)
{
#if defined(CONFIG_SOC_S5E8825)
	int i;
#endif
	vts_dev_dbg(dev, "%s\n", __func__);

	switch (dbg_type) {
	case RUNTIME_SUSPEND_DUMP:
		if (!vts_is_on() || !data->running) {
			vts_dev_info(dev, "%s is skipped due to no power\n",
				__func__);
			return;
		}
#if defined(CONFIG_SOC_S5E9925)
		if (data->dump_obj)
			memlog_do_dump(data->dump_obj, MEMLOG_LEVEL_INFO);
#endif
		vts_memlog_sync_to_file(dev, data);

		/* Save VTS firmware log msgs */
		if (!IS_ERR_OR_NULL(data->p_dump[dbg_type].sram_log)
		    && !IS_ERR_OR_NULL(data->sramlog_baseaddr)) {
			memcpy(data->p_dump[dbg_type].sram_log, VTS_DUMP_MAGIC, sizeof(VTS_DUMP_MAGIC));
			memcpy_fromio(data->p_dump[dbg_type].sram_log +
				sizeof(VTS_DUMP_MAGIC), data->sramlog_baseaddr, VTS_SRAMLOG_SIZE_MAX);
		}
		break;
	case KERNEL_PANIC_DUMP:
	case VTS_ITMON_ERROR:
		if (!data->running) {
			vts_dev_info(dev, "%s is skipped due to not running\n",
				__func__);
			return;
		}
	case VTS_FW_NOT_READY:
	case VTS_IPC_TRANS_FAIL:
	case VTS_FW_ERROR:
#if defined(CONFIG_SOC_S5E8825)
		for (i = 0; i <= 15; i++) {
			vts_dev_info(dev, "R%d: %x\n", i, readl(data->gpr_base + VTS_CM4_R(i)));
			data->p_dump[dbg_type].gpr[i] = readl(data->gpr_base + VTS_CM4_R(i));
		}

		vts_dev_info(dev, "PC: %x\n", readl(data->gpr_base + VTS_CM4_PC));
		data->p_dump[dbg_type].gpr[i++] = readl(data->gpr_base + VTS_CM4_PC);
#else
		vts_dev_info(dev, "PC: 0x%x\n", readl(data->gpr_base + 0x0));
		data->p_dump[dbg_type].gpr[0] = readl(data->gpr_base + 0x0);
#endif
		if (data->dump_obj &&
			(dbg_type == VTS_IPC_TRANS_FAIL ||
			dbg_type == VTS_FW_ERROR)) {
			memlog_do_dump(data->dump_obj,
				MEMLOG_LEVEL_ERR);
			vts_memlog_sync_to_file(dev, data);
		}

		/* Save VTS firmware log msgs */
		if (!IS_ERR_OR_NULL(data->p_dump[dbg_type].sram_log) &&
			!IS_ERR_OR_NULL(data->sramlog_baseaddr)) {
			memcpy(data->p_dump[dbg_type].sram_log,
				VTS_DUMP_MAGIC, sizeof(VTS_DUMP_MAGIC));
			memcpy_fromio(data->p_dump[dbg_type].sram_log +
				sizeof(VTS_DUMP_MAGIC),
				data->sramlog_baseaddr,
				VTS_SRAMLOG_SIZE_MAX);
		}

		/* Save VTS firmware all */
		if (!IS_ERR_OR_NULL(data->p_dump[dbg_type].sram_fw) &&
			!IS_ERR_OR_NULL(data->sram_base)) {
			memcpy_fromio(data->p_dump[dbg_type].sram_fw,
				data->sram_base, data->sram_size);
		}
		break;
	default:
		vts_dev_info(dev, "%s is skipped due to invalid debug type\n",
			__func__);
		return;
	}
	data->p_dump[dbg_type].time = sched_clock();
	data->p_dump[dbg_type].dbg_type = dbg_type;
}

static void exynos_vts_panic_handler(void)
{
	static bool has_run;
	struct vts_data *data = p_vts_data;
	struct device *dev =
		data ? (data->pdev ? &data->pdev->dev : NULL) : NULL;

	vts_dev_dbg(dev, "%s\n", __func__);

	if (vts_is_on() && dev) {
		if (has_run) {
			vts_dev_info(dev, "already dumped\n");
			return;
		}
		has_run = true;

		/* Dump VTS GPR register & SRAM */
		vts_dbg_dump_fw_gpr(dev, data, KERNEL_PANIC_DUMP);
	} else {
		vts_dev_info(dev, "%s: dump is skipped due to no power\n",
			__func__);
	}
}

static int vts_panic_handler(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	exynos_vts_panic_handler();
	return NOTIFY_OK;
}

static struct notifier_block vts_panic_notifier = {
	.notifier_call	= vts_panic_handler,
	.next		= NULL,
	.priority	= 0	/* priority: INT_MAX >= x >= 0 */
};

#define SLIF_SEL_VTS	(0x824) /* 0x15510824 */
static int vts_do_reg(struct device *dev, bool enable, int cmd)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (IS_ENABLED(CONFIG_SOC_S5E8825)) {
		if (enable)
			writel(0x1, data->gpr_base);  /* enable DUMP_GPR */

		return ret;
	}

	if (!IS_ENABLED(CONFIG_SOC_S5E9925_EVT0))
		return ret;

	/* HACK */
	if (enable) {
		/* SYSREG_VTS + 0x824 */
		/*0 : SERIAL_LIF, 1 : DMIC_AHB */
		writel(0x0, data->sfr_base + SLIF_SEL_VTS);

		/* SL_INT_EN_SET : 0x040 */
		/* [9:8] Interrupt ENABLE SL_READ_POINTER_SET0 & */
		/* SL_READ_POINTER_SET1 */
		/* writel(0x300, data->sfr_slif_vts + 0x40); */
		/* INPUT_EN: EN0, EN1 */
		/* writel(0x3, data->sfr_slif_vts + 0x114); */
		/* CONFIG_MASTER: 16bit, 2ch */
		/* writel(0x00102009, data->sfr_slif_vts + 0x104); */
		/* CHANNEL_MAP: 0 1 0 1 0 1 0 1 */
		/* writel(0x20202020, data->sfr_slif_vts + 0x138); */
		/* writel(0x1111, data->sfr_slif_vts + 0x100); */
	} else {
		writel(0x0, data->sfr_base + SLIF_SEL_VTS);
		/* writel(0x0, data->sfr_slif_vts + 0x40); */
		/* writel(0x0, data->sfr_slif_vts + 0x114); */
		/* writel(0x00102009, data->sfr_slif_vts + 0x104); */
		/* writel(0x76543210, data->sfr_slif_vts + 0x138); */
		/* writel(0x0, data->sfr_slif_vts + 0x100); */

		/* writel(0x0, data->sfr_slif_vts + 0x100); */
		/* writel(0x0, data->sfr_base + 0x1000); */
	}

	return ret;
}

static void vts_update_config(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);

	/* update forcely, if you want */
	if (data->target_sysclk != data->sysclk_rate) {
		int ret = 0;
		vts_dev_info(dev, "System Clock: %lu -> %lu\n",
			clk_get_rate(data->clk_sys), data->target_sysclk);
		ret = clk_set_rate(data->clk_sys, data->target_sysclk);
		if (ret < 0)
			vts_dev_err(dev, "failed set clk_sys: %d\n", ret);
	}

	vts_dev_dbg(dev, "System Clock target:%lu current:%lu\n",
			data->target_sysclk, clk_get_rate(data->clk_sys));
}

static void vts_check_version(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	u32 values[3];
	u32 ret_value[3];
	char *ver;
	int ret = 0;

	if (data->google_version == 0) {
		values[0] = 2;
		values[1] = 0;
		values[2] = 0;
		ret = vts_start_ipc_transaction(dev, data,
			VTS_IRQ_AP_GET_VERSION, &values, 0, 2);
		if (ret < 0)
			dev_err(dev, "VTS_IRQ_AP_GET_VERSION ipc failed\n");
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							ret_value, 0, 3);
		vts_dev_dbg(dev, "get version 0x%x 0x%x 0x%x\n",
			ret_value[0], ret_value[1], ret_value[2]);

		data->google_version = ret_value[2];
		memcpy(data->google_uuid, data->shared_info->hotword_id, 40);
	} else {
		vts_dev_dbg(dev, "google version = %d\n",
				data->google_version);
	}

	if (data->vtsdetectlib_version == 0) {
		values[0] = 1;
		values[1] = 0;
		values[2] = 0;

		ret = vts_start_ipc_transaction(dev, data,
			VTS_IRQ_AP_GET_VERSION, &values, 0, 2);
		if (ret < 0)
			dev_err(dev, "VTS_IRQ_AP_GET_VERSION ipc failed\n");
		vts_mailbox_read_shared_register(data->pdev_mailbox,
							ret_value, 0, 3);

		vts_dev_dbg(dev, "get version 0x%x 0x%x 0x%x\n",
			ret_value[0], ret_value[1], ret_value[2]);

		data->vtsfw_version = ret_value[0];
		data->vtsdetectlib_version = ret_value[2];
	}

	ver = (char *)(&data->vtsfw_version);
	vts_dev_info(dev, "firmware version: (%c%c%c%c) lib: 0x%x 0x%x\n",
		ver[3], ver[2], ver[1], ver[0],
		data->vtsdetectlib_version,
		data->google_version);
}

static int vts_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct vts_data *data = dev_get_drvdata(dev);
	u32 values[3] = {0, 0, 0};
	int result = 0;
	unsigned long flag;

	vts_dev_info(dev, "%s\n", __func__);
	if (data->sysevent_dev)
		sysevent_put((void *)data->sysevent_dev);

	if (data->running) {
		vts_dev_info(dev, "RUNNING %s :%d\n", __func__, __LINE__);
		if (data->audiodump_enabled) {
			values[0] = VTS_DISABLE_AUDIODUMP;
			values[1] = 0;
			values[2] = 0;
			result = vts_start_ipc_transaction(dev, data,
						VTS_IRQ_AP_COMMAND,
						&values, 0, 1);
			if (result < 0)
				vts_dev_warn(dev, "Disable_AudioDump ipc failed\n");
			/* reset audio dump offset & size */
			vts_dump_addr_register(dev, 0, 0, VTS_AUDIO_DUMP);
		}

		if (data->logdump_enabled) {
			values[0] = VTS_DISABLE_LOGDUMP;
			values[1] = 0;
			values[2] = 0;
			result = vts_start_ipc_transaction(dev, data,
						VTS_IRQ_AP_COMMAND,
						&values, 0, 1);
			if (result < 0)
				vts_dev_warn(dev, "Disable_LogDump ipc failed\n");
			/* reset audio dump offset & size */
			vts_dump_addr_register(dev, 0, 0, VTS_LOG_DUMP);
		}

		if (data->fw_logfile_enabled || data->fw_logger_enabled) {
			values[0] = VTS_DISABLE_DEBUGLOG;
			values[1] = 0;
			values[2] = 0;
			result = vts_start_ipc_transaction(dev, data,
					VTS_IRQ_AP_COMMAND,
					&values, 0, 1);
			if (result < 0)
				vts_dev_warn(dev, "Disable_debuglog ipc transaction failed\n");
			/* reset VTS SRAM debug log buffer */
			vts_register_log_buffer(dev, 0, 0);
		}
	}

	spin_lock_irqsave(&data->state_spinlock, flag);
	data->vts_state = VTS_STATE_RUNTIME_SUSPENDING;
	spin_unlock_irqrestore(&data->state_spinlock, flag);

	vts_dev_info(dev, "%s save register :%d\n", __func__, __LINE__);
	vts_save_register(data);
	if (data->running) {
		values[0] = 0;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev,
			data, VTS_IRQ_AP_POWER_DOWN, &values, 0, 1);
		if (result < 0) {
			vts_dev_warn(dev, "POWER_DOWN IPC transaction Failed\n");

			result = vts_start_ipc_transaction(dev,
				data, VTS_IRQ_AP_POWER_DOWN, &values, 0, 1);
			if (result < 0)
				vts_dev_warn(dev, "POWER_DOWN IPC transaction 2nd Failed\n");
		}

		/* Dump VTS GPR register & Log messages */
		vts_dbg_dump_fw_gpr(dev, data, RUNTIME_SUSPEND_DUMP);
		vts_dev_info(dev, "shared_info 0x%x, %d %d",
			data->shared_info->vendor_data[0],
			data->shared_info->vendor_data[1],
			data->shared_info->vendor_data[2]);

		vts_cpu_enable(dev, false);

		if (data->irq_state) {
			vts_irq_enable(pdev, false);
			data->irq_state = false;
		}

		vts_do_reg(dev, false, 0);

		result = vts_soc_runtime_suspend(dev);
		if (result < 0) {
			vts_dev_warn(dev, "vts_soc_rt_suspend %d\n", result);
		}

		vts_log_flush(&data->pdev->dev);

		spin_lock_irqsave(&data->state_spinlock, flag);
		data->vts_state = VTS_STATE_RUNTIME_SUSPENDED;
		spin_unlock_irqrestore(&data->state_spinlock, flag);

		vts_cpu_power(dev, false);
		data->running = false;
	} else {
		spin_lock_irqsave(&data->state_spinlock, flag);
		data->vts_state = VTS_STATE_RUNTIME_SUSPENDED;
		spin_unlock_irqrestore(&data->state_spinlock, flag);
	}

	vts_port_cfg(dev, VTS_PORT_PAD0, VTS_PORT_ID_VTS, DPDM, false);

	mailbox_update_vts_is_on(false);
	data->enabled = false;
	data->exec_mode = VTS_RECOGNIZE_STOP;
	data->voicerecog_start = 0;
	data->target_size = 0;
	/* reset micbias setting count */
	data->micclk_init_cnt = 0;
	data->mic_ready = 0;
	data->vts_ready = 0;

	vts_dev_info(dev, "%s Exit\n", __func__);
	return 0;
}

int vts_start_runtime_resume(struct device *dev, int skip_log)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct vts_data *data = dev_get_drvdata(dev);
	u32 values[3];
	int result;
	unsigned long long kernel_time;

	if (data->running) {
		if (!skip_log)
			vts_dev_info(dev, "SKIP %s\n", __func__);
		return 0;
	}
	vts_dev_info(dev, "%s\n", __func__);

	if (!data->clk_sys_mux) {
#if defined(CONFIG_SOC_S5E8825)
		vts_dev_info(dev, "%s: don't have to set clk_sys_mux\n", __func__);
#else
		vts_dev_info(dev, "%s clk_sys_mux is NULL\n", __func__);
		return 0;
#endif
	}

	result = vts_soc_runtime_resume(dev);
	if (result < 0) {
		vts_dev_warn(dev, "vts_soc_rt_resume %d\n", result);
		goto error_clk;
	}

	data->vts_state = VTS_STATE_RUNTIME_RESUMING;
	data->enabled = true;
	mailbox_update_vts_is_on(true);

	vts_restore_register(data);
	vts_port_cfg(dev, VTS_PORT_PAD0, VTS_PORT_ID_VTS, DPDM, true);
	vts_port_cfg(dev, VTS_PORT_PAD0, VTS_PORT_ID_VTS, DPDM, false);
	vts_pad_retention(false);

	if (!data->irq_state) {
		vts_irq_enable(pdev, true);
		data->irq_state = true;
	}

	vts_do_reg(dev, true, 0);

	if (IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)) {
		vts_dev_info(dev, "imgloader_boot : %d\n", data->imgloader);
		if (data->imgloader)
			result = imgloader_boot(&data->vts_imgloader_desc);
	} else
		result = vts_download_firmware(pdev);

	if (result < 0) {
		vts_dev_err(dev, "Failed to download firmware\n");
		data->enabled = false;
		mailbox_update_vts_is_on(false);
		goto error_firmware;
	}

	if (IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)) {
		vts_dev_info(dev, "VTS IMGLOADER\n");
	} else {
		vts_cpu_power(dev, true);
	}
	vts_dev_info(dev, "GPR DUMP BASE Enable\n");

	result = vts_wait_for_fw_ready(dev);
	/* ADD FW READY STATUS */
	if (result < 0) {
		vts_dev_err(dev, "Failed to vts_wait_for_fw_ready\n");
		data->enabled = false;
		mailbox_update_vts_is_on(false);
		goto error_firmware;
	}

	vts_update_config(dev);
	vts_check_version(dev);

	/* Configure select sys clock rate */
	vts_clk_set_rate(dev, data->syssel_rate);

	data->dma_area_vts = vts_set_baaw(data->baaw_base,
			data->dmab.addr, BUFFER_BYTES_MAX);

	values[0] = data->dma_area_vts;
	values[1] = 0x140;
	values[2] = 0x800;
	result = vts_start_ipc_transaction(dev,
		data, VTS_IRQ_AP_SET_DRAM_BUFFER, &values, 0, 1);
	if (result < 0) {
		vts_dev_err(dev, "DRAM_BUFFER Setting IPC transaction Failed\n");
		goto error_firmware;
	}

	data->exec_mode = VTS_RECOGNIZE_STOP;

	values[0] = VTS_ENABLE_SRAM_LOG;
	values[1] = 0;
	values[2] = 0;
	result = vts_start_ipc_transaction(dev,
		data, VTS_IRQ_AP_COMMAND, &values, 0, 1);
	if (result < 0) {
		vts_dev_err(dev, "Enable_SRAM_log ipc transaction failed\n");
		goto error_firmware;
	}

	if (data->fw_logfile_enabled || data->fw_logger_enabled) {
		values[0] = VTS_ENABLE_DEBUGLOG;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(dev,
			data, VTS_IRQ_AP_COMMAND, &values, 0, 1);
		if (result < 0) {
			vts_dev_err(dev, "Enable_debuglog ipc transaction failed\n");
			goto error_firmware;
		}
	}

	/* Enable Audio data Dump */
	if (data->audiodump_enabled) {
		values[0] = VTS_ENABLE_AUDIODUMP;
		values[1] = (VTS_ADUIODUMP_AFTER_MINS * 60);
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_COMMAND, &values,
				0, 2);
		if (result < 0) {
			vts_dev_err(dev, "Enable_AudioDump ipc failed\n");
			goto error_firmware;
		}
	}

	/* Enable VTS FW log Dump */
	if (data->logdump_enabled) {
		values[0] = VTS_ENABLE_LOGDUMP;
		values[1] = (VTS_LOGDUMP_AFTER_MINS * 60);
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_COMMAND, &values,
				0, 2);
		if (result < 0) {
			vts_dev_err(dev, "Enable_LogDump ipc failed\n");
			goto error_firmware;
		}
	}

	vts_update_kernel_time(dev);
	/* Send Kernel Time */
	kernel_time = sched_clock();
	values[0] = VTS_KERNEL_TIME;
	values[1] = (u32)(kernel_time / 1000000000);
	values[2] = (u32)(kernel_time % 1000000000 / 1000000);
	vts_dev_info(dev, "Time : %d.%d\n", values[1], values[2]);
	result = vts_start_ipc_transaction(dev, data,
		VTS_IRQ_AP_COMMAND, &values, 0, 2);
	if (result < 0)
		vts_dev_err(dev, "VTS_KERNEL_TIME ipc failed\n");
	vts_dev_dbg(dev, "%s DRAM-setting and VTS-Mode is completed\n",
		__func__);
	vts_dev_info(dev, "%s Exit\n", __func__);
	data->running = true;
	data->vts_state = VTS_STATE_RUNTIME_RESUMED;

	return 0;

error_firmware:
	vts_cpu_power(dev, false);
	vts_soc_runtime_suspend(dev);

error_clk:
	if (data->irq_state) {
		vts_irq_enable(pdev, false);
		data->irq_state = false;
	}
	data->running = false;
	data->enabled = false;
	mailbox_update_vts_is_on(false);
	return 0;
}
EXPORT_SYMBOL(vts_start_runtime_resume);

static int vts_runtime_resume(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	void *retval = NULL;

	if (IS_ENABLED(CONFIG_SOC_EXYNOS2100)) {
		cmu_vts_rco_400_control(1);
		vts_dev_info(dev, "%s RCO400 ON\n", __func__);
	}
	if (data->sysevent_dev) {
		retval = sysevent_get(data->sysevent_desc.name);
		if (!retval)
			vts_dev_err(dev, "fail in sysevent_get\n");
	}
	data->enabled = true;
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops samsung_vts_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(vts_suspend, vts_resume)
	SET_RUNTIME_PM_OPS(vts_runtime_suspend, vts_runtime_resume, NULL)
};

static const struct of_device_id exynos_vts_of_match[] = {
	{
		.compatible = "samsung,vts",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_vts_of_match);

static ssize_t vtsfw_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned int version = data->vtsfw_version;

	buf[0] = ((version >> 24) & 0xFF);
	buf[1] = ((version >> 16) & 0xFF);
	buf[2] = ((version >> 8) & 0xFF);
	buf[3] = (version & 0xFF);
	buf[4] = '\n';
	buf[5] = '\0';

	return 6;
}

static ssize_t vtsdetectlib_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned int version = data->vtsdetectlib_version;

	buf[0] = (((version >> 24) & 0xFF) + '0');
	buf[1] = '.';
	buf[2] = (((version >> 16) & 0xFF) + '0');
	buf[3] = '.';
	buf[4] = ((version & 0xFF) + '0');
	buf[5] = '\n';
	buf[6] = '\0';

	return 7;
}

static ssize_t vts_audiodump_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct vts_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->audiodump_enabled);
}

static ssize_t vts_audiodump_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct vts_data *data = dev_get_drvdata(dev);
	u32 val = 0;
	u32 values[3] = {0, 0, 0};
	int result;
	int err = kstrtouint(buf, 0, &val);

	if (err < 0)
		return err;
	data->audiodump_enabled = (val ? true : false);

	if (data->vts_ready) {
		if (data->audiodump_enabled) {
			values[0] = VTS_ENABLE_AUDIODUMP;
			values[1] = (VTS_ADUIODUMP_AFTER_MINS * 60);
		} else {
			values[0] = VTS_DISABLE_AUDIODUMP;
			values[1] = 0;
		}
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_COMMAND, &values,
				0, 2);
		if (result < 0) {
			vts_dev_err(dev, "AudioDump[%d] ipc failed\n",
				data->audiodump_enabled);
		}
	}

	vts_dev_info(dev, "%s: Audio dump %sabled\n",
		__func__, (val ? "en" : "dis"));
	return count;
}

static ssize_t vts_logdump_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct vts_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->logdump_enabled);
}

static ssize_t vts_logdump_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct vts_data *data = dev_get_drvdata(dev);
	u32 val = 0;
	u32 values[3] = {0, 0, 0};
	int result;
	int err = kstrtouint(buf, 0, &val);

	if (err < 0)
		return err;

	data->logdump_enabled = (val ? true : false);

	if (data->vts_ready) {
		if (data->logdump_enabled) {
			values[0] = VTS_ENABLE_LOGDUMP;
			values[1] = (VTS_LOGDUMP_AFTER_MINS * 60);
		} else {
			values[0] = VTS_DISABLE_LOGDUMP;
			values[1] = 0;
		}
		values[2] = 0;
		result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_COMMAND, &values,
				0, 2);
		if (result < 0) {
			vts_dev_err(dev, "LogDump[%d] ipc failed\n",
				data->logdump_enabled);
		}
	}

	vts_dev_info(dev, "%s: Log dump %sabled\n",
		__func__, (val ? "en" : "dis"));
	return count;
}

static ssize_t vts_cmd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	static const char cmd_test_fwlog[] = "TESTFWLOG";
       struct vts_data *data = dev_get_drvdata(dev);

	vts_dev_info(dev, "%s(%s)\n", __func__, buf);

	if (!strncmp(cmd_test_fwlog, buf, sizeof(cmd_test_fwlog) - 1)) {
               vts_dev_info(dev, "%s: %s\n", __func__, buf);

               if (vts_is_on() && dev) {
			/* log dump */
			print_hex_dump(KERN_ERR, "vts-fw-log",
				DUMP_PREFIX_OFFSET, 32, 4, data->sramlog_baseaddr,
				VTS_SRAM_EVENTLOG_SIZE_MAX, true);

			print_hex_dump(KERN_ERR, "vts-time-log",
				DUMP_PREFIX_OFFSET, 32, 4,
				data->sramlog_baseaddr + VTS_SRAM_EVENTLOG_SIZE_MAX,
				VTS_SRAM_TIMELOG_SIZE_MAX, true);
               } else {
                       vts_dev_info(dev, "%s: no power\n", __func__);
               }
       }



	return count;
}
static DEVICE_ATTR_RO(vtsfw_version);
static DEVICE_ATTR_RO(vtsdetectlib_version);
static DEVICE_ATTR_RW(vts_audiodump);
static DEVICE_ATTR_RW(vts_logdump);
static DEVICE_ATTR_WO(vts_cmd);


static void __iomem *samsung_vts_membase_request_and_map(
	struct platform_device *pdev, const char *name, const char *size)
{
	const __be32 *prop;
	unsigned int len = 0;
	unsigned int data_base = 0;
	unsigned int data_size = 0;
	void __iomem *result = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	prop = of_get_property(np, name, &len);
	if (!prop) {
		vts_dev_err(&pdev->dev, "Failed to of_get_property %s\n", name);
		return ERR_PTR(-EFAULT);
	}
	data_base = be32_to_cpup(prop);
	prop = of_get_property(np, size, &len);
	if (!prop) {
		vts_dev_err(&pdev->dev, "Failed to of_get_property %s\n", size);
		return ERR_PTR(-EFAULT);
	}
	data_size = be32_to_cpup(prop);
	if (data_base && data_size) {
		result = ioremap(data_base, data_size);
		if (IS_ERR_OR_NULL(result)) {
			vts_dev_err(&pdev->dev, "Failed to map %s\n", name);
			return ERR_PTR(-EFAULT);
		}
	}
	return result;
}

static void __iomem *samsung_vts_devm_request_and_map(
	struct platform_device *pdev, const char *name,
	phys_addr_t *phys_addr, size_t *size)
{
	struct resource *res;
	void __iomem *result;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (IS_ERR_OR_NULL(res)) {
		vts_dev_err(&pdev->dev, "Failed to get %s\n", name);
		return ERR_PTR(-EINVAL);
	}
	if (phys_addr)
		*phys_addr = res->start;
	if (size)
		*size = resource_size(res);
	res = devm_request_mem_region(&pdev->dev,
		res->start, resource_size(res), name);
	if (IS_ERR_OR_NULL(res)) {
		vts_dev_err(&pdev->dev, "Failed to request %s\n", name);
		return ERR_PTR(-EFAULT);
	}
	result = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(result)) {
		vts_dev_err(&pdev->dev, "Failed to map %s\n", name);
		return ERR_PTR(-EFAULT);
	}
	vts_dev_dbg(&pdev->dev, "%s: %s(%pK) is mapped on %pK with size of %zu",
			__func__, name,
			(void *)res->start, result, (size_t)resource_size(res));
	vts_dev_info(&pdev->dev, "%s is mapped(size:%zu)",
			name, (size_t)resource_size(res));
	return result;
}

static int samsung_vts_devm_request_threaded_irq(
		struct platform_device *pdev, const char *irq_name,
		unsigned int hw_irq, irq_handler_t thread_fn)
{
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);
	int result;

	data->irq[hw_irq] = platform_get_irq_byname(pdev, irq_name);
	if (data->irq[hw_irq] < 0) {
		vts_dev_err(dev, "Failed to get irq %s: %d\n",
			irq_name, data->irq[hw_irq]);
		return data->irq[hw_irq];
	}

	result = devm_request_threaded_irq(dev, data->irq[hw_irq],
			NULL, thread_fn,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev->init_name,
			pdev);

	if (result < 0)
		vts_dev_err(dev, "Unable to request irq %s: %d\n",
			irq_name, result);

	return result;
}

static struct clk *devm_clk_get_and_prepare(
	struct device *dev, const char *name)
{
	struct clk *clk;
	int result;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		vts_dev_err(dev, "Failed to get clock %s\n", name);
		goto error;
	}

	result = clk_prepare(clk);
	if (result < 0) {
		vts_dev_err(dev, "Failed to prepare clock %s\n", name);
		goto error;
	}

error:
	return clk;
}

static const struct reg_default vts_dmic_reg_defaults[] = {
	{0x0000, 0x00030000},
	{0x0004, 0x00000000},
};

static const struct regmap_config vts_component_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = VTS_DMIC_CONTROL_DMIC_IF,
	.reg_defaults = vts_dmic_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(vts_dmic_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
	.fast_io = true,
};

static int vts_fio_open(struct inode *inode, struct file *file)
{
	file->private_data = p_vts_data;
	return 0;
}

static long vts_fio_common_ioctl(struct file *file,
		unsigned int cmd, int __user *_arg)
{
	struct vts_data *data = (struct vts_data *)file->private_data;
	struct platform_device *pdev;
	struct device *dev;
	int arg;
	struct vts_ipc_msg ipc_msg;
	u32 values[3] = {0, 0, 0};
	struct vts_model_bin_info sm_info;

	if (!data || (((cmd >> 8) & 0xff) != 'V'))
		return -ENOTTY;
	pdev = data->pdev;
	dev = &pdev->dev;

	switch (cmd) {
	case VTSDRV_MISC_IOCTL_LOAD_SOUND_MODEL:
		if (copy_from_user(&sm_info, (struct vts_model_bin_info __user *)_arg,
					sizeof(struct vts_model_bin_info))) {
			vts_dev_err(dev, "%s: LOAD_SOUND_MODEL failed", __func__);
			return -EFAULT;
		}

		if (sm_info.actual_sz < 0 || sm_info.actual_sz > sm_info.max_sz)
			return -EINVAL;
		if (sm_info.actual_sz > VTS_MODEL_BIN_MAXSZ)
			return -EINVAL;
		memcpy(data->sm_data, data->dmab_model.area, sm_info.actual_sz);
		data->sm_loaded = true;
		data->sm_info.actual_sz = sm_info.actual_sz;
		data->sm_info.max_sz = sm_info.max_sz;
		data->sm_info.offset = sm_info.offset;
		vts_dev_info(dev, "%s: LOAD_SOUND_MODEL actual_sz=%d, max_sz=%d, offset=0x%x",
				__func__, data->sm_info.actual_sz, data->sm_info.max_sz, data->sm_info.offset);
		break;
	case VTSDRV_MISC_IOCTL_READ_GOOGLE_VERSION:
		if (get_user(arg, _arg))
			return -EFAULT;

		if (data->google_version == 0) {
			vts_dev_info(dev, "get_sync : Read Google Version");
			pm_runtime_get_sync(dev);
			vts_start_runtime_resume(dev, 0);
			if (pm_runtime_active(dev))
				pm_runtime_put(dev);
		}
		vts_dev_info(dev, "Google Version: %d", data->google_version);

		put_user(data->google_version, (int __user *)_arg);
		break;
	case VTSDRV_MISC_IOCTL_READ_GOOGLE_UUID:
		vts_dev_info(dev, "Google UUID: %s", data->google_uuid);
		copy_to_user((void __user *)_arg, data->google_uuid, 40);
		break;
	case VTSDRV_MISC_IOCTL_READ_EVENT_TYPE:
		if (get_user(arg, _arg))
			return -EFAULT;

		put_user(data->poll_event_type, (int __user *)_arg);
		data->poll_event_type = EVENT_NONE;
		break;
	case VTSDRV_MISC_IOCTL_WRITE_EXIT_POLLING:
		if (get_user(arg, _arg))
			return -EFAULT;

		data->poll_event_type |= EVENT_STOP_POLLING|EVENT_READY;
		wake_up(&data->poll_wait_queue);
		break;
	case VTSDRV_MISC_IOCTL_SET_PARAM:
		if (copy_from_user(&ipc_msg, (struct vts_ipc_msg __user *)_arg,
					sizeof(struct vts_ipc_msg))) {
			vts_dev_err(dev, "%s: SET_PARAM failed", __func__);
			return -EFAULT;
		}
		values[0] = ipc_msg.values[0];
		values[1] = ipc_msg.values[1];
		values[2] = ipc_msg.values[2];
		vts_dev_info(dev, "%s: SET_PARAM msg: %d, value: 0x%x, 0x%x, 0x%x",
				__func__, ipc_msg.msg, values[0], values[1], values[2]);
		if (vts_start_ipc_transaction(dev, data, ipc_msg.msg, &values, 0, 1) < 0) {
			vts_dev_err(dev, "%s: SET_PARAM ipc transaction failed\n", __func__);
		}
		break;
	default:
		pr_err("VTS unknown ioctl = 0x%x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static long vts_fio_ioctl(struct file *file,
		unsigned int cmd, unsigned long _arg)
{
	return vts_fio_common_ioctl(file, cmd, (int __user *)_arg);
}

#ifdef CONFIG_COMPAT
static long vts_fio_compat_ioctl(struct file *file,
		unsigned int cmd, unsigned long _arg)
{
	return vts_fio_common_ioctl(file, cmd, compat_ptr(_arg));
}
#endif /* CONFIG_COMPAT */

static int vts_fio_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct vts_data *data = (struct vts_data *)file->private_data;

	return dma_mmap_wc(&data->pdev->dev, vma, data->dmab_model.area,
			   data->dmab_model.addr, data->dmab_model.bytes);
}

static unsigned int vts_fio_poll(struct file *file, poll_table *wait)
{
	struct vts_data *data = (struct vts_data *)file->private_data;
	int ret = 0;

	poll_wait(file, &data->poll_wait_queue, wait);

	if (data->poll_event_type & EVENT_READY) {
		data->poll_event_type &= ~EVENT_READY;
		ret = POLLIN | POLLRDNORM;
	}

	return ret;
}

static const struct file_operations vts_fio_fops = {
	.owner			= THIS_MODULE,
	.open			= vts_fio_open,
	.release		= NULL,
	.unlocked_ioctl		= vts_fio_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= vts_fio_compat_ioctl,
#endif /* CONFIG_COMPAT */
	.mmap			= vts_fio_mmap,
	.poll			= vts_fio_poll,
};

static struct miscdevice vts_fio_miscdev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     "vts_fio_dev",
	.fops =     &vts_fio_fops
};

static int vts_memlog_file_completed(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int vts_memlog_status_notify(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int vts_memlog_level_notify(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int vts_memlog_enable_notify(struct memlog_obj *obj, u32 flags)
{
	struct vts_data *data = p_vts_data;
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	u32 values[3] = {0, 0, 0};
	int result = 0;

	if (data->kernel_log_obj)
		vts_dev_info(dev, "vts kernel_log_obj enable : %d",
			data->kernel_log_obj->enabled);

	if (data->fw_log_obj) {
		data->fw_logger_enabled = data->fw_log_obj->enabled;
		vts_dev_info(dev, "firmware logger enabled : %d",
			data->fw_logger_enabled);
		if (data->vts_ready) {
			if (data->fw_logger_enabled)
				values[0] = VTS_ENABLE_DEBUGLOG;
			else {
				values[0] = VTS_DISABLE_DEBUGLOG;
				vts_register_log_buffer(dev, 0, 0);
			}
			values[1] = 0;
			values[2] = 0;
			result = vts_start_ipc_transaction(dev, data,
				VTS_IRQ_AP_COMMAND, &values, 0, 1);
			if (result < 0)
				vts_dev_err(dev, "%s debuglog ipc transaction failed\n",
					__func__);
		}
	}
	return 0;
}

static const struct memlog_ops vts_memlog_ops = {
	.file_ops_completed = vts_memlog_file_completed,
	.log_status_notify = vts_memlog_status_notify,
	.log_level_notify = vts_memlog_level_notify,
	.log_enable_notify = vts_memlog_enable_notify,
};

static int vts_pm_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct vts_data *data = container_of(nb, struct vts_data, pm_nb);
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;

	vts_dev_info(dev, "%s(%lu)\n", __func__, action);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		/* Nothing to do */
		break;
	default:
		/* Nothing to do */
		break;
	}
	return NOTIFY_DONE;
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
static int vts_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct vts_data *data = container_of(nb, struct vts_data, itmon_nb);
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	struct itmon_notifier *itmon_data = nb_data;

	vts_dev_info(dev, "%s: action:%ld, master:%s, dest:%s\n",
			__func__, action, itmon_data->master, itmon_data->dest);

	if (itmon_data
	    && ((strncmp(itmon_data->master, VTS_ITMON_NAME, sizeof(VTS_ITMON_NAME) - 1) == 0)
		|| (strncmp(itmon_data->dest, VTS_ITMON_NAME, sizeof(VTS_ITMON_NAME) - 1) == 0))) {
		vts_dev_info(dev, "%s(%lu)\n", __func__, action);
		vts_dev_info(dev, "%s: shared_info: 0x%x, %d, %d", __func__,
			data->shared_info->vendor_data[0], data->shared_info->vendor_data[1],
			data->shared_info->vendor_data[2]);

		/* Dump VTS GPR register & SRAM */
		vts_dbg_dump_fw_gpr(dev, data, VTS_ITMON_ERROR);
		print_hex_dump(KERN_ERR, "vts-fw-log", DUMP_PREFIX_OFFSET, 32, 4, data->sramlog_baseaddr,
			VTS_SRAM_EVENTLOG_SIZE_MAX, true);
		print_hex_dump(KERN_ERR, "vts-time-log", DUMP_PREFIX_OFFSET, 32, 4,
			data->sramlog_baseaddr + VTS_SRAM_EVENTLOG_SIZE_MAX,
			VTS_SRAM_TIMELOG_SIZE_MAX, true);

		data->enabled = false;
		mailbox_update_vts_is_on(false);

		return NOTIFY_BAD;
	}
	return NOTIFY_DONE;
}
#endif

static int vts_sysevent_ramdump(
	int enable, const struct sysevent_desc *sysevent)
{
	struct vts_data *data = p_vts_data;
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	/*
	 * This function is called in syseventtem_put / restart
	 * TODO: Ramdump related
	 */
	vts_dev_info(dev, "%s: call-back function\n", __func__);
	return 0;
}

static int vts_sysevent_powerup(const struct sysevent_desc *sysevent)
{
	struct vts_data *data = p_vts_data;
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	/*
	 * This function is called in syseventtem_get / restart
	 * TODO: Power up related
	 */
	vts_dev_info(dev, "%s: call-back function\n", __func__);
	return 0;
}

static int vts_sysevent_shutdown(
	const struct sysevent_desc *sysevent, bool force_stop)
{
	struct vts_data *data = p_vts_data;
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	/*
	 * This function is called in syseventtem_get / restart
	 * TODO: Shutdown related
	 */
	vts_dev_info(dev, "%s: call-back function\n", __func__);
	return 0;
}

static void vts_sysevent_crash_shutdown(const struct sysevent_desc *sysevent)
{
	struct vts_data *data = p_vts_data;
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	/*
	 * This function is called in syseventtem_get / restart
	 * TODO: Crash Shutdown related
	 */
	vts_dev_info(dev, "%s: call-back function\n", __func__);
}

static int vts_component_probe(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;
	struct vts_data *data = dev_get_drvdata(dev);
	int ret = 0;

	vts_dev_info(dev, "%s\n", __func__);
	data->cmpnt = cmpnt;

	ret = vts_soc_cmpnt_probe(dev);
	if (ret) {
		vts_dev_err(dev, "vts_soc_cmpnt_probe err(%d)\n", ret);

		return ret;
	}

	vts_dev_info(dev, "%s(%d)\n", __func__, __LINE__);

	return ret;
}

static const struct snd_soc_component_driver vts_component = {
	.probe = vts_component_probe,
	.controls = vts_controls,
	.num_controls = ARRAY_SIZE(vts_controls),
	.dapm_widgets = vts_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(vts_dapm_widgets),
	.dapm_routes = vts_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(vts_dapm_routes),
};

static struct platform_driver * const vts_sub_drivers[] = {
	&samsung_vts_dma_driver,
};

static int samsung_vts_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct vts_data *data;
	int i, result;

	vts_dev_info(dev, "%s\n", __func__);
	data = devm_kzalloc(dev, sizeof(struct vts_data), GFP_KERNEL);
	if (!data) {
		vts_dev_err(dev, "Failed to allocate memory\n");
		result = -ENOMEM;
		goto error;
	}

	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));

	vts_port_init(dev);

	/* Model binary memory allocation */
	data->google_version = 0;
	data->sm_info.max_sz = VTS_MODEL_BIN_MAXSZ;
	data->sm_info.actual_sz = 0;
	data->sm_info.offset = 0;
	data->sm_loaded = false;
	data->sm_data = vmalloc(VTS_MODEL_BIN_MAXSZ);
	if (!data->sm_data) {
		vts_dev_err(dev, "%s Failed to allocate Grammar Bin memory\n",
			__func__);
		result = -ENOMEM;
		goto error;
	}

	/* initialize device structure members */
	data->active_trigger = 0;

	/* initialize micbias setting count */
	data->micclk_init_cnt = 0;
	data->mic_ready = 0;
	data->vts_state = VTS_STATE_NONE;
	data->vts_rec_state = VTS_REC_STATE_STOP;
	data->vts_tri_state = VTS_TRI_STATE_COPY_STOP;

	platform_set_drvdata(pdev, data);
	data->pdev = pdev;
	p_vts_data = data;

	init_waitqueue_head(&data->ipc_wait_queue);
	init_waitqueue_head(&data->poll_wait_queue);
	spin_lock_init(&data->ipc_spinlock);
	spin_lock_init(&data->state_spinlock);
	mutex_init(&data->ipc_mutex);
	mutex_init(&data->mutex_pin);
	data->wake_lock = wakeup_source_register(dev, "vts");
	data->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(data->pinctrl)) {
		vts_dev_err(dev, "Couldn't get pins (%li)\n",
				PTR_ERR(data->pinctrl));
		data->pinctrl = NULL;
	}

	data->sfr_base = samsung_vts_devm_request_and_map(pdev,
		"sfr", NULL, NULL);
	if (IS_ERR(data->sfr_base)) {
		result = PTR_ERR(data->sfr_base);
		goto error;
	}

	data->baaw_base = samsung_vts_devm_request_and_map(pdev,
		"baaw", NULL, NULL);
	if (IS_ERR(data->baaw_base)) {
		result = PTR_ERR(data->baaw_base);
		goto error;
	}

	data->sram_base = samsung_vts_devm_request_and_map(pdev,
		"sram", &data->sram_phys, &data->sram_size);
	if (IS_ERR(data->sram_base)) {
		result = PTR_ERR(data->sram_base);
		goto error;
	}

	data->dmic_if0_base = samsung_vts_devm_request_and_map(pdev,
		"dmic", NULL, NULL);
	if (IS_ERR(data->dmic_if0_base)) {
		result = PTR_ERR(data->dmic_if0_base);
		goto error;
	}

	data->dmic_if1_base = samsung_vts_devm_request_and_map(pdev,
		"dmic1", NULL, NULL);
	if (IS_ERR(data->dmic_if1_base)) {
		result = PTR_ERR(data->dmic_if1_base);
		goto error;
	}
#if defined(CONFIG_SOC_S5E8825)
	data->gpr_base = samsung_vts_devm_request_and_map(pdev,
		"gpr", NULL, NULL);
	if (IS_ERR(data->gpr_base)) {
		result = PTR_ERR(data->gpr_base);
		goto error;
	}
#else
	data->gpr_base = data->sfr_base + VTS_SYSREG_YAMIN_DUMP;
	if (IS_ERR(data->gpr_base)) {
		result = PTR_ERR(data->gpr_base);
		goto error;
	}
#endif

	data->sicd_base = samsung_vts_membase_request_and_map(pdev,
		"sicd-base", "sicd-size");
	if (IS_ERR(data->sicd_base)) {
		result = PTR_ERR(data->sicd_base);
		goto error;
	}
#if defined(CONFIG_SOC_S5E9925)
	data->intmem_code = samsung_vts_devm_request_and_map(pdev,
		"intmem_code", NULL, NULL);
	if (IS_ERR(data->intmem_code)) {
		result = PTR_ERR(data->intmem_code);
		goto error;
	}
	data->intmem_data = samsung_vts_devm_request_and_map(pdev,
		"intmem_data", NULL, NULL);
	if (IS_ERR(data->intmem_data)) {
		result = PTR_ERR(data->intmem_data);
		goto error;
	}
	data->intmem_pcm = samsung_vts_devm_request_and_map(pdev,
		"intmem_pcm", NULL, NULL);
	if (IS_ERR(data->intmem_pcm)) {
		result = PTR_ERR(data->intmem_pcm);
		goto error;
	}

	data->intmem_data1 = samsung_vts_devm_request_and_map(pdev,
		"intmem_data1", NULL, NULL);
	if (IS_ERR(data->intmem_data1)) {
		result = PTR_ERR(data->intmem_data1);
		goto error;
	}

	data->sfr_slif_vts = samsung_vts_devm_request_and_map(pdev,
		"slif_vts", NULL, NULL);
	if (IS_ERR(data->sfr_slif_vts)) {
		result = PTR_ERR(data->sfr_slif_vts);
		goto error;
	}
#endif

#if defined(CONFIG_SOC_S5E8825)
	data->dmic_ahb0_base = samsung_vts_devm_request_and_map(pdev,
		"dmic_ahb0", NULL, NULL);
	if (IS_ERR(data->dmic_ahb0_base)) {
		result = PTR_ERR(data->dmic_ahb0_base);
		goto error;
	}

	data->timer0_base = samsung_vts_devm_request_and_map(pdev,
		"timer0", NULL, NULL);
	if (IS_ERR(data->timer0_base)) {
		result = PTR_ERR(data->timer0_base);
		goto error;
	}
#endif

	data->lpsdgain = 0;
	data->dmicgain = 0;
	data->amicgain = 0;

	/* read tunned VTS gain values */
	of_property_read_u32(np, "lpsd-gain", &data->lpsdgain);
	of_property_read_u32(np, "dmic-gain", &data->dmicgain);
	of_property_read_u32(np, "amic-gain", &data->amicgain);
	data->imgloader = of_property_read_bool(np,
		"samsung,imgloader-vts-support");

	vts_dev_info(dev, "VTS Tunned Gain value LPSD: %d DMIC: %d AMIC: %d\n",
			data->lpsdgain, data->dmicgain, data->amicgain);

	vts_dev_info(dev, "VTS: use dmam_alloc_coherent\n");
	data->dmab.area = dmam_alloc_coherent(dev,
		BUFFER_BYTES_MAX, &data->dmab.addr, GFP_KERNEL);
	if (data->dmab.area == NULL) {
		result = -ENOMEM;
		goto error;
	}
	data->dmab.bytes = BUFFER_BYTES_MAX/2;
	data->dmab.dev.dev = dev;
	data->dmab.dev.type = SNDRV_DMA_TYPE_DEV;

	data->dmab_rec.area = (data->dmab.area + BUFFER_BYTES_MAX/2);
	data->dmab_rec.addr = (data->dmab.addr + BUFFER_BYTES_MAX/2);
	data->dmab_rec.bytes = BUFFER_BYTES_MAX/2;
	data->dmab_rec.dev.dev = dev;
	data->dmab_rec.dev.type = SNDRV_DMA_TYPE_DEV;

	data->dmab_log.area = dmam_alloc_coherent(dev, LOG_BUFFER_BYTES_MAX,
				&data->dmab_log.addr, GFP_KERNEL);
	if (data->dmab_log.area == NULL) {
		result = -ENOMEM;
		goto error;
	}
	data->dmab_log.bytes = LOG_BUFFER_BYTES_MAX;
	data->dmab_log.dev.dev = dev;
	data->dmab_log.dev.type = SNDRV_DMA_TYPE_DEV;

	data->dmab_model.area = dmam_alloc_coherent(dev,
		VTSDRV_MISC_MODEL_BIN_MAXSZ,
		&data->dmab_model.addr, GFP_KERNEL);
	if (data->dmab_model.area == NULL) {
		result = -ENOMEM;
		goto error;
	}
	data->dmab_model.bytes = VTSDRV_MISC_MODEL_BIN_MAXSZ;
	data->dmab_model.dev.dev = dev;
	data->dmab_model.dev.type = SNDRV_DMA_TYPE_DEV;
#if defined(CONFIG_SOC_S5E8825)
	data->mux_dmic_clk = devm_clk_get_and_prepare(dev, "mux_dmic_clk");
	if (IS_ERR(data->mux_dmic_clk)) {
		result = PTR_ERR(data->mux_dmic_clk);
		data->mux_dmic_clk = NULL;
		goto error;
	}
#else
	data->clk_vts_src = devm_clk_get_and_prepare(dev, "clk_vts_src");
	if (IS_ERR(data->clk_vts_src)) {
		result = PTR_ERR(data->clk_vts_src);
		goto error;
	}

	result = clk_enable(data->clk_vts_src);
	if (result < 0) {
		vts_dev_err(dev, "Failed to enable the rco\n");
		goto error;
	}

	/* HACK */
	result = clk_set_rate(data->clk_vts_src, 400000000);
	if (result < 0) {
		vts_dev_err(dev, "Failed to enable the rco\n");
		goto error;
	}

	data->clk_sys_mux = devm_clk_get_and_prepare(dev, "clk_sys_mux");
	if (IS_ERR(data->clk_sys_mux)) {
		result = PTR_ERR(data->clk_sys_mux);
		data->clk_sys_mux = NULL;
		goto error;
	}
#endif
	data->mux_clk_dmic_if = devm_clk_get_and_prepare(dev, "clk_dmic_mux");
	if (IS_ERR(data->mux_clk_dmic_if)) {
		result = PTR_ERR(data->mux_clk_dmic_if);
		vts_dev_warn(dev, "clk_dmic_mux(%li)\n", result);
		data->mux_clk_dmic_if = NULL;
	}

	data->clk_dmic_if = devm_clk_get_and_prepare(dev, "dmic_if");
	if (IS_ERR(data->clk_dmic_if)) {
		result = PTR_ERR(data->clk_dmic_if);
		data->clk_dmic_if = NULL;
		goto error;
	}

	data->clk_dmic_sync = devm_clk_get_and_prepare(dev, "dmic_sync");
	if (IS_ERR(data->clk_dmic_sync)) {
		result = PTR_ERR(data->clk_dmic_sync);
		data->clk_dmic_sync = NULL;
		goto error;
	}

	data->clk_sys = devm_clk_get_and_prepare(dev, "clk_sys");
	if (IS_ERR(data->clk_sys)) {
		result = PTR_ERR(data->clk_sys);
		data->clk_sys = NULL;
		goto error;
	}
#if IS_ENABLED(CONFIG_SOC_S5E9925)
	data->clk_slif_src = devm_clk_get_and_prepare(dev, "clk_slif_src");
	if (IS_ERR(data->clk_slif_src)) {
		result = PTR_ERR(data->clk_slif_src);
		goto error;
	}
	data->clk_slif_src1 = devm_clk_get_and_prepare(dev, "clk_slif_src1");
	if (IS_ERR(data->clk_slif_src1)) {
		result = PTR_ERR(data->clk_slif_src1);
		vts_dev_warn(dev, "clk_slif_src1(%li)\n", result);
		data->clk_slif_src1 = NULL;
	}

	data->clk_slif_src2 = devm_clk_get_and_prepare(dev, "clk_slif_src2");
	if (IS_ERR(data->clk_slif_src2)) {
		result = PTR_ERR(data->clk_slif_src2);
		vts_dev_warn(dev, "clk_slif_src2(%li)\n", result);
		data->clk_slif_src2 = NULL;
	}

	data->clk_slif_vts = devm_clk_get_and_prepare(dev, "clk_slif_vts");
	if (IS_ERR(data->clk_slif_vts)) {
		result = PTR_ERR(data->clk_slif_vts);
		data->clk_slif_vts = NULL;
		goto error;
	}
#endif
	result = samsung_vts_devm_request_threaded_irq(pdev, "error",
			VTS_IRQ_VTS_ERROR, vts_error_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "boot_completed",
			VTS_IRQ_VTS_BOOT_COMPLETED, vts_boot_completed_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "ipc_received",
			VTS_IRQ_VTS_IPC_RECEIVED, vts_ipc_received_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev,
		"voice_triggered",
		VTS_IRQ_VTS_VOICE_TRIGGERED, vts_voice_triggered_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev,
		"trigger_period_elapsed",
		VTS_IRQ_VTS_PERIOD_ELAPSED, vts_trigger_period_elapsed_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev,
		"record_period_elapsed",
		VTS_IRQ_VTS_REC_PERIOD_ELAPSED,
		vts_record_period_elapsed_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "debuglog_bufzero",
		VTS_IRQ_VTS_DBGLOG_BUFZERO, vts_debuglog_bufzero_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "debuglog_bufone",
			VTS_IRQ_VTS_DBGLOG_BUFONE, vts_debuglog_bufone_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "audio_dump",
			VTS_IRQ_VTS_AUDIO_DUMP, vts_audiodump_handler);
	if (result < 0)
		goto error;

	result = samsung_vts_devm_request_threaded_irq(pdev, "log_dump",
			VTS_IRQ_VTS_LOG_DUMP, vts_logdump_handler);
	if (result < 0)
		goto error;

	data->irq_state = true;

	data->pdev_mailbox = of_find_device_by_node(of_parse_phandle(np,
		"mailbox", 0));
	if (!data->pdev_mailbox) {
		vts_dev_err(dev, "Failed to get mailbox\n");
		result = -EPROBE_DEFER;
		goto error;
	}

	if (data->imgloader) {
		result = vts_core_imgloader_desc_init(pdev);
		if (result < 0)
			return result;
	}

	data->regmap_dmic = devm_regmap_init_mmio_clk(dev,
			NULL,
			data->dmic_if0_base,
			&vts_component_regmap_config);

	result = snd_soc_register_component(dev,
		&vts_component, vts_dai, ARRAY_SIZE(vts_dai));
	if (result < 0) {
		vts_dev_err(dev, "Failed to register ASoC component\n");
		goto error;
	}

#ifdef EMULATOR
	pmu_alive = ioremap(0x16480000, 0x10000);
#endif

	data->pm_nb.notifier_call = vts_pm_notifier;
	register_pm_notifier(&data->pm_nb);

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	data->itmon_nb.notifier_call = vts_itmon_notifier;
	itmon_notifier_chain_register(&data->itmon_nb);
#endif

	data->sysevent_dev = NULL;
	data->sysevent_desc.name = pdev->name;
	data->sysevent_desc.owner = THIS_MODULE;
	data->sysevent_desc.ramdump = vts_sysevent_ramdump;
	data->sysevent_desc.powerup = vts_sysevent_powerup;
	data->sysevent_desc.shutdown = vts_sysevent_shutdown;
	data->sysevent_desc.crash_shutdown = vts_sysevent_crash_shutdown;
	data->sysevent_desc.dev = &pdev->dev;
	data->sysevent_dev = sysevent_register(&data->sysevent_desc);
	if (IS_ERR(data->sysevent_dev)) {
		result = PTR_ERR(data->sysevent_dev);
		vts_dev_err(dev, "fail in device_event_probe : %d\n", result);
		goto error;
	}
	vts_proc_probe();
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	vts_clk_set_rate(dev, 0);
	vts_port_cfg(dev, VTS_PORT_PAD0, VTS_PORT_ID_VTS, DPDM, false);

	data->voicecall_enabled = false;
	data->voicerecog_start = 0;
	data->syssel_rate = 0;
	data->target_sysclk = 0;
	data->target_size = 0;
	data->vtsfw_version = 0x0;
	data->vtsdetectlib_version = 0x0;
	data->fw_logfile_enabled = 0;
	data->fw_logger_enabled = 0;
	data->audiodump_enabled = false;
	data->logdump_enabled = false;
	vts_set_clk_src(dev, VTS_CLK_SRC_RCO);

	result = device_create_file(dev, &dev_attr_vtsfw_version);
	if (result < 0)
		vts_dev_warn(dev, "Failed to create file: %s\n",
			"vtsfw_version");

	result = device_create_file(dev, &dev_attr_vtsdetectlib_version);
	if (result < 0)
		vts_dev_warn(dev, "Failed to create file: %s\n",
			"vtsdetectlib_version");

	result = device_create_file(dev, &dev_attr_vts_cmd);
	if (result < 0)
		vts_dev_warn(dev, "Failed to create file: %s\n",
			"vts_cmd");

	result = device_create_file(dev, &dev_attr_vts_audiodump);
	if (result < 0)
		vts_dev_warn(dev, "Failed to create file: %s\n",
			"vts_audiodump");

	result = device_create_file(dev, &dev_attr_vts_logdump);
	if (result < 0)
		vts_dev_warn(dev, "Failed to create file: %s\n",
			"vts_logdump");

	data->sramlog_baseaddr = (char *)(data->sram_base + VTS_SRAMLOG_MSGS_OFFSET);

	data->shared_info = (struct vts_shared_info *)(data->sramlog_baseaddr +	VTS_SRAMLOG_SIZE_MAX);

	atomic_notifier_chain_register(&panic_notifier_list,
		&vts_panic_notifier);

	/* initialize log buffer offset as non */
	vts_register_log_buffer(dev, 0, 0);

	device_init_wakeup(dev, true);

	/* 21.09.08 Disable vts memlogger */
	goto skip_memlog;

	result = memlog_register("VTS_DRV", dev, &data->log_desc);
	if (result) {
		vts_dev_err(dev, "memlog_register fail");
		goto skip_memlog;
	}
	data->log_desc->ops = vts_memlog_ops;
	data->kernel_log_file_obj = memlog_alloc_file(data->log_desc,
		"ker-mem.memlog", SZ_256K, SZ_512K, 5000, 1);
	data->kernel_log_obj = memlog_alloc_printf(data->log_desc,
		SZ_256K, data->kernel_log_file_obj, "ker-mem", 0);
	data->dump_file_obj = memlog_alloc_file(data->log_desc,
		"fw-dmp.memlog", SZ_2K, SZ_4K, 5000, 1);
	data->dump_obj = memlog_alloc_dump(data->log_desc,
		SZ_2K, data->sram_phys + VTS_SRAMLOG_MSGS_OFFSET,
		false, data->dump_file_obj, "fw-dmp");
	data->fw_log_file_obj = memlog_alloc_file(data->log_desc,
		"fw-mem.memlog", SZ_128K, SZ_256K, 5000, 1);
	data->fw_log_obj = memlog_alloc(data->log_desc,
		SZ_128K, data->fw_log_file_obj, "fw-mem",
		MEMLOG_UFALG_NO_TIMESTAMP);
	/* 20.12.28 Disable fw memlogger
	if (data->fw_log_obj)
		data->fw_logger_enabled = data->fw_log_obj->enabled;
	*/
skip_memlog:

	/* Allocate Memory for error logging */
	for (i = 0; i < VTS_DUMP_LAST; i++) {
		data->p_dump[i].sram_log =
			kzalloc(VTS_SRAMLOG_SIZE_MAX +
				sizeof(VTS_DUMP_MAGIC), GFP_KERNEL);
		if (!data->p_dump[i].sram_log) {
			vts_dev_info(dev, "%s is skipped due to lack of memory for sram log\n",
				__func__);
			continue;
		}
		if (i != RUNTIME_SUSPEND_DUMP) {
			/* Allocate VTS firmware all */
			data->p_dump[i].sram_fw =
				kzalloc(data->sram_size, GFP_KERNEL);
			if (!data->p_dump[i].sram_fw) {
				vts_dev_info(dev, "%s is skipped due to lack of memory for sram dump\n",
					__func__);
				continue;
			}
		}
	}

	result = misc_register(&vts_fio_miscdev);
	if (result)
		vts_dev_warn(dev, "Failed to create device for sound model download\n");
	else
		vts_dev_info(dev, "misc_register ok");

#ifdef DEF_VTS_PCM_DUMP
	vts_pcm_dump_init(dev);
	vts_pcm_dump_register(dev, 0, "vts_rec", data->dmab_rec.area,
		data->dmab_rec.addr, BUFFER_BYTES_MAX/2);
	vts_pcm_dump_register(dev, 1, "vts_tri", data->dmab.area,
		data->dmab.addr, BUFFER_BYTES_MAX/2);
#endif

	vts_dev_info(dev, "register sub drivers\n");
	result = platform_register_drivers(vts_sub_drivers,
			ARRAY_SIZE(vts_sub_drivers));
	if (result)
		vts_dev_warn(dev, "Failed to register sub drivers\n");
	else {
		vts_dev_info(dev, "register sub drivers OK\n");
		of_platform_populate(np, NULL, NULL, dev);
	}

	if (pm_runtime_active(dev))
		pm_runtime_put(dev);

	if (vts_soc_probe(dev))
		vts_dev_warn(dev, "Failed to vts_soc_probe\n");
	else
		vts_dev_dbg(dev, "vts_soc_probe ok");

	vts_dev_info(dev, "Probed successfully\n");
error:
	return result;
}

static int samsung_vts_remove(struct platform_device *pdev)
{
	int i;
	struct device *dev = &pdev->dev;
	struct vts_data *data = platform_get_drvdata(pdev);

	pm_runtime_disable(dev);

	vts_soc_remove(dev);

#ifndef CONFIG_PM
	vts_runtime_suspend(dev);
#endif
	release_firmware(data->firmware);
	if (data->sm_data)
		vfree(data->sm_data);

	for (i = 0; i < RUNTIME_SUSPEND_DUMP; i++) {
		/* Free memory for VTS firmware */
		kfree(data->p_dump[i].sram_fw);
	}

	snd_soc_unregister_component(dev);
#ifdef EMULATOR
	iounmap(pmu_alive);
#endif
	return 0;
}

static struct platform_driver samsung_vts_driver = {
	.probe  = samsung_vts_probe,
	.remove = samsung_vts_remove,
	.driver = {
		.name = "samsung-vts",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_vts_of_match),
		.pm = &samsung_vts_pm,
	},
};

module_platform_driver(samsung_vts_driver);

/* Module information */
MODULE_AUTHOR("Gyeongtaek Lee, <gt82.lee@samsung.com>");
MODULE_AUTHOR("Palli Satish Kumar Reddy, <palli.satish@samsung.com>");
MODULE_AUTHOR("Jaewon Kim, <jaewons.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung Voice Trigger System");
MODULE_ALIAS("platform:samsung-vts");
MODULE_LICENSE("GPL");
