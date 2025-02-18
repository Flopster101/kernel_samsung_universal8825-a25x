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

#include "interface/is-interface-library.h"
#include "votf/camerapp-votf.h"

/* Define the test cases. */

static struct votf_dev votfdev;

static void pablo_interface_library_is_log_write_console_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(char *str))test_func)("TEST\n");

	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_file_read_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 1);
	u32 size = 0;
	void *data = NULL;

	/* TODO: Fix test faial */
	return;

#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	KUNIT_EXPECT_PTR_EQ(test, (void *)test_func, NULL);
	return;
#endif

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(const char *ppath, const char *pfname, void **pdata, u32 *size))test_func)("/data/vendor/camera/", "is_rta.bin", &data, &size);

	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_file_write_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 2);
	char *data = "KUNIT TEST DATA";

#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	KUNIT_EXPECT_PTR_EQ(test, (void *)test_func, NULL);
	return;
#endif

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(const char *pfname, void *pdata, int size))test_func)("/data/vendor/camera/kunit_out1.txt", (void *)data, 15);

	/* File write is always fail due to permission issue */
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

/* Skip 3: Assert: we cannot test */

static void pablo_interface_library_is_sema_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 4);
	struct semaphore *sema;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* sema init */
	ret = ((int (*)(void **sema, int sema_count))test_func)((void *)&sema, 1);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* sema up */
	test_func = pablo_kunit_get_os_system_func(type, 6);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(void *sema))test_func)(sema);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* sema down */
	test_func = pablo_kunit_get_os_system_func(type, 7);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(void *sema))test_func)(sema);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* sema finish */
	test_func = pablo_kunit_get_os_system_func(type, 5);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(void *sema))test_func)(sema);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_mutex_kunit_test(struct kunit *test)
{
	int ret;
	bool locked;
	int type = KUNIT_TEST_LIB_ISP;
	struct mutex *test_mutex;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 8);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* mutex init */
	ret = ((int (*)(void **mutex))test_func)((void *)&test_mutex);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* mutex lock */
	test_func = pablo_kunit_get_os_system_func(type, 10);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(void *mutex))test_func)((void *)test_mutex);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* mutex trylock */
	test_func = pablo_kunit_get_os_system_func(type, 11);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	locked = ((bool (*)(void *mutex))test_func)((void *)test_mutex);
	KUNIT_ASSERT_EQ(test, locked, (bool)false);

	/* mutex unlock */
	test_func = pablo_kunit_get_os_system_func(type, 12);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(void *mutex))test_func)((void *)test_mutex);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* mutex finish */
	test_func = pablo_kunit_get_os_system_func(type, 9);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
}

static int timer_func(void *data) {
	struct kunit *test = data;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test);

	kunit_info(test, "timer_func is called\n");

	return 0;
}

static void pablo_interface_library_is_timer_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	int id = 0;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 13);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* timer create */
	ret = ((int (*)(int timer_id, void *func, void *data))test_func)(id, timer_func, (void *)test);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* timer reset : this function is not implemented */
	test_func = pablo_kunit_get_os_system_func(type, 15);
	ret = ((int (*)(void *timer, ulong expires))test_func)(NULL, 100000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* timer query : this function is not implemented */
	test_func = pablo_kunit_get_os_system_func(type, 16);
	ret = ((int (*)(void *timer, ulong timerValue))test_func)(NULL, 100000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* timer enable */
	test_func = pablo_kunit_get_os_system_func(type, 17);
	ret = ((int (*)(int timer_id, u32 timeout_ms))test_func)(id, 5);
	KUNIT_ASSERT_EQ(test, ret, 0);

	msleep(10);

	/* timer disable */
	test_func = pablo_kunit_get_os_system_func(type, 18);
	ret = ((int (*)(int timer_id))test_func)(id);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* timer delete */
	test_func = pablo_kunit_get_os_system_func(type, 14);
	ret = ((int (*)(int timer_id))test_func)(id);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_interrupt_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	struct hwip_intr_handler info;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 19);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* interrupt handler is registered at probe time, we cannot change */
	/* register interrupt */
	info.id = 0;
	info.chain_id = ID_3AAISP_MAX;
	ret = ((int (*)(struct hwip_intr_handler info))test_func)(info);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* enable interrupt */
	test_func = pablo_kunit_get_os_system_func(type, 21);
	ret = ((int (*)(u32 interrupt_id))test_func)(info.id);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* disable interrupt */
	test_func = pablo_kunit_get_os_system_func(type, 22);
	ret = ((int (*)(u32 interrupt_id))test_func)(info.id);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* clear interrupt */
	test_func = pablo_kunit_get_os_system_func(type, 23);
	ret = ((int (*)(u32 interrupt_id))test_func)(info.id);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* unregister interrupt */
	test_func = pablo_kunit_get_os_system_func(type, 20);
	ret = ((int (*)(u32 intr_id, u32 chain_id))test_func)(info.id, info.chain_id);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_interface_library_is_svc_spin_lock_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 24);
	ulong flags = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* svc spin lock save */
	flags = ((ulong (*)(void))test_func)();

	/* svc spin unlock restore */
	test_func = pablo_kunit_get_os_system_func(type, 25);
	((void (*)(ulong flags))test_func)(flags);
}

