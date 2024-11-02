// SPDX-License-Identifier: GPL-2.0
/*
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
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/soc/samsung/exynos-soc.h>

#include <asm/cacheflush.h>

#include "gnss_mbox.h"
#include "gnss_prj.h"
#include "gnss_link_device_shmem.h"
#include "pmu-gnss.h"
#include "gnss_utils.h"

#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_MBIM)
#include <linux/of_gpio.h>
#endif

#define BCMD_WAKELOCK_TIMEOUT	(HZ / 10) /* 100 msec */
#define REQ_BCMD_TIMEOUT	(200) /* ms */
#define SW_INIT_TIMEOUT	(1000) /* ms */

#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_MBIM)
static int register_gnss_pwr_interrupt(struct gnss_ctl *gc);
#endif

static void gnss_state_changed(struct gnss_ctl *gc, enum gnss_state state)
{
	struct io_device *iod = gc->iod;
	int old_state = gc->gnss_state;

	if (old_state != state) {
		gc->gnss_state = state;
		gif_info("%s state changed (%s -> %s)\n", gc->name,
			get_gnss_state_str(old_state), get_gnss_state_str(state));
	}

	if (state == STATE_OFFLINE || state == STATE_FAULT)
		wake_up(&iod->wq);
}

static irqreturn_t kepler_sw_init_isr(int irq, void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;

	gif_err_limited("SW_INIT Interrupt occurred!\n");

	if (gc->use_sw_init_intr)
		complete_all(&gc->sw_init_cmpl);

	return IRQ_HANDLED;
}

static irqreturn_t kepler_active_isr(int irq, void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;
	struct io_device *iod = gc->iod;

	gif_err_limited("ACTIVE Interrupt occurred!\n");

	if (!gnssif_wake_lock_active(gc->gc_fault_ws))
		gnssif_wake_lock_timeout(gc->gc_fault_ws, HZ);

	gnss_state_changed(gc, STATE_FAULT);
	wake_up(&iod->wq);

	gc->pmu_ops->clear_int(GNSS_INT_ACTIVE_CLEAR);

	return IRQ_HANDLED;
}

static irqreturn_t kepler_wdt_isr(int irq, void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;
	struct io_device *iod = gc->iod;

	gif_err_limited("WDT Interrupt occurred!\n");

	gif_disable_irq_nosync(&gc->irq_gnss_wdt);

	if (!gnssif_wake_lock_active(gc->gc_fault_ws))
		gnssif_wake_lock_timeout(gc->gc_fault_ws, HZ);

	gnss_state_changed(gc, STATE_FAULT);
	wake_up(&iod->wq);

	gc->pmu_ops->clear_int(GNSS_INT_WDT_RESET_CLEAR);
	return IRQ_HANDLED;
}

static irqreturn_t kepler_irq_bcmd_handler(int irq, void *data)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)data;

	/* Signal kepler_req_bcmd */
	complete_all(&gc->bcmd_cmpl);

	return IRQ_HANDLED;
}

static irqreturn_t gnss_mbox_kepler_simple_lock(int irq, void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;
	struct gnss_mbox *mbx = gc->pdata->mbx;

	gif_err_limited("WAKE interrupt(Mbox15) occurred\n");

	gnss_mbox_set_interrupt(mbx->id, mbx->int_ack_wake_set);

	return IRQ_HANDLED;
}

static irqreturn_t gnss_mbox_kepler_rsp_fault_info(int irq, void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;

	complete_all(&gc->fault_cmpl);

	return IRQ_HANDLED;
}

static DEFINE_MUTEX(reset_lock);

