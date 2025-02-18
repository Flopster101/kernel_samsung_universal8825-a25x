/*
***************************************************************************
*
*  Copyright (C) 2020 Advanced Micro Devices, Inc.  All rights reserved.
*
***************************************************************************/

#include "amdgpu.h"
#include "amdgpu_ih.h"

#include "soc15_common.h"
#include "vangogh_lite_ih.h"

#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "gc/gc_10_4_0_default.h"

static void vangogh_lite_ih_set_interrupt_funcs(struct amdgpu_device *adev);

/**
 * vangogh_lite_ih_enable_interrupts - Enable the interrupt ring buffer
 *
 * @adev: amdgpu_device pointer
 *
 * Enable the interrupt ring buffer (NAVI10).
 */
static void vangogh_lite_ih_enable_interrupts(struct amdgpu_device *adev)
{
	u32 ih_rb_cntl = RREG32_SOC15(GC, 0, mmGIH_RB_CNTL);

	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, GIH_RB_CNTL, RB_ENABLE, 1);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, GIH_RB_CNTL, ENABLE_INTR, 1);
	WREG32_SOC15(GC, 0, mmGIH_RB_CNTL, ih_rb_cntl);
	adev->irq.ih.enabled = true;
}

/**
 * vangogh_lite_ih_disable_interrupts - Disable the interrupt ring buffer
 *
 * @adev: amdgpu_device pointer
 *
 * Disable the interrupt ring buffer (NAVI10).
 */
static void vangogh_lite_ih_disable_interrupts(struct amdgpu_device *adev)
{
	u32 ih_rb_cntl = RREG32_SOC15(GC, 0, mmGIH_RB_CNTL);

	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, GIH_RB_CNTL, RB_ENABLE, 0);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, GIH_RB_CNTL, ENABLE_INTR, 0);
	WREG32_SOC15(GC, 0, mmGIH_RB_CNTL, ih_rb_cntl);
	/* set rptr, wptr to 0 */
	WREG32_SOC15(GC, 0, mmGIH_RB_RPTR, 0);
	WREG32_SOC15(GC, 0, mmGIH_RB_WPTR, 0);
	adev->irq.ih.enabled = false;
	adev->irq.ih.rptr = 0;
}

static uint32_t vangogh_lite_ih_rb_cntl(struct amdgpu_ih_ring *ih,
		uint32_t ih_rb_cntl)
{
	int rb_bufsz = order_base_2(ih->ring_size / 4);
	int wb = ih->use_write_back? 1:0;

	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, GIH_RB_CNTL,
				   WPTR_OVERFLOW_CLEAR, 1);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, GIH_RB_CNTL,
				   WPTR_OVERFLOW_ENABLE, 1);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, GIH_RB_CNTL, RB_SIZE, rb_bufsz);
	ih_rb_cntl = REG_SET_FIELD(ih_rb_cntl, GIH_RB_CNTL,
				   WPTR_WRITEBACK_ENABLE, wb);

	return ih_rb_cntl;
}

/**
 * vangogh_lite_ih_irq_init - init and enable the interrupt ring
 *
 * @adev: amdgpu_device pointer
 *
 * Allocate a ring buffer for the interrupt controller,
 * enable the RLC, disable interrupts, enable the IH
 * ring buffer and enable it (NAVI).
 * Called at device load and reume.
 * Returns 0 for success, errors for failure.
 */
