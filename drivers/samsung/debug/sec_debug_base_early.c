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
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/sec_debug.h>
#include <soc/samsung/debug-snapshot.h>

#include "sec_debug_internal.h"

#define SEC_DEBUG_NEXT_MEMORY_NAME	"sec_debug_next"

static struct sec_debug_gen3 *secdbg_base_va;
static unsigned int sec_debug_gen3_size;
static struct sec_debug_base_param secdbg_base_param;

static uint64_t get_sdn_lv1(int type)
{
	if (type < secdbg_base_va->used_offset)
		return (uint64_t)secdbg_base_get_ncva(secdbg_base_va->lv1_data[type].addr);

	return 0;
};

#if IS_ENABLED(CONFIG_SEC_DEBUG_LOCKUP_INFO)
void secdbg_base_set_info_hard_lockup(unsigned int cpu, struct task_struct *task)
{
	struct sec_debug_kernel_data *kernd;

	if (!secdbg_base_va)
		return;

	kernd = (struct sec_debug_kernel_data *)get_sdn_lv1(SDN_LV1_KERNEL_DATA);
	if (kernd->task_in_hard_lockup)
		return;

	kernd->cpu_in_hard_lockup = (uint64_t)cpu;
	kernd->task_in_hard_lockup = (uint64_t)task;
	pr_info("%s: cpu%u\n", __func__, cpu);
}
EXPORT_SYMBOL(secdbg_base_set_info_hard_lockup);
#endif /* CONFIG_SEC_DEBUG_LOCKUP_INFO */

/* initialize kernel constant data in sec debug next */
static void secdbg_base_set_kconstants(void)
{
	struct sec_debug_kcnst *kcnst;

	kcnst = (struct sec_debug_kcnst *)get_sdn_lv1(SDN_LV1_KERNEL_CONSTANT);
	if (!kcnst) {
		pr_crit("%s: KERNEL CONSTANT is not set\n", __func__);
		return;
	}

	pr_info("%s: start to get kernel constants (%p)\n", __func__, kcnst);

	kcnst->nr_cpus = num_possible_cpus();
	kcnst->per_cpu_offset.pa = virt_to_phys(__per_cpu_offset);
	kcnst->per_cpu_offset.size = sizeof(__per_cpu_offset[0]);
	kcnst->per_cpu_offset.count = ARRAY_SIZE(__per_cpu_offset);

	kcnst->phys_offset = PHYS_OFFSET;
	kcnst->phys_mask = PHYS_MASK;
	kcnst->page_offset = PAGE_OFFSET;
	kcnst->page_mask = PAGE_MASK;
	kcnst->page_shift = PAGE_SHIFT;

	kcnst->va_bits = VA_BITS;
	kcnst->kimage_vaddr = kimage_vaddr;
	kcnst->kimage_voffset = kimage_voffset;

	kcnst->pgdir_shift = PGDIR_SHIFT;
	kcnst->pud_shift = PUD_SHIFT;
	kcnst->pmd_shift = PMD_SHIFT;
	kcnst->ptrs_per_pgd = PTRS_PER_PGD;
	kcnst->ptrs_per_pud = PTRS_PER_PUD;
	kcnst->ptrs_per_pmd = PTRS_PER_PMD;
	kcnst->ptrs_per_pte = PTRS_PER_PTE;

	pr_info("%s: end to get kernel constants\n", __func__);
}

dss_extern_get_log_by_cpu(task);
dss_extern_get_log_by_cpu(work);
dss_extern_get_log_by_cpu(cpuidle);
dss_extern_get_log_by_cpu(freq);
dss_extern_get_log_by_cpu(irq);
dss_extern_get_log(thermal);
dss_extern_get_log(acpm);

