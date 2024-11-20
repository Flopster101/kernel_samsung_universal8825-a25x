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
#include <linux/of_fdt.h>
#include <linux/string.h>

int sec_current_device = DEVICE_UNKNOWN;
EXPORT_SYMBOL(sec_current_device);
int sec_detect_init(void) {
	const char *machine_name = of_flat_dt_get_machine_name();
    if (strstr(machine_name, "A25") == 0) {
      sec_current_device = SEC_A25;
    } else if (strstr(machine_name, "A33") == 0) {
      sec_current_device = SEC_A33;
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