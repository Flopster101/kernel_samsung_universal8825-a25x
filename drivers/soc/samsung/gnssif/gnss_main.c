// SPDX-License-Identifier: GPL-2.0
/* linux/drivers/misc/gnss/gnss_main.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/if_arp.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/clk.h>
#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#endif

#include "gnss_mbox.h"
#include "gnss_prj.h"
#include "gnss_utils.h"

static struct gnss_ctl *create_ctl_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gnss_pdata *pdata = pdev->dev.platform_data;
	struct gnss_ctl *gnssctl;
	struct clk *qch_clk;
	int ret;

	/* create GNSS control device */
	gnssctl = devm_kzalloc(dev, sizeof(struct gnss_ctl), GFP_KERNEL);
	if (!gnssctl) {
		gif_err("%s: gnssctl devm_kzalloc fail\n", pdata->name);
		return NULL;
	}

	gnssctl->dev = dev;
	gnssctl->gnss_state = STATE_OFFLINE;

	gnssctl->pdata = pdata;
	gnssctl->name = pdata->name;

	qch_clk = devm_clk_get(dev, "ccore_qch_lh_gnss");
	if (!IS_ERR(qch_clk)) {
		gif_err("Found Qch clk!\n");
		gnssctl->ccore_qch_lh_gnss = qch_clk;
	} else {
		gnssctl->ccore_qch_lh_gnss = NULL;
	}

	/* init gnssctl device for getting gnssctl operations */
	ret = init_gnssctl_device(gnssctl, pdata);
	if (ret) {
		gif_err("%s: init_gnssctl_device fail (err %d)\n",
			pdata->name, ret);
		devm_kfree(dev, gnssctl);
		return NULL;
	}

	gnssctl->reset_count = 0;

	gif_info("%s is created!!!\n", pdata->name);

	return gnssctl;
}

static struct io_device *create_io_device(struct platform_device *pdev,
		struct gnss_io_t *io_t, struct link_device *ld,
		struct gnss_ctl *gnssctl, struct gnss_pdata *pdata)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct io_device *iod;

	iod = devm_kzalloc(dev, sizeof(struct io_device), GFP_KERNEL);
	if (!iod) {
		gif_err("iod is NULL\n");
		return NULL;
	}

	iod->name = io_t->name;
	iod->app = io_t->app;
	atomic_set(&iod->opened, 0);

	/* link between io device and gnss control */
	iod->gc = gnssctl;
	gnssctl->iod = iod;

	/* link between io device and link device */
	iod->ld = ld;
	ld->iod = iod;

	/* register misc device */
	ret = exynos_init_gnss_io_device(iod, dev);
	if (ret) {
		devm_kfree(dev, iod);
		gif_err("exynos_init_gnss_io_device fail (%d)\n", ret);
		return NULL;
	}

	gif_info("%s created\n", iod->name);
	return iod;
}

#if IS_ENABLED(CONFIG_OF_RESERVED_MEM)

#if defined(MODULE)
static int gnss_rmem_setup_latecall(struct platform_device *pdev)
{
	struct device_node *np;
	struct reserved_mem *rmem;
	struct gnss_pdata *pdata = (struct gnss_pdata *)pdev->dev.platform_data;

	np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!np)
		return -EINVAL;

	rmem = of_reserved_mem_lookup(np);
	if (!rmem) {
		gif_err("of_reserved_mem_lookup() failed\n");
		return -EINVAL;
	}

	pdata->shmem_base = rmem->base;
	pdata->shmem_size = rmem->size;

	gif_info("rmem 0x%llX 0x%08X\n", pdata->shmem_base, pdata->shmem_size);

	return 0;
}
#else
static int gnss_dma_device_init(struct reserved_mem *rmem, struct device *dev)
{
	struct gnss_pdata *pdata;
	if (!dev && !dev->platform_data)
		return -ENODEV;

	/* Save reserved memory information. */
	pdata = (struct gnss_pdata *)dev->platform_data;
	pdata->shmem_base = rmem->base;
	pdata->shmem_size = rmem->size;

	return 0;
}

static void gnss_dma_device_release(struct reserved_mem *rmem, struct device *dev)
{
	return;
}

static const struct reserved_mem_ops gnss_dma_ops = {
	.device_init	= gnss_dma_device_init,
	.device_release	= gnss_dma_device_release,
};

