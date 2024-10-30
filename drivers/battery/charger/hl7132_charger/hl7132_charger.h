#ifndef _HL7132_CHARGER_H_
#define _HL7132_CHARGER_H_

#define BITS(_end, _start)          ((BIT(_end) - BIT(_start)) + BIT(_end))
#define MIN(a, b)                   ((a < b) ? (a):(b))
#define MASK2SHIFT(_mask)           __ffs(_mask)

#define REG_DEVICE_ID					    0x00

#define REG_INT								0x01
#define BIT_STATE_CHG_I			            BIT(7)
#define BIT_REG_I						    BIT(6)
#define BIT_TS_TEMP_I					    BIT(5)
#define BIT_V_OK_I						    BIT(4)
#define BIT_CUR_I							BIT(3)
#define BIT_SHORT_I					        BIT(2)
#define BIT_WDOG_I					        BIT(1)


#define REG_INT_MSK							0x02
#define BIT_STATE_CHG_M			            BIT(7)
#define BIT_REG_M					    	BIT(6)
#define BIT_TS_TEMP_M				        BIT(5)
#define BIT_V_OK_M					        BIT(4)
#define BIT_CUR_M						    BIT(3)
#define BIT_SHORT_I_M				        BIT(2)
#define WDOG_M					            BIT(1)

#define REG_INT_STS_A					    0x03
#define BIT_STATE_CHG_STS                   BITS(7,6)
#define BIT_REG_STS                         BITS(5,2)
#define BIT_TS_TEMP_STS                     BIT(1)

#define REG_INT_STS_B					    0x04
#define BIT_V_NOT_OK_STS			    	BIT(4)
#define BIT_CUR_STS					    	BIT(3)
#define BIT_SHORT_STS				    	BIT(2)
#define BIT_WDOG_STS				    	BIT(1)

#define REG_STATUS_A					    0x05
#define BIT_VIN_OVP_STS						BIT(7)
#define BIT_VIN_UVLO_STS				    BIT(6)
#define BIT_TRACK_OV_STS					BIT(5)
#define BIT_TRACK_UV_STS				    BIT(4)
#define BIT_VBAT_OVP_STS					BIT(3)
#define BIT_VOUT_OVP_STS					BIT(2)
#define BIT_PMID_QUAL_STS					BIT(1)

#define REG_STATUS_B					    0x06
#define BIT_IIN_OCP_STS						BIT(7)
#define BIT_IBAT_OCP_STS					BIT(6)
#define BIT_IIN_UCP_STS						BIT(5)
#define BIT_FET_SHORT_STS					BIT(4)
#define BIT_CFLY_SHORT_STS					BIT(3)
#define BIT_DEV_MODE_STS					BITS(2,1)
#define BIT_THSD_STS                        BIT(0)

#define REG_STATUS_C						0x07
#define BIT_QPUMP_STS                       BIT(7)

#define REG_VBAT_REG						0x08
#define BIT_VBAT_OVP_DIS					BIT(7)
#define BIT_TVBAT_OVP_DEB				    BIT(6)
#define BITS_VBAT_REG_TH					BITS(5,0)

#define REG_IBAT_REG						0x0A
#define BIT_IBAT_OCP_DIS					BIT(7)
#define BIT_TIBAT_OCP_DEB				    BIT(6)
#define BITS_IBAT_REG_TH					BITS(5, 0)


#define REG_VIN_OVP						    0x0C
#define BIT_VIN_PD_CFG					    BIT(7)
#define BIT_VIN_OVP_DIS					    BIT(6)
#define BITS_VIN_OVP						BITS(3, 0)

#define REG_IIN_REG						    0x0E
#define BIT_IIN_OCP_DIS					    BIT(7)
#define BITS_IIN_REG_TH					    BITS(6, 0)

#define REG_IIN_OC_ALM					    0x0F
#define BIT_TIIN_OC_DEB					    BIT(7)
#define BIT_IIN_ADJ_3						BIT(3)
#define BIT_IIN_ADJ_2						BIT(2)
#define BIT_IIN_ADJ_1						BIT(1)
#define BIT_IIN_ADJ_0						BIT(0)

#define REG_CTRL10_0					0x10
#define BIT_TDIE_REG_DIS					BIT(7)
#define BIT_IIN_REG_DIS						BIT(6)
#define BITS_TDIE_REG_TH					BITS(5, 4)
#define BITS_IIN_OCP_TH						BIT(3, 2)

