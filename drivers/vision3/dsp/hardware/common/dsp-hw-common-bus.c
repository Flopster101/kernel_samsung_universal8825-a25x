// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>

#include "dsp-log.h"
#include "dsp-hw-common-bus.h"

static int __dsp_hw_common_bus_mo_put(struct dsp_bus *bus,
		struct dsp_bus_scenario *scen)
{
	int ret;

	dsp_enter();
	if (!scen->bts_scen_idx) {
		ret = -EINVAL;
		dsp_dbg("scenario_name(%s) was not initilized\n", scen->name);
		goto p_err;
	}
	dsp_dbg("scenario[%s] idx : %u\n", scen->name, scen->bts_scen_idx);

	mutex_lock(&scen->lock);
	if (scen->enabled) {
		bts_del_scenario(scen->bts_scen_idx);
		scen->enabled = false;
		dsp_info("bus scenario[%s] is disabled\n", scen->name);
	} else {
		dsp_dbg("bus scenario[%s] is not enabled\n", scen->name);
	}
	mutex_unlock(&scen->lock);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_common_bus_mo_get(struct dsp_bus *bus,
		struct dsp_bus_scenario *scen)
{
	int ret;
	unsigned int idx;

	dsp_enter();
	if (!scen->bts_scen_idx) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) was not initilized\n", scen->name);
		goto p_err;
	}
	dsp_dbg("scenario[%s] idx : %u\n", scen->name, scen->bts_scen_idx);

	for (idx = 0; idx < bus->scen_count; ++idx) {
		if (idx == scen->id)
			continue;

		__dsp_hw_common_bus_mo_put(bus, &bus->scen[idx]);
	}

	mutex_lock(&scen->lock);
	if (!scen->enabled) {
		bts_add_scenario(scen->bts_scen_idx);
		scen->enabled = true;
		dsp_info("bus scenario[%s] is enabled\n", scen->name);
	} else {
		dsp_dbg("bus scenario[%s] is already enabled\n", scen->name);
	}
	mutex_unlock(&scen->lock);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static struct dsp_bus_scenario *__dsp_hw_common_bus_get_scen(
		struct dsp_bus *bus, unsigned char *name)
{
	int idx;

	dsp_enter();
	for (idx = 0; idx < bus->scen_count; ++idx) {
		if (!strncmp(bus->scen[idx].name, name,
					DSP_BUS_SCENARIO_NAME_LEN)) {
			dsp_leave();
			return &bus->scen[idx];
		}
	}

	dsp_err("bus scenario_name(%s) is invalid\n", name);
	return NULL;
}

int dsp_hw_common_bus_mo_get_by_name(struct dsp_bus *bus, unsigned char *name)
{
	int ret;
	struct dsp_bus_scenario *scen;

	dsp_enter();
	scen = __dsp_hw_common_bus_get_scen(bus, name);
	if (!scen) {
		ret = -EINVAL;
		goto p_err;
	}

	ret = __dsp_hw_common_bus_mo_get(bus, scen);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_bus_mo_put_by_name(struct dsp_bus *bus, unsigned char *name)
{
	int ret;
	struct dsp_bus_scenario *scen;

	dsp_enter();
	scen = __dsp_hw_common_bus_get_scen(bus, name);
	if (!scen) {
		ret = -EINVAL;
		goto p_err;
	}

	ret = __dsp_hw_common_bus_mo_put(bus, scen);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_common_bus_check_id(struct dsp_bus *bus, unsigned int id)
{
	dsp_enter();
	if (id >= bus->scen_count) {
		dsp_err("bus scenario_id(%u) is invalid\n", id);
		return -EINVAL;
	}
	dsp_leave();
	return 0;
}


int dsp_hw_common_bus_mo_get_by_id(struct dsp_bus *bus, unsigned int id)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_common_bus_check_id(bus, id);
	if (ret)
		goto p_err;

	ret = __dsp_hw_common_bus_mo_get(bus, &bus->scen[id]);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_bus_mo_put_by_id(struct dsp_bus *bus, unsigned int id)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_common_bus_check_id(bus, id);
	if (ret)
		goto p_err;

	ret = __dsp_hw_common_bus_mo_put(bus, &bus->scen[id]);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_bus_mo_all_put(struct dsp_bus *bus)
{
	unsigned int idx;

	dsp_enter();
	for (idx = 0; idx < bus->scen_count; ++idx)
		__dsp_hw_common_bus_mo_put(bus, &bus->scen[idx]);

	dsp_leave();
	return 0;
}

int dsp_hw_common_bus_open(struct dsp_bus *bus)
{
	int idx;
	struct dsp_bus_scenario *scen;

	dsp_enter();
	for (idx = 0; idx < bus->scen_count; ++idx) {
		scen = &bus->scen[idx];

		dsp_dbg("scenario[%d/%d] info : [%s/%u]\n",
				idx, bus->scen_count, scen->name,
				scen->bts_scen_idx);
	}
	dsp_leave();
	return 0;
}

int dsp_hw_common_bus_close(struct dsp_bus *bus)
{
	dsp_enter();
	dsp_hw_common_bus_mo_all_put(bus);
	dsp_leave();
	return 0;
}

int dsp_hw_common_bus_probe(struct dsp_bus *bus, void *sys)
{
	int ret;
	struct dsp_bus_scenario *scen;
	int idx;

	dsp_enter();
	bus->sys = sys;

	if (!bus->scen_count) {
		dsp_info("bus_scenario count is zero\n");
		return 0;
	}

	scen = kzalloc(sizeof(*scen) * bus->scen_count, GFP_KERNEL);
	if (!scen) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc bus_scenario(%u)\n", bus->scen_count);
		goto p_err;
	}

	for (idx = 0; idx < bus->scen_count; ++idx) {
		scen[idx].id = idx;
		mutex_init(&scen[idx].lock);
	}

	bus->scen = scen;
	bus->max_scen_id = -1;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_hw_common_bus_remove(struct dsp_bus *bus)
{
	int idx;

	dsp_enter();
	if (!bus->scen_count)
		return;

	for (idx = 0; idx < bus->scen_count; ++idx)
		mutex_destroy(&bus->scen[idx].lock);
	kfree(bus->scen);
	dsp_leave();
}

int dsp_hw_common_bus_set_ops(struct dsp_bus *bus, const void *ops)
{
	dsp_enter();
	bus->ops = ops;
	dsp_leave();
	return 0;
}
