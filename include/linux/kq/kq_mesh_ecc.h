/* spdx-license-identifier: gpl-2.0-or-later */
/*
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license version 2 as
 * published by the free software foundation.
 *
 * KQ(Kernel Quality) MESH driver ECC implementation
 *  : jaecheol kim <jc22.kim@samsung.com>
 */

#ifndef __KQ_MESH_ECC_H__
#define __KQ_MESH_ECC_H__

/*
 * list of ecc decided by erridr.num of cpu
 */
enum {
	KQ_MESH_FUNC_ECC_SEL_0 = 0,
	KQ_MESH_FUNC_ECC_SEL_1,
	KQ_MESH_FUNC_ECC_SEL_2,

	KQ_MESH_FUNC_ECC_SEL_MAX,
};

/*
 * cpu list by supported mesh driver
 */
enum {
	KQ_MESH_CPU_TYPE_CORTEX_A55 = 0,
	KQ_MESH_CPU_TYPE_CORTEX_A76,
	KQ_MESH_CPU_TYPE_CORTEX_X1,

	KQ_MESH_CPU_TYPE_CORTEX_A510,
	KQ_MESH_CPU_TYPE_CORTEX_A710,
	KQ_MESH_CPU_TYPE_CORTEX_X2,

	KQ_MESH_CPU_TYPE_CORTEX_MAX,
};

#define KQ_MESH_CPU_TYPE_SIZE_9		(9)
#define KQ_MESH_CPU_TYPE_SIZE_10	(10)
#define KQ_MESH_CPU_TYPE_SIZE_11	(11)

/*
 * Name of ecc : each cortex cpu has it's own name
 * - cpu : l1/l2
 * - dsu : dynamicq shared unit (L3)
 * - l1 : level 1
 * - l2 : level 2
 */
struct kq_mesh_ecc_name {
	const char *name;
};

/*
 * list of ecc information of each cpu
 */
struct kq_mesh_ecc_type {
	const char *name;
	int size;
	int type;
	int maxsel;
	struct kq_mesh_ecc_name sel[KQ_MESH_FUNC_ECC_SEL_MAX];
};

/*
 * list of ecc information of each cpu
 * exynos2100:
 * - cortex-a55 : cpu, dsu
 * - cortex-a76 : cpu, dsu
 * - cortex-x1 : cpu, dsu
 * exynos2200:
 * - cortex-a510 : dsu, l1, l2
 * - cortex-a710 : dsu, cpu
 * - cortex-x2 : dsu, cpu
 */
struct kq_mesh_ecc_type kq_mesh_support_ecc_list[] = {
	{ "cortex-a55", KQ_MESH_CPU_TYPE_SIZE_10, KQ_MESH_CPU_TYPE_CORTEX_A55,
		KQ_MESH_FUNC_ECC_SEL_2, { {"cpu"}, {"dsu"}, {"none"}, }, },
	{ "cortex-a76", KQ_MESH_CPU_TYPE_SIZE_10, KQ_MESH_CPU_TYPE_CORTEX_A76,
		KQ_MESH_FUNC_ECC_SEL_2, { {"cpu"}, {"dsu"}, {"none"}, }, },
	{ "cortex-x1", KQ_MESH_CPU_TYPE_SIZE_9, KQ_MESH_CPU_TYPE_CORTEX_X1,
		KQ_MESH_FUNC_ECC_SEL_2, { {"cpu"}, {"dsu"}, {"none"}, }, },

	{ "cortex-a510", KQ_MESH_CPU_TYPE_SIZE_11, KQ_MESH_CPU_TYPE_CORTEX_A510,
		KQ_MESH_FUNC_ECC_SEL_MAX, { {"dsu"}, {"l1"}, {"l2"}, }, },
	{ "cortex-a710", KQ_MESH_CPU_TYPE_SIZE_11, KQ_MESH_CPU_TYPE_CORTEX_A710,
		KQ_MESH_FUNC_ECC_SEL_2, { {"dsu"}, {"cpu"}, {"none"}, }, },
	{ "cortex-x2", KQ_MESH_CPU_TYPE_SIZE_9, KQ_MESH_CPU_TYPE_CORTEX_X2,
		KQ_MESH_FUNC_ECC_SEL_2, { {"dsu"}, {"cpu"}, {"none"}, }, },

	{ NULL, KQ_MESH_CPU_TYPE_SIZE_11, KQ_MESH_CPU_TYPE_CORTEX_MAX,
		KQ_MESH_FUNC_ECC_SEL_MAX, { {"none"}, {"none"}, {"none"}, }, },
};

/*
 * Cluster information of system
 */
struct kq_mesh_cluster {
	/* list of ecc domains */
	struct list_head list;

	/* cluster domain information */
	char *name;
	int start;
	int end;

	struct kq_mesh_ecc_type *ecc;
};

/*
 * ofo : Sticky overflow bit, other.
 * ceco : Corrected error count, other.
 * ofr : Sticky overflow bit, repeat.
 * cecr : Corrected error count, repeat.
 */
struct kq_mesh_func_ecc_info {
	int ofo;
	int ceco;
	int ofr;
	int cecr;
};

struct kq_mesh_func_ecc_result {
	struct kq_mesh_func_ecc_info sel[KQ_MESH_FUNC_ECC_SEL_MAX];
};

#endif	//__KQ_MESH_ECC_H__
