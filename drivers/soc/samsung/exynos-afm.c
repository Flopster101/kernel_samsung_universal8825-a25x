/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - AFM(Adaptive Frequency Manager) support
 * Auther : LEE DAEYEONG (daeyeong.lee@samsung.com)
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/io.h>
#include <linux/irq_work.h>
#include <linux/module.h>
#include <linux/cpuhotplug.h>

#include <soc/samsung/exynos-cpuhp.h>
#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/exynos-acme.h>

/* HTU */

#define OCPMCTL				(0x0)
#define OCPDEBOUNCER			(0x10)
#define OCPINTCTL			(0x20)
#define OCPTOPPWRTHRESH			(0x48)
#define OCPTHROTTCTL			(0x6c)
#define OCPTHROTTCNTA			(0x80)
#define OCPTHROTTPROFD			(0x84)
#define OCPTHROTTCNTM			(0x118)
#define GLOBALTHROTTEN			(0x200)
#define OCPFILTER			(0x300)

#define OCPMCTL_EN_VALUE		(0x1)
#define QACTIVEENABLE_CLK_VALUE		(0x1 << 31)
#define HIU1ST				(0x1 << 5)

#define OCPTHROTTERRA_VALUE		(0x1 << 27)
#define TCRST_MASK			(0x7 << 21)
#define TCRST_VALUE			(0x5 << 21)
#define TRW_VALUE			(0x2 << 3)
#define TEW_VALUE			(0x0 << 2)

#define TDT_MASK			(0xfff << 20)
#define TDT_VALUE			(0x8 << 20)
#define TDC_MASK			(0xfff << 4)
#define OCPTHROTTERRAEN_VALUE		(0x1 << 1)
#define TDCEN_VALUE			(0x1 << 0)

#define IRP_MASK			(0x3f)
#define IRP_SHIFT			(24)

#define OCPTHROTTCNTM_EN_VALUE		(0x1 << 31)
#define EN_MASK				(0x1)

#define RELEASE_MAX_LIMIT	0
#define SET_MAX_LIMIT		1

#define CLUSTER_OFF		(-1)

#define AFM_WARN_ENABLE			1
#define AFM_WARN_DISABLE		0

#define MAIN_PMIC	1
#define SUB_PMIC	2

#define BOOT_CPU	0

struct afm_stats {
	unsigned int			max_state;
	unsigned int			last_index;
	unsigned long long		last_time;
	unsigned long long		total_trans;
	unsigned int			*freq_table;
	unsigned long long		*time_in_state;
};

struct afm_domain {
	bool				enabled;
	bool				flag;

	int				irq;
	struct work_struct		work;
	struct delayed_work 		delayed_work;

	void __iomem *			base;
	unsigned int			base_addr;

	struct i2c_client		*i2c;

	struct afm_stats		*stats;

	struct cpumask			cpus;
	struct cpumask			possible_cpus;
	unsigned int			cpu;

	unsigned int			clipped_freq;
	unsigned int			max_freq;
	unsigned int			max_freq_wo_afm;
	unsigned int			min_freq;
	unsigned int			cur_freq;
	unsigned int			down_step;
	struct freq_qos_request		max_qos_req;

	unsigned int			release_duration;

	unsigned int			throttle_cnt;
	unsigned int			profile_started;
	unsigned int			s2mps_afm_offset;
	struct kobject			kobj;
	struct list_head		list;

	unsigned int			interrupt_src;
	struct cpumask			interrupt_affinity;

	int (*pmic_update_reg)(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
	int (*pmic_get_i2c)(struct i2c_client **i2c);
};

struct exynos_afm_data {
	struct list_head		afm_domain_list;
};
static struct exynos_afm_data *afm_data;
static int waiting_freq_change;
static bool ocp_reactor_disabled;

extern int main_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int main_pmic_get_i2c(struct i2c_client **i2c);
extern int sub_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);
extern int sub_pmic_get_i2c(struct i2c_client **i2c);

/****************************************************************/
/*			HELPER FUNCTION				*/
/****************************************************************/
#define SYS_READ(base, reg, val)	do { val = __raw_readl(base + reg); } while (0);
#define SYS_WRITE(base, reg, val)	do { __raw_writel(val, base + reg); } while (0);
#define S2MPS_AFM_WARN_EN_SHIFT		7

