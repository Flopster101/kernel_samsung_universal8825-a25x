#ifndef _UFS_CAL_TABLE_
#define _UFS_CAL_TABLE_

#define UFS_CAL_VER 0
#include "ufs-cal-if.h"

struct ufs_cal_phy_cfg {
	u32 mib;
	u32 addr;
	u32 val;
	u32 flg;
	u32 lyr;
	u8 board;
};

enum {
	PHY_CFG_NONE = 0,
	PHY_PCS_COMN,
	PHY_PCS_RXTX,
	PHY_PMA_COMN,
	PHY_PMA_TRSV,
	PHY_PLL_WAIT,
	PHY_CDR_WAIT,
	PHY_CDR_AFC_WAIT,
	UNIPRO_STD_MIB,
	UNIPRO_DBG_MIB,
	UNIPRO_DBG_APB,

	PHY_PCS_RX,
	PHY_PCS_TX,
	PHY_PCS_RX_PRD,
	PHY_PCS_TX_PRD,
	UNIPRO_DBG_PRD,
	PHY_PMA_TRSV_LANE1_SQ_OFF,
	PHY_PMA_TRSV_SQ,
	COMMON_WAIT,

	PHY_PCS_RX_LR_PRD,
	PHY_PCS_TX_LR_PRD,
	PHY_PCS_RX_PRD_ROUND_OFF,
	PHY_PCS_TX_PRD_ROUND_OFF,
	UNIPRO_ADAPT_LENGTH,
	PHY_EMB_CDR_WAIT,
	PHY_EMB_CAL_WAIT,

	HCI_AH8_THIBERN,
	HCI_AH8_REFCLKGATINGTING,
	HCI_AH8_ACTIVE_LANE,
	HCI_AH8_CMN,
	HCI_AH8_TRSV,
	HCI_STD,
};

enum {
	PMD_PWM_G1 = 0,
	PMD_PWM_G2,
	PMD_PWM_G3,
	PMD_PWM_G4,
	PMD_PWM_G5,
	PMD_PWM,

	PMD_HS_G1,
	PMD_HS_G2,
	PMD_HS_G3,
	PMD_HS_G4,
	PMD_HS,

	PMD_ALL,
};

