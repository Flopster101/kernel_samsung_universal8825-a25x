/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * IPs Traffic Monitor(ITMON) Driver for Samsung Exynos SOC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/pm_domain.h>
#include <linux/of_reserved_mem.h>
#include <linux/sched/clock.h>
#include <soc/samsung/exynos-itmon.h>
#include <soc/samsung/debug-snapshot.h>
#include "exynos-itmon-local.h"
#include <linux/sec_debug.h>

static struct itmon_policy err_policy[] = {
	[TMOUT]		= {"err_tmout",		0, 0, 0, false},
	[PRTCHKER]	= {"err_prtchker",	0, 0, 0, false},
	[DECERR]	= {"err_decerr",	0, 0, 0, false},
	[SLVERR]	= {"err_slverr",	0, 0, 0, false},
	[FATAL]		= {"err_fatal",		0, 0, 0, false},
};

static const char * const itmon_dpm_action[] = {
	GO_DEFAULT,
	GO_PANIC,
	GO_WATCHDOG,
	GO_S2D,
	GO_ARRAYDUMP,
	GO_SCANDUMP,
	GO_HALT,
};

static const char *itmon_pathtype[] = {
	"DATA Path transaction",
	"Configuration(SFR) Path transaction",
	"BUS Path transaction",
	"DATA Path transaction",
};

/* Error Code Description */
const static char *itmon_errcode[] = {
	"Error Detect by the Slave(SLVERR)",
	"Decode error(DECERR)",
	"Unsupported transaction error",
	"Power Down access error",
	"Unsupported transaction",
	"Unsupported transaction",
	"Timeout error - response timeout in timeout value",
	"Invalid errorcode",
};

const static char *itmon_node_string[] = {
	"M_NODE",
	"TAXI_S_NODE",
	"TAXI_M_NODE",
	"S_NODE",
};

const static char *itmon_cpu_node_string[] = {
	"CLUSTER0_P",
	"M_CPU",
	"SCI_IRPM",
	"SCI_CCM",
	"CCI",
};

const static char *itmon_mid_mnode_string[] = {
	"M_PERI",
	"NOCL0_DP_M",
};

const static char *itmon_mid_snode_string[] = {
	"NOCL0_DP",
	"PERI_SFR",
};

const static unsigned int itmon_invalid_addr[] = {
	0x03000000,
	0x04000000,
};

static struct itmon_dev *g_itmon;

/* declare notifier_list */
static ATOMIC_NOTIFIER_HEAD(itmon_notifier_list);

static const struct of_device_id itmon_dt_match[] = {
	{.compatible = "samsung,exynos-itmon",
	 .data = NULL,},
	{},
};
MODULE_DEVICE_TABLE(of, itmon_dt_match);

static struct itmon_nodeinfo *itmon_get_nodeinfo_by_group(struct itmon_dev *itmon,
							  struct itmon_nodegroup *group,
							  const char *name)
{
	struct itmon_nodeinfo *node = group->nodeinfo;
	int i;

	for (i = 0; i < group->nodesize; i++)
		if (!strncmp(node[i].name, name, strlen(name)))
			return &node[i];

	return NULL;
}

static struct itmon_nodeinfo *itmon_get_nodeinfo(struct itmon_dev *itmon,
						 struct itmon_nodegroup *group,
						 const char *name)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *node = NULL;
	int i;

	if (!name)
		return NULL;

	if (group) {
		return itmon_get_nodeinfo_by_group(itmon, group, name);
	} else {
		for (i = 0; i < pdata->num_nodegroup; i++) {
			group = &pdata->nodegroup[i];
			node = itmon_get_nodeinfo_by_group(itmon, group, name);
			if (node)
				return node;
		}
	}

	return NULL;
}

static struct itmon_rpathinfo *itmon_get_rpathinfo(struct itmon_dev *itmon,
						   unsigned int id,
						   char *dest_name)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i;

	if (!dest_name)
		return NULL;

	for (i = 0; i < pdata->num_rpathinfo; i++) {
		if (pdata->rpathinfo[i].id == (id & pdata->rpathinfo[i].bits)) {
			if (!strncmp(pdata->rpathinfo[i].dest_name, dest_name,
				  strlen(pdata->rpathinfo[i].dest_name)))
				return &pdata->rpathinfo[i];
		}
	}

	return NULL;
}

static struct itmon_masterinfo *itmon_get_masterinfo(struct itmon_dev *itmon,
						     char *port_name,
						     unsigned int user)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i;

	if (!port_name)
		return NULL;

	for (i = 0; i < pdata->num_masterinfo; i++) {
		if ((user & pdata->masterinfo[i].bits) ==
			pdata->masterinfo[i].user) {
			if (!strncmp(pdata->masterinfo[i].port_name,
				port_name, strlen(port_name)))
				return &pdata->masterinfo[i];
	}
}

	return NULL;
}

static void itmon_enable_addr_detect(struct itmon_dev *itmon,
			             struct itmon_nodeinfo *node,
			             bool enabled)
{
	/* This feature is only for M_NODE */
	unsigned int tmp, val;
	unsigned int offset = OFFSET_PRT_CHK;

	val = __raw_readl(node->regs + offset + REG_PRT_CHK_CTL);
	val |= INTEND_ACCESS_INT_ENABLE;
	__raw_writel(val, node->regs + offset + REG_PRT_CHK_CTL);

	val = ((unsigned int)INTEND_ADDR_START & U32_MAX);
	__raw_writel(val, node->regs + offset + REG_PRT_CHK_START_ADDR_LOW);

	val = (unsigned int)(((unsigned long)INTEND_ADDR_START >> 32) & U16_MAX);
	__raw_writel(val, node->regs + offset
			+ REG_PRT_CHK_START_END_ADDR_UPPER);

	val = ((unsigned int)INTEND_ADDR_END & 0xFFFFFFFF);
	__raw_writel(val, node->regs + offset
			+ REG_PRT_CHK_END_ADDR_LOW);
	val = ((unsigned int)(((unsigned long)INTEND_ADDR_END >> 32)
							& 0XFFFF0000) << 16);
	tmp = readl(node->regs + offset + REG_PRT_CHK_START_END_ADDR_UPPER);
	__raw_writel(tmp | val, node->regs + offset
			+ REG_PRT_CHK_START_END_ADDR_UPPER);

	dev_dbg(itmon->dev, "ITMON - %s addr detect %sabled\n",
		node->name, enabled == true ? "en" : "dis");
}

static void itmon_enable_prt_chk(struct itmon_dev *itmon,
			       struct itmon_nodeinfo *node,
			       bool enabled)
{
	unsigned int offset = OFFSET_PRT_CHK;
	unsigned int val = 0;

	if (enabled)
		val = RD_RESP_INT_ENABLE | WR_RESP_INT_ENABLE |
		     ARLEN_RLAST_INT_ENABLE | AWLEN_WLAST_INT_ENABLE;

	writel(val, node->regs + offset + REG_PRT_CHK_CTL);

	dev_dbg(itmon->dev, "ITMON - %s hw_assert %sabled\n",
		node->name, enabled == true ? "en" : "dis");
}

static void itmon_enable_err_report(struct itmon_dev *itmon,
			       struct itmon_nodeinfo *node,
			       bool enabled)
{
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned int offset = OFFSET_REQ_R;

	if (!pdata->probed || !node->retention)
		writel(1, node->regs + offset + REG_INT_CLR);
	/* enable interrupt */
	writel(enabled, node->regs + offset + REG_INT_MASK);

	/* clear previous interrupt of req_write */
	offset = OFFSET_REQ_W;
	if (!pdata->probed || !node->retention)
		writel(1, node->regs + offset + REG_INT_CLR);
	/* enable interrupt */
	writel(enabled, node->regs + offset + REG_INT_MASK);

	/* clear previous interrupt of response_read */
	offset = OFFSET_RESP_R;
	if (!pdata->probed || !node->retention)
		writel(1, node->regs + offset + REG_INT_CLR);
	/* enable interrupt */
	writel(enabled, node->regs + offset + REG_INT_MASK);

	/* clear previous interrupt of response_write */
	offset = OFFSET_RESP_W;
	if (!pdata->probed || !node->retention)
		writel(1, node->regs + offset + REG_INT_CLR);
	/* enable interrupt */
	writel(enabled, node->regs + offset + REG_INT_MASK);

	dev_dbg(itmon->dev, "ITMON - %s error reporting %sabled\n",
		node->name, enabled == true ? "en" : "dis");
}

static void itmon_enable_timeout(struct itmon_dev *itmon,
			       struct itmon_nodeinfo *node,
			       bool enabled)
{
	unsigned int offset = OFFSET_TMOUT_REG;

	/* Enable Timeout setting */
	writel(enabled, node->regs + offset + REG_DBG_CTL);

	/* set tmout interval value */
	writel(node->time_val, node->regs + offset + REG_TMOUT_INIT_VAL);

	/* Enable freezing */
	writel(node->tmout_frz_enabled, node->regs + offset + REG_TMOUT_FRZ_EN);

	dev_dbg(itmon->dev, "ITMON - %s timeout %sabled\n",
		node->name, enabled == true ? "en" : "dis");
}

static void itmon_enable_nodepolicy(struct itmon_dev *itmon,
				   struct itmon_nodeinfo *node)
{
	struct itmon_nodepolicy *policy = &node->policy;
	struct itmon_nodegroup *group = node->group;

	if (!policy->chk_set)
		return;

	if (group->pd_support && !group->pd_status) {
		dev_err(g_itmon->dev, "%s group - %s node NOT pd on\n",
			group, node);
	}