static int control_afm_warn(struct afm_domain *afm_dom, int enable)
{
	int ret;
	ret = afm_dom->pmic_update_reg(afm_dom->i2c, afm_dom->s2mps_afm_offset,
			((enable) << S2MPS_AFM_WARN_EN_SHIFT), (1 << S2MPS_AFM_WARN_EN_SHIFT));

	if (ret)
		pr_err("exynos-afm: cpu%d: Failed to control afm warn\n", afm_dom->cpu);

	return ret;
}

static int get_afm_freq_index(struct afm_stats *stats, unsigned int freq)
{
	int index;

	for (index = 0; index < stats->max_state; index++)
		if (stats->freq_table[index] == freq)
			return index;

	return -1;
}

static void update_afm_stats(struct afm_domain *dom)
{
	struct afm_stats *stats = dom->stats;
	unsigned long long cur_time = get_jiffies_64();

	/* If OCP operation is disabled, do not update OCP stats */
	if (dom->enabled == false)
		return;

	stats->time_in_state[stats->last_index] += cur_time - stats->last_time;
	stats->last_time = cur_time;
}

static unsigned int get_afm_target_max_limit(struct afm_domain *afm_dom)
{
	struct afm_stats *stats = afm_dom->stats;
	unsigned int index, ret_freq;

	/* Find the position of the current frequency in the frequency table. */
	index = get_afm_freq_index(stats, afm_dom->clipped_freq);

	/* Find target max limit that lower by "down_step" than current max limit */
	index -= afm_dom->down_step;
	if (index > stats->max_state)
		index = stats->max_state;
	ret_freq = stats->freq_table[index];

	return ret_freq;
}

static void set_afm_max_limit(struct afm_domain *afm_dom, bool set_max_limit)
{
	unsigned int target_max;

	if (afm_dom->cpu == CLUSTER_OFF) {
		dbg_snapshot_printk("OCP_cpus_off\n");
		return;
	}

	/*
	 * If the down step is greater than 0,
	 * the target max limit is set to a frequency
	 * that is lower than the current frequency by a down step.
	 * Otherwise release the target max limit to max frequency.
	 */
	if (set_max_limit) {
		target_max = get_afm_target_max_limit(afm_dom);
		if (target_max) {
			afm_dom->clipped_freq = target_max;
			pr_debug("AFM : CPU %d max limit is set to %u kHz\n",
						afm_dom->cpu, afm_dom->clipped_freq);
		} else
			return;
	} else {
		afm_dom->clipped_freq = afm_dom->max_freq;
		pr_debug("AFM : CPU %d max limit is released\n", afm_dom->cpu);
	}

	dbg_snapshot_printk("[CPU %d]AFM_enter:%ukHz(%s)\n", afm_dom->cpu,
			afm_dom->clipped_freq, afm_dom->down_step ? "throttle" : "release");
	freq_qos_update_request(&afm_dom->max_qos_req, afm_dom->clipped_freq);
	dbg_snapshot_printk("[CPU %d]AFM_exit:%ukHz(%s)\n", afm_dom->cpu,
			afm_dom->clipped_freq, afm_dom->down_step ? "throttle" : "release");

	/* Whenever afm max limit is changed, afm stats should be updated. */
	update_afm_stats(afm_dom);
	afm_dom->stats->last_index = get_afm_freq_index(afm_dom->stats, afm_dom->clipped_freq);
	afm_dom->stats->total_trans++;
}

#define CURRENT_METER_MODE		(0)
#define CPU_UTIL_MODE			(1)
#define BUCK2_COEFF			(46875)
#define ONE_HUNDRED			(100)

/*
 * Check BPC condition based on cpu load information.
 * If sum util_avg of each core is lower than configured ratio of capacity,
 * BPC condition is true.
 */
static bool is_bpc_condition(struct afm_domain *afm_dom)
{
	/* TO DO if NEEDED */
	return true;
}

