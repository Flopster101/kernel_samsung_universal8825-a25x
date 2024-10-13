/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEBUG_H
#define IS_DEBUG_H

#include <soc/samsung/memlogger.h>

#include <linux/module.h>
#include "is-video.h"

#define DEBUG_SENTENCE_MAX		300
#define LOG_INTERVAL_OF_WARN		30

#define IS_MEMLOG_FILE_DRV_NAME		"ker-fil"
#define IS_MEMLOG_FILE_DDK_NAME		"bin-fil"
#define IS_MEMLOG_PRINTF_DRV_SIZE	(SZ_2M)
#define IS_MEMLOG_PRINTF_DDK_SIZE	(SZ_1M)
#define IS_MEMLOG_DUMP_MAX		25

/* use sysfs for actuator */
#define INIT_MAX_SETTING		5

enum is_debug_state {
	IS_DEBUG_OPEN
};

enum dbg_dma_dump_type {
	DBG_DMA_DUMP_IMAGE,
	DBG_DMA_DUMP_META,
};

enum is_debug_param {
	IS_DEBUG_PARAM_CLK,
	IS_DEBUG_PARAM_STREAM,
	IS_DEBUG_PARAM_VIDEO,
	IS_DEBUG_PARAM_HW,
	IS_DEBUG_PARAM_DEVICE,
	IS_DEBUG_PARAM_IRQ,
	IS_DEBUG_PARAM_CSI,
	IS_DEBUG_PARAM_TIME_LAUNCH,
	IS_DEBUG_PARAM_TIME_QUEUE,
	IS_DEBUG_PARAM_TIME_SHOT,
	IS_DEBUG_PARAM_PDP,
	IS_DEBUG_PARAM_PAFSTAT,
	IS_DEBUG_PARAM_S2D,
	IS_DEBUG_PARAM_DVFS,
	IS_DEBUG_PARAM_MEM,
	IS_DEBUG_PARAM_LVN,
	IS_DEBUG_PARAM_LLC,
	IS_DEBUG_PARAM_YUVP,
	IS_DEBUG_PARAM_IQ,
	IS_DEBUG_PARAM_PHY_TUNE,
	IS_DEBUG_PARAM_CRC_SEED,
	IS_DEBUG_PARAM_MAX,
};

/**
 * struct is_debug_dma_info - dma_buf memory layout information.
 *
 * @pixeltype: pixel_size
 * @bpp: bits per pixel
 * @pixelformat: V4L2_PIX_FMT_XXX
 * @addr: kva from dma_buf
 */
struct is_debug_dma_info {
	u32 width;
	u32 height;
	u32 pixeltype;
	u32 bpp;
	u32 pixelformat;
	ulong addr;
};

struct is_mobj_dump_info {
	struct memlog_obj	*obj;
	char			name[PATH_MAX];
	phys_addr_t		paddr;
	size_t			size;
};

struct is_debug {
	struct dentry		*root;
	struct dentry		*imgfile;
	struct dentry		*event_dir;
	struct dentry		*logfile;

	/* log dump */
	size_t			read_vptr;
	struct is_minfo	*minfo;

	/* debug message */
	size_t			dsentence_pos;
	char			dsentence[DEBUG_SENTENCE_MAX];

	struct memlog			*memlog_desc;
	struct memlog_obj		*mobj_printf_drv;
	struct memlog_obj		*mobj_printf_ddk;
	struct memlog_obj		*mobj_file_drv;
	struct memlog_obj		*mobj_file_ddk;
	struct is_mobj_dump_info	mobj_dump[IS_MEMLOG_DUMP_MAX];
	atomic_t			mobj_dump_nums;

	unsigned long		state;

#ifdef ENABLE_CONTINUOUS_DDK_LOG
	/* blocking mode */
	bool                    read_debug_fs_logfile;
	wait_queue_head_t       debug_fs_logfile_queue;
#endif
	/* cdump */
	spinlock_t			slock_cdump;
	ulong				cdump_ptr;
};

struct is_sysfs_debug {
	unsigned int en_dvfs;
	unsigned int pattern_en;
	unsigned int pattern_fps;
	unsigned long hal_debug_mode;
	unsigned int hal_debug_delay;
};

struct is_sysfs_sensor {
	bool		is_en;
	bool		is_fps_en;
	unsigned int	frame_duration;
	unsigned int	long_exposure_time;
	unsigned int	short_exposure_time;
	unsigned int	long_analog_gain;
	unsigned int	short_analog_gain;
	unsigned int	long_digital_gain;
	unsigned int	short_digital_gain;
	unsigned int	set_fps;
	int		max_fps;
};

struct is_sysfs_actuator {
	unsigned int init_step;
	int init_positions[INIT_MAX_SETTING];
	int init_delays[INIT_MAX_SETTING];
	bool enable_fixed;
	int fixed_position;
};

struct is_sysfs_eeprom {
	bool eeprom_reload;
	bool eeprom_dump;
};

int is_debug_probe(struct device *dev);
int is_debug_open(struct is_minfo *minfo);
struct is_debug *is_debug_get(void);
int is_debug_close(void);

int is_get_debug_param(int param_idx);
int is_set_debug_param(int param_idx, int val);
int is_get_debug_sensor(void);
int is_get_debug_assert_crash(void);
struct is_sysfs_debug *is_get_sysfs_debug(void);
struct is_sysfs_sensor *is_get_sysfs_sensor(void);
struct is_sysfs_actuator *is_get_sysfs_actuator(void);
struct is_sysfs_eeprom *is_get_sysfs_eeprom(void);

