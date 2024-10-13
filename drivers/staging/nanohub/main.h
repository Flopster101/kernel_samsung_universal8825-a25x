/*
 * Copyright (C) 2016 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _NANOHUB_MAIN_H
#define _NANOHUB_MAIN_H

#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/pm_wakeup.h>
#include <linux/kthread.h>

#include "comms.h"
#include "chub.h"

#define NANOHUB_NAME "nanohub"

struct nanohub_buf {
	struct list_head list;
	uint8_t buffer[255];
	uint8_t length;
};

struct nanohub_data;

struct nanohub_io {
	struct device *dev;
	struct nanohub_data *data;
	wait_queue_head_t buf_wait;
	struct list_head buf_list;
};

struct sensor_map_pack {
	char magic[15];
	uint8_t num_os;
	struct sensor_map sensormap;
};

static inline struct nanohub_data *dev_get_nanohub_data(struct device *dev)
{
	struct nanohub_io *io = dev_get_drvdata(dev);

	if(io == NULL) {
		nanohub_err("%s io not available!\n", __func__);
		return NULL;
	}

	return io->data;
}

#define CHUB_USER_DEBUG
#ifdef CHUB_USER_DEBUG
enum chub_user_err_id {
	chub_user_err_missmatch,
	chub_user_err_timeout,
	chub_user_err_wakeaq,
	chub_user_err_max,
};

#define CHUB_USER_DEBUG_NUM (20)
#define CHUB_USER_DEBUG_ERR_NUM (10)

#define CHUB_COMMS_SIZE (16)

struct chub_user {
	u64 pid;
	u64 lastTime;
	char name[CHUB_COMMS_SIZE];
	u32 lock_mode;
	long ret;
};

struct chub_user_debug {
	/* dump for last acquire lock */
	struct chub_user user_acq;
	/* dump for current */
	struct chub_user user_cur;
	/* dump for always */
	struct chub_user user[CHUB_USER_DEBUG_NUM];
	struct chub_user user_out[CHUB_USER_DEBUG_NUM];
	/* dump for error */
	struct chub_user user_err[chub_user_err_max][CHUB_USER_DEBUG_ERR_NUM];
	struct chub_user user_err_out[CHUB_USER_DEBUG_ERR_NUM];
	u32 index;
	u32 index_err[chub_user_err_max];
	u32 err_cnt[chub_user_err_max];
};

void print_chub_user(struct nanohub_data *data);
#else
#define print_chub_user(a) ((void)0)
#endif

struct nanohub_data {
	struct iio_dev *iio_dev;
	struct nanohub_io io;

	struct nanohub_comms comms;
	struct nanohub_platform_data *pdata;
	u64 wakelock_req_time;
	int irq;

	atomic_t kthread_run;
	atomic_t thread_state;
	atomic_t in_reset;
	wait_queue_head_t kthread_wait;

	struct wakeup_source *ws;

	struct nanohub_io free_pool;

	atomic_t lock_mode;
	/* these 3 vars should be accessed only with wakeup_wait.lock held */
	atomic_t wakeup_cnt;
	atomic_t wakeup_lock_cnt;
	atomic_t wakeup_acquired;
	wait_queue_head_t wakeup_wait;

	uint32_t interrupts[8];

	ktime_t wakeup_err_ktime;
	int wakeup_err_cnt;
	int wakeup_cnt_acq_err;

	ktime_t kthread_err_ktime;
	int kthread_err_cnt;

	void *vbuf;
	struct task_struct *thread;

#ifdef CHUB_USER_DEBUG
	struct chub_user_debug chub_user;
#endif
};

enum {
	KEY_WAKEUP_NONE,
	KEY_WAKEUP,
	KEY_WAKEUP_LOCK,
};

enum {
	LOCK_MODE_NONE,
	LOCK_MODE_NORMAL,
	LOCK_MODE_RESET,
	LOCK_MODE_SUSPEND_RESUME,
};

int request_wakeup_ex(struct nanohub_data *data, long timeout,
		      int key, int lock_mode);
void release_wakeup_ex(struct nanohub_data *data, int key, int lock_mode);
int nanohub_wakeup_eom(struct nanohub_data *data, bool repeat);
struct iio_dev *nanohub_probe(struct device *dev, struct iio_dev *iio_dev);
int nanohub_remove(struct iio_dev *iio_dev);

int nanohub_init(void);
void nanohub_cleanup(void);

void nanohub_handle_irq(struct nanohub_data *data);

static inline int nanohub_irq_fired(struct nanohub_data *data)
{
	struct contexthub_ipc_info *chub = data->pdata->mailbox_client;

	return !atomic_read(&chub->irq_apInt);
}

void nanohub_add_dump_request(struct nanohub_data *data);

int nanohub_hw_reset(struct nanohub_data *data);

static inline int request_wakeup_timeout(struct nanohub_data *data, int timeout)
{
	return request_wakeup_ex(data, timeout, KEY_WAKEUP, LOCK_MODE_NORMAL);
}

static inline int request_wakeup(struct nanohub_data *data)
{
	return request_wakeup_ex(data, MAX_SCHEDULE_TIMEOUT, KEY_WAKEUP,
				 LOCK_MODE_NORMAL);
}

static inline void release_wakeup(struct nanohub_data *data)
{
	release_wakeup_ex(data, KEY_WAKEUP, LOCK_MODE_NORMAL);
}
#endif
