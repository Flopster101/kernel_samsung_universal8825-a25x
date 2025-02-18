/*
 * sound/soc/codec/s5m3700x-jack.c
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include <linux/input.h>
#include <linux/of_gpio.h>
#include <uapi/linux/input-event-codes.h>

#include "s5m3700x.h"
#include "s5m3700x-jack.h"

static BLOCKING_NOTIFIER_HEAD(s5m3700x_notifier);
struct notifier_block s5m3700x_notifier_block;

struct s5m3700x_notifier_struct {
	struct s5m3700x_priv *s5m3700x;
};
static struct s5m3700x_notifier_struct s5m3700x_notifier_t;

int s5m3700x_register_notifier(struct notifier_block *n,
		struct s5m3700x_priv *s5m3700x)
{
	int ret;

	s5m3700x_notifier_t.s5m3700x = s5m3700x;
	ret = blocking_notifier_chain_register(&s5m3700x_notifier, n);
	if (ret < 0)
		pr_err("[IRQ] %s(%d)\n", __func__, __LINE__);
	return ret;
}

static void jack_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}

static void jack_wake_unlock(struct wakeup_source *ws)
{
	__pm_relax(ws);
}

static int jack_set_wake_lock(struct s5m3700x_jack *jackdet)
{
	struct wakeup_source *ws = NULL;
	ws = wakeup_source_register(NULL, "s5m3700x-jack");

	if (ws == NULL)
		goto err;

	jackdet->jack_wakeup = ws;

	return 0;
err:
	return -1;
}
/*
 * mute_mic() - Mute mic if it is active
 *
 * @s5m3700x: codec information struct
 * @on: mic mute is true, mic unmute is false
 *
 * Desc: It can mute adc depending on the parameter
 */
static void mute_mic(struct s5m3700x_priv *s5m3700x, bool on)
{
	dev_info(s5m3700x->dev, "%s called, %s\n", __func__, on ? "Mute" : "Unmute");

	if (on)
		s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, true);
	else
		s5m3700x_adc_digital_mute(s5m3700x, ADC_MUTE_ALL, false);
}

static int read_adc(struct s5m3700x_jack *jackdet, int type, int pin, int repeat)
{
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;
	u8 v1, v2;
	unsigned int value = 0, i;

	mutex_lock(&jackdet->adc_lock);

	switch (pin) {
	case JACK_LDET_ADC:
		regcache_cache_switch(s5m3700x, false);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E4_DCTR_FSM5,
					GPMUX_POLE_MASK, POLE_HPL_DET << GPMUX_POLE_SHIFT);
		regcache_cache_switch(s5m3700x, true);
		break;
	case JACK_MDET_ADC:
		regcache_cache_switch(s5m3700x, false);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E4_DCTR_FSM5,
					GPMUX_POLE_MASK, POLE_MIC_DET << GPMUX_POLE_SHIFT);
		regcache_cache_switch(s5m3700x, true);
		break;
	case JACK_IMP_ADC:
		break;
	case JACK_WTP_LDET_ADC:
		regcache_cache_switch(s5m3700x, false);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E4_DCTR_FSM5,
					GPMUX_WTP_MASK, POLE_HPL_DET << GPMUX_WTP_SHIFT);
		regcache_cache_switch(s5m3700x, true);
		break;
	}

	switch (type) {
	case ADC_POLE:
		regcache_cache_switch(s5m3700x, false);
		for (i = 0; i < repeat; i++) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E5_DCTR_FSM6,
					AP_REREAD_POLE_MASK, AP_REREAD_POLE_MASK);
			s5m3700x_usleep(100);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E5_DCTR_FSM6,
					AP_REREAD_POLE_MASK, 0);
			jackdet->earjack_re_read = 1;
		}
		regcache_cache_switch(s5m3700x, true);

		msleep(100);

		s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_F4_STATUS5, &v1);
		s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_F7_STATUS8, &v2);

		v1 = v1 & 0x0F;
		value = (v1 << 8) + v2;
		break;
	case ADC_BTN:
		regcache_cache_switch(s5m3700x, false);
		for (i = 0; i < repeat; i++) {
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E5_DCTR_FSM6,
					AP_REREAD_BTN_MASK, AP_REREAD_BTN_MASK);
			s5m3700x_usleep(100);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E5_DCTR_FSM6,
					AP_REREAD_BTN_MASK, 0);
		}
		regcache_cache_switch(s5m3700x, true);
		msleep(10);

		s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_F5_STATUS6, &v1);
		s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_F8_STATUS9, &v2);

		v1 = v1 & 0xF0;
		value = (v1 << 4) + v2;
		break;
	case ADC_IMP:
		s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_F5_STATUS6, &v1);
		s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_F9_STATUS10, &v2);

		v1 = v1 & 0x0F;
		value = (v1 << 8) + v2;
		break;
	case ADC_WTP:
		s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_F2_STATUS3, &v1);
		s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_F3_STATUS4, &v2);

		v1 = v1 & 0xF0;
		value = (v1 << 8) + v2;
		break;
	}

	mutex_unlock(&jackdet->adc_lock);
	dev_info(s5m3700x->dev, "%s called. ADC Pin: %d, Value: %d\n",
			__func__, pin, value);
	return value;
}

static void s5m3700x_read_imp_otp(struct s5m3700x_jack *jackdet)
{
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;
	bool msb;
	u8 jack_otp[4];

	/* Jack OTP Setting */
	s5m3700x_i2c_read_reg(S5M3700X_OTP_ADDR, S5M3700X_2D2_JACK_OTP02, &jack_otp[0]);
	s5m3700x_i2c_read_reg(S5M3700X_OTP_ADDR, S5M3700X_2D3_JACK_OTP03, &jack_otp[1]);
	s5m3700x_i2c_read_reg(S5M3700X_OTP_ADDR, S5M3700X_2D4_JACK_OTP04, &jack_otp[2]);
	s5m3700x_i2c_read_reg(S5M3700X_OTP_ADDR, S5M3700X_2D5_JACK_OTP05, &jack_otp[3]);

	jackdet->imp_p[0] = jack_otp[1];
	jackdet->imp_p[1] = ((jack_otp[0] & 0x70) << 4) + jack_otp[2];
	jackdet->imp_p[2] = ((jack_otp[0] & 0x0C) << 6) + jack_otp[3];
	msb = jackdet->imp_p[2] & 0x200;

	if (msb)
		jackdet->imp_p[2] = -(0x400 - jackdet->imp_p[2]);

	dev_info(s5m3700x->dev, "%s called. p1: %d, p2: %d, p3: %d\n",
			__func__, jackdet->imp_p[0],
			jackdet->imp_p[1], jackdet->imp_p[2]);
}

static int calculate_imp(struct s5m3700x_jack *jackdet, unsigned int d_out)
{
	long long p1 = jackdet->imp_p[0];
	long long p2 = jackdet->imp_p[1];
	long long p3 = jackdet->imp_p[2];
	long long c1 = 99, c2 = 79, c3 = 670;
	long long temp1, temp2, temp3, result;

	if ((p1 == 0) || (p2 == 0) || (p3 == 0))
		return 32;

	temp1 = p1 * ((c3 * p1) + (long long)1024) *
		((c2 * (long long)d_out) - (c2 * p3) - (c1 * p2) - (c2 * p2));
	temp2 = p2 * ((c1 * p1) + (c2 * p1) + (c3 * p1) + (long long)1024);
	temp3 = (d_out - p3) * ((c3 * p1) + (c2 * p1) + (long long)1024);
	result = temp1 / ((long long)128 * (temp2 - temp3));

	dev_info(jackdet->p_s5m3700x->dev,
			"%s called. tp1: %lld, tp2: %lld, tp3: %lld\n",
			__func__, temp1, temp2, temp3);

	return result;
}

