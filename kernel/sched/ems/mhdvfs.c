/*
 * Multi-purpose Load tracker
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include "../sched.h"
#include "../sched-pelt.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

static void (*dsufreq_callback)(int ratio, int period_ns);
static void (*miffreq_callback)(int ratio);
static void (*cpufreq_callback)(int *ratio);

void register_mhdvfs_dsufreq_callback(void (*callback)(int ratio, int period_ns))
{
	dsufreq_callback = callback;
}
EXPORT_SYMBOL(register_mhdvfs_dsufreq_callback);

void register_mhdvfs_miffreq_callback(void (*callback)(int ratio))
{
	miffreq_callback = callback;
}
EXPORT_SYMBOL(register_mhdvfs_miffreq_callback);

void register_mhdvfs_cpufreq_callback(void (*callback)(int *ratio))
{
	cpufreq_callback = callback;
}
EXPORT_SYMBOL(register_mhdvfs_cpufreq_callback);

static int mhdvfs_interval_jiffies = 5;
static u64 last_mhdvfs_time;
static int target_mspc = 1500;
static int memstall_duration_jiffies = 10;
static u64 memstall_start_time;
static int low_perf_ipc = 1000;

struct {
	struct work_struct	work;

	int			dsu_ratio;
	int			mif_ratio;
	int			*cpu_ratio;
} mhdvfs_update;

static void mhdvfs_update_work(struct work_struct *work)
{
	/* call dsufreq callback */
	if (!dsufreq_callback)
		return;

	dsufreq_callback(mhdvfs_update.dsu_ratio,
			mhdvfs_interval_jiffies * TICK_NSEC);

	/* call miffreq callback */
	if (!miffreq_callback)
		return;

	miffreq_callback(mhdvfs_update.mif_ratio);

	/* call cpufreq callback */
	if (!cpufreq_callback)
		return;

	cpufreq_callback(mhdvfs_update.cpu_ratio);
}

static void kick_mhdvfs_update(int dsu_ratio, int mif_ratio, int *cpu_ratio)
{
	if (work_busy(&mhdvfs_update.work))
		return;

	mhdvfs_update.dsu_ratio = dsu_ratio;
	mhdvfs_update.mif_ratio = mif_ratio;
	mhdvfs_update.cpu_ratio = cpu_ratio;

	schedule_work_on(smp_processor_id(), &mhdvfs_update.work);
}

struct uarch_info {
	u64 core_cycles;
	u64 inst_ret;
	u64 mem_stall;

	u64 last_core_cycles;
	u64 last_inst_ret;
	u64 last_mem_stall;
};

static struct uarch_info __percpu *pcpu_uinfo;

static void this_cpu_uarch_info_update(void)
{
	/* Uarch information can be accessed by only this cpu */
	struct uarch_info *uinfo = per_cpu_ptr(pcpu_uinfo, smp_processor_id());

	uinfo->core_cycles = amu_core_cycles();
	uinfo->inst_ret = amu_inst_ret();
	uinfo->mem_stall = amu_mem_stall();
}

static DEFINE_SPINLOCK(mhdvfs_lock);

