// SPDX-License-Identifier: GPL-2.0
/*
 * xhci-plat.c - xHCI host controller driver platform Bus Glue.
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com
 * Author: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * A lot of code borrowed from the Linux xHCI driver.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/usb/phy.h>
#include <linux/slab.h>
#include <linux/phy/phy.h>
#include <linux/acpi.h>
#include <linux/usb/of.h>
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
#include "../../../sound/usb/exynos_usb_audio.h"
#include <linux/types.h>
#include "xhci-trace.h"
#endif

#include "../core/hub.h"
#include "../core/phy.h"
#include "xhci.h"
#include "xhci-plat.h"
#include "xhci-mvebu.h"
#include "xhci-rcar.h"
#include "../dwc3/dwc3-exynos.h"
#include "../dwc3/exynos-otg.h"
#include "xhci-exynos.h"
#include <soc/samsung/exynos-cpupm.h>

static struct hc_driver __read_mostly xhci_exynos_hc_driver;

static int xhci_exynos_setup(struct usb_hcd *hcd);
static int xhci_exynos_start(struct usb_hcd *hcd);
static int xhci_exynos_bus_suspend(struct usb_hcd *hcd);
static int xhci_exynos_bus_resume(struct usb_hcd *hcd);
static int xhci_exynos_wake_lock(struct xhci_hcd_exynos *xhci_exynos,
				   int is_main_hcd, int is_lock);

static int is_rewa_enabled;

static struct xhci_hcd_exynos *g_xhci_exynos;
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
extern struct usb_xhci_pre_alloc xhci_pre_alloc;
extern struct hcd_hw_info *g_hwinfo;
extern int xhci_check_trb_in_td_math(struct xhci_hcd *xhci);

void xhci_exynos_ring_free(struct xhci_hcd *xhci, struct xhci_ring *ring);

struct xhci_ring *xhci_ring_alloc_uram(struct xhci_hcd *xhci,
		unsigned int num_segs, unsigned int cycle_state,
		enum xhci_ring_type type, unsigned int max_packet, gfp_t flags,
		u32 endpoint_type);
u32 ext_ep_type = 0;
static void *dma_pre_alloc_coherent(struct xhci_hcd *xhci, size_t size,
			 dma_addr_t *dma_handle, gfp_t gfp)
{
	struct usb_xhci_pre_alloc *xhci_alloc = g_xhci_exynos->xhci_alloc;
	u64 align = size % PAGE_SIZE;
	u64 b_offset = xhci_alloc->offset;

	if (align)
		xhci_alloc->offset = xhci_alloc->offset + size + (PAGE_SIZE - align);
	else
		xhci_alloc->offset = xhci_alloc->offset + size;

	*dma_handle = xhci_alloc->dma + b_offset;

	return (void *)xhci_alloc->pre_dma_alloc + b_offset;
}

static void xhci_exynos_usb_offload_enable_event_ring(struct xhci_hcd *xhci)
{
	u32 temp;
	u64 temp_64;

	temp_64 = xhci_read_64(xhci, &g_xhci_exynos->ir_set_audio->erst_dequeue);
	temp_64 &= ~ERST_PTR_MASK;
	xhci_info(xhci,	"ERST2 deq = 64'h%0lx", (unsigned long) temp_64);

	xhci_info(xhci, "// [USB Audio] Set the interrupt modulation register");
	temp = readl(&g_xhci_exynos->ir_set_audio->irq_control);
	temp &= ~ER_IRQ_INTERVAL_MASK;
	/*
	 * the increment interval is 8 times as much as that defined
	 * in xHCI spec on MTK's controller
	 */
	temp |= (u32) ((xhci->quirks & XHCI_MTK_HOST) ? 20 : 160);
	writel(temp, &g_xhci_exynos->ir_set_audio->irq_control);

	temp = readl(&g_xhci_exynos->ir_set_audio->irq_pending);
	xhci_info(xhci, "// [USB Audio] Enabling event ring interrupter %p by writing 0x%x to irq_pending",
			g_xhci_exynos->ir_set_audio, (unsigned int) ER_IRQ_ENABLE(temp));
	writel(ER_IRQ_ENABLE(temp), &g_xhci_exynos->ir_set_audio->irq_pending);
}

static void xhci_exynos_usb_offload_store_hw_info(struct xhci_hcd *xhci, struct usb_hcd *hcd, struct usb_device *udev)
{
	struct xhci_virt_device *virt_dev;
	struct xhci_erst_entry *entry = &g_xhci_exynos->erst_audio.entries[0];

	virt_dev = xhci->devs[udev->slot_id];

	g_hwinfo->dcbaa_dma = xhci->dcbaa->dma;
	g_hwinfo->save_dma = g_xhci_exynos->save_dma;
	g_hwinfo->cmd_ring = xhci->op_regs->cmd_ring;
	g_hwinfo->slot_id = udev->slot_id;
	g_hwinfo->in_dma = g_xhci_exynos->in_dma;
	g_hwinfo->in_buf = g_xhci_exynos->in_addr;
	g_hwinfo->out_dma = g_xhci_exynos->out_dma;
	g_hwinfo->out_buf = g_xhci_exynos->out_addr;
	g_hwinfo->in_ctx = virt_dev->in_ctx->dma;
	g_hwinfo->out_ctx = virt_dev->out_ctx->dma;
	g_hwinfo->erst_addr = entry->seg_addr;
	g_hwinfo->speed = udev->speed;
	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO)
		g_hwinfo->use_uram = true;
	else
		g_hwinfo->use_uram = false;

	pr_info("<<< %s\n", __func__);
}
static void xhci_exynos_set_hc_event_deq_audio(struct xhci_hcd *xhci)
{
	u64 temp;
	dma_addr_t deq;

	deq = xhci_trb_virt_to_dma(g_xhci_exynos->event_ring_audio->deq_seg,
			g_xhci_exynos->event_ring_audio->dequeue);
	if (deq == 0 && !in_interrupt())
		xhci_warn(xhci, "WARN something wrong with SW event ring "
				"dequeue ptr.\n");
	/* Update HC event ring dequeue pointer */
	temp = xhci_read_64(xhci, &g_xhci_exynos->ir_set_audio->erst_dequeue);
	temp &= ERST_PTR_MASK;
	/* Don't clear the EHB bit (which is RW1C) because
	 * there might be more events to service.
	 */
	temp &= ~ERST_EHB;
	xhci_info(xhci,	"//[%s] Write event ring dequeue pointer = 0x%llx, "
			"preserving EHB bit",__func__,
			((u64) deq & (u64) ~ERST_PTR_MASK) | temp);
	xhci_write_64(xhci, ((u64) deq & (u64) ~ERST_PTR_MASK) | temp,
			&g_xhci_exynos->ir_set_audio->erst_dequeue);
}

