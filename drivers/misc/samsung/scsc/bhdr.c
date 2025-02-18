/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/kernel.h>
#include <scsc/scsc_logring.h>
#include <linux/slab.h>
#include <scsc/scsc_logring.h>
#include "fwhdr_if.h"
#include "bhdr.h"

/** Definition of tags */
enum bhdr_tag {
	/**
         * The total length of the entire BHDR this tag is contained in, including the size of all
         * tag, length and value fields.
         *
         * This special tag must be present as first item for validation/integrity check purposes,
         * and to be able to determine the overall length of the BHDR.
         *
         * Value length: 4 byte
         * Value encoding: uint32_t
         */
	BHDR_TAG_TOTAL_LENGTH = 0x52444842, /* "BHDR" */

	/**
         * Offset of location to load the Bluetooth firmware binary image. The offset is relative to
         * begining of the entire memory region that is shared between the AP and WLBT.
         *
         * Value length: 4 byte
         * Value encoding: uint32_t
         */
	BHDR_TAG_FW_OFFSET = 1,

	/**
         * Runtime size of firmware
         *
         * This accommodates not only the Bluetooth firmware binary image itself, but also any
         * statically allocated memory (such as zero initialised static variables) immediately
         * following it that is used by the firmware at runtime after it has booted.
         *
         * Value length: 4 byte
         * Value encoding: uint32_t
         */
	BHDR_TAG_FW_RUNTIME_SIZE = 2,

	/**
         * Firmware build identifier string
         *
         * Value length: N byte
         * Value encoding: uint8_t[N]
         */
	BHDR_TAG_FW_BUILD_ID = 3,

	/**
	 * Offset of location of system error record. The offset is relative to the beginning of the
	 * entire memory region that is shared between the AP and WLBT.
	 *
	 * Value length: 4 byte
	 * Value encoding: uint32_t
	 */
	BHDR_TAG_SYSTEM_ERROR_RECORD_OFFSET = 4,
};

/**
 * Structure describing the tag and length of each item in the BHDR.
 * The value of the item follows immediately after.
 */
struct bhdr_tag_length {
	uint32_t tag; /* One of bhdr_tag */
	uint32_t length; /* Length of value in bytes */
};

/* bthdr */
/**
 * Maximum length of build identifier including 0 termination.
 *
 * The length of the value of BHDR_TAG_FW_BUILD_ID is fixed to this value instead of the possibly
 * varying length of the build identifier string, so that the length of the firmware header doesn't
 * change for different build identifier strings in an otherwise identical firmware build.
 */
#define FIRMWARE_BUILD_ID_MAX_LENGTH 128

struct bt_firmware_header {
	/* BHDR_TAG_TOTAL_LENGTH  */
	struct bhdr_tag_length total_length_tl;
	uint32_t total_length_val;

	/* BHDR_TAG_FW_OFFSET */
	struct bhdr_tag_length fw_offset_tl;
	uint32_t fw_offset_val;

	/* BHDR_TAG_FW_RUNTIME_SIZE */
	struct bhdr_tag_length fw_runtime_size_tl;
	uint32_t fw_runtime_size_val;

	/* BHDR_TAG_FW_BUILD_ID */
	struct bhdr_tag_length fw_build_id_tl;
	const char fw_build_id_val[FIRMWARE_BUILD_ID_MAX_LENGTH];
};

const void *bhdr_lookup_tag(const void *blob, enum bhdr_tag tag, uint32_t *length);

struct bhdr {
	struct fwhdr_if fw_if;
	uint32_t bt_fw_offset_val;
	uint32_t bt_fw_runtime_size_val;
	uint32_t bt_panic_record_offset;
	char *fw_dram_addr;
	uint32_t fw_size;
};

#define bhdr_from_fwhdr_if(FWHDR_IF_PTR) container_of(FWHDR_IF_PTR, struct bhdr, fw_if)

/** Return the next item after a given item and decrement the remaining length */
static const struct bhdr_tag_length *bhdr_next_item(const struct bhdr_tag_length *item, uint32_t *bhdr_length)
{
	uint32_t skip_length = sizeof(*item) + item->length;

	if (skip_length < *bhdr_length) {
		*bhdr_length -= skip_length;
		return (const struct bhdr_tag_length *)((uintptr_t)item + skip_length);
	}

	return NULL;
}

/** Return the value of an item */
static const void *bhdr_item_value(const struct bhdr_tag_length *item)
{
	SCSC_TAG_DBG4(FW_LOAD, "bhdr_lookup_tag %p\n", item);
	SCSC_TAG_DBG4(FW_LOAD, "bhdr_lookup_tag current tag %u lenght %u\n", item->tag, item->length);
	return (const void *)((uintptr_t)item + sizeof(*item));
}