static int gnss_if_reserved_mem_setup(struct reserved_mem *remem)
{
	gif_info("%s: memory reserved: paddr=%#lx, t_size=%zd\n",
		__func__, (unsigned long)remem->base, (size_t)remem->size);
	remem->ops = &gnss_dma_ops;

	return 0;
}
RESERVEDMEM_OF_DECLARE(gnss_rmem, "samsung,exynos-gnss", gnss_if_reserved_mem_setup);
#endif

#endif

#if IS_ENABLED(CONFIG_OF)
static int parse_dt_common_pdata(struct device_node *np,
					struct gnss_pdata *pdata)
{
	gif_dt_read_string(np, "shmem,name", pdata->name);
	gif_dt_read_string(np, "device_node_name", pdata->device_node_name);
	gif_dt_read_u32(np, "shmem,code_offset", pdata->code_offset);
	gif_dt_read_u32(np, "shmem,code_allowed_size", pdata->code_allowed_size);
	gif_dt_read_u32(np, "shmem,ipc_offset", pdata->ipc_offset);
	gif_dt_read_u32(np, "shmem,ipc_size", pdata->ipc_size);
	gif_dt_read_u32(np, "shmem,ipc_rx_offset", pdata->ipc_rx_offset);
	gif_dt_read_u32(np, "shmem,ipc_rx_size", pdata->ipc_rx_size);
	gif_dt_read_u32(np, "shmem,ipc_tx_offset", pdata->ipc_tx_offset);
	gif_dt_read_u32(np, "shmem,ipc_tx_size", pdata->ipc_tx_size);
	gif_dt_read_u32(np, "shmem,ipc_reg_offset", pdata->ipc_reg_offset);
	gif_dt_read_u32(np, "shmem,ipc_reg_size", pdata->ipc_reg_size);

#if IS_ENABLED(CONFIG_SOC_S5E9925)
	gif_dt_read_u32(np, "shmem,base_addr", pdata->base_addr);
	gif_dt_read_u32(np, "shmem,base_addr_2nd", pdata->base_addr_2nd);
#endif

	return 0;
}

static int parse_dt_mbox_pdata(struct device *dev, struct device_node *np,
					struct gnss_pdata *pdata)
{
	struct gnss_mbox *mbox = pdata->mbx;
	struct device_node *mbox_info;

	mbox = devm_kzalloc(dev, sizeof(struct gnss_mbox), GFP_KERNEL);
	if (!mbox) {
		gif_err("mbox: failed to alloc memory\n");
		return -ENOMEM;
	}
	pdata->mbx = mbox;

	mbox_info = of_parse_phandle(np, "mbox_info", 0);
	if (IS_ERR(mbox_info)) {
		mbox->id = MBOX_REGION_GNSS;
	} else {
		gif_dt_read_u32(mbox_info, "mbox,id", mbox->id);
		of_node_put(mbox_info);
	}

	gif_dt_read_u32(np, "mbx,int_bcmd", mbox->int_bcmd);
	gif_dt_read_u32(np, "mbx,int_req_fault_info",
			mbox->int_req_fault_info);
	gif_dt_read_u32(np, "mbx,int_ipc_msg", mbox->int_ipc_msg);
	gif_dt_read_u32(np, "mbx,int_ack_wake_set",
			mbox->int_ack_wake_set);

	gif_dt_read_u32(np, "mbx,irq_bcmd", mbox->irq_bcmd);
	gif_dt_read_u32(np, "mbx,irq_rsp_fault_info",
			mbox->irq_rsp_fault_info);
	gif_dt_read_u32(np, "mbx,irq_ipc_msg", mbox->irq_ipc_msg);
	gif_dt_read_u32(np, "mbx,irq_req_wake_clr",
			mbox->irq_req_wake_clr);
	gif_dt_read_u32(np, "mbx,irq_simple_lock", mbox->irq_simple_lock);

	gif_dt_read_u32_array(np, "mbx,reg_bcmd_ctrl", mbox->reg_bcmd_ctrl,
			      BCMD_CTRL_COUNT);

	return 0;
}

static int alloc_gnss_reg(struct device *dev, struct gnss_shared_reg **areg,
		const char *reg_name, u32 reg_device, u32 reg_value)
{
	struct gnss_shared_reg *ret = NULL;
	if (!(*areg)) {
		ret = devm_kzalloc(dev, sizeof(struct gnss_shared_reg), GFP_KERNEL);
		if (ret) {
			ret->name = reg_name;
			ret->device = reg_device;
			ret->value.index = reg_value;
			*areg = ret;
		}
	} else {
		gif_err("Register %s is already allocated!\n", reg_name);
	}
	return (*areg == NULL);
}