static void secdbg_base_get_kevent_info(struct ess_info_offset *p, int type)
{
	unsigned long kevent_base_va = dss_get_vaddr_task_log_by_cpu(0);
	unsigned long kevent_base_pa = (unsigned long)dbg_snapshot_get_item_paddr("log_kevents");

	switch (type) {
	case DSS_KEVENT_TASK:
		p->base = kevent_base_pa + dss_get_vaddr_task_log_by_cpu(0) - kevent_base_va;
		p->nr = (unsigned int)dss_get_len_task_log_by_cpu(0);
		p->size = (unsigned int)dss_get_sizeof_task_log();
		p->per_core = 1;
		p->last = dss_get_last_paddr_task_log(0);
		break;

	case DSS_KEVENT_WORK:
		p->base = kevent_base_pa + dss_get_vaddr_work_log_by_cpu(0) - kevent_base_va;
		p->nr = (unsigned int)dss_get_len_work_log_by_cpu(0);
		p->size = (unsigned int)dss_get_sizeof_work_log();
		p->per_core = 1;
		p->last = dss_get_last_paddr_work_log(0);
		break;

	case DSS_KEVENT_IRQ:
		p->base = kevent_base_pa + dss_get_vaddr_irq_log_by_cpu(0) - kevent_base_va;
		p->nr = (unsigned int)dss_get_len_irq_log_by_cpu(0);
		p->size = (unsigned int)dss_get_sizeof_irq_log();
		p->per_core = 1;
		p->last = dss_get_last_paddr_irq_log(0);
		break;

	case DSS_KEVENT_FREQ:
		p->base = kevent_base_pa + dss_get_vaddr_freq_log_by_cpu(0) - kevent_base_va;
		p->nr = (unsigned int)dss_get_len_freq_log_by_cpu(0);
		p->size = (unsigned int)dss_get_sizeof_freq_log();
		p->per_core = dss_get_len_freq_log();
		p->last = dss_get_last_paddr_freq_log(0);
		break;

	case DSS_KEVENT_IDLE:
		p->base = kevent_base_pa + dss_get_vaddr_cpuidle_log_by_cpu(0) - kevent_base_va;
		p->nr = (unsigned int)dss_get_len_cpuidle_log_by_cpu(0);
		p->size = (unsigned int)dss_get_sizeof_cpuidle_log();
		p->per_core = 1;
		p->last = dss_get_last_paddr_cpuidle_log(0);
		break;

	case DSS_KEVENT_THRM:
		p->base = kevent_base_pa + dss_get_vaddr_thermal_log() - kevent_base_va;
		p->nr = (unsigned int)dss_get_len_thermal_log();
		p->size = (unsigned int)dss_get_sizeof_thermal_log();
		p->per_core = 0;
		p->last = dss_get_last_paddr_thermal_log();
		break;

	case DSS_KEVENT_ACPM:
		p->base = kevent_base_pa + dss_get_vaddr_acpm_log() - kevent_base_va;
		p->nr = (unsigned int)dss_get_len_acpm_log();
		p->size = (unsigned int)dss_get_sizeof_acpm_log();
		p->per_core = 0;
		p->last = dss_get_last_paddr_acpm_log();
		break;

	default:
		p->base = 0;
		p->nr = 0;
		p->size = 0;
		p->per_core = 0;
		p->last = 0;
		break;
	}
}

static void init_ess_info(struct secdbg_snapshot_offset *ss_info, unsigned int index, char *key)
{
	struct ess_info_offset *p = &ss_info->data[index];

	secdbg_base_get_kevent_info(p, index);

	memset(p->key, 0, SD_ESSINFO_KEY_SIZE);
	snprintf(p->key, SD_ESSINFO_KEY_SIZE, "%s", key);
}

/* initialize snapshot offset data in sec debug next */
static void secdbg_base_set_essinfo(void)
{
	unsigned int index = 0;
	struct secdbg_snapshot_offset *ss_info;

	ss_info = (struct secdbg_snapshot_offset *)get_sdn_lv1(SDN_LV1_SNAPSHOT);

	memset(ss_info, 0, sizeof(struct sec_debug_ess_info));

	init_ess_info(ss_info, index++, "kevnt-task");
	init_ess_info(ss_info, index++, "kevnt-work");
	init_ess_info(ss_info, index++, "kevnt-irq");
	init_ess_info(ss_info, index++, "kevnt-freq");
	init_ess_info(ss_info, index++, "kevnt-idle");

	if (!dbg_snapshot_is_minized_kevents()) {
		init_ess_info(ss_info, index++, "kevnt-thrm");
		init_ess_info(ss_info, index++, "kevnt-acpm");
	}

	for (; index < SD_NR_ESSINFO_ITEMS;)
		init_ess_info(ss_info, index++, "empty");

	for (index = 0; index < SD_NR_ESSINFO_ITEMS; index++)
		pr_info("%s: key: %s offset: %lx nr: %x last: %lx\n", __func__,
				ss_info->data[index].key,
				ss_info->data[index].base,
				ss_info->data[index].nr,
				ss_info->data[index].last);
}

