// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && defined(CONFIG_EXYNOS_DSP_HW_P0)
#include <soc/samsung/debug-snapshot.h>
#else
#define dbg_snapshot_expire_watchdog()
#endif

#include "dsp-log.h"
#include "dsp-dump.h"
#include "dsp-hw-common-init.h"
#include "dsp-device.h"
#include "dsp-binary.h"
#include "dsp-hw-p0-pm.h"
#include "dsp-hw-p0-clk.h"
#include "dsp-hw-p0-bus.h"
#include "dsp-hw-p0-llc.h"
#include "dsp-hw-p0-memory.h"
#include "dsp-hw-p0-interface.h"
#include "dsp-hw-p0-ctrl.h"
#include "dsp-hw-p0-mailbox.h"
#include "dsp-hw-p0-governor.h"
#include "dsp-hw-p0-imgloader.h"
#include "dsp-hw-p0-sysevent.h"
#include "dsp-hw-p0-memlogger.h"
#include "dsp-hw-p0-log.h"
#include "dsp-hw-p0-debug.h"
#include "dsp-hw-p0-dump.h"
#include "dsp-hw-common-system.h"
#include "dsp-hw-p0-system.h"

static void dsp_hw_p0_system_iovmm_fault_dump(struct dsp_system *sys)
{
	dsp_enter();
	dsp_hw_common_system_iovmm_fault_dump(sys);
	dsp_leave();
}

static int __dsp_hw_p0_system_master_copy(struct dsp_system *sys)
{
	int ret;
	struct dsp_reserved_mem *rmem;

	dsp_enter();
	rmem = &sys->memory.reserved_mem[DSP_P0_RESERVED_MEM_FW_MASTER];

	if (!sys->boot_bin_size) {
		dsp_warn("master bin is not loaded yet\n");
		ret = dsp_binary_load(DSP_P0_MASTER_FW_NAME, NULL,
				DSP_P0_FW_EXTENSION, rmem->kvaddr, rmem->size,
				&rmem->used_size);
		if (ret)
			goto p_err;

		sys->boot_bin_size = rmem->used_size;
		dsp_info("master binary[%zu] is loaded\n", sys->boot_bin_size);
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_p0_system_init(struct dsp_system *sys)
{
	struct dsp_priv_mem *pmem;
	unsigned int chip_id;
	unsigned int product_id;

	dsp_enter();
	pmem = sys->memory.priv_mem;

	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_TO_CC_INT_STATUS), 0);
	dsp_ctrl_dhcp_writel(
			DSP_P0_DHCP_IDX(DSP_P0_DHCP_TO_HOST_INT_STATUS), 0);

	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_DEBUG_LAYER_START),
			sys->layer_start);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_DEBUG_LAYER_END),
			sys->layer_end);

	memcpy(pmem[DSP_P0_PRIV_MEM_FW].kvaddr,
			pmem[DSP_P0_PRIV_MEM_FW].bac_kvaddr,
			pmem[DSP_P0_PRIV_MEM_FW].used_size);

	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_FW_RESERVED_SIZE),
			pmem[DSP_P0_PRIV_MEM_FW].size);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_IVP_PM_IOVA),
			pmem[DSP_P0_PRIV_MEM_IVP_PM].iova);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_IVP_PM_SIZE),
			pmem[DSP_P0_PRIV_MEM_IVP_PM].used_size);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_IVP_DM_IOVA),
			pmem[DSP_P0_PRIV_MEM_IVP_DM].iova);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_IVP_DM_SIZE),
			pmem[DSP_P0_PRIV_MEM_IVP_DM].used_size);

	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_MAILBOX_VERSION), 0);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_MESSAGE_VERSION), 0);

	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_FW_LOG_MEMORY_IOVA),
			pmem[DSP_P0_PRIV_MEM_FW_LOG].iova);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_FW_LOG_MEMORY_SIZE),
			pmem[DSP_P0_PRIV_MEM_FW_LOG].size);
	dsp_ctrl_dhcp_writel(
			DSP_P0_DHCP_IDX(DSP_P0_DHCP_TO_CC_MBOX_MEMORY_IOVA),
			pmem[DSP_P0_PRIV_MEM_MBOX_MEMORY].iova);
	dsp_ctrl_dhcp_writel(
			DSP_P0_DHCP_IDX(DSP_P0_DHCP_TO_CC_MBOX_MEMORY_SIZE),
			pmem[DSP_P0_PRIV_MEM_MBOX_MEMORY].size);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_TO_CC_MBOX_POOL_IOVA),
			pmem[DSP_P0_PRIV_MEM_MBOX_POOL].iova);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_TO_CC_MBOX_POOL_SIZE),
			pmem[DSP_P0_PRIV_MEM_MBOX_POOL].size);

	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_KERNEL_MODE),
			DSP_DYNAMIC_KERNEL);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_DL_OUT_IOVA),
			pmem[DSP_P0_PRIV_MEM_DL_OUT].iova);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_DL_OUT_SIZE),
			pmem[DSP_P0_PRIV_MEM_DL_OUT].size);

