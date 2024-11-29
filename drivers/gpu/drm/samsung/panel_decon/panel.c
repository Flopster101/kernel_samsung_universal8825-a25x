// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include <linux/lcd.h>
#include <linux/spi/spi.h>
#include <linux/sec_detect.h>
#include "decon_panel_kunit.h"
#include "decon_panel_drv.h"
#include "decon_panel.h"
#include "decon_panel_debug.h"
#include "decon_panel_bl.h"
#include "decon_panel_dimming.h"
#include "decon_maptbl.h"
#if defined(CONFIG_PANEL_FREQ_HOP)
#include "decon_panel_freq_hop.h"
#endif
#ifdef CONFIG_EXYNOS_DECON_LCD_SPI
#include "spi.h"
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
#include "decon_dimming.h"
#endif

const char *cmd_type_name[MAX_CMD_TYPE] = {
	[CMD_TYPE_NONE] = "NONE",
	[CMD_TYPE_DELAY] = "DELAY",
	[CMD_TYPE_DELAY_NO_SLEEP] = "DELAY_NO_SLEEP",
	[CMD_TYPE_FRAME_DELAY] = "FRAME_DELAY",
	[CMD_TYPE_VSYNC_DELAY] = "VSYNC_DELAY",
	[CMD_TYPE_TIMER_DELAY] = "TIMER_DELAY",
	[CMD_TYPE_TIMER_DELAY_BEGIN] = "TIMER_DELAY_BEGIN",
	[CMD_TYPE_VARIABLE_DELAY] = "VAR_DELAY",
	[CMD_TYPE_VARIABLE_DELAY_MARK] = "VAR_DELAY_MARK",
	[CMD_TYPE_PINCTL] = "PINCTRL",
	[CMD_PKT_TYPE_NONE] = "PKT_NONE",
	[SPI_PKT_TYPE_WR] = "SPI_WR",
	[DSI_PKT_TYPE_WR] = "DSI_WR",
	[DSI_PKT_TYPE_WR_NO_WAKE] = "DSI_WR_NO_WAKE",
	[DSI_PKT_TYPE_COMP] = "DSI_DSC_WR",
	[DSI_PKT_TYPE_WR_SR] = "DSI_WR_SR",
	[DSI_PKT_TYPE_SR_FAST] = "DSI_SR_FAST",
	[DSI_PKT_TYPE_WR_MEM] = "DSI_WR_MEM",
	[DSI_PKT_TYPE_PPS] = "DSI_WR_PPS",
	[SPI_PKT_TYPE_RD] = "SPI_RD",
	[DSI_PKT_TYPE_RD] = "DSI_RD",
#ifdef CONFIG_SUPPORT_DDI_FLASH
	[DSI_PKT_TYPE_RD_POC] = "DSI_RD_POC",
#endif
	[CMD_TYPE_RES] = "RES",
	[CMD_TYPE_SEQ] = "SEQ",
	[CMD_TYPE_KEY] = "KEY",
	[CMD_TYPE_MAP] = "MAPTBL",
	[CMD_TYPE_DMP] = "DUMP",
	[CMD_TYPE_COND_IF] = "COND_IF",
	[CMD_TYPE_COND_EL] = "COND_EL",
	[CMD_TYPE_COND_FI] = "COND_FI",
	[CMD_TYPE_PCTRL] = "PWRCTRL",
	[CMD_TYPE_PROP] = "PROP",
};

const char *pnobj_type_to_string(u32 type)
{
	return (type < MAX_CMD_TYPE) ?
		cmd_type_name[type] : cmd_type_name[CMD_TYPE_NONE];
}

#define MAX_PRINT_DATA_LEN 1024
void print_data(char *data, int size)
{
	char buf[256];
	int i, len = 0, sz_buf = ARRAY_SIZE(buf);
	bool newline = true;

	if (data == NULL || size <= 0) {
		panel_warn("invalid parameter (size %d)\n", size);
		return;
	}

	if (decon_panel_log_level < 7)
		return;

	if (size > MAX_PRINT_DATA_LEN) {
		panel_info("size %d -> %d\n", size, MAX_PRINT_DATA_LEN);
		size = MAX_PRINT_DATA_LEN;
	}

	for (i = 0; i < size; i++) {
		if (newline)
			len += snprintf(buf + len, sz_buf - len,
					"[%02Xh]  ", i);
		len += snprintf(buf + len, sz_buf - len,
				"%02X", data[i] & 0xFF);
		if (!((i + 1) % 32) || (i + 1 == size)) {
			len += snprintf(buf + len, sz_buf - len, "\n");
			newline = true;
		} else {
			len += snprintf(buf + len, sz_buf - len, " ");
			newline = false;
		}

		if (newline) {
			panel_info("%s", buf);
			len = 0;
		}
	}
}
EXPORT_SYMBOL(print_data);

__visible_for_testing int nr_panel;
__visible_for_testing struct common_panel_info *panel_list[MAX_PANEL];
int decon_register_common_panel(struct common_panel_info *info)
{
	int i;

	if (unlikely(!info)) {
		panel_err("invalid panel_info\n");
		return -EINVAL;
	}

	if (unlikely(nr_panel >= MAX_PANEL)) {
		panel_warn("exceed max seqtbl\n");
		return -EINVAL;
	}

	for (i = 0; i < nr_panel; i++) {
		if (!strcmp(panel_list[i]->name, info->name) &&
				panel_list[i]->id == info->id &&
				panel_list[i]->rev == info->rev) {
			panel_warn("already exist panel (%s id-0x%06X rev-%d)\n", info->name, info->id, info->rev);
			return -EINVAL;
		}
	}

	panel_find_max_brightness_from_cpi(info);
	panel_list[nr_panel++] = info;
	panel_info("name:%s id:0x%06X rev:%d registered\n",
			info->name, info->id, info->rev);

	return 0;
}
EXPORT_SYMBOL(decon_register_common_panel);

int decon_deregister_common_panel(struct common_panel_info *info)
{
	/* TODO: implement deregister function using list */

	return 0;
}
EXPORT_SYMBOL(decon_deregister_common_panel);

static struct common_panel_info *find_common_panel_with_name(const char *name)
{
	int i;

	if (!name) {
		panel_err("invalid name\n");
		return NULL;
	}

	for (i = 0; i < nr_panel; i++) {
		if (!panel_list[i]->name)
			continue;
		if (!strncmp(panel_list[i]->name, name, 128)) {
			panel_info("found panel:%s id:0x%06X rev:%d\n", panel_list[i]->name,
					panel_list[i]->id, panel_list[i]->rev);
			return panel_list[i];
		}
	}

	return NULL;
}

void print_panel_lut(struct panel_dt_lut *lut_info)
{
	struct panel_id_mask *id_mask = NULL;;
	if (IS_ERR_OR_NULL(lut_info)) {
		panel_err("Null lut_info in panel_lut_list\n");
		return ;
	}
	list_for_each_entry(id_mask, &lut_info->id_mask_list, head) {
		if (IS_ERR_OR_NULL(id_mask)) {
			panel_err("Null id_mask in id_mask_list\n");
			return ;
		}
		panel_dbg("panel_lut id:0x%08X mask:0x%08X\n", id_mask->id, id_mask->mask);
	}
}

struct panel_dt_lut *find_panel_lut(struct panel_device *panel, u32 id)
{
	struct panel_dt_lut *lut_info = NULL, *default_lut = NULL;
	struct panel_id_mask *id_mask = NULL;

	if (!panel)
		return ERR_PTR(-EINVAL);

	list_for_each_entry(lut_info, &panel->panel_lut_list, head) {
		if (IS_ERR_OR_NULL(lut_info)) {
			panel_warn("Null lut_info in %s\n", panel->of_node_name);
			continue;
		}
		list_for_each_entry(id_mask, &lut_info->id_mask_list, head) {
			if (IS_ERR_OR_NULL(id_mask)) {
				panel_warn("Null id_mask in lut name %s %s\n", panel->of_node_name, lut_info->name);
				continue;
			}
			if (id_mask->id == 0 && id_mask->mask == 0) {
				panel_info("default %s (id:0x%08X lut_id:0x%08X lut_mask:0x%08X)\n",
					lut_info->name, id, id_mask->id, id_mask->mask);
				if (default_lut == NULL)
					default_lut = lut_info;
				continue;
			}
			if ((id & id_mask->mask) == id_mask->id) {
				panel_info("found %s (id:0x%08X lut_id:0x%08X lut_mask:0x%08X)\n",
					lut_info->name, id, id_mask->id, id_mask->mask);
				return lut_info;
			}
			panel_dbg("panel name %s (id:0x%08X lut_id:0x%08X lut_mask:0x%08X) - not same\n",
				lut_info->name, id, id_mask->id, id_mask->mask);
		}
	}
	if (default_lut) {
		panel_info("use default lut %s\n", default_lut->name);
		return default_lut;
	}
	panel_err("panel not found!! (id 0x%08X)\n", id);
	return ERR_PTR(-ENODEV);
}
struct common_panel_info *find_panel(struct panel_device *panel, u32 id)
{
	struct panel_dt_lut *lut_info;

	lut_info = find_panel_lut(panel, id);
	if (IS_ERR_OR_NULL(lut_info)) {
		panel_err("failed to find panel lookup table\n");
		return NULL;
	}

	if (!lut_info->name) {
		panel_err("invalid name\n");
		return NULL;
	}

	return find_common_panel_with_name(lut_info->name);
}

struct device_node * __mockable
find_panel_ddi_node(struct panel_device *panel, u32 id)
{
	struct panel_dt_lut *lut_info;

	lut_info = find_panel_lut(panel, id);
	if (IS_ERR_OR_NULL(lut_info)) {
		panel_err("failed to find panel lookup table\n");
		return NULL;
	}

	if (!lut_info->ap_vendor_setting_node) {
		panel_err("invalid ap_vendor_setting_node\n");
		return NULL;
	}

	return lut_info->ap_vendor_setting_node;
}

struct device_node * __mockable
find_panel_modes_node(struct panel_device *panel, u32 id)
{
	struct panel_dt_lut *lut_info;

	lut_info = find_panel_lut(panel, id);
	if (IS_ERR_OR_NULL(lut_info)) {
		panel_err("failed to find panel lookup table\n");
		return NULL;
	}

	if (!lut_info->panel_modes_node) {
		panel_err("invalid panel_modes_node\n");
		return NULL;
	}

	return lut_info->panel_modes_node;
}

struct device_node * __mockable
find_panel_power_ctrl_node(struct panel_device *panel, u32 id)
{
	struct panel_dt_lut *lut_info;

	lut_info = find_panel_lut(panel, id);
	if (IS_ERR_OR_NULL(lut_info)) {
		panel_err("failed to find panel lookup table\n");
		return NULL;
	}

	if (!lut_info->power_ctrl_node) {
		panel_err("invalid power_ctrl_node\n");
		return NULL;
	}

	return lut_info->power_ctrl_node;
}

const char *get_panel_lut_dqe_suffix(struct panel_device *panel, u32 id)
{
	struct panel_dt_lut *lut_info;

	lut_info = find_panel_lut(panel, id);
	if (IS_ERR_OR_NULL(lut_info)) {
		panel_err("failed to find panel lookup table\n");
		return NULL;
	}

	return lut_info->dqe_suffix;
}

#ifdef CONFIG_PANEL_FREQ_HOP
struct device_node *get_panel_lut_freq_hop_node(struct panel_device *panel, u32 id)
{
	struct panel_dt_lut *lut_info;

