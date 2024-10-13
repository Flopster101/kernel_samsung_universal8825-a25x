/*****************************************************************************
 *
 * Copyright (c) 2021 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include "dev.h"
#include "debug.h"
#include "slsi_cpuhp_monitor.h"

static enum cpuhp_state cpuhp_state_num = CPUHP_INVALID;

struct cpuhp_cb_node {
	struct hlist_node node;
	int (*online)(int cpu, void *data);
	int (*offline)(int cpu, void *data);
	void *data;
};

static int slsi_cpuhp_online(unsigned int cpu, struct hlist_node *node)
{
	struct cpuhp_cb_node *cb_node = container_of(node, struct cpuhp_cb_node, node);

	return cb_node->online(cpu, cb_node->data);
}

static int slsi_cpuhp_offline(unsigned int cpu, struct hlist_node *node)
{
	struct cpuhp_cb_node *cb_node = container_of(node, struct cpuhp_cb_node, node);

	return cb_node->offline(cpu, cb_node->data);
}

struct hlist_node *slsi_cpuhp_monitor_register_callback(int(*online)(int cpu, void *data), int(*offline)(int cpu, void *data), void *data)
{
	struct cpuhp_cb_node *new_node;

	if (cpuhp_state_num == CPUHP_INVALID) {
		SLSI_ERR_NODEV("cpuhp_monitor was not initialized.\n");
		return NULL;
	}

	new_node = kmalloc(sizeof(*new_node), GFP_KERNEL);
	if (!new_node) {
		SLSI_ERR_NODEV("cannot allocate memory for storing the callbacks.\n");
		return NULL;
	}

	new_node->online = online;
	new_node->offline = offline;
	new_node->data = data;
	cpuhp_state_add_instance_nocalls(cpuhp_state_num, &new_node->node);
	SLSI_INFO_NODEV("registered new callbacks\n");

	return &new_node->node;
}

int slsi_cpuhp_monitor_unregister_callback(struct hlist_node *node)
{
	struct cpuhp_cb_node *cb_node;

	if (cpuhp_state_num == CPUHP_INVALID) {
		SLSI_ERR_NODEV("cpuhp_monitor was not initialized.\n");
		return -ENODEV;
	}

	if (!node) {
		SLSI_ERR_NODEV("No node information\n");
		return -EINVAL;
	}

	cb_node = container_of(node, struct cpuhp_cb_node, node);
	cpuhp_state_remove_instance_nocalls(cpuhp_state_num, &cb_node->node);
	SLSI_INFO_NODEV("unregistered callbacks\n");
	kfree(cb_node);

	return 0;
}

int slsi_cpuhp_monitor_init(void)
{
	int ret;

	ret = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN, "net/wlbt:online", slsi_cpuhp_online, slsi_cpuhp_offline);
	if (ret > 0)
		cpuhp_state_num = ret;
	return ret;
}

void slsi_cpuhp_monitor_deinit(void)
{
	cpuhp_remove_multi_state(cpuhp_state_num);
	cpuhp_state_num = CPUHP_INVALID;
}
