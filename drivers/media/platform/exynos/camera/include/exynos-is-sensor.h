/*
 * /include/media/exynos_is_sensor.h
 *
 * Copyright (C) 2012 Samsung Electronics, Co. Ltd
 *
 * Exynos series exynos_is_sensor device support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MEDIA_EXYNOS_SENSOR_H
#define MEDIA_EXYNOS_SENSOR_H

#include <dt-bindings/camera/exynos_is_dt.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>

#define IS_SENSOR_DEV_NAME "exynos-is-sensor"
#define IS_PINNAME_LEN 32
#define IS_MAX_NAME_LEN 32

#define CSI_VIRTUAL_CH_0	0
#define CSI_VIRTUAL_CH_1	1
#define CSI_VIRTUAL_CH_2	2
#define CSI_VIRTUAL_CH_3	3
#define CSI_VIRTUAL_CH_MAX	4
#define DMA_VIRTUAL_CH_MAX	4
#define INIT_WDMA_CH_HINT	U16_MAX

enum exynos_csi_id {
	CSI_ID_A = 0,
	CSI_ID_B = 1,
	CSI_ID_C = 2,
	CSI_ID_D = 3,
	CSI_ID_E = 4,
	CSI_ID_F = 5,
	CSI_ID_G = 6,
	CSI_ID_MAX
};

enum exynos_sensor_channel {
	SENSOR_CONTROL_I2C0	 = 0,
	SENSOR_CONTROL_I2C1	 = 1,
	SENSOR_CONTROL_I2C2	 = 2,
	SENSOR_CONTROL_I2C3	 = 3,
	SENSOR_CONTROL_I2C4	 = 4,
	SENSOR_CONTROL_I2C5	 = 5,
	SENSOR_CONTROL_I2C6	 = 6,
	SENSOR_CONTROL_I2C7	 = 7,
	SENSOR_CONTROL_I2C8	 = 8,
	SENSOR_CONTROL_I2C9	 = 9,
	SENSOR_CONTROL_I2C10	 = 10,
	SENSOR_CONTROL_I2C11	 = 11,
	SENSOR_CONTROL_I2C12	 = 12,
	SENSOR_CONTROL_I2C13	 = 13,
	SENSOR_CONTROL_I2C14	 = 14,
	SENSOR_CONTROL_I2C_MAX
};

enum exynos_sensor_position {
	/* for the position of real sensors */
	SENSOR_POSITION_REAR		= SP_REAR,
	SENSOR_POSITION_FRONT		= SP_FRONT,
	SENSOR_POSITION_REAR2		= SP_REAR2,
	SENSOR_POSITION_FRONT2		= SP_FRONT2,
	SENSOR_POSITION_REAR3		= SP_REAR3,
	SENSOR_POSITION_FRONT3		= SP_FRONT3,
	SENSOR_POSITION_REAR4		= SP_REAR4,
	SENSOR_POSITION_FRONT4		= SP_FRONT4,
	SENSOR_POSITION_REAR_TOF	= SP_REAR_TOF,
	SENSOR_POSITION_FRONT_TOF	= SP_FRONT_TOF,
	SENSOR_POSITION_MAX,
};

enum actuator_name {
	ACTUATOR_NAME_AD5823	= 1,
	ACTUATOR_NAME_DWXXXX	= 2,
	ACTUATOR_NAME_AK7343	= 3,
	ACTUATOR_NAME_HYBRIDVCA	= 4,
	ACTUATOR_NAME_LC898212	= 5,
	ACTUATOR_NAME_WV560     = 6,
	ACTUATOR_NAME_AK7345	= 7,
	ACTUATOR_NAME_DW9804	= 8,
	ACTUATOR_NAME_AK7348	= 9,
	ACTUATOR_NAME_SHAF3301	= 10,
	ACTUATOR_NAME_BU64241GWZ = 11,
	ACTUATOR_NAME_AK7371	= 12,
	ACTUATOR_NAME_DW9807	= 13,
	ACTUATOR_NAME_ZC533     = 14,
	ACTUATOR_NAME_BU63165	= 15,
	ACTUATOR_NAME_AK7372    = 16,
	ACTUATOR_NAME_AK7371_DUAL = 17,
	ACTUATOR_NAME_AK737X    = 18,
	ACTUATOR_NAME_DW9780	= 19,
	ACTUATOR_NAME_LC898217	= 20,
	ACTUATOR_NAME_ZC569     = 21,
	ACTUATOR_NAME_DW9823	= 22,
	ACTUATOR_NAME_DW9839	= 23,
	ACTUATOR_NAME_DW9808	= 24,
	ACTUATOR_NAME_ZC535	= 25,
	ACTUATOR_NAME_DW9817	= 26,
	ACTUATOR_NAME_FP5529	= 27,
	ACTUATOR_NAME_DW9800	= 28,
	ACTUATOR_NAME_LC898219	= 29,
	ACTUATOR_NAME_DW9714	= 30,
	ACTUATOR_NAME_BU64981	= 31,
	ACTUATOR_NAME_SEM1215S	= 32,
	ACTUATOR_NAME_END,
	ACTUATOR_NAME_NOTHING	= 100,
};

