/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef TFA98XX_INTERNALS_H
#define TFA98XX_INTERNALS_H

#include "config.h"
#include "tfa_service.h"  /* TODO cleanup for enum tfa98xx_status_id */

/*
 * tfadsp_fw_api.c
 */
/**
 * Return a text version of the firmware status ID code
 * @param status the given status ID code
 * @return the firmware status ID string
 */
const char *tfadsp_fw_status_string(enum tfa98xx_status_id status);
int tfadsp_fw_start(struct tfa_device *tfa, int prof_idx, int vstep_idx);
int tfadsp_fw_get_api_version(struct tfa_device *tfa, uint8_t *buffer);
#define FW_MAXTAG 150
int tfadsp_fw_get_tag(struct tfa_device *tfa, uint8_t *buffer);
int tfadsp_fw_get_status_change(struct tfa_device *tfa, uint8_t *buffer);
int tfadsp_fw_set_re25(struct tfa_device *tfa, int prim, int sec);
int tfadsp_fw_get_re25(struct tfa_device *tfa, uint8_t *buffer);

/*
 * the order matches the ACK bits order in TFA98XX_CF_STATUS
 */
enum tfa_fw_event {/* not all available on each device */
	TFA_FW_I2C_CMD_ACK,
	TFA_FW_RESET_START,
	TFA_FW_SHORT_ON_MIPS,
	TFA_FW_SOFT_MUTE_READY,
	TFA_FW_VOLUME_READY,
	TFA_FW_ERROR_DAMAGE,
	TFA_FW_CALIBRATE_DONE,
	TFA_FW_MAX
};

/* the following type mappings are compiler specific */
#define subaddress_t unsigned char

/* module Ids */
#define MODULE_FRAMEWORK		0
#define MODULE_SPEAKERBOOST		1
#define MODULE_BIQUADFILTERBANK	2
#define MODULE_TAPTRIGGER		5
#define MODULE_SETRE			9
#define MODULE_CUSTOM			15

/* RPC commands */
/* SET */
#define FW_PAR_ID_SET_VBAT_FACTORS      0x02
#define FW_PAR_ID_SET_MEMORY            0x03 /* in use? */
#define FW_PAR_ID_SET_SENSES_DELAY      0x04
#define FW_PAR_ID_SET_SENSES_CAL        0x05
#define FW_PAR_ID_SET_INPUT_SELECTOR    0x06
#define FW_PAR_ID_SET_OUTPUT_SELECTOR   0x08
#define FW_PAR_ID_SET_PROGRAM_CONFIG    0x09
#define FW_PAR_ID_SET_GAINS             0x0A
#define FW_PAR_ID_SET_MEMTRACK          0x0B
#define FW_PAR_ID_SET_AMPLIFIER_EQ      0x0C
/* #define FW_PAR_ID_SET_FWKUSECASE        0x11 */ /* in use? */
#define FW_PAR_ID_SET_HW_CONFIG         0x13
#define FW_PAR_ID_SET_CHIP_TEMP_SELECTOR 0x14
#define FW_PAR_ID_SET_MULTI_MESSAGE     0x15
/* GET */
#define FW_PAR_ID_GET_MEMORY            0x83 /* in use? */
#define FW_PAR_ID_GLOBAL_GET_INFO       0x84 /* in use? */
#define FW_PAR_ID_GET_FEATURE_INFO      0x85 /* in use? */
#define FW_PAR_ID_GET_MEMTRACK          0x8B
#define FW_PAR_ID_GET_STATUS_CHANGE     0x8D
#define FW_PAR_ID_GET_LIBRARY_VERSION   0xFD
#define FW_PAR_ID_GET_API_VERSION       0xFE
/* #define FW_PAR_ID_GET_TAG               0xFF */ /* removed */

/* old version */
#define TFA1_FW_PAR_ID_SET_CURRENT_DELAY 0x03
#define TFA1_FW_PAR_ID_SET_CURFRAC_DELAY 0x06

/* Load a full model into SpeakerBoost. */
/* SET */
#define SB_PARAM_SET_ALGO_PARAMS        0x00
/* #define SB_PARAM_SET_LAGW               0x01 */ /* removed */
#define SB_PARAM_SET_ALGO_PARAMS_WITHOUT_RESET 0x02
#define SB_PARAM_SET_MUTE               0x03
#define SB_PARAM_SET_VOLUME             0x04
#define SB_PARAM_SET_RE25C              0x05
#define SB_PARAM_SET_LSMODEL            0x06
#define SB_PARAM_SET_MBDRC              0x07
#define SB_PARAM_SET_MBDRC_WITHOUT_RESET 0x08
/* #define SB_PARAM_SET_EXCURSION_FILTERS  0x0A */ /* removed */
#define SB_PARAM_SET_PARM_STEREO_BOOST  0x0A
#define SB_PARAM_SET_DATA_LOGGER        0x0D
#define SB_PARAM_SET_POWER_SAVER        0x0E
#define SB_PARAM_SET_DRC                0x0F /* old version */
#if !defined(TFA_USE_STC_VOLUME_TABLE)
#if !defined(TFA_USE_CUSTOM_SET_TSURF)
#define SB_PARAM_SET_TSURF              0x7F /* to be defined */
#endif
#endif