	if (policy->chk_errrpt)
		itmon_enable_err_report(itmon, node, policy->en_errrpt);
	if (policy->chk_tmout || policy->chk_tmout_val)
		itmon_enable_timeout(itmon, node, policy->en_tmout);
	if (policy->chk_prtchk)
		itmon_enable_prt_chk(itmon, node, policy->en_prtchk);
}


static void itmon_set_nodepolicy(struct itmon_dev *itmon,
				   struct itmon_nodeinfo *node,
				   struct itmon_nodepolicy policy,
				   bool now)
{
	node->policy.enabled = policy.enabled;

	if (policy.chk_errrpt)
		node->err_enabled = policy.en_errrpt;
	if (policy.chk_tmout)
		node->tmout_enabled = policy.en_tmout;
	if (policy.chk_prtchk)
		node->prt_chk_enabled = policy.en_prtchk;
	if (policy.chk_tmout_val)
		node->time_val = policy.en_tmout_val;
	if (policy.chk_freeze)
		node->tmout_frz_enabled = policy.en_freeze;

	if (policy.chk_errrpt || policy.chk_tmout ||
		policy.chk_prtchk || policy.chk_tmout_val ||
		policy.chk_errrpt || policy.chk_prtchk_job ||
		policy.chk_tmout || policy.chk_freeze ||
		policy.chk_decerr_job || policy.chk_slverr_job || policy.chk_tmout_job)
		node->policy.chk_set = 1;

	if (now)
		itmon_enable_nodepolicy(itmon, node);
}

int itmon_enable_by_name(const char *name, bool enabled)
{
	struct itmon_nodeinfo *node;
	struct itmon_nodegroup *group;

	if (!g_itmon)
		return -ENODEV;

	node = itmon_get_nodeinfo(g_itmon, NULL, name);
	if (!node) {
		dev_err(g_itmon->dev, "%s node is not found\n", name);
		return -ENODEV;
	}

	group = node->group;
	if (!group) {
		dev_err(g_itmon->dev, "%s node's group is  not found\n", name);
		return -ENODEV;
	}

	if (group->pd_support && !group->pd_status) {
		dev_err(g_itmon->dev, "%s group - %s node NOT pd on\n",
			group, node);
		return -EIO;
	}

	itmon_enable_err_report(g_itmon, node, enabled);
	itmon_enable_prt_chk(g_itmon, node, enabled);
	if (node->type == S_NODE)
		itmon_enable_timeout(g_itmon, node, enabled);

	return 0;
}
EXPORT_SYMBOL(itmon_enable_by_name);

int itmon_set_nodepolicy_by_name(const char *name, u64 enabled, bool now)
{
	struct itmon_nodeinfo *node;
	struct itmon_nodepolicy policy;

	if (!g_itmon)
		return -ENODEV;

	node = itmon_get_nodeinfo(g_itmon, NULL, name);
	if (!node)
		return -ENODEV;

	policy.enabled = enabled;
	itmon_set_nodepolicy(g_itmon, node, policy, now);

	return 0;
}
EXPORT_SYMBOL(itmon_set_nodepolicy_by_name);

static void itmon_init_by_group(struct itmon_dev *itmon,
				struct itmon_nodegroup *group,
				bool enabled)
{
	struct itmon_nodeinfo *node = group->nodeinfo;
	int i;

	dev_dbg(itmon->dev,
		"%s: group:%s enabled:%x\n", __func__, group->name, enabled);

	for (i = 0; i < group->nodesize; i++) {
		if (enabled) {
			if (node[i].err_enabled)
				itmon_enable_err_report(itmon, &node[i], enabled);
			if (node[i].prt_chk_enabled)
				itmon_enable_prt_chk(itmon, &node[i], enabled);
		} else {
			/* as default enable */
			itmon_enable_err_report(itmon, &node[i], enabled);
			itmon_enable_prt_chk(itmon, &node[i], enabled);
		}
		if (node[i].type == S_NODE && node[i].tmout_enabled)
			itmon_enable_timeout(itmon, &node[i], enabled);
		if (node[i].addr_detect_enabled)
			itmon_enable_addr_detect(itmon, &node[i], enabled);
	}
}

static void itmon_init(struct itmon_dev *itmon, bool enabled)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodegroup *group;
	int i;

	for (i = 0; i < pdata->num_nodegroup; i++) {
		group = &pdata->nodegroup[i];
		if (group->pd_support && !group->pd_status)
			continue;
		itmon_init_by_group(itmon, group, enabled);
	}

	pdata->enabled = enabled;
	dev_info(itmon->dev, "itmon %sabled\n", pdata->enabled ? "en" : "dis");
}

void itmon_pd_sync(const char *pd_name, bool enabled)
{
	struct itmon_dev *itmon = g_itmon;
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodegroup *group;
	int i;

	if (!pdata->probed)
		return;

	for (i = 0; i < pdata->num_nodegroup; i++) {
		group = &pdata->nodegroup[i];
		if (group->pd_support &&
			!strncmp(pd_name, group->pd_name, strlen(pd_name))) {
			dev_dbg(itmon->dev,
				"%s: pd_name:%s enabled:%x, pd_status:%x\n",
				__func__, pd_name, enabled, group->pd_status);
			if (group->pd_status != enabled) {
				if (enabled)
					itmon_init_by_group(itmon, group, enabled);
				group->pd_status = enabled;
			}
			return;
		}
	}
}
EXPORT_SYMBOL(itmon_pd_sync);

void itmon_enable(bool enabled)
{
	if (g_itmon)
		itmon_init(g_itmon, enabled);
}
EXPORT_SYMBOL(itmon_enable);

int itmon_get_dpm_policy(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i, policy = -1;

	for (i = 0; i < TYPE_MAX; i++) {
		if (pdata->policy[i].error && pdata->policy[i].policy > policy)
			policy = pdata->policy[i].policy;
	}

	return policy;
}

static void itmon_dump_dpm_policy(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i;

	pr_err("\t\tError Policy Information\n\n\t\t> NAME           |Error    |Policy-def |Policy-now\n");
	for (i = 0; i < TYPE_MAX; i++) {
		pr_info("\t\t> %15s%10s%10s%10s\n",
			pdata->policy[i].name,
			pdata->policy[i].error == true ? "TRUE" : "FALSE",
			itmon_dpm_action[pdata->policy[i].policy_def],
			itmon_dpm_action[pdata->policy[i].policy]);
	}
	pr_err("\n");
}

static void itmon_do_dpm_policy(struct itmon_dev *itmon, bool clear)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i, sel, policy = -1;

	for (i = 0; i < TYPE_MAX; i++) {
		if (pdata->policy[i].error && pdata->policy[i].policy > policy) {
			policy = pdata->policy[i].policy;
			sel = i;
		}
		if (clear) {
			pdata->policy[i].policy = pdata->policy[i].policy_def;
			pdata->policy[i].prio = 0;
		}
	}

	if (policy >= GO_DEFAULT_ID) {
		dev_err(itmon->dev, "%s: %s: policy:%s\n", __func__,
					pdata->policy[sel].name, itmon_dpm_action[policy]);
		dbg_snapshot_do_dpm_policy(policy);
	}
}

static void itmon_reflect_policy_by_notifier(struct itmon_dev *itmon,
						struct itmon_traceinfo *info,
						int ret)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_policy *pl_policy;
	int sig, num;

	if ((ret & ITMON_NOTIFY_MASK) != ITMON_NOTIFY_MASK)
		return;

	sig = ret & ~ITMON_NOTIFY_MASK;

	switch (info->errcode) {
	case ERRCODE_DECERR:
		num = DECERR;
		break;
	case ERRCODE_SLVERR:
		num = SLVERR;
		break;
	case ERRCODE_TMOUT:
		num = TMOUT;
		break;
	default:
		dev_warn(itmon->dev, "%s: Invalid error code(%u)\n",
					__func__, info->errcode);
		return;
	}
	pl_policy = &pdata->policy[num];

	pr_err("\nPolicy Changes: %d -> %d in %s by notifier-call\n\n",
			pl_policy->policy, sig, itmon_errcode[info->errcode]);

	pl_policy->policy = sig;
	pl_policy->prio = CUSTOM_MAX_PRIO;
}

static int itmon_notifier_handler(struct itmon_dev *itmon,
					   struct itmon_traceinfo *info,
					   unsigned int trans_type)
{
	int ret = 0;

	/* After treatment by port */
	if (!info->port || strlen(info->port) < 1)
		return -1;

	itmon->notifier_info.port = info->port;
	itmon->notifier_info.master = info->master;
	itmon->notifier_info.dest = info->dest;
	itmon->notifier_info.read = info->read;
	itmon->notifier_info.target_addr = info->target_addr;
	itmon->notifier_info.errcode = info->errcode;
	itmon->notifier_info.onoff = info->onoff;

	pr_err("----------------------------------------------------------------------------------\n\t\t+ITMON Notifier Call Information\n");

	/* call notifier_call_chain of itmon */
	ret = atomic_notifier_call_chain(&itmon_notifier_list,
				0, &itmon->notifier_info);

	pr_err("\t\t-ITMON Notifier Call Information\n----------------------------------------------------------------------------------\n");

	return ret;
}

