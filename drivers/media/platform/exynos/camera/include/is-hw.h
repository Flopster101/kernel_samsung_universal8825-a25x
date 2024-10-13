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

#ifndef IS_HW_H
#define IS_HW_H

#include <linux/phy/phy.h>
#include <linux/interrupt.h>

#include "is-type.h"
#include "is-hw-chain.h"
#include "is-framemgr.h"
#include "is-device-ischain.h"
#include "is-hw-control.h"
#include "is-config.h"
#include "is-framemgr.h"
#include "is-groupmgr.h"
#include "is-interface.h"
#include "exynos-is-sensor.h"
#include "is-err.h"
#include "is-hw-phy.h"

#define IS_CSIS_VERSION(a, b, c, d) \
	(((a) << 24) + ((b) << 16) + ((c) << 8) + (d))

#define F_ID_SIZE		4	/* for frame id decoder: 4 bit */
#define CHECK_ID_60FPS(id)	((((id) >> 0) & 0xF) == 1 ||	\
				(((id) >>  4) & 0xF) == 1 ||	\
				(((id) >>  8) & 0xF) == 1 ||	\
				(((id) >> 12) & 0xF) == 1 ||	\
				(((id) >> 16) & 0xF) == 1 ||	\
				(((id) >> 20) & 0xF) == 1 ||	\
				(((id) >> 24) & 0xF) == 1 ||	\
				(((id) >> 28) & 0xF) == 1)
#define MAX_NUM_CSIS_OTF_CH	4

#define SENSOR_DUMMY_ELEMS	2

#define SHARED_META_INIT	0
#define SHARED_META_SHOT	1
#define SHARED_META_SHOT_DONE	2

/*
 * Get each lane speed (Mbps)
 * w : width, h : height, fps : framerate
 * bit : bit width per pixel, lanes : total lane number (1 ~ 4)
 * margin : S/W margin (15 means 15%)
 */
#define CSI_GET_LANE_SPEED(w, h, fps, bit, lanes, margin) \
	({u64 tmp; tmp = ((u64)w) * ((u64)h) * ((u64)fps) * ((u64)bit) / (lanes); \
	  tmp *= (100 + margin); tmp /= 100; tmp /= 1000000; (u32)tmp;})

/*
 * Get binning ratio.
 * The ratio is expressed by 0.5 step(500)
 * The improtant thing is that it was round off the ratio to the closet 500 unit.
 */
#define BINNING(x, y) (rounddown((x) * 1000 / (y), 500))

/*
 * Get size ratio.
 * w : width, h : height
 * The return value is defined of the enum ratio_size.
 */
#define SIZE_RATIO(w, h) ((w) * 10 / (h))

#define DEBUG_FRAME_COUNT		3
#define DEBUG_EXT_MAX			2

#define DUMP_SPLIT_MAX			10
#define IS_MAX_SCENARIO             (128)
#define IS_MAX_SETFILE              (128)
/*
 * This enum will be used in order to know the size ratio based upon RATIO mecro return value.
 */
enum ratio_size {
	RATIO_1_1		= 1,
	RATIO_11_9		= 12,
	RATIO_4_3		= 13,
	RATIO_3_2		= 15,
	RATIO_5_3		= 16,
	RATIO_16_9		= 17,
};

enum csis_hw_type {
	CSIS_LINK			= 0,
	CSIS_WDMA			= 1,
};

/*
 * This enum will be used for current error status by reading interrupt source.
 */
enum csis_hw_err_id {
	CSIS_ERR_ID,
	CSIS_ERR_CRC,
	CSIS_ERR_ECC,
	CSIS_ERR_WRONG_CFG,
	CSIS_ERR_OVERFLOW_VC,
	CSIS_ERR_LOST_FE_VC,
	CSIS_ERR_LOST_FS_VC,
	CSIS_ERR_SOT_VC,
	CSIS_ERR_INVALID_CODE_HS,
	CSIS_ERR_SOT_SYNC_HS,
	CSIS_ERR_DESKEW_OVER,
	CSIS_ERR_SKEW,
	CSIS_ERR_MAL_CRC,
	CSIS_ERR_VRESOL_MISMATCH,
	CSIS_ERR_HRESOL_MISMATCH,
	CSIS_ERR_CRC_CPHY,
	CSIS_ERR_DMA_OTF_OVERLAP_VC,
	CSIS_ERR_DMA_DMAFIFO_FULL,
	CSIS_ERR_DMA_ABORT_DONE,
	CSIS_ERR_DMA_FRAME_DROP_VC,
	CSIS_ERR_DMA_LASTDATA_ERROR,
	CSIS_ERR_DMA_LASTADDR_ERROR,
	CSIS_ERR_DMA_FSTART_IN_FLUSH_VC,
	CSIS_ERR_DMA_C2COM_LOST_FLUSH_VC,
	CSIS_ERR_END
};

/*
 * This enum will be used in csi_hw_s_control api to set specific functions.
 */