/* GET */
#define SB_PARAM_GET_ALGO_PARAMS        0x80
/* #define SB_PARAM_GET_LAGW               0x81 */ /* removed */
#define SB_PARAM_GET_RE25C              0x85
#define SB_PARAM_GET_LSMODEL            0x86
#define SB_PARAM_GET_MBDRC              0x87
#define SB_PARAM_GET_MBDRC_DYNAMICS     0x89
/* #define SB_PARAM_GET_EXCURSION_FILTERS  0x8A */ /* removed */
#define SB_PARAM_GET_PARM_STEREO_BOOST  0x8A
/* #define SB_PARAM_GET_XMODEL_COEFFS      0x8C */ /* removed */
#define SB_PARAM_GET_DATA_LOGGER        0x8D
#define SB_PARAM_GET_POWER_SAVER        0x8E
#define SB_PARAM_GET_Z_FILTER           0x8F
#define SB_PARAM_GET_X_FILTER           0x90
#define SB_PARAM_GET_DISTO_DYNAMICS     0x91
#define SB_PARAM_GET_SIBILANCE_LB_FILT  0x92
#define SB_PARAM_GET_SIBILANCE_B1_FILT  0x93
#define SB_PARAM_GET_SIBILANCE_B2_FILT  0x94
#define SB_PARAM_GET_SIBILANCE_B3_FILT  0x95
#define SB_PARAM_GET_SIBILANCE_GAIN_FILT 0x96
#define SB_PARAM_GET_TSPKR              0xA8
/* #define SB_PARAM_GET_TAG                0xFF */ /* removed */

/* aligned with wrapper */
/* SET */
#if !defined(TFA_USE_STC_VOLUME_TABLE)
#if defined(TFA_USE_CUSTOM_SET_TSURF)
#define CUSTOM_PARAM_SET_TSURF          0x01
#endif
#endif
#define CUSTOM_PARAM_SET_BYPASS         0x02

/* GET */
#define CUSTOM_PARAM_GET_CONFIGURED     0x81

/* old version */
#define SB_PARAM_SET_EQ                 0x0A /* 2 EQ filters */
#define SB_PARAM_SET_PRESET             0x0D /* Load a preset */
#define SB_PARAM_SET_CONFIG             0x0E /* Load a config */
#define SB_PARAM_SET_AGCINS             0x10
#define SB_PARAM_SET_CURRENT_DELAY      0x03
#define SB_PARAM_GET_STATE              0xC0 /* old version */
/* #define SB_PARAM_GET_XMODEL             0xC1 */

/* SET: TAPTRIGGER */
#define TAP_PARAM_SET_ALGO_PARAMS       0x01
#define TAP_PARAM_SET_DECIMATION_PARAMS 0x02
/* GET: TAPTRIGGER */
#define TAP_PARAM_GET_ALGO_PARAMS		0x81
#define TAP_PARAM_GET_TAP_RESULTS		0x84

/* sets the speaker calibration impedance (@25 degrees celsius) */
/* #define SB_PARAM_SET_RE0                0x89 */ /* old version */

#define BFB_PAR_ID_SET_COEFS            0x00
#define BFB_PAR_ID_GET_COEFS            0x80
#define BFB_PAR_ID_GET_CONFIG           0x81

/* for compatibility */
#define FW_PARAM_GET_STATE          FW_PAR_ID_GLOBAL_GET_INFO
#define FW_PARAM_GET_FEATURE_BITS   FW_PAR_ID_GET_FEATURE_INFO

/* RPC Status results */
#define STATUS_OK                  0
#define STATUS_INVALID_MODULE_ID   2
#define STATUS_INVALID_PARAM_ID    3
#define STATUS_INVALID_INFO_ID     4

/* the maximum message length in the communication with the DSP */
#define TFA2_MAX_PARAM_SIZE (507*3) /* TFA2 */
#define TFA1_MAX_PARAM_SIZE (145*3) /* TFA1 */

#define ROUND_DOWN(a, n) (((a)/(n))*(n))

/* feature bits */
#define FEATURE1_TCOEF 0x100 /* bit8 set means tCoefA expected */
#define FEATURE1_DRC   0x200 /* bit9 NOT set means DRC expected */

/* DSP firmware xmem defines */
#define TFA1_FW_XMEM_CALIBRATION_DONE   231
#define TFA2_FW_XMEM_CALIBRATION_DONE   516
#define TFA1_FW_XMEM_COUNT_BOOT         0xa1
#define TFA2_FW_XMEM_COUNT_BOOT         512
#define TFA2_FW_XMEM_CMD_COUNT          520

/* note that the following defs rely on the handle variable */
#define TFA_FW_XMEM_CALIBRATION_DONE	\
	TFA_FAM_FW(tfa, XMEM_CALIBRATION_DONE)
#define TFA_FW_XMEM_COUNT_BOOT	TFA_FAM_FW(tfa, XMEM_COUNT_BOOT)
#define TFA_FW_XMEM_CMD_COUNT TFA_FAM_FW(tfa, XMEM_CMD_COUNT)

#define TFA2_FW_ReZ_SCALE	65536
#define TFA1_FW_ReZ_SCALE	16384
#define TFA_FW_ReZ_SCALE	TFA_FAM_FW(tfa, ReZ_SCALE)

#define TFA2_FW_ReZ_SHIFT	16
#define TFA1_FW_ReZ_SHIFT	14
#define TFA_FW_ReZ_SHIFT	TFA_FAM_FW(tfa, ReZ_SHIFT)

#define TFA_ReZ_FP_INT(value, shift) ((value) >> (shift))
#define TFA_ReZ_FP_FRAC(value, shift) \
((((value) & ((1 << (shift)) - 1)) * 1000) >> (shift))
#define TFA_ReZ_CALC(value, shift) (((value) << (shift)) / 1000)

#define TFA2_FW_X_DATA_SCALE	262144 /* 2097152 */
#define TFA2_FW_X_DATA_UM_SCALE	262 /* 2097 */
#define TFA2_FW_T_DATA_SCALE	16384
#define TFA2_FW_T_DATA_MAX	512 /* 10-bit signed integer */
#endif /* TFA98XX_INTERNALS_H */