static void itmon_reflect_policy_by_node(struct itmon_dev *itmon,
					      struct itmon_nodeinfo *node,
					      int job)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_policy *pl_policy = &pdata->policy[job];
	struct itmon_nodepolicy *nd_policy = &node->policy;
	u64 chk_job = 0, en_job = 0, en_mask = 0;

	pl_policy = &pdata->policy[job];
	pl_policy->error = true;
	nd_policy = &node->policy;

	switch (job) {
	case TMOUT:
		chk_job = nd_policy->chk_tmout_job;
		en_job = nd_policy->en_tmout_job;
		en_mask = nd_policy->en_irq_mask;
		if (nd_policy->en_irq_mask && node->type == S_NODE)
			itmon_enable_timeout(itmon, node, false);
		break;
	case PRTCHKER:
		chk_job = nd_policy->chk_prtchk_job;
		en_job = nd_policy->en_prtchk_job;
		if (nd_policy->en_irq_mask)
			itmon_enable_prt_chk(itmon, node, false);
		break;
	case DECERR:
		chk_job = nd_policy->chk_decerr_job;
		en_job = nd_policy->en_decerr_job;
		if (nd_policy->en_irq_mask)
			itmon_enable_err_report(itmon, node, false);
		break;
	case SLVERR:
		chk_job = nd_policy->chk_slverr_job;
		en_job = nd_policy->en_slverr_job;
		if (nd_policy->en_irq_mask)
			itmon_enable_err_report(itmon, node, false);
		break;
	}

	if (chk_job && pl_policy->prio <= nd_policy->prio) {
		pl_policy->policy = en_job;
		pl_policy->prio = nd_policy->prio;
	}
}

static void itmon_reflect_policy(struct itmon_dev *itmon,
				    struct itmon_traceinfo *info)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_policy *pl_policy = NULL;
	struct itmon_nodepolicy nd_policy1 = {0,}, nd_policy2 = {0,}, nd_policy = {0,};
	u64 chk_job1 = 0, chk_job2 = 0, en_job1 = 0, en_job2 = 0, en_job = 0;

	if (info->s_node) {
		nd_policy1.enabled = info->s_node->policy.enabled;
		if (nd_policy1.en_irq_mask) {
			if (info->errcode == ERRCODE_TMOUT)
				itmon_enable_timeout(itmon, info->s_node, false);
			if (info->errcode == ERRCODE_DECERR ||
				info->errcode == ERRCODE_SLVERR)
				itmon_enable_err_report(itmon, info->s_node, false);
		}
	}
	if (info->m_node) {
		nd_policy2.enabled = info->m_node->policy.enabled;
		if (nd_policy2.en_irq_mask) {
			if (info->errcode == ERRCODE_DECERR ||
				info->errcode == ERRCODE_SLVERR)
				itmon_enable_err_report(itmon, info->m_node, false);
		}
	}

	switch (info->errcode) {
	case ERRCODE_DECERR:
		pl_policy = &pdata->policy[DECERR];
		pl_policy->error = true;
		if (nd_policy1.chk_set) {
			chk_job1 = nd_policy1.chk_decerr_job;
			en_job1 = nd_policy1.en_decerr_job;
		}
		if (nd_policy2.chk_set) {
			chk_job2 = nd_policy2.chk_decerr_job;
			en_job2 = nd_policy2.en_decerr_job;
		}
		break;
	case ERRCODE_SLVERR:
		pl_policy = &pdata->policy[SLVERR];
		pl_policy->error = true;
		if (nd_policy1.chk_set) {
			chk_job1 = nd_policy1.chk_slverr_job;
			en_job1 = nd_policy1.en_slverr_job;
		}
		if (nd_policy2.chk_set) {
			chk_job2 = nd_policy2.chk_slverr_job;
			en_job2 = nd_policy2.en_slverr_job;
		}
		break;
	case ERRCODE_TMOUT:
		pl_policy = &pdata->policy[TMOUT];
		pl_policy->error = true;
		if (nd_policy1.chk_set) {
			chk_job1 = nd_policy1.chk_tmout_job;
			en_job1 = nd_policy1.en_tmout_job;
		}
		if (nd_policy2.chk_set) {
			chk_job2 = nd_policy2.chk_tmout_job;
			en_job2 = nd_policy2.en_tmout_job;
		}
		break;
	default:
		dev_warn(itmon->dev, "%s: Invalid error code(%u)\n",
					__func__, info->errcode);
		return;
	}

	if (chk_job1 && chk_job2) {
		if (nd_policy1.prio >= nd_policy2.prio) {
			nd_policy.enabled = nd_policy1.enabled;
			en_job = en_job1;
		} else {
			nd_policy.enabled = nd_policy2.enabled;
			en_job = en_job2;
		}
	} else if (chk_job1) {
		nd_policy = nd_policy1;
		en_job = en_job1;
	} else if (chk_job2) {
		nd_policy = nd_policy2;
		en_job = en_job2;
	} /* nd_policy = NULL */

	if (nd_policy.chk_set && pl_policy->prio <= nd_policy.prio) {
		pl_policy->policy = en_job;
		pl_policy->prio = nd_policy.prio;
	}
}

static void itmon_post_handler(struct itmon_dev *itmon, bool err)
{
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned long ts = pdata->last_time;
	unsigned long rem_nsec = do_div(ts, 1000000000);
	unsigned long cur_time = local_clock();
	unsigned long delta;

	delta = pdata->last_time == 0 ? 0 : cur_time - pdata->last_time;

	dev_err(itmon->dev, "Before ITMON: [%5lu.%06lu], delta: %lu, last_errcnt: %d\n",
		(unsigned long)ts, rem_nsec / 1000, delta, pdata->last_errcnt);

	if (err)
		itmon_do_dpm_policy(itmon, true);

	/* delta < 1s */
	if (delta > 0 && delta < 1000000000UL) {
		pdata->last_errcnt++;
		if (pdata->last_errcnt > ERR_THRESHOLD)
			dbg_snapshot_do_dpm_policy(GO_S2D_ID);
	} else {
		pdata->last_errcnt = 0;
	}

	pdata->last_time = cur_time;
}

static void itmon_report_timeout(struct itmon_dev *itmon,
				struct itmon_nodeinfo *node,
				unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned int id, payload0, payload1, payload2, payload3, payload4, payload5;
	unsigned int axid, valid, timeout;
	unsigned long addr, user;
	struct itmon_nodegroup *group = NULL;
	int i, num = (trans_type == TRANS_TYPE_READ ? SZ_128 : SZ_64);
	int rw_offset = (trans_type == TRANS_TYPE_READ ? 0 : REG_TMOUT_BUF_WR_OFFSET);
	u32 axburst, axprot, axlen, axsize, domain;

	if (!node)
		return;

	group = node->group;

	pr_err("\nITMON Report (%s) Timeout Error Occurred : Master --> %s\n\n",
			trans_type == TRANS_TYPE_READ ? "READ" : "WRITE", node->name);
	if (pdata->big_bus)
		pr_err("> NUM|    MASTER BLOCK|          MASTER|VALID|TMOUT|    ID|DOMAIN|SIZE|PROT|         ADDRESS|PAYLOAD0|PAYLOAD1|PAYLOAD2|PAYLOAD3|PAYLOAD4|PAYLOAD5");
	else
		pr_err("> NUM|    MASTER BLOCK|          MASTER|VALID|TMOUT|    ID|DOMAIN|SIZE|PROT|         ADDRESS|PAYLOAD0|PAYLOAD1|PAYLOAD2|PAYLOAD3");

	for (i = 0; i < num; i++) {
		struct itmon_rpathinfo *port = NULL;
		struct itmon_masterinfo *master = NULL;

		writel(i, node->regs + OFFSET_TMOUT_REG + REG_TMOUT_BUF_POINT_ADDR + rw_offset);
		id = readl(node->regs + OFFSET_TMOUT_REG + REG_TMOUT_BUF_ID + rw_offset);
		payload0 = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_0 + rw_offset);
		if (pdata->big_bus) {
			payload1 = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_1 + rw_offset);
			payload5 = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_5 + rw_offset);
		}
		payload2 = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_2 + rw_offset);	/* mid/small: payload 1 */
		payload3 = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_3 + rw_offset);	/* mid/small: payload 2 */
		payload4 = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_4 + rw_offset);	/* mid/small: payload 3 */

		if (pdata->big_bus) {
			timeout = (payload0 & (unsigned int)(GENMASK(7, 4))) >> 4;
			user = ((unsigned long)payload2 << 32ULL) | payload1;
			addr = (((unsigned long)payload3 & GENMASK(15, 0)) << 32ULL) | payload4;
		} else {
			timeout = (payload0 & (unsigned int)(GENMASK(19, 16))) >> 16;
			user = (payload0 & (unsigned int)(GENMASK(15, 8))) >> 8;
			addr = (((unsigned long)payload2 & GENMASK(15, 0)) << 32ULL) | payload3;
			payload5 = payload4;
		}
		axid = id & GENMASK(5, 0); /* ID[5:0] 6bit : R-PATH */
		valid = payload0 & BIT(0); /* PAYLOAD[0] : Valid or Not valid */
		axburst = BIT_AXBURST(payload5);
		axprot = BIT_AXPROT(payload5);
		axlen = (payload5 & (unsigned int)GENMASK(7, 4)) >> 4;
		axsize = (payload5 & (unsigned int)GENMASK(18, 16)) >> 16;
		domain = (payload5 & BIT(19));
		port = itmon_get_rpathinfo(itmon, axid, node->name);
		if (port)
			master = itmon_get_masterinfo(itmon, port->port_name, user);

		if (pdata->big_bus)
			pr_err("> %03d|%16s|%16s|%5u|%5x|%06x|%6s|%4u|%4s|%016zx|%08x|%08x|%08x|%08x|%08x|%08x\n",
				i, port ? port->port_name : NO_NAME,
				master ? master->master_name : NO_NAME,
				valid, timeout, id, domain ? "SHARE" : "NSHARE",
				int_pow(2, axsize * (axlen + 1)),
				axprot & BIT(1) ? "NSEC" : "SEC",
				addr, payload0, payload1, payload2, payload3, payload4, payload5);
		else
			pr_err("> %03d|%16s|%16s|%5u|%5x|%06x|%6s|%4u|%4s|%016zx|%08x|%08x|%08x|%08x\n",
				i, port ? port->port_name : NO_NAME,
				master ? master->master_name : NO_NAME,
				valid, timeout, id, domain ? "SHARE" : "NSHARE",
				int_pow(2, axsize * (axlen + 1)),
				axprot & BIT(1) ? "NSEC" : "SEC",
				addr, payload0, payload2, payload3, payload4);
	}
}

