/* a96t396.h -- Linux driver for A96T396 chip as grip sensor
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 * Author: YunJae Hwang <yjz.hwang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#ifndef __LINUX_A96T396_H
#define __LINUX_A96T396_H

//#include <linux/sensor/sensors_core.h>
#define VENDOR_NAME	"ABOV"

#define UNKNOWN_ON  1
#define UNKNOWN_OFF 2

#define NOTI_MODULE_NAME	"grip_notifier"
/* ioctl */
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
struct a96t396_fw_data {
	char fw_data[14500];
	int fw_size;
	int pass;
};
#define SDCARD_FW_LOARDING 10
#define A96T396_IOCTL_MAGIC 254
#define A96T396_SET_FW_DATA \
	_IOWR(A96T396_IOCTL_MAGIC, 0x01, struct a96t396_fw_data)
#endif

enum ic_num {
#if IS_ENABLED(CONFIG_SENSORS_A96T396)
	MAIN_GRIP = 0,
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB)
	SUB_GRIP,
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB2)
	SUB2_GRIP,
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_WIFI)
	WIFI_GRIP,
#endif
	GRIP_MAX_CNT
};

const char *grip_name[GRIP_MAX_CNT] = {
#if IS_ENABLED(CONFIG_SENSORS_A96T396)
	"MAIN",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB)
	"SUB",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB2)
	"SUB2",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_WIFI)
	"WIFI"
#endif
};

const char *device_name[GRIP_MAX_CNT] = {
#if IS_ENABLED(CONFIG_SENSORS_A96T396)
	"A96T3X6",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB)
	"A96T3X6_SUB",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB2)
	"A96T3X6_SUB2",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_WIFI)
	"A96T3X6_WIFI"
#endif
};

const char *module_name[GRIP_MAX_CNT] = {
#if IS_ENABLED(CONFIG_SENSORS_A96T396)
	"grip_sensor",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB)
	"grip_sensor_sub",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB2)
	"grip_sensor_sub2",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_WIFI)
	"grip_sensor_wifi"
#endif
};

const char *sdcard_fw_path[GRIP_MAX_CNT] = {
#if IS_ENABLED(CONFIG_SENSORS_A96T396)
	"/sdcard/Firmware/Grip/abov_fw.bin",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB)
	"/sdcard/Firmware/GripSub/abov_fw.bin",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB2)
	"/sdcard/Firmware/GripSub2/abov_fw.bin",
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_WIFI)
	"/sdcard/Firmware/GripWifi/abov_fw.bin"
#endif
};

/* registers */
#define REG_FW_VER                      0x02
#define REG_TUNING_VER                  0x03
#define REG_BTNSTATUS                   0x00
#define REG_SAR_TOTALCAP_READ           0x10
#define REG_SAR_TOTALCAP_READ_2CH       0x12
#define REG_VENDORID                    0x05
#define REG_TSPTA                       0x08
#define REG_MODEL_NO                    0x04
#define REG_SW_RESET                    0x06
#define REG_SAR_ENABLE                  0x09
#define REG_PROJECT_ID                  0x0C
#define REG_SAR_NOISE_THRESHOLD         0x2A
#define REG_SAR_NOISE_THRESHOLD_2CH     0x2C
#define REG_SAR_THRESHOLD               0x22
#define REG_SAR_THRESHOLD_2CH           0x24
#define REG_SAR_RELEASE_THRESHOLD       0x26
#define REG_SAR_RELEASE_THRESHOLD_2CH   0x28
#define REG_SAR_RAWDATA                 0x1A
#define REG_SAR_RAWDATA_2CH             0x1C
#define REG_SAR_BASELINE                0x1E
#define REG_SAR_BASELINE_2CH            0x20
#define REG_SAR_DIFFDATA                0x16
#define REG_SAR_DIFFDATA_D_2CH          0x18
#define REG_REF_CAP			0x14
#define REG_GAINDATA		0x2E
#ifdef CONFIG_SENSORS_A96T396_2CH
#define REG_GAINDATA_2CH	0x2F
#endif

#define REG_REF_GAINDATA	0x30
#define REG_GRIP_ALWAYS_ACTIVE	0x07

