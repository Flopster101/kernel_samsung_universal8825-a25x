/*
 * sound/soc/codec/s5m3700x-jack.h
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S5M3700X_JACK_H
#define _S5M3700X_JACK_H

/* Jack state flag */
#define JACK_OUT		BIT(0)
#define JACK_POLE_DEC	BIT(1)
#define JACK_3POLE		BIT(2)
#define JACK_4POLE		BIT(3)
#define JACK_AUX		BIT(4)
#define JACK_OMTP		BIT(5)
#define JACK_CMP		BIT(6)
#define JACK_IMP		BIT(7)
#define JACK_WTP_DEC	BIT(8)
#define JACK_WTP_JIO	BIT(9)
#define JACK_WTP_CJI	BIT(10)
#define JACK_WTP_ETC	BIT(11)
#define JACK_WTP_POLL	BIT(12)
#define JACK_WTP_JO		BIT(13)
#define JACK_IN			(BIT(2) | BIT(3) | BIT(4) | BIT(5))
#define JACK_WTP_ST		(BIT(8) | BIT(9) | BIT(10) | BIT(11) | BIT(12) | BIT(13))

#define USB_OUT			BIT(16)
#define USB_INIT		BIT(17)
#define USB_JACK_CMP	BIT(18)
#define USB_JACK_IMP	BIT(19)
#define USB_POLE_DEC	BIT(20)
#define USB_3POLE		BIT(21)
#define USB_4POLE		BIT(22)
#define USB_AUX			BIT(23)
#define USB_MCHK_DONE	BIT(24)
#define USB_IN			(BIT(21) | BIT(22) | BIT(23))
#define USB_INSERTING	(BIT(17) | BIT(18) | BIT(19) | BIT(20) | BIT(24))

#define S5M3700X_GDET_DELAY					20
#define S5M3700X_MDET_DELAY					50
#define S5M3700X_MIC_ADC_RANGE_0			1120
#define S5M3700X_MIC_ADC_RANGE_1			3447
#define S5M3700X_MDET_CHK_DEFAULT			20
#define S5M3700X_LDET_CHK_DEFAULT			0
#define S5M3700X_OMTP_ADC_DEFAULT			-1
#define S5M3700X_USB_ADC_DEFAULT			300
#define S5M3700X_BTN_ADC_DELAY				0
#define S5M3700X_BTN_REL_DEFAULT			1100
#define S5M3700X_BTN_DBNC_DEFAULT			3
#define S5M3700X_ADC_THD_FAKE_JACK			177
#define S5M3700X_ADC_THD_WATER_IN			494
#define S5M3700X_ADC_THD_WATER_OUT			3686
#define S5M3700X_JACK_IMP_RANGE_0			0
#define S5M3700X_JACK_IMP_RANGE_1			15
#define S5M3700X_JACK_IMP_RANGE_2			32
#define S5M3700X_JACK_IMP_RANGE_3			50
#define S5M3700X_JACK_IMP_RANGE_4			110
#define S5M3700X_USB_HP_CHL_OFFSET			100
#define S5M3700X_USB_HP_CHR_OFFSET			100

/* Button state */
enum {
	BUTTON_RELEASE = 0,
	BUTTON_PRESS,
	BUTTON_PRESS_RELEASE,
};

/* Jack ADC type */
enum {
	ADC_POLE = 0,
	ADC_BTN,
	ADC_IMP,
	ADC_WTP,
};

/* ADC Type */
enum {
	JACK_LDET_ADC = 0,
	JACK_GDET_ADC = 1,
	JACK_MDET_ADC = 2,
	JACK_IMP_ADC = 3,
	JACK_WTP_LDET_ADC = 4,
};

/* IRQ return type */
enum {
	IRQ_ST_CMP_JACK_IN = 0,
	IRQ_ST_IMP_CHK,
	IRQ_ST_JACKDET,
	IRQ_ST_JACKOUT,
	IRQ_ST_WTJACK_DEC,
	IRQ_ST_WTJACK_IN,
	IRQ_ST_WTJACK_OUT,
	IRQ_ST_WTJACK_ETC,
	IRQ_ST_BTN_DET_R,
	IRQ_ST_BTN_DET_F,
	IRQ_ST_BTN_DET_R_F,
	IRQ_ST_ERR,
	IRQ_ST_USB_JACKDET,
};

