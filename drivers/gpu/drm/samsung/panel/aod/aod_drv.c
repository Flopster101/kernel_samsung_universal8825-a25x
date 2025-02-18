/*
 * linux/drivers/video/fbdev/exynos/aod/aod_drv.c
 *
 * Source file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>

#include "aod_drv_ioctl.h"
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "aod_drv.h"

struct seqinfo *find_aod_seq_by_name(struct aod_dev_info *aod, char *seqname)
{
	if (!aod) {
		panel_err("aod is null\n");
		return NULL;
	}

	return find_panel_seq_by_name(to_panel_device(aod), seqname);
}

bool check_aod_seqtbl_exist(struct aod_dev_info *aod, char *seqname)
{
	if (!aod) {
		panel_err("aod is null\n");
		return false;
	}

	return check_seqtbl_exist(to_panel_device(aod), seqname);
}
EXPORT_SYMBOL(check_aod_seqtbl_exist);

int panel_do_aod_seqtbl_by_name_nolock(struct aod_dev_info *aod, char *seqname)
{
	struct seqinfo *seq;

	if (!aod) {
		panel_err("aod is null\n");
		return -EINVAL;
	}

	seq = find_aod_seq_by_name(aod, seqname);
	if (!seq)
		return -EINVAL;

	return execute_sequence_nolock(to_panel_device(aod), seq);
}
EXPORT_SYMBOL(panel_do_aod_seqtbl_by_name_nolock);

int panel_do_aod_seqtbl_by_name(struct aod_dev_info *aod, char *seqname)
{
	int ret = 0;
	struct panel_device *panel;

	if (!aod)
		return -EINVAL;

	panel = to_panel_device(aod);
	panel_mutex_lock(&panel->op_lock);
	ret = panel_do_aod_seqtbl_by_name_nolock(aod, seqname);
	panel_mutex_unlock(&panel->op_lock);

	return ret;
}

static int aod_init_panel(struct aod_dev_info *aod, int lock)
{
	int ret;
	struct aod_ioctl_props *props;
	struct panel_device *panel;

	if (!aod) {
		panel_err("aod is null\n");
		return -EINVAL;
	}

	panel = to_panel_device(aod);
	props = &aod->props;

	panel_mutex_lock(&aod->lock);
	if (!props->self_mask_en) {
		panel_mutex_unlock(&aod->lock);
		return 0;
	}

	if (lock == INIT_WITH_LOCK)
		panel_mutex_lock(&panel->op_lock);

	if (check_aod_seqtbl_exist(aod, SELF_MASK_IMG_SEQ)) {
		ret = panel_do_aod_seqtbl_by_name_nolock(aod, SELF_MASK_IMG_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					SELF_MASK_IMG_SEQ);
			goto err;
		}

		if (aod->custom_ops && aod->custom_ops->self_mask_data_check) {
			ret = aod->custom_ops->self_mask_data_check(aod);
			if (ret < 0) {
				panel_err("failed to run self_mask_data_check(%d)\n", ret);
				goto err;
			}
			panel_info("self_mask_data_check passed!\n");
		}
	}

	ret = panel_do_aod_seqtbl_by_name_nolock(aod, SELF_MASK_ENA_SEQ);
	if (ret < 0) {
		panel_err("failed to run sequence(%s)\n",
				SELF_MASK_ENA_SEQ);
		goto err;
	}

err:
	if (lock == INIT_WITH_LOCK)
		panel_mutex_unlock(&panel->op_lock);

	panel_mutex_unlock(&aod->lock);

	return ret;
}

static int aod_enter_to_lpm(struct aod_dev_info *aod_dev)
{
	int ret = 0;
	struct aod_ioctl_props *props = &aod_dev->props;

	panel_mutex_lock(&aod_dev->lock);
	if (props->self_mask_en) {
		ret = panel_do_aod_seqtbl_by_name(aod_dev, SELF_MASK_DIS_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					SELF_MASK_DIS_SEQ);
			goto err;
		}
	}

	if (check_aod_seqtbl_exist(aod_dev, ENTER_AOD_SEQ)) {
		ret = panel_do_aod_seqtbl_by_name(aod_dev, ENTER_AOD_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					ENTER_AOD_SEQ);
			goto err;
		}
	}

	if (props->partial.scan_en || props->partial.hlpm_scan_en) {
		ret = panel_do_aod_seqtbl_by_name(aod_dev, ENABLE_PARTIAL_SCAN_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					ENABLE_PARTIAL_SCAN_SEQ);
			goto err;
		}
	}

	/* write image to side ram */
	if (aod_dev->reset_flag) {
#if defined(SUPPORT_SELF_ICON)
		if (aod_dev->icon_img.up_flag) {
			panel_info("write self icon img\n");
			ret = panel_do_aod_seqtbl_by_name(aod_dev, SELF_ICON_IMG_SEQ);
			if (ret < 0) {
				panel_err("failed to run sequence(%s)\n",
						SELF_ICON_IMG_SEQ);
				goto err;
			}
		}
#endif
		if (aod_dev->ac_img.up_flag) {
			panel_info("write analog clock img\n");
			ret = panel_do_aod_seqtbl_by_name(aod_dev, ANALOG_IMG_SEQ);
			if (ret < 0) {
				panel_err("failed to run sequence(%s)\n",
						ANALOG_IMG_SEQ);
				goto err;
			}
		}
		if (aod_dev->dc_img.up_flag) {
			panel_info("write digital clock img\n");
			ret = panel_do_aod_seqtbl_by_name(aod_dev, DIGITAL_IMG_SEQ);
			if (ret < 0) {
				panel_err("failed to run sequence(%s)\n",
						DIGITAL_IMG_SEQ);
				goto err;
			}
		}
	}