unsigned int is_get_digit_ctrl(void);

void is_dmsg_init(void);
void is_dmsg_concate(const char *fmt, ...);
char *is_dmsg_print(void);
void is_print_buffer(char *buffer, size_t len);
int is_dbg_dma_dump(struct is_queue *queue, u32 instance, u32 index, u32 vid, u32 type);
int is_dbg_dma_dump_by_frame(struct is_frame *queue, u32 vid, u32 type);
void is_dbg_draw_digit(struct is_debug_dma_info *dinfo, u64 digit);

int imgdump_request(ulong cookie, ulong kvaddr, size_t size);

#define IS_EVENT_MAX_NUM	SZ_4K
#define EVENT_STR_MAX		SZ_128

typedef enum is_event_store_type {
	/* normal log - full then replace first normal log buffer */
	IS_EVENT_NORMAL = 0x1,
	/* critical log - full then stop to add log to critical log buffer*/
	IS_EVENT_CRITICAL = 0x2,
	IS_EVENT_OVERFLOW_CSI = 0x3,
	IS_EVENT_OVERFLOW_3AA = 0x4,
	IS_EVENT_ALL,
} is_event_store_type_t;

struct is_debug_event_log {
	unsigned int log_num;
	ktime_t time;
	is_event_store_type_t event_store_type;
	char dbg_msg[EVENT_STR_MAX];
	void (*callfunc)(void *);

	/* This parameter should be used in non-atomic context */
	void *ptrdata;
	int cpu;
};

struct is_debug_event {
	struct dentry			*log;
	u32				log_filter;
	u32				log_enable;

#ifdef ENABLE_DBG_EVENT_PRINT
	atomic_t			event_index;
	atomic_t			critical_log_tail;
	atomic_t			normal_log_tail;
	struct is_debug_event_log	event_log_critical[IS_EVENT_MAX_NUM];
	struct is_debug_event_log	event_log_normal[IS_EVENT_MAX_NUM];
#endif

	atomic_t			overflow_csi;
	atomic_t			overflow_3aa;
};

#ifdef ENABLE_DBG_EVENT_PRINT
void is_debug_event_print(u32 event_type, void (*callfunc)(void *), void *ptrdata, size_t datasize, const char *fmt, ...);
#else
#define is_debug_event_print(...)	do { } while(0)
#endif
void is_debug_event_count(u32 event_type);
void is_dbg_print(char *fmt, ...);
int is_debug_info_dump(struct seq_file *s, struct is_debug_event *debug_event);

void is_debug_s2d(bool en_s2d, const char *fmt, ...);
void is_debug_bcm(bool en, unsigned int bcm_owner);

/* Memlogger API & Macro */
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define is_memlog_info(fmt, args...)	\
	do {										\
		struct is_debug *idp = is_debug_get();					\
		if (idp->mobj_printf_drv)						\
			memlog_write_printf(idp->mobj_printf_drv, MEMLOG_LEVEL_INFO,	\
					"[@]%s:%d " fmt, __func__, __LINE__, ##args);	\
	} while(0)
#define is_memlog_dbg(fmt, args...)	\
	do {										\
		struct is_debug *idp = is_debug_get();					\
		if (idp->mobj_printf_drv)						\
			memlog_write_printf(idp->mobj_printf_drv, MEMLOG_LEVEL_DEBUG,	\
					"[@]%s:%d " fmt, __func__, __LINE__, ##args);	\
	} while(0)
#define is_memlog_warn(fmt, args...)	\
	do {										\
		struct is_debug *idp = is_debug_get();					\
		if (idp->mobj_printf_drv)						\
			memlog_write_printf(idp->mobj_printf_drv, MEMLOG_LEVEL_CAUTION,	\
					"[@]%s:%d " fmt, __func__, __LINE__, ##args);	\
	} while(0)
#define is_memlog_err(fmt, args...)	\
	do {										\
		struct is_debug *idp = is_debug_get();					\
		if (idp->mobj_printf_drv)						\
			memlog_write_printf(idp->mobj_printf_drv, MEMLOG_LEVEL_ERR,	\
					"[@]%s:%d " fmt, __func__, __LINE__, ##args);	\
	} while(0)
int is_debug_memlog_alloc_dump(phys_addr_t paddr, size_t size, const char *name);
void is_debug_memlog_dump_cr_all(int log_level);
#else
#define is_memlog_info(fmt, args...)	do { } while(0)
#define is_memlog_dbg(fmt, args...)	do { } while(0)
#define is_memlog_warn(fmt, args...)	do { } while(0)
#define is_memlog_err(fmt, args...)	do { } while(0)
#define is_debug_memlog_alloc_dump(p, s, n)	({0;})
#define is_debug_memlog_dump_cr_all(l)		do { } while(0)
#endif

/* cdump */
#define SIZE_CDUMP_PREFIX	18	/* [%5lu.%06lu] [%d]  */
#define SIZE_CDUMP_BUF		256
#define MAX_CDUMP_BUF		(SIZE_CDUMP_PREFIX + SIZE_CDUMP_BUF)
void hex_cdump(const char *level, const char *prefix_str, int prefix_type,
		    int rowsize, int groupsize,
		    const void *buf, size_t len, bool ascii);
#define cinfo(fmt, args...)	is_memlog_warn(fmt, ##args)

#endif
