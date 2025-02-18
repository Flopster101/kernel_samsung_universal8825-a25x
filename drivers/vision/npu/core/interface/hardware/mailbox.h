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

#ifndef NPU_MAILBOX_H_
#define NPU_MAILBOX_H_

#if (CONFIG_NPU_MAILBOX_VERSION == 4)
#include "mailbox_v4.h"
#elif (CONFIG_NPU_MAILBOX_VERSION == 7)
#include "mailbox_v7.h"
#elif (CONFIG_NPU_MAILBOX_VERSION == 9)
#include "mailbox_v9.h"
#else
#error Unsupported Mailbox version
#endif

#endif	/* NPU_MAILBOX_H_ */
