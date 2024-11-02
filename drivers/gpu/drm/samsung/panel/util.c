// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <asm-generic/errno-base.h>
#include <linux/string.h>
#include <linux/bits.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include "panel_kunit.h"
#include "panel_debug.h"
#include "util.h"

#ifdef CONFIG_UML
#define rtc_time_to_tm(a, b)	(memset(b, 0, sizeof(struct rtc_time)))
#else
#define rtc_time_to_tm(a, b)	rtc_time64_to_tm(a, b)
#endif

/*
 * copy from slided source byte array to
 * continuous destination byte array
 */
int copy_from_sliced_byte_array(u8 *dest, const u8 *src,
		int start, int stop, int step)
{
	u8 *d = dest;
	const u8 *s = src;
	int i;

	if (!dest || !src)
		return -EINVAL;

	if (step == 0)
		return 0;

	if (start < stop && step > 0)
		for (i = start; i < stop && i >= 0; i += step)
			*d++ = *(s + i);
	else if (start > stop && step < 0)
		for (i = start; i > stop && i >= 0; i += step)
			*d++ = *(s + i);

	return (int)(d - dest);
}
EXPORT_SYMBOL(copy_from_sliced_byte_array);

/*
 * copy to slided destination byte array from
 * continuous source byte array
 */
int copy_to_sliced_byte_array(u8 *dest, const u8 *src,
		int start, int stop, int step)
{
	u8 *d = dest;
	const u8 *s = src;
	int i;

	if (!dest || !src)
		return -EINVAL;

	if (start < stop && step > 0)
		for (i = start; i < stop && i >= 0; i += step)
			*(d + i) = *s++;
	else if (start > stop && step < 0)
		for (i = start; i > stop && i >= 0; i += step)
			*(d + i) = *s++;

	return (int)(s - src);
}
EXPORT_SYMBOL(copy_to_sliced_byte_array);

/*
 * hextos32 - hexa-decimal to signed int
 * @hex : input hexa deciaml number
 * @bits : total number of bits
 * MSB(most-significant-bit) is signed bit.
 * for example, hextos32(0x3FF, 10) returns -511.
 */
s32 hextos32(u32 hex, u32 bits)
{
	int sign = (hex & BIT_MASK(bits - 1)) ? -1 : 1;

	return sign * (hex & GENMASK(bits - 2, 0));
}
EXPORT_SYMBOL(hextos32);

/*
 * s32tohex - signed int to hexa-decimal
 * @dec : input signed deciaml number
 * @bits : total number of bits
 * MSB(most-significant-bit) is signed bit.
 * for example, s32tohex(-511, 10) returns 0x3FF.
 */
u32 s32tohex(s32 dec, u32 bits)
{
	u32 signed_bit = (dec < 0) ? BIT_MASK(bits - 1) : 0;

	return (signed_bit | (abs(dec) & GENMASK(bits - 2, 0)));
}
EXPORT_SYMBOL(s32tohex);

u16 calc_checksum_16bit(u8 *arr, int size)
{
	u16 chksum = 0;
	int i;

	for (i = 0; i < size; i++)
		chksum += arr[i];

	return chksum;
}

__visible_for_testing int usdm_snprintf_bytes_line(char *linebuf, size_t linebuflen, int rowsize,
		const u8 *bytes, size_t sz_bytes)
{
	int i, lx = 0;
	u8 ch;

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	if (sz_bytes > rowsize)
		sz_bytes = rowsize;

	if (!linebuflen)
		return 0;

	if (!sz_bytes)
		goto nil;

	for (i = 0; i < sz_bytes; i++) {
		if (linebuflen < lx + 2)
			goto nil;
		ch = bytes[i];
		linebuf[lx++] = hex_asc_hi(ch);
		if (linebuflen < lx + 2)
			goto nil;
		linebuf[lx++] = hex_asc_lo(ch);
		if (linebuflen < lx + 2)
			goto nil;
		if (i + 1 < sz_bytes)
			linebuf[lx++] = ' ';
	}

nil:
	linebuf[lx] = '\0';
	return lx;
}

#define HEX_DUMP_ROWSIZE (32)
int usdm_snprintf_bytes(char *str, size_t size,
		const u8 *bytes, size_t sz_bytes)
{
	int i, len = 0, linelen, remaining = sz_bytes;

	for (i = 0; (i < sz_bytes) && (remaining > 0); i += HEX_DUMP_ROWSIZE) {
		/* 2-digits and null terminator */
		if (size < len + 3)
			break;

		linelen = min_t(int, remaining, HEX_DUMP_ROWSIZE);
		remaining -= HEX_DUMP_ROWSIZE;

		if (i > 0 && size > len + 1)
			len += snprintf(str + len, size - len, "\n");
		len += usdm_snprintf_bytes_line(str + len, size - len,
				HEX_DUMP_ROWSIZE, bytes + i, linelen);
	}

	return len;
}

#define MAX_PRINT_HEX_LEN (1024)
void usdm_print_bytes(int log_level, const void *buf, size_t len)
{
	const u8 *bytes = buf;
	int i, linelen, remaining = len;
	unsigned char linebuf[HEX_DUMP_ROWSIZE * 3 + 2 + HEX_DUMP_ROWSIZE + 1];

	if (!buf)
		return;

	if (log_level > panel_log_level)
		return;

	if (len > MAX_PRINT_HEX_LEN) {
		panel_warn("force set len %lu->%d\n", len, MAX_PRINT_HEX_LEN);
		len = MAX_PRINT_HEX_LEN;
	}

	for (i = 0; i < len; i += HEX_DUMP_ROWSIZE) {
		linelen = min(remaining, HEX_DUMP_ROWSIZE);
		remaining -= HEX_DUMP_ROWSIZE;

		usdm_snprintf_bytes_line(linebuf, sizeof(linebuf),
				HEX_DUMP_ROWSIZE, bytes + i, linelen);
		if (log_level >= 7)
			panel_dbg("%.4xh: %s\n", i, linebuf);
		else
			panel_info("%.4xh: %s\n", i, linebuf);
	}
}
EXPORT_SYMBOL(usdm_print_bytes);

#if IS_ENABLED(CONFIG_RTC_LIB)
void usdm_get_rtc_time(struct rtc_time *tm)
{
	struct timespec64 now;
	unsigned long local_time;

	ktime_get_real_ts64(&now);
	local_time = (now.tv_sec - (sys_tz.tz_minuteswest * 60));
	rtc_time64_to_tm(local_time, tm);
}

int usdm_snprintf_rtc_time(char *buf, size_t size, struct rtc_time *tm)
{
	return snprintf(buf, size, "%02d-%02d %02d:%02d:%02d UTC",
			tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

int usdm_snprintf_current_rtc_time(char *buf, size_t size)
{
	struct rtc_time tm;

	usdm_get_rtc_time(&tm);
	return usdm_snprintf_rtc_time(buf, size, &tm);
}
#endif
