#ifndef _REGS_DPU_H_
#define _REGS_DPU_H_

enum dqe_regs_id {
	REGS_DQE0_ID = 0,
	REGS_DQE1_ID,
	REGS_DQE_ID_MAX
};

enum dqe_regs_type {
	REGS_DQE = 0,
	REGS_EDMA,
	REGS_DQE_TYPE_MAX
};

#define SHADOW_DQE_OFFSET		0x0200

/* DQE_TOP */
#define DQE_TOP_CORE_SECURITY		(0x0000)
#define DQE_CORE_SECURITY		(0x1 << 0)

#define DQE_TOP_IMG_SIZE		(0x0004)
#define DQE_IMG_VSIZE(_v)		((_v) << 16)
#define DQE_IMG_VSIZE_MASK		(0x3FFF << 16)
#define DQE_IMG_HSIZE(_v)		((_v) << 0)
#define DQE_IMG_HSIZE_MASK		(0x3FFF << 0)

#define DQE_TOP_FRM_SIZE		(0x0008)
#define DQE_FULL_IMG_VSIZE(_v)		((_v) << 16)
#define DQE_FULL_IMG_VSIZE_MASK	(0x3FFF << 16)
#define DQE_FULL_IMG_HSIZE(_v)		((_v) << 0)
#define DQE_FULL_IMG_HSIZE_MASK	(0x3FFF << 0)

#define DQE_TOP_FRM_PXL_NUM		(0x000C)
#define DQE_FULL_PXL_NUM(_v)		((_v) << 0)
#define DQE_FULL_PXL_NUM_MASK		(0xFFFFFFF << 0)

#define DQE_TOP_PARTIAL_START		(0x0010)
#define DQE_PARTIAL_START_Y(_v)	((_v) << 16)
#define DQE_PARTIAL_START_Y_MASK	(0x3FFF << 16)
#define DQE_PARTIAL_START_X(_v)	((_v) << 0)
#define DQE_PARTIAL_START_X_MASK	(0x3FFF << 0)

#define DQE_TOP_PARTIAL_CON		(0x0014)
#define DQE_PARTIAL_SAME(_v)		((_v) << 2)
#define DQE_PARTIAL_SAME_MASK		(0x1 << 2)
#define DQE_PARTIAL_UPDATE_EN(_v)	((_v) << 0)
#define DQE_PARTIAL_UPDATE_EN_MASK	(0x1 << 0)

/*----------------------[TOP_LPD_MODE]---------------------------------------*/
#define DQE_TOP_LPD_MODE_CONTROL	(0x0018)
#define DQE_LPD_MODE_EXIT(_v)		((_v) << 0)
#define DQE_LPD_MODE_EXIT_MASK		(0x1 << 0)

#define DQE_TOP_LPD_ATC_CON		(0x001C)
#define LPD_ATC_PU_ON			(0x1 << 5)
#define LPD_ATC_FRAME_CNT(_v)		((_v) << 3)
#define LPD_ATC_FRAME_CNT_MASK		(0x3 << 3)
#define LPD_ATC_DIMMING_PROG		(0x1 << 2)
#define LPD_ATC_FRAME_STATE(_v)		((_v) << 0)
#define LPD_ATC_FRAME_STATE_MASK	(0x3 << 0)

#define DQE_TOP_LPD_ATC_TDR_0		(0x0020)
#define LPD_ATC_TDR_MIN(_v)		((_v) << 10)
#define LPD_ATC_TDR_MIN_MASK		(0x3FF << 10)
#define LPD_ATC_TDR_MAX(_v)		((_v) << 0)
#define LPD_ATC_TDR_MAX_MASK		(0x3FF << 0)

#define DQE_TOP_LPD_ATC_TDR_1		(0x0024)
#define LPD_ATC_TDR_MIN_D1(_v)		((_v) << 10)
#define LPD_ATC_TDR_MIN_D1_MASK		(0x3FF << 10)
#define LPD_ATC_TDR_MAX_D1(_v)		((_v) << 0)
#define LPD_ATC_TDR_MAX_D1_MASK		(0x3FF << 0)

#define DQE_TOP_LPD_SCALE(_n)		(0x0028 + ((_n) * 0x4))
#define DQE_TOP_LPD_SCALE0		(0x0028)
#define DQE_TOP_LPD_SCALE1(_n)		(0x002C + ((_n) * 0x4))
#define DQE_TOP_LPD_SCALE2(_n)		(0x0040 + ((_n) * 0x4))
#define DQE_TOP_LPD_SCALE0_D1		(0x007C)
#define DQE_TOP_LPD_SCALE1_D1(_n)	(0x0080 + ((_n) * 0x4))
#define DQE_TOP_LPD_SCALE2_D1(_n)	(0x0094 + ((_n) * 0x4))

#define DQE_TOP_LPD_HSC			(0x00D0)
#define LPD_HSC_PU_ON			(0x1 << 22)
#define LPD_HSC_FRAME_STATE(_v)		((_v) << 20)
#define LPD_HSC_FRAME_STATE_MASK	(0x3 << 20)
#define LPD_HSC_AVG_UPDATE(_v)		((_v) << 10)
#define LPD_HSC_AVG_UPDATE_MASK		(0x3FF << 10)
#define LPD_HSC_AVG_NOUPDATE(_v)	((_v) << 0)
#define LPD_HSC_AVG_NOUPDATE_MASK	(0x3FF << 0)