#if defined(SUPPORT_SELF_ICON)
	if (props->icon.en) {
		ret = panel_do_aod_seqtbl_by_name(aod_dev, CTRL_ICON_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					CTRL_ICON_SEQ);
			goto err;
		}
	}
#endif
	if (props->analog.en && aod_dev->ac_img.up_flag) {
		panel_info("analog enable\n");
		ret = panel_do_aod_seqtbl_by_name(aod_dev, ANALOG_CTRL_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					ANALOG_CTRL_SEQ);
			goto err;
		}
	}

	if (props->digital.en && aod_dev->dc_img.up_flag) {
		ret = panel_do_aod_seqtbl_by_name(aod_dev, DIGITAL_CTRL_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					DIGITAL_CTRL_SEQ);
			goto err;
		}
	}

err:
	aod_dev->reset_flag = 0;
	panel_mutex_unlock(&aod_dev->lock);

	return ret;
}

static int aod_exit_from_lpm(struct aod_dev_info *aod_dev)
{
	int ret = 0;
	struct aod_ioctl_props *props = &aod_dev->props;

	panel_mutex_lock(&aod_dev->lock);

	if (props->analog.en)
		panel_warn("analog must be disabled, but still enabled\n");

	if (props->digital.en)
		panel_warn("digital must be disabled, but still enabled\n");

	if (props->partial.scan_en || props->partial.hlpm_scan_en) {
		ret = panel_do_aod_seqtbl_by_name(aod_dev, DISABLE_PARTIAL_SCAN_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					DISABLE_PARTIAL_SCAN_SEQ);
			goto err;
		}
	}

	if (props->icon.en || props->self_grid.en) {
		ret = panel_do_aod_seqtbl_by_name(aod_dev, ICON_GRID_OFF_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					ICON_GRID_OFF_SEQ);
			goto err;
		}
	}

	if (props->self_move_en || props->icon.en) {
		ret = panel_do_aod_seqtbl_by_name(aod_dev, SELF_MOVE_OFF_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					SELF_MOVE_OFF_SEQ);
			goto err;
		}
	}

	if (check_aod_seqtbl_exist(aod_dev, EXIT_AOD_SEQ)) {
		ret = panel_do_aod_seqtbl_by_name(aod_dev, EXIT_AOD_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					EXIT_AOD_SEQ);
			goto err;
		}
	}

	if (props->self_mask_en) {
		ret = panel_do_aod_seqtbl_by_name(aod_dev, SELF_MASK_ENA_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					SELF_MASK_ENA_SEQ);
			goto err;
		}
	}

err:
	props->analog.en = 0;
	props->digital.en = 0;
	props->self_move_en = AOD_DRV_SELF_MOVE_OFF;
	props->partial.scan_en = 0;
	props->partial.hlpm_scan_en = 0;
	props->first_clk_update = 1;

	panel_mutex_unlock(&aod_dev->lock);

	return ret;
}

static int aod_doze_suspend(struct aod_dev_info *aod_dev)
{
	panel_mutex_lock(&aod_dev->lock);
	panel_info("was called\n");
	panel_mutex_unlock(&aod_dev->lock);

	return 0;
}

static int aod_power_off(struct aod_dev_info *aod_dev)
{
	struct aod_ioctl_props *props = &aod_dev->props;

	panel_mutex_lock(&aod_dev->lock);
	panel_info("was called\n");

	if (props->analog.en)
		panel_warn("analog must be disabled, but still enabled\n");

	if (props->digital.en)
		panel_warn("digital must be disabled, but still enabled\n");

	props->analog.en = 0;
	props->digital.en = 0;
	props->self_move_en = AOD_DRV_SELF_MOVE_OFF;

	/* set flag reseted */
	aod_dev->reset_flag = 1;
	props->first_clk_update = 1;
	props->self_reset_cnt = 0;
	panel_mutex_unlock(&aod_dev->lock);

	return 0;
}

