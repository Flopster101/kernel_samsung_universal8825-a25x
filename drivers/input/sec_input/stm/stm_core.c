/* drivers/input/sec_input/stm/stm_core.c
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "stm_dev.h"
#include "stm_reg.h"

struct stm_ts_data *g_ts;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#if IS_ENABLED(CONFIG_GH_RM_DRV)
static void stm_ts_trusted_touch_abort_handler(struct stm_ts_data *ts,
						int error);
static struct gh_acl_desc *stm_ts_vm_get_acl(enum gh_vm_names vm_name)
{
	struct gh_acl_desc *acl_desc;
	gh_vmid_t vmid;

	gh_rm_get_vmid(vm_name, &vmid);

	acl_desc = kzalloc(offsetof(struct gh_acl_desc, acl_entries[1]),
			GFP_KERNEL);
	if (!acl_desc)
		return ERR_PTR(ENOMEM);

	acl_desc->n_acl_entries = 1;
	acl_desc->acl_entries[0].vmid = vmid;
	acl_desc->acl_entries[0].perms = GH_RM_ACL_R | GH_RM_ACL_W;

	return acl_desc;
}

static struct gh_sgl_desc *stm_ts_vm_get_sgl(
				struct trusted_touch_vm_info *vm_info)
{
	struct gh_sgl_desc *sgl_desc;
	int i;

	sgl_desc = kzalloc(offsetof(struct gh_sgl_desc,
			sgl_entries[vm_info->iomem_list_size]), GFP_KERNEL);
	if (!sgl_desc)
		return ERR_PTR(ENOMEM);

	sgl_desc->n_sgl_entries = vm_info->iomem_list_size;

	for (i = 0; i < vm_info->iomem_list_size; i++) {
		sgl_desc->sgl_entries[i].ipa_base = vm_info->iomem_bases[i];
		sgl_desc->sgl_entries[i].size = vm_info->iomem_sizes[i];
	}

	return sgl_desc;
}

static int stm_ts_populate_vm_info(struct stm_ts_data *ts)
{
	int rc = 0;
	struct trusted_touch_vm_info *vm_info;
	struct device_node *np = ts->client->dev.of_node;
	int num_regs, num_sizes = 0;

	vm_info = kzalloc(sizeof(struct trusted_touch_vm_info), GFP_KERNEL);
	if (!vm_info) {
		rc = -ENOMEM;
		goto error;
	}

	ts->vm_info = vm_info;
	vm_info->vm_name = GH_TRUSTED_VM;
	rc = of_property_read_u32(np, "trusted-touch-spi-irq", &vm_info->hw_irq);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to read trusted touch SPI irq:%d\n", rc);
		goto vm_error;
	}
	num_regs = of_property_count_u32_elems(np, "trusted-touch-io-bases");
	if (num_regs < 0) {
		input_err(true, &ts->client->dev, "Invalid number of IO regions specified\n");
		rc = -EINVAL;
		goto vm_error;
	}

	num_sizes = of_property_count_u32_elems(np, "trusted-touch-io-sizes");
	if (num_sizes < 0) {
		input_err(true, &ts->client->dev, "Invalid number of IO regions specified\n");
		rc = -EINVAL;
		goto vm_error;
	}

	if (num_regs != num_sizes) {
		input_err(true, &ts->client->dev, "IO bases and sizes doe not match\n");
		rc = -EINVAL;
		goto vm_error;
	}

	vm_info->iomem_list_size = num_regs;

	vm_info->iomem_bases = kcalloc(num_regs, sizeof(*vm_info->iomem_bases), GFP_KERNEL);
	if (!vm_info->iomem_bases) {
		rc = -ENOMEM;
		goto vm_error;
	}

	rc = of_property_read_string(np, "trusted-touch-type",
						&vm_info->trusted_touch_type);
	if (rc) {
		pr_warn("%s: No trusted touch type selection made\n", __func__);
		vm_info->mem_tag = GH_MEM_NOTIFIER_TAG_TOUCH_PRIMARY;
		vm_info->irq_label = GH_IRQ_LABEL_TRUSTED_TOUCH_PRIMARY;
		rc = 0;
	} else if (!strcmp(vm_info->trusted_touch_type, "primary")) {
		vm_info->mem_tag = GH_MEM_NOTIFIER_TAG_TOUCH_PRIMARY;
		vm_info->irq_label = GH_IRQ_LABEL_TRUSTED_TOUCH_PRIMARY;
	} else if (!strcmp(vm_info->trusted_touch_type, "secondary")) {
		vm_info->mem_tag = GH_MEM_NOTIFIER_TAG_TOUCH_SECONDARY;
		vm_info->irq_label = GH_IRQ_LABEL_TRUSTED_TOUCH_SECONDARY;
	}

	rc = of_property_read_string(np, "trusted-touch-type",
						&vm_info->trusted_touch_type);
	if (rc)
		pr_warn("%s: No trusted touch type selection made\n", __func__);

	rc = of_property_read_u32_array(np, "trusted-touch-io-bases",
			vm_info->iomem_bases, vm_info->iomem_list_size);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to read trusted touch io bases:%d\n", rc);
		goto io_bases_error;
	}

	vm_info->iomem_sizes = kzalloc(
			sizeof(*vm_info->iomem_sizes) * num_sizes, GFP_KERNEL);
	if (!vm_info->iomem_sizes) {
		rc = -ENOMEM;
		goto io_bases_error;
	}

	rc = of_property_read_u32_array(np, "trusted-touch-io-sizes",
			vm_info->iomem_sizes, vm_info->iomem_list_size);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to read trusted touch io sizes:%d\n", rc);
		goto io_sizes_error;
	}
	return rc;

io_sizes_error:
	kfree(vm_info->iomem_sizes);
io_bases_error:
	kfree(vm_info->iomem_bases);
vm_error:
	kfree(vm_info);
error:
	return rc;
}

static void stm_ts_destroy_vm_info(struct stm_ts_data *ts)
{
	kfree(ts->vm_info->iomem_sizes);
	kfree(ts->vm_info->iomem_bases);
	kfree(ts->vm_info);
}

static void stm_ts_vm_deinit(struct stm_ts_data *ts)
{
	if (ts->vm_info->mem_cookie)
		gh_mem_notifier_unregister(ts->vm_info->mem_cookie);
	stm_ts_destroy_vm_info(ts);
}

#if IS_ENABLED(CONFIG_ARCH_QTI_VM)
static int stm_ts_vm_mem_release(struct stm_ts_data *ts);
static void stm_ts_trusted_touch_tvm_vm_mode_disable(struct stm_ts_data *ts);
static void stm_ts_trusted_touch_abort_tvm(struct stm_ts_data *ts);
static void stm_ts_trusted_touch_event_notify(struct stm_ts_data *ts, int event);

void stm_ts_trusted_touch_tvm_i2c_failure_report(struct stm_ts_data *ts)
{
	input_err(true, &ts->client->dev, "initiating trusted touch abort due to i2c failure\n");
	stm_ts_trusted_touch_abort_handler(ts,
			TRUSTED_TOUCH_EVENT_I2C_FAILURE);
}

static void stm_trusted_touch_intr_gpio_toggle(struct stm_ts_data *ts,
		bool enable)
{
	void __iomem *base;
	u32 val = 0;

	base = ioremap(TOUCH_INTR_GPIO_BASE, TOUCH_INTR_GPIO_SIZE);
	if (enable) {
		val |= BIT(0);
		writel_relaxed(val, base + TOUCH_INTR_GPIO_OFFSET);
		/* wait until toggle to finish*/
		wmb();
	} else {
		val &= ~BIT(0);
		writel_relaxed(val, base + TOUCH_INTR_GPIO_OFFSET);
		/* wait until toggle to finish*/
		wmb();
	}
	iounmap(base);
}

static int stm_ts_trusted_touch_get_tvm_driver_state(struct stm_ts_data *ts)
{
	return atomic_read(&ts->vm_info->vm_state);
}

static void stm_ts_trusted_touch_set_tvm_driver_state(struct stm_ts_data *ts,
						int state)
{
	atomic_set(&ts->vm_info->vm_state, state);
}

static int stm_ts_sgl_cmp(const void *a, const void *b)
{
	struct gh_sgl_entry *left = (struct gh_sgl_entry *)a;
	struct gh_sgl_entry *right = (struct gh_sgl_entry *)b;

	return (left->ipa_base - right->ipa_base);
}

static int stm_ts_vm_compare_sgl_desc(struct gh_sgl_desc *expected,
		struct gh_sgl_desc *received)
{
	int idx;

	if (expected->n_sgl_entries != received->n_sgl_entries)
		return -E2BIG;
	sort(received->sgl_entries, received->n_sgl_entries,
			sizeof(received->sgl_entries[0]), stm_ts_sgl_cmp, NULL);
	sort(expected->sgl_entries, expected->n_sgl_entries,
			sizeof(expected->sgl_entries[0]), stm_ts_sgl_cmp, NULL);

	for (idx = 0; idx < expected->n_sgl_entries; idx++) {
		struct gh_sgl_entry *left = &expected->sgl_entries[idx];
		struct gh_sgl_entry *right = &received->sgl_entries[idx];

		if ((left->ipa_base != right->ipa_base) ||
				(left->size != right->size)) {
			pr_err("%s: sgl mismatch: left_base:%lld right base:%lld left size:%lld right size:%lld\n",
					__func__, left->ipa_base, right->ipa_base,
					left->size, right->size);
			return -EINVAL;
		}
	}
	return 0;
}

static int stm_ts_vm_handle_vm_hardware(struct stm_ts_data *ts)
{
	int rc = 0;

	enable_irq(ts->irq);
	stm_ts_trusted_touch_set_tvm_driver_state(ts, TVM_INTERRUPT_ENABLED);
	return rc;
}

static void stm_ts_trusted_touch_tvm_vm_mode_enable(struct stm_ts_data *ts)
{

	struct gh_sgl_desc *sgl_desc, *expected_sgl_desc;
	struct gh_acl_desc *acl_desc;
	struct irq_data *irq_data;
	int rc = 0;
	int irq = 0;

	if (stm_ts_trusted_touch_get_tvm_driver_state(ts) !=
					TVM_ALL_RESOURCES_LENT_NOTIFIED) {
		input_err(true, &ts->client->dev, "All lend notifications not received\n");
		stm_ts_trusted_touch_event_notify(ts,
				TRUSTED_TOUCH_EVENT_NOTIFICATIONS_PENDING);
		return;
	}

	acl_desc = stm_ts_vm_get_acl(GH_TRUSTED_VM);
	if (IS_ERR(acl_desc)) {
		input_err(true, &ts->client->dev, "failed to populated acl data:rc=%ld\n",
				PTR_ERR(acl_desc));
		goto accept_fail;
	}

	sgl_desc = gh_rm_mem_accept(ts->vm_info->vm_mem_handle,
			GH_RM_MEM_TYPE_IO,
			GH_RM_TRANS_TYPE_LEND,
			GH_RM_MEM_ACCEPT_VALIDATE_ACL_ATTRS |
			GH_RM_MEM_ACCEPT_VALIDATE_LABEL |
			GH_RM_MEM_ACCEPT_DONE,  TRUSTED_TOUCH_MEM_LABEL,
			acl_desc, NULL, NULL, 0);
	if (IS_ERR_OR_NULL(sgl_desc)) {
		input_err(true, &ts->client->dev, "failed to do mem accept :rc=%ld\n",
				PTR_ERR(sgl_desc));
		goto acl_fail;
	}
	stm_ts_trusted_touch_set_tvm_driver_state(ts, TVM_IOMEM_ACCEPTED);

	/* Initiate session on tvm */
	rc = pm_runtime_get_sync(ts->client->controller->dev.parent);

	if (rc < 0) {
		input_err(true, &ts->client->dev, "failed to get sync rc:%d\n", rc);
		goto acl_fail;
	}
	stm_ts_trusted_touch_set_tvm_driver_state(ts, TVM_I2C_SESSION_ACQUIRED);

	expected_sgl_desc = stm_ts_vm_get_sgl(ts->vm_info);
	if (stm_ts_vm_compare_sgl_desc(expected_sgl_desc, sgl_desc)) {
		input_err(true, &ts->client->dev, "IO sg list does not match\n");
		goto sgl_cmp_fail;
	}

	kfree(expected_sgl_desc);
	kfree(acl_desc);

	irq = gh_irq_accept(ts->vm_info->irq_label, -1, IRQ_TYPE_EDGE_RISING);
	stm_trusted_touch_intr_gpio_toggle(ts, false);
	if (irq < 0) {
		input_err(true, &ts->client->dev, "failed to accept irq\n");
		goto accept_fail;
	}
	stm_ts_trusted_touch_set_tvm_driver_state(ts, TVM_IRQ_ACCEPTED);


	irq_data = irq_get_irq_data(irq);
	if (!irq_data) {
		input_err(true, &ts->client->dev, "Invalid irq data for trusted touch\n");
		goto accept_fail;
	}
	if (!irq_data->hwirq) {
		input_err(true, &ts->client->dev, "Invalid irq in irq data\n");
		goto accept_fail;
	}
	if (irq_data->hwirq != ts->vm_info->hw_irq) {
		input_err(true, &ts->client->dev, "Invalid irq lent\n");
		goto accept_fail;
	}

	input_err(true, &ts->client->dev, "irq:returned from accept:%d\n", irq);
	ts->irq = irq;

	rc = stm_ts_vm_handle_vm_hardware(ts);
	if (rc) {
		input_err(true, &ts->client->dev, " Delayed probe failure on VM!\n");
		goto accept_fail;
	}
	atomic_set(&ts->trusted_touch_enabled, 1);
	input_err(true, &ts->client->dev, " trusted touch enabled\n");

	return;
sgl_cmp_fail:
	kfree(expected_sgl_desc);
acl_fail:
	kfree(acl_desc);
accept_fail:
	stm_ts_trusted_touch_abort_handler(ts,
			TRUSTED_TOUCH_EVENT_ACCEPT_FAILURE);
}
static void stm_ts_vm_irq_on_lend_callback(void *data,
					unsigned long notif_type,
					enum gh_irq_label label)
{
	struct stm_ts_data *ts = data;

