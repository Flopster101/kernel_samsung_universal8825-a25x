/*
 * sound/soc/codec/s5m3700x.h
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S5M3700X_H
#define _S5M3700X_H

#include <linux/completion.h>
#include <sound/soc.h>
#include <linux/iio/consumer.h>

#define REV_0_0							0
#define REV_0_1							1
#define REV_0_2							2
#define REV_0_3							3

#define S5M3700X_DIGITAL_ADDR			0x00
#define S5M3700X_ANALOG_ADDR			0X01
#define S5M3700X_OTP_ADDR				0x02
#define S5M3700X_MV_ADDR				0x03

#define S5M3700X_REGCACHE_SYNC_START	0x00
#define S5M3700X_REGCACHE_SYNC_END		0xFF

#define S5M3700X_SAMPLE_RATE_48KHZ		48000
#define S5M3700X_SAMPLE_RATE_192KHZ		192000
#define S5M3700X_SAMPLE_RATE_384KHZ		384000

#define BIT_RATE_16						16
#define BIT_RATE_24						24
#define BIT_RATE_32						32

#define CHANNEL_2						2
#define CHANNEL_4						4

struct s5m3700x_jack;

enum s5m3700x_type {
	CODEC_CLOSE = -1,
	S5M3700D = 0,
	S5M3700A,
	S5M3700O,
};

enum s5m3700x_vts_type {
	VTS_OFF = 0,
	VTS1_ON = 1,
	VTS2_ON = 2,
	VTS_DUAL = 3,
};

enum s5m3700x_vts_dmic_type {
	VTS_DMIC_OFF = 0,
	VTS_DMIC1_ON = 1,
	VTS_DMIC2_ON = 2,
};

enum s5m3700x_hifi_type {
	HIFI_OFF = 0,
	HIFI_ON = 1,
};

enum s5m3700x_hp_imp {
	HP_IMP_32 = 0,
	HP_IMP_6 = 1,
};

enum s5m3700x_rcv_imp {
	RCV_IMP_32 = 0,
	RCV_IMP_12 = 1,
	RCV_IMP_6 = 2,
};

/* Jack type */
enum {
	JACK = 0,
	USB_JACK,
};

struct s5m3700x_priv {
	/* codec driver default */
	struct snd_soc_component *codec;
	struct s5m3700x_jack *p_jackdet;
	struct device *dev;
	struct i2c_client *i2c_priv[3];
	struct regmap *regmap[3];
	/* regulator */
	struct regulator *vdd;
	struct regulator *vdd2;
	int regulator_count;
	/* codec mutex */
	struct mutex i2c_mutex;
	struct mutex adc_mute_lock;
	struct mutex dacl_mute_lock;
	struct mutex dacr_mute_lock;
	struct mutex regcache_lock;
	/* codec workqueue */
	struct delayed_work adc_mute_work;
	struct workqueue_struct *adc_mute_wq;
	/* codec debug */
	bool codec_debug;
	bool tx_dapm_done;
	bool rx_dapm_done;
	/* saved information */
	u8 codec_ver;
	unsigned int codec_sw_ver;
	unsigned int hp_normal_trim[50];
	unsigned int hp_uhqa_trim[50];
	bool is_suspend;
	bool pm_suspend;
	bool is_probe_done;
	bool playback_on;
	bool capture_on;
	bool vts1_on;
	bool vts2_on;
	int vts_dmic_on;
	bool hifi_on;
	unsigned int playback_aifrate;
	unsigned int capture_aifrate;
	int regcache_count;
	int resetb_gpio;
	int external_ldo;
	int hp_avc_gain;
	int rcv_avc_gain;
	int dmic_delay;
	int amic_delay;
	unsigned int hp_imp;
	unsigned int rcv_imp;
	int hp_bias_cnt;
	int hp_bias_mix_cnt;
	unsigned int jack_type;
};

/* Forward Declarations */
/* Main Functions */
int s5m3700x_i2c_read_reg(u8 slave, u8 reg, u8 *val);
int s5m3700x_i2c_write_reg(u8 slave, u8 reg, u8 val);
int s5m3700x_i2c_update_reg(u8 slave, u8 reg, u8 mask, u8 val);
int s5m3700x_write(struct s5m3700x_priv *s5m3700x,
		unsigned int slave, unsigned int addr, unsigned int value);
int s5m3700x_update_bits(struct s5m3700x_priv *s5m3700x,
		unsigned int slave, unsigned int addr, unsigned int mask, unsigned int value);
int s5m3700x_read(struct s5m3700x_priv *s5m3700x,
		unsigned int slave, unsigned int addr, unsigned int *value);
int s5m3700x_disable(struct device *dev);
int s5m3700x_enable(struct device *dev);
void regcache_cache_switch(struct s5m3700x_priv *s5m3700x, bool on);
void s5m3700x_usleep(unsigned int u_sec);
void s5m3700x_adc_digital_mute(struct s5m3700x_priv *s5m3700x, unsigned int channel, bool on);
bool s5m3700x_is_probe_done(void);

/* Jack Functions */
int s5m3700x_jack_probe(struct s5m3700x_priv *s5m3700x);
int s5m3700x_jack_remove(struct snd_soc_component *codec);
int s5m3700x_get_usbimp(void);

/*
 * Helper macros for creating bitmasks
 */
#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)

/*
 * Register Map
 */

/* IRQ and Status */

#define S5M3700X_00_BASE_REG					0x00
#define S5M3700X_01_IRQ1						0x01
#define S5M3700X_02_IRQ2						0x02
#define S5M3700X_03_IRQ3						0x03
#define S5M3700X_04_IRQ4						0x04
#define S5M3700X_05_IRQ5						0x05
#define S5M3700X_06_IRQ6						0x06

#define S5M3700X_08_IRQ1M						0x08
#define S5M3700X_09_IRQ2M						0x09
#define S5M3700X_0A_IRQ3M						0x0A
#define S5M3700X_0B_IRQ4M						0x0B
#define S5M3700X_0C_IRQ5M						0x0C
#define S5M3700X_0D_IRQ6M						0x0D

/* Clock and Reset */
#define S5M3700X_10_CLKGATE0					0x10
#define S5M3700X_11_CLKGATE1					0x11
#define S5M3700X_12_CLKGATE2					0x12
#define S5M3700X_14_RESETB0						0x14
#define S5M3700X_15_RESETB1						0x15
#define S5M3700X_16_CLK_MODE_SEL1				0x16
#define S5M3700X_17_CLK_MODE_SEL2				0x17
#define S5M3700X_18_PWAUTO_AD					0x18
#define S5M3700X_19_PWAUTO_DA					0x19

/* Reserved for SW Operation */
#define S5M3700X_1E_CHOP1				0x1E
#define S5M3700X_1F_CHOP2				0x1F

/* Digital Audio Interface */
#define S5M3700X_20_IF_FORM1					0x20
#define S5M3700X_21_IF_FORM2					0x21
#define S5M3700X_22_IF_FORM3					0x22
#define S5M3700X_23_IF_FORM4					0x23
#define S5M3700X_24_IF_FORM5					0x24
#define S5M3700X_29_IF_FORM6					0x29
#define S5M3700X_2F_DMIC_ST						0x2F

/* Recording path Digital Setting */
#define S5M3700X_30_ADC1						0x30
#define S5M3700X_31_ADC2						0x31
#define S5M3700X_32_ADC3						0x32
#define S5M3700X_33_ADC4						0x33
#define S5M3700X_34_AD_VOLL						0x34
#define S5M3700X_35_AD_VOLR						0x35
#define S5M3700X_36_AD_VOLC						0x36
#define S5M3700X_37_AD_HPF						0x37
#define S5M3700X_38_AD_TRIM1					0x38
#define S5M3700X_39_AD_TRIM2					0x39
#define S5M3700X_3A_AD_VOL						0x3A

/* Playback path Digital Setting */
#define S5M3700X_40_PLAY_MODE					0x40
#define S5M3700X_41_PLAY_VOLL					0x41
#define S5M3700X_42_PLAY_VOLR					0x42
#define S5M3700X_44_PLAY_MIX0					0x44
#define S5M3700X_45_PLAY_MIX1					0x45
#define S5M3700X_46_PLAY_MIX2					0x46
#define S5M3700X_47_TRIM_DAC0					0x47
#define S5M3700X_48_TRIM_DAC1					0x48
#define S5M3700X_4B_OFFSET_CTR					0x4B
#define S5M3700X_4F_HSYS_CP2_SIGN				0x4F

/* Adaptive Volume Control */
#define S5M3700X_50_AVC1						0x50
#define S5M3700X_51_AVC2						0x51
#define S5M3700X_53_AVC4						0x53
#define S5M3700X_54_AVC5						0x54
#define S5M3700X_58_AVC9						0x58
#define S5M3700X_5C_AVC13						0x5C
#define S5M3700X_5F_AVC16						0x5F
#define S5M3700X_61_AVC18						0x61
#define S5M3700X_63_AVC20						0x63
#define S5M3700X_68_AVC25						0x68
#define S5M3700X_71_AVC34						0x71
#define S5M3700X_72_AVC35						0x72
#define S5M3700X_73_AVC36						0x73
#define S5M3700X_74_AVC37						0x74
#define S5M3700X_7A_AVC43						0x7A
#define S5M3700X_7C_OCPCTRL0					0x7C
#define S5M3700X_7E_AVC45						0x7E

/* Digital DSM Control */
#define S5M3700X_93_DSM_CON1					0x93
#define S5M3700X_9D_AVC_DWA						0x9D

/* Auto SEQ in Detail */
#define S5M3700X_AF_BST_CTR						0xAF

/* Manual Control */
#define S5M3700X_B4_ODSEL5						0xB4
#define S5M3700X_B5_ODSEL6						0xB5
#define S5M3700X_BA_ODSEL11						0xBA
#define S5M3700X_BC_AMU_CTR_CP1					0xBC
#define S5M3700X_BD_AMU_CTR_CP2					0xBD
#define S5M3700X_BE_AMU_CTR_CP3					0xBE

/* HMU Control */
#define S5M3700X_C0_ACTR_JD1					0xC0
#define S5M3700X_C1_ACTR_JD2					0xC1
#define S5M3700X_C2_ACTR_JD3					0xC2
#define S5M3700X_C3_ACTR_JD4					0xC3
#define S5M3700X_C4_ACTR_JD5					0xC4
#define S5M3700X_C5_ACTR_MCB1					0xC5
#define S5M3700X_C6_ACTR_MCB2					0xC6
#define S5M3700X_C7_ACTR_MCB3					0xC7
#define S5M3700X_C8_ACTR_MCB4					0xC8
#define S5M3700X_C9_ACTR_MCB5					0xC9
#define S5M3700X_CA_ACTR_IMP1					0xCA
#define S5M3700X_D0_DCTR_CM						0xD0
#define S5M3700X_D1_DCTR_TEST1					0xD1
#define S5M3700X_D2_DCTR_TEST2					0xD2
#define S5M3700X_D4_DCTR_TEST4					0xD4
#define S5M3700X_D5_DCTR_TEST5					0xD5
#define S5M3700X_D6_DCTR_TEST6					0xD6
#define S5M3700X_D7_DCTR_TEST7					0xD7
#define S5M3700X_D8_DCTR_DBNC1					0xD8
#define S5M3700X_D9_DCTR_DBNC2					0xD9
#define S5M3700X_DA_DCTR_DBNC3					0xDA
#define S5M3700X_DB_DCTR_DBNC4					0xDB
#define S5M3700X_DC_DCTR_DBNC5					0xDC
#define S5M3700X_DD_DCTR_DBNC6					0xDD
#define S5M3700X_DE_DCTR_DLY1					0xDE
#define S5M3700X_DF_DCTR_DLY2					0xDF
#define S5M3700X_E0_DCTR_FSM1					0xE0
#define S5M3700X_E1_DCTR_FSM2					0xE1
#define S5M3700X_E2_DCTR_FSM3					0xE2
#define S5M3700X_E3_DCTR_FSM4					0xE3
#define S5M3700X_E4_DCTR_FSM5					0xE4
#define S5M3700X_E5_DCTR_FSM6					0xE5
#define S5M3700X_E7_DCTR_IMP1					0xE7
#define S5M3700X_E9_DCTR_IMP3					0xE9
#define S5M3700X_EA_DCTR_IMP4					0xEA
#define S5M3700X_EB_DCTR_IMP5					0xEB
#define S5M3700X_EC_DCTR_IMP6					0xEC
#define S5M3700X_ED_DCTR_IMP7					0xED
#define S5M3700X_EE_ACTR_ETC1					0xEE
#define S5M3700X_F0_STATUS1						0xF0
#define S5M3700X_F1_STATUS2						0xF1
#define S5M3700X_F2_STATUS3						0xF2
#define S5M3700X_F3_STATUS4						0xF3
#define S5M3700X_F4_STATUS5						0xF4
#define S5M3700X_F5_STATUS6						0xF5
#define S5M3700X_F7_STATUS8						0xF7
#define S5M3700X_F8_STATUS9						0xF8
#define S5M3700X_F9_STATUS10					0xF9
#define S5M3700X_FA_STATUS11					0xFA
#define S5M3700X_FB_STATUS12					0xFB
#define S5M3700X_FD_ACTR_GP						0xFD
#define S5M3700X_FE_DCTR_GP1					0xFE
#define S5M3700X_FF_DCTR_GP2					0xFF