static int aod_black_grid_on(struct aod_dev_info *aod_dev)
{
	int ret;

	if (!check_aod_seqtbl_exist(aod_dev, ICON_GRID_ON_SEQ)) {
		panel_dbg("seq does not exists\n");
		return 0;
	}

	ret = panel_do_aod_seqtbl_by_name(aod_dev, ICON_GRID_ON_SEQ);
	if (ret < 0) {
		panel_err("failed to run sequence(%s)\n",
				ICON_GRID_ON_SEQ);
		return ret;
	}

	panel_info("was called\n");

	return 0;
}

static int aod_black_grid_off(struct aod_dev_info *aod_dev)
{
	int ret;

	if (!check_aod_seqtbl_exist(aod_dev, ICON_GRID_OFF_SEQ)) {
		panel_dbg("seq does not exists\n");
		return 0;
	}

	ret = panel_do_aod_seqtbl_by_name(aod_dev, ICON_GRID_OFF_SEQ);
	if (ret < 0) {
		panel_err("failed to run sequence(%s)\n",
				ICON_GRID_OFF_SEQ);
		return ret;
	}

	panel_info("was called\n");

	return 0;
}

#if defined(SUPPORT_NORMAL_SELF_MOVE)
static int self_move_pattern_update(struct aod_dev_info *aod)
{
	int ret;
	struct aod_ioctl_props *props;
	struct panel_device *panel;

	if (!aod) {
		panel_err("aod is null\n");
		return -EINVAL;
	}

	props = &aod->props;
	panel = to_panel_device(aod);
	if (panel_get_cur_state(panel) == PANEL_STATE_ALPM) {
		panel_info("self move pattern ignored in LPM\n");
		return 0;
	}

	if (panel_get_cur_state(panel) != PANEL_STATE_NORMAL ||
			panel->state.disp_on != PANEL_DISPLAY_ON) {
		panel_info("self move pattern ignored in DISPLAY OFF\n");
		return 0;
	}

#ifdef CONFIG_USDM_PANEL_MULTI_RESOLUTION
	if (panel->panel_data.props.mres_updated) {
		panel_info("self move pattern ignored during mres updates\n");
		return 0;
	}
#endif

	if (props->self_move_pattern == 0) {
		ret = panel_do_aod_seqtbl_by_name(aod, DISABLE_SELF_MOVE_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					DISABLE_SELF_MOVE_SEQ);
			return ret;
		}
		panel_info("disable self move(pattern:%d)\n",
				props->self_move_pattern);
	} else {
		ret = panel_do_aod_seqtbl_by_name(aod, ENABLE_SELF_MOVE_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					ENABLE_SELF_MOVE_SEQ);
			return ret;
		}
		panel_info("enable self move(pattern:%d)\n",
				props->self_move_pattern);
	}

	return 0;
}
#endif

static struct aod_ops aod_drv_ops = {
	.init_panel = aod_init_panel,
	.enter_to_lpm = aod_enter_to_lpm,
	.exit_from_lpm = aod_exit_from_lpm,
	.doze_suspend = aod_doze_suspend,
	.power_off = aod_power_off,
	.black_grid_on = aod_black_grid_on,
	.black_grid_off = aod_black_grid_off,
#if defined(SUPPORT_NORMAL_SELF_MOVE)
	.self_move_pattern_update = self_move_pattern_update,
#endif
};

static int __seq_aod_self_move_en(struct aod_dev_info *aod, unsigned long arg)
{
	int ret;
	struct aod_ioctl_props *props;
	struct panel_device *panel;

	if (!aod) {
		panel_err("aod is null\n");
		return -EINVAL;
	}

	props = &aod->props;
	panel = to_panel_device(aod);

	panel_info("inteval : %d\n", props->cur_time.interval);
	if (panel_get_cur_state(panel) != PANEL_STATE_ALPM) {
		panel_info("self move on ignored\n");
		return -EAGAIN;
	}

	if (props->first_clk_update) {
		ret = panel_do_aod_seqtbl_by_name(aod, SET_TIME_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					SET_TIME_SEQ);
			return ret;
		}
	}

	ret = panel_do_aod_seqtbl_by_name(aod, SELF_MOVE_ON_SEQ);
	if (ret < 0) {
		panel_err("failed to run sequence(%s)\n",
				SELF_MOVE_ON_SEQ);
		return ret;
	}

	return 0;
}