	input_err(true, &ts->client->dev, "received irq lend request for label:%d\n", label);
	if (stm_ts_trusted_touch_get_tvm_driver_state(ts) ==
		TVM_IOMEM_LENT_NOTIFIED) {
		stm_ts_trusted_touch_set_tvm_driver_state(ts,
		TVM_ALL_RESOURCES_LENT_NOTIFIED);
	} else {
		stm_ts_trusted_touch_set_tvm_driver_state(ts,
			TVM_IRQ_LENT_NOTIFIED);
	}
}

static void stm_ts_vm_mem_on_lend_handler(enum gh_mem_notifier_tag tag,
		unsigned long notif_type, void *entry_data, void *notif_msg)
{
	struct gh_rm_notif_mem_shared_payload *payload;
	struct trusted_touch_vm_info *vm_info;
	struct stm_ts_data *ts;

	ts = (struct stm_ts_data *)entry_data;
	vm_info = ts->vm_info;
	if (!vm_info) {
		input_err(true, &ts->client->dev, "Invalid vm_info\n");
		return;
	}

	if (notif_type != GH_RM_NOTIF_MEM_SHARED ||
			tag != vm_info->mem_tag) {
		input_err(true, &ts->client->dev, "Invalid command passed from rm\n");
		return;
	}

	if (!entry_data || !notif_msg) {
		input_err(true, &ts->client->dev, "Invalid entry data passed from rm\n");
		return;
	}


	payload = (struct gh_rm_notif_mem_shared_payload  *)notif_msg;
	if (payload->trans_type != GH_RM_TRANS_TYPE_LEND ||
			payload->label != TRUSTED_TOUCH_MEM_LABEL) {
		input_err(true, &ts->client->dev, "Invalid label or transaction type\n");
		return;
	}

	vm_info->vm_mem_handle = payload->mem_handle;
	input_err(true, &ts->client->dev, "received mem lend request with handle:%d\n",
		vm_info->vm_mem_handle);
	if (stm_ts_trusted_touch_get_tvm_driver_state(ts) ==
		TVM_IRQ_LENT_NOTIFIED) {
		stm_ts_trusted_touch_set_tvm_driver_state(ts,
			TVM_ALL_RESOURCES_LENT_NOTIFIED);
	} else {
		stm_ts_trusted_touch_set_tvm_driver_state(ts,
		TVM_IOMEM_LENT_NOTIFIED);
	}
}

static int stm_ts_vm_mem_release(struct stm_ts_data *ts)
{
	int rc = 0;

	if (!ts->vm_info->vm_mem_handle) {
		input_err(true, &ts->client->dev, "Invalid memory handle\n");
		return -EINVAL;
	}

	rc = gh_rm_mem_release(ts->vm_info->vm_mem_handle, 0);
	if (rc)
		input_err(true, &ts->client->dev, "VM mem release failed: rc=%d\n", rc);

	rc = gh_rm_mem_notify(ts->vm_info->vm_mem_handle,
				GH_RM_MEM_NOTIFY_OWNER_RELEASED,
				ts->vm_info->mem_tag, 0);
	if (rc)
		input_err(true, &ts->client->dev, "Failed to notify mem release to PVM: rc=%d\n", rc);
	input_err(true, &ts->client->dev, "vm mem release succeded\n");

	ts->vm_info->vm_mem_handle = 0;
	return rc;
}

static void stm_ts_trusted_touch_tvm_vm_mode_disable(struct stm_ts_data *ts)
{
	int rc = 0;

	atomic_set(&ts->trusted_touch_transition, 1);

	if (atomic_read(&ts->trusted_touch_abort_status)) {
		stm_ts_trusted_touch_abort_tvm(ts);
		return;
	}

	disable_irq(ts->irq);

	stm_ts_trusted_touch_set_tvm_driver_state(ts,
				TVM_INTERRUPT_DISABLED);

	rc = gh_irq_release(ts->vm_info->irq_label);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to release irq rc:%d\n", rc);
		goto error;
	} else {
		stm_ts_trusted_touch_set_tvm_driver_state(ts,
					TVM_IRQ_RELEASED);
	}
	rc = gh_irq_release_notify(ts->vm_info->irq_label);
	if (rc)
		input_err(true, &ts->client->dev, "Failed to notify release irq rc:%d\n", rc);

	input_err(true, &ts->client->dev, "vm irq release succeded\n");

	stm_ts_release_all_finger(ts);

	pm_runtime_put_sync(ts->client->controller->dev.parent);

	stm_ts_trusted_touch_set_tvm_driver_state(ts,
					TVM_I2C_SESSION_RELEASED);
	rc = stm_ts_vm_mem_release(ts);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to release mem rc:%d\n", rc);
		goto error;
	} else {
		stm_ts_trusted_touch_set_tvm_driver_state(ts,
					TVM_IOMEM_RELEASED);
	}
	stm_ts_trusted_touch_set_tvm_driver_state(ts, TRUSTED_TOUCH_TVM_INIT);
	atomic_set(&ts->trusted_touch_transition, 0);
	atomic_set(&ts->trusted_touch_enabled, 0);
	input_err(true, &ts->client->dev, " trusted touch disabled\n");
	return;
error:
	stm_ts_trusted_touch_abort_handler(ts,
			TRUSTED_TOUCH_EVENT_RELEASE_FAILURE);
}

int stm_ts_handle_trusted_touch_tvm(struct stm_ts_data *ts, int value)
{
	int err = 0;

	switch (value) {
	case 0:
		if ((atomic_read(&ts->trusted_touch_enabled) == 0) &&
			(atomic_read(&ts->trusted_touch_abort_status) == 0)) {
			input_err(true, &ts->client->dev, "Trusted touch is already disabled\n");
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			stm_ts_trusted_touch_tvm_vm_mode_disable(ts);
		} else {
			input_err(true, &ts->client->dev, "Unsupported trusted touch mode\n");
		}
		break;

	case 1:
		if (atomic_read(&ts->trusted_touch_enabled)) {
			input_err(true, &ts->client->dev, "Trusted touch usecase underway\n");
			err = -EBUSY;
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			stm_ts_trusted_touch_tvm_vm_mode_enable(ts);
		} else {
			input_err(true, &ts->client->dev, "Unsupported trusted touch mode\n");
		}
		break;

	default:
		input_err(true, &ts->client->dev, "unsupported value: %d\n", value);
		err = -EINVAL;
		break;
	}

	return err;
}

static void stm_ts_trusted_touch_abort_tvm(struct stm_ts_data *ts)
{
	int rc = 0;
	int vm_state = stm_ts_trusted_touch_get_tvm_driver_state(ts);

	if (vm_state >= TRUSTED_TOUCH_TVM_STATE_MAX) {
		input_err(true, &ts->client->dev, "invalid tvm driver state: %d\n", vm_state);
		return;
	}

	switch (vm_state) {
	case TVM_INTERRUPT_ENABLED:
		disable_irq(ts->irq);
	case TVM_IRQ_ACCEPTED:
	case TVM_INTERRUPT_DISABLED:
		rc = gh_irq_release(ts->vm_info->irq_label);
		if (rc)
			input_err(true, &ts->client->dev, "Failed to release irq rc:%d\n", rc);
		rc = gh_irq_release_notify(ts->vm_info->irq_label);
		if (rc)
			input_err(true, &ts->client->dev, "Failed to notify irq release rc:%d\n", rc);
	case TVM_I2C_SESSION_ACQUIRED:
	case TVM_IOMEM_ACCEPTED:
	case TVM_IRQ_RELEASED:
		stm_ts_release_all_finger(ts);
		pm_runtime_put_sync(ts->client->controller->dev.parent);

	case TVM_I2C_SESSION_RELEASED:
		rc = stm_ts_vm_mem_release(ts);
		if (rc)
			input_err(true, &ts->client->dev, "Failed to release mem rc:%d\n", rc);
	case TVM_IOMEM_RELEASED:
	case TVM_ALL_RESOURCES_LENT_NOTIFIED:
	case TRUSTED_TOUCH_TVM_INIT:
	case TVM_IRQ_LENT_NOTIFIED:
	case TVM_IOMEM_LENT_NOTIFIED:
		atomic_set(&ts->trusted_touch_enabled, 0);
	}

	atomic_set(&ts->trusted_touch_abort_status, 0);
	stm_ts_trusted_touch_set_tvm_driver_state(ts, TRUSTED_TOUCH_TVM_INIT);
}

#else

static void stm_ts_bus_put(struct stm_ts_data *ts);

static int stm_ts_trusted_touch_get_pvm_driver_state(struct stm_ts_data *ts)
{
	return atomic_read(&ts->vm_info->vm_state);
}

static void stm_ts_trusted_touch_set_pvm_driver_state(struct stm_ts_data *ts,
							int state)
{
	atomic_set(&ts->vm_info->vm_state, state);
}

static void stm_ts_trusted_touch_abort_pvm(struct stm_ts_data *ts)
{
	int rc = 0;
	int vm_state = stm_ts_trusted_touch_get_pvm_driver_state(ts);

	if (vm_state >= TRUSTED_TOUCH_PVM_STATE_MAX) {
		input_err(true, &ts->client->dev, "Invalid driver state: %d\n", vm_state);
		return;
	}

	switch (vm_state) {
	case PVM_IRQ_RELEASE_NOTIFIED:
	case PVM_ALL_RESOURCES_RELEASE_NOTIFIED:
	case PVM_IRQ_LENT:
	case PVM_IRQ_LENT_NOTIFIED:
		rc = gh_irq_reclaim(ts->vm_info->irq_label);
		if (rc)
			input_err(true, &ts->client->dev, "failed to reclaim irq on pvm rc:%d\n", rc);
	case PVM_IRQ_RECLAIMED:
	case PVM_IOMEM_LENT:
	case PVM_IOMEM_LENT_NOTIFIED:
	case PVM_IOMEM_RELEASE_NOTIFIED:
		rc = gh_rm_mem_reclaim(ts->vm_info->vm_mem_handle, 0);
		if (rc)
			input_err(true, &ts->client->dev, "failed to reclaim iomem on pvm rc:%d\n", rc);
		ts->vm_info->vm_mem_handle = 0;
	case PVM_IOMEM_RECLAIMED:
	case PVM_INTERRUPT_DISABLED:
		enable_irq(ts->irq);
	case PVM_I2C_RESOURCE_ACQUIRED:
	case PVM_INTERRUPT_ENABLED:
		stm_ts_bus_put(ts);
	case TRUSTED_TOUCH_PVM_INIT:
	case PVM_I2C_RESOURCE_RELEASED:
		atomic_set(&ts->trusted_touch_enabled, 0);
		atomic_set(&ts->trusted_touch_transition, 0);
	}

	atomic_set(&ts->trusted_touch_abort_status, 0);

	stm_ts_trusted_touch_set_pvm_driver_state(ts, TRUSTED_TOUCH_PVM_INIT);
}

