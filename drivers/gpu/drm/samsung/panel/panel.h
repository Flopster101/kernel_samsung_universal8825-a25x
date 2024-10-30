/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_H__
#define __PANEL_H__

#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/spi/spi.h>
#include <linux/sysfs.h>
#include "panel_kunit.h"
#include "panel_obj.h"
#include "maptbl.h"
#include "util.h"
#include "panel_sequence.h"
#include "panel_property.h"
#include "panel_delay.h"
#include "panel_packet.h"
#include "panel_resource.h"
#include "panel_condition.h"
#include "panel_bl.h"
#include "panel_config.h"

#ifdef CONFIG_USDM_PANEL_DIMMING
#include "dimming.h"
#include "panel_dimming.h"
#endif

#ifdef CONFIG_USDM_MDNIE
#include "mdnie.h"
#endif

#ifdef CONFIG_USDM_PANEL_COPR
#include "copr.h"
#endif

#include "panel_vrr.h"

#include "panel_modes.h"

#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY
#include "./aod/aod_drv.h"
#endif

#include "panel_obj.h"
#include "panel_dump.h"

#if defined(CONFIG_USDM_SDP_ADAPTIVE_MIPI)
#include "sdp_adaptive_mipi.h"
#elif defined(CONFIG_USDM_ADAPTIVE_MIPI)
#include "adaptive_mipi.h"
#endif

enum {
	MIPI_DSI_WR_UNKNOWN = 0,
	MIPI_DSI_WR_GEN_CMD,
	MIPI_DSI_WR_DSC_CMD,
	MIPI_DSI_WR_PPS_CMD,
	MIPI_DSI_WR_GRAM_CMD,
	MIPI_DSI_WR_SRAM_CMD,
	MIPI_DSI_WR_SR_FAST_CMD,
	MAX_MIPI_DSI_CMD_TYPE,
};

#define CONFIG_LCD_HBM_60_STEP
#define MIPI_DCS_WRITE_GRAM_START			(0x2C)
#define MIPI_DCS_WRITE_GRAM_CONTINUE		(0x3C)
#define MIPI_DCS_WRITE_SIDE_RAM_START		(0x4C)
#define MIPI_DCS_WRITE_SIDE_RAM_CONTINUE	(0x5C)
#define MIPI_DCS_WRITE_SPSRAM				(0x6C)

#ifdef CONFIG_USDM_FACTORY
#define CONFIG_SUPPORT_PANEL_SWAP
#define CONFIG_SUPPORT_ISC_DEFECT
#endif

#define CONFIG_SUPPORT_XTALK_MODE
#define CONFIG_SUPPORT_GRAYSPOT_TEST

#define CONFIG_SUPPORT_SPI_IF_SEL

#define to_panel_device(_m_)	container_of(_m_, struct panel_device, _m_)

/* DUMMY PANEL NAME */
#define __pn_name__	xx_xx
#define __PN_NAME__	XX_XX

#define PN_CONCAT(a, b)  _PN_CONCAT(a, b)
#define _PN_CONCAT(a, b) a ## _ ## b

#if IS_ENABLED(CONFIG_USDM_PANEL_ID_READ_REG_ADAEAF)
#define PANEL_ID_REG		(0xAD)
#elif IS_ENABLED(CONFIG_USDM_PANEL_ID_READ_REG_DADBDC) || \
	(!IS_ENABLED(CONFIG_USDM_PANEL_ID_READ_REG_04) && IS_ENABLED(CONFIG_USDM_PANEL_TFT_COMMON))
#define PANEL_ID_REG		(0xDA)
#else
#define PANEL_ID_REG		(0x04)
#endif
#define PANEL_ID_LEN		(3)
#define PANEL_OCTA_ID_LEN	(20)
#define PANEL_POC_CHKSUM_LEN	(5)
#define PANEL_POC_CTRL_LEN	(4)
#define PANEL_CODE_LEN		(5)
#define PANEL_COORD_LEN		(4)
#define PANEL_DATE_LEN		(7)
#define PANEL_RDDPM_LEN		(1)
#define PANEL_RDDSM_LEN		(1)
#define PANEL_VENDOR_LEN	(3)

#define NORMAL_TEMPERATURE			(25)
#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define UNDER_MINUS_15(temperature)	(temperature <= -15)
#define UNDER_0(temperature)	(temperature <= 0)
#define CAPS_IS_ON(level)	(level >= 41)
#define PANEL_WAIT_VSYNC_TIMEOUT_MSEC	(110)

struct mdnie_info;

/* external pin control command */
struct pininfo {
	struct pnobj base;
	int pin;
	int onoff;
};

/* command type */
enum {
	CMD_TYPE_NONE,
	CMD_TYPE_PROP,
	CMD_TYPE_FUNC,
	CMD_TYPE_MAP,
	/* delay */
	CMD_TYPE_DELAY,
	CMD_TYPE_TIMER_DELAY,
	CMD_TYPE_TIMER_DELAY_BEGIN,
	/* condition */
	CMD_TYPE_COND_IF,
	CMD_TYPE_COND_EL,
	CMD_TYPE_COND_FI,
	CMD_TYPE_PCTRL,
	CMD_TYPE_RX_PACKET,
	/* dependant command type */
	CMD_TYPE_CFG,
	CMD_TYPE_TX_PACKET,
	CMD_TYPE_KEY,
	CMD_TYPE_RES,
	CMD_TYPE_DMP,
	CMD_TYPE_SEQ,
	MAX_CMD_TYPE,
};