static int s5m3700x_get_imp(struct s5m3700x_jack *jackdet)
{
	unsigned int imp_adc;
	int cal;

	imp_adc = read_adc(jackdet, ADC_IMP, JACK_IMP_ADC, 0);
	cal = calculate_imp(jackdet, imp_adc);

	dev_info(jackdet->p_s5m3700x->dev,
			"%s called. impedance: %d\n", __func__, cal);

	return cal;
}

static char *s5m3700x_return_status_name(unsigned int status)
{
	switch (status) {
	case JACK_OUT:
		return "JACK_OUT";
	case JACK_POLE_DEC:
		return "JACK_POLE_DEC";
	case JACK_3POLE:
		return "JACK_3POLE";
	case JACK_4POLE:
		return "JACK_4POLE";
	case JACK_AUX:
		return "JACK_AUX";
	case JACK_OMTP:
		return "JACK_OMTP";
	case JACK_CMP:
		return "JACK_CMP";
	case JACK_IMP:
		return "JACK_IMP";
	case JACK_WTP_DEC:
		return "JACK_WTP_DEC";
	case JACK_WTP_JIO:
		return "JACK_WTP_JIO";
	case JACK_WTP_CJI:
		return "JACK_WTP_CJI";
	case JACK_WTP_ETC:
		return "JACK_WTP_ETC";
	case JACK_WTP_POLL:
		return "JACK_WTP_POLL";
	case JACK_WTP_JO:
		return "JACK_WTP_JO";
	case JACK_IN:
		return "JACK_IN";
	default:
		return "No status";
	}
}

static void s5m3700x_print_status_change(struct device *dev, unsigned int prv_jack, unsigned int cur_jack)
{
	dev_info(dev, "Priv: %s -> Cur: %s\n"
		, s5m3700x_return_status_name(prv_jack), s5m3700x_return_status_name(cur_jack));
}

static bool s5m3700x_jackstate_register(struct s5m3700x_jack *jackdet)
{
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_s5m3700x->dev;
	unsigned int cur_jack, prv_jack, mic4_on, imp_value;

	cur_jack = jackstate->cur_jack_state;
	prv_jack = jackstate->prv_jack_state;

	regcache_cache_switch(s5m3700x, false);

	switch (cur_jack) {
	case JACK_OUT:
		s5m3700x_read(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_1E_CHOP1, &mic4_on);
		mic4_on &= MIC4_ON_MASK;

		if (mic4_on)
			mute_mic(s5m3700x, true);

		if (prv_jack & (JACK_IN | JACK_WTP_ST | JACK_POLE_DEC | JACK_IMP | JACK_CMP)) {
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
			jackdet->earjack_re_read = 0;

			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_3A_AD_VOL, 0x00);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E0_DCTR_FSM1, 0x0A);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E3_DCTR_FSM4, 0x00);

			/* ODSEL5 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_B4_ODSEL5,
					EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK, EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK);
		} else {
			goto err;
		}
		break;
	case JACK_CMP:
		if (prv_jack & (JACK_OUT | JACK_WTP_CJI | JACK_WTP_ETC))
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
		else
			goto err;
		break;
	case JACK_WTP_DEC:
		if (prv_jack & (JACK_OUT | JACK_CMP | JACK_WTP_JIO))
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
		else
			goto err;
		break;
	case JACK_WTP_CJI:
		if (prv_jack & JACK_WTP_DEC) {
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
			/* Completely Jack In */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E0_DCTR_FSM1,
					AP_CJI_MASK, AP_CJI_MASK);
			s5m3700x_usleep(100);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E0_DCTR_FSM1,
					AP_CJI_MASK, 0x0);
		} else {
			goto err;
		}
		break;
	case JACK_WTP_ETC:
		if (prv_jack & JACK_WTP_DEC) {
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
			/* Slightly Jack In */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E0_DCTR_FSM1,
					AP_ETC_MASK, AP_ETC_MASK);
			s5m3700x_usleep(100);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E0_DCTR_FSM1,
					AP_ETC_MASK, 0x0);
		} else {
			goto err;
		}
		break;
	case JACK_WTP_POLL:
		if (prv_jack & JACK_WTP_DEC) {
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
			/* Water In */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E0_DCTR_FSM1,
					AP_POLLING_MASK, AP_POLLING_MASK);
			s5m3700x_usleep(100);
		} else {
			goto err;
		}
		break;
	case JACK_WTP_JO:
		if (prv_jack & JACK_WTP_DEC) {
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
			/* Jack Out*/
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E0_DCTR_FSM1,
					AP_JO_MASK, AP_JO_MASK);
			s5m3700x_usleep(100);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E0_DCTR_FSM1,
					AP_JO_MASK, 0x0);
		} else {
			goto err;
		}
		break;
	case JACK_WTP_JIO:
		if (prv_jack & JACK_WTP_POLL) {
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
			/* Polling off */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E0_DCTR_FSM1,
					AP_POLLING_MASK, 0x00);
			s5m3700x_usleep(100);
			/* GPADC Re-read */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E5_DCTR_FSM6,
					AP_REREAD_WTP_MASK, AP_REREAD_WTP_MASK);
			s5m3700x_usleep(100);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E5_DCTR_FSM6,
					AP_REREAD_WTP_MASK, 0x00);
		} else {
			goto err;
		}
		break;
	case JACK_IMP:
		if (prv_jack & JACK_CMP) {
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);

			imp_value = s5m3700x_get_imp(jackdet);

			if (imp_value < 10)
				s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_EE_ACTR_ETC1,
						IMP_VAL_MASK, 2 << IMP_VAL_SHIFT);
			else if (imp_value >= 10 && imp_value < 32)
				s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_EE_ACTR_ETC1,
						IMP_VAL_MASK, 1 << IMP_VAL_SHIFT);
			else
				s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_EE_ACTR_ETC1,
						IMP_VAL_MASK, 0 << IMP_VAL_SHIFT);
		} else {
			goto err;
		}
		break;
	case JACK_POLE_DEC:
		if (prv_jack & (JACK_OUT | JACK_IMP))
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
		else
			goto err;
		break;
	case JACK_4POLE:
		if (prv_jack & JACK_POLE_DEC) {
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E3_DCTR_FSM4,
					POLE_VALUE_MASK, POLE_VALUE_4POLE << POLE_VALUE_SHIFT);
			s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_3A_AD_VOL, 0x07);
			/* ODSEL5 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_B4_ODSEL5,
					EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK, 0x00);
		} else {
			goto err;
		}
		break;
	case JACK_3POLE:
		if (prv_jack & JACK_POLE_DEC) {
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E3_DCTR_FSM4,
					POLE_VALUE_MASK, POLE_VALUE_3POLE << POLE_VALUE_SHIFT);
			/* ODSEL5 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_B4_ODSEL5,
					EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK, 0x00);
		} else {
			goto err;
		}
		break;
	case JACK_AUX:
		if (prv_jack & (JACK_POLE_DEC | JACK_WTP_ETC)) {
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E3_DCTR_FSM4,
					POLE_VALUE_MASK, POLE_VALUE_AUX << POLE_VALUE_SHIFT);
			/* ODSEL5 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_B4_ODSEL5,
					EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK, 0x00);
		} else {
			goto err;
		}
		break;
	case JACK_OMTP:
		if (prv_jack & JACK_POLE_DEC) {
			s5m3700x_print_status_change(dev, prv_jack, cur_jack);

			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E3_DCTR_FSM4,
					POLE_VALUE_MASK, POLE_VALUE_OMTP << POLE_VALUE_SHIFT);
			/* ODSEL5 */
			s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_B4_ODSEL5,
					EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK, 0x00);
		} else {
			goto err;
		}
		break;
	}

	regcache_cache_switch(s5m3700x, true);
	return true;

