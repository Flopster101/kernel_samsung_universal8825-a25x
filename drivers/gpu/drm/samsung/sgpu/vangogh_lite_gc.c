/*
 *
 *  Copyright (C) 2020 - 2021 Advanced Micro Devices, Inc.  All rights reserved.
 *
 */

#include "soc15_common.h"

#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "vangogh_lite_reg.h"
#include <soc/samsung/cal-if.h>

#define PD_G3DCORE (0xB138000A)
#define PD_G3DCORE_IFPO (0xB138001B)
void vangogh_lite_gc_update_median_grain_clock_gating(struct amdgpu_device *adev,
						      bool enable)
{
	uint32_t data, def;

	/* It is disabled by HW by default */
	if (enable && (adev->cg_flags & AMD_CG_SUPPORT_GFX_MGCG)) {
		/* 1 - RLC_CGTT_MGCG_OVERRIDE */
		def = data = RREG32_SOC15(GC, 0, mmRLC_CGTT_MGCG_OVERRIDE);
		data &= ~(RLC_CGTT_MGCG_OVERRIDE__GRBM_CGTT_SCLK_OVERRIDE_MASK |
			  RLC_CGTT_MGCG_OVERRIDE__GFXIP_MGCG_OVERRIDE_MASK |
			  RLC_CGTT_MGCG_OVERRIDE__RLC_CGTT_SCLK_OVERRIDE_MASK |
			  RLC_CGTT_MGCG_OVERRIDE__ENABLE_CGTS_LEGACY_MASK);

		/* FGCG */
		data &= ~(RLC_CGTT_MGCG_OVERRIDE__GFXIP_FGCG_OVERRIDE_MASK |
			  RLC_CGTT_MGCG_OVERRIDE__GFXIP_REP_FGCG_OVERRIDE_MASK);

		if (def != data)
			WREG32_SOC15(GC, 0, mmRLC_CGTT_MGCG_OVERRIDE, data);
	} else {
		/* 1 - MGCG_OVERRIDE */
		def = data = RREG32_SOC15(GC, 0, mmRLC_CGTT_MGCG_OVERRIDE);
		data |= (RLC_CGTT_MGCG_OVERRIDE__RLC_CGTT_SCLK_OVERRIDE_MASK |
			 RLC_CGTT_MGCG_OVERRIDE__GRBM_CGTT_SCLK_OVERRIDE_MASK |
			 RLC_CGTT_MGCG_OVERRIDE__GFXIP_MGCG_OVERRIDE_MASK |
			 RLC_CGTT_MGCG_OVERRIDE__ENABLE_CGTS_LEGACY_MASK |
			 RLC_CGTT_MGCG_OVERRIDE__GFXIP_FGCG_OVERRIDE_MASK |
			 RLC_CGTT_MGCG_OVERRIDE__GFXIP_REP_FGCG_OVERRIDE_MASK);
		if (def != data)
			WREG32_SOC15(GC, 0, mmRLC_CGTT_MGCG_OVERRIDE, data);
	}
}

void vangogh_lite_gc_clock_gating_workaround(struct amdgpu_device *adev)
{
	uint32_t data, def;

	/* Synopsys: Read return data can be dropped in tcp_mem
	 * due to the clk enable going false.  This results in
	 * head of line blocking in the return data path because
	 * tcp_ofifo is waiting for the lost data.
	 *
	 * Symptoms: TCP will lock up when the LFIFO becomes full.
	 *
	 * Workaround: Set CGTT_TCPI_CLK_CTRL_0[17] to 1.  This
	 * will override the clock gating for tcp_mem so that
	 * it's clock is always on.
	 */
	if (AMDGPU_IS_MGFX0_EVT0(adev->grbm_chip_rev) & adev->cg_flags &
	    (AMD_CG_SUPPORT_GFX_CGCG | AMD_CG_SUPPORT_GFX_MGCG |
	     AMD_CG_SUPPORT_GFX_3D_CGCG)) {
		def = data = RREG32_SOC15(GC, 0, mmCGTT_TCPI_CLK_CTRL_0);
		data |= CGTT_TCPI_CLK_CTRL_0__SOFT_OVERRIDE_17_MASK;
		if (def != data)
			WREG32_SOC15(GC, 0, mmCGTT_TCPI_CLK_CTRL_0, data);
	}
}

void vangogh_lite_gc_perfcounter_cg_workaround(struct amdgpu_device *adev,
					        bool enable)
{
	uint32_t data, def;

	/* Synopsys: Four stall flops are on mgcg clock domain
	 * and are used for the respective stall perfcounter
	 * computation.  Under certain specific timing/pipeline
	 * scenarios when the clocks re off, the flops are
	 * errorneous and hence affect the perfcounter values.
	 *
	 * Symptoms: The perfcounters using the above stall
	 * signals can be wrong.  These perfcounter are not KPI
	 * but are used for debug purposes.
	 *
	 * Workaround: Driver to program
	 * CGTT_GL1C_4CYCLE_CLK_CTRL.SOFT_OVERRIDE8 to 1 to
	 * override the MGCG clock domain so that the clocks are
	 * on to get the correct perfcounter values.  Note: It is  This
	 * only suggested to override the Clocks only when used in
	 * perfcounter capture and analysis.
	 */
	if (adev->cg_flags & AMD_CG_SUPPORT_GFX_MGCG) {
		def = data = RREG32_SOC15(GC, 0, mmCGTT_GL1C_4CYCLE_CLK_CTRL);
		if (enable)
			data |= CGTT_GL1C_4CYCLE_CLK_CTRL__SOFT_OVERRIDE8_MASK;
		else
			data &= ~CGTT_GL1C_4CYCLE_CLK_CTRL__SOFT_OVERRIDE8_MASK;
		if (def != data)
			WREG32_SOC15(GC, 0, mmCGTT_GL1C_4CYCLE_CLK_CTRL, data);
	}
}

