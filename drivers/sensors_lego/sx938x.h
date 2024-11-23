/*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*/
#ifndef SX938x_H
#define SX938x_H


#define VENDOR_NAME              "SEMTECH"
#define NOTI_MODULE_NAME         "grip_notifier"

enum ic_num {
	MAIN_GRIP = 0,
	SUB_GRIP,
	SUB2_GRIP,
	WIFI_GRIP,
	GRIP_MAX_CNT
};

const char *grip_name[GRIP_MAX_CNT] = {
	"MAIN",
	"SUB",
	"SUB2",
	"WIFI"
};

const char *device_name[GRIP_MAX_CNT] = {
	"SX9380",
	"SX9380_SUB",
	"SX9380_SUB2",
	"SX9380_WIFI"
};

const char *module_name[GRIP_MAX_CNT] = {
	"grip_sensor",
	"grip_sensor_sub",
	"grip_sensor_sub2",
	"grip_sensor_wifi"
};

/* IrqStat 0:Inactive 1:Active */
#define SX938x_IRQSTAT_RESET_FLAG		0x10
#define SX938x_IRQSTAT_TOUCH_FLAG		0x08
#define SX938x_IRQSTAT_RELEASE_FLAG		0x04
#define SX938x_IRQSTAT_COMPDONE_FLAG		0x02
#define SX938x_IRQSTAT_CONV_FLAG		0x01

#define SX938x_STAT_COMPSTAT_ALL_FLAG (0x02 | 0x04)

/*
*  I2C Registers
*/
//-Interrupt and status
#define SX938x_IRQSTAT_REG		0x00
#define SX938x_STAT_REG			0x01
#define SX938x_IRQ_ENABLE_REG	0x02
#define SX938x_IRQCFG_REG		0x03
//-General control
#define SX938x_GNRL_CTRL0_REG	0x10
#define SX938x_GNRL_CTRL1_REG	0x11
#define SX938x_GNRL_CTRL2_REG	0x12
//-AFE Control - Main - Ref
#define SX938x_AFE_CTRL1_REG		0x21
#define SX938x_AFE_PARAM0_PHR_REG	0x22
#define SX938x_AFE_PARAM1_PHR_REG	0x23
#define SX938x_AFE_PARAM0_PHM_REG	0x24
#define SX938x_AFE_PARAM1_PHM_REG	0x25

//-Advanced Digital Processing control
#define SX938x_PROXCTRL0_PHR_REG	0x40
#define SX938x_PROXCTRL0_PHM_REG	0x41
#define SX938x_PROXCTRL1_REG	0x42
#define SX938x_PROXCTRL2_REG	0x43
#define SX938x_PROXCTRL3_REG	0x44
#define SX938x_PROXCTRL4_REG	0x45
#define SX938x_PROXCTRL5_REG	0x46
//Ref
#define SX938x_REFCORR0_REG		0x60
#define SX938x_REFCORR1_REG		0x61
//Use Filter
#define SX938x_USEFILTER0_REG	0x70
#define SX938x_USEFILTER1_REG	0x71
#define SX938x_USEFILTER2_REG	0x72
#define SX938x_USEFILTER3_REG	0x73
#define SX938x_USEFILTER4_REG	0x74

/*      Sensor Readback */
#define SX938x_USEMSB_PHR			0x90
#define SX938x_USELSB_PHR			0x91
#define SX938x_OFFSETMSB_PHR		0x92
#define SX938x_OFFSETLSB_PHR		0x93

#define SX938x_USEMSB_PHM			0x94
#define SX938x_USELSB_PHM			0x95
#define SX938x_AVGMSB_PHM			0x96
#define SX938x_AVGLSB_PHM			0x97
#define SX938x_DIFFMSB_PHM			0x98
#define SX938x_DIFFLSB_PHM			0x99
#define SX938x_OFFSETMSB_PHM		0x9A
#define SX938x_OFFSETLSB_PHM		0x9B
#define SX938x_USEFILTMSB			0x9E
#define SX938x_USEFILTLSB			0x9F