static void itmon_report_prt_chk_rawdata(struct itmon_dev *itmon,
					 struct itmon_tracedata *data)
{
	struct itmon_nodeinfo *node = data->node;
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	char temp_buf[SZ_128];
#endif

	/* Output Raw register information */
	dev_err(itmon->dev,
		"\n-----------------------------------------"
		"-----------------------------------------\n"
		"\t\tProtocol Checker Raw Register Information"
		"(ITMON information)\n\n");
	dev_err(itmon->dev,
		"\t\t> %s(%s, 0x%08llX)\n"
		"\t\t> REG(0x100~0x10C)      : 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
		node->name, itmon_node_string[node->type],
		node->phy_regs,
		data->dbg_mo_cnt,
		data->prt_chk_ctl,
		data->prt_chk_info,
		data->prt_chk_int_id);
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	snprintf(temp_buf, SZ_128, "%s/ %s/ 0x%08X/ %s/ 0x%08X, 0x%08X, 0x%08X, 0x%08X",
		"Protocol Error", node->name, node->phy_regs,
		itmon_node_string[node->type],
		data->dbg_mo_cnt,
		data->prt_chk_ctl,
		data->prt_chk_info,
		data->prt_chk_int_id);
	secdbg_exin_set_busmon(temp_buf);
#endif
}

static void itmon_report_rawdata(struct itmon_dev *itmon,
				 struct itmon_tracedata *data)
{
	struct itmon_nodeinfo *node = data->node;

	if (BIT_PRT_CHK_ERR_OCCURRED(data->prt_chk_info) &&
		(data->prt_chk_info & GENMASK(8, 1))) {
		itmon_report_prt_chk_rawdata(itmon, data);
		return;
	}

	/* Output Raw register information */
	pr_err("\t\tRaw Register Information ---------------------------------------------------\n"
		"\t\t> %s(%s, 0x%08llX)\n"
		"\t\t> REG(0x08~0x18,0x80)   : 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X\n"
		"\t\t> REG(0x100~0x10C)      : 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
		node->name, itmon_node_string[node->type],
		node->phy_regs + data->offset,
		data->int_info,
		data->ext_info_0,
		data->ext_info_1,
		data->ext_info_2,
		data->ext_user,
		data->dbg_mo_cnt,
		data->prt_chk_ctl,
		data->prt_chk_info,
		data->prt_chk_int_id);
}

static void itmon_report_traceinfo(struct itmon_dev *itmon,
				   struct itmon_traceinfo *info,
				   unsigned int trans_type)
{
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	char temp_buf[SZ_128];
#endif
	/* SEC DEBUG FEATURE */
	int acon = (itmon_get_dpm_policy(itmon) > 0) ? 1 : 0;
	/* SEC DEBUG FEATURE */

	if (!info->dirty)
		return;

	pr_auto_on(acon, ASL3,
		"\n-----------------------------------------"
		"-----------------------------------------\n"
		"\t\tTransaction Information\n\n"
		"\t\t> Master         : %s %s\n"
		"\t\t> Target         : %s\n"
		"\t\t> Target Address : 0x%llX %s\n"
		"\t\t> Type           : %s\n"
		"\t\t> Error code     : %s\n",
		info->port, info->master ? info->master : "",
		info->dest ? info->dest : NO_NAME,
		info->target_addr,
		info->baaw_prot == true ? "(BAAW Remapped address)" : "",
		trans_type == TRANS_TYPE_READ ? "READ" : "WRITE",
		itmon_errcode[info->errcode]);

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	if (acon) {
		snprintf(temp_buf, SZ_128, "%s %s/ %s/ 0x%zx %s/ %s/ %s",
			info->port, info->master ? info->master : "",
			info->dest ? info->dest : NO_NAME,
			info->target_addr,
			info->baaw_prot == true ? "(by CP maybe)" : "",
			trans_type == TRANS_TYPE_READ ? "READ" : "WRITE",
			itmon_errcode[info->errcode]);
		secdbg_exin_set_busmon(temp_buf);
	}
#endif

	pr_auto_on(acon, ASL3,
		"\n-----------------------------------------"
		"-----------------------------------------\n"
		"\t\t> Size           : %u bytes x %u burst => %u bytes\n"
		"\t\t> Burst Type     : %u (0:FIXED, 1:INCR, 2:WRAP)\n"
		"\t\t> Level          : %s\n"
		"\t\t> Protection     : %s\n"
		"\t\t> Path Type      : %s\n\n",
		int_pow(2, info->axsize), info->axlen + 1,
		int_pow(2, info->axsize) * (info->axlen + 1),
		info->axburst,
		info->axprot & BIT(0) ? "Privileged" : "Unprivileged",
		info->axprot & BIT(1) ? "Non-secure" : "Secure",
		itmon_pathtype[info->path_type]);

	if (acon) {
		dbg_snapshot_set_str_offset(OFFSET_DSS_CUSTOM, SZ_128, "%s %s/ %s/ 0x%zx %s/ %s/ %s",
			info->port, info->master ? info->master : "",
			info->dest ? info->dest : NO_NAME,
			info->target_addr,
			info->baaw_prot == true ? "(BAAW Remapped address)" : "",
			trans_type == TRANS_TYPE_READ ? "READ" : "WRITE",
			itmon_errcode[info->errcode]);
	}
}

static void itmon_report_pathinfo(struct itmon_dev *itmon,
				  struct itmon_tracedata *data,
				  struct itmon_traceinfo *info,
				  unsigned int trans_type)

{
	struct itmon_nodeinfo *node = data->node;
	/* SEC DEBUG FEATURE */
	int acon = (itmon_get_dpm_policy(itmon) > 0) ? 1 : 0;
	/* SEC DEBUG FEATURE */


	if (BIT_PRT_CHK_ERR_OCCURRED(data->prt_chk_info) &&
		(data->prt_chk_info & GENMASK(8, 1)))
		return;

	if (!info->path_dirty) {
		pr_auto_on(acon, ASL3,
			"\n-----------------------------------------"
			"-----------------------------------------\n"
			"\t\tITMON Report (%s)\n"
			"-----------------------------------------"
			"-----------------------------------------\n"
			"\t\tPATH Information\n\n",
			trans_type == TRANS_TYPE_READ ? "READ" : "WRITE");
		info->path_dirty = true;
	}
	switch (node->type) {
	case M_NODE:
		pr_auto_on(acon, ASL3, "\t\t> %14s, %8s(0x%08llX)\n",
			node->name, "M_NODE", node->phy_regs + data->offset);
		break;
	case T_S_NODE:
		pr_auto_on(acon, ASL3, "\t\t> %14s, %8s(0x%08llX)\n",
			node->name, "T_S_NODE", node->phy_regs + data->offset);
		break;
	case T_M_NODE:
		pr_auto_on(acon, ASL3, "\t\t> %14s, %8s(0x%08llX)\n",
			node->name, "T_M_NODE", node->phy_regs + data->offset);
		break;
	case S_NODE:
		pr_auto_on(acon, ASL3, "\t\t> %14s, %8s(0x%08llX)\n",
			node->name, "S_NODE", node->phy_regs + data->offset);
		break;
	}
}

static int itmon_parse_cpuinfo(struct itmon_dev *itmon,
			       struct itmon_tracedata *data,
			       struct itmon_traceinfo *info,
			       unsigned int userbit)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *node = data->node;
	int cluster_num, core_num, i;

	for (i = 0; i < (int)ARRAY_SIZE(itmon_cpu_node_string); i++) {
		if (!strncmp(node->name, itmon_cpu_node_string[i],
			strlen(itmon_cpu_node_string[i]))) {
				core_num = (userbit & (GENMASK(2, 0)));
				cluster_num = 0;
				if (pdata->cpu_parsing)
					snprintf(info->buf, sizeof(info->buf) - 1,
						"CPU%d Cluster%d", core_num, cluster_num);
				else
					snprintf(info->buf, sizeof(info->buf) - 1, "CPU");
				info->port = info->buf;
				return 1;
			}
	};

	return 0;
}

static int itmon_chk_midnode(struct itmon_nodeinfo *node)
{
	const char **string;
	int i, num;

	if (node->type == M_NODE) {
		num = (int)ARRAY_SIZE(itmon_mid_mnode_string);
		string = (const char **)&itmon_mid_mnode_string[0];
	} else {
		num = (int)ARRAY_SIZE(itmon_mid_snode_string);
		string = (const char **)&itmon_mid_snode_string[0];
	}

	for (i = 0; i < num; i++) {
		if (!strncmp(node->name, string[i], strlen(string[i])))
			return 1;
	}

	return 0;
}

