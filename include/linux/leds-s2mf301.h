/*
 * leds-s2mf301.h - Flash-led driver for Samsung S2MF301
 *
 * Copyright (C) 2023 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LEDS_S2MF301_FLASH_H__
#define __LEDS_S2MF301_FLASH_H__
#include <linux/power_supply.h>
#include <linux/leds.h>

#define MASK(width, shift)		(((0x1 << (width)) - 1) << shift)

#define FLED_EN				0
#define DEBUG_TEST_READ			0

#define S2MF301_CH_MAX			1

#define S2MF301_FLED_PMIC_ID		0xF5
#define S2MF301_FLED_IC_VER		0xF4
#define S2MF301_FLED_REV_NO		MASK(4, 0)
#define S2MF301_FLASH_LIGHT_MAX		5

/* Interrupt register */
#define S2MF301_FLED_INT		0x02

#define S2MF301_FLED_INT_MASK		0x09

/* Status register */
#define S2MF301_FLED_STATUS1		0x10
#define S2MF301_FLED_STATUS2		0x11

/* Mask for status register */
#define S2MF301_CH1_FLASH_ON		MASK(1, 7)
#define S2MF301_CH1_TORCH_ON		MASK(1, 6)
#define S2MF301_FLED_ON_CHECK		MASK(2, 6)

/* Channel control */
#define S2MF301_FLED_CH1_CTRL0		0x12
#define S2MF301_FLED_CH1_CTRL1		0x13
#define S2MF301_FLED_CH1_CTRL2		0x14

/* Mask for channel control register */
#define S2MF301_CHX_OPEN_PROT_EN	MASK(1, 7)
#define S2MF301_CHX_SHORT_PROT_EN	MASK(1, 6)
#define S2MF301_CHX_TORCH_IOUT		MASK(5, 0)
#define S2MF301_CHX_TORCH_TMR_MODE	MASK(1, 7)
#define S2MF301_CHX_DIS_TORCH_TMR	MASK(1, 6)
#define S2MF301_CHX_FLASH_TMR_MODE	MASK(1, 5)
#define S2MF301_CHX_FLASH_IOUT		MASK(5, 0)
#define S2MF301_CHX_FLASH_TMR_DUR	MASK(4, 4)
#define S2MF301_CHX_TORCH_TMR_DUR	MASK(4, 0)

/* Mode control */
#define S2MF301_FLED_CTRL0		0x15
#define S2MF301_FLED_CTRL1		0x16
#define S2MF301_FLED_CTRL2		0x17
#define S2MF301_FLED_CTRL3		0x18
#define S2MF301_FLED_CTRL4		0x19
#define S2MF301_FLED_CTRL5		0x1A
#define S2MF301_FLED_TEST3		0x66
#define S2MF301_FLED_TEST4		0x67

/* Mask for channel control register */
#define S2MF301_CHX_FLASH_FLED_EN	MASK(2, 4)
#define S2MF301_CHX_TORCH_FLED_EN	MASK(2, 0)
#define S2MF301_EN_FLED_PRE		MASK(1, 5)
#define S2MF301_FLED_FLASH_SHIFT	4
#define S2MF301_FLED_TORCH_SHIFT	0

#define S2MF301_FLED_FLASH_GPIO		0x1
#define S2MF301_FLED_TORCH_GPIO		0x2
#define S2MF301_FLED_I2C_EN		0x3

#define S2MF301_FLED_GPIO_EN1		0x01
#define S2MF301_FLED_GPIO_EN2		0x02
#define S2MF301_FLED_GPIO_EN3		0x03
#define S2MF301_FLED_GPIO_EN4		0x04

#define S2MF301_FALSH_FLED_EN_SHIFT 0x4

/* Mask for Mode control register */
#define S2MF301_FLED_MODE		MASK(2, 6)
#define S2MF301_EN_FLED_PRE		MASK(1, 5)
#define S2MF301_FLED_SOFT_ON_TIME	MASK(3, 1)
#define S2MF301_FLED_REG_RESET		MASK(1, 0)

/* FLED operating mode enable */
enum operating_mode {
	AUTO_MODE = 0,
	BOOST_MODE,
	TA_MODE,
	SYS_MODE,
};

enum cam_flash_mode {
	CAM_FLASH_MODE_NONE = 0,	//CAM2_FLASH_MODE_NONE=0,
	CAM_FLASH_MODE_OFF,		//CAM2_FLASH_MODE_OFF,
	CAM_FLASH_MODE_SINGLE,		//CAM2_FLASH_MODE_SINGLE,
	CAM_FLASH_MODE_TORCH,		//CAM2_FLASH_MODE_TORCH,
};

enum s2mf301_fled_mode {
	S2MF301_FLED_MODE_OFF,
	S2MF301_FLED_MODE_TORCH,
	S2MF301_FLED_MODE_FLASH,
	S2MF301_FLED_MODE_MOVIE,
	S2MF301_FLED_MODE_FACTORY,
	S2MF301_FLED_MODE_SHUTDOWN_OFF,
	S2MF301_FLED_MODE_MAX,
};

struct s2mf301_fled_chan {
	int id;
	u32 curr;
	u32 timer;
	u8 mode;
};

struct s2mf301_fled_platform_data {
	struct s2mf301_fled_chan *channel;
	int chan_num;
	int flash_gpio;
	int torch_gpio;
	u32 default_current;
	u32 max_current;
	u8 default_mode;
	u32 default_timer;
	int sysfs_input_data;
	unsigned int flash_current;
	unsigned int torch_current;
	unsigned int preflash_current;
	unsigned int movie_current;
	unsigned int factory_torch_current;
	unsigned int factory_flash_current;
	unsigned int flashlight_current[S2MF301_FLASH_LIGHT_MAX];
	bool en_flash;
	bool en_torch;
};

struct s2mf301_fled_data {
	struct s2mf301_fled_platform_data *pdata;
	struct s2mf301_fled_chan channel[S2MF301_CH_MAX];
	struct led_classdev cdev;
	struct device *dev;
	struct device *flash_dev;
	int rev_id;
	bool ic_301x;

	struct power_supply *psy_fled;
	struct power_supply_desc psy_fled_desc;
	int set_on_factory;
	int flash_gpio;
	int torch_gpio;
	int sysfs_input_data;
	int control_mode; /* 0 : I2C, 1 : GPIO */

	/* charger mode control */
	bool is_en_flash;
	struct power_supply *psy_chg;

	struct i2c_client *i2c;
	struct i2c_client *chg;
	struct mutex lock;
	u32 default_current;
	unsigned int flash_current;
	unsigned int torch_current;
	unsigned int preflash_current;
	unsigned int movie_current;
	unsigned int factory_torch_current;
	unsigned int factory_flash_current;
	unsigned int flashlight_current[S2MF301_FLASH_LIGHT_MAX];
};

int s2mf301_fled_set_mode_ctrl(int chan, enum cam_flash_mode cam_mode);
int s2mf301_fled_set_curr(int chan, enum cam_flash_mode cam_mode);
int s2mf301_fled_get_curr(int chan, enum cam_flash_mode cam_mode);
int s2mf301_led_mode_ctrl(int state);
extern void s2mf301_fled_set_operation_mode(int val);
extern int s2mf301_create_sysfs(struct class *class);
#endif
