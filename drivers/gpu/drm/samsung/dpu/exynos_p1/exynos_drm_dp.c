/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS SoC DisplayPort driver.
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
#include <linux/console.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/module.h>
#include <video/mipi_display.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-dv-timings.h>
#include <linux/debugfs.h>
#include <soc/samsung/exynos-devfreq.h>
#include <dt-bindings/soc/samsung/s5e9925-devfreq.h>
#if defined(CONFIG_CPU_IDLE)
#include <soc/samsung/exynos-cpupm.h>
#endif
#include <linux/smc.h>
#include <linux/iommu.h>
#include <linux/switch.h>

#include <drm/drm_vblank.h>
#ifdef CONFIG_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
#include <linux/ktime.h>
#endif

#if IS_ENABLED(CONFIG_PHY_EXYNOS_USBDRD)
#include "../../../../../../drivers/phy/samsung/phy-exynos-usbdrd.h"
#endif

#ifdef CONFIG_SEC_DISPLAYPORT_SELFTEST
#include "../dp_ext_func/dp_self_test.h"
#endif

#include "exynos_drm_crtc.h"
#include "exynos_drm_dp.h"
#include "exynos_drm_dp_hdcp22_if.h"
#include "exynos_drm_decon.h"

#define PIXELCLK_2160P30HZ 297000000 /* UHD 30hz */
#define PIXELCLK_1080P60HZ 148500000 /* FHD 60Hz */
#define PIXELCLK_1080P30HZ 74250000 /* FHD 30Hz */

/* DP debug module sysfs */
static int dp_underrun_dump = 0;
module_param(dp_underrun_dump, int, 0644);
int dp_log_level = 6;
module_param(dp_log_level, int, 0644);
static int dp_sst1_bist_test = 0;
module_param(dp_sst1_bist_test, int, 0644);
static int dp_sst2_bist_test = 0;
module_param(dp_sst2_bist_test, int, 0644);

static int dp_hdp_link_tr_fail = 0;
module_param(dp_hdp_link_tr_fail, int, 0644);
static int dp_hdp_read_fail = 0;
module_param(dp_hdp_read_fail, int, 0644);


/* force link rate for test
 * LINK_RATE_1_62Gbps 0x06
 * LINK_RATE_2_7Gbps 0x0A
 * LINK_RATE_5_4Gbps 0x14
 * LINK_RATE_8_1Gbps 0x1E
 */
static int forced_linkrate = 0x0;
module_param(forced_linkrate, int, 0644);

/* force hdcp version for test
 * HDCP_VERSION_1_3 13
 * HDCP_VERSION_2_2 22
 */
static int forced_hdcp = 0;
module_param(forced_hdcp, int, 0644);

static int forced_bist_en = 0;
module_param(forced_bist_en, int, 0644);

//static u64 reduced_resolution;
static struct dp_debug_param g_dp_debug_param;

extern enum hdcp22_auth_def hdcp22_auth_state;
struct dp_device *dp_drvdata;
EXPORT_SYMBOL(dp_drvdata);

extern u32 phy_eq_hbr0_1[phy_table][4][4][5];
extern u32 phy_eq_hbr2_3[phy_table][4][4][5];

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
struct blocking_notifier_head dp_ado_notifier_head =
		BLOCKING_NOTIFIER_INIT(dp_ado_notifier_head);
EXPORT_SYMBOL(dp_ado_notifier_head);
#endif

void dp_hdcp22_enable(u32 en);
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
extern void dp_register_func_for_hdcp22(void (*func0)(u32 en),
		int (*func1)(u32 address, u32 length, u8 *data),
		int (*func2)(u32 address, u32 length, u8 *data));
#endif

static inline struct dp_device *encoder_to_dp(struct drm_encoder *e)
{
	return container_of(e, struct dp_device, encoder);
}

static inline struct dp_device *connector_to_dp(struct drm_connector *c)
{
	return container_of(c, struct dp_device, connector);
}

#define PREFIX_LEN      40
#define ROW_LEN         32
void dp_print_hex_dump(void __iomem *regs, const void *buf, size_t len)
{
	char prefix_buf[PREFIX_LEN];
	unsigned long p;
	size_t i, row;

	for (i = 0; i < len; i += ROW_LEN) {
		p = buf - regs + i;

		if (len - i < ROW_LEN)
			row = len - i;
		else
			row = ROW_LEN;

		snprintf(prefix_buf, sizeof(prefix_buf), "[%08lX] ", p);
		print_hex_dump(KERN_NOTICE, prefix_buf, DUMP_PREFIX_NONE,
				32, 4, buf + i, row, false);
	}
}

#define CR_BUF_LEN	256
#define MAX_ERR_CNT	10
void dp_dump_cr(struct dp_device *dp)
{
	int i, j;
	int height, num_word;
	int size = 0x4354;
	char buf[CR_BUF_LEN];
	int idx;
	int err, err_cnt = 0;
	u16 val;

	dp_info(dp, "=== DP CR DUMP ===\n");
	height = (size / 32) + 1;
	num_word = size;
	for (i = 0; i < height; i++) {
		idx = snprintf(buf, sizeof(buf), "[0x%08x] ", i * 32);
		for (j = 0; j < 32; j++) {
			if (num_word <= 0)
				break;
			if (err_cnt >= MAX_ERR_CNT)
				break;

			val = dp_reg_cr_read(i * 32 + j, &err);
			if (err) {
				err_cnt++;
				idx += snprintf(buf + idx, sizeof(buf) - idx, " ERR ");
			} else {
				idx += snprintf(buf + idx, sizeof(buf) - idx, "%04x ", val);
			}

			num_word--;
		}
		dp_info(dp, "%s\n", buf);
		if (num_word <= 0)
			break;
		if (err_cnt >= MAX_ERR_CNT)
			break;
	}

	if (err_cnt)
		dp_info(dp, "=== CR read error: %d\n", err_cnt);
}

void dp_dump_registers(struct dp_device *dp)
{
	dp_info(dp, "=== DP SFR DUMP ===\n");
	dp_print_hex_dump(dp->res.link_regs,
			dp->res.link_regs, 0xC0);
	dp_print_hex_dump(dp->res.link_regs,
			dp->res.link_regs + 0x100, 0x0C);
	dp_print_hex_dump(dp->res.link_regs,
			dp->res.link_regs + 0x200, 0x08);
	dp_print_hex_dump(dp->res.link_regs,
			dp->res.link_regs + 0x2000, 0x64);
	dp_print_hex_dump(dp->res.link_regs,
			dp->res.link_regs + 0x3000, 0x130);
	dp_print_hex_dump(dp->res.link_regs,
			dp->res.link_regs + 0x5000, 0x104);
	dp_print_hex_dump(dp->res.link_regs,
			dp->res.link_regs + 0x5400, 0x9AC);
	dp_print_hex_dump(dp->res.link_regs,
			dp->res.link_regs + 0x6000, 0x104);
	dp_print_hex_dump(dp->res.link_regs,
			dp->res.link_regs + 0x6400, 0x46C);
}


void dp_phy_dump_registers(struct dp_device *dp)
{
	/* DP PHY range of offset is 0x0 ~ 0x250
	 * But several SFR's can not be read.
	 */
	dp_info(dp, "=== DP PHY SFR DUMP ===\n");

	dp_print_hex_dump(dp->res.phy_regs,
			dp->res.phy_regs, 0x10);
	dp_print_hex_dump(dp->res.phy_regs,
			dp->res.phy_regs + 0x100, 0x7C);
	dp_print_hex_dump(dp->res.phy_regs,
			dp->res.phy_regs + 0x200, 0x54);
}

void dp_phy_tca_dump_registers(struct dp_device *dp)
{
	dp_info(dp, "=== DP PHY TCA SFR DUMP ===\n");
	dp_print_hex_dump(dp->res.phy_tca_regs,
			dp->res.phy_tca_regs, 0xFC);
}


#if IS_ENABLED(CONFIG_ANDROID_SWITCH)

static struct switch_dev switch_secdp_hpd = {
	.name = "hdmi",
};
static struct switch_dev switch_secdp_msg = {
	.name = "secdp_msg",
};

static void dp_set_switch_poor_connect(void)
{
	struct dp_device *dp = get_dp_drvdata();

	if (++dp->poor_connect_count > MAX_POOR_CONNECT_EVENT)
		return;

	dp_err(dp, "set poor connect switch event\n");
	switch_set_state(&switch_secdp_msg, 1);
	switch_set_state(&switch_secdp_msg, 0);
}

/*for HDMI_PLUGGED intent*/
static void dp_set_switch_hpd_state(int state)
{
	switch_set_state(&switch_secdp_hpd, state);
}

#else
static void dp_set_switch_poor_connect(void)
{
	struct dp_device *dp = get_dp_drvdata();

	dp_err(dp, "ERROR: Need functions for uevents\n");
}
static void dp_set_switch_hpd_state(int state)
{
	struct dp_device *dp = get_dp_drvdata();

	dp_err(dp, "ERROR: Need functions for uevents\n");
}
#endif


static int dp_remove(struct platform_device *pdev)
{
	struct dp_device *dp = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
	switch_dev_unregister(&switch_secdp_msg);
	switch_dev_unregister(&switch_secdp_hpd);
#endif
	mutex_destroy(&dp->cmd_lock);
	mutex_destroy(&dp->hpd_lock);
	mutex_destroy(&dp->cal_res.aux_lock);
	mutex_destroy(&dp->training_lock);
	mutex_destroy(&dp->hdcp2_lock);
	destroy_workqueue(dp->dp_wq);
	destroy_workqueue(dp->hdcp2_wq);
	dp_info(dp, "dp driver removed\n");

	return 0;
}

static void dp_init_info(struct dp_device *dp)
{
	dp->hpd_current_state = EXYNOS_HPD_UNPLUG;
	dp->state = DP_STATE_INIT;
	dp->cal_res.bpc = BPC_8;
	dp->dyn_range = VESA_RANGE;
	dp->bist_used = 0;
	dp->bist_type = COLOR_BAR;
	dp->audio_state = AUDIO_DISABLE;
	dp->cal_res.audio_buf_empty_check = 0;
	dp->decon_run = 0;
	dp->connector.status = connector_status_disconnected;
}

static u64 dp_find_edid_max_pixelclock(struct dp_device *dp)
{
#ifdef FEATURE_USE_DRM_EDID_PARSER
	return dp->best_mode.clock * 1000;
#else
	int i;
	u64 max_pclk = 0;

	for (i = supported_videos_pre_cnt - 1; i > 0; i--) {
		if (supported_videos[i].edid_support_match &&
				supported_videos[i].dv_timings.bt.pixelclock > max_pclk)
			max_pclk = supported_videos[i].dv_timings.bt.pixelclock;
	}
	dp_info(dp, "find max pclk : %llu\n", max_pclk);
	return max_pclk;
#endif
}

#ifndef FEATURE_USE_DRM_EDID_PARSER
static int dp_check_edid_max_clock(struct dp_device *dp,
		videoformat video_format, enum bit_depth bpc)
{
	int ret_val = true;
	u64 calc_pixel_clock = 0;

	switch (bpc) {
	case BPC_8:
		calc_pixel_clock = supported_videos[video_format].dv_timings.bt.pixelclock;
		break;
	case BPC_10:
		calc_pixel_clock = supported_videos[video_format].dv_timings.bt.pixelclock * 125 / 100;
		break;
	default:
		calc_pixel_clock = supported_videos[video_format].dv_timings.bt.pixelclock;
		break;
	}

	if (dp->rx_edid_data.max_support_clk != 0) {
		if (calc_pixel_clock > (u64)dp->rx_edid_data.max_support_clk * (u64)MHZ) {
			dp_info(dp, "RX support Max TMDS Clock = %d, but pixel clock = %llu\n",
					dp->rx_edid_data.max_support_clk * MHZ, calc_pixel_clock);
			ret_val = false;
		}
	} else
		dp_info(dp, "Can't check RX support Max TMDS Clock\n");

	return ret_val;
}
#endif

u64 dp_get_cur_bandwidth(u8 link_rate, u8 lane_cnt, enum bit_depth bpc)
{
	u64 pixel_clock;

	switch (link_rate) {
	case LINK_RATE_1_62Gbps:
		pixel_clock = RBR_PIXEL_CLOCK_PER_LANE * (u64)lane_cnt;
		break;
	case LINK_RATE_2_7Gbps:
		pixel_clock = HBR_PIXEL_CLOCK_PER_LANE * (u64)lane_cnt;
		break;
	case LINK_RATE_5_4Gbps:
		pixel_clock = HBR2_PIXEL_CLOCK_PER_LANE * (u64)lane_cnt;
		break;
	case LINK_RATE_8_1Gbps:
		pixel_clock = HBR3_PIXEL_CLOCK_PER_LANE * (u64)lane_cnt;
		break;
	default:
		pixel_clock = HBR2_PIXEL_CLOCK_PER_LANE * (u64)lane_cnt;
		break;
	}

	if (bpc == BPC_10)
		pixel_clock = pixel_clock * 100 / 125;

	return pixel_clock;
}

static int dp_check_link_rate_pixel_clock(struct dp_device *dp,
		u8 link_rate, u8 lane_cnt, u64 pixel_clock, enum bit_depth bpc)
{
	u64 calc_pixel_clock = 0;
	int ret_val = false;

	calc_pixel_clock = dp_get_cur_bandwidth(link_rate, lane_cnt, bpc);

	if (calc_pixel_clock >= pixel_clock)
		ret_val = true;

	if (ret_val == false)
		dp_info(dp, "link rate: 0x%x, lane cnt: %d, pclk = %llu, calc_pclk = %llu\n",
				link_rate, lane_cnt, pixel_clock, calc_pixel_clock);

	return ret_val;
}

#ifndef FEATURE_USE_DRM_EDID_PARSER
static int dp_check_pixel_clock_for_hdr(struct dp_device *dp,
		videoformat video_format)
{
	int ret_val = false;

	if (dp->rx_edid_data.hdr_support) {
		ret_val = dp_check_edid_max_clock(dp, video_format, BPC_10);

		if (ret_val == true)
			ret_val = dp_check_link_rate_pixel_clock(dp,
					dp_reg_phy_get_link_bw(&dp->cal_res),
					dp_reg_get_lane_count(),
					supported_videos[video_format].dv_timings.bt.pixelclock,
					BPC_10);
	}

	return ret_val;
}
#endif

static int dp_get_min_link_rate(struct dp_device *dp,
		u8 rx_link_rate, u8 lane_cnt, enum bit_depth bpc)
{
	int i = 0;
	int link_rate[MAX_LINK_RATE_NUM] = {LINK_RATE_1_62Gbps,
			LINK_RATE_2_7Gbps, LINK_RATE_5_4Gbps, LINK_RATE_8_1Gbps};
	u64 max_pclk = 0;
	u8 min_link_rate = 0;

	if (rx_link_rate == LINK_RATE_1_62Gbps)
		return rx_link_rate;

	if (lane_cnt > 4)
		return LINK_RATE_5_4Gbps;

	max_pclk = dp_find_edid_max_pixelclock(dp);
	dp_info(dp, "max_pixel clock: %d\n", max_pclk);
	for (i = 0; i < MAX_LINK_RATE_NUM; i++) {
		if (dp_check_link_rate_pixel_clock(dp, link_rate[i],
				lane_cnt, max_pclk, bpc) == true)
			break;
	}

	if (i >= MAX_LINK_RATE_NUM)
		min_link_rate = LINK_RATE_5_4Gbps;
	else
		min_link_rate = link_rate[i] > rx_link_rate ? rx_link_rate : link_rate[i];

	dp_info(dp, "set link late: 0x%x, lane cnt:%d\n", min_link_rate, lane_cnt);

	return min_link_rate;
}

void dp_get_voltage_and_pre_emphasis_max_reach(u8 *drive_current, u8 *pre_emphasis, u8 *max_reach_value)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (drive_current[i] >= MAX_REACHED_CNT) {
			max_reach_value[i] &= ~(1 << MAX_SWING_REACHED_BIT_POS);
			max_reach_value[i] |= (1 << MAX_SWING_REACHED_BIT_POS);
		} else
			max_reach_value[i] &= ~(1 << MAX_SWING_REACHED_BIT_POS);

		if (pre_emphasis[i] >= MAX_REACHED_CNT) {
			max_reach_value[i] &= ~(1 << MAX_PRE_EMPHASIS_REACHED_BIT_POS);
			max_reach_value[i] |= (1 << MAX_PRE_EMPHASIS_REACHED_BIT_POS);
		} else
			max_reach_value[i] &= ~(1 << MAX_PRE_EMPHASIS_REACHED_BIT_POS);
	}
}

#define ALLOW_RE_PHY_SET
static int dp_full_link_training(struct dp_device *dp)
{
	u8 link_rate;
	u8 lane_cnt;
	u8 training_aux_rd_interval;
	u8 pre_emphasis[MAX_LANE_CNT];
	u8 drive_current[MAX_LANE_CNT];
	u8 voltage_swing_lane[MAX_LANE_CNT];
	u8 pre_emphasis_lane[MAX_LANE_CNT];
	u8 max_reach_value[MAX_LANE_CNT];
	int training_retry_no, eq_training_retry_no, i;
	int total_retry_cnt = 0;
	u8 val[DPCD_BUF_SIZE] = {0,};
	u8 eq_val[DPCD_BUF_SIZE] = {0,};
	u8 lane_cr_done;
	u8 lane_channel_eq_done;
	u8 lane_symbol_locked_done;
	u8 interlane_align_done;
	u8 enhanced_frame_cap;
	int tps3_supported = 0;
	int tps4_supported = 0;
	enum bit_depth bpc = BPC_8;
	struct decon_device *decon = get_decon_drvdata(CONNETED_DECON_ID);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (dp->pdic_cable_state == PDIC_NOTIFY_DETACH) {
		dp_err(dp, "ccic cable is detached\n");
		return -ENODEV;
	}
#endif
	if (dp_reg_dpcd_read_burst(&dp->cal_res, DPCD_ADD_REVISION_NUMBER, DPCD_BUF_SIZE, val)) {
		dp_info(dp, "aux fail in linktrining\n");
		return -EINVAL;
	}
	dp_info(dp, "Full Link Training Start + : %02x %02x %02x\n", val[1], val[2], val[2]);

	if (dp->hpd_current_state == EXYNOS_HPD_UNPLUG) {
		dp_info(dp, "hpd is low in full link training\n");
		return -EPERM;
	}

	link_rate = val[1];
	lane_cnt = val[2] & MAX_LANE_COUNT;
	tps3_supported = val[2] & TPS3_SUPPORTED;
	enhanced_frame_cap = val[2] & ENHANCED_FRAME_CAP;
	tps4_supported = val[3] & TPS4_SUPPORTED;

	dp_info(dp, "DPCD link_rate = %x, lane_cnt = %x, tps3: %d, tps4: %d, enhanced_frame_cap: %d\n",
			link_rate, lane_cnt, tps3_supported, tps4_supported, enhanced_frame_cap);

	if (link_rate == 0 || lane_cnt == 0) {
		dp_err(dp, "Can't read link rate or lane count in full link training\n");
		return -EINVAL;
	}

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_MAX_LANE_COUNT, lane_cnt);
	secdp_bigdata_save_item(BD_MAX_LINK_RATE, link_rate);
#endif
	if (link_rate > LINK_RATE_5_4Gbps) {
		dp_info(dp, "HBR3 not support. reduce to HBR2\n");
		link_rate = LINK_RATE_5_4Gbps;
	}

	if (!dp->auto_test_mode &&
			!(supported_videos[dp->best_video].pro_audio_support &&
			edid_support_pro_audio())) {
		if (dp->rx_edid_data.hdr_support)
			bpc = BPC_10;

		link_rate = dp_get_min_link_rate(dp, link_rate, lane_cnt, bpc);
		dp->auto_test_mode = 0;
	}

	if (g_dp_debug_param.param_used) {
		link_rate = g_dp_debug_param.link_rate;
		lane_cnt = g_dp_debug_param.lane_cnt;
		dp_info(dp, "link training test lane:%d, rate:0x%x, tps4:0x%x\n",
				lane_cnt, link_rate, tps4_supported);
	}

	dp_reg_dpcd_read(&dp->cal_res, DPCD_ADD_TRAINING_AUX_RD_INTERVAL, 1, val);
	training_aux_rd_interval = val[0] & 0x7F;

	if (forced_linkrate)
		link_rate = forced_linkrate;
Reduce_Link_Rate_Retry:
	dp_info(dp, "Reduce_Link_Rate_Retry(0x%X)\n", link_rate);

	if (dp->hpd_current_state == EXYNOS_HPD_UNPLUG) {
		dp_info(dp, "hpd is low in link rate retry\n");
		return -EPERM;
	}

	for (i = 0; i < 4; i++) {
		pre_emphasis[i] = 0;
		drive_current[i] = 0;
		max_reach_value[i] = 0;
	}

	training_retry_no = 0;

	if (decon->state != DECON_STATE_ON
		|| dp_reg_phy_get_link_bw(&dp->cal_res) != link_rate
		|| dp_reg_get_lane_count() != lane_cnt) {

		if (decon->state == DECON_STATE_ON) {
			dp_info(dp, "phy_reset not permitted on decon on state\n");
			return -EINVAL;
		}
#ifdef ALLOW_RE_PHY_SET
		dp_reg_phy_reset(1);
		dp_reg_phy_init_setting();

		dp_reg_phy_set_link_bw(&dp->cal_res, link_rate);
		dp_info(dp, "link_rate = %x\n", link_rate);

		dp_reg_phy_mode_setting(&dp->cal_res);
#endif
		dp_reg_set_lane_count(lane_cnt);
		dp_info(dp, "lane_cnt = %x\n", lane_cnt);

		if (enhanced_frame_cap)
			dp_write_mask(SST1_MAIN_CONTROL, 1, ENHANCED_MODE);

		/* wait for 60us */
		usleep_range(60, 61);

#ifdef ALLOW_RE_PHY_SET
		dp_reg_phy_reset(0);
#endif
	} else
		dp_info(dp, "skip phy_reset in link training\n");

	val[0] = link_rate;
	val[1] = lane_cnt;

	if (enhanced_frame_cap)
		val[1] |= ENHANCED_FRAME_CAP;

	dp_reg_dpcd_write_burst(&dp->cal_res, DPCD_ADD_LINK_BW_SET, 2, val);

	dp_reg_wait_phy_pll_lock();
	dp_reg_phy_post_init(&dp->cal_res);
	dp_reg_set_training_pattern(TRAINING_PATTERN_1);

	val[0] = 0x21;	/* SCRAMBLING_DISABLE, TRAINING_PATTERN_1 */
	dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_TRANING_PATTERN_SET, 1, val);

#ifdef FEATURE_MANAGE_HMD_LIST
	if (dp->is_hmd_dev &&
		!strncmp(dp->sink_info.monitor_name, "PicoVR", MON_NAME_LEN)) {
		dp_info(dp, "increase swing level\n");
		for (i = 0; i < 4; i++)
			drive_current[i] = MAX_REACHED_CNT - 1;
	}
