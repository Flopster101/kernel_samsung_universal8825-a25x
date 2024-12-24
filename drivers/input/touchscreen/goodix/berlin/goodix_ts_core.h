/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _GOODIX_TS_CORE_H_
#define _GOODIX_TS_CORE_H_

#include "../../../sec_input/sec_input.h"

extern struct device *ptsp;

#define GOODIX_CORE_DRIVER_NAME			"goodix_ts"
#define GOODIX_DRIVER_VERSION			"v1.0.34"
#define GOODIX_MAX_TOUCH			10
#define GOODIX_CFG_MAX_SIZE			4096
#define GOODIX_MAX_STR_LABLE_LEN		128

#define GOODIX_BIN_FW_INFO_ADDR			0x6

#define GOODIX_NOCODE				"NOCODE"

#define TOUCH_PRINT_INFO_DWORK_TIME		30000	/* 30s */

#define GOODIX_NORMAL_RESET_DELAY_MS		200
#define GOODIX_HOLD_CPU_RESET_DELAY_MS		5

#define GOODIX_RETRY_3				3
#define GOODIX_RETRY_5				5
#define GOODIX_RETRY_10				10

#define DEFAULT_MAX_DRV_NUM			75
#define DEFAULT_MAX_SEN_NUM			75

#define SEC_JITTER1_TEST			0
#define SEC_JITTER2_TEST			1
#define SEC_SRAM_TEST				2
#define SEC_SHORT_TEST				3
#define SEC_OPEN_TEST				4
#define SEC_TEST_MAX_ITEM			9

#define JITTER_100_FRAMES			0
#define JITTER_1000_FRAMES			1

#define FREQ_NORMAL				0
#define FREQ_HIGH				1
#define FREQ_LOW				2

/* GOODIX REGISTER ADDR */
#define GOODIX_GLOVE_MODE_ADDR		0x72
#define GOODIX_COVER_MODE_ADDR		0x78
#define GOODIX_ED_MODE_ADDR			0x93
#define GOODIX_LS_MODE_ADDR			0x40
#define WATCH_DOG_REG			0xD040
#define GIO_REG_BD				0xC804
#define GIO_REG_BB				0xC808

/* SEC status type */
#define TYPE_STATUS_EVENT_CMD_DRIVEN	0
#define TYPE_STATUS_EVENT_ERR			1
#define TYPE_STATUS_EVENT_INFO			2
#define TYPE_STATUS_EVENT_USER_INPUT	3
#define TYPE_STATUS_EVENT_SPONGE_INFO	6
#define TYPE_STATUS_EVENT_VENDOR_INFO	7

#define SEC_TS_READY_STATUS         0x00
#define SEC_TS_ACK_WET_MODE			0x01
#define SEC_TS_NOISE_MODE			0x02

#define STATUS_EVENT_VENDOR_STATE_CHANGED		0x61
#define STATUS_EVENT_VENDOR_ACK_NOISE_STATUS_NOTI	0x64
#define STATUS_EVENT_VENDOR_PROXIMITY			0x6A
#define STATUS_EVENT_VENDOR_ACK_PRE_NOISE_STATUS_NOTI	0x6D
#define STATUS_EVENT_VENDOR_ACK_CHARGER_STATUS_NOTI	0x6E

/* SEC status event id */
#define SEC_TS_COORDINATE_EVENT			0
#define SEC_TS_STATUS_EVENT			1
#define SEC_TS_GESTURE_EVENT			2
#define SEC_TS_EMPTY_EVENT			3

/* SEC touch type */
#define SEC_TS_TOUCHTYPE_NORMAL		0
#define SEC_TS_TOUCHTYPE_HOVER		1
#define SEC_TS_TOUCHTYPE_FLIPCOVER	2
#define SEC_TS_TOUCHTYPE_GLOVE		3
#define SEC_TS_TOUCHTYPE_STYLUS		4
#define SEC_TS_TOUCHTYPE_PALM		5
#define SEC_TS_TOUCHTYPE_WET		6
#define SEC_TS_TOUCHTYPE_PROXIMITY	7
#define SEC_TS_TOUCHTYPE_JIG		8