static void control_afm_operation(struct afm_domain *afm_dom, bool enable)
{
	if (afm_dom->enabled == enable)
		return;

	if (afm_dom->cpu == CLUSTER_OFF) {
		pr_info("all AFM releated cpus are off\n");
		return;
	}

	/*
	 * When enabling OCP operation, first enable OCP_WARN and release OCP max limit.
	 * Conversely, when disabling OCP operation, first press OCP max limit and disable OCP_WARN.
	 */
	if (enable) {
		unsigned long long cur_time = get_jiffies_64();

		control_afm_warn(afm_dom, AFM_WARN_ENABLE);

		/* Release OCP max limit */
		afm_dom->clipped_freq = afm_dom->max_freq;
		freq_qos_update_request(&afm_dom->max_qos_req, afm_dom->clipped_freq);

		/* Re-init OCP stats */
		afm_dom->stats->last_index = get_afm_freq_index(afm_dom->stats, afm_dom->clipped_freq);
		afm_dom->stats->last_time = cur_time;
	} else {
		cancel_delayed_work_sync(&afm_dom->delayed_work);
		afm_dom->flag = false;

		/* Update OCP stats before disabling OCP operation */
		update_afm_stats(afm_dom);

		/* Press OCP max limit to max frequency without OCP */
		afm_dom->clipped_freq = afm_dom->max_freq_wo_afm;
		freq_qos_update_request(&afm_dom->max_qos_req, afm_dom->clipped_freq);

		control_afm_warn(afm_dom, AFM_WARN_DISABLE);
	}

	afm_dom->enabled = enable;

	pr_info("AFM operation is %s\n", (enable)?"enabled":"disabled");
	dbg_snapshot_printk("AFM_%s\n", (enable)?"enabled":"disabled");
}

/****************************************************************/
/*			AFM PROFILER				*/
/****************************************************************/
static void afm_profile_start_ipi(void *arg)
{
	struct afm_domain *afm_dom = arg;

	SYS_WRITE(afm_dom->base, OCPTHROTTCNTM, (1 << 31));
}

static void afm_profile_end_ipi(void *arg)
{
	struct afm_domain *afm_dom = arg;

	SYS_READ(afm_dom->base, OCPTHROTTCNTM, afm_dom->throttle_cnt);
	afm_dom->throttle_cnt = afm_dom->throttle_cnt & ~(1 << 31);
}

static void afm_profile_start(struct afm_domain *afm_dom)
{
	if (afm_dom->profile_started) {
		pr_err("AFM: CPU %d : profile is ongoing\n", afm_dom->cpu);
		return;
	}

	afm_dom->profile_started = true;

	smp_call_function_any(&afm_dom->cpus, afm_profile_start_ipi, afm_dom, 1);

	pr_info("AFM: CPU %d : Profile started\n", afm_dom->cpu);
}

static void afm_profile_end(struct afm_domain *afm_dom)
{
	if (!afm_dom->profile_started) {
		pr_err("AFM: CPU %d : Profile is not ongoing\n", afm_dom->cpu);
		return;
	}
	afm_dom->profile_started = false;

	smp_call_function_any(&afm_dom->cpus, afm_profile_end_ipi, afm_dom, 1);

	pr_info("AFM: CPU %d : Profile finished", afm_dom->cpu);
}

/****************************************************************/
/*			OCP INTERRUPT HANDLER			*/
/****************************************************************/
#define SWI_ENABLE			(1)
#define SWI_DISABLE			(0)
#define POLL_PERIOD			(1000)

static void control_afm_interrupt(struct afm_domain *afm_dom, int enable)
{
	int val;

	SYS_READ(afm_dom->base, OCPTHROTTCNTA, val);

	if (enable)
		val |= OCPTHROTTERRAEN_VALUE;
	else
		val &= ~OCPTHROTTERRAEN_VALUE;

	SYS_WRITE(afm_dom->base, OCPTHROTTCNTA, val);
}

static void check_ocp_reactor_disabled(void *arg)
{
	struct afm_domain *afm_dom = arg;
	int val;

	SYS_READ(afm_dom->base, OCPMCTL, val);
	if (!(val & EN_MASK))
		ocp_reactor_disabled = true;
}

static void init_htu_register_ipi(void *arg)
{
	struct afm_domain *afm_dom = arg;
	int irp, val;
	int reactor;

	irp = (afm_dom->max_freq * 2) / afm_dom->min_freq - 2;
	if (irp > IRP_MASK)
		irp = IRP_MASK;

	SYS_READ(afm_dom->base, OCPTOPPWRTHRESH, val);
	val &= ~(IRP_MASK << IRP_SHIFT);
	val |= (irp << IRP_SHIFT);

	/* disable OCP Controller before change IRP */
	SYS_READ(afm_dom->base, OCPMCTL, reactor);
	reactor &= ~OCPMCTL_EN_VALUE;
	SYS_WRITE(afm_dom->base, OCPMCTL, reactor);

	SYS_WRITE(afm_dom->base, OCPTOPPWRTHRESH, val);

	/* enable OCP controller after change IRP */
	reactor |= OCPMCTL_EN_VALUE;
	SYS_WRITE(afm_dom->base, OCPMCTL, reactor);

	control_afm_interrupt(afm_dom, SWI_ENABLE);
}

