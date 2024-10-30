/*
 * Driver for Power key/Vol Down key on s2mps25 IC by PWRON rising, falling interrupts.
 *
 * drivers/input/keyboard/s2mps25_keys.c
 * S2MPS25 Keyboard Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mfd/samsung/pmic_key.h>
#include <linux/mfd/samsung/s2mps25.h>
#include <linux/mfd/samsung/s2mps25-regulator.h>
#define WAKELOCK_TIME		HZ/10

static int force_key_irq_en = 0;

struct pmic_button_data {
	struct pmic_keys_button *button;
	struct input_dev *input;
	struct work_struct work;
	struct delayed_work key_work;
	struct workqueue_struct *irq_wqueue;
	bool key_pressed;
	bool suspended;
	int irq_key_p;
	int irq_key_r;
};

struct s2mps25_keys_drvdata {
	struct device *dev;
	struct s2mps25_platform_data *s2mps25_pdata;
	const struct pmic_keys_platform_data *pdata;
	struct input_dev *input;
	struct pmic_button_data button_data[0];
};

static int s2mps25_keys_wake_lock_timeout(struct device *dev, long timeout)
{
	struct wakeup_source *ws = NULL;

	if (!dev->power.wakeup) {
		pr_err("%s: Not register wakeup source\n", __func__);
		goto err;
	}

	ws = dev->power.wakeup;
	__pm_wakeup_event(ws, jiffies_to_msecs(timeout));

	return 0;
err:
	return -1;
}

static void s2mps25_keys_power_report_event(struct pmic_button_data *bdata)
{
	const struct pmic_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	struct s2mps25_keys_drvdata *ddata = input_get_drvdata(input);
	unsigned int type = button->type ?: EV_KEY;
	int state = bdata->key_pressed;

	if (s2mps25_keys_wake_lock_timeout(ddata->dev, WAKELOCK_TIME) < 0) {
		pr_err("%s: s2mps25_keys_wake_lock_timeout fail\n", __func__);
		return;
	}

	/* Report new key event */
	input_event(input, type, button->code, !!state);

	/* Sync new input event */
	input_sync(input);
}

static void s2mps25_keys_work_func(struct work_struct *work)
{
	struct pmic_button_data *bdata = container_of(work,
						      struct pmic_button_data,
						      key_work.work);

	s2mps25_keys_power_report_event(bdata);

	if (bdata->button->wakeup)
		pm_relax(bdata->input->dev.parent);
}

static irqreturn_t s2mps25_keys_press_irq_handler(int irq, void *dev_id)
{
	struct s2mps25_keys_drvdata *ddata = dev_id;
	int i = 0;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct pmic_button_data *bdata = &ddata->button_data[i];
		if (bdata->irq_key_p == irq) {
			bdata->key_pressed = true;

			if (bdata->button->wakeup) {
				const struct pmic_keys_button *button = bdata->button;

				pm_stay_awake(bdata->input->dev.parent);
				if (bdata->suspended  &&
				    (button->type == 0 || button->type == EV_KEY)) {
					/*
					 * Simulate wakeup key press in case the key has
					 * already released by the time we got interrupt
					 * handler to run.
					 */
					input_report_key(bdata->input, button->code, 1);
				}
			}

			queue_delayed_work(bdata->irq_wqueue, &bdata->key_work, 0);
		}

	}

	return IRQ_HANDLED;
}

static irqreturn_t s2mps25_keys_release_irq_handler(int irq, void *dev_id)
{
	struct s2mps25_keys_drvdata *ddata = dev_id;
	int i = 0;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct pmic_button_data *bdata = &ddata->button_data[i];
		if (bdata->irq_key_r == irq) {
			bdata->key_pressed = false;
			queue_delayed_work(bdata->irq_wqueue, &bdata->key_work, 0);
		}
	}

	return IRQ_HANDLED;
}

static void s2mps25_keys_report_state(struct s2mps25_keys_drvdata *ddata)
{
	int i;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct pmic_button_data *bdata = &ddata->button_data[i];

		bdata->key_pressed = false;
		s2mps25_keys_power_report_event(bdata);
	}
}