#define REG_CTRL11_1					    0x11
#define BIT_VBAT_REG_DIS                    BIT(7)
#define BIT_IBAT_REG_DIS                    BIT(6)
#define BITS_VBAT_OVP_TH                    BITS(5,4)
#define BITS_IBAT_OCP_TH                    BITS(3,2)

#define REG_CTRL12_0						0x12
#define BIT_CHG_EN                          BIT(7)
#define BITS_FSW_SET                        BITS(6,3)
#define BIT_UNPLUG_DET_EN                   BIT(2)
#define BITS_IIN_UCP_TH                     BITS(1,0)


#define REG_CTRL13_1						0x13
#define BIT_DEEP_SLEEP_EN                   BIT(7)
#define BIT_R_SENSE_CFG                     BIT(6)
#define BIT_VOUT_OVP_DIS                    BIT(5)
#define BIT_TS_RPOT_EN                      BIT(4)
#define BIT_VIN_UV_SEL                      BIT(3)
#define BIT_AUTO_V_REC_EN                   BIT(2)
#define BIT_AUTO_I_REC_EN                   BIT(1)
#define BIT_AUTO_UCP_EN                     BIT(0)

#define REG_CTRL14_2						0x14
#define BIT_SFT_RST                         BIT(7)
#define BIT_WD_DIS                          BIT(2)
#define BITS_WD_TMR                         BITS(1,0)

#define REG_CTRL15_3						0x15
#define BITS_SYNC_CFG                       BITS(1,0)

#define  REG_TRACK_OV_UV				    0x16
#define BIT_TRACK_OV_DIS                    BIT(7)
#define BIT_TRACK_UV_DIS                    BIT(6)
#define BITS_TRACK_OV                       BITS(5,3)
#define BITS_TRACK_UV                       BITS(2,0)

#define REG_TS0_TH_0						0x17
#define BITS_TS0_TH_LSB                     BITS(7,0)

#define REG_TS0_TH_1						0x18
#define BITS_TS0_TH_MSB                     BITS(1,0)

#define REG_TS1_TH_0						0x19
#define BITS_TS1_TH_0_LSB                   BITS(7,0)

#define REG_TS1_TH_1						0x1A
#define BITS_REG_TS1_TH_1_MSB               BITS(1,0)

#define REG_ADC_CTRL_0					    0x1B
#define BIT_ADC_REG_COPY                    BIT(7)
#define BIT_ADC_MAN_COPY                    BIT(6)
#define BIT_ADC_MODE_CFG                    BIT(3)
#define BIT_ADC_AVG_TIME                    BIT(2)
#define BITS_ADC_AVG_TIME                   BITS(2,1)
#define BIT_ADC_EN                          BIT(0)

#define REG_ADC_CTRL_1					    0x1C
#define BIT_VIN_ADC_DIS                     BIT(7)
#define BIT_IIN_ADC_DIS                     BIT(6)
#define BIT_VBAT_ADC_DIS                    BIT(5)
#define BIT_IBAT_ADC_DIS                    BIT(4)
#define BIT_TS_ADC_DIS                      BIT(3)
#define BIT_TDIE_ADC_DIS                    BIT(2)
#define BIT_VOUT_ADC_DIS                    BIT(1)


#define REG_ADC_VIN_0						0x1D
#define BITS_ADC_VIN_MSB                    BITS(7,0)

#define REG_ADC_VIN_1						0x1E
#define BITS_ADC_VIN_LSB                     BITS(1,0)


#define REG_ADC_IIN_0						0x1F
#define BITS_ADC_IIN_MSB                    BITS(7,0)

#define REG_ADC_IIN_1						0x20
#define BITS_ADC_IIN_LSB                    BITS(1,0)

#define REG_ADC_VBAT_0					    0x21
#define BITS_ADC_VBAT_MSB                   BITS(7,0)

#define REG_ADC_VBAT_1					    0x22
#define BITS_ADC_VBAT_LSB                   BITS(1,0)

#define REG_ADC_IBAT_0					    0x23
#define BITS_ADC_IBAT_MSB                   BITS(7,0)

#define REG_ADC_IBAT_1					    0x24
#define BITS_ADC_IBAT_LSB                   BITS(1,0)


#define REG_ADC_VTS_0						0x25
#define BITS_ADC_VTS_MSB                    BITS(7,0)

#define REG_ADC_VTS_1						0x26
#define BITS_ADC_VTS_LSB                    BITS(1,0)


