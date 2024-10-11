#ifndef _LINUX_STM_TS_H_
#define _LINUX_STM_TS_H_

#include <asm/unaligned.h>
#include <linux/completion.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#include "../sec_secure_touch.h"
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#define SECURE_TOUCH_ENABLE	1
#define SECURE_TOUCH_DISABLE	0
#endif

#include "../sec_tclm_v2.h"
#if IS_ENABLED(CONFIG_INPUT_TOUCHSCREEN_TCLMV2)
#define TCLM_CONCEPT
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
#include "../sec_tsp_dumpkey.h"
extern struct tsp_dump_callbacks dump_callbacks;
#endif

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
#include <linux/hall/hall_ic_notifier.h>
#if IS_ENABLED(CONFIG_SUPPORT_SENSOR_FOLD)
#include <linux/sensor/sensors_core.h>
#endif
#endif

#if !IS_ENABLED(CONFIG_ARCH_WAIPIO)
#define ENABLE_RAWDATA_SERVICE
#endif


#include "../sec_input.h"
#include "../sec_tsp_log.h"

#ifndef I2C_M_DMA_SAFE
#define I2C_M_DMA_SAFE		0
#endif

#define input_raw_info_d(mode, dev, fmt, ...) input_raw_info(mode, dev, fmt, ## __VA_ARGS__)

#define USE_OPEN_CLOSE

#define STM_TS_SPI_NAME		"stm_ts_spi"
#define STM_TS_DEVICE_NAME	"STM_TS"

enum stm_ts_error_return {
	STM_TS_NOT_ERROR = 0,
	STM_TS_I2C_ERROR,
	STM_TS_ERROR_INVALID_CHIP_ID,
	STM_TS_ERROR_INVALID_CHIP_VERSION_ID,
	STM_TS_ERROR_INVALID_SW_VERSION,
	STM_TS_ERROR_EVENT_ID,
	STM_TS_ERROR_FW_CORRUPTION,
	STM_TS_ERROR_TIMEOUT,
	STM_TS_ERROR_TIMEOUT_ZERO,
	STM_TS_ERROR_FW_UPDATE_FAIL,
	STM_TS_ERROR_BROKEN_OSC_TRIM,
	STM_TS_ERROR_BROKEN_FW,
};

enum stm_ts_fw_update_status {
	STM_TS_NOT_UPDATE = 10,
	STM_TS_NEED_FW_UPDATE,
	STM_TS_NEED_CALIBRATION_ONLY,
	STM_TS_NEED_FW_UPDATE_N_CALIBRATION,
};

extern struct stm_ts_data *g_ts;

/**
 * struct stm_ts_finger - Represents fingers.
 * @ state: finger status (Event ID).
 * @ mcount: moving counter for debug.
 */
struct stm_ts_finger {
	u8 id;
	u8 prev_ttype;
	u8 ttype;
	u8 action;
	u16 x;
	u16 y;
	u16 p_x;
	u16 p_y;
	u8 z;
	u8 hover_flag;
	u8 glove_flag;
	u8 touch_height;
	u16 mcount;
	u8 major;
	u8 minor;
	bool palm;
	int palm_count;
	u8 left_event;
	u8 max_energy;
	u16 max_energy_x;
	u16 max_energy_y;
	u8 noise_level;
	u8 max_strength;
	u8 hover_id_num;
};

enum switch_system_mode {
	TO_TOUCH_MODE			= 0,
	TO_LOWPOWER_MODE		= 1,
};

enum tsp_status_call_pos {
	STM_TS_STATE_CHK_POS_OPEN = 0,
	STM_TS_STATE_CHK_POS_CLOSE,
	STM_TS_STATE_CHK_POS_HALL,
	STM_TS_STATE_CHK_POS_SYSFS,
};

enum stm_ts_cover_id {
	STM_TS_FLIP_WALLET = 0,
	STM_TS_VIEW_COVER,
	STM_TS_COVER_NOTHING1,
	STM_TS_VIEW_WIRELESS,
	STM_TS_COVER_NOTHING2,
	STM_TS_CHARGER_COVER,
	STM_TS_VIEW_WALLET,
	STM_TS_LED_COVER,
	STM_TS_CLEAR_FLIP_COVER,
	STM_TS_QWERTY_KEYBOARD_EUR,
	STM_TS_QWERTY_KEYBOARD_KOR,
	STM_TS_MONTBLANC_COVER = 100,
};

