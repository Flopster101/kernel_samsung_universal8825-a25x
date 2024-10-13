/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *      Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */

#ifndef __CONTEXTHUB_IPC_H_
#define __CONTEXTHUB_IPC_H_

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/debugfs.h>
#include <linux/random.h>
#include <linux/rtc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/timekeeping.h>
#include <linux/of_gpio.h>
#include <linux/fcntl.h>
#include <uapi/linux/sched/types.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/pm_wakeup.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/platform_data/nanohub.h>
#include <linux/sched/clock.h>
#include <linux/sched/signal.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#include <soc/samsung/exynos-s2mpu.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
#include <soc/samsung/sysevent.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#include <soc/samsung/imgloader.h>
#include <linux/firmware.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_PD_EL3)
#include <soc/samsung/exynos-el3_mon.h>
#endif
#include "ipc_chub.h"
#include "chub_log.h"

#define WAIT_TRY_CNT (3)
#define RESET_WAIT_TRY_CNT (10)
#define WAIT_CHUB_MS (100)
#define WAIT_CHUB_DFS_SCAN_MS_MAX (120000)
#define WAIT_CHUB_DFS_SCAN_MS (2000)
#define WAIT_TIMEOUT_MS (1000)
#define MAX_USI_CNT (15)
#define BAAW_MAX (4)
#define BAAW_MAX_WINDOWS (0x10)

enum { CHUB_ON, CHUB_OFF };
enum { C2A_ON, C2A_OFF };

#define EXYNOS_CHUB 2ull
#ifndef EXYNOS_SET_CONN_TZPC
#define EXYNOS_SET_CONN_TZPC (0)
#endif

#define OS_IMAGE_MULTIOS "os.checked_0.bin"
#define OS_IMAGE_DEFAULT "os.checked.bin"

#define SENSOR_VARIATION 10

enum { OS_TYPE_DEFAULT, OS_TYPE_MULTIOS, OS_TYPE_ONEBINARY };

extern const char *os_image[SENSOR_VARIATION];
#define MAX_FIRMWARE_NUM 3
/* utils for nanohub main */
#define wait_event_interruptible_timeout_locked(q, cond, tmo)		\
({									\
	long __ret = (tmo);						\
	DEFINE_WAIT(__wait);						\
	if (!(cond)) {							\
		for (;;) {						\
			__wait.flags &= ~WQ_FLAG_EXCLUSIVE;		\
			if (list_empty(&__wait.entry))			\
				__add_wait_queue_entry_tail(&(q), &__wait);	\
			set_current_state(TASK_INTERRUPTIBLE);		\
			if ((cond))					\
				break;					\
			if (signal_pending(current)) {			\
				__ret = -ERESTARTSYS;			\
				break;					\
			}						\
			spin_unlock_irq(&(q).lock);			\
			__ret = schedule_timeout(__ret);		\
			spin_lock_irq(&(q).lock);			\
			if (!__ret) {					\
				if ((cond))				\
					__ret = 1;			\
				break;					\
			}						\
		}							\
		__set_current_state(TASK_RUNNING);			\
		if (!list_empty(&__wait.entry))				\
			list_del_init(&__wait.entry);			\
		else if (__ret == -ERESTARTSYS &&			\
			 /*reimplementation of wait_abort_exclusive() */\
			 waitqueue_active(&(q)))			\
			__wake_up_locked_key(&(q), TASK_INTERRUPTIBLE,	\
			NULL);						\
	} else {							\
		__ret = 1;						\
	}								\
	__ret;								\
})

enum mailbox_event {
	MAILBOX_EVT_UTC_MAX = IPC_DEBUG_UTC_MAX,
	MAILBOX_EVT_DUMP_STATUS = IPC_DEBUG_DUMP_STATUS,
	MAILBOX_EVT_DFS_GOVERNOR = IPC_DEBUG_DFS_GOVERNOR,
	MAILBOX_EVT_CLK_DIV = IPC_DEBUG_CLK_DIV,
	MAILBOX_EVT_WAKEUP,
	MAILBOX_EVT_WAKEUP_CLR,
	MAILBOX_EVT_ENABLE_IRQ,
	MAILBOX_EVT_DISABLE_IRQ,
	MAILBOX_EVT_CHUB_ALIVE,
	MAILBOX_EVT_RT_LOGLEVEL,
	MAILBOX_EVT_START_AON,
	MAILBOX_EVT_STOP_AON,
	MAILBOX_EVT_MAX,
};