void xhci_exynos_parse_endpoint(struct xhci_hcd *xhci, struct usb_device *udev, struct usb_endpoint_descriptor *desc, struct xhci_container_ctx *ctx)
{
	struct usb_endpoint_descriptor *d = desc;
	int size = 0x100;
	unsigned ep_index;

	g_hwinfo->rawdesc_length = size;
	ep_index = xhci_get_endpoint_index(d);

	pr_info("udev = 0x%8x, Ep = 0x%x, desc = 0x%8x\n", udev, d->bEndpointAddress, d);

	if (( d->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK ) ==
				USB_ENDPOINT_XFER_ISOC) {
		if ((d->bmAttributes & USB_ENDPOINT_USAGE_MASK) ==
					USB_ENDPOINT_USAGE_FEEDBACK) {
			/* Only Feedback endpoint(Not implict feedback data endpoint) */
			if (d->bEndpointAddress & USB_ENDPOINT_DIR_MASK) {
				g_hwinfo->fb_in_ep =
					d->bEndpointAddress;
				pr_info("Feedback IN ISO endpoint #0%x 0x%x\n",
					d->bEndpointAddress, d->bSynchAddress);
			} else {
				g_hwinfo->fb_out_ep =
					d->bEndpointAddress;
				pr_info("Feedback OUT ISO endpoint #0%x 0x%x\n",
					d->bEndpointAddress, d->bSynchAddress);
			}
		} else {
			/* Data Stream Endpoint only */
			if (d->bEndpointAddress & USB_ENDPOINT_DIR_MASK) {
				if (d->bEndpointAddress != g_hwinfo->fb_in_ep) {
					g_hwinfo->in_ep =
						d->bEndpointAddress;
					pr_info(" This is IN ISO endpoint #0%x 0x%x\n",
						d->bEndpointAddress, d->bSynchAddress);
				} else {
					pr_info("IN ISO endpoint is same with FB #0%x\n",
						d->bEndpointAddress);
				}

				if ((d->bLength > 7) && (d->bSynchAddress != 0x0)) {
					g_hwinfo->fb_out_ep =
						d->bSynchAddress;
					pr_info("Feedback OUT ISO endpoint #0%x 0x%x\n",
						d->bEndpointAddress, d->bSynchAddress);
				}
			} else {
				g_hwinfo->out_ep =
					d->bEndpointAddress;
				pr_info(" This is OUT ISO endpoint #0%x 0x%x\n",
					d->bEndpointAddress, d->bSynchAddress);

				if ((d->bLength > 7) && (d->bSynchAddress != 0x0)) {
					g_hwinfo->fb_in_ep =
						d->bSynchAddress;
					pr_info("Feedback IN ISO endpoint #0%x 0x%x\n",
						d->bEndpointAddress, d->bSynchAddress);
				}
			}
		}
	}


}
int xhci_exynos_address_device(struct usb_hcd *hcd, struct usb_device *udev)
{
	struct xhci_hcd *xhci;
	int ret;

	pr_debug("%s +++", __func__);
	ret = xhci_address_device(hcd, udev);
	xhci = hcd_to_xhci(hcd);

	xhci_exynos_usb_offload_store_hw_info(xhci, hcd, udev);

	pr_debug("%s ---", __func__);
	return ret;
}
int xhci_exynos_add_endpoint(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint *ep)
{
	int ret;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	struct usb_endpoint_descriptor *d = &ep->desc;

	/* Check Feedback Endpoint */
	if ((d->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
				USB_ENDPOINT_XFER_ISOC) {
		if ((d->bmAttributes & USB_ENDPOINT_USAGE_MASK) ==
					USB_ENDPOINT_USAGE_FEEDBACK) {
			/* Only Feedback endpoint(Not implict feedback data endpoint) */
			if (d->bEndpointAddress & USB_ENDPOINT_DIR_MASK) {
				g_hwinfo->fb_in_ep =
					d->bEndpointAddress;
				pr_info("Feedback IN EP used #0%x 0x%x\n",
					d->bEndpointAddress, d->bSynchAddress);
			} else {
				g_hwinfo->fb_out_ep =
					d->bEndpointAddress;
				pr_info("Feedback OUT EP used #0%x 0x%x\n",
					d->bEndpointAddress, d->bSynchAddress);
			}
		}
	}

	/* Check Feedback EP is already allocated */
	if (g_hwinfo->fb_in_ep == d->bEndpointAddress ||
	    g_hwinfo->fb_out_ep == d->bEndpointAddress)
		g_hwinfo->feedback = 1;
	else
		g_hwinfo->feedback = 0;

	pr_debug("%s +++", __func__);
	ret = xhci_add_endpoint(hcd, udev, ep);
	g_hwinfo->feedback = 0;

	if (!ret && udev->slot_id) {
		xhci = hcd_to_xhci(hcd);
		virt_dev = xhci->devs[udev->slot_id];
		xhci_exynos_parse_endpoint(xhci, udev, &ep->desc, virt_dev->out_ctx);
	}
	pr_debug("%s ---", __func__);
	return ret;
}
static int xhci_exynos_alloc_event_ring(struct xhci_hcd *xhci, gfp_t flags)
{
	dma_addr_t	dma;
	unsigned int	val;
	u64		val_64;
	struct xhci_segment	*seg;

	g_xhci_exynos->ir_set_audio = &xhci->run_regs->ir_set[1];
	g_xhci_exynos->save_addr = dma_pre_alloc_coherent(xhci,
			sizeof(PAGE_SIZE), &dma, flags);
	g_xhci_exynos->save_dma = dma;
	xhci_info(xhci, "// Save address = 0x%llx (DMA), %p (virt)",
			(unsigned long long)g_xhci_exynos->save_dma, g_xhci_exynos->save_addr);

	if ((xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO)) {
		/* for AUDIO erst */
		g_xhci_exynos->event_ring_audio = xhci_ring_alloc_uram(xhci, ERST_NUM_SEGS,
				1, TYPE_EVENT, 0, flags, 0);
		if (!g_xhci_exynos->event_ring_audio)
			goto fail;

		g_xhci_exynos->erst_audio.entries = ioremap(EXYNOS_URAM_ABOX_ERST_SEG_ADDR,
				sizeof(struct xhci_erst_entry) * ERST_NUM_SEGS);
		if (!g_xhci_exynos->erst_audio.entries)
			goto fail;

		dma = EXYNOS_URAM_ABOX_ERST_SEG_ADDR;
		xhci_info(xhci, "ABOX audio ERST allocated at 0x%x",
					EXYNOS_URAM_ABOX_ERST_SEG_ADDR);
	} else {
		/* for AUDIO erst */
		g_xhci_exynos->event_ring_audio = xhci_ring_alloc(xhci, ERST_NUM_SEGS, 1,
					TYPE_EVENT, 0, flags);
		if (!g_xhci_exynos->event_ring_audio)
			goto fail;
		if (xhci_check_trb_in_td_math(xhci) < 0)
			goto fail;
		g_xhci_exynos->erst_audio.entries = dma_pre_alloc_coherent(xhci,
				sizeof(struct xhci_erst_entry) * ERST_NUM_SEGS, &dma,
				flags);
		if (!g_xhci_exynos->erst_audio.entries)
			goto fail;
	}
	xhci_info(xhci,	"// Allocated event ring segment table at 0x%llx",
			(unsigned long long)dma);

	memset(g_xhci_exynos->erst_audio.entries, 0, sizeof(struct xhci_erst_entry) *
							ERST_NUM_SEGS);
	g_xhci_exynos->erst_audio.num_entries = ERST_NUM_SEGS;
	g_xhci_exynos->erst_audio.erst_dma_addr = dma;
	xhci_info(xhci,	"// Set ERST to 0; private num segs = %i, virt addr = %p, dma addr = 0x%llx",
			xhci->erst.num_entries,
			xhci->erst.entries,
			(unsigned long long)xhci->erst.erst_dma_addr);

	/* set ring base address and size for each segment table entry */
	for (val = 0, seg = g_xhci_exynos->event_ring_audio->first_seg;
					val < ERST_NUM_SEGS; val++) {
		struct xhci_erst_entry *entry = &g_xhci_exynos->erst_audio.entries[val];

		entry->seg_addr = cpu_to_le64(seg->dma);
		entry->seg_size = cpu_to_le32(TRBS_PER_SEGMENT);
		entry->rsvd = 0;
		seg = seg->next;
	}

	/* set ERST count with the number of entries in the segment table */
	val = readl(&g_xhci_exynos->ir_set_audio->erst_size);
	val &= ERST_SIZE_MASK;
	val |= ERST_NUM_SEGS;
	xhci_info(xhci, "// Write ERST size = %i to ir_set 0 (some bits preserved)",
			val);
	writel(val, &g_xhci_exynos->ir_set_audio->erst_size);

	xhci_info(xhci, "// Set ERST entries to point to event ring.");
	/* set the segment table base address */
	xhci_info(xhci,	"// Set ERST base address for ir_set 0 = 0x%llx",
			(unsigned long long)g_xhci_exynos->erst_audio.erst_dma_addr);
	val_64 = xhci_read_64(xhci, &g_xhci_exynos->ir_set_audio->erst_base);
	val_64 &= ERST_PTR_MASK;
	val_64 |= (g_xhci_exynos->erst_audio.erst_dma_addr & (u64) ~ERST_PTR_MASK);
	xhci_write_64(xhci, val_64, &g_xhci_exynos->ir_set_audio->erst_base);

	/* Set the event ring dequeue address */
	xhci_exynos_set_hc_event_deq_audio(xhci);
	xhci_info(xhci,	"// Wrote ERST address to ir_set 1.");

	return 0;
fail:
	return -1;
}

static void xhci_exynos_free_event_ring(struct xhci_hcd *xhci)
{
	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO)
		iounmap(g_xhci_exynos->erst_audio.entries);
	else
		g_xhci_exynos->erst_audio.entries = NULL;

	xhci_info(xhci, "%s: Freed ERST for Audio offloading", __func__);

	if (g_xhci_exynos->event_ring_audio)
		xhci_exynos_ring_free(xhci, g_xhci_exynos->event_ring_audio);
	g_xhci_exynos->event_ring_audio = NULL;
	xhci_info(xhci, "%s: Freed event ring for Audio offloading", __func__);
}

static void xhci_exynos_free_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx)
{
	/* Ignore dma_pool_free if it is allocated from URAM */
	if (ctx->dma != EXYNOS_URAM_DEVICE_CTX_ADDR)
		dma_pool_free(xhci->device_pool, ctx->bytes, ctx->dma);
}

static void xhci_exynos_alloc_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx, int type, gfp_t flags)
{
	if (type != XHCI_CTX_TYPE_INPUT && g_xhci_exynos->exynos_uram_ctx_alloc == 0 &&
			xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		/* Only first Device Context uses URAM */
		int i;
		ctx->bytes = ioremap(EXYNOS_URAM_DEVICE_CTX_ADDR, 2112);
		if (!ctx->bytes)
			return;

		for (i = 0; i < 2112; i++)
			ctx->bytes[i] = 0;

		ctx->dma = EXYNOS_URAM_DEVICE_CTX_ADDR;
		g_xhci_exynos->usb_audio_ctx_addr = ctx->bytes;
		g_xhci_exynos->exynos_uram_ctx_alloc = 1;
		xhci_info(xhci, "First device context allocated at URAM(%x)",
					EXYNOS_URAM_DEVICE_CTX_ADDR);
	} else {
		ctx->bytes = dma_pool_zalloc(xhci->device_pool, flags, &ctx->dma);
		if (!ctx->bytes)
			return;
	}
}

static struct xhci_ring *xhci_exynos_alloc_transfer_ring(struct xhci_hcd *xhci,
		u32 endpoint_type, enum xhci_ring_type ring_type, unsigned int max_packet, gfp_t mem_flags)
{
	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		/* If URAM is not allocated, it try to allocate from URAM */
		if (g_xhci_exynos->exynos_uram_isoc_out_alloc == 0 &&
						endpoint_type == ISOC_OUT_EP) {
			xhci_info(xhci, "First ISOC OUT ring is allocated from URAM.\n");
			return xhci_ring_alloc_uram(xhci, 1, 1, ring_type,
							max_packet, mem_flags,
							endpoint_type);

			g_xhci_exynos->exynos_uram_isoc_out_alloc = 1;
		} else if (g_xhci_exynos->exynos_uram_isoc_in_alloc == 0 &&
					endpoint_type == ISOC_IN_EP &&
					EXYNOS_URAM_ISOC_IN_RING_ADDR != 0x0) {
			xhci_info(xhci, "First ISOC IN ring is allocated from URAM.\n");
			return xhci_ring_alloc_uram(xhci, 1, 1, ring_type,
							max_packet, mem_flags,
							endpoint_type);

			g_xhci_exynos->exynos_uram_isoc_in_alloc = 1;
		} else {
			return xhci_ring_alloc(xhci, 2, 1, ring_type,
							max_packet, mem_flags);
		}

	} else {
		return xhci_ring_alloc(xhci, 2, 1, ring_type, max_packet, mem_flags);
	}
}