enum csis_hw_control_id {
	CSIS_CTRL_INTERLEAVE_MODE,
	CSIS_CTRL_LINE_RATIO,
	CSIS_CTRL_BUS_WIDTH,
	CSIS_CTRL_DMA_ABORT_REQ,
	CSIS_CTRL_ENABLE_LINE_IRQ,
	CSIS_CTRL_PIXEL_ALIGN_MODE,
	CSIS_CTRL_LRTE,
	CSIS_CTRL_DESCRAMBLE,
};

/*
 * This struct will be used in csi_hw_g_irq_src api.
 * In csi_hw_g_irq_src, all interrupt source status should be
 * saved to this structure. otf_start, otf_end, dma_start, dma_end,
 * line_end fields has virtual channel bit set info.
 * Ex. If otf end interrupt occured in virtual channel 0 and 2 and
 *        dma end interrupt occured in virtual channel 0 only,
 *   .otf_end = 0b0101
 *   .dma_end = 0b0001
 */
struct csis_irq_src {
	u32			otf_start;
	u32			otf_end;
	u32			otf_dbg;
	u32			dma_start;
	u32			dma_end;
	u32			line_end;
	u32			dma_abort;
	bool			err_flag;
	ulong			err_id[CSI_VIRTUAL_CH_MAX];
	u32			ebuf_status;
};

/*
 * This enum will be used for masking each interrupt masking.
 * The irq_ids params which masked by shifting this bit(id)
 * was sended to flite_hw_s_irq_msk.
 */
enum flite_hw_irq_id {
	FLITE_MASK_IRQ_START		= 0,
	FLITE_MASK_IRQ_END		= 1,
	FLITE_MASK_IRQ_OVERFLOW		= 2,
	FLITE_MASK_IRQ_LAST_CAPTURE	= 3,
	FLITE_MASK_IRQ_LINE		= 4,
	FLITE_MASK_IRQ_ALL,
};

/*
 * This enum will be used for current status by reading interrupt source.
 */
enum flite_hw_status_id {
	FLITE_STATUS_IRQ_SRC_START		= 0,
	FLITE_STATUS_IRQ_SRC_END		= 1,
	FLITE_STATUS_IRQ_SRC_OVERFLOW		= 2,
	FLITE_STATUS_IRQ_SRC_LAST_CAPTURE	= 3,
	FLITE_STATUS_OFY			= 4,
	FLITE_STATUS_OFCR			= 5,
	FLITE_STATUS_OFCB			= 6,
	FLITE_STATUS_MIPI_VALID			= 7,
	FLITE_STATUS_IRQ_SRC_LINE		= 8,
	FLITE_STATUS_ALL,
};

/*
 * This enum will be used in flite_hw_s_control api to set specific functions.
 */
enum flite_hw_control_id {
	FLITE_CTRL_TEST_PATTERN,
	FLITE_CTRL_LINE_RATIO,
};

/*
 * ******************
 * MIPI-CSIS H/W APIS
 * ******************
 */
u32 csi_hw_get_version(u32 __iomem *base_reg);
int csi_hw_get_ppc_mode(u32 __iomem *base_reg);
u32 csi_hw_s_fcount(u32 __iomem *base_reg, u32 vc, u32 count);
u32 csi_hw_g_fcount(u32 __iomem *base_reg, u32 vc);
int csi_hw_reset(u32 __iomem *base_reg);
int csi_hw_s_phy_sctrl_n(u32 __iomem *base_reg, u32 ctrl, u32 n);
int csi_hw_s_phy_bctrl_n(u32 __iomem *base_reg, u32 ctrl, u32 n);
int csi_hw_s_lane(u32 __iomem *base_reg, u32 lanes, u32 use_cphy);
int csi_hw_s_control(u32 __iomem *base_reg, u32 id, u32 value);
int csi_hw_s_config(u32 __iomem *base_reg, u32 channel, struct is_vci_config *config,
	u32 width, u32 height, bool potf);