err:
	regcache_cache_switch(s5m3700x, true);
	dev_err(dev, "%s Jack state machine error! prv: 0x%02x, cur: 0x%02x\n",
			__func__, jackstate->prv_jack_state, jackstate->cur_jack_state);
	return false;
}

static void s5m3700x_jackstate_set(struct s5m3700x_jack *jackdet,
		unsigned int change_state)
{
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_s5m3700x->dev;
	bool ret;

	/* Save privious jack state */
	jackstate->prv_jack_state = jackstate->cur_jack_state;

	/* Update current jack state */
	jackstate->cur_jack_state = change_state;

	if (jackstate->prv_jack_state != jackstate->cur_jack_state) {
		dev_info(dev, "%s called, Prv: 0x%02x, Cur: 0x%02x\n",  __func__,
				jackstate->prv_jack_state, jackstate->cur_jack_state);

		/* Set jack register */
		ret = s5m3700x_jackstate_register(jackdet);
		dev_info(dev, "Jack register write %s.\n",
				ret ? "complete" : "incomplete");
	} else {
		dev_info(dev, "Prv_jack_state and Cur_jack_state are same.\n");
	}
}

/*
 * process_button_ev() - Process the button events
 *
 * @jackdet: jack information struct
 * @on: button press is true, button release is false.
 *
 * Desc: It processes button press or release events. It gives information
 * about the button state to framework layer.
 */
static void process_button_ev(struct s5m3700x_jack *jackdet, bool on)
{
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;

	dev_info(s5m3700x->dev, "%s: key %d is %s, adc: %d\n",
			__func__, jackstate->button_code,
			on ? "pressed" : "released", jackstate->btn_adc);

	input_report_key(jackdet->input, jackstate->button_code, on);
	input_sync(jackdet->input);
	mute_mic(s5m3700x, on);
}

/*
 * before_button_error_chk() - Check button state error
 *
 * @jackdet: jack information struct
 *
 * Desc: Error check condition for button irq before current button state check.
 * Return: There is not problem, return true.
 */
static bool before_button_error_chk(struct s5m3700x_jack *jackdet)
{
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_s5m3700x->dev;

	/* Terminate workqueue Cond1: jack is not detected */
	if (jackstate->cur_jack_state & JACK_OUT) {
		dev_err(dev, "Skip button events because jack is not detected.\n");
		if (jackstate->cur_btn_state == BUTTON_PRESS) {
			jackstate->prv_btn_state = BUTTON_RELEASE;
			process_button_ev(jackdet, BUTTON_RELEASE);
		}
		return false;
	}

	/* Terminate workqueue Cond2: 3 pole earjack */
	if (!(jackstate->cur_jack_state & JACK_4POLE)) {
		dev_err(dev, "Skip button events for 3 pole earjack.\n");
		return false;
	}

	/* Terminate workqueue Cond3: button adc error */
	if (jackstate->btn_adc == -EINVAL) {
		dev_err(dev, "Button ADC Error! BTN ADC: %d\n", jackstate->btn_adc);
		return false;
	}

	return true;
}

/*
 * after_button_error_chk() - Check button state error
 *
 * @jackdet: jack information struct
 *
 * Desc: Error check condition for button irq after current button state check.
 * Return: There is not problem, return true.
 */
static bool after_button_error_chk(struct s5m3700x_jack *jackdet)
{
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_s5m3700x->dev;

	/* Terminate workqueue Cond4: 3 pole earjack with button press */
	if ((jackstate->cur_btn_state == BUTTON_PRESS) &&
			!(jackstate->cur_jack_state & JACK_4POLE)) {
		dev_err(dev, "Skip button press event for 3 pole earjack.\n");
		jackstate->prv_btn_state = BUTTON_RELEASE;
		process_button_ev(jackdet, BUTTON_RELEASE);
		return false;
	}

	/* Terminate workqueue Cond5: button state not changed with button press */
	if ((jackstate->cur_btn_state == BUTTON_PRESS) &&
			(jackstate->prv_btn_state == jackstate->cur_btn_state)) {
		dev_err(dev, "Skip button press event, button status are same.\n");
		jackstate->prv_btn_state = BUTTON_RELEASE;
		process_button_ev(jackdet, BUTTON_RELEASE);
		return false;
	}

	/* Terminate workqueue Cond6: button state not changed */
	if (jackstate->prv_btn_state == jackstate->cur_btn_state) {
		dev_err(dev, "Button status are same.\n");
		return false;
	}

	return true;
}

static void reset_mic4_boost(struct s5m3700x_jack *jackdet)
{
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;

	regcache_cache_switch(s5m3700x, false);

	/* Ear-mic BST Reset */
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_AF_BST_CTR,
			EN_ARESETB_BST4_MASK, EN_ARESETB_BST4_MASK);
	s5m3700x_usleep(100);
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_AF_BST_CTR,
			EN_ARESETB_BST4_MASK, 0);

	regcache_cache_switch(s5m3700x, true);
}

/*
 * s5m3700x_buttons_work() - Process button interrupt
 *
 * @work: delayed workqueue
 *
 * Desc: When button interrupt is occurred, generated buttons_work process.
 * Judge whether button press or release using button adc. And notify to
 * framework layer.
 */
static void s5m3700x_buttons_work(struct work_struct *work)
{
	struct s5m3700x_jack *jackdet =
		container_of(work, struct s5m3700x_jack, buttons_work.work);
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_s5m3700x->dev;
	struct jack_buttons_zone *btn_zones = jackdet->jack_buttons_zones;
	int num_buttons_zones = ARRAY_SIZE(jackdet->jack_buttons_zones);
	int i;

	/* Read adc value */
	jackstate->btn_adc = read_adc(jackdet, ADC_BTN, JACK_MDET_ADC, 0);

	/* Check error status */
	if (!before_button_error_chk(jackdet)) {
		jack_wake_unlock(jackdet->jack_wakeup);
		return;
	}

	/* Check button press/release */
	dev_info(dev, "Button %s! adc: %d, btn_release_value: %d\n",
			jackstate->cur_btn_state == BUTTON_RELEASE ? "Released" : "Pressed",
			jackstate->btn_adc,	jackdet->btn_release_value);

	/* Check error status */
	if (!after_button_error_chk(jackdet)) {
		jack_wake_unlock(jackdet->jack_wakeup);
		return;
	}

	/* Save button information */
	jackstate->prv_btn_state = jackstate->cur_btn_state;

	if (jackstate->cur_btn_state == BUTTON_PRESS) {
		/* Determine which button pressed */
		for (i = 0; i < num_buttons_zones; i++) {
			if (jackstate->btn_adc <= btn_zones[i].adc_high) {
				jackstate->button_code = btn_zones[i].code;
				dev_info(dev,
						"%s: adc: %d, btn code: %d, low: %d, high: %d\n",
						__func__, jackstate->btn_adc, jackstate->button_code,
						btn_zones[i].adc_low, btn_zones[i].adc_high);
				process_button_ev(jackdet, BUTTON_PRESS);
				break;
			}
		}
	} else {
		/* Ear-mic BST4 Reset */
		reset_mic4_boost(jackdet);

		/* Button released */
		process_button_ev(jackdet, BUTTON_RELEASE);
	}

	/* Terminate workqueue Cond7: Button press and release event duplicated. */
	if (jackdet->btn_state == BUTTON_PRESS_RELEASE) {
		jackdet->btn_state = BUTTON_RELEASE;

		dev_err(dev, "Button press and release event duplicated.\n");

		jackstate->prv_btn_state = BUTTON_RELEASE;

		msleep(50);

		/* Ear-mic BST4 Reset */
		reset_mic4_boost(jackdet);

		process_button_ev(jackdet, BUTTON_RELEASE);
	}

	jack_wake_unlock(jackdet->jack_wakeup);
}