#define IS_CMD_TYPE_MAP(_type_) ((_type_) == CMD_TYPE_MAP)
#define IS_CMD_TYPE_DUMP(_type_) ((_type_) == CMD_TYPE_DMP)
#define IS_CMD_TYPE_KEY(_type_) ((_type_) == CMD_TYPE_KEY)
#define IS_CMD_TYPE_RES(_type_) ((_type_) == CMD_TYPE_RES)
#define IS_CMD_TYPE_SEQ(_type_) ((_type_) == CMD_TYPE_SEQ)

#define IS_CMD_TYPE_TX_PKT(_type_) \
	((_type_) == CMD_TYPE_TX_PACKET)

#define IS_CMD_TYPE_RX_PKT(_type_) \
	((_type_) == CMD_TYPE_RX_PACKET)

#define IS_CMD_TYPE_DELAY(_type_) \
	((_type_) == CMD_TYPE_DELAY)

#define IS_CMD_TYPE_TIMER_DELAY(_type_) \
	((_type_) == CMD_TYPE_TIMER_DELAY || \
	(_type_) == CMD_TYPE_TIMER_DELAY_BEGIN)

#define IS_CMD_TYPE_COND(_type_) \
	((_type_) >= CMD_TYPE_COND_IF && (_type_) <= CMD_TYPE_COND_FI)

#define IS_CMD_TYPE_PWRCTRL(_type_) ((_type_) == CMD_TYPE_PCTRL)

#define IS_CMD_TYPE_PROP(_type_) ((_type_) == CMD_TYPE_PROP)

#define IS_CMD_TYPE_CFG(_type_) ((_type_) == CMD_TYPE_CFG)

#define IS_CMD_TYPE_FUNC(_type_) ((_type_) == CMD_TYPE_FUNC)

enum {
	KEY_NONE,
	KEY_DISABLE,
	KEY_ENABLE,
	MAX_KEY_TYPE,
};

enum cmd_level {
	CMD_LEVEL_NONE,
	CMD_LEVEL_1,
	CMD_LEVEL_2,
	CMD_LEVEL_3,
	MAX_CMD_LEVEL,
};

/* key level command */
struct keyinfo {
	struct pnobj base;
	enum cmd_level level;
	u32 en;
	struct pktinfo *packet;
};

#define KEYINFO(_name_) PN_CONCAT(key, _name_)
#define KEYINFO_INIT(_keyname, _lv, _en, _pkt)	\
	{ .base = __PNOBJ_INITIALIZER(_keyname, CMD_TYPE_KEY) \
	, .level = (_lv) \
	, .en = (_en) \
	, .packet = (_pkt) }

#define DEFINE_PANEL_KEY(_keyname, _lv, _en, _pkt)	\
struct keyinfo KEYINFO(_keyname) = KEYINFO_INIT(_keyname, _lv, _en, _pkt)

static inline char *get_key_name(struct keyinfo *key)
{
	return get_pnobj_name(&key->base);
}

enum cmd_mode {
	CMD_MODE_NONE,
	CMD_MODE_RO,
	CMD_MODE_WO,
	CMD_MODE_RW,
	MAX_CMD_MODE,
};

struct ldi_reg_desc {
	u8 addr;
	char *name;
	enum cmd_mode mode;
	enum cmd_level level;
	bool dirty;
};

struct freq_hop_param {
	u32 dsi_freq;
	u32 osc_freq;
};

#define MAX_LDI_REG		(0x100)
#define LDI_REG_DESC(_addr_, _name_, _mode_, _level_)	\
[(_addr_)] = {											\
	.addr = (_addr_),									\
	.name = (_name_),									\
	.mode = (_mode_),									\
	.level = (_level_),									\
	.dirty = (0),										\
}