int csi_hw_s_irq_msk(u32 __iomem *base_reg, bool on, bool f_id_dec);
int csi_hw_g_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src, bool clear);
int csi_hw_enable(u32 __iomem *base_reg, u32 use_cphy);
int csi_hw_disable(u32 __iomem *base_reg);
int csi_hw_dump(u32 __iomem *base_reg);
int csi_hw_vcdma_dump(u32 __iomem *base_reg);
int csi_hw_vcdma_cmn_dump(u32 __iomem *base_reg);
int csi_hw_phy_dump(u32 __iomem *base_reg, u32 instance);
int csi_hw_common_dma_dump(u32 __iomem *base_reg);
int csi_hw_mcb_dump(u32 __iomem *base_reg);
int csi_hw_ebuf_dump(u32 __iomem *base_reg);
int csi_hw_cdump(u32 __iomem *base_reg);
int csi_hw_vcdma_cdump(u32 __iomem *base_reg);
int csi_hw_vcdma_cmn_cdump(u32 __iomem *base_reg);
int csi_hw_phy_cdump(u32 __iomem *base_reg, u32 instance);
int csi_hw_common_dma_cdump(u32 __iomem *base_reg);
int csi_hw_mcb_cdump(u32 __iomem *base_reg);
int csi_hw_ebuf_cdump(u32 __iomem *base_reg);
void csi_hw_dma_reset(u32 __iomem *base_reg);
void csi_hw_s_frameptr(u32 __iomem *base_reg, u32 vc, u32 number, bool clear);
u32 csi_hw_g_frameptr(u32 __iomem *base_reg, u32 vc);
u32 csi_hw_g_dma_sbwc_type(u32 __iomem *ctl_reg);
const char *csi_hw_g_dma_sbwc_name(u32 __iomem *ctl_reg);
void csi_hw_s_dma_addr(u32 __iomem *ctl_reg, u32 __iomem *vc_reg,
		u32 vc, u32 number, u32 addr);
u32 csi_hw_g_img_stride(u32 width, u32 bpp, u32 bitwidth, u32 sbwc_type);
void csi_hw_s_dma_header_addr(u32 __iomem *ctl_reg, u32 __iomem *vc_reg,
		u32 buf_i, u32 dva);
void csi_hw_s_multibuf_dma_addr(u32 __iomem *ctl_reg, u32 __iomem *vc_reg,
		u32 vc, u32 number, u32 addr);
void csi_hw_s_multibuf_dma_fcntseq(u32 __iomem *vc_reg, u32 buffer_num, dma_addr_t addr,
				bool clear);
void csi_hw_s_dma_dbg_cnt(u32 __iomem *vc_reg, struct csis_irq_src *src, u32 vc);
void csi_hw_s_output_dma(u32 __iomem *base_reg, u32 vc, bool enable);
bool csi_hw_g_output_dma_enable(u32 __iomem *base_reg, u32 vc);
bool csi_hw_g_output_cur_dma_enable(u32 __iomem *base_reg, u32 vc);
#ifdef CONFIG_USE_SENSOR_GROUP
int csi_hw_s_config_dma(u32 __iomem *ctl_reg, u32 __iomem *vc_reg,
		u32 vc, struct is_frame_cfg *cfg, u32 hwformat, u32 dummy_pixel);
#else
int csi_hw_s_config_dma(u32 __iomem *base_reg, u32 channel, struct is_image *image, u32 hwformat, u32 dummy_pixel);
#endif
void csi_hw_dma_common_reset(u32 __iomem *base_reg, bool on);
int csi_hw_s_dma_common_dynamic(u32 __iomem *base_reg, size_t size, u32 dma_ch);
int csi_hw_s_dma_common_pattern_enable(u32 __iomem *base_reg, u32 width, u32 height, u32 fps, u32 clk);
void csi_hw_s_dma_common_pattern_disable(u32 __iomem *base_reg);
int csi_hw_s_dma_common_votf_cfg(u32 __iomem *base_reg, u32 width, u32 dma_ch, u32 vc, bool on);
int csi_hw_s_dma_common_frame_id_decoder(u32 __iomem *base_reg, u32 dma_ch,
		u32 enable);
int csi_hw_g_dma_common_frame_id(u32 __iomem *base_reg, u32 batch_num, u32 *frame_id);
void csi_hw_s_dma_common_sbwc_ch(u32 __iomem *top_reg, u32 dma_ch);
int csi_hw_clear_fro_count(u32 __iomem *dma_top_reg, u32 __iomem *vc_reg);
int csi_hw_s_fro_count(u32 __iomem *vc_cmn_reg, u32 batch_num, u32 vc);

int csi_hw_s_dma_irq_msk(u32 __iomem *base_reg, bool on);
int csi_hw_g_dma_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src, bool clear);
int csi_hw_g_dma_irq_src_vc(u32 __iomem *base_reg, struct csis_irq_src *src, u32 vc_abs, bool clear);
int csi_hw_s_config_dma_cmn(u32 __iomem *base_reg, u32 vc, u32 actual_vc, u32 hwformat, bool potf);

void csi_hw_s_mcb_qch(u32 __iomem *base_reg, bool on);
void csi_hw_s_ebuf_enable(u32 __iomem *base_reg, bool on, u32 ebuf_ch, int mode,
			u32 num_of_ebuf);
void csi_hw_s_cfg_ebuf(u32 __iomem *base_reg, u32 ebuf_ch, u32 vc, u32 width,
		u32 height);
void csi_hw_g_ebuf_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src, int ebuf_ch,
			unsigned int num_of_ebuf);
void csi_hw_s_ebuf_fake_sign(u32 __iomem *base_reg, u32 ebuf_ch);

bool csi_hw_s_bns_cfg(u32 __iomem *reg, struct is_sensor_cfg *sensor_cfg,
		u32 *width, u32 *height);
