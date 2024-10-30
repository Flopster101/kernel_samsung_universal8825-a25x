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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/iio/iio.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/sched/prio.h>
#include <linux/sched/signal.h>
#include <linux/time.h>
#include <linux/platform_data/nanohub.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <uapi/linux/sched/types.h>
#include "main.h"
#include "comms.h"
#include "chub.h"
#include "chub_dbg.h"

#define READ_QUEUE_DEPTH	10
#define APP_FROM_HOST_EVENTID	0x000000F8
#define FIRST_SENSOR_EVENTID	0x00000200
#define LAST_SENSOR_EVENTID	0x000002FF
#define APP_TO_HOST_EVENTID	0x00000401
#define OS_LOG_EVENTID		0x3B474F4C
#define WAKEUP_INTERRUPT	1
#define WAKEUP_TIMEOUT_MS	1000
#define SUSPEND_TIMEOUT_MS	100
#define KTHREAD_ERR_TIME_NS	(60LL * NSEC_PER_SEC)
#define KTHREAD_ERR_CNT		20
#define KTHREAD_WARN_CNT	10
#define WAKEUP_ERR_TIME_NS	(60LL * NSEC_PER_SEC)
#define WAKEUP_ERR_CNT		4

static int nanohub_open(struct inode *, struct file *);
static ssize_t nanohub_read(struct file *, char *, size_t, loff_t *);
static ssize_t nanohub_write(struct file *, const char *, size_t, loff_t *);
static unsigned int nanohub_poll(struct file *, poll_table *);
static int nanohub_release(struct inode *, struct file *);
static int nanohub_kthread(void *arg);

static int chub_dev_open(struct inode *, struct file *);
static ssize_t chub_dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t chub_dev_write(struct file *, const char *, size_t, loff_t *);

static struct class *sensor_class;
static int major, chub_dev_major;

extern const char *os_image[SENSOR_VARIATION];
extern struct memlog_obj *memlog_printf_file_chub;
static const struct iio_info nanohub_iio_info;

static const struct file_operations nanohub_fileops = {
	.owner = THIS_MODULE,
	.open = nanohub_open,
	.read = nanohub_read,
	.write = nanohub_write,
	.poll = nanohub_poll,
	.release = nanohub_release,
};

#define CHUB_DEV_IOCTL_SET_DFS		_IOWR('c', 201, __u32)

static long chub_dev_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg_)
{
	struct contexthub_ipc_info *chub = file->private_data;

	if (!chub) {
		nanohub_dev_err(NULL, "%s: dev_nanohub not available\n", __func__);
		return -EINVAL;
	}

	if (!chub->chub_dfs_gov) {
		nanohub_dev_info(chub->dev, "%s: chub dfs is disabled\n", __func__);
		return 0;
	}

	switch (cmd) {
		case CHUB_DEV_IOCTL_SET_DFS:
			chub->chub_dfs_gov = DFS_GVERNOR_MAX;
			break;
		default:
			nanohub_dev_warn(chub->dev, "%s: invalid cmd:%x\n", __func__, cmd);
			break;
	}
	nanohub_dev_info(chub->dev, "%s: cmd:%x gov:%d\n", __func__, cmd, chub->chub_dfs_gov);
	return 0;
}

static const struct file_operations chub_dev_fileops = {
	.owner = THIS_MODULE,
	.open = chub_dev_open,
	.read = chub_dev_read,
	.write = chub_dev_write,
	.unlocked_ioctl = chub_dev_compat_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= chub_dev_compat_ioctl,
#endif
};

enum {
	ST_IDLE,
	ST_ERROR,
	ST_RUNNING
};

static inline bool nanohub_has_priority_lock_locked(struct nanohub_data *data)
{
	return  atomic_read(&data->wakeup_lock_cnt) >
		atomic_read(&data->wakeup_cnt);
}

static inline void nanohub_notify_thread(struct nanohub_data *data)
{
	atomic_set(&data->kthread_run, 1);
	/* wake_up implementation works as memory barrier */
	wake_up_interruptible_sync(&data->kthread_wait);
}

static inline void nanohub_io_init(struct nanohub_io *io,
				   struct nanohub_data *data,
				   struct device *dev)
{
	init_waitqueue_head(&io->buf_wait);
	INIT_LIST_HEAD(&io->buf_list);
	io->data = data;
	io->dev = dev;
}

static inline bool nanohub_io_has_buf(struct nanohub_io *io)
{
	return !list_empty(&io->buf_list);
}

#define EVT_DEBUG_DUMP                   0x00007F02  /* defined on sensorhal */
#define EVT_DFS_DUMP                     0x00007F03

int nanohub_is_reset_notify_io(struct nanohub_buf *buf)
{
	if (buf) {
		uint32_t *buffer = (uint32_t *)buf->buffer;
		if (*buffer == EVT_DEBUG_DUMP)
			return true;
	}
	return false;
}

static struct nanohub_buf *nanohub_io_get_buf(struct nanohub_io *io,
					      bool wait)
{
	struct nanohub_buf *buf = NULL;
	int ret;

	spin_lock(&io->buf_wait.lock);
	if (wait) {
		ret = wait_event_interruptible_locked(io->buf_wait,
						      nanohub_io_has_buf(io));
		if (ret < 0) {
			spin_unlock(&io->buf_wait.lock);
			return ERR_PTR(ret);
		}
	}

	if (nanohub_io_has_buf(io)) {
		buf = list_first_entry(&io->buf_list, struct nanohub_buf, list);
		list_del(&buf->list);
	}
	spin_unlock(&io->buf_wait.lock);

	return buf;
}

static void nanohub_io_put_buf(struct nanohub_io *io,
			       struct nanohub_buf *buf)
{
	bool was_empty;

	spin_lock(&io->buf_wait.lock);
	was_empty = !nanohub_io_has_buf(io);
	list_add_tail(&buf->list, &io->buf_list);
	spin_unlock(&io->buf_wait.lock);

	if (was_empty) {
		if (&io->data->free_pool == io)
			nanohub_notify_thread(io->data);
		else
			wake_up_interruptible(&io->buf_wait);
	}
}