#endif
Voltage_Swing_Retry:
	dp_debug(dp, "Voltage_Swing_Retry\n");

	if (dp->hpd_current_state == EXYNOS_HPD_UNPLUG) {
		dp_info(dp, "hpd is low in swing retry\n");
		return -EPERM;
	}

	dp_reg_set_voltage_and_pre_emphasis(&dp->cal_res, (u8 *)drive_current, (u8 *)pre_emphasis);
	dp_get_voltage_and_pre_emphasis_max_reach((u8 *)drive_current,
			(u8 *)pre_emphasis, (u8 *)max_reach_value);

	val[0] = (pre_emphasis[0]<<3) | drive_current[0] | max_reach_value[0];
	val[1] = (pre_emphasis[1]<<3) | drive_current[1] | max_reach_value[1];
	val[2] = (pre_emphasis[2]<<3) | drive_current[2] | max_reach_value[2];
	val[3] = (pre_emphasis[3]<<3) | drive_current[3] | max_reach_value[3];
	dp_info(dp, "Voltage_Swing_Retry %02x %02x %02x %02x\n", val[0], val[1], val[2], val[3]);
	dp_reg_dpcd_write_burst(&dp->cal_res, DPCD_ADD_TRANING_LANE0_SET, 4, val);

	if (training_aux_rd_interval != 0)
		usleep_range(training_aux_rd_interval * 4 * 1000,
				training_aux_rd_interval * 4 * 1000 + 1);
	else
		usleep_range(100, 101);

	lane_cr_done = 0;

	dp_reg_dpcd_read(&dp->cal_res, DPCD_ADD_LANE0_1_STATUS, 2, val);
	lane_cr_done |= ((val[0] & LANE0_CR_DONE) >> 0);
	lane_cr_done |= ((val[0] & LANE1_CR_DONE) >> 3);
	lane_cr_done |= ((val[1] & LANE2_CR_DONE) << 2);
	lane_cr_done |= ((val[1] & LANE3_CR_DONE) >> 1);

	dp_debug(dp, "lane_cr_done = %x\n", lane_cr_done);

	if (lane_cnt == 0x04) {
		if (lane_cr_done == 0x0F) {
			dp_debug(dp, "lane_cr_done\n");
			goto EQ_Training_Start;
		} else if (drive_current[0] == MAX_REACHED_CNT
			&& drive_current[1] == MAX_REACHED_CNT
			&& drive_current[2] == MAX_REACHED_CNT
			&& drive_current[3] == MAX_REACHED_CNT) {
			dp_debug(dp, "MAX_REACHED_CNT\n");
			goto Check_Link_rate;
		}
	} else if (lane_cnt == 0x02) {
		if (lane_cr_done == 0x03) {
			dp_debug(dp, "lane_cr_done\n");
			goto EQ_Training_Start;
		} else if (drive_current[0] == MAX_REACHED_CNT
			&& drive_current[1] == MAX_REACHED_CNT) {
			dp_debug(dp, "MAX_REACHED_CNT\n");
			goto Check_Link_rate;
		}
	} else if (lane_cnt == 0x01) {
		if (lane_cr_done == 0x01) {
			dp_debug(dp, "lane_cr_done\n");
			goto EQ_Training_Start;
		} else if (drive_current[0] == MAX_REACHED_CNT) {
			dp_debug(dp, "MAX_REACHED_CNT\n");
			goto Check_Link_rate;
		}
	} else {
		val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
		dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_TRANING_PATTERN_SET, 1, val);
		dp_err(dp, "Full Link Training Fail : Link Rate %02x, lane Count %02x\n",
				link_rate, lane_cnt);
		return -EINVAL;
	}

	dp_reg_dpcd_read_burst(&dp->cal_res, DPCD_ADD_ADJUST_REQUEST_LANE0_1, 2, val);
	voltage_swing_lane[0] = (val[0] & VOLTAGE_SWING_LANE0);
	pre_emphasis_lane[0] = (val[0] & PRE_EMPHASIS_LANE0) >> 2;
	voltage_swing_lane[1] = (val[0] & VOLTAGE_SWING_LANE1) >> 4;
	pre_emphasis_lane[1] = (val[0] & PRE_EMPHASIS_LANE1) >> 6;

	voltage_swing_lane[2] = (val[1] & VOLTAGE_SWING_LANE2);
	pre_emphasis_lane[2] = (val[1] & PRE_EMPHASIS_LANE2) >> 2;
	voltage_swing_lane[3] = (val[1] & VOLTAGE_SWING_LANE3) >> 4;
	pre_emphasis_lane[3] = (val[1] & PRE_EMPHASIS_LANE3) >> 6;

	if (drive_current[0] == voltage_swing_lane[0] &&
			drive_current[1] == voltage_swing_lane[1] &&
			drive_current[2] == voltage_swing_lane[2] &&
			drive_current[3] == voltage_swing_lane[3]) {
		if (training_retry_no == 4)
			goto Check_Link_rate;
		else
			training_retry_no++;
	} else
		training_retry_no = 0;

	for (i = 0; i < 4; i++) {
		drive_current[i] = voltage_swing_lane[i];
		pre_emphasis[i] = pre_emphasis_lane[i];
		dp_debug(dp, "v drive_current[%d] = %x\n",
				i, drive_current[i]);
		dp_debug(dp, "v pre_emphasis[%d] = %x\n",
				i, pre_emphasis[i]);
	}

	total_retry_cnt++;

	if (total_retry_cnt >= MAX_RETRY_CNT) {
		dp_err(dp, "total_retry_cnt = %d\n", total_retry_cnt);
		goto Check_Link_rate;
	}

	goto Voltage_Swing_Retry;

Check_Link_rate:
	dp_info(dp, "Check_Link_rate\n");

	total_retry_cnt = 0;

	if (link_rate == LINK_RATE_8_1Gbps) {
		link_rate = LINK_RATE_5_4Gbps;
		goto Reduce_Link_Rate_Retry;
	} else if (link_rate == LINK_RATE_5_4Gbps) {
		link_rate = LINK_RATE_2_7Gbps;
		goto Reduce_Link_Rate_Retry;
	} else if (link_rate == LINK_RATE_2_7Gbps) {
		link_rate = LINK_RATE_1_62Gbps;
		goto Reduce_Link_Rate_Retry;
	} else if (link_rate == LINK_RATE_1_62Gbps) {
		val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
		dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_TRANING_PATTERN_SET, 1, val);
		dp_err(dp, "Full Link Training Fail : Link_Rate Retry\n");
		return -EINVAL;
	}

EQ_Training_Start:
	dp_info(dp, "EQ_Training_Start\n");

	eq_training_retry_no = 0;
	for (i = 0; i < DPCD_BUF_SIZE; i++)
		eq_val[i] = 0;

	if (tps4_supported) {
		dp_reg_set_training_pattern(TRAINING_PATTERN_4);

		val[0] = 0x07;	/* TRAINING_PATTERN_4 */
		dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_TRANING_PATTERN_SET, 1, val);
	} else if (tps3_supported) {
		dp_reg_set_training_pattern(TRAINING_PATTERN_3);

		val[0] = 0x23;	/* SCRAMBLING_DISABLE, TRAINING_PATTERN_3 */
		dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_TRANING_PATTERN_SET, 1, val);
	} else {
		dp_reg_set_training_pattern(TRAINING_PATTERN_2);

		val[0] = 0x22;	/* SCRAMBLING_DISABLE, TRAINING_PATTERN_2 */
		dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_TRANING_PATTERN_SET, 1, val);
	}

EQ_Training_Retry:
	dp_debug(dp, "EQ_Training_Retry\n");

	if (dp->hpd_current_state == EXYNOS_HPD_UNPLUG) {
		dp_info(dp, "hpd is low in eq training retry\n");
		return -EPERM;
	}

	dp_reg_set_voltage_and_pre_emphasis(&dp->cal_res, (u8 *)drive_current, (u8 *)pre_emphasis);
	dp_get_voltage_and_pre_emphasis_max_reach((u8 *)drive_current,
			(u8 *)pre_emphasis, (u8 *)max_reach_value);

	val[0] = (pre_emphasis[0]<<3) | drive_current[0] | max_reach_value[0];
	val[1] = (pre_emphasis[1]<<3) | drive_current[1] | max_reach_value[1];
	val[2] = (pre_emphasis[2]<<3) | drive_current[2] | max_reach_value[2];
	val[3] = (pre_emphasis[3]<<3) | drive_current[3] | max_reach_value[3];
	dp_info(dp, "EQ_Training_Retry %02x %02x %02x %02x\n", val[0], val[1], val[2], val[3]);
	dp_reg_dpcd_write_burst(&dp->cal_res, DPCD_ADD_TRANING_LANE0_SET, 4, val);

	for (i = 0; i < 4; i++)
		eq_val[i] = val[i];

	lane_cr_done = 0;
	lane_channel_eq_done = 0;
	lane_symbol_locked_done = 0;
	interlane_align_done = 0;

	if (training_aux_rd_interval != 0)
		usleep_range(training_aux_rd_interval * 1000 * 4,
					training_aux_rd_interval * 1000 * 4 + 1);
	else
		usleep_range(100, 101);

	dp_reg_dpcd_read_burst(&dp->cal_res, DPCD_ADD_LANE0_1_STATUS, 3, val);
	lane_cr_done |= ((val[0] & LANE0_CR_DONE) >> 0);
	lane_cr_done |= ((val[0] & LANE1_CR_DONE) >> 3);
	lane_channel_eq_done |= ((val[0] & LANE0_CHANNEL_EQ_DONE) >> 1);
	lane_channel_eq_done |= ((val[0] & LANE1_CHANNEL_EQ_DONE) >> 4);
	lane_symbol_locked_done |= ((val[0] & LANE0_SYMBOL_LOCKED) >> 2);
	lane_symbol_locked_done |= ((val[0] & LANE1_SYMBOL_LOCKED) >> 5);

	lane_cr_done |= ((val[1] & LANE2_CR_DONE) << 2);
	lane_cr_done |= ((val[1] & LANE3_CR_DONE) >> 1);
	lane_channel_eq_done |= ((val[1] & LANE2_CHANNEL_EQ_DONE) << 1);
	lane_channel_eq_done |= ((val[1] & LANE3_CHANNEL_EQ_DONE) >> 2);
	lane_symbol_locked_done |= ((val[1] & LANE2_SYMBOL_LOCKED) >> 0);
	lane_symbol_locked_done |= ((val[1] & LANE3_SYMBOL_LOCKED) >> 3);

	interlane_align_done |= (val[2] & INTERLANE_ALIGN_DONE);

	if (lane_cnt == 0x04) {
		if (lane_cr_done != 0x0F)
			goto Check_Link_rate;
	} else if (lane_cnt == 0x02) {
		if (lane_cr_done != 0x03)
			goto Check_Link_rate;
	} else {
		if (lane_cr_done != 0x01)
			goto Check_Link_rate;
	}

	dp_info(dp, "lane_cr_done = %x\n", lane_cr_done);
	dp_info(dp, "lane_channel_eq_done = %x\n", lane_channel_eq_done);
	dp_info(dp, "lane_symbol_locked_done = %x\n", lane_symbol_locked_done);
	dp_info(dp, "interlane_align_done = %x\n", interlane_align_done);

	if (lane_cnt == 0x04) {
		if ((lane_channel_eq_done == 0x0F) && (lane_symbol_locked_done == 0x0F)
				&& (interlane_align_done == 1)) {
			dp_reg_set_training_pattern(NORAMAL_DATA);

			val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
			dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_TRANING_PATTERN_SET, 1, val);

			goto tr_success;
		}
	} else if (lane_cnt == 0x02) {
		if ((lane_channel_eq_done == 0x03) && (lane_symbol_locked_done == 0x03)
				&& (interlane_align_done == 1)) {
			dp_reg_set_training_pattern(NORAMAL_DATA);

			val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
			dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_TRANING_PATTERN_SET, 1, val);

			goto tr_success;
		}
	} else {
		if ((lane_channel_eq_done == 0x01) && (lane_symbol_locked_done == 0x01)
				&& (interlane_align_done == 1)) {
			dp_reg_set_training_pattern(NORAMAL_DATA);

			val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
			dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_TRANING_PATTERN_SET, 1, val);

			goto tr_success;
		}
	}

	if (training_retry_no == 4)
		goto Check_Link_rate;

	if (eq_training_retry_no >= 5) {
		val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
		dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_TRANING_PATTERN_SET, 1, val);
		dp_err(dp, "Full Link Training Fail : EQ_training Retry\n");
		return -EINVAL;
	}

	dp_reg_dpcd_read_burst(&dp->cal_res, DPCD_ADD_ADJUST_REQUEST_LANE0_1, 2, val);
	voltage_swing_lane[0] = (val[0] & VOLTAGE_SWING_LANE0);
	pre_emphasis_lane[0] = (val[0] & PRE_EMPHASIS_LANE0) >> 2;
	voltage_swing_lane[1] = (val[0] & VOLTAGE_SWING_LANE1) >> 4;
	pre_emphasis_lane[1] = (val[0] & PRE_EMPHASIS_LANE1) >> 6;

	voltage_swing_lane[2] = (val[1] & VOLTAGE_SWING_LANE2);
	pre_emphasis_lane[2] = (val[1] & PRE_EMPHASIS_LANE2) >> 2;
	voltage_swing_lane[3] = (val[1] & VOLTAGE_SWING_LANE3) >> 4;
	pre_emphasis_lane[3] = (val[1] & PRE_EMPHASIS_LANE3) >> 6;

	for (i = 0; i < 4; i++) {
		drive_current[i] = voltage_swing_lane[i];
		pre_emphasis[i] = pre_emphasis_lane[i];

		dp_debug(dp, "eq drive_current[%d] = %x\n", i, drive_current[i]);
		dp_debug(dp, "eq pre_emphasis[%d] = %x\n", i, pre_emphasis[i]);
	}

	eq_training_retry_no++;
	goto EQ_Training_Retry;

tr_success:
	dp_info(dp, "Full Link Training Finish - : %02x %02x\n", link_rate, lane_cnt);
	dp_info(dp, "LANE_SET [%d] : %02x %02x %02x %02x\n",
			eq_training_retry_no, eq_val[0], eq_val[1], eq_val[2], eq_val[3]);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_clr_error_cnt(ERR_LINK_TRAIN);
	secdp_bigdata_save_item(BD_CUR_LANE_COUNT, lane_cnt);
	secdp_bigdata_save_item(BD_CUR_LINK_RATE, link_rate);
#endif
#ifdef FEATURE_USE_DRM_EDID_PARSER
	dp->cur_link_rate = link_rate;
	dp->cur_lane_cnt = lane_cnt;
#endif
	return 0;
}

static int dp_check_dfp_type(struct dp_device *dp)
{
	u8 val = 0;
	int port_type = 0;
	char *dfp[] = {"DP", "VGA", "HDMI", "Others"};

	dp_reg_dpcd_read(&dp->cal_res, DPCD_ADD_DOWN_STREAM_PORT_PRESENT, 1, &val);
	port_type = (val & BIT_DFP_TYPE) >> 1;
	if (port_type > DFP_TYPE_OTHERS)
		port_type = DFP_TYPE_OTHERS;
	dp_info(dp, "DFP type: %s(0x%X)\n", dfp[port_type], val);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_ADAPTER_TYPE, dfp[port_type]);
#endif

	return port_type;
}

static int dp_link_sink_status_read(struct dp_device *dp)
{
	u8 val[DPCD_BUF_SIZE] = {0, };
	int ret = 0;

	ret = dp_reg_dpcd_read_burst(&dp->cal_res, DPCD_ADD_REVISION_NUMBER, DPCD_BUF_SIZE, val);

	if (!ret) {
		dp_info(dp, "Read DPCD REV NUM 0_5 %02x %02x %02x %02x %02x %02x\n",
				val[0], val[1], val[2], val[3], val[4], val[5]);
		dp_info(dp, "Read DPCD REV NUM 6_B %02x %02x %02x %02x %02x %02x\n",
				val[6], val[7], val[8], val[9], val[10], val[11]);
	} else
		dp_info(dp, "%s error\n", __func__);
	return ret;
}

static int dp_read_branch_revision(struct dp_device *dp)
{
	int ret = 0;
	u8 val[4] = {0, };

	ret = dp_reg_dpcd_read_burst(&dp->cal_res, DPCD_BRANCH_HW_REVISION, 3, val);
	if (!ret) {
		dp_info(dp, "Branch revision: HW(0x%X), SW(0x%X, 0x%X)\n",
			val[0], val[1], val[2]);
		dp->sink_info.sw_ver[0] = val[1];
		dp->sink_info.sw_ver[1] = val[2];
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_save_item(BD_ADAPTER_HWID, val[0]);
		secdp_bigdata_save_item(BD_ADAPTER_FWVER, (val[1] << 8) | val[2]);
#endif
	}

	return ret;
}

static int dp_link_status_read(struct dp_device *dp)
{
	u8 val[DPCP_LINK_SINK_STATUS_FIELD_LENGTH] = {0, };
	int count = 200;
	int ret = 0;
	int i;

	/* for Link CTS : Branch Device Detection*/
	ret = dp_link_sink_status_read(dp);
	for (i = 0; ret != 0 && i < 4; i++) {
		msleep(50);
		ret = dp_link_sink_status_read(dp);
	}
	if (ret != 0) {
		dp_err(dp, "link_sink_status_read fail\n");
		return -EINVAL;
	}

	do {
		dp_reg_dpcd_read(&dp->cal_res, DPCD_ADD_SINK_COUNT, 1, val);
		if ((val[0] & (SINK_COUNT1 | SINK_COUNT2)) != 0)
			break;
		usleep_range(20000, 20050);
	} while (--count > 0);

	if (count < 200 - 1)
		dp_info(dp, "%s retry: %d\n", __func__, 200 - count);

	if (count == 0)
		return -EINVAL;

	if (count < 10 && count > 0)
		usleep_range(10000, 11000); /* need delay after SINK count is changed to 1 */

	dp_reg_dpcd_read_burst(&dp->cal_res, DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR,
			DPCP_LINK_SINK_STATUS_FIELD_LENGTH - 1, &val[1]);

	dp_info(dp, "Read link status %02x %02x %02x %02x %02x %02x\n",
				val[0], val[1], val[2], val[3], val[4], val[5]);

	if ((val[0] & val[1] & val[2] & val[3] & val[4] & val[5]) == 0xff)
		return -EINVAL;

	if (val[1] == AUTOMATED_TEST_REQUEST) {
		u8 data = 0;

		dp_reg_dpcd_read(&dp->cal_res, DPCD_TEST_REQUEST, 1, &data);

		if ((data & TEST_EDID_READ) == TEST_EDID_READ) {
			val[0] = edid_read_checksum(dp);
			dp_info(dp, "TEST_EDID_CHECKSUM %02x\n", val[0]);

			dp_reg_dpcd_write(&dp->cal_res, DPCD_TEST_EDID_CHECKSUM, 1, val);

			val[0] = 0x04; /*TEST_EDID_CHECKSUM_WRITE*/
			dp_reg_dpcd_write(&dp->cal_res, DPCD_TEST_RESPONSE, 1, val);

		}
	}

	return 0;
}

static int dp_link_training(struct dp_device *dp)
{
	u8 val;
	int ret = 0;

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (dp->pdic_cable_state == PDIC_NOTIFY_DETACH) {
		dp_info(dp, "pdic cable is detached\n");
		return -ENODEV;
	}
#endif
	if (dp->hpd_current_state == EXYNOS_HPD_UNPLUG) {
		dp_info(dp, "hpd is low in link training\n");
		return -ENODEV;
	}

	mutex_lock(&dp->training_lock);
#ifdef FEATURE_USE_DRM_EDID_PARSER
	ret = edid_update_drm(dp);
#else
	ret = edid_update(dp);
#endif
	if (ret < 0) {
		dp_err(dp, "failed to update edid\n");
		//dp_debug_dump();
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_inc_error_cnt(ERR_EDID);
#endif
	}

	dp_reg_dpcd_read(&dp->cal_res, DPCD_ADD_MAX_DOWNSPREAD, 1, &val);
	dp_debug(dp, "DPCD_ADD_MAX_DOWNSPREAD = %x\n", val);

	ret = dp_full_link_training(dp);
#ifdef FEATURE_USE_DRM_EDID_PARSER
	if (ret)
		goto tr_exit;

	dp->cur_pix_clk = dp_get_cur_bandwidth(dp->cur_link_rate,
				dp->cur_lane_cnt, dp->cal_res.bpc) / 1000;
	dp_info(dp, "cur bandwidth: %u\n", dp->cur_pix_clk);
	/* check if the bandwidth is enough after link training*/
	if (!dp->test_sink && dp->best_mode.clock > dp->cur_pix_clk) {
		int flag = MODE_FILTER_MIRROR;

		dp_find_best_mode(dp, flag);
	}

#ifdef FEATURE_DEX_SUPPORT
	if (!dp->test_sink)
		dp_find_best_mode_for_dex(dp);
#endif

tr_exit:
#endif
	mutex_unlock(&dp->training_lock);

	return ret;
}

int dp_wait_state_change(struct dp_device *dp,
		int max_wait_time, enum dp_state state)
{
	int ret = 0;
	int wait_cnt = max_wait_time;

	dp_info(dp, "wait_state_change start\n");
	dp_info(dp, "max_wait_time = %dms, state = %d\n", max_wait_time, state);

	do {
		wait_cnt--;
		usleep_range(1000, 1030);
	} while ((state != dp->state) && (wait_cnt > 0));

	dp_info(dp, "wait_state_change time = %dms, state = %d\n",
			max_wait_time - wait_cnt, state);

	if (wait_cnt <= 0)
		dp_err(dp, "wait state timeout\n");

	ret = wait_cnt;

	dp_info(dp, "wait_state_change end\n");

	return ret;
}

int dp_wait_audio_off_change(struct dp_device *dp,
		int max_wait_time)
{
	int ret = 0;
	int wait_cnt = max_wait_time;

	dp_info(dp, "wait_audio_off_change start\n");
	dp_info(dp, "max_wait_time = %dms\n", max_wait_time);

	do {
		wait_cnt--;
		usleep_range(1000, 1030);
	} while ((dp->audio_state != 0) && (wait_cnt > 0));

	dp_info(dp, "wait_audio_off time = %dms\n", max_wait_time - wait_cnt);

	if (wait_cnt <= 0)
		dp_err(dp, "wait audio off timeout\n");

	ret = wait_cnt;

	dp_info(dp, "wait_audio_off_change end\n");

	return ret;
}

int dp_wait_decon_run(struct dp_device *dp,
		int max_wait_time)
{
	struct decon_device *decon = get_decon_drvdata(CONNETED_DECON_ID);
	int ret = 0;

	ret = wait_event_interruptible_timeout(
			decon->framedone_wait,
			decon->busy == false, msecs_to_jiffies(max_wait_time));
	if (ret <= 0)
		dp_err(dp, "wait_decon_run timeout\n");
	else
		dp->decon_run = 1;

	return ret;
}

