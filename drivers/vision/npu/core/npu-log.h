/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_LOG_H_
#define _NPU_LOG_H_

#define DEBUG

#include <linux/version.h>
#include <linux/types.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/printk.h>
#include <linux/smp.h>
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/memlogger.h>
#else
#include "npu-device.h"
#endif
#include "npu-common.h"
#include "npu-ver-info.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#define NPU_RING_LOG_BUFFER_MAGIC	0x3920FFAC
#define LOG_UNIT_NUM     (1024)

struct npu_device;

/* Log level definition */
typedef enum {
	NPU_LOG_TRACE	= 0,
	NPU_LOG_DBG,	/* Default. Set in npu_log.c */
	NPU_LOG_INFO,
	NPU_LOG_WARN,
	NPU_LOG_ERR,
	NPU_LOG_NONE,
	NPU_LOG_INVALID,
} npu_log_level_e;

/* Default tag definition */
#ifndef NPU_LOG_TAG
#define NPU_LOG_TAG	"*"
#endif

#define NPU_EXYNOS_PRINTK

/*
 * Logging kernel address
 */
#ifdef EXYNOS_NPU_EXPOSE_KERNEL_ADDR
#define NPU_KPTR_LOG    "%p"
#else
#define NPU_KPTR_LOG    "%pK"
#endif

#define NPU_ERR_IN_DMESG_DISABLE	0
#define NPU_ERR_IN_DMESG_ENABLE		1

struct npu_log_ops {
	void (*fw_rprt_manager)(void);
	void (*fw_rprt_gather)(void);
	int (*npu_check_unposted_mbox)(int nCtrl);
};

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
struct npu_log_dvfs {
	u64	timestamp;
	u32	id;		/* pm_qos id */
	u32	from;
	u32	to;
};

struct npu_log_scheduler {
	u64	timestamp;
	u32	temp;	/* 8b : NPU, 8b : CL0, 8b : CL1, 8b : CL2 */
	u32	load;	/* 16b : idle load, 16b : fps load */
	u32	status;	/* 8b : scheduler mode, 8b : BTS scenario, 16b : LLC */
};

struct npu_log_protodrv {
	u64		timestamp;
	u32		uid_cmd;	/* 16b : uid, 16b : cmd */
	u32		param0;		/* frame_id for frame cmd */
	u32		param1;		/* resule code for response */
};
#ifdef CONFIG_NPU_USE_HW_DEVICE
struct npu_log_hwdev {
	u64	timestamp;
	u32	hid;	/* enum npu_hw_device_id */
	u32	status;	/* 16b : power on/off, 16b : clock on/off */
	u32	cmd;	/* IOCTL cmd */
};
#endif

struct npu_log_ioctl {
	u64	timestamp;
	u32	cmd;	/* IOCTL cmd */
	u32	dir;	/* IOCTL dir */
};

struct npu_log_ipc {
	u64	timestamp;
	u32	h2fctrl;
	u32	rptr;
	u32	wptr;
};

union npu_log_tag {
	struct npu_log_dvfs d;
	struct npu_log_scheduler s;
	struct npu_log_protodrv p;
#ifdef CONFIG_NPU_USE_HW_DEVICE
	struct npu_log_hwdev h;
#endif
	struct npu_log_ioctl i;
	struct npu_log_ipc c;
};

#endif

/* Structure for log management object */
struct npu_log {
#ifdef NPU_EXYNOS_PRINTK
	struct device *dev;
#endif
	/* kernel logger level */
	npu_log_level_e		pr_level;
	/* memory storage logger level */
	npu_log_level_e		st_level;

	/* kpi log enable/disable */
	u32	kpi_level;

	/* Buffer for log storage */
	char			*st_buf;
	size_t			wr_pos;
	size_t			rp_pos;
	size_t			st_size;
	size_t			line_cnt;
	char			npu_err_in_dmesg;

	/* Save last dump postion to gracefully handle consecutive error case */
	size_t			last_dump_line_cnt;

	/* Position where the last dump log has read */
	size_t			last_dump_mark_pos;

