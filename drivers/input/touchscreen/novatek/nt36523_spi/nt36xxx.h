/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 46000 $
 * $Date: 2019-06-12 14:25:52 +0800
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#ifndef 	_LINUX_NVT_TOUCH_H
#define		_LINUX_NVT_TOUCH_H

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include "../../../sec_input/sec_input.h"
#include "nt36xxx_mem_map.h"
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#include "../../../sec_input/sec_secure_touch.h"
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#define SECURE_TOUCH_ENABLE	1
#define SECURE_TOUCH_DISABLE	0
#endif

#if IS_ENABLED(CONFIG_MTK_SPI)
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#endif

#if IS_ENABLED(CONFIG_SPI_MT65XX)
#include <linux/platform_data/spi-mt65xx.h>
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
#include <linux/sec_panel_notifier.h>
#endif

#if (IS_ENABLED(CONFIG_EXYNOS_DPU30) || IS_ENABLED(CONFIG_DRM_SAMSUNG_DPU)) && IS_ENABLED(CONFIG_PANEL_NOTIFY)
#include <linux/panel_notify.h>
#endif

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif

extern void stui_tsp_init(int (*stui_tsp_enter)(void), int (*stui_tsp_exit)(void), int (*stui_tsp_type)(void));

#define NVT_DEBUG 1

//---GPIO number---
#define NVTTOUCH_RST_PIN 980
#define NVTTOUCH_INT_PIN 943


//---INT trigger mode---
//#define IRQ_TYPE_EDGE_RISING 1
//#define IRQ_TYPE_EDGE_FALLING 2
#define INT_TRIGGER_TYPE IRQ_TYPE_EDGE_FALLING


//---SPI driver info.---
#define NVT_SPI_NAME "NVT-ts"

#if 0
#if NVT_DEBUG
#define NVT_LOG(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...)    pr_info("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...)    pr_err("[%s] %s %d: " fmt, NVT_SPI_NAME, __func__, __LINE__, ##args)
#endif

//---Input device info.---
//#define NVT_TS_NAME "NVTCapacitiveTouchScreen"
#define NVT_TS_NAME "sec_touchscreen"


//---Touch info.---
#define TOUCH_DEFAULT_MAX_WIDTH 720
#define TOUCH_DEFAULT_MAX_HEIGHT 1480
#define TOUCH_MAX_FINGER_NUM 10
#define TOUCH_KEY_NUM 0
#if TOUCH_KEY_NUM > 0
extern const uint16_t touch_key_array[TOUCH_KEY_NUM];
#endif
#define TOUCH_FORCE_NUM 1000

/* Enable only when module have tp reset pin and connected to host */
#define NVT_TOUCH_SUPPORT_HW_RST 0

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
//#define NVT_TOUCH_MP 1
//#define MT_PROTOCOL_B 1
#define WAKEUP_GESTURE 1
#if WAKEUP_GESTURE
extern const uint16_t gesture_key_array[];
#endif
#define PROXIMITY_FUNCTION	1
#define SEC_LPWG_DUMP 1
#define SEC_FW_STATUS 1

enum NVT_TSP_FW_INDEX {
	NVT_TSP_FW_IDX_BIN	= 0,
	NVT_TSP_FW_IDX_UMS	= 1,
	NVT_TSP_FW_IDX_MP	= 2,
};

#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65

#define NVT_SPI_RETRY_COUNT	3

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 0
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

#define TOUCH_PRINT_INFO_DWORK_TIME	30000	/* 30s */

#if PROXIMITY_FUNCTION
struct nvt_ts_event_proximity {
	u8 reserved_1:3;
	u8 id:5;
	u8 data_page;
	u8 status;
	u8 reserved_2;
} __attribute__ ((packed));
#endif

struct nvt_ts_event_coord {
	u8 status:3;
	u8 id:5;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 w_major;
	u8 pressure_7_0;
} __attribute__ ((packed));

struct nvt_ts_coord {
	u16 x;
	u16 y;
	u16 first_x;
	u16 first_y;
	u8 w_major;
	u8 w_minor;
	u8 status;
	u8 prev_status;
	bool press;
	bool prev_press;
	int move_count;
};

struct nvt_firmware {
	u8 *data;
	size_t size;
};