static int check_afm_interrupt(struct afm_domain *afm_dom)
{
	int val;

	SYS_READ(afm_dom->base, OCPTHROTTCTL, val);

	val = val & OCPTHROTTERRA_VALUE;

	return val;
}

static void clear_afm_interrupt(struct afm_domain *afm_dom)
{
	int val;

	SYS_READ(afm_dom->base, OCPTHROTTCTL, val);
	val |= OCPTHROTTERRA_VALUE;
	SYS_WRITE(afm_dom->base, OCPTHROTTCTL, val);
	val &= ~OCPTHROTTERRA_VALUE;
	SYS_WRITE(afm_dom->base, OCPTHROTTCTL, val);
}

static void clear_afm_throttling_duration_counter(struct afm_domain *afm_dom)
{
	int val;

	SYS_READ(afm_dom->base, OCPTHROTTCNTA, val);
	val &= ~TDC_MASK;
	SYS_WRITE(afm_dom->base, OCPTHROTTCNTA, val);
}

static void exynos_afm_work(struct work_struct *work)
{
	struct afm_domain *afm_dom = container_of(work, struct afm_domain, work);

	if (!cpumask_test_cpu(smp_processor_id(), &afm_dom->cpus))
		return;

	afm_dom->flag = true;
	set_afm_max_limit(afm_dom, SET_MAX_LIMIT);

	if (waiting_freq_change)
		usleep_range(POLL_PERIOD, 2 * POLL_PERIOD);

	/* Before enabling OCP interrupt, clear releated register fields */
	clear_afm_throttling_duration_counter(afm_dom);
	clear_afm_interrupt(afm_dom);

	/* After finish interrupt handling, enable OCP interrupt. */
	control_afm_interrupt(afm_dom, SWI_ENABLE);

	cancel_delayed_work_sync(&afm_dom->delayed_work);
	schedule_delayed_work(&afm_dom->delayed_work, msecs_to_jiffies(afm_dom->release_duration));
}

static void exynos_afm_work_release(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct afm_domain *afm_dom = container_of(dw, struct afm_domain, delayed_work);

	/*
	 * If BPC condition is true, release afm max limit.
	 * Otherwise extend afm max limit as release duration.
	 */
	if (is_bpc_condition(afm_dom)) {
		afm_dom->flag = false;
		set_afm_max_limit(afm_dom, RELEASE_MAX_LIMIT);
	}
	else
		schedule_delayed_work(&afm_dom->delayed_work, msecs_to_jiffies(afm_dom->release_duration));
}

static irqreturn_t exynos_afm_irq_handler(int irq, void *id)
{
	struct afm_domain *afm_dom;
	bool irq_handled = false;

	if (ocp_reactor_disabled)
		return IRQ_NONE;

	list_for_each_entry(afm_dom, &afm_data->afm_domain_list, list) {
		if (afm_dom->irq != irq)
			continue;

		/* Check the source of interrupt */
		if (!check_afm_interrupt(afm_dom))
			continue;

		if (afm_dom->cpu != CLUSTER_OFF) {
			/* Before start interrupt handling, disable OCP interrupt. */
			control_afm_interrupt(afm_dom, SWI_DISABLE);
			schedule_work_on(afm_dom->cpu, &afm_dom->work);
		}

		clear_afm_throttling_duration_counter(afm_dom);
		clear_afm_interrupt(afm_dom);
		irq_handled = true;
	}

	if (!irq_handled)
		return IRQ_NONE;

	return IRQ_HANDLED;
}

