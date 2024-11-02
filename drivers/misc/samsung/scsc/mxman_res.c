/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <scsc/scsc_logring.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include "mxman_res.h"
#include "scsc_mx_impl.h"
#include "mxman.h"
#include "whdr.h"
#include "fwhdr_if.h"
#include "bhdr.h"
#include "fhdr.h"
#include "fw_obj_index.h"
#include "pmuhdr.h"
#include <scsc/kic/slsi_kic_lib.h>
#include <scsc/scsc_release.h>
#include <scsc/scsc_mx.h>

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

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
#include <soc/samsung/memlogger.h>
#endif

/* MIF resources - USES */
#include "mifmboxman.h"
#include "miframman.h"
#include "mifpmuman.h"
#include "mxfwconfig.h"
#include "mxlog_transport.h"
#include "mxlog.h"
#include "gdb_transport.h"
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
#include "mxlogger.h"
#endif
#ifdef CONFIG_SCSC_SMAPPER
#include "mifsmapper.h"
#endif
#ifdef CONFIG_SCSC_QOS
#include "mifqos.h"
#endif
#ifdef CONFIG_SCSC_LAST_PANIC_IN_DRAM
#include "scsc_log_in_dram.h"
#endif

/* This values should be defined in the DTS.. so we may need to
 * provision some calls to platform driver */
#define MX_DRAM_SIZE_SECTION_1 (8 * 1024 * 1024)
#define MX_DRAM_SIZE_SECTION_2 (8 * 1024 * 1024)