#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_MBIM)
static int check_link_order = 1;
static irqreturn_t gnss_power_handler(int irq, void *data)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)data;
	struct io_device *iod = gc->iod;
	int gnss_pwr = gif_gpio_get_value(gc->m2_gpio_gnss_pwr, true);

	gif_disable_irq_nosync(&gc->m2_irq_gnss_pwr);

	if (gc == NULL) {
		gif_err_limited("gnss_ctl is NOT initialized - IGNORING interrupt\n");
		goto irq_done;
	}

	//if (gc->gnss_state != STATE_ONLINE) {
	//	gif_err_limited("gnss_state is NOT ONLINE - IGNORING interrupt\n");
	//	goto irq_done;
	//}

	gif_info("[GNSS_POWER Handler] state:%s gnss_pwr:%d\n",
			get_gnss_state_str(gc->gnss_state), gnss_pwr);

	if (gnss_pwr == check_link_order) {
		gif_info("skip : gnss_power val is same with before : %d\n", gnss_pwr);
		gc->apwake_irq_chip->irq_set_type(
			irq_get_irq_data(gc->m2_irq_gnss_pwr.num),
			(gnss_pwr == 1 ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH));
		gif_enable_irq(&gc->m2_irq_gnss_pwr);
		return IRQ_HANDLED;
	}
	check_link_order = gnss_pwr;

	if (gnss_pwr == 1) {
		gif_info("gnss power on\n");
		gc->gnss_pwr = POWER_ON;
	} else if (gnss_pwr == 0) {
		gif_info("gnss power off\n");
		gc->gnss_pwr = POWER_OFF;
	}
	gc->is_irq_received = true;
	wake_up(&iod->wq);

	gc->apwake_irq_chip->irq_set_type(
		irq_get_irq_data(gc->m2_irq_gnss_pwr.num),
		(gnss_pwr == 1 ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH));
	gif_enable_irq(&gc->m2_irq_gnss_pwr);

	return IRQ_HANDLED;

irq_done:
	gif_disable_irq_nosync(&gc->m2_irq_gnss_pwr);

	return IRQ_HANDLED;
}

static int register_gnss_pwr_interrupt(struct gnss_ctl *gc)
{
	int ret;

	if (gc == NULL)
		return -EINVAL;

	if (gc->m2_irq_gnss_pwr.registered == true) {
		gif_info("Set IRQF_TRIGGER_LOW to gnss_power gpio\n");
		check_link_order = 1;
		ret = gc->apwake_irq_chip->irq_set_type(
				irq_get_irq_data(gc->m2_irq_gnss_pwr.num),
				IRQF_TRIGGER_LOW);
		return ret;
	}

	gif_info("Register GNSS POWER interrupt.\n");
	gif_init_irq(&gc->m2_irq_gnss_pwr, gc->m2_irq_gnss_pwr.num,
			"gnss_power", IRQF_TRIGGER_LOW);

	ret = gif_request_irq(&gc->m2_irq_gnss_pwr, gnss_power_handler, gc);
	if (ret) {
		gif_err("%s: ERR! request_irq(%s#%d) fail (%d)\n",
			gc->name, gc->m2_irq_gnss_pwr.name,
			gc->m2_irq_gnss_pwr.num, ret);
		gif_err("xxx\n");
		return ret;
	}

	return ret;
}
#endif

static int kepler_hold_reset(struct gnss_ctl *gc)
{
	int ret = 0;

	mutex_lock(&reset_lock);

	gif_info("%s+++\n", __func__);

	if (gc->gnss_state == STATE_OFFLINE) {
		gif_err("current kerpler status is offline, so it will be ignored\n");
		mutex_unlock(&reset_lock);
		return -EPERM;
	} else if (gc->gnss_state == STATE_HOLD_RESET) {
		gif_err("current kerpler status is offline, so it will be ignored\n");
		mutex_unlock(&reset_lock);
		return -EPERM;
	}

	if (gc->ccore_qch_lh_gnss) {
		clk_disable_unprepare(gc->ccore_qch_lh_gnss);
		gif_err("Disabled GNSS Qch\n");
	}

	ret = gc->pmu_ops->hold_reset();
	if (ret) {
		gif_err("hold reset fails: apm pending\n");
		mutex_unlock(&reset_lock);
		return -EIO;
	}
	gnss_mbox_sw_reset(gc->pdata->mbx->id);

	mutex_unlock(&reset_lock);

	gnss_state_changed(gc, STATE_HOLD_RESET);

	gif_info("%s---\n", __func__);

	return ret;
}

