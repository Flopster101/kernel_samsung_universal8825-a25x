/*
 * Samsung Exynos SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/i2c.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-hw.h"
#include "is-video.h"
#include "is-dt.h"
#include "is-debug.h"
#include "is-dvfs.h"
#include "is-groupmgr.h"

#include "is-device-sensor.h"
#include "is-interface-wrap.h"
#include "is-device-sensor-peri.h"
#include "is-vender.h"
#include "is-vender-specific.h"
#include "is-votfmgr.h"
#include "is-device-camif-dma.h"
#include "pablo-obte.h"

#if defined(CSIS_PDP_VOTF_GLOBAL_WA)
#define INSTANT_OFF_CNT	(1)
#else
#define INSTANT_OFF_CNT	(0)
#endif

extern const struct is_subdev_ops is_subdev_sensor_ops;
extern const struct is_subdev_ops is_subdev_ssvc_ops;

int is_sensor_runtime_suspend(struct device *dev);
int is_sensor_runtime_resume(struct device *dev);
static int is_sensor_shot(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *check_frame);

static int is_sensor_back_stop(void *device, struct is_queue *iq);

int is_search_sensor_module_with_position(struct is_device_sensor *device,
	u32 position, struct is_module_enum **module)
{
	int ret = 0;
	u32 mindex, mmax;
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *priv = core->vender.private_data;
	struct is_module_enum *module_enum;
	const char *sensor_name;
	u32 sensor_id;

	module_enum = device->module_enum;
	*module = NULL;

	if (position < 0 || position >= SENSOR_POSITION_MAX) {
		err("%s sensor position [%d] is invalid", __func__, position);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_id = priv->sensor_id[position];
	sensor_name = priv->sensor_name[position];

	mmax = atomic_read(&device->module_count);
	for (mindex = 0; mindex < mmax; mindex++) {
		*module = &module_enum[mindex];
		if (!(*module)) {
			merr("module is not probed, mindex = %d", device, mindex);
			ret = -EINVAL;
			goto p_err;
		}

		if ((*module)->position != position)
			continue;

		if ((*module)->sensor_name && sensor_name) {
			if (!strcmp((*module)->sensor_name, sensor_name))
				break;
		}

		if ((*module)->sensor_id == sensor_id)
			break;
	}

	if (mindex >= mmax) {
		*module = NULL;
		mdbgs_sensor(1, "module(%d) is not found, position(%d)", device, sensor_id, position);
		ret = -EINVAL;
	}

p_err:
	return ret;
}

int is_sensor_g_module(struct is_device_sensor *device,
	struct is_module_enum **module)
{
	int ret = 0;

	FIMC_BUG(!device);

	if (!device->subdev_module) {
		merr("sub module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	*module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (!*module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!(*module)->pdata) {
		merr("module->pdata is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int is_sensor_deinit_module(struct is_module_enum *module)
{
	int ret = 0;
	int i;
	struct is_core *core;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg cfg;

	FIMC_BUG(!module->subdev);

	sensor = v4l2_get_subdev_hostdata(module->subdev);

	core = sensor->private_data;

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		if (!test_bit(IS_SENSOR_OPEN, &core->sensor[i].state))
			continue;

		if (!test_bit(IS_SENSOR_S_INPUT, &core->sensor[i].state))
			continue;

		if (core->sensor[i].position == module->position) {
			sensor = &core->sensor[i];
			break;
		}
	}

	if (i >= IS_SENSOR_COUNT) {
		warn("There is no valid sensor");
		goto p_err;
	}

	if (!sensor->ischain) {
		warn("sensor is not initialized");
		goto p_err;
	}

	if (!test_bit(IS_ISCHAIN_INIT, &sensor->ischain->state)) {
		warn("ischain is not initialized");
		goto p_err;
	}

	minfo("Deinit sensor module(%s)\n", sensor, module->sensor_name);

	cfg.mode = SENSOR_MODE_DEINIT;
	ret = is_itf_sensor_mode_wrap(sensor->ischain, &cfg);
	if (ret)
		goto p_err;

p_err:
	return ret;
}

static int is_sensor_g_device(struct platform_device *pdev,
	struct is_device_sensor **device)
{
	int ret = 0;

	FIMC_BUG(!pdev);

	*device = (struct is_device_sensor *)platform_get_drvdata(pdev);
	if (!*device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!(*device)->pdata) {
		merr("device->pdata is NULL", (*device));
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static void is_sensor_s_additional_info(struct is_sensor_cfg *cfg, u32 stat_vc)
{
	u32 img_h = cfg->input[CSI_VIRTUAL_CH_0].height;
	u32 pd_h = cfg->input[stat_vc].height;

	if (cfg->pd_mode == PD_MSPD_TAIL || cfg->pd_mode == PD_MOD3) {
		if (pd_h && (img_h / pd_h) == 1)
			cfg->img_pd_ratio = IS_IMG_PD_RATIO_1_1;
		else
			cfg->img_pd_ratio = 0;
	}
}

struct is_sensor_cfg *is_sensor_g_mode(struct is_device_sensor *device)
{
	struct is_sensor_cfg *select = NULL;
	long approximate_value = LONG_MAX;
	struct is_sensor_cfg *cfg_table;
	u32 cfgs, i, stat_vc;
	u32 width, height, framerate, ex_mode;
	u32 ex_mode_extra;
	u32 target_hw_format = HW_FORMAT_RAW10;
	struct is_module_enum *module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	int deviation;
#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
	struct is_device_sensor_peri *sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	struct is_cis *cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);
#endif

	FIMC_BUG_NULL(!device);

	cfg_table = module->cfg;
	cfgs = module->cfgs;
	stat_vc = module->stat_vc;
	width = device->image.window.width;
	height = device->image.window.height;
	framerate = device->image.framerate;
	ex_mode = device->ex_mode;
	ex_mode_extra = device->ex_mode_extra;

	if (device->ex_mode_format == EX_FORMAT_12BIT)
		target_hw_format = HW_FORMAT_RAW12;

	minfo("try to find sensor mode(%dx%d@%d) ex_mode(%d:%d) hw_format(%x)", device,
		width,
		height,
		framerate,
		ex_mode,
		ex_mode_extra,
		target_hw_format);


#ifdef USE_EX_MODE_EXTRA
	/* find sensor mode first by w/h, fps range and ex_mode_extra */
	if (ex_mode_extra > 0) {
		for (i = 0; i < cfgs; i++) {
			if ((cfg_table[i].width == width) && (cfg_table[i].height == height)
				&& (cfg_table[i].ex_mode_extra == ex_mode_extra)
				&& (cfg_table[i].ex_mode == ex_mode)
				&& (target_hw_format ? (cfg_table[i].output[CSI_VIRTUAL_CH_0].hwformat == target_hw_format) : true)) {
				deviation = cfg_table[i].framerate - framerate;
				if (deviation == 0) {
					/* You don't need to find another sensor mode */
					select = &cfg_table[i];
					break;
				} else if ((deviation > 0) && approximate_value > abs(deviation)) {
					/* try to find framerate smaller than previous */
					approximate_value = abs(deviation);
					select = &cfg_table[i];
				}
			}
		}
	}
#endif

	if (!select) {
		/* find sensor mode by w/h and fps range */
		for (i = 0; i < cfgs; i++) {
			if ((cfg_table[i].width == width) && (cfg_table[i].height == height)
				&& (cfg_table[i].ex_mode_extra == EX_EXTRA_NONE)
				&& (cfg_table[i].ex_mode == ex_mode)
				&& (target_hw_format ? (cfg_table[i].output[CSI_VIRTUAL_CH_0].hwformat == target_hw_format) : true)) {
				deviation = cfg_table[i].framerate - framerate;
				if (deviation == 0) {
					/* You don't need to find another sensor mode */
					select = &cfg_table[i];
					break;
				} else if ((deviation > 0) && approximate_value > abs(deviation)) {
					/* try to find framerate smaller than previous */
					approximate_value = abs(deviation);
					select = &cfg_table[i];
				}
			}
		}

		if (!select) {
			merr("sensor mode(%dx%d@%dfps, ex_mode:%d,%d) is not found",
				device, width, height, framerate, ex_mode, ex_mode_extra);
			goto p_err;
		}
	}

#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
	if ((device->position == SENSOR_POSITION_REAR2) || /* tele1 */
	    (device->position == SENSOR_POSITION_REAR4)) { /* tele2 */
		if (sensor_peri->subdev_cis) {
			CALL_CISOPS(cis, cis_update_pdaf_tail_size, sensor_peri->subdev_cis, select);
			CALL_CISOPS(cis, cis_set_crop_region, sensor_peri->subdev_cis, select->mode);
		}
		/* VC_TAILPDAF dma buffer should match 16 byte alignment */
		if (select->output[CSI_VIRTUAL_CH_1].type == VC_TAILPDAF) {
			select->output[CSI_VIRTUAL_CH_1].width
				= ALIGN(select->output[CSI_VIRTUAL_CH_1].width, 8);

			minfo("VC_TAILPDAF output aligned to (%d x %d)\n", device,
				select->output[CSI_VIRTUAL_CH_1].width,
				select->output[CSI_VIRTUAL_CH_1].height);
		}
	}
#endif

	is_sensor_s_additional_info(select, stat_vc);

	minfo("sensor mode(%dx%d@%d) ex_mode(%d) = %d (lane:%d)\n", device,
		select->width,
		select->height,
		select->framerate,
		(uint32_t)(select->ex_mode),
		select->mode,
		(select->lanes + 1));

p_err:
	return select;
}

int is_sensor_mclk_on(struct is_device_sensor *device, u32 scenario, u32 channel, u32 freq)
{
	int ret = 0;
	struct exynos_platform_is_sensor *pdata;

	FIMC_BUG(!device);
	FIMC_BUG(!device->pdev);
	FIMC_BUG(!device->pdata);
	FIMC_BUG(!device->private_data);

	pdata = device->pdata;

	if (scenario == SENSOR_SCENARIO_STANDBY) {
		minfo("%s: skip standby mode", device, __func__);
		goto p_err;
	}

	if (test_and_set_bit(IS_SENSOR_MCLK_ON, &device->state)) {
		minfo("%s : already clk on", device, __func__);
		goto p_err;
	}

	if (!pdata->mclk_on) {
		merr("mclk_on is NULL", device);
		ret = -EINVAL;
		clear_bit(IS_SENSOR_MCLK_ON, &device->state);
		goto p_err;
	}

	ret = pdata->mclk_on(&device->pdev->dev, scenario, channel, freq);
	if (ret) {
		merr("mclk_on is fail(%d)", device, ret);
		clear_bit(IS_SENSOR_MCLK_ON, &device->state);
		goto p_err;
	}

	set_bit(IS_SENSOR_MCLK_ON, &device->state);

p_err:
	return ret;
}

int is_sensor_mclk_off(struct is_device_sensor *device, u32 scenario, u32 channel)
{
	int ret = 0;
	struct exynos_platform_is_sensor *pdata;

	FIMC_BUG(!device);
	FIMC_BUG(!device->pdev);
	FIMC_BUG(!device->pdata);
	FIMC_BUG(!device->private_data);

	pdata = device->pdata;

	if (!test_and_clear_bit(IS_SENSOR_MCLK_ON, &device->state)) {
		minfo("%s : already clk off", device, __func__);
		goto p_err;
	}

	if (!pdata->mclk_off) {
		merr("mclk_off is NULL", device);
		ret = -EINVAL;
		set_bit(IS_SENSOR_MCLK_ON, &device->state);
		goto p_err;
	}

	ret = pdata->mclk_off(&device->pdev->dev, scenario, channel);
	if (ret) {
		merr("mclk_off is fail(%d)", device, ret);
		set_bit(IS_SENSOR_MCLK_ON, &device->state);
		goto p_err;
	}

	clear_bit(IS_SENSOR_MCLK_ON, &device->state);

p_err:
	return ret;
}

