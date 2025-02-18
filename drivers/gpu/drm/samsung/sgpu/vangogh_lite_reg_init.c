/*
 *Copyright (C) 2019-2021 Advanced Micro Devices, Inc.
 */
#include "amdgpu.h"
#include "nv.h"

#include "soc15_common.h"
#include "soc15_hw_ip.h"

#include "vangogh_lite_ip_offset.h"
#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "vangogh_lite_reg.h"

#include <linux/delay.h>
#ifdef CONFIG_DRM_SGPU_EXYNOS
#include <soc/samsung/exynos-smc.h>
#endif

/* TODO, remove some IP's definitions */
int vangogh_lite_reg_base_init(struct amdgpu_device *adev)
{
	u32 i;

	for (i = 0 ; i < MAX_INSTANCE ; ++i) {
		adev->reg_offset[GC_HWIP][i] = (u32 *)(&GC_BASE.instance[i]);
		adev->reg_offset[NBIO_HWIP][i] =
				(u32 *)(&NBIO_BASE.instance[i]);
	}

	return 0;
}

/* TODO: GFXSW-3317 This code will be removed at silicon */
void vangogh_lite_gpu_hard_reset(struct amdgpu_device *adev)
{
#ifdef CONFIG_DRM_SGPU_EXYNOS
	uint32_t ret = 0;
	uint32_t val = 0x0;
	uint32_t addr = (uint32_t)adev->rmmio_base + RSMU_HARD_RESETB_GC_OFFSET;

	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(addr), val, 0);
	if (ret)
		pr_err("writing 0x%x to 0x%x is fail\n", val, addr);

	udelay(SMC_SAFE_WAIT_US);
	val = 0x1;

	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(addr), val, 0);
	if (ret)
		pr_err("writing 0x%x to 0x%x is fail\n", val, addr);
#else
	WREG32_SOC15(GC, 0, mmRSMU_HARD_RESETB_GC, 0x0);
	msleep(1);
	WREG32_SOC15(GC, 0, mmRSMU_HARD_RESETB_GC, 0x1);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
}

void vangogh_lite_gpu_soft_reset(struct amdgpu_device *adev)
{
	WREG32_SOC15(GC, 0, mmRLC_RLCS_GRBM_SOFT_RESET, 0x1);
}

void vangogh_lite_gpu_quiesce(struct amdgpu_device *adev)
{
	uint32_t i;
	unsigned long val = (0x1 << GL2ACEM_RST__SRT_ST__SHIFT);
	unsigned long timeout = 0;
	bool quiescent = false;

	timeout = jiffies + usecs_to_jiffies(QUIESCE_TIMEOUT_WAIT_US);

	WREG32_SOC15(GC, 0, mmGL2ACEM_RST, val);

	do {
		quiescent = true;

		for (i = 0; i < ACEM_INSTANCES_COUNT; i++) {
			WREG32_SOC15(GC, 0, mmGRBM_GFX_INDEX, i);
			val = RREG32_SOC15(GC, 0, mmGL2ACEM_RST);
			quiescent &= val & GL2ACEM_RST__SRT_CP_MASK;
			udelay(SMC_SAFE_WAIT_US);

			if (!time_before(jiffies, timeout)) {
				DRM_ERROR("failed to quiesce gpu\n");
				quiescent = true;
				break;
			}
		}
	} while (!quiescent);
}