/* initialize task_struct offset data in sec debug next */
static void secdbg_base_set_taskinfo(void)
{
	struct sec_debug_task *task;

	task = (struct sec_debug_task *)get_sdn_lv1(SDN_LV1_TASK_STRUCT);

	task->stack_size = THREAD_SIZE;
	task->start_sp = THREAD_START_SP;
	task->irq_stack.size = IRQ_STACK_SIZE;
	task->irq_stack.start_sp = IRQ_STACK_START_SP;

	task->ti.struct_size = sizeof(struct thread_info);
	SET_MEMBER_TYPE_INFO(&task->ti.flags, struct thread_info, flags);
	SET_MEMBER_TYPE_INFO(&task->ts.cpu, struct task_struct, cpu);

	task->ts.struct_size = sizeof(struct task_struct);
	SET_MEMBER_TYPE_INFO(&task->ts.state, struct task_struct, state);
	SET_MEMBER_TYPE_INFO(&task->ts.exit_state, struct task_struct,
					exit_state);
	SET_MEMBER_TYPE_INFO(&task->ts.stack, struct task_struct, stack);
	SET_MEMBER_TYPE_INFO(&task->ts.flags, struct task_struct, flags);
	SET_MEMBER_TYPE_INFO(&task->ts.on_cpu, struct task_struct, on_cpu);
	SET_MEMBER_TYPE_INFO(&task->ts.pid, struct task_struct, pid);
	SET_MEMBER_TYPE_INFO(&task->ts.on_rq, struct task_struct, on_rq);
	SET_MEMBER_TYPE_INFO(&task->ts.comm, struct task_struct, comm);
	SET_MEMBER_TYPE_INFO(&task->ts.tasks_next, struct task_struct,
					tasks.next);
	SET_MEMBER_TYPE_INFO(&task->ts.thread_group_next,
					struct task_struct, thread_group.next);
	SET_MEMBER_TYPE_INFO(&task->ts.fp, struct task_struct,
					thread.cpu_context.fp);
	SET_MEMBER_TYPE_INFO(&task->ts.sp, struct task_struct,
					thread.cpu_context.sp);
	SET_MEMBER_TYPE_INFO(&task->ts.pc, struct task_struct,
					thread.cpu_context.pc);

	SET_MEMBER_TYPE_INFO(&task->ts.sched_info__pcount,
					struct task_struct, sched_info.pcount);
	SET_MEMBER_TYPE_INFO(&task->ts.sched_info__run_delay,
					struct task_struct,
					sched_info.run_delay);
	SET_MEMBER_TYPE_INFO(&task->ts.sched_info__last_arrival,
					struct task_struct,
					sched_info.last_arrival);
	SET_MEMBER_TYPE_INFO(&task->ts.sched_info__last_queued,
					struct task_struct,
					sched_info.last_queued);
#if IS_ENABLED(CONFIG_SEC_DEBUG_DTASK)
	SET_MEMBER_TYPE_INFO(&task->ts.ssdbg_wait__type,
					struct task_struct,
					android_vendor_data1[0]);
	SET_MEMBER_TYPE_INFO(&task->ts.ssdbg_wait__data,
					struct task_struct,
					android_vendor_data1[1]);
#endif

	task->init_task = (uint64_t)&init_task;
}

/* from PA to NonCachable VA */
static unsigned long sdn_ncva_to_pa_offset;

void *secdbg_base_get_ncva(unsigned long pa)
{
	return (void *)(pa + sdn_ncva_to_pa_offset);
}
EXPORT_SYMBOL(secdbg_base_get_ncva);

struct watchdogd_info *secdbg_base_get_wdd_info(void)
{
	struct sec_debug_kernel_data *kernd;

	if (!secdbg_base_va) {
		pr_crit("%s: return NULL\n", __func__);
		return NULL;
	}

	kernd = (struct sec_debug_kernel_data *)get_sdn_lv1(SDN_LV1_KERNEL_DATA);

