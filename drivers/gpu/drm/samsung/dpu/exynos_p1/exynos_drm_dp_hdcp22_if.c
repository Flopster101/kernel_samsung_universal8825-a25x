/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS SoC DisplayPort HDCP2.2 driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/module.h>
#include <video/mipi_display.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-dv-timings.h>
#if defined(CONFIG_CPU_IDLE)
#include <soc/samsung/exynos-cpupm.h>
#endif
#if defined(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
#include <sound/samsung/dp_ado.h>
#endif
#include <linux/smc.h>
#include <linux/iommu.h>

#if defined(CONFIG_PHY_EXYNOS_USBDRD)
#include "../../../drivers/phy/samsung/phy-exynos-usbdrd.h"
#endif
#include "exynos_drm_dp.h"
#include "exynos_drm_dp_hdcp22_if.h"
#include "exynos_drm_decon.h"

static int reauth_trigger;

int dp_hdcp22_irq_handler(struct dp_device *dp)
{
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
	uint8_t rxstatus = 0;
	int ret = 0;
	int active;

	active = pm_runtime_active(dp->dev);
	if (!active) {
		dp_info(dp, "dp power(%d), state(%d)\n", active, dp->state);
		spin_unlock(&dp->slock);
		return IRQ_HANDLED;
	}

	hdcp_dplink_get_rxstatus(&rxstatus);

	if (rxstatus & DPCD_HDCP22_RXSTATUS_LINK_INTEGRITY_FAIL) {
		/* hdcp22 disable while re-authentication */
		ret = hdcp_dplink_set_integrity_fail();
		reauth_trigger = 1;
		queue_delayed_work(dp->hdcp2_wq,
				&dp->hdcp22_work, msecs_to_jiffies(100));
		dp_info(dp, "LINK_INTEGRITY_FAIL HDCP2 enc off\n");
	} else if (rxstatus & DPCD_HDCP22_RXSTATUS_REAUTH_REQ) {
		/* hdcp22 disable while re-authentication */
		ret = hdcp_dplink_set_reauth();
		reauth_trigger = 1;
		queue_delayed_work(dp->hdcp2_wq,
			&dp->hdcp22_work, msecs_to_jiffies(100));
		dp_info(dp, "REAUTH_REQ HDCP2 enc off\n");
	} else if (rxstatus & DPCD_HDCP22_RXSTATUS_PAIRING_AVAILABLE) {
		/* set pairing avaible flag */
		ret = hdcp_dplink_set_paring_available();
	} else if (rxstatus & DPCD_HDCP22_RXSTATUS_HPRIME_AVAILABLE) {
		/* set hprime avaible flag */
		ret = hdcp_dplink_set_hprime_available();
	} else if (rxstatus & DPCD_HDCP22_RXSTATUS_READY) {
		/* set ready avaible flag */
		/* todo update stream Management */
		ret = hdcp_dplink_set_rp_ready();
		return hdcp_dplink_auth_check(HDCP_RP_READY);
	} else {
		dp_info(dp, "undefined RxStatus(0x%x). ignore\n", rxstatus);
		ret = -EINVAL;
	}

	return ret;
#else
	dp_info(dp, "Not compiled EXYNOS_HDCP2 driver\n");
	return 0;
#endif
}

void reset_dp_hdcp_module(void)
{
	struct dp_device *dp = get_dp_drvdata();

	dp_info(dp, "DP Reset to rest HDCP module\n");
	queue_delayed_work(dp->dp_wq,
			&dp->hpd_unplug_work, 0);
}

int dp_hdcp22_authenticate()
{
	struct dp_device *dp = get_dp_drvdata();
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
#ifdef CONFIG_HDCP2_FUNC_TEST_MODE
	dp_info(dp, "HDCP22 DRM Always On.\n");
	return hdcp_dplink_auth_check(HDCP_DRM_ON);
#else
	dp_reg_set_hdcp22_system_enable(0);
	dp_reg_set_hdcp22_mode(0);
	dp_reg_set_hdcp22_encryption_enable(0);
	if (reauth_trigger)
		hdcp_dplink_auth_check(HDCP_DRM_ON);

	reauth_trigger = 0;
	return 0;
#endif
#endif
	dp_info(dp, "Not compiled EXYNOS_HDCP2 driver\n");
	return 0;
}

void dp_hdcp22_notify_state(enum dp_state_for_hdcp22 state)
{
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
	hdcp_dplink_connect_state(state);
#endif
}
