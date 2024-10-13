/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/*
 * Tag-Length-Value encoding of WLBT Firmware File Header (FHDR)
 *
 * A valid FHDR must start with the FHDR_TAG_TOTAL_LENGTH tag whose value contains the overall
 * length of the FHDR, but can in addition to that contain any number of Tag-Length-Value tuples in
 * any order.
 *
 * The on-disk format is strictly little-endian. A correctly formatted FHDR will start with the
 * ASCII values for the characters "FHDR" in that order. This sequence of characters originate from
 * the FHDR_TAG_TOTAL_LENGTH tag always being the first tag in the FHDR. A generic endianness
 * agnostic reader can use this sequence to determine if byte-swap is needed.
 */

#ifndef FHDR_H__
#define FHDR_H__

/** Definition of tags */
enum fhdr_tag {
	/**
     * The total length of the entire FHDR this tag is contained in, including the size of all
     * tag, length and value fields.
     *
     * This special tag must be present as first item for validation/integrity check purposes,
     * and to be able to determine the overall length of the FHDR.
     *
     * Value length: 4 byte
     * Value encoding: uint32_t
     */
	FHDR_TAG_TOTAL_LENGTH = 0x52444846, /* "FHDR" */

	/**
     * WLAN firmware binary image.
     *
     * Value length: N byte
     * Value encoding: uint8_t[N]
     */
	FHDR_TAG_WLAN_FW = 1,

	/**
     * BT firmware binary image.
     *
     * Value length: N byte
     * Value encoding: uint8_t[N]
     */
	FHDR_TAG_BT_FW = 2,
	/*
     * PMU firmware binary image.
     *
     * Value length : N byte
     * Value encoding : uint8_t[N]
     */
	FHDR_TAG_PMU_FW = 0x3,

	/*
     * WLAN firmware strings(formerly log - strings.bin)
     *
     * Value length : N byte
     * Value encoding : uint8_t[N]
     */
	FHDR_TAG_WLAN_STRINGS = 0x101,

	/*
     * Bluetooth firmware strings(formerly log - strings.bin)
     *
     * Value length : N byte
     * Value encoding : uint8_t[N]
     */

	FHDR_TAG_BT_STRINGS = 0x102,

	/*
     * PMU firmware strings(formerly log - strings.bin)
     *
     * Value length : N byte
     * Value encoding : uint8_t[N]
     */
	FHDR_TAG_PMU_STRINGS = 0x103,

	/*
    * Firmware branch name.
    *
    *  A string of ASCII characters containing the manifest-merge-branch name.
    *
    *  Value length: N byte
    *  Value encoding: uint8_t[N]
    */
	FHDR_TAG_META_BRANCH = 0x201,

	/*
    *  Firmware build identifier.
    *
    *  A string of ASCII characters which identify the specific firmware build in an easily readable
    *  form.
    *
    *  Value length: N byte
    *  Value encoding: uint8_t[N]
    */
	FHDR_TAG_META_BUILD_IDENTIFIER = 0x202,

	/*
    *  Common repository hash represented as a string of hexadecimal characters, followed by '+' if
    *  there are uncommitted changes to the repository during the build.
    *
    *  Value length: N byte
    *  Value encoding: uint8_t[N]
    */
	FHDR_TAG_META_FW_COMMON_HASH = 0x300,

	/*
    *  WLAN repository hash represented as a string of hexadecimal characters, followed by '+' if
    *  there are uncommitted changes to the repository during the build.
    *
    *  Value length: N byte
    *  Value encoding: uint8_t[N]
    */
	FHDR_TAG_META_FW_WLAN_HASH = 0x301,

	/*
    *  Bluetooth repository hash represented as a string of hexadecimal characters, followed by '+'
    *  if there are uncommitted changes to the repository during the build.
    *
    *  Value length: N byte
    *  Value encoding: uint8_t[N]
    */
	FHDR_TAG_META_FW_BT_HASH = 0x302,

	/*
    *  PMU repository hash represented as a string of hexadecimal characters, followed by '+' if
    *  there are uncommitted changes to the repository during the build.
    *
    *  Value length: N byte
    *  Value encoding: uint8_t[N]
    */
	FHDR_TAG_META_FW_PMU_HASH = 0x303,

	/*
    *  Hardware repository hash represented as a string of hexadecimal characters, followed by '+' if
    *  there are uncommitted changes to the repository during the build.
    *
    *  Value length: N byte
    *  Value encoding: uint8_t[N]
    */
	FHDR_TAG_META_HARDWARE_HASH = 0x304
};

/**
 * Structure describing the tag and length of each item in the FHDR.
 * The value of the item follows immediately after.
 */
struct fhdr_tag_length {
	uint32_t tag; /* One of fhdr_tag */
	uint32_t length; /* Length of value in bytes */
};

const void *fhdr_lookup_tag(const void *blob, enum fhdr_tag tag, uint32_t *length);

#endif /* FHDR_H__ */