static int vangogh_lite_ih_irq_init(struct amdgpu_device *adev)
{
	struct amdgpu_ih_ring *ih = &adev->irq.ih;
	int ret = 0;
	u32 ih_rb_cntl;

	/* disable irqs */
	vangogh_lite_ih_disable_interrupts(adev);

	adev->nbio.funcs->ih_control(adev);

	/* Ring Buffer base LSB. [39:8] of 40-bit address */
	WREG32_SOC15(GC, 0, mmGIH_RB_BASE, ih->gpu_addr >> 8);

	ih_rb_cntl = RREG32_SOC15(GC, 0, mmGIH_RB_CNTL);
	ih_rb_cntl = vangogh_lite_ih_rb_cntl(ih, ih_rb_cntl);

	WREG32_SOC15(GC, 0, mmGIH_RB_CNTL, ih_rb_cntl);

	if (ih->use_write_back) {
		/* Addr[31:2] of wptr write-back address */
		WREG32_SOC15(GC, 0, mmGIH_RB_WPTR_ADDR_LO,
			     lower_32_bits(ih->wptr_addr));
		/* Addr[43:32] of wptr write-back address */
		WREG32_SOC15(GC, 0, mmGIH_RB_WPTR_ADDR_HI,
			     upper_32_bits(ih->wptr_addr) & 0xFFF);
	}

	/* set rptr, wptr to 0 */
	WREG32_SOC15(GC, 0, mmGIH_RB_RPTR, 0);
	WREG32_SOC15(GC, 0, mmGIH_RB_WPTR, 0);

	/* TODO:
	 * CLIENT18 is the VMC page fault, which is not a valid client for
	 * gIH, but CLIENT27 is valid, need more debug when enabling the VMC
	 * page fault

	tmp = RREG32_SOC15(GC, 0, mmGIH_STORM_CLIENT_LIST_CNTL);
	tmp = REG_SET_FIELD(tmp, GIH_STORM_CLIENT_LIST_CNTL,
			    CLIENT18_IS_STORM_CLIENT, 1);
	WREG32_SOC15(GC, 0, mmGIH_STORM_CLIENT_LIST_CNTL, tmp);

	tmp = RREG32_SOC15(GC, 0, mmGIH_INT_FLOOD_CNTL);
	tmp = REG_SET_FIELD(tmp, GIH_INT_FLOOD_CNTL, FLOOD_CNTL_ENABLE, 1);
	WREG32_SOC15(GC, 0, mmGIH_INT_FLOOD_CNTL, tmp);
	*/

	if (adev->pdev)
		pci_set_master(adev->pdev);

	/* enable interrupts */
	vangogh_lite_ih_enable_interrupts(adev);

	return ret;
}

/**
 * vangogh_lite_ih_irq_disable - disable interrupts
 *
 * @adev: amdgpu_device pointer
 *
 * Disable interrupts on the hw (NAVI10).
 */
static void vangogh_lite_ih_irq_disable(struct amdgpu_device *adev)
{
	vangogh_lite_ih_disable_interrupts(adev);

	/* Wait and acknowledge irq */
	mdelay(1);
}

/**
 * vangogh_lite_ih_get_wptr - get the IH ring buffer wptr
 *
 * @adev: amdgpu_device pointer
 *
 * Get the IH ring buffer wptr from either the register
 * or the writeback memory buffer.  Also check for
 * ring buffer overflow and deal with it.
 * Returns the value of the wptr.
 */
static u32 vangogh_lite_ih_get_wptr(struct amdgpu_device *adev,
			      struct amdgpu_ih_ring *ih)
{
	u32 wptr, reg, tmp;

	if (ih->use_write_back)
		wptr = le32_to_cpu(*ih->wptr_cpu);
	else {
		reg = SOC15_REG_OFFSET(GC, 0, mmGIH_RB_WPTR);
		wptr = RREG32_NO_KIQ(reg);
	}

	if (!REG_GET_FIELD(wptr, GIH_RB_WPTR, RB_OVERFLOW))
		goto out;

	/* According to gIH team, if write-back is enabled, GIH_RB_WPTR
	 * should not be accessed anymore. So here we trust the overflow
	 * error reported from write back memory, and will NOT do the
	 * second check by reading register GIH_RB_WPTR
	 */

	wptr = REG_SET_FIELD(wptr, GIH_RB_WPTR, RB_OVERFLOW, 0);

	/* When a ring buffer overflow happen start parsing interrupt
	 * from the last not overwritten vector (wptr + 32). Hopefully
	 * this should allow us to catch up.
	 */
	tmp = (wptr + 32) & ih->ptr_mask;
	dev_warn(adev->dev,
		"IH RB overflow, (0x%08X, 0x%08X, 0x%08X)\n",
		 wptr, ih->rptr, tmp);
	ih->rptr = tmp;