void xhci_exynos_segment_free_skip(struct xhci_hcd *xhci, struct xhci_segment *seg)
{
	if (seg->trbs) {
			/* Check URAM address for memory free */
			if (seg->dma == EXYNOS_URAM_ABOX_EVT_RING_ADDR) {
				iounmap(seg->trbs);
			} else if (seg->dma == EXYNOS_URAM_ISOC_OUT_RING_ADDR) {
				g_xhci_exynos->exynos_uram_isoc_out_alloc = 0;
				if (in_interrupt())
					g_xhci_exynos->usb_audio_isoc_out_addr = (u8 *)seg->trbs;
				else
					iounmap(seg->trbs);
			} else if (seg->dma == EXYNOS_URAM_ISOC_IN_RING_ADDR) {
				g_xhci_exynos->exynos_uram_isoc_in_alloc = 0;
				if (in_interrupt())
					g_xhci_exynos->usb_audio_isoc_in_addr = (u8 *)seg->trbs;
				else
					iounmap(seg->trbs);
			} else
				dma_pool_free(xhci->segment_pool, seg->trbs, seg->dma);

		seg->trbs = NULL;
	}
	kfree(seg->bounce_buf);
	kfree(seg);
}

void xhci_exynos_free_segments_for_ring(struct xhci_hcd *xhci,
				struct xhci_segment *first)
{
	struct xhci_segment *seg;

	seg = first->next;

	if (!seg)
		xhci_err(xhci, "segment is null unexpectedly\n");

	while (seg != first) {
		struct xhci_segment *next = seg->next;
		xhci_exynos_segment_free_skip(xhci, seg);
		seg = next;
	}
	xhci_exynos_segment_free_skip(xhci, first);
}

static void xhci_exynos_remove_segment_mapping(struct radix_tree_root *trb_address_map,
		struct xhci_segment *seg)
{
	unsigned long key;

	key = (unsigned long)(seg->dma >> TRB_SEGMENT_SHIFT);
	if (radix_tree_lookup(trb_address_map, key))
		radix_tree_delete(trb_address_map, key);
}

static void xhci_exynos_remove_stream_mapping(struct xhci_ring *ring)
{
	struct xhci_segment *seg;

	if (WARN_ON_ONCE(ring->trb_address_map == NULL))
		return;

	seg = ring->first_seg;
	do {
		xhci_exynos_remove_segment_mapping(ring->trb_address_map, seg);
		seg = seg->next;
	} while (seg != ring->first_seg);
}

void xhci_exynos_ring_free(struct xhci_hcd *xhci, struct xhci_ring *ring)
{
	if (!ring)
		return;

	//trace_xhci_ring_free(ring);

	if (ring->first_seg) {
		if (ring->type == TYPE_STREAM)
			xhci_exynos_remove_stream_mapping(ring);

		xhci_exynos_free_segments_for_ring(xhci, ring->first_seg);
	}

	kfree(ring);
}

int endpoint_ring_table[31];

static void xhci_exynos_free_transfer_ring(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev, unsigned int ep_index)
{
	if (endpoint_ring_table[ep_index]) {
		pr_info("%s: endpoint %d has been already free-ing\n",
			__func__, ep_index);
	}
	if (xhci->quirks & BIT_ULL(60)) {
		pr_info("%s: endpoint %d free is requested from reset_bandwidth\n",
			__func__, ep_index);
		xhci_exynos_ring_free(xhci, virt_dev->eps[ep_index].new_ring);
		xhci->quirks &= ~(BIT_ULL(60));
		return;
	}
	endpoint_ring_table[ep_index] = 1;
	xhci_exynos_ring_free(xhci, virt_dev->eps[ep_index].ring);
	endpoint_ring_table[ep_index] = 0;

	return;
}

static int xhci_exynos_vendor_init(struct xhci_hcd *xhci)
{
	return 0;

}
static void xhci_exynos_vendor_cleanup(struct xhci_hcd *xhci)
{
	xhci_exynos_free_event_ring(xhci);

	return;
}

static bool xhci_exynos_is_usb_offload_enabled(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev, unsigned int ep_index)
{
	return true;
}

static struct xhci_device_context_array *xhci_exynos_alloc_dcbaa(
		struct xhci_hcd *xhci, gfp_t flags)
{
	dma_addr_t dma;

	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		int i;

		xhci_info(xhci, "DCBAA is allocated at 0x%x(URAM)",
						EXYNOS_URAM_DCBAA_ADDR);
		/* URAM allocation for DCBAA */
		xhci->dcbaa = ioremap(EXYNOS_URAM_DCBAA_ADDR,
						sizeof(*xhci->dcbaa));
		if (!xhci->dcbaa)
			return NULL;
		/* Clear DCBAA */
		for (i = 0; i < MAX_HC_SLOTS; i++)
			xhci->dcbaa->dev_context_ptrs[i] = 0x0;

		xhci->dcbaa->dma = EXYNOS_URAM_DCBAA_ADDR;
	} else {
		xhci->dcbaa = dma_pre_alloc_coherent(xhci, sizeof(*xhci->dcbaa),
				&dma, flags);
		if (!xhci->dcbaa)
			return NULL;
		memset(xhci->dcbaa, 0, sizeof *(xhci->dcbaa));
		xhci->dcbaa->dma = dma;
	}
	return xhci->dcbaa;
}

static void xhci_exynos_free_dcbaa(struct xhci_hcd *xhci)
{
	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		iounmap(xhci->dcbaa);
		if (g_xhci_exynos->usb_audio_ctx_addr != NULL) {
			iounmap(g_xhci_exynos->usb_audio_ctx_addr);
			g_xhci_exynos->usb_audio_ctx_addr = NULL;
		}
		if (g_xhci_exynos->usb_audio_isoc_out_addr != NULL) {
			iounmap(g_xhci_exynos->usb_audio_isoc_out_addr);
			g_xhci_exynos->usb_audio_isoc_out_addr = NULL;
		}
		if (g_xhci_exynos->usb_audio_isoc_in_addr != NULL) {
			iounmap(g_xhci_exynos->usb_audio_isoc_in_addr);
			g_xhci_exynos->usb_audio_isoc_in_addr = NULL;
		}
	} else
		xhci->dcbaa = NULL;
}
/* URAM Allocation Functions */
extern void xhci_segment_free(struct xhci_hcd *xhci, struct xhci_segment *seg);
extern void xhci_link_segments(struct xhci_segment *prev,
		struct xhci_segment *next,
		enum xhci_ring_type type, bool chain_links);
extern void xhci_initialize_ring_info(struct xhci_ring *ring,
					unsigned int cycle_state);

static struct xhci_segment *xhci_segment_alloc_uram(struct xhci_hcd *xhci,
					       unsigned int cycle_state,
					       unsigned int max_packet,
					       gfp_t flags)
{
	struct xhci_segment *seg;
	dma_addr_t	dma;
	int		i;
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	seg = kzalloc_node(sizeof(*seg), flags, dev_to_node(dev));
	if (!seg)
		return NULL;

	seg->trbs = ioremap(EXYNOS_URAM_ABOX_EVT_RING_ADDR, TRB_SEGMENT_SIZE);
	if (!seg->trbs)
		return NULL;

	dma = EXYNOS_URAM_ABOX_EVT_RING_ADDR;

	if (max_packet) {
		seg->bounce_buf = kzalloc_node(max_packet, flags,
					dev_to_node(dev));
		if (!seg->bounce_buf) {
			dma_pool_free(xhci->segment_pool, seg->trbs, dma);
			kfree(seg);
			return NULL;
		}
	}
	/* If the cycle state is 0, set the cycle bit to 1 for all the TRBs */
	if (cycle_state == 0) {
		for (i = 0; i < TRBS_PER_SEGMENT; i++)
			seg->trbs[i].link.control |= cpu_to_le32(TRB_CYCLE);
	}
	seg->dma = dma;
	xhci_info(xhci, "ABOX Event Ring is allocated at 0x%x",
					EXYNOS_URAM_ABOX_EVT_RING_ADDR);
	seg->next = NULL;

	return seg;
}

static struct xhci_segment *xhci_segment_alloc_uram_ep(struct xhci_hcd *xhci,
					       unsigned int cycle_state,
					       unsigned int max_packet,
					       gfp_t flags, int seg_num,
					       u32 endpoint_type)
{
	struct xhci_segment *seg;
	dma_addr_t	dma;
	int		i;
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	seg = kzalloc_node(sizeof(*seg), flags, dev_to_node(dev));
	if (!seg)
		return NULL;

	if (seg_num != 0) {
		/* Support just one segment */
		xhci_err(xhci, "%s : Unexpected SEG NUMBER!\n", __func__);
		return NULL;
	}

	if (endpoint_type == ISOC_OUT_EP) {
		if (!g_hwinfo->feedback)
			seg->trbs = ioremap(EXYNOS_URAM_ISOC_OUT_RING_ADDR,
					    TRB_SEGMENT_SIZE);
		else
			seg->trbs = dma_pool_zalloc(xhci->segment_pool, flags, &dma);

		if (!seg->trbs)
			return NULL;

		if (!g_hwinfo->feedback)
			dma = EXYNOS_URAM_ISOC_OUT_RING_ADDR;

		xhci_info(xhci, "First ISOC-OUT Ring is allocated at 0x%8x", dma);
	} else if (endpoint_type == ISOC_IN_EP) {
		if (!g_hwinfo->feedback)
			seg->trbs = ioremap(EXYNOS_URAM_ISOC_IN_RING_ADDR,
					    TRB_SEGMENT_SIZE);
		else
			seg->trbs = dma_pool_zalloc(xhci->segment_pool, flags, &dma);

		if (!seg->trbs)
			return NULL;

		if (!g_hwinfo->feedback)
			dma = EXYNOS_URAM_ISOC_IN_RING_ADDR;

		xhci_info(xhci, "First ISOC-IN Ring is allocated at 0x%8x", dma);
	} else {
		xhci_err(xhci, "%s : Unexpected EP Type!\n", __func__);
		return NULL;
	}

	for (i = 0; i < 256; i++) {
		seg->trbs[i].link.segment_ptr = 0;
		seg->trbs[i].link.intr_target = 0;
		seg->trbs[i].link.control = 0;
	}


	if (max_packet) {
		seg->bounce_buf = kzalloc_node(max_packet, flags,
					dev_to_node(dev));
		if (!seg->bounce_buf) {
			dma_pool_free(xhci->segment_pool, seg->trbs, dma);
			kfree(seg);
			return NULL;
		}
	}
	/* If the cycle state is 0, set the cycle bit to 1 for all the TRBs */
	if (cycle_state == 0) {
		for (i = 0; i < TRBS_PER_SEGMENT; i++)
			seg->trbs[i].link.control |= cpu_to_le32(TRB_CYCLE);
	}
	seg->dma = dma;
	seg->next = NULL;

	return seg;
}