void csi_hw_s_bns_ch(u32 __iomem *reg, u32 ch);
void csi_hw_reset_bns(u32 __iomem *reg);
void csi_hw_bns_dump(u32 __iomem *reg);

void csi_hw_s_otf_preview_only(u32 __iomem *reg, u32 otf_ch, u32 en);
void csi_hw_fro_dump(u32 __iomem *reg);

/*
 * ******************
 * MIPI-PHY H/W APIS
 * ******************
 */
/* CAUTION: This struct must be same as struct in 'phy-exynos-mipi.c'. */
struct mipi_phy_desc {
	struct phy *phy;
	struct exynos_mipi_phy_data *data;
	unsigned int index;
	unsigned int iso_offset;
	unsigned int pg_offset;
	unsigned int rst_bit;
	void __iomem *regs; /* clock */
	void __iomem *regs_lane; /* lane */
};

int csi_hw_s_phy_set(struct phy *phy, u32 lanes, u32 mipi_speed,
		u32 settle, u32 instance, u32 use_cphy,
		struct phy_setfile_table *sf_table,
		void __iomem *phy_reg, void __iomem *csi_reg);
u32 csi_hw_g_mapped_phy_port(u32 csi_ch);

/*
 * ************************************
 * ISCHAIN AND CAMIF CONFIGURE H/W APIS
 * ************************************
 */
/*
 * It's for hw version
 */
#define HW_SET_VERSION(first, second, third, fourth) \
	(((first) << 24) | ((second) << 16) | ((third) << 8) | ((fourth) << 0))

/*
 * This enum will be used in is_hw_s_ctrl api.
 */
enum hw_s_ctrl_id {
	HW_S_CTRL_FULL_BYPASS,
	HW_S_CTRL_CHAIN_IRQ,
	HW_S_CTRL_HWFC_IDX_RESET,
	HW_S_CTRL_MCSC_SET_INPUT,
};

/*
 * This enum will be used in is_hw_g_ctrl api.
 */
enum hw_g_ctrl_id {
	HW_G_CTRL_FRM_DONE_WITH_DMA,
	HW_G_CTRL_HAS_MCSC,
	HW_G_CTRL_HAS_VRA_CH1_ONLY,
};

enum mcsc_cap_enum {
	MCSC_CAP_NOT_SUPPORT = 0,
	MCSC_CAP_SUPPORT,
};

/**
 * struct is_hw_mcsc_cap - capability of mcsc
 *  This Structure specified the spec of mcsc.
 * @hw_ver: type is hexa. eg. 1.22.0 -> 0b0001_0022_0000_0000
 * @max_output: the number of output port to support
 * <below fields has the value in enum mcsc_cap_enum>
 * @in_otf: capability of input otf
 * @in_dma: capability of input dma
 * @hw_fc: capability of hardware flow control
 * @out_otf: capability of output otf
 * @out_dma: capability of output dma
 * @out_hwfc: capability of output dma (each output)
 * @tdnr: capability of 3DNR feature
 */
struct is_hw_mcsc_cap {
	u32			hw_ver;
	u32			max_output;
	u32			max_djag;
	u32			max_cac;
	u32			max_uvsp;
	enum mcsc_cap_enum	in_otf;
	enum mcsc_cap_enum	in_dma;
	enum mcsc_cap_enum	hwfc;
	enum mcsc_cap_enum	out_otf[MCSC_OUTPUT_MAX];
	enum mcsc_cap_enum	out_dma[MCSC_OUTPUT_MAX];
	enum mcsc_cap_enum	out_hwfc[MCSC_OUTPUT_MAX];
	enum mcsc_cap_enum	out_post[MCSC_OUTPUT_MAX];
	bool			enable_shared_output;
	enum mcsc_cap_enum	tdnr;
	enum mcsc_cap_enum	djag;
	enum mcsc_cap_enum	cac;
	enum mcsc_cap_enum	uvsp;
	enum mcsc_cap_enum	ysum;
	enum mcsc_cap_enum	ds_vra;
};

enum is_shot_type {
	SHOT_TYPE_INTERNAL = 1,
	SHOT_TYPE_EXTERNAL,
	SHOT_TYPE_LATE,
	SHOT_TYPE_MULTI,
	SHOT_TYPE_END
};

/**
 * enum is_hw_state - the definition of HW state
 * @HW_OPEN : setbit @ open / clearbit @ close
 *            the upper layer is going to use this HW IP
 *            initialize frame manager for output or input frame control
 *            set by open stage and cleared by close stage
 *            multiple open is permitted but initialization is done
 *            at the first open only
 * @HW_INIT : setbit @ init / clearbit @ close
 *            define HW path at each instance
 *            the HW prepares context for this instance
 *            multiple init is premitted to support multi instance
 * @HW_CONFIG : setbit @ shot / clearbit @ frame start
 *              Update configuration parameters to apply each HW settings
 *              config operation must be done at least one time to run HW
 * @HW_RUN : setbit @ frame start / clearbit @ frame end
 *           running state of each HW
 *         OPEN --> INIT --> CONFIG ---> RUN
 *         | ^      | ^^     | ^           |
 *         |_|      |_||     |_|           |
 *                     |___________________|
 */