static void pablo_interface_library_get_random_int_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 26);
	unsigned int seed1 = 0;
	unsigned int seed2 = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	seed1 = ((unsigned int (*)(void))test_func)();
	seed2 = ((unsigned int (*)(void))test_func)();

	/* how to verify random int ?? */
	/* This is not guarantee that seed1 and seed2 is different */
	KUNIT_EXPECT_NE(test, seed1, seed2);
}

static u32 kunit_task_func(void *params)
{
	struct kunit *test = params;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test);

	kunit_info(test, "kunit_task_func\n");

	return 0;
}

static void pablo_interface_library_is_add_task_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 27);
	bool ret;
	int priority = TASK_PRIORITY_1ST;

	/* TODO: Fix test faial */
	return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((bool (*)(int priority, void *func, void *param))test_func)(priority, kunit_task_func, (void *)test);

	KUNIT_EXPECT_EQ(test, ret, (bool)true);
}

static void pablo_interface_library_is_get_usec_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 28);
	uint64_t time;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(uint64_t *time))test_func)(&time);
	KUNIT_EXPECT_EQ(test, ret, 0);

}

static void pablo_interface_library_is_log_write_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 29);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(const char *str, ...))test_func)("KUNIT TEST\n");
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_dva_dma_taaisp_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 30);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 dva;
	u32 expected_dva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_taaisp, lib->minfo->pb_taaisp,
			MT_TYPE_MB_DMA_TAAISP, "DMA_TAAISP");

	kva = lib->mb_dma_taaisp.kva_base;
	expected_dva = lib->mb_dma_taaisp.dva_base + (kva - lib->mb_dma_taaisp.kva_base);

	ret = ((int (*)(ulong kva, u32 *dva))test_func)(kva, &dva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, dva, expected_dva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_kva_dma_taaisp_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 31);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	u32 dva;
	ulong kva;
	ulong expected_kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_taaisp, lib->minfo->pb_taaisp,
			MT_TYPE_MB_DMA_TAAISP, "DMA_TAAISP");

	dva = lib->mb_dma_taaisp.dva_base;
	expected_kva = lib->mb_dma_taaisp.kva_base + (dva - lib->mb_dma_taaisp.dva_base);

	ret = ((int (*)(u32 dva, ulong *kva))test_func)(dva, &kva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, kva, expected_kva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_sleep_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 32);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	((void (*)(u32 msec))test_func)(10);
}

static void pablo_interface_library_is_inv_dma_taaisp_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 33);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_taaisp.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

static void pablo_interface_library_is_alloc_dma_taaisp_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 34);
	void *kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* alloc dma taaisp */
	kva = ((void *(*)(u32 size))test_func)(1024);

	/* free dma taaisp */
	test_func = pablo_kunit_get_os_system_func(type, 35);
	((void (*)(void *kva))test_func)(kva);
}

