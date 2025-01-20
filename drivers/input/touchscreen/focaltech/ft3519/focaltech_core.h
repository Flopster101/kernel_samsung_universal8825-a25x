/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
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
/*****************************************************************************
*
* File Name: focaltech_core.h

* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

#ifndef __LINUX_FOCALTECH_CORE_H__
#define __LINUX_FOCALTECH_CORE_H__
/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include <linux/pm_wakeup.h>
#include <linux/bitmap.h>
#include "focaltech_reg.h"
#include "focaltech_common.h"

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#include <linux/notifier.h>
#include <linux/vbus_notifier.h>
#endif

extern struct device *ptsp;

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_MAX_POINTS_SUPPORT              10 /* constant value, can't be changed */
#define FTS_MAX_KEYS                        4
#define FTS_KEY_DIM                         10
#define FTS_ONE_TCH_LEN                     6
#define FTS_TOUCH_DATA_LEN  (FTS_MAX_POINTS_SUPPORT * FTS_ONE_TCH_LEN + 3)

#define FTS_GESTURE_POINTS_MAX              6
#define FTS_GESTURE_DATA_LEN               (FTS_GESTURE_POINTS_MAX * 4 + 4)

#define FTS_MAX_ID                          0x0A
#define FTS_TOUCH_X_H_POS                   3
#define FTS_TOUCH_X_L_POS                   4
#define FTS_TOUCH_Y_H_POS                   5
#define FTS_TOUCH_Y_L_POS                   6
#define FTS_TOUCH_PRE_POS                   7
#define FTS_TOUCH_AREA_POS                  8
#define FTS_TOUCH_POINT_NUM                 2
#define FTS_TOUCH_EVENT_POS                 3
#define FTS_TOUCH_ID_POS                    5
#define FTS_COORDS_ARR_SIZE                 4
#define FTS_X_MIN_DISPLAY_DEFAULT           0
#define FTS_Y_MIN_DISPLAY_DEFAULT           0
#define FTS_X_MAX_DISPLAY_DEFAULT           720
#define FTS_Y_MAX_DISPLAY_DEFAULT           1280

#define FTS_TOUCH_DOWN                      0
#define FTS_TOUCH_UP                        1
#define FTS_TOUCH_CONTACT                   2
#define EVENT_DOWN(flag)			((flag == FTS_TOUCH_DOWN) || (flag == FTS_TOUCH_CONTACT))
#define EVENT_UP(flag)				(flag == FTS_TOUCH_UP)
#define EVENT_NO_DOWN(data)                 (!data->point_num)

#define FTS_MAX_COMPATIBLE_TYPE             4
#define FTS_MAX_COMMMAND_LENGTH             16

#define TOUCH_PRINT_INFO_DWORK_TIME		30000 /* 30 secs */

#define FTS_TOUCH_COUNT(x)			hweight_long(*x & BITMAP_LAST_WORD_MASK(FTS_MAX_POINTS_SUPPORT))

/*****************************************************************************
*  Alternative mode (When something goes wrong, the modules may be able to solve the problem.)
*****************************************************************************/
/*
 * For commnication error in PM(deep sleep) state
 */
#define FTS_PATCH_COMERR_PM			1
#define FTS_WAKELOCK_TIME			500
#define FTS_TIMEOUT_COMERR_PM                   700

#define FTS_HIGH_REPORT                         0
#define FTS_SIZE_DEFAULT                        15


/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
struct ftxxxx_proc {
	struct proc_dir_entry *proc_entry;
	u8 opmode;
	u8 cmd_len;
	u8 cmd[FTS_MAX_COMMMAND_LENGTH];
};

struct fts_ts_platform_data {
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	bool have_key;
	u32 key_number;
	u32 keys[FTS_MAX_KEYS];
	u32 key_y_coords[FTS_MAX_KEYS];
	u32 key_x_coords[FTS_MAX_KEYS];
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 area_indicator;
	u32 area_navigation;
	u32 area_edge;
	u32 max_touch_number;
	const char *firmware_name;
	bool enable_settings_aot;
	bool enable_sysinput_enabled;
	bool support_dex;
	bool enable_vbus_notifier;
};