#define DQE_TOP_LPD(_n)			(DQE_TOP_LPD_ATC_CON + ((_n) * 0x4))
#define DQE_LPD_REG_MAX			(46)

/*----------------------[VERSION]---------------------------------------------*/
#define DQE_TOP_VER			(0x00FC)
#define TOP_VER				(0x05000000)
#define TOP_VER_GET(_v)			(((_v) >> 0) & 0xFFFFFFFF)

/*----------------------[ATC]-------------------------------------------------*/
#define DQE_ATC_CONTROL			(0x0400)
#define DQE_ATC_SW_RESET(_v)		(((_v) & 0x1) << 16)
#define DQE_ATC_PIXMAP_EN(_v)			(((_v) & 0x1) << 2)
#define DQE_ATC_PARTIAL_UPDATE_METHOD(_v)	(((_v) & 0x1) << 1)
#define DQE_ATC_EN(_v)			(((_v) & 0x1) << 0)
#define DQE_ATC_EN_MASK			(0x1 << 0)

#define DQE_ATC_GAIN			(0x0404)
#define ATC_ST(_v)			(((_v) & 0xff) << 16)
#define ATC_NS(_v)			(((_v) & 0xff) << 8)
#define ATC_LT(_v)			(((_v) & 0xff) << 0)

#define DQE_ATC_WEIGHT			(0x0408)
#define ATC_LA_W(_v)			(((_v) & 0x7) << 28)
#define ATC_LA_W_ON(_v)			(((_v) & 0x1) << 24)
#define ATC_PL_W2(_v)			(((_v) & 0xf) << 16)
#define ATC_PL_W1(_v)			(((_v) & 0xf) << 0)

#define DQE_ATC_CTMODE			(0x040C)
#define ATC_CTMODE(_v)			(((_v) & 0x3) << 0)

#define DQE_ATC_PPEN			(0x0410)
#define ATC_PP_EN(_v)			(((_v) & 0x1) << 0)

#define DQE_ATC_TDRMINMAX		(0x0414)
#define ATC_UPGRADE_ON(_v)		(((_v) & 0x1) << 31)
#define ATC_TDR_MAX(_v)			(((_v) & 0x3ff) << 16)
#define ATC_TDR_MIN(_v)			(((_v) & 0x3ff) << 0)

#define DQE_ATC_AMBIENT_LIGHT		(0x0418)
#define ATC_AMBIENT_LIGHT(_v)		(((_v) & 0xff) << 0)

#define DQE_ATC_BACK_LIGHT		(0x041C)
#define ATC_BACK_LIGHT(_v)		(((_v) & 0xff) << 0)

#define DQE_ATC_DSTEP			(0x0420)
#define ATC_DSTEP(_v)			(((_v) & 0x3f) << 0)

#define DQE_ATC_SCALE_MODE		(0x0424)
#define ATC_SCALE_MODE(_v)		(((_v) & 0x3) << 0)

#define DQE_ATC_THRESHOLD		(0x0428)
#define ATC_THRESHOLD_3(_v)		(((_v) & 0x3) << 4)
#define ATC_THRESHOLD_2(_v)		(((_v) & 0x3) << 2)
#define ATC_THRESHOLD_1(_v)		(((_v) & 0x3) << 0)

#define DQE_ATC_GAIN_LIMIT		(0x042C)
#define ATC_LT_CALC_AB_SHIFT(_v)	(((_v) & 0x3) << 16)
#define ATC_GAIN_LIMIT(_v)		(((_v) & 0x3ff) << 0)

#define DQE_ATC_DIMMING_DONE_INTR	(0x0430)
#define ATC_DIMMING_IN_PROGRESS(_v)	(((_v) & 0x1) << 0)

#define DQE_ATC_PARTIAL_IBSI_P1		(0x0434)
#define ATC_IBSI_Y_P1(_v)		(((_v) & 0xffff) << 16)
#define ATC_IBSI_X_P1(_v)		(((_v) & 0xffff) << 0)

#define DQE_ATC_PARTIAL_IBSI_P2		(0x043C)
#define ATC_IBSI_Y_P2(_v)		(((_v) & 0xffff) << 16)
#define ATC_IBSI_X_P2(_v)		(((_v) & 0xffff) << 0)

#define DQE_ATC_CONTROL_LUT(_n)		(0x0400 + ((_n) * 0x4))

#define DQE_ATC_REG_MAX		(12)
#define DQE_ATC_LUT_MAX		(23)

/*----------------------[HSC]-------------------------------------------------*/
#define DQE_HSC_CONTROL			(0x0800)
#define HSC_PARTIAL_UPDATE_METHOD(_v)	(((_v) & 0x1) << 1)
#define HSC_PARTIAL_UPDATE_METHOD_MASK	(0x1 << 18)
#define HSC_EN(_v)			(((_v) & 0x1) << 0)
#define HSC_EN_MASK 			(0x1 << 0)

#define DQE_HSC_LOCAL_CONTROL		(0x0804)
#define HSC_LBC_GROUPMODE(_v)		(((_v) & 0x3) << 9)
#define HSC_LBC_ON(_v)			(((_v) & 0x1) << 8)
#define HSC_LHC_GROUPMODE(_v)		(((_v) & 0x3) << 5)
#define HSC_LHC_ON(_v)			(((_v) & 0x1) << 4)
#define HSC_LSC_GROUPMODE(_v)		(((_v) & 0x3) << 1)
#define HSC_LSC_ON(_v)			(((_v) & 0x1) << 0)