enum chub_status {
	CHUB_ST_NO_POWER,
	CHUB_ST_POWER_ON,
	CHUB_ST_RUN,
	CHUB_ST_NO_RESPONSE,
	CHUB_ST_ERR,
	CHUB_ST_HANG,
	CHUB_ST_RESET_FAIL,
	CHUB_ST_ITMON,
};

struct read_wait {
	atomic_t cnt;
	atomic_t flag;
	wait_queue_head_t event;
};

struct chub_evt {
	atomic_t flag;
	wait_queue_head_t event;
};

#ifdef USE_EXYNOS_LOG
#define CHUB_DBG_DIR "/data/exynos/log/chub"
#else
#define CHUB_DBG_DIR "/data"
#endif

#define CHUB_RESET_THOLD (3)
#define CHUB_RESET_THOLD_MINOR (5)

enum chub_err_type {
	CHUB_ERR_NONE,
	CHUB_ERR_ITMON, /* ITMON by CHUB */
	CHUB_ERR_S2MPU, /* S2MPU by CHUB */
	CHUB_ERR_FW_FAULT, /* CSP_FAULT by CHUB */
	CHUB_ERR_FW_WDT, /* watchdog by CHUB */
	CHUB_ERR_FW_REBOOT,
	CHUB_ERR_ISR, /* 5 */
	CHUB_ERR_CHUB_NO_RESPONSE,
	CHUB_ERR_CRITICAL, /* critical errors */
	CHUB_ERR_EVTQ_ADD, /* write event error */
	CHUB_ERR_EVTQ_EMTPY, /* isr empty event */
	CHUB_ERR_EVTQ_WAKEUP, /* 10 */
	CHUB_ERR_EVTQ_WAKEUP_CLR,
	CHUB_ERR_WRITE_FAIL,
	CHUB_ERR_MAJER,	/* majer errors */
	CHUB_ERR_READ_FAIL,
	CHUB_ERR_CHUB_ST_ERR,	/* 15: chub_status ST_ERR */
	CHUB_ERR_COMMS_WAKE_ERR,
	CHUB_ERR_FW_ERROR,
	CHUB_ERR_NEED_RESET, /* reset errors */
	CHUB_ERR_NANOHUB_LOG,
	CHUB_ERR_COMMS_NACK, /* 20: ap comms error */
	CHUB_ERR_COMMS_BUSY,
	CHUB_ERR_COMMS_UNKNOWN,
	CHUB_ERR_COMMS,
	CHUB_ERR_RESET_CNT,
	CHUB_ERR_KERNEL_PANIC,
	CHUB_ERR_MAX,
};

struct contexthub_symbol_addr {
	unsigned int base;
	unsigned int size;
	unsigned int offset;
	unsigned int length;
};

struct contexthub_symbol_table {
	unsigned int size;
	unsigned int count;
	unsigned int name_offset;
	unsigned int reserved;
	struct contexthub_symbol_addr symbol[0];
};

struct contexthub_notifi_info {
	char name[IPC_NAME_MAX];
	enum ipc_owner id;
	struct contexthub_notifier_block *nb;
};

struct chub_baaw {
	const char *name;
	void __iomem *addr;
	unsigned int size;
	unsigned int values[BAAW_MAX_WINDOWS * 5];
};

