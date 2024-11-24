/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for SAMSUNG DPP CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_DP_CAL_H__
#define __SAMSUNG_DP_CAL_H__

/* For Displayport video timming calculation */
#include <media/v4l2-dv-timings.h>
#include <uapi/linux/v4l2-dv-timings.h>

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#include <linux/usb/typec/common/pdic_notifier.h>
#elif IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
#include <linux/ifconn/ifconn_notifier.h>
#endif

#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
#include "../../../../../../drivers/phy/samsung/phy-samsung-usb-cal.h"
#include "../../../../../../drivers/phy/samsung/phy-exynos-usbdp.h"
#include <linux/usb/otg-fsm.h>
#include "../../../../../../drivers/usb/dwc3/dwc3-exynos.h"
#endif

#include <linux/soc/samsung/exynos-soc.h>

/* Display HW enum definition */
enum dp_dynamic_range_type {
	VESA_RANGE = 0,   /* (0 ~ 255) */
	CEA_RANGE = 1,    /* (16 ~ 235) */
};

enum dp_aux_ch_command_type {
	I2C_WRITE = 0x4,
	I2C_READ = 0x5,
	DPCD_WRITE = 0x8,
	DPCD_READ = 0x9,
};

enum dp_interrupt_mask {
	PLL_LOCK_CHG_INT_MASK,
	HOTPLUG_CHG_INT_MASK,
	HPD_LOST_INT_MASK,
	PLUG_INT_MASK,
	HPD_IRQ_INT_MASK,
	RPLY_RECEIV_INT_MASK,
	AUX_ERR_INT_MASK,
	HDCP_LINK_CHECK_INT_MASK,
	HDCP_LINK_FAIL_INT_MASK,
	HDCP_R0_READY_INT_MASK,
	VIDEO_FIFO_UNDER_FLOW_MASK,
	VSYNC_DET_INT_MASK,
	AUDIO_FIFO_UNDER_RUN_INT_MASK,
	AUDIO_FIFO_OVER_RUN_INT_MASK,

	ALL_INT_MASK
};

enum sst_id {
	SST1 = 0,
	SST2,
	SST_MAX
};

enum dp_regs_type {
	REGS_LINK = 0,
	REGS_PHY,
	REGS_PHY_TCA,
	REGS_DP_TYPE_MAX
};

typedef enum {
	NORAMAL_DATA = 0,
	TRAINING_PATTERN_1 = 1,
	TRAINING_PATTERN_2 = 2,
	TRAINING_PATTERN_3 = 3,
	TRAINING_PATTERN_4 = 5,
} dp_training_pattern;

typedef enum {
	DISABLE_PATTEN = 0,
	D10_2_PATTERN = 1,
	SERP_PATTERN = 2,
	PRBS7 = 3,
	CUSTOM_80BIT = 4,
	HBR2_COMPLIANCE = 5,
} dp_qual_pattern;

typedef enum {
	ENABLE_SCRAM = 0,
	DISABLE_SCRAM = 1,
} dp_scrambling;


#define CONNETED_DECON_ID 2

#define MAX_LANE_CNT 4
#define MAX_LINK_RATE_NUM 4
#define RBR_PIXEL_CLOCK_PER_LANE 48600000ULL /* hz */
#define HBR_PIXEL_CLOCK_PER_LANE 85500000ULL /* hz */
#define HBR2_PIXEL_CLOCK_PER_LANE 171000000ULL /* hz */
#define HBR3_PIXEL_CLOCK_PER_LANE 256500000ULL /* hz */
#define DPCD_BUF_SIZE 12

#define FB_AUDIO_LPCM	1

#define FB_AUDIO_192KHZ	(1 << 6)
#define FB_AUDIO_176KHZ	(1 << 5)
#define FB_AUDIO_96KHZ	(1 << 4)
#define FB_AUDIO_88KHZ	(1 << 3)
#define FB_AUDIO_48KHZ	(1 << 2)
#define FB_AUDIO_44KHZ	(1 << 1)
#define FB_AUDIO_32KHZ	(1 << 0)

#define FB_AUDIO_24BIT	(1 << 2)
#define FB_AUDIO_20BIT	(1 << 1)
#define FB_AUDIO_16BIT	(1 << 0)

