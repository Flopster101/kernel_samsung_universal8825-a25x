/*
 * Goodix Touchscreen Driver
 * Copyright (C) 2020 - 2021 Goodix, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38)
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif

#include "goodix_ts_core.h"

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif

#define GOODIX_DEFAULT_CFG_NAME		"goodix_cfg_group.cfg"

struct goodix_module goodix_modules;
int core_module_prob_state = CORE_MODULE_UNPROBED;

static void goodix_ts_force_release_all_finger(struct goodix_ts_core *core_data);
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
static int goodix_set_charger(struct goodix_ts_core *cd, int mode);
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_i2c_lock(struct i2c_adapter *adap);
extern int stui_i2c_unlock(struct i2c_adapter *adap);

static int goodix_stui_tsp_enter(void)
{
	struct goodix_ts_core *ts = dev_get_drvdata(ptsp);
	int ret = 0;

	if (!ts)
		return -EINVAL;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->sec_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif

	ts->hw_ops->irq_enable(ts, false);
	goodix_ts_release_all_finger(ts);

	ret = stui_i2c_lock(to_i2c_client(ptsp)->adapter);
	if (ret) {
		pr_err("[STUI] stui_i2c_lock failed : %d\n", ret);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->sec_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
		ts->hw_ops->irq_enable(ts, true);
		return -1;
	}

	return 0;
}

static int goodix_stui_tsp_exit(void)
{
	struct goodix_ts_core *ts = dev_get_drvdata(ptsp);
	int ret = 0;

	if (!ts)
		return -EINVAL;

	ret = stui_i2c_unlock(to_i2c_client(ptsp)->adapter);
	if (ret)
		pr_err("[STUI] stui_i2c_unlock failed : %d\n", ret);

	ts->hw_ops->irq_enable(ts, true);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->sec_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif

	return ret;
}

static int goodix_stui_tsp_type(void)
{
	return STUI_TSP_TYPE_GOODIX;
}
#endif

/**
 * __do_register_ext_module - register external module
 * to register into touch core modules structure
 * return 0 on success, otherwise return < 0
 */
static int __do_register_ext_module(struct goodix_ext_module *module)
{
	struct goodix_ext_module *ext_module, *next;
	struct list_head *insert_point = &goodix_modules.head;

	/* prority level *must* be set */
	if (module->priority == EXTMOD_PRIO_RESERVED) {
		ts_err("Priority of module [%s] needs to be set",
				module->name);
		return -EINVAL;
	}
	mutex_lock(&goodix_modules.mutex);
	/* find insert point for the specified priority */
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
				&goodix_modules.head, list) {
			if (ext_module == module) {
				ts_info("Module [%s] already exists",
						module->name);
				mutex_unlock(&goodix_modules.mutex);
				return 0;
			}
		}

		/* smaller priority value with higher priority level */
		list_for_each_entry_safe(ext_module, next,
				&goodix_modules.head, list) {
			if (ext_module->priority >= module->priority) {
				insert_point = &ext_module->list;
				break;
			}
		}
	}

	if (module->funcs && module->funcs->init) {
		if (module->funcs->init(goodix_modules.core_data,
					module) < 0) {
			ts_err("Module [%s] init error",
					module->name ? module->name : " ");
			mutex_unlock(&goodix_modules.mutex);
			return -EFAULT;
		}
	}

	list_add(&module->list, insert_point->prev);
	mutex_unlock(&goodix_modules.mutex);

	ts_info("Module [%s] registered,priority:%u", module->name,
			module->priority);
	return 0;
}

static void goodix_register_ext_module_work(struct work_struct *work)
{
	struct goodix_ext_module *module =
		container_of(work, struct goodix_ext_module, work);

	ts_info("module register work IN");

	/* driver probe failed */
	if (core_module_prob_state != CORE_MODULE_PROB_SUCCESS) {
		ts_err("Can't register ext_module core error");
		return;
	}

	if (__do_register_ext_module(module))
		ts_err("failed register module: %s", module->name);
	else
		ts_info("success register module: %s", module->name);
}

static void goodix_core_module_init(void)
{
	if (goodix_modules.initialized)
		return;
	goodix_modules.initialized = true;
	INIT_LIST_HEAD(&goodix_modules.head);
	mutex_init(&goodix_modules.mutex);
}

/**
 * goodix_register_ext_module - interface for register external module
 * to the core. This will create a workqueue to finish the real register
 * work and return immediately. The user need to check the final result
 * to make sure registe is success or fail.
 *
 * @module: pointer to external module to be register
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module(struct goodix_ext_module *module)
{
	if (!module)
		return -EINVAL;

	ts_info("goodix_register_ext_module IN");

	goodix_core_module_init();
	INIT_WORK(&module->work, goodix_register_ext_module_work);
	schedule_work(&module->work);

	ts_info("goodix_register_ext_module OUT");
	return 0;
}

/**
 * goodix_register_ext_module_no_wait
 * return: 0 ok, <0 failed
 */
int goodix_register_ext_module_no_wait(struct goodix_ext_module *module)
{
	if (!module)
		return -EINVAL;
	ts_info("goodix_register_ext_module_no_wait IN");
	goodix_core_module_init();
	/* driver probe failed */
	if (core_module_prob_state != CORE_MODULE_PROB_SUCCESS) {
		ts_err("Can't register ext_module core error");
		return -EINVAL;
	}
	return __do_register_ext_module(module);
}

/**
 * goodix_unregister_ext_module - interface for external module
 * to unregister external modules
 *
 * @module: pointer to external module
 * return: 0 ok, <0 failed
 */
int goodix_unregister_ext_module(struct goodix_ext_module *module)
{
	struct goodix_ext_module *ext_module, *next;
	bool found = false;

	if (!module)
		return -EINVAL;

	if (!goodix_modules.initialized)
		return -EINVAL;

	if (!goodix_modules.core_data)
		return -ENODEV;

	mutex_lock(&goodix_modules.mutex);
	if (!list_empty(&goodix_modules.head)) {
		list_for_each_entry_safe(ext_module, next,
				&goodix_modules.head, list) {
			if (ext_module == module) {
				found = true;
				break;
			}
		}
	} else {
		mutex_unlock(&goodix_modules.mutex);
		return 0;
	}

	if (!found) {
		ts_debug("Module [%s] never registed",
				module->name);
		mutex_unlock(&goodix_modules.mutex);
		return 0;
	}

	list_del(&module->list);
	mutex_unlock(&goodix_modules.mutex);

	if (module->funcs && module->funcs->exit)
		module->funcs->exit(goodix_modules.core_data, module);

	ts_info("Moudle [%s] unregistered",
			module->name ? module->name : " ");
	return 0;
}

static void goodix_ext_sysfs_release(struct kobject *kobj)
{
	ts_info("Kobject released!");
}

#define to_ext_module(kobj)	container_of(kobj,\
		struct goodix_ext_module, kobj)
#define to_ext_attr(attr)	container_of(attr,\
		struct goodix_ext_attribute, attr)

static ssize_t goodix_ext_sysfs_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct goodix_ext_module *module = to_ext_module(kobj);
	struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->show)
		return ext_attr->show(module, buf);

	return -EIO;
}

static ssize_t goodix_ext_sysfs_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	struct goodix_ext_module *module = to_ext_module(kobj);
	struct goodix_ext_attribute *ext_attr = to_ext_attr(attr);

	if (ext_attr->store)
		return ext_attr->store(module, buf, count);

	return -EIO;
}

static const struct sysfs_ops goodix_ext_ops = {
	.show = goodix_ext_sysfs_show,
	.store = goodix_ext_sysfs_store
};

static struct kobj_type goodix_ext_ktype = {
	.release = goodix_ext_sysfs_release,
	.sysfs_ops = &goodix_ext_ops,
};

struct kobj_type *goodix_get_default_ktype(void)
{
	return &goodix_ext_ktype;
}

struct kobject *goodix_get_default_kobj(void)
{
	struct kobject *kobj = NULL;

	if (goodix_modules.core_data &&
			goodix_modules.core_data->pdev)
		kobj = &goodix_modules.core_data->pdev->dev.kobj;
	return kobj;
}

/* show driver information */
static ssize_t goodix_ts_driver_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Goodix ts driver for SEC:%s\n",
			GOODIX_DRIVER_VERSION);
}

/* show chip infoamtion */
//#define NORMANDY_CFG_VER_REG	0x6F78
static ssize_t goodix_ts_chip_info_show(struct device  *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_fw_version chip_ver;
	u8 temp_pid[8] = {0};
	int ret;
	int cnt = 0;

	ret = hw_ops->read_version(core_data, &chip_ver);
	if (!ret) {
		memcpy(temp_pid, chip_ver.rom_pid, sizeof(chip_ver.rom_pid));
		cnt += snprintf(&buf[0], PAGE_SIZE,
				"rom_pid:%s\nrom_vid:%02x%02x%02x\n",
				temp_pid, chip_ver.rom_vid[0],
				chip_ver.rom_vid[1], chip_ver.rom_vid[2]);
		cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"patch_pid:%s\npatch_vid:%02x%02x%02x%02x\n",
				chip_ver.patch_pid, chip_ver.patch_vid[0],
				chip_ver.patch_vid[1], chip_ver.patch_vid[2],
				chip_ver.patch_vid[3]);
		cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"sensorid:%d\n", chip_ver.sensor_id);
	}

	ret = hw_ops->get_ic_info(core_data, &core_data->ic_info);
	if (!ret) {
		cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"config_id:%x\n", core_data->ic_info.version.config_id);
		cnt += snprintf(&buf[cnt], PAGE_SIZE,
				"config_version:%x\n", core_data->ic_info.version.config_version);
		cnt += snprintf(&buf[cnt], PAGE_SIZE,
			"firmware_checksum:%x\n", core_data->ic_info.sec.total_checksum);				
	}

	return cnt;
}

