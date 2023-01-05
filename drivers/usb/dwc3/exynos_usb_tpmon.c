#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/timer.h>

#include "exynos_usb_tpmon.h"
#include "trace.h"

#define IGNORE_DELTA			500000 /* 500 msec */
#define ACCUMULATING_TIME		1000000 /* 1 sec */
#define QOS_UNLOCK_TIME			(HZ * 2) /* 2 sec */

struct tpmon_data {
	u64	accumulated_time; /* usec */
	u64	accumulated_data;

	ktime_t	last_time;
	int gathered_data;
	int qos_level;

	struct workqueue_struct	*tpmon_wq;
	struct work_struct tpmon_work;

	struct timer_list qos_unlock_timer;
};

extern void dwc3_otg_pm_ctrl(struct dwc3_exynos *dwc_exynos, int onoff);
static struct tpmon_data tpmon;

void usb_tpmon_check_tp(void *data, struct dwc3_request *req)
{
	ktime_t current_time;
	u64 time_delta_us;
	u64 val;
	int data_size;

	data_size = req->request.length;
	current_time = ktime_get();
	time_delta_us = ktime_us_delta(current_time, tpmon.last_time);

	if (time_delta_us > IGNORE_DELTA) {
		tpmon.last_time = current_time;
		tpmon.accumulated_time = 0;
		tpmon.accumulated_data = 0;
		tpmon.gathered_data = 0;

		return;
	}
	tpmon.accumulated_time += time_delta_us;
	tpmon.accumulated_data += data_size;
	tpmon.last_time = current_time;
	tpmon.gathered_data++;

	if (tpmon.accumulated_time <= ACCUMULATING_TIME)
		return;

	/*
	printk("KS : TP : %llu / %llu (%d)\n",
		tpmon.accumulated_data, tpmon.accumulated_time,
		tpmon.gathered_data);
	*/
	val = tpmon.accumulated_data / tpmon.accumulated_time *
		1000000 /* usec -> sec */;

	val /= SZ_1M; /* Change to MB */

	pr_info("throughput %d(%d) MB/sec\n", val, val * 8);

	if (val >= 40) { /* 1Gb */
		queue_work(tpmon.tpmon_wq, &tpmon.tpmon_work);
	}

	tpmon.accumulated_time = 0;
	tpmon.accumulated_data = 0;
	tpmon.gathered_data = 0;

	return;
}

void usb_tpmon_init_data(void)
{
	pr_info("Intialization USB TPMON data.\n");
	tpmon.accumulated_time = 0;
	tpmon.accumulated_data = 0;
	tpmon.last_time = 0;
	tpmon.gathered_data = 0;
}

static void timer_qos_unlock(struct timer_list *t)
{
	pr_info("Unlock USB QOS...\n");
	/* 1. Set USB Normal Mode */
	dwc3_otg_pm_ctrl(g_dwc3_exynos, 1);
}

static void usb_tpmon_work(struct work_struct *data)
{
	int check_timer;

	/* Setup Unlock Timer */
	pr_info("Set QOS LOCK and Start Unlock Timer!!\n");
	check_timer = mod_timer(&tpmon.qos_unlock_timer,
					jiffies + QOS_UNLOCK_TIME);
	if (check_timer == 1) {
		/* There is an active timer, it means already QOS locked. */
		pr_info("SKIP QOS LOCK!\n");
		return;
	}

	pr_info("QOS LOCK...\n");
	/* 1. Set USB performance mode */
	dwc3_otg_pm_ctrl(g_dwc3_exynos, 0);
}

void usb_tpmon_init(void)
{
	pr_info("%s\n", __func__);

	register_trace_dwc3_ep_queue(usb_tpmon_check_tp, NULL);
	tpmon.tpmon_wq = alloc_ordered_workqueue("usb_tpmon_wq",
				__WQ_LEGACY | WQ_MEM_RECLAIM | WQ_FREEZABLE);
	INIT_WORK(&tpmon.tpmon_work, usb_tpmon_work);
	timer_setup(&tpmon.qos_unlock_timer, timer_qos_unlock, 0);
}

void usb_tpmon_exit(void)
{
	pr_info("%s\n", __func__);
	destroy_workqueue(tpmon.tpmon_wq);
}

void usb_tpmon_open(void)
{
	pr_info("%s\n", __func__);
	usb_tpmon_init_data();
}

/* Call this function at removing USB */
void usb_tpmon_close(void)
{
	pr_info("%s\n", __func__);
	pr_info("USB TPMON - Unlock all.\n");
	pr_info("USB TPMON - Del Timer!!!.\n");
	del_timer_sync(&tpmon.qos_unlock_timer);
}