#define CHUB_IRQ_PIN_MAX (5)
struct contexthub_ipc_info {
	u32 cur_err;
	int err_cnt[CHUB_ERR_MAX];
	struct device *dev;
	struct nanohub_data *data;
	struct nanohub_platform_data *pdata;
	struct ipc_info *chub_ipc;
	wait_queue_head_t wakeup_wait;
	struct work_struct debug_work;
	spinlock_t logout_lock;
	struct read_wait read_lock;
	struct chub_evt chub_alive_lock;
	struct chub_evt poweron_lock;
	struct chub_evt reset_lock;
	struct chub_evt log_lock;
	resource_size_t sram_size;
	void __iomem *sram;
	phys_addr_t sram_phys;
	void __iomem *mailbox;
	void __iomem *chub_dumpgpr;
	void __iomem *chub_dump_cmu;
	void __iomem *chub_dump_sys;
	void __iomem *chub_dump_wdt;
	void __iomem *chub_dump_timer;
	void __iomem *chub_dump_pwm;
	void __iomem *chub_dump_rtc;
	void __iomem *usi_array[MAX_USI_CNT];
	void __iomem *chub_out;
	void __iomem *upmu;
	int usi_cnt;
	struct chub_baaw chub_baaws[BAAW_MAX];
	struct log_buffer_info *log_info;
	struct log_buffer_info *dd_log;
	struct LOG_BUFFER *dd_log_buffer;
	struct runtimelog_buf chub_rt_log;
	unsigned long clkrate;
	atomic_t irq_apInt;
	atomic_t wakeup_chub;
	atomic_t in_use_ipc;
	int irq_mailbox;
	int irq_wdt;
	bool irq_wdt_disabled;
	int utc_run;
	int powermode;
	atomic_t chub_shutdown;
	atomic_t chub_status;
	atomic_t chub_sleep;
	bool chub_sleep_dumped;
	u32 wakeup_by_chub_cnt;
	atomic_t in_reset;
	atomic_t in_log;
	bool os_load;
	u8 num_os;
	bool multi_os;
	bool alive_mct;
	u32 chub_dfs_gov;
	/* wakeup control */
	struct wakeup_source *ws;
	struct wakeup_source *ws_reset;
	/* chub f/w callstack */
	struct contexthub_symbol_table *symbol_table;
	/* communicate with others */
	/* chub notifiers */
	struct contexthub_notifier_block chub_cipc_nb;
	struct contexthub_notifi_info cipc_noti[IPC_OWN_MAX];
	struct notifier_block itmon_nb;
	struct notifier_block panic_nb;
	u32 irq_pin_len;
	u32 irq_pins[CHUB_IRQ_PIN_MAX];
	struct delayed_work sensor_alive_work;
	bool sensor_alive_work_run;
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	/* image loader */
	struct imgloader_desc chub_img_desc[3];
#endif
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
	struct s2mpu_notifier_block s2mpu_nb;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	/* sysevent */
	struct sysevent_desc sysevent_desc;
	struct sysevent_device *sysevent_dev;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	/* memlogger */
	struct memlogs mlog;
#endif
	void __iomem *pmu_chub_reset;
	void __iomem *pmu_chub_cpu;
	struct task_struct *log_kthread;
	wait_queue_head_t log_kthread_wait;
	bool itmon_dumped;
#if IS_ENABLED(CONFIG_EXYNOS_AONCAM)
	struct notifier_block aon_nb;
	wait_queue_head_t aon_wait;
	bool aon_flag;
#endif
};

#define IPC_HW_WRITE_DUMPGPR_CTRL(base, val) \
	__raw_writel((val), (base) + REG_CHUB_DUMPGPR_CTRL)
#define IPC_HW_READ_DUMPGPR_PCR(base) \
	__raw_readl((base) + REG_CHUB_DUMPGPR_PCR)

#define READ_CHUB_USI_CONF(base) \
	__raw_readl((base) + USI_REG_USI_CONFIG)

/*	CHUB BAAW Registers : CHUB BASE + 0x100000 */
#define REG_BAAW_D_CHUB0 (0x0)
#define REG_BAAW_D_CHUB1 (0x4)
#define REG_BAAW_D_CHUB2 (0x8)
#define REG_BAAW_D_CHUB3 (0xc)
#define BAAW_VAL_MAX (4)
#define BAAW_RW_ACCESS_ENABLE 0x80000003

#define IPC_MAX_TIMEOUT (0xffffff)
#define INIT_CHUB_VAL (-1)