static int stm_ts_clk_prepare_enable(struct stm_ts_data *ts)
{
	int ret;

	ret = clk_prepare_enable(ts->iface_clk);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: error on clk_prepare_enable(iface_clk): %d\n", __func__, ret);
		return ret;
	}

	ret = clk_prepare_enable(ts->core_clk);
	if (ret) {
		clk_disable_unprepare(ts->iface_clk);
		input_err(true, &ts->client->dev, "%s: error clk_prepare_enable(core_clk): %d\n", __func__, ret);
	}
	return ret;
}

static void stm_ts_clk_disable_unprepare(struct stm_ts_data *ts)
{
	clk_disable_unprepare(ts->core_clk);
	clk_disable_unprepare(ts->iface_clk);
}

static int stm_ts_bus_get(struct stm_ts_data *ts)
{
	int rc = 0;
	struct device *dev = NULL;

	reinit_completion(&ts->trusted_touch_powerdown);

	dev = ts->client->controller->dev.parent;

	mutex_lock(&ts->clk_io_ctrl_mutex);
	rc = pm_runtime_get_sync(dev);
	if (rc >= 0 &&  ts->core_clk != NULL &&
				ts->iface_clk != NULL) {
		rc = stm_ts_clk_prepare_enable(ts);
		if (rc)
			pm_runtime_put_sync(dev);
	}

	mutex_unlock(&ts->clk_io_ctrl_mutex);
	return rc;
}

static void stm_ts_bus_put(struct stm_ts_data *ts)
{
	struct device *dev = NULL;

	dev = ts->client->controller->dev.parent;

	mutex_lock(&ts->clk_io_ctrl_mutex);
	if (ts->core_clk != NULL && ts->iface_clk != NULL)
		stm_ts_clk_disable_unprepare(ts);
	pm_runtime_put_sync(dev);
	mutex_unlock(&ts->clk_io_ctrl_mutex);
	complete(&ts->trusted_touch_powerdown);
}

static struct gh_notify_vmid_desc *stm_ts_vm_get_vmid(gh_vmid_t vmid)
{
	struct gh_notify_vmid_desc *vmid_desc;

	vmid_desc = kzalloc(offsetof(struct gh_notify_vmid_desc,
				vmid_entries[1]), GFP_KERNEL);
	if (!vmid_desc)
		return ERR_PTR(ENOMEM);

	vmid_desc->n_vmid_entries = 1;
	vmid_desc->vmid_entries[0].vmid = vmid;
	return vmid_desc;
}

static void stm_trusted_touch_pvm_vm_mode_disable(struct stm_ts_data *ts)
{
	int rc = 0;

	atomic_set(&ts->trusted_touch_transition, 1);

	if (atomic_read(&ts->trusted_touch_abort_status)) {
		stm_ts_trusted_touch_abort_pvm(ts);
		return;
	}

	if (stm_ts_trusted_touch_get_pvm_driver_state(ts) !=
					PVM_ALL_RESOURCES_RELEASE_NOTIFIED)
		input_err(true, &ts->client->dev, "all release notifications are not received yet\n");

	rc = gh_rm_mem_reclaim(ts->vm_info->vm_mem_handle, 0);
	if (rc) {
		input_err(true, &ts->client->dev, "Trusted touch VM mem reclaim failed rc:%d\n", rc);
		goto error;
	}
	stm_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IOMEM_RECLAIMED);
	ts->vm_info->vm_mem_handle = 0;
	input_err(true, &ts->client->dev, "vm mem reclaim succeded!\n");

	rc = gh_irq_reclaim(ts->vm_info->irq_label);
	if (rc) {
		input_err(true, &ts->client->dev, "failed to reclaim irq on pvm rc:%d\n", rc);
		goto error;
	}
	stm_ts_trusted_touch_set_pvm_driver_state(ts,
				PVM_IRQ_RECLAIMED);
	input_err(true, &ts->client->dev, "vm irq reclaim succeded!\n");

	stm_ts_trusted_touch_set_pvm_driver_state(ts, PVM_INTERRUPT_ENABLED);
	stm_ts_bus_put(ts);
	atomic_set(&ts->trusted_touch_transition, 0);
	stm_ts_trusted_touch_set_pvm_driver_state(ts,
						PVM_I2C_RESOURCE_RELEASED);
	stm_ts_trusted_touch_set_pvm_driver_state(ts,
						TRUSTED_TOUCH_PVM_INIT);
	atomic_set(&ts->trusted_touch_enabled, 0);
	input_err(true, &ts->client->dev, " trusted touch disabled\n");

	msleep(200);
	enable_irq(ts->irq);

	return;
error:
	stm_ts_trusted_touch_abort_handler(ts,
			TRUSTED_TOUCH_EVENT_RECLAIM_FAILURE);
}

static void stm_ts_vm_irq_on_release_callback(void *data,
					unsigned long notif_type,
					enum gh_irq_label label)
{
	struct stm_ts_data *ts = data;

	if (notif_type != GH_RM_NOTIF_VM_IRQ_RELEASED) {
		input_err(true, &ts->client->dev, "invalid notification type\n");
		return;
	}

	if (stm_ts_trusted_touch_get_pvm_driver_state(ts) == PVM_IOMEM_RELEASE_NOTIFIED) {
		stm_ts_trusted_touch_set_pvm_driver_state(ts, PVM_ALL_RESOURCES_RELEASE_NOTIFIED);
	} else {
		stm_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IRQ_RELEASE_NOTIFIED);
	}
}

static void stm_ts_vm_mem_on_release_handler(enum gh_mem_notifier_tag tag,
		unsigned long notif_type, void *entry_data, void *notif_msg)
{
	struct gh_rm_notif_mem_released_payload *release_payload;
	struct trusted_touch_vm_info *vm_info;
	struct stm_ts_data *ts;

	if (!entry_data) {
		pr_err("%s: Invalid entry_data\n", __func__);
		return;
	}

	ts = (struct stm_ts_data *)entry_data;
	vm_info = ts->vm_info;
	if (!vm_info) {
		input_err(true, &ts->client->dev, "Invalid vm_info\n");
		return;
	}

	if (notif_type != GH_RM_NOTIF_MEM_RELEASED) {
		input_err(true, &ts->client->dev, "Invalid notification type\n");
		return;
	}

	if (tag != vm_info->mem_tag) {
		input_err(true, &ts->client->dev, "Invalid tag\n");
		return;
	}

	if (!entry_data || !notif_msg) {
		input_err(true, &ts->client->dev, "Invalid data or notification message\n");
		return;
	}

	release_payload = (struct gh_rm_notif_mem_released_payload  *)notif_msg;
	if (release_payload->mem_handle != vm_info->vm_mem_handle) {
		input_err(true, &ts->client->dev, "Invalid mem handle detected\n");
		return;
	}

	input_info(true, &ts->client->dev, "received mem lend request with handle:\n");
	if (stm_ts_trusted_touch_get_pvm_driver_state(ts) ==
				PVM_IRQ_RELEASE_NOTIFIED) {
		stm_ts_trusted_touch_set_pvm_driver_state(ts,
			PVM_ALL_RESOURCES_RELEASE_NOTIFIED);
	} else {
		stm_ts_trusted_touch_set_pvm_driver_state(ts,
			PVM_IOMEM_RELEASE_NOTIFIED);
	}
}

static int stm_ts_vm_mem_lend(struct stm_ts_data *ts)
{
	struct gh_acl_desc *acl_desc;
	struct gh_sgl_desc *sgl_desc;
	struct gh_notify_vmid_desc *vmid_desc;
	gh_memparcel_handle_t mem_handle;
	gh_vmid_t trusted_vmid;
	int rc = 0;

	acl_desc = stm_ts_vm_get_acl(GH_TRUSTED_VM);
	if (IS_ERR(acl_desc)) {
		input_err(true, &ts->client->dev, "Failed to get acl of IO memories for Trusted touch. rc=%ld\n",
			PTR_ERR(acl_desc));
		return -EINVAL;
	}

	sgl_desc = stm_ts_vm_get_sgl(ts->vm_info);
	if (IS_ERR(sgl_desc)) {
		input_err(true, &ts->client->dev, "Failed to get sgl of IO memories for Trusted touch. rc=%ld\n",
			PTR_ERR(sgl_desc));
		rc = -EINVAL;
		goto sgl_error;
	}

	rc = gh_rm_mem_lend(GH_RM_MEM_TYPE_IO, 0, TRUSTED_TOUCH_MEM_LABEL,
			acl_desc, sgl_desc, NULL, &mem_handle);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to lend IO memories for Trusted touch rc:%d\n",
							rc);
		goto error;
	}

	input_err(true, &ts->client->dev, " vm mem lend succeded\n");

	stm_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IOMEM_LENT);

	gh_rm_get_vmid(GH_TRUSTED_VM, &trusted_vmid);

	vmid_desc = stm_ts_vm_get_vmid(trusted_vmid);

	rc = gh_rm_mem_notify(mem_handle, GH_RM_MEM_NOTIFY_RECIPIENT_SHARED,
			ts->vm_info->mem_tag, vmid_desc);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to notify mem lend to hypervisor rc:%d\n", rc);
		goto vmid_error;
	}

	stm_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IOMEM_LENT_NOTIFIED);

	ts->vm_info->vm_mem_handle = mem_handle;
vmid_error:
	kfree(vmid_desc);
error:
	kfree(sgl_desc);
sgl_error:
	kfree(acl_desc);

	return rc;
}

static int stm_ts_trusted_touch_pvm_vm_mode_enable(struct stm_ts_data *ts)
{
	int rc = 0;
	struct trusted_touch_vm_info *vm_info = ts->vm_info;

	atomic_set(&ts->trusted_touch_transition, 1);
	mutex_lock(&ts->transition_lock);

	if (ts->plat_data->enabled == false) {
		input_err(true, &ts->client->dev, "Invalid power state for operation\n");
		atomic_set(&ts->trusted_touch_transition, 0);
		rc =  -EPERM;
		goto error;
	}

	/* i2c session start and resource acquire */
	if (stm_ts_bus_get(ts) < 0) {
		input_err(true, &ts->client->dev, "stm_ts_bus_get failed\n");
		rc = -EIO;
		goto error;
	}

	stm_ts_trusted_touch_set_pvm_driver_state(ts, PVM_I2C_RESOURCE_ACQUIRED);
	/* flush pending interurpts from FIFO */
	disable_irq(ts->irq);
	atomic_set(&ts->trusted_touch_enabled, 1);
	stm_ts_trusted_touch_set_pvm_driver_state(ts, PVM_INTERRUPT_DISABLED);
	stm_ts_release_all_finger(ts);

	rc = stm_ts_vm_mem_lend(ts);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to lend memory\n");
		goto abort_handler;
	}
	input_err(true, &ts->client->dev, "vm mem lend succeded\n");
	rc = gh_irq_lend_v2(vm_info->irq_label, vm_info->vm_name,
		ts->irq, &stm_ts_vm_irq_on_release_callback, ts);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to lend irq\n");
		goto abort_handler;
	}

	input_err(true, &ts->client->dev, "vm irq lend succeded for irq:%d\n", ts->irq);
	stm_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IRQ_LENT);

	rc = gh_irq_lend_notify(vm_info->irq_label);
	if (rc) {
		input_err(true, &ts->client->dev, "Failed to notify irq\n");
		goto abort_handler;
	}
	stm_ts_trusted_touch_set_pvm_driver_state(ts, PVM_IRQ_LENT_NOTIFIED);

	mutex_unlock(&ts->transition_lock);
	atomic_set(&ts->trusted_touch_transition, 0);
	input_err(true, &ts->client->dev, " trusted touch enabled\n");
	return rc;

abort_handler:
	atomic_set(&ts->trusted_touch_enabled, 0);
	stm_ts_trusted_touch_abort_handler(ts, TRUSTED_TOUCH_EVENT_LEND_FAILURE);

error:
	mutex_unlock(&ts->transition_lock);
	return rc;
}

int stm_ts_handle_trusted_touch_pvm(struct stm_ts_data *ts, int value)
{
	int err = 0;

	switch (value) {
	case 0:
		if (atomic_read(&ts->trusted_touch_enabled) == 0 &&
			(atomic_read(&ts->trusted_touch_abort_status) == 0)) {
			input_err(true, &ts->client->dev, "Trusted touch is already disabled\n");
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			stm_trusted_touch_pvm_vm_mode_disable(ts);
		} else {
			input_err(true, &ts->client->dev, "Unsupported trusted touch mode\n");
		}
		break;

	case 1:
		if (atomic_read(&ts->trusted_touch_enabled)) {
			input_err(true, &ts->client->dev, "Trusted touch usecase underway\n");
			err = -EBUSY;
			break;
		}
		if (atomic_read(&ts->trusted_touch_mode) ==
				TRUSTED_TOUCH_VM_MODE) {
			err = stm_ts_trusted_touch_pvm_vm_mode_enable(ts);
		} else {
			input_err(true, &ts->client->dev, "Unsupported trusted touch mode\n");
		}
		break;

	default:
		input_err(true, &ts->client->dev, "unsupported value: %d\n", value);
		err = -EINVAL;
		break;
	}
	return err;
}

