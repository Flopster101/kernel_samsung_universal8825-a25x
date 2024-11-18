// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "stm_dev.h"

#ifdef RAWDATA_MMAP
#include "stm_reg.h"
#include <linux/miscdevice.h>

struct tsp_ioctl {
	int num;
	u8 data[PAGE_SIZE];
};

#define IOCTL_TSP_MAP_READ	_IOR(0, 0, struct tsp_ioctl)

static long tsp_ioctl_handler(struct file *file, unsigned int cmd, void __user *p, int compat_mode)
{
	static struct tsp_ioctl t;
	int num;

	if (copy_from_user(&t, p, sizeof(struct tsp_ioctl))) {
		input_err(true, &g_ts->client->dev, "%s: failed to copyt_from_user\n", __func__);
		return -EFAULT;
	}

	if (cmd == IOCTL_TSP_MAP_READ) {
#if 0
		input_info(true, &g_ts->client->dev, "%s: num: %d\n", __func__, t.num);
#endif
		num = t.num % 5;
		if (num == 0) {
			memcpy(&t.data, g_ts->raw_v0, g_ts->raw_len);
			if (copy_to_user(p, (void *)&t, sizeof(struct tsp_ioctl))) {
				input_err(true, &g_ts->client->dev, "%s: failed to copyt_to_user\n", __func__);
				return -EFAULT;
			}
		} else if (num == 1) {
			memcpy(&t.data, g_ts->raw_v1, g_ts->raw_len);
			if (copy_to_user(p, (void *)&t, sizeof(struct tsp_ioctl))) {
				input_err(true, &g_ts->client->dev, "%s: failed to copyt_to_user\n", __func__);
				return -EFAULT;
			}
		} else if (num == 2) {
			memcpy(&t.data, g_ts->raw_v2, g_ts->raw_len);
			if (copy_to_user(p, (void *)&t, sizeof(struct tsp_ioctl))) {
				input_err(true, &g_ts->client->dev, "%s: failed to copyt_to_user\n", __func__);
				return -EFAULT;
			}
		} else if (num == 3) {
			memcpy(&t.data, g_ts->raw_v3, g_ts->raw_len);
			if (copy_to_user(p, (void *)&t, sizeof(struct tsp_ioctl))) {
				input_err(true, &g_ts->client->dev, "%s: failed to copyt_to_user\n", __func__);
				return -EFAULT;
			}
		} else if (num == 4) {
			memcpy(&t.data, g_ts->raw_v4, g_ts->raw_len);
			if (copy_to_user(p, (void *)&t, sizeof(struct tsp_ioctl))) {
				input_err(true, &g_ts->client->dev, "%s: failed to copyt_to_user\n", __func__);
				return -EFAULT;
			}
		}
	}

	return 0;
}

static long tsp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return tsp_ioctl_handler(file, cmd, (void __user *)arg, 0);
}

static int tsp_open(struct inode *inode, struct file *file)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int tsp_close(struct inode *inode, struct file *file)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static const struct file_operations tsp_io_fos = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tsp_ioctl,
	.open = tsp_open,
	.release = tsp_close,
};

static const struct miscdevice tsp_misc = {
	.fops = &tsp_io_fos,
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tspio",
};

MODULE_ALIAS_MISCDEV(MISC_DYNAMIC_MINOR);

void tsp_ioctl_init(void)
{
	int ret;

	ret = misc_register(&tsp_misc);
	input_info(true, &g_ts->client->dev, "%s: ret: %d\n", __func__, ret);
}

/* tsp rawdata : using mmap */
#define DEV_NAME "tsp_data"
#define DEV_NAME0 "tsp_data0"
#define DEV_NAME1 "tsp_data1"
#define DEV_NAME2 "tsp_data2"
#define DEV_NAME3 "tsp_data3"
#define DEV_NAME4 "tsp_data4"
#define DATA_SIZE (2 * PAGE_SIZE)

static const unsigned int MINOR_BASE;
static const unsigned int MINOR_NUM0 = 1;
static const unsigned int MINOR_NUM1 = 2;
static const unsigned int MINOR_NUM2 = 3;
static const unsigned int MINOR_NUM3 = 4;
static const unsigned int MINOR_NUM4 = 5;