	lut_info = find_panel_lut(panel, id);
	if (IS_ERR_OR_NULL(lut_info)) {
		panel_err("failed to find panel lookup table\n");
		return NULL;
	}

	return lut_info->freq_hop_node;
}
#endif

struct seqinfo *find_panel_seqtbl(struct panel_info *panel_data, char *name)
{
	int i;

	if (unlikely(!panel_data->seqtbl)) {
		panel_err("seqtbl not exist\n");
		return NULL;
	}

	for (i = 0; i < panel_data->nr_seqtbl; i++) {
		if (unlikely(!panel_data->seqtbl[i].name))
			continue;
		if (!strcmp(panel_data->seqtbl[i].name, name)) {
			panel_dbg("found %s panel seqtbl\n", name);
			return &panel_data->seqtbl[i];
		}
	}
	return NULL;
}

bool check_seqtbl_exist(struct panel_info *panel_data, u32 index)
{
	if (unlikely(!panel_data->seqtbl)) {
		panel_err("seqtbl not exist\n");
		return false;
	}

	if (unlikely(index >= MAX_PANEL_SEQ)) {
		panel_err("invalid parameter (index %d)\n", index);
		return false;
	}


	if (panel_data->seqtbl[index].cmdtbl != NULL)
		return true;

	return false;
}

struct seqinfo *find_index_seqtbl(struct panel_info *panel_data, u32 index)
{
	struct seqinfo *tbl;

	if (unlikely(!panel_data->seqtbl)) {
		panel_err("seqtbl not exist\n");
		return NULL;
	}

	if (unlikely(index >= MAX_PANEL_SEQ)) {
		panel_err("invalid parameter (index %d)\n", index);
		return NULL;
	}

	tbl = &panel_data->seqtbl[index];
	if (tbl != NULL)
		panel_dbg("found %s panel seqtbl\n", tbl->name);

	return tbl;
}


struct pktinfo *alloc_static_packet(char *name, u32 type, u8 *data, u32 dlen)
{
	struct pktinfo *info = kzalloc(sizeof(struct pktinfo), GFP_KERNEL);

	info->name = name;
	info->type = type;
	info->data = data;
	info->dlen = dlen;

	return info;
}

struct pktinfo *find_packet(struct seqinfo *seqtbl, char *name)
{
	int i;
	void **cmdtbl;

	if (unlikely(!seqtbl || !name)) {
		panel_err("invalid parameter (seqtbl %p, name %p)\n", seqtbl, name);
		return NULL;
	}

	cmdtbl = seqtbl->cmdtbl;
	if (unlikely(!cmdtbl)) {
		panel_err("invalid command table\n");
		return NULL;
	}

	for (i = 0; i < seqtbl->size; i++) {
		if (cmdtbl[i] == NULL) {
			panel_dbg("end of cmdtbl %d\n", i);
			break;
		}
		if (!strcmp(((struct cmdinfo *)cmdtbl[i])->name, name))
			return cmdtbl[i];
	}

	return NULL;
}


struct pktinfo *find_packet_suffix(struct seqinfo *seqtbl, char *name)
{
	int i, j, len;

	void **cmdtbl;

	if (unlikely(!seqtbl || !name)) {
		panel_err("invalid parameter (seqtbl %p, name %p)\n", seqtbl, name);
		return NULL;
	}

	cmdtbl = seqtbl->cmdtbl;
	if (unlikely(!cmdtbl)) {
		panel_err("invalid command table\n");
		return NULL;
	}

	for (i = 0; i < seqtbl->size; i++) {
		if (cmdtbl[i] == NULL) {
			panel_dbg("end of cmdtbl %d\n", i);
			break;
		}
		len = strlen(((struct cmdinfo *)cmdtbl[i])->name);
		for (j = 0; j < len; j++) {
			if (!strcmp(&((struct cmdinfo *)cmdtbl[i])->name[j], name))
				return cmdtbl[i];
		}
	}

	return NULL;
}

struct panel_dimming_info *
find_panel_dimming(struct panel_info *panel_data, char *name)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(panel_data->panel_dim_info); i++) {
		if (!strcmp(panel_data->panel_dim_info[i]->name, name)) {
			panel_dbg("found %s panel dimming\n", name);
			return panel_data->panel_dim_info[i];
		}
	}
	return NULL;
}

struct maptbl *decon_find_panel_maptbl_by_index(struct panel_info *panel, int index)
{
	if (unlikely(!panel || !panel->maptbl)) {
		panel_err("maptbl not exist\n");
		return NULL;
	}

	if (index < 0 || index >= panel->nr_maptbl) {
		panel_err("out of range (index %d)\n", index);
		return NULL;
	}

	return &panel->maptbl[index];
}
EXPORT_SYMBOL(decon_find_panel_maptbl_by_index);

struct maptbl *decon_find_panel_maptbl_by_name(struct panel_info *panel_data, char *name)
{
	int i = 0;

	if (unlikely(!panel_data || !panel_data->maptbl)) {
		panel_err("maptbl not exist\n");
		return NULL;
	}

	for (i = 0; i < panel_data->nr_maptbl; i++)
		if (strstr(panel_data->maptbl[i].name, name))
			return &panel_data->maptbl[i];
	return NULL;
}
EXPORT_SYMBOL(decon_find_panel_maptbl_by_name);

struct maptbl *maptbl_find(struct panel_info *panel, char *name)
{
	int i;

	if (unlikely(!panel || !name)) {
		panel_err("invalid parameter\n");
		return NULL;
	}

	if (unlikely(!panel->maptbl)) {
		panel_err("maptbl not exist\n");
		return NULL;
	}

	for (i = 0; i < panel->nr_maptbl; i++)
		if (!strcmp(name, panel->maptbl[i].name))
			return &panel->maptbl[i];
	return NULL;
}

int pktui_decon_maptbl_getidx(struct pkt_update_info *pktui)
{
	if (pktui && pktui->getidx)
		return pktui->getidx(pktui);
	return -EINVAL;
}

void panel_update_packet_data(struct panel_device *panel, struct pktinfo *info)
{
	struct pkt_update_info *pktui;
	struct maptbl *maptbl;
	int i, idx, offset, ret;

	if (!info || !info->data || !info->pktui || !info->nr_pktui) {
		panel_warn("invalid pkt update info\n");
		return;
	}

	pktui = info->pktui;
	/*
	 * TODO : move out pktui->pdata initial code
	 * panel_pkt_update_info_init();
	 */
	for (i = 0; i < info->nr_pktui; i++) {
		pktui[i].pdata = panel;
		offset = pktui[i].offset;
		if (pktui[i].nr_maptbl > 1 && pktui[i].getidx) {
			idx = pktui_decon_maptbl_getidx(&pktui[i]);
			if (unlikely(idx < 0)) {
				panel_err("failed to pktui_decon_maptbl_getidx %d\n", idx);
				return;
			}
			maptbl = &pktui[i].maptbl[idx];
		} else {
			maptbl = pktui[i].maptbl;
		}
		if (unlikely(!maptbl)) {
			panel_err("invalid info (offset %d, maptbl %p)\n",
					pktui[i].offset, pktui[i].maptbl);
			return;
		}

		if (!decon_maptbl_is_initialized(maptbl)) {
			panel_info("init maptbl(%s) +\n", maptbl->name);
			decon_maptbl_init(maptbl);
			panel_info("init maptbl(%s) -\n", maptbl->name);
		}

		if (unlikely(offset + maptbl_get_sizeof_copy(maptbl) > info->dlen)) {
			panel_err("out of range (offset %d, sz_copy %d, dlen %d)\n",
					offset, maptbl_get_sizeof_copy(maptbl), info->dlen);
			return;
		}

		ret = decon_maptbl_copy(maptbl, &info->data[offset]);
		if (ret < 0) {
			panel_err("failed to copy maptbl (offset %d, sz_copy %d, dlen %d)\n",
					offset, maptbl_get_sizeof_copy(maptbl), info->dlen);
			return;
		}

		panel_dbg("copy %d bytes from %s to %s[%d]\n",
				maptbl_get_sizeof_copy(maptbl), maptbl->name, info->name, offset);
	}
}

static int panel_do_delay(struct panel_device *panel, struct delayinfo *info, ktime_t s_time)
{
	ktime_t e_time = ktime_get();
	int remained_usec = 0;

	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %p, info %p)\n", panel, info);
		return -EINVAL;
	}

	if (ktime_after(e_time, ktime_add_us(s_time, info->usec))) {
		panel_dbg("skip delay (elapsed %lld usec >= requested %d usec)\n",
				ktime_to_us(ktime_sub(e_time, s_time)), info->usec);
		return 0;
	}

	if (info->usec >= (u32)ktime_to_us(ktime_sub(e_time, s_time)))
		remained_usec = info->usec - (u32)ktime_to_us(ktime_sub(e_time, s_time));

	if (remained_usec > 0)
		usleep_range(remained_usec, remained_usec + 1);

	return 0;
}

static int panel_do_delay_no_sleep(struct panel_device *panel, struct delayinfo *info, ktime_t s_time)
{
	ktime_t e_time = ktime_get();
	int remained_usec = 0;

	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %p, info %p)\n", panel, info);
		return -EINVAL;
	}

	if (ktime_after(e_time, ktime_add_us(s_time, info->usec))) {
		panel_dbg("skip delay (elapsed %lld usec >= requested %d usec)\n",
				ktime_to_us(ktime_sub(e_time, s_time)), info->usec);
		return 0;
	}

	if (info->usec >= (u32)ktime_to_us(ktime_sub(e_time, s_time)))
		remained_usec = info->usec - (u32)ktime_to_us(ktime_sub(e_time, s_time));

	if (remained_usec > 0)
		udelay(remained_usec);

	return 0;
}

static int panel_do_frame_delay(struct panel_device *panel, struct delayinfo *info, ktime_t s_time)
{
	ktime_t e_time = ktime_get();
	int remained_usec = 0;
	int usec;

	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %p, info %p)\n", panel, info);
		return -EINVAL;
	}

	usec = 1000000 / panel->panel_data.props.vrr_origin_fps;
	if (info->nframe > 0)
		usec += (1000000 / panel->panel_data.props.vrr_fps) * (info->nframe - 1);
	if (ktime_after(e_time, ktime_add_us(s_time, usec))) {
		panel_dbg("skip delay (elapsed %lld usec >= requested %d usec)\n",
				ktime_to_us(ktime_sub(e_time, s_time)), usec);
		return 0;
	}

	if (usec >= (u32)ktime_to_us(ktime_sub(e_time, s_time)))
		remained_usec = usec - (u32)ktime_to_us(ktime_sub(e_time, s_time));

	if (remained_usec > 0)
		usleep_range(remained_usec, remained_usec + 1);

	return 0;
}

__visible_for_testing int __mockable
panel_do_vsync_delay(struct panel_device *panel, struct delayinfo *info)
{
	int i, ret;
	u32 timeout = PANEL_WAIT_VSYNC_TIMEOUT_MSEC;
	ktime_t s_time = ktime_get();
	int usec;

	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %p, info %p)\n", panel, info);
		return -EINVAL;
	}

	for (i = 0; i < info->nframe; i++) {
		ret = panel_dsi_wait_for_vsync(panel, timeout);
		if (ret < 0) {
			panel_err("vsync timeout(elapsed:%dms, ret:%d)\n", timeout, ret);
			return 0;
		}
	}
	usec = ktime_to_us(ktime_sub(ktime_get(), s_time));
	panel_dbg("elapsed:%2d.%03d ms in %d FPS\n",
			usec / 1000, usec % 1000,
			panel->panel_data.props.vrr_fps);

