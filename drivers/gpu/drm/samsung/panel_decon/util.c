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
#include "util.h"

/*
 * copy from slided source byte array to
 * continuous destination byte array
 */
int decon_copy_from_sliced_byte_array(u8 *dest, const u8 *src,
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
EXPORT_SYMBOL(decon_copy_from_sliced_byte_array);

/*
 * copy to slided destination byte array from
 * continuous source byte array
 */
int decon_copy_to_sliced_byte_array(u8 *dest, const u8 *src,
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
EXPORT_SYMBOL(decon_copy_to_sliced_byte_array);

/*
 * decon_hextos32 - hexa-decimal to signed int
 * @hex : input hexa deciaml number
 * @bits : total number of bits
 * MSB(most-significant-bit) is signed bit.
 * for example, decon_hextos32(0x3FF, 10) returns -511.
 */
s32 decon_hextos32(u32 hex, u32 bits)
{
	int sign = (hex & BIT_MASK(bits - 1)) ? -1 : 1;

	return sign * (hex & GENMASK(bits - 2, 0));
}
EXPORT_SYMBOL(decon_hextos32);

/*
 * decon_s32tohex - signed int to hexa-decimal
 * @dec : input signed deciaml number
 * @bits : total number of bits
 * MSB(most-significant-bit) is signed bit.
 * for example, decon_s32tohex(-511, 10) returns 0x3FF.
 */
u32 decon_s32tohex(s32 dec, u32 bits)
{
	u32 signed_bit = (dec < 0) ? BIT_MASK(bits - 1) : 0;

	return (signed_bit | (abs(dec) & GENMASK(bits - 2, 0)));
}
EXPORT_SYMBOL(decon_s32tohex);