#if SEC_LPWG_DUMP
struct nvt_ts_lpwg_coordinate_event {
	u8 event_type:2;
	u8 touch_status:2;
	u8 tid:2;
	u8 frame_count_9_8:2;
	u8 frame_count_7_0;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
} __attribute__ ((packed));

struct nvt_ts_lpwg_gesture_event {
	u8 event_type:2;
	u8 gesture_id:6;
	u8 reserved_1;
	u8 reserved_2;
	u8 reserved_3;
	u8 reserved_4;
} __attribute__ ((packed));

struct nvt_ts_lpwg_vendor_event {
	u8 event_type:2;
	u8 info_type:6;
	u8 ng_code;
	u8 reserved_1;
	u8 reserved_2;
	u8 reserved_3;
} __attribute__ ((packed));

enum {
	LPWG_EVENT_COOR = 0x00,
	LPWG_EVENT_GESTURE = 0x01,
	LPWG_EVENT_VENDOR = 0x02,
	LPWG_EVENT_RESERVED_1 = 0x03
};

enum {
	LPWG_TOUCH_STATUS_PRESS = 0x00,
	LPWG_TOUCH_STATUS_RELEASE = 0x01,
	LPWG_TOUCH_STATUS_RESERVED_1 = 0x02,
	LPWG_TOUCH_STATUS_RESERVED_2 = 0x03
};

enum {
	LPWG_GESTURE_DT = 0x00,	// Double tap
	LPWG_GESTURE_SW = 0x01,	// Swipe up
	LPWG_GESTURE_RESERVED_1 = 0x02,
	LPWG_GESTURE_RESERVED_2 = 0x03
};

typedef enum {
	LPWG_DUMP_DISABLE = 0,
	LPWG_DUMP_ENABLE = 1,
} LPWG_DUMP;

#define LPWG_DUMP_PACKET_SIZE	5		/* 5 byte */
#define LPWG_DUMP_TOTAL_SIZE	500		/* 5 byte * 100 */
#endif

struct nvt_ts_platdata {
	int irq_gpio;
	u32 irq_flags;
	int cs_gpio;
	u8 x_num;
	u8 y_num;
	u16 abs_x_max;
	u16 abs_y_max;
	u32 area_indicator;
	u32 area_navigation;
	u32 area_edge;
	u8 max_touch_num;
	struct pinctrl *pinctrl;
	bool support_ear_detect;
	bool enable_settings_aot;
	bool enable_sysinput_enabled;
	bool prox_lp_scan_enabled;
	bool enable_glove_mode;
	const char *firmware_name;
	const char *firmware_name_mp;
	u32 open_test_spec[2];
	u32 short_test_spec[2];
	int diff_test_frame;
	u32 fdm_x_num;
	int lcd_id;

	const char *name_lcd_rst;
	const char *name_lcd_vddi;
	const char *name_lcd_bl_en;
	const char *name_lcd_vsp;
	const char *name_lcd_vsn;

	u32 resume_lp_delay;
};

struct nvt_ts_data {
	struct spi_device *client;
	struct nvt_ts_platdata *platdata;
	struct nvt_ts_coord coords[TOUCH_MAX_FINGER_NUM];
	u8 touch_count;
	struct input_dev *input_dev;
	struct input_dev *input_dev_proximity;
	uint16_t addr;
	int8_t phys[32];
	uint8_t fw_ver;
//	uint8_t x_num;
//	uint8_t y_num;
//	uint16_t abs_x_max;
//	uint16_t abs_y_max;
//	uint8_t max_touch_num;
	uint8_t max_button_num;
	uint32_t int_trigger_type;
//	int32_t irq_gpio;
//	uint32_t irq_flags;
	int32_t reset_gpio;
	uint32_t reset_flags;
	struct mutex lock;
	const struct nvt_ts_mem_map *mmap;
	uint8_t carrier_system;
	uint8_t hw_crc;
	uint16_t nvt_pid;
	uint8_t *rbuf;
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool irq_enabled;
	u8 fw_ver_ic[4];
	u8 fw_ver_ic_bar;
	u8 fw_ver_bin[4];
	u8 fw_ver_bin_bar;
	volatile int power_status;
	volatile bool shutdown_called;
	u16 sec_function;
	int lowpower_mode;
#if IS_ENABLED(CONFIG_MTK_SPI)
	struct mt_chip_conf spi_ctrl;
#endif
#if IS_ENABLED(CONFIG_SPI_MT65XX)
    struct mtk_chip_config spi_ctrl;
#endif
	struct sec_cmd_data sec;
	unsigned int ear_detect_mode;
	bool ed_reset_flag;
	long prox_power_off;
	u8 hover_event;	//virtual_prox
	bool lcdoff_test;

