/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/syscalls.h>

#define MAX_TYPE_CNT		10

enum {
	PARAM_OFF = '0',
	PARAM_ON = '1',
};

#define PARAM_RD		0
#define PARAM_WR		1

/* NOTE: This is for GED bring up; define SEC_PARAM_NAME	"/dev/block/param" */
#define SEC_PARAM_NAME	"/dev/block/by-name/param"
#define STR_LENGTH	2048

struct sec_param_data_s {
	struct work_struct sec_param_work;
	unsigned long offset;
	char val;
};

struct sec_param_data_s_u32 {
	struct work_struct sec_param_work;
	struct completion work;
	unsigned long offset;
	u32 val;
	int direction;
};

struct sec_param_data_s_str {
	struct work_struct sec_param_work;
	struct completion work;
	unsigned long offset;
	char str[STR_LENGTH];
	int direction;
};

struct sec_param_data_s_extra {
	struct work_struct sec_param_work;
	struct completion work;
	unsigned long offset;
	void *extra;
	size_t size;
	int direction;
};

static struct sec_param_data_s sec_param_data;
static struct sec_param_data_s_u32 sec_param_data_u32;
static struct sec_param_data_s_str sec_param_data_str;
static struct sec_param_data_s_extra sec_param_data_extra;

static DEFINE_MUTEX(sec_param_mutex);
static DEFINE_MUTEX(sec_param_mutex_u32);
static DEFINE_MUTEX(sec_param_mutex_str);
static DEFINE_MUTEX(sec_param_mutex_extra);

static u32 *char_offsets;
static u32 type_char_cnt;
static u32 *u32_offsets;
static u32 type_u32_cnt;

static void sec_param_update(struct work_struct *work)
{
	int ret = -1;
	struct file *fp = NULL;
	struct sec_param_data_s *param_data =
		container_of(work, struct sec_param_data_s, sec_param_work);

//	fp = filp_open(SEC_PARAM_NAME, O_WRONLY | O_SYNC, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));
		return;
	}
	pr_info("%s: set param %c at %lu\n", __func__,
		param_data->val, param_data->offset);
	ret = fp->f_op->llseek(fp, param_data->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}

/*
	ret = kernel_write(fp, &param_data->val, 1, &fp->f_pos);
*/
	ret = -1; // force fail, cannot support kernel_write or kernel_read
	if (ret < 0)
		pr_err("%s: write error! %d\n", __func__, ret);

close_fp_out:
//	filp_close(fp, NULL);
	pr_info("%s: exit %d\n", __func__, ret);
}

static void sec_param_update_u32(struct work_struct *work)
{
	int ret = -1;
	struct file *fp = NULL;
	struct sec_param_data_s_u32 *param_data_u32 =
		container_of(work, struct sec_param_data_s_u32, sec_param_work);
#if 0
	int flag = (param_data_u32->direction == PARAM_WR)
			? (O_RDWR | O_SYNC) : O_RDONLY;

	fp = filp_open(SEC_PARAM_NAME, flag, 0);
#endif


	if (IS_ERR(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));
		complete(&param_data_u32->work);
		return;
	}

	ret = vfs_llseek(fp, param_data_u32->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}
/*
	if (param_data_u32->direction == PARAM_WR)
		ret = kernel_write(fp, (const char __user *)&param_data_u32->val, sizeof(param_data_u32->val), &fp->f_pos);
	else if (param_data_u32->direction == PARAM_RD)
		ret = kernel_read(fp, (char __user *)&param_data_u32->val, sizeof(param_data_u32->val), &fp->f_pos);
*/
	ret = -1; // force fail, cannot support kernel_write or kernel_read

	if (ret < 0) {
		pr_err("%s: %s error! %d\n", __func__, param_data_u32->direction ? "write" : "read", ret);
		goto close_fp_out;
	}