void dp_on_by_hpd_high(struct dp_device *dp)
{
	struct drm_connector *connector = &dp->connector;
	struct drm_device *dev = connector->dev;
	int timeout = 0;

	dp_reg_audio_init_config(&dp->cal_res);

	if (dp->bist_used) {
		dp->state = DP_STATE_INIT;
		dp_enable(&dp->encoder); /* for bist video enable */
		//dp_debug_dump();
	} else {
		dp->state = DP_STATE_INIT;
		dp->connector.status = connector_status_connected;
		drm_kms_helper_hotplug_event(dev);
		dp_info(dp, "drm_kms_helper_hotplug_event(connected)\n");
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
		dp_set_switch_hpd_state(1);
#endif
		timeout = dp_wait_state_change(dp, 3000, DP_STATE_ON);

		dp_wait_decon_run(dp, 3000);

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
		blocking_notifier_call_chain(&dp_ado_notifier_head,
			(unsigned long)edid_audio_informs(dp), NULL);
#endif
	}

}

void dp_off_by_hpd_low(struct dp_device *dp)
{
	int timeout = 0;
	struct decon_device *decon;
	struct drm_connector *connector = &dp->connector;
	struct drm_device *dev = connector->dev;

	if (dp->state == DP_STATE_ON
		|| dp->state == DP_STATE_INIT) {
		dp_info(dp, "%s state on or init\n", __func__);
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
		blocking_notifier_call_chain(&dp_ado_notifier_head,
				(unsigned long)-1, NULL);
		dp_info(dp, "audio info = -1\n");
		dp_wait_audio_off_change(dp, 5000);
#endif
		if (dp->bist_used == 0) {
			dp_info(dp, "%s(%d)\n", __func__, __LINE__);
			dp->connector.status = connector_status_disconnected;
			drm_kms_helper_hotplug_event(dev);
			dp_info(dp, "drm_kms_helper_hotplug_event(disconnected)\n");
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
			dp_set_switch_hpd_state(0);
#endif

			timeout = dp_wait_state_change(dp, 3000, DP_STATE_OFF);

			if (timeout <= 0) {
				dp_err(dp, "disable timeout\n");

				decon = get_decon_drvdata(CONNETED_DECON_ID);

				if (dp->state == DP_STATE_INIT) {
					dp_info(dp, "not enabled\n");
				} else if (decon->state == DECON_STATE_OFF &&
						dp->state == DP_STATE_ON) {
					dp_err(dp, "abnormal state: decon%d:%d, dp:%d\n",
							decon->id, decon->state, dp->state);
					dp_disable(&dp->encoder);
				}
			}
		} else {
			dp_info(dp, "%s(%d)\n", __func__, __LINE__);
			dp_disable(&dp->encoder); /* for bist video disable */
		}
	} else {
		if (dp->bist_used == 0) {
			dp->connector.status = connector_status_disconnected;
			drm_kms_helper_hotplug_event(dev);
			dp_info(dp, "drm_kms_helper_hotplug_event(disconnected) dp_state_off\n");
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
			dp_set_switch_hpd_state(0);
#endif
		}
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
		blocking_notifier_call_chain(&dp_ado_notifier_head,
				(unsigned long)-1, NULL);
		dp_info(dp, "audio info = -1\n");
		dp_wait_audio_off_change(dp, 5000);
#endif
		dp_info(dp, "%s state off\n", __func__);
	}
	dp->best_video = V640X480P60;
	dp->cal_res.bpc = BPC_8;
	dp->dyn_range = VESA_RANGE;
}

int dp_wait_usb_init(struct dp_device *dp)
{
#define USB_ININ_WAIT_CNT 30
#define USB_ININ_WAIT_TIME 10000
	u32 cnt = USB_ININ_WAIT_CNT;
	int state = 0;

	do {
		state = dwc3_otg_is_usb_ready();
		usleep_range(USB_ININ_WAIT_TIME, USB_ININ_WAIT_TIME + 10);
		cnt--;
	} while (!state && cnt);

	if (!cnt) {
		dp_err(dp, "usb init fail on 300msec!\n");
		return -EACCES;
	}

	return 0;
}

void dp_hpd_changed(struct dp_device *dp,
		enum hotplug_state state)
{
	int ret = 0;

#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER
	dp_logger_set_max_count(500);
#endif
	mutex_lock(&dp->hpd_lock);
	if (dp->hpd_current_state == state) {
		dp_info(dp, "hpd same state skip %x\n", state);
		mutex_unlock(&dp->hpd_lock);
		return;
	}

	/*dp->bist_used = forced_bist_en;*/

	if (state == EXYNOS_HPD_PLUG || state == EXYNOS_HPD_IRQ) {
		if (dp_wait_usb_init(dp) != 0) {
			dp_err(dp, "skip dp hpd state change(%d)\n", state);
			goto USB_INIT_FAIL;
		}

		dp_info(dp, "dp hpd changed %s\n",
			(state != EXYNOS_HPD_UNPLUG) ? "EXYNOS_HPD_PLUG" : "EXYNOS_HPD_UNPLUG");
		dp->hpd_current_state = state;

		pm_runtime_get_sync(dp->dev);
		dp_info(dp, "pm_rtm_get_sync usage_cnt(%d)\n",
			atomic_read(&dp->dev->power.usage_count));
		if (dp->res.dposc_clk)
			clk_prepare_enable(dp->res.dposc_clk);
		dp_hsi0_domain_power_check();
		dp_hsi0_clk_check();
		pm_stay_awake(dp->dev);

		/* PHY power on */
		usleep_range(10000, 10030);
		dp_reg_sw_reset(&dp->cal_res);
		dp_reg_init(&dp->cal_res); /* for AUX ch read/write. */
		dp_hdcp22_notify_state(DP_CONNECT);

		dp->cal_res.bpc = BPC_8;	/* default setting */
		dp->dyn_range = VESA_RANGE;
		dp->auto_test_mode = 0;
		dp->bist_used = 0;
		dp->best_video = EDID_DEFAULT_TIMINGS_IDX;

		usleep_range(10000, 11000);
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		if (dp->pdic_cable_state == PDIC_NOTIFY_DETACH) {
			dp_err(dp, "pdic cable is detached\n");
			goto HPD_FAIL;
		}
#endif

		/* for Link CTS : (4.2.2.3) EDID Read */
		if (dp_link_status_read(dp) == -EINVAL) {
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
			dp_set_switch_poor_connect();
#endif
			dp_err(dp, "link_status_read fail %d\n", ++dp_hdp_read_fail);
			goto HPD_FAIL;
		}

		if (dp_read_branch_revision(dp))
			dp_err(dp, "branch_revision_read fail\n");

		dp->dfp_type = dp_check_dfp_type(dp);
		/* Enable it! if you want to prevent output according to type
		 * if (dp->dfp_type != DFP_TYPE_DP &&
		 *		dp->dfp_type != DFP_TYPE_HDMI) {
		 *	dp_err(dp, "not supported DFT type\n");
		 *	goto HPD_FAIL;
		 * }
		 */

		dp_info(dp, "link training in hpd_changed\n");
		ret = dp_link_training(dp);
		if (ret < 0) {
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
			dp_set_switch_poor_connect();
#endif
			dp_info(dp, "link training fail %d\n", ++dp_hdp_link_tr_fail);
			//dp_debug_dump();
			goto HPD_FAIL;
		}

		dp_info(dp, "dp_on_by_hpd_high in hpd changed\n");
		dp_on_by_hpd_high(dp);
	} else {/* state == EXYNOS_HPD_UNPLUG; */
		dp_info(dp, "dp hpd changed %s\n",
			(state != EXYNOS_HPD_UNPLUG) ? "EXYNOS_HPD_PLUG" : "EXYNOS_HPD_UNPLUG");
		dp->hpd_current_state = state;

		dp_reg_print_audio_state();
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
		if (dp->hdcp_ver == HDCP_VERSION_2_2)
			hdcp_dplink_cancel_auth();
#endif
		cancel_delayed_work_sync(&dp->hpd_unplug_work);
		cancel_delayed_work_sync(&dp->hpd_irq_work);
		cancel_delayed_work_sync(&dp->hdcp13_work);
		cancel_delayed_work_sync(&dp->hdcp22_work);
		cancel_delayed_work_sync(&dp->hdcp13_integrity_check_work);

		dp_off_by_hpd_low(dp);

		dp_reg_deinit();
		dp_reg_phy_disable(&dp->cal_res);
		if (dp->res.dposc_clk)
			clk_disable_unprepare(dp->res.dposc_clk);
		dp_hsi0_clk_check();
		pm_runtime_put_sync(dp->dev);
		dp_info(dp, "pm_rtm_put_sync usage_cnt(%d)\n",
			atomic_read(&dp->dev->power.usage_count));
		dp_hsi0_domain_power_check();
#if defined(CONFIG_CPU_IDLE)
		/* unblock to enter SICD mode */
		exynos_update_ip_idle_status(dp->idle_ip_index, 1);
#endif
		pm_relax(dp->dev);
#ifdef FEATURE_USE_DRM_EDID_PARSER
		memset(&dp->cur_mode, 0, sizeof(struct drm_display_mode));
#endif
		dp->hdcp_ver = 0;
		dp_info(dp, "hpd is disabled in hpd changed\n");
	}

USB_INIT_FAIL:
	mutex_unlock(&dp->hpd_lock);

	return;

HPD_FAIL:
	dp_err(dp, "HPD FAIL Check PDIC or USB!!\n");
	dp->hpd_current_state = EXYNOS_HPD_UNPLUG;

	dp_reg_deinit();
	dp_reg_phy_disable(&dp->cal_res);
	pm_relax(dp->dev);

	dp_init_info(dp);

	if (dp->res.dposc_clk)
		clk_disable_unprepare(dp->res.dposc_clk);
	dp_hsi0_clk_check();
	pm_runtime_put_sync(dp->dev);
	dp_info(dp, "pm_rtm_put_sync usage_cnt(%d)\n",
		atomic_read(&dp->dev->power.usage_count));
	dp_hsi0_domain_power_check();
#ifdef FEATURE_USE_DRM_EDID_PARSER
	memset(&dp->cur_mode, 0, sizeof(struct drm_display_mode));
#endif
	/* in error case, add delay to avoid very short interval reconnection */
	msleep(300);
	mutex_unlock(&dp->hpd_lock);

	return;
}

void dp_set_reconnection(struct dp_device *dp)
{
	int ret;

	/* PHY power on */
	dp_reg_init(&dp->cal_res); /* for AUX ch read/write. */

	dp_info(dp, "link training in reconnection\n");
	ret = dp_link_training(dp);
	if (ret < 0) {
		dp_debug(dp, "link training fail\n");
		return;
	}
}

static void dp_hpd_unplug_work(struct work_struct *work)
{
	struct dp_device *dp = get_dp_drvdata();

	dp_info(dp, "hpd_unplug_work\n");
	dp_off_by_hpd_low(dp);
}

static int dp_check_dpcd_lane_status(struct dp_device *dp,
		u8 lane0_1_status, u8 lane2_3_status, u8 lane_align_status)
{
	u8 val[2] = {0,};
	u32 link_rate = dp_reg_phy_get_link_bw(&dp->cal_res);
	u32 lane_cnt = dp_reg_get_lane_count();

	dp_reg_dpcd_read(&dp->cal_res, DPCD_ADD_LINK_BW_SET, 2, val);

	dp_info(dp, "check lane %02x %02x %02x %02x\n", link_rate, lane_cnt,
			val[0], (val[1] & MAX_LANE_COUNT));

	if ((link_rate != val[0]) || (lane_cnt != (val[1] & MAX_LANE_COUNT))) {
		dp_err(dp, "%s() link rate, lane_cnt error\n", __func__);
		return -EINVAL;
	}

	if (!(lane_align_status & LINK_STATUS_UPDATE))
		return 0;

	dp_err(dp, "%s() link_status_update bit is set\n", __func__);

	if ((lane_align_status & INTERLANE_ALIGN_DONE) != INTERLANE_ALIGN_DONE) {
		dp_err(dp, "%s() interlane align error\n", __func__);
		return -EINVAL;
	}

	if (lane_cnt >= 1) {
		if ((lane0_1_status & (LANE0_CR_DONE | LANE0_CHANNEL_EQ_DONE | LANE0_SYMBOL_LOCKED))
				!= (LANE0_CR_DONE | LANE0_CHANNEL_EQ_DONE | LANE0_SYMBOL_LOCKED)) {
			dp_err(dp, "%s() lane0 status error\n", __func__);
			return -EINVAL;
		}
	}

	if (lane_cnt >= 2) {
		if ((lane0_1_status & (LANE1_CR_DONE | LANE1_CHANNEL_EQ_DONE | LANE1_SYMBOL_LOCKED))
				!= (LANE1_CR_DONE | LANE1_CHANNEL_EQ_DONE | LANE1_SYMBOL_LOCKED)) {
			dp_err(dp, "%s() lane1 status error\n", __func__);
			return -EINVAL;
		}
	}

	if (lane_cnt == 4) {
		if ((lane2_3_status & (LANE2_CR_DONE | LANE2_CHANNEL_EQ_DONE | LANE2_SYMBOL_LOCKED))
				!= (LANE2_CR_DONE | LANE2_CHANNEL_EQ_DONE | LANE2_SYMBOL_LOCKED)) {
			dp_err(dp, "%s() lane2 stat error\n", __func__);
			return -EINVAL;
		}

		if ((lane2_3_status & (LANE3_CR_DONE | LANE3_CHANNEL_EQ_DONE | LANE3_SYMBOL_LOCKED))
				!= (LANE3_CR_DONE | LANE3_CHANNEL_EQ_DONE | LANE3_SYMBOL_LOCKED)) {
			dp_err(dp, "%s() lane3 status error\n", __func__);
			return -EINVAL;
		}
	}

	return 0;
}

static int dp_automated_test_set_lane_req(struct dp_device *dp, u8 *val)
{
	u8 drive_current[MAX_LANE_CNT];
	u8 pre_emphasis[MAX_LANE_CNT];
	u8 voltage_swing_lane[MAX_LANE_CNT];
	u8 pre_emphasis_lane[MAX_LANE_CNT];
	u8 max_reach_value[MAX_LANE_CNT] = {0, };
	u8 val2[MAX_LANE_CNT];
	int i;

	voltage_swing_lane[0] = (val[0] & VOLTAGE_SWING_LANE0);
	pre_emphasis_lane[0] = (val[0] & PRE_EMPHASIS_LANE0) >> 2;
	voltage_swing_lane[1] = (val[0] & VOLTAGE_SWING_LANE1) >> 4;
	pre_emphasis_lane[1] = (val[0] & PRE_EMPHASIS_LANE1) >> 6;

	voltage_swing_lane[2] = (val[1] & VOLTAGE_SWING_LANE2);
	pre_emphasis_lane[2] = (val[1] & PRE_EMPHASIS_LANE2) >> 2;
	voltage_swing_lane[3] = (val[1] & VOLTAGE_SWING_LANE3) >> 4;
	pre_emphasis_lane[3] = (val[1] & PRE_EMPHASIS_LANE3) >> 6;

	for (i = 0; i < 4; i++) {
		drive_current[i] = voltage_swing_lane[i];
		pre_emphasis[i] = pre_emphasis_lane[i];

		dp_info(dp, "AutoTest: swing[%d] = %x\n", i, drive_current[i]);
		dp_info(dp, "AutoTest: pre_emphasis[%d] = %x\n", i, pre_emphasis[i]);
	}
	dp_reg_set_voltage_and_pre_emphasis(&dp->cal_res, (u8 *)drive_current, (u8 *)pre_emphasis);
	dp_get_voltage_and_pre_emphasis_max_reach((u8 *)drive_current,
			(u8 *)pre_emphasis, (u8 *)max_reach_value);

	val2[0] = (pre_emphasis[0]<<3) | drive_current[0] | max_reach_value[0];
	val2[1] = (pre_emphasis[1]<<3) | drive_current[1] | max_reach_value[1];
	val2[2] = (pre_emphasis[2]<<3) | drive_current[2] | max_reach_value[2];
	val2[3] = (pre_emphasis[3]<<3) | drive_current[3] | max_reach_value[3];
	dp_info(dp, "AutoTest: set %02x %02x %02x %02x\n", val2[0], val2[1], val2[2], val2[3]);
	dp_reg_dpcd_write_burst(&dp->cal_res, DPCD_ADD_TRANING_LANE0_SET, 4, val2);

	return 0;
}

static void dp_automated_test_pattern_set(struct dp_device *dp,
			u8 dpcd_test_pattern_type)
{
	switch (dpcd_test_pattern_type) {
	case DPCD_TEST_PATTERN_COLOR_RAMPS:
		dp->bist_type = CTS_COLOR_RAMP;
		break;
	case DPCD_TEST_PATTERN_BW_VERTICAL_LINES:
		dp->bist_type = CTS_BLACK_WHITE;
		break;
	case DPCD_TEST_PATTERN_COLOR_SQUARE:
		if (dp->dyn_range == CEA_RANGE)
			dp->bist_type = CTS_COLOR_SQUARE_CEA;
		else
			dp->bist_type = CTS_COLOR_SQUARE_VESA;
		break;
	default:
		dp->bist_type = COLOR_BAR;
		break;
	}
}

static int dp_Automated_Test_Request(struct dp_device *dp)
{
	u8 data = 0;
	u8 val[DPCP_LINK_SINK_STATUS_FIELD_LENGTH] = {0, };
	struct drm_connector *connector = &dp->connector;
	struct drm_device *dev = connector->dev;
	u8 dpcd_test_pattern_type = 0;

	dp_reg_dpcd_read(&dp->cal_res, DPCD_TEST_REQUEST, 1, &data);
	dp_info(dp, "TEST_REQUEST %02x\n", data);

	dp->auto_test_mode = 1;
	val[0] = 0x01; /*TEST_ACK*/
	dp_reg_dpcd_write(&dp->cal_res, DPCD_TEST_RESPONSE, 1, val);

	if ((data & TEST_LINK_TRAINING) == TEST_LINK_TRAINING) {
		videoformat cur_video = dp->cur_video;
		u8 bpc = (u8)dp->cal_res.bpc;
		u8 bist_type = (u8)dp->bist_type;
		u8 dyn_range = (u8)dp->dyn_range;

		dp->connector.status = connector_status_disconnected;
		drm_kms_helper_hotplug_event(dev);
		dp_info(dp, "drm_kms_helper_hotplug_event(disconnected)\n");
		dp_wait_state_change(dp, 1000, DP_STATE_OFF); //??

		/* PHY power on */
		dp_reg_init(&dp->cal_res);

		dp_reg_dpcd_read(&dp->cal_res, DPCD_TEST_LINK_RATE, 1, val);
		dp_info(dp, "TEST_LINK_RATE %02x\n", val[0]);
		g_dp_debug_param.link_rate = (val[0]&TEST_LINK_RATE);

		dp_reg_dpcd_read(&dp->cal_res, DPCD_TEST_LANE_COUNT, 1, val);
		dp_info(dp, "TEST_LANE_COUNT %02x\n", val[0]);
		g_dp_debug_param.lane_cnt = (val[0]&TEST_LANE_COUNT);

		g_dp_debug_param.param_used = 1;
		dp_link_training(dp);
		g_dp_debug_param.param_used = 0;

		dp->bist_used = 1;
		dp->bist_type = COLOR_BAR;
		dp_reg_set_bist_video_config(&dp->cal_res, cur_video,
				bpc, bist_type, dyn_range);
		dp_reg_start();
	} else if ((data & TEST_VIDEO_PATTERN) == TEST_VIDEO_PATTERN) {
		if (dp->state == DP_STATE_OFF) {
			if (!atomic_read(&dp->dev->power.usage_count))
				pm_runtime_get_sync(dp->dev);
			if (dp->res.dposc_clk)
				clk_prepare_enable(dp->res.dposc_clk);
			dp_hsi0_clk_check();
			dp_hsi0_domain_power_check();
			dp->connector.status = connector_status_disconnected;
			drm_kms_helper_hotplug_event(dev);
			dp_info(dp, "drm_kms_helper_hotplug_event(disconnected)\n");
			dp_wait_state_change(dp, 1000, DP_STATE_OFF); //??

			/* PHY power on */
			dp_reg_init(&dp->cal_res); /* for AUX ch read/write. */
			g_dp_debug_param.param_used = 1;
			dp_link_training(dp);
			g_dp_debug_param.param_used = 0;
		}

		dp_reg_dpcd_read(&dp->cal_res, DPCD_TEST_PATTERN, 1, val);
		dp_info(dp, "TEST_PATTERN %02x\n", val[0]);

		dpcd_test_pattern_type = val[0];

		dp_automated_test_pattern_set(dp, dpcd_test_pattern_type);

		dp->cur_video = dp->best_video;

		dp_reg_set_bist_video_config(&dp->cal_res, dp->cur_video,
			dp->cal_res.bpc, CTS_COLOR_RAMP, dp->dyn_range);
		dp_reg_start();
	} else if ((data & TEST_PHY_TEST_PATTERN) == TEST_PHY_TEST_PATTERN) {
		dp_reg_stop();
		msleep(120);
		dp_reg_dpcd_read(&dp->cal_res, DPCD_ADD_ADJUST_REQUEST_LANE0_1, 2, val);
		dp_info(dp, "ADJUST_REQUEST_LANE0_1 %02x %02x\n", val[0], val[1]);

		/*set swing, preemp*/
		dp_automated_test_set_lane_req(dp, val);

		dp_reg_dpcd_read(&dp->cal_res, DCDP_ADD_PHY_TEST_PATTERN, 4, val);
		dp_info(dp, "PHY_TEST_PATTERN %02x %02x %02x %02x\n", val[0], val[1], val[2], val[3]);

		switch (val[0]) {
		case DISABLE_PATTEN:
			dp_reg_set_qual_pattern(DISABLE_PATTEN, ENABLE_SCRAM);
			break;
		case D10_2_PATTERN:
			dp_reg_set_qual_pattern(D10_2_PATTERN, DISABLE_SCRAM);
			break;
		case SERP_PATTERN:
			dp_reg_set_qual_pattern(SERP_PATTERN, ENABLE_SCRAM);
			break;
		case PRBS7:
			dp_reg_set_qual_pattern(PRBS7, DISABLE_SCRAM);
			break;
		case CUSTOM_80BIT:
			dp_reg_set_pattern_PLTPAT();
			dp_reg_set_qual_pattern(CUSTOM_80BIT, DISABLE_SCRAM);
			break;
		case HBR2_COMPLIANCE:
			/*option 0*/
			/*dp_reg_set_hbr2_scrambler_reset(252);*/

			/*option 1*/
			dp_reg_set_hbr2_scrambler_reset(252*2);

			/*option 2*/
			/*dp_reg_set_hbr2_scrambler_reset(252);*/
			/*dp_reg_set_PN_Inverse_PHY_Lane(1);*/

			/*option 3*/
			/*dp_reg_set_hbr2_scrambler_reset(252*2);*/
			/*dp_reg_set_PN_Inverse_PHY_Lane(1);*/

			dp_reg_set_qual_pattern(HBR2_COMPLIANCE, ENABLE_SCRAM);
			break;
		default:
			dp_err(dp, "not supported link qual pattern");
			break;
		}
	} else if ((data & TEST_AUDIO_PATTERN) == TEST_AUDIO_PATTERN) {
		struct dp_audio_config_data audio_config_data = {0, };

		dp_reg_dpcd_read(&dp->cal_res, DPCD_TEST_AUDIO_MODE, 1, val);
		dp_info(dp, "TEST_AUDIO_MODE %02x %02x\n",
				(val[0] & TEST_AUDIO_SAMPLING_RATE),
				(val[0] & TEST_AUDIO_CHANNEL_COUNT) >> 4);

		dp_reg_dpcd_read(&dp->cal_res, DPCD_TEST_AUDIO_PATTERN_TYPE, 1, val);
		dp_info(dp, "TEST_AUDIO_PATTERN_TYPE %02x\n", val[0]);

		msleep(300);

		audio_config_data.audio_enable = 1;
		audio_config_data.audio_fs =  (val[0] & TEST_AUDIO_SAMPLING_RATE);
		audio_config_data.audio_channel_cnt = (val[0] & TEST_AUDIO_CHANNEL_COUNT) >> 4;
		audio_config_data.audio_channel_cnt++;
		audio_config_data.audio_bit = AUDIO_16_BIT;
		audio_config_data.audio_packed_mode = 0;
		audio_config_data.audio_word_length = 0;
		dp_audio_bist_config(dp, audio_config_data);
	} else {
		dp_err(dp, "Not Supported AUTOMATED_TEST_REQUEST\n");
		return -EINVAL;
	}
	return 0;
}

