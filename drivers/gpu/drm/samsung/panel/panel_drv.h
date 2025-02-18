/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Author: Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_DRV_H__
#define __PANEL_DRV_H__

#include <linux/device.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <dt-bindings/gpio/gpio.h>

#include "disp_err.h"

#include "panel.h"
#include "panel_power_ctrl.h"

#include "panel_gpio.h"
#include "panel_regulator.h"

#include "panel_hdr_info.h"
#include "panel_mutex.h"
#include "panel_firmware.h"

#ifdef CONFIG_USDM_MDNIE
#include "mdnie.h"
#endif

#ifdef CONFIG_USDM_PANEL_COPR
#include "copr.h"
#endif

#ifdef CONFIG_USDM_PANEL_DDI_FLASH
#include "panel_poc.h"
#endif

#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY
#include "./aod/aod_drv.h"
#endif

#ifdef CONFIG_USDM_PANEL_MAFPC
#include "./mafpc/mafpc_drv.h"
#endif

#ifdef CONFIG_USDM_POC_SPI
#include "panel_spi.h"
#endif

#ifdef CONFIG_USDM_BLIC_I2C
#include "panel_i2c.h"
#endif

#ifdef CONFIG_USDM_PANEL_BLIC
#include "panel_blic.h"
#endif

#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
#include "panel_freq_hop.h"
#endif

#if defined(CONFIG_USDM_SDP_ADAPTIVE_MIPI)
#include "sdp_adaptive_mipi.h"
#elif defined(CONFIG_USDM_ADAPTIVE_MIPI)
#include "adaptive_mipi.h"
#endif

#define PANEL_DEV_NAME ("panel")
#define MAX_PANEL_DEV_NAME_SIZE (32)
#define MAX_PANEL_WORK_NAME_SIZE (64)