#endif

static void stm_ts_trusted_touch_event_notify(struct stm_ts_data *ts, int event)
{
	atomic_set(&ts->trusted_touch_event, event);
	sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "trusted_touch_event");
}

static void stm_ts_trusted_touch_abort_handler(struct stm_ts_data *ts, int error)
{
	atomic_set(&ts->trusted_touch_abort_status, error);
	input_err(true, &ts->client->dev, "TUI session aborted with failure:%d\n", error);
	stm_ts_trusted_touch_event_notify(ts, error);
#if IS_ENABLED(CONFIG_ARCH_QTI_VM)
	input_err(true, &ts->client->dev, "Resetting touch controller\n");
	if (stm_ts_trusted_touch_get_tvm_driver_state(ts) >= TVM_IOMEM_ACCEPTED
			&& error == TRUSTED_TOUCH_EVENT_I2C_FAILURE) {
		input_err(true, &ts->client->dev, "Resetting touch controller ??????????????\n");

	}
#endif
}

static int stm_ts_vm_init(struct stm_ts_data *ts)
{
	int rc = 0;
	struct trusted_touch_vm_info *vm_info;
	void *mem_cookie;

	rc = stm_ts_populate_vm_info(ts);
	if (rc) {
		input_err(true, &ts->client->dev, "Cannot setup vm pipeline\n");
		rc = -EINVAL;
		goto fail;
	}

	vm_info = ts->vm_info;
#if IS_ENABLED(CONFIG_ARCH_QTI_VM)
	mem_cookie = gh_mem_notifier_register(vm_info->mem_tag,
			stm_ts_vm_mem_on_lend_handler, ts);
	if (!mem_cookie) {
		input_err(true, &ts->client->dev, "Failed to register on lend mem notifier\n");
		rc = -EINVAL;
		goto init_fail;
	}
	vm_info->mem_cookie = mem_cookie;
	rc = gh_irq_wait_for_lend_v2(vm_info->irq_label, GH_PRIMARY_VM,
			&stm_ts_vm_irq_on_lend_callback, ts);
	stm_ts_trusted_touch_set_tvm_driver_state(ts, TRUSTED_TOUCH_TVM_INIT);
#else
	mem_cookie = gh_mem_notifier_register(vm_info->mem_tag,
			stm_ts_vm_mem_on_release_handler, ts);
	if (!mem_cookie) {
		input_err(true, &ts->client->dev, "Failed to register on release mem notifier\n");
		rc = -EINVAL;
		goto init_fail;
	}
	vm_info->mem_cookie = mem_cookie;
	stm_ts_trusted_touch_set_pvm_driver_state(ts, TRUSTED_TOUCH_PVM_INIT);
#endif
	return rc;
init_fail:
	stm_ts_vm_deinit(ts);
fail:
	return rc;
}

static void stm_ts_dt_parse_trusted_touch_info(struct stm_ts_data *ts)
{
	struct device_node *np = ts->client->dev.of_node;
	int rc = 0;
	const char *selection;
	const char *environment;

	rc = of_property_read_string(np, "trusted-touch-mode",
								&selection);
	if (rc) {
		input_err(true, &ts->client->dev, "%s: No trusted touch mode selection made\n", __func__);
		atomic_set(&ts->trusted_touch_mode,
						TRUSTED_TOUCH_MODE_NONE);
		return;
	}

	if (!strcmp(selection, "vm_mode")) {
		atomic_set(&ts->trusted_touch_mode,
						TRUSTED_TOUCH_VM_MODE);
		input_err(true, &ts->client->dev, "Selected trusted touch mode to VM mode\n");
	} else {
		atomic_set(&ts->trusted_touch_mode,
						TRUSTED_TOUCH_MODE_NONE);
		input_err(true, &ts->client->dev, "Invalid trusted_touch mode\n");
	}

	rc = of_property_read_string(np, "touch-environment",
						&environment);
	if (rc) {
		input_err(true, &ts->client->dev, "%s: No trusted touch mode environment\n", __func__);
	}
	ts->touch_environment = environment;
	input_err(true, &ts->client->dev, "Trusted touch environment:%s\n",
			ts->touch_environment);
}

static void stm_ts_trusted_touch_init(struct stm_ts_data *ts)
{
	int rc = 0;

	atomic_set(&ts->trusted_touch_initialized, 0);
	stm_ts_dt_parse_trusted_touch_info(ts);

	if (atomic_read(&ts->trusted_touch_mode) ==
						TRUSTED_TOUCH_MODE_NONE)
		return;

	init_completion(&ts->trusted_touch_powerdown);

	/* Get clocks */
	ts->core_clk = devm_clk_get(ts->client->controller->dev.parent,
						"m-ahb");
	if (IS_ERR(ts->core_clk)) {
		ts->core_clk = NULL;
		input_err(true, &ts->client->dev, "%s: core_clk is not defined\n", __func__);
	}

	ts->iface_clk = devm_clk_get(ts->client->controller->dev.parent,
						"se-clk");
	if (IS_ERR(ts->iface_clk)) {
		ts->iface_clk = NULL;
		input_err(true, &ts->client->dev, "%s: iface_clk is not defined\n", __func__);
	}

	if (atomic_read(&ts->trusted_touch_mode) ==
						TRUSTED_TOUCH_VM_MODE) {
		rc = stm_ts_vm_init(ts);
		if (rc)
			input_err(true, &ts->client->dev, "Failed to init VM\n");
	}
	atomic_set(&ts->trusted_touch_initialized, 1);
}

static ssize_t trusted_touch_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	input_err(true, &ts->client->dev, "%s\n", __func__);
	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->trusted_touch_enabled));
}

static ssize_t trusted_touch_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	unsigned long value;
	int ret;

	input_err(true, &ts->client->dev, "%s\n", __func__);
	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0)
		return ret;

	if (!atomic_read(&ts->trusted_touch_initialized))
		return -EIO;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	if (value == 1)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif

	input_info(true, &ts->client->dev, "%s: value: %ld\n", __func__, value);
#if IS_ENABLED(CONFIG_ARCH_QTI_VM)
	ret = stm_ts_handle_trusted_touch_tvm(ts, value);
#else
	ret = stm_ts_handle_trusted_touch_pvm(ts, value);
#endif
	if (ret) {
		input_err(true, &ts->client->dev, "Failed to handle touch vm\n");
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
		return -EINVAL;
	}

	if (value == 1)
		reinit_completion(&ts->secure_powerdown);
	else
		complete_all(&ts->secure_powerdown);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	if (value == 0)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif

	return count;
}

static ssize_t trusted_touch_event_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	input_err(true, &ts->client->dev, "%s\n", __func__);
	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->trusted_touch_event));
}

static ssize_t trusted_touch_event_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	unsigned long value;
	int ret;

	input_err(true, &ts->client->dev, "%s\n", __func__);
	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0)
		return ret;

	if (!atomic_read(&ts->trusted_touch_initialized))
		return -EIO;

	input_info(true, &ts->client->dev, "%s: value: %ld\n", __func__, value);

	atomic_set(&ts->trusted_touch_event, value);

	return count;
}

static ssize_t trusted_touch_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	input_err(true, &ts->client->dev, "%s\n", __func__);
	return scnprintf(buf, PAGE_SIZE, "%s", ts->vm_info->trusted_touch_type);
}
#endif
static irqreturn_t secure_filter_interrupt(struct stm_ts_data *ts)
{
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		if (atomic_cmpxchg(&ts->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		} else {
			input_info(true, &ts->client->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&ts->secure_pending_irqs));
		}

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_runtime : set rpm_resume or rpm_ilde
 */
static ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->secure_enabled));
}

static ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *addr, const char *buf, size_t count)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	unsigned long data;

	if (count > 2) {
		input_err(true, &ts->client->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	ret = kstrtoul(buf, 10, &data);
	if (ret != 0) {
		input_err(true, &ts->client->dev, "%s: failed to read:%d\n",
				__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		if (ts->reset_is_on_going) {
			input_err(true, &ts->client->dev, "%s: reset is on going because i2c fail\n", __func__);
			return -EBUSY;
		}

		/* Enable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
			input_err(true, &ts->client->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		sec_delay(200);
		
		/* syncronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		disable_irq(ts->client->irq);

		/* Release All Finger */
		stm_ts_release_all_finger(ts);

		if (stm_pm_runtime_get_sync(ts) < 0) {
			enable_irq(ts->client->irq);
			input_err(true, &ts->client->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif
		reinit_completion(&ts->secure_powerdown);
		reinit_completion(&ts->secure_interrupt);

		atomic_set(&ts->secure_enabled, 1);
		atomic_set(&ts->secure_pending_irqs, 0);

		enable_irq(ts->client->irq);

		input_info(true, &ts->client->dev, "%s: secure touch enable\n", __func__);
	} else if (data == 0) {
		/* Disable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
			input_err(true, &ts->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_delay(200);

		stm_pm_runtime_put_sync(ts);
		atomic_set(&ts->secure_enabled, 0);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		sec_delay(10);

		stm_ts_irq_thread(ts->client->irq, ts);
		complete(&ts->secure_interrupt);
		complete(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: secure touch disable\n", __func__);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
	} else {
		input_err(true, &ts->client->dev, "%s: unsupport value:%ld\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

static ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
		input_err(true, &ts->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, -1, 0) == -1) {
		input_err(true, &ts->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, 1, 0) == 1) {
		val = 1;
		input_err(true, &ts->client->dev, "%s: pending irq is %d\n",
				__func__, atomic_read(&ts->secure_pending_irqs));
	}

	complete(&ts->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

static ssize_t secure_ownership_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1");
}

static int secure_touch_init(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	init_completion(&ts->secure_interrupt);
	init_completion(&ts->secure_powerdown);

	return 0;
}

static void secure_touch_stop(struct stm_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->secure_enabled)) {
		atomic_set(&ts->secure_pending_irqs, -1);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		if (stop)
			wait_for_completion_interruptible(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: %d\n", __func__, stop);
	}
}

#if IS_ENABLED(CONFIG_GH_RM_DRV)
static DEVICE_ATTR_RW(trusted_touch_enable);
static DEVICE_ATTR_RW(trusted_touch_event);
static DEVICE_ATTR_RO(trusted_touch_type);
#endif
static DEVICE_ATTR_RW(secure_touch_enable);
static DEVICE_ATTR_RO(secure_touch);
static DEVICE_ATTR_RO(secure_ownership);
static struct attribute *secure_attr[] = {
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	&dev_attr_trusted_touch_enable.attr,
	&dev_attr_trusted_touch_event.attr,
	&dev_attr_trusted_touch_type.attr,
#endif
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	&dev_attr_secure_ownership.attr,
	NULL,
};

static struct attribute_group secure_attr_group = {
	.attrs = secure_attr,
};
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
static int stm_touch_notify_call(struct notifier_block *n, unsigned long data, void *v)
{
	struct stm_ts_data *ts = container_of(n, struct stm_ts_data, stm_input_nb);
	u8 reg[3];
	int ret = 0;

	if (!ts->info_work_done) {
		input_info(true, &ts->client->dev, "%s: info work is not done. skip\n", __func__);
		return ret;
	}

	if (ts->plat_data->shutdown_called)
		return -ENODEV;

	reg[0] = STM_TS_CMD_SET_FUNCTION_ONOFF;

	switch (data) {
	case NOTIFIER_TSP_BLOCKING_REQUEST:
		if (ts->plat_data->wirelesscharger_mode != TYPE_WIRELESS_CHARGER_NONE) {
			input_info(true, &ts->client->dev, "%s: Skip tsp block on wireless charger\n", __func__);
		} else {
			reg[1] = STM_TS_CMD_FUNCTION_SET_TSP_BLOCK;
			reg[2] = 1;
			ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
			input_info(true, &ts->client->dev, "%s: tsp block, ret=%d\n", __func__, ret);
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_TSP_SCAN_BLOCK, 0, 0);
		}
		break;
	case NOTIFIER_TSP_BLOCKING_RELEASE:
		if (ts->plat_data->wirelesscharger_mode != TYPE_WIRELESS_CHARGER_NONE) {
			input_info(true, &ts->client->dev, "%s: Skip tsp unblock on wireless charger\n", __func__);
		} else {
			reg[1] = STM_TS_CMD_FUNCTION_SET_TSP_BLOCK;
			reg[2] = 0;
			ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
			input_info(true, &ts->client->dev, "%s: tsp unblock, ret=%d\n", __func__, ret);
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_TSP_SCAN_UNBLOCK, 0, 0);
		}
		break;
	case NOTIFIER_WACOM_PEN_INSERT:
		reg[1] = STM_TS_CMD_FUNCTION_SET_PEN_DETECTION;
		reg[2] = 0;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen in detect, ret=%d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_PEN_REMOVE:
		reg[1] = STM_TS_CMD_FUNCTION_SET_PEN_DETECTION;
		reg[2] = 1;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen out detect, ret=%d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_SAVING_MODE_ON:
		reg[1] = STM_TS_CMD_FUNCTION_SET_PEN_SAVING_MODE;
		reg[2] = 1;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen saving mode on, ret=%d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_SAVING_MODE_OFF:
		reg[1] = STM_TS_CMD_FUNCTION_SET_PEN_SAVING_MODE;
		reg[2] = 0;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen saving mode off, ret=%d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_IN:
		reg[1] = STM_TS_CMD_FUNCTION_SET_HOVER_DETECTION;
		reg[2] = 1;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen hover in detect, ret=%d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_OUT:
		reg[1] = STM_TS_CMD_FUNCTION_SET_HOVER_DETECTION;
		reg[2] = 0;
		ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
		input_info(true, &ts->client->dev, "%s: pen hover out detect, ret=%d\n", __func__, ret);
		break;
	default:
		break;
	}
	return ret;
}
#endif

void stm_ts_reinit(void *data)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)data;
	int ret = 0;
	int retry = 3;

	do {
		ret = stm_ts_systemreset(ts, 0);
		if (ret < 0)
			stm_ts_reset(ts, 20);
		else
			break;
	} while (--retry);

	if (retry == 0) {
		input_err(true, &ts->client->dev, "%s: Failed to system reset\n", __func__);
		goto out;
	}

	input_info(true, &ts->client->dev,
		"%s: charger=0x%x, touch_functions=0x%x, Power mode=0x%x\n",
		__func__, ts->plat_data->wirelesscharger_mode, ts->plat_data->touch_functions, ts->plat_data->power_state);

	ts->plat_data->touch_noise_status = 0;
	ts->plat_data->touch_pre_noise_status = 0;
	ts->plat_data->wet_mode = 0;

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, false);
	stm_ts_release_all_finger(ts);

	if (ts->plat_data->wirelesscharger_mode != TYPE_WIRELESS_CHARGER_NONE)
		stm_ts_set_wirelesscharger_mode(ts);

	if (ts->charger_mode != TYPE_WIRE_CHARGER_NONE)
		stm_ts_set_wirecharger_mode(ts);

	stm_ts_set_cover_type(ts, ts->plat_data->touch_functions & STM_TS_TOUCHTYPE_BIT_COVER);

	stm_ts_set_custom_library(ts);
	stm_ts_set_press_property(ts);
	stm_ts_set_fod_finger_merge(ts);

	if (ts->plat_data->support_fod && ts->plat_data->fod_data.set_val)
		stm_ts_set_fod_rect(ts);

	/* Power mode */
	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		stm_ts_set_opmode(ts, STM_TS_OPMODE_LOWPOWER);
		sec_delay(50);
		if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_AOD)
			stm_ts_set_aod_rect(ts);
	} else {
		sec_input_set_grip_type(&ts->client->dev, ONLY_EDGE_HANDLER);

		stm_ts_set_external_noise_mode(ts, EXT_NOISE_MODE_MAX);

		if (ts->plat_data->touchable_area) {
			ret = stm_ts_set_touchable_area(ts);
			if (ret < 0)
				goto out;
		}
	}

	if (ts->plat_data->ed_enable)
		stm_ts_ear_detect_enable(ts, ts->plat_data->ed_enable);
	if (ts->plat_data->pocket_mode)
		stm_ts_pocket_mode_enable(ts, ts->plat_data->pocket_mode);
out:
	stm_ts_set_scanmode(ts, ts->scan_mode);
	
}
/*
 * don't need it in interrupt handler in reality, but, need it in vendor IC for requesting vendor IC.
 * If you are requested additional i2c protocol in interrupt handler by vendor.
 * please add it in stm_ts_external_func.
 */
static void stm_ts_external_func(struct stm_ts_data *ts)
{
	sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_IN_IRQ);

}

static void stm_ts_coord_parsing(struct stm_ts_data *ts, struct stm_ts_event_coordinate *p_event_coord, u8 t_id)
{
	ts->plat_data->coord[t_id].id = t_id;
	ts->plat_data->coord[t_id].action = p_event_coord->tchsta;
	ts->plat_data->coord[t_id].x = (p_event_coord->x_11_4 << 4) | (p_event_coord->x_3_0);
	ts->plat_data->coord[t_id].y = (p_event_coord->y_11_4 << 4) | (p_event_coord->y_3_0);
	ts->plat_data->coord[t_id].z = p_event_coord->z & 0x3F;
	ts->plat_data->coord[t_id].ttype = p_event_coord->ttype_3_2 << 2 | p_event_coord->ttype_1_0 << 0;
	ts->plat_data->coord[t_id].major = p_event_coord->major;
	ts->plat_data->coord[t_id].minor = p_event_coord->minor;

	if (!ts->plat_data->coord[t_id].palm && (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_PALM))
		ts->plat_data->coord[t_id].palm_count++;

	ts->plat_data->coord[t_id].palm = (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_PALM);
	if (ts->plat_data->coord[t_id].palm)
		ts->plat_data->palm_flag |= (1 << t_id);
	else
		ts->plat_data->palm_flag &= ~(1 << t_id);

	ts->plat_data->coord[t_id].left_event = p_event_coord->left_event;

	ts->plat_data->coord[t_id].noise_level = max(ts->plat_data->coord[t_id].noise_level,
							p_event_coord->noise_level);
	ts->plat_data->coord[t_id].max_strength = max(ts->plat_data->coord[t_id].max_strength,
							p_event_coord->max_strength);
	ts->plat_data->coord[t_id].hover_id_num = max_t(u8, ts->plat_data->coord[t_id].hover_id_num,
							p_event_coord->hover_id_num);
	ts->plat_data->coord[t_id].noise_status = p_event_coord->noise_status;
	ts->plat_data->coord[t_id].freq_id = p_event_coord->freq_id;

	if (ts->plat_data->coord[t_id].z <= 0)
		ts->plat_data->coord[t_id].z = 1;
}

int stm_ts_fod_vi_event(struct stm_ts_data *ts)
{
	int ret = 0;

	ts->plat_data->fod_data.vi_data[0] = SEC_TS_CMD_SPONGE_FOD_POSITION;
	ts->plat_data->fod_data.vi_data[1] = 0x00;
	ret = ts->stm_ts_read_sponge(ts, ts->plat_data->fod_data.vi_data, ts->plat_data->fod_data.vi_size);
	if (ret < 0)
		input_info(true, &ts->client->dev, "%s: failed read fod vi\n", __func__);

	return ret;
}

static void stm_ts_gesture_event(struct stm_ts_data *ts, u8 *event_buff)
{
	struct stm_ts_gesture_status *p_gesture_status;
	int x, y;

	p_gesture_status = (struct stm_ts_gesture_status *)event_buff;

	x = (p_gesture_status->gesture_data_1 << 4) | (p_gesture_status->gesture_data_3 >> 4);
	y = (p_gesture_status->gesture_data_2 << 4) | (p_gesture_status->gesture_data_3 & 0x0F);

	if (p_gesture_status->stype == STM_TS_SPONGE_EVENT_SWIPE_UP) {
		sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_SPAY, 0, 0);
	} else if (p_gesture_status->stype == STM_TS_GESTURE_CODE_DOUBLE_TAP) {
		if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_AOD) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_AOD_DOUBLETAB, x, y);
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_DOUBLETAP_TO_WAKEUP) {
			input_info(true, &ts->client->dev, "%s: AOT\n", __func__);
			input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 1);
			input_sync(ts->plat_data->input_dev);
			input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 0);
			input_sync(ts->plat_data->input_dev);
		}
	} else if (p_gesture_status->stype  == STM_TS_SPONGE_EVENT_SINGLETAP) {
		sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_SINGLE_TAP, x, y);
	} else if (p_gesture_status->stype  == STM_TS_SPONGE_EVENT_PRESS) {
		if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_LONG ||
			p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_NORMAL) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_PRESS, x, y);
			input_info(true, &ts->client->dev, "%s: FOD %sPRESS\n",
					__func__, p_gesture_status->gesture_id ? "" : "LONG");
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_RELEASE) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_RELEASE, x, y);
			input_info(true, &ts->client->dev, "%s: FOD RELEASE\n", __func__);
			memset(ts->plat_data->fod_data.vi_data, 0x0, ts->plat_data->fod_data.vi_size);
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_OUT) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_OUT, x, y);
			input_info(true, &ts->client->dev, "%s: FOD OUT\n", __func__);
		} else if (p_gesture_status->gesture_id == STM_TS_SPONGE_EVENT_GESTURE_ID_FOD_VI) {
			if ((ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS) && ts->plat_data->support_fod_lp_mode)
				stm_ts_fod_vi_event(ts);
		} else {
			input_info(true, &ts->client->dev, "%s: invalid id %d\n",
					__func__, p_gesture_status->gesture_id);
		}
	} else if (p_gesture_status->stype  == STM_TS_GESTURE_CODE_DUMPFLUSH) {
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
		if (ts->sponge_inf_dump) {
			if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
				if (p_gesture_status->gesture_id == STM_TS_SPONGE_DUMP_0)
					stm_ts_sponge_dump_flush(ts, STM_TS_SPONGE_DUMP_0);
				if (p_gesture_status->gesture_id == STM_TS_SPONGE_DUMP_1)
					stm_ts_sponge_dump_flush(ts, STM_TS_SPONGE_DUMP_1);
			} else {
				ts->sponge_dump_delayed_flag = true;
				ts->sponge_dump_delayed_area = p_gesture_status->gesture_id;
			}
		}
#endif
	} else if (p_gesture_status->stype  == STM_TS_VENDOR_EVENT_LARGEPALM) {	//fold
		input_info(true, &ts->client->dev, "%s: LARGE PALM %s\n", __func__,
				p_gesture_status->gesture_id == 1 ? "RELEASED" : "PRESSED");
		if (p_gesture_status->gesture_id == 0x01)
			input_report_key(ts->plat_data->input_dev, BTN_LARGE_PALM, 0);
		else
			input_report_key(ts->plat_data->input_dev, BTN_LARGE_PALM, 1);
		input_sync(ts->plat_data->input_dev);
	} else if (p_gesture_status->stype  == STM_TS_SPONGE_EVENT_LONGPRESS) {
		sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_LONG_PRESS, x, y);
		input_info(true, &ts->client->dev, "%s: LONG PRESS\n", __func__);
	}
}

static void stm_ts_coordinate_event(struct stm_ts_data *ts, u8 *event_buff)
{
	struct stm_ts_event_coordinate *p_event_coord;
	u8 t_id = 0;

	if (ts->plat_data->power_state != SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev,
				"%s: device is closed %x %x %x %x %x %x %x %x\n", __func__,
				event_buff[0], event_buff[1], event_buff[2],
				event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7]);
		return;
	}

	p_event_coord = (struct stm_ts_event_coordinate *)event_buff;
	ts->game_mode = p_event_coord->game_mode;

	t_id = p_event_coord->tid;

	if (t_id < SEC_TS_SUPPORT_TOUCH_COUNT) {
		ts->plat_data->prev_coord[t_id] = ts->plat_data->coord[t_id];
		stm_ts_coord_parsing(ts, p_event_coord, t_id);

		if ((ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_NORMAL)
				|| (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_PALM)
				|| (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_WET)
				|| (ts->plat_data->coord[t_id].ttype == STM_TS_TOUCHTYPE_GLOVE)) {
			sec_input_coord_event_fill_slot(&ts->client->dev, t_id);
		} else {
			input_err(true, &ts->client->dev,
					"%s: do not support coordinate type(%d)\n",
					__func__, ts->plat_data->coord[t_id].ttype);
		}
	} else {
		input_err(true, &ts->client->dev, "%s: tid(%d) is out of range\n", __func__, t_id);
	}
}