static int is_sensor_iclk_on(struct is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_is_sensor *pdata;
	struct is_core *core;

	FIMC_BUG(!device);
	FIMC_BUG(!device->pdev);
	FIMC_BUG(!device->pdata);
	FIMC_BUG(!device->private_data);

	core = device->private_data;
	pdata = device->pdata;

	if (test_bit(IS_SENSOR_ICLK_ON, &device->state)) {
		merr("%s : already clk on", device, __func__);
		goto p_err;
	}

	if (!pdata->iclk_cfg) {
		merr("iclk_cfg is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!pdata->iclk_on) {
		merr("iclk_on is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_cfg(&core->pdev->dev, pdata->scenario, pdata->csi_ch);
	if (ret) {
		merr("iclk_cfg is fail(%d)", device, ret);
		goto p_err;
	}

	ret = pdata->iclk_on(&core->pdev->dev, pdata->scenario, pdata->csi_ch);
	if (ret) {
		merr("iclk_on is fail(%d)", device, ret);
		goto p_err;
	}

	set_bit(IS_SENSOR_ICLK_ON, &device->state);

p_err:
	return ret;
}

int is_sensor_iclk_off(struct is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_is_sensor *pdata;
	struct is_core *core;

	FIMC_BUG(!device);
	FIMC_BUG(!device->pdev);
	FIMC_BUG(!device->pdata);
	FIMC_BUG(!device->private_data);

	core = device->private_data;
	pdata = device->pdata;

	if (!test_bit(IS_SENSOR_ICLK_ON, &device->state)) {
		merr("%s : already clk off", device, __func__);
		goto p_err;
	}

	if (!pdata->iclk_off) {
		merr("iclk_off is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_off(&core->pdev->dev, pdata->scenario, pdata->csi_ch);
	if (ret) {
		merr("iclk_off is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(IS_SENSOR_ICLK_ON, &device->state);

p_err:
	return ret;
}

int is_sensor_gpio_on(struct is_device_sensor *device)
{
	int ret = 0;
	u32 scenario, gpio_scenario;
	struct is_core *core;
	struct is_module_enum *module;
	struct is_vender *vender;

	FIMC_BUG(!device);
	FIMC_BUG(!device->pdev);
	FIMC_BUG(!device->pdata);
	FIMC_BUG(!device->private_data);

	module = NULL;
	gpio_scenario = GPIO_SCENARIO_ON;
	core = device->private_data;
	scenario = device->pdata->scenario;
	vender = &core->vender;

	if (scenario == SENSOR_SCENARIO_STANDBY) {
		minfo("%s: skip standby mode", device, __func__);
		goto p_err;
	}

	if (test_bit(IS_SENSOR_GPIO_ON, &device->state)) {
		merr("%s : already gpio on", device, __func__);
		goto p_err;
	}

	ret = is_sensor_g_module(device, &module);
	if (ret) {
		merr("is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_vender_module_sel(vender, module);
	if (ret) {
		merr("is_vender_module_sel is fail(%d)", device, ret);
		goto p_err;
	}

	if (!test_and_set_bit(IS_MODULE_GPIO_ON, &module->state)) {
		struct exynos_platform_is_module *pdata;

		ret = is_vender_sensor_gpio_on_sel(vender,
			scenario, &gpio_scenario, module);
		if (ret) {
			clear_bit(IS_MODULE_GPIO_ON, &module->state);
			merr("is_vender_sensor_gpio_on_sel is fail(%d)",
				device, ret);
			goto p_err;
		}

		pdata = module->pdata;
		if (!pdata) {
			clear_bit(IS_MODULE_GPIO_ON, &module->state);
			merr("pdata is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (!pdata->gpio_cfg) {
			clear_bit(IS_MODULE_GPIO_ON, &module->state);
			merr("gpio_cfg is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		ret = pdata->gpio_cfg(module, scenario, gpio_scenario);
		if (ret) {
			clear_bit(IS_MODULE_GPIO_ON, &module->state);
			merr("gpio_cfg is fail(%d)", device, ret);
			goto p_err;
		}

		ret = is_vender_sensor_gpio_on(vender,
			scenario, gpio_scenario, module);
		if (ret) {
			merr("is_vender_sensor_gpio_on is fail(%d)", device, ret);
			goto p_err;
		}
	}

	set_bit(IS_SENSOR_GPIO_ON, &device->state);

p_err:
	return ret;
}

int is_sensor_gpio_off(struct is_device_sensor *device)
{
	int ret = 0;
	u32 scenario, gpio_scenario;
	struct is_core *core;
	struct is_module_enum *module;
	struct is_vender *vender;

	FIMC_BUG(!device);
	FIMC_BUG(!device->pdev);
	FIMC_BUG(!device->pdata);
	FIMC_BUG(!device->private_data);

	module = NULL;
	gpio_scenario = GPIO_SCENARIO_OFF;
	core = device->private_data;
	scenario = device->pdata->scenario;
	vender = &core->vender;

	if (!test_bit(IS_SENSOR_GPIO_ON, &device->state)) {
		merr("%s : already gpio off", device, __func__);
		goto p_err;
	}

	ret = is_sensor_g_module(device, &module);
	if (ret) {
		merr("is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	if (test_and_clear_bit(IS_MODULE_GPIO_ON, &module->state)) {
		struct exynos_platform_is_module *pdata;

		ret = is_vender_sensor_gpio_off_sel(vender,
			scenario, &gpio_scenario, module);
		if (ret) {
			set_bit(IS_MODULE_GPIO_ON, &module->state);
			merr("is_vender_sensor_gpio_off_sel is fail(%d)",
				device, ret);
			goto p_err;
		}

		pdata = module->pdata;
		if (!pdata) {
			set_bit(IS_MODULE_GPIO_ON, &module->state);
			merr("pdata is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (!pdata->gpio_cfg) {
			set_bit(IS_MODULE_GPIO_ON, &module->state);
			merr("gpio_cfg is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		ret = pdata->gpio_cfg(module, scenario, gpio_scenario);
		if (ret) {
			set_bit(IS_MODULE_GPIO_ON, &module->state);
			merr("gpio_cfg is fail(%d)", device, ret);
			goto p_err;
		}

		ret = is_vender_sensor_gpio_off(vender, scenario, gpio_scenario, module);
		if (ret) {
			merr("is_vender_sensor_gpio_off is fail(%d)", device, ret);
			goto p_err;
		}
	}

	clear_bit(IS_SENSOR_GPIO_ON, &device->state);

p_err:
	if (module != NULL) {
		is_vender_module_del(vender, module);
	}

	return ret;
}

int is_sensor_gpio_dbg(struct is_device_sensor *device)
{
	int ret = 0;
	u32 scenario;
	struct is_module_enum *module;
	struct exynos_platform_is_module *pdata;

	FIMC_BUG(!device);
	FIMC_BUG(!device->pdev);
	FIMC_BUG(!device->pdata);

	ret = is_sensor_g_module(device, &module);
	if (ret) {
		merr("is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	scenario = device->pdata->scenario;
	pdata = module->pdata;
	if (!pdata) {
		merr("pdata is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!pdata->gpio_dbg) {
		merr("gpio_dbg is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->gpio_dbg(module, scenario, GPIO_SCENARIO_ON);
	if (ret) {
		merr("gpio_dbg is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

void is_sensor_dump(struct is_device_sensor *device)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;

	subdev_module = device->subdev_module;

	ret = v4l2_subdev_call(subdev_module, core, log_status);
	if (ret) {
		err("module cannot support log_status(%d)", ret);
	}
}

IS_TIMER_FUNC(is_sensor_snr)
{
	struct is_device_sensor *device = from_timer(device, (struct timer_list *)data, snr_timer);
	struct is_sysfs_debug *sysfs_debug;

	sysfs_debug = is_get_sysfs_debug();

	/* Don't need to snr check */
	if ((!device->force_stop && !device->snr_check) || sysfs_debug->pattern_en ||
		!test_bit(IS_SENSOR_FRONT_START, &device->state))
		return;

	device->snr_check = false;

	if (device->force_stop) {
		err("forcely reset due to 0x%08lx", device->force_stop);
		device->force_stop = 0;
	} else {
		err("SNR detected");
#if IS_ENABLED(CONFIG_SEC_ABC)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=camera@INFO=mipi_overflow");
#else
		sec_abc_send_event("MODULE=camera@WARN=mipi_overflow");
#endif
#endif
	}

	is_sensor_dump(device);

	set_bit(IS_SENSOR_FRONT_SNR_STOP, &device->state);
}

static int is_sensor_start(struct is_device_sensor *device)
{
	int ret = 0;
	struct is_device_ischain *ischain;
	struct v4l2_subdev *subdev_module;
	struct is_sysfs_debug *sysfs_debug;
	bool sensor_only = test_bit(IS_SENSOR_ONLY, &device->state);

	ischain = device->ischain;
	if (!ischain) {
		merr("ischain is NULL", device);
		return -ENODEV;
	}

	/*
	 * This state is used for DVFS scenario.
	 * So, it should be set before DVFS selection.
	 */
	set_bit(IS_SENSOR_START, &device->state);
	is_set_static_dvfs(ischain, true);

	if (!sensor_only) {
		ret = is_itf_stream_on(ischain);
		if (ret) {
			merr("is_itf_stream_on is fail(%d)", device, ret);
			goto p_err_itf_stream_on;
		}
	}

	sysfs_debug = is_get_sysfs_debug();
	if (sysfs_debug->pattern_en) {
		struct v4l2_subdev *subdev_csi;

		subdev_csi = device->subdev_csi;
		if (!subdev_csi)
			mwarn("subdev_csi is NULL", device);

		ret = v4l2_subdev_call(subdev_csi, core, ioctl, SENSOR_IOCTL_PATTERN_ENABLE, NULL);
		if (ret) {
			mwarn("v4l2_csi_call(ioctl) is fail(%d)", device, ret);
			ret = -EINVAL;
			goto p_err_csi_pattern_en;
		}

		goto p_skip_module_ctrl;
	}

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		merr("subdev_module is NULL", device);
		ret = -EINVAL;
		goto p_err_subdev_module;
	}
	ret = v4l2_subdev_call(subdev_module, video, s_stream, true);
	if (ret) {
		merr("v4l2_subdev_call(s_stream) is fail(%d)", device, ret);
		set_bit(SENSOR_MODULE_GOT_INTO_TROUBLE, &device->state);
		goto p_err_module_s_stream;
	}
	set_bit(IS_SENSOR_WAIT_STREAMING, &device->state);

p_skip_module_ctrl:
	return 0;

p_err_module_s_stream:
p_err_subdev_module:
p_err_csi_pattern_en:
	if (!sensor_only) {
		ret = is_itf_stream_off(ischain);
		if (ret)
			merr("is_itf_stream_off is fail(%d)", device, ret);
	}

p_err_itf_stream_on:
	clear_bit(IS_SENSOR_START, &device->state);
	is_set_static_dvfs(ischain, false);

	return ret;
}

static int __is_sensor_stop(struct is_device_sensor *device)
{
	int ret = 0;
	int retry = 10;
	struct v4l2_subdev *subdev_module;

	while (--retry && test_bit(IS_SENSOR_WAIT_STREAMING, &device->state)) {
		mwarn(" waiting first pixel..\n", device);
		usleep_range(3000, 3100);
	}

	subdev_module = device->subdev_module;
	if (subdev_module) {
		ret = v4l2_subdev_call(subdev_module, video, s_stream, false);
		if (ret)
			merr("v4l2_subdev_call(s_stream) is fail(%d)", device, ret);
	} else {
		merr("subdev_module is NULL", device);
		ret = -EINVAL;
	}

	return ret;
}


static int is_sensor_stop(struct is_device_sensor *device)
{
	int ret = 0;
	struct is_group *group;
	struct is_device_ischain *ischain;
	struct v4l2_subdev *subdev_csi;
	struct is_sysfs_debug *sysfs_debug;
	bool sensor_only = test_bit(IS_SENSOR_ONLY, &device->state);

	ischain = device->ischain;
	if (!ischain) {
		merr("ischain is NULL", device);
		return -ENODEV;
	}

	if (sensor_only) {
		ret = __is_sensor_stop(device);
		if (ret)
			merr("__is_sensor_stop is fail in sensor stand alone(%d)", device, ret);

		goto p_err;
	}

	group = &device->group_sensor;

	subdev_csi = device->subdev_csi;
	if (!subdev_csi)
		mwarn("subdev_csi is NULL", device);

	/* @VOTF: In the case of VOTF, it should stop ischain first */
	if (VOTF_BACK_FIRST_OFF_COND(group, device->cfg)) {
		ret = is_itf_stream_off(ischain);
		if (ret)
			merr("is_itf_stream_off is fail(%d)", device, ret);
	}

	sysfs_debug = is_get_sysfs_debug();
	/* sensor stop */
	if (sysfs_debug->pattern_en) {
		ret = v4l2_subdev_call(subdev_csi, core, ioctl, SENSOR_IOCTL_PATTERN_DISABLE, NULL);
		if (ret)
			mwarn("v4l2_csi_call(ioctl) is fail(%d)", device, ret);
	} else {
		ret = __is_sensor_stop(device);
		if (ret)
			merr("__is_sensor_stop is fail(%d)", device, ret);
	}

	/* @OTF: It should stop ischain later */
	if (!VOTF_BACK_FIRST_OFF_COND(group, device->cfg)) {
		ret = is_itf_stream_off(ischain);
		if (ret)
			merr("is_itf_stream_off is fail(%d)", device, ret);
	}

	if (test_bit(IS_GROUP_VOTF_OUTPUT, &group->state) && group->next) {
		ret = is_votf_destroy_link_sensor(group->next);
		if (ret)
			merr("is_votf_destroy_link_sensor is fail(%d)", device, ret);
	}

p_err:
	clear_bit(IS_SENSOR_START, &device->state);
#ifdef ENABLE_SMOOTH_CAM_CHANGE
	is_set_static_dvfs(ischain, false);
#endif

	return ret;
}

int is_sensor_votf_tag(struct is_device_sensor *device,
		struct is_subdev *subdev)
{
	int ret = 0;
	struct is_frame *votf_frame;
	struct is_framemgr *votf_fmgr, *i_fmgr;
	struct is_group *group;
	struct v4l2_subdev *subdev_csi;
	struct v4l2_control ctrl;
	u32 hw_fcount;
	u32 frameptr;

	FIMC_BUG(!device);
	FIMC_BUG(!subdev);

	subdev_csi = device->subdev_csi;

	group = &device->group_sensor;

	if (test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		ctrl.id = V4L2_CID_IS_G_VC1_FRAMEPTR + (subdev->id - ENTRY_SSVC1);
		ret = v4l2_subdev_call(device->subdev_csi, core, ioctl, SENSOR_IOCTL_CSI_G_CTRL, &ctrl);
		if (ret) {
			mserr("V4L2_CID_IS_G_VC1_FRAMEPTR(g_ctrl) is fail(%d)", subdev, subdev, ret);
			return -EINVAL;
		}

		i_fmgr = GET_SUBDEV_I_FRAMEMGR(subdev);
		if (!i_fmgr) {
			mserr("i_fmr is NULL.", subdev, subdev);
			return -EINVAL;
		}

		frameptr = (ctrl.value + 1) % i_fmgr->num_frames;
		votf_frame = &i_fmgr->frames[frameptr];
	} else {
		ret = v4l2_subdev_call(subdev_csi, core, ioctl, SENSOR_IOCTL_G_HW_FCOUNT, &hw_fcount);
		if (ret) {
			mserr("SENSOR_IOCTL_G_HW_FCOUNT(ioctl) is fail(%d)", subdev, subdev, ret);
			return -EINVAL;
		}

		votf_frame = is_votf_get_frame(group, TWS, subdev->id, hw_fcount);
	}

	if (votf_frame) {
		votf_fmgr = is_votf_get_framemgr(group, TWS, subdev->id);

		/* update mater first for preventing mismatch */
		ret = votf_fmgr_call(votf_fmgr, master, s_addr, votf_frame);
		if (ret)
			mswarn("votf_fmgr_call(master) is fail(%d)",
					device, subdev, ret);

		ret = votf_fmgr_call(votf_fmgr, slave, s_addr, votf_frame);
		if (ret)
			mswarn("votf_fmgr_call(slave) is fail(%d)",
					device, subdev, ret);
	}

	return ret;
}

int is_sensor_buf_tag(struct is_device_sensor *device,
	struct is_subdev *f_subdev,
	struct v4l2_subdev *v_subdev,
	struct is_frame *ldr_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_framemgr *ldr_framemgr;
	struct is_group *group;
	int ldr_req_cnt, sub_req_cnt, ldr_com_cnt;
	int ldr_pro_cnt, sub_pro_cnt, sub_com_cnt;

	group = &device->group_sensor;
	ldr_framemgr = GET_SUBDEV_FRAMEMGR(&group->leader);
	FIMC_BUG(!ldr_framemgr);

	framemgr = GET_SUBDEV_FRAMEMGR(f_subdev);
	FIMC_BUG(!framemgr);

	framemgr_e_barrier_irqs(framemgr, 0, flags);

	ldr_req_cnt = ldr_framemgr->queued_count[FS_REQUEST];
	ldr_pro_cnt = ldr_framemgr->queued_count[FS_PROCESS];
	ldr_com_cnt = ldr_framemgr->queued_count[FS_COMPLETE];
	sub_req_cnt = framemgr->queued_count[FS_REQUEST];
	sub_pro_cnt = framemgr->queued_count[FS_PROCESS];
	sub_com_cnt = framemgr->queued_count[FS_COMPLETE];

	if ((!test_bit(IS_SENSOR_OTF_OUTPUT, &device->state) &&
				((sub_req_cnt > ldr_req_cnt) || (sub_pro_cnt > ldr_pro_cnt))) ||
			sub_com_cnt > ldr_com_cnt)
		mgrwarn("subdev DQ might be blocked: ldr(R%d, P%d, C%d), sub[%s](R%d, P%d, C%d)",
				group, group,
				ldr_frame,
				ldr_req_cnt, ldr_pro_cnt, ldr_com_cnt,
				f_subdev->name,
				sub_req_cnt, sub_pro_cnt, sub_com_cnt);

	frame = peek_frame(framemgr, FS_REQUEST);
	if (frame) {
		if (!frame->stream) {
			framemgr_x_barrier_irqr(framemgr, 0, flags);
			merr("frame->stream is NULL", device);
			BUG();
		}

		frame->fcount = ldr_frame->fcount;
		frame->stream->findex = ldr_frame->index;
		frame->stream->fcount = ldr_frame->fcount;
		frame->result = 0;

		ret = v4l2_subdev_call(v_subdev, video, s_rx_buffer, (void *)frame, NULL);
		if (ret) {
			msrwarn("v4l2_subdev_call(s_rx_buffer) is fail(%d)",
					device, f_subdev, ldr_frame, ret);
			ret = -EINVAL;
		} else {
			set_bit(f_subdev->id, &ldr_frame->out_flag);

			if (test_bit(IS_SUBDEV_VOTF_USE, &f_subdev->state)) {
				/* update mater first for preventing mismatch */
				struct is_framemgr *votf_fmgr;
				struct is_group *group = &device->group_sensor;

				votf_fmgr = is_votf_get_framemgr(group, TWS, f_subdev->id);
				ret = votf_fmgr_call(votf_fmgr, slave, s_addr, frame);
				if (ret)
					msrwarn("votf_fmgr_call(slave) is fail(%d)",
						device, f_subdev, ldr_frame, ret);
			}
		}
	} else {
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, 0, flags);

	return ret;
}

int is_sensor_buf_skip(struct is_device_sensor *device,
	struct is_subdev *f_subdev,
	struct v4l2_subdev *v_subdev,
	struct is_frame *ldr_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_video_ctx *vctx;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_framemgr *ldr_framemgr;
	struct is_group *group;
	int ldr_req_cnt, sub_req_cnt, ldr_com_cnt;
	int ldr_pro_cnt, sub_pro_cnt, sub_com_cnt;

	group = &device->group_sensor;
	ldr_framemgr = GET_SUBDEV_FRAMEMGR(&group->leader);
	FIMC_BUG(!ldr_framemgr);

	framemgr = GET_SUBDEV_FRAMEMGR(f_subdev);
	FIMC_BUG(!framemgr);

	vctx = f_subdev->vctx;
	FIMC_BUG(!vctx);

	framemgr_e_barrier_irqs(framemgr, 0, flags);

	ldr_req_cnt = ldr_framemgr->queued_count[FS_REQUEST];
	ldr_pro_cnt = ldr_framemgr->queued_count[FS_PROCESS];
	ldr_com_cnt = ldr_framemgr->queued_count[FS_COMPLETE];
	sub_req_cnt = framemgr->queued_count[FS_REQUEST];
	sub_pro_cnt = framemgr->queued_count[FS_PROCESS];
	sub_com_cnt = framemgr->queued_count[FS_COMPLETE];

	if ((!test_bit(IS_SENSOR_OTF_OUTPUT, &device->state) &&
				((sub_req_cnt > ldr_req_cnt) || (sub_pro_cnt > ldr_pro_cnt))) ||
			sub_com_cnt > ldr_com_cnt)
		mgrwarn("subdev DQ might be blocked: ldr(R%d, P%d, C%d), sub[%s](R%d, P%d, C%d)",
				group, group,
				ldr_frame,
				ldr_req_cnt, ldr_pro_cnt, ldr_com_cnt,
				f_subdev->name,
				sub_req_cnt, sub_pro_cnt, sub_com_cnt);

	frame = peek_frame(framemgr, FS_REQUEST);
	if (frame) {
		if (!frame->stream) {
			framemgr_x_barrier_irqr(framemgr, 0, flags);
			merr("frame->stream is NULL", device);

			return -EINVAL;
		}

		frame->fcount = ldr_frame->fcount;
		frame->stream->findex = ldr_frame->index;
		frame->stream->fcount = ldr_frame->fcount;
		frame->result = 0;

		trans_frame(framemgr, frame, FS_PROCESS);
	}

	frame = peek_frame(framemgr, FS_PROCESS);
	if (frame) {
		trans_frame(framemgr, frame, FS_COMPLETE);

		ldr_frame = &ldr_framemgr->frames[frame->stream->findex];
		clear_bit(f_subdev->id, &ldr_frame->out_flag);

		CALL_VOPS(vctx, done, frame->index, VB2_BUF_STATE_DONE);
	} else {
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, 0, flags);

	return ret;
}

int is_sensor_dm_tag(struct is_device_sensor *device,
	struct is_frame *frame)
{
	int ret = 0;
	u32 hashkey;
	int i;
	u64 merge_f_id;
	u8 sub_f_id;

	FIMC_BUG(!device);
	FIMC_BUG(!frame);

	hashkey = frame->fcount % IS_TIMESTAMP_HASH_KEY;
	if (frame->shot) {
		frame->shot->dm.request.frameCount = frame->fcount;
		frame->shot->dm.sensor.timeStamp = device->timestamp[hashkey];
		frame->shot->udm.sensor.timeStampBoot = device->timestampboot[hashkey];

		if (device->timestampboot[hashkey] < device->prev_timestampboot)
			minfo("[SS%d][F%d] Reverse timestampboot(p[%llu], c[%llu])\n",
				device, device->device_id,
				frame->fcount,
				device->prev_timestampboot,
				device->timestampboot[hashkey]);

		device->prev_timestampboot = device->timestampboot[hashkey];
		/*
		 * frame_id is extraced form embedded data of sensor.
		 * So, embedded data should be extraced before frame end.
		 */
		merge_f_id = device->frame_id[hashkey];

		for (i = 0; i < BITS_PER_LONG / F_ID_SIZE; i++) {
			sub_f_id = (merge_f_id >> (i * F_ID_SIZE));

			if (!sub_f_id)
				break;

			frame->shot->udm.frame_id[i] =
				sub_f_id & GENMASK(F_ID_SIZE - 1, 0);
		}

		if (is_get_debug_param(IS_DEBUG_PARAM_CSI) && merge_f_id)
			minfo("[SS%d][F%d] frame_id(0x%lx)\n", device, device->device_id,
				frame->fcount, merge_f_id);

#ifdef DBG_JITTER
		is_jitter(frame->shot->dm.sensor.timeStamp);
#endif

		/*
		 * If Mode change scenario(ex> remosaic) occured,
		 * sensor itf function cannot called due to not operated AE.
		 * So need to update sensor dm as preview operating.
		 */
		if (is_vendor_check_remosaic_mode_change(frame)) {
			struct is_module_enum *module =
				(struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
			struct is_device_sensor_peri *sensor_peri;
			struct is_sensor_interface *itf;

			if (!module) {
				merr("module is NULL", device);
				return -EINVAL;
			}

			sensor_peri = (struct is_device_sensor_peri *)module->private_data;
			if (sensor_peri == NULL) {
				merr("sensor_peri is null", device);
				return -EINVAL;
			}

			itf = &sensor_peri->sensor_interface;
			itf->cis_itf_ops.update_sensor_dynamic_meta(itf, frame->fcount,
					&frame->shot->ctl, &frame->shot->dm, &frame->shot->udm);
		}
	}

	return ret;
}

static int is_sensor_notify_by_fstr(struct is_device_sensor *device, void *arg,
	unsigned int notification)
{
	int i = 0;
	int ret = 0;
	u32 fcount;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_device_csi *csi;
	struct is_subdev *dma_subdev;
	struct v4l2_control ctrl;
	u32 frameptr;
	unsigned long flags;
#if defined(MEASURE_TIME) && defined(MONITOR_TIME)
	struct is_group *group;
#endif

	FIMC_BUG(!device);
	FIMC_BUG(!arg);

	fcount = *(u32 *)arg;

	if (device->instant_cnt) {
		device->instant_cnt--;
		if (device->instant_cnt <= INSTANT_OFF_CNT)
			wake_up(&device->instant_wait);
	}

#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
	{
		frame = NULL;
		framemgr = NULL;
		group = &device->group_sensor;
		framemgr = GET_SUBDEV_FRAMEMGR(&group->head->leader);
		if (framemgr)
			frame = peek_frame(framemgr, FS_PROCESS);

		if (frame && frame->fcount == fcount)
			TIME_SHOT(TMS_SHOT2);
	}

#endif
#endif

	csi = v4l2_get_subdevdata(device->subdev_csi);
	if (!csi) {
		merr("CSI is NULL", device);
		return -EINVAL;
	}

	/* tagging for intenernal subdevs */
	for (i = CSI_VIRTUAL_CH_1; i <= CSI_VIRTUAL_CH_3; i++) {
		dma_subdev = csi->dma_subdev[i];

		if (!dma_subdev ||
			!test_bit(IS_SUBDEV_START, &dma_subdev->state) ||
			!test_bit(IS_SUBDEV_INTERNAL_USE, &dma_subdev->state))
			continue;

		framemgr = GET_SUBDEV_I_FRAMEMGR(dma_subdev);
		if (!framemgr) {
			merr("VC%d framemgr is NULL", device, i);
			continue;
		}

		if (!framemgr->frames)
			continue;

		framemgr_e_barrier_irqs(framemgr, 0, flags);

		ctrl.id = V4L2_CID_IS_G_VC1_FRAMEPTR + (i - 1);
		ret = v4l2_subdev_call(device->subdev_csi, core, ioctl, SENSOR_IOCTL_CSI_G_CTRL, &ctrl);
		if (ret) {
			err("csi_g_ctrl fail");
			framemgr_x_barrier_irqr(framemgr, 0, flags);
			return -EINVAL;
		}

		frameptr = (ctrl.value) % framemgr->num_frames;

		frame = &framemgr->frames[frameptr];
		frame->fcount = fcount;

		mdbgd_sensor("%s, VC%d[%d] = %d\n", device, __func__,
					i, frameptr, frame->fcount);

		framemgr_x_barrier_irqr(framemgr, 0, flags);
	}

	return ret;
}

static int is_sensor_notify_by_fend(struct is_device_sensor *device, void *arg,
	unsigned int notification)
{
	int ret = 0;
	u32 status = 0;
	u32 done_state = VB2_BUF_STATE_DONE;
	struct is_frame *frame;
	struct is_group *group;
	struct is_framemgr *framemgr;
	struct is_video_ctx *vctx;
	unsigned long flags;

	FIMC_BUG(!device);

	if (device->snr_check) {
		device->snr_check = false;
		/* we are in softirq, so we can use 'del_timer_sync' */
		if (timer_pending(&device->snr_timer))
			del_timer_sync(&device->snr_timer);
	}

	if (device->force_stop)
		is_sensor_snr((IS_TIMER_PARAM_TYPE)&device->snr_timer);

	group = &device->group_sensor;

	framemgr = GET_SUBDEV_FRAMEMGR(&group->head->leader);
	FIMC_BUG(!framemgr);

	vctx = group->head->leader.vctx;

	framemgr_e_barrier_irqs(framemgr, 0, flags);
	frame = peek_frame(framemgr, FS_PROCESS);
	if (frame) {
		status = *(u32 *)arg;
		if (status) {
			mgrinfo("[ERR] NDONE(%d, E%d)\n", group, group, frame, frame->index, status);
			done_state = VB2_BUF_STATE_ERROR;
			frame->result = status;
		} else {
			mgrdbgs(1, " DONE(%d)\n", group, group, frame, frame->index);
		}
	}

	/* if OTF case, skip buffer done for sensor leader's frame */
	if (test_bit(IS_SENSOR_OTF_OUTPUT, &device->state))
		goto p_err;

	/*
	 * This processing frame's fcount should be equals to sensor device's fcount.
	 * If it's not, it will be processed in next frame.
	 */
	if (frame && (frame->fcount > device->fcount)) {
		mgwarn("frame count is bigger than current (%d > %d)[%d/%d/%d]",
			device, group, frame->fcount, device->fcount,
			framemgr->queued_count[FS_REQUEST],
			framemgr->queued_count[FS_PROCESS],
			framemgr->queued_count[FS_COMPLETE]);
		frame = NULL;
	}

	if (frame) {
#ifdef MEASURE_TIME
#ifdef EXTERNAL_TIME
		ktime_get_ts64(&frame->tzone[TM_FLITE_END]);
#endif
		TIME_SHOT(TMS_SDONE);
#endif
		clear_bit(group->leader.id, &frame->out_flag);
		is_group_done(device->groupmgr, group, frame, done_state);
		trans_frame(framemgr, frame, FS_COMPLETE);
		CALL_VOPS(vctx, done, frame->index, done_state);
	}

p_err:
	framemgr_x_barrier_irqr(framemgr, 0, flags);

	return ret;
}

static int is_sensor_notify_by_dma_end(struct is_device_sensor *device, void *arg,
	unsigned int notification)
{
	int ret = 0;
	struct is_frame *frame;
#ifdef USE_CAMERA_EMBEDDED_HEADER
	struct is_device_csi *csi;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_cis *cis = NULL;
	u32 hashkey;
	u16 frame_id = 0;
#endif

	FIMC_BUG(!device);

	frame = (struct is_frame *)arg;
	if (frame) {
		switch (notification) {
#ifdef USE_CAMERA_EMBEDDED_HEADER
		case CSIS_NOTIFY_DMA_END_VC_EMBEDDED:
			hashkey = frame->fcount % IS_TIMESTAMP_HASH_KEY;

			ret = is_sensor_g_module(device, &module);
			if (ret) {
				mwarn("%s sensor_g_module failed(%d)", device, __func__, ret);
				return -EINVAL;
			}

			sensor_peri = (struct is_device_sensor_peri *)module->private_data;
			if (sensor_peri == NULL) {
				mwarn("sensor_peri is null", device);
				return -EINVAL;
			};

			csi = v4l2_get_subdevdata(device->subdev_csi);
			if (!csi) {
				mwarn("CSI is NULL", device);
				return -EINVAL;
			}

			if (sensor_peri->subdev_cis && !csi->f_id_dec) {
				cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

				CALL_CISOPS(cis, cis_get_frame_id, sensor_peri->subdev_cis,
						(u8 *)frame->kvaddr_buffer[0], &frame_id);

				device->frame_id[hashkey] = frame_id;
			}

			break;
#endif
		case CSIS_NOTIFY_DMA_END_VC_MIPISTAT:
			break;
		default:
			break;
		}
	}

	return ret;
}

static int is_sensor_notify_by_line(struct is_device_sensor *device,
	unsigned int notification)
{
	int ret = 0;
	struct is_group *group;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;

	FIMC_BUG(!device);

	device->line_fcount++;

	group = &device->group_sensor;
	framemgr = GET_SUBDEV_FRAMEMGR(&group->head->leader);
	FIMC_BUG(!framemgr);

	framemgr_e_barrier_irqs(framemgr, 0, flags);
	frame = find_frame(framemgr, FS_PROCESS, frame_fcount, (void *)(ulong)device->line_fcount);
	framemgr_x_barrier_irqr(framemgr, 0, flags);

	/* There's no shot */
	if (!frame ||
		!test_bit(group->leader.id, &frame->out_flag)) {
		mgwarn("[F%d] (SENSOR LINE IRQ) There's no Shot",
				group, group, device->line_fcount);
		goto p_err;
	}

	mgrdbgs(1, " SENSOR LINE IRQ for M2M\n", group, group, frame);

	/*
	 * return first shot only.
	 * Because second shot was applied in line interrupt
	 * like concept of 3AA configure lock.
	 */
	if (!atomic_read(&group->scount))
		goto p_err;

	ret = is_devicemgr_shot_callback(group, frame, frame->fcount, IS_DEVICE_SENSOR);
	if (ret) {
		merr("is_devicemgr_shot_callback fail(%d)", device, ret);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}


static void is_sensor_notify(struct v4l2_subdev *subdev,
	unsigned int notification,
	void *arg)
{
	int ret = 0;
	struct is_device_sensor *device;

	FIMC_BUG_VOID(!subdev);

	device = v4l2_get_subdev_hostdata(subdev);

	switch(notification) {
	case FLITE_NOTIFY_FSTART:
	case CSIS_NOTIFY_FSTART:
		ret = is_sensor_notify_by_fstr(device, arg, notification);
		if (ret)
			merr("is_sensor_notify_by_fstr is fail(%d)", device, ret);
		break;
	case FLITE_NOTIFY_FEND:
	case CSIS_NOTIFY_FEND:
		ret = is_sensor_notify_by_fend(device, arg, notification);
		if (ret)
			merr("is_sensor_notify_by_fend is fail(%d)", device, ret);
		break;
	case FLITE_NOTIFY_DMA_END:
	case CSIS_NOTIFY_DMA_END:
	case CSIS_NOTIFY_DMA_END_VC_EMBEDDED:
	case CSIS_NOTIFY_DMA_END_VC_MIPISTAT:
		ret = is_sensor_notify_by_dma_end(device, arg, notification);
		if (ret < 0)
			merr("is_sensor_notify_by_dma_end is fail(%d)", device, ret);
		break;
	case CSIS_NOTIFY_LINE:
		ret = is_sensor_notify_by_line(device, notification);
		if (ret)
			merr("is_sensor_notify_by_line is fail(%d)", device, ret);
		break;
	case CSI_NOTIFY_VSYNC:
		ret = v4l2_subdev_call(device->subdev_module, core, ioctl, V4L2_CID_SENSOR_NOTIFY_VSYNC, arg);
		if (ret)
			merr("is sensor notify vsync is fail", device);
		break;
	case CSI_NOTIFY_VBLANK:
		ret = v4l2_subdev_call(device->subdev_module, core, ioctl, V4L2_CID_SENSOR_NOTIFY_VBLANK, arg);
		if (ret)
			merr("is sensor notify vblank is fail", device);
		break;
	}
}

static void is_sensor_instanton(struct work_struct *data)
{
	int ret = 0;
	u32 instant_cnt;
	struct is_device_sensor *device;
	struct is_core *core;

	FIMC_BUG_VOID(!data);

	device = container_of(data, struct is_device_sensor, instant_work);
	instant_cnt = device->instant_cnt;
	core = (struct is_core *)device->private_data;

	clear_bit(IS_SENSOR_ESD_RECOVERY, &device->state);
	clear_bit(IS_SENSOR_FRONT_SNR_STOP, &device->state);
	clear_bit(SENSOR_MODULE_GOT_INTO_TROUBLE, &device->state);
	clear_bit(IS_SENSOR_ASSERT_CRASH, &device->state);

	TIME_LAUNCH_STR(LAUNCH_SENSOR_START);

	mutex_lock(&device->mutex_reboot);
	if (device->reboot) {
		minfo("system is rebooting, don't start sensor\n", device);

		if (v4l2_subdev_call(device->subdev_csi, video,
					s_stream, IS_DISABLE_STREAM))
			merr("failed to disable CSI subdev", device);

		ret = -EINVAL;

		mutex_unlock(&device->mutex_reboot);
		goto dont_start_sensor;
	}

	ret = is_sensor_start(device);
	if (ret) {
		merr("failed to start sensor: %d", device, ret);

		/* disable CSI when error occured */
		if (v4l2_subdev_call(device->subdev_csi, video,
					s_stream, IS_DISABLE_STREAM))
			merr("failed to disable CSI subdev", device);

		ret = -EINVAL;

		mutex_unlock(&device->mutex_reboot);
		goto err_sensor_start;
	}

	set_bit(IS_SENSOR_FRONT_START, &device->state);
	mutex_unlock(&device->mutex_reboot);

	TIME_LAUNCH_END(LAUNCH_SENSOR_START);

	if (device->snr_check) {
#if defined(CAMERA_SNR_TIMER_MS)
		mod_timer(&device->snr_timer, jiffies + msecs_to_jiffies(CAMERA_SNR_TIMER_MS));
#else
		mod_timer(&device->snr_timer, jiffies + msecs_to_jiffies(300));
#endif
		minfo("[SS%d] SNR checking...\n", device, device->device_id);
	}

	if (instant_cnt) {
		ulong timetowait, timetoelapse, timeout;

		TIME_LAUNCH_STR(LAUNCH_FAST_AE);
		timeout = IS_FLITE_STOP_TIMEOUT + msecs_to_jiffies(instant_cnt*60);
		timetowait = wait_event_timeout(device->instant_wait,
			(device->instant_cnt <= INSTANT_OFF_CNT),
			timeout);
		if (!timetowait) {
			merr("wait_event_timeout is invalid", device);
			ret = -ETIME;
		}

		is_sensor_front_stop(device, false);
		/* The sstream value is set or cleared by HAL when driver received V4L2_CID_IS_S_STREAM.
		   But, HAL didnt' call s_ctrl after fastAe scenario. So, it needs to be cleared
		   automatically if fastAe scenario is finished.
		 */
		device->sstream = 0;

		timetoelapse = (jiffies_to_msecs(timeout) - jiffies_to_msecs(timetowait));
		minfo("[SS%d:D] instant off(fcount : %d, time : %ldms)\n", device, device->device_id,
			device->instant_cnt,
			timetoelapse);
		TIME_LAUNCH_END(LAUNCH_FAST_AE);
	} else {
		TIME_LAUNCH_END(LAUNCH_TOTAL);
	}

err_sensor_start:
dont_start_sensor:
	device->instant_ret = ret;
}

static int is_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 device_id = -1;
	atomic_t dev_id;
	struct device *is_dev;
	struct is_core *core;
	struct is_device_sensor *device;
	struct exynos_platform_is_sensor *pdata;

	FIMC_BUG(!pdev);

	is_dev = is_get_is_dev();
	if (is_dev == NULL) {
		warn("is_dev is not yet probed(sensor)");
		pdev->dev.init_name = IS_SENSOR_DEV_NAME;
		return -EPROBE_DEFER;
	}

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}

	ret = is_sensor_dev_parse_dt(pdev);
	if (ret) {
		err("parsing device tree is fail(%d)", ret);
		goto p_err;
	}

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		err("pdata is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* 1. get device */
	atomic_set(&dev_id, pdev->id);
	device = &core->sensor[pdev->id];

	/* v4l2 device and device init */
	memset(&device->v4l2_dev, 0, sizeof(struct v4l2_device));
	device_id = v4l2_device_set_name(&device->v4l2_dev, IS_SENSOR_DEV_NAME, &dev_id);
	device->v4l2_dev.notify = is_sensor_notify;
	device->instance = 0;
	device->device_id = device_id;
	device->position = SENSOR_POSITION_REAR;
	device->resourcemgr = &core->resourcemgr;
	device->pdev = pdev;
	device->private_data = core;
	device->pdata = pdata;
	device->groupmgr = &core->groupmgr;
	device->devicemgr = &core->devicemgr;
	device->reboot = false;

	if (IS_ENABLED(INIT_AWB)) {
		memset(device->init_wb, 0, sizeof(float) * WB_GAIN_COUNT);
		memset(device->last_wb, 0, sizeof(float) * WB_GAIN_COUNT);
		memset(device->chk_wb, 0, sizeof(float) * WB_GAIN_COUNT);
	}

	platform_set_drvdata(pdev, device);
	init_waitqueue_head(&device->instant_wait);
	INIT_WORK(&device->instant_work, is_sensor_instanton);
	spin_lock_init(&device->slock_state);
	mutex_init(&device->mlock_state);
	device_init_wakeup(&pdev->dev, true);
	mutex_init(&device->mutex_reboot);

	/* 3. state init */
	memset(&device->state, 0, sizeof(device->state));

	atomic_set(&device->group_open_cnt, 0);

	device->snr_check = false;

#if defined(CONFIG_PM)
	pm_runtime_enable(&pdev->dev);
#endif
	ret = v4l2_device_register(&pdev->dev, &device->v4l2_dev);
	if (ret) {
		merr("v4l2_device_register is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_csi_probe(device, device_id, pdata->csi_ch, pdata->wdma_ch_hint);
	if (ret) {
		merr("is_csi_probe is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ssx_video_probe(device);
	if (ret) {
		merr("is_sensor_video_probe is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ssxvc0_video_probe(device);
	if (ret) {
		merr("is_ssxvc0_video_probe is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ssxvc1_video_probe(device);
	if (ret) {
		merr("is_ssxvc1_video_probe is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ssxvc2_video_probe(device);
	if (ret) {
		merr("is_ssxvc2_video_probe is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ssxvc3_video_probe(device);
	if (ret) {
		merr("is_ssxvc3_video_probe is fail(%d)", device, ret);
		goto p_err;
	}

	is_group_probe(device->groupmgr, &device->group_sensor, device, NULL,
		is_sensor_shot,
		GROUP_SLOT_SENSOR, ENTRY_SENSOR, "SSX", &is_subdev_sensor_ops);

	is_subdev_probe(&device->ssvc0, device_id, ENTRY_SSVC0, "VC0", &is_subdev_ssvc_ops);
	is_subdev_probe(&device->ssvc1, device_id, ENTRY_SSVC1, "VC1", &is_subdev_ssvc_ops);
	is_subdev_probe(&device->ssvc2, device_id, ENTRY_SSVC2, "VC2", &is_subdev_ssvc_ops);
	is_subdev_probe(&device->ssvc3, device_id, ENTRY_SSVC3, "VC3", &is_subdev_ssvc_ops);

	/* clear group open state */
	clear_bit(IS_GROUP_OPEN, &device->group_sensor.state);

	/* clear subdevice state */
	clear_bit(IS_SUBDEV_OPEN, &device->group_sensor.leader.state);
	clear_bit(IS_SUBDEV_OPEN, &device->ssvc0.state);
	clear_bit(IS_SUBDEV_OPEN, &device->ssvc1.state);
	clear_bit(IS_SUBDEV_OPEN, &device->ssvc2.state);
	clear_bit(IS_SUBDEV_OPEN, &device->ssvc3.state);

	clear_bit(IS_SUBDEV_START, &device->group_sensor.leader.state);
	clear_bit(IS_SUBDEV_START, &device->ssvc0.state);
	clear_bit(IS_SUBDEV_START, &device->ssvc1.state);
	clear_bit(IS_SUBDEV_START, &device->ssvc2.state);
	clear_bit(IS_SUBDEV_START, &device->ssvc3.state);

	/* Setup snr_timer check at probe */
	timer_setup(&device->snr_timer, (void (*)(struct timer_list *))is_sensor_snr, 0);

	set_bit(IS_SENSOR_PROBE, &device->state);

p_err:
	info("[SS%d:D] %s(%d)\n", device_id, __func__, ret);
	return ret;
}

int is_sensor_open(struct is_device_sensor *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	int i;

	FIMC_BUG(!device);
	FIMC_BUG(!device->subdev_csi);
	FIMC_BUG(!device->private_data);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	mutex_lock(&device->mlock_state);
	if (test_bit(IS_SENSOR_OPEN, &device->state)) {
		merr("already open", device);
		ret = -EMFILE;
		goto err_open_state;
	}

	/*
	 * Sensor's mclk can be accessed by other ips(ex. preprocessor)
	 * There's a problem that mclk_on can be called twice due to clear_bit.
	 *  - clear_bit(IS_SENSOR_MCLK_ON, &device->state);
	 * Clear_bit of MCLK_ON should be skiped.
	 */
	clear_bit(IS_SENSOR_START, &device->state);
	clear_bit(IS_SENSOR_S_INPUT, &device->state);
	clear_bit(IS_SENSOR_ONLY, &device->state);
	clear_bit(IS_SENSOR_FRONT_START, &device->state);
	clear_bit(IS_SENSOR_FRONT_SNR_STOP, &device->state);
	clear_bit(IS_SENSOR_ESD_RECOVERY, &device->state);
	clear_bit(IS_SENSOR_BACK_START, &device->state);
	clear_bit(IS_SENSOR_OTF_OUTPUT, &device->state);
	clear_bit(IS_SENSOR_WAIT_STREAMING, &device->state);
	clear_bit(SENSOR_MODULE_GOT_INTO_TROUBLE, &device->state);
	clear_bit(IS_SENSOR_ASSERT_CRASH, &device->state);
	clear_bit(IS_SENSOR_S_POWER, &device->state);

	device->smc_state = IS_SENSOR_SMC_INIT;
	device->vctx = vctx;
	device->fcount = 0;
	device->line_fcount = 0;
	device->instant_cnt = 0;
	device->instant_ret = 0;
	device->ischain = NULL;
	device->exposure_time = 0;
	device->frame_duration = 0;
	device->force_stop = 0;
	device->early_buf_done_mode = 0;
	device->ex_scenario = 0;
	device->ex_mode = 0;
	device->ex_mode_extra = 0;
	device->ex_mode_format = 0;
	device->seamless_state = 0L;
	for (i = 0; i < IS_EXP_BACKUP_COUNT; i++) {
		device->exposure_value[i] = 0;
		device->exposure_fcount[i] = 0;
	}
	memset(&device->sensor_ctl, 0, sizeof(struct camera2_sensor_ctl));
	memset(&device->lens_ctl, 0, sizeof(struct camera2_lens_ctl));
	memset(&device->flash_ctl, 0, sizeof(struct camera2_flash_ctl));

	/* copy last awb gain value to init awb value */
	if (IS_ENABLED(INIT_AWB))
		memcpy(device->init_wb, device->last_wb, sizeof(float) * WB_GAIN_COUNT);

	ret = is_devicemgr_open(device->devicemgr, (void *)device, IS_DEVICE_SENSOR);
	if (ret) {
		err("is_devicemgr_open is fail(%d)", ret);
		goto err_devicemgr_open;
	}

	/* get camif ip for wdma */
	ret = is_hw_camif_open((void *)device);
	if (ret) {
		merr("is_hw_camif_open is fail", device);
		goto err_camif_open;
	}

	ret = is_csi_open(device->subdev_csi, GET_FRAMEMGR(vctx));
	if (ret) {
		merr("is_csi_open is fail(%d)", device, ret);
		goto err_csi_open;
	}

	/* for mediaserver force close */
	ret = is_resource_get(device->resourcemgr, device->device_id);
	if (ret) {
		merr("is_resource_get is fail", device);
		goto err_resource_get;
	}

	device->snr_check = true;
	set_bit(IS_SENSOR_OPEN, &device->state);
	mutex_unlock(&device->mlock_state);

	minfo("[SS%d:D] %s():%d\n", device, device->device_id, __func__, ret);

	if (IS_RUNNING_TUNING_SYSTEM())
		pablo_obte_setcount_ssx_open(device->device_id, ret);

	return 0;

err_resource_get:
	ret_err = is_devicemgr_close(device->devicemgr, (void *)device, IS_DEVICE_SENSOR);
	if (ret_err)
		merr("is_devicemgr_close is fail(%d)", device, ret_err);
err_devicemgr_open:
	ret_err = is_csi_close(device->subdev_csi);
	if (ret_err)
		merr("is_csi_close is fail(%d)", device, ret_err);
err_csi_open:
err_camif_open:
err_open_state:
	mutex_unlock(&device->mlock_state);
	merr("[SS%d:D] ret(%d)\n", device, device->device_id, ret);
	return ret;

}
KUNIT_EXPORT_SYMBOL(is_sensor_open);

int is_sensor_close(struct is_device_sensor *device)
{
	int ret = 0;
	struct is_group *group;
	struct is_core *core;

	FIMC_BUG(!device);

	core = device->private_data;

	if (!device->vctx) {
		if (!core)
			minfo("%s: core(null), skip sensor_close() - shutdown\n", device, __func__);
		else if (core->shutdown)
			minfo("%s: vctx(null), skip sensor_close() - shutdown\n", device, __func__);

		return 0;
	}

	FIMC_BUG(!device->vctx);

	if (!test_bit(IS_SENSOR_OPEN, &device->state)) {
		merr("already close", device);
		ret = -EMFILE;
		goto p_err;
	}

	/* for mediaserver force close */
	group = &device->group_sensor;
	if (test_bit(IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
	}

	ret = is_sensor_front_stop(device, true);
	if (ret)
		merr("is_sensor_front_stop is fail(%d)", device, ret);

	ret = is_sensor_back_stop(device, GET_QUEUE(device->vctx));
	if (ret)
		merr("is_sensor_back_stop is fail(%d)", device, ret);

	ret = is_csi_close(device->subdev_csi);
	if (ret)
		merr("is_csi_close is fail(%d)", device, ret);

	ret = is_devicemgr_close(device->devicemgr, (void *)device, IS_DEVICE_SENSOR);
	if (ret)
		merr("is_devicemgr_close is fail(%d)", device, ret);

	/* cancel a work and wait for it to finish */
	cancel_work_sync(&device->instant_work);

	if (device->subdev_module) {
		ret = v4l2_subdev_call(device->subdev_module, core, ioctl, V4L2_CID_SENSOR_DEINIT, device);
		if (ret)
			merr("is sensor deinit is fail, ret(%d)", device, ret);
	}

	/* for mediaserver force close */
	ret = is_resource_put(device->resourcemgr, device->device_id);
	if (ret)
		merr("is_resource_put is fail", device);

	clear_bit(device->position, &core->sensor_map);
	clear_bit(IS_SENSOR_OPEN, &device->state);
	clear_bit(IS_SENSOR_S_INPUT, &device->state);
	clear_bit(IS_SENSOR_ITF_REGISTER, &device->state);

p_err:
	minfo("[SS%d:D] %s():%d\n", device, device->device_id,  __func__, ret);

	if (IS_RUNNING_TUNING_SYSTEM())
		pablo_obte_setcount_ssx_close(device->device_id);

	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_close);

int is_sensor_map_sensor_module(struct is_device_ischain *device, int position)
{
	int ret = 0;
	struct is_core *core = is_get_is_core();
	struct is_module_enum *module;
	int i;

	FIMC_BUG(!core);
	FIMC_BUG(!device);

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		ret = is_search_sensor_module_with_position(&core->sensor[i], position, &module);
		if (!ret)
			break;
	}

	if (ret) {
		merr("fail to map sensor and module(%d)", device, ret);
		goto p_err;
	}

	device->sensor = &core->sensor[i];
	device->sensor->position = position;
	device->subdev_module = module->subdev;
	device->module_enum = module;

	info("%s done", __func__);
p_err:
	return ret;

}

void is_sensor_g_max_size(u32 *max_width, u32 *max_height)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor *sensor;
	struct is_module_enum *module;
	u32 mindex, mmax;
	u32 i;

	*max_width = 0;
	*max_height = 0;

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		sensor = &core->sensor[i];
		mmax = atomic_read(&sensor->module_count);
		for (mindex = 0; mindex < mmax; mindex++) {
			module = &sensor->module_enum[mindex];
			if (*max_height < module->pixel_height)
				*max_height = module->pixel_height;
			if (*max_width < module->pixel_width)
				*max_width = module->pixel_width;
		}
	}
}
KUNIT_EXPORT_SYMBOL(is_sensor_g_max_size);

bool is_sensor_g_aeb_mode(struct is_device_sensor *sensor)
{
	u32 ex_mode = is_sensor_g_ex_mode(sensor);

	return (ex_mode == EX_AEB && sensor->group_sensor.pnext);
}

static void set_cis_data(struct is_device_sensor *s,
				struct is_module_enum *m)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor_peri *sp =
			(struct is_device_sensor_peri *)m->private_data;
	u32 i2c_ch = m->pdata->sensor_i2c_ch;

	if (i2c_ch < SENSOR_CONTROL_I2C_MAX) {
		sp->cis.i2c_lock = &core->i2c_lock[i2c_ch];
		minfo("%s[%d]enable cis i2c client. position = %d\n",
			s, __func__, __LINE__, m->position);
	} else {
		mwarn("wrong cis i2c_channel(%d)", s, i2c_ch);
	}
}

static void set_actuator_data(struct is_device_sensor *s,
				struct is_module_enum *m)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor_peri *sp =
			(struct is_device_sensor_peri *)m->private_data;
	u32 i2c_ch = m->pdata->af_i2c_ch;

	if (!sp->actuator || !sp->subdev_actuator) {
		sp->subdev_actuator = s->subdev_actuator[s->pdev->id];
		sp->actuator = s->actuator[s->pdev->id];
	}

	if (!IS_ENABLED(CONFIG_CAMERA_USE_INTERNAL_MCU) ||
			!IS_ENABLED(USE_TELE_OIS_AF_COMMON_INTERFACE) ||
			!(sp->mcu && sp->mcu->mcu_ctrl_actuator)) {
		if (sp->actuator && (m->pdata->af_product_name
					== s->actuator[s->pdev->id]->id)) {
			sp->actuator->sensor_peri = sp;

			if (i2c_ch < SENSOR_CONTROL_I2C_MAX) {
				sp->actuator->i2c_lock = &core->i2c_lock[i2c_ch];
				set_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sp->peri_state);

				minfo("%s[%d] enable actuator i2c client. position = %d\n",
					s, __func__, __LINE__, m->position);
			} else {
				mwarn("wrong actuator i2c_channel(%d)", s, i2c_ch);
			}
		} else {
			sp->subdev_actuator = NULL;
			sp->actuator = NULL;
		}
	}
}

static void set_flash_data(struct is_device_sensor *s,
				struct is_module_enum *m)
{
	struct is_device_sensor_peri *sp =
			(struct is_device_sensor_peri *)m->private_data;

	if (s->flash && m->pdata->flash_product_name == s->flash->id) {
		sp->subdev_flash = s->subdev_flash;
		sp->flash = s->flash;
		sp->flash->sensor_peri = sp;

		if (sp->flash)
			set_bit(IS_SENSOR_FLASH_AVAILABLE, &sp->peri_state);
	} else {
		sp->subdev_flash = NULL;
		sp->flash = NULL;
	}
}

static void set_ois_data(struct is_device_sensor *s,
				struct is_module_enum *m)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor_peri *sp =
			(struct is_device_sensor_peri *)m->private_data;
	u32 i2c_ch = m->pdata->ois_i2c_ch;

	if (s->ois && m->pdata->ois_product_name == s->ois->id) {
		sp->subdev_ois = s->subdev_ois;
		sp->ois = s->ois;
		sp->ois->sensor_peri = sp;

		if (i2c_ch < SENSOR_CONTROL_I2C_MAX)
			sp->ois->i2c_lock = &core->i2c_lock[i2c_ch];
		else
			mwarn("wrong ois i2c_channel(%d)", s, i2c_ch);

		if (sp->ois)
			set_bit(IS_SENSOR_OIS_AVAILABLE, &sp->peri_state);

		minfo("%s[%d] enable ois i2c client. position = %d\n", s,
				__func__, __LINE__, m->position);
	} else {
		sp->subdev_ois = NULL;
		sp->ois = NULL;
	}
}

static void set_mcu_data(struct is_device_sensor *s,
				struct is_module_enum *m)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor_peri *sp =
			(struct is_device_sensor_peri *)m->private_data;
	u32 i2c_ch = m->pdata->mcu_i2c_ch;

	if (s->mcu && m->pdata->mcu_product_name == s->mcu->name) {
		sp->subdev_mcu = s->subdev_mcu;
		sp->mcu = s->mcu;
		sp->mcu->sensor_peri = sp;

		if (s->mcu->ois) {
			sp->mcu->ois = s->mcu->ois;
			sp->mcu->subdev_ois = s->subdev_mcu;
			sp->mcu->ois->sensor_peri = sp;
		}

		if (s->mcu->aperture) {
			sp->mcu->aperture = s->mcu->aperture;
			sp->mcu->subdev_aperture = s->mcu->subdev_aperture;
			sp->mcu->aperture->sensor_peri = sp;
		}

		if (IS_ENABLED(USE_TELE_OIS_AF_COMMON_INTERFACE) && s->mcu->actuator) {
			sp->actuator = s->mcu->actuator;
			sp->subdev_actuator = s->mcu->actuator->subdev;
			sp->actuator->sensor_peri = sp;
			set_bit(IS_SENSOR_ACTUATOR_AVAILABLE, &sp->peri_state);
		}

		if (!IS_ENABLED(CONFIG_CAMERA_USE_INTERNAL_MCU)) {
			if (i2c_ch < SENSOR_CONTROL_I2C_MAX && s->mcu->ois)
				sp->mcu->ois->i2c_lock = &core->i2c_lock[i2c_ch];
			else
				mwarn("wrong mcu i2c_channel(%d)", s, i2c_ch);
		}

		/* need to be checked for mcu available. */
		if (s->mcu->ois)
			set_bit(IS_SENSOR_OIS_AVAILABLE, &sp->peri_state);

		if (IS_ENABLED(CONFIG_CAMERA_USE_APERTURE) && s->mcu->aperture)
			set_bit(IS_SENSOR_APERTURE_AVAILABLE, &sp->peri_state);

		minfo("%s[%d] enable mcu device. position = %d\n", s,
				__func__, __LINE__, m->position);
	} else {
		sp->subdev_mcu = NULL;
		sp->mcu = NULL;
	}
}

static void set_aperture_data(struct is_device_sensor *s,
				struct is_module_enum *m)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor_peri *sp =
			(struct is_device_sensor_peri *)m->private_data;
	u32 i2c_ch = m->pdata->aperture_i2c_ch;

	if (s->aperture && m->pdata->aperture_product_name == s->aperture->id) {
		sp->subdev_aperture = s->subdev_aperture;
		sp->aperture = s->aperture;
		sp->aperture->sensor_peri = sp;

		if (i2c_ch < SENSOR_CONTROL_I2C_MAX)
			sp->aperture->i2c_lock = &core->i2c_lock[i2c_ch];
		else
			mwarn("wrong aperture i2c_channel(%d)", s, i2c_ch);

		if (sp->aperture)
			set_bit(IS_SENSOR_APERTURE_AVAILABLE, &sp->peri_state);

		minfo("%s[%d] enable aperture i2c client. position = %d\n", s,
				__func__, __LINE__, m->position);
	} else {
		sp->subdev_aperture = NULL;
		sp->aperture = NULL;
	}

}

static void set_eeprom_data(struct is_device_sensor *s,
				struct is_module_enum *m)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor_peri *sp =
			(struct is_device_sensor_peri *)m->private_data;
	u32 i2c_ch = m->pdata->ois_i2c_ch;

	if (s->eeprom && m->pdata->ois_product_name == s->eeprom->id) {
		sp->subdev_eeprom = s->subdev_eeprom;
		sp->eeprom = s->eeprom;
		sp->eeprom->sensor_peri = sp;

		if (i2c_ch < SENSOR_CONTROL_I2C_MAX)
			sp->eeprom->i2c_lock = &core->i2c_lock[i2c_ch];
		else
			mwarn("wrong eeprom i2c_channel(%d)", s, i2c_ch);

		if (sp->eeprom)
			set_bit(IS_SENSOR_EEPROM_AVAILABLE, &sp->peri_state);

		minfo("%s[%d] enable eeprom i2c client. position = %d\n", s,
				__func__, __LINE__, m->position);
	} else {
		sp->subdev_eeprom = NULL;
		sp->eeprom = NULL;
	}
}

static void set_laser_af_data(struct is_device_sensor *s,
				struct is_module_enum *m)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor_peri *sp =
			(struct is_device_sensor_peri *)m->private_data;

	if (s->laser_af && m->pdata->laser_af_product_name == s->laser_af->id) {
		sp->subdev_laser_af = s->subdev_laser_af;
		sp->laser_af = s->laser_af;
		sp->laser_af->sensor_peri = sp;
		sp->laser_af->laser_lock = &core->laser_lock;
		sp->laser_af->active = false;


		info("%s[%d] laser_af position = %d\n",
			__func__, __LINE__, m->position);

		set_bit(IS_SENSOR_LASER_AF_AVAILABLE, &sp->peri_state);
	} else {
		sp->subdev_laser_af = NULL;
		sp->laser_af = NULL;
	}
}

static void init_awb_count(struct is_device_sensor *s, u32 pos)
{
	switch (pos) {
	case SENSOR_POSITION_REAR:
	case SENSOR_POSITION_REAR2:
	case SENSOR_POSITION_REAR3:
	case SENSOR_POSITION_REAR4:
		s->init_wb_cnt = INIT_AWB_COUNT_REAR;
		break;
	case SENSOR_POSITION_FRONT:
	case SENSOR_POSITION_FRONT2:
		s->init_wb_cnt = INIT_AWB_COUNT_FRONT;
		break;
	default:
		s->init_wb_cnt = 0; /* not operated */
		break;
	}
}

int is_sensor_s_input(struct is_device_sensor *device,
	u32 position,
	u32 scenario,
	u32 video_id,
	u32 stream_leader)
{
	int ret = 0;
	int ret_smc;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct is_core *core = is_get_is_core();
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_group *group;
	struct is_groupmgr *groupmgr;
	struct is_vender *vender;

	FIMC_BUG(!device);
	FIMC_BUG(!device->pdata);
	FIMC_BUG(!device->subdev_csi);

	if (test_bit(IS_SENSOR_S_INPUT, &device->state)) {
		merr("already s_input", device);
		ret = -EINVAL;
		goto p_err;
	}

	groupmgr = device->groupmgr;
	group = &device->group_sensor;

	ret = is_group_init(groupmgr, group, GROUP_INPUT_OTF, video_id, stream_leader);
	if (ret) {
		merr("is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_search_sensor_module_with_position(device, position, &module);
	if (ret)
		goto p_err;

	subdev_module = module->subdev;
	if (!subdev_module) {
		merr("subdev module is not probed", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev_csi = device->subdev_csi;

	device->position = module->position;
	device->image.framerate = min_t(u32, SENSOR_DEFAULT_FRAMERATE, module->max_framerate);
	device->image.window.offs_h = 0;
	device->image.window.offs_v = 0;
	device->image.window.width = module->pixel_width;
	device->image.window.height = module->pixel_height;
	device->image.window.o_width = device->image.window.width;
	device->image.window.o_height = device->image.window.height;

	if (!IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ)) {
		set_cis_data(device, module);
		set_actuator_data(device, module);
		set_flash_data(device, module);
		set_ois_data(device, module);
		set_mcu_data(device, module);
		set_aperture_data(device, module);
		set_eeprom_data(device, module);
		set_laser_af_data(device, module);
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	is_sensor_peri_init_work(sensor_peri);

	device->pdata->scenario = scenario;
	if (scenario == SENSOR_SCENARIO_NORMAL || scenario == SENSOR_SCENARIO_STANDBY)
		clear_bit(IS_SENSOR_ONLY, &device->state);
	else
		set_bit(IS_SENSOR_ONLY, &device->state);

	if (IS_ENABLED(SECURE_CAMERA_IRIS)) {
		ret = is_secure_func(core, device, IS_SECURE_CAMERA_IRIS,
				device->pdata->scenario, SMC_SECCAM_PREPARE);
		if (ret)
			goto p_err;
	}

	if (IS_ENABLED(INIT_AWB))
		init_awb_count(device, position);

	if (device->subdev_module) {
		mwarn("subdev_module is already registered", device);
		v4l2_device_unregister_subdev(device->subdev_module);
	}

	ret = v4l2_device_register_subdev(&device->v4l2_dev, subdev_module);
	if (ret) {
		merr("v4l2_device_register_subdev is fail(%d)", device, ret);
		goto p_err;
	}

	device->subdev_module = subdev_module;

	if (test_bit(IS_SENSOR_ONLY, &device->state)) {
		if (atomic_inc_return(&core->resourcemgr.qos_refcount) == 1)
			is_add_dvfs(core, START_DVFS_LEVEL);
	}

	vender = &core->vender;
	is_vender_sensor_s_input(vender, device->position);

	/* Sensor power on */
	if (!IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ)) {
		ret = is_sensor_gpio_on(device);
		if (ret) {
			merr("is_sensor_gpio_on is fail(%d)", device, ret);
			goto p_err;
		}
	}

#if !defined(USE_OFFLINE_PROCESSING)
	if (!IS_ENABLED(CONFIG_SECURE_CAMERA_USE) ||
			(core->scenario != IS_SCENARIO_SECURE)) {
		ret = v4l2_subdev_call(subdev_csi, core, s_power, 1);
		if (ret) {
			merr("v4l2_csi_call(s_power) is fail(%d)", device, ret);
			goto p_err;
		}

		set_bit(IS_SENSOR_S_POWER, &device->state);
	}
#endif

	/* release our wakelock if the secure camera is pre-opening */
	if (IS_ENABLED(CONFIG_SECURE_CAMERA_USE) &&
			(core->scenario == IS_SCENARIO_SECURE))
		pm_relax(&core->pdev->dev);

	ret = v4l2_subdev_call(subdev_csi, core, init, module->enum_id);
	if (ret) {
		merr("v4l2_csi_call(init) is fail(%d)", device, ret);
		goto p_err;
	}

	if (IS_ENABLED(USE_CAMERA_SENSOR_RETENTION))
		is_vender_check_retention(vender, module);

	ret = v4l2_subdev_call(subdev_module, core, init, 0);
	if (ret) {
		merr("v4l2_module_call(init) is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_devicemgr_binding(device->devicemgr, device,
			group->device, IS_DEVICE_SENSOR);
	if (ret) {
		merr("is_devicemgr_binding is fail", device);
		goto p_err;
	}

	set_bit(device->position, &core->sensor_map);
	set_bit(IS_SENSOR_S_INPUT, &device->state);

p_err:
	if (IS_ENABLED(SECURE_CAMERA_IRIS) && ret)
		ret_smc = is_secure_func(NULL, device, IS_SECURE_CAMERA_IRIS,
			device->pdata->scenario, SMC_SECCAM_UNPREPARE);

	minfo("[SS%d:D] %s(pos:%d, sce:%d)(pos:%d)-%d\n", device, device->device_id,
		__func__, position, scenario, device->position, ret);

	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_s_input);

static int is_sensor_s_fmt(void *device, struct is_queue *iq)
{
	int ret = 0;
	struct is_device_sensor *ids = (struct is_device_sensor *)device;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct v4l2_subdev_format subdev_format;
	struct v4l2_subdev_pad_config pad_cfg;
	struct is_fmt *format;
	u32 width;
	u32 height;
	struct is_group *group;

	FIMC_BUG(!ids->subdev_module);
	FIMC_BUG(!ids->subdev_csi);

	subdev_module = ids->subdev_module;
	subdev_csi = ids->subdev_csi;
	format = iq->framecfg.format;
	width = ids->sensor_width;
	height = ids->sensor_height;

	memcpy(&ids->image.format, format, sizeof(struct is_fmt));
	ids->image.window.width = width;
	ids->image.window.o_width = width;
	ids->image.window.height = height;
	ids->image.window.o_height = height;

	subdev_format.format.code = format->pixelformat;
	subdev_format.format.field = format->field;
	subdev_format.format.width = width;
	subdev_format.format.height = height;
	subdev_format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	subdev_format.pad = 0;

	ids->cfg = is_sensor_g_mode(ids);
	if (!ids->cfg) {
		merr("sensor cfg is invalid", ids);
		ret = -EINVAL;
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_module, pad, set_fmt, &pad_cfg, &subdev_format);
	if (ret) {
		merr("v4l2_module_call(s_format) is fail(%d)", ids, ret);
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_csi, pad, set_fmt, &pad_cfg, &subdev_format);
	if (ret) {
		merr("v4l2_csi_call(s_format) is fail(%d)", ids, ret);
		goto p_err;
	}

	/* In only sensor group, VOTF path is determined by sensor mode. */
	group = &ids->group_sensor;
	if (ids->cfg->votf && !test_bit(IS_SENSOR_ONLY, &ids->state)) {
		set_bit(IS_GROUP_VOTF_OUTPUT, &group->state);
		mginfo("VOTF master path is set\n", group, group);

		if (group->next) {
			if (test_bit(IS_GROUP_OTF_INPUT, &group->next->state)) {
				set_bit(IS_GROUP_VOTF_INPUT, &group->next->state);
				mginfo("VOTF slave path is set\n", group->next, group->next);
			}
		}
	} else {
		clear_bit(IS_GROUP_VOTF_OUTPUT, &group->state);
		mginfo("VOTF master path is clear\n", group, group);

		if (group->next) {
			if (test_bit(IS_GROUP_OTF_INPUT, &group->next->state)) {
				clear_bit(IS_GROUP_VOTF_INPUT, &group->next->state);
				mginfo("VOTF slave path is clear\n", group->next, group->next);
			}
		}
	}

p_err:
	return ret;
}

int is_sensor_s_framerate(struct is_device_sensor *device,
	struct v4l2_streamparm *param)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct is_module_enum *module;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tpf;
	u32 framerate = 0;

	FIMC_BUG(!device);
	FIMC_BUG(!device->subdev_module);
	FIMC_BUG(!device->subdev_csi);
	FIMC_BUG(!param);

	cp = &param->parm.capture;
	tpf = &cp->timeperframe;

	if (!tpf->numerator) {
		merr("numerator is 0", device);
		ret = -EINVAL;
		goto p_err;
	}

	framerate = tpf->denominator / tpf->numerator;
	subdev_module = device->subdev_module;
	subdev_csi = device->subdev_csi;

	if (framerate == 0) {
		mwarn("frame rate 0 request is ignored", device);
		goto p_err;
	}

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev_module);
	if (!module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
		merr("front is already stream on", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		merr("type is invalid(%d)", device, param->type);
		ret = -EINVAL;
		goto p_err;
	}

	if (framerate > module->max_framerate) {
		merr("framerate is invalid(%d > %d)", device, framerate, module->max_framerate);
		ret = -EINVAL;
		goto p_err;
	}

	device->image.framerate = framerate;
	device->max_target_fps = framerate;

	minfo("[SS%d:D] framerate: req@%dfps, cur@%dfps\n", device, device->device_id,
		framerate, device->image.framerate);

p_err:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_s_framerate);

int is_sensor_s_ctrl(struct is_device_sensor *device,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;

	FIMC_BUG(!device);
	FIMC_BUG(!device->subdev_module);
	FIMC_BUG(!device->subdev_csi);
	FIMC_BUG(!ctrl);

	subdev_module = device->subdev_module;

	subdev_csi = device->subdev_csi;
	ret = v4l2_subdev_call(subdev_csi, core, ioctl, ctrl->id, ctrl);
	if (ret) {
		mwarn("v4l2_csi_call(ioctl) is fail(%d)", device, ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_module, core, ioctl, SENSOR_IOCTL_MOD_S_CTRL, ctrl);
	if (ret) {
		err("s_ctrl is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_sensor_s_ext_ctrls(struct is_device_sensor *device,
	struct v4l2_ext_controls *ctrls)
{
	int ret;
	struct v4l2_subdev *subdev_module = device->subdev_module;

	FIMC_BUG(!subdev_module);

	ret = v4l2_subdev_call(subdev_module, core, ioctl, SENSOR_IOCTL_MOD_S_EXT_CTRL, ctrls);
	if (ret) {
		err("s_ext_ctrls is fail(%d)", ret);
		return ret;
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_s_ext_ctrls);

int is_sensor_g_ext_ctrls(struct is_device_sensor *device,
			  struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct v4l2_ext_control *ext_ctrl;
#if defined(USE_OFFLINE_PROCESSING)
	struct offline_data off_data;
	struct dma_buf *dbuf;
	struct is_hardware *hardware;
#endif

	FIMC_BUG(!device);
	FIMC_BUG(!device->subdev_module);
	FIMC_BUG(!device->subdev_csi);
	FIMC_BUG(!ctrls);

	subdev_module = device->subdev_module;

	ext_ctrl = ctrls->controls;

	switch (ext_ctrl->id) {
#if defined(USE_OFFLINE_PROCESSING)
	case V4L2_CID_IS_G_OFFLINE_DATA:
		ret = copy_from_user(&off_data, ext_ctrl->ptr,
				sizeof(struct offline_data));
		if (ret) {
			err("fail to copy_from_user, ret(%d)\n", ret);
			goto p_err;
		}

		dbuf = dma_buf_get(off_data.buf_fd);
		if (IS_ERR_OR_NULL(dbuf)) {
			err("failed to get dmabuf of fd: %d\n",
					off_data.buf_fd);
			ret = -EINVAL;
			goto p_err;
		}

		if (dma_buf_begin_cpu_access(dbuf, DMA_BIDIRECTIONAL)) {
			dma_buf_put(dbuf);
			err("failed to access dmabuf of fd: %d\n",
					off_data.buf_fd);
			ret = -EINVAL;
			goto p_err;
		}

		off_data.data_desc.base_kvaddr = (ulong)dma_buf_vmap(dbuf);

		if (!off_data.data_desc.base_kvaddr) {
			dma_buf_end_cpu_access(dbuf, DMA_BIDIRECTIONAL);
			dma_buf_put(dbuf);
			err("failed to kmapping dmabuf of fd: %d\n",
					off_data.buf_fd);
			ret = -EINVAL;
			goto p_err;
		}

		FIMC_BUG(!device->ischain);
		FIMC_BUG(!device->ischain->hardware);

		hardware = device->ischain->hardware;

		ret = is_hardware_get_offline_data(hardware,
				device->instance,
				&device->group_sensor,
				&off_data.data_desc,
				off_data.fcount);
		if (ret) {
			err("fail to get data_desc(%d)", ret);
		} else {
			ret = copy_to_user(ext_ctrl->ptr, &off_data, sizeof(struct offline_data));
			if (ret)
				err("fail to copy_to_user, ret(%d)\n", ret);
		}
		dma_buf_vunmap(dbuf, (void *)off_data.data_desc.base_kvaddr);
		dma_buf_end_cpu_access(dbuf, 0);
		dma_buf_put(dbuf);
		break;
#endif
	default:
		ret = v4l2_subdev_call(subdev_module, core, ioctl, SENSOR_IOCTL_MOD_G_EXT_CTRL, ctrls);
		if (ret) {
			err("s_ext_ctrls is fail(%d)", ret);
			goto p_err;
		}
	}
p_err:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_g_ext_ctrls);

int is_sensor_s_bns(struct is_device_sensor *device,
	u32 width, u32 height)
{
	FIMC_BUG(!device);

	if (!width || !height) {
		merr("BNS size is zero\n", device);
		return -EINVAL;
	}

	device->image.window.otf_width = width;
	device->image.window.otf_height = height;

	return 0;
}

int is_sensor_s_frame_duration(struct is_device_sensor *device,
	u32 framerate)
{
	int ret = 0;
	u64 frame_duration;
	struct v4l2_subdev *subdev_module;
	struct is_module_enum *module;

	FIMC_BUG(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* unit : nano */
	frame_duration = (1000 * 1000 * 1000) / framerate;
	if (frame_duration <= 0) {
		err("it is wrong frame duration(%lld)", frame_duration);
		ret = -EINVAL;
		goto p_err;
	}

	if (device->frame_duration != frame_duration) {
		CALL_MOPS(module, s_duration, subdev_module, frame_duration);
		device->frame_duration = frame_duration;
	}

p_err:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_s_frame_duration);

int is_sensor_s_exposure_time(struct is_device_sensor *device,
	u32 exposure_time)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct is_module_enum *module;

	FIMC_BUG(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (exposure_time <= 0) {
		err("it is wrong exposure time (%d)", exposure_time);
		ret = -EINVAL;
		goto p_err;
	}

	if (device->exposure_time != exposure_time) {
		CALL_MOPS(module, s_exposure, subdev_module, exposure_time);
		device->exposure_time = exposure_time;
	}
p_err:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_s_exposure_time);

int is_sensor_s_again(struct is_device_sensor *device,
	u32 gain)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct is_module_enum *module;

	FIMC_BUG(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (gain <= 0) {
		err("it is wrong gain (%d)", gain);
		ret = -EINVAL;
		goto p_err;
	}

	if (!module->ops) {
		err("ops is null, do nothing");
		goto p_err;
	}

	CALL_MOPS(module, s_again, subdev_module, gain);

p_err:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_s_again);

int is_sensor_s_shutterspeed(struct is_device_sensor *device,
	u32 shutterspeed)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct is_module_enum *module;

	FIMC_BUG(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (shutterspeed <= 0) {
		err("it is wrong gain (%d)", shutterspeed);
		ret = -EINVAL;
		goto p_err;
	}

	CALL_MOPS(module, s_shutterspeed, subdev_module, shutterspeed);

p_err:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_s_shutterspeed);

int is_sensor_g_ctrl(struct is_device_sensor *device,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;

	FIMC_BUG(!device);
	FIMC_BUG(!device->subdev_module);
	FIMC_BUG(!device->subdev_csi);
	FIMC_BUG(!ctrl);

	subdev_module = device->subdev_module;

	ret = v4l2_subdev_call(subdev_module, core, ioctl, SENSOR_IOCTL_MOD_G_CTRL, ctrl);
	if (ret) {
		err("g_ctrl is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}


int is_sensor_g_instance(struct is_device_sensor *device)
{
	FIMC_BUG(!device);
	return device->device_id;
}
KUNIT_EXPORT_SYMBOL(is_sensor_g_instance);

int is_sensor_g_fcount(struct is_device_sensor *device)
{
	FIMC_BUG(!device);
	return device->fcount;
}

int is_sensor_g_ex_mode(struct is_device_sensor *device)
{
	FIMC_BUG(!device);
	FIMC_BUG(!device->cfg);
	return device->cfg->ex_mode;
}
EXPORT_SYMBOL_GPL(is_sensor_g_ex_mode);

int is_sensor_g_framerate(struct is_device_sensor *device)
{
	FIMC_BUG(!device);
	return device->image.framerate;
}

int is_sensor_g_width(struct is_device_sensor *device)
{
	FIMC_BUG(!device);
	return device->image.window.width;
}

int is_sensor_g_height(struct is_device_sensor *device)
{
	FIMC_BUG(!device);
	return device->image.window.height;
}

int is_sensor_g_bns_width(struct is_device_sensor *device)
{
	FIMC_BUG(!device);

	if (device->image.window.otf_width)
		return device->image.window.otf_width;

	return device->image.window.width;
}

int is_sensor_g_bns_height(struct is_device_sensor *device)
{
	FIMC_BUG(!device);
	if (device->image.window.otf_height)
		return device->image.window.otf_height;

	return device->image.window.height;
}

int is_sensor_g_bns_ratio(struct is_device_sensor *device)
{
	int binning = 0;
	u32 sensor_width, sensor_height;
	u32 bns_width, bns_height;

	FIMC_BUG(!device);

	sensor_width = is_sensor_g_width(device);
	sensor_height = is_sensor_g_height(device);
	bns_width = is_sensor_g_bns_width(device);
	bns_height = is_sensor_g_bns_height(device);

	binning = min(BINNING(sensor_width, bns_width),
		BINNING(sensor_height, bns_height));

	return binning;
}

int is_sensor_g_bratio(struct is_device_sensor *device)
{
	struct is_module_enum *module;
	struct v4l2_control ctrl;
	int ret = 0;

	FIMC_BUG(!device);
	FIMC_BUG(!device->subdev_module);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (!module) {
		merr("module is NULL", device);
		goto p_err;
	}

	if (!module->pdata) {
		merr("pdata is NULL", device);
		goto p_err;
	}

	/* TODO: This is will be removed. */
	if (module->pdata->use_binning_ratio_table) {
		ctrl.id = V4L2_CID_SENSOR_GET_BINNING_RATIO;
		ret = v4l2_subdev_call(device->subdev_module, core, ioctl, SENSOR_IOCTL_MOD_G_CTRL, &ctrl);
		if (ret)
			err("g_ctrl is fail(%d), need to add binning ratio info at cis setfile", ret);
		else if (!ctrl.value)
			merr("binning ratio is need to be bigger than zero", device);
		else
			return ctrl.value;
	}

p_err:
	return device->cfg->binning;
}

int is_sensor_g_sensorcrop_bratio(struct is_device_sensor *device)
{
	int binning = 0;
	struct is_module_enum *module;

	FIMC_BUG(!device);
	FIMC_BUG(!device->subdev_module);

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (!module) {
		merr("module is NULL", device);
		goto p_err;
	}

	/* remosaic crop size is 2 times tetra size */
	binning = min(BINNING(module->active_width, device->image.window.width * 2),
		BINNING(module->active_height, device->image.window.height * 2));
	/* sensor binning only support natural number */
	binning = (binning / 1000) * 1000;

p_err:
	return binning;
}

int is_sensor_g_position(struct is_device_sensor *device)
{
	return device->position;
}
KUNIT_EXPORT_SYMBOL(is_sensor_g_position);

int is_sensor_g_csis_error(struct is_device_sensor *device)
{
	int ret = 0;
	u32 errorCode = 0;
	struct v4l2_subdev *subdev_csi;

	subdev_csi = device->subdev_csi;
	if (unlikely(!subdev_csi)) {
		merr("subdev_csi is NULL", device);
		return -EINVAL;
	}

	ret = v4l2_subdev_call(subdev_csi, video, g_input_status, &errorCode);
	if (ret) {
		merr("v4l2_csi_call(s_format) is fail(%d)", device, ret);
		return -EINVAL;
	}

	return errorCode;
}
KUNIT_EXPORT_SYMBOL(is_sensor_g_csis_error);

int is_sensor_g_fast_mode(struct is_device_sensor *device)
{
	u32 ex_mode;
	u32 framerate;
	u32 height;

	/*
	 * Since CSTAT/3AA driver directly sets config_lock event timing,
	 * it doesn't use 'early_config_lock' feature for fast sensor mode.
	 * It gets the enough CSTAT/3AA vvalid time by setting internal corex_delay.
	 * Therefore, there should not such case that fast_mode is set into true.
	 */
	if (IS_ENABLED(CSTAT_DDK_LIB_CALL) ||
			IS_ENABLED(TAA_DDK_LIB_CALL))
		return 0;

	FIMC_BUG(!device);
	FIMC_BUG(!device->cfg);

	ex_mode = is_sensor_g_ex_mode(device);
	framerate = device->cfg->framerate;
	height = is_sensor_g_height(device);

	if (test_bit(IS_SENSOR_OTF_OUTPUT, &device->state) &&
		(ex_mode == EX_DUALFPS_960 ||
		ex_mode == EX_DUALFPS_480 ||
		framerate >= 240 ||
		height <= LINE_FOR_SHOT_VALID_TIME))
		return 1;
	else
		return 0;
}

int __nocfi is_sensor_register_itf(struct is_device_sensor *device)
{
	int ret = 0;
	struct v4l2_subdev *subdev = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct exynos_platform_is_module *pdata = NULL;
	register_sensor_interface register_sensor_itf = NULL;

	FIMC_BUG(!device);

	subdev = device->subdev_module;
	FIMC_BUG(!subdev);

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	FIMC_BUG(!sensor_peri);

	pdata = module->pdata;
	FIMC_BUG(!pdata);

	if (!IS_ENABLED(SKIP_LIB_LOAD)) {
		register_sensor_itf =
			(register_sensor_interface)SENSOR_REGISTER_FUNC_ADDR;

		is_fpsimd_get_func();
		ret = register_sensor_itf((void *)&sensor_peri->sensor_interface);
		is_fpsimd_put_func();
		if (ret < 0) {
			merr("[SS%d][%s] register_sensor_itf(lib) failed(%d)",
				device, device->device_id, module->sensor_name,
				ret);
			goto p_err;
		}

#ifdef USE_RTA_BINARY
		register_sensor_itf = (register_sensor_interface)SENSOR_REGISTER_FUNC_ADDR_RTA;

		is_fpsimd_get_func();
		ret = register_sensor_itf((void *)&sensor_peri->sensor_interface);
		is_fpsimd_put_func();
		if (ret < 0) {
			merr("[SS%d][%s] register_sensor_itf(rta) failed(%d)",
				device, device->device_id, module->sensor_name,
				ret);
			goto p_err;
		}
#endif
	}

p_err:
	return ret;
}

void is_sensor_s_batch_mode(struct is_device_sensor *device, struct is_frame *frame)
{
	struct is_device_csi *csi = v4l2_get_subdevdata(device->subdev_csi);

	csi->otf_batch_num = frame ? frame->num_buffers : 1;

	/* 'f_id_dec' flag must be updated on sensor s_fmt in advance. */
	if (!csi->f_id_dec)
		csi->dma_batch_num = csi->otf_batch_num;
}

int is_sensor_group_tag(struct is_device_sensor *device,
	struct is_frame *frame,
	struct camera2_node *ldr_node)
{
	int ret = 0;
	u32 capture_id;
	struct is_group *group;
	struct is_subdev *subdev;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_sensor;
	node_group = &frame->shot_ext->node_group;

	ret = CALL_SOPS(&group->leader, tag, device, frame, ldr_node);
	if (ret) {
		merr("is_sensor_group_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;

		switch (cap_node->vid) {
		case 0:
			break;
		case IS_VIDEO_SS0VC0_NUM:
		case IS_VIDEO_SS1VC0_NUM:
		case IS_VIDEO_SS2VC0_NUM:
		case IS_VIDEO_SS3VC0_NUM:
		case IS_VIDEO_SS4VC0_NUM:
		case IS_VIDEO_SS5VC0_NUM:
			subdev = group->subdev[ENTRY_SSVC0];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_sensor_group_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_SS0VC1_NUM:
		case IS_VIDEO_SS1VC1_NUM:
		case IS_VIDEO_SS2VC1_NUM:
		case IS_VIDEO_SS3VC1_NUM:
		case IS_VIDEO_SS4VC1_NUM:
		case IS_VIDEO_SS5VC1_NUM:
			subdev = group->subdev[ENTRY_SSVC1];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_sensor_group_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_SS0VC2_NUM:
		case IS_VIDEO_SS1VC2_NUM:
		case IS_VIDEO_SS2VC2_NUM:
		case IS_VIDEO_SS3VC2_NUM:
		case IS_VIDEO_SS4VC2_NUM:
		case IS_VIDEO_SS5VC2_NUM:
			subdev = group->subdev[ENTRY_SSVC2];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_sensor_group_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case IS_VIDEO_SS0VC3_NUM:
		case IS_VIDEO_SS1VC3_NUM:
		case IS_VIDEO_SS2VC3_NUM:
		case IS_VIDEO_SS3VC3_NUM:
		case IS_VIDEO_SS4VC3_NUM:
		case IS_VIDEO_SS5VC3_NUM:
			subdev = group->subdev[ENTRY_SSVC3];
			if (subdev && test_bit(IS_SUBDEV_START, &subdev->state)) {
				ret = CALL_SOPS(subdev, tag, device, frame, cap_node);
				if (ret) {
					merr("is_sensor_group_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		default:
			break;
		}
	}

p_err:
	return ret;
}

static int is_sensor_back_start(void *device, struct is_queue *iq)
{
	int ret = 0;
	int vc;
	struct v4l2_subdev *subdev_csi;
	struct is_device_sensor *ids = device;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct is_device_csi *csi;
	struct is_core *core;

	core = ids->private_data;

	if (test_bit(IS_SENSOR_BACK_START, &ids->state)) {
		err("already back start");
		ret = -EINVAL;
		goto p_err;
	}

	ret = v4l2_subdev_call(ids->subdev_module, video,
				s_routing, 0, 0, 1);
	if (ret) {
		merr("failed at s_routing for module(%d)", ids, ret);
		goto p_err;
	}

#if !defined(USE_OFFLINE_PROCESSING)
	if (IS_ENABLED(CONFIG_SECURE_CAMERA_USE) &&
			(core->scenario == IS_SCENARIO_SECURE) &&
			!test_bit(IS_SENSOR_S_POWER, &ids->state)) {
		/* re-enable our wakelock */
		pm_stay_awake(&core->pdev->dev);

		ret = v4l2_subdev_call(ids->subdev_csi, core, s_power, 1);
		if (ret) {
			merr("v4l2_csi_call(s_power) is fail(%d)", ids, ret);
			goto p_err;
		}

		set_bit(IS_SENSOR_S_POWER, &ids->state);
	}
#endif

	groupmgr = ids->groupmgr;
	group = &ids->group_sensor;

	ret = is_group_start(groupmgr, group);
	if (ret) {
		merr("is_group_start is fail(%d)", ids, ret);
		goto p_err;
	}

	if (test_bit(IS_GROUP_VOTF_OUTPUT, &group->state)) {
		subdev_csi = ids->subdev_csi;
		ret = v4l2_subdev_call(subdev_csi, core, ioctl, SENSOR_IOCTL_REGISTE_VOTF, NULL);
		if (ret) {
			mwarn("v4l2_csi_call(ioctl: SENSOR_IOCTL_REGISTE_VOTF) is fail(%d)", ids, ret);
			goto p_err;
		}
	}

	if (IS_ENABLED(SECURE_CAMERA_FACE)) {
		ret = is_secure_func(core, ids, IS_SECURE_CAMERA_FACE,
				ids->ex_scenario, SMC_SECCAM_PREPARE);
		if (ret)
			goto p_err;
	}

	ret = is_devicemgr_start(ids->devicemgr, (void *)ids, IS_DEVICE_SENSOR);
	if (ret) {
		merr("is_devicemgr_start is fail(%d)", ids, ret);
		goto p_err_after_smc;
	}

	csi = v4l2_get_subdevdata(ids->subdev_csi);
	if (!csi) {
		merr("CSI is NULL", ids);
		ret = -EINVAL;
		goto p_err_after_smc;
	}

	/* Registeration of DMA IRQ */
	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		struct is_subdev *dma_subdev;

		dma_subdev = csi->dma_subdev[vc];
		if (!dma_subdev)
			continue;

		if (!test_bit(IS_SUBDEV_OPEN, &dma_subdev->state))
			continue;

		if (test_bit(IS_SUBDEV_INTERNAL_USE, &dma_subdev->state)
			&& test_bit(IS_SUBDEV_INTERNAL_S_FMT, &dma_subdev->state)) {
			ret = is_subdev_internal_start((void *)ids, IS_DEVICE_SENSOR, dma_subdev);
			if (ret) {
				merr("[CSI%d] VC%d subdev internal start is fail(%d)", csi, csi->ch, vc, ret);
				ret = -EINVAL;
				goto p_err_internal_start;
			}
		}
	}

	set_bit(IS_SENSOR_BACK_START, &ids->state);

	minfo("[SS%d:D] %s(%dx%d, %d)\n", ids, ids->device_id, __func__,
		ids->image.window.width, ids->image.window.height, ret);

	return 0;

p_err_internal_start:
	while (--vc >= CSI_VIRTUAL_CH_0) {
		struct is_subdev *dma_subdev;

		dma_subdev = csi->dma_subdev[vc];
		if (!dma_subdev)
			continue;

		if (!test_bit(IS_SUBDEV_OPEN, &dma_subdev->state))
			continue;

		if (test_bit(IS_SUBDEV_INTERNAL_USE, &dma_subdev->state)
			&& test_bit(IS_SUBDEV_INTERNAL_S_FMT, &dma_subdev->state))
			is_subdev_internal_stop((void *)ids, IS_DEVICE_SENSOR, dma_subdev);
	}

p_err_after_smc:
	if (IS_ENABLED(SECURE_CAMERA_FACE))
		is_secure_func(core, NULL, IS_SECURE_CAMERA_FACE,
				ids->ex_scenario, SMC_SECCAM_UNPREPARE);

p_err:
	merr("[SS%d:D] %s(%dx%d, %d)\n", ids, ids->device_id, __func__,
		ids->image.window.width, ids->image.window.height, ret);

	return ret;
}

static int is_sensor_back_stop(void *device, struct is_queue *iq)
{
	int ret = 0;
	int vc;
	struct is_device_sensor *ids = device;
	struct is_groupmgr *groupmgr;
	struct is_group *group;

	FIMC_BUG(!ids);

	groupmgr = ids->groupmgr;
	group = &ids->group_sensor;

	if (!test_bit(IS_SENSOR_BACK_START, &ids->state)) {
		mwarn("already back stop", ids);
		goto p_err;
	}

	/*
	 * If OTF case, skip force buffer done for sensor leader's frame
	 * because force buffer done will be done in process_stop sequence.
	 */
	is_sensor_group_force_stop(ids, ids->group_sensor.id);

	ret = is_group_stop(groupmgr, group);
	if (ret)
		merr("is_group_stop is fail(%d)", ids, ret);

	for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
		struct is_device_csi *csi;
		struct is_subdev *dma_subdev;

		csi = v4l2_get_subdevdata(ids->subdev_csi);
		if (!csi) {
			merr("CSI is NULL", ids);
			return -EINVAL;
		}

		dma_subdev = csi->dma_subdev[vc];
		if (!dma_subdev)
			continue;

		if (test_bit(IS_SUBDEV_INTERNAL_USE, &dma_subdev->state)
			&& test_bit(IS_SUBDEV_INTERNAL_S_FMT, &dma_subdev->state)) {
			ret = is_subdev_internal_stop((void *)ids, IS_DEVICE_SENSOR, dma_subdev);
			if (ret)
				merr("[CSI] VC%d subdev internal start is fail(%d)", csi, vc, ret);
		}
	}

	ret = is_devicemgr_stop(ids->devicemgr, (void *)ids, IS_DEVICE_SENSOR);
	if (ret)
		merr("is_devicemgr_stop is fail(%d)", ids, ret);

	ret = v4l2_subdev_call(ids->subdev_module, video,
				s_routing, 0, 0, 0);
	if (ret)
		merr("failed at s_routing for module(%d)", ids, ret);

	clear_bit(IS_SENSOR_BACK_START, &ids->state);
p_err:
	minfo("[BAK:D] %s():%d\n", ids, __func__, ret);
	return ret;
}

int is_sensor_standby_flush(struct is_device_sensor *device)
{
	struct is_group *ss_grp, *group, *child;
	int ret = 0;

	group = ss_grp = &device->group_sensor;

	/* Sensor group ischain device process stop */
	set_bit(IS_GROUP_STANDBY, &group->state);
	set_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
	ret = is_group_stop(device->groupmgr, group);
	if (ret == -EPERM) {
		mgerr("group is already stop(%d), skip start sequence",
				group, group, ret);
		goto p_skip;
	} else if (ret != 0) {
		mgerr("is_group_stop is fail(%d)", group, group, ret);
	}

	/* Sensor group ischain device process start for being standby */
	group = ss_grp;
	while (group) {
		child = group;
		while (child) {
			ret = is_itf_process_start(device->ischain, GROUP_ID(child->id));
			if (ret)
				mgerr("is_itf_process_start is fail(%d)", child, child, ret);

			child = child->child;
		}

		ret = is_group_start(device->groupmgr, group);
		if (ret) {
			merr("is_group_start is fail(%d)", device, ret);
			goto p_skip;
		}

		/* Try for multi-channel ischain device group */
		group = group->pnext;
	}

	mginfo("%s() done", device, ss_grp, __func__);

p_skip:
	return ret;
}

int is_sensor_front_start(struct is_device_sensor *device,
	u32 instant_cnt,
	u32 nonblock)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct is_module_enum *module;

	FIMC_BUG(!device);
	FIMC_BUG(!device->pdata);
	FIMC_BUG(!device->subdev_csi);

	if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
		merr("already front start", device);
		ret = -EINVAL;
		goto p_err;
	}

	memset(device->timestamp, 0x0, IS_TIMESTAMP_HASH_KEY * sizeof(u64));
	memset(device->timestampboot, 0x0, IS_TIMESTAMP_HASH_KEY * sizeof(u64));
	memset(device->frame_id, 0x0, IS_TIMESTAMP_HASH_KEY * sizeof(u64));
	device->instant_cnt = instant_cnt;
	subdev_csi = device->subdev_csi;
	subdev_module = device->subdev_module;
	if (!subdev_module) {
		merr("subdev_module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	module = (struct is_module_enum *)v4l2_get_subdevdata(subdev_module);
	if (!module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (IS_ENABLED(USE_CAMIF_FIX_UP)) {
		ret = is_hw_camif_fix_up(device);
		if (ret) {
			merr("failed to fix up CAM I/F", device);
			ret = -EINVAL;
			goto p_err;
		}
	}

	/* Actuator Init because actuator init use cal data */
	ret = v4l2_subdev_call(device->subdev_module, core, ioctl, V4L2_CID_SENSOR_NOTIFY_ACTUATOR_INIT, 0);
	if (ret) {
		merr("Actuator init fail **after first init done**\n", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_csi, video, s_stream, IS_ENABLE_STREAM);
	if (ret) {
		merr("v4l2_csi_call(s_stream) is fail(%d)", device, ret);
		goto p_err;
	}

	mdbgd_sensor("%s(%s, csi ch : %d, size : %d x %d)\n", device,
		__func__,
		module->sensor_name,
		device->pdata->csi_ch,
		device->image.window.width,
		device->image.window.height);

	if (nonblock) {
		schedule_work(&device->instant_work);
	} else {
		is_sensor_instanton(&device->instant_work);
		if (device->instant_ret) {
			merr("is_sensor_instanton is fail(%d)", device, device->instant_ret);
			ret = device->instant_ret;
			goto p_err;
		}
	}

p_err:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_front_start);

int is_sensor_front_stop(struct is_device_sensor *device, bool fstop)
{
	int ret = 0;
	struct v4l2_subdev *subdev_csi;
	struct is_core *core;

	FIMC_BUG(!device);

	core = (struct is_core *)device->private_data;

	mutex_lock(&device->mlock_state);

	if (test_and_clear_bit(SENSOR_MODULE_GOT_INTO_TROUBLE, &device->state)) {
		mwarn("sensor module have got into trouble", device);
		goto reset_the_others;
	}

	if (!test_bit(IS_SENSOR_FRONT_START, &device->state)) {
		mwarn("already front stop", device);
		goto already_stopped;
	}

	subdev_csi = device->subdev_csi;

	ret = is_sensor_stop(device);
	if (ret)
		merr("sensor stream off is failed(%d)\n", device, ret);

	ret = v4l2_subdev_call(subdev_csi, video, s_stream, IS_DISABLE_STREAM);
	if (ret)
		merr("v4l2_csi_call(s_stream) is fail(%d)", device, ret);

	clear_bit(IS_SENSOR_FRONT_START, &device->state);

reset_the_others:
	if (!fstop && device->use_standby)
		is_sensor_standby_flush(device);

	if (device->snr_check) {
		device->snr_check = false;
		if (timer_pending(&device->snr_timer))
			del_timer_sync(&device->snr_timer);
	}

already_stopped:
	minfo("[FRT:D] %s():%d\n", device, __func__, ret);

	mutex_unlock(&device->mlock_state);

	device->ex_mode = 0;
	device->ex_mode_extra = 0;
	device->ex_mode_format = 0;
	device->seamless_state = 0L;

	return ret;
}

void is_sensor_group_force_stop(struct is_device_sensor *device, u32 group_id)
{
	unsigned long flags;
	struct is_frame *frame;
	struct is_group *group;
	struct is_framemgr *framemgr;
	struct is_video_ctx *vctx;

	FIMC_BUG_VOID(!device);

	/* if OTF case, skip force stop */
	if (test_bit(IS_SENSOR_OTF_OUTPUT, &device->state))
		return;

	switch (group_id) {
	case GROUP_ID_SS0:
	case GROUP_ID_SS1:
	case GROUP_ID_SS2:
	case GROUP_ID_SS3:
	case GROUP_ID_SS4:
	case GROUP_ID_SS5:
		group = &device->group_sensor;
		break;
	default:
		group = NULL;
		break;
	}

	FIMC_BUG_VOID(!group);

	if (!group->head) {
		mwarn("group->head is NULL", device);
		return;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(&group->head->leader);
	FIMC_BUG_VOID(!framemgr);

	vctx = group->head->leader.vctx;

	framemgr_e_barrier_irqs(framemgr, 0, flags);
	frame = peek_frame(framemgr, FS_PROCESS);
	while (frame) {
		mgrinfo("[ERR] NDONE(%d, E%X)\n", group, group, frame, frame->index, IS_SHOT_UNPROCESSED);

		clear_bit(group->leader.id, &frame->out_flag);
		is_group_done(device->groupmgr, group, frame, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		CALL_VOPS(vctx, done, frame->index, VB2_BUF_STATE_ERROR);

		frame = peek_frame(framemgr, FS_PROCESS);
	}

	framemgr_x_barrier_irqr(framemgr, 0, flags);

	minfo("[FRT:D] %s()\n", device, __func__);
}

static struct is_queue_ops is_sensor_device_qops = {
	.start_streaming	= is_sensor_back_start,
	.stop_streaming		= is_sensor_back_stop,
	.s_fmt			= is_sensor_s_fmt,
};

struct is_queue_ops *is_get_sensor_device_qops(void)
{
	return &is_sensor_device_qops;
}
KUNIT_EXPORT_SYMBOL(is_get_sensor_device_qops);

static int is_sensor_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct is_device_sensor *device;
	struct exynos_platform_is_sensor *pdata;

	is_vendor_sensor_suspend(pdev);

	if (!is_sensor_g_device(pdev, &device)) {
		pdata = device->pdata;
		pdata->mclk_force_off(dev, 0);
	}

	info("%s\n", __func__);

	return 0;
}

static int is_sensor_resume(struct device *dev)
{
	info("%s\n", __func__);

	return 0;
}

int is_sensor_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct is_device_sensor *device;
	struct is_module_enum *module;
	struct v4l2_subdev *subdev_csi;

	device = NULL;
	module = NULL;

	ret = is_sensor_g_device(pdev, &device);
	if (ret) {
		err("is_sensor_g_device is fail(%d)", ret);
		return -EINVAL;
	}

	ret = is_sensor_runtime_suspend_pre(dev);
	if (ret)
		err("is_sensor_runtime_suspend_pre is fail(%d)", ret);

	subdev_csi = device->subdev_csi;
	if (!subdev_csi)
		mwarn("subdev_csi is NULL", device);

#if !defined(USE_OFFLINE_PROCESSING)
	if (test_bit(IS_SENSOR_S_POWER, &device->state)) {
		ret = v4l2_subdev_call(subdev_csi, core, s_power, 0);
		if (ret)
			mwarn("v4l2_csi_call(s_power) is fail(%d)", device, ret);
		else
			clear_bit(IS_SENSOR_S_POWER, &device->state);
	}
#endif

	ret = is_sensor_g_module(device, &module);
	if (ret) {
		merr("is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_sensor_gpio_off(device);
	if (ret)
		mwarn("is_sensor_gpio_off is fail(%d)", device, ret);

	ret = is_sensor_iclk_off(device);
	if (ret)
		mwarn("is_sensor_iclk_off is fail(%d)", device, ret);

	v4l2_device_unregister_subdev(device->subdev_module);
	device->subdev_module = NULL;

p_err:
	if (IS_ENABLED(SECURE_CAMERA_IRIS))
		ret = is_secure_func(NULL, device, IS_SECURE_CAMERA_IRIS,
				device->pdata->scenario, SMC_SECCAM_UNPREPARE);

	if (test_bit(IS_SENSOR_ONLY, &device->state)) {
		struct is_core *core = device->private_data;

		if (atomic_dec_return(&core->resourcemgr.qos_refcount) == 0)
			is_remove_dvfs(core, START_DVFS_LEVEL);
	}

	minfo("[SS%d:D] %s():%d\n", device, device->device_id, __func__, ret);

	return 0;
}

int is_sensor_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct is_device_sensor *device;
	struct v4l2_subdev *subdev_csi;

	device = NULL;

	ret = is_sensor_g_device(pdev, &device);
	if (ret) {
		err("is_sensor_g_device is fail(%d)", ret);
		return -EINVAL;
	}

	subdev_csi = device->subdev_csi;
	if (!subdev_csi) {
		merr("subdev_csi is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_runtime_resume_pre(dev);
	if (ret) {
		merr("is_sensor_runtime_resume_pre is fail(%d)", device, ret);
		goto p_err;
	}

	/* configuration clock control */
	ret = is_sensor_iclk_on(device);
	if (ret) {
		merr("is_sensor_iclk_on is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	minfo("[SS%d:D] %s():%d\n", device, device->device_id, __func__, ret);
	return ret;
}

static int is_sensor_shot(struct is_device_ischain *ischain,
	struct is_group *group,
	struct is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct is_frame *frame;
	struct is_framemgr *framemgr;
	struct is_device_sensor *sensor;

	FIMC_BUG(!ischain);
	FIMC_BUG(!check_frame);

	frame = NULL;
	sensor = ischain->sensor;

	if (!atomic_read(&group->head->scount)) {
		ret = v4l2_subdev_call(sensor->subdev_csi, core, ioctl,
			SENSOR_IOCTL_CSI_DMA_ATTACH, NULL);
		if (ret) {
			merr("v4l2_csi_call(SENSOR_IOCTL_CSI_DMA_ATTACH) is fail(%d)",
				sensor, ret);
			goto p_err;
		}

		if (test_bit(IS_GROUP_VOTF_OUTPUT, &group->state) && group->next) {
			u32 width = is_sensor_g_bns_width(sensor);
			u32 height = is_sensor_g_bns_height(sensor);

			ret = is_votf_create_link_sensor(group->next, width, height);
			if (ret) {
				merr("is_votf_create_link_sensor is fail(%d)",
					sensor, ret);
				goto p_err;
			}
		}
	}

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, check_frame->cur_shot_idx);
	if (!framemgr) {
		merr("framemgr is NULL", ischain);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_25, flags);
	frame = peek_frame(framemgr, FS_REQUEST);
	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_25, flags);

	if (unlikely(!frame)) {
		merr("frame is NULL", ischain);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", ischain, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", ischain);
		ret = -EINVAL;
		goto p_err;
	}

	mgrdbgs(1, " %s(%d) (sensor:%d, line:%d, scount:%d)\n",
		group, group, frame, __func__, frame->index,
		sensor->fcount, sensor->line_fcount, atomic_read(&group->scount));

	/* It is used for sensor only scenario such as TOF sensor or remosic sequence. */
	if (!test_bit(IS_SENSOR_OTF_OUTPUT, &sensor->state))
		goto shot_callback;

	if (IS_ENABLED(INIT_AWB)
	    && (frame->shot->ctl.aa.awbMode == AA_AWBMODE_WB_AUTO)
	    && (frame->fcount <= sensor->init_wb_cnt)
	    && memcmp(sensor->init_wb, sensor->chk_wb, sizeof(float) * WB_GAIN_COUNT)) {
		/* for applying init AWB feature,
		 * 1. awbMode is AA_AWB_MODE_WB_AUTO
		 * 2. it is applied at only initial count frame num
		 * 3. set only last_ae value exist
		 */
		memcpy(frame->shot->ctl.color.gains, sensor->init_wb, sizeof(float) * WB_GAIN_COUNT);
		frame->shot->ctl.aa.awbMode = AA_AWBMODE_OFF;

		mgrdbgs(1, "init AWB(applied cnt:%d)", group->device, group, frame, sensor->init_wb_cnt);
	}

	PROGRAM_COUNT(8);

shot_callback:
	ret = is_devicemgr_shot_callback(group, frame, frame->fcount, IS_DEVICE_SENSOR);
	if (ret) {
		merr("is_ischainmgr_shot_callback fail(%d)", ischain, ret);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int is_sensor_dma_cancel(struct is_device_sensor *device)
{
	int ret = 0;
	struct v4l2_subdev *subdev_csi;

	FIMC_BUG(!device);

	subdev_csi = device->subdev_csi;
	if (!subdev_csi)
		mwarn("subdev_csi is NULL", device);

	ret = v4l2_subdev_call(subdev_csi, core, ioctl, SENSOR_IOCTL_DMA_CANCEL, NULL);
	if (ret) {
		mwarn("v4l2_csi_call(ioctl) is fail(%d)", device, ret);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	minfo("[SS%d:D] %s():%d\n", device, device->device_id, __func__, ret);
	return ret;
}
static const struct dev_pm_ops is_sensor_pm_ops = {
	.suspend		= is_sensor_suspend,
	.resume			= is_sensor_resume,
	.runtime_suspend	= is_sensor_runtime_suspend,
	.runtime_resume		= is_sensor_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_is_sensor_match[] = {
	{
		.compatible = "samsung,exynos-is-sensor",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_is_sensor_match);

struct platform_driver is_sensor_driver = {
	.probe = is_sensor_probe,
	.driver = {
		.name	= IS_SENSOR_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &is_sensor_pm_ops,
		.of_match_table = exynos_is_sensor_match,
	}
};

#else
static struct platform_device_id is_sensor_driver_ids[] = {
	{
		.name		= IS_SENSOR_DEV_NAME,
		.driver_data	= 0,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, is_sensor_driver_ids);

static struct platform_driver is_sensor_driver = {
	.id_table	= is_sensor_driver_ids,
	.driver	  = {
		.name	= IS_SENSOR_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &is_sensor_pm_ops,
	}
};
#endif

#ifndef MODULE
static int __init is_sensor_init(void)
{
	int ret;

	ret = platform_driver_probe(&is_sensor_driver,
		is_sensor_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			IS_SENSOR_DEV_NAME, ret);

	return ret;
}
device_initcall_sync(is_sensor_init);
#endif

MODULE_AUTHOR("Teahyung Kim<tkon.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos IS_SENSOR driver");
MODULE_LICENSE("GPL");