static void s5m3700x_ldet_chk_work(struct work_struct *work)
{
	struct s5m3700x_jack *jackdet =
		container_of(work, struct s5m3700x_jack, ldet_chk_work.work);
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_s5m3700x->dev;
	unsigned int decision_state = JACK_WTP_JO;

	dev_info(dev, "%s called, cur_jack_state: 0x%02x.\n",
			__func__, jackstate->cur_jack_state);

	/* Pole value decision */
	if (jackstate->cur_jack_state & JACK_WTP_DEC) {
		/* Read ldet adc value for Water Protection */
		// need to change JACK_LDET_ADC to WTP ADC
		jackstate->ldet_adc = read_adc(jackdet, ADC_WTP, JACK_WTP_LDET_ADC, 1);

		if (jackstate->ldet_adc < jackdet->adc_thd_fake_jack) {
			decision_state = JACK_WTP_CJI;
		} else if ((jackstate->ldet_adc >= jackdet->adc_thd_fake_jack)
					&& (jackstate->ldet_adc < jackdet->adc_thd_water_in)) {
			decision_state = JACK_WTP_ETC;
		} else if ((jackstate->ldet_adc >= jackdet->adc_thd_water_in)
					&& (jackstate->ldet_adc < jackdet->adc_thd_water_out)) {
			decision_state = JACK_WTP_POLL;
		} else {
			decision_state = JACK_WTP_JO;
		}
		s5m3700x_jackstate_set(jackdet, decision_state);
	} else {
		dev_info(dev, "%s called, Jack state is not JACK_WTP_DEC\n", __func__);
	}

	jack_wake_unlock(jackdet->jack_wakeup);
}
static char *s5m3700x_print_jack_type_state(int cur_jack_state)
{
	if (cur_jack_state & JACK_4POLE)
		return "4POLE";
	else if (cur_jack_state & JACK_AUX)
		return "AUX";
	else
		return "3POLE";
}
static void s5m3700x_jack_det_work(struct work_struct *work)
{
	struct s5m3700x_jack *jackdet =
		container_of(work, struct s5m3700x_jack, jack_det_work.work);
	struct earjack_state *jackstate = &jackdet->jack_state;
	struct device *dev = jackdet->p_s5m3700x->dev;
	int jack_state = JACK_4POLE;

	mutex_lock(&jackdet->mdet_lock);
	dev_info(dev, "%s called, cur_jack_state: 0x%02x.\n",
			__func__, jackstate->cur_jack_state);

	/* Pole value decision */
	if (jackstate->cur_jack_state & JACK_POLE_DEC) {
		/* Read mdet adc value for detect mic */
		jackstate->mdet_adc = read_adc(jackdet, ADC_POLE, JACK_MDET_ADC, 0);

		if (jackstate->mdet_adc < jackdet->mic_adc_range[0])
			jack_state = JACK_3POLE;
		else if ((jackstate->mdet_adc >= jackdet->mic_adc_range[0]) &&
			(jackstate->mdet_adc < jackdet->mic_adc_range[1]))
			jack_state = JACK_4POLE;
		else
			jack_state = JACK_AUX;

		/* Read ldet adc value for OMTP Jack */
		if ((jackdet->omtp_range != S5M3700X_OMTP_ADC_DEFAULT)
			&& (jack_state == JACK_4POLE)) {
			jackstate->ldet_adc = read_adc(jackdet, ADC_POLE, JACK_LDET_ADC, 1);
			if (jackstate->ldet_adc > jackdet->omtp_range)
				jack_state = JACK_OMTP;
		} else {
			jackstate->ldet_adc = -EINVAL;
		}

		/* Report jack type */
		s5m3700x_jackstate_set(jackdet, jack_state);
		if (jack_state == JACK_4POLE)
			input_report_switch(jackdet->input, SW_MICROPHONE_INSERT, 1);
		else
			input_report_switch(jackdet->input, SW_HEADPHONE_INSERT, 1);
		input_sync(jackdet->input);

		dev_info(dev, "%s ldet: %d, mdet: %d, Jack: %s, Pole: %s\n", __func__,
				jackstate->ldet_adc, jackstate->mdet_adc,
				(jackstate->cur_jack_state & JACK_OMTP) ? "OMTP" : "CTIA",
				s5m3700x_print_jack_type_state(jackstate->cur_jack_state));
	} else if (jackstate->cur_jack_state & JACK_WTP_ETC) {
		/*
		 * Raise Aux state by WTP Process
		 */
		jack_state = JACK_AUX;

		/* Report jack type */
		s5m3700x_jackstate_set(jackdet, jack_state);
		input_report_switch(jackdet->input, SW_HEADPHONE_INSERT, 1);
		input_sync(jackdet->input);

		dev_info(dev, "%s WTP decision Jack: CTIA, Pole: AUX\n", __func__);
	} else if (jackstate->cur_jack_state & JACK_IN) {
		/*
		 * Already Jack-in, Re-Check the Pole
		 */
		dev_dbg(dev, "%s called, Re-check the Pole\n", __func__);
		input_sync(jackdet->input);
	} else {
		dev_info(dev, "%s called, Jack state is not JACK_POLE_DEC\n", __func__);
		/* Send the jack out event to the audio framework */
		input_report_switch(jackdet->input, SW_HEADPHONE_INSERT, 0);
		input_report_switch(jackdet->input, SW_MICROPHONE_INSERT, 0);
		input_sync(jackdet->input);
	}

	dev_info(dev, "%s called, Jack %s, Mic %s\n", __func__,
			(jackstate->cur_jack_state & JACK_IN) ?	"inserted" : "removed",
			(jackstate->cur_jack_state & JACK_4POLE) ? "inserted" : "removed");

	mutex_unlock(&jackdet->mdet_lock);
	jack_wake_unlock(jackdet->jack_wakeup);
}

/*
 * s5m3700x_configure_mic_bias() - Configure mic bias voltage
 *
 * @jackdet: jack information struct
 *
 * Desc: Configure the mic1 and mic2 bias voltages with default value
 * or the value received from the device tree.
 * Also configure the internal LDO voltage.
 */
static void s5m3700x_configure_mic_bias(struct s5m3700x_jack *jackdet)
{
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;

	/* Configure Mic1 Bias Voltage */
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_C8_ACTR_MCB4,
			CTRV_MCB1_MASK, jackdet->mic_bias1_voltage << CTRV_MCB1_SHIFT);

	/* Configure Mic2 Bias Voltage */
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_C9_ACTR_MCB5,
			CTRV_MCB2_MASK, jackdet->mic_bias2_voltage << CTRV_MCB2_SHIFT);

	/* Configure Mic3 Bias Voltage */
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_C9_ACTR_MCB5,
			CTRV_MCB3_MASK, jackdet->mic_bias3_voltage << CTRV_MCB3_SHIFT);
}

/*
 * s5m3700x_jack_parse_dt() - Parsing device tree options about jack
 *
 * @s5m3700x: codec information struct
 *
 * Desc: Initialize jack_det struct variable as parsing device tree options.
 * Customer can tune this options. If not set by customer,
 * it use default value below defined.
 */