#define IPC_HW_WRITE_BAAW_CHUB0(base, val) \
	__raw_writel((val), (base) + REG_BAAW_D_CHUB0)
#define IPC_HW_WRITE_BAAW_CHUB1(base, val) \
	__raw_writel((val), (base) + REG_BAAW_D_CHUB1)
#define IPC_HW_WRITE_BAAW_CHUB2(base, val) \
	__raw_writel((val), (base) + REG_BAAW_D_CHUB2)
#define IPC_HW_WRITE_BAAW_CHUB3(base, val) \
	__raw_writel((val), (base) + REG_BAAW_D_CHUB3)

static inline struct wakeup_source *chub_wake_lock_init(struct device *dev, const char *name)
{
	struct wakeup_source *ws = NULL;

	ws = wakeup_source_register(dev, name);
	if (ws == NULL) {
		nanohub_err("%s: wakelock register fail\n", name);
		return NULL;
	}

	return ws;
}

static inline void chub_wake_lock_destroy(struct wakeup_source *ws)
{
	if (ws == NULL) {
		nanohub_err("wakelock unregister fail\n");
		return;
	}

	wakeup_source_unregister(ws);
}

static inline void chub_wake_lock(struct wakeup_source *ws)
{
	if (ws == NULL) {
		nanohub_err("wakelock fail\n");
		return;
	}

	__pm_stay_awake(ws);
}

static inline void chub_wake_lock_timeout(struct wakeup_source *ws, long timeout)
{
	if (ws == NULL) {
		nanohub_err("wakelock timeout fail\n");
		return;
	}

	__pm_wakeup_event(ws, jiffies_to_msecs(timeout));
}

static inline void chub_wake_unlock(struct wakeup_source *ws)
{
	if (ws == NULL) {
		nanohub_err("wake unlock fail\n");
		return;
	}

	__pm_relax(ws);
}

static inline int chub_wake_lock_active(struct wakeup_source *ws)
{
	if (ws == NULL) {
		nanohub_err("wake unlock fail\n");
		return 0;
	}

	return ws->active;
}

static inline int contexthub_read_in_reset(struct contexthub_ipc_info *chub)
{
	return atomic_read(&chub->in_reset);
}

static inline void chub_wake_event(struct chub_evt *event)
{
	atomic_set(&event->flag, 1);
	wake_up_interruptible_sync(&event->event);
}

static inline int chub_wait_event(struct chub_evt *event, int timeout)
{
	atomic_set(&event->flag, 0);
	return wait_event_interruptible_timeout(event->event,
						atomic_read(&event->flag),
						msecs_to_jiffies(timeout));
}

static inline int contexthub_read_token(struct contexthub_ipc_info *chub)
{
	if (contexthub_read_in_reset(chub))
		return -EINVAL;

	return atomic_read(&chub->in_use_ipc);
}

static inline int contexthub_get_token(struct contexthub_ipc_info *chub)
{
	if (contexthub_read_in_reset(chub))
		return -EINVAL;

	atomic_inc(&chub->in_use_ipc);
	return 0;
}

static inline void contexthub_put_token(struct contexthub_ipc_info *chub)
{
	atomic_dec(&chub->in_use_ipc);
}

static inline void contexthub_status_reset(struct contexthub_ipc_info *chub)
{
	int i;

	/* clear ipc value */
	atomic_set(&chub->wakeup_chub, CHUB_OFF);
	atomic_set(&chub->irq_apInt, C2A_OFF);
	atomic_set(&chub->read_lock.cnt, 0);
	atomic_set(&chub->read_lock.flag, 0);

	/* chub err init */
	for (i = 0; i < CHUB_ERR_COMMS; i++)
		chub->err_cnt[i] = 0;

	ipc_write_hw_value(IPC_VAL_HW_BOOTMODE, chub->os_load);
	ipc_set_chub_clk((u32)chub->clkrate);
	chub->chub_rt_log.loglevel = CHUB_RT_LOG_DUMP_PRT;
	ipc_set_chub_bootmode(BOOTMODE_COLD, chub->chub_rt_log.loglevel);
}