	/* Reference counter for debugfs interface */
	atomic_t		fs_ref;

	/* Wait queue to notify readers */
	wait_queue_head_t	wq;

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	/* memlog for Unified Logging System*/
	struct memlog *memlog_desc_log;
	struct memlog *memlog_desc_array;
	struct memlog *memlog_desc_dump;
	struct memlog_obj *npu_memlog_obj;
	struct memlog_obj *npu_dumplog_obj;
	struct memlog_obj *npu_memfile_obj;
	struct memlog_obj *npu_dvfs_array_obj;
	struct memlog_obj *npu_scheduler_array_obj;
	struct memlog_obj *npu_protodrv_array_obj;
	struct memlog_obj *npu_hwdev_array_obj;
	struct memlog_obj *npu_ioctl_array_obj;
	struct memlog_obj *npu_ipc_array_obj;
	struct memlog_obj *npu_array_file_obj;
	struct npu_log_scheduler s;
#endif
	const struct npu_log_ops	*log_ops;
};

#if !IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
extern struct npu_log	npu_log;
#endif

/*
 * Define interval mask to printout sync mark on kernel log
 * to sync with memory stored log.
 * sync mark is printed if (npu_log.line_cnt & MASK == 0)
 */
#define NPU_STORE_LOG_SYNC_MARK_INTERVAL_MASK	0x7F	/* Sync mark on every 128 messages */
#define NPU_STORE_LOG_SYNC_MARK_MSG		"NPU log sync [%zu]\n"

/* Amount of log dumped on an error occurred */
#define NPU_STORE_LOG_DUMP_SIZE_ON_ERROR	(16 * 1024)

/* Log flush wait interval and count when the store log is de-initialized */
#define NPU_STORE_LOG_FLUSH_INTERVAL_MS		500

/* Chunk size used in bitmap_scnprintf */
#define CHUNKSZ                 32

/* Log size used in memdump with memcpy */
#define MEMLOGSIZE                             16


/* Structure to represent ring buffer log storage */
struct npu_ring_log_buffer {
	u32			magic;
	const char *name;
	struct mutex		lock;
	struct completion	read_wq;
	DECLARE_KFIFO_PTR(fifo, char);
	size_t			size;
	size_t			wPtr;
};

/* Per file descriptor object for ring log buffer */
struct npu_ring_log_buffer_fd_obj {
	struct npu_ring_log_buffer *ring_buf;
	size_t				last_pos;
};

/* Prototypes */
int npu_store_log(npu_log_level_e loglevel, const char *fmt, ...);	/* Shall not be used directly */
void npu_store_log_init(char *buf_addr, size_t size);
void npu_store_log_deinit(void);
void npu_log_on_error(void);

void npu_fw_report_init(char *buf_addr, const size_t size);
int npu_fw_report_store(char *strRep, int nSize);
int npu_fw_profile_store(char *strRep, int nSize);
void dbg_print_fw_report_st(struct npu_log stLog);
void npu_fw_report_deinit(void);

void npu_fw_profile_init(char *buf_addr, const size_t size);
void npu_fw_profile_deinit(void);

void npu_memlog_store(npu_log_level_e loglevel, const char *fmt, ...);
void npu_dumplog_store(npu_log_level_e loglevel, const char *fmt, ...);
bool npu_log_is_kpi_silent(void);


/* Utilities */
s32 atoi(const char *psz_buf);
int bitmap_scnprintf(char *buf, unsigned int buflen, const unsigned long *maskp, int nmaskbits);


/* Memory dump */
int npu_debug_memdump8(u8 *start, u8 *end);
int npu_debug_memdump16(u16 *start, u16 *end);
int npu_debug_memdump32(u32 *start, u32 *end);
int npu_debug_memdump32_by_memcpy(u32 *start, u32 *end);

/* for debug and performance */
inline void npu_log_dvfs_set_data(int hid, int from, int to);
inline void npu_log_protodrv_set_data(void *req, int type, bool act);
inline void npu_log_scheduler_set_data(struct npu_device *device);
#ifdef CONFIG_NPU_USE_HW_DEVICE
inline void npu_log_hwdev_set_data(int id);
#endif
inline void npu_log_ioctl_set_date(int cmd, int dir);
inline void npu_log_ipc_set_date(int h2fctrl, int wptr, int rptr);