/* Analog Ref */
#define S5M3700X_104_CTRL_CP					0X04
#define S5M3700X_106_CTRL_CP2					0X06

/* VTS-ADC Control */
#define S5M3700X_108_PD_VTS						0x08
#define S5M3700X_109_CTRL_VTS1					0x09
#define S5M3700X_10A_CTRL_VTS2					0x0A
#define S5M3700X_10B_CTRL_VTS3					0x0B

/* Recording path Analog Setting */
#define S5M3700X_110_BST_PDB					0x10
#define S5M3700X_111_BST1						0x11
#define S5M3700X_112_BST2						0x12
#define S5M3700X_113_BST3						0x13
#define S5M3700X_114_BST4						0x14
#define S5M3700X_115_DSM1						0x15
#define S5M3700X_116_DSMC_1						0x16
#define S5M3700X_117_DSM2						0x17
#define S5M3700X_118_DSMC_2						0x18
#define S5M3700X_119_DSM3						0x19
#define S5M3700X_11A_DSMC3						0x1A
#define S5M3700x_11B_DSMC4						0x1B
#define S5M3700X_11D_BST_OP						0x1D
#define S5M3700X_11E_CTRL_MIC_I1				0x1E
#define S5M3700X_120_CTRL_MIC_I3				0x20
#define S5M3700X_122_BST4PN						0x22
#define S5M3700X_124_AUTO_DWA					0x24
#define S5M3700X_125_RSVD						0x25

/* Playback path Analog Setting */
#define S5M3700X_130_PD_IDAC1					0x30
#define S5M3700X_131_PD_IDAC2					0x31
#define S5M3700X_132_PD_IDAC3					0x32
#define S5M3700X_135_PD_IDAC4					0x35
#define S5M3700X_138_PD_HP						0x38
#define S5M3700X_139_PD_SURGE					0x39
#define S5M3700X_13A_POP_HP						0x3A
#define S5M3700X_13B_OCP_HP						0x3B
#define S5M3700X_142_REF_SURGE					0x42
#define S5M3700X_143_CTRL_OVP1					0x43
#define S5M3700X_149_PD_EP						0x49
#define S5M3700X_14A_PD_LO						0x4A

/* Analog Clock Control */
#define S5M3700X_151_AD_CLK1					0x51
#define S5M3700X_153_DA_CP0						0x53
#define S5M3700X_154_DA_CP1						0x54
#define S5M3700X_155_DA_CP2						0x55
#define S5M3700X_156_DA_CP3						0x56

/* OTP Register for OFFSET */
#define S5M3700X_288_IDAC3_OTP					0x88
#define S5M3700X_289_IDAC4_OTP					0x89
#define S5M3700X_28A_IDAC5_OTP					0x8A

/* OTP Register for Analog */
#define S5M3700X_2AE_HP_DSML					0xAE
#define S5M3700X_2AF_HP_DSMR					0xAF
#define S5M3700X_2B0_DSM_OFFSETL				0xB0
#define S5M3700X_2B1_DSM_OFFSETR				0xB1
#define S5M3700X_2B3_CTRL_IHP					0xB3
#define S5M3700X_2B4_CTRL_HP0					0xB4
#define S5M3700X_2B5_CTRL_HP1					0xB5
#define S5M3700X_2B7_CTRL_EP0					0xB7
#define S5M3700X_2B8_CTRL_EP1					0xB8
#define S5M3700X_2B9_CTRL_EP2					0xB9
#define S5M3700X_2BA_CTRL_LN1					0xBA
#define S5M3700X_2BB_CTRL_LN2					0xBB
#define S5M3700X_2BE_IDAC0_OTP					0xBE
#define S5M3700X_2BF_IDAC1_OTP					0xBF
#define S5M3700X_2C0_IDAC2_OTP					0xC0

/* OTP Register for CP2 */
#define S5M3700X_2C4_CP2_TH2_32					0xC4
#define S5M3700X_2C5_CP2_TH3_32					0xC5
#define S5M3700X_2C6_CP2_TH1_16					0xC6
#define S5M3700X_2C7_CP2_TH2_16					0xC7
#define S5M3700X_2C8_CP2_TH3_16					0xC8
#define S5M3700X_2CA_CP2_TH2_06					0xCA
#define S5M3700X_2CB_CP2_TH2_06					0xCB

/* OTP Register for Jack */
#define S5M3700X_2D2_JACK_OTP02					0xD2
#define S5M3700X_2D3_JACK_OTP03					0xD3
#define S5M3700X_2D4_JACK_OTP04					0xD4
#define S5M3700X_2D5_JACK_OTP05					0xD5
#define S5M3700X_2DD_JACK_OTP0D					0xDD
#define S5M3700X_2E0_JACK_OTP10					0xE0
#define S5M3700X_2E1_JACK_OTP11					0xE1
#define S5M3700X_2E2_JACK_OTP12					0xE2

/* S5M3700X_01_IRQ1 */
#define ST_JO_R									BIT(7)
#define ST_C_JI_R								BIT(6)
#define ST_S_JI_R								BIT(5)
#define ST_S_JI_TIMO_R							BIT(4)
#define ST_DEC_WTP_R							BIT(3)
#define ST_WT_JI_R								BIT(2)
#define ST_WT_JO_R								BIT(1)
#define ST_IMP_CHK_DONE_R						BIT(0)

/* S5M3700X_02_IRQ2 */
#define ST_DEC_POLE_R							BIT(7)
#define ST_3POLE_R								BIT(6)
#define ST_3POLE_ANT_R							BIT(5)
#define ST_ETC_R								BIT(4)
#define ST_4POLE_R								BIT(3)
#define ST_DEC_BTN_R							BIT(2)
#define GP_AVG_DONE_R							BIT(1)
#define GPADC_EOC_R								BIT(0)

/* S5M3700X_03_IRQ3 */
#define JACK_DET_R								BIT(4)
#define ANT_MDET_R								BIT(3)
#define ANT_LDET_R								BIT(1)
#define BTN_DET_R								BIT(0)

/* S5M3700X_04_IRQ4 */
#define JACK_DET_F								BIT(4)
#define ANT_MDET_F								BIT(3)
#define ANT_LDET_F								BIT(1)
#define BTN_DET_F								BIT(0)

/* S5M3700X_05_IRQ5 */
#define VTS1_SEQ_R								BIT(6)
#define VTS2_SEQ_R								BIT(5)
#define OCP_STAT_IRQ_R							BIT(4)
#define ASEQ_STAT_IRQ_R							BIT(3)
#define I2S_ERR_IRQ_R							BIT(2)
#define NOISE_IRQ_R								BIT(1)
#define ANA_IRQ_R								BIT(0)

/* S5M3700X_06_IRQ6 */
#define VTS1_SEQ_F								BIT(6)
#define VTS2_SEQ_F								BIT(5)
#define OCP_STAT_IRQ_F							BIT(4)
#define ASEQ_STAT_IRQ_F							BIT(3)
#define I2S_ERR_IRQ_F							BIT(2)
#define NOISE_IRQ_F								BIT(1)
#define ANA_IRQ_F								BIT(0)

/* S5M3700X_08_IRQ1M */
#define ST_JO_R_M								BIT(7)
#define ST_C_JI_R_M								BIT(6)
#define ST_S_JI_R_M								BIT(5)
#define ST_S_JI_TIMO_R_M						BIT(4)
#define ST_DEC_WTP_R_M							BIT(3)
#define ST_WT_JI_R_M							BIT(2)
#define ST_WT_JO_R_M							BIT(1)
#define ST_IMP_CHK_DONE_R_M						BIT(0)

/* S5M3700X_09_IRQ2M */
#define ST_DEC_POLE_R_M							BIT(7)
#define ST_3POLE_R_M							BIT(6)
#define ST_3POLE_ANT_R_M						BIT(5)
#define ST_ETC_R_M								BIT(4)
#define ST_4POLE_R_M							BIT(3)
#define ST_DEC_BTN_R_M							BIT(2)
#define GP_AVG_DONE_R_M							BIT(1)
#define GPADC_EOC_R_M							BIT(0)

/* S5M3700X_0A_IRQ3M */
#define JACK_DET_R_M							BIT(4)
#define ANT_MDET_R_M							BIT(3)
#define ANT_LDET_R_M							BIT(1)
#define BTN_DET_R_M								BIT(0)

/* S5M3700X_0B_IRQ4M */
#define JACK_DET_F_M							BIT(4)
#define ANT_MDET_F_M							BIT(3)
#define ANT_LDET_F_M							BIT(1)
#define BTN_DET_F_M								BIT(0)

/* S5M3700X_0C_IRQ5M */
#define VTS1_SEQ_R_M							BIT(6)
#define VTS2_SEQ_R_M							BIT(5)
#define OCP_STAT_IRQ_R_M						BIT(4)
#define ASEQ_STAT_IRQ_R_M						BIT(3)
#define I2S_ERR_IRQ_R_M							BIT(2)
#define NOISE_IRQ_R_M							BIT(1)
#define ANA_IRQ_R_M								BIT(0)

/* S5M3700X_0D_IRQ6M */
#define VTS1_SEQ_F_M							BIT(6)
#define VTS2_SEQ_F_M							BIT(5)
#define OCP_STAT_IRQ_F_M						BIT(4)
#define ASEQ_STAT_IRQ_F_M						BIT(3)
#define I2S_ERR_IRQ_F_M							BIT(2)
#define NOISE_IRQ_F_M							BIT(1)
#define ANA_IRQ_F_M								BIT(0)

/* S5M3700X_10_CLKGATE0 */
#define OVP_CLK_GATE_SHIFT						6
#define OVP_CLK_GATE_MASK						BIT(OVP_CLK_GATE_SHIFT)

#define SEQ_CLK_GATE_SHIFT						5
#define SEQ_CLK_GATE_MASK						BIT(SEQ_CLK_GATE_SHIFT)

#define AVC_CLK_GATE_SHIFT						4
#define AVC_CLK_GATE_MASK						BIT(AVC_CLK_GATE_SHIFT)

#define DMIC_CLK1_GATE_SHIFT					3
#define DMIC_CLK1_GATE_MASK						BIT(DMIC_CLK1_GATE_SHIFT)

#define ADC_CLK_GATE_SHIFT						2
#define ADC_CLK_GATE_MASK						BIT(ADC_CLK_GATE_SHIFT)

#define DAC_CLK_GATE_SHIFT						1
#define DAC_CLK_GATE_MASK						BIT(DAC_CLK_GATE_SHIFT)

#define COM_CLK_GATE_SHIFT						0
#define COM_CLK_GATE_MASK						BIT(COM_CLK_GATE_SHIFT)

/* S5M3700X_11_CLKGATE1 */
#define DMIC_CLK2_GATE_SHIFT					3
#define DMIC_CLK2_GATE_MASK						BIT(DMIC_CLK2_GATE_SHIFT)

#define DAC_CIC_TEMP_SHIFT						2
#define DAC_CIC_TEMP_MASK						BIT(DAC_CIC_TEMP_SHIFT)

