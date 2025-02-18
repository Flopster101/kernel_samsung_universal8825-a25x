/****************************************************************************
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 * Bluetooth Quality of Service
 *
 ****************************************************************************/
#include <linux/types.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/moduleparam.h>
#include <asm/io.h>
#include <asm/termios.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/exynos_pm_qos.h>
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif
#include <scsc/scsc_mx.h>
#include <scsc/scsc_mifram.h>
#include <scsc/scsc_logring.h>

#include "scsc_bt_priv.h"

#ifdef CONFIG_SCSC_QOS

#define SCSC_BT_QOS_LOW_LEVEL 1
#define SCSC_BT_QOS_MED_LEVEL 3
#define SCSC_BT_QOS_HIGH_LEVEL 6

#define SCSC_BT_QOS_TIMEOUT 100

/* QoS level set
 * scsc_bt_qos_low_level, scsc_bt_qos_medium_level, scsc_bt_qos_high_level
 *
 * This QoS level set can be overwritten by scsc_bt_qos_platform_prob() on
 * kernel booting. Also, each value can be changed by writing module parameter
 * on runtime.
 */
static uint32_t scsc_bt_qos_low_level = SCSC_BT_QOS_LOW_LEVEL;
static uint32_t scsc_bt_qos_medium_level = SCSC_BT_QOS_MED_LEVEL;
static uint32_t scsc_bt_qos_high_level = SCSC_BT_QOS_HIGH_LEVEL;
static uint32_t scsc_bt_qos_timeout = SCSC_BT_QOS_TIMEOUT;

module_param(scsc_bt_qos_low_level, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_low_level,
		"Number of outstanding packets which triggers QoS low level");
module_param(scsc_bt_qos_medium_level, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_medium_level,
		"Number of outstanding packets which triggers QoS medium level");
module_param(scsc_bt_qos_high_level, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_high_level,
		"Number of outstanding packets which triggers QoS high level");
module_param(scsc_bt_qos_timeout, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_timeout,
		"Period of time which is used to disable QoS level ");

static struct scsc_qos_service qos_service;

static DEFINE_MUTEX(bt_qos_mutex);

static void scsc_bt_qos_update_work(struct work_struct *data)
{
	mutex_lock(&bt_qos_mutex);
	if (qos_service.enabled && qos_service.pending_state > qos_service.current_state) {
		qos_service.current_state = qos_service.pending_state;
		SCSC_TAG_DEBUG(BT_COMMON, "Bluetooth QoS update (State: %d)\n",
				(uint8_t)qos_service.current_state);
		scsc_service_pm_qos_update_request(bt_service.service, qos_service.current_state);
	}
	mutex_unlock(&bt_qos_mutex);
}

static void scsc_bt_qos_disable_work(struct work_struct *data)
{
	mutex_lock(&bt_qos_mutex);
	if (qos_service.enabled && qos_service.disabling) {
		qos_service.current_state = SCSC_QOS_DISABLED;
		qos_service.disabling = false;
		SCSC_TAG_DEBUG(BT_COMMON, "Bluetooth QoS disable (State: %d)\n",
				(uint8_t)qos_service.current_state);
		scsc_service_pm_qos_update_request(bt_service.service, qos_service.current_state);
	}
	mutex_unlock(&bt_qos_mutex);
}

void scsc_bt_qos_update(uint32_t number_of_outstanding_hci_events,
			uint32_t number_of_outstanding_acl_packets)
{
	if (qos_service.enabled) {
		enum scsc_qos_config next_state = SCSC_QOS_DISABLED;
		u32 next_level = max(number_of_outstanding_acl_packets,
				number_of_outstanding_hci_events);

		/* Calculate next PM state */
		if (next_level >= scsc_bt_qos_high_level)
			next_state = SCSC_QOS_MAX;
		else if (next_level >= scsc_bt_qos_medium_level)
			next_state = SCSC_QOS_MED;
		else if (next_level >= scsc_bt_qos_low_level)
			next_state = SCSC_QOS_MIN;

		mutex_lock(&bt_qos_mutex);
		if (qos_service.disabling || next_state > qos_service.current_state) {
			/* Update PM QoS settings without delay */
			qos_service.pending_state = next_state;
			qos_service.disabling = false;
			schedule_work(&qos_service.update_work);
		} else if (next_state == SCSC_QOS_DISABLED) {
			/* Disable PM QoS settings with delay */
			qos_service.disabling = true;
			mod_delayed_work(system_wq, &qos_service.disable_work,
					msecs_to_jiffies(scsc_bt_qos_timeout));
		}
		mutex_unlock(&bt_qos_mutex);
	}
}