#define DQE_HSC_GLOBAL_CONTROL_1	(0x0808)
#define HSC_GHC_GAIN(_v)		(((_v) & 0x3ff) << 16)
#define HSC_GHC_ON(_v)			(((_v) & 0x1) << 0)

#define DQE_HSC_GLOBAL_CONTROL_0	(0x080C)
#define HSC_GSC_GAIN(_v)		(((_v) & 0x7ff) << 16)
#define HSC_GSC_ON(_v)			(((_v) & 0x1) << 0)

#define DQE_HSC_GLOBAL_CONTROL_2	(0x0810)
#define HSC_GBC_GAIN(_v)		(((_v) & 0x7ff) << 16)
#define HSC_GBC_ON(_v)			(((_v) & 0x1) << 0)

#define DQE_HSC_CONTROL_ALPHA_SAT	(0x0814)
#define HSC_ALPHA_SAT_SHIFT2(_v)	(((_v) & 0x7) << 28)
#define HSC_ALPHA_SAT_SHIFT1(_v)	(((_v) & 0x1ff) << 16)
#define HSC_ALPHA_SAT_SCALE(_v)		(((_v) & 0xf) << 4)
#define HSC_ALPHA_SAT_ON(_v)		(((_v) & 0x1) << 0)

#define DQE_HSC_CONTROL_ALPHA_BRI	(0x0818)
#define HSC_ALPHA_BRI_SHIFT2(_v)	(((_v) & 0x7) << 28)
#define HSC_ALPHA_BRI_SHIFT1(_v)	(((_v) & 0x1ff) << 16)
#define HSC_ALPHA_BRI_SCALE(_v)		(((_v) & 0xf) << 4)
#define HSC_ALPHA_BRI_ON(_v)		(((_v) & 0x1) << 0)

#define DQE_HSC_CONTROL_MC1_R0		(0x081C)
#define HSC_MC_SAT_GAIN_R0(_v)		(((_v) & 0x7ff) << 16)
#define HSC_MC_BC_SAT_R0(_v)		(((_v) & 0x3) << 8)
#define HSC_MC_BC_HUE_R0(_v)		(((_v) & 0x3) << 4)
#define HSC_MC_ON_R0(_v)		(((_v) & 0x1) << 0)

#define DQE_HSC_CONTROL_MC2_R0		(0x0820)
#define HSC_MC_BRI_GAIN_R0(_v)		(((_v) & 0x7ff) << 16)
#define HSC_MC_HUE_GAIN_R0(_v)		(((_v) & 0x3ff) << 0)

#define DQE_HSC_CONTROL_MC3_R0		(0x0824)
#define HSC_MC_S2_R0(_v)		(((_v) & 0x3ff) << 16)
#define HSC_MC_S1_R0(_v)		(((_v) & 0x3ff) << 0)

#define DQE_HSC_CONTROL_MC4_R0		(0x0828)
#define HSC_MC_H2_R0(_v)		(((_v) & 0xfff) << 16)
#define HSC_MC_H1_R0(_v)		(((_v) & 0xfff) << 0)

#define DQE_HSC_CONTROL_MC1_R1		(0x082C)
#define HSC_MC_SAT_GAIN_R1(_v)		(((_v) & 0x7ff) << 16)
#define HSC_MC_BC_SAT_R1(_v)		(((_v) & 0x3) << 8)
#define HSC_MC_BC_HUE_R1(_v)		(((_v) & 0x3) << 4)
#define HSC_MC_ON_R1(_v)		(((_v) & 0x1) << 0)

#define DQE_HSC_CONTROL_MC2_R1		(0x0830)
#define HSC_MC_BRI_GAIN_R1(_v)		(((_v) & 0x7ff) << 16)
#define HSC_MC_HUE_GAIN_R1(_v)		(((_v) & 0x3ff) << 0)

#define DQE_HSC_CONTROL_MC3_R1		(0x0834)
#define HSC_MC_S2_R1(_v)		(((_v) & 0x3ff) << 16)
#define HSC_MC_S1_R1(_v)		(((_v) & 0x3ff) << 0)

#define DQE_HSC_CONTROL_MC4_R1		(0x0838)
#define HSC_MC_H2_R1(_v)		(((_v) & 0xfff) << 16)
#define HSC_MC_H1_R1(_v)		(((_v) & 0xfff) << 0)

#define DQE_HSC_CONTROL_MC1_R2		(0x083C)
#define HSC_MC_SAT_GAIN_R2(_v)		(((_v) & 0x7ff) << 16)
#define HSC_MC_BC_SAT_R2(_v)		(((_v) & 0x3) << 8)
#define HSC_MC_BC_HUE_R2(_v)		(((_v) & 0x3) << 4)
#define HSC_MC_ON_R2(_v)		(((_v) & 0x1) << 0)

#define DQE_HSC_CONTROL_MC2_R2		(0x0840)
#define HSC_MC_BRI_GAIN_R2(_v)		(((_v) & 0x7ff) << 16)
#define HSC_MC_HUE_GAIN_R2(_v)		(((_v) & 0x3ff) << 0)