#ifdef CONFIG_SUPPORT_MASK_LAYER
	if (panel->panel_bl.props.mask_layer_br_hook == MASK_LAYER_HOOK_ON)
		panel_info("elapsed:%2d.%03d ms (cnt:%d)in %d FPS\n",
			usec / 1000, usec % 1000, info->nframe,
			panel->panel_data.props.vrr_fps);
#endif
	return 0;
}

static int panel_do_timer_begin(struct panel_device *panel,
		struct timer_delay_begin_info *begin_info, ktime_t s_time)
{
	struct delayinfo *info;

	if (unlikely(!panel || !begin_info || !begin_info->delay)) {
		panel_err("invalid parameter (panel %p, begin_info %p, delay %p)\n",
				panel, begin_info, begin_info ? begin_info->delay : NULL);
		return -EINVAL;
	}

	info = begin_info->delay;
	info->s_time = s_time;

	return 0;
}

static int panel_do_timer_delay(struct panel_device *panel, struct delayinfo *info, ktime_t dummy)
{
	ktime_t now = ktime_get();
	ktime_t begin, target;
	s64 remained_usec;

	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %p, info %p)\n", panel, info);
		return -EINVAL;
	}

	begin = info->s_time;
	if (begin == 0)
		return 0;

	target = ktime_add_us(begin, info->usec);
	if (ktime_after(now, target)) {
		panel_dbg("skip delay (elapsed %lld.%03lldms >= requested %d.%03dms)\n",
				ktime_to_us(ktime_sub(now, begin)) / 1000UL,
				ktime_to_us(ktime_sub(now, begin)) % 1000UL,
				info->usec / 1000, info->usec % 1000);
		return 0;
	}

	remained_usec = ktime_to_us(ktime_sub(target, now));
	if (remained_usec > 0) {
		usleep_range((u32)remained_usec, (u32)remained_usec + 1);
		panel_info("remained delay %lld.%03lldms\n",
				remained_usec / 1000UL, remained_usec % 1000UL);
	}

	return 0;
}

static int panel_do_variable_delay(struct panel_device *panel,
	struct variable_delayinfo *info, ktime_t s_time)
{
	int usec, ret;

	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %pK, info %pK)\n", panel, info);
		return -EINVAL;
	}

	if (!info->func_delay) {
		panel_err("function not defined.\n");
		return -EINVAL;
	}

	usec = info->func_delay(panel, s_time, info->mark_time);
	if (usec < 0) {
		panel_err("failed to get delay %d\n", usec);
		return usec;
	}

	if (usec > 0) {
		panel_dbg("flush cmdq, wait %d usecs\n", usec);
		mutex_lock(&panel->cmdq.lock);
		ret = panel_cmdq_flush(panel);
		mutex_unlock(&panel->cmdq.lock);
		if (ret < 0)
			panel_err("failed to panel_cmdq_flush %d\n", ret);

		usleep_range(usec, usec + 1);
	}

	return 0;
}

static int panel_do_variable_delay_mark(struct panel_device *panel, struct cmdinfo *info, ktime_t s_time)
{
	struct variable_delayinfo *v_delay;
	int ret = 0;

	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %pK, info %pK)\n", panel, info);
		return -EINVAL;
	}

	v_delay = container_of(info, struct variable_delayinfo, mark);
	if (v_delay->type != CMD_TYPE_VARIABLE_DELAY) {
		panel_err("invalid variable delayinfo (vdelay %pK, mark %pK)\n", v_delay, info);
		return -EINVAL;
	}

	if (v_delay->func_mark) {
		ret = v_delay->func_mark(panel, s_time, &v_delay->mark_time);
		if (ret < 0) {
			panel_err("func_mark returned %d\n", ret);
			return -EINVAL;
		}
	} else
		v_delay->mark_time = s_time;

	panel_dbg("%s marked %lld.%06lld\n", v_delay->name,
		ktime_to_us(v_delay->mark_time) / 1000000, ktime_to_us(v_delay->mark_time) % 1000000);

	return 0;
}

static int panel_do_pinctl(struct panel_device *panel, struct pininfo *info)
{
	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %p, info %p)\n", panel, info);
		return -EINVAL;
	}

	if (info->pin) {
		/*
		 * TODO : control external pin
		 */
		panel_dbg("set %d pin %s\n", info->pin, info->onoff ? "on" : "off");
	}

	return 0;
}
#ifdef CONFIG_SUPPORT_POC_SPI
int __mockable panel_spi_read_data(struct panel_device *panel, u8 cmd_id, u8 *buf, int size)
{
	struct panel_spi_dev *spi_dev;

	if (!panel)
		return -EINVAL;

	spi_dev = &panel->panel_spi_dev;
	if (!spi_dev)
		return -EINVAL;

	return spi_dev->pdrv_ops->pdrv_read(spi_dev, cmd_id, buf, size);
}
#endif

static int panel_dsi_write_data(struct panel_device *panel,
		u8 cmd_id, const u8 *buf, u32 ofs, int size, bool block)
{
	u32 option = 0;

	if (!panel)
		return -EINVAL;

	if ((panel->panel_data.ddi_props.gpara &
					DDI_SUPPORT_POINT_GPARA))
		option |= DSIM_OPTION_POINT_GPARA;
	if ((panel->panel_data.ddi_props.gpara &
					DDI_SUPPORT_2BYTE_GPARA))
		option |= DSIM_OPTION_2BYTE_GPARA;

	if (block)
		option |= DSIM_OPTION_WAIT_TX_DONE;

	return call_panel_adapter_func(panel, write, cmd_id, buf, ofs, size, option);
}

static int panel_dsi_write_table(struct panel_device *panel,
	const struct cmd_set *cmd, int size, bool block)
{
	u32 option = 0;

	if (!panel)
		return -EINVAL;

	if ((panel->panel_data.ddi_props.gpara &
					DDI_SUPPORT_POINT_GPARA))
		option |= DSIM_OPTION_POINT_GPARA;
	if ((panel->panel_data.ddi_props.gpara &
					DDI_SUPPORT_2BYTE_GPARA))
		option |= DSIM_OPTION_2BYTE_GPARA;

	if (block)
		option |= DSIM_OPTION_WAIT_TX_DONE;

	return call_panel_adapter_func(panel, write_table, cmd, size, option);
}

static int panel_cmdq_is_empty(struct panel_device *panel)
{
	return panel->cmdq.top == -1;
}

static int panel_cmdq_is_full(struct panel_device *panel)
{
	return panel->cmdq.top == MAX_PANEL_CMD_QUEUE - 1;
}

int panel_cmdq_get_size(struct panel_device *panel)
{
	return panel->cmdq.top + 1;
}

__visible_for_testing int panel_set_property(struct panel_device *panel, struct propinfo *info)
{
	int ret = 0;

	if (panel == NULL || info == NULL) {
		panel_err("got null ptr\n");
		return -EINVAL;
	}

	switch (info->prop_type) {
	case PANEL_OBJ_PROP_TYPE_VALUE:
		panel_info("prop_name:%s prop_value:%u\n", info->prop_name, info->value);
		ret = panel_obj_set_property_value(&panel->properties, info->prop_name, info->value);
		break;

	case PANEL_OBJ_PROP_TYPE_STR:
		panel_info("prop_name:%s prop_str:%s\n", info->prop_name, info->str);
		ret = panel_obj_set_property_str(&panel->properties, info->prop_name, info->str);
		break;

	default:
		panel_err("property type is abnormal(%d)", info->type);
	}

	return ret;
}

__visible_for_testing int panel_get_property_value(struct panel_device *panel, char *name)
{
	if (panel == NULL) {
		panel_err("got null ptr\n");
		return -EINVAL;
	}

	if (name == NULL) {
		panel_err("name is null\n");
		return -EINVAL;
	}

	return panel_obj_get_property_value(&panel->properties, name);
}

DEFINE_REDIRECT_MOCKABLE(panel_cmdq_flush, RETURNS(int), PARAMS(struct panel_device *));
int REAL_ID(panel_cmdq_flush)(struct panel_device *panel)
{
	int i, ret;
	bool tx_done_wait = true;

	if (unlikely(!panel))
		return -EINVAL;

	if (unlikely(!panel || !panel->adapter.funcs))
		return -EINVAL;

	if (panel_cmdq_is_empty(panel))
		return 0;

	if (panel->adapter.funcs->write_table != NULL) {
		if (panel_get_property_value(panel, PANEL_OBJ_PROPERTY_WAIT_TX_DONE) == WAIT_TX_DONE_MANUAL_OFF)
			tx_done_wait = false;

		ret = panel_dsi_write_table(panel, panel->cmdq.cmd,
				panel_cmdq_get_size(panel), tx_done_wait);
		if (ret < 0)
			panel_err("failed to panel_dsi_write_table %d\n", ret);
	} else if (panel->adapter.funcs->write != NULL) {
		for (i = 0; i < panel_cmdq_get_size(panel); i++) {
			if (panel_get_property_value(panel, PANEL_OBJ_PROPERTY_WAIT_TX_DONE) == WAIT_TX_DONE_MANUAL_OFF)
				tx_done_wait = false;
			else if (panel_get_property_value(panel, PANEL_OBJ_PROPERTY_WAIT_TX_DONE) == WAIT_TX_DONE_MANUAL_ON)
				tx_done_wait = true;
			else if (i == panel_cmdq_get_size(panel) - 1)
				tx_done_wait = true;

			ret = panel_dsi_write_data(panel, panel->cmdq.cmd[i].cmd_id,
					panel->cmdq.cmd[i].buf, panel->cmdq.cmd[i].offset,
					panel->cmdq.cmd[i].size,
					tx_done_wait);
			if (ret != panel->cmdq.cmd[i].size) {
				panel_err("failed to panel_dsi_write_data %d\n", ret);
				break;
			}
		}
	}

	for (i = 0; i < panel_cmdq_get_size(panel); i++) {
		kfree(panel->cmdq.cmd[i].buf);
		panel->cmdq.cmd[i].buf = NULL;
	}
	panel->cmdq.top = -1;
	panel->cmdq.cmd_payload_size = 0;
	panel_dbg("done\n");

	return 0;
}