#define call_panel_drv_func(q, _f, args...)              \
    (((q) && (q)->funcs && (q)->funcs->_f) ? ((q)->funcs->_f(q, ##args)) : 0)

// #define CONFIG_USDM_PANEL_ERRFG_RECOVERY

void clear_pending_bit(int irq);

enum {
	PANEL_REGULATOR_DDI_VCI = 0,
	PANEL_REGULATOR_DDI_VDD3,
	PANEL_REGULATOR_DDR_VDDR,
	PANEL_REGULATOR_SSD,
#ifdef CONFIG_EXYNOS_DPU30_DUAL
	PANEL_SUB_REGULATOR_DDI_VCI,
	PANEL_SUB_REGULATOR_DDI_VDD3,
	PANEL_SUB_REGULATOR_DDR_VDDR,
	PANEL_SUB_REGULATOR_SSD,
#endif
	PANEL_REGULATOR_MAX
};

enum panel_gpio_lists {
	PANEL_GPIO_RESET = 0,
	PANEL_GPIO_DISP_DET,
	PANEL_GPIO_PCD,
	PANEL_GPIO_ERR_FG,
	PANEL_GPIO_CONN_DET,
	PANEL_GPIO_DISP_TE,
	PANEL_GPIO_MAX,
};

#define PANEL_POLL_IRQ_INTERVAL	(1000)	/* 1000us */
#define PANEL_POLL_IRQ_TRY	(20)

enum panel_irq_lists {
	PANEL_IRQ_RESET =			(BIT_MASK(PANEL_GPIO_RESET)),
	PANEL_IRQ_DISP_DET =			(BIT_MASK(PANEL_GPIO_DISP_DET)),
	PANEL_IRQ_PCD =			(BIT_MASK(PANEL_GPIO_PCD)),
	PANEL_IRQ_ERR_FG =			(BIT_MASK(PANEL_GPIO_ERR_FG)),
	PANEL_IRQ_CONN_DET =			(BIT_MASK(PANEL_GPIO_CONN_DET)),
	PANEL_IRQ_DISP_TE =			(BIT_MASK(PANEL_GPIO_DISP_TE)),
	PANEL_IRQ_ALL_WITHOUT_CONN_DET =	(0xFFFFFFFF & ~PANEL_IRQ_CONN_DET),
	PANEL_IRQ_ALL =			(0xFFFFFFFF),
};

#define PANEL_GPIO_NAME_RESET ("disp-reset")
#define PANEL_GPIO_NAME_DISP_DET ("disp-det")
#define PANEL_GPIO_NAME_PCD ("pcd")
#define PANEL_GPIO_NAME_ERR_FG ("err-fg")
#define PANEL_GPIO_NAME_CONN_DET ("conn-det")
#define PANEL_GPIO_NAME_DISP_TE ("disp-te")

#define PANEL_REGULATOR_NAME_DDI_VCI ("ddi-vci")
#define PANEL_REGULATOR_NAME_DDI_VDD3 ("ddi-vdd3")
#define PANEL_REGULATOR_NAME_DDR_VDDR ("ddr-vddr")
#define PANEL_REGULATOR_NAME_DDI_BLIC ("ddi-blic")
#define PANEL_REGULATOR_NAME_SSD ("short-detect")
#define PANEL_REGULATOR_NAME_RESET ("ddi-reset")

#ifdef CONFIG_EXYNOS_DPU30_DUAL
#define PANEL_SUB_REGULATOR_NAME_DDI_VCI ("ddi-sub-vci")
#define PANEL_SUB_REGULATOR_NAME_DDI_VDD3 ("ddi-sub-vdd3")
#define PANEL_SUB_REGULATOR_NAME_DDR_VDDR ("ddr-sub-vddr")
#define PANEL_SUB_REGULATOR_NAME_SSD ("short-sub-detect")
#endif

#define PANEL_PROPERTY_PANEL_STATE ("panel_state")
#define PANEL_PROPERTY_PANEL_ID_1 ("panel_id_1")
#define PANEL_PROPERTY_PANEL_ID_2 ("panel_id_2")
#define PANEL_PROPERTY_PANEL_ID_3 ("panel_id_3")
#define PANEL_PROPERTY_WAIT_TX_DONE ("wait_tx_done")
#define PANEL_PROPERTY_SEPARATE_TX ("separate_tx")
#define PANEL_PROPERTY_PANEL_REFRESH_RATE ("panel_refresh_rate")
#define PANEL_PROPERTY_PANEL_REFRESH_MODE ("panel_refresh_mode")
#define PANEL_PROPERTY_PREV_PANEL_REFRESH_RATE ("prev_panel_refresh_rate")
#define PANEL_PROPERTY_PREV_PANEL_REFRESH_MODE ("prev_panel_refresh_mode")
#define PANEL_PROPERTY_RESOLUTION_CHANGED ("resolution_changed")
#define PANEL_PROPERTY_LFD_FIX ("lfd_fix")
#define PANEL_PROPERTY_HMD_LFD_FIX ("hmd_lfd_fix")
#define PANEL_PROPERTY_LPM_LFD_FIX ("lpm_lfd_fix")
#define PANEL_PROPERTY_DSI_FREQ ("dsi_freq")
#define PANEL_PROPERTY_OSC_FREQ ("osc_freq")
#define PANEL_PROPERTY_IS_FACTORY_MODE ("is_factory_mode")
#define PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE ("brightdot_test_enable")
#define PANEL_PROPERTY_VGLHIGHDOT ("vglhighdot")
#define PANEL_PROPERTY_XTALK_MODE ("xtalk_mode")
#define PANEL_PROPERTY_IRC_MODE ("irc_mode")
#define PANEL_PROPERTY_DIA_MODE ("dia_mode")
#define PANEL_PROPERTY_FD_ENABLED ("fd_enabled")

enum {
	SEPARATE_TX_OFF,    /* TX CMD by using panel drv cmdq */
	SEPARATE_TX_ON, /* TX CMD one by one */
};

enum {
	WAIT_TX_DONE_AUTO,  /* Wait TX DONE every end of cmdq set */
	WAIT_TX_DONE_MANUAL_OFF, /* DO NOT wait for TX DONE */
	WAIT_TX_DONE_MANUAL_ON, /* Wait TX DONE every each cmd */
	MAX_WAIT_TX_DONE_FLAG,
};

struct cmd_set {
	u8 cmd_id;
	u32 offset;
	const u8 *buf;
	int size;
};

enum {
	CLOCK_ID_DSI,
	CLOCK_ID_OSC,
};

struct panel_clock_info {
	u32 clock_id;
	u32 clock_rate;
};

#define DSIM_OPTION_WAIT_TX_DONE	(1U << 0)
#define DSIM_OPTION_POINT_GPARA		(1U << 1)
#define DSIM_OPTION_2BYTE_GPARA		(1U << 2)

enum ctrl_interface_state {
	CTRL_INTERFACE_STATE_INACTIVE,
	CTRL_INTERFACE_STATE_ACTIVE,
	MAX_CTRL_INTERFACE_STATE,
};

#define WAKE_TIMEOUT_MSEC (100)
#define EVASION_DISP_DET_DELAY_MSEC (600)

struct panel_adapter_funcs {
	int (*read)(void *ctx, u8 addr, u32 ofs, u8 *buf, int size, u32 option);
	int (*write)(void *ctx, u8 cmd_id, const u8 *cmd, u32 ofs, int size, u32 option);
	int (*write_table)(void *ctx, const struct cmd_set *cmd, int size, u32 option);
	int (*sr_write)(void *ctx, u8 cmd_id, const u8 *cmd, u32 ofs, int size, u32 option);
	int (*get_state)(void *ctx);
	int (*parse_dt)(void *ctx, struct device_node *node);
	int (*wait_for_vsync)(void *ctx, u32 timeout);
	int (*wait_for_fsync)(void *ctx, u32 timeout);
	int (*set_bypass)(void *ctx, bool on);
	int (*get_bypass)(void *ctx);
	int (*set_commit_retry)(void *ctx, bool on);
	int (*wake_lock)(void *ctx, unsigned long timeout);
	int (*wake_unlock)(void *ctx);
	int (*flush_image)(void *ctx);
	int (*set_lpdt)(void *ctx, bool on);
	int (*dpu_register_dump)(void *ctx);
	int (*dpu_event_log_print)(void *ctx);
	int (*emergency_off)(void *ctx);
#if defined(CONFIG_PANEL_FREQ_HOP) ||\
	defined(CONFIG_USDM_PANEL_FREQ_HOP) ||\
	defined(CONFIG_USDM_SDP_ADAPTIVE_MIPI) ||\
	defined(CONFIG_USDM_ADAPTIVE_MIPI)
	int (*set_freq_hop)(void *ctx, struct freq_hop_param *param);
#endif
	int (*trigger_recovery)(void *ctx);
};

#define call_panel_adapter_func(_p, _func, args...) \
	(((_p) && (_p)->adapter.ctx && (_p)->adapter.funcs && \
	  (_p)->adapter.funcs->_func) ? \
	  (_p)->adapter.funcs->_func(((_p)->adapter.ctx), ##args) : -EINVAL)

#define get_panel_adapter_fifo_size(_p) \
	((_p) ? ((_p)->adapter.fifo_size) : 0)

struct panel_adapter {
	void *ctx;
	unsigned int fifo_size;
	struct panel_adapter_funcs *funcs;
};

struct panel_drv_funcs {
	int (*register_cb)(struct panel_device *, int, void *);
	int (*register_error_cb)(struct panel_device *, void *);
	int (*get_panel_state)(struct panel_device *, void *);
	int (*attach_adapter)(struct panel_device *, void *);

	/* panel control operation */
	int (*probe)(struct panel_device *);
	int (*sleep_in)(struct panel_device *);
	int (*sleep_out)(struct panel_device *);
	int (*display_on)(struct panel_device *);
	int (*display_off)(struct panel_device *);
	int (*power_on)(struct panel_device *);
	int (*power_off)(struct panel_device *);
	int (*debug_dump)(struct panel_device *);
	int (*doze)(struct panel_device *);
	int (*doze_suspend)(struct panel_device *);
	int (*set_display_mode)(struct panel_device *, void *);
	int (*get_display_mode)(struct panel_device *, void *);
#if IS_ENABLED(CONFIG_USDM_PANEL_BIG_LOCK)
	int (*lock)(struct panel_device *);
	int (*unlock)(struct panel_device *);
#endif
	int (*reset_lp11)(struct panel_device *);
	int (*reset_disable)(struct panel_device *);

	/* display controller event operation */
	int (*vsync)(struct panel_device *, void *);
	int (*frame_done)(struct panel_device *, void *);
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
	int (*set_mask_layer)(struct panel_device *, void *);
#endif
	int (*req_set_clock)(struct panel_device *, void *);
	int (*get_ddi_props)(struct panel_device *, void *);
	int (*get_rcd_info)(struct panel_device *, void *);
	int (*first_frame)(struct panel_device *);
	int (*set_brightness)(struct panel_device *, unsigned int brightness_level);
	int (*set_uevent_recovery_state)(struct panel_device *, void *);
};

int panel_drv_attach_adapter_ioctl(struct panel_device *panel, void *arg);
int panel_drv_get_panel_state_ioctl(struct panel_device *panel, void *arg);
int panel_drv_panel_probe_ioctl(struct panel_device *panel, void *arg);
int panel_drv_set_power_ioctl(struct panel_device *panel, void *arg);
int panel_drv_sleep_in_ioctl(struct panel_device *panel, void *arg);
int panel_drv_sleep_out_ioctl(struct panel_device *panel, void *arg);
int panel_drv_disp_on_ioctl(struct panel_device *panel, void *arg);
int panel_drv_panel_dump_ioctl(struct panel_device *panel, void *arg);
int panel_drv_evt_frame_done_ioctl(struct panel_device *panel, void *arg);
int panel_drv_evt_vsync_ioctl(struct panel_device *panel, void *arg);
int panel_drv_doze_ioctl(struct panel_device *panel, void *arg);
int panel_drv_doze_suspend_ioctl(struct panel_device *panel, void *arg);
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
int panel_drv_get_display_mode_ioctl(struct panel_device *panel, void *arg);
int panel_drv_set_display_mode_ioctl(struct panel_device *panel, void *arg);
int panel_drv_reg_display_mode_cb_ioctl(struct panel_device *panel, void *arg);
#endif
int panel_drv_reg_reset_cb_ioctl(struct panel_device *panel, void *arg);
int panel_drv_reg_vrr_cb_ioctl(struct panel_device *panel, void *arg);
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
int panel_drv_set_mask_layer_ioctl(struct panel_device *panel, void *arg);
#endif
bool panel_is_gpio_valid(struct panel_device *panel, enum panel_gpio_lists panel_gpio_id);
int panel_enable_gpio_irq(struct panel_device *panel, enum panel_gpio_lists panel_gpio_id);
int panel_disable_gpio_irq(struct panel_device *panel, enum panel_gpio_lists panel_gpio_id);
int panel_poll_gpio(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id, bool expect_level,
		unsigned long sleep_us, unsigned long timeout_us);
int panel_enable_disp_det_irq(struct panel_device *panel);
int panel_disable_disp_det_irq(struct panel_device *panel);
int panel_enable_irq(struct panel_device *panel, u32 pins);
int panel_disable_irq(struct panel_device *panel, u32 pins);

#define PANEL_INIT_KERNEL		0
#define PANEL_INIT_BOOT			1

#define PANEL_DISP_DET_HIGH		1
#define PANEL_DISP_DET_LOW		0

enum {
	PANEL_STATE_NOK = 0,
	PANEL_STATE_OK = 1,
};

enum {
	PANEL_EL_OFF = 0,
	PANEL_EL_ON = 1,
};

enum {
	PANEL_UB_CONNECTED = 0,
	PANEL_UB_DISCONNECTED = 1,
};

enum panel_bypass {
	PANEL_BYPASS_OFF = 0,
	PANEL_BYPASS_ON = 1,
};

enum panel_uevent_recovery_state {
	PANEL_UEVENT_RECOVERY_IDLE = 0,
	PANEL_UEVENT_RECOVERY_RUNNING = 1,
};

enum {
	PANEL_PCD_BYPASS_OFF = 0,
	PANEL_PCD_BYPASS_ON = 1,
};

enum {
	PANEL_FSYNC_EVENT_OFF = 0,
	PANEL_FSYNC_EVENT_ON = 1,
};

#define ALPM_MODE	0
#define HLPM_MODE	1

enum panel_lpm_lfd_fps {
	LPM_LFD_1HZ = 0,
	LPM_LFD_30HZ,
};

enum panel_active_state {
	PANEL_STATE_OFF = 0,
	PANEL_STATE_ON,
	PANEL_STATE_NORMAL,
	PANEL_STATE_ALPM,
	MAX_PANEL_STATE,
};

enum panel_power {
	PANEL_POWER_OFF = 0,
	PANEL_POWER_ON
};

enum {
	PANEL_DISPLAY_OFF = 0,
	PANEL_DISPLAY_ON,
};

enum {
	PANEL_HMD_OFF = 0,
	PANEL_HMD_ON,
};

enum {
	PANEL_WORK_DISP_DET = 0,
	PANEL_WORK_PCD,
	PANEL_WORK_ERR_FG,
	PANEL_WORK_CONN_DET,
	PANEL_WORK_DIM_FLASH,
	PANEL_WORK_CHECK_CONDITION,
	PANEL_WORK_UPDATE,
	PANEL_WORK_EVASION_DISP_DET,
	PANEL_WORK_LATE_PROBE,
	PANEL_WORK_MAX,
};

enum {
	PANEL_THREAD_MAX,
};

enum {
	PANEL_CB_VRR,
	PANEL_CB_DISPLAY_MODE,
	MAX_PANEL_CB,
};

enum bypass_reason {
	BYPASS_REASON_NONE,
	BYPASS_REASON_DISPLAY_CONNECTOR_IS_DISCONNECTED,
	BYPASS_REASON_AVDD_SWIRE_IS_LOW_AFTER_SLPOUT,
	BYPASS_REASON_PANEL_ID_READ_FAILURE,
	BYPASS_REASON_PCD_DETECTED,
	MAX_BYPASS_REASON,
};

struct panel_state {
	int init_at;
	enum panel_bypass bypass;
	int connected;
	u32 cur_state;
	int power;
	int disp_on;
#ifdef CONFIG_USDM_PANEL_HMD
	int hmd_on;
#endif
	int lpm_brightness;
	int pcd_bypass;
	int fsync_event_on;
	enum panel_uevent_recovery_state uevent_recovery;
};

struct copr_spi_gpios {
	int gpio_sck;
	int gpio_miso;
	int gpio_mosi;
	int gpio_cs;
};

struct host_cb {
	/* callback function type must be same with disp_cb */
	int (*cb)(void *data, void *arg);
	void *data;
};

enum {
	NO_CHECK_STATE = 0,
	PRINT_NORMAL_PANEL_INFO,
	CHECK_NORMAL_PANEL_INFO,
	PRINT_DOZE_PANEL_INFO,
	STATE_MAX
};

#define STR_NO_CHECK			("no state")
#define STR_NOMARL_ON			("after normal disp on")
#define STR_NOMARL_100FRAME		("check normal in 100frames")
#define STR_AOD_ON				("after aod disp on")

struct panel_condition_check {
	bool is_panel_check;
	u32 frame_cnt;
	u8 check_state;
	char str_state[STATE_MAX][30];
};

enum GAMMA_FLASH_RESULT {
	GAMMA_FLASH_ERROR_MTP_OFFSET = -4,
	GAMMA_FLASH_ERROR_CHECKSUM_MISMATCH = -3,
	GAMMA_FLASH_ERROR_NOT_EXIST = -2,
	GAMMA_FLASH_ERROR_READ_FAIL = -1,
	GAMMA_FLASH_PROGRESS = 0,
	GAMMA_FLASH_SUCCESS = 1,
};

struct dim_flash_result {
	atomic_t running;
	bool exist;
	u32 state;
	char result[256];
};

typedef void (*panel_wq_handler)(struct work_struct *);

struct panel_work {
	struct panel_mutex lock;
	char name[MAX_PANEL_WORK_NAME_SIZE];
	struct workqueue_struct *wq;
	struct delayed_work dwork;
	atomic_t running;
	void *data;
	int ret;
};

#define MAX_PANEL_CMD_QUEUE	(1024)

struct panel_cmd_queue {
	struct cmd_set cmd[MAX_PANEL_CMD_QUEUE];
	int top;
	int cmd_payload_size;
	int img_payload_size;
	struct panel_mutex lock;
};

typedef int (*panel_thread_fn)(void *data);

struct panel_thread {
	wait_queue_head_t wait;
	atomic_t count;
	struct task_struct *thread;
	bool should_stop;
};

enum {
	PANEL_DEBUGFS_FILE_LOG,
	PANEL_DEBUGFS_FILE_CMD_LOG,
#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
	PANEL_DEBUGFS_FILE_FREQ_HOP,
#endif
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	PANEL_DEBUGFS_FILE_PANEL_EVENT,
#endif
#if defined(CONFIG_USDM_PANEL_JSON)
	PANEL_DEBUGFS_FILE_EZOP,
	PANEL_DEBUGFS_FILE_FIRMWARE,
#endif
#if defined(CONFIG_USDM_ADAPTIVE_MIPI)
	PANEL_DEBUGFS_FILE_ADAPTIVE_MIPI,
#endif
	MAX_PANEL_DEBUGFS_FILE,
};

enum {
	PANEL_DEBUGFS_DIR_SEQUENCE,
	PANEL_DEBUGFS_DIR_RESOURCE,
	PANEL_DEBUGFS_DIR_PROPERTY,
	MAX_PANEL_DEBUGFS_DIR,
};

#define MAX_PANEL_DEBUGFS_NAME_SIZE (32)

struct panel_debugfs {
	char name[MAX_PANEL_DEBUGFS_NAME_SIZE];
	struct dentry *dir;
	struct dentry *file;
	struct panel_debugfs *parent;
	void *private;
	struct list_head dirs;
	struct list_head list;
};

struct panel_debug {
	struct panel_debugfs *root;
};

#ifdef CONFIG_USDM_PANEL_TESTMODE
struct panel_testmode {
	int enabled;
	struct panel_drv_funcs *funcs_original;
};
#endif

struct panel_device {
	int id;
	int boot_panel_id;
	struct panel_adapter adapter;

	const char *of_node_name;
	struct device_node *ap_vendor_setting_node;
	struct device_node *power_ctrl_node;

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	struct panel_display_modes *panel_modes;
#endif

#ifdef CONFIG_USDM_COPR_SPI
	struct spi_device *spi;
	struct copr_spi_gpios spi_gpio;
#endif

#ifdef CONFIG_USDM_PANEL_COPR
	struct copr_info copr;
#endif
	/* mutex lock for panel operations */
	struct panel_mutex op_lock;
	/* mutex lock for panel's data */
	struct panel_mutex data_lock;
	/* mutex lock for panel's ioctl */
	struct panel_mutex io_lock;
	/* mutex lock for ctrl big lock */
	struct mutex panel_mutex_op_lock;
	/* mutex lock for big lock */
	struct mutex panel_mutex_big_lock;

#if IS_ENABLED(CONFIG_USDM_PANEL_BIG_LOCK)
	/* Total depth of panel mutex */
	int panel_mutex_depth;
	/* commit thread PID */
	int commit_thread_pid;
	bool big_lock;
#endif

	struct device *dev;

#ifdef CONFIG_USDM_MDNIE
	struct mdnie_info mdnie;
#endif
	struct device *lcd_dev;
	struct panel_bl_device panel_bl;

	struct v4l2_subdev sd;

	struct list_head list;
	struct list_head command_initdata_list;

	/* panel internal object(pnobj) list */
	struct list_head maptbl_list;
	struct list_head seq_list;
	struct list_head rdi_list;
	struct list_head res_list;
	struct list_head dump_list;
	struct list_head key_list;
	struct list_head pkt_list;
	struct list_head dly_list;
	struct list_head cond_list;
	struct list_head pwrctrl_list;
	struct list_head cfg_list;
	struct list_head prop_list;
	struct list_head func_list;

	/* panel external object list */
	struct list_head gpio_list;
	struct list_head regulator_list;
	struct list_head power_ctrl_list;
	struct list_head panel_lut_list;

	struct pinctrl *pinctrl;
	struct pinctrl_state *default_gpio_pinctrl;

	struct panel_drv_funcs *funcs;
	struct panel_cmd_queue cmdq;

	struct panel_state state;

	struct panel_work work[PANEL_WORK_MAX];
	struct panel_thread thread[PANEL_THREAD_MAX];

	struct notifier_block fb_notif;
#ifdef CONFIG_USDM_PANEL_DPUI
	struct notifier_block panel_dpui_notif;
#endif
	struct panel_info panel_data;

#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY
	struct aod_dev_info aod;
#endif

#ifdef CONFIG_USDM_PANEL_MAFPC
	struct mafpc_device *mafpc;
	s64 mafpc_write_time;
#endif

#ifdef CONFIG_USDM_PANEL_DDI_FLASH
	struct panel_poc_device poc_dev;
#endif
#ifdef CONFIG_USDM_POC_SPI
	struct panel_spi_dev panel_spi_dev;
#endif
#ifdef CONFIG_USDM_BLIC_I2C
	struct panel_i2c_dev i2c_dev[PANEL_I2C_MAX];
	int nr_i2c_dev;
	struct panel_i2c_dev *i2c_dev_selected;
#endif
#ifdef CONFIG_USDM_PANEL_BLIC
	struct panel_blic_dev blic_dev[PANEL_BLIC_MAX];
	int nr_blic_dev;
#endif
#if defined(CONFIG_INPUT_TOUCHSCREEN)
	struct notifier_block input_notif;
#endif
	struct disp_error_cb_info error_cb_info;
	struct disp_cb_info cb_info[MAX_PANEL_CB];

	ktime_t ktime_panel_on;
	ktime_t ktime_panel_off;
	ktime_t ktime_panel_disp_on;
	ktime_t ktime_first_frame;

	struct dim_flash_result flash_checksum_result;

#ifdef CONFIG_USDM_PANEL_DIM_FLASH
	struct panel_irc_info *irc_info;
#endif
	struct panel_condition_check condition_check;

#ifdef CONFIG_USDM_PANEL_FREQ_HOP
	struct panel_freq_hop freq_hop;
	struct device_node *freq_hop_node;
#endif

#if defined(CONFIG_USDM_SDP_ADAPTIVE_MIPI)
	struct adaptive_mipi_v2_info sdp_adap_mipi;
#endif
#if defined(CONFIG_USDM_SDP_ADAPTIVE_MIPI) ||\
	defined(CONFIG_USDM_ADAPTIVE_MIPI)
	struct panel_adaptive_mipi adap_mipi;
	struct device_node *adap_mipi_node;
#endif
	struct panel_debug d;

	struct panel_hdr_info hdr;

	/* for panel_dsi_write_img */
	unsigned char *cmdbuf;
	struct common_panel_info *cpi;

#ifdef CONFIG_USDM_PANEL_TESTMODE
	struct panel_testmode testmode;
#endif
	struct panel_firmware fw;
#if defined(CONFIG_USDM_LPD_AUTO_BR)
	struct notifier_block lpd_nb;
	/*lists for command packet*/
	struct pnobj_refs lpd_br_list;
	struct pnobj_refs lpd_hbm_br_list;
	struct pnobj_refs lpd_init_list;
#endif
};

struct panel_irq_func {
	int (*enable)(struct panel_device *panel);
	int (*disable)(struct panel_device *panel);
};

int panel_set_property(struct panel_device *panel,
		u32 *property, unsigned int value);
#ifdef CONFIG_USDM_FACTORY_SSR_TEST
int panel_ssr_test(struct panel_device *panel);
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
int panel_ecc_test(struct panel_device *panel);
#endif
int panel_decoder_test(struct panel_device *panel, u8 *buf, int len);
#if defined(CONFIG_USDM_PANEL_VCOM_TRIM_TEST)
int panel_vcom_trim_test(struct panel_device *panel, u8 *buf, int len);
#endif
int panel_check_mipi_read_test(struct panel_device *panel, char *buf);
int panel_ddi_init(struct panel_device *panel);

#ifdef CONFIG_USDM_PANEL_DIM_FLASH
int panel_update_dim_type(struct panel_device *panel, u32 dim_type);
#endif
int panel_flash_checksum_calc(struct panel_device *panel);
int panel_mtp_gamma_check(struct panel_device *panel);

static inline bool IS_PANEL_PWR_ON_STATE(struct panel_device *panel)
{
	return (panel->state.cur_state == PANEL_STATE_ON ||
			panel->state.cur_state == PANEL_STATE_NORMAL ||
			panel->state.cur_state == PANEL_STATE_ALPM);
}

static inline bool IS_PANEL_PWR_OFF_STATE(struct panel_device *panel)
{
	return panel->state.cur_state == PANEL_STATE_OFF;
}

static inline bool panel_bypass_is_on(struct panel_device *panel)
{
	return (panel && panel->state.bypass == PANEL_BYPASS_ON);
}

static inline int panel_set_bypass(struct panel_device *panel,
		enum panel_bypass bypass)
{
	if (!panel)
		return -EINVAL;

	panel->state.bypass = bypass;

	return 0;
}

int panel_display_on(struct panel_device *panel);
int __set_panel_power(struct panel_device *panel, int power);
bool panel_is_factory_mode(struct panel_device *panel);
#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY

#define INIT_WITHOUT_LOCK	0
#define INIT_WITH_LOCK		1

static inline int panel_aod_init_panel(struct panel_device *panel, int lock)

{
	return (panel->aod.ops && panel->aod.ops->init_panel) ?
		panel->aod.ops->init_panel(&panel->aod, lock) : 0;
}

static inline int panel_aod_enter_to_lpm(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->enter_to_lpm) ?
		panel->aod.ops->enter_to_lpm(&panel->aod) : 0;
}

static inline int panel_aod_exit_from_lpm(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->exit_from_lpm) ?
		panel->aod.ops->exit_from_lpm(&panel->aod) : 0;
}

static inline int panel_aod_doze_suspend(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->doze_suspend) ?
		panel->aod.ops->doze_suspend(&panel->aod) : 0;
}

static inline int panel_aod_power_off(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->power_off) ?
		panel->aod.ops->power_off(&panel->aod) : 0;
}

static inline int panel_aod_black_grid_on(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->black_grid_on) ?
		panel->aod.ops->black_grid_on(&panel->aod) : 0;
}