static uint firmware_startup_flags;
module_param(firmware_startup_flags, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(firmware_startup_flags,
		 "0 = Proceed as normal (default); Bit 0 = 1 - spin at start of CRT0; Other bits reserved = 0");

static bool allow_unidentified_firmware;
module_param(allow_unidentified_firmware, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(allow_unidentified_firmware, "Allow unidentified firmware");

static bool skip_header;
module_param(skip_header, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(skip_header, "Skip header, assuming unidentified firmware");

int mxman_res_mem_map(struct mxman *mxman, void **start_dram, size_t *size_dram)
{
	struct scsc_mif_abs *mif;

	mif = scsc_mx_get_mif_abs(mxman->mx);

	*start_dram = mif->map(mif, size_dram);

	if (!*start_dram) {
		SCSC_TAG_ERR(MXMAN, "Error allocating dram\n");
		return -ENOMEM;
	}

	return 0;
}

int mxman_res_mem_unmap(struct mxman *mxman, void *start_dram)
{
	struct scsc_mif_abs *mif;
	mif = scsc_mx_get_mif_abs(mxman->mx);

	mif->unmap(mif, start_dram);

	mxlog_unload_log_strings(scsc_mx_get_mxlog(mxman->mx));
	mxlog_unload_log_strings(scsc_mx_get_mxlog_wpan(mxman->mx));

	return 0;
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
struct memlog_obj *mxman_res_get_memlog_obj(struct scsc_mif_abs *mif, const char *desc_name, size_t len)
{
	struct device *dev = mif->get_mif_device(mif);
	struct memlog *desc = memlog_get_desc(desc_name);
	struct memlog_obj *obj = NULL;
	const char *obj_name = "drm-mem";

	if (!desc) {
		int val = memlog_register(desc_name, dev, &desc);

		if (val) {
			SCSC_TAG_ERR(MXMAN, "Error registering %s to memlog\n", desc_name);
			return NULL;
		}

		obj = memlog_alloc_direct(desc, len, NULL, obj_name);
		if (!obj) {
			/* Alloc fail - null fallback  */
			SCSC_TAG_INFO(MXMAN, "obj alloc failed!!\n");
		}
	} else {
		/* Object exists, return the pointer */
		obj = memlog_get_obj_by_name(desc, obj_name);
	}
	return obj;
}

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
static void mxman_res_set_memlog_version(struct scsc_mif_abs *mif)
{
	struct memlog *desc = memlog_get_desc("WB_LOG");
	struct memlog_obj *scsc_memlog_version_info_obj;

	struct scsc_memlog_version_info {
		char fw_version[128];
		char host_version[64];
		char fapi_version[64];
	} * memlog_version_info;

	if (desc) {
		//const char *fapi_version = "ma:14.1, mlme:14.6, debug:13.3, test:14.0";

		scsc_memlog_version_info_obj = memlog_get_obj_by_name(desc, "str-mem");
		if (!scsc_memlog_version_info_obj) {
			scsc_memlog_version_info_obj =
				memlog_alloc_array(desc, 1, sizeof(struct scsc_memlog_version_info), NULL, "str-mem",
						   "scsc_memlog_version_info", 0);

			if (!scsc_memlog_version_info_obj) {
				/* Alloc fail */
				return;
			}
		}

		memlog_version_info = (struct scsc_memlog_version_info *)scsc_memlog_version_info_obj->vaddr;
	}
}
#endif
#endif

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
int mxman_res_mappings_logger_init(struct mxman *mxman, void *start_dram)
{
	void                *start_dram_section2;
	int                 ret = 0;
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	const char          *desc_name = "WB_LOG";
	struct memlog_obj   *obj;
	struct device       *dev;
	struct scsc_mif_abs *mif;

	mif = scsc_mx_get_mif_abs(mxman->mx);

	SCSC_TAG_INFO(MXMAN, "Using memlogger for mxlogger\n");
	dev = mif->get_mif_device(mif);
	obj = mxman_res_get_memlog_obj(mif, desc_name, MX_DRAM_SIZE_SECTION_LOG);
	if (!obj) {
		SCSC_TAG_ERR(MXMAN, "memlog error\n");
		return -ENOMEM;
	}
	mxman_res_set_memlog_version(mif);
	/* assing memory regions for memory mappings */
	mif->set_mem_region2(mif, obj->vaddr, MX_DRAM_SIZE_SECTION_LOG);
	mif->set_memlog_paddr(mif, obj->paddr);
	start_dram_section2 = (char *)obj->vaddr;
#else
	struct scsc_mif_abs *mif;
	//scsc_mifram_ref mifram_ref;

	mif = scsc_mx_get_mif_abs(mxman->mx);
	start_dram_section2 = (char *)start_dram + MX_DRAM_SIZE_SECTION_1;
#endif
	/* Second partition allocator */
	/* Initialize allocator for mem region for logging WLAN */
	ret = miframman_init(scsc_mx_get_ramman2(mxman->mx), start_dram_section2, MX_DRAM_SIZE_SECTION_WLAN,
		       start_dram_section2);
	if (ret) {
		SCSC_TAG_INFO(MXMAN, "miframman_init failed with %d\n", ret);
		return ret;
	}
	/* Initialize allocator for mem region for logging WPAN */
	start_dram_section2 = (char *)start_dram_section2 + MX_DRAM_SIZE_SECTION_WLAN;
	ret = miframman_init(scsc_mx_get_ramman2_wpan(mxman->mx), start_dram_section2, MX_DRAM_SIZE_SECTION_WPAN,
		       start_dram_section2);
	if (ret) {
		SCSC_TAG_INFO(MXMAN, "miframman_init failed with %d\n", ret);
		return ret;
	}

	mxlogger_init(mxman->mx, scsc_mx_get_mxlogger(mxman->mx), MX_DRAM_SIZE_SECTION_LOG);
	return 0;
}
int mxman_res_mappings_logger_deinit(struct mxman *mxman)
{
	miframman_deinit(scsc_mx_get_ramman_wpan(mxman->mx));
	miframman_deinit(scsc_mx_get_ramman2_wpan(mxman->mx));

	return 0;
}
#endif

int mxman_res_mappings_allocator_init(struct mxman *mxman, void *start_dram)
{
	void *start_mifram_heap;
	u32 length_mifram_heap;
	struct fwhdr_if *whdr_if = mxman->fw_wlan;
	struct fwhdr_if *bhdr_if = mxman->fw_wpan;
	u32 wlan_rt = 0, wpan_rt = 0, wpan_off = 0;
	int ret = 0;

	/* mxconf should be appended after RT FWs */
	wlan_rt = whdr_if->get_fw_rt_len(whdr_if) + sizeof(struct mxconf);
	if (bhdr_if) {
		wpan_rt = bhdr_if->get_fw_rt_len(bhdr_if) + sizeof(struct mxconf);
		wpan_off = bhdr_if->get_fw_offset(bhdr_if);
	} else {
		/* Force MAX memory to WLAN as BT is not present */
		wpan_rt = 0;
		wpan_off = MX_DRAM_SIZE_SECTION_1;
	}

	SCSC_TAG_INFO(MXMAN, "Creating wlan/wpan memory regions. WLAN rt 0x%x WPAN rt 0x%x WPAN offset 0x%x\n", wlan_rt,
		      wpan_rt, wpan_off);

	if (wlan_rt > wpan_off) {
		SCSC_TAG_ERR(MXMAN, "WLAN FW Rt will overlay WPAN FW region\n");
		return -EIO;
	}

	if ((wpan_off + wpan_rt) > MX_DRAM_SIZE_SECTION_1) {
		SCSC_TAG_ERR(MXMAN, "WPAN FW region beyond total MX_DRAM_SIZE_SECTION_1 0x%x\n", MX_DRAM_SIZE_SECTION_1);
		return -EIO;
	}

	/* wlan partition allocator */
	start_mifram_heap = (void *)((uintptr_t)start_dram + wlan_rt);
	length_mifram_heap = wpan_off - wlan_rt;
	ret = miframman_init(scsc_mx_get_ramman(mxman->mx), start_mifram_heap, length_mifram_heap, start_dram);
	if (ret) {
		SCSC_TAG_INFO(MXMAN, "miframman_init failed with %d\n", ret);
		return ret;
	}

	/* Set here the mxconf address */
	mxman->mxconf = (struct mxconf *)((uintptr_t)start_dram + whdr_if->get_fw_rt_len(whdr_if));
	SCSC_TAG_INFO(MXMAN, "WLAN start 0x%x heap 0x%x heap length 0x%x\n", (uintptr_t)start_dram,
		      (uintptr_t)start_mifram_heap, length_mifram_heap);

	/* WPAN partition allocator */
	if (bhdr_if) {
		start_mifram_heap = (void *)((uintptr_t)start_dram + wpan_rt + wpan_off);
		length_mifram_heap = (MX_DRAM_SIZE_SECTION_1 - wpan_off) - wpan_rt;
		miframman_init(scsc_mx_get_ramman_wpan(mxman->mx), start_mifram_heap, length_mifram_heap,
			       (void *)((uintptr_t)start_dram + wpan_off));
		SCSC_TAG_INFO(MXMAN, "WPAN start 0x%x heap 0x%x heap length 0x%x\n", ((uintptr_t)start_dram + wpan_off),
			      (uintptr_t)start_mifram_heap, length_mifram_heap);

		/* Set here the mxconf_wpan address */
		mxman->mxconf_wpan = (struct mxconf *)((uintptr_t)start_dram + bhdr_if->get_fw_rt_len(bhdr_if) + wpan_off);
	}
	return 0;
}

int mxman_res_mappings_allocator_deinit(struct mxman *mxman)
{
	miframman_deinit(scsc_mx_get_ramman(mxman->mx));
	miframman_deinit(scsc_mx_get_ramman2(mxman->mx));
	return 0;
}

int mxman_res_fw_init_get_fhdr_strings(const void *fw)
{
	uint32_t len;
	const void *val;

	val = fhdr_lookup_tag(fw, FHDR_TAG_META_BRANCH, &len);
	if (val == NULL) {
		SCSC_TAG_ERR(MXMAN, "fhdr_lookup_tag FHDR_TAG_META_BRANCH missing\n");
	} else {
		SCSC_TAG_INFO(MXMAN, "FHDR_TAG_META_BRANCH: %.*s\n", len, val);
	}

	val = fhdr_lookup_tag(fw, FHDR_TAG_META_BUILD_IDENTIFIER, &len);
	if (val == NULL) {
		SCSC_TAG_ERR(MXMAN, "fhdr_lookup_tag FHDR_TAG_META_BUILD_IDENTIFIER missing\n");
	} else {
		SCSC_TAG_INFO(MXMAN, "FHDR_TAG_META_BUILD_IDENTIFIER: %.*s\n", len, val);
	}

	val = fhdr_lookup_tag(fw, FHDR_TAG_META_FW_COMMON_HASH, &len);
	if (val == NULL) {
		SCSC_TAG_ERR(MXMAN, "fhdr_lookup_tag FHDR_TAG_META_FW_COMMON_HASH missing\n");
	} else {
		SCSC_TAG_INFO(MXMAN, "FHDR_TAG_META_FW_COMMON_HASH: %.*s\n", len, val);
	}

	val = fhdr_lookup_tag(fw, FHDR_TAG_META_FW_WLAN_HASH, &len);
	if (val == NULL) {
		SCSC_TAG_ERR(MXMAN, "fhdr_lookup_tag FHDR_TAG_META_FW_WLAN_HASH missing\n");
	} else {
		SCSC_TAG_INFO(MXMAN, "FHDR_TAG_META_FW_WLAN_HASH: %.*s\n", len, val);
	}

	val = fhdr_lookup_tag(fw, FHDR_TAG_META_FW_BT_HASH, &len);
	if (val == NULL) {
		SCSC_TAG_ERR(MXMAN, "fhdr_lookup_tag FHDR_TAG_META_FW_BT_HASH missing\n");
	} else {
		SCSC_TAG_INFO(MXMAN, "FHDR_TAG_META_FW_BT_HASH: %.*s\n", len, val);
	}

	val = fhdr_lookup_tag(fw, FHDR_TAG_META_FW_PMU_HASH, &len);
	if (val == NULL) {
		SCSC_TAG_ERR(MXMAN, "fhdr_lookup_tag FHDR_TAG_META_FW_PMU_HASH missing\n");
	} else {
		SCSC_TAG_INFO(MXMAN, "FHDR_TAG_META_FW_PMU_HASH: %.*s\n", len, val);
	}

	val = fhdr_lookup_tag(fw, FHDR_TAG_META_HARDWARE_HASH, &len);
	if (val == NULL) {
		SCSC_TAG_ERR(MXMAN, "fhdr_lookup_tag FHDR_TAG_META_HARDWARE_HASH missing\n");
	} else {
		SCSC_TAG_INFO(MXMAN, "FHDR_TAG_META_HARDWARE_HASH: %.*s\n", len, val);
	}

	return 0;
}

int mxman_res_fw_init(struct mxman *mxman, struct fwhdr_if **fw_wlan, struct fwhdr_if **fw_wpan, void *start_dram,
		      size_t size_dram)
{
	int r;
	char *build_id;
	char *ttid;
	u32 fw_image_size;
	char *fw = start_dram;
	const struct firmware *firm;
	struct fwhdr_if *bhdr_if;
	struct fwhdr_if *whdr_if;
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);
	uint32_t wlan_length;
	uint32_t wlan_fw_runtime_size_val;
	const void *wlan_fw;
	uint32_t bt_length;
	const void *bt_fw;
	uint32_t log_strings_length;
	const void *log_strings;
	uint32_t pmu_length;
	struct pmu_firmware_header *pmu;
	uint32_t bt_fw_runtime_size_val;
	uint32_t bt_fw_offset_val;

	whdr_if = whdr_create();
	if (!whdr_if) {
		SCSC_TAG_ERR(MXMAN, "fwhdr_create() failed\n");
		return -ENOMEM;
	}

	bhdr_if = bhdr_create();
	if (!bhdr_if) {
		SCSC_TAG_ERR(MXMAN, "bwhdr_create() failed\n");
		return -ENOMEM;
	}

	*fw_wlan = whdr_if;
	*fw_wpan = bhdr_if;

	r = mx140_file_get_fw(mxman->mx, &firm);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "mx140_file_get_fw() failed (%d)\n", r);
		return r;
	}
	SCSC_TAG_INFO(MXMAN, "mx140_file_get_fw success,size %zu\n", firm->size);

	/* Print FW header strings */
	mxman_res_fw_init_get_fhdr_strings(firm->data);

	fw_image_size = firm->size;
	/* We need to read the FHDR_TAG_WLAN_FW tag */
	wlan_fw = fhdr_lookup_tag(firm->data, FHDR_TAG_WLAN_FW, &wlan_length);
	if (wlan_fw == NULL) {
		SCSC_TAG_ERR(MXMAN, "fhdr_lookup_tag FHDR_TAG_WLAN_FW failed\n");
		mx140_release_file(mxman->mx, firm);
		return -EINVAL;
	}

	r = whdr_if->init(whdr_if, (char *)wlan_fw, wlan_length, skip_header);
	if (r) {
		/* Check if we allow unidentified fw*/
		if (allow_unidentified_firmware && !whdr_if->get_parsed_ok(whdr_if)) {
			SCSC_TAG_INFO(MXMAN, "Unidentified firmware override\n");
			whdr_if->set_entry_point(whdr_if, 0);
			whdr_if->set_fw_rt_len(whdr_if, MX_FW_RUNTIME_LENGTH);
		} else {
			SCSC_TAG_ERR(MXMAN, "fwhdr_init() failed\n");
			mx140_release_file(mxman->mx, firm);
			return r;
		}
	}
	wlan_fw_runtime_size_val = whdr_if->get_fw_rt_len(whdr_if);

	/* Get BT image */
	bt_fw = fhdr_lookup_tag(firm->data, FHDR_TAG_BT_FW, &bt_length);
	SCSC_TAG_INFO(MXMAN, "BT %p\n", bt_fw);
	if (bt_fw == NULL) {
		SCSC_TAG_ERR(MXMAN, "fhdr_lookup_tag FHDR_TAG_BT_FW failed. Try to read WLAN\n");
		bhdr_destroy(bhdr_if);
		*fw_wpan = bhdr_if = NULL;
		mxman->wpan_present = false;
		goto cont_no_bt;
	}

	r = bhdr_if->init(bhdr_if, (char *)bt_fw, bt_length, skip_header);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "bwhdr_init() failed\n");
		return -EINVAL;
	}

	mxman->wpan_present = true;
	bt_fw_offset_val = bhdr_if->get_fw_offset(bhdr_if);
	bt_fw_runtime_size_val = bhdr_if->get_fw_rt_len(bhdr_if);

	/* Do size check before copying to DRAM */
	if ((bt_fw_runtime_size_val + wlan_fw_runtime_size_val) > size_dram) {
		SCSC_TAG_ERR(MXMAN, "firmware image too big for buffer (%zu > %u)", size_dram,
			     bt_fw_runtime_size_val + wlan_fw_runtime_size_val);
		mx140_release_file(mxman->mx, firm);
		return -EINVAL;
	}
	mif->remap_set(mif, (uintptr_t)bt_fw_offset_val, SCSC_MIF_ABS_TARGET_WPAN);

