/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include "../panel_kunit.h"
#include "../panel.h"
#include "../dpui.h"
#include "../panel_debug.h"
#include "../maptbl.h"
#include "oled_common_dump.h"
#include "../abd.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

static void make_abd_print_arr(struct dumpinfo *dump)
{
	int i, bit;
	char **print_tbl;
	unsigned long expect_mask;

	if (dump->abd_print)
		return;

	print_tbl = kzalloc(sizeof(char *) * dump->res->dlen * BITS_PER_BYTE, GFP_KERNEL);

	for (i = 0; i < dump->nr_expects; i++) {
		expect_mask = dump->expects[i].mask;
		for_each_set_bit(bit, &expect_mask, BITS_PER_BYTE)
			print_tbl[dump->expects[i].offset * BITS_PER_BYTE + bit] = dump->expects[i].msg;
	}

	dump->abd_print = print_tbl;
}

static void check_usdm_abd_mask_bit(struct dumpinfo *dump)
{
	int i = 0;
	u32 res_size;
	u32 offset, offset_end = 0;
	u8 expected_values[4] = {0 };
	u8 values[4] = {0 };

	res_size = get_resource_size(dump->res);
	if (res_size == 0)
		return;

	for (i = 0; i < dump->nr_expects; i++) {
		offset = dump->expects[i].offset;
		/* abd can get max 4 bytes (unsigned int) */
		if (offset >= 4)
			continue;

		values[offset] |= dump->res->data[offset] & dump->expects[i].mask;
		expected_values[offset] |= dump->expects[i].value;
		offset_end = max(offset, offset_end);
	}

	offset_end = min((u32)get_resource_size(dump->res), offset_end + 1);

	if (!memcmp(values, expected_values, offset_end))
		return;

	make_abd_print_arr(dump);
	usdm_abd_mask_bit(NULL, offset_end * BITS_PER_BYTE, get_merged_value(values, 4),
				dump->abd_print, get_merged_value(expected_values, 4));
}

__visible_for_testing int snprintf_dump_expect_memeq(char *buf, size_t size, struct dumpinfo *dump)
{
	int i, l = 0;
	u32 res_size;
	u32 offset, offset_end = 0;
	u8 *masks, *expected_values, *values;
	int result;

	res_size = get_resource_size(dump->res);
	if (res_size == 0)
		return 0;

	masks = kzalloc(res_size, GFP_KERNEL);
	expected_values = kzalloc(res_size, GFP_KERNEL);
	values = kzalloc(res_size, GFP_KERNEL);

	for (i = 0; i < dump->nr_expects; i++) {
		offset = dump->expects[i].offset;
		masks[offset] |= dump->expects[i].mask;
		values[offset] |= dump->res->data[offset] & dump->expects[i].mask;
		expected_values[offset] |= dump->expects[i].value;
		offset_end = max(offset, offset_end);
	}
	offset_end = min((u32)get_resource_size(dump->res), offset_end + 1);
	
	result = (!memcmp(values, expected_values, offset_end)) ?
		DUMP_STATUS_SUCCESS : DUMP_STATUS_FAILURE;
	dump->result = result;

	l += snprintf(buf + l, size - l, "SHOW PANEL REG[%s:%s:(",
			get_resource_name(dump->res),
			(result == DUMP_STATUS_SUCCESS) ? "GD" : "NG");

	for (i = 0; i < offset_end; i++)
		l += snprintf(buf + l, size - l, "%02X", values[i]);

	l += snprintf(buf + l, size - l, " %s ",
			(result == DUMP_STATUS_SUCCESS) ? "==" : "!=");

	for (i = 0; i < offset_end; i++)
		l += snprintf(buf + l, size - l, "%02X", expected_values[i]);

	l += snprintf(buf + l, size - l, ")]\n");

	kfree(masks);
	kfree(expected_values);
	kfree(values);

	return l;
}

__visible_for_testing int snprintf_dump_expect_param_equal(char *buf, size_t size, struct dumpinfo *dump)
{
	int i, l = 0;
	u32 res_size;
	u8 value, expected_value;

	res_size = get_resource_size(dump->res);
	if (res_size == 0)
		return 0;

	for (i = 0; i < dump->nr_expects; i++) {
		expected_value = dump->expects[i].value;
		/* TODO get resource 1 byte using helper */
		value = dump->res->data[dump->expects[i].offset] &
				dump->expects[i].mask;

		if (expected_value != value)
			l += snprintf(buf + l, max((int)size - l, 0), "%s\n",
					dump->expects[i].msg);
	}

	return l;
}

