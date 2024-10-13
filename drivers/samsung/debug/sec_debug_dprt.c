// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 */

#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/blkdev.h>
#include <linux/uio.h>
#include <linux/vmalloc.h>
#include <linux/mount.h>
#include <linux/sec_debug.h>
#include "sec_debug_internal.h"

/* "D9" for all DEBUG partition magic */
#define DPRT_HEADER_MAGIC	(0xD9A70214)
#define DPRT_HEADER_VERSION	(0xD9220214)

/* common header structure for DEBUG partition */
struct dprt_header {
		unsigned int dummy;
		unsigned int magic;
		unsigned int version;
		unsigned int start_offset;
		unsigned int nr_max_item;
		unsigned int per_item_size;
		unsigned int last_item_index;
		unsigned long reserved[8];
		unsigned long private[4];
};

#define DPRT_MAX_LEN_MEMBER_NAME		(16)
#define DPRT_MAX_MEMBERS				(32)

#define DPRT_BORE_DUMP_SIZE				0x7FE00
#define DRPT_DHST_DUMP_SIZE				(SZ_1M + DPRT_BORE_DUMP_SIZE)
#define SDGD_EACH_DUMP_SIZE				(SZ_1M * 2)

/* 16 members -> 1 BLOCK */
struct dprt_member {
		unsigned int offset;
		unsigned int size;
		unsigned int erase;
		unsigned long private;
		char name[DPRT_MAX_LEN_MEMBER_NAME];
};

enum data_member {
	SDGD_AUTO_DUMP,
	SDGD_SUMM_DUMP,
	SDGD_KERN_DUMP,
	NR_DATA_MEMBER
};

static struct block_device *sdg_bdev;
static const char *bdev_path;
static struct dprt_header *part_header;
static struct dprt_member members[DPRT_MAX_MEMBERS];
static int num_member_items;
static int sdg_data_this_offset;

static ssize_t sdg_part_read(void *buf, size_t bytes, loff_t pos)
{
	struct block_device *bdev;
	struct file file;
	struct kiocb kiocb;
	struct iov_iter iter;
	struct kvec iov = {.iov_base = buf, .iov_len = bytes};

	bdev = sdg_bdev;

	pr_debug("%s: start\n", __func__);

	memset(&file, 0, sizeof(struct file));
	file.f_mapping = bdev->bd_inode->i_mapping;
	file.f_flags = O_DSYNC | __O_SYNC | O_NOATIME;
	file.f_inode = bdev->bd_inode;
	file_ra_state_init(&file.f_ra, file.f_mapping);

	init_sync_kiocb(&kiocb, &file);
	kiocb.ki_pos = pos;
	iov_iter_kvec(&iter, READ, &iov, 1, bytes);

	pr_debug("%s: end\n", __func__);

	return generic_file_read_iter(&kiocb, &iter);
}

static int sdg_part_load_header(void)
{
	struct dprt_header *ph;
	char *header_buf = NULL;
	int size;

	size = sizeof(struct dprt_header);

	header_buf = vmalloc(size);
	if (!header_buf)
		return -ENOMEM;

	pr_debug("%s: header buf: %p\n", __func__, header_buf);

	memset(header_buf, 0, size);

	pr_debug("%s: offset %x, size %x\n", __func__, 0x0, size);
	if (!sdg_part_read(header_buf, size, 0x0)) {
		pr_err("%s: fail to read debug members\n", __func__);
		return -1;
	}

	part_header = (struct dprt_header *) header_buf;
	ph = part_header;

	pr_info("%s: magic = %x / version = %x / member_offset = %lx\n",
				__func__, ph->magic, ph->version, ph->private[0]);

	if (ph->magic != DPRT_HEADER_MAGIC || ph->version != DPRT_HEADER_VERSION) {
		pr_info("%s: header mismatch: magic = %x (expected: %x) / version = %x (expected: %x)\n",
			__func__, ph->magic, DPRT_HEADER_MAGIC, ph->version, DPRT_HEADER_VERSION);
		return -1;
	}

	return 0;
}