cont_no_bt:
	whdr_if->copy_fw(whdr_if, (char *)wlan_fw, wlan_length, start_dram);
	if (bhdr_if)
		bhdr_if->copy_fw(bhdr_if, (char *)bt_fw, bt_length, start_dram);

	/* set remaper address */
	mif->remap_set(mif, (uintptr_t)0x00000000, SCSC_MIF_ABS_TARGET_WLAN);

	/* Get log-string.bin */
	log_strings = fhdr_lookup_tag(firm->data, FHDR_TAG_WLAN_STRINGS, &log_strings_length);
	if (log_strings == NULL) {
		SCSC_TAG_ERR(MXMAN, "fhdr_lookup_tag FHDR_TAG_WLAN_STRINGS failed\n");
	} else {
		SCSC_TAG_INFO(MXMAN, "Loading WLAN log-strings %d\n", log_strings_length);
		mxlog_load_log_strings(scsc_mx_get_mxlog(mxman->mx), log_strings, log_strings_length);
	}

	log_strings = fhdr_lookup_tag(firm->data, FHDR_TAG_BT_STRINGS, &log_strings_length);
	if (log_strings == NULL) {
		SCSC_TAG_ERR(MXMAN, "fhdr_lookup_tag FHDR_TAG_BT_STRINGS failed\n");
	} else {
		SCSC_TAG_INFO(MXMAN, "Loading BT log-strings %d\n", log_strings_length);
		mxlog_load_log_strings(scsc_mx_get_mxlog_wpan(mxman->mx), log_strings, log_strings_length);
	}

	SCSC_TAG_ERR(MXMAN, "Search PMU in FWHR blob\n");
	pmu = (struct pmu_firmware_header *)fhdr_lookup_tag(firm->data, FHDR_TAG_PMU_FW, &pmu_length);
	if (pmu)
		goto found;

	SCSC_TAG_ERR(MXMAN, "Search PMU in WLAN blob\n");