#define FB_AUDIO_1CH (1)
#define FB_AUDIO_2CH (1 << 1)
#define FB_AUDIO_6CH (1 << 5)
#define FB_AUDIO_8CH (1 << 7)
#define FB_AUDIO_1N2CH (FB_AUDIO_1CH | FB_AUDIO_2CH)

struct fb_audio {
	u8 format;
	u8 channel_count;
	u8 sample_rates;
	u8 bit_rates;
	u8 speaker;
};

struct fb_vendor {
	u8 vic_len;
	u8 vic_data[16];
};

#define SYNC_POSITIVE 0
#define SYNC_NEGATIVE 1

#define AUDIO_BUF_FULL_SIZE 40
#define AUDIO_DISABLE 0
#define AUDIO_ENABLE 1
#define AUDIO_WAIT_BUF_FULL 2
#define AUDIO_DMA_REQ_HIGH 3

enum phy_tune_info {
	EQ_MAIN = 0,
	EQ_PRE = 1,
	EQ_POST = 2,
	EQ_VSWING = 3,
	RBOOST = 4,
};

typedef enum {
	V640X480P60,
	V720X480P60,
	V720X576P50,
	V1280X800P60RB,
	V1280X720P50,
	V1280X720P60,
	V1366X768P60,
	V1280X1024P60,
	V1920X1080P24,
	V1920X1080P25,
	V1920X1080P30,
	V1600X900P59,
	V1600X900P60RB,
	V1920X1080P50,
	V1920X1080P59,
	V1920X1080P60,
	V2048X1536P60,
	V1920X1440P60,
	V2560X1440P59,
	V1440x2560P60,
	V1440x2560P75,
	V2560X1440P60,
	V2560X1440P120,
	V3840X2160P24,
	V3840X2160P25,
	V3840X2160P30,
	V4096X2160P24,
	V4096X2160P25,
	V4096X2160P30,
	V3840X2160P59RB,
	V3840X2160P50,
	V3840X2160P60,
	V4096X2160P50,
	V4096X2160P60,
	V5120X1440P60,
	V640X10P60SACRC,
	VDUMMYTIMING,
} videoformat;

typedef enum {
	ASYNC_MODE = 0,
	SYNC_MODE,
} audio_sync_mode;

enum audio_sampling_frequency {
	FS_32KHZ	= 0,
	FS_44KHZ	= 1,
	FS_48KHZ	= 2,
	FS_88KHZ	= 3,
	FS_96KHZ	= 4,
	FS_176KHZ	= 5,
	FS_192KHZ	= 6,
};

enum audio_bit_per_channel {
	AUDIO_16_BIT = 0,
	AUDIO_20_BIT,
	AUDIO_24_BIT,
};

enum audio_16bit_dma_mode {
	NORMAL_MODE = 0,
	PACKED_MODE = 1,
	PACKED_MODE2 = 2,
};

enum audio_dma_word_length {
	WORD_LENGTH_1 = 0,
	WORD_LENGTH_2,
	WORD_LENGTH_3,
	WORD_LENGTH_4,
	WORD_LENGTH_5,
	WORD_LENGTH_6,
	WORD_LENGTH_7,
	WORD_LENGTH_8,
};

enum audio_clock_accuracy {
	Level2 = 0,
	Level1 = 1,
	Level3 = 2,
	NOT_MATCH = 3,
};

enum bit_depth {
	BPC_6 = 0,
	BPC_8,
	BPC_10,
	BPC_12,
	BPC_16,
};

enum test_pattern {
	COLOR_BAR = 0,
	WGB_BAR,
	MW_BAR,
	CTS_COLOR_RAMP,
	CTS_BLACK_WHITE,
	CTS_COLOR_SQUARE_VESA,
	CTS_COLOR_SQUARE_CEA,
};

struct dp_audio_config_data {
	u32 audio_enable;
	u32 audio_channel_cnt;
	enum audio_sampling_frequency audio_fs;
	enum audio_bit_per_channel audio_bit;
	enum audio_16bit_dma_mode audio_packed_mode;
	enum audio_dma_word_length audio_word_length;
};

/* Displayport Timing detail */
struct dp_supported_preset {
	videoformat video_format;
	struct v4l2_dv_timings dv_timings;
	u32 fps;
	u32 v_sync_pol;
	u32 h_sync_pol;
	u8 vic;
	char *name;
	bool pro_audio_support;
	bool edid_support_match;
};

