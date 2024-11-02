// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 * Pablo v9.1 specific functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#if IS_ENABLED(CONFIG_EXYNOS_SCI)
#include <soc/samsung/exynos-sci.h>
#endif

#include "is-config.h"
#include "is-param.h"
#include "is-type.h"
#include "is-core.h"
#include "is-hw-chain.h"
#include "is-hw-settle-4nm-lpe.h"
#include "is-device-sensor.h"
#include "is-device-csi.h"
#include "is-device-ischain.h"
#include "is-hw.h"
#include "../../interface/is-interface-library.h"
#include "votf/camerapp-votf.h"
#include "is-hw-dvfs.h"

/* SYSREG register description */
static const struct is_reg sysreg_csis_regs[SYSREG_CSIS_REG_CNT] = {
	{0x0108, "MEMCLK"},
	{0x0180, "POWER_FAIL_DETECT"},
	{0x031C, "CSIS_EMA_STATUS"},
	{0x040C, "CSIS_PDP_VOTF"},
	{0x0410, "CSIS_PDP_SC_CON0"},
	{0x0414, "CSIS_PDP_SC_CON1"},
	{0x0418, "CSIS_PDP_SC_CON2"},
	{0x041C, "CSIS_PDP_SC_CON3"},
	{0x0420, "CSIS_PDP_SC_CON4"},
	{0x0424, "CSIS_PDP_SC_CON5"},
	{0x0428, "CSIS_PDP_SC_CON6"},
	{0x042C, "CSIS_PDP_SC_CON7"},
	{0x0430, "PDP_VC_CON0"},
	{0x0434, "PDP_VC_CON1"},
	{0x0438, "PDP_VC_CON2"},
	{0x043C, "PDP_VC_CON3"},
	{0x0480, "CSIS_PDP_SC_PDP0_IN_EN"},
	{0x0480, "CSIS_PDP_SC_PDP1_IN_EN"},
	{0x0480, "CSIS_PDP_SC_PDP2_IN_EN"},
	{0x0480, "CSIS_PDP_SC_PDP3_IN_EN"},
	{0x0490, "CSIS_PDP_SC_CON8"},
	{0x0494, "CSIS_PDP_BNS_MUX"},
	{0x04A0, "CSIS0_FRAME_ID_EN"},
	{0x04A4, "CSIS1_FRAME_ID_EN"},
	{0x04A8, "CSIS2_FRAME_ID_EN"},
	{0x04AC, "CSIS3_FRAME_ID_EN"},
	{0x04B0, "CSIS4_FRAME_ID_EN"},
	{0x04B4, "CSIS5_FRAME_ID_EN"},
	{0x04B8, "CSIS6_FRAME_ID_EN"},
	{0x0500, "MIPI_PHY_CON"},
};

static const struct is_field sysreg_csis_fields[SYSREG_CSIS_REG_FIELD_CNT] = {
	{"EN", 0, 1, RW, 0x1},
	{"PFD_AVDD085_MIPI_CPHY_STAT", 4, 1, RO, 0x0},
	{"PFD_AVDD085_MIPI_CPHY", 0, 1, RW, 0x0},
	{"SFR_ENABLE", 8, 1, RW, 0x0},
	{"WIDTH_DATA2REQ", 4, 2, RW, 0x3},
	{"EMA_BUSY", 0, 1, RO, 0x0},
	{"I_PDP3_VOTF_VVALID_EN", 3, 1, RW, 0x0},
	{"I_PDP2_VOTF_VVALID_EN", 2, 1, RW, 0x0},
	{"I_PDP1_VOTF_VVALID_EN", 1, 1, RW, 0x0},
	{"I_PDP0_VOTF_VVALID_EN", 0, 1, RW, 0x0},
	{"GLUEMUX_PDP_VAL", 0, 4, RW, 0x0},
	{"GLUEMUX_CSIS_DMA_OTF_SEL", 0, 6, RW, 0x0},
	{"MUX_IMG_VC_PDP", 16, 4, RW, 0x0},
	{"MUX_AF1_VC_PDP", 4, 4, RW, 0x2},
	{"MUX_AF0_VC_PDP", 0, 4, RW, 0x1},
	{"PDP_IN_CSIS6_EN", 6, 1, RW, 0x0},
	{"PDP_IN_CSIS5_EN", 5, 1, RW, 0x0},
	{"PDP_IN_CSIS4_EN", 4, 1, RW, 0x0},
	{"PDP_IN_CSIS3_EN", 3, 1, RW, 0x0},
	{"PDP_IN_CSIS2_EN", 2, 1, RW, 0x0},
	{"PDP_IN_CSIS1_EN", 1, 1, RW, 0x0},
	{"PDP_IN_CSIS0_EN", 0, 1, RW, 0x0},
	{"GLUEMUX_CSIS_BNS_SEL", 0, 2, RW, 0x0},
	{"FID_LOC_BYTE", 16, 16, RW, 0x1b},
	{"FID_LOC_LINE", 8, 2, RW, 0x0},
	{"FRAME_ID_EN_CSIS", 0, 1, RW, 0x0},
	{"MIPI_DPHY_CONFIG", 16, 1, RW, 0x0},
	{"MIPI_RESETN_DPHY_S1", 6, 1, RW, 0x0},
	{"MIPI_RESETN_DPHY_S", 5, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S4", 4, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S3", 3, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S2", 2, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S1", 1, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S", 0, 1, RW, 0x0},
};

static void __iomem *hwfc_rst;

static inline void __nocfi __is_isr_ddk(void *data, int handler_id)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid) {
		is_fpsimd_get_isr();
		intr_hw->handler(intr_hw->id, intr_hw->ctx);
		is_fpsimd_put_isr();
	} else {
		err_itfc("[ID:%d](%d)- chain(%d) empty handler!!",
			itf_hw->id, handler_id, intr_hw->chain_id);
	}
}

static inline void __is_isr_host(void *data, int handler_id)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid)
		intr_hw->handler(intr_hw->id, (void *)itf_hw->hw_ip);
	else
		err_itfc("[ID:%d](1) empty handler!!", itf_hw->id);
}

/*
 * Interrupt handler definitions
 */