/* SEC_TS_GESTURE_TYPE */
#define SEC_TS_GESTURE_CODE_SWIPE			0x00
#define SEC_TS_GESTURE_CODE_DOUBLE_TAP			0x01
#define SEC_TS_GESTURE_CODE_PRESS			0x03
#define SEC_TS_GESTURE_CODE_SINGLE_TAP			0x04

/* SEC_TS_GESTURE_ID */
#define SEC_TS_GESTURE_ID_AOD					0x00
#define SEC_TS_GESTURE_ID_DOUBLETAP_TO_WAKEUP	0x01

#define SNR_TEST_NON_TOUCH						0
#define SNR_TEST_TOUCH							1

#define GOODIX_DEFAULT_CFG_NAME		"goodix_cfg_group.cfg"

typedef enum {
	GOODIX_TEST_RESULT_PASS = 0x00,
	GOODIX_TEST_RESULT_FAIL = 0x01,
} TEST_RESULT;

enum goodix_rawdata_test_type {
	RAWDATA_TEST_TYPE_MUTUAL_RAW = 0,
	RAWDATA_TEST_TYPE_MUTUAL_DIFF,
	RAWDATA_TEST_TYPE_SELF_RAW,
	RAWDATA_TEST_TYPE_SELF_DIFF,
};

enum FW_UPDATE_PARAM {
	TSP_BUILT_IN = 0,
	TSP_SDCARD,
};

enum CORD_PROB_STA {
	CORE_MODULE_UNPROBED = 0,
	CORE_MODULE_PROB_SUCCESS = 1,
	CORE_MODULE_PROB_FAILED = -1,
	CORE_MODULE_REMOVED = -2,
};

enum GOODIX_ERR_CODE {
	GOODIX_EBUS      = (1<<0),
	GOODIX_ECHECKSUM = (1<<1),
	GOODIX_EVERSION  = (1<<2),
	GOODIX_ETIMEOUT  = (1<<3),
	GOODIX_EMEMCMP   = (1<<4),

	GOODIX_EOTHER    = (1<<7)
};

enum IC_TYPE_ID {
	IC_TYPE_NONE,
	IC_TYPE_NORMANDY,
	IC_TYPE_NANJING,
	IC_TYPE_YELLOWSTONE,
	IC_TYPE_BERLIN_A,
	IC_TYPE_BERLIN_B,
	IC_TYPE_BERLIN_D,
	IC_TYPE_GT9916K
};

enum GOODIX_IC_CONFIG_TYPE {
	CONFIG_TYPE_TEST = 0,
	CONFIG_TYPE_NORMAL = 1,
	CONFIG_TYPE_HIGHSENSE = 2,
	CONFIG_TYPE_CHARGER = 3,
	CONFIG_TYPE_CHARGER_HS = 4,
	CONFIG_TYPE_HOLSTER = 5,
	CONFIG_TYPE_HOSTER_CH = 6,
	CONFIG_TYPE_OTHER = 7,
	/* keep this at the last */
	GOODIX_MAX_CONFIG_GROUP = 8,
};

enum CHECKSUM_MODE {
	CHECKSUM_MODE_U8_LE,
	CHECKSUM_MODE_U16_LE,
};

#define MAX_SCAN_FREQ_NUM            10
#define MAX_SCAN_RATE_NUM            10
#define MAX_FREQ_NUM_STYLUS          8
#define MAX_STYLUS_SCAN_FREQ_NUM     6
#pragma pack(1)
struct goodix_fw_version {
	u8 rom_pid[6];               /* rom PID */
	u8 rom_vid[3];               /* Mask VID */
	u8 rom_vid_reserved;
	u8 patch_pid[8];              /* Patch PID */
	u8 patch_vid[4];              /* Patch VID */
	u8 patch_vid_reserved;
	u8 sensor_id;
	u8 reserved[2];
	u16 checksum;
};

