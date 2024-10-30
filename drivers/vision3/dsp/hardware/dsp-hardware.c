// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#ifdef CONFIG_EXYNOS_DSP_HW_N1
#include "hardware/N1/dsp-hw-n1-system.h"
#endif
#ifdef CONFIG_EXYNOS_DSP_HW_N3
#include "hardware/N3/dsp-hw-n3-system.h"
#endif
#ifdef CONFIG_EXYNOS_DSP_HW_O1
#include "hardware/O1/dsp-hw-o1-system.h"
#endif
#ifdef CONFIG_EXYNOS_DSP_HW_O3
#include "hardware/O3/dsp-hw-o3-system.h"
#endif
#ifdef CONFIG_EXYNOS_DSP_HW_P0
#include "hardware/P0/dsp-hw-p0-system.h"
#endif
#include "dsp-hw-common-init.h"
#include "dsp-hardware.h"

static int (*register_ops_list[])(void) = {
#ifdef CONFIG_EXYNOS_DSP_HW_N1
	dsp_hw_n1_system_register_ops,
#endif
#ifdef CONFIG_EXYNOS_DSP_HW_N3
	dsp_hw_n3_system_register_ops,
#endif
#ifdef CONFIG_EXYNOS_DSP_HW_O1
	dsp_hw_o1_system_register_ops,
#endif
#ifdef CONFIG_EXYNOS_DSP_HW_O3
	dsp_hw_o3_system_register_ops,
#endif
#ifdef CONFIG_EXYNOS_DSP_HW_P0
	dsp_hw_p0_system_register_ops,
#endif
};

int dsp_hardware_set_ops(unsigned int dev_id, void *data)
{
	dsp_check();
	return dsp_hw_common_set_ops(dev_id, data);
}

int dsp_hardware_register_ops(void)
{
	int ret;
	size_t idx;
	bool failure = false;

	dsp_enter();
	ret = dsp_hw_common_init();
	if (ret)
		goto p_err;

	for (idx = 0; idx < ARRAY_SIZE(register_ops_list); ++idx) {
		ret = register_ops_list[idx]();
		if (ret) {
			failure = true;
			dsp_err("Failed to register ops[%zu](%d)\n", idx, ret);
		}
	}
	if (failure) {
		ret = -EINVAL;
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