#define DQE_HSC_CONTROL_MC3_R2		(0x0844)
#define HSC_MC_S2_R2(_v)		(((_v) & 0x3ff) << 16)
#define HSC_MC_S1_R2(_v)		(((_v) & 0x3ff) << 0)

#define DQE_HSC_CONTROL_MC4_R2		(0x0848)
#define HSC_MC_H2_R2(_v)		(((_v) & 0xfff) << 16)
#define HSC_MC_H1_R2(_v)		(((_v) & 0xfff) << 0)

#define DQE_HSC_CONTROL_YCOMP		(0x084C)
#define HSC_BLEND_MANUAL_GAIN(_v)	(((_v) & 0xff) << 16)
#define HSC_YCOMP_GAIN(_v)		(((_v) & 0xf) << 8)
#define HSC_BLEND_ON(_v)		(((_v) & 0x1) << 2)
#define HSC_YCOMP_DITH_ON(_v)		(((_v) & 0x1) << 1)
#define HSC_YCOMP_ON(_v)		(((_v) & 0x1) << 0)

#define DQE_HSC_POLY_S(_n)		(0x09D8 + ((_n) * 0x4))
#define HSC_POLY_CURVE_S_H(_v)		(((_v) & 0x3ff) << 16)
#define HSC_POLY_CURVE_S_L(_v)		(((_v) & 0x3ff) << 0)

#define DQE_HSC_POLY_B(_n)		(0x09EC + ((_n) * 0x4))
#define HSC_POLY_CURVE_B_H(_v)		(((_v) & 0x3ff) << 16)
#define HSC_POLY_CURVE_B_L(_v)		(((_v) & 0x3ff) << 0)

#define DQE_HSC_LSC_GAIN_P1(_n)		(0x0C00 + ((_n) * 0x4))
#define HSC_LSC_GAIN_P1_H(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LSC_GAIN_P1_L(_v)		(((_v) & 0x7ff) << 0)

#define DQE_HSC_LSC_GAIN_P2(_n)		(0x0C60 + ((_n) * 0x4))
#define HSC_LSC_GAIN_P2_H(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LSC_GAIN_P2_L(_v)		(((_v) & 0x7ff) << 0)

#define DQE_HSC_LSC_GAIN_P3(_n)		(0x0CC0 + ((_n) * 0x4))
#define HSC_LSC_GAIN_P3_H(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LSC_GAIN_P3_L(_v)		(((_v) & 0x7ff) << 0)

#define DQE_HSC_LHC_GAIN_P1(_n)		(0x0D20 + ((_n) * 0x4))
#define HSC_LHC_GAIN_P1_H(_v)		(((_v) & 0x3ff) << 16)
#define HSC_LHC_GAIN_P1_L(_v)		(((_v) & 0x3ff) << 0)

#define DQE_HSC_LHC_GAIN_P2(_n)		(0x0D80 + ((_n) * 0x4))
#define HSC_LHC_GAIN_P2_H(_v)		(((_v) & 0x3ff) << 16)
#define HSC_LHC_GAIN_P2_L(_v)		(((_v) & 0x3ff) << 0)

#define DQE_HSC_LHC_GAIN_P3(_n)		(0x0DE0 + ((_n) * 0x4))
#define HSC_LHC_GAIN_P3_H(_v)		(((_v) & 0x3ff) << 16)
#define HSC_LHC_GAIN_P3_L(_v)		(((_v) & 0x3ff) << 0)

#define DQE_HSC_LBC_GAIN_P1(_n)		(0x0E40 + ((_n) * 0x4))
#define HSC_LBC_GAIN_P1_H(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LBC_GAIN_P1_L(_v)		(((_v) & 0x7ff) << 0)

#define DQE_HSC_LBC_GAIN_P2(_n)		(0x0EA0 + ((_n) * 0x4))
#define HSC_LBC_GAIN_P2_H(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LBC_GAIN_P2_L(_v)		(((_v) & 0x7ff) << 0)

#define DQE_HSC_LBC_GAIN_P3(_n)		(0x0F00 + ((_n) * 0x4))
#define HSC_LBC_GAIN_P3_H(_v)		(((_v) & 0x7ff) << 16)
#define HSC_LBC_GAIN_P3_L(_v)		(((_v) & 0x7ff) << 0)

#define DQE_HSC_CONTROL_LUT(_n)		(0x0800 + ((_n) * 0x4))
#define DQE_HSC_POLY_LUT(_n)		(0x09D8 + ((_n) * 0x4))
#define DQE_HSC_LSC_GAIN_LUT(_n)	(0x0C00 + ((_n) * 0x4))
#define DQE_HSC_LHC_GAIN_LUT(_n)	(0x0D20 + ((_n) * 0x4))
#define DQE_HSC_LBC_GAIN_LUT(_n)	(0x0E40 + ((_n) * 0x4))

