// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <trace/hooks/sched.h>

static bool __is_might_sleep_bug(struct task_struct *tsk)
{
	return (tsk == NULL);
}

static void _secdbg_trace_sched_bug(void *ignore, struct task_struct *tsk)
{
	if (IS_ENABLED(CONFIG_SEC_DEBUG_ATOMIC_SLEEP_PANIC) &&
			!__is_might_sleep_bug(tsk))
		BUG();
}

int __init secdbg_atsl_init(void)
{
	pr_info("%s\n", __func__);

	register_trace_android_rvh_schedule_bug(_secdbg_trace_sched_bug, NULL);
	register_trace_android_rvh_dequeue_task_idle(_secdbg_trace_sched_bug, NULL);

	return 0;
}

MODULE_DESCRIPTION("Samsung Debug Atomic Sleep driver");
MODULE_LICENSE("GPL v2");
