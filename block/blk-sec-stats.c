// SPDX-License-Identifier: GPL-2.0
/*
 *  Samsung Block Statistics
 *
 *  Copyright (C) 2021 Manjong Lee <mj0123.lee@samsung.com>
 *  Copyright (C) 2021 Junho Kim <junho89.kim@samsung.com>
 *  Copyright (C) 2021 Changheun Lee <nanich.lee@samsung.com>
 *  Copyright (C) 2021 Seunghwan Hyun <seunghwan.hyun@samsung.com>
 *  Copyright (C) 2021 Tran Xuan Nam <nam.tx2@samsung.com>
 */

#include <linux/types.h>
#include <linux/time64.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/part_stat.h>
#include <linux/blk-mq.h>

#include "blk-mq.h"
#include "blk-mq-tag.h"

#define MAX_PIO_NODE_NUM	10000
#define SORT_PIO_NODE_NUM	100

struct disk_info {
	/* fields related with target device itself */
	struct gendisk *gd;
	struct request_queue *queue;
};

struct accumulated_stats {
	struct timespec64 uptime;
	unsigned long sectors[3];       /* READ, WRITE, DISCARD */
	unsigned long ios[3];
	unsigned long iot;
};

struct pio_node {
	struct list_head list;

	pid_t tgid;
	char name[TASK_COMM_LEN];
	u64 start_time;

	unsigned long long bytes[REQ_OP_DISCARD + 1];
};

static unsigned long long transferred_bytes;

static struct disk_info internal_disk;
static unsigned int internal_min_size_mb = 10 * 1024; /* 10GB */

static struct accumulated_stats old, new;

static DEFINE_SPINLOCK(pio_list_lock);
static DEFINE_SPINLOCK(others_pio_lock);
LIST_HEAD(pio_list);
static int pio_cnt;
static int pio_enabled;
static unsigned int pio_duration_ms = 5000;
static unsigned long pio_timeout;
static struct kmem_cache *pio_cache;

static struct pio_node others = {
	.list = LIST_HEAD_INIT(others.list),
	.tgid = 99999,
	.name = "others",
	.start_time = 9999999,
	.bytes = {0, 0, 0, 0},
};

#define SECTORS2MB(x) ((x) / 2 / 1024)

#define SCSI_DISK0_MAJOR 8
#define MMC_BLOCK_MAJOR 179
#define MAJOR8_DEV_NUM 16      /* maximum number of minor devices in scsi disk0 */
#define SCSI_MINORS 16         /* first minor number of scsi disk0 */
#define MMC_TARGET_DEV 16      /* number of mmc devices set of target (maximum 256) */
#define MMC_MINORS 8           /* first minor number of mmc disk */

static bool is_internal_bdev(struct block_device *dev)
{
	int size_mb;

	if (bdev_is_partition(dev))
		return false;

	if (dev->bd_disk->flags & GENHD_FL_REMOVABLE)
		return false;

	size_mb = SECTORS2MB(get_capacity(dev->bd_disk));
	if (size_mb >= internal_min_size_mb)
		return true;

	return false;
}


static struct gendisk *get_internal_disk(void)
{
	struct block_device *bdev;
	struct gendisk *gd = NULL;
	int idx;
	dev_t devno = MKDEV(0, 0);

	for (idx = 0; idx < MAJOR8_DEV_NUM; idx++) {
		devno = MKDEV(SCSI_DISK0_MAJOR, SCSI_MINORS * idx);
		bdev = blkdev_get_by_dev(devno, FMODE_READ, NULL);
		if (IS_ERR(bdev))
			continue;

		if (bdev->bd_disk && is_internal_bdev(bdev))
			gd = bdev->bd_disk;

		blkdev_put(bdev, FMODE_READ);

		if (gd)
			return gd;
	}

	for (idx = 0; idx < MMC_TARGET_DEV; idx++) {
		devno = MKDEV(MMC_BLOCK_MAJOR, MMC_MINORS * idx);
		bdev = blkdev_get_by_dev(devno, FMODE_READ, NULL);
		if (IS_ERR(bdev))
			continue;

		if (bdev->bd_disk && is_internal_bdev(bdev))
			gd = bdev->bd_disk;

		blkdev_put(bdev, FMODE_READ);

		if (gd)
			return gd;
	}

	return NULL;
}

static inline int init_internal_disk_info(void)
{
	/* it only sets internal_disk.gd info.
	 * internal_disk.rq_infos have to be allocated later.
	 */
	if (!internal_disk.gd) {
		internal_disk.gd = get_internal_disk();
		if (unlikely(!internal_disk.gd)) {
			pr_err("%s: can't find internal disk\n", __func__);
			return -ENODEV;
		}
	}
	internal_disk.queue = internal_disk.gd->queue;

	return 0;
}