static inline void mcu_wakeup_mailbox_set_value(struct nanohub_data *data, int val)
{
	contexthub_ipc_write_event(data->pdata->mailbox_client,
				   val ? MAILBOX_EVT_WAKEUP_CLR : MAILBOX_EVT_WAKEUP);
}

static inline void mcu_wakeup_mailbox_get_locked(struct nanohub_data *data, int priority_lock)
{
	atomic_inc(&data->wakeup_lock_cnt);
	if (!priority_lock && atomic_inc_return(&data->wakeup_cnt) == 1 &&
	    !nanohub_has_priority_lock_locked(data))
			mcu_wakeup_mailbox_set_value(data, 0);
}

static inline bool mcu_wakeup_mailbox_put_locked(struct nanohub_data *data,
					      int priority_lock)
{
	bool mailbox_done = priority_lock ?
			 atomic_read(&data->wakeup_cnt) == 0 :
			 atomic_dec_and_test(&data->wakeup_cnt);
	bool done = atomic_dec_and_test(&data->wakeup_lock_cnt);

	if (!nanohub_has_priority_lock_locked(data))
		mcu_wakeup_mailbox_set_value(data, mailbox_done ? 1 : 0);

	return done;
}

static inline bool mcu_wakeup_mailbox_is_locked(struct nanohub_data *data)
{
	return atomic_read(&data->wakeup_lock_cnt) != 0;
}

inline void nanohub_handle_irq(struct nanohub_data *data)
{
	bool locked;

	spin_lock(&data->wakeup_wait.lock);
	locked = mcu_wakeup_mailbox_is_locked(data);
	spin_unlock(&data->wakeup_wait.lock);
	if (!locked)
		nanohub_notify_thread(data);
	else
		wake_up_interruptible_sync(&data->wakeup_wait);
}

static inline bool mcu_wakeup_try_lock(struct nanohub_data *data, int key)
{
	/* implementation contains memory barrier */
	return atomic_cmpxchg(&data->wakeup_acquired, 0, key) == 0;
}

#ifdef CHUB_USER_DEBUG
static void set_chub_error_user(struct nanohub_data *data, enum chub_user_err_id err)
{
	int index = data->chub_user.index_err[err] = (data->chub_user.index_err[err] + 1) % CHUB_USER_DEBUG_ERR_NUM;
	struct chub_user *user_err = &data->chub_user.user_err[err][index];
	struct chub_user *user_lock = &data->chub_user.user[data->chub_user.index];

	memcpy(user_err, user_lock, sizeof(struct chub_user));

	if (err == chub_user_err_missmatch) /* only backup to user_unlock */ {
		memcpy(&data->chub_user.user_err_out[index],
			&data->chub_user.user_out[data->chub_user.index],
			sizeof(struct chub_user));
		data->chub_user.user_err_out[index].ret = err;
	}
}

static void set_chub_user_info(struct chub_user *user, long ret, u32 lock)
{
	if (!user) {
		nanohub_warn("%s: user not available", __func__);
		return;
	}
	user->pid = current->pid;
	snprintf(user->name, CHUB_COMMS_SIZE, current->comm);
	user->lastTime = ktime_get_boottime_ns();
	user->ret = ret;
	user->lock_mode = lock;
}

static void set_chub_user(struct nanohub_data *data, bool in, long time, u32 lock)
{
	struct chub_user *user;
	u32 err = chub_user_err_max;

	if (in) {
		data->chub_user.index = (data->chub_user.index + 1) % CHUB_USER_DEBUG_NUM;

		/* set current user */
		user = &data->chub_user.user_cur;
		user->pid = current->pid;
		user->lastTime = ktime_get_boottime_ns();
		user->lock_mode = lock;

		/* put user into lock-queue */
		user = &data->chub_user.user[data->chub_user.index];
		set_chub_user_info(user, time, lock);

		/* timeout err */
		if (time <= 0) {
			err = chub_user_err_timeout;
			data->chub_user.err_cnt[err]++;
			if (atomic_read(&data->wakeup_acquired))
				err = chub_user_err_wakeaq;
		}
	} else {
		/* put user into unlock-queue */
		user = &data->chub_user.user_out[data->chub_user.index];
		set_chub_user_info(user, time, lock);

		/* lock/unlock missmatch err */
		if (user->pid != data->chub_user.user[data->chub_user.index].pid)
			err = chub_user_err_missmatch;
	}
	if (err != chub_user_err_max) {
		data->chub_user.err_cnt[err]++;
		set_chub_error_user(data, err);
	}
}

static void printout_chub_user_info(struct device *dev, int i, struct chub_user *user_lock, struct chub_user *user_unlock)
{
	if (!user_lock || !user_unlock)
		return;

	if (user_lock && user_unlock)
		nanohub_dev_info(dev,
			"user: i:%d IN(pid:%d, comm:%s, time:%lld, ret:%lld, lock:%d) / OUT(pid:%d, comm:%s, time:%lld, ret:%lld, lock:%d) during %lld\n",
			i,
			user_lock->pid, user_lock->name, user_lock->lastTime, user_lock->ret, user_lock->lock_mode,
			user_unlock->pid, user_unlock->name, user_unlock->lastTime, user_unlock->ret, user_unlock->lock_mode,
			user_unlock->lastTime - user_lock->lastTime);
	else
		nanohub_dev_info(dev,
			"user: i:%d IN(pid:%d, comm:%s, time:%lld, ret:%lld, lock:%d)\n",
			i,
			user_lock->pid, user_lock->name, user_lock->lastTime, user_lock->ret, user_lock->lock_mode);
}