enum {
	SPECIAL_EVENT_TYPE_SPAY					= 0x04,
	SPECIAL_EVENT_TYPE_AOD					= 0x08,
	SPECIAL_EVENT_TYPE_AOD_PRESS				= 0x09,
	SPECIAL_EVENT_TYPE_AOD_LONGPRESS			= 0x0A,
	SPECIAL_EVENT_TYPE_AOD_DOUBLETAB			= 0x0B,
};

enum stm_ts_system_information_address {
	STM_TS_SI_CONFIG_CHECKSUM = 0x58,	/* 4 bytes */
	STM_TS_SI_OSC_TRIM_INFO = 0x60,	/* 4 bytes */
};

enum stm_ts_ito_test_mode {
	OPEN_TEST = 0,			// trx_open_test 1,1
	SHORT_TEST,				// trx_open_test 1,2
	MICRO_OPEN_TEST,		// trx_open_test 2
	MICRO_SHORT_TEST,		// trx_open_test 3
	OPEN_SHORT_CRACK_TEST,
	SAVE_MISCAL_REF_RAW,
};

enum stm_ts_ito_test_result {
	ITO_PASS = 0,
	ITO_FAIL,
	ITO_FAIL_OPEN,
	ITO_FAIL_SHORT,
	ITO_FAIL_MICRO_OPEN,
	ITO_FAIL_MICRO_SHORT,
};

/* STM_TS_OFFSET_SIGNUTRE */
#define STM_TS_OFFSET_SIGNATURE			0x59525446
#define STM_TS_CM2_SIGNATURE			0x324D5446
#define STM_TS_CM3_SIGNATURE			0x334D5446
#define STM_TS_FAIL_HIST_SIGNATURE		0x53484646

enum stm_ts_miscal_test_result {
	MISCAL_PASS = 0,
	MISCAL_FAIL,
};

#define UEVENT_OPEN_SHORT_PASS		1
#define UEVENT_OPEN_SHORT_FAIL		2

/* ----------------------------------------
 * write 0xE4 [ 11 | 10 | 01 | 00 ]
 * MSB <-------------------> LSB
 * read 0xE4
 * mapping sequnce : LSB -> MSB
 * struct sec_ts_test_result {
 * * assy : front + OCTA assay
 * * module : only OCTA
 *	 union {
 *		 struct {
 *			 u8 assy_count:2;	-> 00
 *			 u8 assy_result:2;	-> 01
 *			 u8 module_count:2;	-> 10
 *			 u8 module_result:2;	-> 11
 *		 } __attribute__ ((packed));
 *		 u8 data[1];
 *	 };
 *};
 * ----------------------------------------
 */
struct stm_ts_test_result {
	union {
		struct {
			u8 assy_count:2;
			u8 assy_result:2;
			u8 module_count:2;
			u8 module_result:2;
		} __packed;
		u8 data[1];
	};
};

#define TEST_OCTA_MODULE	1
#define TEST_OCTA_ASSAY		2

#define TEST_OCTA_NONE		0
#define TEST_OCTA_FAIL		1
#define TEST_OCTA_PASS		2

#define SEC_OFFSET_SIGNATURE		0x59525446

#define STM_TS_ITO_RESULT_PRINT_SIZE	1024

struct stm_ts_sec_panel_test_result {
	u8 flag;
	u8 num_of_test;
	u16 max_of_tx_gap;
	u16 max_of_rx_gap;
	u8 tx_of_txmax_gap;
	u8 rx_of_txmax_gap;
	u8 tx_of_rxmax_gap;
	u8 rx_of_rxmax_gap;
} __packed;