#define SX938x_SOFTRESET_REG	0xCF
#define SX938x_WHOAMI_REG		0xFA
#define SX938x_REV_REG			0xFE

#define SX938x_IRQSTAT_PROG2_FLAG		0x04
#define SX938x_IRQSTAT_PROG1_FLAG		0x02
#define SX938x_IRQSTAT_PROG0_FLAG		0x01


/* RegStat0  */
#define SX938x_PROXSTAT_FLAG			0x08

/*      SoftReset */
#define SX938x_SOFTRESET				0xDE
#define SX938x_WHOAMI_VALUE				0x82
#define SX938x_REV_VALUE				0x13

struct smtc_reg_data {
	unsigned char reg;
	unsigned char val;
};

enum {
	SX938x_IRQ_ENABLE_REG_IDX,
	SX938x_IRQCFG_REG_IDX,
	SX938x_GNRL_CTRL0_REG_IDX,
	SX938x_GNRL_CTRL1_REG_IDX,
	SX938x_GNRL_CTRL2_REG_IDX,
	SX938x_AFE_CTRL1_REG_IDX,
	SX938x_AFE_PARAM0_PHR_REG_IDX,
	SX938x_AFE_PARAM1_PHR_REG_IDX,
	SX938x_AFE_PARAM0_PHM_REG_IDX,
	SX938x_AFE_PARAM1_PHM_REG_IDX,
	SX938x_PROXCTRL0_PHR_REG_IDX,
	SX938x_PROXCTRL0_PHM_REG_IDX,
	SX938x_PROXCTRL1_REG_IDX,
	SX938x_PROXCTRL2_REG_IDX,
	SX938x_PROXCTRL3_REG_IDX,
	SX938x_PROXCTRL4_REG_IDX,
	SX938x_PROXCTRL5_REG_IDX,
	SX938x_REFCORR0_REG_IDX,
	SX938x_REFCORR1_REG_IDX,
	SX938x_USEFILTER0_REG_IDX,
	SX938x_USEFILTER1_REG_IDX,
	SX938x_USEFILTER2_REG_IDX,
	SX938x_USEFILTER3_REG_IDX,
	SX938x_USEFILTER4_REG_IDX
};
const char *sx938x_parse_reg[] = {
	"sx938x,irq_enable_reg",
	"sx938x,irqcfg_reg",
	"sx938x,gnrl_ctrl0_reg",
	"sx938x,gnrl_ctrl1_reg",
	"sx938x,gnrl_ctrl2_reg",
	"sx938x,afe_ctrl1_reg",
	"sx938x,afe_param0_phr_reg",
	"sx938x,afe_param1_phr_reg",
	"sx938x,afe_param0_phm_reg",
	"sx938x,afe_param1_phm_reg",
	"sx938x,proxctrl0_phr_reg",
	"sx938x,proxctrl0_phm_reg",
	"sx938x,proxctrl1_reg",
	"sx938x,proxctrl2_reg",
	"sx938x,proxctrl3_reg",
	"sx938x,proxctrl4_reg",
	"sx938x,proxctrl5_reg",
	"sx938x,refcorr0_reg",
	"sx938x,refcorr1_reg",
	"sx938x,usefilter0_reg",
	"sx938x,usefilter1_reg",
	"sx938x,usefilter2_reg",
	"sx938x,usefilter3_reg",
	"sx938x,usefilter4_reg"
};
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
const char *sx938x_sub_parse_reg[] = {
	"sx938x_sub,irq_enable_reg",
	"sx938x_sub,irqcfg_reg",
	"sx938x_sub,gnrl_ctrl0_reg",
	"sx938x_sub,gnrl_ctrl1_reg",
	"sx938x_sub,gnrl_ctrl2_reg",
	"sx938x_sub,afe_ctrl1_reg",
	"sx938x_sub,afe_param0_phr_reg",
	"sx938x_sub,afe_param1_phr_reg",
	"sx938x_sub,afe_param0_phm_reg",
	"sx938x_sub,afe_param1_phm_reg",
	"sx938x_sub,proxctrl0_phr_reg",
	"sx938x_sub,proxctrl0_phm_reg",
	"sx938x_sub,proxctrl1_reg",
	"sx938x_sub,proxctrl2_reg",
	"sx938x_sub,proxctrl3_reg",
	"sx938x_sub,proxctrl4_reg",
	"sx938x_sub,proxctrl5_reg",
	"sx938x_sub,refcorr0_reg",
	"sx938x_sub,refcorr1_reg",
	"sx938x_sub,usefilter0_reg",
	"sx938x_sub,usefilter1_reg",
	"sx938x_sub,usefilter2_reg",
	"sx938x_sub,usefilter3_reg",
	"sx938x_sub,usefilter4_reg"
};
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB2)
const char *sx938x_sub2_parse_reg[] = {
	"sx938x_sub2,irq_enable_reg",
	"sx938x_sub2,irqcfg_reg",
	"sx938x_sub2,gnrl_ctrl0_reg",
	"sx938x_sub2,gnrl_ctrl1_reg",
	"sx938x_sub2,gnrl_ctrl2_reg",
	"sx938x_sub2,afe_ctrl1_reg",
	"sx938x_sub2,afe_param0_phr_reg",
	"sx938x_sub2,afe_param1_phr_reg",
	"sx938x_sub2,afe_param0_phm_reg",
	"sx938x_sub2,afe_param1_phm_reg",
	"sx938x_sub2,proxctrl0_phr_reg",
	"sx938x_sub2,proxctrl0_phm_reg",
	"sx938x_sub2,proxctrl1_reg",
	"sx938x_sub2,proxctrl2_reg",
	"sx938x_sub2,proxctrl3_reg",
	"sx938x_sub2,proxctrl4_reg",
	"sx938x_sub2,proxctrl5_reg",
	"sx938x_sub2,refcorr0_reg",
	"sx938x_sub2,refcorr1_reg",
	"sx938x_sub2,usefilter0_reg",
	"sx938x_sub2,usefilter1_reg",
	"sx938x_sub2,usefilter2_reg",
	"sx938x_sub2,usefilter3_reg",
	"sx938x_sub2,usefilter4_reg"
};
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9380_WIFI)
const char *sx938x_wifi_parse_reg[] = {
	"sx938x_wifi,irq_enable_reg",
	"sx938x_wifi,irqcfg_reg",
	"sx938x_wifi,gnrl_ctrl0_reg",
	"sx938x_wifi,gnrl_ctrl1_reg",
	"sx938x_wifi,gnrl_ctrl2_reg",
	"sx938x_wifi,afe_ctrl1_reg",
	"sx938x_wifi,afe_param0_phr_reg",
	"sx938x_wifi,afe_param1_phr_reg",
	"sx938x_wifi,afe_param0_phm_reg",
	"sx938x_wifi,afe_param1_phm_reg",
	"sx938x_wifi,proxctrl0_phr_reg",
	"sx938x_wifi,proxctrl0_phm_reg",
	"sx938x_wifi,proxctrl1_reg",
	"sx938x_wifi,proxctrl2_reg",
	"sx938x_wifi,proxctrl3_reg",
	"sx938x_wifi,proxctrl4_reg",
	"sx938x_wifi,proxctrl5_reg",
	"sx938x_wifi,refcorr0_reg",
	"sx938x_wifi,refcorr1_reg",
	"sx938x_wifi,usefilter0_reg",
	"sx938x_wifi,usefilter1_reg",
	"sx938x_wifi,usefilter2_reg",
	"sx938x_wifi,usefilter3_reg",
	"sx938x_wifi,usefilter4_reg"
};
#endif