static struct cdev *mmapdev_cdev0;
static struct cdev *mmapdev_cdev1;
static struct cdev *mmapdev_cdev2;
static struct cdev *mmapdev_cdev3;
static struct cdev *mmapdev_cdev4;

static struct class *mmapdev_class;

static dev_t dev0;
static dev_t dev1;
static dev_t dev2;
static dev_t dev3;
static dev_t dev4;

/* mmap0 */
static vm_fault_t mmap0_vm_fault(struct vm_fault *vmf)
{
	struct page *page = NULL;
	unsigned long offset = 0;
	void *page_ptr = NULL;

	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	if (vmf == NULL)
		return VM_FAULT_SIGBUS;

	offset = vmf->address - vmf->vma->vm_start;
	if (offset >= DATA_SIZE)
		return VM_FAULT_SIGBUS;

	page_ptr = g_ts->raw_v0 + offset;
	page = vmalloc_to_page(page_ptr);
	get_page(page);
	vmf->page = page;
	return 0;
}

static const struct vm_operations_struct vma0_ops = {
	.fault = mmap0_vm_fault
};

static int mmap0_open(struct inode *inode, struct file *filp)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int mmap0_release(struct inode *inode, struct file *filp)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int mmap0_remap(struct file *filp, struct vm_area_struct *vma)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);

	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &vma0_ops;
	return 0;
}

static const struct file_operations mmap0_fops = {
	.open = mmap0_open,
	.release = mmap0_release,
	.mmap = mmap0_remap,
};

/* map1 */
static vm_fault_t mmap1_vm_fault(struct vm_fault *vmf)
{
	struct page *page = NULL;
	unsigned long offset = 0;
	void *page_ptr = NULL;

	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	if (vmf == NULL)
		return VM_FAULT_SIGBUS;

	offset = vmf->address - vmf->vma->vm_start;
	if (offset >= DATA_SIZE)
		return VM_FAULT_SIGBUS;

	page_ptr = g_ts->raw_v1 + offset;
	page = vmalloc_to_page(page_ptr);
	get_page(page);
	vmf->page = page;
	return 0;
}

static const struct vm_operations_struct vma1_ops = {
	.fault = mmap1_vm_fault
};

static int mmap1_open(struct inode *inode, struct file *filp)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int mmap1_release(struct inode *inode, struct file *filp)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int mmap1_remap(struct file *filp, struct vm_area_struct *vma)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);

	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &vma1_ops;
	return 0;
}

static const struct file_operations mmap1_fops = {
	.open = mmap1_open,
	.release = mmap1_release,
	.mmap = mmap1_remap,
};

/* map2 */
static vm_fault_t mmap2_vm_fault(struct vm_fault *vmf)
{
	struct page *page = NULL;
	unsigned long offset = 0;
	void *page_ptr = NULL;

	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	if (vmf == NULL)
		return VM_FAULT_SIGBUS;

	offset = vmf->address - vmf->vma->vm_start;
	if (offset >= DATA_SIZE)
		return VM_FAULT_SIGBUS;

	page_ptr = g_ts->raw_v2 + offset;
	page = vmalloc_to_page(page_ptr);
	get_page(page);
	vmf->page = page;
	return 0;
}

static const struct vm_operations_struct vma2_ops = {
	.fault = mmap2_vm_fault
};

static int mmap2_open(struct inode *inode, struct file *filp)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int mmap2_release(struct inode *inode, struct file *filp)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int mmap2_remap(struct file *filp, struct vm_area_struct *vma)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);

	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &vma2_ops;
	return 0;
}

static const struct file_operations mmap2_fops = {
	.open = mmap2_open,
	.release = mmap2_release,
	.mmap = mmap2_remap,
};

