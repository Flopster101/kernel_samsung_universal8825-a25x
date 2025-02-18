/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <asm/bitops.h>

#include "npu-debug.h"
#include "npu-log.h"
#include "npu-device.h"
#include "npu-ver.h"

#define DEBUG_FS_ROOT_NAME "npu"
#define DEBUG_FS_UNITTEST_NAME "idiot"
#define DEBUG_FS_LAYER_INFO_NAME "layer_range"
#define DEBUG_FS_COLD_BOOT_NAME "cold_boot"
#define NPU_DEBUG_FILENAME_LEN 32 /* Maximum file name length under npu */

#ifdef CONFIG_VISION_UNITTEST
#define IDIOT_ALL_TESTCASE_INCLUDE "idiot-npu-all-tests.h"
#include "idiot-decl.h"
#endif

/* Singleton reference to debug object */
static struct npu_debug *npu_debug_ref;

typedef enum {
	FS_READY = 0,
} npu_debug_state_bits_e;

static inline void set_state_bit(npu_debug_state_bits_e state_bit)
{
	int old = test_and_set_bit(state_bit, &(npu_debug_ref->state));

	if (old) {
		npu_warn("state(%d): state set requested but already set.", state_bit);
	}
}

static inline void clear_state_bit(npu_debug_state_bits_e state_bit)
{
	int old = test_and_clear_bit(state_bit, &(npu_debug_ref->state));

	if (!old) {
		npu_warn("state(%d): state clear requested but already cleared.", state_bit);
	}
}

static inline int check_state_bit(npu_debug_state_bits_e state_bit)
{
	return test_bit(state_bit, &(npu_debug_ref->state));
}

#ifdef CONFIG_VISION_UNITTEST
static const struct file_operations npu_debug_unittest_fops = {
    .owner = THIS_MODULE, .open = IDIOT_open, .read = IDIOT_read, .write = IDIOT_write,
};
#endif

static int npu_hw_debug_layer_range_show(struct seq_file *file, void *unused)
{
	struct npu_device	*npu_device;
	struct device		*dev;

	/* Retrieve struct device to use devm_XX api */
	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	BUG_ON(!npu_device);
	dev = npu_device->dev;
	BUG_ON(!dev);

	seq_printf(file, "layer_range start:%#x / end:%#x\n",
			npu_device->system.layer_start, npu_device->system.layer_end);
	seq_puts(file, "Command to set layer_range to run layer by layer\n");
	seq_printf(file, "%#x is default value and it is ignored\n",
			NPU_SET_DEFAULT_LAYER);
	seq_puts(file, " echo ${start} ${end} > /d/npu/hardware/layer_range\n");
	seq_puts(file, "Please use a decimal number\n");

	return 0;
}

static int npu_hw_debug_layer_range_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, npu_hw_debug_layer_range_show,
			inode->i_private);
}

static ssize_t npu_hw_debug_layer_range_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;

	struct npu_device	*npu_device;
	struct device		*dev;

	char buf[30];
	ssize_t size;
	unsigned int start_layer;
	unsigned int end_layer;

	/* Retrieve struct device to use devm_XX api */
	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	BUG_ON(!npu_device);
	dev = npu_device->dev;
	BUG_ON(!dev);


	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%u %u", &start_layer, &end_layer);
	if (ret != 2) {
		npu_err("Failed to get layer_range parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&npu_device->sessionmgr.mlock);
	if (atomic_read(&npu_device->sessionmgr.session_cnt)) {
		npu_err("device already opened (%u)\n",
				atomic_read(&npu_device->sessionmgr.session_cnt));
	} else {
		npu_device->system.layer_start = start_layer;
		npu_device->system.layer_end = end_layer;
		npu_info("layer_range is set (%d ~ %d)\n",
				start_layer, end_layer);
	}
	mutex_unlock(&npu_device->sessionmgr.mlock);

	return count;
p_err:
	return ret;
}

static const struct file_operations npu_hw_debug_layer_range_fops = {
	.open		= npu_hw_debug_layer_range_open,
	.read		= seq_read,
	.write		= npu_hw_debug_layer_range_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int npu_hw_debug_cold_boot_show(struct seq_file *file, void *unused)
{
	int ret = 0;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	seq_printf(file, "Current npu's fw_cold_boot flag is %s(%d).\n",
			npu_system->fw_cold_boot ? "true" : "false", npu_system->fw_cold_boot);

	seq_puts(file, "Please use this command for enabling cold booot : \"echo 1 > /d/npu/cold_boot\"\n");

	return 0;
p_err:
	return ret;
}

static int npu_hw_debug_cold_boot_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_hw_debug_cold_boot_show,
			inode->i_private);
}

