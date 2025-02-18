/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vender functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>

#include "is-vender.h"
#include "is-vender-specific.h"
#include "is-core.h"
#include "is-interface-library.h"
#include "is-device-sensor-peri.h"
#include <videodev2_exynos_camera.h>

extern int is_create_sysfs(void);

u32 sensor_id[SENSOR_POSITION_MAX];
const char *sensor_name[SENSOR_POSITION_MAX];

static u32  ois_sensor_index;
static u32  aperture_sensor_index;

int is_vendor_select_mipi_by_rf_channel(const struct cam_mipi_channel *channel_list, const int size)
{
	return 0;
}

int is_vendor_verify_mipi_channel(const struct cam_mipi_channel *channel_list, const int size)
{
	return 0;
}
EXPORT_SYMBOL_GPL(is_vendor_verify_mipi_channel);

int is_vendor_set_mipi_clock(struct is_device_sensor *device)
{
	return 0;
}
EXPORT_SYMBOL_GPL(is_vendor_set_mipi_clock);

int is_vendor_get_mipi_clock_string(struct is_device_sensor *device, char *cur_mipi_str)
{
	return 0;
}

int is_vendor_update_mipi_info(struct is_device_sensor *device)
{
	return 0;
}

void is_vendor_csi_stream_on(struct is_device_csi *csi)
{

}

void is_vendor_csi_stream_off(struct is_device_csi *csi)
{

}

bool is_vender_aeb_mode_on(void *cis_data)
{
	cis_shared_data *cis = NULL;
	bool ret = false;

	BUG_ON(!cis_data);

	cis = (cis_shared_data *)cis_data;

	if (cis->is_data.sensor_hdr_mode == CAMERA_SENSOR_HDR_MODE_2AEB)
		ret = true;

	return ret;
}
EXPORT_SYMBOL_GPL(is_vender_aeb_mode_on);

void is_vender_csi_err_handler(struct is_device_csi *csi)
{

}

int is_vender_probe(struct is_vender *vender)
{
	int ret = 0, i;
	struct is_vender_specific *priv;

	BUG_ON(!vender);

	snprintf(vender->fw_path, sizeof(vender->fw_path), "%s%s", IS_FW_DUMP_PATH, IS_FW);
	snprintf(vender->request_fw_path, sizeof(vender->request_fw_path), "%s", IS_FW);

	priv = (struct is_vender_specific *)kzalloc(
					sizeof(struct is_vender_specific), GFP_KERNEL);
	if (!priv) {
		probe_err("failed to allocate vender specific");
		return -ENOMEM;
	}

	ret = is_create_sysfs();
	if (ret)
		probe_warn("failed to create sysfs");

	for (i = 0; i < SENSOR_POSITION_MAX; i++) {
		priv->sensor_id[i] = sensor_id[i];
		priv->sensor_name[i] = sensor_name[i];
	}

	priv->ois_sensor_index = ois_sensor_index;
	priv->aperture_sensor_index = aperture_sensor_index;

	vender->private_data = priv;

	return ret;
}

#ifdef MODULE
int is_vender_driver_init(void)
{
	return 0;
}

int is_vender_driver_exit(void)
{
	return 0;
}
#endif

int is_vender_dt(struct device_node *np)
{
	int ret = 0, i;
	struct device_node *si_np;
	struct device_node *sn_np;
	char *dev_id;

	dev_id = __getname();
	if (!dev_id) {
		probe_err("failed to __getname");
		return -ENOMEM;
	}

	si_np = of_get_child_by_name(np, "sensor_id");
	if (si_np) {
		for (i = 0; i < SENSOR_POSITION_MAX; i++) {
			snprintf(dev_id, PATH_MAX, "%d", i);
			if (of_property_read_u32(si_np, dev_id, &sensor_id[i]))
				probe_warn("sensor_id[%d] read is skipped", i);
		}
	}

	sn_np = of_get_child_by_name(np, "sensor_name");
	if (sn_np) {
		for (i = 0; i < SENSOR_POSITION_MAX; i++) {
			snprintf(dev_id, PATH_MAX, "%d", i);
			if (of_property_read_string(sn_np, dev_id, &sensor_name[i]))
				probe_warn("sensor_name[%d] read is skipped", i);
		}
	}

	__putname(dev_id);

	ret = of_property_read_u32(np, "ois_sensor_index", &ois_sensor_index);
	if (ret)
		probe_err("ois_sensor_index read is fail(%d)", ret);

	ret = of_property_read_u32(np, "aperture_sensor_index", &aperture_sensor_index);
	if (ret)
		probe_err("aperture_sensor_index read is fail(%d)", ret);


	return ret;
}