#define V4L2_DV_BT_CVT_3840X2160P59_ADDED { \
	.type = V4L2_DV_BT_656_1120, \
	V4L2_INIT_BT_TIMINGS(3840, 2160, 0, V4L2_DV_HSYNC_POS_POL, \
		533250000, 48, 32, 80, 3, 5, 54, 0, 0, 0, \
		V4L2_DV_BT_STD_DMT | V4L2_DV_BT_STD_CVT, \
		V4L2_DV_FL_REDUCED_BLANKING) \
}

#define V4L2_DV_BT_CVT_2560X1440P120_ADDED { \
	.type = V4L2_DV_BT_656_1120, \
	V4L2_INIT_BT_TIMINGS(2560, 1440, 0, V4L2_DV_HSYNC_POS_POL, \
		497750000, 48, 32, 80, 3, 5, 77, 0, 0, 0, \
		V4L2_DV_BT_STD_DMT | V4L2_DV_BT_STD_CVT, 0) \
}

#define V4L2_DV_BT_CVT_5120X1440P60_ADDED { \
	.type = V4L2_DV_BT_656_1120, \
	V4L2_INIT_BT_TIMINGS(5120, 1440, 0, V4L2_DV_HSYNC_POS_POL, \
		468990000, 48, 32, 80, 3, 10, 28, 0, 0, 0, \
		V4L2_DV_BT_STD_DMT | V4L2_DV_BT_STD_CVT, \
		V4L2_DV_FL_REDUCED_BLANKING) \
}

#define V4L2_DV_BT_CVT_2560X1440P60_ADDED { \
	.type = V4L2_DV_BT_656_1120, \
	V4L2_INIT_BT_TIMINGS(2560, 1440, 0, V4L2_DV_HSYNC_POS_POL, \
		312250000, 192, 272, 464, 3, 5, 45, 0, 0, 0, \
		V4L2_DV_BT_STD_DMT | V4L2_DV_BT_STD_CVT, 0) \
}

#define V4L2_DV_BT_CVT_2560X1440P59_ADDED { \
	.type = V4L2_DV_BT_656_1120, \
	V4L2_INIT_BT_TIMINGS(2560, 1440, 0, V4L2_DV_HSYNC_POS_POL, \
		241500000, 48, 32, 80, 3, 5, 33, 0, 0, 0, \
		V4L2_DV_BT_STD_DMT | V4L2_DV_BT_STD_CVT, 0) \
}

#define V4L2_DV_BT_CVT_2048X1536P60_ADDED { \
	.type = V4L2_DV_BT_656_1120, \
	V4L2_INIT_BT_TIMINGS(2048, 1536, 0, V4L2_DV_HSYNC_POS_POL, \
		209250000, 48, 32, 80, 3, 4, 37, 0, 0, 0, \
		V4L2_DV_BT_STD_DMT | V4L2_DV_BT_STD_CVT, 0) \
}

#define V4L2_DV_BT_CVT_1440X2560P75_ADDED { \
	.type = V4L2_DV_BT_656_1120, \
	V4L2_INIT_BT_TIMINGS(1440, 2560, 0, V4L2_DV_HSYNC_POS_POL, \
		307000000, 10, 8, 120, 6, 2, 26, 0, 0, 0, \
		V4L2_DV_BT_STD_DMT | V4L2_DV_BT_STD_CVT, 0) \
}

#define V4L2_DV_BT_CVT_1440X2560P60_ADDED { \
	.type = V4L2_DV_BT_656_1120, \
	V4L2_INIT_BT_TIMINGS(1440, 2560, 0, V4L2_DV_HSYNC_POS_POL, \
		246510000, 32, 10, 108, 6, 2, 16, 0, 0, 0, \
		V4L2_DV_BT_STD_DMT | V4L2_DV_BT_STD_CVT, 0) \
}

#define V4L2_DV_BT_CVT_1600X900P59_ADDED { \
	.type = V4L2_DV_BT_656_1120, \
	V4L2_INIT_BT_TIMINGS(1600, 900, 0, \
		V4L2_DV_HSYNC_POS_POL | V4L2_DV_VSYNC_POS_POL, \
		97750000, 48, 32, 80, 3, 5, 18, 0, 0, 0, \
		V4L2_DV_BT_STD_DMT, V4L2_DV_FL_REDUCED_BLANKING) \
}