static void dp_hpd_irq_work(struct work_struct *work)
{
	u8 val[DPCP_LINK_SINK_STATUS_FIELD_LENGTH] = {0, };
	struct dp_device *dp = get_dp_drvdata();

	if (dp->hpd_current_state == EXYNOS_HPD_UNPLUG) {
		dp_info(dp, "HPD IRQ work: hpd is low\n");
		return;
	}

	dp_debug(dp, "detect HPD_IRQ\n");

	if (dp->hdcp_ver == HDCP_VERSION_2_2) {
		int ret = dp_reg_dpcd_read_burst(&dp->cal_res, DPCD_ADD_SINK_COUNT,
				DPCP_LINK_SINK_STATUS_FIELD_LENGTH, val);

		dp_info(dp, "HPD IRQ work2 %02x %02x %02x %02x %02x %02x\n",
				val[0], val[1], val[2], val[3], val[4], val[5]);
		if (ret < 0 || (val[0] & val[1] & val[2] & val[3] & val[4] & val[5]) == 0xff) {
			dp_info(dp, "dpcd_read error in HPD IRQ work\n");
			return;
		}

		if ((val[1] & AUTOMATED_TEST_REQUEST) == AUTOMATED_TEST_REQUEST) {
			if (dp_Automated_Test_Request(dp) == 0)
				return;
		}

		if (dp_check_dpcd_lane_status(dp, val[2], val[3], val[4]) != 0) {
			dp_info(dp, "link training in HPD IRQ work2\n");

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_inc_error_cnt(ERR_INF_IRQHPD);
#endif
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
			hdcp_dplink_set_reauth();
#endif
			dp_hdcp22_enable(0);

			dp_link_training(dp);
			queue_delayed_work(dp->hdcp2_wq,
					&dp->hdcp22_work, msecs_to_jiffies(2000));
		}
		if ((val[1] & CP_IRQ) == CP_IRQ) {
			dp_info(dp, "hdcp22: detect CP_IRQ\n");
			ret = dp_hdcp22_irq_handler(dp);
			if (ret == 0)
				return;
		}
		return;
	}

	if (hdcp13_info.auth_state != HDCP13_STATE_AUTH_PROCESS) {
		int ret = dp_reg_dpcd_read_burst(&dp->cal_res, DPCD_ADD_SINK_COUNT,
				DPCP_LINK_SINK_STATUS_FIELD_LENGTH, val);

		dp_info(dp, "HPD IRQ work1 %02x %02x %02x %02x %02x %02x\n",
				val[0], val[1], val[2], val[3], val[4], val[5]);
		if (ret < 0 || (val[0] & val[1] & val[2] & val[3] & val[4] & val[5]) == 0xff) {
			dp_info(dp, "dpcd_read error in HPD IRQ work\n");
			return;
		}

		if ((val[1] & CP_IRQ) == CP_IRQ && dp->hdcp_ver == HDCP_VERSION_1_3) {
			dp_info(dp, "detect CP_IRQ\n");
			hdcp13_info.cp_irq_flag = 1;
			hdcp13_info.link_check = LINK_CHECK_NEED;
			hdcp13_link_integrity_check(dp);

			if (hdcp13_info.auth_state == HDCP13_STATE_FAIL)
				queue_delayed_work(dp->dp_wq,
						&dp->hdcp13_work, msecs_to_jiffies(2000));
		}

		if ((val[1] & AUTOMATED_TEST_REQUEST) == AUTOMATED_TEST_REQUEST) {
			if (dp_Automated_Test_Request(dp) == 0)
				return;
		}

		if (dp->hpd_current_state == EXYNOS_HPD_UNPLUG) {
			dp_info(dp, "HPD IRQ work: hpd is low\n");
			return;
		}

		if (dp_check_dpcd_lane_status(dp, val[2], val[3], val[4]) != 0) {
			dp_info(dp, "link training in HPD IRQ work1\n");
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_inc_error_cnt(ERR_INF_IRQHPD);
#endif
			dp_link_training(dp);

			hdcp13_info.auth_state = HDCP13_STATE_NOT_AUTHENTICATED;
			queue_delayed_work(dp->dp_wq,
					&dp->hdcp13_work, msecs_to_jiffies(2000));
		}
	} else {
		dp_reg_dpcd_read(&dp->cal_res, DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR, 1, val);

		if ((val[0] & CP_IRQ) == CP_IRQ) {
			dp_info(dp, "detect CP_IRQ\n");
			hdcp13_info.cp_irq_flag = 1;
			dp_reg_dpcd_read(&dp->cal_res, ADDR_HDCP13_BSTATUS, 1, HDCP13_DPCD.HDCP13_BSTATUS);
		}
	}
}

static void dp_hdcp13_integrity_check_work(struct work_struct *work)
{
	struct dp_device *dp = get_dp_drvdata();

	if (dp->hdcp_ver == HDCP_VERSION_1_3) {
		hdcp13_info.link_check = LINK_CHECK_NEED;
		hdcp13_link_integrity_check(dp);
	}
}

static void dp_print_bts_info(struct dp_device *dp)
{
	struct decon_device *decon = get_decon_drvdata(CONNETED_DECON_ID);

	dp_info(dp, "bw(prev:%u curr:%u), disp(prev:%u curr:%u), peak(%u)\n",
			decon->bts.prev_total_bw,
			decon->bts.total_bw, decon->bts.prev_max_disp_freq,
			decon->bts.max_disp_freq, decon->bts.peak);

	dp_info(dp, "MIF(%lu), INT(%lu), DISP(%lu)\n",
			exynos_devfreq_get_domain_freq(DEVFREQ_MIF),
			exynos_devfreq_get_domain_freq(DEVFREQ_INT),
			exynos_devfreq_get_domain_freq(DEVFREQ_DISP));

	if (dp_log_level >= 7 && IS_ENABLED(CONFIG_EXYNOS_BTS))
		decon->bts.ops->print_info(decon->crtc);
}

static irqreturn_t dp_irq_handler(int irq, void *dev_data)
{
	struct dp_device *dp = dev_data;
	struct drm_crtc *crtc = dp->encoder.crtc;
	struct decon_device *decon = NULL;
	int active;
	u32 irq_status_reg;

	spin_lock(&dp->slock);

	active = pm_runtime_active(dp->dev);
	if (!active) {
		dp_info(dp, "dp power(%d)\n", active);
		spin_unlock(&dp->slock);
		return IRQ_HANDLED;
	}

	/* Common interrupt */
	irq_status_reg = dp_reg_get_common_interrupt_and_clear();

#if !IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER) && !IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	if (irq_status_reg & HPD_CHG)
		dp_info(dp, "HPD_CHG detect\n");

	if (irq_status_reg & HPD_LOST) {
		dp_info(dp, "HPD_LOST detect\n");
	}

	if (irq_status_reg & HPD_PLUG) {
		dp_info(dp, "HPD_PLUG detect\n");
	}

	if (irq_status_reg & HPD_IRQ) {
		dp_info(dp, "HPD IRQ detect\n");
	}
#endif

	if (irq_status_reg & HDCP_LINK_CHK_FAIL) {
		queue_delayed_work(dp->dp_wq, &dp->hdcp13_integrity_check_work, 0);
		dp_info(dp, "HDCP_LINK_CHK detect\n");
	}

	if (irq_status_reg & HDCP_R0_CHECK_FLAG) {
		hdcp13_info.r0_read_flag = 1;
		dp_info(dp, "R0_CHECK_FLAG detect\n");
	}

	irq_status_reg = dp_reg_get_video_interrupt_and_clear();

	if ((irq_status_reg & MAPI_FIFO_UNDER_FLOW) && dp->decon_run == 1 &&
			dp->hpd_current_state != EXYNOS_HPD_UNPLUG) {
		decon = get_decon_drvdata(CONNETED_DECON_ID);
		dp_err(dp, "VIDEO FIFO_UNDER_FLOW detect\n");
		decon->bts.ops->print_info(decon->crtc);

		if (dp_underrun_dump) {
			__decon_dump(get_decon_drvdata(0)->id,
				&get_decon_drvdata(0)->regs,
				get_decon_drvdata(0)->config.dsc.enabled);
			__decon_dump(get_decon_drvdata(2)->id,
				&get_decon_drvdata(2)->regs,
				get_decon_drvdata(2)->config.dsc.enabled);
			dpp_dump(get_decon_drvdata(0)->dpp,
				get_decon_drvdata(0)->dpp_cnt);
			mdelay(1000);
			BUG();
		}
	}

	if (dp->bist_used == 0) {
		decon = get_decon_drvdata(CONNETED_DECON_ID);

		if (irq_status_reg & VSYNC_DET)
			if (decon->config.mode.op_mode == DECON_VIDEO_MODE && crtc)
				drm_crtc_handle_vblank(crtc);
	}

	irq_status_reg = dp_reg_get_audio_interrupt_and_clear();

	if (irq_status_reg & MASTER_AUDIO_BUFFER_EMPTY_INT) {
		dp_debug(dp, "AFIFO_UNDER detect\n");
		dp->cal_res.audio_buf_empty_check = 1;
	}

	spin_unlock(&dp->slock);

	return IRQ_HANDLED;
}

static u8 dp_get_vic(struct dp_device *dp)
{
#ifdef FEATURE_USE_DRM_EDID_PARSER
	dp->cur_mode_vic = drm_match_cea_mode(&dp->cur_mode);
	return dp->cur_mode_vic;
#else
	return supported_videos[dp->cur_video].vic;
#endif
}

static int dp_make_avi_infoframe_data(struct dp_device *dp,
		struct infoframe *avi_infoframe)
{
	int i;

	avi_infoframe->type_code = INFOFRAME_PACKET_TYPE_AVI;
	avi_infoframe->version_number = AVI_INFOFRAME_VERSION;
	avi_infoframe->length = AVI_INFOFRAME_LENGTH;

	for (i = 0; i < AVI_INFOFRAME_LENGTH; i++)
		avi_infoframe->data[i] = 0x00;

	avi_infoframe->data[0] |= ACTIVE_FORMAT_INFOMATION_PRESENT;
	avi_infoframe->data[1] |= ACITVE_PORTION_ASPECT_RATIO;
	avi_infoframe->data[3] = dp_get_vic(dp);

	return 0;
}

static int dp_make_audio_infoframe_data(struct dp_device *dp,
		struct infoframe *audio_infoframe,
		struct dp_audio_config_data *audio_config_data)
{
	int i;

	audio_infoframe->type_code = INFOFRAME_PACKET_TYPE_AUDIO;
	audio_infoframe->version_number = AUDIO_INFOFRAME_VERSION;
	audio_infoframe->length = AUDIO_INFOFRAME_LENGTH;

	for (i = 0; i < AUDIO_INFOFRAME_LENGTH; i++)
		audio_infoframe->data[i] = 0x00;

	/* Data Byte 1, PCM type and audio channel count */
	audio_infoframe->data[0] = ((u8)audio_config_data->audio_channel_cnt - 1);

	/* Data Byte 4, how various speaker locations are allocated */
	if (audio_config_data->audio_channel_cnt == 8)
		audio_infoframe->data[3] = 0x13;
	else if (audio_config_data->audio_channel_cnt == 6)
		audio_infoframe->data[3] = 0x0b;
	else
		audio_infoframe->data[3] = 0;

	dp_info(dp, "audio_infoframe: type and ch_cnt %02x, SF and bit size %02x, ch_allocation %02x\n",
			audio_infoframe->data[0], audio_infoframe->data[1], audio_infoframe->data[3]);

	return 0;
}
#if 0
static int dp_make_hdr_infoframe_data
	(struct infoframe *hdr_infoframe, struct exynos_hdr_static_info *hdr_info)
{
	int i;

	hdr_infoframe->type_code = INFOFRAME_PACKET_TYPE_HDR;
	hdr_infoframe->version_number = HDR_INFOFRAME_VERSION;
	hdr_infoframe->length = HDR_INFOFRAME_LENGTH;

	for (i = 0; i < HDR_INFOFRAME_LENGTH; i++)
		hdr_infoframe->data[i] = 0x00;

	hdr_infoframe->data[HDR_INFOFRAME_EOTF_BYTE_NUM] = HDR_INFOFRAME_SMPTE_ST_2084;
	hdr_infoframe->data[HDR_INFOFRAME_METADATA_ID_BYTE_NUM]
		= STATIC_MATADATA_TYPE_1;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_0_LSB]
		= hdr_info->stype1.mr.x & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_0_MSB]
		= (hdr_info->stype1.mr.x & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_0_LSB]
		= hdr_info->stype1.mr.y & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_0_MSB]
		= (hdr_info->stype1.mr.y & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_1_LSB]
		= hdr_info->stype1.mg.x & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_1_MSB]
		= (hdr_info->stype1.mg.x & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_1_LSB]
		= hdr_info->stype1.mg.y & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_1_MSB]
		= (hdr_info->stype1.mg.y & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_2_LSB]
		= hdr_info->stype1.mb.x & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_2_MSB]
		= (hdr_info->stype1.mb.x & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_2_LSB]
		= hdr_info->stype1.mb.y & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_2_MSB]
		= (hdr_info->stype1.mb.y & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_WHITE_POINT_X_LSB]
		= hdr_info->stype1.mw.x & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_WHITE_POINT_X_MSB]
		= (hdr_info->stype1.mw.x & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_WHITE_POINT_Y_LSB]
		= hdr_info->stype1.mw.y & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_WHITE_POINT_Y_MSB]
		= (hdr_info->stype1.mw.y & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_LUMI_LSB]
		= hdr_info->stype1.mmax_display_luminance & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_LUMI_MSB]
		= (hdr_info->stype1.mmax_display_luminance & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_MIN_LUMI_LSB]
		= hdr_info->stype1.mmin_display_luminance & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_MIN_LUMI_MSB]
		= (hdr_info->stype1.mmin_display_luminance & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_LIGHT_LEVEL_LSB]
		= hdr_info->stype1.mmax_content_light_level & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_LIGHT_LEVEL_MSB]
		= (hdr_info->stype1.mmax_content_light_level & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_AVERAGE_LEVEL_LSB]
		= hdr_info->stype1.mmax_frame_average_light_level & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_AVERAGE_LEVEL_MSB]
		= (hdr_info->stype1.mmax_frame_average_light_level & MSB_MASK) >> SHIFT_8BIT;

	for (i = 0; i < HDR_INFOFRAME_LENGTH; i++) {
		dp_debug(dp, "hdr_infoframe->data[%d] = 0x%02x", i,
			hdr_infoframe->data[i]);
	}

	print_hex_dump(KERN_INFO, "HDR: ", DUMP_PREFIX_NONE, 32, 1,
			hdr_infoframe->data, HDR_INFOFRAME_LENGTH, false);

	return 0;
}
#endif
static int dp_set_avi_infoframe(struct dp_device *dp)
{
	struct infoframe avi_infoframe;

	dp_make_avi_infoframe_data(dp, &avi_infoframe);
	dp_reg_set_avi_infoframe(avi_infoframe);

	return 0;
}

static int dp_make_spd_infoframe_data(struct infoframe *spd_infoframe)
{
	spd_infoframe->type_code = 0x83;
	spd_infoframe->version_number = 0x1;
	spd_infoframe->length = 25;

	strncpy(&spd_infoframe->data[0], "SEC.MCB", 8);
	strncpy(&spd_infoframe->data[8], "GALAXY", 7);
	/* spd_infoframe->data[24] = 0xA; BD player*/

	return 0;
}

static int dp_set_spd_infoframe(void)
{
	struct infoframe spd_infoframe;

	memset(&spd_infoframe, 0, sizeof(spd_infoframe));
	dp_make_spd_infoframe_data(&spd_infoframe);
	dp_reg_set_spd_infoframe(spd_infoframe);

	return 0;
}

static int dp_set_audio_infoframe(struct dp_device *dp,
		struct dp_audio_config_data *audio_config_data)
{
	struct infoframe audio_infoframe;

	dp_make_audio_infoframe_data(dp, &audio_infoframe, audio_config_data);
	dp_reg_set_audio_infoframe(audio_infoframe, audio_config_data->audio_enable);

	return 0;
}

int dp_set_hdr_infoframe(const struct drm_connector_state *conn_state)
{
	struct dp_device *dp = get_dp_drvdata();
	struct hdmi_drm_infoframe drm_hdr_infoframe;
	struct hdr_output_metadata *hdr_metadata;
	struct infoframe hdr_infoframe = {0,};
	int ret = 0;

	if (!conn_state->hdr_output_metadata) {
		ret = -EINVAL;
		goto end;
	}

	hdr_metadata = conn_state->hdr_output_metadata->data;

	if (hdr_metadata) {
		dp_debug(dp, "dp_set_hdr_infoframe 1\n");
		ret = drm_hdmi_infoframe_set_hdr_metadata(&drm_hdr_infoframe, conn_state);
		if (ret) {
			dp_debug(dp, "dp_set_hdr_infoframe Error(%d)\n", ret);
			goto end;
		}
		hdmi_drm_infoframe_pack_only(&drm_hdr_infoframe, (void*)(&hdr_infoframe), sizeof(hdr_infoframe));
		dp_reg_set_hdr_infoframe(hdr_infoframe, 1);
	} else {
		dp_debug(dp, "dp_set_hdr_infoframe 0\n");
		dp_reg_set_hdr_infoframe(hdr_infoframe, 0);
	}

end:
	return ret;
}

#if 0
static int dp_set_hdr_infoframe(struct exynos_hdr_static_info *hdr_info)
{
	struct infoframe hdr_infoframe = {0,};

	if (hdr_info->mid >= 0) {
		dp_debug(dp, "dp_set_hdr_infoframe 1\n");
		dp_make_hdr_infoframe_data(&hdr_infoframe, hdr_info);
		dp_reg_set_hdr_infoframe(hdr_infoframe, 1);
	} else {
		dp_debug(dp, "dp_set_hdr_infoframe 0\n");
		dp_reg_set_hdr_infoframe(hdr_infoframe, 0);
	}

	return 0;
}
#endif
int dp_audio_config(struct dp_audio_config_data *audio_config_data)
{
	struct dp_device *dp = get_dp_drvdata();
	int ret = 0;

	if (dp->state == DP_STATE_OFF) { // || !phy_status) {
		dp_warn(dp, "power status is off timing\n");
		return -EINVAL;
	}

	dp_info(dp, "audio config(%d ==> %d)\n",
			dp->audio_state, audio_config_data->audio_enable);

	if (audio_config_data->audio_enable == dp->audio_state)
		return 0;

	if (audio_config_data->audio_enable == AUDIO_ENABLE) {
		dp_info(dp, "audio_enable:%d, ch:%d, fs:%d, bit:%d, packed:%d, word_len:%d\n",
				audio_config_data->audio_enable, audio_config_data->audio_channel_cnt,
				audio_config_data->audio_fs, audio_config_data->audio_bit,
				audio_config_data->audio_packed_mode, audio_config_data->audio_word_length);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		{
			int bit[] = {16, 20, 24};
			int fs[] = {32000, 44100, 48000, 88200, 96000, 176400, 192000};

			secdp_bigdata_save_item(BD_AUD_CH, audio_config_data->audio_channel_cnt);
			if (audio_config_data->audio_fs >= 0 && audio_config_data->audio_fs < 7)
				secdp_bigdata_save_item(BD_AUD_FREQ, fs[audio_config_data->audio_fs]);
			if (audio_config_data->audio_bit >= 0 && audio_config_data->audio_bit < 3)
				secdp_bigdata_save_item(BD_AUD_BIT, bit[audio_config_data->audio_bit]);
		}
#endif

		dp_reg_audio_enable(&dp->cal_res, audio_config_data);
		dp_set_audio_infoframe(dp, audio_config_data);
		dp->audio_state = AUDIO_ENABLE;
	} else if (audio_config_data->audio_enable == AUDIO_DISABLE) {
		dp_reg_audio_disable();
		dp_set_audio_infoframe(dp, audio_config_data);
		dp->audio_state = AUDIO_DISABLE;
	} else if (audio_config_data->audio_enable == AUDIO_WAIT_BUF_FULL) {
		dp_reg_audio_wait_buf_full();
		dp->audio_state = AUDIO_WAIT_BUF_FULL;
	} else if (audio_config_data->audio_enable == AUDIO_DMA_REQ_HIGH) {
		dp_reg_set_dma_req_gen(1);
		dp->audio_state = AUDIO_DMA_REQ_HIGH;
	} else
		dp_info(dp, "Not support audio_enable = %d\n", audio_config_data->audio_enable);

	return ret;
}
EXPORT_SYMBOL(dp_audio_config);

void dp_audio_bist_config(struct dp_device *dp,
		struct dp_audio_config_data audio_config_data)
{
	dp_info(dp, "audio_enable = %d\n", audio_config_data.audio_enable);
	dp_info(dp, "audio_channel_cnt = %d\n", audio_config_data.audio_channel_cnt);
	dp_info(dp, "audio_fs = %d\n", audio_config_data.audio_fs);
	dp_reg_audio_bist_enable(&dp->cal_res, audio_config_data);
	dp_set_audio_infoframe(dp, &audio_config_data);
}

int dp_dpcd_read_for_hdcp22(u32 address, u32 length, u8 *data)
{
	struct dp_device *dp = get_dp_drvdata();
	int ret;

	ret = dp_reg_dpcd_read_burst(&dp->cal_res, address, length, data);

	if (ret != 0)
		dp_err(dp, "dpcd_read_for_hdcp22 fail: 0x%X\n", address);

	return ret;
}

int dp_dpcd_write_for_hdcp22(u32 address, u32 length, u8 *data)
{
	struct dp_device *dp = get_dp_drvdata();
	int ret;

	ret = dp_reg_dpcd_write_burst(&dp->cal_res, address, length, data);

	if (ret != 0)
		dp_err(dp, "dpcd_write_for_hdcp22 fail: 0x%X\n", address);

	return ret;
}

