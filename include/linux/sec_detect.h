/*
 * Author: @Flopster101
 * Based on AkiraNoSushi's work for the Mi439 project.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_SEC_H
#define _LINUX_SEC_H

#include <linux/types.h>

#define SEC_DETECT_LOG(fmt, ...) printk(KERN_INFO "sec_detect: " fmt, ##__VA_ARGS__)
static const char *sec_detect_label = "sec_detect: ";

enum SEC_devices {
	DEVICE_UNKNOWN = -1,
	SEC_A25,
	SEC_A33,
	SEC_A53,
	SEC_M33,
	SEC_M34,
	SEC_GTA4XLS
};

extern enum SEC_devices sec_devices;
extern int sec_current_device;
extern bool sec_needs_decon;
extern bool sec_needs_blic;
extern bool sec_doze;
extern bool sec_lcd_device;

// Camera params
extern bool mcd_disable_dual_sync;
extern bool mcd_camera_rear_dual_cal;
extern bool mcd_use_leds_flash_charging_voltage_control;
extern bool mcd_use_camera_adaptive_mipi;
extern bool mcd_use_imx258_13mp_full_size;
extern bool mcd_apply_mirror_vertical_flip;
extern bool mcd_simplify_ois_init;
extern bool mcd_camera_front_fixed_focus;
extern bool mcd_modify_cal_map_for_swremosaic_lib;
extern bool mcd_front_otprom_eeprom;
extern bool mcd_camera_uwide_dualized;
extern bool mcd_read_dual_cal_firmware_data;
extern bool mcd_config_camera_eeprom_dualized;
extern bool mcd_config_check_hw_version_for_mcu_fw_upload;
extern bool mcd_use_camera_act_driver_soft_landing;
extern bool mcd_use_ois_hall_data_for_vdis;

#ifdef CONFIG_SEC_DETECT_SYSFS
extern char sec_current_device_name[32];
#endif

#endif /* _LINUX_SEC_H */
