/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for common functions
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_inspection.h"
#include "himax_modular.h"

#define TOUCH_PRINT_INFO_DWORK_TIME 30000 /* 30 secs */

#ifdef HX_SMART_WAKEUP
#define GEST_SUP_NUM 1
/* Setting cust key define (DF = double finger) */
/* {Double Tap, Up, Down, Left, Rright, C, Z, M,
 *	O, S, V, W, e, m, @, (reserve),
 *	Finger gesture, ^, >, <, f(R), f(L), Up(DF), Down(DF),
 *	Left(DF), Right(DF)}
 */
	uint8_t gest_event[GEST_SUP_NUM] = {0x80};

/*gest_event mapping to gest_key_def*/
	uint16_t gest_key_def[GEST_SUP_NUM] = {KEY_WAKEUP};

uint8_t *wake_event_buffer;
#endif

#define SUPPORT_FINGER_DATA_CHECKSUM 0x0F
#define TS_WAKE_LOCK_TIMEOUT		(1000)
#define FRAME_COUNT 5

uint32_t g_hx_chip_inited;

#if defined(__EMBEDDED_FW__)
struct firmware g_embedded_fw = {
	.data = _binary___Himax_firmware_bin_start,
};
#endif

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
/*
 *#if defined(HX_EN_DYNAMIC_NAME)
 *char *i_CTPM_firmware_name;
 *#else
 *char *i_CTPM_firmware_name = "Himax_firmware.bin";
 *#endif
 */
bool g_auto_update_flag;
#endif
#if defined(HX_AUTO_UPDATE_FW)
unsigned char *i_CTPM_FW;
int i_CTPM_FW_len;
int g_i_FW_VER;
int g_i_CFG_VER;
int g_i_CID_MAJ; /*GUEST ID*/
int g_i_CID_MIN; /*VER for GUEST*/
#endif
#ifdef HX_ZERO_FLASH
int g_f_0f_updat;
#endif

#ifdef SEC_FACTORY_MODE
extern int sec_touch_sysfs(struct himax_ts_data *data);
extern void sec_touch_sysfs_remove(struct himax_ts_data *data);
#endif

struct himax_ts_data *private_ts;
EXPORT_SYMBOL(private_ts);

struct himax_ic_data *ic_data;
EXPORT_SYMBOL(ic_data);

struct himax_report_data *hx_touch_data;
EXPORT_SYMBOL(hx_touch_data);

struct himax_core_fp g_core_fp;
EXPORT_SYMBOL(g_core_fp);

struct himax_debug *debug_data;
EXPORT_SYMBOL(debug_data);

struct proc_dir_entry *himax_touch_proc_dir;
EXPORT_SYMBOL(himax_touch_proc_dir);

struct himax_chip_detect *g_core_chip_dt;
EXPORT_SYMBOL(g_core_chip_dt);

int g_mmi_refcnt;
EXPORT_SYMBOL(g_mmi_refcnt);

#define HIMAX_PROC_TOUCH_FOLDER "android_touch"
/*ts_work about start*/
struct himax_target_report_data *g_target_report_data;
EXPORT_SYMBOL(g_target_report_data);

static void himax_report_all_leave_event(struct himax_ts_data *ts);
/*ts_work about end*/
static int		HX_TOUCH_INFO_POINT_CNT;

unsigned long FW_VER_MAJ_FLASH_ADDR;
EXPORT_SYMBOL(FW_VER_MAJ_FLASH_ADDR);

unsigned long FW_VER_MIN_FLASH_ADDR;
EXPORT_SYMBOL(FW_VER_MIN_FLASH_ADDR);

unsigned long CFG_VER_MAJ_FLASH_ADDR;
EXPORT_SYMBOL(CFG_VER_MAJ_FLASH_ADDR);

unsigned long CFG_VER_MIN_FLASH_ADDR;
EXPORT_SYMBOL(CFG_VER_MIN_FLASH_ADDR);

unsigned long CID_VER_MAJ_FLASH_ADDR;
EXPORT_SYMBOL(CID_VER_MAJ_FLASH_ADDR);

unsigned long CID_VER_MIN_FLASH_ADDR;
EXPORT_SYMBOL(CID_VER_MIN_FLASH_ADDR);

unsigned long PANEL_VERSION_ADDR;
EXPORT_SYMBOL(PANEL_VERSION_ADDR);

unsigned long FW_VER_MAJ_FLASH_LENG;
EXPORT_SYMBOL(FW_VER_MAJ_FLASH_LENG);

unsigned long FW_VER_MIN_FLASH_LENG;
EXPORT_SYMBOL(FW_VER_MIN_FLASH_LENG);

unsigned long CFG_VER_MAJ_FLASH_LENG;
EXPORT_SYMBOL(CFG_VER_MAJ_FLASH_LENG);

unsigned long CFG_VER_MIN_FLASH_LENG;
EXPORT_SYMBOL(CFG_VER_MIN_FLASH_LENG);

unsigned long CID_VER_MAJ_FLASH_LENG;
EXPORT_SYMBOL(CID_VER_MAJ_FLASH_LENG);

unsigned long CID_VER_MIN_FLASH_LENG;
EXPORT_SYMBOL(CID_VER_MIN_FLASH_LENG);

unsigned long PANEL_VERSION_LENG;
EXPORT_SYMBOL(PANEL_VERSION_LENG);

unsigned long FW_CFG_VER_FLASH_ADDR;


unsigned char IC_CHECKSUM;
EXPORT_SYMBOL(IC_CHECKSUM);

uint8_t g_last_fw_irq_flag = 0;
uint8_t g_last_fw_irq_flag2 = 0;

#ifdef HX_ESD_RECOVERY
u8 HX_ESD_RESET_ACTIVATE;
EXPORT_SYMBOL(HX_ESD_RESET_ACTIVATE);

int hx_EB_event_flag;
EXPORT_SYMBOL(hx_EB_event_flag);

int hx_EC_event_flag;
EXPORT_SYMBOL(hx_EC_event_flag);

int hx_ED_event_flag;
EXPORT_SYMBOL(hx_ED_event_flag);

int g_zero_event_count;

#endif

static bool chip_test_r_flag;
u8 HX_HW_RESET_ACTIVATE;

static uint8_t AA_press;
static uint8_t EN_NoiseFilter;
static uint8_t Last_EN_NoiseFilter;

static int p_point_num = 0xFFFF;
#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
static uint8_t vk_press;
static int tpd_key;
static int tpd_key_old;
#endif
static int probe_fail_flag;
bool USB_detect_flag;
EXPORT_SYMBOL(USB_detect_flag);

#ifdef HX_GESTURE_TRACK
static int gest_pt_cnt;
static int gest_pt_x[GEST_PT_MAX_NUM];
static int gest_pt_y[GEST_PT_MAX_NUM];
static int gest_start_x, gest_start_y, gest_end_x, gest_end_y;
static int gest_width, gest_height, gest_mid_x, gest_mid_y;
static int hx_gesture_coor[16];
#endif

int g_ts_dbg;
EXPORT_SYMBOL(g_ts_dbg);

/* File node for Selftest, SMWP and HSEN - Start*/
#define HIMAX_PROC_SELF_TEST_FILE	"self_test"
struct proc_dir_entry *himax_proc_self_test_file;

uint8_t HX_PROC_SEND_FLAG;
EXPORT_SYMBOL(HX_PROC_SEND_FLAG);

#ifdef HX_SMART_WAKEUP
	#define HIMAX_PROC_SMWP_FILE "SMWP"
	struct proc_dir_entry *himax_proc_SMWP_file = NULL;
	#define HIMAX_PROC_GESTURE_FILE "GESTURE"
	struct proc_dir_entry *himax_proc_GESTURE_file = NULL;
	uint8_t HX_SMWP_EN;
#ifdef HX_P_SENSOR
	#define HIMAX_PROC_PSENSOR_FILE "Psensor"
	struct proc_dir_entry *himax_proc_psensor_file = NULL;
#endif
#endif
#if defined(HX_SMART_WAKEUP) || IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
bool FAKE_POWER_KEY_SEND = true;
#endif

#ifdef HX_HIGH_SENSE
	#define HIMAX_PROC_HSEN_FILE "HSEN"
	struct proc_dir_entry *himax_proc_HSEN_file = NULL;
#endif

#if defined(HX_PALM_REPORT)
static int himax_palm_detect(uint8_t *buf)
{
	struct himax_ts_data *ts = private_ts;
	int32_t loop_i;
	int base = 0;
	int x = 0, y = 0, w = 0;

	loop_i = 0;
	base = loop_i * 4;
	x = buf[base] << 8 | buf[base + 1];
	y = (buf[base + 2] << 8 | buf[base + 3]);
	w = buf[(ts->nFinger_support * 4) + loop_i];
	I(" %s HX_PALM_REPORT_loopi=%d,base=%x,X=%x,Y=%x,W=%x\n", __func__, loop_i, base, x, y, w);
	if ((!atomic_read(&ts->suspend_mode)) && (x == 0xFA5A) && (y == 0xFA5A) && (w == 0x00))
		return PALM_REPORT;
	else
		return NOT_REPORT;
}
#endif

static ssize_t himax_self_test(struct seq_file *s, void *v)
{
	int val = 0x00;
	size_t ret = 0;
	int i = 0;

	I("%s: enter, %d\n", __func__, __LINE__);

	if (private_ts->suspended == 1) {
		E("%s: please do self test in normal active mode\n", __func__);
		return HX_INIT_FAIL;
	}

	himax_int_enable(0);/* disable irq */

	private_ts->in_self_test = 1;

	val = g_core_fp.fp_chip_self_test();
/*
 *#ifdef HX_ESD_RECOVERY
 *	HX_ESD_RESET_ACTIVATE = 1;
 *#endif
 *	himax_int_enable(1); //enable irq
 */
	if (val == HX_INSPECT_OK)
		seq_puts(s, "Self_Test Pass:\n");
	else
		seq_puts(s, "Self_Test Fail:\n");

#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
	for (i = 0; i < HX_CRITERIA_ITEM - 1; i++) {
		if (g_test_item_flag[i] == 1) {
			seq_printf(s, "  %s : %s\n", g_himax_inspection_mode[i],
			((val & (1 << (i + ERR_SFT))) == (1 << (i + ERR_SFT)))?"Fail":"OK");
		}
	}

	if ((val & HX_INSPECT_EFILE) == HX_INSPECT_EFILE)
		seq_puts(s, "  Get criteria File Fail\n");
	if ((val & HX_INSPECT_MEMALLCTFAIL) == HX_INSPECT_MEMALLCTFAIL)
		seq_puts(s, "  Allocate memory Fail\n");
	if ((val & HX_INSPECT_EGETRAW) == HX_INSPECT_EGETRAW)
		seq_puts(s, "  Get raw data Fail\n");
	if ((val & HX_INSPECT_ESWITCHMODE) == HX_INSPECT_ESWITCHMODE)
		seq_puts(s, "  Switch mode Fail\n");
#endif

	private_ts->in_self_test = 0;

#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
	if (g_test_item_flag != NULL) {
		kfree(g_test_item_flag);
		g_test_item_flag = NULL;
	}
#endif

#ifdef HX_ESD_RECOVERY
	HX_ESD_RESET_ACTIVATE = 1;
#endif
	himax_int_enable(1);/* enable irq */

	return ret;
}

static void *himax_self_test_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= 1)
		return NULL;


	return (void *)((unsigned long) *pos + 1);
}

static void *himax_self_test_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void himax_self_test_seq_stop(struct seq_file *s, void *v)
{
}

static ssize_t himax_self_test_write(struct file *filp, const char __user *buff,
			size_t len, loff_t *data)
{
	char buf[80];

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == 'r') {
		chip_test_r_flag = true;
		I("%s: Start to read chip test data.\n", __func__);
	}	else {
		chip_test_r_flag = false;
		I("%s: Back to do self test.\n", __func__);
	}

	return len;
}

static int himax_self_test_seq_read(struct seq_file *s, void *v)
{
	size_t ret = 0;

	if (chip_test_r_flag) {
#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
		if (g_rslt_data)
			seq_printf(s, "%s", g_rslt_data);
		else
#endif
			seq_puts(s, "No chip test data.\n");
	} else {
		himax_self_test(s, v);
	}

	return ret;
}

static const struct seq_operations himax_self_test_seq_ops = {
	.start	= himax_self_test_seq_start,
	.next	= himax_self_test_seq_next,
	.stop	= himax_self_test_seq_stop,
	.show	= himax_self_test_seq_read,
};

static int himax_self_test_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &himax_self_test_seq_ops);
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops himax_proc_self_test_ops = {
	.proc_open = himax_self_test_proc_open,
	.proc_read = seq_read,
	.proc_write = himax_self_test_write,
	.proc_release = seq_release,
};
#else
static const struct file_operations himax_proc_self_test_ops = {
	.owner = THIS_MODULE,
	.open = himax_self_test_proc_open,
	.read = seq_read,
	.write = himax_self_test_write,
	.release = seq_release,
};
#endif

#ifdef HX_HIGH_SENSE
static ssize_t himax_HSEN_read(struct file *file, char *buf,
							   size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kcalloc(len, sizeof(char), GFP_KERNEL);
		count = snprintf(temp_buf, PAGE_SIZE, "%d\n", ts->HSEN_enable);

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else {
		HX_PROC_SEND_FLAG = 0;
	}

	return count;
}

static ssize_t himax_HSEN_write(struct file *file, const char *buff,
								size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[80] = {0};

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == '0')
		ts->HSEN_enable = 0;
	else if (buf[0] == '1')
		ts->HSEN_enable = 1;
	else
		return -EINVAL;

	g_core_fp.fp_set_HSEN_enable(ts->HSEN_enable, ts->suspended);
	I("%s: HSEN_enable = %d.\n", __func__, ts->HSEN_enable);
	return len;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops himax_proc_HSEN_ops = {
	.proc_read = himax_HSEN_read,
	.proc_write = himax_HSEN_write,
};
#else
static const struct file_operations himax_proc_HSEN_ops = {
	.owner = THIS_MODULE,
	.read = himax_HSEN_read,
	.write = himax_HSEN_write,
};
#endif
#endif

#ifdef HX_SMART_WAKEUP
static ssize_t himax_SMWP_read(struct file *file, char *buf,
							   size_t len, loff_t *pos)
{
	size_t count = 0;
	struct himax_ts_data *ts = private_ts;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kcalloc(len, sizeof(char), GFP_KERNEL);
		count = snprintf(temp_buf, PAGE_SIZE, "%d\n", ts->SMWP_enable);

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else {
		HX_PROC_SEND_FLAG = 0;
	}

	return count;
}

static ssize_t himax_SMWP_write(struct file *file, const char *buff,
								size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[80] = {0};

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == '0')
		ts->SMWP_enable = 0;
	else if (buf[0] == '1')
		ts->SMWP_enable = 1;
	else
		return -EINVAL;

	g_core_fp.fp_set_SMWP_enable(ts->SMWP_enable, ts->suspended);
	HX_SMWP_EN = ts->SMWP_enable;
	I("%s: SMART_WAKEUP_enable = %d.\n", __func__, HX_SMWP_EN);
	return len;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops himax_proc_SMWP_ops = {
	.proc_read = himax_SMWP_read,
	.proc_write = himax_SMWP_write,
};
#else
static const struct file_operations himax_proc_SMWP_ops = {
	.owner = THIS_MODULE,
	.read = himax_SMWP_read,
	.write = himax_SMWP_write,
};
#endif

static ssize_t himax_GESTURE_read(struct file *file, char *buf,
								  size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	int i = 0;
	size_t ret = 0;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kcalloc(len, sizeof(char), GFP_KERNEL);

		for (i = 0; i < GEST_SUP_NUM; i++)
			ret += snprintf(temp_buf + ret, len - ret, "ges_en[%d]=%d\n", i, ts->gesture_cust_en[i]);

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else {
		HX_PROC_SEND_FLAG = 0;
		ret = 0;
	}

	return ret;
}