void dp_hdcp22_enable(u32 en)
{
	if (en) {
		dp_reg_set_hdcp22_system_enable(1);
		dp_reg_set_hdcp22_mode(1);
		dp_reg_set_hdcp22_encryption_enable(1);
	} else {
		dp_reg_set_hdcp22_system_enable(0);
		dp_reg_set_hdcp22_mode(0);
		dp_reg_set_hdcp22_encryption_enable(0);
	}
}
EXPORT_SYMBOL(dp_hdcp22_enable);
EXPORT_SYMBOL(dp_dpcd_write_for_hdcp22);
EXPORT_SYMBOL(dp_dpcd_read_for_hdcp22);

static void dp_hdcp13_run(struct work_struct *work)
{
	struct dp_device *dp = get_dp_drvdata();

	dp_print_bts_info(dp);
	if (dp->hdcp_ver != HDCP_VERSION_1_3 ||
			dp->hpd_current_state == EXYNOS_HPD_UNPLUG)
		return;

	dp_debug(dp, "[HDCP 1.3] run\n");
	hdcp13_run(dp);
	if (hdcp13_info.auth_state == HDCP13_STATE_FAIL) {
		queue_delayed_work(dp->dp_wq, &dp->hdcp13_work,
				msecs_to_jiffies(2000));
	}
}

static void dp_hdcp22_run(struct work_struct *work)
{
	struct dp_device *dp = get_dp_drvdata();
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
	u32 ret;
	u8 val[2] = {0, };

	mutex_lock(&dp->hdcp2_lock);
	if (IS_DP_HPD_PLUG_STATE(dp) == 0) {
		dp_info(dp, "stop hdcp2 : HPD is low\n");
		goto exit_hdcp;
	}

	dp_hdcp22_notify_state(DP_HDCP_READY);
	ret = dp_hdcp22_authenticate();
	if (ret) {
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_inc_error_cnt(ERR_HDCP_AUTH);
#endif
		goto exit_hdcp;
	}

	if (IS_DP_HPD_PLUG_STATE(dp) == 0) {
		dp_info(dp, "stop hdcp2 : HPD is low\n");
		goto exit_hdcp;
	}

	dp_dpcd_read_for_hdcp22(DPCD_HDCP22_RX_INFO, 2, val);
	dp_info(dp, "HDCP2.2 rx_info: 0:0x%X, 8:0x%X\n", val[1], val[0]);

exit_hdcp:
	mutex_unlock(&dp->hdcp2_lock);
#else
	dp_info(dp, "Not compiled EXYNOS_HDCP2 driver\n");
#endif
	dp_print_bts_info(dp);
}

static int dp_check_hdcp_version(struct dp_device *dp)
{
	int ret = 0;
	u8 val[DPCD_HDCP22_RX_CAPS_LENGTH];
	u32 rx_caps = 0;
	int i;

	hdcp13_dpcd_buffer();

	if (hdcp13_read_bcap(dp) != 0)
		dp_debug(dp, "[HDCP 1.3] NONE HDCP CAPABLE\n");
#if defined(HDCP_SUPPORT)
	else
		ret = HDCP_VERSION_1_3;
#endif
	dp_dpcd_read_for_hdcp22(DPCD_HDCP22_RX_CAPS, DPCD_HDCP22_RX_CAPS_LENGTH, val);

	for (i = 0; i < DPCD_HDCP22_RX_CAPS_LENGTH; i++)
		rx_caps |= (u32)val[i] << ((DPCD_HDCP22_RX_CAPS_LENGTH - (i + 1)) * 8);

	dp_info(dp, "HDCP2.2 rx_caps = 0x%x\n", rx_caps);

	if ((((rx_caps & VERSION) >> DPCD_HDCP_VERSION_BIT_POSITION) == (HDCP_VERSION_2_2))
			&& ((rx_caps & HDCP_CAPABLE) != 0)) {
#if defined(HDCP_2_2)
		ret = HDCP_VERSION_2_2;
#endif
		dp_debug(dp, "dp_rx supports hdcp2.2\n");
	}
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	if (ret == HDCP_VERSION_2_2)
		secdp_bigdata_save_item(BD_HDCP_VER, "hdcp2");
	else if (ret == HDCP_VERSION_1_3)
		secdp_bigdata_save_item(BD_HDCP_VER, "hdcp1");
#endif

	return ret;
}

static void hdcp_start(struct dp_device *dp)
{
	dp->hdcp_ver = dp_check_hdcp_version(dp);
	if(forced_hdcp == 13) dp->hdcp_ver = HDCP_VERSION_1_3;
	if(forced_hdcp == 22) dp->hdcp_ver = HDCP_VERSION_2_2;
	if(forced_hdcp == 99) {
		dp_info(dp, "HDCP is off\n");
		return;
	}
#if defined(HDCP_SUPPORT)
	if (dp->hdcp_ver == HDCP_VERSION_2_2)
		queue_delayed_work(dp->hdcp2_wq, &dp->hdcp22_work,
				msecs_to_jiffies(3500));
	else if (dp->hdcp_ver == HDCP_VERSION_1_3)
		queue_delayed_work(dp->dp_wq, &dp->hdcp13_work,
				msecs_to_jiffies(5500));
	else
		dp_info(dp, "HDCP is not supported\n");
#endif
}

void dp_enable(struct drm_encoder *encoder)
{
	struct dp_device *dp = encoder_to_dp(encoder);
	u8 bpc = (u8)dp->cal_res.bpc;
	u8 bist_type = (u8)dp->bist_type;
	u8 dyn_range = (u8)dp->dyn_range;

	dp_debug(dp, "%s +, state: %d\n", __func__, dp->state);
	mutex_lock(&dp->cmd_lock);

	if (dp->hpd_current_state == EXYNOS_HPD_UNPLUG) {
		dp_err(dp, "%s() hpd is low\n", __func__);
		mutex_unlock(&dp->cmd_lock);
		return;
	}

	if (dp->state == DP_STATE_ON) {
		dp_err(dp, "ignore enable, state:%d\n", dp->state);
		mutex_unlock(&dp->cmd_lock);
		return;
	}

	if (forced_resolution >= 0)
		dp->cur_video = forced_resolution;
	else if (dp->bist_used)
		dp->cur_video = dp->best_video;

#if defined(CONFIG_CPU_IDLE)
	/* block to enter SICD mode */
	exynos_update_ip_idle_status(dp->idle_ip_index, 0);
#endif
	enable_irq(dp->res.irq);

	if (dp->bist_used) {
		dp_reg_set_bist_video_config(&dp->cal_res, dp->cur_video,
				bpc, bist_type, dyn_range);
	} else {
		if (dp->cal_res.bpc == BPC_6 && dp->dfp_type != DFP_TYPE_DP) {
			bpc = BPC_8;
			dp->cal_res.bpc = BPC_8;
		}

		dp_reg_set_video_config(&dp->cal_res, dp->cur_video, bpc, dyn_range);
	}

#ifdef FEATURE_USE_DRM_EDID_PARSER
	dp_info(dp, "%s(%s): cur_video = %s@%d\n",
		__func__, dp->dex.ui_setting ? "dex":"mirror",
		dp->cur_mode.name, drm_mode_vrefresh(&dp->cur_mode));
#else
	dp_info(dp, "cur_video = %s in dp_enable!!!\n",
			supported_videos[dp->cur_video].name);
#endif

	dp_set_avi_infoframe(dp);
	dp_set_spd_infoframe();
#ifdef HDCP_SUPPORT
	dp_reg_video_mute(0);
#endif
	dp_reg_start();

	hdcp_start(dp);

	dp->state = DP_STATE_ON;

#ifdef CONFIG_SEC_DISPLAYPORT_SELFTEST
	if (self_test_on_process()) {
		self_test_resolution_update(dp->cur_mode.hdisplay,
				dp->cur_mode.vdisplay,
				drm_mode_vrefresh(&dp->cur_mode));
	}
#endif

	mutex_unlock(&dp->cmd_lock);
	dp_debug(dp, "%s -, state: %d\n", __func__, dp->state);
}

void dp_disable(struct drm_encoder *encoder)
{
	struct dp_device *dp = encoder_to_dp(encoder);

	dp_debug(dp, "%s +, state: %d\n", __func__, dp->state);
	mutex_lock(&dp->cmd_lock);

	if (dp->state != DP_STATE_ON) {
		dp_err(dp, "ignore disable, state:%d\n", dp->state);
		mutex_unlock(&dp->cmd_lock);
		return;
	}

	dp_reg_stop();
	dp_reg_set_video_bist_mode(0);

	disable_irq(dp->res.irq);

	hdcp13_info.auth_state = HDCP13_STATE_NOT_AUTHENTICATED;

	dp->state = DP_STATE_OFF;

	mutex_unlock(&dp->cmd_lock);
	dp_info(dp, "%s\n", __func__);
}

static int dp_parse_dt(struct dp_device *dp, struct device *dev)
{
	struct device_node *np = dev->of_node;

	if (IS_ERR_OR_NULL(dev->of_node)) {
		dp_err(dp, "no device tree information\n");
		return -EINVAL;
	}

	dp->dev = dev;

	if (of_property_read_u32(dev->of_node, "dp,phy_tuning", &dp->phy_tune_set)) {
		dp_info(dp, "use default phy parameters %d\n", dp->phy_tune_set);
	} else if (dp->phy_tune_set >= phy_table) {
		dp->phy_tune_set = 0;
		dp_info(dp, "invalid phy parameters %d\n", dp->phy_tune_set);
	} else {
		dp_info(dp, "use phy parameters %d\n", dp->phy_tune_set);
	}

	dp->gpio_sw_oe = of_get_named_gpio(np, "dp,aux_sw_oe", 0);
	if (gpio_is_valid(dp->gpio_sw_oe)) {
		dp_info(dp, "Successed to get gpio dp_aux_sw_oe\n");
		gpio_direction_output(dp->gpio_sw_oe, 1);
	} else {
		dp_err(dp, "Failed to get gpio dp_aux_sw_oe(%d)\n", dp->gpio_sw_oe);
		return dp->gpio_sw_oe;
	}

	dp->gpio_sw_sel = of_get_named_gpio(np, "dp,sbu_sw_sel", 0);
	if (gpio_is_valid(dp->gpio_sw_sel))
		dp_info(dp, "Successed to get gpio dp_sbu_sw_sel\n");
	else {
		dp_err(dp, "Failed to get gpio dp_sbu_sw_sel(%d)\n", dp->gpio_sw_sel);
		return dp->gpio_sw_sel;
	}

	dp->gpio_usb_dir = of_get_named_gpio(np, "dp,usb_con_sel", 0);
	if (gpio_is_valid(dp->gpio_usb_dir))
		dp_info(dp, "Successed to get gpio dp_usb_con_sel\n");
	else
		dp_err(dp, "Failed to get gpio dp_usb_con_sel[Not mandatory]\n");


	dp_info(dp, "%s done\n", __func__);

	return 0;
}

static int dp_get_clock(struct dp_device *dp)
{
	int ret = 0;

	dp->res.aclk = devm_clk_get(dp->dev, "aclk");
	if (IS_ERR_OR_NULL(dp->res.aclk)) {
		dp_info(dp, "failed to get aclk(optional)\n");
		dp->res.aclk = NULL;
	}

	dp->res.dposc_clk = devm_clk_get(dp->dev, "dposc_clk");
	if (IS_ERR_OR_NULL(dp->res.dposc_clk)) {
		dp_info(dp, "failed to get dposc_clk(optional)\n");
		dp->res.dposc_clk = NULL;
		ret = -EINVAL;
	}

	if (ret == 0)
		dp_info(dp, "Success to get dp clocks resources\n");

	return ret;
}

static int dp_init_resources(struct dp_device *dp, struct platform_device *pdev)
{
	struct resource *res;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dp_err(dp, "failed to get mem resource\n");
		return -ENOENT;
	}

	dp_info(dp, "link_regs: start(0x%x), end(0x%x)\n", (u32)res->start, (u32)res->end);

	dp->res.link_regs = devm_ioremap_resource(dp->dev, res);
	if (IS_ERR(dp->res.link_regs)) {
		dp_err(dp, "failed to remap DP LINK SFR region\n");
		return -EINVAL;
	}
	dp_regs_desc_init(dp->res.link_regs, "LINK", REGS_LINK, SST1);
#define HACK
#if defined(HACK) || !IS_ENABLED(CONFIG_PHY_EXYNOS_USBDRD)
	dp->res.phy_regs = ioremap((phys_addr_t)0x10AD0000, 0x250);
	dp_info(dp, "usbdp combo phy_regs: start(0x%x)\n", dp->res.phy_regs);
	if (!dp->res.phy_regs) {
		dp_err(dp, "failed to get USBDP combo PHY SFR region\n");
		return -EINVAL;
	}
	dp_regs_desc_init(dp->res.phy_regs, "PHY", REGS_PHY, SST1);
	dp->res.phy_tca_regs = ioremap((phys_addr_t)0x10AE0000, 0xFC);
	dp_info(dp, "usbdp combo phy_tca_regs: start(0x%x)\n", dp->res.phy_tca_regs);
	if (!dp->res.phy_tca_regs) {
		dp_err(dp, "failed to get USBDP combo PHY TCA SFR region\n");
		return -EINVAL;
	}
	dp_regs_desc_init(dp->res.phy_tca_regs, "PHY TCA", REGS_PHY_TCA, SST1);
#elif IS_ENABLED(CONFIG_PHY_EXYNOS_USBDRD) && !defined(HACK)
	//dp->res.phy_regs = phy_exynos_usbdp_get_address();
	dp->res.phy_regs = dpphy_ctrl_addr;
	dp_info(dp, "usbdp combo phy_regs: start(%p)\n", dpphy_ctrl_addr);
	dp_info(dp, "usbdp combo phy_regs: start(0x%x)\n", dp->res.phy_regs);
	if (!dp->res.phy_regs) {
		dp_err(dp, "failed to get USBDP combo PHY SFR region\n");
		return -EINVAL;
	}
	dp_regs_desc_init(dp->res.phy_regs, "PHY", REGS_PHY, SST1);
	dp->res.phy_tca_regs = dpphy_tca_addr;
	dp_info(dp, "usbdp combo phy_tca_regs: start(0x%x)\n", dp->res.phy_tca_regs);
	if (!dp->res.phy_tca_regs) {
		dp_err(dp, "failed to get USBDP combo PHY TCA SFR region\n");
		return -EINVAL;
	}
	dp_regs_desc_init(dp->res.phy_tca_regs, "PHY_TCA", REGS_PHY_TCA, SST1);
#endif

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dp_err(dp, "failed to get irq resource\n");
		return -ENOENT;
	}

	dp->res.irq = res->start;
	ret = devm_request_irq(dp->dev, res->start,
			dp_irq_handler, 0, pdev->name, dp);
	if (ret) {
		dp_err(dp, "failed to install DP irq\n");
		return -EINVAL;
	}
	disable_irq(dp->res.irq);

	ret = dp_get_clock(dp);
	if (ret) {
		dp_err(dp, "failed to get DP clks\n");
		/*return -EINVAL;*/
		dp->res.dposc_clk = NULL;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int dp_aux_onoff(struct dp_device *dp,
		int onoff)
{
	int rc = 0;
	if (!gpio_is_valid(dp->gpio_sw_oe)) {
		dp_err(dp, "gpio_sw_oe is not ready\n");
		return -ENODEV;
	}
	dp_info(dp, "aux vdd onoff = %d\n", onoff);

	if (onoff == 1)
		gpio_direction_output(dp->gpio_sw_oe, 0);
	else
		gpio_direction_output(dp->gpio_sw_oe, 1);

	return rc;
}

static void dp_aux_sel(struct dp_device *dp)
{
	if (gpio_is_valid(dp->gpio_usb_dir) &&
			gpio_is_valid(dp->gpio_sw_sel)) {
		dp->cal_res.dp_sw_sel = !gpio_get_value(dp->gpio_usb_dir);
		gpio_direction_output(dp->gpio_sw_sel, !(dp->cal_res.dp_sw_sel));
		dp_info(dp, "Get direction from PDIC %d\n", dp->cal_res.dp_sw_sel);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_save_item(BD_ORIENTATION,	dp->cal_res.dp_sw_sel ? "CC1" : "CC2");
#endif
	} else if (gpio_is_valid(dp->gpio_usb_dir)) {
		/* for old H/W - AUX switch is controlled by PDIC */
		dp->cal_res.dp_sw_sel = gpio_get_value(dp->gpio_usb_dir);
		dp_info(dp, "Get Direction From PDIC %d\n", !dp->cal_res.dp_sw_sel);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_save_item(BD_ORIENTATION,	dp->cal_res.dp_sw_sel ? "CC1" : "CC2");
#endif
	}
}

static int dp_usb_typec_notification_proceed(struct dp_device *dp,
				PD_NOTI_TYPEDEF *usb_typec_info)
{
	dp_debug(dp, "%s: dump(0x%01x, 0x%01x, 0x%02x, 0x%04x, 0x%04x, 0x%04x)\n",
			__func__, usb_typec_info->src, usb_typec_info->dest, usb_typec_info->id,
			usb_typec_info->sub1, usb_typec_info->sub2, usb_typec_info->sub3);

	switch (usb_typec_info->id) {
	case PDIC_NOTIFY_ID_DP_CONNECT:
		switch (usb_typec_info->sub1) {
		case PDIC_NOTIFY_DETACH:
#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER
			dp_logger_set_max_count(100);
#endif
			dp_info(dp, "PDIC_NOTIFY_ID_DP_CONNECT, %x\n", usb_typec_info->sub1);
			dp->cal_res.pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_UNKNOWN;
			dp->pdic_link_conf = false;
			dp->pdic_hpd = false;
			dp_hdcp22_notify_state(DP_DISCONNECT);
			dp_hpd_changed(dp, EXYNOS_HPD_UNPLUG);
			dp_aux_onoff(dp, 0);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_disconnection();
#endif
			break;
		case PDIC_NOTIFY_ATTACH:
			dp_info(dp, "PDIC_NOTIFY_ID_DP_CONNECT, %x\n", usb_typec_info->sub1);
			dp_aux_onoff(dp, 1);
			dp->sink_info.ven_id = usb_typec_info->sub2;
			dp->sink_info.prod_id = usb_typec_info->sub3;
			dp_info(dp, "VID:0x%llX, PID:0x%llX\n", dp->sink_info.ven_id, dp->sink_info.prod_id);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_connection();
			secdp_bigdata_save_item(BD_ADT_VID, dp->sink_info.ven_id);
			secdp_bigdata_save_item(BD_ADT_PID, dp->sink_info.prod_id);
#endif
			break;
		default:
			break;
		}

		break;

	case PDIC_NOTIFY_ID_DP_LINK_CONF:
		dp_info(dp, "PDIC_NOTIFY_ID_DP_LINK_CONF %x\n",
				usb_typec_info->sub1);
		dp_aux_sel(dp);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_save_item(BD_LINK_CONFIGURE, usb_typec_info->sub1 + 'A' - 1);
#endif
		switch (usb_typec_info->sub1) {
		case PDIC_NOTIFY_DP_PIN_UNKNOWN:
			dp->cal_res.pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_UNKNOWN;
			break;
		case PDIC_NOTIFY_DP_PIN_A:
			dp->cal_res.pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_A;
			break;
		case PDIC_NOTIFY_DP_PIN_B:
			dp->cal_res.dp_sw_sel = !dp->cal_res.dp_sw_sel;
			dp->cal_res.pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_B;
			break;
		case PDIC_NOTIFY_DP_PIN_C:
			dp->cal_res.pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_C;
			break;
		case PDIC_NOTIFY_DP_PIN_D:
			dp->cal_res.pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_D;
			break;
		case PDIC_NOTIFY_DP_PIN_E:
			dp->cal_res.pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_E;
			break;
		case PDIC_NOTIFY_DP_PIN_F:
			dp->cal_res.pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_F;
			break;
		default:
			dp->cal_res.pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_UNKNOWN;
			break;
		}

		if (dp->cal_res.pdic_notify_dp_conf) {
			dp->pdic_link_conf = true;
			if (dp->pdic_hpd)
				dp_hpd_changed(dp, EXYNOS_HPD_PLUG);
		}
		break;

	case PDIC_NOTIFY_ID_DP_HPD:
		dp_info(dp, "PDIC_NOTIFY_ID_DP_HPD, %x, %x\n",
				usb_typec_info->sub1, usb_typec_info->sub2);
		switch (usb_typec_info->sub1) {
		case PDIC_NOTIFY_IRQ:
			break;
		case PDIC_NOTIFY_LOW:
			dp->pdic_hpd = false;
			dp_hdcp22_notify_state(DP_DISCONNECT);
			dp_hpd_changed(dp, EXYNOS_HPD_UNPLUG);
			break;
		case PDIC_NOTIFY_HIGH:
			if ((dp->hpd_current_state != EXYNOS_HPD_UNPLUG) &&
					usb_typec_info->sub2 == PDIC_NOTIFY_IRQ) {
				queue_delayed_work(dp->dp_wq, &dp->hpd_irq_work, 0);
				return 0;
			} else {
				dp->pdic_hpd = true;
				if (dp->pdic_link_conf)
					dp_hpd_changed(dp, EXYNOS_HPD_PLUG);
			}
			break;
		default:
			break;
		}

		break;

	default:
		break;
	}

	return 0;
}

#ifdef CONFIG_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
#define DP_HAL_INIT_TIME	30/*sec*/
static void dp_hal_ready_wait(struct dp_device *dp)
{
	time64_t wait_time = ktime_get_boottime_seconds();

	dp_info(dp, "current time is %lld\n", wait_time);

	if (wait_time < DP_HAL_INIT_TIME) {
		wait_time = DP_HAL_INIT_TIME - wait_time;
		dp_info(dp, "wait for %lld\n", wait_time);

		wait_event_interruptible_timeout(dp->dp_ready_wait,
		    (dp->dp_ready_wait_state == DP_READY_YES), msecs_to_jiffies(wait_time * 1000));
	}
	dp->dp_ready_wait_state = DP_READY_YES;
}

static void dp_pdic_event_proceed_work(struct work_struct *work)
{
	struct dp_device *dp = get_dp_drvdata();
	struct pdic_event *data;

	if (dp->dp_ready_wait_state != DP_READY_YES)
		dp_hal_ready_wait(dp);

	while (!list_empty(&dp->list_pd)) {
		PD_NOTI_TYPEDEF pdic_evt;

		mutex_lock(&dp->pdic_lock);
		data = list_first_entry(&dp->list_pd, typeof(*data), list);
		memcpy(&pdic_evt, &data->event, sizeof(pdic_evt));
		list_del(&data->list);
		kfree(data);
		mutex_unlock(&dp->pdic_lock);
		dp_usb_typec_notification_proceed(dp, &pdic_evt);
		dp_info(dp, "done\n");
	}
}

static void dp_pdic_queue_flush(struct dp_device *dp)
{
	struct pdic_event *data, *next;

	if (list_empty(&dp->list_pd))
		return;

	dp_info(dp, "delete hpd event from pdic queue\n");

	mutex_lock(&dp->pdic_lock);
	list_for_each_entry_safe(data, next, &dp->list_pd, list) {
		if (data->event.id == PDIC_NOTIFY_ID_DP_HPD) {
			list_del(&data->list);
			kfree(data);
		}
	}
	mutex_unlock(&dp->pdic_lock);
}
#endif

static int usb_typec_dp_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct dp_device *dp = get_dp_drvdata();
	PD_NOTI_TYPEDEF usb_typec_info = *(PD_NOTI_TYPEDEF *)data;
#ifdef CONFIG_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
	struct pdic_event *pd_data;

	if (usb_typec_info.dest != PDIC_NOTIFY_DEV_DP)
		return 0;

	dp_debug(dp, "PDIC action(%ld) dump(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n",
		action, usb_typec_info.src, usb_typec_info.dest, usb_typec_info.id,
		usb_typec_info.sub1, usb_typec_info.sub2, usb_typec_info.sub3);

	if (usb_typec_info.id == PDIC_NOTIFY_ID_DP_CONNECT)
		dp->pdic_cable_state = usb_typec_info.sub1;

	pd_data = kzalloc(sizeof(struct pdic_event), GFP_KERNEL);
	if (!pd_data) {
		dp_err(dp, "kzalloc error for pdic event\n");
		return 0;
	}

	switch (usb_typec_info.id) {
	case PDIC_NOTIFY_ID_DP_CONNECT:
		dp_info(dp, "queued CONNECT: %x\n", usb_typec_info.sub1);
		break;
	case PDIC_NOTIFY_ID_DP_LINK_CONF:
		dp_info(dp, "queued LINK_CONF: %x\n", usb_typec_info.sub1);
		break;
	case PDIC_NOTIFY_ID_DP_HPD:
		dp_info(dp, "queued HPD: %x, %x\n", usb_typec_info.sub1, usb_typec_info.sub2);
		break;
	}

	/* if disconnect or hpd low event come, then flush queue */
	if (((usb_typec_info.id == PDIC_NOTIFY_ID_DP_CONNECT &&
			usb_typec_info.sub1 == PDIC_NOTIFY_DETACH) ||
			(usb_typec_info.id == PDIC_NOTIFY_ID_DP_HPD &&
			usb_typec_info.sub1 == PDIC_NOTIFY_LOW))) {
		dp_pdic_queue_flush(dp);
	};

	memcpy(&pd_data->event, &usb_typec_info, sizeof(usb_typec_info));

	mutex_lock(&dp->pdic_lock);
	list_add_tail(&pd_data->list, &dp->list_pd);
	mutex_unlock(&dp->pdic_lock);

	queue_delayed_work(dp->dp_wq, &dp->pdic_event_proceed_work, 0);
#else
	if (usb_typec_info.dest != PDIC_NOTIFY_DEV_DP)
		return 0;

	dp_debug(dp, "pdic action(%ld) dump(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n",
			action, usb_typec_info.src, usb_typec_info.dest, usb_typec_info.id,
			usb_typec_info.sub1, usb_typec_info.sub2, usb_typec_info.sub3);

	dp_usb_typec_notification_proceed(dp, &usb_typec_info);
#endif

	return 0;
}

