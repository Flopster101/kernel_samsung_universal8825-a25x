/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 */
/*
 * internal functions for TFA layer (not shared with SRV and HAL layer!)
 */

#ifndef __TFA_INTERNAL_H__
#define __TFA_INTERNAL_H__

#include "tfa_dsp_fw.h"
#if defined(TFA_SET_EXT_INTERNALLY)
#include "tfa_ext.h"
#else
#include <sound/tfa_ext.h>
#endif

#ifdef QPLATFORM
#if defined(TFA_SET_EXT_INTERNALLY) || defined(TFA_SET_IPC_PORT_ID)
#include <dsp/q6afe-v2.h>
#endif
#if defined(TFADSP_DSP_MSG_PACKET_STRATEGY)
#include <ipc/apr_tal.h>
#endif
#endif

#if __GNUC__ >= 4
  #define TFA_INTERNAL __attribute__((visibility("hidden")))
#else
  #define TFA_INTERNAL
#endif

#define TFA98XX_GENERIC_RESP_ADDRESS 0x1C

/* 2 channels per SB */
#define MAX_HANDLES 4
#define MAX_CHANNELS 2

/* max. length of a alsa mixer control name */
#define MAX_CONTROL_NAME 48

#if defined(TFADSP_DSP_MSG_PACKET_STRATEGY)
#ifdef QPLATFORM
/*
 * in case of CONFIG_MSM_QDSP6_APRV2_GLINK/APRV3_GLINK,
 * with smaller APR_MAX_BUF (512)
 * in other cases, with safe size (4096)
 */
#define APR_RESIDUAL_SIZE 60
#define APR_MAX_BUF2 ((APR_MAX_BUF > 512) ? 4096 : APR_MAX_BUF)
#define MAX_PKT_MSG_SIZE (APR_MAX_BUF2-APR_RESIDUAL_SIZE) /* 452 or 4036 */
/* data size in packet by excluding header (packet_id:2, packet_size:2) */
/* #define STANDARD_PACKET_SIZE	(MAX_APR_MSG_SIZE - 4) */ /* 448 or 4032 */
#else
#define MAX_PKT_MSG_SIZE 65536 /* 64*1024 */
#endif
#endif

#define MIN_CALIBRATION_DATA 0
#define MAX_CALIBRATION_DATA 32000
#if defined(TFA_USE_DUMMY_CAL)
#define DUMMY_CALIBRATION_DATA 6000
#endif

#define DEFAULT_REF_TEMP 25

enum instream_state {
	BIT_PSTREAM = 1, /* b0 */
	BIT_CSTREAM = 2, /* b1 */
	BIT_SAMSTREAM = 4 /* b2 */
};

/* tfa98xx: tfa_device_ops */
void tfanone_ops(struct tfa_device_ops *ops);
#if (defined(USE_TFA9872) || defined(TFA98XX_FULL))
void tfa9872_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9874) || defined(TFA98XX_FULL))
void tfa9874_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9878) || defined(TFA98XX_FULL))
void tfa9878_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9912) || defined(TFA98XX_FULL))
void tfa9912_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9888) || defined(TFA98XX_FULL))
void tfa9888_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9891) || defined(TFA98XX_FULL))
void tfa9891_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9897) || defined(TFA98XX_FULL))
void tfa9897_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9896) || defined(TFA98XX_FULL))
void tfa9896_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9890) || defined(TFA98XX_FULL))
void tfa9890_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9895) || defined(TFA98XX_FULL))
void tfa9895_ops(struct tfa_device_ops *ops);
#endif
#if (defined(USE_TFA9894) || defined(TFA98XX_FULL))
void tfa9894_ops(struct tfa_device_ops *ops);
#endif

TFA_INTERNAL enum tfa98xx_error tfa98xx_check_rpc_status
	(struct tfa_device *tfa, int *p_rpc_status);
TFA_INTERNAL enum tfa98xx_error tfa98xx_wait_result
	(struct tfa_device *tfa, int waitRetryCount);

#if defined(TFADSP_DSP_BUFFER_POOL)
enum tfa98xx_error tfa_buffer_pool(struct tfa_device *tfa,
	int index, int size, int control);
int tfa98xx_buffer_pool_access(int r_index,
	size_t g_size, uint8_t **buf, int control);
#endif

int tfa98xx_get_fssel(unsigned int rate);

struct tfa_device *tfa98xx_get_tfa_device_from_index(int index);
struct tfa_device *tfa98xx_get_tfa_device_from_channel(int channel);

int tfa98xx_count_active_stream(int stream_flag);

int tfa_ext_event_handler(enum tfadsp_event_en tfadsp_event);

void tfa_set_ipc_loaded(int status);
int tfa_get_ipc_loaded(void);

void tfa_set_active_handle(struct tfa_device *tfa, int profile);
void tfa_reset_active_handle(struct tfa_device *tfa);

void tfa_handle_damaged_speakers(struct tfa_device *tfa);

#if defined(TFA_USE_TFACAL_NODE)
enum tfa98xx_error tfa_run_cal(int index, uint16_t *value);
enum tfa98xx_error tfa_get_cal_data(int index, uint16_t *value);
enum tfa98xx_error tfa_get_cal_temp(int index, uint16_t *value);
#endif

#define TFA_LOG_MAX_COUNT	4
#if defined(TFA_BLACKBOX_LOGGING)
int tfa_set_blackbox(int enable);
enum tfa98xx_error tfa_configure_log(int enable);
enum tfa98xx_error tfa_update_log(void);
#endif /* TFA_BLACKBOX_LOGGING */

#if defined(TFA_USE_TFAVVAL_NODE)
enum tfa_vval_result {
	VVAL_UNTESTED = -1,
	VVAL_PASS = 0,
	VVAL_FAIL = 1,
	VVAL_TEST_FAIL = 2
};
enum tfa98xx_error tfa_run_vval(int index, uint16_t *value);
enum tfa98xx_error tfa_get_vval_data(int index, uint16_t *value);
#endif

#if defined(TFA_USE_TFASTC_NODE)
#if defined(TFA_USE_STC_VOLUME_TABLE)
#define STC_TABLE_MAX	(10)
int tfa_get_sknt_data_from_table(int idx, int value);

#if defined(TFA_ENABLE_STC_TUNING)
int tfa_get_stc_minmax(int idx, int minmax);
void tfa_set_stc_minmax(int idx, int minmax, int value);
void tfa_get_stc_gtable(int idx, int *value);
void tfa_set_stc_gtable(int idx, int *value);
#endif /* TFA_ENABLE_STC_TUNING */
#endif /* TFA_USE_STC_VOLUME_TABLE */
#endif /* TFA_USE_TFASTC_NODE */

int tfa_wait_until_calibration_done(struct tfa_device *tfa);

#endif /* __TFA_INTERNAL_H__ */