/* reset chip */
static ssize_t goodix_ts_reset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;

	if (!buf || count <= 0)
		return -EINVAL;
	if (buf[0] != '0')
		hw_ops->reset(core_data, GOODIX_NORMAL_RESET_DELAY_MS);
	return count;
}

/* read config */
static ssize_t goodix_ts_read_cfg_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;
	int i;
	int offset;
	char *cfg_buf = NULL;

	cfg_buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!cfg_buf)
		return -ENOMEM;

	ret = hw_ops->read_config(core_data, cfg_buf, PAGE_SIZE);
	if (ret > 0) {
		offset = 0;
		for (i = 0; i < 200; i++) { // only print 200 bytes
			offset += snprintf(&buf[offset], PAGE_SIZE - offset,
					"%02x,", cfg_buf[i]);
			if ((i + 1) % 20 == 0)
				buf[offset++] = '\n';
		}
	}

	kfree(cfg_buf);
	if (ret <= 0)
		return ret;

	return offset;
}

static u8 ascii2hex(u8 a)
{
	s8 value = 0;

	if (a >= '0' && a <= '9')
		value = a - '0';
	else if (a >= 'A' && a <= 'F')
		value = a - 'A' + 0x0A;
	else if (a >= 'a' && a <= 'f')
		value = a - 'a' + 0x0A;
	else
		value = 0xff;

	return value;
}

static int goodix_ts_convert_0x_data(const u8 *buf, int buf_size,
		u8 *out_buf, int *out_buf_len)
{
	int i, m_size = 0;
	int temp_index = 0;
	u8 high, low;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] == 'x' || buf[i] == 'X')
			m_size++;
	}

	if (m_size <= 1) {
		ts_err("cfg file ERROR, valid data count:%d", m_size);
		return -EINVAL;
	}
	*out_buf_len = m_size;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] != 'x' && buf[i] != 'X')
			continue;

		if (temp_index >= m_size) {
			ts_err("exchange cfg data error, overflow,"
					"temp_index:%d,m_size:%d",
					temp_index, m_size);
			return -EINVAL;
		}
		high = ascii2hex(buf[i + 1]);
		low = ascii2hex(buf[i + 2]);
		if (high == 0xff || low == 0xff) {
			ts_err("failed convert: 0x%x, 0x%x",
					buf[i + 1], buf[i + 2]);
			return -EINVAL;
		}
		out_buf[temp_index++] = (high << 4) + low;
	}
	return 0;
}

/* send config */
static ssize_t goodix_ts_send_cfg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_ic_config *config = NULL;
	const struct firmware *cfg_img = NULL;
	int en;
	int ret;

	if (sscanf(buf, "%d", &en) != 1)
		return -EINVAL;

	if (en != 1)
		return -EINVAL;

	hw_ops->irq_enable(core_data, false);

	ret = request_firmware(&cfg_img, GOODIX_DEFAULT_CFG_NAME, dev);
	if (ret < 0) {
		ts_err("cfg file [%s] not available,errno:%d",
				GOODIX_DEFAULT_CFG_NAME, ret);
		goto exit;
	} else {
		ts_info("cfg file [%s] is ready", GOODIX_DEFAULT_CFG_NAME);
	}

	config = kzalloc(sizeof(*config), GFP_KERNEL);
	if (!config)
		goto exit;

	if (goodix_ts_convert_0x_data(cfg_img->data, cfg_img->size,
				config->data, &config->len)) {
		ts_err("convert config data FAILED");
		goto exit;
	}

	ret = hw_ops->send_config(core_data, config->data, config->len);
	if (ret < 0)
		ts_err("send config failed");


exit:
	hw_ops->irq_enable(core_data, true);
	kfree(config);
	if (cfg_img)
		release_firmware(cfg_img);

	return count;
}

/* reg read/write */
static u32 rw_addr;
static u32 rw_len;
static u8 rw_flag;
static u8 store_buf[32];
static u8 show_buf[PAGE_SIZE];
static ssize_t goodix_ts_reg_rw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;

	if (!rw_addr || !rw_len) {
		ts_err("address(0x%x) and length(%d) can't be null",
				rw_addr, rw_len);
		return -EINVAL;
	}

	if (rw_flag != 1) {
		ts_err("invalid rw flag %d, only support [1/2]", rw_flag);
		return -EINVAL;
	}

	ret = hw_ops->read(core_data, rw_addr, show_buf, rw_len);
	if (ret < 0) {
		ts_err("failed read addr(%x) length(%d)", rw_addr, rw_len);
		return snprintf(buf, PAGE_SIZE, "failed read addr(%x), len(%d)\n",
				rw_addr, rw_len);
	}

	return snprintf(buf, PAGE_SIZE, "0x%x,%d {%*ph}\n",
			rw_addr, rw_len, rw_len, show_buf);
}

static ssize_t goodix_ts_reg_rw_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	char *pos = NULL;
	char *token = NULL;
	long result = 0;
	int ret;
	int i;

	if (!buf || !count) {
		ts_err("invalid parame");
		goto err_out;
	}

	if (buf[0] == 'r') {
		rw_flag = 1;
	} else if (buf[0] == 'w') {
		rw_flag = 2;
	} else {
		ts_err("string must start with 'r/w'");
		goto err_out;
	}

	/* get addr */
	pos = (char *)buf;
	pos += 2;
	token = strsep(&pos, ":");
	if (!token) {
		ts_err("invalid address info");
		goto err_out;
	} else {
		if (kstrtol(token, 16, &result)) {
			ts_err("failed get addr info");
			goto err_out;
		}
		rw_addr = (u32)result;
		ts_info("rw addr is 0x%x", rw_addr);
	}

	/* get length */
	token = strsep(&pos, ":");
	if (!token) {
		ts_err("invalid length info");
		goto err_out;
	} else {
		if (kstrtol(token, 0, &result)) {
			ts_err("failed get length info");
			goto err_out;
		}
		rw_len = (u32)result;
		ts_info("rw length info is %d", rw_len);
		if (rw_len > sizeof(store_buf)) {
			ts_err("data len > %lu", sizeof(store_buf));
			goto err_out;
		}
	}

	if (rw_flag == 1)
		return count;

	for (i = 0; i < rw_len; i++) {
		token = strsep(&pos, ":");
		if (!token) {
			ts_err("invalid data info");
			goto err_out;
		} else {
			if (kstrtol(token, 16, &result)) {
				ts_err("failed get data[%d] info", i);
				goto err_out;
			}
			store_buf[i] = (u8)result;
			ts_info("get data[%d]=0x%x", i, store_buf[i]);
		}
	}
	ret = hw_ops->write(core_data, rw_addr, store_buf, rw_len);
	if (ret < 0) {
		ts_err("failed write addr(%x) data %*ph", rw_addr,
				rw_len, store_buf);
		goto err_out;
	}

	ts_info("%s write to addr (%x) with data %*ph",
			"success", rw_addr, rw_len, store_buf);

	return count;
err_out:
	snprintf(show_buf, PAGE_SIZE, "%s\n",
			"invalid params, format{r/w:4100:length:[41:21:31]}");
	return -EINVAL;

}

/* show irq information */
static ssize_t goodix_ts_irq_info_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct irq_desc *desc;
	size_t offset = 0;
	int r;

	r = snprintf(&buf[offset], PAGE_SIZE, "irq:%u\n", core_data->irq);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "state:%s\n",
			atomic_read(&core_data->irq_enabled) ?
			"enabled" : "disabled");
	if (r < 0)
		return -EINVAL;

	desc = irq_to_desc(core_data->irq);
	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "disable-depth:%d\n",
			desc->depth);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset, "trigger-count:%zu\n",
			core_data->irq_trig_cnt);
	if (r < 0)
		return -EINVAL;

	offset += r;
	r = snprintf(&buf[offset], PAGE_SIZE - offset,
			"echo 0/1 > irq_info to disable/enable irq\n");
	if (r < 0)
		return -EINVAL;

	offset += r;
	return offset;
}

/* enable/disable irq */
static ssize_t goodix_ts_irq_info_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;

	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		hw_ops->irq_enable(core_data, true);
	else
		hw_ops->irq_enable(core_data, false);
	return count;
}

/* show esd status */
static ssize_t goodix_ts_esd_info_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_esd *ts_esd = &core_data->ts_esd;
	int r = 0;

	r = snprintf(buf, PAGE_SIZE, "state:%s\n",
			atomic_read(&ts_esd->esd_on) ?
			"enabled" : "disabled");

	return r;
}

/* enable/disable esd */
static ssize_t goodix_ts_esd_info_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
	else
		goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
	return count;
}

/* debug level show */
static ssize_t goodix_ts_debug_log_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int r = 0;

	r = snprintf(buf, PAGE_SIZE, "state:%s\n",
			debug_log_flag ?
			"enabled" : "disabled");

	return r;
}

/* debug level store */
static ssize_t goodix_ts_debug_log_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (!buf || count <= 0)
		return -EINVAL;

	if (buf[0] != '0')
		debug_log_flag = true;
	else
		debug_log_flag = false;
	return count;
}

static DEVICE_ATTR(driver_info, 0444, goodix_ts_driver_info_show, NULL);
static DEVICE_ATTR(chip_info, 0444, goodix_ts_chip_info_show, NULL);
static DEVICE_ATTR(reset, 0220, NULL, goodix_ts_reset_store);
static DEVICE_ATTR(send_cfg, 0220, NULL, goodix_ts_send_cfg_store);
static DEVICE_ATTR(read_cfg, 0444, goodix_ts_read_cfg_show, NULL);
static DEVICE_ATTR(reg_rw, 0664, goodix_ts_reg_rw_show, goodix_ts_reg_rw_store);
static DEVICE_ATTR(irq_info, 0664, goodix_ts_irq_info_show, goodix_ts_irq_info_store);
static DEVICE_ATTR(esd_info, 0664, goodix_ts_esd_info_show, goodix_ts_esd_info_store);
static DEVICE_ATTR(debug_log, 0664, goodix_ts_debug_log_show, goodix_ts_debug_log_store);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_driver_info.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_reset.attr,
	&dev_attr_send_cfg.attr,
	&dev_attr_read_cfg.attr,
	&dev_attr_reg_rw.attr,
	&dev_attr_irq_info.attr,
	&dev_attr_esd_info.attr,
	&dev_attr_debug_log.attr,
	NULL,
};