#define PANEL_NONE_SEQ ("panel_none_seq")
#define PANEL_INIT_SEQ ("panel_init_seq")
#define PANEL_EXIT_SEQ ("panel_exit_seq")
#define PANEL_BOOT_SEQ ("panel_boot_seq")
#define PANEL_RES_INIT_SEQ ("panel_res_init_seq")
#ifdef CONFIG_USDM_PANEL_DIM_FLASH
#define PANEL_DIM_FLASH_RES_INIT_SEQ ("panel_dim_flash_res_init_seq")
#endif
#ifdef CONFIG_USDM_PANEL_GM2_FLASH
#define PANEL_GM2_FLASH_RES_INIT_SEQ ("panel_gm2_flash_res_init_seq")
#endif
#define PANEL_MAP_INIT_SEQ ("panel_map_init_seq")
#define PANEL_DISPLAY_ON_SEQ ("panel_display_on_seq")
#define PANEL_DISPLAY_OFF_SEQ ("panel_display_off_seq")
#define PANEL_ALPM_INIT_SEQ ("panel_alpm_init_seq")
#define PANEL_ALPM_SET_BL_SEQ  ("panel_alpm_set_bl_seq")
#define PANEL_ALPM_EXIT_SEQ ("panel_alpm_exit_seq")
#define PANEL_ALPM_EXIT_AFTER_SEQ ("panel_alpm_exit_after_seq")
#define PANEL_SET_BL_SEQ ("panel_set_bl_seq")
#ifdef CONFIG_USDM_PANEL_HMD
#define PANEL_HMD_ON_SEQ ("panel_hmd_on_seq")
#define PANEL_HMD_OFF_SEQ ("panel_hmd_off_seq")
#define PANEL_HMD_BL_SEQ ("panel_hmd_bl_seq")
#endif
#define PANEL_HBM_ON_SEQ ("panel_hbm_on_seq")
#define PANEL_HBM_OFF_SEQ ("panel_hbm_off_seq")
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
#define PANEL_DISPLAY_MODE_SEQ ("panel_display_mode_seq")
#endif
#define PANEL_FPS_SEQ ("panel_fps_seq")
#define PANEL_BLACK_AND_FPS_SEQ ("panel_black_and_fps_seq")
#define PANEL_MCD_ON_SEQ ("panel_mcd_on_seq")
#define PANEL_MCD_OFF_SEQ ("panel_mcd_off_seq")
#define PANEL_MCD_RS_ON_SEQ ("panel_mcd_rs_on_seq")
#define PANEL_MCD_RS_OFF_SEQ ("panel_mcd_rs_off_seq")
#define PANEL_MCD_RS_READ_SEQ ("panel_mcd_rs_read_seq")
#ifdef CONFIG_USDM_FACTORY_MST_TEST
#define PANEL_MST_ON_SEQ ("panel_mst_on_seq")
#define PANEL_MST_OFF_SEQ ("panel_mst_off_seq")
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
#define PANEL_GCT_ENTER_SEQ ("panel_gct_enter_seq")
#define PANEL_GCT_VDDM_SEQ ("panel_gct_vddm_seq")
#define PANEL_GCT_IMG_UPDATE_SEQ ("panel_gct_img_update_seq")
#define PANEL_GCT_IMG_0_UPDATE_SEQ ("panel_gct_img_0_update_seq")
#define PANEL_GCT_IMG_1_UPDATE_SEQ ("panel_gct_img_1_update_seq")
#define PANEL_GCT_EXIT_SEQ ("panel_gct_exit_seq")
#endif
#if defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
#define PANEL_FD_SEQ ("panel_fd_seq")
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
#define PANEL_GRAYSPOT_ON_SEQ ("panel_grayspot_on_seq")
#define PANEL_GRAYSPOT_OFF_SEQ ("panel_grayspot_off_seq")
#endif
#ifdef CONFIG_SUPPORT_ISC_DEFECT
#define PANEL_CHECK_ISC_DEFECT_SEQ ("panel_check_isc_defect_seq")
#endif
#ifdef CONFIG_SUPPORT_SPI_IF_SEL
#define PANEL_SPI_IF_ON_SEQ ("panel_spi_if_on_seq")
#define PANEL_SPI_IF_OFF_SEQ ("panel_spi_if_off_seq")
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
#define PANEL_CCD_TEST_SEQ ("panel_ccd_test_seq")
#endif
#define PANEL_FLASH_TEST_SEQ ("panel_flash_test_seq")

#ifdef CONFIG_USDM_PANEL_MAFPC
#define PANEL_MAFPC_ON_SEQ ("panel_mafpc_on_seq")
#define PANEL_MAFPC_OFF_SEQ ("panel_mafpc_off_seq")
#define PANEL_MAFPC_SCALE_FACTOR_SEQ ("panel_mafpc_scale_factor_seq")
#define PANEL_MAFPC_IMG_SEQ ("panel_mafpc_img_seq")
#define PANEL_MAFPC_CHECKSUM_SEQ ("panel_mafpc_checksum_seq")
#endif
#define PANEL_PARTIAL_DISP_ON_SEQ ("panel_partial_disp_on_seq")
#define PANEL_PARTIAL_DISP_OFF_SEQ ("panel_partial_disp_off_seq")
#define PANEL_DUMP_SEQ ("panel_dump_seq")
#ifdef CONFIG_USDM_DDI_CMDLOG
#define PANEL_CMDLOG_DUMP_SEQ ("panel_cmdlog_dump_seq")
#endif
#define PANEL_CHECK_CONDITION_SEQ ("panel_check_condition_seq")
#define PANEL_DIA_ONOFF_SEQ ("panel_dia_onoff_seq")
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
#define PANEL_MASK_LAYER_STOP_DIMMING_SEQ ("panel_mask_layer_stop_dimming_seq")
#define PANEL_MASK_LAYER_ENTER_BR_SEQ ("panel_mask_layer_enter_br_seq")
#define PANEL_MASK_LAYER_EXIT_BR_SEQ ("panel_mask_layer_exit_br_seq")
#endif
#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
#define PANEL_BRIGHTDOT_TEST_SEQ ("panel_brightdot_test_seq")
#endif
#ifdef CONFIG_USDM_FACTORY_VGLHIGHDOT_TEST
#define PANEL_VGLHIGHDOT_TEST_SEQ ("panel_vglhighdot_test_seq")
#endif
#define PANEL_FFC_SEQ ("panel_ffc_seq")
#define PANEL_OSC_SEQ ("panel_osc_seq")
#ifdef CONFIG_USDM_FACTORY_SSR_TEST
#define PANEL_SSR_TEST_SEQ ("panel_ssr_test_seq")
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
#define PANEL_ECC_TEST_SEQ ("panel_ecc_test_seq")
#endif
#define PANEL_DECODER_TEST_SEQ ("panel_decoder_test_seq")
#define PANEL_PCD_DUMP_SEQ ("panel_pcd_dump_seq")
#define PANEL_DUMMY_SEQ ("panel_dummy_seq")
#define PANEL_FMEM_TEST_WRITE_SEQ ("panel_fmem_test_write_seq")
#define PANEL_FMEM_TEST_READ_SEQ ("panel_fmem_test_read_seq")