static void stm_ts_status_event(struct stm_ts_data *ts, u8 *event_buff)
{
	struct stm_ts_event_status *p_event_status;

	p_event_status = (struct stm_ts_event_status *)event_buff;

	if (p_event_status->stype > 0)
		input_info(true, &ts->client->dev, "%s: STATUS %x %x %x %x %x %x %x %x\n", __func__,
				event_buff[0], event_buff[1], event_buff[2],
				event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7]);

	if (p_event_status->stype == STM_TS_EVENT_STATUSTYPE_ERROR) {
		if (p_event_status->status_id == STM_TS_ERR_EVENT_QUEUE_FULL) {
			input_err(true, &ts->client->dev, "%s: IC Event Queue is full\n", __func__);
			stm_ts_release_all_finger(ts);
		} else if (p_event_status->status_id == STM_TS_ERR_EVENT_ESD) {
			input_err(true, &ts->client->dev, "%s: ESD detected\n", __func__);
			if (!ts->reset_is_on_going)
				schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
		}
	} else if (p_event_status->stype == STM_TS_EVENT_STATUSTYPE_INFO) {
		char test[32];
		char result[32];

		if (p_event_status->status_id == STM_TS_INFO_READY_STATUS) {
			if (p_event_status->status_data_1 == 0x10) {
				input_err(true, &ts->client->dev, "%s: IC Reset\n", __func__);
				if (!ts->reset_is_on_going)
					schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
			}
		} else if (p_event_status->status_id == STM_TS_INFO_WET_MODE) {
			ts->plat_data->wet_mode = p_event_status->status_data_1;
			input_info(true, &ts->client->dev, "%s: water wet mode %d\n",
				__func__, ts->plat_data->wet_mode);
			if (ts->plat_data->wet_mode)
				ts->plat_data->hw_param.wet_count++;
			snprintf(test, sizeof(test), "STATUS=WET");
			snprintf(result, sizeof(result), "VALUE=%d", p_event_status->status_data_1);
			sec_cmd_send_event_to_user(&ts->sec, test, result);
		} else if (p_event_status->status_id == STM_TS_INFO_NOISE_MODE) {
			ts->plat_data->touch_noise_status = (p_event_status->status_data_1 >> 4);

			input_info(true, &ts->client->dev, "%s: NOISE MODE %s[%02X]\n",
					__func__, ts->plat_data->touch_noise_status == 0 ? "OFF" : "ON",
					p_event_status->status_data_1);

			if (ts->plat_data->touch_noise_status)
				ts->plat_data->hw_param.noise_count++;
			snprintf(test, sizeof(test), "STATUS=NOISE");
			snprintf(result, sizeof(result), "VALUE=%d", p_event_status->status_data_1);
			sec_cmd_send_event_to_user(&ts->sec, test, result);
		}
	} else if (p_event_status->stype == STM_TS_EVENT_STATUSTYPE_VENDORINFO) {
		if (ts->plat_data->support_ear_detect) {
			if (p_event_status->status_id == 0x6A) {
				ts->hover_event = p_event_status->status_data_1;
				input_report_abs(ts->plat_data->input_dev_proximity, ABS_MT_CUSTOM, p_event_status->status_data_1);
				input_sync(ts->plat_data->input_dev_proximity);
				input_info(true, &ts->client->dev, "%s: proximity: %d\n", __func__, p_event_status->status_data_1);
			}
		}
	}
}

static int stm_ts_get_event(struct stm_ts_data *ts, u8 *data, int *remain_event_count)
{
	int ret = 0;
	u8 address = 0;

	address = STM_TS_READ_ONE_EVENT;
	ret = ts->stm_ts_read(ts, &address, 1, (u8 *)data, STM_TS_EVENT_BUFF_SIZE);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
		return ret;
	}

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_ONEEVENT)
		input_info(true, &ts->client->dev, "ONE: %02X %02X %02X %02X %02X %02X %02X %02X\n",
				data[0], data[1],
				data[2], data[3],
				data[4], data[5],
				data[6], data[7]);

	if (data[0] == 0) {
		input_info(true, &ts->client->dev, "%s: event buffer is empty\n", __func__);
#if IS_ENABLED(CONFIG_SEC_ABC) && IS_ENABLED(CONFIG_SEC_FACTORY)
		ts->irq_empty_count++;
		if (ts->irq_empty_count >= 100) {
			ts->irq_empty_count = 0;
			sec_abc_send_event(SEC_ABC_SEND_EVENT_TYPE);
		}
#endif

		return SEC_ERROR;
	}

	*remain_event_count = data[7] & 0x1F;

	if (*remain_event_count > MAX_EVENT_COUNT - 1) {
		input_err(true, &ts->client->dev, "%s: event buffer overflow\n", __func__);
		address = STM_TS_CMD_CLEAR_ALL_EVENT;
		ret = ts->stm_ts_write(ts, &address, 1, NULL, 0); //guide
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: i2c write clear event failed\n", __func__);

		stm_ts_release_all_finger(ts);

		return SEC_ERROR;
	}

	if (*remain_event_count > 0) {
		address = STM_TS_READ_ALL_EVENT;
		ret = ts->stm_ts_read(ts, &address, 1, &data[1 * STM_TS_EVENT_BUFF_SIZE],
				 (STM_TS_EVENT_BUFF_SIZE) * (*remain_event_count));
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
			return ret;
		}
	}

	return SEC_SUCCESS;
}

#ifdef ENABLE_RAWDATA_SERVICE
#ifdef RAWDATA_MMAP
static int stm_ts_get_rawdata(struct stm_ts_data *ts)
{
	u8 reg[3];
	int ret;
	s16 *val;
	int ii;
	int jj;
	int kk = 0;
	int num;
	static int prev_touch_count;

	if (ts->raw_mode == 0)
		return 0;

	if (!ts->raw_addr_h && !ts->raw_addr_l)
		return 0;

	if (ts->raw_len == 0)
		return 0;

	if (!ts->raw_u8)
		return 0;

	if (!ts->raw)
		return 0;

	reg[0] = 0xA7;
	reg[1] = ts->raw_addr_h;
	reg[2] = ts->raw_addr_l;

	mutex_lock(&ts->raw_lock);
	if (prev_touch_count == 0 && ts->plat_data->touch_count != 0) {
		/* first press */
		ts->raw_irq_count = 1;
		ts->before_irq_count = 1;
	} else if (prev_touch_count != 0 && ts->plat_data->touch_count == 0) {
		/* all finger released */
		ts->raw_irq_count++;
		ts->before_irq_count = 0;
	} else if (prev_touch_count != 0 && ts->plat_data->touch_count != 0) {
		/* move */
		ts->raw_irq_count++;
		ts->before_irq_count++;
	}
	prev_touch_count = ts->plat_data->touch_count;

	ret = ts->stm_ts_read(ts, reg, 3, ts->raw_u8, ts->tx_count * ts->rx_count * 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to get rawdata: %d\n", __func__, ret);
		mutex_unlock(&ts->raw_lock);
		return ret;
	}

	val = (s16 *)ts->raw_u8;

	for (ii = 0; ii < ts->tx_count; ii++) {
		for (jj = (ts->rx_count - 1); jj >= 0; jj--)
			ts->raw[4 + jj * ts->tx_count + ii] = val[kk++];
	}

	ts->raw[3] = ts->plat_data->coord[0].action == 0 ? 3 : ts->plat_data->coord[0].action;
	ts->raw[1] = ts->raw_irq_count;

	num = ts->raw_irq_count % 5;
	if (num == 0)
		memcpy(ts->raw_v0, ts->raw, ts->raw_len);
	else if (num == 1)
		memcpy(ts->raw_v1, ts->raw, ts->raw_len);
	else if (num == 2)
		memcpy(ts->raw_v2, ts->raw, ts->raw_len);
	else if (num == 3)
		memcpy(ts->raw_v3, ts->raw, ts->raw_len);
	else if (num == 4)
		memcpy(ts->raw_v4, ts->raw, ts->raw_len);

	input_info(true, &ts->client->dev, "%s: num: %d | %d | %d | %d | %d\n", __func__, num, ts->raw[0], ts->raw[1], ts->raw[2], ts->raw[3]);
	mutex_unlock(&ts->raw_lock);

	sysfs_notify(&ts->sec.fac_dev->kobj, NULL, "raw_irq");

	return ret;
}
#endif //#ifdef RAWDATA_MMAP
#ifdef RAWDATA_IOCTL
static int stm_ts_get_rawdata(struct stm_ts_data *ts)
{
	u8 reg[5];
	int ret;
	s16 *val;
	int ii;
	int jj;
	int kk = 0;
	static int prev_touch_count;
	u8 *target_mem;

	if (ts->raw_mode == 0)
		return 0;

	if (!ts->raw_addr_h && !ts->raw_addr_l)
		return 0;

	if (ts->raw_len == 0)
		return 0;

	if (!ts->raw_u8)
		return 0;

	if (!ts->raw)
		return 0;

	reg[0] = STM_TS_CMD_REG_R;
	reg[1] = 0x20;
	reg[2] = 0x01;
	reg[3] = ts->raw_addr_h;
	reg[4] = ts->raw_addr_l;

	mutex_lock(&ts->raw_lock);
	prev_touch_count = ts->plat_data->touch_count;
	ret = ts->stm_ts_read(ts, &reg[0], 5, ts->raw_u8, ts->tx_count * ts->rx_count * 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to get rawdata: %d\n", __func__, ret);
		mutex_unlock(&ts->raw_lock);
		return ret;
	}

	val = (s16 *)ts->raw_u8;

	for (ii = 0; ii < ts->tx_count; ii++) {
		for (jj = (ts->rx_count - 1); jj >= 0; jj--)
			ts->raw[4 + jj * ts->tx_count + ii] = val[kk++];
	}

	ts->raw[3] = ts->plat_data->coord[0].action == 0 ? 3 : ts->plat_data->coord[0].action;

	target_mem = ts->raw_pool[ts->raw_write_index++];
	memcpy(target_mem, ts->raw, ts->raw_len);

	if (ts->raw_write_index >= RAW_VEC_NUM)
		ts->raw_write_index = 0;

/*	input_info(true, &ts->client->dev, "%s: | %d | %d | %d | %d\n", __func__, ts->raw[0], ts->raw[1], ts->raw[2], ts->raw[3]);*/

	mutex_unlock(&ts->raw_lock);

	sysfs_notify(&ts->sec.fac_dev->kobj, NULL, "raw_irq");

	return ret;
}


#endif //RAWDATA_IOCTL
#endif

irqreturn_t stm_ts_irq_thread(int irq, void *ptr)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)ptr;
	int ret;
	u8 event_id;
	u8 read_event_buff[MAX_EVENT_COUNT * STM_TS_EVENT_BUFF_SIZE] = {0};
	u8 *event_buff;
	int curr_pos;
	int remain_event_count;
	char taas[32];
	char tresult[64];
	int raw_irq_flag = 0;

	ret = event_id = curr_pos = remain_event_count = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (secure_filter_interrupt(ts) == IRQ_HANDLED) {
		wait_for_completion_interruptible_timeout(&ts->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &ts->client->dev,
				"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return IRQ_HANDLED;
#endif

	ret = sec_input_handler_start(&ts->client->dev);
	if (ret < 0)
		return IRQ_HANDLED;

	ret = stm_ts_get_event(ts, read_event_buff, &remain_event_count);
	if (ret < 0)
		return IRQ_HANDLED;

	mutex_lock(&ts->eventlock);

	do {
		event_buff = &read_event_buff[curr_pos * STM_TS_EVENT_BUFF_SIZE];
		event_id = event_buff[0] & 0x3;
		if (ts->debug_flag & SEC_TS_DEBUG_PRINT_ALLEVENT)
			input_info(true, &ts->client->dev, "ALL: %02X %02X %02X %02X %02X %02X %02X %02X\n",
					event_buff[0], event_buff[1], event_buff[2], event_buff[3],
					event_buff[4], event_buff[5], event_buff[6], event_buff[7]);

		if (event_id == STM_TS_STATUS_EVENT) {
			stm_ts_status_event(ts, event_buff);
		} else if (event_id == STM_TS_COORDINATE_EVENT) {
			stm_ts_coordinate_event(ts, event_buff);
			raw_irq_flag = 1;
		} else if (event_id == STM_TS_GESTURE_EVENT) {
			stm_ts_gesture_event(ts, event_buff);
		} else if (event_id == STM_TS_VENDOR_EVENT) {
			input_info(true, &ts->client->dev,
				"%s: %s event %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
				__func__, (event_buff[1] == 0x01 ? "echo": ""), event_buff[0], event_buff[1], event_buff[2], event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7], event_buff[8], event_buff[9], event_buff[10], event_buff[11],
				event_buff[12], event_buff[13], event_buff[14], event_buff[15]);

			if (event_buff[2] == STM_TS_CMD_SET_GET_OPMODE && event_buff[3] == STM_TS_OPMODE_NORMAL) {
				sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_TSP_SCAN_UNBLOCK, 0, 0);
				input_info(true, &ts->client->dev, "%s: Normal changed\n", __func__);
			} else if (event_buff[2] == STM_TS_CMD_SET_GET_OPMODE && event_buff[3] == STM_TS_OPMODE_LOWPOWER) {
				sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_TSP_SCAN_UNBLOCK, 0, 0);
				input_info(true, &ts->client->dev, "%s: lp changed\n", __func__);
			}

			if ((event_buff[0] == 0xF3) && ((event_buff[1] == 0x02) || (event_buff[1] == 0x46))) {
				if (!ts->reset_is_on_going)
					schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
				break;
			}

			if ((event_buff[0] == 0x43) && (event_buff[1] == 0x05) && (event_buff[1] == 0x11)) {
				snprintf(taas, sizeof(taas), "TAAS=CASB");
				snprintf(tresult, sizeof(tresult), "RESULT=STM TSP Force Calibration Event");
				sec_cmd_send_event_to_user(&ts->sec, taas, tresult);
			}
		} else {
			input_info(true, &ts->client->dev,
					"%s: unknown event %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
						__func__, event_buff[0], event_buff[1], event_buff[2], event_buff[3], event_buff[4], event_buff[5],
						event_buff[6], event_buff[7], event_buff[8], event_buff[9], event_buff[10], event_buff[11],
						event_buff[12], event_buff[13], event_buff[14], event_buff[15]);
		}
		curr_pos++;
		remain_event_count--;
	} while (remain_event_count >= 0);

	sec_input_coord_event_sync_slot(&ts->client->dev);

	stm_ts_external_func(ts);

	mutex_unlock(&ts->eventlock);