const char *dt_reg_prop_table[GNSS_REG_COUNT] = {
	[GNSS_REG_RX_IPC_MSG] = "reg_rx_ipc_msg",
	[GNSS_REG_TX_IPC_MSG] = "reg_tx_ipc_msg",
	[GNSS_REG_RX_HEAD] = "reg_rx_head",
	[GNSS_REG_RX_TAIL] = "reg_rx_tail",
	[GNSS_REG_TX_HEAD] = "reg_tx_head",
	[GNSS_REG_TX_TAIL] = "reg_tx_tail",
};

static int parse_dt_reg_mbox_pdata(struct device *dev, struct gnss_pdata *pdata)
{
	int i;
	unsigned int err;
	struct device_node *np = dev->of_node;
	u32 val[2];

	for (i = 0; i < GNSS_REG_COUNT; i++) {
		err = of_property_read_u32_array(np, dt_reg_prop_table[i],
				val, 2);
		if (ERR_PTR(err))
			continue;

		err = alloc_gnss_reg(dev, &pdata->reg[i], dt_reg_prop_table[i],
				val[0], val[1]);
		if (err)
			goto parse_dt_reg_nomem;
	}

	return 0;

parse_dt_reg_nomem:
	for (i = 0; i < GNSS_REG_COUNT; i++)
		if (pdata->reg[i])
			devm_kfree(dev, pdata->reg[i]);

	gif_err("reg: could not allocate register memory\n");
	return -ENOMEM;
}

static int parse_dt_fault_pdata(struct device *dev, struct gnss_pdata *pdata)
{
	struct device_node *np = dev->of_node;
	u32 tmp[3];

	if (!of_property_read_u32_array(np, "fault_info", tmp, 3)) {
		(pdata)->fault_info.name = "gnss_fault_info";
		(pdata)->fault_info.device = tmp[0];
		(pdata)->fault_info.value.index = tmp[1];
		(pdata)->fault_info.size = tmp[2];
	} else {
		return -EINVAL;
	}
	return 0;
}

static struct gnss_pdata *gnss_if_parse_dt_pdata(struct device *dev)
{
	struct gnss_pdata *pdata;
	int i;
	u32 ret;

	pdata = devm_kzalloc(dev, sizeof(struct gnss_pdata), GFP_KERNEL);
	if (!pdata) {
		gif_err("gnss_pdata: alloc fail\n");
		return ERR_PTR(-ENOMEM);
	}
	dev->platform_data = pdata;
#if !defined(MODULE)
	ret = of_reserved_mem_device_init(dev);
	if (ret != 0) {
		gif_err("Failed to parse reserved memory\n");
		goto parse_dt_pdata_err;
	}
#endif
	ret = parse_dt_common_pdata(dev->of_node, pdata);
	if (ret != 0) {
		gif_err("Failed to parse common pdata.\n");
		goto parse_dt_pdata_err;
	}

	ret = parse_dt_mbox_pdata(dev, dev->of_node, pdata);
	if (ret != 0) {
		gif_err("Failed to parse mailbox pdata.\n");
		goto parse_dt_pdata_err;
	}

	ret = parse_dt_reg_mbox_pdata(dev, pdata);
	if (ret != 0) {
		gif_err("Failed to parse mailbox register pdata.\n");
		goto parse_dt_pdata_err;
	}

	ret = parse_dt_fault_pdata(dev, pdata);
	if (ret != 0) {
		gif_err("Failed to parse fault info pdata.\n");
		goto parse_dt_pdata_err;
	}

	for (i = 0; i < GNSS_REG_COUNT; i++) {
		if (pdata->reg[i])
			gif_info("Found reg: [%d:%d] %s\n",
					pdata->reg[i]->device,
					pdata->reg[i]->value.index,
					pdata->reg[i]->name);
	}

	gif_info("Fault info: %s [%d:%d:%d]\n",
			pdata->fault_info.name,
			pdata->fault_info.device,
			pdata->fault_info.value.index,
			pdata->fault_info.size);

	gif_info("DT parse complete!\n");
	return pdata;

parse_dt_pdata_err:
	if (pdata)
		devm_kfree(dev, pdata);

	if (dev->platform_data)
		dev->platform_data = NULL;

	return ERR_PTR(-EINVAL);
}

static const struct of_device_id gnss_dt_match[] = {
	{ .compatible = "samsung,exynos-gnss", },
	{},
};
MODULE_DEVICE_TABLE(of, gnss_dt_match);
#else /* !CONFIG_OF */
static struct gnss_pdata *gnss_if_parse_dt_pdata(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}
#endif /* CONFIG_OF */