static void itmon_parse_traceinfo(struct itmon_dev *itmon,
				   struct itmon_tracedata *data,
				   unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *new_info = NULL;
	struct itmon_tracedata *find_data = NULL;
	struct itmon_masterinfo *master = NULL;
	struct itmon_rpathinfo *port = NULL;
	struct itmon_nodeinfo *node = data->node, *find_node = NULL;
	struct itmon_nodegroup *group = data->group;
	unsigned int errcode, axid, userbit;
	int i, ret;

	if (BIT_PRT_CHK_ERR_OCCURRED(data->prt_chk_info) &&
		(data->prt_chk_info & GENMASK(8, 1)))
		return;

	if (node->type != S_NODE && node->type != M_NODE)
		return;

	if (!BIT_ERR_VALID(data->int_info))
		return;

	errcode = BIT_ERR_CODE(data->int_info);
	if (node->type == M_NODE) {
		if (errcode != ERRCODE_DECERR)
			return;
		if (itmon_chk_midnode(node))
			return;
	}
	if (node->type == S_NODE) {
		if (errcode != ERRCODE_DECERR && itmon_chk_midnode(node))
			return;
	}
	new_info = kzalloc(sizeof(struct itmon_traceinfo), GFP_ATOMIC);
	if (!new_info) {
		dev_err(itmon->dev, "failed to kmalloc for %s node\n",
			node->name);
		return;
	}
	axid = (unsigned int)BIT_AXID(data->int_info);
	if (group->path_type == DATA_FROM_EXT)
		userbit = BIT_AXUSER(data->ext_user);
	else
		userbit = BIT_AXUSER_PERI(data->ext_info_2);

	switch (node->type) {
	case S_NODE:
		new_info->port = (char *)"See M_NODE of Raw registers";
		new_info->master = (char *)"";
		new_info->dest = node->name;
		new_info->s_node = node;

		port = itmon_get_rpathinfo(itmon, axid, node->name);
		list_for_each_entry(find_data, &pdata->datalist[trans_type], list) {
			find_node = find_data->node;
			if (find_node->type != M_NODE)
				continue;
			ret = itmon_parse_cpuinfo(itmon, find_data, new_info, userbit);
			if (ret || (port && !strncmp(find_node->name,
				port->port_name, strlen(find_node->name)))) {
				new_info->m_node = find_node;
				break;
			}
		}
		if (port) {
			new_info->port = port->port_name;
			master = itmon_get_masterinfo(itmon, new_info->port, userbit);
			if (master)
				new_info->master = master->master_name;
		}
		break;
	case M_NODE:
		new_info->dest = (char *)"See Target address";
		new_info->port = node->name;
		new_info->master = (char *)"";
		new_info->m_node = node;

		ret = itmon_parse_cpuinfo(itmon, data, new_info, userbit);
		if (ret)
			break;

		master = itmon_get_masterinfo(itmon, node->name, userbit);
		if (master)
			new_info->master = master->master_name;
		break;
	}

	/* Common Information */
	new_info->path_type = group->path_type;
	new_info->target_addr = (((u64)data->ext_info_1 &
					GENMASK(15, 0)) << 32ULL);
	new_info->target_addr |= data->ext_info_0;
	new_info->errcode = errcode;
	new_info->dirty = true;
	new_info->axsize = BIT_AXSIZE(data->ext_info_1);
	new_info->axlen = BIT_AXLEN(data->ext_info_1);
	new_info->axburst = BIT_AXBURST(data->ext_info_2);
	new_info->axprot = BIT_AXPROT(data->ext_info_2);
	new_info->baaw_prot = false;

	for (i = 0; i < (int)ARRAY_SIZE(itmon_invalid_addr); i++) {
		if (new_info->target_addr == itmon_invalid_addr[i]) {
			new_info->baaw_prot = true;
			break;
		}
	}
	data->ref_info = new_info;
	list_add(&new_info->list, &pdata->infolist[trans_type]);
}

static void itmon_analyze_errnode(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *info, *next_info;
	struct itmon_tracedata *data, *next_data;
	unsigned int trans_type;
	int i, ret;

	/* Parse */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		list_for_each_entry(data, &pdata->datalist[trans_type], list)
			itmon_parse_traceinfo(itmon, data, trans_type);
	}

	/* Report and Clean-up traceinfo */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		list_for_each_entry_safe(info, next_info, &pdata->infolist[trans_type], list) {
			info->path_dirty = false;
			itmon_reflect_policy(itmon, info);
			ret = itmon_notifier_handler(itmon, info, trans_type);
			itmon_reflect_policy_by_notifier(itmon, info, ret);
			list_for_each_entry(data, &pdata->datalist[trans_type], list) {
				if (data->ref_info == info)
					itmon_report_pathinfo(itmon, data, info, trans_type);
			}
			itmon_report_traceinfo(itmon, info, trans_type);
			list_del(&info->list);
			kfree(info);
		}
	}
	itmon_dump_dpm_policy(itmon);

	/* Report Raw all tracedata and Clean-up tracedata and node */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		for (i = M_NODE; i < NODE_TYPE; i++) {
			list_for_each_entry_safe(data, next_data,
				&pdata->datalist[trans_type], list) {
				if (i == data->node->type) {
					itmon_report_rawdata(itmon, data);
					list_del(&data->list);
					kfree(data);
				}
			}
		}
	}
}

static void itmon_collect_errnode(struct itmon_dev *itmon,
			    struct itmon_nodegroup *group,
			    struct itmon_nodeinfo *node,
			    unsigned int offset)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_tracedata *new_data = NULL;
	unsigned int int_info, info0, info1, info2, user = 0;
	unsigned int prt_chk_ctl, prt_chk_info, prt_chk_int_id, dbg_mo_cnt;
	bool read = TRANS_TYPE_WRITE;

	int_info = readl(node->regs + offset + REG_INT_INFO);
	info0 = readl(node->regs + offset + REG_EXT_INFO_0);
	info1 = readl(node->regs + offset + REG_EXT_INFO_1);
	info2 = readl(node->regs + offset + REG_EXT_INFO_2);
	if (group->path_type == DATA_FROM_EXT)
		user = readl(node->regs + offset + REG_EXT_USER);

	dbg_mo_cnt = readl(node->regs + OFFSET_PRT_CHK);
	prt_chk_ctl = readl(node->regs + OFFSET_PRT_CHK + REG_PRT_CHK_CTL);
	prt_chk_info = readl(node->regs + OFFSET_PRT_CHK + REG_PRT_CHK_INT);
	prt_chk_int_id = readl(node->regs + OFFSET_PRT_CHK + REG_PRT_CHK_INT_ID);
	switch (offset) {
	case OFFSET_REQ_R:
		read = TRANS_TYPE_READ;
		/* fall down */
	case OFFSET_REQ_W:
		/* Only S-Node is able to make log to registers */
		break;
	case OFFSET_RESP_R:
		read = TRANS_TYPE_READ;
		/* fall down */
	case OFFSET_RESP_W:
		/* Only NOT S-Node is able to make log to registers */
		break;
	default:
		pr_auto(ASL3,
			"Unknown Error - node:%s offset:%u\n", node->name, offset);
		break;
	}

	new_data = kzalloc(sizeof(struct itmon_tracedata), GFP_ATOMIC);
	if (!new_data) {
		pr_auto(ASL3,
			"failed to kmalloc for %s node %x offset\n",
			node->name, offset);
		return;
	}

	new_data->int_info = int_info;
	new_data->ext_info_0 = info0;
	new_data->ext_info_1 = info1;
	new_data->ext_info_2 = info2;
	new_data->ext_user = user;
	new_data->dbg_mo_cnt = dbg_mo_cnt;
	new_data->prt_chk_ctl = prt_chk_ctl;
	new_data->prt_chk_info = prt_chk_info;
	new_data->prt_chk_int_id = prt_chk_int_id;

	new_data->offset = offset;
	new_data->read = read;
	new_data->ref_info = NULL;
	new_data->group = group;
	new_data->node = node;

	if (BIT_ERR_VALID(int_info))
		new_data->logging = true;
	else
		new_data->logging = false;

	list_add(&new_data->list, &pdata->datalist[read]);
}

static int __itmon_search_node(struct itmon_dev *itmon,
			       struct itmon_nodegroup *group,
			       bool clear)
{
	struct itmon_nodeinfo *node = NULL;
	unsigned int val, offset, freeze;
	unsigned long vec, bit = 0;
	int i, ret = 0;

	if (group->phy_regs) {
		if (group->ex_table)
			vec = (unsigned long)readq(group->regs);
		else
			vec = (unsigned long)readl(group->regs);
	} else {
		vec = GENMASK(group->nodesize, 0);
	}

	if (!vec)
		return 0;

	node = group->nodeinfo;

	for_each_set_bit(bit, &vec, group->nodesize) {
		/* exist array */
		for (i = 0; i < OFFSET_NUM; i++) {
			offset = i * OFFSET_ERR_REPT;
			/* Check Request information */
			val = readl(node[bit].regs + offset + REG_INT_INFO);
			if (BIT_ERR_OCCURRED(val)) {
				/* This node occurs the error */
				itmon_collect_errnode(itmon, group, &node[bit], offset);
				if (clear)
					writel(1, node[bit].regs
							+ offset + REG_INT_CLR);
				ret++;
			}
		}
		/* Check H/W assertion */
		if (node[bit].prt_chk_enabled) {
			val = readl(node[bit].regs + OFFSET_PRT_CHK +
						REG_PRT_CHK_INT);
			/* if timeout_freeze is enable,
			 * prt_chk interrupt is able to assert without any information */
			if (BIT_PRT_CHK_ERR_OCCURRED(val) && (val & GENMASK(8, 1))) {
				itmon_collect_errnode(itmon, group, &node[bit], 0);
				itmon_reflect_policy_by_node(itmon, &node[bit], PRTCHKER);
				ret++;
			}
		}
		/* Check Timeout freeze enable node */
		if (node[bit].tmout_enabled) {
			val = readl(node[bit].regs + OFFSET_TMOUT_REG  +
						REG_TMOUT_FRZ_STATUS );
			freeze = val & (unsigned int)GENMASK(1, 0);
			if (freeze) {
				if (freeze & BIT(0))
					itmon_report_timeout(itmon, &node[bit], TRANS_TYPE_WRITE);
				if (freeze & BIT(1))
					itmon_report_timeout(itmon, &node[bit], TRANS_TYPE_READ);

				itmon_collect_errnode(itmon, group, &node[bit], 0);
				itmon_reflect_policy_by_node(itmon, &node[bit], TMOUT);
				ret++;
			}
		}
	}

	return ret;
}

