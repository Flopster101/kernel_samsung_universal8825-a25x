/*
* @file sgpu_bpmd.c
* @copyright 2020 Samsung Electronics
*/

#include <linux/atomic.h>
#include <linux/crc32.h>
#include <linux/fs.h>
#include <linux/kernel.h>


#include "amdgpu.h"
#include "sgpu_bpmd.h"

static struct amdgpu_vm *vm[AMDGPU_NUM_VMID] = {NULL};

/**
 * reinit_vm_map - re-initialize the mappings for next BPMD dump
 *
 * The VMID to admgpu_vm mappings can change in between different dumps,
 * therefore re-initializing prevents having older mappings that may
 * not exist anymore.
 */
static void reinit_vm_map(void)
{
	memset((void*)vm, 0, sizeof(vm[0]) * ARRAY_SIZE(vm));
}

static void update_vm_map_job(struct amdgpu_job *job)
{
	WARN(vm[job->vmid] && job->vm != vm[job->vmid],
	     "A vmid should map to only one amdgpu_vm struct");

	vm[job->vmid] = job->vm;
	DRM_DEBUG("Adding mapping : vmid %d -> vm %p", job->vmid, job->vm);
}

/**
 * update_vm_map - Add vmid to amdgpu_vm mappings if not already existing
 */
static void update_vm_map(struct drm_gpu_scheduler* sched)
{
	struct drm_sched_job *s_job = NULL;
	struct amdgpu_job *job = NULL;

	BUG_ON(sched == NULL);

	/* @todo GFXSW-4661 - Make sure this lock is necessary */
	spin_lock(&sched->job_list_lock);
	list_for_each_entry(s_job, &sched->ring_mirror_list, node) {
		job = to_amdgpu_job(s_job);
		update_vm_map_job(job);
	}
	spin_unlock(&sched->job_list_lock);
}

/**
 * sgpu_bpmd_register_bo - register a BO to be dumped
 *
 * @bo_dump_list: list in which to register the BO
 * @bo: bo to register
 */
static void sgpu_bpmd_register_bo(struct list_head *bo_dump_list, struct amdgpu_bo *bo)
{
	if (list_empty(&bo->bpmd_list)) {
		list_add(&bo->bpmd_list, bo_dump_list);
	}
}

/**
 * sgpu_bpmd_get_filename - set a bpmd file name including a path
 *
 * @buf: pointer to a buffer
 * @size: size of buf
 *
 * Return the number of characters (<=size),
 * otherwise a negative number is returned if an encoding error occurs
 */
static int sgpu_bpmd_get_filename(char *buf, size_t size)
{
	int r = 0;
	static const char *name = "bpmd";
	static const char *ext  = "bin";
	static atomic_t seq = ATOMIC_INIT(0);

	BUG_ON(buf == NULL);

	r = snprintf(buf, size, "%s/%s.%d.%s", sgpu_bpmd_path, name,
		     (uint32_t) atomic_inc_return(&seq), ext);
	if (r <= 0) {
		DRM_ERROR("failed to set a filename %d\n", r);
	}

	return r;
}

u32 sgpu_bpmd_crc32 = 0xffffffff;

/**
 * sgpu_bpmd_file_open - open bpmd file and return file pointer
 *
 * @filename: path to open
 *
 */
static struct file *sgpu_bpmd_file_open(const char *filename)
{
	struct file *f = NULL;

	BUG_ON(filename == NULL);
	sgpu_bpmd_crc32 = 0xffffffff;

