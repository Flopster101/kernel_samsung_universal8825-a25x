/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License Version 2 as publised
 * by the Free Software Foundation.
 *
 * BTS Bus Traffic Shaper device driver
 *
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/cal-if.h>
#include <dt-bindings/soc/samsung/exynos-bts.h>

#include "bts.h"

#define BTS_PDEV_NAME	"exynos-bts"
#define ID_DEFAULT	0

#define BTSDBG_LOG(x...)	if (btsdbg_log)	dev_notice(x)

static bool btsdbg_log = false;

static struct exynos_pm_qos_request exynos_mif_qos;
static struct exynos_pm_qos_request exynos_int_qos;

static struct bts_device *btsdev;

static void bts_calc_bw(void)
{
	unsigned int i = 0;
	unsigned int total_read = 0;
	unsigned int total_write = 0;
	unsigned int mif_freq, int_freq;
	struct bts_bw_params *bw_params = &btsdev->bw_params;

	mutex_lock(&btsdev->mutex_lock);

	btsdev->peak_bw = 0;
	btsdev->total_bw = 0;
	for (i = 0; i < btsdev->num_bts; i++) {
		if (btsdev->peak_bw < btsdev->bts_bw[i].peak)
			btsdev->peak_bw = btsdev->bts_bw[i].peak;

		total_read += btsdev->bts_bw[i].read;
		total_write += btsdev->bts_bw[i].write;
	}

	btsdev->total_bw = total_read + total_write;
	if (btsdev->peak_bw < (total_read / bw_params->num_channel))
		btsdev->peak_bw = (total_read / bw_params->num_channel);
	if (btsdev->peak_bw < (total_write / bw_params->num_channel))
		btsdev->peak_bw = (total_write / bw_params->num_channel);

	mif_freq = (btsdev->total_bw * 100) / bw_params->mif_bus_width / bw_params->mif_util;
	int_freq = (btsdev->peak_bw * 100) / bw_params->bus_width / bw_params->int_util;

	BTSDBG_LOG(btsdev->dev, "BW: T:%.8u R:%.8u W:%.8u P:%.8u MIF:%.8u INT:%.8u\n",
			btsdev->total_bw, total_read, total_write, btsdev->peak_bw, mif_freq, int_freq);

#if defined(CONFIG_EXYNOS_PM_QOS) || defined(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_update_request(&exynos_mif_qos, mif_freq);
	exynos_pm_qos_update_request(&exynos_int_qos, int_freq);
#endif

	mutex_unlock(&btsdev->mutex_lock);
}

static void bts_set(unsigned int scen, unsigned int index)
{
	struct bts_info *info = btsdev->bts_list;
	int ret = 0;

	if (info[index].ops->set_bts != NULL) {
		if (info[index].pd_on && info[index].status) {
			/* Check scenario set exists */
			if (info[index].stat[scen].stat_on) {
				ret = info[index].ops->set_bts(info[index].va_base, &(info[index].stat[scen]));
			} else{
				ret = info[index].ops->set_bts(info[index].va_base, &(info[index].stat[ID_DEFAULT]));
			}

		}
		if (ret)
			dev_err(btsdev->dev, "%s failed! (scenario=%u) (index=%u)\n",
					__func__, scen, index);
	}
}

void bts_pd_sync(unsigned int cal_id, int on)
{
	struct bts_info *info = btsdev->bts_list;
	unsigned int i;

	if (info == NULL)
		return;

	spin_lock(&btsdev->lock);

	for (i = 0; i < btsdev->num_bts; i++) {
		if (!info[i].pd_id)
			continue;
		if (info[i].pd_id == cal_id) {
			info[i].pd_on = on ? true : false;
			if (on)
				bts_set(btsdev->top_scen, i);
		}
	}

	spin_unlock(&btsdev->lock);
}

int bts_get_bwindex(const char *name)
{
	struct bts_bw *bw = btsdev->bts_bw;
	unsigned int index = 0;
	int ret = 0;

	spin_lock(&btsdev->lock);

	for (index = 0; (bw[index].name != NULL) && (index < btsdev->num_bts); index++) {
		if (!strcmp(bw[index].name, name)) {
			ret = index;
			goto out;
		}
	}

	if (index == btsdev->num_bts) {
		ret = -EINVAL;
		goto out;
	} else {
		bw[index].name = devm_kstrdup(btsdev->dev, name, GFP_ATOMIC);
		if (bw[index].name == NULL) {
			dev_err(btsdev->dev, "failed to allocate bandwidth name\n");
			ret = -ENOMEM;
			goto out;
		}
		ret = index;
	}
out:
	spin_unlock(&btsdev->lock);
	return ret;
}
EXPORT_SYMBOL(bts_get_bwindex);

unsigned int bts_get_scenindex(const char *name)
{
	unsigned int index;
	struct bts_scen *scen = btsdev->scen_list;

	spin_lock(&btsdev->lock);

	for (index = 1;	index < btsdev->num_scen; index++)
		if (strcmp(name, scen[index].name) == 0)
			break;

	if (index == btsdev->num_scen) {
		spin_unlock(&btsdev->lock);
		return 0;
	}

	spin_unlock(&btsdev->lock);

	return index;
}
EXPORT_SYMBOL(bts_get_scenindex);

int bts_update_bw(unsigned int index, struct bts_bw bw)
{
	if (index >= btsdev->num_bts) {
		dev_err(btsdev->dev, "Invalid index! Should be smaller than %u(index=%u)\n",
				btsdev->num_bts, index);
		return -EINVAL;
	}

	spin_lock(&btsdev->lock);
	btsdev->bts_bw[index].peak = bw.peak;
	btsdev->bts_bw[index].read = bw.read;
	btsdev->bts_bw[index].write = bw.write;
	spin_unlock(&btsdev->lock);

	bts_calc_bw();

	return 0;
}
EXPORT_SYMBOL(bts_update_bw);

int bts_add_scenario(unsigned int index)
{
	struct bts_scen *scen = btsdev->scen_list;
	unsigned int i = 0;

	spin_lock(&btsdev->lock);

	if (index >= btsdev->num_scen) {
		dev_err(btsdev->dev, "Invalid scenario index!\n");
		spin_unlock(&btsdev->lock);
		return -EINVAL;
	}

	if (index == ID_DEFAULT && scen[index].usage_count != 0) {
		dev_notice(btsdev->dev, "Default scenario cannot register additional!\n");
		spin_unlock(&btsdev->lock);
		return 0;
	}

	scen[index].usage_count++;

	if (scen[index].usage_count == 1) {
		list_add(&scen[index].node, &btsdev->scen_node);
		scen[index].status = true;

		if (index >= btsdev->top_scen) {
			btsdev->top_scen = index;
			for (i = 0; i < btsdev->num_bts; i++)
				bts_set(btsdev->top_scen, i);
		}
	}

	spin_unlock(&btsdev->lock);

	return 0;
}
EXPORT_SYMBOL(bts_add_scenario);