static const struct attribute_group sysfs_group = {
	.attrs = sysfs_attrs,
};

static int goodix_ts_sysfs_init(struct goodix_ts_core *core_data)
{
	int ret;

	ret = sysfs_create_group(&core_data->pdev->dev.kobj, &sysfs_group);
	if (ret) {
		ts_err("failed create core sysfs group");
		return ret;
	}

	return ret;
}

static void goodix_ts_sysfs_exit(struct goodix_ts_core *core_data)
{
	sysfs_remove_group(&core_data->pdev->dev.kobj, &sysfs_group);
}

/* prosfs create */
static int rawdata_proc_show(struct seq_file *m, void *v)
{
	struct ts_rawdata_info *info;
	struct goodix_ts_core *cd;
	int tx;
	int rx;
	int ret;
	int i;
	int index;

	if (!m || !v) {
		ts_err("rawdata_proc_show, input null ptr");
		return -EIO;
	}

	cd = m->private;
	if (!cd) {
		ts_err("can't get core data");
		return -EIO;
	}

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		ts_err("Failed to alloc rawdata info memory");
		return -ENOMEM;
	}

	ret = cd->hw_ops->get_capacitance_data(cd, info);
	if (ret < 0) {
		ts_err("failed to get_capacitance_data, exit!");
		goto exit;
	}

	rx = info->buff[0];
	tx = info->buff[1];
	seq_printf(m, "TX:%d  RX:%d\n", tx, rx);
	seq_printf(m, "mutual_rawdata:\n");
	index = 2;
	for (i = 0; i < tx * rx; i++) {
		seq_printf(m, "%5d,", info->buff[index + i]);
		if ((i + 1) % tx == 0)
			seq_printf(m, "\n");
	}
	seq_printf(m, "mutual_diffdata:\n");
	index += tx * rx;
	for (i = 0; i < tx * rx; i++) {
		seq_printf(m, "%3d,", info->buff[index + i]);
		if ((i + 1) % tx == 0)
			seq_printf(m, "\n");
	}

exit:
	kfree(info);
	return ret;
}

static int rawdata_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, rawdata_proc_show, PDE_DATA(inode), PAGE_SIZE * 10);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops rawdata_proc_fops = {
	.proc_open = rawdata_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations rawdata_proc_fops = {
	.open = rawdata_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static void goodix_ts_procfs_init(struct goodix_ts_core *core_data)
{
	struct proc_dir_entry *proc_entry;

	if (!proc_mkdir("goodix_ts", NULL))
		return;

	proc_entry = proc_create_data("goodix_ts/tp_capacitance_data",
			0664, NULL, &rawdata_proc_fops, core_data);
	if (!proc_entry)
		ts_err("failed to create proc entry");
}

static void goodix_ts_procfs_exit(struct goodix_ts_core *core_data)
{
	remove_proc_entry("goodix_ts/tp_capacitance_data", NULL);
	remove_proc_entry("goodix_ts", NULL);
}

/* event notifier */
static BLOCKING_NOTIFIER_HEAD(ts_notifier_list);
/**
 * goodix_ts_register_client - register a client notifier
 * @nb: notifier block to callback on events
 *  see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&ts_notifier_list, nb);
}

/**
 * goodix_ts_unregister_client - unregister a client notifier
 * @nb: notifier block to callback on events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&ts_notifier_list, nb);
}

/**
 * fb_notifier_call_chain - notify clients of fb_events
 *	see enum ts_notify_event in goodix_ts_core.h
 */
int goodix_ts_blocking_notify(enum ts_notify_event evt, void *v)
{
	int ret;

	ret = blocking_notifier_call_chain(&ts_notifier_list,
			(unsigned long)evt, v);
	return ret;
}

#ifdef CONFIG_OF
static int goodix_parse_update_info(struct device_node *node,
		struct goodix_ts_core *core_data)
{
	int ret;

	ret = of_property_read_u32(node, "sec,isp_ram_reg", &core_data->isp_ram_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,flash_cmd_reg", &core_data->flash_cmd_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,isp_buffer_reg", &core_data->isp_buffer_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,config_data_reg", &core_data->config_data_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,misctl_reg", &core_data->misctl_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,watch_dog_reg", &core_data->watch_dog_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,config_id_reg", &core_data->config_id_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,enable_misctl_val", &core_data->enable_misctl_val);
	if (ret < 0)
		return ret;

	return 0;
}

static int goodix_test_prepare(struct device_node *node,
		struct goodix_ts_core *core_data)
{
	struct property *prop;
	int arr_len;
	int size;
	int ret;

	ret = of_property_read_u32(node, "sec,max_drv_num", &core_data->max_drv_num);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,max_sen_num", &core_data->max_sen_num);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,short_test_time_reg", &core_data->short_test_time_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,short_test_status_reg", &core_data->short_test_status_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,short_test_result_reg", &core_data->short_test_result_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,drv_drv_reg", &core_data->drv_drv_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,sen_sen_reg", &core_data->sen_sen_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,drv_sen_reg", &core_data->drv_sen_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,diff_code_reg", &core_data->diff_code_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,production_test_addr", &core_data->production_test_addr);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,switch_cfg_cmd", &core_data->switch_cfg_cmd);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,switch_freq_cmd", &core_data->switch_freq_cmd);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,snr_cmd", &core_data->snr_cmd);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,sensitive_cmd", &core_data->sensitive_cmd);
	if (ret < 0)
		return ret;

	prop = of_find_property(node, "sec,drv_map", &size);
	if (!prop) {
		ts_err("can't find drv_map");
		return -EINVAL;
	}
	arr_len = size / sizeof(u32);
	if (arr_len <= 0) {
		ts_err("invlid array size:%d", arr_len);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "sec,drv_map", core_data->drv_map, arr_len);
	if (ret < 0)
		return ret;

	ts_info("drv_map array size:%d", arr_len);

	prop = of_find_property(node, "sec,sen_map", &size);
	if (!prop) {
		ts_err("can't find sen_map");
		return -EINVAL;
	}
	arr_len = size / sizeof(u32);
	if (arr_len <= 0) {
		ts_err("invlid array size:%d", arr_len);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(node, "sec,sen_map", core_data->sen_map, arr_len);
	if (ret < 0)
		return ret;

	ts_info("sen_map array size:%d", arr_len);

	core_data->enable_esd_check = of_property_read_bool(node, "sec,enable_esd_check");
	ts_info("esd check %s", core_data->enable_esd_check ? "enable" : "disable");

	return 0;
}

static int goodix_parse_dt(struct device *dev, struct goodix_ts_core *core_data)
{
	struct device_node *node = dev->of_node;
	unsigned int ic_type;
	int r;

	if (!core_data) {
		ts_err("invalid core_data");
		return -EINVAL;
	}

	/* get ic type */
	r = of_property_read_u32(node, "sec,ic_type", &ic_type);
	if (r < 0) {
		ts_err("can't parse sec,ic_type, exit");
		return r;
	}

	if (ic_type == 1) {
		ts_info("ic_type is GT6936");
		core_data->bus->ic_type = IC_TYPE_BERLIN_B;
	} else if (ic_type == 2) {
		ts_info("ic_type is GT9895");
		core_data->bus->ic_type = IC_TYPE_BERLIN_D;
	} else {
		ts_err("invalid ic_type:%d", ic_type);
		return -EINVAL;
	}

	/* for refresh_rate_mode cmd */
	core_data->refresh_rate_enable = of_property_read_bool(node, "sec,refresh_rate_enable");
	ts_info("sec,refresh_rate_enable:%d", core_data->refresh_rate_enable);

	of_property_read_u32(node, "sec,specific_fw_update_ver", &core_data->specific_fw_update_ver);
	ts_info("sec,specific_fw_update_ver:0x%x", core_data->specific_fw_update_ver);

	r = goodix_parse_update_info(node, core_data);
	if (r) {
		ts_err("Failed to parse update info");
		return r;
	}

	r = goodix_test_prepare(node, core_data);
	if (r)
		ts_err("Failed to get test information %d", r);

	return 0;
}
#endif

static void goodix_ts_handler_wait_resume_work(struct work_struct *work)
{
	struct goodix_ts_core *core_data = container_of(work, struct goodix_ts_core, irq_work);
	struct irq_desc *desc = irq_to_desc(core_data->irq);
	int ret;

	ret = wait_for_completion_interruptible_timeout(&core_data->resume_done,
			msecs_to_jiffies(SEC_TS_WAKE_LOCK_TIME));
	if (ret == 0) {
		ts_err("LPM: pm resume is not handled");
		goto out;
	}
	if (ret < 0) {
		ts_err("LPM: -ERESTARTSYS if interrupted, %d", ret);
		goto out;
	}

	if (desc && desc->action && desc->action->thread_fn) {
		ts_info("run irq thread");
		desc->action->thread_fn(core_data->irq, desc->action->dev_id);
	}
out:
	core_data->hw_ops->irq_enable_for_handler(core_data, true);
}

/**
 * goodix_ts_threadirq_func - Bottom half of interrupt
 * This functions is excuted in thread context,
 * sleep in this function is permit.
 *
 * @data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
 #define FOECE_RELEASE_TIME	30	// not use it
static irqreturn_t goodix_ts_threadirq_func(int irq, void *data)
{
	struct goodix_ts_core *core_data = data;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_event *ts_event = &core_data->ts_event;
	struct goodix_ts_esd *ts_esd = &core_data->ts_esd;
	int ret;

	ts_esd->irq_status = true;
	core_data->irq_trig_cnt++;

	if (atomic_read(&core_data->suspended)) {
		__pm_wakeup_event(core_data->sec_ws, SEC_TS_WAKE_LOCK_TIME);

		if (!core_data->resume_done.done) {
			if (!IS_ERR_OR_NULL(core_data->irq_workqueue)) {
				ts_info("disable irq and queue waiting work");
				hw_ops->irq_enable_for_handler(core_data, false);
				queue_work(core_data->irq_workqueue, &core_data->irq_work);
			} else {
				ts_info("irq_workqueue not exist");
			}
			return IRQ_HANDLED;
		}

		ts_info("run LPM interrupt handler");
		if (core_data->plat_data->power_state == SEC_INPUT_STATE_LPM) {
			mutex_lock(&goodix_modules.mutex);
			list_for_each_entry_safe(ext_module, next,
					&goodix_modules.head, list) {
				if (!ext_module->funcs->irq_event)
					continue;
				ret = ext_module->funcs->irq_event(core_data, ext_module);
				if (ret == EVT_CANCEL_IRQEVT) {
					mutex_unlock(&goodix_modules.mutex);
					return IRQ_HANDLED;
				}
			}
			mutex_unlock(&goodix_modules.mutex);
		}
	}

	if (core_data->debug_flag & GOODIX_TS_DEBUG_PRINT_ALLEVENT) {
		ts_info("irq_trig_cnt (%zu)", core_data->irq_trig_cnt);
	}

	/* read touch data from touch device */
	if (hw_ops->event_handler == NULL) {
		ts_err("hw_ops->event_handler is NULL");
		return IRQ_HANDLED;
	}

	ret = hw_ops->event_handler(core_data, ts_event);
	if (likely(!ret)) {
		if (ts_event->event_type & EVENT_EMPTY) {
			goodix_ts_force_release_all_finger(core_data);
		}
	}
	ts_event->event_type = EVENT_INVALID;	// clear event type

	return IRQ_HANDLED;
}

static void goodix_ts_force_release_all_finger(struct goodix_ts_core *core_data)
{
	struct sec_ts_plat_data *pdata = core_data->plat_data;
	struct goodix_ts_event *ts_event = &core_data->ts_event;
	int i, tc_cnt = 0;

	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		if (pdata->coord[i].action == SEC_TS_COORDINATE_ACTION_PRESS ||
				pdata->coord[i].action == SEC_TS_COORDINATE_ACTION_MOVE) {
			tc_cnt++;
		}
	}

	if (tc_cnt == 0)
		return;

	ts_info("tc:%d", tc_cnt);
	sec_input_release_all_finger(core_data->bus->dev);

	/* clean event buffer */
	memset(ts_event, 0, sizeof(*ts_event));
}


