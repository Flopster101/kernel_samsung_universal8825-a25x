/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IPC Driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *      Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */

#ifndef _MAILBOX_CHUB_IPC_H
#define _MAILBOX_CHUB_IPC_H

#include "ipc_hw.h"
#include "ipc_common.h"

#if defined(SEOS) || defined(EMBOS)
#include "ipc_chub_chub.h"
#else
#include "ipc_chub_ap.h"
#endif

#define IPC_VERSION (20200831)

#define CHECK_HW_TRIGGER
#define SENSOR_NAME_SIZE (32)

/* contexthub bootargs */
#define BL_OFFSET		(0x0)
#define MAP_INFO_MAX_SIZE (128)
#define CHUB_PERSISTBUF_SIZE (96)

#define INVAL_CHANNEL (-1)

#ifndef HOSTINTF_SENSOR_DATA_MAX
#define HOSTINTF_SENSOR_DATA_MAX    240
#endif

#ifndef PACKET_SIZE_MAX
#define PACKET_SIZE_MAX (272)
#endif

//size for 20 sensors
#define CHUB_DFS_SIZE (120)

#define OS_UPDT_MAGIC	"Nanohub OS"
#define CHUB_IPC_MAGIC "IPC magic"

#define BOOTMODE_COLD       (0x7733)
#define BOOTMODE_PWRGATING  (0x1188)

/* ap/chub status with alive check */
#define AP_WAKE             (0x1)
#define AP_SLEEP            (0x2)
#if IS_ENABLED(CONFIG_SHUB)
#define AP_PREPARE          (0x3)
#define AP_COMPLETE         (0x4)
#else
#define CHUB_PG_IN          (0x3)
#define CHUB_PG_OUT         (0x4)
#endif

#define CHUB_REBOOT_REQ            (0xff)

#define READY_TO_GO 99

struct chub_bootargs {
	char magic[16];
	u32 ipc_version;
	u32 bl_start;
	u32 bl_end;
	u32 code_start;
	u32 code_end;
	u32 ipc_start;
	u32 ipc_end;
	u32 cipc_init_map;
	u32 ram_start;
	u32 ram_end;
	u32 dump_start;
	u32 dump_end;
	u32 chubclk;
	u16 bootmode;
	u16 runtimelog;
	/* LOCAL_POWERGATE */
	u32 psp;
	u32 msp;
	/* boot time parameter for Alive MCT */
	u64 boottime;
	u32 fw_ver;
};

/* ipc map
 * data channel: AP -> CHUB
 * data channel: CHUB -> AP
 * event channel: AP -> CHUB / ctrl
 * event channel: CHUB -> AP / ctrl
 * logbuf / logbuf_ctrl
 */
enum sr_num {
	SR_0 = 0,
	SR_1 = 1,
	SR_2 = 2,
	SR_3 = 3,
};

#define SR_DEBUG_VAL_LOW SR_1
#define SR_DEBUG_VAL_HIGH SR_2
#define SR_CHUB_ALIVE SR_3
#define SR_BOOT_MODE SR_0

enum irq_chub {
	IRQ_NUM_CHUB_LOG = IRQ_NUM_EVT_END,
	IRQ_NUM_CHUB_ALIVE,
	IRQ_INVAL = 0xff,
};

enum irq_evt_chub {
	IRQ_EVT_A2C_RESET = CIPC_REG_MAX,
	IRQ_EVT_A2C_WAKEUP,
	IRQ_EVT_A2C_WAKEUP_CLR,
	IRQ_EVT_A2C_SHUTDOWN,
	IRQ_EVT_A2C_LOG,
	IRQ_EVT_A2C_DEBUG,
	IRQ_EVT_C2A_DEBUG,
	IRQ_EVT_C2A_ASSERT,
	IRQ_EVT_C2A_INT, /* 30 */
	IRQ_EVT_C2A_INTCLR,
	IRQ_EVT_C2A_LOG,
	IRQ_EVT_A2C_AON_START,
	IRQ_EVT_A2C_AON_STOP,
	IRQ_EVT_CHUB_MAX,
	IRQ_EVT_CHUB_INVAL = 0xffff,
};

