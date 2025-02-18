/*
 * linux/drivers/video/fbdev/exynos/mafpc/mafpc_drv.c
 *
 * Source file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/of.h>
#include <linux/of_reserved_mem.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "../panel_debug.h"
#include "../panel_drv.h"
#include "mafpc_drv.h"

#if IS_ENABLED(CONFIG_LPD_OLED_COMPENSATION)
#include <soc/samsung/exynos/exynos-lpd.h>
#endif

static int mafpc_set_written_flag(struct mafpc_device *mafpc, u32 flags)
{
	if (!mafpc)
		return -EINVAL;

	mafpc->written |= flags;

	return 0;
}

static int mafpc_clear_written_flag(struct mafpc_device *mafpc, u32 flags)
{
	if (!mafpc)
		return -EINVAL;

	mafpc->written &= ~(flags);

	return 0;
}

int mafpc_set_written_to_dev(struct mafpc_device *mafpc)
{
	return mafpc_set_written_flag(mafpc, MAFPC_UPDATED_TO_DEV);
}

int mafpc_clear_written_to_dev(struct mafpc_device *mafpc)
{
	return mafpc_clear_written_flag(mafpc, MAFPC_UPDATED_TO_DEV);
}

static int abc_fops_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct mafpc_device *mafpc = container_of(miscdev, struct mafpc_device, miscdev);

	file->private_data = mafpc;
	panel_info("was called\n");

	return 0;
}


__visible_for_testing ssize_t write_comp_image(struct mafpc_device *mafpc, const char __user *buf, size_t count)
{
	if (!mafpc) {
		panel_err("null mafpc\n");
		return -EINVAL;
	}

	if (!buf) {
		panel_err("buf is null\n");
		return -EINVAL;
	}

	if (!mafpc->comp_img_buf || !mafpc->scale_buf) {
		panel_err("invalid buffer");
		return -EINVAL;
	}

	if (count != ABC_DATA_SIZE(mafpc) &&
		count != (ABC_DATA_SIZE(mafpc) - ABC_SCALE_FACTOR_SIZE(mafpc))) {
		panel_err("invalid buffer size: %ld\n", count);
		return -EINVAL;
	}

	/* CTRL_CMD */
	if (copy_from_user(mafpc->ctrl_cmd, buf + ABC_CTRL_CMD_OFFSET(mafpc),
				ABC_CTRL_CMD_SIZE(mafpc))) {
		panel_err("failed to get ctrl_cmd\n");
		return -EFAULT;
	}

	/* COMP_IMG */
	if (copy_from_user(mafpc->comp_img_buf, buf + ABC_COMP_IMG_OFFSET(mafpc),
				ABC_COMP_IMG_SIZE(mafpc))) {
		panel_err("failed to get comp_img\n");
		return -EFAULT;
	}

	/* SCALE_FACTOR */
	if (count == ABC_DATA_SIZE(mafpc)) {
		if (copy_from_user(mafpc->scale_buf, buf + ABC_SCALE_FACTOR_OFFSET(mafpc),
					ABC_SCALE_FACTOR_SIZE(mafpc))) {
			panel_err("failed to get scale_factor\n");
			return -EFAULT;
		}

	}
	mafpc_set_written_flag(mafpc, MAFPC_UPDATED_FROM_SVC);

	return count;
}

#if IS_ENABLED(CONFIG_LPD_OLED_COMPENSATION)
/* header(1byte) + lut_type(1byte) + reserved + reserved
 * lut_type => 0,  grey ^ coeff [0]: 0, [1] : 1
 * lut_type => 1,  grey/255 ^ coeff  [0] : 0, [255] : 1
 */
static ssize_t write_lut_table(struct mafpc_device *mafpc, const char __user *buf, size_t count)
{
	int ret;
	unsigned int header;
	void *lut_buf;
	struct comp_mem_info *comp_mem;

	if ((mafpc == NULL) || (buf == NULL)) {
		panel_err("invalid param\n");
		return -EINVAL;
	}

	comp_mem = &mafpc->comp_mem;
	panel_info("lut_size: %d, count: %ld\n",
			comp_mem->lut_size, count);
	if (comp_mem->lut_size != count) {
		panel_err("invalid lut_size: %d\n", count);
		return -EINVAL;
	}

	lut_buf = (void *)phys_to_virt(comp_mem->lut_base);
	if (lut_buf == NULL) {
		panel_err("null lut buf\n");
		return -EINVAL;
	}

	ret = copy_from_user(&header, buf, LPD_COMP_LUT_HEADER_SZ);
	if (ret) {
		panel_err("failed to get user's header\n");
		return ret;
	}
	panel_info("header: %c, lut_type: %d\n", header, (header >> 8) & 0xff);

	ret = copy_from_user(lut_buf, buf, count);
	if (ret) {
		panel_err("failed to get user's header\n");
		return ret;
	}

	return count;
}
#endif