enum flash_drv_name {
	FLADRV_NAME_KTD267	= 1,	/* Gpio type(Flash mode, Movie/torch mode) */
	FLADRV_NAME_AAT1290A	= 2,
	FLADRV_NAME_MAX77693	= 3,
	FLADRV_NAME_AS3643	= 4,
	FLADRV_NAME_KTD2692	= 5,
	FLADRV_NAME_LM3560	= 6,
	FLADRV_NAME_SKY81296	= 7,
	FLADRV_NAME_RT5033	= 8,
	FLADRV_NAME_AS3647	= 9,
	FLADRV_NAME_LM3646	= 10,
	FLADRV_NAME_DRV_FLASH_GPIO = 11, /* Common Gpio type(Flash mode, Movie/torch mode) */
	FLADRV_NAME_LM3644	= 12,
	FLADRV_NAME_DRV_FLASH_I2C = 13, /* Common I2C type */
	FLADRV_NAME_S2MU106	= 14,
	FLADRV_NAME_OCP8132A	= 15,
	FLADRV_NAME_RT8547	= 16,
	FLADRV_NAME_S2MU107	= 17,
	FLADRV_NAME_GENERIC	= 18,
	FLADRV_NAME_S2MF301	= 19,
	FLADRV_NAME_SM5714	= 20,
	FLADRV_NAME_END,
	FLADRV_NAME_NOTHING	= 100,
};

enum from_name {
	FROMDRV_NAME_W25Q80BW	= 1,	/* Spi type */
	FROMDRV_NAME_FM25M16A	= 2,	/* Spi type */
	FROMDRV_NAME_FM25M32A	= 3,
	FROMDRV_NAME_NOTHING	= 100,
};

enum preprocessor_name {
	PREPROCESSOR_NAME_73C1 = 1,	/* SPI, I2C, FIMC Lite */
	PREPROCESSOR_NAME_73C2 = 2,
	PREPROCESSOR_NAME_73C3 = 3,
	PREPROCESSOR_NAME_END,
	PREPROCESSOR_NAME_NOTHING = 100,
};

enum ois_name {
	OIS_NAME_RUMBA_S4	= 1,
	OIS_NAME_RUMBA_S6	= 2,
	OIS_NAME_ROHM_BU24218GWL = 3,
	OIS_NAME_SEM1215S	= 4,
	OIS_NAME_END,
	OIS_NAME_NOTHING	= 100,
};

enum mcu_name {
	MCU_NAME_STM32	= 1,
	MCU_NAME_INTERNAL	= 2,
	MCU_NAME_END,
	MCU_NAME_NOTHING	= 100,
};

enum aperture_name {
	APERTURE_NAME_AK7372	= 1,
	APERTURE_NAME_END,
	APERTURE_NAME_NOTHING	= 100,
};

enum eeprom_name {
	EEPROM_NAME_GM1		= 1,
	EEPROM_NAME_5E9		= 2,
	EEPROM_NAME_GW1		= 3,
	EEPROM_NAME_GD1		= 4,
	EEPROM_NAME_HI846	= 5,
	EEPROM_NAME_3M5_TELE	= 6,
	EEPROM_NAME_3M5_FOLD	= 7,
	EEPROM_NAME_GD1_TELE	= 8,
	EEPROM_NAME_GC5035	= 9,
	EEPROM_NAME_OV02A10	= 10,
	EEPROM_NAME_GM2 = 11,
	EEPROM_NAME_OV32A1Q = 12,
	EEPROM_NAME_3P9 = 13,
	EEPROM_NAME_4H7 = 14,
	EEPROM_NAME_END,
	EEPROM_NAME_NOTHING	= 100,
};

enum laser_af_name {
	LASER_AF_NAME_VL53L5	= 1,
	LASER_AF_NAME_TOF8801	= 2,
	LASER_AF_NAME_VL53L1,
	LASER_AF_NAME_END,
	LASER_AF_NAME_NOTHING	= 100,
};