static int xhci_alloc_segments_for_ring_uram(struct xhci_hcd *xhci,
		struct xhci_segment **first, struct xhci_segment **last,
		unsigned int num_segs, unsigned int cycle_state,
		enum xhci_ring_type type, unsigned int max_packet, gfp_t flags,
		u32 endpoint_type)
{
	struct xhci_segment *prev;
	bool chain_links;

	/* Set chain bit for 0.95 hosts, and for isoc rings on AMD 0.96 host */
	chain_links = !!(xhci_link_trb_quirk(xhci) ||
			 (type == TYPE_ISOC &&
			  (xhci->quirks & XHCI_AMD_0x96_HOST)));

	if (type == TYPE_ISOC) {
		prev = xhci_segment_alloc_uram_ep(xhci, cycle_state,
							max_packet, flags, 0,
							endpoint_type);
	} else if (type == TYPE_EVENT) {
		prev = xhci_segment_alloc_uram(xhci, cycle_state, max_packet, flags);
	} else {
		xhci_err(xhci, "Unexpected TYPE for URAM allocation!\n");
		return -ENOMEM;
	}

	if (!prev)
		return -ENOMEM;
	num_segs--;

	*first = prev;
	while (num_segs > 0) {
		struct xhci_segment	*next = NULL;

		if (type == TYPE_ISOC) {
			prev = xhci_segment_alloc_uram_ep(xhci, cycle_state,
							max_packet, flags, 1,
							endpoint_type);
		} else if (type == TYPE_EVENT) {
			next = xhci_segment_alloc_uram(xhci, cycle_state,
							max_packet, flags);
		} else {
			xhci_err(xhci, "Unexpected TYPE for URAM alloc(multi)!\n");
			return -ENOMEM;
		}

		if (!next) {
			prev = *first;
			while (prev) {
				next = prev->next;
				xhci_segment_free(xhci, prev);
				prev = next;
			}
			return -ENOMEM;
		}
		xhci_link_segments(prev, next, type, chain_links);

		prev = next;
		num_segs--;
	}
	xhci_link_segments(prev, *first, type, chain_links);
	*last = prev;

	return 0;
}

struct xhci_ring *xhci_ring_alloc_uram(struct xhci_hcd *xhci,
		unsigned int num_segs, unsigned int cycle_state,
		enum xhci_ring_type type, unsigned int max_packet, gfp_t flags,
		u32 endpoint_type)
{
	struct xhci_ring	*ring;
	int ret;
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	ring = kzalloc_node(sizeof(*ring), flags, dev_to_node(dev));
	if (!ring)
		return NULL;

	ring->num_segs = num_segs;
	ring->bounce_buf_len = max_packet;
	INIT_LIST_HEAD(&ring->td_list);
	ring->type = type;
	if (num_segs == 0)
		return ring;

	ret = xhci_alloc_segments_for_ring_uram(xhci, &ring->first_seg,
			&ring->last_seg, num_segs, cycle_state, type,
			max_packet, flags, endpoint_type);
	if (ret)
		goto fail;

	/* Only event ring does not use link TRB */
	if (type != TYPE_EVENT) {
		/* See section 4.9.2.1 and 6.4.4.1 */
		ring->last_seg->trbs[TRBS_PER_SEGMENT - 1].link.control |=
			cpu_to_le32(LINK_TOGGLE);
	}
	xhci_initialize_ring_info(ring, cycle_state);
	//trace_xhci_ring_alloc(ring);
	return ring;

fail:
	kfree(ring);
	return NULL;
}
//#ifdef CONFIG_SND_EXYNOS_USB_AUDIO

static unsigned int xhci_exynos_get_endpoint_address(unsigned int ep_index)
{
	unsigned int number = DIV_ROUND_UP(ep_index, 2);
	unsigned int direction = ep_index % 2 ? USB_DIR_OUT : USB_DIR_IN;
	return direction | number;
}

int xhci_exynos_sync_dev_ctx(struct xhci_hcd *xhci, unsigned int slot_id)
{
	struct xhci_virt_device *virt_dev;
	struct xhci_slot_ctx *slot_ctx;

	int i;
	int last_ep;
	int last_ep_ctx = 31;

	/* Use default hwinfo in case
	 * there are no audio devices occupied
	 */
	virt_dev = xhci->devs[slot_id];
	slot_ctx = xhci_get_slot_ctx(xhci, virt_dev->in_ctx);

	last_ep = LAST_CTX_TO_EP_NUM(le32_to_cpu(slot_ctx->dev_info));

	if (last_ep < 31)
		last_ep_ctx = last_ep + 1;
	for (i = 0; i < last_ep_ctx; ++i) {
		unsigned int epaddr = xhci_exynos_get_endpoint_address(i);
		struct xhci_ep_ctx *ep_ctx = xhci_get_ep_ctx(xhci, virt_dev->out_ctx, i);

		if (epaddr == g_hwinfo->in_ep) {
			pr_info("[%s] ep%d set in deq : %#08llx\n",
					__func__, i, ep_ctx->deq);
			g_hwinfo->old_in_deq = g_hwinfo->in_deq;
			g_hwinfo->in_deq = ep_ctx->deq;
		} else if (epaddr == g_hwinfo->out_ep) {
			pr_info("[%s] ep%d set out deq : %#08llx\n",
					__func__, i, ep_ctx->deq);
			g_hwinfo->old_out_deq = g_hwinfo->out_deq;
			g_hwinfo->out_deq = ep_ctx->deq;
		} else if (epaddr == g_hwinfo->fb_out_ep) {
			pr_info("[%s] ep%d set fb out deq : %#08llx\n",
					__func__, i, ep_ctx->deq);
			g_hwinfo->fb_old_out_deq = g_hwinfo->fb_out_deq;
			g_hwinfo->fb_out_deq = ep_ctx->deq;
		} else if (epaddr == g_hwinfo->fb_in_ep) {
			pr_info("[%s] ep%d set fb in deq : %#08llx\n",
					__func__, i, ep_ctx->deq);
			g_hwinfo->fb_old_in_deq = g_hwinfo->fb_in_deq;
			g_hwinfo->fb_in_deq = ep_ctx->deq;
		}
	}

	return 0;

}
#endif

extern struct usb_xhci_pre_alloc xhci_pre_alloc;
static const struct xhci_driver_overrides xhci_exynos_overrides __initconst = {
	.extra_priv_size = sizeof(struct xhci_exynos_priv),
	.reset = xhci_exynos_setup,
	.start = xhci_exynos_start,
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	.add_endpoint = xhci_exynos_add_endpoint,
	.address_device = xhci_exynos_address_device,
#endif
	.bus_suspend = xhci_exynos_bus_suspend,
	.bus_resume = xhci_exynos_bus_resume,
};

int xhci_exynos_bus_suspend(struct usb_hcd *hcd)
{
	struct xhci_exynos_priv *priv = hcd_to_xhci_exynos_priv(hcd);
	struct xhci_hcd_exynos *xhci_exynos = priv->xhci_exynos;
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	int ret, main_hcd;

	pr_info("%s, priv = 0x%8x, xhci_exynos = 0x%8x\n",
		__func__, priv, xhci_exynos);

	if (hcd == xhci->main_hcd)
		main_hcd = 1;
	else
		main_hcd = 0;

	ret = xhci_bus_suspend(hcd);

#if 0
	if (hcd == xhci->main_hcd &&
	    xhci_exynos->port_state == PORT_USB2) {
		xhci_info(xhci, "port is connected, phy vendor set\n");
		ret = exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb2, 1, 0);
		if (ret) {
			xhci_info(xhci, "phy vendor set fail\n");
			//return ret;
		}
	}
#endif

	xhci_exynos_wake_lock(xhci_exynos, main_hcd, 0);

	return ret;
}

int xhci_exynos_bus_resume(struct usb_hcd *hcd)
{
	struct xhci_exynos_priv *priv = hcd_to_xhci_exynos_priv(hcd);
	struct xhci_hcd_exynos *xhci_exynos = priv->xhci_exynos;
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);

	int ret, main_hcd;

	if (hcd == xhci->main_hcd)
		main_hcd = 1;
	else
		main_hcd = 0;

	pr_info("%s, hcd = 0x%8x\n", __func__, hcd);

#if 0
	if (hcd == xhci->main_hcd &&
	    xhci_exynos->port_state == PORT_USB2) {
		xhci_info(xhci, "[%s] phy vendor set\n", __func__);
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb2, 1, 1);
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb2, 0, 0);
	}
#endif

	ret = xhci_bus_resume(hcd);

	xhci_exynos_wake_lock(xhci_exynos, main_hcd, 1);

	return ret;
}

static void xhci_priv_exynos_start(struct usb_hcd *hcd)
{
	struct xhci_exynos_priv *priv = hcd_to_xhci_exynos_priv(hcd);

	if (priv->plat_start)
		priv->plat_start(hcd);
}