static int itmon_search_node(struct itmon_dev *itmon,
			     struct itmon_nodegroup *group,
			     bool clear)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i, ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&itmon->ctrl_lock, flags);

	if (group) {
		ret = __itmon_search_node(itmon, group, clear);
	} else {
		/* Processing all group & nodes */
		for (i = 0; i < pdata->num_nodegroup; i++) {
			group = &pdata->nodegroup[i];
			if (group->pd_support && !group->pd_status)
				continue;
			ret += __itmon_search_node(itmon, group, clear);
		}
	}
	if (ret)
		itmon_analyze_errnode(itmon);

	spin_unlock_irqrestore(&itmon->ctrl_lock, flags);
	return ret;
}

static irqreturn_t itmon_irq_handler(int irq, void *data)
{
	struct itmon_dev *itmon = (struct itmon_dev *)data;
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodegroup *group = NULL;
	bool ret;
	int i;

	/* Search itmon group */
	for (i = 0; i < (int)pdata->num_nodegroup; i++) {
		group = &pdata->nodegroup[i];
		if (!group->pd_support) {
			dev_err(itmon->dev, "%d irq, %s group, pd %s, 0x%x vec",
				irq, group->name, "on",
				readl(group->regs));
		} else {
			dev_err(itmon->dev, "%d irq, %s group, pd %s, 0x%x vec",
				irq, group->name,
				group->pd_status ? "on" : "off",
				group->pd_status ? readl(group->regs) : 0);
		}
	}

	ret = itmon_search_node(itmon, NULL, true);
	if (!ret)
		dev_err(itmon->dev, "No errors found %s\n", __func__);
	else
		dev_err(itmon->dev, "Error detected %s, %d\n", __func__, ret);

	itmon_post_handler(itmon, ret);

	return IRQ_HANDLED;
}

void itmon_notifier_chain_register(struct notifier_block *block)
{
	atomic_notifier_chain_register(&itmon_notifier_list, block);
}
EXPORT_SYMBOL(itmon_notifier_chain_register);

static int itmon_logging_panic_handler(struct notifier_block *nb,
				     unsigned long l, void *buf)
{
	struct itmon_panic_block *itmon_panic = (struct itmon_panic_block *)nb;
	struct itmon_dev *itmon = itmon_panic->pdev;
	int ret;

	if (!IS_ERR_OR_NULL(itmon)) {
		/* Check error has been logged */
		ret = itmon_search_node(itmon, NULL, true);
		if (!ret) {
			dev_info(itmon->dev, "No errors found %s\n", __func__);
		} else {
			dev_err(itmon->dev, "Error detected %s, %d\n",
							__func__, ret);
			itmon_do_dpm_policy(itmon, true);
		}
	}
	return 0;
}

#define MAX_CUSTOMIZE		64

static void itmon_parse_dt_customize(struct itmon_dev *itmon,
				     struct device_node *np)
{
	struct device_node *node_np;
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_policy *plat_policy;
	struct itmon_nodepolicy nd_policy;
	struct itmon_nodeinfo *node;
	char snode_name[16];
	const char *node_name;
	unsigned int val;
	int i;

	for (i = 0; i < MAX_CUSTOMIZE; i++) {
		nd_policy.enabled = 0;
		snprintf(snode_name, sizeof(snode_name), "node%d", i);
		node_np = of_get_child_by_name(np, (const char *)snode_name);

		if (!node_np) {
			dev_info(itmon->dev, "%s device node is not found\n",
					snode_name);
			break;
		}

		if(of_property_read_string(node_np, "node", &node_name))
			continue;

		node = itmon_get_nodeinfo(itmon, NULL, node_name);
		if (!node) {
			dev_err(itmon->dev, "%s node is not found\n", node_name);
			continue;
		} else {
			dev_info(itmon->dev, "%s node found\n", node->name);
		}

		if (!of_property_read_u32(node_np, "irq_mask", &val)) {
			nd_policy.chk_irq_mask = 1;
			nd_policy.en_irq_mask = val;
			dev_info(itmon->dev, "%s node: irq mask -> [%d]\n",
					node->name, val);
		}

		if (!of_property_read_u32(node_np, "prio", &val)) {
			nd_policy.prio = val;
			dev_info(itmon->dev, "%s node: prio -> [%d]\n",
					node->name, val);
		}

		if (!of_property_read_u32(node_np, "errrpt", &val)) {
			nd_policy.chk_errrpt = 1;
			nd_policy.en_errrpt = val;
			dev_info(itmon->dev, "%s node: errrtp [%d] -> [%d]\n",
					node->name, node->err_enabled, val);
		}

		if (!of_property_read_u32(node_np, "prtchker", &val)) {
			nd_policy.chk_prtchk = 1;
			nd_policy.en_prtchk = val;
			dev_info(itmon->dev, "%s node: prtchker [%d] -> [%d]\n",
					node->name, node->prt_chk_enabled, val);
		}

		if (!of_property_read_u32(node_np, "tmout", &val)) {
			nd_policy.chk_tmout = 1;
			nd_policy.en_tmout = val;
			dev_info(itmon->dev, "%s node: tmout [%d] -> [%d]\n",
					node->name, node->tmout_enabled, val);
		}

		if (!of_property_read_u32(node_np, "tmout_val", &val)) {
			nd_policy.chk_tmout_val = 1;
			nd_policy.en_tmout_val = val;
			dev_info(itmon->dev, "%s node: tmout_val [%x] -> [%x]\n",
					node->name, node->time_val, val);
		}

		if (!of_property_read_u32(node_np, "freeze", &val)) {
			nd_policy.chk_freeze = 1;
			nd_policy.en_freeze = val;
			dev_info(itmon->dev, "%s node: freeze [%x] -> [%x]\n",
					node->name, node->tmout_frz_enabled, val);
		}

		if (!of_property_read_u32(node_np, "decerr_job", &val)) {
			plat_policy = &pdata->policy[DECERR];
			nd_policy.chk_decerr_job = 1;
			nd_policy.en_decerr_job = val;
			dev_info(itmon->dev, "%s node: decerr_job [%s] -> [%d/%s]\n",
					node->name, itmon_dpm_action[plat_policy->policy_def],
					nd_policy.chk_decerr_job,
					itmon_dpm_action[nd_policy.en_decerr_job]);
		}

		if (!of_property_read_u32(node_np, "slverr_job", &val)) {
			plat_policy = &pdata->policy[SLVERR];
			nd_policy.chk_slverr_job = 1;
			nd_policy.en_slverr_job = val;
			dev_info(itmon->dev, "%s node: slverr_job [%s] -> [%d/%s]\n",
					node->name, itmon_dpm_action[plat_policy->policy_def],
					nd_policy.chk_slverr_job,
					itmon_dpm_action[nd_policy.en_slverr_job]);
		}

		if (!of_property_read_u32(node_np, "tmout_job", &val)) {
			plat_policy = &pdata->policy[TMOUT];
			nd_policy.chk_tmout_job = 1;
			nd_policy.en_tmout_job = val;
			dev_info(itmon->dev, "%s node: tmout_job [%s] -> [%d/%s]\n",
					node->name, itmon_dpm_action[plat_policy->policy_def],
					nd_policy.chk_tmout_job,
					itmon_dpm_action[nd_policy.en_tmout_job]);
		}

		if (!of_property_read_u32(node_np, "prtchker_job", &val)) {
			plat_policy = &pdata->policy[PRTCHKER];
			nd_policy.chk_prtchk_job = 1;
			nd_policy.en_prtchk_job = val;
			dev_info(itmon->dev, "%s node: prtchker_job [%s] -> [%d/%s]\n",
					node->name, itmon_dpm_action[plat_policy->policy_def],
					nd_policy.chk_prtchk_job,
					itmon_dpm_action[nd_policy.en_prtchk_job]);
		}
		itmon_set_nodepolicy(itmon, node, nd_policy, false);
	}
	itmon_dump_dpm_policy(itmon);
}

enum itmon_ipc_cmd {
	eITMON_IPC_CMD_INIT,
	eITMON_IPC_CMD_SET_ENABLE,
	eITMON_IPC_CMD_GET_ENABLE,
};

__maybe_unused static void adv_tracer_itmon_handler(
					struct adv_tracer_ipc_cmd *cmd,
					unsigned int len)
{
	switch (cmd->cmd_raw.cmd) {

	default:
	break;
	}
}