static struct smtc_reg_data setup_reg[][24] = {
	{
		//Interrupt and config
		{
			.reg = SX938x_IRQ_ENABLE_REG,	//0x02
			.val = 0x0E,					// Enable Close and Far -> enable compensation interrupt
		},
		{
			.reg = SX938x_IRQCFG_REG, 		//0x03
			.val = 0x00,
		},
		//--------Sensor enable
		{
			.reg = SX938x_GNRL_CTRL0_REG,    //0x10
			.val = 0x03,
		},
		//--------General control
		{
			.reg = SX938x_GNRL_CTRL1_REG,	//0x11 //SCANPERIOD
			.val = 0x00,
		},
		{
			.reg = SX938x_GNRL_CTRL2_REG,    //0x12 //SCAN PERIOD
			.val = 0x32,
		},
		//--------AFE Control
		{
			.reg = SX938x_AFE_CTRL1_REG,   		//0x21//RESFILTIN
			.val = 0x01,
		},
		{
			.reg = SX938x_AFE_PARAM0_PHR_REG,   //0x22// AFE resolution for REF
			.val = 0x0C,
		},
		{
			.reg = SX938x_AFE_PARAM1_PHR_REG,   //0x23 //AFE AGAIN FREQ for REF
			.val = 0x46,
		},
		{
			.reg = SX938x_AFE_PARAM0_PHM_REG,   //0x24 //AFE resolution for MAIN
			.val = 0x0C,
		},
		{
			.reg = SX938x_AFE_PARAM1_PHM_REG,   //0x25 //AFE AGAIN FREQ for MAIN
			.val = 0x46,
		},