#if defined(CONFIG_USDM_LPD_AUTO_BR)
#define PANEL_LPD_BR_SEQ ("panel_lpd_br_seq")
#define PANEL_LPD_HBM_BR_SEQ ("panel_lpd_hbm_br_seq")
#define PANEL_LPD_INIT_SEQ ("panel_lpd_init_seq")
#endif
#if defined(CONFIG_USDM_PANEL_VCOM_TRIM_TEST)
#define PANEL_VCOM_TRIM_TEST_SEQ ("panel_vcom_trim_test_seq")
#endif
#define PANEL_AGING_ON_SEQ ("panel_aging_on_seq")
#define PANEL_AGING_OFF_SEQ ("panel_aging_off_seq")

/* structure of sequence table */
struct brt_map {
	int brt;
	int lum;
};

#define DDI_SUPPORT_WRITE_GPARA	(1U << 0)
#define DDI_SUPPORT_READ_GPARA	(1U << 1)
#define DDI_SUPPORT_POINT_GPARA	(1U << 2)
#define DDI_SUPPORT_2BYTE_GPARA	(1U << 3)

enum {
	PN_COMP_TYPE_NONE,
	PN_COMP_TYPE_MIC,
	PN_COMP_TYPE_DSC,
	MAX_PN_COMP_TYPE,
};

struct panel_dsc {
	u32 slice_w;
	u32 slice_h;
};

enum vrr_mode {
	VRR_NORMAL_MODE,
	VRR_HS_MODE,
	MAX_VRR_MODE,
};

enum {
	VRR_AID_2_CYCLE,
	VRR_AID_4_CYCLE,
};

#define TE_SKIP_TO_DIV(_sw_skip, _hw_skip) \
	((_sw_skip + 1) * (_hw_skip + 1))

struct panel_vrr {
	u32 fps;
	u32 base_fps;
	u32 base_vactive;
	u32 base_vfp;
	u32 base_vbp;
	int te_sel;
	int te_v_st;
	int te_v_ed;
	u32 te_sw_skip_count;
	u32 te_hw_skip_count;
	u32 mode;
	int aid_cycle;
};

struct panel_resol {
	unsigned int w;
	unsigned int h;
	unsigned int comp_type;
	union {
		struct panel_dsc dsc;
	} comp_param;
	struct panel_vrr **available_vrr;
	unsigned int nr_available_vrr;
};

struct panel_mres {
	u32 nr_resol;
	struct panel_resol *resol;
};

#ifdef CONFIG_USDM_PANEL_MASK_LAYER
enum {
	MASK_LAYER_ON_BEFORE,
	MASK_LAYER_ON_AFTER,
	MASK_LAYER_OFF_BEFORE,
	MASK_LAYER_OFF_AFTER,
	MAX_MASK_LAYER,
};

enum {
	MASK_LAYER_HOOK_OFF,
	MASK_LAYER_HOOK_ON,
	MAX_MASK_LAYER_HOOK,
};

struct mask_layer_data {
	u32 req;
};
#endif

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
struct common_panel_display_mode {
	char name[PANEL_DISPLAY_MODE_NAME_LEN];
	struct panel_resol *resol;
	struct panel_vrr *vrr;
};

struct common_panel_display_modes {
	unsigned int num_modes;
	struct common_panel_display_mode **modes;
};
#endif

struct ddi_properties {
	u32 gpara;
	bool support_partial_disp;
	//D2: avoid abnormal screen issue
	bool ssd_off_lpm_to_normal;
	u32 cmd_fifo_size;
	u32 img_fifo_size;
	bool err_fg_recovery;
	bool err_fg_powerdown;
	bool support_vrr;
	bool support_vrr_lfd;
	u32 dft_dsi_freq; //hs clock speen interface to AP, unit: khz
	u32 dft_osc_freq; //ddi's oscillator clock rate, unit: khz
	bool init_seq_by_lpdt;
	bool support_avoid_sandstorm;
	bool evasion_disp_det;
};

struct ddi_ops {
	int (*ddi_init)(struct panel_device *panel, void *data, u32 size);
	int (*gamma_flash_checksum)(struct panel_device *panel, void *data, u32 size);
	int (*mtp_gamma_check)(struct panel_device *panel, void *data, u32 size);
#ifdef CONFIG_USDM_FACTORY_SSR_TEST
	int (*ssr_test)(struct panel_device *panel, void *data, u32 size);
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
	int (*ecc_test)(struct panel_device *panel, void *data, u32 size);
#endif
	int (*decoder_test)(struct panel_device *panel, void *data, u32 size);
#if defined(CONFIG_USDM_PANEL_VCOM_TRIM_TEST)
	int (*vcom_trim_test)(struct panel_device *panel, void *data, u32 size);
#endif
	int (*get_cell_id)(struct panel_device *panel, void *data);
	int (*get_octa_id)(struct panel_device *panel, void *data);
	int (*get_manufacture_code)(struct panel_device *panel, void *data);
	int (*get_manufacture_date)(struct panel_device *panel, void *data);
	int (*get_temperature_range)(struct panel_device *panel, void *data);
	int (*check_mipi_read)(struct panel_device *panel, void *data);
};