static int itmon_adv_tracer_init(struct itmon_dev *itmon,
				 struct device_node *np)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct adv_tracer_plugin *itmon_adv = NULL;
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	itmon_adv = kzalloc(sizeof(struct adv_tracer_plugin), GFP_KERNEL);
	if (!itmon_adv) {
		ret = -ENOMEM;
		return ret;
	}

	ret = adv_tracer_ipc_request_channel(np,
				(ipc_callback)adv_tracer_itmon_handler,
				&itmon_adv->id, &itmon_adv->len);
	if (ret < 0) {
		dev_err(itmon->dev, "ipc request fail(%d): Request Channel\n", ret);
		ret = -ENODEV;
		kfree(itmon_adv);
		pdata->itmon_adv = NULL;
		return ret;
	}

	pdata->itmon_adv = itmon_adv;

	dev_err(itmon->dev, "%s successful - id:0x%x, len:0x%x\n",
			__func__, itmon_adv->id, itmon_adv->len);

	cmd.cmd_raw.cmd = eITMON_IPC_CMD_INIT;
	ret = adv_tracer_ipc_send_data(itmon_adv->id, &cmd);
	if (ret < 0) {
		dev_err(itmon->dev, "ipc request fail: CMD_INIT(%d)\n", ret);
		ret = -ENODEV;
		kfree(itmon_adv);
		pdata->itmon_adv = NULL;
		return ret;
	}

	pdata->adv_enabled = true;
	dev_err(itmon->dev, "MINITMON Activation\n");

	return ret;
}

static int itmon_adv_tracer_set_enable(struct itmon_dev *itmon, bool enable)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct adv_tracer_plugin *itmon_adv = pdata->itmon_adv;
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	if (!itmon_adv)
		return 0;

	cmd.cmd_raw.cmd = eITMON_IPC_CMD_SET_ENABLE;
	cmd.buffer[1] = enable;

	ret = adv_tracer_ipc_send_data(itmon_adv->id, &cmd);
	if (ret < 0) {
		dev_err(itmon->dev, "ipc request fail: CMD_SET_ENABLE(%d, %d)\n", enable, ret);
		return ret;
	}

	pdata->adv_enabled = enable;

	return 0;
}

static void itmon_parse_dt(struct itmon_dev *itmon)
{
	struct device_node *np = itmon->dev->of_node;
	struct device_node *child_np;
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned int val;
	int i, ret;

	if (of_property_read_bool(np, "big-bus-supporting")) {
		dev_info(itmon->dev, "big bus support\n");
		pdata->big_bus = true;
	} else {
		pdata->big_bus = false;
	}

	if (of_property_read_bool(np, "support-irq-oring")) {
		dev_info(itmon->dev, "only 1 irq support\n");
		pdata->irq_oring = true;
	} else {
		pdata->irq_oring = false;
	}

	if (of_property_read_bool(np, "no-support-cpu-parsing")) {
		dev_info(itmon->dev, "CPU Parser is not support\n");
		pdata->cpu_parsing = false;
	} else {
		pdata->cpu_parsing = true;
	}

	for (i = 0; i < TYPE_MAX; i++) {
		if (!of_property_read_u32(np, pdata->policy[i].name, &val)) {
			pdata->policy[i].policy_def = val;
			pdata->policy[i].policy = val;
		}
	}

	child_np = of_get_child_by_name(np, "customize");
	if (!child_np) {
		dev_info(itmon->dev, "customize is not found\n");
		return;
	}

	itmon_parse_dt_customize(itmon, child_np);

	child_np = of_get_child_by_name(np, "dbgc");
	if (!child_np) {
		dev_info(itmon->dev, "dbgc is not found\n");
		return;
	}

	ret = itmon_adv_tracer_init(itmon, child_np);
	if (ret < 0)
		dev_info(itmon->dev, "minitmon is not support\n");
}

static int itmon_load_keepdata(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	struct device *dev = itmon->dev;
	struct itmon_keepdata *kdata;
	pgprot_t prot = __pgprot(PROT_NORMAL);
	int page_size, i;
	struct page *page, **pages;
	unsigned long flags = VM_NO_GUARD | VM_MAP;
	void *vaddr;

	rmem_np = of_parse_phandle(dev->of_node, "memory-region", 0);
	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		dev_err(dev, "failed to acquire memory region\n");
		return -ENOMEM;
	}

	page_size = rmem->size / PAGE_SIZE;
	pages = kzalloc(sizeof(struct page *) * page_size, GFP_KERNEL);
	page = phys_to_page(rmem->base);

	for (i = 0; i < page_size; i++)
		pages[i] = page++;

	vaddr = vmap(pages, page_size, flags, prot);
	kfree(pages);
	if (!vaddr) {
		dev_err(dev, "paddr:%x page_size:%x failed to vmap\n",
				rmem->base, page_size);
		return -ENOMEM;
	}

	kdata = (struct itmon_keepdata *)(vaddr);

	if (kdata->magic != 0xBEEFBEEF) {
		dev_err(dev, "Invalid Keep data of ITMON\n");
		return -EFAULT;
	}

	dev_info(dev, "reserved - base: 0x%x, magic: 0x%x, rpathinfo: 0x%x,"
		      "masterinfo: 0x%x, nodegroup: 0x%x\n",
			(u64)kdata->base,
			kdata->magic,
			kdata->mem_rpathinfo,
			kdata->mem_masterinfo,
			kdata->mem_nodegroup);

	pdata->rpathinfo = (struct itmon_rpathinfo *)
		((u64)vaddr + (u64)(kdata->mem_rpathinfo - kdata->base));
	pdata->num_rpathinfo = kdata->num_rpathinfo;

	pdata->masterinfo = (struct itmon_masterinfo *)
		((u64)vaddr + (u64)(kdata->mem_masterinfo - kdata->base));
	pdata->num_masterinfo = kdata->num_masterinfo;

	pdata->nodegroup = (struct itmon_nodegroup *)
		((u64)vaddr + (u64)(kdata->mem_nodegroup - kdata->base));
	pdata->num_nodegroup = kdata->num_nodegroup;

	for (i = 0; i < (int)pdata->num_nodegroup; i++) {
		u64 offset = (((u64)(pdata->nodegroup[i].nodeinfo_phy))
						- (u64)(kdata->base));

		pdata->nodegroup[i].nodeinfo =
			(struct itmon_nodeinfo *)((u64)vaddr + (u64)offset);
	}

	return 0;
}

static ssize_t enable_all_show(struct device *dev,
			       struct device_attribute *attr, char *buf)

{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;

	return sysfs_emit(buf, "set itmon enabled: %d\n", pdata->enabled);
}

static ssize_t enable_all_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	unsigned long val = 0;

	if (!buf || size == 0)
		return -EINVAL;

	if ((kstrtoul(buf, SZ_16, &val) != 0) || (val != 1 && val != 0))
		return -EINVAL;

	itmon_enable(val);

	return size;
}

static ssize_t timeout_fix_val_show(struct device *dev,
			       struct device_attribute *attr, char *buf)

{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;

	return sysfs_emit(buf, "set timeout val: %#x\n", pdata->sysfs_tmout_val);
}

static ssize_t timeout_fix_val_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned long val = 0;

	if (!buf || size == 0)
		return -EINVAL;

	if (kstrtoul(buf, SZ_16, &val) != 0 && val != (u32) val)
		return -EINVAL;

	pdata->sysfs_tmout_val = (unsigned int)val;

	return size;
}

static ssize_t timeout_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned int i;
	ssize_t n = 0;

	/* Processing all group & nodes */
	for (i = 0; i < pdata->num_nodegroup; i++) {
		struct itmon_nodegroup *group = &pdata->nodegroup[i];
		struct itmon_nodeinfo *node = group->nodeinfo;
		unsigned long vec = GENMASK(group->nodesize, 0), bit = 0;
		unsigned int offset = OFFSET_TMOUT_REG;

		if (group->pd_support && !group->pd_status)
			continue;

		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE && node[bit].tmout_enabled) {
				n += scnprintf(buf + n, PAGE_SIZE - n,
					"%-12s : 0x%08X, timeout : %x\n",
					node[bit].name, node[bit].phy_regs,
					readl_relaxed(node[bit].regs + offset + REG_DBG_CTL));
			}
		}
	}
	return n;
}

static ssize_t timeout_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned int i;
	char *name;
	int len;

	if (!buf || size == 0)
		return -EINVAL;

	len = min(size, strlen(buf));
	name = (char *)kstrndup(buf, len, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	for (i = 0; i < pdata->num_nodegroup; i++) {
		struct itmon_nodegroup *group = &pdata->nodegroup[i];
		struct itmon_nodeinfo *node = group->nodeinfo;
		unsigned long vec = GENMASK(group->nodesize, 0), bit = 0;
		unsigned int offset = OFFSET_TMOUT_REG;

		if (group->pd_support && !group->pd_status)
			continue;

		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE && node[bit].tmout_enabled &&
				!strncmp(name, node[bit].name, len)) {
				u32 val = readl_relaxed(node[bit].regs + offset + REG_DBG_CTL);

				writel_relaxed(val ? 0 : 1, node[bit].regs + offset + REG_DBG_CTL);
				node[bit].tmout_enabled = val;
			}
		}
	}
	kfree(name);
	return size;
}

static ssize_t timeout_val_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned int i;
	ssize_t n = 0;

	/* Processing all group & nodes */
	for (i = 0; i < pdata->num_nodegroup; i++) {
		struct itmon_nodegroup *group = &pdata->nodegroup[i];
		struct itmon_nodeinfo *node = group->nodeinfo;
		unsigned long vec = GENMASK(group->nodesize, 0), bit = 0;
		unsigned int offset = OFFSET_TMOUT_REG;

		if (group->pd_support && !group->pd_status)
			continue;

		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE && node[bit].tmout_enabled) {
				n += scnprintf(buf + n, PAGE_SIZE - n,
					"%-12s : 0x%08X, timeout : 0x%x\n",
					node[bit].name, node[bit].phy_regs,
					readl_relaxed(node[bit].regs + offset + REG_TMOUT_INIT_VAL));
			}
		}
	}
	return n;
}