int bts_del_scenario(unsigned int index)
{
	struct bts_scen *scen = btsdev->scen_list;
	unsigned int i = 0;

	spin_lock(&btsdev->lock);
	/* CAUTION: Default scenario cannot be deleted! */
	if (index >= btsdev->num_scen) {
		dev_err(btsdev->dev, "Invalid scenario index!\n");
		spin_unlock(&btsdev->lock);
		return -EINVAL;
	}

	if (index == ID_DEFAULT) {
		dev_notice(btsdev->dev, "Default scenario cannot be deleted!\n");
		spin_unlock(&btsdev->lock);
		return 0;
	}

	scen[index].usage_count--;

	if (scen[index].usage_count < 0) {
		dev_warn(btsdev->dev, "Usage count is below 0!\n");
		scen[index].usage_count = 0;
		spin_unlock(&btsdev->lock);
		return 0;
	}

	if (scen[index].usage_count == 0) {
		list_del(&scen[index].node);
		scen[index].status = false;

		if (index == btsdev->top_scen) {
			btsdev->top_scen = ID_DEFAULT;
			list_for_each_entry(scen, &btsdev->scen_node, node)
				if (scen->index >= btsdev->top_scen)
					btsdev->top_scen = scen->index;
			for (i = 0; i < btsdev->num_bts; i++)
				bts_set(btsdev->top_scen, i);
		}
	}

	spin_unlock(&btsdev->lock);

	return 0;
}
EXPORT_SYMBOL(bts_del_scenario);

int bts_change_mo(unsigned int scen, unsigned int ip,
		  unsigned int rmo, unsigned int wmo)
{
	struct bts_info *info = btsdev->bts_list;
	struct bts_stat *stat;
	unsigned int old_rmo = 0;
	unsigned int old_wmo = 0;
	int ret = 0;

	if (ip >= btsdev->num_bts) {
		pr_err("%s: IP ip should be in range of (0 ~ %d). input=(%d)\n",
			__func__, btsdev->num_bts - 1, ip);
		return -EINVAL;
	}

	if (scen >= btsdev->num_scen) {
		pr_err("%s: SCEN ip should be in range of (0 ~ %d). input=(%d)\n",
			__func__, btsdev->num_scen - 1, scen);
		return -EINVAL;
	}

	stat = info[ip].stat;

	spin_lock(&btsdev->lock);
	if (stat[scen].rmo == rmo && stat[scen].wmo == wmo) {
		pr_info("%s: requested same mo: rmo: %d, wmo: %d\n",
				__func__, rmo, wmo);
		goto out;
	}

	stat[scen].stat_on = true;
	old_rmo = stat[scen].rmo;
	old_wmo = stat[scen].wmo;
	stat[scen].rmo = rmo;
	stat[scen].wmo = wmo;

	if (scen != btsdev->top_scen)
		goto out;

	if (info[ip].ops->set_mo != NULL) {
		if (info[ip].pd_on) {
			if (info[ip].ops->set_mo(info[ip].va_base, &stat[scen]))
				pr_warn("%s: set_mo failed. input=(%d)\n",
						__func__, ip);
		}
	}

	pr_info("%s: scenario:%d, ip: %d, rmo: %d -> %d, wmo: %d -> %d\n",
			__func__, scen, ip, old_rmo, rmo, old_wmo, wmo);
out:
	spin_unlock(&btsdev->lock);
	return ret;
}
EXPORT_SYMBOL(bts_change_mo);

/* DebugFS for BTS */
static int exynos_bts_hwstatus_open_show(struct seq_file *buf, void *d)
{
	struct bts_info info;
	struct bts_stat stat;
	int ret, i = 0;

	/* Check BTS setting */
	for (i = 0; i < btsdev->num_bts; i++) {
		info = btsdev->bts_list[i];

		if (!info.va_base)
			continue;
		if (!info.pd_on)
			continue;
		if (info.type == SCI_BTS)
			stat = *(info.stat);

		if (info.ops->get_bts != NULL && info.type == IP_BTS) {
			spin_lock(&btsdev->lock);
			ret = info.ops->get_bts(info.va_base, &stat);
			spin_unlock(&btsdev->lock);

			if (!ret) {
				seq_printf(buf, "%s:\tARQOS 0x%X, AWQOS 0x%X, RMO 0x%.4X, WMO 0x%.4X, "\
					"QUR(0x%X) TH_R 0x%.2X, TH_W 0x%.2X, ",
					info.name, stat.arqos, stat.awqos, stat.rmo, stat.wmo,
					stat.qurgent_on, stat.qurgent_th_r, stat.qurgent_th_w);
				if (stat.blocking_on)
					seq_printf(buf, "BLK(1) FR 0x%.4X, FW 0x%.4X, BR 0x%.4X, BW 0x%.4X, "\
							"MAX0_R 0x%.4X, MAX0_W 0x%.4X, MAX1_R 0x%.4X, MAX1_W 0x%.4X\n",
						stat.qfull_limit_r, stat.qfull_limit_w, stat.qbusy_limit_r, stat.qbusy_limit_w,
						stat.qmax0_limit_r, stat.qmax0_limit_w, stat.qmax1_limit_r, stat.qmax1_limit_w);
				else
					seq_printf(buf, "BLK(0)\n");
			}
		}
	}

	return 0;
}

static int exynos_bts_hwstatus_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_bts_hwstatus_open_show, inode->i_private);
}

static int exynos_bts_scenario_open_show(struct seq_file *buf, void *d)
{
	struct bts_scen *scen;
	unsigned int i = 0;

	seq_printf(buf, "Current Top scenario: [%u]%s\n  ",
			btsdev->top_scen,
			btsdev->scen_list[btsdev->top_scen].name);
	list_for_each_entry(scen, &(btsdev->scen_node), node)
		seq_printf(buf, "%u - ", scen->index);
	seq_printf(buf, "\n");

	for (i = 0; i < btsdev->num_scen; i++)
		seq_printf(buf, "bts scen[%u] %s(%d) - status: %s\n",
			btsdev->scen_list[i].index,
			btsdev->scen_list[i].name,
			btsdev->scen_list[i].usage_count,
			(btsdev->scen_list[i].status ? "on" : "off"));

	return 0;
}