int panel_cmdq_push(struct panel_device *panel, u8 cmd_id, const u8 *buf, int size)
{
	int index, ret;
	u8 *data_buf;

	if (buf == NULL || size <= 0)
		return -EINVAL;

	if (panel_cmdq_get_size(panel) + (buf[0] == 0xB0 ? 2 : 1) > MAX_PANEL_CMD_QUEUE) {
		ret = panel_cmdq_flush(panel);
		if (ret < 0) {
			panel_err("failed to panel_cmdq_flush %d\n", ret);
			return ret;
		}
	}

	data_buf = kzalloc(sizeof(u8) * size, GFP_KERNEL);
	if (data_buf == NULL)
		return -ENOMEM;

	index = ++panel->cmdq.top;
	panel->cmdq.cmd[index].cmd_id = cmd_id;
	panel->cmdq.cmd[index].offset = 0;
	memcpy(data_buf, buf, size);
	panel->cmdq.cmd[index].buf = data_buf;
	panel->cmdq.cmd[index].size = size;

	panel_dbg("cmdq[%d] id:%02X cmd:%02X size:%d\n",
			index, cmd_id, buf[0], size);

	if (panel_get_property_value(panel, PANEL_OBJ_PROPERTY_WAIT_TX_DONE) == WAIT_TX_DONE_MANUAL_ON) {
		ret = panel_cmdq_flush(panel);
		if (ret < 0) {
			panel_err("failed to panel_cmdq_flush %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int panel_dsi_write_cmd(struct panel_device *panel,
		u8 cmd_id, const u8 *buf, u32 ofs, int size, u32 option)
{
	int ret, gpara_len = 1;
	u8 gpara[4] = { 0xB0, 0x00, };
	bool block = (option & PKT_OPTION_CHECK_TX_DONE) ? true : false;

	mutex_lock(&panel->cmdq.lock);
	if (panel->panel_data.ddi_props.cmd_fifo_size &&
		panel->cmdq.cmd_payload_size + ARRAY_SIZE(gpara) + size >=
		panel->panel_data.ddi_props.cmd_fifo_size) {
		ret = panel_cmdq_flush(panel);
		if (ret < 0) {
			panel_err("failed to panel_cmdq_flush %d\n", ret);
			mutex_unlock(&panel->cmdq.lock);
			return ret;
		}
	}

	if (ofs > 0) {
		/* gpara 16bit offset */
		if ((panel->panel_data.ddi_props.gpara &
					DDI_SUPPORT_2BYTE_GPARA))
			gpara[gpara_len++] = (ofs >> 8) & 0xFF;

		gpara[gpara_len++] = ofs & 0xFF;

		/* pointing gpara */
		if ((panel->panel_data.ddi_props.gpara &
					DDI_SUPPORT_POINT_GPARA))
			gpara[gpara_len++] = buf ? buf[0] : 0x00;

		ret = panel_cmdq_push(panel, MIPI_DSI_WR_GEN_CMD,
				gpara, gpara_len);
		if (ret < 0) {
			panel_err("failed to panel_cmdq_push %d\n", ret);
			mutex_unlock(&panel->cmdq.lock);
			return ret;
		}
	}

	ret = panel_cmdq_push(panel, cmd_id, buf, size);
	if (ret < 0) {
		panel_err("failed to panel_cmdq_push %d\n", ret);
		mutex_unlock(&panel->cmdq.lock);
		return ret;
	}
	panel->cmdq.cmd_payload_size += size;

	if (panel_cmdq_is_full(panel) || block) {
		ret = panel_cmdq_flush(panel);
		if (ret < 0) {
			panel_err("failed to panel_cmdq_flush %d\n", ret);
			mutex_unlock(&panel->cmdq.lock);
			return ret;
		}
	}
	mutex_unlock(&panel->cmdq.lock);

	return size;
}

static int panel_dsi_sr_write_data(struct panel_device *panel,
		u8 cmd_id, const u8 *buf, u32 ofs, int size, u32 option)
{
	return call_panel_adapter_func(panel, sr_write, cmd_id, buf, ofs, size, option);
}

static u32 get_write_fifo_size(u32 adapter_fifo,
		u32 props_fifo, u32 option)
{
	u32 fifo = adapter_fifo;

	if (props_fifo > 0)
		fifo = min(fifo, props_fifo);

	if (option & PKT_OPTION_SR_MAX_512)
		fifo = min(fifo, 512U);
	else if (option & PKT_OPTION_SR_MAX_1024)
		fifo = min(fifo, 1024U);
	else if (option & PKT_OPTION_SR_MAX_2048)
		fifo = min(fifo, 2048U);

	return fifo;
}

static inline u32 get_write_img_align(u32 option)
{
	int align;

	if (option & PKT_OPTION_SR_ALIGN_4)
		align = 4U;
	else if (option & PKT_OPTION_SR_ALIGN_12)
		align = 12U;
	else if (option & PKT_OPTION_SR_ALIGN_16)
		align = 16U;
	else {
		/* protect for already released panel: 16byte align */
		panel_warn("sram packets need to align option, set force to 16\n");
		align = 16U;
	}

	return align;
}

__visible_for_testing int panel_dsi_write_img(struct panel_device *panel,
		u8 cmd_id, const u8 *buf, u32 ofs, int size, u32 option)
{
	u8 c_start = 0, c_next = 0;
	int data_size, ret, len = 0, cmd_size;
	u32 fifo_size, adapter_fifo_size, align;
	int remained, padding;
	bool block = (option & PKT_OPTION_CHECK_TX_DONE) ? true : false;
	u8 *cmdbuf;

	if (!panel || !buf || size < 1)
		return -EINVAL;

	cmdbuf = panel->cmdbuf;
	if (!cmdbuf) {
		panel_err("command buffer is null\n");
		return -EINVAL;
	}

	adapter_fifo_size = get_panel_adapter_fifo_size(panel);
	if (!adapter_fifo_size) {
		panel_err("invalid panel adapter fifo size(%d)\n",
				adapter_fifo_size);
		return -EINVAL;
	}

	remained = size;

	fifo_size = get_write_fifo_size(adapter_fifo_size,
			panel->panel_data.ddi_props.img_fifo_size, option);
	align = get_write_img_align(option);

	if (cmd_id == MIPI_DSI_WR_GRAM_CMD) {
		c_start = MIPI_DCS_WRITE_GRAM_START;
		c_next = MIPI_DCS_WRITE_GRAM_CONTINUE;
	} else if (cmd_id == MIPI_DSI_WR_SRAM_CMD) {
		if (option & PKT_OPTION_SR_REG_6C) {
			c_start = MIPI_DCS_WRITE_SPSRAM;
			c_next = MIPI_DCS_WRITE_SPSRAM;
		} else {
			c_start = MIPI_DCS_WRITE_SIDE_RAM_START;
			c_next = MIPI_DCS_WRITE_SIDE_RAM_CONTINUE;
		}
	} else {
		panel_err("invalid cmd_id %d\n", cmd_id);
		return -EINVAL;
	}

	do {
		mutex_lock(&panel->cmdq.lock);
		cmdbuf[0] = (size == remained) ? c_start : c_next;

		/* set cmd size less then (fifo - 1) */
		cmd_size = rounddown(fifo_size - 1, align);

		/* set data size as small of remained and cmd_size */
		data_size = min(remained, cmd_size);
		memcpy(cmdbuf + 1, buf + len, data_size);

		/* if not aligned, add padding with zero */
		padding = roundup(data_size, align) - data_size;
		if (padding)
			memset(cmdbuf + 1 + data_size, 0, padding);

		ret = panel_cmdq_push(panel, MIPI_DSI_WR_GEN_CMD, cmdbuf, data_size + padding + 1);
		if (ret < 0) {
			panel_err("failed to panel_cmdq_push %d\n", ret);
			mutex_unlock(&panel->cmdq.lock);
			return ret;
		}

		if (panel_cmdq_is_full(panel) || (block && (remained <= data_size))) {
			ret = panel_cmdq_flush(panel);
			if (ret < 0) {
				panel_err("failed to panel_cmdq_flush %d\n", ret);
				mutex_unlock(&panel->cmdq.lock);
				return ret;
			}
		}
		mutex_unlock(&panel->cmdq.lock);
		len += data_size;
		remained = (remained > data_size) ? (remained - data_size) : 0;
		panel_dbg("tx_size %d len %d, remained %d\n", data_size, len, remained);
	} while (remained > 0);

	return len;
}

static int panel_dsi_fast_write_mem(struct panel_device *panel,
		u8 cmd_id, const u8 *buf, u32 ofs, int size, u32 option)
{
	int ret;

	if (unlikely(!panel || !panel->adapter.funcs ||
				!panel->adapter.funcs->sr_write))
		return -EINVAL;

	mutex_lock(&panel->cmdq.lock);
	panel_cmdq_flush(panel);
	ret = panel_dsi_sr_write_data(panel, MIPI_DSI_WR_SRAM_CMD,
				buf, 0, size, option);
	mutex_unlock(&panel->cmdq.lock);

	return size;
}

static int panel_dsi_read_data(struct panel_device *panel,
		u8 addr, u32 ofs, u8 *buf, int size)
{
	u32 option = 0;
	int ret;

	if (!panel)
		return -EINVAL;

	if ((panel->panel_data.ddi_props.gpara &
					DDI_SUPPORT_POINT_GPARA))
		option |= DSIM_OPTION_POINT_GPARA;
	if ((panel->panel_data.ddi_props.gpara &
					DDI_SUPPORT_2BYTE_GPARA))
		option |= DSIM_OPTION_2BYTE_GPARA;

	mutex_lock(&panel->cmdq.lock);
	panel_cmdq_flush(panel);
	ret = call_panel_adapter_func(panel, read, addr, ofs, buf, size, option);
	mutex_unlock(&panel->cmdq.lock);

	return ret;
}

static int panel_dsi_get_state(struct panel_device *panel)
{
	return call_panel_adapter_func(panel, get_state);
}

int panel_dsi_wait_for_vsync(struct panel_device *panel, u32 timeout)
{
	return call_panel_adapter_func(panel, wait_for_vsync, timeout);
}

int panel_dsi_set_bypass(struct panel_device *panel, bool on)
{
	return call_panel_adapter_func(panel, set_bypass, on);
}

int panel_dsi_get_bypass(struct panel_device *panel)
{
	return call_panel_adapter_func(panel, get_bypass);
}

int panel_dsi_set_commit_retry(struct panel_device *panel, bool on)
{
	return call_panel_adapter_func(panel, set_commit_retry, on);
}

int panel_dsi_dump_dpu_register(struct panel_device *panel)
{
	return call_panel_adapter_func(panel, dpu_register_dump);
}

int panel_dsi_print_dpu_event_log(struct panel_device *panel)
{
	return call_panel_adapter_func(panel, dpu_event_log_print);
}

int panel_dsi_set_lpdt(struct panel_device *panel, bool on)
{
	return call_panel_adapter_func(panel, set_lpdt, on);
}

int panel_wake_lock(struct panel_device *panel, unsigned long timeout)
{
	return call_panel_adapter_func(panel, wake_lock, timeout);
}

int panel_wake_unlock(struct panel_device *panel)
{
	return call_panel_adapter_func(panel, wake_unlock);
}

int panel_emergency_off(struct panel_device *panel)
{
	return call_panel_adapter_func(panel, emergency_off);
}

int panel_flush_image(struct panel_device *panel)
{
	return call_panel_adapter_func(panel, flush_image);
}

#if defined(CONFIG_PANEL_FREQ_HOP)
int panel_set_freq_hop(struct panel_device *panel, struct freq_hop_param *param)
{
	if (!param)
		return -EINVAL;

	return call_panel_adapter_func(panel, set_freq_hop, param);
}
#endif

int panel_parse_ap_vendor_node(struct panel_device *panel, struct device_node *node)
{
	if (!node)
		return -EINVAL;

	return call_panel_adapter_func(panel, parse_dt, node);
}

int panel_set_key(struct panel_device *panel, int level, bool on)
{
	int ret = 0;
	static const unsigned char SEQ_TEST_KEY_ON_9F[] = { 0x9F, 0xA5, 0xA5 };
	static const unsigned char SEQ_TEST_KEY_ON_F0[] = { 0xF0, 0x5A, 0x5A };
	static const unsigned char SEQ_TEST_KEY_ON_FC[] = { 0xFC, 0x5A, 0x5A };
	static const unsigned char SEQ_TEST_KEY_OFF_9F[] = { 0x9F, 0x5A, 0x5A };
	static const unsigned char SEQ_TEST_KEY_OFF_F0[] = { 0xF0, 0xA5, 0xA5 };
	static const unsigned char SEQ_TEST_KEY_OFF_FC[] = { 0xFC, 0xA5, 0xA5 };

	if (unlikely(!panel)) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (on) {
		if (level >= 1) {
			ret = panel_dsi_write_cmd(panel, MIPI_DSI_WR_GEN_CMD,
					SEQ_TEST_KEY_ON_9F, 0, ARRAY_SIZE(SEQ_TEST_KEY_ON_9F), false);
			if (ret != ARRAY_SIZE(SEQ_TEST_KEY_ON_9F)) {
				panel_err("fail to write CMD : SEQ_TEST_KEY_ON_9F\n");
				return -EIO;
			}
		}

		if (level >= 2) {
			ret = panel_dsi_write_cmd(panel, MIPI_DSI_WR_GEN_CMD,
					SEQ_TEST_KEY_ON_F0, 0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0), false);
			if (ret != ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) {
				panel_err("fail to write CMD : SEQ_TEST_KEY_ON_F0\n");
				return -EIO;
			}
		}

		if (level >= 3) {
			ret = panel_dsi_write_cmd(panel, MIPI_DSI_WR_GEN_CMD,
					SEQ_TEST_KEY_ON_FC, 0, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC), false);
			if (ret != ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) {
				panel_err("fail to write CMD : SEQ_TEST_KEY_ON_FC\n");
				return -EIO;
			}
		}
	} else {
		if (level >= 3) {
			ret = panel_dsi_write_cmd(panel, MIPI_DSI_WR_GEN_CMD,
					SEQ_TEST_KEY_OFF_FC, 0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC), false);
			if (ret != ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) {
				panel_err("fail to write CMD : SEQ_TEST_KEY_OFF_FC\n");
				return -EIO;
			}
		}

		if (level >= 2) {
			ret = panel_dsi_write_cmd(panel, MIPI_DSI_WR_GEN_CMD,
					SEQ_TEST_KEY_OFF_F0, 0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0), false);
			if (ret != ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) {
				panel_err("fail to write CMD : SEQ_TEST_KEY_OFF_F0\n");
				return -EIO;
			}
		}

		if (level >= 1) {
			ret = panel_dsi_write_cmd(panel, MIPI_DSI_WR_GEN_CMD,
					SEQ_TEST_KEY_OFF_9F, 0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_9F), false);
			if (ret != ARRAY_SIZE(SEQ_TEST_KEY_ON_9F)) {
				panel_err("fail to write CMD : SEQ_TEST_KEY_OFF_9F\n");
				return -EIO;
			}
		}
	}

	return 0;
}

