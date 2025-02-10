// SPDX-License-Identifier: GPL-2.0-only
/*
 * Author: @Flopster101
 * Based on AkiraNoSushi's work for the Mi439 project.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sec_detect.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/string.h>
#ifdef CONFIG_SEC_DETECT_SYSFS
#include <linux/kobject.h>
#include <linux/sysfs.h>
#endif

int sec_current_device = DEVICE_UNKNOWN;
char sec_current_device_name[32] = "Unknown";
bool sec_needs_decon;
bool sec_needs_blic = false;
bool sec_doze = false; // Uses Samsung DRM Doze?
bool sec_lcd_device = false;
bool sec_legacy_sinput = false; // Uses old sec_input driver?
bool sec_legacy_usbpd = false; // Uses old slsi usbpd platform?
// No device seems to use the STM driver.
// bool sec_needs_stm = false; // Needs STM drivers?
EXPORT_SYMBOL(sec_needs_decon);
EXPORT_SYMBOL(sec_current_device);
EXPORT_SYMBOL(sec_needs_blic);
EXPORT_SYMBOL(sec_doze);
EXPORT_SYMBOL(sec_lcd_device);
EXPORT_SYMBOL(sec_legacy_sinput);
EXPORT_SYMBOL(sec_legacy_usbpd);

// Camera params
bool mcd_disable_dual_sync = false;
EXPORT_SYMBOL(mcd_disable_dual_sync);

bool mcd_camera_rear_dual_cal = false;
EXPORT_SYMBOL(mcd_camera_rear_dual_cal);

bool mcd_use_leds_flash_charging_voltage_control = false;
EXPORT_SYMBOL(mcd_use_leds_flash_charging_voltage_control);

bool mcd_use_camera_adaptive_mipi = false;
EXPORT_SYMBOL(mcd_use_camera_adaptive_mipi);

bool mcd_use_imx258_13mp_full_size = false;
EXPORT_SYMBOL(mcd_use_imx258_13mp_full_size);

bool mcd_apply_mirror_vertical_flip = false;
EXPORT_SYMBOL(mcd_apply_mirror_vertical_flip);

bool mcd_simplify_ois_init = false;
EXPORT_SYMBOL(mcd_simplify_ois_init);

bool mcd_modify_cal_map_for_swremosaic_lib = false;
EXPORT_SYMBOL(mcd_modify_cal_map_for_swremosaic_lib);

bool mcd_front_otprom_eeprom = false;
EXPORT_SYMBOL(mcd_front_otprom_eeprom);

bool mcd_camera_uwide_dualized = false;
EXPORT_SYMBOL(mcd_camera_uwide_dualized);

bool mcd_read_dual_cal_firmware_data = false;
EXPORT_SYMBOL(mcd_read_dual_cal_firmware_data);

bool mcd_camera_front_fixed_focus = false;
EXPORT_SYMBOL(mcd_camera_front_fixed_focus);

bool mcd_config_camera_eeprom_dualized = false;
EXPORT_SYMBOL(mcd_config_camera_eeprom_dualized);

bool mcd_config_check_hw_version_for_mcu_fw_upload = false;
EXPORT_SYMBOL(mcd_config_check_hw_version_for_mcu_fw_upload);

bool mcd_use_camera_act_driver_soft_landing = false;
EXPORT_SYMBOL(mcd_use_camera_act_driver_soft_landing);

bool mcd_use_ois_hall_data_for_vdis = false;
EXPORT_SYMBOL(mcd_use_ois_hall_data_for_vdis);

bool mcd_use_hi1336c_setfile = false;
EXPORT_SYMBOL(mcd_use_hi1336c_setfile);

bool mcd_camera_use_aois = false;
EXPORT_SYMBOL(mcd_camera_use_aois);

// Power driver configuration
bool sec_power_sm5451 = false;
bool sec_power_sm5714 = false;
bool sec_power_hl7132 = false;
bool sec_power_s2mf301 = false;
bool sec_power_s2mu106 = false;
EXPORT_SYMBOL(sec_power_sm5451);
EXPORT_SYMBOL(sec_power_sm5714);
EXPORT_SYMBOL(sec_power_hl7132);
EXPORT_SYMBOL(sec_power_s2mf301);
EXPORT_SYMBOL(sec_power_s2mu106);

#ifdef CONFIG_SEC_DETECT_SYSFS
// Sysfs attribute to show the current device name
static ssize_t device_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 32, "%s\n", sec_current_device_name);
}

// Sysfs attribute to show the current device model
static ssize_t device_model_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	const char *model_name = "Unknown";
	if (sec_current_device >= 0 && sec_current_device < ARRAY_SIZE(device_names))
		model_name = device_names[sec_current_device];
	return snprintf(buf, 32, "%s\n", model_name);
}

static struct kobj_attribute device_name_attr = __ATTR(device_name, 0444, device_name_show, NULL);
static struct kobj_attribute device_model_attr = __ATTR(device_model, 0444, device_model_show, NULL);

static struct attribute *attrs[] = {
	&device_name_attr.attr,
	&device_model_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *device_kobj;
#endif

static inline void setup_camera_params(void) {
	switch (sec_current_device) {
	case SEC_A25:
		mcd_camera_rear_dual_cal = true;
		mcd_use_imx258_13mp_full_size = true;
		mcd_use_leds_flash_charging_voltage_control = true;
		mcd_use_ois_hall_data_for_vdis = true;
		mcd_apply_mirror_vertical_flip = true;
		mcd_front_otprom_eeprom = true;
		mcd_modify_cal_map_for_swremosaic_lib = true;
		mcd_simplify_ois_init = true;
		mcd_use_camera_adaptive_mipi = true;
		mcd_read_dual_cal_firmware_data = true;
		mcd_camera_use_aois = true;
		break;
	case SEC_A33:
		mcd_camera_front_fixed_focus = true;
		mcd_camera_rear_dual_cal = true;
		mcd_config_check_hw_version_for_mcu_fw_upload = true;
		mcd_disable_dual_sync = true;
		mcd_use_imx258_13mp_full_size = true;
		mcd_use_leds_flash_charging_voltage_control = true;
		mcd_use_ois_hall_data_for_vdis = true;
		mcd_camera_uwide_dualized = true;
		mcd_read_dual_cal_firmware_data = true;
		break;
	case SEC_A53:
		mcd_camera_front_fixed_focus = true;
		mcd_camera_rear_dual_cal = true;
		mcd_disable_dual_sync = true;
		mcd_use_ois_hall_data_for_vdis = true;
		mcd_camera_uwide_dualized = true;
		mcd_read_dual_cal_firmware_data = true;
		mcd_use_leds_flash_charging_voltage_control = true;
		mcd_use_hi1336c_setfile = true;
		break;
	case SEC_M33:
		mcd_camera_rear_dual_cal = true;
		mcd_disable_dual_sync = true;
		mcd_modify_cal_map_for_swremosaic_lib = true;
		mcd_use_camera_adaptive_mipi = true;
		mcd_use_camera_act_driver_soft_landing = true;
		mcd_read_dual_cal_firmware_data = true;
		break;
	case SEC_M34:
		mcd_camera_rear_dual_cal = true;
		mcd_use_ois_hall_data_for_vdis = true;
		mcd_apply_mirror_vertical_flip = true;
		mcd_modify_cal_map_for_swremosaic_lib = true;
		mcd_simplify_ois_init = true;
		mcd_use_camera_adaptive_mipi = true;
		mcd_read_dual_cal_firmware_data = true;
		mcd_use_leds_flash_charging_voltage_control = true;	// Not enabled in stock, but required here?
		mcd_camera_use_aois = true;
		break;
	case SEC_GTA4XLS:
		mcd_use_camera_act_driver_soft_landing = true;
		break;
	default:
		break;
	}
}

// Function to initialize power driver configuration
static inline void setup_power_drivers(void) {
	switch (sec_current_device) {
	case SEC_A25:
		sec_power_s2mf301 = true;
		sec_power_hl7132 = true;
		break;
	case SEC_A33:
		sec_power_s2mu106 = true;
		sec_power_sm5451 = true;
		sec_power_hl7132 = true;
		break;
	case SEC_A53:
		sec_power_sm5714 = true;
		sec_power_sm5451 = true;
		break;
	case SEC_M33:
		sec_power_sm5714 = true;
		sec_power_sm5451 = true;
		break;
	case SEC_M34:
		sec_power_sm5714 = true;
		sec_power_sm5451 = true;
		break;
	case SEC_GTA4XLS:
		sec_power_sm5714 = true;
		break;
	default:
		break;
	}
}

// New function to print machine name and sec_ variables
static inline void print_sec_variables(const char *machine_name) {
	SEC_DETECT_LOG("Current machine name: %s\n", machine_name);
	SEC_DETECT_LOG("sec_needs_blic = %s\n", sec_needs_blic ? "true" : "false");
	SEC_DETECT_LOG("sec_needs_decon = %s\n", sec_needs_decon ? "true" : "false");
	SEC_DETECT_LOG("sec_doze = %s\n", sec_doze ? "true" : "false");
	SEC_DETECT_LOG("sec_lcd_device = %s\n", sec_lcd_device ? "true" : "false");
	SEC_DETECT_LOG("sec_legacy_sinput = %s\n", sec_legacy_sinput ? "true" : "false");
	SEC_DETECT_LOG("sec_legacy_usbpd = %s\n", sec_legacy_usbpd ? "true" : "false");
	SEC_DETECT_LOG("Power drivers present:\n");
	SEC_DETECT_LOG("sm5451 = %s\n", sec_power_sm5451 ? "true" : "false");
	SEC_DETECT_LOG("sm5714 = %s\n", sec_power_sm5714 ? "true" : "false");
	SEC_DETECT_LOG("hl7132 = %s\n", sec_power_hl7132 ? "true" : "false");
	SEC_DETECT_LOG("s2mf301 = %s\n", sec_power_s2mf301 ? "true" : "false");
	SEC_DETECT_LOG("s2mu106 = %s\n", sec_power_s2mu106 ? "true" : "false");
}

int sec_detect_init(void) {
	struct device_node *root;
	const char *machine_name;
	int retval = 0;

	root = of_find_node_by_path("/");
	if (!root) {
		SEC_DETECT_LOG("Failed to find device tree root\n");
		return -ENOENT;
	}

	machine_name = of_get_property(root, "model", NULL);
	if (!machine_name)
		machine_name = of_get_property(root, "compatible", NULL);

	if (!machine_name) {
		SEC_DETECT_LOG("Failed to find machine name\n");
		return -ENOENT;
	}

	if (strstr(machine_name, "A25") != NULL) {
		sec_current_device = SEC_A25;
		strncpy(sec_current_device_name, "a25x", sizeof(sec_current_device_name));
		sec_needs_decon = false;
		sec_doze = true;
	} else if (strstr(machine_name, "A33") != NULL) {
		sec_current_device = SEC_A33;
		strncpy(sec_current_device_name, "a33x", sizeof(sec_current_device_name));
		sec_needs_decon = true;
		sec_legacy_usbpd = true;
	} else if (strstr(machine_name, "A53") != NULL) {
		sec_current_device = SEC_A53;
		strncpy(sec_current_device_name, "a53x", sizeof(sec_current_device_name));
		sec_needs_decon = true;
		sec_doze = true;
	} else if (strstr(machine_name, "M33") != NULL) {
		sec_current_device = SEC_M33;
		strncpy(sec_current_device_name, "m33x", sizeof(sec_current_device_name));
		sec_needs_decon = true;
		sec_needs_blic = true;
		sec_lcd_device = true;
		sec_legacy_sinput = true;
	} else if (strstr(machine_name, "M34") != NULL) {
		sec_current_device = SEC_M34;
		strncpy(sec_current_device_name, "m34x", sizeof(sec_current_device_name));
		sec_needs_decon = false;
		sec_doze = true;
	} else if (strstr(machine_name, "GTA4XLS") != NULL) {
		sec_current_device = SEC_GTA4XLS;
		strncpy(sec_current_device_name, "gta4xls", sizeof(sec_current_device_name));
		sec_needs_decon = false;
		sec_needs_blic = true;
	}

#ifdef CONFIG_SEC_DETECT_SYSFS
	// Create the sysfs entry
	device_kobj = kobject_create_and_add("sec_detect", kernel_kobj);
	if (!device_kobj)
		return -ENOMEM;

	retval = sysfs_create_group(device_kobj, &attr_group);
	if (retval)
		kobject_put(device_kobj);
#endif

	setup_camera_params();
	setup_power_drivers();

	// Print machine name and sec_ variables
	print_sec_variables(machine_name);

	return retval;
}

void sec_detect_exit(void) {
#ifdef CONFIG_SEC_DETECT_SYSFS
	kobject_put(device_kobj);
#endif
	return;
}

rootfs_initcall(sec_detect_init); // runs before regular drivers init
module_exit(sec_detect_exit);

MODULE_AUTHOR("Flopster101");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Detects the Samsung device currently running this kernel. Also exposes device information through sysfs.");