static inline void clear_internal_disk_info(void)
{
	internal_disk.gd = NULL;
	internal_disk.queue = NULL;
}

static inline bool has_valid_disk_info(void)
{
	return !!internal_disk.queue;
}

void blk_sec_stats_account_init(struct request_queue *q)
{
	int ret;

	if (!has_valid_disk_info()) {
		ret = init_internal_disk_info();
		if (ret) {
			clear_internal_disk_info();
			pr_err("%s: Can't find internal disk info!", __func__);
			return;
		}
	}
}
EXPORT_SYMBOL(blk_sec_stats_account_init);

void blk_sec_stats_account_exit(struct elevator_queue *eq)
{
}
EXPORT_SYMBOL(blk_sec_stats_account_exit);

#define UNSIGNED_DIFF(n, o) (((n) >= (o)) ? ((n) - (o)) : ((n) + (0 - (o))))
#define SECTORS2KB(x) ((x) / 2)

static inline void get_monotonic_boottime(struct timespec64 *ts)
{
	*ts = ktime_to_timespec64(ktime_get_boottime());
}

static ssize_t diskios_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret;
	struct hd_struct *hd;
	long hours;

	if (unlikely(!has_valid_disk_info()))
		return -EINVAL;

	hd = &(internal_disk.gd->part0);

	new.ios[STAT_READ] = part_stat_read(hd, ios[STAT_READ]);
	new.ios[STAT_WRITE] = part_stat_read(hd, ios[STAT_WRITE]);
	new.ios[STAT_DISCARD] = part_stat_read(hd, ios[STAT_DISCARD]);
	new.sectors[STAT_READ] = part_stat_read(hd, sectors[STAT_READ]);
	new.sectors[STAT_WRITE] = part_stat_read(hd, sectors[STAT_WRITE]);
	new.sectors[STAT_DISCARD] = part_stat_read(hd, sectors[STAT_DISCARD]);
	new.iot = jiffies_to_msecs(part_stat_read(hd, io_ticks)) / 1000;

	get_monotonic_boottime(&(new.uptime));
	hours = (new.uptime.tv_sec - old.uptime.tv_sec) / 60;
	hours = (hours + 30) / 60;

	ret = sprintf(buf, "\"ReadC\":\"%lu\",\"ReadKB\":\"%lu\","
			"\"WriteC\":\"%lu\",\"WriteKB\":\"%lu\","
			"\"DiscardC\":\"%lu\",\"DiscardKB\":\"%lu\","
			"\"IOT\":\"%lu\","
			"\"Hours\":\"%ld\"\n",
			UNSIGNED_DIFF(new.ios[STAT_READ], old.ios[STAT_READ]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_READ], old.sectors[STAT_READ])),
			UNSIGNED_DIFF(new.ios[STAT_WRITE], old.ios[STAT_WRITE]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_WRITE], old.sectors[STAT_WRITE])),
			UNSIGNED_DIFF(new.ios[STAT_DISCARD], old.ios[STAT_DISCARD]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_DISCARD], old.sectors[STAT_DISCARD])),
			UNSIGNED_DIFF(new.iot, old.iot),
			hours);

	old.ios[STAT_READ] = new.ios[STAT_READ];
	old.ios[STAT_WRITE] = new.ios[STAT_WRITE];
	old.ios[STAT_DISCARD] = new.ios[STAT_DISCARD];
	old.sectors[STAT_READ] = new.sectors[STAT_READ];
	old.sectors[STAT_WRITE] = new.sectors[STAT_WRITE];
	old.sectors[STAT_DISCARD] = new.sectors[STAT_DISCARD];
	old.uptime = new.uptime;
	old.iot = new.iot;

	return ret;
}

static void add_pio_node(struct request *rq, unsigned int data_size,
		pid_t tgid, const char *tg_name, u64 tg_start_time)
{
	struct pio_node *pio = NULL;
	unsigned long flags;

	if (pio_cnt >= MAX_PIO_NODE_NUM) {
add_others:
		spin_lock_irqsave(&others_pio_lock, flags);
		others.bytes[req_op(rq)] += data_size;
		spin_unlock_irqrestore(&others_pio_lock, flags);
		return;
	}

	pio = kmem_cache_alloc(pio_cache, GFP_NOWAIT);
	if (!pio)
		goto add_others;

	INIT_LIST_HEAD(&pio->list);

	pio->tgid = tgid;
	strncpy(pio->name, tg_name, TASK_COMM_LEN - 1);
	pio->name[TASK_COMM_LEN - 1] = '\0';
	pio->start_time = tg_start_time;

	pio->bytes[REQ_OP_READ] = 0;
	pio->bytes[REQ_OP_WRITE] = 0;
	pio->bytes[REQ_OP_FLUSH] = 0;
	pio->bytes[REQ_OP_DISCARD] = 0;

	pio->bytes[req_op(rq)] = data_size;

	spin_lock_irqsave(&pio_list_lock, flags);
	list_add(&pio->list, &pio_list);
	spin_unlock_irqrestore(&pio_list_lock, flags);

	pio_cnt++;
}