#define DQE_HSC_REG_CTRL_MAX		20
#define DQE_HSC_REG_POLY_MAX		10
#define DQE_HSC_REG_LSC_GAIN_MAX	72
#define DQE_HSC_REG_GAIN_MAX		(DQE_HSC_REG_LSC_GAIN_MAX*3)
#define DQE_HSC_REG_MAX			(DQE_HSC_REG_CTRL_MAX + DQE_HSC_REG_POLY_MAX + DQE_HSC_REG_GAIN_MAX) //246
#define DQE_HSC_LUT_CTRL_MAX		57
#define DQE_HSC_LUT_POLY_MAX		18
#define DQE_HSC_LUT_LSC_GAIN_MAX	144
#define DQE_HSC_LUT_GAIN_MAX		(DQE_HSC_LUT_LSC_GAIN_MAX*3)
#define DQE_HSC_LUT_MAX			(DQE_HSC_LUT_CTRL_MAX + DQE_HSC_LUT_POLY_MAX + DQE_HSC_LUT_GAIN_MAX) //507

/*----------------------[GAMMA_MATRIX]---------------------------------------*/
#define DQE_GAMMA_MATRIX_CON		(0x1400)
#define GAMMA_MATRIX_EN(_v)		(((_v) & 0x1) << 0)
#define GAMMA_MATRIX_EN_MASK		(0x1 << 0)

#define DQE_GAMMA_MATRIX_COEFF(_n)	(0x1404 + ((_n) * 0x4))
#define DQE_GAMMA_MATRIX_COEFF0		(0x1404)
#define DQE_GAMMA_MATRIX_COEFF1		(0x1408)
#define DQE_GAMMA_MATRIX_COEFF2		(0x140C)
#define DQE_GAMMA_MATRIX_COEFF3		(0x1410)
#define DQE_GAMMA_MATRIX_COEFF4		(0x1414)

#define GAMMA_MATRIX_COEFF_H(_v)	(((_v) & 0x3FFF) << 16)
#define GAMMA_MATRIX_COEFF_H_MASK	(0x3FFF << 16)
#define GAMMA_MATRIX_COEFF_L(_v)	(((_v) & 0x3FFF) << 0)
#define GAMMA_MATRIX_COEFF_L_MASK	(0x3FFF << 0)

#define DQE_GAMMA_MATRIX_OFFSET0	(0x1418)
#define GAMMA_MATRIX_OFFSET_1(_v)	(((_v) & 0xFFF) << 16)
#define GAMMA_MATRIX_OFFSET_1_MASK	(0xFFF << 16)
#define GAMMA_MATRIX_OFFSET_0(_v)	(((_v) & 0xFFF) << 0)
#define GAMMA_MATRIX_OFFSET_0_MASK	(0xFFF << 0)

#define DQE_GAMMA_MATRIX_OFFSET1	(0x141C)
#define GAMMA_MATRIX_OFFSET_2(_v)	(((_v) & 0xFFF) << 0)
#define GAMMA_MATRIX_OFFSET_2_MASK	(0xFFF << 0)

#define DQE_GAMMA_MATRIX_LUT(_n)	(0x1400 + ((_n) * 0x4))

#define DQE_GAMMA_MATRIX_REG_MAX	(8)
#define DQE_GAMMA_MATRIX_LUT_MAX	(17)

/*----------------------[DEGAMMA]----------------------------------------*/
#define DQE_DEGAMMA_CON			(0x1800)
#define DEGAMMA_EN(_v)			(((_v) & 0x1) << 0)
#define DEGAMMA_EN_MASK			(0x1 << 0)

#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
#define DQE_DEGAMMALUT(_n)		(0x1804 + ((_n) * 0x4))
#else
#define DQE_DEGAMMA_POSX(_n)		(0x1804 + ((_n) * 0x4))
#define DQE_DEGAMMA_POSY(_n)		(0x1848 + ((_n) * 0x4))
#endif
#define DEGAMMA_LUT_H(_v)		(((_v) & 0x1FFF) << 16)
#define DEGAMMA_LUT_H_MASK		(0x1FFF << 16)
#define DEGAMMA_LUT_L(_v)		(((_v) & 0x1FFF) << 0)
#define DEGAMMA_LUT_L_MASK		(0x1FFF << 0)
#define DEGAMMA_LUT(_x, _v)		((_v) << (0 + (16 * ((_x) & 0x1))))
#define DEGAMMA_LUT_MASK(_x)		(0x1FFF << (0 + (16 * ((_x) & 0x1))))

#define DQE_DEGAMMA_LUT(_n)		(0x1800 + ((_n) * 0x4))

#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
#define DQE_DEGAMMA_LUT_CNT		(65)
#define DQE_DEGAMMA_REG_MAX		(1+DIV_ROUND_UP(DQE_DEGAMMA_LUT_CNT,2)) 	// 34 CON+LUT/2
#define DQE_DEGAMMA_LUT_MAX		(1+DQE_DEGAMMA_LUT_CNT) 		// 66 CON+LUT
#else
#define DQE_DEGAMMA_LUT_CNT		(33) // LUT_X + LUT_Y
#define DQE_DEGAMMA_REG_MAX		(1+DIV_ROUND_UP(DQE_DEGAMMA_LUT_CNT,2)*2) // 35 CON+LUT_XY/2
#define DQE_DEGAMMA_LUT_MAX		(1+DQE_DEGAMMA_LUT_CNT*2)		// 67 CON+LUT
#endif

/*----------------------[LINEAR MATRIX]----------------------------------------*/
#define DQE_LINEAR_MATRIX_CON		(0x1C00)

#define DQE_LINEAR_MATRIX_COEFF(_n)	(0x1C04 + ((_n) * 0x4))
#define LINEAR_MATRIX_COEFF_H(_v)	(((_v) & 0x1FFF) << 16)
#define LINEAR_MATRIX_COEFF_L(_v)	(((_v) & 0x1FFF) << 0)