struct rcd_region {
	int x;
	int y;
	int w;
	int h;
};

struct rcd_image {
	char *name;
	struct rcd_region image_rect;
	u8 *image_data;
	u32 image_data_len;
};

struct rcd_resol {
	char *name;
	int resol_x;
	int resol_y;
	struct rcd_image **images;
	int nr_images;
	struct rcd_region block_rect;
};

struct panel_rcd_data {
	u32 version;
	char *name;
	struct rcd_resol **rcd_resol;
	int nr_rcd_resol;
};

enum {
	USDM_DRV_LEVEL_COMMON,
	USDM_DRV_LEVEL_DDI,
	USDM_DRV_LEVEL_MODEL,
	MAX_USDM_DRV_LEVEL,
};

struct common_panel_info {
	char *ldi_name;
	char *name;
	char *model;
	char *vendor;
	u32 id;
	u32 rev;
	struct ddi_properties ddi_props;
	struct ddi_ops ddi_ops;
	struct panel_mres mres;
	struct panel_vrr **vrrtbl;
	int nr_vrrtbl;
	struct maptbl *maptbl;
	int nr_maptbl;
	struct seqinfo *seqtbl;
	int nr_seqtbl;
	struct rdinfo *rditbl;
	int nr_rditbl;
	struct resinfo *restbl;
	int nr_restbl;
	struct dumpinfo *dumpinfo;
	int nr_dumpinfo;
	struct mdnie_tune *mdnie_tune;
	struct panel_dimming_info *panel_dim_info[MAX_PANEL_BL_SUBDEV];
#ifdef CONFIG_USDM_PANEL_COPR
	struct panel_copr_data *copr_data;
#endif
#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY
	struct aod_tune *aod_tune;
#endif
#ifdef CONFIG_USDM_PANEL_DDI_FLASH
	struct panel_poc_data *poc_data;
#endif
#ifdef CONFIG_USDM_POC_SPI
	struct spi_data *spi_data;
	struct spi_data **spi_data_tbl;
	int nr_spi_data_tbl;
#endif
#ifdef CONFIG_USDM_PANEL_BLIC
	struct blic_data **blic_data_tbl;
	int nr_blic_data_tbl;
#endif
#ifdef CONFIG_USDM_PANEL_FREQ_HOP
	struct freq_hop_elem *freq_hop_elems;
	int nr_freq_hop_elems;
#endif

#if defined(CONFIG_USDM_ADAPTIVE_MIPI)
	struct rf_band_element *rf_elements;
	unsigned int rf_element_nr;
#endif

#ifdef CONFIG_USDM_PANEL_MAFPC
	struct mafpc_info *mafpc_info;
#endif

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	struct common_panel_display_modes *common_panel_modes;
#endif
#ifdef CONFIG_USDM_PANEL_RCD
	struct panel_rcd_data *rcd_data;
#endif
	struct panel_prop_list *prop_lists[MAX_USDM_DRV_LEVEL];
	unsigned int num_prop_lists[MAX_USDM_DRV_LEVEL];
	const char *ezop_json;
};

enum {
	OVER_ZERO,
	UNDER_ZERO,
	UNDER_MINUS_FIFTEEN,
	DIM_OVER_ZERO,
	DIM_UNDER_ZERO,
	DIM_UNDER_MINUS_FIFTEEN,
	TEMP_MAX,
};

enum {
	POC_ONOFF_OFF,
	POC_ONOFF_ON,
	POC_ONOFF_MAX
};

enum {
	ACL_PWRSAVE_OFF,
	ACL_PWRSAVE_ON,
	MAX_ACL_PWRSAVE,
};

enum {
	SMOOTH_TRANS_OFF,
	SMOOTH_TRANS_ON,
	SMOOTH_TRANS_MAX,
};

enum {
	NIGHT_DIM_OFF,
	NIGHT_DIM_ON,
};

enum {
	ALPM_OFF = 0,
	ALPM_LOW_BR,
	HLPM_LOW_BR,
	ALPM_HIGH_BR,
	HLPM_HIGH_BR,
};

enum {
	PANEL_HBM_OFF,
	PANEL_HBM_ON,
	MAX_PANEL_HBM,
};

#ifdef CONFIG_SUPPORT_XTALK_MODE
enum {
	XTALK_OFF,
	XTALK_ON,
};
#endif

enum {
	IRC_MODE_MODERATO,
	IRC_MODE_FLAT_GAMMA,
	MAX_IRC_MODE,
};

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
enum {
	GRAM_CHKSUM_OFF,
	GRAM_CHKSUM_LV_TEST_1,
	GRAM_CHKSUM_LV_TEST_2,
	GRAM_CHKSUM_HV_TEST_1,
	GRAM_CHKSUM_HV_TEST_2,
	MAX_GRAM_CHKSUM,
};

enum {
	GCT_PATTERN_NONE,
	GCT_PATTERN_1,
	GCT_PATTERN_2,
	MAX_GCT_PATTERN,
};

