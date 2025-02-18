/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * KQ(Kernel Quality) MESH driver implementation
 *  : Jaecheol Kim <jc22.kim@samsung.com>
 *    ChangHwi Seo <c.seo@samsung.com>
 */

#include <asm/cputype.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/platform_device.h>
#include <linux/sec_class.h>
#include <linux/kq/kq_mesh.h>
#include <linux/kq/kq_mesh_ecc.h>
#include <linux/kq/kq_mesh_sysregs.h>
#include <linux/kq/kq_mesh_user_nad.h>

/* list of cpu type info */
static LIST_HEAD(kq_mesh_cluster_list);

/* per cpu, dsu ecc result */
static DEFINE_PER_CPU(struct kq_mesh_func_ecc_result, mesh_ecc_result);

/* module param to determine driver loading or not loading */
static unsigned int kq_mesh_drv_loading;
module_param(kq_mesh_drv_loading, uint, 0440);

/* func list of /sys/class/sec/sec_kq_mesh/func */
struct kq_mesh_func_name kq_mesh_func_list[] = {
	{ "ecc-checker", KQ_MESH_FEATURE_ECC_CHECKER, kq_mesh_func_ecc_checker },

	{ NULL, KQ_MESH_FEATURE_END, NULL},
};

/* temporary set always enable driver loading */
static inline bool kq_mesh_is_drv_enable(void)
{
#if defined(CONFIG_SEC_FACTORY)
	return true;
#else
	if (kq_mesh_drv_loading == KQ_MESH_IS_DRV_LOADING)
		return true;
	return false;
#endif
}

static inline bool kq_mesh_is_var_null(void *var)
{
	if (!var)
		return true;
	return false;
}

static inline bool kq_mesh_is_support_ecc_func(struct kq_mesh_info *kminfo)
{
	if ((kminfo->support & BIT(KQ_MESH_SUPPORT_ECC)) != 0)
		return true;
	return false;
}

static inline bool kq_mesh_is_support_user_nad_func(struct kq_mesh_info *kminfo)
{
	if ((kminfo->support & BIT(KQ_MESH_SUPPORT_USER_NAD)) != 0)
		return true;
	return false;
}

static inline bool kq_mesh_is_valid_process(void)
{
	struct task_struct *tsk = current;

	/* only mesh process can run this fuction */
	if (!strncmp(tsk->comm, KQ_MESH_VALID_PROCESS_MESH, KQ_MESH_VALID_PROCESS_LEN) ||
			!strncmp(tsk->comm, KQ_MESH_VALID_PROCESS_BPS, KQ_MESH_VALID_PROCESS_LEN))
		return true;

	pr_info("%s [%s] is not valid process!\n", __func__, tsk->comm);
	return false;
}

static inline int kq_mesh_get_next_start_index(const char *buf, size_t count, int index)
{
	while (index < count) {
		index++;
		if (buf[index] == '_')
			break;
	}
	return index + 1;
}

static inline bool kq_mesh_is_valid_func(int func)
{
	if (func == KQ_MESH_FEATURE_INIT ||
		func >= KQ_MESH_FEATURE_END)
		return false;
	return true;
}

static inline bool kq_mesh_is_support_cpu(void)
{
	unsigned int type = read_cpuid_part_number();

	if (type == ARM_CPU_PART_CORTEX_A55 ||
		type == ARM_CPU_PART_CORTEX_A76 ||
		type == ARM_CPU_PART_CORTEX_A77 ||
		type == ARM_CPU_PART_CORTEX_A78 ||
		type == ARM_CPU_PART_CORTEX_X1 ||
		type == ARM_CPU_PART_KLEIN ||
		type == ARM_CPU_PART_MATTERHORN ||
		type == ARM_CPU_PART_MATTERHORN_ELP)
		return true;
	return false;
}

/*
 * V, bit [30] : Status Register Valid
 * - 0 : invalid, 1 : at least one error has been recored.
 * DE, bit [23] : Deferred Error
 * - 0 : No errors were deferred.
 * - 1 : At least one error was not corrected and deferred.
 * UE, bit [29] : Uncorrected Error
 * - 0 : No errors have been detected, or all detected errors have been either corrected or deferred.
 * - 1 : At least one detected error was not corrected and not deferred.
 * CE, bits [25:24] : Corrected Error
 * - 0 : No errors were corrected.
 * - 10 : At least one error was corrected.
 * MV, bit [26] : Miscellaneous Registers Valid
 * - 0 : invalid, 1 : ERR<n>MISC<m> registers contains additional information for an error recorded by this record
 */