static void s5m3700x_jack_parse_dt(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	struct device *dev = s5m3700x->dev;
	struct of_phandle_args args;
	unsigned int tc, delay, mic_range;
	unsigned int bias_v_conf, btn_rel_val;
	int thd_adc;
	int ret, i;

	jackdet->irqb_gpio = of_get_named_gpio(dev->of_node, "s5m3700x-codec-int", 0);
	if (jackdet->irqb_gpio < 0)
		dev_err(dev, "%s cannot find irqb gpio in the dt\n", __func__);
	else
		dev_info(dev, "%s: irqb gpio = %d\n",
				__func__, jackdet->irqb_gpio);

	/*
	 * Setting ADC measurement value in test mode (TC, Delay).
	 */
	ret = of_property_read_u32(dev->of_node, "t-pole-cnt", &tc);
	if (!ret)
		jackdet->t_pole_cnt = tc;
	else
		jackdet->t_pole_cnt = 5;

	ret = of_property_read_u32(dev->of_node, "t-btn-cnt", &tc);
	if (!ret)
		jackdet->t_btn_cnt = tc;
	else
		jackdet->t_btn_cnt = 4;

	ret = of_property_read_u32(dev->of_node, "t-pole-delay", &delay);
	if (!ret)
		jackdet->t_pole_delay = delay;
	else
		jackdet->t_pole_delay = 10;

	ret = of_property_read_u32(dev->of_node, "t-btn-delay", &delay);
	if (!ret)
		jackdet->t_btn_delay = delay;
	else
		jackdet->t_btn_delay = 4;

	dev_info(dev, "TM ADC cnt: %d %d, delay: %d %d\n",
			jackdet->t_pole_cnt, jackdet->t_btn_cnt,
			jackdet->t_pole_delay, jackdet->t_btn_delay);

	/*
	 * Setting ADC detecting range.
	 */
	/* Mic det adc range */
	ret = of_parse_phandle_with_args(dev->of_node, "mic-adc-range", "#list-imp-cells", 0, &args);
	if (!ret) {
		for (i = 0; i < 2; i++)
			jackdet->mic_adc_range[i] = args.args[i];
	} else {
		jackdet->mic_adc_range[0] = S5M3700X_MIC_ADC_RANGE_0;
		jackdet->mic_adc_range[1] = S5M3700X_MIC_ADC_RANGE_1;
	}

	/* Mdet check delay */
	ret = of_property_read_u32(dev->of_node, "mdet-chk-delay", &delay);
	if (!ret)
		jackdet->mdet_chk_delay = delay;
	else
		jackdet->mdet_chk_delay = S5M3700X_MDET_CHK_DEFAULT;

	/* Ldet check delay */
	ret = of_property_read_u32(dev->of_node, "ldet-chk-delay", &delay);
	if (!ret)
		jackdet->ldet_chk_delay = delay;
	else
		jackdet->ldet_chk_delay = S5M3700X_LDET_CHK_DEFAULT;

	/* OMTP det adc range */
	ret = of_property_read_u32(dev->of_node, "omtp-adc-range", &mic_range);
	if (!ret)
		jackdet->omtp_range = mic_range;
	else
		jackdet->omtp_range = S5M3700X_OMTP_ADC_DEFAULT;

	dev_info(dev, "mic: %d %d, omtp: %d, mdet chk: %d\n",
			jackdet->mic_adc_range[0], jackdet->mic_adc_range[1], jackdet->omtp_range,
			jackdet->mdet_chk_delay);

	/* Jack Imp adc range */
	ret = of_parse_phandle_with_args(dev->of_node, "jack-imp-range", "#list-imp-cells", 0, &args);
	if (!ret) {
		for (i = 0; i < 5; i++)
			jackdet->jack_imp_range[i] = args.args[i];
	} else {
		jackdet->jack_imp_range[0] = S5M3700X_JACK_IMP_RANGE_0;
		jackdet->jack_imp_range[1] = S5M3700X_JACK_IMP_RANGE_1;
		jackdet->jack_imp_range[2] = S5M3700X_JACK_IMP_RANGE_2;
		jackdet->jack_imp_range[3] = S5M3700X_JACK_IMP_RANGE_3;
		jackdet->jack_imp_range[4] = S5M3700X_JACK_IMP_RANGE_4;
	}

	dev_info(dev, "jack imp range: %d %d %d %d %d.\n",
			jackdet->jack_imp_range[0],
			jackdet->jack_imp_range[1],
			jackdet->jack_imp_range[2],
			jackdet->jack_imp_range[3],
			jackdet->jack_imp_range[4]);

	/*
	 * Set mic bias 1/2/3 voltages
	 */
	/* Mic Bias 1 */
	ret = of_property_read_u32(dev->of_node, "mic-bias1-voltage", &bias_v_conf);
	if (!ret &&
			((bias_v_conf >= MICBIAS_VO_1_5V) &&
			 (bias_v_conf <= MICBIAS_VO_2_8V))) {
		jackdet->mic_bias1_voltage = bias_v_conf;
	} else {
		jackdet->mic_bias1_voltage = MICBIAS_VO_2_8V;
		dev_warn(dev, "Property 'mic-bias1-voltage' %s",
				ret ? "not found, default set 2.8V" : "used invalid value");
	}

	/* Mic Bias 2 */
	ret = of_property_read_u32(dev->of_node, "mic-bias2-voltage", &bias_v_conf);
	if (!ret &&
			((bias_v_conf >= MICBIAS_VO_1_5V) &&
			 (bias_v_conf <= MICBIAS_VO_2_8V))) {
		jackdet->mic_bias2_voltage = bias_v_conf;
	} else {
		jackdet->mic_bias2_voltage = MICBIAS_VO_2_8V;
		dev_warn(dev, "Property 'mic-bias2-voltage' %s",
				ret ? "not found, default set 2.8V" : "used invalid value");
	}

	/* Mic Bias 3 */
	ret = of_property_read_u32(dev->of_node, "mic-bias3-voltage", &bias_v_conf);
	if (!ret &&
			((bias_v_conf >= MICBIAS_VO_1_5V) &&
			 (bias_v_conf <= MICBIAS_VO_2_8V))) {
		jackdet->mic_bias3_voltage = bias_v_conf;
	} else {
		jackdet->mic_bias3_voltage = MICBIAS_VO_2_8V;
		dev_warn(dev, "Property 'mic-bias3-voltage' %s",
				ret ? "not found, default set 2.8V" : "used invalid value");
	}

	dev_info(dev, "Bias voltage values: bias1=%d, bias2=%d, bias3=%d\n",
			jackdet->mic_bias1_voltage,
			jackdet->mic_bias2_voltage,
			jackdet->mic_bias3_voltage);

	/*
	 * Setting delay time for adc check
	 */
	/* Mic bias on delay time */
	ret = of_property_read_u32(dev->of_node, "mic-det-delay", &delay);
	if (!ret)
		jackdet->mdet_delay = delay;
	else
		jackdet->mdet_delay = S5M3700X_MDET_DELAY;

	/* Button adc check delay time */
	ret = of_property_read_u32(dev->of_node, "btn-adc-delay", &delay);
	if (!ret)
		jackdet->btn_adc_delay = delay;
	else
		jackdet->btn_adc_delay = S5M3700X_BTN_ADC_DELAY;

	dev_info(dev, "ADC check delay, mic: %d, btn: %d\n",
			jackdet->mdet_delay, jackdet->btn_adc_delay);

	/*
	 * Set button adc tuning values
	 */
	/* Button release adc value */
	ret = of_property_read_u32(dev->of_node, "btn-release-value", &btn_rel_val);
	if (!ret)
		jackdet->btn_release_value = btn_rel_val;
	else
		jackdet->btn_release_value = S5M3700X_BTN_REL_DEFAULT;

	/* Button press adc value, a maximum of 4 buttons are supported */
	for (i = 0; i < 4; i++) {
		if (of_parse_phandle_with_args(dev->of_node,
					"but-zones-list", "#list-but-cells", i, &args))
			break;
		jackdet->jack_buttons_zones[i].code = args.args[0];
		jackdet->jack_buttons_zones[i].adc_low = args.args[1];
		jackdet->jack_buttons_zones[i].adc_high = args.args[2];
	}

	for (i = 0; i < 4; i++)
		dev_info(dev, "Button Press: code(%d), low(%d), high(%d)\n",
				jackdet->jack_buttons_zones[i].code,
				jackdet->jack_buttons_zones[i].adc_low,
				jackdet->jack_buttons_zones[i].adc_high);
	dev_info(dev, "Button Release: %d\n", jackdet->btn_release_value);

	/* Button debounce value */
	ret = of_property_read_u32(dev->of_node, "btn-dbnc-value", &btn_rel_val);
	if (!ret)
		jackdet->btn_dbnc_value = btn_rel_val;
	else
		jackdet->btn_dbnc_value = S5M3700X_BTN_DBNC_DEFAULT;

	/* Set ldet adc threshold value */
	ret = of_property_read_u32(dev->of_node, "adc-thd-fake-jack", &thd_adc);
	if (!ret)
		jackdet->adc_thd_fake_jack = thd_adc;
	else
		jackdet->adc_thd_fake_jack = S5M3700X_ADC_THD_FAKE_JACK;

	ret = of_property_read_u32(dev->of_node, "adc-thd-water-in", &thd_adc);
	if (!ret)
		jackdet->adc_thd_water_in = thd_adc;
	else
		jackdet->adc_thd_water_in = S5M3700X_ADC_THD_WATER_IN;

	ret = of_property_read_u32(dev->of_node, "adc-thd-water-out", &thd_adc);
	if (!ret)
		jackdet->adc_thd_water_out = thd_adc;
	else
		jackdet->adc_thd_water_out = S5M3700X_ADC_THD_WATER_OUT;

	dev_info(dev, "LDET adc threshold value: %d %d %d\n",
			jackdet->adc_thd_fake_jack,
			jackdet->adc_thd_water_in,
			jackdet->adc_thd_water_out);
}