#define V4L2_DV_BT_CVT_1920X1080P59_ADDED { \
	.type = V4L2_DV_BT_656_1120, \
	V4L2_INIT_BT_TIMINGS(1920, 1080, 0, V4L2_DV_HSYNC_POS_POL, \
		138500000, 48, 44, 68, 3, 5, 23, 0, 0, 0, \
		V4L2_DV_BT_STD_DMT | V4L2_DV_BT_STD_CVT, 0) \
}

#define V4L2_DV_BT_CVT_640x10P60_ADDED { \
	.type = V4L2_DV_BT_656_1120, \
	V4L2_INIT_BT_TIMINGS(640, 10, 0, V4L2_DV_HSYNC_POS_POL, \
		27000000, 16, 96, 48, 2, 2, 12, 0, 0, 0, \
		V4L2_DV_BT_STD_DMT | V4L2_DV_BT_STD_CVT, 0) \
}

extern const int supported_videos_pre_cnt;
extern struct dp_supported_preset supported_videos[];

struct exynos_dp_data {
	enum {
		EXYNOS_DP_STATE_PRESET = 0,
		EXYNOS_DP_STATE_ENUM_PRESET,
		EXYNOS_DP_STATE_RECONNECTION,
		EXYNOS_DP_STATE_HDR_INFO,
		EXYNOS_DP_STATE_AUDIO,
	} state;
	struct	v4l2_dv_timings timings;
	struct	v4l2_enum_dv_timings etimings;
	__u32	audio_info;
	int hdr_support;
};


/* InfoFrame */
#define	INFOFRAME_PACKET_TYPE_AVI 0x82		/** Auxiliary Video information InfoFrame */
#define INFOFRAME_PACKET_TYPE_AUDIO 0x84	/** Audio information InfoFrame */
#define INFOFRAME_PACKET_TYPE_HDR 0x87		/** HDR Metadata InfoFrame */
#define MAX_INFOFRAME_LENGTH 27
#define INFOFRAME_REGISTER_SIZE 32
#define INFOFRAME_DATA_SIZE 8
#define DATA_NUM_PER_REG (INFOFRAME_REGISTER_SIZE / INFOFRAME_DATA_SIZE)

#define AVI_INFOFRAME_VERSION 0x02
#define AVI_INFOFRAME_LENGTH 0x0D
#define ACTIVE_FORMAT_INFOMATION_PRESENT (1 << 4)	/* No Active Format Infomation */
#define ACITVE_PORTION_ASPECT_RATIO (0x8 << 0)		/* Same as Picture Aspect Ratio */

#define AUDIO_INFOFRAME_VERSION 0x01
#define AUDIO_INFOFRAME_LENGTH 0x0A
#define AUDIO_INFOFRAME_PCM (1 << 4)
#define AUDIO_INFOFRAME_SF_BIT_POSITION 2

#define MASTER_AUDIO_BUFFER_LEVEL_BIT_POS	(5)

#define HDR_INFOFRAME_VERSION 0x01
#define HDR_INFOFRAME_LENGTH 26
#define HDR_INFOFRAME_EOTF_BYTE_NUM 0
#define HDR_INFOFRAME_SMPTE_ST_2084 2
#define STATIC_MATADATA_TYPE_1 0
#define HDR_INFOFRAME_METADATA_ID_BYTE_NUM 1
#define HDR_INFOFRAME_DISP_PRI_X_0_LSB 2
#define HDR_INFOFRAME_DISP_PRI_X_0_MSB 3
#define HDR_INFOFRAME_DISP_PRI_Y_0_LSB 4
#define HDR_INFOFRAME_DISP_PRI_Y_0_MSB 5
#define HDR_INFOFRAME_DISP_PRI_X_1_LSB 6
#define HDR_INFOFRAME_DISP_PRI_X_1_MSB 7
#define HDR_INFOFRAME_DISP_PRI_Y_1_LSB 8
#define HDR_INFOFRAME_DISP_PRI_Y_1_MSB 9
#define HDR_INFOFRAME_DISP_PRI_X_2_LSB 10
#define HDR_INFOFRAME_DISP_PRI_X_2_MSB 11
#define HDR_INFOFRAME_DISP_PRI_Y_2_LSB 12
#define HDR_INFOFRAME_DISP_PRI_Y_2_MSB 13
#define HDR_INFOFRAME_WHITE_POINT_X_LSB 14
#define HDR_INFOFRAME_WHITE_POINT_X_MSB 15
#define HDR_INFOFRAME_WHITE_POINT_Y_LSB 16
#define HDR_INFOFRAME_WHITE_POINT_Y_MSB 17
#define HDR_INFOFRAME_MAX_LUMI_LSB 18
#define HDR_INFOFRAME_MAX_LUMI_MSB 19
#define HDR_INFOFRAME_MIN_LUMI_LSB 20
#define HDR_INFOFRAME_MIN_LUMI_MSB 21
#define HDR_INFOFRAME_MAX_LIGHT_LEVEL_LSB 22
#define HDR_INFOFRAME_MAX_LIGHT_LEVEL_MSB 23
#define HDR_INFOFRAME_MAX_AVERAGE_LEVEL_LSB 24
#define HDR_INFOFRAME_MAX_AVERAGE_LEVEL_MSB 25
#define LSB_MASK 0x00FF
#define MSB_MASK 0xFF00
#define SHIFT_8BIT 8

