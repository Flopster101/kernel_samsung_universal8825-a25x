// SPDX-License-Identifier: GPL-2.0
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

#if KERNEL_VERSION(2, 6, 38) < LINUX_VERSION_CODE
#include <linux/input/mt.h>
#define INPUT_TYPE_B_PROTOCOL
#endif

#include "goodix_ts_core.h"

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif

struct goodix_module goodix_modules;
int core_module_prob_state = CORE_MODULE_UNPROBED;

static void goodix_ts_force_release_all_finger(struct goodix_ts_data *ts);

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_i2c_lock(struct i2c_adapter *adap);
extern int stui_i2c_unlock(struct i2c_adapter *adap);

static int goodix_stui_tsp_enter(void)
{
	struct goodix_ts_data *ts = dev_get_drvdata(ptsp);
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
	struct goodix_ts_data *ts = dev_get_drvdata(ptsp);
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
		if (module->funcs->init(goodix_modules.ts,
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
	struct goodix_ts_data *ts = container_of(work, struct goodix_ts_data,
			work_read_info.work);

	ts_info("module register work IN");

	/* driver probe failed */
	if (core_module_prob_state != CORE_MODULE_PROB_SUCCESS) {
		ts_err("Can't register ext_module core error");
		return;
	}

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		ts_err("shutdown was called");
		return ;
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

	ts_info("%s : IN", __func__);

	goodix_core_module_init();
	INIT_WORK(&module->work, goodix_register_ext_module_work);
	schedule_work(&module->work);

	ts_info("%s : OUT", __func__);
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
	ts_info("%s : IN", __func__);
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

	if (!goodix_modules.ts)
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
		module->funcs->exit(goodix_modules.ts, module);

	ts_info("Moudle [%s] unregistered",
			module->name ? module->name : " ");
	return 0;
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

static int goodix_parse_update_info(struct device_node *node,
		struct goodix_ts_data *ts)
{
	int ret;

	ret = of_property_read_u32(node, "sec,isp_ram_reg", &ts->isp_ram_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,flash_cmd_reg", &ts->flash_cmd_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,isp_buffer_reg", &ts->isp_buffer_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,config_data_reg", &ts->config_data_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,misctl_reg", &ts->misctl_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,watch_dog_reg", &ts->watch_dog_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,config_id_reg", &ts->config_id_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,enable_misctl_val", &ts->enable_misctl_val);
	if (ret < 0)
		return ret;

	return 0;
}

static int goodix_test_prepare(struct device_node *node,
		struct goodix_ts_data *ts)
{
	struct property *prop;
	int arr_len;
	int size;
	int ret;

	ret = of_property_read_u32(node, "sec,max_drv_num", &ts->max_drv_num);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,max_sen_num", &ts->max_sen_num);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,short_test_time_reg", &ts->short_test_time_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,short_test_status_reg", &ts->short_test_status_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,short_test_result_reg", &ts->short_test_result_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,drv_drv_reg", &ts->drv_drv_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,sen_sen_reg", &ts->sen_sen_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,drv_sen_reg", &ts->drv_sen_reg);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,diff_code_reg", &ts->diff_code_reg);
	if (ret < 0)
		return ret;

	if (ts->bus->ic_type != IC_TYPE_GT9916K) {
		ret = of_property_read_u32(node, "sec,production_test_addr", &ts->production_test_addr);
		if (ret < 0)
			return ret;
	}
	ret = of_property_read_u32(node, "sec,switch_cfg_cmd", &ts->switch_cfg_cmd);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,switch_freq_cmd", &ts->switch_freq_cmd);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,snr_cmd", &ts->snr_cmd);
	if (ret < 0)
		return ret;
	ret = of_property_read_u32(node, "sec,sensitive_cmd", &ts->sensitive_cmd);
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

	ret = of_property_read_u32_array(node, "sec,drv_map", ts->drv_map, arr_len);
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

	ret = of_property_read_u32_array(node, "sec,sen_map", ts->sen_map, arr_len);
	if (ret < 0)
		return ret;

	ts_info("sen_map array size:%d", arr_len);

	ts->enable_esd_check = of_property_read_bool(node, "sec,enable_esd_check");
	ts_info("esd check %s", ts->enable_esd_check ? "enable" : "disable");

	return 0;
}

static int goodix_parse_dt(struct device *dev, struct goodix_ts_data *ts)
{
	struct device_node *node = dev->of_node;
	unsigned int ic_type;
	int r;

	if (!ts) {
		ts_err("invalid goodix_ts_data");
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
		ts->bus->ic_type = IC_TYPE_BERLIN_B;
	} else if (ic_type == 2) {
		ts_info("ic_type is GT9895");
		ts->bus->ic_type = IC_TYPE_BERLIN_D;
	} else if (ic_type == 3) {
		ts_info("ic_type is GT9916K");
		ts->bus->ic_type = IC_TYPE_GT9916K;
	} else {
		ts_err("invalid ic_type:%d", ic_type);
		return -EINVAL;
	}

	r = goodix_parse_update_info(node, ts);
	if (r) {
		ts_err("Failed to parse update info");
		return r;
	}

	r = goodix_test_prepare(node, ts);
	if (r)
		ts_err("Failed to get test information %d", r);

	return 0;
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
	struct goodix_ts_data *ts = data;
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_event *ts_event = &ts->ts_event;
	struct goodix_ts_esd *ts_esd = &ts->ts_esd;
	int ret;

	ts_esd->irq_status = true;
	ts->irq_trig_cnt++;

	ret = sec_input_handler_start(ts->bus->dev);
	if (ret == SEC_ERROR)
		return IRQ_HANDLED;

	if (sec_input_cmp_ic_status(ts->bus->dev, CHECK_LPMODE)) {
		mutex_lock(&goodix_modules.mutex);
		list_for_each_entry_safe(ext_module, next,
				&goodix_modules.head, list) {
			if (!ext_module->funcs->irq_event)
				continue;
			ret = ext_module->funcs->irq_event(ts, ext_module);
			if (ret == EVT_CANCEL_IRQEVT) {
				mutex_unlock(&goodix_modules.mutex);
				return IRQ_HANDLED;
			}
		}
		mutex_unlock(&goodix_modules.mutex);
	}

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_ALLEVENT)
		ts_info("irq_trig_cnt (%zu)", ts->irq_trig_cnt);

	/* read touch data from touch device */
	if (hw_ops->event_handler == NULL) {
		ts_err("hw_ops->event_handler is NULL");
		return IRQ_HANDLED;
	}

	ret = hw_ops->event_handler(ts, ts_event);
	if (likely(!ret)) {
		if (ts_event->event_type & EVENT_EMPTY)
			goodix_ts_force_release_all_finger(ts);
	}
	ts_event->event_type = EVENT_INVALID;	// clear event type

	return IRQ_HANDLED;
}

static void goodix_ts_force_release_all_finger(struct goodix_ts_data *ts)
{
	struct sec_ts_plat_data *pdata = ts->plat_data;
	struct goodix_ts_event *ts_event = &ts->ts_event;
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
	sec_input_release_all_finger(ts->bus->dev);

	/* clean event buffer */
	memset(ts_event, 0, sizeof(*ts_event));
}


void goodix_ts_release_all_finger(struct goodix_ts_data *ts)
{
	struct goodix_ts_event *ts_event = &ts->ts_event;

	sec_input_release_all_finger(ts->bus->dev);

	/* clean event buffer */
	memset(ts_event, 0, sizeof(*ts_event));
}

/**
 * goodix_ts_init_irq - Requset interrput line from system
 * @ts: pointer to touch core data
 * return: 0 ok, <0 failed
 */
static int goodix_ts_irq_setup(struct goodix_ts_data *ts)
{
	int ret;

	ts->irq = gpio_to_irq(ts->plat_data->irq_gpio);
	if (ts->irq < 0) {
		ts_err("failed get irq num %d", ts->irq);
		return -EINVAL;
	}

	ts->irq_empty_count = 0;

	ts_info("IRQ:%u, flag:0x%04X", ts->irq, IRQF_TRIGGER_FALLING | IRQF_ONESHOT);
	ret = devm_request_threaded_irq(ts->bus->dev,
			ts->irq, NULL,
			goodix_ts_threadirq_func,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			GOODIX_CORE_DRIVER_NAME,
			ts);
	if (ret < 0)
		ts_err("Failed to requeset threaded irq:%d", ret);

	return ret;
}

/**
 * goodix_ts_power_on - Turn on power to the touch device
 * @ts: pointer to goodix_ts_data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_on(struct goodix_ts_data *ts)
{
	int ret = 0;

	ts_info("power on %d", atomic_read(&ts->plat_data->power_state));
	ts->plat_data->pinctrl_configure(ts->bus->dev, true);

	if (atomic_read(&ts->plat_data->power_state) == SEC_INPUT_STATE_POWER_ON) {
		ts_err("already power on");
		return 0;
	}

	ret = ts->hw_ops->power_on(ts, true);
	if (!ret)
		atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_ON);
	else
		ts_err("failed power on, %d", ret);

	return ret;
}

/**
 * goodix_ts_power_off - Turn off power to the touch device
 * @ts: pointer to goodix_ts_data
 * return: 0 ok, <0 failed
 */
int goodix_ts_power_off(struct goodix_ts_data *ts)
{
	int ret;

	ts_info("power off %d", atomic_read(&ts->plat_data->power_state));
	if (atomic_read(&ts->plat_data->power_state) == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("already power off");
		return 0;
	}

	ret = ts->hw_ops->power_on(ts, false);
	if (!ret)
		atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_OFF);
	else
		ts_err("failed power off, %d", ret);

	ts->plat_data->pinctrl_configure(ts->bus->dev, false);

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
	struct goodix_ts_data *ts = container_of(ts_esd,
			struct goodix_ts_data, ts_esd);
	const struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	int ret = 0;

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		ts_err("shutdown was called");
		return;
	}

	if (ts_esd->irq_status)
		goto exit;

	if (!atomic_read(&ts_esd->esd_on))
		return;

	if (!hw_ops->esd_check)
		return;

	ret = hw_ops->esd_check(ts);
	if (ret) {
		ts_err("esd check failed");
		mutex_lock(&ts->plat_data->enable_mutex);
		ts->hw_ops->reset(ts, 100);
		/* reinit */
		ts->plat_data->init(ts);
		mutex_unlock(&ts->plat_data->enable_mutex);
	}

