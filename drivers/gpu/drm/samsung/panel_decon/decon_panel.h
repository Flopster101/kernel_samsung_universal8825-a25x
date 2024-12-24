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
#include "decon_panel_kunit.h"
#include "decon_maptbl.h"
#include "decon_util.h"

#include "decon_panel_bl.h"

#ifdef CONFIG_PANEL_AID_DIMMING
#include "decon_dimming.h"
#include "decon_panel_dimming.h"
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "decon_mdnie.h"
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
#include "decon_copr.h"
#endif

#include "decon_panel_vrr.h"

#include "decon_panel_modes.h"
#ifdef CONFIG_PANEL_NOTIFY
#include <linux/panel_notify.h>
#endif

#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "./aod/aod_drv.h"
#endif

#include "decon_panel_obj.h"

enum {
	MIPI_DSI_WR_UNKNOWN = 0,
	MIPI_DSI_WR_GEN_CMD,
	MIPI_DSI_WR_CMD_NO_WAKE,
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

#ifdef CONFIG_MCD_PANEL_FACTORY
#define CONFIG_SUPPORT_PANEL_SWAP
#define CONFIG_SUPPORT_XTALK_MODE
#define CONFIG_SUPPORT_ISC_DEFECT
//#define CONFIG_SUPPORT_BRIGHTDOT_TEST
#define CONFIG_SELFMASK_FACTORY
#endif

#define CONFIG_SUPPORT_GRAYSPOT_TEST

#define CONFIG_SUPPORT_SPI_IF_SEL

#define to_panel_device(_m_)	container_of(_m_, struct panel_device, _m_)

/* DUMMY PANEL NAME */
#define __pn_name__	xx_xx
#define __PN_NAME__	XX_XX

#define PN_CONCAT(a, b)  _PN_CONCAT(a, b)
#define _PN_CONCAT(a, b) a ## _ ## b

#if IS_ENABLED(CONFIG_PANEL_ID_READ_REG_ADAEAF)
#define PANEL_ID_REG		(0xAD)
#elif IS_ENABLED(CONFIG_PANEL_ID_READ_REG_DADBDC) || \
	(!IS_ENABLED(CONFIG_PANEL_ID_READ_REG_04) && IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_TFT_COMMON))
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
#define PANEL_RDDPM_LEN		(3)
#define PANEL_RDDSM_LEN		(3)
#define PANEL_VENDOR_LEN	(3)

#define NORMAL_TEMPERATURE			(25)
#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define UNDER_MINUS_15(temperature)	(temperature <= -15)
#define UNDER_0(temperature)	(temperature <= 0)
#define CAPS_IS_ON(level)	(level >= 41)
#define PANEL_WAIT_VSYNC_TIMEOUT_MSEC	(110)

struct mdnie_info;

/*
 * [command types]
 * 1. delay
 * 2. external pin control
 * 3. packet write
 */

/* common cmdinfo */
struct cmdinfo {
	u32 type;
	char *name;
};

/* delay command */
struct delayinfo {
	u32 type;
	char *name;
	u32 usec;
	u32 nframe;
	ktime_t s_time;
};

/* variable delay command */
struct variable_delayinfo {
	u32 type;
	char *name;
	u32 usec;
	ktime_t mark_time;
	int (*func_delay)(struct panel_device *panel, ktime_t s_time, ktime_t mark_time);
	int (*func_mark)(struct panel_device *panel, ktime_t s_time, ktime_t *mark_time);
	struct cmdinfo mark;
};

struct timer_delay_begin_info {
	u32 type;
	char *name;
	struct delayinfo *delay;
};

#define DLYINFO(_name_) (_name_)

#define TIMER_DLYINFO_BEGIN(_name_) PN_CONCAT(begin, _name_)
#define TIMER_DLYINFO(_name_) (_name_)

#define VARIABLE_DLYINFO(_name_) (_name_)
#define VARIABLE_DLYINFO_MARK(_name_) ((_name_).mark)

#define DEFINE_PANEL_UDELAY_NO_SLEEP(_name_, _usec_)	\
struct delayinfo DLYINFO(_name_) =		\
{										\
	.name = #_name_,					\
	.type = CMD_TYPE_DELAY_NO_SLEEP,	\
	.usec = (_usec_),					\
}

#define DEFINE_PANEL_MDELAY_NO_SLEEP(_name_, _msec_)	\
struct delayinfo DLYINFO(_name_) =		\
{										\
	.name = #_name_,					\
	.type = CMD_TYPE_DELAY_NO_SLEEP,	\
	.usec = (_msec_) * 1000,			\
}

#define DEFINE_PANEL_UDELAY(_name_, _usec_)	\
struct delayinfo DLYINFO(_name_) =		\
{										\
	.name = #_name_,					\
	.type = CMD_TYPE_DELAY,				\
	.usec = (_usec_),					\
}

#define DEFINE_PANEL_MDELAY(_name_, _msec_)	\
struct delayinfo DLYINFO(_name_) =		\
{										\
	.name = #_name_,					\
	.type = CMD_TYPE_DELAY,				\
	.usec = (_msec_) * 1000,			\
}