#ifdef ENABLE_RAWDATA_SERVICE
	if (raw_irq_flag && !ts->game_mode) {
		stm_ts_get_rawdata(ts);
	}
#endif
	return IRQ_HANDLED;
}

int stm_ts_input_open(struct input_dev *dev)
{
	struct stm_ts_data *ts = input_get_drvdata(dev);
	int ret;
	u8 data[2] = { 0 };
	int retrycnt = 0;

	if (!ts->probe_done) {
		input_err(true, &ts->client->dev, "%s: device probe is not done\n", __func__);
		return 0;
	}
	cancel_delayed_work_sync(&ts->work_read_info);
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	cancel_delayed_work_sync(&ts->close_work);
#endif

	mutex_lock(&ts->modechange);

	ts->plat_data->enabled = true;
	ts->plat_data->prox_power_off = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 0);
#endif
	cancel_delayed_work_sync(&ts->switching_work);
	mutex_lock(&ts->switching_mutex);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		ts->plat_data->lpmode(ts, TO_TOUCH_MODE);
		sec_input_set_grip_type(&ts->client->dev, ONLY_EDGE_HANDLER);
	} else {
		ret = ts->plat_data->start_device(ts);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);
	}
	mutex_unlock(&ts->switching_mutex);

	if (ts->fix_active_mode) 
		stm_ts_fix_active_mode(ts, STM_TS_ACTIVE_TRUE);

	if (ts->plat_data->low_sensitivity_mode) {
		u8 reg[2];

		reg[0] = STM_TS_CMD_FUNCTION_SET_LOW_SENSITIVITY_MODE;
		reg[1] = ts->plat_data->low_sensitivity_mode;
		ret = ts->stm_ts_write(ts, reg, 2, NULL, 0);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to set low sensitivity mode\n", __func__);
	}

	sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_FORCE);

	mutex_unlock(&ts->modechange);

retry_fodmode:
	if (ts->plat_data->support_fod && (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS)) {
		data[0] = STM_TS_CMD_SPONGE_OFFSET_MODE;
		ret = ts->stm_ts_read_sponge(ts, data, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to read sponge offset data\n", __func__);
			retrycnt++;
			if(retrycnt < 5)
				goto retry_fodmode;

		}
		input_info(true, &ts->client->dev, "%s: read offset data(0x%x)\n", __func__, data[0]);

		if((data[0] & SEC_TS_MODE_SPONGE_PRESS) != (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS)) {
			input_err(true, &ts->client->dev, "%s: fod data not matched(%d,%d) retry %d\n",
					__func__, (data[0] & SEC_TS_MODE_SPONGE_PRESS),
					(ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS), retrycnt);
			retrycnt++;
			/*rewrite*/
			stm_ts_set_custom_library(ts);
			stm_ts_set_press_property(ts);
			stm_ts_set_fod_finger_merge(ts);
			if (retrycnt < 5)
				goto retry_fodmode;
		}
	}

	cancel_delayed_work(&ts->work_print_info);
	ts->plat_data->print_info_cnt_open = 0;
	ts->plat_data->print_info_cnt_release = 0;
	if (!ts->plat_data->shutdown_called)
		schedule_work(&ts->work_print_info.work);
	mutex_lock(&ts->switching_mutex);
	ts->flip_status_prev = ts->flip_status_current;
	mutex_unlock(&ts->switching_mutex);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	if (ts->plat_data->support_flex_mode && (ts->plat_data->support_dual_foldable == MAIN_TOUCH))
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_MAIN_TOUCH_ON, NULL);
#endif

	return 0;
}
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#if IS_ENABLED(CONFIG_GH_RM_DRV)
void stm_ts_close_work(struct work_struct *work)
{
	struct stm_ts_data *ts = container_of(work, struct stm_ts_data, close_work.work);
	struct timespec64 cur_ts;
	struct rtc_time tm;
	ktime_t calltime;
	u64 realtime;
	int curr_time;

	if (atomic_read(&ts->trusted_touch_enabled)) {
		calltime = ktime_get();
		realtime = ktime_to_ns(calltime);
		do_div(realtime, NSEC_PER_USEC);
		curr_time = realtime / USEC_PER_MSEC;
		ktime_get_real_ts64(&cur_ts);
		rtc_time64_to_tm(cur_ts.tv_sec, &tm);
		input_info(true, &ts->client->dev,
			"%s: start waiting for trusted touch to be disable.(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC/ kernel: %d)\n",
			__func__,  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, cur_ts.tv_nsec, curr_time);

		wait_for_completion_interruptible(&ts->trusted_touch_powerdown);

		calltime = ktime_get();
		realtime = ktime_to_ns(calltime);
		do_div(realtime, NSEC_PER_USEC);
		curr_time = realtime / USEC_PER_MSEC;
		ktime_get_real_ts64(&cur_ts);
		rtc_time64_to_tm(cur_ts.tv_sec, &tm);
		input_info(true, &ts->client->dev,
			"%s: disabling trusted touch is done(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC/ kernel: %d)\n",
			__func__,  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, cur_ts.tv_nsec, curr_time);
	}

	mutex_lock(&ts->modechange);

	if (ts->flip_status_prev != ts->flip_status_current || ts->plat_data->prox_power_off) {
		input_report_key(ts->plat_data->input_dev, KEY_INT_CANCEL, 1);
		input_sync(ts->plat_data->input_dev);
		input_report_key(ts->plat_data->input_dev, KEY_INT_CANCEL, 0);
		input_sync(ts->plat_data->input_dev);
	}

	cancel_delayed_work(&ts->reset_work);
	cancel_delayed_work_sync(&ts->switching_work);

	mutex_lock(&ts->switching_mutex);
	if (ts->plat_data->support_dual_foldable == MAIN_TOUCH && ts->flip_status_current == STM_TS_STATUS_FOLDING) {
		ts->plat_data->stop_device(ts);
	} else {
		if (ts->plat_data->lowpower_mode || ts->plat_data->ed_enable || ts->plat_data->pocket_mode || ts->plat_data->fod_lp_mode) {
			if (ts->fix_active_mode)
				stm_ts_fix_active_mode(ts, STM_TS_ACTIVE_FALSE);
			ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
		} else {
			ts->plat_data->stop_device(ts);
		}
	}
	mutex_unlock(&ts->switching_mutex);
	mutex_unlock(&ts->modechange);
}
#endif
#endif
void stm_ts_input_close(struct input_dev *dev)
{
	struct stm_ts_data *ts = input_get_drvdata(dev);

	if (!ts->probe_done) {
		input_err(true, &ts->client->dev, "%s: device probe is not done\n", __func__);
		return;
	}
	cancel_delayed_work_sync(&ts->work_read_info);

	if (ts->plat_data->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return;
	}

	mutex_lock(&ts->modechange);

	ts->plat_data->enabled = false;

#ifdef TCLM_CONCEPT
	sec_tclm_debug_info(ts->tdata);
#endif
	cancel_delayed_work(&ts->work_print_info);
	sec_input_print_info(&ts->client->dev, ts->tdata);
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	secure_touch_stop(ts, 1);
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	if (atomic_read(&ts->trusted_touch_enabled)) {
		input_info(true, &ts->client->dev, "%s wait for disabling trusted touch\n", __func__);
		schedule_work(&ts->close_work.work);
		mutex_unlock(&ts->modechange);
		return;
	}
#endif
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	stui_cancel_session();
#endif
	if (ts->flip_status_prev != ts->flip_status_current || ts->plat_data->prox_power_off) {
		input_report_key(ts->plat_data->input_dev, KEY_INT_CANCEL, 1);
		input_sync(ts->plat_data->input_dev);
		input_report_key(ts->plat_data->input_dev, KEY_INT_CANCEL, 0);
		input_sync(ts->plat_data->input_dev);
	}

	cancel_delayed_work(&ts->reset_work);
	cancel_delayed_work_sync(&ts->switching_work);

	mutex_lock(&ts->switching_mutex);
	if (ts->plat_data->support_dual_foldable == MAIN_TOUCH && ts->flip_status_current == STM_TS_STATUS_FOLDING) {
		ts->plat_data->stop_device(ts);
	} else {
		if (ts->plat_data->lowpower_mode || ts->plat_data->ed_enable || ts->plat_data->pocket_mode || ts->plat_data->fod_lp_mode) {
			if (ts->fix_active_mode)
				stm_ts_fix_active_mode(ts, STM_TS_ACTIVE_FALSE);
			ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
		} else {
			ts->plat_data->stop_device(ts);
		}
	}
	mutex_unlock(&ts->switching_mutex);
	mutex_unlock(&ts->modechange);
}

int stm_ts_stop_device(void *data)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)data;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->device_mutex);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: already power off\n", __func__);
		goto out;
	}

	disable_irq(ts->irq);
	if (ts->plat_data->unuse_dvdd_power)
		ts->stm_ts_command(ts, STM_TS_CMD_SENSE_OFF, false);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_OFF;

	stm_ts_locked_release_all_finger(ts);

	ts->plat_data->power(&ts->client->dev, false);
	ts->plat_data->pinctrl_configure(&ts->client->dev, false);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

int stm_ts_start_device(void *data)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)data;
	int ret = -1;
	u8 address = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	ts->plat_data->pinctrl_configure(&ts->client->dev, true);

	mutex_lock(&ts->device_mutex);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev, "%s: already power on\n", __func__);
		goto out;
	}

	stm_ts_locked_release_all_finger(ts);

	ts->plat_data->power(&ts->client->dev, true);

	sec_delay(TOUCH_POWER_ON_DWORK_TIME);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;
	ts->plat_data->touch_noise_status = 0;

	ts->plat_data->init(ts);

	stm_ts_read_chip_id(ts);

	/* Sense_on */
	address = STM_TS_CMD_SENSE_ON;
	ret = ts->stm_ts_write(ts, &address, 1, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to write Sense_on\n", __func__);

	enable_irq(ts->irq);
out:
	mutex_unlock(&ts->device_mutex);
	return ret;
}

