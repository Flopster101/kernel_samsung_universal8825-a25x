/****************************************************************************
 *
 *   Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>

#include "scsc_mx_impl.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

/* Firmware directory definitions */

#define SCSC_MULTI_RF_CHIP_ID /* Select FW by RF chip ID, not rev */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
#if defined(CONFIG_SCSC_CORE_FW_LOCATION) && !defined(CONFIG_SCSC_CORE_FW_LOCATION_AUTO)
#define MX140_FW_BASE_DIR_SYSTEM_ETC_WIFI      CONFIG_SCSC_CORE_FW_LOCATION
#define MX140_FW_BASE_DIR_VENDOR_ETC_WIFI      CONFIG_SCSC_CORE_FW_LOCATION
#else
#define MX140_FW_BASE_DIR_SYSTEM_ETC_WIFI	"/system/etc/wifi"
#define MX140_FW_BASE_DIR_VENDOR_ETC_WIFI	"/vendor/etc/wifi"
#endif
#else /* when using request_firmware we need to hardcode the folders */
#define MX140_FW_BASE_DIR_SYSTEM_ETC_WIFI	"/vendor/etc/wifi"
#define MX140_FW_BASE_DIR_VENDOR_ETC_WIFI	"/vendor/etc/wifi"
#endif

/* Paths for vendor utilities, used when CONFIG_SCSC_CORE_FW_LOCATION_AUTO=n */
#define MX140_EXE_DIR_VENDOR		"/vendor/bin"    /* Oreo */
#define MX140_EXE_DIR_SYSTEM		"/system/bin"	 /* Before Oreo */

#define MX140_FW_CONF_SUBDIR            "conf"
#define MX140_FW_DEBUG_SUBDIR           "debug"
#define MX140_FW_BIN                    "mx140.bin"
#define MX140_FW_PATH_MAX_LENGTH        (512)

#define MX140_FW_VARIANT_DEFAULT        "mx140"

/* Table of suffixes to append to f/w name */
struct fw_suffix {
	char suffix[6];
	u32 hw_ver;
};

#ifdef SCSC_MULTI_RF_CHIP_ID /* Select by chip ID (S611, S612) */

/* This scheme has one firmware binary for all revisions of an
 * RF chip ID.
 */

/* Table of known RF h/w IDs */
static const struct fw_suffix fw_suffixes[] = {
	{ .suffix = "",      .hw_ver = 0xff, }, /* plain mx140.bin, always used if found */
	{ .suffix = "_s612", .hw_ver = 0xb1, },
	{ .suffix = "_s611", .hw_ver = 0xb0, },
};

#else /* Select by chip revision (EVT0.0, EVT0.1) */

/* This legacy scheme assumes a different fw binary for each revision
 * of an RF chip ID, and those will uniquely identify the
 * right build. This was used for early S5E7570 until a unified
 * binary was available.
 */

/* Table of known RF h/w revs */
static const struct fw_suffix fw_suffixes[] = {
	{ .suffix = "_11", .hw_ver = 0x11, },
	{ .suffix = "_10", .hw_ver = 0x10, },
	{ .suffix = "_00", .hw_ver = 0x00, },
	{ .suffix = "",    .hw_ver = 0xff, }, /* plain mx140.bin, must be last */
};
#endif

/* Once set, we always load this firmware suffix */
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
/* On independent subsystem platforms only plain mx140.bin are supported. If
 * it is -1 then __mx140_file_request_conf will kernel panic in the scnprintf
 * when accessing arrary out of range as fw_suffixes[-1].suffix. Clean up of
 * this is expected to be done via HOST-14381.
 */
static int fw_suffix_found = 0;
#else
/* Legacy? */
static int fw_suffix_found = -1;
#endif

/* Variant of firmware binary to load */
static char *firmware_variant = MX140_FW_VARIANT_DEFAULT;
module_param(firmware_variant, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_variant, "mx140 firmware variant, default mx140");

/* RF hardware version of firmware to load. If "auto" this gets replaced with
 * the suffix of FW that got loaded.
 * If "manual" it loads the version specified by firmware_variant, verbatim.
 */