static int s2mps25_keys_open(struct input_dev *input)
{
	struct s2mps25_keys_drvdata *ddata = input_get_drvdata(input);

	dev_info(ddata->dev, "%s()\n", __func__);

	s2mps25_keys_report_state(ddata);

	return 0;
}

static void s2mps25_keys_close(struct input_dev *input)
{
	struct s2mps25_keys_drvdata *ddata = input_get_drvdata(input);

	dev_info(ddata->dev, "%s()\n", __func__);
}

#if IS_ENABLED(CONFIG_OF)
static struct pmic_keys_platform_data *
s2mps25_keys_get_devtree_pdata(struct s2mps25_dev *iodev)
{
	#define S2MPS25_SUPPORT_KEY_NUM	(2)	// power, vol-down key
	struct device *dev = iodev->dev;
	struct device_node *mfd_np, *key_np, *pp;
	struct pmic_keys_platform_data *pdata;
	struct pmic_keys_button *button;
	int error, nbuttons, i;
	size_t size;

	if (!iodev->dev->of_node) {
		pr_err("%s: error\n", __func__);
		error = -ENODEV;
		goto err_out;
	}

	mfd_np = iodev->dev->of_node;
	if (!mfd_np) {
		pr_err("%s: could not find parent_node\n", __func__);
		error = -ENODEV;
		goto err_out;
	}

	key_np = of_find_node_by_name(mfd_np, "s2mps25-keys");
	if (!key_np) {
		pr_err("%s: could not find current_node\n", __func__);
		error = -ENOENT;
		goto err_out;
	}

	nbuttons = of_get_child_count(key_np);
	if (nbuttons != S2MPS25_SUPPORT_KEY_NUM) {
		dev_warn(dev, "%s: s2mps25-keys support only two button(%d)\n",
			__func__, nbuttons);
		error = -ENODEV;
		goto err_out;
	}

	size = sizeof(*pdata) + nbuttons * sizeof(*button);
	pdata = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!pdata) {
		error = -ENOMEM;
		goto err_out;
	}

	pdata->buttons = (struct pmic_keys_button *)(pdata + 1);
	pdata->nbuttons = nbuttons;

	i = 0;
	for_each_child_of_node(key_np, pp) {
		button = &pdata->buttons[i++];
		if (of_property_read_u32(pp, "linux,code", &button->code)) {
			error = -EINVAL;
			goto err_out;
		}

		button->desc = of_get_property(pp, "label", NULL);

		button->wakeup = of_property_read_bool(pp, "wakeup");

		if (of_property_read_u32(pp, "linux,input-type", &button->type))
			button->type = EV_KEY;
		if (of_property_read_u32(pp, "force_key_irq_en", &force_key_irq_en))
			force_key_irq_en = 0;
	}

	return pdata;
err_out:
	return ERR_PTR(error);
}

static struct of_device_id s2mps25_keys_of_match[] = {
	{ .compatible = "s2mps25-power-keys", },
	{ },
};
MODULE_DEVICE_TABLE(of, s2mps25_keys_of_match);
#else
static inline struct pmic_keys_platform_data *
s2mps25_keys_get_devtree_pdata(struct s2mps25_dev *iodev)
{
	return ERR_PTR(-ENODEV);
}
#endif

static void s2mps25_remove_key(struct pmic_button_data *bdata)
{
	pr_info("%s()\n", __func__);

	cancel_delayed_work_sync(&bdata->key_work);
}

static void s2mps25_keys_force_en_irq(struct s2mps25_keys_drvdata *ddata)
{
	int i;

	if (!force_key_irq_en)
		return;

	pr_info("%s()\n", __func__);

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct pmic_button_data *bdata = &ddata->button_data[i];
		enable_irq(bdata->irq_key_r);
		enable_irq(bdata->irq_key_p);
	}
}

