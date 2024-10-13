/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/* file
 *
 * The tfa_device interface controls a single I2C device instance by
 * referencing to the device specific context provided by means of the
 * tfa_device structure pointer.
 * Multiple instances of tfa_device structures will be created and maintained
 * by the caller.
 *
 * The API is functionally grouped as:
 *  - tfa_dev basic codec interface to probe,
 *    start/stop and control the device state
 *  - access to internal MTP storage
 *  - abstraction for interrupt bits and handling
 *  - container reading support
 */
#ifndef __TFA_DEVICE_H__
#define __TFA_DEVICE_H__

#include "config.h"

struct tfa_device;

/*
 * hw/sw feature bit settings in MTP
 */
enum feature_support {
	SUPPORT_NOT_SET, /**< default means not set yet */
	SUPPORT_NO,      /**< no support */
	SUPPORT_YES      /**< supported */
};
/*
 * supported Digital Audio Interfaces bitmap
 */
enum tfa98xx_dai_bitmap {
	TFA98XX_DAI_I2S = 0x01, /**< I2S only */
	TFA98XX_DAI_TDM = 0x02, /**< TDM, I2S */
	TFA98XX_DAI_PDM = 0x04, /**< PDM  */
};

/*
 * device ops function structure
 */
struct tfa_device_ops {
	int (*dsp_msg)(void *tfa, int length, const char *buf);
	int (*dsp_msg_read)(void *tfa, int length, unsigned char *bytes);
	enum tfa98xx_error (*reg_read)(struct tfa_device *tfa,
		unsigned char subaddress, unsigned short *value);
	enum tfa98xx_error (*reg_write)(struct tfa_device *tfa,
		unsigned char subaddress, unsigned short value);
	enum tfa98xx_error (*mem_read)(struct tfa_device *tfa,
		unsigned int start_offset, int num_words, int *p_values);
	enum tfa98xx_error (*mem_write)(struct tfa_device *tfa,
		unsigned short address, int value, int memtype);

	enum tfa98xx_error (*tfa_init)(struct tfa_device *tfa);
	enum tfa98xx_error (*dsp_reset)(struct tfa_device *tfa, int state);
	enum tfa98xx_error (*dsp_system_stable)(struct tfa_device *tfa,
		int *ready);
	enum tfa98xx_error (*dsp_write_tables)(struct tfa_device *tfa,
		int sample_rate);
	enum tfa98xx_error (*auto_copy_mtp_to_iic)(struct tfa_device *tfa);
	enum tfa98xx_error (*factory_trimmer)(struct tfa_device *tfa);
	int (*set_swprof)(struct tfa_device *tfa, unsigned short new_value);
	int (*get_swprof)(struct tfa_device *tfa);
	int (*set_swvstep)(struct tfa_device *tfa, unsigned short new_value);
	int (*get_swvstep)(struct tfa_device *tfa);
	int (*get_mtpb)(struct tfa_device *tfa);
	enum tfa98xx_error (*set_mute)(struct tfa_device *tfa, int mute);
	enum tfa98xx_error (*faim_protect)(struct tfa_device *tfa, int state);
	enum tfa98xx_error (*set_osc_powerdown)(struct tfa_device *tfa,
		int state);
	enum tfa98xx_error (*update_lpm)(struct tfa_device *tfa, int state);
};

/*
 * Device states and modifier flags to allow a device/type independent fine
 * grained control of the internal state.\n
 * Values below 0x10 are referred to as base states which can be or-ed with
 * state modifiers, from 0x10 and higher.
 *
 */
enum tfa_state {
	TFA_STATE_UNKNOWN,
	TFA_STATE_POWERDOWN,
	TFA_STATE_INIT_HW,
	TFA_STATE_INIT_CF,
	TFA_STATE_INIT_FW,
	TFA_STATE_OPERATING,
	TFA_STATE_FAULT,
	TFA_STATE_RESET,
	/* --sticky state modifiers-- */
	TFA_STATE_MUTE = 0x10,
	TFA_STATE_UNMUTE = 0x20,
	TFA_STATE_CLOCK_ALWAYS = 0x40,
	TFA_STATE_CLOCK_AUDIO = 0x80,
	TFA_STATE_LOW_POWER = 0x100,
};

#if defined(TFADSP_DSP_BUFFER_POOL)
enum pool_control {
	POOL_NOT_SUPPORT,
	POOL_ALLOC,
	POOL_FREE,
	POOL_GET,
	POOL_RETURN,
	POOL_MAX_CONTROL
};

#define POOL_MAX_INDEX 6

struct tfa98xx_buffer_pool {
	int size;
	unsigned char in_use;
	void *pool;
};
#endif /* TFADSP_DSP_BUFFER_POOL */

#if defined(TFA_CHANGE_PCM_FORMAT)
struct tfa98xx_tdm_format {
	unsigned short nbck; /* # of bit clock periods on BCK */
	unsigned short audfs; /* sampling rate from hw_params */
	unsigned short slln; /* # of bits in a slot - 1 */
	unsigned short ssize; /* # of bits in a sample - 1 */
	unsigned short slots; /* # of slots in a frame - 1 */
	unsigned short srcmap; /* source mapping */
};
#endif