/* 16 byte */
struct stm_ts_event_coordinate {
	u8 eid:2;
	u8 tid:4;
	u8 tchsta:2;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 major;
	u8 minor;
	u8 z:6;
	u8 ttype_3_2:2;
	u8 left_event:5;
	u8 max_energy:1;
	u8 ttype_1_0:2;
	u8 noise_level;
	u8 max_strength;
	u8 hover_id_num:4;
	u8 reserved_10:4;
	u8 reserved_11;
	u8 reserved_12;
	u8 reserved_13;
	u8 reserved_14;
	u8 reserved_15;
} __packed;


/* 16 byte */
struct stm_ts_event_status {
	u8 eid:2;
	u8 stype:4;
	u8 sf:2;
	u8 status_id;
	u8 status_data_1;
	u8 status_data_2;
	u8 status_data_3;
	u8 status_data_4;
	u8 status_data_5;
	u8 left_event_4_0:5;
	u8 max_energy:1;
	u8 reserved:2;
	u8 reserved_8;
	u8 reserved_9;
	u8 reserved_10;
	u8 reserved_11;
	u8 reserved_12;
	u8 reserved_13;
	u8 reserved_14;
	u8 reserved_15;
} __packed;

/* 16 byte */
struct stm_ts_gesture_status {
	u8 eid:2;
	u8 stype:4;
	u8 sf:2;
	u8 gesture_id;
	u8 gesture_data_1;
	u8 gesture_data_2;
	u8 gesture_data_3;
	u8 gesture_data_4;
	u8 reserved_6;
	u8 left_event_4_0:5;
	u8 max_energy:1;
	u8 reserved_7:2;
	u8 reserved_8;
	u8 reserved_9;
	u8 reserved_10;
	u8 reserved_11;
	u8 reserved_12;
	u8 reserved_13;
	u8 reserved_14;
	u8 reserved_15;
} __packed;

struct stm_ts_syncframeheader {
	u8		header; // 0
	u8		host_data_mem_id; // 1
	u16	  cnt;  // 2~3
	u8		dbg_frm_len;  // 4
	u8		ms_force_len; // 5
	u8		ms_sense_len; // 6
	u8		ss_force_len; // 7
	u8		ss_sense_len; // 8
	u8		key_len;  // 9
	u16	  reserved1;  // 10~11
	u32	  reserved2;  // 12~15
} __packed;

enum stm_ts_nvm_data_type {		/* Write Command */
	STM_TS_NVM_OFFSET_FAC_RESULT = 1,
	STM_TS_NVM_OFFSET_CAL_COUNT,
	STM_TS_NVM_OFFSET_DISASSEMBLE_COUNT,
	STM_TS_NVM_OFFSET_TUNE_VERSION,
	STM_TS_NVM_OFFSET_CAL_POSITION,
	STM_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT,
	STM_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP,
	STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO,
	STM_TS_NVM_OFFSET_CAL_FAIL_FLAG,
	STM_TS_NVM_OFFSET_CAL_FAIL_COUNT,
};

struct stm_ts_nvm_data_map {
	int type;
	int offset;
	int length;
};

#define STM_TS_COMP_DATA_HEADER_SIZE     16

struct stm_ts_snr_result_cmd {
	s16	status;
	s16	point;
	s16	average;
} __packed;

struct tsp_snr_result_of_point {
	s16	max;
	s16	min;
	s16	average;
	s16	nontouch_peak_noise;
	s16	touch_peak_noise;
	s16	snr1;
	s16	snr2;
} __packed;

struct stm_ts_snr_result {
	s16	status;
	s16	reserved[6];
	struct tsp_snr_result_of_point result[9];
} __packed;

/* This Flash Meory Map is FIXED by STM firmware
 * Do not change MAP.
 */
#define STM_TS_NVM_OFFSET_ALL	31

struct stm_ts_data {
	struct spi_device *client;

	int irq;
	struct sec_ts_plat_data *plat_data;
#ifdef STM_TS_SUPPORT_TA_MODE
	void (*register_cb)(void *);
	struct stm_ts_callbacks callbacks;
#endif
	struct mutex lock;
	bool probe_done;
	struct sec_cmd_data sec;
	int tx_count;
	int rx_count;