/* map3 */
static vm_fault_t mmap3_vm_fault(struct vm_fault *vmf)
{
	struct page *page = NULL;
	unsigned long offset = 0;
	void *page_ptr = NULL;

	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	if (vmf == NULL)
		return VM_FAULT_SIGBUS;

	offset = vmf->address - vmf->vma->vm_start;
	if (offset >= DATA_SIZE)
		return VM_FAULT_SIGBUS;

	page_ptr = g_ts->raw_v3 + offset;
	page = vmalloc_to_page(page_ptr);
	get_page(page);
	vmf->page = page;
	return 0;
}

static const struct vm_operations_struct vma3_ops = {
	.fault = mmap3_vm_fault
};

static int mmap3_open(struct inode *inode, struct file *filp)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int mmap3_release(struct inode *inode, struct file *filp)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int mmap3_remap(struct file *filp, struct vm_area_struct *vma)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);

	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &vma3_ops;
	return 0;
}

static const struct file_operations mmap3_fops = {
	.open = mmap3_open,
	.release = mmap3_release,
	.mmap = mmap3_remap,
};

/* map4 */
static vm_fault_t mmap4_vm_fault(struct vm_fault *vmf)
{
	struct page *page = NULL;
	unsigned long offset = 0;
	void *page_ptr = NULL;

	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	if (vmf == NULL)
		return VM_FAULT_SIGBUS;

	offset = vmf->address - vmf->vma->vm_start;
	if (offset >= DATA_SIZE)
		return VM_FAULT_SIGBUS;

	page_ptr = g_ts->raw_v4 + offset;
	page = vmalloc_to_page(page_ptr);
	get_page(page);
	vmf->page = page;
	return 0;
}

static const struct vm_operations_struct vma4_ops = {
	.fault = mmap4_vm_fault
};

static int mmap4_open(struct inode *inode, struct file *filp)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int mmap4_release(struct inode *inode, struct file *filp)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int mmap4_remap(struct file *filp, struct vm_area_struct *vma)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);

	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &vma4_ops;
	return 0;
}

static const struct file_operations mmap4_fops = {
	.open = mmap4_open,
	.release = mmap4_release,
	.mmap = mmap4_remap,
};

static ssize_t raw_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	ssize_t ret;

	mutex_lock(&ts->raw_lock);
	ret = snprintf(buf, PAGE_SIZE, "%d,%d", ts->before_irq_count, ts->raw_irq_count);
	mutex_unlock(&ts->raw_lock);

	return ret;
}

static DEVICE_ATTR_RO(raw_irq);

static struct attribute *rawdata_attrs[] = {
	&dev_attr_raw_irq.attr,
	NULL,
};

static struct attribute_group rawdata_attr_group = {
	.attrs = rawdata_attrs,
};

int stm_ts_rawdata_buffer_alloc(struct stm_ts_data *ts)
{
	ts->raw_v0 = vmalloc(ts->raw_len);
	if (!ts->raw_v0)
		goto alloc_out;
	ts->raw_v1 = vmalloc(ts->raw_len);
	if (!ts->raw_v1)
		goto alloc_out;
	ts->raw_v2 = vmalloc(ts->raw_len);
	if (!ts->raw_v2)
		goto alloc_out;
	ts->raw_v3 = vmalloc(ts->raw_len);
	if (!ts->raw_v3)
		goto alloc_out;
	ts->raw_v4 = vmalloc(ts->raw_len);
	if (!ts->raw_v4)
		goto alloc_out;

	memset(ts->raw_v0, 0, ts->raw_len);
	memset(ts->raw_v1, 0, ts->raw_len);
	memset(ts->raw_v2, 0, ts->raw_len);
	memset(ts->raw_v3, 0, ts->raw_len);
	memset(ts->raw_v4, 0, ts->raw_len);

	ts->raw_u8 = kzalloc(ts->raw_len, GFP_KERNEL);
	if (!ts->raw_u8)
		goto alloc_out;
	ts->raw = kzalloc(ts->raw_len, GFP_KERNEL);
	if (!ts->raw)
		goto alloc_out;

	return 0;

alloc_out:
	if (!ts->raw_v0)
		vfree(ts->raw_v0);
	if (!ts->raw_v1)
		vfree(ts->raw_v1);
	if (!ts->raw_v2)
		vfree(ts->raw_v2);
	if (!ts->raw_v3)
		vfree(ts->raw_v3);
	if (!ts->raw_v4)
		vfree(ts->raw_v4);
	if (!ts->raw_u8)
		kfree(ts->raw_u8);
	if (!ts->raw)
		kfree(ts->raw);
	return 0;
}