static int s2mps25_keys_set_interrupt(struct s2mps25_keys_drvdata *ddata, int irq_base)
{
	#define POWER_KEY_IDX		(0)
	#define VOLDN_KEY_IDX		(1)
	struct device *dev = ddata->dev;
	int ret = 0, i, idx;
	static char irq_name[4][32] = {0, };
	char name[4][32] = {"PWRONR_IRQ", "PWRONP_IRQ", "VOLDNR_IRQ", "VOLDNP_IRQ"};

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct pmic_button_data *bdata = &ddata->button_data[i];
		idx = i * 2;
		if (i == POWER_KEY_IDX) {
			bdata->irq_key_r = irq_base + S2MPS25_PMIC_IRQ_PWRONR_INT1;
			bdata->irq_key_p = irq_base + S2MPS25_PMIC_IRQ_PWRONP_INT1;
		} else if (i == VOLDN_KEY_IDX) {
			bdata->irq_key_r = irq_base + S2MPS25_PMIC_IRQ_VOLDNR_INT3;
			bdata->irq_key_p = irq_base + S2MPS25_PMIC_IRQ_VOLDNP_INT3;
		}

		snprintf(irq_name[idx], sizeof(irq_name[idx]) - 1, "%s@%s",
			 name[idx], dev_name(ddata->dev));
		snprintf(irq_name[idx + 1], sizeof(irq_name[idx + 1]) - 1, "%s@%s",
			 name[idx + 1], dev_name(ddata->dev));

		ret = devm_request_threaded_irq(dev, bdata->irq_key_r, NULL,
						s2mps25_keys_release_irq_handler, 0,
						irq_name[idx], ddata);
		if (ret < 0) {
			dev_err(dev, "%s: fail to request %s: %d: %d\n",
				__func__, name[idx], bdata->irq_key_r, ret);
			goto err;
		}

		ret = devm_request_threaded_irq(dev, bdata->irq_key_p, NULL,
						s2mps25_keys_press_irq_handler, 0,
						irq_name[idx + 1], ddata);
		if (ret < 0) {
			dev_err(dev, "%s: fail to request %s: %d: %d\n",
				__func__, name[idx + 1], bdata->irq_key_p, ret);
			goto err;
		}
	}

	return 0;
err:
	return -1;
}

static int s2mps25_keys_set_buttondata(struct s2mps25_keys_drvdata *ddata,
				     struct input_dev *input, bool *wakeup)
{
	int cnt = 0;

	for (cnt = 0; cnt < ddata->pdata->nbuttons; cnt++) {
		struct pmic_keys_button *button = &ddata->pdata->buttons[cnt];
		struct pmic_button_data *bdata = &ddata->button_data[cnt];
		char device_name[32] = {0, };

		bdata->input = input;
		bdata->button = button;

		if (button->wakeup)
			*wakeup = 1;

		/* Dynamic allocation for workqueue name */
		snprintf(device_name, sizeof(device_name) - 1,
			"s2mps25-keys-wq%d@%s", cnt, dev_name(ddata->dev));

		bdata->irq_wqueue = create_singlethread_workqueue(device_name);
		if (!bdata->irq_wqueue) {
			pr_err("%s: fail to create workqueue\n", __func__);
			goto err;
		}
		INIT_DELAYED_WORK(&bdata->key_work, s2mps25_keys_work_func);

		input_set_capability(input, button->type ?: EV_KEY, button->code);
	}

	return cnt;
err:
	return -1;
}

static struct s2mps25_keys_drvdata *
s2mps25_keys_set_drvdata(struct platform_device *pdev,
			struct pmic_keys_platform_data *pdata,
			struct input_dev *input, struct s2mps25_dev *iodev)
{
	struct s2mps25_keys_drvdata *ddata = NULL;
	struct device *dev = &pdev->dev;
	size_t size;

	size = sizeof(*ddata) + pdata->nbuttons * sizeof(struct pmic_button_data);
	ddata = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!ddata) {
		dev_err(dev, "failed to allocate ddata.\n");
		return ERR_PTR(-ENOMEM);
	}

	ddata->dev	= dev;
	ddata->pdata	= pdata;
	ddata->input	= input;

	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);

	return ddata;
}