void goodix_ts_release_all_finger(struct goodix_ts_core *core_data)
{
	struct goodix_ts_event *ts_event = &core_data->ts_event;

	sec_input_release_all_finger(core_data->bus->dev);

	/* clean event buffer */
	memset(ts_event, 0, sizeof(*ts_event));
}

/**
 * goodix_ts_init_irq - Requset interrput line from system
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_irq_setup(struct goodix_ts_core *core_data)
{
	int ret;

	core_data->irq = gpio_to_irq(core_data->plat_data->irq_gpio);
	if (core_data->irq < 0) {
		ts_err("failed get irq num %d", core_data->irq);
		return -EINVAL;
	}

	core_data->irq_workqueue = create_singlethread_workqueue("goodix_ts_irq_wq");
	if (!IS_ERR_OR_NULL(core_data->irq_workqueue)) {
		INIT_WORK(&core_data->irq_work, goodix_ts_handler_wait_resume_work);
		ts_info("set goodix_ts_handler_wait_resume_work");
	} else {
		ts_err("failed to create irq_workqueue, err: %ld", PTR_ERR(core_data->irq_workqueue));
	}

	ts_info("IRQ:%u,flags:0x%X", core_data->irq, core_data->plat_data->irq_flag);
	ret = devm_request_threaded_irq(&core_data->pdev->dev,
			core_data->irq, NULL,
			goodix_ts_threadirq_func,
			core_data->plat_data->irq_flag,
			GOODIX_CORE_DRIVER_NAME,
			core_data);
	if (ret < 0)
		ts_err("Failed to requeset threaded irq:%d", ret);
	else
		atomic_set(&core_data->irq_enabled, 1);

	return ret;
}

/**
 * goodix_ts_power_on - Turn on power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_on(struct goodix_ts_core *cd)
{
	int ret = 0;

	ts_info("power on %d", cd->plat_data->power_state);
	cd->plat_data->pinctrl_configure(cd->bus->dev, true);

	if (cd->plat_data->power_state == SEC_INPUT_STATE_POWER_ON) {
		ts_err("already power on");
		return 0;
	}

	ret = cd->hw_ops->power_on(cd, true);
	if (!ret)
		cd->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;
	else
		ts_err("failed power on, %d", ret);

	return ret;
}

/**
 * goodix_ts_power_off - Turn off power to the touch device
 * @core_data: pointer to touch core data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_off(struct goodix_ts_core *cd)
{
	int ret;

	ts_info("power off %d", cd->plat_data->power_state);
	if (cd->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("already power off");
		return 0;
	}

	ret = cd->hw_ops->power_on(cd, false);
	if (!ret)
		cd->plat_data->power_state = SEC_INPUT_STATE_POWER_OFF;
	else
		ts_err("failed power off, %d", ret);

	cd->plat_data->pinctrl_configure(cd->bus->dev, false);

	return ret;
}

/**
 * goodix_ts_esd_work - check hardware status and recovery
 *  the hardware if needed.
 */
static void goodix_ts_esd_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct goodix_ts_esd *ts_esd = container_of(dwork,
			struct goodix_ts_esd, esd_work);
	struct goodix_ts_core *cd = container_of(ts_esd,
			struct goodix_ts_core, ts_esd);
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	int ret = 0;

	if (ts_esd->irq_status)
		goto exit;

	if (!atomic_read(&ts_esd->esd_on))
		return;

	if (!hw_ops->esd_check)
		return;

	ret = hw_ops->esd_check(cd);
	if (ret) {
		ts_err("esd check failed");

		if (mutex_trylock(&cd->input_dev->mutex)) {
			cd->hw_ops->reset(cd, 100);
			/* reinit */
			cd->plat_data->init(cd);
			mutex_unlock(&cd->input_dev->mutex);
		} else {
			ts_err("mutex is busy & skip!");
		}
	}

exit:
	ts_esd->irq_status = false;
	if (atomic_read(&ts_esd->esd_on))
		schedule_delayed_work(&ts_esd->esd_work, 2 * HZ);
}

/**
 * goodix_ts_esd_on - turn on esd protection
 */
static void goodix_ts_esd_on(struct goodix_ts_core *cd)
{
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	struct goodix_ts_esd *ts_esd = &cd->ts_esd;

	if (!misc->esd_addr)
		return;

	if (atomic_read(&ts_esd->esd_on))
		return;

	if (IS_ERR_OR_NULL(&ts_esd->esd_work.work))
		return;

	atomic_set(&ts_esd->esd_on, 1);
	if (!schedule_delayed_work(&ts_esd->esd_work, 2 * HZ)) {
		ts_info("esd work already in workqueue");
	}
	ts_info("esd on");
}

/**
 * goodix_ts_esd_off - turn off esd protection
 */
static void goodix_ts_esd_off(struct goodix_ts_core *cd)
{
	struct goodix_ts_esd *ts_esd = &cd->ts_esd;
	int ret;

	if (!atomic_read(&ts_esd->esd_on))
		return;

	if (IS_ERR_OR_NULL(&ts_esd->esd_work.work))
		return;

	atomic_set(&ts_esd->esd_on, 0);
	ret = cancel_delayed_work_sync(&ts_esd->esd_work);
	ts_info("Esd off, esd work state %d", ret);
}

/**
 * goodix_esd_notifier_callback - notification callback
 *  under certain condition, we need to turn off/on the esd
 *  protector, we use kernel notify call chain to achieve this.
 *
 *  for example: before firmware update we need to turn off the
 *  esd protector and after firmware update finished, we should
 *  turn on the esd protector.
 */
static int goodix_esd_notifier_callback(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct goodix_ts_esd *ts_esd = container_of(nb,
			struct goodix_ts_esd, esd_notifier);

	switch (action) {
	case NOTIFY_FWUPDATE_START:
	case NOTIFY_SUSPEND:
	case NOTIFY_ESD_OFF:
		goodix_ts_esd_off(ts_esd->ts_core);
		break;
	case NOTIFY_FWUPDATE_FAILED:
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_RESUME:
	case NOTIFY_ESD_ON:
		goodix_ts_esd_on(ts_esd->ts_core);
		break;
	default:
		break;
	}

	return 0;
}

/**
 * goodix_ts_esd_init - initialize esd protection
 */
int goodix_ts_esd_init(struct goodix_ts_core *cd)
{
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	struct goodix_ts_esd *ts_esd = &cd->ts_esd;

	if (!cd->hw_ops->esd_check || !misc->esd_addr) {
		ts_info("missing key info for esd check");
		return 0;
	}

	INIT_DELAYED_WORK(&ts_esd->esd_work, goodix_ts_esd_work);
	ts_esd->ts_core = cd;
	atomic_set(&ts_esd->esd_on, 0);
	ts_esd->esd_notifier.notifier_call = goodix_esd_notifier_callback;
	goodix_ts_register_notifier(&ts_esd->esd_notifier);
	goodix_ts_esd_on(cd);

	return 0;
}


/**
 * goodix_ts_suspend - Touchscreen suspend function
 * Called by PM/FB/EARLYSUSPEN module to put the device to  sleep
 */