void xhci_exynos_portsc_power_off(void __iomem *portsc, u32 on, u32 prt)
{
	struct xhci_hcd_exynos	*xhci_exynos = g_xhci_exynos;

	u32 reg;

	spin_lock(&xhci_exynos->xhcioff_lock);

	pr_info("%s, on=%d portsc_control_priority=%d, prt=%d\n",
		__func__, on,  xhci_exynos->portsc_control_priority, prt);

	if (xhci_exynos->portsc_control_priority > prt) {
		spin_unlock(&xhci_exynos->xhcioff_lock);
		return;
	}

	xhci_exynos->portsc_control_priority = prt;

	if (on && !xhci_exynos->port_off_done) {
		pr_info("%s, Do not switch-on port\n", __func__);
		spin_unlock(&xhci_exynos->xhcioff_lock);
		return;
	}

	reg = readl(portsc);

	if (on)
		reg |= PORT_POWER;
	else
		reg &= ~PORT_POWER;

	writel(reg, portsc);
	reg = readl(portsc);

	pr_info("power %s portsc, reg = 0x%x addr = %8x\n",
		on ? "on" : "off", reg, (u64)portsc);

#if 0
	reg = readl(phycon_base_addr + 0x70);
	if (on)
		reg &= ~DIS_RX_DETECT;
	else
		reg |= DIS_RX_DETECT;

	writel(reg, phycon_base_addr + 0x70);
	pr_info("phycon ess_ctrl = 0x%x\n", readl(phycon_base_addr + 0x70));
#endif

	if (on)
		xhci_exynos->port_off_done = 0;
	else
		xhci_exynos->port_off_done = 1;

	spin_unlock(&xhci_exynos->xhcioff_lock);
}

int xhci_exynos_portsc_set(u32 on)
{
	if (g_xhci_exynos->usb3_portsc != NULL && !on) {
		xhci_exynos_portsc_power_off(g_xhci_exynos->usb3_portsc, 0, 2);
		g_xhci_exynos->port_set_delayed = 0;
		return 0;
	}

	if (!on)
		g_xhci_exynos->port_set_delayed = 1;

	pr_info("%s, usb3_portsc is NULL\n", __func__);
	return -EIO;
}

int xhci_exynos_port_power_set(u32 on, u32 prt)
{
	if (g_xhci_exynos->usb3_portsc) {
		xhci_exynos_portsc_power_off(g_xhci_exynos->usb3_portsc, on, prt);
		return 0;
	}

	pr_info("%s, usb3_portsc is NULL\n", __func__);
	return -EIO;
}
EXPORT_SYMBOL_GPL(xhci_exynos_port_power_set);

static int xhci_exynos_check_port(struct usb_device *dev, bool on)
{
	struct usb_device *hdev;
	struct usb_device *udev = dev;
	struct device *ddev = &udev->dev;
	struct xhci_hcd_exynos	*xhci_exynos = g_xhci_exynos;
	enum usb_port_state pre_state;
	int usb3_hub_detect = 0;
	int usb2_detect = 0;
	int port;
	int bInterfaceClass = 0;

	if (udev->bus->root_hub == udev) {
		pr_info("this dev is a root hub\n");
		goto skip;
	}

	pre_state = xhci_exynos->port_state;

	/* Find root hub */
	hdev = udev->parent;
	if (!hdev)
		goto skip;

	hdev = dev->bus->root_hub;
	if (!hdev)
		goto skip;
	pr_info("root hub maxchild = %d\n", hdev->maxchild);

	/* check all ports */
	usb_hub_for_each_child(hdev, port, udev) {
		dev_dbg(ddev, "%s, class = %d, speed = %d\n",
			__func__, udev->descriptor.bDeviceClass,
						udev->speed);
		dev_dbg(ddev, "udev = 0x%8x, state = %d\n", udev, udev->state);
		if (udev && udev->state == USB_STATE_CONFIGURED) {
			if (!dev->config->interface[0])
				continue;

			bInterfaceClass	= udev->config->interface[0]
					->cur_altsetting->desc.bInterfaceClass;
			if (on) {
#if defined(CONFIG_USB_HOST_SAMSUNG_FEATURE)
				if (bInterfaceClass == USB_CLASS_AUDIO) {
#else
				if (bInterfaceClass == USB_CLASS_HID ||
				    bInterfaceClass == USB_CLASS_AUDIO) {
#endif	
					udev->do_remote_wakeup =
						(udev->config->desc.bmAttributes &
							USB_CONFIG_ATT_WAKEUP) ? 1 : 0;
					if (udev->do_remote_wakeup == 1) {
						device_init_wakeup(ddev, 1);
						usb_enable_autosuspend(dev);
					}
					dev_dbg(ddev, "%s, remote_wakeup = %d\n",
						__func__, udev->do_remote_wakeup);
				}
			}
			if (bInterfaceClass == USB_CLASS_HUB) {
				xhci_exynos->port_state = PORT_HUB;
				usb3_hub_detect = 1;
				break;
			} else if (bInterfaceClass == USB_CLASS_BILLBOARD) {
				xhci_exynos->port_state = PORT_DP;
				usb3_hub_detect = 1;
				break;
			}

			if (udev->speed >= USB_SPEED_SUPER) {
				xhci_exynos->port_state = PORT_USB3;
				usb3_hub_detect = 1;
				break;
			} else {
				xhci_exynos->port_state = PORT_USB2;
				usb2_detect = 1;
			}
		} else {
			pr_info("not configured, state = %d\n", udev->state);
		}
	}

	if (!usb3_hub_detect && !usb2_detect)
		xhci_exynos->port_state = PORT_EMPTY;

	pr_info("%s %s state pre=%d now=%d\n", __func__,
		on ? "on" : "off", pre_state, xhci_exynos->port_state);

	return xhci_exynos->port_state;

skip:
	return -EINVAL;
}

static void xhci_exynos_set_port(struct usb_device *dev, bool on)
{
	//cancel_delayed_work_sync(&g_dwc->usb_qos_lock_delayed_work);

	switch (xhci_exynos_check_port(dev, on)) {
	case PORT_EMPTY:
		pr_info("Port check empty\n");
		is_otg_only = 1;
		usb_power_notify_control(0, 1);
		g_xhci_exynos->usb3_phy_control = true;
		xhci_exynos_port_power_set(1, 1);
		//dwc3_otg_qos_lock(g_dwc, 1);
		break;
	case PORT_USB2:
		pr_info("Port check usb2\n");
		is_otg_only = 0;
		xhci_exynos_port_power_set(0, 1);
		usb_power_notify_control(0, 0);
		g_xhci_exynos->usb3_phy_control = false;
		//schedule_delayed_work(&g_dwc->usb_qos_lock_delayed_work,
		//		msecs_to_jiffies(USB_BUS_CLOCK_DELAY_MS));
		break;
	case PORT_USB3:
		/* xhci_port_power_set(1, 1); */
		is_otg_only = 0;
		pr_info("Port check usb3\n");
		break;
	case PORT_HUB:
		/*xhci_exynos_port_power_set(1, 1);*/
		pr_info("Port check hub\n");
		is_otg_only = 0;
		break;
	case PORT_DP:
		/*xhci_exynos_port_power_set(1, 1);*/
		pr_info("Port check DP\n");
		is_otg_only = 0;
		break;
	default:
		break;
	}
}

int xhci_exynos_inform_dp_use(int use, int lane_cnt)
{
	int ret = 0;

	pr_info("[%s] dp use = %d, lane_cnt = %d\n",
			__func__, use, lane_cnt);

	if ((use == 1) && (lane_cnt == 4)) {
#ifdef CONFIG_EXYNOS_USBDRD_PHY30
		exynos_usbdrd_dp_use_notice(lane_cnt);
		if (g_xhci_exynos->usb3_phy_control == 0)
			usb_power_notify_control(1, 1);
#endif
		ret = xhci_exynos_portsc_set(0);
		udelay(1);
	}

	return ret;
}
EXPORT_SYMBOL(xhci_exynos_inform_dp_use);

static int xhci_exynos_power_notify(struct notifier_block *self,
			    unsigned long action, void *dev)
{
	switch (action) {
	case USB_DEVICE_ADD:
		xhci_exynos_set_port(dev, 1);
		break;
	case USB_DEVICE_REMOVE:
		xhci_exynos_set_port(dev, 0);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block dev_nb = {
	.notifier_call = xhci_exynos_power_notify,
};

void xhci_exynos_register_notify(void)
{
	is_otg_only = 1;
	g_xhci_exynos->port_state = PORT_EMPTY;
	usb_register_notify(&dev_nb);
}

void xhci_exynos_unregister_notify(void)
{
	usb_unregister_notify(&dev_nb);
}

static int xhci_priv_init_quirk(struct usb_hcd *hcd)
{
	struct xhci_exynos_priv *priv = hcd_to_xhci_exynos_priv(hcd);

	if (!priv->init_quirk)
		return 0;

	return priv->init_quirk(hcd);
}

static int xhci_priv_suspend_quirk(struct usb_hcd *hcd)
{
	struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

	if (!priv->suspend_quirk)
		return 0;

	return priv->suspend_quirk(hcd);
}

static int xhci_priv_resume_quirk(struct usb_hcd *hcd)
{
	struct xhci_exynos_priv *priv = hcd_to_xhci_exynos_priv(hcd);

	if (!priv->resume_quirk)
		return 0;

	return priv->resume_quirk(hcd);
}

static void xhci_exynos_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	struct xhci_exynos_priv *priv = xhci_to_exynos_priv(xhci);

	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_PLAT | priv->quirks;
}

/* called during probe() after chip reset completes */
static int xhci_exynos_setup(struct usb_hcd *hcd)
{
	int ret;
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
#endif

	ret = xhci_priv_init_quirk(hcd);
	if (ret)
		return ret;

	ret = xhci_gen_setup(hcd, xhci_exynos_quirks);
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	pr_debug("%s: alloc_event_ring!\n", __func__);
	xhci_exynos_alloc_event_ring(xhci, GFP_KERNEL);
#endif

#if 0
	/*
	 * DWC3 WORKAROUND: xhci reset clears PHY CR port settings,
	 * so USB3.0 PHY should be tuned again.
	 */
	if (hcd == xhci->main_hcd) {
		if (xhci_exynos->phy_usb2)
			exynos_usbdrd_phy_tune(xhci_exynos->phy_usb2, OTG_STATE_A_HOST);
	} else {
		if (xhci_exynos->phy_usb3)
			exynos_usbdrd_phy_tune(xhci_exynos->phy_usb3, OTG_STATE_A_HOST);
	}
#endif
	return ret;
}

static int xhci_exynos_start(struct usb_hcd *hcd)
{
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	struct xhci_hcd *xhci;
#endif
	int ret;
	xhci_priv_exynos_start(hcd);
	ret = xhci_run(hcd);

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	xhci = hcd_to_xhci(hcd);
	pr_debug("%s: enable_event_ring!\n", __func__);
	xhci_exynos_usb_offload_enable_event_ring(xhci);
#endif
	return ret;
}

static ssize_t
xhci_exynos_ss_compliance_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	u32			reg;
	void __iomem *reg_base;

	reg_base = hcd->regs;
	reg = readl(reg_base + PORTSC_OFFSET);

	return sysfs_emit(buf, "0x%x\n", reg);
}

static ssize_t
xhci_exynos_ss_compliance_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	int		value;
	u32			reg;
	void __iomem *reg_base;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	reg_base = hcd->regs;

	if (value == 1) {
		/* PORTSC PLS is set to 10, LWS to 1 */
		reg = readl(reg_base + PORTSC_OFFSET);
		reg &= ~((0xF << 5) | (1 << 16));
		reg |= (10 << 5) | (1 << 16);
		writel(reg, reg_base + PORTSC_OFFSET);
		pr_info("SS host compliance enabled portsc 0x%x\n", reg);
	} else
		pr_info("Only 1 is allowed for input value\n");

	return n;
}