exit:
	ts_esd->irq_status = false;
	if (atomic_read(&ts_esd->esd_on))
		schedule_delayed_work(&ts_esd->esd_work, 2 * HZ);
}

/**
 * goodix_ts_esd_on - turn on esd protection
 */
static void goodix_ts_esd_on(struct goodix_ts_data *ts)
{
	struct goodix_ic_info_misc *misc = &ts->ic_info.misc;
	struct goodix_ts_esd *ts_esd = &ts->ts_esd;

	if (!misc->esd_addr)
		return;

	if (atomic_read(&ts_esd->esd_on))
		return;

	if (IS_ERR_OR_NULL(&ts_esd->esd_work.work))
		return;

	atomic_set(&ts_esd->esd_on, 1);
	if (!schedule_delayed_work(&ts_esd->esd_work, 2 * HZ))
		ts_info("esd work already in workqueue");

	ts_info("esd on");
}

/**
 * goodix_ts_esd_off - turn off esd protection
 */
static void goodix_ts_esd_off(struct goodix_ts_data *ts)
{
	struct goodix_ts_esd *ts_esd = &ts->ts_esd;
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
		goodix_ts_esd_off(ts_esd->ts);
		break;
	case NOTIFY_FWUPDATE_FAILED:
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_RESUME:
	case NOTIFY_ESD_ON:
		goodix_ts_esd_on(ts_esd->ts);
		break;
	default:
		break;
	}

	return 0;
}