static int goodix_ts_suspend(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;

	if (core_data->init_stage < CORE_INIT_STAGE2 ||
			atomic_read(&core_data->suspended))
		return 0;

	ts_info("Suspend start, lp:0x%x ed:%d pocket_mode:%d fod_lp_mode:%d",
			core_data->plat_data->lowpower_mode, core_data->plat_data->ed_enable,
			core_data->plat_data->pocket_mode, core_data->plat_data->fod_lp_mode);
	atomic_set(&core_data->suspended, 1);
	/* disable irq */
	hw_ops->irq_enable(core_data, false);

	/* inform external module */
	if (core_data->plat_data->lowpower_mode
			|| core_data->plat_data->ed_enable
			|| core_data->plat_data->pocket_mode
			|| core_data->plat_data->fod_lp_mode) {
		mutex_lock(&goodix_modules.mutex);
		if (!list_empty(&goodix_modules.head)) {
			list_for_each_entry_safe(ext_module, next,
					&goodix_modules.head, list) {
				if (!ext_module->funcs->before_suspend)
					continue;

				ret = ext_module->funcs->before_suspend(core_data,
						ext_module);
				if (ret == EVT_CANCEL_SUSPEND) {
					mutex_unlock(&goodix_modules.mutex);
					ts_info("Canceled by module:%s",
							ext_module->name);
					goto out;
				}
			}
		}
		mutex_unlock(&goodix_modules.mutex);
	}

	/* enter sleep mode or power off */
	goodix_ts_power_off(core_data);
//	if (hw_ops->suspend)
//		hw_ops->suspend(core_data);

out:
	goodix_ts_release_all_finger(core_data);
	ts_info("Suspend end");
	return 0;
}

void goodix_ts_reinit(void *data)
{
	struct goodix_ts_core *core_data = (struct goodix_ts_core *)data;
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	int ret = 0;
#endif

	ts_info("Power mode=0x%x", core_data->plat_data->power_state);

	goodix_ts_release_all_finger(core_data);
	core_data->plat_data->touch_noise_status = 0;
	core_data->plat_data->touch_pre_noise_status = 0;
	core_data->plat_data->wet_mode = 0;

	if (core_data->bus->ic_type == IC_TYPE_BERLIN_D) {
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
		ret = goodix_set_charger(core_data, core_data->usb_plug_status);
		if (ret < 0)
			ts_err("fail to set charger mode(%d)", ret);
#endif
	}

	goodix_set_custom_library(core_data);
	goodix_set_press_property(core_data);

	if (core_data->plat_data->support_fod && core_data->plat_data->fod_data.set_val)
		goodix_set_fod_rect(core_data);

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		core_data->plat_data->lpmode(core_data, TO_LOWPOWER_MODE);
		if (core_data->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_AOD)
			goodix_set_aod_rect(core_data);
	} else {
		sec_input_set_grip_type(core_data->bus->dev, ONLY_EDGE_HANDLER);

//		goodix_ts_set_external_noise_mode(ts, EXT_NOISE_MODE_MAX);

//		if (core_data->plat_data->touchable_area)
//			ret = goodix_ts_set_touchable_area(core_data);
	}

	if (core_data->plat_data->ed_enable)
		core_data->hw_ops->ed_enable(core_data, core_data->plat_data->ed_enable);

	if (core_data->plat_data->pocket_mode)
		core_data->hw_ops->pocket_mode_enable(core_data, core_data->plat_data->pocket_mode);

	if (core_data->refresh_rate)
		set_refresh_rate_mode(core_data);

	if (core_data->flip_enable) {
		ts_info("set cover close [%d]", core_data->plat_data->cover_type);
		goodix_set_cover_mode(core_data);
	}

	if (core_data->glove_enable) {
		ts_info("set glove mode on");
		goodix_set_cmd(core_data, GOODIX_GLOVE_MODE_ADDR, core_data->glove_enable);
	}

	if (core_data->plat_data->low_sensitivity_mode) {
		ts_info("set low sensitivity mode on");
		goodix_set_cmd(core_data, GOODIX_LS_MODE_ADDR, core_data->plat_data->low_sensitivity_mode);
	}
}

/**
 * goodix_ts_resume - Touchscreen resume function
 * Called by PM/FB/EARLYSUSPEN module to wakeup device
 */
static int goodix_ts_resume(struct goodix_ts_core *core_data)
{
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret;

	if (core_data->init_stage < CORE_INIT_STAGE2 ||
			!atomic_read(&core_data->suspended))
		return 0;

	ts_info("Resume start");
	atomic_set(&core_data->suspended, 0);

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		/* disable irq */
		hw_ops->irq_enable(core_data, false);

		mutex_lock(&goodix_modules.mutex);
		if (!list_empty(&goodix_modules.head)) {
			list_for_each_entry_safe(ext_module, next,
					&goodix_modules.head, list) {
				if (!ext_module->funcs->before_resume)
					continue;

				ret = ext_module->funcs->before_resume(core_data,
						ext_module);
				if (ret == EVT_CANCEL_RESUME) {
					mutex_unlock(&goodix_modules.mutex);
					ts_info("Canceled by module:%s",
							ext_module->name);
					goto out;
				}
			}
		}
		mutex_unlock(&goodix_modules.mutex);
	}

	/* reset device or power on*/
	goodix_ts_power_on(core_data);
//	if (hw_ops->resume)
//		hw_ops->resume(core_data);

	/* reinit */
	core_data->plat_data->init(core_data);
out:

	/* enable irq */
	hw_ops->irq_enable(core_data, true);
	ts_info("Resume end");
	return 0;
}

/* for debugging */
static void debug_delayed_work_func(struct work_struct *work)
{
	struct goodix_ts_core *cd = goodix_modules.core_data;
	u8 buf[16 * 34 * 2];
	u8 put[256] = {0};
	int i;
	int cnt = 0;

	cd->hw_ops->read(cd, 0x10308, buf, 40);
	for (i = 0; i < 40; i++)
		cnt += sprintf(&put[cnt], "%x,", buf[i]);
	ts_info("0x10308:%s", put);
}

static int goodix_ts_input_open(struct input_dev *dev)
{
	struct goodix_ts_core *core_data = input_get_drvdata(dev);

	cancel_delayed_work_sync(&core_data->work_read_info);

	mutex_lock(&core_data->modechange_mutex);

	ts_info("called");
	core_data->plat_data->enabled = true;
	core_data->plat_data->prox_power_off = 0;
	goodix_ts_resume(core_data);

	cancel_delayed_work(&core_data->work_print_info);
	core_data->plat_data->print_info_cnt_open = 0;
	core_data->plat_data->print_info_cnt_release = 0;

	mutex_unlock(&core_data->modechange_mutex);

	if (!core_data->plat_data->shutdown_called)
		schedule_work(&core_data->work_print_info.work);

	/* for debugging */
	schedule_delayed_work(&core_data->debug_delayed_work, HZ);

	return 0;
}

static void goodix_ts_input_close(struct input_dev *dev)
{
	struct goodix_ts_core *core_data = input_get_drvdata(dev);

	cancel_delayed_work_sync(&core_data->work_read_info);

	if (core_data->plat_data->shutdown_called) {
		ts_err("shutdown was called");
		return;
	}

	mutex_lock(&core_data->modechange_mutex);

	ts_info("called");
	core_data->plat_data->enabled = false;

	/* for debugging */
	cancel_delayed_work_sync(&core_data->debug_delayed_work);

	cancel_delayed_work(&core_data->work_print_info);
	sec_input_print_info(core_data->bus->dev, NULL);

	goodix_ts_suspend(core_data);

	mutex_unlock(&core_data->modechange_mutex);
}

#ifdef CONFIG_PM
/**
 * goodix_ts_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */
static int goodix_ts_pm_suspend(struct device *dev)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);

	//ts_info("enter");
	reinit_completion(&core_data->resume_done);
	return 0;
}
/**
 * goodix_ts_pm_resume - PM resume function
 * Called by kernel during system wakeup
 */
static int goodix_ts_pm_resume(struct device *dev)
{
	struct goodix_ts_core *core_data =
		dev_get_drvdata(dev);

	//ts_info("enter");
	complete_all(&core_data->resume_done);
	return 0;
}
#endif

/**
 * goodix_generic_noti_callback - generic notifier callback
 *  for goodix touch notification event.
 */
static int goodix_generic_noti_callback(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct goodix_ts_core *cd = container_of(self,
			struct goodix_ts_core, ts_notifier);
	const struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	if (cd->init_stage < CORE_INIT_STAGE2)
		return 0;

	ts_info("notify event type 0x%x", (unsigned int)action);
	switch (action) {
	case NOTIFY_FWUPDATE_START:
		hw_ops->irq_enable(cd, 0);
		break;
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_FWUPDATE_FAILED:
		if (hw_ops->read_version(cd, &cd->fw_version))
			ts_info("failed read fw version info[ignore]");
		hw_ops->irq_enable(cd, 1);
		break;
	default:
		break;
	}
	return 0;
}

static int goodix_ts_get_sponge_info(struct goodix_ts_core *cd)
{
	u8 data[2] = { 0 };
	int ret;

	ret = cd->hw_ops->read_from_sponge(cd, SEC_TS_CMD_SPONGE_LP_DUMP, data, 2);
	if (ret < 0) {
		ts_err("Failed to read dump_data");
		return ret;
	}

	cd->sponge_inf_dump = (data[0] & SEC_TS_SPONGE_DUMP_INF_MASK) >> SEC_TS_SPONGE_DUMP_INF_SHIFT;
	cd->sponge_dump_format = data[0] & SEC_TS_SPONGE_DUMP_EVENT_MASK;
	cd->sponge_dump_event = data[1];
	cd->sponge_dump_border = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT
					+ (cd->sponge_dump_format * cd->sponge_dump_event);
	ts_info("[LP DUMP] infinit dump:%d, format:0x%02X, dump_event:0x%02X, dump_border:0x%02X",
			cd->sponge_inf_dump, cd->sponge_dump_format,
			cd->sponge_dump_event, cd->sponge_dump_border);
	return 0;
}