static inline void clear_err_cnt(struct contexthub_ipc_info *chub,
				 enum chub_err_type err)
{
	if (chub->err_cnt[err])
		chub->err_cnt[err] = 0;
}

/* CHUB Driver */
void contexthub_check_time(void);
int contexthub_notifier_call(struct contexthub_ipc_info *chub, enum CHUB_STATE state);

/* IPC IFs */
int contexthub_ipc_drv_init(struct contexthub_ipc_info *chub);
int contexthub_get_sensortype(struct contexthub_ipc_info *chub, char *buf);
int contexthub_ipc_if_init(struct contexthub_ipc_info *chub);
int contexthub_chub_ipc_init(struct contexthub_ipc_info *chub);
int contexthub_ipc_write_event(struct contexthub_ipc_info *chub, enum mailbox_event event);
int contexthub_ipc_read(struct contexthub_ipc_info *chub, uint8_t *rx, int max_length, int timeout);
int contexthub_ipc_write(struct contexthub_ipc_info *chub, uint8_t *tx, int length, int timeout);

/* Bootup */
void contexthub_handle_debug(struct contexthub_ipc_info *chub, enum chub_err_type err);
int contexthub_verify_symtable(struct contexthub_ipc_info *chub, void *data, size_t size);
int contexthub_bootup_init(struct contexthub_ipc_info *chub);
uint32_t contexthub_get_fw_version(const char *name, struct device *dev);
int contexthub_poweron(struct contexthub_ipc_info *chub);
int contexthub_shutdown(struct contexthub_ipc_info *chub);
int contexthub_reset(struct contexthub_ipc_info *chub, bool force_load, enum chub_err_type err);

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
static int contexthub_s2mpu_notifier(struct s2mpu_notifier_block *nb,
				     struct s2mpu_notifier_info *nb_data)
{
	struct contexthub_ipc_info *data =
	    container_of(nb, struct contexthub_ipc_info, s2mpu_nb);
	struct s2mpu_notifier_info *s2mpu_data = nb_data;

	(void)data;
	(void)s2mpu_data;

	nanohub_info("%s called!\n", __func__);
	contexthub_handle_debug(data, CHUB_ERR_S2MPU);
	return S2MPU_NOTIFY_OK;
}

static inline void contexthub_register_s2mpu_notifier(struct contexthub_ipc_info *chub)
{
	chub->s2mpu_nb.notifier_call = contexthub_s2mpu_notifier;
	chub->s2mpu_nb.subsystem = "CHUB";
	exynos_s2mpu_notifier_call_register(&chub->s2mpu_nb);
}
#else
static int contexthub_s2mpu_notifier(struct s2mpu_notifier_block *nb,
				     struct s2mpu_notifier_info *nb_data)
{
	nanohub_dev_info(chub->dev, "%s not enabled\n", __func__);
	return 0;
}

static inline void contexthub_register_s2mpu_notifier(struct contexthub_ipc_info *chub)
{
	nanohub_dev_info(chub->dev, "%s not enabled\n", __func__);
}

static inline int exynos_request_fw_stage2_ap(const char *str)
{
	nanohub_dev_info(chub->dev, "%s not enabled\n", __func__);

	return 0;
}
#endif

