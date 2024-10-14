// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2021 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#ifndef __JUMP_TABLE_TARGET_H__
#define __JUMP_TABLE_TARGET_H__

#include <linux/uaccess.h>

/* FIXME: this implementaion should be placed on arch/arm64/include/asm. */
/* TODO: Performance degradation is suspected when this code is used. */
#if IS_ENABLED(CONFIG_ARM64)
static const __always_inline void *__arm64_jump_table_target(const void *va)
{
	const uint32_t bti_c_code = 0xD503245F;
	const uint32_t b_op_code = 0x5;
	union b_instr {
		struct {
			uint32_t imm26:26;
			uint32_t op:6;
		};
		struct {
			uint32_t __imm26:25;
			uint32_t __msb26:1;
			uint32_t __op:6;
		};
	};
	uint32_t bti_c_instr;
	const void *b_instr_ptr;
	union b_instr b_instr;
	uintptr_t offset;

	if (!IS_ENABLED(CONFIG_CFI_CLANG))
		return va;

	if (unlikely(get_kernel_nofault(bti_c_instr, va)))
		return va;

	if (unlikely(IS_ENABLED(CONFIG_ARM64_BTI_KERNEL) &&
			bti_c_instr != bti_c_code))
		return va;

	b_instr_ptr = va + (IS_ENABLED(CONFIG_ARM64_BTI_KERNEL) ? 0x4ULL : 0);
	if (unlikely(get_kernel_nofault(b_instr, b_instr_ptr)))
		return va;

	if (unlikely(b_instr.op != b_op_code))
		return va;

	offset = (uintptr_t)b_instr.imm26;
	if (b_instr.__msb26)
		offset |= ~((1ULL << 26ULL) - 1ULL);

	offset = offset << 2ULL;

	return (void *)((uintptr_t)b_instr_ptr + offset);
}

#define __jump_table_target(__va)	__arm64_jump_table_target(__va)
#endif

/* NOTE: architecture independent layer */
#ifdef __jump_table_target
#define jump_table_target(__va)		__jump_table_target(__va)
#else
#define jump_table_target(__va)		(__va)
#endif

#endif /* __JUMP_TABLE_TARGET_H__ */