#define DEFINE_PANEL_FRAME_DELAY(_name_, _nframe_)	\
struct delayinfo DLYINFO(_name_) =		\
{										\
	.name = #_name_,					\
	.type = CMD_TYPE_FRAME_DELAY,		\
	.nframe = (_nframe_),				\
}

#define DEFINE_PANEL_VSYNC_DELAY(_name_, _nvsync_)	\
struct delayinfo DLYINFO(_name_) =		\
{										\
	.name = #_name_,					\
	.type = CMD_TYPE_VSYNC_DELAY,		\
	.nframe = (_nvsync_),				\
}

#define DEFINE_PANEL_VARIABLE_DELAY_MARK(_name_, _func_delay_, _func_mark_)	\
struct variable_delayinfo DLYINFO(_name_) =		\
{										\
	.name = #_name_,					\
	.type = CMD_TYPE_VARIABLE_DELAY,	\
	.usec = 0,							\
	.mark_time = (0ULL),				\
	.func_delay = (_func_delay_),		\
	.func_mark = (_func_mark_),			\
	.mark = {							\
		.type = CMD_TYPE_VARIABLE_DELAY_MARK,		\
		.name = #_name_,							\
	},									\
}

#define DEFINE_PANEL_VARIABLE_DELAY(_name_, _func_delay_)	\
	DEFINE_PANEL_VARIABLE_DELAY_MARK(_name_, _func_delay_, NULL)

#define DEFINE_PANEL_TIMER_BEGIN(_name_, _timer_delay_)		\
struct timer_delay_begin_info TIMER_DLYINFO_BEGIN(_name_) =	\
{										\
	.name = #_name_,					\
	.type = CMD_TYPE_TIMER_DELAY_BEGIN,	\
	.delay = (_timer_delay_),			\
}

#define DEFINE_PANEL_TIMER_UDELAY(_name_, _usec_)	\
struct delayinfo TIMER_DLYINFO(_name_) =		\
{										\
	.name = #_name_,					\
	.type = CMD_TYPE_TIMER_DELAY,		\
	.usec = (_msec_),					\
	.s_time = (0ULL),					\
}

#define DEFINE_PANEL_TIMER_MDELAY(_name_, _msec_)	\
struct delayinfo TIMER_DLYINFO(_name_) =		\
{										\
	.name = #_name_,					\
	.type = CMD_TYPE_TIMER_DELAY,		\
	.usec = (_msec_) * 1000,			\
	.s_time = (0ULL),					\
}

/* external pin control command */
struct pininfo {
	u32 type;
	char *name;
	int pin;
	int onoff;
};

/* packet command */
enum {
	CMD_TYPE_NONE,
	CMD_TYPE_DELAY,
	CMD_TYPE_DELAY_NO_SLEEP,
	CMD_TYPE_FRAME_DELAY,
	CMD_TYPE_VSYNC_DELAY,
	CMD_TYPE_TIMER_DELAY_BEGIN,
	CMD_TYPE_TIMER_DELAY,
	CMD_TYPE_VARIABLE_DELAY,
	CMD_TYPE_VARIABLE_DELAY_MARK,
	CMD_TYPE_PINCTL,
	CMD_TYPE_TX_PKT_START,
	CMD_PKT_TYPE_NONE = CMD_TYPE_TX_PKT_START,
	DSI_PKT_TYPE_WR,
	DSI_PKT_TYPE_WR_NO_WAKE,
	DSI_PKT_TYPE_COMP,
	DSI_PKT_TYPE_PPS,
	/* Command to write ddi side ram */
	DSI_PKT_TYPE_WR_SR,
	/*write command to side ram with busy wait*/
	DSI_PKT_TYPE_SR_FAST,
	DSI_PKT_TYPE_WR_MEM,
	CMD_TYPE_TX_PKT_END = DSI_PKT_TYPE_WR_MEM,
	CMD_TYPE_RX_PKT_START,
	SPI_PKT_TYPE_RD = CMD_TYPE_RX_PKT_START,
	DSI_PKT_TYPE_RD_POC,
	DSI_PKT_TYPE_RD,
	CMD_TYPE_RX_PKT_END = DSI_PKT_TYPE_RD,
	SPI_PKT_TYPE_WR,
	SPI_PKT_TYPE_SETPARAM,
	CMD_TYPE_I2C_START,
	I2C_PKT_TYPE_WR = CMD_TYPE_I2C_START,
	I2C_PKT_TYPE_RD,
	CMD_TYPE_I2C_END = I2C_PKT_TYPE_RD,
	CMD_TYPE_RES,
	CMD_TYPE_SEQ,
	CMD_TYPE_KEY,
	CMD_TYPE_MAP,
	CMD_TYPE_DMP,
	CMD_TYPE_COND_IF,
	CMD_TYPE_COND_EL,
	CMD_TYPE_COND_FI,
	CMD_TYPE_PCTRL,
	CMD_TYPE_PROP,
	MAX_CMD_TYPE,
};

#define IS_CMD_TYPE_TX_MEM_PKT(_type_) \
	(DSI_PKT_TYPE_WR_SR <= (_type_) && (_type_) <= DSI_PKT_TYPE_WR_MEM)

#define IS_CMD_TYPE_TX_PKT(_type_) \
	((CMD_TYPE_TX_PKT_START <= (_type_) && (_type_) <= CMD_TYPE_TX_PKT_END) ||\
	 (_type_) == CMD_TYPE_KEY)