	f = filp_open(filename, O_WRONLY | O_LARGEFILE | O_CREAT,
		      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (IS_ERR(f)) {
		DRM_ERROR("failed to open a file (%s) - %ld\n",
			  filename, PTR_ERR(f));
		return NULL;
	}

	return f;
}

/**
 * sgpu_bpmd_write - write a buffer to a file with alignment
 *
 * @f: pointer to a file
 * @buf: pointer to a buffer
 * @count: byte count to write
 * @align: alignment in byte
 *
 * Return the number of bytes written
 */
ssize_t sgpu_bpmd_write(struct file *f,  const void *buf, size_t count,
			uint32_t align)
{
	ssize_t r 	   = 0;
	loff_t pos 	   = ALIGN(f->f_pos, align);
	loff_t zeros_count = pos - f->f_pos;

	const u8 zero = 0;

	BUG_ON((f == NULL));

	while (zeros_count-- > 0) {
		sgpu_bpmd_crc32 = crc32(sgpu_bpmd_crc32, &zero, 1);
	}
	sgpu_bpmd_crc32 = crc32(sgpu_bpmd_crc32, buf, count);

	r = kernel_write(f, buf, count, &pos);
	if (r > 0) {
		f->f_pos = pos;
	}

	if (r != count) {
		DRM_ERROR("failed to write r %ld count %ld\n", r, count);
	}

	return r;
}

/**
 * sgpu_bpmd_dump_header - dump header
 *
 * @sdev: pointer to sgpu_device
 * @f: pointer to a file
 *
 */
static void sgpu_bpmd_dump_header(struct amdgpu_device *sdev, struct file *f)
{
	struct sgpu_bpmd *bpmd = &sdev->bpmd;

	DRM_DEBUG("%s: start dumping header (pos: %lld)\n", __func__, f->f_pos);

	bpmd->funcs->dump_header(f);
}

static void sgpu_bpmd_dump_footer(struct amdgpu_device *sdev, struct file *f)
{
	sdev->bpmd.funcs->dump_footer(f);
}

static void sgpu_bpmd_dump_system_info(struct amdgpu_device *sdev,
				       struct file *f)
{
	const struct sgpu_bpmd_hw_revision hw_revision = {
		.minor = sdev->minor_rev,
		.major = sdev->major_rev,
		.model = sdev->model_id,
		.generation = sdev->gen_id
	};
	const struct sgpu_bpmd_firmwares_version firmwares[] = {
		{
			.id = SGPU_BPMD_FW_ID_CP_ME,
			.version = sdev->gfx.me_fw_version,
			.feature_version = sdev->gfx.me_feature_version
		},
		{
			.id = SGPU_BPMD_FW_ID_CP_MEC,
			.version = sdev->gfx.mec_fw_version,
			.feature_version = sdev->gfx.mec_feature_version
		},
		{
			.id = SGPU_BPMD_FW_ID_CP_PFP,
			.version = sdev->gfx.pfp_fw_version,
			.feature_version = sdev->gfx.pfp_feature_version
		},
		{
			.id = SGPU_BPMD_FW_ID_RLC,
			.version = sdev->gfx.rlc_fw_version,
			.feature_version = sdev->gfx.rlc_feature_version
		}
	};

	DRM_DEBUG("%s: start dumping firmware version (pos: %lld)\n", __func__, f->f_pos);

	sdev->bpmd.funcs->dump_system_info(f, &hw_revision, ARRAY_SIZE(firmwares), firmwares);
}

/*
 * sgpu_bpmd_dump_regs - dump registers
 *
 * @sdev: pointer to sgpu_device
 * @f: pointer to a file
 *
 */
static void sgpu_bpmd_dump_regs(struct amdgpu_device *sdev, struct file *f)
{
	struct sgpu_bpmd *bpmd = &sdev->bpmd;

	DRM_DEBUG("%s: start dumping regs (pos: %lld)\n", __func__, f->f_pos);

	bpmd->funcs->dump_reg(f, sdev);
}

inline void sgpu_bpmd_remove_ib_info(struct sgpu_bpmd_ib_info *ib_info)
{
	list_del(&ib_info->list);
	kfree(ib_info);
}

static void sgpu_bpmd_find_ib_bo_va_bfs(struct list_head *ib_info_list,
					struct amdgpu_ring *ring,
					struct list_head *bo_dump_list)
{
	int32_t r = 0;
	struct sgpu_bpmd *bpmd = &ring->adev->bpmd;