static int kepler_release_reset(struct gnss_ctl *gc)
{
	int ret;
	unsigned long timeout = msecs_to_jiffies(SW_INIT_TIMEOUT);

	mutex_lock(&reset_lock);

	gif_info("%s+++\n", __func__);

	if (gc->gnss_state != STATE_HOLD_RESET) {
		gif_err("current kerpler status is not reset, so it will be ignored\n");
		mutex_unlock(&reset_lock);
		return -EPERM;
	}

	gnss_mbox_clear_all_interrupt(gc->pdata->mbx->id);

	if (gc->use_sw_init_intr)
		reinit_completion(&gc->sw_init_cmpl);

	ret = gc->pmu_ops->release_reset();
	if (ret) {
		gif_err("failure due to apm pending\n");
		mutex_unlock(&reset_lock);
		return -EIO;
	}

	if (gc->ccore_qch_lh_gnss) {
		ret = clk_prepare_enable(gc->ccore_qch_lh_gnss);
		if (!ret)
			gif_info("GNSS Qch enabled\n");
		else
			gif_err("Could not enable Qch (%d)\n", ret);
	}

	if (gc->use_sw_init_intr) {
		gif_info("waiting sw_init_cmpl\n");
		ret = wait_for_completion_timeout(&gc->sw_init_cmpl, timeout);
		if (ret == 0) {
			gif_err("%s: sw_init_cmpl TIMEOUT!\n", gc->name);
			mutex_unlock(&reset_lock);
			return -EIO;
		}
	}
	msleep(100);
	ret = gc->pmu_ops->req_security();
	if (ret != 0) {
		gif_err("req_security error! %d\n", ret);
		mutex_unlock(&reset_lock);
		return ret;
	}
	gc->pmu_ops->req_baaw();

	gif_enable_irq(&gc->irq_gnss_wdt);

	gc->reset_count++;

	gnss_state_changed(gc, STATE_ONLINE);

	gif_info("RESET COUNT: %d\n", gc->reset_count);
	gif_info("%s---\n", __func__);

	mutex_unlock(&reset_lock);

	return 0;
}

static int kepler_power_on(struct gnss_ctl *gc)
{
	int ret;
	unsigned long timeout = msecs_to_jiffies(SW_INIT_TIMEOUT);

	mutex_lock(&reset_lock);

	gif_info("%s+++\n", __func__);

	if (gc->gnss_state != STATE_OFFLINE) {
		mutex_unlock(&reset_lock);
		gif_err("current kerpler status is not offline, so it will be ignored\n");
		return -EPERM;
	}

	gnss_mbox_clear_all_interrupt(gc->pdata->mbx->id);

	if (gc->use_sw_init_intr)
		reinit_completion(&gc->sw_init_cmpl);

	ret = gc->pmu_ops->power_on(GNSS_POWER_ON);
	if (ret) {
		mutex_unlock(&reset_lock);
		gif_err("GNSS power on fails due to apm pending\n");
		return -EIO;
	}

	if (gc->ccore_qch_lh_gnss) {
		ret = clk_prepare_enable(gc->ccore_qch_lh_gnss);
		if (!ret)
			gif_info("GNSS Qch enabled\n");
		else
			gif_err("Could not enable Qch (%d)\n", ret);
	}

	if (gc->use_sw_init_intr) {
		gif_info("waiting sw_init_cmpl\n");
		ret = wait_for_completion_timeout(&gc->sw_init_cmpl, timeout);
		if (ret == 0) {
			mutex_unlock(&reset_lock);
			gif_err("%s: sw_init_cmpl TIMEOUT!\n", gc->name);
			return -EIO;
		}
	}

	gif_info("enable GNSS active irq\n");
	gif_enable_irq(&gc->irq_gnss_active);

	msleep(100);
	ret = gc->pmu_ops->req_security();
	if (ret != 0) {
		mutex_unlock(&reset_lock);
		gif_err("req_security error! %d\n", ret);
		return ret;
	}
	gc->pmu_ops->req_baaw();

#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_MBIM)
	ret = register_gnss_pwr_interrupt(gc);
	if (ret) {
		gif_err("Err: register_gnss_pwr_interrupt:%d\n", ret);
		return ret;
	}
	gif_enable_irq(&gc->m2_irq_gnss_pwr);
#endif

	gnss_state_changed(gc, STATE_ONLINE);

	gif_info("%s---\n", __func__);

	mutex_unlock(&reset_lock);

	return 0;
}