static int dp_check_gpios(struct dp_device *dp)
{
	struct device_node *np = dp->dev->of_node;
	int ret = 0;

	if (!gpio_is_valid(dp->gpio_sw_oe)) {
		dp->gpio_sw_oe = of_get_named_gpio(np, "dp,aux_sw_oe", 0);
		if (gpio_is_valid(dp->gpio_sw_oe)) {
			dp_info(dp, "dp_aux_sw_oe ok\n");
			gpio_direction_output(dp->gpio_sw_oe, 1);
		} else {
			dp_err(dp, "dp_aux_sw_oe(%d) fail\n", dp->gpio_sw_oe);
			ret = -ENODEV;
		}
	}

	if (!gpio_is_valid(dp->gpio_sw_sel)) {
		dp->gpio_sw_sel = of_get_named_gpio(np, "dp,sbu_sw_sel", 0);
		if (gpio_is_valid(dp->gpio_sw_sel)) {
			dp_info(dp, "dp_sbu_sw_sel ok\n");
		} else {
			dp_err(dp, "dp_sbu_sw_sel(%d) fail\n", dp->gpio_sw_sel);
			ret = -ENODEV;
		}
	}

	if (!gpio_is_valid(dp->gpio_usb_dir)) {
		dp->gpio_usb_dir = of_get_named_gpio(np, "dp,usb_con_sel", 0);
		if (gpio_is_valid(dp->gpio_usb_dir)) {
			dp_info(dp, "dp_usb_con_sel ok\n");
		} else {
			dp_err(dp, "dp_usb_con_sel fail\n");
			ret = -ENODEV;
		}
	}

	return ret;
}

static void dp_notifier_register_work(struct work_struct *work)
{
	struct dp_device *dp = get_dp_drvdata();

	if (!dp->notifier_registered) {
		dp->notifier_registered = 1;
		dp_check_gpios(dp);
		dp_info(dp, "notifier registered\n");
		manager_notifier_register(&dp->dp_typec_nb,
			usb_typec_dp_notification, MANAGER_NOTIFY_PDIC_DP);
	}
}
#elif IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
static int dp_aux_onoff(struct dp_device *dp, int onoff)
{
	int rc = 0;

	dp_info(dp, "aux vdd onoff = %d\n", onoff);

	if (onoff == 1)
		gpio_direction_output(dp->gpio_sw_oe, 0);
	else
		gpio_direction_output(dp->gpio_sw_oe, 1);

	return rc;
}
#if IS_ENABLED(CONFIG_CHECK_CTYPE_SIDE)
extern int usbpd_manager_get_side_check(void);
#endif

static void dp_aux_sel(struct dp_device *dp)
{
	int value = 0;

	value = usbpd_manager_get_side_check();

	if (gpio_is_valid(dp->gpio_sw_sel)) {
		dp->cal_res.dp_sw_sel = (value == 1 ? 1 : 0);
		gpio_direction_output(dp->gpio_sw_sel, !dp->cal_res.dp_sw_sel);
		dp_info(dp, "Get direction from PDIC %d\n", dp->cal_res.dp_sw_sel);
	} else {
		/* for old H/W - AUX switch is controlled by PDIC */
		dp->cal_res.dp_sw_sel = value;
		dp_info(dp, "Get Direction From PDIC %d\n", !dp->cal_res.dp_sw_sel);
	}
	dp_info(dp, "usb direction %d\n", usbpd_manager_get_side_check());
	dp_info(dp, "dp sw_sel     %d\n", dp->cal_res.dp_sw_sel);
	dp_info(dp, "gpio sw_sel   %d\n", gpio_get_value(dp->gpio_sw_sel));
	dp_info(dp, "dp sw_oe      %d\n", dp->gpio_sw_oe);
}

static int usb_typec_dp_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct dp_device *dp = container_of(nb,
			struct dp_device, dp_typec_nb);
	struct ifconn_notifier_template *ifconn_info = (struct ifconn_notifier_template *)data;

	if (ifconn_info->dest != IFCONN_NOTIFY_DP)
		return 0;

	dp_info(dp, "%s (action:%ld, src:0x%01x, dest:0x%01x, id:0x%02x, rid:0x%02x)\n",
			__func__, action,
			ifconn_info->src, ifconn_info->dest, ifconn_info->id, ifconn_info->rid);
	dp_info(dp, "%s (attach:0x%04x, rprd:0x%04x, cable_type:0x%04x, event:0x%04x)\n",
			__func__, ifconn_info->attach, ifconn_info->rprd,
			ifconn_info->cable_type, ifconn_info->event);
	dp_info(dp, "%s (sub1:0x%04x, sub2:0x%04x, sub3:0x%04x)\n",
			__func__, ifconn_info->sub1, ifconn_info->sub2, ifconn_info->sub3);

	switch (ifconn_info->id) {
	case IFCONN_NOTIFY_ID_DP_CONNECT:
		switch (ifconn_info->sub1) {
		case IFCONN_NOTIFY_DETACH:
			dp_info(dp, "IFCONN_NOTIFY_ID_DP_CONNECT, %x\n", ifconn_info->sub1);
			dp->cal_res.ifconn_notify_dp_conf = IFCONN_NOTIFY_DP_PIN_UNKNOWN;
			dp->ifconn_link_conf = false;
			dp->ifconn_hpd = false;
			dp_hdcp22_notify_state(DP_DISCONNECT);
			dp_hpd_changed(dp, EXYNOS_HPD_UNPLUG);
			dp_aux_onoff(dp, 0);
			break;
		case IFCONN_NOTIFY_ATTACH:
			dp_info(dp, "IFCONN_NOTIFY_ID_DP_CONNECT, %x\n", ifconn_info->sub1);
			dp_aux_onoff(dp, 1);
			break;
		default:
			break;
		}

		break;

	case IFCONN_NOTIFY_ID_DP_LINK_CONF:
		dp_info(dp, "IFCONN_NOTIFY_ID_DP_LINK_CONF %x\n",
				ifconn_info->sub1);
		dp_aux_sel(dp);
		switch (ifconn_info->sub1) {
		case IFCONN_NOTIFY_DP_PIN_UNKNOWN:
			dp->cal_res.ifconn_notify_dp_conf = IFCONN_NOTIFY_DP_PIN_UNKNOWN;
			break;
		case IFCONN_NOTIFY_DP_PIN_A:
			dp->cal_res.ifconn_notify_dp_conf = IFCONN_NOTIFY_DP_PIN_A;
			break;
		case IFCONN_NOTIFY_DP_PIN_B:
			dp->cal_res.dp_sw_sel = !dp->cal_res.dp_sw_sel;
			dp->cal_res.ifconn_notify_dp_conf = IFCONN_NOTIFY_DP_PIN_B;
			break;
		case IFCONN_NOTIFY_DP_PIN_C:
			dp->cal_res.ifconn_notify_dp_conf = IFCONN_NOTIFY_DP_PIN_C;
			break;
		case IFCONN_NOTIFY_DP_PIN_D:
			dp->cal_res.ifconn_notify_dp_conf = IFCONN_NOTIFY_DP_PIN_D;
			break;
		case IFCONN_NOTIFY_DP_PIN_E:
			dp->cal_res.ifconn_notify_dp_conf = IFCONN_NOTIFY_DP_PIN_E;
			break;
		case IFCONN_NOTIFY_DP_PIN_F:
			dp->cal_res.ifconn_notify_dp_conf = IFCONN_NOTIFY_DP_PIN_F;
			break;
		default:
			dp->cal_res.ifconn_notify_dp_conf = IFCONN_NOTIFY_DP_PIN_UNKNOWN;
			break;
		}

		if (dp->cal_res.ifconn_notify_dp_conf) {
			dp->ifconn_link_conf = true;
			if (dp->ifconn_hpd)
				dp_hpd_changed(dp, EXYNOS_HPD_PLUG);
		}
		break;

	case IFCONN_NOTIFY_ID_DP_HPD:
		dp_info(dp, "IFCONN_NOTIFY_ID_DP_HPD, %x, %x\n",
				ifconn_info->sub1, ifconn_info->sub2);
		switch (ifconn_info->sub1) {
		case IFCONN_NOTIFY_IRQ:
			break;
		case IFCONN_NOTIFY_LOW:
			dp->ifconn_hpd = false;
			dp_hdcp22_notify_state(DP_DISCONNECT);
			dp_hpd_changed(dp, EXYNOS_HPD_UNPLUG);
			break;
		case IFCONN_NOTIFY_HIGH:
			if ((dp->hpd_current_state == EXYNOS_HPD_PLUG ||
				dp->hpd_current_state == EXYNOS_HPD_IRQ) &&
					ifconn_info->sub2 == IFCONN_NOTIFY_IRQ) {
				queue_delayed_work(dp->dp_wq, &dp->hpd_irq_work, 0);
				return 0;
			} else {
				dp->ifconn_hpd = true;
				if (dp->ifconn_link_conf) {
					dp_hpd_changed(dp, EXYNOS_HPD_PLUG);
				}
			}
			break;
		default:
			break;
		}

		break;

	default:
		break;
	}

	return 0;
}

static void dp_notifier_register_work(struct work_struct *work)
{
	struct dp_device *dp = get_dp_drvdata();

	if (!dp->notifier_registered) {
		dp->notifier_registered = 1;
		dp_info(dp, "notifier registered\n");
		ifconn_notifier_register(&dp->dp_typec_nb,
			usb_typec_dp_notification,
			IFCONN_NOTIFY_DP,
			IFCONN_NOTIFY_MANAGER);
	}
}
#endif

#ifdef DP_TEST
static ssize_t dp_link_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", __func__);
}

static ssize_t dp_link_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	int mode = 0;
	struct dp_device *dp = get_dp_drvdata();
	struct drm_connector *connector = &dp->connector;
	struct drm_device *dev = connector->dev;

	if (kstrtoint(buf, 10, &mode))
		return size;
	dp_info(dp, "%s mode=%d\n", __func__, mode);

	if (mode == 0) {
		dp_hpd_changed(dp, EXYNOS_HPD_UNPLUG);
		dp_link_sink_status_read(dp);
	} else if (mode == 8) {
		queue_delayed_work(dp->dp_wq, &dp->hpd_irq_work, 0);
	} else if (mode == 9) {
		dp_hpd_changed(dp, EXYNOS_HPD_PLUG);
	} else {
		u8 link_rate = mode/10;
		u8 lane_cnt = mode%10;

		if ((link_rate >= 1 && link_rate <= 3) &&
				(lane_cnt == 1 || lane_cnt == 2 || lane_cnt == 4)) {
			if (link_rate == 4)
				link_rate = LINK_RATE_8_1Gbps;
			else if (link_rate == 3)
				link_rate = LINK_RATE_5_4Gbps;
			else if (link_rate == 2)
				link_rate = LINK_RATE_2_7Gbps;
			else
				link_rate = LINK_RATE_1_62Gbps;

			dp_info(dp, "%s: %02x %02x\n", __func__, link_rate, lane_cnt);
			dp_reg_init(&dp->cal_res); /* for AUX ch read/write. */

			dp_link_status_read(dp);

			g_dp_debug_param.param_used = 1;
			g_dp_debug_param.link_rate = link_rate;
			g_dp_debug_param.lane_cnt = lane_cnt;

			dp_full_link_training(dp);

			g_dp_debug_param.param_used = 0;

			dp->connector.status = connector_status_connected;
			drm_kms_helper_hotplug_event(dev);
			dp_info(dp, "drm_kms_helper_hotplug_event(connected)\n");
			dp_wait_state_change(dp, 1000, DP_STATE_ON);
		} else {
			dp_err(dp, "%s: Not support command[%d]\n",
					__func__, mode);
		}
	}

	return size;
}

static CLASS_ATTR(link, 0664, dp_link_show, dp_link_store);

static ssize_t dp_test_bpc_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct dp_device *dp = get_dp_drvdata();

	return snprintf(buf, PAGE_SIZE, "dp bpc %d\n",
			(dp->cal_res.bpc == BPC_6)?6:8);
}
static ssize_t dp_test_bpc_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int mode = 0;
	struct dp_device *dp = get_dp_drvdata();

	if (kstrtoint(buf, 10, &mode))
		return size;
	dp_info(dp, "%s mode=%d\n", __func__, mode);

	switch (mode) {
	case 6:
		dp->cal_res.bpc = BPC_6;
		break;
	case 8:
		dp->cal_res.bpc = BPC_8;
		break;
	default:
		dp_err(dp, "%s: Not support command[%d]\n",
				__func__, mode);
		break;
	}

	return size;
}

static CLASS_ATTR(bpc, 0664, dp_test_bpc_show, dp_test_bpc_store);

static ssize_t dp_test_range_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct dp_device *dp = get_dp_drvdata();

	return sprintf(buf, "dp range %s\n",
		(dp->dyn_range == VESA_RANGE)?"VESA_RANGE":"CEA_RANGE");
}
static ssize_t dp_test_range_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int mode = 0;
	struct dp_device *dp = get_dp_drvdata();

	if (kstrtoint(buf, 10, &mode))
		return size;
	dp_info(dp, "%s mode=%d\n", __func__, mode);

	switch (mode) {
	case 0:
		dp->dyn_range = VESA_RANGE;
		break;
	case 1:
		dp->dyn_range = CEA_RANGE;
		break;
	default:
		dp_err(dp, "%s: Not support command[%d]\n",
				__func__, mode);
		break;
	}

	return size;
}

static CLASS_ATTR(range, 0664, dp_test_range_show, dp_test_range_store);

static ssize_t dp_test_bist_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct dp_device *dp = get_dp_drvdata();
	u32 sst_id = 0;

	return snprintf(buf, PAGE_SIZE, "dp bist used %d type %d\n",
			dp->bist_used,
			dp->bist_type);
}

static ssize_t dp_test_bist_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int mode = 0;
	struct dp_audio_config_data audio_config_data;
	struct dp_device *dp = get_dp_drvdata();
	u32 sst_id = 0;

	if (kstrtoint(buf, 10, &mode))
		return size;
	dp_info(dp, "%s mode=%d\n", __func__, mode);

	switch (mode) {
	case 0:
		dp->bist_used = 0;
		dp->bist_type = COLOR_BAR;
		break;
	case 1:
		dp->bist_used = 1;
		dp->bist_type = COLOR_BAR;
		break;
	case 2:
		dp->bist_used = 1;
		dp->bist_type = WGB_BAR;
		break;
	case 3:
		dp->bist_used = 1;
		dp->bist_type = MW_BAR;
		break;
	case 4:
		dp->bist_used = 1;
		dp->bist_type = CTS_COLOR_RAMP;
		break;
	case 5:
		dp->bist_used = 1;
		dp->bist_type = CTS_BLACK_WHITE;
		break;
	case 6:
		dp->bist_used = 1;
		dp->bist_type = CTS_COLOR_SQUARE_VESA;
		break;
	case 7:
		dp->bist_used = 1;
		dp->bist_type = CTS_COLOR_SQUARE_CEA;
		break;
	case 11:
	case 12:
		audio_config_data.audio_enable = 1;
		audio_config_data.audio_fs = FS_192KHZ;
		audio_config_data.audio_channel_cnt = mode-10;
		audio_config_data.audio_bit = 0;
		audio_config_data.audio_packed_mode = 0;
		audio_config_data.audio_word_length = 0;
		dp_audio_bist_config(dp, audio_config_data);
		break;
	default:
		dp_err(dp, "%s: Not support command[%d]\n",
				__func__, mode);
		break;
	}

	return size;
}
static CLASS_ATTR(bist, 0664, dp_test_bist_show, dp_test_bist_store);

static ssize_t dp_forced_resolution_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;
	int i;

	for (i = 0; i < supported_videos_pre_cnt; i++) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%c %2d : %s\n",
				forced_resolution == i ? '*':' ', i,
				supported_videos[i].name);
	}

	return ret;
}

static ssize_t dp_forced_resolution_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	int val[4] = {0,};

	get_options(buf, 4, val);

	reduced_resolution = 0;

	if (val[1] < 0 || val[1] >= supported_videos_pre_cnt || val[0] < 1)
		forced_resolution = -1;
	else {
		struct dp_device *dp = get_dp_drvdata();
		int hpd_stat = dp->hpd_current_state;

		forced_resolution = val[1];
		if (hpd_stat) {
			dp_hpd_changed(dp, EXYNOS_HPD_UNPLUG);
			msleep(100);
			dp_hpd_changed(dp, EXYNOS_HPD_PLUG);
		}
	}

	return size;
}
static CLASS_ATTR(forced_resolution, 0664, dp_forced_resolution_show,
		dp_forced_resolution_store);

static ssize_t dp_reduced_resolution_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "%llu\n", reduced_resolution);

	return ret;
}

static ssize_t dp_reduced_resolution_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	int val[4] = {0,};

	get_options(buf, 4, val);

	forced_resolution = -1;

	if (val[1] < 0 || val[1] >= supported_videos_pre_cnt || val[0] < 1)
		reduced_resolution = 0;
	else {
		switch (val[1]) {
		case 1:
			reduced_resolution = PIXELCLK_2160P30HZ;
			break;
		case 2:
			reduced_resolution = PIXELCLK_1080P60HZ;
			break;
		case 3:
			reduced_resolution = PIXELCLK_1080P30HZ;
			break;
		default:
			reduced_resolution = 0;
		};
	}

	return size;
}
static CLASS_ATTR(reduced_resolution, 0664, dp_reduced_resolution_show,
		dp_reduced_resolution_store);

static ssize_t dp_log_level_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "dp log level %1d\n", dp_log_level);
}
static ssize_t dp_log_level_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int mode = 0;

	if (kstrtoint(buf, 10, &mode))
		return size;
	dp_log_level = mode;
	dp_err(dp, "log level = %d\n", dp_log_level);

	return size;
}

static CLASS_ATTR(log_level, 0664, dp_log_level_show, dp_log_level_store);
#endif

#ifdef FEATURE_MANAGE_HMD_LIST
/*
 * assume that 1 HMD device has name(14),vid(4),pid(4) each, then
 * max 32 HMD devices(name,vid,pid) need 806 bytes including TAG, NUM, comba
 */
#define MAX_DEX_STORE_LEN	1024
static int dp_update_hmd_list(struct dp_device *dp, const char *buf, size_t size)
{
	int ret = 0;
	char str[MAX_DEX_STORE_LEN] = {0,};
	char *p, *tok;
	u32 num_hmd = 0;
	int j = 0;
	u32 val;

	mutex_lock(&dp->hmd_lock);

	memcpy(str, buf, size);
	p = str;

	tok = strsep(&p, ",");
	if (strncmp(DEX_TAG_HMD, tok, strlen(DEX_TAG_HMD))) {
		dp_debug(dp, "not HMD tag %s\n", tok);
		ret = -EINVAL;
		goto not_tag_exit;
	}

	dp_info(dp, "%s\n", __func__);

	tok = strsep(&p, ",");
	if (tok == NULL || *tok == 0xa/*LF*/) {
		ret = -EPERM;
		goto exit;
	}
	ret = kstrtouint(tok, 10, &num_hmd);
	if (ret || num_hmd > MAX_NUM_HMD) {
		dp_err(dp, "invalid list num %d\n", num_hmd);
		num_hmd = 0;
		ret = -EPERM;
		goto exit;
	}

	for (j = 0; j < num_hmd; j++) {
		/* monitor name */
		tok = strsep(&p, ",");
		if (tok == NULL || *tok == 0xa/*LF*/)
			break;
		strlcpy(dp->hmd_list[j].monitor_name, tok, MON_NAME_LEN);

		/* VID */
		tok  = strsep(&p, ",");
		if (tok == NULL || *tok == 0xa/*LF*/)
			break;
		if (kstrtouint(tok, 16, &val)) {
			ret = -EPERM;
			break;
		}
		dp->hmd_list[j].ven_id = val;

		/* PID */
		tok  = strsep(&p, ",");
		if (tok == NULL || *tok == 0xa/*LF*/)
			break;
		if (kstrtouint(tok, 16, &val)) {
			ret = -EPERM;
			break;
		}
		dp->hmd_list[j].prod_id = val;

		dp_info(dp, "HMD%02d: %s, 0x%04x, 0x%04x\n", j,
				dp->hmd_list[j].monitor_name,
				dp->hmd_list[j].ven_id,
				dp->hmd_list[j].prod_id);
	}

exit:
	/* clear rest */
	for (; j < MAX_NUM_HMD; j++) {
		dp->hmd_list[j].monitor_name[0] = '\0';
		dp->hmd_list[j].ven_id = 0;
		dp->hmd_list[j].prod_id = 0;
	}

not_tag_exit:
	mutex_unlock(&dp->hmd_lock);

	return ret;
}
#endif