static int exynos_bts_scenario_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_bts_scenario_open_show, inode->i_private);
}

static ssize_t exynos_bts_scenario_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[32];
	ssize_t buf_size;
	int ret, scen, on;

	buf_size = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (buf_size < 0)
		return buf_size;

	buf[buf_size] = '\0';

	ret = sscanf(buf, "%d %d\n", &scen, &on);
	if (ret != 2) {
		pr_err("%s: sscanf failed. Invalid input variables. count=(%d)\n",
			__func__, ret);
		return -EINVAL;
	}

	if (scen >= btsdev->num_scen) {
		pr_err("%s: Index should be in range of (0 ~ %d). input=(%d)\n",
			__func__, btsdev->num_scen - 1, scen);
		return -EINVAL;
	}

	if (on == 1)
		bts_add_scenario(scen);
	else if (on == 0)
		bts_del_scenario(scen);

	return buf_size;
}

static int exynos_bts_qos_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *info = btsdev->bts_list;
	struct bts_stat stat;
	int ret, i = 0;

	for (i = 0; i < btsdev->num_bts; i++) {
		if (info[i].ops->get_qos == NULL)
			continue;

		spin_lock(&btsdev->lock);

		if (info[i].pd_on) {
			if (info[i].type == SCI_BTS)
				stat = *(info[i].stat);

			ret = info[i].ops->get_qos(info[i].va_base, &stat);
				if (!stat.bypass)
					seq_printf(buf, "[%d] %s:   \tAR 0x%.1X AW 0x%.1X\n",
						i, info[i].name, stat.arqos, stat.awqos);
		} else {
			seq_printf(buf, "[%d] %s:   \tLocal power off!\n",
					i, info[i].name);
		}

		spin_unlock(&btsdev->lock);
	}

	return 0;
}

static int exynos_bts_qos_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_bts_qos_open_show, inode->i_private);
}

static ssize_t exynos_bts_qos_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[64];
	ssize_t buf_size;

	struct bts_info *info = btsdev->bts_list;
	struct bts_stat *stat;
	int ret, scen, index, bypass, ar, aw;

	buf_size = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (buf_size < 0)
		return buf_size;

	buf[buf_size] = '\0';

	ret = sscanf(buf, "%d %d %d %d %d\n", &scen, &index, &bypass, &ar, &aw);
	if (ret != 5) {
		pr_err("%s: sscanf failed. We need 5 inputs. <SCEN IP BYPASS(0/1) ARQOS AWQOS> count=(%d)\n",
			__func__, ret);
		return -EINVAL;
	}

	if (index >= btsdev->num_bts) {
		pr_err("%s: IP index should be in range of (0 ~ %d). input=(%d)\n",
			__func__, btsdev->num_bts - 1, index);
		return -EINVAL;
	}

	if (info[index].type == SCI_BTS)
		stat = info[index].stat;

	if (scen >= btsdev->num_scen) {
		pr_err("%s: SCEN index should be in range of (0 ~ %d). input=(%d)\n",
			__func__, btsdev->num_scen - 1, scen);
		return -EINVAL;
	}

	stat = info[index].stat;
	if (stat == NULL) {
		pr_err("%s: stat is NULL\n", __func__);
		return -ENOMEM;
	}

	spin_lock(&btsdev->lock);

	stat[scen].stat_on = true;

	if (bypass == 0)
		stat[scen].bypass = false;
	else if (bypass == 1)
		stat[scen].bypass = true;

	stat[scen].arqos = ar;
	stat[scen].awqos = aw;

	if (scen != btsdev->top_scen)
		goto out;

	if (info[index].ops->set_qos != NULL) {
		if (info[index].pd_on) {
			if (info[index].ops->set_qos(info[index].va_base, &stat[scen]))
				pr_warn("%s: set_qos failed. input=(%d) err=(%d)\n",
						__func__, index, ret);
		}
	}

out:
	spin_unlock(&btsdev->lock);

	return buf_size;
}

static int exynos_bts_mo_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *info = btsdev->bts_list;
	struct bts_stat stat;
	int ret, i = 0;

	for (i = 0; i < btsdev->num_bts; i++) {
		if (info[i].ops->get_mo == NULL)
			continue;

		spin_lock(&btsdev->lock);

		if (info[i].pd_on) {
			ret = info[i].ops->get_mo(info[i].va_base, &stat);
			seq_printf(buf, "[%d] %s:   \tRMO 0x%.4X WMO 0x%.4X\n",
					i, info[i].name, stat.rmo, stat.wmo);
		} else {
			seq_printf(buf, "[%d] %s:   \tLocal power off!\n",
					i, info[i].name);
		}

		spin_unlock(&btsdev->lock);
	}

	return 0;
}

static int exynos_bts_mo_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_bts_mo_open_show, inode->i_private);
}

static ssize_t exynos_bts_mo_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[64];
	ssize_t buf_size;

	struct bts_info *info = btsdev->bts_list;
	struct bts_stat *stat;
	int ret, scen, index, rmo, wmo;

	buf_size = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (buf_size < 0)
		return buf_size;

	buf[buf_size] = '\0';

	ret = sscanf(buf, "%d %d %d %d\n", &scen, &index, &rmo, &wmo);
	if (ret != 4) {
		pr_err("%s: sscanf failed. We need 4 inputs. <SCEN IP RMO WMO> count=(%d)\n",
			__func__, ret);
		return -EINVAL;
	}

	if (index >= btsdev->num_bts) {
		pr_err("%s: IP index should be in range of (0 ~ %d). input=(%d)\n",
			__func__, btsdev->num_bts - 1, index);
		return -EINVAL;
	}

	if (scen >= btsdev->num_scen) {
		pr_err("%s: SCEN index should be in range of (0 ~ %d). input=(%d)\n",
			__func__, btsdev->num_scen - 1, scen);
		return -EINVAL;
	}

	stat = info[index].stat;
	if (stat == NULL) {
		pr_err("%s: stat is NULL\n", __func__);
		return -ENOMEM;
	}

	spin_lock(&btsdev->lock);

	stat[scen].stat_on = true;
	stat[scen].rmo = rmo;
	stat[scen].wmo = wmo;

	if (scen != btsdev->top_scen)
		goto out;

	if (info[index].ops->set_mo != NULL) {
		if (info[index].pd_on) {
			if (info[index].ops->set_mo(info[index].va_base, &stat[scen]))
				pr_warn("%s: set_mo failed. input=(%d) err=(%d)\n",
						__func__, index, ret);
		}
	}