int snprintf_dump_expects(char *buf, size_t size, struct dumpinfo *dump)
{
	int l = 0;

	if (!dump) {
		panel_err("dump is null\n");
		return -EINVAL;
	}

	if (!is_valid_resource(dump->res)) {
		panel_err("invalid dump(%s) resource\n",
				get_dump_name(dump));
		return -EINVAL;
	}

	l += snprintf_dump_expect_memeq(buf + l, size - l, dump);
	l += snprintf_dump_expect_param_equal(buf + l, size - l, dump);

	check_usdm_abd_mask_bit(dump);

	return l;
}

int oled_dump_show_expects(struct dumpinfo *dump)
{
	int len;
	char buf[SZ_256];

	if (!dump)
		return -EINVAL;

	if (!is_valid_resource(dump->res))
		return -EINVAL;

	if (!is_resource_initialized(dump->res))
		return -EINVAL;

	len = snprintf_dump_expects(buf, SZ_256, dump);
	if (len > 0)
		panel_info("%s", buf);

	return 0;
}
EXPORT_SYMBOL(oled_dump_show_expects);

int oled_dump_show_resource(struct dumpinfo *dump)
{
	if (!dump)
		return -EINVAL;

	if (!is_valid_resource(dump->res))
		return -EINVAL;

	if (!is_resource_initialized(dump->res))
		return -EINVAL;

	print_resource(dump->res);

	return 0;
}

int oled_dump_show_resource_and_panic(struct dumpinfo *dump)
{
	oled_dump_show_resource(dump);
	BUG();
}

