// SPDX-License-Identifier: GPL-2.0-only
//
// rt5691.c  --  RT5691 ALSA SoC audio component driver
//
// Copyright 2022 Realtek Semiconductor Corp.
// Author: Oder Chiou <oder_chiou@realtek.com>
//

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/acpi.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/iio/consumer.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/rt5691.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#include "rt5691.h"

#ifdef CONFIG_SWITCH
static struct switch_dev rt5691_headset_switch = {
	.name = "h2w",
};
#endif

#ifdef CONFIG_DEBUG_FS
static struct dentry *rt5691_debugfs_root;
#endif

static struct {
	int imp;
	int gain;
} rt5691_hp_gain_table[] = {
	{0x000d, 0},
	{0x0019, 0},
	{0x0035, 0},
	{0x0067, 0},
	{0xffff, 0},
};

static const struct reg_default rt5691_reg[] = {
	{0x0000, 0x0000},
	{0x0003, 0xc000},
	{0x0004, 0x0000},
	{0x0005, 0x0000},
	{0x0006, 0x0000},
	{0x000c, 0x1010},
	{0x000d, 0x1000},
	{0x000f, 0x0002},
	{0x0011, 0x0000},
	{0x0024, 0x0000},
	{0x0026, 0xc0c0},
	{0x0027, 0xc0c0},
	{0x0029, 0x8080},
	{0x002a, 0xa0a8},
	{0x002b, 0x80ff},
	{0x0030, 0x0000},
	{0x0031, 0x0000},
	{0x004b, 0x00c0},
	{0x004c, 0x0000},
	{0x004d, 0x0000},
	{0x0050, 0x0080},
	{0x0051, 0x0000},
	{0x0060, 0x0003},
	{0x0061, 0x0000},
	{0x0062, 0x2408},
	{0x0063, 0x0000},
	{0x0064, 0x0000},
	{0x0065, 0x0009},
	{0x0066, 0x0000},
	{0x0067, 0x2f00},
	{0x006b, 0x0000},
	{0x006c, 0x0000},
	{0x0080, 0x0150},
	{0x0081, 0x0000},
	{0x0082, 0x0000},
	{0x0083, 0x0122},
	{0x0084, 0x0300},
	{0x0085, 0x0000},
	{0x0086, 0x0000},
	{0x0087, 0x0122},
	{0x0088, 0x0300},
	{0x008e, 0x4000},
	{0x008f, 0xaa81},
	{0x0090, 0x7680},
	{0x0091, 0x5c18},
	{0x0092, 0x0425},
	{0x0094, 0x008f},
	{0x0095, 0x4000},
	{0x009f, 0x0000},
	{0x00b6, 0x0000},
	{0x00b7, 0x0000},
	{0x00b8, 0x0000},
	{0x00b9, 0x0000},
	{0x00ba, 0x0002},
	{0x00bb, 0x0000},
	{0x00be, 0x0000},
	{0x00d0, 0x0000},
	{0x00d1, 0x6666},
	{0x00d2, 0xa6aa},
	{0x00d3, 0x6666},
	{0x00d4, 0xa6aa},
	{0x00d5, 0x6666},
	{0x00d6, 0xa6aa},
	{0x00f6, 0x0000},
	{0x00fa, 0x0000},
	{0x00fb, 0x0000},
	{0x00fc, 0x0000},
	{0x00fd, 0x0000},
	{0x00fe, 0x10ec},
	{0x00ff, 0x1091},
	{0x0100, 0x2510},
	{0x0101, 0x2510},
	{0x0102, 0x2510},
	{0x0105, 0x6666},
	{0x0106, 0x6646},
	{0x0107, 0x666a},
	{0x0108, 0x6600},
	{0x0109, 0xaaa0},
	{0x010b, 0x6600},
	{0x010c, 0x6666},
	{0x010d, 0x6666},
	{0x010e, 0x6666},
	{0x010f, 0x6666},
	{0x0110, 0x0800},
	{0x0111, 0x5800},
	{0x0112, 0x5800},
	{0x0117, 0x0001},
	{0x0118, 0x0040},
	{0x0119, 0x0040},
	{0x011a, 0x0041},
	{0x0125, 0x8120},
	{0x013a, 0x3080},
	{0x013b, 0x3080},
	{0x013c, 0x3080},
	{0x013d, 0xa490},
	{0x013e, 0xa490},
	{0x013f, 0xa490},
	{0x0145, 0x0000},
	{0x0146, 0x0000},
	{0x0147, 0x0000},
	{0x0148, 0x0000},
	{0x0149, 0x0000},
	{0x0194, 0x0000},
	{0x0198, 0x0000},
	{0x01a0, 0x00e4},
	{0x01a1, 0x00de},
	{0x01cb, 0x0000},
	{0x01cc, 0x5757},
	{0x01cd, 0x5757},
	{0x01ce, 0x5757},
	{0x01cf, 0x5757},
	{0x01d0, 0x5757},
	{0x01d1, 0x5757},
	{0x01d2, 0x5757},
	{0x01d3, 0x5757},
	{0x01d4, 0x5757},
	{0x01d5, 0x5757},
	{0x01db, 0x0000},
	{0x01dd, 0x1e00},
	{0x01f0, 0x8007},
	{0x01f1, 0x0000},
	{0x0200, 0x0101},
	{0x0201, 0x0101},
	{0x0202, 0x0001},
	{0x0203, 0x1101},
	{0x0205, 0x0000},
	{0x0206, 0x0101},
	{0x0207, 0x0004},
	{0x0208, 0x0000},
	{0x0209, 0x2000},
	{0x020a, 0x0000},
	{0x020b, 0x0000},
	{0x020c, 0x0000},
	{0x020d, 0x0000},
	{0x020e, 0x0001},
	{0x020f, 0x0000},
	{0x0210, 0x0001},
	{0x0211, 0x0001},
	{0x0212, 0x0003},
	{0x0213, 0x0000},
	{0x0214, 0x0000},
	{0x0215, 0x0000},
	{0x0216, 0x0000},
	{0x0217, 0x0002},
	{0x0218, 0x0001},
	{0x0219, 0x0000},
	{0x02fc, 0x0000},
	{0x02fd, 0x0000},
	{0x02fe, 0x0000},
	{0x02ff, 0x0000},
	{0x0300, 0x5757},
	{0x0301, 0x0039},
	{0x0400, 0x0000},
	{0x0401, 0x01fe},
	{0x0402, 0x0000},
	{0x0403, 0x0000},
	{0x0404, 0x0000},
	{0x0405, 0x0000},
	{0x0406, 0x0000},
	{0x0407, 0x8000},
	{0x0500, 0x0000},
	{0x0600, 0x0000},
	{0x0700, 0x0000},
	{0x0701, 0x0000},
	{0x0702, 0x0000},
	{0x0703, 0x0000},
	{0x0704, 0x00c0},
	{0x0705, 0x00a8},
	{0x0706, 0x0031},
	{0x0707, 0x0031},
	{0x0708, 0x0031},
	{0x0709, 0x0031},
	{0x070a, 0x0031},
	{0x070b, 0x0031},
	{0x070c, 0x0000},
	{0x070d, 0x0000},
	{0x070e, 0x0040},
	{0x070f, 0x00aa},
	{0x0710, 0x0000},
	{0x0711, 0x0031},
	{0x0712, 0x0031},
	{0x0713, 0x0031},
	{0x0714, 0x0031},
	{0x0715, 0x0031},
	{0x0716, 0x0031},
	{0x0717, 0x0031},
	{0x0718, 0x0031},
	{0x0719, 0x0000},
	{0x071a, 0x00ff},
	{0x071b, 0x00ff},
	{0x0b00, 0x0000},
	{0x0c00, 0x001c},
	{0x0d00, 0x0001},
	{0x0d01, 0x000a},
	{0x0d02, 0x0000},
	{0x0d03, 0x002f},
	{0x0d04, 0x002f},
	{0x0d05, 0x00f3},
	{0x0d06, 0x0000},
	{0x0d07, 0x0023},
	{0x0d08, 0x0000},
	{0x0e00, 0x0001},
	{0x0e01, 0x000a},
	{0x0e02, 0x0000},
	{0x0e03, 0x002f},
	{0x0e04, 0x002f},
	{0x0e05, 0x00f3},
	{0x0e06, 0x0000},
	{0x0e07, 0x0023},
	{0x0e08, 0x0000},
	{0x0f00, 0x0000},
	{0x0f01, 0x0000},
	{0x0f02, 0x0000},
	{0x0f03, 0x0000},
	{0x0f04, 0x0000},
	{0x0f05, 0x0000},
	{0x0f06, 0x0000},
	{0x0f07, 0x0000},
	{0x0f08, 0x0000},
	{0x0f09, 0x0000},
	{0x0f0a, 0x0000},
	{0x0f0b, 0x0000},
	{0x0f0c, 0x0000},
	{0x0f11, 0x0000},
	{0x0f12, 0x0000},
	{0x0f13, 0x0000},
	{0x0f14, 0x0000},
	{0x0f15, 0x0000},
	{0x0f16, 0x0000},
	{0x0f17, 0x0000},
	{0x0f18, 0x0000},
	{0x0f19, 0x0000},
	{0x0f1a, 0x0000},
	{0x0f1b, 0x0000},
	{0x0f1c, 0x0000},
	{0x1000, 0x0000},
	{0x1010, 0x8000},
	{0x1011, 0x8000},
	{0x1020, 0x0200},
	{0x1021, 0x0000},
	{0x1022, 0x0000},
	{0x1023, 0x0000},
	{0x1024, 0x0000},
	{0x1025, 0x0000},
	{0x1026, 0x0000},
	{0x1027, 0x0000},
	{0x1028, 0x0000},
	{0x1029, 0x0000},
	{0x1030, 0x0200},
	{0x1031, 0x0000},
	{0x1032, 0x0000},
	{0x1033, 0x0000},
	{0x1034, 0x0000},
	{0x1035, 0x0000},
	{0x1036, 0x0000},
	{0x1037, 0x0000},
	{0x1038, 0x0000},
	{0x1039, 0x0000},
	{0x1100, 0x00a6},
	{0x1101, 0x04c3},
	{0x1102, 0x27c8},
	{0x1103, 0xbf50},
	{0x1104, 0x0045},
	{0x1105, 0x0007},
	{0x1106, 0x7418},
	{0x1107, 0x0401},
	{0x1108, 0x0000},
	{0x1109, 0x0010},
	{0x110a, 0x1010},
	{0x1200, 0x0000},
	{0x1201, 0x0000},
	{0x1202, 0x0000},
	{0x1210, 0x8000},
	{0x1211, 0x8000},
	{0x1212, 0x8000},
	{0x1213, 0x8000},
	{0x1214, 0x8000},
	{0x1215, 0x8000},
	{0x1216, 0x8000},
	{0x1217, 0x8000},
	{0x1218, 0x8000},
	{0x1219, 0x8000},
	{0x1220, 0x0000},
	{0x1221, 0x0000},
	{0x1222, 0x0000},
	{0x1223, 0x0000},
	{0x1224, 0x0000},
	{0x1225, 0x0000},
	{0x1226, 0x0000},
	{0x1227, 0x0000},
	{0x1230, 0x0000},
	{0x1231, 0x0000},
	{0x1232, 0x0000},
	{0x1233, 0x0000},
	{0x1234, 0x0000},
	{0x1235, 0x0000},
	{0x1240, 0x0000},
	{0x1241, 0x0000},
	{0x1242, 0x0000},
	{0x1243, 0x0000},
	{0x1244, 0x0000},
	{0x1245, 0x0000},
	{0x1250, 0x0000},
	{0x1251, 0x0000},
	{0x1252, 0x0000},
	{0x1253, 0x0000},
	{0x1254, 0x0000},
	{0x1255, 0x0000},
	{0x1260, 0x0800},
	{0x1261, 0x0800},
	{0x1262, 0x0800},
	{0x1263, 0x0800},
	{0x1300, 0x3001},
	{0x1302, 0x5981},
	{0x1304, 0x0fff},
	{0x1306, 0x0000},
	{0x1308, 0x0fff},
	{0x130a, 0x0fff},
	{0x130c, 0x0040},
	{0x130e, 0x0006},
	{0x1320, 0x0b68},
	{0x1322, 0x366b},
	{0x1324, 0x1415},
	{0x1326, 0x00a9},
	{0x1328, 0x0fff},
	{0x132a, 0x0fff},
	{0x132c, 0x0000},
	{0x132e, 0x0000},
	{0x1400, 0x0002},
	{0x1700, 0x4000},
	{0x1701, 0x0000},
	{0x1702, 0x000f},
	{0x1703, 0x5000},
	{0x1800, 0x0025},
	{0x1801, 0x4000},
	{0x1802, 0x0000},
	{0x1803, 0x00ff},
	{0x1900, 0x3018},
	{0x1a00, 0x3018},
	{0x1b00, 0x6000},
	{0x1b01, 0x007f},
	{0x1b02, 0x007f},
	{0x1b03, 0x0000},
	{0x1b04, 0x0200},
	{0x1b05, 0x007f},
	{0x1b06, 0xffff},
	{0x1b07, 0x0000},
	{0x2000, 0x0200},
	{0x2001, 0x0000},
	{0x2002, 0x0000},
	{0x2400, 0x0038},
	{0x2b00, 0x0000},
	{0x2b01, 0x0000},
	{0x2b02, 0x1ad0},
	{0x2b03, 0x0004},
	{0x2b04, 0x4000},
	{0x2b05, 0x0000},
	{0x2b06, 0x07ff},
	{0x2b07, 0x0000},
	{0x2b08, 0x0000},
	{0x2b09, 0x000a},
	{0x2b0a, 0x0004},
	{0x2b0b, 0x0004},
	{0x2b0c, 0x0004},
	{0x2b0d, 0x0004},
	{0x2b0e, 0x0004},
	{0x2b10, 0x0000},
	{0x2b11, 0x000a},
	{0x2b12, 0x000a},
	{0x2b13, 0x000a},
	{0x2b14, 0x0000},
	{0x2b15, 0x0000},
	{0x2b16, 0x0000},
	{0x2b17, 0x0000},
	{0x2c00, 0x2000},
	{0x2c01, 0x0000},
	{0x2c02, 0x0000},
	{0x2c03, 0x0000},
	{0x2c04, 0x0017},
	{0x2c05, 0x004b},
	{0x2c06, 0x03e8},
	{0x2c07, 0x0000},
	{0x2c08, 0x0000},
	{0x2c09, 0x0400},
	{0x2c0a, 0xb5b6},
	{0x2c0b, 0x9124},
	{0x2c0c, 0x4924},
	{0x2c0d, 0x0009},
	{0x2c0e, 0x0018},
	{0x2c0f, 0x002a},
	{0x2c10, 0x004c},
	{0x2c11, 0x0097},
	{0x2c12, 0x01c3},
	{0x2c13, 0x03e9},
	{0x2c14, 0x1389},
	{0x2c15, 0xc351},
	{0x2c16, 0x02a0},
	{0x2c17, 0x0b0f},
	{0x2c18, 0x0000},
	{0x2c19, 0x0000},
	{0x2c1a, 0x0000},
	{0x2c1b, 0x0000},
	{0x2c1c, 0x0000},
	{0x2c1d, 0x0000},
	{0x2c1e, 0x0000},
	{0x2d00, 0x40af},
	{0x2d01, 0x0702},
	{0x2d02, 0x0000},
	{0x2f00, 0x000b},
	{0x3000, 0x4650},
	{0x3001, 0x0080},
	{0x3002, 0x0080},
	{0x3003, 0x0800},
	{0x3004, 0x0000},
	{0x3005, 0x0000},
	{0x3006, 0x1111},
	{0x3007, 0x0001},
	{0x3008, 0x0000},
	{0x300a, 0x0300},
	{0x3100, 0x4ec0},
	{0x3101, 0x0080},
	{0x3102, 0x0080},
	{0x3103, 0x0800},
	{0x3104, 0x0000},
	{0x3105, 0x0000},
	{0x3106, 0x0000},
	{0x3107, 0x000f},
	{0x3108, 0x000f},
	{0x3109, 0x0001},
	{0x3200, 0x0200},
	{0x3201, 0x2995},
	{0x3202, 0x0000},
	{0x3203, 0x07ff},
	{0x3204, 0x0004},
	{0x3205, 0x0004},
	{0x3206, 0xca0a},
	{0x3207, 0x0004},
	{0x3208, 0x000a},
	{0x3209, 0x8000},
	{0x320a, 0x0007},
	{0x320b, 0x0002},
	{0x320c, 0x0000},
	{0x3300, 0x0084},
	{0x3301, 0x00a2},
	{0x3302, 0x0073},
	{0x3303, 0x00a0},
	{0x3304, 0x0002},
	{0x3308, 0x0004},
	{0x3309, 0x0000},
	{0x330a, 0x0002},
	{0x330b, 0x004c},
	{0x330c, 0x0001},
	{0x330d, 0x0002},
	{0x330e, 0x0000},
	{0x330f, 0x00a3},
	{0x3310, 0x0000},
	{0x3311, 0x0048},
	{0x3312, 0x00f0},
	{0x3313, 0x0000},
	{0x3314, 0x00c0},
	{0x3315, 0x000a},
	{0x3316, 0x07ff},
	{0x3317, 0x0000},
	{0x3400, 0x0200},
	{0x3404, 0x0000},
	{0x3405, 0x0000},
	{0x3406, 0x0000},
	{0x3407, 0x0000},
	{0x3408, 0x0000},
	{0x3409, 0x0000},
	{0x340a, 0x0000},
	{0x340b, 0x0000},
	{0x340c, 0x0000},
	{0x340d, 0x0000},
	{0x340e, 0x0000},
	{0x340f, 0x0000},
	{0x3410, 0x0000},
	{0x3411, 0x0000},
	{0x3412, 0x0000},
	{0x3413, 0x0000},
	{0x3414, 0x0000},
	{0x3415, 0x0000},
	{0x3424, 0x0000},
	{0x3425, 0x0000},
	{0x3426, 0x0000},
	{0x3427, 0x0000},
	{0x3428, 0x0000},
	{0x3429, 0x0000},
	{0x342a, 0x0000},
	{0x342b, 0x0000},
	{0x342c, 0x0000},
	{0x342d, 0x0000},
	{0x342e, 0x0000},
	{0x342f, 0x0000},
	{0x3430, 0x0000},
	{0x3431, 0x0000},
	{0x3432, 0x0000},
	{0x3433, 0x0000},
	{0x3434, 0x0000},
	{0x3435, 0x0000},
	{0x3440, 0x6319},
	{0x3441, 0x3771},
	{0x3800, 0x3d00},
	{0x3801, 0x0804},
	{0x3802, 0xc100},
	{0x3803, 0x0000},
	{0x3804, 0x0000},
	{0x3805, 0x0000},
	{0x3806, 0x0000},
	{0x3807, 0x0000},
	{0x3808, 0x0000},
	{0x3809, 0x0000},
	{0x380a, 0x0000},
	{0x380b, 0x0000},
	{0x380c, 0x0000},
	{0x380d, 0x0000},
	{0x380e, 0x0000},
	{0x3810, 0x0000},
	{0x3811, 0x0000},
	{0x3812, 0x0000},
	{0x3813, 0x0000},
	{0x3814, 0x0000},
	{0x3815, 0x0000},
	{0x3816, 0x0000},
	{0x3817, 0x0000},
	{0x3818, 0x0000},
	{0x3820, 0x7406},
	{0x3821, 0x4103},
	{0x3822, 0x8000},
	{0x3824, 0x0000},
	{0x3825, 0x0000},
	{0x3827, 0x0000},
	{0x3828, 0x0000},
	{0x382b, 0x0000},
	{0x382c, 0x0000},
	{0x3b00, 0x3020},
	{0x3b01, 0x3300},
	{0x3b02, 0x2200},
	{0x3b03, 0x0100},
	{0x3c00, 0x0000},
	{0x3c01, 0x3300},
	{0x3c02, 0x2200},

};

static struct reg_sequence rt5691_init_list[] = {
	{RT5691_DEPOP_CTRL_1,			0xf000},
	{RT5691_ANLG_BIAS_CTRL_2,		0x3480},
	{RT5691_DACL_CTRL_1,			0x1080},
	{RT5691_DACR_CTRL_2,			0x1080},
	{RT5691_DACM_CTRL_3,			0x1080},
	{RT5691_PWR_DIG_2,			0x2008},
	{RT5691_STO1_DAC_MIXER_CTRL,		0xa8a8},
	{RT5691_AD_DA_MIXER_CTRL,		0x8083},
	{RT5691_HP_BEHAVIOR_LOGIC_CTRL_2,	0x0002},
	{RT5691_MONO_OUTPUT_CTRL,		0x0010},
	{RT5691_STO1_ADC_MIXER_CTRL,		0xf0f0},
	{RT5691_STO2_ADC_MIXER_CTRL,		0xf0f0},
	{RT5691_COMBO_JACK_CTRL_4,		0x0104},
	{RT5691_COMBO_WATER_CTRL_3,		0x0f5f},
	{RT5691_COMBO_WATER_CTRL_4,		0x004a},
	{RT5691_COMBO_WATER_CTRL_5,		0x07c0},
	{RT5691_COMBO_JACK_CTRL_2,		0x0010},
	{RT5691_COMBO_JACK_CTRL_3,		0x18e2},
	{RT5691_STO_DRE_CTRL_2,			0x0041},
	{RT5691_STO_DRE_CTRL_3,			0x040c},
	{RT5691_ADC_FILTER_CTRL_3,		0x0090},
	{RT5691_ADC_FILTER2_CTRL_3,		0x0090},
	{RT5691_HPOUT_CP_CTRL_1,		0x5018},
	{RT5691_GPIO_CLK,			0x8000},
	{RT5691_ADC_FILTER_CTRL_7,		0x0001},
	{RT5691_ADC_FILTER_CTRL_9,		0x0001},
};