/* command */
#define CMD_ON			0x20
#define CMD_OFF			0x10
#define GRIP_ALWAYS_ACTIVE_ENABLE	0x0
#define GRIP_ALWAYS_ACTIVE_DISABLE	0x1
#define CMD_SW_RESET		0x10
#define BOOT_DELAY		45000
#define RESET_DELAY		150000
#define FLASH_DELAY		1200000
#define FLASH_MODE		0x18
#define CRC_FAIL		0
#define CRC_PASS		1

#define TK_FW_PATH_BIN		"abov/abov_noble.fw"
#define TK_FW_PATH_SDCARD	"/sdcard/Firmware/Grip/abov_fw.bin"
/* depends on FW address configuration */
#define USER_CODE_ADDRESS	0x400
#define I2C_M_WR 0		/* for i2c */
#define GRIP_LOG_TIME	15 /* 30s */
#define FIRMWARE_VENDOR_CALL_CNT 3
#define TEST_FIRMWARE_DETECT_VER 0xa0

enum {
	BUILT_IN = 0,
	SDCARD,
};

enum {
	OFF = 0,
	ON,
};

#if IS_ENABLED(CONFIG_SENSORS_SUPPORT_LOGIC_PARAMETER)
#define	TUNING_00	0x00
#define	TUNING_01	0x01
#define	TUNING_02	0x02
#define	TUNING_03	0x03
#define	TUNING_04	0x04
#define	TUNING_05	0x05
#define	TUNING_06	0x06
#define	TUNING_07	0x07
#define	TUNING_08	0x08
#define	TUNING_09	0x09
#define	TUNING_0A	0x0A
#define	TUNING_0B	0x0B
#define	TUNING_0C	0x0C
#define	TUNING_0D	0x0D
#define	TUNING_0E	0x0E
#define	TUNING_0F	0x0F
#define	TUNING_10	0x10
#define	TUNING_11	0x11
#define	TUNING_12	0x12
#define	TUNING_13	0x13
#define	TUNING_14	0x14
#define	TUNING_15	0x15
#define	TUNING_16	0x16
#define	TUNING_17	0x17
#define	TUNING_18	0x18
#define	TUNING_19	0x19
#define	TUNING_1A	0x1A
#define	TUNING_1B	0x1B
#define	TUNING_1C	0x1C
#define	TUNING_1D	0x1D
#define	TUNING_1E	0x1E
#define	TUNING_1F	0x1F
#define	TUNING_20	0x20
#define	TUNING_21	0x21
#define	TUNING_22	0x22
#define	TUNING_23	0x23
#define	TUNING_24	0x24
#define	TUNING_25	0x25
#define	TUNING_26	0x26
#define	TUNING_27	0x27
#define	TUNING_28	0x28
#define	TUNING_29	0x29
#define	TUNING_2A	0x2A
#define	TUNING_2B	0x2B
#define	TUNING_2C	0x2C
#define	TUNING_2D	0x2D
#define	TUNING_2E	0x2E
#define	TUNING_2F	0x2F
#define	TUNING_30	0x30
#define	TUNING_31	0x31
#define	TUNING_32	0x32
#define	TUNING_33	0x33
#define	TUNING_34	0x34
#define	TUNING_35	0x35
#define	TUNING_36	0x36
#define	TUNING_37	0x37
#define	TUNING_38	0x38
#define	TUNING_39	0x39
#define	TUNING_3A	0x3A
#define	TUNING_3B	0x3B
#define	TUNING_3C	0x3C
#define	TUNING_3D	0x3D
#define	TUNING_3E	0x3E
#define	TUNING_3F	0x3F
#define	TUNING_40	0x40
#define	TUNING_41	0x41
#define	TUNING_42	0x42
#define	TUNING_43	0x43
#define	TUNING_44	0x44
#define	TUNING_45	0x45
#define	TUNING_46	0x46
#define	TUNING_47	0x47
#define	TUNING_48	0x48
#define	TUNING_49	0x49
#define	TUNING_4A	0x4A
#define	TUNING_4B	0x4B
#define	TUNING_4C	0x4C
#define	TUNING_4D	0x4D
#define	TUNING_4E	0x4E
#define	TUNING_4F	0x4F
#define	TUNING_50	0x50
#define	TUNING_51	0x51
#define	TUNING_52	0x52
#define	TUNING_53	0x53
#define	TUNING_54	0x54
#define	TUNING_55	0x55
#define	TUNING_56	0x56
#define	TUNING_57	0x57
#define	TUNING_58	0x58
#define	TUNING_59	0x59
#define	TUNING_5A	0x5A
#define	TUNING_5B	0x5B
#define	TUNING_5C	0x5C
#define	TUNING_5D	0x5D
#define	TUNING_5E	0x5E
#define	TUNING_5F	0x5F
#define	TUNING_60	0x60
#define	TUNING_61	0x61
#define	TUNING_62	0x62
#define	TUNING_63	0x63
#define	TUNING_64	0x64
#define	TUNING_65	0x65
#define	TUNING_66	0x66
#define	TUNING_67	0x67
#define	TUNING_68	0x68
#define	TUNING_69	0x69
#define	TUNING_6A	0x6A
#define	TUNING_6B	0x6B
#define	TUNING_6C	0x6C
#define	TUNING_6D	0x6D
#define	TUNING_6E	0x6E
#define	TUNING_6F	0x6F
#define	TUNING_70	0x70
#define	TUNING_71	0x71
#define	TUNING_72	0x72
#define	TUNING_73	0x73
#define	TUNING_74	0x74
#define	TUNING_75	0x75
#define	TUNING_76	0x76
#define	TUNING_77	0x77
#define	TUNING_78	0x78
#define	TUNING_79	0x79
#define	TUNING_7A	0x7A
#define	TUNING_7B	0x7B
#define	TUNING_7C	0x7C
#define	TUNING_7D	0x7D
#define	CHECKSUM_MSB	0x7E
#define	CHECKSUM_LSB	0x7F