int oled_dump_show_rddpm(struct dumpinfo *dump)
{
	int ret;
	struct resinfo *res;
	u8 rddpm[PANEL_RDDPM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddpm;
#endif

	if (!dump)
		return -EINVAL;

	res = dump->res;
	if (!is_valid_resource(res))
		return -EINVAL;

	if (!is_resource_initialized(res))
		return -EINVAL;

	if (!res || ARRAY_SIZE(rddpm) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = copy_resource(rddpm, dump->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy resource(%s)\n",
				get_resource_name(res));
		return -EINVAL;
	}

#ifdef CONFIG_LOGGING_BIGDATA_BUG
	g_rddpm = (unsigned int)rddpm[0];
#endif

	return oled_dump_show_expects(dump);
}

int oled_dump_show_rddpm_before_sleep_in(struct dumpinfo *dump)
{
	return oled_dump_show_expects(dump);
}

int oled_dump_show_rddsm(struct dumpinfo *dump)
{
	int ret;
	struct resinfo *res;
	u8 rddsm[PANEL_RDDSM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddsm;
#endif

	if (!dump)
		return -EINVAL;

	res = dump->res;
	if (!is_valid_resource(res))
		return -EINVAL;

	if (!is_resource_initialized(res))
		return -EINVAL;

	if (!res || ARRAY_SIZE(rddsm) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = copy_resource(rddsm, dump->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddsm resource\n");
		return -EINVAL;
	}

#ifdef CONFIG_LOGGING_BIGDATA_BUG
	g_rddsm = (unsigned int)rddsm[0];
#endif

	return oled_dump_show_expects(dump);
}

int oled_dump_show_err(struct dumpinfo *dump)
{
	return oled_dump_show_expects(dump);
}

int oled_dump_show_err_fg(struct dumpinfo *dump)
{
	int ret;
	u8 err_fg[OLED_ERR_FG_LEN] = { 0, };
	struct resinfo *res;

	if (!dump)
		return -EINVAL;

	res = dump->res;
	if (!is_valid_resource(res))
		return -EINVAL;

	if (!is_resource_initialized(res))
		return -EINVAL;

	if (!res || ARRAY_SIZE(err_fg) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = copy_resource(err_fg, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy err_fg resource\n");
		return -EINVAL;
	}

#ifdef CONFIG_USDM_PANEL_DPUI
	if (err_fg[0] & 0x04)
		inc_dpui_u32_field(DPUI_KEY_PNVLO3E, 1);

	if (err_fg[0] & 0x08)
		inc_dpui_u32_field(DPUI_KEY_PNELVDE, 1);

	if (err_fg[0] & 0x40)
		inc_dpui_u32_field(DPUI_KEY_PNVLI1E, 1);
#endif

	return oled_dump_show_expects(dump);
}

int oled_dump_show_dsi_err(struct dumpinfo *dump)
{
	int ret;
	struct resinfo *res;
	u8 dsi_err[OLED_DSI_ERR_LEN] = { 0, };

	if (!dump)
		return -EINVAL;

	res = dump->res;
	if (!is_valid_resource(res))
		return -EINVAL;

	if (!is_resource_initialized(res))
		return -EINVAL;

	if (!res || ARRAY_SIZE(dsi_err) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = copy_resource(dsi_err, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy resource(%s)\n",
				get_resource_name(res));
		return -EINVAL;
	}

#ifdef CONFIG_USDM_PANEL_DPUI
	inc_dpui_u32_field(DPUI_KEY_PNDSIE, dsi_err[0]);
#endif
#if IS_ENABLED(CONFIG_SEC_ABC)
	if (dsi_err[0] > 0)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=display@dump=act_section_dsierr0");
#else
		sec_abc_send_event("MODULE=display@WARN=act_section_dsierr0");
#endif
#endif /* CONFIG_SEC_ABC */

	return oled_dump_show_expects(dump);
}

int oled_dump_show_self_diag(struct dumpinfo *dump)
{
	int ret;
	struct resinfo *res;
	u8 self_diag[OLED_SELF_DIAG_LEN] = { 0, };

	if (!dump)
		return -EINVAL;

	res = dump->res;
	if (!is_valid_resource(res))
		return -EINVAL;

	if (!is_resource_initialized(res))
		return -EINVAL;

	if (!res || ARRAY_SIZE(self_diag) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = copy_resource(self_diag, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy resource(%s)\n",
				get_resource_name(res));
		return -EINVAL;
	}

#ifdef CONFIG_USDM_PANEL_DPUI
	inc_dpui_u32_field(DPUI_KEY_PNSDRE,
			((self_diag[0] & OLED_SELF_DIAG_MASK)
			 == OLED_SELF_DIAG_VALUE) ? 0 : 1);
#endif

	return oled_dump_show_expects(dump);
}

int oled_dump_show_ecc_err(struct dumpinfo *dump)
{
	return oled_dump_show_expects(dump);
}

int oled_dump_show_ssr_err(struct dumpinfo *dump)
{
	return oled_dump_show_expects(dump);
}

int oled_dump_show_flash_loaded(struct dumpinfo *dump)
{
	return oled_dump_show_expects(dump);
}

int oled_dump_show_self_mask_crc(struct dumpinfo *dump)
{
	int ret;
	struct resinfo *res;
	u8 crc[OLED_SELF_MASK_CRC_LEN] = {0, };

	if (!dump)
		return -EINVAL;

	res = dump->res;
	if (!is_valid_resource(res))
		return -EINVAL;

	if (!is_resource_initialized(res))
		return -EINVAL;

	if (!res || ARRAY_SIZE(crc) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = copy_resource(crc, dump->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to self mask crc resource\n");
		return -EINVAL;
	}

	panel_info("======= SHOW PANEL [7Fh:SELF_MASK_CRC] dump =======\n");
	panel_info("* Reg Value : 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
			crc[0], crc[1], crc[2], crc[3]);
	panel_info("====================================================\n");

	return 0;
}

#ifdef CONFIG_USDM_DDI_CMDLOG
int oled_dump_show_cmdlog(struct dumpinfo *dump)
{
	return oled_dump_show_resource(dump);
}
#endif /* CONFIG_USDM_DDI_CMDLOG */

#ifdef CONFIG_USDM_PANEL_MAFPC
int oled_dump_show_mafpc_log(struct dumpinfo *dump)
{
	return oled_dump_show_expects(dump);
}

int oled_dump_show_mafpc_flash_log(struct dumpinfo *dump)
{
	return oled_dump_show_expects(dump);
}

int oled_dump_show_abc_crc_log(struct dumpinfo *dump)
{
	return oled_dump_show_resource(dump);
}
#endif /* CONFIG_USDM_PANEL_MAFPC */

MODULE_DESCRIPTION("oled_common_dump driver");
MODULE_LICENSE("GPL");