static bool rt5691_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RT5691_RESET:
	case RT5691_INT_ST_1:
	case RT5691_VENDOR_ID_2:
	case RT5691_VENDOR_ID_1:
	case RT5691_DEVICE_ID:
	case RT5691_ANLG_READ_STA_324:
	case RT5691_GPIO_ST_1:
	case RT5691_CLK_SRC_SW_TEST:
	case RT5691_MIC_BTN_CTRL_1:
	case RT5691_MIC_BTN_CTRL_2:
	case RT5691_MIC_BTN_CTRL_13:
	case RT5691_MIC_BTN_CTRL_14:
	case RT5691_MIC_BTN_CTRL_27:
	case RT5691_MIC_BTN_CTRL_28:
	case RT5691_DMIC_FLOATING_DET_CTRL_1:
	case RT5691_HARM_COMP_OP_1:
	case RT5691_DAC_BI_FILTER_CTRL_1:
	case RT5691_ALC_PGA_TOP_9:
	case RT5691_ALC_PGA_TOP_10:
	case RT5691_ALC_PGA_TOP_11:
	case RT5691_DAC_EQ_CTRL_3:
	case RT5691_ALC_CTRL_13:
	case RT5691_ALC_CTRL_14:
	case RT5691_ALC_CTRL_15:
	case RT5691_ALC_CTRL_16:
	case RT5691_SIL_DET_TOP_8:
	case RT5691_COMBO_JACK_CTRL_4:
	case RT5691_COMBO_JACK_CTRL_7:
	case RT5691_COMBO_JACK_CTRL_8:
	case RT5691_COMBO_JACK_CTRL_9:
	case RT5691_IMP_SENS_CTRL_1:
	case RT5691_IMP_SENS_CTRL_8:
	case RT5691_IMP_SENS_CTRL_9:
	case RT5691_IMP_SENS_CTRL_25:
	case RT5691_IMP_SENS_CTRL_26:
	case RT5691_IMP_SENS_CTRL_27:
	case RT5691_IMP_SENS_CTRL_28:
	case RT5691_IMP_SENS_CTRL_29:
	case RT5691_IMP_SENS_CTRL_30:
	case RT5691_IMP_SENS_CTRL_31:
	case RT5691_MONO_DRE_CTRL_5:
	case RT5691_MONO_DRE_CTRL_6:
	case RT5691_STO_DRE_CTRL_5:
	case RT5691_STO_DRE_CTRL_6:
	case RT5691_STO_DRE_CTRL_7:
	case RT5691_WATER_DET_CTRL_3:
	case RT5691_WATER_DET_CTRL_4:
	case RT5691_SAR_ADC_DET_CTRL_2:
	case RT5691_SAR_ADC_DET_CTRL_3:
	case RT5691_SAR_ADC_DET_CTRL_9:
	case RT5691_SAR_ADC_DET_CTRL_10:
	case RT5691_SAR_ADC_DET_CTRL_19:
	case RT5691_SAR_ADC_DET_CTRL_23:
	case RT5691_SAR_ADC_DET_CTRL_24:
	case RT5691_EFUSE_WRITE_1:
	case RT5691_EFUSE_READ_1:
	case RT5691_EFUSE_READ_2:
	case RT5691_EFUSE_READ_3:
	case RT5691_EFUSE_READ_4:
	case RT5691_EFUSE_READ_5:
	case RT5691_EFUSE_READ_6:
	case RT5691_EFUSE_READ_7:
	case RT5691_EFUSE_READ_8:
	case RT5691_EFUSE_READ_9:
	case RT5691_EFUSE_READ_10:
	case RT5691_EFUSE_READ_11:
	case RT5691_EFUSE_READ_12:
	case RT5691_EFUSE_READ_13:
	case RT5691_EFUSE_READ_14:
	case RT5691_EFUSE_READ_15:
	case RT5691_EFUSE_READ_16:
	case RT5691_EFUSE_READ_17:
	case RT5691_EFUSE_READ_18:
	case RT5691_HP_AMP_DET_CTRL_9:
	case RT5691_HP_AMP_DET_CTRL_12:
	case RT5691_HP_AMP_DET_CTRL_13:
	case RT5691_OFFSET_CAL_TOP_9:
	case RT5691_OFFSET_CAL_TOP_10:
	case RT5691_OFFSET_CAL_TOP_11:
	case RT5691_OFFSET_CAL_TOP_12:
	case RT5691_OFFSET_CAL_TOP_13:
	case RT5691_OFFSET_CAL_TOP_14:
	case RT5691_OFFSET_CAL_TOP_15:
	case RT5691_OFFSET_CAL_21:
	case RT5691_OFFSET_CAL_22:
	case RT5691_OFFSET_CAL_30:
	case RT5691_OFFSET_CAL_31:
	case RT5691_OFFSET_CAL_32:
	case RT5691_OFFSET_CAL_33:
		return true;
	default:
		return false;
	}
}

static bool rt5691_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RT5691_RESET:
	case RT5691_HP_AMP_CTRL_2:
	case RT5691_MONO_OUTPUT_CTRL:
	case RT5691_HP_AMP_L_GAIN_CTRL:
	case RT5691_HP_AMP_R_GAIN_CTRL:
	case RT5691_IN1_IN2_CTRL:
	case RT5691_IN3_CTRL:
	case RT5691_ZCD_CTRL:
	case RT5691_JACK_TYPE_DET_CTRL_2:
	case RT5691_ST_MUX_CTRL:
	case RT5691_STO1_ADC_MIXER_CTRL:
	case RT5691_STO2_ADC_MIXER_CTRL:
	case RT5691_AD_DA_MIXER_CTRL:
	case RT5691_STO1_DAC_MIXER_CTRL:
	case RT5691_MONO_DAC_VOL_CTRL:
	case RT5691_FIFO_CTRL:
	case RT5691_PDM_OUTPUT_CTRL:
	case RT5691_HP_ANLG_OFFSET_CTRL_1:
	case RT5691_HP_ANLG_OFFSET_CTRL_2:
	case RT5691_HP_ANLG_OFFSET_CTRL_3:
	case RT5691_MONO_ANLG_OFFSET_CTRL_1:
	case RT5691_MONO_ANLG_OFFSET_CTRL_2:
	case RT5691_DMIC_CTRL:
	case RT5691_PWR_DIG_1:
	case RT5691_PWR_DIG_2:
	case RT5691_PWR_ANLG_1:
	case RT5691_PWR_ANLG_2:
	case RT5691_JD2_CTRL_1:
	case RT5691_BGLDO33_CTRL_1:
	case RT5691_BGLDO33_CTRL_2:
	case RT5691_MCLK_DET_CTRL_1:
	case RT5691_MCLK_DET_CTRL_2:
	case RT5691_ANLG_LDO_CTRL_1:
	case RT5691_PLLA_CTRL_1:
	case RT5691_PLLA_CTRL_2:
	case RT5691_PLLA_CTRL_3:
	case RT5691_PLLA_CTRL_4:
	case RT5691_PLLB_CTRL_1:
	case RT5691_PLLB_CTRL_2:
	case RT5691_PLLB_CTRL_3:
	case RT5691_PLLB_CTRL_4:
	case RT5691_DEPOP_CTRL_1:
	case RT5691_ANLG_BIAS_CTRL_1:
	case RT5691_ANLG_BIAS_CTRL_2:
	case RT5691_HPOUT_CP_CTRL_1:
	case RT5691_HPOUT_CP_CTRL_2:
	case RT5691_OSC_CTRL_1:
	case RT5691_HP_CAL_CTRL_2:
	case RT5691_RC_CLK_CTRL:
	case RT5691_IRQ_CTRL_1:
	case RT5691_IRQ_CTRL_2:
	case RT5691_IRQ_CTRL_3:
	case RT5691_IRQ_CTRL_4:
	case RT5691_IRQ_CTRL_5:
	case RT5691_IRQ_CTRL_6:
	case RT5691_INT_ST_1:
	case RT5691_HP_AMP_DET_CTRL_1:
	case RT5691_HP_AMP_DET_CTRL_2:
	case RT5691_HP_AMP_DET_CTRL_3:
	case RT5691_HP_AMP_DET_CTRL_4:
	case RT5691_HP_AMP_DET_CTRL_5:
	case RT5691_HP_AMP_DET_CTRL_6:
	case RT5691_HP_AMP_DET_CTRL_7:
	case RT5691_JACK_DETECT_CTRL_1:
	case RT5691_DIG_MISC_CTRL:
	case RT5691_DUMMY_REGISTER_2:
	case RT5691_DUMMY_REGISTER_3:
	case RT5691_VENDOR_ID_2:
	case RT5691_VENDOR_ID_1:
	case RT5691_DEVICE_ID:
	case RT5691_ANLG_BST1_CTRL_1:
	case RT5691_ANLG_BST2_CTRL_2:
	case RT5691_ANLG_BST3_CTRL_3:
	case RT5691_ANLG_BIAS_CTRL_3:
	case RT5691_ANLG_BIAS_CTRL_4:
	case RT5691_ANLG_BIAS_CTRL_5:
	case RT5691_ANLG_BIAS_CTRL_6:
	case RT5691_ANLG_BIAS_CTRL_7:
	case RT5691_ANLG_BIAS_CTRL_8:
	case RT5691_ANLG_BIAS_CTRL_9:
	case RT5691_ANLG_BIAS_CTRL_10:
	case RT5691_ANLG_BIAS_CTRL_11:
	case RT5691_ANLG_BIAS_CTRL_12:
	case RT5691_DACREF_CTRL_1:
	case RT5691_CALL_BUF_MIX_CTRL_1:
	case RT5691_CALR_BUF_MIX_CTRL_2:
	case RT5691_MONO_ANLG_DRE_CTRL_1:
	case RT5691_MONO_ANLG_DRE_CTRL_2:
	case RT5691_MONO_ANLG_DRE_CTRL_3:
	case RT5691_MONO_ANLG_DRE_CTRL_4:
	case RT5691_ANLG_BIAS_CTRL_13:
	case RT5691_DACL_CTRL_1:
	case RT5691_DACR_CTRL_2:
	case RT5691_DACM_CTRL_3:
	case RT5691_ADC12_CTRL:
	case RT5691_ADC34_CTRL:
	case RT5691_ADC56_CTRL:
	case RT5691_TEST_MODE_CTRL:
	case RT5691_GPIO_TEST_MODE3:
	case RT5691_GPIO_TEST_MODE4:
	case RT5691_GPIO_TEST_MODE5:
	case RT5691_GPIO_TEST_MODE6:
	case RT5691_SIL_DET_CTRL8:
	case RT5691_SIL_DET_CTRL9:
	case RT5691_LPF_AD10:
	case RT5691_LPF_DMIC11:
	case RT5691_HP_IMP_SEN_DIG_CTRL:
	case RT5691_HP_IMP_SEN_DIG_CTRL_1:
	case RT5691_HP_IMP_SEN_DIG_CTRL_2:
	case RT5691_HP_IMP_SEN_DIG_CTRL_3:
	case RT5691_HP_IMP_SEN_DIG_CTRL_4:
	case RT5691_HP_IMP_SEN_DIG_CTRL_5:
	case RT5691_HP_IMP_SEN_DIG_CTRL_6:
	case RT5691_HP_IMP_SEN_DIG_CTRL_7:
	case RT5691_HP_IMP_SEN_DIG_CTRL_8:
	case RT5691_HP_IMP_SEN_DIG_CTRL_9:
	case RT5691_HP_IMP_SEN_DIG_CTRL_10:
	case RT5691_HP_BEHAVIOR_LOGIC_CTRL_2:
	case RT5691_ANLG_READ_STA_324:
	case RT5691_I2C_SLAVE_CTRL25:
	case RT5691_I2C_SLAVE_CTRL26:
	case RT5691_DA_STO1_FILTER_CLK_DIV:
	case RT5691_AD_STO1_FILTER_CLK_DIV:
	case RT5691_CLK_SEL_RX_FIFO:
	case RT5691_CLK_SEL_STO_DAC_PDM_FIFO:
	case RT5691_SYS_CLK_SRC:
	case RT5691_ADC_AND_DAC_OSR:
	case RT5691_I2S_MASTER_CLK:
	case RT5691_MCLK_DET_CTRL_3:
	case RT5691_PWR_DA_PATH_1:
	case RT5691_PWR_DA_PATH_2:
	case RT5691_PWR_AD_PATH:
	case RT5691_AD_DA_ASRC:
	case RT5691_PLL_CTRL_2:
	case RT5691_I2S:
	case RT5691_TRACKING_CTRL:
	case RT5691_CLK_DIV_PDM:
	case RT5691_CLK_DIV_DMIC:
	case RT5691_EFUSE_CTRL:
	case RT5691_EN_SW_CTRL:
	case RT5691_GLITCH_FREE_MUX_CTRL:
	case RT5691_GPIO_CLK:
	case RT5691_RC_CLK:
	case RT5691_FRAC_I2S1_MASTER_CTRL_1:
	case RT5691_FRAC_I2S1_MASTER_CTRL_2:
	case RT5691_FRAC_I2S1_MASTER_CTRL_3:
	case RT5691_AD_CLK_GATING_FUNC_1:
	case RT5691_AD_CLK_GATING_FUNC_2:
	case RT5691_DA_CLK_GATING_FUNC_1:
	case RT5691_DA_CLK_GATING_FUNC_2:
	case RT5691_HP_IMP_FSOV_GAIN_CTRL_1:
	case RT5691_HP_IMP_FSOV_GAIN_CTRL_2:
	case RT5691_MULTI_FUNC_PIN_CTRL_1:
	case RT5691_GPIO_CTRL_1:
	case RT5691_GPIO_CTRL_3:
	case RT5691_GPIO_ST_1:
	case RT5691_GPIO_PULL_CTRL_1:
	case RT5691_PAD_DRIVING_CTRL_1:
	case RT5691_PAD_DRIVING_CTRL_5:
	case RT5691_PAD_DRIVING_CTRL_6:
	case RT5691_SCAN_CTRL_1:
	case RT5691_CLK_SRC_SW_TEST:
	case RT5691_MIC_BTN_CTRL_1:
	case RT5691_MIC_BTN_CTRL_2:
	case RT5691_MIC_BTN_CTRL_3:
	case RT5691_MIC_BTN_CTRL_4:
	case RT5691_MIC_BTN_CTRL_5:
	case RT5691_MIC_BTN_CTRL_6:
	case RT5691_MIC_BTN_CTRL_7:
	case RT5691_MIC_BTN_CTRL_8:
	case RT5691_MIC_BTN_CTRL_9:
	case RT5691_MIC_BTN_CTRL_10:
	case RT5691_MIC_BTN_CTRL_11:
	case RT5691_MIC_BTN_CTRL_12:
	case RT5691_MIC_BTN_CTRL_13:
	case RT5691_MIC_BTN_CTRL_14:
	case RT5691_MIC_BTN_CTRL_15:
	case RT5691_MIC_BTN_CTRL_16:
	case RT5691_MIC_BTN_CTRL_17:
	case RT5691_MIC_BTN_CTRL_18:
	case RT5691_MIC_BTN_CTRL_19:
	case RT5691_MIC_BTN_CTRL_20:
	case RT5691_MIC_BTN_CTRL_21:
	case RT5691_MIC_BTN_CTRL_22:
	case RT5691_MIC_BTN_CTRL_23:
	case RT5691_MIC_BTN_CTRL_24:
	case RT5691_MIC_BTN_CTRL_25:
	case RT5691_MIC_BTN_CTRL_26:
	case RT5691_MIC_BTN_CTRL_27:
	case RT5691_MIC_BTN_CTRL_28:
	case RT5691_DMIC_FLOATING_DET_CTRL_1:
	case RT5691_PDM_CTRL_1:
	case RT5691_ADC_FILTER_CTRL_1:
	case RT5691_ADC_FILTER_CTRL_2:
	case RT5691_ADC_FILTER_CTRL_3:
	case RT5691_ADC_FILTER_CTRL_4:
	case RT5691_ADC_FILTER_CTRL_5:
	case RT5691_ADC_FILTER_CTRL_6:
	case RT5691_ADC_FILTER_CTRL_7:
	case RT5691_ADC_FILTER_CTRL_8:
	case RT5691_ADC_FILTER_CTRL_9:
	case RT5691_ADC_FILTER2_CTRL_1:
	case RT5691_ADC_FILTER2_CTRL_2:
	case RT5691_ADC_FILTER2_CTRL_3:
	case RT5691_ADC_FILTER2_CTRL_4:
	case RT5691_ADC_FILTER2_CTRL_5:
	case RT5691_ADC_FILTER2_CTRL_6:
	case RT5691_ADC_FILTER2_CTRL_7:
	case RT5691_ADC_FILTER2_CTRL_8:
	case RT5691_ADC_FILTER2_CTRL_9:
	case RT5691_HARM_COMP_OP_1:
	case RT5691_HARM_COMP_OP_2:
	case RT5691_HARM_COMP_OP_3:
	case RT5691_HARM_COMP_OP_4:
	case RT5691_HARM_COMP_OP_5:
	case RT5691_HARM_COMP_OP_6:
	case RT5691_HARM_COMP_OP_7:
	case RT5691_HARM_COMP_OP_8:
	case RT5691_HARM_COMP_OP_9:
	case RT5691_HARM_COMP_OP_10:
	case RT5691_HARM_COMP_OP_11:
	case RT5691_HARM_COMP_OP_12:
	case RT5691_HARM_COMP_OP_13:
	case RT5691_HARM_COMP_OP_14:
	case RT5691_HARM_COMP_OP_15:
	case RT5691_HARM_COMP_OP_16:
	case RT5691_HARM_COMP_OP_17:
	case RT5691_HARM_COMP_OP_18:
	case RT5691_HARM_COMP_OP_19:
	case RT5691_HARM_COMP_OP_20:
	case RT5691_HARM_COMP_OP_21:
	case RT5691_HARM_COMP_OP_22:
	case RT5691_HARM_COMP_OP_23:
	case RT5691_HARM_COMP_OP_24:
	case RT5691_HARM_COMP_OP_25:
	case RT5691_DAC_BI_FILTER_CTRL_1:
	case RT5691_DAC_BI_FILTER_CTRL_2:
	case RT5691_DAC_BI_FILTER_CTRL_3:
	case RT5691_DAC_BI_FILTER_CTRL_4:
	case RT5691_DAC_BI_FILTER_CTRL_5:
	case RT5691_DAC_BI_FILTER_CTRL_6:
	case RT5691_DAC_BI_FILTER_CTRL_7:
	case RT5691_DAC_BI_FILTER_CTRL_8:
	case RT5691_DAC_BI_FILTER_CTRL_9:
	case RT5691_DAC_BI_FILTER_CTRL_10:
	case RT5691_DAC_BI_FILTER_CTRL_11:
	case RT5691_DAC_BI_FILTER_CTRL_12:
	case RT5691_DAC_BI_FILTER_CTRL_13:
	case RT5691_DAC_BI_FILTER_CTRL_14:
	case RT5691_DAC_BI_FILTER_CTRL_15:
	case RT5691_DAC_BI_FILTER_CTRL_16:
	case RT5691_DAC_BI_FILTER_CTRL_17:
	case RT5691_DAC_BI_FILTER_CTRL_18:
	case RT5691_DAC_BI_FILTER_CTRL_19:
	case RT5691_DAC_BI_FILTER_CTRL_20:
	case RT5691_DAC_BI_FILTER_CTRL_21:
	case RT5691_DAC_BI_FILTER_CTRL_22:
	case RT5691_DAC_BI_FILTER_CTRL_23:
	case RT5691_ALC_PGA_TOP_1:
	case RT5691_ALC_PGA_TOP_2:
	case RT5691_ALC_PGA_TOP_3:
	case RT5691_ALC_PGA_TOP_4:
	case RT5691_ALC_PGA_TOP_5:
	case RT5691_ALC_PGA_TOP_6:
	case RT5691_ALC_PGA_TOP_7:
	case RT5691_ALC_PGA_TOP_8:
	case RT5691_ALC_PGA_TOP_9:
	case RT5691_ALC_PGA_TOP_10:
	case RT5691_ALC_PGA_TOP_11:
	case RT5691_DAC_EQ_CTRL_1:
	case RT5691_DAC_EQ_CTRL_2:
	case RT5691_DAC_EQ_CTRL_3:
	case RT5691_DAC_EQ_CTRL_4:
	case RT5691_DAC_EQ_CTRL_5:
	case RT5691_DAC_EQ_CTRL_6:
	case RT5691_DAC_EQ_CTRL_7:
	case RT5691_DAC_EQ_CTRL_8:
	case RT5691_DAC_EQ_CTRL_9:
	case RT5691_DAC_EQ_CTRL_10:
	case RT5691_DAC_EQ_CTRL_11:
	case RT5691_DAC_EQ_CTRL_12:
	case RT5691_DAC_EQ_CTRL_13:
	case RT5691_DAC_EQ_CTRL_14:
	case RT5691_DAC_EQ_CTRL_15:
	case RT5691_DAC_EQ_CTRL_16:
	case RT5691_DAC_EQ_CTRL_17:
	case RT5691_DAC_EQ_CTRL_18:
	case RT5691_DAC_EQ_CTRL_19:
	case RT5691_DAC_EQ_CTRL_20:
	case RT5691_DAC_EQ_CTRL_21:
	case RT5691_DAC_EQ_CTRL_22:
	case RT5691_DAC_EQ_CTRL_23:
	case RT5691_DAC_EQ_CTRL_24:
	case RT5691_DAC_EQ_CTRL_25:
	case RT5691_DAC_EQ_CTRL_26:
	case RT5691_DAC_EQ_CTRL_27:
	case RT5691_DAC_EQ_CTRL_28:
	case RT5691_DAC_EQ_CTRL_29:
	case RT5691_DAC_EQ_CTRL_30:
	case RT5691_DAC_EQ_CTRL_31:
	case RT5691_DAC_EQ_CTRL_32:
	case RT5691_DAC_EQ_CTRL_33:
	case RT5691_DAC_EQ_CTRL_34:
	case RT5691_DAC_EQ_CTRL_35:
	case RT5691_DAC_EQ_CTRL_36:
	case RT5691_DAC_EQ_CTRL_37:
	case RT5691_DAC_EQ_CTRL_38:
	case RT5691_DAC_EQ_CTRL_39:
	case RT5691_DAC_EQ_CTRL_40:
	case RT5691_DAC_EQ_CTRL_41:
	case RT5691_DAC_EQ_CTRL_42:
	case RT5691_DAC_EQ_CTRL_43:
	case RT5691_ALC_CTRL_1:
	case RT5691_ALC_CTRL_2:
	case RT5691_ALC_CTRL_3:
	case RT5691_ALC_CTRL_4:
	case RT5691_ALC_CTRL_5:
	case RT5691_ALC_CTRL_6:
	case RT5691_ALC_CTRL_7:
	case RT5691_ALC_CTRL_8:
	case RT5691_ALC_CTRL_9:
	case RT5691_ALC_CTRL_10:
	case RT5691_ALC_CTRL_11:
	case RT5691_ALC_CTRL_12:
	case RT5691_ALC_CTRL_13:
	case RT5691_ALC_CTRL_14:
	case RT5691_ALC_CTRL_15:
	case RT5691_ALC_CTRL_16:
	case RT5691_EQ_ALC_SRC_CTRL:
	case RT5691_DA_DVOL_MONO_1:
	case RT5691_DA_DVOL_MONO_2:
	case RT5691_DA_DVOL_MONO_3:
	case RT5691_DA_DVOL_MONO_4:
	case RT5691_DA_DVOL_STO_1:
	case RT5691_DA_DVOL_STO_2:
	case RT5691_DA_DVOL_STO_3:
	case RT5691_DA_DVOL_STO_4:
	case RT5691_SIL_DET_MONO_TOP:
	case RT5691_SIL_DET_TOP:
	case RT5691_SIL_DET_TOP_1:
	case RT5691_SIL_DET_TOP_2:
	case RT5691_SIL_DET_TOP_3:
	case RT5691_SIL_DET_TOP_4:
	case RT5691_SIL_DET_TOP_5:
	case RT5691_SIL_DET_TOP_6:
	case RT5691_SIL_DET_TOP_7:
	case RT5691_SIL_DET_TOP_8:
	case RT5691_ASRCIN_TCON_1:
	case RT5691_ASRCIN_TCON_2:
	case RT5691_ASRCIN_TCON_3:
	case RT5691_I2S_CTRL_1:
	case RT5691_COMBO_JACK_CTRL_1:
	case RT5691_COMBO_JACK_CTRL_2:
	case RT5691_COMBO_JACK_CTRL_3:
	case RT5691_COMBO_JACK_CTRL_4:
	case RT5691_COMBO_JACK_CTRL_5:
	case RT5691_COMBO_JACK_CTRL_6:
	case RT5691_COMBO_JACK_CTRL_7:
	case RT5691_COMBO_JACK_CTRL_8:
	case RT5691_COMBO_JACK_CTRL_9:
	case RT5691_COMBO_JACK_CTRL_10:
	case RT5691_COMBO_JACK_CTRL_11:
	case RT5691_COMBO_JACK_CTRL_12:
	case RT5691_COMBO_JACK_CTRL_13:
	case RT5691_COMBO_JACK_CTRL_14:
	case RT5691_COMBO_JACK_CTRL_15:
	case RT5691_COMBO_JACK_CTRL_16:
	case RT5691_COMBO_JACK_CTRL_17:
	case RT5691_COMBO_JACK_CTRL_18:
	case RT5691_COMBO_JACK_CTRL_19:
	case RT5691_COMBO_JACK_CTRL_20:
	case RT5691_COMBO_JACK_CTRL_21:
	case RT5691_COMBO_JACK_CTRL_22:
	case RT5691_COMBO_JACK_CTRL_23:
	case RT5691_IMP_SENS_CTRL_1:
	case RT5691_IMP_SENS_CTRL_2:
	case RT5691_IMP_SENS_CTRL_3:
	case RT5691_IMP_SENS_CTRL_4:
	case RT5691_IMP_SENS_CTRL_5:
	case RT5691_IMP_SENS_CTRL_6:
	case RT5691_IMP_SENS_CTRL_7:
	case RT5691_IMP_SENS_CTRL_8:
	case RT5691_IMP_SENS_CTRL_9:
	case RT5691_IMP_SENS_CTRL_10:
	case RT5691_IMP_SENS_CTRL_11:
	case RT5691_IMP_SENS_CTRL_12:
	case RT5691_IMP_SENS_CTRL_13:
	case RT5691_IMP_SENS_CTRL_14:
	case RT5691_IMP_SENS_CTRL_15:
	case RT5691_IMP_SENS_CTRL_16:
	case RT5691_IMP_SENS_CTRL_17:
	case RT5691_IMP_SENS_CTRL_18:
	case RT5691_IMP_SENS_CTRL_19:
	case RT5691_IMP_SENS_CTRL_20:
	case RT5691_IMP_SENS_CTRL_21:
	case RT5691_IMP_SENS_CTRL_22:
	case RT5691_IMP_SENS_CTRL_23:
	case RT5691_IMP_SENS_CTRL_24:
	case RT5691_IMP_SENS_CTRL_25:
	case RT5691_IMP_SENS_CTRL_26:
	case RT5691_IMP_SENS_CTRL_27:
	case RT5691_IMP_SENS_CTRL_28:
	case RT5691_IMP_SENS_CTRL_29:
	case RT5691_IMP_SENS_CTRL_30:
	case RT5691_IMP_SENS_CTRL_31:
	case RT5691_HP_IMP_SEN_DIG_CTRL_11:
	case RT5691_HP_IMP_SEN_DIG_CTRL_12:
	case RT5691_HP_IMP_SEN_DIG_CTRL_13:
	case RT5691_COMBO_JACK_CTRL:
	case RT5691_MONO_DRE_CTRL_1:
	case RT5691_MONO_DRE_CTRL_2:
	case RT5691_MONO_DRE_CTRL_3:
	case RT5691_MONO_DRE_CTRL_4:
	case RT5691_MONO_DRE_CTRL_5:
	case RT5691_MONO_DRE_CTRL_6:
	case RT5691_MONO_DRE_CTRL_7:
	case RT5691_MONO_DRE_CTRL_8:
	case RT5691_MONO_DRE_CTRL_9:
	case RT5691_MONO_DRE_CTRL_10:
	case RT5691_STO_DRE_CTRL_1:
	case RT5691_STO_DRE_CTRL_2:
	case RT5691_STO_DRE_CTRL_3:
	case RT5691_STO_DRE_CTRL_4:
	case RT5691_STO_DRE_CTRL_5:
	case RT5691_STO_DRE_CTRL_6:
	case RT5691_STO_DRE_CTRL_7:
	case RT5691_STO_DRE_CTRL_8:
	case RT5691_STO_DRE_CTRL_9:
	case RT5691_STO_DEBOUNCE_CTRL_9:
	case RT5691_WATER_DET_CTRL_1:
	case RT5691_WATER_DET_CTRL_2:
	case RT5691_WATER_DET_CTRL_3:
	case RT5691_WATER_DET_CTRL_4:
	case RT5691_COMBO_WATER_CTRL_1:
	case RT5691_COMBO_WATER_CTRL_2:
	case RT5691_COMBO_WATER_CTRL_3:
	case RT5691_COMBO_WATER_CTRL_4:
	case RT5691_COMBO_WATER_CTRL_5:
	case RT5691_COMBO_WATER_CTRL_6:
	case RT5691_COMBO_WATER_CTRL_7:
	case RT5691_SAR_ADC_DET_CTRL_6:
	case RT5691_SAR_ADC_DET_CTRL_7:
	case RT5691_SAR_ADC_DET_CTRL_1:
	case RT5691_SAR_ADC_DET_CTRL_2:
	case RT5691_SAR_ADC_DET_CTRL_3:
	case RT5691_SAR_ADC_DET_CTRL_4:
	case RT5691_SAR_ADC_DET_CTRL_5:
	case RT5691_SAR_ADC_DET_CTRL_9:
	case RT5691_SAR_ADC_DET_CTRL_10:
	case RT5691_SAR_ADC_DET_CTRL_11:
	case RT5691_SAR_ADC_DET_CTRL_12:
	case RT5691_SAR_ADC_DET_CTRL_13:
	case RT5691_SAR_ADC_DET_CTRL_14:
	case RT5691_SAR_ADC_DET_CTRL_15:
	case RT5691_SAR_ADC_DET_CTRL_16:
	case RT5691_SAR_ADC_DET_CTRL_17:
	case RT5691_SAR_ADC_DET_CTRL_18:
	case RT5691_SAR_ADC_DET_CTRL_19:
	case RT5691_SAR_ADC_DET_CTRL_20:
	case RT5691_SAR_ADC_DET_CTRL_21:
	case RT5691_SAR_ADC_DET_CTRL_22:
	case RT5691_SAR_ADC_DET_CTRL_23:
	case RT5691_SAR_ADC_DET_CTRL_24:
	case RT5691_EFUSE_WRITE_1:
	case RT5691_EFUSE_WRITE_2:
	case RT5691_EFUSE_WRITE_3:
	case RT5691_EFUSE_WRITE_4:
	case RT5691_EFUSE_WRITE_5:
	case RT5691_EFUSE_WRITE_6:
	case RT5691_EFUSE_WRITE_7:
	case RT5691_EFUSE_WRITE_8:
	case RT5691_EFUSE_WRITE_9:
	case RT5691_EFUSE_WRITE_10:
	case RT5691_EFUSE_WRITE_11:
	case RT5691_EFUSE_WRITE_12:
	case RT5691_EFUSE_WRITE_13:
	case RT5691_EFUSE_WRITE_14:
	case RT5691_EFUSE_WRITE_15:
	case RT5691_EFUSE_WRITE_16:
	case RT5691_EFUSE_WRITE_17:
	case RT5691_EFUSE_WRITE_18:
	case RT5691_EFUSE_WRITE_19:
	case RT5691_EFUSE_READ_1:
	case RT5691_EFUSE_READ_2:
	case RT5691_EFUSE_READ_3:
	case RT5691_EFUSE_READ_4:
	case RT5691_EFUSE_READ_5:
	case RT5691_EFUSE_READ_6:
	case RT5691_EFUSE_READ_7:
	case RT5691_EFUSE_READ_8:
	case RT5691_EFUSE_READ_9:
	case RT5691_EFUSE_READ_10:
	case RT5691_EFUSE_READ_11:
	case RT5691_EFUSE_READ_12:
	case RT5691_EFUSE_READ_13:
	case RT5691_EFUSE_READ_14:
	case RT5691_EFUSE_READ_15:
	case RT5691_EFUSE_READ_16:
	case RT5691_EFUSE_READ_17:
	case RT5691_EFUSE_READ_18:
	case RT5691_EFUSE_READ_19:
	case RT5691_EFUSE_READ_20:
	case RT5691_OFFSET_CAL_1:
	case RT5691_OFFSET_CAL_2:
	case RT5691_HP_AMP_DET_CTRL_8:
	case RT5691_HP_AMP_DET_CTRL_9:
	case RT5691_HP_AMP_DET_CTRL_10:
	case RT5691_HP_AMP_DET_CTRL_11:
	case RT5691_HP_AMP_DET_CTRL_12:
	case RT5691_HP_AMP_DET_CTRL_13:
	case RT5691_OFFSET_CAL_TOP_9:
	case RT5691_OFFSET_CAL_TOP_10:
	case RT5691_OFFSET_CAL_TOP_11:
	case RT5691_OFFSET_CAL_TOP_12:
	case RT5691_OFFSET_CAL_TOP_13:
	case RT5691_OFFSET_CAL_TOP_14:
	case RT5691_OFFSET_CAL_TOP_15:
	case RT5691_OFFSET_CAL_TOP_16:
	case RT5691_OFFSET_CAL_TOP_17:
	case RT5691_OFFSET_CAL_18:
	case RT5691_OFFSET_CAL_19:
	case RT5691_OFFSET_CAL_20:
	case RT5691_OFFSET_CAL_21:
	case RT5691_OFFSET_CAL_22:
	case RT5691_OFFSET_CAL_23:
	case RT5691_OFFSET_CAL_24:
	case RT5691_OFFSET_CAL_25:
	case RT5691_OFFSET_CAL_26:
	case RT5691_OFFSET_CAL_27:
	case RT5691_OFFSET_CAL_28:
	case RT5691_OFFSET_CAL_29:
	case RT5691_OFFSET_CAL_30:
	case RT5691_OFFSET_CAL_31:
	case RT5691_OFFSET_CAL_32:
	case RT5691_OFFSET_CAL_33:
	case RT5691_HP_AMP_DET_CTRL_14:
	case RT5691_HP_AMP_DET_CTRL_15:
	case RT5691_HP_AMP_DET_CTRL_16:
	case RT5691_HP_AMP_DET_CTRL_17:
	case RT5691_HP_AMP_DET_CTRL_18:
	case RT5691_HP_AMP_DET_CTRL_19:
	case RT5691_HP_AMP_DET_CTRL_20:
		return true;
	default:
		return false;
	}
}