#define DAC_CIC_CGLR_SHIFT						1
#define DAC_CIC_CGLR_MASK						BIT(DAC_CIC_CGLR_SHIFT)

#define DAC_CIC_CGC_SHIFT						0
#define DAC_CIC_CGC_MASK						BIT(DAC_CIC_CGC_SHIFT)

/* S5M3700X_12_CLKGATE2 */
#define CED_CLK_GATE_SHIFT						5
#define CED_CLK_GATE_MASK						BIT(CED_CLK_GATE_SHIFT)

#define VTS_CLK_GATE_SHIFT						4
#define VTS_CLK_GATE_MASK						BIT(VTS_CLK_GATE_SHIFT)

#define ADC_CIC_CGL_SHIFT						2
#define ADC_CIC_CGL_MASK						BIT(ADC_CIC_CGL_SHIFT)

#define ADC_CIC_CGR_SHIFT						1
#define ADC_CIC_CGR_MASK						BIT(ADC_CIC_CGR_SHIFT)

#define ADC_CIC_CGC_SHIFT						0
#define ADC_CIC_CGC_MASK						BIT(ADC_CIC_CGC_SHIFT)

/* S5M3700X_14_RESETB0 */
#define AVC_RESETB_SHIFT						7
#define AVC_RESETB_MASK							BIT(AVC_RESETB_SHIFT)

#define ENOF_RESETDAC_SHIFT						6
#define ENOF_RESETDAC_MASK						BIT(ENOF_RESETDAC_SHIFT)

#define RSTB_ADC_SHIFT							5
#define RSTB_ADC_MASK							BIT(RSTB_ADC_SHIFT)

#define RSTB_DAC_DSM_SHIFT						4
#define RSTB_DAC_DSM_MASK						BIT(RSTB_DAC_DSM_SHIFT)

#define ADC_RESETB_SHIFT						3
#define ADC_RESETB_MASK							BIT(ADC_RESETB_SHIFT)

#define DAC_RESETB_SHIFT						2
#define DAC_RESETB_MASK							BIT(DAC_RESETB_SHIFT)

#define CORE_RESETB_SHIFT						1
#define CORE_RESETB_MASK						BIT(CORE_RESETB_SHIFT)

#define SYSTEM_RESETB_SHIFT						0
#define SYSTEM_RESETB_MASK						BIT(SYSTEM_RESETB_SHIFT)

/* S5M3700X_15_RESETB1 */
#define RSTB_DAC_DSM_C_SHIFT					1
#define RSTB_DAC_DSM_C_MASK						BIT(RSTB_DAC_DSM_C_SHIFT)

#define VTS_RESETB_SHIFT						0
#define VTS_RESETB_MASK							BIT(VTS_RESETB_SHIFT)

/* S5M3700X_16_CLK_MODE_SEL1 */
#define ADC_FSEL_SHIFT							4
#define ADC_FSEL_WIDTH							2
#define ADC_FSEL_MASK							MASK(ADC_FSEL_WIDTH, ADC_FSEL_SHIFT)

#define ADC_MODE_STEREO							0
#define ADC_MODE_TRIPLE							1
#define ADC_MODE_MONO							2
#define ADC_MODE_NONE							3

#define DAC_DSEL_SHIFT							2
#define DAC_DSEL_WIDTH							2
#define DAC_DSEL_MASK							MASK(DAC_DSEL_WIDTH, DAC_DSEL_SHIFT)

#define DAC_FSEL_SHIFT							1
#define DAC_FSEL_MASK							BIT(DAC_FSEL_SHIFT)

#define AVC_CLK_SEL_SHIFT						0
#define AVC_CLK_SEL_MASK						BIT(AVC_CLK_SEL_SHIFT)

/* S5M3700X_18_PWAUTO_AD */
#define APW_VTS1_SHIFT							5
#define APW_VTS1_MASK							BIT(APW_VTS1_SHIFT)

#define APW_VTS2_SHIFT							4
#define APW_VTS2_MASK							BIT(APW_VTS2_SHIFT)

#define APW_MIC1L_SHIFT							3
#define APW_MIC1L_MASK							BIT(APW_MIC1L_SHIFT)

#define APW_MIC2C_SHIFT							2
#define APW_MIC2C_MASK							BIT(APW_MIC2C_SHIFT)

#define APW_MIC3R_SHIFT							1
#define APW_MIC3R_MASK							BIT(APW_MIC3R_SHIFT)

#define APW_MIC4R_SHIFT							0
#define APW_MIC4R_MASK							BIT(APW_MIC4R_SHIFT)

/* S5M3700X_19_PWAUTO_DA */
#define APW_LINE_SHIFT							2
#define APW_LINE_MASK							BIT(APW_LINE_SHIFT)

#define APW_HP_SHIFT							1
#define APW_HP_MASK								BIT(APW_HP_SHIFT)

#define APW_RCV_SHIFT							0
#define APW_RCV_MASK							BIT(APW_RCV_SHIFT)

/* S5M3700X_1E_CHOP1 */
#define HP_ON_SHIFT								7
#define HP_ON_MASK								BIT(HP_ON_SHIFT)

#define EP_ON_SHIFT								6
#define EP_ON_MASK								BIT(EP_ON_SHIFT)

#define DAC_ON_SHIFT							6
#define DAC_ON_WIDTH							2
#define DAC_ON_MASK								MASK(DAC_ON_WIDTH, DAC_ON_SHIFT)

#define DMIC2_ON_SHIFT							5
#define DMIC2_ON_MASK							BIT(DMIC2_ON_SHIFT)

#define DMIC1_ON_SHIFT							4
#define DMIC1_ON_MASK							BIT(DMIC1_ON_SHIFT)

#define DMIC_ON_SHIFT							4
#define DMIC_ON_WIDTH							2
#define DMIC_ON_MASK							MASK(DMIC_ON_WIDTH, DMIC_ON_SHIFT)

#define MIC4_ON_SHIFT							3
#define MIC4_ON_MASK							BIT(MIC4_ON_SHIFT)

#define MIC3_ON_SHIFT							2
#define MIC3_ON_MASK							BIT(MIC3_ON_SHIFT)

#define MIC2_ON_SHIFT							1
#define MIC2_ON_MASK							BIT(MIC2_ON_SHIFT)

#define MIC1_ON_SHIFT							0
#define MIC1_ON_MASK							BIT(MIC1_ON_SHIFT)

#define AMIC_ON_SHIFT							0
#define AMIC_ON_WIDTH							4
#define AMIC_ON_MASK							MASK(AMIC_ON_WIDTH, AMIC_ON_SHIFT)

#define AMIC_ZERO								0
#define AMIC_MONO								1
#define AMIC_STEREO								2
#define AMIC_TRIPLE								3

/* S5M3700X_1F_CHOP2 */
#define LINE_ON_SHIFT							0
#define LINE_ON_MASK							BIT(LINE_ON_SHIFT)

/* S5M3700X_20_IF_FORM1 */
#define I2S_DF_SHIFT							4
#define I2S_DF_WIDTH							3
#define I2S_DF_MASK								MASK(I2S_DF_WIDTH, I2S_DF_SHIFT)

#define BCK_POL_SHIFT							1
#define BCK_POL_MASK							BIT(BCK_POL_SHIFT)

#define LRCK_POL_SHIFT							0
#define LRCK_POL_MASK							BIT(LRCK_POL_SHIFT)

/* S5M3700X_21_IF_FORM2 */
#define I2S_DL_SHIFT							0
#define I2S_DL_WIDTH							6
#define I2S_DL_MASK								MASK(I2S_DL_WIDTH, I2S_DL_SHIFT)

#define I2S_DL_16								0x10
#define I2S_DL_20								0x14
#define I2S_DL_24								0x18
#define I2S_DL_32								0x20

/* S5M3700X_22_IF_FORM3 */
#define I2S_XFS_SHIFT							0
#define I2S_XFS_WIDTH							8
#define I2S_XFS_MASK							MASK(I2S_XFS_WIDTH, I2S_XFS_SHIFT)

#define I2S_XFS_32								0x20
#define I2S_XFS_48								0x30
#define I2S_XFS_64								0x40
#define I2S_XFS_96								0x60
#define I2S_XFS_128								0x80

/* S5M3700X_23_IF_FORM4 */
#define SEL_ADC3_SHIFT							6
#define SEL_ADC3_WIDTH							2
#define SEL_ADC3_MASK							MASK(SEL_ADC3_WIDTH, SEL_ADC3_SHIFT)

#define SEL_ADC2_SHIFT							4
#define SEL_ADC2_WIDTH							2
#define SEL_ADC2_MASK							MASK(SEL_ADC2_WIDTH, SEL_ADC2_SHIFT)

#define SEL_ADC1_SHIFT							2
#define SEL_ADC1_WIDTH							2
#define SEL_ADC1_MASK							MASK(SEL_ADC1_WIDTH, SEL_ADC1_SHIFT)

#define SEL_ADC0_SHIFT							0
#define SEL_ADC0_WIDTH							2
#define SEL_ADC0_MASK							MASK(SEL_ADC0_WIDTH, SEL_ADC0_SHIFT)

/* S5M3700X_24_IF_FORM5 */
#define SDIN_TIE0_SHIFT							7
#define SDIN_TIE0_MASK							BIT(SDIN_TIE0_SHIFT)

#define SDOUT_TIE0_SHIFT						6
#define SDOUT_TIE0_MASK							BIT(SDOUT_TIE0_SHIFT)

#define SEL_DAC1_SHIFT							4
#define SEL_DAC1_WIDTH							2
#define SEL_DAC1_MASK							MASK(SEL_DAC1_WIDTH, SEL_DAC1_SHIFT)

#define SEL_DAC0_SHIFT							0
#define SEL_DAC0_WIDTH							2
#define SEL_DAC0_MASK							MASK(SEL_DAC0_WIDTH, SEL_DAC0_SHIFT)

/* S5M3700X_29_IF_FORM6 */
#define ADC_OUT_FORMAT_SEL_SHIFT				0
#define ADC_OUT_FORMAT_SEL_WIDTH				3
#define ADC_OUT_FORMAT_SEL_MASK					MASK(ADC_OUT_FORMAT_SEL_WIDTH, ADC_OUT_FORMAT_SEL_SHIFT)

#define ADC_FM_48K_AT_48K			0
#define ADC_FM_192K_AT_192K			1
#define ADC_FM_48K_ZP_AT_192K		2
#define ADC_FM_48K_4COPY_AT_192K	3
#define ADC_FM_192K_ZP_AT_384K		4
#define ADC_FM_192K_2COPY_AT_384K	5
#define ADC_FM_48K_ZP_AT_384K		6
#define ADC_FM_48K_8COPY_AT_384K	7

/* S5M3700X_2F_DMIC_ST */
#define VTS_DATA_ENB_SHIFT						4
#define VTS_DATA_ENB_MASK						BIT(VTS_DATA_ENB_SHIFT)

#define DMIC_ST_SHIFT							2
#define DMIC_ST_WIDTH							2
#define DMIC_ST_MASK							MASK(DMIC_ST_WIDTH, DMIC_ST_SHIFT)

#define DMIC_IO_4mA								0
#define DMIC_IO_8mA								1
#define DMIC_IO_12mA							2
#define DMIC_IO_16mA							3

#define SDOUT_ST_SHIFT							0
#define SDOUT_ST_WIDTH							2
#define SDOUT_ST_MASK							MASK(SDOUT_ST_WIDTH, SDOUT_ST_SHIFT)

#define SDOUT_IO_4mA							0
#define SDOUT_IO_8mA							1
#define SDOUT_IO_12mA							2
#define SDOUT_IO_16mA							3

/* S5M3700X_30_ADC1 */
#define FS_SEL_SHIFT							6
#define FS_SEL_MASK								BIT(FS_SEL_SHIFT)

#define CH_MODE_SHIFT							4
#define CH_MODE_WIDTH							2
#define CH_MODE_MASK							MASK(CH_MODE_WIDTH, CH_MODE_SHIFT)

#define ADC_CH_STEREO							0
#define ADC_CH_MONO								1
#define ADC_CH_TRIPLE							2

#define MU_TYPE_SHIFT							3
#define MU_TYPE_MASK							BIT(MU_TYPE_SHIFT)