struct goodix_ic_info_version {
	u8 info_customer_id;
	u8 info_version_id;
	u8 ic_die_id;
	u8 ic_version_id;
	u32 config_id;
	u8 config_version;
	u8 frame_data_customer_id;
	u8 frame_data_version_id;
	u8 touch_data_customer_id;
	u8 touch_data_version_id;
	u8 reserved[3];
};

struct goodix_ic_info_feature { /* feature info*/
	u16 freqhop_feature;
	u16 calibration_feature;
	u16 gesture_feature;
	u16 side_touch_feature;
	u16 stylus_feature;
};

struct goodix_ic_info_param { /* param */
	u8 drv_num;
	u8 sen_num;
	u8 button_num;
	u8 force_num;
	u8 active_scan_rate_num;
	u16 active_scan_rate[MAX_SCAN_RATE_NUM];
	u8 mutual_freq_num;
	u16 mutual_freq[MAX_SCAN_FREQ_NUM];
	u8 self_tx_freq_num;
	u16 self_tx_freq[MAX_SCAN_FREQ_NUM];
	u8 self_rx_freq_num;
	u16 self_rx_freq[MAX_SCAN_FREQ_NUM];
	u8 stylus_freq_num;
	u16 stylus_freq[MAX_FREQ_NUM_STYLUS];
};

struct goodix_ic_info_misc { /* other data */
	u32 cmd_addr;
	u16 cmd_max_len;
	u32 cmd_reply_addr;
	u16 cmd_reply_len;
	u32 fw_state_addr;
	u16 fw_state_len;
	u32 fw_buffer_addr;
	u16 fw_buffer_max_len;
	u32 frame_data_addr;
	u16 frame_data_head_len;
	u16 fw_attr_len;
	u16 fw_log_len;
	u8 pack_max_num;
	u8 pack_compress_version;
	u16 stylus_struct_len;
	u16 mutual_struct_len;
	u16 self_struct_len;
	u16 noise_struct_len;
	u32 touch_data_addr;
	u16 touch_data_head_len;
	u16 point_struct_len;
	u16 reserved1;
	u16 reserved2;
	u32 mutual_rawdata_addr;
	u32 mutual_diffdata_addr;
	u32 mutual_refdata_addr;
	u32 self_rawdata_addr;
	u32 self_diffdata_addr;
	u32 self_refdata_addr;
	u32 iq_rawdata_addr;
	u32 iq_refdata_addr;
	u32 im_rawdata_addr;
	u16 im_readata_len;
	u32 noise_rawdata_addr;
	u16 noise_rawdata_len;
	u32 stylus_rawdata_addr;
	u16 stylus_rawdata_len;
	u32 noise_data_addr;
	u32 esd_addr;
	u8 reserved[10];
};

struct goodix_ic_info_sec {
	u16 ic_vendor;
	u8 ic_name_list;
	u8 project_id;
	u8 module_version;
	u8 firmware_version;
	u32 total_checksum;
	u32 sponge_addr;
	u16 sponge_len;
	u32 test_buf_addr;
};

struct goodix_ic_info_other {
	u32 irrigation_data_addr;
	u32 algo_debug_data_addr;
	u16 algo_debug_data_len;
	u32 update_sync_adata_addr;
	u16 screen_max_x;
	u16 screen_max_y;
};

struct goodix_ic_info {
	u16 length;
	struct goodix_ic_info_version version;
	struct goodix_ic_info_feature feature;
	struct goodix_ic_info_param parm;
	struct goodix_ic_info_misc misc;
	struct goodix_ic_info_sec sec;
};
#pragma pack()

typedef struct {
	uint32_t checksum;
	uint32_t address;
	uint32_t length;
} __packed flash_head_info_t;

/*
 * struct ts_rawdata_info
 *
 */