#define DQE_LINEAR_MATRIX_OFFSET0	(0x1C18)
#define LINEAR_MATRIX_OFFSET_1(_v)	(((_v) & 0xFFF) << 16)
#define LINEAR_MATRIX_OFFSET_1_MASK	(0xFFF << 16)
#define LINEAR_MATRIX_OFFSET_0(_v)	(((_v) & 0xFFF) << 0)
#define LINEAR_MATRIX_OFFSET_0_MASK	(0xFFF << 0)

#define DQE_LINEAR_MATRIX_OFFSET1	(0x1C1C)
#define LINEAR_MATRIX_OFFSET_2(_v)	(((_v) & 0xFFF) << 0)
#define LINEAR_MATRIX_OFFSET_2_MASK	(0xFFF << 0)

#define LINEAR_MATRIX_LUT(_n)		(0x1C00 + ((_n) * 0x4))

#define DQE_LINEAR_MATRIX_LUT_CNT	(9)
#define DQE_LINEAR_MATRIX_REG_MAX	(1+(DQE_LINEAR_MATRIX_LUT_CNT/2+1)) 	// 6 CON+LUT/2
#define DQE_LINEAR_MATRIX_LUT_MAX	(1+DQE_LINEAR_MATRIX_LUT_CNT)		// 10 CON+LUT

/*----------------------[CGC]------------------------------------------------*/
#define DQE_CGC_CON			(0x2000)
#define CGC_COEF_DMA_REQ(_v)		((_v) << 4)
#define CGC_COEF_DMA_REQ_MASK		(0x1 << 4)
#define CGC_LUT_READ_SHADOW(_v)		((_v) << 2)
#define CGC_LUT_READ_SHADOW_MASK	(0x1 << 2)
#define CGC_EN(_v)			(((_v) & 0x1) << 0)
#define CGC_EN_MASK			(0x1 << 0)

#define DQE_CGC_MC_R(_n)		(0x2004 + ((_n) * 0x4))
#define DQE_CGC_MC_R0			(0x2004)
#define DQE_CGC_MC_R1			(0x2008)
#define DQE_CGC_MC_R2			(0x200C)

#define CGC_MC_GAIN_R(_v)		((_v) << 16)
#define CGC_MC_GAIN_R_MASK		(0xFFF << 16)
#define CGC_MC_INVERSE_R(_v)		((_v) << 1)
#define CGC_MC_INVERSE_R_MASK		(0x1 << 1)
#define CGC_MC_ON_R(_v)			((_v) << 0)
#define CGC_MC_ON_R_MASK		(0x1 << 0)

/*----------------------[REGAMMA]----------------------------------------*/
#define DQE_REGAMMA_CON			(0x2400)
#define REGAMMA_EN(_v)			(((_v) & 0x1) << 0)
#define REGAMMA_EN_MASK			(0x1 << 0)

#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
#define DQE_REGAMMALUT_R(_n)		(0x2404 + ((_n) * 0x4))
#define DQE_REGAMMALUT_G(_n)		(0x2488 + ((_n) * 0x4))
#define DQE_REGAMMALUT_B(_n)		(0x250C + ((_n) * 0x4))
#else
#define DQE_REGAMMA_R_POSX(_n)		(0x2404 + ((_n) * 0x4))
#define DQE_REGAMMA_R_POSY(_n)		(0x2448 + ((_n) * 0x4))
#define DQE_REGAMMA_G_POSX(_n)		(0x248C + ((_n) * 0x4))
#define DQE_REGAMMA_G_POSY(_n)		(0x24D0 + ((_n) * 0x4))
#define DQE_REGAMMA_B_POSX(_n)		(0x2514 + ((_n) * 0x4))
#define DQE_REGAMMA_B_POSY(_n)		(0x2558 + ((_n) * 0x4))
#endif

#define REGAMMA_LUT_H(_v)		(((_v) & 0x1FFF) << 16)
#define REGAMMA_LUT_H_MASK		(0x1FFF << 16)
#define REGAMMA_LUT_L(_v)		(((_v) & 0x1FFF) << 0)
#define REGAMMA_LUT_L_MASK		(0x1FFF << 0)
#define REGAMMA_LUT(_x, _v)		((_v) << (0 + (16 * ((_x) & 0x1))))
#define REGAMMA_LUT_MASK(_x)		(0x1FFF << (0 + (16 * ((_x) & 0x1))))

#define DQE_REGAMMA_LUT(_n)		(0x2400 + ((_n) * 0x4))

#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
#define DQE_REGAMMA_LUT_CNT		(65) // LUT R/G/B
#define DQE_REGAMMA_REG_MAX		(1+DIV_ROUND_UP(DQE_REGAMMA_LUT_CNT,2)*3) // 100 CON + LUT_RGB
#define DQE_REGAMMA_LUT_MAX		(1+DQE_REGAMMA_LUT_CNT*3)	// 196 CON + LUT_RGB
#else
#define DQE_REGAMMA_LUT_CNT		(33) // LUT_RGB_X/Y
#define DQE_REGAMMA_REG_MAX		(1+DIV_ROUND_UP(DQE_REGAMMA_LUT_CNT,2)*2*3) // 103 CON + LUT_RGB_X/Y
#define DQE_REGAMMA_LUT_MAX		(1+DQE_REGAMMA_LUT_CNT*2*3) 	// 199 CON + LUT_RGB_X/Y
#endif