struct infoframe {
	u8 type_code;
	u8 version_number;
	u8 length;
	u8 data[MAX_INFOFRAME_LENGTH];
};

/* displayport CAL resource structure definition
 * It can be used CAL function and driver layer
 */
struct dp_cal_res {
/* PDIC */
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	pdic_notifier_dp_pinconf_t pdic_notify_dp_conf;
#elif IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	ifconn_notifier_dp_pinconf_t ifconn_notify_dp_conf;
#endif
/* Type C gpio */
	int dp_sw_sel;
/* DP link HW */
	enum bit_depth bpc;
	u32 g_link_rate;
/* Audio */
	int audio_buf_empty_check;
/* Aux lock - Anxious layer */
	struct mutex aux_lock;
/* USBDP combo phy enable ref count */
	atomic_t usbdp_phy_en_cnt;
};

/* Debug for External DEVICE */
int dp_hsi0_domain_power_check(void);
void dp_hsi0_clk_check(void);

/* DP APB and CR interface */
void dp_reg_cr_write_mask(u16 addr, u16 data, u16 mask);
void dp_reg_cr_write(u16 addr, u16 data);
u16 dp_reg_cr_read(u16 addr, int *err);
inline u32 dp_read(u32 reg_id);
inline u32 dp_read_mask(u32 reg_id, u32 mask);
inline void dp_write(u32 reg_id, u32 val);
inline void dp_write_mask(u32 reg_id, u32 val, u32 mask);
inline void dp_formal_write_mask(u32 reg_id, u32 val, u32 mask);
inline u32 dp_phy_read(u32 reg_id);
inline u32 dp_phy_read_mask(u32 reg_id, u32 mask);
inline void dp_phy_write(u32 reg_id, u32 val);
inline void dp_phy_write_mask(u32 reg_id, u32 val, u32 mask);
inline void dp_phy_formal_write_mask(u32 reg_id, u32 val, u32 mask);
inline u32 dp_phy_tca_read(u32 reg_id);
inline u32 dp_phy_tca_read_mask(u32 reg_id, u32 mask);
inline void dp_phy_tca_write(u32 reg_id, u32 val);
inline void dp_phy_tca_write_mask(u32 reg_id, u32 val, u32 mask);
inline void dp_phy_tca_formal_write_mask(u32 reg_id, u32 val, u32 mask);


/* DP memory map interface */
void dp_regs_desc_init(void __iomem *regs, const char *name,
		enum dp_regs_type type, unsigned int id);

/* DP register control interface */
void dp_reg_lh_p_ch_power(u32 en);
void dp_reg_init(struct dp_cal_res *cal_res);
void dp_reg_deinit(void);
void dp_reg_sw_reset(struct dp_cal_res *cal_res);
void dp_reg_osc_clk_qch_con(unsigned int en);
void dp_reg_set_osc_clk(unsigned int mhz);
void dp_reg_set_common_interrupt_mask(enum dp_interrupt_mask param, u8 set);
void dp_reg_set_interrupt_mask(enum dp_interrupt_mask param, u8 set);
u32 dp_reg_get_common_interrupt_and_clear(void);
u32 dp_reg_get_video_interrupt_and_clear(void);
u32 dp_reg_get_audio_interrupt_and_clear(void);
void dp_reg_start(void);
void dp_reg_video_mute(u32 en);
void dp_reg_stop(void);
void dp_reg_set_video_config(struct dp_cal_res *cal_res, videoformat video_format,
		u8 bpc, u8 range);