#define TS_RAWDATA_BUFF_MAX             7000
#define TS_RAWDATA_RESULT_MAX           100
struct ts_rawdata_info {
	int used_size; //fill in rawdata size
	s16 buff[TS_RAWDATA_BUFF_MAX];
	char result[TS_RAWDATA_RESULT_MAX];
};

/*
 * struct goodix_module - external modules container
 * @head: external modules list
 * @initialized: whether this struct is initialized
 * @mutex: mutex lock
 * @wq: workqueue to do register work
 * @ts: goodix_ts_data pointer
 */
struct goodix_module {
	struct list_head head;
	bool initialized;
	struct mutex mutex;
	struct workqueue_struct *wq;
	struct goodix_ts_data *ts;
};

#define MAX_CMD_DATA_LEN 10
#define MAX_CMD_BUF_LEN  16
#pragma pack(1)
struct goodix_ts_cmd {
	union {
		struct {
			u8 state;
			u8 ack;
			u8 len;
			u8 cmd;
			u8 data[MAX_CMD_DATA_LEN];
		};
		u8 buf[MAX_CMD_BUF_LEN];
	};
};
#pragma pack()

enum USB_PLUG_SATUS {
	USB_PLUG_DETACHED = 0,
	USB_PLUG_ATTACHED = 1
};

/* interrupt event type */
enum ts_event_type {
	EVENT_INVALID = 0,
	EVENT_TOUCH = (1 << 0), /* finger touch event */
	EVENT_PEN = (1 << 1),   /* pen event */
	EVENT_REQUEST = (1 << 2),
	EVENT_GESTURE = (1 << 3),
	EVENT_STATUS = (1 << 4),
	EVENT_EMPTY = (1 << 5),
};

enum ts_request_type {
	REQUEST_TYPE_CONFIG = 1,
	REQUEST_TYPE_RESET = 3,
};

/* notifier event */
enum ts_notify_event {
	NOTIFY_FWUPDATE_START,
	NOTIFY_FWUPDATE_FAILED,
	NOTIFY_FWUPDATE_SUCCESS,
	NOTIFY_SUSPEND,
	NOTIFY_RESUME,
	NOTIFY_ESD_OFF,
	NOTIFY_ESD_ON,
	NOTIFY_CFG_BIN_FAILED,
	NOTIFY_CFG_BIN_SUCCESS,
};

enum check_update_status {
	SKIP_FW_UPDATE = 0,
	NEED_FW_UPDATE,
};

/*
 * struct goodix_ts_event - touch event struct
 * @event_type: touch event type, touch data or
 *	request event
 * @event_data: event data
 */
struct goodix_ts_event {
	u16 event_type;
	u8 request_code; /* represent the request type */

	u8 gesture_type;
	u8 gesture_id;
	u8 gesture_data[4];

	unsigned char status_type;
	unsigned char status_id;
	unsigned char status_data[5];

	u8 hover_event; //virtual_prox
};

enum goodix_ic_bus_type {
	GOODIX_BUS_TYPE_I2C,
	GOODIX_BUS_TYPE_SPI,
	GOODIX_BUS_TYPE_I3C,
};

struct goodix_bus_interface {
	int bus_type;
	int ic_type;
	struct device *dev;
	int (*read)(struct device *dev, unsigned int addr,
			unsigned char *data, size_t len);
	int (*write)(struct device *dev, unsigned int addr,
			unsigned char *data, size_t len);
};