static int __seq_aod_self_move_off(struct aod_dev_info *aod)
{
	int ret;
	struct panel_device *panel;

	if (!aod) {
		panel_err("aod is null\n");
		return -EINVAL;
	}

	panel = to_panel_device(aod);
	if (panel_get_cur_state(panel) != PANEL_STATE_ALPM) {
		panel_info("self move off ignored\n");
		return -EAGAIN;
	}

	ret = panel_do_aod_seqtbl_by_name(aod, SELF_MOVE_RESET_SEQ);
	if (ret < 0) {
		panel_err("failed to run sequence(%s)\n",
				SELF_MOVE_RESET_SEQ);
		return ret;
	}

	return 0;
}

static int __seq_aod_self_move_reset(struct aod_dev_info *aod, unsigned long arg)
{
	int ret;
	struct panel_device *panel;

	if (!aod) {
		panel_err("aod is null\n");
		return -EINVAL;
	}

	panel = to_panel_device(aod);
	if (panel_get_cur_state(panel) != PANEL_STATE_ALPM) {
		panel_info("self move off ignored\n");
		return -EAGAIN;
	}

	ret = panel_do_aod_seqtbl_by_name(aod, SELF_MOVE_RESET_SEQ);
	if (ret < 0) {
		panel_info("failed to seq_self_move off\n");
		return ret;
	}

	return 0;
}

#if defined(SUPPORT_SELF_ICON)
static int __get_icon_img_info(struct aod_dev_info *aod)
{
	struct seqinfo *img_seqtbl = NULL;
	struct pktinfo *img_pktinfo = NULL;

	img_seqtbl = find_aod_seq_by_name(aod, SELF_ICON_IMG_SEQ);
	if (!img_seqtbl) {
		panel_err("failed to find sequence(%s)\n",
				SELF_ICON_IMG_SEQ);
		return -EINVAL;
	}

	img_pktinfo = find_packet_suffix(img_seqtbl, "self_icon_img");
	if (!img_pktinfo) {
		panel_err("failed to get icon pktinfo\n");
		return -EINVAL;
	}
	aod->icon_img.buf = img_pktinfo->data;
	aod->icon_img.size = img_pktinfo->dlen;

	return 0;
}
#endif

static int __get_ac_img_info(struct aod_dev_info *aod)
{
	struct seqinfo *img_seqtbl = NULL;
	struct pktinfo *img_pktinfo = NULL;

	img_seqtbl = find_aod_seq_by_name(aod, ANALOG_IMG_SEQ);
	if (!img_seqtbl) {
		panel_err("failed to find sequence(%s)\n",
				ANALOG_IMG_SEQ);
		return -EINVAL;
	}

	img_pktinfo = find_packet_suffix(img_seqtbl, "analog_img");
	if (!img_pktinfo) {
		panel_err("failed to get analog img pktinfo\n");
		return -EINVAL;
	}

	aod->ac_img.buf = get_pktinfo_txbuf(img_pktinfo);
	aod->ac_img.size = img_pktinfo->dlen;

	return 0;
}

static int __get_dc_img_info(struct aod_dev_info *aod)
{
	struct seqinfo *img_seqtbl = NULL;
	struct pktinfo *img_pktinfo = NULL;

	img_seqtbl = find_aod_seq_by_name(aod, DIGITAL_IMG_SEQ);
	if (!img_seqtbl) {
		panel_err("failed to find sequence(%s)\n",
				DIGITAL_IMG_SEQ);
		return -EINVAL;
	}

	img_pktinfo = find_packet_suffix(img_seqtbl, "digital_img");
	if (!img_pktinfo) {
		panel_err("failed to get analog img pktinfo\n");
		return -EINVAL;
	}

	aod->dc_img.buf = get_pktinfo_txbuf(img_pktinfo);
	aod->dc_img.size = img_pktinfo->dlen;

	return 0;
}

static int aod_drv_fops_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct miscdevice *dev = file->private_data;
	struct aod_dev_info *aod = container_of(dev, struct aod_dev_info, dev);
	struct panel_device *panel = to_panel_device(aod);

	panel_wake_lock(panel, WAKE_TIMEOUT_MSEC);
	panel_mutex_lock(&panel->io_lock);
	panel_mutex_lock(&aod->lock);

	panel_info("was called\n");

	file->private_data = aod;

#if defined(SUPPORT_SELF_ICON)
	ret = __get_icon_img_info(aod);
	if (ret)
		panel_err("failed to get icon img info\n");
#endif
	ret = __get_ac_img_info(aod);
	if (ret)
		panel_err("failed to get ac img info\n");

	ret = __get_dc_img_info(aod);
	if (ret)
		panel_err("failed to get dc img info\n");

	panel_mutex_unlock(&aod->lock);
	panel_mutex_unlock(&panel->io_lock);

	return ret;
}