static int sdg_part_load_data_header(struct dprt_member *dm)
{
	struct dprt_header *dh;
	char *header_buf = NULL;
	int sdg_data_offset;
	int size;

	sdg_data_offset = dm->offset;
	pr_debug("sdg data offset: %x\n", sdg_data_offset);

	if (!sdg_data_offset) {
		pr_err("data section is not initialized\n");
		return -1;
	}

	size = sizeof(struct dprt_header);

	header_buf = vmalloc(size);
	if (!header_buf)
		return -ENOMEM;

	pr_debug("%s: header buf: %p\n", __func__, header_buf);

	memset(header_buf, 0, size);

	pr_info("%s: offset %x, size %x\n", __func__, sdg_data_offset, size);
	if (!sdg_part_read(header_buf, size, sdg_data_offset)) {
		pr_err("%s: fail to read data member head\n", __func__);
		return -1;
	}

	dh = (struct dprt_header *) header_buf;
	pr_info("%s: start_offset = %x / per_item_size = %d/ last_item_index = %d\n",
				__func__, dh->start_offset, dh->per_item_size, dh->last_item_index);

	sdg_data_this_offset = dh->start_offset + dh->per_item_size * dh->last_item_index;

	return 0;
}

static int sdg_part_load_members(void)
{
	struct dprt_header *ph;
	struct dprt_member *p;
	unsigned long member_offset;
	char *member_buf = NULL;
	int i, size;

	ph = part_header;
	num_member_items = (int) ph->nr_max_item;

	/* Set Members offset & size*/
	member_offset = ph->private[0];
	size = num_member_items * sizeof(struct dprt_member);

	member_buf = vmalloc(size);
	if (!member_buf)
		return -ENOMEM;

	pr_debug("%s: member buf: %p\n", __func__, member_buf);

	memset(member_buf, 0, size);

	pr_info("%s: member buf: offset 0x%lx / size 0x%x\n", __func__, member_offset, size);
	if (!sdg_part_read(member_buf, size, member_offset)) {
		pr_err("%s: fail to read debug members\n", __func__);
		goto fail;
	}

	p = (struct dprt_member *)member_buf;
	for (i = 0; i < num_member_items; i++) {
		members[i].offset = p[i].offset;
		members[i].size = p[i].size;
		members[i].erase = p[i].erase;
		members[i].private = p[i].private;
		strncpy(members[i].name, p[i].name, DPRT_MAX_LEN_MEMBER_NAME);
	}

	vfree(member_buf);
	return 0;

fail:
	pr_err("fail to setting members\n");
	vfree(member_buf);
	return -1;
}

static int init_sdg_bdev(void)
{
	struct block_device *bdev;
	fmode_t mode = FMODE_READ | FMODE_WRITE;
	int err = 0;

	pr_debug("%s: start\n", __func__);

	bdev = blkdev_get_by_path(bdev_path, mode, NULL);
	if (IS_ERR(bdev)) {
		dev_t devt;

		pr_err("%s: plan b\n", __func__);
		devt = name_to_dev_t(bdev_path);
		if (devt == 0) {
			pr_err("'name_to_dev_t' failed!\n");
			err = -EPROBE_DEFER;
			goto err_blkdev;
		}

		bdev = blkdev_get_by_dev(devt, mode, NULL);
		if (IS_ERR(bdev)) {
			pr_err("'blkdev_get_by_dev' failed! (%ld)\n",
					PTR_ERR(bdev));
			err = -EPROBE_DEFER;
			goto err_blkdev;
		}
	}

	sdg_bdev = bdev;

	if (sdg_part_load_header()) {
		sdg_bdev = NULL;
		return -1;
	}
	if (sdg_part_load_members()) {
		sdg_bdev = NULL;
		return -1;
	}

	return 0;

err_blkdev:
	pr_err("can't find a block device - %s\n", bdev_path);
	return err;
}

static struct dprt_member *get_sdg_part_member_by_name(const char *name)
{
	int i;