static inline int panel_aod_black_grid_off(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->black_grid_off) ?
		panel->aod.ops->black_grid_off(&panel->aod) : 0;
}

#ifdef SUPPORT_NORMAL_SELF_MOVE
static inline int panel_self_move_pattern_update(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->self_move_pattern_update) ?
		panel->aod.ops->self_move_pattern_update(&panel->aod) : 0;
}
#endif
#endif

int panel_find_max_brightness_from_cpi(struct common_panel_info *info);
char *get_panel_state_names(enum panel_active_state);
void panel_set_cur_state(struct panel_device *panel,
		enum panel_active_state state);
enum panel_active_state panel_get_cur_state(struct panel_device *panel);

struct panel_device *panel_device_create(void);
void panel_device_destroy(struct panel_device *panel);
int panel_device_init(struct panel_device *panel);
int panel_device_exit(struct panel_device *panel);

void panel_init_v4l2_subdev(struct panel_device *panel);
int panel_get_octa_id(struct panel_device *panel, char *buf);
int panel_get_cell_id(struct panel_device *panel, char *buf);
int panel_get_manufacture_code(struct panel_device *panel, char *buf);
int panel_get_manufacture_date(struct panel_device *panel, char *buf);
int panel_get_temperature_range(struct panel_device *panel, char *buf);