void scsc_bt_qos_service_stop(void)
{
	/* Ensure no crossing of work and stop */
	mutex_lock(&bt_qos_mutex);
	qos_service.pending_state = SCSC_QOS_DISABLED;
	qos_service.disabling = false;
	if (qos_service.enabled) {
		scsc_service_pm_qos_remove_request(bt_service.service);
		qos_service.enabled = false;
	}
	mutex_unlock(&bt_qos_mutex);
}

void scsc_bt_qos_service_start(void)
{
	if (!scsc_service_pm_qos_add_request(bt_service.service, SCSC_QOS_DISABLED))
		qos_service.enabled = true;
	else
		qos_service.enabled = false;

	INIT_WORK(&qos_service.update_work, scsc_bt_qos_update_work);
	INIT_DELAYED_WORK(&qos_service.disable_work, scsc_bt_qos_disable_work);
	SCSC_TAG_INFO(BT_COMMON, "QoS level: low(%d) med(%d) high(%d)\n",
			scsc_bt_qos_low_level, scsc_bt_qos_medium_level,
			scsc_bt_qos_high_level);
}

static void scsc_bt_qos_level_reset(struct platform_device *pdev)
{
	if (of_property_read_u32(pdev->dev.of_node, "samsung,qos_level_low",
			&scsc_bt_qos_low_level))
		scsc_bt_qos_low_level = SCSC_BT_QOS_LOW_LEVEL;
	if (of_property_read_u32(pdev->dev.of_node, "samsung,qos_level_medium",
			&scsc_bt_qos_medium_level))
		scsc_bt_qos_medium_level = SCSC_BT_QOS_MED_LEVEL;
	if (of_property_read_u32(pdev->dev.of_node, "samsung,qos_level_high",
			&scsc_bt_qos_high_level))
		scsc_bt_qos_high_level = SCSC_BT_QOS_HIGH_LEVEL;
	if (of_property_read_u32(pdev->dev.of_node, "samsung,qos_timeout",
			&scsc_bt_qos_timeout))
		scsc_bt_qos_timeout = SCSC_BT_QOS_TIMEOUT;

	SCSC_TAG_INFO(BT_COMMON, "Reset QoS level: low(%d) med(%d) high(%d)\n",
			scsc_bt_qos_low_level, scsc_bt_qos_medium_level,
			scsc_bt_qos_high_level);
}

static struct platform_device *qos_pdev = NULL;
static int scsc_bt_qos_reset_set_param_cb(const char *buffer,
					    const struct kernel_param *kp)
{
	if (qos_pdev) {
		scsc_bt_qos_level_reset(qos_pdev);
		return 0;
	}
	return -ENXIO;
}

static struct kernel_param_ops scsc_bt_qos_reset_ops = {
	.set = scsc_bt_qos_reset_set_param_cb,
	.get = NULL,
};

module_param_cb(scsc_bt_qos_reset_levels, &scsc_bt_qos_reset_ops, NULL,
	S_IWUSR);
MODULE_PARM_DESC(scsc_bt_qos_reset_levels,
		 "Reset the QoS level values to device default value");

static int scsc_bt_qos_platform_probe(struct platform_device *pdev)
{
	qos_pdev = pdev;
	scsc_bt_qos_level_reset(pdev);
	return 0;
}

static const struct of_device_id scsc_bt_qos[] = {
	{ .compatible = "samsung,scsc_bt_qos" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, scsc_bt_qos);

static struct platform_driver platform_bt_qos_driver = {
	.probe  = scsc_bt_qos_platform_probe,
	.driver = {
		.name = "SCSC_BT_QOS",
		.owner = THIS_MODULE,
		.pm = NULL,
		.of_match_table = of_match_ptr(scsc_bt_qos),
	},
};

void scsc_bt_qos_service_init(void)
{
	int ret;
	qos_service.enabled = false;
	qos_service.pending_state = SCSC_QOS_DISABLED;
	qos_service.disabling = false;

	ret = platform_driver_register(&platform_bt_qos_driver);
	if (ret)
		SCSC_TAG_WARNING(BT_COMMON,
			"platform_driver_register for SCSC_BT_QOS is failed\n",
			ret);
}

void scsc_bt_qos_service_exit(void)
{
	qos_pdev = NULL;
	platform_driver_unregister(&platform_bt_qos_driver);
}
#endif /* CONFIG_SCSC_QOS */
