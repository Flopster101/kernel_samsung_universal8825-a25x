/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Samsung EXYNOS SoC DisplayPort driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DP_H_
#define _DP_H_

#include <drm/drm_of.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/exynos_drm.h>

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/component.h>
#include <media/v4l2-subdev.h>
#include <linux/phy/phy.h>
#include <dp_cal.h>

#include <exynos_display_common.h>
#include <exynos_drm_debug.h>
#include "exynos_drm_crtc.h"
#include "exynos_drm_decon.h"

#include "./cal_pm/regs-dp.h"
#include "./cal_pm/regs-usbdpphy_ctrl.h"
#include "./cal_pm/regs-usbdpphy_tca_ctrl.h"

#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER
#include "../dp_ext_func/dp_logger.h"
#endif
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
#include "../dp_ext_func/displayport_bigdata.h"
#endif

#define FEATURE_USE_DRM_EDID_PARSER
#define FEATURE_DEX_SUPPORT
#define FEATURE_MANAGE_HMD_LIST
#define FEATURE_DEX_ADAPTER_TWEAK

extern int forced_resolution;
extern int dp_log_level;
//extern int phy_status;
#define MAX_POOR_CONNECT_EVENT 10

#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER
extern int dp_log_level;
#define dp_err(dp, fmt, ...)						\
	do {								\
		if (dp_log_level >= 3 && dp) {				\
			pr_err("Displayport: "fmt, ##__VA_ARGS__);	\
			dp_logger_print(fmt, ##__VA_ARGS__);		\
		}							\
	} while (0)

#define dp_warn(dp, fmt, ...)						\
	do {								\
		if (dp_log_level >= 4 && dp) {				\
			pr_warn("Displayport: "fmt, ##__VA_ARGS__);	\
			dp_logger_print(fmt, ##__VA_ARGS__);		\
		}							\
	} while (0)

#define dp_info(dp, fmt, ...)						\
	do {								\
		if (dp_log_level >= 6 && dp) {				\
			pr_info("Displayport: "fmt, ##__VA_ARGS__);	\
			dp_logger_print(fmt, ##__VA_ARGS__);		\
		}							\
	} while (0)

#define dp_debug(dp, fmt, ...)						\
	do {								\
		if (dp_log_level >= 7 && dp)	{			\
			pr_info("Displayport: "fmt, ##__VA_ARGS__);	\
			dp_logger_print(fmt, ##__VA_ARGS__);		\
		}							\
	} while (0)

#undef cal_log_debug
#define cal_log_debug(id, fmt, ...)				\
	do {							\
		if (dp_log_level >= 7)	{			\
			pr_debug("Displayport: "fmt, ##__VA_ARGS__);\
			dp_logger_print("%d "fmt, id, ##__VA_ARGS__);\
		}						\
	} while (0)

#undef cal_log_info
#define cal_log_info(id, fmt, ...)				\
	do {							\
		pr_info("Displayport: "fmt, ##__VA_ARGS__);	\
		if (dp_log_level >= 7)				\
			dp_logger_print("%d "fmt, id, ##__VA_ARGS__);	\
	} while (0)

#undef cal_log_warn
#define cal_log_warn(id, fmt, ...)				\
	do {							\
		pr_warn("Displayport: "fmt, ##__VA_ARGS__);	\
		dp_logger_print("%d "fmt, id, ##__VA_ARGS__);	\
	} while (0)

#undef cal_log_err
#define cal_log_err(id, fmt, ...)				\
	do {							\
		pr_err("Displayport: "fmt, ##__VA_ARGS__);	\
		dp_logger_print("%d "fmt, id, ##__VA_ARGS__);	\
	} while (0)

#else /* CONFIG_SEC_DISPLAYPORT_LOGGER */

#define dp_info(dp, fmt, ...)	\
dpu_pr_info(drv_name((dp)), 0, dp_log_level, fmt, ##__VA_ARGS__)

#define dp_warn(dp, fmt, ...)	\
dpu_pr_warn(drv_name((dp)), 0, dp_log_level, fmt, ##__VA_ARGS__)

#define dp_err(dp, fmt, ...)	\
dpu_pr_err(drv_name((dp)), 0, dp_log_level, fmt, ##__VA_ARGS__)

#define dp_debug(dp, fmt, ...)	\
dpu_pr_debug(drv_name((dp)), 0, dp_log_level, fmt, ##__VA_ARGS__)

#endif /* CONFIG_SEC_DISPLAYPORT_LOGGER */

extern struct dp_device *dp_drvdata;

enum dp_state {
	DP_STATE_INIT,
	DP_STATE_ON,
	DP_STATE_OFF,
};

struct dp_resources {
	int aux_ch_mux_gpio;
	int irq;
	void __iomem *link_regs;
	void __iomem *phy_regs;
	void __iomem *phy_tca_regs;
	struct clk *aclk;
	struct clk *dposc_clk;
};

enum dp_get_sst_id_type {
	FIND_SST_ID,
	ALLOC_SST_ID,
};

#define MAX_RETRY_CNT 16
#define MAX_REACHED_CNT 3
#define MAX_SWING_REACHED_BIT_POS 2
#define MAX_PRE_EMPHASIS_REACHED_BIT_POS 5
#define DPCP_LINK_SINK_STATUS_FIELD_LENGTH 8

#define DPCD_ADD_REVISION_NUMBER 0x00000
#define DPCD_VER_1_0 0x10
#define DPCD_VER_1_1 0x11
#define DPCD_VER_1_2 0x12
#define DPCD_VER_1_3 0x13
#define DPCD_VER_1_4 0x14

#define DPCD_ADD_MAX_LINK_RATE 0x00001
#define LINK_RATE_1_62Gbps 0x06
#define LINK_RATE_2_7Gbps 0x0A
#define LINK_RATE_5_4Gbps 0x14
#define LINK_RATE_8_1Gbps 0x1E

#define DPCD_ADD_MAX_LANE_COUNT 0x00002
#define MAX_LANE_COUNT (0x1F << 0)
#define TPS3_SUPPORTED (1 << 6)
#define ENHANCED_FRAME_CAP (1 << 7)

#define DPCD_ADD_MAX_DOWNSPREAD 0x00003
#define NO_AUX_HANDSHAKE_LINK_TRANING (1 << 6)
#define TPS4_SUPPORTED (1 << 7)

#define DPCD_ADD_DOWN_STREAM_PORT_PRESENT 0x00005
#define BIT_DFP_TYPE 0x6
#define DFP_TYPE_DP 0x00
#define DFP_TYPE_VGA 0x01 /* analog video */
#define DFP_TYPE_HDMI 0x2
#define DFP_TYPE_OTHERS 0x3 /* not have EDID like composite and Svideo port */

#define DPCD_ADD_I2C_SPEED_CONTROL_CAPABILITES 0x0000C
#define I2C_1Kbps 0x01
#define I2C_5Kbps 0x02
#define I2C_10Kbps 0x04
#define I2C_100Kbps 0x08
#define I2C_400Kbps 0x10
#define I2C_1Mbps 0x20

#define DPCD_ADD_TRAINING_AUX_RD_INTERVAL 0x0000E
#define TRANING_AUX_RD_INTERVAL_400us 0x00
#define TRANING_AUX_RD_INTERVAL_4ms 0x01
#define TRANING_AUX_RD_INTERVAL_8ms 0x02
#define TRANING_AUX_RD_INTERVAL_12ms 0x03
#define TRANING_AUX_RD_INTERVAL_16ms 0x04

#define DPCD_ADD_MSTM_CAP 0x00021
#define MST_CAP (1 << 0)

#define DPCD_ADD_LINK_BW_SET 0x00100

#define DPCD_ADD_LANE_COUNT_SET 0x00101

#define DPCD_ADD_TRANING_PATTERN_SET 0x00102
#define TRAINING_PTTERN_SELECT (3 << 0)
#define RECOVERED_CLOCK_OUT_EN (1 << 4)
#define DPCD_SCRAMBLING_DISABLE (1 << 5)
#define SYMBOL_ERROR_COUNT_SEL (3 << 6)

#define DPCD_ADD_TRANING_LANE0_SET 0x00103
#define DPCD_ADD_TRANING_LANE1_SET 0x00104
#define DPCD_ADD_TRANING_LANE2_SET 0x00105
#define DPCD_ADD_TRANING_LANE3_SET 0x00106
#define VOLTAGE_SWING_SET (3 << 0)
#define MAX_SWING_REACHED (1 << 2)
#define PRE_EMPHASIS_SWING_SET (3 << 3)
#define MAX_PRE_EMPHASIS_REACHED (1 << 5)

#define DPCD_ADD_I2C_SPEED_CONTROL_STATUS 0x00109

#define DPCD_ADD_LINK_QUAL_LANE0_SET 0x0010B
#define DPCD_ADD_LINK_QUAL_LANE1_SET 0x0010C
#define DPCD_ADD_LINK_QUAL_LANE2_SET 0x0010D
#define DPCD_ADD_LINK_QUAL_LANE3_SET 0x0010E
#define DPCD_LINK_QUAL_PATTERN_SET (7 << 0)

#define DPCD_ADD_MSTM_CTRL 0x00111
#define DPCD_MST_EN (1 << 0)
#define UP_REQ_EN (1 << 1)
#define UPSTREAM_IS_SRC (1 << 2)

#define DPCD_ADD_PAYLOAD_ALLOCATE_SET 0x001C0
#define DPCD_ADD_PAYLOAD_ALLOCATE_START_TIME_SLOT 0x001C1
#define DPCD_ADD_PAYLOAD_ALLOCATE_TIME_SLOT_COUNT 0x001C2

#define DPCD_ADD_SINK_COUNT 0x00200
#define SINK_COUNT2 (1 << 7)
#define CP_READY (1 << 6)
#define SINK_COUNT1 (0x3F << 0)

#define DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR 0x00201
#define DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR_ESI0 0x02003
#define AUTOMATED_TEST_REQUEST (1 << 1)
#define CP_IRQ (1 << 2)
#define MCCS_IRQ (1 << 3)
#define DOWN_REP_MSG_RDY (1 << 4)
#define UP_REQ_MSG_RDY (1 << 5)
#define SINK_SPECIFIC_IRQ (1 << 6)

#define DPCD_ADD_LANE0_1_STATUS 0x00202
#define LANE0_CR_DONE (1 << 0)
#define LANE0_CHANNEL_EQ_DONE (1 << 1)
#define LANE0_SYMBOL_LOCKED (1 << 2)
#define LANE1_CR_DONE (1 << 4)
#define LANE1_CHANNEL_EQ_DONE (1 << 5)
#define LANE1_SYMBOL_LOCKED (1 << 6)

#define DPCD_ADD_LANE2_3_STATUS 0x00203
#define LANE2_CR_DONE (1 << 0)
#define LANE2_CHANNEL_EQ_DONE (1 << 1)
#define LANE2_SYMBOL_LOCKED (1 << 2)
#define LANE3_CR_DONE (1 << 4)
#define LANE3_CHANNEL_EQ_DONE (1 << 5)
#define LANE3_SYMBOL_LOCKED (1 << 6)

#define DPCD_ADD_LANE_ALIGN_STATUS_UPDATE 0x00204
#define INTERLANE_ALIGN_DONE (1 << 0)
#define DOWNSTREAM_PORT_STATUS_CHANGED (1 << 6)
#define LINK_STATUS_UPDATE (1 << 7)

#define DPCD_ADD_SINK_STATUS 0x00205
#define RECEIVE_PORT_0_STATUS (1 << 0)
#define RECEIVE_PORT_1_STATUS (1 << 1)

#define DPCD_ADD_ADJUST_REQUEST_LANE0_1 0x00206
#define VOLTAGE_SWING_LANE0 (3 << 0)
#define PRE_EMPHASIS_LANE0 (3 << 2)
#define VOLTAGE_SWING_LANE1 (3 << 4)
#define PRE_EMPHASIS_LANE1 (3 << 6)

#define DPCD_ADD_ADJUST_REQUEST_LANE2_3 0x00207
#define VOLTAGE_SWING_LANE2 (3 << 0)
#define PRE_EMPHASIS_LANE2 (3 << 2)
#define VOLTAGE_SWING_LANE3 (3 << 4)
#define PRE_EMPHASIS_LANE3 (3 << 6)

#define DPCD_TEST_REQUEST 0x00218
#define TEST_LINK_TRAINING (1 << 0)
#define TEST_VIDEO_PATTERN (1 << 1)
#define TEST_EDID_READ (1 << 2)
#define TEST_PHY_TEST_PATTERN (1 << 3)
#define TEST_FAUX_TEST_PATTERN (1<<4)
#define TEST_AUDIO_PATTERN (1<<5)
#define TEST_AUDIO_DISABLED_VIDEO (1<<6)

#define DPCD_TEST_LINK_RATE 0x00219
#define TEST_LINK_RATE (0xFF << 0)

#define DPCD_TEST_LANE_COUNT 0x00220
#define TEST_LANE_COUNT (0x1F << 0)

#define DPCD_TEST_PATTERN 0x00221
#define DPCD_TEST_PATTERN_COLOR_RAMPS 0x01
#define DPCD_TEST_PATTERN_BW_VERTICAL_LINES 0x02
#define DPCD_TEST_PATTERN_COLOR_SQUARE 0x03


#define DPCD_TEST_H_TOTAL_1 0x00222	//[15:8]
#define DPCD_TEST_H_TOTAL_2 0x00223	//[7:0]
#define DPCD_TEST_V_TOTAL_1 0x00224	//[15:8]
#define DPCD_TEST_V_TOTAL_2 0x00225	//[7:0]

#define DPCD_TEST_H_START_1 0x00226	//[15:8]
#define DPCD_TEST_H_START_2 0x00227	//[7:0]
#define DPCD_TEST_V_START_1 0x00228	//[15:8]
#define DPCD_TEST_V_START_2 0x00229	//[7:0]

#define DPCD_TEST_H_SYNC_1 0x0022A	//[15:8]
#define DPCD_TEST_H_SYNC_2 0x0022B	//[7:0]
#define DPCD_TEST_V_SYNC_1 0x0022C	//[15:8]
#define DPCD_TEST_V_SYNC_2 0x0022D	//[7:0]

#define DPCD_TEST_H_WIDTH_1 0x0022E	//[15:8]
#define DPCD_TEST_H_WIDTH_2 0x0022F	//[7:0]
#define DPCD_TEST_V_HEIGHT_1 0x00230	//[15:8]
#define DPCD_TEST_V_HEIGHT_2 0x00231	//[7:0]

#define DPCD_TEST_MISC_1 0x00232
#define TEST_SYNCHRONOUS_CLOCK (1 << 0)
#define TEST_COLOR_FORMAT (3 << 1)
#define TEST_DYNAMIC_RANGE (1 << 3)
#define TEST_YCBCR_COEFFICIENTS (1 << 4)
#define TEST_BIT_DEPTH (7 << 5)

#define DPCD_TEST_MISC_2 0x00233
#define TEST_REFRESH_DENOMINATOR (1 << 0)
#define TEST_INTERLACED (1 << 1)

#define DPCD_TEST_REFRESH_RATE_NUMERATOR 0x00234

#define DCDP_ADD_PHY_TEST_PATTERN 0x00248
#define PHY_TEST_PATTERN_SEL (3 << 0)

#define DPCD_TEST_RESPONSE 0x00260
#define TEST_ACK (1 << 0)
#define TEST_NAK (1 << 1)
#define TEST_EDID_CHECKSUM_WRITE (1 << 2)

#define DPCD_TEST_EDID_CHECKSUM 0x00261

#define DPCD_TEST_AUDIO_MODE 0x00271
#define TEST_AUDIO_SAMPLING_RATE (0x0F << 0)
#define TEST_AUDIO_CHANNEL_COUNT (0xF0 << 0)

#define DPCD_TEST_AUDIO_PATTERN_TYPE 0x00272

#define DPCD_ADD_PAYLOAD_TABLE_UPDATE_STATUS 0x002C0
#define VC_PAYLOAD_ID_TABLE_UPDATE (1 << 0)

#define DPCD_BRANCH_HW_REVISION	0x509
#define DPCD_BRANCH_SW_REVISION_MAJOR	0x50A
#define DPCD_BRANCH_SW_REVISION_MINOR	0x50B

#define DPCD_ADD_SET_POWER 0x00600
#define SET_POWER_STATE (3 << 0)
#define SET_POWER_DOWN 0x02
#define SET_POWER_NORMAL 0x01

#define DPCD_HDCP22_RX_CAPS 0x6921D
#define VERSION (0xFF << 16)
#define HDCP_CAPABLE (1 << 1)

#define SMC_CHECK_STREAM_TYPE_ID		((unsigned int)0x82004022)
#define DPCD_HDCP22_RX_INFO 0x69330

#define DPCD_HDCP22_RX_CAPS_LENGTH 3
#define DPCD_HDCP_VERSION_BIT_POSITION 16

#define DPCD_HDCP22_RXSTATUS_READY			(0x1 << 0)
#define DPCD_HDCP22_RXSTATUS_HPRIME_AVAILABLE		(0x1 << 1)
#define DPCD_HDCP22_RXSTATUS_PAIRING_AVAILABLE		(0x1 << 2)
#define DPCD_HDCP22_RXSTATUS_REAUTH_REQ			(0x1 << 3)
#define DPCD_HDCP22_RXSTATUS_LINK_INTEGRITY_FAIL	(0x1 << 4)

#define HDCP_VERSION_1_3 0x13
#define HDCP_VERSION_2_2 0x02

enum drm_state {
	DRM_OFF = 0x0,
	DRM_ON = 0x1,
	DRM_SAME_STREAM_TYPE = 0x2      /* If the previous contents and stream_type id are the same flag */
};

enum auth_state {
	HDCP_AUTH_PROCESS_IDLE  = 0x1,
	HDCP_AUTH_PROCESS_STOP  = 0x2,
	HDCP_AUTH_PROCESS_DONE  = 0x3
};

enum auth_signal {
        HDCP_DRM_OFF    = 0x100,
        HDCP_DRM_ON     = 0x200,
        HDCP_RP_READY   = 0x300,
};

enum hotplug_state {
	EXYNOS_HPD_UNPLUG = 0,
	EXYNOS_HPD_PLUG,
	EXYNOS_HPD_IRQ,
};

#ifdef CONFIG_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
enum dp_wait_state {
	DP_READY_NO,
	DP_READY_YES,
};
struct pdic_event {
	struct list_head list;
	PD_NOTI_TYPEDEF event;
};
#endif

#define MAX_EDID_BLOCK 4
#define EDID_BLOCK_SIZE 128

#ifdef FEATURE_USE_DRM_EDID_PARSER
#define MODE_FILTER_TEST_SINK		(1 << 0)
#define MODE_FILTER_MAX_RESOLUTION	(1 << 1)
#define MODE_FILTER_PIXEL_CLOCK		(1 << 2)
#define MODE_FILTER_DEX_MODE		(1 << 3)
#define MODE_FILTER_DEX_ADAPTER		(1 << 4)
#define MODE_FILTER_MIN_FPS		(1 << 5)
#define MODE_FILTER_MAX_FPS		(1 << 6)

#define MODE_FILTER_PRINT		(1 << 9)

#define MODE_FILTER_PREFER	(MODE_FILTER_PIXEL_CLOCK |\
				MODE_FILTER_MAX_RESOLUTION |\
				MODE_FILTER_TEST_SINK |\
				MODE_FILTER_PRINT)

#define MODE_FILTER_MIRROR	(MODE_FILTER_PIXEL_CLOCK |\
				MODE_FILTER_MAX_RESOLUTION |\
				MODE_FILTER_TEST_SINK)

#define MODE_FILTER_DEX		(MODE_FILTER_PIXEL_CLOCK |\
				MODE_FILTER_MAX_RESOLUTION |\
				MODE_FILTER_DEX_MODE |\
				MODE_FILTER_MIN_FPS |\
				MODE_FILTER_MAX_FPS)

#define MODE_MAX_RESOLUTION_WIDTH	4096
#define MODE_MAX_RESOLUTION_HEIGHT	4096
#define MODE_MIN_FPS	50
#define MODE_MAX_FPS	60
#endif

struct edid_data {
	int max_support_clk;
	bool support_10bpc;
	bool hdr_support;
	u8 eotf;
	u8 max_lumi_data;
	u8 max_average_lumi_data;
	u8 min_lumi_data;

	u8 edid_manufacturer[4];
	u32 edid_product;
	u32 edid_serial;

	int edid_data_size;
	u8 edid_buf[MAX_EDID_BLOCK * EDID_BLOCK_SIZE];
};

enum dp_state_for_hdcp22 {
	DP_DISCONNECT,
	DP_CONNECT,
	DP_HDCP_READY,
};

/* monitor aspect ratio */
enum mon_aspect_ratio_t {
	MON_RATIO_NA,
	MON_RATIO_3_2,
	MON_RATIO_4_3,
	MON_RATIO_5_3,
	MON_RATIO_5_4,
	MON_RATIO_10P5_9,
	MON_RATIO_11_10,
	MON_RATIO_16_9,
	MON_RATIO_16_10,
	MON_RATIO_21_9,
	MON_RATIO_21_10,
	MON_RATIO_32_9,
	MON_RATIO_32_10,
};


#ifdef FEATURE_DEX_SUPPORT
enum dex_state {
	DEX_OFF,
	DEX_ON,
	DEX_RECONNECTING,
};

enum dex_support_type {
	DEX_NOT_SUPPORT = 0,
	DEX_FHD_SUPPORT,
	DEX_WQHD_SUPPORT,
	DEX_UHD_SUPPORT
};

struct dex_support_modes {
	u32  active_h;
	u32  active_v;
	enum mon_aspect_ratio_t mon_ratio;
	enum dex_support_type support_type;
};

struct dex_data {
	enum dex_state ui_setting;
	enum dex_state ui_run;
	enum dex_state cur_state;
	enum dex_support_type adapter_type;
	struct drm_display_mode best_mode;
	bool skip_adapter_check;
};
#endif

#ifdef FEATURE_MANAGE_HMD_LIST
#define MAX_NUM_HMD	32
#define DEX_TAG_HMD	"HMD"

enum dex_hmd_type {
	DEX_HMD_MON = 0,	/* monitor name field */
	DEX_HMD_VID,		/* vid field */
	DEX_HMD_PID,		/* pid field */
	DEX_HMD_FIELD_MAX,
};
#endif

#define MON_NAME_LEN	14	/* monitor name */
struct secdp_sink_dev {
	u32 ven_id;		/* vendor id from PDIC */
	u32 prod_id;		/* product id from PDIC */
	char monitor_name[MON_NAME_LEN]; /* max 14 bytes, from EDID */
	u8 sw_ver[2];		/* sw revision */
};

#define phy_table 2
struct dp_device {
	struct drm_encoder encoder;
	struct drm_connector connector;

	enum exynos_drm_output_type output_type;

	struct device *dev;
	struct dp_resources res;

	spinlock_t slock;

	struct workqueue_struct *dp_wq;
	struct workqueue_struct *hdcp2_wq;
	struct delayed_work hpd_unplug_work;
	struct delayed_work hpd_irq_work;

	struct delayed_work hdcp13_work;
	struct delayed_work hdcp22_work;
	struct delayed_work hdcp13_integrity_check_work;
	int hdcp_ver;

	struct mutex cmd_lock;
	struct mutex hpd_lock;
	struct mutex training_lock;
	struct mutex hdcp2_lock;


	enum hotplug_state hpd_current_state;
	int gpio_sw_oe;
	int gpio_sw_sel;
	int gpio_usb_dir;
	int dfp_type;
	const char *aux_vdd;
	u32 phy_tune_set;

	int poor_connect_count;
	int auto_test_mode;

	int idle_ip_index;
	enum drm_state drm_start_state;
	enum drm_state drm_smc_state;

	bool decon_run;

	enum dp_state state;

	struct edid_data rx_edid_data;
	videoformat best_video;

	videoformat cur_video;
	enum dp_dynamic_range_type dyn_range;

#ifdef FEATURE_USE_DRM_EDID_PARSER
	int cur_mode_vic; /*VIC number of cur_mode */
	struct drm_display_mode cur_mode;
	struct drm_display_mode best_mode;
	struct drm_display_mode pref_mode;
	int cur_link_rate;
	int cur_lane_cnt;
	u32 cur_pix_clk; /*KHz: bandwidth of current link */
	bool fail_safe;
	bool test_sink;
#endif
	struct secdp_sink_dev sink_info;

#ifdef FEATURE_MANAGE_HMD_LIST
	struct secdp_sink_dev hmd_list[MAX_NUM_HMD];  /*list of supported HMD device*/
	struct mutex hmd_lock;
	bool is_hmd_dev;
#endif

	u8 bist_used;
	enum test_pattern bist_type;

	int audio_state;

	struct dentry *debug_root;
	struct dentry *debug_dump;
/* PDIC */
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct delayed_work notifier_register_work;
	struct notifier_block dp_typec_nb;
	int notifier_registered;
	bool pdic_link_conf;
	bool pdic_hpd;
	int pdic_cable_state;
#ifdef CONFIG_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
	struct list_head list_pd;
	struct delayed_work pdic_event_proceed_work;
	struct mutex pdic_lock;
	enum dp_wait_state dp_ready_wait_state;
	wait_queue_head_t dp_ready_wait;
#endif
#elif IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	struct delayed_work notifier_register_work;
	struct notifier_block dp_typec_nb;
	int notifier_registered;
	bool ifconn_link_conf;
	bool ifconn_hpd;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	struct notifier_block itmon_nb;
	bool itmon_notified;
#endif

	struct dp_cal_res cal_res;
#ifdef FEATURE_DEX_SUPPORT
	struct dex_data dex;
#endif
#ifdef CONFIG_SEC_DISPLAYPORT_SELFTEST
	void (*hpd_changed)(struct dp_device *dp, enum hotplug_state state);
#endif

	bool hdr_test_enable;
#ifdef CONFIG_SEC_DISPLAYPORT_DBG
	bool edid_test_enable;
#endif
};

struct dp_debug_param {
	u8 param_used;
	u8 link_rate;
	u8 lane_cnt;
};

/* EDID functions */
/* default preset configured on probe */
#define EDID_DEFAULT_TIMINGS_IDX (0) /* 640x480@60Hz */

#define EDID_ADDRESS 0x50
#define AUX_DATA_BUF_COUNT 16
#define AUX_RETRY_COUNT 3
#define AUX_TIMEOUT_1800us 0x03

#define DATA_BLOCK_TAG_CODE_MASK 0xE0
#define DATA_BLOCK_LENGTH_MASK 0x1F
#define DATA_BLOCK_TAG_CODE_BIT_POSITION 5

#define VSDB_TAG_CODE 3
#define HDMI14_IEEE_OUI_0 0x03
#define HDMI14_IEEE_OUI_1 0x0C
#define HDMI14_IEEE_OUI_2 0x00
#define IEEE_OUI_0_BYTE_NUM 1
#define IEEE_OUI_1_BYTE_NUM 2
#define IEEE_OUI_2_BYTE_NUM 3
#define VSDB_LATENCY_FILEDS_PRESETNT_MASK 0x80
#define VSDB_I_LATENCY_FILEDS_PRESETNT_MASK 0x40
#define VSDB_HDMI_VIDEO_PRESETNT_MASK 0x20
#define VSDB_VIC_FIELD_OFFSET 14
#define VSDB_VIC_LENGTH_MASK 0xE0
#define VSDB_VIC_LENGTH_BIT_POSITION 5

#define HDMI20_IEEE_OUI_0 0xD8
#define HDMI20_IEEE_OUI_1 0x5D
#define HDMI20_IEEE_OUI_2 0xC4
#define MAX_TMDS_RATE_BYTE_NUM 5
#define DC_SUPPORT_BYTE_NUM 7
#define DC_30BIT (0x01 << 0)

#define USE_EXTENDED_TAG_CODE 7
#define EXTENDED_HDR_TAG_CODE 6
#define EXTENDED_TAG_CODE_BYTE_NUM 1
#define SUPPORTED_EOTF_BYTE_NUM 2
#define SDR_LUMI (0x01 << 0)
#define HDR_LUMI (0x01 << 1)
#define SMPTE_ST_2084 (0x01 << 2)
#define MAX_LUMI_BYTE_NUM 4
#define MAX_AVERAGE_LUMI_BYTE_NUM 5
#define MIN_LUMI_BYTE_NUM 6

#define DETAILED_TIMING_DESCRIPTION_SIZE 18
#define AUDIO_DATA_BLOCK 1
#define VIDEO_DATA_BLOCK 2
#define SPEAKER_DATA_BLOCK 4
#define SVD_VIC_MASK 0x7F

typedef struct{
	u8 HDCP13_BKSV[5];
	u8 HDCP13_R0[2];
	u8 HDCP13_AKSV[5];
	u8 HDCP13_AN[8];
	u8 HDCP13_V_H[20];
	u8 HDCP13_BCAP[1];
	u8 HDCP13_BSTATUS[1];
	u8 HDCP13_BINFO[2];
	u8 HDCP13_KSV_FIFO[15];
	u8 HDCP13_AINFO[1];
} HDCP13;

enum HDCP13_STATE {
	HDCP13_STATE_NOT_AUTHENTICATED,
	HDCP13_STATE_AUTH_PROCESS,
	HDCP13_STATE_SECOND_AUTH_DONE,
	HDCP13_STATE_AUTHENTICATED,
	HDCP13_STATE_FAIL
};

struct hdcp13_info {
	u8 cp_irq_flag;
	u8 is_repeater;
	u8 device_cnt;
	u8 revocation_check;
	u8 r0_read_flag;
	int link_check;
	enum HDCP13_STATE auth_state;
};

/* HDCP 1.3 */
extern HDCP13 HDCP13_DPCD;
extern struct hdcp13_info hdcp13_info;

#define ADDR_HDCP13_BKSV 0x68000
#define ADDR_HDCP13_R0 0x68005
#define ADDR_HDCP13_AKSV 0x68007
#define ADDR_HDCP13_AN 0x6800C
#define ADDR_HDCP13_V_H0 0x68014
#define ADDR_HDCP13_V_H1 0x68018
#define ADDR_HDCP13_V_H2 0x6801C
#define ADDR_HDCP13_V_H3 0x68020
#define ADDR_HDCP13_V_H4 0x68024
#define ADDR_HDCP13_BCAP 0x68028
#define ADDR_HDCP13_BSTATUS 0x68029
#define ADDR_HDCP13_BINFO 0x6802A
#define ADDR_HDCP13_KSV_FIFO 0x6802C
#define ADDR_HDCP13_AINFO 0x6803B
#define ADDR_HDCP13_RSVD 0x6803C
#define ADDR_HDCP13_DBG 0x680C0

#define BCAPS_RESERVED_BIT_MASK 0xFC
#define BCAPS_REPEATER (1 << 1)
#define BCAPS_HDCP_CAPABLE (1 << 0)

#define BSTATUS_READY (1<<0)
#define BSTATUS_R0_AVAILABLE (1<<1)
#define BSTATUS_LINK_INTEGRITY_FAIL (1<<2)
#define BSTATUS_REAUTH_REQ (1<<3)

#define BINFO_DEVICE_COUNT (0x7F<<0)
#define BINFO_MAX_DEVS_EXCEEDED (1<<7)
#define BINFO_DEPTH (7<<8)
#define BINFO_MAX_CASCADE_EXCEEDED (1<<11)

#define RI_READ_RETRY_CNT 3
#define RI_AVAILABLE_WAITING 2
#define RI_DELAY 100
#define RI_WAIT_COUNT (RI_DELAY / RI_AVAILABLE_WAITING)
#define REPEATER_READY_MAX_WAIT_DELAY 5000
#define REPEATER_READY_WAIT_COUNT (REPEATER_READY_MAX_WAIT_DELAY / RI_AVAILABLE_WAITING)
#define HDCP_RETRY_COUNT 1
#define KSV_SIZE 5
#define KSV_FIFO_SIZE 15
#define MAX_KSV_LIST_COUNT 127
#define M0_SIZE 8
#define SHA1_SIZE 20
#define BINFO_SIZE 2
#define V_READ_RETRY_CNT 3

enum{
	LINK_CHECK_PASS = 0,
	LINK_CHECK_NEED = 1,
	LINK_CHECK_FAIL = 2,
};

enum{
	FIRST_AUTH  = 0,
	SECOND_AUTH = 1,
};

static inline struct dp_device *get_dp_drvdata(void)
{
	return dp_drvdata;
}

/* EXPORTED function and variable */
#if IS_ENABLED(CONFIG_PHY_EXYNOS_USBDRD)
extern void __iomem *dpphy_ctrl_addr;
extern void __iomem *dpphy_tca_addr;
extern int dwc3_otg_is_usb_ready(void);
#endif

/* register access subroutines */
int dp_debug_dump(void);
void dp_print_hex_dump(void __iomem *regs, const void *buf, size_t len);

static inline bool IS_DP_HPD_PLUG_STATE(struct dp_device *dp)
{
	return (dp->hpd_current_state == EXYNOS_HPD_UNPLUG) ? false : true;
}

void dp_dump_registers(struct dp_device *dp);
int dp_get_sst_id_with_port_number(u8 port_number, u8 get_sst_id_type);
#if 0
void dp_clr_vc_config(u32 ch,
		struct dp_device *dp);
int dp_calc_vc_config(u32 ch,
		struct dp_device *dp);
#endif
void dp_enable(struct drm_encoder *encoder);
void dp_disable(struct drm_encoder *encoder);

void dp_on_by_hpd_high(struct dp_device *dp);
void dp_off_by_hpd_low(struct dp_device *dp);
void dp_hpd_changed(struct dp_device *dp,
		enum hotplug_state state);

void dp_audio_bist_config(struct dp_device *dp,
		struct dp_audio_config_data audio_config_data);

int edid_read(struct dp_device *dp);
int edid_update(struct dp_device *dp);
struct v4l2_dv_timings edid_preferred_preset(void);
void edid_set_preferred_preset(int mode);
int edid_find_resolution(u16 xres, u16 yres, u16 refresh);
u8 edid_read_checksum(struct dp_device *dp);
u32 edid_audio_informs(struct dp_device *dp);
bool edid_support_pro_audio(void);

void hdcp13_run(struct dp_device *dp);
void hdcp13_dpcd_buffer(void);
u8 hdcp13_read_bcap(struct dp_device *dp);
void hdcp13_link_integrity_check(struct dp_device *dp);

extern int hdcp_calc_sha1(u8 *digest, const u8 *buf, unsigned int buflen);

u64 dp_get_cur_bandwidth(u8 link_rate, u8 lane_cnt, enum bit_depth bpc);
#ifdef FEATURE_USE_DRM_EDID_PARSER
int edid_update_drm(struct dp_device *dp);
bool dp_find_prefer_mode(struct dp_device *dp, int flag);
void dp_find_best_mode(struct dp_device *dp, int flag);
bool dp_mode_filter(struct dp_device *dp, struct drm_display_mode *mode, int flag);
void dp_mode_filter_out(struct dp_device *dp, int mode_num);
void dp_mode_clean_up(struct dp_device *dp);
void dp_mode_set_fail_safe(struct dp_device *dp);
void dp_mode_sort_for_mirror(struct dp_device *dp);
#ifdef FEATURE_DEX_SUPPORT
void dp_find_best_mode_for_dex(struct dp_device *dp);
#endif
#endif
#endif
