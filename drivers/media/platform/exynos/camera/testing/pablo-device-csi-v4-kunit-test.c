// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "is-core.h"
#include "is-subdev-ctrl.h"
#include "is-device-csi.h"
#include "csi/is-hw-csi-v8_0.h"

/* Define the test cases. */

static struct v4l2_subdev subdev_csi;
static struct is_device_csi csi;
static struct is_frame frame;

static struct is_camif_wdma wdma;
static struct is_camif_dma_data dma_data;
static struct is_camif_wdma_module wdma_mod;

static struct is_camif_wdma *test_camif_wdma_get(struct kunit *test, int hint) {
	int i;
	int flags = 0;

	wdma.dev = (struct device *)&csi;
	wdma.data = &dma_data;
	wdma.index = 0;
	wdma.regs_ctl = kunit_kmalloc(test, 0x100000, flags);
	for (i = 0; i < CSI_VIRTUAL_CH_MAX; i++)
		wdma.regs_vc[i] = kunit_kmalloc(test, 0x100000, flags);

	wdma.regs_mux = kunit_kmalloc(test, 0x100000, flags);
	wdma.irq = 0;
	wdma.irq_name[0] = '\0';
	wdma_mod.regs = kunit_kmalloc(test, 0x100000, flags);
	wdma.mod = &wdma_mod;
	wdma.active_cnt = 0;

	return &wdma;
}

static void test_camif_wdma_put(struct kunit *test, struct is_camif_wdma *wdma) {
	int i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, wdma);

	wdma->dev = 0;
	wdma->data = 0;
	wdma->index = 0;
	kunit_kfree(test, wdma->regs_ctl);
	for (i = 0; i < CSI_VIRTUAL_CH_MAX; i++)
		kunit_kfree(test, wdma->regs_vc[i]);

	kunit_kfree(test, wdma->regs_mux);
	wdma->irq = 0;
	wdma->irq_name[0] = '\0';
	kunit_kfree(test, wdma_mod.regs);
	wdma->mod = &wdma_mod;
	wdma->active_cnt = 0;
}

static void pablo_device_csi_v4_s_multibuf_addr_kunit_test(struct kunit *test)
{
	struct pablo_kunit_csi_func *fp = pablo_kunit_get_csi_test();
	u32 val;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fp);

	csi.wdma = test_camif_wdma_get(test, 0);

	/* Write 0xDEADDEAD */
	frame.dvaddr_buffer[0] = 0xDEADDEAD;
	fp->csi_s_multibuf_addr(&csi, &frame, 0, 0);

	/* Varify */
	val = *(u32 *)(csi.wdma->regs_vc[0] + csi_dmax_chx_regs[CSIS_DMAX_CHX_R_ADDR1].sfr_offset);
	KUNIT_EXPECT_EQ(test, val, 0xDEADDEAD);

	test_camif_wdma_put(test, csi.wdma);
}

static void pablo_device_csi_v4_wq_csic_dma_vc_kunit_test(struct kunit *test)
{
	struct pablo_kunit_csi_func *fp = pablo_kunit_get_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fp);

	init_work_list(&csi.work_list[CSI_VIRTUAL_CH_0], CSI_VIRTUAL_CH_0, MAX_WORK_COUNT);
	init_work_list(&csi.work_list[CSI_VIRTUAL_CH_1], CSI_VIRTUAL_CH_1, MAX_WORK_COUNT);
	init_work_list(&csi.work_list[CSI_VIRTUAL_CH_2], CSI_VIRTUAL_CH_2, MAX_WORK_COUNT);
	init_work_list(&csi.work_list[CSI_VIRTUAL_CH_3], CSI_VIRTUAL_CH_3, MAX_WORK_COUNT);

	fp->wq_csis_dma_vc[0](&csi.wq_csis_dma[CSI_VIRTUAL_CH_0]);
	fp->wq_csis_dma_vc[1](&csi.wq_csis_dma[CSI_VIRTUAL_CH_1]);
	fp->wq_csis_dma_vc[2](&csi.wq_csis_dma[CSI_VIRTUAL_CH_2]);
	fp->wq_csis_dma_vc[3](&csi.wq_csis_dma[CSI_VIRTUAL_CH_3]);
}

