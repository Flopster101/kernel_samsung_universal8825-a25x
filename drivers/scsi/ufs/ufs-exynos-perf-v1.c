// SPDX-License-Identifier: GPL-2.0-only
/*
 * IO Performance mode with UFS
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Authors:
 *	Kiwoong <kwmad.kim@samsung.com>
 */
#include <linux/of.h>
#include "ufs-exynos-perf.h"
#include "ufs-cal-if.h"
#include "ufs-exynos.h"
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
#include <soc/samsung/exynos-cpupm.h>
#endif

#include <trace/events/ufs_exynos_perf.h>

/* control knob */
static int __ctrl_dvfs(struct ufs_perf *perf, enum ctrl_op op)
{
	trace_ufs_perf_lock("dvfs", op);
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	if (op == CTRL_OP_UP) {
		if (perf->val_pm_qos_int)
			exynos_pm_qos_update_request(&perf->pm_qos_int, perf->val_pm_qos_int);
		if (perf->val_pm_qos_mif)
			exynos_pm_qos_update_request(&perf->pm_qos_mif, perf->val_pm_qos_mif);
	} else if (op == CTRL_OP_DOWN) {
		if (perf->val_pm_qos_int)
			exynos_pm_qos_update_request(&perf->pm_qos_int, 0);
		if (perf->val_pm_qos_mif)
			exynos_pm_qos_update_request(&perf->pm_qos_mif, 0);
	} else {
		return -1;
	}
#endif
#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	if (unlikely(!perf->val_pm_qos_cluster0))
		ufs_init_cpufreq_request(perf, false);

	if (op == CTRL_OP_UP) {
		if (perf->val_pm_qos_cluster0)
			freq_qos_update_request(&perf->pm_qos_cluster0, perf->val_pm_qos_cluster0);
		if (perf->val_pm_qos_cluster1)
			freq_qos_update_request(&perf->pm_qos_cluster1, perf->val_pm_qos_cluster1);
		if (perf->val_pm_qos_cluster2)
			freq_qos_update_request(&perf->pm_qos_cluster2, perf->val_pm_qos_cluster2);
	} else if (op == CTRL_OP_DOWN) {
		if (perf->val_pm_qos_cluster0)
			freq_qos_update_request(&perf->pm_qos_cluster0, 0);
		if (perf->val_pm_qos_cluster1)
			freq_qos_update_request(&perf->pm_qos_cluster1, 0);
		if (perf->val_pm_qos_cluster2)
			freq_qos_update_request(&perf->pm_qos_cluster2, 0);
	} else {
		return -1;
	}
#endif
	return 0;
}

/* policy */
static enum policy_res __policy_heavy(struct ufs_perf *perf, u32 i_policy)
{
	int index;
	enum ctrl_op op;
	unsigned long flags;
	struct ufs_perf_stat_v1 *stat = &perf->stat_v1;
	enum policy_res res = R_OK;
	int mapped = 0;

	/* for up, ORed. for down, ANDed */
	if (stat->hat_state == HAT_REACH || stat->freq_state == FREQ_REACH)
		op = CTRL_OP_UP;
	else if ((stat->hat_state == HAT_DWELL || stat->hat_state == HAT_PRE_RARE) &&
		stat->freq_state != FREQ_DROP)
		op = CTRL_OP_NONE;
	else if ((stat->freq_state == FREQ_DWELL || stat->freq_state == FREQ_PRE_RARE) &&
		stat->hat_state != HAT_DROP)
		op = CTRL_OP_NONE;
	else if ((stat->freq_state == FREQ_RARE || stat->freq_state == FREQ_DROP) &&
		stat->hat_state == HAT_DROP)
		op = CTRL_OP_DOWN;
	else if ((stat->hat_state == HAT_RARE || stat->hat_state == HAT_DROP) &&
		stat->freq_state == FREQ_DROP)
		op = CTRL_OP_DOWN;
	else
		op = CTRL_OP_NONE;

	/* put request */
	spin_lock_irqsave(&perf->lock_handle, flags);
	for (index = 0; index <= __CTRL_REQ_WB; index++) {
		if (!(BIT(index) & stat->req_bits[i_policy]) ||
		    op == CTRL_OP_NONE || perf->ctrl_handle[index] == op)
			continue;
		perf->ctrl_handle[index] = op;
		mapped++;
	}
	spin_unlock_irqrestore(&perf->lock_handle, flags);
	if (op != CTRL_OP_NONE && mapped)
		res = R_CTRL;

	return res;
}

static enum policy_res (*__policy_set[__POLICY_MAX])(struct ufs_perf *perf, u32 i_policy) = {
	__policy_heavy,
};

/* updater(stat) */
static void __transit_for_count(struct ufs_perf_stat_v1 *stat,
				enum __traffic traffic, s64 time)
{
	s64 diff;
	bool transit = false;