#define REG_ADC_VOUT_0						0x27
#define BITS_ADC_VOUT_MSB                   BITS(7,0)

#define REG_ADC_VOUT_1						0x28
#define BITS_ADC_VOUT_LSB                   BITS(1,0)

#define REG_ADC_TDIE_0						0x29
#define BITS_ADC_TDIE_MSB                   BITS(7,0)

#define REG_ADC_TDIE_1						0x2A
#define BITS_ADC_TDIE_LSB                   BITS(1,0)


/*i2c regmap init setting */
#define REG_MAX         0x2A
#define HL7132_I2C_NAME "hl7132"
#define HL7132_MODULE_VERSION "17.6.11302023"

#define CONFIG_HALO_PASS_THROUGH
/* #define CONFIG_FG_READING_FOR_CVMODE */

#ifdef CONFIG_FG_READING_FOR_CVMODE
#define HL7132_CVMODE_FG_READING_CHECK_T    2000            //2 seconds
#endif

/*Input Current Limit Default Setting*/
#define HL7132_IIN_CFG_DFT                  2000000         // 2A
#define HL7132_IIN_CFG_MIN                  500000          // 500mA
#define HL7132_IIN_CFG_MAX                  3500000         // 3.5A

/*Charging Current Setting*/
#define HL7132_ICHG_CFG_DFT                 6000000         // 6A
#define HL7132_ICHG_CFG_MIN                 0
#define HL7132_ICHG_CFG_MAX                 8000000         // 8A

/*VBAT REG Default Setting */
#define HL7132_VBAT_REG_DFT                 4340000         // 4.34V
#define HL7132_VBAT_REG_MIN                 3725000         // 3.725V

#define HL7132_IIN_DONE_DFT                 500000          // 500mA

#define HL7132_TS0_TH_DFT                   0x0174
#define HL7132_TS1_TH_DFT                   0x0289
#define HL7132_ADC0_DFT                     0x85            //Enable Manual Mode, Set 8-sample, Enable ADC read

#define HL7132_IIN_CFG_STEP                 50000           //50mA

#ifdef CONFIG_SEC_FACTORY
#define HL7132_TA_VOL_STEP_ADJ_CC           80000           //80mV
#else
#define HL7132_TA_VOL_STEP_ADJ_CC           40000           //40mV
#endif

//IIN_REG_TH (A) = 1A + DEC (6:0) * 50mA
#define HL7132_IIN_TO_HEX(__iin)            ((__iin - 1000000)/HL7132_IIN_CFG_STEP) //1A + DEC (6:0) * 50mA

//VBAT_REG_TH (V) = 4.0V + DEC (5:0) * 10mV
#define HL7132_VBAT_REG_OFFSET			4000000
#define HL7132_VBAT_REG(_vbat_reg)          ((_vbat_reg-HL7132_VBAT_REG_OFFSET)/10000)
//#define HL7132_IIN_OFFSET_CUR               100000          //100mA
/* For Low CC-Current issue, TA has tolerance +- 150mA even high (almost 200mA) */
#define HL7132_IIN_OFFSET_CUR               200000          //200mA

/* pre cc mode ta-vol step*/
#define HL7132_TA_VOL_STEP_PRE_CC           100000  //100mV
#define HL7132_TA_VOL_STEP_PRE_CV           20000   //20mV
/* TA OFFSET V */
//#define HL7132_TA_V_OFFSET                  200000 //200mV
#define HL7132_TA_V_OFFSET                  100000 //100mV //workaround for iin-reg-loop
/* TA TOP-OFF CURRENT */
#define HL7132_TA_TOPOFF_CUR                500000 //500mA
/* PD Message Voltage and Current Step */
#define PD_MSG_TA_VOL_STEP                  20000   // 20mV
#define PD_MSG_TA_CUR_STEP                  50000   // 50mA

/* Maximum TA voltage threshold */
#define HL7132_TA_MAX_VOL                   9800000 // 9800000uV
/* Maximum TA current threshold */
#define HL7132_TA_MAX_CUR                   2450000 // 2450000uA
/* Mimimum TA current threshold */
#define HL7132_TA_MIN_CUR                   1000000 // 1000000uA - PPS minimum current
/* Minimum TA voltage threshold in Preset mode */
#ifdef CONFIG_SEC_FACTORY
#define HL7132_TA_MIN_VOL_PRESET            8800000 //8.8V
#else
#define HL7132_TA_MIN_VOL_PRESET            7500000 //7.5V
#endif
/* Preset TA VOLtage offset*/
#define HL7132_TA_VOL_PRE_OFFSET            300000  // 300mV