int panel_verify_tx_packet(struct panel_device *panel, u8 *src, u32 ofs, u8 len)
{
	u8 *buf;
	int i, ret = 0;
	unsigned char addr = src[0];
	unsigned char *data = src + 1;

	if (unlikely(!panel)) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!IS_PANEL_ACTIVE(panel))
		return 0;

	buf = kcalloc(len, sizeof(u8), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	panel_rx_nbytes(panel, DSI_PKT_TYPE_RD, buf, addr, ofs, len - 1);
	for (i = 0; i < len - 1; i++) {
		if (buf[i] != data[i]) {
			panel_warn("%02Xh[%2d] - (tx 0x%02X, rx 0x%02X) not match\n",
					addr, i, data[i], buf[i]);
			ret = -EINVAL;
		} else {
			panel_info("%02Xh[%2d] - (tx 0x%02X, rx 0x%02X) match\n",
					addr, i, data[i], buf[i]);
		}
	}
	kfree(buf);
	return ret;
}

static int panel_do_tx_packet(struct panel_device *panel, struct pktinfo *info, bool block)
{
	int ret;
	u32 type;
	u8 addr = 0, cmd_id = MIPI_DSI_WR_UNKNOWN;
	u32 option;

	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %p, info %p)\n", panel, info);
		return -EINVAL;
	}

	if (info->pktui)
		panel_update_packet_data(panel, info);

	type = info->type;
	panel_dbg("send packet %s - start\n", info->name);
	switch (type) {
	case DSI_PKT_TYPE_COMP:
		cmd_id = MIPI_DSI_WR_DSC_CMD;
		break;
	case DSI_PKT_TYPE_PPS:
		cmd_id = MIPI_DSI_WR_PPS_CMD;
		break;
	case DSI_PKT_TYPE_WR:
		cmd_id = MIPI_DSI_WR_GEN_CMD;
		addr = info->data ? info->data[0] : 0;
		break;
	case DSI_PKT_TYPE_WR_NO_WAKE:
		cmd_id = MIPI_DSI_WR_CMD_NO_WAKE;
		addr = info->data ? info->data[0] : 0;
		break;
	case DSI_PKT_TYPE_WR_MEM:
		cmd_id = MIPI_DSI_WR_GRAM_CMD;
		break;
	case DSI_PKT_TYPE_WR_SR:
		cmd_id = MIPI_DSI_WR_SRAM_CMD;
		break;
	case DSI_PKT_TYPE_SR_FAST:
		cmd_id = MIPI_DSI_WR_SR_FAST_CMD;
		break;

	case CMD_PKT_TYPE_NONE:
	case DSI_PKT_TYPE_RD:
	default:
		cmd_id = MIPI_DSI_WR_UNKNOWN;
		break;
	}

	if (cmd_id == MIPI_DSI_WR_UNKNOWN) {
		panel_err("unknown type\n");
		return -EINVAL;
	}

	panel_dbg("%s:start\n", info->name);

	if (info->option & PKT_OPTION_CHECK_TX_DONE ||
		(addr == MIPI_DCS_SOFT_RESET ||
		 addr == MIPI_DCS_SET_DISPLAY_ON ||
		 addr == MIPI_DCS_SET_DISPLAY_OFF ||
		 addr == MIPI_DCS_ENTER_SLEEP_MODE ||
		 addr == MIPI_DCS_EXIT_SLEEP_MODE ||
		 /* poc command */
		 addr == 0xC0 || addr == 0xC1))
		block = true;

	option = info->option;
	if (block)
		option |= PKT_OPTION_CHECK_TX_DONE;

	panel_dbg("%s: ofs 0x%X option %d %d block %s\n", info->name, info->offset,
		info->option, option, block ? "true" : "false");

	if (cmd_id == MIPI_DSI_WR_GRAM_CMD || cmd_id == MIPI_DSI_WR_SRAM_CMD)
		ret = panel_dsi_write_img(panel, cmd_id, info->data, info->offset, info->dlen, option);
	else if (cmd_id == MIPI_DSI_WR_SR_FAST_CMD)
		ret = panel_dsi_fast_write_mem(panel, cmd_id, info->data, info->offset, info->dlen, option);
	else
		ret = panel_dsi_write_cmd(panel, cmd_id, info->data, info->offset, info->dlen, option);

	if (ret != info->dlen) {
		panel_err("failed to send packet %s (ret %d)\n", info->name, ret);
		return -EINVAL;
	}
	panel_dbg("%s:end\n", info->name);
	print_data(info->data, info->dlen);

	return 0;
}

static inline bool panel_do_cond(struct panel_device *panel, struct condinfo *info)
{
	if (panel == NULL || info == NULL) {
		panel_err("got null ptr\n");
		return false;
	}

	if (info->cond == NULL) {
		panel_err("cond_start %s: func is not set\n", info->name);
		return false;
	}
	return info->cond(panel);
}

__visible_for_testing int panel_do_power_ctrl(struct panel_device *panel, struct pctrlinfo *info)
{
	if (panel == NULL || info == NULL) {
		panel_err("got null ptr\n");
		return -EINVAL;
	}

	if (info->type != CMD_TYPE_PCTRL) {
		panel_err("got invalid type\n");
		return -EINVAL;
	}

	if (info->name == NULL) {
		panel_err("got null name\n");
		return -EINVAL;
	}

	if (info->pctrl_name == NULL) {
		panel_err("got null power_ctrl name\n");
		return -EINVAL;
	}

	if (!panel_power_control_exists(panel, info->pctrl_name)) {
		panel_warn("cannot found %s in power_ctrl list\n", info->pctrl_name);
		return -ENODATA;
	}
	panel_dbg("run power_ctrl seq: %s\n", info->pctrl_name);
	return panel_power_control_execute(panel, info->pctrl_name);
}

#ifdef CONFIG_SUPPORT_POC_SPI
int __mockable panel_spi_packet(struct panel_device *panel, struct pktinfo *info)
{
	struct panel_spi_dev *spi_dev;
	int ret = 0;
	u32 type;

	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %p, info %p)\n", panel, info);
		return -EINVAL;
	}
	spi_dev = &panel->panel_spi_dev;

	if (!spi_dev) {
		panel_err("spi_dev not found\n");
		return -EINVAL;
	}

	if (!spi_dev->ops) {
		panel_err("spi_dev.ops not found\n");
		return -EINVAL;
	}

	if (info->pktui)
		panel_update_packet_data(panel, info);

	type = info->type;
	switch (type) {
	case SPI_PKT_TYPE_WR:
		if (!spi_dev->pdrv_ops->pdrv_cmd) {
			ret = -EINVAL;
			break;
		}
		ret = spi_dev->pdrv_ops->pdrv_cmd(spi_dev, info->data, info->dlen, NULL, 0);
		break;
	case SPI_PKT_TYPE_SETPARAM:
		if (!spi_dev->pdrv_ops->pdrv_read_param) {
			ret = -EINVAL;
			break;
		}
		ret = spi_dev->pdrv_ops->pdrv_read_param(spi_dev, info->data, info->dlen);
		break;
	default:
		break;
	}

	if (ret < 0) {
		panel_err("failed to send spi packet %s (ret %d type %d)\n",
				info->name, ret, type);
		return -EINVAL;
	}
	return 0;

}
#endif

#ifdef CONFIG_MCD_PANEL_I2C
static int panel_do_i2c_packet(struct panel_device *panel, struct pktinfo *info)
{
	struct panel_i2c_dev *i2c_dev;
	int ret = 0;
	u32 type;

	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %pK, info %pK)\n",
				panel, info);
		return -EINVAL;
	}

	/*
	 * Use i2c_dev seleted before.
	 * It is locked by panel->op_lock
	 */

	i2c_dev = panel->i2c_dev_selected;

	if (!i2c_dev) {
		panel_err("i2c_dev not found\n");
		return -EINVAL;
	}

	if (!i2c_dev->ops) {
		panel_err("i2c_dev.ops not found\n");
		return -EINVAL;
	}

	type = info->type;
	switch (type) {
	case I2C_PKT_TYPE_WR:
		if (!i2c_dev->ops->tx) {
			ret = -ENOTSUPP;
			break;
		}
		ret = i2c_dev->ops->tx(i2c_dev, info->data, info->dlen);
		break;
	case I2C_PKT_TYPE_RD:
		if (!i2c_dev->ops->rx) {
			ret = -ENOTSUPP;
			break;
		}
		ret = i2c_dev->ops->rx(i2c_dev, info->data, info->dlen);
		break;
	default:
		break;
	}

	if (ret < 0) {
		panel_err("failed to send i2c packet %s (ret %d type %d)\n", info->name, ret, type);
		return -EINVAL;
	}
	return 0;
}
#endif