static struct ufs_cal_phy_cfg init_cfg_evt0[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x44, 0x60, PMD_ALL, UNIPRO_DBG_PRD, BRD_ALL},

	{0x200, 0x2800, 0x40, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x012, 0x2048, 0x00, PMD_ALL, PHY_PCS_RX_PRD_ROUND_OFF, BRD_ALL},
	{0x0AA, 0x22A8, 0x00, PMD_ALL, PHY_PCS_TX_PRD_ROUND_OFF, BRD_ALL},
	{0x065, 0x2194, 0x01, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x084, 0x2210, 0x01, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x004, 0x2010, 0x01, PMD_ALL, PHY_PCS_TX, BRD_ALL},

	{0x0A9, 0x22A4, 0x02, PMD_ALL, PHY_PCS_TX, BRD_ALL},
	{0x0AB, 0x22AC, 0x00, PMD_ALL, PHY_PCS_TX_LR_PRD, BRD_ALL},
	{0x011, 0x2044, 0x00, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x01B, 0x206C, 0x00, PMD_ALL, PHY_PCS_RX_LR_PRD, BRD_ALL},
	{0x025, 0x2094, 0xF6, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x200, 0x2800, 0x00, PMD_ALL, PHY_PCS_COMN, BRD_ALL},

	{0x155E, 0x3178, 0x0, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x3000, 0x5000, 0x0, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x3001, 0x5004, 0x1, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x4021, 0x6084, 0x1, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x4020, 0x6080, 0x1, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},

	{0x0000, 0x08C, 0x80, PMD_ALL, PHY_PMA_COMN, BRD_ALL},

	{0x0000, 0x074, 0x10, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x110, 0xB5, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x134, 0x43, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x16C, 0x20, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x178, 0xC0, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x1B0, 0x18, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x0E0, 0x36, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x10C, 0x80, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1B4, 0x12, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x164, 0x58, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x120, 0xC0, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x08C, 0xC0, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x08C, 0x00, PMD_ALL, PHY_PMA_COMN, BRD_ALL},

	{0x0000, 0x000, 0xC8, PMD_ALL, COMMON_WAIT, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg init_cfg_evt1[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_init_cfg_evt0[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_init_cfg_evt1[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_pwm[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x2041, 0x4104, 8064, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x2042, 0x4108, 28224, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x2043, 0x410C, 20160, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B0, 0x32C0, 12000, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B1, 0x32C4, 32000, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B2, 0x32C8, 16000, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},

	{0x0000, 0x7888, 8064, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x788C, 28224, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x7890, 20160, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78B8, 12000, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78BC, 32000, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C0, 16000, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},

	{0x0000, 0x0C8, 0x40, PMD_PWM, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x0F0, 0x77, PMD_PWM, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x134, 0x43, PMD_PWM, PHY_PMA_TRSV, BRD_ALL},
	//disable AH8 @PWM
	//{0x0000, 0x18, 0x0, PMD_PWM, HCI_AH8_CMN, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_pwm[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_a[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x2041, 0x4104, 8064, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x2042, 0x4108, 28224, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x2043, 0x410C, 20160, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B0, 0x32C0, 12000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B1, 0x32C4, 32000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B2, 0x32C8, 16000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},

	{0x0000, 0x7888, 8064, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x788C, 28224, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x7890, 20160, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78B8, 12000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78BC, 32000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C0, 16000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},

	{0x0000, 0x0C8, 0xBC, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x0F0, 0x7F, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x134, 0x23, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x178, 0xC0, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_a[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x1fc, 0x40, PMD_HS, PHY_CDR_AFC_WAIT, BRD_ALL},
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_b[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x2041, 0x4104, 8064, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x2042, 0x4108, 28224, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x2043, 0x410C, 20160, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B0, 0x32C0, 12000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B1, 0x32C4, 32000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B2, 0x32C8, 16000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},

	{0x0000, 0x7888, 8064, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x788C, 28224, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x7890, 20160, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78B8, 12000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78BC, 32000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C0, 16000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},

	{0x0000, 0x0C8, 0xBC, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x0F0, 0x7F, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x134, 0x23, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x178, 0xC0, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_b[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x1fc, 0x40, PMD_HS, PHY_CDR_AFC_WAIT, BRD_ALL},
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_ah8_cfg[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x504, 0x00, PMD_HS, HCI_AH8_THIBERN, BRD_ALL},
	{0x0000, 0x508, 0x00, PMD_HS, HCI_AH8_REFCLKGATINGTING, BRD_ALL},
	{0x0000, 0x51C, 0x00, PMD_HS, HCI_AH8_ACTIVE_LANE, BRD_ALL},
	{0x0000, 0x524, 0x35B60, PMD_HS, HCI_AH8_CMN, BRD_ALL},

	// Retry Number & Access Number
	{0x0000, 0x520, 0x001300A7, PMD_HS, HCI_AH8_CMN, BRD_ALL},

	// need to lane# check
	{0x0000, 0x610, 0x000000C4, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x650, 0x00000099, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x614, 0x000000E8, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x654, 0x0000007F, PMD_HS, HCI_AH8_CMN, BRD_ALL},

	{0x0000, 0x618, 0x000000F0, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x658, 0x0000007F, PMD_HS, HCI_AH8_CMN, BRD_ALL},

	{0x0000, 0x061C, 0x00000004, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x065C, 0x00000002, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x690, 0x00000004, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6D0, 0x0000003F, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x694, 0x401E0000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6D4, 0x00000000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x698, 0x000000C4, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x6D8, 0x000000D9, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x69C, 0x000000E8, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x6DC, 0x00000077, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x06A0, 0x000000F0, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x06E0, 0x0000007F, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x06A4, 0x000000F0, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x06E4, 0x000000FF, PMD_HS, HCI_AH8_CMN, BRD_ALL},

	{0x0000, 0x06A8, 0x40280000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x06E8, 0x00000000, PMD_HS, HCI_AH8_CMN, BRD_ALL},

	{0x0000, 0x06AC, 0x202801FC, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x06EC, 0x00010001, PMD_HS, HCI_AH8_CMN, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg lane1_sq_off[] = {
	/* mib(just to monitor), sfr offset, value, .. */

	{0x0000, 0x0C4, 0x19, PMD_ALL, PHY_PMA_TRSV_LANE1_SQ_OFF, BRD_ALL},
	{0x0000, 0x0E8, 0xFF, PMD_ALL, PHY_PMA_TRSV_LANE1_SQ_OFF, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_h8_enter[] = {
	/* mib(just to monitor), sfr offset, value, .. */

	{0x0000, 0x0C4, 0x99, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},
	{0x0000, 0x0E8, 0x7F, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},
	{0x0000, 0x0F0, 0x7F, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x004, 0x02, PMD_ALL, PHY_PMA_COMN, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg pre_h8_exit[] = {
	/* mib(just to monitor), sfr offset, value, .. */

	{0x0000, 0x004, 0x3F, PMD_HS, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x004, 0x00, PMD_PWM, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x000, 0x0A, PMD_ALL, COMMON_WAIT, BRD_ALL},
	{0x0000, 0x0C4, 0xD9, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},
	{0x0000, 0x0E8, 0x77, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},
	{0x0000, 0x000, 0x0A, PMD_HS, COMMON_WAIT, BRD_ALL},
	{0x0000, 0x0F0, 0xFF, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1fc, 0x01, PMD_HS, PHY_CDR_AFC_WAIT, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_init[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_set_1[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_set_2[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg eom_prepare[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg init_cfg_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_init_cfg_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_pwm_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_pwm_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_a_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_a_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_b_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_b_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg lane1_sq_off_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_h8_enter_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg pre_h8_exit_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_init_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_set_1_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_set_2_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};
#endif	/* _UFS_CAL_TABLE_ */