close_fp_out:
//	filp_close(fp, NULL);
	complete(&param_data_u32->work);
	pr_info("%s: exit %d\n", __func__, ret);
}

static void sec_param_update_str(struct work_struct *work)
{
	int ret = -1;
	struct file *fp = NULL;
	struct sec_param_data_s_str *param_data_str =
		container_of(work, struct sec_param_data_s_str, sec_param_work);
#if 0
	int flag = (param_data_str->direction == PARAM_WR)
			? (O_RDWR | O_SYNC) : O_RDONLY;

	fp = filp_open(SEC_PARAM_NAME, flag, 0);
#endif

	if (IS_ERR(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));
		complete(&param_data_str->work);
		return;
	}

	ret = vfs_llseek(fp, param_data_str->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}
/*
	if (param_data_str->direction == PARAM_WR)
		ret = kernel_write(fp, (const char __user *)param_data_str->str, sizeof(param_data_str->str), &fp->f_pos);
	else if (param_data_str->direction == PARAM_RD)
		ret = kernel_read(fp, (char __user *)param_data_str->str, sizeof(param_data_str->str), &fp->f_pos);
*/
	ret = -1; // force fail, cannot support kernel_write or kernel_read

	if (ret < 0) {
		pr_err("%s: %s error! %d\n", __func__, param_data_str->direction ? "write" : "read", ret);
		goto close_fp_out;
	}

close_fp_out:
//	filp_close(fp, NULL);
	complete(&param_data_str->work);
	pr_info("%s: exit %d\n", __func__, ret);
}

static void sec_param_update_extra(struct work_struct *work)
{
	int ret = -1;
	struct file *fp = NULL;
	struct sec_param_data_s_extra *param_data_extra =
		container_of(work, struct sec_param_data_s_extra, sec_param_work);
#if 0
	int flag = (param_data_extra->direction == PARAM_WR)
			? (O_RDWR | O_SYNC) : O_RDONLY;

	fp = filp_open(SEC_PARAM_NAME, flag, 0);
#endif

	if (IS_ERR(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));
		complete(&param_data_extra->work);
		return;
	}

	ret = vfs_llseek(fp, param_data_extra->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}
/*
	if (param_data_extra->direction == PARAM_WR)
		ret = kernel_write(fp, (const char __user *)param_data_extra->extra, param_data_extra->size, &fp->f_pos);
	else if (param_data_extra->direction == PARAM_RD)
		ret = kernel_read(fp, (char __user *)param_data_extra->extra,  param_data_extra->size, &fp->f_pos);

*/
	ret = -1; // force fail, cannot support kernel_write or kernel_read

	if (ret < 0) {
		pr_err("%s: %s error! %d\n", __func__, param_data_extra->direction ? "write" : "read", ret);
		goto close_fp_out;
	}

close_fp_out:
//	filp_close(fp, NULL);
	complete(&param_data_extra->work);
	pr_info("%s: exit %d\n", __func__, ret);
}

int sec_set_param(unsigned long offset, char val)
{
	int i, ret = -1;

	if (!type_char_cnt)
		return ret;

	mutex_lock(&sec_param_mutex);

	i = 0;
	while (i < type_char_cnt) {
		if (offset >= char_offsets[i*2] &&
				offset < char_offsets[i*2] + char_offsets[i*2+1])
			break;

		if (type_char_cnt == ++i)
			goto unlock_out;
	}

	switch (val) {
	case PARAM_OFF:
	case PARAM_ON:
		goto set_param;
	default:
		goto unlock_out;
	}

set_param:
	sec_param_data.offset = offset;
	sec_param_data.val = val;

	schedule_work(&sec_param_data.sec_param_work);

	/* how to determine to return success or fail ? */
	ret = 0;
unlock_out:
	mutex_unlock(&sec_param_mutex);
	return ret;
}