#define IS_CMD_TYPE_RX_PKT(_type_) \
	(CMD_TYPE_RX_PKT_START <= (_type_) && (_type_) <= CMD_TYPE_RX_PKT_END)

#define IS_CMD_TYPE_DELAY(_type_) \
	((_type_) >= CMD_TYPE_DELAY && (_type_) <= CMD_TYPE_FRAME_DELAY)

#define IS_CMD_TYPE_TIMER_DELAY(_type_) \
	((_type_) == CMD_TYPE_TIMER_DELAY_BEGIN || \
	 (_type_) == CMD_TYPE_TIMER_DELAY)

#define IS_CMD_TYPE_COND(_type_) \
	((_type_) >= CMD_TYPE_COND_IF && (_type_) <= CMD_TYPE_COND_FI)

#define IS_CMD_TYPE_VARIABLE_DELAY(_type_) \
	((_type_) == CMD_TYPE_VARIABLE_DELAY || (_type_) == CMD_TYPE_VARIABLE_DELAY_MARK)

enum {
	RES_UNINITIALIZED,
	RES_INITIALIZED,
	MAX_RES_INIT_STATE
};

enum {
	KEY_NONE,
	KEY_DISABLE,
	KEY_ENABLE,
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
	u32 type;
	char *name;
	enum cmd_level level;
	u32 en;
	struct pktinfo *packet;
};

#define KEYINFO(_name_) PN_CONCAT(key, _name_)
#define KEYINFO_INIT(_name_, _lv_, _en_, _pkt_)	\
{									\
	.name = #_name_,				\
	.type = (CMD_TYPE_KEY),			\
	.level = (_lv_),				\
	.en = (_en_),				\
	.packet = (_pkt_),				\
}

#define DEFINE_PANEL_KEY(_name_, _lv_, _en_, _pkt_)	\
struct keyinfo KEYINFO(_name_) = KEYINFO_INIT(_name_, _lv_, _en_, _pkt_)

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

struct pkt_update_info {
	u32 offset;
	struct maptbl *maptbl;
	u32 nr_maptbl;
	int (*getidx)(struct pkt_update_info *pktui);
	void *pdata;
};

enum {
	PKT_OPTION_NONE = 0,
	PKT_OPTION_CHECK_TX_DONE = (1 << 0),
	PKT_OPTION_SR_ALIGN_4 = (1 << 1),
	PKT_OPTION_SR_ALIGN_12 = (1 << 2),
	PKT_OPTION_SR_ALIGN_16 = (1 << 3),
	PKT_OPTION_SR_REG_6C = (1 << 4),
	PKT_OPTION_SR_MAX_512 = (1 << 5),
	PKT_OPTION_SR_MAX_1024 = (1 << 6),
	PKT_OPTION_SR_MAX_2048 = (1 << 7),
};

struct pktinfo {
	u32 type;
	char *name;
	u32 addr;
	u8 *data;
	u32 offset;
	u32 dlen;
	struct pkt_update_info *pktui;
	u32 nr_pktui;
	u32 option;
};

struct rdinfo {
	u32 type;
	char *name;
	u32 addr;
	u32 offset;
	u32 len;
	u8 *data;
};

struct res_update_info {
	u32 offset;
	struct rdinfo *rditbl;
};

struct resinfo {
	u32 type;
	char *name;
	int state;
	u8 *data;
	u32 dlen;
	struct res_update_info *resui;
	u32 nr_resui;
};

struct dumpinfo {
	u32 type;
	char *name;
	struct resinfo *res;
	int (*callback)(struct dumpinfo *info);
};

struct condinfo {
	u32 type;
	char *name;
	bool (*cond)(struct panel_device *panel);
};

struct condinfo_cont {
	struct condinfo cond_if;
	struct condinfo cond_el;
	struct condinfo cond_fi;
};

struct pctrlinfo {
	u32 type;
	char *name;
	char *pctrl_name;
};

struct propinfo {
	u32 type;
	char *name;
	char *prop_name;
	enum PANEL_OBJ_PROP_TYPE prop_type;
	union {
		unsigned int value;
		char *str;
	};
};

#define DUMPINFO_INIT(_name_, _res_, _callback_)	\
{											\
	.name = #_name_,						\
	.type = (CMD_TYPE_DMP),					\
	.res = (_res_),							\
	.callback = (_callback_),				\
}

#define RESUI(_name_) PN_CONCAT(resui, _name_)
#define RESINFO(_name_) PN_CONCAT(res, _name_)
#define RESNAME(_name_) (_name_)
#define RDINFO(_name_) PN_CONCAT(rd, _name_)

#define RDINFO_INIT(_name_, _type_, _addr_, _offset_, _len_)	\
{											\
	.name = #_name_,						\
	.type = (_type_),						\
	.addr = (_addr_),						\
	.offset = (_offset_),					\
	.len = (_len_),							\
}

#define DEFINE_RDINFO(_name_, _type_, _addr_, _offset_, _len_)	\
struct rdinfo RDINFO(_name_) = RDINFO_INIT(_name_, _type_, _addr_, _offset_, _len_)