#define MAX_FIRMWARE_NUM 3
#define SRAM_OFFSET_OS SZ_4K

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU) && IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
static void contexthub_imgloader_checker(struct imgloader_desc *desc, phys_addr_t fw_phys_base,
					 size_t fw_bin_size, size_t fw_mem_size)
{
	const struct firmware *entry;
	int ret = 0;

	nanohub_dev_warn(desc->dev, "=====compare SRAM and F/W binary=====\n");
	nanohub_dev_warn(desc->dev, "loaded fw name : %s\n", desc->fw_name);
	nanohub_dev_warn(desc->dev, "loaded fw size : 0x%X\n", fw_bin_size);

	ret = request_firmware(&entry, desc->fw_name, desc->dev);

	if (ret) {
		nanohub_dev_err(desc->dev, "request_firmware failed for checking after s2mpu verification failed\n");
		release_firmware(entry);
	} else {
		int diff_check = 0, i;
		void *sram_phy = ioremap(fw_phys_base, fw_mem_size);
		u8 *sram = kmalloc(fw_bin_size, GFP_KERNEL);

		memcpy_fromio(sram, sram_phy, fw_bin_size);

		for (i = 0; i < fw_bin_size; i++) {
			if (sram[i] != entry->data[i]) {
				nanohub_dev_warn(desc->dev, "diff offset : 0x%x\n", i);
				nanohub_dev_warn(desc->dev, "sram val : 0x%x, bin val : 0x%x\n",
						 sram[i], entry->data[i]);
				diff_check++;
			}
		}

		if (diff_check == 0)
			nanohub_dev_warn(desc->dev, "no diff between chub sram and binary\n");
		else
			nanohub_dev_warn(desc->dev, "has %d diff between chub sram and binary\n", diff_check);

		iounmap(sram);
		release_firmware(entry);
	}
	nanohub_dev_warn(desc->dev, "===compare SRAM and F/W binary end===\n");
}

