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

#ifndef IS_DEVICE_SENSOR_H
#define IS_DEVICE_SENSOR_H

#include <dt-bindings/camera/exynos_is_dt.h>

#include "exynos-is-sensor.h"
#include "pablo-mem.h"
#include "is-video.h"
#include "is-resourcemgr.h"
#include "is-groupmgr.h"
#include "is-device-csi.h"
#include "is-helper-i2c.h"

struct is_video_ctx;
struct is_device_ischain;

#define EXPECT_FRAME_START	0
#define EXPECT_FRAME_END	1
#define LOG_INTERVAL_OF_DROPS 30

#define FLITE_NOTIFY_FSTART		0
#define FLITE_NOTIFY_FEND		1
#define FLITE_NOTIFY_DMA_END		2
#define CSIS_NOTIFY_FSTART		3
#define CSIS_NOTIFY_FEND		4
#define CSIS_NOTIFY_DMA_END		5
#define CSIS_NOTIFY_DMA_END_VC_EMBEDDED	6
#define CSIS_NOTIFY_DMA_END_VC_MIPISTAT	7
#define CSIS_NOTIFY_LINE		8

#define SENSOR_MAX_ENUM			4 /* 4 modules(2 rear, 2 front) for same csis */
#define MODULE_SETFILE_PRELOAD_TRY	5
#define ACTUATOR_MAX_ENUM		IS_SENSOR_COUNT
#define SENSOR_DEFAULT_FRAMERATE	30

#define SENSOR_MODE_MASK		0xFFFF0000
#define SENSOR_MODE_SHIFT		16
#define SENSOR_MODE_DEINIT		0xFFFF

#define SENSOR_SCENARIO_MASK		0xF0000000
#define SENSOR_SCENARIO_SHIFT		28
#define ASB_SCENARIO_MASK		0xF000
#define ASB_SCENARIO_SHIFT        	12
#define SENSOR_MODULE_MASK		0x00000FFF
#define SENSOR_MODULE_SHIFT		0

#define SENSOR_SSTREAM_MASK		0x0000000F
#define SENSOR_SSTREAM_SHIFT		0
#define SENSOR_USE_STANDBY_MASK		0x000000F0
#define SENSOR_USE_STANDBY_SHIFT	4
#define SENSOR_INSTANT_MASK		0x0FFF0000
#define SENSOR_INSTANT_SHIFT		16
#define SENSOR_NOBLOCK_MASK		0xF0000000
#define SENSOR_NOBLOCK_SHIFT		28

#define SENSOR_I2C_CH_MASK		0xFF
#define SENSOR_I2C_CH_SHIFT		0
#define ACTUATOR_I2C_CH_MASK		0xFF00
#define ACTUATOR_I2C_CH_SHIFT		8
#define OIS_I2C_CH_MASK 		0xFF0000
#define OIS_I2C_CH_SHIFT		16

#define SENSOR_I2C_ADDR_MASK		0xFF
#define SENSOR_I2C_ADDR_SHIFT		0
#define ACTUATOR_I2C_ADDR_MASK		0xFF00
#define ACTUATOR_I2C_ADDR_SHIFT		8
#define OIS_I2C_ADDR_MASK		0xFF0000
#define OIS_I2C_ADDR_SHIFT		16

#define SENSOR_OTP_PAGE			256
#define SENSOR_OTP_PAGE_SIZE		64

#define SENSOR_SIZE_WIDTH_MASK		0xFFFF0000
#define SENSOR_SIZE_WIDTH_SHIFT		16
#define SENSOR_SIZE_HEIGHT_MASK		0xFFFF
#define SENSOR_SIZE_HEIGHT_SHIFT	0

#define IS_TIMESTAMP_HASH_KEY	20

struct is_sensor_subdev_ioctl_PD_arg {
	struct is_subdev *vc_dma;
	struct is_device_csi *csi;
	int frame_idx;
	bool start_end;
};

enum is_sensor_subdev_ioctl {
	SENSOR_IOCTL_DMA_CANCEL,
	SENSOR_IOCTL_PATTERN_ENABLE,
	SENSOR_IOCTL_PATTERN_DISABLE,
	SENSOR_IOCTL_REGISTE_VOTF,
	SENSOR_IOCTL_G_FRAME_ID,
	SENSOR_IOCTL_G_HW_FCOUNT,
	SENSOR_IOCTL_DISABLE_VC1_VOTF,
	SENSOR_IOCTL_CSI_DMA_DISABLE,
	SENSOR_IOCTL_CSI_DMA_ATTACH,
	SENSOR_IOCTL_G_DMA_CH,