static void free_pio_node(struct list_head *remove_list)
{
	struct pio_node *pio;
	struct pio_node *pion;
	unsigned long flags;

	list_for_each_entry_safe(pio, pion, remove_list, list) {
		list_del(&pio->list);
		kmem_cache_free(pio_cache, pio);
	}

	spin_lock_irqsave(&others_pio_lock, flags);
	others.bytes[REQ_OP_READ] = 0;
	others.bytes[REQ_OP_WRITE] = 0;
	others.bytes[REQ_OP_FLUSH] = 0;
	others.bytes[REQ_OP_DISCARD] = 0;
	spin_unlock_irqrestore(&others_pio_lock, flags);

	pio_cnt = 0;
}

static void update_pio_node(struct request *rq, unsigned int data_size,
		pid_t tgid, const char *tg_name, u64 tg_start_time)
{
	struct pio_node *pio;
	unsigned long size = 0;
	unsigned long flags;
	LIST_HEAD(remove_list);

	if (pio_enabled == 0)
		return;
	if (time_after(jiffies, pio_timeout))
		return;
	if (req_op(rq) > REQ_OP_DISCARD)
		return;

	size = (req_op(rq) == REQ_OP_FLUSH) ? 1 : data_size;

	spin_lock_irqsave(&pio_list_lock, flags);
	list_for_each_entry(pio, &pio_list, list) {
		if (pio->tgid != tgid)
			continue;
		if (pio->start_time != tg_start_time)
			continue;

		strncpy(pio->name, tg_name, TASK_COMM_LEN - 1);
		pio->name[TASK_COMM_LEN - 1] = '\0';
		pio->bytes[req_op(rq)] += size;
		spin_unlock_irqrestore(&pio_list_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&pio_list_lock, flags);

	add_pio_node(rq, data_size, tgid, tg_name, tg_start_time);
}

static inline bool may_account_rq(struct request *rq)
{
	if (unlikely(!has_valid_disk_info()))
		return false;

	if (internal_disk.queue != rq->q)
		return false;

	return true;
}

void blk_sec_stats_account_io_done(struct request *rq, unsigned int data_size,
		pid_t tgid, const char *tg_name, u64 tg_start_time)
{
	if (unlikely(!may_account_rq(rq)))
		return;

	transferred_bytes += data_size;
	update_pio_node(rq, data_size, tgid, tg_name, tg_start_time);
}
EXPORT_SYMBOL(blk_sec_stats_account_io_done);

#define GET_PIO_PRIO(pio) \
	((pio)->bytes[REQ_OP_READ] + (pio)->bytes[REQ_OP_WRITE]*2)

static void sort_pios(struct list_head *remove_list)
{
	struct pio_node *max_pio = NULL;
	struct pio_node *pio;
	unsigned long long max = 0;
	LIST_HEAD(sorted_list);
	int i;

	for (i = 0; i < SORT_PIO_NODE_NUM; i++) {
		list_for_each_entry(pio, remove_list, list) {
			if (GET_PIO_PRIO(pio) > max) {
				max = GET_PIO_PRIO(pio);
				max_pio = pio;
			}
		}
		if (max_pio != NULL)
			list_move_tail(&max_pio->list, &sorted_list);

		max = 0;
		max_pio = NULL;
	}
	list_splice_init(&sorted_list, remove_list);
}

static ssize_t pio_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct pio_node *pio;
	int len = 0;
	unsigned long flags;
	LIST_HEAD(remove_list);

	spin_lock_irqsave(&pio_list_lock, flags);
	list_replace_init(&pio_list, &remove_list);
	spin_unlock_irqrestore(&pio_list_lock, flags);

	if (pio_cnt > SORT_PIO_NODE_NUM)
		sort_pios(&remove_list);

	list_for_each_entry(pio, &remove_list, list) {
		if (PAGE_SIZE - len > 80) {
			/* pid read(KB) write(KB) comm printed */
			len += scnprintf(buf + len, PAGE_SIZE - len, "%d %llu %llu %s\n",
					pio->tgid, pio->bytes[REQ_OP_READ] / 1024,
					pio->bytes[REQ_OP_WRITE] / 1024,  pio->name);
		} else {
			spin_lock_irqsave(&others_pio_lock, flags);
			others.bytes[REQ_OP_READ] += pio->bytes[REQ_OP_READ];
			others.bytes[REQ_OP_WRITE] += pio->bytes[REQ_OP_WRITE];
			others.bytes[REQ_OP_FLUSH] += pio->bytes[REQ_OP_FLUSH];
			others.bytes[REQ_OP_DISCARD] += pio->bytes[REQ_OP_DISCARD];
			spin_unlock_irqrestore(&others_pio_lock, flags);
		}
	}
	spin_lock_irqsave(&others_pio_lock, flags);

	if (others.bytes[REQ_OP_READ] + others.bytes[REQ_OP_WRITE])
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d %llu %llu %s\n",
				others.tgid, others.bytes[REQ_OP_READ] / 1024,
				others.bytes[REQ_OP_WRITE] / 1024,  others.name);
	spin_unlock_irqrestore(&others_pio_lock, flags);

	free_pio_node(&remove_list);

	pio_timeout = jiffies + msecs_to_jiffies(pio_duration_ms);

	return len;
}