static int kepler_req_fault_info(struct gnss_ctl *gc)
{
	int ret;
	struct gnss_pdata *pdata;
	struct gnss_mbox *mbx;
	unsigned long timeout = msecs_to_jiffies(1000);
	u32 size = 0;

	mutex_lock(&reset_lock);

	if (!gc) {
		gif_err("No gnss_ctl info!\n");
		ret = -ENODEV;
		goto req_fault_exit;
	}

	pdata = gc->pdata;
	mbx = pdata->mbx;

	reinit_completion(&gc->fault_cmpl);

	gnss_mbox_set_interrupt(mbx->id, mbx->int_req_fault_info);

	ret = wait_for_completion_timeout(&gc->fault_cmpl, timeout);
	if (ret == 0) {
		gif_err("Req Fault Info TIMEOUT!\n");
		ret = -EIO;
		goto req_fault_exit;
	}

	switch (pdata->fault_info.device) {
	case GNSS_IPC_MBOX:
		size = pdata->fault_info.size * sizeof(u32);
		ret = size;
		break;
	case GNSS_IPC_SHMEM:
		size = gnss_mbox_get_sr(mbx->id, mbx->reg_bcmd_ctrl[CTRL3]);
		ret = size;
		break;
	default:
		gif_err("No device for fault dump!\n");
		ret = -ENODEV;
	}

req_fault_exit:
	if (gc && gc->gc_fault_ws)
		gnssif_wake_unlock(gc->gc_fault_ws);

	mutex_unlock(&reset_lock);

	return ret;
}


static int kepler_suspend(struct gnss_ctl *gc)
{
	return 0;
}

static int kepler_resume(struct gnss_ctl *gc)
{
	return 0;
}

static int kepler_req_bcmd(struct gnss_ctl *gc, u16 cmd_id, u16 flags,
		u32 param1, u32 param2)
{
	u32 ctrl[BCMD_CTRL_COUNT], ret_val;
	unsigned long timeout = msecs_to_jiffies(REQ_BCMD_TIMEOUT);
	int ret = 0;
	struct gnss_mbox *mbx = gc->pdata->mbx;

	mutex_lock(&reset_lock);

	if (gc->gnss_state != STATE_ONLINE) {
		gif_err("GNSS is not online, error return\n");
		mutex_unlock(&reset_lock);
		return -EPERM;
	}

	/* Parse arguments */
	/* Flags: Command flags */
	/* Param1/2 : Paramter 1/2 */

	ctrl[CTRL0] = (flags << 16) + cmd_id;
	ctrl[CTRL1] = param1;
	ctrl[CTRL2] = param2;
	gif_info("%s : set param  0 : 0x%x, 1 : 0x%x, 2 : 0x%x\n",
			__func__, ctrl[CTRL0], ctrl[CTRL1], ctrl[CTRL2]);
	gnss_mbox_set_sr(mbx->id, mbx->reg_bcmd_ctrl[CTRL0], ctrl[CTRL0]);
	gnss_mbox_set_sr(mbx->id, mbx->reg_bcmd_ctrl[CTRL1], ctrl[CTRL1]);
	gnss_mbox_set_sr(mbx->id, mbx->reg_bcmd_ctrl[CTRL2], ctrl[CTRL2]);
	/*
	 * 0xff is MAGIC number to avoid confuging that
	 * register is set from Kepler.
	 */
	gnss_mbox_set_sr(mbx->id, mbx->reg_bcmd_ctrl[CTRL3], 0xff);

	reinit_completion(&gc->bcmd_cmpl);

	gnss_mbox_set_interrupt(mbx->id, mbx->int_bcmd);

	if (cmd_id == 0x4) { /* BLC_Branch does not have return value */
		gif_info("cmd_id 0x%x\n", cmd_id);

		mutex_unlock(&reset_lock);
		return 0;
	}

	ret = wait_for_completion_interruptible_timeout(&gc->bcmd_cmpl,
						timeout);
	if (ret == 0) {
		gif_err("%s: bcmd TIMEOUT!\n", gc->name);
		mutex_unlock(&reset_lock);
		return -EIO;
	}

	ret_val = gnss_mbox_get_sr(mbx->id, mbx->reg_bcmd_ctrl[CTRL3]);

	gif_info("BCMD cmd_id 0x%x returned 0x%x\n", cmd_id, ret_val);

	mutex_unlock(&reset_lock);

	return ret_val;
}

