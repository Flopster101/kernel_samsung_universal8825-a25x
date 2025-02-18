/*
 * Samsung TUI HW Handler driver. Display functions.
 *
 * Copyright (c) 2015-2021msung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../../stui_core.h"
#include "../../stui_hal.h"
#include "../../stui_inf.h"

#include <linux/dma-buf.h>
#include <linux/fb.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include <linux/ion.h>
#include "../../../../drivers/gpu/drm/samsung/dpu/exynos_drm_tui.h"
#include <linux/dma-heap.h>

#define ION_EXYNOS_FLAG_PROTECTED       (1 << 16)

extern int decon_tui_protection(bool tui_en);

static struct dma_buf *g_dma_buf;
struct dma_buf_attachment *g_attachment;
struct sg_table	*g_sgt;
struct dma_heap *dma_heap;

extern struct stui_buf_info g_stui_buf_info;
extern struct device *dev_tui;

#define DEFAULT_BPP 32

/* Find suitable framebuffer device driver */
static struct device *get_fb_dev_for_tui(void)
{
	struct device *fb_dev;

	/* get the first framebuffer device */
	fb_dev = dev_tui;

	return fb_dev;
}

static int fb_protection_for_tui(bool tui_en)
{
	struct device *fb_dev;
	int ret = 0;

	pr_debug(TUIHW_LOG_TAG "%s - state %d start\n", __func__, tui_en);

	fb_dev = get_fb_dev_for_tui();
	if (!fb_dev)
		return -1;

	if (tui_en) {
		ret = exynos_atomic_enter_tui();
		if (ret)
			pr_err(TUIHW_LOG_TAG "protect error - %d\n", __func__, ret);
	} else {
		ret = exynos_atomic_exit_tui();
		if (ret)
			pr_err(TUIHW_LOG_TAG "unprotect error - %d\n", ret);
	}
	pr_info(TUIHW_LOG_TAG "%s - state %d end\n", __func__, tui_en);

	return ret;
}

void stui_free_video_space(void)
{
	if (g_attachment && g_sgt) {
		dma_buf_unmap_attachment(g_attachment, g_sgt, DMA_BIDIRECTIONAL);
		g_sgt = NULL;
	}
	if (g_dma_buf && g_attachment) {
		dma_buf_detach(g_dma_buf, g_attachment);
		g_attachment = NULL;
	}
	if (g_dma_buf) {
		dma_buf_put(g_dma_buf);
		g_dma_buf = NULL;
	}
}


unsigned int find_heapmask(void)
{
	int i, cnt = ion_query_heaps_kernel(NULL, 0);
	const char *heapname;
	struct ion_heap_data data[ION_NUM_MAX_HEAPS];

	heapname = "tui_heap";
	ion_query_heaps_kernel((struct ion_heap_data *)data, cnt);
	for (i = 0; i < cnt; i++) {
		if (!strncmp(data[i].name, heapname, MAX_HEAP_NAME))
			break;
	}

	if (i == cnt) {
		pr_err(TUIHW_LOG_TAG "heap %s is not found\n", heapname);
		return 0;
	}

	return 1 << data[i].heap_id;
}

int stui_alloc_video_space(struct tui_hw_buffer *buffer)
{
	dma_addr_t phys_addr = 0;
	size_t framebuf_size;
	size_t workbuf_size;
	struct resolution_info lcd_info;

	exynos_tui_get_resolution(&lcd_info);

	pr_info(TUIHW_LOG_TAG " resolution %d * %d\n", lcd_info.xres, lcd_info.yres);
	framebuf_size = (lcd_info.xres * lcd_info.yres * (DEFAULT_BPP >> 3));
	framebuf_size = STUI_ALIGN_UP(framebuf_size, STUI_ALIGN_4kB_SZ);
	workbuf_size = (lcd_info.xres * lcd_info.yres * (2 * (DEFAULT_BPP >> 3) + 1));
	workbuf_size = STUI_ALIGN_UP(workbuf_size, STUI_ALIGN_4kB_SZ);

	dma_heap = dma_heap_find("tui-secure");
	if (!dma_heap) {
		pr_err(TUIHW_LOG_TAG "%s - fail to get dma_heap for tui\n", __func__);
		goto err_alloc;
	}

	g_dma_buf = dma_heap_buffer_alloc(dma_heap, framebuf_size + workbuf_size + STUI_ALIGN_4kB_SZ, 0, 0);
	if (IS_ERR(g_dma_buf)) {
		pr_err(TUIHW_LOG_TAG "fail to allocate dma buffer\n");
		goto err_alloc;
	}

	g_attachment = dma_buf_attach(g_dma_buf, dev_tui);
	if (IS_ERR_OR_NULL(g_attachment)) {
		pr_err(TUIHW_LOG_TAG " fail to dma buf attachment\n");
		goto err_attach;
	}

	g_sgt = dma_buf_map_attachment(g_attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(g_sgt)) {
		pr_err(TUIHW_LOG_TAG " fail to map attachment\n");
		goto err_attachment;
	}

	phys_addr = sg_phys(g_sgt->sgl);
	phys_addr = STUI_ALIGN_UP(phys_addr, STUI_ALIGN_4kB_SZ);

	buffer->width = lcd_info.xres;
	buffer->height = lcd_info.yres;
	buffer->fb_physical = (uint64_t)phys_addr;
	buffer->wb_physical = (uint64_t)((workbuf_size) ? (phys_addr + framebuf_size) : 0);
	buffer->fb_size = framebuf_size;
	buffer->wb_size = workbuf_size;

	return 0;

err_attachment:
	dma_buf_detach(g_dma_buf, g_attachment);
err_attach:
	dma_buf_put(g_dma_buf);
err_alloc:
	return -ENOMEM;
}

int stui_get_resolution(struct tui_hw_buffer *buffer)
{
	struct resolution_info lcd_info;

	exynos_tui_get_resolution(&lcd_info);

	buffer->width = lcd_info.xres;
	buffer->height = lcd_info.yres;
	return 0;
}

int stui_prepare_tui(void)
{
	pr_info(TUIHW_LOG_TAG " %s - start\n", __func__);
	return fb_protection_for_tui(true);
}

void stui_finish_tui(void)
{
	pr_info(TUIHW_LOG_TAG " %s - start\n", __func__);
	if (fb_protection_for_tui(false))
		pr_err(TUIHW_LOG_TAG " failed to unprotect tui\n");
}

int stui_get_lcd_info(uint64_t *lcd_buf, int size)
{
	int ret;
	unsigned int i;

	pr_info(TUIHW_LOG_TAG " %s - start\n", __func__);
	ret = exynos_tui_get_panel_info(lcd_buf, size);
	if (!ret) {
		for (i = 0; i < size; i++)
			pr_info("lcd info[%u] = %lu\n", i, lcd_buf[i]);
	}

	return ret;
}