static ssize_t himax_GESTURE_write(struct file *file, const char *buff,
								   size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	int i = 0;
	int j = 0;
	char buf[80] = {0};

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	I("himax_GESTURE_store= %s, len = %d\n", buf, (int)len);

	for (i = 0; i < len; i++) {
		if (buf[i] == '0' && j < GEST_SUP_NUM) {
			ts->gesture_cust_en[j] = 0;
			I("gesture en[%d]=%d\n", j, ts->gesture_cust_en[j]);
			j++;
		}	else if (buf[i] == '1' && j < GEST_SUP_NUM) {
			ts->gesture_cust_en[j] = 1;
			I("gesture en[%d]=%d\n", j, ts->gesture_cust_en[j]);
			j++;
		}	else
			I("Not 0/1 or >=GEST_SUP_NUM : buf[%d] = %c\n", i, buf[i]);
	}

	return len;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops himax_proc_Gesture_ops = {
	.proc_read = himax_GESTURE_read,
	.proc_write = himax_GESTURE_write,
};
#else
static const struct file_operations himax_proc_Gesture_ops = {
	.owner = THIS_MODULE,
	.read = himax_GESTURE_read,
	.write = himax_GESTURE_write,
};
#endif

#ifdef HX_P_SENSOR
static ssize_t himax_psensor_read(struct file *file, char *buf,
								  size_t len, loff_t *pos)
{
	size_t count = 0;
	struct himax_ts_data *ts = private_ts;
	char *temp_buf;

	if (!HX_PROC_SEND_FLAG) {
		temp_buf = kcalloc(len, sizeof(char), GFP_KERNEL);
		count = snprintf(temp_buf, PAGE_SIZE, "p-sensor flag = %d\n", ts->psensor_flag);

		if (copy_to_user(buf, temp_buf, len))
			I("%s,here:%d\n", __func__, __LINE__);

		kfree(temp_buf);
		HX_PROC_SEND_FLAG = 1;
	} else {
		HX_PROC_SEND_FLAG = 0;
	}

	return count;
}

static ssize_t himax_psensor_write(struct file *file, const char *buff,
								   size_t len, loff_t *pos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[80] = {0};

	if (len >= 80) {
		I("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len))
		return -EFAULT;

	if (buf[0] == '0' && ts->SMWP_enable == 1) {
		ts->psensor_flag = false;
		g_core_fp.fp_black_gest_ctrl(false);
	}	else if (buf[0] == '1' && ts->SMWP_enable == 1) {
		ts->psensor_flag = true;
		g_core_fp.fp_black_gest_ctrl(true);
	} else if (ts->SMWP_enable == 0) {
		I("%s: SMWP is disable, not supprot to ctrl p-sensor.\n", __func__);
	}	else
		return -EINVAL;

	I("%s: psensor_flag = %d.\n", __func__, ts->psensor_flag);
	return len;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops himax_proc_psensor_ops = {
	.proc_read = himax_psensor_read,
	.proc_write = himax_psensor_write,
};
#else
static const struct file_operations himax_proc_psensor_ops = {
	.owner = THIS_MODULE,
	.read = himax_psensor_read,
	.write = himax_psensor_write,
};
#endif
#endif
#endif

int himax_common_proc_init(void)
{
	himax_touch_proc_dir = proc_mkdir(HIMAX_PROC_TOUCH_FOLDER, NULL);

	if (himax_touch_proc_dir == NULL) {
		E(" %s: himax_touch_proc_dir file create failed!\n", __func__);
		return -ENOMEM;
	}
#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)
	if (fp_himax_self_test_init != NULL)
		fp_himax_self_test_init();
#endif

	himax_proc_self_test_file = proc_create(HIMAX_PROC_SELF_TEST_FILE, 0444, himax_touch_proc_dir, &himax_proc_self_test_ops);
	if (himax_proc_self_test_file == NULL) {
		E(" %s: proc self_test file create failed!\n", __func__);
		goto fail_1;
	}

#ifdef HX_HIGH_SENSE
	himax_proc_HSEN_file = proc_create(HIMAX_PROC_HSEN_FILE, 0666,
									   himax_touch_proc_dir, &himax_proc_HSEN_ops);

	if (himax_proc_HSEN_file == NULL) {
		E(" %s: proc HSEN file create failed!\n", __func__);
		goto fail_2;
	}

#endif
#ifdef HX_SMART_WAKEUP
	himax_proc_SMWP_file = proc_create(HIMAX_PROC_SMWP_FILE, 0666,
									   himax_touch_proc_dir, &himax_proc_SMWP_ops);

	if (himax_proc_SMWP_file == NULL) {
		E(" %s: proc SMWP file create failed!\n", __func__);
		goto fail_3;
	}

	himax_proc_GESTURE_file = proc_create(HIMAX_PROC_GESTURE_FILE, 0666,
										  himax_touch_proc_dir, &himax_proc_Gesture_ops);

	if (himax_proc_GESTURE_file == NULL) {
		E(" %s: proc GESTURE file create failed!\n", __func__);
		goto fail_4;
	}
#ifdef HX_P_SENSOR
	himax_proc_psensor_file = proc_create(HIMAX_PROC_PSENSOR_FILE, 0666,
										  himax_touch_proc_dir, &himax_proc_psensor_ops);

	if (himax_proc_psensor_file == NULL) {
		E(" %s: proc GESTURE file create failed!\n", __func__);
		goto fail_5;
	}
#endif
#endif
	return 0;
#ifdef HX_SMART_WAKEUP
#ifdef HX_P_SENSOR
fail_5:
#endif
	remove_proc_entry(HIMAX_PROC_GESTURE_FILE, himax_touch_proc_dir);
fail_4:
	remove_proc_entry(HIMAX_PROC_SMWP_FILE, himax_touch_proc_dir);
fail_3:
#endif
#ifdef HX_HIGH_SENSE
	remove_proc_entry(HIMAX_PROC_HSEN_FILE, himax_touch_proc_dir);
fail_2:
#endif
	remove_proc_entry(HIMAX_PROC_SELF_TEST_FILE, himax_touch_proc_dir);
fail_1:
	return -ENOMEM;
}

void himax_common_proc_deinit(void)
{
remove_proc_entry(HIMAX_PROC_SELF_TEST_FILE, himax_touch_proc_dir);
#ifdef HX_HIGH_SENSE
	remove_proc_entry(HIMAX_PROC_HSEN_FILE, himax_touch_proc_dir);
#endif
#ifdef HX_SMART_WAKEUP
#ifdef HX_P_SENSOR
	remove_proc_entry(HIMAX_PROC_PSENSOR_FILE, himax_touch_proc_dir);
#endif
	remove_proc_entry(HIMAX_PROC_GESTURE_FILE, himax_touch_proc_dir);
	remove_proc_entry(HIMAX_PROC_SMWP_FILE, himax_touch_proc_dir);
#endif
	remove_proc_entry(HIMAX_PROC_TOUCH_FOLDER, NULL);
}

/* File node for SMWP and HSEN - End*/

int himax_input_register(struct himax_ts_data *ts, struct input_dev *input_dev, u8 propbit)
{
	int ret = 0;
#if defined(HX_SMART_WAKEUP)
	int i = 0;
#endif

	if (!input_dev) {
		E("%s, input_dev is null\n", __func__);
		return -ENODEV;
	}

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(KEY_INT_CANCEL, input_dev->keybit);
#if defined(HX_PLATFOME_DEFINE_KEY)
	himax_platform_key();
#else
	//set_bit(KEY_BACK, input_dev->keybit);
	//set_bit(KEY_HOME, input_dev->keybit);
	//set_bit(KEY_MENU, input_dev->keybit);
	//set_bit(KEY_SEARCH, input_dev->keybit);
#endif
#if defined(HX_SMART_WAKEUP)
	for (i = 0; i < GEST_SUP_NUM; i++)
		set_bit(gest_key_def[i], input_dev->keybit);
#elif IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_INSPECT) || defined(HX_PALM_REPORT)
	set_bit(KEY_POWER, input_dev->keybit);
#endif
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(propbit, input_dev->propbit);
#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
	set_bit(KEY_APPSELECT, input_dev->keybit);
#endif
#ifdef	HX_PROTOCOL_A
	/*input_dev->mtsize = ts->nFinger_support;*/
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 3, 0, 0);
#else
	set_bit(MT_TOOL_FINGER, input_dev->keybit);
#if defined(HX_PROTOCOL_B_3PA)
	if (propbit == INPUT_PROP_DIRECT)
		input_mt_init_slots(input_dev, ts->nFinger_support, INPUT_MT_DIRECT);
	else
		input_mt_init_slots(input_dev, ts->nFinger_support, INPUT_MT_POINTER);
#else
	input_mt_init_slots(input_dev, ts->nFinger_support);
#endif
#endif
	I("input_set_abs_params: mix_x %d, max_x %d, min_y %d, max_y %d\n",
		ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, ts->pdata->abs_x_min,
				ts->pdata->abs_x_max, ts->pdata->abs_x_fuzz, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, ts->pdata->abs_y_min,
				ts->pdata->abs_y_max, ts->pdata->abs_y_fuzz, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, ts->pdata->abs_pressure_min,
				ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);

#ifdef SEC_PALM_FUNC
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, ts->pdata->abs_pressure_min,
				ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
	set_bit(BTN_PALM, input_dev->keybit);
#endif

#ifndef	HX_PROTOCOL_A
	if (propbit != INPUT_PROP_POINTER)
		input_set_abs_params(input_dev, ABS_MT_PRESSURE, ts->pdata->abs_pressure_min,
					ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, ts->pdata->abs_width_min,
				ts->pdata->abs_width_max, ts->pdata->abs_pressure_fuzz, 0);
#endif

	if (himax_input_register_device(input_dev) == 0)
		ret = NO_ERR;
	else
		ret = INPUT_REGISTER_FAIL;

	I("%s, input device %s registered.\n", __func__, input_dev->name);

	return ret;
}
EXPORT_SYMBOL(himax_input_register);

static void calcDataSize(void)
{
	struct himax_ts_data *ts_data = private_ts;

	ts_data->x_channel = ic_data->HX_RX_NUM;
	ts_data->y_channel = ic_data->HX_TX_NUM;
	ts_data->nFinger_support = ic_data->HX_MAX_PT;

	ts_data->coord_data_size = 4 * ts_data->nFinger_support;
	ts_data->area_data_size = ((ts_data->nFinger_support / 4) + (ts_data->nFinger_support % 4 ? 1 : 0)) * 4;
	ts_data->coordInfoSize = ts_data->coord_data_size + ts_data->area_data_size + 4;
	ts_data->raw_data_frame_size = 128 - ts_data->coord_data_size - ts_data->area_data_size - 4 - 4 - 1;

	if (ts_data->raw_data_frame_size == 0) {
		E("%s: could NOT calculate!\n", __func__);
		return;
	}

	ts_data->raw_data_nframes  = ((uint32_t)ts_data->x_channel * ts_data->y_channel +
									ts_data->x_channel + ts_data->y_channel) / ts_data->raw_data_frame_size +
									(((uint32_t)ts_data->x_channel * ts_data->y_channel +
									ts_data->x_channel + ts_data->y_channel) % ts_data->raw_data_frame_size) ? 1 : 0;
	I("%s: coord_data_size: %d, area_data_size:%d, raw_data_frame_size:%d, raw_data_nframes:%d\n", __func__, ts_data->coord_data_size, ts_data->area_data_size, ts_data->raw_data_frame_size, ts_data->raw_data_nframes);
}

static void calculate_point_number(void)
{
	HX_TOUCH_INFO_POINT_CNT = ic_data->HX_MAX_PT * 4;

	if ((ic_data->HX_MAX_PT % 4) == 0)
		HX_TOUCH_INFO_POINT_CNT += (ic_data->HX_MAX_PT / 4) * 4;
	else
		HX_TOUCH_INFO_POINT_CNT += ((ic_data->HX_MAX_PT / 4) + 1) * 4;
}
#if defined(HX_AUTO_UPDATE_FW)
static int himax_auto_update_check(void)
{
	int32_t ret;
	//int flag_sys_ex_is_same = 0;

	I("%s:Entering!\n", __func__);
	if (g_core_fp.fp_fw_ver_bin() == 0) {
		I("%s: CID major, minor IC: %02X,%02X, bin: %02X,%02X\n", __func__,
				ic_data->vendor_cid_maj_ver, ic_data->vendor_cid_min_ver, g_i_CID_MAJ, g_i_CID_MIN);
		if ((ic_data->vendor_cid_maj_ver != g_i_CID_MAJ) || (ic_data->vendor_cid_min_ver < g_i_CID_MIN)) {
			I("Need to update!\n");
			ret = NO_ERR;
		} else if ((ic_data->vendor_cid_maj_ver == g_i_CID_MAJ) &&
					(ic_data->vendor_cid_min_ver == g_i_CID_MIN) &&
					(g_core_fp.fp_flash_lastdata_check(ic_data->flash_size, i_CTPM_FW, i_CTPM_FW_len) != 0)) {
			I("last 4 bytes check failed!!!!!, need to update\n");
			ret = NO_ERR;
		} else {
			I("No need to update!\n");
			ret = 1;
		}
	} else {
		E("FW bin fail!\n");
		ret = 1;
	}

	return ret;
}

static int i_get_FW(void)
{
	int ret = 0;
	const struct firmware *image = NULL;

	I("file name = %s\n", private_ts->pdata->i_CTPM_firmware_name);
	ret = request_firmware(&image, private_ts->pdata->i_CTPM_firmware_name, private_ts->dev);
	if (ret < 0) {
#if defined(__EMBEDDED_FW__)
		image = &g_embedded_fw;
		I("%s: Couldn't find userspace FW, use embedded FW(size:%zu) instead.\n",
					__func__, g_embedded_fw.size);
#else
		E("%s,fail in line%d error code=%d\n", __func__, __LINE__, ret);
		return OPEN_FILE_FAIL;
#endif
	}

	if (image != NULL) {
		i_CTPM_FW_len = image->size;
		i_CTPM_FW = kcalloc(i_CTPM_FW_len, sizeof(char), GFP_KERNEL);
		memcpy(i_CTPM_FW, image->data, sizeof(char)*i_CTPM_FW_len);

		I("%s firmware ver bin: %02X%02X%02X, firmware ver IC: %02X%02X%02X\n", __func__,
			image->data[CID_VER_MAJ_FLASH_ADDR], image->data[PANEL_VERSION_ADDR], image->data[CID_VER_MIN_FLASH_ADDR],
			ic_data->vendor_cid_maj_ver, ic_data->vendor_panel_ver, ic_data->vendor_cid_min_ver);
		
	} else {
		I("%s: i_CTPM_FW = NULL\n", __func__);
		return OPEN_FILE_FAIL;
	}

	if (ret >= 0)
		release_firmware(image);
	ret = NO_ERR;
	return ret;
}
static int i_update_FW(void)
{
	int upgrade_times = 0;
	int8_t ret = 0, result = 0;

	himax_int_enable(0);


update_retry:

	if (i_CTPM_FW_len == FW_SIZE_32k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k(i_CTPM_FW, i_CTPM_FW_len, false);
	else if (i_CTPM_FW_len == FW_SIZE_60k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k(i_CTPM_FW, i_CTPM_FW_len, false);
	else if (i_CTPM_FW_len == FW_SIZE_64k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k(i_CTPM_FW, i_CTPM_FW_len, false);
	else if (i_CTPM_FW_len == FW_SIZE_124k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k(i_CTPM_FW, i_CTPM_FW_len, false);
	else if (i_CTPM_FW_len == FW_SIZE_128k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k(i_CTPM_FW, i_CTPM_FW_len, false);
	else if (i_CTPM_FW_len == FW_SIZE_255k)
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_255k(i_CTPM_FW, i_CTPM_FW_len, false);

	if (ret == 0) {
		upgrade_times++;
		E("%s: TP upgrade error, upgrade_times = %d\n", __func__, upgrade_times);

		if (upgrade_times < 3)
			goto update_retry;
		else
			result = -1;


	} else {
		g_core_fp.fp_read_FW_ver();
		g_core_fp.fp_touch_information();
		calculate_point_number();
		calcDataSize();
		result = 1;/*upgrade success*/
		I("%s: TP upgrade OK\n", __func__);
	}

	kfree(i_CTPM_FW);
	i_CTPM_FW = NULL;

#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(true, false);
#else
	g_core_fp.fp_sense_on(0x00);
#endif
	himax_int_enable(1);
	return result;
}
#endif


static int himax_loadSensorConfig(struct himax_i2c_platform_data *pdata)
{
	I("%s: initialization complete\n", __func__);
	return NO_ERR;
}

#ifdef HX_ESD_RECOVERY
static void himax_esd_hw_reset(void)
{
#ifdef HX_ZERO_FLASH
	int result = 0;
#endif
	if (g_ts_dbg != 0)
		I("%s: Entering\n", __func__);

	I("START_Himax TP: ESD - Reset\n");

	if (private_ts->in_self_test == 1) {
		I("In self test , not  TP: ESD - Reset\n");
		return;
	}

	g_core_fp.fp_esd_ic_reset();
#ifdef HX_ZERO_FLASH
	I("It will update fw after esd event in zero flash mode!\n");
	result = g_core_fp.fp_0f_operation_dirly();
	if (result) {
		E("Something is wrong! Skip Update with zero flash!\n");
		goto ESCAPE_0F_UPDATE;
	}
	g_core_fp.fp_reload_disable(0);
	g_core_fp.fp_sense_on(0x00);
	himax_report_all_leave_event(private_ts);
	himax_int_enable(1);
ESCAPE_0F_UPDATE:
#endif
	I("END_Himax TP: ESD - Reset\n");
}
#endif

#ifdef HX_SMART_WAKEUP
#ifdef HX_GESTURE_TRACK
static void gest_pt_log_coordinate(int rx, int tx)
{
	/*driver report x y with range 0 - 255 , we scale it up to x/y pixel*/
	gest_pt_x[gest_pt_cnt] = rx * (ic_data->HX_X_RES) / 255;
	gest_pt_y[gest_pt_cnt] = tx * (ic_data->HX_Y_RES) / 255;
}
#endif
static int himax_wake_event_parse(struct himax_ts_data *ts, int ts_status)
{
	uint8_t *buf = wake_event_buffer;
#ifdef HX_GESTURE_TRACK
	int tmp_max_x = 0x00, tmp_min_x = 0xFFFF, tmp_max_y = 0x00, tmp_min_y = 0xFFFF;
	int gest_len;
#endif
	int i = 0, check_FC = 0, ret;
	int j = 0, gesture_pos = 0, gesture_flag = 0;

	if (g_ts_dbg != 0)
		I("%s: Entering!, ts_status=%d\n", __func__, ts_status);

	if (buf == NULL) {
		ret = -ENOMEM;
		goto END;
	}

	memcpy(buf, hx_touch_data->hx_event_buf, hx_touch_data->event_size);

	for (i = 0; i < GEST_PTLG_ID_LEN; i++) {
		for (j = 0; j < GEST_SUP_NUM; j++) {
			if (buf[i] == gest_event[j]) {
				gesture_flag = buf[i];
				gesture_pos = j;
				break;
			}
		}
		I("0x%2.2X ", buf[i]);
		if (buf[i] == gesture_flag) {
			check_FC++;
		} else {
			I("ID START at %x , value = 0x%2X skip the event\n", i, buf[i]);
			break;
		}
	}

	I("Himax gesture_flag= %x\n", gesture_flag);
	I("Himax check_FC is %d\n", check_FC);

	if (check_FC != GEST_PTLG_ID_LEN) {
		ret = 0;
		goto END;
	}

	if (buf[GEST_PTLG_ID_LEN] != GEST_PTLG_HDR_ID1 ||
		buf[GEST_PTLG_ID_LEN + 1] != GEST_PTLG_HDR_ID2) {
		ret = 0;
		goto END;
	}

#ifdef HX_GESTURE_TRACK

	if (buf[GEST_PTLG_ID_LEN] == GEST_PTLG_HDR_ID1 &&
		buf[GEST_PTLG_ID_LEN + 1] == GEST_PTLG_HDR_ID2) {
		gest_len = buf[GEST_PTLG_ID_LEN + 2];
		I("gest_len = %d\n", gest_len);
		i = 0;
		gest_pt_cnt = 0;
		I("gest doornidate start\n %s", __func__);

		while (i < (gest_len + 1) / 2) {
			gest_pt_log_coordinate(buf[GEST_PTLG_ID_LEN + 4 + i * 2], buf[GEST_PTLG_ID_LEN + 4 + i * 2 + 1]);
			i++;
			I("gest_pt_x[%d]=%d\n", gest_pt_cnt, gest_pt_x[gest_pt_cnt]);
			I("gest_pt_y[%d]=%d\n", gest_pt_cnt, gest_pt_y[gest_pt_cnt]);
			gest_pt_cnt += 1;
		}

		if (gest_pt_cnt) {
			for (i = 0; i < gest_pt_cnt; i++) {
				if (tmp_max_x < gest_pt_x[i])
					tmp_max_x = gest_pt_x[i];
				if (tmp_min_x > gest_pt_x[i])
					tmp_min_x = gest_pt_x[i];
				if (tmp_max_y < gest_pt_y[i])
					tmp_max_y = gest_pt_y[i];
				if (tmp_min_y > gest_pt_y[i])
					tmp_min_y = gest_pt_y[i];
			}

			I("gest_point x_min= %d, x_max= %d, y_min= %d, y_max= %d\n", tmp_min_x, tmp_max_x, tmp_min_y, tmp_max_y);
			gest_start_x = gest_pt_x[0];
			hx_gesture_coor[0] = gest_start_x;
			gest_start_y = gest_pt_y[0];
			hx_gesture_coor[1] = gest_start_y;
			gest_end_x = gest_pt_x[gest_pt_cnt - 1];
			hx_gesture_coor[2] = gest_end_x;
			gest_end_y = gest_pt_y[gest_pt_cnt - 1];
			hx_gesture_coor[3] = gest_end_y;
			gest_width = tmp_max_x - tmp_min_x;
			hx_gesture_coor[4] = gest_width;
			gest_height = tmp_max_y - tmp_min_y;
			hx_gesture_coor[5] = gest_height;
			gest_mid_x = (tmp_max_x + tmp_min_x) / 2;
			hx_gesture_coor[6] = gest_mid_x;
			gest_mid_y = (tmp_max_y + tmp_min_y) / 2;
			hx_gesture_coor[7] = gest_mid_y;
			hx_gesture_coor[8] = gest_mid_x;/*gest_up_x*/
			hx_gesture_coor[9] = gest_mid_y - gest_height / 2; /*gest_up_y*/
			hx_gesture_coor[10] = gest_mid_x;/*gest_down_x*/
			hx_gesture_coor[11] = gest_mid_y + gest_height / 2;	/*gest_down_y*/
			hx_gesture_coor[12] = gest_mid_x - gest_width / 2;	/*gest_left_x*/
			hx_gesture_coor[13] = gest_mid_y;	/*gest_left_y*/
			hx_gesture_coor[14] = gest_mid_x + gest_width / 2;	/*gest_right_x*/
			hx_gesture_coor[15] = gest_mid_y; /*gest_right_y*/
		}
	}

#endif

	if (!ts->gesture_cust_en[gesture_pos]) {
		I("%s NOT report key [%d] = %d\n", __func__, gesture_pos, gest_key_def[gesture_pos]);
		g_target_report_data->SMWP_event_chk = 0;
		ret = 0;
	} else {
		g_target_report_data->SMWP_event_chk = gest_key_def[gesture_pos];
		ret = gesture_pos;
	}
END:
	return ret;
}

static void himax_wake_event_report(void)
{
	int KEY_EVENT = g_target_report_data->SMWP_event_chk;

	if (g_ts_dbg != 0)
		I("%s: Entering!\n", __func__);

	if (KEY_EVENT) {
		I(" %s SMART WAKEUP KEY event %d press\n", __func__, KEY_EVENT);
		input_report_key(private_ts->input_dev, KEY_WAKEUP, 1);
		input_sync(private_ts->input_dev);
		I(" %s SMART WAKEUP KEY event %d release\n", __func__, KEY_EVENT);
		input_report_key(private_ts->input_dev, KEY_WAKEUP, 0);
		input_sync(private_ts->input_dev);
		FAKE_POWER_KEY_SEND = true;
#ifdef HX_GESTURE_TRACK
		I("gest_start_x= %d, gest_start_y= %d, gest_end_x= %d, gest_end_y= %d\n", gest_start_x, gest_start_y,
		  gest_end_x, gest_end_y);
		I("gest_width= %d, gest_height= %d, gest_mid_x= %d, gest_mid_y= %d\n", gest_width, gest_height,
		  gest_mid_x, gest_mid_y);
		I("gest_up_x= %d, gest_up_y= %d, gest_down_x= %d, gest_down_y= %d\n", hx_gesture_coor[8], hx_gesture_coor[9],
		  hx_gesture_coor[10], hx_gesture_coor[11]);
		I("gest_left_x= %d, gest_left_y= %d, gest_right_x= %d, gest_right_y= %d\n", hx_gesture_coor[12], hx_gesture_coor[13],
		  hx_gesture_coor[14], hx_gesture_coor[15]);
#endif
		g_target_report_data->SMWP_event_chk = 0;
	}
}

#endif

int himax_report_data_init(void)
{
	if (hx_touch_data->hx_coord_buf != NULL) {
		kfree(hx_touch_data->hx_coord_buf);
		hx_touch_data->hx_coord_buf = NULL;
	}

	if (hx_touch_data->hx_rawdata_buf != NULL) {
		kfree(hx_touch_data->hx_rawdata_buf);
		hx_touch_data->hx_rawdata_buf = NULL;
	}

#if defined(HX_SMART_WAKEUP)
	hx_touch_data->event_size = g_core_fp.fp_get_touch_data_size();

	if (hx_touch_data->hx_event_buf != NULL) {
		kfree(hx_touch_data->hx_event_buf);
		hx_touch_data->hx_event_buf = NULL;
	}

	if (wake_event_buffer != NULL) {
		kfree(wake_event_buffer);
		wake_event_buffer = NULL;
	}

#endif
	hx_touch_data->touch_all_size = g_core_fp.fp_get_touch_data_size();
	hx_touch_data->raw_cnt_max = ic_data->HX_MAX_PT / 4;
	hx_touch_data->raw_cnt_rmd = ic_data->HX_MAX_PT % 4;
	/* more than 4 fingers */
	if (hx_touch_data->raw_cnt_rmd != 0x00) {
		hx_touch_data->rawdata_size = g_core_fp.fp_cal_data_len(hx_touch_data->raw_cnt_rmd, ic_data->HX_MAX_PT, hx_touch_data->raw_cnt_max);
		hx_touch_data->touch_info_size = (ic_data->HX_MAX_PT + hx_touch_data->raw_cnt_max + 2) * 4;
	} else { /* less than 4 fingers */
		hx_touch_data->rawdata_size = g_core_fp.fp_cal_data_len(hx_touch_data->raw_cnt_rmd, ic_data->HX_MAX_PT, hx_touch_data->raw_cnt_max);
		hx_touch_data->touch_info_size = (ic_data->HX_MAX_PT + hx_touch_data->raw_cnt_max + 1) * 4;
	}
#ifdef SEC_PALM_FUNC
	hx_touch_data->touch_info_size += SEC_FINGER_INFO_SZ;
	hx_touch_data->rawdata_size -= SEC_FINGER_INFO_SZ;
#endif
	if ((ic_data->HX_TX_NUM * ic_data->HX_RX_NUM + ic_data->HX_TX_NUM + ic_data->HX_RX_NUM) % hx_touch_data->rawdata_size == 0)
		hx_touch_data->rawdata_frame_size = (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM + ic_data->HX_TX_NUM + ic_data->HX_RX_NUM) / hx_touch_data->rawdata_size;
	else
		hx_touch_data->rawdata_frame_size = (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM + ic_data->HX_TX_NUM + ic_data->HX_RX_NUM) / hx_touch_data->rawdata_size + 1;

	I("%s: rawdata_frame_size = %d\n", __func__, hx_touch_data->rawdata_frame_size);
	I("%s: ic_data->HX_MAX_PT:%d, hx_raw_cnt_max:%d, hx_raw_cnt_rmd:%d, g_hx_rawdata_size:%d, hx_touch_data->touch_info_size:%d\n", __func__, ic_data->HX_MAX_PT, hx_touch_data->raw_cnt_max, hx_touch_data->raw_cnt_rmd, hx_touch_data->rawdata_size, hx_touch_data->touch_info_size);
	hx_touch_data->hx_coord_buf = kzalloc(sizeof(uint8_t) * (hx_touch_data->touch_info_size), GFP_KERNEL);

	if (hx_touch_data->hx_coord_buf == NULL)
		goto mem_alloc_fail;

	if (g_target_report_data == NULL) {
		g_target_report_data = kzalloc(sizeof(struct himax_target_report_data), GFP_KERNEL);
		if (g_target_report_data == NULL)
			goto mem_alloc_fail;
		g_target_report_data->x = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
		if (g_target_report_data->x == NULL)
			goto mem_alloc_fail;
		g_target_report_data->y = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
		if (g_target_report_data->y == NULL)
			goto mem_alloc_fail;
		g_target_report_data->w = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
		if (g_target_report_data->w == NULL)
			goto mem_alloc_fail;
#ifdef SEC_PALM_FUNC
		g_target_report_data->maj = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
		if (g_target_report_data->maj == NULL)
			goto mem_alloc_fail;
		g_target_report_data->min = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
		if (g_target_report_data->min == NULL)
			goto mem_alloc_fail;
		g_target_report_data->palm = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
		if (g_target_report_data->palm == NULL)
			goto mem_alloc_fail;
#endif
		g_target_report_data->finger_id = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
		if (g_target_report_data->finger_id == NULL)
			goto mem_alloc_fail;
		g_target_report_data->mv_cnt = kzalloc(sizeof(int)*(ic_data->HX_MAX_PT), GFP_KERNEL);
		if (g_target_report_data->mv_cnt == NULL)
			goto mem_alloc_fail;
	}
#ifdef HX_SMART_WAKEUP
	g_target_report_data->SMWP_event_chk = 0;
	wake_event_buffer = kcalloc(hx_touch_data->event_size, sizeof(uint8_t), GFP_KERNEL);
	if (wake_event_buffer == NULL)
		goto mem_alloc_fail;
#endif

	hx_touch_data->hx_rawdata_buf = kzalloc(sizeof(uint8_t) * (hx_touch_data->touch_all_size - hx_touch_data->touch_info_size), GFP_KERNEL);

	if (hx_touch_data->hx_rawdata_buf == NULL)
		goto mem_alloc_fail;

#if defined(HX_SMART_WAKEUP)
	hx_touch_data->hx_event_buf = kzalloc(sizeof(uint8_t) * (hx_touch_data->event_size), GFP_KERNEL);

	if (hx_touch_data->hx_event_buf == NULL)
		goto mem_alloc_fail;

#endif
	return NO_ERR;
mem_alloc_fail:
	if(g_target_report_data != NULL) {
		if (g_target_report_data->finger_id != NULL) {
			kfree(g_target_report_data->finger_id);
			g_target_report_data->finger_id = NULL;
		}
#ifdef SEC_PALM_FUNC
		if (g_target_report_data->palm != NULL) {
			kfree(g_target_report_data->palm);
			g_target_report_data->palm = NULL;
		}
		if (g_target_report_data->min != NULL) {
			kfree(g_target_report_data->min);
			g_target_report_data->min = NULL;
		}
		if (g_target_report_data->maj != NULL) {
			kfree(g_target_report_data->maj);
			g_target_report_data->maj = NULL;
		}
#endif
		if (g_target_report_data->w != NULL) {
			kfree(g_target_report_data->w);
			g_target_report_data->w = NULL;
		}
		if (g_target_report_data->y != NULL) {
			kfree(g_target_report_data->y);
			g_target_report_data->y = NULL;
		}
		if (g_target_report_data->x != NULL) {
			kfree(g_target_report_data->x);
			g_target_report_data->x = NULL;
		}
		if (g_target_report_data->mv_cnt != NULL) {
			kfree(g_target_report_data->mv_cnt);
			g_target_report_data->mv_cnt = NULL;
		}
		kfree(g_target_report_data);
		g_target_report_data = NULL;
	}

#if defined(HX_SMART_WAKEUP)
	if (wake_event_buffer != NULL) {
		kfree(wake_event_buffer);
		wake_event_buffer = NULL;
	}
	if (hx_touch_data->hx_event_buf != NULL) {
		kfree(hx_touch_data->hx_event_buf);
		hx_touch_data->hx_event_buf = NULL;
	}
#endif
	if (hx_touch_data->hx_rawdata_buf != NULL) {
		kfree(hx_touch_data->hx_rawdata_buf);
		hx_touch_data->hx_rawdata_buf = NULL;
	}
	if (hx_touch_data->hx_coord_buf != NULL) {
		kfree(hx_touch_data->hx_coord_buf);
		hx_touch_data->hx_coord_buf = NULL;
	}

	I("%s: Memory allocate fail!\n", __func__);
	return MEM_ALLOC_FAIL;
}
EXPORT_SYMBOL(himax_report_data_init);

void himax_report_data_deinit(void)
{
	kfree(g_target_report_data->finger_id);
	g_target_report_data->finger_id = NULL;
#ifdef SEC_PALM_FUNC
	kfree(g_target_report_data->palm);
	g_target_report_data->palm = NULL;
	kfree(g_target_report_data->min);
	g_target_report_data->min = NULL;
	kfree(g_target_report_data->maj);
	g_target_report_data->maj = NULL;	
#endif
	kfree(g_target_report_data->w);
	g_target_report_data->w = NULL;
	kfree(g_target_report_data->y);
	g_target_report_data->y = NULL;
	kfree(g_target_report_data->x);
	g_target_report_data->x = NULL;
	kfree(g_target_report_data);
	g_target_report_data = NULL;

#if defined(HX_SMART_WAKEUP)
	kfree(wake_event_buffer);
	wake_event_buffer = NULL;
	kfree(hx_touch_data->hx_event_buf);
	hx_touch_data->hx_event_buf = NULL;
#endif
	kfree(hx_touch_data->hx_rawdata_buf);
	hx_touch_data->hx_rawdata_buf = NULL;
	kfree(hx_touch_data->hx_coord_buf);
	hx_touch_data->hx_coord_buf = NULL;
}

/*start ts_work*/
#if defined(HX_USB_DETECT_GLOBAL)
void himax_cable_detect_func(bool force_renew)
{
	struct himax_ts_data *ts;

	/*u32 connect_status = 0;*/
	uint8_t connect_status = 0;

	connect_status = USB_detect_flag;/* upmu_is_chr_det(); */
	ts = private_ts;

	/* I("Touch: cable status=%d, cable_config=%p, usb_connected=%d\n", connect_status, ts->cable_config, ts->usb_connected); */
	if (ts->cable_config) {
		if ((connect_status != ts->usb_connected) || force_renew) {
			if (connect_status) {
				ts->cable_config[1] = 0x01;
				ts->usb_connected = 0x01;
			} else {
				ts->cable_config[1] = 0x00;
				ts->usb_connected = 0x00;
			}

			g_core_fp.fp_usb_detect_set(ts->cable_config);
			I("%s: Cable status change: 0x%2.2X\n", __func__, ts->usb_connected);
		}

		/*else */
		/*I("%s: Cable status is the same as previous one, ignore.\n", __func__); */
	}
}
#endif


static void location_detect(struct himax_ts_data *ts, char *loc, int x, int y)
{
	memset(loc, 0, 4);

	if (ts->pdata) {
		if (x < ts->pdata->area_edge)
			strncat(loc, "E.", 2);
		else if (x < (ts->pdata->screenWidth - ts->pdata->area_edge))
			strncat(loc, "C.", 2);
		else
			strncat(loc, "e.", 2);

		if (y < ts->pdata->area_indicator)
			strncat(loc, "S", 1);
		else if (y < (ts->pdata->screenHeight - ts->pdata->area_navigation))
			strncat(loc, "C", 1);
		else
			strncat(loc, "N", 1);
	}
}

void hx_log_touch_event(struct himax_ts_data *ts)
{
	int loop_i = 0;
	char loc[4] = { 0 };
	int x = 0, y = 0, mc = 0, ma = 0, mi = 0;

	for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
		if ((((ts->old_finger >> loop_i & 1) == 0)
			&& (g_target_report_data->finger_id[loop_i] == 1))) {
			ts->touch_count++;
			ts->p_x[loop_i] = x = ts->pre_finger_data[loop_i][0];
			ts->p_y[loop_i] = y = ts->pre_finger_data[loop_i][1];
#ifdef SEC_PALM_FUNC
			ma = ts->pre_finger_data[loop_i][4];
			mi = ts->pre_finger_data[loop_i][5];
#else
			ma = mi = ts->pre_finger_data[loop_i][2];
#endif
			location_detect(ts, loc, x, y);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			I("[P] tID:%d.%d x:%d y:%d p:%d major:%d minor:%d loc:%s tc:%d\n",
				loop_i,
				(ts->input_dev->mt->trkid - 1) & TRKID_MAX, x, y,
				g_target_report_data->palm[loop_i], ma, mi, loc,
				ts->touch_count);
#else
			I("[P] tID:%d.%d p:%d major:%d minor:%d loc:%s tc:%d\n",
				loop_i,
				(ts->input_dev->mt->trkid - 1) & TRKID_MAX,
				g_target_report_data->palm[loop_i], ma, mi, loc,
				ts->touch_count);
#endif
		} else
			if ((((ts->old_finger >> loop_i & 1) == 1)
			&& (g_target_report_data->finger_id[loop_i] == 0))) {
			if (ts->touch_count > 0)
				ts->touch_count--;
			if (ts->touch_count == 0)
				ts->print_info_cnt_release = 0;

			x = ts->pre_finger_data[loop_i][0];
			y = ts->pre_finger_data[loop_i][1];
			mc = ts->pre_finger_data[loop_i][3];

			location_detect(ts, loc, x, y);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			I("[R] tID:%d loc:%s dd:%d,%d pc:%d mc:%d tc:%d lx:%d ly:%d\n",
				loop_i, loc, x - ts->p_x[loop_i], y - ts->p_y[loop_i],
				bitmap_weight(&ts->palm_flag, ts->nFinger_support),
				mc, ts->touch_count, x, y);
#else
			I("[R] tID:%d loc:%s dd:%d,%d pc:%d mc:%d tc:%d\n",
				loop_i, loc, x - ts->p_x[loop_i], y - ts->p_y[loop_i],
				bitmap_weight(&ts->palm_flag, ts->nFinger_support),
				mc, ts->touch_count);
#endif
		}
	}
}

static int himax_ts_work_status(struct himax_ts_data *ts)
{
	/* 1: normal, 2:SMWP */
	int result = HX_REPORT_COORD;

	hx_touch_data->diag_cmd = ts->diag_cmd;
	if (hx_touch_data->diag_cmd)
		result = HX_REPORT_COORD_RAWDATA;

#ifdef HX_SMART_WAKEUP
	if (atomic_read(&ts->suspend_mode) && (!FAKE_POWER_KEY_SEND) && (ts->SMWP_enable) && (!hx_touch_data->diag_cmd))
		result = HX_REPORT_SMWP_EVENT;
#endif
	/* I("Now Status is %d\n", result); */
	return result;
}

static int himax_touch_get(struct himax_ts_data *ts, uint8_t *buf, int ts_path, int ts_status)
{
	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	switch (ts_path) {
	/*normal*/
	case HX_REPORT_COORD:
		if ((HX_HW_RESET_ACTIVATE)
#ifdef HX_ESD_RECOVERY
			|| (HX_ESD_RESET_ACTIVATE)
#endif
			) {
			if (!g_core_fp.fp_read_event_stack(buf, 128)) {
				E("%s: can't read data from chip!\n", __func__);
				ts_status = HX_TS_GET_DATA_FAIL;
			}
		} else {
			if (!g_core_fp.fp_read_event_stack(buf, hx_touch_data->touch_info_size)) {
				E("%s: can't read data from chip!\n", __func__);
				ts_status = HX_TS_GET_DATA_FAIL;
			}
		}
		break;
#if defined(HX_SMART_WAKEUP)

	/*SMWP*/
	case HX_REPORT_SMWP_EVENT:
		__pm_wakeup_event(ts->ts_SMWP_wake_lock, TS_WAKE_LOCK_TIMEOUT);
		msleep(20);
		g_core_fp.fp_burst_enable(0);

		if (!g_core_fp.fp_read_event_stack(buf, hx_touch_data->event_size)) {
			E("%s: can't read data from chip!\n", __func__);
			ts_status = HX_TS_GET_DATA_FAIL;
		}
		break;
#endif
	case HX_REPORT_COORD_RAWDATA:
		if (!g_core_fp.fp_read_event_stack(buf, 128)) {
			E("%s: can't read data from chip!\n", __func__);
			ts_status = HX_TS_GET_DATA_FAIL;
		}
		break;
	default:
		break;
	}

	return ts_status;
}

/* start error_control*/
static int himax_checksum_cal(struct himax_ts_data *ts, uint8_t *buf, int ts_path, int ts_status)
{
	uint16_t check_sum_cal = 0;
	int32_t	i = 0;
	int length = 0;
	int zero_cnt = 0;
	int raw_data_sel = 0;
	int ret_val = ts_status;

	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	/* Normal */
	switch (ts_path) {
	case HX_REPORT_COORD:
		length = hx_touch_data->touch_info_size;
		break;
#if defined(HX_SMART_WAKEUP)
/* SMWP */
	case HX_REPORT_SMWP_EVENT:
		length = (GEST_PTLG_ID_LEN + GEST_PTLG_HDR_LEN);
		break;
#endif
	case HX_REPORT_COORD_RAWDATA:
		length = hx_touch_data->touch_info_size;
		break;
	default:
		I("%s, Neither Normal Nor SMWP error!\n", __func__);
		ret_val = HX_PATH_FAIL;
		goto END_FUNCTION;
	}

	for (i = 0; i < length; i++) {
		check_sum_cal += buf[i];
		if (buf[i] == 0x00)
			zero_cnt++;
	}

	if (check_sum_cal % 0x100 != 0) {
		I("[HIMAX TP MSG] point data_checksum not match : check_sum_cal: 0x%02X\n", check_sum_cal);
		ret_val = HX_CHKSUM_FAIL;
	} else if (zero_cnt == length) {
		if (ts->use_irq)
			I("[HIMAX TP MSG] All Zero event\n");

		ret_val = HX_CHKSUM_FAIL;
	} else {
		raw_data_sel = buf[HX_TOUCH_INFO_POINT_CNT]>>4 & 0x0F;
		/*I("%s:raw_out_sel=%x , hx_touch_data->diag_cmd=%x.\n", __func__, raw_data_sel, hx_touch_data->diag_cmd);*/
		if ((raw_data_sel != 0x0F) && (raw_data_sel != hx_touch_data->diag_cmd)) {/*raw data out not match skip it*/
			/*I("%s:raw data out not match.\n", __func__);*/
			if (!hx_touch_data->diag_cmd) {
				g_core_fp.fp_read_event_stack(buf, (128-hx_touch_data->touch_info_size));/*Need to clear event stack here*/
				/*I("%s: size =%d, buf[0]=%x ,buf[1]=%x, buf[2]=%x, buf[3]=%x.\n", __func__,(128-hx_touch_data->touch_info_size), buf[0], buf[1], buf[2], buf[3]);*/
				/*I("%s:also clear event stack.\n", __func__);*/
			}
			ret_val = HX_READY_SERVE;
		}
	}

END_FUNCTION:
	if (g_ts_dbg != 0)
		I("%s: END, ret_val=%d!\n", __func__, ret_val);
	return ret_val;
}

#ifdef HX_ESD_RECOVERY
#ifdef HX_ZERO_FLASH
void hx_update_dirly_0f(void)
{
	I("It will update fw after esd event in zero flash mode!\n");
	g_core_fp.fp_0f_operation_dirly();
}
#endif
static int himax_ts_event_check(struct himax_ts_data *ts, uint8_t *buf, int ts_path, int ts_status)
{
	int hx_EB_event = 0;
	int hx_EC_event = 0;
	int hx_ED_event = 0;
	int hx_esd_event = 0;
	int hx_zero_event = 0;
	int shaking_ret = 0;

	int32_t	loop_i = 0;
	int length = 0;
	int ret_val = ts_status;

	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	/* Normal */
	switch (ts_path) {
	case HX_REPORT_COORD:
#ifdef SEC_PALM_FUNC
		length = ts->coordInfoSize;
#else
		length = hx_touch_data->touch_info_size;
#endif
		break;
#if defined(HX_SMART_WAKEUP)
/* SMWP */
	case HX_REPORT_SMWP_EVENT:
		length = (GEST_PTLG_ID_LEN + GEST_PTLG_HDR_LEN);
		break;
#endif
	case HX_REPORT_COORD_RAWDATA:
#ifdef SEC_PALM_FUNC
		length = ts->coordInfoSize;
#else
		length = hx_touch_data->touch_info_size;
#endif
		break;
	default:
		I("%s, Neither Normal Nor SMWP error!\n", __func__);
		ret_val = HX_PATH_FAIL;
		goto END_FUNCTION;
	}

	if (g_ts_dbg != 0)
		I("Now Path=%d, Now status=%d, length=%d\n", ts_path, ts_status, length);

	if (ts_path == HX_REPORT_COORD || ts_path == HX_REPORT_COORD_RAWDATA) {
		for (loop_i = 0; loop_i < length; loop_i++) {
			/* case 1 ESD recovery flow */
			if (buf[loop_i] == 0xEB) {
				hx_EB_event++;
			} else if (buf[loop_i] == 0xEC) {
				hx_EC_event++;
			} else if (buf[loop_i] == 0xED) {
				hx_ED_event++;
			} else if (buf[loop_i] == 0x00) { /* case 2 ESD recovery flow-Disable */
				hx_zero_event++;
			} else {
				g_zero_event_count = 0;
				break;
			}
		}
	}

	if (hx_EB_event == length) {
		hx_esd_event = length;
		hx_EB_event_flag++;
		I("[HIMAX TP MSG]: ESD event checked - ALL 0xEB.\n");
	} else if (hx_EC_event == length) {
		hx_esd_event = length;
		hx_EC_event_flag++;
		I("[HIMAX TP MSG]: ESD event checked - ALL 0xEC.\n");
	} else if (hx_ED_event == length) {
		hx_esd_event = length;
		hx_ED_event_flag++;
		I("[HIMAX TP MSG]: ESD event checked - ALL 0xED.\n");
	}

	if ((hx_esd_event == length || hx_zero_event == length)
		&& (HX_HW_RESET_ACTIVATE == 0)
		&& (HX_ESD_RESET_ACTIVATE == 0)
		&& (hx_touch_data->diag_cmd == 0)
		&& (ts->in_self_test == 0)) {
		shaking_ret = g_core_fp.fp_ic_esd_recovery(hx_esd_event, hx_zero_event, length);

		if (shaking_ret == HX_ESD_EVENT) {
			himax_esd_hw_reset();
			ret_val = HX_ESD_EVENT;
		} else if (shaking_ret == HX_ZERO_EVENT_COUNT) {
			ret_val = HX_ZERO_EVENT_COUNT;
		} else {
			I("I2C running. Nothing to be done!\n");
			ret_val = HX_IC_RUNNING;
		}
	} else if (HX_ESD_RESET_ACTIVATE) { /* drop 1st interrupts after chip reset */
		HX_ESD_RESET_ACTIVATE = 0;
		I("[HX_ESD_RESET_ACTIVATE]:%s: Back from reset, ready to serve.\n", __func__);
		ret_val = HX_ESD_REC_OK;
	}

END_FUNCTION:
	if (g_ts_dbg != 0)
		I("%s: END, ret_val=%d!\n", __func__, ret_val);

	return ret_val;
}
#endif

static int himax_err_ctrl(struct himax_ts_data *ts, uint8_t *buf, int ts_path, int ts_status)
{
#ifdef HX_RST_PIN_FUNC
	if (HX_HW_RESET_ACTIVATE) {
		/* drop 1st interrupts after chip reset */
		HX_HW_RESET_ACTIVATE = 0;
		I("[HX_HW_RESET_ACTIVATE]:%s: Back from reset, ready to serve.\n", __func__);
		ts_status = HX_RST_OK;
		goto END_FUNCTION;
	}
#endif

	ts_status = himax_checksum_cal(ts, buf, ts_path, ts_status);
	if (ts_status == HX_CHKSUM_FAIL) {
		goto CHK_FAIL;
	} else {
#ifdef HX_ESD_RECOVERY
		/* continuous N times record, not total N times. */
		g_zero_event_count = 0;
#endif
		goto END_FUNCTION;
	}

CHK_FAIL:
#ifdef HX_ESD_RECOVERY
	ts_status = himax_ts_event_check(ts, buf, ts_path, ts_status);
#endif


END_FUNCTION:
	if (g_ts_dbg != 0)
		I("%s: END, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}
/* end error_control*/

/* start distribute_data*/
static int himax_distribute_touch_data(uint8_t *buf, int ts_path, int ts_status)
{
	uint8_t hx_state_info_pos = hx_touch_data->touch_info_size - 3;

#ifdef SEC_PALM_FUNC
	hx_state_info_pos -= SEC_FINGER_INFO_SZ;
#endif

	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	if (ts_path == HX_REPORT_COORD) {
		memcpy(hx_touch_data->hx_coord_buf, &buf[0], hx_touch_data->touch_info_size);

		if (buf[hx_state_info_pos] != 0xFF && buf[hx_state_info_pos + 1] != 0xFF)
			memcpy(hx_touch_data->hx_state_info, &buf[hx_state_info_pos], 2);
		else
			memset(hx_touch_data->hx_state_info, 0x00, sizeof(hx_touch_data->hx_state_info));

		if ((HX_HW_RESET_ACTIVATE)
#ifdef HX_ESD_RECOVERY
		|| (HX_ESD_RESET_ACTIVATE)
#endif
		) {
			memcpy(hx_touch_data->hx_rawdata_buf, &buf[hx_touch_data->touch_info_size], hx_touch_data->touch_all_size - hx_touch_data->touch_info_size);
		}
	} else if (ts_path == HX_REPORT_COORD_RAWDATA) {
		memcpy(hx_touch_data->hx_coord_buf, &buf[0], hx_touch_data->touch_info_size);

		if (buf[hx_state_info_pos] != 0xFF && buf[hx_state_info_pos + 1] != 0xFF)
			memcpy(hx_touch_data->hx_state_info, &buf[hx_state_info_pos], 2);
		else
			memset(hx_touch_data->hx_state_info, 0x00, sizeof(hx_touch_data->hx_state_info));

		memcpy(hx_touch_data->hx_rawdata_buf, &buf[hx_touch_data->touch_info_size], hx_touch_data->touch_all_size - hx_touch_data->touch_info_size);
#if defined(HX_SMART_WAKEUP)
	} else if (ts_path == HX_REPORT_SMWP_EVENT) {
		memcpy(hx_touch_data->hx_event_buf, buf, hx_touch_data->event_size);
#endif
	} else {
		E("%s, Fail Path!\n", __func__);
		ts_status = HX_PATH_FAIL;
	}
	/* debug info start */
	if (buf[hx_state_info_pos] != 0xFF && buf[hx_state_info_pos + 1] != 0xFF) {
#ifdef HX_NEW_EVENT_STACK_FORMAT
		if (g_last_fw_irq_flag != hx_touch_data->hx_state_info[0]) {
			if ((g_last_fw_irq_flag & 0x03) != (hx_touch_data->hx_state_info[0] & 0x03))
				I("%s ReCal change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[0] & 0x03);
			if ((g_last_fw_irq_flag >> 3 & 0x01) != (hx_touch_data->hx_state_info[0] >> 3 & 0x01))
				I("%s Palm change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[0] >> 3 & 0x01);
			if ((g_last_fw_irq_flag >> 7 & 0x01) != (hx_touch_data->hx_state_info[0] >> 7 & 0x01))
				I("%s AC mode change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[0] >> 7 & 0x01);
			if ((g_last_fw_irq_flag >> 5 & 0x01) != (hx_touch_data->hx_state_info[0] >> 5 & 0x01))
				I("%s Water change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[0] >> 5 & 0x01);
			if ((g_last_fw_irq_flag >> 6 & 0x01) != (hx_touch_data->hx_state_info[0] >> 6 & 0x01))
				I("%s TX Hop change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[0] >> 6 & 0x01);
		}
		if (g_last_fw_irq_flag2 != hx_touch_data->hx_state_info[1]) {
			if ((g_last_fw_irq_flag2 & 0x01) != (hx_touch_data->hx_state_info[1] & 0x01))
				I("%s High Sensitivity change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[1] & 0x01);

			private_ts->noise_mode = hx_touch_data->hx_state_info[1] >> 3 & 0x01;
			if ((g_last_fw_irq_flag2 >> 3 & 0x01) != private_ts->noise_mode) {
				I("%s nosie mode change to %d\n",
						HIMAX_LOG_TAG, private_ts->noise_mode);
			}

			private_ts->lamp_noise_mode =  hx_touch_data->hx_state_info[1] >> 4 & 0x01;
			if ((g_last_fw_irq_flag2 >> 4 & 0x01) != private_ts->lamp_noise_mode) {
				I("%s lamp noise mode change to %d\n",
						HIMAX_LOG_TAG, private_ts->lamp_noise_mode);
			}
		}
		g_last_fw_irq_flag = hx_touch_data->hx_state_info[0];
		g_last_fw_irq_flag2 = hx_touch_data->hx_state_info[1];
#else
		if (g_last_fw_irq_flag != hx_touch_data->hx_state_info[0]) {
			if ((g_last_fw_irq_flag & 0x01) != (hx_touch_data->hx_state_info[0] & 0x01))
				I("%s ReCal change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[0] & 0x01);
			if ((g_last_fw_irq_flag >> 1 & 0x01) != (hx_touch_data->hx_state_info[0] >> 1 & 0x01))
				I("%s Palm change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[0] >> 1 & 0x01);
			if ((g_last_fw_irq_flag >> 2 & 0x01) != (hx_touch_data->hx_state_info[0] >> 2 & 0x01))
				I("%s AC mode change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[0] >> 2 & 0x01);
			if ((g_last_fw_irq_flag >> 3 & 0x01) != (hx_touch_data->hx_state_info[0] >> 3 & 0x01))
				I("%s Water change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[0] >> 3 & 0x01);
			if ((g_last_fw_irq_flag >> 4 & 0x01) != (hx_touch_data->hx_state_info[0] >> 4 & 0x01))
				I("%s Glove change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[0] >> 4 & 0x01);
			if ((g_last_fw_irq_flag >> 5 & 0x01) != (hx_touch_data->hx_state_info[0] >> 5 & 0x01))
				I("%s TX Hop change to %d\n",
						HIMAX_LOG_TAG, hx_touch_data->hx_state_info[0] >> 5 & 0x01);
		}
		g_last_fw_irq_flag = hx_touch_data->hx_state_info[0];
#endif
	}
	/* debug info end */

	if (g_ts_dbg != 0)
		I("%s: End, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}
/* end assign_data*/

/* start parse_report_data*/
int himax_parse_report_points(struct himax_ts_data *ts, int ts_path, int ts_status)
{
	int x = 0, y = 0, w = 0;
#ifdef SEC_PALM_FUNC
	int maj = 0, min = 0, palm = 0;
#endif
	int base = 0;
	int32_t	loop_i = 0;

	if (g_ts_dbg != 0)
		I("%s: start!\n", __func__);

	ts->old_finger = ts->pre_finger_mask;
	if (ts->hx_point_num == 0) {
		if (g_ts_dbg != 0)
			I("%s: hx_point_num = 0!\n", __func__);

		for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
			if (g_target_report_data->finger_id[loop_i] != 0) {
				g_target_report_data->finger_id[loop_i] = 0;
				ts->pre_finger_data[loop_i][3] =
					g_target_report_data->mv_cnt[loop_i];
				g_target_report_data->mv_cnt[loop_i] = 0;
			}
		}
		return ts_status;
	}
	ts->pre_finger_mask = 0;
	hx_touch_data->finger_num = hx_touch_data->hx_coord_buf[ts->coordInfoSize - 4] & 0x0F;
	hx_touch_data->finger_on = 1;
	AA_press = 1;

	g_target_report_data->finger_num = hx_touch_data->finger_num;
	g_target_report_data->finger_on = hx_touch_data->finger_on;
	g_target_report_data->ig_count = hx_touch_data->hx_coord_buf[ts->coordInfoSize - 5];
#ifdef SEC_PALM_FUNC
#ifdef HX_NEW_EVENT_STACK_FORMAT
	palm = (hx_touch_data->hx_state_info[0] >> 3 & 0x01);
#else
	palm = (hx_touch_data->hx_state_info[0] >> 1 & 0x01);
#endif
#endif

	if (g_ts_dbg != 0)
		I("%s:finger_num = 0x%2X, finger_on = %d\n", __func__, g_target_report_data->finger_num, g_target_report_data->finger_on);

	for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
		base = loop_i * 4;
		x = hx_touch_data->hx_coord_buf[base] << 8 | hx_touch_data->hx_coord_buf[base + 1];
		y = (hx_touch_data->hx_coord_buf[base + 2] << 8 | hx_touch_data->hx_coord_buf[base + 3]);
		w = hx_touch_data->hx_coord_buf[(ts->nFinger_support * 4) + loop_i];
#ifdef SEC_PALM_FUNC
		maj = hx_touch_data->hx_coord_buf[ts->coordInfoSize+(loop_i*2)];
		min = hx_touch_data->hx_coord_buf[ts->coordInfoSize+(loop_i*2)+1];
		//palm = ((hx_touch_data->hx_coord_buf[ts->coordInfoSize+(ts->nFinger_support * 2)] << 8 | hx_touch_data->hx_coord_buf[ts->coordInfoSize+(ts->nFinger_support * 2)+1]) & (1<<loop_i))>>(loop_i);
#endif

		if (g_ts_dbg != 0)
#ifndef SEC_PALM_FUNC
			D("%s: now parsing[%d]:x=%d, y=%d, w=%d\n", __func__, loop_i, x, y, w);
#else
			D("%s: now parsing[%d]:x=%d, y=%d, w=%d, maj=%d, min=%d, palm=%d\n", __func__, loop_i, x, y, w, maj, min, palm);
#endif

		if (x >= 0 && x <= ts->pdata->abs_x_max && y >= 0 && y <= ts->pdata->abs_y_max) {
			hx_touch_data->finger_num--;

			g_target_report_data->x[loop_i] = x;
			g_target_report_data->y[loop_i] = y;
			g_target_report_data->w[loop_i] = w;
#ifdef SEC_PALM_FUNC
			g_target_report_data->maj[loop_i] = maj;
			g_target_report_data->min[loop_i] = min;
			g_target_report_data->palm[loop_i] = palm;
			if (palm)
				ts->palm_flag |= BIT(loop_i);
			else
				ts->palm_flag &= ~BIT(loop_i);
#endif
			g_target_report_data->finger_id[loop_i] = 1;
			g_target_report_data->mv_cnt[loop_i]++;

			/*I("%s: g_target_report_data->x[loop_i]=%d, g_target_report_data->y[loop_i]=%d, g_target_report_data->w[loop_i]=%d",*/
			/*__func__, g_target_report_data->x[loop_i], g_target_report_data->y[loop_i], g_target_report_data->w[loop_i]); */


			if (!ts->first_pressed) {
				ts->first_pressed = 1;
				I("S1@%d, %d\n", x, y);
			}

			ts->pre_finger_data[loop_i][0] = x;
			ts->pre_finger_data[loop_i][1] = y;
#ifdef SEC_PALM_FUNC
			ts->pre_finger_data[loop_i][4] = maj;
			ts->pre_finger_data[loop_i][5] = min;
#endif

			ts->pre_finger_mask = ts->pre_finger_mask + (1 << loop_i);
		} else {/* report coordinates */
			g_target_report_data->x[loop_i] = x;
			g_target_report_data->y[loop_i] = y;
			g_target_report_data->w[loop_i] = w;
#ifdef SEC_PALM_FUNC
			g_target_report_data->maj[loop_i] = maj;
			g_target_report_data->min[loop_i] = min;
			g_target_report_data->palm[loop_i] = 0;
			ts->palm_flag &= ~BIT(loop_i);
#endif
			g_target_report_data->finger_id[loop_i] = 0;
			ts->pre_finger_data[loop_i][3] = g_target_report_data->mv_cnt[loop_i];
			g_target_report_data->mv_cnt[loop_i] = 0;

			if (loop_i == 0 && ts->first_pressed == 1) {
				ts->first_pressed = 2;
				I("E1@%d, %d\n", ts->pre_finger_data[0][0], ts->pre_finger_data[0][1]);
			}
		}
	}

	if (g_ts_dbg != 0) {
		for (loop_i = 0; loop_i < 10; loop_i++)
			D("DBG X=%d  Y=%d ID=%d\n", g_target_report_data->x[loop_i], g_target_report_data->y[loop_i], g_target_report_data->finger_id[loop_i]);

		D("DBG finger number %d\n", g_target_report_data->finger_num);
	}

	if (g_ts_dbg != 0)
		I("%s: end!\n", __func__);
	return ts_status;
}

static int himax_parse_report_data(struct himax_ts_data *ts, int ts_path, int ts_status)
{

	if (g_ts_dbg != 0)
		I("%s: start now_status=%d!\n", __func__, ts_status);


	EN_NoiseFilter = (hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 3);
	/* I("EN_NoiseFilter=%d\n", EN_NoiseFilter); */
	EN_NoiseFilter = EN_NoiseFilter & 0x01;
	/* I("EN_NoiseFilter2=%d\n", EN_NoiseFilter); */
#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
	tpd_key = (hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 4);

	/* All (VK+AA)leave */
	if (tpd_key == 0x0F)
		tpd_key = 0x00;

#endif
	p_point_num = ts->hx_point_num;

	if (hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT] == 0xff)
		ts->hx_point_num = 0;
	else
		ts->hx_point_num = hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT] & 0x0f;


	switch (ts_path) {
	case HX_REPORT_COORD:
		ts_status = himax_parse_report_points(ts, ts_path, ts_status);
		break;
	case HX_REPORT_COORD_RAWDATA:
		/* touch monitor rawdata */
		if (debug_data != NULL) {
			if (debug_data->fp_set_diag_cmd(ic_data, hx_touch_data))
				I("%s: raw data_checksum not match\n", __func__);
		} else {
			E("%s,There is no init set_diag_cmd\n", __func__);
		}
		ts_status = himax_parse_report_points(ts, ts_path, ts_status);
		break;
#ifdef HX_SMART_WAKEUP
	case HX_REPORT_SMWP_EVENT:
		himax_wake_event_parse(ts, ts_status);
		break;
#endif
	default:
		E("%s:Fail Path!\n", __func__);
		ts_status = HX_PATH_FAIL;
		break;
	}
	if (g_ts_dbg != 0)
		I("%s: end now_status=%d!\n", __func__, ts_status);
	return ts_status;
}

/* end parse_report_data*/

static void himax_report_all_leave_event(struct himax_ts_data *ts)
{
	int loop_i = 0;
	int x = 0, y = 0, mc = 0;
	char loc[4] = { 0 };

	if (ts->prox_power_off == 1) {
		input_report_key(ts->input_dev, KEY_INT_CANCEL, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, KEY_INT_CANCEL, 0);
		input_sync(ts->input_dev);
	}

	for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
#ifndef	HX_PROTOCOL_A
		input_mt_slot(ts->input_dev, loop_i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#endif
		if (g_target_report_data->finger_id[loop_i] != 0) {
			g_target_report_data->finger_id[loop_i] = 0;
			ts->touch_count--;

			x = g_target_report_data->x[loop_i];
			y = g_target_report_data->y[loop_i];
			mc = g_target_report_data->mv_cnt[loop_i];

			location_detect(ts, loc, x, y);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			I("[RA] tID:%d loc:%s dd:%d,%d mc:%d tc:%d lx:%d ly:%d\n", loop_i,
				loc, x - ts->p_x[loop_i], y - ts->p_y[loop_i], mc, ts->touch_count, x, y);
#else
			I("[RA] tID:%d loc:%s dd:%d,%d mc:%d tc:%d\n", loop_i,
				loc, x - ts->p_x[loop_i], y - ts->p_y[loop_i], mc, ts->touch_count);
#endif
		}
	}

#ifdef SEC_PALM_FUNC
	ts->palm_flag = 0;
	input_report_key(ts->input_dev, BTN_PALM, 0);
#endif
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
	input_sync(ts->input_dev);

	ts->touch_count = 0;
}

/* start report_data */
#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
static void himax_key_report_operation(int tp_key_index, struct himax_ts_data *ts)
{
	uint16_t x_position = 0, y_position = 0;

	if (g_ts_dbg != 0)
		I("%s: Entering\n", __func__);

	if (tp_key_index != 0x00) {
		I("virtual key index =%x\n", tp_key_index);

		if (tp_key_index == 0x01) {
			vk_press = 1;
			I("back key pressed\n");

			if (ts->pdata->virtual_key) {
				if (ts->button[0].index) {
					x_position = (ts->button[0].x_range_min + ts->button[0].x_range_max) / 2;
					y_position = (ts->button[0].y_range_min + ts->button[0].y_range_max) / 2;
				}

#ifdef	HX_PROTOCOL_A
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
				input_mt_sync(ts->input_dev);
#else
				input_mt_slot(ts->input_dev, 0);
				input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
#endif
			}	else {
				input_report_key(ts->input_dev, KEY_BACK, 1);
			}
		} else if (tp_key_index == 0x02) {
			vk_press = 1;
			I("home key pressed\n");

			if (ts->pdata->virtual_key) {
				if (ts->button[1].index) {
					x_position = (ts->button[1].x_range_min + ts->button[1].x_range_max) / 2;
					y_position = (ts->button[1].y_range_min + ts->button[1].y_range_max) / 2;
				}

#ifdef	HX_PROTOCOL_A
				input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
				input_mt_sync(ts->input_dev);
#else
				input_mt_slot(ts->input_dev, 0);
				input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
#endif
			} else {
				input_report_key(ts->input_dev, KEY_HOME, 1);
			}
		} else if (tp_key_index == 0x04) {
			vk_press = 1;
			I("APP_switch key pressed\n");

			if (ts->pdata->virtual_key) {
				if (ts->button[2].index) {
					x_position = (ts->button[2].x_range_min + ts->button[2].x_range_max) / 2;
					y_position = (ts->button[2].y_range_min + ts->button[2].y_range_max) / 2;
				}

#ifdef HX_PROTOCOL_A
				input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
				input_mt_sync(ts->input_dev);
#else
				input_mt_slot(ts->input_dev, 0);
				input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y_position);
#endif
			} else {
				input_report_key(ts->input_dev, KEY_APPSELECT, 1);
			}
		}
		input_sync(ts->input_dev);
	} else { /*tp_key_index =0x00*/
		I("virtual key released\n");
		vk_press = 0;
#ifndef	HX_PROTOCOL_A
		input_mt_slot(ts->input_dev, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#else
		input_mt_sync(ts->input_dev);
#endif
		input_report_key(ts->input_dev, KEY_BACK, 0);
		input_report_key(ts->input_dev, KEY_HOME, 0);
		input_report_key(ts->input_dev, KEY_APPSELECT, 0);
#ifndef	HX_PROTOCOL_A
		input_sync(ts->input_dev);
#endif
	}
}

void himax_finger_report_key(struct himax_ts_data *ts)
{
	if (ts->hx_point_num != 0) {
		/*Touch KEY*/
		if ((tpd_key_old != 0x00) && (tpd_key == 0x00)) {
			/* temp_x[0] = 0xFFFF;
			 * temp_y[0] = 0xFFFF;
			 * temp_x[1] = 0xFFFF;
			 * temp_y[1] = 0xFFFF;
			 */
			hx_touch_data->finger_on = 0;
#ifdef HX_PROTOCOL_A
			input_report_key(ts->input_dev, BTN_TOUCH, 0);
#endif
			himax_key_report_operation(tpd_key, ts);
#ifndef HX_PROTOCOL_A
			input_report_key(ts->input_dev, BTN_TOUCH, 0);
#endif
		}
		input_sync(ts->input_dev);
	}
}

void himax_finger_leave_key(struct himax_ts_data *ts)
{
	if (tpd_key != 0x00) {
		hx_touch_data->finger_on = 1;
#ifdef HX_PROTOCOL_A
		input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif
		himax_key_report_operation(tpd_key, ts);
#ifndef HX_PROTOCOL_A
		input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif
	} else if ((tpd_key_old != 0x00) && (tpd_key == 0x00)) {
		hx_touch_data->finger_on = 0;
#ifdef HX_PROTOCOL_A
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
#endif
		himax_key_report_operation(tpd_key, ts);
#ifndef HX_PROTOCOL_A
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
#endif
	}
	input_sync(ts->input_dev);
}

static void himax_report_key(struct himax_ts_data *ts)
{
	if (ts->hx_point_num != 0) { /* Touch KEY */
		himax_finger_report_key(ts);
	} else { /* Key */
		himax_finger_leave_key(ts);
	}

	tpd_key_old = tpd_key;
	Last_EN_NoiseFilter = EN_NoiseFilter;
}
#endif

/* start report_point*/
static void himax_finger_report(struct himax_ts_data *ts)
{
	int i = 0;
	bool valid = false;


	if (g_ts_dbg != 0) {
		I("%s:start\n", __func__);
		I("hx_touch_data->finger_num=%d\n", hx_touch_data->finger_num);
	}
	for (i = 0; i < ts->nFinger_support; i++) {
		if (g_target_report_data->x[i] >= 0 && g_target_report_data->x[i] <= ts->pdata->abs_x_max && g_target_report_data->y[i] >= 0 && g_target_report_data->y[i] <= ts->pdata->abs_y_max)
			valid = true;
		else
			valid = false;
		if (g_ts_dbg != 0)
			I("valid=%d\n", valid);
		if (valid) {
			if (g_ts_dbg != 0)
				I("g_target_report_data->x[i]=%d, g_target_report_data->y[i]=%d, g_target_report_data->w[i]=%d\n", g_target_report_data->x[i], g_target_report_data->y[i], g_target_report_data->w[i]);
#ifndef	HX_PROTOCOL_A
			input_mt_slot(ts->input_dev, i);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
#else
			input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif
#ifdef SEC_PALM_FUNC
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_target_report_data->maj[i]);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, g_target_report_data->min[i]);
#else
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_target_report_data->w[i]);
#endif

#ifndef	HX_PROTOCOL_A
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, g_target_report_data->w[i]);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, g_target_report_data->w[i]);
#else
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
#endif
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_target_report_data->x[i]);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_target_report_data->y[i]);
#ifndef	HX_PROTOCOL_A
			ts->last_slot = i;
#else
			input_mt_sync(ts->input_dev);
#endif
		} else {
#ifndef	HX_PROTOCOL_A
			input_mt_slot(ts->input_dev, i);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#endif
		}
	}
#ifndef	HX_PROTOCOL_A
#ifdef SEC_PALM_FUNC
	input_report_key(ts->input_dev, BTN_PALM, ts->palm_flag);
#endif
	input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif
	input_sync(ts->input_dev);

	if (g_ts_dbg != 0)
		I("%s:end\n", __func__);
}

static void himax_finger_leave(struct himax_ts_data *ts)
{
#ifndef	HX_PROTOCOL_A
	int32_t loop_i = 0;
#endif

	if (g_ts_dbg != 0)
		I("%s: start!\n", __func__);
#if defined(HX_PALM_REPORT)
	if (himax_palm_detect(hx_touch_data->hx_coord_buf) == PALM_REPORT) {
		I(" %s HX_PALM_REPORT KEY power event press\n", __func__);
		input_report_key(ts->input_dev, KEY_POWER, 1);
		input_sync(ts->input_dev);
		msleep(100);

		I(" %s HX_PALM_REPORT KEY power event release\n", __func__);
		input_report_key(ts->input_dev, KEY_POWER, 0);
		input_sync(ts->input_dev);
		return;
	}
#endif

	hx_touch_data->finger_on = 0;
	g_target_report_data->finger_on  = 0;
	g_target_report_data->finger_num = 0;
	AA_press = 0;

#ifdef HX_PROTOCOL_A
	input_mt_sync(ts->input_dev);
#endif
#ifndef	HX_PROTOCOL_A
	for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
		input_mt_slot(ts->input_dev, loop_i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
	}
#endif
	if (ts->pre_finger_mask > 0)
		ts->pre_finger_mask = 0;

	if (ts->first_pressed == 1) {
		ts->first_pressed = 2;
		I("E1@%d, %d\n", ts->pre_finger_data[0][0], ts->pre_finger_data[0][1]);
	}

	/*if (ts->debug_log_level & BIT(1)) */
	/*himax_log_touch_event(x, y, w, loop_i, EN_NoiseFilter, HX_FINGER_LEAVE); */

#ifdef SEC_PALM_FUNC
	ts->palm_flag = 0;
	input_report_key(ts->input_dev, BTN_PALM, ts->palm_flag);
#endif
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
	input_sync(ts->input_dev);

	if (g_ts_dbg != 0)
		I("%s: end!\n", __func__);


}

static void himax_report_points(struct himax_ts_data *ts)
{
	if (g_ts_dbg != 0)
		I("%s: start!\n", __func__);

	if (ts->hx_point_num != 0)
		himax_finger_report(ts);
	else
		himax_finger_leave(ts);

	Last_EN_NoiseFilter = EN_NoiseFilter;

	hx_log_touch_event(ts);

	if (g_ts_dbg != 0)
		I("%s: end!\n", __func__);
}
/* end report_points*/

int himax_report_data(struct himax_ts_data *ts, int ts_path, int ts_status)
{
	if (g_ts_dbg != 0)
		I("%s: Entering, ts_status=%d!\n", __func__, ts_status);

	if (ts_path == HX_REPORT_COORD || ts_path == HX_REPORT_COORD_RAWDATA) {
		/* Touch Point information */
		himax_report_points(ts);

#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
		/*report key(question mark)*/
		if (tpd_key && tpd_key_old)
			himax_report_key(ts);

#endif
#ifdef HX_SMART_WAKEUP
	} else if (ts_path == HX_REPORT_SMWP_EVENT) {
		himax_wake_event_report();
#endif
	} else {
		E("%s:Fail Path!\n", __func__);
		ts_status = HX_PATH_FAIL;
	}

	if (g_ts_dbg != 0)
		I("%s: END, ts_status=%d!\n", __func__, ts_status);
	return ts_status;
}
/* end report_data */

static int himax_ts_operation(struct himax_ts_data *ts, int ts_path, int ts_status)
{
	uint8_t hw_reset_check[2];

	memset(ts->xfer_buff, 0x00, 128 * sizeof(uint8_t));
	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));

	ts_status = himax_touch_get(ts, ts->xfer_buff, ts_path, ts_status);
	if (ts_status == HX_TS_GET_DATA_FAIL)
		goto END_FUNCTION;

	ts_status = himax_distribute_touch_data(ts->xfer_buff, ts_path, ts_status);
	ts_status = himax_err_ctrl(ts, ts->xfer_buff, ts_path, ts_status);
	if (ts_status == HX_REPORT_DATA || ts_status == HX_TS_NORMAL_END)
		ts_status = himax_parse_report_data(ts, ts_path, ts_status);
	else
		goto END_FUNCTION;


	ts_status = himax_report_data(ts, ts_path, ts_status);


END_FUNCTION:
	return ts_status;
}

void himax_ts_work(struct himax_ts_data *ts)
{

	int ts_status = HX_TS_NORMAL_END;
	int ts_path = 0;
	int ret = 0;

	if (atomic_read(&ts->suspend_mode) == HIMAX_STATE_POWER_OFF) {
		input_err(true, ts->dev, "%s: POWER_OFF & not handled\n", __func__);
		return;
	}

	if (atomic_read(&ts->suspend_mode) == HIMAX_STATE_LPM) {
		__pm_wakeup_event(ts->ts_SMWP_wake_lock, TS_WAKE_LOCK_TIMEOUT);

		/* waiting for blsp block resuming, if not occurs spi error */
		ret = wait_for_completion_interruptible_timeout(&ts->resume_done, msecs_to_jiffies(1000));
		if (ret == 0) {
			input_err(true, ts->dev, "%s: LPM: pm resume is not handled\n", __func__);
			return;
		}

		if (ret < 0) {
			input_err(true, ts->dev, "%s: LPM: -ERESTARTSYS if interrupted, %d\n", __func__, ret);
			return;
		}

		input_info(true, ts->dev, "%s: run LPM interrupt handler, %d\n", __func__, ret);
		/* run lpm interrupt handler */
	}

	if (debug_data != NULL)
		debug_data->fp_ts_dbg_func(ts, HX_FINGER_ON);

	ts_path = himax_ts_work_status(ts);
	switch (ts_path) {
	case HX_REPORT_COORD:
		ts_status = himax_ts_operation(ts, ts_path, ts_status);
		break;
	case HX_REPORT_SMWP_EVENT:
		ts_status = himax_ts_operation(ts, ts_path, ts_status);
		break;
	case HX_REPORT_COORD_RAWDATA:
		ts_status = himax_ts_operation(ts, ts_path, ts_status);
		break;
	default:
		E("%s:Path Fault! value=%d\n", __func__, ts_path);
		goto END_FUNCTION;
	}

	if (ts_status == HX_TS_GET_DATA_FAIL)
		goto GET_TOUCH_FAIL;
	else
		goto END_FUNCTION;

GET_TOUCH_FAIL:
	I("%s: Now reset the Touch chip.\n", __func__);
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(false, true);
#else
	g_core_fp.fp_system_reset();
#endif
#ifdef HX_ZERO_FLASH
	if (g_core_fp.fp_0f_reload_to_active)
		g_core_fp.fp_0f_reload_to_active();
#endif
END_FUNCTION:
	if (debug_data != NULL)
		debug_data->fp_ts_dbg_func(ts, HX_FINGER_LEAVE);

}
/*end ts_work*/
EXPORT_SYMBOL(himax_ts_work);

enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer)
{
	struct himax_ts_data *ts;


	ts = container_of(timer, struct himax_ts_data, timer);
	queue_work(ts->himax_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

#if defined(HX_USB_DETECT_CALLBACK)
static void himax_cable_tp_status_handler_func(int connect_status)
{
	struct himax_ts_data *ts;

	I("Touch: cable change to %d\n", connect_status);

	ts = private_ts;

	if (ts->cable_config) {
		if (!atomic_read(&ts->suspend_mode)) {
			if (connect_status != ts->usb_connected) {
				if (connect_status) {
					ts->cable_config[1] = 0x01;
					ts->usb_connected = 0x01;
				} else {
					ts->cable_config[1] = 0x00;
					ts->usb_connected = 0x00;
				}

				himax_bus_master_write(ts->cable_config,
										sizeof(ts->cable_config), HIMAX_I2C_RETRY_TIMES);
				I("%s: Cable status change: 0x%2.2X\n", __func__, ts->cable_config[1]);
			} else
				I("%s: Cable status is the same as previous one, ignore.\n", __func__);
		} else {
			if (connect_status)
				ts->usb_connected = 0x01;
			else
				ts->usb_connected = 0x00;

			I("%s: Cable status remembered: 0x%2.2X\n", __func__, ts->usb_connected);
		}
	}
}

static struct t_cable_status_notifier himax_cable_status_handler = {
	.name = "usb_tp_connected",
	.func = himax_cable_tp_status_handler_func,
};

#endif

#ifdef HX_AUTO_UPDATE_FW
static void himax_update_register(struct work_struct *work)
{
	I("%s %s: in\n", HIMAX_LOG_TAG, __func__);

	if (i_get_FW() != 0)
		return;

	if (g_auto_update_flag == true) {
		I("Update FW Directly");
		goto UPDATE_FW;
	}

	if (himax_auto_update_check() != 0) {
		I("%s:Don't run auto update fw, so free allocated!\n", __func__);
		kfree(i_CTPM_FW);
		i_CTPM_FW = NULL;
		return;
	}

UPDATE_FW:
	if (i_update_FW() <= 0)
		I("Auto update FW fail!\n");
	else
		I("It have Updated\n");

}
#endif

#ifndef HX_ZERO_FLASH
static int hx_chk_flash_sts(uint32_t size)
{
	int rslt = 0;

	I("%s: Entering, %d\n", __func__, size);

	rslt = (!g_core_fp.fp_calculateChecksum(false, size));
	rslt |= g_core_fp.fp_flash_lastdata_check(size, NULL, 0);

	return rslt;
}
#endif

#ifdef HX_CONTAINER_SPEED_UP
static void himax_resume_work_func(struct work_struct *work)
{
	himax_chip_common_resume(private_ts);
}

#endif

static void himax_print_info(struct himax_ts_data *ts)
{
	if (ic_data->vendor_ic_name_ver == 0x00) {
		if (strcmp(HX_83112A_SERIES_PWON, private_ts->chip_name) == 0)
			ic_data->vendor_ic_name_ver = 0x01;
		else if (strcmp(HX_83102D_SERIES_PWON, private_ts->chip_name) == 0)
			ic_data->vendor_ic_name_ver = 0x02;
		else if (strcmp(HX_83102E_SERIES_PWON, private_ts->chip_name) == 0)
			ic_data->vendor_ic_name_ver = 0x03;
		else if (strcmp(HX_83121A_SERIES_PWON, private_ts->chip_name) == 0)
			ic_data->vendor_ic_name_ver = 0x04;
		else {
			ic_data->vendor_ic_name_ver = 0x00;
			input_err(true, ts->dev, "%s : have to check ic name(%s)", __func__, private_ts->chip_name);
		}
	}

	ts->print_info_cnt_open++;

	if (ts->print_info_cnt_open > 0xfff0)
		ts->print_info_cnt_open = 0;

	if (ts->touch_count == 0)
		ts->print_info_cnt_release++;

	input_info(true, ts->dev,
		"tc:%d lp:%x ta:%d noise:%d lamp:%d // v:HX%02X%02X%02X%02X // #%d %d\n",
		ts->touch_count, ts->SMWP_enable, USB_detect_flag,
		ts->noise_mode, ts->lamp_noise_mode,
		ic_data->vendor_ic_name_ver, ic_data->vendor_cid_maj_ver,
		ic_data->vendor_panel_ver, ic_data->vendor_cid_min_ver,
		ts->print_info_cnt_open, ts->print_info_cnt_release);
}

static void himax_read_info_work(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data,
			work_read_info.work);

	himax_run_rawdata_all(ts);

	schedule_work(&ts->work_print_info.work);
}

static void himax_print_info_work(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data,
			work_print_info.work);

	himax_print_info(ts);

	schedule_delayed_work(&ts->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

int hx_ic_register(void)
{
	int ret = !NO_ERR;

	I("%s:Entering!\n", __func__);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102)
	if (_hx83102_init()) {
		ret = NO_ERR;
		goto END;
	}
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83121)
	if (_hx83121_init()) {
		ret = NO_ERR;
		goto END;
	}
#endif

END:
	if (ret == NO_ERR)
		I("%s: detect IC!\n", __func__);
	else
		E("%s: There is no IC!\n", __func__);
	I("%s:END!\n", __func__);

	return ret;
}

int himax_chip_common_init(void)
{

	int ret = 0, err = 0;
	struct himax_ts_data *ts = private_ts;
	struct himax_i2c_platform_data *pdata;

#if defined(__EMBEDDED_FW__)
	g_embedded_fw.size = (size_t)_binary___Himax_firmware_bin_end -
			(size_t)_binary___Himax_firmware_bin_start;
#endif
	ts->xfer_buff = devm_kzalloc(ts->dev, 128 * sizeof(uint8_t), GFP_KERNEL);
	if (ts->xfer_buff == NULL) {
		err = -ENOMEM;
		goto err_xfer_buff_fail;
	}

	I("%s: PDATA START\n", HIMAX_LOG_TAG);
	pdata = kzalloc(sizeof(struct himax_i2c_platform_data), GFP_KERNEL);

	if (pdata == NULL) { /*Allocate Platform data space*/
		err = -ENOMEM;
		goto err_dt_platform_data_fail;
	}

	I("%s ic_data START\n", HIMAX_LOG_TAG);
	ic_data = kzalloc(sizeof(struct himax_ic_data), GFP_KERNEL);
	if (ic_data == NULL) { /*Allocate IC data space*/
		err = -ENOMEM;
		goto err_dt_ic_data_fail;
	}
	/* default 128k, different size please follow HX83121A style */
	ic_data->flash_size = FW_SIZE_128k;

	/* allocate report data */
	hx_touch_data = kzalloc(sizeof(struct himax_report_data), GFP_KERNEL);
	if (hx_touch_data == NULL) {
		err = -ENOMEM;
		goto err_alloc_touch_data_failed;
	}

	if (himax_parse_dt(ts, pdata) < 0) {
		I(" pdata is NULL for DT\n");
		err = -ENODEV;
		goto err_alloc_dt_pdata_failed;
	}

#ifdef HX_RST_PIN_FUNC
	ts->rst_gpio = pdata->gpio_reset;
#endif
	himax_gpio_power_config(pdata);

	/* Get pinctrl if target uses pinctrl */
	ts->pinctrl = devm_pinctrl_get(ts->dev);
	if (IS_ERR(ts->pinctrl)) {
		if (PTR_ERR(ts->pinctrl) == -EPROBE_DEFER) {
			err = -EPROBE_DEFER;
			goto err_pinctrl;
		}

		I("%s: Target does not use pinctrl\n", __func__);
		ts->pinctrl = NULL;
	}

	ret = himax_pinctrl_configure(ts, true);
	if (ret)
		I("%s cannot set pinctrl state\n", __func__);

	atomic_set(&ts->suspend_mode, HIMAX_STATE_POWER_ON);
	init_completion(&ts->resume_done);
	complete_all(&ts->resume_done);

	g_hx_chip_inited = 0;

	if (hx_ic_register() != NO_ERR) {
		E("%s: can't detect IC!\n", __func__);
		err = -ENODEV;
		goto error_ic_detect_failed;
	}

	if (g_core_fp.fp_chip_init != NULL) {
		g_core_fp.fp_chip_init();
	} else {
		E("%s: function point of chip_init is NULL!\n", __func__);
		err = -ENXIO;
		goto error_ic_detect_failed;
	}

	if (pdata->virtual_key)
		ts->button = pdata->virtual_key;

#ifndef HX_ZERO_FLASH
	if (hx_chk_flash_sts(ic_data->flash_size) == 1) {
		E("%s: check flash fail\n", __func__);
#ifdef HX_AUTO_UPDATE_FW
		g_auto_update_flag = true;
#endif
	} else {
		g_core_fp.fp_read_FW_ver();
	}
#endif


#ifdef HX_AUTO_UPDATE_FW
	ts->himax_update_wq = create_singlethread_workqueue("HMX_update_reuqest");
	if (!ts->himax_update_wq) {
		E(" allocate himax_update_wq failed\n");
		err = -ENOMEM;
		goto err_update_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->work_update, himax_update_register);
	//queue_delayed_work(ts->himax_update_wq, &ts->work_update, msecs_to_jiffies(2000));
#endif
#ifdef HX_ZERO_FLASH
	g_auto_update_flag = true;
	ts->himax_0f_update_wq = create_singlethread_workqueue("HMX_0f_update_reuqest");
	if (!ts->himax_0f_update_wq) {
		E(" allocate himax_0f_update_wq failed\n");
		err = -ENOMEM;
		goto err_0f_update_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->work_0f_update, g_core_fp.fp_0f_operation);
	queue_delayed_work(ts->himax_0f_update_wq, &ts->work_0f_update, msecs_to_jiffies(2000));
#endif

#ifdef HX_CONTAINER_SPEED_UP
	ts->ts_int_workqueue = create_singlethread_workqueue("himax_ts_resume_wq");
	if (!ts->ts_int_workqueue) {
		E("%s: create ts_resume workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_ts_resume_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->ts_int_work, himax_resume_work_func);
#endif

	/*Himax Power On and Load Config*/
	if (himax_loadSensorConfig(pdata)) {
		E("%s: Load Sesnsor configuration failed, unload driver.\n", __func__);
		err = -ENODEV;
		goto err_detect_failed;
	}

	ts->pdata = pdata;

	g_core_fp.fp_power_on_init();
	calculate_point_number();

	/*calculate the i2c data size*/
	calcDataSize();
	I("%s: calcDataSize complete\n", __func__);
#if IS_ENABLED(CONFIG_OF)
	ts->pdata->abs_pressure_min        = 0;
	ts->pdata->abs_pressure_max        = 200;
	ts->pdata->abs_width_min           = 0;
	ts->pdata->abs_width_max           = 200;
	pdata->cable_config[0]             = 0xF0;
	pdata->cable_config[1]             = 0x00;
#endif
	ts->suspended                      = false;
#if defined(HX_USB_DETECT_CALLBACK) || defined(HX_USB_DETECT_GLOBAL)
	ts->usb_connected = 0x00;
	ts->cable_config = pdata->cable_config;
#endif
#ifdef	HX_PROTOCOL_A
	ts->protocol_type = PROTOCOL_TYPE_A;
#else
	ts->protocol_type = PROTOCOL_TYPE_B;
#endif
	I("%s: Use Protocol Type %c\n", __func__,
	  ts->protocol_type == PROTOCOL_TYPE_A ? 'A' : 'B');

	ret = himax_dev_set(ts);
	if (ret < 0) {
		I("%s, input device alloc fail!\n", __func__);
		err = -ENOMEM;
		goto err_input_register_device_failed;
	}

	ret = himax_input_register(ts, ts->input_dev, INPUT_PROP_DIRECT);
	if (ret) {
		E("%s: Unable to register input device\n", __func__);
		err = -ENODEV;
		goto err_input_register_device_failed;
	}

	if (ts->pdata->support_dex) {
		if (himax_input_register(ts, ts->input_dev_pad, INPUT_PROP_POINTER))
			E("%s: Unable to register dex input device\n", __func__);
	}

	mutex_init(&ts->irq_lock);
	mutex_init(&ts->device_lock);
	ts->initialized = true;

#ifdef HX_SMART_WAKEUP
	ts->SMWP_enable = 0;
	ts->gesture_cust_en[0] = 0;
	ts->ts_SMWP_wake_lock = wakeup_source_register(NULL, "TSP");
#endif
#ifdef HX_HIGH_SENSE
	ts->HSEN_enable = 0;
#endif

	/*touch data init*/
	ts->glove_enabled = 0;
	ts->touch_count = 0;
	ts->print_info_cnt_open = 0;
	ts->print_info_cnt_release = 0;

	err = himax_report_data_init();
	if (err)
		goto err_report_data_init_failed;

	if (himax_common_proc_init()) {
		E(" %s: himax_common proc_init failed!\n", __func__);
		goto err_creat_proc_file_failed;
	}

	himax_ts_register_interrupt();

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
	if (g_auto_update_flag)
		himax_int_enable(0);
#endif
#ifdef HX_AUTO_UPDATE_FW
	himax_update_register(&ts->work_update.work);
#endif

#ifdef SEC_FACTORY_MODE
	err = sec_touch_sysfs(ts);
	if (err)
		goto err_sec_sysfs;
#endif

#if defined(HX_USB_DETECT_CALLBACK)
	if (ts->cable_config)
		cable_detect_register_notifier(&himax_cable_status_handler);
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	if (himax_debug_init())
		E(" %s: debug initial failed!\n", __func__);
#endif

	INIT_DELAYED_WORK(&ts->work_print_info, himax_print_info_work);
	INIT_DELAYED_WORK(&ts->work_read_info, himax_read_info_work);
	schedule_work(&ts->work_read_info.work);

	g_hx_chip_inited = true;
	return 0;

#ifdef SEC_FACTORY_MODE
//	sec_touch_sysfs_remove(ts);
//	sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
err_sec_sysfs:
#endif
	himax_ts_unregister_interrupt();
err_creat_proc_file_failed:
	himax_report_data_deinit();
err_report_data_init_failed:
#ifdef HX_SMART_WAKEUP
	wakeup_source_unregister(ts->ts_SMWP_wake_lock);
#endif
	mutex_destroy(&ts->device_lock);
	mutex_destroy(&ts->irq_lock);
err_input_register_device_failed:
err_detect_failed:

#ifdef HX_CONTAINER_SPEED_UP
	cancel_delayed_work_sync(&ts->ts_int_work);
	destroy_workqueue(ts->ts_int_workqueue);
err_create_ts_resume_wq_failed:
#endif

#ifdef HX_ZERO_FLASH
	cancel_delayed_work_sync(&ts->work_0f_update);
	destroy_workqueue(ts->himax_0f_update_wq);
err_0f_update_wq_failed:
#endif

#ifdef HX_AUTO_UPDATE_FW
	if (g_auto_update_flag) {
		cancel_delayed_work_sync(&ts->work_update);
		destroy_workqueue(ts->himax_update_wq);
	}
err_update_wq_failed:
#endif
error_ic_detect_failed:
err_pinctrl:
	himax_gpio_power_deconfig(pdata);
err_alloc_dt_pdata_failed:
	kfree(hx_touch_data);
	hx_touch_data = NULL;
err_alloc_touch_data_failed:
	kfree(ic_data);
	ic_data = NULL;
err_dt_ic_data_fail:
	kfree(pdata);
	pdata = NULL;
err_dt_platform_data_fail:
	devm_kfree(ts->dev, ts->xfer_buff);
	ts->xfer_buff = NULL;
err_xfer_buff_fail:
	probe_fail_flag = 1;
	E(" %s: probe failed!\n", __func__);
	return err;
}

void himax_chip_common_deinit(void)
{
	struct himax_ts_data *ts = private_ts;

	himax_ts_unregister_interrupt();

	cancel_delayed_work_sync(&ts->work_read_info);
	cancel_delayed_work_sync(&ts->work_print_info);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_debug_remove();
#endif

	himax_common_proc_deinit();
	himax_report_data_deinit();

#ifdef SEC_FACTORY_MODE
	sec_touch_sysfs_remove(ts);
	sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
#endif

#ifdef HX_SMART_WAKEUP
	wakeup_source_unregister(ts->ts_SMWP_wake_lock);
#endif
#ifdef HX_CONTAINER_SPEED_UP
	cancel_delayed_work_sync(&ts->ts_int_work);
	destroy_workqueue(ts->ts_int_workqueue);
#endif
#ifdef HX_ZERO_FLASH
	cancel_delayed_work_sync(&ts->work_0f_update);
	destroy_workqueue(ts->himax_0f_update_wq);
#endif
#ifdef HX_AUTO_UPDATE_FW
	cancel_delayed_work_sync(&ts->work_update);
	destroy_workqueue(ts->himax_update_wq);
#endif
	himax_gpio_power_deconfig(ts->pdata);
	if (himax_mcu_cmd_struct_free)
		himax_mcu_cmd_struct_free();

	mutex_destroy(&ts->device_lock);
	mutex_destroy(&ts->irq_lock);

	kfree(hx_touch_data);
	hx_touch_data = NULL;
	kfree(ic_data);
	ic_data = NULL;
	kfree(ts->pdata->virtual_key);
	ts->pdata->virtual_key = NULL;
	devm_kfree(ts->dev, ts->xfer_buff);
	ts->xfer_buff = NULL;
	kfree(ts->pdata);
	ts->pdata = NULL;
	kfree(ts);
	ts = NULL;
	probe_fail_flag = 0;

	KI("%s: Common section deinited!\n", __func__);
}

int himax_pinctrl_configure(struct himax_ts_data *ts, bool active)
{
	struct pinctrl_state *set_state = NULL;

	int retval;

	if (!ts->pinctrl) {
		E("%s: pinctrl is null\n", __func__);
		return -ENODEV;
	}

	I("%s %s\n", __func__, active ? "ACTIVE" : "SUSPEND");

	set_state = pinctrl_lookup_state(ts->pinctrl,
					 active ? "on_state" : "off_state");
	if (IS_ERR(set_state)) {
		I("%s cannot get active state\n", __func__);
		return -EINVAL;
	}

	retval = pinctrl_select_state(ts->pinctrl, set_state);
	if (retval) {
		I("%s cannot set pinctrl %s state, retval = %d\n",
			__func__, active ? "active" : "suspend", retval);
		return -EINVAL;
	}

	return 0;
}

int himax_ctrl_lcd_reset_regulator(struct himax_ts_data *ts, bool on)
{
	static struct regulator *panel_reset;

	static bool enabled;
	int ret = 0;

	if (!ts->pdata->panel_reset)
		return 0;

	input_info(true, ts->dev, "%s: called! %s, enabled(%d)\n",
					__func__, on ? "on" : "off", enabled);

	if (enabled == on)
		return ret;

	if (IS_ERR_OR_NULL(panel_reset)) {
		panel_reset = devm_regulator_get(ts->dev, ts->pdata->panel_reset);
		if (IS_ERR_OR_NULL(panel_reset)) {
			input_err(true, ts->dev, "%s: Failed to get panel_reset(%s) regulator.\n",
					__func__, ts->pdata->panel_reset);
			ret = PTR_ERR(panel_reset);
			goto error;
		}
	}

	input_info(true, ts->dev, "%s: %s: [BEFORE] reset:%d\n",
			__func__, on ? "on" : "off",
			regulator_is_enabled(panel_reset));

	if (on) {
		ret = regulator_enable(panel_reset);
		if (ret) {
			input_err(true, ts->dev, "%s: Failed to enable panel_reset: %d\n", __func__, ret);
			goto out;
		}
	} else {
		regulator_disable(panel_reset);
	}

	enabled = on;

out:
	input_info(true, ts->dev, "%s: %s: [AFTER] reset:%d\n",
			__func__, on ? "on" : "off",
			regulator_is_enabled(panel_reset));

error:
	return ret;

}

int himax_ctrl_lcd_regulators(struct himax_ts_data *ts, bool on)
{
	static struct regulator *panel_bl_en;
	static struct regulator *panel_vddi;
	static bool enabled;
	int ret = 0;

	input_info(true, ts->dev, "%s: called! %s, enabled(%d)\n",
					__func__, on ? "on" : "off", enabled);

	if (enabled == on)
		return ret;

	if (ts->pdata->panel_bl_en) {
		if (IS_ERR_OR_NULL(panel_bl_en)) {
			panel_bl_en = devm_regulator_get(ts->dev, ts->pdata->panel_bl_en);
			if (IS_ERR_OR_NULL(panel_bl_en)) {
				input_err(true, ts->dev, "%s: Failed to get panel_bl_en(%s) regulator.\n",
						__func__, ts->pdata->panel_bl_en);
				ret = PTR_ERR(panel_bl_en);
				goto error;
			}
		}
	}

	if (ts->pdata->panel_vddi) {
		if (IS_ERR_OR_NULL(panel_vddi)) {
			panel_vddi = devm_regulator_get(ts->dev, ts->pdata->panel_vddi);
			if (IS_ERR_OR_NULL(panel_vddi)) {
				input_err(true, ts->dev, "%s: Failed to get panel_vddi(%s) regulator.\n",
						__func__, ts->pdata->panel_vddi);
				ret = PTR_ERR(panel_vddi);
				goto error;
			}
		}
	}

	if (on) {
		if (!IS_ERR_OR_NULL(panel_vddi)) {
			ret = regulator_enable(panel_vddi);
			if (ret) {
				input_err(true, ts->dev, "%s: Failed to enable panel_vddi: %d\n", __func__, ret);
				goto out;
			}
			input_info(true, ts->dev, "%s: enable panel_vddi:%d\n", __func__, regulator_is_enabled(panel_vddi));
		}
		if (!IS_ERR_OR_NULL(panel_bl_en)) {
			ret = regulator_enable(panel_bl_en);
			if (ret) {
				input_err(true, ts->dev, "%s: Failed to enable panel_bl_en: %d\n", __func__, ret);
				goto out;
			}
			input_info(true, ts->dev, "%s: enable panel_bl_en:%d\n", __func__, regulator_is_enabled(panel_bl_en));
		}
	} else {
		if (!IS_ERR_OR_NULL(panel_vddi))
			regulator_disable(panel_vddi);
		if (!IS_ERR_OR_NULL(panel_bl_en))
			regulator_disable(panel_bl_en);
	}

	enabled = on;

out:
	input_info(true, ts->dev, "%s: %s done", __func__, on ? "on" : "off");

error:
	return ret;
}

int himax_chip_common_suspend(struct himax_ts_data *ts)
{
#if SEC_LPWG_DUMP
	u8 lpwg_dump[5] = {0x3, 0x0, 0x0, 0x0, 0x0};
#endif

	mutex_lock(&ts->device_lock);

	input_info(true, ts->dev, "%s %s: START lp:%d cover:%d\n",
			HIMAX_LOG_TAG, __func__, ts->SMWP_enable, ts->cover_closed);

	if (ts->suspended) {
		I("%s: Already suspended. Skipped.\n", __func__);
		goto END;
	} else {
		ts->suspended = true;
		I("%s: enter\n", __func__);
	}

	if (debug_data != NULL && debug_data->flash_dump_going == true) {
		I("[himax] %s: Flash dump is going, reject suspend\n", __func__);
		goto END;
	}

	if (ts->prox_power_off == 1 && ts->aot_enabled) {
		ts->SMWP_enable = 0;
		input_info(true, ts->dev, "%s: prox_power_off & disable aot\n", __func__);
	}

#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) || defined(HX_USB_DETECT_GLOBAL)
#ifndef HX_RESUME_SEND_CMD
	g_core_fp.fp_resend_cmd_func(ts->suspended);
#endif
#endif
#ifdef HX_SMART_WAKEUP

	if (ts->SMWP_enable) {

#ifdef HX_CODE_OVERLAY
		if (ts->in_self_test == 0)
			g_core_fp.fp_0f_overlay(2, 0);
#endif

		atomic_set(&ts->suspend_mode, HIMAX_STATE_LPM);

		himax_ctrl_lcd_regulators(ts, true);
		himax_ctrl_lcd_reset_regulator(ts, true);

		ts->pre_finger_mask = 0;
		FAKE_POWER_KEY_SEND = false;

#ifdef HX_RST_PIN_FUNC
		if (!gpio_get_value(ts->pdata->gpio_reset)) {
			gpio_set_value(ts->pdata->gpio_reset, 1);
			I("%s make TP_RESET high\n", __func__);
		}
#endif
		I("[himax] %s: SMART_WAKEUP enable, reject suspend\n", __func__);
		goto END;
	}

#endif
	himax_int_enable(0);
	/*if (g_core_fp.fp_suspend_ic_action != NULL)*/
		/*g_core_fp.fp_suspend_ic_action();*/

	if (!ts->use_irq) {
		int32_t cancel_state;

		cancel_state = cancel_work_sync(&ts->work);
		if (cancel_state)
			himax_int_enable(1);
	}

	/*ts->first_pressed = 0;*/
	atomic_set(&ts->suspend_mode, HIMAX_STATE_POWER_OFF);
	ts->pre_finger_mask = 0;

#ifdef HX_RST_PIN_FUNC
	if (gpio_get_value(ts->pdata->gpio_reset)) {
		gpio_set_value(ts->pdata->gpio_reset, 0);
		I("%s make TP_RESET low\n", __func__);
	}
#endif
	himax_pinctrl_configure(ts, false);
#if SEC_LPWG_DUMP
	if (atomic_read(&ts->suspend_mode) == HIMAX_STATE_LPM) {
		himax_lpwg_dump_buf_write(lpwg_dump);
	}
#endif
END:
	himax_report_all_leave_event(ts);

	if (ts->in_self_test == 1)
		ts->suspend_resume_done = 1;

	cancel_delayed_work(&ts->work_print_info);
	himax_print_info(ts);

	input_info(true, ts->dev, "%s: END\n", __func__);
	mutex_unlock(&ts->device_lock);

	return 0;
}

#if SEC_LPWG_DUMP
void himax_lpwg_dump_buf_init(void)
{
	private_ts->lpwg_dump_buf = devm_kzalloc(private_ts->dev, LPWG_DUMP_TOTAL_SIZE, GFP_KERNEL);
	if (private_ts->lpwg_dump_buf == NULL) {
		input_err(true, private_ts->dev, "kzalloc for lpwg_dump_buf failed!\n");
		return;
	}
	private_ts->lpwg_dump_buf_idx = 0;
	input_info(true, private_ts->dev, "%s : done\n", __func__);
}

int himax_lpwg_dump_buf_write(u8 *buf)
{
	int i = 0;

	if (private_ts->lpwg_dump_buf == NULL) {
		input_err(true, private_ts->dev, "%s: kzalloc for lpwg_dump_buf failed!\n", __func__);
		return -EFAULT;
	}
//	input_info(true, private_ts->dev, "%s : idx(%d) data (0x%X,0x%X,0x%X,0x%X,0x%X)\n",
//			__func__, private_ts->lpwg_dump_buf_idx, buf[0], buf[1], buf[2], buf[3], buf[4]);

	for (i = 0 ; i < LPWG_DUMP_PACKET_SIZE ; i++) {
		private_ts->lpwg_dump_buf[private_ts->lpwg_dump_buf_idx++] = buf[i];
	}
	if (private_ts->lpwg_dump_buf_idx >= LPWG_DUMP_TOTAL_SIZE) {
		input_info(true, private_ts->dev, "%s : write end of data buf(%d)!\n",
					__func__, private_ts->lpwg_dump_buf_idx);
		private_ts->lpwg_dump_buf_idx = 0;
	}
	return 0;
}

int himax_lpwg_dump_buf_read(u8 *buf)
{
	u8 read_buf[30] = { 0 };
	int read_packet_cnt;
	int start_idx;
	int i;

	if (private_ts->lpwg_dump_buf == NULL) {
		input_err(true, private_ts->dev, "%s : kzalloc for lpwg_dump_buf failed!\n", __func__);
		return 0;
	}

	if (private_ts->lpwg_dump_buf[private_ts->lpwg_dump_buf_idx] == 0
		&& private_ts->lpwg_dump_buf[private_ts->lpwg_dump_buf_idx + 1] == 0
		&& private_ts->lpwg_dump_buf[private_ts->lpwg_dump_buf_idx + 2] == 0) {
		start_idx = 0;
		read_packet_cnt = private_ts->lpwg_dump_buf_idx / LPWG_DUMP_PACKET_SIZE;
	} else {
		start_idx = private_ts->lpwg_dump_buf_idx;
		read_packet_cnt = LPWG_DUMP_TOTAL_SIZE / LPWG_DUMP_PACKET_SIZE;
	}

	input_info(true, private_ts->dev, "%s : lpwg_dump_buf_idx(%d), start_idx (%d), read_packet_cnt(%d)\n",
				__func__, private_ts->lpwg_dump_buf_idx, start_idx, read_packet_cnt);

	for (i = 0 ; i < read_packet_cnt ; i++) {
		memset(read_buf, 0x00, 30);
		snprintf(read_buf, 30, "%03d : %02X%02X%02X%02X%02X\n",
					i, private_ts->lpwg_dump_buf[start_idx + 0], private_ts->lpwg_dump_buf[start_idx + 1],
					private_ts->lpwg_dump_buf[start_idx + 2], private_ts->lpwg_dump_buf[start_idx + 3],
					private_ts->lpwg_dump_buf[start_idx + 4]);

		//input_info(true, private_ts->dev, "%s : %s\n", __func__, read_buf);
		strlcat(buf, read_buf, PAGE_SIZE);

		if (start_idx + LPWG_DUMP_PACKET_SIZE >= LPWG_DUMP_TOTAL_SIZE) {
			start_idx = 0;
		} else {
			start_idx += 5;
		}
	}

	return 0;
}

void himax_parse_gesture_history(void)
{
	uint8_t *total_buffer;
	uint32_t daddr;
	uint8_t tmp_addr[4] = {0};
	int pack_num = 0;
	int i = 0;
	int read_times = 0;

//	int event_type = -1; /* 0: coordinate, 1:gesture*/
	/*=====info of coordinate======*/
//	int frame_count = 0;
//	int tid = 0;
//	int touch_status = 0; /*0: press, 1: release*/
//	int touch_x = 0;
//	int touch_y = 0;
	/*=====info of gesture ========*/
//	int gesture_id = 0; /* 0: double tap, 1: swipe up*/
	uint8_t size_buf[4] = {0};
	int total_sz = 0;

	I("%s start\n", __func__);

	g_core_fp.fp_register_read(pfw_op->addr_gesture_history, 4, size_buf, 0);
	pack_num = size_buf[0];
	pack_num = pack_num >= 99 ? 99 : pack_num;
	total_sz = pack_num * 5;

	I("%s: package number=%d, total size=%d\n", __func__, pack_num, total_sz);

	if (pack_num > 0) {
		total_buffer = kcalloc(total_sz + 1, sizeof(uint8_t), GFP_KERNEL);
		read_times = total_sz / FLASH_RW_MAX_LEN + (total_sz % FLASH_RW_MAX_LEN > 0 ? 1 : 0);

		for (i = 0; i < read_times; i++) {
			daddr = (pfw_op->addr_gesture_history[3] << 24
				| pfw_op->addr_gesture_history[2] << 16
				| pfw_op->addr_gesture_history[1] << 8
				| pfw_op->addr_gesture_history[0]) + i * FLASH_RW_MAX_LEN;

			tmp_addr[3] = (uint8_t)((daddr >> 24) & 0x00FF);
			tmp_addr[2] = (uint8_t)((daddr >> 16) & 0x00FF);
			tmp_addr[1] = (uint8_t)((daddr >> 8) & 0x00FF);
			tmp_addr[0] = (uint8_t)((daddr) & 0x00FF);
			if (i == (read_times - 1))
				g_core_fp.fp_register_read(tmp_addr, total_sz % FLASH_RW_MAX_LEN, &total_buffer[i * FLASH_RW_MAX_LEN], 0);
			else
				g_core_fp.fp_register_read(tmp_addr, FLASH_RW_MAX_LEN, &total_buffer[i * FLASH_RW_MAX_LEN], 0);
		}

		/* original data */
		for (i = 0; i < pack_num; i++) {
			//I("%03d : %02X%02X%02X%02X%02X\n", i * 5,
			// 	total_buffer[i * 5  + 1], total_buffer[i * 5  + 2], total_buffer[i * 5  + 3],
			// 	total_buffer[i * 5  + 4], total_buffer[i * 5  + 5]);
			himax_lpwg_dump_buf_write(&total_buffer[i * 5  + 1]);
		}
		I("%s end\n", __func__);
		kfree(total_buffer);
	}
}
#endif

int himax_chip_common_resume(struct himax_ts_data *ts)
{
#if defined(HX_ZERO_FLASH) && defined(HX_RESUME_SET_FW)
	int result = 0;
#endif
#if SEC_LPWG_DUMP
	u8 lpwg_dump[5] = {0x7, 0x0, 0x0, 0x0, 0x0};
	int previous_suspend_mode = 0;
#endif

	mutex_lock(&ts->device_lock);

	input_info(true, ts->dev, "%s %s: START lp:%d cover:%d\n",
			HIMAX_LOG_TAG, __func__, ts->SMWP_enable, ts->cover_closed);

	if (ts->suspended == false) {
		I("%s: It had entered resume, skip this step\n", __func__);
		goto END;
	} else {
		ts->suspended = false;
	}

#ifdef HX_ESD_RECOVERY
	/* continuous N times record, not total N times. */
	g_zero_event_count = 0;
#endif

#ifdef HX_RST_PIN_FUNC
	if (!gpio_get_value(ts->pdata->gpio_reset)) {
		gpio_set_value(ts->pdata->gpio_reset, 1);
		I("%s make TP_RESET high\n", __func__);
		usleep_range(5000, 5000);
	}
#endif

	if (ts->SMWP_enable)
		himax_ctrl_lcd_regulators(ts, false);
	else
		himax_pinctrl_configure(ts, true);
#if SEC_LPWG_DUMP
	previous_suspend_mode = atomic_read(&ts->suspend_mode);
#endif
	atomic_set(&ts->suspend_mode, HIMAX_STATE_POWER_ON);

#if defined(HX_RST_PIN_FUNC) && defined(HX_RESUME_HW_RESET)
	if (g_core_fp.fp_ic_reset != NULL)
		g_core_fp.fp_ic_reset(false, false);
#endif

#if defined(HX_ZERO_FLASH) && defined(HX_RESUME_SET_FW)
#ifdef HX_SMART_WAKEUP
	if (!ts->SMWP_enable) {
#endif
		I("It will update fw after resume in zero flash mode!\n");
		if (g_core_fp.fp_0f_operation_dirly != NULL) {
			result = g_core_fp.fp_0f_operation_dirly();
			if (result) {
				E("Something is wrong! Skip Update with zero flash!\n");
				goto ESCAPE_0F_UPDATE;
			}
		}
		if (g_core_fp.fp_reload_disable != NULL)
			g_core_fp.fp_reload_disable(0);
		if (g_core_fp.fp_sense_on != NULL)
			g_core_fp.fp_sense_on(0x00);
#ifdef HX_SMART_WAKEUP
	}
#endif
#endif

	if (ts->aot_enabled)
		ts->SMWP_enable = 1;
	ts->prox_power_off = 0;

#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) || defined(HX_USB_DETECT_GLOBAL)
	if (g_core_fp.fp_resend_cmd_func != NULL)
		g_core_fp.fp_resend_cmd_func(ts->suspended);

#ifdef HX_CODE_OVERLAY
	if (ts->SMWP_enable && ts->in_self_test == 0)
		g_core_fp.fp_0f_overlay(3, 0);
#endif
#endif
#if defined(SEC_LPWG_DUMP)
	if (previous_suspend_mode == HIMAX_STATE_LPM) {
		himax_parse_gesture_history();
		himax_lpwg_dump_buf_write(lpwg_dump);
	}
#endif
	himax_report_all_leave_event(ts);

	if (g_core_fp.fp_sense_on != NULL)
		g_core_fp.fp_resume_ic_action();
	himax_int_enable(1);
#if defined(HX_ZERO_FLASH) && defined(HX_RESUME_SET_FW)
ESCAPE_0F_UPDATE:
#endif
END:
	if (ts->in_self_test == 1)
		ts->suspend_resume_done = 1;

	cancel_delayed_work(&ts->work_print_info);
	ts->print_info_cnt_open = 0;
	ts->print_info_cnt_release = 0;
	schedule_work(&ts->work_print_info.work);

	I("%s: END\n", __func__);
	mutex_unlock(&ts->device_lock);
	return 0;
}