static DEVICE_ATTR_RW(xhci_exynos_ss_compliance);


static struct attribute *xhci_exynos_attrs[] = {
	&dev_attr_xhci_exynos_ss_compliance.attr,
	NULL
};
ATTRIBUTE_GROUPS(xhci_exynos);

#ifdef CONFIG_OF
static const struct xhci_plat_priv xhci_plat_marvell_armada = {
	.init_quirk = xhci_mvebu_mbus_init_quirk,
};

static const struct xhci_plat_priv xhci_plat_marvell_armada3700 = {
	.init_quirk = xhci_mvebu_a3700_init_quirk,
};

static const struct xhci_plat_priv xhci_plat_renesas_rcar_gen2 = {
	SET_XHCI_PLAT_PRIV_FOR_RCAR(XHCI_RCAR_FIRMWARE_NAME_V1)
};

static const struct xhci_plat_priv xhci_plat_renesas_rcar_gen3 = {
	SET_XHCI_PLAT_PRIV_FOR_RCAR(XHCI_RCAR_FIRMWARE_NAME_V3)
};

static const struct of_device_id usb_xhci_of_match[] = {
	{
		.compatible = "generic-xhci",
	}, {
		.compatible = "xhci-platform",
	},
	{},
};
MODULE_DEVICE_TABLE(of, usb_xhci_of_match);
#endif

static void xhci_pm_runtime_init(struct device *dev)
{
	dev->power.runtime_status = RPM_SUSPENDED;
	dev->power.idle_notification = false;

	dev->power.disable_depth = 1;
	atomic_set(&dev->power.usage_count, 0);

	dev->power.runtime_error = 0;

	atomic_set(&dev->power.child_count, 0);
	pm_suspend_ignore_children(dev, false);
	dev->power.runtime_auto = true;

	dev->power.request_pending = false;
	dev->power.request = RPM_REQ_NONE;
	dev->power.deferred_resume = false;
	dev->power.accounting_timestamp = jiffies;

	dev->power.timer_expires = 0;
	init_waitqueue_head(&dev->power.wait_queue);
}

static struct xhci_plat_priv_overwrite xhci_plat_vendor_overwrite;

int xhci_exynos_register_vendor_ops(struct xhci_vendor_ops *vendor_ops)
{
	if (vendor_ops == NULL)
		return -EINVAL;

	xhci_plat_vendor_overwrite.vendor_ops = vendor_ops;

	return 0;
}
EXPORT_SYMBOL_GPL(xhci_exynos_register_vendor_ops);


static int xhci_vendor_init(struct xhci_hcd *xhci)
{
	struct xhci_vendor_ops *ops = NULL;

	if (xhci_plat_vendor_overwrite.vendor_ops)
		ops = xhci->vendor_ops = xhci_plat_vendor_overwrite.vendor_ops;

	if (ops && ops->vendor_init)
		return ops->vendor_init(xhci);
	return 0;
}

static void xhci_vendor_cleanup(struct xhci_hcd *xhci)
{
	struct xhci_vendor_ops *ops = xhci_vendor_get_ops(xhci);

	if (ops && ops->vendor_cleanup)
		ops->vendor_cleanup(xhci);

	xhci->vendor_ops = NULL;
}

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
static struct xhci_vendor_ops ops = {
	.vendor_init = xhci_exynos_vendor_init,
	.vendor_cleanup = xhci_exynos_vendor_cleanup,
	.is_usb_offload_enabled = xhci_exynos_is_usb_offload_enabled,
	.alloc_dcbaa = xhci_exynos_alloc_dcbaa,
	.free_dcbaa = xhci_exynos_free_dcbaa,
	.alloc_transfer_ring = xhci_exynos_alloc_transfer_ring,
	.free_transfer_ring = xhci_exynos_free_transfer_ring,
	.alloc_container_ctx = xhci_exynos_alloc_container_ctx,
	.free_container_ctx = xhci_exynos_free_container_ctx,
	.sync_dev_ctx = xhci_exynos_sync_dev_ctx,
};
#endif

int xhci_exynos_wake_lock(struct xhci_hcd_exynos *xhci_exynos,
				   int is_main_hcd, int is_lock)
{
	struct usb_hcd	*hcd = xhci_exynos->hcd;
	int idle_ip_index;
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct wakeup_source *main_wakelock, *shared_wakelock;

	main_wakelock = xhci_exynos->main_wakelock;
	shared_wakelock = xhci_exynos->shared_wakelock;

	dev_info(xhci_exynos->dev, "<<< %s - hcd = 0x%8x 0x%8x 0x%8x\n",
					__func__, hcd, main_wakelock, shared_wakelock);

	if (xhci->xhc_state & XHCI_STATE_REMOVING) {
		dev_info(xhci_exynos->dev, "%s - Host removing return!\n",
				__func__);
		return -ESHUTDOWN;
	}

	if (is_lock) {
		if (is_main_hcd) {
			dev_info(xhci_exynos->dev, "%s: Main HCD WAKE LOCK\n", __func__);
			__pm_stay_awake(main_wakelock);
		} else {
			dev_info(xhci_exynos->dev, "%s: Shared HCD WAKE LOCK\n", __func__);
			__pm_stay_awake(shared_wakelock);
		}

		/* Add a routine for disable IDLEIP (IP idle) */
		dev_info(xhci_exynos->dev, "IDLEIP(SICD) disable.\n");
		idle_ip_index = get_idle_ip_index();
		pr_info("%s, usb idle ip = %d\n", __func__, idle_ip_index);
		exynos_update_ip_idle_status(idle_ip_index, 0);
	} else {
		if (is_main_hcd) {
			dev_info(xhci_exynos->dev, "%s: Main HCD WAKE UNLOCK\n", __func__);
			__pm_relax(main_wakelock);
		} else {
			dev_info(xhci_exynos->dev, "%s: Shared HCD WAKE UNLOCK\n", __func__);
			__pm_relax(shared_wakelock);
		}

		if (!main_wakelock->active && !shared_wakelock->active) {
			xhci_info(xhci, "Try to IDLEIP Enable!!!\n");

			/* Add a routine for enable IDLEIP (IP idle) */
			dev_info(xhci_exynos->dev, "IDLEIP(SICD) Enable.\n");
			idle_ip_index = get_idle_ip_index();
			pr_info("%s, usb idle ip = %d\n", __func__, idle_ip_index);
			exynos_update_ip_idle_status(idle_ip_index, 1);
		}
	}

	return 0;
}