enum sensor_peri_type {
	SE_NULL		= 0,
	SE_I2C		= 1,
	SE_SPI		= 2,
	SE_GPIO		= 3,
	SE_MPWM		= 4,
	SE_ADC		= 5,
	SE_DMA		= 6,
};

enum sensor_dma_channel_type {
	DMA_CH_NOT_DEFINED	= 100,
};

enum itf_vc_stat_type {
	VC_STAT_TYPE_INVALID = -1,

	/* Types for SW PDAF(tail mode buffer type) */
	VC_STAT_TYPE_TAIL_FOR_SW_PDAF = 100,

	/* Types for IMX PDAF sensors */
	VC_STAT_TYPE_IMX_FLEXIBLE = 200,
	VC_STAT_TYPE_IMX_STATIC,

	/* Types for PAF_STAT */
	VC_STAT_TYPE_PAFSTAT_FLOATING = 300,
	VC_STAT_TYPE_PAFSTAT_STATIC,

	/* Types for PDP 1.0 in Lhotse/Makalu EVT0 */
	VC_STAT_TYPE_OLD_PDP_1_0_PDAF_STAT0 = 400,
	VC_STAT_TYPE_OLD_PDP_1_0_PDAF_STAT1,

	/* Types for PDP 1.1 in Makalu EVT1 */
	VC_STAT_TYPE_OLD_PDP_1_1_PDAF_STAT0 = 500,
	VC_STAT_TYPE_OLD_PDP_1_1_PDAF_STAT1,

	/* Types for 3HDR */
	VC_STAT_TYPE_TAIL_FOR_3HDR_LSI = 600,
	VC_STAT_TYPE_TAIL_FOR_3HDR_IMX,
	VC_STAT_TYPE_TAIL_FOR_3HDR_IMX_2_STAT0,
	VC_STAT_TYPE_TAIL_FOR_3HDR_IMX_2_STAT1,

	/* Types for PDP 1.0 in 2020 EVT0 */
	VC_STAT_TYPE_PDP_1_0_PDAF_STAT0 = 700,
	VC_STAT_TYPE_PDP_1_0_PDAF_STAT1,

	/* Types for PDP 1.1 in 2020 EVT1 */
	VC_STAT_TYPE_PDP_1_1_PDAF_STAT0 = 800,
	VC_STAT_TYPE_PDP_1_1_PDAF_STAT1,

	/* Types for PDP 3.0 in Olympus EVT0 */
	VC_STAT_TYPE_PDP_3_0_PDAF_STAT0 = 900,
	VC_STAT_TYPE_PDP_3_0_PDAF_STAT1,
	VC_STAT_TYPE_PDP_3_0_PDAF_PD_DUMP,

	/* Types for PDP 3.1 in Olympus EVT1 */
	VC_STAT_TYPE_PDP_3_1_PDAF_STAT0 = 1000,
	VC_STAT_TYPE_PDP_3_1_PDAF_STAT1,
	VC_STAT_TYPE_PDP_3_1_PDAF_PD_DUMP,

	/* Types for PDP 4.0 in Pamir */
	VC_STAT_TYPE_PDP_4_0_PDAF_STAT0 = 1100,
	VC_STAT_TYPE_PDP_4_0_PDAF_STAT1,
	VC_STAT_TYPE_PDP_4_0_PDAF_PD_DUMP,

	/* Types for PDP 4.1 in Pamir EVT1 */
	VC_STAT_TYPE_PDP_4_1_PDAF_STAT0 = 1200,
	VC_STAT_TYPE_PDP_4_1_PDAF_STAT1,
	VC_STAT_TYPE_PDP_4_1_PDAF_PD_DUMP,
};

enum itf_vc_sensor_mode {
	VC_SENSOR_MODE_INVALID = -1,

	/* 2PD */
	VC_SENSOR_MODE_2PD_MODE1 = 100,
	VC_SENSOR_MODE_2PD_MODE2,
	VC_SENSOR_MODE_2PD_MODE3,
	VC_SENSOR_MODE_2PD_MODE4,
	VC_SENSOR_MODE_2PD_MODE1_HDR,
	VC_SENSOR_MODE_2PD_MODE2_HDR,
	VC_SENSOR_MODE_2PD_MODE3_HDR,
	VC_SENSOR_MODE_2PD_MODE4_HDR,

	/* MSPD */
	VC_SENSOR_MODE_MSPD_NORMAL = 200,
	VC_SENSOR_MODE_MSPD_TAIL,
	VC_SENSOR_MODE_MSPD_GLOBAL_NORMAL,
	VC_SENSOR_MODE_MSPD_GLOBAL_TAIL,