	u16 landscape_deadzone[2];

	struct delayed_work work_read_info;
	struct delayed_work work_print_info;
	u32 print_info_cnt_open;
	u32 print_info_cnt_release;
	u16 print_info_currnet_mode;

	struct regulator *regulator_lcd_rst;
	struct regulator *regulator_lcd_vddi;
	struct regulator *regulator_lcd_bl_en;
	struct regulator *regulator_lcd_vsp;
	struct regulator *regulator_lcd_vsn;

	u8 noise_mode;
	u8 prox_in_aot;
	unsigned int scrub_id;

#if SEC_FW_STATUS
	u8 fw_status_record;
#endif

	int fw_index;
	struct nvt_firmware	*cur_fw;
	struct nvt_firmware	*nvt_bin_fw;
	struct nvt_firmware	*nvt_mp_fw;
	struct nvt_firmware	*nvt_ums_fw;

#if SEC_LPWG_DUMP
	u8 *lpwg_dump_buf;
	u16 lpwg_dump_buf_idx;
	u16 lpwg_dump_buf_size;
#endif
	struct notifier_block nb;
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	struct notifier_block vbus_nb;
	struct delayed_work work_vbus;
#endif
	int lcd_esd_recovery;
	struct completion resume_done;

	bool cmd_result_all_onboot;
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	atomic_t secure_enabled;
	atomic_t secure_pending_irqs;
	struct completion secure_powerdown;
	struct completion secure_interrupt;
	struct mutex secure_lock;
#endif
};

#if NVT_TOUCH_PROC
struct nvt_flash_data{
	rwlock_t lock;
};
#endif

typedef enum {
	RESET_STATE_INIT = 0xA0,// IC reset
	RESET_STATE_REK,		// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN,	// normal run
	RESET_STATE_MAX  = 0xAF
} RST_COMPLETE_STATE;

typedef enum {
	EVENT_MAP_HOST_CMD		= 0x50,
	EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE	= 0x51,
	EVENT_MAP_FUNCT_STATE		= 0x5C,
	EVENT_MAP_RESET_COMPLETE	= 0x60,
	EVENT_MAP_FWINFO			= 0x78,
	EVENT_MAP_PROXIMITY_SCAN	= 0x7F,
	EVENT_MAP_PANEL			= 0x8F,
	EVENT_MAP_PROJECTID		= 0x9A,
	EVENT_MAP_SENSITIVITY_DIFF	= 0x9D,	// 18 bytes
	EVENT_MAP_PROXIMITY_SUM		= 0xB0, // 2 bytes
	EVENT_MAP_PROXIMITY_THD		= 0xB2, // 2 bytes
} SPI_EVENT_MAP;

typedef enum {
	TEST_RESULT_PASS = 0x00,
	TEST_RESULT_FAIL = 0x01,
} TEST_RESULT;

enum {
	LP_OFF = 0,
	LP_AOT = (1 << 0),
	LP_SPAY = (1 << 1)
};

enum {
	POWER_OFF_STATUS = 0,
	POWER_ON_STATUS,
	LP_MODE_STATUS,
	LP_MODE_EXIT,
};

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define NVT_TRANSFER_LEN	(63*1024)
#define NVT_READ_LEN		(2*1024)
#define BLOCK_64KB_NUM 4

#define DEEP_SLEEP_ENTER		0x11
#define LPWG_ENTER		0x13
#define PROX_SCAN_START	0x08
#define PROX_SCAN_STOP	0x09

#if PROXIMITY_FUNCTION
#define PROXIMITY_ENTER		0x85
#define PROXIMITY_LEAVE		0x86
#define PROXIMITY_INSENSITIVITY	0x00
#define PROXIMITY_NORMAL	0x03
#endif

#define SHOW_NOT_SUPPORT_CMD	0x00

#define NORMAL_MODE		0x00
#define TEST_MODE_1		0x21
#define TEST_MODE_2		0x22
#define MP_MODE_CC		0x41
#define FREQ_HOP_DISABLE	0x66
#define FREQ_HOP_ENABLE		0x65
#define HANDSHAKING_HOST_READY	0xBB