	for (i = 0; i < num_member_items; i++)
		if (!strncmp(name, members[i].name, DPRT_MAX_LEN_MEMBER_NAME))
			return &members[i];

	pr_err("fail to match member: %s\n", name);
	return NULL;
}

static int get_sdg_part_data_member_offset(int member)
{
	if (!sdg_data_this_offset)
		return 0;

	return sdg_data_this_offset + SDGD_EACH_DUMP_SIZE * member;
}

static ssize_t secdbg_part_read_member(char __user *buf, int size, int target_offset, size_t len, loff_t *offset, loff_t dm_offset)
{
	loff_t pos = dm_offset;
	ssize_t count, ret = 0;
	char *base = NULL;

	count = min(len, (size_t)(size - pos));
	pr_debug("%s: count: %ld\n", __func__, count);

	base = vmalloc(size);
	pr_debug("%s: base: %p\n", __func__, base);

	if (!sdg_part_read(base, size, target_offset)) {
		pr_err("%s: fail to read\n", __func__);
		goto fail;
	}

	if (copy_to_user(buf, base + pos, count)) {
		pr_err("%s: fail to copy to use\n", __func__);
		ret = -EFAULT;
	} else {
		*offset += count;
		ret = count;
	}

fail:
	vfree(base);
	return ret;
}

static ssize_t secdbg_part_dhst_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	ssize_t ret = 0;
	ssize_t in_dhst = 0;
	struct dprt_member *dh, *db;
	loff_t bore_offset = *offset;

	if (!sdg_bdev) {
		pr_debug("%s: re-init sdg bdev\n", __func__);
		ret = (ssize_t)init_sdg_bdev();
		if (ret)
			return ret;
	}

	dh = get_sdg_part_member_by_name("dhst_dump");
	if (!dh) {
		pr_err("%s: fail to get dhst_dump\n", __func__);
		return ret;
	}
	pr_debug("%s: dprt_member(dhst_dump): %p\n", __func__, dh);
	pr_debug("%s: %s: size 0x%x / offset 0x%x\n", __func__, dh->name, dh->size, dh->offset);

	in_dhst = (size_t)(dh->size - *offset);
	if (in_dhst > 0) {
		pr_debug("%s: dhst_dump: f_offset(%lld) p_offset(%lld)\n", __func__, *offset, *offset);
		ret = secdbg_part_read_member(buf, dh->size, dh->offset, len, offset, *offset);
	} else {
		db = get_sdg_part_member_by_name("bore_dump");
		if (!db) {
			pr_err("%s: fail to get bore_dump\n", __func__);
			return ret;
		}
		pr_debug("%s: dprt_member(bore_dump): %p\n", __func__, db);
		pr_debug("%s: %s: size 0x%x / offset 0x%x\n", __func__, db->name, db->size, db->offset);

		bore_offset -= dh->size;
		pr_debug("%s: bore_dump: f_offset(%lld) p_offset(%lld)\n", __func__, *offset, bore_offset);
		ret = secdbg_part_read_member(buf, db->size, db->offset, len, offset, bore_offset);
	}

	return ret;
}

static ssize_t secdbg_part_bore_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	ssize_t ret = 0;
	struct dprt_member *dm;

	if (!sdg_bdev) {
		pr_err("%s: re-init sdg bdev\n", __func__);
		ret = (ssize_t)init_sdg_bdev();
		if (ret)
			return ret;
	}

	dm = get_sdg_part_member_by_name("bore_dump");
	if (!dm) {
		pr_err("%s: fail to get sdg_part_member\n", __func__);
		return ret;
	}
	pr_debug("%s: dprt_member(bore_dump): %p\n", __func__, dm);
	pr_debug("%s: %s: size 0x%x / offset 0x%x\n", __func__, dm->name, dm->size, dm->offset);

	ret = secdbg_part_read_member(buf, dm->size, dm->offset, len, offset, *offset);

	return ret;
}