static inline bool kq_mesh_func_ecc_is_misc_valid(ERXSTATUS_EL1_t erxstatus_el1)
{
	if (!erxstatus_el1.field.Valid ||
		erxstatus_el1.field.DE ||
		erxstatus_el1.field.UE ||
		!erxstatus_el1.field.CE ||
		!erxstatus_el1.field.MV)
		return false;
	return true;
}

static inline bool kq_mesh_func_ecc_is_erxstatus_zero(ERXSTATUS_EL1_t erxstatus_el1)
{
	if (erxstatus_el1.reg == 0)
		return true;
	return false;
}

/*
 * OFO, [47] : Sticky overflow bit, other
 * - 1: The fault handling interrupt is generated when the corrected fault handling interrupt is enabled and either overflow bit is set to 1.
 * CECO, [46:40] : Corrected error count, other.
 * OFR, [39] : Sticky overflow bit, repeat
 * - 1: The fault handling interrupt is generated when the corrected fault handling interrupt is enabled and either overflow bit is set to 1.
 * CECR, [38:32] : Corrected error count, repeat.
 */
static void kq_mesh_func_ecc_record_counter(int num)
{
	struct kq_mesh_func_ecc_result *result;
	ERXMISC0_EL1_t erxmisc0_el1;

	erxmisc0_el1.reg = kq_mesh_read_ERXMISC0_EL1();

	result = this_cpu_ptr(&mesh_ecc_result);

	if (unlikely(kq_mesh_is_var_null(result)))
		return;

	result->sel[num].ofo = erxmisc0_el1.field.OFO;
	result->sel[num].ceco = erxmisc0_el1.field.CECO;
	result->sel[num].ofr = erxmisc0_el1.field.OFR;
	result->sel[num].cecr = erxmisc0_el1.field.CECR;

	pr_info("%s [num:%d] [%d:%d:%d:%d]\n", __func__, num,
		result->sel[num].ofo, result->sel[num].ceco, result->sel[num].ofr, result->sel[num].cecr);
}

/*
 * select cache type and return erxstatus_el1
 * - 0 : cpu, 1 : dsu
 */
static void kq_mesh_func_ecc_set_cache_type(int type)
{
	ERRSELR_EL1_t errselr_el1;

	errselr_el1.reg = kq_mesh_read_ERRSELR_EL1();
	errselr_el1.field.SEL = type;
	kq_mesh_write_ERRSELR_EL1(errselr_el1.reg);

	isb();
}

static void kq_mesh_func_ecc_check_addr_error(ERXSTATUS_EL1_t erxstatus_el1, int cpu, int i)
{
	ERXADDR_EL1_t erxaddr_el1;

	if (erxstatus_el1.field.AV) {
		erxaddr_el1.reg = kq_mesh_read_ERXADDR_EL1();
		pr_info("%s [cpu:%d] el1.sel = %d, has error addr : [0x%lx]\n",
			__func__, cpu, i, (unsigned long)erxaddr_el1.reg);
	}
}

static void kq_mesh_func_ecc_check_overflow_and_counter(ERXSTATUS_EL1_t erxstatus_el1, int cpu, int i)
{
	if (!kq_mesh_func_ecc_is_misc_valid(erxstatus_el1) &&
		!kq_mesh_func_ecc_is_erxstatus_zero(erxstatus_el1)) {
		pr_info("%s [cpu:%d] el1.sel = %d, status = 0x%lx\n",
			__func__, cpu, i, (unsigned long)erxstatus_el1.reg);
		return;
	}

	/* update ecc overflow and counter */
	kq_mesh_func_ecc_record_counter(i);
}

/*
 * ECC monitor
 */
static void kq_mesh_func_ecc_check_cpu(void *info)
{
	ERRIDR_EL1_t erridr_el1;
	ERXSTATUS_EL1_t erxstatus_el1;
	int cpu = raw_smp_processor_id();
	int i;

	/* check is ecc support cpu */
	if (!likely(kq_mesh_is_support_cpu())) {
		pr_info("%s [cpu:%d] not supported cpu type check your system!\n", __func__, cpu);
		return;
	}

	/* check if system support cpu, dsu ecc */
	erridr_el1.reg = kq_mesh_read_ERRIDR_EL1();

	for (i = 0; i < (int) erridr_el1.field.NUM; i++) {
		/* set type of ecc */
		kq_mesh_func_ecc_set_cache_type(i);

		erxstatus_el1.reg = kq_mesh_read_ERXSTATUS_EL1();

		/* check if address associated error occured */
		kq_mesh_func_ecc_check_addr_error(erxstatus_el1, cpu, i);

		/* check ecc counter and overflow */
		kq_mesh_func_ecc_check_overflow_and_counter(erxstatus_el1, cpu, i);
	}
}