int goodix_ts_stage2_init(struct goodix_ts_core *cd)
{
	int ret;

	cd->plat_data->support_mt_pressure = false;
	ret = sec_input_device_register(cd->bus->dev, cd);
	if (ret) {
		ts_err("failed to register input device, %d", ret);
		return ret;
	}

	cd->input_dev = cd->plat_data->input_dev;
	cd->input_dev_proximity = cd->plat_data->input_dev_proximity;

	/* request irq line */
	ret = goodix_ts_irq_setup(cd);
	if (ret < 0) {
		ts_info("failed set irq");
		goto exit;
	}
	ts_info("success register irq");

	/* create sysfs files */
	goodix_ts_sysfs_init(cd);

	/* create procfs files */
	goodix_ts_procfs_init(cd);

	/* esd protector */
	if (cd->enable_esd_check)
		goodix_ts_esd_init(cd);

	/* gesture init */
	gesture_module_init();

	/* inspect init */
	inspect_module_init();

	cd->plat_data->enabled = true;
	cd->input_dev->open = goodix_ts_input_open;
	cd->input_dev->close = goodix_ts_input_close;
	goodix_ts_cmd_init(cd);

	cd->sec_ws = wakeup_source_register(NULL, "tsp");

	goodix_ts_get_sponge_info(cd);

	return 0;
exit:
	wakeup_source_unregister(cd->sec_ws);
	return ret;
}

static int goodix_check_update_skip(struct goodix_ts_core *core_data, struct goodix_ic_info_sec *fw_info_bin)
{
	struct goodix_fw_version fw_version;
	struct goodix_ic_info ic_info;
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	int ret = 0;

	ret = hw_ops->read_version(core_data, &fw_version);
	if (ret)
		return NEED_FW_UPDATE;

	if (hw_ops->get_ic_info(core_data, &ic_info)) {
		ts_err("invalid ic info, abort");
		return NEED_FW_UPDATE;
	}

	if (core_data->plat_data->bringup == 2) {
		ts_info("bringup 2, skip fw update");
		return SKIP_FW_UPDATE;
	} else if (core_data->plat_data->bringup == 5) {
		ts_info("bringup 5, force fw update");
		return NEED_FW_UPDATE;
	}

	if (!memcmp(fw_version.rom_pid, GOODIX_NOCODE, 6) ||
			!memcmp(fw_version.patch_pid, GOODIX_NOCODE, 6)) {
		ts_info("there is no code in the chip");
		return NEED_FW_UPDATE;
	}

	if ((ic_info.sec.ic_name_list == 0) && (ic_info.sec.project_id == 0) &&
			(ic_info.sec.module_version == 0) && (ic_info.sec.firmware_version == 0)) {
		ts_err("ic has no sec firmware information, do update");
		return NEED_FW_UPDATE;
	}

	if ((core_data->plat_data->bringup == 3) &&
			((ic_info.sec.ic_name_list != fw_info_bin->ic_name_list) ||
			(ic_info.sec.project_id != fw_info_bin->project_id) ||
			(ic_info.sec.module_version != fw_info_bin->module_version) ||
			(ic_info.sec.firmware_version != fw_info_bin->firmware_version))) {
		ts_info("bringup 3, force fw update because fw version is not equal");
		return NEED_FW_UPDATE;
	}

	if (core_data->specific_fw_update_ver) {
		unsigned int ic_version = (ic_info.sec.ic_name_list << 24) | (ic_info.sec.project_id << 16) | (ic_info.sec.module_version << 8) | (ic_info.sec.firmware_version);

		ts_info("ic_version : 0x%x specific_fw_update_ver : 0x%x", ic_version, core_data->specific_fw_update_ver);
		if (core_data->specific_fw_update_ver == ic_version) {
			ts_err("need fw update by specific case : 0x%x", core_data->specific_fw_update_ver);
			return NEED_FW_UPDATE;
		}
	}

	if (ic_info.sec.ic_name_list != fw_info_bin->ic_name_list) {
		ts_err("ic version is not matching");
		return SKIP_FW_UPDATE;
	} else if (ic_info.sec.project_id != fw_info_bin->project_id) {
		ts_err("project id is not matching");
		return NEED_FW_UPDATE;
	} else if (ic_info.sec.module_version != fw_info_bin->module_version) {
		ts_err("module version is not matching");
		return SKIP_FW_UPDATE;
	} else if (ic_info.sec.firmware_version < fw_info_bin->firmware_version) {
		ts_info("ic firmware version is lower than binary firwmare version");
		return NEED_FW_UPDATE;
	}

	/* compare patch vid */
	if (fw_version.patch_vid[3] < core_data->merge_bin_ver.patch_vid[3]) {
		ts_err("WARNING:chip VID[%x] < bin VID[%x], need upgrade",
				fw_version.patch_vid[3], core_data->merge_bin_ver.patch_vid[3]);
		return NEED_FW_UPDATE;
	}

	ts_info("ic fw version is latest, skip fw update");

	return SKIP_FW_UPDATE;
}

int goodix_fw_update(struct goodix_ts_core *cd, int update_type, bool force_update)
{
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	struct goodix_ic_info_sec fw_info_bin;
	const char *fw_name;
	const struct firmware *firmware = NULL;
	int pid_offset;
	int ret;
	int mode = UPDATE_MODE_BLOCK | UPDATE_MODE_SRC_REQUEST;
	bool is_fw_signed = false;

	switch (update_type) {
	case TSP_BUILT_IN:
		if (cd->plat_data->bringup == 1) {
			ts_info("skip fw update because bringup 1");
			ret = 0;
			goto skip_update;
		}
		if (!cd->plat_data->firmware_name) {
			ts_err("firmware name is null");
			return -EINVAL;
		}
		fw_name = cd->plat_data->firmware_name;
		break;
	case TSP_SDCARD:
		fw_name = TSP_EXTERNAL_FW;
		break;
	case TSP_SIGNED_SDCARD:
		fw_name = TSP_EXTERNAL_FW_SIGNED;
		is_fw_signed = true;
		break;
	case TSP_SPU:
	case TSP_VERIFICATION:
		fw_name = TSP_SPU_FW_SIGNED;
		is_fw_signed = true;
		break;
	default:
		ts_err("update_type %d is invalid", update_type);
		return -EINVAL;
	}

	ts_info("fw_name:%s%s", fw_name, is_fw_signed ? ", signed fw" : "");

	ret = request_firmware(&firmware, fw_name, &cd->pdev->dev);
	ts_info("request firmware %s,(%d)", fw_name, ret);
	if (ret) {
		ts_err("failed to request firmware %s,(%d)", fw_name, ret);
		ret = 0;
		goto skip_update;
	}

	memset(&fw_info_bin, 0x00, sizeof(struct goodix_ic_info_sec));
	if (firmware->size < GOODIX_BIN_FW_INFO_ADDR + 4) {
		ts_err("firmware size is abnormal %ld", firmware->size);
		release_firmware(firmware);
		return -EINVAL;
	}

	pid_offset = be32_to_cpup((__be32 *)firmware->data) + 6 + 16 + 17;
	memcpy(cd->merge_bin_ver.patch_pid, firmware->data + pid_offset, 8);
	memcpy(cd->merge_bin_ver.patch_vid, firmware->data + pid_offset + 8, 4);

	fw_info_bin.ic_name_list = firmware->data[GOODIX_BIN_FW_INFO_ADDR] & 0xFF;
	fw_info_bin.project_id = firmware->data[GOODIX_BIN_FW_INFO_ADDR + 1] & 0xFF;
	fw_info_bin.module_version = firmware->data[GOODIX_BIN_FW_INFO_ADDR + 2] & 0xFF;
	fw_info_bin.firmware_version = firmware->data[GOODIX_BIN_FW_INFO_ADDR + 3] & 0xFF;

	ts_info("[BIN] ic name:0x%02X, project:0x%02X, module:0x%02X, fw ver:0x%02X",
			fw_info_bin.ic_name_list,
			fw_info_bin.project_id,
			fw_info_bin.module_version,
			fw_info_bin.firmware_version);

	if (update_type == TSP_BUILT_IN)
		memcpy(&cd->fw_info_bin, &fw_info_bin, sizeof(struct goodix_ic_info_sec));

	/* check signing */
#ifdef SUPPORT_FW_SIGNED
	if (is_fw_signed) {
		long ori_size = firmware->size - SPU_METADATA_SIZE(TSP);
		long spu_ret = spu_firmware_signature_verify("TSP", firmware->data, firmware->size);

		if (spu_ret != ori_size) {
			ts_err("signature verify failed, spu_ret:%ld ori_size:%ld", spu_ret, ori_size);
			ret = -EPERM;
			goto out;
		} else {
			ts_info("signature verify succeeded");
			ret = 0;
		}
	}
#endif

	if (update_type == TSP_VERIFICATION) {
		ts_info("just verify signing, do not fw update");
		goto out;
	}

	if (!force_update && ((update_type == TSP_BUILT_IN) || (update_type == TSP_SPU))) {
		ret = goodix_check_update_skip(cd, &fw_info_bin);
		if (ret == SKIP_FW_UPDATE) {
			ts_info("skip fw update");
			goto skip_update;
		}
	}

	/* setp 1: get config data from config bin */
	ret = goodix_get_config_proc(cd, firmware);
	if (ret < 0)
		ts_info("no valid ic config found");
	else
		ts_info("success get valid ic config");

	/* setp 2: init fw struct add try do fw upgrade */
	ret = goodix_fw_update_init(cd, firmware);
	if (ret) {
		ts_err("failed init fw update module");
		goto out;
	}

	ret = goodix_do_fw_update(cd->ic_configs[CONFIG_TYPE_NORMAL], mode);
	if (ret)
		ts_err("failed do fw update");

skip_update:
	/* setp3: get fw version and ic_info
	 * at this step we believe that the ic is in normal mode,
	 * if the version info is invalid there must have some
	 * problem we cann't cover so exit init directly.
	 */
	if (hw_ops->read_version(cd, &cd->fw_version)) {
		ts_err("invalid fw version, abort");
		ret = -EIO;
		goto out;
	}
	if (hw_ops->get_ic_info(cd, &cd->ic_info)) {
		ts_err("invalid ic info, abort");
		ret = -EIO;
		goto out;
	}

	ts_info("done");

out:
	if (cd->plat_data->bringup != 1)
		release_firmware(firmware);
	return ret;
}

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
static int goodix_set_charger(struct goodix_ts_core *cd, int mode)
{
	struct goodix_ts_cmd temp_cmd;
	int ret;

	if (mode) {
		temp_cmd.cmd = 0xAF;
		temp_cmd.data[0] = 1;
		temp_cmd.len = 5;
	} else {
		temp_cmd.cmd = 0xAF;
		temp_cmd.data[0] = 0;
		temp_cmd.len = 5;
	}

	ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
	if (ret < 0) {
		ts_err("send charger cmd(%d) failed(%d)", mode, ret);
	} else {
		ts_info("set charger %s", mode ? "ON" : "OFF");
	}
	return ret;

}