bool panel_disconnected(struct panel_device *panel);
#ifdef CONFIG_USDM_DDI_CMDLOG
int panel_seq_cmdlog_dump(struct panel_device *panel);
#endif
void panel_send_ubconn_uevent(struct panel_device *panel);

int panel_create_debugfs(struct panel_device *panel);
void panel_destroy_debugfs(struct panel_device *panel);
int panel_create_panel_object_debugfs(struct panel_device *panel);
void panel_destroy_panel_object_debugfs(struct panel_device *panel);
#if defined(CONFIG_USDM_PANEL_LPM)
int panel_seq_exit_alpm(struct panel_device *panel);
int panel_seq_set_alpm(struct panel_device *panel);
#endif
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
bool panel_display_mode_is_supported(struct panel_device *panel);
int panel_display_mode_cb(struct panel_device *panel);
int panel_update_display_mode(struct panel_device *panel);
int find_panel_mode_by_common_panel_display_mode(struct panel_device *panel,
		struct common_panel_display_mode *cpdm);
int panel_display_mode_find_panel_mode(struct panel_device *panel,
		struct panel_display_mode *pdm);
int panel_display_mode_get_native_mode(struct panel_device *panel);
int panel_set_display_mode_nolock(struct panel_device *panel, int panel_mode);
int panel_get_display_mode(struct panel_device *panel, void *arg);
#endif
bool panel_drv_power_ctrl_exists(struct panel_device *panel, const char *name);
int panel_drv_power_ctrl_execute(struct panel_device *panel, const char *name);
struct panel_gpio *panel_gpio_list_add(struct panel_device *panel, struct panel_gpio *_gpio);
struct panel_gpio *panel_gpio_list_find_by_name(struct panel_device *panel, const char *name);
struct panel_regulator *find_panel_regulator_by_node_name(struct panel_device *panel, const char *node_name);

