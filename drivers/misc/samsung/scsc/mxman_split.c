/****************************************************************************
 *
 * Copyright (c) 2014 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/notifier.h>
#ifdef CONFIG_ARCH_EXYNOS
#include <linux/soc/samsung/exynos-soc.h>
#endif
#include "scsc_mx_impl.h"
#include "miframman.h"
#include "mifmboxman.h"
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
#include "mifpmuman.h"
#include "mxman_res.h"
#endif
#include "mxman.h"
#include "srvman.h"
#include "mxmgmt_transport.h"
#include "gdb_transport.h"
#include "mxconf.h"
#include "fwimage.h"
#include "whdr.h"
#include "bhdr.h"
#include "fwhdr_if.h"
#include "mxlog.h"
#include "mxlogger.h"
#include "fw_panic_record.h"
#include "panicmon.h"
#include "mxproc.h"
#include "mxlog_transport.h"
#include "mxsyserr.h"
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
#include "mxman_sysevent.h"
#endif
#ifdef CONFIG_SCSC_SMAPPER
#include "mifsmapper.h"
#endif
#ifdef CONFIG_SCSC_QOS
#include "mifqos.h"
#endif
#include "mxfwconfig.h"
#include <scsc/kic/slsi_kic_lib.h>
#include <scsc/scsc_release.h>
#include <scsc/scsc_mx.h>
#include <linux/fs.h>
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif

#include <scsc/scsc_logring.h>
#ifdef CONFIG_SCSC_WLBTD
#include "scsc_wlbtd.h"
#define SCSC_SCRIPT_MOREDUMP "moredump"
#define SCSC_SCRIPT_LOGGER_DUMP "mx_logger_dump.sh"
static struct work_struct wlbtd_work;
#else
#define MEMDUMP_FILE_FOR_RECOVERY 2
#endif

#include "scsc_lerna.h"
#ifdef CONFIG_SCSC_LAST_PANIC_IN_DRAM
#include "scsc_log_in_dram.h"
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/debug-snapshot.h>
#else
#include <linux/debug-snapshot.h>
#endif
#endif

#include <asm/page.h>
#include <scsc/api/bt_audio.h>

#if 0
#include <soc/samsung/memlogger.h>
#endif

#define STRING_BUFFER_MAX_LENGTH 512
#define NUMBER_OF_STRING_ARGS 5
#define MX_DRAM_SIZE (4 * 1024 * 1024)
#define MX_DRAM_SIZE_SECTION_1 (8 * 1024 * 1024)

#if defined(CONFIG_SOC_EXYNOS3830)
#define MX_DRAM_SIZE_SECTION_2 (4 * 1024 * 1024)
#else
#define MX_DRAM_SIZE_SECTION_2 (8 * 1024 * 1024)
#endif

#define MX_DRAM_OFFSET_SECTION_2 MX_DRAM_SIZE_SECTION_1

#define MX_FW_RUNTIME_LENGTH (1024 * 1024)
#define WAIT_FOR_FW_TO_START_DELAY_MS 1000
#define MBOX2_MAGIC_NUMBER 0xbcdeedcb
#define MBOX_INDEX_0 0
#define MBOX_INDEX_1 1
#define MBOX_INDEX_2 2
#define MBOX_INDEX_3 3
#ifdef CONFIG_SOC_EXYNOS7570
#define MBOX_INDEX_4 4
#define MBOX_INDEX_5 5
#define MBOX_INDEX_6 6
#define MBOX_INDEX_7 7
#endif

#define SCSC_PANIC_ORIGIN_FW (0x0 << 15)
#define SCSC_PANIC_ORIGIN_HOST (0x1 << 15)

#define SCSC_PANIC_TECH_WLAN (0x0 << 13)
#define SCSC_PANIC_TECH_CORE (0x1 << 13)
#define SCSC_PANIC_TECH_BT (0x2 << 13)
#define SCSC_PANIC_TECH_UNSP (0x3 << 13)

#define SCSC_PANIC_CODE_MASK 0xFFFF
#define SCSC_PANIC_ORIGIN_MASK 0x8000
#define SCSC_PANIC_TECH_MASK 0x6000
#define SCSC_PANIC_SUBCODE_MASK_LEGACY 0x0FFF
#define SCSC_PANIC_SUBCODE_MASK 0x7FFF

#define SCSC_R4_V2_MINOR_52 52
#define SCSC_R4_V2_MINOR_53 53
#define SCSC_R4_V2_MINOR_54 54

#define MM_HALT_RSP_TIMEOUT_MS 100

/* If limits below are exceeded, a service level reset will be raised to level 7 */
#define SYSERR_LEVEL7_HISTORY_SIZE (4)
/* Minimum time between system error service resets (ms) */
#define SYSERR_LEVEL7_MIN_INTERVAL (300000)
/* No more then SYSERR_RESET_HISTORY_SIZE system error service resets in this period (ms)*/
#define SYSERR_LEVEL7_MONITOR_PERIOD (3600000)

static char panic_record_dump[PANIC_RECORD_DUMP_BUFFER_SZ];
static BLOCKING_NOTIFIER_HEAD(firmware_chain);

/**
 * This mxman reference is initialized/nullified via mxman_init/deinit
 * called by scsc_mx_create/destroy on module probe/remove.
 */
static struct mxman *active_mxman;

/**
 * This will be returned as fw version ONLY if Maxwell
 * was never found or was unloaded.
 */