out:
	spin_unlock(&btsdev->lock);

	return buf_size;
}

static int exynos_bts_urgent_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *info = btsdev->bts_list;
	struct bts_stat stat;
	int ret, i = 0;

	for (i = 0; i < btsdev->num_bts; i++) {
		if (info[i].ops->get_urgent == NULL)
			continue;

		spin_lock(&btsdev->lock);

		if (info[i].pd_on) {
			ret = info[i].ops->get_urgent(info[i].va_base, &stat);
			seq_printf(buf, "[%d] %s:   \tQUR(0x%X) TH_R 0x%.2X TH_W 0x%.2X\n",
					i, info[i].name, stat.qurgent_on, stat.qurgent_th_r, stat.qurgent_th_w);
		} else {
			seq_printf(buf, "[%d] %s:   \tLocal power off!\n",
					i, info[i].name);
		}

		spin_unlock(&btsdev->lock);
	}

	return 0;
}

static int exynos_bts_urgent_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_bts_urgent_open_show, inode->i_private);
}

static ssize_t exynos_bts_urgent_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[64];
	ssize_t buf_size;

	struct bts_info *info = btsdev->bts_list;
	struct bts_stat *stat;
	int ret, scen, index, on, th_r, th_w;

	buf_size = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (buf_size < 0)
		return buf_size;

	buf[buf_size] = '\0';

	ret = sscanf(buf, "%d %d %d %d %d\n", &scen, &index, &on, &th_r, &th_w);
	if (ret != 5) {
		pr_err("%s: sscanf failed. We need 5 inputs. <SCEN IP ON/OFF TH_R TH_W> count=(%d)\n",
			__func__, ret);
		return -EINVAL;
	}

	if (index >= btsdev->num_bts) {
		pr_err("%s: IP index should be in range of (0 ~ %d). input=(%d)\n",
			__func__, btsdev->num_bts - 1, index);
		return -EINVAL;
	}

	if (scen >= btsdev->num_scen) {
		pr_err("%s: SCEN index should be in range of (0 ~ %d). input=(%d)\n",
			__func__, btsdev->num_scen - 1, scen);
		return -EINVAL;
	}

	stat = info[index].stat;
	if (stat == NULL) {
		pr_err("%s: stat is NULL\n", __func__);
		return -ENOMEM;
	}

	spin_lock(&btsdev->lock);

	stat[scen].stat_on = true;
	stat[scen].qurgent_on = on;
	stat[scen].qurgent_th_r = th_r;
	stat[scen].qurgent_th_w = th_w;

	if (scen != btsdev->top_scen)
		goto out;

	if (info[index].ops->set_urgent != NULL) {
		if (info[index].pd_on) {
			if (info[index].ops->set_urgent(info[index].va_base, &stat[scen]))
				pr_warn("%s: set_urgent failed. input=(%d) err=(%d)\n",
						__func__, index, ret);
		}
	}

out:
	spin_unlock(&btsdev->lock);

	return buf_size;
}

static int exynos_bts_blocking_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *info = btsdev->bts_list;
	struct bts_stat stat;
	int ret, i = 0;

	for (i = 0; i < btsdev->num_bts; i++) {
		if (info[i].ops->get_blocking == NULL)
			continue;

		spin_lock(&btsdev->lock);

		if (info[i].pd_on) {
			ret = info[i].ops->get_blocking(info[i].va_base, &stat);
			if (stat.blocking_on)
				seq_printf(buf, "[%d] %s:   \tBLK(1) FR 0x%.4X, FW 0x%.4X, BR 0x%.4X, BW 0x%.4X, "\
						"MAX0_R 0x%.4X, MAX0_W 0x%.4X, MAX1_R 0x%.4X, MAX1_W 0x%.4X\n",
					i, info[i].name,
					stat.qfull_limit_r, stat.qfull_limit_w, stat.qbusy_limit_r, stat.qbusy_limit_w,
					stat.qmax0_limit_r, stat.qmax0_limit_w, stat.qmax1_limit_r, stat.qmax1_limit_w);
			else
				seq_printf(buf, "[%d] %s:   \tBLK(0)\n",
					i, info[i].name);
		} else {
			seq_printf(buf, "[%d] %s:   \tLocal power off!\n",
					i, info[i].name);
		}

		spin_unlock(&btsdev->lock);
	}

	return 0;
}

static int exynos_bts_blocking_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_bts_blocking_open_show, inode->i_private);
}

static ssize_t exynos_bts_blocking_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[64];
	ssize_t buf_size;

	struct bts_info *info = btsdev->bts_list;
	struct bts_stat *stat;
	int ret;
	int scen, index, on, full_r, full_w, busy_r, busy_w, max0_r, max0_w, max1_r, max1_w;

	buf_size = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (buf_size < 0)
		return buf_size;

	buf[buf_size] = '\0';

	ret = sscanf(buf, "%d %d %d %d %d %d %d %d %d %d %d\n",
			&scen, &index, &on, &full_r, &full_w, &busy_r, &busy_w,
			&max0_r, &max0_w, &max1_r, &max1_w);

	if (ret != 11) {
		pr_err("%s: sscanf failed. We need 11 inputs. <SCEN IP ON/OFF FULL_R, FULL_W, BUSY_R, BUSY_W, "\
				"MAX0_R, MAX0_W, MAX1_R, MAX1_W count=(%d)\n", __func__, ret);
		return -EINVAL;
	}

	if (index >= btsdev->num_bts) {
		pr_err("%s: IP index should be in range of (0 ~ %d). input=(%d)\n",
			__func__, btsdev->num_bts - 1, index);
		return -EINVAL;
	}

	if (scen >= btsdev->num_scen) {
		pr_err("%s: SCEN index should be in range of (0 ~ %d). input=(%d)\n",
			__func__, btsdev->num_scen - 1, scen);
		return -EINVAL;
	}

	stat = info[index].stat;
	if (stat == NULL) {
		pr_err("%s: stat is NULL\n", __func__);
		return -ENOMEM;
	}

	spin_lock(&btsdev->lock);

	stat[scen].stat_on = true;
	stat[scen].blocking_on = on;
	stat[scen].qfull_limit_r = full_r;
	stat[scen].qfull_limit_w = full_w;
	stat[scen].qbusy_limit_r = busy_r;
	stat[scen].qbusy_limit_w = busy_w;
	stat[scen].qmax0_limit_r = max0_r;
	stat[scen].qmax0_limit_w = max0_w;
	stat[scen].qmax1_limit_r = max1_r;
	stat[scen].qmax1_limit_w = max1_w;

	if (scen != btsdev->top_scen)
		goto out;

	if (info[index].ops->set_blocking != NULL) {
		if (info[index].pd_on) {
			if (info[index].ops->set_blocking(info[index].va_base, &stat[scen]))
				pr_warn("%s: set_blocking failed. input=(%d) err=(%d)\n",
						__func__, index, ret);
		}
	}