int stm_ts_rawdata_init(struct stm_ts_data *ts)
{
	int alloc_ret = 0, cdev_err = 0;
	unsigned int mmapdev_major0;
	unsigned int mmapdev_major1;
	unsigned int mmapdev_major2;
	unsigned int mmapdev_major3;
	unsigned int mmapdev_major4;

	input_info(true, &ts->client->dev, "%s: num: %d\n", __func__, ts->plat_data->support_rawdata_map_num);

	ts->raw_len = PAGE_SIZE;

	/* map0 */
	mmapdev_cdev0 = cdev_alloc();
	alloc_ret = alloc_chrdev_region(&dev0, MINOR_BASE, MINOR_NUM0, DEV_NAME0);
	if (alloc_ret != 0) {
		input_info(true, &ts->client->dev, "%s: alloc_chrdev_region = %d\n", __func__, alloc_ret);
		return -1;
	}

	mmapdev_major0 = MAJOR(dev0);
	dev0 = MKDEV(mmapdev_major0, MINOR_BASE);

	cdev_init(mmapdev_cdev0, &mmap0_fops);
	mmapdev_cdev0->owner = THIS_MODULE;

	cdev_err = cdev_add(mmapdev_cdev0, dev0, MINOR_NUM0);
	if (cdev_err != 0) {
		input_info(true, &ts->client->dev, "%s: cdev_add = %d\n", __func__, cdev_err);
		goto OUT2;
	}

	/* map1 */
	mmapdev_cdev1 = cdev_alloc();
	alloc_ret = alloc_chrdev_region(&dev1, MINOR_BASE, MINOR_NUM1, DEV_NAME1);
	if (alloc_ret != 0) {
		input_info(true, &ts->client->dev, "%s: alloc_chrdev_region = %d\n", __func__, alloc_ret);
		return -1;
	}

	mmapdev_major1 = MAJOR(dev1);
	dev1 = MKDEV(mmapdev_major1, MINOR_BASE);

	cdev_init(mmapdev_cdev1, &mmap1_fops);
	mmapdev_cdev1->owner = THIS_MODULE;

	cdev_err = cdev_add(mmapdev_cdev1, dev1, MINOR_NUM1);
	if (cdev_err != 0) {
		input_info(true, &ts->client->dev, "%s: cdev_add = %d\n", __func__, cdev_err);
		goto OUT2;
	}

	/* map2 */
	mmapdev_cdev2 = cdev_alloc();
	alloc_ret = alloc_chrdev_region(&dev2, MINOR_BASE, MINOR_NUM2, DEV_NAME2);
	if (alloc_ret != 0) {
		input_info(true, &ts->client->dev, "%s: alloc_chrdev_region = %d\n", __func__, alloc_ret);
		return -1;
	}

	mmapdev_major2 = MAJOR(dev2);
	dev2 = MKDEV(mmapdev_major2, MINOR_BASE);

	cdev_init(mmapdev_cdev2, &mmap2_fops);
	mmapdev_cdev2->owner = THIS_MODULE;

	cdev_err = cdev_add(mmapdev_cdev2, dev2, MINOR_NUM2);
	if (cdev_err != 0) {
		input_info(true, &ts->client->dev, "%s: cdev_add = %d\n", __func__, cdev_err);
		goto OUT2;
	}

	/* map3 */
	mmapdev_cdev3 = cdev_alloc();
	alloc_ret = alloc_chrdev_region(&dev3, MINOR_BASE, MINOR_NUM3, DEV_NAME3);
	if (alloc_ret != 0) {
		input_info(true, &ts->client->dev, "%s: alloc_chrdev_region = %d\n", __func__, alloc_ret);
		return -1;
	}

	mmapdev_major3 = MAJOR(dev3);
	dev3 = MKDEV(mmapdev_major3, MINOR_BASE);

	cdev_init(mmapdev_cdev3, &mmap3_fops);
	mmapdev_cdev3->owner = THIS_MODULE;

	cdev_err = cdev_add(mmapdev_cdev3, dev3, MINOR_NUM3);
	if (cdev_err != 0) {
		input_info(true, &ts->client->dev, "%s: cdev_add = %d\n", __func__, cdev_err);
		goto OUT2;
	}

	/* map4 */
	mmapdev_cdev4 = cdev_alloc();
	alloc_ret = alloc_chrdev_region(&dev4, MINOR_BASE, MINOR_NUM4, DEV_NAME4);
	if (alloc_ret != 0) {
		input_info(true, &ts->client->dev, "%s: alloc_chrdev_region = %d\n", __func__, alloc_ret);
		return -1;
	}

	mmapdev_major4 = MAJOR(dev4);
	dev4 = MKDEV(mmapdev_major4, MINOR_BASE);

	cdev_init(mmapdev_cdev4, &mmap4_fops);
	mmapdev_cdev4->owner = THIS_MODULE;

	cdev_err = cdev_add(mmapdev_cdev4, dev4, MINOR_NUM4);
	if (cdev_err != 0) {
		input_info(true, &ts->client->dev, "%s: cdev_add = %d\n", __func__, cdev_err);
		goto OUT2;
	}

	/* class create */
	mmapdev_class = class_create(THIS_MODULE, "mmap_device");
	if (IS_ERR(mmapdev_class)) {
		input_info(true, &ts->client->dev, "%s: class_create\n", __func__);
		goto OUT;
	}

	sysfs_create_group(&ts->sec.fac_dev->kobj, &rawdata_attr_group);

	device_create(mmapdev_class, NULL, MKDEV(mmapdev_major0, MINOR_BASE), NULL, DEV_NAME0);
	device_create(mmapdev_class, NULL, MKDEV(mmapdev_major1, MINOR_BASE), NULL, DEV_NAME1);
	device_create(mmapdev_class, NULL, MKDEV(mmapdev_major2, MINOR_BASE), NULL, DEV_NAME2);
	device_create(mmapdev_class, NULL, MKDEV(mmapdev_major3, MINOR_BASE), NULL, DEV_NAME3);
	device_create(mmapdev_class, NULL, MKDEV(mmapdev_major4, MINOR_BASE), NULL, DEV_NAME4);


	input_info(true, &ts->client->dev, "%s: done", __func__);

	tsp_ioctl_init();
	return 0;

OUT:
	cdev_del(mmapdev_cdev4);
	cdev_del(mmapdev_cdev3);
	cdev_del(mmapdev_cdev2);
	cdev_del(mmapdev_cdev1);
	cdev_del(mmapdev_cdev0);
OUT2:
	unregister_chrdev_region(dev3, MINOR_NUM4);
	unregister_chrdev_region(dev2, MINOR_NUM3);
	unregister_chrdev_region(dev2, MINOR_NUM2);
	unregister_chrdev_region(dev1, MINOR_NUM1);
	unregister_chrdev_region(dev0, MINOR_NUM0);
	return -1;
}

void stm_ts_rawdata_buffer_remove(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);
	kfree(ts->raw);
	kfree(ts->raw_u8);

	ts->raw = NULL;
	ts->raw_u8 = NULL;

	cdev_del(mmapdev_cdev4);
	cdev_del(mmapdev_cdev3);
	cdev_del(mmapdev_cdev2);
	cdev_del(mmapdev_cdev1);
	cdev_del(mmapdev_cdev0);

	unregister_chrdev_region(dev3, MINOR_NUM4);
	unregister_chrdev_region(dev2, MINOR_NUM3);
	unregister_chrdev_region(dev2, MINOR_NUM2);
	unregister_chrdev_region(dev1, MINOR_NUM1);
	unregister_chrdev_region(dev0, MINOR_NUM0);
}
#endif //#ifdef RAWDATA_MMAP