#ifdef FEATURE_DEX_ADAPTER_TWEAK
#define DEX_ADATER_TWEAK_LEN	32
#define DEX_TAG_ADAPTER_TWEAK "SkipAdapterCheck"
static int dp_dex_adapter_tweak(struct dp_device *dp, const char *buf, size_t size)
{
	char str[DEX_ADATER_TWEAK_LEN] = {0,};
	char *p, *tok;

	if (size >= DEX_ADATER_TWEAK_LEN)
		return -EINVAL;

	memcpy(str, buf, size);
	p = str;

	tok = strsep(&p, ",");
	if (strncmp(DEX_TAG_ADAPTER_TWEAK, tok, strlen(DEX_TAG_ADAPTER_TWEAK)))
		return -EINVAL;

	tok = strsep(&p, ",");
	if (tok == NULL || *tok == 0xa/*LF*/) {
		dp_info(dp, "Dex adapter tweak - Invalid value\n");
		return 0;
	}

	switch (*tok) {
	case '0':
		dp->dex.skip_adapter_check = false;
		break;
	case '1':
		dp->dex.skip_adapter_check = true;
		break;
	}
	dp_info(dp, "%s(%c)\n", __func__, *tok);

	return 0;
}
#endif

#ifdef FEATURE_DEX_SUPPORT
static ssize_t dex_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct dp_device *dp = get_dp_drvdata();
	int ret = 0;
	enum dex_state cur_state = dp->dex.cur_state;

	dp_info(dp, "dex state:%d, setting: %d, pdic state:%d\n",
			cur_state, dp->dex.ui_setting, dp->pdic_hpd);

	if (!dp->pdic_hpd)
		cur_state = DEX_OFF;

	ret = scnprintf(buf, 8, "%d\n", cur_state);

	if (dp->dex.cur_state == DEX_RECONNECTING)
		dp->dex.cur_state = dp->dex.ui_setting;

	return ret;
}

static ssize_t dex_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	struct dp_device *dp = get_dp_drvdata();
	int val = 0;
	u32 dex_run = 0;
	u32 dex_setting = 0;
	bool need_reconnect = false;
#ifdef FEATURE_MANAGE_HMD_LIST
	int ret;

	if (size >= MAX_DEX_STORE_LEN) {
		dp_err(dp, "invalid input size %lu\n", size);
		return -EINVAL;
	}

	ret = dp_update_hmd_list(dp, buf, size);
	if (ret == 0) /* HMD list update success */
		return size;
	else if (ret != -EINVAL) /* tried to update HMD list but error*/
		return ret;
#endif

#ifdef FEATURE_DEX_ADAPTER_TWEAK
	if (!dp_dex_adapter_tweak(dp, buf, size))
		return size;
#endif
	if (kstrtouint(buf, 10, &val)) {
		dp_err(dp, "invalid input %s\n", buf);
		return -EINVAL;
	}

	if (val != 0x00 && val != 0x01 && val != 0x10 && val != 0x11) {
		dp_err(dp, "invalid input 0x%X\n", val);
		return -EINVAL;
	}

	dex_setting = (val & 0xF0) >> 4;
	dex_run = (val & 0x0F);

	dp_info(dp, "dex state:%d, setting:%d->%d, run:%d, hpd:%d\n",
			dp->dex.cur_state, dp->dex.ui_setting, dex_setting,
			dex_run, dp->hpd_current_state);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (dp->dp_ready_wait_state != DP_READY_YES) {
		dp_info(dp, "dp_ready_wait wakeup\n");
		dp->dp_ready_wait_state = DP_READY_YES;
		wake_up_interruptible(&dp->dp_ready_wait);
	}
#endif
#ifdef FEATURE_MANAGE_HMD_LIST
	if (dp->is_hmd_dev) {
		dp_info(dp, "HMD dev\n");
		dp->dex.ui_setting = dex_setting;
		return size;
	}
#endif
	/* if dex setting is changed, then check if reconnecting is needed */
	if (dex_setting != dp->dex.ui_setting) {
		need_reconnect = true;

		if (dex_setting == DEX_ON) {
			/* check if cur mode is the same as best dex mode */
			if (drm_mode_match(&dp->cur_mode, &dp->dex.best_mode, DRM_MODE_MATCH_TIMINGS))
				need_reconnect = false;
		} else if (dex_setting == DEX_OFF) {
			/* check if cur mode is the same as best mode */
			if (drm_mode_match(&dp->cur_mode, &dp->best_mode, DRM_MODE_MATCH_TIMINGS))
				need_reconnect = false;
		}
	}

	dp->dex.ui_setting = dex_setting;

	/* reconnect if setting was mirroring(0) and dex is running(1), */
	if (dp->hpd_current_state != EXYNOS_HPD_UNPLUG && need_reconnect) {
		dp_info(dp, "reconnect to %s mode\n", dp->dex.ui_setting ? "dex":"mirroring");
		dp->dex.cur_state = DEX_RECONNECTING;
		dp_hpd_changed(dp, EXYNOS_HPD_UNPLUG);
		msleep(500);
		if (dp->pdic_cable_state == PDIC_NOTIFY_ATTACH &&
				dp->hpd_current_state != EXYNOS_HPD_PLUG) {
			dp_info(dp, "retry connecting\n");
			dp_hpd_changed(dp, EXYNOS_HPD_PLUG);
		}
	}

	dp_info(dp, "dex exit: state:%d, setting:%d\n",
			dp->dex.cur_state, dp->dex.ui_setting);

	return size;
}
static CLASS_ATTR_RW(dex);

static ssize_t dex_ver_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;
	struct dp_device *dp = get_dp_drvdata();

	ret = scnprintf(buf, 8, "%02X%02X\n", dp->sink_info.sw_ver[0],
			dp->sink_info.sw_ver[1]);

	return ret;
}
static CLASS_ATTR_RO(dex_ver);

static ssize_t monitor_info_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;
	struct dp_device *dp = get_dp_drvdata();

	ret = scnprintf(buf, PAGE_SIZE, "%s,0x%x,0x%x\n",
			dp->rx_edid_data.edid_manufacturer,
			dp->rx_edid_data.edid_product,
			dp->rx_edid_data.edid_serial);

	return ret;
}
static CLASS_ATTR_RO(monitor_info);
#endif

static ssize_t dp_sbu_sw_sel_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	struct dp_device *dp = get_dp_drvdata();
	int val[3] = {0,};
	int aux_sw_sel, aux_sw_oe;

	if (strnchr(buf, size, '-')) {
		dp_err(dp, "%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 3, val);

	aux_sw_sel = val[1];
	aux_sw_oe = val[2];
	dp_info(dp, "sbu_sw_sel(%d), sbu_sw_oe(%d)\n", aux_sw_sel, aux_sw_oe);

	if ((aux_sw_sel == 0 || aux_sw_sel == 1) && (aux_sw_oe == 0 || aux_sw_oe == 1)) {
		if (gpio_is_valid(dp->gpio_sw_sel))
			gpio_direction_output(dp->gpio_sw_sel, aux_sw_sel);
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		dp_aux_onoff(dp, !aux_sw_oe);
#endif
	} else
		dp_err(dp, "invalid aux switch parameter\n");

	return size;
}
static CLASS_ATTR_WO(dp_sbu_sw_sel);

#ifdef CONFIG_SEC_DISPLAYPORT_DBG
static void dp_pm_test(int pwr)
{
	u8 val[2] = {SET_POWER_NORMAL, SET_POWER_DOWN};
	u8 val1 = 0;
	struct dp_device *dp = get_dp_drvdata();

	if (dp->hpd_current_state == EXYNOS_HPD_UNPLUG)
		return;

	dp_err(dp, "set power state for CTS(%d)", pwr);
	if (pwr) {
		dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_SET_POWER, 1, &val[0]);
		dp_link_training(dp);
	} else
		dp_reg_dpcd_write(&dp->cal_res, DPCD_ADD_SET_POWER, 1, &val[1]);

	dp_reg_dpcd_read(&dp->cal_res, DPCD_ADD_SET_POWER, 1, &val1);
	dp_err(dp, "set power state for DPCD_ADD_SET_POWER(%d)", val1);
}

static ssize_t edid_test_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct dp_device *dp = get_dp_drvdata();
	u8 *edid_buf = dp->rx_edid_data.edid_buf;
	u32 blk_cnt;
	int i;
	ssize_t size = 0;

	if (!dp->edid_test_enable)
		return sprintf(buf, "edid test not enabled\n");

	blk_cnt = edid_buf[0x7e] + 1;
	if (blk_cnt > 4)
		return sprintf(buf, "invalied edid data %d\n", blk_cnt);

	for (i = 0; i <= blk_cnt * 128; i++) {
		size += sprintf(buf + size, " %02X", edid_buf[i]);
		if (i % 16 == 15)
			size += sprintf(buf + size, "\n");
		else if (i % 8 == 7)
			size += sprintf(buf + size, " ");
	}

	return size;
}

static ssize_t edid_test_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	struct dp_device *dp = get_dp_drvdata();
	u8 *edid_buf = dp->rx_edid_data.edid_buf;
	int buf_size = sizeof(dp->rx_edid_data.edid_buf);
	int i;
	int edid_idx = 0, hex_cnt = 0, buf_idx = 0;
	u8 hex = 0;
	u8 temp;
	int max_size = (buf_size * 6); /* including ',' ' ' and prefix ', 0xFF' */

	dp->edid_test_enable = false;
	memset(&dp->rx_edid_data, 0, sizeof(struct edid_data));

	for (i = 0; i < size && i < max_size; i++) {
		temp = *(buf + buf_idx++);
		/* value is separated by comma, space or line feed*/
		if (temp == ',' || temp == ' ' || temp == '\x0A') {
			if (hex_cnt != 0)
				edid_buf[edid_idx++] = hex;
			hex = 0;
			hex_cnt = 0;
		} else if (hex_cnt == 0 && temp == '0') {
			hex_cnt++;
			continue;
		} else if (temp == 'x' || temp == 'X') {
			hex_cnt = 0;
			hex = 0;
			continue;
		} else if (!temp || temp == '\0') { /* EOL */
			dp_info(dp, "parse end. edid cnt: %d\n", edid_idx);
			break;
		} else if (temp >= '0' && temp <= '9') {
			hex = (hex << 4) + (temp - '0');
			hex_cnt++;
		} else if (temp >= 'a' && temp <= 'f') {
			hex = (hex << 4) + (temp - 'a') + 0xa;
			hex_cnt++;
		} else if (temp >= 'A' && temp <= 'F') {
			hex = (hex << 4) + (temp - 'A') + 0xa;
			hex_cnt++;
		} else {
			dp_info(dp, "invalid value %c, %d\n", temp, hex_cnt);
			return size;
		}

		if (hex_cnt > 2 || edid_idx > buf_size + 1) {
			dp_info(dp, "wrong input. %d, %d, [%c]\n", hex_cnt, edid_idx, temp);
			return size;
		}
	}

	if (hex_cnt > 0)
		edid_buf[edid_idx] = hex;

	if (edid_idx > 0 && edid_idx % 128 == 0)
		dp->edid_test_enable = true;

	dp_info(dp, "edid size = %d\n", edid_idx);

	return size;
}
static CLASS_ATTR_RW(edid_test);

static ssize_t phy_tune0_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct dp_device *dp = get_dp_drvdata();
	int i;
	int *phy_tune_param0_1 = (int *)phy_eq_hbr0_1[dp->phy_tune_set];
	ssize_t size = 0;

	for (i = 0; i < 80; i++) {
		if (i % 20 == 0)
			size += sprintf(buf + size, "\n");
		size += sprintf(buf + size, " %d,", *phy_tune_param0_1);
		phy_tune_param0_1++;
		if (i % 5 == 4)
			size += sprintf(buf + size, "\n");

	}
	size += sprintf(buf + size, "\n");
	return size;
}

static ssize_t phy_tune0_store(struct class *dev,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	struct dp_device *dp = get_dp_drvdata();
	int val[24] = {0,};
	int ind = 0;
	int i = 0;
	int *phy_tune_param0_1;


	get_options(buf, 22, val);
	if (val[0] != 21 || val[1] > 3 || val[1] < 0) {
		dp_err(dp, "phy tune: invalid input %d %d\n", val[0], val[1]);
		return size;
	}

	ind = val[1];
	phy_tune_param0_1 = (int *)phy_eq_hbr0_1[dp->phy_tune_set][ind];
	for (i = 2; i < 22; i++)
		*phy_tune_param0_1++ = val[i];

	return size;
}
static CLASS_ATTR_RW(phy_tune0);

static ssize_t phy_tune2_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct dp_device *dp = get_dp_drvdata();
	int i;
	int *phy_tune_param2_3 = (int *)phy_eq_hbr2_3[dp->phy_tune_set];
	ssize_t size = 0;

	for (i = 0; i < 80; i++) {
		if (i % 20 == 0)
			size += sprintf(buf + size, "\n");
		size += sprintf(buf + size, " %d,", *phy_tune_param2_3);
		phy_tune_param2_3++;
		if (i % 5 == 4)
			size += sprintf(buf + size, "\n");

	}
	size += sprintf(buf + size, "\n");
	return size;
}

static ssize_t phy_tune2_store(struct class *dev,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	struct dp_device *dp = get_dp_drvdata();
	int val[24] = {0,};
	int ind = 0;
	int i = 0;
	int *phy_tune_param2_3;

	get_options(buf, 22, val);
	if (val[0] != 21 || val[1] > 3 || val[1] < 0) {
		dp_err(dp, "phy tune: invalid input %d %d\n", val[0], val[1]);
		return size;
	}

	ind = val[1];
	phy_tune_param2_3 = (int *)phy_eq_hbr2_3[dp->phy_tune_set][ind];
	for (i = 2; i < 22; i++)
		*phy_tune_param2_3++ = val[i];

	return size;
}
static CLASS_ATTR_RW(phy_tune2);

extern struct fb_audio *edid_get_test_audio_info(void);
static ssize_t audio_test_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int bit = 0;
	int sample = 0;
	struct fb_audio *aud_info = edid_get_test_audio_info();
	int tmp_ch = aud_info->channel_count;
	int channel = 0;

	while (tmp_ch) {
		channel++;
		tmp_ch >>= 1;
	}

	switch (aud_info->bit_rates) {
	case FB_AUDIO_16BIT:
		bit = 16;
		break;
	case FB_AUDIO_20BIT:
		bit = 20;
		break;
	case FB_AUDIO_24BIT:
		bit = 24;
	}

	switch (aud_info->sample_rates) {
	case FB_AUDIO_32KHZ:
		sample = 32;
		break;
	case FB_AUDIO_44KHZ:
		sample = 44;
		break;
	case FB_AUDIO_48KHZ:
		sample = 48;
		break;
	case FB_AUDIO_88KHZ:
		sample = 88;
		break;
	case FB_AUDIO_96KHZ:
		sample = 96;
		break;
	case FB_AUDIO_176KHZ:
		sample = 176;
		break;
	case FB_AUDIO_192KHZ:
		sample = 192;
		break;
	}

	return sprintf(buf, "channel: %d, bit rate: %d, sample rate: %d\n",
				channel, bit, sample);
}