	/* Find out all BOs used for IB chains */
	while (!list_empty(ib_info_list)) {
		struct amdgpu_bo_va_mapping *mapping = NULL;
		struct sgpu_bpmd_ib_info* ib_info = NULL;
		uint64_t gpu_page = 0;
		uint64_t offset_in_bo = 0;
		const uint8_t *buf = NULL;

		/* Get an ib_info */
		ib_info = list_first_entry(ib_info_list,
					   struct sgpu_bpmd_ib_info, list);

		/* Find a mapping */
		if (vm[ib_info->vmid]) {
			gpu_page = ib_info->gpu_addr >> AMDGPU_GPU_PAGE_SHIFT;
			mapping = amdgpu_vm_bo_lookup_mapping(vm[ib_info->vmid],
							      gpu_page);
		} else
			DRM_ERROR("vmid %d doesn't have a mapping", ib_info->vmid);

		if (mapping == NULL || mapping->bo_va == NULL ||
		    mapping->bo_va->base.bo == NULL) {
			DRM_ERROR("Failed to find bo within vm\n");
			goto pop;
		}

		/* Add mapping->bo_va if it's new */
		sgpu_bpmd_register_bo(bo_dump_list, mapping->bo_va->base.bo);

		/* Traverse other IBs (chain or next level) */
		r = amdgpu_bo_kmap(mapping->bo_va->base.bo, (void **)&buf);
		if (r != 0) {
			DRM_ERROR("failed to map bo\n");
			goto pop;
		}

		offset_in_bo = mapping->offset + ib_info->gpu_addr -
			(mapping->start << AMDGPU_GPU_PAGE_SHIFT);
		buf += offset_in_bo;
		bpmd->funcs->find_ibs((uint32_t*) buf, ib_info->size,
				      ib_info->vmid, ib_info_list);

pop:
		/* Remove ib_info once it's processed */
		sgpu_bpmd_remove_ib_info(ib_info);
	}
}


/**
 * sgpu_bpmd_find_ib_bos -
 *
 * @ring: pointer to the ring to walk for IBs
 * @bo_dump_list: list in which BOs will be accumulated for future dump
 *
 */
static void sgpu_bpmd_find_ib_bos(struct amdgpu_ring *ring,
				  struct list_head *bo_dump_list)
{
	int32_t r = 0;
	uint32_t rptr = ring->funcs->get_rptr(ring) & ring->buf_mask;
	uint32_t wptr = ring->funcs->get_wptr(ring) & ring->buf_mask;
	struct sgpu_bpmd *bpmd = &ring->adev->bpmd;
	struct list_head ib_info_list = {0};
	const uint32_t *buf = NULL;
	const uint32_t *start = NULL;
	static const uint32_t dw_byte = 4;
	uint32_t size =  0;

	DRM_DEBUG("%s: start dumping ib bos %d, %d", __func__, rptr, wptr);

	if (ring->ring_obj == NULL || rptr == wptr) {
		DRM_DEBUG("%s: nothing to dump - no pending command", __func__);
		return;
	}

	r = amdgpu_bo_kmap(ring->ring_obj, (void **)&buf);
	if (r != 0) {
		DRM_ERROR("failed to map bo\n");
		return;
	}

	INIT_LIST_HEAD(&ib_info_list);
	if (rptr < wptr) {
		start = buf + rptr;
		size = (wptr - rptr) * dw_byte;
		bpmd->funcs->find_ibs(start, size, SGPU_BPMD_INVALID_VMID,
				      &ib_info_list);
	} else {
		start = buf + rptr;
		size =  amdgpu_bo_size(ring->ring_obj) - (rptr * dw_byte);
		bpmd->funcs->find_ibs(start, size, SGPU_BPMD_INVALID_VMID,
				      &ib_info_list);
		start = buf;
		size = wptr * dw_byte;
		bpmd->funcs->find_ibs(start, size, SGPU_BPMD_INVALID_VMID,
				      &ib_info_list);
	}