static const DECLARE_TLV_DB_SCALE(hp_vol_tlv, -2250, 150, 0);
static const DECLARE_TLV_DB_SCALE(mono_vol_tlv, -2325, 150, 0);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -95625, 375, 0);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -17625, 375, 0);
static const DECLARE_TLV_DB_SCALE(in_bst_tlv, -1200, 75, 0);

static int rt5691_hp_vol_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm = &component->dapm;
	int ret;

	snd_soc_dapm_mutex_lock(dapm);

	dev_dbg(component->dev, "%s %d Impedance value\n", __func__,
		rt5691->imp_value);

	ucontrol->value.integer.value[0] =
		(ucontrol->value.integer.value[0] > rt5691->imp_gain) ?
		(ucontrol->value.integer.value[0] - rt5691->imp_gain) : 0;
	ucontrol->value.integer.value[1] =
		(ucontrol->value.integer.value[1] > rt5691->imp_gain) ?
		(ucontrol->value.integer.value[1] - rt5691->imp_gain) : 0;

	ret = snd_soc_put_volsw(kcontrol, ucontrol);

	snd_soc_dapm_mutex_unlock(dapm);

	return ret;
}

static int rt5691_disable_ng2_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = rt5691->disable_ng2;

	return 0;
}

static void rt5691_noise_gate(struct snd_soc_component *component, bool enable)
{
	if (enable) {
		snd_soc_component_update_bits(component, RT5691_PWR_DA_PATH_1,
			0x0080, 0x0080);
		snd_soc_component_update_bits(component, RT5691_SIL_DET_TOP,
			0xf078, 0xa040);
		snd_soc_component_update_bits(component, RT5691_SIL_DET_CTRL8,
			0xc000, 0xc000);
		snd_soc_component_update_bits(component, RT5691_SIL_DET_CTRL9,
			0xf000, 0xf000);
	} else {
		snd_soc_component_update_bits(component, RT5691_SIL_DET_CTRL9,
			0xf000, 0);
		snd_soc_component_update_bits(component, RT5691_SIL_DET_CTRL8,
			0xc000, 0);
		snd_soc_component_update_bits(component, RT5691_SIL_DET_TOP,
			0x8000, 0);
		snd_soc_component_update_bits(component, RT5691_PWR_DA_PATH_1,
			0x0080, 0);
	}
}

static int rt5691_disable_ng2_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	rt5691->disable_ng2 = !!ucontrol->value.integer.value[0];

	if (rt5691->disable_ng2) {
		snd_soc_component_update_bits(component, RT5691_PWR_DA_PATH_1,
			0x0004, 0);
		rt5691_noise_gate(component, false);
	}

	return 0;
}

static const struct snd_kcontrol_new rt5691_snd_controls[] = {
	/* Headphone Output Volume */
	SOC_DOUBLE_R_EXT_TLV("Headphone Playback Volume",
		RT5691_HP_AMP_L_GAIN_CTRL, RT5691_HP_AMP_R_GAIN_CTRL,
		RT5691_G_HP_SFT, 15, 1, snd_soc_get_volsw, rt5691_hp_vol_put,
		hp_vol_tlv),

	/* Mono Output Volume */
	SOC_SINGLE_TLV("Mono Playback Volume", RT5691_MONO_DRE_CTRL_9,
		RT5691_L_VOL_SFT, 17, 1, mono_vol_tlv),

	/* DAC Digital Volume */
	SOC_DOUBLE_R_TLV("DAC1 Playback Volume", RT5691_MIC_BTN_CTRL_27,
		RT5691_MIC_BTN_CTRL_28, RT5691_R_VOL_SFT, 255, 0, dac_vol_tlv),
	SOC_SINGLE_TLV("DAC2 Playback Volume", RT5691_MONO_DAC_VOL_CTRL,
		RT5691_V_DAC2_L_SFT, 255, 0, dac_vol_tlv),

	/* IN1/IN2/IN3 Volume */
	SOC_SINGLE_TLV("IN1 Boost Volume", RT5691_IN1_IN2_CTRL,
		RT5691_BST1_SFT, 69, 0, in_bst_tlv),
	SOC_SINGLE_TLV("IN2 Boost Volume", RT5691_IN1_IN2_CTRL,
		RT5691_BST2_SFT, 69, 0, in_bst_tlv),
	SOC_SINGLE_TLV("IN3 Boost Volume", RT5691_IN3_CTRL,
		RT5691_BST3_SFT, 69, 0, in_bst_tlv),

	/* ADC Digital Volume Control */
	SOC_DOUBLE_R_TLV("STO1 ADC Capture Volume", RT5691_ADC_FILTER_CTRL_4,
		RT5691_ADC_FILTER_CTRL_5, RT5691_R_VOL_SFT, 127, 0,
		adc_vol_tlv),
	SOC_DOUBLE_R_TLV("STO2 ADC Capture Volume", RT5691_ADC_FILTER2_CTRL_4,
		RT5691_ADC_FILTER2_CTRL_5, RT5691_R_VOL_SFT, 127, 0,
		adc_vol_tlv),

	SOC_SINGLE_EXT("Disable NG2", SND_SOC_NOPM, 0, 1, 0,
		rt5691_disable_ng2_get, rt5691_disable_ng2_put),
};

/***** Muxes *****/
/* STO1 ADC Source */
/* MX-0026 [11:10] [3:2] */
static const char * const rt5691_sto1_adc_src[] = {
	"ADC1", "ADC2", "ADC3"
};

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto1_adcl_enum, RT5691_STO1_ADC_MIXER_CTRL, 10,
	rt5691_sto1_adc_src);

static const struct snd_kcontrol_new rt5691_sto1_adcl_mux =
	SOC_DAPM_ENUM("Stereo1 ADCL Source", rt5691_sto1_adcl_enum);

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto1_adcr_enum, RT5691_STO1_ADC_MIXER_CTRL, 2,
	rt5691_sto1_adc_src);

static const struct snd_kcontrol_new rt5691_sto1_adcr_mux =
	SOC_DAPM_ENUM("Stereo1 ADCR Source", rt5691_sto1_adcr_enum);

/* STO1 ADC1 Source */
/* MX-0026 [13] [5] */
static const char * const rt5691_sto1_adc1_src[] = {
	"DACL2", "STO1 ADC"
};

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto1_adc1l_enum, RT5691_STO1_ADC_MIXER_CTRL, 13,
	rt5691_sto1_adc1_src);

static const struct snd_kcontrol_new rt5691_sto1_adc1l_mux =
	SOC_DAPM_ENUM("Stereo1 ADC1L Source", rt5691_sto1_adc1l_enum);

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto1_adc1r_enum, RT5691_STO1_ADC_MIXER_CTRL, 5,
	rt5691_sto1_adc1_src);

static const struct snd_kcontrol_new rt5691_sto1_adc1r_mux =
	SOC_DAPM_ENUM("Stereo1 ADC1R Source", rt5691_sto1_adc1r_enum);

/* STO1 ADC2 Source */
/* MX-0026 [12] [4] */
static const char * const rt5691_sto1_adc2_src[] = {
	"DAC MIX", "DMIC"
};

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto1_adc2l_enum, RT5691_STO1_ADC_MIXER_CTRL, 12,
	rt5691_sto1_adc2_src);

static const struct snd_kcontrol_new rt5691_sto1_adc2l_mux =
	SOC_DAPM_ENUM("Stereo1 ADC2L Source", rt5691_sto1_adc2l_enum);

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto1_adc2r_enum, RT5691_STO1_ADC_MIXER_CTRL, 4,
	rt5691_sto1_adc2_src);

static const struct snd_kcontrol_new rt5691_sto1_adc2r_mux =
	SOC_DAPM_ENUM("Stereo1 ADC2R Source", rt5691_sto1_adc2r_enum);

/* STO2 ADC Source */
/* MX-0027 [11:10] [3:2] */
static const char * const rt5691_sto2_adc_src[] = {
	"ADC1", "ADC2", "ADC3"
};

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto2_adcl_enum, RT5691_STO2_ADC_MIXER_CTRL, 10,
	rt5691_sto2_adc_src);

static const struct snd_kcontrol_new rt5691_sto2_adcl_mux =
	SOC_DAPM_ENUM("Stereo2 ADCL Source", rt5691_sto2_adcl_enum);

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto2_adcr_enum, RT5691_STO2_ADC_MIXER_CTRL, 2,
	rt5691_sto2_adc_src);

static const struct snd_kcontrol_new rt5691_sto2_adcr_mux =
	SOC_DAPM_ENUM("Stereo2 ADCR Source", rt5691_sto2_adcr_enum);

/* STO2 ADC1 Source */
/* MX-0027 [13] [5] */
static const char * const rt5691_sto2_adc1_src[] = {
	"DACL2", "STO2 ADC"
};

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto2_adc1l_enum, RT5691_STO2_ADC_MIXER_CTRL, 13,
	rt5691_sto2_adc1_src);

static const struct snd_kcontrol_new rt5691_sto2_adc1l_mux =
	SOC_DAPM_ENUM("Stereo2 ADC1L Source", rt5691_sto2_adc1l_enum);

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto2_adc1r_enum, RT5691_STO2_ADC_MIXER_CTRL, 5,
	rt5691_sto2_adc1_src);

static const struct snd_kcontrol_new rt5691_sto2_adc1r_mux =
	SOC_DAPM_ENUM("Stereo2 ADC1R Source", rt5691_sto2_adc1r_enum);

/* STO2 ADC2 Source */
/* MX-0027 [12] [4] */
static const char * const rt5691_sto2_adc2_src[] = {
	"DAC MIX", "DMIC"
};

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto2_adc2l_enum, RT5691_STO2_ADC_MIXER_CTRL, 12,
	rt5691_sto2_adc2_src);

static const struct snd_kcontrol_new rt5691_sto2_adc2l_mux =
	SOC_DAPM_ENUM("Stereo2 ADC2L Source", rt5691_sto2_adc2l_enum);

static SOC_ENUM_SINGLE_DECL(
	rt5691_sto2_adc2r_enum, RT5691_STO2_ADC_MIXER_CTRL, 4,
	rt5691_sto2_adc2_src);

static const struct snd_kcontrol_new rt5691_sto2_adc2r_mux =
	SOC_DAPM_ENUM("Stereo2 ADC2R Source", rt5691_sto2_adc2r_enum);

/* I2S ADCDAT Source */
/* MX-0030 [1:0] */
static const char * const rt5691_if_adc_data_src[] = {
	"STO1 ADC", "STO2 ADC", "DAC"
};