	/* Ultra PD */
	VC_SENSOR_MODE_ULTRA_PD_NORMAL = 300,
	VC_SENSOR_MODE_ULTRA_PD_TAIL,
	VC_SENSOR_MODE_ULTRA_PD_2_NORMAL,
	VC_SENSOR_MODE_ULTRA_PD_2_TAIL,
	VC_SENSOR_MODE_ULTRA_PD_3_NORMAL,
	VC_SENSOR_MODE_ULTRA_PD_3_TAIL,

	/* Super PD */
	VC_SENSOR_MODE_SUPER_PD_NORMAL = 400,
	VC_SENSOR_MODE_SUPER_PD_TAIL,
	VC_SENSOR_MODE_SUPER_PD_2_NORMAL,
	VC_SENSOR_MODE_SUPER_PD_2_TAIL,
	VC_SENSOR_MODE_SUPER_PD_3_NORMAL,
	VC_SENSOR_MODE_SUPER_PD_3_TAIL,        // GW2(Hubble X1/X2/Z3), GH1(Hubble Z3). LLLLL RRRRR
	VC_SENSOR_MODE_SUPER_PD_4_NORMAL_FULL,
	VC_SENSOR_MODE_SUPER_PD_4_NORMAL_3BIN,
	VC_SENSOR_MODE_SUPER_PD_4_NORMAL_6BIN,
	VC_SENSOR_MODE_SUPER_PD_4_TAIL,        // HM1(Hubble Z3). LRLRLRLRLRLRLRLR
	VC_SENSOR_MODE_SUPER_PD_5_NORMAL_FULL,
	VC_SENSOR_MODE_SUPER_PD_5_NORMAL_2BIN,
	VC_SENSOR_MODE_SUPER_PD_5_NORMAL_4BIN,
	VC_SENSOR_MODE_SUPER_PD_5_TAIL,        /* GM2(PD1962). LLLLL RRRRR */

	/* IMX PDAF */
	VC_SENSOR_MODE_IMX_PDAF = 500,
	VC_SENSOR_MODE_IMX_2X1OCL_1_NORMAL,	/* IMX576. One line: LLLLLLLLRRRRRRRR */
	VC_SENSOR_MODE_IMX_2X1OCL_1_TAIL,	/* IMX576. One line: LLLLLLLLRRRRRRRR */
	VC_SENSOR_MODE_IMX_2X1OCL_2_NORMAL,	/* IMX586. One line: LRLRLRLRLRLRLRLR */
	VC_SENSOR_MODE_IMX_2X1OCL_2_TAIL,	/* IMX586. One line: LRLRLRLRLRLRLRLR */

	/* 3HDR */
	VC_SENSOR_MODE_3HDR_LSI = 600,
	VC_SENSOR_MODE_3HDR_IMX,
	VC_SENSOR_MODE_3HDR_IMX_2,

	/* OV SENSOR */
	VC_SENSOR_MODE_OV_PDAF_TYPE1 = 700,
};

/* MIPI-CSI interface */
enum itf_vc_buf_data_type {
	VC_BUF_DATA_TYPE_INVALID = -1,
	VC_BUF_DATA_TYPE_SENSOR_STAT1 = 0,
	VC_BUF_DATA_TYPE_GENERAL_STAT1,
	VC_BUF_DATA_TYPE_SENSOR_STAT2,
	VC_BUF_DATA_TYPE_GENERAL_STAT2,
	VC_BUF_DATA_TYPE_MAX
};

struct sensor_open_extended {
	/* Use sensor retention mode */
	u32 use_retention_mode;
};

enum is_img_pd_ratio {
	IS_IMG_PD_RATIO_1_1 = 1,
};

struct is_fid_loc {
	u32 valid;
	u32 line;
	u32 byte;
};

struct is_vci_config {
	u32			map;
	u32			hwformat;
	u32			extformat;
	u32			type;
	u32			width;
	u32			height;
	u32			buffer_num;
};

struct is_sensor_cfg {
	u32 width;
	u32 height;
	u32 framerate;
	u32 max_fps;
	u32 settle;
	u32 mode;
	u32 lanes;
	u32 mipi_speed;
	u32 interleave_mode;
	u32 lrte;
	u32 pd_mode;
	u32 ex_mode;
	struct is_vci_config input[CSI_VIRTUAL_CH_MAX];
	struct is_vci_config output[CSI_VIRTUAL_CH_MAX];