/****************************************************************/
/*			EXTERNAL EVENT HANDLER			*/
/****************************************************************/
static int exynos_afm_cpuhp_callback(unsigned int cpu, bool up)
{
	bool afm_dom_found = false;
	struct cpumask mask;
	struct afm_domain *afm_dom;

	if (ocp_reactor_disabled)
		return 0;

	list_for_each_entry(afm_dom, &afm_data->afm_domain_list, list)
		if (cpumask_test_cpu(cpu, &afm_dom->cpus)) {
			afm_dom_found = true;
			break;
		}

	if (!afm_dom_found)
		return 0;

	cpumask_and(&mask, &afm_dom->cpus, cpu_online_mask);
	if (up) {
		/*
		 * The first incomming cpu in afm data binds
		 * afm interrupt on data->cpus.
		 */
		if (cpumask_weight(&mask) == 1) {
			afm_dom->cpu = cpu;
			irq_set_affinity_hint(afm_dom->irq, &afm_dom->interrupt_affinity);
			control_afm_interrupt(afm_dom, SWI_ENABLE);
		}
	} else {
		cpumask_clear_cpu(cpu, &mask);
		/* If all data->cpus off, update data->cpu as CLUSTER_OFF */
		if (cpumask_empty(&mask))
			afm_dom->cpu = CLUSTER_OFF;
		else
			afm_dom->cpu = cpumask_any(&mask);
	}

	return 0;
}

static int exynos_afm_cpu_up_callback(unsigned int cpu)
{
	return exynos_afm_cpuhp_callback(cpu, true);
}

static int exynos_afm_cpu_down_callback(unsigned int cpu)
{
	return exynos_afm_cpuhp_callback(cpu, false);
}

unsigned int get_afm_clipped_freq(int cpu)
{
	bool afm_dom_found = false;
	struct afm_domain *afm_dom;

	if (ocp_reactor_disabled)
		return 0;

	list_for_each_entry(afm_dom, &afm_data->afm_domain_list, list)
		if (cpumask_test_cpu(cpu, &afm_dom->possible_cpus)) {
			afm_dom_found = true;
			break;
		}

	if (!afm_dom_found)
		return 0;

	return afm_dom->clipped_freq;
}
EXPORT_SYMBOL(get_afm_clipped_freq);

/****************************************************************/
/*			SYSFS INTERFACE				*/
/****************************************************************/
struct afm_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

#define to_afm_dom(k) container_of(k, struct afm_domain, kobj)