int dp_reg_dpcd_write(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data);
int dp_reg_dpcd_read(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data);
int dp_reg_dpcd_write_burst(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data);
int dp_reg_dpcd_read_burst(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data);
int dp_reg_edid_write(u8 edid_addr_offset, u32 length, u8 *data);
#ifdef FEATURE_USE_DRM_EDID_PARSER
int dp_reg_edid_read(struct dp_cal_res *cal_res, u8 block_cnt, u32 length, u8 *data);
#else
int dp_reg_edid_read(struct dp_cal_res *cal_res, u8 edid_addr_offset, u32 length, u8 *data);
#endif
int dp_reg_i2c_read(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data);
int dp_reg_i2c_write(struct dp_cal_res *cal_res, u32 address, u32 length, u8 *data);

/* DP PHY register control interface */
void dp_reg_phy_reset(u32 en);
void dp_reg_phy_disable(struct dp_cal_res *cal_res);
void dp_reg_phy_init_setting(void);
void dp_reg_phy_mode_setting(struct dp_cal_res *cal_res);
void dp_reg_phy_post_init(struct dp_cal_res *cal_res);
void dp_reg_phy_ssc_enable(u32 en);
void dp_reg_phy_set_link_bw(struct dp_cal_res *cal_res, u8 link_rate);
u32 dp_reg_phy_get_link_bw(struct dp_cal_res *cal_res);
void dp_reg_set_lane_count(u8 lane_cnt);
u32 dp_reg_get_lane_count(void);
void dp_reg_wait_phy_pll_lock(void);
void dp_reg_set_training_pattern(dp_training_pattern pattern);
void dp_reg_set_qual_pattern(dp_qual_pattern pattern, dp_scrambling scramble);
void dp_reg_set_hbr2_scrambler_reset(u32 uResetCount);
void dp_reg_set_pattern_PLTPAT(void);
void dp_reg_set_voltage_and_pre_emphasis(struct dp_cal_res *cal_res, u8 *voltage, u8 *pre_emphasis);
void dp_reg_set_bist_video_config(struct dp_cal_res *cal_res, videoformat video_format,
		u8 bpc, u8 type, u8 range);
void dp_reg_set_bist_video_config_for_blue_screen(struct dp_cal_res *cal_res, videoformat video_format);
void dp_reg_set_video_bist_mode(u32 en);
void dp_reg_set_audio_bist_mode(u32 en);
u32 dp_reg_get_video_clk(void);
u32 dp_reg_get_ls_clk(struct dp_cal_res *cal_res);
int dp_reg_stand_alone_crc_sorting(struct dp_cal_res *cal_res);

/* DP audio register control interface */
void dp_reg_audio_enable(struct dp_cal_res *cal_res, struct dp_audio_config_data *audio_config_data);
void dp_reg_audio_disable(void);
void dp_reg_audio_wait_buf_full(void);
void dp_reg_audio_bist_enable(struct dp_cal_res *cal_res, struct dp_audio_config_data audio_config_data);
void dp_reg_audio_init_config(struct dp_cal_res *cal_res);
void dp_reg_print_audio_state(void);
void dp_reg_set_dma_req_gen(u32 en);

/* DP HDCP22 register control interface */
void dp_reg_set_hdcp22_system_enable(u32 en);
void dp_reg_set_hdcp22_mode(u32 en);
void dp_reg_set_hdcp22_encryption_enable(u32 en);
u32 dp_reg_get_hdcp22_encryption_enable(void);

/* DP auxiliary register control interface */
void dp_reg_set_aux_pn_inv(u32 val);

/* DP infoframe register control interface */
void dp_reg_set_avi_infoframe(struct infoframe avi_infofrmae);
void dp_reg_set_spd_infoframe(struct infoframe spd_infofrmae);
void dp_reg_set_audio_infoframe(struct infoframe audio_infofrmae, u32 en);
void dp_reg_set_hdr_infoframe(struct infoframe hdr_infofrmae, u32 en);

/* External API, It can be used by driver layer and CAL */
extern int exynos_usbdrd_inform_dp_use(int use, int lane_cnt);

#endif /* __SAMSUNG_DP_CAL_H__ */