#if defined(SUPPORT_SELF_GRID)
static int __aod_ioctl_set_grid(struct aod_dev_info *aod, unsigned long arg)
{
	struct self_grid_info grid_info;
	struct aod_ioctl_props *props = &aod->props;

	if (copy_from_user(&grid_info, (struct self_grid_info __user *)arg,
			sizeof(struct self_grid_info))) {
		panel_err("failed to get user's icon info\n");
		return -EINVAL;
	}

	panel_info("%d:%d:%d:%d:%d\n",
		grid_info.en, grid_info.s_pos_x, grid_info.s_pos_y,
		grid_info.e_pos_x, grid_info.e_pos_y);

	memcpy(&props->self_grid, &grid_info, sizeof(struct self_grid_info));

	return 0;
}
#endif

#if defined(SUPPORT_SELF_ICON)
static int __aod_ioctl_set_icon(struct aod_dev_info *aod, unsigned long arg)
{
	int ret = 0;
	struct self_icon_info icon_info;
	struct aod_ioctl_props *props = &aod->props;
	char *seqname;

	if (copy_from_user(&icon_info, (struct self_icon_info __user *)arg,
			sizeof(struct self_icon_info))) {
		panel_err("failed to get user's icon info\n");
		return -EINVAL;
	}

	panel_info("%d:%d:%d:%d:%d\n", icon_info.en,
		icon_info.height, icon_info.width, icon_info.pos_x, icon_info.pos_y);

	memcpy(&props->icon, &icon_info, sizeof(struct self_icon_info));

	seqname = (props->icon.en) ?
		CTRL_ICON_SEQ : DISABLE_ICON_SEQ;
	ret = panel_do_aod_seqtbl_by_name(aod, seqname);
	if (ret < 0) {
		panel_err("failed to run sequence(%s)\n", seqname);
		return ret;
	}

	return 0;
}
#endif

static int __aod_ioctl_set_time(struct aod_dev_info *aod, unsigned long arg)
{
	int ret;
	struct aod_cur_time cur_time;
	struct aod_ioctl_props *props = &aod->props;
	struct panel_device *panel = to_panel_device(aod);

	if (copy_from_user(&cur_time, (struct aod_cur_time __user *)arg,
			sizeof(struct aod_cur_time))) {
		panel_err("failed to get user's curtime\n");
		return -EINVAL;
	}

	panel_info("%d:%d:%d:%d:%d\n", cur_time.cur_h,
		cur_time.cur_m, cur_time.cur_s, cur_time.cur_ms, cur_time.interval);

	memcpy(&props->cur_time, &cur_time, sizeof(struct aod_cur_time));

	if (panel_get_cur_state(panel) != PANEL_STATE_ALPM) {
		panel_info("set time ignored\n");
		return -EAGAIN;
	}

	ret = panel_do_aod_seqtbl_by_name(aod, SET_TIME_SEQ);
	if (ret < 0) {
		panel_err("failed to run sequence(%s)\n",
				SET_TIME_SEQ);
		return ret;
	}

	props->first_clk_update = 0;

	return ret;
}


static int __aod_ioctl_set_analog_clk(struct aod_dev_info *aod, unsigned long arg)
{
	int ret = 0;
	struct analog_clk_info clk;
	struct aod_ioctl_props *props = &aod->props;
	struct panel_device *panel = to_panel_device(aod);

	if (copy_from_user(&clk, (struct analog_clk_info __user *)arg,
		sizeof(struct analog_clk_info))) {
		panel_err("failed to get user's info\n");
		return -EINVAL;
	}

	panel_info("en:%d,pos:%d,%d rot:%d\n",
		clk.en, clk.pos_x, clk.pos_y, clk.rotate);
	memcpy(&props->analog, &clk, sizeof(struct analog_clk_info));

	if (panel_get_cur_state(panel) != PANEL_STATE_ALPM) {
		panel_info("set time ignored\n");
		return -EAGAIN;
	}

	ret = panel_do_aod_seqtbl_by_name(aod, ANALOG_CTRL_SEQ);
	if (ret < 0) {
		panel_err("failed to run sequence(%s)\n",
				ANALOG_CTRL_SEQ);
		return ret;
	}

	if (props->analog.en && props->self_mask_en) {
		ret = panel_do_aod_seqtbl_by_name(aod, SELF_MOVE_ON_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					SELF_MOVE_ON_SEQ);
			return ret;
		}
	}

	/* In case of Big Jump, (AOD App disable analog clock first before Big jump)
		disable analog clock's compensation (req from. D_Lab) */
	if (props->analog.en == 0)
		props->first_clk_update = 1;

	if (clk.en)
		props->prev_rotate = clk.rotate;

	return 0;
}