static char saved_fw_build_id[FW_BUILD_ID_SZ] = "Maxwell WLBT unavailable";
#define FW_BUILD_ID_UNKNOWN "unknown"
#if 0
static bool allow_unidentified_firmware;
module_param(allow_unidentified_firmware, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(allow_unidentified_firmware, "Allow unidentified firmware");

static bool skip_header;
module_param(skip_header, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(skip_header, "Skip header, assuming unidentified firmware");
#endif

static ulong mm_completion_timeout_ms = 1000;
module_param(mm_completion_timeout_ms, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mm_completion_timeout_ms, "Timeout wait_for_mm_msg_start_ind (ms) - default 1000. 0 = infinite");

static bool skip_mbox0_check;
module_param(skip_mbox0_check, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(skip_mbox0_check, "Allow skipping firmware mbox0 signature check");

#if 0
static uint firmware_startup_flags;
module_param(firmware_startup_flags, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_startup_flags, "0 = Proceed as normal (default); Bit 0 = 1 - spin at start of CRT0; Other bits reserved = 0");
#endif

static uint trigger_moredump_level = MX_SYSERR_LEVEL_8;
module_param(trigger_moredump_level, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(trigger_moredump_level, "System error level that triggers moredump - may be 7 or 8 only");

#ifdef CONFIG_SCSC_CHV_SUPPORT
/* First arg controls chv function */
int chv_run;
module_param(chv_run, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(chv_run,
		 "Run chv f/w: 0 = feature disabled, 1 = for continuous checking, 2 = 1 shot, anything else, undefined");

/* Optional array of args for firmware to interpret when chv_run = 1 */
static unsigned int chv_argv[32];
static int chv_argc;

module_param_array(chv_argv, uint, &chv_argc, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(chv_argv, "Array of up to 32 x u32 args for the CHV firmware when chv_run = 1");
#endif

static bool disable_auto_coredump;
module_param(disable_auto_coredump, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_auto_coredump, "Disable driver automatic coredump");

static bool disable_error_handling;
module_param(disable_error_handling, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_error_handling, "Disable error handling");

#define DISABLE_RECOVERY_HANDLING_SCANDUMP 3 /* Halt kernel and scandump on FW failure */

#if defined(SCSC_SEP_VERSION) && (SCSC_SEP_VERSION >= 10)
static int disable_recovery_handling = 2; /* MEMDUMP_FILE_FOR_RECOVERY : for /sys/wifi/memdump */
#else
/* AOSP */
static int disable_recovery_handling = 1; /* Recovery disabled, enable in init.rc, not here. */
#endif

module_param(disable_recovery_handling, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_recovery_handling, "Disable recovery handling");
static bool disable_recovery_from_memdump_file = true;
static int memdump = -1;
static bool disable_recovery_until_reboot;

#if defined(CONFIG_WLBT_DCXO_TUNE)
static unsigned int wlbt_dcxo_caldata = 0;
static int wlbt_temperature_value = 0;

enum dcxo_config_state {
	DCXO_CONFIG_NONE = 0,
	DCXO_CONFIG_DEFAULT,
	DCXO_CONFIG_SYSFS,
};
static enum dcxo_config_state set_dcxo_state = DCXO_CONFIG_NONE;
#endif

static uint scandump_trigger_fw_panic = 0;
module_param(scandump_trigger_fw_panic, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scandump_trigger_fw_panic, "Specify fw panic ID");

static uint panic_record_delay = 1;
module_param(panic_record_delay, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(panic_record_delay, "Delay in ms before accessing the panic record");

static bool disable_logger = true;
module_param(disable_logger, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_logger, "Disable launch of user space logger");

static uint syserr_level7_min_interval = SYSERR_LEVEL7_MIN_INTERVAL;
module_param(syserr_level7_min_interval, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(syserr_level7_min_interval, "Minimum time between system error level 7 resets (ms)");

static uint syserr_level7_monitor_period = SYSERR_LEVEL7_MONITOR_PERIOD;
module_param(syserr_level7_monitor_period, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(syserr_level7_monitor_period, "No more then 4 system error level 7 resets in this period (ms)");

static bool kernel_crash_on_service_fail;
module_param(kernel_crash_on_service_fail, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(kernel_crash_on_service_fail, "Halt kernel and get ready for scandump on service start fail");

/*
 * shared between this module and mgt.c as this is the kobject referring to
 * /sys/wifi directory. Core driver is called 1st we create the directory
 * here and share the kobject, so in mgt.c wifi driver can create
 * /sys/wif/mac_addr using sysfs_create_file api using the kobject
 *
 * If both modules tried to create the dir we were getting kernel panic
 * failure due to kobject associated with dir already existed
 */
static struct kobject *wifi_kobj_ref;
static int refcount;
static ssize_t sysfs_show_memdump(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t sysfs_store_memdump(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static struct kobj_attribute memdump_attr = __ATTR(memdump, 0660, sysfs_show_memdump, sysfs_store_memdump);
#if defined(CONFIG_WLBT_DCXO_TUNE)
static ssize_t sysfs_show_wlbt_dcxo_caldata(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t sysfs_store_wlbt_dcxo_caldata(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static struct kobj_attribute dcxocal_attr = __ATTR(wlbt_dcxo_caldata, 0660, sysfs_show_wlbt_dcxo_caldata, sysfs_store_wlbt_dcxo_caldata);
#endif
/* Time stamps of last level7 resets in jiffies */
static unsigned long syserr_level7_history[SYSERR_LEVEL7_HISTORY_SIZE] = { 0 };
static int syserr_level7_history_index;

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
static int mxman_logring_register_observer(struct scsc_logring_mx_cb *mx_cb, char *name)
{
	return mxlogger_register_global_observer(name);
}

static int mxman_logring_unregister_observer(struct scsc_logring_mx_cb *mx_cb, char *name)
{
	return mxlogger_unregister_global_observer(name);
}

/* callbacks to mxman */
struct scsc_logring_mx_cb mx_logring = {
	.scsc_logring_register_observer = mxman_logring_register_observer,
	.scsc_logring_unregister_observer = mxman_logring_unregister_observer,
};

#endif
int mxman_stop(struct mxman *mxman, enum scsc_subsystem sub);

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
static int mxman_minimoredump_collect(struct scsc_log_collector_client *collect_client, size_t size)
{
	int ret = 0;
	struct mxman *mxman = (struct mxman *)collect_client->prv;
	struct fwhdr_if *fw_if = NULL;

	if (!mxman || !mxman->start_dram)
		return ret;

	fw_if = mxman->fw_wlan;

	SCSC_TAG_INFO(MXMAN, "Collecting Minimoredump runtime_length %d fw_image_size %d\n",
		      fw_if->get_fw_rt_len(fw_if), mxman->fw_image_size);
	/* collect RAM sections of FW */
	ret = scsc_log_collector_write(mxman->start_dram + mxman->fw_image_size,
				       fw_if->get_fw_rt_len(fw_if) - mxman->fw_image_size, 1);

	return ret;
}

struct scsc_log_collector_client mini_moredump_client = {
	.name = "minimoredump",
	.type = SCSC_LOG_MINIMOREDUMP,
	.collect_init = NULL,
	.collect = mxman_minimoredump_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static void mxman_get_fw_version_cb(struct scsc_log_collector_mx_cb *mx_cb, char *version, size_t ver_sz)
{
	mxman_get_fw_version(version, ver_sz);
}

static void mxman_get_drv_version_cb(struct scsc_log_collector_mx_cb *mx_cb, char *version, size_t ver_sz)
{
	mxman_get_driver_version(version, ver_sz);
}

static void call_wlbtd_sable_cb(struct scsc_log_collector_mx_cb *mx_cb, u8 trigger_code, u16 reason_code)
{
	call_wlbtd_sable(trigger_code, reason_code);
}

/* Register callbacks from scsc_collect to mx */
struct scsc_log_collector_mx_cb mx_cb = {
	.get_fw_version = mxman_get_fw_version_cb,
	.get_drv_version = mxman_get_drv_version_cb,
	.call_wlbtd_sable = call_wlbtd_sable_cb,
};

#endif
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
void mxman_scan_dump_mode(void)
{
#if (KERNEL_VERSION(5, 4, 0) < LINUX_VERSION_CODE)
	dbg_snapshot_expire_watchdog();
#elif defined(GO_S2D_ID)
	dbg_snapshot_do_dpm_policy(GO_S2D_ID);
#else
	SCSC_TAG_WARNING(MXMAN, "GO_S2D_ID not defined. No scandump\n");
#endif
}
#endif

/* Retrieve memdump in sysfs global */
static ssize_t sysfs_show_memdump(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", memdump);
}

/* Update memdump in sysfs global */
static ssize_t sysfs_store_memdump(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 10, &memdump);
	if (r < 0)
		memdump = -1;

	switch (memdump) {
	case 0:
	case 2:
		disable_recovery_from_memdump_file = false;
		break;
	case 3:
	default:
		disable_recovery_from_memdump_file = true;
		break;
	}

	SCSC_TAG_INFO(MXMAN, "memdump: %d\n", memdump);

	return (r == 0) ? count : 0;
}

#if defined(CONFIG_WLBT_DCXO_TUNE)
/* Retrieve wlbt_dcxo_caldata in sysfs global */
static ssize_t sysfs_show_wlbt_dcxo_caldata(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u,%d\n", wlbt_dcxo_caldata, wlbt_temperature_value);
}

/* Update wlbt_dcxo_caldata in sysfs global */
static ssize_t sysfs_store_wlbt_dcxo_caldata(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int r;
	struct scsc_mif_abs *mif_abs;

	if (active_mxman == NULL || active_mxman->mx == NULL) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return 0;
	}

	/* Extract dcxo cal tune value and temperature from caldata string */
	r = sscanf(buf, "%u,%d", &wlbt_dcxo_caldata, &wlbt_temperature_value);
	SCSC_TAG_INFO(MXMAN, "Overrided dcxo_cal: %u, temperature value: %d)\n", wlbt_dcxo_caldata, wlbt_temperature_value);

	if (r > 0) {
		SCSC_TAG_INFO(MXMAN, "wlbt_dcxo_caldata: %d(hex value:0x%x)\n", wlbt_dcxo_caldata, wlbt_dcxo_caldata);

		mif_abs = scsc_mx_get_mif_abs(active_mxman->mx);

		r = mifmboxman_set_dcxo_tune_value(mif_abs, wlbt_dcxo_caldata);

		if (r)
			SCSC_TAG_ERR(MX_PROC, "Failed to set DCXO Tune(return: %d)\n", r);
		else {
			set_dcxo_state = DCXO_CONFIG_SYSFS;
			SCSC_TAG_INFO(MX_PROC, "Succeed to set %s DCXO Tune\n", "SYSFS");
		}
	}
	else {
		SCSC_TAG_ERR(MXMAN, "Invaild wlbt_dcxo_caldata value\n");
		return -EINVAL;
	}

	return (r == 0) ? count : -EINVAL;
}

/* Set wlbt_dcxo_caldata with the default value of the specific path for vendor */
static void mxman_set_default_dcxo_caldata(struct mxman *mxman)
{
	char *default_dcxo_path = "../etc/wifi/wlbt_dcxo_caldata";
	const struct firmware *e = NULL;
	int ret;
	struct scsc_mif_abs *mif_abs;

	if (set_dcxo_state != DCXO_CONFIG_NONE) {
		SCSC_TAG_WARNING(MXMAN, "'%s' DCXO Config state is not allowed\n",
			set_dcxo_state == DCXO_CONFIG_DEFAULT ? "DEFAULT":"SYSFS");
		return;
	}

	ret = mx140_request_file(mxman->mx, default_dcxo_path, &e);
	if (ret) {
		SCSC_TAG_WARNING(MXMAN, "Error Loading %s\n", default_dcxo_path);
		goto exit;
	} else if (!e) {
		SCSC_TAG_WARNING(MXMAN, "mx140_request_file() returned success, but firmware was null.\n");
		goto exit;
	}

	ret = sscanf(e->data, "%u,%d", &wlbt_dcxo_caldata, &wlbt_temperature_value);
	if (ret > 0) {
		SCSC_TAG_INFO(MXMAN, "dcxo_cal: %u(0x%x), temperature value: %d)\n",
			wlbt_dcxo_caldata, wlbt_dcxo_caldata, wlbt_temperature_value);

		mif_abs = scsc_mx_get_mif_abs(mxman->mx);

		ret = mifmboxman_set_dcxo_tune_value(mif_abs, wlbt_dcxo_caldata);

		if (ret)
			SCSC_TAG_ERR(MX_PROC, "Failed to set DCXO Tune(cause: %d)\n", ret);
		else {
			set_dcxo_state = DCXO_CONFIG_DEFAULT;
			SCSC_TAG_INFO(MX_PROC, "Succeed to set %s DCXO Tune\n", "DEFAULT");
		}
	}
	else {
		SCSC_TAG_ERR(MXMAN, "Invaild format of wlbt_dcxo_caldata value!\n");
	}
exit:
	mx140_release_file(mxman->mx, e);
}
#endif

struct kobject *mxman_wifi_kobject_ref_get(void)
{
	if (refcount++ == 0) {
		/* Create sysfs directory /sys/wifi */
		wifi_kobj_ref = kobject_create_and_add("wifi", NULL);
		kobject_get(wifi_kobj_ref);
		kobject_uevent(wifi_kobj_ref, KOBJ_ADD);
		SCSC_TAG_INFO(MXMAN, "wifi_kobj_ref: 0x%p\n", wifi_kobj_ref);
		WARN_ON(refcount == 0);
	}
	return wifi_kobj_ref;
}
EXPORT_SYMBOL(mxman_wifi_kobject_ref_get);

void mxman_wifi_kobject_ref_put(void)
{
	if (--refcount == 0) {
		kobject_put(wifi_kobj_ref);
		kobject_uevent(wifi_kobj_ref, KOBJ_REMOVE);
		wifi_kobj_ref = NULL;
		WARN_ON(refcount < 0);
	}
}
EXPORT_SYMBOL(mxman_wifi_kobject_ref_put);

/* Register memdump override */
void mxman_create_sysfs_memdump(void)
{
	int r;
	struct kobject *kobj_ref = mxman_wifi_kobject_ref_get();

	SCSC_TAG_INFO(MXMAN, "kobj_ref: 0x%p\n", kobj_ref);

	if (kobj_ref) {
		/* Create sysfs file /sys/wifi/memdump */
		r = sysfs_create_file(kobj_ref, &memdump_attr.attr);
		if (r) {
			/* Failed, so clean up dir */
			SCSC_TAG_ERR(MXMAN, "Can't create /sys/wifi/memdump\n");
			mxman_wifi_kobject_ref_put();
			return;
		}
	} else {
		SCSC_TAG_ERR(MXMAN, "failed to create /sys/wifi directory");
	}
}

/* Unregister memdump override */
void mxman_destroy_sysfs_memdump(void)
{
	if (!wifi_kobj_ref)
		return;

	/* Destroy /sys/wifi/memdump file */
	sysfs_remove_file(wifi_kobj_ref, &memdump_attr.attr);

	/* Destroy /sys/wifi virtual dir */
	mxman_wifi_kobject_ref_put();
}

#if defined(CONFIG_WLBT_DCXO_TUNE)
/* Register wlbt_dcxo_caldata override */
void mxman_create_sysfs_wlbt_dcxo_caldata(void)
{
	int r;
	struct kobject *kobj_ref = mxman_wifi_kobject_ref_get();

	SCSC_TAG_INFO(MXMAN, "kobj_ref: 0x%p\n", kobj_ref);

	if (kobj_ref) {
		/* Create sysfs file /sys/wifi/wlbt_dcxo_caldata */
		r = sysfs_create_file(kobj_ref, &dcxocal_attr.attr);
		if (r) {
			/* Failed, so clean up dir */
			SCSC_TAG_ERR(MXMAN, "Can't create /sys/wifi/wlbt_dcxo_caldata\n");
			mxman_wifi_kobject_ref_put();
			return;
		}
	} else {
		SCSC_TAG_ERR(MXMAN, "failed to create /sys/wifi directory");
	}
}

/* Unregister wlbt_dcxo_caldata override */
void mxman_destroy_sysfs_wlbt_dcxo_caldata(void)
{
	if (!wifi_kobj_ref)
		return;

	/* Destroy /sys/wifi/wlbt_dcxo_caldata file */
	sysfs_remove_file(wifi_kobj_ref, &dcxocal_attr.attr);

	/* Destroy /sys/wifi virtual dir */
	mxman_wifi_kobject_ref_put();
}
#endif

/* Track when WLBT reset fails to allow debug */
static u64 reset_failed_time;
static int firmware_runtime_flags;
static int syserr_command;
/**
 * This mxman reference is initialized/nullified via mxman_init/deinit
 * called by scsc_mx_create/destroy on module probe/remove.
 */
static struct mxman *active_mxman;
static bool send_fw_config_to_active_mxman(uint32_t fw_runtime_flags);
static bool send_syserr_cmd_to_active_mxman(u32 syserr_cmd);
static void mxman_fail_level8(struct mxman *mxman, u16 scsc_panic_code, const char *reason, enum scsc_subsystem sub);

static bool reset_failed;
static bool mxman_check_reset_failed(struct scsc_mif_abs *mif)
{
	return reset_failed; // || mif->mif_reset_failure(mif);
}

static void mxman_set_reset_failed(void)
{
	reset_failed = true;
}

static int fw_runtime_flags_setter(const char *val, const struct kernel_param *kp)
{
	int ret = -EINVAL;
	uint32_t fw_runtime_flags = 0;

	if (!val)
		return ret;
	ret = kstrtouint(val, 10, &fw_runtime_flags);
	if (!ret) {
		if (send_fw_config_to_active_mxman(fw_runtime_flags))
			firmware_runtime_flags = fw_runtime_flags;
		else
			ret = -EINVAL;
	}
	return ret;
}

/**
 * We don't bother to keep an updated copy of the runtime flags effectively
 * currently set into FW...we should add a new message answer handling both in
 * Kenrel and FW side to be sure and this is just to easy debug at the end.
 */
static struct kernel_param_ops fw_runtime_kops = { .set = fw_runtime_flags_setter, .get = NULL };

module_param_cb(firmware_runtime_flags, &fw_runtime_kops, NULL, 0200);
MODULE_PARM_DESC(
	firmware_runtime_flags,
	"0 = Proceed as normal (default); nnn = Provides FW runtime flags bitmask: unknown bits will be ignored.");

static int syserr_setter(const char *val, const struct kernel_param *kp)
{
	int ret = -EINVAL;
	u32 syserr_cmd = 0;

	if (!val)
		return ret;
	ret = kstrtouint(val, 10, &syserr_cmd);
	if (!ret) {
		enum scsc_subsystem sub_system = (u8)(syserr_cmd / 10);
		u8 level = (u8)(syserr_cmd % 10);

		if (((sub_system > 2) && (sub_system < 8)) || (sub_system > 8) || (level > MX_SYSERR_LEVEL_8))
			ret = -EINVAL;
		else if (level == MX_SYSERR_LEVEL_8) {
			if (active_mxman)
				mxman_fail_level8(active_mxman, SCSC_PANIC_CODE_HOST << 15, __func__, sub_system);
		} else if (send_syserr_cmd_to_active_mxman(syserr_cmd))
			syserr_command = syserr_cmd;
		else
			ret = -EINVAL;
	}
	return ret;
}

static struct kernel_param_ops syserr_kops = { .set = syserr_setter, .get = NULL };

module_param_cb(syserr_command, &syserr_kops, NULL, 0200);
MODULE_PARM_DESC(syserr_command, "Decimal XY - Trigger Type X(0,1,2,8), Level Y(1-8). Some combinations not supported");

/**
 * Maxwell Agent Management Messages.
 *
 * TODO: common defn with firmware, generated.
 *
 * The numbers here *must* match the firmware!
 */
enum { MM_START_IND = 0,
       MM_HALT_REQ = 1,
       MM_FORCE_PANIC = 2,
       MM_HOST_SUSPEND = 3,
       MM_HOST_RESUME = 4,
       MM_FW_CONFIG = 5,
       MM_HALT_RSP = 6,
       MM_FM_RADIO_CONFIG = 7,
       MM_LERNA_CONFIG = 8,
       MM_SYSERR_IND = 9,
       MM_SYSERR_CMD = 10 } ma_msg;

/**
 * Format of the Maxwell agent messages
 * on the Maxwell management transport stream.
 */
struct ma_msg_packet {
	uint8_t ma_msg; /* Message from ma_msg enum */
	uint32_t arg; /* Optional arg set by f/w in some to-host messages */
} __packed;

/**
 * Special case Maxwell management, carrying FM radio configuration structure
 */
struct ma_msg_packet_fm_radio_config {
	uint8_t ma_msg; /* Message from ma_msg enum */
	struct wlbt_fm_params fm_params; /* FM Radio parameters */
} __packed;

/* Helper function to check specific Maxwell Manager states */
static bool mxman_in_started_state(struct mxman *mxman)
{
#ifdef CONFIG_SCSC_INDEPENDENT_SUBSYSTEM
	if (mxman->mxman_state == MXMAN_STATE_STARTED_WLAN		||
		mxman->mxman_state == MXMAN_STATE_STARTED_WPAN		||
		mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN)
		return true;
#else
	if (mxman->mxman_state == MXMAN_STATE_STARTED)
		return true;
#endif
	return false;
}

static bool mxman_in_starting_state(struct mxman *mxman)
{
        if (mxman->mxman_state == MXMAN_STATE_STARTING)
                return true;

	return false;
}

/*
 * Helper function to determine if an scsc subsystem is active
 */
bool mxman_subsys_active(struct mxman *mxman, enum scsc_subsystem sub)
{
	switch(mxman->mxman_state) {
	case MXMAN_STATE_STARTED_WLAN:
		if (sub == SCSC_SUBSYSTEM_WLAN)
			return true;
		break;
	case MXMAN_STATE_STARTED_WPAN:
		if (sub == SCSC_SUBSYSTEM_WPAN)
			return true;
		break;
	case MXMAN_STATE_STARTED_WLAN_WPAN:
		if (sub == SCSC_SUBSYSTEM_WLAN_WPAN || sub == SCSC_SUBSYSTEM_WLAN || sub == SCSC_SUBSYSTEM_WPAN)
			return true;
		break;
	default:
		return false;
	};

	return false;
}

/*
 * Helper function to determine if scsc subsystem is in failed state
 * A subsystem e.g. WLAN is deemed to be in failed state if WLAN subsystem recovery is in
 * progress or full chip reset i.e. all subsystems recovery is going on.
 */
bool mxman_subsys_in_failed_state(struct mxman *mxman, enum scsc_subsystem sub)
{
	switch (mxman->mxman_state) {
	case MXMAN_STATE_FAILED_WLAN:
		if (sub == SCSC_SUBSYSTEM_WLAN)
			return true;
		break;
	case MXMAN_STATE_FAILED_WPAN:
		if (sub == SCSC_SUBSYSTEM_WPAN)
			return true;
		break;
	case MXMAN_STATE_FAILED_PMU:
	case MXMAN_STATE_FAILED_WLAN_WPAN:
			return true;
		break;
	default:
		return false;
	};

	return false;
}

static bool send_fw_config_to_active_mxman(uint32_t fw_runtime_flags)
{
	bool ret = false;
	struct srvman *srvman = NULL;

	SCSC_TAG_INFO(MXMAN, "\n");
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "Active MXMAN NOT FOUND...cannot send running FW config.\n");
		return ret;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && srvman->error) {
		mutex_unlock(&active_mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return ret;
	}

	if (active_mxman->mxman_state == MXMAN_STATE_STARTED) {
		struct ma_msg_packet message = { .ma_msg = MM_FW_CONFIG, .arg = fw_runtime_flags };

		SCSC_TAG_INFO(MXMAN, "MM_FW_CONFIG -  firmware_runtime_flags:%d\n", message.arg);
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(active_mxman->mx),
				      MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
		ret = true;
	} else {
		SCSC_TAG_INFO(MXMAN, "MXMAN is NOT STARTED...cannot send MM_FW_CONFIG msg.\n");
	}
	mutex_unlock(&active_mxman->mxman_mutex);

	return ret;
}

static bool send_syserr_cmd_to_active_mxman(u32 syserr_cmd)
{
	bool ret = false;
	struct srvman *srvman = NULL;

	SCSC_TAG_INFO(MXMAN, "\n");
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "Active MXMAN NOT FOUND...cannot send running FW config.\n");
		return ret;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && srvman->error) {
		mutex_unlock(&active_mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return ret;
	}

	if (active_mxman->mxman_state == MXMAN_STATE_STARTED) {
		struct ma_msg_packet message = { .ma_msg = MM_SYSERR_CMD, .arg = syserr_cmd };

		SCSC_TAG_INFO(MXMAN, "MM_SYSERR_CMD - Args %02d\n", message.arg);
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(active_mxman->mx),
				      MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
		ret = true;
	} else {
		SCSC_TAG_INFO(MXMAN, "MXMAN is NOT STARTED...cannot send MM_SYSERR_CMD msg.\n");
	}
	mutex_unlock(&active_mxman->mxman_mutex);

	return ret;
}

#if 0
static void print_mailboxes(struct mxman *mxman);
#endif
static int wait_for_mm_msg(struct mxman *mxman, struct completion *mm_msg_completion, ulong timeout_ms)
{
	int r;

	(void)mxman; /* unused */

	if (timeout_ms == 0) {
		/* Zero implies infinite wait */
		r = wait_for_completion_interruptible(mm_msg_completion);
		/* r = -ERESTARTSYS if interrupted, 0 if completed */
		return r;
	}
	r = wait_for_completion_timeout(mm_msg_completion, msecs_to_jiffies(timeout_ms));
	if (r == 0) {
		SCSC_TAG_ERR(MXMAN, "timeout\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int wait_for_mm_msg_start_ind(struct mxman *mxman)
{
	return wait_for_mm_msg(mxman, &mxman->mm_msg_start_ind_completion, mm_completion_timeout_ms);
}

static int wait_for_mm_msg_halt_rsp(struct mxman *mxman)
{
	int r;
	(void)mxman; /* unused */

	if (MM_HALT_RSP_TIMEOUT_MS == 0) {
		/* Zero implies infinite wait */
		r = wait_for_completion_interruptible(&mxman->mm_msg_halt_rsp_completion);
		/* r = -ERESTARTSYS if interrupted, 0 if completed */
		return r;
	}

	r = wait_for_completion_timeout(&mxman->mm_msg_halt_rsp_completion, msecs_to_jiffies(MM_HALT_RSP_TIMEOUT_MS));
	if (r)
		SCSC_TAG_INFO(MXMAN, "Received MM_HALT_RSP from firmware\n");
	else
		SCSC_TAG_INFO(MXMAN, "MM_HALT_RSP timeout\n");

	return r;
}

static int send_mm_msg_stop_blocking(struct mxman *mxman, enum scsc_subsystem sub)
{
	int r;
#if IS_ENABLED(CONFIG_SCSC_FM)
	struct ma_msg_packet message = { .ma_msg = MM_HALT_REQ,
		.arg = mxman->on_halt_ldos_on };
#else
	struct ma_msg_packet message = { .ma_msg = MM_HALT_REQ };
#endif
	struct mxmgmt_transport *mxmgmt_transport;

	reinit_completion(&mxman->mm_msg_halt_rsp_completion);
	if (sub == SCSC_SUBSYSTEM_WLAN)
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport(mxman->mx);
	else
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport_wpan(mxman->mx);

	mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
	r = wait_for_mm_msg_halt_rsp(mxman);
	if (r) {
		/*
		 * MM_MSG_HALT_RSP is not implemented in all versions of firmware, so don't treat it's non-arrival
		 * as an error
		 */
		SCSC_TAG_INFO(MXMAN, "wait_for_MM_HALT_RSP completed\n");
	}

	return 0;
}

static char *chip_version(u32 rf_hw_ver)
{
	switch (rf_hw_ver & 0x00ff) {
	default:
		break;
	case 0x00b0:
		if ((rf_hw_ver & 0xff00) > 0x1000)
			return "S610/S611";
		else
			return "S610";
	case 0x00b1:
		return "S612";
	case 0x00b2:
		return "S620";
	case 0x0000:
#if !defined CONFIG_SOC_EXYNOS9610 && !defined CONFIG_SOC_EXYNOS9630
		return "Error: check if RF chip is present";
#else
		return "Unknown";
#endif
	}
	return "Unknown";
}

/*
 * This function is used in this file and in mxproc.c to generate consistent
 * RF CHIP VERSION string for logging on console and for storing the same
 * in proc/drivers/mxman_info/rf_chip_version file.
 */
int mxman_print_rf_hw_version(struct mxman *mxman, char *buf, const size_t bufsz)
{
	int r;

	r = snprintf(buf, bufsz, "RF_CHIP_VERSION: 0x%04x: %s (0x%02x), EVT%x.%x\n", mxman->rf_hw_ver,
		     chip_version(mxman->rf_hw_ver), (mxman->rf_hw_ver & 0x00ff), ((mxman->rf_hw_ver >> 12) & 0xfU),
		     ((mxman->rf_hw_ver >> 8) & 0xfU));

	return r;
}

static void mxman_print_versions(struct mxman *mxman)
{
	char buf[80];

	memset(buf, '\0', sizeof(buf));

	(void)mxman_print_rf_hw_version(mxman, buf, sizeof(buf));

	SCSC_TAG_INFO(MXMAN, "%s", buf);
	SCSC_TAG_INFO(MXMAN, "WLBT FW: %s\n", mxman->fw_build_id);
	SCSC_TAG_INFO(MXMAN, "WLBT Driver: %d.%d.%d.%d.%d\n", SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION,
		      SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER);
#ifdef CONFIG_SCSC_WLBTD
	scsc_wlbtd_get_and_print_build_type();
#endif
}

/** Receive handler for messages from the FW along the maxwell management transport */
static void mxman_message_handler(const void *message, void *data)
{
	struct mxman *mxman = (struct mxman *)data;

	/* Forward the message to the applicable service to deal with */
	const struct ma_msg_packet *msg = message;

	switch (msg->ma_msg) {
	case MM_START_IND:
		/* The arg can be used to determine the WLBT/S610 hardware revision */
		SCSC_TAG_INFO(MXMAN, "Received MM_START_IND message from the firmware, arg=0x%04x\n", msg->arg);
		mxman->rf_hw_ver = msg->arg;
		mxman_print_versions(mxman);
		atomic_inc(&mxman->boot_count);
		complete(&mxman->mm_msg_start_ind_completion);
		break;
	case MM_HALT_RSP:
		complete(&mxman->mm_msg_halt_rsp_completion);
		SCSC_TAG_INFO(MXMAN, "Received MM_HALT_RSP message from the firmware\n");
		break;
	case MM_LERNA_CONFIG:
		/* Message response to a firmware configuration query. */
		SCSC_TAG_INFO(MXMAN, "Received MM_LERNA_CONFIG message from firmware\n");
		scsc_lerna_response(message);
		break;
	case MM_SYSERR_IND:
		/* System Error report from firmware */
		SCSC_TAG_INFO(MXMAN, "Received MM_SYSERR_IND message from firmware\n");
		mx_syserr_handler(mxman, message);
		break;
	default:
		/* HERE: Unknown message, raise fault */
		SCSC_TAG_WARNING(MXMAN, "Received unknown message from the firmware: msg->ma_msg=%d\n", msg->ma_msg);
		break;
	}
}

static void mxman_reset_chip(struct mxman *mxman)
{
	int ret;
	struct scsc_mif_abs *mif;
	struct fwhdr_if *fw_if = mxman->fw_wlan;

	SCSC_TAG_INFO(MXMAN, "\n");

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Unregister minimoredump client */
	scsc_log_collector_unregister_client(&mini_moredump_client);
#endif
	/**
	* Deinit mxlogger on last service stop...BUT before asking for HALT
	*/
	mxlogger_deinit(mxman->mx, scsc_mx_get_mxlogger(mxman->mx));
#endif
	mif = scsc_mx_get_mif_abs(mxman->mx);
	/* If reset is failed, prevent new resets */
	if (mxman_check_reset_failed(mif)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0))
		struct  __kernel_old_timeval tval = ns_to_kernel_old_timeval(reset_failed_time);
#else
		struct timeval tval = ns_to_timeval(reset_failed_time);
#endif
		SCSC_TAG_ERR(MXMAN, "previous reset failed at [%6lu.%06ld], ignoring\n", tval.tv_sec, tval.tv_usec);
		return;
	}

	(void)snprintf(mxman->fw_build_id, sizeof(mxman->fw_build_id), FW_BUILD_ID_UNKNOWN);

	mxproc_remove_ctrl_proc_dir(&mxman->mxproc);

	/* Shutdown the hardware */
	ret = mxman_res_reset(mxman, true);
	if (ret) {
		reset_failed_time = local_clock();
		SCSC_TAG_INFO(MXMAN, "HW reset failed\n");
		mxman_set_reset_failed();

		/* Save log at point of failure */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		scsc_log_collector_schedule_collection(SCSC_LOG_HOST_COMMON, SCSC_LOG_HOST_COMMON_REASON_STOP);
#endif
	}

	panicmon_deinit(scsc_mx_get_panicmon(mxman->mx));

	fw_if->crc_wq_stop(fw_if);

	mxman_res_pmu_deinit(mxman);
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	mxman_res_mappings_logger_deinit(mxman);
#endif
	mxman_res_mappings_allocator_deinit(mxman);

	mxman_res_deinit_common(mxman);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	if (mif->recovery_disabled_unreg)
		mif->recovery_disabled_unreg(mif);
#endif
	whdr_destroy(mxman->fw_wlan);
	bhdr_destroy(mxman->fw_wpan);
	/* Release the MIF memory resources */
	mxman_res_mem_unmap(mxman, mxman->start_dram);
}

#ifdef CONFIG_SOC_S5E8825
static void mxman_pmu_message_handler(void *data, u32 cmd)
{
	struct mxman *mxman = (struct mxman *)data;

	switch(cmd) {
	case MIFPMU_ERROR_WLAN:
		mxman_fail(mxman, SCSC_PANIC_CODE_FW << 15, __func__, SCSC_SUBSYSTEM_WLAN);
		break;
	case MIFPMU_ERROR_WPAN:
		mxman_fail(mxman, SCSC_PANIC_CODE_FW << 15, __func__, SCSC_SUBSYSTEM_WPAN);
		break;
	default:
		SCSC_TAG_INFO(MXMAN, "Incorrect command\n");
	}
}
#endif

/* Allocate the memory , read the FW */
static int mxman_start_boot(struct mxman *mxman, enum scsc_subsystem sub)
{
	int ret = 0;

	/* If chip is in reset , try first to allocate memory */
	ret = mxman_res_mem_map(mxman, &mxman->start_dram, &mxman->size_dram);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error allocating dram\n");
		return ret;
	}
	SCSC_TAG_INFO(MXMAN, "Allocated %zu bytes %p\n", mxman->size_dram, mxman->start_dram);

	/* PMU init is required before fw_init!.. fw_init will call mifpmuman_load_fw */
#ifdef CONFIG_SOC_S5E8825
	ret = mxman_res_pmu_init(mxman, &mxman_pmu_message_handler);
#else
	ret = mxman_res_pmu_init(mxman);
#endif
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_pmu_init\n");
		goto error;
	}

	/* Init the FW */
	/* fw_wlan an fw_wpan handlers will be returned in fw_wlan */
	ret = mxman_res_fw_init(mxman, &mxman->fw_wlan, &mxman->fw_wpan, mxman->start_dram, mxman->size_dram);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_fw_init\n");
		goto error;
	}

	/* Copy the fw build id */
	memcpy(saved_fw_build_id, mxman->fw_build_id, sizeof(saved_fw_build_id));

	if (sub == SCSC_SUBSYSTEM_WPAN && mxman->wpan_present == false) {
		SCSC_TAG_ERR(MXMAN, "WPAN is not present in the FW image\n");
		ret = -ENOENT;
		goto error;
	}

	/* Memory mappings and mifram allocator initialization  */
	ret = mxman_res_mappings_allocator_init(mxman, mxman->start_dram);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_mappings_allocator_init\n");
		goto error;
	}

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	ret = mxman_res_mappings_logger_init(mxman, mxman->start_dram);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_mappings_logger_init\n");
		/* Don't stop here... continue */
	}
#endif
	mxproc_create_ctrl_proc_dir(&mxman->mxproc, mxman);
	panicmon_init(scsc_mx_get_panicmon(mxman->mx), mxman->mx);

	/* Change state to STARTING to allow coredump as we come out of reset */
	mxman->mxman_state = MXMAN_STATE_STARTING;
	/* Release from reset */
	ret = mxman_res_reset(mxman, false);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_reset\n");
		goto error;
	}
	return 0;

error:
	/* Destroy FW objs */
	whdr_destroy(mxman->fw_wlan);
	bhdr_destroy(mxman->fw_wpan);

	/* Unmap memory if error */
	mxman_res_mem_unmap(mxman, mxman->start_dram);
	return ret;
}

/* Function that should start the chip the chip if not enabled. If it is
 * enabled function will initialize all subsystem resources   */
static int mxman_start(struct mxman *mxman, enum scsc_subsystem sub, void *data, size_t data_sz)
{
	int ret = 0;

	/* At this point memory should be mapped and PMU booted
	 * Specific chip resources and
	 * boot pre-conditions should be allocated and assigned before booting
	 * the specific subsystem */
	ret = mxman_res_init_subsystem(mxman, sub, data, data_sz, &mxman_message_handler);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_init_subsystem\n");
		goto error;
	}

	ret = mxman_res_pmu_boot(mxman, sub);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error mxman_res_pmu_boot\n");
		goto error;
	}
error:
	return ret;
}

bool is_bug_on_enabled(struct scsc_mx *mx)
{
	bool bug_on_enabled;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	const struct firmware *firm;
	int r;
#endif

	if ((memdump == 3) && (disable_recovery_handling == MEMDUMP_FILE_FOR_RECOVERY))
		bug_on_enabled = true;
	else
		bug_on_enabled = false;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	(void)firm; /* unused */
	(void)r; /* unused */
	goto out;
#else
	/* non SABLE platforms should also follow /sys/wifi/memdump if enabled */
	if (disable_recovery_handling == MEMDUMP_FILE_FOR_RECOVERY)
		goto out;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
		/* for legacy platforms (including Andorid P) using .memdump.info */
#if defined(SCSC_SEP_VERSION) && (SCSC_SEP_VERSION >= 9)
#define MX140_MEMDUMP_INFO_FILE "/data/vendor/conn/.memdump.info"
#else
#define MX140_MEMDUMP_INFO_FILE "/data/misc/conn/.memdump.info"
#endif

	SCSC_TAG_INFO(MX_FILE, "Loading %s file\n", MX140_MEMDUMP_INFO_FILE);
	r = mx140_request_file(mx, MX140_MEMDUMP_INFO_FILE, &firm);
	if (r) {
		SCSC_TAG_WARNING(MX_FILE, "Error Loading %s file %d\n", MX140_MEMDUMP_INFO_FILE, r);
		return bug_on_enabled;
	}
	if (firm->size < sizeof(char))
		SCSC_TAG_WARNING(MX_FILE, "file is too small\n");
	else if (*firm->data == '3')
		bug_on_enabled = true;
	mx140_release_file(mx, firm);
#endif //(LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
#endif //CONFIG_SCSC_LOG_COLLECTION
out:
	SCSC_TAG_INFO(MX_FILE, "bug_on_enabled %d\n", bug_on_enabled);
	return bug_on_enabled;
}
EXPORT_SYMBOL(is_bug_on_enabled);

static void print_panic_code_legacy(u16 code)
{
	u16 tech = code & SCSC_PANIC_TECH_MASK;
	u16 origin = code & SCSC_PANIC_ORIGIN_MASK;

	SCSC_TAG_INFO(MXMAN, "Decoding panic code=0x%x:\n", code);
	switch (origin) {
	default:
		SCSC_TAG_INFO(MXMAN, "Failed to identify panic origin\n");
		break;
	case SCSC_PANIC_ORIGIN_FW:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_ORIGIN_FW\n");
		break;
	case SCSC_PANIC_ORIGIN_HOST:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_ORIGIN_HOST\n");
		break;
	}

	switch (tech) {
	default:
		SCSC_TAG_INFO(MXMAN, "Failed to identify panic technology\n");
		break;
	case SCSC_PANIC_TECH_WLAN:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_TECH_WLAN\n");
		break;
	case SCSC_PANIC_TECH_CORE:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_TECH_CORE\n");
		break;
	case SCSC_PANIC_TECH_BT:
		SCSC_TAG_INFO(MXMAN, "SCSC_PANIC_TECH_BT\n");
		break;
	case SCSC_PANIC_TECH_UNSP:
		SCSC_TAG_INFO(MXMAN, "PANIC_TECH_UNSP\n");
		break;
	}
	SCSC_TAG_INFO(MXMAN, "panic subcode=0x%x\n", code & SCSC_PANIC_SUBCODE_MASK_LEGACY);
}

static void __print_panic_code(u16 code, enum scsc_subsystem sub)
{
	u16 origin = code & SCSC_PANIC_ORIGIN_MASK; /* Panic origin (host/fw) */
	u16 subcode = code & SCSC_PANIC_SUBCODE_MASK; /* The panic code */

	SCSC_TAG_INFO(MXMAN, "============= %s PANIC CODE ==========\n",
		     (sub == SCSC_SUBSYSTEM_WPAN) ? "WPAN" : "WLAN");
	SCSC_TAG_INFO(MXMAN, "Decoding panic code=0x%x:\n", code);
	SCSC_TAG_INFO(MXMAN, "panic subcode=0x%x\n", code & SCSC_PANIC_SUBCODE_MASK);

	switch (origin) {
	default:
		SCSC_TAG_INFO(MXMAN, "Failed to identify panic origin\n");
		break;
	case SCSC_PANIC_ORIGIN_FW:
		SCSC_TAG_INFO(MXMAN, "WLBT FW PANIC: 0x%02x\n", subcode);
		break;
	case SCSC_PANIC_ORIGIN_HOST:
		SCSC_TAG_INFO(MXMAN, "WLBT HOST detected FW failure, service:\n");
		switch (subcode >> SCSC_SYSERR_HOST_SERVICE_SHIFT) {
		case SCSC_SERVICE_ID_WLAN:
			SCSC_TAG_INFO(MXMAN, " WLAN\n");
			break;
		case SCSC_SERVICE_ID_BT:
			SCSC_TAG_INFO(MXMAN, " BT\n");
			break;
		case SCSC_SERVICE_ID_ANT:
			SCSC_TAG_INFO(MXMAN, " ANT\n");
			break;
		case SCSC_SERVICE_ID_CLK20MHZ:
			SCSC_TAG_INFO(MXMAN, " CLK20MHZ\n");
			break;
		default:
			SCSC_TAG_INFO(MXMAN, " Service 0x%x\n", subcode);
			break;
		}
		break;
	}
	SCSC_TAG_INFO(MXMAN, "============= END %s PANIC CODE ==========\n",
		     (sub == SCSC_SUBSYSTEM_WPAN) ? "WPAN" : "WLAN");
}

static void print_panic_code(struct mxman *mxman)
{
	if (mxman->scsc_panic_code)
		__print_panic_code(mxman->scsc_panic_code, SCSC_SUBSYSTEM_WLAN);
	if (mxman->scsc_panic_code_wpan)
		__print_panic_code(mxman->scsc_panic_code_wpan, SCSC_SUBSYSTEM_WPAN);
}

/**
 * Print the last panic record collected to aid in post mortem.
 *
 * Helps when all we have is kernel log showing WLBT failed some time ago
 *
 * Only prints the R7 record
 */
static void __mxman_show_last_panic_wlan(struct mxman *mxman)
{
	u32 r4_panic_record_length = 0; /* in u32s */
	u32 r4_panic_stack_record_length = 0; /* in u32s */

	SCSC_TAG_INFO(MXMAN, "\n\n--- DETAILS OF LAST WLBT FAILURE [WLAN]---\n\n");

	switch (mxman->scsc_panic_code & SCSC_PANIC_ORIGIN_MASK) {
	case SCSC_PANIC_ORIGIN_HOST:
		SCSC_TAG_INFO(MXMAN, "Last panic was host induced:\n");
		break;

	case SCSC_PANIC_ORIGIN_FW:
		SCSC_TAG_INFO(MXMAN, "Last panic was FW:\n");
		fw_parse_r4_panic_record(mxman->last_panic_rec_r, &r4_panic_record_length, NULL, true);
		fw_parse_r4_panic_stack_record(mxman->last_panic_stack_rec_r, &r4_panic_stack_record_length, true);
		break;

	default:
		SCSC_TAG_INFO(MXMAN, "Last panic unknown origin %d\n", mxman->scsc_panic_code & SCSC_PANIC_ORIGIN_MASK);
		break;
	}

	print_panic_code(mxman);

	SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n", mxman->failure_reason[0] ? mxman->failure_reason : "<null>");
	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", disable_recovery_handling ? "off" : "on");

	if (mxman_recovery_disabled()) {
		/* Labour the point that a reboot is needed when autorecovery is disabled */
		SCSC_TAG_INFO(MXMAN, "\n\n*** HANDSET REBOOT NEEDED TO RESTART WLAN AND BT ***\n\n");
	}

	SCSC_TAG_INFO(MXMAN, "\n\n--- END DETAILS OF LAST WLBT FAILURE ---\n\n");
}

/**
 * Print the last panic record collected to aid in post mortem.
 *
 * Helps when all we have is kernel log showing WLBT failed some time ago
 *
 */
static void __mxman_show_last_panic_wpan(struct mxman *mxman)
{
	SCSC_TAG_INFO(MXMAN, "\n\n--- DETAILS OF LAST WLBT FAILURE [WPAN]---\n\n");

	switch (mxman->scsc_panic_code_wpan & SCSC_PANIC_ORIGIN_MASK) {
	case SCSC_PANIC_ORIGIN_HOST:
		SCSC_TAG_INFO(MXMAN, "Last panic was host induced:\n");
		break;

	case SCSC_PANIC_ORIGIN_FW:
		SCSC_TAG_INFO(MXMAN, "Last panic was FW WPAN. No stored records\n");
		break;

	default:
		SCSC_TAG_INFO(MXMAN, "Last panic unknown origin %d\n", mxman->scsc_panic_code_wpan & SCSC_PANIC_ORIGIN_MASK);
		break;
	}

	print_panic_code(mxman);

	SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n", mxman->failure_reason[0] ? mxman->failure_reason : "<null>");
	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", disable_recovery_handling ? "off" : "on");

	if (mxman_recovery_disabled()) {
		/* Labour the point that a reboot is needed when autorecovery is disabled */
		SCSC_TAG_INFO(MXMAN, "\n\n*** HANDSET REBOOT NEEDED TO RESTART WLAN AND BT ***\n\n");
	}

	SCSC_TAG_INFO(MXMAN, "\n\n--- END DETAILS OF LAST WLBT FAILURE ---\n\n");
}


void mxman_show_last_panic(struct mxman *mxman)
{
	if (mxman->scsc_panic_code)
		__mxman_show_last_panic_wlan(mxman);
	if (mxman->scsc_panic_code_wpan)
		__mxman_show_last_panic_wpan(mxman);
}

static void process_panic_record(struct mxman *mxman, bool dump)
{
	u32 *wpan_panic_record = NULL;
	u32 *r4_panic_record = NULL;
	u32 *r4_panic_stack_record = NULL;
	u32 *m4_panic_record = NULL;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	u32 *m4_1_panic_record = NULL;
#endif
	u32 r4_panic_record_length = 0; /* in u32s */
	u32 r4_panic_stack_record_offset = 0; /* in bytes */
	u32 r4_panic_stack_record_length = 0; /* in u32s */
	u32 m4_panic_record_length = 0; /* in u32s */
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	u32 m4_1_panic_record_length = 0; /* in u32s */
#endif
	u32 full_panic_code = 0;
	u32 full_panic_code_wpan = 0;
	bool wpan_panic_record_ok = false;
	bool r4_panic_record_ok = false;
	bool r4_panic_stack_record_ok = false;
	bool m4_panic_record_ok = false;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	bool m4_1_panic_record_ok = false;
#endif
	bool r4_sympathetic_panic_flag = false;
	bool m4_sympathetic_panic_flag = false;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	bool m4_1_sympathetic_panic_flag = false;
#endif
	struct fwhdr_if *fw_if = mxman->fw_wlan;
	struct fwhdr_if *bt_if = mxman->fw_wpan;

	/* some configurable delay before accessing the panic record */
	msleep(panic_record_delay);

	/*
	* Check if the panic was trigered by MX and set the subcode if so.
	*/
	/* WPAN ?*/
	if (((mxman->scsc_panic_code_wpan & SCSC_PANIC_ORIGIN_MASK) == SCSC_PANIC_ORIGIN_FW) &&
	   (mxman->scsc_panic_sub == SCSC_SUBSYSTEM_WPAN || mxman->scsc_panic_sub == SCSC_SUBSYSTEM_WLAN_WPAN)) {
		/* Check WPAN subsystem */
		if (bt_if->get_panic_record_offset(bt_if, SCSC_MIF_ABS_TARGET_WPAN)) {
			wpan_panic_record =
				(u32 *)(mxman->fw + bt_if->get_panic_record_offset(bt_if, SCSC_MIF_ABS_TARGET_WPAN));
			wpan_panic_record_ok = fw_parse_wpan_panic_record(wpan_panic_record, &full_panic_code_wpan, dump);
			mxman->scsc_panic_code_wpan |= SCSC_PANIC_CODE_MASK & full_panic_code_wpan;
		} else {
			SCSC_TAG_INFO(MXMAN, "WPAN panic record doesn't exist in the firmware header\n");
		}
	}

	/* WLAN ?*/
	if (((mxman->scsc_panic_code & SCSC_PANIC_ORIGIN_MASK) == SCSC_PANIC_ORIGIN_FW) &&
	   (mxman->scsc_panic_sub == SCSC_SUBSYSTEM_WLAN || mxman->scsc_panic_sub == SCSC_SUBSYSTEM_WLAN_WPAN)) {
		/* Check WLAN subsystem */
		if (fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_WLAN)) {
			r4_panic_record =
				(u32 *)(mxman->fw + fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_WLAN));
			r4_panic_record_ok = fw_parse_r4_panic_record(r4_panic_record, &r4_panic_record_length,
								      &r4_panic_stack_record_offset, dump);
		} else {
			SCSC_TAG_INFO(MXMAN, "WLAN panic record doesn't exist in the firmware header\n");
		}

		if (fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_FXM_1)) {
			m4_panic_record =
				(u32 *)(mxman->fw + fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_FXM_1));
			m4_panic_record_ok = fw_parse_m4_panic_record(m4_panic_record, &m4_panic_record_length, dump);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
		} else if (fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_FXM_2)) {
			m4_1_panic_record =
				(u32 *)(mxman->fw + fw_if->get_panic_record_offset(fw_if, SCSC_MIF_ABS_TARGET_FXM_2));
			m4_1_panic_record_ok =
				fw_parse_m4_panic_record(m4_1_panic_record, &m4_1_panic_record_length, dump);
#endif
		} else {
			SCSC_TAG_INFO(MXMAN, "FXMAC panic record doesn't exist in the firmware header\n");
		}

		/* Extract and print the panic code */
		switch (r4_panic_record_length) {
		default:
			SCSC_TAG_WARNING(MXMAN, "Bad panic record length/subversion\n");
			break;
		case SCSC_R4_V2_MINOR_52:
			if (r4_panic_record_ok) {
				full_panic_code = r4_panic_record[2];
				mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & full_panic_code;
			} else if (m4_panic_record_ok)
				mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & m4_panic_record[2];
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
			else if (m4_1_panic_record_ok)
				mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & m4_1_panic_record[2];
#endif
			/* Set unspecified technology for now */
			mxman->scsc_panic_code |= SCSC_PANIC_TECH_UNSP;
			print_panic_code_legacy(mxman->scsc_panic_code);
			break;
		case SCSC_R4_V2_MINOR_54:
		case SCSC_R4_V2_MINOR_53:
			if (r4_panic_record_ok) {
				/* Save the last R4 panic record for future display */
				BUG_ON(sizeof(mxman->last_panic_rec_r) < r4_panic_record_length * sizeof(u32));
				memcpy((u8 *)mxman->last_panic_rec_r, (u8 *)r4_panic_record,
				       r4_panic_record_length * sizeof(u32));
				mxman->last_panic_rec_sz = r4_panic_record_length;

				r4_sympathetic_panic_flag = fw_parse_get_r4_sympathetic_panic_flag(r4_panic_record);
				if (dump)
					SCSC_TAG_INFO(MXMAN, "r4_panic_record_ok=%d r4_sympathetic_panic_flag=%d\n",
						      r4_panic_record_ok, r4_sympathetic_panic_flag);
				/* Check panic stack if present */
				if (r4_panic_record_length >= SCSC_R4_V2_MINOR_54) {
					r4_panic_stack_record = (u32 *)(mxman->fw + r4_panic_stack_record_offset);
					r4_panic_stack_record_ok = fw_parse_r4_panic_stack_record(
						r4_panic_stack_record, &r4_panic_stack_record_length, dump);
				} else {
					r4_panic_stack_record_ok = false;
					r4_panic_stack_record_length = 0;
				}
				if (r4_sympathetic_panic_flag == false) {
					/* process R4 record */
					full_panic_code = r4_panic_record[3];
					mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & full_panic_code;
					if (dump)
						print_panic_code(mxman);
					break;
				}
			}
			if (m4_panic_record_ok) {
				m4_sympathetic_panic_flag = fw_parse_get_m4_sympathetic_panic_flag(m4_panic_record);
				if (dump)
					SCSC_TAG_INFO(MXMAN, "m4_panic_record_ok=%d m4_sympathetic_panic_flag=%d\n",
						      m4_panic_record_ok, m4_sympathetic_panic_flag);
				if (m4_sympathetic_panic_flag == false) {
					/* process M4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & m4_panic_record[3];
				} else if (r4_panic_record_ok) {
					/* process R4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_CODE_MASK & r4_panic_record[3];
				}
				if (dump)
					print_panic_code(mxman);
			}
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT /* this is wrong but not sure what is "right" */
			/* "sympathetic panics" are not really a thing on the Neus architecture unless */
			/* generated by the host                                                       */
			if (m4_1_panic_record_ok) {
				m4_1_sympathetic_panic_flag = fw_parse_get_m4_sympathetic_panic_flag(m4_1_panic_record);
				if (dump) {
					SCSC_TAG_DEBUG(MXMAN,
						       "m4_1_panic_record_ok=%d m4_1_sympathetic_panic_flag=%d\n",
						       m4_1_panic_record_ok, m4_1_sympathetic_panic_flag);
				}
				if (m4_1_sympathetic_panic_flag == false) {
					/* process M4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_SUBCODE_MASK & m4_1_panic_record[3];
				} else if (r4_panic_record_ok) {
					/* process R4 record */
					mxman->scsc_panic_code |= SCSC_PANIC_SUBCODE_MASK & r4_panic_record[3];
				}
				if (dump)
					print_panic_code(mxman);
			}
#endif
			break;
		}
	}

	if (r4_panic_record_ok || wpan_panic_record_ok) {
		/* Populate syserr info with panic equivalent, but don't modify level  */
		mxman->last_syserr.subsys = (u8)((full_panic_code >> SYSERR_SUB_SYSTEM_POSN) & SYSERR_SUB_SYSTEM_MASK);
		mxman->last_syserr.type = (u8)((full_panic_code >> SYSERR_TYPE_POSN) & SYSERR_TYPE_MASK);
		mxman->last_syserr.subcode = (u16)((full_panic_code >> SYSERR_SUB_CODE_POSN) & SYSERR_SUB_CODE_MASK);

		if (dump) {
			SCSC_TAG_DEBUG(MXMAN, "last_syserr.subsys 0x%d\n", mxman->last_syserr.subsys);
			SCSC_TAG_DEBUG(MXMAN, "last_syserr.type 0x%d\n", mxman->last_syserr.type);
			SCSC_TAG_DEBUG(MXMAN, "last_syserr.subcode 0x%x\n", mxman->last_syserr.subcode);
			SCSC_TAG_DEBUG(MXMAN, "last_syserr.subcode 0x%x\n", mxman->last_syserr.subcode);
			SCSC_TAG_DEBUG(MXMAN, "scsc_panic_code_wpan %x\n", mxman->scsc_panic_code_wpan);
			SCSC_TAG_DEBUG(MXMAN, "scsc_panic_code %x\n", mxman->scsc_panic_code);
		}
	}
}

/* Check whether syserr should be promoted based on frequency or service driver override */
static void mxman_check_promote_syserr(struct mxman *mxman)
{
	int i;
	int entry = -1;
	unsigned long now = jiffies;

	/* We use 0 as a NULL timestamp so avoid this */
	now = (now) ? now : 1;

	/* Promote all L7 to L8 to maintain existing moredump scheme,
	 * unless code is found in the filter list
	 */
	if (mxman->last_syserr.level == MX_SYSERR_LEVEL_7) {
		u8 new_level = MX_SYSERR_LEVEL_7;
		for (i = 0; i < ARRAY_SIZE(mxfwconfig_syserr_no_promote); i++) {
			/* End of list reached without match, promote to L8 by default */
			if (mxfwconfig_syserr_no_promote[i] == 0) {
				new_level = MX_SYSERR_LEVEL_8;
				entry = i;
				break;
			}

			/* If 0xFFFFFFFF in list: only if host induced, promote to L8 */
			if (mxfwconfig_syserr_no_promote[i] == 0xFFFFFFFF) {
				if ((mxman->last_syserr.subsys == SYSERR_SUB_SYSTEM_HOST ||
				     mxman->last_syserr.subcode == 0xF0)) {
					/* Host induced so promote */
					new_level = MX_SYSERR_LEVEL_8;
				}
				entry = i;
				break;
			}

			/* If code is in list, don't promote. Note that subsequent loop
			 * detection checks may promote later, though.
			 */
			if (mxfwconfig_syserr_no_promote[i] == mxman->last_syserr.subcode) {
				entry = i;
				break;
			}
		}

		SCSC_TAG_INFO(MXMAN, "entry %d = 0x%x: syserr in %d, subcode 0x%0x: L%d -> L%d\n", entry,
			      (entry != -1) ? mxfwconfig_syserr_no_promote[entry] : 0, mxman->last_syserr.subsys,
			      mxman->last_syserr.subcode, mxman->last_syserr.level, new_level);

		mxman->last_syserr.level = new_level;
	}

	/* last_syserr_level7_recovery_time is always zero-ed before we restart the chip */
	if (mxman->last_syserr_level7_recovery_time) {
		/* Have we had a too recent system error level 7 reset
		 * Chance of false positive here is low enough to be acceptable
		 */
		if ((syserr_level7_min_interval) &&
		    (time_in_range(now, mxman->last_syserr_level7_recovery_time,
				   mxman->last_syserr_level7_recovery_time +
					   msecs_to_jiffies(syserr_level7_min_interval)))) {
			SCSC_TAG_INFO(MXMAN, "Level 7 failure raised to level 8 (less than %dms after last)\n",
				      syserr_level7_min_interval);
			mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
		} else if (syserr_level7_monitor_period) {
			/* Have we had too many system error level 7 resets in one period? */
			/* This will be the case if all our stored history was in this period */
			bool out_of_danger_period_found = false;

			for (i = 0; (i < SYSERR_LEVEL7_HISTORY_SIZE) && (!out_of_danger_period_found); i++)
				out_of_danger_period_found =
					((!syserr_level7_history[i]) ||
					 (!time_in_range(now, syserr_level7_history[i],
							 syserr_level7_history[i] +
								 msecs_to_jiffies(syserr_level7_monitor_period))));

			if (!out_of_danger_period_found) {
				SCSC_TAG_INFO(MXMAN, "Level 7 failure raised to level 8 (too many within %dms)\n",
					      syserr_level7_monitor_period);
				mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
			}
		}
	} else
		/* First syserr level 7 reset since chip was (re)started - zap history */
		for (i = 0; i < SYSERR_LEVEL7_HISTORY_SIZE; i++)
			syserr_level7_history[i] = 0;

	if ((mxman->last_syserr.level != MX_SYSERR_LEVEL_8) && (trigger_moredump_level > MX_SYSERR_LEVEL_7)) {
		/* Allow services to raise to level 8 */
		mxman->last_syserr.level = srvman_notify_services(scsc_mx_get_srvman(mxman->mx), &mxman->last_syserr);
	}

	if (mxman->last_syserr.level != MX_SYSERR_LEVEL_8) {
		/* Log this in our history */
		syserr_level7_history[syserr_level7_history_index++ % SYSERR_LEVEL7_HISTORY_SIZE] = now;
		mxman->last_syserr_level7_recovery_time = now;
	}
}

#define MAX_UHELP_TMO_MS 20000
/*
 * workqueue thread
 */
static void mxman_failure_work(struct work_struct *work)
{
	struct mxman *mxman = container_of(work, struct mxman, failure_work);
	struct srvman *srvman;
	struct scsc_mx *mx = mxman->mx;
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);
	struct fwhdr_if *fw_if = mxman->fw_wlan;
	int used = 0, r = 0;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	u16 reason;
#endif

#ifdef CONFIG_ANDROID
	wake_lock(&mxman->failure_recovery_wake_lock);
#endif
	/* Take mutex shared with syserr recovery */
	mutex_lock(&mxman->mxman_recovery_mutex);

	/* Check panic code for error promotion early on.
	 * Attempt to parse the panic record, to get the panic ID. This will
	 * only succeed for FW induced panics. Later we'll try again and dump.
	 */
	process_panic_record(mxman, false); /* check but don't dump */
	mxman_check_promote_syserr(mxman);

	SCSC_TAG_INFO(MXMAN, "This syserr level %d. Triggering moredump at level %d\n", mxman->last_syserr.level,
		      trigger_moredump_level);

	if (mxman->last_syserr.level >= trigger_moredump_level) {
		slsi_kic_system_event(slsi_kic_system_event_category_error, slsi_kic_system_events_subsystem_crashed,
				      GFP_KERNEL);

		/* Mark as level 8 as services neeed to know this has happened */
		if (mxman->last_syserr.level < MX_SYSERR_LEVEL_8) {
			mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
			SCSC_TAG_INFO(MXMAN, "Syserr level raised to 8\n");
		}
	}

	blocking_notifier_call_chain(&firmware_chain, SCSC_FW_EVENT_FAILURE, NULL);

	SCSC_TAG_INFO(MXMAN, "Complete mm_msg_start_ind_completion\n");
	complete(&mxman->mm_msg_start_ind_completion);
	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);

	if (!mxman_in_started_state(mxman) && !mxman_in_starting_state(mxman)) {
		SCSC_TAG_WARNING(MXMAN, "Not in started state: mxman->mxman_state=%d\n", mxman->mxman_state);
		mxman->panic_in_progress = false;
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		mutex_unlock(&mxman->mxman_recovery_mutex);
		return;
	}

	/**
	 * Set error on mxlog and unregister mxlog msg-handlers.
	 * mxlog ISR and kthread will ignore further messages
	 * but mxlog_thread is NOT stopped here.
	 */
	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN)) {
		mxlog_transport_set_error(scsc_mx_get_mxlog_transport(mx));
		mxlog_release(scsc_mx_get_mxlog(mx));
		/* unregister channel handler */
		mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,NULL, NULL);
		mxmgmt_transport_set_error(scsc_mx_get_mxmgmt_transport(mx));
	}

	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN)) {
		mxlog_transport_set_error(scsc_mx_get_mxlog_transport_wpan(mx));
		mxlog_release(scsc_mx_get_mxlog_wpan(mx));
		/* unregister channel handler */
		mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport_wpan(mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,NULL, NULL);
		mxmgmt_transport_set_error(scsc_mx_get_mxmgmt_transport_wpan(mx));
	}

	srvman_set_error_complete(srvman, NOT_ALLOWED_START_STOP);
	fw_if->crc_wq_stop(fw_if);
	mxman->mxman_state = mxman->mxman_next_state;
	/* Mark any single service recovery as no longer in progress */
	mxman->syserr_recovery_in_progress = false;
	mxman->last_syserr_recovery_time = 0;
	if (!mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WLAN) &&
	    !mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WPAN) &&
	    mxman->mxman_state != MXMAN_STATE_FROZEN) {
		WARN_ON(1);
		SCSC_TAG_ERR(MXMAN, "Bad state=%d\n", mxman->mxman_state);
		mxman->panic_in_progress = false;
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		mutex_unlock(&mxman->mxman_recovery_mutex);
		return;
	}

	/* Signal panic to WLAN Subsystem */
	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN)) {
		SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_WLAN\n");
		mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_WLAN, SCSC_MIF_ABS_TARGET_WLAN);
		SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_FXM_1\n");
		mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_FXM_1, SCSC_MIF_ABS_TARGET_FXM_1);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
		SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_FXM_2\n");
		mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_FXM_2, SCSC_MIF_ABS_TARGET_FXM_2);