static void pablo_interface_library_is_spin_lock_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 36);
	spinlock_t *slock;
	ulong flags;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* spin lock init */
	ret = ((int (*)(void **slock))test_func)((void *)&slock);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, slock);

	/* spin lock */
	test_func = pablo_kunit_get_os_system_func(type, 38);
	ret = ((int (*)(void *slock_lib))test_func)((void *)slock);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* spin unlock */
	test_func = pablo_kunit_get_os_system_func(type, 39);
	ret = ((int (*)(void *slock_lib))test_func)((void *)slock);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* spin lock irq */
	test_func = pablo_kunit_get_os_system_func(type, 40);
	ret = ((int (*)(void *slock_lib))test_func)((void *)slock);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* spin unlock irq */
	test_func = pablo_kunit_get_os_system_func(type, 41);
	ret = ((int (*)(void *slock_lib))test_func)((void *)slock);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* spin lock irqsave */
	test_func = pablo_kunit_get_os_system_func(type, 42);
	flags = ((ulong (*)(void *slock_lib))test_func)((void *)slock);

	/* spin unlock irqsave */
	test_func = pablo_kunit_get_os_system_func(type, 43);
	ret = ((int (*)(void *slock_lib, ulong flag))test_func)((void *)slock, flags);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* spin lock finish */
	test_func = pablo_kunit_get_os_system_func(type, 37);
	ret = ((int (*)(void *slock_lib))test_func)((void *)slock);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_usleep_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 44);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	((void (*)(ulong usec))test_func)(100);
}

/* SKIP 45: function is deprecated */

static void pablo_interface_library_get_reg_addr_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 46);
	ulong regs;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	regs = ((ulong (*)(u32 id))test_func)(0);
}

static void pablo_interface_library_is_lib_in_irq_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 47);
	bool is_in_irq;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	is_in_irq = ((bool (*)(void))test_func)();
	KUNIT_EXPECT_NE(test, is_in_irq, (bool)true);
}

static void pablo_interface_library_is_lib_flush_task_handler_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 48);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = is_init_ddk_thread();
	KUNIT_ASSERT_EQ(test, ret, 0);

	((void (*)(int priority))test_func)(TASK_PRIORITY_1ST);
}

static void pablo_interface_library_get_fd_data_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 49);
	struct nfd_info fd_data;
	struct is_core *core = is_get_is_core();
	struct is_device_ischain *ischain = NULL;
	struct is_region *is_region = NULL;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ischain = &core->ischain[0];
	ischain->is_region = kunit_kmalloc(test, sizeof(struct is_region), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ischain->is_region);

	is_region = ischain->is_region;
	is_region->fd_info.face_num = 7;

	((void (*)(u32 instance, struct nfd_info *fd_data))test_func)(0, &fd_data);
	KUNIT_EXPECT_EQ(test, fd_data.face_num, is_region->fd_info.face_num);

	kunit_kfree(test, ischain->is_region);
	ischain->is_region = NULL;
}

static void pablo_interface_library_is_get_fd_data_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 50);

#if IS_ENABLED(SOC_VRA)
	struct fd_info face_data;
	struct fd_rectangle fd_in_size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	((void (*)(u32, struct fd_info *, struct fd_rectangle *))test_func)(0,
		&face_data, &fd_in_size);
#elif IS_ENABLED(NFD_OBJECT_DETECT)
	struct nfd_info fd_data;
	struct od_info od_data;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	((void (*)(u32, struct nfd_info *fd_data, struct od_info *od_data))test_func)(0,
		&fd_data, &od_data);
#endif
}

/* Skip duplicated 51, 52, 53, 54 */

static void pablo_interface_library_is_dva_dma_orbmch_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 55);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 dva;
	u32 expected_dva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_orbmch, lib->minfo->pb_orbmch,
			MT_TYPE_MB_DMA_ORBMCH, "DMA_ORBMCH");


	kva = lib->mb_dma_orbmch.kva_base;
	expected_dva = lib->mb_dma_orbmch.dva_base + (kva - lib->mb_dma_orbmch.kva_base);

	ret = ((int (*)(ulong kva, u32 *dva))test_func)(kva, &dva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, dva, expected_dva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);

}

static void pablo_interface_library_is_kva_dma_orbmch_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 56);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	u32 dva;
	ulong kva;
	ulong expected_kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_orbmch, lib->minfo->pb_orbmch,
			MT_TYPE_MB_DMA_ORBMCH, "DMA_ORBMCH");

	dva = lib->mb_dma_orbmch.dva_base;
	expected_kva = lib->mb_dma_orbmch.kva_base + (dva - lib->mb_dma_orbmch.dva_base);

	ret = ((int (*)(u32 dva, ulong *kva))test_func)(dva, &kva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, kva, expected_kva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_inv_dma_orbmch_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 57);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_orbmch.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