enum ipc_debug_event {
	IPC_DEBUG_UTC_STOP,	/* no used. UTC_NONE */
	IPC_DEBUG_UTC_AGING,
	IPC_DEBUG_UTC_WDT,
	IPC_DEBUG_UTC_IDLE,
	IPC_DEBUG_UTC_MEM,
	IPC_DEBUG_UTC_TIMER,
	IPC_DEBUG_UTC_GPIO,
	IPC_DEBUG_UTC_SPI,
	IPC_DEBUG_UTC_CMU,
	IPC_DEBUG_UTC_TIME_SYNC,
	IPC_DEBUG_UTC_ASSERT,	/* 10 */
	IPC_DEBUG_UTC_FAULT,
	IPC_DEBUG_UTC_CHECK_STATUS,
	IPC_DEBUG_UTC_CHECK_CPU_UTIL,
	IPC_DEBUG_UTC_HEAP_DEBUG,
	IPC_DEBUG_UTC_HANG,
	IPC_DEBUG_UTC_HANG_ITMON,
	IPC_DEBUG_UTC_DFS,
	IPC_DEBUG_UTC_HANG_IPC_C2A_FULL,
	IPC_DEBUG_UTC_HANG_IPC_C2A_CRASH,
	IPC_DEBUG_UTC_HANG_IPC_C2A_DATA_FULL,
	IPC_DEBUG_UTC_HANG_IPC_C2A_DATA_CRASH,
	IPC_DEBUG_UTC_HANG_IPC_A2C_FULL,
	IPC_DEBUG_UTC_HANG_IPC_A2C_CRASH,
	IPC_DEBUG_UTC_HANG_IPC_A2C_DATA_FULL,
	IPC_DEBUG_UTC_HANG_IPC_A2C_DATA_CRASH,
	IPC_DEBUG_UTC_HANG_IPC_LOGBUF_EQ_CRASH,
	IPC_DEBUG_UTC_HANG_IPC_LOGBUF_DQ_CRASH,
	IPC_DEBUG_UTC_HANG_INVAL_INT,
	IPC_DEBUG_UTC_REBOOT,
	IPC_DEBUG_UTC_IPC_TEST_START,	/* ap request */
	IPC_DEBUG_UTC_IPC_TEST_END,
	IPC_DEBUG_DUMP_STATUS,
	IPC_DEBUG_DFS_GOVERNOR,
	IPC_DEBUG_CLK_DIV,
	IPC_DEBUG_UTC_MAX,
	IPC_DEBUG_NANOHUB_MAX,
	IPC_DEBUG_CHUB_PRINT_LOG,	/* chub request */
	IPC_DEBUG_CHUB_FULL_LOG,
	IPC_DEBUG_CHUB_FAULT,
	IPC_DEBUG_CHUB_ASSERT,
	IPC_DEBUG_CHUB_ERROR,
};

enum ipc_loopback_debug_event {
	IPC_DEBUG_UTC_IPC_AP,
	IPC_DEBUG_UTC_IPC_ABOX,
	IPC_DEBUG_UTC_IPC_GNSS,
	IPC_DEBUG_UTC_IPC_MAX,
};

enum ipc_region {
	IPC_REG_BL,
	IPC_REG_BL_MAP,
	IPC_REG_OS,
	IPC_REG_IPC,
	IPC_REG_IPC_SENSORINFO,
	IPC_REG_RAM,
	IPC_REG_LOG,
	IPC_REG_PERSISTBUF,
	IPC_REG_DUMP,
	IPC_REG_MAX,
};


/* it's from struct HostIntfDataBuffer buf */
struct ipc_log_content {
	u8 pad0;
	u8 length;
	u16 pad1;
	u8 buffer[sizeof(u64) + HOSTINTF_SENSOR_DATA_MAX - sizeof(u32)];
};

#define LOGBUF_SIZE (80)
#ifdef CONFIG_LOGBUF_NUM
#define LOGBUF_NUM CONFIG_LOGBUF_NUM
#else
#define LOGBUF_NUM (80)
#endif
#define LOGBUF_TOTAL_SIZE (LOGBUF_SIZE * LOGBUF_NUM)
/* u8 error, u8 newline, u16 size, void *next(CM4) */
#define LOGBUF_DATA_SIZE (LOGBUF_SIZE - sizeof(u64) - sizeof(u32) - sizeof(u16) - 3 * sizeof(u8))
#define LOGBUF_FLUSH_THRESHOLD (LOGBUF_NUM / 2)

struct logbuf_content {
	char buf[LOGBUF_DATA_SIZE];
	u64 timestamp;
	u32 nextaddr;
	u16 size;
	u8 level;
	u8 error;
	u8 newline;
};

struct logbuf_output {
	char *buf;
	u64 timestamp;
	u16 size;
	u8 level;
};

enum ipc_fw_loglevel {
	CHUB_RT_LOG_OFF,
	CHUB_RT_LOG_DUMP,
	CHUB_RT_LOG_DUMP_PRT,
};
struct ipc_logbuf {
	struct logbuf_content log[LOGBUF_NUM];
	u32 eq;			/* write owner chub (index_writer) */
	u32 dq;			/* read onwer ap (index_reader) */
	u32 size;
	u8 full;
	u8 flush_req;
	u8 flush_active;
	u8 loglevel;
	/* for debug */
	int dbg_full_cnt;
	int errcnt;
	int reqcnt;
	u32 fw_num;
};

