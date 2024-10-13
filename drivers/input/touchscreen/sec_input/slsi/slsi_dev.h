/* drivers/input/sec_input/slsi/slsi_dev.h
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
  *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SLSI_TS_H__
#define __SLSI_TS_H__

#include <asm/unaligned.h>
#include <linux/completion.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH) || IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH_MODULE)
#include "../sec_secure_touch.h"
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#define SECURE_TOUCH_ENABLE	1
#define SECURE_TOUCH_DISABLE	0
#endif

#include "../sec_tclm_v2.h"
#if IS_ENABLED(CONFIG_INPUT_TOUCHSCREEN_TCLMV2)
#define TCLM_CONCEPT
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
#include "../sec_tsp_dumpkey.h"
extern struct tsp_dump_callbacks dump_callbacks;
#endif

#include "../sec_input.h"
#include "../sec_tsp_log.h"

#if IS_ENABLED(CONFIG_EXYNOS_DPU30)
#include <linux/panel_notify.h>
#endif

#define SLSI_TS_I2C_NAME		"slsi_ts"
#define SLSI_TS_DEVICE_NAME	"SLSI_TS"

#define TOUCH_PRINT_INFO_DWORK_TIME	30000	/* 30s */
#define TOUCH_RESET_DWORK_TIME		10
#define TOUCH_POWER_ON_DWORK_TIME	100

#define SLSI_TS_FW_BLK_SIZE_MAX		(512)
#define SLSI_TS_FW_BLK_SIZE_DEFAULT	(512)	// y761 & y771 ~
#define SLSI_TS_SELFTEST_REPORT_SIZE	80

#define SLSI_TS_FW_HEADER_SIGN		0x53494654
#define SLSI_TS_FW_CHUNK_SIGN		0x53434654

#define AMBIENT_CAL			0
#define OFFSET_CAL_SDC			1
#define OFFSET_CAL_SEC			2

#define SLSI_TS_NVM_OFFSET_FAC_RESULT			0
#define SLSI_TS_NVM_OFFSET_DISASSEMBLE_COUNT		1

/* TCLM_CONCEPT */
#define SLSI_TS_NVM_OFFSET_CAL_COUNT			2
#define SLSI_TS_NVM_OFFSET_TUNE_VERSION			3
#define SLSI_TS_NVM_OFFSET_TUNE_VERSION_LENGTH		2

#define SLSI_TS_NVM_OFFSET_CAL_POSITION			5
#define SLSI_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT		6
#define SLSI_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP		7
#define SLSI_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO		8
#define SLSI_TS_NVM_OFFSET_HISTORY_QUEUE_SIZE		20

#define SLSI_TS_NVM_OFFSET_CAL_FAIL_FLAG			(SLSI_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO + SLSI_TS_NVM_OFFSET_HISTORY_QUEUE_SIZE + 1)
#define SLSI_TS_NVM_OFFSET_CAL_FAIL_CNT			(SLSI_TS_NVM_OFFSET_CAL_FAIL_FLAG + 1)
#define SLSI_TS_NVM_OFFSET_LENGTH			(SLSI_TS_NVM_OFFSET_CAL_FAIL_CNT + 1)

#define SLSI_TS_NVM_LAST_BLOCK_OFFSET			SLSI_TS_NVM_OFFSET_LENGTH
#define SLSI_TS_NVM_TOTAL_OFFSET_LENGTH		(SLSI_TS_NVM_LAST_BLOCK_OFFSET + 1)

#define TOUCH_TX_CHANNEL_NUM			50
#define TOUCH_RX_CHANNEL_NUM			50

#define MAX_EVENT_COUNT				31

enum sec_fw_update_status {
	SEC_NOT_UPDATE = 0,
	SEC_NEED_FW_UPDATE,
	SEC_NEED_CALIBRATION_ONLY,
	SEC_NEED_FW_UPDATE_N_CALIBRATION,
};

/* factory test mode */
struct slsi_ts_test_mode {
	u8 type;
	short min;
	short max;
	bool allnode;
	bool frame_channel;
};

/* 48 byte */
struct slsi_ts_selftest_fail_hist {
	u32 tsp_signature;
	u32 tsp_fw_version;
	u8 fail_cnt1;
	u8 fail_cnt2;
	u16 selftest_exec_parm;
	u32 test_result;
	u8 fail_data[8];
	u32 fail_type:8;
	u32 reserved:24;
	u32 defective_data[5];
} __attribute__ ((packed));

