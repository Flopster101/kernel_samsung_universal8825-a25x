/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/syscalls.h>
#include <linux/bug.h>
#if IS_ENABLED(CONFIG_PCI_EXYNOS)
#include <linux/exynos-pci-ctrl.h>
#endif

#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#if defined(CONFIG_SCHED_EHMP)
#include <linux/ehmp.h>
#elif defined(CONFIG_SCHED_EMS)
#include <linux/ems.h>
#endif

#include "is-binary.h"
#include "is-time.h"
#include "is-core.h"
#include "is-param.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"
#include "is-hw.h"
#include "is-spi.h"
#include "is-groupmgr.h"
#include "is-interface-wrap.h"
#include "is-device-ischain.h"
#include "is-dvfs.h"
#include "is-vender-specific.h"
#include "exynos-is-module.h"
#include "./sensor/module_framework/modules/is-device-module-base.h"
#include "is-vender-specific.h"
#include "pablo-device-iommu-group.h"
#include "pablo-obte.h"

static_assert((PARAMETER_MAX_SIZE * IS_PARAM_NUM) <=
		sizeof(((struct camera2_entry_ctl *)0)->parameter));

/* Default setting values */
#define DEFAULT_PREVIEW_STILL_WIDTH		(1280) /* sensor margin : 16 */
#define DEFAULT_PREVIEW_STILL_HEIGHT		(720) /* sensor margin : 12 */
#define DEFAULT_CAPTURE_VIDEO_WIDTH		(1920)
#define DEFAULT_CAPTURE_VIDEO_HEIGHT		(1080)
#define DEFAULT_CAPTURE_STILL_WIDTH		(2560)
#define DEFAULT_CAPTURE_STILL_HEIGHT		(1920)
#define DEFAULT_CAPTURE_STILL_CROP_WIDTH	(2560)
#define DEFAULT_CAPTURE_STILL_CROP_HEIGHT	(1440)
#define DEFAULT_PREVIEW_VIDEO_WIDTH		(640)
#define DEFAULT_PREVIEW_VIDEO_HEIGHT		(480)
#define DEFAULT_WIDTH		(320)
#define DEFAULT_HEIGHT		(240)

extern const struct is_subdev_ops is_subdev_paf_ops;
extern const struct is_subdev_ops is_subdev_3aa_ops;
#if IS_ENABLED(SOC_30S)
extern const struct is_subdev_ops is_subdev_3ac_ops;
extern const struct is_subdev_ops is_subdev_3ap_ops;
extern const struct is_subdev_ops is_subdev_3af_ops;
extern const struct is_subdev_ops is_subdev_3ag_ops;
extern const struct is_subdev_ops is_subdev_3ao_ops;
extern const struct is_subdev_ops is_subdev_3al_ops;
#endif
extern const struct is_subdev_ops is_subdev_byrp_ops;
#if IS_ENABLED(SOC_RGBP)
extern const struct is_subdev_ops is_subdev_rgbp_ops;
#endif
extern const struct is_subdev_ops is_subdev_isp_ops;
extern const struct is_subdev_ops is_subdev_ixc_ops;
extern const struct is_subdev_ops is_subdev_ixp_ops;
extern const struct is_subdev_ops is_subdev_ixt_ops;
extern const struct is_subdev_ops is_subdev_ixv_ops;
extern const struct is_subdev_ops is_subdev_ixw_ops;
extern const struct is_subdev_ops is_subdev_ixg_ops;
#if IS_ENABLED(SOC_ME0S)
extern const struct is_subdev_ops is_subdev_mexc_ops;
#else
const struct is_subdev_ops is_subdev_mexc_ops;
#endif
#if IS_ENABLED(SOC_ORBMCH)
extern const struct is_subdev_ops is_subdev_orb_ops;
#else
const struct is_subdev_ops is_subdev_orb_ops;
#endif
extern const struct is_subdev_ops is_subdev_orbxc_ops;
extern const struct is_subdev_ops is_subdev_mcs_ops;
extern const struct is_subdev_ops is_subdev_mcsp_ops;
extern const struct is_subdev_ops is_subdev_vra_ops;
#if IS_ENABLED(SOC_YPP)
extern const struct is_subdev_ops is_subdev_ypp_ops;
#else
const struct is_subdev_ops is_subdev_ypp_ops;
#endif
#if IS_ENABLED(SOC_LME0)
extern const struct is_subdev_ops is_subdev_lme_ops;
#else
const struct is_subdev_ops is_subdev_lme_ops;
#endif
#if IS_ENABLED(SOC_LME0S)
extern const struct is_subdev_ops is_subdev_lmes_ops;
#else
const struct is_subdev_ops is_subdev_lmes_ops;
#endif
#if IS_ENABLED(SOC_LME0C)
extern const struct is_subdev_ops is_subdev_lmec_ops;
#else
const struct is_subdev_ops is_subdev_lmec_ops;
#endif
#if IS_ENABLED(SOC_MCFP)
extern const struct is_subdev_ops is_subdev_mcfp_ops;
#else
const struct is_subdev_ops is_subdev_mcfp_ops;
#endif
#if IS_ENABLED(SOC_YUVP)
extern const struct is_subdev_ops is_subdev_yuvp_ops;
#else
const struct is_subdev_ops is_subdev_yuvp_ops;
#endif

static int is_ischain_group_shot(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame);

#if IS_ENABLED(SOC_VRA)
static const struct vra_param init_vra_param = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_YUV_FORMAT,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.format = 0,
		.bitwidth = 0,
		.order = 0,
		.plane = 0,
		.width = 0,
		.height = 0,
		.err = 0,
	},
	.config = {
		.cmd = FD_CONFIG_COMMAND_MAXIMUM_NUMBER |
			FD_CONFIG_COMMAND_ROLL_ANGLE |
			FD_CONFIG_COMMAND_YAW_ANGLE |
			FD_CONFIG_COMMAND_SMILE_MODE |
			FD_CONFIG_COMMAND_BLINK_MODE |
			FD_CONFIG_COMMAND_EYES_DETECT |
			FD_CONFIG_COMMAND_MOUTH_DETECT |
			FD_CONFIG_COMMAND_ORIENTATION |
			FD_CONFIG_COMMAND_ORIENTATION_VALUE,
		.mode = FD_CONFIG_MODE_NORMAL,
		.max_number = CAMERA2_MAX_FACES,
		.roll_angle = FD_CONFIG_ROLL_ANGLE_FULL,
		.yaw_angle = FD_CONFIG_YAW_ANGLE_45_90,
		.smile_mode = FD_CONFIG_SMILE_MODE_DISABLE,
		.blink_mode = FD_CONFIG_BLINK_MODE_DISABLE,
		.eye_detect = FD_CONFIG_EYES_DETECT_ENABLE,
		.mouth_detect = FD_CONFIG_MOUTH_DETECT_DISABLE,
		.orientation = FD_CONFIG_ORIENTATION_DISABLE,
		.orientation_value = 0,
		.map_width = 0,
		.map_height = 0,
		.err = ERROR_FD_NO,
	},
};
#endif

void is_ischain_savefirm(struct is_device_ischain *this)
{
#ifdef DEBUG_DUMP_FIRMWARE
	loff_t pos;

	CALL_BUFOP(this->minfo->pb_fw, sync_for_cpu,
		this->minfo->pb_fw,
		0,
		(size_t)FW_MEM_SIZE,
		DMA_FROM_DEVICE);

	write_data_to_file("/data/firmware.bin", (char *)this->minfo->kvaddr,
		(size_t)FW_MEM_SIZE, &pos);
#endif
}

static int is_ischain_open_setfile(struct is_device_ischain *device,
	struct is_vender *vender)
{
	int ret;
	const struct firmware *setfile = NULL;
	struct is_device_sensor *ids = device->sensor;
	u32 position = ids->position;
	struct is_resourcemgr *rscmgr = device->resourcemgr;
	struct is_mem *mem = &rscmgr->mem;
	struct is_minfo *im = &rscmgr->minfo;
	struct is_module_enum *module;
	ktime_t time_to_ready;

	mdbgd_ischain("%s\n", device, __func__);

	if (test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state)) {
		module = device->module_enum;
		goto skip_get_module;
	}

	ret = is_sensor_g_module(ids, &module);
	if (ret) {
		merr("is_sensor_g_module is fail(%d)", device, ret);
		return ret;
	}

skip_get_module:
	time_to_ready = ktime_get();

	/* a module has pinned setfile */
	if (!im->pb_setfile[position]) {
		/* a module has preloaded setfile */
		if (module->setfile_pb) {
			im->pb_setfile[position] = module->setfile_pb;
			im->kvaddr_setfile[position] = module->setfile_kva;
			carve_binary_version(IS_BIN_SETFILE, position, (void *)module->setfile_kva,
									module->setfile_size);
			im->pinned_setfile[position] = module->setfile_pinned;
			im->name_setfile[position] = module->setfile_name;
			if (!im->pinned_setfile[position])
				refcount_set(&im->refcount_setfile[position], 1);

			minfo("preloaded(%ldus) %s is ready to use, size: 0x%zx(0x%zx)\n", device,
							PABLO_KTIME_US_DELTA_NOW(time_to_ready),
							module->setfile_name,
							module->setfile_pb->size,
							module->setfile_size);

		} else {
			ret = request_firmware(&setfile, module->setfile_name, &device->pdev->dev);
			if (ret) {
				merr("failed to get %s(%d)", device, module->setfile_name, ret);
				return ret;
			}

			im->pb_setfile[position] = CALL_PTR_MEMOP(mem, alloc, mem->priv,
									setfile->size, NULL, 0);
			if (IS_ERR(im->pb_setfile[position])) {
				err("failed to allocate buffer for %s, size: 0x%zx",
						module->setfile_name, setfile->size);
				im->pb_setfile[position] = NULL;
				release_firmware(setfile);

				return -ENOMEM;
			}

			im->total_size += im->pb_setfile[position]->size;
			minfo("[MEM] allocation for %s, size: 0x%zx\n", device, module->setfile_name,
									im->pb_setfile[position]->size);

			im->kvaddr_setfile[position] = CALL_BUFOP(im->pb_setfile[position], kvaddr,
									im->pb_setfile[position]);
			memcpy((void *)im->kvaddr_setfile[position], setfile->data, setfile->size);
			carve_binary_version(IS_BIN_SETFILE, position, setfile->data, setfile->size);
			im->pinned_setfile[position] = module->setfile_pinned;
			im->name_setfile[position] = module->setfile_name;
			if (!im->pinned_setfile[position])
				refcount_set(&im->refcount_setfile[position], 1);

			minfo("reloaded(%ldus) %s is ready to use, size: 0x%zx(0x%zx)\n", device,
							PABLO_KTIME_US_DELTA_NOW(time_to_ready),
							im->name_setfile[position],
							im->pb_setfile[position]->size,
							setfile->size);

			release_firmware(setfile);
		}
	} else {
		if (!im->pinned_setfile[position])
			refcount_inc(&im->refcount_setfile[position]);

		minfo("pinned(%ldus) %s is ready to use, size: 0x%zx\n", device,
					PABLO_KTIME_US_DELTA_NOW(time_to_ready),
					im->name_setfile[position],
					im->pb_setfile[position]->size);
	}

	return 0;
}

static void is_ischain_close_setfile(struct is_device_ischain *device)
{
	struct is_device_sensor *ids = device->sensor;
	u32 position = ids->position;
	struct is_minfo *im = &device->resourcemgr->minfo;
	struct is_priv_buf *ipb = im->pb_setfile[position];
	bool pinned = im->pinned_setfile[position];
	struct is_module_enum *module;

	mdbgd_ischain("%s\n", device, __func__);

	if (!pinned && test_bit(IS_ISCHAIN_INIT, &device->state) &&
			refcount_dec_and_test(&im->refcount_setfile[position]) &&
			ipb) {
		minfo("[MEM] free %s, size: 0x%zx\n", device, im->name_setfile[position],
										ipb->size);

		im->total_size -= ipb->size;
		CALL_VOID_BUFOP(ipb, free, ipb);
		im->pb_setfile[position] = NULL;
		im->kvaddr_setfile[position] = 0UL;
		im->name_setfile[position] = NULL;
	}

	/* release the preloaded buffer of the module also */
	if (!pinned && !is_sensor_g_module(ids, &module) && module->setfile_pb) {
		module->setfile_pb = NULL;
		module->setfile_kva = 0UL;
		module->setfile_size = 0;
	}
}

static u64 is_itf_g_logical_group_id(struct is_device_ischain *device)
{
	u64 group_id = 0;
	struct is_group *next;
	struct is_group *pnext;
	struct is_group *group;

	group = get_ischain_leader_group(device);
	pnext = group->head->pnext;

	next = group;
	while (next) {
		group_id |= GROUP_ID(next->id) & GROUP_ID_PARM_MASK;
		next = next->next;
	}

	next = pnext;
	while (next && test_bit(IS_GROUP_USE_MULTI_CH, &next->state)) {
		group_id |= GROUP_ID(next->id) & GROUP_ID_PARM_MASK;
		next = next->next;
	}

	return group_id;
}

int is_itf_s_param(struct is_device_ischain *device,
	struct is_frame *frame,
	IS_DECLARE_PMAP(pmap))
{
	int ret = 0;
	u32 pi;
	ulong dst_base, src_base;

	FIMC_BUG(!device);

	if (frame) {
		IS_OR_PMAP(frame->pmap, pmap);

		dst_base = (ulong)&device->is_region->parameter;
		src_base = (ulong)frame->shot->ctl.vendor_entry.parameter;

		for (pi = 0; !IS_EMPTY_PMAP(pmap) && (pi < IS_PARAM_NUM); pi++) {
			if (test_bit(pi, pmap))
				memcpy((ulong *)(dst_base + (pi * PARAMETER_MAX_SIZE)),
						(ulong *)(src_base + (pi * PARAMETER_MAX_SIZE)),
						PARAMETER_MAX_SIZE);
		}
	} else {
		if (!IS_EMPTY_PMAP(pmap))
			ret = is_itf_s_param_wrap(device, pmap);
	}

	return ret;
}

