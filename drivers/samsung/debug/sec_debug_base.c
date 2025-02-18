// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kdebug.h>
#include <asm/debug-monitors.h>

#if IS_ENABLED(CONFIG_SOC_S5E3830)
#include <soc/samsung/exynos-pmu.h>
#else
#include <soc/samsung/exynos-pmu-if.h>
#endif

#include <linux/sec_debug.h>

#include "../../../kernel/sched/sched.h"
#include "sec_debug_internal.h"

extern int sec_dump_sink_init(void);

/* TODO: masking ? */
enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT		= 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC	= 0x000000C8,
	UPLOAD_CAUSE_CP_ERROR_FATAL	= 0x000000CC,
	UPLOAD_CAUSE_USER_FAULT		= 0x0000002F,
	UPLOAD_CAUSE_HARD_RESET		= 0x00000066,
	UPLOAD_CAUSE_FORCED_UPLOAD	= 0x00000022,
	UPLOAD_CAUSE_USER_FORCED_UPLOAD	= 0x00000074,
};

/* update magic for bootloader */
static void secdbg_base_set_upload_cause(enum sec_debug_upload_cause_t type)
{
	exynos_pmu_write(SEC_DEBUG_PANIC_INFORM, type);

	pr_emerg("sec_debug: set upload cause (0x%x)\n", type);
}

static int secdbg_base_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	pr_emerg("sec_debug: %s\n", __func__);

	/* Set upload cause */
	//secdbg_base_set_upload_magic(UPLOAD_MAGIC_PANIC, buf);
	if (!strncmp(buf, "User Fault", 10))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_USER_FAULT);
	else if (!strncmp(buf, "Hard Reset Hook", 15))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_HARD_RESET);
	else if (!strncmp(buf, "Crash Key", 9))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strncmp(buf, "User Crash Key", 14))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_USER_FORCED_UPLOAD);
	else if (!strncmp(buf, "CP Crash", 8))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_CP_ERROR_FATAL);
	else
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);

	/* dump debugging info */
//	if (dump) {
//		secdbg_base_dump_cpu_stat();
//		debug_show_all_locks();
#ifdef CONFIG_SEC_DEBUG_COMPLETE_HINT
//		secdbg_hint_display_complete_hint();
#endif
//	}

	return NOTIFY_DONE;
}

static int secdbg_base_die_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	struct die_args *args = (struct die_args *)buf;
	struct pt_regs *regs = args->regs;

	if (!regs)
		return NOTIFY_DONE;

	if (IS_ENABLED(CONFIG_SEC_DEBUG_PRINT_PCLR)) {
		pr_auto(ASL5, "PC is at %pS\n", (void *)(regs)->pc);
		pr_auto(ASL5, "LR is at %pS\n",
			compat_user_mode(regs) ?
				(void *)regs->compat_lr :
				(void *)ptrauth_strip_insn_pac(regs->regs[30]));
	}

	return NOTIFY_DONE;
}

#if IS_ENABLED(CONFIG_SEC_DEBUG_LOCKUP_INFO)
extern struct atomic_notifier_head hardlockup_notifier_list;

static int secdbg_base_hardlockup_info_handler(struct notifier_block *nb,
					unsigned long l, void *core)
{
	unsigned int cpu = *(unsigned int *)core;
	struct task_struct *task = cpu_rq(cpu)->curr;

	secdbg_base_set_info_hard_lockup(cpu, task);

	return NOTIFY_DONE;
}

static struct notifier_block secdbg_base_hardlockup_block = {
	.notifier_call = secdbg_base_hardlockup_info_handler,
};

static void __init secdbg_base_setup_hardlockup_handler(void)
{
	pr_info("%s\n", __func__);

	atomic_notifier_chain_register(&hardlockup_notifier_list,
					&secdbg_base_hardlockup_block);
}
#else
static inline void secdbg_base_setup_hardlockup_handler(void)
{
}
#endif /* CONFIG_SEC_DEBUG_LOCKUP_INFO */

static struct notifier_block nb_panic_block = {
	.notifier_call = secdbg_base_panic_handler,
};

static struct notifier_block nb_die_block = {
	.notifier_call = secdbg_base_die_handler,
};

#define UBSAN_TRAP_OOB_BRK_IMM		0x5512
#define UBSAN_TRAP_OOBPTR_BRK_IMM	0x1

static int ubsan_trap_handler(struct pt_regs *regs, unsigned int esr)
{
	if (IS_ENABLED(CONFIG_SEC_DEBUG_FAULT_MSG_ADV))
		pr_auto(ASL1, "UBSAN Trap: Out-of-bounds (0x%x)\n", esr);

	/* make kernel panic */
	return DBG_HOOK_ERROR;
}

static struct break_hook ubsan_oob_hook = {
	.fn = ubsan_trap_handler,
	.imm = UBSAN_TRAP_OOB_BRK_IMM,
};

static struct break_hook ubsan_oobptr_hook = {
	.fn = ubsan_trap_handler,
	.imm = UBSAN_TRAP_OOBPTR_BRK_IMM,
};

static int __init secdbg_base_init(void)
{
	int ret;

	ret = atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	pr_info("%s panic nc register done (%d)\n", __func__, ret);
	register_die_notifier(&nb_die_block);

	secdbg_base_setup_hardlockup_handler();

	secdbg_atsl_init();

	register_kernel_break_hook(&ubsan_oob_hook);
	register_kernel_break_hook(&ubsan_oobptr_hook);

	ret = sec_dump_sink_init();
	if (ret)
		pr_crit("%s: fail to sec_init dump_sink\n", __func__);

	return 0;
}
subsys_initcall(secdbg_base_init);

MODULE_DESCRIPTION("Samsung Debug base driver");
MODULE_LICENSE("GPL v2");