/* 16 byte */
struct slsi_ts_gesture_status {
	u8 eid:2;
	u8 stype:4;
	u8 sf:2;
	u8 gesture_id;
	u8 gesture_data_1;
	u8 gesture_data_2;
	u8 gesture_data_3;
	u8 gesture_data_4;
	u8 reserved_1;
	u8 left_event_5_0:5;
	u8 reserved_2:3;
	u8 noise_level;
	u8 max_strength;
	u8 hover_id_num:4;
	u8 reserved10:4;
	u8 reserved11;
	u8 reserved12;
	u8 reserved13;
	u8 reserved14;
	u8 reserved15;
} __attribute__ ((packed));

/* 16 byte */
struct slsi_ts_event_status {
	u8 eid:2;
	u8 stype:4;
	u8 sf:2;
	u8 status_id;
	u8 status_data_1;
	u8 status_data_2;
	u8 status_data_3;
	u8 status_data_4;
	u8 status_data_5;
	u8 left_event_5_0:5;
	u8 reserved_2:3;
	u8 noise_level;
	u8 max_strength;
	u8 hover_id_num:4;
	u8 reserved10:4;
	u8 reserved11;
	u8 reserved12;
	u8 reserved13;
	u8 reserved14;
	u8 reserved15;
} __attribute__ ((packed));

/* 16 byte */
struct slsi_ts_event_coordinate {
	u8 eid:2;
	u8 tid:4;
	u8 tchsta:2;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 major;
	u8 minor;
	u8 z:6;
	u8 ttype_3_2:2;
//	u8 left_event:6;
	u8 left_event:5;
	u8 max_energy_flag:1;
	u8 ttype_1_0:2;
	u8 noise_level;
	u8 max_strength;
	u8 hover_id_num:4;
	u8 noise_status:2;
	u8 reserved10:2;
	u8 reserved11;
	u8 reserved12;
	u8 reserved13;
	u8 reserved14;
	u8 reserved15;
} __attribute__ ((packed));

struct slsi_ts_data {
	u32 crc_addr;
	u32 fw_addr;
	u32 para_addr;
	u32 flash_page_size;
	u8 boot_ver[3];

	struct device *dev;
	struct i2c_client *client;
	struct sec_ts_plat_data *plat_data;

	struct notifier_block slsi_input_nb;

	int tx_count;
	int rx_count;

	u8 gesture_status[6];
	u8 cal_status;
	struct mutex device_mutex;
	struct mutex i2c_mutex;
	struct mutex eventlock;
	struct mutex modechange;
	struct mutex sponge_mutex;
	struct mutex fn_mutex;
	struct mutex proc_mutex;

	u8 *fw_data;
	size_t fw_size;

	int nv;

	struct delayed_work work_read_info;
	struct delayed_work work_print_info;

	struct delayed_work reset_work;
	volatile bool reset_is_on_going;

	struct delayed_work work_read_functions;
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	atomic_t secure_enabled;
	atomic_t secure_pending_irqs;
	struct completion secure_powerdown;
	struct completion secure_interrupt;
#endif
	struct sec_cmd_data sec;
	short *pFrame;

	bool probe_done;
	bool info_work_done;

	struct delayed_work check_rawdata;
	u8 tsp_dump_lock;

	struct sec_tclm_data *tdata;
	bool is_cal_done;

	bool factory_level;
	u8 factory_position;

	u16 proximity_thd;
	bool proximity_jig_mode; 
	u8 sip_mode;

	/* average value for each channel */
	short ambient_tx[TOUCH_TX_CHANNEL_NUM];
	short ambient_rx[TOUCH_RX_CHANNEL_NUM];

	/* max - min value for each channel */
	short ambient_tx_delta[TOUCH_TX_CHANNEL_NUM];
	short ambient_rx_delta[TOUCH_RX_CHANNEL_NUM];

	/* for factory - factory_cmd_result_all() */
	short cm_raw_set_avg_min;
	short cm_raw_set_avg_max;
	short cm_raw_set_p2p;
	
	short self_raw_set_avg_tx_min;
	short self_raw_set_avg_tx_max;
	short self_raw_set_avg_rx_min;
	short self_raw_set_avg_rx_max;
	short self_raw_set_p2p_tx_diff;
	short self_raw_set_p2p_rx_diff;