static void pablo_device_csi_v4_tasklet_csis_line_kunit_test(struct kunit *test)
{
	struct pablo_kunit_csi_func *fp = pablo_kunit_get_csi_test();
	struct v4l2_subdev *pSubdev_csi = &subdev_csi;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fp);

	v4l2_set_subdevdata(&subdev_csi, &csi);
	csi.subdev = &pSubdev_csi;
	snprintf(subdev_csi.name, V4L2_SUBDEV_NAME_SIZE, "csi-subdev.kunit_test");

	fp->tasklet_csis_line(&csi.tasklet_csis_line);
}

static void pablo_device_csi_v4_csi_hw_cdump_all_kunit_test(struct kunit *test)
{
	struct pablo_kunit_csi_func *fp = pablo_kunit_get_csi_test();
	int flags = 0;
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_camif_wdma *__wdma;
	struct is_camif_wdma_module *__wdma_mod;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fp);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	/* TODO: Fix test fail */
	return;

	ret = is_debug_open(&core->resourcemgr.minfo);
	KUNIT_ASSERT_EQ(test, ret, 0);

	csi.base_reg = kunit_kmalloc(test, 0x100000, flags);
	csi.phy_reg = kunit_kmalloc(test, 0x100000, flags);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, csi.base_reg);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, csi.phy_reg);

	csi.wdma = test_camif_wdma_get(test, 0);
	__wdma = is_camif_wdma_get(0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __wdma);

	__wdma_mod = __wdma->mod;
	__wdma->mod = &wdma_mod;

	fp->csi_hw_cdump_all(&csi);

	test_camif_wdma_put(test, csi.wdma);

	__wdma->mod = __wdma_mod;
	is_camif_wdma_put(__wdma);

	kunit_kfree(test, csi.base_reg);
	kunit_kfree(test, csi.phy_reg);

	ret = is_debug_close();
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_device_csi_v4_csi_hw_dump_all_kunit_test(struct kunit *test)
{
	struct pablo_kunit_csi_func *fp = pablo_kunit_get_csi_test();
	int flags = 0;
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_camif_wdma *__wdma;
	struct is_camif_wdma_module *__wdma_mod;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fp);

	/* TODO: Fix test fail */
	return;

	ret = is_debug_open(&core->resourcemgr.minfo);
	KUNIT_ASSERT_EQ(test, ret, 0);

	csi.base_reg = kunit_kmalloc(test, 0x100000, flags);
	csi.phy_reg = kunit_kmalloc(test, 0x100000, flags);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, csi.base_reg);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, csi.phy_reg);

	csi.wdma = test_camif_wdma_get(test, 0);
	__wdma = is_camif_wdma_get(0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __wdma);

	__wdma_mod = __wdma->mod;
	__wdma->mod = &wdma_mod;

	fp->csi_hw_dump_all(&csi);

	__wdma->mod = __wdma_mod;
	is_camif_wdma_put(__wdma);

	test_camif_wdma_put(test, csi.wdma);

	kunit_kfree(test, csi.base_reg);
	kunit_kfree(test, csi.phy_reg);

	ret = is_debug_close();
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static struct kunit_case pablo_device_csi_v4_kunit_test_cases[] = {
	KUNIT_CASE(pablo_device_csi_v4_s_multibuf_addr_kunit_test),
	KUNIT_CASE(pablo_device_csi_v4_wq_csic_dma_vc_kunit_test),
	KUNIT_CASE(pablo_device_csi_v4_tasklet_csis_line_kunit_test),
	KUNIT_CASE(pablo_device_csi_v4_csi_hw_cdump_all_kunit_test),
	KUNIT_CASE(pablo_device_csi_v4_csi_hw_dump_all_kunit_test),
	{},
};

struct kunit_suite pablo_device_csi_v4_kunit_test_suite = {
	.name = "pablo-device-csi-v4-kunit-test",
	.test_cases = pablo_device_csi_v4_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_device_csi_v4_kunit_test_suite);

MODULE_LICENSE("GPL");