/* CCMODE Charging Current offset */ //version13
#define HL7132_IIN_CFG_OFFSET_CC            50000   //50mA

/* Denominator for TA Voltage and Current setting*/
#define HL7132_SEC_DENOM_U_M                1000 // 1000, denominator
/* PPS request Delay Time */
#define HL7132_PPS_PERIODIC_T	            7500	// 7500ms

/* VIN STEP for Register setting */
#define HL7132_VIN_STEP                     15000   //15mV (0 ~ 15.345V)
/* IIN STEP for Register setting */
#define HL7132_IIN_STEP                     4400	// 4.4mA (from 0A to 4.5A in 4.4mA LSB)    //5mA (0 ~ 5.115A) for the evk
/* VBAT STEP for register setting */
#define HL7132_VBAT_STEP                    5000    //5mV (0 ~ 5.115V)
/* VTS STEP for register setting */
#define HL7132_VTS_STEP                     1760    //1.76mV (0 ~ 1.8V)
/* TDIE STEP for register setting */
#define HL7132_TDIE_STEP                    25    // 0.25degreeC (0 ~ 125)
#define HL7132_TDIE_DENOM                   10   // 10, denomitor
/* VOUT STEP for register setting */
#define HL7132_VOUT_STEP					5000	//5mV (0 ~ 5.115V)

/* Workqueue delay time for VBATMIN */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#define HL7132_VBATMIN_CHECK_T              0
#else
#define HL7132_VBATMIN_CHECK_T              1000
#endif
/* Workqueue delay time for CHECK ACTIVE */
#define HL7132_CHECK_ACTIVE_DELAY_T         150     // 150ms
/* Workqueue delay time for CCMODE */
#define HL7132_CCMODE_CHECK_T	            10000	// 10000ms
/* Workqueue delay time for CVMODE */
#define HL7132_CVMODE_CHECK_T 	            10000	// 10000ms

/* Delay Time after PDMSG */
/* change it to 300ms due to SM USB-PD issue */
#define HL7132_PDMSG_WAIT_T	                200		// 200ms

/* Delay Time for WDT */
#define HL7132_BATT_WDT_CONTROL_T           30000   //30S

/* Battery Threshold*/
#define HL7132_DC_VBAT_MIN                  3400000 //3.4V

/* LOG BUFFER for RPI */
#define LOG_BUFFER_ENTRIES	                1024
#define LOG_BUFFER_ENTRY_SIZE	            128

#ifdef CONFIG_HALO_PASS_THROUGH
/* Default IIN default in pass through mode */
#define HL7132_PT_IIN_DFT                   3000000 // 3A
/* Preset TA VOLtage offset in pass through mode */
#define HL7132_PT_VOL_PRE_OFFSET            200000  // 200mV
/* Preset TA Current offset in pass through mode */
#define HL7132_PT_TA_CUR_LOW_OFFSET         200000  // 200mA
/*  Vbat regulation Volatage in pass through mode */
#define HL7132_PT_VBAT_REG                  4400000 //4.4V
/* Workqueue delay time for entering pass through mode */
#define HL7132_PT_ACTIVE_DELAY_T            150     // 150ms
/* Workqueue delay time for pass through mode */
#define HL7132_PTMODE_DELAY_T               10000	// 10000ms
/* Workqueu Delay time for unpluged detection issue in pass through mode */
#define HL7132_PTMODE_UNPLUGED_DETECTION_T	1000	// 1000ms
#endif

/* IIN Range for FSW setting */
#define HL7132_CHECK_IIN_FOR_DEFAULT		1900000 //1.9A
#define HL7132_CHECK_IIN_FOR_800KHZ		1700000 //1.7A
#define HL7132_CHECK_IIN_FOR_600KHZ		1100000 //1.1A
#define HL7132_CHECK_MAX_POWER				30000000 //30W
#define HL7132_CHECK_IIN_FOR_ADDOFFSET		2500000 //2.5A

/*STEP CHARGING SETTING */
//#define HL7132_STEP_CHARGING
#ifdef HL7132_STEP_CHARGING
#define HL7132_STEP_CHARGING_CNT            3
#define HL7132_STEP1_TARGET_IIN             2000000 //2000mA
#define HL7132_STEP2_TARGET_IIN             1575000 //1575mA
#define HL7132_STEP3_TARGET_IIN             1250000 //1250mA