static int s2mps25_keys_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct s2mps25_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct pmic_keys_platform_data *pdata = NULL;
	struct s2mps25_keys_drvdata *ddata = NULL;
	struct input_dev *input;
	int ret = 0, count = 0;
	bool wakeup = 0;

	pr_info("%s: start\n", __func__);

	pdata = s2mps25_keys_get_devtree_pdata(iodev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);

	input = devm_input_allocate_device(dev);
	if (!input) {
		dev_err(dev, "failed to allocate state\n");
		ret = -ENOMEM;
		goto fail1;
	}

	input->name		= "sec-pmic-key";	//pdata->name ? : pdev->name;
	input->phys		= "s2mps25-keys/input0";
	input->dev.parent	= dev;
	input->open		= s2mps25_keys_open;
	input->close		= s2mps25_keys_close;

	input->id.bustype	= BUS_I2C;
	input->id.vendor	= 0x0001;
	input->id.product	= 0x0001;
	input->id.version	= 0x0100;

	ddata = s2mps25_keys_set_drvdata(pdev, pdata, input, iodev);
	if (IS_ERR(ddata)) {
		pr_err("%s: power_keys_set_drvdata fail\n", __func__);
		return PTR_ERR(ddata);
	}

	ret = s2mps25_keys_set_buttondata(ddata, input, &wakeup);
	if (ret < 0) {
		pr_err("%s: s2mps25_keys_set_buttondata fail\n", __func__);
		goto fail1;
	} else
		count = ret;

	ret = device_init_wakeup(dev, wakeup);
	if (ret < 0) {
		pr_err("%s: device_init_wakeup fail(%d)\n", __func__, ret);
		goto fail1;
	}

	ret = s2mps25_keys_set_interrupt(ddata, iodev->pdata->irq_base);
	if (ret < 0) {
		pr_err("%s: s2mps25_keys_set_interrupt fail\n", __func__);
		goto fail1;
	}

	ret = input_register_device(input);
	if (ret) {
		dev_err(dev, "%s: unable to register input device, error: %d\n",
			__func__, ret);
		goto fail2;
	}

	s2mps25_keys_force_en_irq(ddata);

	pr_info("%s: end\n", __func__);

	return 0;

fail2:
	while (--count >= 0) {
		struct pmic_button_data *bdata = &ddata->button_data[count];

		if (bdata->irq_wqueue)
			destroy_workqueue(bdata->irq_wqueue);

		s2mps25_remove_key(bdata);
	}
fail1:
	return ret;
}

static int s2mps25_keys_remove(struct platform_device *pdev)
{
	struct s2mps25_keys_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	int i;

	device_init_wakeup(&pdev->dev, false);

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct pmic_button_data *bdata = &ddata->button_data[i];

		if (bdata->irq_wqueue)
			destroy_workqueue(bdata->irq_wqueue);

		s2mps25_remove_key(bdata);
	}

	input_unregister_device(input);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int s2mps25_keys_suspend(struct device *dev)
{
	struct s2mps25_keys_drvdata *ddata = dev_get_drvdata(dev);
	int i;

	if (device_may_wakeup(dev)) {
		for (i = 0; i < ddata->pdata->nbuttons; i++) {
			struct pmic_button_data *bdata = &ddata->button_data[i];

			bdata->suspended = true;
		}
	}

	return 0;
}

static int s2mps25_keys_resume(struct device *dev)
{
	struct s2mps25_keys_drvdata *ddata = dev_get_drvdata(dev);
	int i;

	if (device_may_wakeup(dev)) {
		for (i = 0; i < ddata->pdata->nbuttons; i++) {
			struct pmic_button_data *bdata = &ddata->button_data[i];

			bdata->suspended = false;
		}
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(s2mps25_keys_pm_ops, s2mps25_keys_suspend, s2mps25_keys_resume);

static struct platform_driver s2mps25_keys_device_driver = {
	.probe		= s2mps25_keys_probe,
	.remove		= s2mps25_keys_remove,
	.driver		= {
		.name	= "s2mps25-power-keys",
		.owner	= THIS_MODULE,
		.pm	= &s2mps25_keys_pm_ops,
		.of_match_table = of_match_ptr(s2mps25_keys_of_match),
	}
};

module_platform_driver(s2mps25_keys_device_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Keyboard driver for s2mps25");
MODULE_ALIAS("platform:s2mps25 Power/Vol-down key");
