// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>

#include "dsp-log.h"
#include "dsp-dump.h"
#include "dsp-device.h"
#include "dsp-hw-common-system.h"

int dsp_hw_common_system_request_control(struct dsp_system *sys,
		unsigned int id, union dsp_control *cmd)
{
	int ret;

	dsp_enter();
	switch (id) {
	case DSP_CONTROL_ENABLE_DVFS:
		ret = sys->pm.ops->dvfs_enable(&sys->pm, cmd->dvfs.pm_qos);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_DISABLE_DVFS:
		ret = sys->pm.ops->dvfs_disable(&sys->pm, cmd->dvfs.pm_qos);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_ENABLE_BOOST:
		mutex_lock(&sys->boost_lock);
		if (!sys->boost) {
			ret = sys->pm.ops->boost_enable(&sys->pm);
			if (ret) {
				mutex_unlock(&sys->boost_lock);
				goto p_err;
			}

			ret = sys->bus.ops->mo_get_by_id(&sys->bus,
					sys->bus.max_scen_id);
			if (ret) {
				sys->pm.ops->boost_disable(&sys->pm);
				mutex_unlock(&sys->boost_lock);
				goto p_err;
			}

			sys->boost = true;
		}
		mutex_unlock(&sys->boost_lock);
		break;
	case DSP_CONTROL_DISABLE_BOOST:
		mutex_lock(&sys->boost_lock);
		if (sys->boost) {
			sys->bus.ops->mo_put_by_id(&sys->bus,
					sys->bus.max_scen_id);
			sys->pm.ops->boost_disable(&sys->pm);
			sys->boost = false;
		}
		mutex_unlock(&sys->boost_lock);
		break;
	case DSP_CONTROL_REQUEST_MO:
		ret = sys->bus.ops->mo_get_by_name(&sys->bus,
				cmd->mo.scenario_name);
		if (ret)
			goto p_err;
		break;
	case DSP_CONTROL_RELEASE_MO:
		sys->bus.ops->mo_put_by_name(&sys->bus, cmd->mo.scenario_name);
		break;
	case DSP_CONTROL_REQUEST_PRESET:
		sys->governor.ops->request(&sys->governor, &cmd->preset);
		break;
	default:
		ret = -EINVAL;
		dsp_err("control cmd id is invalid(%u)\n", id);
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_common_system_wait_task(struct dsp_system *sys,
		struct dsp_task *task)
{
	int ret;
	unsigned int wait_time;
	long timeout;

	dsp_enter();
	wait_time = sys->wait[DSP_SYSTEM_WAIT_MAILBOX];
#ifndef ENABLE_DSP_VELOCE
	dsp_dbg("wait mode : %u / task : %u/%u\n",
			sys->wait_mode, task->id, task->message_id);
	if (sys->wait_mode == DSP_SYSTEM_WAIT_MODE_INTERRUPT) {
		timeout = dsp_task_manager_wait_done(task,
				msecs_to_jiffies(wait_time));
		if (!timeout) {
			sys->interface.ops->check_irq(&sys->interface);
			if (task->state == DSP_TASK_STATE_COMPLETE) {
				timeout = 1;
				dsp_warn("interrupt was unstable\n");
			}
		}
	} else if (sys->wait_mode == DSP_SYSTEM_WAIT_MODE_BUSY_WAITING) {
		timeout = wait_time * 1000;

		while (timeout) {
			if (task->state == DSP_TASK_STATE_COMPLETE)
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
		if (task->state == DSP_TASK_STATE_COMPLETE)
			break;

		dsp_ctrl_pc_dump();
		dsp_ctrl_userdefined_dump();
		mdelay(100);
	}
	timeout = 1;
#endif
	if (!timeout) {
		ret = -ETIMEDOUT;
		dsp_err("task wait time(%ums) is expired(%u/%u)\n",
				sys->wait[DSP_SYSTEM_WAIT_MAILBOX],
				task->id, task->message_id);
		if (sys->timeout_handler) {
			sys->timeout_handler(sys, DSP_SYSTEM_WAIT_MAILBOX);
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

static void __dsp_hw_common_system_flush(struct dsp_system *sys)
{
	unsigned long flags;

	dsp_enter();
	dsp_task_manager_lock(&flags);
	dsp_task_manager_set_block_mode(true);
	dsp_task_manager_flush_process(-ENOSTR);
	dsp_task_manager_unlock(flags);

	sys->ops->reset(sys);
	dsp_leave();
}

static void __dsp_hw_common_system_recovery(struct dsp_system *sys)
{
	int ret;
	unsigned long flags;

	dsp_enter();
	ret = sys->ops->boot(sys);
	if (ret) {
		dsp_err("Failed to recovery device\n");
		return;
	}

	dsp_graph_manager_recovery(&sys->dspdev->core.graph_manager);

	dsp_task_manager_lock(&flags);
	dsp_task_manager_set_block_mode(false);
	dsp_task_manager_unlock(flags);
	dsp_leave();
}

int dsp_hw_common_system_execute_task(struct dsp_system *sys,
		struct dsp_task *task)
{
	int ret;
	struct dsp_mailbox_pool *pool;

	dsp_enter();
	pool = task->pool;

	if (task->message_id == DSP_COMMON_EXECUTE_MSG)
		sys->governor.ops->check_done(&sys->governor);

	sys->pm.ops->update_devfreq_busy(&sys->pm, pool->pm_qos);
	ret = sys->mailbox.ops->send_task(&sys->mailbox, task);
	if (ret)
		goto p_err;

	dsp_dump_mailbox_pool_debug(pool);

	/* TODO Devfreq change criteria required if not waiting */
	if (task->wait) {
		ret = __dsp_hw_common_system_wait_task(sys, task);
		if (ret) {
			if (task->recovery) {
				__dsp_hw_common_system_flush(sys);
				__dsp_hw_common_system_recovery(sys);
			}
			goto p_err;
		}

		if (task->result) {
			ret = task->result;
			dsp_err("task result is failure(%d/%u/%u)\n",
					ret, task->id, task->message_id);
			goto p_err;
		}
	}

	sys->pm.ops->update_devfreq_idle(&sys->pm, pool->pm_qos);
	task->owner->normal_count++;
	dsp_leave();
	return 0;
p_err:
	sys->pm.ops->update_devfreq_idle(&sys->pm, pool->pm_qos);
	task->owner->error_count++;
	dsp_dump_mailbox_pool_error(pool);
	dsp_dump_task_count();
	dsp_dump_kernel();
	return ret;
}

void dsp_hw_common_system_iovmm_fault_dump(struct dsp_system *sys)
{
	dsp_enter();
	dsp_dump_ctrl();
	dsp_dump_task_count();
	dsp_dump_kernel();
	dsp_leave();
}

int dsp_hw_common_system_power_active(struct dsp_system *sys)
{
	dsp_check();
	return sys->pm.ops->devfreq_active(&sys->pm);
}

int dsp_hw_common_system_set_boot_qos(struct dsp_system *sys, int val)
{
	int ret;

	dsp_enter();
	ret = sys->pm.ops->set_boot_qos(&sys->pm, val);
	if (ret)
		goto p_err;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_system_runtime_resume(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	ret = sys->pm.ops->enable(&sys->pm);
	if (ret)
		goto p_err_pm;

	ret = sys->clk.ops->enable(&sys->clk);
	if (ret)
		goto p_err_clk;

	sys->sysevent.ops->get(&sys->sysevent);

	dsp_leave();
	return 0;
p_err_clk:
	sys->pm.ops->disable(&sys->pm);
p_err_pm:
	return ret;
}

int dsp_hw_common_system_runtime_suspend(struct dsp_system *sys)
{
	dsp_enter();
	sys->sysevent.ops->put(&sys->sysevent);
	sys->governor.ops->flush_work(&sys->governor);
	sys->clk.ops->disable(&sys->clk);
	sys->pm.ops->disable(&sys->pm);
	dsp_leave();
	return 0;
}

int dsp_hw_common_system_resume(struct dsp_system *sys)
{
	dsp_enter();
	__dsp_hw_common_system_recovery(sys);
	dsp_leave();
	return 0;
}

int dsp_hw_common_system_suspend(struct dsp_system *sys)
{
	dsp_enter();
	__dsp_hw_common_system_flush(sys);
	dsp_leave();
	return 0;
}

int dsp_hw_common_system_npu_start(struct dsp_system *sys, bool boot,
		dma_addr_t fw_iova)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_common_system_start(struct dsp_system *sys)
{
	int ret;
	unsigned long flags;

	dsp_enter();
	ret = sys->interface.ops->start(&sys->interface);
	if (ret)
		goto p_err_interface;

	dsp_task_manager_lock(&flags);
	dsp_task_manager_set_block_mode(false);
	dsp_task_manager_unlock(flags);

	dsp_leave();
	return 0;
p_err_interface:
	return ret;
}

int dsp_hw_common_system_stop(struct dsp_system *sys)
{
	dsp_enter();
	sys->interface.ops->stop(&sys->interface);
	dsp_leave();
	return 0;
}

int dsp_hw_common_system_open(struct dsp_system *sys)
{
	int ret;

	dsp_enter();
	ret = sys->pm.ops->open(&sys->pm);
	if (ret)
		goto p_err_pm;

	ret = sys->clk.ops->open(&sys->clk);
	if (ret)
		goto p_err_clk;

	ret = sys->bus.ops->open(&sys->bus);
	if (ret)
		goto p_err_bus;

	ret = sys->llc.ops->open(&sys->llc);
	if (ret)
		goto p_err_llc;

	ret = sys->memory.ops->open(&sys->memory);
	if (ret)
		goto p_err_memory;

	ret = sys->interface.ops->open(&sys->interface);
	if (ret)
		goto p_err_interface;

	ret = sys->ctrl.ops->open(&sys->ctrl);
	if (ret)
		goto p_err_ctrl;

	ret = sys->mailbox.ops->open(&sys->mailbox);
	if (ret)
		goto p_err_mbox;

	ret = sys->governor.ops->open(&sys->governor);
	if (ret)
		goto p_err_governor;

	dsp_leave();
	return 0;
p_err_governor:
	sys->mailbox.ops->close(&sys->mailbox);
p_err_mbox:
	sys->ctrl.ops->close(&sys->ctrl);
p_err_ctrl:
	sys->interface.ops->close(&sys->interface);
p_err_interface:
	sys->memory.ops->close(&sys->memory);
p_err_memory:
	sys->llc.ops->close(&sys->llc);
p_err_llc:
	sys->bus.ops->close(&sys->bus);
p_err_bus:
	sys->clk.ops->close(&sys->clk);
p_err_clk:
	sys->pm.ops->close(&sys->pm);
p_err_pm:
	return ret;
}

int dsp_hw_common_system_close(struct dsp_system *sys)
{
	dsp_enter();
	sys->governor.ops->close(&sys->governor);
	sys->mailbox.ops->close(&sys->mailbox);
	sys->ctrl.ops->close(&sys->ctrl);
	sys->interface.ops->close(&sys->interface);
	sys->memory.ops->close(&sys->memory);
	sys->llc.ops->close(&sys->llc);
	sys->bus.ops->close(&sys->bus);
	sys->clk.ops->close(&sys->clk);
	sys->pm.ops->close(&sys->pm);
	dsp_leave();
	return 0;
}

int dsp_hw_common_system_probe(struct dsp_system *sys, void *dspdev,
		timeout_handler_t handler)
{
	int ret;
	struct platform_device *pdev;
	struct resource *res;
	void __iomem *regs;

	dsp_enter();
	sys->dspdev = dspdev;
	sys->dev = sys->dspdev->dev;
	pdev = to_platform_device(sys->dev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		dsp_err("Failed to get resource0\n");
		goto p_err_resource0;
	}

	regs = devm_ioremap_resource(sys->dev, res);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap resource0(%d)\n", ret);
		goto p_err_ioremap0;
	}

	sys->sfr_pa = res->start;
	sys->sfr = regs;
	sys->sfr_size = resource_size(res);

	regs = devm_ioremap(sys->dev, 0x10000000, 0x4);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap product_id(%d)\n", ret);
		goto p_err_product_id;
	}
	sys->product_id = regs;

	/*
	 * CHIPID_REV[31:24] Reserved
	 * CHIPID_REV[23:20] Main revision
	 * CHIPID_REV[19:16] Sub revision
	 * CHIPID_REV[15:0]  Reserved
	 */
	regs = devm_ioremap(sys->dev, 0x10000010, 0x4);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap chip_id(%d)\n", ret);
		goto p_err_chipid;
	}
	sys->chip_id = regs;

	init_waitqueue_head(&sys->system_wq);

	sys->wait[DSP_SYSTEM_WAIT_BOOT] = DSP_WAIT_BOOT_TIME;
	sys->wait[DSP_SYSTEM_WAIT_MAILBOX] = DSP_WAIT_MAILBOX_TIME;
	sys->wait[DSP_SYSTEM_WAIT_RESET] = DSP_WAIT_RESET_TIME;
	sys->wait_mode = DSP_SYSTEM_WAIT_MODE_INTERRUPT;
	sys->boost = false;
	mutex_init(&sys->boost_lock);

	sys->layer_start = DSP_SET_DEFAULT_LAYER;
	sys->layer_end = DSP_SET_DEFAULT_LAYER;

	ret = sys->pm.ops->probe(&sys->pm, sys);
	if (ret)
		goto p_err_pm;

	ret = sys->clk.ops->probe(&sys->clk, sys);
	if (ret)
		goto p_err_clk;

	ret = sys->bus.ops->probe(&sys->bus, sys);
	if (ret)
		goto p_err_bus;

	ret = sys->llc.ops->probe(&sys->llc, sys);
	if (ret)
		goto p_err_llc;

	ret = sys->memory.ops->probe(&sys->memory, sys);
	if (ret)
		goto p_err_memory;

	ret = sys->interface.ops->probe(&sys->interface, sys);
	if (ret)
		goto p_err_interface;

	ret = sys->ctrl.ops->probe(&sys->ctrl, sys);
	if (ret)
		goto p_err_ctrl;

	ret = sys->mailbox.ops->probe(&sys->mailbox, sys);
	if (ret)
		goto p_err_mbox;

	ret = sys->governor.ops->probe(&sys->governor, sys);
	if (ret)
		goto p_err_governor;

	ret = sys->imgloader.ops->probe(&sys->imgloader, sys);
	if (ret)
		goto p_err_imgloader;

	ret = sys->sysevent.ops->probe(&sys->sysevent, sys);
	if (ret)
		goto p_err_sysevent;

	ret = sys->memlogger.ops->probe(&sys->memlogger, sys);
	if (ret)
		goto p_err_memlogger;

	ret = sys->log.ops->probe(&sys->log, sys);
	if (ret)
		goto p_err_log;

	ret = sys->dump.ops->probe(&sys->dump, sys);
	if (ret)
		goto p_err_dump;

	if (handler)
		sys->timeout_handler = handler;

	dsp_leave();
	return 0;
p_err_dump:
	sys->log.ops->remove(&sys->log);
p_err_log:
	sys->memlogger.ops->remove(&sys->memlogger);
p_err_memlogger:
	sys->sysevent.ops->remove(&sys->sysevent);
p_err_sysevent:
	sys->imgloader.ops->remove(&sys->imgloader);
p_err_imgloader:
	sys->governor.ops->remove(&sys->governor);
p_err_governor:
	sys->mailbox.ops->remove(&sys->mailbox);
p_err_mbox:
	sys->ctrl.ops->remove(&sys->ctrl);
p_err_ctrl:
	sys->interface.ops->remove(&sys->interface);
p_err_interface:
	sys->memory.ops->remove(&sys->memory);
p_err_memory:
	sys->llc.ops->remove(&sys->llc);
p_err_llc:
	sys->bus.ops->remove(&sys->bus);
p_err_bus:
	sys->clk.ops->remove(&sys->clk);
p_err_clk:
	sys->pm.ops->remove(&sys->pm);
p_err_pm:
	devm_iounmap(sys->dev, sys->chip_id);
p_err_chipid:
	devm_iounmap(sys->dev, sys->product_id);
p_err_product_id:
	devm_iounmap(sys->dev, sys->sfr);
p_err_ioremap0:
p_err_resource0:
	return ret;
}

void dsp_hw_common_system_remove(struct dsp_system *sys)
{
	dsp_enter();
	sys->dump.ops->remove(&sys->dump);
	sys->log.ops->remove(&sys->log);
	sys->memlogger.ops->remove(&sys->memlogger);
	sys->sysevent.ops->remove(&sys->sysevent);
	sys->imgloader.ops->remove(&sys->imgloader);
	sys->governor.ops->remove(&sys->governor);
	sys->mailbox.ops->remove(&sys->mailbox);
	sys->ctrl.ops->remove(&sys->ctrl);
	sys->interface.ops->remove(&sys->interface);
	sys->memory.ops->remove(&sys->memory);
	sys->llc.ops->remove(&sys->llc);
	sys->bus.ops->remove(&sys->bus);
	sys->clk.ops->remove(&sys->clk);
	sys->pm.ops->remove(&sys->pm);
	devm_iounmap(sys->dev, sys->chip_id);
	devm_iounmap(sys->dev, sys->product_id);
	devm_iounmap(sys->dev, sys->sfr);
	dsp_leave();
}

int dsp_hw_common_system_set_ops(struct dsp_system *sys, const void *ops)
{
	dsp_enter();
	sys->ops = ops;
	dsp_leave();
	return 0;
}