	SENSOR_IOCTL_CSI_S_CTRL,
	SENSOR_IOCTL_CSI_G_CTRL,
	SENSOR_IOCTL_MOD_S_CTRL,
	SENSOR_IOCTL_MOD_G_CTRL,
	SENSOR_IOCTL_MOD_S_EXT_CTRL,
	SENSOR_IOCTL_MOD_G_EXT_CTRL,
	SENSOR_IOCTL_ACT_S_CTRL,
	SENSOR_IOCTL_ACT_G_CTRL,
	SENSOR_IOCTL_FLS_S_CTRL,
	SENSOR_IOCTL_FLS_G_CTRL,

	SENSOR_IOCTL_CSI_S_PDBUF_MASKING,
};

enum is_sensor_smc_state {
        IS_SENSOR_SMC_INIT = 0,
        IS_SENSOR_SMC_CFW_ENABLE,
        IS_SENSOR_SMC_PREPARE,
        IS_SENSOR_SMC_UNPREPARE,
};

enum is_sensor_output_entity {
	IS_SENSOR_OUTPUT_NONE = 0,
	IS_SENSOR_OUTPUT_FRONT,
};

enum is_sensor_force_stop {
	IS_BAD_FRAME_STOP = 0,
	IS_MIF_THROTTLING_STOP = 1,
	IS_FLITE_OVERFLOW_STOP = 2,
};

enum is_module_state {
	IS_MODULE_GPIO_ON,
	IS_MODULE_STANDBY_ON
};

enum is_sensor_state {
	IS_SENSOR_PROBE,
	IS_SENSOR_OPEN,
	IS_SENSOR_MCLK_ON,
	IS_SENSOR_ICLK_ON,
	IS_SENSOR_GPIO_ON,
	IS_SENSOR_S_INPUT,
	IS_SENSOR_ONLY,	/* SOC sensor, Iris sensor, Vision mode without IS chain */
	IS_SENSOR_FRONT_START,
	IS_SENSOR_FRONT_SNR_STOP,
	IS_SENSOR_BACK_START,
	IS_SENSOR_OTF_OUTPUT,
	IS_SENSOR_ITF_REGISTER,	/* to check whether sensor interfaces are registered */
	IS_SENSOR_WAIT_STREAMING,
	SENSOR_MODULE_GOT_INTO_TROUBLE,
	IS_SENSOR_ESD_RECOVERY,
	IS_SENSOR_ASSERT_CRASH,
	IS_SENSOR_S_POWER,
	IS_SENSOR_START,
};

#define IS_SENSOR_SEAMLESS_MODE_MASK	0xFF
enum is_sensor_seamless_state {
	/* Current seamless mode [7:0] */
	IS_SENSOR_SINGLE_MODE,
	IS_SENSOR_2EXP_MODE,
	IS_SENSOR_SWITCHING,

	/* Seamless mode change ischain shot state [11:8] */
	IS_SKIP_CHAIN_SHOT = 8,
	IS_DO_SHORT_CHAIN_SHOT,

	/* Seamless mode change sensor shot state [15:12] */
	IS_SKIP_SENSOR_SHOT = 12,
};

/*
 * Cal data status
 * [0]: NO ERROR
 * [1]: CRC FAIL
 * [2]: LIMIT FAIL
 * => Check AWB out of the ratio EEPROM/OTP data
 */

enum is_sensor_cal_status {
	CRC_NO_ERROR = 0,
	CRC_ERROR,
	LIMIT_FAILURE,
};

struct is_device_sensor {
	u32						instance;	/* logical stream id: decide at open time*/
	u32						device_id;	/* physical sensor node id: it is decided at probe time */
	u32						position;

	struct v4l2_device				v4l2_dev;
	struct platform_device				*pdev;

	struct is_image					image;

	struct is_video_ctx				*vctx;
	struct is_video					video;

	struct is_device_ischain   			*ischain;
	struct is_groupmgr				*groupmgr;
	struct is_resourcemgr				*resourcemgr;
	struct is_devicemgr				*devicemgr;
	struct is_module_enum				module_enum[SENSOR_MAX_ENUM];
	int						setfile_preload_retry;
	struct is_sensor_cfg				*cfg;

	/* current control value */
	struct camera2_sensor_ctl			sensor_ctl;
	struct camera2_lens_ctl				lens_ctl;
	struct camera2_flash_ctl			flash_ctl;
	u64						timestamp[IS_TIMESTAMP_HASH_KEY];
	u64						timestampboot[IS_TIMESTAMP_HASH_KEY];
	u64						frame_id[IS_TIMESTAMP_HASH_KEY]; /* index 0 ~ 7 */
	u64						prev_timestampboot;