static int xhci_exynos_probe(struct platform_device *pdev)
{
	struct device		*parent = pdev->dev.parent;
	const struct hc_driver	*driver;
	struct device		*sysdev, *tmpdev;
	struct xhci_hcd		*xhci;
	struct xhci_hcd_exynos	*xhci_exynos;
	struct xhci_exynos_priv *priv;
	struct resource         *res;
	struct usb_hcd		*hcd;
	struct usb_device	*hdev;
	struct usb_hub		*hub;
	struct usb_port		*port_dev;
	int			ret;
	int			irq;

	struct wakeup_source	*main_wakelock, *shared_wakelock;
	int			value;

	dev_info(&pdev->dev, "XHCI PLAT START\n");

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	xhci_exynos_register_vendor_ops(&ops);
	pr_info("%s register ops done!\n", __func__);
	pr_info("%s fix ep ring free!\n", __func__);
#endif
	main_wakelock = wakeup_source_register(&pdev->dev, dev_name(&pdev->dev));
	if (main_wakelock)
		__pm_stay_awake(main_wakelock);
	else
		pr_info("%s main_wakelock wakeup_source_register fail !\n", __func__);


	/* Initialization shared wakelock for SS HCD */
	shared_wakelock = wakeup_source_register(&pdev->dev, dev_name(&pdev->dev));
	if (shared_wakelock)
		__pm_stay_awake(shared_wakelock);
	else
		pr_info("%s shared_wakelock wakeup_source_register fail !\n", __func__);

	is_rewa_enabled = 0;

	if (usb_disabled())
		return -ENODEV;

	driver = &xhci_exynos_hc_driver;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	/*
	 * sysdev must point to a device that is known to the system firmware
	 * or PCI hardware. We handle these three cases here:
	 * 1. xhci_plat comes from firmware
	 * 2. xhci_plat is child of a device from firmware (dwc3-plat)
	 * 3. xhci_plat is grandchild of a pci device (dwc3-pci)
	 */
	for (sysdev = &pdev->dev; sysdev; sysdev = sysdev->parent) {
		if (is_of_node(sysdev->fwnode) ||
			is_acpi_device_node(sysdev->fwnode))
			break;
#ifdef CONFIG_PCI
		else if (sysdev->bus == &pci_bus_type)
			break;
#endif
	}

	if (!sysdev)
		sysdev = &pdev->dev;

	/* Try to set 64-bit DMA first */
	if (WARN_ON(!sysdev->dma_mask))
		/* Platform did not initialize dma_mask */
		ret = dma_coerce_mask_and_coherent(sysdev,
						   DMA_BIT_MASK(64));
	else
		ret = dma_set_mask_and_coherent(sysdev, DMA_BIT_MASK(64));

	/* If seting 64-bit DMA mask fails, fall back to 32-bit DMA mask */
	if (ret) {
		ret = dma_set_mask_and_coherent(sysdev, DMA_BIT_MASK(32));
		if (ret)
			return ret;
	}

	xhci_pm_runtime_init(&pdev->dev);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);

	hcd = __usb_create_hcd(driver, sysdev, &pdev->dev,
			       dev_name(&pdev->dev), NULL);
	if (!hcd) {
		ret = -ENOMEM;
		goto disable_runtime;
	}
	hcd->skip_phy_initialization = 1;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hcd->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(hcd->regs)) {
		ret = PTR_ERR(hcd->regs);
		goto put_hcd;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	xhci = hcd_to_xhci(hcd);

	g_xhci_exynos = devm_kzalloc(&pdev->dev, sizeof(struct xhci_hcd_exynos), GFP_KERNEL);
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	g_hwinfo = devm_kzalloc(hcd->self.sysdev, sizeof(struct hcd_hw_info), GFP_KERNEL);
#endif

	ret = xhci_vendor_init(xhci);
	if (ret)
		goto put_hcd;

	if (g_xhci_exynos)
		xhci_exynos = g_xhci_exynos;
	else
		goto put_hcd;

	xhci_exynos->dev = &pdev->dev;

	xhci_exynos->hcd = (struct usb_hcd *)platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, xhci_exynos);

	spin_lock_init(&xhci_exynos->xhcioff_lock);

	xhci_exynos->port_off_done = 0;
	xhci_exynos->portsc_control_priority = 0;
	xhci_exynos->usb3_phy_control = true;

	xhci_exynos->usb3_portsc = hcd->regs + PORTSC_OFFSET;
	if (xhci_exynos->port_set_delayed) {
		pr_info("port power set delayed\n");
		xhci_exynos_portsc_power_off(xhci_exynos->usb3_portsc, 0, 2);
		xhci_exynos->port_set_delayed = 0;
	}

	xhci_exynos_register_notify();

	/*
	 * Not all platforms have clks so it is not an error if the
	 * clock do not exist.
	 */
	xhci->reg_clk = devm_clk_get_optional(&pdev->dev, "reg");
	if (IS_ERR(xhci->reg_clk)) {
		ret = PTR_ERR(xhci->reg_clk);
		goto put_hcd;
	}

	ret = clk_prepare_enable(xhci->reg_clk);
	if (ret)
		goto put_hcd;

	xhci->clk = devm_clk_get_optional(&pdev->dev, NULL);
	if (IS_ERR(xhci->clk)) {
		ret = PTR_ERR(xhci->clk);
		goto disable_reg_clk;
	}

	ret = clk_prepare_enable(xhci->clk);
	if (ret)
		goto disable_reg_clk;

	priv = hcd_to_xhci_exynos_priv(hcd);
	priv->xhci_exynos = xhci_exynos;

	device_wakeup_enable(hcd->self.controller);

	xhci->main_hcd = hcd;
	xhci->shared_hcd = __usb_create_hcd(driver, sysdev, &pdev->dev,
			dev_name(&pdev->dev), hcd);
	if (!xhci->shared_hcd) {
		ret = -ENOMEM;
		goto disable_clk;
	}
	xhci->shared_hcd->skip_phy_initialization = 1;

	xhci_exynos->shared_hcd = xhci->shared_hcd;

	/* imod_interval is the interrupt moderation value in nanoseconds. */
	xhci->imod_interval = 40000;

	/* Iterate over all parent nodes for finding quirks */
	for (tmpdev = &pdev->dev; tmpdev; tmpdev = tmpdev->parent) {

		if (device_property_read_bool(tmpdev, "usb2-lpm-disable"))
			xhci->quirks |= XHCI_HW_LPM_DISABLE;

		if (device_property_read_bool(tmpdev, "usb3-lpm-capable"))
			xhci->quirks |= XHCI_LPM_SUPPORT;

		if (device_property_read_bool(tmpdev, "quirk-broken-port-ped"))
			xhci->quirks |= XHCI_BROKEN_PORT_PED;

		device_property_read_u32(tmpdev, "imod-interval-ns",
					 &xhci->imod_interval);
	}

	hcd->usb_phy = devm_usb_get_phy_by_phandle(sysdev, "usb-phy", 0);
	if (IS_ERR(hcd->usb_phy)) {
		ret = PTR_ERR(hcd->usb_phy);
		if (ret == -EPROBE_DEFER)
			goto put_usb3_hcd;
		hcd->usb_phy = NULL;
	} else {
		ret = usb_phy_init(hcd->usb_phy);
		if (ret)
			goto put_usb3_hcd;
	}

	/* Get USB2.0 PHY for main hcd */
	if (parent) {
		xhci_exynos->phy_usb2 = devm_phy_get(parent, "usb2-phy");
		if (IS_ERR_OR_NULL(xhci_exynos->phy_usb2)) {
			xhci_exynos->phy_usb2 = NULL;
			dev_err(&pdev->dev,
				"%s: failed to get phy\n", __func__);
		}
	}

	/* Get USB3.0 PHY to tune the PHY */
	if (parent) {
		xhci_exynos->phy_usb3 =
				devm_phy_get(parent, "usb3-phy");
		if (IS_ERR_OR_NULL(xhci_exynos->phy_usb3)) {
			xhci_exynos->phy_usb3 = NULL;
			dev_err(&pdev->dev,
				"%s: failed to get phy\n", __func__);
		}
	}

	ret = of_property_read_u32(parent->of_node, "xhci_l2_support", &value);
	if (ret == 0 && value == 1)
		xhci->quirks |= XHCI_L2_SUPPORT;
	else {
		dev_err(&pdev->dev,
			"can't get xhci l2 support, error = %d\n", ret);
	}

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	ret = of_property_read_u32(parent->of_node,
				"xhci_use_uram_for_audio", &value);
	if (ret == 0 && value == 1) {
		/*
		 * Check URAM address. At least the following address should
		 * be defined.(Otherwise, URAM feature will be disabled.)
		 */
		if (EXYNOS_URAM_DCBAA_ADDR == 0x0 ||
				EXYNOS_URAM_ABOX_ERST_SEG_ADDR == 0x0 ||
				EXYNOS_URAM_ABOX_EVT_RING_ADDR == 0x0 ||
				EXYNOS_URAM_DEVICE_CTX_ADDR == 0x0 ||
				EXYNOS_URAM_ISOC_OUT_RING_ADDR == 0x0) {
			dev_info(&pdev->dev,
				"Some URAM addresses are not defiend!\n");
			goto skip_uram;
		}

		dev_info(&pdev->dev, "Support URAM for USB audio.\n");
		xhci->quirks |= XHCI_USE_URAM_FOR_EXYNOS_AUDIO;
		/* Initialization Default Value */
		xhci_exynos->exynos_uram_ctx_alloc = false;
		xhci_exynos->exynos_uram_isoc_out_alloc = false;
		xhci_exynos->exynos_uram_isoc_in_alloc = false;
		xhci_exynos->usb_audio_ctx_addr = NULL;
		xhci_exynos->usb_audio_isoc_out_addr = NULL;
		xhci_exynos->usb_audio_isoc_in_addr = NULL;
	} else {
		dev_err(&pdev->dev, "URAM is not used.\n");
	}
skip_uram:

	xhci_exynos->xhci_alloc = &xhci_pre_alloc;

#endif

	if (main_wakelock)
		xhci_exynos->main_wakelock = main_wakelock;
	if (shared_wakelock)
		xhci_exynos->shared_wakelock = shared_wakelock;

	hcd->tpl_support = of_usb_host_tpl_support(sysdev->of_node);
	xhci->shared_hcd->tpl_support = hcd->tpl_support;
	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret)
		goto disable_usb_phy;

	if (HCC_MAX_PSA(xhci->hcc_params) >= 4)
		xhci->shared_hcd->can_do_streams = 1;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret)
		goto dealloc_usb2_hcd;

	/* Set port_dev quirks for reduce port initialize time */
	hdev = xhci->main_hcd->self.root_hub;
	hub = usb_get_intfdata(hdev->actconfig->interface[0]);
	port_dev = hub->ports[0];
	port_dev->quirks |= USB_PORT_QUIRK_FAST_ENUM;

	/* Set port_dev quirks for reduce port initialize time */
	hdev = xhci->shared_hcd->self.root_hub;
	hub = usb_get_intfdata(hdev->actconfig->interface[0]);
	if (hub) {
		port_dev = hub->ports[0];
		port_dev->quirks |= USB_PORT_QUIRK_FAST_ENUM;
	}

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	ret = of_property_read_u32(parent->of_node,
			"usb_audio_offloading", &value);
	if (ret == 0 && value == 1) {
		ret = exynos_usb_audio_init(parent, pdev);
		if (ret) {
			dev_err(&pdev->dev, "USB Audio INIT fail\n");
			goto dealloc_usb2_hcd;
		}
		dev_info(&pdev->dev, "USB Audio offloading is supported\n");
	} else
		dev_err(&pdev->dev, "No usb offloading, err = %d\n", ret);

	xhci_exynos->out_dma = xhci_data.out_data_dma;
	xhci_exynos->out_addr = xhci_data.out_data_addr;
	xhci_exynos->in_dma = xhci_data.in_data_dma;
	xhci_exynos->in_addr = xhci_data.in_data_addr;
