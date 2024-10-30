/*
* @file vangogh_lite_hw_counter.c
* @copyright 2020 Samsung Electronics
*/

#include <linux/devfreq.h>
#include <linux/time64.h>
#include "soc15_common.h"
#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "gc/gc_10_4_0_default.h"
#include "sgpu_utilization.h"
#include "amdgpu.h"

/* @todo GFXSW - DPM callback function setting
 * Currently theses are set at gfx_v10.0 as callback function.
 * But needed to move proper points*/

uint64_t vangogh_lite_get_hw_utilization(struct amdgpu_device *adev)
{
	uint32_t ref_cnt, busy_cnt, utilization = 0;

	ref_cnt = RREG32_SOC15(GC, 0, mmRSMU_DPM_IPCLK_REF_COUNTER_GC);
	busy_cnt = RREG32_SOC15(GC, 0, mmRSMU_DPM_ACC_GC);

	if(ref_cnt != 0)
		utilization = (busy_cnt * 100) / ref_cnt;

	return utilization;
}

void vangogh_lite_reset_hw_utilization(struct amdgpu_device *adev)
{
	/* TODO: GFXSW-3042 enable reset register for DVFS hw counter */
	/* WREG32_SOC15(GC, 0, RSMU_DPM_ACC_RESET, 0x1); */
}

void vangogh_lite_hw_utilization_init(struct amdgpu_device *adev)
{
	/* qGE_GRBM_STAT_BUSY | qPA_GRBM_STAT_BUSY | qSC_GRBM_STAT_BUSY |
	 * qBCI_GRBM_STAT_BUSY | qCB_GRBM_STAT_BUSY | qDB_GRBM_STAT_BUSY |
	 * qRMI_GRBM_STAT_BUSY */
	uint32_t mask = 0x2a;

	WREG32_SOC15(GC, 0, mmRSMU_DPM_CONTROL_GC, mask);
	if (!adev->in_suspend)
		DRM_INFO("%s mask %x\n", __func__, mask);
}

int vangogh_lite_get_hw_time(struct amdgpu_device *adev,
			     uint32_t *busy, uint32_t *total)
{
	*busy = RREG32_SOC15(GC, 0, mmRSMU_DPM_ACC_GC);
	*total  = RREG32_SOC15(GC, 0, mmRSMU_DPM_IPCLK_REF_COUNTER_GC);

	return 0;
}