static SOC_ENUM_SINGLE_DECL(
	rt5691_if_adc_data_enum, RT5691_FIFO_CTRL, 0, rt5691_if_adc_data_src);

static const struct snd_kcontrol_new rt5691_if_adc_mux =
	SOC_DAPM_ENUM("IF ADC Source", rt5691_if_adc_data_enum);

/* Interface Data Select */
/* MX-2400 [11:10] [9:8] */
static const char * const rt5691_data_select[] = {
	"L/R", "R/L", "L/L", "R/R"
};

static SOC_ENUM_SINGLE_DECL(rt5691_if_data_adc_enum,
	RT5691_I2S_CTRL_1, 8, rt5691_data_select);

static const struct snd_kcontrol_new rt5691_if_data_adc_mux =
	SOC_DAPM_ENUM("IF Data ADC Source", rt5691_if_data_adc_enum);

static SOC_ENUM_SINGLE_DECL(rt5691_if_data_dac_enum,
	RT5691_I2S_CTRL_1, 10, rt5691_data_select);

static const struct snd_kcontrol_new rt5691_if_data_dac_mux =
	SOC_DAPM_ENUM("IF Data DAC Source", rt5691_if_data_dac_enum);

/* DAC2 Source */
/* MX-0031 [4] */
static const char * const rt5691_dac2_src[] = {
	"STO1 DAC Mixer", "DAC2"
};

static SOC_ENUM_SINGLE_DECL(
	rt5691_dac2_enum, RT5691_PDM_OUTPUT_CTRL, 4, rt5691_dac2_src);

static const struct snd_kcontrol_new rt5691_dac2_mux =
	SOC_DAPM_ENUM("DAC2 Source", rt5691_dac2_enum);

/* PDM */
/* MX-0031 [11:10] [9:8] */
static const char * const rt5691_pdm_src[] = {
	"Stereo1 DAC", "DAC2", "Sidetone"
};

static SOC_ENUM_SINGLE_DECL(
	rt5691_pdm_l_enum, RT5691_PDM_OUTPUT_CTRL, 10, rt5691_pdm_src);

static const struct snd_kcontrol_new rt5691_pdm_l_mux =
	SOC_DAPM_ENUM("PDM L Source", rt5691_pdm_l_enum);

static SOC_ENUM_SINGLE_DECL(
	rt5691_pdm_r_enum, RT5691_PDM_OUTPUT_CTRL, 8, rt5691_pdm_src);

static const struct snd_kcontrol_new rt5691_pdm_r_mux =
	SOC_DAPM_ENUM("PDM R Source", rt5691_pdm_r_enum);

/* Sidetone Source */
/* MX-0024 [9] */
static const char * const rt5691_st_src[] = {
	"ADC1", "ADC2", "DMICL", "DMICR"
};

static SOC_ENUM_SINGLE_DECL(
	rt5691_st_enum, RT5691_ST_MUX_CTRL, 9, rt5691_st_src);

static const struct snd_kcontrol_new rt5691_st_mux =
	SOC_DAPM_ENUM("Sidetone Source", rt5691_st_enum);

/***** Mixers *****/
static const struct snd_kcontrol_new rt5691_sto1_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5691_STO1_ADC_MIXER_CTRL,
			15, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5691_STO1_ADC_MIXER_CTRL,
			14, 1, 1),
};

static const struct snd_kcontrol_new rt5691_sto1_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5691_STO1_ADC_MIXER_CTRL,
			7, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5691_STO1_ADC_MIXER_CTRL,
			6, 1, 1),
};

static const struct snd_kcontrol_new rt5691_sto2_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5691_STO2_ADC_MIXER_CTRL,
			15, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5691_STO2_ADC_MIXER_CTRL,
			14, 1, 1),
};

static const struct snd_kcontrol_new rt5691_sto2_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5691_STO2_ADC_MIXER_CTRL,
			7, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5691_STO2_ADC_MIXER_CTRL,
			6, 1, 1),
};

static const struct snd_kcontrol_new rt5691_dac_l_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5691_AD_DA_MIXER_CTRL,
			RT5691_M_ADCMIX_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC1 Switch", RT5691_MIC_BTN_CTRL_26, 7, 1, 1),
};

static const struct snd_kcontrol_new rt5691_dac_r_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5691_AD_DA_MIXER_CTRL,
			RT5691_M_ADCMIX_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC1 Switch", RT5691_MIC_BTN_CTRL_26, 3, 1, 1),
};

static const struct snd_kcontrol_new rt5691_sto_dac_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5691_STO1_DAC_MIXER_CTRL,
			15, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5691_STO1_DAC_MIXER_CTRL,
			13, 1, 1),
	SOC_DAPM_SINGLE("Sidetone Switch", RT5691_STO1_DAC_MIXER_CTRL,
			11, 1, 1),
};

static const struct snd_kcontrol_new rt5691_sto_dac_r_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5691_STO1_DAC_MIXER_CTRL,
			7, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5691_STO1_DAC_MIXER_CTRL,
			5, 1, 1),
	SOC_DAPM_SINGLE("Sidetone Switch", RT5691_STO1_DAC_MIXER_CTRL,
			3, 1, 1),
};

static const struct snd_kcontrol_new rt5691_mono_switch =
	SOC_DAPM_SINGLE("Switch", RT5691_MONO_DAC_VOL_CTRL,
		RT5691_M_DAC2_L_SFT, 1, 1);

static const struct snd_kcontrol_new rt5691_pdm_l_switch =
	SOC_DAPM_SINGLE("Switch", RT5691_PDM_CTRL_1, 3, 1, 1);

static const struct snd_kcontrol_new rt5691_pdm_r_switch =
	SOC_DAPM_SINGLE("Switch", RT5691_PDM_CTRL_1, 2, 1, 1);

static int rt5691_set_verf(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		switch (w->shift) {
		case RT5691_PWR_VREF1_BIT:
			snd_soc_component_update_bits(component,
				RT5691_PWR_ANLG_1, RT5691_PWR_FV1, 0);
			break;

		case RT5691_PWR_VREF2_BIT:
			snd_soc_component_update_bits(component,
				RT5691_PWR_ANLG_1, RT5691_PWR_FV2, 0);
			break;

		case RT5691_PWR_VREF3_BIT:
			snd_soc_component_update_bits(component,
				RT5691_PWR_ANLG_1, RT5691_PWR_FV3, 0);
			break;

		default:
			break;
		}
		break;

	case SND_SOC_DAPM_POST_PMU:
		usleep_range(15000, 20000);
		switch (w->shift) {
		case RT5691_PWR_VREF1_BIT:
			snd_soc_component_update_bits(component,
				RT5691_PWR_ANLG_1, RT5691_PWR_FV1,
				RT5691_PWR_FV1);
			break;

		case RT5691_PWR_VREF2_BIT:
			snd_soc_component_update_bits(component,
				RT5691_PWR_ANLG_1, RT5691_PWR_FV2,
				RT5691_PWR_FV2);
			break;

		case RT5691_PWR_VREF3_BIT:
			snd_soc_component_update_bits(component,
				RT5691_PWR_ANLG_1, RT5691_PWR_FV3,
				RT5691_PWR_FV3);
			break;

		default:
			break;
		}
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5691_calc_clk_div(struct rt5691_priv *rt5691, int fs,
	int clk)
{
	struct snd_soc_component *component = rt5691->component;
	int div[] = {1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128};
	int i, target;

	if (rt5691->sysclk < 1000000 * div[0]) {
		dev_warn(component->dev, "Base clock rate %d is too low\n",
			rt5691->sysclk);
		return -EINVAL;
	}

	if (fs)
		target = rt5691->lrck * fs;
	else
		target = clk;

	for (i = 0; i < ARRAY_SIZE(div); i++) {
		if (target * div[i] >= rt5691->sysclk)
			return i;
	}

	dev_warn(component->dev, "Base clock rate %d is too high\n",
		rt5691->sysclk);

	return -EINVAL;
}

static int rt5691_set_dmic_clk(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);
	int idx = -EINVAL;

	idx = rt5691_calc_clk_div(rt5691, 64, 0);

	if (idx < 0)
		dev_err(component->dev, "Failed to set DMIC clock\n");
	else {
		snd_soc_component_update_bits(component, RT5691_CLK_DIV_DMIC,
			0xf, idx);
	}

	return idx;
}

static int rt5691_set_pdm_clk(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);
	int idx = -EINVAL;

	idx = rt5691_calc_clk_div(rt5691, 128, 0);

	if (idx < 0)
		dev_err(component->dev, "Failed to set PDM clock\n");
	else {
		snd_soc_component_update_bits(component, RT5691_CLK_DIV_PDM,
			0xf, idx);
	}

	return idx;
}

static void rt5691_recalibrate(struct snd_soc_component *component)
{
	static struct reg_sequence rt5691_rek_list[] = {
		{RT5691_MIC_BTN_CTRL_26,		0x0088},
		{RT5691_ADC_FILTER_CTRL_3,		0x0090},
		{RT5691_ADC_FILTER2_CTRL_3,		0x0090},
		{RT5691_RC_CLK,				0x8800},
		{RT5691_SYS_CLK_SRC,			0x007d},
		{RT5691_PWR_DA_PATH_1,			0x6801},
		{RT5691_PWR_DA_PATH_2,			0x0223},
		{RT5691_PWR_AD_PATH,			0x0023},
		{RT5691_DA_STO1_FILTER_CLK_DIV,		0x0202},
		{RT5691_AD_STO1_FILTER_CLK_DIV,		0x0202},
		{RT5691_ADC_AND_DAC_OSR,		0x0202},
		{RT5691_STO1_DAC_MIXER_CTRL,		0x2888},
		{RT5691_HP_BEHAVIOR_LOGIC_CTRL_2,	0x0000},
		{RT5691_BGLDO33_CTRL_1,			0x8000},
		{RT5691_ADC56_CTRL,			0xb490},
		{RT5691_OFFSET_CAL_1,			0x2500},
	};
	unsigned int rt5691_rek_list_saved[ARRAY_SIZE(rt5691_rek_list)];
	int i;

	for (i = 0; i < ARRAY_SIZE(rt5691_rek_list); i++) {
		rt5691_rek_list_saved[i] =
			snd_soc_component_read(component, rt5691_rek_list[i].reg);

		if (rt5691_rek_list[i].reg == RT5691_PWR_DA_PATH_2) {
			snd_soc_component_update_bits(component,
				rt5691_rek_list[i].reg, rt5691_rek_list[i].def,
				rt5691_rek_list[i].def);
			continue;
		}

		snd_soc_component_write(component, rt5691_rek_list[i].reg,
			rt5691_rek_list[i].def);
	}

	snd_soc_component_update_bits(component, RT5691_OFFSET_CAL_2, 0x0300,
		0x0300);
	snd_soc_component_update_bits(component, RT5691_OFFSET_CAL_2, 0x0300,
		0x0100);

	i = 0;
	while (true) {
		if (snd_soc_component_read(component,
			RT5691_HP_AMP_DET_CTRL_12) & 0x8000)
			usleep_range(10000, 10005);
		else
			break;

		if (i > 20) {
			dev_err(component->dev, "HP Recalibration Failure\n");
			break;
		}

		i++;
	}

	snd_soc_component_update_bits(component, RT5691_OFFSET_CAL_2, 0x0300,
		0);

	for (i = ARRAY_SIZE(rt5691_rek_list) - 1; i >= 0; i--)
		snd_soc_component_write(component, rt5691_rek_list[i].reg,
			rt5691_rek_list_saved[i]);
}

static int rt5691_hp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component, RT5691_HPOUT_CP_CTRL_1,
			0x0c00, 0x0c00);

		snd_soc_component_update_bits(component, RT5691_HP_AMP_CTRL_2,
			0x3000, 0x3000);

		snd_soc_component_update_bits(component,
			RT5691_HP_AMP_DET_CTRL_18, 0x000c, 0x0008);
		usleep_range(10000, 10005);
		snd_soc_component_update_bits(component,
			RT5691_HP_AMP_DET_CTRL_14, 0x0070, 0x0040);

		if (!rt5691->disable_ng2) {
			snd_soc_component_update_bits(component,
				RT5691_PWR_DA_PATH_1, 0x0004, 0x0004);
			rt5691_noise_gate(component, true);
		}

		rt5691->rek_timeout = jiffies + (HZ * 60);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component, RT5691_HP_AMP_CTRL_2,
			0x3000, 0);
		snd_soc_component_update_bits(component,
			RT5691_HP_AMP_DET_CTRL_14, 0x0070, 0x0020);
		snd_soc_component_update_bits(component,
			RT5691_HP_AMP_DET_CTRL_18, 0x000c, 0);
		snd_soc_component_update_bits(component, RT5691_HPOUT_CP_CTRL_1,
			0x0c00, 0);

		snd_soc_component_update_bits(component, RT5691_PWR_DA_PATH_1,
			0x0004, 0);
		rt5691_noise_gate(component, false);

		if (time_after(jiffies, rt5691->rek_timeout) && rt5691->rek) {
			rt5691_recalibrate(component);
			rt5691->rek = false;
		}
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5691_mono_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			RT5691_MONO_OUTPUT_CTRL, 0x0020, 0x0020);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			RT5691_MONO_OUTPUT_CTRL, 0x0020, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5691_sto1_l_adc_depop_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		schedule_delayed_work(&rt5691->sto1_l_adc_work,
			msecs_to_jiffies(80));
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cancel_delayed_work_sync(&rt5691->sto1_l_adc_work);
		snd_soc_component_update_bits(component,
			RT5691_ADC_FILTER_CTRL_3, (0x1 << 7), (0x1 << 7));
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5691_sto1_r_adc_depop_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		schedule_delayed_work(&rt5691->sto1_r_adc_work,
			msecs_to_jiffies(80));
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cancel_delayed_work_sync(&rt5691->sto1_r_adc_work);
		snd_soc_component_update_bits(component,
			RT5691_ADC_FILTER_CTRL_3, (0x1 << 4), (0x1 << 4));
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5691_sto2_l_adc_depop_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		schedule_delayed_work(&rt5691->sto2_l_adc_work,
			msecs_to_jiffies(80));
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cancel_delayed_work_sync(&rt5691->sto2_l_adc_work);
		snd_soc_component_update_bits(component,
			RT5691_ADC_FILTER2_CTRL_3, (0x1 << 7), (0x1 << 7));
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5691_sto2_r_adc_depop_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		schedule_delayed_work(&rt5691->sto2_r_adc_work,
			msecs_to_jiffies(80));
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cancel_delayed_work_sync(&rt5691->sto2_r_adc_work);
		snd_soc_component_update_bits(component,
			RT5691_ADC_FILTER2_CTRL_3, (0x1 << 4), (0x1 << 4));
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5691_bst1_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!rt5691->open_gender) {
			snd_soc_component_update_bits(component,
				RT5691_COMBO_JACK_CTRL_2, 0x8000, 0);
			snd_soc_component_update_bits(component,
				RT5691_MONO_ANLG_DRE_CTRL_2, 0x0600, 0);
			snd_soc_component_update_bits(component,
				RT5691_COMBO_JACK_CTRL_3, 0x00f3, 0x00e2);
			snd_soc_component_update_bits(component,
				RT5691_COMBO_JACK_CTRL_2, 0x8000, 0x8000);
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
		if (!rt5691->open_gender) {
			snd_soc_component_update_bits(component,
				RT5691_COMBO_JACK_CTRL_2, 0x8000, 0);
			snd_soc_component_update_bits(component,
				RT5691_MONO_ANLG_DRE_CTRL_2, 0x0600, 0x0600);
			snd_soc_component_update_bits(component,
				RT5691_COMBO_JACK_CTRL_3, 0x00f3, 0x00e1);
			snd_soc_component_update_bits(component,
				RT5691_COMBO_JACK_CTRL_2, 0x8000, 0x8000);
		}
		break;

	default:
		return 0;
	}

	return 0;

}