static ssize_t audio_test_store(struct class *dev,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	int val[6] = {0,};
	struct dp_device *dp = get_dp_drvdata();
	struct fb_audio *aud_info = edid_get_test_audio_info();

	if (strnchr(buf, size, '-')) {
		dp_info(dp, "%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 4, val);
	if (val[0] != 3 || val[1] == 0) {
		dp_info(dp, "invalid input. test disable\n");
		aud_info->channel_count = 0;
		aud_info->bit_rates = 0;
		aud_info->sample_rates = 0;
		return size;
	}

	if (val[1] > 0 && val[1] <= 8)
		aud_info->channel_count = (1 << (val[1] - 1));
	else
		aud_info->channel_count = 2;

	switch (val[2]) {
	case 16:
		aud_info->bit_rates = FB_AUDIO_16BIT;
		break;
	case 20:
		aud_info->bit_rates = FB_AUDIO_20BIT;
		break;
	case 24:
		aud_info->bit_rates = FB_AUDIO_24BIT;
		break;
	default:
		aud_info->bit_rates = FB_AUDIO_16BIT;
	}

	switch (val[3]) {
	case 32:
		aud_info->sample_rates = FB_AUDIO_32KHZ;
		break;
	case 44:
		aud_info->sample_rates = FB_AUDIO_44KHZ;
		break;
	case 48:
		aud_info->sample_rates = FB_AUDIO_48KHZ;
		break;
	case 88:
		aud_info->sample_rates = FB_AUDIO_88KHZ;
		break;
	case 96:
		aud_info->sample_rates = FB_AUDIO_96KHZ;
		break;
	case 176:
		aud_info->sample_rates = FB_AUDIO_176KHZ;
		break;
	case 192:
		aud_info->sample_rates = FB_AUDIO_192KHZ;
		break;
	default:
		aud_info->sample_rates = FB_AUDIO_48KHZ;
	}

	return size;
}
static CLASS_ATTR_RW(audio_test);

static ssize_t dp_test_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct dp_device *dp = get_dp_drvdata();
	int size;
#ifdef FEATURE_MANAGE_HMD_LIST
	int i;
#endif

	size = snprintf(buf, PAGE_SIZE, "0: HPD test\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "1: uevent test\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "2: CTS power management test\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "3: set lane count, link rate\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "4: hdcp restart\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "5: link training test\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "6: unplug work test\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "7: audio bist mode(on,ch,bit,fs)\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "9: send poor connect event\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "10: HDR test enable\n");

	if (gpio_is_valid(dp->gpio_sw_oe) && gpio_is_valid(dp->gpio_sw_oe))
		size += snprintf(buf + size, PAGE_SIZE - size, "\n# gpio oe %d, sel %d, direction %d\n",
				gpio_get_value(dp->gpio_sw_oe),
				gpio_get_value(dp->gpio_sw_sel),
				gpio_get_value(dp->gpio_usb_dir));
	else
		size += snprintf(buf + size, PAGE_SIZE - size, "\n# gpio direction %d\n",
			gpio_get_value(dp->gpio_usb_dir));

#ifdef FEATURE_MANAGE_HMD_LIST
	for (i = 0; i < MAX_NUM_HMD; i++) {
		if (strlen(dp->hmd_list[i].monitor_name) > 1 ||
					dp->hmd_list[i].ven_id != 0 ||
					dp->hmd_list[i].prod_id != 0) {
			dp_info(dp, "HMD%02d: %s, 0x%04x, 0x%04x\n", i,
					dp->hmd_list[i].monitor_name,
					dp->hmd_list[i].ven_id,
					dp->hmd_list[i].prod_id);
			size += snprintf(buf + size, PAGE_SIZE - size,
					"HMD%02d: %s, 0x%04x, 0x%04x\n", i,
						dp->hmd_list[i].monitor_name,
						dp->hmd_list[i].ven_id,
						dp->hmd_list[i].prod_id);
		}
	}
#endif
	return size;
}
static ssize_t dp_test_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct dp_device *dp = get_dp_drvdata();
	struct dp_audio_config_data audio_config_data = {0, };
	int val[8] = {0,};

	if (strnchr(buf, size, '-')) {
		dp_err(dp, "%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 6, val);

	switch (val[1]) {
	case 0:
		if (val[2] == 0 || val[2] == 1)
			dp_hpd_changed(dp, val[2]);
		break;
	case 1:
		if (val[2] == 0) {
			dp->connector.status = connector_status_disconnected;
			drm_kms_helper_hotplug_event(dp->connector.dev);
		} else if (val[2] == 1) {
			edid_update_drm(dp);
			dp->connector.status = connector_status_connected;
			drm_kms_helper_hotplug_event(dp->connector.dev);
		}
		break;
	case 2:
		if (val[2] == 0 || val[2] == 1)
			dp_pm_test(val[2]);
		break;
	case 3:/*lane count, link rate*/
		dp_info(dp, "test lane: %d, link rate: 0x%x\n", val[2], val[3]);
		if ((val[2] == 1 || val[2] == 2 || val[2] == 4) &&
			(val[3] == 6 || val[3] == 0xa || val[3] == 0x14 || val[3] == 0x1e)) {
			g_dp_debug_param.lane_cnt = val[2];
			g_dp_debug_param.link_rate = val[3];
			g_dp_debug_param.param_used = 1;
		} else {
			g_dp_debug_param.lane_cnt = 0;
			g_dp_debug_param.link_rate = 0;
			g_dp_debug_param.param_used = 0;
		}
		break;
	case 4:
		if (dp->hdcp_ver == HDCP_VERSION_2_2) {
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
			hdcp_dplink_set_reauth();
#endif
			dp_hdcp22_enable(0);
			queue_delayed_work(dp->hdcp2_wq,
					&dp->hdcp22_work, msecs_to_jiffies(0));
		} else if (dp->hdcp_ver == HDCP_VERSION_1_3) {
			hdcp13_info.auth_state = HDCP13_STATE_FAIL;
			queue_delayed_work(dp->dp_wq,
					&dp->hdcp13_work, msecs_to_jiffies(0));
		}
		break;
	case 5:
		dp_link_training(dp);
		break;
	case 6:
		queue_delayed_work(dp->dp_wq,
				&dp->hpd_unplug_work, 0);
		break;
	case 7:
		audio_config_data.audio_enable = val[2];
		audio_config_data.audio_fs = FS_48KHZ;
		audio_config_data.audio_channel_cnt = 2;
		audio_config_data.audio_bit = AUDIO_16_BIT;
		audio_config_data.audio_packed_mode = 0;
		audio_config_data.audio_word_length = 0;
		switch (val[3]) {
		case 2:
			audio_config_data.audio_channel_cnt = 2;
			break;
		case 6:
			audio_config_data.audio_channel_cnt = 6;
			break;
		case 8:
			audio_config_data.audio_channel_cnt = 8;
			break;
		default:
			break;
		}

		switch (val[4]) {
		case 16:
			audio_config_data.audio_bit = AUDIO_16_BIT;
			break;
		case 24:
			audio_config_data.audio_bit = AUDIO_24_BIT;
			break;
		default:
			break;
		}

		switch (val[5]) {
		case 48:
			audio_config_data.audio_fs = FS_48KHZ;
			break;
		case 96:
			audio_config_data.audio_fs = FS_96KHZ;
			break;
		case 192:
			audio_config_data.audio_fs = FS_192KHZ;
			break;
		default:
			break;
		}
		dp_audio_bist_config(dp, audio_config_data);
		break;
	case 9:
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
		dp_set_switch_poor_connect();
#endif
		break;
	case 10:
		dp->hdr_test_enable = !!val[2];
		break;
	default:
		break;
	};

	return size;
}

static CLASS_ATTR_RW(dp_test);
#endif

int dp_debug_dump(void)
{
	int acquired;
	struct dp_device *dp = get_dp_drvdata();

	dp_info(dp, "=== DP_DEBUG_DUMP START ===\n");
	acquired = console_trylock();

	dp_dump_registers(dp);
	dp_phy_dump_registers(dp);
	dp_dump_cr(dp);

	if (acquired)
		console_unlock();

	dp_info(dp, "=== DP_DEBUG_DUMP DONE ===\n");

	return 0;
}

static int dp_debug_dump_show(struct seq_file *s, void *unused)
{
	struct dp_device *dp = s->private;

	if (dp->state != DP_STATE_ON) {
		dp_info(dp, "%s: dp is not ON(%d)\n",
				__func__, dp->state);
		return 0;
	}
	dp_dump_registers(dp);
	dp_phy_dump_registers(dp);
	dp_phy_tca_dump_registers(dp);
	return 0;
}

static int dp_debug_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, dp_debug_dump_show, inode->i_private);
}

static const struct file_operations dp_dump_fops = {
	.open = dp_debug_dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static enum drm_mode_status dp_mode_valid(struct drm_encoder *encoder,
					    const struct drm_display_mode *mode)
{
	struct dp_device *dp = encoder_to_dp(encoder);

	if (dp->hpd_current_state == EXYNOS_HPD_PLUG)
		return MODE_OK;

	return MODE_NOMODE;
}

bool dp_match_videoformat(const struct v4l2_dv_timings *t1,
		struct drm_display_mode *mode)
{
	if (t1->bt.width == mode->hdisplay &&
			t1->bt.height == mode->vdisplay &&
			t1->bt.pixelclock == mode->clock * 1000 &&
			t1->bt.hfrontporch == mode->hsync_start - mode->hdisplay &&
			t1->bt.hsync == mode->hsync_end - mode->hsync_start &&
			t1->bt.hbackporch == mode->htotal - mode->hsync_end &&
			t1->bt.height + t1->bt.vfrontporch + t1->bt.vsync + t1->bt.vbackporch == mode->vtotal)
		return true;

	return false;
}

#ifndef FEATURE_USE_DRM_EDID_PARSER
static int dp_mode2videoformat(struct drm_display_mode *mode)
{
	int i;
	struct dp_device *dp = get_dp_drvdata();

	for (i = 0; i < supported_videos_pre_cnt; i++) {
		if (dp_match_videoformat(&supported_videos[i].dv_timings,
					mode))
			return i;
	}

	dp_info(dp, "%s " DRM_MODE_FMT "\n", __func__, DRM_MODE_ARG(mode));

	return -EINVAL;
}
#endif

static void dp_atomic_mode_set(struct drm_encoder *encoder,
				 struct drm_crtc_state *crtc_state,
				 struct drm_connector_state *conn_state)
{
	struct dp_device *dp = encoder_to_dp(encoder);
	struct drm_display_mode *adjusted_mode = &crtc_state->adjusted_mode;

#ifdef FEATURE_USE_DRM_EDID_PARSER
	dp_info(dp, "%s " DRM_MODE_FMT "\n", __func__, DRM_MODE_ARG(adjusted_mode));

	if (dp->rx_edid_data.hdr_support) {
		if (dp_check_link_rate_pixel_clock(dp, dp->cur_link_rate, dp->cur_lane_cnt,
				dp_reg_get_video_clk(), BPC_10)) {
			dp->cal_res.bpc = BPC_10;
		} else {
			dp->rx_edid_data.hdr_support = 0;
			dp->cal_res.bpc = BPC_8;
			dp_info(dp, "Not enough pixel clock for HDR\n");
		}
	}
#else
	videoformat dp_setting_videoformat = V640X480P60;

	dp_info(dp, "%s " DRM_MODE_FMT "\n", __func__, DRM_MODE_ARG(adjusted_mode));
	dp_setting_videoformat = dp_mode2videoformat(adjusted_mode);
	if (dp_setting_videoformat != -EINVAL) {
		dp->cur_video = dp_setting_videoformat;
		dp_info(dp, "New cur_video = %s\n",
				supported_videos[dp->cur_video].name);

		if (dp_check_pixel_clock_for_hdr(dp, dp_setting_videoformat) == true
				&& dp_setting_videoformat >= V3840X2160P50
				&& dp_setting_videoformat < V640X10P60SACRC) {
			dp_info(dp, "sink support HDR\n");
			/* BPC_10 should be enabled when support HDR */
			dp->cal_res.bpc = BPC_10;
		} else {
			dp->rx_edid_data.hdr_support = 0;
			dp->cal_res.bpc = BPC_8;
		}
	} else {
		dp_err(dp, "videoformat not supported in dp_atomic_mode_set\n");

		/*fail safe mode (640x480) with 6 bpc*/
		dp->cur_video = V640X480P60;
		dp->cal_res.bpc = BPC_6;
	}
#endif
}

static int dp_atomic_check(struct drm_encoder *encoder,
			     struct drm_crtc_state *crtc_state,
			     struct drm_connector_state *state)
{
	struct dp_device *dp = encoder_to_dp(encoder);
	struct drm_display_mode *adjusted_mode = &crtc_state->adjusted_mode;
	struct hdr_output_metadata *hdr_metadata;

#ifdef FEATURE_USE_DRM_EDID_PARSER
	dp_info(dp, "%s " DRM_MODE_FMT "\n", __func__, DRM_MODE_ARG(adjusted_mode));
#endif
	if (!dp->rx_edid_data.hdr_support)
		return 0;

	if (state->hdr_output_metadata) {
		hdr_metadata = state->hdr_output_metadata->data;
		if (hdr_metadata) {
			dp_info(dp, "[%s] max_dml(%d) min_dml(%d) max_cll(%d) max_fall(%d)\n", __func__,
				hdr_metadata->hdmi_metadata_type1.max_display_mastering_luminance,
				hdr_metadata->hdmi_metadata_type1.min_display_mastering_luminance,
				hdr_metadata->hdmi_metadata_type1.max_cll,
				hdr_metadata->hdmi_metadata_type1.max_fall);
			if (dp->rx_edid_data.hdr_support)
				dp_set_hdr_infoframe(state);
		}
	} else {
		dp->rx_edid_data.hdr_support = 0;
		dp->cal_res.bpc = BPC_8;
		dp_info(dp, "%s No HDR meta data\n", __func__);
	}

#ifndef FEATURE_USE_DRM_EDID_PARSER
	if (crtc_state->mode_changed) {
		if (dp_mode2videoformat(adjusted_mode) < 0) {
			dp_err(dp, "videoformat not supported in dp_atomic_check\n");
			return -EINVAL;
		}
	}
#endif
	return 0;
}

static const struct drm_encoder_helper_funcs dp_encoder_helper_funcs = {
	.mode_valid = dp_mode_valid,
	.atomic_mode_set = dp_atomic_mode_set,
	.enable = dp_enable,
	.disable = dp_disable,
	.atomic_check = dp_atomic_check,
};

static const struct drm_encoder_funcs dp_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static enum drm_connector_status
dp_detect(struct drm_connector *connector, bool force)
{
	return connector->status;
}

static void dp_connector_destroy(struct drm_connector *connector)
{
	drm_connector_cleanup(connector);
}

#define HDR_DOLBY_VISION	BIT(1)
#define HDR_HDR10		BIT(2)
#define HDR_HLG			BIT(3)

static int dp_get_property(struct drm_connector *connector,
				const struct drm_connector_state *state,
				struct drm_property *property, uint64_t *val)
{
	struct dp_device *dp = connector_to_dp(connector);
	struct exynos_drm_private *priv = connector->dev->dev_private;
	struct exynos_drm_connector_properties *props = &priv->connector_props;

	if (dp->rx_edid_data.hdr_support) {
		pr_debug("%s: prop: %s\n", __func__, property->name);
		if (property == props->hdr_sink_connected)
			*val = dp->rx_edid_data.hdr_support;
		else if (property == props->hdr_formats)
			*val = HDR_HDR10;
		else if (property == props->min_luminance)
			*val = dp->rx_edid_data.min_lumi_data;
		else if (property == props->max_luminance)
			*val = dp->rx_edid_data.max_lumi_data;
		else if (property == props->max_avg_luminance)
			*val = dp->rx_edid_data.max_average_lumi_data;
		else
			return -EINVAL;
	} else { /* Add initial value For P version platform */
		if (property == props->hdr_sink_connected)
			*val = 0;
		else if (property == props->hdr_formats)
			*val = 0;
		else if (property == props->min_luminance)
			*val = 5;
		else if (property == props->max_luminance)
			*val = 2500000;
		else if (property == props->max_avg_luminance)
			*val = 1200000;
		else
			return -EINVAL;
	}

	return 0;
}

static const struct drm_connector_funcs dp_connector_funcs = {
	.detect = dp_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = dp_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.atomic_get_property = dp_get_property,
};

static int dp_get_modes(struct drm_connector *connector)
{
	struct dp_device *dp = connector_to_dp(connector);
	struct edid *edid = (struct edid *)dp->rx_edid_data.edid_buf;
	int mode_num;

	/* clean up first */
	dp_mode_clean_up(dp);

	drm_connector_update_edid_property(connector, edid);
	mode_num = drm_add_edid_modes(connector, edid);
	if (mode_num > 0) {
		dp_mode_filter_out(dp, mode_num);
	}

	if (mode_num == 0 || dp->fail_safe) {
		/* set default mode to VGA */
		struct drm_display_mode *newmode;
		struct drm_device *dev = connector->dev;

		dp_info(dp, "%s add VGA for fail-safe\n", __func__);
		dp_mode_set_fail_safe(dp);
		newmode = drm_mode_duplicate(dev, &dp->cur_mode);
		if (newmode) {
			drm_mode_probed_add(connector, newmode);
			mode_num++;
		}
	}

	dp_info(dp, "%s count: %d, cur mode: %s@%d\n", __func__, mode_num,
			dp->cur_mode.name, drm_mode_vrefresh(&dp->cur_mode));

	return mode_num;
}

static const struct drm_connector_helper_funcs dp_connector_helper_funcs = {
	.get_modes = dp_get_modes,
};

static int dp_attach_hdr_property(struct drm_connector *connector)
{
	struct exynos_drm_private *priv = connector->dev->dev_private;
	struct exynos_drm_connector_properties *props = &priv->connector_props;

	drm_object_attach_property(&connector->base,
			props->hdr_sink_connected, 0);
	drm_object_attach_property(&connector->base,
			props->hdr_formats, 0);
	drm_object_attach_property(&connector->base,
			props->min_luminance, 0);
	drm_object_attach_property(&connector->base,
			props->max_luminance, 0);
	drm_object_attach_property(&connector->base,
			props->max_avg_luminance, 0);
	drm_object_attach_property(&connector->base,
			connector->dev->mode_config.hdr_output_metadata_property, 0);

	return 0;
}

static int dp_create_connector(struct drm_encoder *encoder)
{
	struct dp_device *dp = encoder_to_dp(encoder);
	struct drm_connector *connector = &dp->connector;
	int ret;

	connector->polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_init(encoder->dev, connector,
				 &dp_connector_funcs,
				 DRM_MODE_CONNECTOR_DisplayPort);
	if (ret) {
		dp_err(dp, "Failed to initialize connector with drm\n");
		return ret;
	}

	drm_connector_helper_add(connector, &dp_connector_helper_funcs);
	drm_connector_register(connector);
	drm_connector_attach_encoder(connector, encoder);

	dp_attach_hdr_property(connector);

	return 0;
}

static int dp_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_encoder *encoder = dev_get_drvdata(dev);
	struct dp_device *dp = encoder_to_dp(encoder);
	struct drm_device *drm_dev = data;
	int ret = 0;

	dp_debug(dp, "%s +\n", __func__);

	drm_encoder_init(drm_dev, encoder, &dp_encoder_funcs,
			 DRM_MODE_ENCODER_TMDS, NULL);
	drm_encoder_helper_add(encoder, &dp_encoder_helper_funcs);

	encoder->possible_crtcs = exynos_drm_get_possible_crtcs(encoder,
							dp->output_type);
	if (!encoder->possible_crtcs) {
		dp_err(dp, "failed to get possible crtc, ret = %d\n", ret);
		drm_encoder_cleanup(encoder);
		return -ENOTSUPP;
	}

	ret = dp_create_connector(encoder);
	if (ret) {
		dp_err(dp, "failed to create connector ret = %d\n", ret);
		drm_encoder_cleanup(encoder);
		return ret;
	}

	dp_info(dp, "%s -\n", __func__);

	return ret;
}

static void dp_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct drm_encoder *encoder = dev_get_drvdata(dev);
	struct dp_device *dp = encoder_to_dp(encoder);

	dp_debug(dp, "%s +\n", __func__);
}

static const struct component_ops dp_component_ops = {
	.bind	= dp_bind,
	.unbind	= dp_unbind,
};

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
int dp_itmon_notifier(struct notifier_block *nb, unsigned long act, void *data)
{
	struct dp_device *dp;
	struct itmon_notifier *itmon_data = data;

	dp = container_of(nb, struct dp_device, itmon_nb);

	if (dp->itmon_notified)
		return NOTIFY_DONE;

	if (IS_ERR_OR_NULL(itmon_data))
		return NOTIFY_DONE;

	/* port is master and dest is target */
	if ((itmon_data->dest &&
		(strncmp("HSI0_P", itmon_data->dest, sizeof("HSI0_P")) == 0))) {

		dp_info(dp, "[DP][debug] pm_runtime usage_cnt(%d)\n",
			atomic_read(&dp->dev->power.usage_count));
		dp_info(dp, "[DP][debug] usbdp phy usage cnt(%d)\n",
			atomic_read(&dp->cal_res.usbdp_phy_en_cnt));

		if (dp_hsi0_domain_power_check() == 0)
			dp_hsi0_clk_check();

		dp->itmon_notified = true;
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}
#endif

#define MAX_NAME_SIZE	32
static int dp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct dp_device *dp = NULL;

	struct class *dp_class;

	char name[MAX_NAME_SIZE];

#ifdef CONFIG_SEC_DISPLAYPORT_LOGGER
	dp_logger_init();
#endif

	dev_info(dev, "%s start\n", __func__);

	dp = devm_kzalloc(dev, sizeof(struct dp_device), GFP_KERNEL);
	if (!dp) {
		dp_err(dp, "failed to allocate dp device.\n");
		ret = -ENOMEM;
		goto err;
	}

	dp_init_info(dp);

	dma_set_mask(dev, DMA_BIT_MASK(32));

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	dp->gpio_sw_oe = -EINVAL;
	dp->gpio_sw_sel = -EINVAL;
	dp->gpio_usb_dir = -EINVAL;
	ret = dp_parse_dt(dp, dev);
	if (ret)
		dp_err(dp, "skip gpio init\n", __func__);
#else
	ret = dp_parse_dt(dp, dev);
	if (ret)
		goto err;
#endif

	dp_drvdata = dp;

	dp->output_type = EXYNOS_DISPLAY_TYPE_DP0_SST1;

	spin_lock_init(&dp->slock);
	mutex_init(&dp->cmd_lock);
	mutex_init(&dp->hpd_lock);
	mutex_init(&dp->cal_res.aux_lock);
	mutex_init(&dp->training_lock);
	mutex_init(&dp->hdcp2_lock);

	//spin_lock_init(&dp->spinlock_sfr);

#ifdef FEATURE_MANAGE_HMD_LIST
	mutex_init(&dp->hmd_lock);
	strlcpy(dp->hmd_list[0].monitor_name, "PicoVR", MON_NAME_LEN);
	dp->hmd_list[0].ven_id = 0x2d40;
	dp->hmd_list[0].prod_id = 0x0000;
#endif

	ret = dp_init_resources(dp, pdev);
	if (ret)
		goto err;

	platform_set_drvdata(pdev, dp);

	dp->dp_wq = create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!dp->dp_wq) {
		dp_err(dp, "create wq failed.\n");
		goto err;
	}

	dp->hdcp2_wq = create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!dp->hdcp2_wq) {
		dp_err(dp, "create hdcp2_wq failed.\n");
		goto err;
	}

	//INIT_DELAYED_WORK(&dp->hpd_plug_work, dp_hpd_plug_work);
	INIT_DELAYED_WORK(&dp->hpd_unplug_work, dp_hpd_unplug_work);
	INIT_DELAYED_WORK(&dp->hpd_irq_work, dp_hpd_irq_work);
	INIT_DELAYED_WORK(&dp->hdcp13_work, dp_hdcp13_run);
	INIT_DELAYED_WORK(&dp->hdcp22_work, dp_hdcp22_run);
	INIT_DELAYED_WORK(&dp->hdcp13_integrity_check_work, dp_hdcp13_integrity_check_work);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER) || IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
#ifdef CONFIG_USE_DISPLAYPORT_PDIC_EVENT_QUEUE
	init_waitqueue_head(&dp->dp_ready_wait);
	INIT_LIST_HEAD(&dp->list_pd);
	INIT_DELAYED_WORK(&dp->pdic_event_proceed_work,
			dp_pdic_event_proceed_work);
	mutex_init(&dp->pdic_lock);
#endif
	INIT_DELAYED_WORK(&dp->notifier_register_work,
			dp_notifier_register_work);
	queue_delayed_work(dp->dp_wq, &dp->notifier_register_work,
			msecs_to_jiffies(10000));
#endif
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
	ret = switch_dev_register(&switch_secdp_msg);
	if (ret)
		dp_err(dp, "Failed to register dp msg switch\n");

	ret = switch_dev_register(&switch_secdp_hpd);
	if (ret)
		dp_err(dp, "Failed to register dp hpd switch\n");
#endif


	pm_runtime_enable(dev);

#if defined(CONFIG_CPU_IDLE)
	dp->idle_ip_index =
		exynos_get_idle_ip_index(dev_name(&pdev->dev), 1);
	if (dp->idle_ip_index < 0)
		dp_warn(dp, "idle ip index is not provided for DP\n");
	exynos_update_ip_idle_status(dp->idle_ip_index, 1);
#endif

	ret = device_init_wakeup(dp->dev, true);
	if (ret) {
		dev_err(dp->dev, "failed to init wakeup device\n");
		return -EINVAL;
	}

	dp_class = class_create(THIS_MODULE, "dp_sec");
	if (IS_ERR(dp_class))
		dp_err(dp, "failed to creat dp_class\n");
	else {
#ifdef DP_TEST
		ret = class_create_file(dp_class, &class_attr_link);
		if (ret)
			dp_err(dp, "failed to create attr_link\n");
		ret = class_create_file(dp_class, &class_attr_bpc);
		if (ret)
			dp_err(dp, "failed to create attr_bpc\n");
		ret = class_create_file(dp_class, &class_attr_range);
		if (ret)
			dp_err(dp, "failed to create attr_range\n");
		ret = class_create_file(dp_class, &class_attr_edid);
		if (ret)
			dp_err(dp, "failed to create attr_edid\n");
		ret = class_create_file(dp_class, &class_attr_bist);
		if (ret)
			dp_err(dp, "failed to create attr_bist\n");
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
		ret = class_create_file(dp_class, &class_attr_dp_drm);
		if (ret)
			dp_err(dp, "failed to create attr_dp_drm\n");
#endif
		ret = class_create_file(dp_class, &class_attr_dp);
		if (ret)
			dp_err(dp, "failed to create attr_test\n");
		ret = class_create_file(dp_class, &class_attr_forced_resolution);
		if (ret)
			dp_err(dp, "failed to create attr_dp_forced_resolution\n");
		ret = class_create_file(dp_class, &class_attr_reduced_resolution);
		if (ret)
			dp_err(dp, "failed to create attr_dp_reduced_resolution\n");
		ret = class_create_file(dp_class, &class_attr_log_level);
		if (ret)
			dp_err(dp, "failed to create class_attr_log_level\n");
#endif
#ifdef CONFIG_SEC_DISPLAYPORT_DBG
		ret = class_create_file(dp_class, &class_attr_edid_test);
		if (ret)
			dp_err(dp, "failed to create attr_edid\n");
		ret = class_create_file(dp_class, &class_attr_phy_tune0);
		if (ret)
			dp_err(dp, "failed to create attr_phy_tune0\n");
		ret = class_create_file(dp_class, &class_attr_phy_tune2);
		if (ret)
			dp_err(dp, "failed to create attr_phy_tune2\n");
		ret = class_create_file(dp_class, &class_attr_dp_test);
		if (ret)
			dp_err(dp, "failed to create attr_dp_test\n");
		ret = class_create_file(dp_class, &class_attr_audio_test);
		if (ret)
			dp_err(dp, "failed to create attr_audio_test\n");
#endif
#ifdef FEATURE_DEX_SUPPORT
		ret = class_create_file(dp_class, &class_attr_dex);
		if (ret)
			dp_err(dp, "failed to create attr_dp_dex\n");
		ret = class_create_file(dp_class, &class_attr_dex_ver);
		if (ret)
			dp_err(dp, "failed to create attr_dp_dex_ver\n");
		ret = class_create_file(dp_class, &class_attr_monitor_info);
		if (ret)
			dp_err(dp, "failed to create attr_dp_monitor_info\n");
#endif
		ret = class_create_file(dp_class, &class_attr_dp_sbu_sw_sel);
		if (ret)
			dp_err(dp, "failed to create class_attr_dp_sbu_sw_sel\n");
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_init(dp_class);
#endif
#ifdef CONFIG_SEC_DISPLAYPORT_SELFTEST
		dp->hpd_changed = dp_hpd_changed;
		self_test_init(dp, dp_class);
#endif
	}



	dp->debug_root = debugfs_create_dir("dp", NULL);
	if (!dp->debug_root) {
		dp_err(dp, "failed to create debugfs root directory.\n");
		ret = -ENOENT;
		goto err;
	}

	snprintf(name, MAX_NAME_SIZE, "dp_dump");
	dp->debug_dump = debugfs_create_file(name, 0444,
			dp->debug_root, dp, &dp_dump_fops);
	if (!dp->debug_dump) {
		dp_err(dp, "failed to create SFR dump debugfs file\n");
		goto err;
	}

	g_dp_debug_param.param_used = 0;
	g_dp_debug_param.link_rate = LINK_RATE_2_7Gbps;
	g_dp_debug_param.lane_cnt = 0x04;

#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
	dp->drm_start_state = DRM_OFF;
	dp_register_func_for_hdcp22(dp_hdcp22_enable,
			dp_dpcd_read_for_hdcp22,
			dp_dpcd_write_for_hdcp22);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	dp->itmon_nb.notifier_call = dp_itmon_notifier;
	itmon_notifier_chain_register(&dp->itmon_nb);
#endif
	dp_info(dp, "dp driver has been probed.\n");
	return component_add(dp->dev, &dp_component_ops);

err:
	return ret;
}

static const struct of_device_id dp_of_match[] = {
	{ .compatible = "samsung,exynos-dp" },
	{},
};
MODULE_DEVICE_TABLE(of, dp_of_match);

struct platform_driver dp_driver __refdata = {
	.probe			= dp_probe,
	.remove			= dp_remove,
	.driver = {
		.name		= "exynos-drmdp",
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(dp_of_match),
	}
};

MODULE_AUTHOR("Manseok Kim <manseoks.kim@samsung.com>");
MODULE_DESCRIPTION("Samusung EXYNOS SoC DisplayPort driver");
MODULE_LICENSE("GPL");
