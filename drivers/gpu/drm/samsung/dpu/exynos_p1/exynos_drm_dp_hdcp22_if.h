/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Samsung EXYNOS SoC DisplayPort HDCP2.2 driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

int dp_hdcp22_irq_handler(struct dp_device *dp);
int dp_hdcp22_authenticate(void);
void dp_hdcp22_notify_state(enum dp_state_for_hdcp22 state);

#define HDCP_SUPPORT
#define HDCP_2_2

extern int hdcp_dplink_authenticate(void); /* hdcp 2.2 */
extern int hdcp_dplink_get_rxstatus(uint8_t *status);
extern int hdcp_dplink_set_paring_available(void);
extern int hdcp_dplink_set_hprime_available(void);
extern int hdcp_dplink_set_rp_ready(void);
extern int hdcp_dplink_set_reauth(void);
extern int hdcp_dplink_set_integrity_fail(void);
extern int hdcp_dplink_cancel_auth(void);
extern void hdcp_dplink_clear_all(void);
extern int hdcp_dplink_auth_check(enum auth_signal);
extern void hdcp_dplink_connect_state(enum dp_state_for_hdcp22 state);