	pr_crit("%s: return right value\n", __func__);

	return &(kernd->wddinfo);
}
EXPORT_SYMBOL_GPL(secdbg_base_get_wdd_info);

/* Share SDN memory area */
void *secdbg_base_get_debug_base(int type)
{
	if (secdbg_base_va) {
		if (type == SDN_MAP_AUTO_COMMENT)
			return (void *)(get_sdn_lv1(SDN_LV1_AUTO_COMMENT));
		else if (type == SDN_MAP_EXTRA_INFO)
			return (void *)(get_sdn_lv1(SDN_LV1_EXTRA_INFO));
	}

	pr_crit("%s: return NULL\n", __func__);

	return NULL;
}
EXPORT_SYMBOL(secdbg_base_get_debug_base);

void *secdbg_base_get_kcnst_base(void)
{
	if (secdbg_base_va)
		return (void *)(get_sdn_lv1(SDN_LV1_KERNEL_CONSTANT));

	pr_crit("%s: return NULL\n", __func__);

	return NULL;
}
EXPORT_SYMBOL(secdbg_base_get_kcnst_base);

unsigned long secdbg_base_get_buf_base(int type)
{
	struct secdbg_logbuf_list *p;

	if (secdbg_base_va) {
		p = (struct secdbg_logbuf_list *)get_sdn_lv1(SDN_LV1_LOGBUF_MAP);
		if (p) {
			pr_crit("%s: return %llx\n", __func__, p->data[type].base);
			return p->data[type].base;
		}
	}

	pr_crit("%s: return 0\n", __func__);
	return 0;
}
EXPORT_SYMBOL(secdbg_base_get_buf_base);

unsigned long secdbg_base_get_buf_size(int type)
{
	struct secdbg_logbuf_list *p;

	if (secdbg_base_va) {
		p = (struct secdbg_logbuf_list *)get_sdn_lv1(SDN_LV1_LOGBUF_MAP);
		if (p)
			return p->data[type].size;
	}

	pr_crit("%s: return 0\n", __func__);

	return 0;
}
EXPORT_SYMBOL(secdbg_base_get_buf_size);

unsigned long secdbg_base_get_end_addr(void)
{
	return (unsigned long)secdbg_base_va + sec_debug_gen3_size;
}
EXPORT_SYMBOL(secdbg_base_get_end_addr);

static void secdbg_base_set_version(void)
{
	secdbg_base_va->version[1] = SEC_DEBUG_KERNEL_UPPER_VERSION << 16;
	secdbg_base_va->version[1] += SEC_DEBUG_KERNEL_LOWER_VERSION;
}

static void secdbg_base_set_magic(void)
{
	secdbg_base_va->magic[0] = SEC_DEBUG_GEN3_MAGIC0;
	secdbg_base_va->magic[1] = SEC_DEBUG_GEN3_MAGIC1;
}

static void secdbg_base_clear_sdn(struct sec_debug_gen3 *d)
{
#define clear_sdn_field(__p, __m)	memset((void *)get_sdn_lv1(__m), 0x0, __p->lv1_data[__m].size)

	clear_sdn_field(d, SDN_LV1_MEMTAB);
	clear_sdn_field(d, SDN_LV1_KERNEL_CONSTANT);
	clear_sdn_field(d, SDN_LV1_TASK_STRUCT);
	clear_sdn_field(d, SDN_LV1_SNAPSHOT);
	clear_sdn_field(d, SDN_LV1_KERNEL_DATA);
}

static void secdbg_base_set_sdn_ready(void)
{
	/*
	 * Ensure clearing sdn is visible before we indicate done.
	 * Pairs with the smp_cond_load_acquire() in
	 * secdbg_base_built_wait_for_init().
	 */
	smp_store_release(&secdbg_base_param.init_sdn_done, true);
}

static int secdbg_base_init_sdn(void)
{
	if (!secdbg_base_va) {
		pr_crit("%s: SDN is not allocated, quit\n", __func__);

		return -1;
	}

	secdbg_base_set_magic();
	secdbg_base_set_version();

	secdbg_base_set_kconstants();
	secdbg_base_set_essinfo();
	secdbg_base_set_taskinfo();

	pr_info("%s: done\n", __func__);

	return 0;
}