	short *pFrame;
	u8 miscal_result;
	u8 *cx_data;
	u8 *ito_result;
	struct stm_ts_test_result test_result;
	u8 disassemble_count;
	u8 fac_nv;

	struct sec_tclm_data *tdata;
	bool is_cal_done;

	bool fw_corruption;
	bool glove_enabled;
	u8 brush_mode;

	int resolution_x;
	int resolution_y;

	u8 touch_opmode;
	u8 charger_mode;
	u8 scan_mode;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	atomic_t secure_enabled;
	atomic_t secure_pending_irqs;
	struct completion secure_powerdown;
	struct completion secure_interrupt;
#endif

#ifdef STM_TS_SUPPORT_TA_MODE
	bool TA_Pluged;
#endif

	int fw_version_of_ic;			/* firmware version of IC */
	int fw_version_of_bin;			/* firmware version of binary */
	int config_version_of_ic;		/* Config release data from IC */
	int config_version_of_bin;		/* Config release data from IC */
	u16 fw_main_version_of_ic;	/* firmware main version of IC */
	u16 fw_main_version_of_bin;	/* firmware main version of binary */
	u8 project_id_of_ic;
	u8 project_id_of_bin;
	u8 ic_name_of_ic;
	u8 ic_name_of_bin;
	u8 module_version_of_ic;
	u8 module_version_of_bin;
	int panel_revision;			/* Octa panel revision */
	u32 chip_id;

	int flip_status_prev;
	int flip_status;
	int flip_status_current;
	int change_flip_status;
	int tsp_open_status;
	struct mutex switching_mutex;
	struct mutex status_mutex;
	struct delayed_work switching_work;
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	struct notifier_block hall_ic_nb;
	struct notifier_block hall_ic_nb_ssh;
#endif

	struct notifier_block stm_input_nb;
	struct delayed_work work_print_info;
	struct delayed_work work_read_functions;
	struct delayed_work reset_work;
	struct delayed_work work_read_info;
	struct delayed_work debug_work;
	struct delayed_work check_rawdata;
	struct delayed_work secure_work;
	
	volatile bool reset_is_on_going;

	int debug_flag;
	struct mutex spi_mutex;
	struct mutex device_mutex;
	struct mutex eventlock;
	struct mutex sponge_mutex;
	struct mutex fn_mutex;
	struct mutex modechange;
	struct mutex secure_lock;
	bool info_work_done;

	u8 factory_position;
	int proc_cmoffset_size;
	int proc_cmoffset_all_size;
	char *cmoffset_sdc_proc;
	char *cmoffset_sub_proc;
	char *cmoffset_main_proc;
	char *cmoffset_all_proc;
	char *miscal_proc;

	int proc_fail_hist_size;
	int proc_fail_hist_all_size;
	char *fail_hist_sdc_proc;
	char *fail_hist_sub_proc;
	char *fail_hist_main_proc;
	char *fail_hist_all_proc;

	bool sponge_inf_dump;
	u8 sponge_dump_format;
	u8 sponge_dump_event;
	u8 sponge_dump_border_msb;
	u8 sponge_dump_border_lsb;
	bool sponge_dump_delayed_flag;
	u8 sponge_dump_delayed_area;
	u16 sponge_dump_border;

	bool rear_selfie_mode;

	u8 hover_event;
	
	bool tsp_dump_lock;

	bool fix_active_mode;
	bool touch_aging_mode;
	int sensitivity_mode;

	int raw_irq_count;
	int before_irq_count;
	u8 raw_addr_h;
	u8 raw_addr_l;
	u8 raw_mode;
	int raw_len;
	u8 *raw_u8;//read from IC
	s16 *raw;//convert x/y
	u8 *raw_v0;//mmap0 ...
	u8 *raw_v1;
	u8 *raw_v2;
	u8 *raw_v3;
	u8 *raw_v4;
	short *mmapdata;
	struct mutex raw_lock;

	bool rawcap_lock;
	int rawcap_max;
	int rawcap_max_tx;
	int rawcap_max_rx;
	int rawcap_min;
	int rawcap_min_tx;
	int rawcap_min_rx;

