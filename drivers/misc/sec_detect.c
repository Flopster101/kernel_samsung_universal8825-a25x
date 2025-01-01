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
#ifdef CONFIG_SEC_DETECT_SYSFS
#include <linux/kobject.h>
#include <linux/sysfs.h>
#endif

int sec_current_device = DEVICE_UNKNOWN;
char sec_current_device_name[32] = "Unknown";
bool sec_needs_decon;
bool sec_needs_blic = false;
bool sec_doze = false; // Uses Samsung DRM Doze?
EXPORT_SYMBOL(sec_needs_decon);
EXPORT_SYMBOL(sec_current_device);
EXPORT_SYMBOL(sec_needs_blic);
EXPORT_SYMBOL(sec_doze);

#ifdef CONFIG_SEC_DETECT_SYSFS
// Sysfs attribute to show the current device name
static ssize_t device_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 32, "%s\n", sec_current_device_name);
}

static struct kobj_attribute device_name_attr = __ATTR(device_name, 0444, device_name_show, NULL);

static struct attribute *attrs[] = {
	&device_name_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *device_kobj;
#endif

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

	SEC_DETECT_LOG("Current machine name: %s\n", machine_name);

	if (strstr(machine_name, "A25") != NULL) {
		sec_current_device = SEC_A25;
		strncpy(sec_current_device_name, "a25x", sizeof(sec_current_device_name));
		sec_needs_decon = false;
		sec_doze = true;
	} else if (strstr(machine_name, "A33") != NULL) {
		sec_current_device = SEC_A33;
		strncpy(sec_current_device_name, "a33x", sizeof(sec_current_device_name));
		sec_needs_decon = true;
	} else if (strstr(machine_name, "A53") != NULL) {
		sec_current_device = SEC_A53;
		strncpy(sec_current_device_name, "a53x", sizeof(sec_current_device_name));
		sec_needs_decon = true;
		sec_doze = true;
	} else if (strstr(machine_name, "M33") != NULL) {
		sec_current_device = SEC_M33;
		strncpy(sec_current_device_name, "m33x", sizeof(sec_current_device_name));
		sec_needs_decon = true;
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
MODULE_DESCRIPTION("Detects the Samsung device currently running this kernel. Also exposes it in /sys/kernel/sec_detect/device_name.");