static void pablo_interface_library_is_alloc_dma_orbmch_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 58);
	void *kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* alloc dma orbmch */
	kva = ((void *(*)(u32 size))test_func)(1024);

	/* free dma orbmch */
	test_func = pablo_kunit_get_os_system_func(type, 59);
	((void (*)(void *kva))test_func)(kva);
}

static void pablo_interface_library_is_dva_dma_tnr_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 60);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 dva;
	u32 expected_dva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_tnr, lib->minfo->pb_tnr,
			MT_TYPE_MB_DMA_TNR, "DMA_TNR");


	kva = lib->mb_dma_tnr.kva_base;
	expected_dva = lib->mb_dma_tnr.dva_base + (kva - lib->mb_dma_tnr.kva_base);

	ret = ((int (*)(ulong kva, u32 *dva))test_func)(kva, &dva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, dva, expected_dva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);

}

static void pablo_interface_library_is_kva_dma_tnr_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 61);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	u32 dva;
	ulong kva;
	ulong expected_kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_tnr, lib->minfo->pb_tnr,
			MT_TYPE_MB_DMA_TNR, "DMA_TNR");

	dva = lib->mb_dma_tnr.dva_base;
	expected_kva = lib->mb_dma_tnr.kva_base + (dva - lib->mb_dma_tnr.dva_base);

	ret = ((int (*)(u32 dva, ulong *kva))test_func)(dva, &kva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, kva, expected_kva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_inv_dma_tnr_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 62);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_tnr.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

static void pablo_interface_library_is_alloc_dma_tnr_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 63);
	void *kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* alloc dma tnr */
	kva = ((void *(*)(u32 size))test_func)(1024);

	/* free dma tnr */
	test_func = pablo_kunit_get_os_system_func(type, 64);
	((void (*)(void *kva))test_func)(kva);
}

static void pablo_interface_library_is_dva_dma_medrc_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 65);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 dva;
	u32 expected_dva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_medrc, 0/*lib->minfo->pb_medrc*/,
			MT_TYPE_MB_DMA_MEDRC, "DMA_MEDRC");


	kva = lib->mb_dma_medrc.kva_base;
	expected_dva = lib->mb_dma_medrc.dva_base + (kva - lib->mb_dma_medrc.kva_base);

	ret = ((int (*)(ulong kva, u32 *dva))test_func)(kva, &dva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, dva, expected_dva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);

}

static void pablo_interface_library_is_kva_dma_medrc_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 66);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	u32 dva;
	ulong kva;
	ulong expected_kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_medrc, 0/*lib->minfo->pb_medrc*/,
			MT_TYPE_MB_DMA_MEDRC, "DMA_MEDRC");

	dva = lib->mb_dma_medrc.dva_base;
	expected_kva = lib->mb_dma_medrc.kva_base + (dva - lib->mb_dma_medrc.dva_base);

	ret = ((int (*)(u32 dva, ulong *kva))test_func)(dva, &kva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, kva, expected_kva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_inv_dma_medrc_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 67);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_medrc.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

static void pablo_interface_library_is_alloc_dma_medrc_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 68);
	void *kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* alloc dma medrc */
	kva = ((void *(*)(u32 size))test_func)(1024);

	/* free dma medrc */
	test_func = pablo_kunit_get_os_system_func(type, 69);
	((void (*)(void *kva))test_func)(kva);
}

static void pablo_interface_library_is_dva_dma_clahe_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 70);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 dva;
	u32 expected_dva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_clahe, 0/*lib->minfo->pb_clahe*/,
			MT_TYPE_MB_DMA_CLAHE, "DMA_CLAHE");


	kva = lib->mb_dma_clahe.kva_base;
	expected_dva = lib->mb_dma_clahe.dva_base + (kva - lib->mb_dma_clahe.kva_base);

	ret = ((int (*)(ulong kva, u32 *dva))test_func)(kva, &dva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, dva, expected_dva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);

}

static void pablo_interface_library_is_kva_dma_clahe_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 71);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	u32 dva;
	ulong kva;
	ulong expected_kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_clahe, 0/*lib->minfo->pb_clahe*/,
			MT_TYPE_MB_DMA_CLAHE, "DMA_CLAHE");

	dva = lib->mb_dma_clahe.dva_base;
	expected_kva = lib->mb_dma_clahe.kva_base + (dva - lib->mb_dma_clahe.dva_base);

	ret = ((int (*)(u32 dva, ulong *kva))test_func)(dva, &kva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, kva, expected_kva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_inv_dma_clahe_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 72);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_clahe.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