#define ADC_MUTEL_SHIFT							2
#define ADC_MUTEL_MASK							BIT(ADC_MUTEL_SHIFT)

#define ADC_MUTER_SHIFT							1
#define ADC_MUTER_MASK							BIT(ADC_MUTER_SHIFT)

#define ADC_MUTEC_SHIFT							0
#define ADC_MUTEC_MASK							BIT(ADC_MUTEC_SHIFT)

#define ADC_MUTE_DISABLE			0
#define ADC_MUTE_ENABLE				1
#define ADC_MUTE_ALL				7

/* S5M3700X_31_ADC2 */
#define DMIC_POL_SHIFT							7
#define DMIC_POL_MASK							BIT(DMIC_POL_SHIFT)

#define INP_SEL_R_SHIFT							4
#define INP_SEL_R_WIDTH							3
#define INP_SEL_R_MASK							MASK(INP_SEL_R_WIDTH, INP_SEL_R_SHIFT)

#define INP_SEL_L_SHIFT							0
#define INP_SEL_L_WIDTH							3
#define INP_SEL_L_MASK							MASK(INP_SEL_L_WIDTH, INP_SEL_L_SHIFT)

/* S5M3700X_32_ADC3 */
#define DMIC_CLK_ZTIE_SHIFT						7
#define DMIC_CLK_ZTIE_MASK						BIT(DMIC_CLK_ZTIE_SHIFT)

#define DMIC_GAIN_PRE_SHIFT						4
#define DMIC_GAIN_PRE_WIDTH						3
#define DMIC_GAIN_PRE_MASK						MASK(DMIC_GAIN_PRE_WIDTH, DMIC_GAIN_PRE_SHIFT)

#define DMIC_GAIN_1					0
#define DMIC_GAIN_3					1
#define DMIC_GAIN_5					2
#define DMIC_GAIN_7					3
#define DMIC_GAIN_9					4
#define DMIC_GAIN_11				5
#define DMIC_GAIN_13				6
#define DMIC_GAIN_15				7

#define INP_SEL_C_SHIFT							0
#define INP_SEL_C_WIDTH							3
#define INP_SEL_C_MASK							MASK(INP_SEL_C_WIDTH, INP_SEL_C_SHIFT)

/* S5M3700X_33_ADC4 */
#define DMIC_EN2_SHIFT							7
#define DMIC_EN2_MASK							BIT(DMIC_EN2_SHIFT)

#define DMIC_EN1_SHIFT							6
#define DMIC_EN1_MASK							BIT(DMIC_EN1_SHIFT)

#define CP_TYPE_SEL_SHIFT						2
#define CP_TYPE_SEL_WIDTH						3
#define CP_TYPE_SEL_MASK						MASK(CP_TYPE_SEL_WIDTH, CP_TYPE_SEL_SHIFT)

#define FILTER_TYPE_NORMAL_AMIC					0
#define FILTER_TYPE_UHQA_W_LP_AMIC				1
#define FILTER_TYPE_UHQA_WO_LP_AMIC				2
#define FILTER_TYPE_NORMAL_DMIC					3
#define FILTER_TYPE_UHQA_WO_LP_DMIC				4
#define FILTER_TYPE_BYPASS_COMP					6

#define DMIC_OSR_SHIFT							0
#define DMIC_OSR_WIDTH							2
#define DMIC_OSR_MASK							MASK(DMIC_OSR_WIDTH, DMIC_OSR_SHIFT)

#define DMIC_OSR_NO								0
#define DMIC_OSR_64								1
#define DMIC_OSR_32								2
#define DMIC_OSR_16								3

/* S5M3700X_34_AD_VOLL */
/* S5M3700X_35_AD_VOLR */
/* S5M3700X_36_AD_VOLC */
#define DVOL_ADC_SHIFT							0
#define DVOL_ADC_WIDTH							8
#define DVOL_ADC_MASK							MASK(DVOL_ADC_WIDTH, DVOL_ADC_SHIFT)

#define ADC_DVOL_MAXNUM							0xE5

/* S5M3700X_37_AD_HPF */
#define HPF_SEL_SHIFT							4
#define HPF_SEL_WIDTH							2
#define HPF_SEL_MASK							MASK(HPF_SEL_WIDTH, HPF_SEL_SHIFT)

#define HPF_15_15HZ								0
#define HPF_33_31HZ								1
#define HPF_60_61HZ								2
#define HPF_113_117HZ							3

#define HPF_ORDER_SHIFT							3
#define HPF_ORDER_MASK							BIT(HPF_ORDER_SHIFT)

#define HPF_ORDER_2ND							0
#define HPF_ORDER_1ST							1

#define HPF_ENL_SHIFT							2
#define HPF_ENL_MASK							BIT(HPF_ENL_SHIFT)

#define HPF_ENR_SHIFT							1
#define HPF_ENR_MASK							BIT(HPF_ENR_SHIFT)

#define HPF_ENC_SHIFT							0
#define HPF_ENC_MASK							BIT(HPF_ENC_SHIFT)

#define HPF_DISABLE								0
#define HPF_ENABLE								1

/* S5M3700X_3A_AD_VOL */
#define BTN_MUTEL_SHIFT							2
#define BTN_MUTEL_MASK							BIT(BTN_MUTEL_SHIFT)

#define BTN_MUTER_SHIFT							1
#define BTN_MUTER_MASK							BIT(BTN_MUTER_SHIFT)

#define BTN_MUTEC_SHIFT							0
#define BTN_MUTEC_MASK							BIT(BTN_MUTEC_SHIFT)

/* S5M3700X_40_PLAY_MODE */
#define PLAY_MODE_SEL_SHIFT						4
#define PLAY_MODE_SEL_WIDTH						2
#define PLAY_MODE_SEL_MASK						MASK(PLAY_MODE_SEL_WIDTH, PLAY_MODE_SEL_SHIFT)

#define DA_SMUTEL_SHIFT							1
#define DA_SMUTEL_MASK							BIT(DA_SMUTEL_SHIFT)

#define DA_SMUTER_SHIFT							0
#define DA_SMUTER_MASK							BIT(DA_SMUTER_SHIFT)

#define DAC_MUTE_DISABLE						0
#define DAC_MUTE_ENABLE							1
#define DAC_MUTE_ALL							3

/* S5M3700X_41_PLAY_VOLL */
/* S5M3700X_42_PLAY_VOLR */
#define DVOL_DA_SHIFT							0
#define DVOL_DA_WIDTH							8
#define DVOL_DA_MASK							MASK(DVOL_DA_WIDTH, DVOL_DA_SHIFT)

#define DAC_DVOL_MAXNUM				0x00
#define DAC_DVOL_DEFAULT			0x54
#define DAC_DVOL_MINNUM				0xEA

/* S5M3700X_44_PLAY_MIX0 */
#define EP_MODE_SHIFT							7
#define EP_MODE_MASK							BIT(EP_MODE_SHIFT)

#define DAC_MIXL_SHIFT							4
#define DAC_MIXL_WIDTH							3
#define DAC_MIXL_MASK							MASK(DAC_MIXL_WIDTH, DAC_MIXL_SHIFT)

#define DAC_MIXR_SHIFT							0
#define DAC_MIXR_WIDTH							3
#define DAC_MIXR_MASK							MASK(DAC_MIXR_WIDTH, DAC_MIXR_SHIFT)

/* S5M3700X_45_PLAY_MIX1 */
#define LN_RCV_MIX_SHIFT						3
#define LN_RCV_MIX_WIDTH						2
#define LN_RCV_MIX_MASK							MASK(LN_RCV_MIX_WIDTH, LN_RCV_MIX_SHIFT)

/* S5M3700X_50_AVC1 */
#define AVC_CON_FLAG_SHIFT						6
#define AVC_CON_FLAG_MASK						BIT(AVC_CON_FLAG_SHIFT)

#define AVC_VA_FLAG_SHIFT						5
#define AVC_VA_FLAG_MASK						BIT(AVC_VA_FLAG_SHIFT)

#define AVC_MU_FLAG_SHIFT						4
#define AVC_MU_FLAG_MASK						BIT(AVC_MU_FLAG_SHIFT)

#define AVC_BYPS_SHIFT							3
#define AVC_BYPS_MASK							BIT(AVC_BYPS_SHIFT)

/* S5M3700X_53_AVC4 */
#define CTVOL_AVC_PO_GAIN_SHIFT					0
#define CTVOL_AVC_PO_GAIN_WIDTH					4
#define CTVOL_AVC_PO_GAIN_MASK					MASK(CTVOL_AVC_PO_GAIN_WIDTH, CTVOL_AVC_PO_GAIN_SHIFT)

#define AVC_PO_GAIN_0							0
#define AVC_PO_GAIN_15							15

/* S5M3700X_5C_AVC13 */
#define AMUTE_MASKB_SHIFT						7
#define AMUTE_MASKB_MASK						BIT(AMUTE_MASKB_SHIFT)

#define DMUTE_MASKB_SHIFT						3
#define DMUTE_MASKB_MASK						BIT(DMUTE_MASKB_SHIFT)

/* S5M3700X_5F_AVC16 */
/* S5M3700X_61_AVC18 */
/* S5M3700X_63_AVC20 */

#define SIGN_SHIFT								6
#define SIGN_MASK								BIT(SIGN_SHIFT)

#define AVC_AVOL_SHIFT							0
#define AVC_AVOL_WIDTH							6
#define AVC_AVOL_MASK							MASK(AVC_AVOL_WIDTH, AVC_AVOL_SHIFT)

/* S5M3700X_7C_OCPCTRL0 */
#define OCP_ENL_SHIFT							1
#define OCP_ENL_MASK							BIT(OCP_ENL_SHIFT)

#define OCP_ENR_SHIFT							0
#define OCP_ENR_MASK							BIT(OCP_ENR_SHIFT)

/* S5M3700X_AF_BST_CTR */
#define EN_ARESETB_BST4_SHIFT					0
#define EN_ARESETB_BST4_MASK					BIT(EN_ARESETB_BST4_SHIFT)

/* S5M3700X_B4_ODSEL5 */
#define EN_DAC1_OUTTIE_SHIFT					3
#define EN_DAC1_OUTTIE_MASK						BIT(EN_DAC1_OUTTIE_SHIFT)
#define EN_DAC2_OUTTIE_SHIFT					0
#define EN_DAC2_OUTTIE_MASK						BIT(EN_DAC2_OUTTIE_SHIFT)

/* S5M3700X_B5_ODSEL6 */
#define T_EN_OUTTIEL_SHIFT						4
#define T_EN_OUTTIEL_MASK						BIT(T_EN_OUTTIEL_SHIFT)
#define T_EN_OUTTIER_SHIFT						3
#define T_EN_OUTTIER_MASK						BIT(T_EN_OUTTIER_SHIFT)

/* S5M3700X_BC_AMU_CTR_CP1 */
#define IM_CP1_CLK_SHIFT						4
#define IM_CP1_CLK_WIDTH						3
#define IM_CP1_CLK_MASK							MASK(IM_CP1_CLK_WIDTH, IM_CP1_CLK_SHIFT)

#define IM_CP2_CLK_SHIFT						0
#define IM_CP2_CLK_WIDTH						3
#define IM_CP2_CLK_MASK							MASK(IM_CP2_CLK_WIDTH, IM_CP2_CLK_SHIFT)

/* S5M3700X_BD_AMU_CTR_CP2 */
#define OM_CP1_CLK_HP_SHIFT						4
#define OM_CP1_CLK_HP_WIDTH						3
#define OM_CP1_CLK_HP_MASK						MASK(OM_CP1_CLK_HP_WIDTH, OM_CP1_CLK_HP_SHIFT)

#define OM_CP2_CLK_HP_SHIFT						0
#define OM_CP2_CLK_HP_WIDTH						3
#define OM_CP2_CLK_HP_MASK						MASK(OM_CP2_CLK_HP_WIDTH, OM_CP2_CLK_HP_SHIFT)

/* S5M3700X_BE_AMU_CTR_CP3 */
#define OM_CP1_CLK_RCV_LN_SHIFT					4
#define OM_CP1_CLK_RCV_LN_WIDTH					3
#define OM_CP1_CLK_RCV_LN_MASK					MASK(OM_CP1_CLK_RCV_LN_WIDTH, OM_CP1_CLK_RCV_LN_SHIFT)

