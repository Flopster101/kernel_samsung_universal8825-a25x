/*
* @file sgpu_bpmd_layout_common.h
* @copyright 2020 Samsung Electronics
*/

#ifndef _SGPU_BPMD_LAYOUT_COMMON_H_
#define _SGPU_BPMD_LAYOUT_COMMON_H_

#include <linux/types.h>

/* forward declaration */
enum amdgpu_ring_type;
struct file;

/**
 * A BPMD file consists of
 *   - one bpmd_header
 *   - a stream of bpmd_packets
 *
 * This file contains common layout interface which won't change.
 *
 * Alignment
 *   - Each struct should be 4 byte-aligned
 */

/**
 * Header
 */
#define SGPU_BPMD_LAYOUT_MAGIC_0 0x0D
#define SGPU_BPMD_LAYOUT_MAGIC_1 0x15
#define SGPU_BPMD_LAYOUT_MAGIC_2 0xEA
#define SGPU_BPMD_LAYOUT_MAGIC_3 0x5E

struct sgpu_bpmd_hw_revision {
	uint32_t minor;
	uint32_t major;
	uint32_t model;
	uint32_t generation;
} __attribute__((packed, aligned(1)));

struct sgpu_bpmd_firmwares_version {
	uint32_t id; /* enum sgpu_bpmd_fw_id */
	uint32_t version;
	uint32_t feature_version;
} __attribute__((packed, aligned(1)));

struct sgpu_bpmd_layout_header {
	uint8_t  magic[4];	/* should be SGPU_BPMD_MAGIC_NUM */
	uint16_t major;		/* major version */
	uint16_t minor;		/* minor version */
} __attribute__((packed, aligned(1)));

enum sgpu_bpmd_fw_id {
	SGPU_BPMD_FW_ID_CP_PFP = 0,
	SGPU_BPMD_FW_ID_CP_ME,
	SGPU_BPMD_FW_ID_CP_MEC,
	SGPU_BPMD_FW_ID_RLC,
	SGPU_BPMD_FW_ID_COUNT
};

/**
 * General
 */
#define SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE 4
#define SGPU_BPMD_LAYOUT_INVALID_PACKET_ID 0

enum sgpu_bpmd_layout_packet_kind {
	SGPU_BPMD_LAYOUT_PACKET_REG = 0,
	SGPU_BPMD_LAYOUT_PACKET_RING,
	SGPU_BPMD_LAYOUT_PACKET_BO,
	SGPU_BPMD_LAYOUT_PACKET_BO_VA,
	SGPU_BPMD_LAYOUT_PACKET_FW_VERSION,
	SGPU_BPMD_LAYOUT_PACKET_FOOTER,
	SGPU_BPMD_LAYOUT_PACKET_COUNT
};

enum sgpu_bpmd_layout_reg_domain {
	SGPU_BPMD_LAYOUT_REG_DOMAIN_SFR = 0,
	SGPU_BPMD_LAYOUT_REG_DOMAIN_DOORBELL,
	SGPU_BPMD_LAYOUT_REG_DOMAIN_COUNT
};

enum sgpu_bpmd_layout_memory_domain_kind {
	SGPU_BPMD_LAYOUT_MEMORY_DOMAIN_VRAM = 0,
	SGPU_BPMD_LAYOUT_MEMORY_DOMAIN_GTT,
	SGPU_BPMD_LAYOUT_MEMORY_DOMAIN_COUNT
};

enum sgpu_bpmd_layout_ring_type {
	SGPU_BPMD_LAYOUT_RING_TYPE_GFX = 0,
	SGPU_BPMD_LAYOUT_RING_TYPE_COMPUTE,
	SGPU_BPMD_LAYOUT_RING_TYPE_COUNT
};

struct sgpu_bpmd_layout_packet_header {
	uint32_t kind;	/* sgpu_bpmd_layout_packet_kind */
	uint32_t id;
	uint32_t length; /* Size in bytes of payload */
} __attribute__((packed, aligned(1)));

struct sgpu_bpmd_layout_reg32 {
	uint32_t index;	/* an index from register array */
	uint32_t value;
} __attribute__((packed, aligned(1)));

struct sgpu_bpmd_layout_va_mapping {
	uint64_t start;
	uint64_t last;
	uint64_t offset;
	uint64_t flags;
	uint32_t bo_packet_id;
} __attribute__((packed, aligned(1)));

void sgpu_bpmd_layout_dump_header_version(struct file *f,
					  uint32_t major,
					  uint32_t minor);
enum sgpu_bpmd_layout_ring_type
sgpu_bpmd_layout_get_ring_type(enum amdgpu_ring_type type);

#endif  /* _SGPU_BPMD_LAYOUT_COMMON_H_ */