static int __aod_ioctl_set_digital_clk(struct aod_dev_info *aod, unsigned long arg)
{
	int ret = 0;
	struct digital_clk_info clk;
	struct aod_ioctl_props *props = &aod->props;
	struct panel_device *panel = to_panel_device(aod);

	if (copy_from_user(&clk, (struct digital_clk_info __user *)arg,
		sizeof(struct digital_clk_info))) {
		panel_err("failed to get user's info\n");
		return -EINVAL;
	}

	panel_info("en:%d,pos:%d,%d\n",
		clk.en, clk.pos1_x, clk.pos1_y);
	panel_info("width:%d, height:%d\n",
		clk.img_height, clk.img_width);
	panel_info("hh_en:%d, mm_en:%d\n",
		clk.en_hh, clk.en_mm);
	panel_info("unicode attr : %x\n",
		clk.unicode_attr);

	memcpy(&props->digital, &clk, sizeof(struct digital_clk_info));

	if (panel_get_cur_state(panel) != PANEL_STATE_ALPM) {
		panel_info("set time ignored\n");
		return -EAGAIN;
	}

	ret = panel_do_aod_seqtbl_by_name(aod, DIGITAL_CTRL_SEQ);
	if (ret < 0) {
		panel_err("failed to run sequence(%s)\n",
				DIGITAL_CTRL_SEQ);
		return ret;
	}

	if (props->digital.en && props->self_mask_en) {
		ret = panel_do_aod_seqtbl_by_name(aod, SELF_MOVE_ON_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					SELF_MOVE_ON_SEQ);
			return ret;
		}
	}

	return ret;
}

static int __aod_ioctl_set_partial_scan(struct aod_dev_info *aod, unsigned long arg)
{
	int ret;
	struct partial_scan_info scan_info;
	struct aod_ioctl_props *props = &aod->props;
	struct panel_device *panel = to_panel_device(aod);

	if (copy_from_user(&scan_info, (struct partial_scan_info __user *)arg,
		sizeof(struct partial_scan_info))) {
		panel_err("failed to get user's info\n");
		return -EINVAL;
	}

	memcpy(&props->partial, &scan_info, sizeof(struct partial_scan_info));

	if (panel_get_cur_state(panel) != PANEL_STATE_ALPM) {
		panel_info("Not AOD State.. Partial ignored\n");
		return -EAGAIN;
	}

	panel_info("hlpm enable : %d, partial_scan : %d\n",
			scan_info.hlpm_scan_en, scan_info.scan_en);

	if (scan_info.hlpm_scan_en) {
		panel_info("hlpm area1: 0 ~ %dline, %s",
			scan_info.hlpm_area_1,
			(scan_info.hlpm_mode_sel & 0x01) ? "ALPM" : "HLPM");
		panel_info("hlpm area2: %d ~ %dline, %s",
			scan_info.hlpm_area_1, scan_info.hlpm_area_2,
			(scan_info.hlpm_mode_sel & 0x02) ? "ALPM" : "HLPM");
		panel_info("hlpm area3: %d ~ %dline, %s",
			scan_info.hlpm_area_2, scan_info.hlpm_area_3,
			(scan_info.hlpm_mode_sel & 0x04) ? "ALPM" : "HLPM");
		panel_info("hlpm area4: %d ~ %dline, %s",
			scan_info.hlpm_area_3, scan_info.hlpm_area_4,
			(scan_info.hlpm_mode_sel & 0x08) ? "ALPM" : "HLPM");
		panel_info("hlpm area5: %d ~ end, %s",
			scan_info.hlpm_area_4,
			(scan_info.hlpm_mode_sel & 0x10) ? "ALPM" : "HLPM");
	}

	if (scan_info.scan_en)
		panel_info("scan line: %d ~ %dline",
			scan_info.scan_sl, scan_info.scan_el);

	if (scan_info.scan_en || scan_info.hlpm_scan_en) {
		ret = panel_do_aod_seqtbl_by_name(aod, ENABLE_PARTIAL_SCAN_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					ENABLE_PARTIAL_SCAN_SEQ);
			return ret;
		}
	} else {
		ret = panel_do_aod_seqtbl_by_name(aod, DISABLE_PARTIAL_SCAN_SEQ);
		if (ret < 0) {
			panel_err("failed to run sequence(%s)\n",
					DISABLE_PARTIAL_SCAN_SEQ);
			return ret;
		}
	}

	return 0;
}

