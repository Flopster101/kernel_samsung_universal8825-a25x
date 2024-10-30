// SPDX-License-Identifier: GPL-2.0+
/*
 * Structs and functions to support MPAM CPBM
 *
 * Copyright (C) 2021 Samsung Electronics
 */

#ifndef __SEC_MPAM_SYSFS_H
#define __SEC_MPAM_SYSFS_H

#include <linux/init.h>
#include <linux/notifier.h>

int mpam_late_init_notifier_register(struct notifier_block *nb);

int sec_mpam_late_init(void);
int sec_mpam_get_late_init(void);
int mpam_sysfs_init(void);
void mpam_sysfs_exit(void);

#endif /* __SEC_MPAM_SYSFS_H */