static void pablo_interface_library_is_alloc_dma_clahe_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 73);
	void *kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* alloc dma clahe */
	kva = ((void *(*)(u32 size))test_func)(1024);

	/* free dma clahe */
	test_func = pablo_kunit_get_os_system_func(type, 74);
	((void (*)(void *kva))test_func)(kva);
}

static void pablo_interface_library_is_clean_dma_medrc_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 75);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_medrc.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

static void pablo_interface_library_is_clean_dma_tnr_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 76);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_tnr.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

static void pablo_interface_library_is_clean_dma_orbmch_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 77);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_orbmch.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

/* Skip duplicated function 78 */

static void pablo_interface_library_is_clean_dma_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 79);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_taaisp.kva_base + 0x400;
	size = 1024;

	((void (*)(int type, ulong kva, u32 size))test_func)(ID_DMA_3AAISP, kva, size);
}

static void pablo_interface_library_is_votfif_create_ring_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 80);
	int ip;
	int i;
	struct votf_dev *store_votfdev[VOTF_DEV_NUM];

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	for (ip = 0; ip < IP_MAX; ip++) {
		votfdev.votf_addr[ip] = kunit_kmalloc(test, 0x10000, 0);
	}

	for (i = 0; i < VOTF_DEV_NUM; i++) {
		store_votfdev[i] = pablo_kunit_get_votf_dev(i);
		pablo_kunit_set_votf_dev(i, &votfdev);
	}

	ret = ((int (*)(void))test_func)();
	KUNIT_EXPECT_EQ(test, ret, 1); /* TODO: 1 is no error, need to fix */

	for (i = 0; i < VOTF_DEV_NUM; i++) {
		pablo_kunit_set_votf_dev(i, store_votfdev[i]);
	}

	for (ip = 0; ip < IP_MAX; ip++) {
		kunit_kfree(test, votfdev.votf_addr[ip]);
	}
}

static void pablo_interface_library_is_votfif_destroy_ring_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 81);
	int i;
	struct votf_dev *store_votfdev[VOTF_DEV_NUM];

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	for (i = 0; i < VOTF_DEV_NUM; i++) {
		store_votfdev[i] = pablo_kunit_get_votf_dev(i);
		pablo_kunit_set_votf_dev(i, &votfdev);
	}

	ret = ((int (*)(void))test_func)();
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (i = 0; i < VOTF_DEV_NUM; i++)
		pablo_kunit_set_votf_dev(i, store_votfdev[i]);
}

static void pablo_interface_library_is_votfitf_set_service_cfg_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 82);
	struct votf_info vinfo;
	struct votf_service_cfg cfg;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(struct votf_info *vinfo, struct votf_service_cfg *cfg))test_func)(&vinfo, &cfg);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_interface_library_is_votfitf_reset_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 83);
	struct votf_info vinfo = { 0 };
	int vtype = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(struct votf_info *vinfo, int type))test_func)(&vinfo, vtype);
	/* FIXME: need to check - votf_table's use is false */
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_interface_library_is_votfitf_set_flush_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 84);
	struct votf_info vinfo;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(struct votf_info *vinfo))test_func)(&vinfo);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_interface_library_is_votfitf_set_frame_size_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 85);
	struct votf_info vinfo;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(struct votf_info *vinfo, u32 size))test_func)(&vinfo, 1024);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_interface_library_is_votfitf_set_trs_lost_cfg_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 86);
	struct votf_info vinfo;
	struct votf_lost_cfg cfg;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(struct votf_info *vinfo, struct votf_lost_cfg *cfg))test_func)(&vinfo, &cfg);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_interface_library_is_votfitf_set_token_size_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 87);
	struct votf_info vinfo;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(struct votf_info *vinfo, u32 size))test_func)(&vinfo, 1024);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_interface_library_is_votfitf_set_first_token_size_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 88);
	struct votf_info vinfo;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(struct votf_info *vinfo, u32 size))test_func)(&vinfo, 1024);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_interface_library_is_votfitf_sfr_dump_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 89);

	int ip;
	int i;
	struct votf_dev *store_votfdev[VOTF_DEV_NUM];

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	for (ip = 0; ip < IP_MAX; ip++)
		votfdev.votf_addr[ip] = kunit_kmalloc(test, 0x10000, 0);

	for (i = 0; i < VOTF_DEV_NUM; i++) {
		store_votfdev[i] = pablo_kunit_get_votf_dev(i);
		pablo_kunit_set_votf_dev(i, &votfdev);
	}

	((void (*)(void))test_func)();

	for (i = 0; i < VOTF_DEV_NUM; i++)
		pablo_kunit_set_votf_dev(i, store_votfdev[i]);

	for (ip = 0; ip < IP_MAX; ip++)
		kunit_kfree(test, votfdev.votf_addr[ip]);
}