/**
 * goodix_ts_esd_init - initialize esd protection
 */
int goodix_ts_esd_init(struct goodix_ts_data *ts)
{
	struct goodix_ic_info_misc *misc = &ts->ic_info.misc;
	struct goodix_ts_esd *ts_esd = &ts->ts_esd;

	if (!ts->hw_ops->esd_check || !misc->esd_addr) {
		ts_info("missing key info for esd check");
		return 0;
	}

	INIT_DELAYED_WORK(&ts_esd->esd_work, goodix_ts_esd_work);
	ts_esd->ts = ts;
	atomic_set(&ts_esd->esd_on, 0);
	ts_esd->esd_notifier.notifier_call = goodix_esd_notifier_callback;
	goodix_ts_register_notifier(&ts_esd->esd_notifier);
	goodix_ts_esd_on(ts);

	return 0;
}


/**
 * goodix_ts_suspend - Touchscreen suspend function
 * Called by PM/FB/EARLYSUSPEN module to put the device to  sleep
 */
static int goodix_ts_suspend(struct goodix_ts_data *ts)
{
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	int ret;

	if (ts->init_stage < CORE_INIT_STAGE2)
		return 0;

	ts_info("Suspend start, lp:0x%x ed:%d pocket_mode:%d fod_lp_mode:%d",
			ts->plat_data->lowpower_mode, ts->plat_data->ed_enable,
			ts->plat_data->pocket_mode, ts->plat_data->fod_lp_mode);
	/* disable irq */
	hw_ops->irq_enable(ts, false);

	/* inform external module */
	if (!sec_input_need_ic_off(ts->plat_data)) {
		mutex_lock(&goodix_modules.mutex);
		if (!list_empty(&goodix_modules.head)) {
			list_for_each_entry_safe(ext_module, next,
					&goodix_modules.head, list) {
				if (!ext_module->funcs->before_suspend)
					continue;

				ret = ext_module->funcs->before_suspend(ts,
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
	goodix_ts_power_off(ts);
//	if (hw_ops->suspend)
//		hw_ops->suspend(ts);

out:
	goodix_ts_release_all_finger(ts);
	ts_info("Suspend end");
	return 0;
}

void goodix_ts_reinit(void *data)
{
	struct goodix_ts_data *ts = (struct goodix_ts_data *)data;

	ts_info("Power mode=0x%x", atomic_read(&ts->plat_data->power_state));

	goodix_ts_release_all_finger(ts);
	atomic_set(&ts->plat_data->touch_noise_status, 0);
	atomic_set(&ts->plat_data->touch_pre_noise_status, 0);
	ts->plat_data->wet_mode = 0;

	if (ts->plat_data->charger_flag)
		ts->plat_data->set_charger_mode(ts->bus->dev, true);

	goodix_set_custom_library(ts);
	goodix_set_press_property(ts);

	if (ts->plat_data->support_fod && ts->plat_data->fod_data.set_val)
		goodix_set_fod_rect(ts);

	if (atomic_read(&ts->plat_data->power_state) == SEC_INPUT_STATE_LPM) {
		ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
		if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_AOD)
			goodix_set_aod_rect(ts);
	} else {
		sec_input_set_grip_type(ts->bus->dev, ONLY_EDGE_HANDLER);
	}

	if (ts->plat_data->ed_enable)
		ts->hw_ops->ed_enable(ts, ts->plat_data->ed_enable);

	if (ts->plat_data->pocket_mode)
		ts->hw_ops->pocket_mode_enable(ts, ts->plat_data->pocket_mode);

	if (ts->refresh_rate)
		set_refresh_rate_mode(ts);

	if (ts->flip_enable) {
		ts_info("set cover close [%d]", ts->plat_data->cover_type);
		goodix_set_cover_mode(ts);
	}

	if (ts->glove_enable) {
		ts_info("set glove mode on");
		goodix_set_cmd(ts, GOODIX_GLOVE_MODE_ADDR, ts->glove_enable);
	}

	if (ts->plat_data->low_sensitivity_mode) {
		ts_info("set low sensitivity mode on");
		goodix_set_cmd(ts, GOODIX_LS_MODE_ADDR, ts->plat_data->low_sensitivity_mode);
	}
}

/**
 * goodix_ts_resume - Touchscreen resume function
 * Called by PM/FB/EARLYSUSPEN module to wakeup device
 */
static int goodix_ts_resume(struct goodix_ts_data *ts)
{
	struct goodix_ext_module *ext_module, *next;
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	int ret;

	if (ts->init_stage < CORE_INIT_STAGE2)
		return 0;

	ts_info("Resume start");

	if (atomic_read(&ts->plat_data->power_state) == SEC_INPUT_STATE_LPM) {
		/* disable irq */
		hw_ops->irq_enable(ts, false);

		mutex_lock(&goodix_modules.mutex);
		if (!list_empty(&goodix_modules.head)) {
			list_for_each_entry_safe(ext_module, next,
					&goodix_modules.head, list) {
				if (!ext_module->funcs->before_resume)
					continue;

				ret = ext_module->funcs->before_resume(ts,
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
	goodix_ts_power_on(ts);
//	if (hw_ops->resume)
//		hw_ops->resume(ts);

	/* reinit */
	ts->plat_data->init(ts);
out:

	/* enable irq */
	hw_ops->irq_enable(ts, true);
	ts_info("Resume end");
	return 0;
}

/* for debugging */
static void debug_delayed_work_func(struct work_struct *work)
{
	struct goodix_ts_data *ts = goodix_modules.ts;
	u8 buf[16 * 34 * 2];
	u8 put[256] = {0};
	int i;
	int cnt = 0;

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		ts_err("shutdown was called");
		return;
	}

	ts->hw_ops->read(ts, 0x10308, buf, 40);
	for (i = 0; i < 40; i++)
		cnt += sprintf(&put[cnt], "%x,", buf[i]);
	ts_info("0x10308:%s", put);
}

static int goodix_ts_enable(struct device *dev)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&ts->work_read_info);

	ts_info("called");
	atomic_set(&ts->plat_data->enabled, true);
	ts->plat_data->prox_power_off = 0;
	goodix_ts_resume(ts);

	cancel_delayed_work(&ts->work_print_info);
	ts->plat_data->print_info_cnt_open = 0;
	ts->plat_data->print_info_cnt_release = 0;

	if (!atomic_read(&ts->plat_data->shutdown_called))
		schedule_work(&ts->work_print_info.work);

	/* for debugging */
	schedule_delayed_work(&ts->debug_delayed_work, HZ);

	return 0;
}

static int goodix_ts_disable(struct device *dev)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&ts->work_read_info);

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		ts_err("shutdown was called");
		return 0;
	}

	ts_info("called");
	atomic_set(&ts->plat_data->enabled, false);

	/* for debugging */
	cancel_delayed_work_sync(&ts->debug_delayed_work);

	cancel_delayed_work(&ts->work_print_info);
	sec_input_print_info(ts->bus->dev, NULL);

	goodix_ts_suspend(ts);

	return 0;
}

#ifdef CONFIG_PM
/**
 * goodix_ts_pm_suspend - PM suspend function
 * Called by kernel during system suspend phrase
 */
static int goodix_ts_pm_suspend(struct device *dev)
{
	struct goodix_ts_data *ts =
		dev_get_drvdata(dev);

	//ts_info("enter");
	reinit_completion(&ts->plat_data->resume_done);
	return 0;
}
/**
 * goodix_ts_pm_resume - PM resume function
 * Called by kernel during system wakeup
 */
static int goodix_ts_pm_resume(struct device *dev)
{
	struct goodix_ts_data *ts =
		dev_get_drvdata(dev);

	//ts_info("enter");
	complete_all(&ts->plat_data->resume_done);
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
	struct goodix_ts_data *ts = container_of(self,
			struct goodix_ts_data, ts_notifier);
	const struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;

	if (ts->init_stage < CORE_INIT_STAGE2)
		return 0;

	ts_info("notify event type 0x%x", (unsigned int)action);
	switch (action) {
	case NOTIFY_FWUPDATE_START:
		hw_ops->irq_enable(ts, 0);
		break;
	case NOTIFY_FWUPDATE_SUCCESS:
	case NOTIFY_FWUPDATE_FAILED:
		if (hw_ops->read_version(ts, &ts->fw_version))
			ts_info("failed read fw version info[ignore]");
		hw_ops->irq_enable(ts, 1);
		break;
	default:
		break;
	}
	return 0;
}

int goodix_ts_stage2_init(struct goodix_ts_data *ts)
{
	int ret;

	ret = sec_input_device_register(ts->bus->dev, ts);
	if (ret) {
		ts_err("failed to register input device, %d", ret);
		return ret;
	}

	mutex_init(&ts->plat_data->enable_mutex);

	/* request irq line */
	ret = goodix_ts_irq_setup(ts);
	if (ret < 0) {
		ts_info("failed set irq");
		return ret;
	}
	ts_info("success register irq");

	/* create sysfs files */
	goodix_ts_sysfs_init(ts);

	/* create procfs files */
	goodix_ts_procfs_init(ts);

	/* esd protector */
	if (ts->enable_esd_check)
		goodix_ts_esd_init(ts);

	/* gesture init */
	gesture_module_init(ts);

	/* inspect init */
	inspect_module_init();

	atomic_set(&ts->plat_data->enabled, true);
	ts->plat_data->enable = goodix_ts_enable;
	ts->plat_data->disable = goodix_ts_disable;
	goodix_ts_cmd_init(ts);

	ts->plat_data->sec_ws = wakeup_source_register(NULL, "TSP");

	return 0;
}

static int goodix_check_update_skip(struct goodix_ts_data *ts, struct goodix_ic_info_sec *fw_info_bin)
{
	struct goodix_fw_version fw_version;
	struct goodix_ic_info ic_info;
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	int ret = 0;

	ts->plat_data->img_version_of_bin[SEC_INPUT_FW_IC_VER] = fw_info_bin->ic_name_list;
	ts->plat_data->img_version_of_bin[SEC_INPUT_FW_VER_PROJECT_ID] = fw_info_bin->project_id;
	ts->plat_data->img_version_of_bin[SEC_INPUT_FW_MODULE_VER] = fw_info_bin->module_version;
	ts->plat_data->img_version_of_bin[SEC_INPUT_FW_VER] = fw_info_bin->firmware_version;

	ret = hw_ops->read_version(ts, &fw_version);
	if (ret)
		return NEED_FW_UPDATE;

	if (hw_ops->get_ic_info(ts, &ic_info)) {
		ts_err("invalid ic info, abort");
		return NEED_FW_UPDATE;
	}

	if (ts->plat_data->bringup == 2) {
		ts_info("bringup 2, skip fw update");
		return SKIP_FW_UPDATE;
	} else if (ts->plat_data->bringup == 5) {
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

	if ((ts->plat_data->bringup == 3) &&
			((ic_info.sec.ic_name_list != fw_info_bin->ic_name_list) ||
			(ic_info.sec.project_id != fw_info_bin->project_id) ||
			(ic_info.sec.module_version != fw_info_bin->module_version) ||
			(ic_info.sec.firmware_version != fw_info_bin->firmware_version))) {
		ts_info("bringup 3, force fw update because fw version is not equal");
		return NEED_FW_UPDATE;
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
		ts_info("ic firmware version is lower than binary firmware version");
		return NEED_FW_UPDATE;
	}

	/* compare patch vid */
	if (fw_version.patch_vid[3] < ts->merge_bin_ver.patch_vid[3]) {
		ts_err("WARNING:chip VID[%x] < bin VID[%x], need upgrade",
				fw_version.patch_vid[3], ts->merge_bin_ver.patch_vid[3]);
		return NEED_FW_UPDATE;
	}

	ts_info("ic fw version is latest, skip fw update");

	return SKIP_FW_UPDATE;
}

int goodix_fw_update(struct goodix_ts_data *ts, int update_type, bool force_update)
{
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	struct goodix_ic_info_sec fw_info_bin;
	const char *fw_name;
	const struct firmware *firmware = NULL;
	int pid_offset;
	int ret;

	switch (update_type) {
	case TSP_BUILT_IN:
		if (ts->plat_data->bringup == 1) {
			ts_info("skip fw update because bringup 1");
			ret = 0;
			goto skip_update;
		}
		if (!ts->plat_data->firmware_name) {
			ts_err("firmware name is null");
			return -EINVAL;
		}
		fw_name = ts->plat_data->firmware_name;
		break;
	case TSP_SDCARD:
		fw_name = TSP_EXTERNAL_FW;
		break;
	default:
		ts_err("update_type %d is invalid", update_type);
		return -EINVAL;
	}

	ret = request_firmware(&firmware, fw_name, ts->bus->dev);
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
	memcpy(ts->merge_bin_ver.patch_pid, firmware->data + pid_offset, 8);
	memcpy(ts->merge_bin_ver.patch_vid, firmware->data + pid_offset + 8, 4);

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
		memcpy(&ts->fw_info_bin, &fw_info_bin, sizeof(struct goodix_ic_info_sec));

	if (!force_update && (update_type == TSP_BUILT_IN)) {
		ret = goodix_check_update_skip(ts, &fw_info_bin);
		if (ret == SKIP_FW_UPDATE) {
			ts_info("skip fw update");
			goto skip_update;
		}
	}

	/* setp 1: get config data from config bin */
	ret = goodix_get_config_proc(ts, firmware);
	if (ret < 0)
		ts_info("no valid ic config found");
	else
		ts_info("success get valid ic config");

	/* setp 2: init fw struct add try do fw upgrade */
	ret = goodix_fw_update_init(ts, firmware);
	if (ret) {
		ts_err("failed init fw update module");
		goto out;
	}

	ret = goodix_do_fw_update(ts->ic_configs[CONFIG_TYPE_NORMAL]);
	if (ret)
		ts_err("failed do fw update");

skip_update:
	/* setp3: get fw version and ic_info
	 * at this step we believe that the ic is in normal mode,
	 * if the version info is invalid there must have some
	 * problem we cann't cover so exit init directly.
	 */
	if (hw_ops->read_version(ts, &ts->fw_version)) {
		ts_err("invalid fw version, abort");
		ret = -EIO;
		goto out;
	}
	if (hw_ops->get_ic_info(ts, &ts->ic_info)) {
		ts_err("invalid ic info, abort");
		ret = -EIO;
		goto out;
	}

	ts_info("done");

out:
	if (ts->plat_data->bringup != 1)
		release_firmware(firmware);
	return ret;
}

static int goodix_set_charger(struct device *dev, bool on)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	struct goodix_ts_cmd temp_cmd;
	int ret;

	if (on) {
		temp_cmd.cmd = 0xAF;
		temp_cmd.data[0] = 1;
		temp_cmd.len = 5;
	} else {
		temp_cmd.cmd = 0xAF;
		temp_cmd.data[0] = 0;
		temp_cmd.len = 5;
	}

	ret = ts->hw_ops->send_cmd(ts, &temp_cmd);
	if (ret < 0)
		ts_err("send charger cmd(%d) failed(%d)", on, ret);
	else
		ts_info("set charger %s", on ? "ON" : "OFF");

	return ret;

}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
static int goodix_set_pen_mode(struct goodix_ts_data *ts, bool pen_in)
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

	ret = ts->hw_ops->send_cmd_delay(ts, &temp_cmd, 0);
	if (ret < 0)
		ts_err("send pen mode cmd failed");

	return ret;
}

