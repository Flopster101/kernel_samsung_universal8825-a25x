/*
* @file sgpu_bpmd_layout_1_0.h
* @copyright 2020 Samsung Electronics
*/

#ifndef _SGPU_BPMD_LAYOUT_1_0_H_
#define _SGPU_BPMD_LAYOUT_1_0_H_

#include "sgpu_bpmd_layout_common.h"

#define SGPU_BPMD_LAYOUT_MAJOR_VERSION 1
#define SGPU_BPMD_LAYOUT_MINOR_VERSION 0

/**
 * Register Packet
 */
struct sgpu_bpmd_layout_reg32_packet {
	struct sgpu_bpmd_layout_packet_header header;
	uint32_t		       domain;	/* sgpu_bpmd_layout_reg_domain */
	uint32_t		       num_regs;
	/** Value written to GRBM_GFX_INDEX when the register is read */
	uint32_t 		       grbm_gfx_index;
	struct sgpu_bpmd_layout_reg32  regs[];
} __attribute__((packed, aligned(1)));

/**
 * BO Packet
 */
struct sgpu_bpmd_layout_bo_packet {
	struct sgpu_bpmd_layout_packet_header header;
	uint32_t domain; /* sgpu_bpmd_memory_domain_kind */
	uint32_t data_size;
	uint8_t  data[];
} __attribute__((packed, aligned(1)));

/**
 * Ring Packet
 */
struct sgpu_bpmd_layout_ring_packet {
	struct sgpu_bpmd_layout_packet_header header;
	uint32_t type; /* sgpu_bpmd_layout_ring_type */
	uint32_t me;
	uint32_t pipe;
	uint32_t queue;
	uint64_t rptr;
	uint64_t wptr;
	uint32_t buf_mask;
	uint32_t align_mask;
	uint32_t sync_seq;
	uint32_t last_seq;
	uint32_t num_fences_mask;
	uint32_t hw_submission_limit;
	uint32_t hw_rq_count;
	uint64_t job_id_count;
	uint32_t num_jobs;
	uint32_t ring_bo_packet_id;
	uint32_t mqd_bo_packet_id;
	uint32_t reg32_packet_id;
} __attribute__((packed, aligned(1)));


struct sgpu_bpmd_task_info {
	char	process_name[TASK_COMM_LEN];
	char	task_name[TASK_COMM_LEN];
	u32	pid;
	u32	tgid;
} __attribute__((packed, aligned(1)));

/**
 * BO VA Packet
 */
struct sgpu_bpmd_layout_bo_va_packet {
	struct sgpu_bpmd_layout_packet_header header;
	uint32_t                           vmid;
	struct sgpu_bpmd_task_info	   task_info;
	uint32_t                           num_va_mappings;
	struct sgpu_bpmd_layout_va_mapping va_mappings[];
} __attribute__((packed, aligned(1)));

struct sgpu_bpmd_layout_firmwares_version_packet {
	struct sgpu_bpmd_layout_packet_header header;
	struct sgpu_bpmd_hw_revision hw_revision;
	uint32_t num_firmwares;
	struct sgpu_bpmd_firmwares_version firmwares[];
} __attribute__((packed, aligned(1)));

struct sgpu_bpmd_layout_footer_packet {
	struct sgpu_bpmd_layout_packet_header header;
	uint32_t crc32;
} __attribute__((packed, aligned(1)));

void sgpu_bpmd_layout_dump_header(struct file *f);
uint32_t sgpu_bpmd_layout_dump_reg32_packet(struct file *f,
				    enum sgpu_bpmd_layout_reg_domain domain,
				    uint32_t num_regs,
				    struct sgpu_bpmd_layout_reg32 *reg32,
				    uint32_t grbm_gfx_index);
uint32_t sgpu_bpmd_layout_dump_bo_packet(struct file *f, struct amdgpu_bo *bo);
uint32_t sgpu_bpmd_layout_dump_ring_packet(struct file *f,
					   struct amdgpu_ring *ring,
					   uint32_t reg32_packet_id);
uint32_t sgpu_bpmd_layout_dump_system_info_packet(
	struct file *f,
	const struct sgpu_bpmd_hw_revision *hw_revison,
	size_t firmware_count,
	const struct sgpu_bpmd_firmwares_version *firmwares);
uint32_t sgpu_bpmd_layout_dump_footer_packet(struct file *f);

#endif  /* _SGPU_BPMD_LAYOUT_1_0_H_ */
