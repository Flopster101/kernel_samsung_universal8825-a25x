// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>
#include <linux/delay.h>

#include "dsp-log.h"
#include "dsp-hw-common-system.h"
#include "dsp-hw-common-pm.h"

#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
#include <soc/samsung/freq-qos-tracer.h>
#else
#define freq_qos_tracer_add_request(a, b, c, d)
#define freq_qos_tracer_remove_request(a)
#endif

#define DSP_PM_TOKEN_WAIT_COUNT		(500)

#ifdef ENABLE_DSP_VELOCE
static int pm_for_veloce;
#endif

int dsp_hw_common_pm_devfreq_active(struct dsp_pm *pm)
{
	dsp_check();
#ifndef ENABLE_DSP_VELOCE
	return pm_runtime_active(pm->dev);
#else
	return pm_for_veloce;
#endif
}

static int __dsp_hw_common_pm_check_valid(struct dsp_pm_devfreq *devfreq,
		int val)
{
	dsp_check();
	if (val < 0 || val > devfreq->min_qos) {
		dsp_err("devfreq[%s] value(%d) is invalid(L0 ~ L%u)\n",
				devfreq->name, val, devfreq->min_qos);
		return -EINVAL;
	} else {
		return 0;
	}
}

static int __dsp_hw_common_pm_update_devfreq_raw(struct dsp_pm_devfreq *devfreq,
		int val)
{
	int ret, req_val;

	dsp_enter();
	if (val == DSP_GOVERNOR_DEFAULT)
		req_val = devfreq->min_qos;
	else
		req_val = val;

	ret = __dsp_hw_common_pm_check_valid(devfreq, req_val);
	if (ret)
		goto p_err;

#ifndef ENABLE_DSP_VELOCE
	if (devfreq->current_qos != req_val) {
		if (devfreq->use_freq_qos) {
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
			freq_qos_update_request(&devfreq->freq_qos_req,
					devfreq->table[req_val]);
#endif
		} else {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
			exynos_pm_qos_update_request(&devfreq->req,
					devfreq->table[req_val]);
#else
			pm_qos_update_request(&devfreq->req,
					devfreq->table[req_val]);
#endif
		}
		dsp_dbg("devfreq[%s] is changed from L%d to L%d\n",
				devfreq->name, devfreq->current_qos, req_val);
		devfreq->current_qos = req_val;
	} else {
		dsp_dbg("devfreq[%s] is already running L%d\n",
				devfreq->name, req_val);
	}
#else
	dsp_dbg("devfreq[%s] is not supported\n", devfreq->name);
#endif

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_common_pm_devfreq_get_token(struct dsp_pm_devfreq *devfreq,
		bool async)
{
	int repeat = DSP_PM_TOKEN_WAIT_COUNT;

	dsp_enter();
	while (repeat) {
		spin_lock(&devfreq->slock);
		if (devfreq->async_status < 0) {
			devfreq->async_status = async;
			spin_unlock(&devfreq->slock);
			dsp_dbg("get devfreq[%s/%d] token(wait count:%d/%d)\n",
					devfreq->name, async, repeat,
					DSP_PM_TOKEN_WAIT_COUNT);
			dsp_leave();
			return 0;
		}

		spin_unlock(&devfreq->slock);
		udelay(10);
		repeat--;
	}

	dsp_err("other asynchronous work is not done [%s]\n", devfreq->name);
	return -ETIMEDOUT;
}

static void __dsp_hw_common_pm_devfreq_put_token(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	spin_lock(&devfreq->slock);
	if (devfreq->async_status < 0)
		dsp_warn("devfreq token is invalid(%s/%d)\n",
				devfreq->name, devfreq->async_status);

	devfreq->async_status = -1;
	dsp_dbg("put devfreq[%s] token\n", devfreq->name);
	spin_unlock(&devfreq->slock);
	dsp_leave();
}

static int __dsp_hw_common_pm_update_devfreq(struct dsp_pm_devfreq *devfreq,
		int val)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_common_pm_update_devfreq_raw(devfreq, val);
	if (ret)
		goto p_err;

	if (devfreq->pm->update_devfreq_handler)
		devfreq->pm->update_devfreq_handler(devfreq->pm,
				devfreq->id, val);

	devfreq->pm->ops->update_freq_info(devfreq->pm, devfreq->id);
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void dsp_hw_common_devfreq_async(struct kthread_work *work)
{
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	devfreq = container_of(work, struct dsp_pm_devfreq, work);

	__dsp_hw_common_pm_update_devfreq(devfreq, devfreq->async_qos);
	devfreq->async_qos = -1;
	__dsp_hw_common_pm_devfreq_put_token(devfreq);
	devfreq->pm->sys->clk.ops->dump(&devfreq->pm->sys->clk);
	dsp_leave();
}

static int __dsp_hw_common_pm_update_devfreq_nolock(struct dsp_pm *pm, int id,
		int val, bool async)
{
	int ret;

	dsp_enter();
	if (!pm->ops->devfreq_active(pm)) {
		ret = -EINVAL;
		dsp_warn("dsp is not running\n");
		goto p_err;
	}

	if (id < 0 || id >= pm->devfreq_count) {
		ret = -EINVAL;
		dsp_err("devfreq id(%d) for dsp is invalid\n", id);
		goto p_err;
	}

	ret = __dsp_hw_common_pm_devfreq_get_token(&pm->devfreq[id], async);
	if (ret)
		goto p_err;

	if (async) {
		pm->devfreq[id].async_qos = val;
		kthread_queue_work(&pm->async_worker, &pm->devfreq[id].work);
	} else {
		ret = __dsp_hw_common_pm_update_devfreq(&pm->devfreq[id], val);
		__dsp_hw_common_pm_devfreq_put_token(&pm->devfreq[id]);
		if (ret)
			goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_pm_update_devfreq_nolock(struct dsp_pm *pm, int id, int val)
{
	dsp_check();
	return __dsp_hw_common_pm_update_devfreq_nolock(pm, id, val, false);
}

int dsp_hw_common_pm_update_devfreq_nolock_async(struct dsp_pm *pm, int id,
		int val)
{
	dsp_check();
	return __dsp_hw_common_pm_update_devfreq_nolock(pm, id, val, true);
}

int dsp_hw_common_pm_update_extra_devfreq(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	if (id < 0 || id >= pm->extra_devfreq_count) {
		ret = -EINVAL;
		dsp_err("extra devfreq id(%d) is invalid\n", id);
		goto p_err;
	}

	mutex_lock(&pm->lock);
	ret = __dsp_hw_common_pm_update_devfreq_raw(&pm->extra_devfreq[id],
			val);
	if (ret)
		goto p_err_update;

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err_update:
	mutex_unlock(&pm->lock);
p_err:
	return ret;
}

int dsp_hw_common_pm_set_force_qos(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	if (id < 0 || id >= pm->devfreq_count) {
		ret = -EINVAL;
		dsp_err("devfreq id(%d) for dsp is invalid\n", id);
		goto p_err;
	}

	if (val >= 0) {
		ret = __dsp_hw_common_pm_check_valid(&pm->devfreq[id], val);
		if (ret)
			goto p_err;
	}

	pm->devfreq[id].force_qos = val;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_common_pm_static_reset(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	devfreq->static_qos = devfreq->min_qos;
	devfreq->static_total_count = 0;
	memset(devfreq->static_count, 0x0, DSP_DEVFREQ_RESERVED_NUM << 2);
	dsp_leave();
}

static int __dsp_hw_common_pm_del_static(struct dsp_pm_devfreq *devfreq,
		int val)
{
	int ret, idx;

	dsp_enter();
	if (!devfreq->static_count[val]) {
		ret = -EINVAL;
		dsp_warn("static count is unstable([%s][L%d]%u)\n",
				devfreq->name, val, devfreq->static_count[val]);
		goto p_err;
	} else {
		devfreq->static_count[val]--;
		if (devfreq->static_total_count) {
			devfreq->static_total_count--;
		} else {
			ret = -EINVAL;
			dsp_warn("static total count is unstable([%s]%u)\n",
					devfreq->name,
					devfreq->static_total_count);
			goto p_err;
		}
	}

	if ((val == devfreq->static_qos) && (!devfreq->static_count[val])) {
		for (idx = val + 1; idx <= devfreq->min_qos; ++idx) {
			if (idx == devfreq->min_qos) {
				devfreq->static_qos = idx;
				break;
			}
			if (devfreq->static_count[idx]) {
				devfreq->static_qos = idx;
				break;
			}
		}
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_pm_dvfs_enable(struct dsp_pm *pm, int val)
{
	int ret, run, idx;
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	mutex_lock(&pm->lock);
	if (!pm->dvfs_disable_count) {
		ret = -EINVAL;
		dsp_warn("dvfs disable count is unstable(%u)\n",
				pm->dvfs_disable_count);
		goto p_err;
	}

	if (val == DSP_GOVERNOR_DEFAULT) {
		dsp_info("DVFS enabled forcibly\n");
		for (idx = 0; idx < pm->devfreq_count; ++idx)
			__dsp_hw_common_pm_static_reset(&pm->devfreq[idx]);

		pm->dvfs = true;
		pm->dvfs_disable_count = 0;
	} else {
		for (idx = 0; idx < pm->devfreq_count; ++idx) {
			ret = __dsp_hw_common_pm_check_valid(&pm->devfreq[idx],
					val);
			if (ret)
				goto p_err;
		}

		for (idx = 0; idx < pm->devfreq_count; ++idx)
			__dsp_hw_common_pm_del_static(&pm->devfreq[idx], val);

		if (!(--pm->dvfs_disable_count)) {
			pm->dvfs = true;
			dsp_info("DVFS enabled\n");
		}
	}

	if (!pm->ops->devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	for (idx = 0; idx < pm->devfreq_count; ++idx) {
		devfreq = &pm->devfreq[idx];

		if (devfreq->static_total_count) {
			if (devfreq->force_qos < 0)
				run = devfreq->static_qos;
			else
				run = devfreq->force_qos;
		} else {
			run = devfreq->dynamic_qos;
		}

		pm->ops->update_devfreq_nolock(pm, idx, run);
	}
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

static void __dsp_hw_common_pm_add_static(struct dsp_pm_devfreq *devfreq,
		int val)
{
	dsp_enter();
	devfreq->static_count[val]++;
	devfreq->static_total_count++;

	if (devfreq->static_total_count == 1)
		devfreq->static_qos = val;
	else if (val < devfreq->static_qos)
		devfreq->static_qos = val;

	dsp_leave();
}

int dsp_hw_common_pm_dvfs_disable(struct dsp_pm *pm, int val)
{
	int ret, run, idx;
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	mutex_lock(&pm->lock);
	for (idx = 0; idx < pm->devfreq_count; ++idx) {
		ret = __dsp_hw_common_pm_check_valid(&pm->devfreq[idx], val);
		if (ret)
			goto p_err;
	}

	pm->dvfs = false;
	if (!pm->dvfs_disable_count)
		dsp_info("DVFS disabled\n");
	pm->dvfs_disable_count++;

	for (idx = 0; idx < pm->devfreq_count; ++idx)
		__dsp_hw_common_pm_add_static(&pm->devfreq[idx], val);

	if (!pm->ops->devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	for (idx = 0; idx < pm->devfreq_count; ++idx) {
		devfreq = &pm->devfreq[idx];

		if (devfreq->force_qos < 0)
			run = devfreq->static_qos;
		else
			run = devfreq->force_qos;

		pm->ops->update_devfreq_nolock(pm, idx, run);
	}
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

static void __dsp_hw_common_pm_add_dynamic(struct dsp_pm_devfreq *devfreq,
		int val)
{
	dsp_enter();
	devfreq->dynamic_count[val]++;
	devfreq->dynamic_total_count++;

	if (devfreq->dynamic_total_count == 1)
		devfreq->dynamic_qos = val;
	else if (val < devfreq->dynamic_qos)
		devfreq->dynamic_qos = val;

	dsp_leave();
}

int dsp_hw_common_pm_update_devfreq_busy(struct dsp_pm *pm, int val)
{
	int ret, run, idx;
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	mutex_lock(&pm->lock);
	for (idx = 0; idx < pm->devfreq_count; ++idx) {
		ret = __dsp_hw_common_pm_check_valid(&pm->devfreq[idx], val);
		if (ret)
			goto p_err;
	}

	for (idx = 0; idx < pm->devfreq_count; ++idx)
		__dsp_hw_common_pm_add_dynamic(&pm->devfreq[idx], val);

	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(busy)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (!pm->ops->devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	for (idx = 0; idx < pm->devfreq_count; ++idx) {
		devfreq = &pm->devfreq[idx];

		if (devfreq->force_qos < 0)
			run = devfreq->dynamic_qos;
		else
			run = devfreq->force_qos;

		pm->ops->update_devfreq_nolock(pm, idx, run);
	}

	dsp_dbg("DVFS busy\n");
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

static int __dsp_hw_common_pm_del_dynamic(struct dsp_pm_devfreq *devfreq,
		int val)
{
	int ret, idx;

	dsp_enter();
	if (!devfreq->dynamic_count[val]) {
		ret = -EINVAL;
		dsp_warn("dynamic count is unstable([%s][L%d]%u)\n",
				devfreq->name, val,
				devfreq->dynamic_count[val]);
		goto p_err;
	} else {
		devfreq->dynamic_count[val]--;
		if (devfreq->dynamic_total_count) {
			devfreq->dynamic_total_count--;
		} else {
			ret = -EINVAL;
			dsp_warn("dynamic total count is unstable([%s]%u)\n",
					devfreq->name,
					devfreq->dynamic_total_count);
			goto p_err;
		}
	}

	if ((val == devfreq->dynamic_qos) && (!devfreq->dynamic_count[val])) {
		for (idx = val + 1; idx <= devfreq->min_qos; ++idx) {
			if (idx == devfreq->min_qos) {
				devfreq->dynamic_qos = idx;
				break;
			}
			if (devfreq->dynamic_count[idx]) {
				devfreq->dynamic_qos = idx;
				break;
			}
		}
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_pm_update_devfreq_idle(struct dsp_pm *pm, int val)
{
	int ret, idx;

	dsp_enter();
	mutex_lock(&pm->lock);
	for (idx = 0; idx < pm->devfreq_count; ++idx) {
		ret = __dsp_hw_common_pm_check_valid(&pm->devfreq[idx], val);
		if (ret)
			goto p_err;
	}

	for (idx = 0; idx < pm->devfreq_count; ++idx)
		__dsp_hw_common_pm_del_dynamic(&pm->devfreq[idx], val);

	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(idle)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	if (!pm->ops->devfreq_active(pm)) {
		mutex_unlock(&pm->lock);
		return 0;
	}

	for (idx = 0; idx < pm->devfreq_count; ++idx)
		pm->ops->update_devfreq_nolock_async(pm, idx,
				pm->devfreq[idx].dynamic_qos);

	dsp_dbg("DVFS idle\n");
	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
p_err:
	mutex_unlock(&pm->lock);
	return ret;
}

int dsp_hw_common_pm_update_devfreq_boot(struct dsp_pm *pm)
{
	int idx, boot;

	dsp_enter();
	mutex_lock(&pm->lock);
	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(boot)\n");
		pm->sys->clk.ops->dump(&pm->sys->clk);
		mutex_unlock(&pm->lock);
		return 0;
	}

	for (idx = 0; idx < pm->devfreq_count; ++idx) {
		if (pm->devfreq[idx].force_qos < 0)
			boot = pm->devfreq[idx].boot_qos;
		else
			boot = pm->devfreq[idx].force_qos;

		pm->ops->update_devfreq_nolock(pm, idx, boot);
	}

	dsp_dbg("DVFS boot\n");
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

int dsp_hw_common_pm_update_devfreq_max(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	mutex_lock(&pm->lock);
	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(max)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	for (idx = 0; idx < pm->devfreq_count; ++idx)
		pm->ops->update_devfreq_nolock(pm, idx, 0);

	dsp_dbg("DVFS max\n");
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

int dsp_hw_common_pm_update_devfreq_min(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	mutex_lock(&pm->lock);
	if (!pm->dvfs) {
		dsp_dbg("DVFS was disabled(min)\n");
		mutex_unlock(&pm->lock);
		return 0;
	}

	for (idx = 0; idx < pm->devfreq_count; ++idx)
		pm->ops->update_devfreq_nolock(pm, idx,
				pm->devfreq[idx].min_qos);

	dsp_dbg("DVFS min\n");
	pm->sys->clk.ops->dump(&pm->sys->clk);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static int __dsp_hw_common_pm_set_boot_qos(struct dsp_pm *pm, int id, int val)
{
	int ret;

	dsp_enter();
	if (id < 0 || id >= pm->devfreq_count) {
		ret = -EINVAL;
		dsp_err("devfreq id(%d) is invalid\n", id);
		goto p_err;
	}

	ret = __dsp_hw_common_pm_check_valid(&pm->devfreq[id], val);
	if (ret)
		goto p_err;

	pm->devfreq[id].boot_qos = val;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_pm_set_boot_qos(struct dsp_pm *pm, int val)
{
	int ret, idx;

	dsp_enter();
	for (idx = 0; idx < pm->devfreq_count; ++idx) {
		ret = __dsp_hw_common_pm_set_boot_qos(pm, idx, val);
		if (ret)
			goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_pm_boost_enable(struct dsp_pm *pm)
{
	int ret;
	struct dsp_pm_devfreq *devfreq;
	int idx;

	dsp_enter();
	devfreq = pm->extra_devfreq;

	mutex_lock(&pm->lock);
	for (idx = 0; idx < pm->extra_devfreq_count; ++idx)
		__dsp_hw_common_pm_update_devfreq_raw(&devfreq[idx],
				DSP_PM_QOS_L0);
	mutex_unlock(&pm->lock);

	ret = pm->ops->dvfs_disable(pm, DSP_PM_QOS_L0);
	if (ret)
		goto p_err_dvfs;

	dsp_info("boost mode of pm is enabled\n");
	dsp_leave();
	return 0;
p_err_dvfs:
	mutex_lock(&pm->lock);
	for (idx = pm->extra_devfreq_count - 1; idx >= 0; --idx)
		__dsp_hw_common_pm_update_devfreq_raw(&devfreq[idx],
				devfreq[idx].min_qos);
	mutex_unlock(&pm->lock);
	return ret;
}

int dsp_hw_common_pm_boost_disable(struct dsp_pm *pm)
{
	struct dsp_pm_devfreq *devfreq;
	int idx;

	dsp_enter();
	pm->ops->dvfs_enable(pm, DSP_PM_QOS_L0);

	devfreq = pm->extra_devfreq;
	mutex_lock(&pm->lock);
	for (idx = pm->extra_devfreq_count - 1; idx >= 0; --idx)
		__dsp_hw_common_pm_update_devfreq_raw(&devfreq[idx],
				devfreq[idx].min_qos);
	mutex_unlock(&pm->lock);

	dsp_info("boost mode of pm is disabled\n");
	dsp_leave();
	return 0;
}

static void __dsp_hw_common_pm_enable(struct dsp_pm_devfreq *devfreq, bool dvfs)
{
	int init_qos;

	dsp_enter();
	if (devfreq->force_qos < 0) {
		if (!dvfs) {
			init_qos = devfreq->static_qos;
			dsp_info("devfreq[%s] is enabled(L%d)(static)\n",
					devfreq->name, init_qos);
		} else {
			init_qos = devfreq->min_qos;
			dsp_info("devfreq[%s] is enabled(L%d)(dynamic)\n",
					devfreq->name, init_qos);
		}
	} else {
		init_qos = devfreq->force_qos;
		dsp_info("devfreq[%s] is enabled(L%d)(force)\n",
				devfreq->name, init_qos);
	}

#ifndef ENABLE_DSP_VELOCE
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_add_request(&devfreq->req, devfreq->class_id,
			devfreq->table[init_qos]);
#else
	pm_qos_add_request(&devfreq->req, devfreq->class_id,
			devfreq->table[init_qos]);
#endif
#endif
	devfreq->current_qos = init_qos;
	devfreq->pm->ops->update_freq_info(devfreq->pm, devfreq->id);
	dsp_leave();
}

static void __dsp_hw_common_pm_extra_enable(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
#ifndef ENABLE_DSP_VELOCE
	if (devfreq->use_freq_qos) {
		devfreq->policy = cpufreq_cpu_get(devfreq->class_id);
		if (!devfreq->policy) {
			dsp_err("Failed to get policy[%s/%d]\n",
					devfreq->name, devfreq->class_id);
			return;
		}

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		freq_qos_tracer_add_request(&devfreq->policy->constraints,
				&devfreq->freq_qos_req, FREQ_QOS_MIN,
				devfreq->table[devfreq->min_qos]);
#endif
	} else {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		exynos_pm_qos_add_request(&devfreq->req, devfreq->class_id,
				devfreq->table[devfreq->min_qos]);
#else
		pm_qos_add_request(&devfreq->req, devfreq->class_id,
				devfreq->table[devfreq->min_qos]);
#endif
	}
#endif
	devfreq->current_qos = devfreq->min_qos;
	dsp_dbg("devfreq[%s] is enabled(L%d)\n",
			devfreq->name, devfreq->current_qos);
	dsp_leave();
}

int dsp_hw_common_pm_enable(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	mutex_lock(&pm->lock);

	for (idx = 0; idx < pm->devfreq_count; ++idx)
		__dsp_hw_common_pm_enable(&pm->devfreq[idx], pm->dvfs);

	for (idx = 0; idx < pm->extra_devfreq_count; ++idx)
		__dsp_hw_common_pm_extra_enable(&pm->extra_devfreq[idx]);

#ifdef ENABLE_DSP_VELOCE
	pm_for_veloce = 1;
#endif
	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static void __dsp_hw_common_pm_disable(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	kthread_flush_work(&devfreq->work);
#ifndef ENABLE_DSP_VELOCE
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_remove_request(&devfreq->req);
#else
	pm_qos_remove_request(&devfreq->req);
#endif
#endif
	dsp_info("devfreq[%s] is disabled\n", devfreq->name);
	dsp_leave();
}

static void __dsp_hw_common_pm_extra_disable(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
#ifndef ENABLE_DSP_VELOCE
	if (devfreq->use_freq_qos) {
#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
		freq_qos_tracer_remove_request(&devfreq->freq_qos_req);
#endif
		devfreq->policy = NULL;
	} else {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		exynos_pm_qos_remove_request(&devfreq->req);
#else
		pm_qos_remove_request(&devfreq->req);
#endif
	}
#endif
	dsp_dbg("devfreq[%s] is disabled\n", devfreq->name);
	dsp_leave();
}

int dsp_hw_common_pm_disable(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	mutex_lock(&pm->lock);
#ifdef ENABLE_DSP_VELOCE
	pm_for_veloce = 0;
#endif

	for (idx = pm->extra_devfreq_count - 1; idx >= 0; --idx)
		__dsp_hw_common_pm_extra_disable(&pm->extra_devfreq[idx]);

	for (idx = pm->devfreq_count - 1; idx >= 0; --idx)
		__dsp_hw_common_pm_disable(&pm->devfreq[idx]);

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static int __dsp_hw_common_pm_check_class_id(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	if (devfreq->use_freq_qos) {
		dsp_dbg("devfreq[%s] uses freq_qos(%d)\n",
				devfreq->name, devfreq->class_id);
	} else {
#ifndef ENABLE_DSP_VELOCE
		if (!devfreq->class_id) {
			dsp_err("devfreq[%s] class_id must not be zero(%d)\n",
					devfreq->name, devfreq->class_id);
			return -EINVAL;
		}
#endif
		dsp_dbg("devfreq[%s] class_id is %d\n", devfreq->name,
				devfreq->class_id);
	}
	dsp_leave();
	return 0;
}

int dsp_hw_common_pm_open(struct dsp_pm *pm)
{
	int idx, ret;

	dsp_enter();
	for (idx = 0; idx < pm->devfreq_count; ++idx) {
		ret = __dsp_hw_common_pm_check_class_id(&pm->devfreq[idx]);
		if (ret)
			goto p_err;
	}

	for (idx = 0; idx < pm->extra_devfreq_count; ++idx) {
		ret = __dsp_hw_common_pm_check_class_id(
				&pm->extra_devfreq[idx]);
		if (ret)
			goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_common_pm_init(struct dsp_pm_devfreq *devfreq)
{
	dsp_enter();
	devfreq->static_qos = devfreq->min_qos;
	devfreq->static_total_count = 0;
	memset(devfreq->static_count, 0x0, DSP_DEVFREQ_RESERVED_NUM << 2);
	devfreq->dynamic_qos = devfreq->min_qos;
	devfreq->dynamic_total_count = 0;
	memset(devfreq->dynamic_count, 0x0, DSP_DEVFREQ_RESERVED_NUM << 2);
	devfreq->force_qos = -1;

	devfreq->async_status = -1;
	devfreq->async_qos = -1;
	dsp_leave();
}

int dsp_hw_common_pm_close(struct dsp_pm *pm)
{
	int idx;

	dsp_enter();
	if (!pm->dvfs_lock) {
		dsp_info("DVFS is reinitialized\n");
		pm->dvfs = true;
		pm->dvfs_disable_count = 0;
		for (idx = 0; idx < pm->devfreq_count; ++idx)
			__dsp_hw_common_pm_init(&pm->devfreq[idx]);
	}
	dsp_leave();
	return 0;
}

static int __dsp_hw_common_pm_worker_init(struct dsp_pm *pm)
{
	int ret;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	kthread_init_worker(&pm->async_worker);

	pm->async_task = kthread_run(kthread_worker_fn, &pm->async_worker,
			"dsp_pm_worker");
	if (IS_ERR(pm->async_task)) {
		ret = PTR_ERR(pm->async_task);
		dsp_err("kthread of pm is not running(%d)\n", ret);
		goto p_err_kthread;
	}

	ret = sched_setscheduler_nocheck(pm->async_task, SCHED_FIFO, &param);
	if (ret) {
		dsp_err("Failed to set scheduler of pm worker(%d)\n", ret);
		goto p_err_sched;
	}

	return ret;
p_err_sched:
	kthread_stop(pm->async_task);
p_err_kthread:
	return ret;
}

int dsp_hw_common_pm_probe(struct dsp_pm *pm, void *sys, pm_handler_t handler)
{
	int ret;
	struct dsp_pm_devfreq *devfreq;
	unsigned int idx;

	dsp_enter();
	pm->sys = sys;
	pm->dev = pm->sys->dev;

	pm->devfreq = kcalloc(pm->devfreq_count, sizeof(*devfreq), GFP_KERNEL);
	if (!pm->devfreq) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc pm_devfreq\n");
		goto p_err_devfreq;
	}

	for (idx = 0; idx < pm->devfreq_count; ++idx) {
		devfreq = &pm->devfreq[idx];
		devfreq->pm = pm;

		devfreq->id = idx;
		devfreq->force_qos = -1;

		kthread_init_work(&devfreq->work, dsp_hw_common_devfreq_async);
		spin_lock_init(&devfreq->slock);
		devfreq->async_status = -1;
		devfreq->async_qos = -1;
	}

	pm->extra_devfreq = kzalloc(sizeof(*devfreq) * pm->extra_devfreq_count,
			GFP_KERNEL);
	if (!pm->extra_devfreq) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc extra_devfreq\n");
		goto p_err_extra_devfreq;
	}

	for (idx = 0; idx < pm->extra_devfreq_count; ++idx) {
		devfreq = &pm->extra_devfreq[idx];
		devfreq->pm = pm;

		devfreq->id = idx;
	}

	pm_runtime_enable(pm->dev);
	mutex_init(&pm->lock);
	pm->dvfs = true;
	pm->dvfs_lock = false;

	ret = __dsp_hw_common_pm_worker_init(pm);
	if (ret)
		goto p_err_worker;

	if (handler)
		pm->update_devfreq_handler = handler;

	dsp_leave();
	return 0;
p_err_worker:
	kfree(pm->extra_devfreq);
p_err_extra_devfreq:
	kfree(pm->devfreq);
p_err_devfreq:
	return ret;
}

static void __dsp_hw_common_pm_worker_deinit(struct dsp_pm *pm)
{
	dsp_enter();
	kthread_stop(pm->async_task);
	dsp_leave();
}

void dsp_hw_common_pm_remove(struct dsp_pm *pm)
{
	dsp_enter();
	__dsp_hw_common_pm_worker_deinit(pm);
	mutex_destroy(&pm->lock);
	pm_runtime_disable(pm->dev);
	kfree(pm->extra_devfreq);
	kfree(pm->devfreq);
	dsp_leave();
}

int dsp_hw_common_pm_set_ops(struct dsp_pm *pm, const void *ops)
{
	dsp_enter();
	pm->ops = ops;
	dsp_leave();
	return 0;
}
