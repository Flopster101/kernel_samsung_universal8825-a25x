/*
 * sm5714.h - mfd driver for SM5714.
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SM5714_H__
#define __SM5714_H__

#define MFD_DEV_NAME "sm5714"

#define MAX_TORCH_LEVELS_5714 5

struct sm5714_regulator_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

struct sm5714_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;

	/* USBLDO */
	struct sm5714_regulator_data *regulators;
	int num_regulators;

	struct mfd_cell *sub_devices;
	int num_subdevs;
};

/* For SM5714 Flash LED */
enum sm5714_fled_mode {
	SM5714_FLED_MODE_OFF = 1,
	SM5714_FLED_MODE_MAIN_FLASH,
	SM5714_FLED_MODE_TORCH_FLASH,
	SM5714_FLED_MODE_PREPARE_FLASH,
	SM5714_FLED_MODE_CLOSE_FLASH,
	SM5714_FLED_MODE_PRE_FLASH,
};

enum {
	FLED_MODE_OFF       = 0x0,
	FLED_MODE_TORCH     = 0x1,
	FLED_MODE_FLASH     = 0x2,
	FLED_MODE_EXTERNAL  = 0x3,
};

typedef struct sm5714_fled_platform_data_tag {
	struct {
		const char *name;
		u8 flash_brightness;
		u8 preflash_brightness;
		u8 torch_brightness;
		bool apply_torch_brightness_control;
		u32 torch_brightness_levels[MAX_TORCH_LEVELS_5714];
		u8 timeout;

		int fen_pin;            /* GPIO-pin for Flash */
		int men_pin;            /* GPIO-pin for Torch */

		bool used_gpio_ctrl;
		int sysfs_input_data; //ys1978

		bool pre_fled;
		bool en_fled;
		bool en_mled;
	} led;
} sm5714_fled_platform_data;

struct sm5714_fled_data {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex fled_mutex;

	sm5714_fled_platform_data *pdata;

	int vbus_voltage;
	u8 torch_on_cnt;
	u8 flash_on_cnt;
	u8 flash_prepare_cnt;
};

extern int32_t sm5714_fled_mode_ctrl(int state, uint32_t brightness);
extern int sm5714_create_sysfs(struct class *class);

#endif /* __SM5714_H__ */