	/* option */
	u32 votf;
	u32 max_vc;
	u32 vvalid_time; /* unit(us) */
	u32 req_vvalid_time; /* unit(us) */
	unsigned int wdma_ch_hint; /* Designated CSI WDMA ch for special function */
	u32 binning; /* binning ratio */
	int special_mode;
	enum is_img_pd_ratio img_pd_ratio;
	u32 dummy_pixel[CSI_VIRTUAL_CH_MAX];
	struct is_fid_loc fid_loc;
	u32 dvfs_lv_csis;
	u32 ex_mode_extra;
};

struct is_sensor_vc_extra_info {
	int stat_type;
	int sensor_mode;
	u32 max_width;
	u32 max_height;
	u32 max_element;
};

struct is_sensor_ops {
	int (*stream_on)(struct v4l2_subdev *subdev);
	int (*stream_off)(struct v4l2_subdev *subdev);

	int (*s_duration)(struct v4l2_subdev *subdev, u64 duration);
	int (*g_min_duration)(struct v4l2_subdev *subdev);
	int (*g_max_duration)(struct v4l2_subdev *subdev);

	int (*s_exposure)(struct v4l2_subdev *subdev, u64 exposure);
	int (*g_min_exposure)(struct v4l2_subdev *subdev);
	int (*g_max_exposure)(struct v4l2_subdev *subdev);

	int (*s_again)(struct v4l2_subdev *subdev, u64 sensivity);
	int (*g_min_again)(struct v4l2_subdev *subdev);
	int (*g_max_again)(struct v4l2_subdev *subdev);

	int (*s_dgain)(struct v4l2_subdev *subdev);
	int (*g_min_dgain)(struct v4l2_subdev *subdev);
	int (*g_max_dgain)(struct v4l2_subdev *subdev);

	int (*s_shutterspeed)(struct v4l2_subdev *subdev, u64 shutterspeed);
	int (*g_min_shutterspeed)(struct v4l2_subdev *subdev);
	int (*g_max_shutterspeed)(struct v4l2_subdev *subdev);
};

struct is_module_enum {
	u32					instance;	/* logical stream id */
	u32					sensor_id;	/* TODO: deprecated *//* physical module enum */
	u32					device;		/* physical sensor node id: for searching for sensor device */
	int					enum_id;	/* index for module_enum */

	struct v4l2_subdev			*subdev;	/* connected module subdevice */
	unsigned long				state;
	u32					pixel_width;
	u32					pixel_height;
	u32					active_width;
	u32					active_height;
	u32					margin_left;
	u32					margin_right;
	u32					margin_top;
	u32					margin_bottom;
	u32					max_framerate;
	u32					position;
	u32					bitwidth;
	u32					cfgs;
	u32					stat_vc;
	ktime_t					act_available_time;
	struct is_sensor_cfg			*cfg;
	struct is_sensor_vc_extra_info		vc_extra_info[VC_BUF_DATA_TYPE_MAX];
	struct i2c_client			*client;
	struct sensor_open_extended		ext;
	struct is_sensor_ops			*ops;
	char					*sensor_maker;
	char					*sensor_name;
	char					*setfile_name;
	struct is_priv_buf			*setfile_pb;
	unsigned long				setfile_kva;
	size_t					setfile_size;
	bool					setfile_pinned;
	struct hrtimer				vsync_timer;
	struct work_struct			vsync_work;
	void					*private_data;
	struct exynos_platform_is_module	*pdata;
	struct device				*dev;
};

struct exynos_platform_is_sensor {
	int (*iclk_get)(struct device *dev, u32 scenario, u32 channel);
	int (*iclk_cfg)(struct device *dev, u32 scenario, u32 channel);
	int (*iclk_on)(struct device *dev,u32 scenario, u32 channel);
	int (*iclk_off)(struct device *dev, u32 scenario, u32 channel);
	int (*mclk_on)(struct device *dev, u32 scenario, u32 channel, u32 freq);
	int (*mclk_off)(struct device *dev, u32 scenario, u32 channel);
	int (*mclk_force_off)(struct device *dev, u32 channel);
	u32 id;
	u32 scenario;
	u32 csi_ch;
	unsigned int wdma_ch_hint;
	u32 csi_mux;
	u32 multi_ch;
	u32 use_cphy;
	u32 scramble;
	u32 i2c_dummy_enable;
};

void is_sensor_get_clk_ops(struct exynos_platform_is_sensor *pdata);
#endif /* MEDIA_EXYNOS_SENSOR_H */