#define CHARGER_PLUG_OFF		0x51
#define CHARGER_PLUG_AC			0x53
#define GLOVE_ENTER				0xB1
#define GLOVE_LEAVE				0xB2
#define EDGE_REJ_VERTICLE_MODE	0xBA
#define EDGE_REJ_LEFT_UP_MODE	0xBB
#define EDGE_REJ_RIGHT_UP_MODE	0xBC
#define HIGH_SENSITIVITY_ENTER	0xBD
#define HIGH_SENSITIVITY_LEAVE	0xBE
#define BLOCK_AREA_ENTER		0x71
#define BLOCK_AREA_LEAVE		0x72
#define EDGE_AREA_ENTER			0x73 //min:7
#define EDGE_AREA_LEAVE			0x74 //0~6
#define HOLE_AREA_ENTER			0x75 //0~120
#define HOLE_AREA_LEAVE			0x76 //no report
#define SPAY_SWIPE_ENTER		0x77
#define SPAY_SWIPE_LEAVE		0x78
#define DOUBLE_CLICK_ENTER		0x79
#define DOUBLE_CLICK_LEAVE		0x7A
#define SENSITIVITY_ENTER		0x7B
#define SENSITIVITY_LEAVE		0x7C
#define EXTENDED_CUSTOMIZED_CMD	0x7F

typedef enum {
	SET_GRIP_EXECPTION_ZONE = 1,
	SET_GRIP_PORTRAIT_MODE = 2,
	SET_GRIP_LANDSCAPE_MODE = 3,
	SET_TOUCH_DEBOUNCE = 5,	// for SIP
	SET_GAME_MODE = 6,
	SET_HIGH_SENSITIVITY_MODE = 7,
#if PROXIMITY_FUNCTION
	PROX_SLEEP_IN = 8,
	PROX_SLEEP_OUT = 9,
#endif
#if SEC_LPWG_DUMP
	SET_LPWG_DUMP = 0x0B
#endif
} EXTENDED_CUSTOMIZED_CMD_TYPE;

typedef enum {
	DEBOUNCE_NORMAL = 0,
	DEBOUNCE_LOWER = 1,		//to optimize tapping performance for SIP
} TOUCH_DEBOUNCE;

typedef enum {
	GAME_MODE_DISABLE = 0,
	GAME_MODE_ENABLE = 1,
} GAME_MODE;

typedef enum {
	HIGH_SENSITIVITY_DISABLE = 0,
	HIGH_SENSITIVITY_ENABLE = 1,
} HIGH_SENSITIVITY_MODE;

#define BUS_TRANSFER_LENGTH  256

#define XDATA_SECTOR_SIZE	256

#define CMD_RESULT_WORD_LEN	10

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))


/* function bit combination code */
#define EDGE_REJ_VERTICLE	1
#define EDGE_REJ_LEFT_UP	2
#define EDGE_REJ_RIGHT_UP	3

#define ORIENTATION_0		0
#define ORIENTATION_90		1
#define ORIENTATION_180		2
#define ORIENTATION_270		3

#define OPEN_SHORT_TEST		1
#define CHECK_ONLY_OPEN_TEST	1
#define CHECK_ONLY_SHORT_TEST	2

typedef enum {
	FUNCT_MIN = 0,
	GLOVE = 1,
	CHARGER,
#ifdef PROXIMITY_FUNCTION
	PROXIMITY = 3,
#endif
	EDGE_REJECT_L = 6,
	EDGE_REJECT_H,
	EDGE_PIXEL,		// 8
	HOLE_PIXEL,
	SPAY_SWIPE,		// 10
	DOUBLE_CLICK,
	BLOCK_AREA,		// 12
	HIGH_SENSITIVITY = 14,
	SENSITIVITY,
	FUNCT_MAX,
} FUNCT_BIT;