static ssize_t gnss_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gnss_ctl *gc = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", get_gnss_state_str(gc->gnss_state));
}

static DEVICE_ATTR_RO(gnss_status);

static struct attribute *gnss_attrs[] = {
	&dev_attr_gnss_status.attr,
	NULL,
};
ATTRIBUTE_GROUPS(gnss);

static int gnss_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gnss_pdata *pdata = dev->platform_data;
	struct gnss_ctl *ctl;
	struct io_device *iod;
	struct link_device *ld;
	unsigned int size;
	int ret = 0;

	gif_info("Exynos GNSS interface driver %s\n", get_gnssif_driver_version());

	gif_info("%s: +++\n", pdev->name);

	if (!dev->of_node) {
		gif_err("No DT data!\n");
		goto probe_fail;
	}

	pdata = gnss_if_parse_dt_pdata(dev);
	if (IS_ERR(pdata)) {
		gif_err("DT parse error!\n");
		return PTR_ERR(pdata);
	}

#if defined(MODULE)
	ret = gnss_rmem_setup_latecall(pdev);
	if (ret != 0) {
		gif_err("gnss_rmem_setup_latecall() error:%d\n", ret);
		return ret;
	}
#endif

	/* allocate iodev */
	size = sizeof(struct gnss_io_t);
	pdata->iodev = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!pdata->iodev) {
		gif_err("iodev: failed to alloc memory\n");
		return PTR_ERR(pdata);
	}

	/* GNSS uses one IO device and does not need to be parsed from DT. */
	pdata->iodev->name = pdata->device_node_name;
	pdata->iodev->id = 0; /* Fixed channel 0. */
	pdata->iodev->app = "SLL";

	/* create control device */
	ctl = create_ctl_device(pdev);
	if (!ctl) {
		gif_err("%s: Could not create CTL\n", pdata->name);
		goto probe_fail;
	}

	/* create link device */
	ld = create_link_device_shmem(pdev);
	if (!ld) {
		gif_err("%s: Could not create LD\n", pdata->name);
		goto free_gc;
	}

	ld->gc = ctl;

	/* create io device and connect to ctl device */
	iod = create_io_device(pdev, pdata->iodev, ld, ctl, pdata);
	if (!iod) {
		gif_err("%s: Could not create IOD\n", pdata->name);
		goto free_iod;
	}

	/* attach device */
	gif_info("set %s->%s\n", iod->name, ld->name);
	set_current_link(iod, iod->ld);

	platform_set_drvdata(pdev, ctl);

	if (sysfs_create_groups(&dev->kobj, gnss_groups))
		gif_err("failed to create gnss groups node\n");

	gif_err("%s: ---\n", pdata->name);

	return 0;

free_iod:
	devm_kfree(dev, iod);

free_gc:
	devm_kfree(dev, ctl);

probe_fail:

	gif_err("%s: xxx\n", pdata->name);

	return -ENOMEM;
}

static void gnss_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gnss_ctl *gc = dev_get_drvdata(dev);

	/* Matt - Implement Shutdown */
	gc->gnss_state = STATE_OFFLINE;
}

#if IS_ENABLED(CONFIG_PM)
static int gnss_suspend(struct device *pdev)
{
	struct gnss_ctl *gc = dev_get_drvdata(pdev);

	if (!gc->ops.suspend) {
		gif_err("ops.suspend is null\n");
		return -EPERM;
	}

	return gc->ops.suspend(gc);
}

static int gnss_resume(struct device *pdev)
{
	struct gnss_ctl *gc = dev_get_drvdata(pdev);

	if (!gc->ops.resume) {
		gif_err("ops.resume is null\n");
		return -EPERM;
	}

	return gc->ops.resume(gc);
}
#else
#define gnss_suspend	NULL
#define gnss_resume	NULL
#endif

static const struct dev_pm_ops gnss_pm_ops = {
	.suspend = gnss_suspend,
	.resume = gnss_resume,
};

static struct platform_driver gnss_driver = {
	.probe = gnss_probe,
	.shutdown = gnss_shutdown,
	.driver = {
		.name = "gnss_interface",
		.owner = THIS_MODULE,
		.pm = &gnss_pm_ops,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(gnss_dt_match),
#endif
	},
};

module_platform_driver(gnss_driver);

MODULE_DESCRIPTION("Exynos GNSS interface driver");
MODULE_LICENSE("GPL");
