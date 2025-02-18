/* SPDX-License-Identifier: GPL-2.0 */

/*
 * (C) COPYRIGHT 2021 Samsung Electronics Inc. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

/* Implements */
#include <gpexbe_mem_usage.h>
#include <trace/hooks/mm.h>
/* Uses */
#include <gpex_utils.h>
#include <device/mali_kbase_device.h>
#include <linux/oom.h>

static struct kbase_device *kbdev;

static ssize_t show_kernel_sysfs_gpu_memory(char *buf)
{
	ssize_t ret = 0;
	uint64_t gpu_mem_used = 0;
	bool buffer_full = false;
	const ssize_t buf_size = PAGE_SIZE;
	const int padding = 100;
	struct kbase_context *kctx;

	if (buf == NULL)
		return ret;

	ret += scnprintf(buf + ret, buf_size - ret, "%9s %9s %12s\n", "tgid", "pid",
			 "bytes_used");

	mutex_lock(&kbdev->kctx_list_lock);
	list_for_each_entry (kctx, &kbdev->kctx_list, kctx_list_link) {
		if (ret + padding > buf_size) {
			buffer_full = true;
			break;
		}

		gpu_mem_used = atomic_read(&(kctx->used_pages)) * PAGE_SIZE;
		ret += snprintf(buf + ret, buf_size - ret, "%9d %9d %12llu\n", kctx->tgid,
				kctx->pid, gpu_mem_used);
	}
	mutex_unlock(&kbdev->kctx_list_lock);

	if (buffer_full)
		ret += scnprintf(buf + ret, buf_size - ret, "error: buffer is full\n");

	return ret;
}
CREATE_SYSFS_KOBJECT_READ_FUNCTION(show_kernel_sysfs_gpu_memory);

static int gpu_memory_status_dump(bool print_all_buffers)
{
	struct kbase_context *kctx = NULL;
	struct device *dev = NULL;
	int total_used_pages = 0;

	dev = kbdev->dev;
	total_used_pages += atomic_read(&(kbdev->memdev.used_pages));

	if (print_all_buffers) {
		dev_warn(dev, "%-16s  %10u\n", kbdev->devname, total_used_pages);
		if (mutex_trylock(&kbdev->kctx_list_lock)) {
			list_for_each_entry (kctx, &kbdev->kctx_list, kctx_list_link) {
				dev_warn(dev, "%10u | tgid=%10d | pid=%10d  | name=%20s\n",
						atomic_read(&(kctx->used_pages)),
						kctx->tgid,
						kctx->pid,
						((struct platform_context *)kctx->platform_data)->name);
			}
		}
		mutex_unlock(&kbdev->kctx_list_lock);
	}

	return total_used_pages;
}

static int mali_used_size_notifier(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct seq_file *s;
	int used_pages = gpu_memory_status_dump(false);

	if (used_pages < 0)
		return 0;

	used_pages = (used_pages << (PAGE_SHIFT - 10));

	s = (struct seq_file *)data;
	if (s != NULL)
		seq_printf(s, "mali:           %8lu kB\n", used_pages);
	else
		pr_cont("mali:%lukB ", used_pages);

	return 0;
}

static struct notifier_block mali_used_size_nb = {
	.notifier_call = mali_used_size_notifier,
};

static int mali_used_buffer_oom_notifier(struct notifier_block *nb,
		unsigned long action, void *data)
{
	gpu_memory_status_dump(true);

	return 0;
}

static struct notifier_block mali_used_buffer_oom_nb = {
	.notifier_call = mali_used_buffer_oom_notifier,
};

static void show_mali_used_size_mem(void *data, unsigned int filter,
		nodemask_t *nodemask)
{
	struct seq_file *s;
	int used_pages = gpu_memory_status_dump(false);

	if (used_pages < 0)
		return;

	used_pages = (used_pages << (PAGE_SHIFT - 10));

	s = (struct seq_file *)data;
	if (s != NULL)
		seq_printf(s, "GpuTotal:           %8lu kB\n", used_pages);
	else
		pr_cont("GpuTotal:%lukB ", used_pages);
}

static void show_mali_used_size_proc_meminfo(void *data, struct seq_file *m)
{
	int used_pages = gpu_memory_status_dump(false);

	if (used_pages < 0)
		return;

	used_pages = (used_pages << (PAGE_SHIFT - 10));

	if (m != NULL)
		seq_printf(m, "GpuTotal:           %8lu kB\n", used_pages);
	else
		pr_cont("GpuTotal:%lukB ", used_pages);
}

static void register_mali_used_mem_notifier(void) {
#ifdef CONFIG_MALI_EXYNOS_GPU_MEM_DUMP

	/*This is used to print mali used size when do: echo m > /proc/sysrq-trigger; dmesg*/
	register_trace_android_vh_show_mem(show_mali_used_size_mem,NULL);

	/*This is used to print mali used size when do: cat /proc/meminfo */
	register_trace_android_vh_meminfo_proc_show(show_mali_used_size_proc_meminfo,NULL);

	CSTD_UNUSED(mali_used_size_nb);
	CSTD_UNUSED(mali_used_buffer_oom_nb);
#else
	CSTD_UNUSED(show_mali_used_size_mem);
	CSTD_UNUSED(show_mali_used_size_proc_meminfo);
	CSTD_UNUSED(mali_used_size_nb);
	CSTD_UNUSED(mali_used_buffer_oom_nb);
#endif
}

int gpexbe_mem_usage_init(void)
{
	kbdev = gpex_utils_get_kbase_device();

	GPEX_UTILS_SYSFS_KOBJECT_FILE_ADD_RO(gpu_memory, show_kernel_sysfs_gpu_memory);

	register_mali_used_mem_notifier();

	return 0;
}

void gpexbe_mem_usage_term(void)
{
	return;
}