	/* two states are to control */
	if (stat->freq_state == FREQ_REACH) {
		stat->freq_state = FREQ_DWELL;
		transit = true;
	} else if (stat->freq_state == FREQ_DROP) {
		stat->freq_state = FREQ_RARE;
		transit = true;
	}

	if (traffic == TRAFFIC_HIGH) {
		if (stat->freq_state == FREQ_RARE) {
			stat->freq_state = FREQ_PRE_DWELL;
			transit = true;
		} else if (stat->freq_state == FREQ_PRE_DWELL) {
			diff = ktime_to_ms(ktime_sub(time,
					   stat->last_freq_transit_time));
			if (diff >= stat->th_detect_dwell) {
				stat->freq_state = FREQ_REACH;
				transit = true;
			}
		} else if (stat->freq_state == FREQ_PRE_RARE) {
			stat->freq_state = FREQ_DWELL;
			transit = true;
		} else if (stat->freq_state != FREQ_DWELL) {
			panic("%s %d 1", __func__, stat->freq_state);	//
		}
	} else if (traffic == TRAFFIC_LOW) {
		if (stat->freq_state == FREQ_PRE_DWELL) {
			stat->freq_state = FREQ_RARE;
			transit = true;
		} else if (stat->freq_state == FREQ_DWELL) {
			stat->freq_state = FREQ_PRE_RARE;
			transit = true;
		} else if (stat->freq_state == FREQ_PRE_RARE) {
			diff = ktime_to_ms(ktime_sub(time,
					   stat->last_freq_transit_time));
			if (diff >= stat->th_detect_rare) {
				stat->freq_state = FREQ_DROP;
				transit = true;
			}
		} else if (stat->freq_state != FREQ_RARE) {
			panic("%s %d 2", __func__, stat->freq_state);	//
		}
	}

	if (transit)
		stat->last_freq_transit_time = time;
}

static void __transit_for_hat(struct ufs_perf_stat_v1 *stat,
			      enum __traffic traffic, s64 time)
{
	s64 diff;
	bool transit = false;

	/* two states are to control */
	if (stat->hat_state == HAT_REACH) {
		stat->hat_state = HAT_DWELL;
		transit = true;
	} else if (stat->hat_state == HAT_DROP) {
		stat->hat_state = HAT_RARE;
		transit = true;
	}

	if (traffic == TRAFFIC_HIGH) {
		if (stat->hat_state == HAT_RARE) {
			stat->hat_state = HAT_PRE_DWELL;
			transit = true;
		} else if (stat->hat_state == HAT_PRE_DWELL) {
			diff = ktime_to_ms(ktime_sub(time,
					   stat->last_hat_transit_time));
			if (diff >= stat->th_dwell_in_high) {
				stat->hat_state = HAT_REACH;
				transit = true;
			}
			/*
			diff = ktime_to_ms(ktime_sub(time,
					   stat->last_drop_time));
			if (diff < stat->th_reach_up_to_high) {
				stat->hat_state = HAT_REACH;
				transit = true;
			}
			*/

			if (stat->seq_continue_count > 32) {
				stat->hat_state = HAT_REACH;
				transit = true;
			}
		} else if (stat->hat_state == HAT_PRE_RARE) {
			stat->hat_state = HAT_DWELL;
			transit = true;
		} else if (stat->hat_state != HAT_DWELL) {
			panic("%s %d 1", __func__, stat->hat_state);	//
		}
	} else if (traffic == TRAFFIC_LOW) {
		stat->last_drop_time = time;
		if (stat->hat_state == HAT_PRE_DWELL) {
			stat->hat_state = HAT_RARE;
			transit = true;
		} else if (stat->hat_state == HAT_DWELL) {
			stat->hat_state = HAT_PRE_RARE;
			transit = true;
		} else if (stat->hat_state == HAT_PRE_RARE) {
			diff = ktime_to_ms(ktime_sub(time,
					   stat->last_hat_transit_time));
			if (diff >= stat->th_reach_up_to_high) {
				stat->hat_state = HAT_DROP;
				transit = true;
			}
		} else if (stat->hat_state != HAT_RARE) {
			panic("%s %d 2", __func__, stat->hat_state);	//
		}
	}

	if (transit)
		stat->last_hat_transit_time = time;
}