void mhdvfs(void)
{
	unsigned long now = jiffies;
	u64 ipc[VENDOR_NR_CPUS], mspc[VENDOR_NR_CPUS];
	u64 ipc_sum = 0, mspc_sum = 0;
	int dsu_ratio = 0, mif_ratio = 0;
	int cpu_ratio[VENDOR_NR_CPUS] = { 0, };
	int cpu;

	this_cpu_uarch_info_update();

	if (!spin_trylock(&mhdvfs_lock))
		return;

	if (now < last_mhdvfs_time + mhdvfs_interval_jiffies) {
		spin_unlock(&mhdvfs_lock);
		return;
	}

	last_mhdvfs_time = now;

	/* Gather uarch infomration */
	for_each_cpu(cpu, cpu_active_mask) {
		struct uarch_info *uinfo = per_cpu_ptr(pcpu_uinfo, cpu);

		ipc[cpu] = div64_u64((uinfo->inst_ret - uinfo->last_inst_ret) << 10,
				uinfo->core_cycles - uinfo->last_core_cycles);
		mspc[cpu] = div64_u64((uinfo->mem_stall - uinfo->last_mem_stall) << 10,
				uinfo->core_cycles - uinfo->last_core_cycles);

		ipc_sum += ipc[cpu];
		mspc_sum += mspc[cpu];

		uinfo->last_core_cycles = uinfo->core_cycles;
		uinfo->last_inst_ret = uinfo->inst_ret;
		uinfo->last_mem_stall = uinfo->mem_stall;
	}

	trace_mhdvfs_profile(ipc_sum, mspc_sum);

	spin_unlock(&mhdvfs_lock);
	return;

	if (mspc_sum > target_mspc) {
		struct cpumask cpus_to_visit;

		/*
		 * MSPC and DSU frequency are inverse proportion.
		 *
		 *   f = 1 / mspc
		 *
		 * f and mspc are the current frequency and mspc, f' is
		 * the frequency to reach the target_mspc. Through relation
		 * of f, f', mspc and target_mspc, we can get f'.
		 *
		 *   f : 1 / mspc = f' : 1 / target_mspc
		 *      f' / mspc = f / target_mspc
		 *             f' = f * mspc / target_mspc
		 *
		 * That is, if the current frequency is raised by
		 * (mspc / target_mspc), mspc is lowered to target_mspc.
		 * We therefore calculate dsu_ratio for DSU frequency
		 * as below:
		 *
		 *          dsu_ratio = mspc / target_mspc
		 */
		dsu_ratio = div64_u64(mspc_sum << 10, target_mspc);
		trace_mhdvfs_kick_dsufreq(mspc_sum, dsu_ratio);

		/*
		 * Raises the MIF frequency to break the bottleneck of the
		 * memory when MSPC continues to be high for longer than
		 * memstall_duration_jiffies.
		 */
		if (!memstall_start_time)
			memstall_start_time = now;
		else if (now >= memstall_start_time + memstall_duration_jiffies) {
			/*
			 * It is difficult to predict the exact required
			 * performance of memory since memory is not used only
			 * by the CPU. Therefore, we set MIF frequency to
			 * maximum to relieve memstall as quickly as possible.
			 */
			mif_ratio = UINT_MAX;	/* max freq */
			trace_mhdvfs_kick_miffreq(mspc_sum, mif_ratio,
				div64_u64((now - memstall_start_time)
					* TICK_NSEC, NSEC_PER_MSEC));
		}

		/*
		 * Lowers CPU frequency to reduce unnecessary CPU power when
		 * IPC is lower than low_perf_ipc and MSPC is higher than
		 * target_mspc.
		 */
		cpumask_copy(&cpus_to_visit, cpu_active_mask);
		while (!cpumask_empty(&cpus_to_visit)) {
			struct cpufreq_policy *policy;
			u64 ipc_sum_cpus = 0;
			int i;

			cpu = cpumask_first(&cpus_to_visit);
			policy = cpufreq_cpu_get(cpu);
			if (!policy)
				break;

			for_each_cpu(i, policy->cpus)
				ipc_sum_cpus += ipc[i];

			if (ipc_sum_cpus < low_perf_ipc) {
				cpu_ratio[policy->cpu] = 768;	/* 75% */

				trace_mhdvfs_kick_cpufreq(cpu, ipc_sum_cpus,
						cpu_ratio[policy->cpu]);
			}

			cpumask_andnot(&cpus_to_visit, &cpus_to_visit,
							policy->cpus);
			cpufreq_cpu_put(policy);
		}
	} else
		memstall_start_time = 0;

	kick_mhdvfs_update(dsu_ratio, mif_ratio, cpu_ratio);

	spin_unlock(&mhdvfs_lock);
}

static ssize_t show_amu_count(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct uarch_info *uinfo;
	int cpu;
	int ret = 0;

	ret += snprintf(buf + ret,
			PAGE_SIZE - ret, "cpu       core_cycle     inst_ret_cnt    mem_stall_cnt\n");

	for_each_possible_cpu(cpu) {
		uinfo = per_cpu_ptr(pcpu_uinfo, cpu);
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret, "%2d  %16llu %16llu %16llu\n",
				cpu, uinfo->core_cycles, uinfo->inst_ret, uinfo->mem_stall);
	}

	return ret;
}

static DEVICE_ATTR(amu_count, S_IRUGO, show_amu_count, NULL);

void mhdvfs_init(struct kobject *ems_kobj)
{
	pcpu_uinfo = alloc_percpu(struct uarch_info);

	INIT_WORK(&mhdvfs_update.work, mhdvfs_update_work);

	if (sysfs_create_file(ems_kobj, &dev_attr_amu_count.attr))
		pr_warn("Failed to create amu_count node\n");
}