#if defined(CONFIG_SOC_S5E8825)
	pmu = (struct pmu_firmware_header *)fw_obj_index_lookup_tag(wlan_fw, PMU_8051_INIT, &pmu_length);
#else
	pmu = (struct pmu_firmware_header *)fw_obj_index_lookup_tag(wlan_fw, PMU_CM0_INIT, &pmu_length);
#endif
	if (pmu == NULL) {
		SCSC_TAG_ERR(MXMAN, "PMU not found in WLAN blob or FWHRD\n");
		goto not_found;
	}
found:
	SCSC_TAG_INFO(MXMAN, "Loading PMU fw %u\n", pmu_length);
	SCSC_TAG_INFO(MXMAN, "PMU hdr_version %u size %u\n", pmu->hdr_version, pmu->hdr_size);
	SCSC_TAG_INFO(MXMAN, "PMU fw_offset %u fw_size %u\n", pmu->fw_offset, pmu->fw_size);
	SCSC_TAG_INFO(MXMAN, "PMU fw_patch version 0x%x\n", pmu->fw_patch_ver);
	/* Load the FW extracted in PMU image and copy to platform driver */
	mifpmuman_load_fw(scsc_mx_get_mifpmuman(mxman->mx), (int *)((uintptr_t)pmu + pmu->fw_offset), pmu->fw_size);

not_found:
	SCSC_TAG_INFO(MXMAN, "Releasing firm file\n");
	mx140_release_file(mxman->mx, firm);

	mxman->fw = fw;
	mxman->fw_image_size = fw_image_size;
	if (whdr_if->get_check_crc(whdr_if)) {
		/* do CRC on the entire image */
		r = whdr_if->do_fw_crc32_checks(whdr_if, true);
		if (r) {
			SCSC_TAG_ERR(MXMAN, "do_fw_crc32_checks() failed\n");
			return r;
		}
		whdr_if->crc_wq_start(whdr_if);
	}

	if (whdr_if->get_parsed_ok(whdr_if)) {
		build_id = whdr_if->get_build_id(whdr_if);
		if (build_id) {
			struct slsi_kic_service_info kic_info;

			(void)snprintf(mxman->fw_build_id, sizeof(mxman->fw_build_id), "%s", build_id);
			SCSC_TAG_INFO(MXMAN, "Firmware BUILD_ID: %s\n", mxman->fw_build_id);

			(void)snprintf(kic_info.ver_str, min(sizeof(mxman->fw_build_id), sizeof(kic_info.ver_str)),
				       "%s", mxman->fw_build_id);
			kic_info.fw_api_major = whdr_if->get_fwapi_major(whdr_if);
			kic_info.fw_api_minor = whdr_if->get_fwapi_minor(whdr_if);
			kic_info.release_product = SCSC_RELEASE_PRODUCT;
			kic_info.host_release_iteration = SCSC_RELEASE_ITERATION;
			kic_info.host_release_candidate = SCSC_RELEASE_CANDIDATE;

			slsi_kic_service_information(slsi_kic_technology_type_common, &kic_info);
		} else
			SCSC_TAG_ERR(MXMAN, "Failed to get Firmware BUILD_ID\n");

		ttid = whdr_if->get_ttid(whdr_if);
		if (ttid) {
			(void)snprintf(mxman->fw_ttid, sizeof(mxman->fw_ttid), "%s", ttid);
			SCSC_TAG_INFO(MXMAN, "Firmware ttid: %s\n", mxman->fw_ttid);
		}
	}

	SCSC_TAG_DEBUG(MXMAN, "firmware_entry_point=0x%x fw_runtime_length=%d\n", whdr_if->get_entry_point(whdr_if),
		       whdr_if->get_fw_rt_len(whdr_if));

	return 0;
}
#ifdef CONFIG_SOC_S5E8825
int mxman_res_pmu_init(struct mxman *mxman, mifpmuisr_handler handler)
#else
int mxman_res_pmu_init(struct mxman *mxman)
#endif
{
	struct scsc_mif_abs *mif;
	mif = scsc_mx_get_mif_abs(mxman->mx);

#ifdef CONFIG_SOC_S5E8825
	mifpmuman_init(scsc_mx_get_mifpmuman(mxman->mx), mif, handler, mxman);
#else
	mifpmuman_init(scsc_mx_get_mifpmuman(mxman->mx), mif, NULL, NULL);
#endif

	return 0;
}

int mxman_res_pmu_deinit(struct mxman *mxman)
{
	mifpmuman_deinit(scsc_mx_get_mifpmuman(mxman->mx));
	return 0;
}

int mxman_res_pmu_boot(struct mxman *mxman, enum scsc_subsystem sub)
{
	int r;
#ifdef CONFIG_SOC_S5E8825
	/* This should be a blocking call, mifpmu should do the waitqueue */
	r = mifpmuman_start_subsystem(scsc_mx_get_mifpmuman(mxman->mx), sub);
	if (r)
		SCSC_TAG_INFO(MXMAN, "PMU error\n");
	return r;
#else
        enum pmu_msg msg;

        switch (sub) {
        case SCSC_SUBSYSTEM_WLAN:
                SCSC_TAG_INFO(MXMAN, "Booting WLAN subsystem\n");
                msg = PMU_AP_MB_MSG_START_WLAN;
                break;
        case SCSC_SUBSYSTEM_WPAN:
                SCSC_TAG_INFO(MXMAN, "Booting WPAN subsystem\n");
                msg = PMU_AP_MB_MSG_START_WPAN;
                break;
        default:
                SCSC_TAG_ERR(MXMAN, "Subsystem %d not found\n", sub);
                return -EIO;
        }

        /* This should be a blocking call, mifpmu should do the waitqueue */
        r = mifpmuman_send_msg(scsc_mx_get_mifpmuman(mxman->mx), msg);
        if (r) {
                SCSC_TAG_INFO(MXMAN, "PMU error\n");
                return r;
        }
        return 0;
#endif
}