void print_chub_user(struct nanohub_data *data)
{
	int i, j;
	struct device *dev = data->io.dev;

	nanohub_dev_info(dev, "%s: cur_user: name:%s, id:%d, time:%lld, err:%d, %d, %d\n",
		__func__, data->chub_user.user_acq.name,
		data->chub_user.user_cur.pid, data->chub_user.user_cur.lastTime,
		data->chub_user.err_cnt[chub_user_err_missmatch],
		data->chub_user.err_cnt[chub_user_err_timeout],
		data->chub_user.err_cnt[chub_user_err_wakeaq]);

	nanohub_dev_info(dev, "%s: Last_acq: name:%s, id:%d, time:%lld\n",
		__func__, data->chub_user.user_acq.name,
		data->chub_user.user_acq.pid, data->chub_user.user_acq.lastTime);

	for (j = 0; j < chub_user_err_max; j++) {
		if (data->chub_user.index_err[j]) {
			nanohub_dev_info(dev, "%s: cur_err:%d: cnt:%d\n", __func__, j, data->chub_user.index_err[j]);
			for (i = 0; i < CHUB_USER_DEBUG_ERR_NUM; i++)
				printout_chub_user_info(dev, i,
					&data->chub_user.user_err[j][i],
					(j == chub_user_err_missmatch) ? &data->chub_user.user_err_out[i] : NULL);
		}
	}

	nanohub_dev_info(dev, "%s: dump user of wakeup lock and unlock\n", __func__);
	for (i = data->chub_user.index + 1; i < CHUB_USER_DEBUG_NUM; i++)
		printout_chub_user_info(dev, i, &data->chub_user.user[i], &data->chub_user.user_out[i]);

	for (i = 0; i < data->chub_user.index + 1; i++)
		printout_chub_user_info(dev, i, &data->chub_user.user[i], &data->chub_user.user_out[i]);
}
#else
#define set_chub_user(a, b, c, d) ((void)0)
#define set_chub_user_info(a, b, c) ((void)0)
#endif

static inline void mcu_wakeup_unlock(struct nanohub_data *data, int key)
{
#ifdef CHUB_USER_DEBUG
	bool unlocked = atomic_cmpxchg(&data->wakeup_acquired, key, 0) != key;

	if (unlocked) {
		print_chub_user(data);
		WARN(unlocked, "%s: failed to unlock with key %d; current state: %d",
			__func__, key, atomic_read(&data->wakeup_acquired));
	}
#else
	WARN(atomic_cmpxchg(&data->wakeup_acquired, key, 0) != key,
	     "%s: failed to unlock with key %d; current state: %d",
	     __func__, key, atomic_read(&data->wakeup_acquired));
#endif
}

static inline void nanohub_set_state(struct nanohub_data *data, int state)
{
	atomic_set(&data->thread_state, state);
	smp_mb__after_atomic(); /* updated thread state is now visible */
}

static inline int nanohub_get_state(struct nanohub_data *data)
{
	smp_mb__before_atomic(); /* wait for all updates to finish */
	return atomic_read(&data->thread_state);
}

static inline void nanohub_clear_err_cnt(struct nanohub_data *data)
{
	data->kthread_err_cnt = data->wakeup_err_cnt = 0;
}

int request_wakeup_ex(struct nanohub_data *data, long timeout_ms,
		      int key, int lock_mode)
{
	long timeout;
	bool priority_lock = lock_mode > LOCK_MODE_NORMAL;
	ktime_t ktime_delta;
	ktime_t wakeup_ktime;
	unsigned long flag;
	int wakeup_acquired_org;

	spin_lock_irqsave(&data->wakeup_wait.lock, flag);
	wakeup_acquired_org = atomic_read(&data->wakeup_acquired);
	mcu_wakeup_mailbox_get_locked(data, priority_lock);
	timeout = (timeout_ms != MAX_SCHEDULE_TIMEOUT) ?
		   msecs_to_jiffies(timeout_ms) :
		   MAX_SCHEDULE_TIMEOUT;

	if (!priority_lock && !data->wakeup_err_cnt)
		wakeup_ktime = ktime_get_boottime();
	timeout = wait_event_interruptible_timeout_locked(
			data->wakeup_wait,
			((priority_lock || nanohub_irq_fired(data)) &&
			 mcu_wakeup_try_lock(data, key)),
			timeout
		  );

	set_chub_user(data, true, timeout, lock_mode);
	if (timeout <= 0) {
		nanohub_info("%s: wakeup_acquire: %d->%d, timeout:%d/%d, \
			err_cnt:%d, priority:%d, key:%d, err_cnt:%d\n",
			current->comm,
			wakeup_acquired_org, atomic_read(&data->wakeup_acquired),
			timeout, timeout_ms, data->wakeup_err_cnt, priority_lock, key,
			data->wakeup_cnt_acq_err);
		if (!timeout && !priority_lock) {
			const struct nanohub_platform_data *pdata = data->pdata;
			struct contexthub_ipc_info *chub = pdata->mailbox_client;

			nanohub_info("wakeup: timeout:%d/%d, err_cnt:%d, \
				priority:%d, ap_int:%d, cur->status:%d, \
				lock:%d, reset:%d, w:%d, w_lock:%d, w_acq:%d(key:%d)\n",
				timeout, timeout_ms, data->wakeup_err_cnt, priority_lock,
				nanohub_irq_fired(data), current->state,
				lock_mode, atomic_read(&chub->in_reset),
				atomic_read(&data->wakeup_cnt), atomic_read(&data->wakeup_lock_cnt),
				atomic_read(&data->wakeup_acquired), key);
			print_chub_user(data);
			if (!data->wakeup_err_cnt)
				data->wakeup_err_ktime = wakeup_ktime;
			ktime_delta = ktime_sub(ktime_get_boottime(),
						data->wakeup_err_ktime);
			data->wakeup_err_cnt++;
			if (ktime_to_ns(ktime_delta) > WAKEUP_ERR_TIME_NS
				&& data->wakeup_err_cnt > WAKEUP_ERR_CNT) {
					mcu_wakeup_mailbox_put_locked(data, priority_lock);
				spin_unlock_irqrestore(&data->wakeup_wait.lock, flag);
				nanohub_info("wakeup: hard reset due to consistent error:timeout:%d/%d, err_cnt:%d\n",
					timeout, timeout_ms, data->wakeup_err_cnt);
				return -ETIME;
			}
		}
		/* request_wakeup_ex() is called without release_wakeup_ex().
		   request_wakeup_ex() reduces the following counts. but, it isn't called */
		mcu_wakeup_mailbox_put_locked(data, priority_lock);
		if (atomic_read(&data->wakeup_acquired)) {
			data->wakeup_cnt_acq_err++;
			nanohub_err("wakeup: critical err: w:%d, w_lock:%d, w_acq:%d\n",
				atomic_read(&data->wakeup_cnt),
				atomic_read(&data->wakeup_lock_cnt),
				atomic_read(&data->wakeup_acquired));
		} else
			data->wakeup_cnt_acq_err = 0;

		if (timeout == 0)
			timeout = -ETIME;
	} else {
		data->wakeup_err_cnt = 0;
		timeout = 0;
	}
	set_chub_user_info(&data->chub_user.user_acq, 0, lock_mode);
	spin_unlock_irqrestore(&data->wakeup_wait.lock, flag);

	return timeout;
}