#define OM_CP2_CLK_RCV_LN_SHIFT					0
#define OM_CP2_CLK_RCV_LN_WIDTH					3
#define OM_CP2_CLK_RCV_LN_MASK					MASK(OM_CP2_CLK_RCV_LN_WIDTH, OM_CP2_CLK_RCV_LN_SHIFT)

/* S5M3700X_C0_ACTR_JD1 */
#define EN_IN_MDET_SHIFT						5
#define EN_IN_MDET_MASK							BIT(EN_IN_MDET_SHIFT)

#define PDB_BTN_DET_SHIFT						4
#define PDB_BTN_DET_MASK						BIT(PDB_BTN_DET_SHIFT)

#define PDB_ANT_MDET_SHIFT						3
#define PDB_ANT_MDET_MASK						BIT(PDB_ANT_MDET_SHIFT)

#define PDB_ANT_MDET2_SHIFT						2
#define PDB_ANT_MDET2_MASK						BIT(PDB_ANT_MDET2_SHIFT)

#define PDB_ANT_LDET_SHIFT						1
#define PDB_ANT_LDET_MASK						BIT(PDB_ANT_LDET_SHIFT)

#define PDB_JD_SHIFT							0
#define PDB_JD_MASK								BIT(PDB_JD_SHIFT)

/* S5M3700X_C1_ACTR_JD2 */
#define CTRV_GDET_VTH_SHIFT						4
#define CTRV_GDET_VTH_WIDTH						3
#define CTRV_GDET_VTH_MASK						MASK(CTRV_GDET_VTH_WIDTH, CTRV_GDET_VTH_SHIFT)

#define CTRV_GDET_POP_SHIFT						0
#define CTRV_GDET_POP_WIDTH						3
#define CTRV_GDET_POP_MASK						MASK(CTRV_GDET_POP_WIDTH, CTRV_GDET_POP_SHIFT)

/* S5M3700X_C2_ACTR_JD3 */
#define CTRV_LDET_VTH_SHIFT						4
#define CTRV_LDET_VTH_WIDTH						3
#define CTRV_LDET_VTH_MASK						MASK(CTRV_GDET_VTH_WIDTH, CTRV_GDET_VTH_SHIFT)

#define CTRV_LDET_POP_SHIFT						0
#define CTRV_LDET_POP_WIDTH						3
#define CTRV_LDET_POP_MASK						MASK(CTRV_GDET_POP_WIDTH, CTRV_GDET_POP_SHIFT)

/* S5M3700X_C3_ACTR_JD4 */
#define CTRV_BTN_REF_SHIFT						4
#define CTRV_BTN_REF_WIDTH						3
#define CTRV_BTN_REF_MASK						MASK(CTRV_BTN_REF_WIDTH, CTRV_BTN_REF_SHIFT)

#define CTRV_ANT_LDET_VTH_SHIFT					0
#define CTRV_ANT_LDET_VTH_WIDTH					2
#define CTRV_ANT_LDET_VTH_MASK					MASK(CTRV_ANT_LDET_VTH_WIDTH, CTRV_ANT_LDET_VTH_SHIFT)

#define ANT_LDET_VTH_0							0
#define ANT_LDET_VTH_1							1
#define ANT_LDET_VTH_2							2
#define ANT_LDET_VTH_3							3

/* S5M3700X_C4_ACTR_JD5 */
#define CTRV_ANT_MDET_SHIFT						4
#define CTRV_ANT_MDET_WIDTH						3
#define CTRV_ANT_MDET_MASK						MASK(CTRV_ANT_MDET_WIDTH, CTRV_ANT_MDET_SHIFT)

#define PDB_MCB_LDO_SHIFT						3
#define PDB_MCB_LDO_MASK						BIT(PDB_MCB_LDO_SHIFT)

#define CTRV_MCB_LDO_SHIFT						2
#define CTRV_MCB_LDO_MASK						BIT(CTRV_MCB_LDO_SHIFT)

#define CTPD_MCB_LDO_SHIFT						1
#define CTPD_MCB_LDO_MASK						BIT(CTPD_MCB_LDO_SHIFT)

/* S5M3700X_C5_ACTR_MCB1 */
#define PDB_MCB1_P_SHIFT						2
#define PDB_MCB1_P_MASK							BIT(PDB_MCB1_P_SHIFT)

#define PDB_MCB2_P_SHIFT						1
#define PDB_MCB2_P_MASK							BIT(PDB_MCB2_P_SHIFT)

#define PDB_MCB3_P_SHIFT						0
#define PDB_MCB3_P_MASK							BIT(PDB_MCB3_P_SHIFT)

/* S5M3700X_C6_ACTR_MCB2 */
#define PDB_MCB1_SHIFT							6
#define PDB_MCB1_MASK							BIT(PDB_MCB1_P_SHIFT)

#define PDB_MCB2_SHIFT							5
#define PDB_MCB2_MASK							BIT(PDB_MCB2_P_SHIFT)

#define PDB_MCB3_SHIFT							4
#define PDB_MCB3_MASK							BIT(PDB_MCB3_P_SHIFT)

#define EN_MCB1_LPWR_SHIFT						2
#define EN_MCB1_LPWR_MASK						BIT(EN_MCB1_LPWR_SHIFT)

#define EN_MCB2_LPWR_SHIFT						1
#define EN_MCB2_LPWR_MASK						BIT(EN_MCB2_LPWR_SHIFT)

#define EN_MCB3_LPWR_SHIFT						0
#define EN_MCB3_LPWR_MASK						BIT(EN_MCB3_LPWR_SHIFT)

/* S5M3700X_C7_ACTR_MCB3 */
#define EN_MCB1_BYP_SHIFT						7
#define EN_MCB1_BYP_MASK						BIT(EN_MCB1_BYP_SHIFT)

#define EN_MCB2_BYP_SHIFT						7
#define EN_MCB2_BYP_MASK						BIT(EN_MCB2_BYP_SHIFT)

#define EN_MCB3_BYP_SHIFT						7
#define EN_MCB3_BYP_MASK						BIT(EN_MCB3_BYP_SHIFT)

#define CTPD_MCB1_SHIFT							4
#define CTPD_MCB1_MASK							BIT(CTPD_MCB1_SHIFT)

#define CTPD_MCB2_SHIFT							3
#define CTPD_MCB2_MASK							BIT(CTPD_MCB2_SHIFT)

#define CTPD_MCB3_SHIFT							2
#define CTPD_MCB3_MASK							BIT(CTPD_MCB3_SHIFT)

/* S5M3700X_C8_ACTR_MCB4 */
#define CTRV_MCB1_SHIFT							0
#define CTRV_MCB1_WIDTH							4
#define CTRV_MCB1_MASK							MASK(CTRV_MCB1_WIDTH, CTRV_MCB1_SHIFT)

/* S5M3700X_C9_ACTR_MCB5 */
#define CTRV_MCB2_SHIFT							4
#define CTRV_MCB2_WIDTH							4
#define CTRV_MCB2_MASK							MASK(CTRV_MCB2_WIDTH, CTRV_MCB2_SHIFT)

#define CTRV_MCB3_SHIFT							0
#define CTRV_MCB3_WIDTH							4
#define CTRV_MCB3_MASK							MASK(CTRV_MCB3_WIDTH, CTRV_MCB3_SHIFT)

#define MICBIAS_VO_2_8V							0xF
#define MICBIAS_VO_1_8V							0x5
#define MICBIAS_VO_1_5V							0x2

/* S5M3700X_CA_ACTR_IMP1 */
#define EN_IMP_CAL_SHIFT						4
#define EN_IMP_CAL_MASK							BIT(EN_IMP_CAL_SHIFT)

/* S5M3700X_D0_DCTR_CM */
#define APW_MCB1_SHIFT							7
#define APW_MCB1_MASK							BIT(APW_MCB1_SHIFT)

#define APW_MCB2_SHIFT							6
#define APW_MCB2_MASK							BIT(APW_MCB2_SHIFT)

#define APW_MCB3_SHIFT							5
#define APW_MCB3_MASK							BIT(APW_MCB3_SHIFT)

#define APW_MCB_SHIFT							5
#define APW_MCB_WIDTH							3
#define APW_MCB_MASK							BIT(APW_MCB_SHIFT)

#define OPT_MCB1_LPWR_SHIFT						4
#define OPT_MCB1_LPWR_MASK						BIT(OPT_MCB1_LPWR_SHIFT)

#define OPT_MCB2_LPWR_SHIFT						3
#define OPT_MCB2_LPWR_MASK						BIT(OPT_MCB2_LPWR_SHIFT)

#define OPT_MCB3_LPWR_SHIFT						2
#define OPT_MCB3_LPWR_MASK						BIT(OPT_MCB3_LPWR_SHIFT)

#define PDB_JD_CLK_EN_SHIFT						0
#define PDB_JD_CLK_EN_MASK						BIT(PDB_JD_CLK_EN_SHIFT)

/* S5M3700X_D1_DCTR_TEST1 */
#define T_EN_MCB2_LPWR_SHIFT					5
#define T_EN_MCB2_LPWR_MASK						BIT(T_EN_MCB2_LPWR_SHIFT)

#define T_CTRV_LDET_POP_SHIFT					4
#define T_CTRV_LDET_POP_MASK					BIT(T_CTRV_LDET_POP_SHIFT)

#define T_CTRV_GDET_POP_SHIFT					3
#define T_CTRV_GDET_POP_MASK					BIT(T_CTRV_GDET_POP_SHIFT)

/* S5M3700X_D2_DCTR_TEST2 */
#define T_PDB_MCB_LDO_SHIFT						7
#define T_PDB_MCB_LDO_MASK						BIT(T_PDB_MCB_LDO_SHIFT)

#define T_PDB_MCB2_P_SHIFT						6
#define T_PDB_MCB2_P_MASK						BIT(T_PDB_MCB2_P_SHIFT)

#define T_PDB_MCB2_SHIFT						5
#define T_PDB_MCB2_MASK							BIT(T_PDB_MCB2_SHIFT)

#define T_CTRV_MCB2_SHIFT						4
#define T_CTRV_MCB2_MASK						BIT(T_CTRV_MCB2_SHIFT)

#define T_CTRV_ANT_MDET_SHIFT					3
#define T_CTRV_ANT_MDET_MASK					BIT(T_CTRV_ANT_MDET_SHIFT)

#define T_PDB_ANT_MDET_SHIFT					2
#define T_PDB_ANT_MDET_MASK						BIT(T_PDB_ANT_MDET_SHIFT)

#define T_PDB_ANT_MDET2_SHIFT					1
#define T_PDB_ANT_MDET2_MASK					BIT(T_PDB_ANT_MDET2_SHIFT)

#define T_PDB_BTN_DET_SHIFT						0
#define T_PDB_BTN_DET_MASK						BIT(T_PDB_BTN_DET_SHIFT)

/* S5M3700X_D4_DCTR_TEST4 */
#define OPT_MCB1_BYP_SHIFT						7
#define OPT_MCB1_BYP_MASK						BIT(OPT_MCB1_BYP_SHIFT)

#define OPT_MCB2_BYP_SHIFT						6
#define OPT_MCB2_BYP_MASK						BIT(OPT_MCB2_BYP_SHIFT)

#define OPT_MCB3_BYP_SHIFT						5
#define OPT_MCB3_BYP_MASK						BIT(OPT_MCB3_BYP_SHIFT)

#define T_CTRV_MCB1_SHIFT						4
#define T_CTRV_MCB1_MASK						BIT(T_CTRV_MCB1_SHIFT)

#define T_CTRV_MCB3_SHIFT						3
#define T_CTRV_MCB3_MASK						BIT(T_CTRV_MCB3_SHIFT)

/* S5M3700X_D5_DCTR_TEST5 */
#define T_A2D_LDET_OUT_SHIFT					6
#define T_A2D_LDET_OUT_WIDTH					2
#define T_A2D_LDET_OUT_MASK						MASK(T_A2D_LDET_OUT_WIDTH, T_A2D_LDET_OUT_SHIFT)

