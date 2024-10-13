/*
* @file sgpu_bpmd_layout_1_0.c
* @copyright 2020 Samsung Electronics
*/

#include "amdgpu.h"
#include "amdgpu_vm.h"
#include "sgpu_bpmd.h"
#include "sgpu_bpmd_layout_1_0.h"

/**
 * sgpu_bpmd_layout_dump_header_packet - dump a bpmd header
 *
 * @f: pointer to a file
 *
 */
void sgpu_bpmd_layout_dump_header(struct file *f)
{
	sgpu_bpmd_layout_dump_header_version(f,
		SGPU_BPMD_LAYOUT_MAJOR_VERSION,
		SGPU_BPMD_LAYOUT_MINOR_VERSION);
}

/**
 * sgpu_bpmd_layout_dump_reg32_packet - dump reg32 packet
 *
 * @f: pointer to a file
 * @domain: bpmd_layout_reg_domain
 * @num_regs: number of registers
 * @reg32: pointer to bpmd_layout_reg32 array
 * @type: bpmd_layout_reg_domain
 * @grbm_gfx_index: value written to GRBM_GFX_INDEX when register is read
 *
 * Return a packet ID
 */
uint32_t sgpu_bpmd_layout_dump_reg32_packet(
	struct file *f,
	enum sgpu_bpmd_layout_reg_domain domain,
	uint32_t num_regs,
	struct sgpu_bpmd_layout_reg32 *reg32,
	uint32_t grbm_gfx_index)
{
	ssize_t r = 0;
	size_t size = 0;
	const size_t total_size
		= sizeof(struct sgpu_bpmd_layout_reg32_packet)
		+ num_regs * sizeof(struct sgpu_bpmd_layout_reg32);
	struct sgpu_bpmd_layout_reg32_packet packet = {
		.header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_REG,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		},
		.domain         = domain,
		.num_regs       = num_regs,
		.grbm_gfx_index = grbm_gfx_index
	};

	DRM_DEBUG("%s: start dumping a reg32 packet\n", __func__);

	BUG_ON((num_regs == 0) || (reg32 == NULL));

	DRM_DEBUG("%s: dump reg32 packet(id %u), num %d (pos: %lld)\n",
		 __func__, packet.header.id, num_regs, f->f_pos);

	size = sizeof(packet);
	r = sgpu_bpmd_write(f, (const void *) &packet, size,
			    SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != size) {
		DRM_ERROR("failed to dump reg32 packet\n");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	size = num_regs * sizeof(reg32[0]);
	r = sgpu_bpmd_write(f, (const void *) reg32, size,
			    SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != size) {
		DRM_ERROR("failed to dump reg32 packet data\n");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	return packet.header.id;
}

/**
 * sgpu_bpmd_layout_dump_bo_packet - dump a bo packet
 *
 * @note: This function shouldn't be called immediately when discovering a BO.
 * Instead, call sgpu_bpmd_register_bo() to add it to a list that will be dumped
 * later on using sgpu_bpmd_dump_bo_list(). Once references to the packet_id
 * have been consumed, the list can be safely cleaned with
 * sgpu_bpmd_clean_bo_list().
 *
 * @f: pointer to a file
 * @bo: pointer to a amgpu_bo to dump
 *
 * Return a packet ID
 */
uint32_t sgpu_bpmd_layout_dump_bo_packet(struct file *f, struct amdgpu_bo *bo)
{
	ssize_t r = 0;
	size_t size = 0;
	const void *buf = amdgpu_bo_kptr(bo);
	const size_t bo_size = amdgpu_bo_size(bo);
	const size_t total_size
		= sizeof(struct sgpu_bpmd_layout_bo_packet)
		+ bo_size;
	struct sgpu_bpmd_layout_bo_packet packet = {
		.header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_BO,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		},
		.domain      = SGPU_BPMD_LAYOUT_MEMORY_DOMAIN_GTT,
		.data_size   = bo_size
	};

	if(bo->bpmd_packet_id != SGPU_BPMD_LAYOUT_INVALID_PACKET_ID)
		DRM_WARN("BO was already dumped as packet %d", bo->bpmd_packet_id);

	DRM_DEBUG("%s: start dumping a bo packet", __func__);

	BUG_ON(buf == NULL);

	DRM_DEBUG("%s: dump bo packet(id %u), size %d (pos: %lld)\n",
		 __func__, packet.header.id, packet.data_size, f->f_pos);

	size = sizeof(packet);
	r = sgpu_bpmd_write(f, (const void *) &packet, sizeof(packet),
			    SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != size) {
		DRM_ERROR("failed to dump bo packet\n");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	size = packet.data_size;
	r = sgpu_bpmd_write(f, buf, size, SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != size) {
		DRM_ERROR("failed to dump bo packet data\n");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	bo->bpmd_packet_id = packet.header.id;

	return packet.header.id;
}

/**
 * sgpu_bpmd_layout_dump_ring_packet - dump a ring packet
 *
 * @f: pointer to a file
 * @ring: pointer to a amgpu_ring
 * @reg32_packet_id: packet id for a reg32
 *
 * Return a packet ID
 */
uint32_t sgpu_bpmd_layout_dump_ring_packet(struct file *f,
					   struct amdgpu_ring *ring,
					   uint32_t reg32_packet_id)
{
	ssize_t r = 0;
	size_t size = 0;
	const size_t total_size = sizeof(struct sgpu_bpmd_layout_ring_packet);
	struct sgpu_bpmd_layout_ring_packet packet = {
		. header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_RING,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		}
	};

	DRM_INFO("%s: start dumping a ring packet", __func__);

	packet.type = sgpu_bpmd_layout_get_ring_type(ring->funcs->type);
	packet.me                = ring->me;
	packet.pipe              = ring->pipe;
	packet.queue             = ring->queue;
	packet.rptr              = ring->funcs->get_rptr(ring);
	packet.wptr              = ring->funcs->get_wptr(ring);
	packet.buf_mask          = ring->buf_mask;
	packet.align_mask        = ring->funcs->align_mask;
	packet.sync_seq          = ring->fence_drv.sync_seq;
	packet.last_seq          = atomic_read(&ring->fence_drv.last_seq);
	packet.num_fences_mask   = ring->fence_drv.num_fences_mask;
	packet.hw_submission_limit = ring->sched.hw_submission_limit;
	packet.hw_rq_count       = atomic_read(&ring->sched.hw_rq_count);
	packet.job_id_count      = atomic64_read(&ring->sched.job_id_count);
	packet.num_jobs          = atomic_read(&ring->num_jobs);
	packet.ring_bo_packet_id = ring->ring_obj
		? ring->ring_obj->bpmd_packet_id
		: SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	packet.mqd_bo_packet_id  = ring->ring_obj
		? ring->mqd_obj->bpmd_packet_id
		: SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	packet.reg32_packet_id   = reg32_packet_id;

	DRM_DEBUG("%s: dump ring packet(id %u), me %d, pipe %d, queue %d" \
		  "(pos: %lld)\n",
		  __func__, packet.header.id, ring->me, ring->pipe, ring->queue,
		  f->f_pos);

	size = sizeof(packet);
	r = sgpu_bpmd_write(f, (const void *) &packet, size,
			    SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r != size) {
		DRM_ERROR("failed to dump ring packet");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	}

	return packet.header.id;
}

uint32_t sgpu_bpmd_layout_dump_system_info_packet(
	struct file *f,
	const struct sgpu_bpmd_hw_revision *hw_revision,
	size_t firmware_count,
	const struct sgpu_bpmd_firmwares_version *firmwares)
{
	int i = 0;
	int r = 0;
	const size_t total_size
		= sizeof(struct sgpu_bpmd_layout_firmwares_version_packet)
		+ firmware_count * sizeof(struct sgpu_bpmd_firmwares_version);
	struct sgpu_bpmd_layout_firmwares_version_packet packet = {
		. header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_FW_VERSION,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		},
		.hw_revision = *hw_revision,
		.num_firmwares = firmware_count
	};

	r = sgpu_bpmd_write(f, (const void *) &packet, sizeof(packet),
			    SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);


	for (i = 0; i < firmware_count; i++)
		r += sgpu_bpmd_write(f,
				     (const void *) &firmwares[i],
				     sizeof(struct sgpu_bpmd_firmwares_version),
				     SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);

	if (r != total_size) {
		DRM_ERROR("failed to dump firmware version packet");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	} else {
		return packet.header.id;
	}
}

extern u32 sgpu_bpmd_crc32;

uint32_t sgpu_bpmd_layout_dump_footer_packet(struct file *f)
{
	const size_t total_size = sizeof(struct sgpu_bpmd_layout_footer_packet);
	struct sgpu_bpmd_layout_footer_packet packet = {
		. header = {
			.kind   = SGPU_BPMD_LAYOUT_PACKET_FOOTER,
			.id     = sgpu_bpmd_get_packet_id(),
			.length = total_size - sizeof(struct sgpu_bpmd_layout_packet_header)
		},
		.crc32 = sgpu_bpmd_crc32 ^ 0xffffffff,
	};

	int r = sgpu_bpmd_write(f,
				(const void *) &packet,
				sizeof(struct sgpu_bpmd_layout_footer_packet),
				SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);

	if (r != total_size) {
		DRM_ERROR("failed to dump footer packet");
		return SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
	} else {
		return packet.header.id;
	}
}