static void s5m3700x_hp_trim_initialize(struct s5m3700x_priv *s5m3700x)
{
	u8 temp[50];
	unsigned int i = 0;

	dev_info(s5m3700x->dev, "%s called, setting hp trim values.\n", __func__);

	for (i = 0; i < 50; i++) {
		/* Reading HP Normal Trim */
		s5m3700x_i2c_read_reg(S5M3700X_OTP_ADDR, i, &temp[i]);
		s5m3700x->hp_normal_trim[i] = (unsigned int)temp[i];

		/* Setting HP UHQA Trim */
		s5m3700x->hp_uhqa_trim[i] = ((s5m3700x->hp_normal_trim[i] * 777) + 500) / 1000;
	}
}



static void s5m3700x_jack_register_initialize(struct s5m3700x_priv *s5m3700x)
{
	dev_info(s5m3700x->dev, "%s called, setting defaults\n", __func__);

	/* PDB JD CLK Enable */
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D0_DCTR_CM, 0x01);

	/* PDB JD Comparator On */
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_C0_ACTR_JD1, 0x03);

	/* ANT LDET VTH Setting */
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_C3_ACTR_JD4,
			CTRV_ANT_LDET_VTH_MASK, ANT_LDET_VTH_0 << CTRV_ANT_LDET_VTH_SHIFT);

	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D6_DCTR_TEST6, 0x01);

	/* Tuning Jack DBNC Value */
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D8_DCTR_DBNC1,
			JACK_DBNC_OUT_MASK, 0x02);
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_D9_DCTR_DBNC2,
			JACK_DBNC_INT_OUT_MASK, 0x02);

	/* ANT MDET OUT Debounce Time */
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_DB_DCTR_DBNC4,
			ANT_MDET_DBNC_OUT_MASK, 0x02);

	/* IMP RAMP */
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E9_DCTR_IMP3, 0x82);

	/* ODSEL5 */
	s5m3700x_update_bits(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_B4_ODSEL5,
			EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK, EN_DAC1_OUTTIE_MASK|EN_DAC2_OUTTIE_MASK);

	/* HP PULLDN */
	s5m3700x_update_bits(s5m3700x, S5M3700X_ANALOG_ADDR, S5M3700X_13A_POP_HP,
			EN_HP_PDN_MASK, 0);

	/* LDET ADC Range Setting */
	s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2E0_JACK_OTP10, 0x4D);
	s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2E1_JACK_OTP11, 0x60);
	s5m3700x_write(s5m3700x, S5M3700X_OTP_ADDR, S5M3700X_2E2_JACK_OTP12, 0x77);

	/* IRQ Un-masking */
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_08_IRQ1M, 0x30);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_09_IRQ2M, 0x03);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_0A_IRQ3M, 0xEE);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_0B_IRQ4M, 0xEE);

	/* 5-Pin Setting */
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_E0_DCTR_FSM1, 0x0A);
}

static void s5m3700x_jack_register_exit(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);

	/* IRQ Masking */
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_08_IRQ1M, 0xFF);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_09_IRQ2M, 0xFF);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_0A_IRQ3M, 0xFF);
	s5m3700x_write(s5m3700x, S5M3700X_DIGITAL_ADDR, S5M3700X_0B_IRQ4M, 0xFF);
}

static int return_irq_type(struct s5m3700x_jack *jackdet)
{
	unsigned int pend1, pend2, pend3, pend4, pend5, pend6, stat1, stat2;

	pend1 = jackdet->irq_val[0];
	pend2 = jackdet->irq_val[1];
	pend3 = jackdet->irq_val[2];
	pend4 = jackdet->irq_val[3];
	pend5 = jackdet->irq_val[4];
	pend6 = jackdet->irq_val[5];
	stat1 = jackdet->irq_val[6];
	stat2 = jackdet->irq_val[7];

	if (pend1 & ST_JO_R)
		return IRQ_ST_JACKOUT;
	else if (pend1 & ST_C_JI_R)
		return IRQ_ST_CMP_JACK_IN;
	else if (pend1 & ST_DEC_WTP_R)
		return IRQ_ST_WTJACK_DEC;
	else if (pend1 & ST_WT_JI_R)
		return IRQ_ST_WTJACK_IN;
	else if (pend1 & ST_WT_JO_R)
		return IRQ_ST_WTJACK_OUT;
	else if (pend1 & ST_IMP_CHK_DONE_R)
		return IRQ_ST_IMP_CHK;
	else if (pend2 & ST_DEC_POLE_R)
		return IRQ_ST_JACKDET;
	else if (pend2 & ST_ETC_R)
		return IRQ_ST_WTJACK_ETC;
	else if (pend3 & BTN_DET_R)
		return IRQ_ST_BTN_DET_R;
	else if (pend4 & BTN_DET_F)
		return IRQ_ST_BTN_DET_F;
	else
		return IRQ_ST_ERR;
}