out:
	spin_unlock(&btsdev->lock);

	return buf_size;
}

static int exynos_bts_log_open_show(struct seq_file *buf, void *d)
{
	seq_printf(buf, "BTSDBG_LOG STATUS: %s\n", (btsdbg_log ? "on" : "off"));

	return 0;
}

static int exynos_bts_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_bts_log_open_show, inode->i_private);
}

static ssize_t exynos_bts_log_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[64];
	ssize_t buf_size;

	int ret, status;

	buf_size = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (buf_size < 0)
		return buf_size;

	buf[buf_size] = '\0';

	ret = sscanf(buf, "%d\n", &status);
	if (ret != 1) {
		pr_err("%s: sscanf failed. We need 1 input. <ON> count=(%d)\n",
				__func__, ret);
		return -EINVAL;
	}

	if (status == 1)
		btsdbg_log = true;
	else if (status == 0)
		btsdbg_log = false;

	return buf_size;
}

static int exynos_bts_drex_open_show(struct seq_file *buf, void *d)
{
	struct bts_info *info = btsdev->bts_list;
	struct bts_stat stat;
	int ret, i = 0, idx;
	struct drex_stat drex;
	struct drex_pf_stat drex_pf;
	stat.drex = &drex;
	stat.drex_pf = &drex_pf;
	for (i = 0; i < btsdev->num_bts; i++) {
		if (info[i].ops->get_bts == NULL || info[i].type != DREX_BTS)
			continue;

		spin_lock(&btsdev->lock);

		if (info[i].pd_on) {
			if (info[i].stat[ID_DEFAULT].drex_on) {
				stat.drex_on = 1;
				stat.drex_pf_on = 0;
				memset(stat.drex, 0, sizeof(struct drex_stat));
				ret = info[i].ops->get_bts(info[i].va_base, &stat);
				if (ret) {
					pr_err("%s: failed get bts drex\n", __func__);
					goto err_get_bts;
				}
				seq_printf(buf, "[%d] %s:\n \t(offset: %u) write_config_0\t0x%.8X\n",
					i, info[i].name, offsetof(struct drex_stat, write_flush_config_0),
					drex.write_flush_config_0);
				seq_printf(buf, "\t(offset: %u) write_config_1\t0x%.8X\n",
					offsetof(struct drex_stat, write_flush_config_1), drex.brb_cutoff_con);
				seq_printf(buf, "\tdrex_timeout 0~15\n");
				for (idx = 0; idx < MAX_QOS + 1; idx++)
					seq_printf(buf, "\t\t(offset: %u) 0x%.8X\n",
						offsetof(struct drex_stat, drex_timeout[idx]),
						drex.drex_timeout[idx]);
				seq_printf(buf, "\n\tvc_timer_th 0~7\n");
				for (idx = 0; idx < VC_TIMER_TH_NR; idx++)
					seq_printf(buf, "\t\t(offset: %u) 0x%.8X\n",
						offsetof(struct drex_stat, vc_timer_th[idx]),
						drex.vc_timer_th[idx]);
				seq_printf(buf, "\n\t(offset: %u) cutoff_con\t0x%.8X\n",
					offsetof(struct drex_stat, cutoff_con), drex.cutoff_con);
				seq_printf(buf, "\t(offset: %u) brb_cutoff_con\t0x%.8X\n",
					offsetof(struct drex_stat, brb_cutoff_con), drex.brb_cutoff_con);
				seq_printf(buf, "\t(offset: %u) wdbuf_cutoff_con\t0x%.8X\n\n",
					offsetof(struct drex_stat, wdbuf_cutoff_con), drex.wdbuf_cutoff_con);
	        	}

			if (info[i].stat[ID_DEFAULT].drex_pf_on) {
				stat.drex_on = 0;
				stat.drex_pf_on = 1;
				memset(stat.drex_pf, 0, sizeof(struct drex_pf_stat));
				ret = info[i].ops->get_bts(info[i].va_base, &stat);
				if (ret) {
					pr_err("%s: failed get bts drex_pf\n", __func__);
					goto err_get_bts;
				}
				seq_printf(buf, "[%d] %s:\n", i, info[i].name);
				seq_printf(buf, "\t(offset: %u) pf_token_control\t0x%.8X\n",
					offsetof(struct drex_pf_stat, pf_token_control), drex_pf.pf_token_control);
				seq_printf(buf, "\t(offset: %u) pf_token_threshold0\t0x%.8X\n",
					offsetof(struct drex_pf_stat, pf_token_threshold0), drex_pf.pf_token_threshold0);
				seq_printf(buf, "\t(offset: %u) pf_rreq_thrt_con\t0x%.8X\n",
					offsetof(struct drex_pf_stat, pf_rreq_thrt_con),  drex_pf.pf_rreq_thrt_con);
				seq_printf(buf, "\t(offset: %u) pf_rreq_thrt_mo_p2 \t0x%.8X\n",
					offsetof(struct drex_pf_stat, pf_rreq_thrt_mo_p2), drex_pf.pf_rreq_thrt_mo_p2);
				seq_printf(buf, "\tpf_qos_timer 0~7\n");
				for (idx = 0; idx < PF_TIMER_NR; idx++)
					seq_printf(buf, "\t\t(offset: %u) 0x%.8X\n ", offsetof(struct drex_pf_stat, pf_qos_timer[idx]),
						drex_pf.pf_qos_timer[idx]);
				seq_printf(buf, "\n");
			}
		} else {
			seq_printf(buf, "[%d] %s:   \tLocal power off!\n",
			i, info[i].name);
		}


err_get_bts:
		spin_unlock(&btsdev->lock);
	}

        seq_printf(buf, "We need 5 inputs. <SCEN DREX_TYPE IP OFFSET VALUE>\n\n");

	return 0;
}

