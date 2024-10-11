/*
* @file sgpu_bpmd.h
* @copyright 2020 Samsung Electronics
*/

#ifndef _SGPU_BPMD_H_
#define _SGPU_BPMD_H_

#include <drm/drm_print.h>
#include <linux/limits.h>
#include <linux/types.h>

#include <sgpu_bpmd_layout_common.h>

/* forward declaration */
struct amdgpu_bo;
struct amdgpu_bo_va;
struct amdgpu_device;
struct amdgpu_ring;
struct file;

#define SGPU_BPMD_INVALID_VMID UINT_MAX

/* RFC: Should this use amdgpu_ib */
struct sgpu_bpmd_ib_info {
	uint64_t         gpu_addr;
	uint64_t         size;
	uint32_t         vmid;
	struct list_head list;
};

struct sgpu_bpmd_funcs {
	void (*dump_header)(struct file *f);
	int (*dump_reg)(struct file *f, struct amdgpu_device *sdev);
	uint32_t (*dump_ring)(struct file *f, struct amdgpu_ring *ring);
	uint32_t (*dump_bo)(struct file *f, struct amdgpu_bo *bo);
	void (*find_ibs)(const uint32_t *addr, uint32_t size, uint32_t vmid,
			 struct list_head *list);
	uint32_t (*dump_system_info)(struct file *f,
				     const struct sgpu_bpmd_hw_revision *hw_revison,
				     size_t firmware_count,
				     const struct sgpu_bpmd_firmwares_version *firmwares);
	uint32_t (*dump_footer)(struct file *f);
};

struct sgpu_bpmd {
	const struct sgpu_bpmd_funcs *funcs;
};

static inline bool sgpu_bpmd_add_ib_info(struct list_head* list,
					 uint64_t gpu_addr, uint64_t size,
					 uint32_t vmid)
{
	struct sgpu_bpmd_ib_info *ib_info =
		kmalloc(sizeof(struct sgpu_bpmd_ib_info), GFP_KERNEL);
	if (ib_info == NULL) {
		DRM_ERROR("Out of memory");
		return false;
	}
	ib_info->gpu_addr = gpu_addr;
	ib_info->size     = size;
	ib_info->vmid     = vmid;

	list_add(&ib_info->list, list);
	return true;
}

void sgpu_bpmd_dump(struct amdgpu_device *sdev);
ssize_t sgpu_bpmd_write(struct file *f,  const void *buf, size_t count,
			uint32_t align);
uint32_t sgpu_bpmd_get_packet_id(void);

#endif  /* _SGPU_BPMD_H_ */