	reg = SOC15_REG_OFFSET(GC, 0, mmGIH_RB_CNTL);
	tmp = RREG32_NO_KIQ(reg);
	tmp = REG_SET_FIELD(tmp, GIH_RB_CNTL, WPTR_OVERFLOW_CLEAR, 1);
	WREG32_NO_KIQ(reg, tmp);
out:
	return (wptr & ih->ptr_mask);
}

/**
 * vangogh_lite_ih_decode_iv - decode an interrupt vector
 *
 * @adev: amdgpu_device pointer
 *
 * Decodes the interrupt vector at the current rptr
 * position and also advance the position.
 */
static void vangogh_lite_ih_decode_iv(struct amdgpu_device *adev,
				struct amdgpu_ih_ring *ih,
				struct amdgpu_iv_entry *entry)
{
	/* wptr/rptr are in bytes! */
	u32 ring_index = ih->rptr >> 2;
	uint32_t dw[8];

	dw[0] = le32_to_cpu(ih->ring[ring_index + 0]);
	dw[1] = le32_to_cpu(ih->ring[ring_index + 1]);
	dw[2] = le32_to_cpu(ih->ring[ring_index + 2]);
	dw[3] = le32_to_cpu(ih->ring[ring_index + 3]);
	dw[4] = le32_to_cpu(ih->ring[ring_index + 4]);
	dw[5] = le32_to_cpu(ih->ring[ring_index + 5]);
	dw[6] = le32_to_cpu(ih->ring[ring_index + 6]);
	dw[7] = le32_to_cpu(ih->ring[ring_index + 7]);

	entry->client_id = dw[0] & 0xff;
	entry->src_id = (dw[0] >> 8) & 0xff;

	entry->ring_id = (dw[0] >> 16) & 0xff;
	entry->vmid = (dw[0] >> 24) & 0xf;
	/* Force set to 0 as only gfxhub is valid on vangogh_lite */
	entry->vmid_src = 0;
	entry->timestamp = dw[1] | ((u64)(dw[2] & 0xffff) << 32);
	entry->timestamp_src = dw[2] >> 31;

	entry->pasid = dw[3] & 0xffff;
	entry->pasid_src = dw[3] >> 31;
	entry->src_data[0] = dw[4];
	entry->src_data[1] = dw[5];
	entry->src_data[2] = dw[6];
	entry->src_data[3] = dw[7];

	/* wptr/rptr are in bytes! */
	ih->rptr += 32;
}

/**
 * vangogh_lite_ih_set_rptr - set the IH ring buffer rptr
 *
 * @adev: amdgpu_device pointer
 *
 * Set the IH ring buffer rptr.
 */
static void vangogh_lite_ih_set_rptr(struct amdgpu_device *adev,
			       struct amdgpu_ih_ring *ih)
{
	if (ih->use_doorbell) {
		/* XXX check if swapping is necessary on BE */
		*ih->rptr_cpu = ih->rptr;
		WDOORBELL32(ih->doorbell_index, ih->rptr);
	} else
		WREG32_SOC15(GC, 0, mmGIH_RB_RPTR, ih->rptr);
}

static int vangogh_lite_ih_early_init(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	vangogh_lite_ih_set_interrupt_funcs(adev);
	return 0;
}

static int vangogh_lite_ih_sw_init(void *handle)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;
	bool use_bus_addr;

	/* gIH uses SYSMEM_GPA directly */
	use_bus_addr = true;
	r = amdgpu_ih_ring_init(adev, &adev->irq.ih, 4 * 1024, use_bus_addr);
	if (r)
		return r;

	/* gIH doesn't support doorbell */
	adev->irq.ih.use_doorbell = false;

	/* Force to disable write-back */
	/* This is to workaround a gIH hardware issue when writeback is
	 * enabled (WPTR_WRITEBACK_ENABLE in IH_RB_CNTL). The issue is not
	 * typically occur in current Gopher, but a backup path is still
	 * required before EVT1 hardware fix.
	 * */
	adev->irq.ih.use_write_back = false;

	r = amdgpu_irq_init(adev);

	return r;
}

static int vangogh_lite_ih_sw_fini(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	amdgpu_irq_fini(adev);
	amdgpu_ih_ring_fini(adev, &adev->irq.ih);

	return 0;
}

static int vangogh_lite_ih_hw_init(void *handle)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	r = vangogh_lite_ih_irq_init(adev);
	if (r)
		return r;

	return 0;
}