#ifndef ENABLE_DSP_VELOCE
	chip_id = readl(sys->chip_id);
#else
	chip_id = 0xdeadbeef;
#endif
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_CHIPID_REV), chip_id);
	dsp_info("CHIPID : %#x\n", chip_id);

#ifndef ENABLE_DSP_VELOCE
	product_id = readl(sys->product_id);
#else
	product_id = 0xdeadbeef;
#endif
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_PRODUCT_ID),
			product_id);
	dsp_info("PRODUCT_ID : %#x\n", product_id);

	if (sys->wait_mode == DSP_SYSTEM_WAIT_MODE_INTERRUPT)
		dsp_ctrl_writel(DSP_P0_DNCC_NS_IRQ_MBOX_ENABLE_FR_CC, 0x11);
	else if (sys->wait_mode == DSP_SYSTEM_WAIT_MODE_BUSY_WAITING)
		dsp_ctrl_writel(DSP_P0_DNCC_NS_IRQ_MBOX_ENABLE_FR_CC, 0x0);

	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_INTERRUPT_MODE),
			sys->wait_mode);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_DRIVER_VERSION),
			sys->dspdev->version);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_FIRMWARE_VERSION),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_RESERVED0),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_RESERVED1),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_RESERVED2),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_RESERVED3),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_RESERVED4),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_RESERVED5),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_RESERVED6),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_RESERVED7),
			0xdeadbeef);
	dsp_ctrl_dhcp_writel(DSP_P0_DHCP_IDX(DSP_P0_DHCP_RESERVED8),
			0xdeadbeef);

	dsp_leave();
}

