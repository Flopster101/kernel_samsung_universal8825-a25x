// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_coredump.c
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/elf.h>
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/debug-snapshot-log.h>
#include "sec_debug_internal.h"

#define NT_SEC NT_AUXV
#define SEC_COREDUMP_SIZE sizeof(struct dbg_snapshot_log)
#define SEC_COREDUMP_MINIMIZED_SIZE sizeof(struct dbg_snapshot_log_minimized)

static const char *SEC_COREDUMP_NAME = "SECSYSTEM";

static int sec_elf_coredump_extra_notes_size(void)
{
	int sz;

	if (!dbg_snapshot_get_item_enable(DSS_ITEM_KEVENTS))
		return 0;

	sz = sizeof(struct elf_note);
	sz += roundup(strlen(SEC_COREDUMP_NAME) + 1, 4);
	if (dbg_snapshot_is_minized_kevents())
		sz += roundup(SEC_COREDUMP_MINIMIZED_SIZE, 4);
	else
		sz += roundup(SEC_COREDUMP_SIZE, 4);

	return sz;
}

static int sec_elf_coredump_extra_notes_write(struct coredump_params *cprm)
{
	struct elf_note en;

	if (!dbg_snapshot_get_item_enable(DSS_ITEM_KEVENTS))
		return 0;

	en.n_namesz = strlen(SEC_COREDUMP_NAME)+1;
	en.n_type = NT_SEC;

	if (dbg_snapshot_is_minized_kevents())
		en.n_descsz = SEC_COREDUMP_MINIMIZED_SIZE;
	else
		en.n_descsz = SEC_COREDUMP_SIZE;

	dump_emit(cprm, &en, sizeof(en)) &&
		dump_emit(cprm, SEC_COREDUMP_NAME, en.n_namesz) && dump_align(cprm, 4) &&
		dump_emit(cprm, (const void *)dbg_snapshot_get_item_vaddr(DSS_ITEM_KEVENTS), en.n_descsz) &&
		dump_align(cprm, 4);

	return 0;
}

static __init int sec_coredump_extra_init(void)
{
	register_coredump_hook_notes_size(sec_elf_coredump_extra_notes_size);
	register_coredump_hook_notes_write(sec_elf_coredump_extra_notes_write);

	return 0;
}
subsys_initcall_sync(sec_coredump_extra_init);

MODULE_DESCRIPTION("Samsung Debug Coredump Extra");
MODULE_LICENSE("GPL v2");