struct a96t3xx_reg_data {
	unsigned char reg;
	unsigned char val;
};

static struct a96t3xx_reg_data setup_reg[] = {
	{
		.reg = TUNING_00,
		.val = 0x01,
	},
	{
		.reg = TUNING_01,
		.val = 0x01,
	},
	{
		.reg = TUNING_02,
		.val = 0x21,
	},
	{
		.reg = TUNING_03,
		.val = 0x01,
	},
	{
		.reg = TUNING_04,
		.val = 0x37,
	},
	{
		.reg = TUNING_05,
		.val = 0x3A,
	},
	{
		.reg = TUNING_06,
		.val = 0x18,
	},
	{
		.reg = TUNING_07,
		.val = 0x20,
	},
	{
		.reg = TUNING_08,
		.val = 0x20,
	},
	{
		.reg = TUNING_09,
		.val = 0x10,
	},
	{
		.reg = TUNING_0A,
		.val = 0x28,
	},
	{
		.reg = TUNING_0B,
		.val = 0x30,
	},
	{
		.reg = TUNING_0C,
		.val = 0x33,
	},
	{
		.reg = TUNING_0D,
		.val = 0x00,
	},
	{
		.reg = TUNING_0E,
		.val = 0x23,
	},
	{
		.reg = TUNING_0F,
		.val = 0x01,
	},
	{
		.reg = TUNING_10,
		.val = 0x90,
	},
	{
		.reg = TUNING_11,
		.val = 0x01,
	},
	{
		.reg = TUNING_12,
		.val = 0x90,
	},
	{
		.reg = TUNING_13,
		.val = 0x01,
	},
	{
		.reg = TUNING_14,
		.val = 0x2C,
	},
	{
		.reg = TUNING_15,
		.val = 0x01,
	},
	{
		.reg = TUNING_16,
		.val = 0x2C,
	},
	{
		.reg = TUNING_17,
		.val = 0x00,
	},
	{
		.reg = TUNING_18,
		.val = 0xC8,
	},
	{
		.reg = TUNING_19,
		.val = 0x00,
	},
	{
		.reg = TUNING_1A,
		.val = 0xC8,
	},
	{
		.reg = TUNING_1B,
		.val = 0x00,
	},
	{
		.reg = TUNING_1C,
		.val = 0x00,
	},
	{
		.reg = TUNING_1D,
		.val = 0x00,
	},
	{
		.reg = TUNING_1E,
		.val = 0x00,
	},
	{
		.reg = TUNING_1F,
		.val = 0x00,
	},
	{
		.reg = TUNING_20,
		.val = 0x00,
	},
	{
		.reg = TUNING_21,
		.val = 0x00,
	},
	{
		.reg = TUNING_22,
		.val = 0x00,
	},
	{
		.reg = TUNING_23,
		.val = 0x00,
	},
	{
		.reg = TUNING_24,
		.val = 0x00,
	},
	{
		.reg = TUNING_25,
		.val = 0x00,
	},
	{
		.reg = TUNING_26,
		.val = 0x00,
	},
	{
		.reg = TUNING_27,
		.val = 0x21,
	},
	{
		.reg = TUNING_28,
		.val = 0x10,
	},
	{
		.reg = TUNING_29,
		.val = 0x0F,
	},
	{
		.reg = TUNING_2A,
		.val = 0x23,
	},
	{
		.reg = TUNING_2B,
		.val = 0x23,
	},
	{
		.reg = TUNING_2C,
		.val = 0x00,
	},
	{
		.reg = TUNING_2D,
		.val = 0x00,
	},
	{
		.reg = TUNING_2E,
		.val = 0x00,
	},
	{
		.reg = TUNING_2F,
		.val = 0x00,
	},
	{
		.reg = TUNING_30,
		.val = 0x03,
	},
	{
		.reg = TUNING_31,
		.val = 0x01,
	},
	{
		.reg = TUNING_32,
		.val = 0x02,
	},
	{
		.reg = TUNING_33,
		.val = 0x02,
	},
	{
		.reg = TUNING_34,
		.val = 0x05,
	},
	{
		.reg = TUNING_35,
		.val = 0x32,
	},
	{
		.reg = TUNING_36,
		.val = 0x64,
	},
	{
		.reg = TUNING_37,
		.val = 0x05,
	},
	{
		.reg = TUNING_38,
		.val = 0x01,
	},
	{
		.reg = TUNING_39,
		.val = 0x0E,
	},
	{
		.reg = TUNING_3A,
		.val = 0x05,
	},
	{
		.reg = TUNING_3B,
		.val = 0x1E,
	},
	{
		.reg = TUNING_3C,
		.val = 0x32,
	},
	{
		.reg = TUNING_3D,
		.val = 0x32,
	},
	{
		.reg = TUNING_3E,
		.val = 0x00,
	},
	{
		.reg = TUNING_3F,
		.val = 0x00,
	},
	{
		.reg = TUNING_40,
		.val = 0x0A,
	},
	{
		.reg = TUNING_41,
		.val = 0x04,
	},
	{
		.reg = TUNING_42,
		.val = 0x04,
	},
	{
		.reg = TUNING_43,
		.val = 0x20,
	},
	{
		.reg = TUNING_44,
		.val = 0x20,
	},
	{
		.reg = TUNING_45,
		.val = 0x34,
	},
	{
		.reg = TUNING_46,
		.val = 0x33,
	},
	{
		.reg = TUNING_47,
		.val = 0x28,
	},
	{
		.reg = TUNING_48,
		.val = 0x28,
	},
	{
		.reg = TUNING_49,
		.val = 0x19,
	},
	{
		.reg = TUNING_4A,
		.val = 0x19,
	},
	{
		.reg = TUNING_4B,
		.val = 0x05,
	},
	{
		.reg = TUNING_4C,
		.val = 0x50,
	},
	{
		.reg = TUNING_4D,
		.val = 0x07,
	},
	{
		.reg = TUNING_4E,
		.val = 0x0A,
	},
	{
		.reg = TUNING_4F,
		.val = 0x1E,
	},
	{
		.reg = TUNING_50,
		.val = 0x1E,
	},
	{
		.reg = TUNING_51,
		.val = 0x00,
	},
	{
		.reg = TUNING_52,
		.val = 0x00,
	},
	{
		.reg = TUNING_53,
		.val = 0x0F,
	},
	{
		.reg = TUNING_54,
		.val = 0x25,
	},
	{
		.reg = TUNING_55,
		.val = 0x27,
	},
	{
		.reg = TUNING_56,
		.val = 0x0A,
	},
	{
		.reg = TUNING_57,
		.val = 0x01,
	},
	{
		.reg = TUNING_58,
		.val = 0x0A,
	},
	{
		.reg = TUNING_59,
		.val = 0x0A,
	},
	{
		.reg = TUNING_5A,
		.val = 0x35,
	},
	{
		.reg = TUNING_5B,
		.val = 0x0A,
	},
	{
		.reg = TUNING_5C,
		.val = 0x3C,
	},
	{
		.reg = TUNING_5D,
		.val = 0xA,
	},
	{
		.reg = TUNING_5E,
		.val = 0x0A,
	},
	{
		.reg = TUNING_5F,
		.val = 0x00,
	},
	{
		.reg = TUNING_60,
		.val = 0x00,
	},
	{
		.reg = TUNING_61,
		.val = 0x00,
	},
	{
		.reg = TUNING_62,
		.val = 0x00,
	},
	{
		.reg = TUNING_63,
		.val = 0x32,
	},
	{
		.reg = TUNING_64,
		.val = 0x0A,
	},
	{
		.reg = TUNING_65,
		.val = 0x14
	},
	{
		.reg = TUNING_66,
		.val = 0x21,
	},
	{
		.reg = TUNING_67,
		.val = 0x0A,
	},
	{
		.reg = TUNING_68,
		.val = 0x14,
	},
	{
		.reg = TUNING_69,
		.val = 0x0A,
	},
	{
		.reg = TUNING_6A,
		.val = 0x50,
	},
	{
		.reg = TUNING_6B,
		.val = 0x02,
	},
	{
		.reg = TUNING_6C,
		.val = 0x0A,
	},
	{
		.reg = TUNING_6D,
		.val = 0x50,
	},
	{
		.reg = TUNING_6E,
		.val = 0x14,
	},
	{
		.reg = TUNING_6F,
		.val = 0x0A,
	},
	{
		.reg = TUNING_70,
		.val = 0x32,
	},
	{
		.reg = TUNING_71,
		.val = 0x04,
	},
	{
		.reg = TUNING_72,
		.val = 0x00,
	},
	{
		.reg = TUNING_73,
		.val = 0x00,
	},
	{
		.reg = TUNING_74,
		.val = 0x00,
	},
	{
		.reg = TUNING_75,
		.val = 0x10,
	},
	{
		.reg = TUNING_76,
		.val = 0x00,
	},
	{
		.reg = TUNING_77,
		.val = 0x00,
	},
	{
		.reg = TUNING_78,
		.val = 0x00,
	},
	{
		.reg = TUNING_79,
		.val = 0x00,
	},
	{
		.reg = TUNING_7A,
		.val = 0x00,
	},
	{
		.reg = TUNING_7B,
		.val = 0x00,
	},
	{
		.reg = TUNING_7C,
		.val = 0x00,
	},
	{
		.reg = TUNING_7D,
		.val = 0x00,
	},
	{
		.reg = CHECKSUM_MSB,
		.val = 0x0B,
	},
	{
		.reg = CHECKSUM_LSB,
		.val = 0x0A,
	},
};
#endif

#define GRIP_ERR(fmt, ...) pr_err("[GRIP_%s] %s: "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)
#define GRIP_INFO(fmt, ...) pr_info("[GRIP_%s] %s: "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)
#define GRIP_WARN(fmt, ...) pr_warn("[GRIP_%s] %s: "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)

#ifndef CONFIG_SENSORS_CORE_AP
extern int sensors_create_symlink(struct input_dev *inputdev);
extern void sensors_remove_symlink(struct input_dev *inputdev);
extern int sensors_register(struct device **dev, void *drvdata,
		struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev,
		struct device_attribute *attributes[]);
#endif
#if IS_ENABLED(CONFIG_SENSORS_GRIP_FAILURE_DEBUG)
extern void update_grip_error(u8 idx, u32 error_state);
#endif
#endif /* LINUX_A96T396_H */