enum is_hw_state {
	HW_OPEN,
	HW_INIT,
	HW_CONFIG,
	HW_RUN,
	HW_SUSPEND,
	HW_TUNESET,
	HW_VRA_CH1_START,
	HW_MCS_YSUM_CFG,
	HW_MCS_DS_CFG,
	HW_OVERFLOW_RECOVERY,
	HW_PAFSTAT_RDMA_CFG,
	HW_END
};

enum lboffset_trigger {
	LBOFFSET_NONE,
	LBOFFSET_A_TO_B,
	LBOFFSET_B_TO_A,
	LBOFFSET_A_DIRECT,
	LBOFFSET_B_DIRECT,
	LBOFFSET_READ,
	LBOFFSET_MAX,
};

/* CAUTION: Same HWs must be continuously defined. */
enum is_hardware_id {
	DEV_HW_3AA0	= 1,
	DEV_HW_3AA1,
	DEV_HW_3AA2,
	DEV_HW_3AA3,
	DEV_HW_ISP0,
	DEV_HW_ISP1,
	DEV_HW_MCSC0,
	DEV_HW_MCSC1,
	DEV_HW_VRA,
	DEV_HW_PAF0,	/* PAF RDMA */
	DEV_HW_PAF1,
	DEV_HW_PAF2,
	DEV_HW_PAF3,
	DEV_HW_CLH0,
	DEV_HW_YPP, /* YUVPP */
	DEV_HW_LME0,
	DEV_HW_LME1,
	DEV_HW_BYRP,
	DEV_HW_RGBP,
	DEV_HW_MCFP,
	DEV_HW_ORB0,
	DEV_HW_ORB1,
	DEV_HW_END
};

enum corex_index {
	COREX_SETA = 0,
	COREX_SETB,
	COREX_SET_MAX,
};

enum lic_sram_index {
	LIC_OFFSET_0 = 0,
	LIC_OFFSET_1,
	LIC_OFFSET_2,
	LIC_TRIGGER_MODE,
	LIC_OFFSET_MAX,
};

enum lic_trigger_index {
	LIC_NO_TRIGGER = 0,
	LIC_CONTEXT0_TRIGGER,
	LIC_CONTEXT1_TRIGGER,
	LIC_CONTEXT2_TRIGGER,
	LIC_TRIGGER_MAX,
};

enum v_enum {
	V_BLANK = 0,
	V_VALID
};

enum debug_point {
	DEBUG_POINT_HW_SHOT_E = 0,
	DEBUG_POINT_HW_SHOT_X,
	DEBUG_POINT_LIB_SHOT_E,
	DEBUG_POINT_LIB_SHOT_X,
	DEBUG_POINT_RTA_REGS_E,
	DEBUG_POINT_RTA_REGS_X,
	DEBUG_POINT_FRAME_START,
	DEBUG_POINT_FRAME_END,
	DEBUG_POINT_FRAME_DMA_END,
	DEBUG_POINT_MAX,
};

struct cal_info {
	/* case CAL_TYPE_AF:
	 * Not implemented yet
	 */
	/* case CLA_TYPE_LSC_UVSP
	 * data[0]: lsc_center_x;
	 * data[1]: lsc_center_y;
	 * data[2]: lsc_radial_biquad_a;
	 * data[3]: lsc_radial_biquad_b;
	 * data[4] - data[15]: reserved
	 */
	u32 data[16];
};

struct hw_debug_info {
	u32			fcount;
	u32			instance; /* logical instance */
	u32			cpuid[DEBUG_POINT_MAX];
	unsigned long long	time[DEBUG_POINT_MAX];
};

struct hw_ip_count{
	atomic_t		fs;
	atomic_t		cl;
	atomic_t		fe;
	atomic_t		dma;
};

struct hw_ip_status {
	atomic_t		Vvalid;
	wait_queue_head_t	wait_queue;
};

struct setfile_table_entry {
	ulong addr;
	u32 size;
};

struct is_hw_ip_setfile {
	int				version;
	/* the number of setfile each sub ip has */
	u32				using_count;
	/* which subindex is used at this scenario */
	u32				index[IS_MAX_SCENARIO];
	struct setfile_table_entry	table[IS_MAX_SETFILE];
};

struct is_clk_gate {
	void __iomem			*regs;
	spinlock_t			slock;
	u32				bit[HW_SLOT_MAX];
	int				refcnt[HW_SLOT_MAX];
};

struct is_hw_sfr_dump_region {
	resource_size_t			start;
	resource_size_t			end;
};

enum is_reg_dump_type {
	IS_REG_DUMP_TO_ARRAY,
	IS_REG_DUMP_TO_LOG,
	IS_REG_DUMP_DMA,
};