#ifdef CONFIG_SOC_S5E8825
int mxman_res_pmu_reset(struct mxman *mxman, enum scsc_subsystem sub)
{
	int r;

	/* This should be a blocking call, mifpmu should do the waitqueue */
	r = mifpmuman_stop_subsystem(scsc_mx_get_mifpmuman(mxman->mx), sub);
	if (r)
		SCSC_TAG_INFO(MXMAN, "PMU error\n");
	return r;
}
#else
int mxman_res_pmu_reset(struct mxman *mxman, enum scsc_subsystem sub)
{
	enum pmu_msg msg;
	int r;

	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
		SCSC_TAG_INFO(MXMAN, "Stopping WLAN subsystem\n");
		msg = PMU_AP_MB_MSG_RESET_WLAN;
		break;
	case SCSC_SUBSYSTEM_WPAN:
		SCSC_TAG_INFO(MXMAN, "Stopping WPAN subsystem\n");
		msg = PMU_AP_MB_MSG_RESET_WPAN;
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Subsystem %d not found\n", sub);
		return -EIO;
	}

	/* This should be a blocking call, mifpmu should do the waitqueue */
	r = mifpmuman_send_msg(scsc_mx_get_mifpmuman(mxman->mx), msg);
	if (r) {
		SCSC_TAG_INFO(MXMAN, "PMU error\n");
		return r;
	}
	return 0;
}
#endif

#ifdef CONFIG_SOC_S5E8825
int mxman_res_pmu_monitor(struct mxman *mxman, enum scsc_subsystem sub)
{
	int r;

	/* This should be a blocking call, mifpmu should do the waitqueue */
	r = mifpmuman_force_monitor_mode_subsystem(scsc_mx_get_mifpmuman(mxman->mx), sub);
	if (r)
		SCSC_TAG_INFO(MXMAN, "PMU error\n");
	return r;
}
#endif

static int mxman_res_transports_deinit_wlan(struct mxman *mxman)
{
	mxlog_transport_release(scsc_mx_get_mxlog_transport(mxman->mx));
	mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mxman->mx));
	gdb_transport_release(scsc_mx_get_gdb_transport_wlan(mxman->mx));
	gdb_transport_release(scsc_mx_get_gdb_transport_fxm_1(mxman->mx));
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	gdb_transport_release(scsc_mx_get_gdb_transport_fxm_2(mxman->mx));
#endif
	if (mxman->data_mxconf){
		miframman_free(scsc_mx_get_ramman(mxman->mx), mxman->data_mxconf);
		mxman->data_mxconf = NULL;
	}

	mxlog_release(scsc_mx_get_mxlog(mxman->mx));
	/* unregister channel handler */
	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mxman->mx),
						  MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, NULL, NULL);
	return 0;
}

static int mxman_res_transports_deinit_wpan(struct mxman *mxman)
{
	mxlog_transport_release(scsc_mx_get_mxlog_transport_wpan(mxman->mx));
	mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport_wpan(mxman->mx));
	gdb_transport_release(scsc_mx_get_gdb_transport_wpan(mxman->mx));
	if (mxman->data_mxconf_wpan){
		miframman_free(scsc_mx_get_ramman_wpan(mxman->mx), mxman->data_mxconf_wpan);
		mxman->data_mxconf_wpan = NULL;
	}

	mxlog_release(scsc_mx_get_mxlog_wpan(mxman->mx));
	/* unregister channel handler */
	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport_wpan(mxman->mx),
						  MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, NULL, NULL);
	return 0;
}

int mxman_res_deinit_subsystem(struct mxman *mxman, enum scsc_subsystem sub)
{
	SCSC_TAG_INFO(MXMAN, "Deinit %s subsystem\n", sub ? "WPAN" : "WLAN");

	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
		mxfwconfig_unload(mxman->mx);
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		mxlogger_stop_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WLAN);
		mxlogger_deinit_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WLAN);
#endif
		mxman_res_transports_deinit_wlan(mxman);
		/* Deinit WLAN MIF interrupt */
		mifintrbit_deinit(scsc_mx_get_intrbit(mxman->mx), SCSC_MIF_ABS_TARGET_WLAN);
		mifmboxman_deinit(scsc_mx_get_mboxman(mxman->mx));
		break;
	case SCSC_SUBSYSTEM_WPAN:
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		mxlogger_stop_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WPAN);
		mxlogger_deinit_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WPAN);
#endif
		mxman_res_transports_deinit_wpan(mxman);
		/* Deinit WLAN MIF interrupt */
		mifintrbit_deinit(scsc_mx_get_intrbit_wpan(mxman->mx), SCSC_MIF_ABS_TARGET_WPAN);
		mifmboxman_deinit(scsc_mx_get_mboxman_wpan(mxman->mx));
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Subsystem %d not found\n", sub);
		return -EIO;
	}

	return 0;
}

#if !IS_ENABLED(CONFIG_SCSC_PCIE_PAEAN_X86) && !IS_ENABLED(CONFIG_SOC_S5E9925)
static void mxman_res_mbox_init_wlan(struct mxman *mxman, u32 firmware_entry_point)
{
	u32 *mbox0;
	u32 *mbox1;
	u32 *mbox2;
	u32 *mbox3;
	scsc_mifram_ref mifram_ref;
	struct scsc_mx *mx = mxman->mx;
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);

	/* Place firmware entry address in MIF MBOX 0 so R4 ROM knows where to jump to! */
	mbox0 = mifmboxman_get_mbox_ptr(scsc_mx_get_mboxman(mx), mif, MBOX_INDEX_0);
	mbox1 = mifmboxman_get_mbox_ptr(scsc_mx_get_mboxman(mx), mif, MBOX_INDEX_1);

	/* Write (and flush) entry point to MailBox 0, config address to MBOX 1 */
	*mbox0 = 0xcafecafe;
	mif->get_mifram_ref(mif, mxman->mxconf, &mifram_ref);
	*mbox1 = mifram_ref; /* must be R4-relative address here */

	/*
	 * write the magic number "0xbcdeedcb" to MIF Mailbox #2 &
	 * copy the firmware_startup_flags to MIF Mailbox #3 before starting (reset = 0) the R4
	 */
	mbox2 = mifmboxman_get_mbox_ptr(scsc_mx_get_mboxman(mx), mif, MBOX_INDEX_2);
	*mbox2 = MBOX2_MAGIC_NUMBER;
	mbox3 = mifmboxman_get_mbox_ptr(scsc_mx_get_mboxman(mx), mif, MBOX_INDEX_3);
	*mbox3 = firmware_startup_flags;

	/* CPU memory barrier */
	smp_wmb();
}
#endif