/* SKIP 90: function is deprecated */

static void pablo_interface_library_is_get_binary_version_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 91);
	char *buf;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	((void (*)(char **buf, unsigned int type, unsigned int hint))test_func)(&buf, IS_BIN_LIBRARY, 0);
}

struct is_hw_ip hw_ip;
#if IS_ENABLED(USE_DDK_HWIP_INTERFACE)
struct is_hw_ip_ops hw_ip_ops;
static int kunit_hw_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf)
{
	struct kunit *test = conf;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hw_ip);

	KUNIT_EXPECT_EQ(test, chain_id, (u32)0);
	KUNIT_EXPECT_EQ(test, instance, (u32)1);
	KUNIT_EXPECT_EQ(test, fcount, (u32)2);

	return 0;
}

int kunit_hw_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
		u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct kunit *test = (struct kunit *)regs;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hw_ip);

	KUNIT_EXPECT_EQ(test, chain_id, (u32)0);
	KUNIT_EXPECT_EQ(test, instance, (u32)1);
	KUNIT_EXPECT_EQ(test, fcount, (u32)2);
	KUNIT_EXPECT_EQ(test, regs_size, (u32)3);

	return 0;
}
#endif

static void pablo_interface_library_is_set_config_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 92);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

#if IS_ENABLED(USE_DDK_HWIP_INTERFACE)
	hw_ip_ops.set_config = kunit_hw_set_config;
	hw_ip.ops = &hw_ip_ops;
	ret = ((int (*)(void *this, u32 chain_id, int instance, u32 fcount, void *config))test_func)(
			(void *)&hw_ip, 0, 1, 2, (void*)test);
	KUNIT_EXPECT_EQ(test, ret, 0);
#endif
}

static void pablo_interface_library_is_set_regs_array_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 93);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

#if IS_ENABLED(USE_DDK_HWIP_INTERFACE)
	hw_ip_ops.set_regs = kunit_hw_set_regs;
	hw_ip.ops = &hw_ip_ops;

	ret = ((int (*)(void *this, u32 chain_id,
		int instance, u32 fcount, struct cr_set *regs, u32 regs_size))test_func)(
			(void *)&hw_ip, 0, 1, 2, (void*)test, 3);
	KUNIT_EXPECT_EQ(test, ret, 0);
#endif
}

static void pablo_interface_library_is_dump_regs_array_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 94);
	struct cr_set regs = { 0 };
	int hw_id = DEV_HW_PAF0;
	int instance = 0;
	int fcount = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(int target_hw_id, int instance, u32 fcount, struct cr_set *regs, u32 regs_size))test_func)
		(hw_id, instance, fcount, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_dump_regs_log_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 95);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	hw_ip.ops = NULL;
	ret = ((int (*)(void *this, u32 chain_id, const char *str))test_func)(&hw_ip, 0, "kunit test");
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_dump_regs_addr_value_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 96);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
}

static void pablo_interface_library_is_debug_attr_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 97);
	char cmd[PATH_MAX] = { 0 };
	int p1 = 0, p2 = 0, p3 = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(char *cmd, int *param1, int *param2, int *param3))test_func)(cmd, &p1, &p2, &p3);
	/* TODO: need to check - attr_ddk[0] is null */
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_interface_library_is_debug_level_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 98);
	int dbg_lv = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	dbg_lv = ((int (*)(void))test_func)();
	KUNIT_EXPECT_GT(test, dbg_lv, -1);
	KUNIT_EXPECT_LT(test, dbg_lv, 10);
}