static char *firmware_hw_ver = "auto";
module_param(firmware_hw_ver, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_hw_ver, "mx140 hw version detect, manual=disable");

/* FW base dir readable by usermode script */
#ifdef CONFIG_SCSC_CORE_FW_LOCATION_AUTO
static char *fw_base_dir;
#else
#if IS_ENABLED(CONFIG_SCSC_PCIE)
static char *fw_base_dir = "/lib/firmware/";
#else
static char *fw_base_dir = CONFIG_SCSC_CORE_FW_LOCATION;
#endif
#endif
module_param_named(base_dir, fw_base_dir, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(base_dir, "WLBT FW base directory");

/* Firmware and tool (moredump) exe base directory */
#ifdef CONFIG_SCSC_CORE_FW_LOCATION_AUTO
static char base_dir[MX140_FW_PATH_MAX_LENGTH]; /* auto detect */
static char exe_dir[MX140_FW_PATH_MAX_LENGTH];	/* auto detect */
#else
#if IS_ENABLED(CONFIG_SCSC_PCIE)
static char base_dir[] = "/lib/firmware/";
static char exe_dir[] = "/usr/bin/";
#else
static char base_dir[] = CONFIG_SCSC_CORE_FW_LOCATION;  /* fixed in defconfig */
static char exe_dir[] = CONFIG_SCSC_CORE_TOOL_LOCATION;	/* fixed in defconfig */
#endif
#endif

#if IS_ENABLED(CONFIG_SCSC_PCIE)
static char base_dir_request_fw[] = "";
#else
static char base_dir_request_fw[] = "../etc/wifi";  /* fixed in defconfig */
#endif


static bool enable_auto_sense;
module_param(enable_auto_sense, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_auto_sense, "deprecated");

static bool use_new_fw_structure = true;
module_param(use_new_fw_structure, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(use_new_fw_structure, "deprecated");

static char *cfg_platform = "default";
module_param(cfg_platform, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(cfg_platform, "HCF config subdirectory");

#if defined SCSC_SEP_VERSION
static bool force_flat = true; /* Refer to hcf from /vendor/etc/wifi/ */
#else
/* AOSP */
static bool force_flat = false; /* Refer to hcf from /vendor/etc/wifi/mx140/conf/ */
#endif
module_param(force_flat, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(force_flat, "Forcely request flat conf");


/* Reads a configuration file into memory (f/w profile specific) */
static int __mx140_file_request_conf(struct scsc_mx *mx,
		const struct firmware **conf,
		const char *platform_dir,
		const char *config_rel_path,
		const char *filename,
		const bool flat)

{
	char config_path[MX140_FW_PATH_MAX_LENGTH];
	int r;

	if (mx140_basedir_file(mx))
		return -ENOENT;

	if (flat) {
		/* e.g. /etc/wifi/mx140_wlan.hcf */

		scnprintf(config_path, sizeof(config_path),
			"%s/%s%s_%s",
			base_dir_request_fw,
			firmware_variant,
			fw_suffixes[0].suffix,
			filename);
	} else {
		/* e.g. /etc/wifi/mx140/conf/$platform_dir/wlan/wlan.hcf */

		scnprintf(config_path, sizeof(config_path),
			"%s/%s%s/%s/%s%s%s/%s",
			base_dir_request_fw,
			firmware_variant,
			fw_suffixes[0].suffix,
			MX140_FW_CONF_SUBDIR,
			platform_dir,
			(platform_dir[0] != '\0' ? "/" : ""), /* add "/" if platform_dir not empty */
			config_rel_path,
			filename);
	}
	SCSC_TAG_INFO(MX_FILE, "try %s\n", config_path);

	r = mx140_request_file(mx, config_path, conf);

	/* Confirm what we read */
	if (r == 0)
		SCSC_TAG_INFO(MX_FILE, "loaded %s\n", config_path);

	return r;
}

int mx140_file_request_conf(struct scsc_mx *mx,
			    const struct firmware **conf,
			    const char *config_rel_path,
			    const char *filename)
{
	int r;

	/* First, if the config subdirectory has been overriden by cfg_platform
	 * module parameter, search only in that location.
	 */
	if (strcmp(cfg_platform, "default")) {
		SCSC_TAG_INFO(MX_FILE, "module param cfg_platform = %s\n", cfg_platform);
		return __mx140_file_request_conf(mx, conf, cfg_platform, config_rel_path, filename, false);
	}
	/* This is done to update the fw_suffix_found index */
	if (fw_suffix_found == -1) {
		r = mx140_file_download_fw(mx, NULL, 0, NULL);
		if (r) {
			SCSC_TAG_ERR(MXMAN, "mx140_file_download_fw() failed (%d)\n", r);
			return r;
		}
	}

	if (force_flat) {
		/* Only request "flat" conf, where all hcf files are in FW root dir
		 * e.g. /etc/wifi/<firmware-variant>-wlan.hcf
		 */
		r = __mx140_file_request_conf(mx, conf, "", config_rel_path, filename, true);
		SCSC_TAG_INFO(MX_FILE, "forcely request flat conf = %d\n", r);
	} else {
		/* Search in generic location. This is an override.
		 * e.g. /etc/wifi/mx140/conf/wlan/wlan.hcf
		 */
		r = __mx140_file_request_conf(mx, conf, "", config_rel_path, filename, false);

#if defined CONFIG_SCSC_WLBT_CONFIG_PLATFORM
		/* Then  search in platform location
		 * e.g. /etc/wifi/mx140/conf/$platform_dir/wlan/wlan.hcf
		 */
		if (r) {
			const char *plat = CONFIG_SCSC_WLBT_CONFIG_PLATFORM;

			/* Don't bother if plat is empty string */
			if (plat[0] != '\0')
				r = __mx140_file_request_conf(mx, conf, plat, config_rel_path, filename, false);
		}
#endif

		/* Finally request "flat" conf, where all hcf files are in FW root dir
		 * e.g. /etc/wifi/<firmware-variant>-wlan.hcf
		 */
		if (r)
			r = __mx140_file_request_conf(mx, conf, "", config_rel_path, filename, true);
	}

	return r;
}

EXPORT_SYMBOL(mx140_file_request_conf);

/* Reads a debug configuration file into memory (f/w profile specific) */
int mx140_file_request_debug_conf(struct scsc_mx *mx, const struct firmware **conf, const char *config_rel_path)
{
	char          config_path[MX140_FW_PATH_MAX_LENGTH];

	if (mx140_basedir_file(mx))
		return -ENOENT;

	/* e.g. /etc/wifi/mx140/debug/log_strings.bin */

	scnprintf(config_path, sizeof(config_path),
		"%s/%s%s/%s/%s",
		base_dir_request_fw,
		firmware_variant,
		fw_suffixes[fw_suffix_found].suffix,
		MX140_FW_DEBUG_SUBDIR,
		config_rel_path);

	return mx140_request_file(mx, config_path, conf);
}
EXPORT_SYMBOL(mx140_file_request_debug_conf);

/* Read device configuration file into memory (whole device specific) */
int mx140_file_request_device_conf(struct scsc_mx *mx, const struct firmware **conf, const char *config_rel_path)
{
	char          config_path[MX140_FW_PATH_MAX_LENGTH];

	if (mx140_basedir_file(mx))
		return -ENOENT;

	/* e.g. /etc/wifi/conf/wlan/mac.txt */

	snprintf(config_path, sizeof(config_path),
		"%s/%s%s/%s",
		base_dir_request_fw,
		fw_suffixes[fw_suffix_found].suffix,
		MX140_FW_CONF_SUBDIR,
		config_rel_path);

	return mx140_request_file(mx, config_path, conf);
}
EXPORT_SYMBOL(mx140_file_request_device_conf);

/* Release configuration file memory. */
void mx140_file_release_conf(struct scsc_mx *mx, const struct firmware *conf)
{
	(void)mx;

	mx140_release_file(mx, conf);
}
EXPORT_SYMBOL(mx140_file_release_conf);

static int __mx140_file_download_fw(struct scsc_mx *mx, void *dest, size_t dest_size, u32 *fw_image_size, const char *fw_suffix)
{
	const struct firmware *firm;
	int                   r = 0;
	char                  img_path_name[MX140_FW_PATH_MAX_LENGTH];

	if (mx140_basedir_file(mx))
		return -ENOENT;

	SCSC_TAG_INFO(MX_FILE, "firmware_variant=%s (%s)\n", firmware_variant, fw_suffix);

	/* e.g. /etc/wifi/mx140.bin */
	scnprintf(img_path_name, sizeof(img_path_name),
		"%s/%s%s.bin",
		base_dir_request_fw,
		firmware_variant,
		fw_suffix);

	SCSC_TAG_INFO(MX_FILE, "Load WLBT fw %s in shared address %p\n", img_path_name, dest);
	r = mx140_request_file(mx, img_path_name, &firm);
	if (r) {
		SCSC_TAG_ERR(MX_FILE, "Error Loading FW, error %d\n", r);
		return r;
	}
	SCSC_TAG_DBG4(MX_FILE, "FW Download, size %zu\n", firm->size);

	if (dest) {
		if (firm->size > dest_size) {
			SCSC_TAG_ERR(MX_FILE, "firmware image too big for buffer (%zu > %u)", dest_size, *fw_image_size);
			r = -EINVAL;
		} else {
			memcpy(dest, firm->data, firm->size);
			*fw_image_size = firm->size;
		}
	}
	mx140_release_file(mx, firm);
	return r;
}

/* Download firmware binary into a buffer supplied by the caller */
int mx140_file_download_fw(struct scsc_mx *mx, void *dest, size_t dest_size, u32 *fw_image_size)
{
	int r;
	int i;
	int manual;

	/* Override to use the verbatim image only */
	manual = !strcmp(firmware_hw_ver, "manual");
	if (manual) {
		SCSC_TAG_INFO(MX_FILE, "manual hw version\n");
		fw_suffix_found = sizeof(fw_suffixes) / sizeof(fw_suffixes[0]) - 1;
	}

	SCSC_TAG_DEBUG(MX_FILE, "fw_suffix_found %d\n", fw_suffix_found);

	/* If we know which f/w suffix to use, select it immediately */
	if (fw_suffix_found != -1) {
		r = __mx140_file_download_fw(mx, dest, dest_size, fw_image_size, fw_suffixes[fw_suffix_found].suffix);
		goto done;
	}

	/* Otherwise try the list */
	for (i = 0; i < sizeof(fw_suffixes) / sizeof(fw_suffixes[0]); i++) {
		/* Try to find each suffix in turn */
		SCSC_TAG_INFO(MX_FILE, "try %d %s\n", i, fw_suffixes[i].suffix);
		r = __mx140_file_download_fw(mx, dest, dest_size, fw_image_size, fw_suffixes[i].suffix);
		if (r != -ENOENT)
			break;
	}

	/* Save this for next time */
	if (r == 0)
		fw_suffix_found = i;
done:
	/* Update firmware_hw_ver to reflect what got auto selected, for moredump */
	if (fw_suffix_found != -1 && !manual) {
		/* User will only read this, so casting away const is safe */
		firmware_hw_ver = (char *)fw_suffixes[fw_suffix_found].suffix;
	}
	return r;
}

static int __mx140_file_get_fw(struct scsc_mx *mx, const struct firmware **firm, const char *fw_suffix)
{
	int                   r = 0;
	char                  img_path_name[MX140_FW_PATH_MAX_LENGTH];

	if (mx140_basedir_file(mx))
		return -ENOENT;

	SCSC_TAG_INFO(MX_FILE, "firmware_variant=%s (%s)\n", firmware_variant, fw_suffix);

	/* e.g. /etc/wifi/mx140.bin */
	scnprintf(img_path_name, sizeof(img_path_name),
		"%s/%s%s.bin",
		base_dir_request_fw,
		firmware_variant,
		fw_suffix);

	SCSC_TAG_INFO(MX_FILE, "Get WLBT fw %s\n", img_path_name);
	r = mx140_request_file(mx, img_path_name, firm);
	if (r) {
		SCSC_TAG_ERR(MX_FILE, "Error Loading FW, error %d\n", r);
		return r;
	}
	SCSC_TAG_INFO(MX_FILE, "Get WLBT fw success, size %zu\n", (*firm)->size);

	return r;
}

int mx140_file_get_fw(struct scsc_mx *mx, const struct firmware **firm)
{
	int r;
	int i;
	int manual;

	/* Override to use the verbatim image only */
	manual = !strcmp(firmware_hw_ver, "manual");
	if (manual) {
		SCSC_TAG_INFO(MX_FILE, "manual hw version\n");
		fw_suffix_found = sizeof(fw_suffixes) / sizeof(fw_suffixes[0]) - 1;
	}

	SCSC_TAG_DEBUG(MX_FILE, "fw_suffix_found %d\n", fw_suffix_found);

	/* If we know which f/w suffix to use, select it immediately */
	if (fw_suffix_found != -1) {
		r = __mx140_file_get_fw(mx, firm, fw_suffixes[fw_suffix_found].suffix);
		goto done;
	}

	/* Otherwise try the list */
	for (i = 0; i < sizeof(fw_suffixes) / sizeof(fw_suffixes[0]); i++) {
		/* Try to find each suffix in turn */
		SCSC_TAG_INFO(MX_FILE, "try %d %s\n", i, fw_suffixes[i].suffix);
		r = __mx140_file_get_fw(mx, firm, fw_suffixes[i].suffix);
		if (r != -ENOENT)
			break;
	}

	/* Save this for next time */
	if (r == 0)
		fw_suffix_found = i;
done:
	/* Update firmware_hw_ver to reflect what got auto selected, for moredump */
	if (fw_suffix_found != -1 && !manual) {
		/* User will only read this, so casting away const is safe */
		firmware_hw_ver = (char *)fw_suffixes[fw_suffix_found].suffix;
	}
	return r;
}

int __mx140_request_firmware(struct scsc_mx *mx, char *path, const struct firmware **firmp)
{
	int ret;
	struct device *dev;

	SCSC_TAG_DEBUG(MX_FILE, "request %s\n", path);

	dev = scsc_mx_get_device(mx);
	if (!dev) {
		SCSC_TAG_ERR(MX_FILE, "Error. Device is NULL\n");
		return -EIO;
	}
	scsc_mx_request_firmware_mutex_lock(mx);
	scsc_mx_request_firmware_wake_lock(mx);
	ret = request_firmware(firmp, path, dev);
	scsc_mx_request_firmware_wake_unlock(mx);
	scsc_mx_request_firmware_mutex_unlock(mx);

	SCSC_TAG_DEBUG(MX_FILE, "request %s\n", path);

	return ret;
}

int mx140_request_file(struct scsc_mx *mx, char *path, const struct firmware **firmp)
{
	return __mx140_request_firmware(mx, path, firmp);
}
EXPORT_SYMBOL(mx140_request_file);


int __mx140_release_firmware(struct scsc_mx *mx, const struct firmware *firmp)
{
	if (!firmp || !firmp->data) {
		SCSC_TAG_ERR(MX_FILE, "firmp=%p\n", firmp);
		return -EINVAL;
	}

	SCSC_TAG_DEBUG(MX_FILE, "release firmp=%p, data=%p\n", firmp, firmp->data);

	release_firmware(firmp);

	return 0;
}

int __mx140_release_file(struct scsc_mx *mx, const struct firmware *firmp)
{
	if (!firmp || !firmp->data) {
		SCSC_TAG_ERR(MX_FILE, "firmp=%p\n", firmp);
		return -EINVAL;
	}

	SCSC_TAG_DEBUG(MX_FILE, "release firmp=%p, data=%p\n", firmp, firmp->data);

	vfree(firmp->data);
	kfree(firmp);
	return 0;
}

int mx140_release_file(struct scsc_mx *mx, const struct firmware *firmp)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	return __mx140_release_file(mx, firmp);
#else
	return __mx140_release_firmware(mx, firmp);
#endif
}
EXPORT_SYMBOL(mx140_release_file);

/* Work out correct path for vendor binaries */
int mx140_exe_path(struct scsc_mx *mx, char *path, size_t len, const char *bin)
{
	(void)mx;

	/* Set up when we detect FW path, or statically when
	 * auto-detect is off
	 */
	if (exe_dir[0] == '\0')
		return -ENOENT;

	if (path == NULL)
		return -EINVAL;

	snprintf(path, len, "%s/%s", exe_dir, bin);

	SCSC_TAG_DEBUG(MX_FILE, "exe: %s\n", path);
	return 0;
}
EXPORT_SYMBOL(mx140_exe_path);

/* Try to auto detect f/w directory */
int mx140_basedir_file(struct scsc_mx *mx)
{
	int r = 0;

	/* Already worked out base dir. This is
	 * static if auto-detect is off.
	 */
	if (base_dir[0] != '\0')
		return 0;

	/* Try /vendor partition  (post-Oreo) */
	strlcpy(base_dir, MX140_FW_BASE_DIR_VENDOR_ETC_WIFI, sizeof(base_dir));
	fw_base_dir = MX140_FW_BASE_DIR_VENDOR_ETC_WIFI;
	strlcpy(exe_dir, MX140_EXE_DIR_VENDOR, sizeof(exe_dir));
#if defined(SCSC_SEP_VERSION) && (SCSC_SEP_VERSION < 8)
	/* Try /system partition (pre-Oreo) */
	strlcpy(base_dir, MX140_FW_BASE_DIR_SYSTEM_ETC_WIFI, sizeof(base_dir));
	fw_base_dir = MX140_FW_BASE_DIR_SYSTEM_ETC_WIFI;
	strlcpy(exe_dir, MX140_EXE_DIR_SYSTEM, sizeof(exe_dir));
#endif

	SCSC_TAG_INFO(MX_FILE, "WLBT base_dir is %s\n", base_dir[0] ? base_dir : "not found");
	SCSC_TAG_INFO(MX_FILE, "WLBT fw_base_dir is %s\n", fw_base_dir[0] ? fw_base_dir : "not found");
	return r;
}

/* Select file for h/w version from filesystem */
int mx140_file_select_fw(struct scsc_mx *mx, u32 hw_ver)
{
	int i;

	SCSC_TAG_INFO(MX_FILE, "select f/w for 0x%04x\n", hw_ver);

#ifdef SCSC_MULTI_RF_CHIP_ID
	hw_ver = (hw_ver & 0x00ff);	 /* LSB is the RF HW ID (e.g. S610) */
#else
	hw_ver = (hw_ver & 0xff00) >> 8; /* MSB is the RF HW rev (e.g. EVT1.1) */
#endif

	for (i = 0; i < sizeof(fw_suffixes) / sizeof(fw_suffixes[0]); i++) {
		if (fw_suffixes[i].hw_ver == hw_ver) {
			fw_suffix_found = i;
			SCSC_TAG_DEBUG(MX_FILE, "f/w for 0x%04x: index %d, suffix '%s'\n",
				hw_ver, i, fw_suffixes[i].suffix);
			return 0;
		}
	}

	SCSC_TAG_ERR(MX_FILE, "No known f/w for 0x%04x, default to catchall\n", hw_ver);

	/* Enable when a unified FW image is installed */
#ifdef MX140_UNIFIED_HW_FW
	/* The last f/w is the non-suffixed "<fw>.bin", assume it's compatible */
	fw_suffix_found = i - 1;
#else
	fw_suffix_found = -1; /* None found */
#endif
	return -EINVAL;
}

/* Query whether this HW is supported by the current FW file set */
bool mx140_file_supported_hw(struct scsc_mx *mx, u32 hw_ver)
{
#ifdef SCSC_MULTI_RF_CHIP_ID
	hw_ver = (hw_ver & 0x00ff);	 /* LSB is the RF HW ID (e.g. S610) */
#else
	hw_ver = (hw_ver & 0xff00) >> 8; /* MSB is the RF HW rev (e.g. EVT1.0) */
#endif
	/* Assume installed 0xff is always compatible, and f/w will panic if it isn't */
	if (fw_suffixes[fw_suffix_found].hw_ver == 0xff)
		return true;

	/* Does the select f/w match the hw_ver from chip? */
	return (fw_suffixes[fw_suffix_found].hw_ver == hw_ver);
}