static int goodix_input_notify_call(struct notifier_block *n, unsigned long data, void *v)
{
	struct goodix_ts_data *ts = container_of(n, struct goodix_ts_data, sec_input_nb);

	switch (data) {
	case NOTIFIER_WACOM_PEN_HOVER_IN:
		goodix_set_pen_mode(ts, true);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_OUT:
		goodix_set_pen_mode(ts, false);
		break;
	default:
		break;
	}

	return 0;
}
#endif

static void goodix_set_grip_data_to_ic(struct device *dev, u8 flag)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	struct goodix_ts_cmd temp_cmd;
	int ret;

	ts_info("flag: %02X (clr,lan,nor,edg,han)", flag);

	if (flag & G_SET_EDGE_HANDLER) {
		temp_cmd.len = 0x0A;
		temp_cmd.cmd = 0x68;
		temp_cmd.data[0] = 0x00;
		if (ts->plat_data->grip_data.edgehandler_direction == 0) {
			temp_cmd.data[1] = 0;
			temp_cmd.data[2] = 0;
			temp_cmd.data[3] = 0;
			temp_cmd.data[4] = 0;
			temp_cmd.data[5] = 0;
		} else {
			temp_cmd.data[1] = ts->plat_data->grip_data.edgehandler_direction;
			temp_cmd.data[2] = ts->plat_data->grip_data.edgehandler_start_y & 0xFF;
			temp_cmd.data[3] = (ts->plat_data->grip_data.edgehandler_start_y >> 8) & 0xFF;
			temp_cmd.data[4] = ts->plat_data->grip_data.edgehandler_end_y & 0xFF;
			temp_cmd.data[5] = (ts->plat_data->grip_data.edgehandler_end_y >> 8) & 0xFF;
		}
		ts_info("SET_EDGE_HANDLER: %02x %02x %02x %02x %02x",
				temp_cmd.data[1], temp_cmd.data[2],
				temp_cmd.data[3], temp_cmd.data[4],
				temp_cmd.data[5]);
		ret = ts->hw_ops->send_cmd(ts, &temp_cmd);
		if (ret < 0)
			ts_err("send grip data to ic failed");
	}

	if (flag & G_SET_NORMAL_MODE) {
		temp_cmd.len = 0x0A;
		temp_cmd.cmd = 0x68;
		temp_cmd.data[0] = 0x01;
		temp_cmd.data[1] = ts->plat_data->grip_data.edge_range;
		temp_cmd.data[2] = ts->plat_data->grip_data.deadzone_up_x;
		temp_cmd.data[3] = ts->plat_data->grip_data.deadzone_dn_x;
		temp_cmd.data[4] = ts->plat_data->grip_data.deadzone_y & 0xFF;
		temp_cmd.data[5] = (ts->plat_data->grip_data.deadzone_y >> 8) & 0xFF;
		ts_info("SET_NORMAL_MODE: %02x %02x %02x %02x %02x",
				temp_cmd.data[1], temp_cmd.data[2],
				temp_cmd.data[3], temp_cmd.data[4], temp_cmd.data[5]);
		ret = ts->hw_ops->send_cmd(ts, &temp_cmd);
		if (ret < 0)
			ts_err("send grip data to ic failed");
	}

	if (flag & G_SET_LANDSCAPE_MODE) {
		temp_cmd.len = 0x0C;
		temp_cmd.cmd = 0x68;
		temp_cmd.data[0] = 0x02;
		temp_cmd.data[1] = ts->plat_data->grip_data.landscape_mode;
		temp_cmd.data[2] = ts->plat_data->grip_data.landscape_edge;
		temp_cmd.data[3] = ts->plat_data->grip_data.landscape_deadzone;
		temp_cmd.data[4] = ts->plat_data->grip_data.landscape_top_deadzone;
		temp_cmd.data[5] = ts->plat_data->grip_data.landscape_bottom_deadzone;
		temp_cmd.data[6] = ts->plat_data->grip_data.landscape_top_gripzone;
		temp_cmd.data[7] = ts->plat_data->grip_data.landscape_bottom_gripzone;
		ts_info("SET_LANDSCAPE_MODE: %02x %02x %02x %02x %02x %02x %02x",
				temp_cmd.data[1], temp_cmd.data[2],
				temp_cmd.data[3], temp_cmd.data[4],
				temp_cmd.data[5], temp_cmd.data[6],
				temp_cmd.data[7]);
		ret = ts->hw_ops->send_cmd(ts, &temp_cmd);
		if (ret < 0)
			ts_err("send grip data to ic failed");
	}

	if (flag & G_CLR_LANDSCAPE_MODE) {
		temp_cmd.len = 6;
		temp_cmd.cmd = 0x68;
		temp_cmd.data[0] = 0x03;
		temp_cmd.data[1] = ts->plat_data->grip_data.landscape_mode;
		ts_info("CLR_LANDSCAPE_MODE");
		ret = ts->hw_ops->send_cmd(ts, &temp_cmd);
		if (ret < 0)
			ts_err("send grip data to ic failed");
	}
}