#endif
	}

	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN)) {
		SCSC_TAG_INFO(MXMAN, "Setting MIFINTRBIT_RESERVED_PANIC_WPAN\n");
		mif->irq_bit_set(mif, MIFINTRBIT_RESERVED_PANIC_WPAN, SCSC_MIF_ABS_TARGET_WPAN); /* SCSC_MIFINTR_TARGET_WPAN */
	}

	srvman_freeze_services(srvman, &mxman->last_syserr);
	if ((mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WLAN)) ||
	    (mxman_subsys_in_failed_state(mxman, SCSC_SUBSYSTEM_WPAN))) {
		mxman->last_panic_time = local_clock();
		/* Process and dump panic record, which should be valid now even for host induced panic */
		process_panic_record(mxman, true);
		SCSC_TAG_INFO(MXMAN, "Trying to schedule coredump\n");
		SCSC_TAG_INFO(MXMAN, "scsc_release %d.%d.%d.%d.%d\n", SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION,
			      SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER);
		SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");
#ifdef CONFIG_SCSC_WLBTD
		scsc_wlbtd_get_and_print_build_type();
#endif
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		/* Scandump if requested on this panic. Must be tried after process_panic_record() */
		if (disable_recovery_handling == DISABLE_RECOVERY_HANDLING_SCANDUMP &&
		    scandump_trigger_fw_panic == mxman->scsc_panic_code) {
			SCSC_TAG_WARNING(MXMAN,
					 "WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n",
					 scandump_trigger_fw_panic);
			mxman_scan_dump_mode();
		}
#endif

		if (mxman->last_syserr.level != MX_SYSERR_LEVEL_8) {
			/* schedule system error and wait for it to finish */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
			/* Use wlan panic code as default if exists */
			if (mxman->scsc_panic_code)
				reason = mxman->scsc_panic_code;
			else
				reason = mxman->scsc_panic_code_wpan;
			scsc_log_collector_schedule_collection(SCSC_LOG_SYS_ERR, reason);
#endif
		} else {
			/* Reset level 7 loop protection */
			mxman->last_syserr_level7_recovery_time = 0;

			if (disable_auto_coredump) {
				SCSC_TAG_INFO(MXMAN,
					      "Driver automatic coredump disabled, not launching coredump helper\n");
			} else {
#ifndef CONFIG_SCSC_WLBTD
				/* schedule coredump and wait for it to finish
				 *
				 * Releasing mxman_mutex here gives way to any
				 * eventually running resume process while waiting for
				 * the usermode helper subsystem to be resurrected,
				 * since this last will be re-enabled right at the end
				 * of the resume process itself.
				 */
				mutex_unlock(&mxman->mxman_mutex);
				SCSC_TAG_INFO(MXMAN, "waiting up to %dms for usermode_helper subsystem.\n",
					      MAX_UHELP_TMO_MS);
				/* Waits for the usermode_helper subsytem to be re-enabled. */
				if (usermodehelper_read_lock_wait(msecs_to_jiffies(MAX_UHELP_TMO_MS))) {
					/**
					 * Release immediately the rwsem on usermode_helper
					 * enabled since we anyway already hold a wakelock here
					 */
					usermodehelper_read_unlock();
					/**
					 * We claim back the mxman_mutex immediately to avoid anyone
					 * shutting down the chip while we are dumping the coredump.
					 */
					mutex_lock(&mxman->mxman_mutex);
					SCSC_TAG_INFO(MXMAN, "Invoking coredump helper\n");
					slsi_kic_system_event(slsi_kic_system_event_category_recovery,
							      slsi_kic_system_events_coredump_in_progress, GFP_KERNEL);

					r = coredump_helper();
#else
				/* we can safely call call_wlbtd as we are
					 * in workqueue context
					 */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
				/* Use wlan panic code as default if exists */
				if (mxman->scsc_panic_code)
					reason = mxman->scsc_panic_code;
				else
					reason = mxman->scsc_panic_code_wpan;
				scsc_log_collector_schedule_collection(SCSC_LOG_FW_PANIC, reason);
#else
				r = call_wlbtd(SCSC_SCRIPT_MOREDUMP);
#endif
#endif
					if (r >= 0) {
						slsi_kic_system_event(slsi_kic_system_event_category_recovery,
								      slsi_kic_system_events_coredump_done, GFP_KERNEL);
					}

					used = snprintf(panic_record_dump, PANIC_RECORD_DUMP_BUFFER_SZ,
							"RF HW Ver: 0x%X\n", mxman->rf_hw_ver);
					used += snprintf(panic_record_dump + used, PANIC_RECORD_DUMP_BUFFER_SZ - used,
							 "SCSC Panic Code:: 0x%X\n", mxman->scsc_panic_code);
					used += snprintf(panic_record_dump + used, PANIC_RECORD_DUMP_BUFFER_SZ - used,
							 "SCSC Last Panic Time:: %lld\n", mxman->last_panic_time);
					panic_record_dump_buffer("r4", mxman->last_panic_rec_r,
								 mxman->last_panic_rec_sz, panic_record_dump + used,
								 PANIC_RECORD_DUMP_BUFFER_SZ - used);

					/* Print the host code/reason again so it's near the FW panic
					 * record in the kernel log
					 */
					print_panic_code(mxman);
					SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n",
						      mxman->failure_reason[0] ? mxman->failure_reason : "<null>");

					blocking_notifier_call_chain(&firmware_chain, SCSC_FW_EVENT_MOREDUMP_COMPLETE,
								     &panic_record_dump);
#ifndef CONFIG_SCSC_WLBTD
				} else {
					SCSC_TAG_INFO(MXMAN,
						      "timed out waiting for usermode_helper. Skipping coredump.\n");
					mutex_lock(&mxman->mxman_mutex);
				}
#endif
			}
		}

		if (is_bug_on_enabled(mx)) {
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
			/* Scandump if requested on this panic. Must be tried after process_panic_record() */
			SCSC_TAG_WARNING(MXMAN,
				 "WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n",
				 mxman->scsc_panic_code);
			mxman_scan_dump_mode();
#endif
			SCSC_TAG_ERR(MX_FILE, "Deliberately panic the kernel due to WLBT firmware failure!\n");
			SCSC_TAG_ERR(MX_FILE, "calling BUG_ON(1)\n");
			BUG_ON(1);
		}
		/* Clean up the MIF following error handling */
		if (mif->mif_cleanup && mxman_recovery_disabled())
			mif->mif_cleanup(mif);
	}

	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");

	if (!mxman_recovery_disabled())
		/* Allow the services to close and block any spurios start
		 * from services not respecting the .remove .probe callbacks */
		srvman_set_error(srvman, NOT_ALLOWED_START);
	mutex_unlock(&mxman->mxman_mutex);
	if (!mxman_recovery_disabled()) {
		SCSC_TAG_INFO(MXMAN, "Calling srvman_unfreeze_services\n");
		srvman_unfreeze_services(srvman, &mxman->last_syserr);
		if (scsc_mx_module_reset() < 0)
			SCSC_TAG_INFO(MXMAN, "failed to call scsc_mx_module_reset\n");
		/* At this point services should be probed and mxman status
		 * in stopped state to get new starts */
		srvman_set_error(srvman, ALLOWED_START_STOP);
		atomic_inc(&mxman->recovery_count);
	}
	mxman->panic_in_progress = false;

	/**
	 * If recovery is disabled and an scsc_mx_service_open has been hold up,
	 * release it, rather than wait for the recovery_completion to timeout.
	 */
	if (mxman_recovery_disabled())
		complete(&mxman->recovery_completion);

	/* Safe to allow syserr recovery thread to run */
	mutex_unlock(&mxman->mxman_recovery_mutex);