static int mxman_res_transports_init_wlan(struct mxman *mxman, void *data, size_t data_sz,
					  mxmgmt_channel_handler handler)
{
	struct mxconf *mxconf;
	int r;
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);
	struct scsc_mx *mx = mxman->mx;
#if !IS_ENABLED(CONFIG_SCSC_PCIE_PAEAN_X86) && !IS_ENABLED(CONFIG_SOC_S5E9925)
	struct fwhdr_if *whdr_if = mxman->fw_wlan;
#endif

	/* Initialise mx management stack */
	r = mxmgmt_transport_init(scsc_mx_get_mxmgmt_transport(mx), mx, SCSC_MIF_ABS_TARGET_WLAN);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "mxmgmt_transport_init() failed %d\n", r);
		return r;
	}
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	r = mxlogger_init_transport_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WLAN);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "mxlogger_init_transport_channel() failed %d for WLAN\n", r);
		/* Ignore return value */
	}
#endif
	/* Initialise gdb transport for cortex-R4 */
	r = gdb_transport_init(scsc_mx_get_gdb_transport_wlan(mx), mx, GDB_TRANSPORT_WLAN);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "gdb_transport_init() failed %d\n", r);
		mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mx));
		return r;
	}

	/* Initialise gdb transport for cortex-M4 */
	r = gdb_transport_init(scsc_mx_get_gdb_transport_fxm_1(mx), mx, GDB_TRANSPORT_FXM_1);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "gdb_transport_init() failed %d\n", r);
		gdb_transport_release(scsc_mx_get_gdb_transport_wlan(mx));
		mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mx));
		return r;
	}
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	/* Initialise gdb transport for cortex-M4 */
	r = gdb_transport_init(scsc_mx_get_gdb_transport_fxm_2(mx), mx, GDB_TRANSPORT_FXM_2);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "gdb_transport_init() failed %d\n", r);
		gdb_transport_release(scsc_mx_get_gdb_transport_wlan(mx));
		mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mx));
		return r;
	}
#endif

	/* Initialise mxlog transport */
	r = mxlog_transport_init(scsc_mx_get_mxlog_transport(mx), mx);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "mxlog_transport_init() failed %d\n", r);
		gdb_transport_release(scsc_mx_get_gdb_transport_fxm_1(mx));
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
		gdb_transport_release(scsc_mx_get_gdb_transport_fxm_2(mx));
#endif
		gdb_transport_release(scsc_mx_get_gdb_transport_wlan(mx));
		mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mx));
		return r;
	}

#if 0
	/*
	 * Allocate & Initialise Infrastructre Config Structure
	 * including the mx management stack config information.
	 */
	mxconf = miframman_alloc(scsc_mx_get_ramman(mx), sizeof(struct mxconf), 4, MIFRAMMAN_OWNER_COMMON);
	if (!mxconf) {
		goto error_alloc;
	}
	mxman->mxconf = mxconf;
#endif
	/* mxconf is preallocated when initializing the mem layout */
	mxconf = mxman->mxconf;

	/* Allocate and copy arbiratry service data if exists */
	if (data && data_sz) {
		mxman->data_mxconf = miframman_alloc(scsc_mx_get_ramman(mx), data_sz, 4, MIFRAMMAN_OWNER_COMMON);
		if (!mxman->data_mxconf) {
			miframman_free(scsc_mx_get_ramman(mxman->mx), mxman->mxconf);
			goto error_alloc;
		}
		memcpy(mxman->data_mxconf, data, data_sz);
		mif->get_mifram_ref(mif, mxman->data_mxconf, &mxconf->subsysconf_offset);
		mxconf->subsysconf_length = data_sz;
		SCSC_TAG_INFO(MXMAN, "mxconf subsysconf_offset %d subsysconf_length %d\n", mxconf->subsysconf_offset,
			      mxconf->subsysconf_length);
	} else {
		mxman->data_mxconf = NULL;
	}

	mxconf->magic = MXCONF_MAGIC;
	mxconf->version.major = MXCONF_VERSION_MAJOR;

	mxconf->version.minor = MXCONF_VERSION_MINOR;
	/* Pass pre-existing FM status to FW */
	mxconf->flags = 0;
#if IS_ENABLED(CONFIG_SCSC_FM)
	mxconf->flags |= mxman->on_halt_ldos_on ? MXCONF_FLAGS_FM_ON : 0;
#endif
	SCSC_TAG_INFO(MXMAN, "mxconf flags 0x%08x\n", mxconf->flags);

	/* serialise mxmgmt transport */
	mxmgmt_transport_config_serialise(scsc_mx_get_mxmgmt_transport(mx), &mxconf->mx_trans_conf);
	/* serialise Cortex-R4 gdb transport */
	gdb_transport_config_serialise(scsc_mx_get_gdb_transport_wlan(mx), &mxconf->monitor_c0_trans_conf);
	/* serialise Cortex-M4 gdb transport */
	gdb_transport_config_serialise(scsc_mx_get_gdb_transport_fxm_1(mx), &mxconf->monitor_c1_trans_conf);

	/* Default to Fleximac M4_1 monitor channel not in use.
	 * Allows CONFIG_SCSC_MX450_GDB_SUPPORT to be turned off in Kconfig even though mxconf
	 * struct v5 defines M4_1 channel
	 */
	mxconf->monitor_c2_trans_conf.from_ap_stream_conf.buf_conf.buffer_loc = 0;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	/* serialise Cortex-M4 gdb transport */
	gdb_transport_config_serialise(scsc_mx_get_gdb_transport_fxm_2(mx), &mxconf->monitor_c2_trans_conf);