void goodix_ts_print_info_work(struct work_struct *work)
{
	struct goodix_ts_data *ts = container_of(work, struct goodix_ts_data,
			work_print_info.work);

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		ts_err("shutdown was called");
		return;
	}

	sec_input_print_info(ts->bus->dev, NULL);

	if (!atomic_read(&ts->plat_data->shutdown_called))
		schedule_delayed_work(&ts->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

void goodix_ts_read_info_work(struct work_struct *work)
{
	struct goodix_ts_data *ts = container_of(work, struct goodix_ts_data,
			work_read_info.work);

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		ts_err("shutdown was called");
		return;
	}

	goodix_ts_run_rawdata_all(ts);

	/* reinit */
	ts->plat_data->init(ts);

	ts->info_work_done = true;

	if (!atomic_read(&ts->plat_data->shutdown_called))
		schedule_work(&ts->work_print_info.work);
}


/**
 * goodix_later_init_thread - init IC fw and config
 * @data: point to goodix_ts_data
 *
 * This function respond for get fw version and try upgrade fw and config.
 * Note: when init encounter error, need release all resource allocated here.
 */
static int goodix_later_init_thread(void *data)
{
	int ret, i;
	struct goodix_ts_data *ts = data;
	bool update_flag = false;

	ts_info("start");

	/* dev confirm again. If failed, it means the wrong FW and need to force update */
	ret = ts->hw_ops->dev_confirm(ts);
	if (ret < 0) {
		ts_info("device confirm again failed, maybe wrong FW, need update");
		update_flag = true;
	}

	ret = goodix_fw_update(ts, TSP_BUILT_IN, update_flag);
	if (ret) {
		ts_err("update failed");
		goto uninit_fw;
	}

	/* init other resources */
	ret = goodix_ts_stage2_init(ts);
	if (ret) {
		ts_err("stage2 init failed");
		goto uninit_fw;
	}
	ts->init_stage = CORE_INIT_STAGE2;

	return 0;

uninit_fw:
	ts_err("stage2 init failed");
	ts->init_stage = CORE_INIT_FAIL;
	for (i = 0; i < GOODIX_MAX_CONFIG_GROUP; i++) {
		if (ts->ic_configs[i])
			kfree(ts->ic_configs[i]);
		ts->ic_configs[i] = NULL;
	}
	return ret;
}