#ifdef CONFIG_ANDROID
	wake_unlock(&mxman->failure_recovery_wake_lock);
#endif
}

static void failure_wq_init(struct mxman *mxman)
{
	mxman->failure_wq = create_singlethread_workqueue("failure_wq");
	INIT_WORK(&mxman->failure_work, mxman_failure_work);
}

static void failure_wq_stop(struct mxman *mxman)
{
	cancel_work_sync(&mxman->failure_work);
	flush_workqueue(mxman->failure_wq);
}

static void failure_wq_deinit(struct mxman *mxman)
{
	failure_wq_stop(mxman);
	destroy_workqueue(mxman->failure_wq);
}

static void failure_wq_start(struct mxman *mxman)
{
	if (disable_error_handling)
		SCSC_TAG_INFO(MXMAN, "error handling disabled\n");
	else
		queue_work(mxman->failure_wq, &mxman->failure_work);
	mxman->panic_in_progress = true;
}

/*
 * workqueue thread
 */
static void mxman_syserr_recovery_work(struct work_struct *work)
{
	struct mxman *mxman = container_of(work, struct mxman, syserr_recovery_work);
	struct srvman *srvman;

#ifdef CONFIG_ANDROID
	wake_lock(&mxman->syserr_recovery_wake_lock);
#endif
	if (!mutex_trylock(&mxman->mxman_recovery_mutex)) {
		SCSC_TAG_WARNING(MXMAN, "Syserr during full reset - ignored\n");
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->syserr_recovery_wake_lock);
#endif
		return;
	}

	mutex_lock(&mxman->mxman_mutex);

	if (!mxman_in_started_state(mxman) && !(mxman_in_starting_state(mxman))) {
		SCSC_TAG_WARNING(MXMAN, "Syserr reset ignored: mxman->mxman_state=%d\n", mxman->mxman_state);
#ifdef CONFIG_ANDROID
		wake_unlock(&mxman->syserr_recovery_wake_lock);
#endif
		mutex_unlock(&mxman->mxman_mutex);
		return;
	}

	srvman = scsc_mx_get_srvman(mxman->mx);

	srvman_freeze_sub_system(srvman, &mxman->last_syserr);

