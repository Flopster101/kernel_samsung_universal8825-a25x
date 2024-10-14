/*
 * s2mpu14.h - Driver for the s2mpu14
 *
 *  Copyright (C) 2020 Samsung Electrnoics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __S2MPU14_MFD_H__
#define __S2MPU14_MFD_H__
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define MFD_DEV_NAME "s2mpu14"
/**
 * sec_regulator_data - regulator data
 * @id: regulator id
 * @initdata: regulator init data (contraints, supplies, ...)
 */
struct s2mpu14_regulator_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

/*
 * sec_opmode_data - regulator operation mode data
 * @id: regulator id
 * @mode: regulator operation mode
 */
struct sec_opmode_data {
	int id;
	unsigned int mode;
};

/*
 * samsung regulator operation mode
 * SEC_OPMODE_OFF	Regulator always OFF
 * SEC_OPMODE_ON	Regulator always ON
 * SEC_OPMODE_LOWPOWER  Regulator is on in low-power mode
 * SEC_OPMODE_SUSPEND   Regulator is changed by PWREN pin
 *			If PWREN is high, regulator is on
 *			If PWREN is low, regulator is off
 */
enum sec_opmode {
	SEC_OPMODE_OFF,
	SEC_OPMODE_SUSPEND,
	SEC_OPMODE_LOWPOWER,
	SEC_OPMODE_ON,
	SEC_OPMODE_TCXO = 0x2,
	SEC_OPMODE_MIF = 0x2,
};

struct s2mpu14_platform_data {

	bool use_i2c_speedy;
	bool wakeup;
	bool g3d_en;

	/* IRQ */
	int irq_base;

	int (*cfg_pmic_irq)(void);
	int device_type;
	int ono;
	int buck_ramp_delay;

	bool	b3s_afm_warn1_en;
	bool	b4s_afm_warn1_en;
	bool	b6s_afm_warn_en;
	int		b3s_afm_warn1_cnt;
	int		b4s_afm_warn1_cnt;
	int		b6s_afm_warn_cnt;
	bool	b3s_afm_warn1_dvs_mask;
	bool	b4s_afm_warn1_dvs_mask;
	bool	b6s_afm_warn_dvs_mask;
	int		b3s_afm_warn1_lv;
	int		b3s_afm_warn2_lv;
	int		b4s_afm_warn1_lv;
	int		b4s_afm_warn2_lv;
	int		b6s_afm_warn_lv;

	/* power meter */
	int adc_mode;

	/* wtsr */
	int wtsr_en;

	/* sel_vgpio (control_sel) */
	u32 *sel_vgpio;

	int num_regulators;
	int num_rdata;
	int num_subdevs;
	struct s2mpu14_regulator_data *regulators;
	struct sec_opmode_data *opmode;
	struct mfd_cell *sub_devices;
};

struct s2mpu14 {
	struct regmap *regmap;
};
#endif /* __S2MPU14_MFD_H__ */
