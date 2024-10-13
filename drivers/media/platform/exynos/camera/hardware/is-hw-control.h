/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_HARDWARE_H
#define IS_DEVICE_HARDWARE_H

#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include "is-hw.h"
#include "is-device-ischain.h"
#include "is-interface-ischain.h"
#include "is-interface.h"
#include "is-err.h"
#include "is-param.h"

#define IS_HW_STOP_TIMEOUT		(HZ / 4)
#define IS_HW_CORE_END		(0x20141225) /* magic number */
#define IS_MAX_HW_FRAME		(20)
#define IS_MAX_HW_FRAME_LATE	(5)

#define SET_FILE_MAGIC_NUMBER		(0x12345679)

#if IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
#define SHOT_TIMEOUT			(10000) /* ms */
#else
#define SHOT_TIMEOUT			(3000) /* ms */
#endif

#define SETFILE_DESIGN_BIT_3AA_ISP	(3)
#define SETFILE_DESIGN_BIT_DRC		(4)
#define SETFILE_DESIGN_BIT_SCC		(5)
#define SETFILE_DESIGN_BIT_ODC		(6)
#define SETFILE_DESIGN_BIT_VDIS		(7)
#define SETFILE_DESIGN_BIT_TDNR		(8)
#define SETFILE_DESIGN_BIT_SCX_MCSC	(9)
#define SETFILE_DESIGN_BIT_FD_VRA	(10)
#define SETFILE_DESIGN_BIT_CLAHE	(11)

#define REQ_FLAG(id)			(1LL << (id))
#define OUT_FLAG(flag, subdev_id) (flag & ~(REQ_FLAG(subdev_id)))
#define check_hw_bug_count(this, count) \
	if (atomic_inc_return(&this->bug_count) > count) BUG_ON(1)

#define MCSC_INPUT_FROM_ISP_PATH	1
#define MCSC_INPUT_FROM_DCP_PATH	3

#define EXT1_CHAIN_OFFSET	(4)
#define EXT2_CHAIN_OFFSET	(8)
#define EXT3_CHAIN_OFFSET	(12)
#define EXT4_CHAIN_OFFSET	(16)

#define DEV_HW_3AA_MASK		((1 << DEV_HW_3AA0) | (1 << DEV_HW_3AA1) | (1 << DEV_HW_3AA2) | (1 << DEV_HW_3AA3))
#define DEV_HW_PAF_MASK		((1 << DEV_HW_PAF0) | (1 << DEV_HW_PAF1) | (1 << DEV_HW_PAF2) | (1 << DEV_HW_PAF3))

enum is_setfile_type {
	SETFILE_V2 = 2,
	SETFILE_V3 = 3,
	SETFILE_MAX
};

enum cal_type {
	CAL_TYPE_AF = 1,
	CAL_TYPE_LSC_UVSP = 2,
	CAL_TYPE_MAX
};

enum is_hw_streaming_state {
	HW_SENSOR_STREAMING = 0,
	HW_ISCHAIN_STREAMING = 1,
};

/* file-mapped setfile header */
struct __setfile_header_ver_2 {
	u32	magic_number;
	u32	scenario_num;
	u32	subip_num;
	u32	setfile_offset;
} __attribute__((__packed__));

struct __setfile_header_ver_3 {
	u32	magic_number;
	u32	designed_bit;
	char	version_code[4];
	char	revision_code[4];
	u32	scenario_num;
	u32	subip_num;
	u32	setfile_offset;
} __attribute__((__packed__));

union __setfile_header {
	u32	magic_number;
	struct __setfile_header_ver_2 ver_2;
	struct __setfile_header_ver_3 ver_3;
} __attribute__((__packed__));

struct __setfile_table_entry {
	u32 offset;
	u32 size;
};

/* processed setfile header */
struct is_setfile_header {
	u32	version;

	u32	num_ips;
	u32	num_scenarios;

	ulong	scenario_table_base;	/* scenario : setfile index for each IP */
	ulong	num_setfile_base;	/* number of setfile for each IP */
	ulong	setfile_table_base;	/* setfile index : [offset, size] */
	ulong	setfile_entries_base;	/* actual setfile entries */

	/* extra information depend on the version */
	u32	designed_bits;
	char	version_code[5];
	char	revision_code[5];
};

#define framemgr_e_barrier_common(this, index, flag)		\
	do {							\
		if (in_irq()) {					\
			framemgr_e_barrier(this, index);	\
		} else {						\
			framemgr_e_barrier_irqs(this, index, flag);	\
		}							\
	} while (0)

#define framemgr_x_barrier_common(this, index, flag)		\
	do {							\
		if (in_irq()) {					\
			framemgr_x_barrier(this, index);	\
		} else {						\
			framemgr_x_barrier_irqr(this, index, flag);	\
		}							\
	} while (0)

