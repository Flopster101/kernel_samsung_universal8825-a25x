/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83121 chipset
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_ic_HX83121.h"
#include "himax_modular.h"

static void hx83121a_chip_init(void)
{
	private_ts->chip_cell_type = CHIP_IS_IN_CELL;
	KI("%s:IC cell type = %d\n", __func__, private_ts->chip_cell_type);
	IC_CHECKSUM	=	HX_TP_BIN_CHECKSUM_CRC;
	/*Himax: Set FW and CFG Flash Address*/
	CID_VER_MAJ_FLASH_ADDR	= 0x21402;  /*0x00E802*/
	CID_VER_MAJ_FLASH_LENG	= 1;
	CID_VER_MIN_FLASH_ADDR	= 0x21403;  /*0x00E803*/
	CID_VER_MIN_FLASH_LENG	= 1;
	PANEL_VERSION_ADDR		= 0x21404;  /*0x00E804*/
	PANEL_VERSION_LENG		= 1;
	FW_VER_MAJ_FLASH_ADDR		= 0x21405;  /*0x00E805*/
	FW_VER_MAJ_FLASH_LENG		= 1;
	FW_VER_MIN_FLASH_ADDR		= 0x21406;  /*0x00E806*/
	FW_VER_MIN_FLASH_LENG		= 1;
	CFG_VER_MAJ_FLASH_ADDR	= 0x21500;  /*0x00E900*/
	CFG_VER_MAJ_FLASH_LENG	= 1;
	CFG_VER_MIN_FLASH_ADDR	= 0x21501;  /*0x00E901*/
	CFG_VER_MIN_FLASH_LENG	= 1;
}