#if defined(TFA_BLACKBOX_LOGGING)
/* MAX_HANDLES * ID_BLACKBOX_MAX */
#define LOG_BUFFER_SIZE 24
#endif

/*
 * This is the main tfa device context structure, it will carry all information
 * that is needed to handle a single I2C device instance.
 * All functions dealing with the device will need access to the fields herein.
 */
struct tfa_device {
	int dev_idx;
	int in_use;
	int buffer_size;
	int has_msg;
	unsigned char resp_address;
	unsigned short rev;
	unsigned char tfa_family;
	enum feature_support support_drc;
	enum feature_support support_framework;
	enum feature_support support_saam;
	int sw_feature_bits[2];
	int hw_feature_bits;
	int profile;
	int next_profile;
	int vstep;
	unsigned char spkr_count;
	unsigned char spkr_select;
	unsigned char support_tcoef;
	enum tfa98xx_dai_bitmap daimap;
	int mohm[3];
	struct tfa_device_ops dev_ops;
	uint16_t interrupt_enable[3];
	uint16_t interrupt_status[3];
	int ext_dsp;
	int bus;
	int tfadsp_event;
	int verbose;
	enum tfa_state state;
	struct tfa_container *cnt;
	struct tfa_volume_step_register_info *p_reg_info;
	int partial_enable;
	void *data;
	int convert_dsp32;
	int sync_iv_delay;
	int is_probus_device;
	int advance_keys_handling;
	int needs_reset;
	struct kmem_cache *cachep;
	char fw_itf_ver[4];

	int dev_count;
	int dev_tfadsp;
#if defined(TFA_MIXER_ON_DEVICE)
	int set_active;
#endif
#if defined(TFA_MUTE_CONTROL)
	int mute_state;
#endif
#if defined(TFA_PAUSE_CONTROL)
	int pause_state;
#endif
#if defined(TFA_TDMSPKG_CONTROL)
	int spkgain;
#endif
#if defined(TFA_CHANGE_PCM_FORMAT)
	struct tfa98xx_tdm_format tdm_config;
#endif
	int temp;
	int spkr_damaged; /* 0: okay, 1: damaged */
	int is_cold;
	int is_bypass;
	int is_calibrating;
	int is_configured;
	int reset_mtpex;
	int stream_state; /* b0: pstream (Rx), b1: cstream (Tx), b2:SaaM */
	int prev_samstream;
	int first_after_boot;
	int active_handle;
	int active_count;
	int ampgain;
	int individual_msg;
	int set_device;
	int set_config;
#if defined(TFADSP_DSP_BUFFER_POOL)
	struct tfa98xx_buffer_pool buf_pool[POOL_MAX_INDEX];
#endif
#if defined(TFA_USE_WAITQUEUE_SEQ)
	wait_queue_head_t waitq_seq;
#endif
#if defined(TFA_WAIT_CAL_IN_WORKQUEUE)
	struct workqueue_struct *tfacal_wq;
	struct delayed_work wait_cal_work;
#endif
#if defined(TFA_LIMIT_CAL_FROM_DTS)
	int lower_limit_cal;
	int upper_limit_cal;
#endif
	char fw_lib_ver[3];
#if defined(TFA_BLACKBOX_LOGGING)
	int blackbox_enable;
	int unset_log;
	int log_data[LOG_BUFFER_SIZE];
#endif
	int irq_all;
	int irq_max;
#if defined(TFA_USE_TFAVVAL_NODE)
	int vval_active;
	int vval_result; /* 0: pass, 1: fail */
#endif
#if defined(TFA_DISABLE_AUTO_CAL)
	int disable_auto_cal;
#endif
#if defined(TFA_USE_DUMMY_CAL)
	int dummy_cal;
#endif
};

/*
 * The tfa_dev_probe is called before accessing any device accessing functions.
 * Access to the tfa device register 3 is attempted and will record the
 * returned id for further use. If no device responds the function will abort.
 * The recorded id will by used by the query functions to fill the remaining
 * relevant data fields of the device structure.
 * Data such as MTP features that requires device access will only be read when
 * explicitly called and the result will be then cached in the struct.
 *
 * A structure pointer passed to this device needs to refer to existing memory
 * space allocated by the caller.
 *
 *  @param resp_addr = I2C responder address of the target device (not shifted)
 *  @param tfa struct = points to memory that holds the context for this device
 *  instance
 *
 *  @return
 *   - 0 if the I2C device responded to a read of register address 3\n
 *       when device responds but with an unknown id a warning will be printed
 *   - -1 if no response from the I2C device
 *
 */
int tfa_dev_probe(int resp_addr, struct tfa_device *tfa);

