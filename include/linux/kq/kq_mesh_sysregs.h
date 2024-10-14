/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 */

#ifndef __KQ_MESH_SYSREGS_H__
#define __KQ_MESH_SYSREGS_H__

#ifndef ARM_CPU_PART_CORTEX_A55
#define ARM_CPU_PART_CORTEX_A55         0xD05
#endif

#ifndef ARM_CPU_PART_CORTEX_A78
#define ARM_CPU_PART_CORTEX_A78         0xD41
//#define ARM_CPU_PART_HERCULES
#endif

#ifndef ARM_CPU_PART_CORTEX_X1
#define ARM_CPU_PART_CORTEX_X1          0xD44
//#define ARM_CPU_PART_HERA
#endif

#ifndef ARM_CPU_PART_KLEIN
#define ARM_CPU_PART_KLEIN              0xD46
#endif

#ifndef ARM_CPU_PART_MATTERHORN
#define ARM_CPU_PART_MATTERHORN         0xD47
#endif

#ifndef ARM_CPU_PART_MATTERHORN_ELP
#define ARM_CPU_PART_MATTERHORN_ELP     0xD48
#endif

struct armv8_a_ERRSELR_EL1_field {
	u64 SEL         :16;
	u64 RES0        :48;
};

struct armv8_a_ERRIDR_EL1_field {
	u64 NUM         :16;
	u64 RES0        :48;
};

struct armv8_a_ERXSTATUS_EL1_field {
	u64 SERR        :8;
	u64 IERR        :8;
	u64 RES0        :4;
	u64 UET         :2;
	u64 PN          :1;
	u64 DE          :1;
	u64 CE          :2;
	u64 MV          :1;
	u64 OF          :1;
	u64 ER          :1;
	u64 UE          :1;
	u64 Valid       :1;
	u64 AV          :1;
	u64 RES1        :32;
};

struct armv8_a_ERXMISC0_EL1_field {
	u64 UNIT        :4;
	u64 ARRAY       :2;
	u64 INDEX       :13;
	u64 SUBARRAY    :4;
	u64 BANK        :2;
	u64 SUBBANK     :1;
	u64 RES1        :2;
	u64 WAY         :4;
	u64 CECR        :7;
	u64 OFR         :1;
	u64 CECO        :7;
	u64 OFO         :1;
	u64 RES0        :16;
};

struct armv8_a_ERXMISC1_EL1_field {
	u64 REG;
};

struct armv8_a_ERXADDR_EL1_field {
	u64 REG;
};

#define ARMv8_A_SYSTEM_REGISTER(name)					\
typedef struct armv8_a_##name {							\
        union {											\
                struct armv8_a_##name##_field field;	\
                u64 reg;								\
        };												\
} name##_t;

ARMv8_A_SYSTEM_REGISTER(ERRSELR_EL1);
ARMv8_A_SYSTEM_REGISTER(ERRIDR_EL1);
ARMv8_A_SYSTEM_REGISTER(ERXSTATUS_EL1);
ARMv8_A_SYSTEM_REGISTER(ERXMISC0_EL1);
ARMv8_A_SYSTEM_REGISTER(ERXMISC1_EL1);
ARMv8_A_SYSTEM_REGISTER(ERXADDR_EL1);

/* ARMv8 System Register Part */
static inline u64 kq_mesh_read_ERRSELR_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c3_1\n" : "=r" (reg));
	return reg;
}

static inline void kq_mesh_write_ERRSELR_EL1(u64 val)
{
	asm volatile ("msr S3_0_c5_c3_1, %0\n"
		"isb\n" :: "r" ((__u64)val));
}

static inline u64 kq_mesh_read_ERRIDR_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c3_0\n" : "=r" (reg));
	return reg;
}

static inline u64 kq_mesh_read_ERXSTATUS_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c4_2\n" : "=r" (reg));
	return reg;
}

static inline void __attribute__((unused)) write_ERXSTATUS_EL1(u64 val)
{
	asm volatile ("msr S3_0_c5_c4_2, %0\n"
		"isb\n" :: "r" ((__u64)val));
}

static inline u64 kq_mesh_read_ERXMISC0_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c5_0\n" : "=r" (reg));
	return reg;
}

static inline u64 kq_mesh_read_ERXMISC1_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c5_1\n" : "=r" (reg));
	return reg;
}

static inline u64 kq_mesh_read_ERXADDR_EL1(void)
{
	u64 reg;

	asm volatile ("mrs %0, S3_0_c5_c4_3\n" : "=r" (reg));
	return reg;
}

#endif	//__KQ_MESH_SYSREGS_H__