struct goodix_ts_hw_ops {
	int (*power_on)(struct goodix_ts_data *ts, bool on);
	int (*dev_confirm)(struct goodix_ts_data *ts);
	int (*resume)(struct goodix_ts_data *ts);
	int (*suspend)(struct goodix_ts_data *ts);
	int (*gesture)(struct goodix_ts_data *ts, bool enable);
	int (*reset)(struct goodix_ts_data *ts, int delay_ms);
	int (*irq_enable)(struct goodix_ts_data *ts, bool enable);
	int (*read)(struct goodix_ts_data *ts, unsigned int addr,
			unsigned char *data, size_t len);
	int (*write)(struct goodix_ts_data *ts, unsigned int addr,
			unsigned char *data, size_t len);
	int (*read_from_sponge)(struct goodix_ts_data *ts, u16 offset, u8 *data, int len);
	int (*write_to_sponge)(struct goodix_ts_data *ts, u16 offset, u8 *data, int len);
	int (*write_to_flash)(struct goodix_ts_data *ts, int addr, unsigned char *buf, int len);
	int (*read_from_flash)(struct goodix_ts_data *ts, int addr, unsigned char *buf, int len);
	int (*send_cmd)(struct goodix_ts_data *ts, struct goodix_ts_cmd *cmd);
	int (*send_cmd_delay)(struct goodix_ts_data *ts, struct goodix_ts_cmd *cmd, int delay);
	int (*send_config)(struct goodix_ts_data *ts, u8 *config, int len);
	int (*read_config)(struct goodix_ts_data *ts, u8 *config_data, int size);
	int (*read_version)(struct goodix_ts_data *ts, struct goodix_fw_version *version);
	int (*get_ic_info)(struct goodix_ts_data *ts, struct goodix_ic_info *ic_info);
	int (*esd_check)(struct goodix_ts_data *ts);
	int (*event_handler)(struct goodix_ts_data *ts, struct goodix_ts_event *ts_event);
	int (*after_event_handler)(struct goodix_ts_data *ts); /* clean sync flag */
	int (*get_capacitance_data)(struct goodix_ts_data *ts, struct ts_rawdata_info *info);
	int (*enable_idle)(struct goodix_ts_data *ts, int enable);
	int (*sense_off)(struct goodix_ts_data *ts, int sen_off);
	int (*ed_enable)(struct goodix_ts_data *ts, int enable);
	int (*pocket_mode_enable)(struct goodix_ts_data *ts, int enable);
};

/*
 * struct goodix_ts_esd - esd protector structure
 * @esd_work: esd delayed work
 * @esd_on: 1 - turn on esd protection, 0 - turn
 *  off esd protection
 */
struct goodix_ts_esd {
	bool irq_status;
	atomic_t esd_on;
	struct delayed_work esd_work;
	struct notifier_block esd_notifier;
	struct goodix_ts_data *ts;
};

enum goodix_core_init_stage {
	CORE_UNINIT,
	CORE_INIT_FAIL,
	CORE_INIT_STAGE1,
	CORE_INIT_STAGE2
};

struct goodix_ic_config {
	int len;
	u8 data[GOODIX_CFG_MAX_SIZE];
};

struct goodix_test_info {
	char *item_name;
	bool isFinished;
	int data[16];
};

struct goodix_ts_test_rawdata {
	int16_t data[DEFAULT_MAX_DRV_NUM * DEFAULT_MAX_SEN_NUM];
	int size;
	int max;
	int min;
	int delta;
};

struct goodix_ts_test_self_rawdata {
	int16_t data[DEFAULT_MAX_DRV_NUM + DEFAULT_MAX_SEN_NUM];
	int size;
	int tx_max;
	int tx_min;
	int rx_max;
	int rx_min;
};

struct goodix_ts_test_type {
	enum goodix_rawdata_test_type type;
	int frequency_flag;
	char spec_name[SEC_CMD_STR_LEN];
};

#define QUERYBIT(longlong, bit) (!!(longlong[bit / 8] & (1 << bit % 8)))

#define OPEN_TEST_RESULT		0x10FC2
#define OPEN_TEST_RESULT_LEN	22
#define DRV_CHAN_BYTES			7
#define SEN_CHAN_BYTES			10
#define OPEN_SHORT_TEST_RESULT_LEN			(DRV_CHAN_BYTES + SEN_CHAN_BYTES)	/* TX:7, RX:10 */
#define OPEN_SHORT_TEST_RESULT_RX_OFFSET	7