enum {
	JACK_IMP_VAL_1 = 0,
	JACK_IMP_VAL_2,
	JACK_IMP_VAL_3,
	JACK_IMP_VAL_4,
	JACK_IMP_VAL_5,
};

enum {
	USBJACK_OUT = 0,
	USBJACK_IN = 1,
	MIXER_CONTROL = 2,
};

enum max_function {
	MAX_MIC_GND_SWAP,
	MAX_GND_MIC_SWAP,
	MAX_USBC_AUDIO_HP_ON,
	MAX_USBC_AUDIO_HP_OFF,
	MAX_USBC_ORIENTATION_CC1,
	MAX_USBC_ORIENTATION_CC2,
	MAX_USBC_DISPLAYPORT_DISCONNECTED,
	MAX_USBC_FAST_CHARGE_SELECT,
	MAX_USBC_FAST_CHARGE_EXIT,
	MAX_USBC_SWITCH_ENABLE,
	MAX_USBC_SWITCH_DISABLE,
	MAX_USBC_SWITCH_SBU_DIRECT_CONNECT,
	MAX_USBC_SWITCH_SBU_FLIP_CONNECT,
	MAX_USBC_SWITCH_SBU_HIZ,
	MAX_EVENT_MAX,
};

/* Jack state machine */
struct earjack_state {
	/* Jack */
	int prv_jack_state;
	int cur_jack_state;
	int ldet_adc;
	int mdet_adc;
	/* Button */
	bool prv_btn_state;
	bool cur_btn_state;
	int button_code;
	int btn_adc;
};

/* Button range */
struct jack_buttons_zone {
	int code;
	int adc_low;
	int adc_high;
};

struct s5m3700x_priv;

/* Jack information struct */
struct s5m3700x_jack {
	/* jack driver default */
	struct snd_soc_component *codec;
	struct s5m3700x_priv *p_s5m3700x;
	struct input_dev *input;
	u8 irq_val[10];
	struct mutex key_lock;
	struct mutex mdet_lock;
	struct mutex adc_lock;
	struct mutex usb_mb_lock;
	struct wakeup_source *jack_wakeup;
	/* jack workqueue */
	struct delayed_work buttons_work;
	struct workqueue_struct *buttons_press_wq;
	struct workqueue_struct *buttons_release_wq;
	struct delayed_work jack_det_work;
	struct workqueue_struct *jack_det_wq;
	struct delayed_work usbjack_work;
	struct workqueue_struct *usbjack_wq;
	struct delayed_work mdet_chk_work;
	struct workqueue_struct *mdet_chk_wq;
	struct delayed_work ldet_chk_work;
	struct workqueue_struct *ldet_chk_wq;
	/* jack parse dt */
	int irqb_gpio;
	int codec_irq;
	int mic_bias1_voltage;
	int mic_bias2_voltage;
	int mic_bias3_voltage;
	int mdet_delay;
	int mdet_chk_delay;
	int ldet_chk_delay;
	int t_pole_cnt;
	int t_btn_cnt;
	int t_pole_delay;
	int t_btn_delay;
	int omtp_range;
	int mic_adc_range[2];
	int usb_switch_range;
	int btn_adc_delay;
	int jack_mcb_cnt;
	int mix_mcb_cnt;
	struct jack_buttons_zone jack_buttons_zones[4];
	int btn_release_value;
	int btn_dbnc_value;
	int btn_state;
	int usb_hp_chl;
	int usb_hp_chr;
	/* Imp value */
	int imp_p[3];
	/* ldet adc thd */
	int adc_thd_fake_jack;
	int adc_thd_auxcable;
	int adc_thd_water_in;
	int adc_thd_water_out;
	/* Jack impedance range */
	int jack_imp_range[5];
	/* jack state machine */
	struct earjack_state jack_state;
	int earjack_re_read;
	/* switch ic state */
	int switch_ic_state;
	int switch_jack_done;
	int usbjack_re_read;
};

int s5m3700x_jack_probe(struct s5m3700x_priv *s5m3700x);
int s5m3700x_jack_remove(struct snd_soc_component *codec);
int s5m3700x_usbjack_probe(struct s5m3700x_priv *s5m3700x);
int s5m3700x_usbjack_remove(struct snd_soc_component *codec);

#endif /* _S5M3700X_JACK_H */