typedef enum {
	GLOVE_MASK		= 0x0002,	// bit 1
	CHARGER_MASK		= 0x0004,	// bit 2
#ifdef PROXIMITY_FUNCTION
	PROXIMITY_MASK	= 0x0008,	// bit 3
#endif
	EDGE_REJECT_MASK	= 0x00C0,	// bit [6|7]
	EDGE_PIXEL_MASK		= 0x0100,	// bit 8
	HOLE_PIXEL_MASK		= 0x0200,	// bit 9
	SPAY_SWIPE_MASK		= 0x0400,	// bit 10
	DOUBLE_CLICK_MASK	= 0x0800,	// bit 11
	BLOCK_AREA_MASK		= 0x1000,	// bit 12
	NOISE_MASK			= 0x2000,	// bit 13
	HIGH_SENSITIVITY_MASK = 0x4000, // bit 14
	SENSITIVITY_MASK	= 0x8000,	// bit 15
	PROX_IN_AOT_MASK	= 0x80000,	// bit 19
#ifdef PROXIMITY_FUNCTION
	FUNCT_ALL_MASK		= 0xFFCA,
#else
	FUNCT_ALL_MASK		= 0xFFC2,
#endif
} FUNCT_MASK;

enum {
	BUILT_IN = 0,
	UMS,
	NONE,
	SPU,
};

#if SEC_FW_STATUS
enum {
	FW_STATUS_PRINT_OR_NOT = 0x01,
	FW_STATUS_WATER_FLAG = 0x02,
	FW_STATUS_PALM_FLAG = 0x04,
	FW_STATUS_HOPPING_FLAG = 0x08,
	FW_STATUS_BENDING_FLAG = 0x10,
	FW_STATUS_GLOVE_FLAG = 0x20,
	FW_STATUS_GND_UNSTABLE = 0x40,
	FW_STATUS_TA_PIN = 0x80,
	FW_STATUS_REK_STATUS = 0x07
};
#endif

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;

//---extern structures---
extern struct nvt_ts_data *ts;

//---extern functions---
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len);
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len);
void nvt_bootloader_reset(void);
void nvt_eng_reset(void);
void nvt_sw_reset(void);
void nvt_sw_reset_idle(void);
void nvt_boot_ready(void);
void nvt_bld_crc_enable(void);
void nvt_fw_crc_enable(void);
int32_t nvt_update_firmware(const char *firmware_name);
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
int32_t nvt_get_fw_info(void);
int32_t nvt_clear_fw_status(void);
int32_t nvt_check_fw_status(void);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
int nvt_ts_mode_restore(struct nvt_ts_data *ts);
int nvt_get_fw_info(void);
int nvt_ts_fw_update_from_bin(struct nvt_ts_data *ts);
int nvt_ts_fw_update_from_mp_bin(struct nvt_ts_data *ts, bool is_start);
int nvt_ts_fw_update_from_external(struct nvt_ts_data *ts, const char *file_path);
//int nvt_ts_resume_pd(struct nvt_ts_data *ts);
int nvt_get_checksum(struct nvt_ts_data *ts, u8 *csum_result, u8 csum_size);
int32_t nvt_set_page(uint32_t addr);
#if PROXIMITY_FUNCTION
int set_ear_detect(struct nvt_ts_data *ts, int mode, bool print_log);
int nvt_ts_mode_switch_extened(struct nvt_ts_data *ts, u8 *cmd, u8 len, bool print_log);
#endif
int nvt_ts_mode_switch(struct nvt_ts_data *ts, u8 cmd, bool print_log);
int pinctrl_configure(struct nvt_ts_data *ts, bool enable);
void nvt_irq_enable(bool enable);
bool nvt_ts_lcd_power_check(void);

#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

u16 nvt_ts_mode_read(struct nvt_ts_data *ts);
void nvt_ts_release_all_finger(struct nvt_ts_data *ts);

void nvt_ts_early_resume(struct device *dev);
int32_t nvt_ts_resume(struct device *dev);
int32_t nvt_ts_suspend(struct device *dev);
void read_tsp_info_onboot(void *);
int nvt_sec_mp_parse_dt(struct nvt_ts_data *ts, const char *node_compatible);
#if SEC_LPWG_DUMP
int nvt_ts_lpwg_dump_buf_read(u8 *buf);
#endif

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
extern int ss_panel_notifier_register(struct notifier_block *nb);
extern int ss_panel_notifier_unregister(struct notifier_block *nb);
#endif
#if IS_ENABLED(CONFIG_DRM_SAMSUNG_DPU)
extern int panel_notifier_register(struct notifier_block *nb);
extern int panel_notifier_unregister(struct notifier_block *nb);
#endif
#endif /* _LINUX_NVT_TOUCH_H */
