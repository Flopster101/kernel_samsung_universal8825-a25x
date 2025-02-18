/*
 * Copyright (c) 2018-2021 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <linux/ngksm.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/timekeeping.h>
#include "ngksm_rate_limit.h"
#include "ngksm_common.h"

#define DURATION_MS ((u64)(1000L))
#define MAX_MESSAGES_PER_SEC (30)

static int ngksm_message_count;
static u64 ngksm_start_ms;

static u64 ngksm_get_time_ms(void)
{
	return div_u64(ktime_get_ns(), NSEC_PER_MSEC);
}

int __init ngksm_rate_limit_init(void)
{
	ngksm_message_count = 0;
	ngksm_start_ms = ngksm_get_time_ms();
	NGKSM_LOG_DEBUG("rate limit ngksm_start_ms=%llu",
		ngksm_start_ms);
	return NGKSM_SUCCESS;
}

int ngksm_check_message_rate_limit(void)
{
	u64 current_ms;

	current_ms = ngksm_get_time_ms();
	if (current_ms < ngksm_start_ms) {
		NGKSM_LOG_DEBUG("rate limit reset current_ms=%llu ngksm_start_ms=%llu ngksm_message_count=%d",
			current_ms, ngksm_start_ms, ngksm_message_count);
		ngksm_start_ms = current_ms;
		ngksm_message_count = 0;
	}

	if (current_ms >= (ngksm_start_ms + DURATION_MS)) {
		if (ngksm_message_count > MAX_MESSAGES_PER_SEC)
			NGKSM_LOG_ERROR("rate limit send message skip - count=%d, MAX_PER_SEC = %d",
				ngksm_message_count, MAX_MESSAGES_PER_SEC);
		ngksm_start_ms = current_ms;
		ngksm_message_count = 0;
		return NGKSM_SUCCESS;
	}

	if (++ngksm_message_count > MAX_MESSAGES_PER_SEC)
		return -EBUSY;
	return NGKSM_SUCCESS;
}
