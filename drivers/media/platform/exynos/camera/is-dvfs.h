/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DVFS_H
#define IS_DVFS_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "is-groupmgr.h"
#include "is-device-ischain.h"

#define IS_DVFS_STATIC		0
#define IS_DVFS_DYNAMIC		1

#define IS_DVFS_PATH_M2M	0
#define IS_DVFS_PATH_OTF	1

#define	DVFS_NOT_MATCHED	0 /* not matched in this scenario */
#define	DVFS_MATCHED		1 /* matched in this scenario */
#define	DVFS_SKIP		2 /* matched, but do not anything. skip changing dvfs */

#define IS_DVFS_DEC_DTIME	50

#define KEEP_FRAME_TICK_FAST_LAUNCH	(10)
#define KEEP_FRAME_TICK_THROTT (10)
#define IS_DVFS_DUAL_TICK (4)
#define IS_DVFS_SKIP_DYNAMIC (1)
#define DVFS_SN_STR(__SCENARIO) #__SCENARIO
#define GET_DVFS_CHK_FUNC(__SCENARIO) check_ ## __SCENARIO
#define DECLARE_DVFS_CHK_FUNC(__SCENARIO) \
	int check_ ## __SCENARIO \
		(struct is_device_ischain *device, struct camera2_shot *shot, int position, int resol, \
			int fps, int stream_cnt, int streaming_cnt, unsigned long sensor_map, \
			struct is_dual_info *dual_info, ...)
#define DECLARE_EXT_DVFS_CHK_FUNC(__SCENARIO) \
	int check_ ## __SCENARIO \
		(struct is_device_sensor *device, int position, int resol, int fps, \
			int stream_cnt, int streaming_cnt, unsigned long sensor_map, \
			struct is_dual_info *dual_info, ...)
#define GET_KEY_FOR_DVFS_TBL_IDX(__HAL_VER) \
	(#__HAL_VER "_TBL_IDX")

#define SIZE_HD (720 * 480)
#define SIZE_FHD (1920 * 1080)
#define SIZE_WHD (2560 * 1440)
#define SIZE_UHD (3840 * 2160)
#define SIZE_8K (7680 * 4320)
#define SIZE_16MP_FHD_BDS (3072 * 2304) /* based a 4:3 */
#define SIZE_12MP_FHD_BDS (2688 * 2016) /* based a 4:3 */
#define SIZE_12MP_QHD_BDS (3072 * 2304)
#define SIZE_12MP_UHD_BDS (4032 * 3024)
#define SIZE_8MP_FHD_BDS (2176 * 1632)
#define SIZE_8MP_QHD_BDS (3264 * 1836)
#define SIZE_17MP_FHD_BDS (2432 * 1824)
#define SIZE_17MP (4864 * 3648)

#define dbg_dvfs(level, fmt, device, args...) \
	dbg_common((is_get_debug_param(IS_DEBUG_PARAM_DVFS)) >= (level), \
		"[%d]", fmt, device->instance, ##args)

struct is_dvfs_scenario_ctrl {
	unsigned int cur_scenario_id;	/* selected scenario idx */
	int cur_frame_tick;	/* remained frame tick to keep qos lock in dynamic scenario */
	int scenario_cnt;	/* total scenario count */
	int fixed_scenario_cnt;	/* always scenario count */
	char *scenario_nm;	/* string of scenario_id */
};

#ifdef CONFIG_PM_DEVFREQ
int is_dvfs_init(struct is_resourcemgr *resourcemgr);
int is_dvfs_sel_static(struct is_device_ischain *device);
int is_dvfs_sel_dynamic(struct is_device_ischain *device, struct is_group *group,
	struct is_frame *frame);
int is_dvfs_sel_external(struct is_device_sensor *device);
int is_dvfs_get_cur_lv(u32 dvfs_t);
int is_dvfs_get_freq(u32 dvfs_t);
int is_add_dvfs(struct is_core *core, int scenario_id);
int is_remove_dvfs(struct is_core *core, int scenario_id);
int is_set_dvfs_m2m(struct is_device_ischain *device, int scenario_id);
int is_set_dvfs_otf(struct is_device_ischain *device, int scenario_id);
int pablo_set_static_dvfs(struct is_device_ischain *device,
			  const char *suffix, int force_scn, int path);
void is_set_static_dvfs(struct is_device_ischain *device, bool stream_on);
unsigned int is_get_bit_count(unsigned long bits);
#else
#define is_dvfs_init(r)			({0; })
#define is_dvfs_sel_static(d)		({0; })
#define is_dvfs_sel_dynamic(d, g, f)	({0; })
#define is_dvfs_sel_external(d)		({0; })
#define is_dvfs_get_cur_lv(d)		({0; })
#define is_dvfs_get_freq(d)		({0; })
#define is_add_dvfs(c, i)		({0; })
#define is_remove_dvfs(c, i)		({0; })
#define is_set_dvfs_m2m(d, i)		({0; })
#define is_set_dvfs_otf(d, i)		({0; })
#define is_dual_mode_update(d, g, f)	do { } while (0)
#define is_dual_dvfs_update(d, g, f)	do { } while (0)
#define is_get_bit_count(b)		({0; })
#endif /* CONFIG_PM_DEVFREQ */
#endif /* IS_DVFS_H */