/* CSTAT */
static irqreturn_t __is_isr1_cstat0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_cstat0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_cstat1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_cstat1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_cstat2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_cstat2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_cstat3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_cstat3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* BYRP */
static irqreturn_t __is_isr1_byrp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* RGBP */
static irqreturn_t __is_isr1_rgbp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_rgbp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* LME */
static irqreturn_t __is_isr1_lme(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

/* YUVP */
static irqreturn_t __is_isr1_yuvp(int irq, void *data)
{
	/* To handle host and ddk both, host isr handler is registerd as slot 5 */
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvp(int irq, void *data)
{
	/* To handle host and ddk both, host isr handler is registerd as slot 5 */
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr3_yuvp(int irq, void *data)
{
	/* To handle host and ddk both, host isr handler is registerd as slot 5 */
	__is_isr_host(data, INTR_HWIP3);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr4_yuvp(int irq, void *data)
{
	/* To handle host and ddk both, host isr handler is registerd as slot 5 */
	__is_isr_host(data, INTR_HWIP4);
	return IRQ_HANDLED;
}

/* MCFP */
static irqreturn_t __is_isr1_mcfp(int irq, void *data)
{
	/* To handle host and ddk both, host isr handler is registerd as slot 5 */
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mcfp(int irq, void *data)
{
	/* To handle host and ddk both, host isr handler is registerd as slot 5 */
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MCSC */
static irqreturn_t __is_isr1_mcs0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mcs0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/*
 * HW group related functions
 */
void __is_hw_group_init(struct is_group *group)
{
	int i;

	for (i = ENTRY_SENSOR; i < ENTRY_END; i++)
		group->subdev[i] = NULL;

	INIT_LIST_HEAD(&group->subdev_list);
}

int is_hw_group_cfg(void *group_data)
{
	int ret = 0;
	struct is_group *group;
	struct is_device_sensor *sensor;
	struct is_device_ischain *device;

	FIMC_BUG(!group_data);

	group = (struct is_group *)group_data;

#ifdef CONFIG_USE_SENSOR_GROUP
	if (group->slot == GROUP_SLOT_SENSOR) {
		sensor = group->sensor;
		if (!sensor) {
			err("device is NULL");
			BUG();
		}

		__is_hw_group_init(group);
		group->subdev[ENTRY_SENSOR] = &sensor->group_sensor.leader;
		group->subdev[ENTRY_SSVC0] = &sensor->ssvc0;
		group->subdev[ENTRY_SSVC1] = &sensor->ssvc1;
		group->subdev[ENTRY_SSVC2] = &sensor->ssvc2;
		group->subdev[ENTRY_SSVC3] = &sensor->ssvc3;

		list_add_tail(&sensor->group_sensor.leader.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc0.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc1.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc2.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc3.list, &group->subdev_list);
		return ret;
	}
#endif

	device = group->device;
	if (!device) {
		err("device is NULL");
		BUG();
	}

	group->gframe_skip = true;

	switch (group->slot) {
	case GROUP_SLOT_PAF:
		__is_hw_group_init(group);
		group->subdev[ENTRY_PAF] = &device->group_paf.leader;
		group->subdev[ENTRY_PDAF] = &device->pdaf;
		group->subdev[ENTRY_PDST] = &device->pdst;

		list_add_tail(&device->group_paf.leader.list, &group->subdev_list);
		list_add_tail(&device->pdaf.list, &group->subdev_list);
		list_add_tail(&device->pdst.list, &group->subdev_list);
		break;
	case GROUP_SLOT_3AA:
		__is_hw_group_init(group);
		group->subdev[ENTRY_3AA] = &device->group_3aa.leader;
		group->subdev[ENTRY_3AC] = &device->txc;
		group->subdev[ENTRY_3AP] = &device->txp;
		group->subdev[ENTRY_3AF] = &device->txf;
		group->subdev[ENTRY_3AG] = &device->txg;
		group->subdev[ENTRY_3AO] = &device->txo;
		group->subdev[ENTRY_3AL] = &device->txl;
		group->subdev[ENTRY_MEXC] = &device->mexc;
		group->subdev[ENTRY_ORBXC] = &device->orbxc;

		list_add_tail(&device->group_3aa.leader.list, &group->subdev_list);
		list_add_tail(&device->txc.list, &group->subdev_list);
		list_add_tail(&device->txp.list, &group->subdev_list);
		list_add_tail(&device->txf.list, &group->subdev_list);
		list_add_tail(&device->txg.list, &group->subdev_list);
		list_add_tail(&device->txo.list, &group->subdev_list);
		list_add_tail(&device->txl.list, &group->subdev_list);
		list_add_tail(&device->mexc.list, &group->subdev_list);
		list_add_tail(&device->orbxc.list, &group->subdev_list);
		break;
	case GROUP_SLOT_LME:
		__is_hw_group_init(group);
		group->subdev[ENTRY_LME] = &device->group_lme.leader;
		group->subdev[ENTRY_LMES] = &device->lmes;
		group->subdev[ENTRY_LMEC] = &device->lmec;

		list_add_tail(&device->group_lme.leader.list, &group->subdev_list);
		list_add_tail(&device->lmes.list, &group->subdev_list);
		list_add_tail(&device->lmec.list, &group->subdev_list);
		break;
	case GROUP_SLOT_BYRP:
		__is_hw_group_init(group);
		group->subdev[ENTRY_BYRP] = &device->group_byrp.leader;

		list_add_tail(&device->group_byrp.leader.list, &group->subdev_list);
		break;
	case GROUP_SLOT_RGBP:
		__is_hw_group_init(group);
		group->subdev[ENTRY_RGBP] = &device->group_rgbp.leader;

		list_add_tail(&device->group_rgbp.leader.list, &group->subdev_list);
		break;
	case GROUP_SLOT_MCS:
		__is_hw_group_init(group);
		group->subdev[ENTRY_MCS] = &device->group_mcs.leader;
		group->subdev[ENTRY_M0P] = &device->m0p;
		group->subdev[ENTRY_M1P] = &device->m1p;
		group->subdev[ENTRY_M2P] = &device->m2p;
		group->subdev[ENTRY_M3P] = &device->m3p;
		group->subdev[ENTRY_M4P] = &device->m4p;
		group->subdev[ENTRY_M5P] = &device->m5p;

		list_add_tail(&device->group_mcs.leader.list, &group->subdev_list);
		list_add_tail(&device->m0p.list, &group->subdev_list);
		list_add_tail(&device->m1p.list, &group->subdev_list);
		list_add_tail(&device->m2p.list, &group->subdev_list);
		list_add_tail(&device->m3p.list, &group->subdev_list);
		list_add_tail(&device->m4p.list, &group->subdev_list);
		list_add_tail(&device->m5p.list, &group->subdev_list);

		device->m0p.param_dma_ot = PARAM_MCS_OUTPUT0;
		device->m1p.param_dma_ot = PARAM_MCS_OUTPUT1;
		device->m2p.param_dma_ot = PARAM_MCS_OUTPUT2;
		device->m3p.param_dma_ot = PARAM_MCS_OUTPUT3;
		device->m4p.param_dma_ot = PARAM_MCS_OUTPUT4;
		device->m5p.param_dma_ot = PARAM_MCS_OUTPUT5;
		break;
	case GROUP_SLOT_YUVP:
		__is_hw_group_init(group);
		group->subdev[ENTRY_YUVP] = &device->group_yuvp.leader;
		group->subdev[ENTRY_YUVP_YUV] = &device->subdev_yuvp[0];

		list_add_tail(&device->group_yuvp.leader.list, &group->subdev_list);
		list_add_tail(&device->subdev_yuvp[0].list, &group->subdev_list);
		break;
	case GROUP_SLOT_MCFP:
		__is_hw_group_init(group);
		group->subdev[ENTRY_MCFP] = &device->group_mcfp.leader;
		group->subdev[ENTRY_MCFP_VIDEO] = &device->subdev_mcfp[0];
		group->subdev[ENTRY_MCFP_STILL] = &device->subdev_mcfp[1];

		list_add_tail(&device->group_mcfp.leader.list, &group->subdev_list);
		break;
	default:
		probe_err("group slot(%d) is invalid", group->slot);
		BUG();
		break;
	}

	/* FIXME: exist? */
	/* for hwfc: reset all REGION_IDX registers and outputs */
	hwfc_rst = ioremap(HWFC_INDEX_RESET_ADDR, SZ_4);

	return ret;
}

int is_hw_group_open(void *group_data)
{
	int ret = 0;
	u32 group_id;
	struct is_subdev *leader;
	struct is_group *group;
	struct is_device_ischain *device;

	FIMC_BUG(!group_data);

	group = group_data;
	leader = &group->leader;
	device = group->device;
	group_id = group->id;

	switch (group_id) {
#ifdef CONFIG_USE_SENSOR_GROUP
	case GROUP_ID_SS0:
	case GROUP_ID_SS1:
	case GROUP_ID_SS2:
	case GROUP_ID_SS3:
	case GROUP_ID_SS4:
	case GROUP_ID_SS5:
		leader->constraints_width = GROUP_SENSOR_MAX_WIDTH;
		leader->constraints_height = GROUP_SENSOR_MAX_HEIGHT;
		break;
#endif
	case GROUP_ID_PAF0:
	case GROUP_ID_PAF1:
	case GROUP_ID_PAF2:
	case GROUP_ID_PAF3:
		leader->constraints_width = GROUP_PDP_MAX_WIDTH;
		leader->constraints_height = GROUP_PDP_MAX_HEIGHT;
		break;
	case GROUP_ID_3AA0:
	case GROUP_ID_3AA1:
	case GROUP_ID_3AA2:
	case GROUP_ID_3AA3:
		leader->constraints_width = GROUP_3AA_MAX_WIDTH;
		leader->constraints_height = GROUP_3AA_MAX_HEIGHT;
		break;
	case GROUP_ID_BYRP:
	case GROUP_ID_RGBP:
		leader->constraints_width = GROUP_BYRP_MAX_WIDTH;
		leader->constraints_height = GROUP_BYRP_MAX_HEIGHT;
		break;
	case GROUP_ID_MCFP:
	case GROUP_ID_YPP:
	case GROUP_ID_YUVP:
	case GROUP_ID_MCS0:
	case GROUP_ID_MCS1:
		leader->constraints_width = GROUP_MCFP_MAX_WIDTH;
		leader->constraints_height = GROUP_MCFP_MAX_HEIGHT;
		break;
	case GROUP_ID_LME0:
		leader->constraints_width = GROUP_LME_MAX_WIDTH;
		leader->constraints_height = GROUP_LME_MAX_HEIGHT;
		break;
	default:
		merr("(%s) is invalid", group, group_id_name[group_id]);
		break;
	}

	return ret;
}

inline int is_hw_slot_id(int hw_id)
{
	int slot_id = -1;

	switch (hw_id) {
	case DEV_HW_PAF0:
		slot_id = 0;
		break;
	case DEV_HW_PAF1:
		slot_id = 1;
		break;
	case DEV_HW_PAF2:
		slot_id = 2;
		break;
	case DEV_HW_PAF3:
		slot_id = 3;
		break;
	case DEV_HW_3AA0:
		slot_id = 4;
		break;
	case DEV_HW_3AA1:
		slot_id = 5;
		break;
	case DEV_HW_3AA2:
		slot_id = 6;
		break;
	case DEV_HW_3AA3:
		slot_id = 7;
		break;
	case DEV_HW_LME0:
		slot_id = 8;
		break;
	case DEV_HW_ISP0:
		slot_id = 9;
		break;
	case DEV_HW_YPP:
		slot_id = 10;
		break;
	case DEV_HW_MCSC0:
		slot_id = 11;
		break;
	case DEV_HW_BYRP:
		slot_id = 12;
		break;
	case DEV_HW_RGBP:
		slot_id = 13;
		break;
	case DEV_HW_MCFP:
		slot_id = 14;
		break;
	default:
		err("Invalid hw id(%d)", hw_id);
		break;
	}

	return slot_id;
}
#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_hw_slot_id_wrap(int hw_id)
{
	return is_hw_slot_id(hw_id);
}
KUNIT_EXPORT_SYMBOL(pablo_hw_slot_id_wrap);
#endif

int is_get_hw_list(int group_id, int *hw_list)
{
	int i;
	int hw_index = 0;

	/* initialization */
	for (i = 0; i < GROUP_HW_MAX; i++)
		hw_list[i] = -1;

	switch (group_id) {
	case GROUP_ID_PAF0:
		hw_list[hw_index] = DEV_HW_PAF0; hw_index++;
		break;
	case GROUP_ID_PAF1:
		hw_list[hw_index] = DEV_HW_PAF1; hw_index++;
		break;
	case GROUP_ID_PAF2:
		hw_list[hw_index] = DEV_HW_PAF2; hw_index++;
		break;
	case GROUP_ID_PAF3:
		hw_list[hw_index] = DEV_HW_PAF3; hw_index++;
		break;
	case GROUP_ID_3AA0:
		hw_list[hw_index] = DEV_HW_3AA0; hw_index++;
		break;
	case GROUP_ID_3AA1:
		hw_list[hw_index] = DEV_HW_3AA1; hw_index++;
		break;
	case GROUP_ID_3AA2:
		hw_list[hw_index] = DEV_HW_3AA2; hw_index++;
		break;
	case GROUP_ID_3AA3:
		hw_list[hw_index] = DEV_HW_3AA3; hw_index++;
		break;
	case GROUP_ID_LME0:
		hw_list[hw_index] = DEV_HW_LME0; hw_index++;
		break;
	case GROUP_ID_ISP0:
		hw_list[hw_index] = DEV_HW_ISP0; hw_index++;
		break;
	case GROUP_ID_YPP:
	case GROUP_ID_YUVP:
		hw_list[hw_index] = DEV_HW_YPP; hw_index++;
		break;
	case GROUP_ID_MCS0:
		hw_list[hw_index] = DEV_HW_MCSC0; hw_index++;
		break;
	/* FIXME: the person in charge of each H/W */
	case GROUP_ID_BYRP:
		hw_list[hw_index] = DEV_HW_BYRP; hw_index++;
		break;
	case GROUP_ID_RGBP:
		hw_list[hw_index] = DEV_HW_RGBP; hw_index++;
		break;
	case GROUP_ID_MCFP:
		hw_list[hw_index] = DEV_HW_MCFP; hw_index++;
		break;
	case GROUP_ID_MAX:
		break;
	default:
		err("Invalid group%d(%s)", group_id, group_id_name[group_id]);
		break;
	}

	return hw_index;
}
/*
 * System registers configurations
 */
static int is_hw_get_clk_gate(struct is_hw_ip *hw_ip, int hw_id)
{
	if (!hw_ip) {
		probe_err("hw_id(%d) hw_ip(NULL)", hw_id);
		return -EINVAL;
	}

	hw_ip->clk_gate_idx = 0;
	hw_ip->clk_gate = NULL;

	return 0;
}

static inline int __is_hw_get_address(struct platform_device *pdev,
				struct is_interface_hwip *itf_hwip,
				int hw_id, char *hw_name,
				u32 resource_id, enum base_reg_index reg_index,
				bool alloc_memlog)
{
	struct resource *mem_res = NULL;

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, resource_id);
	if (!mem_res) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		return -EINVAL;
	}

	itf_hwip->hw_ip->regs_start[reg_index] = mem_res->start;
	itf_hwip->hw_ip->regs_end[reg_index] = mem_res->end;
	itf_hwip->hw_ip->regs[reg_index] =
		ioremap(mem_res->start, resource_size(mem_res));
	if (!itf_hwip->hw_ip->regs[reg_index]) {
		dev_err(&pdev->dev, "Failed to remap io region\n");
		return -EINVAL;
	}

	if (alloc_memlog)
		is_debug_memlog_alloc_dump(mem_res->start,
					resource_size(mem_res), hw_name);

	info_itfc("[ID:%2d] %s VA(0x%lx)\n", hw_id, hw_name,
		(ulong)itf_hwip->hw_ip->regs[reg_index]);

	return 0;
}

int is_hw_get_address(void *itfc_data, void *pdev_data, int hw_id)
{
	int ret = 0;
	struct platform_device *pdev = NULL;
	struct is_interface_hwip *itf_hwip = NULL;
	int idx;

	FIMC_BUG(!itfc_data);
	FIMC_BUG(!pdev_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	itf_hwip->hw_ip->dump_for_each_reg = NULL;
	itf_hwip->hw_ip->dump_reg_list_size = 0;

	switch (hw_id) {
	case DEV_HW_3AA0:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT0", IORESOURCE_CSTAT0, REG_SETA, true);
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT_CMN", IORESOURCE_CSTAT0, REG_EXT1, false);
		break;
	case DEV_HW_3AA1:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT1", IORESOURCE_CSTAT1, REG_SETA, true);
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT_CMN", IORESOURCE_CSTAT0, REG_EXT1, false);
		break;
	case DEV_HW_3AA2:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT2", IORESOURCE_CSTAT2, REG_SETA, true);
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT_CMN", IORESOURCE_CSTAT0, REG_EXT1, false);
		break;
	case DEV_HW_3AA3:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT3", IORESOURCE_CSTAT3, REG_SETA, true);
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT_CMN", IORESOURCE_CSTAT0, REG_EXT1, false);
		break;
	case DEV_HW_LME0:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "LME", IORESOURCE_LME, REG_SETA, false);

		/* FIXME: remove it */
		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x8000;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0xFFFF;
		break;
	case DEV_HW_BYRP:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "BYRP", IORESOURCE_BYRP, REG_SETA, true);
		break;
	case DEV_HW_RGBP:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "RGBP", IORESOURCE_RGBP, REG_SETA, true);
		break;
	case DEV_HW_MCFP:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "MCFP", IORESOURCE_MCFP, REG_SETA, true);
		break;
	case DEV_HW_YPP:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "YUVP", IORESOURCE_YUVP, REG_SETA, true);
		__is_hw_get_address(pdev, itf_hwip, hw_id, "DRCP", IORESOURCE_DRCP, REG_EXT1, true);
		break;
	case DEV_HW_MCSC0:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "MCSC0", IORESOURCE_MCSC, REG_SETA, true);
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	ret = is_hw_get_clk_gate(itf_hwip->hw_ip, hw_id);
	if (ret)
		dev_err(&pdev->dev, "is_hw_get_clk_gate is fail\n");

	return ret;
}