/*
 * Start this instance at the profile and vstep as provided.
 * The profile and vstep will be loaded first in case the current value differs
 * from the requested values.
 * Note that this call will not change the mute state of the tfa, which means
 * that of this instance was called in muted state the caller will have to
 * unmute in order to get audio.
 *
 *  @param tfa struct = pointer to context of this device instance
 *  @param profile the selected profile to run
 *  @param vstep the selected vstep to use
 *  @return tfa_error enum
 */
enum tfa_error tfa_dev_start(struct tfa_device *tfa, int profile, int vstep);

enum tfa_error tfa_dev_switch_profile(struct tfa_device *tfa,
	int profile, int vstep);

/*
 * Stop audio for this instance as gracefully as possible.
 * Audio will be muted and the PLL will be shutdown together with any other
 * device/type specific settings needed to prevent audio artifacts or
 * workarounds.
 *
 * Note that this call will change state of the tfa to mute and powered down.
 *
 *  @param tfa struct = pointer to context of this device instance
 *  @return tfa_error enum
 */
enum tfa_error tfa_dev_stop(struct tfa_device *tfa);

#if defined(TFA_CHANGE_PCM_FORMAT)
enum tfa_error tfa_dev_config_pcm_format(struct tfa_device *tfa,
	int ndev, int hw_rate, int sample_size, int slot_size);
#endif

/*
 * This interface allows a device/type independent fine grained control of
 * internal state of the instance.
 * Whenever a base state is requested an attempt is made to  bring device
 * into this state. However this may depend on external conditions beyond
 * this software layer. Therefore in case the state cannot be set an erro will
 * be returned and the current state remains unchanged.
 * The base states, lower values below 0x10, are all mutually exclusive,
 * higher ones can also function as a sticky modifier which means for example
 * that operating state could be in either muted or unmuted state.
 * Or in case of init_cf it can be
 * internal clock (always) or external audio clock.
 * This function is intended to be used for device mute/unmute synchronization
 * when called from higher layers. Mostly internal calls will use this
 * to control the startup and profile transitions in a independent way.
 *
 *  @param tfa struct = pointer to context of this device instance
 *  @param state struct = desired device state after function return
 *  @return tfa_error enum
 */
enum tfa_error tfa_dev_set_state(struct tfa_device *tfa,
	enum tfa_state state, int is_calibration);

/*
 * Retrieve the current state of this instance in an active way.
 * The state field in tfa structure will reflect the result unless an error is
 * returned.
 * Note that the hardware state may change on external events an as such this
 * field should be treated as volatile.
 *
 *  @param tfa struct = pointer to context of this device instance
 *  @return tfa_error enum
 *
 */
enum tfa_state tfa_dev_get_state(struct tfa_device *tfa);

/*****************************************************************************/
/*
 *  MTP support functions
 */
enum tfa_mtp {
	TFA_MTP_OTC,
	TFA_MTP_EX,
	TFA_MTP_RE25,
	TFA_MTP_RE25_PRIM,
	TFA_MTP_RE25_SEC,
	TFA_MTP_LOCK,
};

int tfa_dev_mtp_get(struct tfa_device *tfa, enum tfa_mtp item);
enum tfa_error tfa_dev_mtp_set(struct tfa_device *tfa,
	enum tfa_mtp item, int value);

/*
 * tfa2 interrupt support
 * interrupt bit function to clear
 */
int tfa_irq_clear(struct tfa_device *tfa, int bit);
/*
 * return state of irq or -1 if illegal bit
 */
int tfa_irq_get(struct tfa_device *tfa, int bit);
/*
 * interrupt bit function that operates on the shadow regs in the handle
 */
int tfa_irq_ena(struct tfa_device *tfa, int bit, int state);
/*
 * interrupt bit function that sets the polarity
 */
int tfa_irq_set_pol(struct tfa_device *tfa, int bit, int state);
/*
 * mask interrupts by disabling them
 */
int tfa_irq_mask(struct tfa_device *tfa);
/*
 * unmask interrupts by enabling them again
 */
int tfa_irq_unmask(struct tfa_device *tfa);

enum tfa98xx_error tfa_get_fw_api_version(struct tfa_device *tfa,
	unsigned char *pfw_version);
enum tfa98xx_error tfa_get_fw_lib_version(struct tfa_device *tfa,
	unsigned char *plib_version);

#if defined(TFA_RAMPDOWN_BEFORE_MUTE)
#define RAMPDOWN_MAX 2 /* 5 or higher if usleep_range works */
enum tfa98xx_error tfa_gain_rampdown(struct tfa_device *tfa,
	int step, int count);
enum tfa98xx_error tfa_gain_restore(struct tfa_device *tfa,
	int step, int count);
#endif

#if defined(MPLATFORM)
enum tfa98xx_error ipi_tfadsp_write(struct tfa_device *tfa,
	int length, const char *buf);
enum tfa98xx_error ipi_tfadsp_read(struct tfa_device *tfa,
	int length, unsigned char *bytes);
#endif

#endif /* __TFA_DEVICE_H__ */