void release_wakeup_ex(struct nanohub_data *data, int key, int lock_mode)
{
	bool done;
	bool priority_lock = lock_mode > LOCK_MODE_NORMAL;
	unsigned long flag;

	spin_lock_irqsave(&data->wakeup_wait.lock, flag);

	set_chub_user(data, false, 0, lock_mode);
	done = mcu_wakeup_mailbox_put_locked(data, priority_lock);
	mcu_wakeup_unlock(data, key);

	spin_unlock_irqrestore(&data->wakeup_wait.lock, flag);

	if (!done)
		wake_up_interruptible_sync(&data->wakeup_wait);
	else if (nanohub_irq_fired(data))
		nanohub_notify_thread(data);
}

int nanohub_wakeup_eom(struct nanohub_data *data, bool repeat)
{
	int ret = -EFAULT;
#ifdef LOWLEVEL_DEBUG
	int wakeup_flag = 0;
#endif
	unsigned long flag;

	spin_lock_irqsave(&data->wakeup_wait.lock, flag);
	if (mcu_wakeup_mailbox_is_locked(data)) {
		mcu_wakeup_mailbox_set_value(data, 1);
		if (repeat)
			mcu_wakeup_mailbox_set_value(data, 0);
		ret = 0;
	}

#ifdef LOWLEVEL_DEBUG
		wakeup_flag = 1;
#endif

	spin_unlock_irqrestore(&data->wakeup_wait.lock, flag);

	return ret;
}

static inline int nanohub_wakeup_lock(struct nanohub_data *data, int mode)
{
	int ret;

	ret = request_wakeup_ex(data,
				mode == LOCK_MODE_SUSPEND_RESUME ?
				SUSPEND_TIMEOUT_MS : WAKEUP_TIMEOUT_MS,
				KEY_WAKEUP_LOCK, mode);
	if (ret < 0)
		return ret;

	atomic_set(&data->lock_mode, mode);
	mcu_wakeup_mailbox_set_value(data, true);

	return 0;
}

/* returns lock mode used to perform this lock */
static inline int nanohub_wakeup_unlock(struct nanohub_data *data)
{
	int mode = atomic_read(&data->lock_mode);

	atomic_set(&data->lock_mode, LOCK_MODE_NONE);
	release_wakeup_ex(data, KEY_WAKEUP_LOCK, mode);
	nanohub_notify_thread(data);

	return mode;
}

static void __nanohub_hw_reset(struct nanohub_data *data, int boot0)
{
	contexthub_reset(data->pdata->mailbox_client, 1, CHUB_ERR_NONE);
}

static void nanohub_reset_status(struct nanohub_data *data);
static inline void nanohub_print_status(struct nanohub_data *data, const char *caller)
{
	nanohub_info("%s by %s: nano(%d,%d), wl(%d,%d,%d), lock:%d,in-reset:%d, time(%llu,%d,%llu,%d,%d)\n",
		__func__, caller,
		nanohub_get_state(data),
		atomic_read(&data->kthread_run),
		atomic_read(&data->wakeup_lock_cnt),
		atomic_read(&data->wakeup_cnt),
		atomic_read(&data->wakeup_acquired),
		atomic_read(&data->lock_mode),
		atomic_read(&data->in_reset),
		data->wakeup_err_ktime,
		data->wakeup_err_cnt,
		data->kthread_err_ktime,
		data->kthread_err_cnt,
		data->wakeup_cnt_acq_err);
}

int nanohub_hw_reset(struct nanohub_data *data)
{
	int ret;
	const struct nanohub_platform_data *pdata = data->pdata;
	struct contexthub_ipc_info *chub = pdata->mailbox_client;

	nanohub_dev_info(data->io.dev, "%s enter!\n", __func__);
	atomic_set(&data->in_reset, 1);
	ret = nanohub_wakeup_lock(data, LOCK_MODE_RESET);
	if (!ret) {
		__nanohub_hw_reset(data, 0);
		nanohub_reset_status(data);
		nanohub_wakeup_unlock(data);
	}
	atomic_set(&data->in_reset, 0);
	nanohub_print_status(data, __func__);
	nanohub_dev_info(data->io.dev, "%s finish: chub_status:%d, ret:%d\n",
			 __func__, chub->chub_status, ret);
	return ret;
}

static ssize_t nanohub_try_hw_reset(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);
	int ret;

	ret = nanohub_hw_reset(data);

	return ret < 0 ? ret : count;
}

static ssize_t chub_chipid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 senstype = (u8)ipc_read_value(IPC_VAL_A2C_CHIPID);
	u32 id = 0;
	int trycnt = 0;

	nanohub_dev_info(dev, "%s: senstype:%d\n", __func__, senstype);
	do {
		id = (u32)ipc_get_sensor_info(senstype);
		if (!id && (++trycnt > WAIT_TRY_CNT)) {
			nanohub_warn("%s: can't get result: senstype:%d, id:%d, trycnt:%d\n",
					__func__, senstype, id);
			msleep(WAIT_CHUB_MS);
			break;
		}
	} while (!id);

	nanohub_dev_info(dev, "%s: senstype:%d, id:%d\n", __func__, senstype, id);
	if (id)
		return sprintf(buf, "0x%x\n", id);
	else
		return 0;
}