static ssize_t abc_fops_write(struct file *file, const char __user *buf,  size_t count, loff_t *ppos)
{
	int ret = 0;
	char header[MAFPC_HEADER_SIZE];
	struct mafpc_device *mafpc = file->private_data;

	ret = copy_from_user(header, buf, MAFPC_HEADER_SIZE);
	if (ret) {
		panel_err("failed to get user's header\n");
		return ret;
	}

	panel_info("count: %ld, header: %c\n",
			count, header[MAFPC_DATA_IDENTIFIER]);

	switch (header[MAFPC_DATA_IDENTIFIER]) {
	case 'M':
		ret = write_comp_image(mafpc, buf, count);
		if (ret < 0) {
			panel_err("failed to write compensation image\n");
			return 0;
		}
		break;
#if IS_ENABLED(CONFIG_LPD_OLED_COMPENSATION)
	case 'L':
		ret = write_lut_table(mafpc, buf, count);
		if (ret < 0) {
			panel_err("failed to write lut table\n");
			return 0;
		}
		break;
#endif
	default:
		panel_err("known identifier: %d\n", header[MAFPC_DATA_IDENTIFIER]);
		return 0;
	}

	return count;
}

static int mafpc_instant_on(struct mafpc_device *mafpc)
{
	int ret = 0;
	struct panel_device *panel = mafpc->panel;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_mutex_lock(&mafpc->lock);
	if (!IS_PANEL_ACTIVE(panel)) {
		panel_warn("panel is not active\n");
		goto exit_write;
	}

	if (panel_get_cur_state(panel) == PANEL_STATE_ALPM) {
		panel_warn("ABC not supported on LPM\n");
		goto exit_write;
	}

	/*
	 * mAFPC Instant_on consists of two stage.
	 * 1. Send compensation image for mAFPC to DDI, as soon as transmitted the instant_on ioctl.
	 * 2. Send instant_on command to DDI, after frame done
	 */
	panel_info("++ PANEL_MAFPC_IMG_SEQ\n");
	ret = panel_do_seqtbl_by_name(panel, PANEL_MAFPC_IMG_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to run sequence(%s)\n", PANEL_MAFPC_IMG_SEQ);
		goto exit_write;
	}

	mafpc_set_written_to_dev(mafpc);

	panel_info("++ PANEL_MAFPC_ON_SEQ\n");
	ret = panel_do_seqtbl_by_name(panel, PANEL_MAFPC_ON_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to run sequence(%s)\n", PANEL_MAFPC_ON_SEQ);
		goto exit_write;
	}

exit_write:
	panel_mutex_unlock(&mafpc->lock);

	return ret;
}

static int mafpc_instant_off(struct mafpc_device *mafpc)
{
	int ret = 0;
	struct panel_device *panel = mafpc->panel;

	panel_mutex_lock(&mafpc->lock);
	if (!IS_PANEL_ACTIVE(panel)) {
		panel_warn("panel is not active\n");
		goto err;
	}

	if (panel_get_cur_state(panel) == PANEL_STATE_ALPM) {
		panel_warn("ABC not supported on LPM\n");
		goto err;
	}

	ret = panel_do_seqtbl_by_name(panel, PANEL_MAFPC_OFF_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to run sequence(%s)\n", PANEL_MAFPC_OFF_SEQ);
		goto err;
	}

err:
	panel_mutex_unlock(&mafpc->lock);
	return ret;
}

static void mafpc_instant_handler(struct work_struct *data) {
	struct mafpc_device *mafpc = container_of(data, struct mafpc_device, instant_work);
	int ret = 0;

	switch (mafpc->instant_cmd) {
	case INSTANT_CMD_ON:
		panel_info("mafpc_instant_on +\n");
		ret = mafpc_instant_on(mafpc);
		break;
	case INSTANT_CMD_OFF:
		panel_info("mafpc_instant_off +\n");
		ret = mafpc_instant_off(mafpc);
		break;
	default:
		ret = -ENOENT;
		break;
	}
	if (ret == 0) {
		mafpc->instant_cmd = INSTANT_CMD_NONE;
	}
	panel_info("done %d\n", ret);
}

static int mafpc_clear_image_buffer(struct mafpc_device *mafpc)
{
	void *image_buf;
	struct comp_mem_info *comp_mem;

	if (mafpc == NULL) {
		panel_err("null mafpc\n");
		return -EINVAL;
	}

	comp_mem = &mafpc->comp_mem;
	if (comp_mem->reserved == false) {
		panel_err("does not supprot read ops\n");
		return 0;
	}

	if (comp_mem->image_size == 0) {
		panel_err("wrong buffer size\n");
		return 0;
	}

	image_buf = (void *)phys_to_virt(comp_mem->image_base);
	if (image_buf == NULL) {
		panel_err("failed to get virtual address\n");
		return 0;
	}

	memset(image_buf, 0x00, comp_mem->image_size);

	return 0;
}