int is_hw_get_irq(void *itfc_data, void *pdev_data, int hw_id)
{
	struct is_interface_hwip *itf_hwip = NULL;
	struct platform_device *pdev = NULL;
	int ret = 0;

	FIMC_BUG(!itfc_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	switch (hw_id) {
	case DEV_HW_3AA0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 0);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa0-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 1);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa0-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 2);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa1-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 3);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa1-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 4);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa2-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 5);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa2-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA3:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 6);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa3-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 7);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa3-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_LME0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 8);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq lme0-0\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_BYRP:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 9);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp0-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 10);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp0-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_RGBP:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 11);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq rgbp0-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 12);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq rgbp0-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MCFP:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 13);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mcfp0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 14);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mcfp0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_YPP:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 15);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvp-0");
			return -EINVAL;
		}
		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 16);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvp-1");
			return -EINVAL;
		}
		itf_hwip->irq[INTR_HWIP3] = platform_get_irq(pdev, 17);
		if (itf_hwip->irq[INTR_HWIP3] < 0) {
			err("Failed to get irq drcp-0");
			return -EINVAL;
		}
		itf_hwip->irq[INTR_HWIP4] = platform_get_irq(pdev, 18);
		if (itf_hwip->irq[INTR_HWIP4] < 0) {
			err("Failed to get irq drcp-1");
			return -EINVAL;
		}
		break;
	case DEV_HW_MCSC0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 19);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mcsc0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 20);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mcsc0-1\n");
			return -EINVAL;
		}
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return ret;
}