static ssize_t npu_hw_debug_cold_boot_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	char buf[30];
	ssize_t size;
	int fw_cold_boot;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtoint(buf, 10, &fw_cold_boot);
	if (ret) {
		npu_err("Failed to get fw_cold_boot parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&npu_device->sessionmgr.mlock);
	npu_system->fw_cold_boot = fw_cold_boot;
	mutex_unlock(&npu_device->sessionmgr.mlock);

	return 0;
p_err:
	return ret;
}

static const struct file_operations npu_debug_cold_boot_fops = {
	.open		= npu_hw_debug_cold_boot_open,
	.read		= seq_read,
	.write		= npu_hw_debug_cold_boot_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

int npu_debug_register_arg(
	const char *name, void *private_arg,
	const struct file_operations *ops)
{
	int			ret	    = 0;
	struct npu_device	*npu_device;
	struct device		*dev;
	mode_t			mode = 0;
	struct dentry		*dbgfs_entry;

	/* Check whether the debugfs is properly initialized */
	if (!check_state_bit(FS_READY)) {
		probe_warn("DebugFS not initialized or disabled. Skip creation [%s]\n", name);
		ret = 0;
		goto err_exit;
	}

	/* Default parameter is npu_debug object */
	if (private_arg == NULL) {
		private_arg = (void *)npu_debug_ref;
	}

	/* Retrieve struct device to use devm_XX api */
	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	BUG_ON(!npu_device);
	dev = npu_device->dev;
	BUG_ON(!dev);

	if (name == NULL) {
		npu_err("name is null\n");
		ret = -EFAULT;
		goto err_exit;
	}
	if (ops == NULL) {
		npu_err("ops is null\n");
		ret = -EFAULT;
		goto err_exit;
	}

	/* Setting file permission based on file_operation member */
	if (ops->read || ops->compat_ioctl || ops->unlocked_ioctl)
		mode |= 0400;	/* Read permission to owner */

	if (ops->write)
		mode |= 0200;	/* Write permission to owner */

	/* Register */
	dbgfs_entry = debugfs_create_file(name, mode,
			npu_debug_ref->dfile_root, private_arg, ops);
	if (IS_ERR(dbgfs_entry)) {
		npu_err("fail in DebugFS registration (%s)\n", name);
		ret = PTR_ERR(dbgfs_entry);
		goto err_exit;
	}
	probe_info("success in DebugFS registration (%s) : Mode %04o\n"
		 , name, mode);
	return 0;

err_exit:
	return ret;
}

int npu_debug_register(const char *name, const struct file_operations *ops)
{
      int ret = 0, debugfs_enable = 0;
#ifdef CONFIG_DEBUG_FS
      debugfs_enable = 1;
      ret = npu_debug_register_arg(name, NULL, ops);
      if (ret)
		  probe_err("failed (%d)", ret);
#endif
      probe_info("register %s debugfs_state %s",
			  name, debugfs_enable ? "enabled" : "disabled");
      return ret;
}

int npu_debug_probe(struct npu_device *npu_device)
{
	int ret = 0;

	BUG_ON(!npu_device);

	/* Save reference */
	npu_debug_ref = &npu_device->debug;
	memset(npu_debug_ref, 0, sizeof(*npu_debug_ref));

	probe_info("Loading npu_debug : starting\n");
	npu_debug_ref->dfile_root = debugfs_create_dir(DEBUG_FS_ROOT_NAME, NULL);

	if (!npu_debug_ref->dfile_root) {
		probe_err("Loading npu_debug : debugfs root [%s] can not be created\n",
			  DEBUG_FS_ROOT_NAME);

		npu_debug_ref->dfile_root = NULL;
		ret = -ENOENT;
		goto err_exit;
	}
	probe_info("Loading npu_debug : debugfs root [%s] created\n"
		   , DEBUG_FS_ROOT_NAME);
	set_state_bit(FS_READY);

#ifdef CONFIG_VISION_UNITTEST
	ret = npu_debug_register(DEBUG_FS_UNITTEST_NAME
				 , &npu_debug_unittest_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for unittest can not be created\n");
		goto err_exit;
	}
#endif

	ret = npu_debug_register(DEBUG_FS_LAYER_INFO_NAME
				 , &npu_hw_debug_layer_range_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for layer range can not be created\n");
		goto err_exit;
	}

	ret = npu_debug_register(DEBUG_FS_COLD_BOOT_NAME
				 , &npu_debug_cold_boot_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for cold_boot can not be created\n");
		goto err_exit;
	}

	ret = npu_ver_probe(npu_device);
	if (ret) {
		probe_err("loading npu_debug : npu_ver_probe failed.\n");
		goto err_exit;
	}

	probe_info("loading npu_debug : completed.\n");
	return 0;

err_exit:
	return ret;
}

int npu_debug_release(void)
{
	int ret = 0;
	struct npu_device	*npu_device;
	struct device		*dev;

	probe_info("start in unloading npu_debug\n");
	if (!check_state_bit(FS_READY)) {
		/* No need to clean-up */
		probe_err("not ready in npu_debug\n");
		return -1;
	}

	/* Retrieve struct device to use devm_XX api */
	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	dev = npu_device->dev;

	/* remove version info */
	ret = npu_ver_release(npu_device);
	if (ret) {
		probe_err("npu_ver_release error : ret = %d\n", ret);
		return ret;
	}

	/* Remove debug fs entries */
	debugfs_remove_recursive(npu_debug_ref->dfile_root);
	probe_info("unloading npu_debug: root node is removed.\n");

	clear_state_bit(FS_READY);
	probe_info("complete in unloading npu_debug\n");
	return ret;
}

int npu_debug_open(struct npu_device *npu_device)
{
	npu_info("start in npu_debug open\n");
	npu_info("complete in npu_debug open\n");
	return 0;
}

int npu_debug_close(struct npu_device *npu_device)
{
	npu_info("start in npu_debug close\n");
	npu_info("complete in npu_debug close\n");
	return 0;
}

int npu_debug_start(struct npu_device *npu_device)
{
	npu_info("start in npu_debug start\n");
	npu_info("complete in npu_debug start\n");
	return 0;
}

int npu_debug_stop(struct npu_device *npu_device)
{
	npu_info("start in npu_debug stop\n");
	npu_info("complete in npu_debug stop\n");
	return 0;
}