void * is_itf_g_param(struct is_device_ischain *device,
	struct is_frame *frame,
	u32 index)
{
	ulong dst_base, src_base, dst_param, src_param;

	FIMC_BUG_NULL(!device);

	if (frame) {
		dst_base = (ulong)&frame->shot->ctl.vendor_entry.parameter[0];
		dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));
		src_base = (ulong)&device->is_region->parameter;
		src_param = (src_base + (index * PARAMETER_MAX_SIZE));

		memcpy((ulong *)dst_param, (ulong *)src_param, PARAMETER_MAX_SIZE);
	} else {
		dst_base = (ulong)&device->is_region->parameter;
		dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));
	}

	return (void *)dst_param;
}

int is_itf_a_param(struct is_device_ischain *device,
	u64 group)
{
	int ret = 0;

	FIMC_BUG(!device);

	ret = is_itf_a_param_wrap(device, group);

	return ret;
}

static int is_itf_f_param(struct is_device_ischain *device)
{
	int ret = 0;
	u64 group = 0;

	group = is_itf_g_logical_group_id(device);

	ret = is_itf_f_param_wrap(device, group);

	return ret;
}

static int is_itf_enum(struct is_device_ischain *device)
{
	int ret = 0;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_itf_enum_wrap(device);

	return ret;
}

void is_itf_storefirm(struct is_device_ischain *device)
{
	mdbgd_ischain("%s()\n", device, __func__);

	is_itf_storefirm_wrap(device);
}

void is_itf_restorefirm(struct is_device_ischain *device)
{
	mdbgd_ischain("%s()\n", device, __func__);

	is_itf_restorefirm_wrap(device);
}

int is_itf_set_fwboot(struct is_device_ischain *device, u32 val)
{
	int ret = 0;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_itf_set_fwboot_wrap(device, val);

	return ret;
}

static int is_itf_open(struct is_device_ischain *device, u32 flag)
{
	int ret = 0;
	struct is_region *region;
	struct is_core *core;
	struct is_vender *vender;

	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!device->sensor);
	FIMC_BUG(!device->interface);

	if (test_bit(IS_ISCHAIN_OPEN_STREAM, &device->state)) {
		merr("stream is already open", device);
		ret = -EINVAL;
		goto p_err;
	}

	region = device->is_region;

	core = (struct is_core *)platform_get_drvdata(device->pdev);
	vender = &core->vender;
	is_vender_itf_open(vender);

	memset(&region->fd_info, 0x0, sizeof(struct nfd_info));

#if IS_ENABLED(NFD_OBJECT_DETECT)
	memset(&region->od_info, 0x0, sizeof(struct od_info));
#endif

	ret = is_itf_open_wrap(device, flag);
	if (ret)
		goto p_err;

	set_bit(IS_ISCHAIN_OPEN_STREAM, &device->state);

p_err:
	return ret;
}

static int is_itf_close(struct is_device_ischain *device)
{
	int ret;

	if (!test_bit(IS_ISCHAIN_OPEN_STREAM, &device->state)) {
		mwarn("stream is already close", device);
		return 0;
	}

	ret = is_itf_close_wrap(device);

	if (!IS_ENABLED(SKIP_SETFILE_LOAD))
		is_ischain_close_setfile(device);

	clear_bit(IS_ISCHAIN_OPEN_STREAM, &device->state);

	return ret;
}

static DEFINE_MUTEX(setf_lock);
static int is_itf_setfile(struct is_device_ischain *device,
	struct is_vender *vender)
{
	int ret = 0;
	struct is_interface *itf = device->interface;
	ulong setfile_addr;
	u32 position = device->sensor->position;

	mutex_lock(&setf_lock);

	if (!IS_ENABLED(SKIP_SETFILE_LOAD)) {
		ret = is_ischain_open_setfile(device, vender);
		if (ret) {
			merr("is_ischain_loadsetf is fail(%d)", device, ret);
			goto p_err;
		}
	}

	setfile_addr = device->resourcemgr->minfo.kvaddr_setfile[position];
	ret = is_itf_setfile_wrap(itf, setfile_addr, device);
	if (ret)
		goto p_err;

p_err:
	mutex_unlock(&setf_lock);
	return ret;
}

int is_itf_stream_on(struct is_device_ischain *device)
{
	int ret = 0;
	u32 retry = 30000;
	struct is_device_sensor *sensor;
	struct is_groupmgr *groupmgr;
	struct is_group *group_leader;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;
	struct is_resourcemgr *resourcemgr;
	struct is_group_task *gtask;
	u32 scount, init_shots, qcount;

	FIMC_BUG(!device);
	FIMC_BUG(!device->groupmgr);
	FIMC_BUG(!device->resourcemgr);
	FIMC_BUG(!device->sensor);

	sensor = device->sensor;
	groupmgr = device->groupmgr;
	resourcemgr = device->resourcemgr;
	group_leader = groupmgr->leader[device->instance];
	if (!group_leader) {
		merr("stream leader is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	gtask = &groupmgr->gtask[group_leader->id];
	init_shots = group_leader->init_shots;
	scount = atomic_read(&group_leader->scount);

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group_leader, 0);
	if (!framemgr) {
		merr("leader framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	qcount = framemgr->queued_count[FS_REQUEST] + framemgr->queued_count[FS_PROCESS];

	if (init_shots) {
		/* 3ax  group should be started */
		if (!test_bit(IS_GROUP_START, &group_leader->state)) {
			merr("stream leader is NOT started", device);
			ret = -EINVAL;
			goto p_err;
		}

		while (--retry && (scount < init_shots)) {
			udelay(100);
			scount = atomic_read(&group_leader->scount);
		}
	}

	/*
	 * If batch(FRO) mode is used,
	 * asyn shot count doesn't have to bigger than MIN_OF_ASYNC_SHOTS(=1),
	 * because it will be operated like 60 fps.
	 */
	framemgr_e_barrier_irqs(framemgr, 0, flags);
	frame = peek_frame(framemgr, FS_PROCESS);
	framemgr_x_barrier_irqr(framemgr, 0, flags);
	if (frame && frame->num_buffers <= 1) {
		/* trigger one more asyn_shots */
		if (is_sensor_g_fast_mode(sensor) == 1) {
			group_leader->asyn_shots += 1;
			group_leader->skip_shots = group_leader->asyn_shots;
			atomic_inc(&group_leader->smp_shot_count);
			up(&group_leader->smp_trigger);
			up(&gtask->smp_resource);
		}
	}

	minfo("[ISC:D] stream on %s ready(scnt: %d, qcnt: %d, init: %d, asyn: %d, skip: %d)\n",
		device, retry ? "OK" : "NOT", scount, qcount, init_shots,
		group_leader->asyn_shots, group_leader->skip_shots);

	is_resource_set_global_param(resourcemgr, device);
	is_resource_update_lic_sram(resourcemgr, device, true);

	if (is_get_debug_param(IS_DEBUG_PARAM_CLK) > 0)
		CALL_POPS(device, clk_print);

	ret = is_itf_stream_on_wrap(device);

p_err:
	return ret;
}

int is_itf_stream_off(struct is_device_ischain *device)
{
	int ret = 0;
	struct is_resourcemgr *resourcemgr;

	FIMC_BUG(!device);

	minfo("[ISC:D] stream off ready\n", device);

	ret = is_itf_stream_off_wrap(device);

	resourcemgr = device->resourcemgr;
	is_resource_clear_global_param(resourcemgr, device);
	is_resource_update_lic_sram(resourcemgr, device, false);

	return ret;
}

int is_itf_process_start(struct is_device_ischain *device,
	u64 group)
{
	int ret = 0;

	ret = is_itf_process_on_wrap(device, group);

	return ret;
}

int is_itf_process_stop(struct is_device_ischain *device,
	u64 group)
{
	int ret = 0;

	ret = is_itf_process_off_wrap(device, group, 0);

	return ret;
}

int is_itf_force_stop(struct is_device_ischain *device,
	u64 group)
{
	int ret = 0;

	ret = is_itf_process_off_wrap(device, group, 1);

	return ret;
}

static int is_itf_init_process_start(struct is_device_ischain *device)
{
	int ret = 0;
	u64 group = 0;

	group = is_itf_g_logical_group_id(device);

	ret = is_itf_process_on_wrap(device, group);

	return ret;
}

int is_itf_i2c_lock(struct is_device_ischain *this,
	int i2c_clk, bool lock)
{
	int ret = 0;

	return ret;
}

#ifdef PRINT_CAPABILITY
static int is_itf_g_capability(struct is_device_ischain *this)
{
	int ret = 0;
	u32 metadata;
	u32 index;
	struct camera2_sm *capability;

	memcpy(&this->capability, &this->is_region->shared,
		sizeof(struct camera2_sm));
	capability = &this->capability;

	printk(KERN_INFO "===ColorC================================\n");
	printk(KERN_INFO "===ToneMapping===========================\n");
	metadata = capability->tonemap.maxCurvePoints;
	printk(KERN_INFO "maxCurvePoints : %d\n", metadata);

	printk(KERN_INFO "===Scaler================================\n");
	printk(KERN_INFO "foramt : %d, %d, %d, %d\n",
		capability->scaler.availableFormats[0],
		capability->scaler.availableFormats[1],
		capability->scaler.availableFormats[2],
		capability->scaler.availableFormats[3]);

	printk(KERN_INFO "===StatisTicsG===========================\n");
	index = 0;
	metadata = capability->stats.availableFaceDetectModes[index];
	while (metadata) {
		printk(KERN_INFO "availableFaceDetectModes : %d\n", metadata);
		index++;
		metadata = capability->stats.availableFaceDetectModes[index];
	}
	printk(KERN_INFO "maxFaceCount : %d\n",
		capability->stats.maxFaceCount);
	printk(KERN_INFO "histogrambucketCount : %d\n",
		capability->stats.histogramBucketCount);
	printk(KERN_INFO "maxHistogramCount : %d\n",
		capability->stats.maxHistogramCount);
	printk(KERN_INFO "sharpnessMapSize : %dx%d\n",
		capability->stats.sharpnessMapSize[0],
		capability->stats.sharpnessMapSize[1]);
	printk(KERN_INFO "maxSharpnessMapValue : %d\n",
		capability->stats.maxSharpnessMapValue);

	printk(KERN_INFO "===3A====================================\n");
	printk(KERN_INFO "maxRegions : %d\n", capability->aa.maxRegions);

	index = 0;
	metadata = capability->aa.aeAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "aeAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.aeAvailableModes[index];
	}
	printk(KERN_INFO "aeCompensationStep : %d,%d\n",
		capability->aa.aeCompensationStep.num,
		capability->aa.aeCompensationStep.den);
	printk(KERN_INFO "aeCompensationRange : %d ~ %d\n",
		capability->aa.aeCompensationRange[0],
		capability->aa.aeCompensationRange[1]);
	index = 0;
	metadata = capability->aa.aeAvailableTargetFpsRanges[index][0];
	while (metadata) {
		printk(KERN_INFO "TargetFpsRanges : %d ~ %d\n", metadata,
			capability->aa.aeAvailableTargetFpsRanges[index][1]);
		index++;
		metadata = capability->aa.aeAvailableTargetFpsRanges[index][0];
	}
	index = 0;
	metadata = capability->aa.aeAvailableAntibandingModes[index];
	while (metadata) {
		printk(KERN_INFO "aeAvailableAntibandingModes : %d\n",
			metadata);
		index++;
		metadata = capability->aa.aeAvailableAntibandingModes[index];
	}
	index = 0;
	metadata = capability->aa.awbAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "awbAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.awbAvailableModes[index];
	}
	index = 0;
	metadata = capability->aa.afAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "afAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.afAvailableModes[index];
	}
	return ret;
}
#endif

int is_itf_power_down(struct is_interface *interface)
{
	int ret = 0;

	ret = is_itf_power_down_wrap(interface, 0);

	return ret;
}

int is_itf_sys_ctl(struct is_device_ischain *this,
			int cmd, int val)
{
	int ret = 0;

	ret = is_itf_sys_ctl_wrap(this, cmd, val);

	return ret;
}

static int is_itf_sensor_mode(struct is_device_ischain *device)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *cfg;

	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		goto p_err;

	sensor = device->sensor;

	cfg = is_sensor_g_mode(sensor);
	if (!cfg) {
		merr("is_sensor_g_mode is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_itf_sensor_mode_wrap(device, cfg);
	if (ret)
		goto p_err;

p_err:
	return ret;
}

int is_itf_grp_shot(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame)
{
	int ret = 0;
#ifdef CONFIG_USE_SENSOR_GROUP
	unsigned long flags;
	struct is_group *head;
	struct is_framemgr *framemgr;
	bool is_remosaic_preview = false;
#endif
	FIMC_BUG(!device);
	FIMC_BUG(!group);
	FIMC_BUG(!frame);
	FIMC_BUG(!frame->shot);

	mgrdbgs(1, " SHOT(%d)\n", device, group, frame, frame->index);

#ifdef CONFIG_USE_SENSOR_GROUP

#ifdef ENABLE_MODECHANGE_CAPTURE
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)
		&& is_vendor_check_remosaic_mode_change(frame)) {
		is_remosaic_preview = true;
	}
#endif

	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, group);
	if (head && !is_remosaic_preview) {
		ret = is_itf_shot_wrap(device, group, frame);
	} else {
		framemgr = GET_HEAD_GROUP_FRAMEMGR(group, frame->cur_shot_idx);
		if (!framemgr) {
			merr("framemgr is NULL", group);
			ret = -EINVAL;
			goto p_err;
		}

		set_bit(group->leader.id, &frame->out_flag);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_25, flags);
		trans_frame(framemgr, frame, FS_PROCESS);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_25, flags);
	}
#else
	ret = is_itf_shot_wrap(device, group, frame);
#endif

p_err:
	return ret;
}

int is_ischain_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct is_core *core = (struct is_core *)dev_get_drvdata(dev);
	struct exynos_platform_is *pdata;
	int refcount;
	pdata = dev_get_platdata(dev);

	FIMC_BUG(!pdata);
	FIMC_BUG(!pdata->clk_off);

	info("FIMC_IS runtime suspend in\n");

#if IS_ENABLED(CONFIG_PCI_EXYNOS)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
	exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_CAMERA, 0);
#else
	exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_CAMERA);