#if defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
int panel_fast_discharge_set(struct panel_device *panel);
#endif
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
void panel_send_screen_mode_notify(int display_idx, u32 mode);
#endif
#ifdef CONFIG_USDM_PANEL_RCD
int panel_get_rcd_info(struct panel_device *panel, void *arg);
#else
static inline int panel_get_rcd_info(struct panel_device *panel, void *arg) { return -ENODEV; }
#endif
int panel_set_brightness(struct panel_device *panel, unsigned int level);

struct list_head *panel_get_object_list(struct panel_device *panel,
		unsigned int type);
void destroy_panel_object(struct pnobj *pnobj);
int panel_reprobe_with_pnobj_list(struct panel_device *panel,
		struct list_head *pnobj_list);
int add_command_list(struct pnobj *command, struct list_head *head);
int panel_add_command_from_initdata_maptbl(struct maptbl *arr,
		unsigned int size, struct list_head *initdata_list);
int panel_add_command_from_initdata_seqtbl(struct seqinfo *arr,
		unsigned int size, struct list_head *initdata_list);

#define PRINT_PANEL_STATE_BEGIN(_old_state_, _new_state_, _start_) \
	do { \
		_start_ = ktime_get(); \
		panel_info("panel_state: %s -> %s: begin\n", \
				get_panel_state_names(_old_state_), \
				get_panel_state_names(_new_state_)); \
	} while (0)