static int exynos_bts_drex_open(struct inode *inode, struct file *file)
{
        return single_open(file, exynos_bts_drex_open_show, inode->i_private);
}

static ssize_t exynos_bts_drex_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[64];
	ssize_t buf_size;

	struct bts_info *info = btsdev->bts_list;
	struct bts_stat *stat;
	int ret, scen, index, type;
	unsigned int offset, value;
	char *base;

	buf_size = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (buf_size < 0)
		return buf_size;

	buf[buf_size] = '\0';

	ret = sscanf(buf, "%d %d %d %u %x\n",
			&scen, &type, &index, &offset, &value);

	if (ret != 5) {
	        pr_err("%s: sscanf failed. We need 5 inputs."\
	                        "<SCEN DREX_TYPE IP OFFSET VALUE> count=(%d)\n",
	                        __func__, ret);
	        return -EINVAL;
	}

	if (index >= btsdev->num_bts) {
	        pr_err("%s: IP index should be in range of (0 ~ %d). input=(%d)\n",
        	        __func__, btsdev->num_bts - 1, index);
	        return -EINVAL;
	}

	if (scen >= btsdev->num_scen) {
	        pr_err("%s: SCEN index should be in range of (0 ~ %d). input=(%d)\n",
	                        __func__, btsdev->num_scen - 1, scen);
	        return -EINVAL;
	}

	if (&info[index] == NULL && &info[index].stat[scen] == NULL) {
		pr_err("%s: info is NULL\n", __func__);
		return -ENOMEM;
	}

	if(type == 0) { //drex
		if(offset >= sizeof(struct drex_stat))
			return -EINVAL;
		if(info[index].stat[scen].drex_on == 0)
			return -EINVAL;
		base = (char *)info[index].stat[scen].drex;
	} else if (type == 1) { //drex_pf
		if(offset >= sizeof(struct drex_pf_stat))
			return -EINVAL;
		if(info[index].stat[scen].drex_pf_on == 0)
			return -EINVAL;
		base = (char *)info[index].stat[scen].drex_pf;
	} else {
		pr_err("%s: DREX type should be in rage of (0 ~ 1). input=(%d)\n",
				__func__, type);
		return -EINVAL;
	}
	stat = info[index].stat;

	spin_lock(&btsdev->lock);

	*(unsigned int *)(base + offset) = value;

	if (info[index].ops->set_bts != NULL) {
	        if (scen != btsdev->top_scen)
	                goto out;

	        if (info[index].pd_on) {
	                if (info[index].ops->set_bts(info[index].va_base,
	                                        &stat[scen]))
	                        pr_warn("%s: set_drex_timeout failed. input=(%d) err=(%d)\n",
        	                                __func__, index, ret);
	        }
	} else {
		pr_err("%s: Invalid index. [%d] is %s\n", __func__, index, info[index].name);
	}

out:
	spin_unlock(&btsdev->lock);

	return buf_size;
}