#endif
#endif

	ret = pdata->clk_off(&pdev->dev);
	if (ret)
		err("clk_off is fail(%d)", ret);

	refcount = atomic_dec_return(&core->resourcemgr.qos_refcount);
	if (refcount == 0)
		is_remove_dvfs(core, START_DVFS_LEVEL);

	is_hardware_runtime_suspend(&core->hardware);
	info("FIMC_IS runtime suspend out\n");
	return 0;
}

int is_ischain_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct is_core *core = (struct is_core *)dev_get_drvdata(dev);
	struct exynos_platform_is *pdata;
	int refcount;
	pdata = dev_get_platdata(dev);

	FIMC_BUG(!pdata);
	FIMC_BUG(!pdata->clk_cfg);
	FIMC_BUG(!pdata->clk_on);

	info("FIMC_IS runtime resume in\n");

	ret = is_ischain_runtime_resume_pre(dev);
	if (ret) {
		err("is_runtime_resume_pre is fail(%d)", ret);
		goto p_err;
	}

	ret = pdata->clk_cfg(&pdev->dev);
	if (ret) {
		err("clk_cfg is fail(%d)", ret);
		goto p_err;
	}

	/*
	 * DVFS level should be locked after power on.
	 */
	refcount = atomic_inc_return(&core->resourcemgr.qos_refcount);
	if (refcount == 1)
		is_add_dvfs(core, START_DVFS_LEVEL);

	/* Clock on */
	ret = pdata->clk_on(&pdev->dev);
	if (ret) {
		err("clk_on is fail(%d)", ret);
		goto p_err;
	}

#if IS_ENABLED(CONFIG_PCI_EXYNOS)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
		exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_CAMERA, 0);
#else
		exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_CAMERA);
#endif
#endif

	is_hardware_runtime_resume(&core->hardware);
p_err:
	info("FIMC-IS runtime resume out\n");
	return ret;
}

int is_ischain_power(struct is_device_ischain *device, int on)
{
	int ret = 0;
	int retry = 4;
	struct device *dev;
	struct is_core *core;
	struct is_vender *vender;
	int i;
	struct pablo_device_iommu_group_module *iommu_group_module;

	FIMC_BUG(!device);
	FIMC_BUG(!device->interface);

	dev = &device->pdev->dev;
	core = (struct is_core *)dev_get_drvdata(dev);
	vender = &core->vender;

	if (on) {
		/* runtime suspend callback can be called lately because of power relationship */
		while (test_bit(IS_ISCHAIN_POWER_ON, &device->state) && (retry > 0)) {
			mwarn("sensor is not yet power off", device);
			msleep(500);
			--retry;
		}
		if (!retry) {
			ret = -EBUSY;
			goto p_err;
		}

		/* 2. FIMC-IS local power enable */
#if defined(CONFIG_PM)
		mdbgd_ischain("pm_runtime_suspended = %d\n", device, pm_runtime_suspended(dev));
		ret = pm_runtime_get_sync(dev);
		if (ret < 0)
			merr("pm_runtime_get_sync() return error: %d", device, ret);

		iommu_group_module = pablo_iommu_group_module_get();
		if (iommu_group_module) {
			for (i = 0; i < iommu_group_module->num_of_groups; i++) {
				ret = pm_runtime_get_sync(iommu_group_module->iommu_group[i]->dev);
				if (ret < 0)
					merr("pm_runtime_get_sync(iommu_group[%d]) is fail(%d)",
						device, i, ret);
			}
		}
#else
		is_ischain_runtime_resume(dev);
		minfo("%s(%d) - is runtime resume complete\n", device, __func__, on);
#endif

		ret = is_ischain_runtime_resume_post(dev);
		if (ret)
			merr("is_runtime_suspend_post is fail(%d)", device, ret);

		set_bit(IS_ISCHAIN_LOADED, &device->state);
		set_bit(IS_ISCHAIN_POWER_ON, &device->state);
	} else {
		/* FIMC-IS local power down */
		ret = is_ischain_runtime_suspend_post(dev);
		if (ret)
			merr("is_runtime_suspend_post is fail(%d)", device, ret);

#if defined(CONFIG_PM)
		ret = pm_runtime_put_sync(dev);
		if (ret)
			merr("pm_runtime_put_sync is fail(%d)", device, ret);

		iommu_group_module = pablo_iommu_group_module_get();
		if (iommu_group_module) {
			for (i = 0; i < iommu_group_module->num_of_groups; i++) {
				ret = pm_runtime_put_sync(iommu_group_module->iommu_group[i]->dev);
				if (ret < 0)
					merr("pm_runtime_put_sync(iommu_group[%d]) is fail(%d)",
						device, i, ret);
			}
		}
#else
		ret = is_ischain_runtime_suspend(dev);
		if (ret)
			merr("is_runtime_suspend is fail(%d)", device, ret);
#endif

		clear_bit(IS_ISCHAIN_POWER_ON, &device->state);
	}

p_err:

	minfo("%s(%d):%d\n", device, __func__, on, ret);
	return ret;
}

int is_ischain_s_sensor_size(struct is_device_ischain *device,
	struct is_frame *frame, IS_DECLARE_PMAP(pmap))
{
	int ret = 0;
	struct param_sensor_config *sensor_config;
	struct is_device_sensor *sensor = device->sensor;
	struct is_device_sensor_peri *sensor_peri;
	struct is_cis *cis;
	struct is_module_enum *module;
	struct is_sensor_cfg *cfg;
	u32 binning, bns_binning;
	u32 sensor_width, sensor_height;
	u32 bns_width, bns_height;
	u32 framerate;
	u32 ex_mode;
	u32 pixel_order = 0; /* 0: GRBG, 1: RGGB, 2: BGGR, 3: GBRG */
	u32 mono_mode = 0;

	FIMC_BUG(!sensor);

	ret = is_sensor_g_module(sensor, &module);
	if (ret) {
		merr("is_sensor_g_module is fail(%d)", device, ret);
		return -EINVAL;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	if (!sensor_peri) {
		mwarn("sensor_peri is null", device);
		return -EINVAL;
	}
	cis = &sensor_peri->cis;

	if (!module->pdata) {
		merr("module pdata is NULL", device);
		goto p_sensor_config;
	}

	if (module->pdata->sensor_module_type == SENSOR_TYPE_MONO)
		mono_mode = 1;
	minfo("[ISC:D] mono sensor(%d)\n", device, mono_mode);


	if (sensor->obte_config & BIT(OBTE_CONFIG_BIT_BAYER_ORDER_EN))
		pixel_order = (sensor->obte_config & (BIT(OBTE_CONFIG_BIT_BAYER_ORDER_LSB) |
				BIT(OBTE_CONFIG_BIT_BAYER_ORDER_MSB))) >>
				OBTE_CONFIG_BIT_BAYER_ORDER_LSB;
	else
		pixel_order = cis->bayer_order;

p_sensor_config:
	cfg = is_sensor_g_mode(sensor);
	if (!cfg) {
		merr("is_sensor_g_mode is fail", device);
		return -EINVAL;
	}

	sensor_width = is_sensor_g_width(sensor);
	sensor_height = is_sensor_g_height(sensor);
	bns_width = is_sensor_g_bns_width(sensor);
	bns_height = is_sensor_g_bns_height(sensor);
	framerate = is_sensor_g_framerate(sensor);
	ex_mode = is_sensor_g_ex_mode(sensor);

	if (ex_mode == EX_LOW_RES_TETRA)
		binning = is_sensor_g_sensorcrop_bratio(sensor);
	else
		binning = is_sensor_g_bratio(sensor);

	minfo("[ISC:D] binning(%d):%d\n", device, binning);

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		bns_binning = 1000;
	else
		bns_binning = is_sensor_g_bns_ratio(sensor);

	minfo("[ISC:D] window_width(%d), window_height(%d), otf_width(%d), otf_height(%d)\n", device,
		device->sensor->image.window.width, device->sensor->image.window.height,
		device->sensor->image.window.otf_width, device->sensor->image.window.otf_height);

	sensor_config = is_itf_g_param(device, frame, PARAM_SENSOR_CONFIG);
	sensor_config->width = sensor_width;
	sensor_config->height = sensor_height;
	sensor_config->calibrated_width = module->pixel_width;
	sensor_config->calibrated_height = module->pixel_height;
	sensor_config->sensor_binning_ratio_x = binning;
	sensor_config->sensor_binning_ratio_y = binning;
	sensor_config->bns_binning_ratio_x = bns_binning;
	sensor_config->bns_binning_ratio_y = bns_binning;
	sensor_config->bns_margin_left = 0;
	sensor_config->bns_margin_top = 0;
	sensor_config->bns_output_width = bns_width;
	sensor_config->bns_output_height = bns_height;
	sensor_config->frametime = 10 * 1000 * 1000; /* max exposure time */
	sensor_config->pixel_order = pixel_order;
	sensor_config->vvalid_time = cfg->vvalid_time;
	sensor_config->req_vvalid_time = cfg->req_vvalid_time;
	sensor_config->freeform_sensor_crop_enable = cis->freeform_sensor_crop.enable;
	sensor_config->freeform_sensor_crop_offset_x = cis->freeform_sensor_crop.x_start;
	sensor_config->freeform_sensor_crop_offset_y = cis->freeform_sensor_crop.y_start;

	if (IS_ENABLED(FIXED_FPS_DEBUG)) {
		sensor_config->min_target_fps = FIXED_MAX_FPS_VALUE;
		sensor_config->max_target_fps = FIXED_MAX_FPS_VALUE;
	} else {
		if (sensor->min_target_fps > 0)
			sensor_config->min_target_fps = sensor->min_target_fps;
		if (sensor->max_target_fps > 0)
			sensor_config->max_target_fps = sensor->max_target_fps;
	}

	if (is_sensor_g_fast_mode(sensor) == 1)
		sensor_config->early_config_lock = 1;
	else
		sensor_config->early_config_lock = 0;

#ifdef USE_MONO_SENSOR
	sensor_config->mono_mode = mono_mode;
#endif

	set_bit(PARAM_SENSOR_CONFIG, pmap);

	return ret;
}

static void is_ischain_calc_plane_dva(u32 planes, dma_addr_t dva[], u32 pixelformat,
				u32 width, u32 height, u32 target_addr[])
{
	int i, j;

	switch (pixelformat) {
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		for (i = 0; i < planes; i++) {
			j = i * 2;
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = target_addr[j] + (width * height);
		}
		break;
	case V4L2_PIX_FMT_YVU420M:
		for (i = 0; i < planes; i += 3) {
			target_addr[i] = (typeof(*target_addr))dva[i];
			target_addr[i + 1] = (typeof(*target_addr))dva[i + 2];
			target_addr[i + 2] = (typeof(*target_addr))dva[i + 1];
		}
		break;
	case V4L2_PIX_FMT_YUV420:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = target_addr[j] + (width * height);
			target_addr[j + 2] = target_addr[j + 1] + (width * height / 4);
		}
		break;
	case V4L2_PIX_FMT_YVU420: /* AYV12 spec: The width should be aligned by 16 pixel. */
		for (i = 0; i < planes; i++) {
			j = i * 3;
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 2] = target_addr[j] + (ALIGN(width, 16) * height);
			target_addr[j + 1] = target_addr[j + 2] + (ALIGN(width / 2, 16) * height / 2);
		}
		break;
	case V4L2_PIX_FMT_YUV422P:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = target_addr[j] + (width * height);
			target_addr[j + 2] = target_addr[j + 1] + (width * height / 2);
		}
		break;
	case V4L2_PIX_FMT_NV12M_S10B:
	case V4L2_PIX_FMT_NV21M_S10B:
		for (i = 0; i < planes; i += 2) {
			j = i * 2;
			/* Y_ADDR, UV_ADDR, Y_2BIT_ADDR, UV_2BIT_ADDR */
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = (typeof(*target_addr))dva[i + 1];
			target_addr[j + 2] = target_addr[j] + NV12M_Y_SIZE(width, height);
			target_addr[j + 3] = target_addr[j + 1] + NV12M_CBCR_SIZE(width, height);
		}
		break;
	case V4L2_PIX_FMT_NV16M_S10B:
	case V4L2_PIX_FMT_NV61M_S10B:
		for (i = 0; i < planes; i += 2) {
			j = i * 2;
			/* Y_ADDR, UV_ADDR, Y_2BIT_ADDR, UV_2BIT_ADDR */
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = (typeof(*target_addr))dva[i + 1];
			target_addr[j + 2] = target_addr[j] + NV16M_Y_SIZE(width, height);
			target_addr[j + 3] = target_addr[j + 1] + NV16M_CBCR_SIZE(width, height);
		}
		break;
	case V4L2_PIX_FMT_RGB24:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			target_addr[j + 2] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = target_addr[j + 2] + (width * height);
			target_addr[j] = target_addr[j + 1] + (width * height);
		}
		break;
	case V4L2_PIX_FMT_BGR24:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			target_addr[j] = (typeof(*target_addr))dva[i];
			target_addr[j + 1] = target_addr[j] + (width * height);
			target_addr[j + 2] = target_addr[j + 1] + (width * height);
		}
		break;
	default:
		for (i = 0; i < planes; i++)
			target_addr[i] = (typeof(*target_addr))dva[i];
		break;
	}
}

static void is_ischain_calc_plane_kva(u32 planes, ulong kva[], u32 pixelformat,
				u32 width, u32 height, uint64_t target_addr[])
{
	int i;

	switch (pixelformat) {
	case V4L2_PIX_FMT_Y12:	/* Only for ME : kernel virtual addr*/
	case V4L2_PIX_FMT_YUV32:	/* Only for 32bit data : kernel virtual addr*/
	case V4L2_PIX_FMT_GREY:	/* Only for 1 plane, 1 byte unit custom data*/
		for (i = 0; i < planes; i++)
			target_addr[i] = kva[i];
		break;
	default:
		for (i = 0; i < planes; i++)
			target_addr[i] = kva[i];
		break;
	}
}