static int goodix_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	struct goodix_ts_core *cd = container_of(nb, struct goodix_ts_core, vbus_nb);
	vbus_status_t vbus_type = *(vbus_status_t *) data;
	int ret = 0, mode = 0;

	ts_info("cmd=%lu, vbus_type=%d, otg_flag=%d", cmd, vbus_type, cd->otg_flag);

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:/* vbus_type == 2 */
		if (!cd->otg_flag)
			mode = USB_PLUG_ATTACHED;
		else
			return 0;
		break;
	case STATUS_VBUS_LOW:/* vbus_type == 1 */
		mode = USB_PLUG_DETACHED;
		break;
	default:
		return 0;
		break;
	}

	ts_debug("mode (%d) // usb_plug_status (%d) for debug", mode, cd->usb_plug_status);

	if (cd->usb_plug_status == mode) {
		ts_debug("duplicate setting");
		return 0;
	}

	cd->usb_plug_status = mode;

	if (cd->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("ic off & set later");
		return 0;
	}

	ret = goodix_set_charger(cd, cd->usb_plug_status);
	if (ret < 0) {
		ts_err("fail to set charger mode(%d)", ret);
	}
	ts_info("usb_plug_status %s (%d)",
			cd->usb_plug_status ? "connect" : "disconnect", cd->usb_plug_status);
	return ret;
}

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int goodix_ccic_notification(struct notifier_block *nb,
	   unsigned long action, void *data)
{
	struct goodix_ts_core *cd = container_of(nb, struct goodix_ts_core, ccic_nb);
	PD_NOTI_USB_STATUS_TYPEDEF usb_status = *(PD_NOTI_USB_STATUS_TYPEDEF *)data;

	if (usb_status.dest != PDIC_NOTIFY_DEV_USB) {
		return 0;
	}

	switch (usb_status.drp) {
	case USB_STATUS_NOTIFY_ATTACH_DFP:
		cd->otg_flag = 1;
		ts_info("%s otg_flag %d\n", __func__, cd->otg_flag);
		break;
	case USB_STATUS_NOTIFY_DETACH:
		cd->otg_flag = 0;
		ts_info("%s otg_flag %d\n", __func__, cd->otg_flag);
		break;
	default:
		break;
	}
	return 0;
}
#endif
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
static int goodix_set_pen_mode(struct goodix_ts_core *core_data, bool pen_in)
{
	struct goodix_ts_cmd temp_cmd;
	int ret;

	ts_info("pen mode : %s", pen_in ? "IN" : "OUT");

	temp_cmd.len = 5;
	temp_cmd.cmd = 0x76;
	if (pen_in)
		temp_cmd.data[0] = 1;
	else
		temp_cmd.data[0] = 0;

	ret = core_data->hw_ops->send_cmd_delay(core_data, &temp_cmd, 0);
	if (ret < 0)
		ts_err("send pen mode cmd failed");

	return ret;
}

static int goodix_input_notify_call(struct notifier_block *n, unsigned long data, void *v)
{
	struct goodix_ts_core *core_data = container_of(n, struct goodix_ts_core, sec_input_nb);

	if (!core_data)
		return -ENODEV;

	switch (data) {
	case NOTIFIER_WACOM_PEN_HOVER_IN:
		goodix_set_pen_mode(core_data, true);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_OUT:
		goodix_set_pen_mode(core_data, false);
		break;
	default:
		break;
	}

	return 0;
}
#endif

static void goodix_set_grip_data_to_ic(struct device *dev, u8 flag)
{
	struct goodix_ts_core *cd = dev_get_drvdata(dev);
	struct goodix_ts_cmd temp_cmd;
	int ret;

	ts_info("flag: %02X (clr,lan,nor,edg,han)", flag);

	if (flag & G_SET_EDGE_HANDLER) {
		temp_cmd.len = 0x0A;
		temp_cmd.cmd = 0x68;
		temp_cmd.data[0] = 0x00;
		if (cd->plat_data->grip_data.edgehandler_direction == 0) {
			temp_cmd.data[1] = 0;
			temp_cmd.data[2] = 0;
			temp_cmd.data[3] = 0;
			temp_cmd.data[4] = 0;
			temp_cmd.data[5] = 0;
		} else {
			temp_cmd.data[1] = cd->plat_data->grip_data.edgehandler_direction;
			temp_cmd.data[2] = cd->plat_data->grip_data.edgehandler_start_y & 0xFF;
			temp_cmd.data[3] = (cd->plat_data->grip_data.edgehandler_start_y >> 8) & 0xFF;
			temp_cmd.data[4] = cd->plat_data->grip_data.edgehandler_end_y & 0xFF;
			temp_cmd.data[5] = (cd->plat_data->grip_data.edgehandler_end_y >> 8) & 0xFF;
		}
		ts_info("SET_EDGE_HANDLER: %02x %02x %02x %02x %02x",
				temp_cmd.data[1], temp_cmd.data[2],
				temp_cmd.data[3], temp_cmd.data[4],
				temp_cmd.data[5]);
		ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
		if (ret < 0)
			ts_err("send grip data to ic failed");
	}

	if (flag & G_SET_NORMAL_MODE) {
		temp_cmd.len = 0x0A;
		temp_cmd.cmd = 0x68;
		temp_cmd.data[0] = 0x01;
		temp_cmd.data[1] = cd->plat_data->grip_data.edge_range;
		temp_cmd.data[2] = cd->plat_data->grip_data.deadzone_up_x;
		temp_cmd.data[3] = cd->plat_data->grip_data.deadzone_dn_x;
		temp_cmd.data[4] = cd->plat_data->grip_data.deadzone_y & 0xFF;
		temp_cmd.data[5] = (cd->plat_data->grip_data.deadzone_y >> 8) & 0xFF;
		ts_info("SET_NORMAL_MODE: %02x %02x %02x %02x %02x",
				temp_cmd.data[1], temp_cmd.data[2],
				temp_cmd.data[3], temp_cmd.data[4], temp_cmd.data[5]);
		ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
		if (ret < 0)
			ts_err("send grip data to ic failed");
	}

	if (flag & G_SET_LANDSCAPE_MODE) {
		temp_cmd.len = 0x0C;
		temp_cmd.cmd = 0x68;
		temp_cmd.data[0] = 0x02;
		temp_cmd.data[1] = cd->plat_data->grip_data.landscape_mode;
		temp_cmd.data[2] = cd->plat_data->grip_data.landscape_edge;
		temp_cmd.data[3] = cd->plat_data->grip_data.landscape_deadzone;
		temp_cmd.data[4] = cd->plat_data->grip_data.landscape_top_deadzone;
		temp_cmd.data[5] = cd->plat_data->grip_data.landscape_bottom_deadzone;
		temp_cmd.data[6] = cd->plat_data->grip_data.landscape_top_gripzone;
		temp_cmd.data[7] = cd->plat_data->grip_data.landscape_bottom_gripzone;
		ts_info("SET_LANDSCAPE_MODE: %02x %02x %02x %02x %02x %02x %02x",
				temp_cmd.data[1], temp_cmd.data[2],
				temp_cmd.data[3], temp_cmd.data[4],
				temp_cmd.data[5], temp_cmd.data[6],
				temp_cmd.data[7]);
		ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
		if (ret < 0)
			ts_err("send grip data to ic failed");
	}

	if (flag & G_CLR_LANDSCAPE_MODE) {
		temp_cmd.len = 6;
		temp_cmd.cmd = 0x68;
		temp_cmd.data[0] = 0x03;
		temp_cmd.data[1] = cd->plat_data->grip_data.landscape_mode;
		ts_info("CLR_LANDSCAPE_MODE");
		ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
		if (ret < 0)
			ts_err("send grip data to ic failed");
	}
}