static int secdbg_base_reserved_memory_setup(struct device *dev)
{
	unsigned long i, j;
	struct page *page;
	struct page **pages;
	int page_size, mem_count = 0;
	void *vaddr;
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	unsigned long flags = VM_NO_GUARD | VM_MAP;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;

	pr_info("%s: start\n", __func__);

	mem_count = of_count_phandle_with_args(dev->of_node, "memory-region", NULL);
	if (mem_count <= 0) {
		pr_crit("%s: no such memory-region: ret %d\n", __func__, mem_count);

		return 0;
	}

	for (j = 0 ; j < mem_count ; j++) {
		rmem_np = of_parse_phandle(dev->of_node, "memory-region", j);
		if (!rmem_np) {
			pr_crit("%s: no such memory-region(%lu)\n", __func__, j);

			return 0;
		}

		rmem = of_reserved_mem_lookup(rmem_np);
		if (!rmem) {
			pr_crit("%s: no such reserved mem of node name %s\n", __func__,
				dev->of_node->name);

			return 0;
		} else if (!rmem->base || !rmem->size) {
			pr_crit("%s: wrong base(0x%llx) or size(0x%llx)\n",
					__func__, rmem->base, rmem->size);

			return 0;
		}

		pr_info("%s: %s: set as non cacheable\n", __func__, rmem->name);

		page_size = rmem->size / PAGE_SIZE;
		pages = kcalloc(page_size, sizeof(struct page *), GFP_KERNEL);
		page = phys_to_page(rmem->base);

		for (i = 0; i < page_size; i++)
			pages[i] = page++;

		vaddr = vmap(pages, page_size, flags, prot);
		kfree(pages);
		if (!vaddr) {
			pr_crit("%s: %s: paddr:%llx page_size:%llx failed to mapping between virt and phys\n",
					__func__, rmem->name, rmem->base, rmem->size);

			return 0;
		}

		pr_info("%s: %s: base VA: %llx ++ %x\n", __func__, rmem->name,
			(uint64_t)vaddr, (unsigned int)(rmem->size));

		if (!strncmp(SEC_DEBUG_NEXT_MEMORY_NAME, rmem->name, strlen(rmem->name))) {
			rmem->priv = &secdbg_base_param;
			secdbg_base_param.sdn_vaddr = vaddr;
			secdbg_base_va = (struct sec_debug_gen3 *)vaddr;
			sec_debug_gen3_size = (unsigned int)(rmem->size);
			sdn_ncva_to_pa_offset = (unsigned long)vaddr - (unsigned long)rmem->base;

			secdbg_base_clear_sdn(secdbg_base_va);

			secdbg_base_set_sdn_ready();

			pr_info("%s: set private: %llx, offset from va: %lx\n", __func__,
				(uint64_t)vaddr, sdn_ncva_to_pa_offset);
		}
	}

	return 1;
}

static int secdbg_base_early_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("%s: start\n", __func__);

	if (!secdbg_base_reserved_memory_setup(&pdev->dev)) {
		pr_crit("%s: failed\n", __func__);
		return -ENODEV;
	}

	ret = secdbg_base_init_sdn();
	if (ret) {
		pr_crit("%s: fail to init sdn\n", __func__);
		return -ENODEV;
	}

	ret = secdbg_part_init_bdev_path(&pdev->dev);
	if (ret) {
		pr_crit("%s: fail to init bdev path\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static const struct of_device_id sec_debug_of_match[] = {
	{ .compatible	= "samsung,sec_debug" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_debug_of_match);

static struct platform_driver sec_debug_driver = {
	.probe = secdbg_base_early_probe,
	.driver  = {
		.name  = "sec_debug_base",
		.of_match_table = of_match_ptr(sec_debug_of_match),
	},
};

static int __init secdbg_base_early_init(void)
{
	return platform_driver_register(&sec_debug_driver);
}
core_initcall(secdbg_base_early_init);

static void __exit secdbg_base_early_exit(void)
{
	platform_driver_unregister(&sec_debug_driver);
}
module_exit(secdbg_base_early_exit);

MODULE_DESCRIPTION("Samsung Debug base early driver");
MODULE_LICENSE("GPL v2");