	int (*stop_device)(struct stm_ts_data *ts);
	int (*start_device)(struct stm_ts_data *ts);

	int (*stm_ts_spi_write)(struct stm_ts_data *ts, u8 *reg, int cunum, u8 *data, int len);
	int (*stm_ts_spi_read)(struct stm_ts_data *ts, u8 *reg, int cnum, u8 *data, int len);
	int (*stm_ts_read_sponge)(struct stm_ts_data *ts, u8 *data, int length);
	int (*stm_ts_write_sponge)(struct stm_ts_data *ts, u8 *data, int length);
	int (*stm_ts_systemreset)(struct stm_ts_data *ts, unsigned int msec);
	int (*stm_ts_wait_for_ready)(struct stm_ts_data *ts);
	void (*stm_ts_command)(struct stm_ts_data *ts, u8 cmd, bool checkecho);
};

//core
int stm_ts_stop_device(void *data);
int stm_ts_start_device(void *data);
irqreturn_t stm_ts_irq_thread(int irq, void *ptr);

void stm_ts_reinit(void *data);
int stm_ts_execute_autotune(struct stm_ts_data *ts, bool IsSaving);
int stm_ts_spi_get_tsp_test_result(struct stm_ts_data *ts);
void stm_ts_release_all_finger(struct stm_ts_data *ts);
void stm_ts_locked_release_all_finger(struct stm_ts_data *ts);

int stm_ts_set_external_noise_mode(struct stm_ts_data *ts, u8 mode);
int stm_ts_fix_active_mode(struct stm_ts_data *ts, bool enable);
int stm_ts_get_version_info(struct stm_ts_data *ts);
int stm_ts_wait_for_ready(struct stm_ts_data *ts);
int stm_ts_input_open(struct input_dev *dev);
void stm_ts_input_close(struct input_dev *dev);

//fn
int stm_ts_read_from_sponge(struct stm_ts_data *ts, u8 *data, int length);
int stm_ts_write_to_sponge(struct stm_ts_data *ts, u8 *data, int length);
void stm_ts_command(struct stm_ts_data *ts, u8 cmd, bool checkecho);
void stm_set_grip_data_to_ic(struct device *dev, u8 flag);
int stm_ts_set_temperature(struct device *dev, u8 temperature_data);
int stm_ts_fw_corruption_check(struct stm_ts_data *ts);
int stm_ts_read_chip_id_hw(struct stm_ts_data *ts);
int stm_ts_read_chip_id(struct stm_ts_data *ts);
int stm_ts_get_version_info(struct stm_ts_data *ts);
int stm_ts_systemreset(struct stm_ts_data *ts, unsigned int msec);
int stm_ts_set_scanmode(struct stm_ts_data *ts, u8 scan_mode);
int stm_ts_set_hsync_scanmode(struct stm_ts_data *ts, u8 scan_mode);
int stm_ts_set_lowpowermode(void *data, u8 mode);
int stm_ts_set_aod_rect(struct stm_ts_data *ts);
void stm_ts_reset(struct stm_ts_data *ts, unsigned int ms);
void stm_ts_reset_work(struct work_struct *work);
void stm_ts_read_info_work(struct work_struct *work);
void stm_ts_print_info_work(struct work_struct *work);
int get_nvm_data_by_size(struct stm_ts_data *ts, u8 offset, int length, u8 *nvdata);
int get_nvm_data(struct stm_ts_data *ts, int type, u8 *nvdata);
int stm_ts_set_custom_library(struct stm_ts_data *ts);
void stm_ts_get_custom_library(struct stm_ts_data *ts);
void stm_ts_set_fod_finger_merge(struct stm_ts_data *ts);
int stm_ts_set_fod_rect(struct stm_ts_data *ts);
int stm_ts_set_touchable_area(struct stm_ts_data *ts);
int stm_ts_ear_detect_enable(struct stm_ts_data *ts, u8 enable);
int stm_ts_pocket_mode_enable(struct stm_ts_data *ts, u8 enable);
int stm_ts_set_charger_mode(struct stm_ts_data *ts);
void stm_ts_spi_set_cover_type(struct stm_ts_data *ts, bool enable);
int stm_ts_set_press_property(struct stm_ts_data *ts);
int stm_ts_get_sysinfo_data(struct stm_ts_data *ts, u8 sysinfo_addr, u8 read_cnt, u8 *data);
void stm_ts_change_scan_rate(struct stm_ts_data *ts, u8 rate);
int stm_ts_osc_trim_recovery(struct stm_ts_data *ts);
int get_nvm_data(struct stm_ts_data *ts, int type, u8 *nvdata);
int set_nvm_data(struct stm_ts_data *ts, u8 type, u8 *buf);
int get_nvm_data_by_size(struct stm_ts_data *ts, u8 offset, int length, u8 *nvdata);
int set_nvm_data_by_size(struct stm_ts_data *ts, u8 offset, int length, u8 *buf);
int stm_ts_get_channel_info(struct stm_ts_data *ts);
int stm_ts_set_opmode(struct stm_ts_data *ts, u8 mode);
int stm_ts_set_touch_function(struct stm_ts_data *ts);
void stm_ts_get_touch_function(struct work_struct *work);