struct goodix_ts_test {
	enum goodix_rawdata_test_type type;
	int frequency_flag;
	struct goodix_test_info info[SEC_TEST_MAX_ITEM];
	struct goodix_ts_test_rawdata rawdata;
	struct goodix_ts_test_self_rawdata selfraw;
	short snr_result[9 * 3];	// 9 points * (avg & snr1 & snr2)
	char open_short_test_trx_result[OPEN_SHORT_TEST_RESULT_LEN];
};

struct goodix_ts_data {
	int init_stage;
	struct platform_device *pdev;
	struct goodix_fw_version fw_version;
	struct goodix_fw_version merge_bin_ver;
	struct goodix_ic_info ic_info;
	struct goodix_ic_info_sec fw_info_bin;
	struct goodix_bus_interface *bus;
	struct goodix_ts_test test_data;
	struct goodix_ts_hw_ops *hw_ops;
	struct sec_cmd_data sec;
	struct sec_ts_plat_data *plat_data;
	struct gesture_module *gsx_gesture;

	/* TODO counld we remove this from core data? */
	struct goodix_ts_event ts_event;

	/* every pointer of this array represent a kind of config */
	struct goodix_ic_config *ic_configs[GOODIX_MAX_CONFIG_GROUP];

	/* get from dts */
	unsigned int max_drv_num;
	unsigned int max_sen_num;
	unsigned int drv_map[DEFAULT_MAX_DRV_NUM];
	unsigned int sen_map[DEFAULT_MAX_SEN_NUM];
	unsigned int short_test_time_reg;
	unsigned int short_test_status_reg;
	unsigned int short_test_result_reg;
	unsigned int drv_drv_reg;
	unsigned int sen_sen_reg;
	unsigned int drv_sen_reg;
	unsigned int diff_code_reg;

	unsigned int production_test_addr;
	unsigned int switch_cfg_cmd;
	unsigned int switch_freq_cmd;
	unsigned int snr_cmd;
	unsigned int sensitive_cmd;

	unsigned int isp_ram_reg;
	unsigned int flash_cmd_reg;
	unsigned int isp_buffer_reg;
	unsigned int config_data_reg;
	unsigned int misctl_reg;
	unsigned int watch_dog_reg;
	unsigned int config_id_reg;
	unsigned int enable_misctl_val;

	bool enable_esd_check;

	int irq;
	size_t irq_trig_cnt;

	int factory_position;
	int lpm_coord_event_cnt;

	struct delayed_work work_print_info;
	struct delayed_work work_read_info;
	/* for debugging */
	struct delayed_work debug_delayed_work;
	bool info_work_done;

	/* when this flag is true, driver should not clean the sync flag */
	bool tools_ctrl_sync;

	struct notifier_block ts_notifier;
	struct goodix_ts_esd ts_esd;

	int flip_enable;

	int irq_empty_count;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	struct notifier_block sec_input_nb;
#endif

	int debug_flag;

	u32 refresh_rate;
	u8 glove_enable;

	bool sponge_inf_dump;
	u8 sponge_dump_format;
	u8 sponge_dump_event;
	bool sponge_dump_delayed_flag;
	u8 sponge_dump_delayed_area;
	u16 sponge_dump_border;
};

/* external module structures */
enum goodix_ext_priority {
	EXTMOD_PRIO_RESERVED = 0,
	EXTMOD_PRIO_FWUPDATE,
	EXTMOD_PRIO_GESTURE,
	EXTMOD_PRIO_HOTKNOT,
	EXTMOD_PRIO_DBGTOOL,
	EXTMOD_PRIO_DEFAULT,
};

#define EVT_HANDLED				0
#define EVT_CONTINUE			0
#define EVT_CANCEL				1
#define EVT_CANCEL_IRQEVT		1
#define EVT_CANCEL_SUSPEND		1
#define EVT_CANCEL_RESUME		1
#define EVT_CANCEL_RESET		1