static void kq_mesh_func_ecc_checker(struct kq_mesh_info *kminfo)
{
	struct kq_mesh_cluster *cluster;
	int cpu;

	if (!kq_mesh_is_support_ecc_func(kminfo)) {
		pr_info("%s feature ecc not supported!\n", __func__);
		return;
	}

	list_for_each_entry(cluster, &kq_mesh_cluster_list, list) {
		for (cpu = cluster->start; cpu <= cluster->end; cpu++)
			smp_call_function_single(cpu, kq_mesh_func_ecc_check_cpu, 0, 0);
	}
}

static int kq_mesh_func_get_ecc_result(struct kq_mesh_info *kminfo, char *buf)
{
	struct kq_mesh_cluster *cluster;
	struct kq_mesh_func_ecc_result *result;
	int len = 0;
	int cpu;
	int num;

	if (kminfo->last_func == KQ_MESH_FEATURE_ECC_CHECKER) {
		list_for_each_entry(cluster, &kq_mesh_cluster_list, list) {
			for (cpu = cluster->start; cpu <= cluster->end; cpu++) {
				result = per_cpu_ptr(&mesh_ecc_result, cpu);
				for (num = 0; num < cluster->ecc->maxsel; num++) {
					pr_info("%s [cpu:%d][%s] other[%d,%d], repeat[%d,%d]\n",
						__func__, cpu, cluster->ecc->sel[num].name,
						result->sel[num].ofo, result->sel[num].ceco,
						result->sel[num].ofr, result->sel[num].cecr);
					len += scnprintf(buf + len, PAGE_SIZE - len, "[cpu(%d:%s)o(%d,%d)r(%d,%d)]\n",
						cpu, cluster->ecc->sel[num].name,
						result->sel[num].ofo, result->sel[num].ceco,
						result->sel[num].ofr, result->sel[num].cecr);
				}
			}
		}
	} else
		len += scnprintf(buf + len, PAGE_SIZE - len, "[%s]\n", "NONE");

	return len;
}

static int kq_mesh_check_support_func_list(struct kq_mesh_info *kminfo, char *buf)
{
	int len = 0;

	if (kq_mesh_is_support_ecc_func(kminfo))
		len += scnprintf(buf + len, PAGE_SIZE - len, "[ecc-checker]");

	if (kq_mesh_is_support_user_nad_func(kminfo))
		len += scnprintf(buf + len, PAGE_SIZE - len, "[user-nad]");

	return len;
}

static struct device_attribute kq_mesh_attrs[] = {
	KQ_MESH_ATTR(func),
	KQ_MESH_ATTR(result),
	KQ_MESH_ATTR(panic),
	KQ_MESH_ATTR(support),
};

static ssize_t kq_mesh_show_func_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kq_mesh_info *kminfo = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", kminfo->last_func);
}

static ssize_t kq_mesh_show_result_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kq_mesh_info *kminfo = dev_get_drvdata(dev);

	if (!kq_mesh_is_support_ecc_func(kminfo))
		return sprintf(buf, "%s\n", "ECC NOT SUPPORTED");

	if (!kq_mesh_is_valid_func(kminfo->last_func))
		return 0;

	return kq_mesh_func_get_ecc_result(kminfo, buf);
}

static ssize_t kq_mesh_show_support_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct kq_mesh_info *kminfo = dev_get_drvdata(dev);

	return kq_mesh_check_support_func_list(kminfo, buf);
}

static ssize_t kq_mesh_show_attrs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - kq_mesh_attrs;
	int i = 0;

	pr_info("%s tsk [%s]\n", __func__, current->comm);

	switch (offset) {
	case KQ_MESH_SYSFS_FUNC:
		i = kq_mesh_show_func_attr(dev, attr, buf);
		break;
	case KQ_MESH_SYSFS_RESULT:
		i = kq_mesh_show_result_attr(dev, attr, buf);
		break;
	case KQ_MESH_SYSFS_PANIC:
		break;
	case KQ_MESH_SYSFS_SUPPORT:
		i = kq_mesh_show_support_attr(dev, attr, buf);
		break;
	default:
		pr_err("%s kq mesh out of range\n", __func__);
		i = -EINVAL;
		break;
	}
	return i;
}