static int panel_do_setkey(struct panel_device *panel, struct keyinfo *info, bool block)
{
	struct panel_info *panel_data;
	bool send_packet = false;
	int ret = 0;

	if (unlikely(!panel || !info)) {
		panel_err("invalid parameter (panel %p, info %p)\n", panel, info);
		return -EINVAL;
	}

	if (info->level < 0 || info->level >= MAX_CMD_LEVEL) {
		panel_err("invalid level %d\n", info->level);
		return -EINVAL;
	}

	if (info->en != KEY_ENABLE && info->en != KEY_DISABLE) {
		panel_err("invalid key type %d\n", info->en);
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	if (info->en == KEY_ENABLE) {
		if (panel_data->props.key[info->level] == 0)
			send_packet = true;
		panel_data->props.key[info->level] += 1;
	} else if (info->en == KEY_DISABLE) {
		if (panel_data->props.key[info->level] < 1) {
			panel_err("unbalanced key [%d]%s(%d)\n", info->level,
			(panel_data->props.key[info->level] > 0 ? "ENABLED" : "DISABLED"),
			panel_data->props.key[info->level]);
			return -EINVAL;
		}

		if (panel_data->props.key[info->level] == 1)
			send_packet = true;
		panel_data->props.key[info->level] -= 1;
	}

	if (send_packet) {
		ret = panel_do_tx_packet(panel, info->packet, block);
		if (ret < 0)
			panel_err("failed to tx packet(%s)\n", info->packet->name);
	} else {
		panel_warn("ignore send key %d %s packet\n",
				info->level, info->en ? "ENABLED" : "DISABLED");
	}

	panel_dbg("key [1]%s(%d) [2]%s(%d) [3]%s(%d)\n",
			(panel_data->props.key[CMD_LEVEL_1] > 0 ? "ENABLED" : "DISABLED"),
			panel_data->props.key[CMD_LEVEL_1],
			(panel_data->props.key[CMD_LEVEL_2] > 0 ? "ENABLED" : "DISABLED"),
			panel_data->props.key[CMD_LEVEL_2],
			(panel_data->props.key[CMD_LEVEL_3] > 0 ? "ENABLED" : "DISABLED"),
			panel_data->props.key[CMD_LEVEL_3]);

	return 0;
}

int panel_do_init_maptbl(struct panel_device *panel, struct maptbl *maptbl)
{
	int ret;

	if (unlikely(!panel || !maptbl)) {
		panel_err("invalid parameter (panel %p, maptbl %p)\n", panel, maptbl);
		return -EINVAL;
	}

	ret = decon_maptbl_init(maptbl);
	if (ret < 0) {
		panel_err("failed to init maptbl(%s) ret %d\n", maptbl->name, ret);
		return ret;
	}

	return 0;
}

static int _panel_do_seqtbl(struct panel_device *panel,
		struct seqinfo *seqtbl, int depth)
{
	int i, ret = 0;
	u32 type;
	void **cmdtbl;
	bool block = false;
	bool condition = true, cont;
	int cond_skip_count = 0;
	ktime_t s_time = ktime_get();

	if (unlikely(!panel || !seqtbl)) {
		panel_err("invalid parameter (panel %p, seqtbl %p)\n", panel, seqtbl);
		return -EINVAL;
	}

	cmdtbl = seqtbl->cmdtbl;
	if (unlikely(!cmdtbl)) {
		panel_err("invalid command table\n");
		return -EINVAL;
	}

	for (i = 0; i < seqtbl->size; i++) {
		if (cmdtbl[i] == NULL) {
			panel_dbg("end of cmdtbl %d\n", i);
			break;
		}
		type = *((u32 *)cmdtbl[i]);
		if (type >= MAX_CMD_TYPE) {
			panel_warn("invalid cmd type %u\n", type);
			break;
		}

		panel_dbg("SEQ: %s %s %s+ %s\n", seqtbl->name,
				pnobj_type_to_string(type),
				(((struct cmdinfo *)cmdtbl[i])->name ?
				((struct cmdinfo *)cmdtbl[i])->name : "none"),
				(condition ? "" : "skipped"));

		if (!condition) {
			cont = false;
			//take skip condition
			switch (type) {
			case CMD_TYPE_COND_IF:
				panel_dbg("condition skip %d to %d\n", cond_skip_count, cond_skip_count + 1);
				cond_skip_count++;
				cont = true;
				break;
			case CMD_TYPE_COND_EL:
				if (cond_skip_count > 0)
					cont = true;
				/* other: processed by switch-case below */
				break;
			case CMD_TYPE_COND_FI:
				if (cond_skip_count > 0) {
					panel_dbg("condition skip %d to %d\n", cond_skip_count, cond_skip_count - 1);
					cond_skip_count--;
					cont = true;
				}
				/* other: processed by switch-case below */
				break;
			default:
				cont = true;
				break;
			}
			if (cont)
				continue;
		}

		if (type != CMD_TYPE_KEY &&
			type != CMD_TYPE_SEQ &&
			!IS_CMD_TYPE_TX_PKT(type) &&
			!IS_CMD_TYPE_COND(type) &&
			!IS_CMD_TYPE_VARIABLE_DELAY(type)) {
			mutex_lock(&panel->cmdq.lock);
			panel_cmdq_flush(panel);
			mutex_unlock(&panel->cmdq.lock);
		}

		switch (type) {
		case CMD_TYPE_KEY:
		case CMD_TYPE_TX_PKT_START ... CMD_TYPE_TX_PKT_END:
			block = false;
			/* blocking call if next cmdtype is not tx pkt */
			if (i + 1 < seqtbl->size && cmdtbl[i + 1] &&
					(*((u32 *)cmdtbl[i + 1]) != CMD_TYPE_SEQ) &&
					!IS_CMD_TYPE_COND(*((u32 *)cmdtbl[i + 1])) &&
					!IS_CMD_TYPE_TX_PKT(*((u32 *)cmdtbl[i + 1])) &&
					!IS_CMD_TYPE_VARIABLE_DELAY(*((u32 *)cmdtbl[i + 1]))) {
				block = true;
				panel_dbg("blocking call : next cmd is not tx packet\n");
			}

			/* blocking call if end of seq */
			if (depth == 0 && (i + 1 == seqtbl->size)) {
				block = true;
				panel_dbg("blocking call : end of seq\n");
			}

			if (type == CMD_TYPE_KEY)
				ret = panel_do_setkey(panel, (struct keyinfo *)cmdtbl[i], block);
			else
				ret = panel_do_tx_packet(panel, (struct pktinfo *)cmdtbl[i], block);
			break;
		case CMD_TYPE_DELAY:
			ret = panel_do_delay(panel, (struct delayinfo *)cmdtbl[i], s_time);
			break;
		case CMD_TYPE_DELAY_NO_SLEEP:
			ret = panel_do_delay_no_sleep(panel, (struct delayinfo *)cmdtbl[i], s_time);
			break;
		case CMD_TYPE_FRAME_DELAY:
			ret = panel_do_frame_delay(panel, (struct delayinfo *)cmdtbl[i], s_time);
			break;
		case CMD_TYPE_VSYNC_DELAY:
			ret = panel_do_vsync_delay(panel, (struct delayinfo *)cmdtbl[i]);
			break;
		case CMD_TYPE_TIMER_DELAY_BEGIN:
			ret = panel_do_timer_begin(panel, (struct timer_delay_begin_info *)cmdtbl[i], s_time);
			break;
		case CMD_TYPE_TIMER_DELAY:
			ret = panel_do_timer_delay(panel, (struct delayinfo *)cmdtbl[i], s_time);
			break;
		case CMD_TYPE_VARIABLE_DELAY:
			ret = panel_do_variable_delay(panel, (struct variable_delayinfo *)cmdtbl[i], s_time);
			break;
		case CMD_TYPE_VARIABLE_DELAY_MARK:
			ret = panel_do_variable_delay_mark(panel, (struct cmdinfo *)cmdtbl[i], s_time);
			break;
		case CMD_TYPE_PINCTL:
			ret = panel_do_pinctl(panel, (struct pininfo *)cmdtbl[i]);
			break;
		case CMD_TYPE_RES:
			ret = panel_resource_update(panel, (struct resinfo *)cmdtbl[i]);
			break;
		case CMD_TYPE_SEQ:
			ret = _panel_do_seqtbl(panel, (struct seqinfo *)cmdtbl[i], depth + 1);
			break;
		case CMD_TYPE_MAP:
			ret = panel_do_init_maptbl(panel, (struct maptbl *)cmdtbl[i]);
			break;
		case CMD_TYPE_DMP:
			ret = panel_dumpinfo_update(panel, (struct dumpinfo *)cmdtbl[i]);
			break;
#ifdef CONFIG_SUPPORT_POC_SPI
		case SPI_PKT_TYPE_WR:
		case SPI_PKT_TYPE_SETPARAM:
			ret = panel_spi_packet(panel, (struct pktinfo *)cmdtbl[i]);
			break;
#endif
#ifdef CONFIG_MCD_PANEL_I2C
		case I2C_PKT_TYPE_WR:
		case I2C_PKT_TYPE_RD:
			if (sec_needs_blic)
				ret = panel_do_i2c_packet(panel, (struct pktinfo *)cmdtbl[i]);
			else
				panel_warn("unknown pakcet type %d\n", type);
			break;
#endif

		case CMD_TYPE_COND_IF:
			/* skip cmd until next COND_END */
			condition = panel_do_cond(panel, (struct condinfo *)cmdtbl[i]);
			panel_dbg("condition set to: %s\n", condition ? "true" : "false");
			break;
		case CMD_TYPE_COND_EL:
			condition = !condition;
			panel_dbg("condition else, set to %s\n", condition ? "true" : "false");
			break;
		case CMD_TYPE_COND_FI:
			condition = true;
			panel_dbg("condition clear\n");
			break;
		case CMD_TYPE_PCTRL:
			ret = panel_do_power_ctrl(panel, (struct pctrlinfo *)cmdtbl[i]);
			break;
		case CMD_TYPE_PROP:
			mutex_lock(&panel->cmdq.lock);
			panel_cmdq_flush(panel);
			mutex_unlock(&panel->cmdq.lock);
			ret = panel_set_property(panel, (struct propinfo *)cmdtbl[i]);
			break;
		case CMD_TYPE_NONE:
		default:
			panel_warn("unknown pakcet type %d\n", type);
			break;
		}

		if (ret < 0) {
			panel_err("failed to do(seq:%s type:%s cmd:%s)\n",
					seqtbl->name,
					pnobj_type_to_string(type),
					(((struct cmdinfo *)cmdtbl[i])->name ?
					 ((struct cmdinfo *)cmdtbl[i])->name : "none"));

			/*
			 * CMD_TYP_DMP seq is intended for debugging only.
			 * So if it's failed, go through the dump seq.
			 */
			if (type != CMD_TYPE_DMP)
				return ret;
		}

		s_time = ktime_get();

		panel_dbg("SEQ: %s %s %s- %s\n", seqtbl->name,
				pnobj_type_to_string(type),
				(((struct cmdinfo *)cmdtbl[i])->name ?
				 ((struct cmdinfo *)cmdtbl[i])->name : "none"),
				(condition ? "" : "skipped"));
	}

	/* flush cmdq if end of seq */
	if (depth == 0) {
		mutex_lock(&panel->cmdq.lock);
		panel_cmdq_flush(panel);
		mutex_unlock(&panel->cmdq.lock);
	}

	if (!condition)
		panel_err("condition end missing!\n");

	if (cond_skip_count != 0)
		panel_err("invalid condition pair: %d\n", cond_skip_count);

	return 0;
}

int panel_do_seqtbl(struct panel_device *panel, struct seqinfo *seqtbl)
{
	return _panel_do_seqtbl(panel, seqtbl, 0);
}

int excute_seqtbl_nolock(struct panel_device *panel, struct seqinfo *seqtbl, int index)
{
	int ret;
	struct seqinfo *tbl;
	struct panel_info *panel_data;
	struct timespec64 cur_ts, last_ts, delta_ts;
	s64 elapsed_usec;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (seqtbl == NULL) {
		panel_err("seqtbl is null\n");
		return -EINVAL;
	}

	if (panel_bypass_is_on(panel)) {
		panel_warn("panel no use\n");
		return 0;
	}

	panel_data = &panel->panel_data;
	tbl = seqtbl;
	ktime_get_ts64(&cur_ts);

	if (panel_data->props.key[CMD_LEVEL_1] != 0 ||
			panel_data->props.key[CMD_LEVEL_2] != 0 ||
			panel_data->props.key[CMD_LEVEL_3] != 0) {
		panel_warn("before seq:%s unbalanced key [1]%s(%d) [2]%s(%d) [3]%s(%d)\n",
				tbl[index].name,
				(panel_data->props.key[CMD_LEVEL_1] > 0 ? "ENABLED" : "DISABLED"),
				panel_data->props.key[CMD_LEVEL_1],
				(panel_data->props.key[CMD_LEVEL_2] > 0 ? "ENABLED" : "DISABLED"),
				panel_data->props.key[CMD_LEVEL_2],
				(panel_data->props.key[CMD_LEVEL_3] > 0 ? "ENABLED" : "DISABLED"),
				panel_data->props.key[CMD_LEVEL_3]);
		panel_data->props.key[CMD_LEVEL_1] = 0;
		panel_data->props.key[CMD_LEVEL_2] = 0;
		panel_data->props.key[CMD_LEVEL_3] = 0;
	}

	ret = panel_do_seqtbl(panel, &tbl[index]);
	if (unlikely(ret < 0)) {
		panel_err("failed to excute seqtbl %s(%d)\n",
				tbl[index].name, index);
		ret = -EIO;
		goto do_exit;
	}

do_exit:
	if (panel_data->props.key[CMD_LEVEL_1] != 0 ||
			panel_data->props.key[CMD_LEVEL_2] != 0 ||
			panel_data->props.key[CMD_LEVEL_3] != 0) {
		panel_warn("after seq:%s unbalanced key [1]%s(%d) [2]%s(%d) [3]%s(%d)\n",
				tbl[index].name,
				(panel_data->props.key[CMD_LEVEL_1] > 0 ? "ENABLED" : "DISABLED"),
				panel_data->props.key[CMD_LEVEL_1],
				(panel_data->props.key[CMD_LEVEL_2] > 0 ? "ENABLED" : "DISABLED"),
				panel_data->props.key[CMD_LEVEL_2],
				(panel_data->props.key[CMD_LEVEL_3] > 0 ? "ENABLED" : "DISABLED"),
				panel_data->props.key[CMD_LEVEL_3]);
		panel_data->props.key[CMD_LEVEL_1] = 0;
		panel_data->props.key[CMD_LEVEL_2] = 0;
		panel_data->props.key[CMD_LEVEL_3] = 0;
	}

	ktime_get_ts64(&last_ts);
	delta_ts = timespec64_sub(last_ts, cur_ts);
	elapsed_usec = timespec64_to_ns(&delta_ts) / 1000;
	panel_dbg("seq:%s done (elapsed %2lld.%03lld msec)\n",
			tbl[index].name, elapsed_usec / 1000, elapsed_usec % 1000);

	return 0;
}

int decon_decon_panel_do_seqtbl_by_index_nolock(struct panel_device *panel, int index)
{
	if (panel == NULL) {
		panel_err("invalid panel\n");
		return -EINVAL;
	}

	if (panel->panel_data.seqtbl == NULL) {
		panel_err("invalid seqtbl\n");
		return -EINVAL;
	}

	if (unlikely(index < 0 || index >= MAX_PANEL_SEQ)) {
		panel_err("invalid parameter (panel %p, index %d)\n", panel, index);
		return -EINVAL;
	}

	return excute_seqtbl_nolock(panel, panel->panel_data.seqtbl, index);
}
EXPORT_SYMBOL(decon_decon_panel_do_seqtbl_by_index_nolock);

int decon_panel_do_seqtbl_by_index(struct panel_device *panel, int index)
{
	int ret;

	mutex_lock(&panel->op_lock);
	ret = decon_decon_panel_do_seqtbl_by_index_nolock(panel, index);
	mutex_unlock(&panel->op_lock);

	return ret;
}
EXPORT_SYMBOL(decon_panel_do_seqtbl_by_index);

struct resinfo *decon_find_panel_resource(struct panel_info *panel_data, char *name)
{
	int i = 0;

	if (unlikely(!panel_data->restbl))
		return NULL;

	for (i = 0; i < panel_data->nr_restbl; i++) {
		if (panel_data->restbl[i].name == NULL)
			continue;
		if (!strcmp(name, panel_data->restbl[i].name))
			return &panel_data->restbl[i];
	}
	return NULL;
}
EXPORT_SYMBOL(decon_find_panel_resource);

bool decon_panel_resource_initialized(struct panel_info *panel_data, char *name)
{
	struct resinfo *res = decon_find_panel_resource(panel_data, name);

	if (unlikely(!res)) {
		panel_err("%s not found in resource\n", name);
		return false;
	}

	return res->state == RES_INITIALIZED;
}
EXPORT_SYMBOL(decon_panel_resource_initialized);

int decon_rescpy(u8 *dst, struct resinfo *res, u32 offset, u32 len)
{
	if (unlikely(!dst || !res)) {
		panel_warn("invalid parameter\n");
		return -EINVAL;
	}

	if (unlikely(offset + len > res->dlen)) {
		panel_err("exceed range (offset %d, len %d, res->dlen %d\n",
				offset, len, res->dlen);
		return -EINVAL;
	}

	memcpy(dst, &res->data[offset], len);
	return 0;
}
EXPORT_SYMBOL(decon_rescpy);

int decon_rescpy_by_name(struct panel_info *panel_data, u8 *dst, char *name, u32 offset, u32 len)
{
	struct resinfo *res;

	if (unlikely(!panel_data || !dst || !name)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	res = decon_find_panel_resource(panel_data, name);
	if (unlikely(!res)) {
		panel_err("%s not found in resource\n", name);
		return -EINVAL;
	}

	if (unlikely(offset + len > res->dlen)) {
		panel_err("exceed range (offset %d, len %d, res->dlen %d\n",
				offset, len, res->dlen);
		return -EINVAL;
	}
	memcpy(dst, &res->data[offset], len);
	return 0;
}

int get_panel_resource_size(struct resinfo *res)
{
	if (unlikely(!res)) {
		panel_warn("invalid parameter\n");
		return -EINVAL;
	}

	return res->dlen;
}

int resource_clear(struct resinfo *res)
{
	if (!res)
		return -EINVAL;

	res->state = RES_UNINITIALIZED;
	return 0;
}

int resource_clear_by_name(struct panel_info *panel_data, char *name)
{
	struct resinfo *res;

	if (unlikely(!panel_data || !name)) {
		panel_warn("invalid parameter\n");
		return -EINVAL;
	}

	res = decon_find_panel_resource(panel_data, name);
	if (unlikely(!res)) {
		panel_warn("%s not found in resource\n", name);
		return -EINVAL;
	}

	return resource_clear(res);
}

int resource_copy(u8 *dst, struct resinfo *res)
{
	if (unlikely(!dst || !res)) {
		panel_warn("invalid parameter\n");
		return -EINVAL;
	}

	if (res->state == RES_UNINITIALIZED) {
		panel_warn("%s not initialized\n", res->name);
		return -EINVAL;
	}
	return decon_rescpy(dst, res, 0, res->dlen);
}
EXPORT_SYMBOL(resource_copy);

int resource_copy_by_name(struct panel_info *panel_data, u8 *dst, char *name)
{
	struct resinfo *res;

	if (unlikely(!panel_data || !dst || !name)) {
		panel_warn("invalid parameter\n");
		return -EINVAL;
	}

	res = decon_find_panel_resource(panel_data, name);
	if (unlikely(!res)) {
		panel_warn("%s not found in resource\n", name);
		return -EINVAL;
	}

	if (res->state == RES_UNINITIALIZED) {
		panel_warn("%s not initialized\n", res->name);
		return -EINVAL;
	}
	return decon_rescpy(dst, res, 0, res->dlen);
}
EXPORT_SYMBOL(resource_copy_by_name);

int decon_resource_copy_n_clear_by_name(struct panel_info *panel_data, u8 *dst, char *name)
{
	struct resinfo *res;
	int ret;

	if (unlikely(!panel_data || !dst || !name)) {
		panel_warn("invalid parameter\n");
		return -EINVAL;
	}

	res = decon_find_panel_resource(panel_data, name);
	if (unlikely(!res)) {
		panel_warn("%s not found in resource\n", name);
		return -EINVAL;
	}

	if (res->state == RES_UNINITIALIZED) {
		panel_warn("%s not initialized\n", res->name);
		return -EINVAL;
	}
	ret = decon_rescpy(dst, res, 0, res->dlen);
	resource_clear(res);

	return ret;
}
EXPORT_SYMBOL(decon_resource_copy_n_clear_by_name);

int decon_get_resource_size_by_name(struct panel_info *panel_data, char *name)
{
	struct resinfo *res;

	if (unlikely(!panel_data || !name)) {
		panel_warn("invalid parameter\n");
		return -EINVAL;
	}

	res = decon_find_panel_resource(panel_data, name);
	if (unlikely(!res)) {
		panel_warn("%s not found in resource\n", name);
		return -EINVAL;
	}

	return get_panel_resource_size(res);
}
EXPORT_SYMBOL(decon_get_resource_size_by_name);

#define MAX_READ_BYTES  (128)
int panel_rx_nbytes(struct panel_device *panel,
		u32 type, u8 *buf, u8 addr, u32 offset, u32 len)
{
	int ret, read_len, remained = len, index = 0;
	u32 pos = offset;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!IS_CMD_TYPE_RX_PKT(type)) {
		panel_err("invalid type %d\n", type);
		return -EINVAL;
	}

#ifdef CONFIG_SUPPORT_POC_SPI
	if (type == SPI_PKT_TYPE_RD) {
		ret = panel_spi_read_data(panel, addr, buf, len);
		if (ret < 0)
			return ret;

		return len;
	}
#endif

	while (remained > 0) {
		ret = 0;
		read_len = remained < MAX_READ_BYTES ? remained : MAX_READ_BYTES;
		if (type == DSI_PKT_TYPE_RD) {
			if ((panel->panel_data.ddi_props.gpara &
					DDI_SUPPORT_READ_GPARA) == false) {
				/*
				 * S6E3HA9 DDI NOT SUPPORTED GPARA FOR READ
				 * SO TEMPORARY DISABLE GPRAR FOR READ FUNCTION
				 */
				char *temp_buf = kmalloc(pos + read_len, GFP_KERNEL);

				if (!temp_buf)
					return -EINVAL;

				ret = panel_dsi_read_data(panel,
						addr, 0, temp_buf, pos + read_len);
				ret -= pos;
				memcpy(&buf[index], temp_buf + pos, read_len);
				kfree(temp_buf);
			} else {
				ret = panel_dsi_read_data(panel,
						addr, pos, &buf[index], read_len);
			}
#ifdef CONFIG_EXYNOS_DECON_LCD_SPI
		} else if (type == SPI_PKT_TYPE_RD) {
			if (pos != 0) {
				int gpara_len = 1;
				u8 gpara[4] = { 0xB0, 0x00 };

				/* gpara 16bit offset */
				if (panel->panel_data.ddi_props.gpara &
						DSIM_OPTION_2BYTE_GPARA)
					gpara[gpara_len++] = (pos >> 8) & 0xFF;
				gpara[gpara_len++] = pos & 0xFF;

				ret = panel_dsi_write_cmd(panel,
						MIPI_DSI_WR_GEN_CMD, gpara, 0, gpara_len, true);
				if (ret != gpara_len)
					panel_err("failed to set gpara %d (ret %d)\n", pos, ret);
			}
			ret = panel_spi_read_data(panel->spi,
					addr, &buf[index], read_len);
#endif
		}

		if (ret != read_len) {
			panel_err("failed to read addr:0x%02X, pos:%d, len:%u ret %d\n",
					addr, pos, len, ret);
			return -EINVAL;
		}
		index += read_len;
		pos += read_len;
		remained -= read_len;
	}

	panel_dbg("addr:%2Xh, pos:%u, len:%u\n", addr, offset, len);
	print_data(buf, len);
	return len;
}

int panel_tx_nbytes(struct panel_device *panel,
		u32 type, u8 *buf, u8 addr, u32 pos, u32 len)
{
	int ret;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!IS_CMD_TYPE_TX_PKT(type)) {
		panel_err("invalid type %d\n", type);
		return -EINVAL;
	}

	ret = panel_dsi_write_cmd(panel, MIPI_DSI_WR_GEN_CMD, buf, pos, len, false);
	if (ret != len) {
		panel_err("failed to write addr:0x%02X, pos:%d, len:%u ret %d\n",
				addr, pos, len, ret);
		return -EINVAL;
	}

	panel_dbg("addr:%2Xh, pos:%d, len:%u\n", buf[0], pos, len);
	print_data(buf, len);
	return ret;
}