/**
 * goodix_ts_probe - called by kernel when Goodix touch
 *  platform driver is added.
 */
static int goodix_ts_probe(struct platform_device *pdev)
{
	struct goodix_ts_data *ts = NULL;
	struct goodix_bus_interface *bus_interface;
	struct sec_ts_plat_data *pdata;
	int ret;
	int retry = 3;

	ts_info("%s : IN", __func__);

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

	ts = dev_get_drvdata(bus_interface->dev);
	if (!ts) {
		ts_err("get ts from bus device");
		core_module_prob_state = CORE_MODULE_PROB_FAILED;
		return -ENODEV;
	}

	/* touch core layer is a platform driver */
	ts->pdev = pdev;
	ts->bus = bus_interface;

	if (IS_ENABLED(CONFIG_OF) && bus_interface->dev->of_node) {
		/* parse devicetree property */
		ret = goodix_parse_dt(bus_interface->dev, ts);
		if (ret) {
			ts_err("failed parse device info form dts, %d", ret);
			return -EINVAL;
		}
	} else {
		ts_err("no valid device tree node found");
		return -ENODEV;
	}

	ts->hw_ops = goodix_get_hw_ops();
	if (!ts->hw_ops) {
		ts_err("hw ops is NULL");
		core_module_prob_state = CORE_MODULE_PROB_FAILED;
		return -EINVAL;
	}
	goodix_core_module_init();

	ts->plat_data = pdata;
	ts->plat_data->pinctrl_configure = sec_input_pinctrl_configure;
	ts->plat_data->set_grip_data = goodix_set_grip_data_to_ic;
	ts->plat_data->init = goodix_ts_reinit;
	ts->plat_data->power = sec_input_power;
	ts->plat_data->lpmode = gsx_set_lowpowermode;
	ts->plat_data->set_charger_mode = goodix_set_charger;

	platform_set_drvdata(pdev, ts);

	INIT_DELAYED_WORK(&ts->work_read_info, goodix_ts_read_info_work);
	INIT_DELAYED_WORK(&ts->work_print_info, goodix_ts_print_info_work);

	/* for debugging */
	INIT_DELAYED_WORK(&ts->debug_delayed_work, debug_delayed_work_func);

	init_completion(&ts->plat_data->resume_done);
	complete_all(&ts->plat_data->resume_done);

retry_dev_confirm:
	ret = goodix_ts_power_on(ts);
	if (ret) {
		ts_err("failed power on");

		if (--retry > 0) {
			sec_delay(500);
			goto retry_dev_confirm;
		}
		goto err_out;
	}

	/* generic notifier callback */
	ts->ts_notifier.notifier_call = goodix_generic_noti_callback;
	goodix_ts_register_notifier(&ts->ts_notifier);

	/* debug node init */
	goodix_tools_init();

	ts->init_stage = CORE_INIT_STAGE1;
	goodix_modules.ts = ts;
	core_module_prob_state = CORE_MODULE_PROB_SUCCESS;
	ts_info("goodix_ts_core init stage1 success");

	ret = goodix_later_init_thread(ts);
	if (ret) {
		ts_err("Failed to later init");
		goto err_out;
	}
	ts_info("goodix_ts_core init stage2 success");

	goodix_get_custom_library(ts);
	goodix_set_custom_library(ts);

	sec_input_register_vbus_notifier(ts->bus->dev);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_register_notify(&ts->sec_input_nb, goodix_input_notify_call, 1);
#endif
	ts_info("goodix_ts_core probe success");
	input_log_fix();

	if (!atomic_read(&ts->plat_data->shutdown_called))
		schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(50));

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	pdata->stui_tsp_enter = goodix_stui_tsp_enter;
	pdata->stui_tsp_exit = goodix_stui_tsp_exit;
	pdata->stui_tsp_type = goodix_stui_tsp_type;