#define T_A2D_GDET_OUT_SHIFT					4
#define T_A2D_GDET_OUT_WIDTH					2
#define T_A2D_GDET_OUT_MASK						MASK(T_A2D_GDET_OUT_WIDTH, T_A2D_GDET_OUT_SHIFT)

#define T_A2D_BTN_OUT_SHIFT						2
#define T_A2D_BTN_OUT_WIDTH						2
#define T_A2D_BTN_OUT_MASK						MASK(T_A2D_BTN_OUT_WIDTH, T_A2D_BTN_OUT_SHIFT)

#define T_A2D_ANT_LDET_OUT_SHIFT				0
#define T_A2D_ANT_LDET_OUT_WIDTH				2
#define T_A2D_ANT_LDET_OUT_MASK					MASK(T_A2D_ANT_LDET_OUT_WIDTH, T_A2D_ANT_LDET_OUT_SHIFT)

/* S5M3700X_D6_DCTR_TEST6 */
#define T_A2D_ANT_MDET_OUT_SHIFT				6
#define T_A2D_ANT_MDET_OUT_WIDTH				2
#define T_A2D_ANT_MDET_OUT_MASK					MASK(T_A2D_ANT_MDET_OUT_WIDTH, T_A2D_ANT_MDET_OUT_SHIFT)

#define T_D2D_BTN_DBNC_SHIFT					2
#define T_D2D_BTN_DBNC_WIDTH					2
#define T_D2D_BTN_DBNC_MASK						MASK(T_D2D_BTN_DBNC_WIDTH, T_D2D_BTN_DBNC_SHIFT)

#define T_D2D_ANT_M_SHIFT						0
#define T_D2D_ANT_M_WIDTH						2
#define T_D2D_ANT_M_MASK						MASK(T_D2D_ANT_M_WIDTH, T_D2D_ANT_M_SHIFT)

/* S5M3700X_D7_DCTR_TEST7 */
#define T_D2A_IN_JD_SHIFT						0
#define T_D2A_IN_JD_WIDTH						2
#define T_D2A_IN_JD_MASK						MASK(T_D2A_IN_JD_WIDTH, T_D2A_IN_JD_SHIFT)

/* S5M3700X_D8_DCTR_DBNC1 */
#define JACK_DBNC_IN_SHIFT						4
#define JACK_DBNC_IN_WIDTH						4
#define JACK_DBNC_IN_MASK						MASK(JACK_DBNC_IN_WIDTH, JACK_DBNC_IN_SHIFT)

#define JACK_DBNC_OUT_SHIFT						0
#define JACK_DBNC_OUT_WIDTH						4
#define JACK_DBNC_OUT_MASK						MASK(JACK_DBNC_OUT_WIDTH, JACK_DBNC_OUT_SHIFT)

#define JACK_DBNC_MAX							0xF

/* S5M3700X_D9_DCTR_DBNC2 */
#define JACK_DBNC_INT_IN_SHIFT					4
#define JACK_DBNC_INT_IN_WIDTH					4
#define JACK_DBNC_INT_IN_MASK					MASK(JACK_DBNC_INT_IN_WIDTH, JACK_DBNC_INT_IN_SHIFT)

#define JACK_DBNC_INT_OUT_SHIFT					0
#define JACK_DBNC_INT_OUT_WIDTH					4
#define JACK_DBNC_INT_OUT_MASK					MASK(JACK_DBNC_INT_OUT_WIDTH, JACK_DBNC_INT_OUT_SHIFT)

#define JACK_DBNC_INT_MAX					0xF

/* S5M3700X_DA_DCTR_DBNC3 */
#define ANT_LDET_DBNC_IN_SHIFT					4
#define ANT_LDET_DBNC_IN_WIDTH					4
#define ANT_LDET_DBNC_IN_MASK					MASK(ANT_LDET_DBNC_IN_WIDTH, ANT_LDET_DBNC_IN_SHIFT)

#define ANT_LDET_DBNC_OUT_SHIFT					0
#define ANT_LDET_DBNC_OUT_WIDTH					4
#define ANT_LDET_DBNC_OUT_MASK					MASK(ANT_LDET_DBNC_OUT_WIDTH, ANT_LDET_DBNC_OUT_SHIFT)

/* S5M3700X_DB_DCTR_DBNC4 */
#define ANT_MDET_DBNC_IN_SHIFT					4
#define ANT_MDET_DBNC_IN_WIDTH					4
#define ANT_MDET_DBNC_IN_MASK					MASK(ANT_MDET_DBNC_IN_WIDTH, ANT_MDET_DBNC_IN_SHIFT)

#define ANT_MDET_DBNC_OUT_SHIFT					0
#define ANT_MDET_DBNC_OUT_WIDTH					4
#define ANT_MDET_DBNC_OUT_MASK					MASK(ANT_MDET_DBNC_OUT_WIDTH, ANT_MDET_DBNC_OUT_SHIFT)

/* S5M3700X_DD_DCTR_DBNC6 */
#define CTMD_BTN_DBNC_SHIFT						0
#define CTMD_BTN_DBNC_WIDTH						4
#define CTMD_BTN_DBNC_MASK						MASK(CTMD_BTN_DBNC_WIDTH, CTMD_BTN_DBNC_SHIFT)

/* S5M3700X_DE_DCTR_DLY1 */
#define D2A_IN_JD_DELAY_SHIFT					2
#define D2A_IN_JD_DELAY_WIDTH					2
#define D2A_IN_JD_DELAY_MASK					MASK(D2A_IN_JD_DELAY_WIDTH, D2A_IN_JD_DELAY_SHIFT)

/* S5M3700X_DF_DCTR_DLY2 */
#define CNT_POLLING_SHIFT						0
#define CNT_POLLING_WIDTH						2
#define CNT_POLLING_MASK						MASK(CNT_POLLING_WIDTH, CNT_POLLING_SHIFT)

/* S5M3700X_E0_DCTR_FSM1 */
#define AP_ETC_SHIFT							7
#define AP_ETC_MASK								BIT(AP_ETC_SHIFT)

#define AP_POLLING_SHIFT						6
#define AP_POLLING_MASK							BIT(AP_POLLING_SHIFT)

#define AP_CJI_SHIFT							5
#define AP_CJI_MASK								BIT(AP_CJI_SHIFT)

#define AP_JO_SHIFT								4
#define AP_JO_MASK								BIT(AP_JO_SHIFT)

#define EN_FIVE_SHIFT							1
#define EN_FIVE_MASK							BIT(EN_FIVE_SHIFT)

/* S5M3700X_E1_DCTR_FSM2 */
#define EN_MDET_JO_SHIFT						0
#define EN_MDET_JO_MASK							BIT(EN_MDET_JO_SHIFT)

/* S5M3700X_E3_DCTR_FSM4 */
#define T_JACK_STATE_SHIFT						6
#define T_JACK_STATE_MASK						BIT(T_JACK_STATE_SHIFT)

#define POLE_VALUE_SHIFT						4
#define POLE_VALUE_WIDTH						2
#define POLE_VALUE_MASK							MASK(POLE_VALUE_WIDTH, POLE_VALUE_SHIFT)

#define POLE_VALUE_OMTP							1
#define POLE_VALUE_AUX							1
#define POLE_VALUE_3POLE						2
#define POLE_VALUE_4POLE						3

/* S5M3700X_E4_DCTR_FSM5 */
#define GPMUX_WTP_SHIFT							2
#define GPMUX_WTP_WIDTH							2
#define GPMUX_WTP_MASK							MASK(GPMUX_WTP_WIDTH, GPMUX_WTP_SHIFT)

#define GPMUX_POLE_SHIFT						0
#define GPMUX_POLE_WIDTH						2
#define GPMUX_POLE_MASK							MASK(GPMUX_POLE_WIDTH, GPMUX_POLE_SHIFT)

#define POLE_MIC_DET							0
#define POLE_HPG_DET							1
#define POLE_HPL_DET							2
#define POLE_IMP_DET							3

/* S5M3700X_E5_DCTR_FSM6 */
#define AP_REREAD_WTP_SHIFT						7
#define AP_REREAD_WTP_MASK						BIT(AP_REREAD_WTP_SHIFT)

#define AP_REREAD_POLE_SHIFT					6
#define AP_REREAD_POLE_MASK						BIT(AP_REREAD_POLE_SHIFT)

#define AP_REREAD_BTN_SHIFT						5
#define AP_REREAD_BTN_MASK						BIT(AP_REREAD_BTN_SHIFT)

/* S5M3700X_E7_DCTR_IMP1 */
#define CNT_IMP_OPT1_SHIFT						0
#define CNT_IMP_OPT1_WIDTH						3
#define CNT_IMP_OPT1_MASK						MASK(CNT_IMP_OPT1_WIDTH, CNT_IMP_OPT1_SHIFT)

/* S5M3700X_E9_DCTR_IMP3 */
#define CNT_IMP_OPT5_SHIFT						0
#define CNT_IMP_OPT5_WIDTH						3
#define CNT_IMP_OPT5_MASK						MASK(CNT_IMP_OPT5_WIDTH, CNT_IMP_OPT5_SHIFT)

/* S5M3700X_EA_DCTR_IMP4 */
#define IMP_SKIP_OPT_SHIFT						7
#define IMP_SKIP_OPT_MASK						BIT(IMP_SKIP_OPT_SHIFT)

#define CNT_IMP_OPT7_SHIFT						0
#define CNT_IMP_OPT7_WIDTH						3
#define CNT_IMP_OPT7_MASK						MASK(CNT_IMP_OPT7_WIDTH, CNT_IMP_OPT7_SHIFT)

/* S5M3700X_EB_DCTR_IMP5 */
#define CNT_IMP_OPT8_SHIFT						4
#define CNT_IMP_OPT8_WIDTH						3
#define CNT_IMP_OPT8_MASK						MASK(CNT_IMP_OPT8_WIDTH, CNT_IMP_OPT8_SHIFT)

/* S5M3700X_EC_DCTR_IMP6 */
#define T_IMP_SEQ_SHIFT							7
#define T_IMP_SEQ_MASK							BIT(T_IMP_SEQ_SHIFT)

#define CNT_IMP_OPT10_SHIFT						4
#define CNT_IMP_OPT10_WIDTH                     3
#define CNT_IMP_OPT10_MASK						MASK(CNT_IMP_OPT10_WIDTH, CNT_IMP_OPT10_SHIFT)

#define CNT_IMP_OPT11_SHIFT						0
#define CNT_IMP_OPT11_WIDTH						3
#define CNT_IMP_OPT11_MASK						MASK(CNT_IMP_OPT11_WIDTH, CNT_IMP_OPT11_SHIFT)

/* S5M3700X_ED_DCTR_IMP7 */
#define EN_IMP_CAL_D_SHIFT						3
#define EN_IMP_CAL_D_MASK						BIT(EN_IMP_CAL_D_SHIFT)

/* S5M3700X_EE_ACTR_ETC1 */
#define SEL_MDET_JO_SIG_SHIFT					5
#define SEL_MDET_JO_SIG_MASK					BIT(SEL_MDET_JO_SIG_SHIFT)

#define IMP_VAL_SHIFT							0
#define IMP_VAL_WIDTH							2
#define IMP_VAL_MASK							MASK(IMP_VAL_WIDTH, IMP_VAL_SHIFT)

/* S5M3700X_F0_STATUS1 */
#define JACK_DET_SHIFT							4
#define JACK_DET_MASK							BIT(JACK_DET_SHIFT)

#define ANT_MDET_DET_SHIFT						3
#define ANT_MDET_DET_MASK						BIT(ANT_MDET_DET_SHIFT)

#define BTN_DET_SHIFT							0
#define BTN_DET_MASK							BIT(BTN_DET_SHIFT)

/* S5M3700X_FA_STATUS11 */
#define BTN_STATE_SHIFT							1
#define BTN_STATE_WIDTH							3
#define BTN_STATE_MASK							MASK(BTN_STATE_WIDTH, BTN_STATE_SHIFT)

/* S5M3700X_FD_ACTR_GP */
#define T_EN_AVG_START_SHIFT					7
#define T_EN_AVG_START_MASK						BIT(T_EN_AVG_START_SHIFT)

#define EN_AVG_START_SHIFT						6
#define EN_AVG_START_MASK						BIT(EN_AVG_START_SHIFT)