/*
 * ID READ REG Features:
 * Read 1 byte from each registers if DADBDC or ADAEAF feature is set.
 * It works also for compatibility if REG_04 feature is NOT set and LCD_TFT_COMMON feature is set.
 * Other cases, read 3 bytes from 04h.
 */
#if IS_ENABLED(CONFIG_PANEL_ID_READ_REG_DADBDC) || \
	IS_ENABLED(CONFIG_PANEL_ID_READ_REG_ADAEAF) || \
	(!IS_ENABLED(CONFIG_PANEL_ID_READ_REG_04) && IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_TFT_COMMON))
int read_panel_id(struct panel_device *panel, u8 *buf)
{
	int len, ret = 0;
	int i = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!IS_PANEL_ACTIVE(panel))
		return -ENODEV;

	panel_info("id read from 0x%02X\n", PANEL_ID_REG);
	mutex_lock(&panel->op_lock);
	for (i = 0; i < PANEL_ID_LEN; i++) {
		len = panel_rx_nbytes(panel, DSI_PKT_TYPE_RD, buf + i, PANEL_ID_REG + i, 0, 1);
		if (len < 0) {
			panel_err("failed to read id\n");
			ret = -EINVAL;
			goto read_err;
		}
	}

read_err:
	mutex_unlock(&panel->op_lock);
	return ret;
}
#else
int read_panel_id(struct panel_device *panel, u8 *buf)
{
	int len, ret = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!IS_PANEL_ACTIVE(panel))
		return -ENODEV;

	panel_info("id read from 0x%02X 3bytes\n", PANEL_ID_REG);
	mutex_lock(&panel->op_lock);
	panel_set_key(panel, 3, true);
	len = panel_rx_nbytes(panel, DSI_PKT_TYPE_RD, buf, PANEL_ID_REG, 0, 3);
	if (len != 3) {
		panel_err("failed to read id\n");
		ret = -EINVAL;
		goto read_err;
	}