/*
 * struct goodix_ext_module - external module struct
 * @list: list used to link into modules manager
 * @name: name of external module
 * @priority: module priority vlaue, zero is invalid
 * @funcs: operations callback
 * @priv_data: private data region
 * @kobj: kobject
 * @work: used to queue one work to do registration
 */
struct goodix_ext_module {
	struct list_head list;
	char *name;
	enum goodix_ext_priority priority;
	const struct goodix_ext_module_funcs *funcs;
	void *priv_data;
	struct kobject kobj;
	struct work_struct work;
};

/* external module's operations callback */
struct goodix_ext_module_funcs {
	int (*init)(struct goodix_ts_data *ts,
			struct goodix_ext_module *module);
	int (*exit)(struct goodix_ts_data *ts,
			struct goodix_ext_module *module);
	int (*before_reset)(struct goodix_ts_data *ts,
			struct goodix_ext_module *module);
	int (*after_reset)(struct goodix_ts_data *ts,
			struct goodix_ext_module *module);
	int (*before_suspend)(struct goodix_ts_data *ts,
			struct goodix_ext_module *module);
	int (*after_suspend)(struct goodix_ts_data *ts,
			struct goodix_ext_module *module);
	int (*before_resume)(struct goodix_ts_data *ts,
			struct goodix_ext_module *module);
	int (*after_resume)(struct goodix_ts_data *ts,
			struct goodix_ext_module *module);
	int (*irq_event)(struct goodix_ts_data *ts,
			struct goodix_ext_module *module);
};

#define GSX_GESTURE_TYPE_LEN	32
/*
 * struct gesture_module - gesture module data
 * @registered: module register state
 * @sysfs_node_created: sysfs node state
 * @gesture_type: valid gesture type, each bit represent one gesture type
 * @gesture_data: store latest gesture code get from irq event
 * @gesture_ts_cmd: gesture command data
 */
struct gesture_module {
	atomic_t registered;
	rwlock_t rwlock;
	u8 gesture_type[GSX_GESTURE_TYPE_LEN];
	u8 gesture_data;
	struct goodix_ext_module module;
	struct goodix_ts_data *ts;
};

/*
 * struct goodix_ext_attribute - exteranl attribute struct
 * @attr: attribute
 * @show: show interface of external attribute
 * @store: store interface of external attribute
 */
struct goodix_ext_attribute {
	struct attribute attr;
	ssize_t (*show)(struct goodix_ext_module *, char *);
	ssize_t (*store)(struct goodix_ext_module *, const char *, size_t);
};

/* external attrs helper macro */
#define __EXTMOD_ATTR(_name, _mode, _show, _store)	{	\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.show   = _show,	\
	.store  = _store,	\
}

/* external attrs helper macro, used to define external attrs */
#define DEFINE_EXTMOD_ATTR(_name, _mode, _show, _store)	\
	static struct goodix_ext_attribute ext_attr_##_name = \
	__EXTMOD_ATTR(_name, _mode, _show, _store)

/* log macro */
extern bool debug_log_flag;
#define ts_info(fmt, arg...)	input_info(true, ptsp, "[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define	ts_err(fmt, arg...)	input_err(true, ptsp, "[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)
#define ts_debug(fmt, arg...)	{if (debug_log_flag) input_info(true, ptsp, "[%s:%d] "fmt"\n", __func__, __LINE__, ##arg); }
#define ts_raw_info(fmt, arg...)	input_raw_info(true, ptsp, "[%s:%d] "fmt"\n", __func__, __LINE__, ##arg)

/**
 * goodix_register_ext_module - interface for external module
 * to register into touch core modules structure
 *
 * @module: pointer to external module to be register
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module(struct goodix_ext_module *module);
/* register module no wait */
int goodix_register_ext_module_no_wait(struct goodix_ext_module *module);
/**
 * goodix_unregister_ext_module - interface for external module
 * to unregister external modules
 *
 * @module: pointer to external module
 * return: 0 ok, <0 failed
 */
