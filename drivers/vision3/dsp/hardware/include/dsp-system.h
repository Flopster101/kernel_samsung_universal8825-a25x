/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_SYSTEM_H__
#define __HW_DSP_SYSTEM_H__

#include <linux/device.h>
#include <linux/wait.h>

#include "dsp-pm.h"
#include "dsp-clk.h"
#include "dsp-bus.h"
#include "dsp-llc.h"
#include "dsp-memory.h"
#include "dsp-interface.h"
#include "dsp-ctrl.h"
#include "dsp-task.h"
#include "dsp-mailbox.h"
#include "dsp-governor.h"
#include "dsp-imgloader.h"
#include "dsp-sysevent.h"
#include "dsp-memlogger.h"
#include "dsp-hw-log.h"
#include "dsp-hw-debug.h"
#include "dsp-hw-dump.h"
#include "dsp-control.h"

#define DSP_SET_DEFAULT_LAYER	(0xffffffff)

#define DSP_WAIT_BOOT_TIME	(100)
#define DSP_WAIT_MAILBOX_TIME	(1500)
#define DSP_WAIT_RESET_TIME	(100)

#define DSP_STATIC_KERNEL	(1)
#define DSP_DYNAMIC_KERNEL	(2)

struct dsp_device;
struct dsp_system;

enum dsp_system_flag {
	DSP_SYSTEM_BOOT,
	DSP_SYSTEM_RESET,
};

enum dsp_system_boot_init {
	DSP_SYSTEM_DSP_INIT,
	DSP_SYSTEM_NPU_INIT,
};

enum dsp_system_wait {
	DSP_SYSTEM_WAIT_BOOT,
	DSP_SYSTEM_WAIT_MAILBOX,
	DSP_SYSTEM_WAIT_RESET,
	DSP_SYSTEM_WAIT_NUM,
};

enum dsp_system_wait_mode {
	DSP_SYSTEM_WAIT_MODE_INTERRUPT,
	DSP_SYSTEM_WAIT_MODE_BUSY_WAITING,
	DSP_SYSTEM_WAIT_MODE_NUM,
};

typedef int (*timeout_handler_t)(struct dsp_system *, unsigned int);

struct dsp_system_ops {
	int (*request_control)(struct dsp_system *sys, unsigned int id,
			union dsp_control *cmd);
	int (*execute_task)(struct dsp_system *sys, struct dsp_task *task);
	void (*iovmm_fault_dump)(struct dsp_system *sys);
	int (*boot)(struct dsp_system *sys);
	int (*reset)(struct dsp_system *sys);
	int (*power_active)(struct dsp_system *sys);
	int (*set_boot_qos)(struct dsp_system *sys, int val);
	int (*runtime_resume)(struct dsp_system *sys);
	int (*runtime_suspend)(struct dsp_system *sys);
	int (*resume)(struct dsp_system *sys);
	int (*suspend)(struct dsp_system *sys);

	int (*npu_start)(struct dsp_system *sys, bool boot, dma_addr_t fw_iova);
	int (*start)(struct dsp_system *sys);
	int (*stop)(struct dsp_system *sys);

	int (*open)(struct dsp_system *sys);
	int (*close)(struct dsp_system *sys);
	int (*probe)(struct dsp_system *sys, void *dspdev);
	void (*remove)(struct dsp_system *sys);
};

struct dsp_system {
	struct device			*dev;
	phys_addr_t			sfr_pa;
	void __iomem			*sfr;
	resource_size_t			sfr_size;
	void __iomem			*product_id;
	void __iomem			*chip_id;
	void				*sub_data;

	unsigned long			boot_init;
	size_t				boot_bin_size;
	wait_queue_head_t		system_wq;
	unsigned int			system_flag;
	unsigned int			wait[DSP_SYSTEM_WAIT_NUM];
	unsigned int			wait_mode;
	bool				boost;
	struct mutex			boost_lock;
	char				fw_postfix[32];
	unsigned int			layer_start;
	unsigned int			layer_end;
	unsigned int			debug_mode;

	timeout_handler_t		timeout_handler;
	const struct dsp_system_ops	*ops;
	struct dsp_pm			pm;
	struct dsp_clk			clk;
	struct dsp_bus			bus;
	struct dsp_llc			llc;
	struct dsp_memory		memory;
	struct dsp_interface		interface;
	struct dsp_ctrl			ctrl;
	struct dsp_mailbox		mailbox;
	struct dsp_governor		governor;
	struct dsp_imgloader		imgloader;
	struct dsp_sysevent		sysevent;
	struct dsp_memlogger		memlogger;
	struct dsp_hw_log		log;
	struct dsp_hw_debug		debug;
	struct dsp_hw_dump		dump;
	struct dsp_device		*dspdev;
};

#endif