static int __aod_ioctl_set_scenario(struct aod_dev_info *aod, unsigned long arg)
{
	struct aod_scenario_info info;
	struct panel_info *panel_data;
	struct panel_device *panel;

	if (!aod) {
		panel_err("aod is null\n");
		return -EINVAL;
	}

	panel = to_panel_device(aod);
	panel_data = &panel->panel_data;

	if (copy_from_user(&info, (int __user *)arg,
				sizeof(struct aod_scenario_info))) {
		panel_err("failed to copy user's scenario\n");
		return -ENOMEM;
	}
	panel_info("aod scenario : %d\n", info.scenario);

	if (info.scenario >= AOD_SCENARIO_MAX) {
		panel_err("invalid aod scenario\n");
		return -EINVAL;
	}

	if (info.scenario == AOD_SCENARIO_IMAGE)
		panel_data->props.lpm_fps = LPM_LFD_30HZ;
	else
		panel_data->props.lpm_fps = LPM_LFD_1HZ;

	return 0;
}

static long aod_drv_fops_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct aod_dev_info *aod = file->private_data;
	struct aod_ioctl_props *props = &aod->props;
	struct panel_device *panel = to_panel_device(aod);

	panel_mutex_lock(&panel->io_lock);
	panel_mutex_lock(&aod->lock);

	switch (cmd) {
	case IOCTL_SELF_MOVE_EN:
		panel_info("IOCTL_SELF_MOVE_EN\n");
		props->self_move_en = AOD_DRV_SELF_MOVE_ON;
		ret = __seq_aod_self_move_en(aod, arg);
		if (ret) {
			panel_err("failed to seq self move en\n");
			goto exit_ioctl;
		}
		break;
	case IOCTL_SELF_MOVE_OFF:
		panel_info("IOCTL_SELF_MOVE_OFF\n");
		if (props->self_move_en != AOD_DRV_SELF_MOVE_OFF) {
			props->self_move_en = AOD_DRV_SELF_MOVE_OFF;
			ret = __seq_aod_self_move_off(aod);
			if (ret) {
				panel_err("failed to seq self move off\n");
				goto exit_ioctl;
			}
		}
		break;

	case IOCTL_SELF_MOVE_RESET:
		panel_info("IOCTL_SELF_MOVE_RESET\n");
		ret = __seq_aod_self_move_reset(aod, arg);
		if (ret) {
			panel_err("failed to seq self move reset\n");
			goto exit_ioctl;
		}
		break;

	case IOCTL_SET_TIME:
		panel_info("IOCTL_SET_TIME\n");
		ret = __aod_ioctl_set_time(aod, arg);
		break;

#if defined(SUPPORT_SELF_ICON)
	case IOCTL_SET_ICON:
		panel_info("IOCTL_SET_ICON\n");
		ret = __aod_ioctl_set_icon(aod, arg);
		break;
#endif

	case IOCTL_SET_GRID:
#if defined(SUPPORT_SELF_GRID)
		panel_info("IOCTL_SET_GRID\n");
		ret = __aod_ioctl_set_grid(aod, arg);
#else
		panel_info("IOCTL_SET_GRID: driver does not support grid\n");
		ret = -EINVAL;
#endif
		break;

	case IOCTL_SET_ANALOG_CLK:
		panel_info("IOCTL_SET_ANALOG_CLK\n");
		ret = __aod_ioctl_set_analog_clk(aod, arg);
		break;

	case IOCTL_SET_DIGITAL_CLK:
		panel_info("IOCTL_SET_DIGITAL_CLK\n");
		ret = __aod_ioctl_set_digital_clk(aod, arg);
		break;

	case IOCTL_SET_PARTIAL_HLPM_SCAN:
		panel_info("IOCTL_SET_PARTIAL_HLPM_MAPTBL_SCAN\n");
		ret = __aod_ioctl_set_partial_scan(aod, arg);
		break;

	case IOCTL_SET_SCENARIO:
		panel_info("IOCTL_SET_SCENARIO\n");
		ret = __aod_ioctl_set_scenario(aod, arg);
		break;

	default:
		panel_err("invalid ioctl cmd: %x\n", cmd);
		ret = -EINVAL;
		break;
	}

exit_ioctl:
	panel_mutex_unlock(&aod->lock);
	panel_mutex_unlock(&panel->io_lock);
	return ret;
}