#define HL7132_STEP1_TOPOFF                 HL7132_STEP2_TARGET_IIN     //1575000 //1575mA
#define HL7132_STEP2_TOPOFF                 HL7132_STEP3_TARGET_IIN     //1250000 //1250mA
#define HL7132_STEP3_TOPOFF                 HL7132_TA_TOPOFF_CUR        //500000  //500mA

#define HL7132_STEP1_VBAT_REG               4120000 //4.12V
#define HL7132_STEP2_VBAT_REG               4280000 //4.28V
#define HL7132_STEP3_VBAT_REG               4400000 //4.4V

enum CHARGING_STEP {
	STEP_DIS = 0,
	STEP_ONE = 1,
	STEP_TWO = 2,
	STEP_THREE = 3,
};
#endif

enum {
	ENUM_INT,
	ENUM_INT_MASK,
	ENUM_INT_STS_A,
	ENUM_INT_STS_B,
	ENUM_INT_MAX,
};

/* Switching Frequency */
enum {
	FSW_CFG_500KHZ = 0,
	FSW_CFG_600KHZ,
	FSW_CFG_700KHZ,
	FSW_CFG_800KHZ,
	FSW_CFG_900KHZ,
	FSW_CFG_1000KHZ,
	FSW_CFG_1100KHZ,
	FSW_CFG_1200KHZ,
	FSW_CFG_1300KHZ,
	FSW_CFG_1400KHZ,
	FSW_CFG_1500KHZ,
	FSW_CFG_1600KHZ,
};

/* Watchdong Timer */
enum {
	WDT_DISABLED = 0,
	WDT_ENABLED,
};

enum {
	WD_TMR_2S = 0,
	WD_TMR_4S,
	WD_TMR_8S,
	WD_TMR_16S,
};

/* Increment check for Adjust MODE */
enum {
	INC_NONE,
	INC_TA_VOL,
	INC_TA_CUR,
};

/* TIMER ID for Charging Mode */
enum {
	TIMER_ID_NONE = 0,
	TIMER_VBATMIN_CHECK,
	TIMER_PRESET_DC,
	TIMER_PRESET_CONFIG,
	TIMER_CHECK_ACTIVE,
	TIMER_ADJUST_CCMODE,
	TIMER_ENTER_CCMODE,
	TIMER_CHECK_CCMODE,
	TIMER_CHECK_CVMODE,
	TIMER_PDMSG_SEND,

	TIMER_ADJUST_TAVOL,
	TIMER_ADJUST_TACUR,
#ifdef CONFIG_HALO_PASS_THROUGH
	TIMER_PRESET_PT,
	TIMER_PRESET_CONFIG_PT,
	TIMER_CHECK_PTMODE,
#endif
};

/* DC STATE for matching Timer ID*/
enum {
	DC_STATE_NOT_CHARGING = 0,
	DC_STATE_CHECK_VBAT,
	DC_STATE_PRESET_DC,
	DC_STATE_CHECK_ACTIVE,
	DC_STATE_ADJUST_CC,
	DC_STATE_START_CC,
	DC_STATE_CC_MODE,
	DC_STATE_CV_MODE,
	DC_STATE_CHARGING_DONE,
	DC_STATE_ADJUST_TAVOL,
	DC_STATE_ADJUST_TACUR,
#ifdef CONFIG_HALO_PASS_THROUGH
	DC_STATE_PT_MODE,
	DC_STATE_PRESET_PT,
	DC_STATE_PRESET_CONFIG_PT,
#endif
};

/* ADC Channel */
enum {
	ADCCH_VIN,
	ADCCH_IIN,
	ADCCH_VBAT,
	ADCCH_IBAT,
	ADCCH_VTS,
	ADCCH_VOUT,
	ADCCH_TDIE,
};

/* PD Message Type */
enum {
	PD_MSG_REQUEST_APDO,
	PD_MSG_REQUEST_FIXED_PDO,
};

/* Regulation Loop  */
enum {
	INACTIVE_LOOP   = 0,
	VBAT_REG_LOOP    = 1,
	IIN_REG_LOOP     = 2,
	IBAT_REG_LOOP    = 4,
	T_DIE_REG_LOOP   = 8,
	/* All regulation loop can be triggered at the same time */
	VBAT_IIN_LOOP	= 3,
	VBAT_IBAT_LOOP = 5,
	VBAT_IIN_IBAT_LOOP = 7,
	VBAT_TDIE_LOOP = 9,