#define PRINT_PANEL_STATE_END(_old_state_, _new_state_, _start_) \
	do { \
		char rtc_buf[SZ_128] = { 0, }; \
		\
		usdm_snprintf_current_rtc_time(rtc_buf, sizeof(rtc_buf)); \
		panel_info("panel_state: %s -> %s: took %lld ms [%s]\n", \
				get_panel_state_names(_old_state_), \
				get_panel_state_names(_new_state_), \
				ktime_to_ms(ktime_sub(ktime_get(), _start_)),\
				rtc_buf); \
	} while (0)

#define PANEL_DRV_NAME "panel-drv"

#define PANEL_IOC_BASE	'P'

#define PANEL_IOC_ATTACH_ADAPTER			_IOW(PANEL_IOC_BASE, 1, void *)
#define PANEL_IOC_GET_PANEL_STATE		_IOW(PANEL_IOC_BASE, 3, struct panel_state **)
#define PANEL_IOC_PANEL_PROBE			_IO(PANEL_IOC_BASE, 5)

#define PANEL_IOC_SET_POWER				_IOW(PANEL_IOC_BASE, 6, int *)

#define PANEL_IOC_SLEEP_IN				_IO(PANEL_IOC_BASE, 7)
#define PANEL_IOC_SLEEP_OUT				_IO(PANEL_IOC_BASE, 8)

