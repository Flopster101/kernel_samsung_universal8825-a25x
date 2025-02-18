/*
 * Copyright (C) 2018-2021 Advanced Micro Devices, Inc. All rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "amdgpu.h"
#include "soc15.h"

#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "gc/gc_10_4_0_default.h"

#include "soc15_common.h"
#include "soc15_hw_ip.h"
#include "emu_mobile0_asic_init.h"
#include "emu_mobile1_asic_init.h"

#define DRM_SGPU_PROGRAM_RSMU

static void emu_mobile0_asic_init(struct amdgpu_device *adev)
{

	int i = 0;

	for (i = 0; i < ARRAY_SIZE(emu_mobile0_setting_cl78945); i++) {
		unsigned int val = emu_mobile0_setting_cl78945[i].val;
		unsigned int reg = emu_mobile0_setting_cl78945[i].reg;

		WREG32(reg, val);
		DRM_DEBUG("M0, try to set reg:0x%x to value:0x%x\n", reg,
				val);
	}
}

static void emu_mobile1_asic_init(struct amdgpu_device *adev)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(emu_mobile1_setting_cl87835); i++) {
		uint32_t reg = emu_mobile1_setting_cl87835[i].reg;
		uint32_t val = emu_mobile1_setting_cl87835[i].val;

		/* The original reg address is byte alignment */
		WREG32(reg / 4, val);
		DRM_INFO("M1 init 0x%x(dword:0x%x)=0x%x!\n", reg, reg / 4, val);
	}
}
int emu_soc_asic_init(struct amdgpu_device *adev)
{
	if (sgpu_no_hw_access != 0)
		return 0;
	else if (AMDGPU_IS_MGFX0_EVT1(adev->grbm_chip_rev))
		emu_mobile0_asic_init(adev);
	else if (AMDGPU_IS_MGFX1(adev->grbm_chip_rev))
		emu_mobile1_asic_init(adev);

#if 0
	/* Change register's default value:
	 * SQ_CONFIG: NEW_TRANS_ARB_SCHEME (bit 7)
	 */
	if (AMDGPU_IS_MGFX0(adev->grbm_chip_rev) ||
	    AMDGPU_IS_MGFX1(adev->grbm_chip_rev)) {
		uint32_t data = RREG32_SOC15(GC, 0, mmSQ_CONFIG);

		data = REG_SET_FIELD(data, SQ_CONFIG,
				NEW_TRANS_ARB_SCHEME, 1);
		WREG32(SOC15_REG_OFFSET(GC, 0, mmSQ_CONFIG), data);
	}
#endif

	return 0;
}

#ifdef DRM_SGPU_PROGRAM_RSMU
#include "rsmu/rsmu_0_0_2_offset.h"

void sgpu_mm_wreg32(void __iomem *addr, uint32_t value)
{
	if (sgpu_no_hw_access == 0)
		writel(value, addr);
}

void emu_write_rsmu_registers(struct amdgpu_device *adev)
{
	DRM_INFO("%s] Configure RSMU: register mmio base: 0x%px\n",__func__, (void*)adev->rmmio_base);
	DRM_INFO("%s] Configure RSMU: register mmio size: %u\n",__func__, (unsigned)adev->rmmio_size);

	sgpu_mm_wreg32(adev->rmmio + RSMU_AEB_LOCK_0_GC, 0xfffffff8);
	sgpu_mm_wreg32(adev->rmmio + RSMU_AEB_LOCK_1_GC, 0xffffffff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_COLD_RESETB_GC, 0x00000001);
	sgpu_mm_wreg32(adev->rmmio + RSMU_HARD_RESETB_GC, 0x00000001);
	sgpu_mm_wreg32(adev->rmmio + RSMU_CUSTOM_HARD_RESETB_GC, 0x00000007);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MASTER_TRUST_LEVEL_6_GC, 0x00ffffdb);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_DEFAULT_GC, 0x000001ff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_DEFAULT_GC, 0x000002ff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_DEFAULT_GC, 0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_0_GC, 0x0003f000);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_0_GC, 0x0003ffff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_0_GC, 0x00000101);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_0_GC, 0x00000201);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_0_GC, 0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_1_GC, 0x0003e000);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_1_GC, 0x0003efff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_1_GC, 0x00000107);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_1_GC, 0x00000207);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_1_GC, 0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_2_GC, 0x0003a000);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_2_GC, 0x0003afff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_2_GC,0x00000107);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_2_GC,0x00000207);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_2_GC,0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_3_GC, 0x0003a01c);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_3_GC, 0x0003a027);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_3_GC, 0x00000107);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_3_GC, 0x000002ff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_3_GC, 0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_START_ADDR_GROUP_4_GC, 0x0003b20c);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_END_ADDR_GROUP_4_GC, 0x0003b21b);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET0_GROUP_4_GC, 0x00000107);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MISC_MASK_SET1_GROUP_4_GC, 0x00000207);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_GROUP_4_GC, 0x00000036);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_MASTER_TRUST_LEVEL_RSMU_GC, 0x004bffff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_ACCESS_CONTROL_RSMU_GC, 0x00007fff);
	sgpu_mm_wreg32(adev->rmmio + RSMU_SEC_SLAVE_RANGE_ENABLE_GC, 0x0000001f);
}
#else
void emu_write_rsmu_registers(struct amdgpu_device *adev) {}
#endif
