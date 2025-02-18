// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>

#include "dsp-log.h"
#include "dsp-hw-common-llc.h"

static void __dsp_hw_common_llc_dump_scen(struct dsp_llc_scenario *scen)
{
	unsigned int idx;
	struct dsp_llc_region *region;

	dsp_enter();
	dsp_dbg("scenario[%s] id : %u / region_count : %u\n", scen->name,
			scen->id, scen->region_count);
	for (idx = 0; idx < scen->region_count; ++idx) {
		region = &scen->region_list[idx];
		if (region->idx)
			dsp_dbg("region[%s] idx : %u / way : %u\n",
					region->name, region->idx, region->way);
		else
			dsp_dbg("region(%u) idx is not set\n", idx);
	}

	dsp_leave();
}

static int __dsp_hw_common_llc_check_region(struct dsp_llc_scenario *scen)
{
	int ret;
	unsigned int idx;
	struct dsp_llc_region *region;

	dsp_enter();
	for (idx = 0; idx < scen->region_count; ++idx) {
		region = &scen->region_list[idx];
		if (!region->idx) {
			ret = -EINVAL;
			dsp_err("region(%u) idx is not set\n", idx);
			goto p_err;
		}
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_common_llc_put(struct dsp_llc *llc,
		struct dsp_llc_scenario *scen)
{
	int ret;
	unsigned int idx;
	struct dsp_llc_region *region;

	dsp_enter();
	ret = __dsp_hw_common_llc_check_region(scen);
	if (ret) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) was not initilized\n", scen->name);
		goto p_err;
	}
	__dsp_hw_common_llc_dump_scen(scen);

	mutex_lock(&scen->lock);
	if (scen->enabled) {
		for (idx = 0; idx < scen->region_count; ++idx) {
			region = &scen->region_list[idx];
#ifdef CONFIG_EXYNOS_SCI_MODULE
			llc_region_alloc(region->idx, 0, 0);
#endif
			dsp_dbg("llc region[%s/%u] : off\n", region->name,
					region->idx);
		}
		scen->enabled = false;
		dsp_info("llc scen[%s] is disabled\n", scen->name);
	} else {
		dsp_dbg("llc scen[%s] is not enabled\n", scen->name);
	}
	mutex_unlock(&scen->lock);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_common_llc_get(struct dsp_llc *llc,
		struct dsp_llc_scenario *scen)
{
	int ret;
	unsigned int idx;
	struct dsp_llc_region *region;

	dsp_enter();
	ret = __dsp_hw_common_llc_check_region(scen);
	if (ret) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) was not initilized\n", scen->name);
		goto p_err;
	}
	__dsp_hw_common_llc_dump_scen(scen);

	for (idx = 0; idx < llc->scen_count; ++idx) {
		if (idx == scen->id)
			continue;

		__dsp_hw_common_llc_put(llc, &llc->scen[idx]);
	}

	mutex_lock(&scen->lock);
	if (!scen->enabled) {
		for (idx = 0; idx < scen->region_count; ++idx) {
			region = &scen->region_list[idx];
#ifdef CONFIG_EXYNOS_SCI_MODULE
			llc_region_alloc(region->idx, 1, region->way);
#endif
			dsp_dbg("llc region[%s/%u] : on\n", region->name,
					region->idx);
		}
		scen->enabled = true;
		dsp_info("llc scen[%s] is enabled\n", scen->name);
	} else {
		dsp_dbg("llc scen[%s] is already enabled\n", scen->name);
	}
	mutex_unlock(&scen->lock);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static struct dsp_llc_scenario *__dsp_hw_common_llc_get_scen(
		struct dsp_llc *llc, unsigned char *name)
{
	int idx;

	dsp_enter();
	for (idx = 0; idx < llc->scen_count; ++idx) {
		if (!strncmp(llc->scen[idx].name, name,
					DSP_LLC_SCENARIO_NAME_LEN)) {
			dsp_leave();
			return &llc->scen[idx];
		}
	}

	dsp_err("llc scenario_name(%s) is invalid\n", name);
	return NULL;
}

int dsp_hw_common_llc_get_by_name(struct dsp_llc *llc, unsigned char *name)
{
	int ret;
	struct dsp_llc_scenario *scen;

	dsp_enter();
	scen = __dsp_hw_common_llc_get_scen(llc, name);
	if (!scen) {
		ret = -EINVAL;
		goto p_err;
	}

	ret = __dsp_hw_common_llc_get(llc, scen);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_llc_put_by_name(struct dsp_llc *llc, unsigned char *name)
{
	int ret;
	struct dsp_llc_scenario *scen;

	dsp_enter();
	scen = __dsp_hw_common_llc_get_scen(llc, name);
	if (!scen) {
		ret = -EINVAL;
		goto p_err;
	}

	ret = __dsp_hw_common_llc_put(llc, scen);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_common_llc_check_id(struct dsp_llc *llc, unsigned int id)
{
	dsp_enter();
	if (id >= llc->scen_count) {
		dsp_err("llc scenario_id(%u) is invalid\n", id);
		return -EINVAL;
	}
	dsp_leave();
	return 0;
}

int dsp_hw_common_llc_get_by_id(struct dsp_llc *llc, unsigned int id)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_common_llc_check_id(llc, id);
	if (ret)
		goto p_err;

	ret = __dsp_hw_common_llc_get(llc, &llc->scen[id]);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_llc_put_by_id(struct dsp_llc *llc, unsigned int id)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_common_llc_check_id(llc, id);
	if (ret)
		goto p_err;

	ret = __dsp_hw_common_llc_put(llc, &llc->scen[id]);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_llc_all_put(struct dsp_llc *llc)
{
	unsigned int idx;

	dsp_enter();
	for (idx = 0; idx < llc->scen_count; ++idx)
		__dsp_hw_common_llc_put(llc, &llc->scen[idx]);

	dsp_leave();
	return 0;
}

int dsp_hw_common_llc_open(struct dsp_llc *llc)
{
	int idx;
	struct dsp_llc_scenario *scen;

	dsp_enter();
	for (idx = 0; idx < llc->scen_count; ++idx) {
		scen = &llc->scen[idx];
		__dsp_hw_common_llc_dump_scen(scen);
	}
	dsp_leave();
	return 0;
}

int dsp_hw_common_llc_close(struct dsp_llc *llc)
{
	dsp_enter();
	llc->ops->all_put(llc);
	dsp_leave();
	return 0;
}

int dsp_hw_common_llc_probe(struct dsp_llc *llc, void *sys)
{
	int ret;
	struct dsp_llc_scenario *scen;
	int idx;

	dsp_enter();
	llc->sys = sys;

	if (!llc->scen_count) {
		dsp_info("llc_scenario count is zero\n");
		return 0;
	}

	scen = kzalloc(sizeof(*scen) * llc->scen_count, GFP_KERNEL);
	if (!scen) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc llc_scenario\n");
		goto p_err;
	}

	for (idx = 0; idx < llc->scen_count; ++idx) {
		scen[idx].id = idx;
		mutex_init(&scen[idx].lock);
	}

	llc->scen = scen;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_hw_common_llc_remove(struct dsp_llc *llc)
{
	int idx;

	dsp_enter();
	if (!llc->scen_count)
		return;

	for (idx = 0; idx < llc->scen_count; ++idx)
		mutex_destroy(&llc->scen[idx].lock);

	kfree(llc->scen);
	dsp_leave();
}

int dsp_hw_common_llc_set_ops(struct dsp_llc *llc, const void *ops)
{
	dsp_enter();
	llc->ops = ops;
	dsp_leave();
	return 0;
}
