/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "panel_obj.h"

#ifndef __PANEL_DELAY_H__
#define __PANEL_DELAY_H__

struct delayinfo {
	struct pnobj base;
	u32 usec;
	u32 nframe;
	u32 nvsync;
	ktime_t s_time;
	bool no_sleep;
};

struct timer_delay_begin_info {
	struct pnobj base;
	struct delayinfo *delay;
};

#define DLYINFO(_dlyname) (_dlyname)

#define TIMER_DLYINFO_BEGIN(_dlyname) PN_CONCAT(begin, _dlyname)
#define TIMER_DLYINFO(_dlyname) (_dlyname)

#define VARIABLE_DLYINFO(_dlyname) (_dlyname)

#define __DLYINFO_INITIALIZER(_dlyname, _type, _usec, _nframe, _nvsync, _no_sleep) \
	{ .base = __PNOBJ_INITIALIZER(_dlyname, _type) \
	, .usec = (_usec)\
	, .nframe = (_nframe)\
	, .nvsync = (_nvsync)\
	, .s_time = (0ULL)\
	, .no_sleep = (_no_sleep)}

#define DEFINE_PANEL_UDELAY_NO_SLEEP(_dlyname, _usec) \
	struct delayinfo DLYINFO(_dlyname) = \
		__DLYINFO_INITIALIZER(_dlyname, CMD_TYPE_DELAY, _usec, 0, 0, true)

#define DEFINE_PANEL_MDELAY_NO_SLEEP(_dlyname, _msec) \
	struct delayinfo DLYINFO(_dlyname) = \
		__DLYINFO_INITIALIZER(_dlyname, CMD_TYPE_DELAY, (_msec) * 1000, 0, 0, true)

#define DEFINE_PANEL_UDELAY(_dlyname, _usec) \
	struct delayinfo DLYINFO(_dlyname) = \
		__DLYINFO_INITIALIZER(_dlyname, CMD_TYPE_DELAY, _usec, 0, 0, false)

#define DEFINE_PANEL_MDELAY(_dlyname, _msec) \
	struct delayinfo DLYINFO(_dlyname) = \
		__DLYINFO_INITIALIZER(_dlyname, CMD_TYPE_DELAY, (_msec) * 1000, 0, 0, false)

#define DEFINE_PANEL_FRAME_DELAY(_dlyname, _nframe) \
	struct delayinfo DLYINFO(_dlyname) = \
		__DLYINFO_INITIALIZER(_dlyname, CMD_TYPE_DELAY, 0, _nframe, 0, false)

#define DEFINE_PANEL_VSYNC_DELAY(_dlyname, _nvsync) \
	struct delayinfo DLYINFO(_dlyname) = \
		__DLYINFO_INITIALIZER(_dlyname, CMD_TYPE_DELAY, 0, 0, _nvsync, false)

#define DEFINE_PANEL_TIMER_BEGIN(_dlyname, _timer_delay_) \
struct timer_delay_begin_info TIMER_DLYINFO_BEGIN(_dlyname) = \
	{ .base = __PNOBJ_INITIALIZER(_dlyname, CMD_TYPE_TIMER_DELAY_BEGIN) \
	, .delay = (_timer_delay_) }

#define DEFINE_PANEL_TIMER_UDELAY(_dlyname, _usec) \
	struct delayinfo TIMER_DLYINFO(_dlyname) = \
		__DLYINFO_INITIALIZER(_dlyname, CMD_TYPE_TIMER_DELAY, _usec, 0, 0, false)

#define DEFINE_PANEL_TIMER_MDELAY(_dlyname, _msec) \
	struct delayinfo TIMER_DLYINFO(_dlyname) = \
		__DLYINFO_INITIALIZER(_dlyname, CMD_TYPE_TIMER_DELAY, (_msec) * 1000, 0, 0, false)

#define DEFINE_PANEL_TIMER_FRAME_DELAY(_dlyname, _nframe) \
	struct delayinfo TIMER_DLYINFO(_dlyname) = \
		__DLYINFO_INITIALIZER(_dlyname, CMD_TYPE_TIMER_DELAY, 0, _nframe, 0, false)

bool is_valid_delay(struct delayinfo *delay);
bool is_valid_timer_delay(struct delayinfo *delay);
bool is_valid_timer_delay_begin(struct timer_delay_begin_info *begin);
unsigned int get_delay_type(struct delayinfo *delay);
char *get_delay_name(struct delayinfo *delay);
char *get_timer_delay_begin_name(struct timer_delay_begin_info *begin);
struct delayinfo *create_delay(char *name, u32 type, u32 usec, u32 nframe, u32 nvsync, bool no_sleep);
void destroy_delay(struct delayinfo *delay);
struct timer_delay_begin_info *create_timer_delay_begin(char *name, struct delayinfo *delay);
void destroy_timer_delay_begin(struct timer_delay_begin_info *begin);

#endif /* __PANEL_DELAY_H__ */