	short cm_raw_key_p2p_min;
	short cm_raw_key_p2p_max;
	short cm_raw_key_p2p_diff;
	short cm_raw_key_p2p_diff_data[2][3];	/* key : max support key is 3 */
	short cm_raw_set_p2p_gap_y;
	short cm_raw_set_p2p_gap_y_result;	/* mis_cal pass/fail */
	short gap_max_spec;

	int debug_flag;
	int fix_active_mode;

	int proc_cmoffset_size;
	int proc_cmoffset_all_size;
	char *cmoffset_sdc_proc;
	char *cmoffset_main_proc;
	char *miscal_proc;
	char *cmoffset_all_proc;

	int proc_fail_hist_size;
	int proc_fail_hist_all_size;
	char *fail_hist_sdc_proc;
	char *fail_hist_sub_proc;
	char *fail_hist_main_proc;
	char *fail_hist_all_proc;

	bool sponge_inf_dump;
	u8 sponge_dump_format;
	u8 sponge_dump_event;
	u8 sponge_dump_border_msb;
	u8 sponge_dump_border_lsb;
	bool sponge_dump_delayed_flag;
	u8 sponge_dump_delayed_area;
	u16 sponge_dump_border;

	int (*slsi_ts_i2c_write)(struct slsi_ts_data *ts, u8 reg, u8 *data, int len);
	int (*slsi_ts_i2c_read)(struct slsi_ts_data *ts, u8 reg, u8 *data, int len);
	int (*slsi_ts_i2c_write_burst)(struct slsi_ts_data *ts, u8 *data, int len);
	int (*slsi_ts_i2c_read_bulk)(struct slsi_ts_data *ts, u8 *data, int len);
	int (*slsi_ts_read_sponge)(struct slsi_ts_data *ts, u8 *data, int len);
	int (*slsi_ts_write_sponge)(struct slsi_ts_data *ts, u8 *data, int len);
};

typedef struct {
	u32 signature;			/* signature */
	u32 version;			/* version */
	u32 totalsize;			/* total size */
	u32 checksum;			/* checksum */
	u32 img_ver;			/* image file version */
	u32 img_date;			/* image file date */
	u32 img_description;		/* image file description */
	u32 fw_ver;			/* firmware version */
	u32 fw_date;			/* firmware date */
	u32 fw_description;		/* firmware description */
	u32 para_ver;			/* parameter version */
	u32 para_date;			/* parameter date */
	u32 para_description;		/* parameter description */
	u32 num_chunk;			/* number of chunk */
	u32 reserved1;
	u32 reserved2;
} fw_header;

typedef struct {
	u32 signature;
	u32 addr;
	u32 size;
	u32 reserved;
} fw_chunk;

extern struct device *ptsp;

int slsi_ts_probe(struct i2c_client *client, const struct i2c_device_id *id);
int slsi_ts_remove(struct i2c_client *client);
void slsi_ts_shutdown(struct i2c_client *client);

int slsi_ts_stop_device(void *data);
int slsi_ts_start_device(void *data);
void slsi_ts_reinit(void *data);
int slsi_ts_set_lowpowermode(void *data, u8 mode);

int slsi_ts_set_external_noise_mode(struct slsi_ts_data *ts, u8 mode);
int slsi_ts_firmware_update_on_probe(struct slsi_ts_data *ts, bool force_update);
int slsi_ts_glove_mode_enables(struct slsi_ts_data *ts, int mode);
int slsi_ts_wait_for_ready(struct slsi_ts_data *ts, u8 reg, u8 *data, int len, int delay);
int slsi_ts_fn_init(struct slsi_ts_data *ts);
int slsi_ts_read_calibration_report(struct slsi_ts_data *ts);
int slsi_ts_fix_tmode(struct slsi_ts_data *ts, u8 mode, u8 state);
int slsi_ts_release_tmode(struct slsi_ts_data *ts);
void slsi_ts_get_custom_library(struct slsi_ts_data *ts);
int slsi_ts_set_custom_library(struct slsi_ts_data *ts);
int slsi_ts_set_aod_rect(struct slsi_ts_data *ts);
int slsi_ts_set_fod_rect(struct slsi_ts_data *ts);
int slsi_ts_set_temperature(struct device *dev, u8 temperature_data);
int slsi_ts_set_touchable_area(struct slsi_ts_data *ts);
int slsi_ts_ear_detect_enable(struct slsi_ts_data *ts, u8 enable);
int slsi_ts_set_charger_mode(struct slsi_ts_data *ts);