static int stm_ts_hw_init(struct stm_ts_data *ts)
{
	int ret = 0;
	int retry = 3;

	ts->plat_data->pinctrl_configure(&ts->client->dev, true);

	ts->plat_data->power(&ts->client->dev, true);
	if (!ts->plat_data->regulator_boot_on)
		sec_delay(TOUCH_POWER_ON_DWORK_TIME);

	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;
#if IS_ENABLED(CONFIG_TOUCHSCREEN_STM_SPI)
	stm_ts_set_spi_mode(ts);
#endif
	do {
		ret = stm_ts_fw_corruption_check(ts);
		if (ret == -STM_TS_ERROR_FW_CORRUPTION) {
			ts->plat_data->hw_param.checksum_result = 1;
			break;
		} else if (ret < 0) {
			if (ret == -STM_TS_ERROR_BROKEN_OSC_TRIM) {
				break;
			} else if (ts->plat_data->hw_param.checksum_result) {
				break;
			} else if (ret == -STM_TS_ERROR_TIMEOUT_ZERO) {
				stm_ts_read_chip_id_hw(ts);
			}
			stm_ts_systemreset(ts, 20);
		} else {
			break;
		}
	} while (--retry);

	stm_ts_get_version_info(ts);

	if (ret == -STM_TS_ERROR_BROKEN_OSC_TRIM) {
		ret = stm_ts_osc_trim_recovery(ts);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to recover osc trim\n", __func__);
	}

	if (ts->plat_data->hw_param.checksum_result) {
		ts->fw_version_of_ic = 0;
		ts->config_version_of_ic = 0;
		ts->fw_main_version_of_ic = 0;
	}

	stm_ts_read_chip_id(ts);

	ret = stm_ts_fw_update_on_probe(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to firmware update\n",
				__func__);
		return -STM_TS_ERROR_FW_UPDATE_FAIL;
	}

	ret = stm_ts_get_channel_info(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read failed rc = %d\n", __func__, ret);
		return ret;
	}

	ts->pFrame = kzalloc(ts->rx_count * ts->tx_count * 2 + 1, GFP_KERNEL);
	if (!ts->pFrame) {
		ret = -ENOMEM;
		return ret;
	}

	ts->cx_data = kzalloc(ts->rx_count * ts->tx_count + 1, GFP_KERNEL);
	if (!ts->cx_data) {
		kfree(ts->pFrame);
		ret = -ENOMEM;
		return ret;
	}

	ts->ito_result = kzalloc(STM_TS_ITO_RESULT_PRINT_SIZE, GFP_KERNEL);
	if (!ts->ito_result) {
		kfree(ts->cx_data);
		kfree(ts->pFrame);
		ret = -ENOMEM;
		return ret;
	}

	/* fts driver set functional feature */
	ts->plat_data->touch_count = 0;
	ts->touch_opmode = STM_TS_OPMODE_NORMAL;
	ts->charger_mode = STM_TS_BIT_CHARGER_MODE_NORMAL;
	ts->irq_empty_count = 0;

#ifdef TCLM_CONCEPT
	ts->tdata->external_factory = false;
#endif

	ts->plat_data->touch_functions = STM_TS_TOUCHTYPE_DEFAULT_ENABLE;
	stm_ts_set_touch_function(ts);
	sec_delay(10);

	stm_ts_command(ts, STM_TS_CMD_FORCE_CALIBRATION, true);
	stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);
	ts->scan_mode = STM_TS_SCAN_MODE_DEFAULT;
	stm_ts_set_scanmode(ts, ts->scan_mode);

	mutex_lock(&ts->switching_mutex);
	ts->flip_status = -1;
	mutex_unlock(&ts->switching_mutex);

	if (ts->plat_data->support_dual_foldable == SUB_TOUCH)
		ts->flip_status_current = STM_TS_STATUS_FOLDING;
	else
		ts->flip_status_current = STM_TS_STATUS_UNFOLDING;

	input_info(true, &ts->client->dev, "%s: Initialized\n", __func__);

	stm_ts_init_proc(ts);

	return ret;
}

static void stm_ts_parse_dt(struct device *dev, struct stm_ts_data *ts)
{
	struct device_node *np = dev->of_node;

	if (of_property_read_u32(np, "stm,lpmode_change_delay", &ts->lpmode_change_delay))
		ts->lpmode_change_delay = 5;

	input_info(true, dev, "%s: lpmode_change_delay:%d\n", __func__, ts->lpmode_change_delay);
}

static int stm_ts_init(struct stm_ts_data *ts)
{
	int ret = 0;

	if (ts->client->dev.of_node) {
		ret = sec_input_parse_dt(&ts->client->dev);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: Failed to parse dt\n", __func__);
			goto error_allocate_mem;
		}

		stm_ts_parse_dt(&ts->client->dev, ts);

#ifdef TCLM_CONCEPT
		sec_tclm_parse_dt(&ts->client->dev, ts->tdata);
#endif
	} else {
		ts->plat_data = ts->client->dev.platform_data;
		if (!ts->plat_data) {
			ret = -ENOMEM;
			input_err(true, &ts->client->dev, "%s: No platform data found\n", __func__);
			goto error_allocate_pdata;
		}
	}

	ts->plat_data->pinctrl = devm_pinctrl_get(&ts->client->dev);
	if (IS_ERR(ts->plat_data->pinctrl))
		input_err(true, &ts->client->dev, "%s: could not get pinctrl\n", __func__);

	ts->client->irq = gpio_to_irq(ts->plat_data->irq_gpio);

	ts->irq = ts->client->irq;
	ts->stm_ts_systemreset = stm_ts_systemreset;
	ts->stm_ts_command = stm_ts_command;

	ts->plat_data->pinctrl_configure = sec_input_pinctrl_configure;
	ts->plat_data->power = sec_input_power;
	ts->plat_data->start_device = stm_ts_start_device;
	ts->plat_data->stop_device = stm_ts_stop_device;
	ts->plat_data->init = stm_ts_reinit;
	ts->plat_data->lpmode = stm_ts_set_lowpowermode;
	ts->plat_data->set_grip_data = stm_set_grip_data_to_ic;
	ts->plat_data->set_temperature = stm_ts_set_temperature;

	ptsp = &ts->client->dev;
	g_ts = ts;

#ifdef TCLM_CONCEPT
	sec_tclm_initialize(ts->tdata);
	ts->tdata->tclm_parse_dt = sec_tclm_parse_dt;
#endif

	INIT_DELAYED_WORK(&ts->reset_work, stm_ts_reset_work);
	INIT_DELAYED_WORK(&ts->work_read_info, stm_ts_read_info_work);
	INIT_DELAYED_WORK(&ts->work_print_info, stm_ts_print_info_work);
	INIT_DELAYED_WORK(&ts->work_read_functions, stm_ts_get_touch_function);
	INIT_DELAYED_WORK(&ts->switching_work, stm_switching_work);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	INIT_DELAYED_WORK(&ts->plat_data->interrupt_notify_work, stm_ts_interrupt_notify);
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	INIT_DELAYED_WORK(&ts->close_work, stm_ts_close_work);
#endif
#endif
	mutex_init(&ts->device_mutex);
	mutex_init(&ts->read_write_mutex);
	mutex_init(&ts->eventlock);
	mutex_init(&ts->modechange);
	mutex_init(&ts->sponge_mutex);
	mutex_init(&ts->fn_mutex);
	mutex_init(&ts->switching_mutex);
	mutex_init(&ts->status_mutex);

#ifdef ENABLE_RAWDATA_SERVICE
	mutex_init(&ts->raw_lock);
#endif
	ts->plat_data->sec_ws = wakeup_source_register(&ts->client->dev, "tsp");
	device_init_wakeup(&ts->client->dev, true);

	init_completion(&ts->plat_data->resume_done);
	complete_all(&ts->plat_data->resume_done);

	ret = sec_input_device_register(&ts->client->dev, ts);
	if (ret) {
		input_err(true, &ts->client->dev, "failed to register input device, %d\n", ret);
		goto err_register_input_device;
	}

	ts->plat_data->input_dev->open = stm_ts_input_open;
	ts->plat_data->input_dev->close = stm_ts_input_close;

	ret = stm_ts_fn_init(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: fail to init fn\n", __func__);
		goto err_fn_init;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (sysfs_create_group(&ts->plat_data->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &ts->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);

	sec_secure_touch_register(ts, ts->plat_data->ss_touch_num, &ts->plat_data->input_dev->dev.kobj);

#if IS_ENABLED(CONFIG_GH_RM_DRV)
	stm_ts_trusted_touch_init(ts);
	mutex_init(&ts->clk_io_ctrl_mutex);
	mutex_init(&ts->transition_lock);
#endif
#endif
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	dump_callbacks.inform_dump = stm_ts_dump_tsp_log;
	INIT_DELAYED_WORK(&ts->check_rawdata, stm_ts_check_rawdata);
#endif
	input_info(true, &ts->client->dev, "%s: init resource\n", __func__);

	return 0;

err_fn_init:
err_register_input_device:
	wakeup_source_unregister(ts->plat_data->sec_ws);
	regulator_put(ts->plat_data->dvdd);
	regulator_put(ts->plat_data->avdd);
error_allocate_pdata:
error_allocate_mem:
	input_err(true, &ts->client->dev, "%s: failed(%d)\n", __func__, ret);
	input_log_fix();
	return ret;
}

void stm_ts_release(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_unregister_notify(&ts->stm_input_nb);
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_unregister(&ts->vbus_nb);
#endif
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	if (ts->hall_ic_nb.notifier_call)
		hall_notifier_unregister(&ts->hall_ic_nb);
#endif

	cancel_delayed_work_sync(&ts->work_read_info);
	cancel_delayed_work_sync(&ts->work_print_info);
	cancel_delayed_work_sync(&ts->work_read_functions);
	cancel_delayed_work_sync(&ts->reset_work);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	cancel_delayed_work_sync(&ts->plat_data->interrupt_notify_work);
#endif
	cancel_delayed_work_sync(&ts->switching_work);
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	cancel_delayed_work_sync(&ts->close_work);
#endif
	flush_delayed_work(&ts->reset_work);
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	cancel_delayed_work_sync(&ts->check_rawdata);
	dump_callbacks.inform_dump = NULL;
#endif
	stm_ts_fn_remove(ts);

	device_init_wakeup(&ts->client->dev, false);
	wakeup_source_unregister(ts->plat_data->sec_ws);

	ts->plat_data->lowpower_mode = false;
	ts->probe_done = false;

	ts->plat_data->power(&ts->client->dev, false);

	regulator_put(ts->plat_data->dvdd);
	regulator_put(ts->plat_data->avdd);
}


int stm_ts_probe(struct stm_ts_data *ts)
{
	int ret = 0;
#if IS_MODULE(CONFIG_TOUCHSCREEN_STM) || IS_MODULE(CONFIG_TOUCHSCREEN_STM_SPI)
	static int deferred_flag = 0;

	if (!deferred_flag) {
		deferred_flag = 1;
		input_info(true, &ts->client->dev, "deferred_flag boot %s\n", __func__);
		return -EPROBE_DEFER;
	}
#endif

	input_info(true, &ts->client->dev, "%s\n", __func__);

	ret = stm_ts_init(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to init resource\n", __func__);
		return ret;
	}

	ret = stm_ts_hw_init(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to init hw\n", __func__);
		stm_ts_release(ts);
		return ret;
	}

	stm_ts_get_custom_library(ts);
	stm_ts_set_custom_library(ts);

	input_info(true, &ts->client->dev, "%s: request_irq = %d\n", __func__, ts->client->irq);
	ret = request_threaded_irq(ts->client->irq, NULL, stm_ts_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, STM_TS_I2C_NAME, ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Unable to request threaded irq\n", __func__);
		stm_ts_release(ts);
		return ret;
	}

	ts->probe_done = true;
	ts->plat_data->enabled = true;

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	ts->hall_ic_nb.priority = 1;
	ts->hall_ic_nb.notifier_call = stm_hall_ic_notify;
	hall_notifier_register(&ts->hall_ic_nb);
	input_info(true, &ts->client->dev, "%s: hall ic register\n", __func__);
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_register_notify(&ts->stm_input_nb, stm_touch_notify_call, 1);
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&ts->vbus_nb, stm_ts_vbus_notification,
						VBUS_NOTIFY_DEV_CHARGER);
#endif

	input_err(true, &ts->client->dev, "%s: done\n", __func__);
#ifdef ENABLE_RAWDATA_SERVICE
	stm_ts_rawdata_init(ts);
#endif
	input_log_fix();

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	hall_ic_request_notitfy();
#endif

	if (!ts->plat_data->shutdown_called)
		schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(50));

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP) && IS_ENABLED(CONFIG_TOUCHSCREEN_STM_SPI)
	stm_ts_tool_proc_init(ts);
#endif
	return 0;
}

int stm_ts_remove(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP) && IS_ENABLED(CONFIG_TOUCHSCREEN_STM_SPI)
	stm_ts_tool_proc_remove();
#endif
	mutex_lock(&ts->modechange);
	ts->plat_data->shutdown_called = true;
	mutex_unlock(&ts->modechange);

	disable_irq_nosync(ts->client->irq);
	free_irq(ts->client->irq, ts);

	stm_ts_release(ts);

	return 0;
}

void stm_ts_shutdown(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	stm_ts_remove(ts);
}


#if IS_ENABLED(CONFIG_PM)
int stm_ts_pm_suspend(struct stm_ts_data *ts)
{
	reinit_completion(&ts->plat_data->resume_done);

	return 0;
}

int stm_ts_pm_resume(struct stm_ts_data *ts)
{
	complete_all(&ts->plat_data->resume_done);

	return 0;
}
#endif

MODULE_LICENSE("GPL");
