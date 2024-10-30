/* SPDX-License-Identifier: GPL-2.0-or-later */
/* include/soc/samsung/asv_g_spec.h
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASV_G_SPEC_H__
#define __ASV_G_SPEC_H__

#include <soc/samsung/ect_parser.h>
#include <soc/samsung/cal-if.h>
#include <linux/io.h>

#include "../../../drivers/soc/samsung/cal-if/cmucal.h"
#include "../../../drivers/soc/samsung/cal-if/asv.h"
//#include "vclk.h"

extern struct vclk acpm_vclk_list[];
extern unsigned int acpm_vclk_size;
#define get_domain_name(type)	(acpm_vclk_list[type].name)
#define SPEC_STR_SIZE		(30)

#define asv_g_spec(domain, id, fused_addr, bit_num)						\
static ssize_t show_asv_g_spec_##domain##_fused_volt						\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)					\
{												\
	return show_asv_g_spec(id, ASV_G_FUSED, buf, fused_addr, bit_num);			\
}												\
static struct kobj_attribute asv_g_spec_##domain##_fused_volt =					\
__ATTR(domain##_fused_volt, 0400, show_asv_g_spec_##domain##_fused_volt, NULL);			\
static ssize_t show_asv_g_spec_##domain##_grp_volt						\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)					\
{												\
	return show_asv_g_spec(id, ASV_G_GRP, buf, fused_addr, bit_num);			\
}												\
static struct kobj_attribute asv_g_spec_##domain##_grp_volt =					\
__ATTR(domain##_grp_volt, 0400, show_asv_g_spec_##domain##_grp_volt, NULL)

#define asv_g_spec_attr(domain)									\
	&asv_g_spec_##domain##_fused_volt.attr,							\
	&asv_g_spec_##domain##_grp_volt.attr

enum spec_volt_type {
	ASV_G_FUSED = 0,
	ASV_G_GRP
};

static ssize_t show_asv_g_spec(int id, enum spec_volt_type type,
		char *buf, unsigned int fused_addr, unsigned int bit_num)
{
	void *gen_block;
	struct ect_gen_param_table *spec;
	int asv_tbl_ver, asv_grp, tbl_size, j, vtyp_freq, num_lv;
	unsigned int fused_volt = 0, grp_volt = 0, volt;
	struct dvfs_rate_volt rate_volt[48];
	unsigned int *spec_table = NULL;
	ssize_t size = 0, len = 0;
	char spec_str[SPEC_STR_SIZE] = {0, };
	unsigned long fused_val;
	void __iomem *fused_addr_va;

	if (id >= acpm_vclk_size) {
		pr_err("%s: cannot found dvfs domain: %d\n", __func__, id);
		goto out;
	}

	gen_block = ect_get_block("GEN");
	if (gen_block == NULL) {
		pr_err("%s: Failed to get gen block from ECT\n", __func__);
		goto out;
	}

	asv_grp = asv_get_grp(id | ACPM_VCLK_TYPE);
	if (!asv_grp) {
		pr_err("%s: There has no ASV-G information for %s group 0\n",
				__func__, get_domain_name(id));
		goto out;
	}

	len = snprintf(spec_str, SPEC_STR_SIZE, "SPEC_%s", get_domain_name(id));
	if (len < 0)
		goto out;

	spec = ect_gen_param_get_table(gen_block, spec_str);
	if (spec == NULL) {
		pr_err("%s: Failed to get spec table from ECT\n", __func__);
		goto out;
	}

	asv_tbl_ver = asv_get_table_ver();
	for (j = 0; j < spec->num_of_row; j++) {
		spec_table = &spec->parameter[spec->num_of_col * j];
		if (spec_table[0] == asv_tbl_ver) {
			grp_volt = spec_table[asv_grp + 2];
			vtyp_freq = spec_table[1];
			break;
		}
	}
	if (j == spec->num_of_row) {
		pr_err("%s: Do not support ASV-G, asv table version:%d\n", __func__, asv_tbl_ver);
		goto out;
	}

	if (!grp_volt) {
		pr_err("%s: Failed to get grp volt\n", __func__);
		goto out;
	}

	num_lv = cal_dfs_get_lv_num(id | ACPM_VCLK_TYPE);
	tbl_size = cal_dfs_get_rate_asv_table(id | ACPM_VCLK_TYPE, rate_volt);
	if (!tbl_size) {
		pr_err("%s: Failed to get asv table\n", __func__);
		goto out;
	}

	if (fused_addr) {
		if ((fused_addr & 0x9000) == 0x9000) {
			exynos_smc_readsfr((unsigned long)fused_addr, &fused_val);
		} else {
			fused_addr_va = ioremap(fused_addr, 4);
			fused_val = __raw_readl(fused_addr_va);
			iounmap(fused_addr_va);
		}

		fused_volt = (((unsigned int)fused_val & (0xff << bit_num)) >> bit_num) * 6250;
	} else {
		for (j = 0; j < num_lv; j++) {
			if (rate_volt[j].rate == vtyp_freq) {
				fused_volt = rate_volt[j].volt;
				break;
			}
		}
		if (j == num_lv) {
			pr_err("%s: There has no frequency %d on %d domain\n", __func__,
					vtyp_freq, get_domain_name(id));
			goto out;
		}
	}

	volt = (type == ASV_G_FUSED) ? fused_volt : grp_volt;

	size += snprintf(buf + size, PAGE_SIZE, "%d\n", volt);
out:

	return size;
}
#endif