static inline int __is_hw_request_irq(struct is_interface_hwip *itf_hwip,
	const char *name, int isr_num,
	unsigned int added_irq_flags,
	irqreturn_t (*func)(int, void *))
{
	size_t name_len = 0;
	int ret = 0;

	name_len = sizeof(itf_hwip->irq_name[isr_num]);
	snprintf(itf_hwip->irq_name[isr_num], name_len, "%s-%d", name, isr_num);

	ret = is_request_irq(itf_hwip->irq[isr_num], func,
		itf_hwip->irq_name[isr_num],
		added_irq_flags,
		itf_hwip);
	if (ret) {
		err_itfc("[HW:%s] request_irq [%d] fail", name, isr_num);
		return -EINVAL;
	}

	itf_hwip->handler[isr_num].id = isr_num;
	itf_hwip->handler[isr_num].valid = true;

	return ret;
}

static inline int __is_hw_free_irq(struct is_interface_hwip *itf_hwip, int isr_num)
{
	is_free_irq(itf_hwip->irq[isr_num], itf_hwip);

	return 0;
}

int is_hw_request_irq(void *itfc_data, int hw_id)
{
	struct is_interface_hwip *itf_hwip = NULL;
	int ret = 0;

	FIMC_BUG(!itfc_data);


	itf_hwip = (struct is_interface_hwip *)itfc_data;

	switch (hw_id) {
	case DEV_HW_3AA0:
		ret = __is_hw_request_irq(itf_hwip, "cstat0-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_cstat0);
		ret = __is_hw_request_irq(itf_hwip, "cstat0-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_cstat0);
		break;
	case DEV_HW_3AA1:
		ret = __is_hw_request_irq(itf_hwip, "cstat1-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_cstat1);
		ret = __is_hw_request_irq(itf_hwip, "cstat1-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_cstat1);
		break;
	case DEV_HW_3AA2:
		ret = __is_hw_request_irq(itf_hwip, "cstat2-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_cstat2);
		ret = __is_hw_request_irq(itf_hwip, "cstat2-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_cstat2);
		break;
	case DEV_HW_3AA3:
		ret = __is_hw_request_irq(itf_hwip, "catat3-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_cstat3);
		ret = __is_hw_request_irq(itf_hwip, "cstat3-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_cstat3);
		break;
	case DEV_HW_LME0:
		ret = __is_hw_request_irq(itf_hwip, "lme-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_lme);
		break;
	case DEV_HW_BYRP:
		ret = __is_hw_request_irq(itf_hwip, "byrp-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp);
		ret = __is_hw_request_irq(itf_hwip, "byrp-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp);
		break;
	case DEV_HW_RGBP:
		ret = __is_hw_request_irq(itf_hwip, "rgbp-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_rgbp);
		ret = __is_hw_request_irq(itf_hwip, "rgbp-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_rgbp);
		break;
	case DEV_HW_YPP:
		ret = __is_hw_request_irq(itf_hwip, "yuvp-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvp);
		ret = __is_hw_request_irq(itf_hwip, "yuvp-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvp);
		ret = __is_hw_request_irq(itf_hwip, "drcp-0", INTR_HWIP3,
				IRQF_TRIGGER_NONE, __is_isr3_yuvp);
		ret = __is_hw_request_irq(itf_hwip, "drcp-1", INTR_HWIP4,
				IRQF_TRIGGER_NONE, __is_isr4_yuvp);
		break;
	case DEV_HW_MCFP:
		ret = __is_hw_request_irq(itf_hwip, "mcfp-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mcfp);
		ret = __is_hw_request_irq(itf_hwip, "mcfp-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mcfp);
		break;
	case DEV_HW_MCSC0:
		ret = __is_hw_request_irq(itf_hwip, "mcs-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mcs0);
		ret = __is_hw_request_irq(itf_hwip, "mcs-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mcs0);
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return ret;
}

int is_hw_s_ctrl(void *itfc_data, int hw_id, enum hw_s_ctrl_id id, void *val)
{
	int ret = 0;

	switch (id) {
	case HW_S_CTRL_FULL_BYPASS:
		break;
	case HW_S_CTRL_CHAIN_IRQ:
		break;
	case HW_S_CTRL_HWFC_IDX_RESET:
		if (hw_id == IS_VIDEO_M3P_NUM) {
			struct is_video_ctx *vctx = (struct is_video_ctx *)itfc_data;
			struct is_device_ischain *device;
			unsigned long data = (unsigned long)val;

			FIMC_BUG(!vctx);
			FIMC_BUG(!GET_DEVICE(vctx));

			device = GET_DEVICE(vctx);

			/* reset if this instance is reprocessing */
			if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
				writel(data, hwfc_rst);
		}
		break;
	case HW_S_CTRL_MCSC_SET_INPUT:
		{
			unsigned long mode = (unsigned long)val;

			info_itfc("%s: mode(%lu)\n", __func__, mode);
		}
		break;
	default:
		break;
	}

	return ret;
}

int is_hw_g_ctrl(void *itfc_data, int hw_id, enum hw_g_ctrl_id id, void *val)
{
	switch (id) {
	case HW_G_CTRL_FRM_DONE_WITH_DMA:
		if (hw_id == DEV_HW_YPP)
			*(bool *)val = false;
		else
			*(bool *)val = true;
		break;
	case HW_G_CTRL_HAS_MCSC:
		*(bool *)val = true;
		break;
	case HW_G_CTRL_HAS_VRA_CH1_ONLY:
		*(bool *)val = true;
		break;
	}

	return 0;
}

int is_hw_query_cap(void *cap_data, int hw_id)
{
	int ret = 0;

	FIMC_BUG(!cap_data);

	switch (hw_id) {
	case DEV_HW_MCSC0:
		{
			struct is_hw_mcsc_cap *cap = (struct is_hw_mcsc_cap *)cap_data;

			cap->hw_ver = HW_SET_VERSION(10, 0, 0, 0);
			cap->max_output = 5;
			cap->max_djag = 1;
			cap->max_cac = 1;
			cap->max_uvsp = 2;
			cap->hwfc = MCSC_CAP_SUPPORT;
			cap->in_otf = MCSC_CAP_SUPPORT;
			cap->in_dma = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[0] = MCSC_CAP_SUPPORT;
			cap->out_dma[1] = MCSC_CAP_SUPPORT;
			cap->out_dma[2] = MCSC_CAP_SUPPORT;
			cap->out_dma[3] = MCSC_CAP_SUPPORT;
			cap->out_dma[4] = MCSC_CAP_SUPPORT;
			cap->out_dma[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[0] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[3] = MCSC_CAP_SUPPORT;
			cap->out_hwfc[4] = MCSC_CAP_SUPPORT;
			cap->out_post[0] = MCSC_CAP_SUPPORT;
			cap->out_post[1] = MCSC_CAP_SUPPORT;
			cap->out_post[2] = MCSC_CAP_SUPPORT;
			cap->out_post[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[5] = MCSC_CAP_NOT_SUPPORT;
			cap->enable_shared_output = false;
			cap->tdnr = MCSC_CAP_NOT_SUPPORT;
			cap->djag = MCSC_CAP_SUPPORT;
			cap->cac = MCSC_CAP_NOT_SUPPORT;
			cap->uvsp = MCSC_CAP_NOT_SUPPORT;
			cap->ysum = MCSC_CAP_NOT_SUPPORT;
			cap->ds_vra = MCSC_CAP_NOT_SUPPORT;
		}
		break;
	default:
		break;
	}

	return ret;
}

u32 is_hw_find_settle(u32 mipi_speed, u32 use_cphy)
{
	u32 align_mipi_speed;
	u32 find_mipi_speed;
	const u32 *settle_table;
	size_t max;
	int s, e, m;

	if (use_cphy) {
		settle_table = is_csi_settle_table_cphy;
		max = sizeof(is_csi_settle_table_cphy) / sizeof(u32);
	} else {
		settle_table = is_csi_settle_table;
		max = sizeof(is_csi_settle_table) / sizeof(u32);
	}
	align_mipi_speed = ALIGN(mipi_speed, 10);

	s = 0;
	e = max - 2;

	if (settle_table[s] < align_mipi_speed)
		return settle_table[s + 1];

	if (settle_table[e] > align_mipi_speed)
		return settle_table[e + 1];

	/* Binary search */
	while (s <= e) {
		m = ALIGN((s + e) / 2, 2);
		find_mipi_speed = settle_table[m];

		if (find_mipi_speed == align_mipi_speed)
			break;
		else if (find_mipi_speed > align_mipi_speed)
			s = m + 2;
		else
			e = m - 2;
	}

	return settle_table[m + 1];
}

unsigned int get_dma(struct is_device_sensor *device, u32 *dma_ch)
{
	*dma_ch = 0;

	return 0;
}

void is_hw_camif_init(void)
{
}

#ifdef USE_CAMIF_FIX_UP
int is_hw_camif_fix_up(struct is_device_sensor *sensor)
{
	return 0;
}
#endif

#if (IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ))
static void is_hw_s2mpu_cfg(void)
{
	void __iomem *reg;

	/* for CSIS */
	reg = ioremap(0x17120000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x17150000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x17180000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x171B0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x171E0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* for CSTAT */
	reg = ioremap(0x18560000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* for BYRP */
	reg = ioremap(0x15F50000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* for RGBP */
	reg = ioremap(0x17550000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x17590000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* for MCFP */
	reg = ioremap(0x179C0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x179F0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x17A20000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x17A50000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x17A80000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x17AB0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* for YUVP */
	reg = ioremap(0x18050000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* for DRCP */
	reg = ioremap(0x174A0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* for MCSC */
	reg = ioremap(0x15C90000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x15CC0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	reg = ioremap(0x15CF0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* for LME */
	reg = ioremap(0x177C0000, 0x4);
	writel(0, reg);
	iounmap(reg);
}
#endif

int is_hw_camif_cfg(void *sensor_data)
{
	struct is_device_sensor *sensor;
	struct is_device_csi *csi;
	struct is_fid_loc *fid_loc;
	void __iomem *sysreg_csis;
	u32 val = 0;

	FIMC_BUG(!sensor_data);

	sensor = sensor_data;
	csi = (struct is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);
	fid_loc = &sensor->cfg->fid_loc;
	sysreg_csis = ioremap(SYSREG_CSIS_BASE_ADDR, 0x1000);

	if (csi->f_id_dec) {
		if (!fid_loc->valid) {
			fid_loc->byte = 27;
			fid_loc->line = 0;
			mwarn("fid_loc is NOT properly set.\n", sensor);
		}

		val = is_hw_get_reg(sysreg_csis,
				&sysreg_csis_regs[SYSREG_R_CSIS_CSIS0_FRAME_ID_EN + csi->ch]);
		val = is_hw_set_field_value(val,
				&sysreg_csis_fields[SYSREG_F_CSIS_FID_LOC_BYTE],
				fid_loc->byte);
		val = is_hw_set_field_value(val,
				&sysreg_csis_fields[SYSREG_F_CSIS_FID_LOC_LINE],
				fid_loc->line);
		val = is_hw_set_field_value(val,
				&sysreg_csis_fields[SYSREG_F_CSIS_FRAME_ID_EN_CSIS],
				1);

		info("SYSREG_R_CSIS%d_FRAME_ID_EN:byte %d line %d\n",
				csi->ch, fid_loc->byte, fid_loc->line);
	} else {
		val = 0;
	}

	is_hw_set_reg(sysreg_csis,
		&sysreg_csis_regs[SYSREG_R_CSIS_CSIS0_FRAME_ID_EN + csi->ch],
		val);

	iounmap(sysreg_csis);

#if (IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ))
	printk("[DBG] S2MPU disable\n");
	is_hw_s2mpu_cfg();
#endif

	return 0;
}

int is_hw_camif_open(void *sensor_data)
{
	struct is_device_sensor *sensor;
	struct is_device_csi *csi;

	FIMC_BUG(!sensor_data);

	sensor = sensor_data;
	csi = (struct is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);

	if (csi->ch >= CSI_ID_MAX) {
		merr("CSI channel is invalid(%d)\n", sensor, csi->ch);
		return -EINVAL;
	}

	set_bit(CSIS_DMA_ENABLE, &csi->state);

	if (IS_ENABLED(SOC_SSVC0))
		csi->dma_subdev[CSI_VIRTUAL_CH_0] = &sensor->ssvc0;
	else
		csi->dma_subdev[CSI_VIRTUAL_CH_0] = NULL;

	if (IS_ENABLED(SOC_SSVC1))
		csi->dma_subdev[CSI_VIRTUAL_CH_1] = &sensor->ssvc1;
	else
		csi->dma_subdev[CSI_VIRTUAL_CH_1] = NULL;

	if (IS_ENABLED(SOC_SSVC2))
		csi->dma_subdev[CSI_VIRTUAL_CH_2] = &sensor->ssvc2;
	else
		csi->dma_subdev[CSI_VIRTUAL_CH_2] = NULL;

	if (IS_ENABLED(SOC_SSVC3))
		csi->dma_subdev[CSI_VIRTUAL_CH_3] = &sensor->ssvc3;
	else
		csi->dma_subdev[CSI_VIRTUAL_CH_3] = NULL;

	return 0;
}

void is_hw_ischain_qe_cfg(void)
{
	dbg_hw(2, "%s()\n", __func__);
}

int is_hw_ischain_cfg(void *ischain_data)
{
	int ret = 0;
	struct is_device_ischain *device;

	FIMC_BUG(!ischain_data);

	device = (struct is_device_ischain *)ischain_data;
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		return ret;

	return ret;
}

int is_hw_ischain_enable(struct is_device_ischain *device)
{
	int ret = 0;
	struct is_interface_hwip *itf_hwip = NULL;
	struct is_lib_support *lib = is_get_lib_support();
	int hw_slot = -1;

	/* irq affinity should be restored after S2R at gic600 */
	hw_slot = is_hw_slot_id(DEV_HW_3AA0);
	itf_hwip = &(lib->itfc->itf_ip[hw_slot]);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP3], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP4], true);

	hw_slot = is_hw_slot_id(DEV_HW_3AA1);
	itf_hwip = &(lib->itfc->itf_ip[hw_slot]);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP3], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP4], true);

	hw_slot = is_hw_slot_id(DEV_HW_3AA2);
	itf_hwip = &(lib->itfc->itf_ip[hw_slot]);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP3], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP4], true);

	hw_slot = is_hw_slot_id(DEV_HW_3AA3);
	itf_hwip = &(lib->itfc->itf_ip[hw_slot]);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP3], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP4], true);

	hw_slot = is_hw_slot_id(DEV_HW_LME0);
	itf_hwip = &(lib->itfc->itf_ip[hw_slot]);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);

	hw_slot = is_hw_slot_id(DEV_HW_BYRP);
	itf_hwip = &(lib->itfc->itf_ip[hw_slot]);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);

	hw_slot = is_hw_slot_id(DEV_HW_RGBP);
	itf_hwip = &(lib->itfc->itf_ip[hw_slot]);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);

	hw_slot = is_hw_slot_id(DEV_HW_YPP);
	itf_hwip = &(lib->itfc->itf_ip[hw_slot]);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP3], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP4], true);

	hw_slot = is_hw_slot_id(DEV_HW_MCFP);
	itf_hwip = &(lib->itfc->itf_ip[hw_slot]);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);

	hw_slot = is_hw_slot_id(DEV_HW_MCSC0);
	itf_hwip = &(lib->itfc->itf_ip[hw_slot]);
	is_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);

	votfitf_disable_service();

	info("%s: complete\n", __func__);

	return ret;
}

int is_hw_ischain_disable(struct is_device_ischain *device)
{
	info("%s: complete\n", __func__);

	return 0;
}

#ifdef ENABLE_HWACG_CONTROL
void is_hw_csi_qchannel_enable_all(bool enable)
{

}
#endif

void is_hw_djag_adjust_out_size(struct is_device_ischain *ischain,
					u32 in_width, u32 in_height,
					u32 *out_width, u32 *out_height)
{
	struct is_global_param *g_param;
	int bratio;
	bool is_down_scale;

	if (!ischain) {
		err_hw("device is NULL");
		return;
	}

	g_param = &ischain->resourcemgr->global_param;
	is_down_scale = (*out_width < in_width) || (*out_height < in_height);
	bratio = is_sensor_g_bratio(ischain->sensor);
	if (bratio < 0) {
		err_hw("failed to get sensor_bratio");
		return;
	}

	dbg_hw(2, "%s:video_mode %d is_down_scale %d bratio %d\n", __func__,
			g_param->video_mode, is_down_scale, bratio);

	if (g_param->video_mode
		&& is_down_scale
		&& bratio >= MCSC_DJAG_ENABLE_SENSOR_BRATIO) {
		dbg_hw(2, "%s:%dx%d -> %dx%d\n", __func__,
				*out_width, *out_height, in_width, in_height);

		*out_width = in_width;
		*out_height = in_height;
	}
}

void __nocfi is_hw_interrupt_relay(struct is_group *group, void *hw_ip_data)
{
	struct is_group *child;
	struct is_hw_ip *hw_ip = (struct is_hw_ip *)hw_ip_data;

	child = group->child;
	if (child) {
		int hw_list[GROUP_HW_MAX], hw_slot;
		enum is_hardware_id hw_id;

		is_get_hw_list(child->id, hw_list);
		hw_id = hw_list[0];
		hw_slot = is_hw_slot_id(hw_id);
		if (!valid_hw_slot_id(hw_slot)) {
			serr_hw("invalid slot (%d,%d)", hw_ip, hw_id, hw_slot);
		} else {
			struct is_hardware *hardware;
			struct is_hw_ip *hw_ip_child;
			struct hwip_intr_handler *intr_handler;

			hardware = hw_ip->hardware;
			if (!hardware) {
				serr_hw("hardware is NILL", hw_ip);
			} else {
				hw_ip_child = &hardware->hw_ip[hw_slot];
				intr_handler = hw_ip_child->intr_handler[INTR_HWIP8];
				if (intr_handler && intr_handler->handler) {
					is_fpsimd_get_isr();
					intr_handler->handler(intr_handler->id, intr_handler->ctx);
					is_fpsimd_put_isr();
				}
			}
		}
	}
}

#if IS_ENABLED(CONFIG_EXYNOS_SCI)
static struct is_llc_way_num is_llc_way[IS_LLC_SN_END] = {
	/* default : VOTF 0MB, MCFP 0MB */
	[IS_LLC_SN_DEFAULT].votf = 0,
	[IS_LLC_SN_DEFAULT].mcfp = 0,
	/* FHD scenario : VOTF 0MB, MCFP 1MB */
	[IS_LLC_SN_FHD].votf = 0,
	[IS_LLC_SN_FHD].mcfp = 2,
	/* UHD scenario : VOTF 1MB, MCFP 2MB */
	[IS_LLC_SN_UHD].votf = 2,
	[IS_LLC_SN_UHD].mcfp = 4,
	/* 8K scenario : VOTF 2MB, MCFP 2.5MB */
	[IS_LLC_SN_8K].votf = 4,
	[IS_LLC_SN_8K].mcfp = 5,
	/* preview scenario : VOTF 0MB, MCFP 1.5MB */
	[IS_LLC_SN_PREVIEW].votf = 0,
	[IS_LLC_SN_PREVIEW].mcfp = 3,
};
#endif

void is_hw_configure_llc(bool on, struct is_device_ischain *device, ulong *llc_state)
{
#if IS_ENABLED(CONFIG_EXYNOS_SCI)
	struct is_dvfs_scenario_param param;
	u32 votf, mcfp, size;

	/* way 1 means alloc 512K LLC */
	if (on) {
		is_hw_dvfs_init_face_mask(device, &param);
		is_hw_dvfs_get_scenario_param(device, 0, &param);

		if (param.mode == IS_DVFS_MODE_PHOTO) {
			votf = is_llc_way[IS_LLC_SN_PREVIEW].votf;
			mcfp = is_llc_way[IS_LLC_SN_PREVIEW].mcfp;
		} else if (param.mode == IS_DVFS_MODE_VIDEO) {
			switch (param.resol) {
			case IS_DVFS_RESOL_FHD:
				votf = is_llc_way[IS_LLC_SN_FHD].votf;
				mcfp = is_llc_way[IS_LLC_SN_FHD].mcfp;
				break;
			case IS_DVFS_RESOL_UHD:
				votf = is_llc_way[IS_LLC_SN_UHD].votf;
				mcfp = is_llc_way[IS_LLC_SN_UHD].mcfp;
				break;
			case IS_DVFS_RESOL_8K:
				votf = is_llc_way[IS_LLC_SN_8K].votf;
				mcfp = is_llc_way[IS_LLC_SN_8K].mcfp;
				break;
			default:
				votf = is_llc_way[IS_LLC_SN_DEFAULT].votf;
				mcfp = is_llc_way[IS_LLC_SN_DEFAULT].mcfp;
				break;
			}
		} else {
			votf = is_llc_way[IS_LLC_SN_DEFAULT].votf;
			mcfp = is_llc_way[IS_LLC_SN_DEFAULT].mcfp;
		}

		size = is_get_debug_param(IS_DEBUG_PARAM_LLC);
		if (size) {
			votf = size / 100;
			if (votf > 16) votf = 0;
			mcfp = size % 100;
			if (mcfp > 16) mcfp = 0;
		}

		if (votf) {
			llc_region_alloc(LLC_REGION_CAM_CSIS, 1, votf);
			set_bit(LLC_REGION_CAM_CSIS, llc_state);
		}

		if (mcfp) {
			llc_region_alloc(LLC_REGION_CAM_MCFP, 1, mcfp);
			set_bit(LLC_REGION_CAM_MCFP, llc_state);
		}

		info("[LLC] alloc, VOTF:%d.%dMB, MCFP:%d.%dMB",
				votf / 2, (votf % 2) ? 5 : 0,
				mcfp / 2, (mcfp % 2) ? 5 : 0);
	} else {
		if (test_and_clear_bit(LLC_REGION_CAM_CSIS, llc_state))
			llc_region_alloc(LLC_REGION_CAM_CSIS, 0, 0);
		if (test_and_clear_bit(LLC_REGION_CAM_MCFP, llc_state))
			llc_region_alloc(LLC_REGION_CAM_MCFP, 0, 0);

		info("[LLC] release");
	}

	llc_enable(on);
#endif
}

void is_hw_configure_bts_scen(struct is_resourcemgr *resourcemgr, int scenario_id)
{
	int bts_index = 0;

	switch (scenario_id) {
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24_HF:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30_HF:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K24:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K24_HF:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K30:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K30_HF:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD120:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD240:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD480:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD120:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_SSM:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD120:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD480:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_SSM:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD120:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD120:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_HF:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_SUPERSTEADY:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_HF_SUPERSTEADY:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60_HF:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60_SUPERSTEADY:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_VIDEO_FHD30:
	case IS_DVFS_SN_PIP_DUAL_PHOTO:
	case IS_DVFS_SN_PIP_DUAL_CAPTURE:
	case IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30:
	case IS_DVFS_SN_TRIPLE_PHOTO:
	case IS_DVFS_SN_TRIPLE_VIDEO_FHD30:
	case IS_DVFS_SN_TRIPLE_VIDEO_UHD30:
	case IS_DVFS_SN_TRIPLE_VIDEO_FHD60:
	case IS_DVFS_SN_TRIPLE_VIDEO_UHD60:
	case IS_DVFS_SN_TRIPLE_CAPTURE:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD60:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60:
		bts_index = 1;
		break;
	default:
		bts_index = 0;
		break;
	}

	/* If default scenario & specific scenario selected,
	 * off specific scenario first.
	 */
	if (resourcemgr->cur_bts_scen_idx && bts_index == 0)
		is_bts_scen(resourcemgr, resourcemgr->cur_bts_scen_idx, false);

	if (bts_index && bts_index != resourcemgr->cur_bts_scen_idx)
		is_bts_scen(resourcemgr, bts_index, true);
	resourcemgr->cur_bts_scen_idx = bts_index;
}

int is_hw_get_output_slot(u32 vid)
{
	int ret = -1;

	switch (vid) {
	case IS_VIDEO_SS0_NUM:
	case IS_VIDEO_SS1_NUM:
	case IS_VIDEO_SS2_NUM:
	case IS_VIDEO_SS3_NUM:
	case IS_VIDEO_SS4_NUM:
	case IS_VIDEO_SS5_NUM:
	case IS_VIDEO_PAF0S_NUM:
	case IS_VIDEO_PAF1S_NUM:
	case IS_VIDEO_PAF2S_NUM:
	case IS_VIDEO_PAF3S_NUM:
	case IS_VIDEO_30S_NUM:
	case IS_VIDEO_31S_NUM:
	case IS_VIDEO_32S_NUM:
	case IS_VIDEO_33S_NUM:
	case IS_VIDEO_I0S_NUM:
	case IS_VIDEO_YPP_NUM:
	case IS_VIDEO_MCFP:
	case IS_VIDEO_LME:
		ret = 0;
		break;
	default:
		ret = -1;
	}

	return ret;
}

int is_hw_get_capture_slot(struct is_frame *frame, u32 **taddr, u64 **taddr_k, u32 vid)
{
	int ret = 0;

	switch(vid) {
	case IS_LVN_CSTAT0_LME_DS0:
	case IS_LVN_CSTAT1_LME_DS0:
	case IS_LVN_CSTAT2_LME_DS0:
	case IS_LVN_CSTAT3_LME_DS0:
		*taddr = frame->txldsTargetAddress[frame->cstat_ctx];
		break;
	case IS_LVN_CSTAT0_LME_DS1:
	case IS_LVN_CSTAT1_LME_DS1:
	case IS_LVN_CSTAT2_LME_DS1:
	case IS_LVN_CSTAT3_LME_DS1:
		/* This is for EVT1 */
		break;
	case IS_LVN_CSTAT0_SVHIST:
	case IS_LVN_CSTAT1_SVHIST:
	case IS_LVN_CSTAT2_SVHIST:
	case IS_LVN_CSTAT3_SVHIST:
		*taddr = frame->dva_cstat_vhist[frame->cstat_ctx];
		break;
	case IS_LVN_CSTAT0_FDPIG:
	case IS_LVN_CSTAT1_FDPIG:
	case IS_LVN_CSTAT2_FDPIG:
	case IS_LVN_CSTAT3_FDPIG:
		*taddr = frame->efdTargetAddress[frame->cstat_ctx];
		break;
	case IS_LVN_CSTAT0_DRC:
	case IS_LVN_CSTAT1_DRC:
	case IS_LVN_CSTAT2_DRC:
	case IS_LVN_CSTAT3_DRC:
		*taddr = frame->txdgrTargetAddress[frame->cstat_ctx];
		break;
	case IS_LVN_CSTAT0_CDS:
	case IS_LVN_CSTAT1_CDS:
	case IS_LVN_CSTAT2_CDS:
	case IS_LVN_CSTAT3_CDS:
		*taddr = frame->txpTargetAddress[frame->cstat_ctx];
		break;
	case IS_LVN_BYRP_BYR:
		*taddr = frame->ixcTargetAddress;
		break;
	case IS_LVN_BYRP_HDR:
		*taddr = frame->dva_byrp_hdr;
		break;
	/* RGBP */
	case IS_LVN_RGBP_HF:
		*taddr = frame->dva_rgbp_hf;
		break;
	case IS_LVN_RGBP_SF:
		*taddr = frame->dva_rgbp_sf;
		break;
	case IS_LVN_RGBP_YUV:
		*taddr = frame->dva_rgbp_yuv;
		break;
	case IS_LVN_RGBP_RGB:
		*taddr = frame->dva_rgbp_rgb;
		break;
	case IS_VIDEO_I0C_NUM:
		*taddr = frame->ixcTargetAddress;
		break;
	case IS_VIDEO_I0P_NUM:
		*taddr = frame->ixpTargetAddress;
		break;
	case IS_VIDEO_I0V_NUM:
		*taddr = frame->ixvTargetAddress;
		break;
	case IS_VIDEO_I0W_NUM:
		*taddr = frame->ixwTargetAddress;
		break;
	case IS_VIDEO_I0T_NUM:
		*taddr = frame->ixtTargetAddress;
		break;
	case IS_VIDEO_I0G_NUM:
		*taddr = frame->ixgTargetAddress;
		break;
	case IS_VIDEO_IMM_NUM:
		*taddr = frame->ixmTargetAddress;
		if (taddr_k)
			*taddr_k = frame->ixmKTargetAddress;
		break;
	case IS_VIDEO_IRG_NUM:
		*taddr = frame->ixrrgbTargetAddress;
		break;
	case IS_VIDEO_ISC_NUM:
		*taddr = frame->ixscmapTargetAddress;
		break;
	case IS_VIDEO_IDR_NUM:
		*taddr = frame->ixdgrTargetAddress;
		break;
	case IS_VIDEO_INR_NUM:
		*taddr = frame->ixnoirTargetAddress;
		break;
	case IS_VIDEO_IND_NUM:
		*taddr = frame->ixnrdsTargetAddress;
		break;
	case IS_VIDEO_IDG_NUM:
		*taddr = frame->ixdgaTargetAddress;
		break;
	case IS_VIDEO_ISH_NUM:
		*taddr = frame->ixsvhistTargetAddress;
		break;
	case IS_VIDEO_IHF_NUM:
		*taddr = frame->ixhfTargetAddress;
		break;
	case IS_VIDEO_INW_NUM:
		*taddr = frame->ixnoiTargetAddress;
		break;
	case IS_VIDEO_INRW_NUM:
		*taddr = frame->ixnoirwTargetAddress;
		break;
	case IS_VIDEO_IRGW_NUM:
		*taddr = frame->ixwrgbTargetAddress;
		break;
	case IS_VIDEO_INB_NUM:
		*taddr = frame->ixbnrTargetAddress;
		break;
	/* YUVPP */
	case IS_LVN_YUVP_YUV:
		*taddr = frame->ypnrdsTargetAddress;
		break;
	case IS_LVN_YUVP_DRC:
		*taddr = frame->ypdgaTargetAddress;
		break;
	case IS_LVN_YUVP_SVHIST:
		*taddr = frame->ypsvhistTargetAddress;
		break;
	case IS_LVN_YUVP_SEG:
		*taddr = frame->ypnoiTargetAddress;
		break;
	/* MCSC */
	case IS_LVN_MCSC_P0:
		*taddr = frame->sc0TargetAddress;
		break;
	case IS_LVN_MCSC_P1:
		*taddr = frame->sc1TargetAddress;
		break;
	case IS_LVN_MCSC_P2:
		*taddr = frame->sc2TargetAddress;
		break;
	case IS_LVN_MCSC_P3:
		*taddr = frame->sc3TargetAddress;
		break;
	case IS_LVN_MCSC_P4:
		*taddr = frame->sc4TargetAddress;
		break;
	case IS_LVN_MCSC_P5:
		*taddr = frame->sc5TargetAddress;
		break;
	/* LME */
	case IS_LVN_LME_PREV:
		*taddr = frame->lmesTargetAddress;
		break;
	case IS_LVN_LME_PURE:
		*taddr = frame->lmecTargetAddress;
		if (taddr_k)
			*taddr_k = frame->lmecKTargetAddress;
		break;
	case IS_LVN_LME_PROCESSED:
		/* No DMA out */
		if (taddr_k)
			*taddr_k = frame->lmemKTargetAddress;
		break;
	/* MCFP */
	case IS_LVN_MCFP_PREV_YUV:
		*taddr = frame->dva_mcfp_prev_yuv;
		break;
	case IS_LVN_MCFP_PREV_W:
		*taddr = frame->dva_mcfp_prev_wgt;
		break;
	case IS_LVN_MCFP_DRC:
		*taddr = frame->dva_mcfp_cur_drc;
		break;
	case IS_LVN_MCFP_DP:
		*taddr = frame->dva_mcfp_prev_drc;
		break;
	case IS_LVN_MCFP_MV:
		*taddr = frame->dva_mcfp_motion;
		break;
	case IS_LVN_MCFP_SF:
		*taddr = frame->dva_mcfp_sat_flag;
		break;
	case IS_LVN_MCFP_W:
		*taddr = frame->dva_mcfp_wgt;
		break;
	case IS_LVN_MCFP_YUV:
		*taddr = frame->dva_mcfp_yuv;
		break;
	default:
		err_hw("Unsupported vid(%d)", vid);
		ret = -EINVAL;
	}

	/* Clear subdev frame's target address before set */
	if (taddr && *taddr)
		memset(*taddr, 0x0, sizeof(typeof(**taddr)) * IS_MAX_PLANES);
	if (taddr_k && *taddr_k)
		memset(*taddr_k, 0x0, sizeof(typeof(**taddr_k)) * IS_MAX_PLANES);

	return ret;
}

void * is_get_dma_blk(int type)
{
	struct is_lib_support *lib = is_get_lib_support();
	struct lib_mem_block * mblk = NULL;

	switch (type) {
	case ID_DMA_3AAISP:
		mblk = &lib->mb_dma_taaisp;
		break;
	case ID_DMA_MEDRC:
		mblk = &lib->mb_dma_medrc;
		break;
	case ID_DMA_ORBMCH:
		mblk = &lib->mb_dma_orbmch;
		break;
	case ID_DMA_TNR:
		mblk = &lib->mb_dma_tnr;
		break;
	case ID_DMA_CLAHE:
		mblk = &lib->mb_dma_clahe;
		break;
	default:
		err_hw("Invalid DMA type: %d\n", type);
		return NULL;
	}

	return (void *)mblk;
}

int is_get_dma_id(u32 vid)
{
	return -EINVAL;
}

int is_hw_check_changed_chain_id(struct is_hardware *hardware, u32 hw_id, u32 instance)
{
	int hw_idx;
	int mapped_hw_id = 0;

	FIMC_BUG(!hardware);

	switch (hw_id) {
	case DEV_HW_PAF0:
	case DEV_HW_PAF1:
	case DEV_HW_PAF2:
	case DEV_HW_PAF3:
		for (hw_idx = DEV_HW_PAF0; hw_idx <= DEV_HW_PAF3; hw_idx++) {
			if (test_bit(hw_idx, &hardware->hw_map[instance]) && hw_idx != hw_id) {
				mapped_hw_id = hw_idx;
				break;
			}
		}
		break;
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
	case DEV_HW_3AA2:
	case DEV_HW_3AA3:
		for (hw_idx = DEV_HW_3AA0; hw_idx <= DEV_HW_3AA3; hw_idx++) {
			if (test_bit(hw_idx, &hardware->hw_map[instance]) && hw_idx != hw_id) {
				mapped_hw_id = hw_idx;
				break;
			}
		}
		break;
	default:
		break;
	}

	return mapped_hw_id;
}

void is_hw_fill_target_address(u32 hw_id, struct is_frame *dst, struct is_frame *src,
	bool reset)
{
	/* A previous address should not be cleared for sysmmu debugging. */
	reset = false;

	switch (hw_id) {
	case DEV_HW_PAF0:
	case DEV_HW_PAF1:
	case DEV_HW_PAF2:
		break;
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
	case DEV_HW_3AA2:
	case DEV_HW_3AA3:
		TADDR_COPY(dst, src, txpTargetAddress, reset);
		TADDR_COPY(dst, src, efdTargetAddress, reset);
		TADDR_COPY(dst, src, txdgrTargetAddress, reset);
		TADDR_COPY(dst, src, txldsTargetAddress, reset);
		TADDR_COPY(dst, src, dva_cstat_vhist, reset);
		break;
	case DEV_HW_LME0:
		TADDR_COPY(dst, src, lmesTargetAddress, reset);
		TADDR_COPY(dst, src, lmecTargetAddress, reset);
		TADDR_COPY(dst, src, lmecKTargetAddress, reset);
		TADDR_COPY(dst, src, lmemKTargetAddress, reset);
		break;
	case DEV_HW_BYRP:
		TADDR_COPY(dst, src, dva_byrp_hdr, reset);
		TADDR_COPY(dst, src, ixcTargetAddress, reset);
		break;
	case DEV_HW_RGBP:
		TADDR_COPY(dst, src, dva_rgbp_hf, reset);
		TADDR_COPY(dst, src, dva_rgbp_sf, reset);
		TADDR_COPY(dst, src, dva_rgbp_yuv, reset);
		TADDR_COPY(dst, src, dva_rgbp_rgb, reset);
		break;
	case DEV_HW_YPP:
		TADDR_COPY(dst, src, ixdgrTargetAddress, reset);
		TADDR_COPY(dst, src, ixrrgbTargetAddress, reset);
		TADDR_COPY(dst, src, ixnoirTargetAddress, reset);
		TADDR_COPY(dst, src, ixscmapTargetAddress, reset);
		TADDR_COPY(dst, src, ixnrdsTargetAddress, reset);
		TADDR_COPY(dst, src, ixdgaTargetAddress, reset);
		TADDR_COPY(dst, src, ixnrdsTargetAddress, reset);
		TADDR_COPY(dst, src, ixhfTargetAddress, reset);
		TADDR_COPY(dst, src, ixwrgbTargetAddress, reset);
		TADDR_COPY(dst, src, ixnoirwTargetAddress, reset);
		TADDR_COPY(dst, src, ixbnrTargetAddress, reset);
		TADDR_COPY(dst, src, ixnoiTargetAddress, reset);

		TADDR_COPY(dst, src, ypnrdsTargetAddress, reset);
		TADDR_COPY(dst, src, ypnoiTargetAddress, reset);
		TADDR_COPY(dst, src, ypdgaTargetAddress, reset);
		TADDR_COPY(dst, src, ypsvhistTargetAddress, reset);
		break;
	case DEV_HW_MCFP:
		TADDR_COPY(dst, src, dva_mcfp_prev_yuv, reset);
		TADDR_COPY(dst, src, dva_mcfp_prev_wgt, reset);
		TADDR_COPY(dst, src, dva_mcfp_cur_drc, reset);
		TADDR_COPY(dst, src, dva_mcfp_prev_drc, reset);
		TADDR_COPY(dst, src, dva_mcfp_motion, reset);
		TADDR_COPY(dst, src, dva_mcfp_sat_flag, reset);
		TADDR_COPY(dst, src, dva_mcfp_wgt, reset);
		TADDR_COPY(dst, src, dva_mcfp_yuv, reset);
		break;
	case DEV_HW_MCSC0:
		TADDR_COPY(dst, src, sc0TargetAddress, reset);
		TADDR_COPY(dst, src, sc1TargetAddress, reset);
		TADDR_COPY(dst, src, sc2TargetAddress, reset);
		TADDR_COPY(dst, src, sc3TargetAddress, reset);
		TADDR_COPY(dst, src, sc4TargetAddress, reset);
		TADDR_COPY(dst, src, sc5TargetAddress, reset);
		break;
	default:
		err("[%d] Invalid hw id(%d)", src->instance, hw_id);
		break;
	}
}

void is_hw_chain_probe(void *core)
{
	size_t i, size;

	size = ARRAY_SIZE(is_video_probe_fns);

	for (i = 0; i < size; i++)
		is_video_probe_fns[i](core);
}

struct is_mem *is_hw_get_iommu_mem(u32 vid)
{
	struct is_core *core = is_get_is_core();

	if (!core)
		return NULL;

	return &core->resourcemgr.mem;
}
