#include <linux/sec_detect.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/string.h>

int sec_current_device = DEVICE_UNKNOWN;
bool sec_needs_decon;
EXPORT_SYMBOL(sec_needs_decon);
EXPORT_SYMBOL(sec_current_device);

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
        SEC_DETECT_LOG("Drivers to load: USDM panels, hl7132, s2mf301\n");
    } else if (strstr(machine_name, "A33") != NULL) {
        sec_current_device = SEC_A33;
        sec_needs_decon = true;
        SEC_DETECT_LOG("Drivers to load: DECON panels, s2mu106, sm5451\n");
    } else if (strstr(machine_name, "A53") != NULL) {
        sec_current_device = SEC_A53;
        sec_needs_decon = true;
        SEC_DETECT_LOG("Drivers to load: DECON panels, sm5714, sm5451\n");
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
