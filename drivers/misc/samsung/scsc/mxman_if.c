/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/delay.h>
#include <linux/module.h>
#include <scsc/kic/slsi_kic_lib.h>
#include <scsc/scsc_release.h>
#include <scsc/scsc_mx.h>
#include "mxman_if.h"
#include "mxman.h"

int mxman_if_get_state(struct mxman *mxman)
{
	return mxman_get_state(mxman);
}
EXPORT_SYMBOL(mxman_if_get_state);

bool mxman_if_subsys_in_failed_state(struct mxman *mxman, enum scsc_subsystem sub)
{
	return mxman_subsys_in_failed_state(mxman, sub);
}
EXPORT_SYMBOL(mxman_if_subsys_in_failed_state);

u64 mxman_if_get_last_panic_time(struct mxman *mxman)
{
	return mxman_get_last_panic_time(mxman);
}
EXPORT_SYMBOL(mxman_if_get_last_panic_time);

u32 mxman_if_get_panic_code(struct mxman *mxman)
{
	return mxman_get_panic_code(mxman);
}
EXPORT_SYMBOL(mxman_if_get_panic_code);

void mxman_if_show_last_panic(struct mxman *mxman)
{
	mxman_show_last_panic(mxman);
}
EXPORT_SYMBOL(mxman_if_show_last_panic);

u32 *mxman_if_get_last_panic_rec(struct mxman *mxman)
{
	return mxman_get_last_panic_rec(mxman);
}
EXPORT_SYMBOL(mxman_if_get_last_panic_rec);

u16 mxman_if_get_last_panic_rec_sz(struct mxman *mxman)
{
	return mxman_get_last_panic_rec_sz(mxman);
}
EXPORT_SYMBOL(mxman_if_get_last_panic_rec_sz);

void mxman_if_reinit_completion(struct mxman *mxman)
{
	mxman_reinit_completion(mxman);
}
EXPORT_SYMBOL(mxman_if_reinit_completion);

int mxman_if_wait_for_completion_timeout(struct mxman *mxman, u32 ms)
{
	return mxman_wait_for_completion_timeout(mxman, ms);
}
EXPORT_SYMBOL(mxman_if_wait_for_completion_timeout);

void mxman_if_fail(struct mxman *mxman, u16 scsc_panic_code, const char *reason, enum scsc_subsystem id)
{
	mxman_fail(mxman, scsc_panic_code, reason, id);
}
EXPORT_SYMBOL(mxman_if_fail);


void mxman_if_set_syserr_recovery_in_progress(struct mxman *mxman, bool value)
{
	mxman_set_syserr_recovery_in_progress(mxman, value);
}
EXPORT_SYMBOL(mxman_if_set_syserr_recovery_in_progress);

void mxman_if_set_last_syserr_recovery_time(struct mxman *mxman, unsigned long value)
{
	mxman_set_last_syserr_recovery_time(mxman, value);
}
EXPORT_SYMBOL(mxman_if_set_last_syserr_recovery_time);

bool mxman_if_get_syserr_recovery_in_progress(struct mxman *mxman)
{
	return mxman_get_syserr_recovery_in_progress(mxman);
}
EXPORT_SYMBOL(mxman_if_get_syserr_recovery_in_progress);

u16 mxman_if_get_last_syserr_subsys(struct mxman *mxman)
{
	return mxman_get_last_syserr_subsys(mxman);
}
EXPORT_SYMBOL(mxman_if_get_last_syserr_subsys);

int mxman_if_force_panic(struct mxman *mxman, enum scsc_subsystem sub)
{
	return mxman_force_panic(mxman, sub);
}
EXPORT_SYMBOL(mxman_if_force_panic);

int mxman_if_lerna_send(struct mxman *mxman, void *data, u32 size)
{
	return mxman_lerna_send(mxman, data, size);
}
EXPORT_SYMBOL(mxman_if_lerna_send);

int mxman_if_open(struct mxman *mxman, enum scsc_subsystem sub, void *data, size_t data_sz)
{
	return mxman_open(mxman, sub, data, data_sz);
}
EXPORT_SYMBOL(mxman_if_open);

void mxman_if_close(struct mxman *mxman, enum scsc_subsystem sub)
{
	mxman_close(mxman, sub);
}
EXPORT_SYMBOL(mxman_if_close);

bool mxman_if_subsys_active(struct mxman *mxman, enum scsc_subsystem sub)
{
	return mxman_subsys_active(mxman, sub);
}
EXPORT_SYMBOL(mxman_if_subsys_active);