/**
 * struct is_hw - fimc-is hw data structure
 * @id: unique id to represent sub IP of IS chain
 * @state: HW state flags
 * @base_addr: Each IP mmaped registers region
 * @mcuctl_addr: MCUCTL mmaped registers region
 * @priv_info: the specific structure pointer for each HW IP
 * @group: pointer to indicate the HW IP belongs to the group
 * @region: pointer to parameter region for HW setting
 * @framemgr: pointer to frame manager to manager frame list HW handled
 * @hardware: pointer to device hardware
 * @itf: pointer to interface stucture to reply command
 * @itfc: pointer to chain interface for HW interrupt
 */
struct is_hw_ip {
	u32					id;
	char					name[IS_STR_LEN];
	bool					is_leader;
	ulong					state;
	const struct is_hw_ip_ops		*ops;
	u32					debug_index[2];
	struct hw_debug_info			debug_info[DEBUG_FRAME_COUNT];
	struct hw_debug_info			debug_ext_info[DEBUG_EXT_MAX];
	struct hw_ip_count			count;
	struct hw_ip_status			status;
	atomic_t				fcount;
	atomic_t				instance;
	void __iomem				*regs[REG_SET_MAX];
	resource_size_t				regs_start[REG_SET_MAX];
	resource_size_t				regs_end[REG_SET_MAX];
	void					*priv_info;
	struct is_group			*group[IS_STREAM_COUNT];
	struct is_region			*region[IS_STREAM_COUNT];
	u32					internal_fcount[IS_STREAM_COUNT];
	struct is_framemgr			*framemgr;
	struct is_hardware			*hardware;
	/* callback interface */
	struct is_interface		*itf;
	/* control interface */
	struct is_interface_ischain	*itfc;
	struct is_hw_ip_setfile		setfile[SENSOR_POSITION_MAX];
	u32					applied_scenario;
	u32					lib_mode;
	/* for dump sfr */
	void					*sfr_dump[REG_SET_MAX];
	bool					sfr_dump_flag;
	struct is_hw_sfr_dump_region		dump_region[REG_SET_MAX][DUMP_SPLIT_MAX];
	struct is_reg				*dump_for_each_reg;
	u32					dump_reg_list_size;

	atomic_t				rsccount;
	atomic_t				run_rsccount;

	struct is_clk_gate			*clk_gate;
	u32					clk_gate_idx;

	struct timer_list			shot_timer;
	struct timer_list			lme_frame_start_timer;
	struct timer_list			lme_frame_end_timer;
	struct timer_list			orbmch_stuck_timer;

	/* fast read out in hardware */
	bool					hw_fro_en;

	/* multi-buffer */
	struct is_frame			*mframe; /* CAUTION: read only */
	u32					num_buffers; /* total number of buffers per frame */
	u32					cur_s_int; /* count of start interrupt in multi-buffer */
	u32					cur_e_int; /* count of end interrupt in multi-buffer */
#if defined(MULTI_SHOT_TASKLET)
	struct tasklet_struct			tasklet_mshot;
#elif defined(MULTI_SHOT_KTHREAD)
	struct task_struct			*mshot_task;
	struct kthread_worker			mshot_worker;
	struct kthread_work 			mshot_work;
#endif

	/* currently used for subblock inside MCSC */
	u32					subblk_ctrl;

	struct hwip_intr_handler		*intr_handler[INTR_HWIP_MAX];
};

#define CALL_HWIP_OPS(hw_ip, op, args...)	\
	(((hw_ip)->ops && (hw_ip)->ops->op) ? ((hw_ip)->ops->op(hw_ip, args)) : 0)

struct is_hw_ip_ops {
	int (*open)(struct is_hw_ip *hw_ip, u32 instance,
		struct is_group *group);
	int (*init)(struct is_hw_ip *hw_ip, u32 instance,
		struct is_group *group, bool flag);
	int (*deinit)(struct is_hw_ip *hw_ip, u32 instance);
	int (*close)(struct is_hw_ip *hw_ip, u32 instance);
	int (*enable)(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map);
	int (*disable)(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map);
	int (*shot)(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map);
	int (*set_param)(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map);
	int (*get_meta)(struct is_hw_ip *hw_ip, struct is_frame *frame,
		ulong hw_map);
	int (*get_cap_meta)(struct is_hw_ip *hw_ip, ulong hw_map, u32 instance,
		u32 fcount, u32 size, ulong addr);
	int (*frame_ndone)(struct is_hw_ip *hw_ip, struct is_frame *frame,
		u32 instance, enum ShotErrorType done_type);
	int (*load_setfile)(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map);
	int (*apply_setfile)(struct is_hw_ip *hw_ip, u32 scenario, u32 instance,
		ulong hw_map);
	int (*delete_setfile)(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map);
	void (*size_dump)(struct is_hw_ip *hw_ip);
	int (*restore)(struct is_hw_ip *hw_ip, u32 instance);
	int (*sensor_start)(struct is_hw_ip *hw_ip, u32 instance);
	int (*sensor_stop)(struct is_hw_ip *hw_ip, u32 instance);
	int (*change_chain)(struct is_hw_ip *hw_ip, u32 instance,
		u32 next_id, struct is_hardware *hardware);
	int (*set_regs)(struct is_hw_ip *hw_ip, u32 chain_id,
		u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size);
	int (*dump_regs)(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size,
		enum is_reg_dump_type dump_type);
	int (*set_config)(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf);
	int (*get_offline_data)(struct is_hw_ip *hw_ip, u32 instance,
		void *data_desc, int fcount);
	int (*notify_timeout)(struct is_hw_ip *hw_ip, u32 instance);
	void (*show_status)(struct is_hw_ip *hw_ip, u32 instance);
	void (*suspend)(struct is_hw_ip *hw_ip, u32 instance);
};