void goodix_ts_print_info_work(struct work_struct *work)
{
	struct goodix_ts_core *cd = container_of(work, struct goodix_ts_core,
			work_print_info.work);

	sec_input_print_info(cd->bus->dev, NULL);

	if (!cd->plat_data->shutdown_called)
		schedule_delayed_work(&cd->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

void goodix_ts_read_info_work(struct work_struct *work)
{
	struct goodix_ts_core *cd = container_of(work, struct goodix_ts_core,
			work_read_info.work);

	goodix_ts_run_rawdata_all(cd);

	/* reinit */
	cd->plat_data->init(cd);

	cd->info_work_done = true;

	if (!cd->plat_data->shutdown_called)
		schedule_work(&cd->work_print_info.work);
}


/**
 * goodix_later_init_thread - init IC fw and config
 * @data: point to goodix_ts_core
 *
 * This function respond for get fw version and try upgrade fw and config.
 * Note: when init encounter error, need release all resource allocated here.
 */
static int goodix_later_init_thread(void *data)
{
	int ret, i;
	struct goodix_ts_core *cd = data;
	bool update_flag = false;

	ts_info("start");

	/* dev confirm again. If failed, it means the wrong FW and need to force update */
	ret = cd->hw_ops->dev_confirm(cd);
	if (ret < 0) {
		ts_info("device confirm again failed, maybe wrong FW, need update");
		update_flag = true;
	}

	ret = goodix_fw_update(cd, TSP_BUILT_IN, update_flag);
	if (ret) {
		ts_err("update failed");
		goto uninit_fw;
	}

	/* init other resources */
	ret = goodix_ts_stage2_init(cd);
	if (ret) {
		ts_err("stage2 init failed");
		goto uninit_fw;
	}
	cd->init_stage = CORE_INIT_STAGE2;

	return 0;

uninit_fw:
	goodix_fw_update_uninit();

	ts_err("stage2 init failed");
	cd->init_stage = CORE_INIT_FAIL;
	for (i = 0; i < GOODIX_MAX_CONFIG_GROUP; i++) {
		if (cd->ic_configs[i])
			kfree(cd->ic_configs[i]);
		cd->ic_configs[i] = NULL;
	}
	return ret;
}

//static int goodix_start_later_init(struct goodix_ts_core *ts_core)
//{
//	struct task_struct *init_thrd;
//	/* create and run update thread */
//	init_thrd = kthread_run(goodix_later_init_thread,
//			ts_core, "goodix_init_thread");
//	if (IS_ERR_OR_NULL(init_thrd)) {
//		ts_err("Failed to create update thread:%ld",
//				PTR_ERR(init_thrd));
//		return -EFAULT;
//	}
//	return 0;
//}

/**
 * goodix_ts_probe - called by kernel when Goodix touch
 *  platform driver is added.
 */
static int goodix_ts_probe(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = NULL;
	struct goodix_bus_interface *bus_interface;
	struct sec_ts_plat_data *pdata;
	int ret;
	int retry = 3;

	ts_info("goodix_ts_probe IN");

	bus_interface = pdev->dev.platform_data;
	if (!bus_interface) {
		ts_err("Invalid touch device");
		core_module_prob_state = CORE_MODULE_PROB_FAILED;
		return -ENODEV;
	}

	pdata = bus_interface->dev->platform_data;
	if (!pdata) {
		ts_err("Invalid platform data");
		core_module_prob_state = CORE_MODULE_PROB_FAILED;
		return -ENODEV;
	}

	core_data = dev_get_drvdata(bus_interface->dev);
	if (!core_data) {
		ts_err("get core_data from bus device");
		core_module_prob_state = CORE_MODULE_PROB_FAILED;
		return -ENODEV;
	}

	/* touch core layer is a platform driver */
	core_data->pdev = pdev;
	core_data->bus = bus_interface;

	if (IS_ENABLED(CONFIG_OF) && bus_interface->dev->of_node) {
		/* parse devicetree property */
		ret = goodix_parse_dt(bus_interface->dev, core_data);
		if (ret) {
			ts_err("failed parse device info form dts, %d", ret);
			return -EINVAL;
		}
	} else {
		ts_err("no valid device tree node found");
		return -ENODEV;
	}

	core_data->hw_ops = goodix_get_hw_ops();
	if (!core_data->hw_ops) {
		ts_err("hw ops is NULL");
		core_module_prob_state = CORE_MODULE_PROB_FAILED;
		return -EINVAL;
	}
	goodix_core_module_init();

	core_data->plat_data = pdata;
	core_data->plat_data->pinctrl_configure = sec_input_pinctrl_configure;
	core_data->plat_data->set_grip_data = goodix_set_grip_data_to_ic;
	core_data->plat_data->init = goodix_ts_reinit;
	core_data->plat_data->power = sec_input_power;
	core_data->plat_data->lpmode = gsx_set_lowpowermode;

	platform_set_drvdata(pdev, core_data);

	INIT_DELAYED_WORK(&core_data->work_read_info, goodix_ts_read_info_work);
	INIT_DELAYED_WORK(&core_data->work_print_info, goodix_ts_print_info_work);

	/* for debugging */
	INIT_DELAYED_WORK(&core_data->debug_delayed_work, debug_delayed_work_func);

	init_completion(&core_data->resume_done);
	complete_all(&core_data->resume_done);

retry_dev_confirm:
	ret = goodix_ts_power_on(core_data);
	if (ret) {
		ts_err("failed power on");

		if (--retry > 0) {
			sec_delay(500);
			goto retry_dev_confirm;
		}
		goto err_out;
	}

	/* generic notifier callback */
	core_data->ts_notifier.notifier_call = goodix_generic_noti_callback;
	goodix_ts_register_notifier(&core_data->ts_notifier);

	/* Try start a thread to get config-bin info */
//	ret = goodix_start_later_init(core_data);
//	if (ret) {
//		ts_err("Failed start cfg_bin_proc, %d", ret);
//		goto err_out;
//	}

	/* debug node init */
	goodix_tools_init();

	core_data->init_stage = CORE_INIT_STAGE1;
	goodix_modules.core_data = core_data;
	core_module_prob_state = CORE_MODULE_PROB_SUCCESS;
	mutex_init(&core_data->modechange_mutex);
	ts_info("goodix_ts_core init stage1 success");

	ret = goodix_later_init_thread(core_data);
	if (ret) {
		ts_err("Failed to later init");
		goto err_out;
	}

	goodix_get_custom_library(core_data);
	goodix_set_custom_library(core_data);

	if (core_data->bus->ic_type == IC_TYPE_BERLIN_D) {
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		manager_notifier_register(&core_data->ccic_nb, goodix_ccic_notification, MANAGER_NOTIFY_PDIC_INITIAL);
		ts_info("goodix_ts_core goodix_ccic_notification");
#endif
		vbus_notifier_register(&core_data->vbus_nb, goodix_vbus_notification, VBUS_NOTIFY_DEV_CHARGER);
		ts_info("goodix_ts_core goodix_vbus_notification");
#endif
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_register_notify(&core_data->sec_input_nb, goodix_input_notify_call, 1);
#endif
	ts_info("goodix_ts_core probe success");
	input_log_fix();

	if (!core_data->plat_data->shutdown_called)
		schedule_delayed_work(&core_data->work_read_info, msecs_to_jiffies(50));

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	pdata->stui_tsp_enter = goodix_stui_tsp_enter;
	pdata->stui_tsp_exit = goodix_stui_tsp_exit;
	pdata->stui_tsp_type = goodix_stui_tsp_type;
#endif

	sec_cmd_send_event_to_user(&core_data->sec, NULL, "RESULT=PROBE_DONE");

	return 0;

err_out:
	core_data->init_stage = CORE_INIT_FAIL;
	core_module_prob_state = CORE_MODULE_PROB_FAILED;
	mutex_destroy(&core_data->modechange_mutex);
	ts_err("goodix_ts_core failed, ret:%d", ret);
	return ret;
}

static int goodix_ts_remove(struct platform_device *pdev)
{
	struct goodix_ts_core *core_data = platform_get_drvdata(pdev);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_ts_esd *ts_esd = &core_data->ts_esd;

	ts_info("called");

	core_data->plat_data->shutdown_called = true;
	cancel_delayed_work_sync(&core_data->work_read_info);
	cancel_delayed_work_sync(&core_data->work_print_info);

	/* for debugging */
	cancel_delayed_work_sync(&core_data->debug_delayed_work);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_unregister_notify(&core_data->sec_input_nb);
#endif
	goodix_ts_unregister_notifier(&core_data->ts_notifier);
	goodix_tools_exit();
	mutex_destroy(&core_data->modechange_mutex);

	if (core_data->init_stage >= CORE_INIT_STAGE2) {
		wakeup_source_unregister(core_data->sec_ws);
		goodix_ts_cmd_remove(core_data);
		gesture_module_exit();
		inspect_module_exit();
		hw_ops->irq_enable(core_data, false);

		core_module_prob_state = CORE_MODULE_REMOVED;
		if (atomic_read(&core_data->ts_esd.esd_on))
			goodix_ts_esd_off(core_data);
		goodix_ts_unregister_notifier(&ts_esd->esd_notifier);

		goodix_fw_update_uninit();
		goodix_ts_sysfs_exit(core_data);
		goodix_ts_procfs_exit(core_data);
		goodix_ts_power_off(core_data);
	}

	return 0;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops dev_pm_ops = {
	.suspend = goodix_ts_pm_suspend,
	.resume = goodix_ts_pm_resume,
};
#endif

static const struct platform_device_id ts_core_ids[] = {
	{.name = GOODIX_CORE_DRIVER_NAME},
	{}
};
MODULE_DEVICE_TABLE(platform, ts_core_ids);

static struct platform_driver goodix_ts_driver = {
	.driver = {
		.name = GOODIX_CORE_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &dev_pm_ops,
#endif
	},
	.probe = goodix_ts_probe,
	.remove = goodix_ts_remove,
	.id_table = ts_core_ids,
};

static int __init goodix_ts_core_init(void)
{
	int ret;

	ts_info("Core layer init:%s", GOODIX_DRIVER_VERSION);
	ret = goodix_i2c_bus_init();
	if (ret) {
		ts_err("failed add bus driver");
		return ret;
	}
	return platform_driver_register(&goodix_ts_driver);
}

static void __exit goodix_ts_core_exit(void)
{
	ts_info("Core layer exit");
	platform_driver_unregister(&goodix_ts_driver);
	goodix_i2c_bus_exit();
}

module_init(goodix_ts_core_init);
module_exit(goodix_ts_core_exit);

MODULE_DESCRIPTION("Goodix Touchscreen Core Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
