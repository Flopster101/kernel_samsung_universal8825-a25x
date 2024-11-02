// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *   USB Audio Driver for Exynos
 *
 *   Copyright (c) 2017 by Kyounghye Yun <k-hye.yun@samsung.com>
 *
 */

#ifndef __LINUX_USB_EXYNOS_AUDIO_H
#define __LINUX_USB_EXYNOS_AUDIO_H

#include <sound/samsung/abox.h>
#include "../../../sound/usb/usbaudio.h"

/* for KM */

#define USB_AUDIO_MEM_BASE	0xC0000000

#define USB_AUDIO_SAVE_RESTORE	(USB_AUDIO_MEM_BASE)
#define USB_AUDIO_DEV_CTX	(USB_AUDIO_SAVE_RESTORE+PAGE_SIZE)
#define USB_AUDIO_INPUT_CTX	(USB_AUDIO_DEV_CTX+PAGE_SIZE)
#define USB_AUDIO_OUT_DEQ	(USB_AUDIO_INPUT_CTX+PAGE_SIZE)
#define USB_AUDIO_IN_DEQ	(USB_AUDIO_OUT_DEQ+PAGE_SIZE)
#define USB_AUDIO_FBOUT_DEQ	(USB_AUDIO_IN_DEQ+PAGE_SIZE)
#define USB_AUDIO_FBIN_DEQ	(USB_AUDIO_FBOUT_DEQ+PAGE_SIZE)
#define USB_AUDIO_ERST		(USB_AUDIO_FBIN_DEQ+PAGE_SIZE)
#define USB_AUDIO_DESC		(USB_AUDIO_ERST+PAGE_SIZE)
#define USB_AUDIO_PCM_OUTBUF	(USB_AUDIO_MEM_BASE+0x100000)
#define USB_AUDIO_PCM_INBUF	(USB_AUDIO_MEM_BASE+0x800000)

#if defined(CONFIG_SOC_EXYNOS9820)
#define USB_AUDIO_XHCI_BASE	0x10c00000
#elif defined(CONFIG_SOC_EXYNOS9830)
#define USB_AUDIO_XHCI_BASE	0x10e00000
#elif defined(CONFIG_SOC_EXYNOS2100)
#define USB_AUDIO_XHCI_BASE	0x10e00000
#define USB_URAM_BASE		0x10ff0000
#define USB_URAM_SIZE		SZ_1K * 60
#elif defined(CONFIG_SOC_S5E9815)
#define USB_AUDIO_XHCI_BASE	0x12210000
#define USB_URAM_BASE		0x122a0000
#define USB_URAM_SIZE		SZ_1K * 24
#elif defined(CONFIG_SOC_S5E9925)
#define USB_AUDIO_XHCI_BASE	0x10B00000
#define USB_URAM_BASE		0x02a00000
#define USB_URAM_SIZE		SZ_1K * 24
#elif defined(CONFIG_SOC_S5E8825)
#define USB_AUDIO_XHCI_BASE	0x13200000
#define USB_URAM_BASE		0x13300000
#define USB_URAM_SIZE		SZ_1K * 24
#else
#error
#endif

#define USB_AUDIO_CONNECT		(1 << 0)
#define USB_AUDIO_REMOVING		(1 << 1)
#define USB_AUDIO_DISCONNECT		(1 << 2)
#define USB_AUDIO_TIMEOUT_PROBE	(1 << 3)

#define DISCONNECT_TIMEOUT	(500)

#define AUDIO_MODE_NORMAL		0
#define AUDIO_MODE_RINGTONE		1
#define AUDIO_MODE_IN_CALL		2
#define AUDIO_MODE_IN_COMMUNICATION	3
#define AUDIO_MODE_CALL_SCREEN		4

#define	CALL_INTERVAL_THRESHOLD		3

#define USB_AUDIO_CONNECT		(1 << 0)
#define USB_AUDIO_REMOVING		(1 << 1)
#define USB_AUDIO_DISCONNECT		(1 << 2)
#define USB_AUDIO_TIMEOUT_PROBE	(1 << 3)

#define DISCONNECT_TIMEOUT	(500)

struct host_data {
	dma_addr_t out_data_dma;
	dma_addr_t in_data_dma;
	void *out_data_addr;
	void *in_data_addr;
};

extern struct host_data xhci_data;

struct exynos_usb_audio {
	struct usb_device *udev;
	struct platform_device *abox;
	struct platform_device *hcd_pdev;
	struct mutex    lock;
	struct work_struct usb_work;
	struct completion in_conn_stop;
	struct completion out_conn_stop;
	struct completion discon_done;

	u64 out_buf_addr;
	u64 in_buf_addr;
	u64 pcm_offset;
	u64 desc_addr;
	u64 offset;

	/* for hw_info */
	u64 dcbaa_dma;
	u64 in_ctx;
	u64 out_ctx;
	u64 erst_addr;

	int speed;
	/* 1: in, 0: out */
	int set_ep;
	int is_audio;
	int is_first_probe;
	u8 indeq_map_done;
	u8 outdeq_map_done;
	u8 fb_indeq_map_done;
	u8 fb_outdeq_map_done;
	u32 pcm_open_done;
	u32 usb_audio_state;

	struct snd_kcontrol *kctl;
	u32 user_scenario;

	void *pcm_buf;
	u64 save_dma;

	bool use_uram;
};

struct hcd_hw_info {
	/* for XHCI */
	int slot_id;
	dma_addr_t erst_addr;
	dma_addr_t dcbaa_dma;
	dma_addr_t in_ctx;
	dma_addr_t out_ctx;
	dma_addr_t save_dma;
	u64 cmd_ring;
	/* Data Stream EP */
	u64 old_out_deq;
	u64 old_in_deq;
	u64 out_deq;
	u64 in_deq;
	int in_ep;
	int out_ep;
	/* feedback ep */
	int fb_in_ep;
	int fb_out_ep;
	int feedback;
	u64 fb_old_out_deq;
	u64 fb_old_in_deq;
	u64 fb_out_deq;
	u64 fb_in_deq;
	/* Device Common Information */
	int speed;
	void *out_buf;
	u64 out_dma;
	void *in_buf;
	u64 in_dma;
	int use_uram;
	int rawdesc_length;
};

int exynos_usb_audio_init(struct device *dev, struct platform_device *pdev);
int exynos_usb_audio_exit(void);
#endif /* __LINUX_USB_EXYNOS_AUDIO_H */