int is_ischain_buf_tag_input(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	u32 target_addr[])
{
	int ret = 0;
	int i;
	unsigned long flags;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	FIMC_BUG(!framemgr);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		is_ischain_calc_plane_dva(frame->planes, frame->dvaddr_buffer,
					pixelformat, width, height, target_addr);

		set_bit(subdev->id, &ldr_frame->out_flag);

		/* for draw digit */
		frame->width = width;
		frame->height = height;
	} else {
		for (i = 0; i < IS_MAX_PLANES; i++)
			target_addr[i] = 0;

		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);

	return ret;
}

int is_ischain_buf_tag(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	u32 target_addr[])
{
	int ret = 0;
	int i;
	unsigned long flags;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct camera2_node *capture;
	enum is_frame_state next_state = FS_PROCESS;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	FIMC_BUG(!framemgr);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		struct camera2_stream	*stream;

		stream = frame->stream;
		if (!stream) {
			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
			merr("frame->stream is NULL", device);
			BUG();
		}

		is_ischain_calc_plane_dva(frame->planes, frame->dvaddr_buffer,
					pixelformat, width, height, target_addr);

		capture = &ldr_frame->shot_ext->node_group.capture[subdev->cid];

		frame->fcount = ldr_frame->fcount;
		stream->findex = ldr_frame->index;
		stream->fcount = ldr_frame->fcount;

		if (ldr_frame->stripe_info.region_num
			&& ldr_frame->stripe_info.region_id < ldr_frame->stripe_info.region_num - 1)
			/* Still being stripe processed. */
			next_state = FS_STRIPE_PROCESS;

		if (likely(capture->vid == subdev->vid)) {
			stream->input_crop_region[0] = capture->input.cropRegion[0];
			stream->input_crop_region[1] = capture->input.cropRegion[1];
			stream->input_crop_region[2] = capture->input.cropRegion[2];
			stream->input_crop_region[3] = capture->input.cropRegion[3];
			stream->output_crop_region[0] = capture->output.cropRegion[0];
			stream->output_crop_region[1] = capture->output.cropRegion[1];
			stream->output_crop_region[2] = capture->output.cropRegion[2];
			stream->output_crop_region[3] = capture->output.cropRegion[3];
		} else {
			mserr("capture vid is changed(%d != %d)(%d)",
				subdev, subdev, subdev->vid,
				capture->vid, subdev->cid);
			stream->input_crop_region[0] = 0;
			stream->input_crop_region[1] = 0;
			stream->input_crop_region[2] = 0;
			stream->input_crop_region[3] = 0;
			stream->output_crop_region[0] = 0;
			stream->output_crop_region[1] = 0;
			stream->output_crop_region[2] = 0;
			stream->output_crop_region[3] = 0;
		}

		set_bit(subdev->id, &ldr_frame->out_flag);

		/* for draw digit */
		frame->width = width;
		frame->height = height;

		trans_frame(framemgr, frame, next_state);
	} else {
		for (i = 0; i < IS_MAX_PLANES; i++)
			target_addr[i] = 0;

		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);

	return ret;
}

int is_ischain_buf_tag_64bit(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	uint64_t target_addr[])
{
	int ret = 0;
	int i;
	unsigned long flags;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	BUG_ON(!framemgr);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	frame = peek_frame(framemgr, ldr_frame->state);
	if (frame) {
		struct camera2_stream	*stream;

		stream = frame->stream;
		if (!stream) {
			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
			merr("frame->stream is NULL", device);
			BUG();
		}

		is_ischain_calc_plane_kva(frame->planes, frame->kvaddr_buffer,
					pixelformat, width, height, target_addr);

		frame->fcount = ldr_frame->fcount;
		stream->findex = ldr_frame->index;
		stream->fcount = ldr_frame->fcount;

		set_bit(subdev->id, &ldr_frame->out_flag);

		/* for draw digit */
		frame->width = width;
		frame->height = height;

		trans_frame(framemgr, frame, FS_PROCESS);
	} else {
		for (i = 0; i < IS_MAX_PLANES; i++)
			target_addr[i] = 0;

		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);

	return ret;
}