static ssize_t mbox_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gnss_pdata *pdata = (struct gnss_pdata *)dev->platform_data;
	struct gnss_mbox *mbx = pdata->mbx;

	return sprintf(buf, "CTRL0: 0x%08X\nCTRL1: 0x%08X\n"
			"CTRL2: 0x%08X\nCTRL3: 0x%08X\n",
			gnss_mbox_get_sr(mbx->id, mbx->reg_bcmd_ctrl[CTRL0]),
			gnss_mbox_get_sr(mbx->id, mbx->reg_bcmd_ctrl[CTRL1]),
			gnss_mbox_get_sr(mbx->id, mbx->reg_bcmd_ctrl[CTRL2]),
			gnss_mbox_get_sr(mbx->id, mbx->reg_bcmd_ctrl[CTRL3]));
}

static DEVICE_ATTR_RO(mbox_status);

static struct attribute *mbox_attrs[] = {
	&dev_attr_mbox_status.attr,
	NULL,
};

static const struct attribute_group mbox_group = {
	.attrs = mbox_attrs,
	.name = "mbox",
};

static void gnss_get_ops(struct gnss_ctl *gc)
{
	gc->ops.gnss_hold_reset = kepler_hold_reset;
	gc->ops.gnss_release_reset = kepler_release_reset;
	gc->ops.gnss_power_on = kepler_power_on;
	gc->ops.gnss_req_fault_info = kepler_req_fault_info;
	gc->ops.suspend = kepler_suspend;
	gc->ops.resume = kepler_resume;
	gc->ops.req_bcmd = kepler_req_bcmd;
}

#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_MBIM)
static void gnss_get_pdata(struct gnss_ctl *gc, struct gnss_pdata *pdata)
{
	struct platform_device *pdev = to_platform_device(gc->dev);
	struct device_node *np = pdev->dev.of_node;

	/* GNSS Power */
	gc->m2_gpio_gnss_pwr = of_get_named_gpio(np, "gpio_gnss_pwr_on_off", 0);
	if (gc->m2_gpio_gnss_pwr < 0) {
		gif_err("Can't Get m2_gpio_gnss_pwr!\n");
		return;
	}
	gpio_request(gc->m2_gpio_gnss_pwr, "M2_W_DISABLE2_N");
	gc->m2_irq_gnss_pwr.num = gpio_to_irq(gc->m2_gpio_gnss_pwr);
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
static int gnss_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct gnss_ctl *gc;
	struct itmon_notifier *itmon_data = nb_data;

	gc = container_of(nb, struct gnss_ctl, itmon_nb);

	if (IS_ERR_OR_NULL(itmon_data))
		return NOTIFY_DONE;

	if (itmon_data->port &&
		(strncmp("GNSS", itmon_data->port, sizeof("GNSS") - 1) == 0)) {
		gif_info("detect gnss itmon and enter scandump mode\n");
	} else if (itmon_data->master &&
		(strncmp("GNSS", itmon_data->master, sizeof("GNSS") - 1) == 0)) {
		gif_info("detect gnss itmon and enter scandump mode\n");
	}

	return NOTIFY_DONE;
}
#endif