int is_vender_fw_prepare(struct is_vender *vender, u32 position)
{
	int ret = 0;

	return ret;
}

int is_vender_preproc_fw_load(struct is_vender *vender)
{
	int ret = 0;

	return ret;
}

void is_vender_resource_get(struct is_vender *vender, u32 rsc_type)
{

}

void is_vender_resource_put(struct is_vender *vender, u32 rsc_type)
{

}

#if !defined(ENABLE_CAL_LOAD)
int is_vender_cal_load(struct is_vender *vender,
	void *module_data)
{
	int ret = 0;

	return ret;
}
#else
#ifdef FLASH_CAL_DATA_ENABLE
static int is_led_cal_file_read(const char *file_name, const void *data, unsigned long size)
{
	int ret = 0;
#ifdef USE_KERNEL_VFS_READ_WRITE
	long fsize, nread;
	struct file *fp;

	fp = filp_open(file_name, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		ret = PTR_ERR(fp);
		err("file_open(%s) fail(%d)!!\n", file_name, ret);
		goto p_err;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	nread = kernel_read(fp, data, size, &fp->f_pos);
	info("%s(): read to file(%s) size(%ld)\n", __func__, file_name, nread);
p_err:
	if (!IS_ERR_OR_NULL(fp))
		filp_close(fp, NULL);

#else
	err("not support %s due to kernel_read!", __func__);
	ret = -EINVAL;
#endif
	return ret;
}
#endif

int is_vender_cal_load(struct is_vender *vender,
	void *module_data)
{
	struct is_core *core;
	struct is_module_enum *module = module_data;
	struct is_device_sensor *sensor;
	ulong cal_addr = 0;
	bool led_cal_en = false;
	struct is_eeprom *awb_eeprom;
	u32 awb_cal_sensor_id;
	u32 awb_cal_offset;
	u32 awb_cal_size;
	void *data;
#ifdef FLASH_CAL_DATA_ENABLE
	int ret = 0;
#endif

	core = container_of(vender, struct is_core, vender);
	sensor = v4l2_get_subdev_hostdata(module->subdev);

	cal_addr = core->resourcemgr.minfo.kvaddr_cal[module->position];
	if (!cal_addr) {
		err("%s, wrong cal address(0x%lx)\n", __func__, cal_addr);
		return -EINVAL;
	}

	if (!sensor->eeprom) {
		info("%s, NO EEPROM module", __func__);
		memset((void *)(cal_addr), 0xff, TOTAL_CAL_DATA_SIZE);
		return 0;
	}

	if (sensor->subdev_eeprom && sensor->eeprom->data) {
		memcpy((void *)(cal_addr), (void *)sensor->eeprom->data, sensor->eeprom->total_size);
	} else if (sensor->use_otp_cal) {
		memcpy((void *)(cal_addr), (void *)sensor->otp_cal_buf, sizeof(sensor->otp_cal_buf));
	} else {
		info("%s, not used EEPROM/OTP cal. skip", __func__);
		return 0;
	}

#ifdef FLASH_CAL_DATA_ENABLE
	ret = is_led_cal_file_read(IS_LED_CAL_DATA_PATH, (void *)(cal_addr + CALDATA_SIZE),
			LED_CAL_SIZE);
	if (ret) {
		/* if getting led_cal_data_file is failed, fill buf with 0xff */
		memset((void *)(cal_addr + CALDATA_SIZE), 0xff, LED_CAL_SIZE);
		warn("get led_cal_data fail\n");
	} else {
		led_cal_en = true;
	}
#else
	memset((void *)(cal_addr + CALDATA_SIZE), 0xff, LED_CAL_SIZE);
#endif
	info("LED_CAL data(%d) loading complete(led:%s): 0x%lx\n",
	     module->position, led_cal_en ? "EN" : "NA", cal_addr);

	awb_cal_sensor_id = sensor->eeprom->awb_cal_sensor_id;
	/* it doesn't need to get cal data itself  */
	if (awb_cal_sensor_id == module->sensor_id) {
		info("%s, Skip get AWB CAL data", __func__);
		return 0;
	}

	awb_eeprom = core->sensor[awb_cal_sensor_id].eeprom;
	if (!awb_eeprom) {
		info("%s, NO STANDARD AWB CAL EEPROM module", __func__);
		return 0;
	}

	data = (void *)awb_eeprom->data;
	awb_cal_offset = awb_eeprom->awb_cal_offset;
	awb_cal_size = awb_eeprom->awb_cal_size;
	memcpy((void *)(cal_addr + AWB_CAL_OFFSET), data + awb_cal_offset, awb_cal_size);
	info("%s, get AWB_CAL data from EEPROM of sensor[%d]", __func__, awb_cal_sensor_id);

	return 0;
}
#endif

int is_vender_module_sel(struct is_vender *vender, void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vender_module_del(struct is_vender *vender, void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vender_fw_sel(struct is_vender *vender)
{
	int ret = 0;

	return ret;
}

int is_vender_setfile_sel(struct is_vender *vender,
	char *setfile_name,
	int position)
{
	int ret = 0;

	BUG_ON(!vender);
	BUG_ON(!setfile_name);

	snprintf(vender->setfile_path[position], sizeof(vender->setfile_path[position]),
		"%s%s", IS_SETFILE_SDCARD_PATH, setfile_name);
	snprintf(vender->request_setfile_path[position],
		sizeof(vender->request_setfile_path[position]),
		"%s", setfile_name);

	return ret;
}

int is_vender_preprocessor_gpio_on_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scneario)
{
	int ret = 0;

	return ret;
}

int is_vender_preprocessor_gpio_on(struct is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;
	return ret;
}

int is_vender_sensor_gpio_on_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scenario,
	void *module_data)
{
	int ret = 0;
	return ret;
}

int is_vender_sensor_gpio_on(struct is_vender *vender, u32 scenario, u32 gpio_scenario
	, void *module_data)
{
	int ret = 0;
	return ret;
}

int is_vender_preprocessor_gpio_off_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scneario,
	void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vender_preprocessor_gpio_off(struct is_vender *vender, u32 scenario, u32 gpio_scenario)
{
	int ret = 0;

	return ret;
}

int is_vender_sensor_gpio_off_sel(struct is_vender *vender, u32 scenario, u32 *gpio_scenario,
	void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vender_sensor_gpio_off(struct is_vender *vender, u32 scenario, u32 gpio_scenario,
	void *module_data)
{
	int ret = 0;

	return ret;
}

void is_vendor_sensor_suspend(struct platform_device *pdev)
{
	return;
}

void is_vendor_resource_clean(struct is_core *core)
{
	return;
}

void is_vender_check_retention(struct is_vender *vender, void *module_data)
{
}

void is_vender_itf_open(struct is_vender *vender)
{
	return;
}

int is_vender_set_torch(struct camera2_shot *shot)
{
	return 0;
}

int is_vender_vidioc_s_ctrl(struct is_video_ctx *vctx, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video *video;
	struct is_device_ischain *device;
	unsigned int value = 0;
	unsigned int captureIntent = 0;
	unsigned int captureCount = 0;
	struct is_group *head;
	unsigned long flags;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_DEVICE_ISCHAIN(vctx));
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!ctrl);

	device = GET_DEVICE_ISCHAIN(vctx);
	video = GET_VIDEO(vctx);

	switch (ctrl->id) {
	case V4L2_CID_IS_INTENT:
		value = (unsigned int)ctrl->value;
		captureIntent = (value >> 16) & 0x0000FFFF;
		captureCount = value & 0x0000FFFF;

		head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, &device->group_3aa);

		spin_lock_irqsave(&head->slock_s_ctrl, flags);

		head->intent_ctl.captureIntent = captureIntent;
		head->intent_ctl.vendor_captureCount = captureCount;
		if (captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_MULTI)
			head->remainIntentCount = 2 + INTENT_RETRY_CNT;
		else
			head->remainIntentCount = 0 + INTENT_RETRY_CNT;

		spin_unlock_irqrestore(&head->slock_s_ctrl, flags);

		mvinfo("s_ctrl intent(%d) count(%d) remainIntentCount(%d)\n",
			device, video, captureIntent, captureCount, head->remainIntentCount);
		break;
	default:
		mverr("unsupported ioctl(0x%X)", vctx, video, ctrl->id);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int is_vender_ssx_video_s_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	return 0;
}

int is_vender_ssx_video_g_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	return 0;
}

void is_vender_sensor_s_input(struct is_vender *vender, u32 position)
{
	return;
}

bool is_vender_wdr_mode_on(void *cis_data)
{
	return false;
}
EXPORT_SYMBOL_GPL(is_vender_wdr_mode_on);

bool is_vender_enable_wdr(void *cis_data)
{
	return false;
}

/**
  * is_vender_request_binary: send loading request to the loader
  * @bin: pointer to is_binary structure
  * @path: path of binary file
  * @name: name of binary file
  * @device: device for which binary is being loaded
  **/
int is_vender_request_binary(struct is_binary *bin, const char *path1, const char *path2,
				const char *name, struct device *device)
{

	return 0;
}

int is_vender_s_ctrl(struct is_vender *vender)
{
	return 0;
}

int is_vender_remove_dump_fw_file(void)
{
	return 0;
}
