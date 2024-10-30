/*****************************************************************************
 *
 * Copyright (c) 2021 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __CPUHP_MONITOR_H__
#define __CPUHP_MONITOR_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/cpuhotplug.h>

int slsi_cpuhp_monitor_init(void);
void slsi_cpuhp_monitor_deinit(void);

/* This function installs the instance including callbacks
 * @online : startup callback
 * @offline : teardown callback
 * @data : Address of the data to be used in the callback function
 *
 * If the registration is successful, the hlist_node is returned,
 * or NULL is returned.
 */
struct hlist_node *slsi_cpuhp_monitor_register_callback(int (*online)(int cpu, void *data), int (*offline)(int cpu, void *data), void *data);

/* This function removes the instance
 * @node : Registered node(retrun value of register_cpuhp_callback)
 *
 * If the unregistration is successful, zero is returned,
 * or negative number is returned.
 */
int slsi_cpuhp_monitor_unregister_callback(struct hlist_node *node);

#endif