static const struct snd_soc_dapm_widget rt5691_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY("RC 1M", RT5691_RC_CLK, 11, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("RC 25M", RT5691_RC_CLK, 15, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PLAY CLK", RT5691_SYS_CLK_SRC, 5, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("RECORD CLK", RT5691_SYS_CLK_SRC, 4, 0, NULL,
		0),

	/* Clock Detect */
	SND_SOC_DAPM_SUPPLY("CLKDET", RT5691_MCLK_DET_CTRL_2, 2, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("CLKDET HP", RT5691_MCLK_DET_CTRL_1, 15, 0, NULL,
		0),
	SND_SOC_DAPM_SUPPLY("CLKDET MONO", RT5691_MCLK_DET_CTRL_1, 13, 0, NULL,
		0),

	/* Vref */
	SND_SOC_DAPM_SUPPLY("Vref1", RT5691_PWR_ANLG_1, RT5691_PWR_VREF1_BIT, 0,
		rt5691_set_verf, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY("Vref2", RT5691_PWR_ANLG_1, RT5691_PWR_VREF2_BIT, 0,
		rt5691_set_verf, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY("Vref3", RT5691_PWR_ANLG_1, RT5691_PWR_VREF3_BIT, 0,
		rt5691_set_verf, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),

	/* Input Side */
	SND_SOC_DAPM_SUPPLY("MICBIAS1", RT5691_PWR_ANLG_2, RT5691_PWR_MB1_BIT,
		0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MICBIAS2", RT5691_PWR_ANLG_2, RT5691_PWR_MB2_BIT,
		0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MICBIAS3", RT5691_PWR_ANLG_2, RT5691_PWR_MB3_BIT,
		0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("LDO MICBIAS1", RT5691_MONO_ANLG_DRE_CTRL_2, 7, 0,
		NULL, 0),
	SND_SOC_DAPM_SUPPLY("LDO MICBIAS2", RT5691_MONO_ANLG_DRE_CTRL_3, 7, 0,
		NULL, 0),
	SND_SOC_DAPM_SUPPLY("LDO MICBIAS3", RT5691_MONO_ANLG_DRE_CTRL_4, 7, 0,
		NULL, 0),

	/* Input Lines */
	SND_SOC_DAPM_INPUT("DMICL"),
	SND_SOC_DAPM_INPUT("DMICR"),

	SND_SOC_DAPM_INPUT("IN1P"),
	SND_SOC_DAPM_INPUT("IN1N"),
	SND_SOC_DAPM_INPUT("IN2P"),
	SND_SOC_DAPM_INPUT("IN2N"),
	SND_SOC_DAPM_INPUT("IN3P"),
	SND_SOC_DAPM_INPUT("IN3N"),

	SND_SOC_DAPM_SUPPLY("DMIC CLK", SND_SOC_NOPM, 0, 0,
		rt5691_set_dmic_clk, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("DMIC Power", RT5691_DMIC_CTRL, 7, 0, NULL, 0),

	/* Boost */
	SND_SOC_DAPM_PGA_E("BST1", SND_SOC_NOPM,
		0, 0, NULL, 0, rt5691_bst1_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA("BST2", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("BST3", SND_SOC_NOPM,
		0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("BST1 Power", RT5691_PWR_ANLG_2,
		RT5691_PWR_BST1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("BST2 Power", RT5691_PWR_ANLG_2,
		RT5691_PWR_BST2_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("BST3 Power", RT5691_PWR_ANLG_2,
		RT5691_PWR_BST3_BIT, 0, NULL, 0),

	/* ADCs */
	SND_SOC_DAPM_ADC("ADC1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC2", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC3", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_SUPPLY("ADC1 Power", RT5691_PWR_DIG_1, 5, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC2 Power", RT5691_PWR_DIG_1, 4, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC3 Power", RT5691_PWR_DIG_1, 3, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC12 clock", RT5691_ADC12_CTRL, 12, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC34 clock", RT5691_ADC34_CTRL, 12, 0, NULL, 0),

	/* Sidetone */
	SND_SOC_DAPM_MUX("Sidetone Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_st_mux),

	/* ADC Mux */
	SND_SOC_DAPM_MUX("Stereo1 ADC L1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto1_adc1l_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC R1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto1_adc1r_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC L2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto1_adc2l_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC R2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto1_adc2r_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC L Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto1_adcl_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC R Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto1_adcr_mux),

	SND_SOC_DAPM_MUX("Stereo2 ADC L1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto2_adc1l_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC R1 Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto2_adc1r_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC L2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto2_adc2l_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC R2 Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto2_adc2r_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC L Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto2_adcl_mux),
	SND_SOC_DAPM_MUX("Stereo2 ADC R Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_sto2_adcr_mux),

	/* ADC Mixer */
	SND_SOC_DAPM_SUPPLY("ADC Stereo1 Filter", RT5691_PWR_AD_PATH,
		0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("Stereo1 ADC MIXL", SND_SOC_NOPM,
		0, 0, rt5691_sto1_adc_l_mix, ARRAY_SIZE(rt5691_sto1_adc_l_mix),
		rt5691_sto1_l_adc_depop_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("Stereo1 ADC MIXR", SND_SOC_NOPM,
		0, 0, rt5691_sto1_adc_r_mix, ARRAY_SIZE(rt5691_sto1_adc_r_mix),
		rt5691_sto1_r_adc_depop_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("ADC Stereo2 Filter", RT5691_PWR_AD_PATH,
		1, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("Stereo2 ADC MIXL", SND_SOC_NOPM,
		0, 0, rt5691_sto2_adc_l_mix, ARRAY_SIZE(rt5691_sto2_adc_l_mix),
		rt5691_sto2_l_adc_depop_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("Stereo2 ADC MIXR", SND_SOC_NOPM,
		0, 0, rt5691_sto2_adc_r_mix, ARRAY_SIZE(rt5691_sto2_adc_r_mix),
		rt5691_sto2_r_adc_depop_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("ADC Stereo FIFO", RT5691_PWR_AD_PATH,
		5, 0, NULL, 0),

	/* ADC PGA */
	SND_SOC_DAPM_PGA("Stereo1 ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo2 ADC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Digital Interface */
	SND_SOC_DAPM_SUPPLY("I2S", RT5691_I2S, 0, 0, NULL, 0),

	SND_SOC_DAPM_PGA("IF DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF DAC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF DAC R", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Digital Interface Select */
	SND_SOC_DAPM_MUX("IF ADC Mux", SND_SOC_NOPM, 0, 0,
		&rt5691_if_adc_mux),
	SND_SOC_DAPM_MUX("IF ADC Swap Mux", SND_SOC_NOPM, 0, 0,
			&rt5691_if_data_adc_mux),
	SND_SOC_DAPM_MUX("IF DAC Swap Mux", SND_SOC_NOPM, 0, 0,
			&rt5691_if_data_dac_mux),

	/* Audio Interface */
	SND_SOC_DAPM_AIF_IN("AIF RX", "AIF Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF TX", "AIF Capture", 0, SND_SOC_NOPM, 0, 0),

	/* Output Side */
	/* DAC mixer before sound effect */
	SND_SOC_DAPM_MIXER("DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5691_dac_l_mix, ARRAY_SIZE(rt5691_dac_l_mix)),
	SND_SOC_DAPM_MIXER("DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5691_dac_r_mix, ARRAY_SIZE(rt5691_dac_r_mix)),

	/* DAC channel Mux */
	SND_SOC_DAPM_MUX("DAC2 Mux", SND_SOC_NOPM, 0, 0, &rt5691_dac2_mux),

	/* DAC Mixer */
	SND_SOC_DAPM_SUPPLY("DAC FIFO Stereo", RT5691_PWR_DA_PATH_1, 11, 0,
		NULL, 0),
	SND_SOC_DAPM_SUPPLY("DAC FIFO Mono", RT5691_PWR_DA_PATH_1, 12, 0,
		NULL, 0),
	SND_SOC_DAPM_MIXER("Stereo DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5691_sto_dac_l_mix, ARRAY_SIZE(rt5691_sto_dac_l_mix)),
	SND_SOC_DAPM_MIXER("Stereo DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5691_sto_dac_r_mix, ARRAY_SIZE(rt5691_sto_dac_r_mix)),

	/* DACs */
	SND_SOC_DAPM_SUPPLY("DAC Stereo", RT5691_PWR_DA_PATH_1, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DAC Mono", RT5691_PWR_DA_PATH_1, 1, 0, NULL, 0),

	SND_SOC_DAPM_DAC("DACL", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DACR", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DACM", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_PGA("DAC1 MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DACL2", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("DACM Power", RT5691_PWR_DIG_1, 7, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY_S("DACL Clock", 1, RT5691_PWR_DA_PATH_2,
		0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DACR Clock", 1, RT5691_PWR_DA_PATH_2,
		1, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DACM Clock", 1, RT5691_PWR_DA_PATH_2,
		2, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("HP Clock", 1, RT5691_PWR_DA_PATH_2,
		5, 0, NULL, 0),

	SND_SOC_DAPM_PGA_S("HP Amp", 1, SND_SOC_NOPM, 0, 0, rt5691_hp_event,
		SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_PGA_S("Mono Amp", 1, RT5691_PWR_ANLG_1, RT5691_PWR_MA_BIT,
		0, rt5691_mono_event,
		SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_SWITCH("Mono Playback", SND_SOC_NOPM, 0, 0,
		&rt5691_mono_switch),
	SND_SOC_DAPM_SWITCH("PDM L Playback", SND_SOC_NOPM, 0, 0,
		&rt5691_pdm_l_switch),
	SND_SOC_DAPM_SWITCH("PDM R Playback", SND_SOC_NOPM, 0, 0,
		&rt5691_pdm_r_switch),

	/* PDM */
	SND_SOC_DAPM_SUPPLY("PDM CLK", RT5691_PWR_DA_PATH_1, 15, 0,
		rt5691_set_pdm_clk, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MUX("PDM L Mux", SND_SOC_NOPM, 0, 0, &rt5691_pdm_l_mux),
	SND_SOC_DAPM_MUX("PDM R Mux", SND_SOC_NOPM, 0, 0, &rt5691_pdm_r_mux),

	/* Output Lines */
	SND_SOC_DAPM_OUTPUT("HPOL"),
	SND_SOC_DAPM_OUTPUT("HPOR"),
	SND_SOC_DAPM_OUTPUT("MONOOUT"),
	SND_SOC_DAPM_OUTPUT("PDML"),
	SND_SOC_DAPM_OUTPUT("PDMR"),
};

static const struct snd_soc_dapm_route rt5691_dapm_routes[] = {
	{"MICBIAS1", NULL, "Vref1"},
	{"MICBIAS1", NULL, "Vref2"},
	{"MICBIAS1", NULL, "LDO MICBIAS1"},

	{"MICBIAS2", NULL, "Vref1"},
	{"MICBIAS2", NULL, "Vref2"},
	{"MICBIAS2", NULL, "LDO MICBIAS2"},

	{"MICBIAS3", NULL, "Vref1"},
	{"MICBIAS3", NULL, "Vref2"},
	{"MICBIAS3", NULL, "LDO MICBIAS3"},

	{"CLKDET HP", NULL, "CLKDET"},
	{"CLKDET MONO", NULL, "CLKDET"},

	{"BST1", NULL, "IN1P"},
	{"BST1", NULL, "IN1N"},
	{"BST1", NULL, "BST1 Power"},

	{"BST2", NULL, "IN2P"},
	{"BST2", NULL, "IN2N"},
	{"BST2", NULL, "BST2 Power"},

	{"BST3", NULL, "IN3P"},
	{"BST3", NULL, "IN3N"},
	{"BST3", NULL, "BST3 Power"},

	{"ADC1", NULL, "BST1"},
	{"ADC1", NULL, "ADC1 Power"},
	{"ADC1", NULL, "ADC12 clock"},
	{"ADC1", NULL, "Vref1"},
	{"ADC1", NULL, "Vref2"},

	{"ADC2", NULL, "BST2"},
	{"ADC2", NULL, "ADC2 Power"},
	{"ADC2", NULL, "ADC12 clock"},
	{"ADC2", NULL, "Vref1"},
	{"ADC2", NULL, "Vref2"},

	{"ADC3", NULL, "BST3"},
	{"ADC3", NULL, "ADC3 Power"},
	{"ADC3", NULL, "ADC34 clock"},
	{"ADC3", NULL, "Vref1"},
	{"ADC3", NULL, "Vref2"},

	{"DMICL", NULL, "DMIC CLK"},
	{"DMICL", NULL, "DMIC Power"},
	{"DMICR", NULL, "DMIC CLK"},
	{"DMICR", NULL, "DMIC Power"},

	{"Sidetone Mux", "ADC1", "ADC1"},
	{"Sidetone Mux", "ADC2", "ADC2"},
	{"Sidetone Mux", "DMICL", "DMICL"},
	{"Sidetone Mux", "DMICR", "DMICR"},

	{"Stereo1 ADC L Mux", "ADC1", "ADC1"},
	{"Stereo1 ADC L Mux", "ADC2", "ADC2"},
	{"Stereo1 ADC L Mux", "ADC3", "ADC3"},
	{"Stereo1 ADC R Mux", "ADC1", "ADC1"},
	{"Stereo1 ADC R Mux", "ADC2", "ADC2"},
	{"Stereo1 ADC R Mux", "ADC3", "ADC3"},

	{"Stereo1 ADC L1 Mux", "DACL2", "DACL2"},
	{"Stereo1 ADC L1 Mux", "STO1 ADC", "Stereo1 ADC L Mux"},
	{"Stereo1 ADC R1 Mux", "DACL2", "DACL2"},
	{"Stereo1 ADC R1 Mux", "STO1 ADC", "Stereo1 ADC R Mux"},

	{"Stereo1 ADC L2 Mux", "DAC MIX", "DAC MIXL"},
	{"Stereo1 ADC L2 Mux", "DMIC", "DMICL"},
	{"Stereo1 ADC R2 Mux", "DAC MIX", "DAC MIXR"},
	{"Stereo1 ADC R2 Mux", "DMIC", "DMICR"},

	{"Stereo1 ADC MIXL", "ADC1 Switch", "Stereo1 ADC L1 Mux"},
	{"Stereo1 ADC MIXL", "ADC2 Switch", "Stereo1 ADC L2 Mux"},
	{"Stereo1 ADC MIXL", NULL, "ADC Stereo1 Filter"},
	{"Stereo1 ADC MIXL", NULL, "ADC Stereo FIFO"},

	{"Stereo1 ADC MIXR", "ADC1 Switch", "Stereo1 ADC R1 Mux"},
	{"Stereo1 ADC MIXR", "ADC2 Switch", "Stereo1 ADC R2 Mux"},
	{"Stereo1 ADC MIXR", NULL, "ADC Stereo1 Filter"},
	{"Stereo1 ADC MIXR", NULL, "ADC Stereo FIFO"},

	{"Stereo2 ADC L Mux", "ADC1", "ADC1"},
	{"Stereo2 ADC L Mux", "ADC2", "ADC2"},
	{"Stereo2 ADC L Mux", "ADC3", "ADC3"},
	{"Stereo2 ADC R Mux", "ADC1", "ADC1"},
	{"Stereo2 ADC R Mux", "ADC2", "ADC2"},
	{"Stereo2 ADC R Mux", "ADC3", "ADC3"},

	{"Stereo2 ADC L1 Mux", "DACL2", "DACL2"},
	{"Stereo2 ADC L1 Mux", "STO2 ADC", "Stereo2 ADC L Mux"},
	{"Stereo2 ADC R1 Mux", "DACL2", "DACL2"},
	{"Stereo2 ADC R1 Mux", "STO2 ADC", "Stereo2 ADC R Mux"},

	{"Stereo2 ADC L2 Mux", "DAC MIX", "DAC MIXL"},
	{"Stereo2 ADC L2 Mux", "DMIC", "DMICL"},
	{"Stereo2 ADC R2 Mux", "DAC MIX", "DAC MIXR"},
	{"Stereo2 ADC R2 Mux", "DMIC", "DMICR"},

	{"Stereo2 ADC MIXL", "ADC1 Switch", "Stereo2 ADC L1 Mux"},
	{"Stereo2 ADC MIXL", "ADC2 Switch", "Stereo2 ADC L2 Mux"},
	{"Stereo2 ADC MIXL", NULL, "ADC Stereo2 Filter"},
	{"Stereo2 ADC MIXL", NULL, "ADC Stereo FIFO"},

	{"Stereo2 ADC MIXR", "ADC1 Switch", "Stereo2 ADC R1 Mux"},
	{"Stereo2 ADC MIXR", "ADC2 Switch", "Stereo2 ADC R2 Mux"},
	{"Stereo2 ADC MIXR", NULL, "ADC Stereo2 Filter"},
	{"Stereo2 ADC MIXR", NULL, "ADC Stereo FIFO"},

	{"Stereo1 ADC MIX", NULL, "Stereo1 ADC MIXL"},
	{"Stereo1 ADC MIX", NULL, "Stereo1 ADC MIXR"},
	{"Stereo2 ADC MIX", NULL, "Stereo2 ADC MIXL"},
	{"Stereo2 ADC MIX", NULL, "Stereo2 ADC MIXR"},

	{"IF ADC Mux", "STO1 ADC", "Stereo1 ADC MIX"},
	{"IF ADC Mux", "STO2 ADC", "Stereo2 ADC MIX"},
	{"IF ADC Mux", "DAC", "DAC1 MIX"},

	{"IF ADC Swap Mux", "L/R", "IF ADC Mux"},
	{"IF ADC Swap Mux", "R/L", "IF ADC Mux"},
	{"IF ADC Swap Mux", "L/L", "IF ADC Mux"},
	{"IF ADC Swap Mux", "R/R", "IF ADC Mux"},

	{"AIF TX", NULL, "IF ADC Swap Mux"},

	{"AIF TX", NULL, "I2S"},
	{"AIF TX", NULL, "RECORD CLK"},

	{"AIF RX", NULL, "I2S"},
	{"AIF RX", NULL, "PLAY CLK"},

	{"IF DAC Swap Mux", "L/R", "AIF RX"},
	{"IF DAC Swap Mux", "R/L", "AIF RX"},
	{"IF DAC Swap Mux", "L/L", "AIF RX"},
	{"IF DAC Swap Mux", "R/R", "AIF RX"},
	{"IF DAC", NULL, "IF DAC Swap Mux"},

	{"IF DAC L", NULL, "IF DAC"},
	{"IF DAC R", NULL, "IF DAC"},

	{"DAC MIXL", "Stereo ADC Switch", "Stereo1 ADC MIXL"},
	{"DAC MIXL", "DAC1 Switch", "IF DAC L"},
	{"DAC MIXR", "Stereo ADC Switch", "Stereo1 ADC MIXR"},
	{"DAC MIXR", "DAC1 Switch", "IF DAC R"},

	{"DAC1 MIX", NULL, "DAC MIXL"},
	{"DAC1 MIX", NULL, "DAC MIXR"},

	{"Stereo DAC MIXL", "DAC L1 Switch", "DAC MIXL"},
	{"Stereo DAC MIXL", "DAC R1 Switch", "DAC MIXR"},
	{"Stereo DAC MIXL", "Sidetone Switch", "Sidetone Mux"},
	{"Stereo DAC MIXL", NULL, "DAC Stereo"},
	{"Stereo DAC MIXL", NULL, "DAC FIFO Stereo"},

	{"Stereo DAC MIXR", "DAC L1 Switch", "DAC MIXL"},
	{"Stereo DAC MIXR", "DAC R1 Switch", "DAC MIXR"},
	{"Stereo DAC MIXR", "Sidetone Switch", "Sidetone Mux"},
	{"Stereo DAC MIXR", NULL, "DAC Stereo"},
	{"Stereo DAC MIXR", NULL, "DAC FIFO Stereo"},

	{"DACL", NULL, "Stereo DAC MIXL"},
	{"DACL", NULL, "DACL Clock"},
	{"DACR", NULL, "Stereo DAC MIXR"},
	{"DACR", NULL, "DACR Clock"},

	{"DACL2", NULL, "IF DAC L"},
	{"DACL2", NULL, "DAC Mono"},
	{"DACL2", NULL, "DAC FIFO Mono"},

	{"DAC2 Mux", "STO1 DAC Mixer", "Stereo DAC MIXL"},
	{"DAC2 Mux", "DAC2", "DACL2"},

	{"DACM", NULL, "DAC2 Mux"},
	{"DACM", NULL, "DACM Clock"},
	{"DACM", NULL, "DACM Power"},

	{"PDM L Mux", "Stereo1 DAC", "Stereo DAC MIXL"},
	{"PDM L Mux", "DAC2", "DACL2"},
	{"PDM L Mux", "Sidetone", "Sidetone Mux"},
	{"PDM L Mux", NULL, "PDM CLK"},

	{"PDM R Mux", "Stereo1 DAC", "Stereo DAC MIXR"},
	{"PDM R Mux", "DAC2", "DACL2"},
	{"PDM R Mux", "Sidetone", "Sidetone Mux"},
	{"PDM R Mux", NULL, "PDM CLK"},

	{"HP Amp", NULL, "DACL"},
	{"HP Amp", NULL, "DACR"},
	{"HP Amp", NULL, "Vref1"},
	{"HP Amp", NULL, "Vref2"},
	{"HP Amp", NULL, "RC 1M"},
	{"HP Amp", NULL, "HP Clock"},
	{"HP Amp", NULL, "CLKDET HP"},

	{"HPOL", NULL, "HP Amp"},
	{"HPOR", NULL, "HP Amp"},

	{"Mono Amp", NULL, "DACM"},
	{"Mono Amp", NULL, "Vref1"},
	{"Mono Amp", NULL, "Vref2"},
	{"Mono Amp", NULL, "Vref3"},
	{"Mono Amp", NULL, "RC 1M"},
	{"Mono Amp", NULL, "CLKDET MONO"},
	{"Mono Playback", "Switch", "Mono Amp"},
	{"MONOOUT", NULL, "Mono Playback"},

	{"PDM L Playback", "Switch", "PDM L Mux"},
	{"PDM R Playback", "Switch", "PDM R Mux"},
	{"PDML", NULL, "PDM L Playback"},
	{"PDMR", NULL, "PDM R Playback"},
};

static int rt5691_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);
	int sys_div, osr_div, val = 0;

	rt5691->lrck = params_rate(params);
	sys_div = rt5691_calc_clk_div(rt5691, 256, 0);
	if (sys_div < 0) {
		dev_err(component->dev, "Unsupported clock setting %d for DAI %d\n",
			rt5691->lrck, dai->id);
		return -EINVAL;
	}

	dev_dbg(dai->dev, "lrck is %dHz and sys_div is %d for iis %d\n",
				rt5691->lrck, sys_div, dai->id);

	snd_soc_component_update_bits(component, RT5691_DA_STO1_FILTER_CLK_DIV,
		0x0f0f, ((sys_div + 1) << 8) | (sys_div + 1));
	snd_soc_component_update_bits(component, RT5691_AD_STO1_FILTER_CLK_DIV,
		0x0f0f, ((sys_div + 1) << 8) | (sys_div + 1));

	osr_div = rt5691_calc_clk_div(rt5691, 0, 12288000);

	snd_soc_component_update_bits(component, RT5691_ADC_AND_DAC_OSR,
		0x0f0f, ((osr_div + 1) << 8) | (osr_div + 1));

	switch (params_width(params)) {
	case 16:
		val = 1;
		break;

	case 20:
		val = 2;
		break;

	case 24:
		val = 3;
		break;

	case 32:
		val = 4;
		break;

	default:
		return -EINVAL;
	}

	snd_soc_component_update_bits(component, RT5691_I2S_CTRL_1,
		0x70, val << 4);

	return 0;
}

static int rt5691_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);
	unsigned int reg_val = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		rt5691->master = 1;
		break;

	case SND_SOC_DAIFMT_CBS_CFS:
		reg_val |= (0x1 << 3);
		rt5691->master = 0;
		break;

	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;

	case SND_SOC_DAIFMT_IB_NF:
		reg_val |= (0x1 << 13);
		break;

	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;

	case SND_SOC_DAIFMT_LEFT_J:
		reg_val |= 0x1;
		break;

	case SND_SOC_DAIFMT_DSP_A:
		reg_val |= 0x2;
		break;

	case SND_SOC_DAIFMT_DSP_B:
		reg_val |= 0x3;
		break;

	default:
		return -EINVAL;
	}

	snd_soc_component_update_bits(component, RT5691_I2S_CTRL_1,
		0x200f, reg_val);

	return 0;
}

static int rt5691_set_component_sysclk(struct snd_soc_component *component,
		int clk_id, int source, unsigned int freq, int dir)
{
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);
	unsigned int reg_val = 0;

	switch (clk_id) {
	case RT5691_SCLK_S_MCLK:
		break;

	case RT5691_SCLK_S_PLL1:
		reg_val |= (0x1 << 2);
		snd_soc_component_update_bits(component, RT5691_PLLA_CTRL_2,
			0xc400, 0xc400);
		snd_soc_component_update_bits(component, RT5691_PLLA_CTRL_3,
			0x4000, 0x4000);
		snd_soc_component_update_bits(component, RT5691_PLL_CTRL_2,
			0x1, 0x1);
		break;

	case RT5691_SCLK_S_PLL2:
		reg_val |= (0x2 << 2);
		break;

	case RT5691_SCLK_S_RCCLK:
		reg_val |= (0x3 << 2);
		break;

	default:
		dev_err(component->dev, "Invalid clock id (%d)\n", clk_id);
		return -EINVAL;
	}

	snd_soc_component_update_bits(component, RT5691_SYS_CLK_SRC,
		0xc, reg_val);

	rt5691->sysclk = freq;
	rt5691->sysclk_src = clk_id;

	dev_dbg(component->dev, "Sysclk is %dHz and clock id is %d\n", freq, clk_id);

	return 0;
}

static const struct rt5691_pll_calc_map rt5691_pll_preset_table[] = {
	{false, 19200000, 24576000, 3, 30, 3, false, false},
};

int rt5691_plla_calc(struct rt5691_priv *rt5691, const unsigned int freq_in,
	const unsigned int freq_out, struct rt5691_pll_code *pll_code)
{
	struct snd_soc_component *component = rt5691->component;
	int max_n = RT5691_PLL_N_MAX, max_m = RT5691_PLL_M_MAX;
	int k, red, n_t, pll_out, in_t, out_t;
	int n = 0, m = 0, m_t = 0;
	int red_t = abs(freq_out - freq_in);
	bool m_bypass = false, k_bypass = false;

	if (RT5691_PLL_INP_MAX < freq_in || RT5691_PLL_INP_MIN > freq_in)
		return -EINVAL;

	k = 100000000 / freq_out - 2;
	if (k > RT5691_PLL_K_MAX)
		k = RT5691_PLL_K_MAX;
	if (k < 0) {
		k = 0;
		k_bypass = true;
	}
	for (n_t = 0; n_t <= max_n; n_t++) {
		in_t = freq_in / (k_bypass ? 1 : (k + 2));
		pll_out = freq_out / (n_t + 2);
		if (in_t < 0)
			continue;
		if (in_t == pll_out) {
			m_bypass = true;
			n = n_t;
			goto code_find;
		}
		red = abs(in_t - pll_out);
		if (red < red_t) {
			m_bypass = true;
			n = n_t;
			m = m_t;
			if (red == 0)
				goto code_find;
			red_t = red;
		}
		for (m_t = 0; m_t <= max_m; m_t++) {
			out_t = in_t / (m_t + 2);
			red = abs(out_t - pll_out);
			if (red < red_t) {
				m_bypass = false;
				n = n_t;
				m = m_t;
				if (red == 0)
					goto code_find;
				red_t = red;
			}
		}
	}
	dev_dbg(component->dev, "Only get approximation about PLL\n");

code_find:

	pll_code->plla_m_bp = m_bypass;
	pll_code->plla_k_bp = k_bypass;
	pll_code->plla_m_code = m;
	pll_code->plla_n_code = n;
	pll_code->plla_k_code = k;

	return 0;
}

static int rt5691_set_component_pll(struct snd_soc_component *component,
		int pll_id, int source, unsigned int freq_in,
		unsigned int freq_out)
{
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);
	struct rt5691_pll_code pll_code = {0};
	int ret, i;

	if (source == rt5691->pll_src && freq_in == rt5691->pll_in &&
	    freq_out == rt5691->pll_out)
		return 0;

	if (!freq_in || !freq_out) {
		dev_dbg(component->dev, "PLL disabled\n");

		rt5691->pll_in = 0;
		rt5691->pll_out = 0;
		snd_soc_component_update_bits(component, RT5691_PLL_CTRL_2,
			0x30, 0x0);

		return 0;
	}

	switch (source) {
	case RT5691_PLL1_S_MCLK:
		snd_soc_component_update_bits(component, RT5691_PLL_CTRL_2,
			0x30, 0x0);
		break;

	case RT5691_PLL1_S_BCLK:
		snd_soc_component_update_bits(component, RT5691_PLL_CTRL_2,
			0x30, 0x10);

		break;

	default:
		dev_err(component->dev, "Unknown PLL Source %d\n", source);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(rt5691_pll_preset_table); i++) {
		if (freq_in == rt5691_pll_preset_table[i].pll_in &&
			freq_out == rt5691_pll_preset_table[i].pll_out) {
			pll_code.plla_k_code = rt5691_pll_preset_table[i].plla_k;
			pll_code.plla_m_code = rt5691_pll_preset_table[i].plla_m;
			pll_code.plla_n_code = rt5691_pll_preset_table[i].plla_n;
			pll_code.plla_m_bp = rt5691_pll_preset_table[i].plla_m_bp;
			pll_code.plla_k_bp = rt5691_pll_preset_table[i].plla_k_bp;

			if (rt5691_pll_preset_table[i].use_pllb) {
				pll_code.use_pllb = rt5691_pll_preset_table[i].use_pllb;
				pll_code.pllb_k_code = rt5691_pll_preset_table[i].pllb_k;
				pll_code.pllb_m_code = rt5691_pll_preset_table[i].pllb_m;
				pll_code.pllb_n_code = rt5691_pll_preset_table[i].pllb_n;
				pll_code.pllb_m_bp = rt5691_pll_preset_table[i].pllb_m_bp;
				pll_code.pllb_k_bp = rt5691_pll_preset_table[i].pllb_k_bp;
				pll_code.pllb_pulse = rt5691_pll_preset_table[i].pllb_pulse;
			}

			dev_dbg(component->dev, "Use preset PLL parameter table\n");

			goto set_pll;
		}
	}

	ret = rt5691_plla_calc(rt5691, freq_in, freq_out, &pll_code);
	if (ret < 0) {
		dev_err(component->dev, "Unsupport input clock %d\n", freq_in);
		return ret;
	}

set_pll:

	dev_dbg(component->dev, "PLLA m_bypass=%d m=%d n=%d k_bypass=%d k=%d\n",
		pll_code.plla_m_bp,
		(pll_code.plla_m_bp ? 0 : pll_code.plla_m_code),
		pll_code.plla_n_code, pll_code.plla_k_bp,
		(pll_code.plla_k_bp ? 0 : pll_code.plla_k_code));

	snd_soc_component_write(component, RT5691_PLLA_CTRL_1,
		(pll_code.plla_k_bp ? 0 : pll_code.plla_k_code) << 8 |
		(pll_code.plla_m_bp ? 0 : pll_code.plla_m_code) |
		pll_code.plla_m_bp << 14 | pll_code.plla_k_bp << 15);

	snd_soc_component_update_bits(component, RT5691_PLLA_CTRL_2,
		RT5691_PLL_N_MAX, pll_code.plla_n_code);

	if (pll_code.use_pllb) {
		dev_dbg(component->dev, "PLLB m_bypass=%d m=%d n=%d k_bypass=%d k=%d pulse=%d\n",
			pll_code.pllb_m_bp,
			(pll_code.pllb_m_bp ? 0 : pll_code.pllb_m_code),
			pll_code.pllb_n_code, pll_code.pllb_k_bp,
			(pll_code.pllb_k_bp ? 0 : pll_code.pllb_k_code),
			pll_code.pllb_pulse);

		snd_soc_component_write(component, RT5691_PLLB_CTRL_1,
			(pll_code.pllb_k_bp ? 0 : pll_code.pllb_k_code) << 8 |
			(pll_code.pllb_m_bp ? 0 : pll_code.pllb_m_code) |
			pll_code.pllb_m_bp << 14 | pll_code.pllb_k_bp << 15);

		snd_soc_component_update_bits(component, RT5691_PLLB_CTRL_2,
			RT5691_PLL_N_MAX, pll_code.pllb_n_code);

		snd_soc_component_update_bits(component, RT5691_PLLB_CTRL_4,
			1 << 15, pll_code.pllb_pulse << 15);
	}

	rt5691->pll_in = freq_in;
	rt5691->pll_out = freq_out;
	rt5691->pll_src = source;

	return 0;
}

static int rt5691_set_bias_level(struct snd_soc_component *component,
			enum snd_soc_bias_level level)
{
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		regmap_update_bits(rt5691->regmap, RT5691_SYS_CLK_SRC,
			0x41, 0x41);
		break;

	case SND_SOC_BIAS_STANDBY:
		regmap_update_bits(rt5691->regmap, RT5691_PWR_DIG_1,
			0x1 << 8, 0x1 << 8);
		regmap_update_bits(rt5691->regmap, RT5691_PWR_ANLG_1,
			RT5691_PWR_MB | RT5691_PWR_BG,
			RT5691_PWR_MB | RT5691_PWR_BG);
		regmap_update_bits(rt5691->regmap, RT5691_SYS_CLK_SRC, 0x41, 0);

		if (snd_soc_component_get_bias_level(component) == SND_SOC_BIAS_PREPARE) {
			regmap_update_bits(rt5691->regmap, RT5691_PLL_CTRL_2, 0x1, 0);
			snd_soc_component_update_bits(component, RT5691_PLLA_CTRL_2,
				0xc400, 0);
			snd_soc_component_update_bits(component, RT5691_PLLA_CTRL_3,
				0x4000, 0);
		}
		break;

	case SND_SOC_BIAS_OFF:
		regmap_update_bits(rt5691->regmap, RT5691_PWR_DIG_1,
			0x1 << 8, 0);
		regmap_update_bits(rt5691->regmap, RT5691_PWR_ANLG_1,
			RT5691_PWR_MB | RT5691_PWR_BG, 0);
		break;

	default:
		break;
	}

	return 0;
}

static int rt5691_probe(struct snd_soc_component *component)
{
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	rt5691->component = component;

	schedule_delayed_work(&rt5691->calibrate_work, msecs_to_jiffies(100));

	return ret;
}

static void rt5691_remove(struct snd_soc_component *component)
{
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	rt5691->cal_done = true;
	cancel_delayed_work_sync(&rt5691->calibrate_work);

	regmap_write(rt5691->regmap, RT5691_RESET, 0);
}

#ifdef CONFIG_PM
static int rt5691_suspend(struct snd_soc_component *component)
{
	dev_info(component->dev, "%s\n", __func__);
	return 0;
}

static int rt5691_resume(struct snd_soc_component *component)
{
	dev_info(component->dev, "%s\n", __func__);
	return 0;
}
#else
#define rt5691_suspend NULL
#define rt5691_resume NULL
#endif

#define RT5691_STEREO_RATES SNDRV_PCM_RATE_8000_192000
#define RT5691_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8 | \
		SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops rt5691_aif_dai_ops = {
	.hw_params = rt5691_hw_params,
	.set_fmt = rt5691_set_dai_fmt,
};

static struct snd_soc_dai_driver rt5691_dai[] = {
	{
		.name = "rt5691-aif",
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RT5691_STEREO_RATES,
			.formats = RT5691_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RT5691_STEREO_RATES,
			.formats = RT5691_FORMATS,
		},
		.ops = &rt5691_aif_dai_ops,
	},
};

static int rt5691_set_jack_detect(struct snd_soc_component *component,
	struct snd_soc_jack *hs_jack, void *data)
{
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	if (!rt5691->pdata.jd_src)
		return 0;

	regmap_update_bits(rt5691->regmap, RT5691_BGLDO33_CTRL_1,
		0x1 << 15, 0x1 << 15);
	regmap_update_bits(rt5691->regmap, RT5691_MULTI_FUNC_PIN_CTRL_1,
		0x1 << 15, 0x1 << 15);
	regmap_update_bits(rt5691->regmap, RT5691_RC_CLK_CTRL,
		0xf << 12, 0xf << 12);
	regmap_write(rt5691->regmap, RT5691_IRQ_CTRL_6, 0x6102);

	switch (rt5691->pdata.jd_src) {
	case RT5691_JD1:
		regmap_update_bits(rt5691->regmap, RT5691_WATER_DET_CTRL_1,
			0x1 << 8, 0x1 << 8);
		regmap_update_bits(rt5691->regmap, RT5691_IRQ_CTRL_1,
			0x1 << 3, 0x1 << 3);
		break;

	default:
		dev_warn(component->dev, "Wrong JD source\n");
		break;
	}

	rt5691->hs_jack = hs_jack;

	return 0;
}

static int rt5691_i2c_read(void *context, unsigned int reg, unsigned int *val)
{
	struct i2c_client *i2c = context;
	struct rt5691_priv *rt5691 = i2c_get_clientdata(i2c);
	struct rt5691_i2c_err *i2c_err = rt5691->i2c_err;
	int ret, i;

	for (i = 0; i < rt5691->pdata.i2c_op_count; i++) {
		ret = regmap_read(rt5691->i2c_regmap, reg, val);
		if (ret) {
			dev_err(&i2c->dev, "%s err: %d\n", __func__, ret);
			if (i2c_err && i2c_err->i2c_err_cb)
				i2c_err->i2c_err_cb(i2c_err->i2c_err_priv);
			continue;
		} else {
			break;
		}
	}

	return ret;
}

static int rt5691_i2c_write(void *context, unsigned int reg, unsigned int val)
{
	struct i2c_client *i2c = context;
	struct rt5691_priv *rt5691 = i2c_get_clientdata(i2c);
	struct rt5691_i2c_err *i2c_err = rt5691->i2c_err;
	int ret, i;

	for (i = 0; i < rt5691->pdata.i2c_op_count; i++) {
		ret = regmap_write(rt5691->i2c_regmap, reg, val);
		if (ret) {
			dev_err(&i2c->dev, "%s err: %d\n", __func__, ret);
			if (i2c_err && i2c_err->i2c_err_cb)
				i2c_err->i2c_err_cb(i2c_err->i2c_err_priv);
			continue;
		} else {
			break;
		}
	}

	return ret;
}

const struct snd_soc_component_driver rt5691_soc_component_dev = {
	.probe = rt5691_probe,
	.remove = rt5691_remove,
	.suspend = rt5691_suspend,
	.resume = rt5691_resume,
	.set_bias_level = rt5691_set_bias_level,
	.controls = rt5691_snd_controls,
	.num_controls = ARRAY_SIZE(rt5691_snd_controls),
	.dapm_widgets = rt5691_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt5691_dapm_widgets),
	.dapm_routes = rt5691_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt5691_dapm_routes),
	.set_sysclk = rt5691_set_component_sysclk,
	.set_pll = rt5691_set_component_pll,
	.set_jack = rt5691_set_jack_detect,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

static const struct regmap_config rt5691_i2c_regmap = {
	.name = "i2c",
	.reg_bits = 16,
	.val_bits = 16,
	.cache_type = REGCACHE_NONE,
};

static const struct regmap_config rt5691_debug_regmap = {
	.reg_bits = 16,
	.val_bits = 16,
	.max_register = RT5691_HP_AMP_DET_CTRL_20,
	.volatile_reg = rt5691_volatile_register,
	.readable_reg = rt5691_readable_register,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = rt5691_reg,
	.num_reg_defaults = ARRAY_SIZE(rt5691_reg),
	.use_single_read = true,
	.use_single_write = true,
	.reg_read = rt5691_i2c_read,
	.reg_write = rt5691_i2c_write,
};

static const struct regmap_config rt5691_regmap = {
	.reg_bits = 16,
	.val_bits = 16,
	.max_register = RT5691_HP_AMP_DET_CTRL_20,
	.volatile_reg = rt5691_volatile_register,
	.readable_reg = rt5691_readable_register,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = rt5691_reg,
	.num_reg_defaults = ARRAY_SIZE(rt5691_reg),
	.use_single_read = true,
	.use_single_write = true,
};

static const struct i2c_device_id rt5691_i2c_id[] = {
	{"rt5691", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, rt5691_i2c_id);

static int rt5691_parse_dt(struct rt5691_priv *rt5691, struct device *dev)
{
	int i, i2c_retry = 0;
	u32 imp_data[ARRAY_SIZE(rt5691_hp_gain_table) * 2];

	rt5691->pdata.in1_diff = of_property_read_bool(dev->of_node,
					"realtek,in1-differential");
	rt5691->pdata.in2_diff = of_property_read_bool(dev->of_node,
					"realtek,in2-differential");
	rt5691->pdata.in3_diff = of_property_read_bool(dev->of_node,
					"realtek,in3-differential");

	rt5691->pdata.ldo1_en = of_get_named_gpio(dev->of_node,
		"realtek,ldo1_en", 0);
	rt5691->pdata.gpio_1v8 = of_get_named_gpio(dev->of_node,
		"realtek,gpio_1v8", 0);
	rt5691->pdata.gpio_3v3 = of_get_named_gpio(dev->of_node,
		"realtek,gpio_3v3", 0);

	of_property_read_u32(dev->of_node, "realtek,gpio2-func",
		&rt5691->pdata.gpio2_func);
	of_property_read_u32(dev->of_node, "realtek,gpio3-func",
		&rt5691->pdata.gpio3_func);
	of_property_read_u32(dev->of_node, "realtek,jd-src",
		&rt5691->pdata.jd_src);
	of_property_read_u32(dev->of_node, "realtek,jd-resistor",
		&rt5691->pdata.jd_resistor);

	of_property_read_u32(dev->of_node, "realtek,delay-plug-in",
		&rt5691->pdata.delay_plug_in);
	of_property_read_u32(dev->of_node, "realtek,delay-plug-out-pb",
		&rt5691->pdata.delay_plug_out_pb);

	of_property_read_u32(dev->of_node, "realtek,sar-hs-type",
		&rt5691->pdata.sar_hs_type);
	of_property_read_u32(dev->of_node, "realtek,sar-hs-open-gender",
		&rt5691->pdata.sar_hs_open_gender);
	of_property_read_u32(dev->of_node, "realtek,sar-pb-vth0",
		&rt5691->pdata.sar_pb_vth0);
	of_property_read_u32(dev->of_node, "realtek,sar-pb-vth1",
		&rt5691->pdata.sar_pb_vth1);
	of_property_read_u32(dev->of_node, "realtek,sar-pb-vth2",
		&rt5691->pdata.sar_pb_vth2);
	of_property_read_u32(dev->of_node, "realtek,sar-pb-vth3",
		&rt5691->pdata.sar_pb_vth3);

	if (!of_property_read_u32_array(dev->of_node, "realtek,imp_table",
		imp_data, (ARRAY_SIZE(rt5691_hp_gain_table) * 2))) {

		for (i = 0; i < ARRAY_SIZE(rt5691_hp_gain_table); i++) {
			rt5691_hp_gain_table[i].imp = imp_data[i * 2];
			rt5691_hp_gain_table[i].gain = imp_data[(i * 2) + 1];
		}
	}

	if (of_property_read_bool(dev->of_node, "realtek,i2c-dbg-cb"))
		rt5691->pdata.i2c_op_count = 1;

	of_property_read_u32(dev->of_node, "realtek,i2c-dbg-retry",
		&i2c_retry);

	if (i2c_retry)
		rt5691->pdata.i2c_op_count = i2c_retry + 1;

	of_property_read_u32(dev->of_node, "realtek,button-clk",
		&rt5691->pdata.button_clk);
	of_property_read_u32(dev->of_node, "realtek,hpa-capless-bias",
		&rt5691->pdata.hpa_capless_bias);

	return 0;
}

static void rt5691_enable_push_button_irq(struct snd_soc_component *component,
	bool enable)
{
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);

	if (enable) {
		snd_soc_component_write(component, RT5691_MIC_BTN_CTRL_16,
			rt5691->pdata.button_clk);
		snd_soc_component_write(component, RT5691_MIC_BTN_CTRL_17, 0x3);
		snd_soc_component_update_bits(component,
			RT5691_SAR_ADC_DET_CTRL_4, 0x8, 0x8);
		snd_soc_component_update_bits(component, RT5691_PWR_DA_PATH_2, 0x40, 0x40);
		if (rt5691->open_gender)
			msleep(20);
		snd_soc_component_update_bits(component, RT5691_MIC_BTN_CTRL_15, 0x80, 0x80);
		snd_soc_component_update_bits(component, RT5691_IRQ_CTRL_3, 0x40, 0x40);
	} else {
		snd_soc_component_update_bits(component, RT5691_IRQ_CTRL_3, 0x40, 0);
		snd_soc_component_update_bits(component, RT5691_MIC_BTN_CTRL_15, 0x80, 0);
		snd_soc_component_update_bits(component, RT5691_PWR_DA_PATH_2, 0x40, 0);
	}
}

static int rt5691_water_detect(struct snd_soc_component *component,
	bool enable)
{
	struct snd_soc_dapm_context *dapm = &component->dapm;
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);
	static struct reg_sequence rt5691_wt_list[] = {
		{RT5691_MIC_BTN_CTRL_26,		0x0088},
		{RT5691_ADC_FILTER_CTRL_3,		0x0090},
		{RT5691_ADC_FILTER2_CTRL_3,		0x0090},
		{RT5691_HP_AMP_L_GAIN_CTRL,		0x0000},
		{RT5691_HP_AMP_R_GAIN_CTRL,		0x0000},
		{RT5691_SYS_CLK_SRC,			0x006d},
		{RT5691_DA_STO1_FILTER_CLK_DIV,		0x0202},
		{RT5691_ADC_AND_DAC_OSR,		0x0202},
		{RT5691_PWR_DA_PATH_1,			0x2001},
		{RT5691_PWR_DA_PATH_2,			0x0020},
		{RT5691_STO1_DAC_MIXER_CTRL,		0xa8a8},
		{RT5691_HP_AMP_CTRL_2,			0xe000},
		{RT5691_HP_AMP_DET_CTRL_18,		0x0008},
		{RT5691_HP_AMP_DET_CTRL_14,		0x3040},
	};
	unsigned int rt5691_wt_list_saved[ARRAY_SIZE(rt5691_wt_list)];
	int i;

	if (enable) {
		snd_soc_dapm_force_enable_pin(dapm, "Vref1");
		snd_soc_dapm_force_enable_pin(dapm, "Vref2");
		snd_soc_dapm_force_enable_pin(dapm, "RC 1M");
		snd_soc_dapm_force_enable_pin(dapm, "RC 25M");
		snd_soc_dapm_sync(dapm);
		snd_soc_dapm_mutex_lock(dapm);

		for (i = 0; i < ARRAY_SIZE(rt5691_wt_list); i++) {
			rt5691_wt_list_saved[i] =
				snd_soc_component_read(component, rt5691_wt_list[i].reg);
			snd_soc_component_write(component, rt5691_wt_list[i].reg,
				rt5691_wt_list[i].def);
			if (rt5691_wt_list[i].reg == RT5691_HP_AMP_DET_CTRL_18)
				usleep_range(10000, 10005);
		}

		regmap_update_bits(rt5691->regmap, RT5691_PWR_DA_PATH_2, 0x1100,
			0x1100);

		regmap_update_bits(rt5691->regmap, RT5691_WATER_DET_CTRL_3, 0x8000,
			0x8000);

		for (i = 0; i < 25; i++) {
			if (!(snd_soc_component_read(component,
				RT5691_WATER_DET_CTRL_3) & 0x4))
				msleep(20);
			else
				break;
		}

		if ((snd_soc_component_read(component,
			RT5691_WATER_DET_CTRL_3) & 0x3) == 0x2) {
			regmap_update_bits(rt5691->regmap, RT5691_WATER_DET_CTRL_3, 0x8000,
				0);

			snd_soc_component_update_bits(component, RT5691_HP_AMP_CTRL_2,
				0x3000, rt5691_wt_list_saved[ARRAY_SIZE(rt5691_wt_list) - 3]);
			snd_soc_component_update_bits(component,
				RT5691_HP_AMP_DET_CTRL_14, 0x0070,
				rt5691_wt_list_saved[ARRAY_SIZE(rt5691_wt_list) - 1]);
			snd_soc_component_update_bits(component,
				RT5691_HP_AMP_DET_CTRL_18, 0x000c,
				rt5691_wt_list_saved[ARRAY_SIZE(rt5691_wt_list) - 2]);

			for (i = ARRAY_SIZE(rt5691_wt_list) - 4; i >= 0; i--)
				snd_soc_component_write(component, rt5691_wt_list[i].reg,
					rt5691_wt_list_saved[i]);

			snd_soc_dapm_mutex_unlock(dapm);
			snd_soc_dapm_disable_pin(dapm, "Vref1");
			snd_soc_dapm_disable_pin(dapm, "Vref2");
			snd_soc_dapm_disable_pin(dapm, "RC 1M");
			snd_soc_dapm_disable_pin(dapm, "RC 25M");
			snd_soc_dapm_sync(dapm);
		} else {
			dev_info(component->dev, "water detected\n");

			regmap_update_bits(rt5691->regmap, RT5691_IRQ_CTRL_4,
				0x0008, 0x0008);
			rt5691->wt_en = true;

			snd_soc_component_update_bits(component, RT5691_HP_AMP_CTRL_2,
				0x3000, rt5691_wt_list_saved[ARRAY_SIZE(rt5691_wt_list) - 3]);
			snd_soc_component_update_bits(component,
				RT5691_HP_AMP_DET_CTRL_14, 0x0070,
				rt5691_wt_list_saved[ARRAY_SIZE(rt5691_wt_list) - 1]);
			snd_soc_component_update_bits(component,
				RT5691_HP_AMP_DET_CTRL_18, 0x000c,
				rt5691_wt_list_saved[ARRAY_SIZE(rt5691_wt_list) - 2]);

			for (i = ARRAY_SIZE(rt5691_wt_list) - 4; i >= 0; i--) {
				if (rt5691_wt_list[i].reg == RT5691_PWR_DA_PATH_2)
					rt5691_wt_list_saved[i] |= 0x1100;

				snd_soc_component_write(component, rt5691_wt_list[i].reg,
					rt5691_wt_list_saved[i]);
			}

			snd_soc_dapm_mutex_unlock(dapm);
			snd_soc_dapm_disable_pin(dapm, "Vref1");
			snd_soc_dapm_disable_pin(dapm, "Vref2");
			snd_soc_dapm_disable_pin(dapm, "RC 25M");
			snd_soc_dapm_sync(dapm);
		}
	} else {
		regmap_update_bits(rt5691->regmap, RT5691_IRQ_CTRL_4, 0x0008, 0);
		regmap_update_bits(rt5691->regmap, RT5691_WATER_DET_CTRL_3,
			0x8000, 0);
		regmap_update_bits(rt5691->regmap, RT5691_PWR_DA_PATH_2, 0x1100,
			0);
		snd_soc_dapm_disable_pin(dapm, "RC 1M");
		snd_soc_dapm_sync(dapm);
		rt5691->wt_en = false;
	}

	return 0;
}

static int rt5691_headset_detect(struct snd_soc_component *component, int jack_insert)
{
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm = &component->dapm;
	unsigned int sar_hs_type;

	if (jack_insert) {
		regmap_update_bits(rt5691->regmap, RT5691_WATER_DET_CTRL_2, 0xf0, 0);
		regmap_update_bits(rt5691->regmap, RT5691_IRQ_CTRL_1, 0x8, 0);
		regmap_update_bits(rt5691->regmap, RT5691_COMBO_JACK_CTRL_5, 0xc007, 0xc005);
		regmap_update_bits(rt5691->regmap, RT5691_COMBO_JACK_CTRL_4, 0x600, 0x600);
		regmap_update_bits(rt5691->regmap, RT5691_PWR_DA_PATH_2, 0x480, 0x480);
		snd_soc_dapm_force_enable_pin(dapm, "MICBIAS1");
		snd_soc_dapm_sync(dapm);

		regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_2, 0x2c);
		regmap_write(rt5691->regmap, RT5691_JACK_TYPE_DET_CTRL_2, 0xfc00);
		regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_1, 0);

		msleep(20);

		rt5691->adc_val = snd_soc_component_read(component,
			RT5691_SAR_ADC_DET_CTRL_23);

		sar_hs_type = rt5691->pdata.sar_hs_type ?
			rt5691->pdata.sar_hs_type : 729;

		dev_info(component->dev, "sar_adc_value = %d\n", rt5691->adc_val);

		if (rt5691->pdata.sar_hs_open_gender) {
			if (rt5691->adc_val > rt5691->pdata.sar_hs_open_gender) {
				rt5691->jack_type = 0;
				rt5691->open_gender = true;
				regmap_update_bits(rt5691->regmap,
					RT5691_COMBO_JACK_CTRL_2, 0x8000,
					0x8000);
				regmap_update_bits(rt5691->regmap,
					RT5691_COMBO_JACK_CTRL_3, 0x00f3, 0x00e2);
				regmap_update_bits(rt5691->regmap,
					RT5691_IRQ_CTRL_1, 0x0004, 0);
				regmap_update_bits(rt5691->regmap,
					RT5691_IRQ_CTRL_2, 0x30, 0x20);
				regmap_write(rt5691->regmap,
					RT5691_SAR_ADC_DET_CTRL_2, 0x20);
				rt5691_water_detect(component, false);
				regmap_update_bits(rt5691->regmap,
					RT5691_WATER_DET_CTRL_2, 0xf0, 0x90);
				regmap_update_bits(rt5691->regmap,
					RT5691_IRQ_CTRL_1, 0x8, 0x8);
				dev_info(component->dev, "jack_type = open gender\n");
				return rt5691->jack_type;
			}
		}

		if (rt5691->adc_val > sar_hs_type) {
			rt5691->jack_type = SND_JACK_HEADSET;
			if (!rt5691->open_gender) {
				regmap_update_bits(rt5691->regmap,
					RT5691_IRQ_CTRL_1, 0x0004, 0x0004);
				regmap_update_bits(rt5691->regmap,
					RT5691_IRQ_CTRL_2, 0x0018, 0x0018);
				regmap_write(rt5691->regmap, RT5691_INT_ST_1, 0);
				regmap_update_bits(rt5691->regmap,
					RT5691_COMBO_JACK_CTRL_3, 0x00f3, 0x00e1);
				regmap_update_bits(rt5691->regmap,
					RT5691_MONO_ANLG_DRE_CTRL_2, 0x0600, 0x0600);
				if (rt5691->adc_val > 2000) {
					regmap_update_bits(rt5691->regmap,
						RT5691_COMBO_JACK_CTRL_5, 0xc007, 0xc005);
				} else {
					regmap_update_bits(rt5691->regmap,
						RT5691_COMBO_JACK_CTRL_2, 0x8000, 0x8000);
					regmap_update_bits(rt5691->regmap,
						RT5691_COMBO_JACK_CTRL_5, 0xc007, 0xc004);
				}
				regmap_update_bits(rt5691->regmap,
					RT5691_COMBO_JACK_CTRL_4, 0x600, 0x600);
				regmap_update_bits(rt5691->regmap,
					RT5691_PWR_DA_PATH_2, 0x400, 0x400);
			}
			rt5691_enable_push_button_irq(component, true);
		} else {
			rt5691->jack_type = SND_JACK_HEADPHONE;
			if (!rt5691->open_gender) {
				regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_2, 0x20);
				regmap_update_bits(rt5691->regmap, RT5691_COMBO_JACK_CTRL_4, 0x400, 0);
				regmap_update_bits(rt5691->regmap, RT5691_MONO_ANLG_DRE_CTRL_2, 0x0600, 0);
				regmap_update_bits(rt5691->regmap, RT5691_PWR_DA_PATH_2, 0x480, 0);
				snd_soc_dapm_disable_pin(dapm, "MICBIAS1");
				snd_soc_dapm_sync(dapm);
			}
		}

		regmap_update_bits(rt5691->regmap, RT5691_WATER_DET_CTRL_2, 0xf0, 0x90);
		regmap_update_bits(rt5691->regmap, RT5691_IRQ_CTRL_1, 0x8, 0x8);
	} else {
		if (!rt5691->open_gender) {
			snd_soc_dapm_disable_pin(dapm, "MICBIAS1");
			snd_soc_dapm_sync(dapm);

			regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_2, 0x20);
			regmap_update_bits(rt5691->regmap, RT5691_COMBO_JACK_CTRL_4, 0x400, 0);
			regmap_update_bits(rt5691->regmap, RT5691_MONO_ANLG_DRE_CTRL_2, 0x0600, 0);
			regmap_update_bits(rt5691->regmap, RT5691_PWR_DA_PATH_2, 0x480, 0);
		}

		if (rt5691->jack_type == SND_JACK_HEADSET)
			rt5691_enable_push_button_irq(component, false);

		rt5691->jack_type = 0;
	}

	dev_info(component->dev, "jack_type = %d\n", rt5691->jack_type);

	return rt5691->jack_type;
}

static int rt5691_button_detect(struct snd_soc_component *component)
{
	int reg1, reg2;

	reg1 = snd_soc_component_read(component, RT5691_MIC_BTN_CTRL_13);
	snd_soc_component_write(component, RT5691_MIC_BTN_CTRL_13, reg1);

	reg2 = snd_soc_component_read(component, RT5691_MIC_BTN_CTRL_14);
	snd_soc_component_write(component, RT5691_MIC_BTN_CTRL_14, reg2);

	return (reg1 << 8) | reg2;
}

static unsigned int rt5691_imp_detect(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm = &component->dapm;
	struct rt5691_priv *rt5691 = snd_soc_component_get_drvdata(component);
	static struct reg_sequence rt5691_imp_list[] = {
		{RT5691_MIC_BTN_CTRL_26,		0x0088},
		{RT5691_ADC_FILTER_CTRL_3,		0x0090},
		{RT5691_ADC_FILTER2_CTRL_3,		0x0090},
		{RT5691_HP_AMP_L_GAIN_CTRL,		0x0000},
		{RT5691_HP_AMP_R_GAIN_CTRL,		0x0000},
		{RT5691_RC_CLK,				0x8800},
		{RT5691_SYS_CLK_SRC,			0x007d},
		{RT5691_DA_STO1_FILTER_CLK_DIV,		0x0202},
		{RT5691_AD_STO1_FILTER_CLK_DIV,		0x0202},
		{RT5691_ADC_AND_DAC_OSR,		0x0202},
		{RT5691_PWR_DA_PATH_1,			0x2001},
		{RT5691_PWR_DA_PATH_2,			0x0013},
		{RT5691_PWR_AD_PATH,			0x0002},
		{RT5691_ADC56_CTRL,			0xb490},
		{RT5691_STO1_DAC_MIXER_CTRL,		0x2888},
		{RT5691_STO2_ADC_MIXER_CTRL,		0x6c6c},
		{RT5691_HP_BEHAVIOR_LOGIC_CTRL_2,	0x0001},
		{RT5691_ADC_FILTER2_CTRL_6,		0x0003},
		{RT5691_IMP_SENS_CTRL_8,		0x2000},
		{RT5691_IMP_SENS_CTRL_24,		0x1907},
		{RT5691_IMP_SENS_CTRL_1,		0xa000},
	};
	unsigned int rt5691_imp_list_saved[ARRAY_SIZE(rt5691_imp_list)];
	int i;

	snd_soc_dapm_force_enable_pin(dapm, "Vref1");
	snd_soc_dapm_force_enable_pin(dapm, "Vref2");
	snd_soc_dapm_sync(dapm);

	snd_soc_dapm_mutex_lock(dapm);

	for (i = 0; i < ARRAY_SIZE(rt5691_imp_list); i++) {
		rt5691_imp_list_saved[i] =
			snd_soc_component_read(component, rt5691_imp_list[i].reg);
		snd_soc_component_write(component, rt5691_imp_list[i].reg,
			rt5691_imp_list[i].def);
	}

	for (i = 0; i < 50; i++) {
		msleep(20);
		if (!(snd_soc_component_read(component, RT5691_IMP_SENS_CTRL_1) & 0x8000))
			break;
	}

	rt5691->imp_value = snd_soc_component_read(component, RT5691_IMP_SENS_CTRL_25);

	for (i = ARRAY_SIZE(rt5691_imp_list) - 1; i >= 0; i--)
		snd_soc_component_write(component, rt5691_imp_list[i].reg,
			rt5691_imp_list_saved[i]);

	snd_soc_dapm_mutex_unlock(dapm);

	snd_soc_dapm_disable_pin(dapm, "Vref1");
	snd_soc_dapm_disable_pin(dapm, "Vref2");
	snd_soc_dapm_sync(dapm);

	for (i = 0; i < ARRAY_SIZE(rt5691_hp_gain_table); i++) {
		if (rt5691->imp_value <= rt5691_hp_gain_table[i].imp) {
			rt5691->imp_gain = rt5691_hp_gain_table[i].gain;
			break;
		}
	}

	dev_dbg(component->dev, "[%d] SET GAIN %d for 0x%x Impedance value\n",
			i, rt5691->imp_gain, rt5691->imp_value);

	return 0;
}

static void rt5691_jack_detect_handler(struct work_struct *work)
{
	struct rt5691_priv *rt5691 =
		container_of(work, struct rt5691_priv, jack_detect_work.work);
	struct snd_soc_component *component = rt5691->component;
	int val, btn_type, mask, i;

	pm_stay_awake(component->dev);

	dev_info(component->dev, "%s\n", __func__);

	if (rt5691->is_suspend) {
		dev_info(component->dev, "%s wait resume\n", __func__);
		i = 0;
		while (i < 10 && rt5691->is_suspend) {
			msleep(50);
			i++;
		}
	}

	rt5691->mic_check_break = true;
	cancel_delayed_work_sync(&rt5691->mic_check_work);

	mask = 0x8000;

	i = 0;
	while (regmap_read(rt5691->regmap, RT5691_ANLG_READ_STA_324, &val) && i < 5) {
		msleep(100);
		i++;
	}

	dev_info(component->dev, "JD state = 0x%04x\n", val);

	if (!(val & mask)) {
		if (rt5691->open_gender) {
			if (val & 0x2000) {
				rt5691->irq_work_delay =
					rt5691->pdata.delay_plug_out_pb ?
					rt5691->pdata.delay_plug_out_pb + 500 :
					500;

				if (rt5691->jack_type) {
					/* jack out */
					rt5691->jack_type = rt5691_headset_detect(rt5691->component, 0);
#ifdef CONFIG_SWITCH
					switch_set_state(&rt5691_headset_switch, 0);
#endif
					snd_soc_jack_report(rt5691->hs_jack, rt5691->jack_type,
							SND_JACK_HEADSET |
							SND_JACK_BTN_0 | SND_JACK_BTN_1 |
							SND_JACK_BTN_2 | SND_JACK_BTN_3);

					dev_info(component->dev, "open gender jack out\n");
				}

				pm_wakeup_event(component->dev, 1000);
				return;
			}
		} else if (!rt5691->wt_en && !rt5691->jack_type) {
			rt5691_water_detect(component, true);
		}

		if (rt5691->wt_en) {
			regmap_read(rt5691->regmap, RT5691_WATER_DET_CTRL_3, &val);

			if (val & 0x2) {
				rt5691_water_detect(component, false);
			} else {
				dev_info(component->dev, "water detected 0x%04x\n",
					val);
				pm_wakeup_event(component->dev, 1000);
				return;
			}
		}

		/* jack in */
		if (rt5691->jack_type == 0) {
			/* jack was out, report jack type */
			rt5691_imp_detect(rt5691->component);

			rt5691->jack_type = rt5691_headset_detect(
				rt5691->component, 1);

			regmap_read(rt5691->regmap, RT5691_ANLG_READ_STA_324, &val);
			if (val & mask) {
				queue_delayed_work(system_wq, &rt5691->jack_detect_work,
					msecs_to_jiffies(50));
				pm_wakeup_event(component->dev, 1000);
				return;
			}

			rt5691->irq_work_delay =
				rt5691->pdata.delay_plug_out_pb ?
				rt5691->pdata.delay_plug_out_pb : 0;

			if (rt5691->jack_type == SND_JACK_HEADSET) {
#ifdef CONFIG_SWITCH
				switch_set_state(&rt5691_headset_switch, 1);
#endif
				rt5691->button_timeout = jiffies + (HZ * 1);
			} else if (rt5691->jack_type == SND_JACK_HEADPHONE) {
#ifdef CONFIG_SWITCH
				switch_set_state(&rt5691_headset_switch, 2);
#endif
				if (!rt5691->open_gender) {
					schedule_delayed_work(&rt5691->mic_check_work,
						msecs_to_jiffies(40));
				}
			} else {
#ifdef CONFIG_SWITCH
				switch_set_state(&rt5691_headset_switch, 0);
#endif
				rt5691->irq_work_delay += 500;
			}
		} else {
			if ((rt5691->jack_type & SND_JACK_HEADSET) ==
				SND_JACK_HEADSET) {
				/* jack is already in, report button event */
				rt5691->jack_type = SND_JACK_HEADSET;
				btn_type = rt5691_button_detect(rt5691->component);

				if (!rt5691->open_gender) {
					regmap_read(rt5691->regmap, RT5691_INT_ST_1, &val);
					if (val & 0x2000) {
						regmap_write(rt5691->regmap, RT5691_INT_ST_1, 0);
						btn_type = 0;
						dev_info(component->dev, "JD1 trigger\n");

						msleep(200);
						for (i = 0; i < 16; i++) {
							if (!rt5691_button_detect(rt5691->component))
								break;
							msleep(50);
						}
					} else if (val & 0x0001) {
						regmap_write(rt5691->regmap, RT5691_INT_ST_1, 0);
						btn_type = 0;
						dev_info(component->dev, "JD3 trigger\n");
					}
				}

				if (time_before(jiffies, rt5691->button_timeout)) {
					btn_type = 0;
					dev_warn(rt5691->component->dev,
						"%s: invalid button event\n",
						__func__);
				}

				rt5691->adc_val = snd_soc_component_read(component,
					RT5691_SAR_ADC_DET_CTRL_23);

				switch (btn_type) {
				case 0x4000:
				case 0x2000:
				case 0x1000:
					rt5691->jack_type |= SND_JACK_BTN_0;
					break;
				case 0x0400:
				case 0x0200:
				case 0x0100:
					rt5691->jack_type |= SND_JACK_BTN_1;
					break;
				case 0x0040:
				case 0x0020:
				case 0x0010:
					rt5691->jack_type |= SND_JACK_BTN_2;
					break;
				case 0x0004:
				case 0x0002:
				case 0x0001:
					rt5691->jack_type |= SND_JACK_BTN_3;
					break;
				case 0x0000: /* unpressed */
					if (snd_soc_component_read(component,
						RT5691_SAR_ADC_DET_CTRL_23) > 2000) {
						regmap_update_bits(rt5691->regmap,
							RT5691_COMBO_JACK_CTRL_5,
							0xc007, 0xc005);
						regmap_update_bits(rt5691->regmap,
							RT5691_COMBO_JACK_CTRL_2,
							0x8000, 0);
						regmap_write(rt5691->regmap,
							RT5691_INT_ST_1, 0);
					}
					break;
				default:
					btn_type = 0;
					dev_err(rt5691->component->dev,
						"Unexpected button code 0x%04x\n",
						btn_type);
					break;
				}

				dev_info(component->dev, "jack_type = 0x%04x\n",
					rt5691->jack_type);

				rt5691->btn_det =
					(rt5691->jack_type & SND_JACK_BTN_0)
					== SND_JACK_BTN_0 ? 1 : 0;
			} else {
				dev_err(component->dev, "%s invalid event\n",
					__func__);
			}

		}
	} else {
		/* jack out */
		rt5691->open_gender = false;
		rt5691->jack_type = rt5691_headset_detect(rt5691->component, 0);
#ifdef CONFIG_SWITCH
		switch_set_state(&rt5691_headset_switch, 0);
#endif
		rt5691_water_detect(component, false);
		regmap_update_bits(rt5691->regmap, RT5691_COMBO_JACK_CTRL_2, 0x8000, 0);
		regmap_update_bits(rt5691->regmap, RT5691_IRQ_CTRL_2, 0x20, 0);
		regmap_update_bits(rt5691->regmap, RT5691_IRQ_CTRL_1, 0x0004, 0);

		if (rt5691->pdata.delay_plug_in)
			rt5691->irq_work_delay =
				rt5691->pdata.delay_plug_in;
		else
			rt5691->irq_work_delay = 50;

		rt5691->adc_val = 0;
	}

	snd_soc_jack_report(rt5691->hs_jack, rt5691->jack_type,
			SND_JACK_HEADSET |
			SND_JACK_BTN_0 | SND_JACK_BTN_1 |
			SND_JACK_BTN_2 | SND_JACK_BTN_3);

	pm_wakeup_event(component->dev, 1000);
}

static irqreturn_t rt5691_irq(int irq, void *data)
{
	struct rt5691_priv *rt5691 = data;
	struct snd_soc_component *component = rt5691->component;

	dev_info(component->dev, "%s\n", __func__);
	pm_wakeup_event(component->dev, 3000);
	cancel_delayed_work_sync(&rt5691->jack_detect_work);
	queue_delayed_work(system_wq, &rt5691->jack_detect_work,
		msecs_to_jiffies(rt5691->irq_work_delay));

	return IRQ_HANDLED;
}

static void rt5691_calibrate(struct rt5691_priv *rt5691)
{
	struct snd_soc_component *component = rt5691->component;
	struct snd_soc_dapm_context *dapm = &component->dapm;
	static struct reg_sequence rt5691_cal_list[] = {
		{0x008e, 0xf000},
		{0x0090, 0x3480},
		{0x00d0, 0x0001},
		{0x0213, 0x0001},
		{0x3b00, 0x302b},
		{0x3c00, 0x0003},
		{0x013a, 0x1080},
		{0x013b, 0x1080},
		{0x013c, 0x1080},
		{0x0062, 0x2008},
		{0x2b03, 0x1004},
		{0x0061, 0x3dbf},
		{0x0063, 0xaa80},
		{0x0216, 0x8800},
		{0x0205, 0x007d},
		{0x0209, 0xfff3},
		{0x020a, 0xffff},
		{0x020b, 0xffff},
		{0x0200, 0x0202},
		{0x0201, 0x0202},
		{0x0206, 0x0202},
		{0x01db, 0x0000},
		{0x0066, 0x8000},
		{0x013f, 0xb490},
		{0x0194, 0x0000},
	};
	unsigned int rt5691_cal_list_saved[ARRAY_SIZE(rt5691_cal_list)];
	int i, ret;

	snd_soc_dapm_mutex_lock(dapm);

	regcache_cache_bypass(rt5691->regmap, true);

	regmap_write(rt5691->regmap, RT5691_RESET, 0);

	if (rt5691->pdata.hpa_capless_bias)
		regmap_write(rt5691->regmap, RT5691_ANLG_BIAS_CTRL_4,
			rt5691->pdata.hpa_capless_bias);

	for (i = 0; i < ARRAY_SIZE(rt5691_cal_list); i++)
		rt5691_cal_list_saved[i] =
			snd_soc_component_read(component, rt5691_cal_list[i].reg);

	regmap_multi_reg_write(rt5691->regmap,
		rt5691_cal_list, ARRAY_SIZE(rt5691_cal_list));

	msleep(20);

	regmap_write(rt5691->regmap, RT5691_PWR_ANLG_1, 0xfec0);

	regmap_write(rt5691->regmap, RT5691_OFFSET_CAL_1, 0xbd00);
	i = 0;
	while (true) {
		if (snd_soc_component_read(component, RT5691_HP_AMP_DET_CTRL_12) & 0x8000)
			usleep_range(10000, 10005);
		else
			break;

		if (i > 20) {
			dev_err(component->dev, "HP Calibration Failure\n");
			regmap_write(rt5691->regmap, RT5691_RESET, 0);
			regcache_cache_bypass(rt5691->regmap, false);
			return;
		}

		i++;
	}

	regmap_write(rt5691->regmap, RT5691_OFFSET_CAL_25, 0xf406);
	usleep_range(400000, 400005);

	for (i = ARRAY_SIZE(rt5691_cal_list) - 1; i >= 0; i--) {
		if (rt5691_cal_list[i].reg == 0x0209)
			rt5691_cal_list_saved[i] |= 0x2000;

		snd_soc_component_write(component, rt5691_cal_list[i].reg,
			rt5691_cal_list_saved[i]);
	}

	regcache_cache_bypass(rt5691->regmap, false);

	regcache_mark_dirty(rt5691->regmap);
	regcache_sync(rt5691->regmap);

	/* volatile settings */
	regmap_write(rt5691->regmap, RT5691_COMBO_JACK_CTRL_4, 0x0104);

	snd_soc_dapm_mutex_unlock(dapm);

	if (rt5691->irq) {
		rt5691_irq(0, rt5691);

		ret = request_threaded_irq(rt5691->irq, NULL, rt5691_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, "rt5691", rt5691);
		if (ret)
			dev_err(component->dev, "Failed to reguest IRQ: %d\n", ret);

		ret = irq_set_irq_wake(rt5691->irq, 1);
		if (ret)
			dev_err(component->dev, "Failed to reguest IRQ wake: %d\n",
				ret);
	}
}

static void rt5691_calibrate_handler(struct work_struct *work)
{
	struct rt5691_priv *rt5691 = container_of(work, struct rt5691_priv,
		calibrate_work.work);
	int i = 0;

	while (!rt5691->component->card->instantiated) {
		msleep(20);

		if (rt5691->cal_done || i > 250)
			return;

		i++;
	}

	rt5691_calibrate(rt5691);
}

static void rt5691_mic_check_handler(struct work_struct *work)
{
	struct rt5691_priv *rt5691 = container_of(work, struct rt5691_priv,
		mic_check_work.work);
	struct snd_soc_component *component = rt5691->component;
	struct snd_soc_dapm_context *dapm = &component->dapm;
	int sar_hs_type, i;

	pm_wakeup_event(component->dev, 3000);

	rt5691->mic_check_break = false;

	if (rt5691->is_suspend) {
		/* Because some SOCs need wake up time of I2C controller */
		dev_info(component->dev, "%s wait resume\n", __func__);
		i = 0;
		while (i < 10 && rt5691->is_suspend) {
			msleep(50);
			i++;
		}
	}

	regmap_update_bits(rt5691->regmap, RT5691_PWR_DA_PATH_2, 0x80, 0x80);
	snd_soc_dapm_force_enable_pin(dapm, "MICBIAS1");
	snd_soc_dapm_sync(dapm);

	regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_2, 0x2c);
	regmap_write(rt5691->regmap, RT5691_JACK_TYPE_DET_CTRL_2, 0xfc00);
	regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_1, 0);

	sar_hs_type = rt5691->pdata.sar_hs_type ?
		rt5691->pdata.sar_hs_type : 729;

	for (i = 0; i < 100; i++) {
		msleep(20);

		if (i % 10 == 0) {
			rt5691->adc_val = snd_soc_component_read(component,
				RT5691_SAR_ADC_DET_CTRL_23);
		}

		if (rt5691->mic_check_break)
			break;
	}

	if (!rt5691->mic_check_break && rt5691->adc_val > sar_hs_type) {
		regmap_update_bits(rt5691->regmap,
			RT5691_IRQ_CTRL_1, 0x0004, 0x0004);
		regmap_update_bits(rt5691->regmap,
			RT5691_IRQ_CTRL_2, 0x0018, 0x0018);
		regmap_write(rt5691->regmap, RT5691_INT_ST_1, 0);
		regmap_update_bits(rt5691->regmap,
			RT5691_COMBO_JACK_CTRL_3, 0x00f3, 0x00e1);
		regmap_update_bits(rt5691->regmap,
			RT5691_MONO_ANLG_DRE_CTRL_2, 0x0600, 0x0600);
		if (rt5691->adc_val > 2000) {
			regmap_update_bits(rt5691->regmap,
				RT5691_COMBO_JACK_CTRL_5, 0xc007, 0xc005);
		} else {
			regmap_update_bits(rt5691->regmap,
				RT5691_COMBO_JACK_CTRL_2, 0x8000, 0x8000);
			regmap_update_bits(rt5691->regmap,
				RT5691_COMBO_JACK_CTRL_5, 0xc007, 0xc004);
		}
		regmap_update_bits(rt5691->regmap,
			RT5691_COMBO_JACK_CTRL_4, 0x600, 0x600);
		regmap_update_bits(rt5691->regmap,
			RT5691_PWR_DA_PATH_2, 0x400, 0x400);

		rt5691->jack_type = SND_JACK_HEADSET;
		rt5691_enable_push_button_irq(component, true);
#ifdef CONFIG_SWITCH
		switch_set_state(&rt5691_headset_switch, 1);
#endif
		rt5691->button_timeout = jiffies + (HZ * 1);
		snd_soc_jack_report(rt5691->hs_jack, rt5691->jack_type,
			SND_JACK_HEADSET);

		dev_info(component->dev, "%s: jack_type = 0x%04x\n", __func__,
			rt5691->jack_type);
		return;
	}

	regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_2, 0x20);
	regmap_update_bits(rt5691->regmap, RT5691_COMBO_JACK_CTRL_4, 0x400, 0);
	regmap_update_bits(rt5691->regmap, RT5691_MONO_ANLG_DRE_CTRL_2, 0x0600, 0);
	regmap_update_bits(rt5691->regmap, RT5691_PWR_DA_PATH_2, 0x80, 0);
	snd_soc_dapm_disable_pin(dapm, "MICBIAS1");
	snd_soc_dapm_sync(dapm);
}

static void rt5691_sto1_l_adc_handler(struct work_struct *work)
{
	struct rt5691_priv *rt5691 =
		container_of(work, struct rt5691_priv, sto1_l_adc_work.work);
	struct snd_soc_component *component = rt5691->component;

	snd_soc_component_update_bits(component, RT5691_ADC_FILTER_CTRL_3,
		(0x1 << 7), 0);
}

static void rt5691_sto1_r_adc_handler(struct work_struct *work)
{
	struct rt5691_priv *rt5691 =
		container_of(work, struct rt5691_priv, sto1_r_adc_work.work);
	struct snd_soc_component *component = rt5691->component;

	snd_soc_component_update_bits(component, RT5691_ADC_FILTER_CTRL_3,
		(0x1 << 4), 0);
}

static void rt5691_sto2_l_adc_handler(struct work_struct *work)
{
	struct rt5691_priv *rt5691 =
		container_of(work, struct rt5691_priv, sto2_l_adc_work.work);
	struct snd_soc_component *component = rt5691->component;

	snd_soc_component_update_bits(component, RT5691_ADC_FILTER2_CTRL_3,
		(0x1 << 7), 0);
}

static void rt5691_sto2_r_adc_handler(struct work_struct *work)
{
	struct rt5691_priv *rt5691 =
		container_of(work, struct rt5691_priv, sto2_r_adc_work.work);
	struct snd_soc_component *component = rt5691->component;

	snd_soc_component_update_bits(component, RT5691_ADC_FILTER2_CTRL_3,
		(0x1 << 4), 0);
}

#ifdef CONFIG_DEBUG_FS
static ssize_t rt5691_codec_reg_read_file(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct rt5691_priv *rt5691 = file->private_data;
	char *buf;
	unsigned int val, i;
	size_t ret = 0, size = 0;

	if (rt5691->dump_reg > RT5691_HP_AMP_DET_CTRL_20) {
		rt5691->dump_reg = RT5691_RESET;
		return 0;
	}

	buf = kzalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = rt5691->dump_reg; i <= RT5691_HP_AMP_DET_CTRL_20; i++) {
		if (rt5691_readable_register(NULL, i)) {
			if (size + ret >= count)
				break;

			regmap_read(rt5691->regmap, i, &val);
			ret = sprintf(buf + size, "%04x: %04x\n", i, val);
			size += ret;
		}
	}

	if (copy_to_user(user_buf, buf, size)) {
		kfree(buf);
		return -EFAULT;
	}

	rt5691->dump_reg = i;
	kfree(buf);

	return size;
}

static ssize_t rt5691_codec_reg_write_file(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct rt5691_priv *rt5691 = file->private_data;
	char buf[32];
	size_t buf_size;
	char *start = buf;
	unsigned long reg, value;
	int ret;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	while (*start == ' ')
		start++;
	reg = simple_strtoul(start, &start, 16);
	while (*start == ' ')
		start++;
	if (kstrtoul(start, 16, &value))
		return -EINVAL;

	ret = regmap_write(rt5691->regmap, reg, value);
	if (ret < 0)
		return ret;
	return buf_size;
}

static const struct file_operations rt5691_codec_reg_fops = {
	.open = simple_open,
	.read = rt5691_codec_reg_read_file,
	.write = rt5691_codec_reg_write_file,
	.llseek = default_llseek,
};

static ssize_t rt5691_codec_reg_adb_read_file(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct rt5691_priv *rt5691 = file->private_data;
	char *buf;
	unsigned int val, i;
	size_t ret = 0, size = 0;

	if (!rt5691->adb_reg_num)
		return 0;

	buf = kzalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < rt5691->adb_reg_num; i++) {
		if (rt5691_readable_register(NULL, rt5691->adb_reg_addr[i])) {
			if (size + ret >= count)
				break;

			regmap_read(rt5691->regmap, rt5691->adb_reg_addr[i],
				&val);
			ret = sprintf(buf + size, "%04x: %04x\n",
				rt5691->adb_reg_addr[i], val);
			size += ret;
		}
	}

	if (copy_to_user(user_buf, buf, size)) {
		kfree(buf);
		return -EFAULT;
	}

	rt5691->adb_reg_num = 0;
	kfree(buf);

	return size;
}

static ssize_t rt5691_codec_reg_adb_write_file(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct rt5691_priv *rt5691 = file->private_data;
	char buf[768];
	size_t buf_size;
	unsigned int value;
	int i = 2, j = 0;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (buf[0] == 'R' || buf[0] == 'r') {
		while (j <= 0x3f && i < count) {
			rt5691->adb_reg_addr[j] = 0;
			value = 0;
			for ( ; i < count; i++) {
				if (*(buf + i) <= '9' && *(buf + i) >= '0')
					value = (value << 4) | (*(buf + i) - '0');
				else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
					value = (value << 4) | ((*(buf + i) - 'a') + 0xa);
				else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
					value = (value << 4) | ((*(buf + i) - 'A') + 0xa);
				else
					break;
			}
			i++;

			rt5691->adb_reg_addr[j] = value;
			j++;
		}
		rt5691->adb_reg_num = j;
	} else if (buf[0] == 'W' || buf[0] == 'w') {
		while (j <= 0x3f && i < count) {
			/* Get address */
			rt5691->adb_reg_addr[j] = 0;
			value = 0;
			for ( ; i < count; i++) {
				if (*(buf + i) <= '9' && *(buf + i) >= '0')
					value = (value << 4) | (*(buf + i) - '0');
				else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
					value = (value << 4) | ((*(buf + i) - 'a') + 0xa);
				else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
					value = (value << 4) | ((*(buf + i) - 'A') + 0xa);
				else
					break;
			}
			i++;
			rt5691->adb_reg_addr[j] = value;

			/* Get value */
			rt5691->adb_reg_value[j] = 0;
			value = 0;
			for ( ; i < count; i++) {
				if (*(buf + i) <= '9' && *(buf + i) >= '0')
					value = (value << 4) | (*(buf + i) - '0');
				else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
					value = (value << 4) | ((*(buf + i) - 'a') + 0xa);
				else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
					value = (value << 4) | ((*(buf + i) - 'A') + 0xa);
				else
					break;
			}
			i++;
			rt5691->adb_reg_value[j] = value;

			j++;
		}

		rt5691->adb_reg_num = j;

		for (i = 0; i < rt5691->adb_reg_num; i++) {
			regmap_write(rt5691->regmap,
				rt5691->adb_reg_addr[i],
				rt5691->adb_reg_value[i]);
		}
	}

	return count;
}

static const struct file_operations rt5691_codec_reg_adb_fops = {
	.open = simple_open,
	.read = rt5691_codec_reg_adb_read_file,
	.write = rt5691_codec_reg_adb_write_file,
	.llseek = default_llseek,
};
#endif

static int rt5691_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	struct rt5691_platform_data *pdata = dev_get_platdata(&i2c->dev);
	struct rt5691_priv *rt5691;
	struct regulator *regulator_1v8, *regulator_3v3;
	int ret;
	unsigned int val;

	rt5691 = devm_kzalloc(&i2c->dev, sizeof(struct rt5691_priv),
		GFP_KERNEL);

	if (rt5691 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt5691);

	if (pdata)
		rt5691->pdata = *pdata;
	else
		rt5691_parse_dt(rt5691, &i2c->dev);

	regulator_1v8 = devm_regulator_get(&i2c->dev, "regulator_1v8");
	if (IS_ERR(regulator_1v8))
		dev_err(&i2c->dev, "Fail to get regulator_1v8\n");
	else if (regulator_enable(regulator_1v8))
		dev_err(&i2c->dev, "Fail to enable regulator_1v8\n");

	regulator_3v3 = devm_regulator_get(&i2c->dev, "regulator_3v3");
	if (IS_ERR(regulator_3v3))
		dev_err(&i2c->dev, "Fail to get regulator_3v3\n");
	else if (regulator_enable(regulator_3v3))
		dev_err(&i2c->dev, "Fail to enable regulator_3v3\n");

	if (gpio_is_valid(rt5691->pdata.gpio_1v8)) {
		if (devm_gpio_request(&i2c->dev, rt5691->pdata.gpio_1v8,
			"rt5691"))
			dev_err(&i2c->dev, "Fail gpio_request gpio_1v8\n");
		else if (gpio_direction_output(rt5691->pdata.gpio_1v8, 1))
			dev_err(&i2c->dev, "Fail gpio_direction gpio_1v8\n");
	}

	if (gpio_is_valid(rt5691->pdata.gpio_3v3)) {
		if (devm_gpio_request(&i2c->dev, rt5691->pdata.gpio_3v3,
			"rt5691"))
			dev_err(&i2c->dev, "Fail gpio_request gpio_3v3\n");
		else if (gpio_direction_output(rt5691->pdata.gpio_3v3, 1))
			dev_err(&i2c->dev, "Fail gpio_direction gpio_ldo\n");
	}

	if (gpio_is_valid(rt5691->pdata.ldo1_en)) {
		if (devm_gpio_request(&i2c->dev, rt5691->pdata.ldo1_en,
			"rt5691"))
			dev_err(&i2c->dev, "Fail gpio_request gpio_ldo\n");
		else if (gpio_direction_output(rt5691->pdata.ldo1_en, 1))
			dev_err(&i2c->dev, "Fail gpio_direction gpio_ldo\n");
	}

	/* Sleep for 100 ms minimum */
	usleep_range(100000, 150000);

	if (rt5691->pdata.i2c_op_count) {
		rt5691->i2c_regmap = devm_regmap_init_i2c(i2c, &rt5691_i2c_regmap);
		if (IS_ERR(rt5691->i2c_regmap)) {
			ret = PTR_ERR(rt5691->i2c_regmap);
			dev_err(&i2c->dev, "Failed to allocate i2c_regmap: %d\n",
				ret);
			return ret;
		}

		rt5691->regmap = devm_regmap_init(&i2c->dev, NULL, i2c,
						&rt5691_debug_regmap);
		if (IS_ERR(rt5691->regmap)) {
			ret = PTR_ERR(rt5691->regmap);
			dev_err(&i2c->dev, "Failed to allocate debug_regmap: %d\n",
				ret);
			return ret;
		}
	} else {
		rt5691->regmap = devm_regmap_init_i2c(i2c, &rt5691_regmap);
		if (IS_ERR(rt5691->regmap)) {
			ret = PTR_ERR(rt5691->regmap);
			dev_err(&i2c->dev, "Failed to allocate regmap: %d\n",
				ret);
			return ret;
		}
	}

	regmap_read(rt5691->regmap, RT5691_DEVICE_ID, &val);
	if (val != 0x1091 && val != 0x6827) {
		dev_err(&i2c->dev,
			"Device with ID register %x is not rt5691\n", val);

		if (IS_ERR(regulator_1v8))
			dev_err(&i2c->dev, "Fail to get regulator_1v8\n");
		else if (regulator_disable(regulator_1v8))
			dev_err(&i2c->dev, "Fail to disable regulator_1v8\n");

		if (IS_ERR(regulator_3v3))
			dev_err(&i2c->dev, "Fail to get regulator_3v3\n");
		else if (regulator_disable(regulator_3v3))
			dev_err(&i2c->dev, "Fail to disable regulator_3v3\n");

		return -ENODEV;
	} else {
		dev_info(&i2c->dev, "Device ID = 0x%04x\n", val);
	}

	regmap_write(rt5691->regmap, RT5691_RESET, 0);

	regmap_multi_reg_write(rt5691->regmap,
		rt5691_init_list, ARRAY_SIZE(rt5691_init_list));

	if (rt5691->pdata.in1_diff)
		regmap_update_bits(rt5691->regmap, RT5691_IN1_IN2_CTRL,
			0x1 << 15, 0x1 << 15);

	if (rt5691->pdata.in2_diff)
		regmap_update_bits(rt5691->regmap, RT5691_IN1_IN2_CTRL,
			0x1 << 7, 0x1 << 7);

	if (rt5691->pdata.in3_diff)
		regmap_update_bits(rt5691->regmap, RT5691_IN3_CTRL,
			0x1 << 15, 0x1 << 15);

	if (rt5691->pdata.gpio2_func)
		regmap_update_bits(rt5691->regmap, RT5691_MULTI_FUNC_PIN_CTRL_1,
			0x3 << 13, rt5691->pdata.gpio2_func << 13);

	if (rt5691->pdata.gpio3_func)
		regmap_update_bits(rt5691->regmap, RT5691_MULTI_FUNC_PIN_CTRL_1,
			0x3 << 11, rt5691->pdata.gpio3_func << 11);

	if (rt5691->pdata.sar_pb_vth0) {
		regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_17,
			(rt5691->pdata.sar_pb_vth0 & 0x700) >> 8);
		regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_18,
			rt5691->pdata.sar_pb_vth0 & 0xff);
	}

	if (rt5691->pdata.sar_pb_vth1) {
		regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_15,
			(rt5691->pdata.sar_pb_vth1 & 0x700) >> 8);
		regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_16,
			rt5691->pdata.sar_pb_vth1 & 0xff);
	}

	if (rt5691->pdata.sar_pb_vth2) {
		regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_13,
			(rt5691->pdata.sar_pb_vth2 & 0x700) >> 8);
		regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_14,
			rt5691->pdata.sar_pb_vth2 & 0xff);
	}

	if (rt5691->pdata.sar_pb_vth3) {
		regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_11,
			(rt5691->pdata.sar_pb_vth3 & 0x700) >> 8);
		regmap_write(rt5691->regmap, RT5691_SAR_ADC_DET_CTRL_12,
			rt5691->pdata.sar_pb_vth3 & 0xff);
	}

	if (rt5691->pdata.jd_resistor)
		regmap_write(rt5691->regmap, RT5691_COMBO_WATER_CTRL_4,
			rt5691->pdata.jd_resistor);

	if (rt5691->pdata.hpa_capless_bias)
		regmap_write(rt5691->regmap, RT5691_ANLG_BIAS_CTRL_4,
			rt5691->pdata.hpa_capless_bias);

#ifdef CONFIG_SWITCH
	switch_dev_register(&rt5691_headset_switch);
#endif

#ifdef CONFIG_DEBUG_FS
		rt5691_debugfs_root = debugfs_create_dir("rt5691", NULL);
		if (!rt5691_debugfs_root) {
			dev_err(&i2c->dev, "Failed to create debugfs root\n");
		} else {
			debugfs_create_file("codec_reg", 0664,
				rt5691_debugfs_root, rt5691,
				&rt5691_codec_reg_fops);
			debugfs_create_file("codec_reg_adb", 0664,
				rt5691_debugfs_root, rt5691,
				&rt5691_codec_reg_adb_fops);
		}
#endif

	INIT_DELAYED_WORK(&rt5691->jack_detect_work,
		rt5691_jack_detect_handler);
	INIT_DELAYED_WORK(&rt5691->calibrate_work, rt5691_calibrate_handler);
	INIT_DELAYED_WORK(&rt5691->sto1_l_adc_work, rt5691_sto1_l_adc_handler);
	INIT_DELAYED_WORK(&rt5691->sto1_r_adc_work, rt5691_sto1_r_adc_handler);
	INIT_DELAYED_WORK(&rt5691->sto2_l_adc_work, rt5691_sto2_l_adc_handler);
	INIT_DELAYED_WORK(&rt5691->sto2_r_adc_work, rt5691_sto2_r_adc_handler);
	INIT_DELAYED_WORK(&rt5691->mic_check_work, rt5691_mic_check_handler);

	if (i2c->irq)
		rt5691->irq = i2c->irq;

	device_init_wakeup(&i2c->dev, true);

	return devm_snd_soc_register_component(&i2c->dev, &rt5691_soc_component_dev,
			rt5691_dai, ARRAY_SIZE(rt5691_dai));
}

static int rt5691_i2c_remove(struct i2c_client *i2c)
{

#ifdef CONFIG_SWITCH
	switch_dev_unregister(&rt5691_headset_switch);
#endif

	device_init_wakeup(&i2c->dev, false);

	return 0;
}

static void rt5691_i2c_shutdown(struct i2c_client *client)
{
	struct rt5691_priv *rt5691 = i2c_get_clientdata(client);

	regmap_write(rt5691->regmap, RT5691_RESET, 0);
}

#ifdef CONFIG_OF
static const struct of_device_id rt5691_of_match[] = {
	{.compatible = "realtek,rt5691"},
	{},
};
MODULE_DEVICE_TABLE(of, rt5691_of_match);
#endif

#ifdef CONFIG_ACPI
static struct acpi_device_id rt5691_acpi_match[] = {
	{"10EC5691", 0,},
	{},
};
MODULE_DEVICE_TABLE(acpi, rt5691_acpi_match);
#endif

#ifdef CONFIG_PM_SLEEP
static int rt5691_pm_suspend(struct device *dev)
{
	struct rt5691_priv *rt5691 = dev_get_drvdata(dev);
	struct snd_soc_component *component = rt5691->component;

	dev_info(component->dev, "%s\n", __func__);
	rt5691->is_suspend = true;

	return 0;
}

static int rt5691_pm_resume(struct device *dev)
{
	struct rt5691_priv *rt5691 = dev_get_drvdata(dev);
	struct snd_soc_component *component = rt5691->component;

	dev_info(component->dev, "%s\n", __func__);
	rt5691->is_suspend = false;
	rt5691->rek = true;

	return 0;
}
#endif

static const struct dev_pm_ops rt5691_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rt5691_pm_suspend, rt5691_pm_resume)
};

struct i2c_driver rt5691_i2c_driver = {
	.driver = {
		.name = "rt5691",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt5691_of_match),
		.acpi_match_table = ACPI_PTR(rt5691_acpi_match),
		.pm = &rt5691_pm_ops,
	},
	.probe = rt5691_i2c_probe,
	.remove = rt5691_i2c_remove,
	.shutdown = rt5691_i2c_shutdown,
	.id_table = rt5691_i2c_id,
};
module_i2c_driver(rt5691_i2c_driver);

MODULE_DESCRIPTION("ASoC RT5691 driver");
MODULE_AUTHOR("Oder Chiou<oder_chiou@realtek.com>");
MODULE_LICENSE("GPL v2");