#define PANEL_PROPERTY_GCT_PATTERN ("panel_gct_pattern")

enum {
	VDDM_ORIG,
	VDDM_LV,
	VDDM_HV,
	MAX_VDDM,
};

#define PANEL_PROPERTY_GCT_VDDM ("panel_gct_vddm")

enum {
	GRAM_TEST_OFF,
	GRAM_TEST_ON,
	GRAM_TEST_SKIPPED,
};
#endif

enum {
	DECODER_TEST_OFF,
	DECODER_TEST_ON,
	DECODER_TEST_SKIPPED,
};

enum {
	MCD_RS_1_RIGHT,
	MCD_RS_1_LEFT,
	MCD_RS_2_RIGHT,
	MCD_RS_2_LEFT,
	MAX_MCD_RS,
};

enum {
	DIM_TYPE_AID_DIMMING,
	DIM_TYPE_DIM_FLASH,
	MAX_DIM_TYPE,
};

#if defined(CONFIG_UML)
#define MAX_PANEL (128)
#else
#define MAX_PANEL (32)
#endif
#define MAX_PANEL_DDI (8)
#define MAX_PANEL_LUT (128)

enum {
	PANEL_ID,
	PANEL_MASK,
	PANEL_INDEX,
	PANEL_DDI_INDEX,
};

struct panel_id_mask {
	int id;
	int mask;
	struct list_head head;
};

struct panel_dt_lut {
	const char *name;
	struct device_node *ap_vendor_setting_node;
	struct device_node *panel_modes_node;
	struct device_node *power_ctrl_node;
#ifdef CONFIG_USDM_PANEL_FREQ_HOP
	struct device_node *freq_hop_node;
#endif
	struct list_head head;
	struct list_head id_mask_list;
	const char *dqe_suffix;
#if defined(CONFIG_USDM_SDP_ADAPTIVE_MIPI) ||\
	defined(CONFIG_USDM_ADAPTIVE_MIPI)
	struct device_node *adap_mipi_node;
#endif
};

struct panel_lut {
	u32 id;
	u32 mask;
	u32 index;
	u32 ddi_index;
};

struct panel_lut_info {
	const char *names[MAX_PANEL];
	int nr_panel;
	struct device_node *ap_vendor_setting_node[MAX_PANEL_DDI];
	int nr_ap_vendor_setting;
	struct device_node *panel_modes_node[MAX_PANEL_DDI];
	int nr_panel_modes;
	struct device_node *panel_power_ctrl[MAX_PANEL_DDI];
	int nr_panel_power_ctrl;
	struct panel_lut lut[MAX_PANEL_LUT];
	int nr_lut;
};

struct panel_properties {
	s32 temperature;
	u32 siop_enable;
	u32 adaptive_control;
	u32 alpm_mode;
	u32 cur_alpm_mode;
	s32 lpm_brightness;
	u32 lpm_opr;
	u32 lpm_fps;
	u32 cur_lpm_opr;
	u32 mcd_on;
	u32 mcd_resistance;
	int mcd_rs_range[MAX_MCD_RS][2];
	int mcd_rs_flash_range[MAX_MCD_RS][2];
#ifdef CONFIG_USDM_FACTORY_MST_TEST
	u32 mst_on;
#endif
	u32 hmd_on;
	u32 hmd_brightness;
	u32 lux;
#ifdef CONFIG_SUPPORT_XTALK_MODE
	u32 xtalk_mode;
#endif
#ifdef CONFIG_USDM_PANEL_DDI_FLASH
	u32 poc_op;
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	u32 gct_on;
	u32 gct_vddm;
	u32 gct_pattern;
	u8 gct_valid_chksum[4];
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	u32 grayspot;
#endif
	u32 poc_onoff;
	u32 key[MAX_CMD_LEVEL];
	u32 irc_mode;
#ifdef CONFIG_USDM_PANEL_DIM_FLASH
	bool dim_flash_on;
	u32 dim_flash_state;	/* success or fail */
	u32 cur_dim_type;	/* AID DIMMING or DIM FLASH */
#endif
	u32 panel_partial_disp;
	u32 panel_mode;
	/* resolution */
	u32 mres_updated;
	u32 old_mres_mode;
	u32 mres_mode;
	u32 xres;
	u32 yres;
	/* variable refresh rate */
	u32 vrr_fps;
	u32 vrr_mode;
	u32 vrr_idx;
	/* original variable refresh rate */
	u32 vrr_origin_fps;
	u32 vrr_origin_mode;
	u32 vrr_origin_idx;
	bool vrr_updated;
	struct vrr_lfd_info vrr_lfd_info;
	struct vrr_lfd_info prev_vrr_lfd_info;
	u32 dia_mode;
	u32 ub_con_cnt;
	u32 conn_det_enable;
#if defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	u32 enable_fd;
#endif
#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
	u32 brightdot_test_enable;
#endif
#ifdef CONFIG_USDM_FACTORY_VGLHIGHDOT_TEST
	u32 vglhighdot;
#endif
	u32 panel_aging;
	u32 dsi_freq;
	u32 osc_freq;
	u32 board_rev;
	bool is_valid_mtp;
};

struct panel_info {
	const char *ldi_name;
	unsigned int id[PANEL_ID_LEN];
	unsigned char vendor[PANEL_ID_LEN];
	struct panel_properties props;
	struct ddi_properties ddi_props;
	struct ddi_ops ddi_ops;
	struct panel_mres mres;