static ssize_t kq_mesh_store_func_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct kq_mesh_info *kminfo = dev_get_drvdata(dev);
	struct kq_mesh_func_name *info;

	for (info = kq_mesh_func_list; info->func; info++) {
		if (!strncmp(buf, info->name, sizeof((const char **)&info->name))) {
			pr_info("%s run %s func!\n", __func__, info->name);
			kminfo->last_func = info->type;
			info->func(kminfo);
		}
	}
	return count;
}

static ssize_t kq_mesh_store_result_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t kq_mesh_store_panic_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	panic("[mesh-k] %s\n", buf);
	return count;
}

static ssize_t kq_mesh_store_support_attr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t kq_mesh_store_attrs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	const ptrdiff_t offset = attr - kq_mesh_attrs;

	pr_info("%s tsk [%s]\n", __func__, current->comm);

	/* Only mesh can do these operations */
	if (!kq_mesh_is_valid_process())
		return count;

	switch (offset) {
	case KQ_MESH_SYSFS_FUNC:
		count = kq_mesh_store_func_attr(dev, attr, buf, count);
		break;
	case KQ_MESH_SYSFS_RESULT:
		count = kq_mesh_store_result_attr(dev, attr, buf, count);
		break;
	case KQ_MESH_SYSFS_PANIC:
		count = kq_mesh_store_panic_attr(dev, attr, buf, count);
		break;
	case KQ_MESH_SYSFS_SUPPORT:
		count = kq_mesh_store_support_attr(dev, attr, buf, count);
		break;
	default:
		pr_info("%s store kq mesh out of range\n", __func__);
		break;
	}
	return count;
}

static int kq_mesh_create_attr(struct device *dev)
{
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(kq_mesh_attrs); i++) {
		ret = device_create_file(dev, &kq_mesh_attrs[i]);
		if (ret)
			goto create_mesh_attr_fail;
	}
	return 0;

create_mesh_attr_fail:
	while (i--) {
		pr_err("%s remove %dth file\n", __func__, i);
		device_remove_file(dev, &kq_mesh_attrs[i]);
	}
	return -EINVAL;
}

static int kq_mesh_create_sysfs(struct platform_device *pdev,
	struct kq_mesh_info *kminfo)
{
	int ret;

	kminfo->sdev = sec_device_create(kminfo, "sec_kq_mesh");
	if (IS_ERR(kminfo->sdev)) {
		pr_err("%s sysfs create fail\n", __func__);
		return PTR_ERR(kminfo->sdev);
	}

	ret = kq_mesh_create_attr(kminfo->sdev);
	if (ret) {
		pr_err("%s attr sysfs fail\n", __func__);
		goto error_create_mesh_sysfs;
	}
	return 0;

error_create_mesh_sysfs:
	sec_device_destroy(kminfo->sdev->devt);
	return -EINVAL;
}

static int kq_mesh_parse_feature_ecc_dt(struct platform_device *pdev,
	struct device_node *dn, struct kq_mesh_info *kminfo)
{
	struct device_node *ecc_dp = of_find_node_by_name(dn, "ecc");
	struct device_node *cluster_dp;
	struct kq_mesh_cluster *cluster;
	struct kq_mesh_ecc_type *info;
	int ret;

	if (unlikely(kq_mesh_is_var_null(ecc_dp))) {
		pr_err("%s ecc not exist\n", __func__);
		return -EINVAL;
	}

	for_each_child_of_node(ecc_dp, cluster_dp) {
		cluster = devm_kzalloc(&pdev->dev,
			sizeof(struct kq_mesh_cluster), GFP_KERNEL);

		if (!cluster) {
			pr_err("%s failed alloc cluster\n", __func__);
			return -ENOMEM;
		}

		ret = of_property_read_string(cluster_dp, "cpu,name",
			(char const **)&cluster->name);
		if (ret) {
			pr_err("%s can't get cpu,name\n", __func__);
			devm_kfree(&pdev->dev, cluster);
			return ret;
		}

		ret = of_property_read_u32(cluster_dp, "cpu,start", &cluster->start);
		if (ret) {
			pr_err("%s can't get cpu,start\n", __func__);
			devm_kfree(&pdev->dev, cluster);
			return ret;
		}

		ret = of_property_read_u32(cluster_dp, "cpu,end", &cluster->end);
		if (ret) {
			pr_err("%s can't get cpu,end\n", __func__);
			devm_kfree(&pdev->dev, cluster);
			return ret;
		}

		for (info = kq_mesh_support_ecc_list; info->name; info++) {
			if (!strncmp(cluster->name, info->name, info->size)) {
				pr_info("%s %s supported!\n", __func__, info->name);
				cluster->ecc = info;
				break;
			}
		}

		if (!info->name) {
			pr_info("%s %s not supported in mesh driver!\n", __func__, cluster->name);
			devm_kfree(&pdev->dev, cluster);
			return -EINVAL;
		}

		kminfo->cpu += 1;

		pr_info("%s [cluster:%s start:%d end:%d][%d] registered!\n", __func__,
			cluster->name, cluster->start, cluster->end, kminfo->cpu);

		list_add_tail(&cluster->list, &kq_mesh_cluster_list);
	}
	return 0;
}