#define RESUI_INIT(_rditbl_, _offset_)\
{												\
	.offset = (_offset_),						\
	.rditbl = (_rditbl_),						\
}

#define DECLARE_RESUI(_name_) \
struct res_update_info RESUI(_name_)[]

#define DEFINE_RESUI(_name_, _rditbl_, _offset_)	\
struct res_update_info RESUI(_name_)[] =			\
{													\
	RESUI_INIT(_rditbl_, _offset_),		\
}

#define RESINFO_INIT(_name_, _arr_, _resui_)	\
{												\
	.name = #_name_,							\
	.type = (CMD_TYPE_RES),						\
	.state = (RES_UNINITIALIZED),				\
	.data = (_arr_),							\
	.dlen = ARRAY_SIZE((_arr_)),				\
	.resui = (_resui_),							\
	.nr_resui = (ARRAY_SIZE(_resui_)),			\
}

#define DEFINE_RESOURCE(_name_, _arr_, _resui_)	\
struct resinfo RESINFO(_name_) = RESINFO_INIT(_name_, _arr_, _resui_)

#define PKTUI(_name_) PN_CONCAT(pktui, _name_)
#define PKTINFO(_name_) PN_CONCAT(pkt, _name_)

#define DECLARE_PKTUI(_name_) \
struct pkt_update_info PKTUI(_name_)[]

#define DEFINE_PKTUI(_name_, _maptbl_, _update_offset_)	\
struct pkt_update_info PKTUI(_name_)[] =			\
{													\
	{												\
		.offset = (_update_offset_),				\
		.maptbl = (_maptbl_),						\
		.nr_maptbl = (1),							\
		.getidx = (NULL),							\
	},												\
}