static void __update_v1_queued(struct ufs_perf_stat_v1 *stat, u32 qd)
{
	ktime_t time = ktime_get();
	enum __traffic traffic = 0;
	s64 diff;

	/*
	 * There are only one case that we need to consider
	 *
	 * First, reset is intervened right after request done.
	 * In this situation, stat update will start again a little bit less
	 * than usual and this is not a big deal.
	 * For scenarios to require high throughput, a sort of tuning is
	 * required to prevent from frequent resets.
	 */
	del_timer(&stat->reset_timer);

	/* stats for hat */
	if (qd >= stat->th_qd_max)
		traffic = TRAFFIC_HIGH;
	else if (qd <= stat->th_qd_min)
		traffic = TRAFFIC_LOW;
	__transit_for_hat(stat, traffic, time);

	/* stats for random */
	if (stat->start_count_time == -1LL) {
		stat->start_count_time = time;
		traffic = TRAFFIC_NONE;
	} else {
		diff = ktime_to_ms(ktime_sub(time,
					stat->start_count_time));
		++stat->count;
		if (diff >= stat->th_duration) {
			if (stat->count >= stat->th_count) {
				stat->start_count_time = -1LL;
				stat->count = 0;
				traffic = TRAFFIC_HIGH;
			} else {
				stat->start_count_time = -1LL;
				stat->count = 0;
				traffic = TRAFFIC_LOW;
			}
		} else {
			traffic = TRAFFIC_NONE;
		}
	}
	__transit_for_count(stat, traffic, time);

	trace_ufs_perf_update_v1("queued", stat->count, stat->hat_state,
				 stat->freq_state,
				 ktime_to_us(ktime_sub(ktime_get(), time)));
}

static void __update_v1_reset(struct ufs_perf_stat_v1 *stat)
{
	__update_v1_queued(stat, 0);

	mod_timer(&stat->reset_timer,
		  jiffies + msecs_to_jiffies(stat->th_reset_in_ms));
}

static enum policy_res __do_policy(struct ufs_perf *perf)
{
	struct ufs_perf_stat_v1 *stat = &perf->stat_v1;
	enum policy_res res = R_OK;
	enum policy_res res_t;
	int index;

	for (index = 0; index < __POLICY_MAX; index++) {
		if (!(BIT(index) & stat->policy_bits))
			continue;
		res_t = __policy_set[index](perf, index);
		if (res_t == R_CTRL)
			res = res_t;
	}

	return res;
}

static enum policy_res __update_v1(struct ufs_perf *perf, u32 qd, enum ufs_perf_op op, enum ufs_perf_entry entry)
{
	struct ufs_perf_stat_v1 *stat = &perf->stat_v1;
	enum policy_res res = R_OK;
	unsigned long flags;

	/* sync case, freeze state for count */
	if (op == UFS_PERF_OP_S) {
		stat->start_count_time = -1LL;
		stat->count = 0;
		return res;
	}

	spin_lock_irqsave(&stat->lock, flags);
	switch (entry) {
	case UFS_PERF_ENTRY_QUEUED:
		__update_v1_queued(stat, qd);
		break;
	case UFS_PERF_ENTRY_RESET:
		__update_v1_reset(stat);
		break;
	}

	/* do policy */
	res = __do_policy(perf);
	spin_unlock_irqrestore(&stat->lock, flags);

	return res;
}

static void __reset_timer(struct timer_list *t)
{
	struct ufs_perf_stat_v1 *stat = from_timer(stat, t, reset_timer);
	struct ufs_perf *perf = container_of(stat, struct ufs_perf, stat_v1);
	ktime_t time = ktime_get();
	unsigned long flags;

	spin_lock_irqsave(&stat->lock, flags);

	stat->hat_state = HAT_DROP;
	stat->freq_state = FREQ_DROP;

	/* do policy */
	__do_policy(perf);
	spin_unlock_irqrestore(&stat->lock, flags);

	/* wake-up handler */
	ufs_perf_complete(perf);

	trace_ufs_perf_update_v1("reset", stat->count,
				stat->hat_state, stat->freq_state,
				ktime_to_us(ktime_sub(ktime_get(), time)));
}

/* sysfs*/
struct __sysfs_attr {
	struct attribute attr;
	ssize_t (*show)(struct ufs_perf_stat_v1 *stat, char *buf);
	int (*store)(struct ufs_perf_stat_v1 *stat, u32 value);
};