	sgpu_bpmd_find_ib_bo_va_bfs(&ib_info_list, ring, bo_dump_list);
}

static void sgpu_bpmd_search_bos(struct amdgpu_device *sdev,
				 struct list_head *bo_dump_list)
{
	unsigned int i = 0;
	for (i = 0; i < sdev->gfx.num_gfx_rings; i++) {
		struct amdgpu_ring *ring = &sdev->gfx.gfx_ring[i];
		DRM_INFO("%s: dump gfx ring %d\n", __func__, i);
		update_vm_map(&ring->sched);
		sgpu_bpmd_register_bo(bo_dump_list, ring->ring_obj);
		sgpu_bpmd_register_bo(bo_dump_list, ring->mqd_obj);
		sgpu_bpmd_find_ib_bos(ring, bo_dump_list);
	}
}

/**
 * sgpu_bpmd_dump_ring - dump rings
 *
 * @sdev: pointer to sgpu_device
 * @f: pointer to a file
 *
 */
static void sgpu_bpmd_dump_rings(struct amdgpu_device *sdev,
				 struct file *f)
{
	struct sgpu_bpmd *bpmd = &sdev->bpmd;
	uint32_t i = 0;

	DRM_DEBUG("%s: start dumping rings (pos: %lld)\n", __func__, f->f_pos);

	for (i = 0; i < sdev->gfx.num_gfx_rings; i++) {
		struct amdgpu_ring *ring = &sdev->gfx.gfx_ring[i];
		DRM_INFO("%s: dump gfx ring %d\n", __func__, i);
		bpmd->funcs->dump_ring(f, ring);
	}

	// TODO: enable this when compute ring is ready on qemu
	/*
	for (i = 0; i < sdev->gfx.num_compute_rings; i++)
		DRM_INFO("%s: dump compute ring %d\n", __func__, i);
		bpmd->funcs->dump_ring(f, &sdev->gfx.compute_ring[i]);
	*/

	// TODO: Dump more rings
}

/**
 * sgpu_bpmd_dump_gem_bos - dump gem bos (created from user space)
 *
 * @sdev: pointer to sgpu_device
 * @f: pointer to a file
 *
 */
static void sgpu_bpmd_dump_gem_bos(struct amdgpu_device *sdev, struct file *f)
{
#ifdef SGPU_BPMD_ENABLE_GEM_BO_DUMP
	struct drm_device *ddev = sdev->ddev;
	struct drm_file *file = NULL;
	struct sgpu_bpmd *bpmd = &sdev->bpmd;

	DRM_INFO("sgpu, bpmd, start dumping gem bos (pos: %lld)\n", f->f_pos);

	mutex_lock(&ddev->filelist_mutex);
	list_for_each_entry(file, &ddev->filelist, lhead) {
		struct drm_gem_object *gobj = NULL;
		int handle = 0;

		spin_lock(&file->table_lock);
		idr_for_each_entry(&file->object_idr, gobj, handle) {
			struct amdgpu_bo *bo = gem_to_amdgpu_bo(gobj);
			bpmd->funcs->dump_bo(f, bo);
		}
		spin_unlock(&file->table_lock);
	}

	mutex_unlock(&ddev->filelist_mutex);
#endif /* SGPU_BPMD_ENABLE_GEM_BO_DUMP */
}

/**
 * sgpu_bpmd_dump_gem_bos - The VM dump should happen after every BOs are dumped
 * This is because the VA will map to a BO's packet ID, which can
 * only be know after it is dumped.
 *
 * @sdev: pointer to sgpu_device
 * @f: pointer to a file
 *
 */
static void sgpu_bpmd_dump_vms(struct amdgpu_device *sdev, struct file *f)
{
	int i = 0;
	for(i = 0; i < ARRAY_SIZE(vm); ++i) {
		if (vm[i])
			sgpu_bpmd_dump_vm(f, vm[i], i);
	}

	/* Mappings could change for the next dump.
	 * Make sure nothing carries over
	 */
	reinit_vm_map();
}

/**
 * sgpu_bpmd_is_valid - check bpmd setting
 *
 * @sdev: pointer to sgpu_device
 *
 * Return true if bpmd is valid, flase otherwise
 */
static bool sgpu_bpmd_is_valid(struct amdgpu_device *sdev)
{
	if ((sdev->bpmd.funcs->dump_header == NULL) ||
	    (sdev->bpmd.funcs->dump_reg == NULL) ||
	    (sdev->bpmd.funcs->dump_ring == NULL) ||
	    (sdev->bpmd.funcs->dump_system_info == NULL) ||
	    (sdev->bpmd.funcs->dump_bo == NULL)) {
		DRM_ERROR("bpmd function is not fully set");
		return false;
	}

	return true;
}

/**
 *  Dump BOs in list
 */
static void sgpu_bpmd_dump_bo_list(struct amdgpu_device *sdev,
				   struct file *f,
				   struct list_head *bo_list)
{
	struct sgpu_bpmd *bpmd = &sdev->bpmd;
	struct amdgpu_bo *bo;

