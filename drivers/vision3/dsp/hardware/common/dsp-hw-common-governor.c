// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/delay.h>

#include "dsp-log.h"
#include "dsp-hw-common-governor.h"

#define DSP_GOVERNOR_TOKEN_WAIT_COUNT		(1000)

static int __dsp_hw_common_governor_get_token(struct dsp_governor *governor,
		bool async)
{
	int repeat = DSP_GOVERNOR_TOKEN_WAIT_COUNT;

	dsp_enter();
	while (repeat) {
		spin_lock(&governor->slock);
		if (governor->async_status < 0) {
			governor->async_status = async;
			spin_unlock(&governor->slock);
			dsp_dbg("get governor[%d] token(wait count:%d/%d)\n",
					async, repeat,
					DSP_GOVERNOR_TOKEN_WAIT_COUNT);
			dsp_leave();
			return 0;
		}

		spin_unlock(&governor->slock);
		udelay(100);
		repeat--;
	}

	dsp_err("asynchronous governor work is not done\n");
	return -ETIMEDOUT;
}

static void __dsp_hw_common_governor_put_token(struct dsp_governor *governor)
{
	dsp_enter();
	spin_lock(&governor->slock);
	if (governor->async_status < 0)
		dsp_warn("governor token is invalid(%d)\n",
				governor->async_status);

	governor->async_status = -1;
	spin_unlock(&governor->slock);
	dsp_dbg("put governor token\n");
	dsp_leave();
}

static void dsp_hw_common_governor_async(struct kthread_work *work)
{
	int ret;
	struct dsp_governor *governor;

	dsp_enter();
	governor = container_of(work, struct dsp_governor, work);

	ret = governor->handler(governor, &governor->async_req);
	if (ret)
		dsp_err("Failed to request governor asynchronously(%d)\n", ret);
	__dsp_hw_common_governor_put_token(governor);
	dsp_leave();
}

int dsp_hw_common_governor_request(struct dsp_governor *governor,
		struct dsp_control_preset *req)
{
	int ret;
	bool async;

	dsp_enter();
	if (req->async > 0)
		async = true;
	else
		async = false;

	ret = __dsp_hw_common_governor_get_token(governor, async);
	if (ret)
		goto p_err;

	if (async) {
		governor->async_req = *req;
		kthread_queue_work(&governor->worker, &governor->work);
	} else {
		ret = governor->handler(governor, req);
		__dsp_hw_common_governor_put_token(governor);
		if (ret)
			goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_governor_check_done(struct dsp_governor *governor)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_common_governor_get_token(governor, false);
	if (ret)
		goto p_err;

	__dsp_hw_common_governor_put_token(governor);
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_governor_flush_work(struct dsp_governor *governor)
{
	dsp_enter();
	kthread_flush_work(&governor->work);
	dsp_leave();
	return 0;
}

int dsp_hw_common_governor_open(struct dsp_governor *governor)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_common_governor_close(struct dsp_governor *governor)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int __dsp_hw_common_governor_worker_init(struct dsp_governor *governor)
{
	int ret;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	kthread_init_worker(&governor->worker);

	governor->task = kthread_run(kthread_worker_fn, &governor->worker,
			"dsp_hw_common_governor_worker");
	if (IS_ERR(governor->task)) {
		ret = PTR_ERR(governor->task);
		dsp_err("kthread of governor is not running(%d)\n", ret);
		goto p_err_kthread;
	}

	ret = sched_setscheduler_nocheck(governor->task, SCHED_FIFO, &param);
	if (ret) {
		dsp_err("Failed to set scheduler of governor worker(%d)\n",
				ret);
		goto p_err_sched;
	}

	return ret;
p_err_sched:
	kthread_stop(governor->task);
p_err_kthread:
	return ret;
}

int dsp_hw_common_governor_probe(struct dsp_governor *governor, void *sys)
{
	int ret;

	dsp_enter();
	governor->sys = sys;

	ret = __dsp_hw_common_governor_worker_init(governor);
	if (ret)
		goto p_err;

	kthread_init_work(&governor->work, dsp_hw_common_governor_async);
	spin_lock_init(&governor->slock);
	governor->async_status = -1;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_common_governor_worker_deinit(
		struct dsp_governor *governor)
{
	dsp_enter();
	kthread_stop(governor->task);
	dsp_leave();
}

void dsp_hw_common_governor_remove(struct dsp_governor *governor)
{
	dsp_enter();
	__dsp_hw_common_governor_worker_deinit(governor);
	dsp_leave();
}

int dsp_hw_common_governor_set_ops(struct dsp_governor *governor,
		const void *ops)
{
	dsp_enter();
	governor->ops = ops;
	dsp_leave();
	return 0;
}