int goodix_unregister_ext_module(struct goodix_ext_module *module);
/* remove all registered ext module
 * return 0 on success, otherwise return < 0
 */
int goodix_ts_blocking_notify(enum ts_notify_event evt, void *v);

struct goodix_ts_hw_ops *goodix_get_hw_ops(void);
int goodix_get_config_proc(struct goodix_ts_data *ts, const struct firmware *firmware);

int goodix_spi_bus_init(void);
void goodix_spi_bus_exit(void);
int goodix_i2c_bus_init(void);
void goodix_i2c_bus_exit(void);

u32 goodix_append_checksum(u8 *data, int len, int mode);
u8 checksum_u8(u8 *data, u32 size);
u16 checksum_be16(u8 *data, u32 size);
u32 checksum16_u32(const uint8_t *data, int size);
int checksum_cmp(const u8 *data, size_t size, int mode);
int is_risk_data(const u8 *data, int size);
u32 goodix_get_file_config_id(u8 *ic_config);
void goodix_rotate_abcd2cbad(int tx, int rx, s16 *data);
int goodix_gesture_enable(struct goodix_ts_data *ts, int enable);

int goodix_fw_update(struct goodix_ts_data *ts, int update_type, bool force_update);
int goodix_fw_update_init(struct goodix_ts_data *ts, const struct firmware *firmware);
void goodix_fw_update_uninit(void);
int goodix_do_fw_update(struct goodix_ic_config *ic_config);

int gesture_module_init(struct goodix_ts_data *ts);
void gesture_module_exit(struct goodix_ts_data *ts);
int inspect_module_init(void);
void inspect_module_exit(void);
int goodix_tools_init(void);
void goodix_tools_exit(void);

int goodix_ts_sysfs_init(struct goodix_ts_data *ts);
void goodix_ts_sysfs_exit(struct goodix_ts_data *ts);
void goodix_ts_procfs_init(struct goodix_ts_data *ts);
void goodix_ts_procfs_exit(struct goodix_ts_data *ts);

int goodix_jitter_test(struct goodix_ts_data *ts, u8 type);
int goodix_open_test(struct goodix_ts_data *ts);
int goodix_sram_test(struct goodix_ts_data *ts);
int goodix_short_test(struct goodix_ts_data *ts);
int goodix_cache_rawdata(struct goodix_ts_data *ts, struct goodix_ts_test_type *test, int16_t *data);
int goodix_read_realtime(struct goodix_ts_data *ts, struct goodix_ts_test_type *test);
int goodix_snr_test(struct goodix_ts_data *ts, int type, int frames);
void goodix_ts_run_rawdata_all(struct goodix_ts_data *ts);
void goodix_data_statistics(s16 *data, int data_size, int *max, int *min);

int goodix_write_nvm_data(struct goodix_ts_data *ts, unsigned char *data, int size);
int goodix_read_nvm_data(struct goodix_ts_data *ts, unsigned char *data, int size);

int goodix_ts_cmd_init(struct goodix_ts_data *ts);
void goodix_ts_cmd_remove(struct goodix_ts_data *ts);

int goodix_ts_power_on(struct goodix_ts_data *ts);
int goodix_ts_power_off(struct goodix_ts_data *ts);

int gsx_set_lowpowermode(void *data, u8 mode);
void goodix_get_custom_library(struct goodix_ts_data *ts);
int goodix_set_custom_library(struct goodix_ts_data *ts);
int goodix_set_press_property(struct goodix_ts_data *ts);
int goodix_set_fod_rect(struct goodix_ts_data *ts);
int goodix_set_aod_rect(struct goodix_ts_data *ts);

void goodix_ts_release_all_finger(struct goodix_ts_data *ts);
int set_refresh_rate_mode(struct goodix_ts_data *ts);
int goodix_set_cover_mode(struct goodix_ts_data *ts);
int goodix_set_cmd(struct goodix_ts_data *ts, u8 reg, u8 mode);

#endif