#define AFM_ATTR_RW(name)								\
struct afm_attr afm_attr_##name =							\
__ATTR(name, 0644, afm_##name##_show, afm_##name##_store)

#define AFM_ATTR_RO(name)								\
struct afm_attr afm_attr_##name =							\
__ATTR(name, 0444, afm_##name##_show, NULL)

#define afm_show(name, type)								\
static ssize_t afm_##name##_show(struct kobject *kobj, char *buf)			\
{											\
	int ret;									\
	struct afm_domain *afm_dom = to_afm_dom(kobj);					\
											\
	ret = snprintf(buf, PAGE_SIZE, "%d\n", afm_dom->type);				\
											\
	return ret;									\
}

#define afm_store(name, type)								\
static ssize_t afm_##name##_store(struct kobject *kobj, const char *buf, size_t count)	\
{											\
	unsigned int val;								\
	struct afm_domain *afm_dom = to_afm_dom(kobj);					\
											\
	if (sscanf(buf, "%d", &val) != 1)						\
		return -EINVAL;								\
											\
	afm_dom->type = val;								\
											\
	return count;									\
}

afm_show(flag, flag);

afm_store(down_step, down_step);
afm_show(down_step, down_step);

afm_store(release_duration, release_duration);
afm_show(release_duration, release_duration);

afm_show(clipped_freq, clipped_freq);

static ssize_t
afm_enable_show(struct kobject *kobj, char *buf)
{
	int ret = 0;
	struct afm_domain *afm_dom = to_afm_dom(kobj);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", afm_dom->enabled);

	return ret;
}

static ssize_t
afm_enable_store(struct kobject *kobj, const char *buf, size_t count)
{
	unsigned int val;
	struct afm_domain *afm_dom = to_afm_dom(kobj);

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	control_afm_operation(afm_dom, val);

	return count;
}

static ssize_t
afm_total_trans_show(struct kobject *kobj, char *buf)
{
	int ret;
	struct afm_domain *afm_dom = to_afm_dom(kobj);

	ret = snprintf(buf, PAGE_SIZE, "%llu\n", afm_dom->stats->total_trans);

	return ret;
}

static ssize_t
afm_time_in_state_show(struct kobject *kobj, char *buf)
{
	struct afm_domain *afm_dom = to_afm_dom(kobj);
	struct afm_stats *stats = afm_dom->stats;
	ssize_t len = 0;
	int i;

	update_afm_stats(afm_dom);

	for (i = 0; i < afm_dom->stats->max_state; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%u %llu\n",
				stats->freq_table[i],
				(unsigned long long)jiffies_64_to_clock_t(stats->time_in_state[i]));
	}

	return len;
}

static ssize_t
afm_profile_show(struct kobject *kobj, char *buf)
{
	struct afm_domain *afm_dom = to_afm_dom(kobj);
	int ret;

	if (afm_dom->profile_started)
		ret = snprintf(buf, PAGE_SIZE, "profile is ongoing\n");
	else
		ret = snprintf(buf, PAGE_SIZE, "%d\n", afm_dom->throttle_cnt);

	return ret;
}

static ssize_t
afm_profile_store(struct kobject *kobj, const char *buf, size_t count)
{
	unsigned int val;
	struct afm_domain *afm_dom = to_afm_dom(kobj);
	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	if (val)
		afm_profile_start(afm_dom);
	else
		afm_profile_end(afm_dom);

	return count;
}

static AFM_ATTR_RW(enable);
static AFM_ATTR_RO(flag);
static AFM_ATTR_RW(down_step);
static AFM_ATTR_RW(release_duration);
static AFM_ATTR_RO(clipped_freq);
static AFM_ATTR_RO(total_trans);
static AFM_ATTR_RO(time_in_state);
static AFM_ATTR_RW(profile);

static struct attribute *exynos_afm_attrs[] = {
	&afm_attr_enable.attr,
	&afm_attr_flag.attr,
	&afm_attr_down_step.attr,
	&afm_attr_release_duration.attr,
	&afm_attr_clipped_freq.attr,
	&afm_attr_total_trans.attr,
	&afm_attr_time_in_state.attr,
	&afm_attr_profile.attr,
	NULL,
};

static ssize_t show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct afm_attr *fvattr = container_of(at, struct afm_attr, attr);
	return fvattr->show(kobj, buf);
}

static ssize_t store(struct kobject *kobj, struct attribute *at,
					const char *buf, size_t count)
{
	struct afm_attr *fvattr = container_of(at, struct afm_attr, attr);
	return fvattr->store(kobj, buf, count);
}

static const struct sysfs_ops afm_sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct kobj_type ktype_afm = {
	.sysfs_ops	= &afm_sysfs_ops,
	.default_attrs	= exynos_afm_attrs,
};

/****************************************************************/
/*		INITIALIZE EXYNOS AFM DRIVER			*/
/****************************************************************/
static int alloc_pmic_function(struct afm_domain *afm_dom)
{
	int ret = 0;
	switch (afm_dom->interrupt_src) {
		case MAIN_PMIC:
			afm_dom->pmic_update_reg = main_pmic_update_reg;
			afm_dom->pmic_get_i2c = main_pmic_get_i2c;
			break;
		case SUB_PMIC:
			afm_dom->pmic_update_reg = sub_pmic_update_reg;
			afm_dom->pmic_get_i2c = sub_pmic_get_i2c;
			break;
		default:
			pr_err("exynos-afm: cpu%d: Failed to init pmic function", afm_dom->cpu);
			ret = -ENODEV;
			break;
	}

	/*
	 * Regulator could be unloaded when probing afm driver, and if so
	 * fail to probe it. Let's check whether regulator is loaded
	 * and if not, defer probing afm driver.
	 */
	if (afm_dom->pmic_get_i2c(&afm_dom->i2c)) {
		pr_err("exynos-afm: cpu%d: Failed to load regulator", afm_dom->cpu);
		return -ENODEV;
	}

	return ret;
}