	DRM_INFO("Dumping bo list");
	list_for_each_entry(bo, bo_list, bpmd_list) {
		bpmd->funcs->dump_bo(f, bo);
	}
}

static void sgpu_bpmd_clean_bo_list(struct list_head *bo_list)
{
	struct list_head *pos = NULL;
	struct list_head *n   = NULL;

	DRM_INFO("Cleaning bo list");
	list_for_each_safe(pos, n, bo_list) {
		struct amdgpu_bo *bo = list_entry(pos,
						  struct amdgpu_bo,
						  bpmd_list);

		bo->bpmd_packet_id = SGPU_BPMD_LAYOUT_INVALID_PACKET_ID;
		list_del_init(pos);
	}
}

/**
 * sgpu_bpmd_dump - create and dump a bpmd file
 *
 * @sdev: pointer to sgpu_device
 *
 */
#define MAX_BUF_SIZE 256
void sgpu_bpmd_dump(struct amdgpu_device *sdev)
{
	struct file *f = NULL;
	char filename[MAX_BUF_SIZE] = {};
	struct list_head bo_dump_list;
	/* @todo GFXSW-6022 - Remove the mutex */
	static DEFINE_MUTEX(dump_lock);
	INIT_LIST_HEAD(&bo_dump_list);

	if (sgpu_bpmd_is_valid(sdev) == false) {
		DRM_WARN("%s: skip bpmd", __func__);
		return;
	}

	sgpu_bpmd_get_filename(filename, MAX_BUF_SIZE);

	f = sgpu_bpmd_file_open(filename);
	if (f != NULL) {
		mutex_lock(&dump_lock);

		DRM_INFO("%s: start dumping to file (%s)\n", __func__, filename);

		sgpu_bpmd_dump_header(sdev, f);
		/* system_info is to follow immediately the header per file specs */
		sgpu_bpmd_dump_system_info(sdev, f);
		sgpu_bpmd_dump_regs(sdev, f);
		sgpu_bpmd_search_bos(sdev, &bo_dump_list);
		sgpu_bpmd_dump_gem_bos(sdev, f);
		/* BOs needs to be dumped beore the VM as the each mappings in
		   the VM dump will refer to a BO's packet ID */
		sgpu_bpmd_dump_bo_list(sdev, f, &bo_dump_list);
		/* mqd and ring object must be registered before the rings are dumped */
		sgpu_bpmd_dump_rings(sdev, f);
		sgpu_bpmd_dump_vms(sdev, f);
		sgpu_bpmd_dump_footer(sdev, f);

		sgpu_bpmd_clean_bo_list(&bo_dump_list);

		filp_close(f, NULL);

		DRM_INFO("%s: end dumping\n", __func__);

		mutex_unlock(&dump_lock);
	}
}

/**
 * sgpu_bpmd_get_packet_id - get a packet id
 *
 * Return a packet id
 */
uint32_t sgpu_bpmd_get_packet_id(void)
{
	static atomic_t id = ATOMIC_INIT(0);
	uint32_t ret = (uint32_t) atomic_inc_return(&id);
	return ret;
}