#endif
	/* serialise mxlog transport */
	mxlog_transport_config_serialise(scsc_mx_get_mxlog_transport(mx), &mxconf->mxlogconf);
	SCSC_TAG_DEBUG(
		MXMAN,
		"read_bit_idx=%d write_bit_idx=%d buffer=%p num_packets=%d packet_size=%d read_index=%d write_index=%d\n",
		scsc_mx_get_mxlog_transport(mx)->mif_stream.read_bit_idx,
		scsc_mx_get_mxlog_transport(mx)->mif_stream.write_bit_idx,
		scsc_mx_get_mxlog_transport(mx)->mif_stream.buffer.buffer,
		scsc_mx_get_mxlog_transport(mx)->mif_stream.buffer.num_packets,
		scsc_mx_get_mxlog_transport(mx)->mif_stream.buffer.packet_size,
		*scsc_mx_get_mxlog_transport(mx)->mif_stream.buffer.read_index,
		*scsc_mx_get_mxlog_transport(mx)->mif_stream.buffer.write_index);

	/* Need to initialise fwconfig or else random data can make firmware data abort. */
	mxconf->fwconfig.offset = 0;
	mxconf->fwconfig.size = 0;

	mxconf->mxlogger_area_offset = 0;
	mxconf->mxlogger_area_length = 0;
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	mxconf->mxlogger_area_offset =
		mxlogger_get_channel_ref(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WLAN);
	mxconf->mxlogger_area_length =
		mxlogger_get_channel_len(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WLAN);
#endif
	SCSC_TAG_INFO(MXMAN, "mxlogger_area_offset  0x%x mxlogger_area_length 0x%x\n", mxconf->mxlogger_area_offset,
		     mxconf->mxlogger_area_length);

#ifdef CONFIG_SCSC_COMMON_HCF
	/* Load Common Config HCF */
	mxfwconfig_load(mxman->mx, &mxconf->fwconfig);
#endif

#if !IS_ENABLED(CONFIG_SCSC_PCIE_PAEAN_X86) && !IS_ENABLED(CONFIG_SOC_S5E9925)
	mxman_res_mbox_init_wlan(mxman, whdr_if->get_entry_point(whdr_if));
#endif

	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mxman->mx),
						  MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, handler, mxman);
	mxlog_init(scsc_mx_get_mxlog(mxman->mx), mxman->mx, mxman->fw_build_id, SCSC_MIF_ABS_TARGET_WLAN);

	return 0;

error_alloc:
	SCSC_TAG_ERR(MXMAN, "miframman_alloc() failed\n");
	gdb_transport_release(scsc_mx_get_gdb_transport_fxm_1(mx));
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	gdb_transport_release(scsc_mx_get_gdb_transport_fxm_2(mx));
#endif
	gdb_transport_release(scsc_mx_get_gdb_transport_wlan(mx));
	mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport(mx));
	mxlog_transport_release(scsc_mx_get_mxlog_transport(mx));
	return -ENOMEM;
}

static int mxman_res_transports_init_wpan(struct mxman *mxman, void *data, size_t data_sz,
					  mxmgmt_channel_handler handler)
{
	struct mxconf *mxconf_wpan;
	int r;
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mxman->mx);
	struct scsc_mx *mx = mxman->mx;

	/* Initialise mx management stack for WPAN  */
	r = mxmgmt_transport_init(scsc_mx_get_mxmgmt_transport_wpan(mx), mx, SCSC_MIF_ABS_TARGET_WPAN);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "mxmgmt_transport_init() failed %d for WPAN\n", r);
		return r;
	}
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	r = mxlogger_init_transport_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WPAN);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "mxlogger_init_transport_channel() failed %d for WPAN\n", r);
		/* Ignore return value */
	}
#endif
	/* Initialise gdb transport for cortex-WPAN */
	r = gdb_transport_init(scsc_mx_get_gdb_transport_wpan(mx), mx, GDB_TRANSPORT_WPAN);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "gdb_transport_init() failed %d\n", r);
		mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport_wpan(mx));
		return r;
	}

	/* Initialise mxlog transport */
	r = mxlog_transport_init_wpan(scsc_mx_get_mxlog_transport_wpan(mx), mx);
	if (r) {
		SCSC_TAG_ERR(MXMAN, "mxlog_transport_init() failed %d\n", r);
		gdb_transport_release(scsc_mx_get_gdb_transport_wpan(mx));
		mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport_wpan(mx));
		return r;
	}

	/*
	 * Allocate & Initialise Infrastructre Config Structure
	 * including the mx management stack config information.
	 */
	/* mxconf is preallocated when initializing the mem layout */
	mxconf_wpan = mxman->mxconf_wpan;

	/* Allocate and copy arbiratry service data if exists */
	if (data && data_sz) {
		mxman->data_mxconf_wpan =
			miframman_alloc(scsc_mx_get_ramman_wpan(mx), data_sz, 4, MIFRAMMAN_OWNER_COMMON);
		if (!mxman->data_mxconf_wpan) {
			miframman_free(scsc_mx_get_ramman_wpan(mxman->mx), mxman->mxconf_wpan);
			goto error_alloc;
		}
		memcpy(mxman->data_mxconf_wpan, data, data_sz);
		mif->get_mifram_ref(mif, mxman->data_mxconf_wpan, &mxconf_wpan->subsysconf_offset);
		mxconf_wpan->subsysconf_length = data_sz;
		SCSC_TAG_INFO(MXMAN, "mxconf_wpan subsysconf_offset %d subsysconf_length %d\n",
			      mxconf_wpan->subsysconf_offset, mxconf_wpan->subsysconf_length);
	} else {
		mxman->data_mxconf_wpan = NULL;
	}

	mxconf_wpan->magic = MXCONF_MAGIC;
	mxconf_wpan->version.major = MXCONF_VERSION_MAJOR;

	mxconf_wpan->version.minor = MXCONF_VERSION_MINOR;
	/* Pass pre-existing FM status to FW */
	mxconf_wpan->flags = 0;
#if IS_ENABLED(CONFIG_SCSC_FM)
	mxconf_wpan->flags |= mxman->on_halt_ldos_on ? MXCONF_FLAGS_FM_ON : 0;