#endif

	device_enable_async_suspend(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);

	device_set_wakeup_enable(&xhci->main_hcd->self.root_hub->dev, 1);

#ifdef CONFIG_EXYNOS_USBDRD_PHY30
	device_set_wakeup_enable(&xhci->shared_hcd->self.root_hub->dev, 1);
#else
	device_set_wakeup_enable(&xhci->shared_hcd->self.root_hub->dev, 0);
#endif

	/*
	 * Prevent runtime pm from being on as default, users should enable
	 * runtime pm using power/control in sysfs.
	 */
	pm_runtime_forbid(&pdev->dev);

	return 0;


dealloc_usb2_hcd:
	usb_remove_hcd(hcd);

disable_usb_phy:
	usb_phy_shutdown(hcd->usb_phy);

put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);

disable_clk:
	clk_disable_unprepare(xhci->clk);

disable_reg_clk:
	clk_disable_unprepare(xhci->reg_clk);

put_hcd:
	xhci_exynos_unregister_notify();
	usb_put_hcd(hcd);

disable_runtime:
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static int xhci_exynos_remove(struct platform_device *dev)
{
	struct xhci_hcd_exynos *xhci_exynos = platform_get_drvdata(dev);
	struct usb_hcd	*hcd = xhci_exynos->hcd;
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	struct device	*parent = dev->dev.parent;
#endif
	struct clk *clk = xhci->clk;
	struct clk *reg_clk = xhci->reg_clk;
	struct usb_hcd *shared_hcd = xhci->shared_hcd;
	struct usb_device *rhdev = hcd->self.root_hub;
	struct usb_device *srhdev = shared_hcd->self.root_hub;
	struct usb_device *udev;
	int port, need_wait, timeout;

	dev_info(&dev->dev, "XHCI PLAT REMOVE\n");

#if defined(CONFIG_USB_HOST_SAMSUNG_FEATURE)
	pr_info("%s\n", __func__);
	/* In order to prevent kernel panic */
	if (!pm_runtime_suspended(&xhci->shared_hcd->self.root_hub->dev)) {
		pr_info("%s, shared_hcd pm_runtime_forbid\n", __func__);
		pm_runtime_forbid(&xhci->shared_hcd->self.root_hub->dev);
	}
	if (!pm_runtime_suspended(&xhci->main_hcd->self.root_hub->dev)) {
		pr_info("%s, main_hcd pm_runtime_forbid\n", __func__);
		pm_runtime_forbid(&xhci->main_hcd->self.root_hub->dev);
	}
#endif

	pm_runtime_get_sync(&dev->dev);
	xhci->xhc_state |= XHCI_STATE_REMOVING;

	if (xhci_exynos->port_state == PORT_USB2) {
		usb_power_notify_control(0, 1);
	}
	xhci_exynos_port_power_set(1, 3);

	xhci_exynos->usb3_portsc = NULL;
	xhci_exynos->port_set_delayed = 0;

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	xhci_exynos->xhci_alloc->offset = 0;
	dev_info(&dev->dev, "WAKE UNLOCK\n");
#endif

	__pm_relax(xhci_exynos->main_wakelock);

	__pm_relax(xhci_exynos->shared_wakelock);

	xhci_exynos_unregister_notify();

	if (!rhdev || !srhdev)
		goto remove_hcd;

	/* check all ports */
	for (timeout = 0; timeout < XHCI_HUB_EVENT_TIMEOUT; timeout++) {
		need_wait = false;
		usb_hub_for_each_child(rhdev, port, udev) {
			if (udev && udev->devnum != -1)
				need_wait = true;
		}
		if (need_wait == false) {
			usb_hub_for_each_child(srhdev, port, udev) {
				if (udev && udev->devnum != -1)
					need_wait = true;
			}
		}
		if (need_wait == true) {
			usleep_range(20000, 22000);
			timeout += 20;
			xhci_info(xhci, "Waiting USB hub disconnect\n");
		} else {
			xhci_info(xhci,	"device disconnect all done\n");
			break;
		}
	}

remove_hcd:
#ifdef CONFIG_USB_DEBUG_DETAILED_LOG
	dev_info(&dev->dev, "remove hcd (shared)\n");
#endif
	usb_remove_hcd(shared_hcd);
	xhci->shared_hcd = NULL;
	usb_phy_shutdown(hcd->usb_phy);
	/*
	 * In usb_remove_hcd, phy_exit is called if phy is not NULL.
	 * However, in the case that PHY was turn on or off as runtime PM,
	 * PHY sould not exit at this time. So, to prevent the PHY exit,
	 * PHY pointer have to be NULL.
	 */
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	if (parent && xhci_exynos->phy_usb2)
		xhci_exynos->phy_usb2 = NULL;

	if (parent && xhci_exynos->phy_usb3)
		xhci_exynos->phy_usb3 = NULL;
#endif
#ifdef CONFIG_USB_DEBUG_DETAILED_LOG
	dev_info(&dev->dev, "remove hcd (main)\n");
#endif
	usb_remove_hcd(hcd);

	xhci_vendor_cleanup(xhci);
	wakeup_source_unregister(xhci_exynos->main_wakelock);
	wakeup_source_unregister(xhci_exynos->shared_wakelock);

	devm_iounmap(&dev->dev, hcd->regs);
	usb_put_hcd(shared_hcd);

	clk_disable_unprepare(clk);
	clk_disable_unprepare(reg_clk);
	usb_put_hcd(hcd);

	pm_runtime_disable(&dev->dev);
	pm_runtime_put_noidle(&dev->dev);
	pm_runtime_set_suspended(&dev->dev);

	return 0;
}

/*
static void xhci_exynos_shutdown(struct platform_device *dev)
{
	struct xhci_hcd_exynos *xhci_exynos = platform_get_drvdata(dev);
	struct usb_hcd	*hcd = xhci_exynos->hcd;

	platform_set_drvdata(dev, hcd);
	usb_hcd_platform_shutdown(dev);
	platform_set_drvdata(dev, xhci_exynos);
}
*/
extern u32 otg_is_connect(void);
static int __maybe_unused xhci_exynos_suspend(struct device *dev)
{
	struct xhci_hcd_exynos *xhci_exynos = dev_get_drvdata(dev);
	struct usb_hcd	*hcd = xhci_exynos->hcd;
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	int ret = 0;

	pr_info("[%s]\n", __func__);

	if (otg_is_connect() == 1) { /* If it is OTG_CONNECT_ONLY */
		ret = xhci_priv_suspend_quirk(hcd);
		if (ret)
			return ret;
		/*
		 * xhci_suspend() needs `do_wakeup` to know whether host is allowed
		 * to do wakeup during suspend.
		 */
		pr_info("[%s]: xhci_suspend!\n", __func__);
		ret = xhci_suspend(xhci, device_may_wakeup(dev));
	} else {  /* Enable HS ReWA */
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb2, 1, 0);
#ifdef CONFIG_EXYNOS_USBDRD_PHY30
		/* Enable SS ReWA */
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb3, 1, 0);
#endif
		is_rewa_enabled = 1;
	}
	usb_dr_role_control(0);

	return ret;
}

static int __maybe_unused xhci_exynos_resume(struct device *dev)
{
	struct xhci_hcd_exynos *xhci_exynos = dev_get_drvdata(dev);
	struct usb_hcd	*hcd = xhci_exynos->hcd;
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	int ret;

	pr_info("[%s]\n", __func__);

	ret = xhci_priv_resume_quirk(hcd);
	if (ret)
		return ret;

	if (otg_is_connect() == 1) { /* If it is OTG_CONNECT_ONLY */
		pr_info("[%s]: xhci_resume!\n", __func__);
		ret = xhci_resume(xhci, 0);

		if (ret)
			return ret;
	}

	if (is_rewa_enabled == 1) {
#ifdef CONFIG_EXYNOS_USBDRD_PHY30
		/* Disable SS ReWA */
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb3, 1, 1);
#endif
		/* Disablee HS ReWA */
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb2, 1, 1);
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb2, 0, 0);
		is_rewa_enabled = 0;
	}
	usb_dr_role_control(1);

	return ret;
}

static const struct dev_pm_ops xhci_plat_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(xhci_exynos_suspend, xhci_exynos_resume)
};

static const struct acpi_device_id usb_xhci_acpi_match[] = {
	/* XHCI-compliant USB Controller */
	{ "PNP0D10", },
	{ }
};
MODULE_DEVICE_TABLE(acpi, usb_xhci_acpi_match);

static struct platform_driver usb_xhci_driver = {
	.probe	= xhci_exynos_probe,
	.remove	= xhci_exynos_remove,
	/* Host is supposed to removed in usb_reboot_noti.
	Thats's why we don't need xhci_exynos_shutdown. */
	//.shutdown = xhci_exynos_shutdown,
	.driver	= {
		.name = "xhci-hcd-exynos",
		.pm = &xhci_plat_pm_ops,
		.of_match_table = of_match_ptr(usb_xhci_of_match),
		.acpi_match_table = ACPI_PTR(usb_xhci_acpi_match),
		.dev_groups = xhci_exynos_groups,
	},
};
MODULE_ALIAS("platform:xhci-hcd-exynos");

static int __init xhci_exynos_init(void)
{
	xhci_init_driver(&xhci_exynos_hc_driver, &xhci_exynos_overrides);
	return platform_driver_register(&usb_xhci_driver);
}
module_init(xhci_exynos_init);

static void __exit xhci_exynos_exit(void)
{
	platform_driver_unregister(&usb_xhci_driver);
}
module_exit(xhci_exynos_exit);

MODULE_DESCRIPTION("xHCI Exynos Platform Host Controller Driver");
MODULE_LICENSE("GPL");