static ssize_t secdbg_part_summ_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	ssize_t ret = 0;
	struct dprt_member *dm;
	int sdg_data_summ_offset;

	if (!sdg_bdev) {
		pr_notice("%s: re-init sdg bdev\n", __func__);
		ret = (ssize_t)init_sdg_bdev();
		if (ret)
			return ret;
	}

	if (!sdg_data_this_offset) {
		dm = get_sdg_part_member_by_name("data");
		if (!dm) {
			pr_err("%s: fail to get sdg_part_member\n", __func__);
			return ret;
		}
		pr_debug("%s: dprt_member(data)): %p\n", __func__, dm);
		pr_debug("%s: %s: size 0x%x / offset 0x%x\n", __func__, dm->name, dm->size, dm->offset);

		pr_debug("%s: init sdg_data_this_offset\n", __func__);
		ret = sdg_part_load_data_header(dm);
		if (ret)
			return ret;
	}
	pr_debug("%s: sdg_data_this_offset: 0x%x\n", __func__, sdg_data_this_offset);

	sdg_data_summ_offset = get_sdg_part_data_member_offset(SDGD_SUMM_DUMP);
	if (!sdg_data_summ_offset)
		return 0;
	pr_debug("%s: sdg_data_summ_offset: 0x%x\n", __func__, sdg_data_summ_offset);

	ret = secdbg_part_read_member(buf, SDGD_EACH_DUMP_SIZE, sdg_data_summ_offset, len, offset, *offset);

	return ret;
}

int secdbg_part_init_bdev_path(struct device *dev)
{
	int ret = 0;

	pr_debug("%s: start\n", __func__);

	ret = of_property_read_string(dev->of_node, "bdev_path", &bdev_path);
	if (ret < 0) {
		pr_err("%s: failed\n", __func__);
		return -ENODEV;
	}

	pr_info("%s: bdev_path = %s\n", __func__, bdev_path);
	return 0;
}
EXPORT_SYMBOL(secdbg_part_init_bdev_path);

static const struct proc_ops dpart_dhst_file_ops = {
	.proc_read = secdbg_part_dhst_read,
};

static const struct proc_ops dpart_bore_file_ops = {
	.proc_read = secdbg_part_bore_read,
};

static const struct proc_ops dpart_summ_file_ops = {
	.proc_read = secdbg_part_summ_read,
};

static int __init secdbg_dprt_init(void)
{
	struct proc_dir_entry *dhst_entry;
	struct proc_dir_entry *bore_entry;
	struct proc_dir_entry *summ_entry;

#if IS_ENABLED(CONFIG_SEC_DEBUG_DISABLE_DPRT)
	pr_err("%s: disable configuration enabled\n", __func__);
	return 0;
#endif

	dhst_entry = proc_create("debug_history", S_IFREG | 0444, NULL, &dpart_dhst_file_ops);
	if (!dhst_entry) {
		pr_err("%s: failed to create proc entry debug part debug history log\n", __func__);
		return 0;
	}

	bore_entry = proc_create("boot_reset", S_IFREG | 0444, NULL, &dpart_bore_file_ops);
	if (!bore_entry) {
		pr_err("%s: failed to create proc entry debug part boot reset log\n", __func__);
		return 0;
	}

	summ_entry = proc_create("reset_summary", S_IFREG | 0444, NULL, &dpart_summ_file_ops);
	if (!summ_entry) {
		pr_err("%s: failed to create proc entry debug part dump summary\n", __func__);
		return 0;
	}

	pr_info("%s: success to create proc entry\n", __func__);

	proc_set_size(dhst_entry, (size_t)(DRPT_DHST_DUMP_SIZE));
	proc_set_size(bore_entry, (size_t)(DPRT_BORE_DUMP_SIZE));
	proc_set_size(summ_entry, (size_t)(SDGD_EACH_DUMP_SIZE));

	return 0;
}
module_init(secdbg_dprt_init);

static void __exit secdbg_dprt_exit(void)
{
}
module_exit(secdbg_dprt_exit);

MODULE_DESCRIPTION("Samsung Debug Dprt driver");
MODULE_LICENSE("GPL v2");