	/* platform dependent data - ex> exynos : dsim_device */
	void *pdata;
	void *dim_info[MAX_PANEL_BL_SUBDEV];
	void *dim_flash_info[MAX_PANEL_BL_SUBDEV];
	struct panel_vrr **vrrtbl;
	int nr_vrrtbl;
	struct panel_dimming_info *panel_dim_info[MAX_PANEL_BL_SUBDEV];
	int nr_panel_dim_info;
	struct list_head panel_lut_info;
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	struct common_panel_display_modes *common_panel_modes;
#endif
#ifdef CONFIG_USDM_PANEL_RCD
	struct panel_rcd_data *rcd_data;
#endif
	const char *dqe_suffix;
	const char *ezop_json;
};

struct attr_show_args {
	const char *name;
	char *buf;
	size_t size;
};

struct attr_store_args {
	const char *name;
	const char *buf;
	size_t size;
};

struct attr_exist_args {
	const char *name;
};

enum panel_sysfs_attr_flag {
	PA_USER =			(BIT_MASK(1)),	/* USER build */
	PA_FACTORY =			(BIT_MASK(2)),	/* FACTORY build */
	PA_DEBUG_ONLY =		(BIT_MASK(3)),	/* Not shown in case of ship build  */
	PA_DEFAULT =			PA_USER | PA_FACTORY,
};

struct panel_device_attr {
	struct device_attribute dev_attr;
	ssize_t (*store)(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
	u32 flags;
	bool node_created;
};

#define __PANEL_ATTR_RO(_name, _mode, _flags) {	\
				.dev_attr = __ATTR(_name, _mode,		\
					 PN_CONCAT(_name, show), NULL),		\
				.flags = _flags,				\
			}

#define __PANEL_ATTR_WO(_name, _mode, _flags) {	\
				.dev_attr = __ATTR(_name, _mode,		\
					 NULL, sysfs_store_check_test_mode),		\
				.flags = _flags,				\
				.store = PN_CONCAT(_name, store) \
			}

#define __PANEL_ATTR_RW(_name, _mode, _flags) {	\
				.dev_attr = __ATTR(_name, _mode,		\
					 PN_CONCAT(_name, show), sysfs_store_check_test_mode),		\
				.flags = _flags,				\
				.store = PN_CONCAT(_name, store) \
			}

enum sysfs_arg_type {
	SYSFS_ARG_TYPE_NONE,
	SYSFS_ARG_TYPE_S32,
	SYSFS_ARG_TYPE_U32,
	SYSFS_ARG_TYPE_STR,
	MAX_SYSFS_ARG_TYPE,
};

#define MAX_SYSFS_ARG_NUM		(6)
#define MAX_SYSFS_ARG_STR_LEN	(32)

struct sysfs_arg {
	const char *name;
	unsigned int nargs;
	enum sysfs_arg_type type;
};

struct sysfs_arg_out {
	union {
		s32 val_s32;
		u32 val_u32;
		char val_str[MAX_SYSFS_ARG_STR_LEN];
	} d[MAX_SYSFS_ARG_NUM];
	unsigned int nargs;
};

#ifdef CONFIG_USDM_FACTORY_SSR_TEST
enum ssr_test_result {
	PANEL_SSR_TEST_FAIL = 0,
	PANEL_SSR_TEST_PASS = 1,
	MAX_PANEL_SSR_TEST
};
#endif

#ifdef CONFIG_USDM_FACTORY_ECC_TEST
enum ecc_test_result {
	PANEL_ECC_TEST_FAIL = 0,
	PANEL_ECC_TEST_PASS = 1,
	MAX_PANEL_ECC_TEST
};
#endif

enum decoder_test_result {
	PANEL_DECODER_TEST_FAIL = -1,
	PANEL_DECODER_TEST_PASS = 1,
	MAX_PANEL_DECODER_TEST
};

#if defined(CONFIG_USDM_PANEL_VCOM_TRIM_TEST)
enum vcom_trim_test_result {
	PANEL_VCOM_TRIM_TEST_FAIL = 0,
	PANEL_VCOM_TRIM_TEST_PASS = 1,
	MAX_PANEL_VCOM_TRIM_TEST
};
#endif

static inline int search_table_u32(u32 *tbl, u32 sz_tbl, u32 value)
{
	int i;

	if (!tbl)
		return -EINVAL;

	for (i = 0; i < sz_tbl; i++)
		if (tbl[i] == value)
			return i;

	return -1;
}

static inline int search_table(void *tbl, int itemsize, u32 sz_tbl, void *value)
{
	int i;

	if (!tbl)
		return -EINVAL;

	for (i = 0; i < sz_tbl; i++)
		if (!memcmp(tbl + (i * itemsize),
					value, itemsize))
			return i;

	return -1;
}

#define disp_div_round(n, m) ((((n) * 10 / (m)) + 5) / 10)