static inline int contexthub_imgloader_verify_fw(struct imgloader_desc *desc,
						 phys_addr_t fw_phys_base,
						 size_t fw_bin_size,
						 size_t fw_mem_size)
{
	u64 ret64 = exynos_verify_subsystem_fw(desc->name, desc->fw_id,
					       fw_phys_base, fw_bin_size,
					       ALIGN(fw_mem_size, SZ_4K));
	if (ret64) {
		nanohub_dev_warn(desc->dev,
				 "Failed F/W verification, ret=%llu\n", ret64);

		contexthub_imgloader_checker(desc, fw_phys_base, fw_bin_size, fw_mem_size);

		return -EIO;
	}
	ret64 = exynos_request_fw_stage2_ap(desc->name);
	if (ret64) {
		nanohub_dev_warn(desc->dev,
				 "Failed F/W verification to S2MPU, ret=%llu\n",
				 ret64);
		return -EIO;
	}
	return 0;
}
#else
static inline int contexthub_imgloader_verify_fw(struct imgloader_desc *desc,
						 phys_addr_t fw_phys_base,
						 size_t fw_bin_size,
						 size_t fw_mem_size)
{
	nanohub_dev_info(chub->dev, "%s not enabled\n", __func__);
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
static inline int contexthub_imgloader_mem_setup(struct imgloader_desc *desc, const u8 *metadata,
						 size_t size, phys_addr_t *fw_phys_base,
						 size_t *fw_bin_size, size_t *fw_mem_size)
{
	void *addr = NULL;
	struct contexthub_ipc_info *chub = (struct contexthub_ipc_info *)desc->data;

	*fw_phys_base = chub->sram_phys;
	*fw_bin_size = size;
	*fw_mem_size = chub->sram_size;
	addr = chub->sram;
	memcpy_toio(addr, metadata, size);
	nanohub_info("%s chub image %s loaded by imgloader\n", __func__,
		     desc->fw_name);
	contexthub_verify_symtable(chub, (void *)metadata, size);

	return 0;
}

static inline int contexthub_download_image(struct contexthub_ipc_info *chub, int idx)
{
#if IS_ENABLED(CONFIG_SHUB)
	enum {
		FW_DEFAULT = 0,
		FW_SPU = 1,
		FW_MAX = 2,
	};

	u32 fw_version[FW_MAX] = {0, };
	int i = 0;

	for (i = 0; i < FW_MAX; i++) {
		fw_version[i] = contexthub_get_fw_version(chub->chub_img_desc[i].fw_name, chub->dev);
		nanohub_info("%s fw name: %s(%d)\n", __func__, chub->chub_img_desc[i].fw_name, fw_version[i]);
	}

	idx = fw_version[FW_DEFAULT] < fw_version[FW_SPU] ? FW_SPU : FW_DEFAULT;
#endif
	return imgloader_boot(&chub->chub_img_desc[idx]);
}

static inline int contexthub_imgloader_desc_init(struct contexthub_ipc_info *chub,
						 struct imgloader_desc *desc,
						 const char *name, int id)
{
	static struct imgloader_ops contexthub_imgloader_ops = {
		.mem_setup = contexthub_imgloader_mem_setup,
		.verify_fw = contexthub_imgloader_verify_fw
	};

	desc->dev = chub->dev;
	desc->owner = THIS_MODULE;
	desc->ops = &contexthub_imgloader_ops;
	desc->data = (void *)chub;
	desc->name = "CHUB";
	desc->s2mpu_support = true;
	desc->fw_name = name;
	desc->fw_id = id;

	return imgloader_desc_init(desc);
}

static inline int contexthub_imgloader_init(struct contexthub_ipc_info *chub)
{
	int ret = 0;
	int i = 0, j;

	if (!chub->multi_os) {
#if IS_ENABLED(CONFIG_SHUB)
		ret = contexthub_imgloader_desc_init(chub, &chub->chub_img_desc[i], OS_IMAGE_DEFAULT, i);
		ret = contexthub_imgloader_desc_init(chub, &chub->chub_img_desc[i+1], "/sensorhub/shub_spu.bin", i + 1);
#else
		ret |= contexthub_imgloader_desc_init(chub,
				&chub->chub_img_desc[i], OS_IMAGE_DEFAULT, i);
		i++;
#endif
	} else {
		do {
			ret |= contexthub_imgloader_desc_init(chub,
					&chub->chub_img_desc[i], os_image[i], i);
			i++;
		} while (i < MAX_FIRMWARE_NUM);
	}

	for (j = 0 ; j < i ; j++)
		nanohub_dev_debug(chub->dev, "%s fw name: %s\n", __func__,
				  chub->chub_img_desc[j].fw_name);

	return ret;
}
#else
static inline int contexthub_imgloader_mem_setup(struct imgloader_desc *desc, const u8 *metadata,
						 size_t size, phys_addr_t *fw_phys_base,
						 size_t *fw_bin_size, size_t *fw_mem_size)
{
	nanohub_dev_info(chub->dev, "%s not enabled\n", __func__);
	return 0;
}

static inline int contexthub_download_image(struct contexthub_ipc_info *chub, int idx)
{
	const struct firmware *entry;
	int ret = 0;
	void *chub_addr = NULL;

	if (!chub->multi_os) {
		nanohub_dev_info(chub->dev, "%s: download %s\n", __func__, OS_IMAGE_DEFAULT);
		ret = request_firmware(&entry, OS_IMAGE_DEFAULT, chub->dev);
		chub_addr = chub->sram;
	} else {
		nanohub_dev_info(chub->dev, "%s: download %s\n", __func__, os_image[idx]);
		ret = request_firmware(&entry, os_image[idx], chub->dev);
		chub_addr = ipc_get_base(IPC_REG_OS);
	}

	if (ret) {
		nanohub_dev_err(chub->dev,
				"%s: request_firmware failed (%d)\n", __func__, idx);
		release_firmware(entry);
		return ret;
	}
	memcpy_toio(chub_addr, entry->data, entry->size);
	nanohub_dev_info(chub->dev, "%s: idx:%d, bin(size:%d) on %lx\n",
			 __func__, idx, (int)entry->size, (unsigned long)ipc_get_base(reg));
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
	if (idx > 0)
		ret = exynos_verify_subsystem_fw("CHUB", 1,
						 chub->sram_phys + SZ_4K,
						 entry->size, chub->sram_size - SZ_4K);
	else
		ret = exynos_verify_subsystem_fw("CHUB", 0,
						 chub->sram_phys, entry->size, SZ_4K);

	if (ret) {
		nanohub_dev_err(chub->dev, "%s: verify fail!:%d\n", __func__,
				ret);
		release_firmware(entry);
		return -1;
	}
#endif
	if (idx)
		contexthub_verify_symtable(chub, (void *)entry->data, entry->size);

	release_firmware(entry);

	return 0;
}

static inline int contexthub_imgloader_init(struct contexthub_ipc_info *chub)
{
	nanohub_dev_info(chub->dev, "%s not enabled\n", __func__);
	return 0;
}
#endif
#endif