	u32						fcount;
	u32						line_fcount;
	u32						instant_cnt;
	int						instant_ret;
	wait_queue_head_t				instant_wait;
	struct work_struct				instant_work;
	unsigned long					state;
	spinlock_t					slock_state;
	struct mutex					mlock_state;
	atomic_t					group_open_cnt;
	enum is_sensor_smc_state			smc_state;

	/* hardware configuration */
	struct v4l2_subdev				*subdev_module;
	struct v4l2_subdev				*subdev_csi;

	/* sensor dma video node */
	struct is_video					video_ssxvc0;
	struct is_video					video_ssxvc1;
	struct is_video					video_ssxvc2;
	struct is_video					video_ssxvc3;

	/* subdev for dma */
	struct is_subdev				ssvc0;
	struct is_subdev				ssvc1;
	struct is_subdev				ssvc2;
	struct is_subdev				ssvc3;
	struct is_subdev				bns;

	/* gain boost */
	int						min_target_fps;
	int						max_target_fps;
	int						scene_mode;

	/* for vision control */
	int						exposure_time;
	u64						frame_duration;

	/* Sensor No Response */
	bool						snr_check;
	struct timer_list				snr_timer;
	unsigned long					force_stop;

	/* for early buffer done */
	u32						early_buf_done_mode;
	struct hrtimer					early_buf_timer;

	struct exynos_platform_is_sensor		*pdata;
	atomic_t					module_count;
	struct v4l2_subdev 				*subdev_actuator[ACTUATOR_MAX_ENUM];
	struct is_actuator				*actuator[ACTUATOR_MAX_ENUM];
	struct v4l2_subdev				*subdev_flash;
	struct is_flash					*flash;
	struct v4l2_subdev				*subdev_ois;
	struct is_ois					*ois;
	struct v4l2_subdev				*subdev_mcu;
	struct is_mcu					*mcu;
	struct v4l2_subdev				*subdev_aperture;
	struct is_aperture				*aperture;
	struct v4l2_subdev				*subdev_eeprom;
	struct is_eeprom				*eeprom;
	struct v4l2_subdev				*subdev_laser_af;
	struct is_laser_af				*laser_af;
	void						*private_data;
	struct is_group					group_sensor;

	u32						sensor_width;
	u32						sensor_height;

	int						num_of_ch_mode;
	bool						dma_abstract;
	u32						use_standby;
	u32						sstream;
	u32						ex_mode;
	u32						ex_mode_extra;
	u32						ex_mode_format;
	u32						ex_scenario;

	/* backup AWB gains for use initial gain */
	float						init_wb[WB_GAIN_COUNT];
	float						last_wb[WB_GAIN_COUNT];
	float						chk_wb[WB_GAIN_COUNT];
	u32						init_wb_cnt;

#ifdef ENABLE_MODECHANGE_CAPTURE
	struct is_frame					*mode_chg_frame;
#endif

	bool					use_otp_cal;
	u32					cal_status[CAMERA_CRC_INDEX_MAX];
	u8					otp_cal_buf[SENSOR_OTP_PAGE][SENSOR_OTP_PAGE_SIZE];

	struct i2c_client			*client;
	struct mutex				mutex_reboot;
	bool					reboot;

	bool					use_stripe_flag;

	/* for set timeout */
	int						exposure_value[IS_EXP_BACKUP_COUNT];
	u32						exposure_fcount[IS_EXP_BACKUP_COUNT];
	u32						obte_config;

	ulong						seamless_state;
};

int is_sensor_open(struct is_device_sensor *device,
	struct is_video_ctx *vctx);
int is_sensor_close(struct is_device_sensor *device);
#ifdef CONFIG_USE_SENSOR_GROUP
int is_sensor_s_input(struct is_device_sensor *device,
	u32 position,
	u32 scenario,
	u32 video_id,
	u32 stream_leader);
#else
int is_sensor_s_input(struct is_device_sensor *device,
	u32 input,
	u32 scenario);
#endif
int is_sensor_s_ctrl(struct is_device_sensor *device,
	struct v4l2_control *ctrl);
int is_sensor_s_ext_ctrls(struct is_device_sensor *device,
	struct v4l2_ext_controls *ctrls);
int is_sensor_subdev_buffer_queue(struct is_device_sensor *device,
	enum is_subdev_id subdev_id,
	u32 index);
int is_sensor_buffer_queue(struct is_device_sensor *device,
	struct is_queue *queue,
	u32 index);
int is_sensor_buffer_finish(struct is_device_sensor *device,
	u32 index);

int is_sensor_front_start(struct is_device_sensor *device,
	u32 instant_cnt,
	u32 nonblock);