#endif

	return 0;

err_out:
	ts->init_stage = CORE_INIT_FAIL;
	core_module_prob_state = CORE_MODULE_PROB_FAILED;
	ts_err("goodix_ts_core failed, ret:%d", ret);
	return ret;
}

static int goodix_ts_remove(struct platform_device *pdev)
{
	struct goodix_ts_data *ts = platform_get_drvdata(pdev);
	struct goodix_ts_hw_ops *hw_ops = ts->hw_ops;
	struct goodix_ts_esd *ts_esd = &ts->ts_esd;

	ts_info("called");

	ts->plat_data->enable = NULL;
	ts->plat_data->disable = NULL;

	atomic_set(&ts->plat_data->shutdown_called, true);
	cancel_delayed_work_sync(&ts->work_read_info);
	cancel_delayed_work_sync(&ts->work_print_info);

	/* for debugging */
	cancel_delayed_work_sync(&ts->debug_delayed_work);
	cancel_delayed_work_sync(&ts_esd->esd_work);

	goodix_ts_unregister_notifier(&ts->ts_notifier);
	goodix_tools_exit();

	if (ts->init_stage >= CORE_INIT_STAGE2) {
		sec_input_unregister_vbus_notifier(ts->bus->dev);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_unregister_notify(&ts->sec_input_nb);
#endif
		wakeup_source_unregister(ts->plat_data->sec_ws);
		goodix_ts_cmd_remove(ts);
		gesture_module_exit(ts);
		inspect_module_exit();
		hw_ops->irq_enable(ts, false);

		core_module_prob_state = CORE_MODULE_REMOVED;
		if (atomic_read(&ts->ts_esd.esd_on))
			goodix_ts_esd_off(ts);
		goodix_ts_unregister_notifier(&ts_esd->esd_notifier);

		goodix_ts_sysfs_exit(ts);
		goodix_ts_procfs_exit(ts);
		goodix_ts_power_off(ts);
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

	pr_info("Core layer init:%s", GOODIX_DRIVER_VERSION);
	ret = goodix_i2c_bus_init();
	if (ret) {
		pr_err("failed add bus driver");
		return ret;
	}
	return platform_driver_register(&goodix_ts_driver);
}

static void __exit goodix_ts_core_exit(void)
{
	pr_info("Core layer exit");
	platform_driver_unregister(&goodix_ts_driver);
	goodix_i2c_bus_exit();
}

module_init(goodix_ts_core_init);
module_exit(goodix_ts_core_exit);

MODULE_DESCRIPTION("Goodix Touchscreen Core Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