#ifdef CONFIG_SCSC_WLBTD
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Wait for log generation if not finished */
	SCSC_TAG_INFO(MXMAN, "Wait for syserr sable logging\n");
	scsc_wlbtd_wait_for_sable_logging();
	SCSC_TAG_INFO(MXMAN, "Syserr sable logging complete\n");
#endif
#endif

	srvman_unfreeze_sub_system(srvman, &mxman->last_syserr);

#ifdef CONFIG_ANDROID
	wake_unlock(&mxman->syserr_recovery_wake_lock);
#endif
	mutex_unlock(&mxman->mxman_recovery_mutex);
	mutex_unlock(&mxman->mxman_mutex);
}

static void syserr_recovery_wq_init(struct mxman *mxman)
{
	mxman->syserr_recovery_wq = create_singlethread_workqueue("syserr_recovery_wq");
	INIT_WORK(&mxman->syserr_recovery_work, mxman_syserr_recovery_work);
}

static void syserr_recovery_wq_stop(struct mxman *mxman)
{
	cancel_work_sync(&mxman->syserr_recovery_work);
	flush_workqueue(mxman->syserr_recovery_wq);
}

static void syserr_recovery_wq_deinit(struct mxman *mxman)
{
	syserr_recovery_wq_stop(mxman);
	destroy_workqueue(mxman->syserr_recovery_wq);
}