struct ts_event {
	int x;      /*x coordinate */
	int y;      /*y coordinate */
	int p_x;
	int p_y;
	int p;      /* pressure */
	int flag;   /* touch event flag: 0 -- down; 1-- up; 2 -- contact */
	int id;     /*touch ID */
	int area;
	int area_minor;
	int palm;
	int mcount;
};

struct pen_event {
	int inrange;
	int tip;
	int x;      /*x coordinate */
	int y;      /*y coordinate */
	int p;      /* pressure */
	int flag;   /* touch event flag: 0 -- down; 1-- up; 2 -- contact */
	int id;     /*touch ID */
	int tilt_x;
	int tilt_y;
	int tool_type;
};

struct fts_fw_ver {
	u8 ic_name;
	u8 project_name;
	u8 module_id;
	u8 fw_ver;
};

struct fts_ts_data {
	struct i2c_client *client;
	struct spi_device *spi;
	struct device *dev;
	struct input_dev *input_dev;
	struct input_dev *input_dev_pad;
	struct input_dev *input_dev_proximity;
	struct input_dev *pen_dev;
	struct sec_ts_plat_data *pdata;
	struct ts_ic_info ic_info;
	struct sec_cmd_data sec;
	struct sec_input_grip_data grip_data;
	struct workqueue_struct *ts_workqueue;
	struct delayed_work esdcheck_work;
	struct delayed_work prc_work;
	struct work_struct resume_work;
	struct ftxxxx_proc proc;
	struct fts_fw_ver ic_fw_ver;
	struct fts_fw_ver bin_fw_ver;
	struct mutex report_mutex;
	struct mutex bus_lock;
	struct mutex device_lock;
	struct mutex irq_lock;
	unsigned long intr_jiffies;
	int irq;
	int log_level;
	int fw_is_running;      /* confirm fw is running when using spi:default 0 */
	int dummy_byte;
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	struct wakeup_source *wake_lock;
	struct completion pm_completion;
	bool pm_suspend;
#endif
	bool suspended;
	bool fw_loading;
	bool irq_disabled;
	bool power_disabled;
	bool glove_mode;
	bool cover_mode;
	bool charger_mode;
	u8   gesture_mode;      /*bit_0:single tap; bit_1: double click; bit_2:spay; bit_3:fod*/
	bool aot_enable;
	bool spay_enable;
	bool prc_mode;
	u8 singletap_enable;
	u8 power_mode;
	u8 refresh_rate;
	u8 pocket_state;
	u8 fod_state;
	int fod_mode;
	struct pen_event pevent;
	/* multi-touch */
	struct ts_event *events;
	unsigned long palm_flag;
	u8 *bus_tx_buf;
	u8 *bus_rx_buf;
	int bus_type;
	u8 *point_buf;
	int pnt_buf_size;
	unsigned long touchs;
	int key_state;
	int touch_point;
	int point_num;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
#if FTS_PINCTRL_EN
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_active;
	struct pinctrl_state *pins_suspend;
	struct pinctrl_state *pins_release;
#endif

	struct delayed_work print_info_work;
	u32 print_info_cnt_open;
	u32 print_info_cnt_release;

	bool have_key;
	u32 key_number;
	u32 keys[FTS_MAX_KEYS];
	u32 key_y_coords[FTS_MAX_KEYS];
	u32 key_x_coords[FTS_MAX_KEYS];
	u32 max_touch_number;

	int tx_num;
	int rx_num;
	int *pFrame;
	int pFrame_size;
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	struct notifier_block vbus_nb;
	bool ta_status;
#endif

	u8 hover_event;
};

enum _FTS_BUS_TYPE {
	BUS_TYPE_NONE,
	BUS_TYPE_I2C,
	BUS_TYPE_SPI,
	BUS_TYPE_SPI_V2,
};

