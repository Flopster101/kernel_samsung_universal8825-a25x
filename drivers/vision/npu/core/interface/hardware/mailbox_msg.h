/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef NPU_MAILBOX_MSG_H_
#define NPU_MAILBOX_MSG_H_

/* Increase sub-version to support new load interface. */
#define NPU_COMMAND_SUB_VERSION 1
#if (CONFIG_NPU_COMMAND_VERSION == 7)
#include "mailbox_msg_v7.h"
#elif (CONFIG_NPU_COMMAND_VERSION == 8)
#include "mailbox_msg_v8.h"
#elif (CONFIG_NPU_COMMAND_VERSION == 9)
#include "mailbox_msg_v9.h"
#elif (CONFIG_NPU_COMMAND_VERSION == 10)
#include "mailbox_msg_v10.h"
#else
#error Unsupported Command version
#endif

#endif	/* NPU_MAILBOX_MSG_H_ */