static void pablo_interface_library_is_event_write_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 99);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret  = ((int (*)(const char *str, ...))test_func)("KUNIT_TEST\n");
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_dva_dma_tnr_s_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 100);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 dva;
	u32 expected_dva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_tnr_s, lib->minfo->pb_tnr_s,
			MT_TYPE_MB_DMA_TNR_S, "DMA_TNR_S");


	kva = lib->mb_dma_tnr_s.kva_base;
	expected_dva = lib->mb_dma_tnr_s.dva_base + (kva - lib->mb_dma_tnr_s.kva_base);

	ret = ((int (*)(ulong kva, u32 *dva))test_func)(kva, &dva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, dva, expected_dva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_kva_dma_tnr_s_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 101);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	u32 dva;
	ulong kva;
	ulong expected_kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_dma_tnr_s, lib->minfo->pb_tnr_s,
			MT_TYPE_MB_DMA_TNR_S, "DMA_TNR_S");

	dva = lib->mb_dma_tnr_s.dva_base;
	expected_kva = lib->mb_dma_tnr_s.kva_base + (dva - lib->mb_dma_tnr_s.dva_base);

	ret = ((int (*)(u32 dva, ulong *kva))test_func)(dva, &kva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, kva, expected_kva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_inv_dma_tnr_s_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 102);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_tnr_s.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

static void pablo_interface_library_is_alloc_dma_tnr_s_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 103);
	void *kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* alloc dma tnr_s */
	kva = ((void *(*)(u32 size))test_func)(1024);

	/* free dma tnr_s */
	test_func = pablo_kunit_get_os_system_func(type, 104);
	((void (*)(void *kva))test_func)(kva);
}

static void pablo_interface_library_is_get_secure_scenario_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_ISP;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 105);
	bool ret;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((bool (*)(void))test_func)();
	KUNIT_EXPECT_EQ(test, ret, (bool)false);
}

/* RTA */
static void pablo_interface_library_is_add_task_rta_kunit_test(struct kunit *test)
{
	bool ret;
	int type = KUNIT_TEST_LIB_RTA;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 40);
	int priority = TASK_PRIORITY_6TH;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((bool (*)(int priority, void *func, void *param))test_func)(priority, kunit_task_func, (void *)test);

	KUNIT_EXPECT_EQ(test, ret, (bool)true);
}

static void pablo_interface_library_is_svc_spin_lock_rta_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_RTA;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 80);
	ulong flags = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* svc spin lock save rta */
	flags = ((ulong (*)(void))test_func)();

	/* svc spin unlock restore rta */
	test_func = pablo_kunit_get_os_system_func(type, 81);
	((void (*)(ulong flags))test_func)(flags);
}

static void pablo_interface_library_is_debug_level_rta_kunit_test(struct kunit *test)
{
	int ret;
	int type = KUNIT_TEST_LIB_RTA;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 92);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	ret = ((int (*)(void))test_func)();
	KUNIT_EXPECT_EQ(test, ret, 0);
}

/* VRA */
static void pablo_interface_library_is_alloc_vra_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_VRA;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 0);
	void *kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* alloc dma vra */
	kva = ((void *(*)(u32 size))test_func)(1024);

	/* free dma vra */
	test_func = pablo_kunit_get_os_system_func(type, 1);
	((void (*)(void *kva))test_func)(kva);
}

static void pablo_interface_library_is_alloc_vra_net_array_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_VRA;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 2);
	void *kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);

	/* alloc dma vra */
	kva = ((void *(*)(u32 size))test_func)(1024);

	/* free dma vra */
	test_func = pablo_kunit_get_os_system_func(type, 3);
	((void (*)(void *kva))test_func)(kva);
}

static void pablo_interface_library_is_dva_vra_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_VRA;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 4);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 dva;
	u32 expected_dva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_vra, 0/*lib->minfo->pb_vra*/,
			MT_TYPE_MB_VRA, "VRA");


	kva = lib->mb_vra.kva_base;
	expected_dva = lib->mb_vra.dva_base + (kva - lib->mb_vra.kva_base);

	ret = ((int (*)(ulong kva, u32 *dva))test_func)(kva, &dva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, dva, expected_dva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_dva_vra_net_array_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_VRA;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 5);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 dva;
	u32 expected_dva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_vra, 0/*lib->minfo->pb_vra*/,
			MT_TYPE_MB_VRA, "VRA");


	kva = lib->mb_vra.kva_base;
	expected_dva = lib->mb_vra.dva_base + (kva - lib->mb_vra.kva_base);

	ret = ((int (*)(ulong kva, u32 *dva))test_func)(kva, &dva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, dva, expected_dva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_kva_vra_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_VRA;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 6);
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_lib_support *lib = is_get_lib_support();
	u32 dva;
	ulong kva;
	ulong expected_kva;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	ret = pablo_kunit_resourcemgr_init_dynamic_mem(&core->resourcemgr);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* Initialize mblk */
	/* pb_xxx is not allocated, need to check valid */
	pablo_kunit_mblk_init(&lib->mb_vra, 0/*lib->minfo->pb_vra*/,
			MT_TYPE_MB_VRA, "VRA");

	dva = lib->mb_vra.dva_base;
	expected_kva = lib->mb_vra.kva_base + (dva - lib->mb_vra.dva_base);

	ret = ((int (*)(u32 dva, ulong *kva))test_func)(dva, &kva);
	if (TAAISP_DMA_SIZE > 0) {
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, kva, expected_kva);
	}

	ret = pablo_kunit_resourcemgr_deinit_dynamic_mem(&core->resourcemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_library_is_inv_vra_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_VRA;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 7);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_tnr_s.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