const char *cmd_type_to_string(u32 type);
int string_to_cmd_type(const char *str);
int register_common_panel(struct common_panel_info *info);
int panel_vote_up_to_probe(struct panel_device *panel);
int deregister_common_panel(struct common_panel_info *info);
struct panel_dt_lut *find_panel_lut(struct panel_device *panel, u32 panel_id);
struct maptbl *find_panel_maptbl_by_substr(struct panel_device *panel, char *substr);
struct common_panel_info *find_panel(struct panel_device *panel, u32 id);
void print_panel_lut(struct panel_dt_lut *lut_info);
bool check_seqtbl_exist(struct panel_device *panel, char *seqname);
struct seqinfo *find_panel_seq_by_name(struct panel_device *panel, char *seqname);
struct pktinfo *find_packet_suffix(struct seqinfo *seqtbl, char *name);
struct panel_dimming_info *find_panel_dimming(struct panel_info *panel_data, char *name);
int execute_sequence_nolock(struct panel_device *panel, struct seqinfo *seq);
int panel_do_seqtbl(struct panel_device *panel, struct seqinfo *seqtbl);
int panel_do_seqtbl_by_name_nolock(struct panel_device *panel, char *seqname);
int panel_do_seqtbl_by_name(struct panel_device *panel, char *seqname);
struct resinfo *find_panel_resource(struct panel_device *panel, char *name);
bool is_panel_resource_initialized(struct panel_device *panel, char *name);
int panel_resource_clear(struct panel_device *panel, char *name);
int panel_resource_copy(struct panel_device *panel, u8 *dst, char *name);
int panel_resource_copy_and_clear(struct panel_device *panel, u8 *dst, char *name);
int get_panel_resource_size(struct panel_device *panel, char *name);
int panel_resource_update(struct panel_device *panel, struct resinfo *res);
int panel_resource_update_by_name(struct panel_device *panel, char *name);
int panel_do_init_maptbl(struct panel_device *panel, struct maptbl *maptbl);
struct dumpinfo *find_panel_dumpinfo(struct panel_device *panel, char *name);
struct resinfo *panel_get_dump_resource(struct panel_device *panel, char *name);
bool panel_is_dump_status_success(struct panel_device *panel, char *name);
int panel_init_dumpinfo(struct panel_device *panel, char *name);
int panel_dumpinfo_update(struct panel_device *panel, struct dumpinfo *info);
int panel_rx_nbytes(struct panel_device *panel, u32 type, u8 *buf, u8 addr, u32 pos, u32 len);
int panel_tx_nbytes(struct panel_device *panel,	u32 type, u8 *buf, u8 addr, u32 pos, u32 len);
int panel_verify_tx_packet(struct panel_device *panel, u8 *src, u32 ofs, u8 len);
int panel_set_key(struct panel_device *panel, int level, bool on);
struct pktinfo *alloc_static_packet(char *name, u32 type, u8 *data, u32 dlen);
int check_panel_active(struct panel_device *panel, const char *caller);
int panel_dsi_wait_for_vsync(struct panel_device *panel, u32 timeout);
int panel_dsi_set_bypass(struct panel_device *panel, bool on);
int panel_dsi_get_bypass(struct panel_device *panel);
int panel_dsi_set_commit_retry(struct panel_device *panel, bool on);
int panel_dsi_print_dpu_event_log(struct panel_device *panel);
int panel_dsi_dump_dpu_register(struct panel_device *panel);
int panel_dsi_set_lpdt(struct panel_device *panel, bool on);
int panel_wake_lock(struct panel_device *panel, unsigned long timeout);
int panel_wake_unlock(struct panel_device *panel);
int panel_emergency_off(struct panel_device *panel);
#if defined(CONFIG_USDM_PANEL_FREQ_HOP) ||\
	defined(CONFIG_USDM_SDP_ADAPTIVE_MIPI) ||\
	defined(CONFIG_USDM_ADAPTIVE_MIPI)
int panel_set_freq_hop(struct panel_device *panel, struct freq_hop_param *param);
#endif
int panel_trigger_recovery(struct panel_device *panel);
int panel_parse_ap_vendor_node(struct panel_device *panel, struct device_node *node);
int panel_flush_image(struct panel_device *panel);
int read_panel_id(struct panel_device *panel, u8 *buf);
int panel_resource_prepare(struct panel_device *panel);
void print_panel_resource(struct panel_device *panel);
int panel_cmdq_get_size(struct panel_device *panel);
DECLARE_REDIRECT_MOCKABLE(panel_cmdq_flush, RETURNS(int), PARAMS(struct panel_device *));
#ifdef CONFIG_USDM_PANEL_SYSFS
int panel_sysfs_probe(struct panel_device *panel);
int panel_sysfs_remove(struct panel_device *panel);
ssize_t attr_store_for_each(struct class *cls, const char *name, const char *buf, size_t size);
ssize_t attr_show_for_each(struct class *cls, const char *name, char *buf);
ssize_t attr_exist_for_each(struct class *cls, const char *name);
#else
static inline int panel_sysfs_probe(struct panel_device *panel) { return 0; }
static inline int panel_sysfs_remove(struct panel_device *panel) { return 0; }
static inline ssize_t attr_store_for_each(struct class *cls, const char *name, const char *buf, size_t size) { return 0; }
static inline ssize_t attr_show_for_each(struct class *cls, const char *name, char *buf) { return 0; }
static inline ssize_t attr_exist_for_each(struct class *cls, const char *name) { return 0; }
#endif
ssize_t sysfs_store_check_test_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
#define IS_PANEL_ACTIVE(_panel) check_panel_active(_panel, __func__)
#endif /* __PANEL_H__ */