int slsi_ts_set_touch_function(struct slsi_ts_data *ts);

void slsi_ts_locked_release_all_finger(struct slsi_ts_data *ts);
void slsi_ts_unlocked_release_all_finger(struct slsi_ts_data *ts);

void slsi_ts_fn_remove(struct slsi_ts_data *ts);
void slsi_ts_run_rawdata_all(struct slsi_ts_data *ts, bool full_read);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
int slsi_ts_raw_device_init(struct slsi_ts_data *ts);
#endif

void slsi_ts_get_touch_function(struct work_struct *work);
void slsi_ts_init_proc(struct slsi_ts_data *ts);

void slsi_set_grip_data_to_ic(struct device *dev, u8 flag);

void slsi_ts_ioctl_init(struct slsi_ts_data *ts);
void slsi_ts_ioctl_remove(struct slsi_ts_data *ts);

void set_tsp_nvm_data_clear(struct slsi_ts_data *ts, u8 offset);
int get_tsp_nvm_data(struct slsi_ts_data *ts, u8 offset);

int slsi_ts_set_press_property(struct slsi_ts_data *ts);
int slsi_ts_set_fod_rect(struct slsi_ts_data *ts);

int slsi_ts_i2c_write(struct slsi_ts_data *ts, u8 reg, u8 *data, int len);
int slsi_ts_i2c_read(struct slsi_ts_data *ts, u8 reg, u8 *data, int len);
int slsi_ts_i2c_write_burst(struct slsi_ts_data *ts, u8 *data, int len);
int slsi_ts_i2c_read_bulk(struct slsi_ts_data *ts, u8 *data, int len);
int slsi_ts_read_from_sponge(struct slsi_ts_data *ts, u8 *data, int len);
int slsi_ts_write_to_sponge(struct slsi_ts_data *ts, u8 *data, int len);
void slsi_ts_set_utc_sponge(struct slsi_ts_data *ts);

int slsi_ts_fix_tmode(struct slsi_ts_data *ts, u8 mode, u8 state);
int slsi_ts_p2p_tmode(struct slsi_ts_data *ts);
int execute_p2ptest(struct slsi_ts_data *ts);
int slsi_ts_release_tmode(struct slsi_ts_data *ts);
int slsi_ts_set_press_property(struct slsi_ts_data *ts);

void slsi_ts_reset_work(struct work_struct *work);
void slsi_ts_read_info_work(struct work_struct *work);
void slsi_ts_print_info_work(struct work_struct *work);

int slsi_ts_input_open(struct input_dev *dev);
void slsi_ts_input_close(struct input_dev *dev);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
void slsi_ts_check_rawdata(struct work_struct *work);
void slsi_ts_dump_tsp_log(struct device *dev);
void slsi_ts_sponge_dump_flush(struct slsi_ts_data *ts, int dump_area);
#endif

irqreturn_t slsi_ts_irq_thread(int irq, void *ptr);

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
irqreturn_t secure_filter_interrupt(struct slsi_ts_data *ts);
ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *addr, const char *buf, size_t count);
ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t secure_ownership_show(struct device *dev,
		struct device_attribute *attr, char *buf);

int secure_touch_init(struct slsi_ts_data *ts);
void secure_touch_stop(struct slsi_ts_data *ts, bool stop);
#endif

ssize_t get_miscal_dump(struct slsi_ts_data *ts, char *buf);
ssize_t get_cmoffset_dump_all(struct slsi_ts_data *ts, char *buf, u8 position);
ssize_t get_selftest_fail_hist_dump_all(struct slsi_ts_data *ts, char *buf, u8 position);
int slsi_ts_write_factory_level(struct slsi_ts_data *ts, u8 pos);

extern int slsi_ts_set_cover_type(struct slsi_ts_data *ts, bool enable);
extern int slsi_ts_firmware_update_on_hidden_menu(struct slsi_ts_data *ts, int update_type);

int slsi_ts_set_scan_mode(struct slsi_ts_data *ts, int mode);

#endif