static const struct file_operations debug_bts_hwstatus_fops = {
	.open		= exynos_bts_hwstatus_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_bts_scenario_fops = {
	.open		= exynos_bts_scenario_open,
	.read		= seq_read,
	.write		= exynos_bts_scenario_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_bts_qos_fops = {
	.open		= exynos_bts_qos_open,
	.read		= seq_read,
	.write		= exynos_bts_qos_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_bts_mo_fops = {
	.open		= exynos_bts_mo_open,
	.read		= seq_read,
	.write		= exynos_bts_mo_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_bts_urgent_fops = {
	.open		= exynos_bts_urgent_open,
	.read		= seq_read,
	.write		= exynos_bts_urgent_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_bts_blocking_fops = {
	.open		= exynos_bts_blocking_open,
	.read		= seq_read,
	.write		= exynos_bts_blocking_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_bts_log_fops = {
	.open		= exynos_bts_log_open,
	.read		= seq_read,
	.write		= exynos_bts_log_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debug_bts_drex_fops = {
        .open           = exynos_bts_drex_open,
        .read           = seq_read,
	.write		= exynos_bts_drex_write,
        .llseek         = seq_lseek,
        .release        = single_release,
};


int exynos_bts_debugfs_init(void)
{
	struct dentry *den;

	den = debugfs_create_dir("bts", NULL);
	if (IS_ERR(den))
		return -ENOMEM;

	debugfs_create_file("status", 0440, den, NULL,
				&debug_bts_hwstatus_fops);
	debugfs_create_file("scenario", 0440, den, NULL,
				&debug_bts_scenario_fops);
	debugfs_create_file("qos", 0440, den, NULL,
				&debug_bts_qos_fops);
	debugfs_create_file("mo", 0440, den, NULL,
				&debug_bts_mo_fops);
	debugfs_create_file("urgent", 0440, den, NULL,
				&debug_bts_urgent_fops);
	debugfs_create_file("blocking", 0440, den, NULL,
				&debug_bts_blocking_fops);
	debugfs_create_file("log", 0440, den, NULL,
				&debug_bts_log_fops);
	debugfs_create_file("drex", 0440, den, NULL,
				&debug_bts_drex_fops);

	return 0;
}

static int exynos_bts_syscore_suspend(void)
{
	return 0;
}

static void exynos_bts_syscore_resume(void)
{
	struct bts_info *info = btsdev->bts_list;
	unsigned int i = 0;

	spin_lock(&btsdev->lock);
	for (i = 0; i < btsdev->num_bts; i++) {
		if (info[i].ops->init_bts != NULL)
			info[i].ops->init_bts(info[i].va_base);
		bts_set(btsdev->top_scen, i);
	}
	spin_unlock(&btsdev->lock);
}

/* Syscore operation */
static struct syscore_ops exynos_bts_syscore_ops = {
	.suspend	= exynos_bts_syscore_suspend,
	.resume		= exynos_bts_syscore_resume,
};

/* BTS Initialize */
static int bts_initialize(struct bts_device *data)
{
	struct bts_info *info = btsdev->bts_list;
	unsigned int i = 0;
	int ret = 0;

	/* Default scenario should be enabled */
	spin_lock(&btsdev->lock);
	for (i = 0; i < btsdev->num_bts; i++) {
		if (info[i].ops->init_bts != NULL)
			info[i].ops->init_bts(info[i].va_base);
	}
	spin_unlock(&btsdev->lock);

	btsdev->top_scen = ID_DEFAULT;
	ret = bts_add_scenario(ID_DEFAULT);
	if (ret) {
		dev_err(data->dev, "failed to add scenario!\n");
		return ret;
	}

	return ret;
}

static int bts_parse_setting(struct device_node *np, struct bts_stat *stat)
{
	int tmp = 0, ret = 0;

	of_property_read_u32(np, "stat_on", &tmp);
	spin_lock(&btsdev->lock);
	stat->stat_on = tmp ? true : false;
	spin_unlock(&btsdev->lock);

	/* Initialize */
	if (stat->stat_on) {
		of_property_read_u32(np, "bypass", &tmp);
		stat->bypass = tmp ? true : false;

		if (of_property_read_u32(np, "arqos", &(stat->arqos)))
			stat->arqos = DEFAULT_QOS;
		if (of_property_read_u32(np, "awqos", &(stat->awqos)))
			stat->awqos = DEFAULT_QOS;
		if (of_property_read_u32(np, "rmo", &(stat->rmo)))
			stat->rmo = MAX_MO;
		if (of_property_read_u32(np, "wmo", &(stat->wmo)))
			stat->wmo = MAX_MO;

		of_property_read_u32(np, "qurgent_on", &(stat->qurgent_on));

		if (of_property_read_u32(np, "qurgent_th_r", &(stat->qurgent_th_r)))
			stat->qurgent_th_r = MAX_QUTH;
		if (of_property_read_u32(np, "qurgent_th_w", &(stat->qurgent_th_w)))
			stat->qurgent_th_w = MAX_QUTH;

		of_property_read_u32(np, "blocking_on", &tmp);
		stat->blocking_on = tmp ? true : false;

		if (of_property_read_u32(np, "qfull_limit_r", &(stat->qfull_limit_r)))
			stat->qfull_limit_r = MAX_MO;
		if (of_property_read_u32(np, "qfull_limit_w", &(stat->qfull_limit_w)))
			stat->qfull_limit_w = MAX_MO;
		if (of_property_read_u32(np, "qbusy_limit_r", &(stat->qbusy_limit_r)))
			stat->qbusy_limit_r = MAX_MO;
		if (of_property_read_u32(np, "qbusy_limit_w", &(stat->qbusy_limit_w)))
			stat->qbusy_limit_w = MAX_MO;
		if (of_property_read_u32(np, "qmax0_limit_r", &(stat->qmax0_limit_r)))
			stat->qmax0_limit_r = MAX_MO;
		if (of_property_read_u32(np, "qmax0_limit_w", &(stat->qmax0_limit_w)))
			stat->qmax0_limit_w = MAX_MO;
		if (of_property_read_u32(np, "qmax1_limit_r", &(stat->qmax1_limit_r)))
			stat->qmax1_limit_r = MAX_MO;
		if (of_property_read_u32(np, "qmax1_limit_w", &(stat->qmax1_limit_w)))
			stat->qmax1_limit_w = MAX_MO;

		/* Parsing vc-cfg */
		if (of_property_read_u32(np, "vc-cfg", &(stat->vc_cfg)))
			stat->vc_cfg = 0;
		tmp = 0;
		of_property_read_u32(np, "drex_on", &tmp);
		stat->drex_on = tmp ? true: false;
		if(stat->drex_on) {
			stat->drex = devm_kzalloc(btsdev->dev, sizeof(struct drex_stat), GFP_KERNEL);
			if(stat->drex == NULL)
				goto err;
			ret |= of_property_read_u32(np, "write_flush_config_0", &(stat->drex->write_flush_config_0));
			ret |= of_property_read_u32(np, "write_flush_config_1", &(stat->drex->write_flush_config_1));
			ret |= of_property_read_u32_array(np, "drex_timeout", stat->drex->drex_timeout, MAX_QOS + 1);
			ret |= of_property_read_u32_array(np, "vc_timer_th", stat->drex->vc_timer_th, VC_TIMER_TH_NR);
			ret |= of_property_read_u32(np, "cutoff_con", &(stat->drex->cutoff_con));
			ret |= of_property_read_u32(np, "brb_cutoff_con", &(stat->drex->brb_cutoff_con));
			ret |= of_property_read_u32(np, "wdbuf_cutoff_con", &(stat->drex->wdbuf_cutoff_con));
			if(ret)
				goto err;
		}
		tmp = 0;
		of_property_read_u32(np, "drex_pf_on", &tmp);
		stat->drex_pf_on = tmp ? true: false;
		if(stat->drex_pf_on) {
			stat->drex_pf = devm_kzalloc(btsdev->dev, sizeof(struct drex_pf_stat), GFP_KERNEL);
			if(stat->drex_pf == NULL)
				goto err;
			ret |= of_property_read_u32(np, "pf_token_control", &(stat->drex_pf->pf_token_control));
			ret |= of_property_read_u32(np, "pf_token_threshold0", &(stat->drex_pf->pf_token_threshold0));
			ret |= of_property_read_u32(np, "pf_rreq_thrt_con", &(stat->drex_pf->pf_rreq_thrt_con));
			ret |= of_property_read_u32(np, "pf_rreq_thrt_mo_p2", &(stat->drex_pf->pf_rreq_thrt_mo_p2));
			ret |= of_property_read_u32_array(np, "pf_qos_timer", stat->drex_pf->pf_qos_timer, PF_TIMER_NR);
			if(ret)
				goto err;
		}
	}

	return 0;
err:
	dev_err(btsdev->dev, "%s failed", __func__);
	return ret;
}

static int bts_parse_data(struct device_node *np, struct bts_device *data)
{
	struct bts_scen *scen;
	struct bts_info *info;
	struct device_node *child_np = NULL;
	struct device_node *snp = NULL;
	struct resource res;
	int i, j = 0;
	int ret = 0;

	if (of_have_populated_dt()) {
		if(of_property_read_u32_array(np, "bw_params", (u32 *)&data->bw_params,
								sizeof(struct bts_bw_params)/sizeof(int))) {
			BTSDBG_LOG(data->dev, "Unable to parse bandwidth params\n");
			ret = -EINVAL;
			goto err;
		}

		data->num_scen = of_property_count_strings(np, "list-scen");
		if (data->num_scen <= 0) {
			BTSDBG_LOG(data->dev, "There should be at least one scenario\n");
			ret = -EINVAL;
			goto err;
		}

		scen = devm_kcalloc(data->dev, data->num_scen, sizeof(struct bts_scen), GFP_KERNEL);
		if (scen == NULL) {
			ret = -ENOMEM;
			goto err;
		}

		for (i = 0; i < data->num_scen; i++) {
			scen[i].index = i;
			scen[i].status = false;
			scen[i].usage_count = 0;
			ret = of_property_read_string_index(np, "list-scen", i, &(scen[i].name));
			if (ret < 0) {
				dev_err(data->dev, "Unable to get name of bts scenarios\n");
				goto err;
			}
		}

		data->num_bts = (unsigned int)of_get_child_count(np);
		if (!data->num_bts) {
			BTSDBG_LOG(data->dev, "There should be at least one bts\n");
			ret = -EINVAL;
			goto err;
		}

		info = devm_kcalloc(data->dev, data->num_bts, sizeof(struct bts_info), GFP_KERNEL);
		if (info == NULL) {
			ret = -ENOMEM;
			goto err;
		}

		i = 0;

		for_each_child_of_node(np, child_np) {
			/* Parsing scenario data */
			info[i].stat = devm_kcalloc(data->dev, data->num_scen, sizeof(struct bts_stat), GFP_KERNEL);
			if (info[i].stat == NULL) {
				ret = -ENOMEM;
				goto err;
			}

			/* IOREMAP physical address */
			ret = of_address_to_resource(child_np, 0, &res);
			if (ret) {
				dev_err(data->dev, "failed to get address_to_resource\n");
				if (ret == -EINVAL)
					ret = 0;
			} else {
				info[i].va_base = devm_ioremap_resource(data->dev, &res);
				if (IS_ERR(info[i].va_base)) {
					dev_err(data->dev, "failed to ioremap register\n");
						ret = -ENOMEM;
				}
				ret = of_address_to_resource(child_np, 1, &res);
				if (!ret) {
					info[i].stat->qos_va_base = devm_ioremap_resource(data->dev, &res);
					if (IS_ERR(info[i].stat->qos_va_base)) {
						dev_err(data->dev, "failed to ioremap register\n");
						ret = -ENOMEM;
					}
				}
			}

			info[i].name = child_np->name;
			info[i].status = of_device_is_available(child_np);

			/* Parsing bts-type */
			if (of_property_read_u32(child_np, "bts-type", &(info[i].type))) {
				dev_warn(data->dev, "failed to get bts-type\n");
				ret = -EEXIST;
				goto err;
			}

			/* Register operation function */
			ret = register_btsops(&info[i]);
			if (ret)
				goto err;

			/* Parsing local power domain information */
			if (of_property_read_u32(child_np, "cal-pdid", &(info[i].pd_id))) {
				info[i].pd_id = 0;
				info[i].pd_on = true;
			} else {
				info[i].pd_on = cal_pd_status(info[i].pd_id) ? true : false;
			}

			if (!of_get_child_count(child_np)) {
				info[i].stat = NULL;
				i++;
				continue;
			}

			spin_lock(&btsdev->lock);
			for (j = 0; j < data->num_scen; j++)
				info[i].stat[j].stat_on = 0;
			spin_unlock(&btsdev->lock);

			for_each_child_of_node(child_np, snp) {
				for (j = 0; j < data->num_scen; j++) {
					if (strcmp(snp->name, scen[j].name))
						continue;
					bts_parse_setting(snp, &(info[i].stat[j]));
				}
			}

			i++;
		}

		data->bts_bw = devm_kcalloc(data->dev, data->num_bts, sizeof(struct bts_bw), GFP_KERNEL);
		if (data->bts_bw == NULL) {
			ret = -ENOMEM;
			goto err;
		}

		data->bts_list = info;
		data->scen_list = scen;

		exynos_cal_pd_bts_sync = bts_pd_sync;
	} else {
		dev_err(data->dev, "Invalid device tree node!\n");
	}

err:
	return ret;
}

static int bts_probe(struct platform_device *pdev)
{
	int ret = 0;

	btsdev = devm_kmalloc(&pdev->dev, sizeof(struct bts_device), GFP_KERNEL);
	if (btsdev == NULL)
		return -ENOMEM;

	btsdev->dev = &pdev->dev;

	if (strcmp(pdev->name, "exynos-bts") == 0) {
		spin_lock_init(&btsdev->lock);
		ret = bts_parse_data(btsdev->dev->of_node, btsdev);
		if (ret) {
			dev_err(btsdev->dev, "failed to parse data (err=%d)\n", ret);
			devm_kfree(btsdev->dev, btsdev);
			return ret;
		}
		mutex_init(&btsdev->mutex_lock);
		INIT_LIST_HEAD(&btsdev->scen_node);

		ret = bts_initialize(btsdev);
		if (ret) {
			dev_err(btsdev->dev, "failed to initialize (err=%d)\n", ret);
			devm_kfree(btsdev->dev, btsdev);
			return ret;
		}
	} else {
		dev_err(btsdev->dev, "failed to get bts data from device tree\n");
	}

	platform_set_drvdata(pdev, btsdev);

#if defined(CONFIG_EXYNOS_PM_QOS) || defined(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_add_request(&exynos_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&exynos_int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
#endif

#if defined(CONFIG_DEBUG_FS) || defined(CONFIG_DEBUG_FS_MODULE)
	ret = exynos_bts_debugfs_init();
	if (ret)
		dev_err(btsdev->dev, "exynos_bts_debugfs_init failed\n");
#endif

	register_syscore_ops(&exynos_bts_syscore_ops);

	pr_info("%s successfully done.\n", __func__);

	return ret;
}

static int bts_remove(struct platform_device *pdev)
{
	devm_kfree(&pdev->dev, btsdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

/* Device tree compatible information */
static const struct of_device_id exynos_bts_match[] = {
	{
		.compatible = "samsung,exynos-bts",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_bts_match);

static struct platform_driver bts_pdrv = {
	.probe			= bts_probe,
	.remove			= bts_remove,
	.driver			= {
		.name		= BTS_PDEV_NAME,
		.of_match_table	= exynos_bts_match,
	},
};

static int exynos_bts_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&bts_pdrv);
	if (ret) {
		pr_err("Platform driver registration failed (err=%d)\n", ret);
		return ret;
	}

	pr_info("%s successfully done.\n", __func__);

	return 0;
}
arch_initcall(exynos_bts_init);

MODULE_LICENSE("GPL");