//cmd
void stm_ts_fn_remove(struct stm_ts_data *ts);
int stm_ts_fn_init(struct stm_ts_data *ts);
int stm_ts_panel_ito_test(struct stm_ts_data *ts, int testmode);
void stm_ts_run_rawdata_all(struct stm_ts_data *ts);

//dump
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
void stm_ts_check_rawdata(struct work_struct *work);
void stm_ts_dump_tsp_log(struct device *dev);
void stm_ts_spi_sponge_dump_flush(struct stm_ts_data *ts, int dump_area);
#endif
void stm_ts_init_proc(struct stm_ts_data *ts);
ssize_t get_cmoffset_dump(struct stm_ts_data *ts, char *buf, u8 position);
ssize_t get_lp_dump(struct device *dev, struct device_attribute *attr, char *buf);

//fw
int stm_ts_spi_fw_update_on_probe(struct stm_ts_data *ts);
int stm_ts_spi_fw_update_on_hidden_menu(struct stm_ts_data *ts, int update_type);
int stm_ts_wait_for_echo_event(struct stm_ts_data *ts, u8 *cmd, u8 cmd_cnt, int delay);
int stm_ts_fw_wait_for_event(struct stm_ts_data *ts, u8 *result, u8 result_cnt);
int stm_tclm_data_read(struct spi_device *client, int address);
int stm_tclm_data_write(struct spi_device *client, int address);
int stm_ts_tclm_execute_force_calibration(struct spi_device *client, int cal_mode);
void stm_ts_checking_miscal(struct stm_ts_data *ts);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
int stm_notifier_call(struct notifier_block *n, unsigned long data, void *v);
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
void stm_switching_work(struct work_struct *work);

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
int stm_hall_ic_notify(struct notifier_block *nb, unsigned long flip_cover, void *v);
#endif
#if IS_ENABLED(CONFIG_SUPPORT_SENSOR_FOLD)
int stm_hall_ic_ssh_notify(struct notifier_block *nb, unsigned long flip_cover, void *v);
#endif
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
extern struct tsp_dump_callbacks dump_callbacks;
#endif

#ifdef STM_TS_SUPPORT_TA_MODE
extern struct stm_ts_callbacks *STM_TS_charger_callbacks;
struct stm_ts_callbacks {
	void (*inform_charger)(struct STM_TS_callbacks *, int);
};
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
void secure_touch_stop(struct stm_ts_data *ts, bool stop);
#endif
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
int stm_ts_spi_tool_proc_init(struct stm_ts_data *ts);
int stm_ts_spi_tool_proc_remove(void);
#endif
void stm_ts_read_rawdata_address(struct stm_ts_data *ts);
#ifdef ENABLE_RAWDATA_SERVICE
int stm_ts_rawdata_map_alloc(struct stm_ts_data *ts);
int stm_ts_rawdata_map_init(struct stm_ts_data *ts);
void  stm_ts_rawdata_map_remove(struct stm_ts_data *ts);
#endif
#endif /* _LINUX_stm_ts_H_ */