/*----------------------[CGC_DITHER]-----------------------------------------*/
#define DQE_CGC_DITHER			(0x2800)
#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
#define CGC_DITHER_TABLE_SEL_B		(0x1 << 7)
#define CGC_DITHER_TABLE_SEL_G		(0x1 << 6)
#define CGC_DITHER_TABLE_SEL_R		(0x1 << 5)
#define CGC_DITHER_TABLE_SEL(_v, _n)	((_v) << (5 + (_n)))
#define CGC_DITHER_TABLE_SEL_MASK(_n)	(0x1 << (5 + (_n)))
#define CGC_DITHER_FRAME_OFFSET(_v)	((_v) << 3)
#define CGC_DITHER_FRAME_OFFSET_MASK	(0x3 << 3)
#define CGC_DITHER_FRAME_CON(_v)	((_v) << 2)
#define CGC_DITHER_FRAME_CON_MASK	(0x1 << 2)
#define CGC_DITHER_MODE(_v)		((_v) << 1)
#define CGC_DITHER_MODE_MASK		(0x1 << 1)
#define CGC_DITHER_EN(_v)		((_v) << 0)
#define CGC_DITHER_EN_MASK		(0x1 << 0)
#else
#define CGC_DITHER_FRAME_OFFSET(_v)	((_v) << 12)
#define CGC_DITHER_FRAME_OFFSET_MASK	(0xF << 12)
#define CGC_DITHER_BIT(_v)		(((_v) & 0x1) << 8)
#define CGC_DITHER_BIT_MASK		(0x1 << 8)
#define CGC_DITHER_TABLE_SEL_B		(0x1 << 7)
#define CGC_DITHER_TABLE_SEL_G		(0x1 << 6)
#define CGC_DITHER_TABLE_SEL_R		(0x1 << 5)
#define CGC_DITHER_TABLE_SEL(_v, _n)	((_v) << (5 + (_n)))
#define CGC_DITHER_TABLE_SEL_MASK(_n)	(0x1 << (5 + (_n)))
#define CGC_DITHER_FRAME_CON(_v)	((_v) << 2)
#define CGC_DITHER_FRAME_CON_MASK	(0x1 << 2)
#define CGC_DITHER_MODE(_v)		((_v) << 1)
#define CGC_DITHER_MODE_MASK		(0x1 << 1)
#define CGC_DITHER_EN(_v)		((_v) << 0)
#define CGC_DITHER_EN_MASK		(0x1 << 0)
#endif
#define DQE_CGC_DITHER_LUT_MAX	(8)

/*----------------------[DISP_DITHER]-----------------------------------------*/
#define DQE_DISP_DITHER			(0x2C00)
#define DISP_DITHER_TABLE_SEL_B		(0x1 << 7)
#define DISP_DITHER_TABLE_SEL_G		(0x1 << 6)
#define DISP_DITHER_TABLE_SEL_R		(0x1 << 5)
#define DISP_DITHER_TABLE_SEL(_v, _n)	((_v) << (5 + (_n)))
#define DISP_DITHER_TABLE_SEL_MASK(_n)	(0x1 << (5 + (_n)))
#define DISP_DITHER_FRAME_OFFSET(_v)	((_v) << 3)
#define DISP_DITHER_FRAME_OFFSET_MASK	(0x3 << 3)
#define DISP_DITHER_FRAME_CON(_v)	((_v) << 2)
#define DISP_DITHER_FRAME_CON_MASK	(0x1 << 2)
#define DISP_DITHER_MODE(_v)		((_v) << 1)
#define DISP_DITHER_MODE_MASK		(0x1 << 1)
#define DISP_DITHER_EN(_v)		((_v) << 0)
#define DISP_DITHER_EN_MASK		(0x1 << 0)

#define DQE_DISP_DITHER_LUT_MAX	(7)

/*----------------------[RCD]-----------------------------------------*/
#define DQE_RCD				(0x3000)
#define RCD_EN(_v)			((_v) << 0)
#define RCD_EN_MASK			(0x1 << 0)

/*----------------------[DE]-----------------------------------------*/
#define DQE_DE_CONTROL			(0x3200)
#define DE_BRATIO_SMOOTH(_v)		(((_v) & 0x1FF) << 16)
#define DE_FILTER_TYPE(_v)		(((_v) & 0x3) << 4)
#define DE_SMOOTH_EN(_v)		(((_v) & 0x1) << 3)
#define DE_ROI_IN(_v)			(((_v) & 0x1) << 2)
#define DE_ROI_EN(_v)			(((_v) & 0x1) << 1)
#define DE_EN(_v)			(((_v) & 0x1) << 0)
#define DE_EN_MASK			((0x1) << 0)

#define DQE_DE_ROI_START		(0x3204)
#define DE_ROI_START_Y(_v)		(((_v) & 0xFFF) << 16)
#define DE_ROI_START_X(_v)		(((_v) & 0xFFF) << 0)

#define DQE_DE_ROI_END			(0x3208)
#define DE_ROI_END_Y(_v)		(((_v) & 0xFFF) << 16)
#define DE_ROI_END_X(_v)		(((_v) & 0xFFF) << 0)