static int afm_dt_parsing(struct device_node *dn, struct afm_domain *afm_dom)
{
	const char *buf, *buf2;
	int ret = 0;

	ret |= of_property_read_u32(dn, "down-step", &afm_dom->down_step);
	ret |= of_property_read_u32(dn, "max-freq-wo-afm", &afm_dom->max_freq_wo_afm);
	ret |= of_property_read_u32(dn, "release-duration", &afm_dom->release_duration);
	ret |= of_property_read_u32(dn, "s2mps-afm-offset", &afm_dom->s2mps_afm_offset);
	ret |= of_property_read_u32(dn, "htu-base-addr", &afm_dom->base_addr);
	ret |= of_property_read_u32(dn, "interrupt-src", &afm_dom->interrupt_src);

	ret |= of_property_read_string(dn, "sibling-cpus", &buf);
	if (ret)
		return ret;

	cpulist_parse(buf, &afm_dom->cpus);
	cpumask_and(&afm_dom->possible_cpus, &afm_dom->cpus, cpu_possible_mask);
	cpumask_and(&afm_dom->cpus, &afm_dom->cpus, cpu_possible_mask);
	cpumask_and(&afm_dom->cpus, &afm_dom->cpus, cpu_online_mask);
	afm_dom->cpu = cpumask_first(&afm_dom->cpus);

	if (of_property_read_string(dn, "interrupt-affinity", &buf2))
		cpumask_copy(&afm_dom->interrupt_affinity, &afm_dom->cpus);
	else
		cpulist_parse(buf2, &afm_dom->interrupt_affinity);

	if (alloc_pmic_function(afm_dom))
		return -ENODEV;

	if (cpumask_weight(&afm_dom->cpus) == 0) {
		control_afm_warn(afm_dom, AFM_WARN_DISABLE);
		return -ENODEV;
	}

	return 0;
}

static void
afm_stats_create_table(struct afm_domain *afm_dom, struct cpufreq_policy *policy)
{
	unsigned int i = 0, count = 0, alloc_size;
	struct afm_stats *stats;
	struct cpufreq_frequency_table *pos, *table;

	if (unlikely(!afm_dom))
		return;

	if (afm_dom && afm_dom->stats)
		return;

	table = policy->freq_table;
	if (unlikely(!table))
		return;

	stats = kzalloc(sizeof(*stats), GFP_KERNEL);
	if (!stats)
		return;

	cpufreq_for_each_valid_entry(pos, table)
		count++;

	alloc_size = count * sizeof(int) + count * sizeof(u64);

	stats->time_in_state = kzalloc(alloc_size, GFP_KERNEL);
	if (!stats->time_in_state)
		goto free_stat;

	stats->freq_table = (unsigned int *)(stats->time_in_state + count);

	stats->max_state = count;

	cpufreq_for_each_valid_entry(pos, table)
		stats->freq_table[i++] = pos->frequency;

	stats->last_time = get_jiffies_64();
	stats->last_index = get_afm_freq_index(stats, afm_dom->clipped_freq);

	afm_dom->stats = stats;
	return;
free_stat:
	kfree(stats);
}

static int init_afm_domain(struct device_node *dn, struct platform_device *pdev)
{
	struct afm_domain *afm_dom;
	struct cpufreq_policy *policy;
	int ret;

	afm_dom = kzalloc(sizeof(struct afm_domain), GFP_KERNEL);
	if (!afm_dom)
		return -ENOMEM;

	INIT_LIST_HEAD(&afm_dom->list);
	list_add(&afm_dom->list, &afm_data->afm_domain_list);

	ret = afm_dt_parsing(dn, afm_dom);
	if (ret) {
		dev_err(&pdev->dev, "Failed to parse AFM data\n");
		return -ENODEV;
	}

	afm_dom->base = ioremap(afm_dom->base_addr, SZ_64K);
	if (!afm_dom->i2c) {
		dev_err(&pdev->dev, "Failed to get s2mps19 i2c_client\n");
		return -ENODEV;
	}

	smp_call_function_any(&afm_dom->cpus, check_ocp_reactor_disabled, afm_dom, 1);

	if (ocp_reactor_disabled) {
		dev_err(&pdev->dev, "OCP reactor is disabled\n");
		return -ENODEV;
	}

	INIT_WORK(&afm_dom->work, exynos_afm_work);
	INIT_DELAYED_WORK(&afm_dom->delayed_work, exynos_afm_work_release);

	if (kobject_init_and_add(&afm_dom->kobj, &ktype_afm,
				&pdev->dev.kobj, "cpu%d", afm_dom->cpu)) {
		dev_err(&pdev->dev, "Failed to init sysfs\n");
		return -ENOMEM;
	}

	policy = cpufreq_cpu_get(cpumask_first(&afm_dom->cpus));
	if (!policy) {
		dev_err(&pdev->dev, "Failed to get policy\n");
		return -ENODEV;
	}

	afm_dom->enabled = true;
	afm_dom->flag = false;
	afm_dom->min_freq = policy->cpuinfo.min_freq;
	afm_dom->max_freq = policy->cpuinfo.max_freq;
	afm_dom->clipped_freq = afm_dom->max_freq;
	afm_stats_create_table(afm_dom, policy);
	smp_call_function_any(&afm_dom->cpus, init_htu_register_ipi, afm_dom, 1);

	ret = freq_qos_tracer_add_request(&policy->constraints, &afm_dom->max_qos_req,
				FREQ_QOS_MAX, afm_dom->clipped_freq);
	if (ret < 0)
		return ret;

	afm_dom->irq = irq_of_parse_and_map(dn, 0);
	if (afm_dom->irq <= 0) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		return -ENODEV;
	}

	ret = devm_request_irq(&pdev->dev, afm_dom->irq, exynos_afm_irq_handler,
			IRQF_TRIGGER_HIGH | IRQF_SHARED, dev_name(&pdev->dev), afm_data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ handler: %d\n", afm_dom->irq);
		return -ENODEV;
	}

	irq_set_affinity_hint(afm_dom->irq, &afm_dom->interrupt_affinity);

	cpufreq_cpu_put(policy);

	pr_info("exynos-afm: AFM data(cpu%d) structure update complete\n", afm_dom->cpu);

	return 0;
}