typedef int (*hw_ip_probe_fn_t)(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);

#define CALL_HW_OPS(hw, op, args...)	\
	(((hw) && (hw)->ops->op) ? ((hw)->ops->op(args)) : 0)

struct is_hardware_ops {
	void (*frame_start)(struct is_hw_ip *hw_ip, u32 instance);
	int (*config_lock)(struct is_hw_ip *hw_ip, u32 instance, u32 framenum);
	int (*frame_done)(struct is_hw_ip *hw_ip, struct is_frame *frame,
		int wq_id, u32 output_id, enum ShotErrorType done_type, bool get_meta);
	int (*frame_ndone)(struct is_hw_ip *ldr_hw_ip,
		struct is_frame *frame, u32 instance,
		enum ShotErrorType done_type);
	void (*flush_frame)(struct is_hw_ip *hw_ip,
		enum is_frame_state state,
		enum ShotErrorType done_type);
	void (*dbg_trace)(struct is_hw_ip *hw_ip, u32 fcount, u32 dbg_pts);
};
/**
 * struct is_hardware - common HW chain structure
 * @taa0: 3AA0 HW IP structure
 * @taa1: 3AA1 HW IP structure
 * @isp0: ISP0 HW IP structure
 * @isp1: ISP1 HW IP structure
 * @fd: LHFD HW IP structure
 * @framemgr: frame manager structure. each group has its own frame manager
 */
struct is_hardware {
	struct is_hw_ip		hw_ip[HW_SLOT_MAX];
	struct is_framemgr		framemgr[GROUP_ID_MAX];
	atomic_t			rsccount;
	struct mutex            itf_lock;
	const struct is_hardware_ops	*ops;

	/* keep last configuration */
	ulong				logical_hw_map[IS_STREAM_COUNT]; /* logical */
	ulong				hw_map[IS_STREAM_COUNT]; /* physical */
	u32				sensor_position[IS_STREAM_COUNT];

	struct cal_info			cal_info[SENSOR_POSITION_MAX];
	atomic_t			streaming[SENSOR_POSITION_MAX];
	atomic_t			bug_count;
	atomic_t			log_count;

	bool				video_mode;

	unsigned long			hw_recovery_flag;

	/*
	 * To deliver MCSC noise index.
	 * 0: MCSC0, 1: MCSC1
	 */
	struct camera2_ni_udm		ni_udm[2][NI_BACKUP_MAX];
	u32				lic_offset[LIC_CHAIN_NUM][LIC_CHAIN_OFFSET_NUM];
	u32				lic_offset_def[LIC_CHAIN_OFFSET_NUM];
	atomic_t			lic_updated;

	u32				ypp_internal_buf_max_width;

	atomic_t			slot_rsccount[GROUP_SLOT_MAX];
};

int is_hw_group_cfg(void *group_data);
int is_hw_group_open(void *group_data);
void is_hw_camif_init(void);
int is_hw_camif_cfg(void *sensor_data);
int is_hw_camif_open(void *sensor_data);
#ifdef USE_CAMIF_FIX_UP
int is_hw_camif_fix_up(struct is_device_sensor *sensor);
#else
#define is_hw_camif_fix_up(a) ({ int __retval = 0; do {} while (0); __retval; })
#endif
void is_hw_ischain_qe_cfg(void);
int is_hw_ischain_cfg(void *ischain_data);
int is_hw_ischain_enable(struct is_device_ischain *device);
int is_hw_ischain_disable(struct is_device_ischain *device);
int is_hw_orbmch_isr_clear_register(u32 hw_id, bool enable);
int is_hw_lme_isr_clear_register(u32 hw_id, bool enable);
int is_hw_get_address(void *itfc_data, void *pdev_data, int hw_id);
int is_hw_get_irq(void *itfc_data, void *pdev_data, int hw_id);
int is_hw_request_irq(void *itfc_data, int hw_id);
int is_hw_slot_id(int hw_id);
#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_hw_slot_id_wrap(int hw_id);
#endif
int is_get_hw_list(int group_id, int *hw_list);
int is_hw_s_ctrl(void *itfc_data, int hw_id, enum hw_s_ctrl_id id, void *val);
int is_hw_g_ctrl(void *itfc_data, int hw_id, enum hw_g_ctrl_id id, void *val);
int is_hw_query_cap(void *cap_data, int hw_id);
int is_hw_shared_meta_update(struct is_device_ischain *device,
		struct is_group *group, struct is_frame *frame, int shot_done_flag);