const void *bhdr_lookup_tag(const void *blob, enum bhdr_tag tag, uint32_t *length)
{
	const struct bhdr_tag_length *bhdr = (const struct bhdr_tag_length *)((char *)blob + 8);
	const struct bhdr_tag_length *item = bhdr;
	uint32_t bhdr_length;

	if (bhdr->tag != BHDR_TAG_TOTAL_LENGTH) {
		SCSC_TAG_WARNING(FW_LOAD, "Unexpected first tag in bhdr: %u \n", item->tag);
		return NULL;
	}

	/* First item is BHCD_TAG_TOTAL_LENGTH which contains overall length of the BHCD */
	bhdr_length = *(const uint32_t *)bhdr_item_value(bhdr);

	SCSC_TAG_DBG4(FW_LOAD, "bhdr_lookup_tag %u\n", tag);

	while (item != NULL) {
		if (item->tag == tag) {
			if (length != NULL) {
				*length = item->length;
			}
			SCSC_TAG_DBG4(FW_LOAD, "bhdr_lookup_tag %u found with size %u\n", item->tag, item->length);
			return bhdr_item_value(item);
		}
		item = bhdr_next_item(item, &bhdr_length);
	}

	return NULL;
}

static int bhdr_copy_fw(struct fwhdr_if *interface, char *fw_data, size_t fw_size, void *dram_addr)
{
	struct bhdr *bhdr = bhdr_from_fwhdr_if(interface);

	bhdr->fw_dram_addr = (char *)dram_addr;
	bhdr->fw_size = fw_size;

	memcpy((void *)((uintptr_t)dram_addr + bhdr->bt_fw_offset_val), fw_data, fw_size);

	/* Bit of 'paranoia' here, but make sure FW is copied over and visible
	 * for all CPUs*/
	smp_mb();

	return 0;
}

static int bhdr_init(struct fwhdr_if *interface, char *fw_data, size_t fw_len, bool skip_header)
{
	const void *bt_fw;
	const void *bt_res;
	struct bhdr *bhdr = bhdr_from_fwhdr_if(interface);

	bt_fw = (const void *)((uintptr_t)fw_data);
	bt_res = bhdr_lookup_tag(bt_fw, BHDR_TAG_FW_OFFSET, NULL);
	if (!bt_res)
		return -EIO;
	bhdr->bt_fw_offset_val = *(uint32_t *)bt_res;
	bt_res = bhdr_lookup_tag(bt_fw, BHDR_TAG_FW_RUNTIME_SIZE, NULL);
	if (!bt_res)
		return -EIO;
	bhdr->bt_fw_runtime_size_val = *(uint32_t *)bt_res;

	bt_res = bhdr_lookup_tag(bt_fw, BHDR_TAG_SYSTEM_ERROR_RECORD_OFFSET, NULL);
	if (!bt_res)
		return -EIO;
	bhdr->bt_panic_record_offset = *(uint32_t *)bt_res;

	SCSC_TAG_DBG4(FW_LOAD, "BT fw_len 0x%x runtime_size 0x%x offset 0x%x panic_record_val 0x%x from start of Shared DRAM\n", fw_len,
		      bhdr->bt_fw_runtime_size_val, bhdr->bt_fw_offset_val, bhdr->bt_panic_record_offset);

	return 0;
}

static u32 bhdr_get_fw_rt_len(struct fwhdr_if *interface)
{
	struct bhdr *bhdr = bhdr_from_fwhdr_if(interface);

	return bhdr->bt_fw_runtime_size_val;
}

static u32 bhdr_get_fw_offset(struct fwhdr_if *interface)
{
	struct bhdr *bhdr = bhdr_from_fwhdr_if(interface);

	return bhdr->bt_fw_offset_val;
}

static u32 bhdr_get_panic_record_offset(struct fwhdr_if *interface, enum scsc_mif_abs_target target)
{
	struct bhdr *bhdr = bhdr_from_fwhdr_if(interface);

	return bhdr->bt_panic_record_offset;
}

/* Implementation creation */
struct fwhdr_if *bhdr_create(void)
{
	struct fwhdr_if *fw_if;
	struct bhdr *bhdr = kzalloc(sizeof(struct bhdr), GFP_KERNEL);

	if (!bhdr)
		return NULL;

	fw_if = &bhdr->fw_if;
	fw_if->init = bhdr_init;
	fw_if->get_panic_record_offset = bhdr_get_panic_record_offset;
	fw_if->get_fw_rt_len = bhdr_get_fw_rt_len;
	fw_if->get_fw_offset = bhdr_get_fw_offset;
	fw_if->copy_fw = bhdr_copy_fw;

	return fw_if;
}

/* Implementation destroy */
void bhdr_destroy(struct fwhdr_if *interface)
{
	struct bhdr *bhdr = bhdr_from_fwhdr_if(interface);
	struct fwhdr_if *fw_if;

	if (!bhdr)
		return;

	fw_if = &bhdr->fw_if;
	kfree(bhdr);
}