#endif
	SCSC_TAG_INFO(MXMAN, "mxconf_wpan flags 0x%08x\n", mxconf_wpan->flags);

	/* serialise mxmgmt transport */
	mxmgmt_transport_config_serialise(scsc_mx_get_mxmgmt_transport_wpan(mx), &mxconf_wpan->mx_trans_conf);
	/* serialise WPAN gdb transport */
	gdb_transport_config_serialise(scsc_mx_get_gdb_transport_wpan(mx), &mxconf_wpan->monitor_c0_trans_conf);

	/* serialise mxlog transport */
	mxlog_transport_config_serialise(scsc_mx_get_mxlog_transport_wpan(mx), &mxconf_wpan->mxlogconf);
	SCSC_TAG_DEBUG(
		MXMAN,
		"read_bit_idx=%d write_bit_idx=%d buffer=%p num_packets=%d packet_size=%d read_index=%d write_index=%d\n",
		scsc_mx_get_mxlog_transport_wpan(mx)->mif_stream.read_bit_idx,
		scsc_mx_get_mxlog_transport_wpan(mx)->mif_stream.write_bit_idx,
		scsc_mx_get_mxlog_transport_wpan(mx)->mif_stream.buffer.buffer,
		scsc_mx_get_mxlog_transport_wpan(mx)->mif_stream.buffer.num_packets,
		scsc_mx_get_mxlog_transport_wpan(mx)->mif_stream.buffer.packet_size,
		*scsc_mx_get_mxlog_transport_wpan(mx)->mif_stream.buffer.read_index,
		*scsc_mx_get_mxlog_transport_wpan(mx)->mif_stream.buffer.write_index);

	/* Need to initialise fwconfig or else random data can make firmware data abort. */
	mxconf_wpan->fwconfig.offset = 0;
	mxconf_wpan->fwconfig.size = 0;

	mxconf_wpan->mxlogger_area_offset = 0;
	mxconf_wpan->mxlogger_area_length = 0;
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	mxconf_wpan->mxlogger_area_offset =
		mxlogger_get_channel_ref(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WPAN);
	mxconf_wpan->mxlogger_area_length =
		mxlogger_get_channel_len(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WPAN);
#endif
	SCSC_TAG_INFO(MXMAN, "mxlogger_area_offset  0x%x mxlogger_area_length 0x%x\n", mxconf_wpan->mxlogger_area_offset,
		     mxconf_wpan->mxlogger_area_length);

	SCSC_TAG_INFO(MXMAN, "mxconfig in DRAM\n");

	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport_wpan(mxman->mx),
						  MMTRANS_CHAN_ID_MAXWELL_MANAGEMENT, handler, mxman);
	mxlog_init(scsc_mx_get_mxlog_wpan(mxman->mx), mxman->mx, mxman->fw_build_id, SCSC_MIF_ABS_TARGET_WPAN);

	return 0;

error_alloc:
	SCSC_TAG_ERR(MXMAN, "miframman_alloc() failed\n");
	gdb_transport_release(scsc_mx_get_gdb_transport_wpan(mx));
	mxmgmt_transport_release(scsc_mx_get_mxmgmt_transport_wpan(mx));
	mxlog_transport_release(scsc_mx_get_mxlog_transport_wpan(mx));
	return -ENOMEM;
}

/* Initialize common components/subystems */
int mxman_res_init_common(struct mxman *mxman)
{
	struct scsc_mif_abs *mif;
	int ret = 0;

	SCSC_TAG_INFO(MXMAN, "Init common components\n");
	mif = scsc_mx_get_mif_abs(mxman->mx);
#ifdef CONFIG_SCSC_SMAPPER
	/* Initialize SMAPPER */
	ret = mifsmapper_init(scsc_mx_get_smapper(mxman->mx), mif);
	if(ret)
		return ret;
#endif
#ifdef CONFIG_SCSC_QOS
	ret = mifqos_init(scsc_mx_get_qos(mxman->mx), mif);
	if(ret)
		return ret;
#endif
#ifdef CONFIG_SCSC_LAST_PANIC_IN_DRAM
	ret = scsc_log_in_dram_mmap_create();
	if(ret)
		return ret;
#endif
	return ret;
}

/* Deinitialize common components/subystems */
int mxman_res_deinit_common(struct mxman *mxman)
{
	int ret = 0;

	SCSC_TAG_INFO(MXMAN, "Deinit common components\n");
#ifdef CONFIG_SCSC_SMAPPER
	ret = mifsmapper_deinit(scsc_mx_get_smapper(mxman->mx));
	if(ret)
		return ret;
#endif
#ifdef CONFIG_SCSC_QOS
	ret = mifqos_deinit(scsc_mx_get_qos(mxman->mx));
	if(ret)
		return ret;
#endif
#ifdef CONFIG_SCSC_LAST_PANIC_IN_DRAM
	ret = scsc_log_in_dram_mmap_destroy();
	if(ret)
		return ret;
#endif
	return ret;
}

int mxman_res_init_subsystem(struct mxman *mxman, enum scsc_subsystem sub, void *data, size_t data_sz,
			     mxmgmt_channel_handler handler)
{
	struct scsc_mif_abs *mif;
	int r = -EIO;

	mif = scsc_mx_get_mif_abs(mxman->mx);

	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
		mxfwconfig_init(mxman->mx);
		/* INTMIF init before allocating transports */
		mifintrbit_init(scsc_mx_get_intrbit(mxman->mx), mif, SCSC_MIF_ABS_TARGET_WLAN);
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		/* Mxlogger init before allocating transports*/
		/* Ignore return value */
		mxlogger_init_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WLAN);
#endif
		r = mxman_res_transports_init_wlan(mxman, data, data_sz, handler);
		break;
	case SCSC_SUBSYSTEM_WPAN:
		/* INTMIF init before allocating transports */
		mifintrbit_init(scsc_mx_get_intrbit_wpan(mxman->mx), mif, SCSC_MIF_ABS_TARGET_WPAN);
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		/* Mxlogger init before allocating transports*/
		mxlogger_init_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WPAN);
#endif
		r = mxman_res_transports_init_wpan(mxman, data, data_sz, handler);
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Subsystem %d not found\n", sub);
		return -EIO;
	}

	return r;
}

int mxman_res_reset(struct mxman *mxman, bool reset)
{
	int r;
	struct scsc_mif_abs *mif;

	mif = scsc_mx_get_mif_abs(mxman->mx);

	r = mif->reset(mif, reset);
	if (r && !reset) {
		SCSC_TAG_ERR(MXMAN, "HW reset deassertion failed\n");
		return r;
	} else if (r && reset) {
		SCSC_TAG_ERR(MXMAN, "HW reset assertion failed\n");
		return r;
	}

	return 0;
}

int mxman_res_post_init_subsystem(struct mxman *mxman, enum scsc_subsystem sub)
{
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	int r;
#endif
	switch (sub) {
	case SCSC_SUBSYSTEM_WLAN:
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		r = mxlogger_start_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WLAN);
		if (r) {
			mxlogger_deinit_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WPAN);
		}
#endif
		break;
	case SCSC_SUBSYSTEM_WPAN:
#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
		r = mxlogger_start_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WPAN);
		if (r) {
			mxlogger_deinit_channel(scsc_mx_get_mxlogger(mxman->mx), SCSC_MIF_ABS_TARGET_WPAN);
		}
#endif
		break;
	default:
		return -EIO;
	}
	return 0;
}

int mxman_res_init(struct mxman *mxman)
{
	return 0;
}