#ifdef CONFIG_DRM_SGPU_EXYNOS
void vangogh_lite_gc_set_sysregs(struct amdgpu_device *adev)
{
	void *addr = adev->pm.sysreg_mmio;

	/* Enable MGCG */
	writel(0x6, addr + 0x8);
}
#endif

void vangogh_lite_ifpo_init(struct amdgpu_device *adev)
{
	void *addr = adev->pm.pmu_mmio;

	if (!adev->ifpo)
		return;
	// RLC Safe mode entry
	WREG32_SOC15(GC, 0, mmRLC_SAFE_MODE, 0x3);
	while(RREG32_SOC15(GC, 0, mmRLC_SAFE_MODE) != 0x2);

	// Initialize PM/IFPO registers
	WREG32_SOC15(GC, 0, mmRLC_GPM_TIMER_INT_3, 0x1000); // Write(RLC_GPM_TIMER_INT_3, _val_);
	WREG32_SOC15(GC, 0, mmRLC_PG_CNTL, 0x1);			// Write(RLC_PG_CNTL, 0x1);  // POWER_GATING_ENABLE
	writel(0x2a, addr + BG3D_PWRCTL_CTL2_OFFSET); // Write(BG3D_PWRCTL_CTL2, 0x2a); // COLDVSGFXOFF|FAXI_MODE|GPUPWRREQ

	WREG32_SOC15(GC, 0, mmRLC_SAFE_MODE, 0x1);
	while(RREG32_SOC15(GC, 0, mmRLC_SAFE_MODE) != 0x0);
	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO INIT");

	atomic_set(&adev->ifpo_state, 0);
}

void vangogh_lite_ifpo_reset(struct amdgpu_device *adev)
{
	void *addr = adev->pm.pmu_mmio;

	writel(0x12, addr + BG3D_PWRCTL_CTL2_OFFSET);
}

void vangogh_lite_ifpo_power_off(struct amdgpu_device *adev)
{
	if (!adev->probe_done || !adev->ifpo || adev->in_runpm)
		return;

	if (atomic_read(&adev->ifpo_state) == 0)
		return;

	WREG32_SOC15(GC, 0, mmRLC_SRM_CNTL, 0x3);

	if (cal_pd_control(PD_G3DCORE_IFPO, 0) != 0)
		DRM_ERROR("%s fail to power down\n", __func__);
	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO POWER OFF");

	atomic_set(&adev->ifpo_state, 0);
}

void __vangogh_lite_ifpo_power_on(struct amdgpu_device *adev)
{
	void *addr = adev->pm.pmu_mmio;
	uint32_t gpu_status = 0;

	if (atomic_read(&adev->ifpo_state) == 1) {
		if (cal_pd_status(PD_G3DCORE) == 0)
			SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "PD_G3DCORE == 0, ifpo_state %d\n",
						atomic_read(&adev->ifpo_state));
		return;
	}

	if (cal_pd_status(PD_G3DCORE) == 0) {
		writel(0x28, addr + BG3D_PWRCTL_CTL2_OFFSET);

		if (cal_pd_control(PD_G3DCORE, 1) != 0)
			DRM_ERROR("[%s][%d] - failed to gpu inter frame power on\n", __func__, __LINE__);

		do {
			gpu_status =  readl(addr + BG3D_PWRCTL_STATUS_OFFSET);
		} while ((gpu_status & BG3D_PWRCTL_STATUS_GPU_READY_MASK) == 0);
		SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO POWER ON");

		WREG32_SOC15(GC, 0, mmRLC_SRM_CNTL, 0x2);
	} else {
		SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "Pass IFPO POWER ON function");
	}

	atomic_set(&adev->ifpo_state, 1);
}

void vangogh_lite_ifpo_power_on(struct amdgpu_device *adev)
{
	mutex_lock(&adev->ifpo_mutex);
	atomic_inc(&adev->in_ifpo);

	if (!adev->probe_done || !adev->ifpo || adev->in_runpm) {
		mutex_unlock(&adev->ifpo_mutex);
		return;
	}

	__vangogh_lite_ifpo_power_on(adev);
	mutex_unlock(&adev->ifpo_mutex);
}

void vangogh_lite_ifpo_power_on_nocount(struct amdgpu_device *adev)
{
	mutex_lock(&adev->ifpo_mutex);
	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "vangogh_lite_ifpo_power_on_nocount");

	if (!adev->probe_done || !adev->ifpo) {
		mutex_unlock(&adev->ifpo_mutex);
		return;
	}

	__vangogh_lite_ifpo_power_on(adev);
	mutex_unlock(&adev->ifpo_mutex);
}

void vangogh_lite_ifpo_flag_initialize(struct amdgpu_device *adev)
{
	atomic_set(&adev->ifpo_state, 0);
	atomic_set(&adev->in_ifpo, 0);
	atomic_set(&adev->ifpo_active_jobs, 0);
	adev->ifpo_runtime_control = true;
}