int init_gnssctl_device(struct gnss_ctl *gc, struct gnss_pdata *pdata)
{
	int ret = 0, irq = 0;
	struct platform_device *pdev = NULL;
	struct gnss_mbox *mbox = gc->pdata->mbx;

	gif_info("Initializing GNSS Control\n");

	gnss_get_ops(gc);
	gnss_get_pmu_ops(gc);
#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_MBIM)
	gnss_get_pdata(gc, pdata);
#endif

	gc->use_sw_init_intr = true;

#if defined(S5E9925_SOC_ID)
	if (exynos_soc_info.product_id == S5E9925_SOC_ID) {
		if (exynos_soc_info.revision == 0)
			gc->use_sw_init_intr = false;
	}
#endif
	gif_info("use_sw_init_intr: %d\n", gc->use_sw_init_intr);

	dev_set_drvdata(gc->dev, gc);

	init_completion(&gc->fault_cmpl);
	init_completion(&gc->bcmd_cmpl);
	if (gc->use_sw_init_intr)
		init_completion(&gc->sw_init_cmpl);

	pdev = to_platform_device(gc->dev);

	gc->gc_fault_ws = gnssif_wake_lock_register(&pdev->dev, "gnss_fault_wake_lock");
	if (gc->gc_fault_ws == NULL) {
		gif_err("gnss_fault_wake_lock: wakeup_source_register fail\n");
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_MBIM)
	gif_err("Register GPIO interrupts\n");
	gc->apwake_irq_chip = irq_get_chip(gc->m2_irq_gnss_pwr.num);
	if (gc->apwake_irq_chip == NULL) {
		gif_err("Can't get irq_chip structure!!!!\n");
		return -EINVAL;
	}
#endif

	/* GNSS_ACTIVE */
	irq = platform_get_irq_byname(pdev, "ACTIVE");
	if (irq < 0) {
		gif_err("GNSS ACTIVE IRQ not found\n");
		ret = -ENODEV;
		goto error;
	}

	gif_init_irq(&gc->irq_gnss_active, irq, "kepler_active_handler", IRQF_ONESHOT);
	ret = gif_request_irq(&gc->irq_gnss_active, kepler_active_isr, gc);
	if (ret) {
		gif_err("Request irq fail - kepler_active_isr(%d)\n", ret);
		goto error;
	}
	gif_info("disable GNSS active wakeup\n");
	gif_disable_irq_sync(&gc->irq_gnss_active);

	/* GNSS_WATCHDOG */
	irq = platform_get_irq_byname(pdev, "WATCHDOG");
	if (irq < 0) {
		gif_err("GNSS WATCHDOG IRQ not found\n");
		ret = -ENODEV;
		goto error;
	}

	gif_init_irq(&gc->irq_gnss_wdt, irq, "kepler_wdt_handler", IRQF_ONESHOT);
	ret = gif_request_irq(&gc->irq_gnss_wdt, kepler_wdt_isr, gc);
	if (ret) {
		gif_err("Request irq fail - kepler_wdt_isr(%d)\n", ret);
		goto error;
	}

	gif_info("Using simple lock sequence!!!\n");

	ret = gnss_mbox_register_irq(mbox->id, mbox->irq_simple_lock, gnss_mbox_kepler_simple_lock, (void *)gc);
	if (ret < 0) {
		gif_err("simple_lock register mailbox fail\n");
		goto error;
	}

	/* GNSS2AP */
	if (gc->use_sw_init_intr) {
		gif_info("use_sw_init_intr is set\n");

		irq = platform_get_irq_byname(pdev, "SW_INIT");
		if (irq < 0) {
			gif_err("GNSS SW INIT IRQ not found\n");
			ret = -ENODEV;
			goto error;
		}

		gif_init_irq(&gc->irq_gnss_sw_init, irq, "kepler_sw_init_handler", IRQF_ONESHOT);
		ret = gif_request_irq(&gc->irq_gnss_sw_init, kepler_sw_init_isr, gc);
		if (ret) {
			gif_err("Request irq fail - kepler_sw_init_isr(%d)\n", ret);
			goto error;
		}
	}

	/* Initializing Shared Memory for GNSS */
	gif_info("Initializing shared memory for GNSS.\n");

	gc->pmu_ops->init_conf(gc);
	gc->gnss_state = STATE_OFFLINE;

	gif_info("Register mailbox for GNSS2AP fault handling\n");

	ret = gnss_mbox_register_irq(mbox->id, mbox->irq_rsp_fault_info,
			 gnss_mbox_kepler_rsp_fault_info, (void *)gc);
	if (ret < 0) {
		gif_err("rsp_fault_info register mailbox fail\n");
		goto error;
	}

	ret = gnss_mbox_register_irq(mbox->id, mbox->irq_bcmd,
			kepler_irq_bcmd_handler, (void *)gc);
	if (ret < 0) {
		gif_err("bcmd register mailbox fail\n");
		goto error;
	}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	gc->itmon_nb.notifier_call = gnss_itmon_notifier;
	itmon_notifier_chain_register(&gc->itmon_nb);
#endif

	if (sysfs_create_group(&pdev->dev.kobj, &mbox_group))
		gif_err("failed to create mbox sysfs node\n");

	gif_info("---\n");

	return ret;

error:
	gnss_mbox_unregister_irq(mbox->id, mbox->irq_bcmd, kepler_irq_bcmd_handler);
	gnss_mbox_unregister_irq(mbox->id, mbox->irq_rsp_fault_info, gnss_mbox_kepler_rsp_fault_info);
	gnss_mbox_unregister_irq(mbox->id, mbox->irq_simple_lock, gnss_mbox_kepler_simple_lock);

	return ret;
}