static void syserr_recovery_wq_start(struct mxman *mxman)
{
	queue_work(mxman->syserr_recovery_wq, &mxman->syserr_recovery_work);
}

#ifdef CONFIG_SCSC_WLBTD
static void wlbtd_work_func(struct work_struct *work)
{
	/* require sleep-able workqueue to run successfully */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Collect mxlogger logs */
	/* Extend to scsc_log_collector_collect() if required */
#else
	call_wlbtd(SCSC_SCRIPT_LOGGER_DUMP);
#endif
}

static void wlbtd_wq_init(struct mxman *mx)
{
	INIT_WORK(&wlbtd_work, wlbtd_work_func);
}

static void wlbtd_wq_deinit(struct mxman *mx)
{
	/* flush and block until work is complete */
	flush_work(&wlbtd_work);
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
int mxman_sysevent_desc_init(struct mxman *mxman)
{
	int ret = 0;
	struct device *dev;
	struct scsc_mif_abs *mif;

	mif = scsc_mx_get_mif_abs(mxman->mx);
	dev = mif->get_mif_device(mif);

	mxman->sysevent_dev = NULL;
	mxman->sysevent_desc.name = "wlbt";
	mxman->sysevent_desc.owner = THIS_MODULE;
	mxman->sysevent_desc.powerup = wlbt_sysevent_powerup;
	mxman->sysevent_desc.shutdown = wlbt_sysevent_shutdown;
	mxman->sysevent_desc.ramdump = wlbt_sysevent_ramdump;
	mxman->sysevent_desc.crash_shutdown = wlbt_sysevent_crash_shutdown;
	mxman->sysevent_desc.dev = dev;
	mxman->sysevent_dev = sysevent_register(&mxman->sysevent_desc);
	if (IS_ERR(mxman->sysevent_dev)) {
		ret = PTR_ERR(mxman->sysevent_dev);
		SCSC_TAG_WARNING(MXMAN, "sysevent_register failed :%d\n", ret);
	} else
		SCSC_TAG_INFO(MXMAN, "sysevent_register success\n");

	return ret;
}
#endif

/*
 * Check for matching f/w and h/w
 *
 * Returns	0:  f/w and h/w match
 *		1:  f/w and h/w mismatch, try the next config
 *		-ve fatal error
 */
static int mxman_hw_ver_check(struct mxman *mxman)
{
	if (mx140_file_supported_hw(mxman->mx, mxman->rf_hw_ver))
		return 0;
	else
		return 1;
}

/*
 * Select the f/w version to load next
 */
static int mxman_select_next_fw(struct mxman *mxman)
{
	return mx140_file_select_fw(mxman->mx, mxman->rf_hw_ver);
}

static void mxman_close_on_start_failure(struct mxman *mxman, enum scsc_subsystem sub)
{
	SCSC_TAG_INFO(MXMAN, "Cleanup subsys:%s\n", sub == SCSC_SUBSYSTEM_WPAN ? "WPAN" : "WLAN");
	mxman_stop(mxman, sub);
	if (mxman->users == 0 && mxman->users_wpan == 0) {
		mxman_reset_chip(mxman);
	}
}

static int __mxman_open(struct mxman *mxman, enum scsc_subsystem sub, void *data, size_t data_sz)
{
	int ret;

	SCSC_TAG_INFO(MXMAN, "Number of wlan_users=%d wpan_users=%d Maxwell state=%d\n", mxman->users, mxman->users_wpan, mxman->mxman_state);
	if (mxman->users == 0 && mxman->users_wpan == 0 && mxman->mxman_state == MXMAN_STATE_STARTING)
	{
		/*
		 * If chip is off, memory will be allocated and FW will be loaded to shared dram.
		 * PMU which is a shared resource will also be initialized.
		 * A subystem is required to tell pmu which CPU to boot.
		*/
		ret = mxman_start_boot(mxman, sub);
		if (ret) {
			SCSC_TAG_ERR(MXMAN, "Error mxman_res_pmu_boot\n");
			goto error;
		}

		ret = mxman_res_init_common(mxman);
		if (ret) {
			SCSC_TAG_ERR(MXMAN, "Error mxman_res_init_common\n");
			goto error;
		}

#if defined(CONFIG_WLBT_DCXO_TUNE)
		if (set_dcxo_state == DCXO_CONFIG_NONE) {
			mxman_set_default_dcxo_caldata(mxman);
		}
#endif
	}

	/* Print information about any active services on any subsystem */
	if (mxman->users || mxman->users_wpan) {
		SCSC_TAG_INFO(MXMAN, "Active WLAN Subsystem services: users_wlan=%d Active WPAN Subsystem services: users_wpan=%d\n", mxman->users, mxman->users_wpan);
		mxman_print_versions(mxman);
		if (sub == SCSC_SUBSYSTEM_WPAN && mxman->wpan_present == false) {
			SCSC_TAG_ERR(MXMAN, "WPAN is not present in the FW image\n");
			return -EIO;
		}
	}

	reinit_completion(&mxman->mm_msg_start_ind_completion);

	if ((mxman->users == 0 && sub == SCSC_SUBSYSTEM_WLAN) ||
		(mxman->users_wpan == 0 && sub == SCSC_SUBSYSTEM_WPAN))	{
		/* Start subsystem (sub) */
		ret = mxman_start(mxman, sub, data, data_sz);
		if (ret) {
			SCSC_TAG_ERR(MXMAN, "maxwell_manager_start() failed ret=%d users_wlan=%d users_wpan=%d\n",
				     ret, mxman->users, mxman->users_wpan);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
			if (kernel_crash_on_service_fail) {
				SCSC_TAG_WARNING(MXMAN, "WLBT FW failure - halt kernel 0x%x!\n", kernel_crash_on_service_fail);
				mxman_scan_dump_mode();
			}
#endif
			mxman_close_on_start_failure(mxman, sub);
			return ret;
		}

		ret = wait_for_mm_msg_start_ind(mxman);
		if (ret) {
			SCSC_TAG_ERR(MXMAN, "wait_for_MM_START_IND() for subsfailed: r=%d users_wlan=%d users_wpan=%d\n",
				     ret, mxman->users, mxman->users_wpan);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
			if (kernel_crash_on_service_fail) {
				SCSC_TAG_WARNING(MXMAN, "WLBT FW failure - halt kernel 0x%x!\n", kernel_crash_on_service_fail);
				mxman_scan_dump_mode();
			}
#endif
			mxman_close_on_start_failure(mxman, sub);
			return ret;
		}

		/* Call post start conditions */
		mxman_res_post_init_subsystem(mxman, sub);
	}

	/*
	 * Increment the user count for each subsystem services and change the state if an
	 * existing subsystem has booted
	 */
	if (mxman->users == 0 && mxman->users_wpan == 0) {
		if (sub == SCSC_SUBSYSTEM_WPAN) {
			mxman->mxman_state = MXMAN_STATE_STARTED_WPAN;
			mxman->users_wpan++;
		} else if (sub == SCSC_SUBSYSTEM_WLAN) {
			mxman->mxman_state = MXMAN_STATE_STARTED_WLAN;
			mxman->users++;
		}
	} else {
		switch(mxman->mxman_state) {
		case MXMAN_STATE_STARTED_WLAN:
			if (sub == SCSC_SUBSYSTEM_WLAN) {
				mxman->users++;
			}
			else if (sub == SCSC_SUBSYSTEM_WPAN) {
				mxman->users_wpan++;
				mxman->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
			}
			break;
		case MXMAN_STATE_STARTED_WPAN:
			if (sub == SCSC_SUBSYSTEM_WLAN) {
				mxman->users++;
				mxman->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
			}
			else if (sub == SCSC_SUBSYSTEM_WPAN) {
				mxman->users_wpan++;
			}
			break;
		case MXMAN_STATE_STARTED_WLAN_WPAN:
			if (sub == SCSC_SUBSYSTEM_WLAN)
				mxman->users++;
			else if (sub == SCSC_SUBSYSTEM_WPAN)
				mxman->users_wpan++;
			break;
		default:
			SCSC_TAG_ERR(MXMAN, "Encountered invalid mxman state in __mxman_open %d\n", mxman->mxman_state);
		};
	}

	SCSC_TAG_INFO(MXMAN, "Users Wlan=%d Users Wpan=%d\n", mxman->users, mxman->users_wpan);
	return 0;
error:
	return -EIO;
}

int mxman_open(struct mxman *mxman, enum scsc_subsystem sub, void *data, size_t data_sz)
{
	struct srvman *srvman;
	int ret = 0;
	int try = 0;
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);

	mutex_lock(&mxman->mxman_mutex);

	SCSC_TAG_INFO(MXMAN, "mxman open %s subsystem\n", (sub == SCSC_SUBSYSTEM_WPAN) ? "WPAN" : "WLAN");

	mx140_basedir_file(mxman->mx);

	if (mxman->scsc_panic_code) {
		SCSC_TAG_INFO(MXMAN, "Previously recorded crash panic code: scsc_panic_code=0x%x\n",
			      mxman->scsc_panic_code);
		SCSC_TAG_INFO(MXMAN, "Reason: '%s'\n", mxman->failure_reason[0] ? mxman->failure_reason : "<null>");
		print_panic_code(mxman);
	}

	SCSC_TAG_INFO(MXMAN, "Auto-recovery: %s\n", mxman_recovery_disabled() ? "off" : "on");
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && srvman->error) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return -EINVAL;
	}

	SCSC_TAG_INFO(MXMAN, "Mxman Start Current state %d wlan_users=%d wpan_users=%d\n", mxman->mxman_state, mxman->users, mxman->users_wpan);
	switch (mxman->mxman_state) {
	case MXMAN_STATE_STOPPED:
		mxman->mxman_state = MXMAN_STATE_STARTING;
		break;
	case MXMAN_STATE_STARTED_WLAN:
		if (sub == SCSC_SUBSYSTEM_WLAN) {
			SCSC_TAG_INFO(MXMAN, "Subsystem WLAN exists so new service added on the subsystem\n");
		}
		else {
			mxman->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
		}
		break;
	case MXMAN_STATE_STARTED_WPAN:
		if (sub == SCSC_SUBSYSTEM_WPAN) {
			SCSC_TAG_INFO(MXMAN, "Subsystem WPAN exists so new service added on the subsystem \n");
		}
		else {
			mxman->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
		}
		break;
	case MXMAN_STATE_STARTED_WLAN_WPAN:
			SCSC_TAG_INFO(MXMAN, "Subsystem WLAN/WPAN exists so new service added on the subsystem %d\n", sub);
			break;	
	case MXMAN_STATE_STARTING:
	default:
		/* this state is an anomaly */
		SCSC_TAG_INFO(MXMAN, "Mxman in invalid state. Ignoring open request\n");
		ret = -EINVAL;
		goto error;
		break;
	}

	/* If we reach here we have to either boot the chip or turn on a
	 * subystem (via PMU)*/
	for (try = 0; try < 2; try ++) {
		/* Boot WLBT. This will determine the h/w version */
		ret = __mxman_open(mxman, sub, data, data_sz);
		if (ret) {
			if (sub == SCSC_SUBSYSTEM_WPAN && !mxman->users_wpan) {
				if (mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN)
					mxman->mxman_state = MXMAN_STATE_STARTED_WLAN;
				else
					mxman->mxman_state = MXMAN_STATE_STOPPED;
			} else if (sub == SCSC_SUBSYSTEM_WLAN && !mxman->users) {
				if (mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN)
					mxman->mxman_state = MXMAN_STATE_STARTED_WPAN;
				else
					mxman->mxman_state = MXMAN_STATE_STOPPED;
			}
			mutex_unlock(&mxman->mxman_mutex);
			return ret;
		}

		/* Check the h/w and f/w versions are compatible */
		ret = mxman_hw_ver_check(mxman);
		if (ret > 0) {
			/* Not compatible, so try next f/w */
			SCSC_TAG_INFO(MXMAN, "Incompatible h/w 0x%04x vs f/w, close and try next\n", mxman->rf_hw_ver);

			/* Temporarily return USBPLL owner to AP to keep USB alive */
			if (mif->mif_cleanup)
				mif->mif_cleanup(mif);

			/* Stop WLBT */
			mxman_close(mxman, sub);

			/* Select the new f/w for this hw ver */
			mxman_select_next_fw(mxman);
		} else
			break; /* Running or given up */
	}