static int s5m3700x_earjack_handler(struct s5m3700x_priv *s5m3700x)
{
	struct device *dev = s5m3700x->dev;
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;
	struct earjack_state *jackstate = &jackdet->jack_state;
	int irq_type = return_irq_type(jackdet);

	mutex_lock(&jackdet->key_lock);

	switch (irq_type) {
	case IRQ_ST_JACKOUT:
		dev_info(dev, "[IRQ] %s Jack out interrupt, line: %d\n",
				__func__, __LINE__);
		s5m3700x_jackstate_set(jackdet, JACK_OUT);

		cancel_delayed_work(&jackdet->ldet_chk_work);
		cancel_delayed_work(&jackdet->jack_det_work);
		queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
				msecs_to_jiffies(0));

		queue_delayed_work(jackdet->buttons_release_wq, &jackdet->buttons_work,
				msecs_to_jiffies(jackdet->btn_adc_delay));
		break;
	case IRQ_ST_CMP_JACK_IN:
		dev_info(dev, "[IRQ] %s Completely Jack in interrupt, line: %d\n",
				__func__, __LINE__);
		s5m3700x_jackstate_set(jackdet, JACK_CMP);
		break;
	case IRQ_ST_WTJACK_DEC:
		if (jackstate->cur_jack_state == JACK_WTP_DEC) {
			dev_info(dev, "[IRQ] %s IRQ_ST_WTJACK_DEC is already on going : %d\n",
				__func__, __LINE__);
			break;
		}
		dev_info(dev, "[IRQ] %s Water Protection Decision interrupt, line: %d\n",
				__func__, __LINE__);
		s5m3700x_jackstate_set(jackdet, JACK_WTP_DEC);
		/* lock for jack and button irq */
		jack_wake_lock(jackdet->jack_wakeup);

		/* run ldet adc workqueue */
		cancel_delayed_work(&jackdet->ldet_chk_work);
		queue_delayed_work(jackdet->ldet_chk_wq, &jackdet->ldet_chk_work,
				msecs_to_jiffies(jackdet->ldet_chk_delay));
		break;
	case IRQ_ST_WTJACK_IN:
	case IRQ_ST_WTJACK_OUT:
		if (irq_type == IRQ_ST_WTJACK_IN)
			dev_info(dev, "[IRQ] %s IRQ_ST_WTJACK_IN, line: %d\n",
					__func__, __LINE__);
		else
			dev_info(dev, "[IRQ] %s IRQ_ST_WTJACK_OUT, line: %d\n",
					__func__, __LINE__);
		s5m3700x_jackstate_set(jackdet, JACK_WTP_JIO);
		break;
	case IRQ_ST_IMP_CHK:
		dev_info(dev, "[IRQ] %s IMP interrupt, line: %d\n",
				__func__, __LINE__);
		s5m3700x_jackstate_set(jackdet, JACK_IMP);
		break;
	case IRQ_ST_JACKDET:
		dev_info(dev, "[IRQ] %s Jack det interrupt, line: %d\n",
				__func__, __LINE__);

		if (jackdet->earjack_re_read == 1) {
			jackdet->earjack_re_read = 0;
			break;
		}

		s5m3700x_jackstate_set(jackdet, JACK_POLE_DEC);

		/* lock for jack and button irq */
		jack_wake_lock(jackdet->jack_wakeup);

		cancel_delayed_work(&jackdet->jack_det_work);
		queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
				msecs_to_jiffies(jackdet->mdet_delay));
		break;
	case IRQ_ST_WTJACK_ETC:
		dev_info(dev, "[IRQ] %s State WTP Aux interrupt, line: %d\n",
				__func__, __LINE__);

		/* lock for jack and button irq */
		jack_wake_lock(jackdet->jack_wakeup);

		cancel_delayed_work(&jackdet->jack_det_work);
		queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
				msecs_to_jiffies(jackdet->mdet_delay));
		break;
	case IRQ_ST_BTN_DET_R:
		dev_info(dev, "[IRQ] %s Button Press interrupt, line: %d\n",
				__func__, __LINE__);
		/* lock for jack and button irq */
		jack_wake_lock(jackdet->jack_wakeup);

		jackstate->cur_btn_state = BUTTON_PRESS;
		queue_delayed_work(jackdet->buttons_press_wq, &jackdet->buttons_work,
				msecs_to_jiffies(jackdet->btn_adc_delay));
		break;
	case IRQ_ST_BTN_DET_F:
		dev_info(dev, "[IRQ] %s Button Release interrupt, line: %d\n",
				__func__, __LINE__);
		/* lock for jack and button irq */
		jack_wake_lock(jackdet->jack_wakeup);

		jackstate->cur_btn_state = BUTTON_RELEASE;
		queue_delayed_work(jackdet->buttons_release_wq, &jackdet->buttons_work,
				msecs_to_jiffies(jackdet->btn_adc_delay));
		break;
	default:
		dev_info(dev, "[IRQ] %s IRQ return type skip, line %d\n",
				__func__, __LINE__);
		break;
	}

	mutex_unlock(&jackdet->key_lock);
	return IRQ_HANDLED;
}

/*
 * s5m3700x_notifier_handler() - Codec IRQ Handler
 *
 * Desc: Set codec register according to codec IRQ.
 */
static int s5m3700x_notifier_handler(struct notifier_block *nb,
		unsigned long insert, void *data)
{
	struct s5m3700x_notifier_struct *ns = data;
	struct s5m3700x_priv *s5m3700x = ns->s5m3700x;

	dev_err(s5m3700x->dev, "%s called. insert: %d\n", __func__, insert);

	s5m3700x_earjack_handler(s5m3700x);

	return IRQ_HANDLED;
}

static irqreturn_t s5m3700x_irq_thread(int irq, void *irq_data)
{
	struct s5m3700x_jack *jackdet = irq_data;
	struct s5m3700x_priv *s5m3700x = jackdet->p_s5m3700x;

	s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_01_IRQ1, &jackdet->irq_val[0]);
	s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_02_IRQ2, &jackdet->irq_val[1]);
	s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_03_IRQ3, &jackdet->irq_val[2]);
	s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_04_IRQ4, &jackdet->irq_val[3]);
	s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_05_IRQ5, &jackdet->irq_val[4]);
	s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_06_IRQ6, &jackdet->irq_val[5]);
	s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_F0_STATUS1, &jackdet->irq_val[6]);
	s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_F1_STATUS2, &jackdet->irq_val[7]);
	s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_F2_STATUS3, &jackdet->irq_val[8]);
	s5m3700x_i2c_read_reg(S5M3700X_DIGITAL_ADDR, S5M3700X_FA_STATUS11, &jackdet->irq_val[9]);

	dev_info(s5m3700x->dev,
			"[IRQ] %s(%d) 0x1:%02x 0x2:%02x 0x3:%02x 0x4:%02x 0x5:%02x 0x6:%02x st1:%02x st2:%02x st3:%02x st4:%02x\n",
			__func__, __LINE__, jackdet->irq_val[0], jackdet->irq_val[1],
			jackdet->irq_val[2], jackdet->irq_val[3], jackdet->irq_val[4],
			jackdet->irq_val[5], jackdet->irq_val[6], jackdet->irq_val[7], jackdet->irq_val[8], jackdet->irq_val[9]);

	blocking_notifier_call_chain(&s5m3700x_notifier, 0, &s5m3700x_notifier_t);
	return IRQ_HANDLED;
}

/*
 * s5m3700x_register_inputdev() - Register button input device
 *
 * @jackdet: jack information struct
 *
 * Desc: Register button input device
 */
