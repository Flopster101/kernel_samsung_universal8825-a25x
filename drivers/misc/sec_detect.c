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

#include <linux/sec_detect.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/string.h>

int sec_current_device = DEVICE_UNKNOWN;
bool sec_needs_decon;
bool sec_needs_blic = false;
bool sec_doze = false; // Uses Samsung DRM Doze?
EXPORT_SYMBOL(sec_needs_decon);
EXPORT_SYMBOL(sec_current_device);
EXPORT_SYMBOL(sec_needs_blic);
EXPORT_SYMBOL(sec_doze);

int sec_detect_init(void) {
    struct device_node *root;
    const char *machine_name;

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

    SEC_DETECT_LOG("Current machine name: %s\n", machine_name);

    if (strstr(machine_name, "A25") != NULL) {
        sec_current_device = SEC_A25;
        sec_needs_decon = false;
        sec_doze = true;
    } else if (strstr(machine_name, "A33") != NULL) {
        sec_current_device = SEC_A33;
        sec_needs_decon = true;
    } else if (strstr(machine_name, "A53") != NULL) {
        sec_current_device = SEC_A53;
        sec_needs_decon = true;
        sec_doze = true;
    } else if (strstr(machine_name, "M33") != NULL) {
        sec_current_device = SEC_M33;
        sec_needs_decon = true;
    } else if (strstr(machine_name, "M34") != NULL) {
        sec_current_device = SEC_M34;
        sec_needs_decon = false;
        sec_doze = true;
    } else if (strstr(machine_name, "GTA4XLS") != NULL) {
        sec_current_device = SEC_GTA4XLS;
        sec_needs_decon = false;
        sec_needs_blic = true;
    }
    return 0;
}

void sec_detect_exit(void) {
    return;
}

rootfs_initcall(sec_detect_init); // runs before regular drivers init
module_exit(sec_detect_exit);

MODULE_AUTHOR("Flopster101");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Detects the Samsung device currently running this kernel.");