struct is_hw_ip *is_get_hw_ip(u32 group_id, struct is_hardware *hardware);
int is_hardware_probe(struct is_hardware *hardware,
	struct is_interface *itf, struct is_interface_ischain *itfc,
	struct platform_device *pdev);
int is_hardware_set_param(struct is_hardware *hardware, u32 instance,
	struct is_region *region, IS_DECLARE_PMAP(pmap), ulong hw_map);
int is_hardware_shot(struct is_hardware *hardware, u32 instance,
	struct is_group *group, struct is_frame *frame,
	struct is_framemgr *framemgr, ulong hw_map, u32 framenum);
int is_hardware_grp_shot(struct is_hardware *hardware, u32 instance,
	struct is_group *group, struct is_frame *frame, ulong hw_map);
int is_hardware_config_lock(struct is_hw_ip *hw_ip, u32 instance, u32 framenum);
void is_hardware_frame_start(struct is_hw_ip *hw_ip, u32 instance);
int is_hardware_sensor_start(struct is_hardware *hardware, u32 instance,
	ulong hw_map, struct is_group *group);
int is_hardware_sensor_stop(struct is_hardware *hardware, u32 instance,
	ulong hw_map, struct is_group *group);
int is_hardware_process_start(struct is_hardware *hardware, u32 instance,
	u32 group_id);
void is_hardware_process_stop(struct is_hardware *hardware, u32 instance,
	u32 group_id, u32 mode);
int is_hardware_open(struct is_hardware *hardware, u32 hw_id,
	struct is_group *group, u32 instance, bool rep_flag);
int is_hardware_close(struct is_hardware *hardware, u32 hw_id, u32 instance);
int is_hardware_change_chain(struct is_hardware *hardware, struct is_group *group,
	u32 instance, u32 next_id);
int is_hardware_frame_done(struct is_hw_ip *hw_ip, struct is_frame *frame,
	int wq_id, u32 output_id, enum ShotErrorType done_type, bool get_meta);
int is_hardware_load_setfile(struct is_hardware *hardware, ulong addr,
	u32 instance, ulong hw_map);
int is_hardware_apply_setfile(struct is_hardware *hardware, u32 instance,
	u32 scenario, ulong hw_map);
int is_hardware_delete_setfile(struct is_hardware *hardware, u32 instance,
	ulong hw_map);
void is_hardware_size_dump(struct is_hw_ip *hw_ip);
int is_hardware_runtime_resume(struct is_hardware *hardware);
int is_hardware_runtime_suspend(struct is_hardware *hardware);
int is_hardware_capture_meta_request(struct is_hardware *hardware,
	struct is_group *group, u32 instance, u32 fcount, u32 size, ulong addr);
void is_hardware_sfr_dump(struct is_hardware *hardware, u32 hw_id, bool flag_print_log);
void print_all_hw_frame_count(struct is_hardware *hardware);
int is_hardware_flush_frame_by_group(struct is_hardware *hardware,
	struct is_group *head, u32 instance);
int is_hardware_restore_by_group(struct is_hardware *hardware,
	struct is_group *group, u32 instance);
int is_hardware_recovery_shot(struct is_hardware *hardware, u32 instance,
	struct is_group *group, u32 recov_fcount, struct is_framemgr *framemgr);
void is_hardware_dma_cfg_64bit(char *name, struct is_hw_ip *hw_ip,
			struct is_frame *frame, int cur_idx,
			u32 *cmd, u32 plane,
			u64 *dst_kva, u64 *src_kva);
u32 is_hardware_dma_cfg_32bit(char *name, struct is_hw_ip *hw_ip,
			struct is_frame *frame, int cur_idx, u32 num_buffers,
			u32 *cmd, u32 plane,
			u32 *dst_dva, u32 *src_dva);
u32 is_hardware_dma_cfg(char *name, struct is_hw_ip *hw_ip,
			struct is_frame *frame, int cur_idx, u32 num_buffers,
			u32 *cmd, u32 plane,
			u32 *dst_dva, dma_addr_t *src_dva);
void _is_hw_frame_dbg_trace(struct is_hw_ip *hw_ip, u32 fcount, u32 dbg_pts);
void _is_hw_frame_dbg_ext_trace(struct is_hw_ip *hw_ip, u32 fcount, u32 dbg_pts, u32 ext_id);
int is_hardware_get_offline_data(struct is_hardware *hardware, u32 instance,
	struct is_group *group, void *data_desc, int fcount);
void is_hardware_debug_otf(struct is_hardware *hardware, struct is_group *group);
int is_hardware_free_internal_buffer(struct is_device_ischain *device,
	struct is_subdev *subdev);
int is_hardware_alloc_internal_buffer(struct is_device_ischain *device, struct is_subdev *subdev,
	int vid, u32 width, u32 height, u32 bitsperpixel, u32 num_buffers, const char *name);
int clear_gather_crc_status(u32 instance, struct is_hw_ip *hw_ip);
#endif