#define MAX_PHYSENSOR_NUM (10)
#define MAX_SENSOR_NAME (24)
#define MAX_SENSOR_VENDOR_NAME (8)

struct sensor_info {
	uint8_t sensortype;
	uint8_t senstype;
	uint8_t chipid;
	char vendorname[MAX_SENSOR_VENDOR_NAME];
	char name[MAX_SENSOR_NAME];
};

struct sensor_map {
	char magic[16];
	int index;
	struct sensor_info sinfo[MAX_PHYSENSOR_NUM];
};

enum ipc_value_id {
	IPC_VAL_A2C_DEBUG,
	IPC_VAL_A2C_DEBUG2,
	IPC_VAL_A2C_SENSORID,
	IPC_VAL_A2C_DFS,
	IPC_VAL_A2C_CHIPID,
	IPC_VAL_C2A_DEBUG,
	IPC_VAL_C2A_SENSORID,
	IPC_VAL_HW_BOOTMODE,
	IPC_VAL_HW_AP_STATUS,
	IPC_VAL_HW_DEBUG,
	IPC_VAL_MAX
};

struct chub_dfs {
	u32 numSensor;
	u32 governor;
	u32 sampleTime[CHUB_DFS_SIZE];
};

struct chub_persist {
	u8 fault_handler[CHUB_PERSISTBUF_SIZE];
	/* dfs info */
	struct chub_dfs dfs;
};

#if defined(CONFIG_CONTEXTHUB_SENSOR_DEBUG)
struct ipc_cpu_info {
        u32 pgcnt;
        u64 pgtime;
        u32 cgcnt;
        u64 cgtime;
        u64 curtime;
};

struct sensor_check_cnt {
	u32 sensor_cnt_last;
	u32 event_flush_last;
	u32 event_rtc_last;
	u32 event_hrm_last;
	u32 rtc_expired_last;
	u32 sensor_cnt_no_update;
};
#endif

struct ipc_map_area {
	struct chub_persist persist;
	char magic[16];
	u16 wake;
	u64 value[IPC_VAL_MAX];
#if defined(CONFIG_CONTEXTHUB_SENSOR_DEBUG)
        u32 sensor_cnt;
        u32 event_flush_cnt;
        u32 event_rtc_cnt;
        u32 event_hrm_cnt;
        u32 rtc_expired_cnt;
        struct ipc_cpu_info cpu_info;
#endif
	struct sensor_map sensormap;
	struct ipc_logbuf logbuf;
	struct cipc_map_area *cipc_base;
};

/* ap-chub ipc */
struct ipc_info {
	struct ipc_area ipc_addr[IPC_REG_MAX];
	struct ipc_map_area *ipc_map;
	struct cipc_info *cipc;
	enum ipc_owner owner;
	void *mb_base;
	void *sram_base;
	enum ipc_mb_id my_mb_id;
	enum ipc_mb_id opp_mb_id;
};

bool ipc_have_sensor_info(struct sensor_map *sensor_map);
struct ipc_info *ipc_init(enum ipc_owner owner, enum ipc_mb_id mb_id,
			  void *sram_base, void *mb_base,
			  struct cipc_funcs *func);
void *ipc_get_base(enum ipc_region area);
u32 ipc_get_size(enum ipc_region area);
int ipc_hw_read_int_start_index(enum ipc_owner owner);
int ipc_sensor_alive_check(void);
/* logbuf functions */
enum ipc_fw_loglevel ipc_logbuf_loglevel(enum ipc_fw_loglevel loglevel,
					 int set);
struct ipc_info *ipc_get_info(void);
void ipc_set_chub_boottime(bool alive_mct_enabled);
void ipc_set_chub_bootmode(u16 bootmode, u16 rtlog);
u16 ipc_get_chub_rtlogmode(void);
u16 ipc_get_chub_bootmode(void);
void ipc_set_dfs_gov(uint32_t gov);
u32 ipc_get_dfs_gov(void);
void ipc_set_dfs_numSensor(uint32_t num);
u32 ipc_get_dfs_numSensor(void);
void ipc_set_ap_wake(u16 wake);
u16 ipc_get_ap_wake(void);
void ipc_dump(void);
u64 ipc_read_hw_value(enum ipc_value_id id);
void ipc_write_hw_value(enum ipc_value_id id, u64 val);
void ipc_write_value(enum ipc_value_id id, u64 val);
u64 ipc_read_value(enum ipc_value_id id);
#endif