static ssize_t pio_enabled_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int enable = 0;
	int ret;
	unsigned long flags;
	LIST_HEAD(remove_list);

	ret = kstrtoint(buf, 10, &enable);
	if (ret)
		return ret;

	pio_enabled = (enable >= 1) ? 1 : 0;

	spin_lock_irqsave(&pio_list_lock, flags);
	list_replace_init(&pio_list, &remove_list);
	spin_unlock_irqrestore(&pio_list_lock, flags);
	free_pio_node(&remove_list);

	pio_timeout = jiffies + msecs_to_jiffies(pio_duration_ms);

	return count;
}

static ssize_t pio_enabled_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = scnprintf(buf, PAGE_SIZE, "%d\n", pio_enabled);

	return len;
}

static ssize_t pio_duration_ms_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint(buf, 10, &pio_duration_ms);
	if (ret)
		return ret;

	if (pio_duration_ms > 10000)
		pio_duration_ms = 10000;

	return count;
}

static ssize_t pio_duration_ms_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = scnprintf(buf, PAGE_SIZE, "%u\n", pio_duration_ms);

	return len;
}

static ssize_t transferred_bytes_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%llu\n", transferred_bytes);
}

static struct kobj_attribute diskios_attr = __ATTR(diskios, 0444, diskios_show,  NULL);
static struct kobj_attribute pios_attr = __ATTR(pios, 0444, pio_show,  NULL);
static struct kobj_attribute pios_enable_attr = __ATTR(pios_enable, 0644,
		pio_enabled_show,  pio_enabled_store);
static struct kobj_attribute pios_duration_ms_attr = __ATTR(pios_duration_ms, 0644,
		pio_duration_ms_show, pio_duration_ms_store);
static struct kobj_attribute transferred_bytes_attr = __ATTR(transferred_bytes, 0444,
		transferred_bytes_show,  NULL);

static struct attribute *blk_sec_stats_attrs[] = {
	&diskios_attr.attr,
	&pios_attr.attr,
	&pios_enable_attr.attr,
	&pios_duration_ms_attr.attr,
	&transferred_bytes_attr.attr,
	NULL,
};

static struct attribute_group blk_sec_stats_group = {
	.attrs = blk_sec_stats_attrs,
	NULL,
};

static struct kobject *blk_sec_stats_kobj;

static int __init blk_sec_stats_init(void)
{
	int retval;

	blk_sec_stats_kobj = kobject_create_and_add("blk_sec_stats", kernel_kobj);
	if (!blk_sec_stats_kobj)
		return -ENOMEM;

	pio_cache = kmem_cache_create("pio_node", sizeof(struct pio_node), 0, 0, NULL);
	if (!pio_cache)
		return -ENOMEM;

	retval = sysfs_create_group(blk_sec_stats_kobj, &blk_sec_stats_group);
	if (retval)
		kobject_put(blk_sec_stats_kobj);

	retval = init_internal_disk_info();
	if (retval) {
		clear_internal_disk_info();
		pr_err("%s: Can't find internal disk info!", __func__);
	}

	return 0;
}

static void __exit blk_sec_stats_exit(void)
{
	clear_internal_disk_info();
	kmem_cache_destroy(pio_cache);
	kobject_put(blk_sec_stats_kobj);
}

module_init(blk_sec_stats_init);
module_exit(blk_sec_stats_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Manjong Lee <mj0123.lee@samsung.com>");
MODULE_DESCRIPTION("SEC Storage stats module for various purposes");
MODULE_VERSION("0.1");