static int __dsp_hw_p0_system_wait_boot(struct dsp_system *sys)
{
	int ret;
	unsigned int wait_time;
	long timeout;

	dsp_enter();
	wait_time = sys->wait[DSP_SYSTEM_WAIT_BOOT];
#ifndef ENABLE_DSP_VELOCE
	dsp_dbg("[boot] wait mode : %u\n", sys->wait_mode);
	if (sys->wait_mode == DSP_SYSTEM_WAIT_MODE_INTERRUPT) {
		timeout = wait_event_timeout(sys->system_wq,
				sys->system_flag & BIT(DSP_SYSTEM_BOOT),
				msecs_to_jiffies(wait_time));
		if (!timeout) {
			sys->interface.ops->check_irq(&sys->interface);
			if (sys->system_flag & BIT(DSP_SYSTEM_BOOT)) {
				timeout = 1;
				dsp_warn("interrupt was unstable\n");
			}
		}
	} else if (sys->wait_mode == DSP_SYSTEM_WAIT_MODE_BUSY_WAITING) {
		timeout = wait_time * 1000;

		while (timeout) {
			if (sys->system_flag & BIT(DSP_SYSTEM_BOOT))
				break;

			sys->interface.ops->check_irq(&sys->interface);
			timeout -= 10;
			udelay(10);
		}
	} else {
		ret = -EINVAL;
		dsp_err("wait mode(%u) is invalid\n", sys->wait_mode);
		goto p_err;
	}
#else
	while (1) {
		if (sys->system_flag & BIT(DSP_SYSTEM_BOOT))
			break;

		dsp_ctrl_pc_dump();
		dsp_ctrl_userdefined_dump();
		mdelay(100);
	}
	timeout = 1;
#endif
	if (!timeout) {
		ret = -ETIMEDOUT;
		dsp_err("Failed to boot DSP (wait time %ums)\n",
				sys->wait[DSP_SYSTEM_WAIT_BOOT]);
		if (sys->timeout_handler) {
			sys->timeout_handler(sys, DSP_SYSTEM_WAIT_BOOT);
		} else {
			dsp_warn("timeout handler was not registered\n");
			dsp_dump_ctrl();
			dsp_dump_task_count();
		}
		goto p_err;
	} else {
		dsp_info("Completed to boot DSP\n");
		dsp_dump_task_count();
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_p0_system_check_kernel_mode(struct dsp_system *sys)
{
	int kernel_mode;

	dsp_enter();
	kernel_mode = dsp_ctrl_dhcp_readl(
			DSP_P0_DHCP_IDX(DSP_P0_DHCP_KERNEL_MODE));
	if (kernel_mode != DSP_DYNAMIC_KERNEL) {
		dsp_err("static kernel is no longer available\n");
		return -EINVAL;
	}
	dsp_leave();
	return 0;
}

static int dsp_hw_p0_system_boot(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	sys->system_flag = 0x0;

	sys->ctrl.ops->all_init(&sys->ctrl);
	ret = __dsp_hw_p0_system_master_copy(sys);
	if (ret)
		goto p_err;

	ret = sys->imgloader.ops->boot(&sys->imgloader);
	if (ret) {
		dsp_err("Failed to boot imgloader ops(%d/%zu)\n",
				ret, sys->boot_bin_size);
		goto p_err;
	}

	__dsp_hw_p0_system_init(sys);
	sys->log.ops->start(&sys->log);

	sys->pm.ops->update_devfreq_boot(&sys->pm);
	sys->ctrl.ops->start(&sys->ctrl);
	ret = __dsp_hw_p0_system_wait_boot(sys);
	sys->pm.ops->update_devfreq_min(&sys->pm);
	if (ret)
		goto p_err_reset;

	ret = __dsp_hw_p0_system_check_kernel_mode(sys);
	if (ret)
		goto p_err_reset;

	ret = sys->mailbox.ops->start(&sys->mailbox);
	if (ret)
		goto p_err_reset;

	sys->boot_init |= BIT(DSP_SYSTEM_DSP_INIT);
	dsp_leave();
	return 0;
p_err_reset:
	sys->ops->reset(sys);
p_err:
	return ret;
}

static int __dsp_hw_p0_system_wait_reset(struct dsp_system *sys)
{
	int ret;
	unsigned int wait_time;
	long timeout;

	dsp_enter();
	wait_time = sys->wait[DSP_SYSTEM_WAIT_RESET];
#ifndef ENABLE_DSP_VELOCE
	dsp_dbg("[reset] wait mode : %u\n", sys->wait_mode);
	if (sys->wait_mode == DSP_SYSTEM_WAIT_MODE_INTERRUPT) {
		timeout = wait_event_timeout(sys->system_wq,
				sys->system_flag & BIT(DSP_SYSTEM_RESET),
				msecs_to_jiffies(wait_time));
		if (!timeout) {
			sys->interface.ops->check_irq(&sys->interface);
			if (sys->system_flag & BIT(DSP_SYSTEM_RESET)) {
				timeout = 1;
				dsp_warn("interrupt was unstable\n");
			}
		}
	} else if (sys->wait_mode == DSP_SYSTEM_WAIT_MODE_BUSY_WAITING) {
		timeout = wait_time * 1000;

		while (timeout) {
			if (sys->system_flag & BIT(DSP_SYSTEM_RESET))
				break;

			sys->interface.ops->check_irq(&sys->interface);
			timeout -= 10;
			udelay(10);
		}
	} else {
		ret = -EINVAL;
		dsp_err("wait mode(%u) is invalid\n", sys->wait_mode);
		goto p_err;
	}
#else
	while (1) {
		if (sys->system_flag & BIT(DSP_SYSTEM_RESET))
			break;

		dsp_ctrl_pc_dump();
		dsp_ctrl_userdefined_dump();
		mdelay(100);
	}
	timeout = 1;
#endif
	if (!timeout) {
		ret = -ETIMEDOUT;
		dsp_err("Failed to reset DSP (wait time %ums)\n",
				sys->wait[DSP_SYSTEM_WAIT_RESET]);
		if (sys->timeout_handler) {
			sys->timeout_handler(sys, DSP_SYSTEM_WAIT_RESET);
		} else {
			dsp_warn("timeout handler was not registered\n");
			dsp_dump_ctrl();
			dsp_dump_task_count();
		}
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_p0_system_reset(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	sys->system_flag = 0x0;
	sys->pm.ops->update_devfreq_boot(&sys->pm);
	sys->interface.ops->send_irq(&sys->interface, BIT(DSP_TO_CC_INT_RESET));
	ret = __dsp_hw_p0_system_wait_reset(sys);
	if (ret)
		sys->ctrl.ops->force_reset(&sys->ctrl);
	else
		sys->ctrl.ops->reset(&sys->ctrl);
	sys->pm.ops->update_devfreq_min(&sys->pm);

	sys->mailbox.ops->stop(&sys->mailbox);
	sys->log.ops->stop(&sys->log);
	sys->boot_init &= ~BIT(DSP_SYSTEM_DSP_INIT);
	sys->imgloader.ops->shutdown(&sys->imgloader);
	dsp_leave();
	return 0;
}

static int __dsp_hw_p0_system_binary_load(struct dsp_system *sys)
{
	int ret;
	struct dsp_memory *mem;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	mem = &sys->memory;

	pmem = &mem->priv_mem[DSP_P0_PRIV_MEM_FW];
	ret = dsp_binary_load(DSP_P0_FW_NAME, sys->fw_postfix,
			DSP_P0_FW_EXTENSION, pmem->bac_kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_P0_PRIV_MEM_IVP_PM];
	ret = dsp_binary_load(DSP_P0_IVP_PM_NAME, sys->fw_postfix,
			DSP_P0_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	pmem = &mem->priv_mem[DSP_P0_PRIV_MEM_IVP_DM];
	ret = dsp_binary_load(DSP_P0_IVP_DM_NAME, sys->fw_postfix,
			DSP_P0_FW_EXTENSION, pmem->kvaddr, pmem->size,
			&pmem->used_size);
	if (ret)
		goto p_err_load;

	dsp_leave();
	return 0;
p_err_load:
	return ret;
}

static int dsp_hw_p0_system_start(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_p0_system_binary_load(sys);
	if (ret)
		goto p_err;

	ret = dsp_hw_common_system_start(sys);
	if (ret) {
		dsp_err("Failed to start common system(%d)\n", ret);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_p0_system_timeout_handler(struct dsp_system *sys,
		unsigned int wait_id)
{
	bool check;

	dsp_enter();
	if ((sys->debug_mode & BIT(DSP_DEBUG_MODE_TASK_TIMEOUT_PANIC)) &&
			(wait_id == DSP_SYSTEM_WAIT_MAILBOX))
		check = true;
	else if ((sys->debug_mode & BIT(DSP_DEBUG_MODE_BOOT_TIMEOUT_PANIC)) &&
			(wait_id == DSP_SYSTEM_WAIT_BOOT))
		check = true;
	else if ((sys->debug_mode & BIT(DSP_DEBUG_MODE_RESET_TIMEOUT_PANIC)) &&
			(wait_id == DSP_SYSTEM_WAIT_RESET))
		check = true;
	else
		check = false;

	if (check) {
		dsp_dump_ctrl();
		dsp_dump_task_count();
		dbg_snapshot_expire_watchdog();
		BUG();
	} else {
		dsp_dump_ctrl();
		dsp_dump_task_count();
	}

	dsp_leave();
	return 0;
}

static void __dsp_hw_p0_system_master_load_async(const struct firmware *fw,
		void *context)
{
	int ret, idx, retry = 10;
	struct dsp_system *sys;
	char full_name[DSP_BINARY_NAME_SIZE];
	struct dsp_reserved_mem *rmem;

	dsp_enter();
	sys = context;
	rmem = &sys->memory.reserved_mem[DSP_P0_RESERVED_MEM_FW_MASTER];

	snprintf(full_name, DSP_BINARY_NAME_SIZE, "%s.%s",
			DSP_P0_MASTER_FW_NAME, DSP_P0_FW_EXTENSION);

	if (!fw) {
		for (idx = 0; idx < retry; ++idx) {
#if KERNEL_VERSION(4, 18, 0) > LINUX_VERSION_CODE
			ret = request_firmware_direct(&fw, full_name, sys->dev);
#else
			ret = firmware_request_nowarn(&fw, full_name, sys->dev);
#endif
			if (ret >= 0)
				break;
			/* Wait for the file system to be mounted at boot time*/
			msleep(500);
		}
		if (ret < 0) {
			dsp_err("Failed to request binary[%s]\n", full_name);
			return;
		}
	}

	if (rmem->size < fw->size) {
		dsp_err("master bin size is over(%zu/%zu)\n",
				rmem->size, fw->size);
		release_firmware(fw);
		return;
	}

	memcpy(rmem->kvaddr, fw->data, fw->size);
	rmem->used_size = fw->size;
	sys->boot_bin_size = fw->size;
	release_firmware(fw);
	dsp_info("binary[%s/%zu] is loaded\n", full_name, sys->boot_bin_size);
	dsp_leave();
}

static int dsp_hw_p0_system_probe(struct dsp_system *sys, void *dspdev)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_system_probe(sys, dspdev,
			dsp_hw_p0_system_timeout_handler);
	if (ret) {
		dsp_err("Failed to probe common system(%d)\n", ret);
		goto p_err;
	}

	ret = dsp_binary_load_async(DSP_P0_MASTER_FW_NAME, NULL,
			DSP_P0_FW_EXTENSION, sys,
			__dsp_hw_p0_system_master_load_async);
	if (ret < 0)
		goto p_err_bin_load;

	dsp_leave();
	return 0;
p_err_bin_load:
	dsp_hw_common_system_remove(sys);
p_err:
	return ret;
}

static void dsp_hw_p0_system_remove(struct dsp_system *sys)
{
	dsp_enter();
	dsp_hw_common_system_remove(sys);
	dsp_leave();
}

static const struct dsp_system_ops p0_system_ops = {
	.request_control	= dsp_hw_common_system_request_control,
	.execute_task		= dsp_hw_common_system_execute_task,
	.iovmm_fault_dump	= dsp_hw_p0_system_iovmm_fault_dump,
	.boot			= dsp_hw_p0_system_boot,
	.reset			= dsp_hw_p0_system_reset,
	.power_active		= dsp_hw_common_system_power_active,
	.set_boot_qos		= dsp_hw_common_system_set_boot_qos,
	.runtime_resume		= dsp_hw_common_system_runtime_resume,
	.runtime_suspend	= dsp_hw_common_system_runtime_suspend,
	.resume			= dsp_hw_common_system_resume,
	.suspend		= dsp_hw_common_system_suspend,

	.npu_start		= dsp_hw_common_system_npu_start,
	.start			= dsp_hw_p0_system_start,
	.stop			= dsp_hw_common_system_stop,

	.open			= dsp_hw_common_system_open,
	.close			= dsp_hw_common_system_close,
	.probe			= dsp_hw_p0_system_probe,
	.remove			= dsp_hw_p0_system_remove,
};

int dsp_hw_p0_system_register_ops(void)
{
	int ret;
	int (*ops_list[])(void) = {
		dsp_hw_p0_pm_register_ops,
		dsp_hw_p0_clk_register_ops,
		dsp_hw_p0_bus_register_ops,
		dsp_hw_p0_llc_register_ops,
		dsp_hw_p0_memory_register_ops,
		dsp_hw_p0_interface_register_ops,
		dsp_hw_p0_ctrl_register_ops,
		dsp_hw_p0_mailbox_register_ops,
		dsp_hw_p0_governor_register_ops,
		dsp_hw_p0_imgloader_register_ops,
		dsp_hw_p0_sysevent_register_ops,
		dsp_hw_p0_memlogger_register_ops,
		dsp_hw_p0_log_register_ops,
		dsp_hw_p0_debug_register_ops,
		dsp_hw_p0_dump_register_ops,
	};
	size_t size, idx;

	dsp_enter();
	size = ARRAY_SIZE(ops_list);
	if (size != DSP_HW_OPS_COUNT - 1) {
		ret = -EINVAL;
		dsp_err("size of ops_list is not enough(%zu/%zu)\n",
				size, DSP_HW_OPS_COUNT - 1);
		goto p_err;
	}

	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_SYSTEM,
			&p0_system_ops, sizeof(p0_system_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	for (idx = 0; idx < size; ++idx) {
		ret = ops_list[idx]();
		if (ret) {
			dsp_err("Failed to register ops at system(%zu/%zu)\n",
					idx, size);
			goto p_err;
		}
	}

	dsp_info("hw_p0(%u) was initilized\n", DSP_DEVICE_ID_P0);
	dsp_leave();
	return 0;
p_err:
	return ret;
}