static int s5m3700x_register_inputdev(struct s5m3700x_jack *jackdet)
{
	struct device *dev = jackdet->p_s5m3700x->dev;
	int i, ret;

	/* Register Headset Device */
	jackdet->input = devm_input_allocate_device(dev);
	if (!jackdet->input) {
		dev_err(dev, "Failed to allocate switch input device\n");
		return -ENOMEM;
	}

	jackdet->input->name = "S5M3700X Headset Input";
	jackdet->input->phys = dev_name(dev);
	jackdet->input->id.bustype = BUS_I2C;

	ret = input_register_device(jackdet->input);
	if (ret != 0) {
		jackdet->input = NULL;
		dev_err(dev, "Failed to register switch input device\n");
	}

	/*
	 * input_set_capability (dev, type, code)
	 * @dev : input device
	 * @type : event type (EV_KEY, EV_SW, etc...)
	 * @code : event code (4POLE, 3POLE, LINE...)
	 */
	/* 3-Pole event */
	input_set_capability(jackdet->input, EV_SW, SW_HEADPHONE_INSERT);
	/* 4-Pole event */
	input_set_capability(jackdet->input, EV_SW, SW_MICROPHONE_INSERT);
	/* Button event */
	for (i = 0; i < 4; i++)
		input_set_capability(jackdet->input, EV_KEY,
				jackdet->jack_buttons_zones[i].code);

	return 0;
}

/*
 * s5m3700x_jack_probe() - Initialize variable related jack
 *
 * @codec: SoC audio codec device
 *
 * Desc: This function is called by s5m3700x_component_probe. For separate codec
 * and jack code, this function called from codec driver. This function initialize
 * jack variable, workqueue, mutex, and wakelock.
 */

int s5m3700x_jack_probe(struct s5m3700x_priv *s5m3700x)
{
	struct s5m3700x_jack *jackdet;
	struct earjack_state *jackstate;
	int ret;

	dev_info(s5m3700x->dev, "Codec Jack Probe: (%s)\n", __func__);

	jackdet = kzalloc(sizeof(struct s5m3700x_jack), GFP_KERNEL);
	if (jackdet == NULL)
		return -ENOMEM;

	jackdet->codec = s5m3700x->codec;
	jackdet->p_s5m3700x = s5m3700x;
	s5m3700x->p_jackdet = jackdet;
	jackstate = &jackdet->jack_state;

	/* Initialize workqueue */
	/* Initiallize workqueue for jack detect handling */
	INIT_DELAYED_WORK(&jackdet->jack_det_work, s5m3700x_jack_det_work);
	jackdet->jack_det_wq = create_singlethread_workqueue("jack_det_wq");
	if (jackdet->jack_det_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create jack_det_wq\n");
		return -ENOMEM;
	}

	/* Initialize workqueue for button handling */
	INIT_DELAYED_WORK(&jackdet->buttons_work, s5m3700x_buttons_work);
	jackdet->buttons_press_wq = create_singlethread_workqueue("buttons_press_wq");
	if (jackdet->buttons_press_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create buttons_press_wq\n");
		return -ENOMEM;
	}
	jackdet->buttons_release_wq = create_singlethread_workqueue("buttons_release_wq");
	if (jackdet->buttons_release_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create buttons_release_wq\n");
		return -ENOMEM;
	}

	/* Initialize workqueue for ldet detect handling */
	INIT_DELAYED_WORK(&jackdet->ldet_chk_work, s5m3700x_ldet_chk_work);
	jackdet->ldet_chk_wq = create_singlethread_workqueue("ldet_chk_wq");
	if (jackdet->ldet_chk_wq == NULL) {
		dev_err(s5m3700x->dev, "Failed to create ldet_chk_wq\n");
		return -ENOMEM;
	}

	/* Initialize mutex lock */
	mutex_init(&jackdet->key_lock);
	mutex_init(&jackdet->mdet_lock);
	mutex_init(&jackdet->adc_lock);
	if (jack_set_wake_lock(jackdet) < 0) {
		pr_err("%s: jack_set_wake_lock fail\n", __func__);
		goto err_wake_lock;
	}

	/* Device Tree for jack */
	s5m3700x_jack_parse_dt(s5m3700x);

	/* Register input device */
	s5m3700x_register_inputdev(jackdet);

	/* Jack Notifier initialize */
	s5m3700x_notifier_block.notifier_call = s5m3700x_notifier_handler,
		s5m3700x_register_notifier(&s5m3700x_notifier_block, s5m3700x);

	/* Request Jack IRQ */
	if (jackdet->irqb_gpio > 0) {
		jackdet->codec_irq = gpio_to_irq(jackdet->irqb_gpio);
		dev_info(s5m3700x->dev, "%s: codec_irq = %d\n", __func__, jackdet->codec_irq);
		if (jackdet->codec_irq > 0) {
			ret = request_threaded_irq(jackdet->codec_irq,
					NULL, s5m3700x_irq_thread,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					"s5m3700x-irq", jackdet);
			if (ret)
				dev_err(s5m3700x->dev, "%s Failed to Request IRQ. ret: %d\n", __func__, ret);
			else {
				dev_info(s5m3700x->dev, "%s Request IRQ. ret: %d\n", __func__, ret);

				ret = enable_irq_wake(jackdet->codec_irq);
				if (ret < 0) {
					dev_err(s5m3700x->dev, "%s Failed to Enable Wakeup Source(%d)\n", __func__, ret);
					goto err_wake_irq;
				}
			}
		} else
			dev_err(s5m3700x->dev, "%s Failed gpio_to_irq. ret: %d\n", __func__, jackdet->codec_irq);
	}

	/* Initialize jack state variable */
	jackdet->earjack_re_read = 0;
	jackdet->btn_state = 0;
	jackstate->ldet_adc = -EINVAL;
	jackstate->mdet_adc = -EINVAL;
	jackstate->prv_btn_state = BUTTON_RELEASE;
	jackstate->cur_btn_state = BUTTON_RELEASE;
	jackstate->button_code = -EINVAL;
	jackstate->btn_adc = -EINVAL;

#if IS_ENABLED(CONFIG_PM)
	pm_runtime_get_sync(s5m3700x->dev);
#else
	s5m3700x_enable(s5m3700x->dev);
#endif

	/* register value init for jack */
	jackstate->prv_jack_state = JACK_OUT;
	jackstate->cur_jack_state = JACK_OUT;

	s5m3700x_jack_register_initialize(s5m3700x);

	if (s5m3700x->codec_ver >= REV_0_3)
		s5m3700x_hp_trim_initialize(s5m3700x);

	/* Configure mic bias voltage */
	s5m3700x_configure_mic_bias(jackdet);

#if IS_ENABLED(CONFIG_PM)
	pm_runtime_put_sync(s5m3700x->dev);
#else
	s5m3700x_disable(s5m3700x->dev);
#endif

	/* Jack OTP Read */
	s5m3700x_read_imp_otp(jackdet);

	return true;
err_wake_irq:
	disable_irq_wake(jackdet->codec_irq);
err_wake_lock:
	wakeup_source_unregister(jackdet->jack_wakeup);
	return false;
}

/*
 * s5m3700x_jack_remove() - Remove variable related jack
 *
 * @codec: SoC audio codec device
 *
 * Desc: This function is called by s5m3700x_component_remove. For separate codec
 * and jack code, this function called from codec driver. This function destroy
 * workqueue.
 */
int s5m3700x_jack_remove(struct snd_soc_component *codec)
{
	struct s5m3700x_priv *s5m3700x = snd_soc_component_get_drvdata(codec);
	struct s5m3700x_jack *jackdet = s5m3700x->p_jackdet;

	dev_info(codec->dev, "(*) %s called\n", __func__);

	destroy_workqueue(jackdet->jack_det_wq);
	destroy_workqueue(jackdet->buttons_press_wq);
	destroy_workqueue(jackdet->buttons_release_wq);
	destroy_workqueue(jackdet->ldet_chk_wq);

	s5m3700x_jack_register_exit(codec);

	/* Unregister ADC pin */
	kfree(jackdet);

	return true;
}