static int kq_mesh_parse_feature_user_nad_dt(struct platform_device *pdev,
	struct device_node *dn, struct kq_mesh_info *kminfo)
{
	struct device_node *user_nad_dp = of_find_node_by_name(dn, "user_nad");
	struct kq_mesh_user_nad_info *kmn_info = (struct kq_mesh_user_nad_info *)kminfo->user_nad_info;
	int ret, len, i;
	unsigned int *u32_copy_area_address_arr = NULL;

	if (unlikely(kq_mesh_is_var_null(user_nad_dp))) {
		pr_err("%s there's no features nad inforamtion!\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(user_nad_dp, "user_nad,copy_area_len", &len);
	if (ret) {
		pr_err("%s can't get copy_area_len\n", __func__);
		return ret;
	}

	kmn_info->area_len = len;
	kmn_info->copy_area_size_arr = devm_kzalloc(&pdev->dev,
				sizeof(kmn_info->copy_area_size_arr) * len, GFP_KERNEL);
	if (unlikely(kq_mesh_is_var_null(kmn_info->copy_area_size_arr))) {
		pr_err("%s alloc fail(copy_area_size_arr)\n", __func__);
		goto error_init_area_variables;
	}
	ret = of_property_read_u32_array(user_nad_dp, "user_nad,copy_area_size_arr",
					kmn_info->copy_area_size_arr, kmn_info->area_len);
	if (ret) {
		pr_err("%s can't get copy_area_size_arr\n", __func__);
		goto free_copy_area_size_arr;
	}

	kmn_info->copy_area_address_arr = devm_kzalloc(&pdev->dev,
					sizeof(kmn_info->copy_area_address_arr) * len, GFP_KERNEL);
	if (unlikely(kq_mesh_is_var_null(kmn_info->copy_area_address_arr))) {
		pr_err("%s alloc fail(copy_area_address_arr)\n", __func__);
		goto free_copy_area_size_arr;
	}

	u32_copy_area_address_arr = kmalloc(sizeof(unsigned int) * len * 2, GFP_KERNEL);
	if (unlikely(kq_mesh_is_var_null(u32_copy_area_address_arr))) {
		pr_err("%s alloc fail(u32_copy_area_address_arr)\n", __func__);
		goto free_copy_area_address_arr;
	}
	ret = of_property_read_u32_array(user_nad_dp, "user_nad,copy_area_address_arr",
					u32_copy_area_address_arr, kmn_info->area_len);
	if (ret) {
		pr_err("%s can't get copy_area_address_arr\n", __func__);
		goto free_u32_copy_area_address_arr;
	}

	for (i = 0; i < len; i++)
		kmn_info->copy_area_address_arr[i] = ((unsigned long long)u32_copy_area_address_arr[i << 1] << 32)
					+ u32_copy_area_address_arr[(i << 1) + 1];
	kfree(u32_copy_area_address_arr);

	return 0;

free_u32_copy_area_address_arr:
	kfree(u32_copy_area_address_arr);
free_copy_area_address_arr:
	devm_kfree(&pdev->dev, kmn_info->copy_area_address_arr);
free_copy_area_size_arr:
	devm_kfree(&pdev->dev, kmn_info->copy_area_size_arr);
error_init_area_variables:
	return -EINVAL;
}

static int kq_mesh_parse_dt(struct platform_device *pdev,
	struct kq_mesh_info *kminfo)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *dn = of_find_node_by_name(np, "features");

	if (unlikely(kq_mesh_is_var_null(dn))) {
		pr_err("%s there's no features mesh information!\n", __func__);
		return -EINVAL;
	}

	if (kq_mesh_parse_feature_ecc_dt(pdev, dn, kminfo))
		pr_err("%s failed to initialize ecc feature\n", __func__);
	else
		kminfo->support |= BIT(KQ_MESH_SUPPORT_ECC);

	if (kq_mesh_parse_feature_user_nad_dt(pdev, dn, kminfo))
		pr_err("%s failed to initialize user nad feature\n", __func__);
	else
		kminfo->support |= BIT(KQ_MESH_SUPPORT_USER_NAD);

	return 0;
}

static struct kq_mesh_info *kq_mesh_alloc_init_info(struct platform_device *pdev)
{
	struct kq_mesh_info *kminfo;