static int check_regulator(void)
{
	struct i2c_client *regulator_i2c = NULL;

	if (main_pmic_get_i2c(&regulator_i2c))
		return -ENODEV;
	if (sub_pmic_get_i2c(&regulator_i2c))
		return -ENODEV;

	return 0;
}

static int exynos_afm_suspend(struct device *dev)
{
	return 0;
}

static int exynos_afm_resume(struct device *dev)
{
	struct afm_domain *afm_dom;
	bool afm_dom_found = false;

	if (ocp_reactor_disabled)
		return 0;

	list_for_each_entry(afm_dom, &afm_data->afm_domain_list, list)
		if (cpumask_test_cpu(BOOT_CPU, &afm_dom->cpus)) {
			afm_dom_found = true;
			break;
		}
	if (!afm_dom_found)
		return 0;

	irq_set_affinity_hint(afm_dom->irq, &afm_dom->interrupt_affinity);
	control_afm_interrupt(afm_dom, SWI_ENABLE);

	return 0;
}

static int exynos_afm_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node, *child;

	if (check_regulator())
		return -EPROBE_DEFER;

	afm_data = kzalloc(sizeof(struct exynos_afm_data), GFP_KERNEL);
	if (!afm_data)
		return -ENOMEM;

	INIT_LIST_HEAD(&afm_data->afm_domain_list);

	if (of_property_read_bool(dn, "waiting-freq-change")) {
		waiting_freq_change = 1;
		pr_info("exynos-afm: enable waiting-freq-change");
	}

	for_each_child_of_node(dn, child) {
		if (init_afm_domain(child, pdev)) {
			pr_err("exynos-afm: Failed to init init_afm_domain\n");
			goto free_data;
		}
	}

	platform_set_drvdata(pdev, afm_data);

	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
					"exynos:afm",
					exynos_afm_cpu_up_callback,
					exynos_afm_cpu_down_callback);

	dev_info(&pdev->dev, "Complete AFM Handler initialization\n");
	return 0;

free_data:
	kfree(afm_data);
	return -ENODEV;
}

static const struct of_device_id of_exynos_afm_match[] = {
	{ .compatible = "samsung,exynos-afm", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_afm_match);

static const struct dev_pm_ops exynos_afm_pm_ops = {
	.suspend	= exynos_afm_suspend,
	.resume		= exynos_afm_resume,
};

static struct platform_driver exynos_afm_driver = {
	.driver = {
		.name = "exynos-afm",
		.owner = THIS_MODULE,
		.pm    = &exynos_afm_pm_ops,
		.of_match_table = of_exynos_afm_match,
	},
	.probe		= exynos_afm_probe,
};

module_platform_driver(exynos_afm_driver);

MODULE_SOFTDEP("pre: exynos-acme");
MODULE_DESCRIPTION("Exynos AFM drvier");
MODULE_LICENSE("GPL");