static long abc_fops_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct mafpc_device *mafpc = file->private_data;

	if (mafpc == NULL) {
		panel_err("null mafpc\n");
		return -EINVAL;
	}
	switch (cmd) {
	case IOCTL_MAFPC_ON:
		panel_info("mAFPC on\n");
		mafpc->enable = true;
		break;
	case IOCTL_MAFPC_ON_INSTANT:
		panel_info("mAFPC on instantly On\n");
		mafpc->enable = true;
		mafpc->instant_cmd = INSTANT_CMD_ON;
		if (!mafpc->panel) {
			panel_info("panel is not ready. pending instant on\n");
			break;
		}
		queue_work(mafpc->instant_wq, &mafpc->instant_work);
		break;
	case IOCTL_MAFPC_OFF:
		panel_info("mAFPC off\n");
		mafpc->enable = false;
		break;
	case IOCTL_MAFPC_OFF_INSTANT:
		panel_info("mAFPC off instantly\n");
		mafpc->enable = false;
		mafpc->instant_cmd = INSTANT_CMD_OFF;
		if (!mafpc->panel) {
			panel_info("panel is not ready. pending instant off\n");
			break;
		}
		queue_work(mafpc->instant_wq, &mafpc->instant_work);
		break;
	case IOCTL_CLEAR_IMAGE_BUFFER:
		panel_info("mAFPC clear image buffer\n");
		ret = mafpc_clear_image_buffer(mafpc);
		if (ret)
			panel_info("failed to clear image buffer\n");
		break;
	default:
		panel_info("Invalid Command\n");
		break;
	}

	return ret;
}

static ssize_t abc_fops_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	void *image_buf;
	struct comp_mem_info *comp_mem;
	struct mafpc_device *mafpc = file->private_data;

	panel_info("count: %ld\n", count);

	if (mafpc == NULL) {
		panel_err("null mafpc\n");
		return -EINVAL;
	}

	comp_mem = &mafpc->comp_mem;
	if (comp_mem->reserved == false) {
		panel_err("does not supprot read ops\n");
		return 0;
	}

	if (count >= comp_mem->image_size) {
		panel_err("wrong buffer size\n");
		return 0;
	}

	image_buf = (void *)phys_to_virt(comp_mem->image_base);
	if (image_buf == NULL) {
		panel_err("failed to get virtual address\n");
		return 0;
	}

	print_hex_dump(KERN_ERR, "image_base", DUMP_PREFIX_ADDRESS, 32, 4, image_buf, 32, false);

	ret = copy_to_user(buf, image_buf, count);
	if (ret) {
		panel_err("failed to copy to user\n");
		return ret;
	}

	return count;
}

static int abc_fops_release(struct inode *inode, struct file *file)
{
	panel_info("was called\n");

	return 0;
}

static const struct file_operations abc_drv_fops = {
	.owner = THIS_MODULE,
	.open = abc_fops_open,
	.write = abc_fops_write,
	.read = abc_fops_read,
	.compat_ioctl = abc_fops_ioctl,
	.unlocked_ioctl = abc_fops_ioctl,
	.release = abc_fops_release,
};

int mafpc_device_probe(struct mafpc_device *mafpc, struct mafpc_info *info)
{
	if (!info) {
		panel_err("got null mafpc info\n");
		return -EINVAL;
	}

	if (!info->abc_img) {
		panel_err("ABC: Can't found abc image\n");
		return -EINVAL;
	}

	if (!info->abc_crc) {
		panel_err("ABC: Can't found abc's crc value\n");
		return -EINVAL;
	}

	if (!info->abc_scale_factor) {
		panel_err("ABC: Can't found abc's scale factor\n");
		return -EINVAL;
	}

	if (!info->abc_scale_map_tbl) {
		panel_err("ABC: Can't found abc's scale br map\n");
		return -EINVAL;
	}

	mafpc->comp_img_buf = info->abc_img;
	mafpc->comp_img_len = info->abc_img_len;
	mafpc->scale_buf = info->abc_scale_factor;
	mafpc->scale_len = info->abc_scale_factor_len;
	mafpc->comp_crc_buf = info->abc_crc;
	mafpc->comp_crc_len = info->abc_crc_len;
	mafpc->scale_map_br_tbl = info->abc_scale_map_tbl;
	mafpc->scale_map_br_tbl_len = info->abc_scale_map_tbl_len;
	mafpc->ctrl_cmd_len = info->abc_ctrl_cmd_len;

	panel_info("ABC: image size: %d\n", mafpc->comp_img_len);
	panel_info("ABC: scale factor size: %d\n", mafpc->scale_len);

	/* check pending instant cmd */
	if (mafpc->instant_cmd == INSTANT_CMD_ON || mafpc->instant_cmd == INSTANT_CMD_OFF) {
		queue_work(mafpc->instant_wq, &mafpc->instant_work);
	}

	return 0;
}