	kminfo = devm_kzalloc(&pdev->dev,
				sizeof(struct kq_mesh_info), GFP_KERNEL);

	if (unlikely(kq_mesh_is_var_null(kminfo)))
		return NULL;

	kminfo->user_nad_info = devm_kzalloc(&pdev->dev,
				sizeof(struct kq_mesh_user_nad_info), GFP_KERNEL);

	if (unlikely(kq_mesh_is_var_null(kminfo->user_nad_info))) {
		devm_kfree(&pdev->dev, kminfo);
		return NULL;
	}

	kminfo->cpu = KQ_MESH_FEATURE_INIT;
	kminfo->last_func = KQ_MESH_FEATURE_INIT;
	kminfo->support = KQ_MESH_SUPPORT_INIT;

	kq_mesh_user_nad_init_kmn_info(kminfo->user_nad_info);

	return kminfo;
}

static void kq_mesh_ecc_result_init(void)
{
	struct kq_mesh_func_ecc_result *result;
	struct kq_mesh_cluster *cluster;
	int cpu;
	int num;

	list_for_each_entry(cluster, &kq_mesh_cluster_list, list) {
		for (cpu = cluster->start; cpu <= cluster->end; cpu++) {
			result = per_cpu_ptr(&mesh_ecc_result, cpu);
			for (num = 0; num < cluster->ecc->maxsel; num++) {
				result->sel[num].ofo = KQ_MESH_ECC_INIT_VAR;
				result->sel[num].ceco = KQ_MESH_ECC_INIT_VAR;
				result->sel[num].ofr = KQ_MESH_ECC_INIT_VAR;
				result->sel[num].cecr = KQ_MESH_ECC_INIT_VAR;
			}
		}
	}
}

static int kq_mesh_probe(struct platform_device *pdev)
{
	struct kq_mesh_info *kminfo = NULL;
	int ret = 0;

	if (!kq_mesh_is_drv_enable())
		return -EINVAL;

	pr_info("%s mesh drv loading..\n", __func__);

	kminfo = kq_mesh_alloc_init_info(pdev);
	if (unlikely(kq_mesh_is_var_null(kminfo))) {
		pr_err("%s failed to allocate mesh info\n", __func__);
		return ret;
	}

	if (pdev->dev.of_node) {
		ret = kq_mesh_parse_dt(pdev, kminfo);
		if (ret) {
			pr_err("%s failed to parse mesh dt\n", __func__);
			goto free_mesh_info;
		}
		if (kq_mesh_is_support_ecc_func(kminfo))
			kq_mesh_ecc_result_init();
	} else
		return -EINVAL;

	ret = kq_mesh_create_sysfs(pdev, kminfo);
	if (ret) {
		pr_err("%s failed to create mesh sysfs\n", __func__);
		goto free_mesh_info;
	}

	if (kq_mesh_is_support_user_nad_func(kminfo)) {
		ret = kq_mesh_user_nad_create_devfs();
		if (ret) {
			pr_err("%s failed to create mesh devfs\n", __func__);
			goto free_mesh_info;
		}
	}

	platform_set_drvdata(pdev, kminfo);
	return ret;

free_mesh_info:
	devm_kfree(&pdev->dev, kminfo);
	return -EINVAL;
}

static const struct of_device_id of_kq_mesh_match[] = {
	{ .compatible = "samsung,kq-mesh", },
	{ },
};

MODULE_DEVICE_TABLE(of, of_kq_mesh_match);

static struct platform_driver kq_mesh_driver = {
	.driver = {
		.name = "kq-mesh",
		.owner = THIS_MODULE,
		.of_match_table = of_kq_mesh_match,
	},
	.probe = kq_mesh_probe,
};

module_platform_driver(kq_mesh_driver);

MODULE_DESCRIPTION("kernel quality mesh driver");
MODULE_AUTHOR("kq-slsi");
MODULE_LICENSE("GPL");