#define T_PDB_GPADC_SHIFT						5
#define T_PDB_GPADC_MASK						BIT(T_PDB_GPADC_SHIFT)

#define PDB_GPADC_SHIFT							4
#define PDB_GPADC_MASK							BIT(PDB_GPADC_SHIFT)

#define T_CTMP_GPADCIN_SHIFT					3
#define T_CTMP_GPADCIN_MASK						BIT(T_CTMP_GPADCIN_SHIFT)

#define CTMP_GPADCIN_SHIFT						0
#define CTMP_GPADCIN_WIDTH						3
#define CTMP_GPADCIN_MASK						MASK(CTMP_GPADCIN_WIDTH, CTMP_GPADCIN_SHIFT)

#define GPADC_HPOUTR							2
#define GPADC_HPOUTL							3
#define GPADC_MDET								4
#define GPADC_GDET								5
#define GPADC_LDET								6
#define GPADC_IMP								7

/* S5M3700X_FE_DCTR_GP1 */
#define T_CNT_ADC_DIV_SHIFT						7
#define T_CNT_ADC_DIV_MASK						BIT(T_CNT_ADC_DIV_SHIFT)

#define CNT_ADC_DIV_SHIFT						4
#define CNT_ADC_DIV_WIDTH						3
#define CNT_ADC_DIV_MASK						MASK(CNT_ADC_DIV_WIDTH, CNT_ADC_DIV_SHIFT)

#define CNT_GPADC_DUMP_SHIFT					0
#define CNT_GPADC_DUMP_WIDTH					3
#define CNT_GPADC_DUMP_MASK						MASK(CNT_GPADC_DUMP_WIDTH, CNT_GPADC_DUMP_SHIFT)

/* S5M3700X_FF_DCTR_GP2 */
#define T_ADC_STARTTIME_SHIFT					4
#define T_ADC_STARTTIME_MASK					BIT(T_ADC_STARTTIME_SHIFT)

#define ADC_STARTTIME_SHIFT						0
#define ADC_STARTTIME_WIDTH						4
#define ADC_STARTTIME_MASK						MASK(ADC_STARTTIME_WIDTH, ADC_STARTTIME_SHIFT)

/* S5M3700X_108_PD_VTS */
#define PDB_VTS_IGEN_SHIFT						7
#define PDB_VTS_IGEN_MASK						BIT(PDB_VTS_IGEN_SHIFT)

#define PDB_VTS_BST_CH1_SHIFT					6
#define PDB_VTS_BST_CH1_MASK					BIT(PDB_VTS_BST_CH1_SHIFT)

#define PDB_VTS_DSM_CH2_SHIFT					5
#define PDB_VTS_DSM_CH2_MASK					BIT(PDB_VTS_DSM_CH2_SHIFT)

#define RESETB_VTS_DSM_CH1_SHIFT				4
#define RESETB_VTS_DSM_CH1_MASK					BIT(RESETB_VTS_DSM_CH1_SHIFT)

#define RESETB_VTS_DSM_CH2_SHIFT				3
#define RESETB_VTS_DSM_CH2_MASK					BIT(RESETB_VTS_DSM_CH2_SHIFT)

#define VTS_DATA_PHASE_SHIFT					0
#define VTS_DATA_PHASE_MASK						BIT(VTS_DATA_PHASE_SHIFT)

/* S5M3700X_109_CTRL_VTS1 */
#define CTMI_VTS_INT1_SHIFT						5
#define CTMI_VTS_INT1_WIDTH						3
#define CTMI_VTS_INT1_MASK						MASK(CTMI_VTS_INT1_WIDTH, CTMI_VTS_INT1_SHIFT)

#define CTMI_VTS_INT2_SHIFT						1
#define CTMI_VTS_INT2_WIDTH						3
#define CTMI_VTS_INT2_MASK						MASK(CTMI_VTS_INT2_WIDTH, CTMI_VTS_INT2_SHIFT)

/* S5M3700X_10A_CTRL_VTS2 */
#define CTMI_VTS_INT3_SHIFT						5
#define CTMI_VTS_INT3_WIDTH						3
#define CTMI_VTS_INT3_MASK						MASK(CTMI_VTS_INT3_WIDTH, CTMI_VTS_INT3_SHIFT)

#define CTMI_VTS_INT4_SHIFT						1
#define CTMI_VTS_INT4_WIDTH						3
#define CTMI_VTS_INT4_MASK						MASK(CTMI_VTS_INT4_WIDTH, CTMI_VTS_INT4_SHIFT)

#define CTMI_VTS_0								0
#define CTMI_VTS_1								1
#define CTMI_VTS_2								2
#define CTMI_VTS_3								3
#define CTMI_VTS_4								4
#define CTMI_VTS_5								5
#define CTMI_VTS_6								6
#define CTMI_VTS_7								7

/* S5M3700X_10B_CTRL_VTS3 */
#define CTMF_VTS_LPF_CH1_SHIFT					5
#define CTMF_VTS_LPF_CH1_WIDTH					3
#define CTMF_VTS_LPF_CH1_MASK					MASK(CTMF_VTS_LPF_CH1_WIDTH, CTMF_VTS_LPF_CH1_SHIFT)

#define CTMF_VTS_LPF_CH2_SHIFT					1
#define CTMF_VTS_LPF_CH2_WIDTH					3
#define CTMF_VTS_LPF_CH2_MASK					MASK(CTMF_VTS_LPF_CH2_WIDTH, CTMF_VTS_LPF_CH2_SHIFT)

#define VTS_LPF_12K								0
#define VTS_LPF_48K								2
#define VTS_LPF_72K								6

/* S5M3700X_110_BST_PDB */
#define PDB_BST1_SHIFT							7
#define PDB_BST1_MASK							BIT(PDB_BST1_SHIFT)

#define PDB_BST2_SHIFT							6
#define PDB_BST2_MASK							BIT(PDB_BST2_SHIFT)

#define PDB_BST3_SHIFT							5
#define PDB_BST3_MASK							BIT(PDB_BST3_SHIFT)

#define PDB_BST4_SHIFT							4
#define PDB_BST4_MASK							BIT(PDB_BST4_SHIFT)

#define PDB_MIC_IGEN_SHIFT						0
#define PDB_MIC_IGEN_MASK						BIT(PDB_MIC_IGEN_SHIFT)

/* S5M3700X_111_BST1 */
#define EN_BST1_LPM_SHIFT						3
#define EN_BST1_LPM_MASK						BIT(EN_BST1_LPM_SHIFT)

#define EN_BST2_LPM_SHIFT						2
#define EN_BST2_LPM_MASK						BIT(EN_BST2_LPM_SHIFT)

#define EN_BST3_LPM_SHIFT						1
#define EN_BST3_LPM_MASK						BIT(EN_BST3_LPM_SHIFT)

#define EN_BST4_LPM_SHIFT						0
#define EN_BST4_LPM_MASK						BIT(EN_BST4_LPM_SHIFT)

/* S5M3700X_112_BST2 */
#define EN_BST1_FASTSET_SHIFT					3
#define EN_BST1_FASTSET_MASK					BIT(EN_BST1_FASTSET_SHIFT)

#define EN_BST2_FASTSET_SHIFT					2
#define EN_BST2_FASTSET_MASK					BIT(EN_BST2_FASTSET_SHIFT)

#define EN_BST3_FASTSET_SHIFT					1
#define EN_BST3_FASTSET_MASK					BIT(EN_BST3_FASTSET_SHIFT)

#define EN_BST4_FASTSET_SHIFT					0
#define EN_BST4_FASTSET_MASK					BIT(EN_BST4_FASTSET_SHIFT)

/* S5M3700X_113_BST3 */
/* S5M3700X_114_BST4 */
#define CTVOL_BST_SHIFT							4
#define CTVOL_BST_WIDTH							4
#define CTVOL_BST_MASK							MASK(CTVOL_BST_WIDTH, CTVOL_BST_SHIFT)

#define CTVOL_BST_MAXNUM						0x0B

#define CTMR_BSTR1_SHIFT						2
#define CTMR_BSTR1_WIDTH						2
#define CTMR_BSTR1_MASK							MASK(CTMR_BSTR1_WIDTH, CTMR_BSTR1_SHIFT)

#define CTMR_BSTR2_SHIFT						0
#define CTMR_BSTR2_WIDTH						2
#define CTMR_BSTR2_MASK							MASK(CTMR_BSTR2_WIDTH, CTMR_BSTR2_SHIFT)

#define CTMR_BSTR_0								0
#define CTMR_BSTR_1								1
#define CTMR_BSTR_2								2
#define CTMR_BSTR_3								3

/* S5M3700X_115_DSM1 */
#define PDB_DSML_SHIFT							7
#define PDB_DSML_MASK							BIT(PDB_DSML_SHIFT)

#define RESETB_INTL_SHIFT						6
#define RESETB_INTL_MASK						BIT(RESETB_INTL_SHIFT)

#define RESETB_QUANTL_SHIFT						5
#define RESETB_QUANTL_MASK						BIT(RESETB_QUANTL_SHIFT)

#define EN_ISIBLKL_SHIFT						4
#define EN_ISIBLKL_MASK							BIT(EN_ISIBLKL_SHIFT)

#define EN_CHOPL_SHIFT							3
#define EN_CHOPL_MASK							BIT(EN_CHOPL_SHIFT)

#define EN_CHOPC_SHIFT							2
#define EN_CHOPC_MASK							BIT(EN_CHOPC_SHIFT)

#define EN_CHOPR_SHIFT							1
#define EN_CHOPR_MASK							BIT(EN_CHOPR_SHIFT)

#define CAL_SKIP_SHIFT							0
#define CAL_SKIP_MASK							BIT(CAL_SKIP_SHIFT)

/* S5M3700X_117_DSM2 */
#define PDB_DSMC_SHIFT							7
#define PDB_DSMC_MASK							BIT(PDB_DSMC_SHIFT)

#define RESETB_INTC_SHIFT						6
#define RESETB_INTC_MASK						BIT(RESETB_INTC_SHIFT)

#define RESETB_QUANTC_SHIFT						5
#define RESETB_QUANTC_MASK						BIT(RESETB_QUANTC_SHIFT)

#define EN_ISIBLKC_SHIFT						4
#define EN_ISIBLKC_MASK							BIT(EN_ISIBLKC_SHIFT)

/* S5M3700X_119_DSM3 */
#define PDB_DSMR_SHIFT							7
#define PDB_DSMR_MASK							BIT(PDB_DSMR_SHIFT)

#define RESETB_INTR_SHIFT						6
#define RESETB_INTR_MASK						BIT(RESETB_INTR_SHIFT)

#define RESETB_QUANTR_SHIFT						5
#define RESETB_QUANTR_MASK						BIT(RESETB_QUANTR_SHIFT)

#define EN_ISIBLKR_SHIFT						4
#define EN_ISIBLKR_MASK							BIT(EN_ISIBLKR_SHIFT)

/* S5M3700X_11D_BST_OP */
#define SEL_MIC34_SHIFT							2
#define SEL_MIC34_MASK							BIT(SEL_MIC34_SHIFT)

/* S5M3700X_11E_CTRL_MIC_I1 */
#define CTMI_MIC_BST_SHIFT						5
#define CTMI_MIC_BST_WIDTH						3
#define CTMI_MIC_BST_MASK						MASK(CTMI_MIC_BST_WIDTH, CTMI_MIC_BST_SHIFT)

#define CTMI_MIC_INT1_SHIFT						2
#define CTMI_MIC_INT1_WIDTH						3
#define CTMI_MIC_INT1_MASK						MASK(CTMI_MIC_INT1_WIDTH, CTMI_MIC_INT1_SHIFT)

#define MIC_VAL_0								0
#define MIC_VAL_1								1
#define MIC_VAL_2								2
#define MIC_VAL_3								3
#define MIC_VAL_4								4
#define MIC_VAL_5								5
#define MIC_VAL_6								6
#define MIC_VAL_7								7

/* S5M3700X_125_RSVD */
#define RSVD_SHIFT								6
#define RSVD_WIDTH								2
#define RSVD_MASK								MASK(RSVD_WIDTH, RSVD_SHIFT)