struct mafpc_device *mafpc_device_create(void)
{
	struct mafpc_device *mafpc;

	mafpc = kzalloc(sizeof(struct mafpc_device), GFP_KERNEL);
	if (!mafpc)
		return NULL;

	return mafpc;
}
EXPORT_SYMBOL(mafpc_device_create);

int mafpc_device_init(struct mafpc_device *mafpc)
{
	if (!mafpc)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(mafpc_device_init);

struct mafpc_device *get_mafpc_device(struct panel_device *panel)
{
	if (!panel)
		return NULL;

	return panel->mafpc;
}
EXPORT_SYMBOL(get_mafpc_device);

#if IS_ENABLED(CONFIG_LPD_OLED_COMPENSATION)
int parse_lpd_rmem(struct mafpc_device *mafpc, struct device *dev)
{
	struct device_node *np;
	struct device_node *node;
	struct reserved_mem *rmem;
	struct comp_mem_info *comp_mem;
	unsigned int canvas_size;
	phys_addr_t canvas_base;

	if ((mafpc == NULL) || (dev == NULL)) {
		panel_err("invalid param");
		return -EINVAL;
	}

	node = dev->of_node;
	if (node == NULL) {
		panel_err("null node\n");
		return -EINVAL;
	}

	np = of_parse_phandle(node, "memory-region", 0);
	if (np == NULL) {
		panel_err("failed to parse reserved mem for burnin\n");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(np);
	if (rmem == NULL) {
		panel_err("failed to get reserved mem for burnin\n");
		return -EINVAL;
	}

	comp_mem = &mafpc->comp_mem;

	comp_mem->image_size = LPD_COMP_IMAGE_SIZE;
	canvas_size = LPD_COMP_CANVAS_SIZE;
	comp_mem->lut_size = LPD_COMP_LUT_SIZE;

	comp_mem->image_base = rmem->base + rmem->size - LPD_MAX_COMP_MEMORY;
	canvas_base = comp_mem->image_base + comp_mem->image_size;
	comp_mem->lut_base = canvas_base + canvas_size;

	comp_mem->reserved = true;

	panel_info("LPD DRAM is reserved at addr %x total size %x\n", rmem->base, rmem->size);

	panel_info("image base: %x, size: %x, lut base: %x, size: %x, total rmem: %x\n",
		comp_mem->image_base, comp_mem->image_size, comp_mem->lut_base, comp_mem->lut_size, rmem->size);

	return 0;
}
#endif

static int mafpc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct mafpc_device *mafpc;

	mafpc = mafpc_device_create();
	if (!mafpc) {
		panel_err("failed to allocate mafpc device.\n");
		return -ENOMEM;
	}

	mafpc->dev = dev;
	mafpc->miscdev.minor = MISC_DYNAMIC_MINOR;
	mafpc->miscdev.fops = &abc_drv_fops;
	mafpc->miscdev.name = MAFPC_DEV_NAME;
	mafpc->id = 0;

	platform_set_drvdata(pdev, mafpc);

	ret = mafpc_device_init(mafpc);
	if (ret < 0) {
		panel_err("failed to initialize mafpc device\n");
		return ret;
	}

	ret = misc_register(&mafpc->miscdev);
	if (ret < 0) {
		panel_err("failed to register mafpc drv\n");
		return ret;
	}

	mafpc->comp_mem.reserved = false;

#if IS_ENABLED(CONFIG_LPD_OLED_COMPENSATION)
	ret = parse_lpd_rmem(mafpc, dev);
	if (ret) {
		panel_err("failed to parse lpd mem\n");
		return ret;
	}
#endif

	INIT_WORK(&mafpc->instant_work, mafpc_instant_handler);
	mafpc->instant_wq = create_singlethread_workqueue("mafpc_instant");
	if (mafpc->instant_wq == NULL) {
		panel_err("failed to create mafpc instant workqueue\n");
		return -ENOMEM;
	}
	mafpc->instant_cmd = INSTANT_CMD_NONE;

	panel_info("ABC: probed done\n");

	return 0;
}

static const struct of_device_id mafpc_drv_of_match_table[] = {
	{ .compatible = "samsung,panel-mafpc", },
	{},
};

MODULE_DEVICE_TABLE(of, mafpc_drv_of_match_table);

struct platform_driver mafpc_driver = {
	.probe = mafpc_probe,
	.driver = {
		.name = MAFPC_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mafpc_drv_of_match_table),
	}
};

MODULE_AUTHOR("<minwoo7945.kim@samsung.com>");
MODULE_LICENSE("GPL");