		//--------Advanced control (default)
		{
			.reg = SX938x_PROXCTRL0_PHR_REG, //0x40 //GAIN RAWFILT for REF
			.val = 0x09,
		},
		{
			.reg = SX938x_PROXCTRL0_PHM_REG,//0x41 //GAIN RAWFILT for MAIN
			.val = 0x09,
		},
		{
			.reg = SX938x_PROXCTRL1_REG,//0x42 //AVG INIT NEGTHRESHOLD
			.val = 0x20,
		},
		{
			.reg = SX938x_PROXCTRL2_REG,//0x43 //AVG DEB POSTHRESHOLD
			.val = 0x60,
		},
		{
			.reg = SX938x_PROXCTRL3_REG,//0x44 //AVG FILTER
			.val = 0x0C,
		},
		{
			.reg = SX938x_PROXCTRL4_REG,//0x45 //HYST DEB
			.val = 0x00,
		},
		{
			.reg = SX938x_PROXCTRL5_REG,//0x46 //PROX THRESHOLD
			.val = 0x10,
		},
		//Ref
		{
			.reg = SX938x_REFCORR0_REG,//0x60 //REF EN
			.val = 0x00,
		},
		{
			.reg = SX938x_REFCORR1_REG,//0x61 //REF COEFF
			.val = 0x00,
		},
		//USEFILTER
		{
			.reg = SX938x_USEFILTER0_REG,//0x70 //Non-detection threshold
			.val = 0x00,
		},
		{
			.reg = SX938x_USEFILTER1_REG,//0x71 //pos detection threshold
			.val = 0x00,
		},
		{
			.reg = SX938x_USEFILTER2_REG,//0x72 //neg detection threshold
			.val = 0x00,
		},
		{
			.reg = SX938x_USEFILTER3_REG,//0x73 //Non-detection, pos detection correction
			.val = 0x00,
		},
		{
			.reg = SX938x_USEFILTER4_REG,//0x74 //neg detection correction, factor, EN
			.val = 0x00,
		},
	},
#if IS_ENABLED(CONFIG_SENSORS_SX9380_SUB)
	{
		//Interrupt and config
		{
			.reg = SX938x_IRQ_ENABLE_REG,	//0x02
			.val = 0x0E,					// Enable Close and Far -> enable compensation interrupt
		},
		{
			.reg = SX938x_IRQCFG_REG, 		//0x03
			.val = 0x00,
		},
		//--------Sensor enable
		{
			.reg = SX938x_GNRL_CTRL0_REG,    //0x10
			.val = 0x03,
		},
		//--------General control
		{
			.reg = SX938x_GNRL_CTRL1_REG,	//0x11 //SCANPERIOD
			.val = 0x00,
		},
		{
			.reg = SX938x_GNRL_CTRL2_REG,    //0x12 //SCAN PERIOD
			.val = 0x32,
		},
		//--------AFE Control
		{
			.reg = SX938x_AFE_CTRL1_REG,   		//0x21//RESFILTIN
			.val = 0x01,
		},
		{
			.reg = SX938x_AFE_PARAM0_PHR_REG,   //0x22// AFE resolution for REF
			.val = 0x0C,
		},
		{
			.reg = SX938x_AFE_PARAM1_PHR_REG,   //0x23 //AFE AGAIN FREQ for REF
			.val = 0x46,
		},
		{
			.reg = SX938x_AFE_PARAM0_PHM_REG,   //0x24 //AFE resolution for MAIN
			.val = 0x0C,
		},
		{
			.reg = SX938x_AFE_PARAM1_PHM_REG,   //0x25 //AFE AGAIN FREQ for MAIN
			.val = 0x46,
		},

		//--------Advanced control (default)
		{
			.reg = SX938x_PROXCTRL0_PHR_REG, //0x40 //GAIN RAWFILT for REF
			.val = 0x09,
		},
		{
			.reg = SX938x_PROXCTRL0_PHM_REG,//0x41 //GAIN RAWFILT for MAIN
			.val = 0x09,
		},
		{
			.reg = SX938x_PROXCTRL1_REG,//0x42 //AVG INIT NEGTHRESHOLD
			.val = 0x20,
		},
		{
			.reg = SX938x_PROXCTRL2_REG,//0x43 //AVG DEB POSTHRESHOLD
			.val = 0x60,
		},
		{
			.reg = SX938x_PROXCTRL3_REG,//0x44 //AVG FILTER
			.val = 0x0C,
		},
		{
			.reg = SX938x_PROXCTRL4_REG,//0x45 //HYST DEB
			.val = 0x00,
		},
		{
			.reg = SX938x_PROXCTRL5_REG,//0x46 //PROX THRESHOLD
			.val = 0x10,
		},
		//Ref
		{
			.reg = SX938x_REFCORR0_REG,//0x60 //REF EN
			.val = 0x00,
		},
		{
			.reg = SX938x_REFCORR1_REG,//0x61 //REF COEFF
			.val = 0x00,
		},
		//USEFILTER
		{
			.reg = SX938x_USEFILTER0_REG,//0x70 //Non-detection threshold
			.val = 0x00,
		},
		{
			.reg = SX938x_USEFILTER1_REG,//0x71 //pos detection threshold
			.val = 0x00,
		},
		{
			.reg = SX938x_USEFILTER2_REG,//0x72 //neg detection threshold
			.val = 0x00,
		},
		{
			.reg = SX938x_USEFILTER3_REG,//0x73 //Non-detection, pos detection correction
			.val = 0x00,
		},
		{
			.reg = SX938x_USEFILTER4_REG,//0x74 //neg detection correction, factor, EN
			.val = 0x00,
		},
	},
#endif
};

#define MAX_NUM_STATUS_BITS (8)

enum {
	OFF = 0,
	ON = 1
};

#define GRIP_ERR(fmt, ...) pr_err("[GRIP_%s] %s "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)
#define GRIP_INFO(fmt, ...) pr_info("[GRIP_%s] %s "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)
#define GRIP_WARN(fmt, ...) pr_warn("[GRIP_%s] %s "fmt, grip_name[data->ic_num], __func__, ##__VA_ARGS__)

#if IS_ENABLED(CONFIG_SENSORS_GRIP_FAILURE_DEBUG)
extern void update_grip_error(u8 idx, u32 error_state);
#endif

#if !IS_ENABLED(CONFIG_SENSORS_CORE_AP)
extern int sensors_create_symlink(struct input_dev *inputdev);
extern void sensors_remove_symlink(struct input_dev *inputdev);
extern int sensors_register(struct device **dev, void *drvdata,
			    struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev,
			       struct device_attribute *attributes[]);
#endif
#endif