#define SEL_CLK_LCP_SHIFT						5
#define SEL_CLK_LCP_MASK						BIT(SEL_CLK_LCP_SHIFT)

#define SEL_BST1_SHIFT							4
#define SEL_BST1_MASK							BIT(SEL_BST1_SHIFT)

#define SEL_BST2_SHIFT							3
#define SEL_BST2_MASK							BIT(SEL_BST2_SHIFT)

#define SEL_BST3_SHIFT							2
#define SEL_BST3_MASK							BIT(SEL_BST3_SHIFT)

#define SEL_BST4_SHIFT							1
#define SEL_BST4_MASK							BIT(SEL_BST4_SHIFT)

#define SEL_BST_IN_VCM_SHIFT					0
#define SEL_BST_IN_VCM_MASK						BIT(SEL_BST_IN_VCM_SHIFT)

/* S5M3700X_130_PD_IDAC1 */
#define EN_IDAC1_OUT_SHIFT						7
#define EN_IDAC1_OUT_MASK						BIT(EN_IDAC1_OUT_SHIFT)

#define EN_IDAC2_OUT_SHIFT						6
#define EN_IDAC2_OUT_MASK						BIT(EN_IDAC2_OUT_SHIFT)

#define EN_IDAC1_LS_SHIFT						5
#define EN_IDAC1_LS_MASK						BIT(EN_IDAC1_LS_SHIFT)

#define EN_IDAC2_LS_SHIFT						4
#define EN_IDAC2_LS_MASK						BIT(EN_IDAC2_LS_SHIFT)

#define PDB_IDAC1_SHIFT							3
#define PDB_IDAC1_MASK							BIT(PDB_IDAC1_SHIFT)

#define PDB_IDAC2_SHIFT							2
#define PDB_IDAC2_MASK							BIT(PDB_IDAC2_SHIFT)

#define PDB_IDAC1_AMP_SHIFT						1
#define PDB_IDAC1_AMP_MASK						BIT(PDB_IDAC1_AMP_SHIFT)

#define PDB_IDAC2_AMP_SHIFT						0
#define PDB_IDAC2_AMP_MASK						BIT(PDB_IDAC2_AMP_SHIFT)

/* S5M3700X_131_PD_IDAC2 */
#define PDB_IDAC12_CH_SHIFT						5
#define PDB_IDAC12_CH_MASK						BIT(PDB_IDAC12_CH_SHIFT)

#define PDB_IDAC12_IGEN_PN_SHIFT				4
#define PDB_IDAC12_IGEN_PN_MASK					BIT(PDB_IDAC12_IGEN_PN_SHIFT)

#define PDB_IDAC12_IGEN_SHIFT					3
#define PDB_IDAC12_IGEN_MASK					BIT(PDB_IDAC12_IGEN_SHIFT)

#define PDB_IDAC12_IGEN_AMP1_SHIFT				2
#define PDB_IDAC12_IGEN_AMP1_MASK				BIT(PDB_IDAC12_IGEN_AMP1_SHIFT)

#define PDB_IDAC12_IGEN_AMP2_SHIFT				1
#define PDB_IDAC12_IGEN_AMP2_MASK				BIT(PDB_IDAC12_IGEN_AMP2_SHIFT)

#define PDB_IDAC12_IGEN_AMP3_SHIFT				0
#define PDB_IDAC12_IGEN_AMP3_MASK				BIT(PDB_IDAC12_IGEN_AMP3_SHIFT)

/* S5M3700X_132_PD_IDAC3 */
#define EN_IDAC3_OUTTIE_SHIFT					7
#define EN_IDAC3_OUTTIE_MASK					BIT(EN_IDAC3_OUTTIE_SHIFT)

#define EN_IDAC3_OUT_SHIFT						6
#define EN_IDAC3_OUT_MASK						BIT(EN_IDAC3_OUT_SHIFT)

#define EN_IDAC3_LS_SHIFT						5
#define EN_IDAC3_LS_MASK						BIT(EN_IDAC3_LS_SHIFT)

#define PDB_IDAC3_IGEN_PN_SHIFT					4
#define PDB_IDAC3_IGEN_PN_MASK					BIT(PDB_IDAC3_IGEN_PN_SHIFT)

#define PDB_IDAC3_IGEN_SHIFT					3
#define PDB_IDAC3_IGEN_MASK						BIT(PDB_IDAC3_IGEN_SHIFT)

#define PDB_IDAC3_IGEN_AMP1_SHIFT				2
#define PDB_IDAC3_IGEN_AMP1_MASK				BIT(PDB_IDAC3_IGEN_AMP1_SHIFT)

#define PDB_IDAC3_IGEN_AMP2_SHIFT				1
#define PDB_IDAC3_IGEN_AMP2_MASK				BIT(PDB_IDAC3_IGEN_AMP2_SHIFT)

#define PDB_IDAC3_IGEN_AMP3_SHIFT				0
#define PDB_IDAC3_IGEN_AMP3_MASK				BIT(PDB_IDAC3_IGEN_AMP3_SHIFT)

/* S5M3700X_135_PD_IDAC4 */
#define EN_IDAC1_OUTTIE_SHIFT					7
#define EN_IDAC1_OUTTIE_MASK					BIT(EN_IDAC1_OUTTIE_SHIFT)

#define EN_IDAC2_OUTTIE_SHIFT					6
#define EN_IDAC2_OUTTIE_MASK					BIT(EN_IDAC2_OUTTIE_SHIFT)

#define EN_OUTTIEL_SHIFT						1
#define EN_OUTTIEL_MASK							BIT(EN_OUTTIEL_SHIFT)

#define EN_OUTTIER_SHIFT						0
#define EN_OUTTIER_MASK							BIT(EN_OUTTIER_SHIFT)

/* S5M3700X_138_PD_HP */
#define PDB_HP_BIAS_SHIFT						7
#define PDB_HP_BIAS_MASK						BIT(PDB_HP_BIAS_SHIFT)

#define PDB_HP_1STL_SHIFT						6
#define PDB_HP_1STL_MASK						BIT(PDB_HP_1STL_SHIFT)

#define PDB_HP_2NDL_SHIFT						5
#define PDB_HP_2NDL_MASK						BIT(PDB_HP_2NDL_SHIFT)

#define PDB_HP_DRVL_SHIFT						4
#define PDB_HP_DRVL_MASK						BIT(PDB_HP_DRVL_SHIFT)

#define PDB_HP_1STR_SHIFT						2
#define PDB_HP_1STR_MASK						BIT(PDB_HP_1STR_SHIFT)

#define PDB_HP_2NDR_SHIFT						1
#define PDB_HP_2NDR_MASK						BIT(PDB_HP_2NDR_SHIFT)

#define PDB_HP_DRVR_SHIFT						0
#define PDB_HP_DRVR_MASK						BIT(PDB_HP_DRVR_SHIFT)

/* S5M3700X_139_PD_SURGE */
#define PDB_SURGE_REGL_SHIFT					7
#define PDB_SURGE_REGL_MASK						BIT(PDB_SURGE_REGL_SHIFT)

#define PDB_SURGE_POS_NEGL_SHIFT				6
#define PDB_SURGE_POS_NEGL_MASK					BIT(PDB_SURGE_POS_NEGL_SHIFT)

#define PDB_SURGE_NEGL_SHIFT					5
#define PDB_SURGE_NEGL_MASK						BIT(PDB_SURGE_NEGL_SHIFT)

#define EN_SURGEL_SHIFT							4
#define EN_SURGEL_MASK							BIT(EN_SURGEL_SHIFT)

#define PDB_SURGE_REGR_SHIFT					3
#define PDB_SURGE_REGR_MASK						BIT(PDB_SURGE_REGR_SHIFT)

#define PDB_SURGE_POS_NEGR_SHIFT				2
#define PDB_SURGE_POS_NEGR_MASK					BIT(PDB_SURGE_POS_NEGR_SHIFT)

#define PDB_SURGE_NEGR_SHIFT					1
#define PDB_SURGE_NEGR_MASK						BIT(PDB_SURGE_NEGR_SHIFT)

#define EN_SURGER_SHIFT							0
#define EN_SURGER_MASK							BIT(EN_SURGER_SHIFT)

/* S5M3700X_13A_POP_HP */
#define EN_UHQA_SHIFT							7
#define EN_UHQA_MASK							BIT(EN_UHQA_SHIFT)

#define EN_HP_BODY_SHIFT						6
#define EN_HP_BODY_MASK							BIT(EN_HP_BODY_SHIFT)

#define SEL_RES_SUB_SHIFT						5
#define SEL_RES_SUB_MASK						BIT(SEL_RES_SUB_SHIFT)

#define CTRF_HPZ_SHIFT							4
#define CTRF_HPZ_MASK							BIT(CTRF_HPZ_SHIFT)

#define EN_HP_PDN_SHIFT							0
#define EN_HP_PDN_MASK							BIT(EN_HP_PDN_SHIFT)

/* S5M3700X_13B_OCP_HP */
#define EN_HP_OCPL_SHIFT						1
#define EN_HP_OCPL_MASK							BIT(EN_HP_OCPL_SHIFT)

#define EN_HP_OCPR_SHIFT						0
#define EN_HP_OCPR_MASK							BIT(EN_HP_OCPR_SHIFT)

/* S5M3700X_142_REF_SURGE */
#define CTRV_SURGE_REFP_SHIFT					5
#define CTRV_SURGE_REFP_WIDTH					3
#define CTRV_SURGE_REFP_MASK					MASK(CTRV_SURGE_REFP_WIDTH, CTRV_SURGE_REFP_SHIFT)

#define CTRV_SURGE_REFN_SHIFT					5
#define CTRV_SURGE_REFN_WIDTH					3
#define CTRV_SURGE_REFN_MASK					MASK(CTRV_SURGE_REFN_WIDTH, CTRV_SURGE_REFN_SHIFT)

/* S5M3700X_149_PD_EP */
#define EN_EP_OCP_SHIFT							5
#define EN_EP_OCP_MASK							BIT(EN_EP_OCP_SHIFT)

#define PDB_EP_BIAS_SHIFT						3
#define PDB_EP_BIAS_MASK						BIT(PDB_EP_BIAS_SHIFT)

#define PDB_EP_1ST_SHIFT						2
#define PDB_EP_1ST_MASK							BIT(PDB_EP_1ST_SHIFT)

#define PDB_EP_2ND_SHIFT						1
#define PDB_EP_2ND_MASK							BIT(PDB_EP_2ND_SHIFT)

#define PDB_EP_DRV_SHIFT						0
#define PDB_EP_DRV_MASK							BIT(PDB_EP_DRV_SHIFT)

/* S5M3700X_14A_PD_LO */
#define PDB_LO_BIAS_SHIFT						3
#define PDB_LO_BIAS_MASK						BIT(PDB_LO_BIAS_SHIFT)

#define PDB_LO_1ST_SHIFT						2
#define PDB_LO_1ST_MASK							BIT(PDB_LO_1ST_SHIFT)

#define PDB_LO_2ND_SHIFT						1
#define PDB_LO_2ND_MASK							BIT(PDB_LO_2ND_SHIFT)

#define PDB_LO_DRV_SHIFT						0
#define PDB_LO_DRV_MASK							BIT(PDB_LO_DRV_SHIFT)

/* S5M3700X_153_DA_CP0 */
#define SEL_AUTO_CP1_SHIFT						7
#define SEL_AUTO_CP1_MASK						BIT(SEL_AUTO_CP1_SHIFT)

#define SEL_AUTO_CP2_SHIFT						3
#define SEL_AUTO_CP2_MASK						BIT(SEL_AUTO_CP2_SHIFT)

#define CTMF_CP_CLK2_SHIFT						0
#define CTMF_CP_CLK2_WIDTH						3
#define CTMF_CP_CLK2_MASK						MASK(CTMF_CP_CLK2_WIDTH, CTMF_CP_CLK2_SHIFT)

/* S5M3700X_2B5_CTRL_HP1 */
#define CTRV_HP_OCP_SHIFT						4
#define CTRV_HP_OCP_WIDTH						2
#define CTRV_HP_OCP_MASK						MASK(CTRV_HP_OCP_WIDTH, CTRV_HP_OCP_SHIFT)

#endif /* _S5M3700X_H */