static ssize_t chub_chipid_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	long id;
	int err = kstrtol(&buf[0], 10, &id);

	nanohub_dev_info(dev, "%s: id: %d\n", __func__, id);
	if (!err) {
		ipc_write_value(IPC_VAL_A2C_CHIPID, (u32)id);
		return count;
	} else {
		return 0;
	}
}

static ssize_t chub_sensortype_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	return 0;
}

static ssize_t chub_sensortype_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);

	return contexthub_get_sensortype(data->pdata->mailbox_client, buf);
}

void nanohub_add_dump_request(struct nanohub_data *data)
{
	struct nanohub_io *io = &data->io;
	struct nanohub_buf *buf = nanohub_io_get_buf(&data->free_pool, false);
	uint32_t *buffer;

	if (buf) {
		buffer = (uint32_t *)buf->buffer;
		*buffer = EVT_DEBUG_DUMP;
		buf->length += sizeof(EVT_DEBUG_DUMP);
		nanohub_io_put_buf(io, buf);
		chub_wake_lock_timeout(data->ws, msecs_to_jiffies(250));
	} else {
		nanohub_err("%s: can't get io buf\n", __func__);
	}
}

static ssize_t chub_dumpio_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);
	struct nanohub_buf *desc = NULL;
	int buf_io_cnt = 0;
	int free_io_cnt = 0;

	list_for_each_entry(desc, &data->io.buf_list, list)
		buf_io_cnt++;
	list_for_each_entry(desc, &data->free_pool.buf_list, list)
		free_io_cnt++;

	return sprintf(buf, "%s: sensor:%d free:%d \n", __func__, buf_io_cnt, free_io_cnt);
}

static ssize_t nanohub_sync_memlogger(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);

	contexthub_sync_memlogger(data->pdata->mailbox_client);

	return count;
}

static struct device_attribute attributes[] = {
	__ATTR(reset, 0220, NULL, nanohub_try_hw_reset),
	__ATTR(chipid, 0664, chub_chipid_show, chub_chipid_store),
	__ATTR(sensortype, 0775, chub_sensortype_show, chub_sensortype_store),
	__ATTR(dumpio, 0440, chub_dumpio_show, NULL),
	__ATTR(sync_memlogger, 0220, NULL, nanohub_sync_memlogger),
};

static inline int nanohub_create_sensor(struct nanohub_data *data)
{
	int i, ret;
	struct device *sensor_dev = data->io.dev;

	for (i = 0, ret = 0; i < ARRAY_SIZE(attributes); i++) {
		ret = device_create_file(sensor_dev, &attributes[i]);
		if (ret) {
			nanohub_dev_err(sensor_dev,
				"create sysfs attr %d [%s] failed; err=%d\n",
				i, attributes[i].attr.name, ret);
			goto fail_attr;
		}
	}

	ret = sysfs_create_link(&sensor_dev->kobj,
				&data->iio_dev->dev.kobj, "iio");
	if (ret) {
		nanohub_dev_err(sensor_dev,
			"sysfs_create_link failed; err=%d\n", ret);
		goto fail_attr;
	}
	goto done;

fail_attr:
	for (i--; i >= 0; i--)
		device_remove_file(sensor_dev, &attributes[i]);
done:
	return ret;
}

static int nanohub_create_devices(struct nanohub_data *data)
{
	int ret;
	struct device *chub_dev;
	struct nanohub_io *io = &data->io;

	nanohub_io_init(io, data, device_create(sensor_class, NULL,
						MKDEV(major, 0), io, "nanohub"));
	if (IS_ERR(io->dev)) {
		ret = PTR_ERR(io->dev);
		nanohub_err("nanohub: device_create failed for nanohub; err=%d\n", ret);
		goto fail_dev;
	}
	chub_dev = device_create(sensor_class, NULL, MKDEV(chub_dev_major, 0), NULL, "chub_dev");
	if(chub_dev == NULL)
		nanohub_info("nanohub: device_create failed for chub_dev!\n");
	ret = nanohub_create_sensor(data);
	if (!ret)
		goto done;

fail_dev:
	device_destroy(sensor_class, MKDEV(major, 0));
	device_destroy(sensor_class, MKDEV(chub_dev_major, 0));
done:
	return ret;
}

static int nanohub_match_devt(struct device *dev, const void *data)
{
	const dev_t *devt = data;

	return dev->devt == *devt;
}

static int nanohub_open(struct inode *inode, struct file *file)
{
	dev_t devt = inode->i_rdev;
	struct device *dev;
	struct nanohub_io *io;

	dev = class_find_device(sensor_class, NULL, &devt, nanohub_match_devt);
	if (dev) {
		file->private_data = dev_get_drvdata(dev);
		nonseekable_open(inode, file);
		io = file->private_data;
		if (!contexthub_poweron(io->data->pdata->mailbox_client)) {
			io->data->thread = kthread_run(nanohub_kthread, io->data, "nanohub");
			usleep_range(30, 100);
		}

		return 0;
	}

	return -ENODEV;
}