void hx83121_burst_enable(uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[4];

	tmp_data[0] = 0x31;
	if (himax_bus_write(0x13, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	tmp_data[0] = (0x10 | auto_add_4_byte);
	if (himax_bus_write(0x0D, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
}

int hx83121_flash_write_burst(uint8_t *reg_byte, uint8_t *write_data)
{
	uint8_t data_byte[8];
	int i = 0, j = 0;

	for (i = 0; i < 4; i++)
		data_byte[i] = reg_byte[i];
	for (j = 4; j < 8; j++)
		data_byte[j] = write_data[j-4];

	if (himax_bus_write(0x00, data_byte, 8, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}
	return 0;
}

static int hx83121_register_read(uint8_t *read_addr, int read_length, uint8_t *read_data)
{
	uint8_t tmp_data[4];
	int i = 0;
	int address = 0;

	if (read_length > 256) {
		E("%s: read len over 256!\n", __func__);
		return LENGTH_FAIL;
	}
	if (read_length > 4)
		hx83121_burst_enable(1);
	else
		hx83121_burst_enable(0);

	address = (read_addr[3] << 24) + (read_addr[2] << 16) + (read_addr[1] << 8) + read_addr[0];
	i = address;
	tmp_data[0] = (uint8_t)i;
	tmp_data[1] = (uint8_t)(i >> 8);
	tmp_data[2] = (uint8_t)(i >> 16);
	tmp_data[3] = (uint8_t)(i >> 24);
	if (himax_bus_write(0x00, tmp_data, 4, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}
	tmp_data[0] = 0x00;
	if (himax_bus_write(0x0C, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}

	if (himax_bus_read(0x08, read_data, read_length, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return I2C_FAIL;
	}
	if (read_length > 4)
		hx83121_burst_enable(0);

	return 0;
}

#ifdef HX_RST_PIN_FUNC
static void hx83121_pin_reset(void)
{
	KI("%s: Now reset the Touch chip.\n", __func__);
	himax_rst_gpio_set(private_ts->rst_gpio, 0);
	msleep(20);
	himax_rst_gpio_set(private_ts->rst_gpio, 1);
	msleep(50);
}
#endif

static bool hx83121_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_writ[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];

	do {
		if (cnt == 0 || (tmp_data[0] != 0xA5 && tmp_data[0] != 0x00 && tmp_data[0] != 0x87)) {
			tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x5C;
			tmp_writ[3] = 0x00; tmp_writ[2] = 0x00; tmp_writ[1] = 0x00; tmp_writ[0] = 0xA5;
			hx83121_flash_write_burst(tmp_addr, tmp_writ);
		}
		msleep(20);

		/* check fw status */
		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0xA8;
		hx83121_register_read(tmp_addr, DATA_LEN_4, tmp_data);

		if (tmp_data[0] != 0x05) {
			KI("%s: Do not need wait FW, Status = 0x%02X!\n", __func__, tmp_data[0]);
			break;
		}

		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x5C;
		hx83121_register_read(tmp_addr, DATA_LEN_4, tmp_data);
		KI("%s: cnt = %d, data[0] = 0x%02X!\n", __func__, cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 50) && check_en == true);

	cnt = 0;


	do {
		/*===========================================
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 *===========================================
		 */
		tmp_data[0] = 0x27;
		if (himax_bus_write(0x31, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*===========================================
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 *===========================================
		 */
		tmp_data[0] = 0x95;
		if (himax_bus_write(0x32, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/* ======================
		 *Check enter_save_mode
		 *======================
		 */
		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0xA8;
		hx83121_register_read(tmp_addr, ADDR_LEN_4, tmp_data);
		KI("%s: Check enter_save_mode data[0]=%X\n", __func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
		#if 0
			/*=====================================
			 *Reset TCON
			 *=====================================
			 */
			tmp_addr[3] = 0x80; tmp_addr[2] = 0x02; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
			tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
			hx83121_flash_write_burst(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
			tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x01;
			hx83121_flash_write_burst(tmp_addr, tmp_data);
			/*=====================================
			 *Reset ADC
			 *=====================================
			 */
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x94;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			hx83121_flash_write_burst(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			hx83121_flash_write_burst(tmp_addr, tmp_data);
		#endif
			return true;
		}
		usleep_range(10000, 10001);
#ifdef HX_RST_PIN_FUNC
		hx83121_pin_reset();
#endif

	} while (cnt++ < 15);

	return false;
}

static void hx83121a_sense_on(uint8_t FlashMode)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};

	I("Enter %s\n", __func__);
	g_core_fp.fp_interface_on();
	g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr,
		sizeof(pfw_op->data_clear), pfw_op->data_clear, 0);
	/*msleep(20);*/
	usleep_range(10000, 11000);
	if (!FlashMode) {
#ifdef HX_RST_PIN_FUNC
		g_core_fp.fp_ic_reset(false, false);
#else
		g_core_fp.fp_system_reset();
#endif
	} else {
		if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0)
			E("%s: i2c access fail!\n", __func__);

		if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0)
			E("%s: i2c access fail!\n", __func__);
	}
}

static bool hx83121a_sense_off(bool check_en)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];

	I("Enter %s\n", __func__);

	do {
		if (cnt == 0 || (tmp_data[0] != 0xA5 && tmp_data[0] != 0x00 && tmp_data[0] != 0x87))
			g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr, DATA_LEN_4, pfw_op->data_fw_stop, 0);
		/*msleep(20);*/
		usleep_range(10000, 10001);

		/* check fw status */
		g_core_fp.fp_register_read(pic_op->addr_cs_central_state, ADDR_LEN_4, tmp_data, 0);

		if (tmp_data[0] != 0x05) {
			KI("%s: Do not need wait FW, Status = 0x%02X!\n", __func__, tmp_data[0]);
			break;
		}

		g_core_fp.fp_register_read(pfw_op->addr_ctrl_fw_isr, 4, tmp_data, false);
		KI("%s: cnt = %d, data[0] = 0x%02X!\n", __func__, cnt, tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (++cnt < 10) && check_en == true);

	cnt = 0;

	do {
		/*===========================================
		 *I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
		 *===========================================
		 */
		tmp_data[0] = 0x27;
		if (himax_bus_write(0x31, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*===========================================
		 *I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
		 *===========================================
		 */
		tmp_data[0] = 0x95;
		if (himax_bus_write(0x32, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return false;
		}

		/* ======================
		 *Check enter_save_mode
		 *======================
		 */
		tmp_addr[3] = 0x90; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0xA8;
		hx83121_register_read(tmp_addr, ADDR_LEN_4, tmp_data);
		KI("%s: Check enter_save_mode data[0]=%X\n", __func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
		#if 0
			/*=====================================
			 *Reset TCON
			 *=====================================
			 */
			tmp_addr[3] = 0x80; tmp_addr[2] = 0x02; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
			tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
			hx83121_flash_write_burst(tmp_addr, tmp_data);
			usleep_range(1000, 1001);
		#endif
			return true;
		}
		/*msleep(10);*/
		usleep_range(5000, 5001);
#ifdef HX_RST_PIN_FUNC
		himax_rst_gpio_set(private_ts->rst_gpio, 0);
		msleep(20);
		himax_rst_gpio_set(private_ts->rst_gpio, 1);
		msleep(50);
#endif

	} while (cnt++ < 5);

	return false;
}

static bool hx83121a_read_event_stack(uint8_t *buf, uint8_t length)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
	struct timespec64 t_start, t_end, t_delta;
#else
	struct timespec t_start, t_end, t_delta;
#endif
	int len = length;
	int i2c_speed = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
	if (private_ts->debug_log_level & BIT(2))
		ktime_get_real_ts64(&t_start);
#else
	if (private_ts->debug_log_level & BIT(2))
		getnstimeofday(&t_start);
#endif

	himax_bus_read(pfw_op->addr_event_addr[0], buf, length, HIMAX_I2C_RETRY_TIMES);

	if (private_ts->debug_log_level & BIT(2)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
		ktime_get_real_ts64(&t_end);
#else
		getnstimeofday(&t_end);
#endif
		t_delta.tv_nsec = (t_end.tv_sec * 1000000000 + t_end.tv_nsec) - (t_start.tv_sec * 1000000000 + t_start.tv_nsec); /*ns*/
		i2c_speed =	(len * 9 * 1000000 / (int)t_delta.tv_nsec) * 13 / 10;
		private_ts->bus_speed = (int)i2c_speed;
		}
	return 1;
}

static void himax_hx83121a_reg_re_init(void)
{
	KI("%s:Entering!\n", __func__);
	himax_in_parse_assign_cmd(hx83121a_fw_addr_raw_out_sel, pfw_op->addr_raw_out_sel, sizeof(pfw_op->addr_raw_out_sel));
	himax_in_parse_assign_cmd(hx83121a_data_df_rx, pdriver_op->data_df_rx, sizeof(pdriver_op->data_df_rx));
	himax_in_parse_assign_cmd(hx83121a_data_df_tx, pdriver_op->data_df_tx, sizeof(pdriver_op->data_df_tx));
//	kp_himax_in_parse_assign_cmd(hx83121a_data_df_x_res, (*kp_pdriver_op)->data_df_x_res, sizeof((*kp_pdriver_op)->data_df_x_res));
//	kp_himax_in_parse_assign_cmd(hx83121a_data_df_y_res, (*kp_pdriver_op)->data_df_y_res, sizeof((*kp_pdriver_op)->data_df_y_res));
//	kp_himax_in_parse_assign_cmd(hx83121a_ic_adr_tcon_rst, (*kp_pic_op)->addr_tcon_on_rst, sizeof((*kp_pic_op)->addr_tcon_on_rst));
}

static void himax_hx83121a_func_re_init(void)
{
	KI("%s:Entering!\n", __func__);

	g_core_fp.fp_chip_init = hx83121a_chip_init;
	g_core_fp.fp_sense_on = hx83121a_sense_on;
	g_core_fp.fp_sense_off = hx83121a_sense_off;
	g_core_fp.fp_read_event_stack = hx83121a_read_event_stack;
}

static bool hx83121_chip_detect(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_addr[DATA_LEN_4];
	bool ret_data = false;
	int ret = 0;
	int i = 0;

#ifdef HX_RST_PIN_FUNC
	hx83121_pin_reset();
#endif

	if (himax_bus_read(0x13, tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return false;
	}
	
#if defined(HX_ZERO_FLASH)
	if (hx83121_sense_off(false) == false) {
#else
	if (hx83121_sense_off(true) == false) {
#endif
		ret_data = false;
		E("%s:hx83121_sense_off Fail:\n", __func__);
		return ret_data;
	}

	for (i = 0; i < 5; i++) {
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xD0;
		if (hx83121_register_read(tmp_addr, DATA_LEN_4, tmp_data) != 0) {
			ret_data = false;
			E("%s:hx83121_register_read Fail:\n", __func__);
			return ret_data;
		}
		KI("%s:Read driver IC ID = %X,%X,%X\n", __func__, tmp_data[3], tmp_data[2], tmp_data[1]); /*83,12,1X*/

		if ((tmp_data[3] == 0x83) && (tmp_data[2] == 0x12) && (tmp_data[1] == 0x1a)) {
			strlcpy(private_ts->chip_name, HX_83121A_SERIES_PWON, 30);

			ic_data->flash_size = HX83121A_FLASH_SIZE;

			KI("%s:detect IC HX83121A successfully\n", __func__);

			ret = himax_mcu_in_cmd_struct_init();
			if (ret < 0) {
				ret_data = false;
				E("%s:cmd_struct_init Fail:\n", __func__);
				return ret_data;
			}

			himax_mcu_in_cmd_init();

			himax_hx83121a_reg_re_init();
			himax_hx83121a_func_re_init();

			ret_data = true;
			break;
		}

		ret_data = false;
		E("%s:Read driver ID register Fail:\n", __func__);
		E("Could NOT find Himax Chipset\n");
		E("Please check 1.VCCD,VCCA,VSP,VSN\n");
		E("2. LCM_RST,TP_RST\n");
		E("3. Power On Sequence\n");

	}

	return ret_data;
}

bool _hx83121_init(void)
{
	bool ret = false;

	I("%s\n", __func__);
	ret = hx83121_chip_detect();
	return ret;
}
