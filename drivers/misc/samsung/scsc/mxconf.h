/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/**
 * mx140 Infrastructure Configuration Structure.
 *
 * Used to pass configuration data from AP to R4 infrastructure
 * on Maxwell Subsystem startup.
 *
 * Notes:
 *
 * - All multi-octet integers shall be stored LittleEndian.
 *
 * - All location fields ("*_loc") are 32 bit octet offsets w.r.t. the R4
 * address map. They can therefore refer to DRAM memory or Mailbox registers.
 *
 * - "typedefs" are avoided to allow inclusion in linux source code.
 */
#ifndef MXCONF_H__
#define MXCONF_H__

/* Uses */

/* It appears that due to the previous syntax "__packed struct foo" used in this
 * header, the structures here don't actually get packed. Clang warns that
 * that syntax is ignored. But correcting it causes a misalignment with FW.
 * The __MXPACKED macro is used to stop packing the structures in this
 * header until we've investigated further.
 */
#define __MXPACKED /* TODO: HACK - don't actually pack! */

/* Definitions */

/**
 *  Config structure magic number.
 *
 *  The AP writes this value and the R4 checks it to trap endian mismatches.
 */
#define MXCONF_MAGIC 0x79828486

/**
 *  Config structure version
 *
 *  The AP writes these values and the R4 checks them to trap config structure
 *  mismatches.
 */
#define MXCONF_VERSION_MAJOR 0
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
#define MXCONF_VERSION_MINOR 7 /* For Service Arbitrary data */
#else
#define MXCONF_VERSION_MINOR 5 /* For fleximac moredump */
#endif
#ifdef CONFIG_SOC_EXYNOS7885
#define MXCONF_VERSION_MINOR_1 1
#define MXCONF_VERSION_MINOR_2 2
#endif

/* Types */

/**
 * Maxwell Circular Packet Buffer Configuration.
 */
struct mxcbufconf {
	scsc_mifram_ref buffer_loc;      /**< Location of allocated buffer in DRAM */
	uint32_t        num_packets;     /**< Total number of packets that can be stored in the buffer */
	uint32_t        packet_size;     /**< Size of each individual packet within the buffer */
	scsc_mifram_ref read_index_loc;  /**< Location of 32bit read index in DRAM or Mailbox */
	scsc_mifram_ref write_index_loc; /**< Location of 32bit write index */
} __MXPACKED;

/**
 * Maxwell Management Simplex Stream Configuration
 *
 * A circular buffer plus a pair of R/W signaling bits.
 */
struct mxstreamconf {
	/** Circular Packet Buffer configuration */
	struct mxcbufconf buf_conf;

	/** Allocated MIF Interrupt Read Bit Index */
	uint8_t           read_bit_idx;

	/** Allocated MIF Interrupt Write Bit Index */
	uint8_t           write_bit_idx;
} __MXPACKED;

/**
 * Maxwell Management Transport Configuration
 *
 * A pair of simplex streams.
 */
struct mxtransconf {
	struct mxstreamconf to_ap_stream_conf;
	struct mxstreamconf from_ap_stream_conf;
} __MXPACKED;

/**
 * Maxwell Infrastructure Configuration Version
 */
struct mxconfversion {
	uint16_t major;
	uint16_t minor;
} __MXPACKED;

/**
  * Mxlog Event Buffer Configuration.
  *
  * A circular buffer. Size must be a multiple of 2.
  */
struct mxlogconf
{
	struct mxstreamconf stream_conf;
} __MXPACKED;


/**
 * Maxwell Infrastructure Configuration Override (HCF block)
 */
struct mxmibref {
	uint32_t offset;
	uint32_t size;
} __MXPACKED;


/**
 * Maxwell Infrastructure Configuration
 */
struct mxconf {
	/**
	 * Config Magic Number
	 *
	 * Always 1st field in config.
	 */
	uint32_t magic;

	/**
	 * Config Version.
	 *
	 * Always second field in config.
	 */
	struct mxconfversion version;

	/**
	 * MX Management Message Transport Configuration.
	 */
	struct mxtransconf   mx_trans_conf;

	/**
	 * MX Management GDB Message Transport Configuration.
	 */
	/**
	* Monitor Mode Channel 0 Transport Configuration.
	*/
	struct mxtransconf   monitor_c0_trans_conf;
	/**
	* Monitor Mode Channel 1 Transport Configuration.
	*/
	struct mxtransconf   monitor_c1_trans_conf;

	/**
	 * Mxlog Event Buffer Configuration.
	 */
	struct mxlogconf mxlogconf;

	/* FROM MINOR_2 */

	/**
	 * SOC HW revision override from host
	 */
	uint32_t soc_revision;

	/* FROM MINOR_3 */

	/**
	 * Setup flags
	 */
#define MXCONF_FLAGS_FM_ON	(BIT(0))	/* FM already on */
	uint32_t flags;

	/* FROM MINOR_4 */

	/**
	 * Common HCF offset
	 */
	struct mxmibref fwconfig;

	/* FROM MINOR_5 */

	/**
	* Monitor Mode Channel 2 Transport Configuration.
	*/
	struct mxtransconf   monitor_c2_trans_conf;

	/* FROM MINOR_6 */
	/**
	* Subsystem specific configuration offset and length
	*/
	uint32_t subsysconf_offset;
	uint32_t subsysconf_length;

	/* FROM MINOR_ 7 */
	/*
	 * mxlogger area offset and length
	 */
	uint32_t mxlogger_area_offset;
	uint32_t mxlogger_area_length;
} __MXPACKED;

#endif /* MXCONF_H__ */
