/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Common IPC Driver Types
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *
 */

#ifndef _MAILBOX_IPC_TYPE_H
#define _MAILBOX_IPC_TYPE_H

#define CIPC_RAW_READL(a)      (*(volatile unsigned int *)(a))
#define CIPC_RAW_WRITEL(v, a)  (*(volatile unsigned int *)(a) = ((unsigned int)v))

#define CIPC_TRUE 1
#define CIPC_FALSE 0
#define CIPC_NULL 0
#define CIPC_ERR -1

#ifndef CU64
typedef unsigned long long CU64;
#endif

#ifndef CU32
typedef unsigned long int CU32;
#endif
#endif