#define ISPRINTABLE(strValue)	((isascii(strValue) && isprint(strValue)) ? \
	((strValue == '%') ? '.' : strValue) : '.')

#ifdef NPU_EXYNOS_PRINTK
#define probe_info(fmt, ...)            pr_info("NPU:" fmt, ##__VA_ARGS__)
#define probe_warn(fmt, args...)        pr_warn("NPU:[WRN]" fmt, ##args)
#define probe_err(fmt, args...)         pr_err("NPU:[ERR]%s:%d:" fmt, __func__, __LINE__, ##args)
#define probe_trace(fmt, ...)           pr_info(fmt, ##__VA_ARGS__)

#ifdef DEBUG_LOG_MEMORY
#define npu_log_on_lv_target(LV, DEV_FUNC, fmt, ...)	DEV_FUNC(npu_log.dev, fmt, ##__VA_ARGS__)
#else
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define npu_log_on_lv_target(LV, fmt, ...)	\
				((console_printk[0] > LV) ? npu_memlog_store(LV, "[%d: %15s:%5d]" fmt, __smp_processor_id(), current->comm, \
					current->pid,##__VA_ARGS__) : 0)
#define npu_dump_on_lv_target(LV, fmt, ...)	\
				npu_dumplog_store(LV, "[%d: %15s:%5d]" fmt, smp_processor_id(), current->comm, current->pid, ##__VA_ARGS__)
#else
#define npu_log_on_lv_target(LV, DEV_FUNC, fmt, ...)		\
	do {	\
		int __npu_ret = 0;	\
		if (npu_log.st_level <= (LV))	\
			__npu_ret = npu_store_log((LV), fmt, ##__VA_ARGS__);	\
		if (__npu_ret)		\
			DEV_FUNC(npu_log.dev, fmt, ##__VA_ARGS__);	\
		else {			\
			if (npu_log.pr_level <= (LV))		\
				DEV_FUNC(npu_log.dev, fmt, ##__VA_ARGS__);	\
		}	\
	} while (0)
#endif  // IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#endif  // DEBUG_LOG_MEMORY

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define npu_err_target(fmt, ...)	npu_log_on_lv_target(MEMLOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#define npu_warn_target(fmt, ...)	npu_log_on_lv_target(MEMLOG_LEVEL_CAUTION, fmt, ##__VA_ARGS__)
#define npu_info_target(fmt, ...)	npu_log_on_lv_target(MEMLOG_LEVEL_CAUTION, fmt, ##__VA_ARGS__)
#define npu_notice_target(fmt, ...)	npu_log_on_lv_target(MEMLOG_LEVEL_CAUTION, fmt, ##__VA_ARGS__)
#define npu_dbg_target(fmt, ...)	npu_log_on_lv_target(MEMLOG_LEVEL_INFO, fmt, ##__VA_ARGS__)  // debug
#define npu_trace_target(fmt, ...)	npu_log_on_lv_target(MEMLOG_LEVEL_NOTICE, fmt, ##__VA_ARGS__)  // performance
#define npu_dump_target(fmt, ...)	npu_dump_on_lv_target(MEMLOG_LEVEL_ERR, fmt, ##__VA_ARGS__)  // dump
#else
#define npu_err_target(fmt, ...)	\
	do {	\
		npu_log_on_lv_target(NPU_LOG_ERR, dev_err, fmt, ##__VA_ARGS__);	\
	} while (0)
#define npu_warn_target(fmt, ...)	npu_log_on_lv_target(NPU_LOG_WARN, dev_warn, fmt, ##__VA_ARGS__)
#define npu_info_target(fmt, ...)	npu_log_on_lv_target(NPU_LOG_INFO, dev_notice, fmt, ##__VA_ARGS__)
#define npu_notice_target(fmt, ...)	npu_log_on_lv_target(NPU_LOG_DBG, dev_dbg, fmt, ##__VA_ARGS__)
#define npu_dbg_target(fmt, ...)	npu_log_on_lv_target(NPU_LOG_DBG, dev_info, fmt, ##__VA_ARGS__)
#define npu_trace_target(fmt, ...)	npu_log_on_lv_target(NPU_LOG_TRACE, dev_dbg, fmt, ##__VA_ARGS__)
#endif  // IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)

#else // !NPU_EXYNOS_PRINTK
#define probe_info(fmt, ...)            pr_info("NPU:" fmt, ##__VA_ARGS__)
#define probe_warn(fmt, args...)        pr_warn("NPU:[WRN]" fmt, ##args)
#define probe_err(fmt, args...)         pr_err("NPU:[ERR]%s:%d:" fmt, __func__, __LINE__, ##args)
#define probe_trace(fmt, ...)           ((npu_log.pr_level <= NPU_LOG_TRACE)?pr_info(fmt, ##__VA_ARGS__):0)

#ifdef DEBUG_LOG_MEMORY
#define npu_log_on_lv_target(LV, PR_FUNC, fmt, ...)	PR_FUNC(fmt, ##__VA_ARGS__)
#else
#define npu_log_on_lv_target(LV, PR_FUNC, fmt, ...)		\
	do {	\
		int __npu_ret = 0;	\
		if (npu_log.st_level <= (LV))	\
			__npu_ret = npu_store_log((LV), fmt, ##__VA_ARGS__);	\
		if (__npu_ret)		\
			PR_FUNC(fmt, ##__VA_ARGS__);	\
		else {			\
			if (npu_log.pr_level <= (LV))		\
				PR_FUNC(fmt, ##__VA_ARGS__);	\
		}	\
	} while (0)
#endif

#define npu_err_target(fmt, ...)	\
	do {	\
		npu_log_on_lv_target(NPU_LOG_ERR, pr_err, fmt, ##__VA_ARGS__);	\
		npu_log_on_error();	\
	} while (0)
#define npu_warn_target(fmt, ...)	npu_log_on_lv_target(NPU_LOG_WARN, pr_warning, fmt, ##__VA_ARGS__)
#define npu_info_target(fmt, ...)	npu_log_on_lv_target(NPU_LOG_INFO, pr_notice, fmt, ##__VA_ARGS__)
#define npu_notice_target(fmt, ...)	npu_log_on_lv_target(NPU_LOG_DBG, pr_debug, fmt, ##__VA_ARGS__)
#define npu_dbg_target(fmt, ...)	npu_log_on_lv_target(NPU_LOG_DBG, pr_info, fmt, ##__VA_ARGS__)
#define npu_trace_target(fmt, ...)	npu_log_on_lv_target(NPU_LOG_TRACE, pr_debug, fmt, ##__VA_ARGS__)
#endif

#define npu_err(fmt, args...) \
	npu_err_target("NPU:[" NPU_LOG_TAG "][ERR]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_warn(fmt, args...) \
	npu_warn_target("NPU:[" NPU_LOG_TAG "][WRN]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_info(fmt, args...) \
	npu_info_target("NPU:[" NPU_LOG_TAG "]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_notice(fmt, args...)	\
	npu_notice_target("NPU:[" NPU_LOG_TAG "][NOTICE]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_dbg(fmt, args...) \
	npu_dbg_target("NPU:[" NPU_LOG_TAG "]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_trace(fmt, args...) \
	npu_trace_target("NPU:[T][" NPU_LOG_TAG "]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_dump(fmt, args...) \
	npu_dump_target("NPU:[T][" NPU_LOG_TAG "]%s(%d):" fmt, __func__, __LINE__, ##args)

/* Printout context ID */
#define npu_ierr(fmt, vctx, args...) \
	npu_err_target("NPU:[" NPU_LOG_TAG "][I%d][ERR]%s(%d):" fmt, (vctx)->id, __func__, __LINE__, ##args)

#define npu_iwarn(fmt, vctx, args...) \
	npu_warn_target("NPU:[" NPU_LOG_TAG "][I%d][WRN]%s(%d):" fmt, (vctx)->id, __func__, __LINE__, ##args)

#define npu_iinfo(fmt, vctx, args...) \
	npu_info_target("NPU:[" NPU_LOG_TAG "][I%d]%s(%d):" fmt, (vctx)->id, __func__, __LINE__, ##args)

#define npu_inotice(fmt, vctx, args...)	\
	npu_notice_target("NPU:[" NPU_LOG_TAG "][I%d][NOTICE]%s(%d):" fmt, (vctx)->id, __func__, __LINE__, ##args)

#define npu_idbg(fmt, vctx, args...) \
	npu_dbg_target("NPU:[" NPU_LOG_TAG "][I%d]%s(%d):" fmt, (vctx)->id, __func__, __LINE__, ##args)

#define npu_itrace(fmt, vctx, args...) \
	npu_trace_target("NPU:[T][" NPU_LOG_TAG "][I%d]%s(%d):" fmt, (vctx)->id, __func__, __LINE__, ##args)

/* Printout unique ID */
#define npu_uerr(fmt, uid_obj, args...) \
	npu_err_target("NPU:[" NPU_LOG_TAG "][U%d][ERR]%s(%d):" fmt, (uid_obj)->uid, __func__, __LINE__, ##args)

#define npu_uwarn(fmt, uid_obj, args...) \
	npu_warn_target("NPU:[" NPU_LOG_TAG "][U%d][WRN]%s(%d):" fmt, (uid_obj)->uid, __func__, __LINE__, ##args)

#define npu_uinfo(fmt, uid_obj, args...) \
	npu_info_target("NPU:[" NPU_LOG_TAG "][U%d]%s(%d):" fmt, (uid_obj)->uid, __func__, __LINE__, ##args)

#define npu_unotice(fmt, uid_obj, args...)	\
	npu_notice_target("NPU:[" NPU_LOG_TAG "][U%d][NOTICE]%s(%d):" fmt, (uid_obj)->uid, __func__, __LINE__, ##args)

#define npu_udbg(fmt, uid_obj, args...) \
	npu_dbg_target("NPU:[" NPU_LOG_TAG "][U%d]%s(%d):" fmt, (uid_obj)->uid, __func__, __LINE__, ##args)

#define npu_utrace(fmt, uid_obj, args...) \
	npu_trace_target("NPU:[T][" NPU_LOG_TAG "][U%d]%s(%d):" fmt, (uid_obj)->uid, __func__, __LINE__, ##args)

/* Printout unique ID and frame ID */
#define npu_uferr(fmt, obj_ptr, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_err_target("NPU:[" NPU_LOG_TAG "][U%u][F%u][ERR]%s(%d):" fmt, (__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

#define npu_ufwarn(fmt, obj_ptr, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_warn_target("NPU:[" NPU_LOG_TAG "][U%u][F%u][WRN]%s(%d):" fmt, (__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

#define npu_ufinfo(fmt, obj_ptr, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_info_target("NPU:[" NPU_LOG_TAG "][U%u][F%u]%s(%d):" fmt, (__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

#define npu_ufnotice(fmt, obj_ptr, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_notice_target("NPU:[" NPU_LOG_TAG "][U%u][F%u][NOTICE]%s(%d):" fmt, \
							(__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

#define npu_ufdbg(fmt, obj_ptr, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_dbg_target("NPU:[" NPU_LOG_TAG "][U%u][F%u]%s(%d):" fmt, (__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

#define npu_uftrace(fmt, obj_ptr, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_trace_target("NPU:[T][" NPU_LOG_TAG "][U%u][F%u]%s(%d):" fmt, (__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

/* Exported functions */
int npu_log_probe(struct npu_device *npu_device);
int npu_log_release(struct npu_device *npu_device);
int npu_log_open(struct npu_device *npu_device);
int npu_log_close(struct npu_device *npu_device);
int npu_log_start(struct npu_device *npu_device);
int npu_log_stop(struct npu_device *npu_device);
int fw_will_note(size_t len);
int fw_will_note_to_kernel(size_t len);

extern const struct npu_log_ops npu_log_ops;

#endif /* _NPU_LOG_H_ */