void __iomem *is_hw_get_sysreg(ulong core_regs);
u32 is_hw_find_settle(u32 mipi_speed, u32 use_cphy);
#ifdef ENABLE_FULLCHAIN_OVERFLOW_RECOVERY
int is_hw_overflow_recovery(void);
#endif
unsigned int get_dma(struct is_device_sensor *device, u32 *dma_ch);
void is_hw_interrupt_relay(struct is_group *group, void *hw_ip);
void is_hw_configure_llc(bool on, struct is_device_ischain *device, ulong *llc_state);
void is_hw_configure_bts_scen(struct is_resourcemgr *resourcemgr, int scenario_id);
int is_hw_get_output_slot(u32 vid);
int is_hw_get_capture_slot(struct is_frame *frame, u32 **taddr, u64 **taddr_k, u32 vid);
void * is_get_dma_blk(int type);
int is_get_dma_id(u32 vid);
int is_hw_check_changed_chain_id(struct is_hardware *hardware, u32 hw_id, u32 instance);
void is_hw_fill_target_address(u32 hw_id, struct is_frame *dst,	struct is_frame *src,
	bool reset);
#define TADDR_COPY(dst, src, taddr, reset)				\
	do {								\
		memcpy(dst->taddr, src->taddr, sizeof(src->taddr));	\
		if (reset)						\
			memset(src->taddr, 0x0, sizeof(src->taddr));	\
	} while (0);
void is_hw_chain_probe(void *core_data);
struct is_mem *is_hw_get_iommu_mem(u32 vid);
/*
 * ********************
 * RUNTIME-PM FUNCTIONS
 * ********************
 */
int is_sensor_runtime_suspend_pre(struct device *dev);
int is_sensor_runtime_resume_pre(struct device *dev);

int is_ischain_runtime_suspend_post(struct device *dev);
int is_ischain_runtime_resume_pre(struct device *dev);
int is_ischain_runtime_resume_post(struct device *dev);

int is_ischain_runtime_suspend(struct device *dev);
int is_ischain_runtime_resume(struct device *dev);

int is_runtime_suspend_pre(struct device *dev);
int is_runtime_suspend_post(struct device *dev);
int is_runtime_resume_post(struct device *dev);

void is_hw_djag_get_input(struct is_device_ischain *ischain, u32 *djag_in);
void is_hw_djag_adjust_out_size(struct is_device_ischain *ischain,
					u32 in_width, u32 in_height,
					u32 *out_width, u32 *out_height);

/*
 * ************
 * FP/SIMD APIS
 * ************
 */
#if defined(ENABLE_FPSIMD_FOR_USER)
#if defined(VH_FPSIMD_API) /* Using android vendor hook to save and restore fpsimd regs */
extern void is_fpsimd_save_state(struct is_fpsimd_state *state);
extern void is_fpsimd_load_state(struct is_fpsimd_state *state);
void is_fpsimd_get_isr(void);
void is_fpsimd_put_isr(void);
void is_fpsimd_get_func(void);
void is_fpsimd_put_func(void);
void is_fpsimd_get_task(void);
void is_fpsimd_put_task(void);
void is_fpsimd_set_task_using(struct task_struct *t, struct is_kernel_fpsimd_state *kst);
int is_fpsimd_probe(void);
#else
#define is_fpsimd_get_isr()			kernel_neon_begin()
#define is_fpsimd_put_isr()			kernel_neon_end()
#define is_fpsimd_get_func()			fpsimd_get()
#define is_fpsimd_put_func()			fpsimd_put()
#define is_fpsimd_get_task()			do { } while(0)
#define is_fpsimd_put_task()			do { } while(0)
#define is_fpsimd_set_task_using(t, kst)	fpsimd_set_task_using(t)
#define is_fpsimd_probe()			({0;})
#endif
#else
#define is_fpsimd_get_isr()			do { } while(0)
#define is_fpsimd_put_isr()			do { } while(0)
#define is_fpsimd_get_func()			do { } while(0)
#define is_fpsimd_put_func()			do { } while(0)
#define is_fpsimd_get_task()			do { } while(0)
#define is_fpsimd_put_task()			do { } while(0)
#define is_fpsimd_set_task_using(t, kst)	do { } while(0)
#define is_fpsimd_probe()			({0;})
#endif

extern void is_clean_dcache_area(void *kaddr, u32 size);
#endif