read_err:
	panel_set_key(panel, 3, false);
	mutex_unlock(&panel->op_lock);
	return ret;
}
#endif

static struct rdinfo *find_panel_rdinfo(struct panel_info *panel_data, char *name)
{
	int i;

	for (i = 0; i < panel_data->nr_rditbl; i++)
		if (!strcmp(name, panel_data->rditbl[i].name))
			return &panel_data->rditbl[i];

	return NULL;
}

int panel_rdinfo_update(struct panel_device *panel, struct rdinfo *rdi)
{
	int ret;

	if (unlikely(!panel || !rdi)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	kfree(rdi->data);
	rdi->data = kcalloc(rdi->len, sizeof(u8), GFP_KERNEL);
	if (!rdi->data)
		return -ENOMEM;

#ifdef CONFIG_SUPPORT_DDI_FLASH
	if (rdi->type == DSI_PKT_TYPE_RD_POC)
		ret = copy_poc_partition(&panel->poc_dev, rdi->data, rdi->addr, rdi->offset, rdi->len);
	else
		ret = panel_rx_nbytes(panel, rdi->type, rdi->data, rdi->addr, rdi->offset, rdi->len);
#else
	ret = panel_rx_nbytes(panel, rdi->type, rdi->data, rdi->addr, rdi->offset, rdi->len);
#endif
	if (unlikely(ret != rdi->len)) {
		panel_err("read failed\n");
		return -EIO;
	}

	return 0;
}

int panel_rdinfo_update_by_name(struct panel_device *panel, char *name)
{
	int ret;
	struct rdinfo *rdi;
	struct panel_info *panel_data;

	if (unlikely(!panel || !name)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	rdi = find_panel_rdinfo(panel_data, name);
	if (unlikely(!rdi)) {
		panel_err("read info %s not found\n", name);
		return -ENODEV;
	}

	ret = panel_rdinfo_update(panel, rdi);
	if (ret) {
		panel_err("failed to update read info\n");
		return -EINVAL;
	}

	return 0;
}

void print_panel_resource(struct panel_device *panel)
{
	struct panel_info *panel_data;
	int i;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return;
	}

	panel_data = &panel->panel_data;
	if (unlikely(!panel_data)) {
		panel_err("panel_data is null\n");
		return;
	}

	for (i = 0; i < panel_data->nr_restbl; i++) {
		panel_info("[panel_resource : %12s %3d bytes] %s\n",
				panel_data->restbl[i].name, panel_data->restbl[i].dlen,
				(panel_data->restbl[i].state == RES_UNINITIALIZED ?
				 "UNINITIALIZED" : "INITIALIZED"));
		if (panel_data->restbl[i].state == RES_INITIALIZED)
			print_data(panel_data->restbl[i].data,
					panel_data->restbl[i].dlen);
	}
}

int panel_resource_update(struct panel_device *panel, struct resinfo *res)
{
	int i, ret;
	struct rdinfo *rdi;

	if (unlikely(!panel || !res)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	for (i = 0; i < res->nr_resui; i++) {
		rdi = res->resui[i].rditbl;
		if (unlikely(!rdi)) {
			panel_warn("read info not exist\n");
			return -EINVAL;
		}
		ret = panel_rdinfo_update(panel, rdi);
		if (ret) {
			panel_err("failed to update read info\n");
			return -EINVAL;
		}

		if (!res->data) {
			panel_warn("resource data not exist\n");
			return -EINVAL;
		}
		memcpy(res->data + res->resui[i].offset, rdi->data, rdi->len);
		res->state = RES_INITIALIZED;
	}

	panel_dbg("[panel_resource : %12s %3d bytes] %s\n",
			res->name, res->dlen, (res->state == RES_UNINITIALIZED ?
				"UNINITIALIZED" : "INITIALIZED"));
	if (res->state == RES_INITIALIZED)
		print_data(res->data, res->dlen);

	return 0;
}

int __mockable decon_panel_resource_update_by_name(struct panel_device *panel, char *name)
{
	int ret;
	struct resinfo *res;
	struct panel_info *panel_data;

	if (unlikely(!panel || !name)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	res = decon_find_panel_resource(panel_data, name);
	if (unlikely(!res)) {
		panel_warn("%s not found in resource\n", name);
		return -EINVAL;
	}

	ret = panel_resource_update(panel, res);
	if (unlikely(ret)) {
		panel_err("failed to update resource\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(decon_panel_resource_update_by_name);

int panel_dumpinfo_update(struct panel_device *panel, struct dumpinfo *info)
{
	int ret;

	if (unlikely(!panel || !info || !info->res || !info->callback)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	ret = panel_resource_update(panel, info->res);
	if (unlikely(ret < 0)) {
		panel_err("dump:%s failed to update resource\n", info->name);
		return ret;
	}

	ret = info->callback(info);
	if (unlikely(ret < 0)) {
		panel_err("dump:%s failed to callback\n", info->name);
		return ret;
	}

	return 0;
}

u16 calc_checksum_16bit(u8 *arr, int size)
{
	u16 chksum = 0;
	int i;

	for (i = 0; i < size; i++)
		chksum += arr[i];

	return chksum;
}

int check_panel_active(struct panel_device *panel, const char *caller)
{
	struct panel_state *state;
	int ctrl_interface_state;

	if (unlikely(!panel)) {
		panel_err("%s:panel is null\n", caller);
		return 0;
	}

	state = &panel->state;
	if (panel_bypass_is_on(panel)) {
		panel_warn("%s:panel no use\n", caller);
		return 0;
	}
	ctrl_interface_state = panel_dsi_get_state(panel);
	if (ctrl_interface_state < 0) {
		panel_warn("%s:unable to get dsi state\n", caller);
		return 0;
	}

	if (ctrl_interface_state == CTRL_INTERFACE_STATE_INACTIVE) {
		panel_warn("%s:dsi is inactive state\n", caller);
		return 0;
	}

#if defined(CONFIG_SUPPORT_PANEL_SWAP)
	if (panel_disconnected(panel)) {
		panel_warn("panel disconnected\n");
		return 0;
	}
#endif

	return 1;
}

int find_sysfs_arg_by_name(struct sysfs_arg *arglist, int nr_arglist, char *s)
{
	int i;
	const char *name;

	if (arglist == NULL || s == NULL)
		return -EINVAL;

	for (i = 0; i < nr_arglist; i++) {
		name = arglist[i].name;
		if (name == NULL)
			continue;

		if (!strncmp(name, s, strlen(name)))
			return i;
	}

	return -EINVAL;
}

int parse_sysfs_arg(int nargs, enum sysfs_arg_type type,
		char *s, struct sysfs_arg_out *out)
{
	int i, rc, parse;
	char *p = s;

	if (s == NULL || out == NULL ||
		nargs > MAX_SYSFS_ARG_NUM ||
		type >= MAX_SYSFS_ARG_TYPE)
		return -EINVAL;

	if (type == SYSFS_ARG_TYPE_NONE)
		return 0;

	for (i = 0; i < nargs; i++) {
		if (type == SYSFS_ARG_TYPE_S32)
			rc = sscanf(p, "%d%n", &out->d[i].val_s32, &parse);
		else if (type == SYSFS_ARG_TYPE_U32)
			rc = sscanf(p, "%u%n", &out->d[i].val_u32, &parse);
		else if (type == SYSFS_ARG_TYPE_STR)
			rc = sscanf(p, "%31s%n", out->d[i].val_str, &parse);
		if (rc != 1) {
			panel_err("invalid arg(%s), nargs(%d), type(%d), rc(%d)\n",
					s, nargs, type, rc);
			return -EINVAL;
		}
		p += parse;
	}
	out->nargs = nargs;

	return (int)(p - s);
}