/*
 *  Copyright (C) 2021, Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __ISG6320_REG_H__
#define __ISG6320_REG_H__



#define ISG6320_MAX_FREQ_STEP      16

#if IS_ENABLED(CONFIG_FLIP_COVER_DETECTOR_NOTIFIER)
#include <linux/notifier.h>
#define FCD_ATTACH		1
#define FCD_DETACH		0
#endif

enum ic_num {
	MAIN_GRIP = 0,
	SUB_GRIP,
	SUB2_GRIP,
	WIFI_GRIP,
	GRIP_MAX_CNT
};

const char *device_name[GRIP_MAX_CNT] = {
	"ISG6320",
	"ISG6320_SUB",
	"ISG6320_SUB2",
	"ISG6320_WIFI"
};

const char *module_name[GRIP_MAX_CNT] = {
	"grip_sensor",
	"grip_sensor_sub",
	"grip_sensor_sub2",
	"grip_sensor_wifi"
};
#define NOTI_MODULE_NAME        "grip_notifier"
enum registers {
	ISG6320_IRQSRC_REG = 0x00,
	ISG6320_IRQSTS_REG,
	ISG6320_IRQMSK_REG = 0x04,
	ISG6320_IRQCON_REG,
	ISG6320_OSCCON_REG,
	ISG6320_IRQFUNC_REG,
	ISG6320_WUTDATA_REG = 0x08,
	ISG6320_WUTDATA_LSB_REG = 0x09,
	ISG6320_WUTDATD_REG = 0x0A,
	ISG6320_BS_ON_WD_REG = 0x0B,
	ISG6320_CML_BIAS_REG = 0x0E,
	ISG6320_NUM_OF_CLK = 0x17,
	ISG6320_SCANCTRL1_REG = 0x29,
	ISG6320_SCANCTRL2_REG,
	ISG6320_ACTSCAN_REG = 0x2C,
	ISG6320_SCANCTRL11_REG = 0x30,
	ISG6320_SCANCTRL13_REG,
	ISG6320_A_COARSE_OUT_REG,
	ISG6320_FINE_OUT_A_REG,
	ISG6320_B_COARSE_OUT_REG,
	ISG6320_FINE_OUT_B_REG,
	ISG6320_CFCAL_RTN_REG,
	ISG6320_CDC16_A_H_REG = 0x3D,
	ISG6320_CDC16_A_L_REG,
	ISG6320_CDC16_TA_H_REG,
	ISG6320_CDC16_TA_L_REG,
	ISG6320_BSL16_A_H_REG,
	ISG6320_BSL16_A_L_REG,
	ISG6320_CDC16_B_H_REG = 0x45,
	ISG6320_CDC16_B_L_REG,
	ISG6320_CDC16_TB_H_REG,
	ISG6320_CDC16_TB_L_REG,
	ISG6320_BSL16_B_H_REG,
	ISG6320_BSL16_B_L_REG,
	ISG6320_NUM_OF_CLK_B,
	ISG6320_A_TEMPERATURE_ENABLE_REG = 0x61,
	ISG6320_A_ANALOG_GAIN,
	ISG6320_A_COARSE_REG,
	ISG6320_A_FINE_REG,
	ISG6320_A_PROXCTL1_REG,
	ISG6320_A_PROXCTL2_REG,
	ISG6320_A_PROXCTL4_REG,
	ISG6320_A_PROXCTL8_REG,
	ISG6320_A_ACALCTL4_REG,
	ISG6320_A_ACALCTL5_REG,
	ISG6320_A_LSUM_TYPE_REG = 0X6D,
	ISG6320_A_DIGITAL_ACC_REG = 0x6F,
	ISG6320_A_CDC_UP_COEF_REG = 0x71,
	ISG6320_A_CDC_DN_COEF_REG,
	ISG6320_B_TEMPERATURE_ENABLE_REG = 0x95,
	ISG6320_B_ANALOG_GAIN,
	ISG6320_B_COARSE_REG,
	ISG6320_B_FINE_REG,
	ISG6320_B_PROXCTL1_REG,
	ISG6320_B_PROXCTL2_REG,
	ISG6320_B_PROXCTL4_REG,
	ISG6320_B_PROXCTL8_REG,
	ISG6320_B_ACALCTL4_REG,
	ISG6320_B_ACALCTL5_REG,
	ISG6320_B_LSUM_TYPE_REG = 0XA1,
	ISG6320_B_DIGITAL_ACC_REG = 0xA3,
	ISG6320_B_CDC_UP_COEF_REG = 0xA5,
	ISG6320_B_CDC_DN_COEF_REG,
	ISG6320_RESETCON_REG = 0xC9,
	ISG6320_PROTECT_REG = 0xCD,
	ISG6320_SOFTRESET_REG = 0xFD,
};

#define ISG6320_CHANNEL_DISABLE     0x00

#define ISG6320_PROX_A_STATE		4
#define ISG6320_PROX_B_STATE		0

#define ISG6320_IRQ_ENABLE		0x0C
#define ISG6320_IRQ_ENABLE_A		0x09
#define ISG6320_IRQ_DISABLE		0x0D

#define ISG6320_DFE_ENABLE		0x80
#define ISG6320_BFCAL_START		0x03
#define ISG6320_CFCAL_START		0xD7
#define ISG6320_CHECK_TIME		50

#define ISG6320_SCAN_STOP		0x00
#define ISG6320_SCAN_READY		0x2A

#define ISG6320_OSC_SLEEP		0xB0
#define ISG6320_OSC_NOMAL		0xF0
#define ISG6320_ES_RESET_CONDITION		17000
#define ISG6320_CS_RESET_CONDITION		5000

#define ISG6320_RST_VALUE		0xDE
#define ISG6320_PRT_VALUE		0xDE

#define ISG6320_BS_WD_OFF		0x1A
#define ISG6320_BS_WD_ON		0x9A

#define ISG6320_DFE_RESET_ON	0x10
#define ISG6320_DFE_RESET_OFF	0x00

#define MAX_REGISTRY_CNT		70

#define ISG6320_CAL_RTN_A_MASK		0x02
#define ISG6320_CAL_RTN_B_MASK		0x01

#define SCAN_ON_CMD  0x03
#define SCAN_OFF_CMD 0x00

#define UNKNOWN_ON  1
#define UNKNOWN_OFF 2

enum {
	OFF = 0,
	ON,
};

enum {
	NONE_ENABLE = -1,
	FAR = 0,
	CLOSE,
};

struct isg6320_reg_data {
	unsigned char addr;
	unsigned char val;
};

static const struct isg6320_reg_data setup_reg[MAX_REGISTRY_CNT] = {
	{	.addr = 0X17,	.val = 0X31,	},
	{	.addr = 0X18,	.val = 0X2B,	},
	{	.addr = 0X19,	.val = 0X00,	},
	{	.addr = 0X1A,	.val = 0X19,	},
	{	.addr = 0X1B,	.val = 0X11,	},
	{	.addr = 0X1C,	.val = 0X19,	},
	{	.addr = 0X1D,	.val = 0X2A,	},
	{	.addr = 0X1E,	.val = 0X00,	},
	{	.addr = 0X1F,	.val = 0X05,	},
	{	.addr = 0X20,	.val = 0X11,	},
	{	.addr = 0X21,	.val = 0X11,	},
	{	.addr = 0X22,	.val = 0X1E,	},
	{	.addr = 0X23,	.val = 0X2A,	},
	{	.addr = 0X24,	.val = 0X05,	},
	{	.addr = 0X25,	.val = 0X1E,	},
	{	.addr = 0X26,	.val = 0X2A,	},
	{	.addr = 0X27,	.val = 0X05,	},
	{	.addr = 0X28,	.val = 0X11,	},
	{	.addr = 0X61,	.val = 0X08,	},
	{	.addr = 0X62,	.val = 0XA8,	},
	{	.addr = 0X91,	.val = 0XA8,	},
	{	.addr = 0X67,	.val = 0X58,	},
	{	.addr = 0X68,	.val = 0X34,	},
	{	.addr = 0X69,	.val = 0X5D,	},
	{	.addr = 0X6A,	.val = 0XC0,	},
	{	.addr = 0X6C,	.val = 0X80,	},
	{	.addr = 0X6D,	.val = 0X88,	},
	{	.addr = 0X6E,	.val = 0X44,	},
	{	.addr = 0X6F,	.val = 0X08,	},
	{	.addr = 0X70,	.val = 0X60,	},
	{	.addr = 0X71,	.val = 0X80,	},
	{	.addr = 0X72,	.val = 0X40,	},
	{	.addr = 0X73,	.val = 0X00,	},
	{	.addr = 0X74,	.val = 0XED,	},
	{	.addr = 0X75,	.val = 0X40,	},
	{	.addr = 0X76,	.val = 0XED,	},
	{	.addr = 0X77,	.val = 0X80,	},
	{	.addr = 0X78,	.val = 0XF0,	},
	{	.addr = 0X79,	.val = 0XF0,	},
	{	.addr = 0X95,	.val = 0X08,	},
	{	.addr = 0X96,	.val = 0XA8,	},
	{	.addr = 0XC5,	.val = 0XA8,	},
	{	.addr = 0X9B,	.val = 0X58,	},
	{	.addr = 0X9C,	.val = 0X34,	},
	{	.addr = 0X9D,	.val = 0X5D,	},
	{	.addr = 0X9E,	.val = 0XC0,	},
	{	.addr = 0XA0,	.val = 0X80,	},
	{	.addr = 0XA1,	.val = 0X88,	},
	{	.addr = 0XA2,	.val = 0X44,	},
	{	.addr = 0XA3,	.val = 0X08,	},
	{	.addr = 0XA4,	.val = 0X60,	},
	{	.addr = 0XA5,	.val = 0X80,	},
	{	.addr = 0XA6,	.val = 0X40,	},
	{	.addr = 0XA7,	.val = 0X00,	},
	{	.addr = 0XA8,	.val = 0XED,	},
	{	.addr = 0XA9,	.val = 0X40,	},
	{	.addr = 0XAA,	.val = 0XED,	},
	{	.addr = 0XAB,	.val = 0X80,	},
	{	.addr = 0XAC,	.val = 0XF0,	},
	{	.addr = 0XAD,	.val = 0XF0,	},
	{	.addr = 0X05,	.val = 0XB8,	},
	{	.addr = 0X06,	.val = 0X80,	},
	{	.addr = 0X07,	.val = 0X0C,	},
	{	.addr = 0X08,	.val = 0X01,	},
	{	.addr = 0X09,	.val = 0X80,	},
	{	.addr = 0X11,	.val = 0X06,	},
	{	.addr = 0X12,	.val = 0X08,	},
	{	.addr = 0X2C,	.val = 0X03,	},
};

#ifndef CONFIG_SENSORS_CORE_AP
extern int sensors_create_symlink(struct input_dev *inputdev);
extern void sensors_remove_symlink(struct input_dev *inputdev);
extern int sensors_register(struct device **dev, void *drvdata,
			    struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev,
			       struct device_attribute *attributes[]);
#endif
#if 0 //IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
extern unsigned int lpcharge;
#endif

#if IS_ENABLED(CONFIG_FLIP_COVER_DETECTOR_NOTIFIER)
extern int fcd_notifier_register(struct notifier_block *nb);
extern int fcd_notifier_unregister(struct notifier_block *nb);
#endif
#if IS_ENABLED(CONFIG_SENSORS_GRIP_FAILURE_DEBUG)
extern void update_grip_error(u8 idx, u32 error_state);
#endif
#endif /* __ISG6320_REG_H__ */