int sec_set_param_u32(unsigned long offset, u32 val)
{
	int i, ret = -1;

	if (!type_u32_cnt)
		return ret;

	mutex_lock(&sec_param_mutex_u32);

	i = 0;
	while (i < type_u32_cnt) {
		if (offset == u32_offsets[i])
			break;

		if (type_u32_cnt == ++i)
			goto unlock_out;
	}

	sec_param_data_u32.val = val;
	sec_param_data_u32.offset = offset;
	sec_param_data_u32.direction = PARAM_WR;

	schedule_work(&sec_param_data_u32.sec_param_work);
	wait_for_completion_timeout(&sec_param_data_u32.work, msecs_to_jiffies(HZ));

	ret = 0;
unlock_out:
	mutex_unlock(&sec_param_mutex_u32);
	return ret;
}

int sec_get_param_u32(unsigned long offset, u32 *val)
{
	int i, ret = -1;

	if (!type_u32_cnt)
		return ret;

	mutex_lock(&sec_param_mutex_u32);

	i = 0;
	while (i < type_u32_cnt) {
		if (offset == u32_offsets[i])
			break;

		if (type_u32_cnt == ++i)
			goto unlock_out;
	}

	sec_param_data_u32.val = 0;
	sec_param_data_u32.offset = offset;
	sec_param_data_u32.direction = PARAM_RD;

	schedule_work(&sec_param_data_u32.sec_param_work);
	wait_for_completion_timeout(&sec_param_data_u32.work, msecs_to_jiffies(HZ));

	*val = sec_param_data_u32.val;

	ret = 0;
unlock_out:
	mutex_unlock(&sec_param_mutex_u32);
	return ret;
}

int sec_set_param_str(unsigned long offset, const char *str, int size)
{
	int ret = 0;

	mutex_lock(&sec_param_mutex_str);

	if (!strcmp(str, "")) {
		mutex_unlock(&sec_param_mutex_str);
		return -1;
	}

	memset(sec_param_data_str.str, 0, sizeof(sec_param_data_str.str));
	sec_param_data_str.offset = offset;
	sec_param_data_str.direction = PARAM_WR;
	strncpy(sec_param_data_str.str, str, size);

	schedule_work(&sec_param_data_str.sec_param_work);
	wait_for_completion_timeout(&sec_param_data_str.work, msecs_to_jiffies(HZ));

	mutex_unlock(&sec_param_mutex_str);
	return ret;
}

int sec_get_param_str(unsigned long offset, char *str)
{
	mutex_lock(&sec_param_mutex_str);

	memset(sec_param_data_str.str, 0, sizeof(sec_param_data_str.str));
	sec_param_data_str.offset = offset;
	sec_param_data_str.direction = PARAM_RD;

	schedule_work(&sec_param_data_str.sec_param_work);
	wait_for_completion_timeout(&sec_param_data_str.work, msecs_to_jiffies(HZ));

	snprintf(str, sizeof(sec_param_data_str.str), "%s", sec_param_data_str.str);

	mutex_unlock(&sec_param_mutex_str);
	return 0;
}

int sec_set_param_extra(unsigned long offset, void *extra, size_t size)
{
	int ret = 0;

	mutex_lock(&sec_param_mutex_extra);

	if (!extra || !size) {
		mutex_unlock(&sec_param_mutex_extra);
		return -1;
	}

	sec_param_data_extra.offset = offset;
	sec_param_data_extra.direction = PARAM_WR;
	sec_param_data_extra.extra = (void *)extra;
	sec_param_data_extra.size = size;

	schedule_work(&sec_param_data_extra.sec_param_work);
	wait_for_completion_timeout(&sec_param_data_extra.work, msecs_to_jiffies(HZ));

	mutex_unlock(&sec_param_mutex_extra);
	return ret;
}