static ssize_t timeout_val_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned int i;
	char *name;
	int len;

	if (!buf || size == 0)
		return -EINVAL;

	len = min(size, strlen(buf));
	name = (char *)kstrndup(buf, len, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	for (i = 0; i < pdata->num_nodegroup; i++) {
		struct itmon_nodegroup *group = &pdata->nodegroup[i];
		struct itmon_nodeinfo *node = group->nodeinfo;
		unsigned long vec = GENMASK(group->nodesize, 0), bit = 0;
		unsigned int offset = OFFSET_TMOUT_REG;

		if (group->pd_support && !group->pd_status)
			continue;

		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE && node[bit].tmout_enabled &&
				!strncmp(name, node[bit].name, len)) {
				writel_relaxed(pdata->sysfs_tmout_val,
						node[bit].regs + offset + REG_TMOUT_INIT_VAL);
				node[bit].time_val = pdata->sysfs_tmout_val;
			}
		}
	}
	kfree(name);
	return size;
}

static ssize_t timeout_freeze_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned int i;
	ssize_t n = 0;

	/* Processing all group & nodes */
	for (i = 0; i < pdata->num_nodegroup; i++) {
		struct itmon_nodegroup *group = &pdata->nodegroup[i];
		struct itmon_nodeinfo *node = group->nodeinfo;
		unsigned long vec = GENMASK(group->nodesize, 0), bit = 0;
		unsigned int offset = OFFSET_TMOUT_REG;

		if (group->pd_support && !group->pd_status)
			continue;

		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE && node[bit].tmout_enabled) {
				n += scnprintf(buf + n, PAGE_SIZE - n,
					"%-12s : 0x%08X, timeout_freeze : %x\n",
					node[bit].name, node[bit].phy_regs,
					readl_relaxed(node[bit].regs + offset + REG_TMOUT_FRZ_EN));
			}
		}
	}
	return n;
}

static ssize_t timeout_freeze_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct itmon_platdata *pdata = g_itmon->pdata;
	unsigned int i;
	char *name;
	int len;

	if (!buf || size == 0)
		return -EINVAL;

	len = min(size, strlen(buf));
	name = (char *)kstrndup(buf, len, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	for (i = 0; i < pdata->num_nodegroup; i++) {
		struct itmon_nodegroup *group = &pdata->nodegroup[i];
		struct itmon_nodeinfo *node = group->nodeinfo;
		unsigned long vec = GENMASK(group->nodesize, 0), bit = 0;
		unsigned int offset = OFFSET_TMOUT_REG;

		if (group->pd_support && !group->pd_status)
			continue;

		for_each_set_bit(bit, &vec, group->nodesize) {
			if (node[bit].type == S_NODE && node[bit].tmout_enabled &&
				!strncmp(name, node[bit].name, len)) {
				u32 val = readl_relaxed(node[bit].regs + offset + REG_TMOUT_FRZ_EN);

				writel_relaxed(val ? 0 : 1, node[bit].regs + offset + REG_TMOUT_FRZ_EN);
				node[bit].tmout_frz_enabled = val;
			}
		}
	}
	kfree(name);
	return size;
}

static DEVICE_ATTR_RW(enable_all);
static DEVICE_ATTR_RW(timeout_fix_val);
static DEVICE_ATTR_RW(timeout);
static DEVICE_ATTR_RW(timeout_val);
static DEVICE_ATTR_RW(timeout_freeze);

static struct attribute *itmon_sysfs_attrs[] = {
	&dev_attr_enable_all.attr,
	&dev_attr_timeout_fix_val.attr,
	&dev_attr_timeout.attr,
	&dev_attr_timeout_val.attr,
	&dev_attr_timeout_freeze.attr,
	NULL,
};
ATTRIBUTE_GROUPS(itmon_sysfs);

static void itmon_set_irq_affinity(struct itmon_dev *itmon,
				   unsigned int irq, unsigned long affinity)
{
	struct cpumask affinity_mask;
	unsigned long bit;
	char *buf = (char *)__get_free_page(GFP_KERNEL);

	cpumask_clear(&affinity_mask);

	if (affinity) {
		for_each_set_bit(bit, &affinity, nr_cpu_ids)
			cpumask_set_cpu(bit, &affinity_mask);
	} else {
		cpumask_copy(&affinity_mask, cpu_online_mask);
	}
	irq_set_affinity_hint(irq, &affinity_mask);

	cpumap_print_to_pagebuf(true, buf, &affinity_mask);
	dev_dbg(itmon->dev, "affinity of irq%d is %s", irq, buf);
	free_page((unsigned long)buf);
}

static int itmon_probe(struct platform_device *pdev)
{
	struct itmon_dev *itmon;
	struct itmon_panic_block *itmon_panic = NULL;
	struct itmon_platdata *pdata;
	struct itmon_nodeinfo *node;
	unsigned int irq, val = 0;
	char *dev_name;
	int ret, i, j;

	itmon = devm_kzalloc(&pdev->dev,
				sizeof(struct itmon_dev), GFP_KERNEL);
	if (!itmon)
		return -ENOMEM;

	itmon->dev = &pdev->dev;

	spin_lock_init(&itmon->ctrl_lock);

	pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct itmon_platdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	itmon->pdata = pdata;

	if (itmon_load_keepdata(itmon) < 0)
		return -ENOMEM;

	itmon->pdata->policy = err_policy;

	itmon_parse_dt(itmon);

	for (i = 0; i < (int)pdata->num_nodegroup; i++) {
		dev_name = pdata->nodegroup[i].name;
		node = pdata->nodegroup[i].nodeinfo;

		if (pdata->nodegroup[i].phy_regs) {
			pdata->nodegroup[i].regs =
				devm_ioremap(&pdev->dev,
						 pdata->nodegroup[i].phy_regs, SZ_16K);
			if (pdata->nodegroup[i].regs == NULL) {
				dev_err(&pdev->dev,
					"failed to claim register region - %s\n",
					dev_name);
				return -ENOENT;
			}
		}

		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			node[j].regs = devm_ioremap(&pdev->dev,
						node[j].phy_regs, SZ_16K);
			if (node[j].regs == NULL) {
				dev_err(&pdev->dev,
					"failed to claim register region - %s\n",
					dev_name);
				return -ENOENT;
			}
			node[j].group = &pdata->nodegroup[i];
		}
		irq = irq_of_parse_and_map(pdev->dev.of_node, i);
		pdata->nodegroup[i].irq = SET_IRQ(irq);
		if (!irq)
			continue;

		of_property_read_u32_index(pdev->dev.of_node, "interrupt-affinity", i, &val);
		itmon_set_irq_affinity(itmon, irq, val);

		pdata->nodegroup[i].irq |= SET_AFFINITY(val);

		irq_set_status_flags(irq, IRQ_NOAUTOEN);
		disable_irq(irq);
		ret = devm_request_irq(&pdev->dev, irq,
				       itmon_irq_handler,
				       IRQF_NOBALANCING, dev_name, itmon);
		if (ret == 0) {
			dev_info(&pdev->dev,
				 "success to register request irq%u - %s\n",
				 irq, dev_name);
		} else {
			dev_err(&pdev->dev, "failed to request irq - %s\n",
				dev_name);
			return -ENOENT;
		}
	}
	itmon_panic = devm_kzalloc(&pdev->dev, sizeof(struct itmon_panic_block),
				 GFP_KERNEL);

	if (!itmon_panic) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				    "panic handler data\n");
	} else {
		itmon_panic->nb_panic_block.notifier_call = itmon_logging_panic_handler;
		itmon_panic->pdev = itmon;
		atomic_notifier_chain_register(&panic_notifier_list,
					       &itmon_panic->nb_panic_block);
	}

	platform_set_drvdata(pdev, itmon);

	itmon_init(itmon, true);

	pdata->last_time = 0;
	pdata->last_errcnt = 0;

	g_itmon = itmon;

	ret = sysfs_create_groups(&pdev->dev.kobj, itmon_sysfs_groups);
	if (ret)
		dev_info(&pdev->dev, "fail to register sysfs\n");

	for (i = 0; i < TRANS_TYPE_NUM; i++) {
		INIT_LIST_HEAD(&pdata->datalist[i]);
		INIT_LIST_HEAD(&pdata->infolist[i]);
	}

	pdata->probed = true;
	dev_info(&pdev->dev, "success to probe Exynos ITMON driver\n");

	for (i = 0; i < (int)pdata->num_nodegroup; i++)
		if (pdata->nodegroup[i].irq)
			enable_irq(GET_IRQ(pdata->nodegroup[i].irq));

	return 0;
}

static int itmon_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int itmon_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);

	itmon_adv_tracer_set_enable(itmon, false);

	dev_info(dev, "%s\n", __func__);
	return 0;
}

static int itmon_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;
	int i;

	for (i = 0; i < (int)pdata->num_nodegroup; i++) {
		unsigned int irq = pdata->nodegroup[i].irq;

		if (irq)
			itmon_set_irq_affinity(itmon, GET_IRQ(irq), GET_AFFINITY(irq));
	}
	itmon_init(itmon, true);
	itmon_adv_tracer_set_enable(itmon, true);

	dev_info(dev, "%s\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(itmon_pm_ops, itmon_suspend, itmon_resume);
#define ITMON_PM	(itmon_pm_ops)
#else
#define ITM_ONPM	NULL
#endif

static struct platform_driver exynos_itmon_driver = {
	.probe = itmon_probe,
	.remove = itmon_remove,
	.driver = {
		   .name = "exynos-itmon",
		   .of_match_table = itmon_dt_match,
		   .pm = &itmon_pm_ops,
		   },
};
module_platform_driver(exynos_itmon_driver);

MODULE_DESCRIPTION("Samsung Exynos ITMON COMMON DRIVER");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-itmon");