static int vangogh_lite_ih_hw_fini(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	vangogh_lite_ih_irq_disable(adev);

	return 0;
}

static int vangogh_lite_ih_suspend(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	return vangogh_lite_ih_hw_fini(adev);
}

static int vangogh_lite_ih_resume(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	return vangogh_lite_ih_hw_init(adev);
}

static bool vangogh_lite_ih_is_idle(void *handle)
{
	/* todo */
	return true;
}

static int vangogh_lite_ih_wait_for_idle(void *handle)
{
	/* todo */
	return -ETIMEDOUT;
}

static int vangogh_lite_ih_soft_reset(void *handle)
{
	/* todo */
	return 0;
}

static void vangogh_lite_ih_update_clockgating_state(struct amdgpu_device *adev,
					       bool enable)
{
	uint32_t data, def, field_val;

	/* TODO, Enable it in nv_common_early_init() */
	if (adev->cg_flags & AMD_CG_SUPPORT_IH_CG) {
		def = data = RREG32_SOC15(GC, 0, mmGIH_CLK_CTRL);
		field_val = enable ? 0 : 1;
		data = REG_SET_FIELD(data, GIH_CLK_CTRL,
				     DBUS_MUX_CLK_SOFT_OVERRIDE, field_val);
		data = REG_SET_FIELD(data, GIH_CLK_CTRL,
				     DYN_CLK_SOFT_OVERRIDE, field_val);
		data = REG_SET_FIELD(data, GIH_CLK_CTRL,
				     REG_CLK_SOFT_OVERRIDE, field_val);
		if (def != data)
			WREG32_SOC15(GC, 0, mmGIH_CLK_CTRL, data);
	}
}

static int vangogh_lite_ih_set_clockgating_state(void *handle,
					   enum amd_clockgating_state state)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	vangogh_lite_ih_update_clockgating_state(adev,
				state == AMD_CG_STATE_GATE ? true : false);
	return 0;
}

static int vangogh_lite_ih_set_powergating_state(void *handle,
					   enum amd_powergating_state state)
{
	return 0;
}

static void vangogh_lite_ih_get_clockgating_state(void *handle, u32 *flags)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	if (!RREG32_SOC15(GC, 0, mmGIH_CLK_CTRL))
		*flags |= AMD_CG_SUPPORT_IH_CG;
}

static const struct amd_ip_funcs vangogh_lite_ih_ip_funcs = {
	.name = "vangogh_lite_ih",
	.early_init = vangogh_lite_ih_early_init,
	.late_init = NULL,
	.sw_init = vangogh_lite_ih_sw_init,
	.sw_fini = vangogh_lite_ih_sw_fini,
	.hw_init = vangogh_lite_ih_hw_init,
	.hw_fini = vangogh_lite_ih_hw_fini,
	.suspend = vangogh_lite_ih_suspend,
	.resume = vangogh_lite_ih_resume,
	.is_idle = vangogh_lite_ih_is_idle,
	.wait_for_idle = vangogh_lite_ih_wait_for_idle,
	.soft_reset = vangogh_lite_ih_soft_reset,
	.set_clockgating_state = vangogh_lite_ih_set_clockgating_state,
	.set_powergating_state = vangogh_lite_ih_set_powergating_state,
	.get_clockgating_state = vangogh_lite_ih_get_clockgating_state,
};

static const struct amdgpu_ih_funcs vangogh_lite_ih_funcs = {
	.get_wptr = vangogh_lite_ih_get_wptr,
	.decode_iv = vangogh_lite_ih_decode_iv,
	.set_rptr = vangogh_lite_ih_set_rptr
};

static void vangogh_lite_ih_set_interrupt_funcs(struct amdgpu_device *adev)
{
	if (adev->irq.ih_funcs == NULL)
		adev->irq.ih_funcs = &vangogh_lite_ih_funcs;
}

const struct amdgpu_ip_block_version vangogh_lite_ih_ip_block = {
	.type = AMD_IP_BLOCK_TYPE_IH,
	.major = 1,
	.minor = 0,
	.rev = 0,
	.funcs = &vangogh_lite_ih_ip_funcs,
};