#define __SYSFS_NODE(_name, _format)						\
static ssize_t __sysfs_##_name##_show(struct ufs_perf_stat_v1 *stat,		\
						   char *buf)			\
{										\
	return snprintf(buf, PAGE_SIZE, _format, stat->th_##_name);		\
};										\
static int __sysfs_##_name##_store(struct ufs_perf_stat_v1 *stat, u32 value)	\
{										\
	stat->th_##_name = value;						\
										\
	return 0;								\
};										\
static struct __sysfs_attr __sysfs_node_##_name = {				\
	.attr = { .name = #_name, .mode = 0666 },				\
	.show = __sysfs_##_name##_show,						\
	.store = __sysfs_##_name##_store,					\
}										\

__SYSFS_NODE(qd_max, "%u\n");
__SYSFS_NODE(dwell_in_high, "%lld\n");
__SYSFS_NODE(reach_up_to_high, "%lld\n");
__SYSFS_NODE(duration, "%lld\n");
__SYSFS_NODE(detect_dwell, "%lld\n");
__SYSFS_NODE(detect_rare, "%lld\n");
__SYSFS_NODE(count, "%u\n");
__SYSFS_NODE(reset_in_ms, "%u\n");

const static struct attribute *__sysfs_attrs[] = {
	&__sysfs_node_qd_max.attr,
	&__sysfs_node_dwell_in_high.attr,
	&__sysfs_node_reach_up_to_high.attr,
	&__sysfs_node_duration.attr,
	&__sysfs_node_detect_dwell.attr,
	&__sysfs_node_detect_rare.attr,
	&__sysfs_node_count.attr,
	&__sysfs_node_reset_in_ms.attr,
	NULL,
};

static ssize_t __sysfs_show(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	struct ufs_perf_stat_v1 *stat = container_of(kobj, struct ufs_perf_stat_v1, sysfs_kobj);
	struct __sysfs_attr *param = container_of(attr, struct __sysfs_attr, attr);

	return param->show(stat, buf);
}

static ssize_t __sysfs_store(struct kobject *kobj,
				      struct attribute *attr,
				      const char *buf, size_t length)
{
	struct ufs_perf_stat_v1 *stat = container_of(kobj, struct ufs_perf_stat_v1, sysfs_kobj);
	struct __sysfs_attr *param = container_of(attr, struct __sysfs_attr, attr);
	u32 val;
	int ret = 0;

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	ret = param->store(stat, val);
	return (ret == 0) ? length : (ssize_t)ret;
}

static const struct sysfs_ops __sysfs_ops = {
	.show	= __sysfs_show,
	.store	= __sysfs_store,
};

static struct kobj_type __sysfs_ktype = {
	.sysfs_ops	= &__sysfs_ops,
	.release	= NULL,
};

static int __sysfs_init(struct ufs_perf_stat_v1 *stat)
{
	int error;

	/* create a path of /sys/kernel/ufs_perf_x */
	kobject_init(&stat->sysfs_kobj, &__sysfs_ktype);
	error = kobject_add(&stat->sysfs_kobj, kernel_kobj, "ufs_perf_v1_%c", (char)('0'));	//
	if (error) {
		pr_err("%s register sysfs directory: %d\n",
		       __res_token[__TOKEN_FAIL], error);
		goto fail_kobj;
	}

	/* create attributes */
	error = sysfs_create_files(&stat->sysfs_kobj, __sysfs_attrs);
	if (error) {
		pr_err("%s create sysfs files: %d\n",
		       __res_token[__TOKEN_FAIL], error);
		goto fail_kobj;
	}

	return 0;

fail_kobj:
	kobject_put(&stat->sysfs_kobj);
	return error;
}

static inline void __sysfs_exit(struct ufs_perf_stat_v1 *stat)
{
	kobject_put(&stat->sysfs_kobj);
}

int ufs_perf_init_v1(struct ufs_perf *perf)
{
	struct ufs_perf_stat_v1 *stat = &perf->stat_v1;
	int index;
	int res;

	/* register callbacks */
	perf->update[__UPDATE_V1] = __update_v1;
	perf->ctrl[__CTRL_REQ_DVFS] = __ctrl_dvfs;

	/* default thresholds for stats */
	stat->th_qd_max = 14;
	stat->th_qd_min = 2;
	stat->th_dwell_in_high = 1;
	stat->th_reach_up_to_high = 30;

	stat->th_duration = 1;
	stat->th_count = 16;
	stat->th_detect_dwell = 3;
	stat->th_detect_rare = 12;
	stat->th_reset_in_ms = 100;
	stat->seq_continue_count = 0;
	stat->last_lba = 0;

	/* init stats */
	stat->start_count_time = -1LL;

	/* enable bits */
	stat->policy_bits = POLICY_HEAVY;	//
	for (index = 0; index < __POLICY_MAX; index++) {
		if (!(BIT(index) & stat->policy_bits))
			continue;
		stat->req_bits[index] = CTRL_REQ_DVFS | CTRL_REQ_WB;	//
	}

	/* sysfs */
	res = __sysfs_init(stat);

	/* sync */
	spin_lock_init(&stat->lock);

	/* reset timer */
	timer_setup(&stat->reset_timer, __reset_timer, 0);

	return res;
}

void ufs_perf_exit_v1(struct ufs_perf *perf)
{
	struct ufs_perf_stat_v1 *stat = &perf->stat_v1;

	/* sysfs */
	__sysfs_exit(stat);
}

MODULE_DESCRIPTION("Exynos UFS performance booster");
MODULE_AUTHOR("Kiwoong Kim <kwmad.kim@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