static int sec_param_dt_parse(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	u32 val;
	//int i;

	if (!(of_property_read_u32(np, "char-table-size", &val))) {
		if (MAX_TYPE_CNT < val) {
			pr_err("%s: invalid char-table-size %d\n", __func__, val);
			return -EINVAL;
		}

		char_offsets = devm_kzalloc(&pdev->dev,
				sizeof(u32) * val * 2, GFP_KERNEL);

		if (!char_offsets) {
			pr_err("%s: failed to alloc char-offset-table\n", __func__);
			return -ENOMEM;
		}
	
		if (of_property_read_u32_array(np, "char-offset-table",
				char_offsets, val * 2)) {
			pr_err("%s: invalid char-offset-table\n", __func__);
			devm_kfree(&pdev->dev, char_offsets);
			return -EINVAL;
		}
		type_char_cnt = val;
	}

	if (!(of_property_read_u32(np, "u32-table-size", &val))) {
		if (MAX_TYPE_CNT < val) {
			pr_err("%s: invalid u32-table-size %d\n", __func__, val);
			return -EINVAL;
		}

		u32_offsets = devm_kzalloc(&pdev->dev,
				sizeof(u32) * val, GFP_KERNEL);

		if (!u32_offsets) {
			pr_err("%s: failed to alloc u32-offset-table\n", __func__);
			return -ENOMEM;
		}
	
		if (of_property_read_u32_array(np, "u32-offset-table",
				u32_offsets, val)) {
			pr_err("%s: invalid u32-offset-table\n", __func__);
			devm_kfree(&pdev->dev, u32_offsets);
			return -EINVAL;
		}
		type_u32_cnt = val;
	}

#if 0
	for (i = 0; i < type_char_cnt; i++)
		pr_info("%s: char-table [%d] %d,%d\n", __func__, i, char_offsets[i*2], char_offsets[i*2+1]);

	for (i = 0; i < type_u32_cnt; i++)
		pr_info("%s: u32-table [%d] %d\n", __func__, i, u32_offsets[i]);
#endif

	return 0;
}

static int sec_param_probe(struct platform_device *pdev)
{
	int ret;

	dev_info(&pdev->dev, "%s: start\n", __func__);

	ret = sec_param_dt_parse(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s failed.\n", __func__);
		return ret;
	}

	sec_param_data.offset = 0;
	sec_param_data.val = '0';

	sec_param_data_str.offset = 0;
	memset(sec_param_data_str.str, 0, sizeof(sec_param_data_str.str));
	sec_param_data_str.direction = 0;

	sec_param_data_u32.offset = 0;
	sec_param_data_u32.val = 0;
	sec_param_data_u32.direction = 0;

	INIT_WORK(&sec_param_data.sec_param_work, sec_param_update);
	init_completion(&sec_param_data_u32.work);
	INIT_WORK(&sec_param_data_u32.sec_param_work, sec_param_update_u32);
	init_completion(&sec_param_data_str.work);
	INIT_WORK(&sec_param_data_str.sec_param_work, sec_param_update_str);
	init_completion(&sec_param_data_extra.work);
	INIT_WORK(&sec_param_data_extra.sec_param_work, sec_param_update_extra);

	return 0;
}

static void sec_param_shutdown(struct platform_device *pdev)
{
	cancel_work_sync(&sec_param_data.sec_param_work);
	cancel_work_sync(&sec_param_data_u32.sec_param_work);
	cancel_work_sync(&sec_param_data_str.sec_param_work);
	cancel_work_sync(&sec_param_data_extra.sec_param_work);
	dev_info(&pdev->dev, "%s: exit\n", __func__);
}

static const struct of_device_id sec_param_of_match[] = {
	{ .compatible = "samsung,sec-param" },
	{}
};

static struct platform_driver sec_param_driver = {
	.probe = sec_param_probe,
	.shutdown = sec_param_shutdown,
	.driver = {
		.name = "sec-param",
		.of_match_table = sec_param_of_match,
	},
};
module_platform_driver(sec_param_driver);

MODULE_DESCRIPTION("Samsung PARAM driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sec-param");
