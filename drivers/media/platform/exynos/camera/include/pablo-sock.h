// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if IS_ENABLED(CONFIG_PABLO_SOCKET_LAYER)
int pablo_tuning_start(void);
void pablo_tuning_stop(void *data);

int pablo_sock_init(void);
void pablo_sock_exit(void);
#else
#define pablo_tuning_start()	({0;})
#define pablo_tuning_stop(d)	do { } while(0)

#define pablo_sock_init()	({0;})
#define pablo_sock_exit()	do { } while(0)
#endif