enum {
	TSP_BUILT_IN = 0,
	TSP_SDCARD,
	TSP_SIGNED_SDCARD,
	TSP_SPU,
	TSP_VERIFICATION,
};

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
extern struct fts_ts_data *fts_data;

/* communication interface */
int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen);
int fts_read_reg(u8 addr, u8 *value);
int fts_write(u8 *writebuf, u32 writelen);
int fts_write_reg(u8 addr, u8 value);
void fts_hid2std(void);
int fts_bus_init(struct fts_ts_data *ts_data);
int fts_bus_exit(struct fts_ts_data *ts_data);

int fts_ts_suspend(struct fts_ts_data *ts_data);
int fts_ts_resume(struct fts_ts_data *ts_data);

/* Gesture functions */
int fts_gesture_init(struct fts_ts_data *ts_data);
int fts_gesture_exit(struct fts_ts_data *ts_data);
void fts_gesture_recovery(struct fts_ts_data *ts_data);
int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *data);
int fts_gesture_suspend(struct fts_ts_data *ts_data);
int fts_gesture_resume(struct fts_ts_data *ts_data);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
/* Apk and functions */
int fts_create_apk_debug_channel(struct fts_ts_data *);
void fts_release_apk_debug_channel(struct fts_ts_data *);

/* ADB functions */
int fts_create_sysfs(struct fts_ts_data *ts_data);
int fts_remove_sysfs(struct fts_ts_data *ts_data);
#endif

int fts_sec_cmd_init(struct fts_ts_data *ts_data);
void fts_sec_cmd_exit(struct fts_ts_data *ts_data);

/* ESD */
#if FTS_ESDCHECK_EN
int fts_esdcheck_init(struct fts_ts_data *ts_data);
int fts_esdcheck_exit(struct fts_ts_data *ts_data);
int fts_esdcheck_switch(bool enable);
int fts_esdcheck_proc_busy(bool proc_debug);
int fts_esdcheck_set_intr(bool intr);
int fts_esdcheck_suspend(void);
int fts_esdcheck_resume(void);
#endif

/* Point Report Check*/
#if FTS_POINT_REPORT_CHECK_EN
int fts_point_report_check_init(struct fts_ts_data *ts_data);
int fts_point_report_check_exit(struct fts_ts_data *ts_data);
void fts_prc_queue_work(struct fts_ts_data *ts_data);
#endif

/* FW upgrade */
int fts_fwupg_init(struct fts_ts_data *ts_data);
int fts_fwupg_exit(struct fts_ts_data *ts_data);
int fts_upgrade_bin(char *fw_name, bool force);
int fts_enter_test_environment(bool test_state);
int fts_get_fw_ver(struct fts_ts_data *ts_data);
int fts_fwupg_func(int update_type, bool force);

int fts_write_cnt_info(u16 cnt);
int fts_read_cnt_info(u16 *cnt);
int fts_flash_read(u32 addr, u8 *buf, u32 len);

/* Other */
int fts_reset_proc(int hdelayms);
int fts_check_cid(struct fts_ts_data *ts_data, u8 id_h);
int fts_wait_tp_to_valid(void);
void fts_release_all_finger(void);
void fts_tp_state_recovery(struct fts_ts_data *ts_data);
int fts_ex_mode_init(struct fts_ts_data *ts_data);
int fts_ex_mode_exit(struct fts_ts_data *ts_data);
int fts_ex_mode_recovery(struct fts_ts_data *ts_data);
void fts_set_edge_handler(struct fts_ts_data *ts);
void fts_set_scan_off(bool scan_off);

void fts_irq_disable(void);
void fts_irq_enable(void);
int fts_ts_input_open(struct input_dev *dev);
void fts_ts_input_close(struct input_dev *dev);

int fts_read_fod_data(struct fts_ts_data *ts_data);
int fts_set_fod_rect(struct fts_ts_data *ts_data);
int fts_set_refresh_rate(struct fts_ts_data *ts_data);

#endif /* __LINUX_FOCALTECH_CORE_H__ */