#if IS_ENABLED(CONFIG_SCSC_FM)
	/* If we have stored FM radio parameters, deliver them to FW now */
	if (ret == 0 && mxman->fm_params_pending) {
		SCSC_TAG_INFO(MXMAN, "Send pending FM params\n");
		mxman_fm_set_params(&mxman->fm_params);
	}
#endif

error:
	SCSC_TAG_INFO(MXMAN, "Exit state %d users_wlan=%d users_wpan=%d\n", mxman->mxman_state, mxman->users, mxman->users_wpan);
	mutex_unlock(&mxman->mxman_mutex);
	return ret;
}

int mxman_stop(struct mxman *mxman, enum scsc_subsystem sub)
{
	int ret;
	/*
	 * Ask the subsystem to stop (MM_STOP_REQ), and wait
	 * for response (MM_STOP_RSP).
	 */
	SCSC_TAG_INFO(MXMAN, "Sending mxman stop %s subsystem\n", (sub == SCSC_SUBSYSTEM_WPAN) ? "WPAN" : "WLAN");
	ret = send_mm_msg_stop_blocking(mxman, sub);
	if (ret)
		SCSC_TAG_ERR(MXMAN, "send_mm_msg_stop_blocking failed: ret=%d\n", ret);

	mxman_res_deinit_subsystem(mxman, sub);
	mxman_res_pmu_reset(mxman, sub);
	return 0;
}

void mxman_close(struct mxman *mxman, enum scsc_subsystem sub)
{
	struct srvman *srvman;

	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && !srvman_allow_close(srvman)) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return;
	}

	SCSC_TAG_INFO(MXMAN, "mxman close sub = %d\n", sub);
	SCSC_TAG_INFO(MXMAN, "mxman close %s subsystem\n", sub ? "WPAN" : "WLAN");
	SCSC_TAG_INFO(MXMAN, "Mxman close current state %d users_wlan=%d users_wpan=%d\n", mxman->mxman_state, mxman->users, mxman->users_wpan);
	switch (mxman->mxman_state) {
	case MXMAN_STATE_STARTED_WLAN:
		if (sub == SCSC_SUBSYSTEM_WPAN) {
			SCSC_TAG_ERR(MXMAN, "Invalid mxman state=%d\n", mxman->mxman_state);
			goto err_wpan_not_exists;
		} else if (sub == SCSC_SUBSYSTEM_WLAN) {
			if (--(mxman->users) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_STOPPED;
			}
		}
		break;
	case MXMAN_STATE_STARTED_WPAN:
		if (sub == SCSC_SUBSYSTEM_WLAN) {
			SCSC_TAG_ERR(MXMAN, "Invalid mxman state=%d\n", mxman->mxman_state);
			goto err_wlan_not_exists;
		} else if (sub == SCSC_SUBSYSTEM_WPAN) {
			if (--(mxman->users_wpan) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_STOPPED;
			}
		}
		break;
	case MXMAN_STATE_STARTED_WLAN_WPAN:
		if (sub == SCSC_SUBSYSTEM_WLAN) {
			if (--(mxman->users) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_STARTED_WPAN;
			}
		} else if (sub == SCSC_SUBSYSTEM_WPAN) {
			if (--(mxman->users_wpan) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_STARTED_WLAN;
			}
		}
		break;
	case MXMAN_STATE_FAILED_PMU:
		mxman->mxman_state = MXMAN_STATE_STOPPED;
		break;
	case MXMAN_STATE_FAILED_WLAN:
		if (--(mxman->users) == 0) {
			mxman_stop(mxman, sub);
			mxman->mxman_state = MXMAN_STATE_STOPPED;
		}
		break;
	case MXMAN_STATE_FAILED_WPAN:
		if (--(mxman->users_wpan) == 0) {
			mxman_stop(mxman, sub);
			mxman->mxman_state = MXMAN_STATE_STOPPED;
		}
		break;
	case MXMAN_STATE_FAILED_WLAN_WPAN:
		if (sub == SCSC_SUBSYSTEM_WLAN) {
			if (--(mxman->users) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_FAILED_WPAN;
			}
		} else if (sub == SCSC_SUBSYSTEM_WPAN) {
			if (--(mxman->users_wpan) == 0) {
				mxman_stop(mxman, sub);
				mxman->mxman_state = MXMAN_STATE_FAILED_WLAN;
			}
		}
		break;
	default:
		/* this state is an anomaly */
		SCSC_TAG_ERR(MXMAN, "Invalid mxman state=%d\n", mxman->mxman_state);
		goto error;
		break;
	}

	if (mxman->users || mxman->users_wpan) {
		SCSC_TAG_INFO(MXMAN, "Current number of users_wlan=%d users_wpan=%d\n", mxman->users, mxman->users_wpan);
		mutex_unlock(&mxman->mxman_mutex);
		return;
	}

	/* For now reset chip only when we are shutting down last service on last active subsystem */
	mxman_reset_chip(mxman);
err_wpan_not_exists:
err_wlan_not_exists:
error:
	mutex_unlock(&mxman->mxman_mutex);
	return;
}

void mxman_syserr(struct mxman *mxman, struct mx_syserr_decode *syserr)
{
	mxman->syserr_recovery_in_progress = true;

	mxman->last_syserr.subsys = syserr->subsys;
	mxman->last_syserr.level = syserr->level;
	mxman->last_syserr.type = syserr->type;
	mxman->last_syserr.subcode = syserr->subcode;

	syserr_recovery_wq_start(mxman);
}


static __always_inline void mxman_promote_error(struct mxman *mxman, enum scsc_subsystem *sub)
{
	/* TODO: when single recovery is enabled, return immediately
	 * return;
	 * */

	/* If full recovery is enabled we may need to promote the subsystem to
	full WLAN_WPAN failure if WLAN and WPAN are ON */
	if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN_WPAN)) {
		switch (*sub) {
		case SCSC_SUBSYSTEM_WLAN:
		case SCSC_SUBSYSTEM_WPAN:
			*sub = SCSC_SUBSYSTEM_WLAN_WPAN;
			break;
		default:
			break;
		};
	}
	return;
}

void mxman_fail(struct mxman *mxman, u16 failure_source, const char *reason, enum scsc_subsystem sub)
{
	SCSC_TAG_WARNING(MXMAN, "WLBT FW failure 0x%x subsystem id: %d\n", failure_source, sub);

	/* For FW failure, scsc_panic_code is not set up fully until process_panic_record() checks it */
	if (disable_recovery_handling == DISABLE_RECOVERY_HANDLING_SCANDUMP) {
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		if (scandump_trigger_fw_panic == 0) {
			SCSC_TAG_WARNING(MXMAN, "WLBT FW failure - halt Exynos kernel for scandump on code 0x%x!\n",
					 scandump_trigger_fw_panic);
			mxman_scan_dump_mode();
		}
#else
		/* Support not present, fallback to vanilla moredump and stop WLBT */
		disable_recovery_handling = 1;
		SCSC_TAG_WARNING(MXMAN, "WLBT FW failure - scandump requested but not supported in kernel\n");
#endif
	}

	if(mxman->panic_in_progress) {
		SCSC_TAG_WARNING(MXMAN, "Last panic in progress. Reject new panic\n");
		return;
	}

	/* The STARTING state allows a crash during firmware boot to be handled */
	if (mxman_in_starting_state(mxman) || mxman_in_started_state(mxman)) {

		mxman_promote_error(mxman, &sub);

		switch (sub) {
		case SCSC_SUBSYSTEM_WLAN:
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
			mxman->scsc_panic_code = failure_source;
			mxman->scsc_panic_code_wpan = 0;
			break;
		case SCSC_SUBSYSTEM_WPAN:
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WPAN;
			mxman->scsc_panic_code = 0;
			mxman->scsc_panic_code_wpan = failure_source;
			break;
		case SCSC_SUBSYSTEM_PMU:
			if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN))
				mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
			else if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN))
				mxman->mxman_next_state = MXMAN_STATE_FAILED_WPAN;
			else if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN_WPAN))
				mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN_WPAN;
			else
				mxman->mxman_next_state = MXMAN_STATE_FAILED_PMU;
			mxman->scsc_panic_code = failure_source;
			mxman->scsc_panic_code_wpan = failure_source;
			break;
		case SCSC_SUBSYSTEM_WLAN_WPAN:
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN_WPAN;
			mxman->scsc_panic_code = failure_source;
			mxman->scsc_panic_code_wpan = failure_source;
			break;
		default:
			SCSC_TAG_ERR(MXMAN, "Received invalid subsystem %d value\n", sub);
			return;
		};

		strlcpy(mxman->failure_reason, reason, sizeof(mxman->failure_reason));
		/* If recovery is disabled, don't let it be
		 * re-enabled from now on. Device must reboot
		 */
		if (mxman_recovery_disabled())
			disable_recovery_until_reboot = true;

		/* Populate syserr info with panic equivalent or best we can */
		mxman->last_syserr.subsys = failure_source >> SYSERR_SUB_SYSTEM_POSN;
		mxman->last_syserr.level = MX_SYSERR_LEVEL_7;
		mxman->last_syserr.type = failure_source;
		mxman->last_syserr.subcode = failure_source;
		/* mark the subsystem that triggered the panic */
		mxman->scsc_panic_sub = sub;
		atomic_inc(&mxman->cancel_resume);
		failure_wq_start(mxman);
	} else {
		SCSC_TAG_WARNING(MXMAN, "Not in MXMAN_STATE_STARTED state, ignore (state %d)\n", mxman->mxman_state);
	}
}

void mxman_fail_level8(struct mxman *mxman, u16 failure_source, const char *reason, enum scsc_subsystem sub)
{
	SCSC_TAG_WARNING(MXMAN, "WLBT FW level 8 failure 0x%0x\n", failure_source);

	if(mxman->panic_in_progress) {
		SCSC_TAG_WARNING(MXMAN, "Last panic in progress. Reject new trigger\n");
		return;
	}

	/* The STARTING state allows a crash during firmware boot to be handled */
	if (mxman_in_starting_state(mxman) || mxman_in_started_state(mxman)) {

		mxman_promote_error(mxman, &sub);

		switch (sub) {
		case SCSC_SUBSYSTEM_WLAN:
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
			mxman->scsc_panic_code = failure_source;
			mxman->scsc_panic_code_wpan = 0;
			break;
		case SCSC_SUBSYSTEM_WPAN:
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WPAN;
			mxman->scsc_panic_code = 0;
			mxman->scsc_panic_code_wpan = failure_source;
			break;
		case SCSC_SUBSYSTEM_PMU:
			if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN))
				mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
			else if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN))
				mxman->mxman_next_state = MXMAN_STATE_FAILED_WPAN;
			else if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN_WPAN))
				mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN_WPAN;
			else
				mxman->mxman_next_state = MXMAN_STATE_FAILED_PMU;
			mxman->scsc_panic_code = failure_source;
			mxman->scsc_panic_code_wpan = failure_source;
			break;
		case SCSC_SUBSYSTEM_WLAN_WPAN:
			mxman->mxman_next_state = MXMAN_STATE_FAILED_WLAN_WPAN;
			mxman->scsc_panic_code = failure_source;
			mxman->scsc_panic_code_wpan = failure_source;
			break;
		default:
			SCSC_TAG_ERR(MXMAN, "Received invalid subsystem %d value\n", sub);
			return;
		};

		mxman->scsc_panic_code = failure_source;
		strlcpy(mxman->failure_reason, reason, sizeof(mxman->failure_reason));
		/* If recovery is disabled, don't let it be
		 * re-enabled from now on. Device must reboot
		 */
		if (mxman_recovery_disabled())
			disable_recovery_until_reboot = true;

		/* Populate syserr info with panic equivalent or best we can */
		mxman->last_syserr.subsys = failure_source >> SYSERR_SUB_SYSTEM_POSN;
		mxman->last_syserr.level = MX_SYSERR_LEVEL_8;
		mxman->last_syserr.type = failure_source;
		mxman->last_syserr.subcode = failure_source;

		/* mark the subsystem that triggered the panic */
		mxman->scsc_panic_sub = sub;
		failure_wq_start(mxman);
	} else {
		SCSC_TAG_WARNING(MXMAN, "Not in MXMAN_STATE_STARTED state, ignore (state %d)\n", mxman->mxman_state);
	}
}

void mxman_freeze(struct mxman *mxman)
{
	SCSC_TAG_WARNING(MXMAN, "WLBT FW frozen\n");

	if(mxman->panic_in_progress) {
		SCSC_TAG_WARNING(MXMAN, "Last panic in progress. Reject freeze\n");
		return;
	}

	if (mxman_in_started_state(mxman)) {
		mxman->mxman_next_state = MXMAN_STATE_FROZEN;
		failure_wq_start(mxman);
	} else {
		SCSC_TAG_WARNING(MXMAN, "Not in MXMAN_STATE_STARTED state, ignore (state %d)\n", mxman->mxman_state);
	}
}

void mxman_init(struct mxman *mxman, struct scsc_mx *mx)
{
	mxman->mx = mx;
	mxman->suspended = 0;
#if IS_ENABLED(CONFIG_SCSC_FM)
	mxman->on_halt_ldos_on = 0;
	mxman->fm_params_pending = 0;
#endif
	//fw_crc_wq_init(mxman);
	failure_wq_init(mxman);
	syserr_recovery_wq_init(mxman);
#ifdef CONFIG_SCSC_WLBTD
	wlbtd_wq_init(mxman);
#endif
	mutex_init(&mxman->mxman_mutex);
	mutex_init(&mxman->mxman_recovery_mutex);
	init_completion(&mxman->recovery_completion);
	init_completion(&mxman->mm_msg_start_ind_completion);
	init_completion(&mxman->mm_msg_halt_rsp_completion);
#ifdef CONFIG_ANDROID
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	wake_lock_init(&mxman->failure_recovery_wake_lock, WAKE_LOCK_SUSPEND, "mxman_recovery");
	wake_lock_init(&mxman->syserr_recovery_wake_lock, WAKE_LOCK_SUSPEND, "mxman_syserr_recovery");
#else
	wake_lock_init(NULL, &mxman->failure_recovery_wake_lock.ws, "mxman_recovery");
	wake_lock_init(NULL, &mxman->syserr_recovery_wake_lock.ws, "mxman_syserr_recovery");
#endif
#endif
	mxman->last_syserr_level7_recovery_time = 0;

	atomic_set(&mxman->cancel_resume, 0);

	mxman->syserr_recovery_in_progress = false;
	mxman->last_syserr_recovery_time = 0;
	mxman->panic_in_progress = false;

	/* set the initial state */
	mxman->mxman_state = MXMAN_STATE_STOPPED;
	(void)snprintf(mxman->fw_build_id, sizeof(mxman->fw_build_id), FW_BUILD_ID_UNKNOWN);
	memcpy(saved_fw_build_id, mxman->fw_build_id, sizeof(saved_fw_build_id));
	(void)snprintf(mxman->fw_ttid, sizeof(mxman->fw_ttid), "unknown");
	mxproc_create_info_proc_dir(&mxman->mxproc, mxman);
	active_mxman = mxman;

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	if (!mxman_sysevent_desc_init(mxman)) {
		mxman->sysevent_nb.notifier_call = wlbt_sysevent_notifier_cb;
		sysevent_notif_register_notifier(mxman->sysevent_desc.name, &mxman->sysevent_nb);
	}
#endif

#if defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 9
	mxman_create_sysfs_memdump();
#if defined(CONFIG_WLBT_DCXO_TUNE)
	mxman_create_sysfs_wlbt_dcxo_caldata();
#endif
#endif
	scsc_lerna_init();

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	scsc_logring_register_mx_cb(&mx_logring);
#endif
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_register_mx_cb(&mx_cb);
#endif
}