static ssize_t nanohub_read(struct file *file, char *buffer, size_t length,
			    loff_t *offset)
{
	struct nanohub_io *io = file->private_data;
	struct nanohub_data *data = io->data;
	struct nanohub_buf *buf;
	int ret;

	if (!nanohub_io_has_buf(io) && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;

	buf = nanohub_io_get_buf(io, true);
	if (IS_ERR_OR_NULL(buf))
		return PTR_ERR(buf);

	ret = copy_to_user(buffer, buf->buffer, buf->length);
	if (ret != 0)
		ret = -EFAULT;
	else
		ret = buf->length;

	if (nanohub_is_reset_notify_io(buf)) {
		io = &io->data->free_pool;
		spin_lock(&io->buf_wait.lock);
		list_add_tail(&buf->list, &io->buf_list);
		spin_unlock(&io->buf_wait.lock);
	} else
		nanohub_io_put_buf(&data->free_pool, buf);

	return ret;
}

#define CHUB_RESET_THOLD (3)
#define CHUB_RESET_THOLD_MINOR (5)

static void nanohub_status_dump(struct nanohub_data *data, const char *caller)
{
	const struct nanohub_platform_data *pdata = data->pdata;
	struct contexthub_ipc_info *chub = pdata->mailbox_client;
	int i = 0;

	nanohub_print_status(data, caller);
	print_chub_user(data);
	for (i = 0; i < CHUB_ERR_MAX; i++)
		if (chub->err_cnt[i])
			nanohub_info("%s: ipc err%d:%d\n", __func__, i, chub->err_cnt[i]);
}

static void nanohub_reset_status(struct nanohub_data *data)
{
	nanohub_info("%s\n", __func__);
	msleep_interruptible(WAKEUP_TIMEOUT_MS);
	nanohub_set_state(data, ST_RUNNING);
	nanohub_clear_err_cnt(data);
	nanohub_status_dump(data, __func__);
}

static void chub_error_check(struct nanohub_data *data)
{
	struct contexthub_ipc_info *chub = data->pdata->mailbox_client;
	int i;
	int thold;

	for (i = 0; i < CHUB_ERR_NEED_RESET; i++) {
		if (chub->err_cnt[i]) {
			thold = (i < CHUB_ERR_CRITICAL) ? 1 :
				((i < CHUB_ERR_MAJER) ? CHUB_RESET_THOLD : CHUB_RESET_THOLD_MINOR);

			if (chub->err_cnt[i] >= thold)
				contexthub_handle_debug(data->pdata->mailbox_client, i);
		}
	}
}

static DEFINE_MUTEX(chub_dfs_mutex);

static void contexthub_wait_dfs_scan(struct contexthub_ipc_info *chub)
{
	u32 trycnt = 0;

	mutex_lock(&chub_dfs_mutex);
	if (chub->chub_dfs_gov == DFS_GVERNOR_MAX) {
		if (contexthub_get_token(chub)) {
			nanohub_warn("%s: chub is not running, exit scan", __func__);
			mutex_unlock(&chub_dfs_mutex);
			return;
		}
		nanohub_dev_info(chub->dev, "%s wait for dfs scanning. dfs table:%d\n",
				 __func__, ipc_get_dfs_numSensor());

		while ((ipc_get_dfs_gov() == DFS_GVERNOR_MAX) &&
			(trycnt++ < (WAIT_CHUB_DFS_SCAN_MS_MAX / WAIT_CHUB_DFS_SCAN_MS))) {
			nanohub_dev_info(chub->dev, "%s dfs scanning(%d). dfs gov:%d, table:%d\n",
					 __func__, trycnt, ipc_get_dfs_gov(),
					 ipc_get_dfs_numSensor());
			contexthub_put_token(chub);
			msleep(WAIT_CHUB_DFS_SCAN_MS);
			if (contexthub_get_token(chub)) {
				nanohub_warn("%s: chub is not running, exit scan", __func__);
				mutex_unlock(&chub_dfs_mutex);
				return;
			}
		}

		chub->chub_dfs_gov = ipc_get_dfs_gov();
		nanohub_dev_info(chub->dev, "%s dfs scanning (%d) done. dfs:%d table:%d\n",
				 __func__, trycnt, chub->chub_dfs_gov, ipc_get_dfs_numSensor());
		if (chub->chub_dfs_gov == DFS_GVERNOR_MAX) {
			nanohub_dev_err(chub->dev, "%s fails to get dfs scanning\n", __func__);
			/* set chub_dfs_gov to 0 not to call this function again */
			chub->chub_dfs_gov = 0;
		}
		contexthub_put_token(chub);
	}
	mutex_unlock(&chub_dfs_mutex);
}

static ssize_t nanohub_write(struct file *file, const char *buffer,
			     size_t length, loff_t *offset)
{
	struct nanohub_io *io = file->private_data;
	struct nanohub_data *data = io->data;
	struct contexthub_ipc_info *chub = data->pdata->mailbox_client;
	int ret;

	/* check nanohub is in reset */
	if ((atomic_read(&chub->chub_status) != CHUB_ST_RUN) || atomic_read(&data->in_reset)) {
		nanohub_dev_warn(data->io.dev,
			"%s fails. nanohub isn't running: %d, %d\n", __func__,
			atomic_read(&chub->chub_status), atomic_read(&data->in_reset));
		return -EINVAL;
	}

	if (chub->chub_dfs_gov == DFS_GVERNOR_MAX)
		contexthub_wait_dfs_scan(chub);

	/* wakeup timeout should be bigger than timeout_write (544) to support both usecase */
	ret = request_wakeup_timeout(data, 644);
	if (ret) {
		nanohub_dev_warn(data->io.dev,
			"%s fails to wakeup. ret:%d\n", __func__, ret);
		contexthub_handle_debug(chub, CHUB_ERR_COMMS_WAKE_ERR);
	} else {
		if (chub->err_cnt[CHUB_ERR_COMMS_WAKE_ERR])
			chub->err_cnt[CHUB_ERR_COMMS_WAKE_ERR] = 0;
	}

	if (ret)
		return ret;

	ret = nanohub_comms_write(data, buffer, length);

	release_wakeup(data);

	return ret;
}

static unsigned int nanohub_poll(struct file *file, poll_table *wait)
{
	struct nanohub_io *io = file->private_data;
	unsigned int mask = POLLOUT | POLLWRNORM;

	poll_wait(file, &io->buf_wait, wait);

	if (nanohub_io_has_buf(io))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int nanohub_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static int nanohub_match_name(struct device *dev, const void *data)
{
	const char *name = data;

	if(dev->kobj.name == NULL) {
		nanohub_info("nanohub device name invalid\n");
		return 0;
	}

	nanohub_info("nanohub device name = %s\n", dev->kobj.name);
	return !strcmp(dev->kobj.name, name);
}

static int chub_dev_open(struct inode *inode, struct file *file)
{
	struct device *dev_nanohub;
	struct nanohub_data *data;
	struct contexthub_ipc_info *chub;

	nanohub_info("%s\n", __func__);
	dev_nanohub = class_find_device(sensor_class, NULL, "nanohub", nanohub_match_name);
	if (!dev_nanohub) {
		nanohub_err("%s: dev_nanohub not available\n", __func__);
		return -ENODEV;
	}

	data = dev_get_nanohub_data(dev_nanohub);
	if (!data) {
		nanohub_err("%s nanohub_data not available\n", __func__);
		return -ENODEV;
	}

	chub = data->pdata->mailbox_client;
	if (!chub) {
		nanohub_err("%s ipc not available\n", __func__);
		return -ENODEV;
	}
	file->private_data = chub;

	nanohub_info("%s open chub dev\n", __func__);
	return 0;
}

static ssize_t chub_dev_read(struct file *file, char *buffer,
			     size_t length, loff_t *offset)
{
	return 0;
}

static ssize_t chub_dev_write(struct file *file, const char *buffer,
			     size_t length, loff_t *offset)
{
	struct contexthub_ipc_info *chub = file->private_data;
	int8_t num_os;
	int ret;

	if (!chub) {
		nanohub_err("%s: dev_nanohub not available\n", __func__);
		return -EINVAL;
	}

	/* read int8_t num_os */
	ret = copy_from_user(&num_os, buffer, sizeof(num_os));

	if(num_os > 0 && num_os < SENSOR_VARIATION) {
		chub->multi_os = false;
		chub->num_os = num_os;
		nanohub_info("%s saved os name: %s\n", __func__, os_image[num_os]);
	} else {
		nanohub_warn("%s num_os is invalid %d\n", __func__, num_os);
	}

	return 0;
}

static void nanohub_destroy_devices(struct nanohub_data *data)
{
	int i;
	struct device *sensor_dev = data->io.dev;

	sysfs_remove_link(&sensor_dev->kobj, "iio");
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(sensor_dev, &attributes[i]);
	device_destroy(sensor_class, MKDEV(major, 0));
	device_destroy(sensor_class, MKDEV(chub_dev_major, 0));
}

static void nanohub_process_buffer(struct nanohub_data *data,
				   struct nanohub_buf **buf,
				   int ret)
{
	uint32_t event_id;
	uint8_t interrupt;
	bool wakeup = false;
	struct nanohub_io *io = &data->io;

	data->kthread_err_cnt = 0;
	if (ret < 4) {
		release_wakeup(data);
		return;
	}

	(*buf)->length = ret;

	event_id = le32_to_cpu((((uint32_t *)(*buf)->buffer)[0]) & 0x7FFFFFFF);
	if (ret >= sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) &&
	    event_id > FIRST_SENSOR_EVENTID &&
	    event_id <= LAST_SENSOR_EVENTID) {
		interrupt = (*buf)->buffer[sizeof(uint32_t) +
					   sizeof(uint64_t) + 3];
		if (interrupt == WAKEUP_INTERRUPT)
			wakeup = true;
	}

	nanohub_io_put_buf(io, *buf);

	*buf = NULL;
	/* (for wakeup interrupts): hold a wake lock for 250ms so the sensor hal
	 * has time to grab its own wake lock */
	if (wakeup) {
		u64 diff_time = ktime_get_boottime_ns() - data->wakelock_req_time;

		if ((diff_time / 1000000) > 1) {
			nanohub_dev_info(io->dev, "%s: prev:%lld, diff:%lld\n", __func__, data->wakelock_req_time, diff_time);
			data->wakelock_req_time = ktime_get_boottime_ns();
			chub_wake_lock_timeout(data->ws, msecs_to_jiffies(250));
		} else
			nanohub_dev_warn(io->dev, "%s: skip wakelock: prev:%lld, diff:%lld\n", __func__, data->wakelock_req_time, diff_time);
	}
	release_wakeup(data);
}

static int nanohub_kthread(void *arg)
{
	struct nanohub_data *data = (struct nanohub_data *)arg;
	struct nanohub_buf *buf = NULL;
	int ret;
	ktime_t ktime_delta;
	uint32_t clear_interrupts[8] = { 0x00000006 };
	static const struct sched_param param = {
		.sched_priority = (MAX_USER_RT_PRIO/2)-1,
	};

	data->kthread_err_cnt = 0;
	sched_setscheduler(current, SCHED_FIFO, &param);
	nanohub_set_state(data, ST_IDLE);

	while (!kthread_should_stop()) {
		chub_error_check(data);
		switch (nanohub_get_state(data)) {
		case ST_IDLE:
			wait_event_interruptible(data->kthread_wait,
						 atomic_read(&data->kthread_run)
						 );
			nanohub_set_state(data, ST_RUNNING);
			break;
		case ST_ERROR:
			ktime_delta = ktime_sub(ktime_get_boottime(),
						data->kthread_err_ktime);
			if (ktime_to_ns(ktime_delta) > KTHREAD_ERR_TIME_NS
				&& data->kthread_err_cnt > KTHREAD_ERR_CNT) {
				nanohub_info("kthread: hard reset due to consistent error\n");
				ret = nanohub_hw_reset(data);
				if (ret) {
					nanohub_info("%s: failed to reset nanohub: ret=%d\n",
						__func__, ret);
				}
			}
			msleep_interruptible(WAKEUP_TIMEOUT_MS);
			nanohub_set_state(data, ST_RUNNING);
			break;
		case ST_RUNNING:
			break;
		}
		atomic_set(&data->kthread_run, 0);
		if (!buf)
			buf = nanohub_io_get_buf(&data->free_pool,
						 false);
		if (buf) {
			ret = request_wakeup_timeout(data, 600);
			if (ret) {
				nanohub_info("%s: request_wakeup_timeout: ret=%d, err_cnt:%d\n",
					 __func__, ret, data->kthread_err_cnt);
				continue;
			}

			ret = nanohub_comms_tx_rx_retrans(
			    data, CMD_COMMS_READ, 0, 0, buf->buffer,
			    sizeof(buf->buffer), false, 10, 0);

			if (ret > 0) {
				nanohub_process_buffer(data, &buf, ret);
				if (!nanohub_irq_fired(data)) {
					nanohub_set_state(data, ST_IDLE);
					continue;
				}
			} else if (ret == 0) {
				/* queue empty, go to sleep */
				data->kthread_err_cnt = 0;
				data->interrupts[0] &= ~0x00000006;
				release_wakeup(data);
				nanohub_set_state(data, ST_IDLE);
				continue;
			} else {
#ifdef CHUB_USER_DEBUG
				nanohub_err("%s#1: kthread_err_cnt=%d\n",
					__func__,
					data->kthread_err_cnt);
				nanohub_status_dump(data, __func__);
#endif
				release_wakeup(data);
				if (data->kthread_err_cnt == 0)
					data->kthread_err_ktime =
						ktime_get_boottime();

				data->kthread_err_cnt++;
				if (data->kthread_err_cnt >= KTHREAD_WARN_CNT) {
					nanohub_err("%s: kthread_err_cnt=%d\n",
						__func__,
						data->kthread_err_cnt);
					nanohub_set_state(data, ST_ERROR);
					continue;
				}
			}
		} else {
			if (!nanohub_irq_fired(data)) {
				nanohub_set_state(data, ST_IDLE);
				continue;
			}
			/* pending interrupt, but no room to read data -
			 * clear interrupts */
			if (request_wakeup(data))
				continue;
			nanohub_comms_tx_rx_retrans(data,
						    CMD_COMMS_CLR_GET_INTR,
						    (uint8_t *)
						    clear_interrupts,
						    sizeof(clear_interrupts),
						    (uint8_t *) data->
						    interrupts,
						    sizeof(data->interrupts),
						    false, 10, 0);
			release_wakeup(data);
			nanohub_set_state(data, ST_IDLE);
		}
	}

	return 0;
}

struct iio_dev *nanohub_probe(struct device *dev, struct iio_dev *iio_dev)
{
	int ret, i;
	struct nanohub_platform_data *pdata;
	struct nanohub_data *data;
	struct nanohub_buf *buf;
	bool own_iio_dev = !iio_dev;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	if (own_iio_dev) {
		iio_dev = iio_device_alloc(dev, sizeof(struct nanohub_data));
		if (!iio_dev)
			return ERR_PTR(-ENOMEM);
	}

	iio_dev->name = "nanohub";
	iio_dev->driver_module = THIS_MODULE;
	iio_dev->dev.parent = dev;
	iio_dev->info = &nanohub_iio_info;
	iio_dev->channels = NULL;
	iio_dev->num_channels = 0;

	data = iio_priv(iio_dev);
	data->iio_dev = iio_dev;
	data->pdata = pdata;

	init_waitqueue_head(&data->kthread_wait);

	nanohub_io_init(&data->free_pool, data, dev);

	buf = vmalloc(sizeof(*buf) * READ_QUEUE_DEPTH);
	data->vbuf = buf;
	if (!buf) {
		ret = -ENOMEM;
		goto fail_vma;
	}

	for (i = 0; i < READ_QUEUE_DEPTH; i++)
		nanohub_io_put_buf(&data->free_pool, &buf[i]);
	atomic_set(&data->kthread_run, 0);
	data->ws = chub_wake_lock_init(dev, "nanohub_wakelock_read");

	atomic_set(&data->lock_mode, LOCK_MODE_NONE);
	atomic_set(&data->wakeup_cnt, 0);
	atomic_set(&data->wakeup_lock_cnt, 0);
	atomic_set(&data->wakeup_acquired, 0);
	atomic_set(&data->in_reset, 0);
	init_waitqueue_head(&data->wakeup_wait);

	ret = iio_device_register(iio_dev);
	if (ret) {
		nanohub_err("nanohub: iio_device_register failed\n");
		goto fail_irq;
	}

	ret = nanohub_create_devices(data);
	if (ret)
		goto fail_dev;
	usleep_range(30, 100);

	return iio_dev;

fail_dev:
	iio_device_unregister(iio_dev);
fail_irq:
	chub_wake_lock_destroy(data->ws);
	vfree(buf);
fail_vma:
	if (own_iio_dev)
		iio_device_free(iio_dev);

	return ERR_PTR(ret);
}

int nanohub_remove(struct iio_dev *iio_dev)
{
	struct nanohub_data *data = iio_priv(iio_dev);

	nanohub_notify_thread(data);
	kthread_stop(data->thread);

	nanohub_destroy_devices(data);
	iio_device_unregister(iio_dev);
	chub_wake_lock_destroy(data->ws);
	vfree(data->vbuf);
	iio_device_free(iio_dev);

	return 0;
}

int nanohub_init(void)
{
	int ret = 0;

	sensor_class = class_create(THIS_MODULE, "nanohub");
	if (IS_ERR(sensor_class)) {
		ret = PTR_ERR(sensor_class);
		pr_err("nanohub: class_create failed; err=%d\n", ret);
	}

	if (!ret)
		major = __register_chrdev(0, 0, 1, "nanohub", &nanohub_fileops);

	chub_dev_major = __register_chrdev(0, 0, 1, "chub_dev", &chub_dev_fileops);
	if(chub_dev_major < 0) {
		pr_info("nanohub : can't register chub_dev; err = %d\n", chub_dev_major);
		chub_dev_major = 0;
	} else {
		pr_info("nanohub : registered chub_dev; %d\n", chub_dev_major);
	}

	if (major < 0) {
		ret = major;
		major = 0;
		pr_err("nanohub: can't register; err=%d\n", ret);
	} else
		pr_info("nanohub : registered major; %d\n", major);

	pr_info("nanohub: loaded; ret=%d\n", ret);
	return ret;
}

void nanohub_cleanup(void)
{
	__unregister_chrdev(major, 0, 1, "nanohub");
	__unregister_chrdev(chub_dev_major, 0, 1, "chub_dev");
	class_destroy(sensor_class);
	major = 0;
	sensor_class = 0;
}

MODULE_AUTHOR("Ben Fennema");
MODULE_LICENSE("GPL");