static void pablo_interface_library_is_clean_vra_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_VRA;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 8);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_tnr_s.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

static void pablo_interface_library_is_inv_vra_net_array_kunit_test(struct kunit *test)
{
	int type = KUNIT_TEST_LIB_VRA;
	os_system_func_t test_func = pablo_kunit_get_os_system_func(type, 9);
	struct is_lib_support *lib = is_get_lib_support();
	ulong kva;
	u32 size;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	kva = lib->mb_dma_tnr_s.kva_base + 0x400;
	size = 1024;

	((void (*)(ulong kva, u32 size))test_func)(kva, size);
}

static struct kunit_case pablo_interface_library_kunit_test_cases[] = {
	KUNIT_CASE(pablo_interface_library_is_log_write_console_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_file_read_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_file_write_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_sema_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_mutex_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_timer_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_interrupt_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_svc_spin_lock_kunit_test),
	KUNIT_CASE(pablo_interface_library_get_random_int_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_add_task_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_get_usec_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_log_write_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_dva_dma_taaisp_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_kva_dma_taaisp_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_sleep_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_inv_dma_taaisp_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_alloc_dma_taaisp_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_spin_lock_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_usleep_kunit_test),
	KUNIT_CASE(pablo_interface_library_get_reg_addr_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_lib_in_irq_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_lib_flush_task_handler_kunit_test),
	KUNIT_CASE(pablo_interface_library_get_fd_data_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_get_fd_data_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_dva_dma_orbmch_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_kva_dma_orbmch_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_inv_dma_orbmch_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_alloc_dma_orbmch_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_dva_dma_tnr_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_kva_dma_tnr_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_inv_dma_tnr_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_alloc_dma_tnr_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_dva_dma_medrc_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_kva_dma_medrc_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_inv_dma_medrc_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_alloc_dma_medrc_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_dva_dma_clahe_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_kva_dma_clahe_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_inv_dma_clahe_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_alloc_dma_clahe_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_clean_dma_medrc_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_clean_dma_tnr_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_clean_dma_orbmch_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_clean_dma_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_votfif_create_ring_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_votfif_destroy_ring_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_votfitf_set_service_cfg_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_votfitf_reset_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_votfitf_set_flush_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_votfitf_set_frame_size_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_votfitf_set_trs_lost_cfg_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_votfitf_set_token_size_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_votfitf_set_first_token_size_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_votfitf_sfr_dump_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_get_binary_version_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_set_config_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_set_regs_array_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_dump_regs_array_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_dump_regs_log_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_dump_regs_addr_value_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_debug_attr_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_debug_level_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_event_write_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_dva_dma_tnr_s_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_kva_dma_tnr_s_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_inv_dma_tnr_s_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_alloc_dma_tnr_s_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_get_secure_scenario_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_add_task_rta_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_svc_spin_lock_rta_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_debug_level_rta_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_alloc_vra_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_alloc_vra_net_array_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_dva_vra_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_dva_vra_net_array_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_kva_vra_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_inv_vra_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_clean_vra_kunit_test),
	KUNIT_CASE(pablo_interface_library_is_inv_vra_net_array_kunit_test),
	{},
};

struct kunit_suite pablo_interface_library_kunit_test_suite = {
	.name = "pablo-interface-library-kunit-test",
	.test_cases = pablo_interface_library_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_interface_library_kunit_test_suite);

MODULE_LICENSE("GPL");
