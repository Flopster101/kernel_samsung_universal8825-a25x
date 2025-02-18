// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-p0-system.h"
#include "dsp-hw-common-bus.h"
#include "dsp-hw-p0-bus.h"

static int dsp_hw_p0_bus_probe(struct dsp_bus *bus, void *sys)
{
	int ret;

	dsp_enter();
	bus->scen_count = DSP_P0_MO_SCENARIO_COUNT;

	ret = dsp_hw_common_bus_probe(bus, sys);
	if (ret) {
		dsp_err("Failed to probe common bus(%d)\n", ret);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static const struct dsp_bus_ops p0_bus_ops = {
	.mo_get_by_name	= dsp_hw_common_bus_mo_get_by_name,
	.mo_put_by_name	= dsp_hw_common_bus_mo_put_by_name,
	.mo_get_by_id	= dsp_hw_common_bus_mo_get_by_id,
	.mo_put_by_id	= dsp_hw_common_bus_mo_put_by_id,
	.mo_all_put	= dsp_hw_common_bus_mo_all_put,

	.open		= dsp_hw_common_bus_open,
	.close		= dsp_hw_common_bus_close,
	.probe		= dsp_hw_p0_bus_probe,
	.remove		= dsp_hw_common_bus_remove,
};

int dsp_hw_p0_bus_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_BUS,
			&p0_bus_ops, sizeof(p0_bus_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