static ssize_t aod_drv_fops_write(struct file *file, const char __user *buf,
		  size_t count, loff_t *ppos)
{
	size_t size;
	u8 header[IMG_HD_SZ];
	struct img_info *img_info;
	struct aod_dev_info *aod = file->private_data;
	struct panel_device *panel = to_panel_device(aod);

	panel_wake_lock(panel, WAKE_TIMEOUT_MSEC);
	panel_mutex_lock(&panel->io_lock);
	panel_mutex_lock(&aod->lock);

	if (copy_from_user(header, buf, IMG_HD_SZ)) {
		panel_err("failed to get user's header\n");
		goto exit_write;
	}
	panel_info("header:[0]: %c, [1]: %c, count: %ld\n",
			header[0], header[1], count);

	if (!strncmp(header, HEADER_SELF_ICON, IMG_HD_SZ)) {
		img_info = &aod->icon_img;
	} else if (!strncmp(header, HEADER_ANALOG_CLK, IMG_HD_SZ)) {
		img_info = &aod->ac_img;
		/* to make mutually exclusion */
		aod->dc_img.up_flag = 0;
	} else if (!strncmp(header, HEADER_DIGITAL_CLK, IMG_HD_SZ)) {
		img_info = &aod->dc_img;
		/* to make mutually exclusion */
		aod->ac_img.up_flag = 0;
	} else {
		panel_info("invalid header : %c%c\n",
			header[0], header[1]);
		goto exit_write;
	}

	if (!img_info->buf) {
		panel_info("img buf is null\n");
		goto exit_write;
	}

	size = count - IMG_HD_SZ;
	if (size > img_info->size) {
		panel_info("exceed buffer size (%d:%d)\n",
			(u32)size, img_info->size);
		goto exit_write;
	}

	if (copy_from_user(img_info->buf, buf + IMG_HD_SZ, size)) {
		panel_err("failed to copy img body\n");
		goto exit_write;
	}

	img_info->up_flag = 1;

exit_write:
	panel_mutex_unlock(&aod->lock);
	panel_mutex_unlock(&panel->io_lock);
	panel_wake_unlock(panel);

	return count;
}

static int aod_drv_fops_release(struct inode *inode, struct file *file)
{
	struct aod_dev_info *aod = file->private_data;
	struct panel_device *panel = to_panel_device(aod);

	panel_wake_unlock(panel);

	return 0;
}

static const struct file_operations aod_drv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = aod_drv_fops_ioctl,
	.open = aod_drv_fops_open,
	.release = aod_drv_fops_release,
	.write = aod_drv_fops_write,
};

int aod_drv_prepare(struct panel_device *panel,
		struct aod_tune *aod_tune)
{
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;
	int ret;

	if (!panel) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!aod_tune) {
		panel_err("aod_tune is null\n");
		return -EINVAL;
	}

	aod = &panel->aod;
	props = &aod->props;

	ret = panel_add_command_from_initdata_maptbl(aod_tune->maptbl,
			aod_tune->nr_maptbl, &panel->command_initdata_list);
	if (ret < 0) {
		panel_err("failed to panel_add_command_from_initdata_maptbl\n");
		return ret;
	}

	ret = panel_add_command_from_initdata_seqtbl(aod_tune->seqtbl,
			aod_tune->nr_seqtbl, &panel->command_initdata_list);
	if (ret < 0) {
		panel_err("failed to panel_add_command_from_initdata_seqtbl\n");
		return ret;
	}

	props->self_mask_en = aod_tune->self_mask_en;
	props->self_reset_cnt = 0;
	if (aod_tune->self_mask_crc_len > 0) {
		props->self_mask_crc =
			kmemdup(aod_tune->self_mask_crc,
					aod_tune->self_mask_crc_len, GFP_KERNEL);
		props->self_mask_crc_len =
			aod_tune->self_mask_crc_len;
	}

	return 0;
}

int aod_drv_unprepare(struct panel_device *panel)
{
	struct aod_dev_info *aod = &panel->aod;
	struct aod_ioctl_props *props = &aod->props;

	kfree(props->self_mask_crc);
	props->self_mask_crc = NULL;
	props->self_mask_crc_len = 0;

	return 0;
}

int aod_drv_probe(struct panel_device *panel, struct aod_tune *aod_tune)
{
	int ret;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!panel) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!aod_tune) {
		panel_err("aod_tune is null\n");
		return -EINVAL;
	}

	aod = &panel->aod;
	props = &aod->props;

	aod->ops = &aod_drv_ops;
	aod->custom_ops = &aod_tune->custom_ops;

	aod->reset_flag = 1;
	props->first_clk_update = 1;

	panel_mutex_init(panel, &aod->lock);

	props->cur_time.cur_h = 6;
	props->cur_time.cur_m = 30;
	props->cur_time.cur_ms = 0;
	props->cur_time.interval = ALG_INTERVAL_1000m;
	props->cur_time.disp_24h = 1;

	aod->dev.minor = MISC_DYNAMIC_MINOR;
	aod->dev.fops = &aod_drv_fops;
	aod->dev.name = AOD_DEV_NAME;
	ret = misc_register(&aod->dev);
	if (ret) {
		panel_err("failed to register for aod_dev\n");
		return ret;
	}

	return 0;
}

int aod_drv_remove(struct panel_device *panel)
{
	return 0;
}