	IIN_IBAT_LOOP = 6,
	IIN_TDIE_LOOP = 10,
	IIN_VBAT_TDIE = 11,
	IIN_VBAT_TDIE_IBAT_LOOP = 15,

	IBAT_TDIE_LOOP = 12,
	VBAT_TDIE_IBAT_LOOP = 13,
	IIN_IBAT_TDIE_LOOP = 14,
};

/*DEV STATE */
enum {
	DEV_STATE_RESET = 0,
	DEV_STATE_SHUTDOWN,
	DEV_STATE_STANDBY,
	DEV_STATE_ACTIVE,
};

/*REGMAP CONFIG */
static const struct regmap_config hl7132_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

/* Power Supply Name */
static char *hl7132_supplied_to[] = {
	"hl7132-charger",
};

/*PLATFORM DATA */
struct hl7132_platform_data{
	int irq_gpio;

	unsigned int iin_cfg;
	unsigned int ichfg_cfg;
	unsigned int vbat_reg;
	unsigned int iin_topoff;
	unsigned int vbat_reg_max;

	unsigned int snsres;
	unsigned int fsw_cfg;
	unsigned int ntc_th;
	unsigned int wd_tmr;
	unsigned int ts0_th;
	unsigned int ts1_th;
	unsigned int adc_ctrl0;

	bool ts_prot_en;
	bool all_auto_recovery;
	bool wd_dis;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	char *sec_dc_name;
#endif
#ifdef CONFIG_HALO_PASS_THROUGH
	int pass_through_mode;
#endif
	/* Switching Frequency */
	unsigned int fsw_set;
};

/* DEV - CHIP DATA*/
struct hl7132_charger{
	struct regmap                   *regmap;
	struct device                   *dev;
	struct power_supply             *psy_chg;
	struct mutex                    i2c_lock;
	struct mutex                    lock;
	struct wakeup_source            *monitor_ws;
	struct hl7132_platform_data     *pdata;
	struct i2c_client               *client;
	struct delayed_work             timer_work;
	struct delayed_work             pps_work;

#ifdef HL7132_STEP_CHARGING
	struct delayed_work             step_work;
	unsigned int current_step;
#endif

	struct dentry                   *debug_root;

	u32  debug_address;

	unsigned int chg_state;
	unsigned int reg_state;

	unsigned int charging_state;
	unsigned int dc_state;
	unsigned int timer_id;
	unsigned long timer_period;

	/* ADC CON */
	int adc_vin;
	int adc_iin;
	int adc_vbat;
	int adc_vts;
	int adc_tdie;
	int adc_vout;

	unsigned int iin_cc;
	unsigned int ta_cur;
	unsigned int ta_vol;
	unsigned int ta_objpos;
	unsigned int ta_target_vol;
	unsigned int ta_v_ofs;

	unsigned int new_vbat_reg;
	unsigned int new_iin;
	bool         req_new_vbat_reg;
	bool         req_new_iin;

	unsigned int ta_max_cur;
	unsigned int ta_max_vol;
	unsigned int ta_max_pwr;

	unsigned int ret_state;

	unsigned int prev_iin;
	unsigned int prev_inc;
	bool         prev_dec;

	unsigned int pdo_index;
	unsigned int pdo_max_voltage;
	unsigned int pdo_max_current;

	bool online;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	bool wdt_kick;
	struct delayed_work wdt_control_work;
#endif
	/* flag for abnormal TA conecction */
	bool ab_ta_connected;
	unsigned int ab_try_cnt;
	unsigned int ab_prev_cur;
	unsigned int fault_sts_cnt;
	unsigned int tp_set;

#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
	/* lock for log buffer access */
	struct mutex logbuffer_lock;
	int logbuffer_head;
	int logbuffer_tail;
	u8 *logbuffer[LOG_BUFFER_ENTRIES];
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	int input_current;
	int float_voltage;
	int health_status;
#endif
#ifdef CONFIG_HALO_PASS_THROUGH
	int pass_through_mode;
	bool req_pt_mode;
#endif
#ifdef CONFIG_FG_READING_FOR_CVMODE
	int vbat_fg;
#endif
};

#endif /* _HL7132_CHARGER_H_ */