static int is_ischain_chg_setfile(struct is_device_ischain *device)
{
	int ret = 0;
	u64 group = 0;

	FIMC_BUG(!device);

	group = is_itf_g_logical_group_id(device);

	ret = is_itf_process_stop(device, group);
	if (ret) {
		merr("is_itf_process_stop fail", device);
		goto p_err;
	}

	ret = is_itf_a_param(device, group);
	if (ret) {
		merr("is_itf_a_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_itf_process_start(device, group);
	if (ret) {
		merr("is_itf_process_start fail", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	minfo("[ISC:D] %s(%d):%d\n", device, __func__,
		device->setfile & IS_SETFILE_MASK, ret);
	return ret;
}

#ifdef ENABLE_VRA
static int is_ischain_vra_bypass(struct is_device_ischain *device,
	struct is_frame *frame,
	bool bypass)
{
	int ret;
	struct param_control *control;
	struct is_group *group;
	struct is_subdev *leader, *subdev;
	IS_DECLARE_PMAP(pmap);

	FIMC_BUG(!device);

	subdev = &device->group_vra.leader;
	leader = subdev->leader;
	group = container_of(leader, struct is_group, leader);
	IS_INIT_PMAP(pmap);

	if (!test_bit(IS_SUBDEV_START, &subdev->state)) {
		mserr("subdev is not start", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	control = is_itf_g_param(device, frame, PARAM_FD_CONTROL);
	if (bypass)
		control->cmd = CONTROL_COMMAND_STOP;
	else
		control->cmd = CONTROL_COMMAND_START;

	control->bypass = CONTROL_BYPASS_DISABLE;
	set_bit(PARAM_FD_CONTROL, pmap);

	ret = is_itf_s_param(device, frame, pmap);
	if (ret) {
		merr("is_itf_s_param is fail(%d)", device, ret);
		goto p_err;
	}

	if (bypass)
		clear_bit(IS_SUBDEV_RUN, &subdev->state);
	else
		set_bit(IS_SUBDEV_RUN, &subdev->state);

p_err:
	msrinfo("bypass : %d\n", device, subdev, frame, bypass);
	return ret;
}
#endif


int is_ischain_g_ddk_capture_meta_update(struct is_group *group, struct is_device_ischain *device,
	u32 fcount, u32 size, ulong addr)
{
	struct device *dev;
	struct is_core *core;

	dev = &device->pdev->dev;
	core = is_get_is_core();

	return  is_hardware_capture_meta_request(&core->hardware, group, device->instance, fcount, size, addr);
}

int is_ischain_g_ddk_setfile_version(struct is_device_ischain *device,
	void *user_ptr)
{
	int ret = 0;
	int position;
	struct ddk_setfile_ver *version_info;

	version_info = vzalloc(sizeof(struct ddk_setfile_ver));
	if (!version_info) {
		merr("version_info is NULL", device);
		ret = -ENOMEM;
		goto p_err;
	}

	strncpy(version_info->ddk_version, get_binary_version(IS_BIN_LIBRARY, IS_BIN_LIB_HINT_DDK), 60);
	position = device->sensor->position;
	strncpy(version_info->setfile_version, get_binary_version(IS_BIN_SETFILE, position), 60);
	version_info->header1 = SETFILE_VERSION_INFO_HEADER1;
	version_info->header2 = SETFILE_VERSION_INFO_HEADER2;

	ret = copy_to_user(user_ptr, version_info, sizeof(struct ddk_setfile_ver));

	vfree(version_info);
p_err:
	return ret;
}

int is_ischain_g_capability(struct is_device_ischain *device,
	ulong user_ptr)
{
	int ret = 0;
#ifdef PRINT_CAPABILITY
	struct camera2_sm *capability;

	capability = vzalloc(sizeof(struct camera2_sm));
	if (!capability) {
		merr("capability is NULL", device);
		ret = -ENOMEM;
		goto p_err;
	}

	ret = is_itf_g_capability(device);
	if (ret) {
		merr("is_itf_g_capability is fail(%d)", device, ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = copy_to_user((void *)user_ptr, capability, sizeof(struct camera2_sm));

p_err:
	vfree(capability);
#endif

	return ret;
}

int is_ischain_probe(struct is_device_ischain *device,
	struct is_interface *interface,
	struct is_resourcemgr *resourcemgr,
	struct is_groupmgr *groupmgr,
	struct is_devicemgr *devicemgr,
	struct platform_device *pdev,
	u32 instance)
{
	int ret = 0;
#if IS_ENABLED(SOC_CSTAT0)
	u32 sd_i;
#endif

	FIMC_BUG(!interface);
	FIMC_BUG(!pdev);
	FIMC_BUG(!device);

	device->interface	= interface;
	device->pdev		= pdev;
	device->pdata		= pdev->dev.platform_data;
	device->instance	= instance;
	device->groupmgr	= groupmgr;
	device->resourcemgr	= resourcemgr;
	device->minfo		= &resourcemgr->minfo;
	device->is_region	= (struct is_region *)(resourcemgr->minfo.kvaddr_region
					+ (device->instance * PARAM_REGION_SIZE));
	device->devicemgr	= devicemgr;
	device->sensor		= NULL;
	device->setfile		= 0;
	device->apply_setfile_fcount = 0;

	atomic_set(&device->group_open_cnt, 0);
	atomic_set(&device->open_cnt, 0);
	atomic_set(&device->init_cnt, 0);

	is_group_probe(groupmgr, &device->group_paf, NULL, device,
		is_ischain_group_shot,
		GROUP_SLOT_PAF, ENTRY_PAF, "PXS", &is_subdev_paf_ops);
	is_group_probe(groupmgr, &device->group_3aa, NULL, device,
		is_ischain_group_shot,
		GROUP_SLOT_3AA, ENTRY_3AA, "3XS", &is_subdev_3aa_ops);

	if (IS_ENABLED(SOC_LME0))
		is_group_probe(groupmgr, &device->group_lme, NULL, device,
			is_ischain_group_shot,
			GROUP_SLOT_LME, ENTRY_LME, "LME", &is_subdev_lme_ops);

	if (IS_ENABLED(SOC_ORBMCH))
		is_group_probe(groupmgr, &device->group_orb, NULL, device,
			is_ischain_group_shot,
			GROUP_SLOT_ORB, ENTRY_ORB, "ORB", &is_subdev_orb_ops);

	if (IS_ENABLED(SOC_I0S))
		is_group_probe(groupmgr, &device->group_isp, NULL, device,
			is_ischain_group_shot,
			GROUP_SLOT_ISP, ENTRY_ISP, "IXS", &is_subdev_isp_ops);

	if (IS_ENABLED(SOC_BYRP))
		is_group_probe(groupmgr, &device->group_byrp, NULL, device,
			is_ischain_group_shot,
			GROUP_SLOT_BYRP, ENTRY_BYRP, "BYRP", &is_subdev_byrp_ops);

#if IS_ENABLED(SOC_RGBP)
	is_group_probe(groupmgr, &device->group_rgbp, NULL, device,
		is_ischain_group_shot,
		GROUP_SLOT_RGBP, ENTRY_RGBP, "RGBP", &is_subdev_rgbp_ops);
#endif

	is_group_probe(groupmgr, &device->group_mcs, NULL, device,
		is_ischain_group_shot,
		GROUP_SLOT_MCS, ENTRY_MCS, "MXS", &is_subdev_mcs_ops);

	if (IS_ENABLED(SOC_VRA)) {
		is_group_probe(groupmgr, &device->group_vra, NULL, device,
			is_ischain_group_shot,
			GROUP_SLOT_VRA, ENTRY_VRA, "VXS", &is_subdev_vra_ops);
	}

	if (IS_ENABLED(SOC_YPP))
		is_group_probe(groupmgr, &device->group_ypp, NULL, device,
				is_ischain_group_shot,
				GROUP_SLOT_YPP, ENTRY_YPP, "YPP", &is_subdev_ypp_ops);

	if (IS_ENABLED(SOC_MCFP))
		is_group_probe(groupmgr, &device->group_mcfp, NULL, device,
				is_ischain_group_shot,
				GROUP_SLOT_MCFP, ENTRY_MCFP, "MCFP",
				&is_subdev_mcfp_ops);

	if (IS_ENABLED(SOC_YUVP))
		is_group_probe(groupmgr, &device->group_yuvp, NULL, device,
				is_ischain_group_shot,
				GROUP_SLOT_YUVP, ENTRY_YUVP, "YUVP",
				&is_subdev_yuvp_ops);

	is_subdev_probe(&device->pdaf, instance, ENTRY_PDAF, "PDAF", NULL);
	is_subdev_probe(&device->pdst, instance, ENTRY_PDST, "PDST", NULL);

#if IS_ENABLED(SOC_30S)
	is_subdev_probe(&device->txc, instance, ENTRY_3AC, "3XC", &is_subdev_3ac_ops);
	is_subdev_probe(&device->txp, instance, ENTRY_3AP, "3XP", &is_subdev_3ap_ops);
	is_subdev_probe(&device->txf, instance, ENTRY_3AF, "3XF", &is_subdev_3af_ops);
	is_subdev_probe(&device->txg, instance, ENTRY_3AG, "3XG", &is_subdev_3ag_ops);
	is_subdev_probe(&device->txo, instance, ENTRY_3AO, "3XO", &is_subdev_3ao_ops);
	is_subdev_probe(&device->txl, instance, ENTRY_3AL, "3XL", &is_subdev_3al_ops);
#endif

#if IS_ENABLED(SOC_CSTAT0)
	for (sd_i = 0; sd_i < IS_CSTAT_SUBDEV_NUM; sd_i++)
		is_subdev_probe(&device->subdev_cstat[sd_i], instance,
				ENTRY_CSTAT_CDAF + sd_i,
				is_cstat_subdev_name[sd_i], NULL);
#endif

	is_subdev_probe(&device->lmes, instance, ENTRY_LMES, "LMES", &is_subdev_lmes_ops);
	is_subdev_probe(&device->lmec, instance, ENTRY_LMEC, "LMEC", &is_subdev_lmec_ops);

	if (IS_ENABLED(SOC_ORB0C))
		is_subdev_probe(&device->orbxc, instance, ENTRY_ORBXC, "ORBXC",
				&is_subdev_orbxc_ops);

	if (IS_ENABLED(SOC_I0S)) {
		is_subdev_probe(&device->ixc, instance, ENTRY_IXC, "IXC", &is_subdev_ixc_ops);
		is_subdev_probe(&device->ixp, instance, ENTRY_IXP, "IXP", &is_subdev_ixp_ops);
	}

	if (IS_ENABLED(SOC_TNR_MERGER)) {
		is_subdev_probe(&device->ixt, instance, ENTRY_IXT,
				"IXT", &is_subdev_ixt_ops);
		is_subdev_probe(&device->ixg, instance, ENTRY_IXG,
				"IXG", &is_subdev_ixg_ops);
		is_subdev_probe(&device->ixv, instance, ENTRY_IXV,
				"IXV", &is_subdev_ixv_ops);
		is_subdev_probe(&device->ixw, instance, ENTRY_IXW,
				"IXW", &is_subdev_ixw_ops);
	}

	if (IS_ENABLED(SOC_MCFP)) {
		is_subdev_probe(&device->subdev_mcfp[0], instance, ENTRY_MCFP_VIDEO,
			"MCFP_VIDEO", NULL);
		is_subdev_probe(&device->subdev_mcfp[1], instance, ENTRY_MCFP_STILL,
			"MCFP_STILL", NULL);
	}
	if (IS_ENABLED(SOC_LME0))
		is_subdev_probe(&device->subdev_lme[IS_LME_SUBDEV_MBMV], instance, ENTRY_LME_MBMV,
			"LME_MBMV", NULL);

	is_subdev_probe(&device->mexc, instance, ENTRY_MEXC, "MEX", &is_subdev_mexc_ops);

	is_subdev_probe(&device->m0p, instance, ENTRY_M0P, "M0P", &is_subdev_mcsp_ops);
	is_subdev_probe(&device->m1p, instance, ENTRY_M1P, "M1P", &is_subdev_mcsp_ops);
	is_subdev_probe(&device->m2p, instance, ENTRY_M2P, "M2P", &is_subdev_mcsp_ops);
	is_subdev_probe(&device->m3p, instance, ENTRY_M3P, "M3P", &is_subdev_mcsp_ops);
	is_subdev_probe(&device->m4p, instance, ENTRY_M4P, "M4P", &is_subdev_mcsp_ops);
	is_subdev_probe(&device->m5p, instance, ENTRY_M5P, "M5P", &is_subdev_mcsp_ops);

	clear_bit(IS_ISCHAIN_OPEN, &device->state);
	clear_bit(IS_ISCHAIN_LOADED, &device->state);
	clear_bit(IS_ISCHAIN_POWER_ON, &device->state);
	clear_bit(IS_ISCHAIN_OPEN_STREAM, &device->state);
	clear_bit(IS_ISCHAIN_REPROCESSING, &device->state);
	clear_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state);

	/* clear group open state */
	clear_bit(IS_GROUP_OPEN, &device->group_3aa.state);
	clear_bit(IS_GROUP_OPEN, &device->group_lme.state);
	clear_bit(IS_GROUP_OPEN, &device->group_orb.state);
	clear_bit(IS_GROUP_OPEN, &device->group_isp.state);
	clear_bit(IS_GROUP_OPEN, &device->group_byrp.state);
	clear_bit(IS_GROUP_OPEN, &device->group_rgbp.state);
	clear_bit(IS_GROUP_OPEN, &device->group_mcs.state);
	clear_bit(IS_GROUP_OPEN, &device->group_vra.state);

	/* clear subdevice state */
	clear_bit(IS_SUBDEV_OPEN, &device->group_3aa.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_lme.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_orb.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_isp.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_byrp.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_rgbp.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_mcs.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->group_vra.leader.state);

	clear_bit(IS_SUBDEV_START, &device->group_3aa.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_lme.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_orb.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_isp.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_byrp.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_rgbp.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_mcs.leader.state);
	clear_bit(IS_SUBDEV_START, &device->group_vra.leader.state);

	return ret;
}

static int is_ischain_open(struct is_device_ischain *device)
{
	int ret = 0;
#if IS_ENABLED(SOC_VRA)
	bool has_vra_ch1_only = false;
#endif

	clear_bit(IS_ISCHAIN_INITING, &device->state);
	clear_bit(IS_ISCHAIN_INIT, &device->state);
	clear_bit(IS_ISCHAIN_START, &device->state);
	clear_bit(IS_ISCHAIN_OPEN_STREAM, &device->state);
	clear_bit(IS_ISCHAIN_REPROCESSING, &device->state);
	clear_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state);

	/* 2. Init variables */
#if !defined(FAST_FDAE)
	memset(&device->fdUd, 0, sizeof(struct camera2_fd_uctl));
#endif
#ifdef ENABLE_SENSOR_DRIVER
	memset(&device->peri_ctls, 0, sizeof(struct camera2_uctl)*SENSOR_MAX_CTL);
#endif

	/* initial state, it's real apply to setting when opening */
	atomic_set(&device->init_cnt, 0);
	device->sensor = NULL;
	device->apply_setfile_fcount = 0;
	device->llc_mode = false;
	device->yuv_max_width = 0;
	device->yuv_max_height = 0;

	spin_lock_init(&device->is_region->fd_info_slock);

#if IS_ENABLED(SOC_VRA)
	ret = is_hw_g_ctrl(NULL, 0, HW_G_CTRL_HAS_VRA_CH1_ONLY,
			(void *)&has_vra_ch1_only);
	if (!has_vra_ch1_only)
		is_subdev_open(&device->group_vra.leader, NULL,
				(void *)&init_vra_param.control,
				device->instance);
#endif

	device->dbuf_q = is_init_dbuf_q();
	if (IS_ERR_OR_NULL(device->dbuf_q)) {
		merr("is_resource_get is fail", device);
		goto p_err;
	}

	ret = is_devicemgr_open(device->devicemgr, (void *)device, IS_DEVICE_ISCHAIN);
	if (ret) {
		err("is_devicemgr_open is fail(%d)", ret);
		goto p_err;
	}

	/* for mediaserver force close */
	ret = is_resource_get(device->resourcemgr, RESOURCE_TYPE_ISCHAIN);
	if (ret) {
		merr("is_resource_get is fail", device);
		goto p_err;
	}

p_err:
	clear_bit(IS_ISCHAIN_OPENING, &device->state);
	set_bit(IS_ISCHAIN_OPEN, &device->state);

	minfo("[ISC:D] %s():%d\n", device, __func__, ret);
	return ret;
}

int is_ischain_open_wrap(struct is_device_ischain *device, bool EOS)
{
	int ret = 0;
	struct is_core *core;
	struct is_device_ischain *ischain;
	u32 stream;

	FIMC_BUG(!device);

	if (test_bit(IS_ISCHAIN_CLOSING, &device->state)) {
		merr("open is invalid on closing", device);
		ret = -EPERM;
		goto p_err;
	}

	if (test_bit(IS_ISCHAIN_OPEN, &device->state)) {
		merr("already open", device);
		ret = -EMFILE;
		goto p_err;
	}

	if (atomic_read(&device->open_cnt) > ENTRY_END) {
		merr("open count is invalid(%d)", device, atomic_read(&device->open_cnt));
		ret = -EMFILE;
		goto p_err;
	}

	if (EOS) {
		ret = is_ischain_open(device);
		if (ret) {
			merr("is_chain_open is fail(%d)", device, ret);
			goto p_err;
		}

		/* Check other ischain device for multi_channel */
		core = (struct is_core *)platform_get_drvdata(device->pdev);
		for (stream = 0; stream < IS_STREAM_COUNT; ++stream) {
			ischain = &core->ischain[stream];

			if (!test_bit(IS_ISCHAIN_MULTI_CH, &ischain->state) ||
					!test_bit(ischain->instance, &device->ginstance_map))
				continue;

			set_bit(IS_ISCHAIN_OPEN, &ischain->state);
		}
	} else {
		atomic_inc(&device->open_cnt);
		set_bit(IS_ISCHAIN_OPENING, &device->state);
	}

p_err:
	return ret;
}

static int is_ischain_close(struct is_device_ischain *device)
{
	int ret = 0;
	bool has_vra_ch1_only = false;

	FIMC_BUG(!device);

	if (!test_bit(IS_ISCHAIN_OPEN, &device->state)) {
		mwarn("this chain has not been opened", device);
		goto p_err;
	}

	/* subdev close */
	if (IS_ENABLED(SOC_VRA)) {
		is_hw_g_ctrl(NULL, 0, HW_G_CTRL_HAS_VRA_CH1_ONLY,
				(void *)&has_vra_ch1_only);
		if(!has_vra_ch1_only)
			is_subdev_close(&device->group_vra.leader);
	}

	ret = is_itf_close(device);
	if (ret)
		merr("is_itf_close is fail", device);

	is_deinit_dbuf_q(device->dbuf_q);

	/* for mediaserver force close */
	ret = is_resource_put(device->resourcemgr, RESOURCE_TYPE_ISCHAIN);
	if (ret)
		merr("is_resource_put is fail", device);

	ret = is_devicemgr_close(device->devicemgr, (void *)device, IS_DEVICE_ISCHAIN);
	if (ret) {
		err("is_devicemgr_close is fail(%d)", ret);
		goto p_err;
	}

	atomic_set(&device->open_cnt, 0);
	clear_bit(IS_ISCHAIN_OPEN_STREAM, &device->state);

	minfo("[ISC:D] %s():%d\n", device, __func__, ret);

p_err:
	return ret;
}

int is_ischain_close_wrap(struct is_device_ischain *device)
{
	int ret = 0;
	struct is_core *core;
	struct is_device_ischain *ischain;
	u32 stream;

	FIMC_BUG(!device);

	if (test_bit(IS_ISCHAIN_OPENING, &device->state)) {
		mwarn("close on opening", device);
		clear_bit(IS_ISCHAIN_OPENING, &device->state);
	}

	if (!atomic_read(&device->open_cnt)) {
		merr("open count is invalid(%d)", device, atomic_read(&device->open_cnt));
		ret = -ENOENT;
		goto p_err;
	}

	atomic_dec(&device->open_cnt);
	set_bit(IS_ISCHAIN_CLOSING, &device->state);

	if (!atomic_read(&device->open_cnt)) {
		ret = is_ischain_close(device);
		if (ret) {
			merr("is_chain_close is fail(%d)", device, ret);
			goto p_err;
		}

		clear_bit(IS_ISCHAIN_CLOSING, &device->state);
		clear_bit(IS_ISCHAIN_OPEN, &device->state);
		clear_bit(device->instance, &device->ginstance_map);

		/* Close another ischain device for multi_channel. */
		core = (struct is_core *)platform_get_drvdata(device->pdev);

		for (stream = 0; stream < IS_STREAM_COUNT; ++stream) {
			ischain = &core->ischain[stream];

			if (!test_bit(IS_ISCHAIN_MULTI_CH, &ischain->state) ||
					!test_bit(ischain->instance, &device->ginstance_map))
				continue;

			clear_bit(IS_ISCHAIN_MULTI_CH, &ischain->state);
			clear_bit(IS_ISCHAIN_OPEN, &ischain->state);
			clear_bit(ischain->instance, &device->ginstance_map);
		}
	}

p_err:
	return ret;
}

static int is_ischain_init(struct is_device_ischain *device)
{
	int ret = 0;
	struct is_core *core;
	struct is_module_enum *module;
	struct is_device_sensor *sensor;
	struct is_vender *vender;
	u32 flag;

	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);

	mdbgd_ischain("%s\n", device, __func__);

	sensor = device->sensor;
	core = (struct is_core *)platform_get_drvdata(device->pdev);
	vender = &core->vender;

	if (test_bit(IS_ISCHAIN_INIT, &device->state)) {
		minfo("stream is already initialized", device);
		goto p_err;
	}

	if (test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state)) {
		module = device->module_enum;
		goto skip_get_module;
	}

	if (!test_bit(IS_SENSOR_S_INPUT, &sensor->state)) {
		merr("I2C gpio is not yet set", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_g_module(sensor, &module);
	if (ret) {
		merr("is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

skip_get_module:
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		ret = is_vender_cal_load(vender, module);
		if (ret) {
			merr("is_vender_cal_load is fail(%d)", device, ret);
			goto p_err;
		}
	}

	ret = is_vender_setfile_sel(vender, module->setfile_name, module->position);
	if (ret) {
		merr("is_vender_setfile_sel is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_itf_enum(device);
	if (ret) {
		merr("is_itf_enum is fail(%d)", device, ret);
		goto p_err;
	}

	flag = test_bit(IS_ISCHAIN_REPROCESSING, &device->state) ? 1 : 0;

	flag |= (sensor->pdata->scenario == SENSOR_SCENARIO_STANDBY) ? (0x1 << 16) : (0x0 << 16);

	ret = is_itf_open(device, flag);
	if (ret) {
		merr("open fail", device);
		goto p_err;
	}

	ret = is_itf_setfile(device, vender);
	if (ret) {
		merr("setfile fail", device);
		goto p_err;
	}

	clear_bit(IS_ISCHAIN_INITING, &device->state);
	set_bit(IS_ISCHAIN_INIT, &device->state);

p_err:
	return ret;
}

static int is_ischain_init_wrap(struct is_device_ischain *device,
	u32 stream_type,
	u32 position)
{
	int ret = 0;
	u32 sindex;
	struct is_core *core;
	struct is_device_sensor *sensor;
	struct is_module_enum *module;
	struct is_groupmgr *groupmgr;

	FIMC_BUG(!device);
	FIMC_BUG(!device->groupmgr);

	if (!test_bit(IS_ISCHAIN_OPEN, &device->state)) {
		merr("NOT yet open", device);
		ret = -EMFILE;
		goto p_err;
	}

	if (test_bit(IS_ISCHAIN_START, &device->state)) {
		merr("already start", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (atomic_read(&device->init_cnt) >= atomic_read(&device->group_open_cnt)) {
		merr("init count value(%d) is invalid", device, atomic_read(&device->init_cnt));
		ret = -EINVAL;
		goto p_err;
	}

	groupmgr = device->groupmgr;
	core = container_of(groupmgr, struct is_core, groupmgr);
	atomic_inc(&device->init_cnt);
	set_bit(IS_ISCHAIN_INITING, &device->state);
	mdbgd_ischain("%s(%d, %d)\n", device, __func__,
		atomic_read(&device->init_cnt), atomic_read(&device->group_open_cnt));

	if (atomic_read(&device->init_cnt) == atomic_read(&device->group_open_cnt)) {
		if (test_bit(IS_GROUP_OPEN, &device->group_3aa.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_3aa.state)) {
			merr("invalid 3aa group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OPEN, &device->group_lme.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_lme.state)) {
			merr("invalid lme group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OPEN, &device->group_orb.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_orb.state)) {
			merr("invalid orb group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OPEN, &device->group_isp.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_isp.state)) {
			merr("invalid isp group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OPEN, &device->group_byrp.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_byrp.state)) {
			merr("invalid byrp group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OPEN, &device->group_rgbp.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_rgbp.state)) {
			merr("invalid rgbp group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OPEN, &device->group_vra.state) &&
			!test_bit(IS_GROUP_INIT, &device->group_vra.state)) {
			merr("invalid vra group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (stream_type == IS_OFFLINE_REPROCESSING_STREAM)
			goto skip_map_sensor;

		for (sindex = 0; sindex < IS_SENSOR_COUNT; ++sindex) {
			sensor = &core->sensor[sindex];

			if (!test_bit(IS_SENSOR_OPEN, &sensor->state))
				continue;

			if (!test_bit(IS_SENSOR_S_INPUT, &sensor->state))
				continue;

			ret = is_sensor_g_module(sensor, &module);
			if (ret) {
				merr("is_sensor_g_module is fail(%d)", device, ret);
				goto p_err;
			}

			ret = is_search_sensor_module_with_position(sensor, position, &module);
			if (!ret) {
				u32 ginstance;

				for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
					if (!test_bit(ginstance, &device->ginstance_map))
						continue;
					core->ischain[ginstance].sensor = sensor;
				}

				minfo("%s: pos(%d)", device, __func__, position);
				break;
			}
		}

		if (ret) {
			merr("position(%d) is not supported.", device, position);
			goto p_err;
		}

		if (sindex >= IS_SENSOR_COUNT) {
			merr("position(%d) is invalid", device, position);
			ret = -EINVAL;
			goto p_err;
		}

		if (!sensor || !sensor->pdata) {
			merr("sensor is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

skip_map_sensor:
		if (stream_type) {
			set_bit(IS_ISCHAIN_REPROCESSING, &device->state);
			if (stream_type == IS_OFFLINE_REPROCESSING_STREAM) {
				ret = is_sensor_map_sensor_module(device, position);
				if (ret) {
					merr("fail to map(%d)", device, ret);
					ret = -EINVAL;
					goto p_err;
				}
				sensor = device->sensor;
				set_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state);
			}
		} else {
			if (sensor->instance != device->instance) {
				merr("instance is mismatched (!= %d)[SS%d]", device, sensor->instance,
					sensor->device_id);
				ret = -EINVAL;
				goto p_err;
			}

			clear_bit(IS_ISCHAIN_REPROCESSING, &device->state);
			clear_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state);
			sensor->ischain = device;
		}

		ret = is_groupmgr_init(device->groupmgr, device);
		if (ret) {
			merr("is_groupmgr_init is fail(%d)", device, ret);
			goto p_err;
		}

		/* register sensor(DDK, RTA)/preporcessor interface*/
		if (!test_bit(IS_SENSOR_ITF_REGISTER, &sensor->state) &&
			!test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state)) {
			ret = is_sensor_register_itf(sensor);
			if (ret) {
				merr("is_sensor_register_itf fail(%d)", device, ret);
				goto p_err;
			}

			set_bit(IS_SENSOR_ITF_REGISTER, &sensor->state);
		}

		ret = is_ischain_init(device);
		if (ret) {
			merr("is_ischain_init is fail(%d)", device, ret);
			goto p_err;
		}

		atomic_set(&device->init_cnt, 0);

		ret = is_devicemgr_binding(device->devicemgr, sensor, device, IS_DEVICE_ISCHAIN);
		if (ret) {
			merr("is_devicemgr_binding is fail", device);
			goto p_err;
		}
	}

p_err:
	return ret;
}

static void is_fastctl_manager_init(struct is_device_ischain *device)
{
	int i;
	unsigned long flags;
	struct fast_control_mgr *fastctlmgr;
	struct is_fast_ctl *fast_ctl;

	fastctlmgr = &device->fastctlmgr;

	spin_lock_init(&fastctlmgr->slock);
	fastctlmgr->fast_capture_count = 0;

	spin_lock_irqsave(&fastctlmgr->slock, flags);

	for (i = 0; i < IS_FAST_CTL_STATE; i++) {
		fastctlmgr->queued_count[i] = 0;
		INIT_LIST_HEAD(&fastctlmgr->queued_list[i]);
	}

	for (i = 0; i < MAX_NUM_FAST_CTL; i++) {
		fast_ctl = &fastctlmgr->fast_ctl[i];
		fast_ctl->state = IS_FAST_CTL_FREE;
		list_add_tail(&fast_ctl->list, &fastctlmgr->queued_list[IS_FAST_CTL_FREE]);
		fastctlmgr->queued_count[IS_FAST_CTL_FREE]++;
	}

	spin_unlock_irqrestore(&fastctlmgr->slock, flags);
}

static int is_ischain_start(struct is_device_ischain *device)
{
	int ret = 0;
	IS_DECLARE_PMAP(pmap);

	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);

	IS_INIT_PMAP(pmap);

	memset(&device->prfi, 0x0, sizeof(struct pablo_rta_frame_info));
	device->prfi.magic = 0x5F4D5246; /* FRM_ */

	ret = is_hw_ischain_cfg((void *)device);
	if (ret) {
		merr("hw init fail", device);
		goto p_err;
	}

	if (!test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state)) {
		ret = is_ischain_s_sensor_size(device, NULL, pmap);
		if (ret) {
			merr("is_ischain_s_sensor_size is fail(%d)", device, ret);
			goto p_err;
		}
	}

#if !defined(FAST_FDAE)
	/* previous fd infomation should be clear */
	memset(&device->fdUd, 0x0, sizeof(struct camera2_fd_uctl));
#endif

	is_fastctl_manager_init(device);

	/* NI information clear */
	memset(&device->cur_noise_idx, 0xFFFFFFFF, sizeof(device->cur_noise_idx));
	memset(&device->next_noise_idx, 0xFFFFFFFF, sizeof(device->next_noise_idx));

	ret = is_itf_sensor_mode(device);
	if (ret) {
		merr("is_itf_sensor_mode is fail(%d)", device, ret);
		goto p_err;
	}

#if IS_ENABLED(SOC_30S)
	if (device->sensor->scene_mode >= AA_SCENE_MODE_DISABLED)
		device->is_region->parameter.taa.vdma1_input.scene_mode = device->sensor->scene_mode;
#endif

	ret = is_itf_s_param(device, NULL, pmap);
	if (ret) {
		merr("is_itf_s_param is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_itf_f_param(device);
	if (ret) {
		merr("is_itf_f_param is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_itf_init_process_start(device);
	if (ret) {
		merr("is_itf_init_process_start is fail(%d)", device, ret);
		goto p_err;
	}

	set_bit(IS_ISCHAIN_START, &device->state);

p_err:
	minfo("[ISC:D] %s(%d):%d\n", device, __func__,
		device->setfile & IS_SETFILE_MASK, ret);

	return ret;
}

int is_ischain_start_wrap(struct is_device_ischain *device,
	struct is_group *group)
{
	int ret = 0;
	struct is_group *leader;

	if (!test_bit(IS_ISCHAIN_INIT, &device->state)) {
		merr("device is not yet init", device);
		ret = -EINVAL;
		goto p_err;
	}

	leader = device->groupmgr->leader[device->instance];
	if (leader != group)
		goto p_err;

	if (test_bit(IS_ISCHAIN_START, &device->state)) {
		merr("already start", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_groupmgr_start(device->groupmgr, device);
	if (ret) {
		merr("is_groupmgr_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_start(device);
	if (ret) {
		merr("is_chain_start is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_stop(struct is_device_ischain *device)
{
	int ret = 0;

	return ret;
}

int is_ischain_stop_wrap(struct is_device_ischain *device,
	struct is_group *group)
{
	int ret = 0;
	struct is_group *leader;

	leader = device->groupmgr->leader[device->instance];
	if (leader != group)
		goto p_err;

	if (!test_bit(IS_ISCHAIN_START, &device->state)) {
		mwarn("already stop", device);
		goto p_err;
	}

	ret = is_groupmgr_stop(device->groupmgr, device);
	if (ret) {
		merr("is_groupmgr_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_stop(device);
	if (ret) {
		merr("is_ischain_stop is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(IS_ISCHAIN_START, &device->state);

p_err:
	return ret;
}

static int change_vctx_group(struct is_device_ischain *device,
			struct is_video_ctx *ivc)
{
	struct is_device_ischain *ischain;
	struct is_core *core
		= (struct is_core *)platform_get_drvdata(device->pdev);
	u32 stream;

	for (stream = 0; stream < IS_STREAM_COUNT; ++stream) {
		ischain = &core->ischain[stream];

		if (ischain->instance == device->instance)
			continue;

		if (!test_bit(IS_ISCHAIN_OPEN, &ischain->state))
			break;
	}

	if (stream == IS_STREAM_COUNT) {
		merr("could not find ischain device", device);
		return -ENODEV;
	}

	ivc->group = (struct is_group *)((char *)ischain + ivc->video->group_ofs);
	set_bit(IS_GROUP_USE_MULTI_CH, &ivc->group->state);
	set_bit(IS_ISCHAIN_MULTI_CH, &ischain->state);
	minfo("use ischain(%d) group for V%02d",
			device, ischain->instance, ivc->video->id);

	return 0;
}

int is_ischain_group_open(struct is_device_ischain *device,
			struct is_video_ctx *vctx, u32 group_id)
{
	int ret = 0;
	int ret_err = 0;
	struct is_groupmgr *groupmgr;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	groupmgr = device->groupmgr;

retry_group_open:
	ret = is_group_open(groupmgr,
		vctx->group,
		group_id,
		vctx);
	if (ret) {
		if (ret == -EAGAIN) {
			ret = change_vctx_group(device, vctx);
			if (ret) {
				merr("change_vctx_group is fail(%d)", device, ret);
				goto err_group_open;
			}
			goto retry_group_open;
		} else {
			merr("is_group_open is fail(%d)", device, ret);
			goto err_group_open;
		}
	}

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	set_bit(vctx->group->instance, &device->ginstance_map);
	atomic_inc(&device->group_open_cnt);

	return 0;

err_ischain_open:
	ret_err = is_group_close(groupmgr, vctx->group);
	if (ret_err)
		merr("is_group_close is fail(%d)", device, ret_err);
err_group_open:
	return ret;
}

static int is_ischain_group_start(struct is_device_ischain *idi,
					struct is_queue *iq, struct is_group *ig)
{
	struct is_groupmgr *groupmgr = idi->groupmgr;
	int ret;

	ret = is_group_start(groupmgr, ig);
	if (ret) {
		merr("is_group_start is fail(%d)", idi, ret);
		return ret;
	}

	ret = is_ischain_start_wrap(idi, ig);
	if (ret) {
		merr("is_ischain_start_wrap is fail(%d)", idi, ret);
		return ret;
	}

	return 0;
}

static int is_ischain_group_stop(struct is_device_ischain *idi,
					struct is_queue *iq, struct is_group *ig)
{
	int ret = 0;
	struct is_groupmgr *groupmgr = idi->groupmgr;

	if (!test_bit(IS_GROUP_INIT, &ig->state)) {
		mgwarn("group is not initialized yet", ig, ig);
		goto err_group_init;
	}

	ret = is_group_stop(groupmgr, ig);
	if (ret) {
		if (ret == -EPERM)
			ret = 0;
		else
			merr("is_group_stop is fail(%d)", idi, ret);
		goto err_group_stop;
	}

	ret = is_ischain_stop_wrap(idi, ig);
	if (ret) {
		merr("is_ischain_stop_wrap is fail(%d)", idi, ret);
		goto err_ischain_stop;
	}

err_ischain_stop:
err_group_stop:
err_group_init:
	mginfo("%s, group->scount: %d (%d)\n", idi, ig,  __func__,
					atomic_read(&ig->scount), ret);

	return ret;
}

static int is_ischain_group_s_fmt(struct is_device_ischain *idi,
					struct is_queue *iq, struct is_group *ig)
{
	struct is_subdev *leader = &ig->leader;

	leader->input.width = iq->framecfg.width;
	leader->input.height = iq->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return 0;
}

int is_ischain_group_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx, struct is_group *group)
{
	int ret;
	struct is_groupmgr *groupmgr;
	struct is_queue *queue;
	struct is_sysfs_debug *sysfs_debug = is_get_sysfs_debug();

	groupmgr = device->groupmgr;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug->hal_debug_mode)) {
			msleep(sysfs_debug->hal_debug_delay);
			panic("HAL sudden group close #1");
		}
	}

	if (group->head && test_bit(IS_GROUP_START, &group->head->state)) {
		mgwarn("sudden group close", device, group);
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
			is_itf_sudden_stop_wrap(device, device->instance, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug->hal_debug_mode)) {
			msleep(sysfs_debug->hal_debug_delay);
			panic("HAL sudden group close #2");
		}
	}

	ret = is_ischain_group_stop(device, queue, group);
	if (ret)
		merr("is_ischain_group_stop is fail", device);

	ret = is_ischain_close_wrap(device);
	if (ret)
		merr("is_ischain_close_wrap is fail(%d)", device, ret);

	ret = is_group_close(groupmgr, group);
	if (ret)
		merr("is_group_close is fail", device);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int is_ischain_group_s_input(struct is_device_ischain *device,
	struct is_group *group,
	u32 stream_type,
	u32 position,
	u32 video_id,
	u32 input_type,
	u32 stream_leader)
{
	int ret;
	struct is_groupmgr *groupmgr;

	groupmgr = device->groupmgr;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = is_group_init(groupmgr, group, input_type, video_id, stream_leader);
	if (ret) {
		merr("is_group_init is fail(%d)", device, ret);
		goto err_initialize_group;
	}

	ret = is_ischain_init_wrap(device, stream_type, position);
	if (ret) {
		merr("is_ischain_init_wrap is fail(%d)", device, ret);
		goto err_initialize_ischain;
	}

err_initialize_ischain:
err_initialize_group:
	return ret;
}

static int is_ischain_device_start(void *device, struct is_queue *iq)
{
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);

	return is_ischain_group_start((struct is_device_ischain *)device, iq, ivc->group);
}

static int is_ischain_device_stop(void *device, struct is_queue *iq)
{
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);

	return is_ischain_group_stop((struct is_device_ischain *)device, iq, ivc->group);
}

static int is_ischain_device_s_fmt(void *device, struct is_queue *iq)
{
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);

	return is_ischain_group_s_fmt((struct is_device_ischain *)device, iq, ivc->group);
}

static struct is_queue_ops is_ischain_device_qops = {
	.start_streaming	= is_ischain_device_start,
	.stop_streaming		= is_ischain_device_stop,
	.s_fmt			= is_ischain_device_s_fmt,
};

struct is_queue_ops *is_get_ischain_device_qops(void)
{
	return &is_ischain_device_qops;
}

static int is_ischain_paf_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	struct is_group *group;

	group = &device->group_paf;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_paf_tag is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_3aa_group_tag(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	u32 capture_id;
	struct is_subdev *subdev;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	node_group = &frame->shot_ext->node_group;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_3aa_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;
		switch (cap_node->vid) {
		case 0:
			break;
		case IS_VIDEO_30C_NUM:
		case IS_VIDEO_31C_NUM:
		case IS_VIDEO_32C_NUM:
			subdev = group->subdev[ENTRY_3AC];
			break;
		case IS_VIDEO_30P_NUM:
		case IS_VIDEO_31P_NUM:
		case IS_VIDEO_32P_NUM:
			subdev = group->subdev[ENTRY_3AP];
			break;
		case IS_VIDEO_30F_NUM:
		case IS_VIDEO_31F_NUM:
		case IS_VIDEO_32F_NUM:
			subdev = group->subdev[ENTRY_3AF];
			break;
		case IS_VIDEO_30G_NUM:
		case IS_VIDEO_31G_NUM:
		case IS_VIDEO_32G_NUM:
			subdev = group->subdev[ENTRY_3AG];
			break;
		case IS_VIDEO_30O_NUM:
		case IS_VIDEO_31O_NUM:
		case IS_VIDEO_32O_NUM:
		case IS_VIDEO_33O_NUM:
			subdev = group->subdev[ENTRY_3AO];
			break;
		case IS_VIDEO_30L_NUM:
		case IS_VIDEO_31L_NUM:
		case IS_VIDEO_32L_NUM:
		case IS_VIDEO_33L_NUM:
			subdev = group->subdev[ENTRY_3AL];
			break;
		case IS_VIDEO_ME0C_NUM:
		case IS_VIDEO_ME1C_NUM:
			subdev = group->subdev[ENTRY_MEXC];
			break;
		case IS_VIDEO_ORB0C_NUM:
		case IS_VIDEO_ORB1C_NUM:
			subdev = group->subdev[ENTRY_ORBXC];
			break;
		default:
			break;
		}

		if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
			ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
			if (ret) {
				mserr("is_ischain_3aa_group_tag is fail(%d)", device, subdev, ret);
				goto p_err;
			}
		}
	}
p_err:
	return ret;
}

static int is_ischain_isp_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	u32 capture_id;
	struct is_group *group;
	struct is_subdev *subdev, *vra;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_isp;
	vra = group->subdev[ENTRY_VRA];
	node_group = &frame->shot_ext->node_group;

#ifdef ENABLE_VRA
	if (vra) {
		if (frame->shot_ext->fd_bypass) {
			if (test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = is_ischain_vra_bypass(device, frame, true);
				if (ret) {
					merr("fd_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = is_ischain_vra_bypass(device, frame, false);
				if (ret) {
					merr("fd_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_isp_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;

		switch (cap_node->vid) {
		case 0:
			break;
		case IS_VIDEO_I0C_NUM:
		case IS_VIDEO_I1C_NUM:
			subdev = group->subdev[ENTRY_IXC];
			break;
		case IS_VIDEO_I0P_NUM:
		case IS_VIDEO_I1P_NUM:
			subdev = group->subdev[ENTRY_IXP];
			break;
		case IS_VIDEO_I0T_NUM:
			subdev = group->subdev[ENTRY_IXT];
			break;
		case IS_VIDEO_I0G_NUM:
			subdev = group->subdev[ENTRY_IXG];
			break;
		case IS_VIDEO_I0V_NUM:
			subdev = group->subdev[ENTRY_IXV];
			break;
		case IS_VIDEO_I0W_NUM:
			subdev = group->subdev[ENTRY_IXW];
			break;
		case IS_VIDEO_ME0C_NUM:
		case IS_VIDEO_ME1C_NUM:
			subdev = group->subdev[ENTRY_MEXC];
			break;
		default:
			break;
		}

		if (subdev && test_bit(IS_SUBDEV_START, &subdev->state) &&
			!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
			ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
			if (ret) {
				mserr("is_ischain_isp_group_tag is fail(%d)", device, subdev, ret);
				goto p_err;
			}
		}
	}

p_err:
	return ret;
}

static int is_ischain_byrp_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	struct is_group *group;

	group = &device->group_byrp;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_byrp_tag is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_rgbp_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	struct is_group *group;
	struct camera2_node_group *node_group;

	group = &device->group_rgbp;
	node_group = &frame->shot_ext->node_group;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_rgbp_tag is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_ypp_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	struct is_group *group;

	group = &device->group_ypp;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_ypp_group_tag is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_mcfp_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	struct is_group *group;

	group = &device->group_mcfp;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_mcfp_group_tag is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_yuvp_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	struct is_group *group;

	group = &device->group_yuvp;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_yuvp_group_tag is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_mcs_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	u32 capture_id;
	struct is_group *group;
	struct is_group *head;
	struct is_subdev *subdev, *vra;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_mcs;
	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, group);
	vra = group->subdev[ENTRY_VRA];
	node_group = &frame->shot_ext->node_group;

#ifdef ENABLE_VRA
	if (vra) {
		if (frame->shot_ext->fd_bypass) {
			if (test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = CALL_SOPS(&group->leader, bypass, device, frame, true);
				if (ret) {
					merr("mcs_bypass(1) is fail(%d)", device, ret);
					goto p_err;
				}
				ret = is_ischain_vra_bypass(device, frame, true);
				if (ret) {
					merr("fd_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = CALL_SOPS(&group->leader, bypass, device, frame, false);
				if (ret) {
					merr("mcs_bypass(0) is fail(%d)", device, ret);
					goto p_err;
				}
				ret = is_ischain_vra_bypass(device, frame, false);
				if (ret) {
					merr("fd_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

	/* At Full-OTF case(Head group is OTF),
	 * set next_noise_idx for next frame applying.
	 *
	 * At DMA input case, set cur_noise_idx for current frame appling.
	 */
	if (test_bit(IS_GROUP_OTF_INPUT, &head->state)) {
		frame->noise_idx = device->next_noise_idx[frame->fcount % NI_BACKUP_MAX];
		/* clear back up NI value */
		device->next_noise_idx[frame->fcount % NI_BACKUP_MAX] = 0xFFFFFFFF;
	} else {
		frame->noise_idx = device->cur_noise_idx[frame->fcount % NI_BACKUP_MAX];
		/* clear back up NI value */
		device->cur_noise_idx[frame->fcount % NI_BACKUP_MAX] = 0xFFFFFFFF;
	}

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_mcs_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;

		switch (cap_node->vid) {
		case 0:
			break;
		case IS_VIDEO_M0P_NUM:
			subdev = group->subdev[ENTRY_M0P];
			break;
		case IS_VIDEO_M1P_NUM:
			subdev = group->subdev[ENTRY_M1P];
			break;
		case IS_VIDEO_M2P_NUM:
			subdev = group->subdev[ENTRY_M2P];
			break;
		case IS_VIDEO_M3P_NUM:
			subdev = group->subdev[ENTRY_M3P];
			break;
		case IS_VIDEO_M4P_NUM:
			subdev = group->subdev[ENTRY_M4P];
			break;
		case IS_VIDEO_M5P_NUM:
			subdev = group->subdev[ENTRY_M5P];
			break;
		default:
			break;
		}

		if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
			ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
			if (ret) {
				mserr("is_ischain_mcs_group_tag is fail(%d)", device, subdev, ret);
				goto p_err;
			}
		}
	}

p_err:
	return ret;
}

static int is_ischain_vra_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	struct is_group *group;
	struct is_subdev *vra;

	group = &device->group_vra;
	vra = &group->leader;

#ifdef ENABLE_VRA
	if (vra) {
		if (frame->shot_ext->fd_bypass) {
			if (test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = is_ischain_vra_bypass(device, frame, true);
				if (ret) {
					merr("fd_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(IS_SUBDEV_RUN, &vra->state)) {
				ret = is_ischain_vra_bypass(device, frame, false);
				if (ret) {
					merr("fd_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	 if (ret) {
			 merr("is_ischain_vra_tag fail(%d)", device, ret);
			 goto p_err;
	 }

p_err:
	return ret;
}

static int is_ischain_lme_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	u32 capture_id;
	struct is_group *group;
	struct is_subdev *subdev;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_lme;
	node_group = &frame->shot_ext->node_group;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_isp_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;
		switch (cap_node->vid) {
		case 0:
			break;
		case IS_VIDEO_LME0S_NUM:
		case IS_VIDEO_LME1S_NUM:
			subdev = group->subdev[ENTRY_LMES];
			break;
		case IS_VIDEO_LME0C_NUM:
		case IS_VIDEO_LME1C_NUM:
			subdev = group->subdev[ENTRY_LMEC];
			break;
		default:
			break;
		}

		if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
			ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
			if (ret) {
				mserr("is_ischain_lme_group_tag is fail(%d)", device, subdev, ret);
				goto p_err;
			}
		}
	}

p_err:
	return ret;
}

static int is_ischain_orb_group_tag(struct is_device_ischain *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	struct is_group *group;

	group = &device->group_orb;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_ischain_orb_tag is fail(%d)", device, ret);
		goto p_err;
	}
p_err:
	return ret;
}

static struct is_frame *is_ischain_check_frame(struct is_group *group,
						struct is_frame *check_frame)
{
	struct is_frame *frame;
	struct is_framemgr *framemgr;

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, check_frame->cur_shot_idx);
	if (!framemgr) {
		mgerr("framemgr is NULL", group, group);
		return ERR_PTR(-EINVAL);
	}

	if (check_frame->state != FS_REQUEST && check_frame->state != FS_STRIPE_PROCESS) {
		mgerr("check_frame state is invalid(%d)", group, group, check_frame->state);
		return ERR_PTR(-EINVAL);
	}

	frame = peek_frame(framemgr, check_frame->state);
	if (unlikely(!frame)) {
		mgerr("frame is NULL", group, group);
		return ERR_PTR(-EINVAL);
	}

	if (unlikely(frame != check_frame)) {
		mgerr("frame checking is fail(%p != %p)", group, group, frame, check_frame);
		return ERR_PTR(-EINVAL);
	}

	if (unlikely(!frame->shot)) {
		mgerr("frame->shot is NULL", group, group);
		return ERR_PTR(-EINVAL);
	}

	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;

	PROGRAM_COUNT(8);

	return frame;
}

static void is_ischain_update_setfile(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame)
{
	int ret;
	bool update_condition = false;
	u32 setfile_save = 0;

	/* setfile update will do at below conditions.
	 * 1. Preview & leader group shot.
	 * 2. Reprocessing & First group that attempt to shot.
	 */
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		if (group->id == get_ischain_leader_group(device)->id)
			update_condition = true;
	} else {
		if (frame->fcount != device->apply_setfile_fcount)
			update_condition = true;
	}

	if ((frame->shot_ext->setfile != device->setfile) && update_condition) {
		setfile_save = device->setfile;
		device->setfile = frame->shot_ext->setfile;
		device->apply_setfile_fcount = frame->fcount;

		mgrinfo(" setfile change at shot(%d -> %d)\n", device, group, frame,
			setfile_save, device->setfile & IS_SETFILE_MASK);

		if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
			ret = is_ischain_chg_setfile(device);
			if (ret) {
				merr("is_ischain_chg_setfile is fail", device);
				device->setfile = setfile_save;
			}
		}
	}

	PROGRAM_COUNT(9);
}

static int is_ischain_child_tag(struct is_device_ischain *device,
				struct is_group *group,
				struct is_frame *frame)
{
	int ret = 0;
	struct is_group *child, *vra, *pnext;
	struct camera2_node_group *node_group;
	struct camera2_node ldr_node = {0, };

	node_group = &frame->shot_ext->node_group;

	ldr_node.pixelformat = node_group->leader.pixelformat;
	ldr_node.pixelsize = node_group->leader.pixelsize;

	TRANS_CROP(ldr_node.input.cropRegion,
		node_group->leader.input.cropRegion);
	TRANS_CROP(ldr_node.output.cropRegion,
		ldr_node.input.cropRegion);

	child = group;
	pnext = group->head->pnext;
	while (child) {
		if (child->slot > GROUP_SLOT_MAX) {
			child = child->child;
			continue;
		}

		switch (child->slot) {
		case GROUP_SLOT_PAF:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);

			ret = is_ischain_paf_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_paf_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_3AA:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);

			if (child->junction && child->junction->cid < CAPTURE_NODE_MAX)
				TRANS_CROP(ldr_node.output.cropRegion,
					node_group->capture[child->junction->cid].output.cropRegion);

			ret = is_ischain_3aa_group_tag(device, child, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_3aa_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_LME:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);

			ret = is_ischain_lme_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_lme_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_ORB:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);

			ret = is_ischain_orb_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_orb_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_ISP:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);

			ret = is_ischain_isp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_isp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_YPP:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);

			ret = is_ischain_ypp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_ypp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_BYRP:
			TRANS_CROP(ldr_node.input.cropRegion,
					node_group->leader.input.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
					ldr_node.input.cropRegion);

			ldr_node.pixelformat = node_group->leader.pixelformat;
			ldr_node.pixelsize = node_group->leader.pixelsize;
			ret = is_ischain_byrp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_byrp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_RGBP:
			TRANS_CROP(ldr_node.input.cropRegion,
					node_group->leader.input.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
					ldr_node.input.cropRegion);

			ldr_node.pixelformat = node_group->leader.pixelformat;
			ldr_node.pixelsize = node_group->leader.pixelsize;
			ret = is_ischain_rgbp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_rgbp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_MCFP:
			TRANS_CROP(ldr_node.input.cropRegion,
				node_group->leader.input.cropRegion);

			ret = is_ischain_mcfp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_mcfp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_YUVP:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);
			ret = is_ischain_yuvp_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_yuvp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_MCS:
			TRANS_CROP(ldr_node.input.cropRegion,
				ldr_node.output.cropRegion);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);

			ret = is_ischain_mcs_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_mcs_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_VRA:
			vra = &device->group_vra;
			TRANS_CROP(ldr_node.input.cropRegion,
				(u32 *)&vra->prev->junction->output.crop);
			TRANS_CROP(ldr_node.output.cropRegion,
				ldr_node.input.cropRegion);

			ret = is_ischain_vra_group_tag(device, frame, &ldr_node);
			if (ret) {
				merr("is_ischain_vra_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		child = child->child;
		if (!child) {
			child = pnext;
			pnext = NULL;
		}
	}

p_err:
	PROGRAM_COUNT(10);

	return ret;
}

static void is_ischain_update_shot(struct is_device_ischain *device,
	struct is_frame *frame)
{
	struct fast_control_mgr *fastctlmgr = &device->fastctlmgr;
	struct is_fast_ctl *fast_ctl = NULL;
	struct camera2_shot *shot = frame->shot;
	unsigned long flags;
	u32 state;

#ifdef ENABLE_ULTRA_FAST_SHOT
	if (device->fastctlmgr.fast_capture_count) {
		device->fastctlmgr.fast_capture_count--;
		frame->shot->ctl.aa.captureIntent = AA_CAPTURE_INTENT_PREVIEW;
		mrinfo("captureIntent update\n", device, frame);
	}
#endif

	spin_lock_irqsave(&fastctlmgr->slock, flags);

	state = IS_FAST_CTL_REQUEST;
	if (fastctlmgr->queued_count[state]) {
		/* get req list */
		fast_ctl = list_first_entry(&fastctlmgr->queued_list[state],
			struct is_fast_ctl, list);
		list_del(&fast_ctl->list);
		fastctlmgr->queued_count[state]--;

		/* Read fast_ctl: lens */
		if (fast_ctl->lens_pos_flag) {
			shot->uctl.lensUd.pos = fast_ctl->lens_pos;
			shot->uctl.lensUd.posSize = 12; /* fixed value: bit size(12 bit) */
			shot->uctl.lensUd.direction = 0; /* fixed value: 0(infinite), 1(macro) */
			shot->uctl.lensUd.slewRate = 0; /* fixed value: only DW actuator speed */
			shot->ctl.aa.afMode = AA_AFMODE_OFF;
			fast_ctl->lens_pos_flag = false;
		}

		/* set free list */
		state = IS_FAST_CTL_FREE;
		fast_ctl->state = state;
		list_add_tail(&fast_ctl->list, &fastctlmgr->queued_list[state]);
		fastctlmgr->queued_count[state]++;
	}

	spin_unlock_irqrestore(&fastctlmgr->slock, flags);

	if (fast_ctl)
		mrinfo("fast_ctl: uctl.lensUd.pos(%d)\n", device, frame, shot->uctl.lensUd.pos);
}

static void is_ischain_update_otf_data(struct is_device_ischain *device,
					struct is_group *group,
					struct is_frame *frame)
{
	struct is_resourcemgr *resourcemgr = device->resourcemgr;
	struct is_device_sensor *sensor = device->sensor;
	enum aa_capture_intent captureIntent;
	unsigned long flags;

	spin_lock_irqsave(&group->slock_s_ctrl, flags);

	captureIntent = group->intent_ctl.captureIntent;
	if (captureIntent != AA_CAPTURE_INTENT_CUSTOM) {
#if defined(CONFIG_CAMERA_VENDER_MCD) || defined(CONFIG_CAMERA_VENDER_MCD_V2)
		if (group->remainIntentCount > 0) {
			frame->shot->ctl.aa.captureIntent = captureIntent;
			frame->shot->ctl.aa.vendor_captureCount = group->intent_ctl.vendor_captureCount;
			frame->shot->ctl.aa.vendor_captureExposureTime = group->intent_ctl.vendor_captureExposureTime;
			frame->shot->ctl.aa.vendor_captureEV = group->intent_ctl.vendor_captureEV;
			frame->shot->ctl.aa.vendor_captureExtraInfo = group->intent_ctl.vendor_captureExtraInfo;

			if (group->slot == GROUP_SLOT_PAF || group->slot == GROUP_SLOT_3AA) {
				memcpy(&(frame->shot->ctl.aa.vendor_multiFrameEvList), 
					&(group->intent_ctl.vendor_multiFrameEvList), EV_LIST_SIZE);
			}
			group->remainIntentCount--;
		} else {
			group->intent_ctl.captureIntent = AA_CAPTURE_INTENT_CUSTOM;
			group->intent_ctl.vendor_captureCount = 0;
			group->intent_ctl.vendor_captureExposureTime = 0;
			group->intent_ctl.vendor_captureEV = 0;
			group->intent_ctl.vendor_captureExtraInfo = 0;
			memset(&(group->intent_ctl.vendor_multiFrameEvList), 0, EV_LIST_SIZE);
		}

		minfo("frame count(%d), intent(%d), count(%d) captureExposureTime(%d) remainIntentCount(%d)\n", 
		    device, frame->fcount,
			frame->shot->ctl.aa.captureIntent, frame->shot->ctl.aa.vendor_captureCount,
			frame->shot->ctl.aa.vendor_captureExposureTime, group->remainIntentCount);
#endif
	}

	if (group->remainFlashCtlCount) {
		frame->shot->uctl.flashMode = group->flash_ctl;

		group->remainFlashCtlCount--;
		if (group->remainFlashCtlCount == 0)
			group->flash_ctl = CAMERA_FLASH_MODE_OFF;
	}

	spin_unlock_irqrestore(&group->slock_s_ctrl, flags);

	if (CHK_UPDATE_EXP_BY_RTA_CAPTURE_SCN(frame->shot->ctl.aa.captureIntent)) {
		/*
		 * Set shot_timeout value depend on captureintent
		 * CHK_UPDATE_EXP_BY_RTA_CAPTURE_SCN is defined by each metadata header
		 */
		if (sensor->exposure_fcount[frame->fcount % IS_EXP_BACKUP_COUNT] == (frame->fcount - SENSOR_REQUEST_DELAY)) {
			resourcemgr->shot_timeout =
				(sensor->exposure_value[frame->fcount % IS_EXP_BACKUP_COUNT] / 1000) + SHOT_TIMEOUT;
			resourcemgr->shot_timeout_tick = KEEP_FRAME_TICK_DEFAULT;
		} else {
			mgwarn("Intent(%d): There is no matching fcount(%d)!=(%d)", device, group,
				frame->shot->ctl.aa.captureIntent, frame->fcount,
				sensor->exposure_fcount[frame->fcount % IS_EXP_BACKUP_COUNT]);
		}
	} else if (frame->shot->ctl.aa.vendor_captureExposureTime >= ((SHOT_TIMEOUT * 1000) - 1000000)) {
		/*
		 * Adjust the shot timeout value based on sensor exposure time control.
		 * Exposure Time >= (SHOT_TIMEOUT - 1sec): Increase shot timeout value.
		 */
		resourcemgr->shot_timeout = (frame->shot->ctl.aa.vendor_captureExposureTime / 1000) + SHOT_TIMEOUT;
		resourcemgr->shot_timeout_tick = KEEP_FRAME_TICK_DEFAULT;
	} else if (resourcemgr->shot_timeout_tick > 0) {
		resourcemgr->shot_timeout_tick--;
	} else {
		resourcemgr->shot_timeout = SHOT_TIMEOUT;
	}

	/* fd information copy */
	if (!IS_ENABLED(ENABLE_SHARED_METADATA) && !IS_ENABLED(FAST_FDAE))
		memcpy(&frame->shot->uctl.fdUd, &device->fdUd,
			sizeof(struct camera2_fd_uctl));

	is_ischain_update_shot(device, frame);
}

#if defined(USE_OFFLINE_PROCESSING)
int is_ischain_update_sensor_mode(struct is_device_ischain *device,
	struct is_frame *frame)
{
	int rep_flag = 0;
	int ret = 0;
	struct param_sensor_config *sensor_config;
	IS_DECLARE_PMAP(pmap);

	rep_flag = test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state);
	IS_INIT_PMAP(pmap);

	if (rep_flag) {
		sensor_config = is_itf_g_param(device, NULL, PARAM_SENSOR_CONFIG);
		sensor_config->width = frame->shot_ext->user.sensor_mode.width;
		sensor_config->height = frame->shot_ext->user.sensor_mode.height;
		sensor_config->calibrated_width = device->module_enum->pixel_width;
		sensor_config->calibrated_height = device->module_enum->pixel_height;
		sensor_config->sensor_binning_ratio_x = frame->shot_ext->user.sensor_mode.binning;
		sensor_config->sensor_binning_ratio_y = frame->shot_ext->user.sensor_mode.binning;
		sensor_config->bns_binning_ratio_x = 1000;
		sensor_config->bns_binning_ratio_y = 1000;
		sensor_config->bns_margin_left = 0;
		sensor_config->bns_margin_top = 0;
		sensor_config->bns_output_width = frame->shot_ext->user.sensor_mode.width;
		sensor_config->bns_output_height = frame->shot_ext->user.sensor_mode.height;
		sensor_config->frametime = 10 * 1000 * 1000; /*  max exposure time */
		sensor_config->min_target_fps = 0;
		sensor_config->max_target_fps = 0;

		set_bit(PARAM_SENSOR_CONFIG, pmap);

		ret = is_itf_s_param_wrap(device, pmap);
		if (ret)
			merr("is_itf_s_param_wrap is fail(%d)", device, ret);

		device->binning = frame->shot_ext->user.sensor_mode.binning;
		device->off_dvfs_param.face = frame->shot_ext->user.off_dvfs.face;
		device->off_dvfs_param.num = frame->shot_ext->user.off_dvfs.num;
		device->off_dvfs_param.sensor = frame->shot_ext->user.off_dvfs.sensor;
		device->off_dvfs_param.resol = frame->shot_ext->user.off_dvfs.resol;
		device->off_dvfs_param.fps = frame->shot_ext->user.off_dvfs.fps;
	} else {
		frame->shot_ext->user.sensor_mode.width = is_sensor_g_width(device->sensor);
		frame->shot_ext->user.sensor_mode.height = is_sensor_g_height(device->sensor);;
		frame->shot_ext->user.sensor_mode.binning = is_sensor_g_bratio(device->sensor);
		frame->shot_ext->user.sensor_mode.ex_mode = is_sensor_g_ex_mode(device->sensor);
		frame->shot_ext->user.off_dvfs.face = device->off_dvfs_param.face;
		frame->shot_ext->user.off_dvfs.num = device->off_dvfs_param.num;
		frame->shot_ext->user.off_dvfs.sensor = device->off_dvfs_param.sensor;
		frame->shot_ext->user.off_dvfs.resol = device->off_dvfs_param.resol;
		frame->shot_ext->user.off_dvfs.fps = device->off_dvfs_param.fps;
	}

	return ret;
}
#endif

static int is_ischain_group_shot(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	enum is_frame_state next_state = FS_PROCESS;

	mgrdbgs(4, " %s\n", group, group, check_frame, check_frame->index, __func__);

	frame = is_ischain_check_frame(group, check_frame);
	if (IS_ERR_OR_NULL(frame)) {
		merr("is_ischain_check_frame is fail", device);
		return -EINVAL;
	}

	is_ischain_update_setfile(device, group, frame);

	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		is_ischain_update_otf_data(device, group, frame);

	if (!atomic_read(&group->head->scount)) /* first shot */
		set_bit(IS_SUBDEV_FORCE_SET, &group->head->leader.state);
	else
		clear_bit(IS_SUBDEV_FORCE_SET, &group->head->leader.state);

#if defined(USE_OFFLINE_PROCESSING)
	ret = is_ischain_update_sensor_mode(device, frame);
	if (ret) {
		merr("is_ischain_update_sensor_mode is fail(%d)", device, ret);
		goto p_err;
	}
#endif

	ret = is_ischain_child_tag(device, group, frame);
	if (ret) {
		merr("is_ischain_child_tag is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	if (frame->stripe_info.region_num
		&& frame->stripe_info.region_id < frame->stripe_info.region_num - 1)
		next_state = FS_STRIPE_PROCESS;

	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		framemgr = GET_HEAD_GROUP_FRAMEMGR(group, check_frame->cur_shot_idx);
		if (!framemgr) {
			mgerr("[F%d] Failed to get head framemgr. cur_shot_idx %d", group, group,
					check_frame->fcount, check_frame->cur_shot_idx);
			return -EINVAL;
		}

		set_bit(group->leader.id, &frame->out_flag);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_26, flags);
		trans_frame(framemgr, frame, next_state);

		if (IS_ENABLED(SENSOR_REQUEST_DELAY)
		    && test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (framemgr->queued_count[FS_REQUEST] < SENSOR_REQUEST_DELAY)
				mgrwarn(" late sensor control shot", device, group, frame);
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_26, flags);
	}

	return ret;
}

void is_bts_control(struct is_device_ischain *device)
{
#if defined(USE_BTS_SCEN)
	struct is_groupmgr *groupmgr;
	struct is_group *group_leader;

	FIMC_BUG_VOID(!device);
	FIMC_BUG_VOID(!device->groupmgr);

	groupmgr = device->groupmgr;
	group_leader = groupmgr->leader[device->instance];

	if (test_bit(IS_GROUP_OTF_INPUT, &group_leader->state))
		bts_scen_update(TYPE_CAM_BNS, false);
	else
		bts_scen_update(TYPE_CAM_BNS, true);
#endif
}