#define PANEL_IOC_DISP_ON				_IOW(PANEL_IOC_BASE, 9, int *)

#define PANEL_IOC_PANEL_DUMP			_IO(PANEL_IOC_BASE, 10)

#define PANEL_IOC_EVT_FRAME_DONE		_IOW(PANEL_IOC_BASE, 11, struct timespec64 *)
#define PANEL_IOC_EVT_VSYNC				_IOW(PANEL_IOC_BASE, 12, struct timespec64 *)

#ifdef CONFIG_USDM_PANEL_LPM
#define PANEL_IOC_DOZE					_IO(PANEL_IOC_BASE, 31)
#define PANEL_IOC_DOZE_SUSPEND			_IO(PANEL_IOC_BASE, 32)
#endif

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
#define PANEL_IOC_GET_DISPLAY_MODE		_IOR(PANEL_IOC_BASE, 48, void *)
#define PANEL_IOC_SET_DISPLAY_MODE		_IOW(PANEL_IOC_BASE, 49, void *)
#define PANEL_IOC_REG_DISPLAY_MODE_CB	_IOR(PANEL_IOC_BASE, 50, struct host_cb *)
#endif

#define PANEL_IOC_REG_RESET_CB			_IOR(PANEL_IOC_BASE, 51, struct host_cb *)
#define PANEL_IOC_REG_VRR_CB			_IOR(PANEL_IOC_BASE, 52, struct host_cb *)
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
#define PANEL_IOC_SET_MASK_LAYER		_IOW(PANEL_IOC_BASE, 61, struct mask_layer_data *)
#endif
#endif //__PANEL_DRV_H__