#define DQE_DE_FLATNESS			(0x320C)
#define DE_EDGE_BALANCE_TH(_v)		(((_v) & 0x3FF) << 16)
#define DE_FLAT_EDGE_TH(_v)		(((_v) & 0x3FF) << 0)

#define DQE_DE_CLIPPING			(0x3210)
#define DE_MINUS_CLIP(_v)		(((_v) & 0x3FF) << 16)
#define DE_PLUS_CLIP(_v)		(((_v) & 0x3FF) << 0)

#define DQE_DE_GAIN			(0x3214)
#define DF_MAX_LUMINANCE(_v)		(((_v) & 0x3FF) << 22)
#define DE_FLAT_DEGAIN_SHIFT(_v)	(((_v) & 0x1F) << 17)
#define DE_FLAT_DEGAIN_FLAG(_v)		(((_v) & 0x1) << 16)
#define DE_LUMA_DEGAIN_TH(_v)		(((_v) & 0x3FF) << 6)
#define DE_GAIN(_v)			(((_v) & 0x3F) << 0)

#define DQE_DE_LUT(_n)			(0x3200 + ((_n) * 0x4))

#define DQE_DE_REG_MAX			(6)
#define DQE_DE_LUT_MAX			(19)

#if !IS_ENABLED(CONFIG_SOC_S5E9925_EVT0) // Enable for EVT1 only as some definition is duplicated
/*----------------------[SCL]-----------------------------------------*/
#define DQE_SCL_SCALED_IMG_SIZE		(0x3400)
#define SCALED_IMG_HEIGHT(_v)		((_v) << 16)
#define SCALED_IMG_HEIGHT_MASK		(0x3FFF << 16)
#define SCALED_IMG_WIDTH(_v)		((_v) << 0)
#define SCALED_IMG_WIDTH_MASK		(0x3FFF << 0)

#define DQE_SCL_MAIN_H_RATIO		(0x3404)
#define H_RATIO(_v)			((_v) << 0)
#define H_RATIO_MASK			(0xFFFFFF << 0)

#define DQE_SCL_MAIN_V_RATIO		(0x3408)
#define V_RATIO(_v)			((_v) << 0)
#define V_RATIO_MASK			(0xFFFFFF << 0)

#define DQE_SCL_Y_VCOEF(_n)		(0x3410 + ((_n) * 0x4))
#define DQE_SCL_Y_HCOEF(_n)		(0x34A0 + ((_n) * 0x4))
#define SCL_COEF(_v)			((_v) << 0)
#define SCL_COEF_LUT(_v)		(((_v) & 0x7FF) << 0)
#define SCL_COEF_MASK			(0x7FF << 0)

#define DQE_SCL_YHPOSITION		(0x35C0)
#define DQE_SCL_YVPOSITION		(0x35C4)
#define POS_I(_v)			((_v) << 20)
#define POS_I_MASK			(0xFFF << 20)
#define POS_I_GET(_v)			(((_v) >> 20) & 0xFFF)
#define POS_F(_v)			((_v) << 0)
#define POS_F_MASK			(0xFFFFF << 0)
#define POS_F_GET(_v)			(((_v) >> 0) & 0xFFFFF)
#endif

#define DQE_SCL_COEF_SET		(9) // 0X ~ 8X
#define DQE_SCL_VCOEF_MAX		(4) // nA ~ nD
#define DQE_SCL_HCOEF_MAX		(8) // nA ~ nH
#define DQE_SCL_COEF_MAX		(DQE_SCL_VCOEF_MAX+DQE_SCL_HCOEF_MAX)
#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
#define DQE_SCL_COEF_CNT		(2) // Y & C coef
#else
#define DQE_SCL_COEF_CNT		(1) // Y coef only
#endif
#define DQE_SCL_REG_MAX			(DQE_SCL_COEF_SET*DQE_SCL_COEF_MAX*DQE_SCL_COEF_CNT)
#define DQE_SCL_LUT_MAX			(DQE_SCL_REG_MAX)

/*----------------------[CGC_LUT]-----------------------------------------*/
#define DQE_CGC_LUT_R(_n)		(0x4000 + ((_n) * 0x4))
#define DQE_CGC_LUT_G(_n)		(0x8000 + ((_n) * 0x4))
#define DQE_CGC_LUT_B(_n)		(0xC000 + ((_n) * 0x4))

#define CGC_LUT_H_SHIFT			(16)
#define CGC_LUT_H(_v)			((_v) << CGC_LUT_H_SHIFT)
#define CGC_LUT_H_MASK			(0x1FFF << CGC_LUT_H_SHIFT)
#define CGC_LUT_L_SHIFT			(0)
#define CGC_LUT_L(_v)			((_v) << CGC_LUT_L_SHIFT)
#define CGC_LUT_L_MASK			(0x1FFF << CGC_LUT_L_SHIFT)
#define CGC_LUT_LH(_x, _v)		((_v) << (0 + (16 * ((_x) & 0x1))))
#define CGC_LUT_LH_MASK(_x)		(0x1FFF << (0 + (16 * ((_x) & 0x1))))

#define DQE_CGC_REG_MAX			(2457)
#define DQE_CGC_LUT_MAX			(4914)
#define DQE_CGC_CON_REG_MAX		(3)
#define DQE_CGC_CON_LUT_MAX		(10)

#endif