void mxman_deinit(struct mxman *mxman)
{
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	scsc_logring_unregister_mx_cb(&mx_logring);
#endif
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_unregister_mx_cb(&mx_cb);
#endif
	scsc_lerna_deinit();
#if defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 9
	mxman_destroy_sysfs_memdump();
#if defined(CONFIG_WLBT_DCXO_TUNE)
	mxman_destroy_sysfs_wlbt_dcxo_caldata();
#endif
#endif
	active_mxman = NULL;
	mxproc_remove_info_proc_dir(&mxman->mxproc);
#if 0
	fw_crc_wq_deinit(mxman);
#else
	whdr_destroy(mxman->fw_wlan);
	bhdr_destroy(mxman->fw_wpan);
#endif
	failure_wq_deinit(mxman);
	syserr_recovery_wq_deinit(mxman);
#ifdef CONFIG_SCSC_WLBTD
	wlbtd_wq_deinit(mxman);
#endif
#ifdef CONFIG_ANDROID
	wake_lock_destroy(&mxman->failure_recovery_wake_lock);
	wake_lock_destroy(&mxman->syserr_recovery_wake_lock);
#endif
	mutex_destroy(&mxman->mxman_recovery_mutex);
	mutex_destroy(&mxman->mxman_mutex);
}

int mxman_force_panic(struct mxman *mxman, enum scsc_subsystem sub)
{
	struct srvman *srvman;
	struct ma_msg_packet message = { .ma_msg = MM_FORCE_PANIC };

	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && srvman->error) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return -EINVAL;
	}

	if ((sub == SCSC_SUBSYSTEM_WLAN_WPAN) && (mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN)) {
		SCSC_TAG_INFO(MXMAN, "Both WLAN and WPAN subsystems active\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
				      &message, sizeof(message));

		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport_wpan(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
				      &message, sizeof(message));

		mutex_unlock(&mxman->mxman_mutex);
		return 0;
	}

	if ((sub == SCSC_SUBSYSTEM_WLAN || sub == SCSC_SUBSYSTEM_WLAN_WPAN) && mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN)) {
		SCSC_TAG_INFO(MXMAN, "WLAN subsystem active\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
				      &message, sizeof(message));

		mutex_unlock(&mxman->mxman_mutex);
		return 0;
	}

	if ((sub == SCSC_SUBSYSTEM_WPAN || sub == SCSC_SUBSYSTEM_WLAN_WPAN) && mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN)) { 		 SCSC_TAG_INFO(MXMAN, "WPAN subsystem active\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport_wpan(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
				      &message, sizeof(message));
		mutex_unlock(&mxman->mxman_mutex);
		return 0;
	}

	mutex_unlock(&mxman->mxman_mutex);
	return -EINVAL;
}

int mxman_suspend(struct mxman *mxman)
{
	struct srvman *srvman;
	struct ma_msg_packet message = { .ma_msg = MM_HOST_SUSPEND };
	int ret;

	SCSC_TAG_INFO(MXMAN, "\n");

	atomic_set(&mxman->cancel_resume, 0);
	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);

	if (srvman && srvman->error) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return 0;
	}

	/* Call Service suspend callbacks */
	if (srvman) {
		ret = srvman_suspend_services(srvman);
	} else {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "srvman not found - ignore\n");
		return 0;
	}

	if (ret) {
		mutex_unlock(&mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Service Suspend canceled - ignore %d\n", ret);
		return ret;
	}

	if (mxman_in_started_state(mxman)) {
		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN)) {
			SCSC_TAG_INFO(MXMAN, "MM_HOST_SUSPEND WLAN Subsystem\n");
			mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
				&message, sizeof(message));
		}

		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN)) {
			SCSC_TAG_INFO(MXMAN, "MM_HOST_SUSPEND WPAN Subsystem\n");
			mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport_wpan(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT,
				&message, sizeof(message));
		}
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		mxlogger_generate_sync_record(scsc_mx_get_mxlogger(mxman->mx), MXLOGGER_SYN_SUSPEND);
#endif
		mxman->suspended = 1;
		atomic_inc(&mxman->suspend_count);
	}
	mutex_unlock(&mxman->mxman_mutex);
	return 0;
}

void mxman_resume(struct mxman *mxman)
{
	struct srvman *srvman;
	struct ma_msg_packet message = { .ma_msg = MM_HOST_RESUME };
	int ret;

	SCSC_TAG_INFO(MXMAN, "\n");
	if (atomic_read(&mxman->cancel_resume)) {
		SCSC_TAG_INFO(MXMAN, "Recovery in progress ... ignoring");
		return;
	}

	mutex_lock(&mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(mxman->mx);
	if (srvman && srvman->error) {
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		mutex_unlock(&mxman->mxman_mutex);
		return;
	}

	if (mxman_in_started_state(mxman)) {
		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WLAN)) {
			SCSC_TAG_INFO(MXMAN, "MM_HOST_RESUME WLAN Subsystem\n");
			mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
		}

		if (mxman_subsys_active(mxman, SCSC_SUBSYSTEM_WPAN)) {
			SCSC_TAG_INFO(MXMAN, "MM_HOST_RESUME WPAN Subsystem\n");
			mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport_wpan(mxman->mx), MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message, sizeof(message));
		}
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		mxlogger_generate_sync_record(scsc_mx_get_mxlogger(mxman->mx), MXLOGGER_SYN_RESUME);
#endif

		mxman->suspended = 0;
	}

	/* Call Service Resume callbacks */
	if (srvman) {
		ret = srvman_resume_services(srvman);
	} else {
		SCSC_TAG_INFO(MXMAN, "srvman not found - ignore\n");
		mutex_unlock(&mxman->mxman_mutex);
		return;
	}

	if (ret)
		SCSC_TAG_INFO(MXMAN, "Service Resume error %d\n", ret);

	mutex_unlock(&mxman->mxman_mutex);
}

bool mxman_recovery_disabled(void)
{
#ifdef CONFIG_SCSC_WLBT_AUTORECOVERY_PERMANENT_DISABLE
	/* Add option to kill autorecovery, ignoring module parameter
	 * to work around platform that enables it against our wishes
	 */
	SCSC_TAG_ERR(MXMAN, "CONFIG_SCSC_WLBT_AUTORECOVERY_PERMANENT_DISABLE is set\n");
	return true;
#endif
	/* If FW has panicked when recovery was disabled, don't allow it to
	 * be enabled. The horse has bolted.
	 */
	if (disable_recovery_until_reboot)
		return true;

	if (disable_recovery_handling == MEMDUMP_FILE_FOR_RECOVERY)
		return disable_recovery_from_memdump_file;
	else
		return disable_recovery_handling ? true : false;
}
EXPORT_SYMBOL(mxman_recovery_disabled);

/**
 * This returns the last known loaded FW build_id
 * even when the fw is NOT running at the time of the request.
 *
 * It could be used anytime by Android Enhanced Logging
 * to query for fw version.
 */
void mxman_get_fw_version(char *version, size_t ver_sz)
{
	/* unavailable only if chip not probed ! */
	snprintf(version, ver_sz, "%s", saved_fw_build_id);
	SCSC_TAG_INFO(MXMAN, "Returning FW Version = %s\n", version);
}
EXPORT_SYMBOL(mxman_get_fw_version);

void mxman_get_driver_version(char *version, size_t ver_sz)
{
	/* IMPORTANT - Do not change the formatting as User space tooling is parsing the string
	* to read SAP fapi versions. */
	snprintf(version, ver_sz, "drv_ver: %u.%u.%u.%u.%u", SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION,
		 SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER);
#ifdef CONFIG_SCSC_WLBTD
	scsc_wlbtd_get_and_print_build_type();
#endif
}
EXPORT_SYMBOL(mxman_get_driver_version);

int mxman_register_firmware_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&firmware_chain, nb);
}
EXPORT_SYMBOL(mxman_register_firmware_notifier);

int mxman_unregister_firmware_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&firmware_chain, nb);
}
EXPORT_SYMBOL(mxman_unregister_firmware_notifier);

#if 0
enum mxman_states mxman_get_state(struct mxman *mxman)
{
	return (enum mxman_states)fsm_getstate(mxman->fsm);
}
#else
int mxman_get_state(struct mxman *mxman)
{
	return mxman->mxman_state;
}
#endif

u64 mxman_get_last_panic_time(struct mxman *mxman)
{
	return mxman->last_panic_time;
}

u32 mxman_get_panic_code(struct mxman *mxman)
{
	u16 reason;

	/* Use wlan panic code as default if exists */
	if (mxman->scsc_panic_code)
		reason = mxman->scsc_panic_code;
	else
		reason = mxman->scsc_panic_code_wpan;

	return reason;
}

u32 *mxman_get_last_panic_rec(struct mxman *mxman)
{
	return mxman->last_panic_rec_r;
}

u16 mxman_get_last_panic_rec_sz(struct mxman *mxman)
{
	return mxman->last_panic_rec_sz;
}

void mxman_reinit_completion(struct mxman *mxman)
{
	reinit_completion(&mxman->recovery_completion);
}

int mxman_wait_for_completion_timeout(struct mxman *mxman, u32 ms)
{
	return wait_for_completion_timeout(&mxman->recovery_completion,
				msecs_to_jiffies(ms));
}

void mxman_set_syserr_recovery_in_progress(struct mxman *mxman, bool value)
{
	mxman->syserr_recovery_in_progress = value;
}

void mxman_set_last_syserr_recovery_time(struct mxman *mxman, unsigned long value)
{
	mxman->last_syserr_recovery_time = value;
}

bool mxman_get_syserr_recovery_in_progress(struct mxman *mxman)
{
	return mxman->syserr_recovery_in_progress;
}

u16 mxman_get_last_syserr_subsys(struct mxman *mxman)
{
	return mxman->last_syserr.subsys;
}

int mxman_lerna_send(struct mxman *mxman, void *message, u32 message_size)
{
	struct srvman *srvman = NULL;

	/* May be called when WLBT is off, so find the context in this case */
	if (!mxman)
		mxman = active_mxman;

	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return -EINVAL;
	}

	if (!message || (message_size == 0)) {
		SCSC_TAG_INFO(MXMAN, "No lerna request provided.\n");
		return 0;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && srvman->error) {
		mutex_unlock(&active_mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Lerna configuration called during error - ignore\n");
		return 0;
	}

	if (active_mxman->mxman_state == MXMAN_STATE_STARTED) {
		SCSC_TAG_INFO(MXMAN, "MM_LERNA_CONFIG\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(active_mxman->mx),
				      MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, message, message_size);
		mutex_unlock(&active_mxman->mxman_mutex);
		return 0;
	}

	SCSC_TAG_INFO(MXMAN, "MXMAN is NOT STARTED...cannot send MM_LERNA_CONFIG msg.\n");
	mutex_unlock(&active_mxman->mxman_mutex);
	return -EAGAIN;
}

#if IS_ENABLED(CONFIG_SCSC_FM)
static bool send_fm_params_to_active_mxman(struct wlbt_fm_params *params)
{
	bool ret = false;
	struct srvman *srvman = NULL;

	SCSC_TAG_INFO(MXMAN, "\n");
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "Active MXMAN NOT FOUND...cannot send FM params\n");
		return false;
	}

	mutex_lock(&active_mxman->mxman_mutex);
	srvman = scsc_mx_get_srvman(active_mxman->mx);
	if (srvman && srvman->error) {
		mutex_unlock(&active_mxman->mxman_mutex);
		SCSC_TAG_INFO(MXMAN, "Called during error - ignore\n");
		return false;
	}

	if (active_mxman->mxman_state == MXMAN_STATE_STARTED_WPAN	||
		active_mxman->mxman_state == MXMAN_STATE_STARTED_WLAN_WPAN) {
		struct ma_msg_packet_fm_radio_config message = { .ma_msg = MM_FM_RADIO_CONFIG,
			.fm_params = *params };

		SCSC_TAG_INFO(MXMAN, "MM_FM_RADIO_CONFIG\n");
		mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport_wpan(active_mxman->mx),
				MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, &message,
				sizeof(message));

		ret = true;     /* Success */
	} else
		SCSC_TAG_INFO(MXMAN, "MXMAN is NOT STARTED...cannot send MM_FM_RADIO_CONFIG msg.\n");

	mutex_unlock(&active_mxman->mxman_mutex);

	return ret;
}

void mxman_fm_on_halt_ldos_on(void)
{
	/* Should always be an active mxman unless module is unloaded */
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return;
	}

	active_mxman->on_halt_ldos_on = 1;

	/* FM status to pass into FW at next FW init,
	 * by which time driver context is lost.
	 * This is required, because now WLBT gates
	 * LDOs with TCXO instead of leaving them
	 * always on, to save power in deep sleep.
	 * FM, however, needs them always on. So
	 * we need to know when to leave the LDOs
	 * alone at WLBT boot.
	 */
	//is_fm_on = 1;
}
EXPORT_SYMBOL(mxman_fm_on_halt_ldos_on);

void mxman_fm_on_halt_ldos_off(void)
{
	/* Should always be an active mxman unless module is unloaded */
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return;
	}

	/* Newer FW no longer need set shared LDOs
	 * always-off at WLBT halt, as TCXO gating
	 * has the same effect. But pass the "off"
	 * request for backwards compatibility
	 * with old FW.
	 */
	active_mxman->on_halt_ldos_on = 0;
	//is_fm_on = 0;
}
EXPORT_SYMBOL(mxman_fm_on_halt_ldos_off);

/* Update parameters passed to WLBT FM */
int mxman_fm_set_params(struct wlbt_fm_params *params)
{
	/* Should always be an active mxman unless module is unloaded */
	if (!active_mxman) {
		SCSC_TAG_ERR(MXMAN, "No active MXMAN\n");
		return -EINVAL;
	}

	/* Params are no longer valid (FM stopped) */
	if (!params) {
		active_mxman->fm_params_pending = 0;
		SCSC_TAG_INFO(MXMAN, "FM params cleared\n");
		return 0;
	}

	/* Once set the value needs to be remembered for each time WLBT starts */
	active_mxman->fm_params = *params;
	active_mxman->fm_params_pending = 1;

	if (send_fm_params_to_active_mxman(params)) {
		SCSC_TAG_INFO(MXMAN, "FM params sent to FW\n");
		return 0;
	}

	/* Stored for next time FW is up */
	SCSC_TAG_INFO(MXMAN, "FM params stored\n");

	return -EAGAIN;
}
EXPORT_SYMBOL(mxman_fm_set_params);
#endif