#define PKTINFO_INIT(_name_, _type_, _arr_, _ofs_, _option_, _pktui_, _nr_pktui_)	\
{												\
	.name = (#_name_),							\
	.type = (_type_),							\
	.data = (_arr_),							\
	.offset = (_ofs_),							\
	.dlen = ARRAY_SIZE((_arr_)),				\
	.pktui = (_pktui_),							\
	.nr_pktui = (_nr_pktui_),					\
	.option = (_option_),							\
}

#define DEFINE_PACKET(_name_, _type_, _arr_, _ofs_, _option_, _pktui_, _nr_pktui_)	\
struct pktinfo PKTINFO(_name_) = PKTINFO_INIT(_name_, _type_, _arr_, _ofs_, _option_, _pktui_, _nr_pktui_)

#define DEFINE_VARIABLE_PACKET(_name_, _type_, _arr_, _ofs_)				\
DEFINE_PACKET(_name_, _type_, _arr_, _ofs_, 0, PKTUI(_name_), ARRAY_SIZE(PKTUI(_name_)))

#define DEFINE_VARIABLE_PACKET_WITH_OPTION(_name_, _type_, _arr_, _ofs_, _option_)				\
DEFINE_PACKET(_name_, _type_, _arr_, _ofs_, _option_, PKTUI(_name_), ARRAY_SIZE(PKTUI(_name_)))

#define DEFINE_STATIC_PACKET(_name_, _type_, _arr_, _ofs_)		\
DEFINE_PACKET(_name_, _type_, _arr_, _ofs_, 0, (NULL), (0))

#define DEFINE_STATIC_PACKET_WITH_OPTION(_name_, _type_, _arr_, _ofs_, _option_)		\
DEFINE_PACKET(_name_, _type_, _arr_, _ofs_, _option_, (NULL), (0))

#define PN_MAP_DATA_TABLE(_name_)	PN_CONCAT(PN_CONCAT(__pn_name__, _name_), table)
#define PN_CMD_DATA_TABLE(_name_)	PN_CONCAT(__PN_NAME__, _name_)

#define DEFINE_PN_STATIC_PACKET(_name_, _type_, _arr_)			\
DEFINE_PACKET(PN_CONCAT(__pn_name__, _name_), _type_,	\
		PN_CONCAT(__PN_NAME__, _arr_), _ofs_, 0, NULL, 0)

#define CONDINFO_IF(_name_) ((PN_CONCAT(cond_info, _name_)).cond_if)
#define CONDINFO_EL(_name_) ((PN_CONCAT(cond_info, _name_)).cond_el)
#define CONDINFO_FI(_name_) ((PN_CONCAT(cond_info, _name_)).cond_fi)

#define DEFINE_COND(_name_, _func_)						\
struct condinfo_cont PN_CONCAT(cond_info, _name_) = {	\
	.cond_if = {											\
		.name = (#_name_),								\
		.type = (CMD_TYPE_COND_IF),					\
		.cond = (void *) _func_,						\
	},													\
	.cond_el = {											\
		.name = (#_name_),								\
		.type = (CMD_TYPE_COND_EL),					\
		.cond = NULL,									\
	},													\
	.cond_fi = {											\
		.name = (#_name_),								\
		.type = (CMD_TYPE_COND_FI),					\
		.cond = NULL,									\
	},													\
}

#define DEFINE_PCTRL(_name_, _pctrl_name_)				\
struct pctrlinfo PN_CONCAT(pctrl_info_, _name_) = {		\
	.type = (CMD_TYPE_PCTRL),							\
	.name = (#_name_),									\
	.pctrl_name = (_pctrl_name_),						\
}

#define PCTRLINFO(_name_) PN_CONCAT(pctrl_info_, _name_)

#define DEFINE_SETPROP_VALUE(_name_, _prop_name_, _value_)			\
struct propinfo PN_CONCAT(propinfo_, _name_) = {		\
	.type = (CMD_TYPE_PROP),				\
	.name = (#_name_),					\
	.prop_name = (_prop_name_),					\
	.prop_type = PANEL_OBJ_PROP_TYPE_VALUE,		\
	.value = (_value_),				\
}
#define SETPROP(_name_) PN_CONCAT(propinfo_, _name_)

enum PANEL_SEQ {
	PANEL_INIT_SEQ,
	PANEL_EXIT_SEQ,
	PANEL_BOOT_SEQ,
	PANEL_RES_INIT_SEQ,
#ifdef CONFIG_SUPPORT_DIM_FLASH
	PANEL_DIM_FLASH_RES_INIT_SEQ,
#endif
#ifdef CONFIG_SUPPORT_GM2_FLASH
	PANEL_GM2_FLASH_RES_INIT_SEQ,
#endif
	PANEL_MAP_INIT_SEQ,
	PANEL_DISPLAY_ON_SEQ,
	PANEL_DISPLAY_OFF_SEQ,
	PANEL_ALPM_INIT_SEQ,
	PANEL_ALPM_ENTER_SEQ,
	PANEL_ALPM_SET_BL_SEQ = PANEL_ALPM_ENTER_SEQ,
	PANEL_ALPM_DELAY_SEQ,
	PANEL_ALPM_EXIT_SEQ,
	PANEL_ALPM_EXIT_AFTER_SEQ,
	PANEL_SET_BL_SEQ,
#ifdef CONFIG_SUPPORT_HMD
	PANEL_HMD_ON_SEQ,
	PANEL_HMD_OFF_SEQ,
	PANEL_HMD_BL_SEQ,
#endif
	PANEL_HBM_ON_SEQ,
	PANEL_HBM_OFF_SEQ,
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	PANEL_DISPLAY_MODE_SEQ,
#endif
#ifdef CONFIG_SUPPORT_DSU
	PANEL_DSU_SEQ,
#endif
	PANEL_FPS_SEQ,
	PANEL_BLACK_AND_FPS_SEQ,
	PANEL_MCD_ON_SEQ,
	PANEL_MCD_OFF_SEQ,
	PANEL_MCD_RS_ON_SEQ,
	PANEL_MCD_RS_OFF_SEQ,
	PANEL_MCD_RS_READ_SEQ,
#ifdef CONFIG_SUPPORT_MST
	PANEL_MST_ON_SEQ,
	PANEL_MST_OFF_SEQ,
#endif
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	PANEL_GCT_ENTER_SEQ,
	PANEL_GCT_VDDM_SEQ,
	PANEL_GCT_IMG_UPDATE_SEQ,
	PANEL_GCT_IMG_0_UPDATE_SEQ,
	PANEL_GCT_IMG_1_UPDATE_SEQ,
	PANEL_GCT_EXIT_SEQ,
#endif
#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	PANEL_FD_SEQ,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	PANEL_GRAYSPOT_ON_SEQ,
	PANEL_GRAYSPOT_OFF_SEQ,
#endif
#ifdef CONFIG_SUPPORT_TDMB_TUNE
	PANEL_TDMB_TUNE_SEQ,
#endif
#ifdef CONFIG_SUPPORT_ISC_DEFECT
	PANEL_CHECK_ISC_DEFECT_SEQ,
#endif
#ifdef CONFIG_SUPPORT_SPI_IF_SEL
	PANEL_SPI_IF_ON_SEQ,
	PANEL_SPI_IF_OFF_SEQ,
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	PANEL_CCD_TEST_SEQ,
#endif
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	PANEL_DYNAMIC_HLPM_ON_SEQ,
	PANEL_DYNAMIC_HLPM_OFF_SEQ,
#endif
#ifdef CONFIG_SUPPORT_ISC_TUNE_TEST
	PANEL_ISC_THRESHOLD_SEQ,
	PANEL_STM_TUNE_SEQ,
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	PANEL_MAFPC_ON_SEQ,
	PANEL_MAFPC_OFF_SEQ,
	PANEL_MAFPC_SCALE_FACTOR_SEQ,
	PANEL_MAFPC_IMG_SEQ,
	PANEL_MAFPC_CHECKSUM_SEQ,
#endif
	PANEL_PARTIAL_DISP_ON_SEQ,
	PANEL_PARTIAL_DISP_OFF_SEQ,
	PANEL_DUMP_SEQ,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	PANEL_CMDLOG_DUMP_SEQ,
#endif
	PANEL_CHECK_CONDITION_SEQ,
	PANEL_DIA_ONOFF_SEQ,
#ifdef CONFIG_SUPPORT_MASK_LAYER
	PANEL_MASK_LAYER_STOP_DIMMING_SEQ,
	PANEL_MASK_LAYER_BEFORE_SEQ,
	PANEL_MASK_LAYER_AFTER_SEQ,
	PANEL_MASK_LAYER_ENTER_BR_SEQ,
	PANEL_MASK_LAYER_EXIT_BR_SEQ,
	PANEL_MASK_LAYER_EXIT_AFTER_SEQ,
#endif
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	PANEL_BRIGHTDOT_TEST_SEQ,
#endif
	PANEL_FFC_SEQ, //sequence to set ddi's ffc operation
	PANEL_OSC_SEQ, //sequence to chagne ddi's oscillator clock
#ifdef CONFIG_SUPPORT_SSR_TEST
	PANEL_SSR_TEST_SEQ,
#endif
#ifdef CONFIG_SUPPORT_ECC_TEST
	PANEL_ECC_TEST_SEQ,
#endif
	PANEL_DECODER_TEST_SEQ,
#ifdef CONFIG_SUPPORT_PANEL_VCOM_TRIM_TEST
	PANEL_VCOM_TRIM_TEST_SEQ,
#endif
	PANEL_DUMMY_SEQ,
	MAX_PANEL_SEQ,
};

/* structure of sequence table */
struct seqinfo {
	u32 type;
	char *name;
	void **cmdtbl;
	u32 size;
};

#define SEQINFO(_name_)	(_name_)
#define SEQINFO_INIT(_name_, _cmdtbl_)	\
{										\
	.type = (CMD_TYPE_SEQ),				\
	.name = (_name_),					\
	.cmdtbl = (_cmdtbl_),				\
	.size = ARRAY_SIZE((_cmdtbl_)),		\
}

#define DEFINE_SEQINFO(_name_, _cmdtbl_) \
struct seqinfo SEQINFO(_name_) = SEQINFO_INIT((#_name_), (_cmdtbl_))

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

#ifdef CONFIG_SUPPORT_MASK_LAYER
enum {
	MASK_LAYER_TRIGGER_BEFORE,
	MASK_LAYER_TRIGGER_AFTER,
	MASK_LAYER_TRIGGER_MAX,
};

enum {
	MASK_LAYER_OFF,
	MASK_LAYER_ON,
	MASK_LAYER_MAX,
};

enum {
	MASK_LAYER_HOOK_OFF,
	MASK_LAYER_HOOK_ON,
	MASK_LAYER_HOOK_MAX,
};

struct mask_layer_data {
	u32 trigger_time;
	u32 req_mask_layer;
};
#endif

#if defined(CONFIG_PANEL_DISPLAY_MODE)
struct common_panel_display_mode_bridge_ops {
	/*
	 * check if to change bridge refresh rate is permitted.
	 * (e.g. actual brightness 98nit ~ 420nit)
	 */
	bool (*check_changeable)(struct panel_device *panel);

	/*
	 * check if to jump to target refresh rate is permitted directly.
	 * (e.g. brightness, outdoor light level, copr or image updated)
	 */
	bool (*check_jumpmode)(struct panel_device *panel);
};

struct common_panel_display_mode_bridge {
	struct common_panel_display_mode *mode;
	int nframe_duration;
};

struct common_panel_display_mode {
	char name[PANEL_DISPLAY_MODE_NAME_LEN];
	struct panel_resol *resol;
	struct panel_vrr *vrr;
};

struct common_panel_display_modes {
	unsigned int num_modes;
	struct common_panel_display_mode **modes;
	struct common_panel_display_mode_bridge	*bridges;
	struct common_panel_display_mode_bridge_ops *bridge_ops;
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
};

struct ddi_ops {
	int (*ddi_init)(struct panel_device *panel, void *data, u32 size);
	int (*gamma_flash_checksum)(struct panel_device *panel, void *data, u32 size);
	int (*mtp_gamma_check)(struct panel_device *panel, void *data, u32 size);
#ifdef CONFIG_SUPPORT_SSR_TEST
	int (*ssr_test)(struct panel_device *panel, void *data, u32 size);
#endif
#ifdef CONFIG_SUPPORT_ECC_TEST
	int (*ecc_test)(struct panel_device *panel, void *data, u32 size);
#endif
	int (*decoder_test)(struct panel_device *panel, void *data, u32 size);
#ifdef CONFIG_SUPPORT_PANEL_VCOM_TRIM_TEST
	int (*vcom_trim_test)(struct panel_device *panel, void *data, u32 size);
#endif
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
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	struct panel_copr_data *copr_data;
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	struct aod_tune *aod_tune;
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
	struct panel_poc_data *poc_data;
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	struct spi_data *spi_data;
	struct spi_data **spi_data_tbl;
	int nr_spi_data_tbl;
#endif
#ifdef CONFIG_MCD_PANEL_BLIC
	struct blic_data **blic_data_tbl;
	int nr_blic_data_tbl;
#endif
#ifdef CONFIG_PANEL_FREQ_HOP
	struct freq_hop_elem *freq_hop_elems;
	int nr_freq_hop_elems;
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	struct mafpc_info *mafpc_info;
#endif

#if defined(CONFIG_PANEL_DISPLAY_MODE)
	struct common_panel_display_modes *common_panel_modes;
#endif
#ifdef CONFIG_MCD_PANEL_RCD
	struct panel_rcd_data *rcd_data;
#endif
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
	ACL_OPR_OFF,
	ACL_OPR_03P,
	ACL_OPR_06P,
	ACL_OPR_08P,
	ACL_OPR_12P,
	ACL_OPR_15P,
	ACL_OPR_MAX
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

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
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

enum {
	VDDM_ORIG,
	VDDM_LV,
	VDDM_HV,
	MAX_VDDM,
};

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

#ifdef CONFIG_SUPPORT_ISC_TUNE_TEST
enum stm_field_num {
	STM_CTRL_EN = 0,
	STM_MAX_OPT,
	STM_DEFAULT_OPT,
	STM_DIM_STEP,
	STM_FRAME_PERIOD,
	STM_MIN_SECT,
	STM_PIXEL_PERIOD,
	STM_LINE_PERIOD,
	STM_MIN_MOVE,
	STM_M_THRES,
	STM_V_THRES,
	STM_FIELD_MAX
};
#endif

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
#ifdef CONFIG_PANEL_FREQ_HOP
	struct device_node *freq_hop_node;
#endif
	struct list_head head;
	struct list_head id_mask_list;
	const char *dqe_suffix;
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
	int nr_panel_ddi;
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
#ifdef CONFIG_SUPPORT_MST
	u32 mst_on;
#endif
	u32 hmd_on;
	u32 hmd_brightness;
	u32 lux;
#ifdef CONFIG_SUPPORT_XTALK_MODE
	u32 xtalk_mode;
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
	u32 poc_op;
#endif
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	u32 gct_on;
	u32 gct_vddm;
	u32 gct_pattern;
	u8 gct_valid_chksum[4];
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	u32 grayspot;
#endif
	u32 poc_onoff;
#ifdef CONFIG_SUPPORT_TDMB_TUNE
	u32 tdmb_on;
	u32 cur_tdmb_on;
#endif
	u32 key[MAX_CMD_LEVEL];
	u32 irc_mode;
#ifdef CONFIG_SUPPORT_DIM_FLASH
	bool dim_flash_on;
	u32 dim_flash_state;	/* success or fail */
	u32 cur_dim_type;	/* AID DIMMING or DIM FLASH */
#endif
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	u32 dynamic_hlpm;
#endif
#ifdef CONFIG_SUPPORT_ISC_TUNE_TEST
	u8 isc_threshold;
	u8 stm_field_info[STM_FIELD_MAX];
#endif
	u32 panel_partial_disp;
	u32 panel_mode;
#ifdef CONFIG_PANEL_VRR_BRIDGE
	u32 target_panel_mode;
#endif
	/* resolution */
	bool mres_updated;
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
#ifdef CONFIG_PANEL_VRR_BRIDGE
	bool vrr_bridge_enable;
#endif
	bool vrr_updated;
	struct vrr_lfd_info vrr_lfd_info;
	u32 dia_mode;
	u32 ub_con_cnt;
	u32 conn_det_enable;
#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	u32 enable_fd;
#endif
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	u32 brightdot_test_enable;
#endif
	u32 dsi_freq;
	u32 osc_freq;
};

struct panel_info {
	const char *ldi_name;
	unsigned char id[PANEL_ID_LEN];
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
	struct maptbl *maptbl;
	int nr_maptbl;
	struct seqinfo *seqtbl;
	int nr_seqtbl;
	struct rdinfo *rditbl;
	int nr_rditbl;
	struct dumpinfo *dumpinfo;
	int nr_dumpinfo;
	struct resinfo *restbl;
	int nr_restbl;
	struct panel_dimming_info *panel_dim_info[MAX_PANEL_BL_SUBDEV];
	int nr_panel_dim_info;
	struct list_head panel_lut_info;
#ifdef CONFIG_MCD_PANEL_BLIC
	struct blic_data **blic_data_tbl;
	int nr_blic_data_tbl;
#endif
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	struct common_panel_display_modes *common_panel_modes;
#endif
#ifdef CONFIG_MCD_PANEL_RCD
	struct panel_rcd_data *rcd_data;
#endif
	const char *dqe_suffix;
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

#define __PANEL_ATTR_RO(_name, _mode) __ATTR(_name, _mode,		\
			 PN_CONCAT(_name, show), NULL)

#define __PANEL_ATTR_WO(_name, _mode) __ATTR(_name, _mode,		\
			 NULL, PN_CONCAT(_name, store))

#define __PANEL_ATTR_RW(_name, _mode) __ATTR(_name, _mode,		\
			 PN_CONCAT(_name, show), PN_CONCAT(_name, store))

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

#ifdef CONFIG_SUPPORT_SSR_TEST
enum ssr_test_result {
	PANEL_SSR_TEST_FAIL = 0,
	PANEL_SSR_TEST_PASS = 1,
	MAX_PANEL_SSR_TEST
};
#endif

#ifdef CONFIG_SUPPORT_ECC_TEST
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

#ifdef CONFIG_SUPPORT_PANEL_VCOM_TRIM_TEST
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

const char *pnobj_type_to_string(u32 type);
void print_data(char *data, int size);
int decon_register_common_panel(struct common_panel_info *info);
int decon_deregister_common_panel(struct common_panel_info *info);
struct maptbl *decon_find_panel_maptbl_by_index(struct panel_info *panel, int index);
struct maptbl *decon_find_panel_maptbl_by_name(struct panel_info *panel_data, char *name);
struct common_panel_info *find_panel(struct panel_device *panel, u32 id);
struct device_node *find_panel_ddi_node(struct panel_device *panel, u32 id);
struct device_node *find_panel_modes_node(struct panel_device *panel, u32 id);
const char *get_panel_lut_dqe_suffix(struct panel_device *panel, u32 id);
#ifdef CONFIG_PANEL_FREQ_HOP
struct device_node *get_panel_lut_freq_hop_node(struct panel_device *panel, u32 id);
#endif
struct device_node *find_panel_power_ctrl_node(struct panel_device *panel, u32 id);
void print_panel_lut(struct panel_dt_lut *lut_info);
bool check_seqtbl_exist(struct panel_info *panel_data, u32 index);
struct seqinfo *find_panel_seqtbl(struct panel_info *panel_data, char *name);
struct seqinfo *find_index_seqtbl(struct panel_info *panel_data, u32 index);
struct pktinfo *find_packet(struct seqinfo *seqtbl, char *name);
struct pktinfo *find_packet_suffix(struct seqinfo *seqtbl, char *name);
struct panel_dimming_info *find_panel_dimming(struct panel_info *panel_data, char *name);
int excute_seqtbl_nolock(struct panel_device *panel, struct seqinfo *seqtbl, int index);
int panel_do_seqtbl(struct panel_device *panel, struct seqinfo *seqtbl);
int decon_decon_panel_do_seqtbl_by_index_nolock(struct panel_device *panel, int index);
int decon_panel_do_seqtbl_by_index(struct panel_device *panel, int index);
struct resinfo *decon_find_panel_resource(struct panel_info *panel, char *name);
bool decon_panel_resource_initialized(struct panel_info *panel_data, char *name);
int decon_rescpy(u8 *dst, struct resinfo *res, u32 offset, u32 len);
int decon_rescpy_by_name(struct panel_info *panel, u8 *dst, char *name, u32 offset, u32 len);
int resource_clear(struct resinfo *res);
int resource_clear_by_name(struct panel_info *panel_data, char *name);
int resource_copy(u8 *dst, struct resinfo *res);
int resource_copy_by_name(struct panel_info *panel_data, u8 *dst, char *name);
int decon_resource_copy_n_clear_by_name(struct panel_info *panel_data, u8 *dst, char *name);
int decon_get_resource_size_by_name(struct panel_info *panel_data, char *name);
int get_panel_resource_size(struct resinfo *res);
int panel_resource_update(struct panel_device *panel, struct resinfo *res);
int decon_panel_resource_update_by_name(struct panel_device *panel, char *name);
int panel_dumpinfo_update(struct panel_device *panel, struct dumpinfo *info);
int panel_rx_nbytes(struct panel_device *panel, u32 type, u8 *buf, u8 addr, u32 pos, u32 len);
int panel_tx_nbytes(struct panel_device *panel,	u32 type, u8 *buf, u8 addr, u32 pos, u32 len);
u16 calc_checksum_16bit(u8 *arr, int size);
void panel_update_packet_data(struct panel_device *panel, struct pktinfo *info);
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
#if defined(CONFIG_PANEL_FREQ_HOP)
int panel_set_freq_hop(struct panel_device *panel, struct freq_hop_param *param);
#endif
int panel_parse_ap_vendor_node(struct panel_device *panel, struct device_node *node);
int panel_flush_image(struct panel_device *panel);
int read_panel_id(struct panel_device *panel, u8 *buf);
int panel_resource_prepare(struct panel_device *panel);
void print_panel_resource(struct panel_device *panel);
int find_sysfs_arg_by_name(struct sysfs_arg *arglist, int nr_arglist, char *s);
int parse_sysfs_arg(int nargs, enum sysfs_arg_type type,
		char *s, struct sysfs_arg_out *out);
int panel_cmdq_get_size(struct panel_device *panel);
DECLARE_REDIRECT_MOCKABLE(panel_cmdq_flush, RETURNS(int), PARAMS(struct panel_device *));
struct panel_dt_lut *find_panel_lut(struct panel_device *panel, u32 id);
#ifdef CONFIG_EXYNOS_DECON_LCD_SYSFS
int panel_sysfs_probe(struct panel_device *panel);
int panel_sysfs_remove(struct panel_device *panel);
ssize_t decon_attr_store_for_each(struct class *cls, const char *name, const char *buf, size_t size);
ssize_t decon_attr_show_for_each(struct class *cls, const char *name, char *buf);
ssize_t decon_attr_exist_for_each(struct class *cls, const char *name);
#else
static inline int panel_sysfs_probe(struct panel_device *panel) { return 0; }
static inline int panel_sysfs_remove(struct panel_device *panel) { return 0; }
static inline ssize_t decon_attr_store_for_each(struct class *cls, const char *name, const char *buf, size_t size) { return 0; }
static inline ssize_t decon_attr_show_for_each(struct class *cls, const char *name, char *buf) { return 0; }
static inline ssize_t decon_attr_exist_for_each(struct class *cls, const char *name) { return 0; }
#endif
#define IS_PANEL_ACTIVE(_panel) check_panel_active(_panel, __func__)
#endif /* __PANEL_H__ */