int is_sensor_front_stop(struct is_device_sensor *device, bool fstop);
void is_sensor_group_force_stop(struct is_device_sensor *device, u32 group_id);

int is_sensor_s_framerate(struct is_device_sensor *device,
	struct v4l2_streamparm *param);
int is_sensor_s_bns(struct is_device_sensor *device,
	u32 width, u32 height);

int is_sensor_s_frame_duration(struct is_device_sensor *device,
	u32 frame_duration);
int is_sensor_s_exposure_time(struct is_device_sensor *device,
	u32 exposure_time);
int is_sensor_s_again(struct is_device_sensor *device, u32 gain);
int is_sensor_s_shutterspeed(struct is_device_sensor *device, u32 shutterspeed);

struct is_sensor_cfg * is_sensor_g_mode(struct is_device_sensor *device);
int is_sensor_mclk_on(struct is_device_sensor *device, u32 scenario, u32 channel, u32 freq);
int is_sensor_mclk_off(struct is_device_sensor *device, u32 scenario, u32 channel);
int is_sensor_gpio_on(struct is_device_sensor *device);
int is_sensor_gpio_off(struct is_device_sensor *device);
int is_sensor_gpio_dbg(struct is_device_sensor *device);
void is_sensor_dump(struct is_device_sensor *device);

int is_sensor_g_ctrl(struct is_device_sensor *device,
	struct v4l2_control *ctrl);
int is_sensor_g_ext_ctrls(struct is_device_sensor *device,
	struct v4l2_ext_controls *ctrls);
int is_sensor_g_instance(struct is_device_sensor *device);
int is_sensor_g_ex_mode(struct is_device_sensor *device);
int is_sensor_g_framerate(struct is_device_sensor *device);
int is_sensor_g_fcount(struct is_device_sensor *device);
int is_sensor_g_width(struct is_device_sensor *device);
int is_sensor_g_height(struct is_device_sensor *device);
int is_sensor_g_bns_width(struct is_device_sensor *device);
int is_sensor_g_bns_height(struct is_device_sensor *device);
int is_sensor_g_bns_ratio(struct is_device_sensor *device);
int is_sensor_g_bratio(struct is_device_sensor *device);
int is_sensor_g_sensorcrop_bratio(struct is_device_sensor *device);
int is_sensor_g_module(struct is_device_sensor *device,
	struct is_module_enum **module);
int is_sensor_deinit_module(struct is_module_enum *module);
int is_sensor_g_position(struct is_device_sensor *device);
int is_sensor_g_fast_mode(struct is_device_sensor *device);
int is_search_sensor_module_with_position(struct is_device_sensor *device,
	u32 position, struct is_module_enum **module);
int is_search_sensor_module(struct is_device_sensor *device, struct is_module_enum **module);
int is_sensor_votf_tag(struct is_device_sensor *device, struct is_subdev *subdev);
int is_sensor_dm_tag(struct is_device_sensor *device,
	struct is_frame *frame);
int is_sensor_buf_tag(struct is_device_sensor *device,
	struct is_subdev *f_subdev,
	struct v4l2_subdev *v_subdev,
	struct is_frame *ldr_frame);
int is_sensor_buf_skip(struct is_device_sensor *device,
	struct is_subdev *f_subdev,
	struct v4l2_subdev *v_subdev,
	struct is_frame *ldr_frame);
int is_sensor_g_csis_error(struct is_device_sensor *device);
int is_sensor_register_itf(struct is_device_sensor *device);
void is_sensor_s_batch_mode(struct is_device_sensor *device, struct is_frame *frame);
int is_sensor_group_tag(struct is_device_sensor *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node);
int is_sensor_dma_cancel(struct is_device_sensor *device);
struct is_queue_ops *is_get_sensor_device_qops(void);
int is_sensor_map_sensor_module(struct is_device_ischain *device, int position);
void is_sensor_g_max_size(u32 *max_width, u32 *max_height);
bool is_sensor_g_aeb_mode(struct is_device_sensor *sensor);
#define CALL_MOPS(s, op, args...) (((s)->ops) ? (((s)->ops->op) ? ((s)->ops->op(args)) : 0) : 0)

#define VOTF_BACK_FIRST_OFF_COND(group, cfg)			\
	((IS_ENABLED(VOTF_BACK_FIRST_OFF))			\
	&& (!IS_ENABLED(CONFIG_VOTF_ONESHOT))			\
	&& (test_bit(IS_GROUP_VOTF_OUTPUT, &group->state))	\
	&& (!CHECK_SDC_EN(cfg->input[CSI_VIRTUAL_CH_0].extformat)))

#endif
